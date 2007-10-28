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
 */

#if 0
#ifndef lint
static char sccsid[] = "@(#)keyword.c	8.5 (Berkeley) 4/2/94";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ps.h"

static VAR *findvar(char *, int, char **header);
static int  vcmp(const void *, const void *);

/* Compute offset in common structures. */
#define	KOFF(x)	offsetof(struct kinfo_proc, x)
#define	ROFF(x)	offsetof(struct rusage, x)

#define	EMULLEN	13		/* enough for "FreeBSD ELF32" */
#define	LWPFMT	"d"
#define	LWPLEN	6
#define	NLWPFMT	"d"
#define	NLWPLEN	4
#define	UIDFMT	"u"
#define	UIDLEN	5
#define	PIDFMT	"d"
#define	PIDLEN	5
#define USERLEN (MAXLOGNAME - 1)

/* PLEASE KEEP THE TABLE BELOW SORTED ALPHABETICALLY!!! */
static VAR var[] = {
	{"%cpu", "%CPU", NULL, 0, pcpu, NULL, 4, 0, CHAR, NULL, 0},
	{"%mem", "%MEM", NULL, 0, pmem, NULL, 4, 0, CHAR, NULL, 0},
	{"acflag", "ACFLG", NULL, 0, kvar, NULL, 3, KOFF(ki_acflag), USHORT,
		"x", 0},
	{"acflg", "", "acflag", 0, NULL, NULL, 0, 0, CHAR, NULL, 0},
	{"args", "COMMAND", NULL, COMM|LJUST|USER, arguments, NULL, 16, 0,
		CHAR, NULL, 0},
	{"blocked", "", "sigmask", 0, NULL, NULL, 0, 0, CHAR, NULL, 0},
	{"caught", "", "sigcatch", 0, NULL, NULL, 0, 0, CHAR, NULL, 0},
	{"comm", "COMMAND", NULL, LJUST, ucomm, NULL, MAXCOMLEN, 0, CHAR,
		NULL, 0},
	{"command", "COMMAND", NULL, COMM|LJUST|USER, command, NULL, 16, 0,
		CHAR, NULL, 0},
	{"cpu", "CPU", NULL, 0, kvar, NULL, 3, KOFF(ki_estcpu), UINT, "d",
		0},
	{"cputime", "", "time", 0, NULL, NULL, 0, 0, CHAR, NULL, 0},
	{"emul", "EMUL", NULL, LJUST, emulname, NULL, EMULLEN, 0, CHAR,
		NULL, 0},
	{"etime", "ELAPSED", NULL, USER, elapsed, NULL, 12, 0, CHAR, NULL, 0},
	{"f", "F", NULL, 0, kvar, NULL, 7, KOFF(ki_flag), INT, "x", 0},
	{"flags", "", "f", 0, NULL, NULL, 0, 0, CHAR, NULL, 0},
	{"ignored", "", "sigignore", 0, NULL, NULL, 0, 0, CHAR, NULL, 0},
	{"inblk", "INBLK", NULL, USER, rvar, NULL, 4, ROFF(ru_inblock), LONG,
		"ld", 0},
	{"inblock", "", "inblk", 0, NULL, NULL, 0, 0, CHAR, NULL, 0},
	{"jid", "JID", NULL, 0, kvar, NULL, 6, KOFF(ki_jid), INT, "d", 0},
	{"jobc", "JOBC", NULL, 0, kvar, NULL, 4, KOFF(ki_jobc), SHORT, "d",
		0},
	{"ktrace", "KTRACE", NULL, 0, kvar, NULL, 8, KOFF(ki_traceflag), INT,
		"x", 0},
	{"label", "LABEL", NULL, LJUST|DSIZ, label, s_label, SHRT_MAX, 0, CHAR,
	    NULL, 0},
	{"lim", "LIM", NULL, 0, maxrss, NULL, 5, 0, CHAR, NULL, 0},
	{"lockname", "LOCK", NULL, LJUST, lockname, NULL, 6, 0, CHAR, NULL,
		0},
	{"login", "LOGIN", NULL, LJUST, logname, NULL, MAXLOGNAME-1, 0, CHAR,
		NULL, 0},
	{"logname", "", "login", 0, NULL, NULL, 0, 0, CHAR, NULL, 0},
	{"lstart", "STARTED", NULL, LJUST|USER, lstarted, NULL, 28, 0, CHAR,
		NULL, 0},
	{"lwp", "LWP", NULL, 0, kvar, NULL, LWPLEN, KOFF(ki_tid), UINT,
		LWPFMT, 0},
	{"majflt", "MAJFLT", NULL, USER, rvar, NULL, 4, ROFF(ru_majflt),
		LONG, "ld", 0},
	{"minflt", "MINFLT", NULL, USER, rvar, NULL, 4, ROFF(ru_minflt),
		LONG, "ld", 0},
	{"msgrcv", "MSGRCV", NULL, USER, rvar, NULL, 4, ROFF(ru_msgrcv),
		LONG, "ld", 0},
	{"msgsnd", "MSGSND", NULL, USER, rvar, NULL, 4, ROFF(ru_msgsnd),
		LONG, "ld", 0},
	{"mwchan", "MWCHAN", NULL, LJUST, mwchan, NULL, 6, 0, CHAR, NULL, 0},
	{"ni", "", "nice", 0, NULL, NULL, 0, 0, CHAR, NULL, 0},
	{"nice", "NI", NULL, 0, kvar, NULL, 2, KOFF(ki_nice), CHAR, "d",
		0},
	{"nivcsw", "NIVCSW", NULL, USER, rvar, NULL, 5, ROFF(ru_nivcsw),
		LONG, "ld", 0},
	{"nlwp", "NLWP", NULL, 0, kvar, NULL, NLWPLEN, KOFF(ki_numthreads),
		UINT, NLWPFMT, 0},
	{"nsignals", "", "nsigs", 0, NULL, NULL, 0, 0, CHAR, NULL, 0},
	{"nsigs", "NSIGS", NULL, USER, rvar, NULL, 4, ROFF(ru_nsignals),
		LONG, "ld", 0},
	{"nswap", "NSWAP", NULL, USER, rvar, NULL, 4, ROFF(ru_nswap),
		LONG, "ld", 0},
	{"nvcsw", "NVCSW", NULL, USER, rvar, NULL, 5, ROFF(ru_nvcsw),
		LONG, "ld", 0},
	{"nwchan", "NWCHAN", NULL, LJUST, nwchan, NULL, 8, 0, CHAR, NULL, 0},
	{"oublk", "OUBLK", NULL, USER, rvar, NULL, 4, ROFF(ru_oublock),
		LONG, "ld", 0},
	{"oublock", "", "oublk", 0, NULL, NULL, 0, 0, CHAR, NULL, 0},
	{"paddr", "PADDR", NULL, 0, kvar, NULL, 8, KOFF(ki_paddr), KPTR,
		"lx", 0},
	{"pagein", "PAGEIN", NULL, USER, pagein, NULL, 6, 0, CHAR, NULL, 0},
	{"pcpu", "", "%cpu", 0, NULL, NULL, 0, 0, CHAR, NULL, 0},
	{"pending", "", "sig", 0, NULL, NULL, 0, 0, CHAR, NULL, 0},
	{"pgid", "PGID", NULL, 0, kvar, NULL, PIDLEN, KOFF(ki_pgid), UINT,
		PIDFMT, 0},
	{"pid", "PID", NULL, 0, kvar, NULL, PIDLEN, KOFF(ki_pid), UINT,
		PIDFMT, 0},
	{"pmem", "", "%mem", 0, NULL, NULL, 0, 0, CHAR, NULL, 0},
	{"ppid", "PPID", NULL, 0, kvar, NULL, PIDLEN, KOFF(ki_ppid), UINT,
		PIDFMT, 0},
	{"pri", "PRI", NULL, 0, pri, NULL, 3, 0, CHAR, NULL, 0},
	{"re", "RE", NULL, INF127, kvar, NULL, 3, KOFF(ki_swtime), UINT, "d",
		0},
	{"rgid", "RGID", NULL, 0, kvar, NULL, UIDLEN, KOFF(ki_rgid),
		UINT, UIDFMT, 0},
	{"rgroup", "RGROUP", NULL, LJUST|DSIZ, rgroupname, s_rgroupname,
		USERLEN, 0, CHAR, NULL, 0},
	{"rss", "RSS", NULL, 0, kvar, NULL, 5, KOFF(ki_rssize), PGTOK, "ld", 0},
	{"rtprio", "RTPRIO", NULL, 0, priorityr, NULL, 7, KOFF(ki_pri), CHAR,
		NULL, 0},
	{"ruid", "RUID", NULL, 0, kvar, NULL, UIDLEN, KOFF(ki_ruid),
		UINT, UIDFMT, 0},
	{"ruser", "RUSER", NULL, LJUST|DSIZ, runame, s_runame, USERLEN,
		0, CHAR, NULL, 0},
	{"sid", "SID", NULL, 0, kvar, NULL, PIDLEN, KOFF(ki_sid), UINT,
		PIDFMT, 0},
	{"sig", "PENDING", NULL, 0, kvar, NULL, 8, KOFF(ki_siglist), INT,
		"x", 0},
	{"sigcatch", "CAUGHT", NULL, 0, kvar, NULL, 8, KOFF(ki_sigcatch),
		UINT, "x", 0},
	{"sigignore", "IGNORED", NULL, 0, kvar, NULL, 8, KOFF(ki_sigignore),
		UINT, "x", 0},
	{"sigmask", "BLOCKED", NULL, 0, kvar, NULL, 8, KOFF(ki_sigmask),
		UINT, "x", 0},
	{"sl", "SL", NULL, INF127, kvar, NULL, 3, KOFF(ki_slptime), UINT, "d",
		0},
	{"start", "STARTED", NULL, LJUST|USER, started, NULL, 7, 0, CHAR, NULL,
		0},
	{"stat", "", "state", 0, NULL, NULL, 0, 0, CHAR, NULL, 0},
	{"state", "STAT", NULL, 0, state, NULL, 4, 0, CHAR, NULL, 0},
	{"svgid", "SVGID", NULL, 0, kvar, NULL, UIDLEN, KOFF(ki_svgid),
		UINT, UIDFMT, 0},
	{"svuid", "SVUID", NULL, 0, kvar, NULL, UIDLEN, KOFF(ki_svuid),
		UINT, UIDFMT, 0},
	{"tdev", "TDEV", NULL, 0, tdev, NULL, 4, 0, CHAR, NULL, 0},
	{"tdnam", "TDNAM", NULL, LJUST, tdnam, NULL, COMMLEN, 0, CHAR, NULL, 0},
	{"time", "TIME", NULL, USER, cputime, NULL, 9, 0, CHAR, NULL, 0},
	{"tpgid", "TPGID", NULL, 0, kvar, NULL, 4, KOFF(ki_tpgid), UINT,
		PIDFMT, 0},
	{"tsid", "TSID", NULL, 0, kvar, NULL, PIDLEN, KOFF(ki_tsid), UINT,
		PIDFMT, 0},
	{"tsiz", "TSIZ", NULL, 0, kvar, NULL, 4, KOFF(ki_tsize), PGTOK, "ld", 0},
	{"tt", "TT ", NULL, 0, tname, NULL, 4, 0, CHAR, NULL, 0},
	{"tty", "TTY", NULL, LJUST, longtname, NULL, 8, 0, CHAR, NULL, 0},
	{"ucomm", "UCOMM", NULL, LJUST, ucomm, NULL, MAXCOMLEN, 0, CHAR, NULL,
		0},
	{"uid", "UID", NULL, 0, kvar, NULL, UIDLEN, KOFF(ki_uid), UINT,
		UIDFMT, 0},
	{"upr", "UPR", NULL, 0, upr, NULL, 3, 0, CHAR, NULL, 0},
	{"uprocp", "UPROCP", NULL, 0, kvar, NULL, 8, KOFF(ki_paddr), KPTR,
		"lx", 0},
	{"user", "USER", NULL, LJUST|DSIZ, uname, s_uname, USERLEN, 0, CHAR,
		NULL, 0},
	{"usrpri", "", "upr", 0, NULL, NULL, 0, 0, CHAR, NULL, 0},
	{"vsize", "", "vsz", 0, NULL, NULL, 0, 0, CHAR, NULL, 0},
	{"vsz", "VSZ", NULL, 0, vsize, NULL, 5, 0, CHAR, NULL, 0},
	{"wchan", "WCHAN", NULL, LJUST, wchan, NULL, 6, 0, CHAR, NULL, 0},
	{"xstat", "XSTAT", NULL, 0, kvar, NULL, 4, KOFF(ki_xstat), USHORT,
		"x", 0},
	{"", NULL, NULL, 0, NULL, NULL, 0, 0, CHAR, NULL, 0},
};

void
showkey(void)
{
	VAR *v;
	int i;
	const char *p, *sep;

	i = 0;
	sep = "";
	for (v = var; *(p = v->name); ++v) {
		int len = strlen(p);
		if (termwidth && (i += len + 1) > termwidth) {
			i = len;
			sep = "\n";
		}
		(void) printf("%s%s", sep, p);
		sep = " ";
	}
	(void) printf("\n");
}

void
parsefmt(const char *p, int user)
{
	char *tempstr, *tempstr1;

#define		FMTSEP	" \t,\n"
	tempstr1 = tempstr = strdup(p);
	while (tempstr && *tempstr) {
		char *cp, *hp;
		VAR *v;
		struct varent *vent;

		/*
		 * If an item contains an equals sign, it specifies a column
		 * header, may contain embedded separator characters and
		 * is always the last item.	
		 */
		if (tempstr[strcspn(tempstr, "="FMTSEP)] != '=')
			while ((cp = strsep(&tempstr, FMTSEP)) != NULL &&
			    *cp == '\0')
				/* void */;
		else {
			cp = tempstr;
			tempstr = NULL;
		}
		if (cp == NULL || !(v = findvar(cp, user, &hp)))
			continue;
		if (!user) {
			/*
			 * If the user is NOT adding this field manually,
			 * get on with our lives if this VAR is already
			 * represented in the list.
			 */
			vent = find_varentry(v);
			if (vent != NULL)
				continue;
		}
		if ((vent = malloc(sizeof(struct varent))) == NULL)
			errx(1, "malloc failed");
		vent->header = v->header;
		if (hp) {
			hp = strdup(hp);
			if (hp)
				vent->header = hp;
		}
		vent->var = malloc(sizeof(*vent->var));
		if (vent->var == NULL)
			errx(1, "malloc failed");
		memcpy(vent->var, v, sizeof(*vent->var));
		STAILQ_INSERT_TAIL(&varlist, vent, next_ve);
	}
	free(tempstr1);
	if (STAILQ_EMPTY(&varlist)) {
		warnx("no valid keywords; valid keywords:");
		showkey();
		exit(1);
	}
}

static VAR *
findvar(char *p, int user, char **header)
{
	size_t rflen;
	VAR *v, key;
	char *hp, *realfmt;

	hp = strchr(p, '=');
	if (hp)
		*hp++ = '\0';

	key.name = p;
	v = bsearch(&key, var, sizeof(var)/sizeof(VAR) - 1, sizeof(VAR), vcmp);

	if (v && v->alias) {
		/*
		 * If the user specified an alternate-header for this
		 * (aliased) format-name, then we need to copy that
		 * alternate-header when making the recursive call to
		 * process the alias.
		 */
		if (hp == NULL)
			parsefmt(v->alias, user);
		else {
			/*
			 * XXX - This processing will not be correct for
			 * any alias which expands into a list of format
			 * keywords.  Presently there are no aliases
			 * which do that.
			 */
			rflen = strlen(v->alias) + strlen(hp) + 2;
			realfmt = malloc(rflen);
			snprintf(realfmt, rflen, "%s=%s", v->alias, hp);
			parsefmt(realfmt, user);
		}
		return ((VAR *)NULL);
	}
	if (!v) {
		warnx("%s: keyword not found", p);
		eval = 1;
	}
	if (header)
		*header = hp;
	return (v);
}

static int
vcmp(const void *a, const void *b)
{
        return (strcmp(((const VAR *)a)->name, ((const VAR *)b)->name));
}
