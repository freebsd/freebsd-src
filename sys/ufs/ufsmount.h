/*
 * Copyright (c) 1982, 1986, 1989 Regents of the University of California.
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
 *	from: @(#)ufsmount.h	7.9 (Berkeley) 5/8/91
 *	$Id: ufsmount.h,v 1.2 1993/10/16 18:18:05 rgrimes Exp $
 */

/*
 * This structure describes the UFS specific mount structure data.
 */
struct	ufsmount {
	struct	mount *um_mountp;	/* vfs structure for this filesystem */
	dev_t	um_dev;			/* device mounted */
	struct	vnode *um_devvp;	/* vnode for block device mounted */
	struct	fs *um_fs;		/* pointer to superblock */
	struct	vnode *um_quotas[MAXQUOTAS]; /* pointer to quota files */
	struct	ucred *um_cred[MAXQUOTAS]; /* cred for access to quota file */
	time_t	um_btime[MAXQUOTAS];	/* block quota time limit */
	time_t	um_itime[MAXQUOTAS];	/* inode quota time limit */
	char	um_qflags[MAXQUOTAS];	/* quota specific flags, see below */
};
/*
 * Flags describing the state of quotas.
 */
#define	QTF_OPENING	0x01		/* Q_QUOTAON in progress */
#define	QTF_CLOSING	0x02		/* Q_QUOTAOFF in progress */

#ifdef KERNEL
/*
 * Convert mount ptr to ufsmount ptr.
 */
#define VFSTOUFS(mp)	((struct ufsmount *)((mp)->mnt_data))
#endif /* KERNEL */

/*
 * Prototypes for UFS mount operations
 */
int ufs_mount __P((struct mount *mp, char *path, caddr_t data,
	struct nameidata *ndp, struct proc *p));
int ufs_start __P((struct mount *mp, int flags, struct proc *p));
int ufs_unmount __P((struct mount *mp, int mntflags, struct proc *p));
int ufs_root __P((struct mount *mp, struct vnode **vpp));
int ufs_quotactl __P((struct mount *mp, int cmds, int uid, /* should be uid_t */
	caddr_t arg, struct proc *p));
int ufs_statfs __P((struct mount *mp, struct statfs *sbp, struct proc *p));
int ufs_sync __P((struct mount *mp, int waitfor));
int ufs_fhtovp __P((struct mount *mp, struct fid *fhp, struct vnode **vpp));
int ufs_vptofh __P((struct vnode *vp, struct fid *fhp));
int ufs_init __P(());
