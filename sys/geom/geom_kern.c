/*-
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/sbuf.h>
#include <geom/geom.h>
#include <geom/geom_int.h>

MALLOC_DEFINE(M_GEOM, "GEOM", "Geom data structures");

struct sx topology_lock;

static struct proc *g_up_proc;

int g_debugflags;

/*
 * G_UP and G_DOWN are the two threads which push I/O through the
 * stack.
 *
 * Things are procesed in a FIFO order, but these threads could be
 * part of I/O prioritization by deciding which bios/bioqs to service
 * in what order.
 *
 * We have only one thread in each direction, it is belived that until
 * a very non-trivial workload in the UP/DOWN path this will be enough,
 * but more than one can actually be run without problems.
 *
 * Holding the "mymutex" is a debugging feature:  It prevents people
 * from sleeping in the UP/DOWN I/O path by mistake or design (doing
 * so almost invariably result in deadlocks since it stalls all I/O
 * processing in the given direction.
 */

static void
g_up_procbody(void)
{
	struct proc *p = g_up_proc;
	struct thread *tp = FIRST_THREAD_IN_PROC(p);
	struct mtx mymutex;

	bzero(&mymutex, sizeof mymutex);
	mtx_init(&mymutex, "g_up", MTX_DEF, 0);
	mtx_lock(&mymutex);
	tp->td_base_pri = PRIBIO;
	for(;;) {
		g_io_schedule_up(tp);
		msleep(&g_wait_up, &mymutex, PRIBIO, "g_up", hz/10);
	}
}

struct kproc_desc g_up_kp = {
	"g_up",
	g_up_procbody,
	&g_up_proc,
};

static struct proc *g_down_proc;

static void
g_down_procbody(void)
{
	struct proc *p = g_down_proc;
	struct thread *tp = FIRST_THREAD_IN_PROC(p);
	struct mtx mymutex;

	bzero(&mymutex, sizeof mymutex);
	mtx_init(&mymutex, "g_down", MTX_DEF, 0);
	mtx_lock(&mymutex);
	tp->td_base_pri = PRIBIO;
	for(;;) {
		g_io_schedule_down(tp);
		msleep(&g_wait_down, &mymutex, PRIBIO, "g_down", hz/10);
	}
}

struct kproc_desc g_down_kp = {
	"g_down",
	g_down_procbody,
	&g_down_proc,
};

static struct proc *g_event_proc;

static void
g_event_procbody(void)
{
	struct proc *p = g_down_proc;
	struct thread *tp = FIRST_THREAD_IN_PROC(p);

	tp->td_base_pri = PRIBIO;
	for(;;) {
		g_run_events();
		tsleep(&g_wait_event, PRIBIO, "g_events", hz/10);
	}
}

struct kproc_desc g_event_kp = {
	"g_event",
	g_event_procbody,
	&g_event_proc,
};

void
g_init(void)
{
	printf("Initializing GEOMetry subsystem\n");
	if (bootverbose)
		g_debugflags |= G_T_TOPOLOGY;
	sx_init(&topology_lock, "GEOM topology");
	g_io_init();
	g_event_init();
	mtx_lock(&Giant);
	kproc_start(&g_event_kp);
	kproc_start(&g_up_kp);
	kproc_start(&g_down_kp);
	mtx_unlock(&Giant);
}

static int
sysctl_kern_geom_dotconf(SYSCTL_HANDLER_ARGS)
{
	int i, error;
	struct sbuf *sb;

	i = 0;
	sb = g_confdot();
	error = sysctl_handle_opaque(oidp, sbuf_data(sb), sbuf_len(sb), req);
	sbuf_delete(sb);
	return (error);
}

static int
sysctl_kern_geom_xmlconf(SYSCTL_HANDLER_ARGS)
{
	int i, error;
	struct sbuf *sb;

	i = 0;
	sb = g_conf();
	error = sysctl_handle_opaque(oidp, sbuf_data(sb), sbuf_len(sb), req);
	sbuf_delete(sb);
	return (error);
}

SYSCTL_NODE(_kern, OID_AUTO, geom, CTLFLAG_RW, 0, "GEOMetry management");

SYSCTL_PROC(_kern_geom, OID_AUTO, xmlconf, CTLTYPE_STRING|CTLFLAG_RD,
	0, 0, sysctl_kern_geom_xmlconf, "A",
	"Dump the GEOM config");

SYSCTL_PROC(_kern_geom, OID_AUTO, dotconf, CTLTYPE_STRING|CTLFLAG_RD,
	0, 0, sysctl_kern_geom_dotconf, "A",
	"Dump the GEOM config");

SYSCTL_INT(_kern_geom, OID_AUTO, debugflags, CTLTYPE_INT|CTLFLAG_RW,
	&g_debugflags, 0, "");

SYSCTL_INT(_debug_sizeof, OID_AUTO, g_class, CTLTYPE_INT|CTLFLAG_RD,
	0, sizeof(struct g_class), "");
SYSCTL_INT(_debug_sizeof, OID_AUTO, g_geom, CTLTYPE_INT|CTLFLAG_RD,
	0, sizeof(struct g_geom), "");
SYSCTL_INT(_debug_sizeof, OID_AUTO, g_provider, CTLTYPE_INT|CTLFLAG_RD,
	0, sizeof(struct g_provider), "");
SYSCTL_INT(_debug_sizeof, OID_AUTO, g_consumer, CTLTYPE_INT|CTLFLAG_RD,
	0, sizeof(struct g_consumer), "");
SYSCTL_INT(_debug_sizeof, OID_AUTO, g_bioq, CTLTYPE_INT|CTLFLAG_RD,
	0, sizeof(struct g_bioq), "");
SYSCTL_INT(_debug_sizeof, OID_AUTO, g_event, CTLTYPE_INT|CTLFLAG_RD,
	0, sizeof(struct g_event), "");
