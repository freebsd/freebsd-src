/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2008-2022 NetApp, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/boottrace.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sbuf.h>
#include <sys/stdarg.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#define	dprintf(fmt, ...)						\
	do {								\
		if (dotrace_debugging)					\
			printf(fmt, ##__VA_ARGS__);			\
	} while (0);

static MALLOC_DEFINE(M_BOOTTRACE, "boottrace", "memory for boot tracing");

#define	BT_TABLE_DEFSIZE	3000
#define	BT_TABLE_RUNSIZE	2000
#define	BT_TABLE_SHTSIZE	1000
#define	BT_TABLE_MINSIZE	500

/*
 * Boot-time & shutdown-time event.
 */
struct bt_event {
	uint64_t tsc;			/* Timestamp */
	uint64_t tick;			/* Kernel tick */
	uint64_t cputime;		/* Microseconds of process CPU time */
	uint32_t cpuid;			/* CPU on which the event ran */
	uint32_t inblock;		/* # of blocks in */
	uint32_t oublock;		/* # of blocks out */
	pid_t pid;			/* Current PID */
	char name[BT_EVENT_NAMELEN];	/* Event name */
	char tdname[BT_EVENT_TDNAMELEN]; /* Thread name */
};

struct bt_table {
	uint32_t size;			/* Trace table size */
	uint32_t curr;			/* Trace entry to use */
	uint32_t wrap;			/* Wrap-around, instead of dropping */
	uint32_t drops_early;		/* Trace entries dropped before init */
	uint32_t drops_full;		/* Trace entries dropped after full */
	struct bt_event *table;
};

/* Boot-time tracing */
static struct bt_table bt;

/* Run-time tracing */
static struct bt_table rt;

/* Shutdown-time tracing */
static struct bt_table st;

/* Set when system boot is complete, and we've switched to runtime tracing. */
static bool bootdone;

/* Set when system shutdown has started. */
static bool shutdown;

/* Turn on dotrace() debug logging to console. */
static bool dotrace_debugging;
TUNABLE_BOOL("debug.boottrace.dotrace_debugging", &dotrace_debugging);

/* Trace kernel events */
static bool dotrace_kernel = true;
TUNABLE_BOOL("kern.boottrace.dotrace_kernel", &dotrace_kernel);

/* Trace userspace events */
static bool dotrace_user = true;
TUNABLE_BOOL("kern.boottrace.dotrace_user", &dotrace_user);

static int sysctl_log(SYSCTL_HANDLER_ARGS);
static int sysctl_boottrace(SYSCTL_HANDLER_ARGS);
static int sysctl_runtrace(SYSCTL_HANDLER_ARGS);
static int sysctl_shuttrace(SYSCTL_HANDLER_ARGS);
static int sysctl_boottrace_reset(SYSCTL_HANDLER_ARGS);

SYSCTL_NODE(_kern, OID_AUTO, boottrace, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "boottrace statistics");

SYSCTL_PROC(_kern_boottrace, OID_AUTO, log,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE | CTLFLAG_SKIP,
    0, 0, sysctl_log, "A",
    "Print a log of the boottrace trace data");
SYSCTL_PROC(_kern_boottrace, OID_AUTO, boottrace,
    CTLTYPE_STRING | CTLFLAG_WR | CTLFLAG_MPSAFE,
    0, 0, sysctl_boottrace, "A",
    "Capture a boot-time trace event");
SYSCTL_PROC(_kern_boottrace, OID_AUTO, runtrace,
    CTLTYPE_STRING | CTLFLAG_WR | CTLFLAG_MPSAFE,
    0, 0, sysctl_runtrace, "A",
    "Capture a run-time trace event");
SYSCTL_PROC(_kern_boottrace, OID_AUTO, shuttrace,
    CTLTYPE_STRING | CTLFLAG_WR | CTLFLAG_MPSAFE,
    0, 0, sysctl_shuttrace, "A",
    "Capture a shutdown-time trace event");
SYSCTL_PROC(_kern_boottrace, OID_AUTO, reset,
    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_MPSAFE,
    0, 0, sysctl_boottrace_reset, "A",
    "Reset run-time tracing table");

/*
 * Global enable.
 */
bool __read_mostly boottrace_enabled = false;
SYSCTL_BOOL(_kern_boottrace, OID_AUTO, enabled, CTLFLAG_RDTUN,
    &boottrace_enabled, 0,
    "Boot-time and shutdown-time tracing enabled");

/*
 * Enable dumping of the shutdown trace entries to console.
 */
bool shutdown_trace = false;
SYSCTL_BOOL(_kern_boottrace, OID_AUTO, shutdown_trace, CTLFLAG_RWTUN,
   &shutdown_trace, 0,
   "Enable kernel shutdown tracing to the console");

/*
 * Set the delta threshold (ms) below which events are ignored, used in
 * determining what to dump to the console.
 */
static int shutdown_trace_threshold;
SYSCTL_INT(_kern_boottrace, OID_AUTO, shutdown_trace_threshold, CTLFLAG_RWTUN,
   &shutdown_trace_threshold, 0,
   "Tracing threshold (ms) below which tracing is ignored");

SYSCTL_UINT(_kern_boottrace, OID_AUTO, table_size,
    CTLFLAG_RDTUN | CTLFLAG_NOFETCH, &bt.size, 0,
    "Boot-time tracing table size");

/*
 * Dump a trace to buffer or if buffer is NULL to console.
 *
 * Non-zero delta_threshold selectively prints entries based on delta
 * with current and previous entry. Otherwise, delta_threshold of 0
 * prints every trace entry and delta.
 *
 * Output something like this:
 *
 * CPU      msecs      delta process                  event
 *  11 1228262715          0 init                     shutdown pre sync begin
 *   3 1228265622       2907 init                     shutdown pre sync complete
 *   3 1228265623          0 init                     shutdown turned swap off
 *  18 1228266466        843 init                     shutdown unmounted all filesystems
 *
 * How to interpret:
 *
 * delta column represents the time in milliseconds between this event and the previous.
 * Usually that means you can take the previous event, current event, match them
 * up in the code, and whatever lies between is the culprit taking time.
 *
 * For example, above: Pre-Sync is taking 2907ms, and something between swap and unmount
 * filesystems is taking 843 milliseconds.
 *
 * An event with a delta of 0 are 'landmark' events that simply exist in the output
 * for the developer to know where the time measurement begins. The 0 is an arbitrary
 * number that can effectively be ignored.
 */
#define BTC_DELTA_PRINT(bte, msecs, delta) do {				\
	if (sbp) {							\
		sbuf_printf(sbp, fmt, (bte)->cpuid, msecs, delta,	\
		    (bte)->tdname, (bte)->name, (bte)->pid,		\
		    (bte)->cputime / 1000000,				\
		    ((bte)->cputime % 1000000) / 10000,			\
		    (bte)->inblock, (bte)->oublock);			\
	} else {							\
		printf(fmt, (bte)->cpuid, msecs, delta,			\
		    (bte)->tdname, (bte)->name, (bte)->pid,		\
		    (bte)->cputime / 1000000,				\
		    ((bte)->cputime % 1000000) / 10000,			\
		    (bte)->inblock, (bte)->oublock);			\
	}								\
} while (0)

/*
 * Print the trace entries to the message buffer, or to an sbuf, if provided.
 *
 * Entries with a difference less than dthres will not be printed.
 */
static void
boottrace_display(struct sbuf *sbp, struct bt_table *btp, uint64_t dthres)
{
	struct bt_event *evtp;
	struct bt_event *last_evtp;
	uint64_t msecs;
	uint64_t first_msecs;
	uint64_t last_msecs;
	uint64_t dmsecs;
	uint64_t last_dmsecs;
	uint64_t total_dmsecs;
	uint32_t i;
	uint32_t curr;
	const char *fmt     = "%3u %10llu %10llu %-24s %-40s %5d %4d.%02d %5u %5u\n";
	const char *hdr_fmt = "\n\n%3s %10s %10s %-24s %-40s %5s %6s %5s %5s\n";
	bool printed;
	bool last_printed;

	/* Print the header */
	if (sbp != NULL)
		sbuf_printf(sbp, hdr_fmt,
		    "CPU", "msecs", "delta", "process",
		    "event", "PID", "CPUtime", "IBlks", "OBlks");
	else
		printf(hdr_fmt,
		    "CPU", "msecs", "delta", "process",
		    "event", "PID", "CPUtime", "IBlks", "OBlks");

	first_msecs = 0;
	last_evtp = NULL;
	last_msecs = 0;
	last_dmsecs = 0;
	last_printed = false;
	i = curr = btp->curr;

	do {
		evtp = &btp->table[i];
		if (evtp->tsc == 0)
			goto next;

		msecs = cputick2usec(evtp->tick) / 1000;
		dmsecs = (last_msecs != 0 && msecs > last_msecs) ?
		    msecs - last_msecs : 0;
		printed = false;

		/*
		 * If a threshold is defined, start filtering events by the
		 * delta of msecs.
		 */
		if (dthres != 0 && (dmsecs > dthres)) {
			/*
			 * Print the previous entry as a landmark, even if it
			 * falls below the threshold.
			 */
			if (last_evtp != NULL && !last_printed)
				BTC_DELTA_PRINT(last_evtp, last_msecs,
				    last_dmsecs);
			BTC_DELTA_PRINT(evtp, msecs, dmsecs);
			printed = true;
		} else if (dthres == 0) {
			BTC_DELTA_PRINT(evtp, msecs, dmsecs);
			printed = true;
		}
		if (first_msecs == 0 || msecs < first_msecs)
			first_msecs = msecs;
		last_evtp = evtp;
		last_msecs = msecs;
		last_dmsecs = dmsecs;
		last_printed = printed;
		maybe_yield();
next:
		i = (i + 1) % btp->size;
	} while (i != curr);

	total_dmsecs = last_msecs > first_msecs ?
	    (last_msecs - first_msecs) : 0;
	if (sbp != NULL)
		sbuf_printf(sbp, "Total measured time: %ju msecs\n",
		    (uintmax_t)total_dmsecs);
	else
		printf("Total measured time: %ju msecs\n",
		    (uintmax_t)total_dmsecs);
}

/*
 * Dump trace table entries to the console, given a delta threshold.
 */
void
boottrace_dump_console(void)
{
	if (!boottrace_enabled)
		return;

	if (shutdown || rebooting || KERNEL_PANICKED()) {
		boottrace_display(NULL, &st, shutdown_trace_threshold);
	} else {
		boottrace_display(NULL, &bt, 0);
		boottrace_display(NULL, &rt, 0);
	}
}

/*
 * Records a new tracing event to the specified table.
 *
 * We don't use a lock because we want this to be callable from interrupt
 * context.
 */
static int
dotrace(struct bt_table *btp, const char *eventname, const char *tdname)
{
	uint32_t idx, nxt;
	struct rusage usage;

	MPASS(boottrace_enabled);
	if (tdname == NULL)
		tdname = (curproc->p_flag & P_SYSTEM) ?
		    curthread->td_name : curproc->p_comm;

	dprintf("dotrace[");
	dprintf("cpu=%u, pid=%d, tsc=%ju, tick=%d, td='%s', event='%s'",
	    PCPU_GET(cpuid), curthread->td_proc->p_pid,
	    (uintmax_t)get_cyclecount(), ticks, tdname, eventname);
	if (btp->table == NULL) {
		btp->drops_early++;
		dprintf(", return=ENOSPC_1]\n");
		return (ENOSPC);
	}

	/* Claim a slot in the table. */
	do {
		idx = btp->curr;
		nxt = (idx + 1) % btp->size;
		if (nxt == 0 && btp->wrap == 0) {
			btp->drops_full++;
			dprintf(", return=ENOSPC_2]\n");
			return (ENOSPC);
		}
	} while (!atomic_cmpset_int(&btp->curr, idx, nxt));

	btp->table[idx].cpuid = PCPU_GET(cpuid);
	btp->table[idx].tsc = get_cyclecount(),
	btp->table[idx].tick = cpu_ticks();
	btp->table[idx].pid = curthread->td_proc->p_pid;

	/*
	 * Get the resource usage for the process. Don't attempt this for the
	 * kernel proc0 or for critical section activities.
	 */
	if ((curthread->td_proc == &proc0) || (curthread->td_critnest != 0)) {
		btp->table[idx].cputime = 0;
		btp->table[idx].inblock = 0;
		btp->table[idx].oublock = 0;
	} else {
		kern_getrusage(curthread, RUSAGE_CHILDREN, &usage);
		btp->table[idx].cputime =
		    (uint64_t)(usage.ru_utime.tv_sec * 1000000 +
		    usage.ru_utime.tv_usec + usage.ru_stime.tv_sec * 1000000 +
		    usage.ru_stime.tv_usec);
		btp->table[idx].inblock = (uint32_t)usage.ru_inblock;
		btp->table[idx].oublock = (uint32_t)usage.ru_oublock;
	}
	strlcpy(btp->table[idx].name, eventname, BT_EVENT_NAMELEN);
	strlcpy(btp->table[idx].tdname, tdname, BT_EVENT_TDNAMELEN);

	dprintf(", return=0]\n");
	return (0);
}

/*
 * Log various boot-time events, with the trace message encoded using
 * printf-like arguments.
 */
int
boottrace(const char *tdname, const char *fmt, ...)
{
	char eventname[BT_EVENT_NAMELEN];
	struct bt_table *btp;
	va_list ap;

	if (!dotrace_kernel)
		 return (ENXIO);

	va_start(ap, fmt);
	vsnprintf(eventname, sizeof(eventname), fmt, ap);
	va_end(ap);

	btp = &bt;
	if (bootdone)
		btp = &rt;
	if (shutdown || rebooting || KERNEL_PANICKED())
		btp = &st;

	return (dotrace(btp, eventname, tdname));
}

/*
 * Log a run-time event & switch over to run-time tracing mode.
 */
static int
runtrace(const char *eventname, const char *tdname)
{

	bootdone = true;
	return (dotrace(&rt, eventname, tdname));
}

/*
 * Parse a boottrace message from userspace.
 *
 * The input from may contain a ':' to denote tdname. If not, tdname is
 * inferred from the process' name.
 *
 * e.g. reboot(8):SIGINT to init(8)
 */
static void
boottrace_parse_message(char *message, char **eventname, char **tdname)
{
	char *delim;

	delim = strchr(message, ':');
	if (delim != NULL) {
		*delim = '\0';
		*tdname = message;
		*eventname = ++delim;
	} else {
		*tdname = curproc->p_comm;
		*eventname = message;
	}
}

static int
_boottrace_sysctl(struct bt_table *btp, struct sysctl_oid *oidp,
    struct sysctl_req *req)
{
	char message[BT_MSGLEN];
	char *eventname, *tdname;
	int error;

	if (!dotrace_user)
	       return (ENXIO);

	message[0] = '\0';
	error = sysctl_handle_string(oidp, message, sizeof(message), req);
	if (error)
		return (error);

	boottrace_parse_message(message, &eventname, &tdname);
	error = dotrace(btp, eventname, tdname);
	if (error == ENOSPC) {
		/* Ignore table full error. */
		error = 0;
	}
	return (error);
}

static int
sysctl_log(SYSCTL_HANDLER_ARGS)
{
	struct sbuf *sbuf;
	int error;

	if (!boottrace_enabled || req->newptr != NULL)
		return (0);

	sbuf = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND);
	boottrace_display(sbuf, &bt, 0);
	boottrace_display(sbuf, &rt, 0);
	sbuf_finish(sbuf);
	error = SYSCTL_OUT(req, sbuf_data(sbuf), sbuf_len(sbuf));
	sbuf_delete(sbuf);

	return (error);
}

static int
sysctl_boottrace(SYSCTL_HANDLER_ARGS)
{

	if (!boottrace_enabled)
		return (0);

	/* No output */
	if (req->newptr == NULL)
		return (0);

	/* Trace to rt if we have reached multi-user. */
	return (_boottrace_sysctl(bootdone ? &rt : &bt, oidp, req));
}

/*
 * Log a run-time event and switch over to run-time tracing mode.
 */
static int
sysctl_runtrace(SYSCTL_HANDLER_ARGS)
{

	if (!boottrace_enabled)
		return (0);

	/* No output */
	if (req->newptr == NULL)
		return (0);

	bootdone = true;
	return (_boottrace_sysctl(&rt, oidp, req));
}

/*
 * Log a shutdown-time event and switch over to shutdown tracing mode.
 */
static int
sysctl_shuttrace(SYSCTL_HANDLER_ARGS)
{

	if (!boottrace_enabled)
		return (0);

	/* No output */
	if (req->newptr == NULL)
		return (0);

	shutdown = true;
	return (_boottrace_sysctl(&st, oidp, req));
}

/*
 * Reset the runtime tracing buffer.
 */
void
boottrace_reset(const char *actor)
{
	char tmpbuf[64];

	snprintf(tmpbuf, sizeof(tmpbuf), "reset: %s", actor);
	runtrace(tmpbuf, NULL);
}

/*
 * Note that a resize implies a reset, i.e., the index is reset to 0.
 * We never shrink the array; we can only increase its size.
 */
int
boottrace_resize(u_int newsize)
{

	if (newsize <= rt.size) {
		return (EINVAL);
	}
	rt.table = realloc(rt.table, newsize * sizeof(struct bt_event),
	    M_BOOTTRACE, M_WAITOK | M_ZERO);
	rt.size = newsize;
	boottrace_reset("boottrace_resize");
	return (0);
}

static int
sysctl_boottrace_reset(SYSCTL_HANDLER_ARGS)
{

	if (!boottrace_enabled)
		return (0);

	if (req->newptr != NULL)
		boottrace_reset("sysctl_boottrace_reset");

	return (0);
}

static void
boottrace_init(void)
{

	if (!boottrace_enabled)
		return;

	/* Boottime trace table */
	bt.size = BT_TABLE_DEFSIZE;
	TUNABLE_INT_FETCH("kern.boottrace.table_size", &bt.size);
	bt.size = max(bt.size, BT_TABLE_MINSIZE);
	bt.table = malloc(bt.size * sizeof(struct bt_event), M_BOOTTRACE,
	    M_WAITOK | M_ZERO);

	/* Stick in an initial entry. */
	bt.table[0].cpuid = PCPU_GET(cpuid);
	strlcpy(bt.table[0].tdname, "boottime", BT_EVENT_TDNAMELEN);
	strlcpy(bt.table[0].name, "initial event", BT_EVENT_NAMELEN);
	bt.curr = 1;

	/* Run-time trace table (may wrap-around). */
	rt.wrap = 1;
	rt.size = BT_TABLE_RUNSIZE;
	rt.table = malloc(rt.size * sizeof(struct bt_event), M_BOOTTRACE,
	    M_WAITOK | M_ZERO);

	/* Shutdown trace table */
	st.size = BT_TABLE_SHTSIZE;
	st.table = malloc(st.size * sizeof(struct bt_event), M_BOOTTRACE,
	    M_WAITOK | M_ZERO);
}
SYSINIT(boottrace, SI_SUB_CPU, SI_ORDER_ANY, boottrace_init, NULL);
