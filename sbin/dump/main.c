/*-
 * Copyright (c) 1980, 1991, 1993, 1994
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
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1980, 1991, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)main.c	8.4 (Berkeley) 4/15/94";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#ifdef sunos
#include <sys/vnode.h>

#include <ufs/inode.h>
#include <ufs/fs.h>
#else
#include <ufs/ffs/fs.h>
#include <ufs/ufs/dinode.h>
#endif

#include <protocols/dumprestore.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <fstab.h>
#include <signal.h>
#include <stdio.h>
#ifdef __STDC__
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include "dump.h"
#include "pathnames.h"

#ifndef SBOFF
#define SBOFF (SBLOCK * DEV_BSIZE)
#endif

int	notify = 0;	/* notify operator flag */
int	blockswritten = 0;	/* number of blocks written on current tape */
int	tapeno = 0;	/* current tape number */
int	density = 0;	/* density in bytes/0.1" " <- this is for hilit19 */
int	ntrec = NTREC;	/* # tape blocks in each tape record */
int	cartridge = 0;	/* Assume non-cartridge tape */
long	dev_bsize = 1;	/* recalculated below */
long	blocksperfile;	/* output blocks per file */
char	*host = NULL;	/* remote host (if any) */

static long numarg __P((int, char *, long, long, int *, char ***));
static void missingarg __P((int, char *)) __dead2;

int
main(argc, argv)
	int argc;
	char **argv;
{
	register ino_t ino;
	register int dirty;
	register struct dinode *dp;
	register struct	fstab *dt;
	register char *map;
	register char *cp;
	int i, anydirskipped, bflag = 0, Tflag = 0, honorlevel = 1;
	ino_t maxino;

	spcl.c_date = 0;
	(void)time((time_t *)&spcl.c_date);

	tsize = 0;	/* Default later, based on 'c' option for cart tapes */
	if ((tape = getenv("TAPE")) == NULL)
		tape = _PATH_DEFTAPE;
	dumpdates = _PATH_DUMPDATES;
	temp = _PATH_DTMP;
	if (TP_BSIZE / DEV_BSIZE == 0 || TP_BSIZE % DEV_BSIZE != 0)
		quit("TP_BSIZE must be a multiple of DEV_BSIZE\n");
	level = '0';
	if (argc == 1) {
		(void) fprintf(stderr, "Must specify a key.\n");
		Exit(X_ABORT);
	}
	argv++;
	argc -= 2;
	for (cp = *argv++; cp != NULL && *cp != '\0'; cp++) {
		switch (*cp) {
		case '-':
			break;

		case 'w':
			lastdump('w');	/* tell us only what has to be done */
			exit(0);

		case 'W':		/* what to do */
			lastdump('W');	/* tell us state of what is done */
			exit(0);	/* do nothing else */

		case 'f':		/* output file */
			if (argc < 1)
				missingarg('f', "output file");
			tape = *argv++;
			argc--;
			break;

		case 'd':		/* density, in bits per inch */
			density = numarg('d', "density",
			    10L, 327670L, &argc, &argv) / 10;
			if (density >= 625 && !bflag)
				ntrec = HIGHDENSITYTREC;
			break;

		case 's':		/* tape size, feet */
			tsize = numarg('s', "size",
			    1L, 0L, &argc, &argv) * 12 * 10;
			break;

		case 'T':		/* time of last dump */
			if (argc < 1)
				missingarg('T', "time of last dump");
			spcl.c_ddate = unctime(*argv);
			if (spcl.c_ddate < 0) {
				(void)fprintf(stderr, "bad time \"%s\"\n",
				    *argv);
				exit(X_ABORT);
			}
			Tflag = 1;
			lastlevel = '?';
			argc--;
			argv++;
			break;

		case 'b':		/* blocks per tape write */
			ntrec = numarg('b', "number of blocks per write",
			    1L, 1000L, &argc, &argv);
			/* XXX restore is unable to restore dumps that 
			   were created  with a blocksize larger than 32K.
			   Possibly a bug in the scsi tape driver. */
			if ( ntrec > 32 ) {
				msg("please choose a blocksize <= 32\n");
				exit(X_ABORT);
			}
			break;

		case 'B':		/* blocks per output file */
			blocksperfile = numarg('B', "number of blocks per file",
			    1L, 0L, &argc, &argv);
			break;

		case 'c':		/* Tape is cart. not 9-track */
			cartridge = 1;
			break;

		/* dump level */
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			level = *cp;
			break;

		case 'u':		/* update /etc/dumpdates */
			uflag = 1;
			break;

		case 'n':		/* notify operators */
			notify = 1;
			break;

		case 'h':
			honorlevel = numarg('h', "honor level",
			    0L, 10L, &argc, &argv);
			break;

		default:
			(void)fprintf(stderr, "bad key '%c'\n", *cp);
			exit(X_ABORT);
		}
	}
	if (argc < 1) {
		(void)fprintf(stderr, "Must specify disk or filesystem\n");
		exit(X_ABORT);
	}
	disk = *argv++;
	argc--;
	if (argc >= 1) {
		(void)fprintf(stderr, "Unknown arguments to dump:");
		while (argc--)
			(void)fprintf(stderr, " %s", *argv++);
		(void)fprintf(stderr, "\n");
		exit(X_ABORT);
	}
	if (Tflag && uflag) {
	        (void)fprintf(stderr,
		    "You cannot use the T and u flags together.\n");
		exit(X_ABORT);
	}
	if (strcmp(tape, "-") == 0) {
		pipeout++;
		tape = "standard output";
	}

	if (blocksperfile)
		blocksperfile = blocksperfile / ntrec * ntrec; /* round down */
	else {
		/*
		 * Determine how to default tape size and density
		 *
		 *         	density				tape size
		 * 9-track	1600 bpi (160 bytes/.1")	2300 ft.
		 * 9-track	6250 bpi (625 bytes/.1")	2300 ft.
		 * cartridge	8000 bpi (100 bytes/.1")	1700 ft.
		 *						(450*4 - slop)
		 * hilit19 hits again: "
		 */
		if (density == 0)
			density = cartridge ? 100 : 160;
		if (tsize == 0)
			tsize = cartridge ? 1700L*120L : 2300L*120L;
	}

	if (index(tape, ':')) {
		host = tape;
		tape = index(host, ':');
		*tape++ = '\0';
#ifdef RDUMP
		if (index(tape, "\n") {
		    (void)fprintf(stderr, "invalid characters in tape\n");
		    exit(X_ABORT);
		}
		if (rmthost(host) == 0)
			exit(X_ABORT);
#else
		(void)fprintf(stderr, "remote dump not enabled\n");
		exit(X_ABORT);
#endif
	}
	(void)setuid(getuid()); /* rmthost() is the only reason to be setuid */

	if (signal(SIGHUP, SIG_IGN) != SIG_IGN)
		signal(SIGHUP, sig);
	if (signal(SIGTRAP, SIG_IGN) != SIG_IGN)
		signal(SIGTRAP, sig);
	if (signal(SIGFPE, SIG_IGN) != SIG_IGN)
		signal(SIGFPE, sig);
	if (signal(SIGBUS, SIG_IGN) != SIG_IGN)
		signal(SIGBUS, sig);
	if (signal(SIGSEGV, SIG_IGN) != SIG_IGN)
		signal(SIGSEGV, sig);
	if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
		signal(SIGTERM, sig);
	if (signal(SIGINT, interrupt) == SIG_IGN)
		signal(SIGINT, SIG_IGN);

	set_operators();	/* /etc/group snarfed */
	getfstab();		/* /etc/fstab snarfed */
	/*
	 *	disk can be either the full special file name,
	 *	the suffix of the special file name,
	 *	the special name missing the leading '/',
	 *	the file system name with or without the leading '/'.
	 */
	dt = fstabsearch(disk);
	if (dt != NULL) {
		disk = rawname(dt->fs_spec);
		(void)strncpy(spcl.c_dev, dt->fs_spec, NAMELEN);
		(void)strncpy(spcl.c_filesys, dt->fs_file, NAMELEN);
	} else {
		(void)strncpy(spcl.c_dev, disk, NAMELEN);
		(void)strncpy(spcl.c_filesys, "an unlisted file system",
		    NAMELEN);
	}
	spcl.c_dev[NAMELEN-1]='\0';
	spcl.c_filesys[NAMELEN-1]='\0';
	(void)strcpy(spcl.c_label, "none");
	(void)gethostname(spcl.c_host, NAMELEN);
	spcl.c_level = level - '0';
	spcl.c_type = TS_TAPE;
	if (!Tflag)
	        getdumptime();		/* /etc/dumpdates snarfed */

	msg("Date of this level %c dump: %s", level,
		spcl.c_date == 0 ? "the epoch\n" : ctime(&spcl.c_date));
 	msg("Date of last level %c dump: %s", lastlevel,
		spcl.c_ddate == 0 ? "the epoch\n" : ctime(&spcl.c_ddate));
	msg("Dumping %s ", disk);
	if (dt != NULL)
		msgtail("(%s) ", dt->fs_file);
	if (host)
		msgtail("to %s on host %s\n", tape, host);
	else
		msgtail("to %s\n", tape);

	if ((diskfd = open(disk, O_RDONLY)) < 0) {
		msg("Cannot open %s\n", disk);
		exit(X_ABORT);
	}
	sync();
	sblock = (struct fs *)sblock_buf;
	bread(SBOFF, (char *) sblock, SBSIZE);
	if (sblock->fs_magic != FS_MAGIC)
		quit("bad sblock magic number\n");
	dev_bsize = sblock->fs_fsize / fsbtodb(sblock, 1);
	dev_bshift = ffs(dev_bsize) - 1;
	if (dev_bsize != (1 << dev_bshift))
		quit("dev_bsize (%d) is not a power of 2", dev_bsize);
	tp_bshift = ffs(TP_BSIZE) - 1;
	if (TP_BSIZE != (1 << tp_bshift))
		quit("TP_BSIZE (%d) is not a power of 2", TP_BSIZE);
#ifdef FS_44INODEFMT
	if (sblock->fs_inodefmt >= FS_44INODEFMT)
		spcl.c_flags |= DR_NEWINODEFMT;
#endif
	maxino = sblock->fs_ipg * sblock->fs_ncg;
	mapsize = roundup(howmany(maxino, NBBY), TP_BSIZE);
	usedinomap = (char *)calloc((unsigned) mapsize, sizeof(char));
	dumpdirmap = (char *)calloc((unsigned) mapsize, sizeof(char));
	dumpinomap = (char *)calloc((unsigned) mapsize, sizeof(char));
	tapesize = 3 * (howmany(mapsize * sizeof(char), TP_BSIZE) + 1);

	nonodump = spcl.c_level < honorlevel;

	msg("mapping (Pass I) [regular files]\n");
	anydirskipped = mapfiles(maxino, &tapesize);

	msg("mapping (Pass II) [directories]\n");
	while (anydirskipped) {
		anydirskipped = mapdirs(maxino, &tapesize);
	}

	if (pipeout) {
		tapesize += 10;	/* 10 trailer blocks */
		msg("estimated %ld tape blocks.\n", tapesize);
	} else {
		double fetapes;

		if (blocksperfile)
			fetapes = (double) tapesize / blocksperfile;
		else if (cartridge) {
			/* Estimate number of tapes, assuming streaming stops at
			   the end of each block written, and not in mid-block.
			   Assume no erroneous blocks; this can be compensated
			   for with an artificially low tape size. */
			fetapes =
			(	  tapesize	/* blocks */
				* TP_BSIZE	/* bytes/block */
				* (1.0/density)	/* 0.1" / byte " */
			  +
				  tapesize	/* blocks */
				* (1.0/ntrec)	/* streaming-stops per block */
				* 15.48		/* 0.1" / streaming-stop " */
			) * (1.0 / tsize );	/* tape / 0.1" " */
		} else {
			/* Estimate number of tapes, for old fashioned 9-track
			   tape */
			int tenthsperirg = (density == 625) ? 3 : 7;
			fetapes =
			(	  tapesize	/* blocks */
				* TP_BSIZE	/* bytes / block */
				* (1.0/density)	/* 0.1" / byte " */
			  +
				  tapesize	/* blocks */
				* (1.0/ntrec)	/* IRG's / block */
				* tenthsperirg	/* 0.1" / IRG " */
			) * (1.0 / tsize );	/* tape / 0.1" " */
		}
		etapes = fetapes;		/* truncating assignment */
		etapes++;
		/* count the dumped inodes map on each additional tape */
		tapesize += (etapes - 1) *
			(howmany(mapsize * sizeof(char), TP_BSIZE) + 1);
		tapesize += etapes + 10;	/* headers + 10 trailer blks */
		msg("estimated %ld tape blocks on %3.2f tape(s).\n",
		    tapesize, fetapes);
	}

	/*
	 * Allocate tape buffer.
	 */
	if (!alloctape())
		quit("can't allocate tape buffers - try a smaller blocking factor.\n");

	startnewtape(1);
	(void)time((time_t *)&(tstart_writing));
	dumpmap(usedinomap, TS_CLRI, maxino - 1);

	msg("dumping (Pass III) [directories]\n");
	dirty = 0;		/* XXX just to get gcc to shut up */
	for (map = dumpdirmap, ino = 1; ino < maxino; ino++) {
		if (((ino - 1) % NBBY) == 0)	/* map is offset by 1 */
			dirty = *map++;
		else
			dirty >>= 1;
		if ((dirty & 1) == 0)
			continue;
		/*
		 * Skip directory inodes deleted and maybe reallocated
		 */
		dp = getino(ino);
		if ((dp->di_mode & IFMT) != IFDIR)
			continue;
		(void)dumpino(dp, ino);
	}

	msg("dumping (Pass IV) [regular files]\n");
	for (map = dumpinomap, ino = 1; ino < maxino; ino++) {
		int mode;

		if (((ino - 1) % NBBY) == 0)	/* map is offset by 1 */
			dirty = *map++;
		else
			dirty >>= 1;
		if ((dirty & 1) == 0)
			continue;
		/*
		 * Skip inodes deleted and reallocated as directories.
		 */
		dp = getino(ino);
		mode = dp->di_mode & IFMT;
		if (mode == IFDIR)
			continue;
		(void)dumpino(dp, ino);
	}

	(void)time((time_t *)&(tend_writing));
	spcl.c_type = TS_END;
	for (i = 0; i < ntrec; i++)
		writeheader(maxino - 1);
	if (pipeout)
		msg("DUMP: %ld tape blocks\n", spcl.c_tapea);
	else
		msg("DUMP: %ld tape blocks on %d volumes(s)\n",
		    spcl.c_tapea, spcl.c_volume);

	/* report dump performance, avoid division through zero */
	if (tend_writing - tstart_writing == 0)
		msg("finished in less than a second\n");
	else
		msg("finished in %d seconds, throughput %d KBytes/sec\n",
		    tend_writing - tstart_writing,
		    spcl.c_tapea / (tend_writing - tstart_writing));

	putdumptime();
	trewind();
	broadcast("DUMP IS DONE!\7\7\n");
	msg("DUMP IS DONE\n");
	Exit(X_FINOK);
	/* NOTREACHED */
}

/*
 * Pick up a numeric argument.  It must be nonnegative and in the given
 * range (except that a vmax of 0 means unlimited).
 */
static long
numarg(letter, meaning, vmin, vmax, pargc, pargv)
	int letter;
	char *meaning;
	long vmin, vmax;
	int *pargc;
	char ***pargv;
{
	register char *p;
	long val;
	char *str;

	if (--*pargc < 0)
		missingarg(letter, meaning);
	str = *(*pargv)++;
	for (p = str; *p; p++)
		if (!isdigit(*p))
			goto bad;
	val = atol(str);
	if (val < vmin || (vmax && val > vmax))
		goto bad;
	return (val);

bad:
	(void)fprintf(stderr, "bad '%c' (%s) value \"%s\"\n",
	    letter, meaning, str);
	exit(X_ABORT);
}

static void
missingarg(letter, meaning)
	int letter;
	char *meaning;
{

	(void)fprintf(stderr, "The '%c' flag (%s) requires an argument\n",
	    letter, meaning);
	exit(X_ABORT);
}

void
sig(signo)
	int signo;
{
	switch(signo) {
	case SIGALRM:
	case SIGBUS:
	case SIGFPE:
	case SIGHUP:
	case SIGTERM:
	case SIGTRAP:
		if (pipeout)
			quit("Signal on pipe: cannot recover\n");
		msg("Rewriting attempted as response to unknown signal.\n");
		(void)fflush(stderr);
		(void)fflush(stdout);
		close_rewind();
		exit(X_REWRITE);
		/* NOTREACHED */
	case SIGSEGV:
		msg("SIGSEGV: ABORTING!\n");
		(void)signal(SIGSEGV, SIG_DFL);
		(void)kill(0, SIGSEGV);
		/* NOTREACHED */
	}
}

char *
rawname(cp)
	char *cp;
{
	static char rawbuf[MAXPATHLEN];
	char *dp = rindex(cp, '/');

	if (dp == NULL)
		return (NULL);
	*dp = '\0';
	(void)strncpy(rawbuf, cp, MAXPATHLEN - 1);
	rawbuf[MAXPATHLEN-1] = '\0';
	*dp = '/';
	(void)strncat(rawbuf, "/r", MAXPATHLEN - 1 - strlen(rawbuf));
	(void)strncat(rawbuf, dp + 1, MAXPATHLEN - 1 - strlen(rawbuf));
	return (rawbuf);
}

#ifdef sunos
const char *
strerror(errnum)
	int errnum;
{
	extern int sys_nerr;
	extern const char *const sys_errlist[];

	if (errnum < sys_nerr)
		return (sys_errlist[errnum]);
	else
		return ("bogus errno in strerror");
}
#endif
