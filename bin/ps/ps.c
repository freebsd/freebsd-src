/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * ------+---------+---------+-------- + --------+---------+---------+---------*
 * Copyright (c) 2004  - Garance Alistair Drosehn <gad@FreeBSD.org>.
 * All rights reserved.
 *
 * Significant modifications made to bring `ps' options somewhat closer
 * to the standard for `ps' as described in SingleUnixSpec-v3.
 * ------+---------+---------+-------- + --------+---------+---------+---------*
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1990, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#if 0
#ifndef lint
static char sccsid[] = "@(#)ps.c	8.4 (Berkeley) 4/2/94";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/mount.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <kvm.h>
#include <limits.h>
#include <locale.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ps.h"

#define	W_SEP	" \t"		/* "Whitespace" list separators */
#define	T_SEP	","		/* "Terminate-element" list separators */

#ifdef LAZY_PS
#define	DEF_UREAD	0
#define	OPT_LAZY_f	"f"
#else
#define	DEF_UREAD	1	/* Always do the more-expensive read. */
#define	OPT_LAZY_f		/* I.e., the `-f' option is not added. */
#endif

/*
 * isdigit takes an `int', but expects values in the range of unsigned char.
 * This wrapper ensures that values from a 'char' end up in the correct range.
 */
#define	isdigitch(Anychar) isdigit((u_char)(Anychar))

int	 cflag;			/* -c */
int	 eval;			/* Exit value */
time_t	 now;			/* Current time(3) value */
int	 rawcpu;		/* -C */
int	 sumrusage;		/* -S */
int	 termwidth;		/* Width of the screen (0 == infinity). */
int	 totwidth;		/* Calculated-width of requested variables. */

struct velisthead varlist = STAILQ_HEAD_INITIALIZER(varlist);

static int	 forceuread = DEF_UREAD; /* Do extra work to get u-area. */
static kvm_t	*kd;
static KINFO	*kinfo;
static int	 needcomm;	/* -o "command" */
static int	 needenv;	/* -e */
static int	 needuser;	/* -o "user" */
static int	 optfatal;	/* Fatal error parsing some list-option. */

static enum sort { DEFAULT, SORTMEM, SORTCPU } sortby = DEFAULT;

struct listinfo;
typedef	int	addelem_rtn(struct listinfo *_inf, const char *_elem);

struct listinfo {
	int		 count;
	int		 maxcount;
	int		 elemsize;
	addelem_rtn	*addelem;
	const char	*lname;
	union {
		gid_t	*gids;
		pid_t	*pids;
		dev_t	*ttys;
		uid_t	*uids;
		void	*ptr;
	} l;
};

static int	 check_procfs(void);
static int	 addelem_gid(struct listinfo *, const char *);
static int	 addelem_pid(struct listinfo *, const char *);
static int	 addelem_tty(struct listinfo *, const char *);
static int	 addelem_uid(struct listinfo *, const char *);
static void	 add_list(struct listinfo *, const char *);
static void	 dynsizevars(KINFO *);
static void	*expand_list(struct listinfo *);
static const char *
		 fmt(char **(*)(kvm_t *, const struct kinfo_proc *, int),
		    KINFO *, char *, int);
static void	 free_list(struct listinfo *);
static void	 init_list(struct listinfo *, addelem_rtn, int, const char *);
static char	*kludge_oldps_options(const char *, char *, const char *);
static int	 pscomp(const void *, const void *);
static void	 saveuser(KINFO *);
static void	 scanvars(void);
static void	 sizevars(void);
static void	 usage(void);

static char dfmt[] = "pid,tt,state,time,command";
static char jfmt[] = "user,pid,ppid,pgid,sid,jobc,state,tt,time,command";
static char lfmt[] = "uid,pid,ppid,cpu,pri,nice,vsz,rss,mwchan,state,"
			"tt,time,command";
static char   o1[] = "pid";
static char   o2[] = "tt,state,time,command";
static char ufmt[] = "user,pid,%cpu,%mem,vsz,rss,tt,state,start,time,command";
static char vfmt[] = "pid,state,time,sl,re,pagein,vsz,rss,lim,tsiz,"
			"%cpu,%mem,command";
static char Zfmt[] = "label";

#define	PS_ARGS	"AaCce" OPT_LAZY_f "G:gHhjLlM:mN:O:o:p:rSTt:U:uvwXxZ"

int
main(int argc, char *argv[])
{
	struct listinfo gidlist, pgrplist, pidlist;
	struct listinfo ruidlist, sesslist, ttylist, uidlist;
	struct kinfo_proc *kp;
	KINFO *next_KINFO;
	struct varent *vent;
	struct winsize ws;
	const char *nlistf, *memf;
	char *cols;
	int all, ch, dropgid, elem, flag, _fmt, i, lineno;
	int nentries, nkept, nselectors;
	int prtheader, showthreads, wflag, what, xkeep, xkeep_implied;
	char errbuf[_POSIX2_LINE_MAX];

	(void) setlocale(LC_ALL, "");
	time(&now);			/* Used by routines in print.c. */

	if ((cols = getenv("COLUMNS")) != NULL && *cols != '\0')
		termwidth = atoi(cols);
	else if ((ioctl(STDOUT_FILENO, TIOCGWINSZ, (char *)&ws) == -1 &&
	     ioctl(STDERR_FILENO, TIOCGWINSZ, (char *)&ws) == -1 &&
	     ioctl(STDIN_FILENO,  TIOCGWINSZ, (char *)&ws) == -1) ||
	     ws.ws_col == 0)
		termwidth = 79;
	else
		termwidth = ws.ws_col - 1;

	/*
	 * Hide a number of option-processing kludges in a separate routine,
	 * to support some historical BSD behaviors, such as `ps axu'.
	 */
	if (argc > 1)
		argv[1] = kludge_oldps_options(PS_ARGS, argv[1], argv[2]);

	all = dropgid = _fmt = nselectors = optfatal = 0;
	prtheader = showthreads = wflag = xkeep_implied = 0;
	xkeep = -1;			/* Neither -x nor -X. */
	init_list(&gidlist, addelem_gid, sizeof(gid_t), "group");
	init_list(&pgrplist, addelem_pid, sizeof(pid_t), "process group");
	init_list(&pidlist, addelem_pid, sizeof(pid_t), "process id");
	init_list(&ruidlist, addelem_uid, sizeof(uid_t), "ruser");
	init_list(&sesslist, addelem_pid, sizeof(pid_t), "session id");
	init_list(&ttylist, addelem_tty, sizeof(dev_t), "tty");
	init_list(&uidlist, addelem_uid, sizeof(uid_t), "user");
	memf = nlistf = _PATH_DEVNULL;
	while ((ch = getopt(argc, argv, PS_ARGS)) != -1)
		switch ((char)ch) {
		case 'A':
			/*
			 * Exactly the same as `-ax'.   This has been
			 * added for compatability with SUSv3, but for
			 * now it will not be described in the man page.
			 */
			nselectors++;
			all = xkeep = 1;
			break;
		case 'a':
			nselectors++;
			all = 1;
			break;
		case 'C':
			rawcpu = 1;
			break;
		case 'c':
			cflag = 1;
			break;
		case 'e':			/* XXX set ufmt */
			needenv = 1;
			break;
#ifdef LAZY_PS
		case 'f':
			if (getuid() == 0 || getgid() == 0)
				forceuread = 1;
			break;
#endif
		case 'G':
			add_list(&gidlist, optarg);
			xkeep_implied = 1;
			nselectors++;
			break;
		case 'g':
#if 0
			/*-
			 * XXX - This SUSv3 behavior is still under debate
			 *	since it conflicts with the (undocumented)
			 *	`-g' option.  So we skip it for now.
			 */
			add_list(&pgrplist, optarg);
			xkeep_implied = 1;
			nselectors++;
			break;
#else
			/* The historical BSD-ish (from SunOS) behavior. */
			break;			/* no-op */
#endif
		case 'H':
			showthreads = KERN_PROC_INC_THREAD;
			break;
		case 'h':
			prtheader = ws.ws_row > 5 ? ws.ws_row : 22;
			break;
		case 'j':
			parsefmt(jfmt, 0);
			_fmt = 1;
			jfmt[0] = '\0';
			break;
		case 'L':
			showkey();
			exit(0);
		case 'l':
			parsefmt(lfmt, 0);
			_fmt = 1;
			lfmt[0] = '\0';
			break;
		case 'M':
			memf = optarg;
			dropgid = 1;
			break;
		case 'm':
			sortby = SORTMEM;
			break;
		case 'N':
			nlistf = optarg;
			dropgid = 1;
			break;
		case 'O':
			parsefmt(o1, 1);
			parsefmt(optarg, 1);
			parsefmt(o2, 1);
			o1[0] = o2[0] = '\0';
			_fmt = 1;
			break;
		case 'o':
			parsefmt(optarg, 1);
			_fmt = 1;
			break;
		case 'p':
			add_list(&pidlist, optarg);
			/*
			 * Note: `-p' does not *set* xkeep, but any values
			 * from pidlist are checked before xkeep is.  That
			 * way they are always matched, even if the user
			 * specifies `-X'.
			 */
			nselectors++;
			break;
#if 0
		case 'R':
			/*-
			 * XXX - This un-standard option is still under
			 *	debate.  This is what SUSv3 defines as
			 *	the `-U' option, and while it would be
			 *	nice to have, it could cause even more
			 *	confusion to implement it as `-R'.
			 */
			add_list(&ruidlist, optarg);
			xkeep_implied = 1;
			nselectors++;
			break;
#endif
		case 'r':
			sortby = SORTCPU;
			break;
		case 'S':
			sumrusage = 1;
			break;
#if 0
		case 's':
			/*-
			 * XXX - This non-standard option is still under
			 *	debate.  This *is* supported on Solaris,
			 *	Linux, and IRIX, but conflicts with `-s'
			 *	on NetBSD and maybe some older BSD's.
			 */
			add_list(&sesslist, optarg);
			xkeep_implied = 1;
			nselectors++;
			break;
#endif
		case 'T':
			if ((optarg = ttyname(STDIN_FILENO)) == NULL)
				errx(1, "stdin: not a terminal");
			/* FALLTHROUGH */
		case 't':
			add_list(&ttylist, optarg);
			xkeep_implied = 1;
			nselectors++;
			break;
		case 'U':
			/* This is what SUSv3 defines as the `-u' option. */
			add_list(&uidlist, optarg);
			xkeep_implied = 1;
			nselectors++;
			break;
		case 'u':
			parsefmt(ufmt, 0);
			sortby = SORTCPU;
			_fmt = 1;
			ufmt[0] = '\0';
			break;
		case 'v':
			parsefmt(vfmt, 0);
			sortby = SORTMEM;
			_fmt = 1;
			vfmt[0] = '\0';
			break;
		case 'w':
			if (wflag)
				termwidth = UNLIMITED;
			else if (termwidth < 131)
				termwidth = 131;
			wflag++;
			break;
		case 'X':
			/*
			 * Note that `-X' and `-x' are not standard "selector"
			 * options. For most selector-options, we check *all*
			 * processes to see if any are matched by the given
			 * value(s).  After we have a set of all the matched
			 * processes, then `-X' and `-x' govern whether we
			 * modify that *matched* set for processes which do
			 * not have a controlling terminal.  `-X' causes
			 * those processes to be deleted from the matched
			 * set, while `-x' causes them to be kept.
			 */
			xkeep = 0;
			break;
		case 'x':
			xkeep = 1;
			break;
		case 'Z':
			parsefmt(Zfmt, 0);
			Zfmt[0] = '\0';
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/*
	 * If the user specified ps -e then they want a copy of the process
	 * environment kvm_getenvv(3) attempts to open /proc/<pid>/mem.
	 * Check to make sure that procfs is mounted on /proc, otherwise
	 * print a warning informing the user that output will be incomplete.
	 */
	if (needenv == 1 && check_procfs() == 0)
		warnx("Process environment requires procfs(5)");
	/*
	 * If there arguments after processing all the options, attempt
	 * to treat them as a list of process ids.
	 */
	while (*argv) {
		if (!isdigitch(**argv))
			break;
		add_list(&pidlist, *argv);
		argv++;
	}
	if (*argv) {
		fprintf(stderr, "%s: illegal argument: %s\n",
		    getprogname(), *argv);
		usage();
	}
	if (optfatal)
		exit(1);		/* Error messages already printed. */
	if (xkeep < 0)			/* Neither -X nor -x was specified. */
		xkeep = xkeep_implied;


	/*
	 * Discard setgid privileges if not the running kernel so that bad
	 * guys can't print interesting stuff from kernel memory.
	 */
	if (dropgid)
		setgid(getgid());

	kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, errbuf);
	if (kd == 0)
		errx(1, "%s", errbuf);

	if (!_fmt)
		parsefmt(dfmt, 0);

	if (nselectors == 0) {
		uidlist.l.ptr = malloc(sizeof(uid_t));
		if (uidlist.l.ptr == NULL)
			errx(1, "malloc failed");
		nselectors = 1;
		uidlist.count = uidlist.maxcount = 1;
		*uidlist.l.uids = getuid();
	}

	/*
	 * scan requested variables, noting what structures are needed,
	 * and adjusting header widths as appropriate.
	 */
	scanvars();

	/*
	 * Get process list.  If the user requested just one selector-
	 * option, then kvm_getprocs can be asked to return just those
	 * processes.  Otherwise, have it return all processes, and
	 * then this routine will search that full list and select the
	 * processes which match any of the user's selector-options.
	 */
	what = showthreads != 0 ? KERN_PROC_ALL : KERN_PROC_PROC;
	flag = 0;
	if (nselectors == 1) {
		if (gidlist.count == 1) {
			what = KERN_PROC_RGID | showthreads;
			flag = *gidlist.l.gids;
			nselectors = 0;
		} else if (pgrplist.count == 1) {
			what = KERN_PROC_PGRP | showthreads;
			flag = *pgrplist.l.pids;
			nselectors = 0;
		} else if (pidlist.count == 1) {
			what = KERN_PROC_PID | showthreads;
			flag = *pidlist.l.pids;
			nselectors = 0;
		} else if (ruidlist.count == 1) {
			what = KERN_PROC_RUID | showthreads;
			flag = *ruidlist.l.uids;
			nselectors = 0;
		} else if (sesslist.count == 1) {
			what = KERN_PROC_SESSION | showthreads;
			flag = *sesslist.l.pids;
			nselectors = 0;
		} else if (ttylist.count == 1) {
			what = KERN_PROC_TTY | showthreads;
			flag = *ttylist.l.ttys;
			nselectors = 0;
		} else if (uidlist.count == 1) {
			what = KERN_PROC_UID | showthreads;
			flag = *uidlist.l.uids;
			nselectors = 0;
		} else if (all) {
			/* No need for this routine to select processes. */
			nselectors = 0;
		}
	}

	/*
	 * select procs
	 */
	nentries = -1;
	kp = kvm_getprocs(kd, what, flag, &nentries);
	if ((kp == NULL && nentries > 0) || (kp != NULL && nentries < 0))
		errx(1, "%s", kvm_geterr(kd));
	nkept = 0;
	if (nentries > 0) {
		if ((kinfo = malloc(nentries * sizeof(*kinfo))) == NULL)
			errx(1, "malloc failed");
		for (i = nentries; --i >= 0; ++kp) {
			/*
			 * If the user specified multiple selection-criteria,
			 * then keep any process matched by the inclusive OR
			 * of all the selection-criteria given.
			 */
			if (pidlist.count > 0) {
				for (elem = 0; elem < pidlist.count; elem++)
					if (kp->ki_pid == pidlist.l.pids[elem])
						goto keepit;
			}
			/*
			 * Note that we had to process pidlist before
			 * filtering out processes which do not have
			 * a controlling terminal.
			 */
			if (xkeep == 0) {
				if ((kp->ki_tdev == NODEV ||
				    (kp->ki_flag & P_CONTROLT) == 0))
					continue;
			}
			if (nselectors == 0)
				goto keepit;
			if (gidlist.count > 0) {
				for (elem = 0; elem < gidlist.count; elem++)
					if (kp->ki_rgid == gidlist.l.gids[elem])
						goto keepit;
			}
			if (pgrplist.count > 0) {
				for (elem = 0; elem < pgrplist.count; elem++)
					if (kp->ki_pgid ==
					    pgrplist.l.pids[elem])
						goto keepit;
			}
			if (ruidlist.count > 0) {
				for (elem = 0; elem < ruidlist.count; elem++)
					if (kp->ki_ruid ==
					    ruidlist.l.uids[elem])
						goto keepit;
			}
			if (sesslist.count > 0) {
				for (elem = 0; elem < sesslist.count; elem++)
					if (kp->ki_sid == sesslist.l.pids[elem])
						goto keepit;
			}
			if (ttylist.count > 0) {
				for (elem = 0; elem < ttylist.count; elem++)
					if (kp->ki_tdev == ttylist.l.ttys[elem])
						goto keepit;
			}
			if (uidlist.count > 0) {
				for (elem = 0; elem < uidlist.count; elem++)
					if (kp->ki_uid == uidlist.l.uids[elem])
						goto keepit;
			}
			/*
			 * This process did not match any of the user's
			 * selector-options, so skip the process.
			 */
			continue;

		keepit:
			next_KINFO = &kinfo[nkept];
			next_KINFO->ki_p = kp;
			next_KINFO->ki_pcpu = getpcpu(next_KINFO);
			if (sortby == SORTMEM)
				next_KINFO->ki_memsize = kp->ki_tsize +
				    kp->ki_dsize + kp->ki_ssize;
			if (needuser)
				saveuser(next_KINFO);
			dynsizevars(next_KINFO);
			nkept++;
		}
	}

	sizevars();

	/*
	 * print header
	 */
	printheader();
	if (nkept == 0)
		exit(1);

	/*
	 * sort proc list
	 */
	qsort(kinfo, nkept, sizeof(KINFO), pscomp);
	/*
	 * For each process, call each variable output function.
	 */
	for (i = lineno = 0; i < nkept; i++) {
		STAILQ_FOREACH(vent, &varlist, next_ve) {
			(vent->var->oproc)(&kinfo[i], vent);
			if (STAILQ_NEXT(vent, next_ve) != NULL)
				(void)putchar(' ');
		}
		(void)putchar('\n');
		if (prtheader && lineno++ == prtheader - 4) {
			(void)putchar('\n');
			printheader();
			lineno = 0;
		}
	}
	free_list(&gidlist);
	free_list(&pidlist);
	free_list(&pgrplist);
	free_list(&ruidlist);
	free_list(&sesslist);
	free_list(&ttylist);
	free_list(&uidlist);

	exit(eval);
}

static int
addelem_gid(struct listinfo *inf, const char *elem)
{
	struct group *grp;
	const char *nameorID;
	char *endp;
	u_long bigtemp;

	if (*elem == '\0' || strlen(elem) >= MAXLOGNAME) {
		if (*elem == '\0')
			warnx("Invalid (zero-length) %s name", inf->lname);
		else
			warnx("%s name too long: %s", inf->lname, elem);
		optfatal = 1;
		return (0);		/* Do not add this value. */
	}

	/*
	 * SUSv3 states that `ps -G grouplist' should match "real-group
	 * ID numbers", and does not mention group-names.  I do want to
	 * also support group-names, so this tries for a group-id first,
	 * and only tries for a name if that doesn't work.  This is the
	 * opposite order of what is done in addelem_uid(), but in
	 * practice the order would only matter for group-names which
	 * are all-numeric.
	 */
	grp = NULL;
	nameorID = "named";
	errno = 0;
	bigtemp = strtoul(elem, &endp, 10);
	if (errno == 0 && *endp == '\0' && bigtemp <= GID_MAX) {
		nameorID = "name or ID matches";
		grp = getgrgid((gid_t)bigtemp);
	}
	if (grp == NULL)
		grp = getgrnam(elem);
	if (grp == NULL) {
		warnx("No %s %s '%s'", inf->lname, nameorID, elem);
		optfatal = 1;
		return (0);
	}
	if (inf->count >= inf->maxcount)
		expand_list(inf);
	inf->l.gids[(inf->count)++] = grp->gr_gid;
	return (1);
}

#define	BSD_PID_MAX	99999		/* Copy of PID_MAX from sys/proc.h. */
static int
addelem_pid(struct listinfo *inf, const char *elem)
{
	char *endp;
	long tempid;

	if (*elem == '\0') {
		warnx("Invalid (zero-length) process id");
		optfatal = 1;
		return (0);		/* Do not add this value. */
	}

	errno = 0;
	tempid = strtol(elem, &endp, 10);
	if (*endp != '\0' || tempid < 0 || elem == endp) {
		warnx("Invalid %s: %s", inf->lname, elem);
		errno = ERANGE;
	} else if (errno != 0 || tempid > BSD_PID_MAX) {
		warnx("%s too large: %s", inf->lname, elem);
		errno = ERANGE;
	}
	if (errno == ERANGE) {
		optfatal = 1;
		return (0);
	}
	if (inf->count >= inf->maxcount)
		expand_list(inf);
	inf->l.pids[(inf->count)++] = tempid;
	return (1);
}
#undef	BSD_PID_MAX

/*-
 * The user can specify a device via one of three formats:
 *     1) fully qualified, e.g.:     /dev/ttyp0 /dev/console
 *     2) missing "/dev", e.g.:      ttyp0      console
 *     3) two-letters, e.g.:         p0         co
 *        (matching letters that would be seen in the "TT" column)
 */
static int
addelem_tty(struct listinfo *inf, const char *elem)
{
	const char *ttypath;
	struct stat sb;
	char pathbuf[PATH_MAX], pathbuf2[PATH_MAX];

	ttypath = NULL;
	pathbuf2[0] = '\0';
	switch (*elem) {
	case '/':
		ttypath = elem;
		break;
	case 'c':
		if (strcmp(elem, "co") == 0) {
			ttypath = _PATH_CONSOLE;
			break;
		}
		/* FALLTHROUGH */
	default:
		strlcpy(pathbuf, _PATH_DEV, sizeof(pathbuf));
		strlcat(pathbuf, elem, sizeof(pathbuf));
		ttypath = pathbuf;
		if (strncmp(pathbuf, _PATH_TTY, strlen(_PATH_TTY)) == 0)
			break;
		if (strcmp(pathbuf, _PATH_CONSOLE) == 0)
			break;
		/* Check to see if /dev/tty${elem} exists */
		strlcpy(pathbuf2, _PATH_TTY, sizeof(pathbuf2));
		strlcat(pathbuf2, elem, sizeof(pathbuf2));
		if (stat(pathbuf2, &sb) == 0 && S_ISCHR(sb.st_mode)) {
			/* No need to repeat stat() && S_ISCHR() checks */
			ttypath = NULL;	
			break;
		}
		break;
	}
	if (ttypath) {
		if (stat(ttypath, &sb) == -1) {
			if (pathbuf2[0] != '\0')
				warn("%s and %s", pathbuf2, ttypath);
			else
				warn("%s", ttypath);
			optfatal = 1;
			return (0);
		}
		if (!S_ISCHR(sb.st_mode)) {
			if (pathbuf2[0] != '\0')
				warnx("%s and %s: Not a terminal", pathbuf2,
				    ttypath);
			else
				warnx("%s: Not a terminal", ttypath);
			optfatal = 1;
			return (0);
		}
	}
	if (inf->count >= inf->maxcount)
		expand_list(inf);
	inf->l.ttys[(inf->count)++] = sb.st_rdev;
	return (1);
}

static int
addelem_uid(struct listinfo *inf, const char *elem)
{
	struct passwd *pwd;
	char *endp;
	u_long bigtemp;

	if (*elem == '\0' || strlen(elem) >= MAXLOGNAME) {
		if (*elem == '\0')
			warnx("Invalid (zero-length) %s name", inf->lname);
		else
			warnx("%s name too long: %s", inf->lname, elem);
		optfatal = 1;
		return (0);		/* Do not add this value. */
	}

	pwd = getpwnam(elem);
	if (pwd == NULL) {
		errno = 0;
		bigtemp = strtoul(elem, &endp, 10);
		if (errno != 0 || *endp != '\0' || bigtemp > UID_MAX)
			warnx("No %s named '%s'", inf->lname, elem);
		else {
			/* The string is all digits, so it might be a userID. */
			pwd = getpwuid((uid_t)bigtemp);
			if (pwd == NULL)
				warnx("No %s name or ID matches '%s'",
				    inf->lname, elem);
		}
	}
	if (pwd == NULL) {
		/*
		 * These used to be treated as minor warnings (and the
		 * option was simply ignored), but now they are fatal
		 * errors (and the command will be aborted).
		 */
		optfatal = 1;
		return (0);
	}
	if (inf->count >= inf->maxcount)
		expand_list(inf);
	inf->l.uids[(inf->count)++] = pwd->pw_uid;
	return (1);
}

static void
add_list(struct listinfo *inf, const char *argp)
{
	const char *savep;
	char *cp, *endp;
	int toolong;
	char elemcopy[PATH_MAX];

	if (*argp == 0)
		inf->addelem(inf, elemcopy);
	while (*argp != '\0') {
		while (*argp != '\0' && strchr(W_SEP, *argp) != NULL)
			argp++;
		savep = argp;
		toolong = 0;
		cp = elemcopy;
		if (strchr(T_SEP, *argp) == NULL) {
			endp = elemcopy + sizeof(elemcopy) - 1;
			while (*argp != '\0' && cp <= endp &&
			    strchr(W_SEP T_SEP, *argp) == NULL)
				*cp++ = *argp++;
			if (cp > endp)
				toolong = 1;
		}
		if (!toolong) {
			*cp = '\0';
			/*
			 * Add this single element to the given list.
			 */
			inf->addelem(inf, elemcopy);
		} else {
			/*
			 * The string is too long to copy.  Find the end
			 * of the string to print out the warning message.
			 */
			while (*argp != '\0' && strchr(W_SEP T_SEP,
			    *argp) == NULL)
				argp++;
			warnx("Value too long: %.*s", (int)(argp - savep),
			    savep);
			optfatal = 1;
		}
		/*
		 * Skip over any number of trailing whitespace characters,
		 * but only one (at most) trailing element-terminating
		 * character.
		 */
		while (*argp != '\0' && strchr(W_SEP, *argp) != NULL)
			argp++;
		if (*argp != '\0' && strchr(T_SEP, *argp) != NULL) {
			argp++;
			/* Catch case where string ended with a comma. */
			if (*argp == '\0')
				inf->addelem(inf, argp);
		}
	}
}

static void *
expand_list(struct listinfo *inf)
{
	void *newlist;
	int newmax;

	newmax = (inf->maxcount + 1) << 1;
	newlist = realloc(inf->l.ptr, newmax * inf->elemsize);
	if (newlist == NULL) {
		free(inf->l.ptr);
		errx(1, "realloc to %d %ss failed", newmax, inf->lname);
	}
	inf->maxcount = newmax;
	inf->l.ptr = newlist;

	return (newlist);
}

static void
free_list(struct listinfo *inf)
{

	inf->count = inf->elemsize = inf->maxcount = 0;
	if (inf->l.ptr != NULL)
		free(inf->l.ptr);
	inf->addelem = NULL;
	inf->lname = NULL;
	inf->l.ptr = NULL;
}

static void
init_list(struct listinfo *inf, addelem_rtn artn, int elemsize,
    const char *lname)
{

	inf->count = inf->maxcount = 0;
	inf->elemsize = elemsize;
	inf->addelem = artn;
	inf->lname = lname;
	inf->l.ptr = NULL;
}

VARENT *
find_varentry(VAR *v)
{
	struct varent *vent;

	STAILQ_FOREACH(vent, &varlist, next_ve) {
		if (strcmp(vent->var->name, v->name) == 0)
			return vent;
	}
	return NULL;
}

static void
scanvars(void)
{
	struct varent *vent;
	VAR *v;

	STAILQ_FOREACH(vent, &varlist, next_ve) {
		v = vent->var;
		if (v->flag & DSIZ) {
			v->dwidth = v->width;
			v->width = 0;
		}
		if (v->flag & USER)
			needuser = 1;
		if (v->flag & COMM)
			needcomm = 1;
	}
}

static void
dynsizevars(KINFO *ki)
{
	struct varent *vent;
	VAR *v;
	int i;

	STAILQ_FOREACH(vent, &varlist, next_ve) {
		v = vent->var;
		if (!(v->flag & DSIZ))
			continue;
		i = (v->sproc)( ki);
		if (v->width < i)
			v->width = i;
		if (v->width > v->dwidth)
			v->width = v->dwidth;
	}
}

static void
sizevars(void)
{
	struct varent *vent;
	VAR *v;
	int i;

	STAILQ_FOREACH(vent, &varlist, next_ve) {
		v = vent->var;
		i = strlen(vent->header);
		if (v->width < i)
			v->width = i;
		totwidth += v->width + 1;	/* +1 for space */
	}
	totwidth--;
}

static const char *
fmt(char **(*fn)(kvm_t *, const struct kinfo_proc *, int), KINFO *ki,
    char *comm, int maxlen)
{
	const char *s;

	s = fmt_argv((*fn)(kd, ki->ki_p, termwidth), comm, maxlen);
	return (s);
}

#define UREADOK(ki)	(forceuread || (ki->ki_p->ki_sflag & PS_INMEM))

static void
saveuser(KINFO *ki)
{

	if (ki->ki_p->ki_sflag & PS_INMEM) {
		/*
		 * The u-area might be swapped out, and we can't get
		 * at it because we have a crashdump and no swap.
		 * If it's here fill in these fields, otherwise, just
		 * leave them 0.
		 */
		ki->ki_valid = 1;
	} else
		ki->ki_valid = 0;
	/*
	 * save arguments if needed
	 */
	if (needcomm) {
		if (ki->ki_p->ki_stat == SZOMB)
			ki->ki_args = strdup("<defunct>");
		else if (UREADOK(ki) || (ki->ki_p->ki_args != NULL))
			ki->ki_args = strdup(fmt(kvm_getargv, ki,
			    ki->ki_p->ki_comm, MAXCOMLEN));
		else
			asprintf(&ki->ki_args, "(%s)", ki->ki_p->ki_comm);
		if (ki->ki_args == NULL)
			errx(1, "malloc failed");
	} else {
		ki->ki_args = NULL;
	}
	if (needenv) {
		if (UREADOK(ki))
			ki->ki_env = strdup(fmt(kvm_getenvv, ki,
			    (char *)NULL, 0));
		else
			ki->ki_env = strdup("()");
		if (ki->ki_env == NULL)
			errx(1, "malloc failed");
	} else {
		ki->ki_env = NULL;
	}
}

/* A macro used to improve the readability of pscomp(). */
#define	DIFF_RETURN(a, b, field) do {	\
	if ((a)->field != (b)->field)	\
		return (((a)->field < (b)->field) ? -1 : 1); 	\
} while (0)

static int
pscomp(const void *a, const void *b)
{
	const KINFO *ka, *kb;

	ka = a;
	kb = b;
	/* SORTCPU and SORTMEM are sorted in descending order. */
	if (sortby == SORTCPU)
		DIFF_RETURN(kb, ka, ki_pcpu);
	if (sortby == SORTMEM)
		DIFF_RETURN(kb, ka, ki_memsize);
	/*
	 * TTY's are sorted in ascending order, except that all NODEV
	 * processes come before all processes with a device.
	 */
	if (ka->ki_p->ki_tdev != kb->ki_p->ki_tdev) {
		if (ka->ki_p->ki_tdev == NODEV)
			return (-1);
		if (kb->ki_p->ki_tdev == NODEV)
			return (1);
		DIFF_RETURN(ka, kb, ki_p->ki_tdev);
	}

	/* PID's and TID's (threads) are sorted in ascending order. */
	DIFF_RETURN(ka, kb, ki_p->ki_pid);
	DIFF_RETURN(ka, kb, ki_p->ki_tid);
	return (0);
}
#undef DIFF_RETURN

/*
 * ICK (all for getopt), would rather hide the ugliness
 * here than taint the main code.
 *
 *  ps foo -> ps -foo
 *  ps 34 -> ps -p34
 *
 * The old convention that 't' with no trailing tty arg means the users
 * tty, is only supported if argv[1] doesn't begin with a '-'.  This same
 * feature is available with the option 'T', which takes no argument.
 */
static char *
kludge_oldps_options(const char *optlist, char *origval, const char *nextarg)
{
	size_t len;
	char *argp, *cp, *newopts, *ns, *optp, *pidp;

	/*
	 * See if the original value includes any option which takes an
	 * argument (and will thus use up the remainder of the string).
	 */
	argp = NULL;
	if (optlist != NULL) {
		for (cp = origval; *cp != '\0'; cp++) {
			optp = strchr(optlist, *cp);
			if ((optp != NULL) && *(optp + 1) == ':') {
				argp = cp;
				break;
			}
		}
	}
	if (argp != NULL && *origval == '-')
		return (origval);

	/*
	 * if last letter is a 't' flag with no argument (in the context
	 * of the oldps options -- option string NOT starting with a '-' --
	 * then convert to 'T' (meaning *this* terminal, i.e. ttyname(0)).
	 *
	 * However, if a flag accepting a string argument is found earlier
	 * in the option string (including a possible `t' flag), then the
	 * remainder of the string must be the argument to that flag; so
	 * do not modify that argument.  Note that a trailing `t' would
	 * cause argp to be set, if argp was not already set by some
	 * earlier option.
	 */
	len = strlen(origval);
	cp = origval + len - 1;
	pidp = NULL;
	if (*cp == 't' && *origval != '-' && cp == argp) {
		if (nextarg == NULL || *nextarg == '-' || isdigitch(*nextarg))
			*cp = 'T';
	} else if (argp == NULL) {
		/*
		 * The original value did not include any option which takes
		 * an argument (and that would include `p' and `t'), so check
		 * the value for trailing number, or comma-separated list of
		 * numbers, which we will treat as a pid request.
		 */
		if (isdigitch(*cp)) {
			while (cp >= origval && (*cp == ',' || isdigitch(*cp)))
				--cp;
			pidp = cp + 1;
		}
	}

	/*
	 * If nothing needs to be added to the string, then return
	 * the "original" (although possibly modified) value.
	 */
	if (*origval == '-' && pidp == NULL)
		return (origval);

	/*
	 * Create a copy of the string to add '-' and/or 'p' to the
	 * original value.
	 */
	if ((newopts = ns = malloc(len + 3)) == NULL)
		errx(1, "malloc failed");

	if (*origval != '-')
		*ns++ = '-';	/* add option flag */

	if (pidp == NULL)
		strcpy(ns, origval);
	else {
		/*
		 * Copy everything before the pid string, add the `p',
		 * and then copy the pid string.
		 */
		len = pidp - origval;
		memcpy(ns, origval, len);
		ns += len;
		*ns++ = 'p';
		strcpy(ns, pidp);
	}

	return (newopts);
}

static int
check_procfs(void)
{
	struct statfs mnt;

	if (statfs("/proc", &mnt) < 0)
		return (0);
	if (strcmp(mnt.f_fstypename, "procfs") != 0)
		return (0);
	return (1);
}

static void
usage(void)
{
#define	SINGLE_OPTS	"[-aCce" OPT_LAZY_f "HhjlmrSTuvwXxZ]"

	(void)fprintf(stderr, "%s\n%s\n%s\n%s\n",
	    "usage: ps " SINGLE_OPTS " [-O fmt | -o fmt] [-G gid[,gid...]]",
	    "          [-M core] [-N system]",
	    "          [-p pid[,pid...]] [-t tty[,tty...]] [-U user[,user...]]",
	    "       ps [-L]");
	exit(1);
}
