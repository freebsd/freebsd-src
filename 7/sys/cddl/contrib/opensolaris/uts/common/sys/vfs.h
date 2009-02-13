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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1983, 1984, 1985, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * Portions of this source code were derived from Berkeley 4.3 BSD
 * under license from the Regents of the University of California.
 */

#ifndef _SYS_VFS_H
#define	_SYS_VFS_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/statvfs.h>
#include <sys/refstr.h>
#include <sys/avl.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Data associated with mounted file systems.
 */

/*
 * Operations vector.  This is used internal to the kernel; file systems
 * supply their list of operations via vfs_setfsops().
 */

typedef struct vfsops vfsops_t;

/*
 * File system identifier. Should be unique (at least per machine).
 */
typedef struct {
	int val[2];			/* file system id type */
} fsid_t;

/*
 * File identifier.  Should be unique per filesystem on a single
 * machine.  This is typically called by a stateless file server
 * in order to generate "file handles".
 *
 * Do not change the definition of struct fid ... fid_t without
 * letting the CacheFS group know about it!  They will have to do at
 * least two things, in the same change that changes this structure:
 *   1. change CFSVERSION in usr/src/uts/common/sys/fs/cachefs_fs.h
 *   2. put the old version # in the canupgrade array
 *	in cachfs_upgrade() in usr/src/cmd/fs.d/cachefs/fsck/fsck.c
 * This is necessary because CacheFS stores FIDs on disk.
 *
 * Many underlying file systems cast a struct fid into other
 * file system dependent structures which may require 4 byte alignment.
 * Because a fid starts with a short it may not be 4 byte aligned, the
 * fid_pad will force the alignment.
 */
#define	MAXFIDSZ	64
#define	OLD_MAXFIDSZ	16

typedef struct fid {
	union {
		long fid_pad;
		struct {
			ushort_t len;	/* length of data in bytes */
			char	data[MAXFIDSZ]; /* data (variable len) */
		} _fid;
	} un;
} fid_t;

#ifdef _SYSCALL32
/*
 * Solaris 64 - use old-style cache format with 32-bit aligned fid for on-disk
 * struct compatibility.
 */
typedef struct fid32 {
	union {
		int32_t fid_pad;
		struct {
			uint16_t  len;   /* length of data in bytes */
			char    data[MAXFIDSZ]; /* data (variable len) */
		} _fid;
	} un;
} fid32_t;
#else /* not _SYSCALL32 */
#define	fid32	fid
typedef fid_t	fid32_t;
#endif /* _SYSCALL32 */

#define	fid_len		un._fid.len
#define	fid_data	un._fid.data

/*
 * Structure defining a mount option for a filesystem.
 * option names are found in mntent.h
 */
typedef struct mntopt {
	char	*mo_name;	/* option name */
	char	**mo_cancel;	/* list of options cancelled by this one */
	char	*mo_arg;	/* argument string for this option */
	int	mo_flags;	/* flags for this mount option */
	void	*mo_data;	/* filesystem specific data */
} mntopt_t;

/*
 * Flags that apply to mount options
 */

#define	MO_SET		0x01		/* option is set */
#define	MO_NODISPLAY	0x02		/* option not listed in mnttab */
#define	MO_HASVALUE	0x04		/* option takes a value */
#define	MO_IGNORE	0x08		/* option ignored by parser */
#define	MO_DEFAULT	MO_SET		/* option is on by default */
#define	MO_TAG		0x10		/* flags a tag set by user program */
#define	MO_EMPTY	0x20		/* empty space in option table */

#define	VFS_NOFORCEOPT	0x01		/* honor MO_IGNORE (don't set option) */
#define	VFS_DISPLAY	0x02		/* Turn off MO_NODISPLAY bit for opt */
#define	VFS_NODISPLAY	0x04		/* Turn on MO_NODISPLAY bit for opt */
#define	VFS_CREATEOPT	0x08		/* Create the opt if it's not there */

/*
 * Structure holding mount option strings for the mounted file system.
 */
typedef struct mntopts {
	uint_t		mo_count;		/* number of entries in table */
	mntopt_t	*mo_list;		/* list of mount options */
} mntopts_t;

/*
 * The kstat structures associated with the vopstats are kept in an
 * AVL tree.  This is to avoid the case where a file system does not
 * use a unique fsid_t for each vfs (e.g., namefs).  In order to do
 * this, we need a structure that the AVL tree can use that also
 * references the kstat.
 * Note that the vks_fsid is generated from the value reported by
 * VFS_STATVFS().
 */
typedef struct vskstat_anchor {
	avl_node_t	vsk_node;	/* Required for use by AVL routines */
	kstat_t		*vsk_ksp;	/* kstat structure for vopstats */
	ulong_t		vsk_fsid;	/* fsid associated w/this FS */
} vsk_anchor_t;

extern avl_tree_t	vskstat_tree;
extern kmutex_t		vskstat_tree_lock;

/*
 * Structure per mounted file system.  Each mounted file system has
 * an array of operations and an instance record.
 *
 * The file systems are kept on a doubly linked circular list headed by
 * "rootvfs".
 * File system implementations should not access this list;
 * it's intended for use only in the kernel's vfs layer.
 *
 * Each zone also has its own list of mounts, containing filesystems mounted
 * somewhere within the filesystem tree rooted at the zone's rootpath.  The
 * list is doubly linked to match the global list.
 *
 * mnttab locking: the in-kernel mnttab uses the vfs_mntpt, vfs_resource and
 * vfs_mntopts fields in the vfs_t. mntpt and resource are refstr_ts that
 * are set at mount time and can only be modified during a remount.
 * It is safe to read these fields if you can prevent a remount on the vfs,
 * or through the convenience funcs vfs_getmntpoint() and vfs_getresource().
 * The mntopts field may only be accessed through the provided convenience
 * functions, as it is protected by the vfs list lock. Modifying a mount
 * option requires grabbing the vfs list write lock, which can be a very
 * high latency lock.
 */
struct zone;		/* from zone.h */
struct fem_head;	/* from fem.h */

/*
 * Private vfs data, NOT to be used by a file system implementation.
 */
typedef struct vfs_impl {
	struct fem_head	*vi_femhead;		/* fs monitoring */
	/*
	 * Support for statistics on the vnode operations
	 */
	vsk_anchor_t	*vi_vskap;		/* anchor for vopstats' kstat */
	vopstats_t	*vi_fstypevsp;		/* ptr to per-fstype vopstats */
	vopstats_t	vi_vopstats;		/* per-mount vnode op stats */
} vfs_impl_t;

typedef struct vfs {
	struct vfs	*vfs_next;		/* next VFS in VFS list */
	struct vfs	*vfs_prev;		/* prev VFS in VFS list */

/* vfs_op should not be used directly.  Accessor functions are provided */
	vfsops_t	*vfs_op;		/* operations on VFS */

	struct vnode	*vfs_vnodecovered;	/* vnode mounted on */
	uint_t		vfs_flag;		/* flags */
	uint_t		vfs_bsize;		/* native block size */
	int		vfs_fstype;		/* file system type index */
	fsid_t		vfs_fsid;		/* file system id */
	void		*vfs_data;		/* private data */
	dev_t		vfs_dev;		/* device of mounted VFS */
	ulong_t		vfs_bcount;		/* I/O count (accounting) */
	struct vfs	*vfs_list;		/* sync list pointer */
	struct vfs	*vfs_hash;		/* hash list pointer */
	ksema_t		vfs_reflock;		/* mount/unmount/sync lock */
	uint_t		vfs_count;		/* vfs reference count */
	mntopts_t	vfs_mntopts;		/* options mounted with */
	refstr_t	*vfs_resource;		/* mounted resource name */
	refstr_t	*vfs_mntpt;		/* mount point name */
	time_t		vfs_mtime;		/* time we were mounted */
	vfs_impl_t 	*vfs_implp;		/* impl specific data */
	/*
	 * Zones support.  Note that the zone that "owns" the mount isn't
	 * necessarily the same as the zone in which the zone is visible.
	 * That is, vfs_zone and (vfs_zone_next|vfs_zone_prev) may refer to
	 * different zones.
	 */
	struct zone	*vfs_zone;		/* zone that owns the mount */
	struct vfs	*vfs_zone_next;		/* next VFS visible in zone */
	struct vfs	*vfs_zone_prev;		/* prev VFS visible in zone */
} vfs_t;

#define	vfs_femhead	vfs_implp->vi_femhead
#define	vfs_vskap	vfs_implp->vi_vskap
#define	vfs_fstypevsp	vfs_implp->vi_fstypevsp
#define	vfs_vopstats	vfs_implp->vi_vopstats

/*
 * VFS flags.
 */
#define	VFS_RDONLY	0x01		/* read-only vfs */
#define	VFS_NOMNTTAB	0x02		/* vfs not seen in mnttab */
#define	VFS_NOSETUID	0x08		/* setuid disallowed */
#define	VFS_REMOUNT	0x10		/* modify mount options only */
#define	VFS_NOTRUNC	0x20		/* does not truncate long file names */
#define	VFS_UNLINKABLE	0x40		/* unlink(2) can be applied to root */
#define	VFS_PXFS	0x80		/* clustering: global fs proxy vfs */
#define	VFS_UNMOUNTED	0x100		/* file system has been unmounted */
#define	VFS_NBMAND	0x200		/* allow non-blocking mandatory locks */
#define	VFS_XATTR	0x400		/* fs supports extended attributes */
#define	VFS_NODEVICES	0x800		/* device-special files disallowed */
#define	VFS_NOEXEC	0x1000		/* executables disallowed */
#define	VFS_STATS	0x2000		/* file system can collect stats */

#define	VFS_NORESOURCE	"unspecified_resource"
#define	VFS_NOMNTPT	"unspecified_mountpoint"

/*
 * Argument structure for mount(2).
 *
 * Flags are defined in <sys/mount.h>.
 *
 * Note that if the MS_SYSSPACE bit is set in flags, the pointer fields in
 * this structure are to be interpreted as kernel addresses.  File systems
 * should be prepared for this possibility.
 */
struct mounta {
	char	*spec;
	char	*dir;
	int	flags;
	char	*fstype;
	char	*dataptr;
	int	datalen;
	char	*optptr;
	int	optlen;
};

/*
 * Reasons for calling the vfs_mountroot() operation.
 */
enum whymountroot { ROOT_INIT, ROOT_REMOUNT, ROOT_UNMOUNT};
typedef enum whymountroot whymountroot_t;

/*
 * Reasons for calling the VFS_VNSTATE():
 */
enum vntrans {
	VNTRANS_EXISTS,
	VNTRANS_IDLED,
	VNTRANS_RECLAIMED,
	VNTRANS_DESTROYED
};
typedef enum vntrans vntrans_t;

/*
 * VFS_OPS defines all the vfs operations.  It is used to define
 * the vfsops structure (below) and the fs_func_p union (vfs_opreg.h).
 */
#define	VFS_OPS								\
	int	(*vfs_mount)(vfs_t *, vnode_t *, struct mounta *, cred_t *); \
	int	(*vfs_unmount)(vfs_t *, int, cred_t *);			\
	int	(*vfs_root)(vfs_t *, vnode_t **);			\
	int	(*vfs_statvfs)(vfs_t *, statvfs64_t *);			\
	int	(*vfs_sync)(vfs_t *, short, cred_t *);			\
	int	(*vfs_vget)(vfs_t *, vnode_t **, fid_t *);		\
	int	(*vfs_mountroot)(vfs_t *, enum whymountroot);		\
	void	(*vfs_freevfs)(vfs_t *);				\
	int	(*vfs_vnstate)(vfs_t *, vnode_t *, vntrans_t)	/* NB: No ";" */

/*
 * Operations supported on virtual file system.
 */
struct vfsops {
	VFS_OPS;	/* Signature of all vfs operations (vfsops) */
};

extern int	fsop_mount(vfs_t *, vnode_t *, struct mounta *, cred_t *);
extern int	fsop_unmount(vfs_t *, int, cred_t *);
extern int	fsop_root(vfs_t *, vnode_t **);
extern int	fsop_statfs(vfs_t *, statvfs64_t *);
extern int	fsop_sync(vfs_t *, short, cred_t *);
extern int	fsop_vget(vfs_t *, vnode_t **, fid_t *);
extern int	fsop_mountroot(vfs_t *, enum whymountroot);
extern void	fsop_freefs(vfs_t *);
extern int	fsop_sync_by_kind(int, short, cred_t *);
extern int	fsop_vnstate(vfs_t *, vnode_t *, vntrans_t);

#define	VFS_MOUNT(vfsp, mvp, uap, cr) fsop_mount(vfsp, mvp, uap, cr)
#define	VFS_UNMOUNT(vfsp, flag, cr) fsop_unmount(vfsp, flag, cr)
#define	VFS_ROOT(vfsp, vpp) fsop_root(vfsp, vpp)
#define	VFS_STATVFS(vfsp, sp) fsop_statfs(vfsp, sp)
#define	VFS_SYNC(vfsp, flag, cr) fsop_sync(vfsp, flag, cr)
#define	VFS_VGET(vfsp, vpp, fidp) fsop_vget(vfsp, vpp, fidp)
#define	VFS_MOUNTROOT(vfsp, init) fsop_mountroot(vfsp, init)
#define	VFS_FREEVFS(vfsp) fsop_freefs(vfsp)
#define	VFS_VNSTATE(vfsp, vn, ns)	fsop_vnstate(vfsp, vn, ns)

#define	VFSNAME_MOUNT		"mount"
#define	VFSNAME_UNMOUNT		"unmount"
#define	VFSNAME_ROOT		"root"
#define	VFSNAME_STATVFS		"statvfs"
#define	VFSNAME_SYNC		"sync"
#define	VFSNAME_VGET		"vget"
#define	VFSNAME_MOUNTROOT	"mountroot"
#define	VFSNAME_FREEVFS		"freevfs"
#define	VFSNAME_VNSTATE		"vnstate"
/*
 * Filesystem type switch table.
 */

typedef struct vfssw {
	char		*vsw_name;	/* type name -- max len _ST_FSTYPSZ */
	int		(*vsw_init) (int, char *);
				/* init routine (for non-loadable fs only) */
	int		vsw_flag;	/* flags */
	mntopts_t	vsw_optproto;	/* mount options table prototype */
	uint_t		vsw_count;	/* count of references */
	kmutex_t	vsw_lock;	/* lock to protect vsw_count */
	vfsops_t	vsw_vfsops;	/* filesystem operations vector */
} vfssw_t;

/*
 * Filesystem type definition record.  All file systems must export a record
 * of this type through their modlfs structure.
 */

typedef struct vfsdef_v3 {
	int		def_version;	/* structure version, must be first */
	char		*name;		/* filesystem type name */
	int		(*init) (int, char *);	/* init routine */
	int		flags;		/* filesystem flags */
	mntopts_t	*optproto;	/* mount options table prototype */
} vfsdef_v3;

typedef struct vfsdef_v3 vfsdef_t;

enum {
	VFSDEF_VERSION = 3
};

/*
 * flags for vfssw and vfsdef
 */
#define	VSW_HASPROTO	0x01	/* struct has a mount options prototype */
#define	VSW_CANRWRO	0x02	/* file system can transition from rw to ro */
#define	VSW_CANREMOUNT	0x04	/* file system supports remounts */
#define	VSW_NOTZONESAFE	0x08	/* zone_enter(2) should fail for these files */
#define	VSW_VOLATILEDEV	0x10	/* vfs_dev can change each time fs is mounted */
#define	VSW_STATS	0x20	/* file system can collect stats */

#define	VSW_INSTALLED	0x8000	/* this vsw is associated with a file system */

#if defined(_KERNEL)
/*
 * Public operations.
 */
struct umounta;
struct statvfsa;
struct fstatvfsa;

void	vfs_freevfsops(vfsops_t *);
int	vfs_freevfsops_by_type(int);
void	vfs_setops(vfs_t *, vfsops_t *);
vfsops_t *vfs_getops(vfs_t *vfsp);
int	vfs_matchops(vfs_t *, vfsops_t *);
int	vfs_can_sync(vfs_t *vfsp);
void	vfs_init(vfs_t *vfsp, vfsops_t *, void *);
void	vfsimpl_setup(vfs_t *vfsp);
void	vfsimpl_teardown(vfs_t *vfsp);
void	vn_exists(vnode_t *);
void	vn_idle(vnode_t *);
void	vn_reclaim(vnode_t *);
void	vn_invalid(vnode_t *);

int	rootconf(void);
int	svm_rootconf(void);
int	domount(char *, struct mounta *, vnode_t *, struct cred *,
	    struct vfs **);
int	dounmount(struct vfs *, int, cred_t *);
int	vfs_lock(struct vfs *);
int	vfs_rlock(struct vfs *);
void	vfs_lock_wait(struct vfs *);
void	vfs_rlock_wait(struct vfs *);
void	vfs_unlock(struct vfs *);
int	vfs_lock_held(struct vfs *);
struct	_kthread *vfs_lock_owner(struct vfs *);
void	sync(void);
void	vfs_sync(int);
void	vfs_mountroot(void);
void	vfs_add(vnode_t *, struct vfs *, int);
void	vfs_remove(struct vfs *);

/* The following functions are not for general use by filesystems */

void	vfs_createopttbl(mntopts_t *, const char *);
void	vfs_copyopttbl(const mntopts_t *, mntopts_t *);
void	vfs_mergeopttbl(const mntopts_t *, const mntopts_t *, mntopts_t *);
void	vfs_freeopttbl(mntopts_t *);
void	vfs_parsemntopts(mntopts_t *, char *, int);
int	vfs_buildoptionstr(const mntopts_t *, char *, int);
struct mntopt *vfs_hasopt(const mntopts_t *, const char *);
void	vfs_mnttab_modtimeupd(void);

void	vfs_clearmntopt(struct vfs *, const char *);
void	vfs_setmntopt(struct vfs *, const char *, const char *, int);
void	vfs_setresource(struct vfs *, const char *);
void	vfs_setmntpoint(struct vfs *, const char *);
refstr_t *vfs_getresource(const struct vfs *);
refstr_t *vfs_getmntpoint(const struct vfs *);
int	vfs_optionisset(const struct vfs *, const char *, char **);
int	vfs_settag(uint_t, uint_t, const char *, const char *, cred_t *);
int	vfs_clrtag(uint_t, uint_t, const char *, const char *, cred_t *);
void	vfs_syncall(void);
void	vfs_syncprogress(void);
void	vfsinit(void);
void	vfs_unmountall(void);
void	vfs_make_fsid(fsid_t *, dev_t, int);
void	vfs_addmip(dev_t, struct vfs *);
void	vfs_delmip(struct vfs *);
int	vfs_devismounted(dev_t);
int	vfs_devmounting(dev_t, struct vfs *);
int	vfs_opsinuse(vfsops_t *);
struct vfs *getvfs(fsid_t *);
struct vfs *vfs_dev2vfsp(dev_t);
struct vfs *vfs_mntpoint2vfsp(const char *);
struct vfssw *allocate_vfssw(char *);
struct vfssw *vfs_getvfssw(char *);
struct vfssw *vfs_getvfsswbyname(char *);
struct vfssw *vfs_getvfsswbyvfsops(vfsops_t *);
void	vfs_refvfssw(struct vfssw *);
void	vfs_unrefvfssw(struct vfssw *);
uint_t	vf_to_stf(uint_t);
void	vfs_mnttab_modtime(timespec_t *);
void	vfs_mnttab_poll(timespec_t *, struct pollhead **);

void	vfs_list_lock(void);
void	vfs_list_read_lock(void);
void	vfs_list_unlock(void);
void	vfs_list_add(struct vfs *);
void	vfs_list_remove(struct vfs *);
void	vfs_hold(vfs_t *vfsp);
void	vfs_rele(vfs_t *vfsp);
void	fs_freevfs(vfs_t *);
void	vfs_root_redev(vfs_t *vfsp, dev_t ndev, int fstype);

int	vfs_zone_change_safe(vfs_t *);

#define	VFSHASH(maj, min) (((int)((maj)+(min))) & (vfshsz - 1))
#define	VFS_ON_LIST(vfsp) \
	((vfsp)->vfs_next != (vfsp) && (vfsp)->vfs_next != NULL)

/*
 * Globals.
 */

extern struct vfssw vfssw[];		/* table of filesystem types */
extern krwlock_t vfssw_lock;
extern char rootfstype[];		/* name of root fstype */
extern const int nfstype;		/* # of elements in vfssw array */
extern vfsops_t *EIO_vfsops;		/* operations for vfs being torn-down */

/*
 * The following variables are private to the the kernel's vfs layer.  File
 * system implementations should not access them.
 */
extern struct vfs *rootvfs;		/* ptr to root vfs structure */
typedef struct {
	struct vfs *rvfs_head;		/* head vfs in chain */
	kmutex_t rvfs_lock;		/* mutex protecting this chain */
	uint32_t rvfs_len;		/* length of this chain */
} rvfs_t;
extern rvfs_t *rvfs_list;
extern int vfshsz;			/* # of elements in rvfs_head array */
extern const mntopts_t vfs_mntopts;	/* globally recognized options */

#endif /* defined(_KERNEL) */

#define	VFS_HOLD(vfsp) { \
	vfs_hold(vfsp); \
}

#define	VFS_RELE(vfsp)	{ \
	vfs_rele(vfsp); \
}

#define	VFS_INIT(vfsp, op, data) { \
	vfs_init((vfsp), (op), (data)); \
	vfsimpl_setup((vfsp)); \
}


#define	VFS_INSTALLED(vfsswp)	(((vfsswp)->vsw_flag & VSW_INSTALLED) != 0)
#define	ALLOCATED_VFSSW(vswp)		((vswp)->vsw_name[0] != '\0')
#define	RLOCK_VFSSW()			(rw_enter(&vfssw_lock, RW_READER))
#define	RUNLOCK_VFSSW()			(rw_exit(&vfssw_lock))
#define	WLOCK_VFSSW()			(rw_enter(&vfssw_lock, RW_WRITER))
#define	WUNLOCK_VFSSW()			(rw_exit(&vfssw_lock))
#define	VFSSW_LOCKED()			(RW_LOCK_HELD(&vfssw_lock))
#define	VFSSW_WRITE_LOCKED()		(RW_WRITE_HELD(&vfssw_lock))
/*
 * VFS_SYNC flags.
 */
#define	SYNC_ATTR	0x01		/* sync attributes only */
#define	SYNC_CLOSE	0x02		/* close open file */
#define	SYNC_ALL	0x04		/* force to sync all fs */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_VFS_H */
