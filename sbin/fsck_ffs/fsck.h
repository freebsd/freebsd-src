/*
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Marshall
 * Kirk McKusick and Network Associates Laboratories, the Security
 * Research Division of Network Associates, Inc. under DARPA/SPAWAR
 * contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA CHATS
 * research program.
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
 * Copyright (c) 1980, 1986, 1993
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
 *	@(#)fsck.h	8.4 (Berkeley) 5/9/95
 * $FreeBSD$
 */

#ifndef _FSCK_H_
#define	_FSCK_H_

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define	MAXDUP		10	/* limit on dup blks (per inode) */
#define	MAXBAD		10	/* limit on bad blks (per inode) */
#define	MAXBUFSPACE	40*1024	/* maximum space to allocate to buffers */
#define	INOBUFSIZE	56*1024	/* size of buffer to read inodes in pass1 */

union dinode {
	struct ufs1_dinode dp1;
	struct ufs2_dinode dp2;
};
#define	DIP(dp, field) \
	((sblock.fs_magic == FS_UFS1_MAGIC) ? \
	(dp)->dp1.field : (dp)->dp2.field)

#define DIP_SET(dp, field, val) do { \
	if (sblock.fs_magic == FS_UFS1_MAGIC) \
		(dp)->dp1.field = (val); \
	else \
		(dp)->dp2.field = (val); \
	} while (0)

/*
 * Each inode on the file system is described by the following structure.
 * The linkcnt is initially set to the value in the inode. Each time it
 * is found during the descent in passes 2, 3, and 4 the count is
 * decremented. Any inodes whose count is non-zero after pass 4 needs to
 * have its link count adjusted by the value remaining in ino_linkcnt.
 */
struct inostat {
	char	ino_state;	/* state of inode, see below */
	char	ino_type;	/* type of inode */
	short	ino_linkcnt;	/* number of links not found */
};
/*
 * Inode states.
 */
#define	USTATE	0x1		/* inode not allocated */
#define	FSTATE	0x2		/* inode is file */
#define	FZLINK	0x3		/* inode is file with a link count of zero */
#define	DSTATE	0x4		/* inode is directory */
#define	DZLINK	0x5		/* inode is directory with a zero link count  */
#define	DFOUND	0x6		/* directory found during descent */
/*     		0x7		   UNUSED - see S_IS_DVALID() definition */
#define	DCLEAR	0x8		/* directory is to be cleared */
#define	FCLEAR	0x9		/* file is to be cleared */
/*     	DUNFOUND === (state == DSTATE || state == DZLINK) */
#define	S_IS_DUNFOUND(state)	(((state) & ~0x1) == DSTATE)
/*     	DVALID   === (state == DSTATE || state == DZLINK || state == DFOUND) */
#define	S_IS_DVALID(state)	(((state) & ~0x3) == DSTATE)
#define	INO_IS_DUNFOUND(ino)	S_IS_DUNFOUND(inoinfo(ino)->ino_state)
#define	INO_IS_DVALID(ino)	S_IS_DVALID(inoinfo(ino)->ino_state)
/*
 * Inode state information is contained on per cylinder group lists
 * which are described by the following structure.
 */
struct inostatlist {
	long	il_numalloced;	/* number of inodes allocated in this cg */
	struct inostat *il_stat;/* inostat info for this cylinder group */
} *inostathead;

/*
 * buffer cache structure.
 */
struct bufarea {
	struct bufarea *b_next;		/* free list queue */
	struct bufarea *b_prev;		/* free list queue */
	ufs2_daddr_t b_bno;
	int b_size;
	int b_errs;
	int b_flags;
	union {
		char *b_buf;			/* buffer space */
		ufs1_daddr_t *b_indir1;		/* UFS1 indirect block */
		ufs2_daddr_t *b_indir2;		/* UFS2 indirect block */
		struct fs *b_fs;		/* super block */
		struct cg *b_cg;		/* cylinder group */
		struct ufs1_dinode *b_dinode1;	/* UFS1 inode block */
		struct ufs2_dinode *b_dinode2;	/* UFS2 inode block */
	} b_un;
	char b_dirty;
};

#define	IBLK(bp, i) \
	((sblock.fs_magic == FS_UFS1_MAGIC) ? \
	(bp)->b_un.b_indir1[i] : (bp)->b_un.b_indir2[i])

#define IBLK_SET(bp, i, val) do { \
	if (sblock.fs_magic == FS_UFS1_MAGIC) \
		(bp)->b_un.b_indir1[i] = (val); \
	else \
		(bp)->b_un.b_indir2[i] = (val); \
	} while (0)

#define	B_INUSE 1

#define	MINBUFS		5	/* minimum number of buffers required */
struct bufarea bufhead;		/* head of list of other blks in filesys */
struct bufarea sblk;		/* file system superblock */
struct bufarea cgblk;		/* cylinder group blocks */
struct bufarea *pdirbp;		/* current directory contents */
struct bufarea *pbp;		/* current inode block */

#define	dirty(bp) do { \
	if (fswritefd < 0) \
		pfatal("SETTING DIRTY FLAG IN READ_ONLY MODE\n"); \
	else \
		(bp)->b_dirty = 1; \
} while (0)
#define	initbarea(bp) do { \
	(bp)->b_dirty = 0; \
	(bp)->b_bno = (ufs2_daddr_t)-1; \
	(bp)->b_flags = 0; \
} while (0)

#define	sbdirty()	dirty(&sblk)
#define	cgdirty()	dirty(&cgblk)
#define	sblock		(*sblk.b_un.b_fs)
#define	cgrp		(*cgblk.b_un.b_cg)

enum fixstate {DONTKNOW, NOFIX, FIX, IGNORE};
ino_t cursnapshot;

struct inodesc {
	enum fixstate id_fix;	/* policy on fixing errors */
	int (*id_func)(struct inodesc *);
				/* function to be applied to blocks of inode */
	ino_t id_number;	/* inode number described */
	ino_t id_parent;	/* for DATA nodes, their parent */
	ufs_lbn_t id_lbn;	/* logical block number of current block */
	ufs2_daddr_t id_blkno;	/* current block number being examined */
	int id_numfrags;	/* number of frags contained in block */
	off_t id_filesize;	/* for DATA nodes, the size of the directory */
	ufs2_daddr_t id_entryno;/* for DATA nodes, current entry number */
	int id_loc;		/* for DATA nodes, current location in dir */
	struct direct *id_dirp;	/* for DATA nodes, ptr to current entry */
	char *id_name;		/* for DATA nodes, name to find or enter */
	char id_type;		/* type of descriptor, DATA or ADDR */
};
/* file types */
#define	DATA	1	/* a directory */
#define	SNAP	2	/* a snapshot */
#define	ADDR	3	/* anything but a directory or a snapshot */

/*
 * Linked list of duplicate blocks.
 *
 * The list is composed of two parts. The first part of the
 * list (from duplist through the node pointed to by muldup)
 * contains a single copy of each duplicate block that has been
 * found. The second part of the list (from muldup to the end)
 * contains duplicate blocks that have been found more than once.
 * To check if a block has been found as a duplicate it is only
 * necessary to search from duplist through muldup. To find the
 * total number of times that a block has been found as a duplicate
 * the entire list must be searched for occurences of the block
 * in question. The following diagram shows a sample list where
 * w (found twice), x (found once), y (found three times), and z
 * (found once) are duplicate block numbers:
 *
 *    w -> y -> x -> z -> y -> w -> y
 *    ^		     ^
 *    |		     |
 * duplist	  muldup
 */
struct dups {
	struct dups *next;
	ufs2_daddr_t dup;
};
struct dups *duplist;		/* head of dup list */
struct dups *muldup;		/* end of unique duplicate dup block numbers */

/*
 * Inode cache data structures.
 */
struct inoinfo {
	struct	inoinfo *i_nexthash;	/* next entry in hash chain */
	ino_t	i_number;		/* inode number of this entry */
	ino_t	i_parent;		/* inode number of parent */
	ino_t	i_dotdot;		/* inode number of `..' */
	size_t	i_isize;		/* size of inode */
	u_int	i_numblks;		/* size of block array in bytes */
	ufs2_daddr_t i_blks[1];		/* actually longer */
} **inphead, **inpsort;
long numdirs, dirhash, listmax, inplast;
long countdirs;			/* number of directories we actually found */

#define MIBSIZE	3		/* size of fsck sysctl MIBs */
int	adjrefcnt[MIBSIZE];	/* MIB command to adjust inode reference cnt */
int	adjblkcnt[MIBSIZE];	/* MIB command to adjust inode block count */
int	adjndir[MIBSIZE];	/* MIB command to adjust number of directories */
int	adjnbfree[MIBSIZE];	/* MIB command to adjust number of free blocks */
int	adjnifree[MIBSIZE];	/* MIB command to adjust number of free inodes */
int	adjnffree[MIBSIZE];	/* MIB command to adjust number of free frags */
int	adjnumclusters[MIBSIZE];	/* MIB command to adjust number of free clusters */
int	freefiles[MIBSIZE];	/* MIB command to free a set of files */
int	freedirs[MIBSIZE];	/* MIB command to free a set of directories */
int	freeblks[MIBSIZE];	/* MIB command to free a set of data blocks */
struct	fsck_cmd cmd;		/* sysctl file system update commands */
char	snapname[BUFSIZ];	/* when doing snapshots, the name of the file */
char	*cdevname;		/* name of device being checked */
long	dev_bsize;		/* computed value of DEV_BSIZE */
long	secsize;		/* actual disk sector size */
long	real_dev_bsize;
char	nflag;			/* assume a no response */
char	yflag;			/* assume a yes response */
int	bkgrdflag;		/* use a snapshot to run on an active system */
int	bflag;			/* location of alternate super block */
int	debug;			/* output debugging info */
int	Eflag;			/* zero out empty data blocks */
int	inoopt;			/* trim out unused inodes */
char	ckclean;		/* only do work if not cleanly unmounted */
int	cvtlevel;		/* convert to newer file system format */
int	bkgrdcheck;		/* determine if background check is possible */
int	bkgrdsumadj;		/* whether the kernel have ability to adjust superblock summary */
char	usedsoftdep;		/* just fix soft dependency inconsistencies */
char	preen;			/* just fix normal inconsistencies */
char	rerun;			/* rerun fsck. Only used in non-preen mode */
int	returntosingle;		/* 1 => return to single user mode on exit */
char	resolved;		/* cleared if unresolved changes => not clean */
char	havesb;			/* superblock has been read */
char	skipclean;		/* skip clean file systems if preening */
int	fsmodified;		/* 1 => write done to file system */
int	fsreadfd;		/* file descriptor for reading file system */
int	fswritefd;		/* file descriptor for writing file system */

ufs2_daddr_t maxfsblock;	/* number of blocks in the file system */
char	*blockmap;		/* ptr to primary blk allocation map */
ino_t	maxino;			/* number of inodes in file system */

ino_t	lfdir;			/* lost & found directory inode number */
const char *lfname;		/* lost & found directory name */
int	lfmode;			/* lost & found directory creation mode */

ufs2_daddr_t n_blks;		/* number of blocks in use */
ino_t n_files;			/* number of files in use */

volatile sig_atomic_t	got_siginfo;	/* received a SIGINFO */
volatile sig_atomic_t	got_sigalarm;	/* received a SIGALRM */

#define	clearinode(dp) \
	if (sblock.fs_magic == FS_UFS1_MAGIC) { \
		(dp)->dp1 = ufs1_zino; \
	} else { \
		(dp)->dp2 = ufs2_zino; \
	}
struct	ufs1_dinode ufs1_zino;
struct	ufs2_dinode ufs2_zino;

#define	setbmap(blkno)	setbit(blockmap, blkno)
#define	testbmap(blkno)	isset(blockmap, blkno)
#define	clrbmap(blkno)	clrbit(blockmap, blkno)

#define	STOP	0x01
#define	SKIP	0x02
#define	KEEPON	0x04
#define	ALTERED	0x08
#define	FOUND	0x10

#define	EEXIT	8		/* Standard error exit. */

struct fstab;


void		adjust(struct inodesc *, int lcnt);
ufs2_daddr_t	allocblk(long frags);
ino_t		allocdir(ino_t parent, ino_t request, int mode);
ino_t		allocino(ino_t request, int type);
void		blkerror(ino_t ino, const char *type, ufs2_daddr_t blk);
char	       *blockcheck(char *name);
int		blread(int fd, char *buf, ufs2_daddr_t blk, long size);
void		bufinit(void);
void		blwrite(int fd, char *buf, ufs2_daddr_t blk, long size);
void		blerase(int fd, ufs2_daddr_t blk, long size);
void		cacheino(union dinode *dp, ino_t inumber);
void		catch(int);
void		catchquit(int);
int		changeino(ino_t dir, const char *name, ino_t newnum);
int		check_cgmagic(int cg, struct cg *cgp);
int		chkrange(ufs2_daddr_t blk, int cnt);
void		ckfini(int markclean);
int		ckinode(union dinode *dp, struct inodesc *);
void		clri(struct inodesc *, const char *type, int flag);
int		clearentry(struct inodesc *);
void		direrror(ino_t ino, const char *errmesg);
int		dirscan(struct inodesc *);
int		dofix(struct inodesc *, const char *msg);
int		eascan(struct inodesc *, struct ufs2_dinode *dp);
void		fileerror(ino_t cwd, ino_t ino, const char *errmesg);
int		findino(struct inodesc *);
int		findname(struct inodesc *);
void		flush(int fd, struct bufarea *bp);
void		freeblk(ufs2_daddr_t blkno, long frags);
void		freeino(ino_t ino);
void		freeinodebuf(void);
int		ftypeok(union dinode *dp);
void		getblk(struct bufarea *bp, ufs2_daddr_t blk, long size);
struct bufarea *getdatablk(ufs2_daddr_t blkno, long size);
struct inoinfo *getinoinfo(ino_t inumber);
union dinode   *getnextinode(ino_t inumber, int rebuildcg);
void		getpathname(char *namebuf, ino_t curdir, ino_t ino);
union dinode   *ginode(ino_t inumber);
void		infohandler(int sig);
void		alarmhandler(int sig);
void		inocleanup(void);
void		inodirty(void);
struct inostat *inoinfo(ino_t inum);
int		linkup(ino_t orphan, ino_t parentdir, char *name);
int		makeentry(ino_t parent, ino_t ino, const char *name);
void		panic(const char *fmt, ...) __printflike(1, 2);
void		pass1(void);
void		pass1b(void);
int		pass1check(struct inodesc *);
void		pass2(void);
void		pass3(void);
void		pass4(void);
int		pass4check(struct inodesc *);
void		pass5(void);
void		pfatal(const char *fmt, ...) __printflike(1, 2);
void		pinode(ino_t ino);
void		propagate(void);
void		pwarn(const char *fmt, ...) __printflike(1, 2);
int		readsb(int listerr);
int		reply(const char *question);
void		rwerror(const char *mesg, ufs2_daddr_t blk);
void		sblock_init(void);
void		setinodebuf(ino_t);
int		setup(char *dev);
void		gjournal_check(const char *filesys);
int		suj_check(const char *filesys);

#endif	/* !_FSCK_H_ */
