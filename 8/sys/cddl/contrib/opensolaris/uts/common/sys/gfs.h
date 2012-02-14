/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
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
 * Copyright (c) 2004, 2010, Oracle and/or its affiliates. All rights reserved.
 */

/*
 * These are Consolidation Private interfaces and are subject to change.
 */

#ifndef _SYS_GFS_H
#define	_SYS_GFS_H

#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/mutex.h>
#include <sys/dirent.h>
#include <sys/extdirent.h>
#include <sys/uio.h>
#include <sys/list.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	GFS_CACHE_VNODE		0x1

typedef struct gfs_dirent {
	char			*gfse_name;	/* entry name */
	vnode_t *(*gfse_ctor)(vnode_t *);	/* constructor */
	int			gfse_flags;	/* flags */
	list_node_t		gfse_link;	/* dynamic list */
	vnode_t			*gfse_vnode;	/* cached vnode */
} gfs_dirent_t;

typedef enum gfs_type {
	GFS_DIR,
	GFS_FILE
} gfs_type_t;

typedef struct gfs_file {
	vnode_t		*gfs_vnode;	/* current vnode */
	vnode_t		*gfs_parent;	/* parent vnode */
	size_t		gfs_size;	/* size of private data structure */
	gfs_type_t	gfs_type;	/* type of vnode */
	int		gfs_index;	/* index in parent dir */
	ino64_t		gfs_ino;	/* inode for this vnode */
} gfs_file_t;

typedef int (*gfs_readdir_cb)(vnode_t *, void *, int *, offset_t *,
    offset_t *, void *, int);
typedef int (*gfs_lookup_cb)(vnode_t *, const char *, vnode_t **, ino64_t *,
    cred_t *, int, int *, pathname_t *);
typedef ino64_t (*gfs_inode_cb)(vnode_t *, int);

typedef struct gfs_dir {
	gfs_file_t	gfsd_file;	/* generic file attributes */
	gfs_dirent_t	*gfsd_static;	/* statically defined entries */
	int		gfsd_nstatic;	/* # static entries */
	kmutex_t	gfsd_lock;	/* protects entries */
	int		gfsd_maxlen;	/* maximum name length */
	gfs_readdir_cb	gfsd_readdir;	/* readdir() callback */
	gfs_lookup_cb	gfsd_lookup;	/* lookup() callback */
	gfs_inode_cb	gfsd_inode;	/* get an inode number */
} gfs_dir_t;

struct vfs;

extern vnode_t *gfs_file_create(size_t, vnode_t *, vfs_t *, vnodeops_t *);
extern vnode_t *gfs_dir_create(size_t, vnode_t *, vfs_t *, vnodeops_t *,
    gfs_dirent_t *, gfs_inode_cb, int, gfs_readdir_cb, gfs_lookup_cb);
extern vnode_t *gfs_root_create(size_t, vfs_t *, vnodeops_t *, ino64_t,
    gfs_dirent_t *, gfs_inode_cb, int, gfs_readdir_cb, gfs_lookup_cb);
extern vnode_t *gfs_root_create_file(size_t, struct vfs *, vnodeops_t *,
    ino64_t);

extern void *gfs_file_inactive(vnode_t *);
extern void *gfs_dir_inactive(vnode_t *);

extern int gfs_dir_case_lookup(vnode_t *, const char *, vnode_t **, cred_t *,
    int, int *, pathname_t *);
extern int gfs_dir_lookup(vnode_t *, const char *, vnode_t **, cred_t *,
    int, int *, pathname_t *);
extern int gfs_vop_lookup(vnode_t *, char *, vnode_t **, pathname_t *,
    int, vnode_t *, cred_t *, caller_context_t *, int *, pathname_t *);
extern int gfs_dir_readdir(vnode_t *, uio_t *, int *, int *, u_long **, void *,
    cred_t *, int flags);

#define	gfs_dir_lock(gd)	mutex_enter(&(gd)->gfsd_lock)
#define	gfs_dir_unlock(gd)	mutex_exit(&(gd)->gfsd_lock)
#define	GFS_DIR_LOCKED(gd)	MUTEX_HELD(&(gd)->gfsd_lock)

#define	gfs_file_parent(vp)	(((gfs_file_t *)(vp)->v_data)->gfs_parent)

#define	gfs_file_index(vp)	(((gfs_file_t *)(vp)->v_data)->gfs_index)
#define	gfs_file_set_index(vp, idx)	\
	(((gfs_file_t *)(vp)->v_data)->gfs_index = (idx))

#define	gfs_file_inode(vp)	(((gfs_file_t *)(vp)->v_data)->gfs_ino)
#define	gfs_file_set_inode(vp, ino)	\
	(((gfs_file_t *)(vp)->v_data)->gfs_ino = (ino))

typedef struct gfs_readdir_state {
	void		*grd_dirent;	/* directory entry buffer */
	size_t		grd_namlen;	/* max file name length */
	size_t		grd_ureclen;	/* exported record size */
	ssize_t		grd_oresid;	/* original uio_resid */
	ino64_t		grd_parent;	/* inode of parent */
	ino64_t		grd_self;	/* inode of self */
	int		grd_flags;	/* flags from VOP_READDIR */
} gfs_readdir_state_t;

extern int gfs_readdir_init(gfs_readdir_state_t *, int, int, uio_t *, ino64_t,
    ino64_t, int);
extern int gfs_readdir_emit(gfs_readdir_state_t *, uio_t *, offset_t, ino64_t,
    const char *, int, int *, u_long **);
extern int gfs_readdir_pred(gfs_readdir_state_t *, uio_t *, offset_t *, int *,
    u_long **);
extern int gfs_readdir_fini(gfs_readdir_state_t *, int, int *, int);
extern int gfs_get_parent_ino(vnode_t *, cred_t *, caller_context_t *,
    ino64_t *, ino64_t *);

/*
 * Objects with real extended attributes will get their . and ..
 * readdir entries from the real xattr directory. GFS_STATIC_ENTRY_OFFSET
 * lets us skip right to the static entries in the GFS directory.
 */
#define	GFS_STATIC_ENTRY_OFFSET	((offset_t)2)

extern int gfs_lookup_dot(vnode_t **, vnode_t *, vnode_t *, const char *);

extern int gfs_vop_readdir(struct vop_readdir_args *);
extern int gfs_vop_inactive(struct vop_inactive_args *);


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_GFS_H */
