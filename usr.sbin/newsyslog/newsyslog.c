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

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <grp.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/wait.h>

#include "pathnames.h"

#define kbytes(size)  (((size) + 1023) >> 10)

#ifdef _IBMR2
/* Calculates (db * DEV_BSIZE) */
#define dbtob(db)  ((unsigned)(db) << UBSHIFT)
#endif

#define CE_COMPACT 1		/* Compact the achived log files */
#define CE_BINARY  2		/* Logfile is in binary, don't add */
				/*  status messages */
#define	CE_TRIMAT  4		/* trim at a specific time */

#define NONE -1

struct conf_entry {
	char *log;		/* Name of the log */
	char *pid_file;		/* PID file */
	int uid;		/* Owner of log */
	int gid;		/* Group of log */
	int numlogs;		/* Number of logs to keep */
	int size;		/* Size cutoff to trigger trimming the log */
	int hours;		/* Hours between log trimming */
	time_t trim_at;		/* Specific time to do trimming */
	int permissions;	/* File permissions on the log */
	int flags;		/* Flags (CE_COMPACT & CE_BINARY)  */
	int sig;		/* Signal to send */
	struct conf_entry *next;/* Linked list pointer */
};

int archtodir = 0;		/* Archive old logfiles to other directory */
int verbose = 0;		/* Print out what's going on */
int needroot = 1;		/* Root privs are necessary */
int noaction = 0;		/* Don't do anything, just show it */
int force = 0;			/* Force the trim no matter what */
char *archdirname;		/* Directory path to old logfiles archive */
char *conf = _PATH_CONF;	/* Configuration file to use */
time_t timenow;

#define MIN_PID         5
#define MAX_PID		99999	/* was lower, see /usr/include/sys/proc.h */
char hostname[MAXHOSTNAMELEN + 1];	/* hostname */
char *daytime;			/* timenow in human readable form */

static struct conf_entry *parse_file(char **files);
static char *sob(char *p);
static char *son(char *p);
static char *missing_field(char *p, char *errline);
static void do_entry(struct conf_entry * ent);
static void PRS(int argc, char **argv);
static void usage();
static void dotrim(char *log, char *pid_file, int numdays, int falgs, int perm, int owner_uid, int group_gid, int sig);
static int log_trim(char *log);
static void compress_log(char *log);
static int sizefile(char *file);
static int age_old_log(char *file);
static pid_t get_pid(char *pid_file);
static time_t parse8601(char *s);
static void movefile(char *from, char *to, int perm, int owner_uid, int group_gid);
static void createdir(char *dirpart);
static time_t parseDWM(char *s);

int
main(int argc, char **argv)
{
	struct conf_entry *p, *q;

	PRS(argc, argv);
	if (needroot && getuid() && geteuid())
		errx(1, "must have root privs");
	p = q = parse_file(argv + optind);

	while (p) {
		do_entry(p);
		p = p->next;
		free((char *) q);
		q = p;
	}
	return (0);
}

static void
do_entry(struct conf_entry * ent)
{
	int size, modtime;
	char *pid_file;

	if (verbose) {
		if (ent->flags & CE_COMPACT)
			printf("%s <%dZ>: ", ent->log, ent->numlogs);
		else
			printf("%s <%d>: ", ent->log, ent->numlogs);
	}
	size = sizefile(ent->log);
	modtime = age_old_log(ent->log);
	if (size < 0) {
		if (verbose)
			printf("does not exist.\n");
	} else {
		if (ent->flags & CE_TRIMAT) {
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
		if (force || ((ent->size > 0) && (size >= ent->size)) ||
		    (ent->hours <= 0 && (ent->flags & CE_TRIMAT)) ||
		    ((ent->hours > 0) && ((modtime >= ent->hours)
			    || (modtime < 0)))) {
			if (verbose)
				printf("--> trimming log....\n");
			if (noaction && !verbose) {
				if (ent->flags & CE_COMPACT)
					printf("%s <%dZ>: trimming\n",
					    ent->log, ent->numlogs);
				else
					printf("%s <%d>: trimming\n",
					    ent->log, ent->numlogs);
			}
			if (ent->pid_file) {
				pid_file = ent->pid_file;
			} else {
				/* Only try to notify syslog if we are root */
				if (needroot)
					pid_file = _PATH_SYSLOGPID;
				else
					pid_file = NULL;
			}
			dotrim(ent->log, pid_file, ent->numlogs,
			    ent->flags, ent->permissions, ent->uid, ent->gid, ent->sig);
		} else {
			if (verbose)
				printf("--> skipping\n");
		}
	}
}

static void
PRS(int argc, char **argv)
{
	int c;
	char *p;

	timenow = time((time_t *) 0);
	daytime = ctime(&timenow) + 4;
	daytime[15] = '\0';

	/* Let's get our hostname */
	(void) gethostname(hostname, sizeof(hostname));

	/* Truncate domain */
	if ((p = strchr(hostname, '.'))) {
		*p = '\0';
	}
	while ((c = getopt(argc, argv, "nrvFf:a:t:")) != -1)
		switch (c) {
		case 'n':
			noaction++;
			break;
		case 'a':
			archtodir++;
			archdirname = optarg;
			break;
		case 'r':
			needroot = 0;
			break;
		case 'v':
			verbose++;
			break;
		case 'f':
			conf = optarg;
			break;
		case 'F':
			force++;
			break;
		default:
			usage();
		}
}

static void
usage(void)
{
	fprintf(stderr, "usage: newsyslog [-Fnrv] [-f config-file] [-a directory]\n");
	exit(1);
}

/*
 * Parse a configuration file and return a linked list of all the logs to
 * process
 */
static struct conf_entry *
parse_file(char **files)
{
	FILE *f;
	char line[BUFSIZ], *parse, *q;
	char *errline, *group;
	char **p;
	struct conf_entry *first = NULL;
	struct conf_entry *working = NULL;
	struct passwd *pass;
	struct group *grp;
	int eol;

	if (strcmp(conf, "-"))
		f = fopen(conf, "r");
	else
		f = stdin;
	if (!f)
		err(1, "%s", conf);
	while (fgets(line, BUFSIZ, f)) {
		if ((line[0] == '\n') || (line[0] == '#'))
			continue;
		errline = strdup(line);

		q = parse = missing_field(sob(line), errline);
		parse = son(line);
		if (!*parse)
			errx(1, "malformed line (missing fields):\n%s", errline);
		*parse = '\0';

		if (*files) {
			for (p = files; *p; ++p)
				if (strcmp(*p, q) == 0)
					break;
			if (!*p)
				continue;
		}

		if (!first) {
			working = (struct conf_entry *) malloc(sizeof(struct conf_entry));
			first = working;
		} else {
			working->next = (struct conf_entry *) malloc(sizeof(struct conf_entry));
			working = working->next;
		}
		working->log = strdup(q);

		q = parse = missing_field(sob(++parse), errline);
		parse = son(parse);
		if (!*parse)
			errx(1, "malformed line (missing fields):\n%s", errline);
		*parse = '\0';
		if ((group = strchr(q, ':')) != NULL ||
		    (group = strrchr(q, '.')) != NULL) {
			*group++ = '\0';
			if (*q) {
				if (!(isnumber(*q))) {
					if ((pass = getpwnam(q)) == NULL)
						errx(1,
						    "error in config file; unknown user:\n%s",
						    errline);
					working->uid = pass->pw_uid;
				} else
					working->uid = atoi(q);
			} else
				working->uid = NONE;

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
				working->gid = NONE;

			q = parse = missing_field(sob(++parse), errline);
			parse = son(parse);
			if (!*parse)
				errx(1, "malformed line (missing fields):\n%s", errline);
			*parse = '\0';
		} else
			working->uid = working->gid = NONE;

		if (!sscanf(q, "%o", &working->permissions))
			errx(1, "error in config file; bad permissions:\n%s",
			    errline);

		q = parse = missing_field(sob(++parse), errline);
		parse = son(parse);
		if (!*parse)
			errx(1, "malformed line (missing fields):\n%s", errline);
		*parse = '\0';
		if (!sscanf(q, "%d", &working->numlogs))
			errx(1, "error in config file; bad number:\n%s",
			    errline);

		q = parse = missing_field(sob(++parse), errline);
		parse = son(parse);
		if (!*parse)
			errx(1, "malformed line (missing fields):\n%s", errline);
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

			if (*ep != '\0' && *ep != '@' && *ep != '*' && *ep != '$')
				errx(1, "malformed interval/at:\n%s", errline);
			if (*ep == '@') {
				if ((working->trim_at = parse8601(ep + 1))
				    == (time_t) - 1)
					errx(1, "malformed at:\n%s", errline);
				working->flags |= CE_TRIMAT;
			} else if (*ep == '$') {
				if ((working->trim_at = parseDWM(ep + 1))
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

		while (q && *q && !isspace(*q)) {
			if ((*q == 'Z') || (*q == 'z'))
				working->flags |= CE_COMPACT;
			else if ((*q == 'B') || (*q == 'b'))
				working->flags |= CE_BINARY;
			else if (*q != '-')
				errx(1, "illegal flag in config file -- %c", *q);
			q++;
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
				errx(1, "illegal pid file or signal number in config file:\n%s", errline);
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
				errx(1, "illegal signal number in config file:\n%s", errline);
			}
			if (working->sig < 1 || working->sig >= NSIG)
				goto err_sig;
		}
		free(errline);
	}
	if (working)
		working->next = (struct conf_entry *) NULL;
	(void) fclose(f);
	return (first);
}

static char *
missing_field(char *p, char *errline)
{
	if (!p || !*p)
		errx(1, "missing field in config file:\n%s", errline);
	return (p);
}

static void
dotrim(char *log, char *pid_file, int numdays, int flags, int perm,
    int owner_uid, int group_gid, int sig)
{
	char dirpart[MAXPATHLEN + 1], namepart[MAXPATHLEN + 1];
	char file1[MAXPATHLEN + 1], file2[MAXPATHLEN + 1];
	char zfile1[MAXPATHLEN + 1], zfile2[MAXPATHLEN + 1];
	int notified, need_notification, fd, _numdays;
	struct stat st;
	pid_t pid;

#ifdef _IBMR2
	/*
	 * AIX 3.1 has a broken fchown- if the owner_uid is -1, it will
	 * actually change it to be owned by uid -1, instead of leaving it
	 * as is, as it is supposed to.
	 */
	if (owner_uid == -1)
		owner_uid = geteuid();
#endif

	if (archtodir) {
		char *p;

		/* build complete name of archive directory into dirpart */
		if (*archdirname == '/') {	/* absolute */
			strcpy(dirpart, archdirname);
		} else {	/* relative */
			/* get directory part of logfile */
			strcpy(dirpart, log);
			if ((p = rindex(dirpart, '/')) == NULL)
				dirpart[0] = '\0';
			else
				*(p + 1) = '\0';
			strcat(dirpart, archdirname);
		}

		/* check if archive directory exists, if not, create it */
		if (lstat(dirpart, &st))
			createdir(dirpart);

		/* get filename part of logfile */
		if ((p = rindex(log, '/')) == NULL)
			strcpy(namepart, log);
		else
			strcpy(namepart, p + 1);

		/* name of oldest log */
		(void) sprintf(file1, "%s/%s.%d", dirpart, namepart, numdays);
		(void) strcpy(zfile1, file1);
		(void) strcat(zfile1, COMPRESS_POSTFIX);
	} else {
		/* name of oldest log */
		(void) sprintf(file1, "%s.%d", log, numdays);
		(void) strcpy(zfile1, file1);
		(void) strcat(zfile1, COMPRESS_POSTFIX);
	}

	if (noaction) {
		printf("rm -f %s\n", file1);
		printf("rm -f %s\n", zfile1);
	} else {
		(void) unlink(file1);
		(void) unlink(zfile1);
	}

	/* Move down log files */
	_numdays = numdays;	/* preserve */
	while (numdays--) {

		(void) strcpy(file2, file1);

		if (archtodir)
			(void) sprintf(file1, "%s/%s.%d", dirpart, namepart, numdays);
		else
			(void) sprintf(file1, "%s.%d", log, numdays);

		(void) strcpy(zfile1, file1);
		(void) strcpy(zfile2, file2);
		if (lstat(file1, &st)) {
			(void) strcat(zfile1, COMPRESS_POSTFIX);
			(void) strcat(zfile2, COMPRESS_POSTFIX);
			if (lstat(zfile1, &st))
				continue;
		}
		if (noaction) {
			printf("mv %s %s\n", zfile1, zfile2);
			printf("chmod %o %s\n", perm, zfile2);
			printf("chown %d.%d %s\n",
			    owner_uid, group_gid, zfile2);
		} else {
			(void) rename(zfile1, zfile2);
			(void) chmod(zfile2, perm);
			(void) chown(zfile2, owner_uid, group_gid);
		}
	}
	if (!noaction && !(flags & CE_BINARY))
		(void) log_trim(log);	/* Report the trimming to the old log */

	if (!_numdays) {
		if (noaction)
			printf("rm %s\n", log);
		else
			(void) unlink(log);
	} else {
		if (noaction)
			printf("mv %s to %s\n", log, file1);
		else {
			if (archtodir)
				movefile(log, file1, perm, owner_uid, group_gid);
			else
				(void) rename(log, file1);
		}
	}

	if (noaction)
		printf("Start new log...");
	else {
		fd = creat(log, perm);
		if (fd < 0)
			err(1, "can't start new log");
		if (fchown(fd, owner_uid, group_gid))
			err(1, "can't chmod new log file");
		(void) close(fd);
		if (!(flags & CE_BINARY))
			if (log_trim(log))	/* Add status message */
				err(1, "can't add status message to log");
	}
	if (noaction)
		printf("chmod %o %s...\n", perm, log);
	else
		(void) chmod(log, perm);

	pid = 0;
	need_notification = notified = 0;
	if (pid_file != NULL) {
		need_notification = 1;
		pid = get_pid(pid_file);
	}
	if (pid) {
		if (noaction) {
			notified = 1;
			printf("kill -%d %d\n", sig, (int) pid);
		} else if (kill(pid, sig))
			warn("can't notify daemon, pid %d", (int) pid);
		else {
			notified = 1;
			if (verbose)
				printf("daemon pid %d notified\n", (int) pid);
		}
	}
	if ((flags & CE_COMPACT)) {
		if (need_notification && !notified)
			warnx("log %s not compressed because daemon not notified", log);
		else if (noaction)
			printf("Compress %s.0\n", log);
		else {
			if (notified) {
				if (verbose)
					printf("small pause to allow daemon to close log\n");
				sleep(10);
			}
			if (archtodir) {
				(void) sprintf(file1, "%s/%s", dirpart, namepart);
				compress_log(file1);
			} else {
				compress_log(log);
			}
		}
	}
}

/* Log the fact that the logs were turned over */
static int
log_trim(char *log)
{
	FILE *f;

	if ((f = fopen(log, "a")) == NULL)
		return (-1);
	fprintf(f, "%s %s newsyslog[%d]: logfile turned over\n",
	    daytime, hostname, (int) getpid());
	if (fclose(f) == EOF)
		err(1, "log_trim: fclose:");
	return (0);
}

/* Fork of gzip to compress the old log file */
static void
compress_log(char *log)
{
	pid_t pid;
	char tmp[MAXPATHLEN + 1];

	(void) sprintf(tmp, "%s.0", log);
	pid = fork();
	if (pid < 0)
		err(1, "fork");
	else if (!pid) {
		(void) execl(_PATH_GZIP, _PATH_GZIP, "-f", tmp, 0);
		err(1, _PATH_GZIP);
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
			strcpy(tmp, archdirname);
		} else {	/* relative */
			/* get directory part of logfile */
			strcpy(tmp, file);
			if ((p = rindex(tmp, '/')) == NULL)
				tmp[0] = '\0';
			else
				*(p + 1) = '\0';
			strcat(tmp, archdirname);
		}

		strcat(tmp, "/");

		/* get filename part of logfile */
		if ((p = rindex(file, '/')) == NULL)
			strcat(tmp, file);
		else
			strcat(tmp, p + 1);
	} else {
		(void) strcpy(tmp, file);
	}

	if (stat(strcat(tmp, ".0"), &sb) < 0)
		if (stat(strcat(tmp, COMPRESS_POSTFIX), &sb) < 0)
			return (-1);
	return ((int) (timenow - sb.st_mtime + 1800) / 3600);
}

static pid_t
get_pid(char *pid_file)
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
				warnx("preposterous process number: %d", (int) pid);
				pid = 0;
			}
		} else
			warn("can't read %s pid file to restart a daemon",
			    pid_file);
		(void) fclose(f);
	}
	return pid;
}

/* Skip Over Blanks */
char *
sob(char *p)
{
	while (p && *p && isspace(*p))
		p++;
	return (p);
}

/* Skip Over Non-Blanks */
char *
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
parse8601(char *s)
{
	char *t;
	struct tm tm, *tmp;
	u_long ul;

	tmp = localtime(&timenow);
	tm = *tmp;

	tm.tm_hour = tm.tm_min = tm.tm_sec = 0;

	ul = strtoul(s, &t, 10);
	if (*t != '\0' && *t != 'T')
		return -1;

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
		tm.tm_year = tm.tm_year - (tm.tm_year % 100);
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
		return -1;
	}

	/* sanity check */
	if (tm.tm_year < 70 || tm.tm_mon < 0 || tm.tm_mon > 12
	    || tm.tm_mday < 1 || tm.tm_mday > 31)
		return -1;

	if (*t != '\0') {
		s = ++t;
		ul = strtoul(s, &t, 10);
		if (*t != '\0' && !isspace(*t))
			return -1;

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
			return -1;
		}

		/* sanity check */
		if (tm.tm_sec < 0 || tm.tm_sec > 60 || tm.tm_min < 0
		    || tm.tm_min > 59 || tm.tm_hour < 0 || tm.tm_hour > 23)
			return -1;
	}
	return mktime(&tm);
}

/* physically move file */
static void
movefile(char *from, char *to, int perm, int owner_uid, int group_gid)
{
	FILE *src, *dst;
	int c;

	if ((src = fopen(from, "r")) == NULL)
		err(1, "can't fopen %s for reading", from);
	if ((dst = fopen(to, "w")) == NULL)
		err(1, "can't fopen %s for writing", to);
	if (fchown(fileno(dst), owner_uid, group_gid))
		err(1, "can't fchown %s", to);
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
	char *s, *d;
	char mkdirpath[MAXPATHLEN + 1];
	struct stat st;

	s = dirpart;
	d = mkdirpath;

	for (;;) {
		*d++ = *s++;
		if (*s == '/' || *s == '\0') {
			*d = '\0';
			if (lstat(mkdirpath, &st))
				mkdir(mkdirpath, 0755);
		}
		if (*s == '\0')
			break;
	}
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
parseDWM(char *s)
{
	char *t;
	struct tm tm, *tmp;
	u_long ul;
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
				return -1;
			Dseen++;
			s++;
			ul = strtoul(s, &t, 10);
			if (ul < 0 || ul > 23)
				return -1;
			tm.tm_hour = ul;
			break;

		case 'W':
			if (WMseen)
				return -1;
			WMseen++;
			s++;
			ul = strtoul(s, &t, 10);
			if (ul < 0 || ul > 6)
				return -1;
			if (ul != tm.tm_wday) {
				int save;

				if (ul < tm.tm_wday) {
					save = 6 - tm.tm_wday;
					save += (ul + 1);
				} else {
					save = ul - tm.tm_wday;
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
				return -1;
			WMseen++;
			s++;
			if (tolower(*s) == 'l') {
				tm.tm_mday = nd;
				s++;
				t = s;
			} else {
				ul = strtoul(s, &t, 10);
				if (ul < 1 || ul > 31)
					return -1;

				if (ul > nd)
					return -1;
				tm.tm_mday = ul;
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
	return mktime(&tm);
}
