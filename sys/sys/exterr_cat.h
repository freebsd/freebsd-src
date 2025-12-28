/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software were developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

/*
 * The category identifiers for the extended errors.
 * The ids participate in ABI between kernel and libc, so they must
 * never be reused or changed.  Only new ids can be added.
 *
 * After adding a new category id, run
 * tools/build/make_libc_exterr_cat_filenames.sh
 * from the top of the source tree, and commit updated file
 * lib/libc/gen/exterr_cat_filenames.h
 */

#ifndef _SYS_EXTERR_CAT_H_
#define	_SYS_EXTERR_CAT_H_

#define	EXTERR_CAT_MMAP		1
#define	EXTERR_CAT_FILEDESC	2
#define	EXTERR_KTRACE		3	/* To allow inclusion of this
					   file into kern_ktrace.c */
#define	EXTERR_CAT_FUSE_VNOPS	4
#define	EXTERR_CAT_INOTIFY	5
#define	EXTERR_CAT_GENIO	6
#define	EXTERR_CAT_BRIDGE	7
#define	EXTERR_CAT_SWAP		8
#define	EXTERR_CAT_VFSSYSCALL	9
#define	EXTERR_CAT_VFSBIO	10
#define	EXTERR_CAT_GEOMVFS	11
#define	EXTERR_CAT_GEOM		12
#define	EXTERR_CAT_FUSE_VFS	13
#define	EXTERR_CAT_FUSE_DEVICE	14

#endif

