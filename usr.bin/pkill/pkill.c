/*	$NetBSD: pkill.c,v 1.8 2005/03/02 15:31:44 abs Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/user.h>

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <paths.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <regex.h>
#include <ctype.h>
#include <fcntl.h>
#include <kvm.h>
#include <err.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <locale.h>

#define	STATUS_MATCH	0
#define	STATUS_NOMATCH	1
#define	STATUS_BADUSAGE	2
#define	STATUS_ERROR	3

#define	MIN_PID	5
#define	MAX_PID	99999

/* Check for system-processes which should always be ignored. */
#define	IS_KERNPROC(kp)	((kp)->ki_flag & P_KTHREAD)

enum listtype {
	LT_GENERIC,
	LT_USER,
	LT_GROUP,
	LT_TTY,
	LT_PGRP,
	LT_SID
};

struct list {
	SLIST_ENTRY(list) li_chain;
	long	li_number;
};

SLIST_HEAD(listhead, list);

struct kinfo_proc	*plist;
char	*selected;
const char	*delim = "\n";
int	nproc;
int	pgrep;
int	signum = SIGTERM;
int	newest;
int	inverse;
int	longfmt;
int	matchargs;
int	fullmatch;
kvm_t	*kd;
pid_t	mypid;

struct listhead euidlist = SLIST_HEAD_INITIALIZER(list);
struct listhead ruidlist = SLIST_HEAD_INITIALIZER(list);
struct listhead rgidlist = SLIST_HEAD_INITIALIZER(list);
struct listhead pgrplist = SLIST_HEAD_INITIALIZER(list);
struct listhead ppidlist = SLIST_HEAD_INITIALIZER(list);
struct listhead tdevlist = SLIST_HEAD_INITIALIZER(list);
struct listhead sidlist = SLIST_HEAD_INITIALIZER(list);
struct listhead jidlist = SLIST_HEAD_INITIALIZER(list);

int	main(int, char **);
void	usage(void);
void	killact(struct kinfo_proc *);
void	grepact(struct kinfo_proc *);
void	makelist(struct listhead *, enum listtype, char *);
int	takepid(const char *);

int
main(int argc, char **argv)
{
	extern char *optarg;
	extern int optind;
	char buf[_POSIX2_LINE_MAX], *mstr, **pargv, *p, *q;
	const char *execf, *coref;
	int debug_opt;
	int i, ch, bestidx, rv, criteria, pidfromfile;
	size_t jsz;
	void (*action)(struct kinfo_proc *);
	struct kinfo_proc *kp;
	struct list *li;
	struct timeval best_tval;
	regex_t reg;
	regmatch_t regmatch;

	setlocale(LC_ALL, "");

	if (strcmp(getprogname(), "pgrep") == 0) {
		action = grepact;
		pgrep = 1;
	} else {
		action = killact;
		p = argv[1];

		if (argc > 1 && p[0] == '-') {
			p++;
			i = (int)strtol(p, &q, 10);
			if (*q == '\0') {
				signum = i;
				argv++;
				argc--;
			} else {
				if (strncasecmp(p, "sig", 3) == 0)
					p += 3;
				for (i = 1; i < NSIG; i++)
					if (strcasecmp(sys_signame[i], p) == 0)
						break;
				if (i != NSIG) {
					signum = i;
					argv++;
					argc--;
				}
			}
		}
	}

	criteria = 0;
	debug_opt = 0;
	pidfromfile = -1;
	execf = coref = _PATH_DEVNULL;

	while ((ch = getopt(argc, argv, "DF:G:M:N:P:U:d:fg:j:lns:t:u:vx")) != -1)
		switch (ch) {
		case 'D':
			debug_opt++;
			break;
		case 'F':
			pidfromfile = takepid(optarg);
			criteria = 1;
			break;
		case 'G':
			makelist(&rgidlist, LT_GROUP, optarg);
			criteria = 1;
			break;
		case 'M':
			coref = optarg;
			break;
		case 'N':
			execf = optarg;
			break;
		case 'P':
			makelist(&ppidlist, LT_GENERIC, optarg);
			criteria = 1;
			break;
		case 'U':
			makelist(&ruidlist, LT_USER, optarg);
			criteria = 1;
			break;
		case 'd':
			if (!pgrep)
				usage();
			delim = optarg;
			break;
		case 'f':
			matchargs = 1;
			break;
		case 'g':
			makelist(&pgrplist, LT_PGRP, optarg);
			criteria = 1;
			break;
		case 'j':
			makelist(&jidlist, LT_GENERIC, optarg);
			criteria = 1;
			break;
		case 'l':
			if (!pgrep)
				usage();
			longfmt = 1;
			break;
		case 'n':
			newest = 1;
			criteria = 1;
			break;
		case 's':
			makelist(&sidlist, LT_SID, optarg);
			criteria = 1;
			break;
		case 't':
			makelist(&tdevlist, LT_TTY, optarg);
			criteria = 1;
			break;
		case 'u':
			makelist(&euidlist, LT_USER, optarg);
			criteria = 1;
			break;
		case 'v':
			inverse = 1;
			break;
		case 'x':
			fullmatch = 1;
			break;
		default:
			usage();
			/* NOTREACHED */
		}

	argc -= optind;
	argv += optind;
	if (argc != 0)
		criteria = 1;
	if (!criteria)
		usage();

	mypid = getpid();

	/*
	 * Retrieve the list of running processes from the kernel.
	 */
	kd = kvm_openfiles(execf, coref, NULL, O_RDONLY, buf);
	if (kd == NULL)
		errx(STATUS_ERROR, "kvm_openfiles(): %s", buf);

	/*
	 * Use KERN_PROC_PROC instead of KERN_PROC_ALL, since we
	 * just want processes and not individual kernel threads.
	 */
	plist = kvm_getprocs(kd, KERN_PROC_PROC, 0, &nproc);
	if (plist == NULL)
		errx(STATUS_ERROR, "kvm_getprocs() failed");

	/*
	 * Allocate memory which will be used to keep track of the
	 * selection.
	 */
	if ((selected = malloc(nproc)) == NULL)
		errx(STATUS_ERROR, "memory allocation failure");
	memset(selected, 0, nproc);

	/*
	 * Refine the selection.
	 */
	for (; *argv != NULL; argv++) {
		if ((rv = regcomp(&reg, *argv, REG_EXTENDED)) != 0) {
			regerror(rv, &reg, buf, sizeof(buf));
			errx(STATUS_BADUSAGE, "bad expression: %s", buf);
		}

		for (i = 0, kp = plist; i < nproc; i++, kp++) {
			if (IS_KERNPROC(kp) != 0) {
				if (debug_opt > 0)
				    fprintf(stderr, "* Skipped %5d %3d %s\n",
					kp->ki_pid, kp->ki_uid, kp->ki_comm);
				continue;
			}

			if (matchargs) {
				if ((pargv = kvm_getargv(kd, kp, 0)) == NULL)
					continue;

				jsz = 0;
				while (jsz < sizeof(buf) && *pargv != NULL) {
					jsz += snprintf(buf + jsz,
					    sizeof(buf) - jsz,
					    pargv[1] != NULL ? "%s " : "%s",
					    pargv[0]);
					pargv++;
				}

				mstr = buf;
			} else
				mstr = kp->ki_comm;

			rv = regexec(&reg, mstr, 1, &regmatch, 0);
			if (rv == 0) {
				if (fullmatch) {
					if (regmatch.rm_so == 0 &&
					    regmatch.rm_eo ==
					    (off_t)strlen(mstr))
						selected[i] = 1;
				} else
					selected[i] = 1;
			} else if (rv != REG_NOMATCH) {
				regerror(rv, &reg, buf, sizeof(buf));
				errx(STATUS_ERROR, "regexec(): %s", buf);
			}
			if (debug_opt > 1) {
				const char *rv_res = "NoMatch";
				if (selected[i])
					rv_res = "Matched";
				fprintf(stderr, "* %s %5d %3d %s\n", rv_res,
				    kp->ki_pid, kp->ki_uid, mstr);
			}
		}

		regfree(&reg);
	}

	for (i = 0, kp = plist; i < nproc; i++, kp++) {
		if (IS_KERNPROC(kp) != 0)
			continue;

		if (pidfromfile >= 0 && kp->ki_pid != pidfromfile) {
			selected[i] = 0;
			continue;
		}

		SLIST_FOREACH(li, &ruidlist, li_chain)
			if (kp->ki_ruid == (uid_t)li->li_number)
				break;
		if (SLIST_FIRST(&ruidlist) != NULL && li == NULL) {
			selected[i] = 0;
			continue;
		}

		SLIST_FOREACH(li, &rgidlist, li_chain)
			if (kp->ki_rgid == (gid_t)li->li_number)
				break;
		if (SLIST_FIRST(&rgidlist) != NULL && li == NULL) {
			selected[i] = 0;
			continue;
		}

		SLIST_FOREACH(li, &euidlist, li_chain)
			if (kp->ki_uid == (uid_t)li->li_number)
				break;
		if (SLIST_FIRST(&euidlist) != NULL && li == NULL) {
			selected[i] = 0;
			continue;
		}

		SLIST_FOREACH(li, &ppidlist, li_chain)
			if (kp->ki_ppid == (pid_t)li->li_number)
				break;
		if (SLIST_FIRST(&ppidlist) != NULL && li == NULL) {
			selected[i] = 0;
			continue;
		}

		SLIST_FOREACH(li, &pgrplist, li_chain)
			if (kp->ki_pgid == (pid_t)li->li_number)
				break;
		if (SLIST_FIRST(&pgrplist) != NULL && li == NULL) {
			selected[i] = 0;
			continue;
		}

		SLIST_FOREACH(li, &tdevlist, li_chain) {
			if (li->li_number == -1 &&
			    (kp->ki_flag & P_CONTROLT) == 0)
				break;
			if (kp->ki_tdev == (dev_t)li->li_number)
				break;
		}
		if (SLIST_FIRST(&tdevlist) != NULL && li == NULL) {
			selected[i] = 0;
			continue;
		}

		SLIST_FOREACH(li, &sidlist, li_chain)
			if (kp->ki_sid == (pid_t)li->li_number)
				break;
		if (SLIST_FIRST(&sidlist) != NULL && li == NULL) {
			selected[i] = 0;
			continue;
		}

		SLIST_FOREACH(li, &jidlist, li_chain) {
			if (kp->ki_jid > 0) {
				if (li->li_number == 0)
					break;
				if (kp->ki_jid == (int)li->li_number)
					break;
			}
		}
		if (SLIST_FIRST(&jidlist) != NULL && li == NULL) {
			selected[i] = 0;
			continue;
		}

		if (argc == 0)
			selected[i] = 1;
	}

	if (newest) {
		best_tval.tv_sec = 0;
		best_tval.tv_usec = 0;
		bestidx = -1;

		for (i = 0, kp = plist; i < nproc; i++, kp++) {
			if (!selected[i])
				continue;

			if (kp->ki_start.tv_sec > best_tval.tv_sec ||
			    (kp->ki_start.tv_sec == best_tval.tv_sec
			    && kp->ki_start.tv_usec > best_tval.tv_usec)) {
				best_tval.tv_sec = kp->ki_start.tv_sec;
				best_tval.tv_usec = kp->ki_start.tv_usec;
				bestidx = i;
			}
		}

		memset(selected, 0, nproc);
		if (bestidx != -1)
			selected[bestidx] = 1;
	}

	/*
	 * Take the appropriate action for each matched process, if any.
	 */
	for (i = 0, rv = 0, kp = plist; i < nproc; i++, kp++) {
		if (kp->ki_pid == mypid)
			continue;
		if (selected[i]) {
			if (inverse)
				continue;
		} else if (!inverse)
			continue;

		if (IS_KERNPROC(kp) != 0)
			continue;

		rv = 1;
		(*action)(kp);
	}

	exit(rv ? STATUS_MATCH : STATUS_NOMATCH);
}

void
usage(void)
{
	const char *ustr;

	if (pgrep)
		ustr = "[-flnvx] [-d delim]";
	else
		ustr = "[-signal] [-fnvx]";

	fprintf(stderr,
		"usage: %s %s [-F pidfile] [-G gid] [-M core] [-N system]\n"
		"             [-P ppid] [-U uid] [-g pgrp] [-j jid] [-s sid]\n"
		"             [-t tty] [-u euid] pattern ...\n", getprogname(),
		ustr);

	exit(STATUS_ERROR);
}

void
killact(struct kinfo_proc *kp)
{

	if (kill(kp->ki_pid, signum) == -1)
		err(STATUS_ERROR, "signalling pid %d", (int)kp->ki_pid);
}

void
grepact(struct kinfo_proc *kp)
{
	char **argv;

	if (longfmt && matchargs) {
		if ((argv = kvm_getargv(kd, kp, 0)) == NULL)
			return;

		printf("%d ", (int)kp->ki_pid);
		for (; *argv != NULL; argv++) {
			printf("%s", *argv);
			if (argv[1] != NULL)
				putchar(' ');
		}
	} else if (longfmt)
		printf("%d %s", (int)kp->ki_pid, kp->ki_comm);
	else
		printf("%d", (int)kp->ki_pid);

	printf("%s", delim);
}

void
makelist(struct listhead *head, enum listtype type, char *src)
{
	struct list *li;
	struct passwd *pw;
	struct group *gr;
	struct stat st;
	const char *cp;
	char *sp, *p, buf[MAXPATHLEN];
	int empty;

	empty = 1;

	while ((sp = strsep(&src, ",")) != NULL) {
		if (*sp == '\0')
			usage();

		if ((li = malloc(sizeof(*li))) == NULL)
			errx(STATUS_ERROR, "memory allocation failure");
		SLIST_INSERT_HEAD(head, li, li_chain);
		empty = 0;

		li->li_number = (uid_t)strtol(sp, &p, 0);
		if (*p == '\0') {
			switch (type) {
			case LT_PGRP:
				if (li->li_number == 0)
					li->li_number = getpgrp();
				break;
			case LT_SID:
				if (li->li_number == 0)
					li->li_number = getsid(mypid);
				break;
			case LT_TTY:
				usage();
			default:
				break;
			}
			continue;
		}

		switch (type) {
		case LT_USER:
			if ((pw = getpwnam(sp)) == NULL)
				errx(STATUS_BADUSAGE, "unknown user `%s'",
				    sp);
			li->li_number = pw->pw_uid;
			break;
		case LT_GROUP:
			if ((gr = getgrnam(sp)) == NULL)
				errx(STATUS_BADUSAGE, "unknown group `%s'",
				    sp);
			li->li_number = gr->gr_gid;
			break;
		case LT_TTY:
			if (strcmp(sp, "-") == 0) {
				li->li_number = -1;
				break;
			} else if (strcmp(sp, "co") == 0)
				cp = "console";
			else if (strncmp(sp, "tty", 3) == 0)
				cp = sp;
			else
				cp = NULL;

			if (cp == NULL)
				snprintf(buf, sizeof(buf), "/dev/tty%s", sp);
			else
				snprintf(buf, sizeof(buf), "/dev/%s", cp);

			if (stat(buf, &st) < 0) {
				if (errno == ENOENT)
					errx(STATUS_BADUSAGE,
					    "no such tty: `%s'", sp);
				err(STATUS_ERROR, "stat(%s)", sp);
			}

			if ((st.st_mode & S_IFCHR) == 0)
				errx(STATUS_BADUSAGE, "not a tty: `%s'", sp);

			li->li_number = st.st_rdev;
			break;
		default:
			usage();
		};
	}

	if (empty)
		usage();
}

int
takepid(const char *pidfile)
{
	char *endp, line[BUFSIZ];
	FILE *fh;
	long rval;
 
	fh = fopen(pidfile, "r");
	if (fh == NULL)
		err(STATUS_ERROR, "can't open pid file `%s'", pidfile);
 
	if (fgets(line, sizeof(line), fh) == NULL) {
		if (feof(fh)) {
			(void)fclose(fh);
			errx(STATUS_ERROR, "pid file `%s' is empty", pidfile);
		}
		(void)fclose(fh);
		err(STATUS_ERROR, "can't read from pid file `%s'", pidfile);
	}
	(void)fclose(fh);
 
	rval = strtol(line, &endp, 10);
	if (*endp != '\0' && !isspace((unsigned char)*endp))
		errx(STATUS_ERROR, "invalid pid in file `%s'", pidfile);
	else if (rval < MIN_PID || rval > MAX_PID)
		errx(STATUS_ERROR, "invalid pid in file `%s'", pidfile);
	return (rval);
}
