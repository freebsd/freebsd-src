/*
 * Copyright (c) 1982, 1989 The Regents of the University of California.
 * All rights reserved.
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
 *	from: @(#)inode.h	7.17 (Berkeley) 5/8/91
 *	$FreeBSD$
 */

#ifndef _UFS_INODE_H_
#define _UFS_INODE_H_ 1

#ifdef KERNEL
include "../ufs/dinode.h"
#else
#include "dinode.h"
#endif

/*
 * The inode is used to describe each active (or recently active)
 * file in the UFS filesystem. It is composed of two types of
 * information. The first part is the information that is needed
 * only while the file is active (such as the identity of the file
 * and linkage to speed its lookup). The second part is the 
 * permannent meta-data associated with the file which is read
 * in from the permanent dinode from long term storage when the
 * file becomes active, and is put back when the file is no longer
 * being used.
 */
struct inode {
	struct	inode *i_chain[2]; /* hash chain, MUST be first */
	struct	vnode *i_vnode;	/* vnode associated with this inode */
	struct	vnode *i_devvp;	/* vnode for block I/O */
	u_long	i_flag;		/* see below */
	dev_t	i_dev;		/* device where inode resides */
	ino_t	i_number;	/* the identity of the inode */
	struct	fs *i_fs;	/* filesystem associated with this inode */
	struct	dquot *i_dquot[MAXQUOTAS]; /* pointer to dquot structures */
	struct	lockf *i_lockf;	/* head of byte-level lock list */
	long	i_diroff;	/* offset in dir, where we found last entry */
	off_t	i_endoff;	/* end of useful stuff in directory */
	long	i_spare0;
	long	i_spare1;
	struct	dinode i_din;	/* the on-disk dinode */
};

#define	FASTLINK(ip)	(DFASTLINK((ip)->i_din))
#define	i_symlink	i_din.di_symlink
#define	i_mode		i_din.di_mode
#define	i_nlink		i_din.di_nlink
#define	i_uid		i_din.di_uid
#define	i_gid		i_din.di_gid
#if BYTE_ORDER == LITTLE_ENDIAN || defined(tahoe) /* ugh! -- must be fixed */
#define	i_size		i_din.di_qsize.val[0]
#else /* BYTE_ORDER == BIG_ENDIAN */
#define	i_size		i_din.di_qsize.val[1]
#endif
#define	i_db		i_din.di_db
#define	i_ib		i_din.di_ib
#define	i_atime		i_din.di_atime
#define	i_mtime		i_din.di_mtime
#define	i_ctime		i_din.di_ctime
#define i_blocks	i_din.di_blocks
#define	i_rdev		i_din.di_db[0]
#define i_flags		i_din.di_flags
#define i_gen		i_din.di_gen
#define	i_forw		i_chain[0]
#define	i_back		i_chain[1]
#define	i_di_spare	i_din.di_spare

/* flags */
#define	ILOCKED		0x0001		/* inode is locked */
#define	IWANT		0x0002		/* some process waiting on lock */
#define	IRENAME		0x0004		/* inode is being renamed */
#define	IUPD		0x0010		/* file has been modified */
#define	IACC		0x0020		/* inode access time to be updated */
#define	ICHG		0x0040		/* inode has been changed */
#define	IMOD		0x0080		/* inode has been modified */
#define	ISHLOCK		0x0100		/* file has shared lock */
#define	IEXLOCK		0x0200		/* file has exclusive lock */
#define	ILWAIT		0x0400		/* someone waiting on file lock */

#ifdef KERNEL
/*
 * Convert between inode pointers and vnode pointers
 */
#define VTOI(vp)	((struct inode *)(vp)->v_data)
#define ITOV(ip)	((ip)->i_vnode)

/*
 * Convert between vnode types and inode formats
 */
extern enum vtype	iftovt_tab[];
extern int		vttoif_tab[];
#define IFTOVT(mode)	(iftovt_tab[((mode) & IFMT) >> 12])
#define VTTOIF(indx)	(vttoif_tab[(int)(indx)])

#define MAKEIMODE(indx, mode)	(int)(VTTOIF(indx) | (mode))

extern u_long	nextgennumber;	/* next generation number to assign */

extern ino_t	dirpref();

/*
 * Lock and unlock inodes.
 */
#ifdef notdef
#define	ILOCK(ip) { \
	while ((ip)->i_flag & ILOCKED) { \
		(ip)->i_flag |= IWANT; \
		(void) sleep((caddr_t)(ip), PINOD); \
	} \
	(ip)->i_flag |= ILOCKED; \
}

#define	IUNLOCK(ip) { \
	(ip)->i_flag &= ~ILOCKED; \
	if ((ip)->i_flag&IWANT) { \
		(ip)->i_flag &= ~IWANT; \
		wakeup((caddr_t)(ip)); \
	} \
}
#else
#define ILOCK(ip)	ilock(ip)
#define IUNLOCK(ip)	iunlock(ip)
#endif

#define	IUPDAT(ip, t1, t2, waitfor) { \
	if (ip->i_flag&(IUPD|IACC|ICHG|IMOD)) \
		(void) iupdat(ip, t1, t2, waitfor); \
}

#define	ITIMES(ip, t1, t2) { \
	if ((ip)->i_flag&(IUPD|IACC|ICHG)) { \
		(ip)->i_flag |= IMOD; \
		if ((ip)->i_flag&IACC) \
			(ip)->i_atime = (t1)->tv_sec; \
		if ((ip)->i_flag&IUPD) \
			(ip)->i_mtime = (t2)->tv_sec; \
		if ((ip)->i_flag&ICHG) \
			(ip)->i_ctime = time.tv_sec; \
		(ip)->i_flag &= ~(IACC|IUPD|ICHG); \
	} \
}

/*
 * This overlays the fid sturcture (see mount.h)
 */
struct ufid {
	u_short	ufid_len;	/* length of structure */
	u_short	ufid_pad;	/* force long alignment */
	ino_t	ufid_ino;	/* file number (ino) */
	long	ufid_gen;	/* generation number */
};

/*
 * Prototypes for UFS vnode operations
 */
int ufs_lookup __P((struct vnode *vp, struct nameidata *ndp, struct proc *p));
int ufs_create __P((struct nameidata *ndp, struct vattr *vap, struct proc *p));
int ufs_mknod __P((struct nameidata *ndp, struct vattr *vap, struct ucred *cred,
	struct proc *p));
int ufs_open __P((struct vnode *vp, int mode, struct ucred *cred,
	struct proc *p));
int ufs_close __P((struct vnode *vp, int fflag, struct ucred *cred,
	struct proc *p));
int ufs_access __P((struct vnode *vp, int mode, struct ucred *cred,
	struct proc *p));
int ufs_getattr __P((struct vnode *vp, struct vattr *vap, struct ucred *cred,
	struct proc *p));
int ufs_setattr __P((struct vnode *vp, struct vattr *vap, struct ucred *cred,
	struct proc *p));
int ufs_read __P((struct vnode *vp, struct uio *uio, int ioflag,
	struct ucred *cred));
int ufs_write __P((struct vnode *vp, struct uio *uio, int ioflag,
	struct ucred *cred));
int ufs_ioctl __P((struct vnode *vp, int command, caddr_t data, int fflag,
	struct ucred *cred, struct proc *p));
int ufs_select __P((struct vnode *vp, int which, int fflags, struct ucred *cred,
	struct proc *p));
int ufs_mmap __P((struct vnode *vp, int fflags, struct ucred *cred,
	struct proc *p));
int ufs_fsync __P((struct vnode *vp, int fflags, struct ucred *cred,
	int waitfor, struct proc *p));
int ufs_seek __P((struct vnode *vp, off_t oldoff, off_t newoff,
	struct ucred *cred));
int ufs_remove __P((struct nameidata *ndp, struct proc *p));
int ufs_link __P((struct vnode *vp, struct nameidata *ndp, struct proc *p));
int ufs_rename __P((struct nameidata *fndp, struct nameidata *tdnp,
	struct proc *p));
int ufs_mkdir __P((struct nameidata *ndp, struct vattr *vap, struct proc *p));
int ufs_rmdir __P((struct nameidata *ndp, struct proc *p));
int ufs_symlink __P((struct nameidata *ndp, struct vattr *vap, char *target,
	struct proc *p));
int ufs_readdir __P((struct vnode *vp, struct uio *uio, struct ucred *cred,
	int *eofflagp));
int ufs_readlink __P((struct vnode *vp, struct uio *uio, struct ucred *cred));
int ufs_abortop __P((struct nameidata *ndp));
int ufs_inactive __P((struct vnode *vp, struct proc *p));
int ufs_reclaim __P((struct vnode *vp));
int ufs_lock __P((struct vnode *vp));
int ufs_unlock __P((struct vnode *vp));
int ufs_bmap __P((struct vnode *vp, daddr_t bn, struct vnode **vpp,
	daddr_t *bnp));
int ufs_strategy __P((struct buf *bp));
void ufs_print __P((struct vnode *vp));
int ufs_islocked __P((struct vnode *vp));
int ufs_advlock __P((struct vnode *vp, caddr_t id, int op, struct flock *fl,
	int flags));

extern void blkfree(struct inode *, daddr_t, off_t);
extern void ifree(struct inode *, ino_t, int);
extern void iput(struct inode *);
extern void ilock(struct inode *);
extern void iunlock(struct inode *);
extern void dirbad(struct inode *, off_t, char *);

extern int alloc(struct inode *, daddr_t, daddr_t, int, daddr_t *);
extern int realloccg(struct inode *, off_t, daddr_t, int, int, struct buf **);
extern int ialloc(struct inode *, ino_t, int, struct ucred *, struct inode **);
extern daddr_t blkpref(struct inode *, daddr_t, int, daddr_t *);
extern u_long hashalloc(struct inode *, int, long, int, 
			u_long (*)(struct inode *, int, long, int));
extern daddr_t fragextend(struct inode *, int, long, int, int);
extern daddr_t alloccg(struct inode *, int, daddr_t, int);

struct cg;			/* I really don't want to know why */
struct direct;			/* this header is required by NFS... */

extern daddr_t alloccgblk(struct fs *, struct cg *, daddr_t);
extern ino_t ialloccg(struct inode *, int, daddr_t, int);
extern int ufs_lookup(struct vnode *, struct nameidata *, struct proc *);
extern int dirbadentry(struct direct *, int);
extern int direnter(struct inode *, struct nameidata *);
extern int dirremove(struct nameidata *);
extern int dirrewrite(struct inode *, struct inode *, struct nameidata *);
extern int blkatoff(struct inode *, off_t, char **, struct buf **);
extern int dirempty(struct inode *, ino_t, struct ucred *);
extern int checkpath(struct inode *, struct inode *, struct ucred *);

extern void ufs_init(void);
extern int iget(struct inode *, ino_t, struct inode **);
extern int ufs_inactive(struct vnode *, struct proc *);
extern int ufs_reclaim(struct vnode *);
extern int iupdat(struct inode *, struct timeval *, struct timeval *,
		  int);
extern int itrunc(struct inode *, u_long, int);
extern int indirtrunc(struct inode *, daddr_t, daddr_t, int, long *);

extern int bmap(struct inode *, daddr_t, daddr_t *);
extern int balloc(struct inode *, daddr_t, int, struct buf **, int);

#endif /* KERNEL */
#endif /* _UFS_INODE_H_ */
