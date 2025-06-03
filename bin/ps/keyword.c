/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2025 The FreeBSD Foundation
 *
 * Portions of this software were developed by Olivier Certner
 * <olce@FreeBSD.org> at Kumacom SARL under sponsorship from the FreeBSD
 * Foundation.
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

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxo/xo.h>

#include "ps.h"

static int  vcmp(const void *, const void *);

/* Compute offset in common structures. */
#define	KOFF(x)	offsetof(struct kinfo_proc, x)
#define	ROFF(x)	offsetof(struct rusage, x)

#define	LWPFMT	"d"
#define	NLWPFMT	"d"
#define	UIDFMT	"u"
#define	PIDFMT	"d"

/* PLEASE KEEP THE TABLE BELOW SORTED ALPHABETICALLY!!! */
static VAR keywords[] = {
	{"%cpu", {NULL}, "%CPU", "percent-cpu", 0, pcpu, 0, UNSPEC, NULL},
	{"%mem", {NULL}, "%MEM", "percent-memory", 0, pmem, 0, UNSPEC, NULL},
	{"acflag", {NULL}, "ACFLG", "accounting-flag", 0, kvar, KOFF(ki_acflag),
	 USHORT, "x"},
	{"acflg", {"acflag"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"args", {NULL}, "COMMAND", "arguments", COMM|LJUST|USER, arguments, 0,
	 UNSPEC, NULL},
	{"blocked", {"sigmask"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"caught", {"sigcatch"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"class", {NULL}, "CLASS", "login-class", LJUST, loginclass, 0,
	 UNSPEC, NULL},
	{"comm", {NULL}, "COMMAND", "command", LJUST, ucomm, 0, UNSPEC, NULL},
	{"command", {NULL}, "COMMAND", "command", COMM|LJUST|USER, command, 0,
	 UNSPEC, NULL},
	{"cow", {NULL}, "COW", "copy-on-write-faults", 0, kvar, KOFF(ki_cow),
	 UINT, "u"},
	{"cpu", {NULL}, "C", "on-cpu", 0, cpunum, 0, UNSPEC, NULL},
	{"cputime", {"time"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"dsiz", {NULL}, "DSIZ", "data-size", 0, kvar, KOFF(ki_dsize),
	 PGTOK, "ld"},
	{"egid", {"gid"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"egroup", {"group"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"emul", {NULL}, "EMUL", "emulation-envirnment", LJUST, emulname, 0,
	 UNSPEC, NULL},
	{"etime", {NULL}, "ELAPSED", "elapsed-time", USER, elapsed, 0,
	 UNSPEC, NULL},
	{"etimes", {NULL}, "ELAPSED", "elapsed-times", USER, elapseds, 0,
	 UNSPEC, NULL},
	{"euid", {"uid"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"f", {NULL}, "F", "flags", 0, kvar, KOFF(ki_flag), LONG, "lx"},
	{"f2", {NULL}, "F2", "flags2", 0, kvar, KOFF(ki_flag2), INT, "08x"},
	{"fib", {NULL}, "FIB", "fib", 0, kvar, KOFF(ki_fibnum), INT, "d"},
	{"flags", {"f"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"flags2", {"f2"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"gid", {NULL}, "GID", "gid", 0, kvar, KOFF(ki_groups), UINT, UIDFMT},
	{"group", {NULL}, "GROUP", "group", LJUST, egroupname, 0, UNSPEC, NULL},
	{"ignored", {"sigignore"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"inblk", {NULL}, "INBLK", "read-blocks", USER, rvar, ROFF(ru_inblock),
	 LONG, "ld"},
	{"inblock", {"inblk"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"jail", {NULL}, "JAIL", "jail-name", LJUST, jailname, 0, UNSPEC, NULL},
	{"jid", {NULL}, "JID", "jail-id", 0, kvar, KOFF(ki_jid), INT, "d"},
	{"jobc", {NULL}, "JOBC", "job-control-count", 0, kvar, KOFF(ki_jobc),
	 SHORT, "d"},
	{"ktrace", {NULL}, "KTRACE", "ktrace", 0, kvar, KOFF(ki_traceflag),
	 INT, "x"},
	{"label", {NULL}, "LABEL", "label", LJUST, label, 0, UNSPEC, NULL},
	{"lim", {NULL}, "LIM", "memory-limit", 0, maxrss, 0, UNSPEC, NULL},
	{"lockname", {NULL}, "LOCK", "lock-name", LJUST, lockname, 0,
	 UNSPEC, NULL},
	{"login", {NULL}, "LOGIN", "login-name", LJUST, logname, 0,
	 UNSPEC, NULL},
	{"logname", {"login"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"lstart", {NULL}, "STARTED", "start-time", LJUST|USER, lstarted, 0,
	 UNSPEC, NULL},
	{"lwp", {NULL}, "LWP", "thread-id", 0, kvar, KOFF(ki_tid),
	 UINT, LWPFMT},
	{"majflt", {NULL}, "MAJFLT", "major-faults", USER, rvar, ROFF(ru_majflt),
	 LONG, "ld"},
	{"minflt", {NULL}, "MINFLT", "minor-faults", USER, rvar, ROFF(ru_minflt),
	 LONG, "ld"},
	{"msgrcv", {NULL}, "MSGRCV", "received-messages", USER, rvar,
	 ROFF(ru_msgrcv), LONG, "ld"},
	{"msgsnd", {NULL}, "MSGSND", "sent-messages", USER, rvar,
	 ROFF(ru_msgsnd), LONG, "ld"},
	{"mwchan", {NULL}, "MWCHAN", "wait-channel", LJUST, mwchan, 0,
	 UNSPEC, NULL},
	{"ni", {"nice"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"nice", {NULL}, "NI", "nice", 0, kvar, KOFF(ki_nice), CHAR, "d"},
	{"nivcsw", {NULL}, "NIVCSW", "involuntary-context-switches", USER, rvar,
	 ROFF(ru_nivcsw), LONG, "ld"},
	{"nlwp", {NULL}, "NLWP", "threads", 0, kvar, KOFF(ki_numthreads),
	 UINT, NLWPFMT},
	{"nsignals", {"nsigs"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"nsigs", {NULL}, "NSIGS", "signals-taken", USER, rvar,
	 ROFF(ru_nsignals), LONG, "ld"},
	{"nswap", {NULL}, "NSWAP", "swaps", USER, rvar, ROFF(ru_nswap),
	 LONG, "ld"},
	{"nvcsw", {NULL}, "NVCSW", "voluntary-context-switches", USER, rvar,
	 ROFF(ru_nvcsw), LONG, "ld"},
	{"nwchan", {NULL}, "NWCHAN", "wait-channel-address", LJUST, nwchan, 0,
	 UNSPEC, NULL},
	{"oublk", {NULL}, "OUBLK", "written-blocks", USER, rvar,
	 ROFF(ru_oublock), LONG, "ld"},
	{"oublock", {"oublk"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"paddr", {NULL}, "PADDR", "process-address", 0, kvar, KOFF(ki_paddr),
	 KPTR, "lx"},
	{"pagein", {NULL}, "PAGEIN", "pageins", USER, pagein, 0, UNSPEC, NULL},
	{"pcpu", {"%cpu"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"pending", {"sig"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"pgid", {NULL}, "PGID", "process-group", 0, kvar, KOFF(ki_pgid),
	 UINT, PIDFMT},
	{"pid", {NULL}, "PID", "pid", 0, kvar, KOFF(ki_pid), UINT, PIDFMT},
	{"pmem", {"%mem"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"ppid", {NULL}, "PPID", "ppid", 0, kvar, KOFF(ki_ppid), UINT, PIDFMT},
	{"pri", {NULL}, "PRI", "priority", 0, pri, 0, UNSPEC, NULL},
	{"re", {NULL}, "RE", "residency-time", INF127, kvar, KOFF(ki_swtime),
	 UINT, "d"},
	{"rgid", {NULL}, "RGID", "real-gid", 0, kvar, KOFF(ki_rgid),
	 UINT, UIDFMT},
	{"rgroup", {NULL}, "RGROUP", "real-group", LJUST, rgroupname, 0,
	 UNSPEC, NULL},
	{"rss", {NULL}, "RSS", "rss", 0, kvar, KOFF(ki_rssize), PGTOK, "ld"},
	{"rtprio", {NULL}, "RTPRIO", "realtime-priority", 0, priorityr,
	 KOFF(ki_pri), UNSPEC, NULL},
	{"ruid", {NULL}, "RUID", "real-uid", 0, kvar, KOFF(ki_ruid),
	 UINT, UIDFMT},
	{"ruser", {NULL}, "RUSER", "real-user", LJUST, runame, 0, UNSPEC, NULL},
	{"sid", {NULL}, "SID", "sid", 0, kvar, KOFF(ki_sid), UINT, PIDFMT},
	{"sig", {NULL}, "PENDING", "signals-pending", 0, kvar, KOFF(ki_siglist),
	 INT, "x"},
	{"sigcatch", {NULL}, "CAUGHT", "signals-caught", 0, kvar,
	 KOFF(ki_sigcatch), UINT, "x"},
	{"sigignore", {NULL}, "IGNORED", "signals-ignored", 0, kvar,
	 KOFF(ki_sigignore), UINT, "x"},
	{"sigmask", {NULL}, "BLOCKED", "signal-mask", 0, kvar, KOFF(ki_sigmask),
	 UINT, "x"},
	{"sl", {NULL}, "SL", "sleep-time", INF127, kvar, KOFF(ki_slptime),
	 UINT, "d"},
	{"ssiz", {NULL}, "SSIZ", "stack-size", 0, kvar, KOFF(ki_ssize),
	 PGTOK, "ld"},
	{"start", {NULL}, "STARTED", "start-time", LJUST|USER, started, 0,
	 UNSPEC, NULL},
	{"stat", {"state"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"state", {NULL}, "STAT", "state", LJUST, state, 0, UNSPEC, NULL},
	{"svgid", {NULL}, "SVGID", "saved-gid", 0, kvar, KOFF(ki_svgid),
	 UINT, UIDFMT},
	{"svuid", {NULL}, "SVUID", "saved-uid", 0, kvar, KOFF(ki_svuid),
	 UINT, UIDFMT},
	{"systime", {NULL}, "SYSTIME", "system-time", USER, systime, 0,
	 UNSPEC, NULL},
	{"tdaddr", {NULL}, "TDADDR", "thread-address", 0, kvar, KOFF(ki_tdaddr),
	 KPTR, "lx"},
	{"tdev", {NULL}, "TDEV", "terminal-device", 0, tdev, 0, UNSPEC, NULL},
	{"tdnam", {"tdname"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"tdname", {NULL}, "TDNAME", "thread-name", LJUST, tdnam, 0,
	 UNSPEC, NULL},
	{"tid", {"lwp"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"time", {NULL}, "TIME", "cpu-time", USER, cputime, 0, UNSPEC, NULL},
	{"tpgid", {NULL}, "TPGID", "terminal-process-gid", 0, kvar,
	 KOFF(ki_tpgid), UINT, PIDFMT},
	{"tracer", {NULL}, "TRACER", "tracer", 0, kvar, KOFF(ki_tracer),
	 UINT, PIDFMT},
	{"tsid", {NULL}, "TSID", "terminal-sid", 0, kvar, KOFF(ki_tsid),
	 UINT, PIDFMT},
	{"tsiz", {NULL}, "TSIZ", "text-size", 0, kvar, KOFF(ki_tsize),
	 PGTOK, "ld"},
	{"tt", {NULL}, "TT ", "terminal-name", 0, tname, 0, UNSPEC, NULL},
	{"tty", {NULL}, "TTY", "tty", LJUST, longtname, 0, UNSPEC, NULL},
	{"ucomm", {NULL}, "UCOMM", "accounting-name", LJUST, ucomm, 0,
	 UNSPEC, NULL},
	{"uid", {NULL}, "UID", "uid", 0, kvar, KOFF(ki_uid), UINT, UIDFMT},
	{"upr", {NULL}, "UPR", "user-priority", 0, upr, 0, UNSPEC, NULL},
	{"uprocp", {NULL}, "UPROCP", "process-address", 0, kvar, KOFF(ki_paddr),
	 KPTR, "lx"},
	{"user", {NULL}, "USER", "user", LJUST, username, 0, UNSPEC, NULL},
	{"usertime", {NULL}, "USERTIME", "user-time", USER, usertime, 0,
	 UNSPEC, NULL},
	{"usrpri", {"upr"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"vmaddr", {NULL}, "VMADDR", "vmspace-address", 0, kvar,
	 KOFF(ki_vmspace), KPTR, "lx"},
	{"vsize", {"vsz"}, NULL, NULL, 0, NULL, 0, UNSPEC, NULL},
	{"vsz", {NULL}, "VSZ", "virtual-size", 0, vsize, 0, UNSPEC, NULL},
	{"wchan", {NULL}, "WCHAN", "wait-channel", LJUST, wchan, 0,
	 UNSPEC, NULL},
	{"xstat", {NULL}, "XSTAT", "exit-status", 0, kvar, KOFF(ki_xstat),
	 USHORT, "x"},
};

const size_t known_keywords_nb = nitems(keywords);

size_t
aliased_keyword_index(const VAR *const v)
{
	const VAR *const fv = (v->flag & RESOLVED_ALIAS) == 0 ?
	    v : v->final_kw;
	const size_t idx = fv - keywords;

	assert(idx < known_keywords_nb);
	return (idx);
}

/*
 * Sanity checks on declared keywords.
 *
 * Checks specific to aliases are done in resolve_alias() instead.
 *
 * Currently, only checks that keywords are alphabetically ordered by their
 * names.  More checks could be added, such as the absence of type (UNSPEC),
 * 'fmt' (NULL) when the output routine is not kval()/rval().
 *
 * Called from main() on PS_CHECK_KEYWORDS, else available when debugging.
 */
void
check_keywords(void)
{
	const VAR *k, *next_k;
	bool order_violated = false;

	k = &keywords[0];
	for (size_t i = 1; i < known_keywords_nb; ++i) {
		next_k = &keywords[i];
		if (vcmp(k, next_k) >= 0) {
			xo_warnx("keywords bad order: '%s' followed by '%s'",
			    k->name, next_k->name);
			order_violated = true;
		}
		k = next_k;
	}
	if (order_violated)
		/* Must be the case as we rely on bsearch() + vcmp(). */
		xo_errx(2, "keywords are not in ascending order "
		    "(internal error)");
}

static void
alias_errx(const char *const name, const char *const what)
{
	xo_errx(2, "alias keyword '%s' specifies %s (internal error)",
	    name, what);
}

static void
merge_alias(VAR *const k, VAR *const tgt)
{
	if ((tgt->flag & RESOLVED_ALIAS) != 0)
		k->final_kw = tgt->final_kw;
	else {
		k->final_kw = tgt;
		assert(tgt->aliased == NULL);
	}

#define MERGE_IF_SENTINEL(field, sentinel) do {				\
	if (k->field == sentinel)					\
		k->field = tgt->field;					\
} while (0)

	MERGE_IF_SENTINEL(header, NULL);
	MERGE_IF_SENTINEL(field, NULL);
	/* If NOINHERIT is present, no merge occurs. */
	MERGE_IF_SENTINEL(flag, 0);

#undef MERGE_IF_SENTINEL

	/* We also check that aliases don't specify things they should not. */
#define MERGE_CHECK_SENTINEL(field, sentinel, field_descr) do {		\
	if (k->field != sentinel)					\
		alias_errx(k->name, field_descr);			\
	k->field = tgt->field;						\
} while (0);

	MERGE_CHECK_SENTINEL(oproc, NULL, "an output routine");
	MERGE_CHECK_SENTINEL(off, 0, "a structure offset");
	MERGE_CHECK_SENTINEL(type, UNSPEC, "a different type than UNSPEC");
	MERGE_CHECK_SENTINEL(fmt, NULL, "a printf format");

#undef MERGE_CHECK_SENTINEL
}

static void
resolve_alias(VAR *const k)
{
	VAR *t, key;

	if ((k->flag & RESOLVED_ALIAS) != 0 || k->aliased == NULL)
		return;

	if ((k->flag & RESOLVING_ALIAS) != 0)
		xo_errx(2, "cycle when resolving alias keyword '%s'", k->name);
	k->flag |= RESOLVING_ALIAS;

	key.name = k->aliased;
	t = bsearch(&key, keywords, known_keywords_nb, sizeof(VAR), vcmp);
	if (t == NULL)
		xo_errx(2, "unknown target '%s' for keyword alias '%s'",
		    k->aliased, k->name);

	resolve_alias(t);
	merge_alias(k, t);

	k->flag &= ~RESOLVING_ALIAS;
	k->flag |= RESOLVED_ALIAS;
}

/*
 * Resolve all aliases immediately.
 *
 * Called from main() on PS_CHECK_KEYWORDS, else available when debugging.
 */
void
resolve_aliases(void)
{
	for (size_t i = 0; i < known_keywords_nb; ++i)
		resolve_alias(&keywords[i]);
}

void
showkey(void)
{
	const VAR *v;
	const VAR *const end = keywords + known_keywords_nb;
	const char *sep;
	int i;

	i = 0;
	sep = "";
	xo_open_list("key");
	for (v = keywords; v < end; ++v) {
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
	char *copy, *cp;
	char *hdr_p, sep;
	size_t sep_idx;
	VAR *v, key;
	struct varent *vent;

	cp = copy = strdup(p);
	if (copy == NULL)
		xo_err(1, "strdup");

	sep = cp[0]; /* We only care if it's 0 or not here. */
	sep_idx = -1;
	while (sep != '\0') {
		cp += sep_idx + 1;

		/*
		 * If an item contains an equals sign, it specifies a column
		 * header, may contain embedded separator characters and
		 * is always the last item.
		 */
		sep_idx = strcspn(cp, "= \t,\n");
		sep = cp[sep_idx];
		cp[sep_idx] = 0;
		if (sep == '=') {
			hdr_p = cp + sep_idx + 1;
			sep = '\0'; /* No more keywords. */
		} else
			hdr_p = NULL;

		/* At this point, '*cp' is '\0' iff 'sep_idx' is 0. */
		if (*cp == '\0') {
			/*
			 * Empty keyword.  Skip it, and silently unless some
			 * header has been specified.
			 */
			if (hdr_p != NULL)
				xo_warnx("empty keyword with header '%s'",
				    hdr_p);
			continue;
		}

		/* Find the keyword. */
		key.name = cp;
		v = bsearch(&key, keywords,
		    known_keywords_nb, sizeof(VAR), vcmp);
		if (v == NULL) {
			xo_warnx("%s: keyword not found", cp);
			eval = 1;
			continue;
		}

#ifndef PS_CHECK_KEYWORDS
		/*
		 * On PS_CHECK_KEYWORDS, this is not necessary as all aliases
		 * are resolved at startup in main() by calling
		 * resolve_aliases().
		 */
		resolve_alias(v);
#endif

		if ((vent = malloc(sizeof(struct varent))) == NULL)
			xo_errx(1, "malloc failed");
		vent->header = v->header;
		if (hdr_p) {
			hdr_p = strdup(hdr_p);
			if (hdr_p)
				vent->header = hdr_p;
		}
		vent->width = strlen(vent->header);
		vent->var = v;
		vent->flags = user ? VE_KEEP : 0;
		STAILQ_INSERT_TAIL(var_list, vent, next_ve);
	}

	free(copy);

	if (STAILQ_EMPTY(var_list)) {
		xo_warnx("no valid keywords; valid keywords:");
		showkey();
		exit(1);
	}
}

static int
vcmp(const void *a, const void *b)
{
        return (strcmp(((const VAR *)a)->name, ((const VAR *)b)->name));
}
