/*-
 * SPDX-License-Identifier: BSD-3-Clause and BSD-2-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include <sys/queue.h>

#define	MAXDUP		10	/* limit on dup blks (per inode) */
#define	MAXBAD		10	/* limit on bad blks (per inode) */
#define	MINBUFS		100	/* minimum number of buffers required */
#define	INOBUFSIZE	64*1024	/* size of buffer to read inodes in pass1 */
#define	ZEROBUFSIZE	(dev_bsize * 128) /* size of zero buffer used by -Z */

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
	u_char	ino_state;	/* state of inode, see below */
	u_char	ino_type:4;	/* type of inode */
	u_char	ino_idtype:4;	/* idesc id_type, SNAP or ADDR */
	u_short	ino_linkcnt;	/* number of links not found */
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
extern struct inostatlist {
	long	il_numalloced;	/* number of inodes allocated in this cg */
	struct inostat *il_stat;/* inostat info for this cylinder group */
} *inostathead;

/*
 * Structure to reference a dinode.
 */
struct inode {
	struct bufarea *i_bp;	/* buffer containing the dinode */
	union dinode *i_dp;	/* pointer to dinode in buffer */
	ino_t i_number;		/* inode number */
};

/*
 * Size of hash tables
 */
#define	HASHSIZE	2048
#define	HASH(x)		((x * 2654435761) & (HASHSIZE - 1))

/*
 * buffer cache structure.
 */
struct bufarea {
	TAILQ_ENTRY(bufarea) b_list;		/* LRU buffer queue */
	LIST_ENTRY(bufarea) b_hash;		/* hash list */
	ufs2_daddr_t b_bno;			/* disk block number */
	int b_size;				/* size of I/O */
	int b_errs;				/* I/O error */
	int b_flags;				/* B_ flags below */
	int b_type;				/* BT_ type below */
	int b_refcnt;				/* ref count of users */
	int b_index;				/* for BT_LEVEL, ptr index */
						/* for BT_INODES, first inum */
	union {
		char *b_buf;			/* buffer space */
		ufs1_daddr_t *b_indir1;		/* UFS1 indirect block */
		ufs2_daddr_t *b_indir2;		/* UFS2 indirect block */
		struct fs *b_fs;		/* super block */
		struct cg *b_cg;		/* cylinder group */
		struct ufs1_dinode *b_dinode1;	/* UFS1 inode block */
		struct ufs2_dinode *b_dinode2;	/* UFS2 inode block */
	} b_un;
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

/*
 * Buffer flags
 */
#define	B_DIRTY 	0x00000001	/* Buffer is dirty */
/*
 * Type of data in buffer
 */
#define	BT_UNKNOWN 	 0	/* Buffer type is unknown */
#define	BT_SUPERBLK 	 1	/* Buffer holds a superblock */
#define	BT_CYLGRP 	 2	/* Buffer holds a cylinder group map */
#define	BT_LEVEL1 	 3	/* Buffer holds single level indirect */
#define	BT_LEVEL2 	 4	/* Buffer holds double level indirect */
#define	BT_LEVEL3 	 5	/* Buffer holds triple level indirect */
#define	BT_EXTATTR 	 6	/* Buffer holds external attribute data */
#define	BT_INODES 	 7	/* Buffer holds inodes */
#define	BT_DIRDATA 	 8	/* Buffer holds directory data */
#define	BT_DATA	 	 9	/* Buffer holds user data */
#define BT_NUMBUFTYPES	10
#define BT_NAMES {			\
	"unknown",			\
	"Superblock",			\
	"Cylinder Group",		\
	"Single Level Indirect",	\
	"Double Level Indirect",	\
	"Triple Level Indirect",	\
	"External Attribute",		\
	"Inode Block",			\
	"Directory Contents",		\
	"User Data" }
extern char *buftype[];
#define BT_BUFTYPE(type) \
	type < BT_NUMBUFTYPES ? buftype[type] : buftype[BT_UNKNOWN]
extern long readcnt[BT_NUMBUFTYPES];
extern long totalreadcnt[BT_NUMBUFTYPES];
extern struct timespec readtime[BT_NUMBUFTYPES];
extern struct timespec totalreadtime[BT_NUMBUFTYPES];
extern struct timespec startprog;

extern struct bufarea *icachebp;	/* inode cache buffer */
extern struct bufarea sblk;		/* file system superblock */
extern struct bufarea *pdirbp;		/* current directory contents */

#define	dirty(bp) do { \
	if (fswritefd < 0) \
		pfatal("SETTING DIRTY FLAG IN READ_ONLY MODE\n"); \
	else \
		(bp)->b_flags |= B_DIRTY; \
} while (0)
#define	initbarea(bp, type) do { \
	(bp)->b_bno = (ufs2_daddr_t)-4; \
	(bp)->b_size = 0; \
	(bp)->b_errs = 0; \
	(bp)->b_flags = 0; \
	(bp)->b_type = type; \
	(bp)->b_refcnt = 0; \
	(bp)->b_index = 0; \
} while (0)

#define	sbdirty()	dirty(&sblk)
#define	sblock		(*sblk.b_un.b_fs)

enum fixstate {DONTKNOW, NOFIX, FIX, IGNORE};
extern ino_t cursnapshot;

struct inodesc {
	enum fixstate id_fix;	/* policy on fixing errors */
	int (*id_func)(struct inodesc *);
				/* function to be applied to blocks of inode */
	struct bufarea *id_bp;	/* ckinode: buffer with indirect pointers */
	union dinode *id_dp;	/* ckinode: dinode being traversed */
	ino_t id_number;	/* inode number described */
	ino_t id_parent;	/* for DATA nodes, their parent */
	ufs_lbn_t id_lbn;	/* logical block number of current block */
	ufs2_daddr_t id_blkno;	/* current block number being examined */
	int id_level;		/* level of indirection of this block */
	int id_numfrags;	/* number of frags contained in block */
	ufs_lbn_t id_lballoc;	/* pass1: last LBN that is allocated */
	off_t id_filesize;	/* for DATA nodes, the size of the directory */
	ufs2_daddr_t id_entryno;/* for DATA nodes, current entry number */
	int id_loc;		/* for DATA nodes, current location in dir */
	struct direct *id_dirp;	/* for DATA nodes, ptr to current entry */
	char *id_name;		/* for DATA nodes, name to find or enter */
	char id_type;		/* type of descriptor, DATA, ADDR, or SNAP */
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
 * the entire list must be searched for occurrences of the block
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
extern struct dups *duplist;		/* head of dup list */
extern struct dups *muldup;		/* end of unique duplicate dup block numbers */

/*
 * Inode cache data structures.
 */
struct inoinfo {
	SLIST_ENTRY(inoinfo) i_hash;	/* hash list */
	ino_t	i_number;		/* inode number of this entry */
	ino_t	i_parent;		/* inode number of parent */
	ino_t	i_dotdot;		/* inode number of `..' */
	size_t	i_isize;		/* size of inode */
	u_int	i_depth;		/* depth of directory from root */
	u_int	i_flags;		/* flags, see below */
	u_int	i_numblks;		/* size of block array in bytes */
	ufs2_daddr_t i_blks[1];		/* actually longer */
};
extern SLIST_HEAD(inohash, inoinfo) *inphash;
extern struct inoinfo **inpsort;
/*
 * flags for struct inoinfo
 */
#define INFO_NEW	0x0000001	/* replaced broken directory */

extern long dirhash, inplast;
extern unsigned long numdirs, listmax;
extern long countdirs;		/* number of directories we actually found */

#define MIBSIZE	3		/* size of fsck sysctl MIBs */
extern int adjblkcnt[MIBSIZE];	/* MIB cmd to adjust inode block count */
extern int adjrefcnt[MIBSIZE];	/* MIB cmd to adjust inode reference count */
extern int adjndir[MIBSIZE];	/* MIB cmd to adjust number of directories */
extern int adjnbfree[MIBSIZE];	/* MIB cmd to adjust number of free blocks */
extern int adjnifree[MIBSIZE];	/* MIB cmd to adjust number of free inodes */
extern int adjnffree[MIBSIZE];	/* MIB cmd to adjust number of free frags */
extern int adjnumclusters[MIBSIZE]; /* MIB cmd adjust number of free clusters */
extern int adjdepth[MIBSIZE];	/* MIB cmd to adjust directory depth count */
extern int freefiles[MIBSIZE];	/* MIB cmd to free a set of files */
extern int freedirs[MIBSIZE];	/* MIB cmd to free a set of directories */
extern int freeblks[MIBSIZE];	/* MIB cmd to free a set of data blocks */
extern int setsize[MIBSIZE];	/* MIB cmd to set inode size */
extern struct fsck_cmd cmd;	/* sysctl file system update commands */

extern int bkgrdcheck;		/* determine if background check is possible */
extern int bkgrdsumadj;		/* whether the kernel has the ability to adjust
				   the superblock summary fields */
extern off_t bflag;		/* location of alternate super block */
extern int bkgrdflag;		/* use a snapshot to run on an active system */
extern char *blockmap;		/* ptr to primary blk allocation map */
extern char *cdevname;		/* name of device being checked */
extern int cgheader_corrupt;	/* one or more CG headers are corrupt */
extern char ckclean;		/* only do work if not cleanly unmounted */
extern int ckhashadd;		/* check hashes to be added */
extern char *copybuf;		/* buffer to copy snapshot blocks */
extern int cvtlevel;		/* convert to newer file system format */
extern long dev_bsize;		/* computed value of DEV_BSIZE */
extern u_int real_dev_bsize;	/* actual disk sector size, not overridden */
extern int debug;		/* output debugging info */
extern int Eflag;		/* delete empty data blocks */
extern int fsmodified;		/* 1 => write done to file system */
extern int fsreadfd;		/* file descriptor for reading file system */
extern int fswritefd;		/* file descriptor for writing file system */
extern char havesb;		/* superblock has been read */
extern int inoopt;		/* trim out unused inodes */
extern ino_t lfdir;		/* lost & found directory inode number */
extern int lfmode;		/* lost & found directory creation mode */
extern const char *lfname; 	/* lost & found directory name */
extern ufs2_daddr_t maxfsblock; /* number of blocks in the file system */
extern ino_t maxino;		/* number of inodes in file system */
extern ufs2_daddr_t n_blks;	/* number of blocks in use */
extern ino_t n_files;		/* number of files in use */
extern char nflag;		/* assume a no response */
extern char preen;		/* just fix normal inconsistencies */
extern char rerun;		/* rerun fsck. Only used in non-preen mode */
extern char resolved;		/* cleared if unresolved changes => not clean */
extern int returntosingle;	/* 1 => return to single user mode on exit */
extern long secsize;		/* actual disk sector size */
extern char skipclean;		/* skip clean file systems if preening */
extern int snapcnt;		/* number of active snapshots */
extern struct inode snaplist[FSMAXSNAP + 1]; /* list of active snapshots */
extern char snapname[BUFSIZ];	/* when doing snapshots, the name of the file */
extern int sujrecovery;		/* 1 => doing check using the journal */
extern int surrender;		/* Give up if reads fail */
extern char usedsoftdep;	/* just fix soft dependency inconsistencies */
extern int wantrestart;		/* Restart fsck on early termination */
extern char yflag;		/* assume a yes response */
extern int zflag;		/* zero unused directory space */
extern int Zflag;		/* zero empty data blocks */

extern volatile sig_atomic_t	got_siginfo;	/* received a SIGINFO */
extern volatile sig_atomic_t	got_sigalarm;	/* received a SIGALRM */

#define	clearinode(dp) \
	if (sblock.fs_magic == FS_UFS1_MAGIC) { \
		(dp)->dp1 = zino.dp1; \
	} else { \
		(dp)->dp2 = zino.dp2; \
	}
extern union dinode zino;

#define	setbmap(blkno)	setbit(blockmap, blkno)
#define	testbmap(blkno)	isset(blockmap, blkno)
#define	clrbmap(blkno)	clrbit(blockmap, blkno)

#define	STOP	0x01
#define	SKIP	0x02
#define	KEEPON	0x04
#define	ALTERED	0x08
#define	FOUND	0x10

#define	EEXIT	8		/* Standard error exit. */
#define	ERERUN	16		/* fsck needs to be re-run. */
#define	ERESTART -1

int flushentry(void);
/*
 * Wrapper for malloc() that flushes the cylinder group cache to try 
 * to get space.
 */
static inline void*
Malloc(size_t size)
{
	void *retval;

	while ((retval = malloc(size)) == NULL)
		if (flushentry() == 0)
			break;
	return (retval);
}

/*
 * Wrapper for calloc() that flushes the cylinder group cache to try 
 * to get space.
 */
static inline void*
Calloc(size_t cnt, size_t size)
{
	void *retval;

	while ((retval = calloc(cnt, size)) == NULL)
		if (flushentry() == 0)
			break;
	return (retval);
}

struct fstab;


void		adjust(struct inodesc *, int lcnt);
void		alarmhandler(int sig);
ufs2_daddr_t	allocblk(long cg, long frags, ufs2_daddr_t (*checkblkavail)
		    (ufs2_daddr_t blkno, long frags));
ino_t		allocdir(ino_t parent, ino_t request, int mode);
ino_t		allocino(ino_t request, int type);
void		binval(struct bufarea *);
void		blkerror(ino_t ino, const char *type, ufs2_daddr_t blk);
char	       *blockcheck(char *name);
int		blread(int fd, char *buf, ufs2_daddr_t blk, long size);
void		bufinit(void);
void		blwrite(int fd, char *buf, ufs2_daddr_t blk, ssize_t size);
void		blerase(int fd, ufs2_daddr_t blk, long size);
void		blzero(int fd, ufs2_daddr_t blk, long size);
void		brelse(struct bufarea *);
struct inoinfo *cacheino(union dinode *dp, ino_t inumber);
void		catch(int);
void		catchquit(int);
void		cgdirty(struct bufarea *);
struct bufarea *cglookup(int cg);
int		changeino(ino_t dir, const char *name, ino_t newnum, int depth);
void		check_blkcnt(struct inode *ip);
int		check_cgmagic(int cg, struct bufarea *cgbp);
void		rebuild_cg(int cg, struct bufarea *cgbp);
void		check_dirdepth(struct inoinfo *inp);
int		chkrange(ufs2_daddr_t blk, int cnt);
void		ckfini(int markclean);
int		ckinode(union dinode *dp, struct inodesc *);
void		clri(struct inodesc *, const char *type, int flag);
int		clearentry(struct inodesc *);
void		copyonwrite(struct fs *, struct bufarea *,
		    ufs2_daddr_t (*checkblkavail)(ufs2_daddr_t, long));
void		direrror(ino_t ino, const char *errmesg);
int		dirscan(struct inodesc *);
int		dofix(struct inodesc *, const char *msg);
int		eascan(struct inodesc *, struct ufs2_dinode *dp);
void		fileerror(ino_t cwd, ino_t ino, const char *errmesg);
void		finalIOstats(void);
int		findino(struct inodesc *);
int		findname(struct inodesc *);
void		flush(int fd, struct bufarea *bp);
int		freeblock(struct inodesc *);
void		freedirino(ino_t ino, ino_t parent);
void		freeino(ino_t ino);
void		freeinodebuf(void);
void		fsckinit(void);
void		fsutilinit(void);
int		ftypeok(union dinode *dp);
void		getblk(struct bufarea *bp, ufs2_daddr_t blk, long size);
struct bufarea *getdatablk(ufs2_daddr_t blkno, long size, int type);
struct inoinfo *getinoinfo(ino_t inumber);
union dinode   *getnextinode(ino_t inumber, int rebuiltcg);
void		getpathname(char *namebuf, ino_t curdir, ino_t ino);
void		ginode(ino_t, struct inode *);
void		gjournal_check(const char *filesys);
void		infohandler(int sig);
void		irelse(struct inode *);
ufs2_daddr_t	ino_blkatoff(union dinode *, ino_t, ufs_lbn_t, int *,
		    struct bufarea **);
void		inocleanup(void);
void		inodirty(struct inode *);
struct inostat *inoinfo(ino_t inum);
void		IOstats(char *what);
int		linkup(ino_t orphan, ino_t parentdir, char *name);
int		makeentry(ino_t parent, ino_t ino, const char *name);
int		openfilesys(char *dev);
void		panic(const char *fmt, ...) __printflike(1, 2);
void		pass1(void);
void		pass1b(void);
int		pass1check(struct inodesc *);
void		pass2(void);
void		pass3(void);
void		pass4(void);
void		pass5(void);
void		pfatal(const char *fmt, ...) __printflike(1, 2);
void		propagate(void);
void		prtbuf(struct bufarea *, const char *, ...) __printflike(2, 3);
void		prtinode(struct inode *);
void		pwarn(const char *fmt, ...) __printflike(1, 2);
int		readsb(void);
int		removecachedino(ino_t);
int		reply(const char *question);
void		rwerror(const char *mesg, ufs2_daddr_t blk);
void		sblock_init(void);
void		setinodebuf(int, ino_t);
int		setup(char *dev);
int		snapblkfree(struct fs *, ufs2_daddr_t, long, ino_t,
		    ufs2_daddr_t (*)(ufs2_daddr_t, long));
void		snapremove(ino_t);
void		snapflush(ufs2_daddr_t (*checkblkavail)(ufs2_daddr_t, long));
ufs2_daddr_t	std_checkblkavail(ufs2_daddr_t blkno, long frags);
ufs2_daddr_t	suj_checkblkavail(ufs2_daddr_t, long);
int		suj_check(const char *filesys);
void		update_maps(struct cg *, struct cg*, int);

#endif	/* !_FSCK_H_ */
