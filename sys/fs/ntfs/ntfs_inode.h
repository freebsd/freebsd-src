/*	$NetBSD: ntfs_inode.h,v 1.8 1999/10/31 19:45:26 jdolecek Exp $	*/

/*-
 * Copyright (c) 1998, 1999 Semen Ustimenko
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/* These flags are kept in i_flag. */
#if defined(__FreeBSD__)
#define	IN_ACCESS	0x0001	/* Access time update request. */
#define	IN_CHANGE	0x0002	/* Inode change time update request. */
#define	IN_UPDATE	0x0004	/* Modification time update request. */
#define	IN_MODIFIED	0x0008	/* Inode has been modified. */
#define	IN_RENAME	0x0010	/* Inode is being renamed. */
#define	IN_SHLOCK	0x0020	/* File has shared lock. */
#define	IN_EXLOCK	0x0040	/* File has exclusive lock. */
#define	IN_LAZYMOD	0x0080	/* Modified, but don't write yet. */
#else /* defined(__NetBSD__) */
#define	IN_ACCESS	0x0001	/* Access time update request. */
#define	IN_CHANGE	0x0002	/* Inode change time update request. */
#define	IN_EXLOCK	0x0004	/* File has exclusive lock. */
#define	IN_LOCKED	0x0008	/* Inode lock. */
#define	IN_LWAIT	0x0010	/* Process waiting on file lock. */
#define	IN_MODIFIED	0x0020	/* Inode has been modified. */
#define	IN_RENAME	0x0040	/* Inode is being renamed. */
#define	IN_SHLOCK	0x0080	/* File has shared lock. */
#define	IN_UPDATE	0x0100	/* Modification time update request. */
#define	IN_WANTED	0x0200	/* Inode is wanted by a process. */
#define	IN_RECURSE	0x0400	/* Recursion expected */
#endif

#define	IN_HASHED	0x0800	/* Inode is on hash list */
#define	IN_LOADED	0x8000	/* ntvattrs loaded */
#define	IN_PRELOADED	0x4000	/* loaded from directory entry */

struct ntnode {
	struct vnode   *i_devvp;	/* vnode of blk dev we live on */
	dev_t           i_dev;		/* Device associated with the inode. */

	LIST_ENTRY(ntnode)	i_hash;
	struct ntnode  *i_next;
	struct ntnode **i_prev;
	struct ntfsmount       *i_mp;
	ino_t           i_number;
	u_int32_t       i_flag;

	/* locking */
	struct lock	i_lock;
	struct mtx	i_interlock;
	int		i_usecount;

	LIST_HEAD(,fnode)	i_fnlist;
	LIST_HEAD(,ntvattr)	i_valist;

	long		i_nlink;	/* MFR */
	ino_t		i_mainrec;	/* MFR */
	u_int32_t	i_frflag;	/* MFR */
};

#define	FN_PRELOADED	0x0001
#define	FN_VALID	0x0002
#define	FN_AATTRNAME	0x0004	/* space allocated for f_attrname */
struct fnode {
#ifdef __FreeBSD__
	struct lock	f_lock;	/* fnode lock >Keep this first< */
#endif
	
	LIST_ENTRY(fnode) f_fnlist;
	struct vnode   *f_vp;		/* Associatied vnode */
	struct ntnode  *f_ip;		/* Associated ntnode */
	u_long		f_flag;

	ntfs_times_t	f_times;	/* $NAME/dirinfo */
	ino_t		f_pnumber;	/* $NAME/dirinfo */
	u_int32_t       f_fflag;	/* $NAME/dirinfo */
	u_int64_t	f_size;		/* defattr/dirinfo: */
	u_int64_t	f_allocated;	/* defattr/dirinfo */

	u_int32_t	f_attrtype;
	char	       *f_attrname;

	/* for ntreaddir */
	u_int32_t       f_lastdattr;
	u_int32_t       f_lastdblnum;
	u_int32_t       f_lastdoff;
	u_int32_t       f_lastdnum;
	caddr_t         f_dirblbuf;
	u_int32_t       f_dirblsz;
};

/* This overlays the fid structure (see <sys/mount.h>) */
struct ntfid {
        u_int16_t ntfid_len;     /* Length of structure. */
        u_int16_t ntfid_pad;     /* Force 32-bit alignment. */
        ino_t     ntfid_ino;     /* File number (ino). */
        int32_t   ntfid_gen;     /* Generation number. */
};
