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
 *	$Id: mount.h,v 1.59 1998/03/28 10:33:22 bde Exp $
 */

#ifndef _SYS_MOUNT_H_
#define _SYS_MOUNT_H_

#include <sys/ucred.h>
#include <sys/queue.h>
#include <sys/lock.h>

typedef struct fsid { int32_t val[2]; } fsid_t;	/* file system id type */

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
 * file system statistics
 */

#define MFSNAMELEN	16	/* length of fs type name, including null */
#define	MNAMELEN	90	/* length of buffer for returned name */

struct statfs {
	long	f_spare2;		/* placeholder */
	long	f_bsize;		/* fundamental file system block size */
	long	f_iosize;		/* optimal transfer block size */
	long	f_blocks;		/* total data blocks in file system */
	long	f_bfree;		/* free blocks in fs */
	long	f_bavail;		/* free blocks avail to non-superuser */
	long	f_files;		/* total file nodes in file system */
	long	f_ffree;		/* free file nodes in fs */
	fsid_t	f_fsid;			/* file system id */
	uid_t	f_owner;		/* user that mounted the filesystem */
	int	f_type;			/* type of filesystem (see below) */
	int	f_flags;		/* copy of mount exported flags */
	long    f_syncwrites;		/* count of sync writes since mount */
	long    f_asyncwrites;		/* count of async writes since mount */
	char	f_fstypename[MFSNAMELEN]; /* fs type name */
	char	f_mntonname[MNAMELEN];	/* directory on which mounted */
	char	f_mntfromname[MNAMELEN];/* mounted filesystem */
};

/*
 * File system types (for backwards compat with 4.4Lite.)
 */
#define	MOUNT_NONE	0
#define	MOUNT_UFS	1	/* Fast Filesystem */
#define	MOUNT_NFS	2	/* Sun-compatible Network Filesystem */
#define	MOUNT_MFS	3	/* Memory-based Filesystem */
#define	MOUNT_MSDOS	4	/* MS/DOS Filesystem */
#define	MOUNT_LFS	5	/* Log-based Filesystem */
#define	MOUNT_LOFS	6	/* Loopback Filesystem */
#define	MOUNT_FDESC	7	/* File Descriptor Filesystem */
#define	MOUNT_PORTAL	8	/* Portal Filesystem */
#define MOUNT_NULL	9	/* Minimal Filesystem Layer */
#define MOUNT_UMAP	10	/* User/Group Identifier Remapping Filesystem */
#define MOUNT_KERNFS	11	/* Kernel Information Filesystem */
#define MOUNT_PROCFS	12	/* /proc Filesystem */
#define MOUNT_AFS	13	/* Andrew Filesystem */
#define MOUNT_CD9660	14	/* ISO9660 (aka CDROM) Filesystem */
#define MOUNT_UNION	15	/* Union (translucent) Filesystem */
#define MOUNT_DEVFS	16	/* existing device Filesystem */
#define	MOUNT_EXT2FS	17	/* Linux EXT2FS */
#define MOUNT_TFS	18	/* Netcon Novell filesystem */
#define	MOUNT_CFS	19	/* Coda filesystem */
#define	MOUNT_MAXTYPE	19

#define INITMOUNTNAMES { \
	"none",		/*  0 MOUNT_NONE */ \
	"ufs",		/*  1 MOUNT_UFS */ \
	"nfs",		/*  2 MOUNT_NFS */ \
	"mfs",		/*  3 MOUNT_MFS */ \
	"msdos",	/*  4 MOUNT_MSDOS */ \
	"lfs",		/*  5 MOUNT_LFS */ \
	"lofs",		/*  6 MOUNT_LOFS */ \
	"fdesc",	/*  7 MOUNT_FDESC */ \
	"portal",	/*  8 MOUNT_PORTAL */ \
	"null",		/*  9 MOUNT_NULL */ \
	"umap",		/* 10 MOUNT_UMAP */ \
	"kernfs",	/* 11 MOUNT_KERNFS */ \
	"procfs",	/* 12 MOUNT_PROCFS */ \
	"afs",		/* 13 MOUNT_AFS */ \
	"cd9660",	/* 14 MOUNT_CD9660 */ \
	"union",	/* 15 MOUNT_UNION */ \
	"devfs",	/* 16 MOUNT_DEVFS */ \
	"ext2fs",	/* 17 MOUNT_EXT2FS */ \
	"tfs",		/* 18 MOUNT_TFS */ \
	"cfs",		/* 19 MOUNT_CFS */ \
	0,		/* 20 MOUNT_SPARE */ \
}

/*
 * Structure per mounted file system.  Each mounted file system has an
 * array of operations and an instance record.  The file systems are
 * put on a doubly linked list.
 */
LIST_HEAD(vnodelst, vnode);

struct mount {
	CIRCLEQ_ENTRY(mount) mnt_list;		/* mount list */
	struct vfsops	*mnt_op;		/* operations on fs */
	struct vfsconf	*mnt_vfc;		/* configuration info */
	struct vnode	*mnt_vnodecovered;	/* vnode we mounted on */
	struct vnode	*mnt_syncer;		/* syncer vnode */
	struct vnodelst	mnt_vnodelist;		/* list of vnodes this mount */
	struct lock	mnt_lock;		/* mount structure lock */
	int		mnt_flag;		/* flags shared with user */
	int		mnt_kern_flag;		/* kernel only flags */
	int		mnt_maxsymlinklen;	/* max size of short symlink */
	struct statfs	mnt_stat;		/* cache of filesystem stats */
	qaddr_t		mnt_data;		/* private data */
	time_t		mnt_time;		/* last time written*/
};

/*
 * User specifiable flags.
 */
#define	MNT_RDONLY	0x00000001	/* read only filesystem */
#define	MNT_SYNCHRONOUS	0x00000002	/* file system written synchronously */
#define	MNT_NOEXEC	0x00000004	/* can't exec from filesystem */
#define	MNT_NOSUID	0x00000008	/* don't honor setuid bits on fs */
#define	MNT_NODEV	0x00000010	/* don't interpret special files */
#define	MNT_UNION	0x00000020	/* union with underlying filesystem */
#define	MNT_ASYNC	0x00000040	/* file system written asynchronously */
#define	MNT_SUIDDIR	0x00100000	/* special handling of SUID on dirs */
#define	MNT_SOFTDEP	0x00200000	/* soft updates being done */
#define	MNT_NOATIME	0x10000000	/* disable update of file access time */
#define	MNT_NOCLUSTERR	0x40000000	/* disable cluster read */
#define	MNT_NOCLUSTERW	0x80000000	/* disable cluster write */

/*
 * NFS export related mount flags.
 */
#define	MNT_EXRDONLY	0x00000080	/* exported read only */
#define	MNT_EXPORTED	0x00000100	/* file system is exported */
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

/*
 * Mask of flags that are visible to statfs()
 * XXX I think that this could now become (~(MNT_CMDFLAGS))
 * but the 'mount' program may need changing to handle this.
 * XXX MNT_EXPUBLIC is presently left out. I don't know why.
 */
#define	MNT_VISFLAGMASK	(MNT_RDONLY	| MNT_SYNCHRONOUS | MNT_NOEXEC	| \
			MNT_NOSUID	| MNT_NODEV	| MNT_UNION	| \
			MNT_ASYNC	| MNT_EXRDONLY	| MNT_EXPORTED	| \
			MNT_DEFEXPORTED	| MNT_EXPORTANON| MNT_EXKERB	| \
			MNT_LOCAL	| MNT_USER	| MNT_QUOTA	| \
			MNT_ROOTFS	| MNT_NOATIME	| MNT_NOCLUSTERR| \
			MNT_NOCLUSTERW	| MNT_SUIDDIR	| MNT_SOFTDEP	 \
			/*	| MNT_EXPUBLIC */)
/*
 * External filesystem command modifier flags.
 * Unmount can use the MNT_FORCE flag.
 * XXX These are not STATES and really should be somewhere else.
 */
#define	MNT_UPDATE	0x00010000	/* not a real mount, just an update */
#define	MNT_DELEXPORT	0x00020000	/* delete export host lists */
#define	MNT_RELOAD	0x00040000	/* reload filesystem data */
#define	MNT_FORCE	0x00080000	/* force unmount or readonly change */
#define MNT_CMDFLAGS	(MNT_UPDATE|MNT_DELEXPORT|MNT_RELOAD|MNT_FORCE)
/*
 * Internal filesystem control flags stored in mnt_kern_flag.
 *
 * MNTK_UNMOUNT locks the mount entry so that name lookup cannot proceed
 * past the mount point.  This keeps the subtree stable during mounts
 * and unmounts.
 */
#define MNTK_UNMOUNT	0x01000000	/* unmount in progress */
#define	MNTK_MWAIT	0x02000000	/* waiting for unmount to finish */
#define MNTK_WANTRDWR	0x04000000	/* upgrade to read/write requested */

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
	fsid_t	fh_fsid;	/* File system id of mount point */
	struct	fid fh_fid;	/* File sys specific id */
};
typedef struct fhandle	fhandle_t;

/*
 * Export arguments for local filesystem mount calls.
 */
struct export_args {
	int	ex_flags;		/* export related flags */
	uid_t	ex_root;		/* mapping for root uid */
	struct	ucred ex_anon;		/* mapping for anonymous user */
	struct	sockaddr *ex_addr;	/* net address to which exported */
	int	ex_addrlen;		/* and the net address length */
	struct	sockaddr *ex_mask;	/* mask of valid bits in saddr */
	int	ex_masklen;		/* and the smask length */
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

#ifdef KERNEL

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_MOUNT);
#endif
extern int maxvfsconf;		/* highest defined filesystem type */
extern struct vfsconf *vfsconf;	/* head of list of filesystem types */

/*
 * Operations supported on mounted file system.
 */
#ifdef __STDC__
struct nameidata;
struct mbuf;
#endif

struct vfsops {
	int	(*vfs_mount)	__P((struct mount *mp, char *path, caddr_t data,
				    struct nameidata *ndp, struct proc *p));
	int	(*vfs_start)	__P((struct mount *mp, int flags,
				    struct proc *p));
	int	(*vfs_unmount)	__P((struct mount *mp, int mntflags,
				    struct proc *p));
	int	(*vfs_root)	__P((struct mount *mp, struct vnode **vpp));
	int	(*vfs_quotactl)	__P((struct mount *mp, int cmds, uid_t uid,
				    caddr_t arg, struct proc *p));
	int	(*vfs_statfs)	__P((struct mount *mp, struct statfs *sbp,
				    struct proc *p));
	int	(*vfs_sync)	__P((struct mount *mp, int waitfor,
				    struct ucred *cred, struct proc *p));
	int	(*vfs_vget)	__P((struct mount *mp, ino_t ino,
				    struct vnode **vpp));
	int	(*vfs_vrele)	__P((struct mount *mp, struct vnode *vp));
	int	(*vfs_fhtovp)	__P((struct mount *mp, struct fid *fhp,
				    struct sockaddr *nam, struct vnode **vpp,
				    int *exflagsp, struct ucred **credanonp));
	int	(*vfs_vptofh)	__P((struct vnode *vp, struct fid *fhp));
	int	(*vfs_init)	__P((struct vfsconf *));
};

#define VFS_MOUNT(MP, PATH, DATA, NDP, P) \
	(*(MP)->mnt_op->vfs_mount)(MP, PATH, DATA, NDP, P)
#define VFS_START(MP, FLAGS, P)	  (*(MP)->mnt_op->vfs_start)(MP, FLAGS, P)
#define VFS_UNMOUNT(MP, FORCE, P) (*(MP)->mnt_op->vfs_unmount)(MP, FORCE, P)
#define VFS_ROOT(MP, VPP)	  (*(MP)->mnt_op->vfs_root)(MP, VPP)
#define VFS_QUOTACTL(MP,C,U,A,P)  (*(MP)->mnt_op->vfs_quotactl)(MP, C, U, A, P)
#define VFS_STATFS(MP, SBP, P)	  (*(MP)->mnt_op->vfs_statfs)(MP, SBP, P)
#define VFS_SYNC(MP, WAIT, C, P)  (*(MP)->mnt_op->vfs_sync)(MP, WAIT, C, P)
#define VFS_VGET(MP, INO, VPP)	  (*(MP)->mnt_op->vfs_vget)(MP, INO, VPP)
#define VFS_VRELE(MP, VP)	  (*(MP)->mnt_op->vfs_vrele)(MP, VP)
#define VFS_FHTOVP(MP, FIDP, NAM, VPP, EXFLG, CRED) \
	(*(MP)->mnt_op->vfs_fhtovp)(MP, FIDP, NAM, VPP, EXFLG, CRED)
#define	VFS_VPTOFH(VP, FIDP)	  (*(VP)->v_mount->mnt_op->vfs_vptofh)(VP, FIDP)

#ifdef VFS_LKM
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>

#define VFS_SET(vfsops, fsname, index, flags) \
	static struct vfsconf _fs_vfsconf = { \
		&vfsops, \
		#fsname, \
		index, \
		0, \
		flags, \
	}; \
	extern struct linker_set MODVNOPS; \
	MOD_VFS(fsname,&MODVNOPS,&_fs_vfsconf); \
	extern int \
	fsname ## _mod __P((struct lkm_table *, int, int)); \
	int \
	fsname ## _mod(struct lkm_table *lkmtp, int cmd, int ver) { \
		MOD_DISPATCH(fsname, \
		lkmtp, cmd, ver, lkm_nullcmd, lkm_nullcmd, lkm_nullcmd); }
#else

#define VFS_SET(vfsops, fsname, index, flags) \
	static struct vfsconf _fs_vfsconf = { \
		&vfsops, \
		#fsname, \
		index, \
		0, \
		flags | VFCF_STATIC, \
	}; \
	DATA_SET(vfs_set,_fs_vfsconf)
#endif /* VFS_LKM */

#endif /* KERNEL */

#ifdef KERNEL
#include <net/radix.h>

#define	AF_MAX		30	/* XXX */

/*
 * Network address lookup element
 */
struct netcred {
	struct	radix_node netc_rnodes[2];
	int	netc_exflags;
	struct	ucred netc_anon;
};

/*
 * Network export information
 */
struct netexport {
	struct	netcred ne_defexported;		      /* Default export */
	struct	radix_node_head *ne_rtable[AF_MAX+1]; /* Individual exports */
};

extern	char *mountrootfsname;

/*
 * exported vnode operations
 */
int	dounmount __P((struct mount *, int, struct proc *));
int	vfs_setpublicfs			    /* set publicly exported fs */
	  __P((struct mount *, struct netexport *, struct export_args *));
int	vfs_lock __P((struct mount *));         /* lock a vfs */
void	vfs_msync __P((struct mount *, int));
void	vfs_unlock __P((struct mount *));       /* unlock a vfs */
int	vfs_busy __P((struct mount *, int, struct simplelock *, struct proc *));
int	vfs_export			    /* process mount export info */
	  __P((struct mount *, struct netexport *, struct export_args *));
int	vfs_vrele __P((struct mount *, struct vnode *));
struct	netcred *vfs_export_lookup	    /* lookup host in fs export list */
	  __P((struct mount *, struct netexport *, struct sockaddr *));
int	vfs_allocate_syncvnode __P((struct mount *));
void	vfs_getnewfsid __P((struct mount *));
struct	mount *vfs_getvfs __P((fsid_t *));      /* return vfs given fsid */
int	vfs_mountedon __P((struct vnode *));    /* is a vfs mounted on vp */
int	vfs_rootmountalloc __P((char *, char *, struct mount **));
void	vfs_unbusy __P((struct mount *, struct proc *));
void	vfs_unmountall __P((void));
extern	CIRCLEQ_HEAD(mntlist, mount) mountlist;	/* mounted filesystem list */
extern	struct simplelock mountlist_slock;
extern	struct nfs_public nfs_pub;

#else /* !KERNEL */

#include <sys/cdefs.h>

__BEGIN_DECLS
int	fstatfs __P((int, struct statfs *));
int	getfh __P((const char *, fhandle_t *));
int	getfsstat __P((struct statfs *, long, int));
int	getmntinfo __P((struct statfs **, int));
int	mount __P((const char *, const char *, int, void *));
int	statfs __P((const char *, struct statfs *));
int	unmount __P((const char *, int));

/* C library stuff */
void	endvfsent __P((void));
struct	ovfsconf *getvfsbyname __P((const char *));
struct	ovfsconf *getvfsbytype __P((int));
struct	ovfsconf *getvfsent __P((void));
#define	getvfsbyname	new_getvfsbyname
int	new_getvfsbyname __P((const char *, struct vfsconf *));
void	setvfsent __P((int));
int	vfsisloadable __P((const char *));
int	vfsload __P((const char *));
__END_DECLS

#endif /* KERNEL */

#endif /* !_SYS_MOUNT_H_ */
