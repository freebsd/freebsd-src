/*
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
#include <sys/_label.h>
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

#define	MFSNAMELEN	16	/* length of fs type name, including null */
#define	MNAMELEN	(88 - 2 * sizeof(long))	/* size of on/from name bufs */

/* XXX getfsstat.2 is out of date with write and read counter changes here. */
/* XXX statfs.2 is out of date with read counter changes here. */
struct statfs {
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
	char	f_fstypename[MFSNAMELEN]; /* fs type name */
	char	f_mntonname[MNAMELEN];	/* directory on which mounted */
	long	f_syncreads;		/* count of sync reads since mount */
	long	f_asyncreads;		/* count of async reads since mount */
	short	f_spares1;		/* unused spare */
	char	f_mntfromname[MNAMELEN];/* mounted filesystem */
	short	f_spares2;		/* unused spare */
	/*
	 * XXX on machines where longs are aligned to 8-byte boundaries, there
	 * is an unnamed int32_t here.  This spare was after the apparent end
	 * of the struct until we bit off the read counters from f_mntonname.
	 */
	long	f_spare[2];		/* unused spare */
};

#ifdef _KERNEL
#define	MMAXOPTIONLEN	65536		/* maximum length of a mount option */

TAILQ_HEAD(vnodelst, vnode);
TAILQ_HEAD(vfsoptlist, vfsopt);
struct vfsopt {
	TAILQ_ENTRY(vfsopt) link;
	char	*name;
	void	*value;
	int	len;
};

/*
 * Structure per mounted filesystem.  Each mounted filesystem has an
 * array of operations and an instance record.  The filesystems are
 * put on a doubly linked list.
 *
 * NOTE: mnt_nvnodelist and mnt_reservedvnlist.  At the moment vnodes
 * are linked into mnt_nvnodelist.  At some point in the near future the
 * vnode list will be split into a 'dirty' and 'clean' list. mnt_nvnodelist
 * will become the dirty list and mnt_reservedvnlist will become the 'clean'
 * list.  Filesystem kld's syncing code should remain compatible since
 * they only need to scan the dirty vnode list (nvnodelist -> dirtyvnodelist).
 */
struct mount {
	TAILQ_ENTRY(mount) mnt_list;		/* mount list */
	struct vfsops	*mnt_op;		/* operations on fs */
	struct vfsconf	*mnt_vfc;		/* configuration info */
	struct vnode	*mnt_vnodecovered;	/* vnode we mounted on */
	struct vnode	*mnt_syncer;		/* syncer vnode */
	struct vnodelst	mnt_nvnodelist;		/* list of vnodes this mount */
	struct vnodelst	mnt_reservedvnlist;	/* (future) dirty vnode list */
	struct lock	mnt_lock;		/* mount structure lock */
	int		mnt_writeopcount;	/* write syscalls in progress */
	int		mnt_flag;		/* flags shared with user */
	struct vfsoptlist *mnt_opt;		/* current mount options */
	struct vfsoptlist *mnt_optnew;		/* new options passed to fs */
	int		mnt_kern_flag;		/* kernel only flags */
	int		mnt_maxsymlinklen;	/* max size of short symlink */
	struct statfs	mnt_stat;		/* cache of filesystem stats */
	struct ucred	*mnt_cred;		/* credentials of mounter */
	qaddr_t		mnt_data;		/* private data */
	time_t		mnt_time;		/* last time written*/
	int		mnt_iosize_max;		/* max size for clusters, etc */
	struct netexport *mnt_export;		/* export list */
	struct label	mnt_mntlabel;		/* MAC label for the mount */
	struct label	mnt_fslabel;		/* MAC label for the fs */
};
#endif /* _KERNEL */

/*
 * User specifiable flags.
 */
#define	MNT_RDONLY	0x00000001	/* read only filesystem */
#define	MNT_SYNCHRONOUS	0x00000002	/* filesystem written synchronously */
#define	MNT_NOEXEC	0x00000004	/* can't exec from filesystem */
#define	MNT_NOSUID	0x00000008	/* don't honor setuid bits on fs */
#define	MNT_NODEV	0x00000010	/* don't interpret special files */
#define	MNT_UNION	0x00000020	/* union with underlying filesystem */
#define	MNT_ASYNC	0x00000040	/* filesystem written asynchronously */
#define	MNT_SUIDDIR	0x00100000	/* special handling of SUID on dirs */
#define	MNT_SOFTDEP	0x00200000	/* soft updates being done */
#define	MNT_NOSYMFOLLOW	0x00400000	/* do not follow symlinks */
#define	MNT_JAILDEVFS	0x02000000	/* jail-friendly DEVFS behaviour */
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
			MNT_NOSUID	| MNT_NODEV	| MNT_UNION	| \
			MNT_ASYNC	| MNT_EXRDONLY	| MNT_EXPORTED	| \
			MNT_DEFEXPORTED	| MNT_EXPORTANON| MNT_EXKERB	| \
			MNT_LOCAL	| MNT_USER	| MNT_QUOTA	| \
			MNT_ROOTFS	| MNT_NOATIME	| MNT_NOCLUSTERR| \
			MNT_NOCLUSTERW	| MNT_SUIDDIR	| MNT_SOFTDEP	| \
			MNT_IGNORE	| MNT_EXPUBLIC	| MNT_NOSYMFOLLOW | \
			MNT_JAILDEVFS	| MNT_MULTILABEL | MNT_ACLS)

/* Mask of flags that can be updated. */
#define	MNT_UPDATEMASK (MNT_NOSUID	| MNT_NOEXEC	| MNT_NODEV	| \
			MNT_SYNCHRONOUS	| MNT_UNION	| MNT_ASYNC	| \
			MNT_NOATIME | \
			MNT_NOSYMFOLLOW	| MNT_IGNORE	| MNT_JAILDEVFS	| \
			MNT_NOCLUSTERR	| MNT_NOCLUSTERW | MNT_SUIDDIR)
  
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
#define MNT_CMDFLAGS   (MNT_UPDATE	| MNT_DELEXPORT	| MNT_RELOAD	| \
			MNT_FORCE	| MNT_SNAPSHOT)
/*
 * Still available
 */
#define	MNT_SPARE3	0x08000000
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
#define MNTK_WANTRDWR	0x04000000	/* upgrade to read/write requested */
#define	MNTK_SUSPEND	0x08000000	/* request write suspension */
#define	MNTK_SUSPENDED	0x10000000	/* write operations are suspended */

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
 */
struct vfsconf {
	struct	vfsops *vfc_vfsops;	/* filesystem operations vector */
	char	vfc_name[MFSNAMELEN];	/* filesystem type name */
	int	vfc_typenum;		/* historic filesystem type number */
	int	vfc_refcount;		/* number mounted of this type */
	int	vfc_flags;		/* permanent flags */
	struct	vfsoptdecl *vfc_opts;	/* mount options */
	struct	vfsconf *vfc_next;	/* next in list */
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

struct ovfsconf {
	void	*vfc_vfsops;
	char	vfc_name[32];
	int	vfc_index;
	int	vfc_refcount;
	int	vfc_flags;
};

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

struct iovec;
struct uio;

#ifdef _KERNEL

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_MOUNT);
#endif
extern int maxvfsconf;		/* highest defined filesystem type */
extern int nfs_mount_type;	/* vfc_typenum for nfs, or -1 */
extern struct vfsconf *vfsconf;	/* head of list of filesystem types */

/*
 * Operations supported on mounted filesystem.
 */
struct mount_args;
struct nameidata;

typedef int vfs_mount_t(struct mount *mp, char *path, caddr_t data,
			struct nameidata *ndp, struct thread *td);
typedef int vfs_start_t(struct mount *mp, int flags, struct thread *td);
typedef int vfs_unmount_t(struct mount *mp, int mntflags, struct thread *td);
typedef int vfs_root_t(struct mount *mp, struct vnode **vpp);
typedef	int vfs_quotactl_t(struct mount *mp, int cmds, uid_t uid,
		    caddr_t arg, struct thread *td);
typedef	int vfs_statfs_t(struct mount *mp, struct statfs *sbp,
		    struct thread *td);
typedef	int vfs_sync_t(struct mount *mp, int waitfor, struct ucred *cred,
		    struct thread *td);
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
typedef	int vfs_nmount_t(struct mount *mp, struct nameidata *ndp,
		    struct thread *td);

struct vfsops {
	vfs_mount_t		*vfs_mount;
	vfs_start_t		*vfs_start;
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
	/* Additions below are not binary compatible with 5.0 and below. */
	vfs_nmount_t		*vfs_nmount;
};

#define VFS_NMOUNT(MP, NDP, P)    (*(MP)->mnt_op->vfs_nmount)(MP, NDP, P)
#define VFS_MOUNT(MP, PATH, DATA, NDP, P) \
	(*(MP)->mnt_op->vfs_mount)(MP, PATH, DATA, NDP, P)
#define VFS_START(MP, FLAGS, P)	  (*(MP)->mnt_op->vfs_start)(MP, FLAGS, P)
#define VFS_UNMOUNT(MP, FORCE, P) (*(MP)->mnt_op->vfs_unmount)(MP, FORCE, P)
#define VFS_ROOT(MP, VPP)	  (*(MP)->mnt_op->vfs_root)(MP, VPP)
#define VFS_QUOTACTL(MP,C,U,A,P)  (*(MP)->mnt_op->vfs_quotactl)(MP, C, U, A, P)
#define VFS_STATFS(MP, SBP, P)	  (*(MP)->mnt_op->vfs_statfs)(MP, SBP, P)
#define VFS_SYNC(MP, WAIT, C, P)  (*(MP)->mnt_op->vfs_sync)(MP, WAIT, C, P)
#define VFS_VGET(MP, INO, FLAGS, VPP) \
	(*(MP)->mnt_op->vfs_vget)(MP, INO, FLAGS, VPP)
#define VFS_FHTOVP(MP, FIDP, VPP) \
	(*(MP)->mnt_op->vfs_fhtovp)(MP, FIDP, VPP)
#define	VFS_VPTOFH(VP, FIDP)	  (*(VP)->v_mount->mnt_op->vfs_vptofh)(VP, FIDP)
#define VFS_CHECKEXP(MP, NAM, EXFLG, CRED) \
	(*(MP)->mnt_op->vfs_checkexp)(MP, NAM, EXFLG, CRED)
#define VFS_EXTATTRCTL(MP, C, FN, NS, N, P) \
	(*(MP)->mnt_op->vfs_extattrctl)(MP, C, FN, NS, N, P)

#include <sys/module.h>

#define VFS_SET(vfsops, fsname, flags) \
	static struct vfsconf fsname ## _vfsconf = {		\
		&vfsops,					\
		#fsname,					\
		-1,						\
		0,						\
		flags						\
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
int	dounmount(struct mount *, int, struct thread *td);
int	kernel_mount(struct iovec *iovp, unsigned int iovcnt, int flags);
int	kernel_vmount(int flags, ...);
int	vfs_getopt(struct vfsoptlist *, const char *, void **, int *);
int	vfs_copyopt(struct vfsoptlist *, const char *, void *, int);
int	vfs_mount(struct thread *td, const char *type, char *path,
	    int flags, void *data);
int	vfs_setpublicfs			    /* set publicly exported fs */
	    (struct mount *, struct netexport *, struct export_args *);
int	vfs_lock(struct mount *);         /* lock a vfs */
void	vfs_msync(struct mount *, int);
void	vfs_unlock(struct mount *);       /* unlock a vfs */
int	vfs_busy(struct mount *, int, struct mtx *, struct thread *td);
int	vfs_export			 /* process mount export info */
	    (struct mount *, struct export_args *);
struct	netcred *vfs_export_lookup	    /* lookup host in fs export list */
	    (struct mount *, struct sockaddr *);
int	vfs_allocate_syncvnode(struct mount *);
void	vfs_getnewfsid(struct mount *);
dev_t	vfs_getrootfsid(struct mount *);
struct	mount *vfs_getvfs(fsid_t *);      /* return vfs given fsid */
int	vfs_modevent(module_t, int, void *);
int	vfs_mountedon(struct vnode *);    /* is a vfs mounted on vp */
void	vfs_mountroot(void);			/* mount our root filesystem */
int	vfs_rootmountalloc(char *, char *, struct mount **);
void	vfs_unbusy(struct mount *, struct thread *td);
void	vfs_unmountall(void);
int	vfs_register(struct vfsconf *);
int	vfs_unregister(struct vfsconf *);
extern	TAILQ_HEAD(mntlist, mount) mountlist;	/* mounted filesystem list */
extern	struct mtx mountlist_mtx;
extern	struct nfs_public nfs_pub;

/* 
 * Declarations for these vfs default operations are located in 
 * kern/vfs_default.c, they should be used instead of making "dummy" 
 * functions or casting entries in the VFS op table to "enopnotsupp()".
 */ 
vfs_start_t		vfs_stdstart;
vfs_root_t		vfs_stdroot;
vfs_quotactl_t		vfs_stdquotactl;
vfs_statfs_t		vfs_stdstatfs;
vfs_sync_t		vfs_stdsync;
vfs_vget_t		vfs_stdvget;
vfs_fhtovp_t		vfs_stdfhtovp;
vfs_checkexp_t		vfs_stdcheckexp;
vfs_vptofh_t		vfs_stdvptofh;
vfs_init_t		vfs_stdinit;
vfs_uninit_t		vfs_stduninit;
vfs_extattrctl_t	vfs_stdextattrctl;

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
int	mount(const char *, const char *, int, void *);
int	nmount(struct iovec *, u_int, int);
int	statfs(const char *, struct statfs *);
int	unmount(const char *, int);

/* C library stuff */
void	endvfsent(void);
struct	ovfsconf *getvfsbytype(int);
struct	ovfsconf *getvfsent(void);
int	getvfsbyname(const char *, struct xvfsconf *);
void	setvfsent(int);
int	vfsisloadable(const char *);
int	vfsload(const char *);
__END_DECLS

#endif /* _KERNEL */

#endif /* !_SYS_MOUNT_H_ */
