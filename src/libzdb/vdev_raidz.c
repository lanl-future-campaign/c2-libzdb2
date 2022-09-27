/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or https://opensource.org/licenses/CDDL-1.0.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2012, 2020 by Delphix. All rights reserved.
 * Copyright (c) 2016 Gvozden Nešković. All rights reserved.
 * Copyright (c) 2022 Triad National Security, LLC as operator of Los Alamos
 *     National Laboratory. All rights reserved.
 */
#include <libzdb/vdev_raidz.h>

#include <sys/vdev_impl.h>        /* ZFS */
#include <sys/vdev_raidz_impl.h>  /* ZFS */

#include <stdlib.h>

void
libzdb_vdev_raidz_map_alloc(zio_t *zio, uint64_t ashift, uint64_t dcols,
                        uint64_t nparity, char **backing, uint64_t actual_size,
                        libzdb_dva_t *dva) {
	raidz_row_t *rr;
	/* The starting RAIDZ (parent) vdev sector of the block. */
	uint64_t b = zio->io_offset >> ashift;
	/* The zio's size in units of the vdev's minimum sector size. */
	uint64_t s = zio->io_size >> ashift;
	/* The first column for this stripe. */
	uint64_t f = b % dcols;
	/* The starting byte offset on each child vdev. */
	uint64_t o = (b / dcols) << ashift;
	uint64_t q, r, c, bc, col, acols, scols, coff, devidx, asize, tot;

	raidz_map_t *rm =
	    kmem_zalloc(offsetof(raidz_map_t, rm_row[1]), KM_SLEEP);
	rm->rm_nrows = 1;

	/*
	 * "Quotient": The number of data sectors for this stripe on all but
	 * the "big column" child vdevs that also contain "remainder" data.
	 */
	q = s / (dcols - nparity);

	/*
	 * "Remainder": The number of partial stripe data sectors in this I/O.
	 * This will add a sector to some, but not all, child vdevs.
	 */
	r = s - q * (dcols - nparity);

	/* The number of "big columns" - those which contain remainder data. */
	bc = (r == 0 ? 0 : r + nparity);

	/*
	 * The total number of data and parity sectors associated with
	 * this I/O.
	 */
	tot = s + nparity * (q + (r == 0 ? 0 : 1));

	/*
	 * acols: The columns that will be accessed.
	 * scols: The columns that will be accessed or skipped.
	 */
	if (q == 0) {
		/* Our I/O request doesn't span all child vdevs. */
		acols = bc;
		scols = MIN(dcols, roundup(bc, nparity + 1));
	} else {
		acols = dcols;
		scols = dcols;
	}

	ASSERT3U(acols, <=, scols);

	rr = kmem_alloc(offsetof(raidz_row_t, rr_col[scols]), KM_SLEEP);
	rm->rm_row[0] = rr;

	rr->rr_cols = acols;
	rr->rr_scols = scols;
	rr->rr_bigcols = bc;
	rr->rr_missingdata = 0;
	rr->rr_missingparity = 0;
	rr->rr_firstdatacol = nparity;
	rr->rr_abd_empty = NULL;
	rr->rr_nempty = 0;
#ifdef ZFS_DEBUG
	rr->rr_offset = zio->io_offset;
	rr->rr_size = zio->io_size;
#endif

	asize = 0;

	for (c = 0; c < scols; c++) {
		raidz_col_t *rc = &rr->rr_col[c];
		col = f + c;
		coff = o;
		if (col >= dcols) {
			col -= dcols;
			coff += 1ULL << ashift;
		}
		rc->rc_devidx = col;
		rc->rc_offset = coff;
		rc->rc_abd = NULL;
		rc->rc_orig_data = NULL;
		rc->rc_error = 0;
		rc->rc_tried = 0;
		rc->rc_skipped = 0;
		rc->rc_force_repair = 0;
		rc->rc_allow_repair = 1;
		rc->rc_need_orig_restore = B_FALSE;

		if (c >= acols)
			rc->rc_size = 0;
		else if (c < bc)
			rc->rc_size = (q + 1) << ashift;
		else
			rc->rc_size = q << ashift;

		asize += rc->rc_size;
	}

	ASSERT3U(asize, ==, tot << ashift);
	rm->rm_nskip = roundup(tot, nparity + 1) - tot;
	rm->rm_skipstart = bc;

	/*
	 * If all data stored spans all columns, there's a danger that parity
	 * will always be on the same device and, since parity isn't read
	 * during normal operation, that device's I/O bandwidth won't be
	 * used effectively. We therefore switch the parity every 1MB.
	 *
	 * ... at least that was, ostensibly, the theory. As a practical
	 * matter unless we juggle the parity between all devices evenly, we
	 * won't see any benefit. Further, occasional writes that aren't a
	 * multiple of the LCM of the number of children and the minimum
	 * stripe width are sufficient to avoid pessimal behavior.
	 * Unfortunately, this decision created an implicit on-disk format
	 * requirement that we need to support for all eternity, but only
	 * for single-parity RAID-Z.
	 *
	 * If we intend to skip a sector in the zeroth column for padding
	 * we must make sure to note this swap. We will never intend to
	 * skip the first column since at least one data and one parity
	 * column must appear in each row.
	 */
	ASSERT(rr->rr_cols >= 2);
	ASSERT(rr->rr_col[0].rc_size == rr->rr_col[1].rc_size);

	if (rr->rr_firstdatacol == 1 && (zio->io_offset & (1ULL << 20))) {
		devidx = rr->rr_col[0].rc_devidx;
		o = rr->rr_col[0].rc_offset;
		rr->rr_col[0].rc_devidx = rr->rr_col[1].rc_devidx;
		rr->rr_col[0].rc_offset = rr->rr_col[1].rc_offset;
		rr->rr_col[1].rc_devidx = devidx;
		rr->rr_col[1].rc_offset = o;

		if (rm->rm_skipstart == 0)
			rm->rm_skipstart = 1;
	}

  for (c = rr->rr_firstdatacol; c < rr->rr_cols; c++)
    {
      raidz_col_t *rc = &rr->rr_col[c];
      rc->rc_offset += VDEV_LABEL_START_SIZE;

      const uint64_t col_size = MIN(actual_size, rc->rc_size);

      libzdb_record_t *record = malloc(sizeof(libzdb_record_t));
      record->type = RAIDZ;
      record->col = c;
      record->vdevidx = rc->rc_devidx;
      record->dev = (char *)backing[rc->rc_devidx];
      record->offset = rc->rc_offset;
      record->size = col_size;
      libzdb_list_pushback(&dva->records, record);

      actual_size -= col_size;
    }

  free (rm);
}
