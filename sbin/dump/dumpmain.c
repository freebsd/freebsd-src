/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
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
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1980, 1991 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)dumpmain.c	5.16 (Berkeley) 4/24/91";
static char rcsid[] = "$Header: /a/cvs/386BSD/src/sbin/dump/dumpmain.c,v 1.3 1993/09/13 21:25:11 rgrimes Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <ufs/dir.h>
#include <ufs/dinode.h>
#include <ufs/fs.h>
#include <protocols/dumprestore.h>
#include <signal.h>
#ifdef __STDC__
#include <time.h>
#endif
#include <fcntl.h>
#include <fstab.h>
#include <stdio.h>
#ifdef __STDC__
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#endif
#include "dump.h"
#include "pathnames.h"

int	notify = 0;	/* notify operator flag */
int	blockswritten = 0;	/* number of blocks written on current tape */
int	tapeno = 0;	/* current tape number */
int	density = 0;	/* density in bytes/0.1" */
int	ntrec = NTREC;	/* # tape blocks in each tape record */
int	cartridge = 0;	/* Assume non-cartridge tape */
long	dev_bsize = 1;	/* recalculated below */
long	blocksperfile;	/* output blocks per file */
#ifdef RDUMP
char	*host;
char	*user;
int	rmthost();
#endif

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
	int i, anydirskipped, bflag = 0;
	float fetapes;
	ino_t maxino;

	time(&(spcl.c_date));

	tsize = 0;	/* Default later, based on 'c' option for cart tapes */
	tape = _PATH_DEFTAPE;
	dumpdates = _PATH_DUMPDATES;
	temp = _PATH_DTMP;
	if (TP_BSIZE / DEV_BSIZE == 0 || TP_BSIZE % DEV_BSIZE != 0)
		quit("TP_BSIZE must be a multiple of DEV_BSIZE\n");
	level = '0';
	argv++;
	argc -= 2;
	for (cp = *argv++; cp && *cp; cp++) {
		switch (*cp) {
		case '-':
			continue;

		case 'w':
			lastdump('w');	/* tell us only what has to be done */
			exit(0);

		case 'W':		/* what to do */
			lastdump('W');	/* tell us state of what is done */
			exit(0);	/* do nothing else */

		case 'f':		/* output file */
			if (argc < 1)
				break;
			tape = *argv++;
			argc--;
			continue;

		case 'd':		/* density, in bits per inch */
			if (argc < 1)
				break;
			density = atoi(*argv) / 10;
			if (density < 1) {
				fprintf(stderr, "bad density \"%s\"\n", *argv);
				Exit(X_ABORT);
			}
			argc--;
			argv++;
			if (density >= 625 && !bflag)
				ntrec = HIGHDENSITYTREC;
			continue;

		case 's':		/* tape size, feet */
			if (argc < 1)
				break;
			tsize = atol(*argv);
			if (tsize < 1) {
				fprintf(stderr, "bad size \"%s\"\n", *argv);
				Exit(X_ABORT);
			}
			argc--;
			argv++;
			tsize *= 12 * 10;
			continue;

		case 'b':		/* blocks per tape write */
			if (argc < 1)
				break;
			bflag++;
			ntrec = atoi(*argv);
			if (ntrec < 1) {
				fprintf(stderr, "%s \"%s\"\n",
				    "bad number of blocks per write ", *argv);
				Exit(X_ABORT);
			}
			argc--;
			argv++;
			continue;

		case 'B':		/* blocks per output file */
			if (argc < 1)
				break;
			blocksperfile = atol(*argv);
			if (blocksperfile < 1) {
				fprintf(stderr, "%s \"%s\"\n",
				    "bad number of blocks per file ", *argv);
				Exit(X_ABORT);
			}
			argc--;
			argv++;
			continue;

		case 'c':		/* Tape is cart. not 9-track */
			cartridge++;
			continue;

		case '0':		/* dump level */
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			level = *cp;
			continue;

		case 'u':		/* update /etc/dumpdates */
			uflag++;
			continue;

		case 'n':		/* notify operators */
			notify++;
			continue;

		default:
			fprintf(stderr, "bad key '%c'\n", *cp);
			Exit(X_ABORT);
		}
		fprintf(stderr, "missing argument to '%c'\n", *cp);
		Exit(X_ABORT);
	}
	if (argc < 1) {
		fprintf(stderr, "Must specify disk or filesystem\n");
		Exit(X_ABORT);
	} else {
		disk = *argv++;
		argc--;
	}
	if (argc >= 1) {
		fprintf(stderr, "Unknown arguments to dump:");
		while (argc--)
			fprintf(stderr, " %s", *argv++);
		fprintf(stderr, "\n");
		Exit(X_ABORT);
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
		 */
		if (density == 0)
			density = cartridge ? 100 : 160;
		if (tsize == 0)
			tsize = cartridge ? 1700L*120L : 2300L*120L;
	}

#ifdef RDUMP
	{ char *index();
	  user = tape;
	  host = index(user, '@');
	  if(host == 0) {
		user = NULL;
	  host = tape;
	  }
	  else
	  	*host++ = 0;
	  tape = index(host, ':');
	  if (tape == 0) {
		msg("need keyletter ``f'' and device ``host:tape''\n");
		exit(1);
	  }
	  *tape++ = 0;
	  if (rmthost(host,user) == 0)
		exit(X_ABORT);
	}
	setuid(getuid());	/* rmthost() is the only reason to be setuid */
#endif
	if (signal(SIGHUP, SIG_IGN) != SIG_IGN)
		signal(SIGHUP, sighup);
	if (signal(SIGTRAP, SIG_IGN) != SIG_IGN)
		signal(SIGTRAP, sigtrap);
	if (signal(SIGFPE, SIG_IGN) != SIG_IGN)
		signal(SIGFPE, sigfpe);
	if (signal(SIGBUS, SIG_IGN) != SIG_IGN)
		signal(SIGBUS, sigbus);
	if (signal(SIGSEGV, SIG_IGN) != SIG_IGN)
		signal(SIGSEGV, sigsegv);
	if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
		signal(SIGTERM, sigterm);
	

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
	if (dt != 0) {
		disk = rawname(dt->fs_spec);
		strncpy(spcl.c_dev, dt->fs_spec, NAMELEN);
		strncpy(spcl.c_filesys, dt->fs_file, NAMELEN);
	} else {
		strncpy(spcl.c_dev, disk, NAMELEN);
		strncpy(spcl.c_filesys, "an unlisted file system", NAMELEN);
	}
	strcpy(spcl.c_label, "none");
	gethostname(spcl.c_host, NAMELEN);
	spcl.c_level = level - '0';
	spcl.c_type = TS_TAPE;
	getdumptime();		/* /etc/dumpdates snarfed */

	msg("Date of this level %c dump: %s", level,
		spcl.c_date == 0 ? "the epoch\n" : ctime(&spcl.c_date));
 	msg("Date of last level %c dump: %s", lastlevel,
		spcl.c_ddate == 0 ? "the epoch\n" : ctime(&spcl.c_ddate));
	msg("Dumping %s ", disk);
	if (dt != 0)
		msgtail("(%s) ", dt->fs_file);
#ifdef RDUMP
	msgtail("to %s on host %s\n", tape, host);
#else
	msgtail("to %s\n", tape);
#endif

	if ((diskfd = open(disk, O_RDONLY)) < 0) {
		msg("Cannot open %s\n", disk);
		Exit(X_ABORT);
	}
	sync();
	sblock = (struct fs *)buf;
	bread(SBOFF, sblock, SBSIZE);
	if (sblock->fs_magic != FS_MAGIC)
		quit("bad sblock magic number\n");
	dev_bsize = sblock->fs_fsize / fsbtodb(sblock, 1);
	dev_bshift = ffs(dev_bsize) - 1;
	if (dev_bsize != (1 << dev_bshift))
		quit("dev_bsize (%d) is not a power of 2", dev_bsize);
	tp_bshift = ffs(TP_BSIZE) - 1;
	if (TP_BSIZE != (1 << tp_bshift))
		quit("TP_BSIZE (%d) is not a power of 2", TP_BSIZE);
	maxino = sblock->fs_ipg * sblock->fs_ncg - 1;
	mapsize = roundup(howmany(sblock->fs_ipg * sblock->fs_ncg, NBBY),
		TP_BSIZE);
	usedinomap = (char *)calloc(mapsize, sizeof(char));
	dumpdirmap = (char *)calloc(mapsize, sizeof(char));
	dumpinomap = (char *)calloc(mapsize, sizeof(char));
	tapesize = 3 * (howmany(mapsize * sizeof(char), TP_BSIZE) + 1);

	msg("mapping (Pass I) [regular files]\n");
	anydirskipped = mapfiles(maxino, &tapesize);

	msg("mapping (Pass II) [directories]\n");
	while (anydirskipped) {
		anydirskipped = mapdirs(maxino, &tapesize);
	}

	if (pipeout)
		tapesize += 10;	/* 10 trailer blocks */
	else {
		if (blocksperfile)
			fetapes = (float) tapesize / blocksperfile;
		else if (cartridge) {
			/* Estimate number of tapes, assuming streaming stops at
			   the end of each block written, and not in mid-block.
			   Assume no erroneous blocks; this can be compensated
			   for with an artificially low tape size. */
			fetapes = 
			(	  tapesize	/* blocks */
				* TP_BSIZE	/* bytes/block */
				* (1.0/density)	/* 0.1" / byte */
			  +
				  tapesize	/* blocks */
				* (1.0/ntrec)	/* streaming-stops per block */
				* 15.48		/* 0.1" / streaming-stop */
			) * (1.0 / tsize );	/* tape / 0.1" */
		} else {
			/* Estimate number of tapes, for old fashioned 9-track
			   tape */
			int tenthsperirg = (density == 625) ? 3 : 7;
			fetapes =
			(	  tapesize	/* blocks */
				* TP_BSIZE	/* bytes / block */
				* (1.0/density)	/* 0.1" / byte */
			  +
				  tapesize	/* blocks */
				* (1.0/ntrec)	/* IRG's / block */
				* tenthsperirg	/* 0.1" / IRG */
			) * (1.0 / tsize );	/* tape / 0.1" */
		}
		etapes = fetapes;		/* truncating assignment */
		etapes++;
		/* count the dumped inodes map on each additional tape */
		tapesize += (etapes - 1) *
			(howmany(mapsize * sizeof(char), TP_BSIZE) + 1);
		tapesize += etapes + 10;	/* headers + 10 trailer blks */
	}
	if (pipeout)
		msg("estimated %ld tape blocks.\n", tapesize);
	else
		msg("estimated %ld tape blocks on %3.2f tape(s).\n",
		    tapesize, fetapes);

	alloctape();			/* Allocate tape buffer */

	startnewtape();
	time(&(tstart_writing));
	dumpmap(usedinomap, TS_CLRI, maxino);

	msg("dumping (Pass III) [directories]\n");
	for (map = dumpdirmap, ino = 0; ino < maxino; ) {
		if ((ino % NBBY) == 0)
			dirty = *map++;
		else
			dirty >>= 1;
		ino++;
		if ((dirty & 1) == 0)
			continue;
		/*
		 * Skip directory inodes deleted and maybe reallocated
		 */
		dp = getino(ino);
		if ((dp->di_mode & IFMT) != IFDIR)
			continue;
		dumpino(dp, ino);
	}

	msg("dumping (Pass IV) [regular files]\n");
	for (map = dumpinomap, ino = 0; ino < maxino; ) {
		if ((ino % NBBY) == 0)
			dirty = *map++;
		else
			dirty >>= 1;
		ino++;
		if ((dirty & 1) == 0)
			continue;
		/*
		 * Skip inodes deleted and reallocated as directories.
		 */
		dp = getino(ino);
		if ((dp->di_mode & IFMT) == IFDIR)
			continue;
		dumpino(dp, ino);
	}

	spcl.c_type = TS_END;
#ifndef RDUMP
	for (i = 0; i < ntrec; i++)
		writeheader(maxino);
#endif
	if (pipeout)
		msg("DUMP: %ld tape blocks\n",spcl.c_tapea);
	else
		msg("DUMP: %ld tape blocks on %d volumes(s)\n",
		    spcl.c_tapea, spcl.c_volume);
	msg("DUMP IS DONE\n");

	putdumptime();
#ifndef RDUMP
	if (!pipeout) {
		close(tapefd);
		trewind();
	}
#else
	for (i = 0; i < ntrec; i++)
		writeheader(curino);
	trewind();
#endif
	broadcast("DUMP IS DONE!\7\7\n");
	Exit(X_FINOK);
	/* NOTREACHED */
}

void
sigAbort()
{
	if (pipeout)
		quit("Unknown signal, cannot recover\n");
	msg("Rewriting attempted as response to unknown signal.\n");
	fflush(stderr);
	fflush(stdout);
	close_rewind();
	exit(X_REWRITE);
}

void	sighup(){	msg("SIGHUP()  try rewriting\n"); sigAbort();}
void	sigtrap(){	msg("SIGTRAP()  try rewriting\n"); sigAbort();}
void	sigfpe(){	msg("SIGFPE()  try rewriting\n"); sigAbort();}
void	sigbus(){	msg("SIGBUS()  try rewriting\n"); sigAbort();}
void	sigsegv(){	msg("SIGSEGV()  ABORTING!\n"); abort();}
void	sigalrm(){	msg("SIGALRM()  try rewriting\n"); sigAbort();}
void	sigterm(){	msg("SIGTERM()  try rewriting\n"); sigAbort();}

char *
rawname(cp)
	char *cp;
{
	static char rawbuf[32];
	char *rindex();
	char *dp = rindex(cp, '/');

	if (dp == 0)
		return (0);
	*dp = 0;
	strcpy(rawbuf, cp);
	*dp = '/';
	strcat(rawbuf, "/r");
	strcat(rawbuf, dp+1);
	return (rawbuf);
}
