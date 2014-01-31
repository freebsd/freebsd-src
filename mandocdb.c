/*	$Id: mandocdb.c,v 1.49.2.10 2013/11/21 01:53:48 schwarze Exp $ */
/*
 * Copyright (c) 2011, 2012 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011, 2012 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__APPLE__)
# include <libkern/OSByteOrder.h>
#elif defined(__linux__)
# include <endian.h>
#elif defined(__sun)
# include <sys/byteorder.h>
# include <sys/stat.h>
#else
# include <sys/endian.h>
#endif

#if defined(__linux__) || defined(__sun)
# include <db_185.h>
#else
# include <db.h>
#endif

#include "man.h"
#include "mdoc.h"
#include "mandoc.h"
#include "mandocdb.h"
#include "manpath.h"

#define	MANDOC_BUFSZ	  BUFSIZ
#define	MANDOC_SLOP	  1024

#define	MANDOC_SRC	  0x1
#define	MANDOC_FORM	  0x2

/* Access to the mandoc database on disk. */

struct	mdb {
	char		  idxn[PATH_MAX]; /* index db filename */
	char		  dbn[PATH_MAX]; /* keyword db filename */
	DB		 *idx; /* index recno database */
	DB		 *db; /* keyword btree database */
};

/* Stack of temporarily unused index records. */

struct	recs {
	recno_t		 *stack; /* pointer to a malloc'ed array */
	size_t		  size; /* number of allocated slots */
	size_t		  cur; /* current number of empty records */
	recno_t		  last; /* last record number in the index */
};

/* Tiny list for files.  No need to bring in QUEUE. */

struct	of {
	char		 *fname; /* heap-allocated */
	char		 *sec;
	char		 *arch;
	char		 *title;
	int		  src_form;
	struct of	 *next; /* NULL for last one */
	struct of	 *first; /* first in list */
};

/* Buffer for storing growable data. */

struct	buf {
	char		 *cp;
	size_t		  len; /* current length */
	size_t		  size; /* total buffer size */
};

/* Operation we're going to perform. */

enum	op {
	OP_DEFAULT = 0, /* new dbs from dir list or default config */
	OP_CONFFILE, /* new databases from custom config file */
	OP_UPDATE, /* delete/add entries in existing database */
	OP_DELETE, /* delete entries from existing database */
	OP_TEST /* change no databases, report potential problems */
};

#define	MAN_ARGS	  DB *hash, \
			  struct buf *buf, \
			  struct buf *dbuf, \
			  const struct man_node *n
#define	MDOC_ARGS	  DB *hash, \
			  struct buf *buf, \
			  struct buf *dbuf, \
			  const struct mdoc_node *n, \
			  const struct mdoc_meta *m

static	void		  buf_appendmdoc(struct buf *, 
				const struct mdoc_node *, int);
static	void		  buf_append(struct buf *, const char *);
static	void		  buf_appendb(struct buf *, 
				const void *, size_t);
static	void		  dbt_put(DB *, const char *, DBT *, DBT *);
static	void		  hash_put(DB *, const struct buf *, uint64_t);
static	void		  hash_reset(DB **);
static	void		  index_merge(const struct of *, struct mparse *,
				struct buf *, struct buf *, DB *,
				struct mdb *, struct recs *);
static	void		  index_prune(const struct of *, struct mdb *,
				struct recs *);
static	void		  ofile_argbuild(int, char *[], struct of **,
				const char *);
static	void		  ofile_dirbuild(const char *, const char *,
				const char *, int, struct of **);
static	void		  ofile_free(struct of *);
static	void		  pformatted(DB *, struct buf *, 
				struct buf *, const struct of *);
static	int		  pman_node(MAN_ARGS);
static	void		  pmdoc_node(MDOC_ARGS);
static	int		  pmdoc_head(MDOC_ARGS);
static	int		  pmdoc_body(MDOC_ARGS);
static	int		  pmdoc_Fd(MDOC_ARGS);
static	int		  pmdoc_In(MDOC_ARGS);
static	int		  pmdoc_Fn(MDOC_ARGS);
static	int		  pmdoc_Nd(MDOC_ARGS);
static	int		  pmdoc_Nm(MDOC_ARGS);
static	int		  pmdoc_Sh(MDOC_ARGS);
static	int		  pmdoc_St(MDOC_ARGS);
static	int		  pmdoc_Xr(MDOC_ARGS);

#define	MDOCF_CHILD	  0x01  /* Automatically index child nodes. */

struct	mdoc_handler {
	int		(*fp)(MDOC_ARGS);  /* Optional handler. */
	uint64_t	  mask;  /* Set unless handler returns 0. */
	int		  flags;  /* For use by pmdoc_node. */
};

static	const struct mdoc_handler mdocs[MDOC_MAX] = {
	{ NULL, 0, 0 },  /* Ap */
	{ NULL, 0, 0 },  /* Dd */
	{ NULL, 0, 0 },  /* Dt */
	{ NULL, 0, 0 },  /* Os */
	{ pmdoc_Sh, TYPE_Sh, MDOCF_CHILD }, /* Sh */
	{ pmdoc_head, TYPE_Ss, MDOCF_CHILD }, /* Ss */
	{ NULL, 0, 0 },  /* Pp */
	{ NULL, 0, 0 },  /* D1 */
	{ NULL, 0, 0 },  /* Dl */
	{ NULL, 0, 0 },  /* Bd */
	{ NULL, 0, 0 },  /* Ed */
	{ NULL, 0, 0 },  /* Bl */
	{ NULL, 0, 0 },  /* El */
	{ NULL, 0, 0 },  /* It */
	{ NULL, 0, 0 },  /* Ad */
	{ NULL, TYPE_An, MDOCF_CHILD },  /* An */
	{ NULL, TYPE_Ar, MDOCF_CHILD },  /* Ar */
	{ NULL, TYPE_Cd, MDOCF_CHILD },  /* Cd */
	{ NULL, TYPE_Cm, MDOCF_CHILD },  /* Cm */
	{ NULL, TYPE_Dv, MDOCF_CHILD },  /* Dv */
	{ NULL, TYPE_Er, MDOCF_CHILD },  /* Er */
	{ NULL, TYPE_Ev, MDOCF_CHILD },  /* Ev */
	{ NULL, 0, 0 },  /* Ex */
	{ NULL, TYPE_Fa, MDOCF_CHILD },  /* Fa */
	{ pmdoc_Fd, TYPE_In, 0 },  /* Fd */
	{ NULL, TYPE_Fl, MDOCF_CHILD },  /* Fl */
	{ pmdoc_Fn, 0, 0 },  /* Fn */
	{ NULL, TYPE_Ft, MDOCF_CHILD },  /* Ft */
	{ NULL, TYPE_Ic, MDOCF_CHILD },  /* Ic */
	{ pmdoc_In, TYPE_In, 0 },  /* In */
	{ NULL, TYPE_Li, MDOCF_CHILD },  /* Li */
	{ pmdoc_Nd, TYPE_Nd, MDOCF_CHILD },  /* Nd */
	{ pmdoc_Nm, TYPE_Nm, MDOCF_CHILD },  /* Nm */
	{ NULL, 0, 0 },  /* Op */
	{ NULL, 0, 0 },  /* Ot */
	{ NULL, TYPE_Pa, MDOCF_CHILD },  /* Pa */
	{ NULL, 0, 0 },  /* Rv */
	{ pmdoc_St, TYPE_St, 0 },  /* St */
	{ NULL, TYPE_Va, MDOCF_CHILD },  /* Va */
	{ pmdoc_body, TYPE_Va, MDOCF_CHILD },  /* Vt */
	{ pmdoc_Xr, TYPE_Xr, 0 },  /* Xr */
	{ NULL, 0, 0 },  /* %A */
	{ NULL, 0, 0 },  /* %B */
	{ NULL, 0, 0 },  /* %D */
	{ NULL, 0, 0 },  /* %I */
	{ NULL, 0, 0 },  /* %J */
	{ NULL, 0, 0 },  /* %N */
	{ NULL, 0, 0 },  /* %O */
	{ NULL, 0, 0 },  /* %P */
	{ NULL, 0, 0 },  /* %R */
	{ NULL, 0, 0 },  /* %T */
	{ NULL, 0, 0 },  /* %V */
	{ NULL, 0, 0 },  /* Ac */
	{ NULL, 0, 0 },  /* Ao */
	{ NULL, 0, 0 },  /* Aq */
	{ NULL, TYPE_At, MDOCF_CHILD },  /* At */
	{ NULL, 0, 0 },  /* Bc */
	{ NULL, 0, 0 },  /* Bf */
	{ NULL, 0, 0 },  /* Bo */
	{ NULL, 0, 0 },  /* Bq */
	{ NULL, TYPE_Bsx, MDOCF_CHILD },  /* Bsx */
	{ NULL, TYPE_Bx, MDOCF_CHILD },  /* Bx */
	{ NULL, 0, 0 },  /* Db */
	{ NULL, 0, 0 },  /* Dc */
	{ NULL, 0, 0 },  /* Do */
	{ NULL, 0, 0 },  /* Dq */
	{ NULL, 0, 0 },  /* Ec */
	{ NULL, 0, 0 },  /* Ef */
	{ NULL, TYPE_Em, MDOCF_CHILD },  /* Em */
	{ NULL, 0, 0 },  /* Eo */
	{ NULL, TYPE_Fx, MDOCF_CHILD },  /* Fx */
	{ NULL, TYPE_Ms, MDOCF_CHILD },  /* Ms */
	{ NULL, 0, 0 },  /* No */
	{ NULL, 0, 0 },  /* Ns */
	{ NULL, TYPE_Nx, MDOCF_CHILD },  /* Nx */
	{ NULL, TYPE_Ox, MDOCF_CHILD },  /* Ox */
	{ NULL, 0, 0 },  /* Pc */
	{ NULL, 0, 0 },  /* Pf */
	{ NULL, 0, 0 },  /* Po */
	{ NULL, 0, 0 },  /* Pq */
	{ NULL, 0, 0 },  /* Qc */
	{ NULL, 0, 0 },  /* Ql */
	{ NULL, 0, 0 },  /* Qo */
	{ NULL, 0, 0 },  /* Qq */
	{ NULL, 0, 0 },  /* Re */
	{ NULL, 0, 0 },  /* Rs */
	{ NULL, 0, 0 },  /* Sc */
	{ NULL, 0, 0 },  /* So */
	{ NULL, 0, 0 },  /* Sq */
	{ NULL, 0, 0 },  /* Sm */
	{ NULL, 0, 0 },  /* Sx */
	{ NULL, TYPE_Sy, MDOCF_CHILD },  /* Sy */
	{ NULL, TYPE_Tn, MDOCF_CHILD },  /* Tn */
	{ NULL, 0, 0 },  /* Ux */
	{ NULL, 0, 0 },  /* Xc */
	{ NULL, 0, 0 },  /* Xo */
	{ pmdoc_head, TYPE_Fn, 0 },  /* Fo */
	{ NULL, 0, 0 },  /* Fc */
	{ NULL, 0, 0 },  /* Oo */
	{ NULL, 0, 0 },  /* Oc */
	{ NULL, 0, 0 },  /* Bk */
	{ NULL, 0, 0 },  /* Ek */
	{ NULL, 0, 0 },  /* Bt */
	{ NULL, 0, 0 },  /* Hf */
	{ NULL, 0, 0 },  /* Fr */
	{ NULL, 0, 0 },  /* Ud */
	{ NULL, TYPE_Lb, MDOCF_CHILD },  /* Lb */
	{ NULL, 0, 0 },  /* Lp */
	{ NULL, TYPE_Lk, MDOCF_CHILD },  /* Lk */
	{ NULL, TYPE_Mt, MDOCF_CHILD },  /* Mt */
	{ NULL, 0, 0 },  /* Brq */
	{ NULL, 0, 0 },  /* Bro */
	{ NULL, 0, 0 },  /* Brc */
	{ NULL, 0, 0 },  /* %C */
	{ NULL, 0, 0 },  /* Es */
	{ NULL, 0, 0 },  /* En */
	{ NULL, TYPE_Dx, MDOCF_CHILD },  /* Dx */
	{ NULL, 0, 0 },  /* %Q */
	{ NULL, 0, 0 },  /* br */
	{ NULL, 0, 0 },  /* sp */
	{ NULL, 0, 0 },  /* %U */
	{ NULL, 0, 0 },  /* Ta */
};

static	const char	 *progname;
static	int		  use_all;  /* Use all directories and files. */
static	int		  verb;  /* Output verbosity level. */
static	int		  warnings;  /* Potential problems in manuals. */

int
main(int argc, char *argv[])
{
	struct mparse	*mp; /* parse sequence */
	struct manpaths	 dirs;
	struct mdb	 mdb;
	struct recs	 recs;
	enum op		 op; /* current operation */
	const char	*dir;
	char		*cp;
	char		 pbuf[PATH_MAX];
	int		 ch, i, flags;
	DB		*hash; /* temporary keyword hashtable */
	BTREEINFO	 info; /* btree configuration */
	size_t		 sz1, sz2, ipath;
	struct buf	 buf, /* keyword buffer */
			 dbuf; /* description buffer */
	struct of	*of; /* list of files for processing */
	extern int	 optind;
	extern char	*optarg;

	progname = strrchr(argv[0], '/');
	if (progname == NULL)
		progname = argv[0];
	else
		++progname;

	memset(&dirs, 0, sizeof(struct manpaths));
	memset(&mdb, 0, sizeof(struct mdb));
	memset(&recs, 0, sizeof(struct recs));

	of = NULL;
	mp = NULL;
	hash = NULL;
	op = OP_DEFAULT;
	dir = NULL;

	while (-1 != (ch = getopt(argc, argv, "aC:d:tu:vW")))
		switch (ch) {
		case ('a'):
			use_all = 1;
			break;
		case ('C'):
			if (op) {
				fprintf(stderr,
				    "-C: conflicting options\n");
				goto usage;
			}
			dir = optarg;
			op = OP_CONFFILE;
			break;
		case ('d'):
			if (op) {
				fprintf(stderr,
				    "-d: conflicting options\n");
				goto usage;
			}
			dir = optarg;
			op = OP_UPDATE;
			break;
		case ('t'):
			dup2(STDOUT_FILENO, STDERR_FILENO);
			if (op) {
				fprintf(stderr,
				    "-t: conflicting options\n");
				goto usage;
			}
			op = OP_TEST;
			use_all = 1;
			warnings = 1;
			break;
		case ('u'):
			if (op) {
				fprintf(stderr,
				    "-u: conflicting options\n");
				goto usage;
			}
			dir = optarg;
			op = OP_DELETE;
			break;
		case ('v'):
			verb++;
			break;
		case ('W'):
			warnings = 1;
			break;
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;

	if (OP_CONFFILE == op && argc > 0) {
		fprintf(stderr, "-C: too many arguments\n");
		goto usage;
	}

	memset(&info, 0, sizeof(BTREEINFO));
	info.lorder = 4321;
	info.flags = R_DUP;

	mp = mparse_alloc(MPARSE_AUTO, MANDOCLEVEL_FATAL, NULL, NULL, NULL);

	memset(&buf, 0, sizeof(struct buf));
	memset(&dbuf, 0, sizeof(struct buf));

	buf.size = dbuf.size = MANDOC_BUFSZ;

	buf.cp = mandoc_malloc(buf.size);
	dbuf.cp = mandoc_malloc(dbuf.size);

	if (OP_TEST == op) {
		ofile_argbuild(argc, argv, &of, NULL);
		if (NULL == of)
			goto out;
		index_merge(of, mp, &dbuf, &buf, hash, &mdb, &recs);
		goto out;
	}

	if (OP_UPDATE == op || OP_DELETE == op) {
		if (NULL == realpath(dir, pbuf)) {
			perror(dir);
			exit((int)MANDOCLEVEL_BADARG);
		}
		if (strlcat(pbuf, "/", PATH_MAX) >= PATH_MAX) {
			fprintf(stderr, "%s: path too long\n", pbuf);
			exit((int)MANDOCLEVEL_BADARG);
		}

		strlcat(mdb.dbn, pbuf, PATH_MAX);
		sz1 = strlcat(mdb.dbn, MANDOC_DB, PATH_MAX);

		strlcat(mdb.idxn, pbuf, PATH_MAX);
		sz2 = strlcat(mdb.idxn, MANDOC_IDX, PATH_MAX);

		if (sz1 >= PATH_MAX || sz2 >= PATH_MAX) {
			fprintf(stderr, "%s: path too long\n", mdb.idxn);
			exit((int)MANDOCLEVEL_BADARG);
		}

		flags = O_CREAT | O_RDWR;
		mdb.db = dbopen(mdb.dbn, flags, 0644, DB_BTREE, &info);
		mdb.idx = dbopen(mdb.idxn, flags, 0644, DB_RECNO, NULL);

		if (NULL == mdb.db) {
			perror(mdb.dbn);
			exit((int)MANDOCLEVEL_SYSERR);
		} else if (NULL == mdb.idx) {
			perror(mdb.idxn);
			exit((int)MANDOCLEVEL_SYSERR);
		}

		ofile_argbuild(argc, argv, &of, pbuf);

		if (NULL == of)
			goto out;

		index_prune(of, &mdb, &recs);

		/*
		 * Go to the root of the respective manual tree.
		 * This must work or no manuals may be found (they're
		 * indexed relative to the root).
		 */

		if (OP_UPDATE == op) {
			if (-1 == chdir(dir)) {
				perror(dir);
				exit((int)MANDOCLEVEL_SYSERR);
			}
			index_merge(of, mp, &dbuf, &buf, hash,
					&mdb, &recs);
		}

		goto out;
	}

	/*
	 * Configure the directories we're going to scan.
	 * If we have command-line arguments, use them.
	 * If not, we use man(1)'s method (see mandocdb.8).
	 */

	if (argc > 0) {
		dirs.paths = mandoc_calloc(argc, sizeof(char *));
		dirs.sz = argc;
		for (i = 0; i < argc; i++) {
			if (NULL == (cp = realpath(argv[i], pbuf))) {
				perror(argv[i]);
				goto out;
			}
			dirs.paths[i] = mandoc_strdup(cp);
		}
	} else
		manpath_parse(&dirs, dir, NULL, NULL);

	for (ipath = 0; ipath < dirs.sz; ipath++) {

		/*
		 * Go to the root of the respective manual tree.
		 * This must work or no manuals may be found:
		 * They are indexed relative to the root.
		 */

		if (-1 == chdir(dirs.paths[ipath])) {
			perror(dirs.paths[ipath]);
			exit((int)MANDOCLEVEL_SYSERR);
		}

		/* Create a new database in two temporary files. */

		flags = O_CREAT | O_EXCL | O_RDWR;
		while (NULL == mdb.db) {
			strlcpy(mdb.dbn, MANDOC_DB, PATH_MAX);
			strlcat(mdb.dbn, ".XXXXXXXXXX", PATH_MAX);
			if (NULL == mktemp(mdb.dbn)) {
				perror(mdb.dbn);
				exit((int)MANDOCLEVEL_SYSERR);
			}
			mdb.db = dbopen(mdb.dbn, flags, 0644,
					DB_BTREE, &info);
			if (NULL == mdb.db && EEXIST != errno) {
				perror(mdb.dbn);
				exit((int)MANDOCLEVEL_SYSERR);
			}
		}
		while (NULL == mdb.idx) {
			strlcpy(mdb.idxn, MANDOC_IDX, PATH_MAX);
			strlcat(mdb.idxn, ".XXXXXXXXXX", PATH_MAX);
			if (NULL == mktemp(mdb.idxn)) {
				perror(mdb.idxn);
				unlink(mdb.dbn);
				exit((int)MANDOCLEVEL_SYSERR);
			}
			mdb.idx = dbopen(mdb.idxn, flags, 0644,
					DB_RECNO, NULL);
			if (NULL == mdb.idx && EEXIST != errno) {
				perror(mdb.idxn);
				unlink(mdb.dbn);
				exit((int)MANDOCLEVEL_SYSERR);
			}
		}

		/*
		 * Search for manuals and fill the new database.
		 */

	       	ofile_dirbuild(".", "", "", 0, &of);

		if (NULL != of) {
			index_merge(of, mp, &dbuf, &buf, hash,
			     &mdb, &recs);
			ofile_free(of);
			of = NULL;
		}

		(*mdb.db->close)(mdb.db);
		(*mdb.idx->close)(mdb.idx);
		mdb.db = NULL;
		mdb.idx = NULL;

		/*
		 * Replace the old database with the new one.
		 * This is not perfectly atomic,
		 * but i cannot think of a better way.
		 */

		if (-1 == rename(mdb.dbn, MANDOC_DB)) {
			perror(MANDOC_DB);
			unlink(mdb.dbn);
			unlink(mdb.idxn);
			exit((int)MANDOCLEVEL_SYSERR);
		}
		if (-1 == rename(mdb.idxn, MANDOC_IDX)) {
			perror(MANDOC_IDX);
			unlink(MANDOC_DB);
			unlink(MANDOC_IDX);
			unlink(mdb.idxn);
			exit((int)MANDOCLEVEL_SYSERR);
		}
	}

out:
	if (mdb.db)
		(*mdb.db->close)(mdb.db);
	if (mdb.idx)
		(*mdb.idx->close)(mdb.idx);
	if (hash)
		(*hash->close)(hash);
	if (mp)
		mparse_free(mp);

	manpath_free(&dirs);
	ofile_free(of);
	free(buf.cp);
	free(dbuf.cp);
	free(recs.stack);

	return(MANDOCLEVEL_OK);

usage:
	fprintf(stderr,
		"usage: %s [-avvv] [-C file] | dir ... | -t file ...\n"
		"                        -d dir [file ...] | "
		"-u dir [file ...]\n",
		progname);

	return((int)MANDOCLEVEL_BADARG);
}

void
index_merge(const struct of *of, struct mparse *mp,
		struct buf *dbuf, struct buf *buf, DB *hash,
		struct mdb *mdb, struct recs *recs)
{
	recno_t		 rec;
	int		 ch, skip;
	DBT		 key, val;
	DB		*files;  /* temporary file name table */
	struct mdoc	*mdoc;
	struct man	*man;
	const char	*fn, *msec, *march, *mtitle;
	char		*p;
	uint64_t	 mask;
	size_t		 sv;
	unsigned	 seq;
	uint64_t	 vbuf[2];
	char		 type;

	static char	 emptystring[] = "";

	if (warnings) {
		files = NULL;
		hash_reset(&files);
	}

	rec = 0;
	for (of = of->first; of; of = of->next) {
		fn = of->fname;

		/*
		 * Try interpreting the file as mdoc(7) or man(7)
		 * source code, unless it is already known to be
		 * formatted.  Fall back to formatted mode.
		 */

		mparse_reset(mp);
		mdoc = NULL;
		man = NULL;

		if ((MANDOC_SRC & of->src_form ||
		    ! (MANDOC_FORM & of->src_form)) &&
		    MANDOCLEVEL_FATAL > mparse_readfd(mp, -1, fn))
			mparse_result(mp, &mdoc, &man);

		if (NULL != mdoc) {
			msec = mdoc_meta(mdoc)->msec;
			march = mdoc_meta(mdoc)->arch;
			if (NULL == march)
				march = "";
			mtitle = mdoc_meta(mdoc)->title;
		} else if (NULL != man) {
			msec = man_meta(man)->msec;
			march = "";
			mtitle = man_meta(man)->title;
		} else {
			msec = of->sec;
			march = of->arch;
			mtitle = of->title;
		}

		/*
		 * Check whether the manual section given in a file
		 * agrees with the directory where the file is located.
		 * Some manuals have suffixes like (3p) on their
		 * section number either inside the file or in the
		 * directory name, some are linked into more than one
		 * section, like encrypt(1) = makekey(8).  Do not skip
		 * manuals for such reasons.
		 */

		skip = 0;
		assert(of->sec);
		assert(msec);
		if (warnings)
			if (strcasecmp(msec, of->sec))
				fprintf(stderr, "%s: "
					"section \"%s\" manual "
					"in \"%s\" directory\n",
					fn, msec, of->sec);

		/*
		 * Manual page directories exist for each kernel
		 * architecture as returned by machine(1).
		 * However, many manuals only depend on the
		 * application architecture as returned by arch(1).
		 * For example, some (2/ARM) manuals are shared
		 * across the "armish" and "zaurus" kernel
		 * architectures.
		 * A few manuals are even shared across completely
		 * different architectures, for example fdformat(1)
		 * on amd64, i386, sparc, and sparc64.
		 * Thus, warn about architecture mismatches,
		 * but don't skip manuals for this reason.
		 */

		assert(of->arch);
		assert(march);
		if (warnings)
			if (strcasecmp(march, of->arch))
				fprintf(stderr, "%s: "
					"architecture \"%s\" manual "
					"in \"%s\" directory\n",
					fn, march, of->arch);

		/*
		 * By default, skip a file if the title given
		 * in the file disagrees with the file name.
		 * Do not warn, this happens for all MLINKs.
		 */

		assert(of->title);
		assert(mtitle);
		if (strcasecmp(mtitle, of->title))
			skip = 1;

		/*
		 * Build a title string for the file.  If it matches
		 * the location of the file, remember the title as
		 * found; else, remember it as missing.
		 */

		if (warnings) {
			buf->len = 0;
			buf_appendb(buf, mtitle, strlen(mtitle));
			buf_appendb(buf, "(", 1);
			buf_appendb(buf, msec, strlen(msec));
			if ('\0' != *march) {
				buf_appendb(buf, "/", 1);
				buf_appendb(buf, march, strlen(march));
			}
			buf_appendb(buf, ")", 2);
			for (p = buf->cp; '\0' != *p; p++)
				*p = tolower((unsigned char)*p);
			key.data = buf->cp;
			key.size = buf->len;
			val.data = NULL;
			val.size = 0;
			if (0 == skip)
				val.data = emptystring;
			else {
				ch = (*files->get)(files, &key, &val, 0);
				if (ch < 0) {
					perror("hash");
					exit((int)MANDOCLEVEL_SYSERR);
				} else if (ch > 0) {
					val.data = (void *)fn;
					val.size = strlen(fn) + 1;
				} else
					val.data = NULL;
			}
			if (NULL != val.data &&
			    (*files->put)(files, &key, &val, 0) < 0) {
				perror("hash");
				exit((int)MANDOCLEVEL_SYSERR);
			}
		}

		if (skip && !use_all)
			continue;

		/*
		 * The index record value consists of a nil-terminated
		 * filename, a nil-terminated manual section, and a
		 * nil-terminated description.  Use the actual
		 * location of the file, such that the user can find
		 * it with man(1).  Since the description may not be
		 * set, we set a sentinel to see if we're going to
		 * write a nil byte in its place.
		 */

		dbuf->len = 0;
		type = mdoc ? 'd' : (man ? 'a' : 'c');
		buf_appendb(dbuf, &type, 1);
		buf_appendb(dbuf, fn, strlen(fn) + 1);
		buf_appendb(dbuf, of->sec, strlen(of->sec) + 1);
		buf_appendb(dbuf, of->title, strlen(of->title) + 1);
		buf_appendb(dbuf, of->arch, strlen(of->arch) + 1);

		sv = dbuf->len;

		/*
		 * Collect keyword/mask pairs.
		 * Each pair will become a new btree node.
		 */

		hash_reset(&hash);
		if (mdoc)
			pmdoc_node(hash, buf, dbuf,
				mdoc_node(mdoc), mdoc_meta(mdoc));
		else if (man)
			pman_node(hash, buf, dbuf, man_node(man));
		else
			pformatted(hash, buf, dbuf, of);

		/* Test mode, do not access any database. */

		if (NULL == mdb->db || NULL == mdb->idx)
			continue;

		/*
		 * Make sure the file name is always registered
		 * as an .Nm search key.
		 */
		buf->len = 0;
		buf_append(buf, of->title);
		hash_put(hash, buf, TYPE_Nm);

		/*
		 * Reclaim an empty index record, if available.
		 * Use its record number for all new btree nodes.
		 */

		if (recs->cur > 0) {
			recs->cur--;
			rec = recs->stack[(int)recs->cur];
		} else if (recs->last > 0) {
			rec = recs->last;
			recs->last = 0;
		} else
			rec++;
		vbuf[1] = htobe64(rec);

		/*
		 * Copy from the in-memory hashtable of pending
		 * keyword/mask pairs into the database.
		 */

		seq = R_FIRST;
		while (0 == (ch = (*hash->seq)(hash, &key, &val, seq))) {
			seq = R_NEXT;
			assert(sizeof(uint64_t) == val.size);
			memcpy(&mask, val.data, val.size);
			vbuf[0] = htobe64(mask);
			val.size = sizeof(vbuf);
			val.data = &vbuf;
			dbt_put(mdb->db, mdb->dbn, &key, &val);
		}
		if (ch < 0) {
			perror("hash");
			unlink(mdb->dbn);
			unlink(mdb->idxn);
			exit((int)MANDOCLEVEL_SYSERR);
		}

		/*
		 * Apply to the index.  If we haven't had a description
		 * set, put an empty one in now.
		 */

		if (dbuf->len == sv)
			buf_appendb(dbuf, "", 1);

		key.data = &rec;
		key.size = sizeof(recno_t);

		val.data = dbuf->cp;
		val.size = dbuf->len;

		if (verb)
			printf("%s: adding to index\n", fn);

		dbt_put(mdb->idx, mdb->idxn, &key, &val);
	}

	/*
	 * Iterate the remembered file titles and check that
	 * all files can be found by their main title.
	 */

	if (warnings) {
		seq = R_FIRST;
		while (0 == (*files->seq)(files, &key, &val, seq)) {
			seq = R_NEXT;
			if (val.size)
				fprintf(stderr, "%s: probably "
				    "unreachable, title is %s\n",
				    (char *)val.data, (char *)key.data);
		}
		(*files->close)(files);
	}
}

/*
 * Scan through all entries in the index file `idx' and prune those
 * entries in `ofile'.
 * Pruning consists of removing from `db', then invalidating the entry
 * in `idx' (zeroing its value size).
 */
static void
index_prune(const struct of *ofile, struct mdb *mdb, struct recs *recs)
{
	const struct of	*of;
	const char	*fn;
	uint64_t	 vbuf[2];
	unsigned	 seq, sseq;
	DBT		 key, val;
	int		 ch;

	recs->cur = 0;
	seq = R_FIRST;
	while (0 == (ch = (*mdb->idx->seq)(mdb->idx, &key, &val, seq))) {
		seq = R_NEXT;
		assert(sizeof(recno_t) == key.size);
		memcpy(&recs->last, key.data, key.size);

		/* Deleted records are zero-sized.  Skip them. */

		if (0 == val.size)
			goto cont;

		/*
		 * Make sure we're sane.
		 * Read past our mdoc/man/cat type to the next string,
		 * then make sure it's bounded by a NUL.
		 * Failing any of these, we go into our error handler.
		 */

		fn = (char *)val.data + 1;
		if (NULL == memchr(fn, '\0', val.size - 1))
			break;

		/*
		 * Search for the file in those we care about.
		 * XXX: build this into a tree.  Too slow.
		 */

		for (of = ofile->first; of; of = of->next)
			if (0 == strcmp(fn, of->fname))
				break;

		if (NULL == of)
			continue;

		/*
		 * Search through the keyword database, throwing out all
		 * references to our file.
		 */

		sseq = R_FIRST;
		while (0 == (ch = (*mdb->db->seq)(mdb->db,
					&key, &val, sseq))) {
			sseq = R_NEXT;
			if (sizeof(vbuf) != val.size)
				break;

			memcpy(vbuf, val.data, val.size);
			if (recs->last != betoh64(vbuf[1]))
				continue;

			if ((ch = (*mdb->db->del)(mdb->db,
					&key, R_CURSOR)) < 0)
				break;
		}

		if (ch < 0) {
			perror(mdb->dbn);
			exit((int)MANDOCLEVEL_SYSERR);
		} else if (1 != ch) {
			fprintf(stderr, "%s: corrupt database\n",
					mdb->dbn);
			exit((int)MANDOCLEVEL_SYSERR);
		}

		if (verb)
			printf("%s: deleting from index\n", fn);

		val.size = 0;
		ch = (*mdb->idx->put)(mdb->idx, &key, &val, R_CURSOR);

		if (ch < 0)
			break;
cont:
		if (recs->cur >= recs->size) {
			recs->size += MANDOC_SLOP;
			recs->stack = mandoc_realloc(recs->stack,
					recs->size * sizeof(recno_t));
		}

		recs->stack[(int)recs->cur] = recs->last;
		recs->cur++;
	}

	if (ch < 0) {
		perror(mdb->idxn);
		exit((int)MANDOCLEVEL_SYSERR);
	} else if (1 != ch) {
		fprintf(stderr, "%s: corrupt index\n", mdb->idxn);
		exit((int)MANDOCLEVEL_SYSERR);
	}

	recs->last++;
}

/*
 * Grow the buffer (if necessary) and copy in a binary string.
 */
static void
buf_appendb(struct buf *buf, const void *cp, size_t sz)
{

	/* Overshoot by MANDOC_BUFSZ. */

	while (buf->len + sz >= buf->size) {
		buf->size = buf->len + sz + MANDOC_BUFSZ;
		buf->cp = mandoc_realloc(buf->cp, buf->size);
	}

	memcpy(buf->cp + (int)buf->len, cp, sz);
	buf->len += sz;
}

/*
 * Append a nil-terminated string to the buffer.  
 * This can be invoked multiple times.  
 * The buffer string will be nil-terminated.
 * If invoked multiple times, a space is put between strings.
 */
static void
buf_append(struct buf *buf, const char *cp)
{
	size_t		 sz;

	if (0 == (sz = strlen(cp)))
		return;

	if (buf->len)
		buf->cp[(int)buf->len - 1] = ' ';

	buf_appendb(buf, cp, sz + 1);
}

/*
 * Recursively add all text from a given node.  
 * This is optimised for general mdoc nodes in this context, which do
 * not consist of subexpressions and having a recursive call for n->next
 * would be wasteful.
 * The "f" variable should be 0 unless called from pmdoc_Nd for the
 * description buffer, which does not start at the beginning of the
 * buffer.
 */
static void
buf_appendmdoc(struct buf *buf, const struct mdoc_node *n, int f)
{

	for ( ; n; n = n->next) {
		if (n->child)
			buf_appendmdoc(buf, n->child, f);

		if (MDOC_TEXT == n->type && f) {
			f = 0;
			buf_appendb(buf, n->string, 
					strlen(n->string) + 1);
		} else if (MDOC_TEXT == n->type)
			buf_append(buf, n->string);

	}
}

static void
hash_reset(DB **db)
{
	DB		*hash;

	if (NULL != (hash = *db))
		(*hash->close)(hash);

	*db = dbopen(NULL, O_CREAT|O_RDWR, 0644, DB_HASH, NULL);
	if (NULL == *db) {
		perror("hash");
		exit((int)MANDOCLEVEL_SYSERR);
	}
}

/* ARGSUSED */
static int
pmdoc_head(MDOC_ARGS)
{

	return(MDOC_HEAD == n->type);
}

/* ARGSUSED */
static int
pmdoc_body(MDOC_ARGS)
{

	return(MDOC_BODY == n->type);
}

/* ARGSUSED */
static int
pmdoc_Fd(MDOC_ARGS)
{
	const char	*start, *end;
	size_t		 sz;

	if (SEC_SYNOPSIS != n->sec)
		return(0);
	if (NULL == (n = n->child) || MDOC_TEXT != n->type)
		return(0);

	/*
	 * Only consider those `Fd' macro fields that begin with an
	 * "inclusion" token (versus, e.g., #define).
	 */
	if (strcmp("#include", n->string))
		return(0);

	if (NULL == (n = n->next) || MDOC_TEXT != n->type)
		return(0);

	/*
	 * Strip away the enclosing angle brackets and make sure we're
	 * not zero-length.
	 */

	start = n->string;
	if ('<' == *start || '"' == *start)
		start++;

	if (0 == (sz = strlen(start)))
		return(0);

	end = &start[(int)sz - 1];
	if ('>' == *end || '"' == *end)
		end--;

	assert(end >= start);

	buf_appendb(buf, start, (size_t)(end - start + 1));
	buf_appendb(buf, "", 1);
	return(1);
}

/* ARGSUSED */
static int
pmdoc_In(MDOC_ARGS)
{

	if (NULL == n->child || MDOC_TEXT != n->child->type)
		return(0);

	buf_append(buf, n->child->string);
	return(1);
}

/* ARGSUSED */
static int
pmdoc_Fn(MDOC_ARGS)
{
	struct mdoc_node *nn;
	const char	*cp;

	nn = n->child;

	if (NULL == nn || MDOC_TEXT != nn->type)
		return(0);

	/* .Fn "struct type *name" "char *arg" */

	cp = strrchr(nn->string, ' ');
	if (NULL == cp)
		cp = nn->string;

	/* Strip away pointer symbol. */

	while ('*' == *cp)
		cp++;

	/* Store the function name. */

	buf_append(buf, cp);
	hash_put(hash, buf, TYPE_Fn);

	/* Store the function type. */

	if (nn->string < cp) {
		buf->len = 0;
		buf_appendb(buf, nn->string, cp - nn->string);
		buf_appendb(buf, "", 1);
		hash_put(hash, buf, TYPE_Ft);
	}

	/* Store the arguments. */

	for (nn = nn->next; nn; nn = nn->next) {
		if (MDOC_TEXT != nn->type)
			continue;
		buf->len = 0;
		buf_append(buf, nn->string);
		hash_put(hash, buf, TYPE_Fa);
	}

	return(0);
}

/* ARGSUSED */
static int
pmdoc_St(MDOC_ARGS)
{

	if (NULL == n->child || MDOC_TEXT != n->child->type)
		return(0);

	buf_append(buf, n->child->string);
	return(1);
}

/* ARGSUSED */
static int
pmdoc_Xr(MDOC_ARGS)
{

	if (NULL == (n = n->child))
		return(0);

	buf_appendb(buf, n->string, strlen(n->string));

	if (NULL != (n = n->next)) {
		buf_appendb(buf, ".", 1);
		buf_appendb(buf, n->string, strlen(n->string) + 1);
	} else
		buf_appendb(buf, ".", 2);

	return(1);
}

/* ARGSUSED */
static int
pmdoc_Nd(MDOC_ARGS)
{

	if (MDOC_BODY != n->type)
		return(0);

	buf_appendmdoc(dbuf, n->child, 1);
	return(1);
}

/* ARGSUSED */
static int
pmdoc_Nm(MDOC_ARGS)
{

	if (SEC_NAME == n->sec)
		return(1);
	else if (SEC_SYNOPSIS != n->sec || MDOC_HEAD != n->type)
		return(0);

	if (NULL == n->child)
		buf_append(buf, m->name);

	return(1);
}

/* ARGSUSED */
static int
pmdoc_Sh(MDOC_ARGS)
{

	return(SEC_CUSTOM == n->sec && MDOC_HEAD == n->type);
}

static void
hash_put(DB *db, const struct buf *buf, uint64_t mask)
{
	uint64_t	 oldmask;
	DBT		 key, val;
	int		 rc;

	if (buf->len < 2)
		return;

	key.data = buf->cp;
	key.size = buf->len;

	if ((rc = (*db->get)(db, &key, &val, 0)) < 0) {
		perror("hash");
		exit((int)MANDOCLEVEL_SYSERR);
	} else if (0 == rc) {
		assert(sizeof(uint64_t) == val.size);
		memcpy(&oldmask, val.data, val.size);
		mask |= oldmask;
	}

	val.data = &mask;
	val.size = sizeof(uint64_t); 

	if ((rc = (*db->put)(db, &key, &val, 0)) < 0) {
		perror("hash");
		exit((int)MANDOCLEVEL_SYSERR);
	} 
}

static void
dbt_put(DB *db, const char *dbn, DBT *key, DBT *val)
{

	assert(key->size);
	assert(val->size);

	if (0 == (*db->put)(db, key, val, 0))
		return;
	
	perror(dbn);
	exit((int)MANDOCLEVEL_SYSERR);
	/* NOTREACHED */
}

/*
 * Call out to per-macro handlers after clearing the persistent database
 * key.  If the macro sets the database key, flush it to the database.
 */
static void
pmdoc_node(MDOC_ARGS)
{

	if (NULL == n)
		return;

	switch (n->type) {
	case (MDOC_HEAD):
		/* FALLTHROUGH */
	case (MDOC_BODY):
		/* FALLTHROUGH */
	case (MDOC_TAIL):
		/* FALLTHROUGH */
	case (MDOC_BLOCK):
		/* FALLTHROUGH */
	case (MDOC_ELEM):
		buf->len = 0;

		/*
		 * Both NULL handlers and handlers returning true
		 * request using the data.  Only skip the element
		 * when the handler returns false.
		 */

		if (NULL != mdocs[n->tok].fp &&
		    0 == (*mdocs[n->tok].fp)(hash, buf, dbuf, n, m))
			break;

		/*
		 * For many macros, use the text from all children.
		 * Set zero flags for macros not needing this.
		 * In that case, the handler must fill the buffer.
		 */

		if (MDOCF_CHILD & mdocs[n->tok].flags)
			buf_appendmdoc(buf, n->child, 0);

		/*
		 * Cover the most common case:
		 * Automatically stage one string per element.
		 * Set a zero mask for macros not needing this.
		 * Additional staging can be done in the handler.
		 */

		if (mdocs[n->tok].mask)
			hash_put(hash, buf, mdocs[n->tok].mask);
		break;
	default:
		break;
	}

	pmdoc_node(hash, buf, dbuf, n->child, m);
	pmdoc_node(hash, buf, dbuf, n->next, m);
}

static int
pman_node(MAN_ARGS)
{
	const struct man_node *head, *body;
	char		*start, *sv, *title;
	size_t		 sz, titlesz;

	if (NULL == n)
		return(0);

	/*
	 * We're only searching for one thing: the first text child in
	 * the BODY of a NAME section.  Since we don't keep track of
	 * sections in -man, run some hoops to find out whether we're in
	 * the correct section or not.
	 */

	if (MAN_BODY == n->type && MAN_SH == n->tok) {
		body = n;
		assert(body->parent);
		if (NULL != (head = body->parent->head) &&
				1 == head->nchild &&
				NULL != (head = (head->child)) &&
				MAN_TEXT == head->type &&
				0 == strcmp(head->string, "NAME") &&
				NULL != (body = body->child) &&
				MAN_TEXT == body->type) {

			title = NULL;
			titlesz = 0;
			/*
			 * Suck the entire NAME section into memory.
			 * Yes, we might run away.
			 * But too many manuals have big, spread-out
			 * NAME sections over many lines.
			 */
			for ( ; NULL != body; body = body->next) {
				if (MAN_TEXT != body->type)
					break;
				if (0 == (sz = strlen(body->string)))
					continue;
				title = mandoc_realloc
					(title, titlesz + sz + 1);
				memcpy(title + titlesz, body->string, sz);
				titlesz += sz + 1;
				title[(int)titlesz - 1] = ' ';
			}
			if (NULL == title)
				return(0);

			title = mandoc_realloc(title, titlesz + 1);
			title[(int)titlesz] = '\0';

			/* Skip leading space.  */

			sv = title;
			while (isspace((unsigned char)*sv))
				sv++;

			if (0 == (sz = strlen(sv))) {
				free(title);
				return(0);
			}

			/* Erase trailing space. */

			start = &sv[sz - 1];
			while (start > sv && isspace((unsigned char)*start))
				*start-- = '\0';

			if (start == sv) {
				free(title);
				return(0);
			}

			start = sv;

			/* 
			 * Go through a special heuristic dance here.
			 * This is why -man manuals are great!
			 * (I'm being sarcastic: my eyes are bleeding.)
			 * Conventionally, one or more manual names are
			 * comma-specified prior to a whitespace, then a
			 * dash, then a description.  Try to puzzle out
			 * the name parts here.
			 */

			for ( ;; ) {
				sz = strcspn(start, " ,");
				if ('\0' == start[(int)sz])
					break;

				buf->len = 0;
				buf_appendb(buf, start, sz);
				buf_appendb(buf, "", 1);

				hash_put(hash, buf, TYPE_Nm);

				if (' ' == start[(int)sz]) {
					start += (int)sz + 1;
					break;
				}

				assert(',' == start[(int)sz]);
				start += (int)sz + 1;
				while (' ' == *start)
					start++;
			}

			buf->len = 0;

			if (sv == start) {
				buf_append(buf, start);
				free(title);
				return(1);
			}

			while (isspace((unsigned char)*start))
				start++;

			if (0 == strncmp(start, "-", 1))
				start += 1;
			else if (0 == strncmp(start, "\\-\\-", 4))
				start += 4;
			else if (0 == strncmp(start, "\\-", 2))
				start += 2;
			else if (0 == strncmp(start, "\\(en", 4))
				start += 4;
			else if (0 == strncmp(start, "\\(em", 4))
				start += 4;

			while (' ' == *start)
				start++;

			sz = strlen(start) + 1;
			buf_appendb(dbuf, start, sz);
			buf_appendb(buf, start, sz);

			hash_put(hash, buf, TYPE_Nd);
			free(title);
		}
	}

	for (n = n->child; n; n = n->next)
		if (pman_node(hash, buf, dbuf, n))
			return(1);

	return(0);
}

/*
 * Parse a formatted manual page.
 * By necessity, this involves rather crude guesswork.
 */
static void
pformatted(DB *hash, struct buf *buf, 
		struct buf *dbuf, const struct of *of)
{
	FILE		*stream;
	char		*line, *p, *title;
	size_t		 len, plen, titlesz;

	if (NULL == (stream = fopen(of->fname, "r"))) {
		if (warnings)
			perror(of->fname);
		return;
	}

	/*
	 * Always use the title derived from the filename up front,
	 * do not even try to find it in the file.  This also makes
	 * sure we don't end up with an orphan index record, even if
	 * the file content turns out to be completely unintelligible.
	 */

	buf->len = 0;
	buf_append(buf, of->title);
	hash_put(hash, buf, TYPE_Nm);

	/* Skip to first blank line. */

	while (NULL != (line = fgetln(stream, &len)))
		if ('\n' == *line)
			break;

	/*
	 * Assume the first line that is not indented
	 * is the first section header.  Skip to it.
	 */

	while (NULL != (line = fgetln(stream, &len)))
		if ('\n' != *line && ' ' != *line)
			break;
	
	/*
	 * Read up until the next section into a buffer.
	 * Strip the leading and trailing newline from each read line,
	 * appending a trailing space.
	 * Ignore empty (whitespace-only) lines.
	 */

	titlesz = 0;
	title = NULL;

	while (NULL != (line = fgetln(stream, &len))) {
		if (' ' != *line || '\n' != line[(int)len - 1])
			break;
		while (len > 0 && isspace((unsigned char)*line)) {
			line++;
			len--;
		}
		if (1 == len)
			continue;
		title = mandoc_realloc(title, titlesz + len);
		memcpy(title + titlesz, line, len);
		titlesz += len;
		title[(int)titlesz - 1] = ' ';
	}


	/*
	 * If no page content can be found, or the input line
	 * is already the next section header, or there is no
	 * trailing newline, reuse the page title as the page
	 * description.
	 */

	if (NULL == title || '\0' == *title) {
		if (warnings)
			fprintf(stderr, "%s: cannot find NAME section\n",
					of->fname);
		buf_appendb(dbuf, buf->cp, buf->size);
		hash_put(hash, buf, TYPE_Nd);
		fclose(stream);
		free(title);
		return;
	}

	title = mandoc_realloc(title, titlesz + 1);
	title[(int)titlesz] = '\0';

	/*
	 * Skip to the first dash.
	 * Use the remaining line as the description (no more than 70
	 * bytes).
	 */

	if (NULL != (p = strstr(title, "- "))) {
		for (p += 2; ' ' == *p || '\b' == *p; p++)
			/* Skip to next word. */ ;
	} else {
		if (warnings)
			fprintf(stderr, "%s: no dash in title line\n",
					of->fname);
		p = title;
	}

	plen = strlen(p);

	/* Strip backspace-encoding from line. */

	while (NULL != (line = memchr(p, '\b', plen))) {
		len = line - p;
		if (0 == len) {
			memmove(line, line + 1, plen--);
			continue;
		} 
		memmove(line - 1, line + 1, plen - len);
		plen -= 2;
	}

	buf_appendb(dbuf, p, plen + 1);
	buf->len = 0;
	buf_appendb(buf, p, plen + 1);
	hash_put(hash, buf, TYPE_Nd);
	fclose(stream);
	free(title);
}

static void
ofile_argbuild(int argc, char *argv[], struct of **of,
		const char *basedir)
{
	char		 buf[PATH_MAX];
	char		 pbuf[PATH_MAX];
	const char	*sec, *arch, *title;
	char		*relpath, *p;
	int		 i, src_form;
	struct of	*nof;

	for (i = 0; i < argc; i++) {
		if (NULL == (relpath = realpath(argv[i], pbuf))) {
			perror(argv[i]);
			continue;
		}
		if (NULL != basedir) {
			if (strstr(pbuf, basedir) != pbuf) {
				fprintf(stderr, "%s: file outside "
				    "base directory %s\n",
				    pbuf, basedir);
				continue;
			}
			relpath = pbuf + strlen(basedir);
		}

		/*
		 * Try to infer the manual section, architecture and
		 * page title from the path, assuming it looks like
		 *   man*[/<arch>]/<title>.<section>   or
		 *   cat<section>[/<arch>]/<title>.0
		 */

		if (strlcpy(buf, relpath, sizeof(buf)) >= sizeof(buf)) {
			fprintf(stderr, "%s: path too long\n", relpath);
			continue;
		}
		sec = arch = title = "";
		src_form = 0;
		p = strrchr(buf, '\0');
		while (p-- > buf) {
			if ('\0' == *sec && '.' == *p) {
				sec = p + 1;
				*p = '\0';
				if ('0' == *sec)
					src_form |= MANDOC_FORM;
				else if ('1' <= *sec && '9' >= *sec)
					src_form |= MANDOC_SRC;
				continue;
			}
			if ('/' != *p)
				continue;
			if ('\0' == *title) {
				title = p + 1;
				*p = '\0';
				continue;
			}
			if (0 == strncmp("man", p + 1, 3))
				src_form |= MANDOC_SRC;
			else if (0 == strncmp("cat", p + 1, 3))
				src_form |= MANDOC_FORM;
			else
				arch = p + 1;
			break;
		}
		if ('\0' == *title) {
			if (warnings)
				fprintf(stderr,
				    "%s: cannot deduce title "
				    "from filename\n",
				    relpath);
			title = buf;
		}

		/*
		 * Build the file structure.
		 */

		nof = mandoc_calloc(1, sizeof(struct of));
		nof->fname = mandoc_strdup(relpath);
		nof->sec = mandoc_strdup(sec);
		nof->arch = mandoc_strdup(arch);
		nof->title = mandoc_strdup(title);
		nof->src_form = src_form;

		/*
		 * Add the structure to the list.
		 */

		if (NULL == *of) {
			*of = nof;
			(*of)->first = nof;
		} else {
			nof->first = (*of)->first;
			(*of)->next = nof;
			*of = nof;
		}
	}
}

/*
 * Recursively build up a list of files to parse.
 * We use this instead of ftw() and so on because I don't want global
 * variables hanging around.
 * This ignores the mandoc.db and mandoc.index files, but assumes that
 * everything else is a manual.
 * Pass in a pointer to a NULL structure for the first invocation.
 */
static void
ofile_dirbuild(const char *dir, const char* psec, const char *parch,
		int p_src_form, struct of **of)
{
	char		 buf[PATH_MAX];
#if defined(__sun)
	struct stat	 sb;
#endif
	size_t		 sz;
	DIR		*d;
	const char	*fn, *sec, *arch;
	char		*p, *q, *suffix;
	struct of	*nof;
	struct dirent	*dp;
	int		 src_form;

	if (NULL == (d = opendir(dir))) {
		if (warnings)
			perror(dir);
		return;
	}

	while (NULL != (dp = readdir(d))) {
		fn = dp->d_name;

		if ('.' == *fn)
			continue;

		src_form = p_src_form;

#if defined(__sun)
		stat(dp->d_name, &sb);
		if (S_IFDIR & sb.st_mode) {
#else
		if (DT_DIR == dp->d_type) {
#endif
			sec = psec;
			arch = parch;

			/*
			 * By default, only use directories called:
			 *   man<section>/[<arch>/]   or
			 *   cat<section>/[<arch>/]
			 */

			if ('\0' == *sec) {
				if(0 == strncmp("man", fn, 3)) {
					src_form |= MANDOC_SRC;
					sec = fn + 3;
				} else if (0 == strncmp("cat", fn, 3)) {
					src_form |= MANDOC_FORM;
					sec = fn + 3;
				} else {
					if (warnings) fprintf(stderr,
					    "%s/%s: bad section\n",
					    dir, fn);
					if (use_all)
						sec = fn;
					else
						continue;
				}
			} else if ('\0' == *arch) {
				if (NULL != strchr(fn, '.')) {
					if (warnings) fprintf(stderr,
					    "%s/%s: bad architecture\n",
					    dir, fn);
					if (0 == use_all)
						continue;
				}
				arch = fn;
			} else {
				if (warnings) fprintf(stderr, "%s/%s: "
				    "excessive subdirectory\n", dir, fn);
				if (0 == use_all)
					continue;
			}

			buf[0] = '\0';
			strlcat(buf, dir, PATH_MAX);
			strlcat(buf, "/", PATH_MAX);
			sz = strlcat(buf, fn, PATH_MAX);

			if (PATH_MAX <= sz) {
				if (warnings) fprintf(stderr, "%s/%s: "
				    "path too long\n", dir, fn);
				continue;
			}

			ofile_dirbuild(buf, sec, arch, src_form, of);
			continue;
		}

#if defined(__sun)
		if (0 == S_IFREG & sb.st_mode) {
#else
		if (DT_REG != dp->d_type) {
#endif
			if (warnings)
				fprintf(stderr,
				    "%s/%s: not a regular file\n",
				    dir, fn);
			continue;
		}
		if (!strcmp(MANDOC_DB, fn) || !strcmp(MANDOC_IDX, fn))
			continue;
		if ('\0' == *psec) {
			if (warnings)
				fprintf(stderr,
				    "%s/%s: file outside section\n",
				    dir, fn);
			if (0 == use_all)
				continue;
		}

		/*
		 * By default, skip files where the file name suffix
		 * does not agree with the section directory
		 * they are located in.
		 */

		suffix = strrchr(fn, '.');
		if (NULL == suffix) {
			if (warnings)
				fprintf(stderr,
				    "%s/%s: no filename suffix\n",
				    dir, fn);
			if (0 == use_all)
				continue;
		} else if ((MANDOC_SRC & src_form &&
				strcmp(suffix + 1, psec)) ||
			    (MANDOC_FORM & src_form &&
				strcmp(suffix + 1, "0"))) {
			if (warnings)
				fprintf(stderr,
				    "%s/%s: wrong filename suffix\n",
				    dir, fn);
			if (0 == use_all)
				continue;
			if ('0' == suffix[1])
				src_form |= MANDOC_FORM;
			else if ('1' <= suffix[1] && '9' >= suffix[1])
				src_form |= MANDOC_SRC;
		}

		/*
		 * Skip formatted manuals if a source version is
		 * available.  Ignore the age: it is very unlikely
		 * that people install newer formatted base manuals
		 * when they used to have source manuals before,
		 * and in ports, old manuals get removed on update.
		 */
		if (0 == use_all && MANDOC_FORM & src_form &&
				'\0' != *psec) {
			buf[0] = '\0';
			strlcat(buf, dir, PATH_MAX);
			p = strrchr(buf, '/');
			if ('\0' != *parch && NULL != p)
				for (p--; p > buf; p--)
					if ('/' == *p)
						break;
			if (NULL == p)
				p = buf;
			else
				p++;
			if (0 == strncmp("cat", p, 3))
				memcpy(p, "man", 3);
			strlcat(buf, "/", PATH_MAX);
			sz = strlcat(buf, fn, PATH_MAX);
			if (sz >= PATH_MAX) {
				if (warnings) fprintf(stderr,
				    "%s/%s: path too long\n",
				    dir, fn);
				continue;
			}
			q = strrchr(buf, '.');
			if (NULL != q && p < q++) {
				*q = '\0';
				sz = strlcat(buf, psec, PATH_MAX);
				if (sz >= PATH_MAX) {
					if (warnings) fprintf(stderr,
					    "%s/%s: path too long\n",
					    dir, fn);
					continue;
				}
				if (0 == access(buf, R_OK))
					continue;
			}
		}

		buf[0] = '\0';
		assert('.' == dir[0]);
		if ('/' == dir[1]) {
			strlcat(buf, dir + 2, PATH_MAX);
			strlcat(buf, "/", PATH_MAX);
		}
		sz = strlcat(buf, fn, PATH_MAX);
		if (sz >= PATH_MAX) {
			if (warnings) fprintf(stderr,
			    "%s/%s: path too long\n", dir, fn);
			continue;
		}

		nof = mandoc_calloc(1, sizeof(struct of));
		nof->fname = mandoc_strdup(buf);
		nof->sec = mandoc_strdup(psec);
		nof->arch = mandoc_strdup(parch);
		nof->src_form = src_form;

		/*
		 * Remember the file name without the extension,
		 * to be used as the page title in the database.
		 */

		if (NULL != suffix)
			*suffix = '\0';
		nof->title = mandoc_strdup(fn);

		/*
		 * Add the structure to the list.
		 */

		if (NULL == *of) {
			*of = nof;
			(*of)->first = nof;
		} else {
			nof->first = (*of)->first;
			(*of)->next = nof;
			*of = nof;
		}
	}

	closedir(d);
}

static void
ofile_free(struct of *of)
{
	struct of	*nof;

	if (NULL != of)
		of = of->first;

	while (NULL != of) {
		nof = of->next;
		free(of->fname);
		free(of->sec);
		free(of->arch);
		free(of->title);
		free(of);
		of = nof;
	}
}
