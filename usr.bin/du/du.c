/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fnmatch.h>
#include <fts.h>
#include <getopt.h>
#include <libutil.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <libxo/xo.h>

#define SI_OPT	(CHAR_MAX + 1)

#define UNITS_2		1
#define UNITS_SI	2

#define DU_XO_VERSION "1"

static SLIST_HEAD(ignhead, ignentry) ignores;
struct ignentry {
	char			*mask;
	SLIST_ENTRY(ignentry)	next;
};

static bool	check_threshold(FTSENT *);
static void	ignoreadd(const char *);
static void	ignoreclean(void);
static int	ignorep(FTSENT *);
static int	linkchk(FTSENT *);
static void	print_file_size(FTSENT *);
static void	prthumanval(const char *, int64_t);
static void	record_file_size(FTSENT *);
static void	siginfo(int __unused);
static void	usage(void);

static int	nodumpflag = 0;
static int	Aflag, hflag;
static long	blocksize, cblocksize;
static volatile sig_atomic_t info;
static off_t	threshold, threshold_sign;

static const struct option long_options[] = {
	{ "si", no_argument, NULL, SI_OPT },
	{ NULL, no_argument, NULL, 0 },
};

int
main(int argc, char *argv[])
{
	FTS		*fts;
	FTSENT		*p;
	off_t		savednumber;
	int		ftsoptions;
	int		depth;
	int		Hflag, Lflag, aflag, sflag, dflag, cflag;
	int		lflag, ch, notused, rval;
	char 		**save;
	static char	dot[] = ".";

	setlocale(LC_ALL, "");

	Hflag = Lflag = aflag = sflag = dflag = cflag = lflag = Aflag = 0;

	save = argv;
	ftsoptions = FTS_PHYSICAL;
	savednumber = 0;
	threshold = 0;
	threshold_sign = 1;
	cblocksize = DEV_BSIZE;
	blocksize = 0;
	depth = INT_MAX;
	SLIST_INIT(&ignores);

	argc = xo_parse_args(argc, argv);
	if (argc < 0)
		exit(EX_USAGE);

	while ((ch = getopt_long(argc, argv, "+AB:HI:LPasd:cghklmnrt:x",
	    long_options, NULL)) != -1)
		switch (ch) {
		case 'A':
			Aflag = 1;
			break;
		case 'B':
			errno = 0;
			cblocksize = atoi(optarg);
			if (errno == ERANGE || cblocksize <= 0) {
				xo_warnx("invalid argument to option B: %s",
				    optarg);
				usage();
			}
			break;
		case 'H':
			Hflag = 1;
			Lflag = 0;
			break;
		case 'I':
			ignoreadd(optarg);
			break;
		case 'L':
			Lflag = 1;
			Hflag = 0;
			break;
		case 'P':
			Hflag = Lflag = 0;
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
				xo_warnx("invalid argument to option d: %s",
				    optarg);
				usage();
			}
			break;
		case 'c':
			cflag = 1;
			break;
		case 'g':
			hflag = 0;
			blocksize = 1073741824;
			break;
		case 'h':
			hflag = UNITS_2;
			break;
		case 'k':
			hflag = 0;
			blocksize = 1024;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'm':
			hflag = 0;
			blocksize = 1048576;
			break;
		case 'n':
			nodumpflag = 1;
			break;
		case 'r':		 /* Compatibility. */
			break;
		case 't':
			if (expand_number(optarg, &threshold) != 0 ||
			    threshold == 0) {
				xo_warnx("invalid threshold: %s", optarg);
				usage();
			} else if (threshold < 0)
				threshold_sign = -1;
			break;
		case 'x':
			ftsoptions |= FTS_XDEV;
			break;
		case SI_OPT:
			hflag = UNITS_SI;
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
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

	if (Hflag)
		ftsoptions |= FTS_COMFOLLOW;
	if (Lflag) {
		ftsoptions &= ~FTS_PHYSICAL;
		ftsoptions |= FTS_LOGICAL;
	}

	if (!Aflag && (cblocksize % DEV_BSIZE) != 0)
		cblocksize = howmany(cblocksize, DEV_BSIZE) * DEV_BSIZE;

	if (aflag + dflag + sflag > 1)
		usage();
	if (sflag)
		depth = 0;

	if (!*argv) {
		argv = save;
		argv[0] = dot;
		argv[1] = NULL;
	}

	if (blocksize == 0)
		(void)getbsize(&notused, &blocksize);

	if (!Aflag) {
		cblocksize /= DEV_BSIZE;
		blocksize /= DEV_BSIZE;
	}

	if (threshold != 0)
		threshold = howmany(threshold / DEV_BSIZE * cblocksize,
		    blocksize);

	rval = 0;

	(void)signal(SIGINFO, siginfo);

	if ((fts = fts_open(argv, ftsoptions, NULL)) == NULL)
		err(1, "fts_open");


	xo_set_version(DU_XO_VERSION);
	xo_open_container("disk-usage-information");
	xo_open_list("paths");
	while (errno = 0, (p = fts_read(fts)) != NULL) {
		switch (p->fts_info) {
		case FTS_D:			/* Ignore. */
			if (ignorep(p))
				fts_set(fts, p, FTS_SKIP);
			break;
		case FTS_DP:			/* Directory files */
			if (ignorep(p))
				break;

			record_file_size(p);

			if (p->fts_level <= depth && check_threshold(p))
				print_file_size(p);

			if (info) {
				info = 0;
				(void)printf("\t%s\n", p->fts_path);
			}
			break;
		case FTS_DC:			/* Ignore. */
			break;
		case FTS_DNR:			/* Warn, continue. */
		case FTS_ERR:
		case FTS_NS:
			xo_warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			rval = 1;
			break;
		default:			/* All other files */
			if (ignorep(p))
				break;

			if (lflag == 0 && p->fts_statp->st_nlink > 1 &&
			    linkchk(p))
				break;

			record_file_size(p);

			if ((aflag || p->fts_level == 0) && check_threshold(p))
				print_file_size(p);
		}
		savednumber = p->fts_parent->fts_number;
	}
	xo_close_list("paths");

	if (errno)
		xo_err(1, "fts_read");

	if (cflag) {
		if (hflag > 0) {
			prthumanval("{:total-blocks/%4s}\ttotal\n",
			    savednumber);
		} else {
			xo_emit("{:total-blocks/%jd}\ttotal\n",
			    (intmax_t)howmany(
			    savednumber * cblocksize, blocksize));
		}
	}

	ignoreclean();
	xo_close_container("disk-usage-information");
	if (xo_finish() < 0)
		xo_err(1, "stdout");
	exit(rval);
}

static int
linkchk(FTSENT *p)
{
	struct links_entry {
		struct links_entry *next;
		struct links_entry *previous;
		int	 links;
		dev_t	 dev;
		ino_t	 ino;
	};
	static const size_t links_hash_initial_size = 8192;
	static struct links_entry **buckets;
	static struct links_entry *free_list;
	static size_t number_buckets;
	static unsigned long number_entries;
	static char stop_allocating;
	struct links_entry *le, **new_buckets;
	struct stat *st;
	size_t i, new_size;
	int hash;

	st = p->fts_statp;

	/* If necessary, initialize the hash table. */
	if (buckets == NULL) {
		number_buckets = links_hash_initial_size;
		buckets = malloc(number_buckets * sizeof(buckets[0]));
		if (buckets == NULL)
			errx(1, "No memory for hardlink detection");
		for (i = 0; i < number_buckets; i++)
			buckets[i] = NULL;
	}

	/* If the hash table is getting too full, enlarge it. */
	if (number_entries > number_buckets * 10 && !stop_allocating) {
		new_size = number_buckets * 2;
		new_buckets = calloc(new_size, sizeof(struct links_entry *));

		/* Try releasing the free list to see if that helps. */
		if (new_buckets == NULL && free_list != NULL) {
			while (free_list != NULL) {
				le = free_list;
				free_list = le->next;
				free(le);
			}
			new_buckets = calloc(new_size, sizeof(new_buckets[0]));
		}

		if (new_buckets == NULL) {
			stop_allocating = 1;
			xo_warnx("No more memory for tracking hard links");
		} else {
			for (i = 0; i < number_buckets; i++) {
				while (buckets[i] != NULL) {
					/* Remove entry from old bucket. */
					le = buckets[i];
					buckets[i] = le->next;

					/* Add entry to new bucket. */
					hash = (le->dev ^ le->ino) % new_size;

					if (new_buckets[hash] != NULL)
						new_buckets[hash]->previous =
						    le;
					le->next = new_buckets[hash];
					le->previous = NULL;
					new_buckets[hash] = le;
				}
			}
			free(buckets);
			buckets = new_buckets;
			number_buckets = new_size;
		}
	}

	/* Try to locate this entry in the hash table. */
	hash = (st->st_dev ^ st->st_ino) % number_buckets;
	for (le = buckets[hash]; le != NULL; le = le->next) {
		if (le->dev == st->st_dev && le->ino == st->st_ino) {
			/*
			 * Save memory by releasing an entry when we've seen
			 * all of its links.
			 */
			if (--le->links <= 0) {
				if (le->previous != NULL)
					le->previous->next = le->next;
				if (le->next != NULL)
					le->next->previous = le->previous;
				if (buckets[hash] == le)
					buckets[hash] = le->next;
				number_entries--;
				/* Recycle this node through the free list */
				if (stop_allocating) {
					free(le);
				} else {
					le->next = free_list;
					free_list = le;
				}
			}
			return (1);
		}
	}

	if (stop_allocating)
		return (0);

	/* Add this entry to the links cache. */
	if (free_list != NULL) {
		/* Pull a node from the free list if we can. */
		le = free_list;
		free_list = le->next;
	} else
		/* Malloc one if we have to. */
		le = malloc(sizeof(struct links_entry));
	if (le == NULL) {
		stop_allocating = 1;
		xo_warnx("No more memory for tracking hard links");
		return (0);
	}
	le->dev = st->st_dev;
	le->ino = st->st_ino;
	le->links = st->st_nlink - 1;
	number_entries++;
	le->next = buckets[hash];
	le->previous = NULL;
	if (buckets[hash] != NULL)
		buckets[hash]->previous = le;
	buckets[hash] = le;
	return (0);
}

static void
prthumanval(const char *fmt, int64_t bytes)
{
	char buf[5];
	int flags;

	bytes *= cblocksize;
	flags = HN_B | HN_NOSPACE | HN_DECIMAL;
	if (!Aflag)
		bytes *= DEV_BSIZE;
	if (hflag == UNITS_SI)
		flags |= HN_DIVISOR_1000;

	humanize_number(buf, sizeof(buf), bytes, "", HN_AUTOSCALE, flags);

	xo_emit(fmt, buf);
}

static void
usage(void)
{
	xo_error("%s\n%s\n%s\n",
	    "usage: du [--libxo] [-Aclnx] [-H | -L | -P] [-g | -h | -k | -m]",
	    "          [-a | -s | -d depth] [-B blocksize] [-I mask] [-t threshold]",
	    "          [file ...]");
	exit(EX_USAGE);
}

static void
ignoreadd(const char *mask)
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

static void
ignoreclean(void)
{
	struct ignentry *ign;

	while (!SLIST_EMPTY(&ignores)) {
		ign = SLIST_FIRST(&ignores);
		SLIST_REMOVE_HEAD(&ignores, next);
		free(ign->mask);
		free(ign);
	}
}

static int
ignorep(FTSENT *ent)
{
	struct ignentry *ign;

	if (nodumpflag && (ent->fts_statp->st_flags & UF_NODUMP))
		return (1);
	SLIST_FOREACH(ign, &ignores, next)
		if (fnmatch(ign->mask, ent->fts_name, 0) != FNM_NOMATCH)
			return (1);
	return (0);
}

static void
siginfo(int sig __unused)
{
	info = 1;
}

/*
 * Record the total disk/block size of the file or directory. The fts_number
 * variable provided in FTSENT is used for keeping track of the total size.
 * See FTS(3).
 */
static void
record_file_size(FTSENT *p)
{
	p->fts_number += Aflag ?
	    howmany(p->fts_statp->st_size, cblocksize) :
	    howmany(p->fts_statp->st_blocks, cblocksize);

	p->fts_parent->fts_number += p->fts_number;
}

static bool
check_threshold(FTSENT *p)
{
	return (threshold <= threshold_sign *
	    howmany(p->fts_number * cblocksize, blocksize));
}

static void
print_file_size(FTSENT *p)
{
	xo_open_instance("paths");
	if (hflag > 0) {
		prthumanval("{:blocks/%4s}", p->fts_number);
		xo_emit("\t{:path/%s}\n", p->fts_path);
	} else {
		xo_emit("{:blocks/%jd}\t{:path/%s}\n",
		    (intmax_t)howmany(p->fts_number * cblocksize, blocksize),
		p->fts_path);
	}
	xo_close_instance("paths");
}
