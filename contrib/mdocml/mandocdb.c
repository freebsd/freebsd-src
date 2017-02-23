/*	$Id: mandocdb.c,v 1.215 2016/01/08 17:48:09 schwarze Exp $ */
/*
 * Copyright (c) 2011, 2012 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011-2016 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <assert.h>
#include <ctype.h>
#if HAVE_ERR
#include <err.h>
#endif
#include <errno.h>
#include <fcntl.h>
#if HAVE_FTS
#include <fts.h>
#else
#include "compat_fts.h"
#endif
#include <getopt.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sqlite3.h>

#include "mandoc_aux.h"
#include "mandoc_ohash.h"
#include "mandoc.h"
#include "roff.h"
#include "mdoc.h"
#include "man.h"
#include "manconf.h"
#include "mansearch.h"

extern int mansearch_keymax;
extern const char *const mansearch_keynames[];

#define	SQL_EXEC(_v) \
	if (SQLITE_OK != sqlite3_exec(db, (_v), NULL, NULL, NULL)) \
		say("", "%s: %s", (_v), sqlite3_errmsg(db))
#define	SQL_BIND_TEXT(_s, _i, _v) \
	if (SQLITE_OK != sqlite3_bind_text \
		((_s), (_i)++, (_v), -1, SQLITE_STATIC)) \
		say(mlink->file, "%s", sqlite3_errmsg(db))
#define	SQL_BIND_INT(_s, _i, _v) \
	if (SQLITE_OK != sqlite3_bind_int \
		((_s), (_i)++, (_v))) \
		say(mlink->file, "%s", sqlite3_errmsg(db))
#define	SQL_BIND_INT64(_s, _i, _v) \
	if (SQLITE_OK != sqlite3_bind_int64 \
		((_s), (_i)++, (_v))) \
		say(mlink->file, "%s", sqlite3_errmsg(db))
#define SQL_STEP(_s) \
	if (SQLITE_DONE != sqlite3_step((_s))) \
		say(mlink->file, "%s", sqlite3_errmsg(db))

enum	op {
	OP_DEFAULT = 0, /* new dbs from dir list or default config */
	OP_CONFFILE, /* new databases from custom config file */
	OP_UPDATE, /* delete/add entries in existing database */
	OP_DELETE, /* delete entries from existing database */
	OP_TEST /* change no databases, report potential problems */
};

struct	str {
	const struct mpage *mpage; /* if set, the owning parse */
	uint64_t	 mask; /* bitmask in sequence */
	char		 key[]; /* rendered text */
};

struct	inodev {
	ino_t		 st_ino;
	dev_t		 st_dev;
};

struct	mpage {
	struct inodev	 inodev;  /* used for hashing routine */
	int64_t		 pageid;  /* pageid in mpages SQL table */
	char		*sec;     /* section from file content */
	char		*arch;    /* architecture from file content */
	char		*title;   /* title from file content */
	char		*desc;    /* description from file content */
	struct mpage	*next;    /* singly linked list */
	struct mlink	*mlinks;  /* singly linked list */
	int		 form;    /* format from file content */
	int		 name_head_done;
};

struct	mlink {
	char		 file[PATH_MAX]; /* filename rel. to manpath */
	char		*dsec;    /* section from directory */
	char		*arch;    /* architecture from directory */
	char		*name;    /* name from file name (not empty) */
	char		*fsec;    /* section from file name suffix */
	struct mlink	*next;    /* singly linked list */
	struct mpage	*mpage;   /* parent */
	int		 dform;   /* format from directory */
	int		 fform;   /* format from file name suffix */
	int		 gzip;	  /* filename has a .gz suffix */
};

enum	stmt {
	STMT_DELETE_PAGE = 0,	/* delete mpage */
	STMT_INSERT_PAGE,	/* insert mpage */
	STMT_INSERT_LINK,	/* insert mlink */
	STMT_INSERT_NAME,	/* insert name */
	STMT_SELECT_NAME,	/* retrieve existing name flags */
	STMT_INSERT_KEY,	/* insert parsed key */
	STMT__MAX
};

typedef	int (*mdoc_fp)(struct mpage *, const struct roff_meta *,
			const struct roff_node *);

struct	mdoc_handler {
	mdoc_fp		 fp; /* optional handler */
	uint64_t	 mask;  /* set unless handler returns 0 */
};

static	void	 dbclose(int);
static	void	 dbadd(struct mpage *);
static	void	 dbadd_mlink(const struct mlink *mlink);
static	void	 dbadd_mlink_name(const struct mlink *mlink);
static	int	 dbopen(int);
static	void	 dbprune(void);
static	void	 filescan(const char *);
static	int	 fts_compare(const FTSENT *const *, const FTSENT *const *);
static	void	 mlink_add(struct mlink *, const struct stat *);
static	void	 mlink_check(struct mpage *, struct mlink *);
static	void	 mlink_free(struct mlink *);
static	void	 mlinks_undupe(struct mpage *);
static	void	 mpages_free(void);
static	void	 mpages_merge(struct mparse *);
static	void	 names_check(void);
static	void	 parse_cat(struct mpage *, int);
static	void	 parse_man(struct mpage *, const struct roff_meta *,
			const struct roff_node *);
static	void	 parse_mdoc(struct mpage *, const struct roff_meta *,
			const struct roff_node *);
static	int	 parse_mdoc_head(struct mpage *, const struct roff_meta *,
			const struct roff_node *);
static	int	 parse_mdoc_Fd(struct mpage *, const struct roff_meta *,
			const struct roff_node *);
static	void	 parse_mdoc_fname(struct mpage *, const struct roff_node *);
static	int	 parse_mdoc_Fn(struct mpage *, const struct roff_meta *,
			const struct roff_node *);
static	int	 parse_mdoc_Fo(struct mpage *, const struct roff_meta *,
			const struct roff_node *);
static	int	 parse_mdoc_Nd(struct mpage *, const struct roff_meta *,
			const struct roff_node *);
static	int	 parse_mdoc_Nm(struct mpage *, const struct roff_meta *,
			const struct roff_node *);
static	int	 parse_mdoc_Sh(struct mpage *, const struct roff_meta *,
			const struct roff_node *);
static	int	 parse_mdoc_Va(struct mpage *, const struct roff_meta *,
			const struct roff_node *);
static	int	 parse_mdoc_Xr(struct mpage *, const struct roff_meta *,
			const struct roff_node *);
static	void	 putkey(const struct mpage *, char *, uint64_t);
static	void	 putkeys(const struct mpage *, char *, size_t, uint64_t);
static	void	 putmdockey(const struct mpage *,
			const struct roff_node *, uint64_t);
static	int	 render_string(char **, size_t *);
static	void	 say(const char *, const char *, ...);
static	int	 set_basedir(const char *, int);
static	int	 treescan(void);
static	size_t	 utf8(unsigned int, char [7]);

static	char		 tempfilename[32];
static	int		 nodb; /* no database changes */
static	int		 mparse_options; /* abort the parse early */
static	int		 use_all; /* use all found files */
static	int		 debug; /* print what we're doing */
static	int		 warnings; /* warn about crap */
static	int		 write_utf8; /* write UTF-8 output; else ASCII */
static	int		 exitcode; /* to be returned by main */
static	enum op		 op; /* operational mode */
static	char		 basedir[PATH_MAX]; /* current base directory */
static	struct ohash	 mpages; /* table of distinct manual pages */
static	struct ohash	 mlinks; /* table of directory entries */
static	struct ohash	 names; /* table of all names */
static	struct ohash	 strings; /* table of all strings */
static	sqlite3		*db = NULL; /* current database */
static	sqlite3_stmt	*stmts[STMT__MAX]; /* current statements */
static	uint64_t	 name_mask;
static	struct mpage	*mpage_head;

static	const struct mdoc_handler mdocs[MDOC_MAX] = {
	{ NULL, 0 },  /* Ap */
	{ NULL, 0 },  /* Dd */
	{ NULL, 0 },  /* Dt */
	{ NULL, 0 },  /* Os */
	{ parse_mdoc_Sh, TYPE_Sh }, /* Sh */
	{ parse_mdoc_head, TYPE_Ss }, /* Ss */
	{ NULL, 0 },  /* Pp */
	{ NULL, 0 },  /* D1 */
	{ NULL, 0 },  /* Dl */
	{ NULL, 0 },  /* Bd */
	{ NULL, 0 },  /* Ed */
	{ NULL, 0 },  /* Bl */
	{ NULL, 0 },  /* El */
	{ NULL, 0 },  /* It */
	{ NULL, 0 },  /* Ad */
	{ NULL, TYPE_An },  /* An */
	{ NULL, TYPE_Ar },  /* Ar */
	{ NULL, TYPE_Cd },  /* Cd */
	{ NULL, TYPE_Cm },  /* Cm */
	{ NULL, TYPE_Dv },  /* Dv */
	{ NULL, TYPE_Er },  /* Er */
	{ NULL, TYPE_Ev },  /* Ev */
	{ NULL, 0 },  /* Ex */
	{ NULL, TYPE_Fa },  /* Fa */
	{ parse_mdoc_Fd, 0 },  /* Fd */
	{ NULL, TYPE_Fl },  /* Fl */
	{ parse_mdoc_Fn, 0 },  /* Fn */
	{ NULL, TYPE_Ft },  /* Ft */
	{ NULL, TYPE_Ic },  /* Ic */
	{ NULL, TYPE_In },  /* In */
	{ NULL, TYPE_Li },  /* Li */
	{ parse_mdoc_Nd, 0 },  /* Nd */
	{ parse_mdoc_Nm, 0 },  /* Nm */
	{ NULL, 0 },  /* Op */
	{ NULL, 0 },  /* Ot */
	{ NULL, TYPE_Pa },  /* Pa */
	{ NULL, 0 },  /* Rv */
	{ NULL, TYPE_St },  /* St */
	{ parse_mdoc_Va, TYPE_Va },  /* Va */
	{ parse_mdoc_Va, TYPE_Vt },  /* Vt */
	{ parse_mdoc_Xr, 0 },  /* Xr */
	{ NULL, 0 },  /* %A */
	{ NULL, 0 },  /* %B */
	{ NULL, 0 },  /* %D */
	{ NULL, 0 },  /* %I */
	{ NULL, 0 },  /* %J */
	{ NULL, 0 },  /* %N */
	{ NULL, 0 },  /* %O */
	{ NULL, 0 },  /* %P */
	{ NULL, 0 },  /* %R */
	{ NULL, 0 },  /* %T */
	{ NULL, 0 },  /* %V */
	{ NULL, 0 },  /* Ac */
	{ NULL, 0 },  /* Ao */
	{ NULL, 0 },  /* Aq */
	{ NULL, TYPE_At },  /* At */
	{ NULL, 0 },  /* Bc */
	{ NULL, 0 },  /* Bf */
	{ NULL, 0 },  /* Bo */
	{ NULL, 0 },  /* Bq */
	{ NULL, TYPE_Bsx },  /* Bsx */
	{ NULL, TYPE_Bx },  /* Bx */
	{ NULL, 0 },  /* Db */
	{ NULL, 0 },  /* Dc */
	{ NULL, 0 },  /* Do */
	{ NULL, 0 },  /* Dq */
	{ NULL, 0 },  /* Ec */
	{ NULL, 0 },  /* Ef */
	{ NULL, TYPE_Em },  /* Em */
	{ NULL, 0 },  /* Eo */
	{ NULL, TYPE_Fx },  /* Fx */
	{ NULL, TYPE_Ms },  /* Ms */
	{ NULL, 0 },  /* No */
	{ NULL, 0 },  /* Ns */
	{ NULL, TYPE_Nx },  /* Nx */
	{ NULL, TYPE_Ox },  /* Ox */
	{ NULL, 0 },  /* Pc */
	{ NULL, 0 },  /* Pf */
	{ NULL, 0 },  /* Po */
	{ NULL, 0 },  /* Pq */
	{ NULL, 0 },  /* Qc */
	{ NULL, 0 },  /* Ql */
	{ NULL, 0 },  /* Qo */
	{ NULL, 0 },  /* Qq */
	{ NULL, 0 },  /* Re */
	{ NULL, 0 },  /* Rs */
	{ NULL, 0 },  /* Sc */
	{ NULL, 0 },  /* So */
	{ NULL, 0 },  /* Sq */
	{ NULL, 0 },  /* Sm */
	{ NULL, 0 },  /* Sx */
	{ NULL, TYPE_Sy },  /* Sy */
	{ NULL, TYPE_Tn },  /* Tn */
	{ NULL, 0 },  /* Ux */
	{ NULL, 0 },  /* Xc */
	{ NULL, 0 },  /* Xo */
	{ parse_mdoc_Fo, 0 },  /* Fo */
	{ NULL, 0 },  /* Fc */
	{ NULL, 0 },  /* Oo */
	{ NULL, 0 },  /* Oc */
	{ NULL, 0 },  /* Bk */
	{ NULL, 0 },  /* Ek */
	{ NULL, 0 },  /* Bt */
	{ NULL, 0 },  /* Hf */
	{ NULL, 0 },  /* Fr */
	{ NULL, 0 },  /* Ud */
	{ NULL, TYPE_Lb },  /* Lb */
	{ NULL, 0 },  /* Lp */
	{ NULL, TYPE_Lk },  /* Lk */
	{ NULL, TYPE_Mt },  /* Mt */
	{ NULL, 0 },  /* Brq */
	{ NULL, 0 },  /* Bro */
	{ NULL, 0 },  /* Brc */
	{ NULL, 0 },  /* %C */
	{ NULL, 0 },  /* Es */
	{ NULL, 0 },  /* En */
	{ NULL, TYPE_Dx },  /* Dx */
	{ NULL, 0 },  /* %Q */
	{ NULL, 0 },  /* br */
	{ NULL, 0 },  /* sp */
	{ NULL, 0 },  /* %U */
	{ NULL, 0 },  /* Ta */
	{ NULL, 0 },  /* ll */
};


int
mandocdb(int argc, char *argv[])
{
	struct manconf	  conf;
	struct mparse	 *mp;
	const char	 *path_arg, *progname;
	size_t		  j, sz;
	int		  ch, i;

#if HAVE_PLEDGE
	if (pledge("stdio rpath wpath cpath fattr flock proc exec", NULL) == -1) {
		warn("pledge");
		return (int)MANDOCLEVEL_SYSERR;
	}
#endif

	memset(&conf, 0, sizeof(conf));
	memset(stmts, 0, STMT__MAX * sizeof(sqlite3_stmt *));

	/*
	 * We accept a few different invocations.
	 * The CHECKOP macro makes sure that invocation styles don't
	 * clobber each other.
	 */
#define	CHECKOP(_op, _ch) do \
	if (OP_DEFAULT != (_op)) { \
		warnx("-%c: Conflicting option", (_ch)); \
		goto usage; \
	} while (/*CONSTCOND*/0)

	path_arg = NULL;
	op = OP_DEFAULT;

	while (-1 != (ch = getopt(argc, argv, "aC:Dd:npQT:tu:v")))
		switch (ch) {
		case 'a':
			use_all = 1;
			break;
		case 'C':
			CHECKOP(op, ch);
			path_arg = optarg;
			op = OP_CONFFILE;
			break;
		case 'D':
			debug++;
			break;
		case 'd':
			CHECKOP(op, ch);
			path_arg = optarg;
			op = OP_UPDATE;
			break;
		case 'n':
			nodb = 1;
			break;
		case 'p':
			warnings = 1;
			break;
		case 'Q':
			mparse_options |= MPARSE_QUICK;
			break;
		case 'T':
			if (strcmp(optarg, "utf8")) {
				warnx("-T%s: Unsupported output format",
				    optarg);
				goto usage;
			}
			write_utf8 = 1;
			break;
		case 't':
			CHECKOP(op, ch);
			dup2(STDOUT_FILENO, STDERR_FILENO);
			op = OP_TEST;
			nodb = warnings = 1;
			break;
		case 'u':
			CHECKOP(op, ch);
			path_arg = optarg;
			op = OP_DELETE;
			break;
		case 'v':
			/* Compatibility with espie@'s makewhatis. */
			break;
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;

#if HAVE_PLEDGE
	if (nodb) {
		if (pledge("stdio rpath", NULL) == -1) {
			warn("pledge");
			return (int)MANDOCLEVEL_SYSERR;
		}
	}
#endif

	if (OP_CONFFILE == op && argc > 0) {
		warnx("-C: Too many arguments");
		goto usage;
	}

	exitcode = (int)MANDOCLEVEL_OK;
	mchars_alloc();
	mp = mparse_alloc(mparse_options, MANDOCLEVEL_BADARG, NULL, NULL);
	mandoc_ohash_init(&mpages, 6, offsetof(struct mpage, inodev));
	mandoc_ohash_init(&mlinks, 6, offsetof(struct mlink, file));

	if (OP_UPDATE == op || OP_DELETE == op || OP_TEST == op) {

		/*
		 * Most of these deal with a specific directory.
		 * Jump into that directory first.
		 */
		if (OP_TEST != op && 0 == set_basedir(path_arg, 1))
			goto out;

		if (dbopen(1)) {
			/*
			 * The existing database is usable.  Process
			 * all files specified on the command-line.
			 */
#if HAVE_PLEDGE
			if (!nodb) {
				if (pledge("stdio rpath wpath cpath fattr flock", NULL) == -1) {
					warn("pledge");
					exitcode = (int)MANDOCLEVEL_SYSERR;
					goto out;
				}
			}
#endif
			use_all = 1;
			for (i = 0; i < argc; i++)
				filescan(argv[i]);
			if (OP_TEST != op)
				dbprune();
		} else {
			/*
			 * Database missing or corrupt.
			 * Recreate from scratch.
			 */
			exitcode = (int)MANDOCLEVEL_OK;
			op = OP_DEFAULT;
			if (0 == treescan())
				goto out;
			if (0 == dbopen(0))
				goto out;
		}
		if (OP_DELETE != op)
			mpages_merge(mp);
		dbclose(OP_DEFAULT == op ? 0 : 1);
	} else {
		/*
		 * If we have arguments, use them as our manpaths.
		 * If we don't, grok from manpath(1) or however else
		 * manconf_parse() wants to do it.
		 */
		if (argc > 0) {
			conf.manpath.paths = mandoc_reallocarray(NULL,
			    argc, sizeof(char *));
			conf.manpath.sz = (size_t)argc;
			for (i = 0; i < argc; i++)
				conf.manpath.paths[i] = mandoc_strdup(argv[i]);
		} else
			manconf_parse(&conf, path_arg, NULL, NULL);

		if (conf.manpath.sz == 0) {
			exitcode = (int)MANDOCLEVEL_BADARG;
			say("", "Empty manpath");
		}

		/*
		 * First scan the tree rooted at a base directory, then
		 * build a new database and finally move it into place.
		 * Ignore zero-length directories and strip trailing
		 * slashes.
		 */
		for (j = 0; j < conf.manpath.sz; j++) {
			sz = strlen(conf.manpath.paths[j]);
			if (sz && conf.manpath.paths[j][sz - 1] == '/')
				conf.manpath.paths[j][--sz] = '\0';
			if (0 == sz)
				continue;

			if (j) {
				mandoc_ohash_init(&mpages, 6,
				    offsetof(struct mpage, inodev));
				mandoc_ohash_init(&mlinks, 6,
				    offsetof(struct mlink, file));
			}

			if ( ! set_basedir(conf.manpath.paths[j], argc > 0))
				continue;
			if (0 == treescan())
				continue;
			if (0 == dbopen(0))
				continue;

			mpages_merge(mp);
			if (warnings && !nodb &&
			    ! (MPARSE_QUICK & mparse_options))
				names_check();
			dbclose(0);

			if (j + 1 < conf.manpath.sz) {
				mpages_free();
				ohash_delete(&mpages);
				ohash_delete(&mlinks);
			}
		}
	}
out:
	manconf_free(&conf);
	mparse_free(mp);
	mchars_free();
	mpages_free();
	ohash_delete(&mpages);
	ohash_delete(&mlinks);
	return exitcode;
usage:
	progname = getprogname();
	fprintf(stderr, "usage: %s [-aDnpQ] [-C file] [-Tutf8]\n"
			"       %s [-aDnpQ] [-Tutf8] dir ...\n"
			"       %s [-DnpQ] [-Tutf8] -d dir [file ...]\n"
			"       %s [-Dnp] -u dir [file ...]\n"
			"       %s [-Q] -t file ...\n",
		        progname, progname, progname, progname, progname);

	return (int)MANDOCLEVEL_BADARG;
}

static int
fts_compare(const FTSENT *const *a, const FTSENT *const *b)
{

	/*
	 * The mpage list is processed in the opposite order to which pages are
	 * added, so traverse the hierarchy in reverse alpha order, resulting
	 * in database inserts in alpha order. This is not required for correct
	 * operation, but is helpful when inspecting the database during
	 * development.
	 */
	return -strcmp((*a)->fts_name, (*b)->fts_name);
}

/*
 * Scan a directory tree rooted at "basedir" for manpages.
 * We use fts(), scanning directory parts along the way for clues to our
 * section and architecture.
 *
 * If use_all has been specified, grok all files.
 * If not, sanitise paths to the following:
 *
 *   [./]man*[/<arch>]/<name>.<section>
 *   or
 *   [./]cat<section>[/<arch>]/<name>.0
 *
 * TODO: accomodate for multi-language directories.
 */
static int
treescan(void)
{
	char		 buf[PATH_MAX];
	FTS		*f;
	FTSENT		*ff;
	struct mlink	*mlink;
	int		 dform, gzip;
	char		*dsec, *arch, *fsec, *cp;
	const char	*path;
	const char	*argv[2];

	argv[0] = ".";
	argv[1] = (char *)NULL;

	f = fts_open((char * const *)argv, FTS_PHYSICAL | FTS_NOCHDIR,
	    fts_compare);
	if (f == NULL) {
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say("", "&fts_open");
		return 0;
	}

	dsec = arch = NULL;
	dform = FORM_NONE;

	while ((ff = fts_read(f)) != NULL) {
		path = ff->fts_path + 2;
		switch (ff->fts_info) {

		/*
		 * Symbolic links require various sanity checks,
		 * then get handled just like regular files.
		 */
		case FTS_SL:
			if (realpath(path, buf) == NULL) {
				if (warnings)
					say(path, "&realpath");
				continue;
			}
			if (strstr(buf, basedir) != buf
#ifdef HOMEBREWDIR
			    && strstr(buf, HOMEBREWDIR) != buf
#endif
			) {
				if (warnings) say("",
				    "%s: outside base directory", buf);
				continue;
			}
			/* Use logical inode to avoid mpages dupe. */
			if (stat(path, ff->fts_statp) == -1) {
				if (warnings)
					say(path, "&stat");
				continue;
			}
			/* FALLTHROUGH */

		/*
		 * If we're a regular file, add an mlink by using the
		 * stored directory data and handling the filename.
		 */
		case FTS_F:
			if ( ! strcmp(path, MANDOC_DB))
				continue;
			if ( ! use_all && ff->fts_level < 2) {
				if (warnings)
					say(path, "Extraneous file");
				continue;
			}
			gzip = 0;
			fsec = NULL;
			while (fsec == NULL) {
				fsec = strrchr(ff->fts_name, '.');
				if (fsec == NULL || strcmp(fsec+1, "gz"))
					break;
				gzip = 1;
				*fsec = '\0';
				fsec = NULL;
			}
			if (fsec == NULL) {
				if ( ! use_all) {
					if (warnings)
						say(path,
						    "No filename suffix");
					continue;
				}
			} else if ( ! strcmp(++fsec, "html")) {
				if (warnings)
					say(path, "Skip html");
				continue;
			} else if ( ! strcmp(fsec, "ps")) {
				if (warnings)
					say(path, "Skip ps");
				continue;
			} else if ( ! strcmp(fsec, "pdf")) {
				if (warnings)
					say(path, "Skip pdf");
				continue;
			} else if ( ! use_all &&
			    ((dform == FORM_SRC &&
			      strncmp(fsec, dsec, strlen(dsec))) ||
			     (dform == FORM_CAT && strcmp(fsec, "0")))) {
				if (warnings)
					say(path, "Wrong filename suffix");
				continue;
			} else
				fsec[-1] = '\0';

			mlink = mandoc_calloc(1, sizeof(struct mlink));
			if (strlcpy(mlink->file, path,
			    sizeof(mlink->file)) >=
			    sizeof(mlink->file)) {
				say(path, "Filename too long");
				free(mlink);
				continue;
			}
			mlink->dform = dform;
			mlink->dsec = dsec;
			mlink->arch = arch;
			mlink->name = ff->fts_name;
			mlink->fsec = fsec;
			mlink->gzip = gzip;
			mlink_add(mlink, ff->fts_statp);
			continue;

		case FTS_D:
		case FTS_DP:
			break;

		default:
			if (warnings)
				say(path, "Not a regular file");
			continue;
		}

		switch (ff->fts_level) {
		case 0:
			/* Ignore the root directory. */
			break;
		case 1:
			/*
			 * This might contain manX/ or catX/.
			 * Try to infer this from the name.
			 * If we're not in use_all, enforce it.
			 */
			cp = ff->fts_name;
			if (ff->fts_info == FTS_DP) {
				dform = FORM_NONE;
				dsec = NULL;
				break;
			}

			if ( ! strncmp(cp, "man", 3)) {
				dform = FORM_SRC;
				dsec = cp + 3;
			} else if ( ! strncmp(cp, "cat", 3)) {
				dform = FORM_CAT;
				dsec = cp + 3;
			} else {
				dform = FORM_NONE;
				dsec = NULL;
			}

			if (dsec != NULL || use_all)
				break;

			if (warnings)
				say(path, "Unknown directory part");
			fts_set(f, ff, FTS_SKIP);
			break;
		case 2:
			/*
			 * Possibly our architecture.
			 * If we're descending, keep tabs on it.
			 */
			if (ff->fts_info != FTS_DP && dsec != NULL)
				arch = ff->fts_name;
			else
				arch = NULL;
			break;
		default:
			if (ff->fts_info == FTS_DP || use_all)
				break;
			if (warnings)
				say(path, "Extraneous directory part");
			fts_set(f, ff, FTS_SKIP);
			break;
		}
	}

	fts_close(f);
	return 1;
}

/*
 * Add a file to the mlinks table.
 * Do not verify that it's a "valid" looking manpage (we'll do that
 * later).
 *
 * Try to infer the manual section, architecture, and page name from the
 * path, assuming it looks like
 *
 *   [./]man*[/<arch>]/<name>.<section>
 *   or
 *   [./]cat<section>[/<arch>]/<name>.0
 *
 * See treescan() for the fts(3) version of this.
 */
static void
filescan(const char *file)
{
	char		 buf[PATH_MAX];
	struct stat	 st;
	struct mlink	*mlink;
	char		*p, *start;

	assert(use_all);

	if (0 == strncmp(file, "./", 2))
		file += 2;

	/*
	 * We have to do lstat(2) before realpath(3) loses
	 * the information whether this is a symbolic link.
	 * We need to know that because for symbolic links,
	 * we want to use the orginal file name, while for
	 * regular files, we want to use the real path.
	 */
	if (-1 == lstat(file, &st)) {
		exitcode = (int)MANDOCLEVEL_BADARG;
		say(file, "&lstat");
		return;
	} else if (0 == ((S_IFREG | S_IFLNK) & st.st_mode)) {
		exitcode = (int)MANDOCLEVEL_BADARG;
		say(file, "Not a regular file");
		return;
	}

	/*
	 * We have to resolve the file name to the real path
	 * in any case for the base directory check.
	 */
	if (NULL == realpath(file, buf)) {
		exitcode = (int)MANDOCLEVEL_BADARG;
		say(file, "&realpath");
		return;
	}

	if (OP_TEST == op)
		start = buf;
	else if (strstr(buf, basedir) == buf)
		start = buf + strlen(basedir);
#ifdef HOMEBREWDIR
	else if (strstr(buf, HOMEBREWDIR) == buf)
		start = buf;
#endif
	else {
		exitcode = (int)MANDOCLEVEL_BADARG;
		say("", "%s: outside base directory", buf);
		return;
	}

	/*
	 * Now we are sure the file is inside our tree.
	 * If it is a symbolic link, ignore the real path
	 * and use the original name.
	 * This implies passing stuff like "cat1/../man1/foo.1"
	 * on the command line won't work.  So don't do that.
	 * Note the stat(2) can still fail if the link target
	 * doesn't exist.
	 */
	if (S_IFLNK & st.st_mode) {
		if (-1 == stat(buf, &st)) {
			exitcode = (int)MANDOCLEVEL_BADARG;
			say(file, "&stat");
			return;
		}
		if (strlcpy(buf, file, sizeof(buf)) >= sizeof(buf)) {
			say(file, "Filename too long");
			return;
		}
		start = buf;
		if (OP_TEST != op && strstr(buf, basedir) == buf)
			start += strlen(basedir);
	}

	mlink = mandoc_calloc(1, sizeof(struct mlink));
	mlink->dform = FORM_NONE;
	if (strlcpy(mlink->file, start, sizeof(mlink->file)) >=
	    sizeof(mlink->file)) {
		say(start, "Filename too long");
		free(mlink);
		return;
	}

	/*
	 * First try to guess our directory structure.
	 * If we find a separator, try to look for man* or cat*.
	 * If we find one of these and what's underneath is a directory,
	 * assume it's an architecture.
	 */
	if (NULL != (p = strchr(start, '/'))) {
		*p++ = '\0';
		if (0 == strncmp(start, "man", 3)) {
			mlink->dform = FORM_SRC;
			mlink->dsec = start + 3;
		} else if (0 == strncmp(start, "cat", 3)) {
			mlink->dform = FORM_CAT;
			mlink->dsec = start + 3;
		}

		start = p;
		if (NULL != mlink->dsec && NULL != (p = strchr(start, '/'))) {
			*p++ = '\0';
			mlink->arch = start;
			start = p;
		}
	}

	/*
	 * Now check the file suffix.
	 * Suffix of `.0' indicates a catpage, `.1-9' is a manpage.
	 */
	p = strrchr(start, '\0');
	while (p-- > start && '/' != *p && '.' != *p)
		/* Loop. */ ;

	if ('.' == *p) {
		*p++ = '\0';
		mlink->fsec = p;
	}

	/*
	 * Now try to parse the name.
	 * Use the filename portion of the path.
	 */
	mlink->name = start;
	if (NULL != (p = strrchr(start, '/'))) {
		mlink->name = p + 1;
		*p = '\0';
	}
	mlink_add(mlink, &st);
}

static void
mlink_add(struct mlink *mlink, const struct stat *st)
{
	struct inodev	 inodev;
	struct mpage	*mpage;
	unsigned int	 slot;

	assert(NULL != mlink->file);

	mlink->dsec = mandoc_strdup(mlink->dsec ? mlink->dsec : "");
	mlink->arch = mandoc_strdup(mlink->arch ? mlink->arch : "");
	mlink->name = mandoc_strdup(mlink->name ? mlink->name : "");
	mlink->fsec = mandoc_strdup(mlink->fsec ? mlink->fsec : "");

	if ('0' == *mlink->fsec) {
		free(mlink->fsec);
		mlink->fsec = mandoc_strdup(mlink->dsec);
		mlink->fform = FORM_CAT;
	} else if ('1' <= *mlink->fsec && '9' >= *mlink->fsec)
		mlink->fform = FORM_SRC;
	else
		mlink->fform = FORM_NONE;

	slot = ohash_qlookup(&mlinks, mlink->file);
	assert(NULL == ohash_find(&mlinks, slot));
	ohash_insert(&mlinks, slot, mlink);

	memset(&inodev, 0, sizeof(inodev));  /* Clear padding. */
	inodev.st_ino = st->st_ino;
	inodev.st_dev = st->st_dev;
	slot = ohash_lookup_memory(&mpages, (char *)&inodev,
	    sizeof(struct inodev), inodev.st_ino);
	mpage = ohash_find(&mpages, slot);
	if (NULL == mpage) {
		mpage = mandoc_calloc(1, sizeof(struct mpage));
		mpage->inodev.st_ino = inodev.st_ino;
		mpage->inodev.st_dev = inodev.st_dev;
		mpage->next = mpage_head;
		mpage_head = mpage;
		ohash_insert(&mpages, slot, mpage);
	} else
		mlink->next = mpage->mlinks;
	mpage->mlinks = mlink;
	mlink->mpage = mpage;
}

static void
mlink_free(struct mlink *mlink)
{

	free(mlink->dsec);
	free(mlink->arch);
	free(mlink->name);
	free(mlink->fsec);
	free(mlink);
}

static void
mpages_free(void)
{
	struct mpage	*mpage;
	struct mlink	*mlink;

	while (NULL != (mpage = mpage_head)) {
		while (NULL != (mlink = mpage->mlinks)) {
			mpage->mlinks = mlink->next;
			mlink_free(mlink);
		}
		mpage_head = mpage->next;
		free(mpage->sec);
		free(mpage->arch);
		free(mpage->title);
		free(mpage->desc);
		free(mpage);
	}
}

/*
 * For each mlink to the mpage, check whether the path looks like
 * it is formatted, and if it does, check whether a source manual
 * exists by the same name, ignoring the suffix.
 * If both conditions hold, drop the mlink.
 */
static void
mlinks_undupe(struct mpage *mpage)
{
	char		  buf[PATH_MAX];
	struct mlink	**prev;
	struct mlink	 *mlink;
	char		 *bufp;

	mpage->form = FORM_CAT;
	prev = &mpage->mlinks;
	while (NULL != (mlink = *prev)) {
		if (FORM_CAT != mlink->dform) {
			mpage->form = FORM_NONE;
			goto nextlink;
		}
		(void)strlcpy(buf, mlink->file, sizeof(buf));
		bufp = strstr(buf, "cat");
		assert(NULL != bufp);
		memcpy(bufp, "man", 3);
		if (NULL != (bufp = strrchr(buf, '.')))
			*++bufp = '\0';
		(void)strlcat(buf, mlink->dsec, sizeof(buf));
		if (NULL == ohash_find(&mlinks,
		    ohash_qlookup(&mlinks, buf)))
			goto nextlink;
		if (warnings)
			say(mlink->file, "Man source exists: %s", buf);
		if (use_all)
			goto nextlink;
		*prev = mlink->next;
		mlink_free(mlink);
		continue;
nextlink:
		prev = &(*prev)->next;
	}
}

static void
mlink_check(struct mpage *mpage, struct mlink *mlink)
{
	struct str	*str;
	unsigned int	 slot;

	/*
	 * Check whether the manual section given in a file
	 * agrees with the directory where the file is located.
	 * Some manuals have suffixes like (3p) on their
	 * section number either inside the file or in the
	 * directory name, some are linked into more than one
	 * section, like encrypt(1) = makekey(8).
	 */

	if (FORM_SRC == mpage->form &&
	    strcasecmp(mpage->sec, mlink->dsec))
		say(mlink->file, "Section \"%s\" manual in %s directory",
		    mpage->sec, mlink->dsec);

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
	 */

	if (strcasecmp(mpage->arch, mlink->arch))
		say(mlink->file, "Architecture \"%s\" manual in "
		    "\"%s\" directory", mpage->arch, mlink->arch);

	/*
	 * XXX
	 * parse_cat() doesn't set NAME_TITLE yet.
	 */

	if (FORM_CAT == mpage->form)
		return;

	/*
	 * Check whether this mlink
	 * appears as a name in the NAME section.
	 */

	slot = ohash_qlookup(&names, mlink->name);
	str = ohash_find(&names, slot);
	assert(NULL != str);
	if ( ! (NAME_TITLE & str->mask))
		say(mlink->file, "Name missing in NAME section");
}

/*
 * Run through the files in the global vector "mpages"
 * and add them to the database specified in "basedir".
 *
 * This handles the parsing scheme itself, using the cues of directory
 * and filename to determine whether the file is parsable or not.
 */
static void
mpages_merge(struct mparse *mp)
{
	char			 any[] = "any";
	struct mpage		*mpage, *mpage_dest;
	struct mlink		*mlink, *mlink_dest;
	struct roff_man		*man;
	char			*sodest;
	char			*cp;
	int			 fd;

	if ( ! nodb)
		SQL_EXEC("BEGIN TRANSACTION");

	for (mpage = mpage_head; mpage != NULL; mpage = mpage->next) {
		mlinks_undupe(mpage);
		if ((mlink = mpage->mlinks) == NULL)
			continue;

		name_mask = NAME_MASK;
		mandoc_ohash_init(&names, 4, offsetof(struct str, key));
		mandoc_ohash_init(&strings, 6, offsetof(struct str, key));
		mparse_reset(mp);
		man = NULL;
		sodest = NULL;

		if ((fd = mparse_open(mp, mlink->file)) == -1) {
			say(mlink->file, "&open");
			goto nextpage;
		}

		/*
		 * Interpret the file as mdoc(7) or man(7) source
		 * code, unless it is known to be formatted.
		 */
		if (mlink->dform != FORM_CAT || mlink->fform != FORM_CAT) {
			mparse_readfd(mp, fd, mlink->file);
			close(fd);
			mparse_result(mp, &man, &sodest);
		}

		if (sodest != NULL) {
			mlink_dest = ohash_find(&mlinks,
			    ohash_qlookup(&mlinks, sodest));
			if (mlink_dest == NULL) {
				mandoc_asprintf(&cp, "%s.gz", sodest);
				mlink_dest = ohash_find(&mlinks,
				    ohash_qlookup(&mlinks, cp));
				free(cp);
			}
			if (mlink_dest != NULL) {

				/* The .so target exists. */

				mpage_dest = mlink_dest->mpage;
				while (1) {
					mlink->mpage = mpage_dest;

					/*
					 * If the target was already
					 * processed, add the links
					 * to the database now.
					 * Otherwise, this will
					 * happen when we come
					 * to the target.
					 */

					if (mpage_dest->pageid)
						dbadd_mlink_name(mlink);

					if (mlink->next == NULL)
						break;
					mlink = mlink->next;
				}

				/* Move all links to the target. */

				mlink->next = mlink_dest->next;
				mlink_dest->next = mpage->mlinks;
				mpage->mlinks = NULL;
			}
			goto nextpage;
		} else if (man != NULL && man->macroset == MACROSET_MDOC) {
			mdoc_validate(man);
			mpage->form = FORM_SRC;
			mpage->sec = man->meta.msec;
			mpage->sec = mandoc_strdup(
			    mpage->sec == NULL ? "" : mpage->sec);
			mpage->arch = man->meta.arch;
			mpage->arch = mandoc_strdup(
			    mpage->arch == NULL ? "" : mpage->arch);
			mpage->title = mandoc_strdup(man->meta.title);
		} else if (man != NULL && man->macroset == MACROSET_MAN) {
			man_validate(man);
			mpage->form = FORM_SRC;
			mpage->sec = mandoc_strdup(man->meta.msec);
			mpage->arch = mandoc_strdup(mlink->arch);
			mpage->title = mandoc_strdup(man->meta.title);
		} else {
			mpage->form = FORM_CAT;
			mpage->sec = mandoc_strdup(mlink->dsec);
			mpage->arch = mandoc_strdup(mlink->arch);
			mpage->title = mandoc_strdup(mlink->name);
		}
		putkey(mpage, mpage->sec, TYPE_sec);
		if (*mpage->arch != '\0')
			putkey(mpage, mpage->arch, TYPE_arch);

		for ( ; mlink != NULL; mlink = mlink->next) {
			if ('\0' != *mlink->dsec)
				putkey(mpage, mlink->dsec, TYPE_sec);
			if ('\0' != *mlink->fsec)
				putkey(mpage, mlink->fsec, TYPE_sec);
			putkey(mpage, '\0' == *mlink->arch ?
			    any : mlink->arch, TYPE_arch);
			putkey(mpage, mlink->name, NAME_FILE);
		}

		assert(mpage->desc == NULL);
		if (man != NULL && man->macroset == MACROSET_MDOC)
			parse_mdoc(mpage, &man->meta, man->first);
		else if (man != NULL)
			parse_man(mpage, &man->meta, man->first);
		else
			parse_cat(mpage, fd);
		if (mpage->desc == NULL)
			mpage->desc = mandoc_strdup(mpage->mlinks->name);

		if (warnings && !use_all)
			for (mlink = mpage->mlinks; mlink;
			     mlink = mlink->next)
				mlink_check(mpage, mlink);

		dbadd(mpage);
		mlink = mpage->mlinks;

nextpage:
		ohash_delete(&strings);
		ohash_delete(&names);
	}

	if (0 == nodb)
		SQL_EXEC("END TRANSACTION");
}

static void
names_check(void)
{
	sqlite3_stmt	*stmt;
	const char	*name, *sec, *arch, *key;

	sqlite3_prepare_v2(db,
	  "SELECT name, sec, arch, key FROM ("
	    "SELECT name AS key, pageid FROM names "
	    "WHERE bits & ? AND NOT EXISTS ("
	      "SELECT pageid FROM mlinks "
	      "WHERE mlinks.pageid == names.pageid "
	      "AND mlinks.name == names.name"
	    ")"
	  ") JOIN ("
	    "SELECT sec, arch, name, pageid FROM mlinks "
	    "GROUP BY pageid"
	  ") USING (pageid);",
	  -1, &stmt, NULL);

	if (sqlite3_bind_int64(stmt, 1, NAME_TITLE) != SQLITE_OK)
		say("", "%s", sqlite3_errmsg(db));

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		name = (const char *)sqlite3_column_text(stmt, 0);
		sec  = (const char *)sqlite3_column_text(stmt, 1);
		arch = (const char *)sqlite3_column_text(stmt, 2);
		key  = (const char *)sqlite3_column_text(stmt, 3);
		say("", "%s(%s%s%s) lacks mlink \"%s\"", name, sec,
		    '\0' == *arch ? "" : "/",
		    '\0' == *arch ? "" : arch, key);
	}
	sqlite3_finalize(stmt);
}

static void
parse_cat(struct mpage *mpage, int fd)
{
	FILE		*stream;
	char		*line, *p, *title;
	size_t		 linesz, plen, titlesz;
	ssize_t		 len;
	int		 offs;

	stream = (-1 == fd) ?
	    fopen(mpage->mlinks->file, "r") :
	    fdopen(fd, "r");
	if (NULL == stream) {
		if (-1 != fd)
			close(fd);
		if (warnings)
			say(mpage->mlinks->file, "&fopen");
		return;
	}

	line = NULL;
	linesz = 0;

	/* Skip to first blank line. */

	while (getline(&line, &linesz, stream) != -1)
		if (*line == '\n')
			break;

	/*
	 * Assume the first line that is not indented
	 * is the first section header.  Skip to it.
	 */

	while (getline(&line, &linesz, stream) != -1)
		if (*line != '\n' && *line != ' ')
			break;

	/*
	 * Read up until the next section into a buffer.
	 * Strip the leading and trailing newline from each read line,
	 * appending a trailing space.
	 * Ignore empty (whitespace-only) lines.
	 */

	titlesz = 0;
	title = NULL;

	while ((len = getline(&line, &linesz, stream)) != -1) {
		if (*line != ' ')
			break;
		offs = 0;
		while (isspace((unsigned char)line[offs]))
			offs++;
		if (line[offs] == '\0')
			continue;
		title = mandoc_realloc(title, titlesz + len - offs);
		memcpy(title + titlesz, line + offs, len - offs);
		titlesz += len - offs;
		title[titlesz - 1] = ' ';
	}
	free(line);

	/*
	 * If no page content can be found, or the input line
	 * is already the next section header, or there is no
	 * trailing newline, reuse the page title as the page
	 * description.
	 */

	if (NULL == title || '\0' == *title) {
		if (warnings)
			say(mpage->mlinks->file,
			    "Cannot find NAME section");
		fclose(stream);
		free(title);
		return;
	}

	title[titlesz - 1] = '\0';

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
			say(mpage->mlinks->file,
			    "No dash in title line");
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

	mpage->desc = mandoc_strdup(p);
	fclose(stream);
	free(title);
}

/*
 * Put a type/word pair into the word database for this particular file.
 */
static void
putkey(const struct mpage *mpage, char *value, uint64_t type)
{
	char	 *cp;

	assert(NULL != value);
	if (TYPE_arch == type)
		for (cp = value; *cp; cp++)
			if (isupper((unsigned char)*cp))
				*cp = _tolower((unsigned char)*cp);
	putkeys(mpage, value, strlen(value), type);
}

/*
 * Grok all nodes at or below a certain mdoc node into putkey().
 */
static void
putmdockey(const struct mpage *mpage,
	const struct roff_node *n, uint64_t m)
{

	for ( ; NULL != n; n = n->next) {
		if (NULL != n->child)
			putmdockey(mpage, n->child, m);
		if (n->type == ROFFT_TEXT)
			putkey(mpage, n->string, m);
	}
}

static void
parse_man(struct mpage *mpage, const struct roff_meta *meta,
	const struct roff_node *n)
{
	const struct roff_node *head, *body;
	char		*start, *title;
	char		 byte;
	size_t		 sz;

	if (n == NULL)
		return;

	/*
	 * We're only searching for one thing: the first text child in
	 * the BODY of a NAME section.  Since we don't keep track of
	 * sections in -man, run some hoops to find out whether we're in
	 * the correct section or not.
	 */

	if (n->type == ROFFT_BODY && n->tok == MAN_SH) {
		body = n;
		if ((head = body->parent->head) != NULL &&
		    (head = head->child) != NULL &&
		    head->next == NULL &&
		    head->type == ROFFT_TEXT &&
		    strcmp(head->string, "NAME") == 0 &&
		    body->child != NULL) {

			/*
			 * Suck the entire NAME section into memory.
			 * Yes, we might run away.
			 * But too many manuals have big, spread-out
			 * NAME sections over many lines.
			 */

			title = NULL;
			deroff(&title, body);
			if (NULL == title)
				return;

			/*
			 * Go through a special heuristic dance here.
			 * Conventionally, one or more manual names are
			 * comma-specified prior to a whitespace, then a
			 * dash, then a description.  Try to puzzle out
			 * the name parts here.
			 */

			start = title;
			for ( ;; ) {
				sz = strcspn(start, " ,");
				if ('\0' == start[sz])
					break;

				byte = start[sz];
				start[sz] = '\0';

				/*
				 * Assume a stray trailing comma in the
				 * name list if a name begins with a dash.
				 */

				if ('-' == start[0] ||
				    ('\\' == start[0] && '-' == start[1]))
					break;

				putkey(mpage, start, NAME_TITLE);
				if ( ! (mpage->name_head_done ||
				    strcasecmp(start, meta->title))) {
					putkey(mpage, start, NAME_HEAD);
					mpage->name_head_done = 1;
				}

				if (' ' == byte) {
					start += sz + 1;
					break;
				}

				assert(',' == byte);
				start += sz + 1;
				while (' ' == *start)
					start++;
			}

			if (start == title) {
				putkey(mpage, start, NAME_TITLE);
				if ( ! (mpage->name_head_done ||
				    strcasecmp(start, meta->title))) {
					putkey(mpage, start, NAME_HEAD);
					mpage->name_head_done = 1;
				}
				free(title);
				return;
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

			mpage->desc = mandoc_strdup(start);
			free(title);
			return;
		}
	}

	for (n = n->child; n; n = n->next) {
		if (NULL != mpage->desc)
			break;
		parse_man(mpage, meta, n);
	}
}

static void
parse_mdoc(struct mpage *mpage, const struct roff_meta *meta,
	const struct roff_node *n)
{

	assert(NULL != n);
	for (n = n->child; NULL != n; n = n->next) {
		switch (n->type) {
		case ROFFT_ELEM:
		case ROFFT_BLOCK:
		case ROFFT_HEAD:
		case ROFFT_BODY:
		case ROFFT_TAIL:
			if (NULL != mdocs[n->tok].fp)
			       if (0 == (*mdocs[n->tok].fp)(mpage, meta, n))
				       break;
			if (mdocs[n->tok].mask)
				putmdockey(mpage, n->child,
				    mdocs[n->tok].mask);
			break;
		default:
			assert(n->type != ROFFT_ROOT);
			continue;
		}
		if (NULL != n->child)
			parse_mdoc(mpage, meta, n);
	}
}

static int
parse_mdoc_Fd(struct mpage *mpage, const struct roff_meta *meta,
	const struct roff_node *n)
{
	char		*start, *end;
	size_t		 sz;

	if (SEC_SYNOPSIS != n->sec ||
	    NULL == (n = n->child) ||
	    n->type != ROFFT_TEXT)
		return 0;

	/*
	 * Only consider those `Fd' macro fields that begin with an
	 * "inclusion" token (versus, e.g., #define).
	 */

	if (strcmp("#include", n->string))
		return 0;

	if ((n = n->next) == NULL || n->type != ROFFT_TEXT)
		return 0;

	/*
	 * Strip away the enclosing angle brackets and make sure we're
	 * not zero-length.
	 */

	start = n->string;
	if ('<' == *start || '"' == *start)
		start++;

	if (0 == (sz = strlen(start)))
		return 0;

	end = &start[(int)sz - 1];
	if ('>' == *end || '"' == *end)
		end--;

	if (end > start)
		putkeys(mpage, start, end - start + 1, TYPE_In);
	return 0;
}

static void
parse_mdoc_fname(struct mpage *mpage, const struct roff_node *n)
{
	char	*cp;
	size_t	 sz;

	if (n->type != ROFFT_TEXT)
		return;

	/* Skip function pointer punctuation. */

	cp = n->string;
	while (*cp == '(' || *cp == '*')
		cp++;
	sz = strcspn(cp, "()");

	putkeys(mpage, cp, sz, TYPE_Fn);
	if (n->sec == SEC_SYNOPSIS)
		putkeys(mpage, cp, sz, NAME_SYN);
}

static int
parse_mdoc_Fn(struct mpage *mpage, const struct roff_meta *meta,
	const struct roff_node *n)
{

	if (n->child == NULL)
		return 0;

	parse_mdoc_fname(mpage, n->child);

	for (n = n->child->next; n != NULL; n = n->next)
		if (n->type == ROFFT_TEXT)
			putkey(mpage, n->string, TYPE_Fa);

	return 0;
}

static int
parse_mdoc_Fo(struct mpage *mpage, const struct roff_meta *meta,
	const struct roff_node *n)
{

	if (n->type != ROFFT_HEAD)
		return 1;

	if (n->child != NULL)
		parse_mdoc_fname(mpage, n->child);

	return 0;
}

static int
parse_mdoc_Va(struct mpage *mpage, const struct roff_meta *meta,
	const struct roff_node *n)
{
	char *cp;

	if (n->type != ROFFT_ELEM && n->type != ROFFT_BODY)
		return 0;

	if (n->child != NULL &&
	    n->child->next == NULL &&
	    n->child->type == ROFFT_TEXT)
		return 1;

	cp = NULL;
	deroff(&cp, n);
	if (cp != NULL) {
		putkey(mpage, cp, TYPE_Vt | (n->tok == MDOC_Va ||
		    n->type == ROFFT_BODY ? TYPE_Va : 0));
		free(cp);
	}

	return 0;
}

static int
parse_mdoc_Xr(struct mpage *mpage, const struct roff_meta *meta,
	const struct roff_node *n)
{
	char	*cp;

	if (NULL == (n = n->child))
		return 0;

	if (NULL == n->next) {
		putkey(mpage, n->string, TYPE_Xr);
		return 0;
	}

	mandoc_asprintf(&cp, "%s(%s)", n->string, n->next->string);
	putkey(mpage, cp, TYPE_Xr);
	free(cp);
	return 0;
}

static int
parse_mdoc_Nd(struct mpage *mpage, const struct roff_meta *meta,
	const struct roff_node *n)
{

	if (n->type == ROFFT_BODY)
		deroff(&mpage->desc, n);
	return 0;
}

static int
parse_mdoc_Nm(struct mpage *mpage, const struct roff_meta *meta,
	const struct roff_node *n)
{

	if (SEC_NAME == n->sec)
		putmdockey(mpage, n->child, NAME_TITLE);
	else if (n->sec == SEC_SYNOPSIS && n->type == ROFFT_HEAD) {
		if (n->child == NULL)
			putkey(mpage, meta->name, NAME_SYN);
		else
			putmdockey(mpage, n->child, NAME_SYN);
	}
	if ( ! (mpage->name_head_done ||
	    n->child == NULL || n->child->string == NULL ||
	    strcasecmp(n->child->string, meta->title))) {
		putkey(mpage, n->child->string, ROFFT_HEAD);
		mpage->name_head_done = 1;
	}
	return 0;
}

static int
parse_mdoc_Sh(struct mpage *mpage, const struct roff_meta *meta,
	const struct roff_node *n)
{

	return n->sec == SEC_CUSTOM && n->type == ROFFT_HEAD;
}

static int
parse_mdoc_head(struct mpage *mpage, const struct roff_meta *meta,
	const struct roff_node *n)
{

	return n->type == ROFFT_HEAD;
}

/*
 * Add a string to the hash table for the current manual.
 * Each string has a bitmask telling which macros it belongs to.
 * When we finish the manual, we'll dump the table.
 */
static void
putkeys(const struct mpage *mpage, char *cp, size_t sz, uint64_t v)
{
	struct ohash	*htab;
	struct str	*s;
	const char	*end;
	unsigned int	 slot;
	int		 i, mustfree;

	if (0 == sz)
		return;

	mustfree = render_string(&cp, &sz);

	if (TYPE_Nm & v) {
		htab = &names;
		v &= name_mask;
		if (v & NAME_FIRST)
			name_mask &= ~NAME_FIRST;
		if (debug > 1)
			say(mpage->mlinks->file,
			    "Adding name %*s, bits=%d", sz, cp, v);
	} else {
		htab = &strings;
		if (debug > 1)
		    for (i = 0; i < mansearch_keymax; i++)
			if ((uint64_t)1 << i & v)
			    say(mpage->mlinks->file,
				"Adding key %s=%*s",
				mansearch_keynames[i], sz, cp);
	}

	end = cp + sz;
	slot = ohash_qlookupi(htab, cp, &end);
	s = ohash_find(htab, slot);

	if (NULL != s && mpage == s->mpage) {
		s->mask |= v;
		return;
	} else if (NULL == s) {
		s = mandoc_calloc(1, sizeof(struct str) + sz + 1);
		memcpy(s->key, cp, sz);
		ohash_insert(htab, slot, s);
	}
	s->mpage = mpage;
	s->mask = v;

	if (mustfree)
		free(cp);
}

/*
 * Take a Unicode codepoint and produce its UTF-8 encoding.
 * This isn't the best way to do this, but it works.
 * The magic numbers are from the UTF-8 packaging.
 * They're not as scary as they seem: read the UTF-8 spec for details.
 */
static size_t
utf8(unsigned int cp, char out[7])
{
	size_t		 rc;

	rc = 0;
	if (cp <= 0x0000007F) {
		rc = 1;
		out[0] = (char)cp;
	} else if (cp <= 0x000007FF) {
		rc = 2;
		out[0] = (cp >> 6  & 31) | 192;
		out[1] = (cp       & 63) | 128;
	} else if (cp <= 0x0000FFFF) {
		rc = 3;
		out[0] = (cp >> 12 & 15) | 224;
		out[1] = (cp >> 6  & 63) | 128;
		out[2] = (cp       & 63) | 128;
	} else if (cp <= 0x001FFFFF) {
		rc = 4;
		out[0] = (cp >> 18 &  7) | 240;
		out[1] = (cp >> 12 & 63) | 128;
		out[2] = (cp >> 6  & 63) | 128;
		out[3] = (cp       & 63) | 128;
	} else if (cp <= 0x03FFFFFF) {
		rc = 5;
		out[0] = (cp >> 24 &  3) | 248;
		out[1] = (cp >> 18 & 63) | 128;
		out[2] = (cp >> 12 & 63) | 128;
		out[3] = (cp >> 6  & 63) | 128;
		out[4] = (cp       & 63) | 128;
	} else if (cp <= 0x7FFFFFFF) {
		rc = 6;
		out[0] = (cp >> 30 &  1) | 252;
		out[1] = (cp >> 24 & 63) | 128;
		out[2] = (cp >> 18 & 63) | 128;
		out[3] = (cp >> 12 & 63) | 128;
		out[4] = (cp >> 6  & 63) | 128;
		out[5] = (cp       & 63) | 128;
	} else
		return 0;

	out[rc] = '\0';
	return rc;
}

/*
 * If the string contains escape sequences,
 * replace it with an allocated rendering and return 1,
 * such that the caller can free it after use.
 * Otherwise, do nothing and return 0.
 */
static int
render_string(char **public, size_t *psz)
{
	const char	*src, *scp, *addcp, *seq;
	char		*dst;
	size_t		 ssz, dsz, addsz;
	char		 utfbuf[7], res[6];
	int		 seqlen, unicode;

	res[0] = '\\';
	res[1] = '\t';
	res[2] = ASCII_NBRSP;
	res[3] = ASCII_HYPH;
	res[4] = ASCII_BREAK;
	res[5] = '\0';

	src = scp = *public;
	ssz = *psz;
	dst = NULL;
	dsz = 0;

	while (scp < src + *psz) {

		/* Leave normal characters unchanged. */

		if (strchr(res, *scp) == NULL) {
			if (dst != NULL)
				dst[dsz++] = *scp;
			scp++;
			continue;
		}

		/*
		 * Found something that requires replacing,
		 * make sure we have a destination buffer.
		 */

		if (dst == NULL) {
			dst = mandoc_malloc(ssz + 1);
			dsz = scp - src;
			memcpy(dst, src, dsz);
		}

		/* Handle single-char special characters. */

		switch (*scp) {
		case '\\':
			break;
		case '\t':
		case ASCII_NBRSP:
			dst[dsz++] = ' ';
			scp++;
			continue;
		case ASCII_HYPH:
			dst[dsz++] = '-';
			/* FALLTHROUGH */
		case ASCII_BREAK:
			scp++;
			continue;
		default:
			abort();
		}

		/*
		 * Found an escape sequence.
		 * Read past the slash, then parse it.
		 * Ignore everything except characters.
		 */

		scp++;
		if (mandoc_escape(&scp, &seq, &seqlen) != ESCAPE_SPECIAL)
			continue;

		/*
		 * Render the special character
		 * as either UTF-8 or ASCII.
		 */

		if (write_utf8) {
			unicode = mchars_spec2cp(seq, seqlen);
			if (unicode <= 0)
				continue;
			addsz = utf8(unicode, utfbuf);
			if (addsz == 0)
				continue;
			addcp = utfbuf;
		} else {
			addcp = mchars_spec2str(seq, seqlen, &addsz);
			if (addcp == NULL)
				continue;
			if (*addcp == ASCII_NBRSP) {
				addcp = " ";
				addsz = 1;
			}
		}

		/* Copy the rendered glyph into the stream. */

		ssz += addsz;
		dst = mandoc_realloc(dst, ssz + 1);
		memcpy(dst + dsz, addcp, addsz);
		dsz += addsz;
	}
	if (dst != NULL) {
		*public = dst;
		*psz = dsz;
	}

	/* Trim trailing whitespace and NUL-terminate. */

	while (*psz > 0 && (*public)[*psz - 1] == ' ')
		--*psz;
	if (dst != NULL) {
		(*public)[*psz] = '\0';
		return 1;
	} else
		return 0;
}

static void
dbadd_mlink(const struct mlink *mlink)
{
	size_t		 i;

	i = 1;
	SQL_BIND_TEXT(stmts[STMT_INSERT_LINK], i, mlink->dsec);
	SQL_BIND_TEXT(stmts[STMT_INSERT_LINK], i, mlink->arch);
	SQL_BIND_TEXT(stmts[STMT_INSERT_LINK], i, mlink->name);
	SQL_BIND_INT64(stmts[STMT_INSERT_LINK], i, mlink->mpage->pageid);
	SQL_STEP(stmts[STMT_INSERT_LINK]);
	sqlite3_reset(stmts[STMT_INSERT_LINK]);
}

static void
dbadd_mlink_name(const struct mlink *mlink)
{
	uint64_t	 bits;
	size_t		 i;

	dbadd_mlink(mlink);

	i = 1;
	SQL_BIND_INT64(stmts[STMT_SELECT_NAME], i, mlink->mpage->pageid);
	bits = NAME_FILE & NAME_MASK;
	if (sqlite3_step(stmts[STMT_SELECT_NAME]) == SQLITE_ROW) {
		bits |= sqlite3_column_int64(stmts[STMT_SELECT_NAME], 0);
		sqlite3_reset(stmts[STMT_SELECT_NAME]);
	}

	i = 1;
	SQL_BIND_INT64(stmts[STMT_INSERT_NAME], i, bits);
	SQL_BIND_TEXT(stmts[STMT_INSERT_NAME], i, mlink->name);
	SQL_BIND_INT64(stmts[STMT_INSERT_NAME], i, mlink->mpage->pageid);
	SQL_STEP(stmts[STMT_INSERT_NAME]);
	sqlite3_reset(stmts[STMT_INSERT_NAME]);
}

/*
 * Flush the current page's terms (and their bits) into the database.
 * Wrap the entire set of additions in a transaction to make sqlite be a
 * little faster.
 * Also, handle escape sequences at the last possible moment.
 */
static void
dbadd(struct mpage *mpage)
{
	struct mlink	*mlink;
	struct str	*key;
	char		*cp;
	size_t		 i;
	unsigned int	 slot;
	int		 mustfree;

	mlink = mpage->mlinks;

	if (nodb) {
		for (key = ohash_first(&names, &slot); NULL != key;
		     key = ohash_next(&names, &slot))
			free(key);
		for (key = ohash_first(&strings, &slot); NULL != key;
		     key = ohash_next(&strings, &slot))
			free(key);
		if (0 == debug)
			return;
		while (NULL != mlink) {
			fputs(mlink->name, stdout);
			if (NULL == mlink->next ||
			    strcmp(mlink->dsec, mlink->next->dsec) ||
			    strcmp(mlink->fsec, mlink->next->fsec) ||
			    strcmp(mlink->arch, mlink->next->arch)) {
				putchar('(');
				if ('\0' == *mlink->dsec)
					fputs(mlink->fsec, stdout);
				else
					fputs(mlink->dsec, stdout);
				if ('\0' != *mlink->arch)
					printf("/%s", mlink->arch);
				putchar(')');
			}
			mlink = mlink->next;
			if (NULL != mlink)
				fputs(", ", stdout);
		}
		printf(" - %s\n", mpage->desc);
		return;
	}

	if (debug)
		say(mlink->file, "Adding to database");

	cp = mpage->desc;
	i = strlen(cp);
	mustfree = render_string(&cp, &i);
	i = 1;
	SQL_BIND_TEXT(stmts[STMT_INSERT_PAGE], i, cp);
	SQL_BIND_INT(stmts[STMT_INSERT_PAGE], i, mpage->form);
	SQL_STEP(stmts[STMT_INSERT_PAGE]);
	mpage->pageid = sqlite3_last_insert_rowid(db);
	sqlite3_reset(stmts[STMT_INSERT_PAGE]);
	if (mustfree)
		free(cp);

	while (NULL != mlink) {
		dbadd_mlink(mlink);
		mlink = mlink->next;
	}
	mlink = mpage->mlinks;

	for (key = ohash_first(&names, &slot); NULL != key;
	     key = ohash_next(&names, &slot)) {
		assert(key->mpage == mpage);
		i = 1;
		SQL_BIND_INT64(stmts[STMT_INSERT_NAME], i, key->mask);
		SQL_BIND_TEXT(stmts[STMT_INSERT_NAME], i, key->key);
		SQL_BIND_INT64(stmts[STMT_INSERT_NAME], i, mpage->pageid);
		SQL_STEP(stmts[STMT_INSERT_NAME]);
		sqlite3_reset(stmts[STMT_INSERT_NAME]);
		free(key);
	}
	for (key = ohash_first(&strings, &slot); NULL != key;
	     key = ohash_next(&strings, &slot)) {
		assert(key->mpage == mpage);
		i = 1;
		SQL_BIND_INT64(stmts[STMT_INSERT_KEY], i, key->mask);
		SQL_BIND_TEXT(stmts[STMT_INSERT_KEY], i, key->key);
		SQL_BIND_INT64(stmts[STMT_INSERT_KEY], i, mpage->pageid);
		SQL_STEP(stmts[STMT_INSERT_KEY]);
		sqlite3_reset(stmts[STMT_INSERT_KEY]);
		free(key);
	}
}

static void
dbprune(void)
{
	struct mpage	*mpage;
	struct mlink	*mlink;
	size_t		 i;
	unsigned int	 slot;

	if (0 == nodb)
		SQL_EXEC("BEGIN TRANSACTION");

	for (mpage = ohash_first(&mpages, &slot); NULL != mpage;
	     mpage = ohash_next(&mpages, &slot)) {
		mlink = mpage->mlinks;
		if (debug)
			say(mlink->file, "Deleting from database");
		if (nodb)
			continue;
		for ( ; NULL != mlink; mlink = mlink->next) {
			i = 1;
			SQL_BIND_TEXT(stmts[STMT_DELETE_PAGE],
			    i, mlink->dsec);
			SQL_BIND_TEXT(stmts[STMT_DELETE_PAGE],
			    i, mlink->arch);
			SQL_BIND_TEXT(stmts[STMT_DELETE_PAGE],
			    i, mlink->name);
			SQL_STEP(stmts[STMT_DELETE_PAGE]);
			sqlite3_reset(stmts[STMT_DELETE_PAGE]);
		}
	}

	if (0 == nodb)
		SQL_EXEC("END TRANSACTION");
}

/*
 * Close an existing database and its prepared statements.
 * If "real" is not set, rename the temporary file into the real one.
 */
static void
dbclose(int real)
{
	size_t		 i;
	int		 status;
	pid_t		 child;

	if (nodb)
		return;

	for (i = 0; i < STMT__MAX; i++) {
		sqlite3_finalize(stmts[i]);
		stmts[i] = NULL;
	}

	sqlite3_close(db);
	db = NULL;

	if (real)
		return;

	if ('\0' == *tempfilename) {
		if (-1 == rename(MANDOC_DB "~", MANDOC_DB)) {
			exitcode = (int)MANDOCLEVEL_SYSERR;
			say(MANDOC_DB, "&rename");
		}
		return;
	}

	switch (child = fork()) {
	case -1:
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say("", "&fork cmp");
		return;
	case 0:
		execlp("cmp", "cmp", "-s",
		    tempfilename, MANDOC_DB, (char *)NULL);
		say("", "&exec cmp");
		exit(0);
	default:
		break;
	}
	if (-1 == waitpid(child, &status, 0)) {
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say("", "&wait cmp");
	} else if (WIFSIGNALED(status)) {
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say("", "cmp died from signal %d", WTERMSIG(status));
	} else if (WEXITSTATUS(status)) {
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say(MANDOC_DB,
		    "Data changed, but cannot replace database");
	}

	*strrchr(tempfilename, '/') = '\0';
	switch (child = fork()) {
	case -1:
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say("", "&fork rm");
		return;
	case 0:
		execlp("rm", "rm", "-rf", tempfilename, (char *)NULL);
		say("", "&exec rm");
		exit((int)MANDOCLEVEL_SYSERR);
	default:
		break;
	}
	if (-1 == waitpid(child, &status, 0)) {
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say("", "&wait rm");
	} else if (WIFSIGNALED(status) || WEXITSTATUS(status)) {
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say("", "%s: Cannot remove temporary directory",
		    tempfilename);
	}
}

/*
 * This is straightforward stuff.
 * Open a database connection to a "temporary" database, then open a set
 * of prepared statements we'll use over and over again.
 * If "real" is set, we use the existing database; if not, we truncate a
 * temporary one.
 * Must be matched by dbclose().
 */
static int
dbopen(int real)
{
	const char	*sql;
	int		 rc, ofl;

	if (nodb)
		return 1;

	*tempfilename = '\0';
	ofl = SQLITE_OPEN_READWRITE;

	if (real) {
		rc = sqlite3_open_v2(MANDOC_DB, &db, ofl, NULL);
		if (SQLITE_OK != rc) {
			exitcode = (int)MANDOCLEVEL_SYSERR;
			if (SQLITE_CANTOPEN != rc)
				say(MANDOC_DB, "%s", sqlite3_errstr(rc));
			return 0;
		}
		goto prepare_statements;
	}

	ofl |= SQLITE_OPEN_CREATE | SQLITE_OPEN_EXCLUSIVE;

	remove(MANDOC_DB "~");
	rc = sqlite3_open_v2(MANDOC_DB "~", &db, ofl, NULL);
	if (SQLITE_OK == rc)
		goto create_tables;
	if (MPARSE_QUICK & mparse_options) {
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say(MANDOC_DB "~", "%s", sqlite3_errstr(rc));
		return 0;
	}

	(void)strlcpy(tempfilename, "/tmp/mandocdb.XXXXXX",
	    sizeof(tempfilename));
	if (NULL == mkdtemp(tempfilename)) {
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say("", "&%s", tempfilename);
		return 0;
	}
	(void)strlcat(tempfilename, "/" MANDOC_DB,
	    sizeof(tempfilename));
	rc = sqlite3_open_v2(tempfilename, &db, ofl, NULL);
	if (SQLITE_OK != rc) {
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say("", "%s: %s", tempfilename, sqlite3_errstr(rc));
		return 0;
	}

create_tables:
	sql = "CREATE TABLE \"mpages\" (\n"
	      " \"desc\" TEXT NOT NULL,\n"
	      " \"form\" INTEGER NOT NULL,\n"
	      " \"pageid\" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL\n"
	      ");\n"
	      "\n"
	      "CREATE TABLE \"mlinks\" (\n"
	      " \"sec\" TEXT NOT NULL,\n"
	      " \"arch\" TEXT NOT NULL,\n"
	      " \"name\" TEXT NOT NULL,\n"
	      " \"pageid\" INTEGER NOT NULL REFERENCES mpages(pageid) "
		"ON DELETE CASCADE\n"
	      ");\n"
	      "CREATE INDEX mlinks_pageid_idx ON mlinks (pageid);\n"
	      "\n"
	      "CREATE TABLE \"names\" (\n"
	      " \"bits\" INTEGER NOT NULL,\n"
	      " \"name\" TEXT NOT NULL,\n"
	      " \"pageid\" INTEGER NOT NULL REFERENCES mpages(pageid) "
		"ON DELETE CASCADE,\n"
	      " UNIQUE (\"name\", \"pageid\") ON CONFLICT REPLACE\n"
	      ");\n"
	      "\n"
	      "CREATE TABLE \"keys\" (\n"
	      " \"bits\" INTEGER NOT NULL,\n"
	      " \"key\" TEXT NOT NULL,\n"
	      " \"pageid\" INTEGER NOT NULL REFERENCES mpages(pageid) "
		"ON DELETE CASCADE\n"
	      ");\n"
	      "CREATE INDEX keys_pageid_idx ON keys (pageid);\n";

	if (SQLITE_OK != sqlite3_exec(db, sql, NULL, NULL, NULL)) {
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say(MANDOC_DB, "%s", sqlite3_errmsg(db));
		sqlite3_close(db);
		return 0;
	}

prepare_statements:
	if (SQLITE_OK != sqlite3_exec(db,
	    "PRAGMA foreign_keys = ON", NULL, NULL, NULL)) {
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say(MANDOC_DB, "PRAGMA foreign_keys: %s",
		    sqlite3_errmsg(db));
		sqlite3_close(db);
		return 0;
	}

	sql = "DELETE FROM mpages WHERE pageid IN "
		"(SELECT pageid FROM mlinks WHERE "
		"sec=? AND arch=? AND name=?)";
	sqlite3_prepare_v2(db, sql, -1, &stmts[STMT_DELETE_PAGE], NULL);
	sql = "INSERT INTO mpages "
		"(desc,form) VALUES (?,?)";
	sqlite3_prepare_v2(db, sql, -1, &stmts[STMT_INSERT_PAGE], NULL);
	sql = "INSERT INTO mlinks "
		"(sec,arch,name,pageid) VALUES (?,?,?,?)";
	sqlite3_prepare_v2(db, sql, -1, &stmts[STMT_INSERT_LINK], NULL);
	sql = "SELECT bits FROM names where pageid = ?";
	sqlite3_prepare_v2(db, sql, -1, &stmts[STMT_SELECT_NAME], NULL);
	sql = "INSERT INTO names "
		"(bits,name,pageid) VALUES (?,?,?)";
	sqlite3_prepare_v2(db, sql, -1, &stmts[STMT_INSERT_NAME], NULL);
	sql = "INSERT INTO keys "
		"(bits,key,pageid) VALUES (?,?,?)";
	sqlite3_prepare_v2(db, sql, -1, &stmts[STMT_INSERT_KEY], NULL);

#ifndef __APPLE__
	/*
	 * When opening a new database, we can turn off
	 * synchronous mode for much better performance.
	 */

	if (real && SQLITE_OK != sqlite3_exec(db,
	    "PRAGMA synchronous = OFF", NULL, NULL, NULL)) {
		exitcode = (int)MANDOCLEVEL_SYSERR;
		say(MANDOC_DB, "PRAGMA synchronous: %s",
		    sqlite3_errmsg(db));
		sqlite3_close(db);
		return 0;
	}
#endif

	return 1;
}

static int
set_basedir(const char *targetdir, int report_baddir)
{
	static char	 startdir[PATH_MAX];
	static int	 getcwd_status;  /* 1 = ok, 2 = failure */
	static int	 chdir_status;  /* 1 = changed directory */
	char		*cp;

	/*
	 * Remember the original working directory, if possible.
	 * This will be needed if the second or a later directory
	 * on the command line is given as a relative path.
	 * Do not error out if the current directory is not
	 * searchable: Maybe it won't be needed after all.
	 */
	if (0 == getcwd_status) {
		if (NULL == getcwd(startdir, sizeof(startdir))) {
			getcwd_status = 2;
			(void)strlcpy(startdir, strerror(errno),
			    sizeof(startdir));
		} else
			getcwd_status = 1;
	}

	/*
	 * We are leaving the old base directory.
	 * Do not use it any longer, not even for messages.
	 */
	*basedir = '\0';

	/*
	 * If and only if the directory was changed earlier and
	 * the next directory to process is given as a relative path,
	 * first go back, or bail out if that is impossible.
	 */
	if (chdir_status && '/' != *targetdir) {
		if (2 == getcwd_status) {
			exitcode = (int)MANDOCLEVEL_SYSERR;
			say("", "getcwd: %s", startdir);
			return 0;
		}
		if (-1 == chdir(startdir)) {
			exitcode = (int)MANDOCLEVEL_SYSERR;
			say("", "&chdir %s", startdir);
			return 0;
		}
	}

	/*
	 * Always resolve basedir to the canonicalized absolute
	 * pathname and append a trailing slash, such that
	 * we can reliably check whether files are inside.
	 */
	if (NULL == realpath(targetdir, basedir)) {
		if (report_baddir || errno != ENOENT) {
			exitcode = (int)MANDOCLEVEL_BADARG;
			say("", "&%s: realpath", targetdir);
		}
		return 0;
	} else if (-1 == chdir(basedir)) {
		if (report_baddir || errno != ENOENT) {
			exitcode = (int)MANDOCLEVEL_BADARG;
			say("", "&chdir");
		}
		return 0;
	}
	chdir_status = 1;
	cp = strchr(basedir, '\0');
	if ('/' != cp[-1]) {
		if (cp - basedir >= PATH_MAX - 1) {
			exitcode = (int)MANDOCLEVEL_SYSERR;
			say("", "Filename too long");
			return 0;
		}
		*cp++ = '/';
		*cp = '\0';
	}
	return 1;
}

static void
say(const char *file, const char *format, ...)
{
	va_list		 ap;
	int		 use_errno;

	if ('\0' != *basedir)
		fprintf(stderr, "%s", basedir);
	if ('\0' != *basedir && '\0' != *file)
		fputc('/', stderr);
	if ('\0' != *file)
		fprintf(stderr, "%s", file);

	use_errno = 1;
	if (NULL != format) {
		switch (*format) {
		case '&':
			format++;
			break;
		case '\0':
			format = NULL;
			break;
		default:
			use_errno = 0;
			break;
		}
	}
	if (NULL != format) {
		if ('\0' != *basedir || '\0' != *file)
			fputs(": ", stderr);
		va_start(ap, format);
		vfprintf(stderr, format, ap);
		va_end(ap);
	}
	if (use_errno) {
		if ('\0' != *basedir || '\0' != *file || NULL != format)
			fputs(": ", stderr);
		perror(NULL);
	} else
		fputc('\n', stderr);
}
