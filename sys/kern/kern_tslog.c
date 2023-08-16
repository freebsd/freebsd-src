/*-
 * Copyright (c) 2017 Colin Percival
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
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/tslog.h>

#include <machine/atomic.h>
#include <machine/cpu.h>

#ifndef TSLOGSIZE
#define TSLOGSIZE 262144
#endif

static volatile long nrecs = 0;
static struct timestamp {
	void * td;
	int type;
	const char * f;
	const char * s;
	uint64_t tsc;
} timestamps[TSLOGSIZE];

void
tslog(void * td, int type, const char * f, const char * s)
{
	uint64_t tsc = get_cyclecount();
	long pos;

	/* A NULL thread is thread0 before curthread is set. */
	if (td == NULL)
		td = &thread0;

	/* Grab a slot. */
	pos = atomic_fetchadd_long(&nrecs, 1);

	/* Store record. */
	if (pos < nitems(timestamps)) {
		timestamps[pos].td = td;
		timestamps[pos].type = type;
		timestamps[pos].f = f;
		timestamps[pos].s = s;
		timestamps[pos].tsc = tsc;
	}
}

static int
sysctl_debug_tslog(SYSCTL_HANDLER_ARGS)
{
	int error;
	struct sbuf *sb;
	size_t i, limit;
	caddr_t loader_tslog;
	void * loader_tslog_buf;
	size_t loader_tslog_len;

	/*
	 * This code can race against the code in tslog() which stores
	 * records: Theoretically we could end up reading a record after
	 * its slots have been reserved but before it has been written.
	 * Since this code takes orders of magnitude longer to run than
	 * tslog() takes to write a record, it is highly unlikely that
	 * anyone will ever experience this race.
	 */
	sb = sbuf_new_for_sysctl(NULL, NULL, 1024, req);

	/* Get data from the boot loader, if it provided any. */
	loader_tslog = preload_search_by_type("TSLOG data");
	if (loader_tslog != NULL) {
		loader_tslog_buf = preload_fetch_addr(loader_tslog);
		loader_tslog_len = preload_fetch_size(loader_tslog);
		sbuf_bcat(sb, loader_tslog_buf, loader_tslog_len);
	}

	/* Add data logged within the kernel. */
	limit = MIN(nrecs, nitems(timestamps));
	for (i = 0; i < limit; i++) {
		sbuf_printf(sb, "%p", timestamps[i].td);
		sbuf_printf(sb, " %llu",
		    (unsigned long long)timestamps[i].tsc);
		switch (timestamps[i].type) {
		case TS_ENTER:
			sbuf_printf(sb, " ENTER");
			break;
		case TS_EXIT:
			sbuf_printf(sb, " EXIT");
			break;
		case TS_THREAD:
			sbuf_printf(sb, " THREAD");
			break;
		case TS_EVENT:
			sbuf_printf(sb, " EVENT");
			break;
		}
		sbuf_printf(sb, " %s", timestamps[i].f ? timestamps[i].f : "(null)");
		if (timestamps[i].s)
			sbuf_printf(sb, " %s\n", timestamps[i].s);
		else
			sbuf_printf(sb, "\n");
	}
	error = sbuf_finish(sb);
	sbuf_delete(sb);
	return (error);
}

SYSCTL_PROC(_debug, OID_AUTO, tslog,
    CTLTYPE_STRING|CTLFLAG_RD|CTLFLAG_MPSAFE|CTLFLAG_SKIP,
    0, 0, sysctl_debug_tslog, "", "Dump recorded event timestamps");

MALLOC_DEFINE(M_TSLOGUSER, "tsloguser", "Strings used by userland tslog");
static struct procdata {
	pid_t ppid;
	uint64_t tsc_forked;
	uint64_t tsc_exited;
	char * execname;
	char * namei;
	int reused;
} procs[PID_MAX + 1];

void
tslog_user(pid_t pid, pid_t ppid, const char * execname, const char * namei)
{
	uint64_t tsc = get_cyclecount();

	/* If we wrapped, do nothing. */
	if (procs[pid].reused)
		return;

	/* If we have a ppid, we're recording a fork. */
	if (ppid != (pid_t)(-1)) {
		/* If we have a ppid already, we wrapped. */
		if (procs[pid].ppid) {
			procs[pid].reused = 1;
			return;
		}

		/* Fill in some fields. */
		procs[pid].ppid = ppid;
		procs[pid].tsc_forked = tsc;
		return;
	}

	/* If we have an execname, record it. */
	if (execname != NULL) {
		if (procs[pid].execname != NULL)
			free(procs[pid].execname, M_TSLOGUSER);
		procs[pid].execname = strdup(execname, M_TSLOGUSER);
		return;
	}

	/* Record the first namei for the process. */
	if (namei != NULL) {
		if (procs[pid].namei == NULL)
			procs[pid].namei = strdup(namei, M_TSLOGUSER);
		return;
	}

	/* Otherwise we're recording an exit. */
	procs[pid].tsc_exited = tsc;
}

static int
sysctl_debug_tslog_user(SYSCTL_HANDLER_ARGS)
{
	int error;
	struct sbuf *sb;
	pid_t pid;

	sb = sbuf_new_for_sysctl(NULL, NULL, 1024, req);

	/* Export the data we logged. */
	for (pid = 0; pid <= PID_MAX; pid++) {
		sbuf_printf(sb, "%zu", (size_t)pid);
		sbuf_printf(sb, " %zu", (size_t)procs[pid].ppid);
		sbuf_printf(sb, " %llu",
		    (unsigned long long)procs[pid].tsc_forked);
		sbuf_printf(sb, " %llu",
		    (unsigned long long)procs[pid].tsc_exited);
		sbuf_printf(sb, " \"%s\"", procs[pid].execname ?
		    procs[pid].execname : "");
		sbuf_printf(sb, " \"%s\"", procs[pid].namei ?
		    procs[pid].namei : "");
		sbuf_printf(sb, "\n");
	}
	error = sbuf_finish(sb);
	sbuf_delete(sb);
	return (error);
}

SYSCTL_PROC(_debug, OID_AUTO, tslog_user,
    CTLTYPE_STRING|CTLFLAG_RD|CTLFLAG_MPSAFE|CTLFLAG_SKIP,
    0, 0, sysctl_debug_tslog_user,
    "", "Dump recorded userland event timestamps");
