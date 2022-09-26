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

/* libzdb context */
typedef struct libzdb libzdb_t;

/* call these functions in this order */

/* global initialization */
void libzdb_init();

/* cache reusable dataset data */
libzdb_t *libzdb_ds_init(char *zpool_name);

/* look up data dvas (can be called multiple times for the same dataset) */
void libzdb_get_dvas(libzdb_t *ctx, char *path);

/* clean up zpool vdevs cache */
void libzdb_zpool_fini(libzdb_t *ctx);

/* global teardown */
void libzdb_fini();
