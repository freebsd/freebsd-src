/*
 * Copyright (c) 1983 The Regents of the University of California.
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
 *	@(#)restore.h	5.8 (Berkeley) 6/1/90
 */

#include <stdio.h>
#include <sys/param.h>
#include <sys/time.h>
#include <ufs/dinode.h>
#include <ufs/fs.h>

/*
 * Flags
 */
extern int	cvtflag;	/* convert from old to new tape format */
extern int	bflag;		/* set input block size */
extern int	dflag;		/* print out debugging info */
extern int	hflag;		/* restore heirarchies */
extern int	mflag;		/* restore by name instead of inode number */
extern int	Nflag;		/* do not write the disk */
extern int	vflag;		/* print out actions taken */
extern int	yflag;		/* always try to recover from tape errors */
/*
 * Global variables
 */
extern char	*dumpmap; 	/* map of inodes on this dump tape */
extern char	*clrimap; 	/* map of inodes to be deleted */
extern ino_t	maxino;		/* highest numbered inode in this file system */
extern long	dumpnum;	/* location of the dump on this tape */
extern long	volno;		/* current volume being read */
extern long	ntrec;		/* number of TP_BSIZE records per tape block */
extern time_t	dumptime;	/* time that this dump begins */
extern time_t	dumpdate;	/* time that this dump was made */
extern char	command;	/* opration being performed */
extern FILE	*terminal;	/* file descriptor for the terminal input */

/*
 * Each file in the file system is described by one of these entries
 */
struct entry {
	char	*e_name;		/* the current name of this entry */
	u_char	e_namlen;		/* length of this name */
	char	e_type;			/* type of this entry, see below */
	short	e_flags;		/* status flags, see below */
	ino_t	e_ino;			/* inode number in previous file sys */
	long	e_index;		/* unique index (for dumpped table) */
	struct	entry *e_parent;	/* pointer to parent directory (..) */
	struct	entry *e_sibling;	/* next element in this directory (.) */
	struct	entry *e_links;		/* hard links to this inode */
	struct	entry *e_entries;	/* for directories, their entries */
	struct	entry *e_next;		/* hash chain list */
};
/* types */
#define	LEAF 1			/* non-directory entry */
#define NODE 2			/* directory entry */
#define LINK 4			/* synthesized type, stripped by addentry */
/* flags */
#define EXTRACT		0x0001	/* entry is to be replaced from the tape */
#define NEW		0x0002	/* a new entry to be extracted */
#define KEEP		0x0004	/* entry is not to change */
#define REMOVED		0x0010	/* entry has been removed */
#define TMPNAME		0x0020	/* entry has been given a temporary name */
#define EXISTED		0x0040	/* directory already existed during extract */
/*
 * functions defined on entry structs
 */
extern struct entry *lookupino();
extern struct entry *lookupname();
extern struct entry *lookupparent();
extern struct entry *addentry();
extern char *myname();
extern char *savename();
extern char *gentempname();
extern char *flagvalues();
extern ino_t lowerbnd();
extern ino_t upperbnd();
#define NIL ((struct entry *)(0))
/*
 * Constants associated with entry structs
 */
#define HARDLINK	1
#define SYMLINK		2
#define TMPHDR		"RSTTMP"

/*
 * The entry describes the next file available on the tape
 */
struct context {
	char	*name;		/* name of file */
	ino_t	ino;		/* inumber of file */
	struct	dinode *dip;	/* pointer to inode */
	char	action;		/* action being taken on this file */
} curfile;
/* actions */
#define	USING	1	/* extracting from the tape */
#define	SKIP	2	/* skipping */
#define UNKNOWN 3	/* disposition or starting point is unknown */

/*
 * Definitions for library routines operating on directories.
 */
typedef struct dirdesc DIR;
extern DIR *rst_opendir();
extern struct direct *rst_readdir();

/*
 * Other exported routines
 */
extern ino_t psearch();
extern ino_t dirlookup();
extern long listfile();
extern long deletefile();
extern long addfile();
extern long nodeupdates();
extern long verifyfile();
extern char *rindex();
extern char *index();
extern char *strcat();
extern char *strncat();
extern char *strcpy();
extern char *strncpy();
extern char *fgets();
extern char *mktemp();
extern char *malloc();
extern char *calloc();
extern char *realloc();
extern long lseek();

/*
 * Useful macros
 */
#define	MWORD(m,i) (m[(unsigned)(i-1)/NBBY])
#define	MBIT(i)	(1<<((unsigned)(i-1)%NBBY))
#define	BIS(i,w)	(MWORD(w,i) |=  MBIT(i))
#define	BIC(i,w)	(MWORD(w,i) &= ~MBIT(i))
#define	BIT(i,w)	(MWORD(w,i) & MBIT(i))

#define dprintf		if (dflag) fprintf
#define vprintf		if (vflag) fprintf

#define GOOD 1
#define FAIL 0
