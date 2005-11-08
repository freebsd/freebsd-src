/*-
 * Copyright (c) 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)mount.h	8.21 (Berkeley) 5/20/95
 * $FreeBSD$
 */

#ifndef _SYS_MOUNT_H_
#define _SYS_MOUNT_H_

#include <sys/ucred.h>
#include <sys/queue.h>
#ifdef _KERNEL
#include <sys/lockmgr.h>
#include <sys/_lock.h>
#include <sys/_mutex.h>
#endif

typedef struct fsid { int32_t val[2]; } fsid_t;	/* filesystem id type */

/*
 * File identifier.
 * These are unique per filesystem on a single machine.
 */
#define	MAXFIDSZ	16

struct fid {
	u_short		fid_len;		/* length of data in bytes */
	u_short		fid_reserved;		/* force longword alignment */
	char		fid_data[MAXFIDSZ];	/* data (variable length) */
};

/*
 * filesystem statistics
 */
#define	MFSNAMELEN	16		/* length of type name including null */
#define	MNAMELEN	88		/* size of on/from name bufs */
#define	STATFS_VERSION	0x20030518	/* current version number */
struct statfs {
	uint32_t f_version;		/* structure version number */
	uint32_t f_type;		/* type of filesystem */
	uint64_t f_flags;		/* copy of mount exported flags */
	uint64_t f_bsize;		/* filesystem fragment size */
	uint64_t f_iosize;		/* optimal transfer block size */
	uint64_t f_blocks;		/* total data blocks in filesystem */
	uint64_t f_bfree;		/* free blocks in filesystem */
	int64_t	 f_bavail;		/* free blocks avail to non-superuser */
	uint64_t f_files;		/* total file nodes in filesystem */
	int64_t	 f_ffree;		/* free nodes avail to non-superuser */
	uint64_t f_syncwrites;		/* count of sync writes since mount */
	uint64_t f_asyncwrites;		/* count of async writes since mount */
	uint64_t f_syncreads;		/* count of sync reads since mount */
	uint64_t f_asyncreads;		/* count of async reads since mount */
	uint64_t f_spare[10];		/* unused spare */
	uint32_t f_namemax;		/* maximum filename length */
	uid_t	  f_owner;		/* user that mounted the filesystem */
	fsid_t	  f_fsid;		/* filesystem id */
	char	  f_charspare[80];	    /* spare string space */
	char	  f_fstypename[MFSNAMELEN]; /* filesystem type name */
	char	  f_mntfromname[MNAMELEN];  /* mounted filesystem */
	char	  f_mntonname[MNAMELEN];    /* directory on which mounted */
};

#ifdef _KERNEL
#define	OMFSNAMELEN	16	/* length of fs type name, including null */
#define	OMNAMELEN	(88 - 2 * sizeof(long))	/* size of on/from name bufs */

/* XXX getfsstat.2 is out of date with write and read counter changes here. */
/* XXX statfs.2 is out of date with read counter changes here. */
struct ostatfs {
	long	f_spare2;		/* placeholder */
	long	f_bsize;		/* fundamental filesystem block size */
	long	f_iosize;		/* optimal transfer block size */
	long	f_blocks;		/* total data blocks in filesystem */
	long	f_bfree;		/* free blocks in fs */
	long	f_bavail;		/* free blocks avail to non-superuser */
	long	f_files;		/* total file nodes in filesystem */
	long	f_ffree;		/* free file nodes in fs */
	fsid_t	f_fsid;			/* filesystem id */
	uid_t	f_owner;		/* user that mounted the filesystem */
	int	f_type;			/* type of filesystem */
	int	f_flags;		/* copy of mount exported flags */
	long	f_syncwrites;		/* count of sync writes since mount */
	long	f_asyncwrites;		/* count of async writes since mount */
	char	f_fstypename[OMFSNAMELEN]; /* fs type name */
	char	f_mntonname[OMNAMELEN];	/* directory on which mounted */
	long	f_syncreads;		/* count of sync reads since mount */
	long	f_asyncreads;		/* count of async reads since mount */
	short	f_spares1;		/* unused spare */
	char	f_mntfromname[OMNAMELEN];/* mounted filesystem */
	short	f_spares2;		/* unused spare */
	/*
	 * XXX on machines where longs are aligned to 8-byte boundaries, there
	 * is an unnamed int32_t here.  This spare was after the apparent end
	 * of the struct until we bit off the read counters from f_mntonname.
	 */
	long	f_spare[2];		/* unused spare */
};

#define	MMAXOPTIONLEN	65536		/* maximum length of a mount option */

TAILQ_HEAD(vnodelst, vnode);

struct vfsoptlist;
struct vfsopt;

/*
 * Structure per mounted filesystem.  Each mounted filesystem has an
 * array of operations and an instance record.  The filesystems are
 * put on a doubly linked list.
 *
 */
struct mount {
	TAILQ_ENTRY(mount) mnt_list;		/* mount list */
	struct vfsops	*mnt_op;		/* operations on fs */
	struct vfsconf	*mnt_vfc;		/* configuration info */
	struct vnode	*mnt_vnodecovered;	/* vnode we mounted on */
	struct vnode	*mnt_syncer;		/* syncer vnode */
	struct vnodelst	mnt_nvnodelist;		/* list of vnodes this mount */
	struct lock	mnt_lock;		/* mount structure lock */
	struct mtx	mnt_mtx;		/* mount structure interlock */
	int		mnt_writeopcount;	/* write syscalls in progress */
	u_int		mnt_flag;		/* flags shared with user */
	struct vfsoptlist *mnt_opt;		/* current mount options */
	struct vfsoptlist *mnt_optnew;		/* new options passed to fs */
	int		mnt_kern_flag;		/* kernel only flags */
	int		mnt_maxsymlinklen;	/* max size of short symlink */
	struct statfs	mnt_stat;		/* cache of filesystem stats */
	struct ucred	*mnt_cred;		/* credentials of mounter */
	void *		mnt_data;		/* private data */
	time_t		mnt_time;		/* last time written*/
	int		mnt_iosize_max;		/* max size for clusters, etc */
	struct netexport *mnt_export;		/* export list */
	struct label	*mnt_mntlabel;		/* MAC label for the mount */
	struct label	*mnt_fslabel;		/* MAC label for the fs */
	int		mnt_nvnodelistsize;	/* # of vnodes on this mount */
	u_int		mnt_hashseed;		/* Random seed for vfs_hash */
};

struct vnode *__mnt_vnode_next(struct vnode **nvp, struct mount *mp);

#define MNT_VNODE_FOREACH(vp, mp, vp2) \
	for ((vp2) = TAILQ_FIRST(&(mp)->mnt_nvnodelist);	\
		(vp = __mnt_vnode_next(&(vp2), (mp))) != NULL;)


#define	MNT_ILOCK(mp)	mtx_lock(&(mp)->mnt_mtx)
#define	MNT_IUNLOCK(mp)	mtx_unlock(&(mp)->mnt_mtx)
#define	MNT_MTX(mp)	(&(mp)->mnt_mtx)

#endif /* _KERNEL */

/*
 * User specifiable flags.
 */
#define	MNT_RDONLY	0x00000001	/* read only filesystem */
#define	MNT_SYNCHRONOUS	0x00000002	/* filesystem written synchronously */
#define	MNT_NOEXEC	0x00000004	/* can't exec from filesystem */
#define	MNT_NOSUID	0x00000008	/* don't honor setuid bits on fs */
#define	MNT_NODEV	0		/* Deprecated option */
#define	MNT_UNION	0x00000020	/* union with underlying filesystem */
#define	MNT_ASYNC	0x00000040	/* filesystem written asynchronously */
#define	MNT_SUIDDIR	0x00100000	/* special handling of SUID on dirs */
#define	MNT_SOFTDEP	0x00200000	/* soft updates being done */
#define	MNT_NOSYMFOLLOW	0x00400000	/* do not follow symlinks */
#define	MNT_MULTILABEL	0x04000000	/* MAC support for individual objects */
#define	MNT_ACLS	0x08000000	/* ACL support enabled */
#define	MNT_NOATIME	0x10000000	/* disable update of file access time */
#define	MNT_NOCLUSTERR	0x40000000	/* disable cluster read */
#define	MNT_NOCLUSTERW	0x80000000	/* disable cluster write */

/*
 * NFS export related mount flags.
 */
#define	MNT_EXRDONLY	0x00000080	/* exported read only */
#define	MNT_EXPORTED	0x00000100	/* filesystem is exported */
#define	MNT_DEFEXPORTED	0x00000200	/* exported to the world */
#define	MNT_EXPORTANON	0x00000400	/* use anon uid mapping for everyone */
#define	MNT_EXKERB	0x00000800	/* exported with Kerberos uid mapping */
#define	MNT_EXPUBLIC	0x20000000	/* public export (WebNFS) */

/*
 * Flags set by internal operations,
 * but visible to the user.
 * XXX some of these are not quite right.. (I've never seen the root flag set)
 */
#define	MNT_LOCAL	0x00001000	/* filesystem is stored locally */
#define	MNT_QUOTA	0x00002000	/* quotas are enabled on filesystem */
#define	MNT_ROOTFS	0x00004000	/* identifies the root filesystem */
#define	MNT_USER	0x00008000	/* mounted by a user */
#define	MNT_IGNORE	0x00800000	/* do not show entry in df */

/*
 * Mask of flags that are visible to statfs().
 * XXX I think that this could now become (~(MNT_CMDFLAGS))
 * but the 'mount' program may need changing to handle this.
 */
#define	MNT_VISFLAGMASK	(MNT_RDONLY	| MNT_SYNCHRONOUS | MNT_NOEXEC	| \
			MNT_NOSUID	| MNT_UNION	| \
			MNT_ASYNC	| MNT_EXRDONLY	| MNT_EXPORTED	| \
			MNT_DEFEXPORTED	| MNT_EXPORTANON| MNT_EXKERB	| \
			MNT_LOCAL	| MNT_USER	| MNT_QUOTA	| \
			MNT_ROOTFS	| MNT_NOATIME	| MNT_NOCLUSTERR| \
			MNT_NOCLUSTERW	| MNT_SUIDDIR	| MNT_SOFTDEP	| \
			MNT_IGNORE	| MNT_EXPUBLIC	| MNT_NOSYMFOLLOW | \
			MNT_MULTILABEL	| MNT_ACLS)

/* Mask of flags that can be updated. */
#define	MNT_UPDATEMASK (MNT_NOSUID	| MNT_NOEXEC	| \
			MNT_SYNCHRONOUS	| MNT_UNION	| MNT_ASYNC	| \
			MNT_NOATIME | \
			MNT_NOSYMFOLLOW	| MNT_IGNORE	| \
			MNT_NOCLUSTERR	| MNT_NOCLUSTERW | MNT_SUIDDIR	| \
			MNT_ACLS	| MNT_USER)

/*
 * External filesystem command modifier flags.
 * Unmount can use the MNT_FORCE flag.
 * XXX These are not STATES and really should be somewhere else.
 */
#define	MNT_UPDATE	0x00010000	/* not a real mount, just an update */
#define	MNT_DELEXPORT	0x00020000	/* delete export host lists */
#define	MNT_RELOAD	0x00040000	/* reload filesystem data */
#define	MNT_FORCE	0x00080000	/* force unmount or readonly change */
#define	MNT_SNAPSHOT	0x01000000	/* snapshot the filesystem */
#define	MNT_BYFSID	0x08000000	/* specify filesystem by ID. */
#define MNT_CMDFLAGS   (MNT_UPDATE	| MNT_DELEXPORT	| MNT_RELOAD	| \
			MNT_FORCE	| MNT_SNAPSHOT	| MNT_BYFSID)
/*
 * Still available.
 */
#define	MNT_SPARE_0x02000000	0x02000000
/*
 * Internal filesystem control flags stored in mnt_kern_flag.
 *
 * MNTK_UNMOUNT locks the mount entry so that name lookup cannot proceed
 * past the mount point.  This keeps the subtree stable during mounts
 * and unmounts.
 *
 * MNTK_UNMOUNTF permits filesystems to detect a forced unmount while
 * dounmount() is still waiting to lock the mountpoint. This allows
 * the filesystem to cancel operations that might otherwise deadlock
 * with the unmount attempt (used by NFS).
 */
#define MNTK_UNMOUNTF	0x00000001	/* forced unmount in progress */
#define MNTK_UNMOUNT	0x01000000	/* unmount in progress */
#define	MNTK_MWAIT	0x02000000	/* waiting for unmount to finish */
#define	MNTK_SUSPEND	0x08000000	/* request write suspension */
#define	MNTK_SUSPENDED	0x10000000	/* write operations are suspended */
#define	MNTK_MPSAFE	0x20000000	/* Filesystem is MPSAFE. */
#define	MNTK_NOKNOTE	0x80000000	/* Don't send KNOTEs from VOP hooks */

/*
 * Sysctl CTL_VFS definitions.
 *
 * Second level identifier specifies which filesystem. Second level
 * identifier VFS_VFSCONF returns information about all filesystems.
 * Second level identifier VFS_GENERIC is non-terminal.
 */
#define	VFS_VFSCONF		0	/* get configured filesystems */
#define	VFS_GENERIC		0	/* generic filesystem information */
/*
 * Third level identifiers for VFS_GENERIC are given below; third
 * level identifiers for specific filesystems are given in their
 * mount specific header files.
 */
#define VFS_MAXTYPENUM	1	/* int: highest defined filesystem type */
#define VFS_CONF	2	/* struct: vfsconf for filesystem given
				   as next argument */

/*
 * Flags for various system call interfaces.
 *
 * waitfor flags to vfs_sync() and getfsstat()
 */
#define MNT_WAIT	1	/* synchronously wait for I/O to complete */
#define MNT_NOWAIT	2	/* start all I/O, but do not wait for it */
#define MNT_LAZY	3	/* push data not written by filesystem syncer */

/*
 * Generic file handle
 */
struct fhandle {
	fsid_t	fh_fsid;	/* Filesystem id of mount point */
	struct	fid fh_fid;	/* Filesys specific id */
};
typedef struct fhandle	fhandle_t;

/*
 * Export arguments for local filesystem mount calls.
 */
struct export_args {
	int	ex_flags;		/* export related flags */
	uid_t	ex_root;		/* mapping for root uid */
	struct	xucred ex_anon;		/* mapping for anonymous user */
	struct	sockaddr *ex_addr;	/* net address to which exported */
	u_char	ex_addrlen;		/* and the net address length */
	struct	sockaddr *ex_mask;	/* mask of valid bits in saddr */
	u_char	ex_masklen;		/* and the smask length */
	char	*ex_indexfile;		/* index file for WebNFS URLs */
};

/*
 * Structure holding information for a publicly exported filesystem
 * (WebNFS). Currently the specs allow just for one such filesystem.
 */
struct nfs_public {
	int		np_valid;	/* Do we hold valid information */
	fhandle_t	np_handle;	/* Filehandle for pub fs (internal) */
	struct mount	*np_mount;	/* Mountpoint of exported fs */
	char		*np_index;	/* Index file */
};

/*
 * Filesystem configuration information. One of these exists for each
 * type of filesystem supported by the kernel. These are searched at
 * mount time to identify the requested filesystem.
 *
 * XXX: Never change the first two arguments!
 */
struct vfsconf {
	u_int	vfc_version;		/* ABI version number */
	char	vfc_name[MFSNAMELEN];	/* filesystem type name */
	struct	vfsops *vfc_vfsops;	/* filesystem operations vector */
	int	vfc_typenum;		/* historic filesystem type number */
	int	vfc_refcount;		/* number mounted of this type */
	int	vfc_flags;		/* permanent flags */
	struct	vfsoptdecl *vfc_opts;	/* mount options */
	TAILQ_ENTRY(vfsconf) vfc_list;	/* list of vfscons */
};

/* Userland version of the struct vfsconf. */
struct xvfsconf {
	struct	vfsops *vfc_vfsops;	/* filesystem operations vector */
	char	vfc_name[MFSNAMELEN];	/* filesystem type name */
	int	vfc_typenum;		/* historic filesystem type number */
	int	vfc_refcount;		/* number mounted of this type */
	int	vfc_flags;		/* permanent flags */
	struct	vfsconf *vfc_next;	/* next in list */
};

#ifndef BURN_BRIDGES
struct ovfsconf {
	void	*vfc_vfsops;
	char	vfc_name[32];
	int	vfc_index;
	int	vfc_refcount;
	int	vfc_flags;
};
#endif

/*
 * NB: these flags refer to IMPLEMENTATION properties, not properties of
 * any actual mounts; i.e., it does not make sense to change the flags.
 */
#define	VFCF_STATIC	0x00010000	/* statically compiled into kernel */
#define	VFCF_NETWORK	0x00020000	/* may get data over the network */
#define	VFCF_READONLY	0x00040000	/* writes are not implemented */
#define VFCF_SYNTHETIC	0x00080000	/* data does not represent real files */
#define	VFCF_LOOPBACK	0x00100000	/* aliases some other mounted FS */
#define	VFCF_UNICODE	0x00200000	/* stores file names as Unicode*/

typedef uint32_t fsctlop_t;

struct vfsidctl {
	int		vc_vers;	/* should be VFSIDCTL_VERS1 (below) */
	fsid_t		vc_fsid;	/* fsid to operate on. */
	char		vc_fstypename[MFSNAMELEN];
					/* type of fs 'nfs' or '*' */
	fsctlop_t	vc_op;		/* operation VFS_CTL_* (below) */
	void		*vc_ptr;	/* pointer to data structure. */
	size_t		vc_len;		/* sizeof said structure. */
	u_int32_t	vc_spare[12];	/* spare (must be zero). */
};

/* vfsidctl API version. */
#define VFS_CTL_VERS1	0x01

/*
 * New style VFS sysctls, do not reuse/conflict with the namespace for
 * private sysctls.
 * All "global" sysctl ops have the 33rd bit set:
 * 0x...1....
 * Private sysctl ops should have the 33rd bit unset.
 */
#define VFS_CTL_QUERY	0x00010001	/* anything wrong? (vfsquery) */
#define VFS_CTL_TIMEO	0x00010002	/* set timeout for vfs notification */
#define VFS_CTL_NOLOCKS	0x00010003	/* disable file locking */

struct vfsquery {
	u_int32_t	vq_flags;
	u_int32_t	vq_spare[31];
};

/* vfsquery flags */
#define VQ_NOTRESP	0x0001	/* server down */
#define VQ_NEEDAUTH	0x0002	/* server bad auth */
#define VQ_LOWDISK	0x0004	/* we're low on space */
#define VQ_MOUNT	0x0008	/* new filesystem arrived */
#define VQ_UNMOUNT	0x0010	/* filesystem has left */
#define VQ_DEAD		0x0020	/* filesystem is dead, needs force unmount */
#define VQ_ASSIST	0x0040	/* filesystem needs assistance from external
				   program */
#define VQ_NOTRESPLOCK	0x0080	/* server lockd down */
#define VQ_FLAG0100	0x0100	/* placeholder */
#define VQ_FLAG0200	0x0200	/* placeholder */
#define VQ_FLAG0400	0x0400	/* placeholder */
#define VQ_FLAG0800	0x0800	/* placeholder */
#define VQ_FLAG1000	0x1000	/* placeholder */
#define VQ_FLAG2000	0x2000	/* placeholder */
#define VQ_FLAG4000	0x4000	/* placeholder */
#define VQ_FLAG8000	0x8000	/* placeholder */

#ifdef _KERNEL
/* Point a sysctl request at a vfsidctl's data. */
#define VCTLTOREQ(vc, req)						\
	do {								\
		(req)->newptr = (vc)->vc_ptr;				\
		(req)->newlen = (vc)->vc_len;				\
		(req)->newidx = 0;					\
	} while (0)
#endif

struct iovec;
struct uio;

#ifdef _KERNEL

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_MOUNT);
#endif
extern int maxvfsconf;		/* highest defined filesystem type */
extern int nfs_mount_type;	/* vfc_typenum for nfs, or -1 */

TAILQ_HEAD(vfsconfhead, vfsconf);
extern struct vfsconfhead vfsconf;

/*
 * Operations supported on mounted filesystem.
 */
struct mount_args;
struct nameidata;
struct sysctl_req;
struct mntarg;

typedef int vfs_cmount_t(struct mntarg *ma, void *data, int flags, struct thread *td);
typedef int vfs_unmount_t(struct mount *mp, int mntflags, struct thread *td);
typedef int vfs_root_t(struct mount *mp, int flags, struct vnode **vpp,
		    struct thread *td);
typedef	int vfs_quotactl_t(struct mount *mp, int cmds, uid_t uid,
		    caddr_t arg, struct thread *td);
typedef	int vfs_statfs_t(struct mount *mp, struct statfs *sbp,
		    struct thread *td);
typedef	int vfs_sync_t(struct mount *mp, int waitfor, struct thread *td);
typedef	int vfs_vget_t(struct mount *mp, ino_t ino, int flags,
		    struct vnode **vpp);
typedef	int vfs_fhtovp_t(struct mount *mp, struct fid *fhp, struct vnode **vpp);
typedef	int vfs_checkexp_t(struct mount *mp, struct sockaddr *nam,
		    int *extflagsp, struct ucred **credanonp);
typedef	int vfs_vptofh_t(struct vnode *vp, struct fid *fhp);
typedef	int vfs_init_t(struct vfsconf *);
typedef	int vfs_uninit_t(struct vfsconf *);
typedef	int vfs_extattrctl_t(struct mount *mp, int cmd,
		    struct vnode *filename_vp, int attrnamespace,
		    const char *attrname, struct thread *td);
typedef	int vfs_mount_t(struct mount *mp, struct thread *td);
typedef int vfs_sysctl_t(struct mount *mp, fsctlop_t op,
		    struct sysctl_req *req);

struct vfsops {
	vfs_mount_t		*vfs_mount;
	vfs_cmount_t		*vfs_cmount;
	vfs_unmount_t		*vfs_unmount;
	vfs_root_t		*vfs_root;
	vfs_quotactl_t		*vfs_quotactl;
	vfs_statfs_t		*vfs_statfs;
	vfs_sync_t		*vfs_sync;
	vfs_vget_t		*vfs_vget;
	vfs_fhtovp_t		*vfs_fhtovp;
	vfs_checkexp_t		*vfs_checkexp;
	vfs_vptofh_t		*vfs_vptofh;
	vfs_init_t		*vfs_init;
	vfs_uninit_t		*vfs_uninit;
	vfs_extattrctl_t	*vfs_extattrctl;
	vfs_sysctl_t		*vfs_sysctl;
};

vfs_statfs_t	__vfs_statfs;

#define VFS_MOUNT(MP, P)    (*(MP)->mnt_op->vfs_mount)(MP, P)
#define VFS_UNMOUNT(MP, FORCE, P) (*(MP)->mnt_op->vfs_unmount)(MP, FORCE, P)
#define VFS_ROOT(MP, FLAGS, VPP, P) \
	(*(MP)->mnt_op->vfs_root)(MP, FLAGS, VPP, P)
#define VFS_QUOTACTL(MP,C,U,A,P)  (*(MP)->mnt_op->vfs_quotactl)(MP, C, U, A, P)
#define VFS_STATFS(MP, SBP, P)	  __vfs_statfs((MP), (SBP), (P))
#define VFS_SYNC(MP, WAIT, P)  (*(MP)->mnt_op->vfs_sync)(MP, WAIT, P)
#define VFS_VGET(MP, INO, FLAGS, VPP) \
	(*(MP)->mnt_op->vfs_vget)(MP, INO, FLAGS, VPP)
#define VFS_FHTOVP(MP, FIDP, VPP) \
	(*(MP)->mnt_op->vfs_fhtovp)(MP, FIDP, VPP)
#define	VFS_VPTOFH(VP, FIDP)	  (*(VP)->v_mount->mnt_op->vfs_vptofh)(VP, FIDP)
#define VFS_CHECKEXP(MP, NAM, EXFLG, CRED) \
	(*(MP)->mnt_op->vfs_checkexp)(MP, NAM, EXFLG, CRED)
#define VFS_EXTATTRCTL(MP, C, FN, NS, N, P) \
	(*(MP)->mnt_op->vfs_extattrctl)(MP, C, FN, NS, N, P)
#define VFS_SYSCTL(MP, OP, REQ) \
	(*(MP)->mnt_op->vfs_sysctl)(MP, OP, REQ)

extern int mpsafe_vfs;

#define	VFS_NEEDSGIANT(MP)						\
    (!mpsafe_vfs || ((MP) != NULL && ((MP)->mnt_kern_flag & MNTK_MPSAFE) == 0))

#define	VFS_LOCK_GIANT(MP) __extension__				\
({									\
	int _locked;							\
	if (VFS_NEEDSGIANT((MP))) {					\
		mtx_lock(&Giant);					\
		_locked = 1;						\
	} else								\
		_locked = 0;						\
	_locked;							\
})
#define	VFS_UNLOCK_GIANT(locked)	if ((locked)) mtx_unlock(&Giant);
#define	VFS_ASSERT_GIANT(MP) do 					\
{									\
	if (VFS_NEEDSGIANT((MP)))					\
		mtx_assert(&Giant, MA_OWNED);				\
} while (0)

#define VFS_KNOTE_LOCKED(vp, hint) do					\
{									\
	if (((vp)->v_vflag & VV_NOKNOTE) == 0)				\
		VN_KNOTE((vp), (hint), 1);				\
} while (0)

#define VFS_KNOTE_UNLOCKED(vp, hint) do					\
{									\
	if (((vp)->v_vflag & VV_NOKNOTE) == 0)				\
		VN_KNOTE((vp), (hint), 0);				\
} while (0)

#include <sys/module.h>

/*
 * Version numbers.
 */
#define VFS_VERSION_00	0x19660120
#define VFS_VERSION	VFS_VERSION_00

#define VFS_SET(vfsops, fsname, flags) \
	static struct vfsconf fsname ## _vfsconf = {		\
		.vfc_version = VFS_VERSION,			\
		.vfc_name = #fsname,				\
		.vfc_vfsops = &vfsops,				\
		.vfc_typenum = -1,				\
		.vfc_flags = flags,				\
	};							\
	static moduledata_t fsname ## _mod = {			\
		#fsname,					\
		vfs_modevent,					\
		& fsname ## _vfsconf				\
	};							\
	DECLARE_MODULE(fsname, fsname ## _mod, SI_SUB_VFS, SI_ORDER_MIDDLE)

extern	char *mountrootfsname;

/*
 * exported vnode operations
 */

int	dounmount(struct mount *, int, struct thread *);

int	kernel_mount(struct mntarg *ma, int flags);
int	kernel_vmount(int flags, ...);
struct mntarg *mount_arg(struct mntarg *ma, const char *name, const void *val, int len);
struct mntarg *mount_argb(struct mntarg *ma, int flag, const char *name);
struct mntarg *mount_argf(struct mntarg *ma, const char *name, const char *fmt, ...);
struct mntarg *mount_argsu(struct mntarg *ma, const char *name, const void *val, int len);
struct vfsconf *vfs_byname(const char *);
struct vfsconf *vfs_byname_kld(const char *, struct thread *td, int *);
void	vfs_event_signal(fsid_t *, u_int32_t, intptr_t);
int	vfs_flagopt(struct vfsoptlist *opts, const char *name, u_int *w, u_int val);
int	vfs_getopt(struct vfsoptlist *, const char *, void **, int *);
char	*vfs_getopts(struct vfsoptlist *, const char *, int *error);
int	vfs_copyopt(struct vfsoptlist *, const char *, void *, int);
int	vfs_filteropt(struct vfsoptlist *, const char **legal);
int	vfs_scanopt(struct vfsoptlist *opts, const char *name, const char *fmt, ...);
int	vfs_setpublicfs			    /* set publicly exported fs */
	    (struct mount *, struct netexport *, struct export_args *);
int	vfs_lock(struct mount *);         /* lock a vfs */
void	vfs_msync(struct mount *, int);
void	vfs_unlock(struct mount *);       /* unlock a vfs */
int	vfs_busy(struct mount *, int, struct mtx *, struct thread *);
int	vfs_export			 /* process mount export info */
	    (struct mount *, struct export_args *);
int	vfs_allocate_syncvnode(struct mount *);
void	vfs_getnewfsid(struct mount *);
struct cdev *vfs_getrootfsid(struct mount *);
struct	mount *vfs_getvfs(fsid_t *);      /* return vfs given fsid */
int	vfs_modevent(module_t, int, void *);
void	vfs_mount_error(struct mount *, const char *, ...);
void	vfs_mountroot(void);			/* mount our root filesystem */
void	vfs_mountedfrom(struct mount *, const char *from);
int	vfs_suser(struct mount *, struct thread *);
void	vfs_unbusy(struct mount *, struct thread *);
void	vfs_unmountall(void);
extern	TAILQ_HEAD(mntlist, mount) mountlist;	/* mounted filesystem list */
extern	struct mtx mountlist_mtx;
extern	struct nfs_public nfs_pub;

/*
 * Declarations for these vfs default operations are located in
 * kern/vfs_default.c, they should be used instead of making "dummy"
 * functions or casting entries in the VFS op table to "enopnotsupp()".
 */
vfs_root_t		vfs_stdroot;
vfs_quotactl_t		vfs_stdquotactl;
vfs_statfs_t		vfs_stdstatfs;
vfs_sync_t		vfs_stdsync;
vfs_sync_t		vfs_stdnosync;
vfs_vget_t		vfs_stdvget;
vfs_fhtovp_t		vfs_stdfhtovp;
vfs_checkexp_t		vfs_stdcheckexp;
vfs_vptofh_t		vfs_stdvptofh;
vfs_init_t		vfs_stdinit;
vfs_uninit_t		vfs_stduninit;
vfs_extattrctl_t	vfs_stdextattrctl;
vfs_sysctl_t		vfs_stdsysctl;

/* XXX - these should be indirect functions!!! */
int	softdep_fsync(struct vnode *);
int	softdep_process_worklist(struct mount *);

#else /* !_KERNEL */

#include <sys/cdefs.h>

struct stat;

__BEGIN_DECLS
int	fhopen(const struct fhandle *, int);
int	fhstat(const struct fhandle *, struct stat *);
int	fhstatfs(const struct fhandle *, struct statfs *);
int	fstatfs(int, struct statfs *);
int	getfh(const char *, fhandle_t *);
int	getfsstat(struct statfs *, long, int);
int	getmntinfo(struct statfs **, int);
int	lgetfh(const char *, fhandle_t *);
int	mount(const char *, const char *, int, void *);
int	nmount(struct iovec *, unsigned int, int);
int	statfs(const char *, struct statfs *);
int	unmount(const char *, int);

/* C library stuff */
int	getvfsbyname(const char *, struct xvfsconf *);
__END_DECLS

#endif /* _KERNEL */

#endif /* !_SYS_MOUNT_H_ */
