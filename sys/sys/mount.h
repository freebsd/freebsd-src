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
 *	@(#)mount.h	8.13 (Berkeley) 3/27/94
 *	$Id: mount.h,v 1.27 1995/09/09 18:10:30 davidg Exp $
 */

#ifndef _SYS_MOUNT_H_
#define _SYS_MOUNT_H_

#ifndef KERNEL
#include <sys/ucred.h>
#endif
#include <sys/queue.h>

typedef struct { long val[2]; } fsid_t;		/* file system id type */

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

#define MNAMELEN 90	/* length of buffer for returned name */

struct statfs {
	short	f_type;			/* type of filesystem (see below) */
	short	f_flags;		/* copy of mount flags */
	long	f_bsize;		/* fundamental file system block size */
	long	f_iosize;		/* optimal transfer block size */
	long	f_blocks;		/* total data blocks in file system */
	long	f_bfree;		/* free blocks in fs */
	long	f_bavail;		/* free blocks avail to non-superuser */
	long	f_files;		/* total file nodes in file system */
	long	f_ffree;		/* free file nodes in fs */
	fsid_t	f_fsid;			/* file system id */
	long	f_spare[9];		/* spare for later */
	char	f_mntonname[MNAMELEN];	/* directory on which mounted */
	char	f_mntfromname[MNAMELEN];/* mounted filesystem */
};

/*
 * File system types.
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
#define MOUNT_UMAP	10	/* User/Group Identifer Remapping Filesystem */
#define MOUNT_KERNFS	11	/* Kernel Information Filesystem */
#define MOUNT_PROCFS	12	/* /proc Filesystem */
#define MOUNT_AFS	13	/* Andrew Filesystem */
#define MOUNT_CD9660	14	/* ISO9660 (aka CDROM) Filesystem */
#define MOUNT_UNION	15	/* Union (translucent) Filesystem */
#define MOUNT_DEVFS	16	/* existing device Filesystem */
#define	MOUNT_EXT2FS	17	/* Linux EXT2FS */
#define	MOUNT_MAXTYPE	17

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
	0,		/* 17 MOUNT_SPARE */ \
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
	struct vnodelst	mnt_vnodelist;		/* list of vnodes this mount */
	int		mnt_flag;		/* flags */
	int		mnt_maxsymlinklen;	/* max size of short symlink */
	struct statfs	mnt_stat;		/* cache of filesystem stats */
	qaddr_t		mnt_data;		/* private data */
/*	struct vfsconf	*mnt_vfc; */		/* configuration info */
	time_t		mnt_time;		/* last time written*/
};

/*
 * Mount flags.
 *
 * Unmount uses MNT_FORCE flag.
 */
#define	MNT_RDONLY	0x00000001	/* read only filesystem */
#define	MNT_SYNCHRONOUS	0x00000002	/* file system written synchronously */
#define	MNT_NOEXEC	0x00000004	/* can't exec from filesystem */
#define	MNT_NOSUID	0x00000008	/* don't honor setuid bits on fs */
#define	MNT_NODEV	0x00000010	/* don't interpret special files */
#define	MNT_UNION	0x00000020	/* union with underlying filesystem */
#define	MNT_ASYNC	0x00000040	/* file system written asynchronously */

/*
 * exported mount flags.
 */
#define	MNT_EXRDONLY	0x00000080	/* exported read only */
#define	MNT_EXPORTED	0x00000100	/* file system is exported */
#define	MNT_DEFEXPORTED	0x00000200	/* exported to the world */
#define	MNT_EXPORTANON	0x00000400	/* use anon uid mapping for everyone */
#define	MNT_EXKERB	0x00000800	/* exported with Kerberos uid mapping */

/*
 * Flags set by internal operations.
 */
#define	MNT_LOCAL	0x00001000	/* filesystem is stored locally */
#define	MNT_QUOTA	0x00002000	/* quotas are enabled on filesystem */
#define	MNT_ROOTFS	0x00004000	/* identifies the root filesystem */
#define	MNT_USER	0x00008000	/* mounted by a user */

/*
 * Mask of flags that are visible to statfs()
 */
#define	MNT_VISFLAGMASK	0x0000ffff

/*
 * filesystem control flags.
 *
 * MNT_MLOCK lock the mount entry so that name lookup cannot proceed
 * past the mount point.  This keeps the subtree stable during mounts
 * and unmounts.
 */
#define	MNT_UPDATE	0x00010000	/* not a real mount, just an update */
#define	MNT_DELEXPORT	0x00020000	/* delete export host lists */
#define	MNT_RELOAD	0x00040000	/* reload filesystem data */
#define	MNT_FORCE	0x00080000	/* force unmount or readonly change */
#define	MNT_MLOCK	0x00100000	/* lock so that subtree is stable */
#define	MNT_MWAIT	0x00200000	/* someone is waiting for lock */
#define MNT_MPBUSY	0x00400000	/* scan of mount point in progress */
#define MNT_MPWANT	0x00800000	/* waiting for mount point */
#define MNT_UNMOUNT	0x01000000	/* unmount in progress */
#define MNT_WANTRDWR	0x02000000	/* want upgrade to read/write */

/*
 * used to get configured filesystems information
 */
#define VFS_MAXNAMELEN 32
struct vfsconf {
	void *vfc_vfsops;
	char vfc_name[VFS_MAXNAMELEN];
	int vfc_index;
	int vfc_refcount;
	int vfc_flags;
};

/*
 * NB: these flags refer to IMPLEMENTATION properties, not properties of
 * any actual mounts; i.e., it does not make sense to change the flags.
 */
#define	VFCF_STATIC	0x00000001	/* statically compiled into kernel */
#define	VFCF_NETWORK	0x00000002	/* may get data over the network */
#define	VFCF_READONLY	0x00000004	/* writes are not implemented */
#define VFCF_SYNTHETIC	0x00000008	/* data does not represent real files */
#define	VFCF_LOOPBACK	0x00000010	/* aliases some other mounted FS */
#define	VFCF_UNICODE	0x00000020	/* stores file names as Unicode*/

/*
 * Operations supported on mounted file system.
 */
#ifdef KERNEL

extern	int	doforce;	/* Flag to permit forcible unmounting. */
extern struct vfsconf void_vfsconf;
extern struct vfsconf *vfsconf[];

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
	int	(*vfs_fhtovp)	__P((struct mount *mp, struct fid *fhp,
				    struct mbuf *nam, struct vnode **vpp,
				    int *exflagsp, struct ucred **credanonp));
	int	(*vfs_vptofh)	__P((struct vnode *vp, struct fid *fhp));
	int	(*vfs_init)	__P((void));
	int	(*vfs_sysctl)	__P((int *, u_int, void *, size_t *, void *, size_t,
				     struct proc *));
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
		flags \
	}; \
	extern struct linker_set MODVNOPS; \
	MOD_VFS(#fsname,index,&MODVNOPS,&_fs_vfsconf); \
	int \
	fsname ## _mod(struct lkm_table *lkmtp, int cmd, int ver) { \
		DISPATCH(lkmtp, cmd, ver, nosys, nosys, nosys); }
#else

#define VFS_SET(vfsops, fsname, index, flags) \
	static struct vfsconf _fs_vfsconf = { \
		&vfsops, \
		#fsname, \
		index, \
		0, \
		flags | VFCF_STATIC \
	}; \
	DATA_SET(vfs_set,_fs_vfsconf)
#endif /* VFS_LKM */

#endif /* KERNEL */

/*
 * Flags for various system call interfaces.
 *
 * waitfor flags to vfs_sync() and getfsstat()
 */
#define MNT_WAIT	1
#define MNT_NOWAIT	2

/*
 * Generic file handle
 */
struct fhandle {
	fsid_t	fh_fsid;	/* File system id of mount point */
	struct	fid fh_fid;	/* File sys specific id */
};
typedef struct fhandle	fhandle_t;

#ifdef KERNEL
#include <net/radix.h>
#include <sys/socket.h>		/* XXX for AF_MAX */

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
#endif /* KERNEL */

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
};

/*
 * Arguments to mount UFS-based filesystems
 */
struct ufs_args {
	char	*fspec;			/* block special device to mount */
	struct	export_args export;	/* network export information */
};

#ifdef MFS
/*
 * Arguments to mount MFS
 */
struct mfs_args {
	char	*fspec;			/* name to export for statfs */
	struct	export_args export;	/* if exported MFSes are supported */
	caddr_t	base;			/* base of file system in memory */
	u_long size;			/* size of file system */
};
#endif /* MFS */

#ifdef MSDOSFS
/*
 *  Arguments to mount MSDOS filesystems.
 */
struct msdosfs_args {
	char	*fspec;		/* blocks special holding the fs to mount */
	struct	export_args export;	/* network export information */
	uid_t	uid;		/* uid that owns msdosfs files */
	gid_t	gid;		/* gid that owns msdosfs files */
	mode_t	mask;		/* mask to be applied for msdosfs perms */
};
#endif

#ifdef CD9660
/*
 * Arguments to mount ISO 9660 filesystems.
 */
struct iso_args {
	char *fspec;			/* block special device to mount */
	struct	export_args export;	/* network export info */
	int flags;			/* mounting flags, see below */

};
#define ISOFSMNT_NORRIP		0x00000001 /* disable Rock Ridge Ext.*/
#define ISOFSMNT_GENS		0x00000002 /* enable generation numbers */
#define ISOFSMNT_EXTATT		0x00000004 /* enable extended attributes */
#endif /* CD9660 */

#ifdef NFS
/*
 * Arguments to mount NFS
 */
struct nfs_args {
	struct sockaddr	*addr;		/* file server address */
	int		addrlen;	/* length of address */
	int		sotype;		/* Socket type */
	int		proto;		/* and Protocol */
	u_char		*fh;		/* File handle to be mounted */
	int		fhsize;		/* Size, in bytes, of fh */
	int		flags;		/* flags */
	int		wsize;		/* write size in bytes */
	int		rsize;		/* read size in bytes */
	int		readdirsize;	/* readdir size in bytes */
	int		timeo;		/* initial timeout in .1 secs */
	int		retrans;	/* times to retry send */
	int		maxgrouplist;	/* Max. size of group list */
	int		readahead;	/* # of blocks to readahead */
	int		leaseterm;	/* Term (sec) of lease */
	int		deadthresh;	/* Retrans threshold */
	char		*hostname;	/* server's name */
};

/*
 * NFS mount option flags
 */
#define	NFSMNT_SOFT		0x00000001  /* soft mount (hard is default) */
#define	NFSMNT_WSIZE		0x00000002  /* set write size */
#define	NFSMNT_RSIZE		0x00000004  /* set read size */
#define	NFSMNT_TIMEO		0x00000008  /* set initial timeout */
#define	NFSMNT_RETRANS		0x00000010  /* set number of request retrys */
#define	NFSMNT_MAXGRPS		0x00000020  /* set maximum grouplist size */
#define	NFSMNT_INT		0x00000040  /* allow interrupts on hard mount */
#define	NFSMNT_NOCONN		0x00000080  /* Don't Connect the socket */
#define	NFSMNT_NQNFS		0x00000100  /* Use Nqnfs protocol */
#define	NFSMNT_NFSV3		0x00000200  /* Use NFS Version 3 protocol */
#define	NFSMNT_KERB		0x00000400  /* Use Kerberos authentication */
#define	NFSMNT_DUMBTIMR		0x00000800  /* Don't estimate rtt dynamically */
#define	NFSMNT_LEASETERM	0x00001000  /* set lease term (nqnfs) */
#define	NFSMNT_READAHEAD	0x00002000  /* set read ahead */
#define	NFSMNT_DEADTHRESH	0x00004000  /* set dead server retry thresh */
#define	NFSMNT_RESVPORT		0x00008000  /* Allocate a reserved port */
#define NFSMNT_RDIRPLUS		0x00010000  /* Use Readdirplus for V3 */
#define NFSMNT_READDIRSIZE	0x00020000  /* Set readdir size */
#define	NFSMNT_INTERNAL		0xfffc0000  /* Bits set internally */
#define NFSMNT_HASWRITEVERF	0x00040000  /* Has write verifier for V3 */
#define NFSMNT_GOTPATHCONF	0x00080000  /* Got the V3 pathconf info */
#define NFSMNT_GOTFSINFO	0x00100000  /* Got the V3 fsinfo */
#define	NFSMNT_MNTD		0x00200000  /* Mnt server for mnt point */
#define	NFSMNT_DISMINPROG	0x00400000  /* Dismount in progress */
#define	NFSMNT_DISMNT		0x00800000  /* Dismounted */
#define	NFSMNT_SNDLOCK		0x01000000  /* Send socket lock */
#define	NFSMNT_WANTSND		0x02000000  /* Want above */
#define	NFSMNT_RCVLOCK		0x04000000  /* Rcv socket lock */
#define	NFSMNT_WANTRCV		0x08000000  /* Want above */
#define	NFSMNT_WAITAUTH		0x10000000  /* Wait for authentication */
#define	NFSMNT_HASAUTH		0x20000000  /* Has authenticator */
#define	NFSMNT_WANTAUTH		0x40000000  /* Wants an authenticator */
#define	NFSMNT_AUTHERR		0x80000000  /* Authentication error */
#endif /* NFS */

#ifdef KERNEL
extern	int (*mountroot) __P((void *));
extern	struct vfsops	*mountrootvfsops;

/*
 * exported vnode operations
 */
int	dounmount __P((struct mount *, int, struct proc *));
struct	mount *getvfs __P((fsid_t *));      /* return vfs given fsid */
void	getnewfsid __P((struct mount *, int));
int	vflush __P((struct mount *, struct vnode *, int));
int	vfs_export			    /* process mount export info */
	  __P((struct mount *, struct netexport *, struct export_args *));
struct	netcred *vfs_export_lookup	    /* lookup host in fs export list */
	  __P((struct mount *, struct netexport *, struct mbuf *));
int	vfs_lock __P((struct mount *));         /* lock a vfs */
int	vfs_mountedon __P((struct vnode *));    /* is a vfs mounted on vp */
int	vfs_mountroot __P((void *));		/* XXX goes away? */
void	vfs_msync __P((struct mount *, int));
void	vfs_unlock __P((struct mount *));       /* unlock a vfs */
void	vfs_unmountall __P((void));
int	vfs_busy __P((struct mount *));         /* mark a vfs  busy */
void	vfs_unbusy __P((struct mount *));       /* mark a vfs not busy */
extern	CIRCLEQ_HEAD(mntlist, mount) mountlist;	/* mounted filesystem list */
extern	struct vfsops *vfssw[];			/* filesystem type table */

#else /* KERNEL */

#include <sys/cdefs.h>

__BEGIN_DECLS
int	fstatfs __P((int, struct statfs *));
int	getfh __P((const char *, fhandle_t *));
int	getfsstat __P((struct statfs *, long, int));
int	getmntinfo __P((struct statfs **, int));
int	mount __P((int, const char *, int, void *));
int	statfs __P((const char *, struct statfs *));
int	unmount __P((const char *, int));

/* C library stuff */
struct	vfsconf *getvfsbyname __P((const char *));
struct	vfsconf *getvfsbytype __P((int));
struct	vfsconf *getvfsent __P((void));
void	setvfsent __P((int));
void	endvfsent __P((void));
int	vfsisloadable __P((const char *));
int	vfsload __P((const char *));
__END_DECLS

#endif /* KERNEL */

#endif /* !_SYS_MOUNT_H_ */
