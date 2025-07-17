/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2002 Poul-Henning Kamp
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Poul-Henning Kamp
 * and NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/eventhandler.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/unistd.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sbuf.h>
#include <sys/sched.h>
#include <sys/sx.h>
#include <geom/geom.h>
#include <geom/geom_int.h>

MALLOC_DEFINE(M_GEOM, "GEOM", "Geom data structures");

struct sx topology_lock;

static struct proc *g_proc;
struct thread __read_mostly *g_up_td;
struct thread __read_mostly *g_down_td;
static struct thread __read_mostly *g_event_td;

int __read_mostly g_debugflags;
int __read_mostly g_collectstats = G_STATS_PROVIDERS;
int g_shutdown;
int g_notaste;

/*
 * G_UP and G_DOWN are the two threads which push I/O through the
 * stack.
 *
 * Things are procesed in a FIFO order, but these threads could be
 * part of I/O prioritization by deciding which bios/bioqs to service
 * in what order.
 *
 * We have only one thread in each direction, it is believed that until
 * a very non-trivial workload in the UP/DOWN path this will be enough,
 * but more than one can actually be run without problems.
 *
 * Holding the "mymutex" is a debugging feature:  It prevents people
 * from sleeping in the UP/DOWN I/O path by mistake or design (doing
 * so almost invariably result in deadlocks since it stalls all I/O
 * processing in the given direction.
 */

static void
g_up_procbody(void *arg)
{

	thread_lock(g_up_td);
	sched_prio(g_up_td, PRIBIO);
	thread_unlock(g_up_td);
	for(;;) {
		g_io_schedule_up(g_up_td);
	}
}

static void
g_down_procbody(void *arg)
{

	thread_lock(g_down_td);
	sched_prio(g_down_td, PRIBIO);
	thread_unlock(g_down_td);
	for(;;) {
		g_io_schedule_down(g_down_td);
	}
}

static void
g_event_procbody(void *arg)
{

	thread_lock(g_event_td);
	sched_prio(g_event_td, PRIBIO);
	thread_unlock(g_event_td);
	g_run_events();
	/* NOTREACHED */
}

int
g_is_geom_thread(struct thread *td)
{

	return (td == g_up_td || td == g_down_td || td == g_event_td);
}

static void
geom_shutdown(void *foo __unused)
{

	g_shutdown = 1;
}

void
g_init(void)
{

	g_trace(G_T_TOPOLOGY, "g_ignition");
	sx_init(&topology_lock, "GEOM topology");
	g_io_init();
	g_event_init();
	g_ctl_init();
	kproc_kthread_add(g_event_procbody, NULL, &g_proc, &g_event_td,
	    RFHIGHPID, 0, "geom", "g_event");
	kproc_kthread_add(g_up_procbody, NULL, &g_proc, &g_up_td,
	    RFHIGHPID, 0, "geom", "g_up");
	kproc_kthread_add(g_down_procbody, NULL, &g_proc, &g_down_td,
	    RFHIGHPID, 0, "geom", "g_down");
	EVENTHANDLER_REGISTER(shutdown_pre_sync, geom_shutdown, NULL,
		SHUTDOWN_PRI_FIRST);
}

static int
sysctl_kern_geom_confany(struct sysctl_req *req, g_event_t *func, size_t *hint)
{
	size_t len = 0;
	int error = 0;
	struct sbuf *sb;

	if (req->oldptr == NULL) {
		sb = sbuf_new(NULL, NULL, PAGE_SIZE, SBUF_FIXEDLEN |
		    SBUF_INCLUDENUL);
		sbuf_set_drain(sb, sbuf_count_drain, &len);
		g_waitfor_event(func, sb, M_WAITOK, NULL);
		req->oldidx = *hint = len;
	} else {
		sb = sbuf_new(NULL, NULL, *hint, SBUF_AUTOEXTEND |
		    SBUF_INCLUDENUL);
		g_waitfor_event(func, sb, M_WAITOK, NULL);
		*hint = sbuf_len(sb);
		error = SYSCTL_OUT(req, sbuf_data(sb), sbuf_len(sb));
	}
	sbuf_delete(sb);
	return error;
}

static int
sysctl_kern_geom_conftxt(SYSCTL_HANDLER_ARGS)
{
	static size_t hint = PAGE_SIZE;

	return (sysctl_kern_geom_confany(req, g_conftxt, &hint));
}

static int
sysctl_kern_geom_confdot(SYSCTL_HANDLER_ARGS)
{
	static size_t hint = PAGE_SIZE;

	return (sysctl_kern_geom_confany(req, g_confdot, &hint));
}

static int
sysctl_kern_geom_confxml(SYSCTL_HANDLER_ARGS)
{
	static size_t hint = PAGE_SIZE;

	return (sysctl_kern_geom_confany(req, g_confxml, &hint));
}

SYSCTL_NODE(_kern, OID_AUTO, geom, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "GEOMetry management");

SYSCTL_PROC(_kern_geom, OID_AUTO, confxml,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, 0, 0,
    sysctl_kern_geom_confxml, "",
    "Dump the GEOM config in XML");

SYSCTL_PROC(_kern_geom, OID_AUTO, confdot,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, 0, 0,
    sysctl_kern_geom_confdot, "",
    "Dump the GEOM config in dot");

SYSCTL_PROC(_kern_geom, OID_AUTO, conftxt,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, 0, 0,
    sysctl_kern_geom_conftxt, "",
    "Dump the GEOM config in txt");

SYSCTL_INT(_kern_geom, OID_AUTO, debugflags, CTLFLAG_RWTUN,
	&g_debugflags, 0, "Set various trace levels for GEOM debugging");

SYSCTL_INT(_kern_geom, OID_AUTO, notaste, CTLFLAG_RW,
	&g_notaste, 0, "Prevent GEOM tasting");

SYSCTL_INT(_kern_geom, OID_AUTO, collectstats, CTLFLAG_RW,
	&g_collectstats, 0,
	"Control statistics collection on GEOM providers and consumers");

SYSCTL_SIZEOF_STRUCT(g_class);
SYSCTL_SIZEOF_STRUCT(g_geom);
SYSCTL_SIZEOF_STRUCT(g_provider);
SYSCTL_SIZEOF_STRUCT(g_consumer);
SYSCTL_SIZEOF_STRUCT(g_bioq);
