/*	$NetBSD: sort.c,v 1.26 2001/04/30 00:25:09 ross Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Peter McIlroy.
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

/* Sort sorts a file using an optional user-defined key.
 * Sort uses radix sort for internal sorting, and allows
 * a choice of merge sort and radix sort for external sorting.
 */

#include "sort.h"
#include "fsort.h"
#include "pathnames.h"

#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1993\nThe Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
__RCSID("$NetBSD: sort.c,v 1.26 2001/04/30 00:25:09 ross Exp $");
__SCCSID("@(#)sort.c	8.1 (Berkeley) 6/6/93");
#endif
#endif /* not lint */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <paths.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>

int REC_D = '\n';
u_char d_mask[NBINS];		/* flags for rec_d, field_d, <blank> */
/*
 * weight tables.  Gweights is one of ascii, Rascii..
 * modified to weight rec_d = 0 (or 255)
 */
u_char ascii[NBINS], Rascii[NBINS], RFtable[NBINS], Ftable[NBINS];
int SINGL_FLD = 0, SEP_FLAG = 0, UNIQUE = 0;
struct coldesc clist[(ND+1)*2];
int ncols = 0;
extern struct coldesc clist[(ND+1)*2];
extern int ncols;

/*
 * Default to stable sort.
 */
int stable_sort = 1;

char toutpath[MAXPATHLEN];

const char *tmpdir;	/* where temporary files should be put */

static void cleanup __P((void));
static void onsignal __P((int));
static void usage __P((const char *));
static void many_files __P((void));

int main __P((int argc, char **argv));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	get_func_t get;
	int ch, i, stdinflag = 0, tmp = 0;
	char cflag = 0, mflag = 0;
	char *outfile, *outpath = 0;
	struct field fldtab[ND+2], *ftpos;
	struct filelist filelist;
	FILE *outfp = NULL;

	setlocale(LC_ALL, "");

	memset(fldtab, 0, (ND+2)*sizeof(struct field));
	memset(d_mask, 0, NBINS);
	d_mask[REC_D = '\n'] = REC_D_F;
	SINGL_FLD = SEP_FLAG = 0;
	d_mask['\t'] = d_mask[' '] = BLANK | FLD_D;
	ftpos = fldtab;
	many_files();

	fixit(&argc, argv);
	if (!(tmpdir = getenv("TMPDIR")))
		tmpdir = _PATH_TMP;

	while ((ch = getopt(argc, argv, "bcdfik:mHno:rR:sSt:T:ux")) != -1) {
		switch (ch) {
		case 'b':
			fldtab->flags |= BI | BT;
			break;
		case 'c':
			cflag = 1;
			break;
		case 'd': case 'f': case 'i': case 'n': case 'r':
			tmp |= optval(ch, 0);
			if ((tmp & R) && (tmp & F))
				fldtab->weights = RFtable;
			else if (tmp & F)
				fldtab->weights = Ftable;
			else if (tmp & R)
				fldtab->weights = Rascii;
			fldtab->flags |= tmp;
			break;
		case 'H':
			PANIC = 0;
			break;
		case 'k':
			setfield(optarg, ++ftpos, fldtab->flags);
			break;
		case 'm':
			mflag = 1;
			break;
		case 'o':
			outpath = optarg;
			break;
		case 's':
			/* for GNU sort compatibility (this is our default) */
			stable_sort = 1;
			break;
		case 'S':
			stable_sort = 0;
			break;
		case 't':
			if (SEP_FLAG)
				usage("multiple field delimiters");
			SEP_FLAG = 1;
			d_mask[' '] &= ~FLD_D;
			d_mask['\t'] &= ~FLD_D;
			d_mask[(u_char)*optarg] |= FLD_D;
			if (d_mask[(u_char)*optarg] & REC_D_F)
				errx(2, "record/field delimiter clash");
			break;
		case 'R':
			if (REC_D != '\n')
				usage("multiple record delimiters");
			if ('\n' == (REC_D = *optarg))
				break;
			d_mask['\n'] = d_mask[' '];
			d_mask[REC_D] = REC_D_F;
			break;
		case 'T':
			/* -T tmpdir */
			tmpdir = optarg;
			break;
		case 'u':
			UNIQUE = 1;
			break;
		case '?':
		default:
			usage(NULL);
		}
	}
	if (cflag && argc > optind+1)
		errx(2, "too many input files for -c option");
	if (argc - 2 > optind && !strcmp(argv[argc-2], "-o")) {
		outpath = argv[argc-1];
		argc -= 2;
	}
	if (mflag && argc - optind > (MAXFCT - (16+1))*16)
		errx(2, "too many input files for -m option");
	for (i = optind; i < argc; i++) {
		/* allow one occurrence of /dev/stdin */
		if (!strcmp(argv[i], "-") || !strcmp(argv[i], _PATH_STDIN)) {
			if (stdinflag)
				warnx("ignoring extra \"%s\" in file list",
				    argv[i]);
			else
				stdinflag = 1;

			/* change to /dev/stdin if '-' */
			if (argv[i][0] == '-')
				argv[i] = strdup(_PATH_STDIN);

		} else if ((ch = access(argv[i], R_OK)))
			err(2, "%s", argv[i]);
	}
	if (!(fldtab->flags & (I|D|N) || fldtab[1].icol.num)) {
		SINGL_FLD = 1;
		fldtab[0].icol.num = 1;
	} else {
		if (!fldtab[1].icol.num) {
			fldtab[0].flags &= ~(BI|BT);
			setfield("1", ++ftpos, fldtab->flags);
		}
		fldreset(fldtab);
		fldtab[0].flags &= ~F;
	}
	settables(fldtab[0].flags);
	num_init();
	fldtab->weights = gweights;
	if (optind == argc) {
		static const char * const names[] = { _PATH_STDIN, NULL };

		filelist.names = names;
		optind--;
	} else
		filelist.names = (const char * const *) &argv[optind];

	if (SINGL_FLD)
		get = makeline;
	else
		get = makekey;

	if (cflag) {
		order(&filelist, get, fldtab);
		/* NOT REACHED */
	}
	if (!outpath) {
		(void)snprintf(toutpath,
		    sizeof(toutpath), "%sstdout", _PATH_DEV);
		outfile = outpath = toutpath;
		outfp = stdout;
	} else if (!(ch = access(outpath, 0)) &&
	    strncmp(_PATH_DEV, outpath, 5)) {
		static const struct sigaction act =
		    { { onsignal }, SA_RESTART | SA_RESETHAND, { { 0 } } };
		static const int sigtable[] = {SIGHUP, SIGINT, SIGPIPE,
		    SIGXCPU, SIGXFSZ, SIGVTALRM, SIGPROF, 0};
		int outfd;
		errno = 0;
		if (access(outpath, W_OK))
			err(2, "%s", outpath);
		(void)snprintf(toutpath, sizeof(toutpath), "%sXXXXXX",
		    outpath);
		if ((outfd = mkstemp(toutpath)) == -1)
			err(2, "Cannot create temporary file `%s'", toutpath);
		if ((outfp = fdopen(outfd, "w")) == NULL)
			err(2, "Cannot open temporary file `%s'", toutpath);
		outfile = toutpath;
		(void)atexit(cleanup);
		for (i = 0; sigtable[i]; ++i)	/* always unlink toutpath */
			sigaction(sigtable[i], &act, 0);
	} else
		outfile = outpath;

	if (outfp == NULL && (outfp = fopen(outfile, "w")) == NULL)
		err(2, "output file %s", outfile);

	if (mflag) {
		fmerge(-1, 0, &filelist, argc-optind, get, outfp, putline,
			fldtab);
	} else
		fsort(-1, 0, 0, &filelist, argc-optind, outfp, fldtab);

	if (outfile != outpath) {
		if (access(outfile, 0))
			err(2, "%s", outfile);
		(void)unlink(outpath);
		if (link(outfile, outpath))
			err(2, "cannot link %s: output left in %s",
			    outpath, outfile);
		(void)unlink(outfile);
	}
	exit(0);
}

static void
onsignal(sig)
	int sig __unused;
{
	cleanup();
}

static void
cleanup()
{
	if (toutpath[0])
		(void)unlink(toutpath);
}

static void
usage(msg)
	const char *msg;
{
	if (msg != NULL)
		(void)fprintf(stderr, "sort: %s\n", msg);
	(void)fprintf(stderr, "usage: [-o output] [-cmubdfinrsS] [-t char] ");
	(void)fprintf(stderr, "[-R char] [-k keydef] ... [files]\n");
	exit(2);
}

static void
many_files()
{
#if 0
	struct rlimit rlp_many_files[1];

	if (getrlimit(RLIMIT_NOFILE, rlp_many_files) == 0) {
		rlp_many_files->rlim_cur = rlp_many_files->rlim_max;
		setrlimit(RLIMIT_NOFILE, rlp_many_files);
	}
#endif
}
