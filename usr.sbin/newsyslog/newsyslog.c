/*
 * This file contains changes from the Open Software Foundation.
 */

/*
 * Copyright 1988, 1989 by the Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of M.I.T. and the M.I.T. S.I.P.B. not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission. M.I.T. and the M.I.T.
 * S.I.P.B. make no representations about the suitability of this software
 * for any purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 */

/*
 * newsyslog - roll over selected logs at the appropriate time, keeping the a
 * specified number of backup files around.
 */

#ifndef lint
static const char rcsid[] =
"$FreeBSD$";
#endif	/* not lint */

#define OSF
#ifndef COMPRESS_POSTFIX
#define COMPRESS_POSTFIX ".gz"
#endif
#ifndef	BZCOMPRESS_POSTFIX
#define	BZCOMPRESS_POSTFIX ".bz2"
#endif

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <glob.h>
#include <grp.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "pathnames.h"

/*
 * Bit-values for the 'flags' parsed from a config-file entry.
 */
#define CE_COMPACT	0x0001	/* Compact the achived log files with gzip. */
#define CE_BZCOMPACT	0x0002	/* Compact the achived log files with bzip2. */
#define CE_COMPACTWAIT	0x0004	/* wait until compressing one file finishes */
				/*    before starting the next step. */
#define CE_BINARY	0x0008	/* Logfile is in binary, do not add status */
				/*    messages to logfile(s) when rotating. */
#define CE_NOSIGNAL	0x0010	/* There is no process to signal when */
				/*    trimming this file. */
#define CE_TRIMAT	0x0020	/* trim file at a specific time. */
#define CE_GLOB		0x0040	/* name of the log is file name pattern. */

#define MIN_PID         5	/* Don't touch pids lower than this */
#define MAX_PID		99999	/* was lower, see /usr/include/sys/proc.h */

#define kbytes(size)  (((size) + 1023) >> 10)

struct conf_entry {
	char *log;		/* Name of the log */
	char *pid_file;		/* PID file */
	char *r_reason;		/* The reason this file is being rotated */
	int rotate;		/* Non-zero if this file should be rotated */
	uid_t uid;		/* Owner of log */
	gid_t gid;		/* Group of log */
	int numlogs;		/* Number of logs to keep */
	int size;		/* Size cutoff to trigger trimming the log */
	int hours;		/* Hours between log trimming */
	time_t trim_at;		/* Specific time to do trimming */
	int permissions;	/* File permissions on the log */
	int flags;		/* CE_COMPACT, CE_BZCOMPACT, CE_BINARY */
	int sig;		/* Signal to send */
	int def_cfg;		/* Using the <default> rule for this file */
	struct conf_entry *next;/* Linked list pointer */
};

#define DEFAULT_MARKER "<default>"

int archtodir = 0;		/* Archive old logfiles to other directory */
int verbose = 0;		/* Print out what's going on */
int needroot = 1;		/* Root privs are necessary */
int noaction = 0;		/* Don't do anything, just show it */
int nosignal;			/* Do not send any signals */
int force = 0;			/* Force the trim no matter what */
int rotatereq = 0;		/* -R = Always rotate the file(s) as given */
				/*    on the command (this also requires   */
				/*    that a list of files *are* given on  */
				/*    the run command). */
char *requestor;		/* The name given on a -R request */
char *archdirname;		/* Directory path to old logfiles archive */
const char *conf;		/* Configuration file to use */
time_t timenow;

char hostname[MAXHOSTNAMELEN];	/* hostname */
char daytime[16];		/* timenow in human readable form */

static struct conf_entry *get_worklist(char **files);
static void parse_file(FILE *cf, const char *cfname, struct conf_entry **work_p,
		struct conf_entry **defconf_p);
static char *sob(char *p);
static char *son(char *p);
static char *missing_field(char *p, char *errline);
static void do_entry(struct conf_entry * ent);
static void free_entry(struct conf_entry *ent);
static struct conf_entry *init_entry(const char *fname,
		struct conf_entry *src_entry);
static void parse_args(int argc, char **argv);
static void usage(void);
static void dotrim(const struct conf_entry *ent, char *log,
		int numdays, int flags);
static int log_trim(const char *log, const struct conf_entry *log_ent);
static void compress_log(char *log, int dowait);
static void bzcompress_log(char *log, int dowait);
static int sizefile(char *file);
static int age_old_log(char *file);
static pid_t get_pid(const char *pid_file);
static time_t parse8601(char *s, char *errline);
static void movefile(char *from, char *to, int perm, uid_t owner_uid,
		gid_t group_gid);
static void createdir(char *dirpart);
static time_t parseDWM(char *s, char *errline);

/*
 * All the following are defined to work on an 'int', in the
 * range 0 to 255, plus EOF.  Define wrappers which can take
 * values of type 'char', either signed or unsigned.
 */
#define isprintch(Anychar)    isprint(((int) Anychar) & 255)
#define isspacech(Anychar)    isspace(((int) Anychar) & 255)
#define tolowerch(Anychar)    tolower(((int) Anychar) & 255)

int
main(int argc, char **argv)
{
	struct conf_entry *p, *q;
	char *savglob;
	glob_t pglob;
	int i;

	parse_args(argc, argv);
	argc -= optind;
	argv += optind;

	if (needroot && getuid() && geteuid())
		errx(1, "must have root privs");
	p = q = get_worklist(argv);

	while (p) {
		if ((p->flags & CE_GLOB) == 0) {
			do_entry(p);
		} else {
			if (verbose > 2)
				printf("\t+ Processing pattern %s\n", p->log);
			if (glob(p->log, GLOB_NOCHECK, NULL, &pglob) != 0) {
				warn("can't expand pattern: %s", p->log);
			} else {
				savglob = p->log;
				for (i = 0; i < pglob.gl_matchc; i++) {
					p->log = pglob.gl_pathv[i];
					do_entry(p);
				}
				globfree(&pglob);
				p->log = savglob;
				if (verbose > 2)
					printf("\t+ Done with pattern\n");
			}
		}
		p = p->next;
		free_entry(q);
		q = p;
	}
	while (wait(NULL) > 0 || errno == EINTR)
		;
	return (0);
}

static struct conf_entry *
init_entry(const char *fname, struct conf_entry *src_entry)
{
	struct conf_entry *tempwork;

	if (verbose > 4)
		printf("\t--> [creating entry for %s]\n", fname);

	tempwork = malloc(sizeof(struct conf_entry));
	if (tempwork == NULL)
		err(1, "malloc of conf_entry for %s", fname);

	tempwork->log = strdup(fname);
	if (tempwork->log == NULL)
		err(1, "strdup for %s", fname);

	if (src_entry != NULL) {
		tempwork->pid_file = NULL;
		if (src_entry->pid_file)
			tempwork->pid_file = strdup(src_entry->pid_file);
		tempwork->r_reason = NULL;
		tempwork->rotate = 0;
		tempwork->uid = src_entry->uid;
		tempwork->gid = src_entry->gid;
		tempwork->numlogs = src_entry->numlogs;
		tempwork->size = src_entry->size;
		tempwork->hours = src_entry->hours;
		tempwork->trim_at = src_entry->trim_at;
		tempwork->permissions = src_entry->permissions;
		tempwork->flags = src_entry->flags;
		tempwork->sig = src_entry->sig;
		tempwork->def_cfg = src_entry->def_cfg;
	} else {
		/* Initialize as a "do-nothing" entry */
		tempwork->pid_file = NULL;
		tempwork->r_reason = NULL;
		tempwork->rotate = 0;
		tempwork->uid = (uid_t)-1;
		tempwork->gid = (gid_t)-1;
		tempwork->numlogs = 1;
		tempwork->size = -1;
		tempwork->hours = -1;
		tempwork->trim_at = (time_t)0;
		tempwork->permissions = 0;
		tempwork->flags = 0;
		tempwork->sig = SIGHUP;
		tempwork->def_cfg = 0;
	}
	tempwork->next = NULL;

	return (tempwork);
}

static void
free_entry(struct conf_entry *ent)
{

	if (ent == NULL)
		return;

	if (ent->log != NULL) {
		if (verbose > 4)
			printf("\t--> [freeing entry for %s]\n", ent->log);
		free(ent->log);
		ent->log = NULL;
	}

	if (ent->pid_file != NULL) {
		free(ent->pid_file);
		ent->pid_file = NULL;
	}

	if (ent->r_reason != NULL) {
		free(ent->r_reason);
		ent->r_reason = NULL;
	}

	free(ent);
}

static void
do_entry(struct conf_entry * ent)
{
#define REASON_MAX	80
	int size, modtime;
	char temp_reason[REASON_MAX];

	if (verbose) {
		if (ent->flags & CE_COMPACT)
			printf("%s <%dZ>: ", ent->log, ent->numlogs);
		else if (ent->flags & CE_BZCOMPACT)
			printf("%s <%dJ>: ", ent->log, ent->numlogs);
		else
			printf("%s <%d>: ", ent->log, ent->numlogs);
	}
	size = sizefile(ent->log);
	modtime = age_old_log(ent->log);
	ent->rotate = 0;
	if (size < 0) {
		if (verbose)
			printf("does not exist.\n");
	} else {
		if (ent->flags & CE_TRIMAT && !force && !rotatereq) {
			if (timenow < ent->trim_at
			    || difftime(timenow, ent->trim_at) >= 60 * 60) {
				if (verbose)
					printf("--> will trim at %s",
					    ctime(&ent->trim_at));
				return;
			} else if (verbose && ent->hours <= 0) {
				printf("--> time is up\n");
			}
		}
		if (verbose && (ent->size > 0))
			printf("size (Kb): %d [%d] ", size, ent->size);
		if (verbose && (ent->hours > 0))
			printf(" age (hr): %d [%d] ", modtime, ent->hours);

		/*
		 * Figure out if this logfile needs to be rotated.
		 */
		temp_reason[0] = '\0';
		if (rotatereq) {
			ent->rotate = 1;
			snprintf(temp_reason, REASON_MAX, " due to -R from %s",
			    requestor);
		} else if (force) {
			ent->rotate = 1;
			snprintf(temp_reason, REASON_MAX, " due to -F request");
		} else if ((ent->size > 0) && (size >= ent->size)) {
			ent->rotate = 1;
			snprintf(temp_reason, REASON_MAX, " due to size>%dK",
			    ent->size);
		} else if (ent->hours <= 0 && (ent->flags & CE_TRIMAT)) {
			ent->rotate = 1;
		} else if ((ent->hours > 0) && ((modtime >= ent->hours) ||
		    (modtime < 0))) {
			ent->rotate = 1;
		}

		/*
		 * If the file needs to be rotated, then rotate it.
		 */
		if (ent->rotate) {
			if (temp_reason[0] != '\0')
				ent->r_reason = strdup(temp_reason);
			if (verbose)
				printf("--> trimming log....\n");
			if (noaction && !verbose) {
				if (ent->flags & CE_COMPACT)
					printf("%s <%dZ>: trimming\n",
					    ent->log, ent->numlogs);
				else if (ent->flags & CE_BZCOMPACT)
					printf("%s <%dJ>: trimming\n",
					    ent->log, ent->numlogs);
				else
					printf("%s <%d>: trimming\n",
					    ent->log, ent->numlogs);
			}
			dotrim(ent, ent->log, ent->numlogs, ent->flags);
		} else {
			if (verbose)
				printf("--> skipping\n");
		}
	}
#undef REASON_MAX
}

static void
parse_args(int argc, char **argv)
{
	int ch;
	char *p;

	timenow = time(NULL);
	(void)strncpy(daytime, ctime(&timenow) + 4, 15);
	daytime[15] = '\0';

	/* Let's get our hostname */
	(void)gethostname(hostname, sizeof(hostname));

	/* Truncate domain */
	if ((p = strchr(hostname, '.')) != NULL)
		*p = '\0';

	/* Parse command line options. */
	while ((ch = getopt(argc, argv, "a:f:nrsvFR:")) != -1)
		switch (ch) {
		case 'a':
			archtodir++;
			archdirname = optarg;
			break;
		case 'f':
			conf = optarg;
			break;
		case 'n':
			noaction++;
			break;
		case 'r':
			needroot = 0;
			break;
		case 's':
			nosignal = 1;
			break;
		case 'v':
			verbose++;
			break;
		case 'F':
			force++;
			break;
		case 'R':
			rotatereq++;
			requestor = strdup(optarg);
			break;
		case 'm':	/* Used by OpenBSD for "monitor mode" */
		default:
			usage();
			/* NOTREACHED */
		}

	if (rotatereq) {
		if (optind == argc) {
			warnx("At least one filename must be given when -R is specified.");
			usage();
			/* NOTREACHED */
		}
		/* Make sure "requestor" value is safe for a syslog message. */
		for (p = requestor; *p != '\0'; p++) {
			if (!isprintch(*p) && (*p != '\t'))
				*p = '.';
		}
	}
}

static void
usage(void)
{

	fprintf(stderr,
	    "usage: newsyslog [-Fnrsv] [-a directory] [-f config-file]\n"
	    "                 [ [-R requestor] filename ... ]\n");
	exit(1);
}

/*
 * Parse a configuration file and return a linked list of all the logs
 * which should be processed.
 */
static struct conf_entry *
get_worklist(char **files)
{
	FILE *f;
	const char *fname;
	char **given;
	struct conf_entry *defconf, *dupent, *ent, *firstnew;
	struct conf_entry *newlist, *worklist;
	int gmatch;

	defconf = worklist = NULL;

	fname = conf;
	if (fname == NULL)
		fname = _PATH_CONF;

	if (strcmp(fname, "-") != 0)
		f = fopen(fname, "r");
	else {
		f = stdin;
		fname = "<stdin>";
	}
	if (!f)
		err(1, "%s", conf);

	parse_file(f, fname, &worklist, &defconf);
	(void) fclose(f);

	/*
	 * All config-file information has been read in and turned into
	 * a worklist.  If there were no specific files given on the run
	 * command, then the work of this routine is done.
	 */
	if (*files == NULL) {
		if (defconf != NULL)
			free_entry(defconf);
		return (worklist);
		/* NOTREACHED */
	}

	/*
	 * If newsyslog was given a specific list of files to process,
	 * it may be that some of those files were not listed in any
	 * config file.  Those unlisted files should get the default
	 * rotation action.  First, create the default-rotation action
	 * if none was found in a system config file.
	 */
	if (defconf == NULL) {
		defconf = init_entry(DEFAULT_MARKER, NULL);
		defconf->numlogs = 3;
		defconf->size = 50;
		defconf->permissions = S_IRUSR|S_IWUSR;
	}

	/*
	 * If newsyslog was run with a list of specific filenames,
	 * then create a new worklist which has only those files in
	 * it, picking up the rotation-rules for those files from
	 * the original worklist.
	 *
	 * XXX - Note that this will copy multiple rules for a single
	 *	logfile, if multiple entries are an exact match for
	 *	that file.  That matches the historic behavior, but do
	 *	we want to continue to allow it?  If so, it should
	 *	probably be handled more intelligently.
	 */
	firstnew = newlist = NULL;
	for (given = files; *given; ++given) {
		gmatch = 0;
		/*
		 * First try to find exact-matches for this given file.
		 */
		for (ent = worklist; ent; ent = ent->next) {
			if ((ent->flags & CE_GLOB) != 0)
				continue;
			if (strcmp(ent->log, *given) == 0) {
				gmatch++;
				dupent = init_entry(*given, ent);
				if (!firstnew)
					firstnew = dupent;
				else
					newlist->next = dupent;
				newlist = dupent;
			}
		}
		if (gmatch) {
			if (verbose > 2)
				printf("\t+ Matched entry %s\n", *given);
			continue;
		}

		/*
		 * There was no exact-match for this given file, so look
		 * for a "glob" entry which does match.
		 */
		for (ent = worklist; ent; ent = ent->next) {
			if ((ent->flags & CE_GLOB) == 0)
				continue;
			if (fnmatch(ent->log, *given, FNM_PATHNAME) == 0) {
				gmatch++;
				dupent = init_entry(*given, ent);
				if (!firstnew)
					firstnew = dupent;
				else
					newlist->next = dupent;
				newlist = dupent;
				/* This work entry is *not* a glob! */
				dupent->flags &= ~CE_GLOB;
				/* Only allow a match to one glob-entry */
				break;
			}
		}
		if (gmatch) {
			if (verbose > 2)
				printf("\t+ Matched %s via %s\n", *given,
				    ent->log);
			continue;
		}

		/*
		 * This given file was not found in any config file, so
		 * add a worklist item based on the default entry.
		 */
		if (verbose > 2)
			printf("\t+ No entry matched %s  (will use %s)\n",
			    *given, DEFAULT_MARKER);
		dupent = init_entry(*given, defconf);
		if (!firstnew)
			firstnew = dupent;
		else
			newlist->next = dupent;
		/* Mark that it was *not* found in a config file */
		dupent->def_cfg = 1;
		newlist = dupent;
	}

	/*
	 * Free all the entries in the original work list, and then
	 * return the new work list.
	 */
	while (worklist) {
		ent = worklist->next;
		free_entry(worklist);
		worklist = ent;
	}

	free_entry(defconf);
	return (firstnew);
}

/*
 * Parse a configuration file and update a linked list of all the logs to
 * process.
 */
static void
parse_file(FILE *cf, const char *cfname, struct conf_entry **work_p,
    struct conf_entry **defconf_p)
{
	char line[BUFSIZ], *parse, *q;
	char *cp, *errline, *group;
	struct conf_entry *working, *worklist;
	struct passwd *pwd;
	struct group *grp;
	int eol;

	/*
	 * XXX - for now, assume that only one config file will be read,
	 *	ie, this routine is only called one time.
	 */
	worklist = NULL;

	while (fgets(line, BUFSIZ, cf)) {
		if ((line[0] == '\n') || (line[0] == '#') ||
		    (strlen(line) == 0))
			continue;
		errline = strdup(line);
		for (cp = line + 1; *cp != '\0'; cp++) {
			if (*cp != '#')
				continue;
			if (*(cp - 1) == '\\') {
				strcpy(cp - 1, cp);
				cp--;
				continue;
			}
			*cp = '\0';
			break;
		}

		q = parse = missing_field(sob(line), errline);
		parse = son(line);
		if (!*parse)
			errx(1, "malformed line (missing fields):\n%s",
			    errline);
		*parse = '\0';

		working = init_entry(q, NULL);
		if (strcasecmp(DEFAULT_MARKER, q) == 0) {
			if (defconf_p == NULL) {
				warnx("Ignoring entry for %s in %s!", q,
				    cfname);
				free_entry(working);
				continue;
			} else if (*defconf_p != NULL) {
				warnx("Ignoring duplicate entry for %s!", q);
				free_entry(working);
				continue;
			}
			*defconf_p = working;
		} else {
			if (!*work_p)
				*work_p = working;
			else
				worklist->next = working;
			worklist = working;
		}

		q = parse = missing_field(sob(++parse), errline);
		parse = son(parse);
		if (!*parse)
			errx(1, "malformed line (missing fields):\n%s",
			    errline);
		*parse = '\0';
		if ((group = strchr(q, ':')) != NULL ||
		    (group = strrchr(q, '.')) != NULL) {
			*group++ = '\0';
			if (*q) {
				if (!(isnumber(*q))) {
					if ((pwd = getpwnam(q)) == NULL)
						errx(1,
				     "error in config file; unknown user:\n%s",
						    errline);
					working->uid = pwd->pw_uid;
				} else
					working->uid = atoi(q);
			} else
				working->uid = (uid_t)-1;

			q = group;
			if (*q) {
				if (!(isnumber(*q))) {
					if ((grp = getgrnam(q)) == NULL)
						errx(1,
				    "error in config file; unknown group:\n%s",
						    errline);
					working->gid = grp->gr_gid;
				} else
					working->gid = atoi(q);
			} else
				working->gid = (gid_t)-1;

			q = parse = missing_field(sob(++parse), errline);
			parse = son(parse);
			if (!*parse)
				errx(1, "malformed line (missing fields):\n%s",
				    errline);
			*parse = '\0';
		} else {
			working->uid = (uid_t)-1;
			working->gid = (gid_t)-1;
		}

		if (!sscanf(q, "%o", &working->permissions))
			errx(1, "error in config file; bad permissions:\n%s",
			    errline);

		q = parse = missing_field(sob(++parse), errline);
		parse = son(parse);
		if (!*parse)
			errx(1, "malformed line (missing fields):\n%s",
			    errline);
		*parse = '\0';
		if (!sscanf(q, "%d", &working->numlogs) || working->numlogs < 0)
			errx(1, "error in config file; bad value for count of logs to save:\n%s",
			    errline);

		q = parse = missing_field(sob(++parse), errline);
		parse = son(parse);
		if (!*parse)
			errx(1, "malformed line (missing fields):\n%s",
			    errline);
		*parse = '\0';
		if (isdigit(*q))
			working->size = atoi(q);
		else
			working->size = -1;

		working->flags = 0;
		q = parse = missing_field(sob(++parse), errline);
		parse = son(parse);
		eol = !*parse;
		*parse = '\0';
		{
			char *ep;
			u_long ul;

			ul = strtoul(q, &ep, 10);
			if (ep == q)
				working->hours = 0;
			else if (*ep == '*')
				working->hours = -1;
			else if (ul > INT_MAX)
				errx(1, "interval is too large:\n%s", errline);
			else
				working->hours = ul;

			if (*ep != '\0' && *ep != '@' && *ep != '*' &&
			    *ep != '$')
				errx(1, "malformed interval/at:\n%s", errline);
			if (*ep == '@') {
				if ((working->trim_at = parse8601(ep + 1, errline))
				    == (time_t) - 1)
					errx(1, "malformed at:\n%s", errline);
				working->flags |= CE_TRIMAT;
			} else if (*ep == '$') {
				if ((working->trim_at = parseDWM(ep + 1, errline))
				    == (time_t) - 1)
					errx(1, "malformed at:\n%s", errline);
				working->flags |= CE_TRIMAT;
			}
		}

		if (eol)
			q = NULL;
		else {
			q = parse = sob(++parse);	/* Optional field */
			parse = son(parse);
			if (!*parse)
				eol = 1;
			*parse = '\0';
		}

		for (; q && *q && !isspacech(*q); q++) {
			switch (tolowerch(*q)) {
			case 'b':
				working->flags |= CE_BINARY;
				break;
			case 'c':	/* Used by NetBSD  for "CE_CREATE" */
				/*
				 * netbsd uses 'c' for "create".  We will
				 * temporarily accept it for 'g', because
				 * earlier freebsd versions had a typo
				 * of ('G' || 'c')...
				 */
				warnx("Assuming 'g' for 'c' in flags for line:\n%s",
				    errline);
				/* FALLTHROUGH */
			case 'g':
				working->flags |= CE_GLOB;
				break;
			case 'j':
				working->flags |= CE_BZCOMPACT;
				break;
			case 'n':
				working->flags |= CE_NOSIGNAL;
				break;
			case 'w':
				working->flags |= CE_COMPACTWAIT;
				break;
			case 'z':
				working->flags |= CE_COMPACT;
				break;
			case '-':
				break;
			case 'f':	/* Used by OpenBSD for "CE_FOLLOW" */
			case 'm':	/* Used by OpenBSD for "CE_MONITOR" */
			case 'p':	/* Used by NetBSD  for "CE_PLAIN0" */
			default:
				errx(1, "illegal flag in config file -- %c",
				    *q);
			}
		}

		if (eol)
			q = NULL;
		else {
			q = parse = sob(++parse);	/* Optional field */
			parse = son(parse);
			if (!*parse)
				eol = 1;
			*parse = '\0';
		}

		working->pid_file = NULL;
		if (q && *q) {
			if (*q == '/')
				working->pid_file = strdup(q);
			else if (isdigit(*q))
				goto got_sig;
			else
				errx(1,
			"illegal pid file or signal number in config file:\n%s",
				    errline);
		}
		if (eol)
			q = NULL;
		else {
			q = parse = sob(++parse);	/* Optional field */
			*(parse = son(parse)) = '\0';
		}

		working->sig = SIGHUP;
		if (q && *q) {
			if (isdigit(*q)) {
		got_sig:
				working->sig = atoi(q);
			} else {
		err_sig:
				errx(1,
				    "illegal signal number in config file:\n%s",
				    errline);
			}
			if (working->sig < 1 || working->sig >= NSIG)
				goto err_sig;
		}

		/*
		 * Finish figuring out what pid-file to use (if any) in
		 * later processing if this logfile needs to be rotated.
		 */
		if ((working->flags & CE_NOSIGNAL) == CE_NOSIGNAL) {
			/*
			 * This config-entry specified 'n' for nosignal,
			 * see if it also specified an explicit pid_file.
			 * This would be a pretty pointless combination.
			 */
			if (working->pid_file != NULL) {
				warnx("Ignoring '%s' because flag 'n' was specified in line:\n%s",
				    working->pid_file, errline);
				free(working->pid_file);
				working->pid_file = NULL;
			}
		} else if (working->pid_file == NULL) {
			/*
			 * This entry did not specify the 'n' flag, which
			 * means it should signal syslogd unless it had
			 * specified some other pid-file.  But we only
			 * try to notify syslog if we are root
			 */
			if (needroot)
				working->pid_file = strdup(_PATH_SYSLOGPID);
		}

		free(errline);
		errline = NULL;
	}
}

static char *
missing_field(char *p, char *errline)
{

	if (!p || !*p)
		errx(1, "missing field in config file:\n%s", errline);
	return (p);
}

static void
dotrim(const struct conf_entry *ent, char *log, int numdays, int flags)
{
	char dirpart[MAXPATHLEN], namepart[MAXPATHLEN];
	char file1[MAXPATHLEN], file2[MAXPATHLEN];
	char zfile1[MAXPATHLEN], zfile2[MAXPATHLEN];
	char jfile1[MAXPATHLEN];
	char tfile[MAXPATHLEN];
	int notified, need_notification, fd, _numdays;
	struct stat st;
	pid_t pid;

	if (archtodir) {
		char *p;

		/* build complete name of archive directory into dirpart */
		if (*archdirname == '/') {	/* absolute */
			strlcpy(dirpart, archdirname, sizeof(dirpart));
		} else {	/* relative */
			/* get directory part of logfile */
			strlcpy(dirpart, log, sizeof(dirpart));
			if ((p = rindex(dirpart, '/')) == NULL)
				dirpart[0] = '\0';
			else
				*(p + 1) = '\0';
			strlcat(dirpart, archdirname, sizeof(dirpart));
		}

		/* check if archive directory exists, if not, create it */
		if (lstat(dirpart, &st))
			createdir(dirpart);

		/* get filename part of logfile */
		if ((p = rindex(log, '/')) == NULL)
			strlcpy(namepart, log, sizeof(namepart));
		else
			strlcpy(namepart, p + 1, sizeof(namepart));

		/* name of oldest log */
		(void) snprintf(file1, sizeof(file1), "%s/%s.%d", dirpart,
		    namepart, numdays);
		(void) snprintf(zfile1, sizeof(zfile1), "%s%s", file1,
		    COMPRESS_POSTFIX);
		snprintf(jfile1, sizeof(jfile1), "%s%s", file1,
		    BZCOMPRESS_POSTFIX);
	} else {
		/* name of oldest log */
		(void) snprintf(file1, sizeof(file1), "%s.%d", log, numdays);
		(void) snprintf(zfile1, sizeof(zfile1), "%s%s", file1,
		    COMPRESS_POSTFIX);
		snprintf(jfile1, sizeof(jfile1), "%s%s", file1,
		    BZCOMPRESS_POSTFIX);
	}

	if (noaction) {
		printf("\trm -f %s\n", file1);
		printf("\trm -f %s\n", zfile1);
		printf("\trm -f %s\n", jfile1);
	} else {
		(void) unlink(file1);
		(void) unlink(zfile1);
		(void) unlink(jfile1);
	}

	/* Move down log files */
	_numdays = numdays;	/* preserve */
	while (numdays--) {

		(void) strlcpy(file2, file1, sizeof(file2));

		if (archtodir)
			(void) snprintf(file1, sizeof(file1), "%s/%s.%d",
			    dirpart, namepart, numdays);
		else
			(void) snprintf(file1, sizeof(file1), "%s.%d", log,
			    numdays);

		(void) strlcpy(zfile1, file1, sizeof(zfile1));
		(void) strlcpy(zfile2, file2, sizeof(zfile2));
		if (lstat(file1, &st)) {
			(void) strlcat(zfile1, COMPRESS_POSTFIX,
			    sizeof(zfile1));
			(void) strlcat(zfile2, COMPRESS_POSTFIX,
			    sizeof(zfile2));
			if (lstat(zfile1, &st)) {
				strlcpy(zfile1, file1, sizeof(zfile1));
				strlcpy(zfile2, file2, sizeof(zfile2));
				strlcat(zfile1, BZCOMPRESS_POSTFIX,
				    sizeof(zfile1));
				strlcat(zfile2, BZCOMPRESS_POSTFIX,
				    sizeof(zfile2));
				if (lstat(zfile1, &st))
					continue;
			}
		}
		if (noaction) {
			printf("\tmv %s %s\n", zfile1, zfile2);
			printf("\tchmod %o %s\n", ent->permissions, zfile2);
			if (ent->uid != (uid_t)-1 || ent->gid != (gid_t)-1)
				printf("\tchown %u:%u %s\n",
				    ent->uid, ent->gid, zfile2);
		} else {
			(void) rename(zfile1, zfile2);
			if (chmod(zfile2, ent->permissions))
				warn("can't chmod %s", file2);
			if (ent->uid != (uid_t)-1 || ent->gid != (gid_t)-1)
				if (chown(zfile2, ent->uid, ent->gid))
					warn("can't chown %s", zfile2);
		}
	}
	if (!noaction && !(flags & CE_BINARY)) {
		/* Report the trimming to the old log */
		(void) log_trim(log, ent);
	}

	if (!_numdays) {
		if (noaction)
			printf("\trm %s\n", log);
		else
			(void) unlink(log);
	} else {
		if (noaction)
			printf("\tmv %s to %s\n", log, file1);
		else {
			if (archtodir)
				movefile(log, file1, ent->permissions, ent->uid,
				    ent->gid);
			else
				(void) rename(log, file1);
		}
	}

	/* Now move the new log file into place */
	strlcpy(tfile, log, sizeof(tfile));
	strlcat(tfile, ".XXXXXX", sizeof(tfile));
	if (noaction) {
		printf("Start new log...\n");
		printf("\tmktemp %s\n", tfile);
	} else {
		mkstemp(tfile);
		fd = creat(tfile, ent->permissions);
		if (fd < 0)
			err(1, "can't start new log");
		if (ent->uid != (uid_t)-1 || ent->gid != (gid_t)-1)
			if (fchown(fd, ent->uid, ent->gid))
			    err(1, "can't chown new log file");
		(void) close(fd);
		if (!(flags & CE_BINARY)) {
			/* Add status message to new log file */
			if (log_trim(tfile, ent))
				err(1, "can't add status message to log");
		}
	}
	if (noaction) {
		printf("\tchmod %o %s\n", ent->permissions, tfile);
		printf("\tmv %s %s\n", tfile, log);
	} else {
		(void) chmod(tfile, ent->permissions);
		if (rename(tfile, log) < 0) {
			err(1, "can't start new log");
			(void) unlink(tfile);
		}
	}

	/*
	 * Find out if there is a process to signal.  If nosignal (-s) was
	 * specified, then do not signal any process.  Note that nosignal
	 * will trigger a warning message if the rotated logfile needs to
	 * be compressed, *unless* -R was specified.  This is because there
	 * presumably still are process(es) writing to the old logfile, but
	 * we assume that a -sR request comes from a process which writes 
	 * to the logfile, and as such, that process has already made sure
	 * that the logfile is not presently in use.
	 */
	pid = 0;
	need_notification = notified = 0;
	if (ent->pid_file != NULL) {
		need_notification = 1;
		if (!nosignal)
			pid = get_pid(ent->pid_file);	/* the normal case! */
		else if (rotatereq)
			need_notification = 0;
	}
	if (pid) {
		if (noaction) {
			notified = 1;
			printf("\tkill -%d %d\n", ent->sig, (int) pid);
		} else if (kill(pid, ent->sig))
			warn("can't notify daemon, pid %d", (int) pid);
		else {
			notified = 1;
			if (verbose)
				printf("daemon pid %d notified\n", (int) pid);
		}
	}
	if ((flags & CE_COMPACT) || (flags & CE_BZCOMPACT)) {
		if (need_notification && !notified)
			warnx(
			    "log %s.0 not compressed because daemon not notified",
			    log);
		else if (noaction)
			if (flags & CE_COMPACT)
				printf("\tgzip %s.0\n", log);
			else
				printf("\tbzip2 %s.0\n", log);
		else {
			if (notified) {
				if (verbose)
					printf("small pause to allow daemon to close log\n");
				sleep(10);
			}
			if (archtodir) {
				(void) snprintf(file1, sizeof(file1), "%s/%s",
				    dirpart, namepart);
				if (flags & CE_COMPACT)
					compress_log(file1,
					    flags & CE_COMPACTWAIT);
				else if (flags & CE_BZCOMPACT)
					bzcompress_log(file1,
					    flags & CE_COMPACTWAIT);
			} else {
				if (flags & CE_COMPACT)
					compress_log(log,
					    flags & CE_COMPACTWAIT);
				else if (flags & CE_BZCOMPACT)
					bzcompress_log(log,
					    flags & CE_COMPACTWAIT);
			}
		}
	}
}

/* Log the fact that the logs were turned over */
static int
log_trim(const char *log, const struct conf_entry *log_ent)
{
	FILE *f;
	const char *xtra;

	if ((f = fopen(log, "a")) == NULL)
		return (-1);
	xtra = "";
	if (log_ent->def_cfg)
		xtra = " using <default> rule";
	if (log_ent->r_reason != NULL)
		fprintf(f, "%s %s newsyslog[%d]: logfile turned over%s%s\n",
		    daytime, hostname, (int) getpid(), log_ent->r_reason, xtra);
	else
		fprintf(f, "%s %s newsyslog[%d]: logfile turned over%s\n",
		    daytime, hostname, (int) getpid(), xtra);
	if (fclose(f) == EOF)
		err(1, "log_trim: fclose:");
	return (0);
}

/* Fork of gzip to compress the old log file */
static void
compress_log(char *log, int dowait)
{
	pid_t pid;
	char tmp[MAXPATHLEN];

	while (dowait && (wait(NULL) > 0 || errno == EINTR))
		;
	(void) snprintf(tmp, sizeof(tmp), "%s.0", log);
	pid = fork();
	if (pid < 0)
		err(1, "gzip fork");
	else if (!pid) {
		(void) execl(_PATH_GZIP, _PATH_GZIP, "-f", tmp, (char *)0);
		err(1, _PATH_GZIP);
	}
}

/* Fork of bzip2 to compress the old log file */
static void
bzcompress_log(char *log, int dowait)
{
	pid_t pid;
	char tmp[MAXPATHLEN];

	while (dowait && (wait(NULL) > 0 || errno == EINTR))
		;
	snprintf(tmp, sizeof(tmp), "%s.0", log);
	pid = fork();
	if (pid < 0)
		err(1, "bzip2 fork");
	else if (!pid) {
		execl(_PATH_BZIP2, _PATH_BZIP2, "-f", tmp, (char *)0);
		err(1, _PATH_BZIP2);
	}
}

/* Return size in kilobytes of a file */
static int
sizefile(char *file)
{
	struct stat sb;

	if (stat(file, &sb) < 0)
		return (-1);
	return (kbytes(dbtob(sb.st_blocks)));
}

/* Return the age of old log file (file.0) */
static int
age_old_log(char *file)
{
	struct stat sb;
	char tmp[MAXPATHLEN + sizeof(".0") + sizeof(COMPRESS_POSTFIX) + 1];

	if (archtodir) {
		char *p;

		/* build name of archive directory into tmp */
		if (*archdirname == '/') {	/* absolute */
			strlcpy(tmp, archdirname, sizeof(tmp));
		} else {	/* relative */
			/* get directory part of logfile */
			strlcpy(tmp, file, sizeof(tmp));
			if ((p = rindex(tmp, '/')) == NULL)
				tmp[0] = '\0';
			else
				*(p + 1) = '\0';
			strlcat(tmp, archdirname, sizeof(tmp));
		}

		strlcat(tmp, "/", sizeof(tmp));

		/* get filename part of logfile */
		if ((p = rindex(file, '/')) == NULL)
			strlcat(tmp, file, sizeof(tmp));
		else
			strlcat(tmp, p + 1, sizeof(tmp));
	} else {
		(void) strlcpy(tmp, file, sizeof(tmp));
	}

	if (stat(strcat(tmp, ".0"), &sb) < 0)
		if (stat(strcat(tmp, COMPRESS_POSTFIX), &sb) < 0)
			return (-1);
	return ((int)(timenow - sb.st_mtime + 1800) / 3600);
}

static pid_t
get_pid(const char *pid_file)
{
	FILE *f;
	char line[BUFSIZ];
	pid_t pid = 0;

	if ((f = fopen(pid_file, "r")) == NULL)
		warn("can't open %s pid file to restart a daemon",
		    pid_file);
	else {
		if (fgets(line, BUFSIZ, f)) {
			pid = atol(line);
			if (pid < MIN_PID || pid > MAX_PID) {
				warnx("preposterous process number: %d",
				   (int)pid);
				pid = 0;
			}
		} else
			warn("can't read %s pid file to restart a daemon",
			    pid_file);
		(void) fclose(f);
	}
	return (pid);
}

/* Skip Over Blanks */
static char *
sob(char *p)
{
	while (p && *p && isspace(*p))
		p++;
	return (p);
}

/* Skip Over Non-Blanks */
static char *
son(char *p)
{
	while (p && *p && !isspace(*p))
		p++;
	return (p);
}

/*
 * Parse a limited subset of ISO 8601. The specific format is as follows:
 *
 * [CC[YY[MM[DD]]]][THH[MM[SS]]]	(where `T' is the literal letter)
 *
 * We don't accept a timezone specification; missing fields (including timezone)
 * are defaulted to the current date but time zero.
 */
static time_t
parse8601(char *s, char *errline)
{
	char *t;
	time_t tsecs;
	struct tm tm, *tmp;
	u_long ul;

	tmp = localtime(&timenow);
	tm = *tmp;

	tm.tm_hour = tm.tm_min = tm.tm_sec = 0;

	ul = strtoul(s, &t, 10);
	if (*t != '\0' && *t != 'T')
		return (-1);

	/*
	 * Now t points either to the end of the string (if no time was
	 * provided) or to the letter `T' which separates date and time in
	 * ISO 8601.  The pointer arithmetic is the same for either case.
	 */
	switch (t - s) {
	case 8:
		tm.tm_year = ((ul / 1000000) - 19) * 100;
		ul = ul % 1000000;
	case 6:
		tm.tm_year -= tm.tm_year % 100;
		tm.tm_year += ul / 10000;
		ul = ul % 10000;
	case 4:
		tm.tm_mon = (ul / 100) - 1;
		ul = ul % 100;
	case 2:
		tm.tm_mday = ul;
	case 0:
		break;
	default:
		return (-1);
	}

	/* sanity check */
	if (tm.tm_year < 70 || tm.tm_mon < 0 || tm.tm_mon > 12
	    || tm.tm_mday < 1 || tm.tm_mday > 31)
		return (-1);

	if (*t != '\0') {
		s = ++t;
		ul = strtoul(s, &t, 10);
		if (*t != '\0' && !isspace(*t))
			return (-1);

		switch (t - s) {
		case 6:
			tm.tm_sec = ul % 100;
			ul /= 100;
		case 4:
			tm.tm_min = ul % 100;
			ul /= 100;
		case 2:
			tm.tm_hour = ul;
		case 0:
			break;
		default:
			return (-1);
		}

		/* sanity check */
		if (tm.tm_sec < 0 || tm.tm_sec > 60 || tm.tm_min < 0
		    || tm.tm_min > 59 || tm.tm_hour < 0 || tm.tm_hour > 23)
			return (-1);
	}
	if ((tsecs = mktime(&tm)) == -1)
		errx(1, "nonexistent time:\n%s", errline);
	return (tsecs);
}

/* physically move file */
static void
movefile(char *from, char *to, int perm, uid_t owner_uid, gid_t group_gid)
{
	FILE *src, *dst;
	int c;

	if ((src = fopen(from, "r")) == NULL)
		err(1, "can't fopen %s for reading", from);
	if ((dst = fopen(to, "w")) == NULL)
		err(1, "can't fopen %s for writing", to);
	if (owner_uid != (uid_t)-1 || group_gid != (gid_t)-1) {
		if (fchown(fileno(dst), owner_uid, group_gid))
			err(1, "can't fchown %s", to);
	}
	if (fchmod(fileno(dst), perm))
		err(1, "can't fchmod %s", to);

	while ((c = getc(src)) != EOF) {
		if ((putc(c, dst)) == EOF)
			err(1, "error writing to %s", to);
	}

	if (ferror(src))
		err(1, "error reading from %s", from);
	if ((fclose(src)) != 0)
		err(1, "can't fclose %s", to);
	if ((fclose(dst)) != 0)
		err(1, "can't fclose %s", from);
	if ((unlink(from)) != 0)
		err(1, "can't unlink %s", from);
}

/* create one or more directory components of a path */
static void
createdir(char *dirpart)
{
	int res;
	char *s, *d;
	char mkdirpath[MAXPATHLEN];
	struct stat st;

	s = dirpart;
	d = mkdirpath;

	for (;;) {
		*d++ = *s++;
		if (*s != '/' && *s != '\0')
			continue;
		*d = '\0';
		res = lstat(mkdirpath, &st);
		if (res != 0) {
			if (noaction) {
				printf("\tmkdir %s\n", mkdirpath);
			} else {
				res = mkdir(mkdirpath, 0755);
				if (res != 0)
					err(1, "Error on mkdir(\"%s\") for -a",
					    mkdirpath);
			}
		}
		if (*s == '\0')
			break;
	}
	if (verbose)
		printf("created directory '%s' for -a\n", dirpart);
}

/*-
 * Parse a cyclic time specification, the format is as follows:
 *
 *	[Dhh] or [Wd[Dhh]] or [Mdd[Dhh]]
 *
 * to rotate a logfile cyclic at
 *
 *	- every day (D) within a specific hour (hh)	(hh = 0...23)
 *	- once a week (W) at a specific day (d)     OR	(d = 0..6, 0 = Sunday)
 *	- once a month (M) at a specific day (d)	(d = 1..31,l|L)
 *
 * We don't accept a timezone specification; missing fields
 * are defaulted to the current date but time zero.
 */
static time_t
parseDWM(char *s, char *errline)
{
	char *t;
	time_t tsecs;
	struct tm tm, *tmp;
	long l;
	int nd;
	static int mtab[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	int WMseen = 0;
	int Dseen = 0;

	tmp = localtime(&timenow);
	tm = *tmp;

	/* set no. of days per month */

	nd = mtab[tm.tm_mon];

	if (tm.tm_mon == 1) {
		if (((tm.tm_year + 1900) % 4 == 0) &&
		    ((tm.tm_year + 1900) % 100 != 0) &&
		    ((tm.tm_year + 1900) % 400 == 0)) {
			nd++;	/* leap year, 29 days in february */
		}
	}
	tm.tm_hour = tm.tm_min = tm.tm_sec = 0;

	for (;;) {
		switch (*s) {
		case 'D':
			if (Dseen)
				return (-1);
			Dseen++;
			s++;
			l = strtol(s, &t, 10);
			if (l < 0 || l > 23)
				return (-1);
			tm.tm_hour = l;
			break;

		case 'W':
			if (WMseen)
				return (-1);
			WMseen++;
			s++;
			l = strtol(s, &t, 10);
			if (l < 0 || l > 6)
				return (-1);
			if (l != tm.tm_wday) {
				int save;

				if (l < tm.tm_wday) {
					save = 6 - tm.tm_wday;
					save += (l + 1);
				} else {
					save = l - tm.tm_wday;
				}

				tm.tm_mday += save;

				if (tm.tm_mday > nd) {
					tm.tm_mon++;
					tm.tm_mday = tm.tm_mday - nd;
				}
			}
			break;

		case 'M':
			if (WMseen)
				return (-1);
			WMseen++;
			s++;
			if (tolower(*s) == 'l') {
				tm.tm_mday = nd;
				s++;
				t = s;
			} else {
				l = strtol(s, &t, 10);
				if (l < 1 || l > 31)
					return (-1);

				if (l > nd)
					return (-1);
				tm.tm_mday = l;
			}
			break;

		default:
			return (-1);
			break;
		}

		if (*t == '\0' || isspace(*t))
			break;
		else
			s = t;
	}
	if ((tsecs = mktime(&tm)) == -1)
		errx(1, "nonexistent time:\n%s", errline);
	return (tsecs);
}
