/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
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
static char sccsid[] = "@(#)keyword.c	5.9 (Berkeley) 6/3/91";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/proc.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "ps.h"

#ifdef SPPWAIT
#define NEWVM
#endif

#ifdef NEWVM
#include <sys/ucred.h>
#include <sys/kinfo_proc.h>
#endif

int	command(), cputime(), evar(), logname(), longtname(), lstarted(),
	maxrss(), p_rssize(), pagein(), pcpu(), pmem(), pri(), pvar(),
	rssize(), runame(), rvar(), started(), state(), tdev(), tname(),
	tsize(), ucomm(), uname(), uvar(), vsize(), wchan();
#ifndef NEWVM
int	trss();
#endif

#ifdef NOTINUSE
int	utime(), stime(), ixrss(), idrss(), isrss();
	{{"utime"}, "UTIME", USER, utime, 4},
	{{"stime"}, "STIME", USER, stime, 4},
	{{"ixrss"}, "IXRSS", USER, ixrss, 4},
	{{"idrss"}, "IDRSS", USER, idrss, 4},
	{{"isrss"}, "ISRSS", USER, isrss, 4},
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
#define	USERLEN	8

VAR var[] = {
#ifdef NEWVM
	{"%cpu", "%CPU", NULL, 0, pcpu, 4},
	{"%mem", "%MEM", NULL, 0, pmem, 4},
	{"acflag", "ACFLG", NULL, 0, pvar, 3, POFF(p_acflag), SHORT, "x"},
	{"acflg", "", "acflag"},
	{"blocked", "", "sigmask"},
	{"caught", "", "sigcatch"},
	{"command", "COMMAND", NULL, COMM|LJUST, command, 16},
	{"cpu", "CPU", NULL, 0, pvar, 3, POFF(p_cpu), UCHAR, "d"},
	{"cputime", "", "time"},
	{"f", "F", NULL, 0, pvar, 7, POFF(p_flag), LONG, "x"},
	{"flags", "", "f"},
	{"ignored", "", "sigignore"},
	{"inblk", "INBLK", NULL, USER, rvar, 4, ROFF(ru_inblock), LONG, "d"},
	{"inblock", "", "inblk"},
	{"jobc", "JOBC", NULL, 0, evar, 4, EOFF(e_jobc), SHORT, "d"},
	{"ktrace", "KTRACE", NULL, 0, pvar, 8, POFF(p_traceflag), LONG, "x"},
	{"ktracep", "KTRACEP", NULL, 0, pvar, 8, POFF(p_tracep), LONG, "x"},
	{"lim", "LIM", NULL, 0, maxrss, 5},
	{"login", "LOGIN", NULL, LJUST, logname, MAXLOGNAME},
	{"logname", "", "login"},
	{"lstart", "STARTED", NULL, LJUST|USER, lstarted, 28},
	{"majflt", "MAJFLT", NULL, USER, rvar, 4, ROFF(ru_majflt), LONG, "d"},
	{"minflt", "MINFLT", NULL, USER, rvar, 4, ROFF(ru_minflt), LONG, "d"},
	{"msgrcv", "MSGRCV", NULL, USER, rvar, 4, ROFF(ru_msgrcv), LONG, "d"},
	{"msgsnd", "MSGSND", NULL, USER, rvar, 4, ROFF(ru_msgsnd), LONG, "d"},
	{"ni", "", "nice"},
	{"nice", "NI", NULL, 0, pvar, 3, POFF(p_nice), CHAR, "d"},
	{"nivcsw", "NIVCSW", NULL, USER, rvar, 5, ROFF(ru_nivcsw), LONG, "d"},
	{"nsignals", "", "nsigs"},
	{"nsigs", "NSIGS", NULL, USER, rvar, 4, ROFF(ru_nsignals), LONG, "d"},
	{"nswap", "NSWAP", NULL, USER, rvar, 4, ROFF(ru_nswap), LONG, "d"},
	{"nvcsw", "NVCSW", NULL, USER, rvar, 5, ROFF(ru_nvcsw), LONG, "d"},
	{"nwchan", "WCHAN", NULL, 0, pvar, 6, POFF(p_wchan), KPTR, "x"},
	{"oublk", "OUBLK", NULL, USER, rvar, 4, ROFF(ru_oublock), LONG, "d"},
	{"oublock", "", "oublk"},
	{"p_ru", "P_RU", NULL, 0, pvar, 6, POFF(p_ru), KPTR, "x"},
	{"paddr", "PADDR", NULL, 0, evar, 6, EOFF(e_paddr), KPTR, "x"},
	{"pagein", "PAGEIN", NULL, USER, pagein, 6},
	{"pcpu", "", "%cpu"},
	{"pending", "", "sig"},
	{"pgid", "PGID", NULL, 0, evar, PIDLEN, EOFF(e_pgid), USHORT, PIDFMT},
	{"pid", "PID", NULL, 0, pvar, PIDLEN, POFF(p_pid),SHORT, PIDFMT},
	{"pmem", "", "%mem"},
	{"ppid", "PPID", NULL, 0, evar, PIDLEN, EOFF(e_ppid), SHORT, PIDFMT},
	{"pri", "PRI", NULL, 0, pri, 3},
	{"re", "RE", NULL, 0, pvar, 3, POFF(p_time), CHAR, "d"},
	{"rgid", "RGID", NULL, 0, evar, UIDLEN, EOFF(e_pcred.p_rgid),
		USHORT, UIDFMT},
	{"rlink", "RLINK", NULL, 0, pvar, 8, POFF(p_rlink), KPTR, "x"},
	{"rss", "RSS", NULL, 0, p_rssize, 4},
	{"rssize", "", "rsz"},
	{"rsz", "RSZ", NULL, 0, rssize, 4},
	{"ruid", "RUID", NULL, 0, evar, UIDLEN, EOFF(e_pcred.p_ruid),
		USHORT, UIDFMT},
	{"ruser", "RUSER", NULL, LJUST, runame, USERLEN},
	{"sess", "SESS", NULL, 0, evar, 6, EOFF(e_sess), KPTR, "x"},
	{"sig", "PENDING", NULL, 0, pvar, 8, POFF(p_sig), LONG, "x"},
	{"sigcatch", "CAUGHT", NULL, 0, pvar, 8, POFF(p_sigcatch), LONG, "x"},
	{"sigignore", "IGNORED",
		NULL, 0, pvar, 8, POFF(p_sigignore), LONG, "x"},
	{"sigmask", "BLOCKED", NULL, 0, pvar, 8, POFF(p_sigmask), LONG, "x"},
	{"sl", "SL", NULL, 0, pvar, 3, POFF(p_slptime), CHAR, "d"},
	{"start", "STARTED", NULL, LJUST|USER, started, 8},
	{"stat", "", "state"},
	{"state", "STAT", NULL, 0, state, 4},
	{"svgid", "SVGID",
		NULL, 0, evar, UIDLEN, EOFF(e_pcred.p_svgid), USHORT, UIDFMT},
	{"svuid", "SVUID",
		NULL, 0, evar, UIDLEN, EOFF(e_pcred.p_svuid), USHORT, UIDFMT},
	{"tdev", "TDEV", NULL, 0, tdev, 4},
	{"time", "TIME", NULL, USER, cputime, 9},
	{"tpgid", "TPGID", NULL, 0, evar, 4, EOFF(e_tpgid), USHORT, PIDFMT},
	{"tsess", "TSESS", NULL, 0, evar, 6, EOFF(e_tsess), KPTR, "x"},
	{"tsiz", "TSIZ", NULL, 0, tsize, 4},
	{"tt", "TT", NULL, LJUST, tname, 3},
	{"tty", "TTY", NULL, LJUST, longtname, 8},
	{"ucomm", "UCOMM", NULL, LJUST, ucomm, MAXCOMLEN},
	{"uid", "UID", NULL, 0, evar, UIDLEN, EOFF(e_ucred.cr_uid),
		USHORT, UIDFMT},
	{"upr", "UPR", NULL, 0, pvar, 3, POFF(p_usrpri), CHAR, "d"},
	{"user", "USER", NULL, LJUST, uname, USERLEN},
	{"usrpri", "", "upr"},
	{"vsize", "", "vsz"},
	{"vsz", "VSZ", NULL, 0, vsize, 5},
	{"wchan", "WCHAN", NULL, LJUST, wchan, 6},
	{"xstat", "XSTAT", NULL, 0, pvar, 4, POFF(p_xstat), USHORT, "x"},
#else
	{"%cpu", "%CPU", NULL, 0, pcpu, 4},
	{"%mem", "%MEM", NULL, 0, pmem, 4},
	{"acflag", "ACFLG", NULL, USER, uvar, 3, UOFF(u_acflag), SHORT, "x"},
	{"acflg", "", "acflag"},
	{"blocked", "", "sigmask"},
	{"caught", "", "sigcatch"},
	{"command", "COMMAND", NULL, COMM|LJUST|USER, command, 16},
	{"cpu", "CPU", NULL, 0, pvar, 3, POFF(p_cpu), UCHAR, "d"},
	{"cputime", "", "time"},
	{"f", "F", NULL, 0, pvar, 7, POFF(p_flag), LONG, "x"},
	{"flags", "", "f"},
	{"ignored", "", "sigignore"},
	{"inblk", "INBLK", NULL, USER, rvar, 4, ROFF(ru_inblock), LONG, "d"},
	{"inblock", "", "inblk"},
	{"jobc", "JOBC", NULL, 0, evar, 4, EOFF(e_jobc), SHORT, "d"},
	{"ktrace", "KTRACE", NULL, 0, pvar, 8, POFF(p_traceflag), LONG, "x"},
	{"ktracep", "KTRACEP", NULL, 0, pvar, 8, POFF(p_tracep), LONG, "x"},
	{"lim", "LIM", NULL, 0, maxrss, 5},
	{"logname", "LOGNAME", NULL, LJUST, logname, MAXLOGNAME},
	{"lstart", "STARTED", NULL, LJUST|USER, lstarted, 28},
	{"majflt", "MAJFLT", NULL, USER, rvar, 4, ROFF(ru_majflt), LONG, "d"},
	{"minflt", "MINFLT", NULL, USER, rvar, 4, ROFF(ru_minflt), LONG, "d"},
	{"msgrcv", "MSGRCV", NULL, USER, rvar, 4, ROFF(ru_msgrcv), LONG, "d"},
	{"msgsnd", "MSGSND", NULL, USER, rvar, 4, ROFF(ru_msgsnd), LONG, "d"},
	{"ni", "", "nice"},
	{"nice", "NI", NULL, 0, pvar, 2, POFF(p_nice), CHAR, "d"},
	{"nivcsw", "NIVCSW", NULL, USER, rvar, 5, ROFF(ru_nivcsw), LONG, "d"},
	{"nsignals", "", "nsigs"},
	{"nsigs", "NSIGS", NULL, USER, rvar, 4, ROFF(ru_nsignals), LONG, "d"},
	{"nswap", "NSWAP", NULL, USER, rvar, 4, ROFF(ru_nswap), LONG, "d"},
	{"nvcsw", "NVCSW", NULL, USER, rvar, 5, ROFF(ru_nvcsw), LONG, "d"},
	{"nwchan", "WCHAN", NULL, 0, pvar, 6, POFF(p_wchan), KPTR, "x"},
	{"oublk", "OUBLK", NULL, USER, rvar, 4, ROFF(ru_oublock), LONG, "d"},
	{"oublock", "", "oublk"},
	{"p_ru", "P_RU", NULL, 0, pvar, 6, POFF(p_ru), KPTR, "x"},
	{"paddr", "PADDR", NULL, 0, evar, 6, EOFF(e_paddr), KPTR, "x"},
	{"pagein", "PAGEIN", NULL, USER, pagein, 6},
	{"pcpu", "", "%cpu"},
	{"pending", "", "sig"},
	{"pgid", "PGID", NULL, 0, evar, PIDLEN, EOFF(e_pgid), USHORT, PIDFMT},
	{"pid", "PID", NULL, 0, pvar, PIDLEN, POFF(p_pid),SHORT, PIDFMT},
	{"pmem", "", "%mem"},
	{"poip", "POIP", NULL, 0, pvar, 4, POFF(p_poip), SHORT, "d"},
	{"ppid", "PPID", NULL, 0, pvar, PIDLEN, POFF(p_ppid), SHORT, PIDFMT},
	{"pri", "PRI", NULL, 0, pri, 3},
	{"re", "RE", NULL, 0, pvar, 3, POFF(p_time), CHAR, "d"},
	{"rgid", "RGID", NULL, 0, pvar, UIDLEN, POFF(p_rgid), USHORT, UIDFMT},
	{"rlink", "RLINK", NULL, 0, pvar, 8, POFF(p_rlink), KPTR, "x"},
	{"rss", "RSS", NULL, 0, p_rssize, 4},
	{"rssize", "", "rsz"},
	{"rsz", "RSZ", NULL, 0, rssize, 4},
	{"ruid", "RUID", NULL, 0, pvar, UIDLEN, POFF(p_ruid), USHORT, UIDFMT},
	{"ruser", "RUSER", NULL, LJUST, runame, USERLEN},
	{"sess", "SESS", NULL, 0, evar, 6, EOFF(e_sess), KPTR, "x"},
	{"sig", "PENDING", NULL, 0, pvar, 8, POFF(p_sig), LONG, "x"},
	{"sigcatch", "CAUGHT", NULL, 0, pvar, 8, POFF(p_sigcatch), LONG, "x"},
	{"sigignore", "IGNORED",
		NULL, 0, pvar, 8, POFF(p_sigignore), LONG, "x"},
	{"sigmask", "BLOCKED", NULL, 0, pvar, 8, POFF(p_sigmask), LONG, "x"},
	{"sl", "SL", NULL, 0, pvar, 3, POFF(p_slptime), CHAR, "d"},
	{"start", "STARTED", NULL, LJUST|USER, started, 8},
	{"stat", "", "state"},
	{"state", "STAT", NULL, 0, state, 4},
	{"svgid", "SVGID",
		NULL, 0, pvar, UIDLEN, POFF(p_svgid), USHORT, UIDFMT},
	{"svuid", "SVUID",
		NULL, 0, pvar, UIDLEN, POFF(p_svuid), USHORT, UIDFMT},
	{"tdev", "TDEV", NULL, 0, tdev, 4},
	{"time", "TIME", NULL, USER, cputime, 9},
	{"tpgid", "TPGID", NULL, 0, evar, 4, EOFF(e_tpgid), USHORT, PIDFMT},
	{"trs", "TRS", NULL, 0, trss, 3},
	{"tsess", "TSESS", NULL, 0, evar, 6, EOFF(e_tsess), KPTR, "x"},
	{"tsiz", "TSIZ", NULL, 0, tsize, 4},
	{"tt", "TT", NULL, LJUST, tname, 3},
	{"tty", "TTY", NULL, LJUST, longtname, 8},
	{"ucomm", "UCOMM", NULL, LJUST, ucomm, MAXCOMLEN},
	{"uid", "UID", NULL, 0, pvar, UIDLEN, POFF(p_uid),USHORT, UIDFMT},
	{"upr", "UPR", NULL, 0, pvar, 3, POFF(p_usrpri), CHAR, "d"},
	{"uprocp", "UPROCP", NULL, USER, uvar, 6, UOFF(u_procp), KPTR, "x"},
	{"user", "USER", NULL, LJUST, uname, USERLEN},
	{"usrpri", "", "upr"},
	{"vsize", "", "vsz"},
	{"vsz", "VSZ", NULL, 0, vsize, 5},
	{"wchan", "WCHAN", NULL, LJUST, wchan, 6},
	{"xstat", "XSTAT", NULL, 0, pvar, 4, POFF(p_xstat), USHORT, "x"},
#endif
	{""},
};

showkey()
{
	extern int termwidth;
	register VAR *v;
	register int i, len;
	register char *p, *sep;

	i = 0;
	sep = "";
	for (v = var; *(p = v->name); ++v) {
		len = strlen(p);
		if (termwidth && (i += len + 1) > termwidth) {
			i = len;
			sep = "\n";
		}
		(void) printf("%s%s", sep, p);
		sep = " ";
	}
	(void) printf("\n");
}

parsefmt(p)
	char *p;
{
	static struct varent *vtail;
	register VAR *v;
	register char *cp;
	register struct varent *vent;
	static VAR *findvar();

#define	FMTSEP	" \t,\n"
	while (p && *p) {
		while ((cp = strsep(&p, FMTSEP)) != NULL && *cp == '\0')
			/* void */;
		if (!(v = findvar(cp)))
			continue;
		if ((vent = malloc(sizeof(struct varent))) == NULL)
			err("%s", strerror(errno));
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
		err("no valid keywords\n");
}

static VAR *
findvar(p)
	char *p;
{
	extern int eval;
	VAR *v, key;
	char *hp;
	int vcmp();

	key.name = p;

	hp = index(p, '=');
	if (hp)
		*hp++ = '\0';

	key.name = p;
	v = (VAR *)bsearch(&key, var,
	    sizeof(var)/sizeof(VAR), sizeof(VAR), vcmp);

	if (v && v->alias) {
		if (hp) {
			(void)fprintf(stderr,
			    "ps: %s: illegal keyword specification\n", p);
			eval = 1;
		}
		parsefmt(v->alias);
		return((VAR *)NULL);
	}
	if (!v) {
		(void)fprintf(stderr, "ps: keyword %s not found\n", p);
		eval = 1;
	}
	if (hp)
		v->header = hp;
	return(v);
}

vcmp(a, b)
        VAR *a, *b;
{
        return(strcmp(a->name, b->name));
}
