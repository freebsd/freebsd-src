/*-
 * ------+---------+---------+-------- + --------+---------+---------+---------*
 * This file includes significant modifications done by:
 * Copyright (c) 2003, 2004  - Garance Alistair Drosehn <gad@FreeBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * ------+---------+---------+-------- + --------+---------+---------+---------*
 */

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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define	OSF
#ifndef COMPRESS_POSTFIX
#define	COMPRESS_POSTFIX ".gz"
#endif
#ifndef	BZCOMPRESS_POSTFIX
#define	BZCOMPRESS_POSTFIX ".bz2"
#endif

#include <sys/param.h>
#include <sys/queue.h>
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
#include "extern.h"

/*
 * Bit-values for the 'flags' parsed from a config-file entry.
 */
#define	CE_COMPACT	0x0001	/* Compact the archived log files with gzip. */
#define	CE_BZCOMPACT	0x0002	/* Compact the archived log files with bzip2. */
#define	CE_BINARY	0x0008	/* Logfile is in binary, do not add status */
				/*    messages to logfile(s) when rotating. */
#define	CE_NOSIGNAL	0x0010	/* There is no process to signal when */
				/*    trimming this file. */
#define	CE_TRIMAT	0x0020	/* trim file at a specific time. */
#define	CE_GLOB		0x0040	/* name of the log is file name pattern. */
#define	CE_SIGNALGROUP	0x0080	/* Signal a process-group instead of a single */
				/*    process when trimming this file. */
#define	CE_CREATE	0x0100	/* Create the log file if it does not exist. */
#define	CE_NODUMP	0x0200	/* Set 'nodump' on newly created log file. */

#define	MIN_PID         5	/* Don't touch pids lower than this */
#define	MAX_PID		99999	/* was lower, see /usr/include/sys/proc.h */

#define	kbytes(size)  (((size) + 1023) >> 10)

#define	DEFAULT_MARKER	"<default>"
#define	DEBUG_MARKER	"<debug>"

struct conf_entry {
	char *log;		/* Name of the log */
	char *pid_file;		/* PID file */
	char *r_reason;		/* The reason this file is being rotated */
	int firstcreate;	/* Creating log for the first time (-C). */
	int rotate;		/* Non-zero if this file should be rotated */
	int fsize;		/* size found for the log file */
	uid_t uid;		/* Owner of log */
	gid_t gid;		/* Group of log */
	int numlogs;		/* Number of logs to keep */
	int trsize;		/* Size cutoff to trigger trimming the log */
	int hours;		/* Hours between log trimming */
	struct ptime_data *trim_at;	/* Specific time to do trimming */
	unsigned int permissions;	/* File permissions on the log */
	int flags;		/* CE_COMPACT, CE_BZCOMPACT, CE_BINARY */
	int sig;		/* Signal to send */
	int def_cfg;		/* Using the <default> rule for this file */
	struct conf_entry *next;/* Linked list pointer */
};

struct sigwork_entry {
	SLIST_ENTRY(sigwork_entry) sw_nextp;
	int	 sw_signum;		/* the signal to send */
	int	 sw_pidok;		/* true if pid value is valid */
	pid_t	 sw_pid;		/* the process id from the PID file */
	const char *sw_pidtype;		/* "daemon" or "process group" */
	char	 sw_fname[1];		/* file the PID was read from */
};

struct zipwork_entry {
	SLIST_ENTRY(zipwork_entry) zw_nextp;
	const struct conf_entry *zw_conf;	/* for chown/perm/flag info */
	const struct sigwork_entry *zw_swork;	/* to know success of signal */
	int	 zw_fsize;		/* size of the file to compress */
	char	 zw_fname[1];		/* the file to compress */
};

typedef enum {
	FREE_ENT, KEEP_ENT
}	fk_entry;

SLIST_HEAD(swlisthead, sigwork_entry) swhead = SLIST_HEAD_INITIALIZER(swhead);
SLIST_HEAD(zwlisthead, zipwork_entry) zwhead = SLIST_HEAD_INITIALIZER(zwhead);

int dbg_at_times;		/* -D Show details of 'trim_at' code */

int archtodir = 0;		/* Archive old logfiles to other directory */
int createlogs;			/* Create (non-GLOB) logfiles which do not */
				/*    already exist.  1=='for entries with */
				/*    C flag', 2=='for all entries'. */
int verbose = 0;		/* Print out what's going on */
int needroot = 1;		/* Root privs are necessary */
int noaction = 0;		/* Don't do anything, just show it */
int norotate = 0;		/* Don't rotate */
int nosignal;			/* Do not send any signals */
int enforcepid = 0;		/* If PID file does not exist or empty, do nothing */
int force = 0;			/* Force the trim no matter what */
int rotatereq = 0;		/* -R = Always rotate the file(s) as given */
				/*    on the command (this also requires   */
				/*    that a list of files *are* given on  */
				/*    the run command). */
char *requestor;		/* The name given on a -R request */
char *archdirname;		/* Directory path to old logfiles archive */
char *destdir = NULL;		/* Directory to treat at root for logs */
const char *conf;		/* Configuration file to use */

struct ptime_data *dbg_timenow;	/* A "timenow" value set via -D option */
struct ptime_data *timenow;	/* The time to use for checking at-fields */

#define	DAYTIME_LEN	16
char daytime[DAYTIME_LEN];	/* The current time in human readable form,
				 * used for rotation-tracking messages. */
char hostname[MAXHOSTNAMELEN];	/* hostname */

const char *path_syslogpid = _PATH_SYSLOGPID;

static struct conf_entry *get_worklist(char **files);
static void parse_file(FILE *cf, const char *cfname, struct conf_entry **work_p,
		struct conf_entry **glob_p, struct conf_entry **defconf_p);
static char *sob(char *p);
static char *son(char *p);
static int isnumberstr(const char *);
static char *missing_field(char *p, char *errline);
static void	 change_attrs(const char *, const struct conf_entry *);
static fk_entry	 do_entry(struct conf_entry *);
static fk_entry	 do_rotate(const struct conf_entry *);
static void	 do_sigwork(struct sigwork_entry *);
static void	 do_zipwork(struct zipwork_entry *);
static struct sigwork_entry *
		 save_sigwork(const struct conf_entry *);
static struct zipwork_entry *
		 save_zipwork(const struct conf_entry *, const struct
		    sigwork_entry *, int, const char *);
static void	 set_swpid(struct sigwork_entry *, const struct conf_entry *);
static int	 sizefile(const char *);
static void expand_globs(struct conf_entry **work_p,
		struct conf_entry **glob_p);
static void free_clist(struct conf_entry **firstent);
static void free_entry(struct conf_entry *ent);
static struct conf_entry *init_entry(const char *fname,
		struct conf_entry *src_entry);
static void parse_args(int argc, char **argv);
static int parse_doption(const char *doption);
static void usage(void);
static int log_trim(const char *logname, const struct conf_entry *log_ent);
static int age_old_log(char *file);
static void savelog(char *from, char *to);
static void createdir(const struct conf_entry *ent, char *dirpart);
static void createlog(const struct conf_entry *ent);

/*
 * All the following take a parameter of 'int', but expect values in the
 * range of unsigned char.  Define wrappers which take values of type 'char',
 * whether signed or unsigned, and ensure they end up in the right range.
 */
#define	isdigitch(Anychar) isdigit((u_char)(Anychar))
#define	isprintch(Anychar) isprint((u_char)(Anychar))
#define	isspacech(Anychar) isspace((u_char)(Anychar))
#define	tolowerch(Anychar) tolower((u_char)(Anychar))

int
main(int argc, char **argv)
{
	fk_entry free_or_keep;
	struct conf_entry *p, *q;
	struct sigwork_entry *stmp;
	struct zipwork_entry *ztmp;

	SLIST_INIT(&swhead);
	SLIST_INIT(&zwhead);

	parse_args(argc, argv);
	argc -= optind;
	argv += optind;

	if (needroot && getuid() && geteuid())
		errx(1, "must have root privs");
	p = q = get_worklist(argv);

	/*
	 * Rotate all the files which need to be rotated.  Note that
	 * some users have *hundreds* of entries in newsyslog.conf!
	 */
	while (p) {
		free_or_keep = do_entry(p);
		p = p->next;
		if (free_or_keep == FREE_ENT)
			free_entry(q);
		q = p;
	}

	/*
	 * Send signals to any processes which need a signal to tell
	 * them to close and re-open the log file(s) we have rotated.
	 * Note that zipwork_entries include pointers to these
	 * sigwork_entry's, so we can not free the entries here.
	 */
	if (!SLIST_EMPTY(&swhead)) {
		if (noaction || verbose)
			printf("Signal all daemon process(es)...\n");
		SLIST_FOREACH(stmp, &swhead, sw_nextp)
			do_sigwork(stmp);
		if (noaction)
			printf("\tsleep 10\n");
		else {
			if (verbose)
				printf("Pause 10 seconds to allow daemon(s)"
				    " to close log file(s)\n");
			sleep(10);
		}
	}
	/*
	 * Compress all files that we're expected to compress, now
	 * that all processes should have closed the files which
	 * have been rotated.
	 */
	if (!SLIST_EMPTY(&zwhead)) {
		if (noaction || verbose)
			printf("Compress all rotated log file(s)...\n");
		while (!SLIST_EMPTY(&zwhead)) {
			ztmp = SLIST_FIRST(&zwhead);
			do_zipwork(ztmp);
			SLIST_REMOVE_HEAD(&zwhead, zw_nextp);
			free(ztmp);
		}
	}
	/* Now free all the sigwork entries. */
	while (!SLIST_EMPTY(&swhead)) {
		stmp = SLIST_FIRST(&swhead);
		SLIST_REMOVE_HEAD(&swhead, sw_nextp);
		free(stmp);
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

	if (destdir == NULL || fname[0] != '/')
		tempwork->log = strdup(fname);
	else
		asprintf(&tempwork->log, "%s%s", destdir, fname);
	if (tempwork->log == NULL)
		err(1, "strdup for %s", fname);

	if (src_entry != NULL) {
		tempwork->pid_file = NULL;
		if (src_entry->pid_file)
			tempwork->pid_file = strdup(src_entry->pid_file);
		tempwork->r_reason = NULL;
		tempwork->firstcreate = 0;
		tempwork->rotate = 0;
		tempwork->fsize = -1;
		tempwork->uid = src_entry->uid;
		tempwork->gid = src_entry->gid;
		tempwork->numlogs = src_entry->numlogs;
		tempwork->trsize = src_entry->trsize;
		tempwork->hours = src_entry->hours;
		tempwork->trim_at = NULL;
		if (src_entry->trim_at != NULL)
			tempwork->trim_at = ptime_init(src_entry->trim_at);
		tempwork->permissions = src_entry->permissions;
		tempwork->flags = src_entry->flags;
		tempwork->sig = src_entry->sig;
		tempwork->def_cfg = src_entry->def_cfg;
	} else {
		/* Initialize as a "do-nothing" entry */
		tempwork->pid_file = NULL;
		tempwork->r_reason = NULL;
		tempwork->firstcreate = 0;
		tempwork->rotate = 0;
		tempwork->fsize = -1;
		tempwork->uid = (uid_t)-1;
		tempwork->gid = (gid_t)-1;
		tempwork->numlogs = 1;
		tempwork->trsize = -1;
		tempwork->hours = -1;
		tempwork->trim_at = NULL;
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

	if (ent->trim_at != NULL) {
		ptime_free(ent->trim_at);
		ent->trim_at = NULL;
	}

	free(ent);
}

static void
free_clist(struct conf_entry **firstent)
{
	struct conf_entry *ent, *nextent;

	if (firstent == NULL)
		return;			/* There is nothing to do. */

	ent = *firstent;
	firstent = NULL;

	while (ent) {
		nextent = ent->next;
		free_entry(ent);
		ent = nextent;
	}
}

static fk_entry
do_entry(struct conf_entry * ent)
{
#define	REASON_MAX	80
	int modtime;
	fk_entry free_or_keep;
	double diffsecs;
	char temp_reason[REASON_MAX];

	free_or_keep = FREE_ENT;
	if (verbose) {
		if (ent->flags & CE_COMPACT)
			printf("%s <%dZ>: ", ent->log, ent->numlogs);
		else if (ent->flags & CE_BZCOMPACT)
			printf("%s <%dJ>: ", ent->log, ent->numlogs);
		else
			printf("%s <%d>: ", ent->log, ent->numlogs);
	}
	ent->fsize = sizefile(ent->log);
	modtime = age_old_log(ent->log);
	ent->rotate = 0;
	ent->firstcreate = 0;
	if (ent->fsize < 0) {
		/*
		 * If either the C flag or the -C option was specified,
		 * and if we won't be creating the file, then have the
		 * verbose message include a hint as to why the file
		 * will not be created.
		 */
		temp_reason[0] = '\0';
		if (createlogs > 1)
			ent->firstcreate = 1;
		else if ((ent->flags & CE_CREATE) && createlogs)
			ent->firstcreate = 1;
		else if (ent->flags & CE_CREATE)
			strlcpy(temp_reason, " (no -C option)", REASON_MAX);
		else if (createlogs)
			strlcpy(temp_reason, " (no C flag)", REASON_MAX);

		if (ent->firstcreate) {
			if (verbose)
				printf("does not exist -> will create.\n");
			createlog(ent);
		} else if (verbose) {
			printf("does not exist, skipped%s.\n", temp_reason);
		}
	} else {
		if (ent->flags & CE_TRIMAT && !force && !rotatereq) {
			diffsecs = ptimeget_diff(timenow, ent->trim_at);
			if (diffsecs < 0.0) {
				/* trim_at is some time in the future. */
				if (verbose) {
					ptime_adjust4dst(ent->trim_at,
					    timenow);
					printf("--> will trim at %s",
					    ptimeget_ctime(ent->trim_at));
				}
				return (free_or_keep);
			} else if (diffsecs >= 3600.0) {
				/*
				 * trim_at is more than an hour in the past,
				 * so find the next valid trim_at time, and
				 * tell the user what that will be.
				 */
				if (verbose && dbg_at_times)
					printf("\n\t--> prev trim at %s\t",
					    ptimeget_ctime(ent->trim_at));
				if (verbose) {
					ptimeset_nxtime(ent->trim_at);
					printf("--> will trim at %s",
					    ptimeget_ctime(ent->trim_at));
				}
				return (free_or_keep);
			} else if (verbose && noaction && dbg_at_times) {
				/*
				 * If we are just debugging at-times, then
				 * a detailed message is helpful.  Also
				 * skip "doing" any commands, since they
				 * would all be turned off by no-action.
				 */
				printf("\n\t--> timematch at %s",
				    ptimeget_ctime(ent->trim_at));
				return (free_or_keep);
			} else if (verbose && ent->hours <= 0) {
				printf("--> time is up\n");
			}
		}
		if (verbose && (ent->trsize > 0))
			printf("size (Kb): %d [%d] ", ent->fsize, ent->trsize);
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
		} else if ((ent->trsize > 0) && (ent->fsize >= ent->trsize)) {
			ent->rotate = 1;
			snprintf(temp_reason, REASON_MAX, " due to size>%dK",
			    ent->trsize);
		} else if (ent->hours <= 0 && (ent->flags & CE_TRIMAT)) {
			ent->rotate = 1;
		} else if ((ent->hours > 0) && ((modtime >= ent->hours) ||
		    (modtime < 0))) {
			ent->rotate = 1;
		}

		/*
		 * If the file needs to be rotated, then rotate it.
		 */
		if (ent->rotate && !norotate) {
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
			free_or_keep = do_rotate(ent);
		} else {
			if (verbose)
				printf("--> skipping\n");
		}
	}
	return (free_or_keep);
#undef REASON_MAX
}

static void
parse_args(int argc, char **argv)
{
	int ch;
	char *p;

	timenow = ptime_init(NULL);
	ptimeset_time(timenow, time(NULL));
	strlcpy(daytime, ptimeget_ctime(timenow) + 4, DAYTIME_LEN);

	/* Let's get our hostname */
	(void)gethostname(hostname, sizeof(hostname));

	/* Truncate domain */
	if ((p = strchr(hostname, '.')) != NULL)
		*p = '\0';

	/* Parse command line options. */
	while ((ch = getopt(argc, argv, "a:d:f:nrsvCD:FNPR:S:")) != -1)
		switch (ch) {
		case 'a':
			archtodir++;
			archdirname = optarg;
			break;
		case 'd':
			destdir = optarg;
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
		case 'C':
			/* Useful for things like rc.diskless... */
			createlogs++;
			break;
		case 'D':
			/*
			 * Set some debugging option.  The specific option
			 * depends on the value of optarg.  These options
			 * may come and go without notice or documentation.
			 */
			if (parse_doption(optarg))
				break;
			usage();
			/* NOTREACHED */
		case 'F':
			force++;
			break;
		case 'N':
			norotate++;
			break;
		case 'P':
			enforcepid++;
			break;
		case 'R':
			rotatereq++;
			requestor = strdup(optarg);
			break;
		case 'S':
			path_syslogpid = optarg;
			break;
		case 'm':	/* Used by OpenBSD for "monitor mode" */
		default:
			usage();
			/* NOTREACHED */
		}

	if (force && norotate) {
		warnx("Only one of -F and -N may be specified.");
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

	if (dbg_timenow) {
		/*
		 * Note that the 'daytime' variable is not changed.
		 * That is only used in messages that track when a
		 * logfile is rotated, and if a file *is* rotated,
		 * then it will still rotated at the "real now" time.
		 */
		ptime_free(timenow);
		timenow = dbg_timenow;
		fprintf(stderr, "Debug: Running as if TimeNow is %s",
		    ptimeget_ctime(dbg_timenow));
	}

}

/*
 * These debugging options are mainly meant for developer use, such
 * as writing regression-tests.  They would not be needed by users
 * during normal operation of newsyslog...
 */
static int
parse_doption(const char *doption)
{
	const char TN[] = "TN=";
	int res;

	if (strncmp(doption, TN, sizeof(TN) - 1) == 0) {
		/*
		 * The "TimeNow" debugging option.  This might be off
		 * by an hour when crossing a timezone change.
		 */
		dbg_timenow = ptime_init(NULL);
		res = ptime_relparse(dbg_timenow, PTM_PARSE_ISO8601,
		    time(NULL), doption + sizeof(TN) - 1);
		if (res == -2) {
			warnx("Non-existent time specified on -D %s", doption);
			return (0);			/* failure */
		} else if (res < 0) {
			warnx("Malformed time given on -D %s", doption);
			return (0);			/* failure */
		}
		return (1);			/* successfully parsed */

	}

	if (strcmp(doption, "ats") == 0) {
		dbg_at_times++;
		return (1);			/* successfully parsed */
	}

	/* XXX - This check could probably be dropped. */
	if ((strcmp(doption, "neworder") == 0) || (strcmp(doption, "oldorder")
	    == 0)) {
		warnx("NOTE: newsyslog always uses 'neworder'.");
		return (1);			/* successfully parsed */
	}

	warnx("Unknown -D (debug) option: '%s'", doption);
	return (0);				/* failure */
}

static void
usage(void)
{

	fprintf(stderr,
	    "usage: newsyslog [-CFNnrsv] [-a directory] [-d directory] [-f config-file]\n"
	    "                 [-S pidfile] [ [-R requestor] filename ... ]\n");
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
	struct conf_entry *globlist, *lastnew, *worklist;
	int gmatch, fnres;

	defconf = globlist = worklist = NULL;

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
		err(1, "%s", fname);

	parse_file(f, fname, &worklist, &globlist, &defconf);
	(void) fclose(f);

	/*
	 * All config-file information has been read in and turned into
	 * a worklist and a globlist.  If there were no specific files
	 * given on the run command, then the only thing left to do is to
	 * call a routine which finds all files matched by the globlist
	 * and adds them to the worklist.  Then return the worklist.
	 */
	if (*files == NULL) {
		expand_globs(&worklist, &globlist);
		free_clist(&globlist);
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
		defconf->trsize = 50;
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
	firstnew = lastnew = NULL;
	for (given = files; *given; ++given) {
		/*
		 * First try to find exact-matches for this given file.
		 */
		gmatch = 0;
		for (ent = worklist; ent; ent = ent->next) {
			if (strcmp(ent->log, *given) == 0) {
				gmatch++;
				dupent = init_entry(*given, ent);
				if (!firstnew)
					firstnew = dupent;
				else
					lastnew->next = dupent;
				lastnew = dupent;
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
		gmatch = 0;
		if (verbose > 2 && globlist != NULL)
			printf("\t+ Checking globs for %s\n", *given);
		for (ent = globlist; ent; ent = ent->next) {
			fnres = fnmatch(ent->log, *given, FNM_PATHNAME);
			if (verbose > 2)
				printf("\t+    = %d for pattern %s\n", fnres,
				    ent->log);
			if (fnres == 0) {
				gmatch++;
				dupent = init_entry(*given, ent);
				if (!firstnew)
					firstnew = dupent;
				else
					lastnew->next = dupent;
				lastnew = dupent;
				/* This new entry is not a glob! */
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
			lastnew->next = dupent;
		/* Mark that it was *not* found in a config file */
		dupent->def_cfg = 1;
		lastnew = dupent;
	}

	/*
	 * Free all the entries in the original work list, the list of
	 * glob entries, and the default entry.
	 */
	free_clist(&worklist);
	free_clist(&globlist);
	free_entry(defconf);

	/* And finally, return a worklist which matches the given files. */
	return (firstnew);
}

/*
 * Expand the list of entries with filename patterns, and add all files
 * which match those glob-entries onto the worklist.
 */
static void
expand_globs(struct conf_entry **work_p, struct conf_entry **glob_p)
{
	int gmatch, gres;
	size_t i;
	char *mfname;
	struct conf_entry *dupent, *ent, *firstmatch, *globent;
	struct conf_entry *lastmatch;
	glob_t pglob;
	struct stat st_fm;

	if ((glob_p == NULL) || (*glob_p == NULL))
		return;			/* There is nothing to do. */

	/*
	 * The worklist contains all fully-specified (non-GLOB) names.
	 *
	 * Now expand the list of filename-pattern (GLOB) entries into
	 * a second list, which (by definition) will only match files
	 * that already exist.  Do not add a glob-related entry for any
	 * file which already exists in the fully-specified list.
	 */
	firstmatch = lastmatch = NULL;
	for (globent = *glob_p; globent; globent = globent->next) {

		gres = glob(globent->log, GLOB_NOCHECK, NULL, &pglob);
		if (gres != 0) {
			warn("cannot expand pattern (%d): %s", gres,
			    globent->log);
			continue;
		}

		if (verbose > 2)
			printf("\t+ Expanding pattern %s\n", globent->log);
		for (i = 0; i < pglob.gl_matchc; i++) {
			mfname = pglob.gl_pathv[i];

			/* See if this file already has a specific entry. */
			gmatch = 0;
			for (ent = *work_p; ent; ent = ent->next) {
				if (strcmp(mfname, ent->log) == 0) {
					gmatch++;
					break;
				}
			}
			if (gmatch)
				continue;

			/* Make sure the named matched is a file. */
			gres = lstat(mfname, &st_fm);
			if (gres != 0) {
				/* Error on a file that glob() matched?!? */
				warn("Skipping %s - lstat() error", mfname);
				continue;
			}
			if (!S_ISREG(st_fm.st_mode)) {
				/* We only rotate files! */
				if (verbose > 2)
					printf("\t+  . skipping %s (!file)\n",
					    mfname);
				continue;
			}

			if (verbose > 2)
				printf("\t+  . add file %s\n", mfname);
			dupent = init_entry(mfname, globent);
			if (!firstmatch)
				firstmatch = dupent;
			else
				lastmatch->next = dupent;
			lastmatch = dupent;
			/* This new entry is not a glob! */
			dupent->flags &= ~CE_GLOB;
		}
		globfree(&pglob);
		if (verbose > 2)
			printf("\t+ Done with pattern %s\n", globent->log);
	}

	/* Add the list of matched files to the end of the worklist. */
	if (!*work_p)
		*work_p = firstmatch;
	else {
		ent = *work_p;
		while (ent->next)
			ent = ent->next;
		ent->next = firstmatch;
	}

}

/*
 * Parse a configuration file and update a linked list of all the logs to
 * process.
 */
static void
parse_file(FILE *cf, const char *cfname, struct conf_entry **work_p,
    struct conf_entry **glob_p, struct conf_entry **defconf_p)
{
	char line[BUFSIZ], *parse, *q;
	char *cp, *errline, *group;
	struct conf_entry *lastglob, *lastwork, *working;
	struct passwd *pwd;
	struct group *grp;
	int eol, ptm_opts, res, special;

	/*
	 * XXX - for now, assume that only one config file will be read,
	 *	ie, this routine is only called one time.
	 */
	lastglob = lastwork = NULL;

	errline = NULL;
	while (fgets(line, BUFSIZ, cf)) {
		if ((line[0] == '\n') || (line[0] == '#') ||
		    (strlen(line) == 0))
			continue;
		if (errline != NULL)
			free(errline);
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

		/*
		 * Allow people to set debug options via the config file.
		 * (NOTE: debug optons are undocumented, and may disappear
		 * at any time, etc).
		 */
		if (strcasecmp(DEBUG_MARKER, q) == 0) {
			q = parse = missing_field(sob(++parse), errline);
			parse = son(parse);
			if (!*parse)
				warnx("debug line specifies no option:\n%s",
				    errline);
			else {
				*parse = '\0';
				parse_doption(q);
			}
			continue;
		}

		special = 0;
		working = init_entry(q, NULL);
		if (strcasecmp(DEFAULT_MARKER, q) == 0) {
			special = 1;
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
				if (!(isnumberstr(q))) {
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
				if (!(isnumberstr(q))) {
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
		if (isdigitch(*q))
			working->trsize = atoi(q);
		else if (strcmp(q, "*") == 0)
			working->trsize = -1;
		else {
			warnx("Invalid value of '%s' for 'size' in line:\n%s",
			    q, errline);
			working->trsize = -1;
		}

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

			if (*ep == '\0' || strcmp(ep, "*") == 0)
				goto no_trimat;
			if (*ep != '@' && *ep != '$')
				errx(1, "malformed interval/at:\n%s", errline);

			working->flags |= CE_TRIMAT;
			working->trim_at = ptime_init(NULL);
			ptm_opts = PTM_PARSE_ISO8601;
			if (*ep == '$')
				ptm_opts = PTM_PARSE_DWM;
			ptm_opts |= PTM_PARSE_MATCHDOM;
			res = ptime_relparse(working->trim_at, ptm_opts,
			    ptimeget_secs(timenow), ep + 1);
			if (res == -2)
				errx(1, "nonexistent time for 'at' value:\n%s",
				    errline);
			else if (res < 0)
				errx(1, "malformed 'at' value:\n%s", errline);
		}
no_trimat:

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
			case 'c':
				/*
				 * XXX - 	Ick! Ugly! Remove ASAP!
				 * We want `c' and `C' for "create".  But we
				 * will temporarily treat `c' as `g', because
				 * FreeBSD releases <= 4.8 have a typo of
				 * checking  ('G' || 'c')  for CE_GLOB.
				 */
				if (*q == 'c') {
					warnx("Assuming 'g' for 'c' in flags for line:\n%s",
					    errline);
					warnx("The 'c' flag will eventually mean 'CREATE'");
					working->flags |= CE_GLOB;
					break;
				}
				working->flags |= CE_CREATE;
				break;
			case 'd':
				working->flags |= CE_NODUMP;
				break;
			case 'g':
				working->flags |= CE_GLOB;
				break;
			case 'j':
				working->flags |= CE_BZCOMPACT;
				break;
			case 'n':
				working->flags |= CE_NOSIGNAL;
				break;
			case 'u':
				working->flags |= CE_SIGNALGROUP;
				break;
			case 'w':
				/* Depreciated flag - keep for compatibility purposes */
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
			 * specified some other pid-file (and obviously the
			 * syslog pid-file will not be for a process-group).
			 * Also, we should only try to notify syslog if we
			 * are root.
			 */
			if (working->flags & CE_SIGNALGROUP) {
				warnx("Ignoring flag 'U' in line:\n%s",
				    errline);
				working->flags &= ~CE_SIGNALGROUP;
			}
			if (needroot)
				working->pid_file = strdup(path_syslogpid);
		}

		/*
		 * Add this entry to the appropriate list of entries, unless
		 * it was some kind of special entry (eg: <default>).
		 */
		if (special) {
			;			/* Do not add to any list */
		} else if (working->flags & CE_GLOB) {
			if (!*glob_p)
				*glob_p = working;
			else
				lastglob->next = working;
			lastglob = working;
		} else {
			if (!*work_p)
				*work_p = working;
			else
				lastwork->next = working;
			lastwork = working;
		}
	}
	if (errline != NULL)
		free(errline);
}

static char *
missing_field(char *p, char *errline)
{

	if (!p || !*p)
		errx(1, "missing field in config file:\n%s", errline);
	return (p);
}

static fk_entry
do_rotate(const struct conf_entry *ent)
{
	char dirpart[MAXPATHLEN], namepart[MAXPATHLEN];
	char file1[MAXPATHLEN], file2[MAXPATHLEN];
	char zfile1[MAXPATHLEN], zfile2[MAXPATHLEN];
	char jfile1[MAXPATHLEN];
	int flags, numlogs_c;
	fk_entry free_or_keep;
	struct sigwork_entry *swork;
	struct stat st;

	flags = ent->flags;
	free_or_keep = FREE_ENT;

	if (archtodir) {
		char *p;

		/* build complete name of archive directory into dirpart */
		if (*archdirname == '/') {	/* absolute */
			strlcpy(dirpart, archdirname, sizeof(dirpart));
		} else {	/* relative */
			/* get directory part of logfile */
			strlcpy(dirpart, ent->log, sizeof(dirpart));
			if ((p = rindex(dirpart, '/')) == NULL)
				dirpart[0] = '\0';
			else
				*(p + 1) = '\0';
			strlcat(dirpart, archdirname, sizeof(dirpart));
		}

		/* check if archive directory exists, if not, create it */
		if (lstat(dirpart, &st))
			createdir(ent, dirpart);

		/* get filename part of logfile */
		if ((p = rindex(ent->log, '/')) == NULL)
			strlcpy(namepart, ent->log, sizeof(namepart));
		else
			strlcpy(namepart, p + 1, sizeof(namepart));

		/* name of oldest log */
		(void) snprintf(file1, sizeof(file1), "%s/%s.%d", dirpart,
		    namepart, ent->numlogs);
		(void) snprintf(zfile1, sizeof(zfile1), "%s%s", file1,
		    COMPRESS_POSTFIX);
		snprintf(jfile1, sizeof(jfile1), "%s%s", file1,
		    BZCOMPRESS_POSTFIX);
	} else {
		/* name of oldest log */
		(void) snprintf(file1, sizeof(file1), "%s.%d", ent->log,
		    ent->numlogs);
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
	numlogs_c = ent->numlogs;		/* copy for countdown */
	while (numlogs_c--) {

		(void) strlcpy(file2, file1, sizeof(file2));

		if (archtodir)
			(void) snprintf(file1, sizeof(file1), "%s/%s.%d",
			    dirpart, namepart, numlogs_c);
		else
			(void) snprintf(file1, sizeof(file1), "%s.%d",
			    ent->log, numlogs_c);

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
		if (noaction)
			printf("\tmv %s %s\n", zfile1, zfile2);
		else {
			/* XXX - Ought to be checking for failure! */
			(void)rename(zfile1, zfile2);
		}
		change_attrs(zfile2, ent);
	}

	if (ent->numlogs > 0) {
		if (noaction) {
			/*
			 * Note that savelog() may succeed with using link()
			 * for the archtodir case, but there is no good way
			 * of knowing if it will when doing "noaction", so
			 * here we claim that it will have to do a copy...
			 */
			if (archtodir)
				printf("\tcp %s %s\n", ent->log, file1);
			else
				printf("\tln %s %s\n", ent->log, file1);
		} else {
			if (!(flags & CE_BINARY)) {
				/* Report the trimming to the old log */
				log_trim(ent->log, ent);
			}
			savelog(ent->log, file1);
		}
		change_attrs(file1, ent);
	}

	/* Create the new log file and move it into place */
	if (noaction)
		printf("Start new log...\n");
	createlog(ent);

	/*
	 * Save all signalling and file-compression to be done after log
	 * files from all entries have been rotated.  This way any one
	 * process will not be sent the same signal multiple times when
	 * multiple log files had to be rotated.
	 */
	swork = NULL;
	if (ent->pid_file != NULL)
		swork = save_sigwork(ent);
	if (ent->numlogs > 0 && (flags & (CE_COMPACT | CE_BZCOMPACT))) {
		/*
		 * The zipwork_entry will include a pointer to this
		 * conf_entry, so the conf_entry should not be freed.
		 */
		free_or_keep = KEEP_ENT;
		save_zipwork(ent, swork, ent->fsize, file1);
	}

	return (free_or_keep);
}

static void
do_sigwork(struct sigwork_entry *swork)
{
	struct sigwork_entry *nextsig;
	int kres, secs;

	if (!(swork->sw_pidok) || swork->sw_pid == 0)
		return;			/* no work to do... */

	/*
	 * If nosignal (-s) was specified, then do not signal any process.
	 * Note that a nosignal request triggers a warning message if the
	 * rotated logfile needs to be compressed, *unless* -R was also
	 * specified.  We assume that an `-sR' request came from a process
	 * which writes to the logfile, and as such, we assume that process
	 * has already made sure the logfile is not presently in use.  This
	 * just sets swork->sw_pidok to a special value, and do_zipwork
	 * will print any necessary warning(s).
	 */
	if (nosignal) {
		if (!rotatereq)
			swork->sw_pidok = -1;
		return;
	}

	/*
	 * Compute the pause between consecutive signals.  Use a longer
	 * sleep time if we will be sending two signals to the same
	 * deamon or process-group.
	 */
	secs = 0;
	nextsig = SLIST_NEXT(swork, sw_nextp);
	if (nextsig != NULL) {
		if (swork->sw_pid == nextsig->sw_pid)
			secs = 10;
		else
			secs = 1;
	}

	if (noaction) {
		printf("\tkill -%d %d \t\t# %s\n", swork->sw_signum,
		    (int)swork->sw_pid, swork->sw_fname);
		if (secs > 0)
			printf("\tsleep %d\n", secs);
		return;
	}

	kres = kill(swork->sw_pid, swork->sw_signum);
	if (kres != 0) {
		/*
		 * Assume that "no such process" (ESRCH) is something
		 * to warn about, but is not an error.  Presumably the
		 * process which writes to the rotated log file(s) is
		 * gone, in which case we should have no problem with
		 * compressing the rotated log file(s).
		 */
		if (errno != ESRCH)
			swork->sw_pidok = 0;
		warn("can't notify %s, pid %d", swork->sw_pidtype,
		    (int)swork->sw_pid);
	} else {
		if (verbose)
			printf("Notified %s pid %d = %s\n", swork->sw_pidtype,
			    (int)swork->sw_pid, swork->sw_fname);
		if (secs > 0) {
			if (verbose)
				printf("Pause %d second(s) between signals\n",
				    secs);
			sleep(secs);
		}
	}
}

static void
do_zipwork(struct zipwork_entry *zwork)
{
	const char *pgm_name, *pgm_path;
	int errsav, fcount, zstatus;
	pid_t pidzip, wpid;
	char zresult[MAXPATHLEN];

	pgm_path = NULL;
	strlcpy(zresult, zwork->zw_fname, sizeof(zresult));
	if (zwork != NULL && zwork->zw_conf != NULL) {
		if (zwork->zw_conf->flags & CE_COMPACT) {
			pgm_path = _PATH_GZIP;
			strlcat(zresult, COMPRESS_POSTFIX, sizeof(zresult));
		} else if (zwork->zw_conf->flags & CE_BZCOMPACT) {
			pgm_path = _PATH_BZIP2;
			strlcat(zresult, BZCOMPRESS_POSTFIX, sizeof(zresult));
		}
	}
	if (pgm_path == NULL) {
		warnx("invalid entry for %s in do_zipwork", zwork->zw_fname);
		return;
	}
	pgm_name = strrchr(pgm_path, '/');
	if (pgm_name == NULL)
		pgm_name = pgm_path;
	else
		pgm_name++;

	if (zwork->zw_swork != NULL && zwork->zw_swork->sw_pidok <= 0) {
		warnx(
		    "log %s not compressed because daemon(s) not notified",
		    zwork->zw_fname);
		change_attrs(zwork->zw_fname, zwork->zw_conf);
		return;
	}

	if (noaction) {
		printf("\t%s %s\n", pgm_name, zwork->zw_fname);
		change_attrs(zresult, zwork->zw_conf);
		return;
	}

	fcount = 1;
	pidzip = fork();
	while (pidzip < 0) {
		/*
		 * The fork failed.  If the failure was due to a temporary
		 * problem, then wait a short time and try it again.
		 */
		errsav = errno;
		warn("fork() for `%s %s'", pgm_name, zwork->zw_fname);
		if (errsav != EAGAIN || fcount > 5)
			errx(1, "Exiting...");
		sleep(fcount * 12);
		fcount++;
		pidzip = fork();
	}
	if (!pidzip) {
		/* The child process executes the compression command */
		execl(pgm_path, pgm_path, "-f", zwork->zw_fname, (char *)0);
		err(1, "execl(`%s -f %s')", pgm_path, zwork->zw_fname);
	}

	wpid = waitpid(pidzip, &zstatus, 0);
	if (wpid == -1) {
		/* XXX - should this be a fatal error? */
		warn("%s: waitpid(%d)", pgm_path, pidzip);
		return;
	}
	if (!WIFEXITED(zstatus)) {
		warnx("`%s -f %s' did not terminate normally", pgm_name,
		    zwork->zw_fname);
		return;
	}
	if (WEXITSTATUS(zstatus)) {
		warnx("`%s -f %s' terminated with a non-zero status (%d)",
		    pgm_name, zwork->zw_fname, WEXITSTATUS(zstatus));
		return;
	}

	/* Compression was successful, set file attributes on the result. */
	change_attrs(zresult, zwork->zw_conf);
}

/*
 * Save information on any process we need to signal.  Any single
 * process may need to be sent different signal-values for different
 * log files, but usually a single signal-value will cause the process
 * to close and re-open all of it's log files.
 */
static struct sigwork_entry *
save_sigwork(const struct conf_entry *ent)
{
	struct sigwork_entry *sprev, *stmp;
	int ndiff;
	size_t tmpsiz;

	sprev = NULL;
	ndiff = 1;
	SLIST_FOREACH(stmp, &swhead, sw_nextp) {
		ndiff = strcmp(ent->pid_file, stmp->sw_fname);
		if (ndiff > 0)
			break;
		if (ndiff == 0) {
			if (ent->sig == stmp->sw_signum)
				break;
			if (ent->sig > stmp->sw_signum) {
				ndiff = 1;
				break;
			}
		}
		sprev = stmp;
	}
	if (stmp != NULL && ndiff == 0)
		return (stmp);

	tmpsiz = sizeof(struct sigwork_entry) + strlen(ent->pid_file) + 1;
	stmp = malloc(tmpsiz);
	set_swpid(stmp, ent);
	stmp->sw_signum = ent->sig;
	strcpy(stmp->sw_fname, ent->pid_file);
	if (sprev == NULL)
		SLIST_INSERT_HEAD(&swhead, stmp, sw_nextp);
	else
		SLIST_INSERT_AFTER(sprev, stmp, sw_nextp);
	return (stmp);
}

/*
 * Save information on any file we need to compress.  We may see the same
 * file multiple times, so check the full list to avoid duplicates.  The
 * list itself is sorted smallest-to-largest, because that's the order we
 * want to compress the files.  If the partition is very low on disk space,
 * then the smallest files are the most likely to compress, and compressing
 * them first will free up more space for the larger files.
 */
static struct zipwork_entry *
save_zipwork(const struct conf_entry *ent, const struct sigwork_entry *swork,
    int zsize, const char *zipfname)
{
	struct zipwork_entry *zprev, *ztmp;
	int ndiff;
	size_t tmpsiz;

	/* Compute the size if the caller did not know it. */
	if (zsize < 0)
		zsize = sizefile(zipfname);

	zprev = NULL;
	ndiff = 1;
	SLIST_FOREACH(ztmp, &zwhead, zw_nextp) {
		ndiff = strcmp(zipfname, ztmp->zw_fname);
		if (ndiff == 0)
			break;
		if (zsize > ztmp->zw_fsize)
			zprev = ztmp;
	}
	if (ztmp != NULL && ndiff == 0)
		return (ztmp);

	tmpsiz = sizeof(struct zipwork_entry) + strlen(zipfname) + 1;
	ztmp = malloc(tmpsiz);
	ztmp->zw_conf = ent;
	ztmp->zw_swork = swork;
	ztmp->zw_fsize = zsize;
	strcpy(ztmp->zw_fname, zipfname);
	if (zprev == NULL)
		SLIST_INSERT_HEAD(&zwhead, ztmp, zw_nextp);
	else
		SLIST_INSERT_AFTER(zprev, ztmp, zw_nextp);
	return (ztmp);
}

/* Send a signal to the pid specified by pidfile */
static void
set_swpid(struct sigwork_entry *swork, const struct conf_entry *ent)
{
	FILE *f;
	long minok, maxok, rval;
	char *endp, *linep, line[BUFSIZ];

	minok = MIN_PID;
	maxok = MAX_PID;
	swork->sw_pidok = 0;
	swork->sw_pid = 0;
	swork->sw_pidtype = "daemon";
	if (ent->flags & CE_SIGNALGROUP) {
		/*
		 * If we are expected to signal a process-group when
		 * rotating this logfile, then the value read in should
		 * be the negative of a valid process ID.
		 */
		minok = -MAX_PID;
		maxok = -MIN_PID;
		swork->sw_pidtype = "process-group";
	}

	f = fopen(ent->pid_file, "r");
	if (f == NULL) {
		if (errno == ENOENT && enforcepid == 0) {
			/*
			 * Warn if the PID file doesn't exist, but do
			 * not consider it an error.  Most likely it
			 * means the process has been terminated,
			 * so it should be safe to rotate any log
			 * files that the process would have been using.
			 */
			swork->sw_pidok = 1;
			warnx("pid file doesn't exist: %s", ent->pid_file);
		} else
			warn("can't open pid file: %s", ent->pid_file);
		return;
	}

	if (fgets(line, BUFSIZ, f) == NULL) {
		/*
		 * Warn if the PID file is empty, but do not consider
		 * it an error.  Most likely it means the process has
		 * has terminated, so it should be safe to rotate any
		 * log files that the process would have been using.
		 */
		if (feof(f) && enforcepid == 0) {
			swork->sw_pidok = 1;
			warnx("pid file is empty: %s", ent->pid_file);
		} else
			warn("can't read from pid file: %s", ent->pid_file);
		(void)fclose(f);
		return;
	}
	(void)fclose(f);

	errno = 0;
	linep = line;
	while (*linep == ' ')
		linep++;
	rval = strtol(linep, &endp, 10);
	if (*endp != '\0' && !isspacech(*endp)) {
		warnx("pid file does not start with a valid number: %s",
		    ent->pid_file);
	} else if (rval < minok || rval > maxok) {
		warnx("bad value '%ld' for process number in %s",
		    rval, ent->pid_file);
		if (verbose)
			warnx("\t(expecting value between %ld and %ld)",
			    minok, maxok);
	} else {
		swork->sw_pidok = 1;
		swork->sw_pid = rval;
	}

	return;
}

/* Log the fact that the logs were turned over */
static int
log_trim(const char *logname, const struct conf_entry *log_ent)
{
	FILE *f;
	const char *xtra;

	if ((f = fopen(logname, "a")) == NULL)
		return (-1);
	xtra = "";
	if (log_ent->def_cfg)
		xtra = " using <default> rule";
	if (log_ent->firstcreate)
		fprintf(f, "%s %s newsyslog[%d]: logfile first created%s\n",
		    daytime, hostname, (int) getpid(), xtra);
	else if (log_ent->r_reason != NULL)
		fprintf(f, "%s %s newsyslog[%d]: logfile turned over%s%s\n",
		    daytime, hostname, (int) getpid(), log_ent->r_reason, xtra);
	else
		fprintf(f, "%s %s newsyslog[%d]: logfile turned over%s\n",
		    daytime, hostname, (int) getpid(), xtra);
	if (fclose(f) == EOF)
		err(1, "log_trim: fclose");
	return (0);
}

/* Return size in kilobytes of a file */
static int
sizefile(const char *file)
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
	char *endp;
	char tmp[MAXPATHLEN + sizeof(".0") + sizeof(COMPRESS_POSTFIX) +
		sizeof(BZCOMPRESS_POSTFIX) + 1];

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

	strlcat(tmp, ".0", sizeof(tmp));
	if (stat(tmp, &sb) < 0) {
		/*
		 * A plain '.0' file does not exist.  Try again, first
		 * with the added suffix of '.gz', then with an added
		 * suffix of '.bz2' instead of '.gz'.
		 */
		endp = strchr(tmp, '\0');
		strlcat(tmp, COMPRESS_POSTFIX, sizeof(tmp));
		if (stat(tmp, &sb) < 0) {
			*endp = '\0';		/* Remove .gz */
			strlcat(tmp, BZCOMPRESS_POSTFIX, sizeof(tmp));
			if (stat(tmp, &sb) < 0)
				return (-1);
		}
	}
	return ((int)(ptimeget_secs(timenow) - sb.st_mtime + 1800) / 3600);
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

/* Check if string is actually a number */
static int
isnumberstr(const char *string)
{
	while (*string) {
		if (!isdigitch(*string++))
			return (0);
	}
	return (1);
}

/*
 * Save the active log file under a new name.  A link to the new name
 * is the quick-and-easy way to do this.  If that fails (which it will
 * if the destination is on another partition), then make a copy of
 * the file to the new location.
 */
static void
savelog(char *from, char *to)
{
	FILE *src, *dst;
	int c, res;

	res = link(from, to);
	if (res == 0)
		return;

	if ((src = fopen(from, "r")) == NULL)
		err(1, "can't fopen %s for reading", from);
	if ((dst = fopen(to, "w")) == NULL)
		err(1, "can't fopen %s for writing", to);

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
}

/* create one or more directory components of a path */
static void
createdir(const struct conf_entry *ent, char *dirpart)
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
	if (verbose) {
		if (ent->firstcreate)
			printf("Created directory '%s' for new %s\n",
			    dirpart, ent->log);
		else
			printf("Created directory '%s' for -a\n", dirpart);
	}
}

/*
 * Create a new log file, destroying any currently-existing version
 * of the log file in the process.  If the caller wants a backup copy
 * of the file to exist, they should call 'link(logfile,logbackup)'
 * before calling this routine.
 */
void
createlog(const struct conf_entry *ent)
{
	int fd, failed;
	struct stat st;
	char *realfile, *slash, tempfile[MAXPATHLEN];

	fd = -1;
	realfile = ent->log;

	/*
	 * If this log file is being created for the first time (-C option),
	 * then it may also be true that the parent directory does not exist
	 * yet.  Check, and create that directory if it is missing.
	 */
	if (ent->firstcreate) {
		strlcpy(tempfile, realfile, sizeof(tempfile));
		slash = strrchr(tempfile, '/');
		if (slash != NULL) {
			*slash = '\0';
			failed = stat(tempfile, &st);
			if (failed && errno != ENOENT)
				err(1, "Error on stat(%s)", tempfile);
			if (failed)
				createdir(ent, tempfile);
			else if (!S_ISDIR(st.st_mode))
				errx(1, "%s exists but is not a directory",
				    tempfile);
		}
	}

	/*
	 * First create an unused filename, so it can be chown'ed and
	 * chmod'ed before it is moved into the real location.  mkstemp
	 * will create the file mode=600 & owned by us.  Note that all
	 * temp files will have a suffix of '.z<something>'.
	 */
	strlcpy(tempfile, realfile, sizeof(tempfile));
	strlcat(tempfile, ".zXXXXXX", sizeof(tempfile));
	if (noaction)
		printf("\tmktemp %s\n", tempfile);
	else {
		fd = mkstemp(tempfile);
		if (fd < 0)
			err(1, "can't mkstemp logfile %s", tempfile);

		/*
		 * Add status message to what will become the new log file.
		 */
		if (!(ent->flags & CE_BINARY)) {
			if (log_trim(tempfile, ent))
				err(1, "can't add status message to log");
		}
	}

	/* Change the owner/group, if we are supposed to */
	if (ent->uid != (uid_t)-1 || ent->gid != (gid_t)-1) {
		if (noaction)
			printf("\tchown %u:%u %s\n", ent->uid, ent->gid,
			    tempfile);
		else {
			failed = fchown(fd, ent->uid, ent->gid);
			if (failed)
				err(1, "can't fchown temp file %s", tempfile);
		}
	}

	/* Turn on NODUMP if it was requested in the config-file. */
	if (ent->flags & CE_NODUMP) {
		if (noaction)
			printf("\tchflags nodump %s\n", tempfile);
		else {
			failed = fchflags(fd, UF_NODUMP);
			if (failed) {
				warn("log_trim: fchflags(NODUMP)");
			}
		}
	}

	/*
	 * Note that if the real logfile still exists, and if the call
	 * to rename() fails, then "neither the old file nor the new
	 * file shall be changed or created" (to quote the standard).
	 * If the call succeeds, then the file will be replaced without
	 * any window where some other process might find that the file
	 * did not exist.
	 * XXX - ? It may be that for some error conditions, we could
	 *	retry by first removing the realfile and then renaming.
	 */
	if (noaction) {
		printf("\tchmod %o %s\n", ent->permissions, tempfile);
		printf("\tmv %s %s\n", tempfile, realfile);
	} else {
		failed = fchmod(fd, ent->permissions);
		if (failed)
			err(1, "can't fchmod temp file '%s'", tempfile);
		failed = rename(tempfile, realfile);
		if (failed)
			err(1, "can't mv %s to %s", tempfile, realfile);
	}

	if (fd >= 0)
		close(fd);
}

/*
 * Change the attributes of a given filename to what was specified in
 * the newsyslog.conf entry.  This routine is only called for files
 * that newsyslog expects that it has created, and thus it is a fatal
 * error if this routine finds that the file does not exist.
 */
static void
change_attrs(const char *fname, const struct conf_entry *ent)
{
	int failed;

	if (noaction) {
		printf("\tchmod %o %s\n", ent->permissions, fname);

		if (ent->uid != (uid_t)-1 || ent->gid != (gid_t)-1)
			printf("\tchown %u:%u %s\n",
			    ent->uid, ent->gid, fname);

		if (ent->flags & CE_NODUMP)
			printf("\tchflags nodump %s\n", fname);
		return;
	}

	failed = chmod(fname, ent->permissions);
	if (failed) {
		if (errno != EPERM)
			err(1, "chmod(%s) in change_attrs", fname);
		warn("change_attrs couldn't chmod(%s)", fname);
	}

	if (ent->uid != (uid_t)-1 || ent->gid != (gid_t)-1) {
		failed = chown(fname, ent->uid, ent->gid);
		if (failed)
			warn("can't chown %s", fname);
	}

	if (ent->flags & CE_NODUMP) {
		failed = chflags(fname, UF_NODUMP);
		if (failed)
			warn("can't chflags %s NODUMP", fname);
	}
}
