/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)tape.c	8.9 (Berkeley) 5/1/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/mtio.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <ufs/ufs/dinode.h>
#include <protocols/dumprestore.h>

#include <errno.h>
#include <paths.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "restore.h"
#include "extern.h"

static long	fssize = MAXBSIZE;
static int	mt = -1;
static int	pipein = 0;
static char	*magtape;
static int	blkcnt;
static int	numtrec;
static char	*tapebuf;
static union	u_spcl endoftapemark;
static long	blksread;		/* blocks read since last header */
static long	tapeaddr = 0;		/* current TP_BSIZE tape record */
static long	tapesread;
static jmp_buf	restart;
static int	gettingfile = 0;	/* restart has a valid frame */
static char	*host = NULL;

static int	ofile;
static char	*map;
static char	lnkbuf[MAXPATHLEN + 1];
static int	pathlen;

int		oldinofmt;	/* old inode format conversion required */
int		Bcvt;		/* Swap Bytes (for CCI or sun) */
static int	Qcvt;		/* Swap quads (for sun) */

#define	FLUSHTAPEBUF()	blkcnt = ntrec + 1

static void	 accthdr __P((struct s_spcl *));
static int	 checksum __P((int *));
static void	 findinode __P((struct s_spcl *));
static void	 findtapeblksize __P((void));
static int	 gethead __P((struct s_spcl *));
static void	 readtape __P((char *));
static void	 setdumpnum __P((void));
static u_long	 swabl __P((u_long));
static u_char	*swablong __P((u_char *, int));
static u_char	*swabshort __P((u_char *, int));
static void	 terminateinput __P((void));
static void	 xtrfile __P((char *, long));
static void	 xtrlnkfile __P((char *, long));
static void	 xtrlnkskip __P((char *, long));
static void	 xtrmap __P((char *, long));
static void	 xtrmapskip __P((char *, long));
static void	 xtrskip __P((char *, long));

static int readmapflag;

/*
 * Set up an input source
 */
void
setinput(source)
	char *source;
{
	FLUSHTAPEBUF();
	if (bflag)
		newtapebuf(ntrec);
	else
		newtapebuf(NTREC > HIGHDENSITYTREC ? NTREC : HIGHDENSITYTREC);
	terminal = stdin;

#ifdef RRESTORE
	if (strchr(source, ':')) {
		host = source;
		source = strchr(host, ':');
		*source++ = '\0';
		if (rmthost(host) == 0)
			done(1);
	} else
#endif
	if (strcmp(source, "-") == 0) {
		/*
		 * Since input is coming from a pipe we must establish
		 * our own connection to the terminal.
		 */
		terminal = fopen(_PATH_TTY, "r");
		if (terminal == NULL) {
			(void)fprintf(stderr, "cannot open %s: %s\n",
			    _PATH_TTY, strerror(errno));
			terminal = fopen(_PATH_DEVNULL, "r");
			if (terminal == NULL) {
				(void)fprintf(stderr, "cannot open %s: %s\n",
				    _PATH_DEVNULL, strerror(errno));
				done(1);
			}
		}
		pipein++;
	}
	setuid(getuid());	/* no longer need or want root privileges */
	magtape = strdup(source);
	if (magtape == NULL) {
		fprintf(stderr, "Cannot allocate space for magtape buffer\n");
		done(1);
	}
}

void
newtapebuf(size)
	long size;
{
	static tapebufsize = -1;

	ntrec = size;
	if (size <= tapebufsize)
		return;
	if (tapebuf != NULL)
		free(tapebuf);
	tapebuf = malloc(size * TP_BSIZE);
	if (tapebuf == NULL) {
		fprintf(stderr, "Cannot allocate space for tape buffer\n");
		done(1);
	}
	tapebufsize = size;
}

/*
 * Verify that the tape drive can be accessed and
 * that it actually is a dump tape.
 */
void
setup()
{
	int i, j, *ip;
	struct stat stbuf;

	vprintf(stdout, "Verify tape and initialize maps\n");
#ifdef RRESTORE
	if (host)
		mt = rmtopen(magtape, 0);
	else
#endif
	if (pipein)
		mt = 0;
	else
		mt = open(magtape, O_RDONLY, 0);
	if (mt < 0) {
		fprintf(stderr, "%s: %s\n", magtape, strerror(errno));
		done(1);
	}
	volno = 1;
	setdumpnum();
	FLUSHTAPEBUF();
	if (!pipein && !bflag)
		findtapeblksize();
	if (gethead(&spcl) == FAIL) {
		blkcnt--; /* push back this block */
		blksread--;
		cvtflag++;
		if (gethead(&spcl) == FAIL) {
			fprintf(stderr, "Tape is not a dump tape\n");
			done(1);
		}
		fprintf(stderr, "Converting to new file system format.\n");
	}
	if (pipein) {
		endoftapemark.s_spcl.c_magic = cvtflag ? OFS_MAGIC : NFS_MAGIC;
		endoftapemark.s_spcl.c_type = TS_END;
		ip = (int *)&endoftapemark;
		j = sizeof(union u_spcl) / sizeof(int);
		i = 0;
		do
			i += *ip++;
		while (--j);
		endoftapemark.s_spcl.c_checksum = CHECKSUM - i;
	}
	if (vflag || command == 't')
		printdumpinfo();
	dumptime = spcl.c_ddate;
	dumpdate = spcl.c_date;
	if (stat(".", &stbuf) < 0) {
		fprintf(stderr, "cannot stat .: %s\n", strerror(errno));
		done(1);
	}
	if (stbuf.st_blksize > 0 && stbuf.st_blksize < TP_BSIZE )
		fssize = TP_BSIZE;
	if (stbuf.st_blksize >= TP_BSIZE && stbuf.st_blksize <= MAXBSIZE)
		fssize = stbuf.st_blksize;
	if (((fssize - 1) & fssize) != 0) {
		fprintf(stderr, "bad block size %ld\n", fssize);
		done(1);
	}
	if (spcl.c_volume != 1) {
		fprintf(stderr, "Tape is not volume 1 of the dump\n");
		done(1);
	}
	if (gethead(&spcl) == FAIL) {
		dprintf(stdout, "header read failed at %ld blocks\n", blksread);
		panic("no header after volume mark!\n");
	}
	findinode(&spcl);
	if (spcl.c_type != TS_CLRI) {
		fprintf(stderr, "Cannot find file removal list\n");
		done(1);
	}
	maxino = (spcl.c_count * TP_BSIZE * NBBY) + 1;
	dprintf(stdout, "maxino = %d\n", maxino);
	map = calloc((unsigned)1, (unsigned)howmany(maxino, NBBY));
	if (map == NULL)
		panic("no memory for active inode map\n");
	usedinomap = map;
	curfile.action = USING;
	getfile(xtrmap, xtrmapskip);
	if (spcl.c_type != TS_BITS) {
		fprintf(stderr, "Cannot find file dump list\n");
		done(1);
	}
	map = calloc((unsigned)1, (unsigned)howmany(maxino, NBBY));
	if (map == (char *)NULL)
		panic("no memory for file dump list\n");
	dumpmap = map;
	curfile.action = USING;
	getfile(xtrmap, xtrmapskip);
	/*
	 * If there may be whiteout entries on the tape, pretend that the
	 * whiteout inode exists, so that the whiteout entries can be
	 * extracted.
	 */
	if (oldinofmt == 0)
		SETINO(WINO, dumpmap);
	/* 'r' restores don't call getvol() for tape 1, so mark it as read. */
	if (command == 'r')
		tapesread = 1;
}

/*
 * Prompt user to load a new dump volume.
 * "Nextvol" is the next suggested volume to use.
 * This suggested volume is enforced when doing full
 * or incremental restores, but can be overridden by
 * the user when only extracting a subset of the files.
 */
void
getvol(nextvol)
	long nextvol;
{
	long newvol, prevtapea, savecnt, i;
	union u_spcl tmpspcl;
#	define tmpbuf tmpspcl.s_spcl
	char buf[TP_BSIZE];

	if (nextvol == 1) {
		tapesread = 0;
		gettingfile = 0;
	}
	prevtapea = tapeaddr;
	savecnt = blksread;
	if (pipein) {
		if (nextvol != 1) {
			panic("Changing volumes on pipe input?\n");
			/* Avoid looping if we couldn't ask the user. */
			if (yflag || ferror(terminal) || feof(terminal))
				done(1);
		}
		if (volno == 1)
			return;
		goto gethdr;
	}
again:
	if (pipein)
		done(1); /* pipes do not get a second chance */
	if (command == 'R' || command == 'r' || curfile.action != SKIP)
		newvol = nextvol;
	else
		newvol = 0;
	while (newvol <= 0) {
		if (tapesread == 0) {
			fprintf(stderr, "%s%s%s%s%s%s%s",
			    "You have not read any tapes yet.\n",
			    "If you are extracting just a few files,",
			    " start with the last volume\n",
			    "and work towards the first; restore",
			    " can quickly skip tapes that\n",
			    "have no further files to extract.",
			    " Otherwise, begin with volume 1.\n");
		} else {
			fprintf(stderr, "You have read volumes");
			strcpy(buf, ": ");
			for (i = 0; i < 32; i++)
				if (tapesread & (1 << i)) {
					fprintf(stderr, "%s%ld", buf, i + 1);
					strcpy(buf, ", ");
				}
			fprintf(stderr, "\n");
		}
		do	{
			fprintf(stderr, "Specify next volume #: ");
			(void) fflush(stderr);
			if (fgets(buf, BUFSIZ, terminal) == NULL)
				done(1);
		} while (buf[0] == '\n');
		newvol = atoi(buf);
		if (newvol <= 0) {
			fprintf(stderr,
			    "Volume numbers are positive numerics\n");
		}
	}
	if (newvol == volno) {
		tapesread |= 1 << (volno - 1);
		return;
	}
	closemt();
	fprintf(stderr, "Mount tape volume %ld\n", newvol);
	fprintf(stderr, "Enter ``none'' if there are no more tapes\n");
	fprintf(stderr, "otherwise enter tape name (default: %s) ", magtape);
	(void) fflush(stderr);
	if (fgets(buf, BUFSIZ, terminal) == NULL)
		done(1);
	if (!strcmp(buf, "none\n")) {
		terminateinput();
		return;
	}
	if (buf[0] != '\n') {
		(void) strcpy(magtape, buf);
		magtape[strlen(magtape) - 1] = '\0';
	}
#ifdef RRESTORE
	if (host)
		mt = rmtopen(magtape, 0);
	else
#endif
		mt = open(magtape, O_RDONLY, 0);

	if (mt == -1) {
		fprintf(stderr, "Cannot open %s\n", magtape);
		volno = -1;
		goto again;
	}
gethdr:
	volno = newvol;
	setdumpnum();
	FLUSHTAPEBUF();
	if (gethead(&tmpbuf) == FAIL) {
		dprintf(stdout, "header read failed at %ld blocks\n", blksread);
		fprintf(stderr, "tape is not dump tape\n");
		volno = 0;
		goto again;
	}
	if (tmpbuf.c_volume != volno) {
		fprintf(stderr, "Wrong volume (%ld)\n", tmpbuf.c_volume);
		volno = 0;
		goto again;
	}
	if (tmpbuf.c_date != dumpdate || tmpbuf.c_ddate != dumptime) {
		fprintf(stderr, "Wrong dump date\n\tgot: %s",
			ctime(&tmpbuf.c_date));
		fprintf(stderr, "\twanted: %s", ctime(&dumpdate));
		volno = 0;
		goto again;
	}
	tapesread |= 1 << (volno - 1);
	blksread = savecnt;
 	/*
 	 * If continuing from the previous volume, skip over any
 	 * blocks read already at the end of the previous volume.
 	 *
 	 * If coming to this volume at random, skip to the beginning
 	 * of the next record.
 	 */
	dprintf(stdout, "last rec %ld, tape starts with %ld\n", prevtapea,
	    tmpbuf.c_tapea);
 	if (tmpbuf.c_type == TS_TAPE && (tmpbuf.c_flags & DR_NEWHEADER)) {
 		if (curfile.action != USING) {
			/*
			 * XXX Dump incorrectly sets c_count to 1 in the
			 * volume header of the first tape, so ignore
			 * c_count when volno == 1.
			 */
			if (volno != 1)
				for (i = tmpbuf.c_count; i > 0; i--)
					readtape(buf);
 		} else if (tmpbuf.c_tapea <= prevtapea) {
			/*
			 * Normally the value of c_tapea in the volume
			 * header is the record number of the header itself.
			 * However in the volume header following an EOT-
			 * terminated tape, it is the record number of the
			 * first continuation data block (dump bug?).
			 *
			 * The next record we want is `prevtapea + 1'.
			 */
 			i = prevtapea + 1 - tmpbuf.c_tapea;
			dprintf(stderr, "Skipping %ld duplicate record%s.\n",
				i, i > 1 ? "s" : "");
 			while (--i >= 0)
 				readtape(buf);
 		}
 	}
	if (curfile.action == USING) {
		if (volno == 1)
			panic("active file into volume 1\n");
		return;
	}
	(void) gethead(&spcl);
	findinode(&spcl);
	if (gettingfile) {
		gettingfile = 0;
		longjmp(restart, 1);
	}
}

/*
 * Handle unexpected EOF.
 */
static void
terminateinput()
{

	if (gettingfile && curfile.action == USING) {
		printf("Warning: %s %s\n",
		    "End-of-input encountered while extracting", curfile.name);
	}
	curfile.name = "<name unknown>";
	curfile.action = UNKNOWN;
	curfile.dip = NULL;
	curfile.ino = maxino;
	if (gettingfile) {
		gettingfile = 0;
		longjmp(restart, 1);
	}
}

/*
 * handle multiple dumps per tape by skipping forward to the
 * appropriate one.
 */
static void
setdumpnum()
{
	struct mtop tcom;

	if (dumpnum == 1 || volno != 1)
		return;
	if (pipein) {
		fprintf(stderr, "Cannot have multiple dumps on pipe input\n");
		done(1);
	}
	tcom.mt_op = MTFSF;
	tcom.mt_count = dumpnum - 1;
#ifdef RRESTORE
	if (host)
		rmtioctl(MTFSF, dumpnum - 1);
	else
#endif
		if (ioctl(mt, MTIOCTOP, (char *)&tcom) < 0)
			fprintf(stderr, "ioctl MTFSF: %s\n", strerror(errno));
}

void
printdumpinfo()
{
	fprintf(stdout, "Dump   date: %s", ctime(&spcl.c_date));
	fprintf(stdout, "Dumped from: %s",
	    (spcl.c_ddate == 0) ? "the epoch\n" : ctime(&spcl.c_ddate));
	if (spcl.c_host[0] == '\0')
		return;
	fprintf(stderr, "Level %ld dump of %s on %s:%s\n",
		spcl.c_level, spcl.c_filesys, spcl.c_host, spcl.c_dev);
	fprintf(stderr, "Label: %s\n", spcl.c_label);
}

int
extractfile(name)
	char *name;
{
	int flags;
	mode_t mode;
	struct timeval timep[2];
	struct entry *ep;

	curfile.name = name;
	curfile.action = USING;
	timep[0].tv_sec = curfile.dip->di_atime;
	timep[0].tv_usec = curfile.dip->di_atimensec / 1000;
	timep[1].tv_sec = curfile.dip->di_mtime;
	timep[1].tv_usec = curfile.dip->di_mtimensec / 1000;
	mode = curfile.dip->di_mode;
	flags = curfile.dip->di_flags;
	switch (mode & IFMT) {

	default:
		fprintf(stderr, "%s: unknown file mode 0%o\n", name, mode);
		skipfile();
		return (FAIL);

	case IFSOCK:
		vprintf(stdout, "skipped socket %s\n", name);
		skipfile();
		return (GOOD);

	case IFDIR:
		if (mflag) {
			ep = lookupname(name);
			if (ep == NULL || ep->e_flags & EXTRACT)
				panic("unextracted directory %s\n", name);
			skipfile();
			return (GOOD);
		}
		vprintf(stdout, "extract file %s\n", name);
		return (genliteraldir(name, curfile.ino));

	case IFLNK:
		lnkbuf[0] = '\0';
		pathlen = 0;
		getfile(xtrlnkfile, xtrlnkskip);
		if (pathlen == 0) {
			vprintf(stdout,
			    "%s: zero length symbolic link (ignored)\n", name);
			return (GOOD);
		}
		return (linkit(lnkbuf, name, SYMLINK));

	case IFIFO:
		vprintf(stdout, "extract fifo %s\n", name);
		if (Nflag) {
			skipfile();
			return (GOOD);
		}
		if (uflag && !Nflag)
			(void)unlink(name);
		if (mkfifo(name, mode) < 0) {
			fprintf(stderr, "%s: cannot create fifo: %s\n",
			    name, strerror(errno));
			skipfile();
			return (FAIL);
		}
		(void) chown(name, curfile.dip->di_uid, curfile.dip->di_gid);
		(void) chmod(name, mode);
		utimes(name, timep);
		(void) chflags(name, flags);
		skipfile();
		return (GOOD);

	case IFCHR:
	case IFBLK:
		vprintf(stdout, "extract special file %s\n", name);
		if (Nflag) {
			skipfile();
			return (GOOD);
		}
		if (uflag)
			(void)unlink(name);
		if (mknod(name, mode, (int)curfile.dip->di_rdev) < 0) {
			fprintf(stderr, "%s: cannot create special file: %s\n",
			    name, strerror(errno));
			skipfile();
			return (FAIL);
		}
		(void) chown(name, curfile.dip->di_uid, curfile.dip->di_gid);
		(void) chmod(name, mode);
		utimes(name, timep);
		(void) chflags(name, flags);
		skipfile();
		return (GOOD);

	case IFREG:
		vprintf(stdout, "extract file %s\n", name);
		if (Nflag) {
			skipfile();
			return (GOOD);
		}
		if (uflag)
			(void)unlink(name);
		if ((ofile = open(name, O_WRONLY | O_CREAT | O_TRUNC,
		    0666)) < 0) {
			fprintf(stderr, "%s: cannot create file: %s\n",
			    name, strerror(errno));
			skipfile();
			return (FAIL);
		}
		(void) fchown(ofile, curfile.dip->di_uid, curfile.dip->di_gid);
		(void) fchmod(ofile, mode);
		getfile(xtrfile, xtrskip);
		(void) close(ofile);
		utimes(name, timep);
		(void) chflags(name, flags);
		return (GOOD);
	}
	/* NOTREACHED */
}

/*
 * skip over bit maps on the tape
 */
void
skipmaps()
{

	while (spcl.c_type == TS_BITS || spcl.c_type == TS_CLRI)
		skipfile();
}

/*
 * skip over a file on the tape
 */
void
skipfile()
{

	curfile.action = SKIP;
	getfile(xtrnull, xtrnull);
}

/*
 * Extract a file from the tape.
 * When an allocated block is found it is passed to the fill function;
 * when an unallocated block (hole) is found, a zeroed buffer is passed
 * to the skip function.
 */
void
getfile(fill, skip)
	void	(*fill) __P((char *, long));
	void	(*skip) __P((char *, long));
{
	register int i;
	int curblk = 0;
	quad_t size = spcl.c_dinode.di_size;
	static char clearedbuf[MAXBSIZE];
	char buf[MAXBSIZE / TP_BSIZE][TP_BSIZE];
	char junk[TP_BSIZE];

	if (spcl.c_type == TS_END)
		panic("ran off end of tape\n");
	if (spcl.c_magic != NFS_MAGIC)
		panic("not at beginning of a file\n");
	if (!gettingfile && setjmp(restart) != 0)
		return;
	gettingfile++;
loop:
	for (i = 0; i < spcl.c_count; i++) {
		if (readmapflag || spcl.c_addr[i]) {
			readtape(&buf[curblk++][0]);
			if (curblk == fssize / TP_BSIZE) {
				(*fill)((char *)buf, (long)(size > TP_BSIZE ?
				     fssize : (curblk - 1) * TP_BSIZE + size));
				curblk = 0;
			}
		} else {
			if (curblk > 0) {
				(*fill)((char *)buf, (long)(size > TP_BSIZE ?
				     curblk * TP_BSIZE :
				     (curblk - 1) * TP_BSIZE + size));
				curblk = 0;
			}
			(*skip)(clearedbuf, (long)(size > TP_BSIZE ?
				TP_BSIZE : size));
		}
		if ((size -= TP_BSIZE) <= 0) {
			for (i++; i < spcl.c_count; i++)
				if (readmapflag || spcl.c_addr[i])
					readtape(junk);
			break;
		}
	}
	if (gethead(&spcl) == GOOD && size > 0) {
		if (spcl.c_type == TS_ADDR)
			goto loop;
		dprintf(stdout,
			"Missing address (header) block for %s at %ld blocks\n",
			curfile.name, blksread);
	}
	if (curblk > 0)
		(*fill)((char *)buf, (long)((curblk * TP_BSIZE) + size));
	findinode(&spcl);
	gettingfile = 0;
}

/*
 * Write out the next block of a file.
 */
static void
xtrfile(buf, size)
	char	*buf;
	long	size;
{

	if (Nflag)
		return;
	if (write(ofile, buf, (int) size) == -1) {
		fprintf(stderr,
		    "write error extracting inode %d, name %s\nwrite: %s\n",
			curfile.ino, curfile.name, strerror(errno));
	}
}

/*
 * Skip over a hole in a file.
 */
/* ARGSUSED */
static void
xtrskip(buf, size)
	char *buf;
	long size;
{

	if (lseek(ofile, size, SEEK_CUR) == -1) {
		fprintf(stderr,
		    "seek error extracting inode %d, name %s\nlseek: %s\n",
			curfile.ino, curfile.name, strerror(errno));
		done(1);
	}
}

/*
 * Collect the next block of a symbolic link.
 */
static void
xtrlnkfile(buf, size)
	char	*buf;
	long	size;
{

	pathlen += size;
	if (pathlen > MAXPATHLEN) {
		fprintf(stderr, "symbolic link name: %s->%s%s; too long %d\n",
		    curfile.name, lnkbuf, buf, pathlen);
		done(1);
	}
	(void) strcat(lnkbuf, buf);
}

/*
 * Skip over a hole in a symbolic link (should never happen).
 */
/* ARGSUSED */
static void
xtrlnkskip(buf, size)
	char *buf;
	long size;
{

	fprintf(stderr, "unallocated block in symbolic link %s\n",
		curfile.name);
	done(1);
}

/*
 * Collect the next block of a bit map.
 */
static void
xtrmap(buf, size)
	char	*buf;
	long	size;
{

	memmove(map, buf, size);
	map += size;
}

/*
 * Skip over a hole in a bit map (should never happen).
 */
/* ARGSUSED */
static void
xtrmapskip(buf, size)
	char *buf;
	long size;
{

	panic("hole in map\n");
	map += size;
}

/*
 * Noop, when an extraction function is not needed.
 */
/* ARGSUSED */
void
xtrnull(buf, size)
	char *buf;
	long size;
{

	return;
}

/*
 * Read TP_BSIZE blocks from the input.
 * Handle read errors, and end of media.
 */
static void
readtape(buf)
	char *buf;
{
	long rd, newvol, i;
	int cnt, seek_failed;

	if (blkcnt < numtrec) {
		memmove(buf, &tapebuf[(blkcnt++ * TP_BSIZE)], (long)TP_BSIZE);
		blksread++;
		tapeaddr++;
		return;
	}
	for (i = 0; i < ntrec; i++)
		((struct s_spcl *)&tapebuf[i * TP_BSIZE])->c_magic = 0;
	if (numtrec == 0)
		numtrec = ntrec;
	cnt = ntrec * TP_BSIZE;
	rd = 0;
getmore:
#ifdef RRESTORE
	if (host)
		i = rmtread(&tapebuf[rd], cnt);
	else
#endif
		i = read(mt, &tapebuf[rd], cnt);
	/*
	 * Check for mid-tape short read error.
	 * If found, skip rest of buffer and start with the next.
	 */
	if (!pipein && numtrec < ntrec && i > 0) {
		dprintf(stdout, "mid-media short read error.\n");
		numtrec = ntrec;
	}
	/*
	 * Handle partial block read.
	 */
	if (pipein && i == 0 && rd > 0)
		i = rd;
	else if (i > 0 && i != ntrec * TP_BSIZE) {
		if (pipein) {
			rd += i;
			cnt -= i;
			if (cnt > 0)
				goto getmore;
			i = rd;
		} else {
			/*
			 * Short read. Process the blocks read.
			 */
			if (i % TP_BSIZE != 0)
				vprintf(stdout,
				    "partial block read: %ld should be %ld\n",
				    i, ntrec * TP_BSIZE);
			numtrec = i / TP_BSIZE;
		}
	}
	/*
	 * Handle read error.
	 */
	if (i < 0) {
		fprintf(stderr, "Tape read error while ");
		switch (curfile.action) {
		default:
			fprintf(stderr, "trying to set up tape\n");
			break;
		case UNKNOWN:
			fprintf(stderr, "trying to resynchronize\n");
			break;
		case USING:
			fprintf(stderr, "restoring %s\n", curfile.name);
			break;
		case SKIP:
			fprintf(stderr, "skipping over inode %d\n",
				curfile.ino);
			break;
		}
		if (!yflag && !reply("continue"))
			done(1);
		i = ntrec * TP_BSIZE;
		memset(tapebuf, 0, i);
#ifdef RRESTORE
		if (host)
			seek_failed = (rmtseek(i, 1) < 0);
		else
#endif
			seek_failed = (lseek(mt, i, SEEK_CUR) == (off_t)-1);

		if (seek_failed) {
			fprintf(stderr,
			    "continuation failed: %s\n", strerror(errno));
			done(1);
		}
	}
	/*
	 * Handle end of tape.
	 */
	if (i == 0) {
		vprintf(stdout, "End-of-tape encountered\n");
		if (!pipein) {
			newvol = volno + 1;
			volno = 0;
			numtrec = 0;
			getvol(newvol);
			readtape(buf);
			return;
		}
		if (rd % TP_BSIZE != 0)
			panic("partial block read: %d should be %d\n",
				rd, ntrec * TP_BSIZE);
		terminateinput();
		memmove(&tapebuf[rd], &endoftapemark, (long)TP_BSIZE);
	}
	blkcnt = 0;
	memmove(buf, &tapebuf[(blkcnt++ * TP_BSIZE)], (long)TP_BSIZE);
	blksread++;
	tapeaddr++;
}

static void
findtapeblksize()
{
	register long i;

	for (i = 0; i < ntrec; i++)
		((struct s_spcl *)&tapebuf[i * TP_BSIZE])->c_magic = 0;
	blkcnt = 0;
#ifdef RRESTORE
	if (host)
		i = rmtread(tapebuf, ntrec * TP_BSIZE);
	else
#endif
		i = read(mt, tapebuf, ntrec * TP_BSIZE);

	if (i <= 0) {
		fprintf(stderr, "tape read error: %s\n", strerror(errno));
		done(1);
	}
	if (i % TP_BSIZE != 0) {
		fprintf(stderr, "Tape block size (%ld) %s (%d)\n",
			i, "is not a multiple of dump block size", TP_BSIZE);
		done(1);
	}
	ntrec = i / TP_BSIZE;
	numtrec = ntrec;
	vprintf(stdout, "Tape block size is %ld\n", ntrec);
}

void
closemt()
{

	if (mt < 0)
		return;
#ifdef RRESTORE
	if (host)
		rmtclose();
	else
#endif
		(void) close(mt);
}

/*
 * Read the next block from the tape.
 * Check to see if it is one of several vintage headers.
 * If it is an old style header, convert it to a new style header.
 * If it is not any valid header, return an error.
 */
static int
gethead(buf)
	struct s_spcl *buf;
{
	long i;
	union {
		quad_t	qval;
		int32_t	val[2];
	} qcvt;
	union u_ospcl {
		char dummy[TP_BSIZE];
		struct	s_ospcl {
			int32_t	c_type;
			int32_t	c_date;
			int32_t	c_ddate;
			int32_t	c_volume;
			int32_t	c_tapea;
			u_short	c_inumber;
			int32_t	c_magic;
			int32_t	c_checksum;
			struct odinode {
				unsigned short odi_mode;
				u_short	odi_nlink;
				u_short	odi_uid;
				u_short	odi_gid;
				int32_t	odi_size;
				int32_t	odi_rdev;
				char	odi_addr[36];
				int32_t	odi_atime;
				int32_t	odi_mtime;
				int32_t	odi_ctime;
			} c_dinode;
			int32_t	c_count;
			char	c_addr[256];
		} s_ospcl;
	} u_ospcl;

	if (!cvtflag) {
		readtape((char *)buf);
		if (buf->c_magic != NFS_MAGIC) {
			if (swabl(buf->c_magic) != NFS_MAGIC)
				return (FAIL);
			if (!Bcvt) {
				vprintf(stdout, "Note: Doing Byte swapping\n");
				Bcvt = 1;
			}
		}
		if (checksum((int *)buf) == FAIL)
			return (FAIL);
		if (Bcvt) {
			swabst((u_char *)"8l4s31l", (u_char *)buf);
			swabst((u_char *)"l",(u_char *) &buf->c_level);
			swabst((u_char *)"2l",(u_char *) &buf->c_flags);
		}
		goto good;
	}
	readtape((char *)(&u_ospcl.s_ospcl));
	memset(buf, 0, (long)TP_BSIZE);
	buf->c_type = u_ospcl.s_ospcl.c_type;
	buf->c_date = u_ospcl.s_ospcl.c_date;
	buf->c_ddate = u_ospcl.s_ospcl.c_ddate;
	buf->c_volume = u_ospcl.s_ospcl.c_volume;
	buf->c_tapea = u_ospcl.s_ospcl.c_tapea;
	buf->c_inumber = u_ospcl.s_ospcl.c_inumber;
	buf->c_checksum = u_ospcl.s_ospcl.c_checksum;
	buf->c_magic = u_ospcl.s_ospcl.c_magic;
	buf->c_dinode.di_mode = u_ospcl.s_ospcl.c_dinode.odi_mode;
	buf->c_dinode.di_nlink = u_ospcl.s_ospcl.c_dinode.odi_nlink;
	buf->c_dinode.di_uid = u_ospcl.s_ospcl.c_dinode.odi_uid;
	buf->c_dinode.di_gid = u_ospcl.s_ospcl.c_dinode.odi_gid;
	buf->c_dinode.di_size = u_ospcl.s_ospcl.c_dinode.odi_size;
	buf->c_dinode.di_rdev = u_ospcl.s_ospcl.c_dinode.odi_rdev;
	buf->c_dinode.di_atime = u_ospcl.s_ospcl.c_dinode.odi_atime;
	buf->c_dinode.di_mtime = u_ospcl.s_ospcl.c_dinode.odi_mtime;
	buf->c_dinode.di_ctime = u_ospcl.s_ospcl.c_dinode.odi_ctime;
	buf->c_count = u_ospcl.s_ospcl.c_count;
	memmove(buf->c_addr, u_ospcl.s_ospcl.c_addr, (long)256);
	if (u_ospcl.s_ospcl.c_magic != OFS_MAGIC ||
	    checksum((int *)(&u_ospcl.s_ospcl)) == FAIL)
		return(FAIL);
	buf->c_magic = NFS_MAGIC;

good:
	if ((buf->c_dinode.di_size == 0 || buf->c_dinode.di_size > 0xfffffff) &&
	    (buf->c_dinode.di_mode & IFMT) == IFDIR && Qcvt == 0) {
		qcvt.qval = buf->c_dinode.di_size;
		if (qcvt.val[0] || qcvt.val[1]) {
			printf("Note: Doing Quad swapping\n");
			Qcvt = 1;
		}
	}
	if (Qcvt) {
		qcvt.qval = buf->c_dinode.di_size;
		i = qcvt.val[1];
		qcvt.val[1] = qcvt.val[0];
		qcvt.val[0] = i;
		buf->c_dinode.di_size = qcvt.qval;
	}
	readmapflag = 0;

	switch (buf->c_type) {

	case TS_CLRI:
	case TS_BITS:
		/*
		 * Have to patch up missing information in bit map headers
		 */
		buf->c_inumber = 0;
		buf->c_dinode.di_size = buf->c_count * TP_BSIZE;
		if (buf->c_count > TP_NINDIR)
			readmapflag = 1;
		else 
			for (i = 0; i < buf->c_count; i++)
				buf->c_addr[i]++;
		break;

	case TS_TAPE:
		if ((buf->c_flags & DR_NEWINODEFMT) == 0)
			oldinofmt = 1;
		/* fall through */
	case TS_END:
		buf->c_inumber = 0;
		break;

	case TS_INODE:
	case TS_ADDR:
		break;

	default:
		panic("gethead: unknown inode type %d\n", buf->c_type);
		break;
	}
	/*
	 * If we are restoring a filesystem with old format inodes,
	 * copy the uid/gid to the new location.
	 */
	if (oldinofmt) {
		buf->c_dinode.di_uid = buf->c_dinode.di_ouid;
		buf->c_dinode.di_gid = buf->c_dinode.di_ogid;
	}
	tapeaddr = buf->c_tapea;
	if (dflag)
		accthdr(buf);
	return(GOOD);
}

/*
 * Check that a header is where it belongs and predict the next header
 */
static void
accthdr(header)
	struct s_spcl *header;
{
	static ino_t previno = 0x7fffffff;
	static int prevtype;
	static long predict;
	long blks, i;

	if (header->c_type == TS_TAPE) {
		fprintf(stderr, "Volume header (%s inode format) ",
		    oldinofmt ? "old" : "new");
 		if (header->c_firstrec)
 			fprintf(stderr, "begins with record %ld",
 				header->c_firstrec);
 		fprintf(stderr, "\n");
		previno = 0x7fffffff;
		return;
	}
	if (previno == 0x7fffffff)
		goto newcalc;
	switch (prevtype) {
	case TS_BITS:
		fprintf(stderr, "Dumped inodes map header");
		break;
	case TS_CLRI:
		fprintf(stderr, "Used inodes map header");
		break;
	case TS_INODE:
		fprintf(stderr, "File header, ino %d", previno);
		break;
	case TS_ADDR:
		fprintf(stderr, "File continuation header, ino %d", previno);
		break;
	case TS_END:
		fprintf(stderr, "End of tape header");
		break;
	}
	if (predict != blksread - 1)
		fprintf(stderr, "; predicted %ld blocks, got %ld blocks",
			predict, blksread - 1);
	fprintf(stderr, "\n");
newcalc:
	blks = 0;
	if (header->c_type != TS_END)
		for (i = 0; i < header->c_count; i++)
			if (readmapflag || header->c_addr[i] != 0)
				blks++;
	predict = blks;
	blksread = 0;
	prevtype = header->c_type;
	previno = header->c_inumber;
}

/*
 * Find an inode header.
 * Complain if had to skip.
 */
static void
findinode(header)
	struct s_spcl *header;
{
	static long skipcnt = 0;
	long i;
	char buf[TP_BSIZE];
	int htype;

	curfile.name = "<name unknown>";
	curfile.action = UNKNOWN;
	curfile.dip = NULL;
	curfile.ino = 0;
	do {
		if (header->c_magic != NFS_MAGIC) {
			skipcnt++;
			while (gethead(header) == FAIL ||
			    header->c_date != dumpdate)
				skipcnt++;
		}
		htype = header->c_type;
		switch (htype) {

		case TS_ADDR:
			/*
			 * Skip up to the beginning of the next record
			 */
			for (i = 0; i < header->c_count; i++)
				if (header->c_addr[i])
					readtape(buf);
			while (gethead(header) == FAIL ||
			    header->c_date != dumpdate)
				skipcnt++;
			break;

		case TS_INODE:
			curfile.dip = &header->c_dinode;
			curfile.ino = header->c_inumber;
			break;

		case TS_END:
			/* If we missed some tapes, get another volume. */
			if (tapesread & (tapesread + 1)) {
				getvol(0);
				continue;
			}
			curfile.ino = maxino;
			break;

		case TS_CLRI:
			curfile.name = "<file removal list>";
			break;

		case TS_BITS:
			curfile.name = "<file dump list>";
			break;

		case TS_TAPE:
			panic("unexpected tape header\n");
			/* NOTREACHED */

		default:
			panic("unknown tape header type %d\n", spcl.c_type);
			/* NOTREACHED */

		}
	} while (htype == TS_ADDR);
	if (skipcnt > 0)
		fprintf(stderr, "resync restore, skipped %ld blocks\n",
		    skipcnt);
	skipcnt = 0;
}

static int
checksum(buf)
	register int *buf;
{
	register int i, j;

	j = sizeof(union u_spcl) / sizeof(int);
	i = 0;
	if(!Bcvt) {
		do
			i += *buf++;
		while (--j);
	} else {
		/* What happens if we want to read restore tapes
			for a 16bit int machine??? */
		do
			i += swabl(*buf++);
		while (--j);
	}

	if (i != CHECKSUM) {
		fprintf(stderr, "Checksum error %o, inode %d file %s\n", i,
			curfile.ino, curfile.name);
		return(FAIL);
	}
	return(GOOD);
}

#ifdef RRESTORE
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void
#if __STDC__
msg(const char *fmt, ...)
#else
msg(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
}
#endif /* RRESTORE */

static u_char *
swabshort(sp, n)
	register u_char *sp;
	register int n;
{
	char c;

	while (--n >= 0) {
		c = sp[0]; sp[0] = sp[1]; sp[1] = c;
		sp += 2;
	}
	return (sp);
}

static u_char *
swablong(sp, n)
	register u_char *sp;
	register int n;
{
	char c;

	while (--n >= 0) {
		c = sp[0]; sp[0] = sp[3]; sp[3] = c;
		c = sp[2]; sp[2] = sp[1]; sp[1] = c;
		sp += 4;
	}
	return (sp);
}

void
swabst(cp, sp)
	register u_char *cp, *sp;
{
	int n = 0;

	while (*cp) {
		switch (*cp) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			n = (n * 10) + (*cp++ - '0');
			continue;

		case 's': case 'w': case 'h':
			if (n == 0)
				n = 1;
			sp = swabshort(sp, n);
			break;

		case 'l':
			if (n == 0)
				n = 1;
			sp = swablong(sp, n);
			break;

		default: /* Any other character, like 'b' counts as byte. */
			if (n == 0)
				n = 1;
			sp += n;
			break;
		}
		cp++;
		n = 0;
	}
}

static u_long
swabl(x)
	u_long x;
{
	swabst((u_char *)"l", (u_char *)&x);
	return (x);
}
