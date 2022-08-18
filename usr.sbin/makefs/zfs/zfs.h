/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 The FreeBSD Foundation
 *
 * This software was developed by Mark Johnston under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _MAKEFS_ZFS_H_
#define	_MAKEFS_ZFS_H_

#include <sys/types.h>
#include <sys/endian.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <stdalign.h>
#include <stdbool.h>

#include "makefs.h"

#include "zfs/nvlist.h"
#define	ASSERT		assert
#include "zfs/zfsimpl.h"

#define	MAXBLOCKSHIFT		17	/* 128KB */
#define	MAXBLOCKSIZE		((off_t)(1 << MAXBLOCKSHIFT))
_Static_assert(MAXBLOCKSIZE == SPA_OLDMAXBLOCKSIZE, "");
#define	MINBLOCKSHIFT		9	/* 512B */
#define	MINBLOCKSIZE		((off_t)(1 << MINBLOCKSHIFT))
_Static_assert(MINBLOCKSIZE == SPA_MINBLOCKSIZE, "");
#define	MINDEVSIZE		((off_t)SPA_MINDEVSIZE)

/* All data was written in this transaction group. */
#define	TXG			4

typedef struct zfs_dsl_dataset zfs_dsl_dataset_t;
typedef struct zfs_dsl_dir zfs_dsl_dir_t;
typedef struct zfs_objset zfs_objset_t;
typedef struct zfs_zap zfs_zap_t;

struct dataset_desc {
	char		*params;
	STAILQ_ENTRY(dataset_desc) next;
};

typedef struct {
	/*
	 * Block buffer, needs to be aligned for various on-disk structures,
	 * ZAPs, etc..
	 */
	char		filebuf[MAXBLOCKSIZE] __aligned(alignof(uint64_t));

	bool		nowarn;

	/* Pool parameters. */
	const char	*poolname;
	char		*rootpath;	/* implicit mount point prefix */
	char		*bootfs;	/* bootable dataset, pool property */
	int		ashift;		/* vdev block size */
	uint64_t	mssize;		/* metaslab size */
	STAILQ_HEAD(, dataset_desc) datasetdescs; /* non-root dataset descrs  */

	/* Pool state. */
	uint64_t	poolguid;	/* pool and root vdev GUID */
	zfs_zap_t	*poolprops;

	/* MOS state. */
	zfs_objset_t	*mos;		/* meta object set */
	uint64_t	objarrid;	/* space map object array */

	/* DSL state. */
	zfs_dsl_dir_t	*rootdsldir;	/* root DSL directory */
	zfs_dsl_dataset_t *rootds;
	zfs_dsl_dir_t	*origindsldir;	/* $ORIGIN */
	zfs_dsl_dataset_t *originds;
	zfs_dsl_dataset_t *snapds;
	zfs_zap_t	*cloneszap;
	zfs_dsl_dir_t	*freedsldir;	/* $FREE */
	zfs_dsl_dir_t	*mosdsldir;	/* $MOS */

	/* vdev state. */
	int		fd;		/* vdev disk fd */
	uint64_t	vdevguid;	/* disk vdev GUID */
	off_t		vdevsize;	/* vdev size, including labels */
	off_t		asize;		/* vdev size, excluding labels */
	bitstr_t	*spacemap;	/* space allocation tracking */
	int		spacemapbits;	/* one bit per ashift-sized block */
	uint64_t	msshift;	/* log2(metaslab size) */
	uint64_t	mscount;	/* number of metaslabs for this vdev */
} zfs_opt_t;

/* dsl.c */
void dsl_init(zfs_opt_t *);
const char *dsl_dir_fullname(const zfs_dsl_dir_t *);
uint64_t dsl_dir_id(zfs_dsl_dir_t *);
uint64_t dsl_dir_dataset_id(zfs_dsl_dir_t *);
void dsl_dir_foreach(zfs_opt_t *, zfs_dsl_dir_t *,
    void (*)(zfs_opt_t *, zfs_dsl_dir_t *, void *), void *);
int dsl_dir_get_canmount(zfs_dsl_dir_t *, uint64_t *);
char *dsl_dir_get_mountpoint(zfs_opt_t *, zfs_dsl_dir_t *);
bool dsl_dir_has_dataset(zfs_dsl_dir_t *);
bool dsl_dir_dataset_has_objset(zfs_dsl_dir_t *);
void dsl_dir_dataset_write(zfs_opt_t *, zfs_objset_t *, zfs_dsl_dir_t *);
void dsl_dir_size_add(zfs_dsl_dir_t *, uint64_t);
void dsl_write(zfs_opt_t *);

/* fs.c */
void fs_build(zfs_opt_t *, int, fsnode *);

/* objset.c */
zfs_objset_t *objset_alloc(zfs_opt_t *zfs, uint64_t type);
off_t objset_space_alloc(zfs_opt_t *, zfs_objset_t *, off_t *);
dnode_phys_t *objset_dnode_alloc(zfs_objset_t *, uint8_t, uint64_t *);
dnode_phys_t *objset_dnode_bonus_alloc(zfs_objset_t *, uint8_t, uint8_t,
    uint16_t, uint64_t *);
dnode_phys_t *objset_dnode_lookup(zfs_objset_t *, uint64_t);
void objset_root_blkptr_copy(const zfs_objset_t *, blkptr_t *);
uint64_t objset_space(const zfs_objset_t *);
void objset_write(zfs_opt_t *zfs, zfs_objset_t *os);

/* vdev.c */
void vdev_init(zfs_opt_t *, const char *);
off_t vdev_space_alloc(zfs_opt_t *zfs, off_t *lenp);
void vdev_pwrite_data(zfs_opt_t *zfs, uint8_t datatype, uint8_t cksumtype,
    uint8_t level, uint64_t fill, const void *data, off_t sz, off_t loc,
    blkptr_t *bp);
void vdev_pwrite_dnode_indir(zfs_opt_t *zfs, dnode_phys_t *dnode, uint8_t level,
    uint64_t fill, const void *data, off_t sz, off_t loc, blkptr_t *bp);
void vdev_pwrite_dnode_data(zfs_opt_t *zfs, dnode_phys_t *dnode, const void *data,
    off_t sz, off_t loc);
void vdev_label_write(zfs_opt_t *zfs, int ind, const vdev_label_t *labelp);
void vdev_spacemap_write(zfs_opt_t *);
void vdev_fini(zfs_opt_t *zfs);

/* zap.c */
zfs_zap_t *zap_alloc(zfs_objset_t *, dnode_phys_t *);
void zap_add(zfs_zap_t *, const char *, size_t, size_t, const uint8_t *);
void zap_add_uint64(zfs_zap_t *, const char *, uint64_t);
void zap_add_string(zfs_zap_t *, const char *, const char *);
bool zap_entry_exists(zfs_zap_t *, const char *);
void zap_write(zfs_opt_t *, zfs_zap_t *);

/* zfs.c */
struct dnode_cursor *dnode_cursor_init(zfs_opt_t *, zfs_objset_t *,
    dnode_phys_t *, off_t, off_t);
blkptr_t *dnode_cursor_next(zfs_opt_t *, struct dnode_cursor *, off_t);
void dnode_cursor_finish(zfs_opt_t *, struct dnode_cursor *);

#endif /* !_MAKEFS_ZFS_H_ */
