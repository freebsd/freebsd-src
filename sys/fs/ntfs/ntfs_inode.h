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
 *	$Id: ntfs_inode.h,v 1.8 1999/02/02 01:54:54 semen Exp $
 */

/* These flags are kept in i_flag. */
#if __FreeBSD_version >= 300000
#define	IN_ACCESS	0x0001	/* Access time update request. */
#define	IN_CHANGE	0x0002	/* Inode change time update request. */
#define	IN_UPDATE	0x0004	/* Modification time update request. */
#define	IN_MODIFIED	0x0008	/* Inode has been modified. */
#define	IN_RENAME	0x0010	/* Inode is being renamed. */
#define	IN_SHLOCK	0x0020	/* File has shared lock. */
#define	IN_EXLOCK	0x0040	/* File has exclusive lock. */
#define	IN_HASHED	0x0080	/* Inode is on hash list */
#define	IN_LAZYMOD	0x0100	/* Modified, but don't write yet. */
#else
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

#define	IN_LOADED	0x8000	/* ntvattrs loaded */
#define	IN_PRELOADED	0x4000	/* loaded from directory entry */
#define	IN_AATTRNAME	0x2000	/* spaec allocated for i_defattrname */

struct ntnode {
#if __FreeBSD_version >= 300000
	struct lock     i_lock;	/* Must be first */
#endif
	LIST_ENTRY(ntnode) i_hash;
	struct ntnode  *i_next;
	struct ntnode **i_prev;
	struct vnode   *i_vnode;
	struct vnode   *i_devvp;
	struct ntfsmount *i_mp;
	enum vtype      i_type;
	dev_t           i_dev;
	ino_t           i_number;
	u_int32_t       i_flag;

	long		i_nlink;	/* MFR */
	ino_t		i_mainrec;	/* MFR */
	u_int32_t	i_frflag;	/* MFR */
	ntfs_times_t	i_times;	/* $NAME/dirinfo */
	ino_t		i_pnumber;	/* $NAME/dirinfo */
	u_int32_t       i_fflag;	/* $NAME/dirinfo */
	u_int64_t	i_size;		/* defattr/dirinfo: */
	u_int64_t	i_allocated;	/* defattr/dirinfo */

	u_int32_t       i_lastdattr;
	u_int32_t       i_lastdblnum;
	u_int32_t       i_lastdoff;
	u_int32_t       i_lastdnum;
	caddr_t         i_dirblbuf;
	u_int32_t       i_dirblsz;

	uid_t           i_uid;
	gid_t           i_gid;
	mode_t          i_mode;

	u_int32_t       i_defattr;
	char           *i_defattrname;
	struct ntvattr *i_vattrp;

	int             i_lockcount;	/* Process lock count (recursion) */
	pid_t           i_lockholder;	/* DEBUG: holder of ntnode lock. */
	pid_t           i_lockwaiter;	/* DEBUG: waiter of ntnode lock. */
};
