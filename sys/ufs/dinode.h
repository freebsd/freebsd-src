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
 *	from: @(#)dinode.h	7.10 (Berkeley) 5/8/91
 *	$Id: dinode.h,v 1.3 1993/10/16 18:17:35 rgrimes Exp $
 */

/*
 * A dinode contains all the meta-data associated with a UFS file.
 * This structure defines the on-disk format of a dinode.
 */

#define	NDADDR	12		/* direct addresses in inode */
#define	NIADDR	3		/* indirect addresses in inode */

#define	MAXFASTLINK	(((NDADDR+NIADDR) * sizeof(daddr_t)) - 1)

struct dinode {
	u_short	di_mode;	/*  0: mode and type of file */
	short	di_nlink;	/*  2: number of links to file */
	uid_t	di_uid;		/*  4: owner's user id */
	gid_t	di_gid;		/*  6: owner's group id */
	u_quad	di_qsize;	/*  8: number of bytes in file */
	time_t	di_atime;	/* 16: time last accessed */
	long	di_atspare;
	time_t	di_mtime;	/* 24: time last modified */
	long	di_mtspare;
	time_t	di_ctime;	/* 32: last time inode changed */
	long	di_ctspare;
	union {
		struct {
			daddr_t	di_udb[NDADDR];	/* 40: disk block addresses */
			daddr_t	di_uib[NIADDR];	/* 88: indirect blocks */
		} di_addr;
		char di_usymlink[MAXFASTLINK+1];
	} di_un;
	long	di_flags;	/* 100: status, currently unused */
	long	di_blocks;	/* 104: blocks actually held */
	long	di_gen;		/* 108: generation number */
#define	DI_SPARE_SZ	4		/* 112: spare for 4 longs */
	u_long	di_spare[DI_SPARE_SZ];	/* reserved (unused) */
};

#define	di_db		di_un.di_addr.di_udb
#define di_ib		di_un.di_addr.di_uib
#define	di_symlink	di_un.di_usymlink

#if BYTE_ORDER == LITTLE_ENDIAN || defined(tahoe) /* ugh! -- must be fixed */
#define	di_size		di_qsize.val[0]
#else /* BYTE_ORDER == BIG_ENDIAN */
#define	di_size		di_qsize.val[1]
#endif
#define	di_rdev		di_db[0]

/* file modes */
#define	IFMT		0170000		/* mask of file type */
#define	IFIFO		0010000		/* named pipe (fifo) */
#define	IFCHR		0020000		/* character special device */
#define	IFDIR		0040000		/* directory */
#define	IFBLK		0060000		/* block special device */
#define	IFREG		0100000		/* regular file */
#define	IFLNK		0120000		/* symbolic link */
#define	IFSOCK		0140000		/* UNIX domain socket */

#define	ISUID		04000		/* set user identifier when exec'ing */
#define	ISGID		02000		/* set group identifier when exec'ing */
#define	ISVTX		01000		/* save execution information on exit */
#define	IREAD		0400		/* read permission */
#define	IWRITE		0200		/* write permission */
#define	IEXEC		0100		/* execute permission */

#define	DFASTLINK(di) \
	((((di).di_mode & IFMT) == IFLNK) && \
	 ((di).di_size <= MAXFASTLINK) && \
	 ((di).di_size == (di).di_spare[0]))
