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

/*
 * XXX: How do we in general know that objects referenced in events
 * have not been destroyed before we get around to handle the event ?
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <machine/stdarg.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <geom/geom.h>
#include <geom/geom_int.h>

static struct event_tailq_head g_events = TAILQ_HEAD_INITIALIZER(g_events);
static u_int g_pending_events;
static void g_do_event(struct g_event *ep);
static TAILQ_HEAD(,g_provider) g_doorstep = TAILQ_HEAD_INITIALIZER(g_doorstep);
static struct mtx g_eventlock;
static struct sx g_eventstall;

void
g_waitidle(void)
{

	while (g_pending_events)
		tsleep(&g_pending_events, PPAUSE, "g_waitidle", hz/5);
}

void
g_stall_events(void)
{

	sx_xlock(&g_eventstall);
}

void
g_release_events(void)
{

	sx_xunlock(&g_eventstall);
}

void
g_orphan_provider(struct g_provider *pp, int error)
{

	g_trace(G_T_TOPOLOGY, "g_orphan_provider(%p(%s), %d)",
	    pp, pp->name, error);
	KASSERT(error != 0,
	    ("g_orphan_provider(%p(%s), 0) error must be non-zero\n",
	     pp, pp->name));
	pp->error = error;
	mtx_lock(&g_eventlock);
	TAILQ_INSERT_TAIL(&g_doorstep, pp, orphan);
	mtx_unlock(&g_eventlock);
	wakeup(&g_wait_event);
}

/*
 * This function is called once on each provider which the event handler
 * finds on its g_doorstep.
 */

static void
g_orphan_register(struct g_provider *pp)
{
	struct g_consumer *cp, *cp2;

	g_trace(G_T_TOPOLOGY, "g_orphan_register(%s)", pp->name);
	g_topology_assert();

	/*
	 * Tell all consumers the bad news.
	 * Don't be surprised if they self-destruct.
	 */
	cp = LIST_FIRST(&pp->consumers);
	while (cp != NULL) {
		cp2 = LIST_NEXT(cp, consumers);
		KASSERT(cp->geom->orphan != NULL,
		    ("geom %s has no orphan, class %s",
		    cp->geom->name, cp->geom->class->name));
		cp->geom->orphan(cp);
		cp = cp2;
	}
#ifdef notyet
	cp = LIST_FIRST(&pp->consumers);
	if (cp != NULL)
		return;
	if (pp->geom->flags & G_GEOM_WITHER)
		g_destroy_provider(pp);
#endif
}

static void
g_destroy_event(struct g_event *ep)
{

	g_free(ep);
}

static void
g_do_event(struct g_event *ep)
{
	struct g_class *mp;
	struct g_consumer *cp;
	struct g_provider *pp;
	int i;

	g_trace(G_T_TOPOLOGY, "g_do_event(%p) %d - ", ep, ep->event);
	g_topology_assert();
	switch (ep->event) {
	case EV_CALL_ME:
		ep->func(ep->arg, 0);
		g_topology_assert();
		break;	
	case EV_NEW_PROVIDER:
		if (g_shutdown)
			break;
		pp = ep->ref[0];
		g_trace(G_T_TOPOLOGY, "EV_NEW_PROVIDER(%s)", pp->name);
		LIST_FOREACH(mp, &g_classes, class) {
			if (mp->taste == NULL)
				continue;
			i = 1;
			LIST_FOREACH(cp, &pp->consumers, consumers)
				if(cp->geom->class == mp)
					i = 0;
			if (i) {
				mp->taste(mp, pp, 0);
				g_topology_assert();
			}
		}
		break;
	case EV_LAST:
	default:
		KASSERT(1 == 0, ("Unknown event %d", ep->event));
	}
}

static int
one_event(void)
{
	struct g_event *ep;
	struct g_provider *pp;

	sx_xlock(&g_eventstall);
	g_topology_lock();
	for (;;) {
		mtx_lock(&g_eventlock);
		pp = TAILQ_FIRST(&g_doorstep);
		if (pp != NULL)
			TAILQ_REMOVE(&g_doorstep, pp, orphan);
		mtx_unlock(&g_eventlock);
		if (pp == NULL)
			break;
		g_orphan_register(pp);
	}
	mtx_lock(&g_eventlock);
	ep = TAILQ_FIRST(&g_events);
	if (ep == NULL) {
		mtx_unlock(&g_eventlock);
		g_topology_unlock();
		sx_xunlock(&g_eventstall);
		return (0);
	}
	TAILQ_REMOVE(&g_events, ep, events);
	mtx_unlock(&g_eventlock);
	g_do_event(ep);
	g_destroy_event(ep);
	g_pending_events--;
	if (g_pending_events == 0)
		wakeup(&g_pending_events);
	g_topology_unlock();
	sx_xunlock(&g_eventstall);
	return (1);
}

void
g_run_events()
{

	while (one_event())
		;
}

void
g_post_event(enum g_events ev, ...)
{
	struct g_event *ep;
	va_list ap;
	void *p;
	int n;

	g_trace(G_T_TOPOLOGY, "g_post_event(%d)", ev);
	g_topology_assert();
	ep = g_malloc(sizeof *ep, M_WAITOK | M_ZERO);
	ep->event = ev;
	va_start(ap, ev);
	for (n = 0; n < G_N_EVENTREFS; n++) {
		p = va_arg(ap, void *);
		if (p == NULL)
			break;
		g_trace(G_T_TOPOLOGY, "  ref %p", p);
		ep->ref[n] = p;
	}
	va_end(ap);
	KASSERT(p == NULL, ("Too many references to event"));
	mtx_lock(&g_eventlock);
	g_pending_events++;
	TAILQ_INSERT_TAIL(&g_events, ep, events);
	mtx_unlock(&g_eventlock);
	wakeup(&g_wait_event);
}

void
g_cancel_event(void *ref)
{
	struct g_event *ep, *epn;
	u_int n;

	mtx_lock(&g_eventlock);
	for (ep = TAILQ_FIRST(&g_events); ep != NULL; ep = epn) {
		epn = TAILQ_NEXT(ep, events);
		for (n = 0; n < G_N_EVENTREFS; n++) {
			if (ep->ref[n] == NULL)
				break;
			if (ep->ref[n] == ref) {
				TAILQ_REMOVE(&g_events, ep, events);
				if (ep->event == EV_CALL_ME)
					ep->func(ep->arg, EV_CANCEL);
				g_free(ep);
				break;
			}
		}
	}
	mtx_unlock(&g_eventlock);
}

int
g_call_me(g_call_me_t *func, void *arg, ...)
{
	struct g_event *ep;
	va_list ap;
	void *p;
	u_int n;

	g_trace(G_T_TOPOLOGY, "g_call_me(%p, %p", func, arg);
	ep = g_malloc(sizeof *ep, M_NOWAIT | M_ZERO);
	if (ep == NULL)
		return (ENOMEM);
	va_start(ap, arg);
	for (n = 0; n < G_N_EVENTREFS; n++) {
		p = va_arg(ap, void *);
		if (p == NULL)
			break;
		g_trace(G_T_TOPOLOGY, "  ref %p", p);
		ep->ref[n++] = p;
	}
	va_end(ap);
	KASSERT(p == NULL, ("Too many references to event"));
	ep->event = EV_CALL_ME;
	ep->func = func;
	ep->arg = arg;
	mtx_lock(&g_eventlock);
	g_pending_events++;
	TAILQ_INSERT_TAIL(&g_events, ep, events);
	mtx_unlock(&g_eventlock);
	wakeup(&g_wait_event);
	return (0);
}


void
g_event_init()
{

	mtx_init(&g_eventlock, "GEOM orphanage", NULL, MTX_DEF);
	sx_init(&g_eventstall, "GEOM event stalling");
}
