/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)ufsmount.h	8.6 (Berkeley) 3/30/95
 * $Id: ufsmount.h,v 1.10 1997/10/12 20:26:28 phk Exp $
 */

#ifndef _UFS_UFS_UFSMOUNT_H_
#define _UFS_UFS_UFSMOUNT_H_

/*
 * Arguments to mount UFS-based filesystems
 */
struct ufs_args {
	char	*fspec;			/* block special device to mount */
	struct	export_args export;	/* network export information */
};

/*
 * Arguments to mount MFS
 */
struct mfs_args {
	char	*fspec;			/* name to export for statfs */
	struct	export_args export;	/* if exported MFSes are supported */
	caddr_t	base;			/* base of file system in memory */
	u_long	size;			/* size of file system */
};

#ifdef KERNEL

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_UFSMNT);
#endif

struct buf;
struct inode;
struct nameidata;
struct timeval;
struct ucred;
struct uio;
struct vnode;
struct netexport;

/* This structure describes the UFS specific mount structure data. */
struct ufsmount {
	struct	mount *um_mountp;		/* filesystem vfs structure */
	dev_t	um_dev;				/* device mounted */
	struct	vnode *um_devvp;		/* block device mounted vnode */

	union {					/* pointer to superblock */
		struct	lfs *lfs;		/* LFS */
		struct	fs *fs;			/* FFS */
		struct	ext2_sb_info *e2fs;	/* EXT2FS */
	} ufsmount_u;
#define	um_fs	ufsmount_u.fs
#define	um_lfs	ufsmount_u.lfs
#define	um_e2fs	ufsmount_u.e2fs
#define um_e2fsb ufsmount_u.e2fs->s_es

	struct	vnode *um_quotas[MAXQUOTAS];	/* pointer to quota files */
	struct	ucred *um_cred[MAXQUOTAS];	/* quota file access cred */
	u_long	um_nindir;			/* indirect ptrs per block */
	u_long	um_bptrtodb;			/* indir ptr to disk block */
	u_long	um_seqinc;			/* inc between seq blocks */
	time_t	um_btime[MAXQUOTAS];		/* block quota time limit */
	time_t	um_itime[MAXQUOTAS];		/* inode quota time limit */
	char	um_qflags[MAXQUOTAS];		/* quota specific flags */
	struct	netexport um_export;		/* export information */
	int64_t	um_savedmaxfilesize;		/* XXX - limit maxfilesize */
	struct malloc_type *um_malloctype;	/* The inodes malloctype */
	int	(*um_blkatoff) __P((struct vnode *, off_t, char **, struct buf **));
	int	(*um_truncate) __P((struct vnode *, off_t, int, struct ucred *, struct proc *));
	int	(*um_valloc) __P((struct vnode *, int, struct ucred *, struct vnode **));
	int	(*um_vfree) __P((struct vnode *, ino_t, int));
};

#define UFS_BLKATOFF(aa, bb, cc, dd) VFSTOUFS((aa)->v_mount)->um_blkatoff(aa, bb, cc, dd)
#define UFS_TRUNCATE(aa, bb, cc, dd, ee) VFSTOUFS((aa)->v_mount)->um_truncate(aa, bb, cc, dd, ee)
#define UFS_VALLOC(aa, bb, cc, dd) VFSTOUFS((aa)->v_mount)->um_valloc(aa, bb, cc, dd)
#define UFS_VFREE(aa, bb, cc) VFSTOUFS((aa)->v_mount)->um_vfree(aa, bb, cc)

/*
 * Flags describing the state of quotas.
 */
#define	QTF_OPENING	0x01			/* Q_QUOTAON in progress */
#define	QTF_CLOSING	0x02			/* Q_QUOTAOFF in progress */

/* Convert mount ptr to ufsmount ptr. */
#define VFSTOUFS(mp)	((struct ufsmount *)((mp)->mnt_data))

/*
 * Macros to access file system parameters in the ufsmount structure.
 * Used by ufs_bmap.
 */
#define MNINDIR(ump)			((ump)->um_nindir)
#define	blkptrtodb(ump, b)		((b) << (ump)->um_bptrtodb)
#define	is_sequential(ump, a, b)	((b) == (a) + ump->um_seqinc)
#endif /* KERNEL */

#endif
