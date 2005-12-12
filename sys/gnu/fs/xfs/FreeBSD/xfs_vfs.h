/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 *
 * http://www.sgi.com
 *
 * For further information regarding this notice, see:
 *
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */
#ifndef __XFS_VFS_H__
#define __XFS_VFS_H__

#include <sys/mount.h>
#include "xfs_fs.h"

struct fid;
struct cred;
struct xfs_vnode;
struct statfs;
struct sbuf;
struct xfs_mount_args;
struct mount;

typedef struct statfs xfs_statfs_t;

typedef struct xfs_vfs {
	u_int			vfs_flag;	/* flags */
	xfs_fsid_t		vfs_fsid;	/* file system ID */
	xfs_fsid_t		*vfs_altfsid;	/* An ID fixed for life of FS */
	bhv_head_t		vfs_bh;		/* head of vfs behavior chain */
	struct mount		*vfs_mp;	/* FreeBSD mount struct */
} xfs_vfs_t;

#define	MNTTOXVFS(mp)		((struct xfs_vfs*)(mp)->mnt_data)
#define	XVFSTOMNT(vfs)		((vfs)->vfs_mp)

#define vfs_fbhv		vfs_bh.bh_first	/* 1st on vfs behavior chain */

#define bhvtovfs(bdp)		( (struct xfs_vfs *)BHV_VOBJ(bdp) )
#define bhvtovfsops(bdp)	( (struct xvfsops *)BHV_OPS(bdp) )
#define VFS_BHVHEAD(vfs)	( &(vfs)->vfs_bh )
#define VFS_REMOVEBHV(vfs, bdp)	( bhv_remove(VFS_BHVHEAD(vfs), bdp) )

#define VFS_POSITION_BASE	BHV_POSITION_BASE	/* chain bottom */
#define VFS_POSITION_TOP	BHV_POSITION_TOP	/* chain top */
#define VFS_POSITION_INVALID	BHV_POSITION_INVALID	/* invalid pos. num */

typedef enum {
	VFS_BHV_UNKNOWN,	/* not specified */
	VFS_BHV_XFS,		/* xfs */
	VFS_BHV_DM,		/* data migration */
	VFS_BHV_QM,		/* quota manager */
	VFS_BHV_IO,		/* IO path */
	VFS_BHV_END		/* housekeeping end-of-range */
} vfs_bhv_t;

#define VFS_POSITION_XFS	(BHV_POSITION_BASE)
#define VFS_POSITION_DM		(VFS_POSITION_BASE+10)
#define VFS_POSITION_QM		(VFS_POSITION_BASE+20)
#define VFS_POSITION_IO		(VFS_POSITION_BASE+30)

#define VFS_RDONLY		0x0001	/* read-only vfs */
#define VFS_GRPID		0x0002	/* group-ID assigned from directory */
#define VFS_DMI			0x0004	/* filesystem has the DMI enabled */
#define VFS_UMOUNT		0x0008	/* unmount in progress */
#define VFS_END			0x0008	/* max flag */

#define SYNC_ATTR		0x0001	/* sync attributes */
#define SYNC_CLOSE		0x0002	/* close file system down */
#define SYNC_DELWRI		0x0004	/* look at delayed writes */
#define SYNC_WAIT		0x0008	/* wait for i/o to complete */
#define SYNC_BDFLUSH		0x0010	/* BDFLUSH is calling -- don't block */
#define SYNC_FSDATA		0x0020	/* flush fs data (e.g. superblocks) */
#define SYNC_REFCACHE		0x0040  /* prune some of the nfs ref cache */
#define SYNC_REMOUNT		0x0080  /* remount readonly, no dummy LRs */

#define IGET_NOALLOC		0x0001	/* vfs_get_inode may return NULL */

typedef int	(*xvfs_mount_t)(bhv_desc_t *,
				struct xfs_mount_args *, struct cred *);
typedef int	(*xvfs_parseargs_t)(bhv_desc_t *, char *,
				struct xfs_mount_args *, int);
typedef	int	(*xvfs_showargs_t)(bhv_desc_t *, struct sbuf *);
typedef int	(*xvfs_unmount_t)(bhv_desc_t *, int, struct cred *);
typedef int	(*xvfs_mntupdate_t)(bhv_desc_t *, int *,
				struct xfs_mount_args *);
typedef int	(*xvfs_root_t)(bhv_desc_t *, struct xfs_vnode **);
typedef int	(*xvfs_statvfs_t)(bhv_desc_t *, xfs_statfs_t *, struct xfs_vnode *);
typedef int	(*xvfs_sync_t)(bhv_desc_t *, int, struct cred *);
typedef int	(*xvfs_vget_t)(bhv_desc_t *, struct xfs_vnode **, struct fid *);
typedef int	(*xvfs_dmapiops_t)(bhv_desc_t *, caddr_t);
typedef int	(*xvfs_quotactl_t)(bhv_desc_t *, int, int, caddr_t);
typedef void	(*xvfs_init_vnode_t)(bhv_desc_t *,
				struct xfs_vnode *, bhv_desc_t *, int);
typedef void	(*xvfs_force_shutdown_t)(bhv_desc_t *, int, char *, int);
typedef	struct inode * (*xvfs_get_inode_t)(bhv_desc_t *, xfs_ino_t, int);

typedef struct xvfsops {
	bhv_position_t		xvfs_position;	/* behavior chain position */
	xvfs_mount_t		xvfs_mount;	/* mount file system */
	xvfs_parseargs_t	xvfs_parseargs;	/* parse mount options */
	xvfs_showargs_t		xvfs_showargs;	/* unparse mount options */
	xvfs_unmount_t		xvfs_unmount;	/* unmount file system */
	xvfs_mntupdate_t	xvfs_mntupdate;	/* update file system options */
	xvfs_root_t		xvfs_root;	/* get root vnode */
	xvfs_statvfs_t		xvfs_statvfs;	/* file system statistics */
	xvfs_sync_t		xvfs_sync;	/* flush files */
	xvfs_vget_t		xvfs_vget;	/* get vnode from fid */
	xvfs_dmapiops_t		xvfs_dmapiops;	/* data migration */
	xvfs_quotactl_t		xvfs_quotactl;	/* disk quota */
	xvfs_get_inode_t	xvfs_get_inode;	/* bhv specific iget */
	xvfs_init_vnode_t	xvfs_init_vnode;	/* initialize a new vnode */
	xvfs_force_shutdown_t	xvfs_force_shutdown;	/* crash and burn */
} xvfsops_t;

/*
 * VFS's.  Operates on vfs structure pointers (starts at bhv head).
 */
#define VHEAD(v)			((v)->vfs_fbhv)
#define XVFS_MOUNT(v, ma,cr, rv)	((rv) = xvfs_mount(VHEAD(v), ma,cr))
#define XVFS_PARSEARGS(v, o,ma,f, rv)	((rv) = xvfs_parseargs(VHEAD(v), o,ma,f))
#define XVFS_SHOWARGS(v, m, rv)		((rv) = xvfs_showargs(VHEAD(v), m))
#define XVFS_UNMOUNT(v, f, cr, rv)	((rv) = xvfs_unmount(VHEAD(v), f,cr))
#define XVFS_MNTUPDATE(v, fl, args, rv)	((rv) = xvfs_mntupdate(VHEAD(v), fl, args))
#define XVFS_ROOT(v, vpp, rv)		((rv) = xvfs_root(VHEAD(v), vpp))
#define XVFS_STATVFS(v, sp,vp, rv)	((rv) = xvfs_statvfs(VHEAD(v), sp,vp))
#define XVFS_SYNC(v, flag,cr, rv)	((rv) = xvfs_sync(VHEAD(v), flag,cr))
#define XVFS_VGET(v, vpp,fidp, rv)	((rv) = xvfs_vget(VHEAD(v), vpp,fidp))
#define XVFS_DMAPIOPS(v, p, rv)		((rv) = xvfs_dmapiops(VHEAD(v), p))
#define XVFS_QUOTACTL(v, c,id,p, rv)	((rv) = xvfs_quotactl(VHEAD(v), c,id,p))
#define XVFS_GET_INODE(v, ino, fl)	( xvfs_get_inode(VHEAD(v), ino,fl) )
#define XVFS_INIT_VNODE(v, vp,b,ul)	( xvfs_init_vnode(VHEAD(v), vp,b,ul) )
#define XVFS_FORCE_SHUTDOWN(v, fl,f,l)	( xvfs_force_shutdown(VHEAD(v), fl,f,l) )

/*
 * PVFS's.  Operates on behavior descriptor pointers.
 */
#define PVFS_MOUNT(b, ma,cr, rv)	((rv) = xvfs_mount(b, ma,cr))
#define PVFS_PARSEARGS(b, o,ma,f, rv)	((rv) = xvfs_parseargs(b, o,ma,f))
#define PVFS_SHOWARGS(b, m, rv)		((rv) = xvfs_showargs(b, m))
#define PVFS_UNMOUNT(b, f,cr, rv)	((rv) = xvfs_unmount(b, f,cr))
#define PVFS_MNTUPDATE(b, fl, args, rv)	((rv) = xvfs_mntupdate(b, fl, args))
#define PVFS_ROOT(b, vpp, rv)		((rv) = xvfs_root(b, vpp))
#define PVFS_STATVFS(b, sp,vp, rv)	((rv) = xvfs_statvfs(b, sp,vp))
#define PVFS_SYNC(b, flag,cr, rv)	((rv) = xvfs_sync(b, flag,cr))
#define PVFS_VGET(b, vpp,fidp, rv)	((rv) = xvfs_vget(b, vpp,fidp))
#define PVFS_DMAPIOPS(b, p, rv)		((rv) = xvfs_dmapiops(b, p))
#define PVFS_QUOTACTL(b, c,id,p, rv)	((rv) = xvfs_quotactl(b, c,id,p))
#define PVFS_GET_INODE(b, ino,fl)	( xvfs_get_inode(b, ino,fl) )
#define PVFS_INIT_VNODE(b, vp,b2,ul)	( xvfs_init_vnode(b, vp,b2,ul) )
#define PVFS_FORCE_SHUTDOWN(b, fl,f,l)	( xvfs_force_shutdown(b, fl,f,l) )

extern int xvfs_mount(bhv_desc_t *, struct xfs_mount_args *, struct cred *);
extern int xvfs_parseargs(bhv_desc_t *, char *, struct xfs_mount_args *, int);
extern int xvfs_showargs(bhv_desc_t *, struct sbuf *);
extern int xvfs_unmount(bhv_desc_t *, int, struct cred *);
extern int xvfs_mntupdate(bhv_desc_t *, int *, struct xfs_mount_args *);
extern int xvfs_root(bhv_desc_t *, struct xfs_vnode **);
extern int xvfs_statvfs(bhv_desc_t *, xfs_statfs_t *, struct xfs_vnode *);
extern int xvfs_sync(bhv_desc_t *, int, struct cred *);
extern int xvfs_vget(bhv_desc_t *, struct xfs_vnode **, struct fid *);
extern int xvfs_dmapiops(bhv_desc_t *, caddr_t);
extern int xvfs_quotactl(bhv_desc_t *, int, int, caddr_t);
extern struct inode *xvfs_get_inode(bhv_desc_t *, xfs_ino_t, int);
extern void xvfs_init_vnode(bhv_desc_t *, struct xfs_vnode *, bhv_desc_t *, int);
extern void xvfs_force_shutdown(bhv_desc_t *, int, char *, int);

#define XFS_DMOPS		"xfs_dm_operations"	/* Data Migration */
#define XFS_QMOPS		"xfs_qm_operations"	/* Quota Manager  */
#define XFS_IOOPS		"xfs_io_operations"	/* I/O subsystem  */
#define XFS_DM_MODULE		"xfs_dmapi"
#define XFS_QM_MODULE		"xfs_quota"
#define XFS_IO_MODULE		"xfs_ioops"

typedef struct bhv_vfsops {
	struct xvfsops		bhv_common;
	void *			bhv_custom;
} bhv_vfsops_t;

typedef struct bhv_module {
	bhv_desc_t		bm_desc;
	const char *		bm_name;
	bhv_vfsops_t *		bm_ops;
} bhv_module_t;

#define vfs_bhv_lookup(v, id)	( bhv_lookup_range(&(v)->vfs_bh, (id), (id)) )
#define vfs_bhv_custom(b)	( ((bhv_vfsops_t *)BHV_OPS(b))->bhv_custom )
#define vfs_bhv_set_custom(b,o)	( (b)->bhv_custom = (void *)(o))
#define vfs_bhv_clr_custom(b)	( (b)->bhv_custom = NULL )

extern xfs_vfs_t *vfs_allocate(struct mount *);
extern void vfs_deallocate(xfs_vfs_t *);
extern void vfs_insertops(xfs_vfs_t *, bhv_vfsops_t *);
extern void vfs_insertbhv(xfs_vfs_t *, bhv_desc_t *, xvfsops_t *, void *);

#define bhv_lookup_module(n,m)	( (m) ? \
				inter_module_get_request(n, m) : \
				inter_module_get(n) )
#define bhv_remove_module(n)	inter_module_put(n)
#define bhv_module_init(n,m,op)	inter_module_register(n,m,op)
#define bhv_module_exit(n)	inter_module_unregister(n)

extern void bhv_insert_all_vfsops(struct xfs_vfs *);
extern void bhv_remove_all_vfsops(struct xfs_vfs *, int);
extern void bhv_remove_vfsops(struct xfs_vfs *, int);

#endif	/* __XFS_VFS_H__ */
