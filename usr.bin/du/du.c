/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Newcomb.
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
static const char copyright[] =
"@(#) Copyright (c) 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static const char sccsid[] = "@(#)du.c	8.5 (Berkeley) 5/4/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */


#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fnmatch.h>
#include <fts.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define	KILO_SZ(n) (n)
#define	MEGA_SZ(n) ((n) * (n))
#define	GIGA_SZ(n) ((n) * (n) * (n))
#define	TERA_SZ(n) ((n) * (n) * (n) * (n))
#define	PETA_SZ(n) ((n) * (n) * (n) * (n) * (n))

#define	KILO_2_SZ (KILO_SZ(1024ULL))
#define	MEGA_2_SZ (MEGA_SZ(1024ULL))
#define	GIGA_2_SZ (GIGA_SZ(1024ULL))
#define	TERA_2_SZ (TERA_SZ(1024ULL))
#define	PETA_2_SZ (PETA_SZ(1024ULL))

#define	KILO_SI_SZ (KILO_SZ(1000ULL))
#define	MEGA_SI_SZ (MEGA_SZ(1000ULL))
#define	GIGA_SI_SZ (GIGA_SZ(1000ULL))
#define	TERA_SI_SZ (TERA_SZ(1000ULL))
#define	PETA_SI_SZ (PETA_SZ(1000ULL))

unsigned long long vals_si [] = {1, KILO_SI_SZ, MEGA_SI_SZ, GIGA_SI_SZ, TERA_SI_SZ, PETA_SI_SZ};
unsigned long long vals_base2[] = {1, KILO_2_SZ, MEGA_2_SZ, GIGA_2_SZ, TERA_2_SZ, PETA_2_SZ};
unsigned long long *valp;

typedef enum { NONE, KILO, MEGA, GIGA, TERA, PETA, UNIT_MAX } unit_t;

int unitp [] = { NONE, KILO, MEGA, GIGA, TERA, PETA };

SLIST_HEAD(ignhead, ignentry) ignores;
struct ignentry {
	char			*mask;
	SLIST_ENTRY(ignentry)	next;
};

int		linkchk __P((FTSENT *));
static void	usage __P((void));
void		prthumanval __P((double));
unit_t		unit_adjust __P((double *));
void		ignoreadd __P((const char *));
void		ignoreclean __P((void));
int		ignorep __P((FTSENT *));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	FTS		*fts;
	FTSENT		*p;
	long		blocksize, savednumber = 0;
	int		ftsoptions;
	int		listall;
	int		depth;
	int		Hflag, Lflag, Pflag, aflag, sflag, dflag, cflag, hflag, ch, notused, rval;
	char 		**save;

	Hflag = Lflag = Pflag = aflag = sflag = dflag = cflag = hflag = 0;
	
	save = argv;
	ftsoptions = 0;
	depth = INT_MAX;
	SLIST_INIT(&ignores);
	
	while ((ch = getopt(argc, argv, "HI:LPasd:chkrx")) != -1)
		switch (ch) {
			case 'H':
				Hflag = 1;
				break;
			case 'I':
				ignoreadd(optarg);
				break;
			case 'L':
				if (Pflag)
					usage();
				Lflag = 1;
				break;
			case 'P':
				if (Lflag)
					usage();
				Pflag = 1;
				break;
			case 'a':
				aflag = 1;
				break;
			case 's':
				sflag = 1;
				break;
			case 'd':
				dflag = 1;
				errno = 0;
				depth = atoi(optarg);
				if (errno == ERANGE || depth < 0) {
					warnx("invalid argument to option d: %s", optarg);
					usage();
				}
				break;
			case 'c':
				cflag = 1;
				break;
			case 'h':
				putenv("BLOCKSIZE=512");
				hflag = 1;
				valp = vals_base2;
				break;
			case 'k':
				putenv("BLOCKSIZE=1024");
				break;
			case 'r':		 /* Compatibility. */
				break;
			case 'x':
				ftsoptions |= FTS_XDEV;
				break;
			case '?':
			default:
				usage();
		}

	argc -= optind;
	argv += optind;

	/*
	 * XXX
	 * Because of the way that fts(3) works, logical walks will not count
	 * the blocks actually used by symbolic links.  We rationalize this by
	 * noting that users computing logical sizes are likely to do logical
	 * copies, so not counting the links is correct.  The real reason is
	 * that we'd have to re-implement the kernel's symbolic link traversing
	 * algorithm to get this right.  If, for example, you have relative
	 * symbolic links referencing other relative symbolic links, it gets
	 * very nasty, very fast.  The bottom line is that it's documented in
	 * the man page, so it's a feature.
	 */

	if (Hflag + Lflag + Pflag > 1)
		usage();

	if (Hflag + Lflag + Pflag == 0)
		Pflag = 1;			/* -P (physical) is default */

	if (Hflag)
		ftsoptions |= FTS_COMFOLLOW;

	if (Lflag)
		ftsoptions |= FTS_LOGICAL;

	if (Pflag)
		ftsoptions |= FTS_PHYSICAL;

	listall = 0;

	if (aflag) {
		if (sflag || dflag)
			usage();
		listall = 1;
	} else if (sflag) {
		if (dflag)
			usage();
		depth = 0;
	}

	if (!*argv) {
		argv = save;
		argv[0] = ".";
		argv[1] = NULL;
	}

	(void) getbsize(&notused, &blocksize);
	blocksize /= 512;

	rval = 0;
	
	if ((fts = fts_open(argv, ftsoptions, NULL)) == NULL)
		err(1, "fts_open");

	while ((p = fts_read(fts)) != NULL) {
		switch (p->fts_info) {
			case FTS_D:			/* Ignore. */
				if (ignorep(p))
					fts_set(fts, p, FTS_SKIP);
				break;
			case FTS_DP:
				if (ignorep(p))
					break;

				p->fts_parent->fts_number +=
				    p->fts_number += p->fts_statp->st_blocks;
				
				if (p->fts_level <= depth) {
					if (hflag) {
						(void) prthumanval(howmany(p->fts_number, blocksize));
						(void) printf("\t%s\n", p->fts_path);
					} else {
					(void) printf("%ld\t%s\n",
					    howmany(p->fts_number, blocksize),
					    p->fts_path);
					}
				}
				break;
			case FTS_DC:			/* Ignore. */
				break;
			case FTS_DNR:			/* Warn, continue. */
			case FTS_ERR:
			case FTS_NS:
				warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
				rval = 1;
				break;
			default:
				if (ignorep(p))
					break;

				if (p->fts_statp->st_nlink > 1 && linkchk(p))
					break;
				
				if (listall || p->fts_level == 0) {
					if (hflag) {
						(void) prthumanval(howmany(p->fts_statp->st_blocks,
							blocksize));
						(void) printf("\t%s\n", p->fts_path);
					} else {
						(void) printf("%qd\t%s\n",
							howmany(p->fts_statp->st_blocks, blocksize),
							p->fts_path);
					}
				}

				p->fts_parent->fts_number += p->fts_statp->st_blocks;
		}
		savednumber = p->fts_parent->fts_number;
	}

	if (errno)
		err(1, "fts_read");

	if (cflag) {
		if (hflag) {
			(void) prthumanval(howmany(savednumber, blocksize));
			(void) printf("\ttotal\n");
		} else {
			(void) printf("%ld\ttotal\n", howmany(savednumber, blocksize));
		}
	}

	ignoreclean();
	exit(rval);
}


typedef struct _ID {
	dev_t	dev;
	ino_t	inode;
} ID;


int
linkchk(p)
	FTSENT *p;
{
	static ID *files;
	static int maxfiles, nfiles;
	ID *fp, *start;
	ino_t ino;
	dev_t dev;

	ino = p->fts_statp->st_ino;
	dev = p->fts_statp->st_dev;
	if ((start = files) != NULL)
		for (fp = start + nfiles - 1; fp >= start; --fp)
			if (ino == fp->inode && dev == fp->dev)
				return (1);

	if (nfiles == maxfiles && (files = realloc((char *)files,
	    (u_int)(sizeof(ID) * (maxfiles += 128)))) == NULL)
		errx(1, "can't allocate memory");
	files[nfiles].inode = ino;
	files[nfiles].dev = dev;
	++nfiles;
	return (0);
}

/*
 * Output in "human-readable" format.  Uses 3 digits max and puts
 * unit suffixes at the end.  Makes output compact and easy to read,
 * especially on huge disks.
 *
 */
unit_t
unit_adjust(val)
	double *val;
{
	double abval;
	unit_t unit;
	unsigned int unit_sz;

	abval = fabs(*val);

	unit_sz = abval ? ilogb(abval) / 10 : 0;

	if (unit_sz >= UNIT_MAX) {
		unit = NONE;
	} else {
		unit = unitp[unit_sz];
		*val /= (double)valp[unit_sz];
	}

	return (unit);
}

void
prthumanval(bytes)
	double bytes;
{
	unit_t unit;

	bytes *= 512;
	unit = unit_adjust(&bytes);

	if (bytes == 0)
		(void)printf("  0B");
	else if (bytes > 10)
		(void)printf("%3.0f%c", bytes, "BKMGTPE"[unit]);
	else
		(void)printf("%3.1f%c", bytes, "BKMGTPE"[unit]);
}

static void
usage()
{
	(void)fprintf(stderr,
		"usage: du [-H | -L | -P] [-a | -s | -d depth] [-c] [-h | -k] [-x] [-I mask] [file ...]\n");
	exit(EX_USAGE);
}

void
ignoreadd(mask)
	const char *mask;
{
	struct ignentry *ign;

	ign = calloc(1, sizeof(*ign));
	if (ign == NULL)
		errx(1, "cannot allocate memory");
	ign->mask = strdup(mask);
	if (ign->mask == NULL)
		errx(1, "cannot allocate memory");
	SLIST_INSERT_HEAD(&ignores, ign, next);
}

void
ignoreclean()
{
	struct ignentry *ign;
	
	while (!SLIST_EMPTY(&ignores)) {
		ign = SLIST_FIRST(&ignores);
		SLIST_REMOVE_HEAD(&ignores, next);
		free(ign->mask);
		free(ign);
	}
}

int
ignorep(ent)
	FTSENT *ent;
{
	struct ignentry *ign;

	SLIST_FOREACH(ign, &ignores, next)
		if (fnmatch(ign->mask, ent->fts_name, 0) != FNM_NOMATCH)
			return 1;
	return 0;
}
