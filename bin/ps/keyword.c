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

#ifndef lint
#if 0
static char sccsid[] = "@(#)keyword.c	8.5 (Berkeley) 4/2/94";
#else
static const char rcsid[] =
  "$FreeBSD$";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/ucred.h>
#include <sys/user.h>

#include <err.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utmp.h>

#include "ps.h"

static VAR *findvar __P((char *));
static int  vcmp __P((const void *, const void *));

#ifdef NOTINUSE
int	utime(), stime(), ixrss(), idrss(), isrss();
	{{"utime"}, "UTIME", USER, utime, NULL, 4},
	{{"stime"}, "STIME", USER, stime, NULL, 4},
	{{"ixrss"}, "IXRSS", USER, ixrss, NULL, 4},
	{{"idrss"}, "IDRSS", USER, idrss, NULL, 4},
	{{"isrss"}, "ISRSS", USER, isrss, NULL, 4},
#endif

/* Compute offset in common structures. */
#define	POFF(x)	offsetof(struct proc, x)
#define	EOFF(x)	offsetof(struct eproc, x)
#define	UOFF(x)	offsetof(struct usave, x)
#define	ROFF(x)	offsetof(struct rusage, x)

#define	UIDFMT	"u"
#define	UIDLEN	5
#define	PIDFMT	"d"
#define	PIDLEN	5
#define USERLEN UT_NAMESIZE

VAR var[] = {
	{"%cpu", "%CPU", NULL, 0, pcpu, NULL, 4},
	{"%mem", "%MEM", NULL, 0, pmem, NULL, 4},
	{"acflag", "ACFLG",
		NULL, 0, pvar, NULL, 3, POFF(p_acflag), USHORT, "x"},
	{"acflg", "", "acflag"},
	{"blocked", "", "sigmask"},
	{"caught", "", "sigcatch"},
	{"command", "COMMAND", NULL, COMM|LJUST|USER, command, NULL, 16},
	{"cpu", "CPU", NULL, 0, pvar, NULL, 3, POFF(p_estcpu), UINT, "d"},
	{"cputime", "", "time"},
	{"f", "F", NULL, 0, pvar, NULL, 7, POFF(p_flag), INT, "x"},
	{"flags", "", "f"},
	{"ignored", "", "sigignore"},
	{"inblk", "INBLK",
		NULL, USER, rvar, NULL, 4, ROFF(ru_inblock), LONG, "ld"},
	{"inblock", "", "inblk"},
	{"jobc", "JOBC", NULL, 0, evar, NULL, 4, EOFF(e_jobc), SHORT, "d"},
	{"ktrace", "KTRACE",
		NULL, 0, pvar, NULL, 8, POFF(p_traceflag), INT, "x"},
	{"ktracep", "KTRACEP",
		NULL, 0, pvar, NULL, 8, POFF(p_tracep), LONG, "lx"},
	{"lim", "LIM", NULL, 0, maxrss, NULL, 5},
	{"login", "LOGIN", NULL, LJUST, logname, NULL, MAXLOGNAME-1},
	{"logname", "", "login"},
	{"lstart", "STARTED", NULL, LJUST|USER, lstarted, NULL, 28},
	{"majflt", "MAJFLT",
		NULL, USER, rvar, NULL, 4, ROFF(ru_majflt), LONG, "ld"},
	{"minflt", "MINFLT",
		NULL, USER, rvar, NULL, 4, ROFF(ru_minflt), LONG, "ld"},
	{"msgrcv", "MSGRCV",
		NULL, USER, rvar, NULL, 4, ROFF(ru_msgrcv), LONG, "ld"},
	{"msgsnd", "MSGSND",
		NULL, USER, rvar, NULL, 4, ROFF(ru_msgsnd), LONG, "ld"},
	{"ni", "", "nice"},
	{"nice", "NI", NULL, 0, pvar, NULL, 2, POFF(p_nice), CHAR, "d"},
	{"nivcsw", "NIVCSW",
		NULL, USER, rvar, NULL, 5, ROFF(ru_nivcsw), LONG, "ld"},
	{"nsignals", "", "nsigs"},
	{"nsigs", "NSIGS",
		NULL, USER, rvar, NULL, 4, ROFF(ru_nsignals), LONG, "ld"},
	{"nswap", "NSWAP",
		NULL, USER, rvar, NULL, 4, ROFF(ru_nswap), LONG, "ld"},
	{"nvcsw", "NVCSW",
		NULL, USER, rvar, NULL, 5, ROFF(ru_nvcsw), LONG, "ld"},
	{"nwchan", "WCHAN", NULL, 0, pvar, NULL, 8, POFF(p_wchan), KPTR, "lx"},
	{"oublk", "OUBLK",
		NULL, USER, rvar, NULL, 4, ROFF(ru_oublock), LONG, "ld"},
	{"oublock", "", "oublk"},
	{"p_ru", "P_RU", NULL, 0, pvar, NULL, 6, POFF(p_ru), KPTR, "lx"},
	{"paddr", "PADDR", NULL, 0, evar, NULL, 8, EOFF(e_paddr), KPTR, "lx"},
	{"pagein", "PAGEIN", NULL, USER, pagein, NULL, 6},
	{"pcpu", "", "%cpu"},
	{"pending", "", "sig"},
	{"pgid", "PGID",
		NULL, 0, evar, NULL, PIDLEN, EOFF(e_pgid), UINT, PIDFMT},
	{"pid", "PID", NULL, 0, pvar, NULL, PIDLEN, POFF(p_pid), UINT, PIDFMT},
	{"pmem", "", "%mem"},
	{"ppid", "PPID",
		NULL, 0, evar, NULL, PIDLEN, EOFF(e_ppid), UINT, PIDFMT},
	{"pri", "PRI", NULL, 0, pri, NULL, 3},
	{"re", "RE", NULL, 0, pvar, NULL, 3, POFF(p_swtime), UINT, "d"},
	{"rgid", "RGID", NULL, 0, evar, NULL, UIDLEN, EOFF(e_pcred.p_rgid),
		UINT, UIDFMT},
	{"rlink", "RLINK",
		NULL, 0, pvar, NULL, 8, POFF(p_procq.tqe_prev), KPTR, "lx"},
	{"rss", "RSS", NULL, 0, p_rssize, NULL, 4},
	{"rssize", "", "rsz"},
	{"rsz", "RSZ", NULL, 0, rssize, NULL, 4},
	{"rtprio", "RTPRIO", NULL, 0, rtprior, NULL, 7, POFF(p_rtprio)},
	{"ruid", "RUID", NULL, 0, evar, NULL, UIDLEN, EOFF(e_pcred.p_ruid),
		UINT, UIDFMT},
	{"ruser", "RUSER", NULL, LJUST|DSIZ, runame, s_runame, USERLEN},
	{"sess", "SESS", NULL, 0, evar, NULL, 6, EOFF(e_sess), KPTR, "lx"},
	{"sig", "PENDING", NULL, 0, pvar, NULL, 8, POFF(p_siglist), INT, "x"},
	{"sigcatch", "CAUGHT",
		NULL, 0, evar, NULL, 8, EOFF(e_procsig.ps_sigcatch), UINT, "x"},
	{"sigignore", "IGNORED",
		NULL, 0, evar, NULL, 8, EOFF(e_procsig.ps_sigignore), UINT, "x"},
	{"sigmask", "BLOCKED",
		NULL, 0, pvar, NULL, 8, POFF(p_sigmask), UINT, "x"},
	{"sl", "SL", NULL, 0, pvar, NULL, 3, POFF(p_slptime), UINT, "d"},
	{"start", "STARTED", NULL, LJUST|USER, started, NULL, 7},
	{"stat", "", "state"},
	{"state", "STAT", NULL, 0, state, NULL, 4},
	{"svgid", "SVGID", NULL, 0,
		evar, NULL, UIDLEN, EOFF(e_pcred.p_svgid), UINT, UIDFMT},
	{"svuid", "SVUID", NULL, 0,
		evar, NULL, UIDLEN, EOFF(e_pcred.p_svuid), UINT, UIDFMT},
	{"tdev", "TDEV", NULL, 0, tdev, NULL, 4},
	{"time", "TIME", NULL, USER, cputime, NULL, 9},
	{"tpgid", "TPGID",
		NULL, 0, evar, NULL, 4, EOFF(e_tpgid), UINT, PIDFMT},
	{"tsess", "TSESS", NULL, 0, evar, NULL, 6, EOFF(e_tsess), KPTR, "lx"},
	{"tsiz", "TSIZ", NULL, 0, tsize, NULL, 4},
	{"tt", "TT ", NULL, 0, tname, NULL, 4},
	{"tty", "TTY", NULL, LJUST, longtname, NULL, 8},
	{"ucomm", "UCOMM", NULL, LJUST, ucomm, NULL, MAXCOMLEN},
	{"uid", "UID", NULL, 0, evar, NULL, UIDLEN, EOFF(e_ucred.cr_uid),
		UINT, UIDFMT},
	{"upr", "UPR", NULL, 0, pvar, NULL, 3, POFF(p_usrpri), CHAR, "d"},
	{"user", "USER", NULL, LJUST|DSIZ, uname, s_uname, USERLEN},
	{"usrpri", "", "upr"},
	{"vsize", "", "vsz"},
	{"vsz", "VSZ", NULL, 0, vsize, NULL, 5},
	{"wchan", "WCHAN", NULL, LJUST, wchan, NULL, 6},
	{"xstat", "XSTAT", NULL, 0, pvar, NULL, 4, POFF(p_xstat), USHORT, "x"},
	{""},
};

void
showkey()
{
	VAR *v;
	int i;
	char *p, *sep;

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
parsefmt(p)
	char *p;
{
	static struct varent *vtail;

#define	FMTSEP	" \t,\n"
	while (p && *p) {
		char *cp;
		VAR *v;
		struct varent *vent;

		while ((cp = strsep(&p, FMTSEP)) != NULL && *cp == '\0')
			/* void */;
		if (cp == NULL || !(v = findvar(cp)))
			continue;
		if ((vent = malloc(sizeof(struct varent))) == NULL)
			err(1, NULL);
		vent->var = v;
		vent->next = NULL;
		if (vhead == NULL)
			vhead = vtail = vent;
		else {
			vtail->next = vent;
			vtail = vent;
		}
	}
	if (!vhead)
		errx(1, "no valid keywords");
}

static VAR *
findvar(p)
	char *p;
{
	VAR *v, key;
	char *hp;
	int vcmp();

	hp = strchr(p, '=');
	if (hp)
		*hp++ = '\0';

	key.name = p;
	v = bsearch(&key, var, sizeof(var)/sizeof(VAR) - 1, sizeof(VAR), vcmp);

	if (v && v->alias) {
		if (hp) {
			warnx("%s: illegal keyword specification", p);
			eval = 1;
		}
		parsefmt(v->alias);
		return ((VAR *)NULL);
	}
	if (!v) {
		warnx("%s: keyword not found", p);
		eval = 1;
	} else if (hp)
		v->header = hp;
	return (v);
}

static int
vcmp(a, b)
        const void *a, *b;
{
        return (strcmp(((VAR *)a)->name, ((VAR *)b)->name));
}
