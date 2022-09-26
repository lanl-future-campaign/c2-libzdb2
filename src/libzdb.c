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
 * Copyright (c) 2011, 2019 by Delphix. All rights reserved.
 * Copyright (c) 2014 Integros [integros.com]
 * Copyright 2016 Nexenta Systems, Inc.
 * Copyright (c) 2017, 2018 Lawrence Livermore National Security, LLC.
 * Copyright (c) 2015, 2017, Intel Corporation.
 * Copyright (c) 2020 Datto Inc.
 * Copyright (c) 2020, The FreeBSD Foundation [1]
 *
 * [1] Portions of this software were developed by Allan Jude
 *     under sponsorship from the FreeBSD Foundation.
 * Copyright (c) 2021 Allan Jude
 * Copyright (c) 2021 Toomas Soome <tsoome@me.com>
 * Copyright (c) 2022 Triad National Security, LLC as operator of Los Alamos
 *     National Laboratory. All rights reserved.
 */
#include "c2_libnvpair.h"
#include "list.h"
#include "c2_vdev_raidz.h"

#include <sys/dbuf.h>
#include <sys/dmu.h>
#include <sys/dmu_objset.h>
#include <sys/dsl_dataset.h>
#include <sys/stat.h>
#include <sys/vdev_impl.h>
#include <sys/zap.h>
#include <sys/zfs_acl.h>
#include <sys/zfs_context.h>
#include <sys/zfs_sa.h>
#include <sys/zfs_znode.h>
#include <sys/zio.h>

/* a single block of data */
typedef struct info
{
  uint64_t file_offset;
  uint64_t vdev;
  uint64_t offset;
  uint64_t asize;
} info_t;

/* a single vdev within a zpool */
typedef struct zpool_vdev
{
  char **names;
  zpool_type_t type;
  size_t count;
  size_t nparity;
  size_t ashift;
} zpool_vdev_t;

/* a single zpool */
typedef struct zpool_vdevs
{
  zpool_vdev_t *vdevs;
  size_t count;
} zpool_vdevs_t;

typedef struct libzdb {
    char *ds;
    objset_t *os;
    uint64_t root_obj;
    sa_attr_type_t *sa_attr_table;
    zpool_vdevs_t *vdevs;
} libzdb_t;

static int
open_objset(libzdb_t *ctx, void *tag)
{
	int err;
	uint64_t sa_attrs = 0;
	uint64_t version = 0;

	/*
	 * We can't own an objset if it's redacted.  Therefore, we do this
	 * dance: hold the objset, then acquire a long hold on its dataset, then
	 * release the pool (which is held as part of holding the objset).
	 */
	err = dmu_objset_hold(ctx->ds, tag, &ctx->os);
	if (err != 0) {
		(void) fprintf(stderr, "failed to hold dataset '%s': %s\n",
		    ctx->ds, strerror(err));
		return (err);
	}
	dsl_dataset_long_hold(dmu_objset_ds(ctx->os), tag);
	dsl_pool_rele(dmu_objset_pool(ctx->os), tag);

	if (dmu_objset_type(ctx->os) == DMU_OST_ZFS && !ctx->os->os_encrypted) {
		(void) zap_lookup(ctx->os, MASTER_NODE_OBJ, ZPL_VERSION_STR,
		    8, 1, &version);
		if (version >= ZPL_VERSION_SA) {
			(void) zap_lookup(ctx->os, MASTER_NODE_OBJ, ZFS_SA_ATTRS,
			    8, 1, &sa_attrs);
		}
		err = sa_setup(ctx->os, sa_attrs, zfs_attr_table, ZPL_END,
		    &ctx->sa_attr_table);
		if (err != 0) {
			(void) fprintf(stderr, "sa_setup failed: %s\n",
			    strerror(err));
			dsl_dataset_long_rele(dmu_objset_ds(ctx->os), tag);
			dsl_dataset_rele(dmu_objset_ds(ctx->os), tag);
			ctx->os = NULL;
		}
	}

	return (0);
}

static void
close_objset(libzdb_t *ctx, const void *tag)
{
	if (ctx->os->os_sa != NULL)
		sa_tear_down(ctx->os);
	dsl_dataset_long_rele(dmu_objset_ds(ctx->os), tag);
	dsl_dataset_rele(dmu_objset_ds(ctx->os), tag);
	ctx->sa_attr_table = NULL;
}

static void
snprintf_blkptr_compact(char *blkbuf, size_t buflen, const blkptr_t *bp,
                        info_t *info) {
    const dva_t *dva = bp->blk_dva;
    int ndvas = 1;
    int i;

    if (BP_IS_EMBEDDED(bp)) {
        sprintf(blkbuf, "EMBEDDED et=%u %llxL/%llxP B=%llu",
                (int)BPE_GET_ETYPE(bp),(u_longlong_t)BPE_GET_LSIZE(bp),
                (u_longlong_t)BPE_GET_PSIZE(bp),(u_longlong_t)bp->blk_birth);
        return;
    }

    blkbuf[0] = '\0';

    /* data blocks should only have 1 dva */
    for(i = 0; i < ndvas; i++) {
        /* snprintf(blkbuf + strlen(blkbuf), buflen - strlen(blkbuf), */
        /*           "%llu:%llx:%llx ",(u_longlong_t)DVA_GET_VDEV(&dva[i]), */
        /*          (u_longlong_t)DVA_GET_OFFSET(&dva[i]), */
        /*          (u_longlong_t)DVA_GET_ASIZE(&dva[i])); */
        if (BP_GET_LEVEL(bp) == 0) {
            info->vdev = DVA_GET_VDEV(&dva[i]);
            info->offset = DVA_GET_OFFSET(&dva[i]);
            info->asize = DVA_GET_ASIZE(&dva[i]);
        }
    }
}

static uint64_t
blkid2offset(const dnode_phys_t *dnp, const blkptr_t *bp,
             const zbookmark_phys_t *zb) {
  if (dnp == NULL) {
      ASSERT(zb->zb_level < 0);
      if (zb->zb_object == 0)
          return(zb->zb_blkid);
      return(zb->zb_blkid * BP_GET_LSIZE(bp));
  }

  ASSERT(zb->zb_level >= 0);

  return((zb->zb_blkid <<
     (zb->zb_level *(dnp->dn_indblkshift - SPA_BLKPTRSHIFT))) *
      dnp->dn_datablkszsec << SPA_MINBLOCKSHIFT);
}

static void
print_indirect(blkptr_t *bp, const zbookmark_phys_t *zb,
               const dnode_phys_t *dnp, c2list_t *list) {
    char blkbuf[BP_SPRINTF_LEN];
    int l;

    if (!BP_IS_EMBEDDED(bp)) {
        ASSERT3U(BP_GET_TYPE(bp), ==, dnp->dn_type);
        ASSERT3U(BP_GET_LEVEL(bp), ==, zb->zb_level);
    }

    /* printf("%16llx ",(u_longlong_t)blkid2offset(dnp, bp, zb)); */

    /* ASSERT(zb->zb_level >= 0); */

    /* for(l = dnp->dn_nlevels - 1; l >= -1; l--) { */
    /*     if (l == zb->zb_level) { */
    /*         printf("L%llx",(u_longlong_t)zb->zb_level); */
    /*     } */
    /*     else { */
    /*         printf(" "); */
    /*     } */
    /* } */

    info_t *info = malloc(sizeof(info_t));
    snprintf_blkptr_compact(blkbuf, sizeof(blkbuf), bp, info);
    if (BP_GET_LEVEL(bp) == 0) {
        info->file_offset = blkid2offset(dnp, bp, zb);
        c2list_pushback(list, info);
    }
    else {
        free(info);
    }

    /* printf("%s\n", blkbuf); */
}

static int
visit_indirect(spa_t *spa, const dnode_phys_t *dnp, blkptr_t *bp,
               const zbookmark_phys_t *zb, c2list_t *list) {
    int err = 0;

    if (bp->blk_birth == 0)
        return(0);

    print_indirect(bp, zb, dnp, list);

    if (BP_GET_LEVEL(bp) > 0 && !BP_IS_HOLE(bp)) {
        arc_flags_t flags = ARC_FLAG_WAIT;
        int i;
        blkptr_t *cbp;
        int epb = BP_GET_LSIZE(bp) >> SPA_BLKPTRSHIFT;
        arc_buf_t *buf;
        uint64_t fill = 0;

        err = arc_read(NULL, spa, bp, arc_getbuf_func, &buf,
                       ZIO_PRIORITY_ASYNC_READ, ZIO_FLAG_CANFAIL, &flags, zb);
        if (err)
            return(err);
        ASSERT(buf->b_data);

        /* recursively visit blocks below this */
        cbp = buf->b_data;
        for(i = 0; i < epb; i++, cbp++) {
            zbookmark_phys_t czb;

            SET_BOOKMARK(&czb, zb->zb_objset, zb->zb_object, zb->zb_level - 1,
                         zb->zb_blkid * epb + i);
            err = visit_indirect(spa, dnp, cbp, &czb, list);
            if (err)
                break;
            fill += BP_GET_FILL(cbp);
        }
        if (!err)
            ASSERT3U(fill, ==, BP_GET_FILL(bp));
        arc_buf_destroy(buf, &buf);
    }

    return(err);
}

static void
dump_indirect(dnode_t *dn, const size_t file_size, c2list_t *list) {
    dnode_phys_t *dnp = dn->dn_phys;
    zbookmark_phys_t czb;

    SET_BOOKMARK(&czb, dmu_objset_id(dn->dn_objset),
        dn->dn_object, dnp->dn_nlevels - 1, 0);
    for(int j = 0; j < dnp->dn_nblkptr; j++) {
        czb.zb_blkid = j;
        (void) visit_indirect(dmu_objset_spa(dn->dn_objset), dnp,
            &dnp->dn_blkptr[j], &czb, list);
    }

    /* printf("\n"); */
}

static uint64_t
dump_znode(objset_t *os, uint64_t object, void *data, size_t size,
    sa_attr_type_t *sa_attr_table) {
    sa_handle_t *hdl;
    uint64_t fsize;
    sa_bulk_attr_t bulk[1];
    int idx = 0;

    if (sa_handle_get(os, object, NULL, SA_HDL_PRIVATE, &hdl)) {
        printf("Failed to get handle for SA znode\n");
        return 0;
    }

    SA_ADD_BULK_ATTR(bulk, idx, sa_attr_table[ZPL_SIZE], NULL, &fsize, 8);
    if (sa_bulk_lookup(hdl, bulk, idx)) {
        sa_handle_destroy(hdl);
        return 0;
    }

    /* printf("\tsize      %llu\n",(u_longlong_t)fsize); */
    sa_handle_destroy(hdl);
    return fsize;
}

static void
dump_object(libzdb_t *ctx, uint64_t object) {
    dmu_buf_t *db = NULL;
    dmu_object_info_t doi;
    dnode_t *dn = NULL;
    void *bonus = NULL;
    size_t bsize = 0;
    int error;

    error = dmu_object_info(ctx->os, object, &doi);
    if (error) {
        fprintf(stderr, "dmu_object_info() failed, errno %u\n", error);
        return;
    }

    error = dmu_bonus_hold(ctx->os, object, FTAG, &db);
    if (error) {
        fprintf(stderr, "dmu_bonus_hold(%lu) failed, errno %u", object, error);
        return;
    }
    bonus = db->db_data;
    bsize = db->db_size;
    dn = DB_DNODE((dmu_buf_impl_t *)db);

    const uint64_t fsize = dump_znode(ctx->os, object, bonus, bsize, ctx->sa_attr_table);

    c2list_t block_list;
    c2list_init(&block_list);

    dump_indirect(dn, doi.doi_max_offset, &block_list);

    printf("file size: %zu (%zu blocks)\n", fsize, block_list.count);

    /* add extra info to get last chunk's size */
    info_t *extra = malloc(sizeof(info_t));
    extra->file_offset = fsize;
    c2list_pushback(&block_list, extra);

    for(node_t *node = c2list_head(&block_list); node && c2list_next(node);
        node = c2list_next(node)) {
        node_t *next_node = c2list_next(node);

        info_t *info = c2list_get(node);
        info_t *next = c2list_get(next_node);

        zpool_vdev_t *vdev = &ctx->vdevs->vdevs[info->vdev];

        const uint64_t actual_size = next->file_offset - info->file_offset;

        zio_t zio;
        zio.io_offset = info->offset;
        zio.io_size = P2ROUNDUP(actual_size, 1ULL << vdev->ashift);

        printf("file_offset=%ld vdev=%ld io_offset=%ld record_size=%ld\n",
               info->file_offset, info->vdev, info->offset, actual_size);

        switch(vdev->type) {
            case STRIPE:
                if (vdev->count != 1) {
                    fprintf(stderr, "Warning: Found multiple devices when only 1 "
                            "is expected.\n");
                }
                /* fallthrough */
            case MIRROR:
                printf("vdevidx=%ld dev=%s offset=%llu size=%lu\n", info->vdev,
                       vdev->names[0], info->offset + VDEV_LABEL_START_SIZE,
                       actual_size);

                break;
            case RAIDZ:
                c2_vdev_raidz_map_alloc(&zio, vdev->ashift, vdev->count, vdev->nparity,
                                        vdev->names, actual_size);
                break;
            default:
                break;
        }
    }

    c2list_fin(&block_list, free);

    dmu_buf_rele(db, FTAG);
}

static void
cleanup_zpool(vdti_t *zpool, int print, int clean) {
    if (!zpool) {
        return;
    }

    if (print) {
        printf("%s\n", zpool->name);
    }

    size_t vdev_index = 0;
    node_t *vdev_node = c2list_head(&zpool->vdevs);
    while(vdev_node) {
        vdi_t *vdev = c2list_get(vdev_node);

        if (print) {
            printf("    vdev %zu, ashift %zu, count %zu, ", vdev_index,
                   vdev->ashift, vdev->names.count);

            switch(vdev->type)
            {
                case STRIPE:
                    printf("stripe");
                    break;
                case RAIDZ:
                    printf("raidz %zu", vdev->nparity);
                    break;
                case MIRROR:
                    printf("mirror");
                    break;
                default:
                    printf("unknown");
                    break;
            }

            printf("\n");
        }

        size_t dev_index = 0;
        node_t *dev_node = c2list_head(&vdev->names);
        while(dev_node) {
            char *name = c2list_get(dev_node);
            if (print)
            {
                printf("        dev %zu %s\n", dev_index, name);
            }
            dev_node = c2list_next(dev_node);
            dev_index++;
        }

        if (clean) {
            c2list_fin(&vdev->names, NULL);
        }

        vdev_node = c2list_next(vdev_node);
        vdev_index++;
    }

    if (clean) {
        c2list_fin(&zpool->vdevs, free);
    }

    free(zpool);
}

static zpool_vdevs_t *
dump_cachefile(const char *cachefile, const char *zpool_name) {
	int fd;
	struct stat64 statbuf;
	char *buf;
	nvlist_t *config;

	if ((fd = open64(cachefile, O_RDONLY)) < 0) {
		(void) printf("cannot open '%s': %s\n", cachefile,
		    strerror(errno));
		exit(1);
	}

	if (fstat64(fd, &statbuf) != 0) {
		(void) printf("failed to stat '%s': %s\n", cachefile,
		    strerror(errno));
		exit(1);
	}

	if ((buf = malloc(statbuf.st_size)) == NULL) {
		(void) fprintf(stderr, "failed to allocate %llu bytes\n",
		    (u_longlong_t)statbuf.st_size);
		exit(1);
	}

	if (read(fd, buf, statbuf.st_size) != statbuf.st_size) {
		(void) fprintf(stderr, "failed to read %llu bytes\n",
		    (u_longlong_t)statbuf.st_size);
		exit(1);
	}

	(void) close(fd);

	if (nvlist_unpack(buf, statbuf.st_size, &config, 0) != 0) {
		(void) fprintf(stderr, "failed to unpack nvlist\n");
		exit(1);
	}

	free(buf);

    /* generate list of vdev names here, before nvlist_free */

    vdti_t *zpool = NULL;
    c2_dump_nvlist(config, 0, zpool_name, &zpool, NULL);
    if (!zpool) {
        cleanup_zpool(zpool, 0, 1);
        nvlist_free(config);
        return (NULL);
    }


    zpool_vdevs_t *vdevs = malloc(sizeof(zpool_vdevs_t));
    vdevs->count = zpool->vdevs.count;
    vdevs->vdevs = malloc(sizeof(zpool_vdev_t) * vdevs->count);

    /* copy info from each vdev within the current zpool */
    size_t vdevidx = 0;
    for(node_t *zpool_vdev_node = c2list_head(&zpool->vdevs); zpool_vdev_node;
        zpool_vdev_node = c2list_next(zpool_vdev_node)) {
        vdi_t *zpool_vdev = c2list_get(zpool_vdev_node);

        /* set up current vdev */
        zpool_vdev_t *vdev = &vdevs->vdevs[vdevidx];
        vdev->type = zpool_vdev->type;
        vdev->count = zpool_vdev->names.count;
        vdev->names = malloc(sizeof(char *) * vdev->count);
        vdev->nparity = zpool_vdev->nparity;
        vdev->ashift = zpool_vdev->ashift;

        /* explicitly copy vdev backing device names from nvpair tree */
        size_t devidx = 0;
        for(node_t *node = c2list_head(&zpool_vdev->names); node;
            node = c2list_next(node)) {
            const char *path = c2list_get(node);
            const size_t path_len = strlen(path);
            const size_t path_size =(path_len + 1) * sizeof(char);

            vdev->names[devidx] = malloc(path_size);
            snprintf(vdev->names[devidx], path_size, "%s", path);

            devidx++;
        }

        vdevidx++;
    }

    cleanup_zpool(zpool, 0, 1);

    nvlist_free(config);

    return (vdevs);
}

static int
dump_path_impl(libzdb_t *ctx, uint64_t obj, char *name,
               char *curpath) {
    int err;
    uint64_t child_obj;
    char *s;
    dmu_buf_t *db;
    dmu_object_info_t doi;

    if ((s = strchr(name, '/')) != NULL)
        *s = '\0';
    err = zap_lookup(ctx->os, obj, name, 8, 1, &child_obj);

    strlcat(curpath, name, sizeof(curpath));

    if (err != 0) {
        fprintf(stderr, "failed to lookup %s: %s\n", curpath, strerror(err));
        return err;
    }

    child_obj = ZFS_DIRENT_OBJ(child_obj);
    err = sa_buf_hold(ctx->os, child_obj, FTAG, &db);
    if (err != 0) {
        fprintf(stderr, "failed to get SA dbuf for obj %llu: %s\n",
                (u_longlong_t)child_obj, strerror(err));
        return EINVAL;
    }
    dmu_object_info_from_db(db, &doi);
    sa_buf_rele(db, FTAG);

    if (doi.doi_bonus_type != DMU_OT_SA && doi.doi_bonus_type != DMU_OT_ZNODE) {
        fprintf(stderr, "invalid bonus type %d for obj %llu\n",
                doi.doi_bonus_type,(u_longlong_t)child_obj);
        return EINVAL;
    }

    strlcat(curpath, "/", sizeof(curpath));

    switch(doi.doi_type) {
        /* case DMU_OT_DIRECTORY_CONTENTS: */
        /*   if (s != NULL && *(s + 1) != '\0') */
        /*     return dump_path_impl(os, child_obj, s + 1); */
        /*FALLTHROUGH*/
        case DMU_OT_PLAIN_FILE_CONTENTS:
            dump_object(ctx, child_obj);
            return 0;
        default:
            fprintf(stderr,
                    "object %llu has non-file "
                    "type %d\n",
                    (u_longlong_t)obj, doi.doi_type);
            break;
    }

    return EINVAL;
}

static int
dump_ds(libzdb_t *ctx)
{
    int err = 0;

	err = open_objset(ctx, FTAG);
	if (err != 0)
		return (err);

	err = zap_lookup(ctx->os, MASTER_NODE_OBJ, ZFS_ROOT_OBJ, 8, 1, &ctx->root_obj);
	if (err != 0) {
		(void) fprintf(stderr, "can't lookup root znode: %s\n",
		    strerror(err));
		close_objset(ctx, FTAG);
		return (EINVAL);
	}

    return(err);
}

static int
dump_path(libzdb_t *ctx, char *path)
{
    int err = 0;

    char curpath[PATH_MAX];
	(void) snprintf(curpath, sizeof (curpath), "dataset=%s path=/", ctx->ds);

    err = dump_path_impl(ctx, ctx->root_obj, path, curpath);

    return(err);
}

static void
cleanup_vdevs(zpool_vdevs_t *vdevs)
{
    if (!vdevs) {
        return;
    }

    for(size_t i = 0; i < vdevs->count; i++) {
        zpool_vdev_t *vdev = &(vdevs->vdevs[i]);
        for(size_t j = 0; j < vdev->count; j++) {
            free(vdev->names[j]);
        }
        free(vdev->names);
    }
    free(vdevs->vdevs);
    free(vdevs);
}

void libzdb_init()
{
    kernel_init(SPA_MODE_READ);
}

libzdb_t *libzdb_ds_init(char *name)
{
    zpool_vdevs_t *vdevs = dump_cachefile(ZPOOL_CACHE, name);
    if (!vdevs) {
        return (NULL);
    }

    libzdb_t *ctx = calloc(1, sizeof(libzdb_t));
    ctx->ds = name;
    dump_ds(ctx);
    ctx->vdevs = vdevs;

    return ctx;
}

void libzdb_get_dvas(libzdb_t *ctx, char *path)
{
    dump_path(ctx, path);
}

void libzdb_zpool_fini(libzdb_t *ctx)
{
    if (ctx) {
        cleanup_vdevs(ctx->vdevs);
        close_objset(ctx, FTAG);
        free(ctx);
    }
}

void libzdb_fini()
{
    kernel_fini();
}
