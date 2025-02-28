/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxo/xo.h>

#include "ps.h"

static VAR *findvar(char *, struct velisthead *, int, char **header);
static int  vcmp(const void *, const void *);

/* Compute offset in common structures. */
#define	KOFF(x)	offsetof(struct kinfo_proc, x)
#define	ROFF(x)	offsetof(struct rusage, x)

#define	LWPFMT	"d"
#define	NLWPFMT	"d"
#define	UIDFMT	"u"
#define	PIDFMT	"d"

/* PLEASE KEEP THE TABLE BELOW SORTED ALPHABETICALLY!!! */
static VAR var[] = {
	{"%cpu", "%CPU", NULL, "percent-cpu", 0, pcpu, 0, CHAR, NULL},
	{"%mem", "%MEM", NULL, "percent-memory", 0, pmem, 0, CHAR, NULL},
	{"acflag", "ACFLG", NULL, "accounting-flag", 0, kvar, KOFF(ki_acflag),
	    USHORT, "x"},
	{"acflg", "", "acflag", NULL, 0, NULL, 0, CHAR, NULL},
	{"args", "COMMAND", NULL, "arguments", COMM|LJUST|USER, arguments, 0,
	    CHAR, NULL},
	{"blocked", "", "sigmask", NULL, 0, NULL, 0, CHAR, NULL},
	{"caught", "", "sigcatch", NULL, 0, NULL, 0, CHAR, NULL},
	{"class", "CLASS", NULL, "login-class", LJUST, loginclass, 0, CHAR,
	    NULL},
	{"comm", "COMMAND", NULL, "command", LJUST, ucomm, 0, CHAR, NULL},
	{"command", "COMMAND", NULL, "command", COMM|LJUST|USER, command, 0,
	    CHAR, NULL},
	{"cow", "COW", NULL, "copy-on-write-faults", 0, kvar, KOFF(ki_cow),
	    UINT, "u"},
	{"cpu", "C", NULL, "on-cpu", 0, cpunum, 0, CHAR, NULL},
	{"cputime", "", "time", NULL, 0, NULL, 0, CHAR, NULL},
	{"dsiz", "DSIZ", NULL, "data-size", 0, kvar, KOFF(ki_dsize), PGTOK,
	    "ld"},
	{"egid", "", "gid", NULL, 0, NULL, 0, CHAR, NULL},
	{"egroup", "", "group", NULL, 0, NULL, 0, CHAR, NULL},
	{"emul", "EMUL", NULL, "emulation-envirnment", LJUST, emulname, 0,
	    CHAR, NULL},
	{"etime", "ELAPSED", NULL, "elapsed-time", USER, elapsed, 0, CHAR,
	    NULL},
	{"etimes", "ELAPSED", NULL, "elapsed-times", USER, elapseds, 0, CHAR,
	    NULL},
	{"euid", "", "uid", NULL, 0, NULL, 0, CHAR, NULL},
	{"f", "F", NULL, "flags", 0, kvar, KOFF(ki_flag), LONG, "lx"},
	{"f2", "F2", NULL, "flags2", 0, kvar, KOFF(ki_flag2), INT, "08x"},
	{"fib", "FIB", NULL, "fib", 0, kvar, KOFF(ki_fibnum), INT, "d"},
	{"flags", "", "f", NULL, 0, NULL, 0, CHAR, NULL},
	{"flags2", "", "f2", NULL, 0, NULL, 0, CHAR, NULL},
	{"gid", "GID", NULL, "gid", 0, kvar, KOFF(ki_groups), UINT, UIDFMT},
	{"group", "GROUP", NULL, "group", LJUST, egroupname, 0, CHAR, NULL},
	{"ignored", "", "sigignore", NULL, 0, NULL, 0, CHAR, NULL},
	{"inblk", "INBLK", NULL, "read-blocks", USER, rvar, ROFF(ru_inblock),
	    LONG, "ld"},
	{"inblock", "", "inblk", NULL, 0, NULL, 0, CHAR, NULL},
	{"jail", "JAIL", NULL, "jail-name", LJUST, jailname, 0, CHAR, NULL},
	{"jid", "JID", NULL, "jail-id", 0, kvar, KOFF(ki_jid), INT, "d"},
	{"jobc", "JOBC", NULL, "job-control-count", 0, kvar, KOFF(ki_jobc),
	    SHORT, "d"},
	{"ktrace", "KTRACE", NULL, "ktrace", 0, kvar, KOFF(ki_traceflag), INT,
	    "x"},
	{"label", "LABEL", NULL, "label", LJUST, label, 0, CHAR, NULL},
	{"lim", "LIM", NULL, "memory-limit", 0, maxrss, 0, CHAR, NULL},
	{"lockname", "LOCK", NULL, "lock-name", LJUST, lockname, 0, CHAR, NULL},
	{"login", "LOGIN", NULL, "login-name", LJUST, logname, 0, CHAR, NULL},
	{"logname", "", "login", NULL, 0, NULL, 0, CHAR, NULL},
	{"lstart", "STARTED", NULL, "start-time", LJUST|USER, lstarted, 0,
	    CHAR, NULL},
	{"lwp", "LWP", NULL, "thread-id", 0, kvar, KOFF(ki_tid), UINT,
	    LWPFMT},
	{"majflt", "MAJFLT", NULL, "major-faults", USER, rvar, ROFF(ru_majflt),
	    LONG, "ld"},
	{"minflt", "MINFLT", NULL, "minor-faults", USER, rvar, ROFF(ru_minflt),
	    LONG, "ld"},
	{"msgrcv", "MSGRCV", NULL, "received-messages", USER, rvar,
	    ROFF(ru_msgrcv), LONG, "ld"},
	{"msgsnd", "MSGSND", NULL, "sent-messages", USER, rvar,
	    ROFF(ru_msgsnd), LONG, "ld"},
	{"mwchan", "MWCHAN", NULL, "wait-channel", LJUST, mwchan, 0, CHAR,
	    NULL},
	{"ni", "", "nice", NULL, 0, NULL, 0, CHAR, NULL},
	{"nice", "NI", NULL, "nice", 0, kvar, KOFF(ki_nice), CHAR, "d"},
	{"nivcsw", "NIVCSW", NULL, "involuntary-context-switches", USER, rvar,
	    ROFF(ru_nivcsw), LONG, "ld"},
	{"nlwp", "NLWP", NULL, "threads", 0, kvar, KOFF(ki_numthreads), UINT,
	    NLWPFMT},
	{"nsignals", "", "nsigs", NULL, 0, NULL, 0, CHAR, NULL},
	{"nsigs", "NSIGS", NULL, "signals-taken", USER, rvar,
	    ROFF(ru_nsignals), LONG, "ld"},
	{"nswap", "NSWAP", NULL, "swaps", USER, rvar, ROFF(ru_nswap), LONG,
	    "ld"},
	{"nvcsw", "NVCSW", NULL, "voluntary-context-switches", USER, rvar,
	    ROFF(ru_nvcsw), LONG, "ld"},
	{"nwchan", "NWCHAN", NULL, "wait-channel-address", LJUST, nwchan, 0,
	    CHAR, NULL},
	{"oublk", "OUBLK", NULL, "written-blocks", USER, rvar,
	    ROFF(ru_oublock), LONG, "ld"},
	{"oublock", "", "oublk", NULL, 0, NULL, 0, CHAR, NULL},
	{"paddr", "PADDR", NULL, "process-address", 0, kvar, KOFF(ki_paddr),
	    KPTR, "lx"},
	{"pagein", "PAGEIN", NULL, "pageins", USER, pagein, 0, CHAR, NULL},
	{"pcpu", "", "%cpu", NULL, 0, NULL, 0, CHAR, NULL},
	{"pending", "", "sig", NULL, 0, NULL, 0, CHAR, NULL},
	{"pgid", "PGID", NULL, "process-group", 0, kvar, KOFF(ki_pgid), UINT,
	    PIDFMT},
	{"pid", "PID", NULL, "pid", 0, kvar, KOFF(ki_pid), UINT, PIDFMT},
	{"pmem", "", "%mem", NULL, 0, NULL, 0, CHAR, NULL},
	{"ppid", "PPID", NULL, "ppid", 0, kvar, KOFF(ki_ppid), UINT, PIDFMT},
	{"pri", "PRI", NULL, "priority", 0, pri, 0, CHAR, NULL},
	{"re", "RE", NULL, "residency-time", INF127, kvar, KOFF(ki_swtime),
	    UINT, "d"},
	{"rgid", "RGID", NULL, "real-gid", 0, kvar, KOFF(ki_rgid), UINT,
	    UIDFMT},
	{"rgroup", "RGROUP", NULL, "real-group", LJUST, rgroupname, 0, CHAR,
	    NULL},
	{"rss", "RSS", NULL, "rss", 0, kvar, KOFF(ki_rssize), PGTOK, "ld"},
	{"rtprio", "RTPRIO", NULL, "realtime-priority", 0, priorityr,
	    KOFF(ki_pri), CHAR, NULL},
	{"ruid", "RUID", NULL, "real-uid", 0, kvar, KOFF(ki_ruid), UINT,
	    UIDFMT},
	{"ruser", "RUSER", NULL, "real-user", LJUST, runame, 0, CHAR, NULL},
	{"sid", "SID", NULL, "sid", 0, kvar, KOFF(ki_sid), UINT, PIDFMT},
	{"sig", "PENDING", NULL, "signals-pending", 0, kvar, KOFF(ki_siglist),
	    INT, "x"},
	{"sigcatch", "CAUGHT", NULL, "signals-caught", 0, kvar,
	    KOFF(ki_sigcatch), UINT, "x"},
	{"sigignore", "IGNORED", NULL, "signals-ignored", 0, kvar,
	    KOFF(ki_sigignore), UINT, "x"},
	{"sigmask", "BLOCKED", NULL, "signal-mask", 0, kvar, KOFF(ki_sigmask),
	    UINT, "x"},
	{"sl", "SL", NULL, "sleep-time", INF127, kvar, KOFF(ki_slptime), UINT,
	    "d"},
	{"ssiz", "SSIZ", NULL, "stack-size", 0, kvar, KOFF(ki_ssize), PGTOK,
	    "ld"},
	{"start", "STARTED", NULL, "start-time", LJUST|USER, started, 0, CHAR,
	    NULL},
	{"stat", "", "state", NULL, 0, NULL, 0, CHAR, NULL},
	{"state", "STAT", NULL, "state", LJUST, state, 0, CHAR, NULL},
	{"svgid", "SVGID", NULL, "saved-gid", 0, kvar, KOFF(ki_svgid), UINT,
	    UIDFMT},
	{"svuid", "SVUID", NULL, "saved-uid", 0, kvar, KOFF(ki_svuid), UINT,
	    UIDFMT},
	{"systime", "SYSTIME", NULL, "system-time", USER, systime, 0, CHAR,
	    NULL},
	{"tdaddr", "TDADDR", NULL, "thread-address", 0, kvar, KOFF(ki_tdaddr),
	    KPTR, "lx"},
	{"tdev", "TDEV", NULL, "terminal-device", 0, tdev, 0, CHAR, NULL},
	{"tdnam", "", "tdname", NULL, 0, NULL, 0, CHAR, NULL},
	{"tdname", "TDNAME", NULL, "thread-name", LJUST, tdnam, 0, CHAR,
	    NULL},
	{"tid", "", "lwp", NULL, 0, NULL, 0, CHAR, NULL},
	{"time", "TIME", NULL, "cpu-time", USER, cputime, 0, CHAR, NULL},
	{"tpgid", "TPGID", NULL, "terminal-process-gid", 0, kvar,
	    KOFF(ki_tpgid), UINT, PIDFMT},
	{"tracer", "TRACER", NULL, "tracer", 0, kvar, KOFF(ki_tracer), UINT,
	    PIDFMT},
	{"tsid", "TSID", NULL, "terminal-sid", 0, kvar, KOFF(ki_tsid), UINT,
	    PIDFMT},
	{"tsiz", "TSIZ", NULL, "text-size", 0, kvar, KOFF(ki_tsize), PGTOK,
	    "ld"},
	{"tt", "TT ", NULL, "terminal-name", 0, tname, 0, CHAR, NULL},
	{"tty", "TTY", NULL, "tty", LJUST, longtname, 0, CHAR, NULL},
	{"ucomm", "UCOMM", NULL, "accounting-name", LJUST, ucomm, 0, CHAR,
	    NULL},
	{"uid", "UID", NULL, "uid", 0, kvar, KOFF(ki_uid), UINT, UIDFMT},
	{"upr", "UPR", NULL, "user-priority", 0, upr, 0, CHAR, NULL},
	{"uprocp", "UPROCP", NULL, "process-address", 0, kvar, KOFF(ki_paddr),
	    KPTR, "lx"},
	{"user", "USER", NULL, "user", LJUST, username, 0, CHAR, NULL},
	{"usertime", "USERTIME", NULL, "user-time", USER, usertime, 0, CHAR,
	    NULL},
	{"usrpri", "", "upr", NULL, 0, NULL, 0, CHAR, NULL},
	{"vmaddr", "VMADDR", NULL, "vmspace-address", 0, kvar, KOFF(ki_vmspace),
	    KPTR, "lx"},
	{"vsize", "", "vsz", NULL, 0, NULL, 0, CHAR, NULL},
	{"vsz", "VSZ", NULL, "virtual-size", 0, vsize, 0, CHAR, NULL},
	{"wchan", "WCHAN", NULL, "wait-channel", LJUST, wchan, 0, CHAR, NULL},
	{"xstat", "XSTAT", NULL, "exit-status", 0, kvar, KOFF(ki_xstat),
	    USHORT, "x"},
};

void
showkey(void)
{
	const VAR *v;
	const VAR *const end = var + nitems(var);
	const char *sep;
	int i;

	i = 0;
	sep = "";
	xo_open_list("key");
	for (v = var; v < end; ++v) {
		const char *const p = v->name;
		const int len = strlen(p);

		if (termwidth && (i += len + 1) > termwidth) {
			i = len;
			sep = "\n";
		}
		xo_emit("{P:/%hs}{l:key/%hs}", sep, p);
		sep = " ";
	}
	xo_emit("\n");
	xo_close_list("key");
	if (xo_finish() < 0)
		xo_err(1, "stdout");
}

void
parsefmt(const char *p, struct velisthead *const var_list,
    const int user)
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
		if (cp == NULL || !(v = findvar(cp, var_list, user, &hp)))
			continue;
		if (!user) {
			/*
			 * If the user is NOT adding this field manually,
			 * get on with our lives if this VAR is already
			 * represented in the list.
			 */
			vent = find_varentry(v->name);
			if (vent != NULL)
				continue;
		}
		if ((vent = malloc(sizeof(struct varent))) == NULL)
			xo_errx(1, "malloc failed");
		vent->header = v->header;
		if (hp) {
			hp = strdup(hp);
			if (hp)
				vent->header = hp;
		}
		vent->width = strlen(vent->header);
		vent->var = v;
		STAILQ_INSERT_TAIL(var_list, vent, next_ve);
	}
	free(tempstr1);
	if (STAILQ_EMPTY(var_list)) {
		xo_warnx("no valid keywords; valid keywords:");
		showkey();
		exit(1);
	}
}

static VAR *
findvar(char *p, struct velisthead *const var_list, int user, char **header)
{
	size_t rflen;
	VAR *v, key;
	char *hp, *realfmt;

	hp = strchr(p, '=');
	if (hp)
		*hp++ = '\0';

	key.name = p;
	v = bsearch(&key, var, nitems(var), sizeof(VAR), vcmp);

	if (v && v->alias) {
		/*
		 * If the user specified an alternate-header for this
		 * (aliased) format-name, then we need to copy that
		 * alternate-header when making the recursive call to
		 * process the alias.
		 */
		if (hp == NULL)
			parsefmt(v->alias, var_list, user);
		else {
			/*
			 * XXX - This processing will not be correct for
			 * any alias which expands into a list of format
			 * keywords.  Presently there are no aliases
			 * which do that.
			 */
			rflen = strlen(v->alias) + strlen(hp) + 2;
			realfmt = malloc(rflen);
			if (realfmt == NULL)
				xo_errx(1, "malloc failed");
			snprintf(realfmt, rflen, "%s=%s", v->alias, hp);
			parsefmt(realfmt, var_list, user);
			free(realfmt);
		}
		return ((VAR *)NULL);
	}
	if (!v) {
		xo_warnx("%s: keyword not found", p);
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
