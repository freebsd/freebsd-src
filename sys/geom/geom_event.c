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
#ifndef _KERNEL
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <err.h>
#else
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#endif
#include <sys/errno.h>
#include <sys/time.h>
#include <geom/geom.h>

static struct event_tailq_head g_events = TAILQ_HEAD_INITIALIZER(g_events);
static u_int g_pending_events, g_silence_events;
static void g_do_event(struct g_event *ep, struct thread *tp);
static TAILQ_HEAD(,g_provider) g_doorstep = TAILQ_HEAD_INITIALIZER(g_doorstep);
static struct mtx g_doorlock;

void
g_silence(void)
{

	g_silence_events = 1;
}

void
g_rattle(void)
{

	g_silence_events = 0;
	mtx_lock(&Giant);
	wakeup(&g_silence_events);
	while (g_pending_events)
		tsleep(&g_pending_events, PPAUSE, "g_rattle", hz/5);
	mtx_unlock(&Giant);
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
	mtx_lock(&g_doorlock);
	TAILQ_INSERT_TAIL(&g_doorstep, pp, orphan);
	mtx_unlock(&g_doorlock);
	mtx_lock(&Giant);
	wakeup(&g_wait_event);
	mtx_unlock(&Giant);
}

/*
 * This function is called once on each provider which the event handler
 * finds on its g_doorstep.
 */

static void
g_orphan_register(struct g_provider *pp, struct thread *tp)
{
	struct g_consumer *cp, *cp2;

	g_trace(G_T_TOPOLOGY, "g_orphan_register(%s)", pp->name);
	g_topology_assert();

	/*
	 * Tell all consumers the bad news.
	 * Don't get surprised if they self-destruct.
	 */
	cp = LIST_FIRST(&pp->consumers);
	while (cp != NULL) {
		cp2 = LIST_NEXT(cp, consumers);
		KASSERT(cp->geom->method->orphan != NULL,
		    ("method %s has no orphan, geom %s",
		    cp->geom->method->name, cp->geom->name));
		cp->geom->method->orphan(cp, tp);
		cp = cp2;
	}
}

static void
g_destroy_event(struct g_event *ep)
{

	g_free(ep);
}

static void
g_do_event(struct g_event *ep, struct thread *tp)
{
	struct g_method *mp, *mp2;
	struct g_geom *gp;
	struct g_consumer *cp, *cp2;
	struct g_provider *pp;
	int i;

	g_trace(G_T_TOPOLOGY, "g_do_event(%p) %d m:%p g:%p p:%p c:%p - ",
	    ep, ep->event, ep->method, ep->geom, ep->provider, ep->consumer);
	g_topology_assert();
	switch (ep->event) {
	case EV_NEW_METHOD:
		mp2 = ep->method;
		if (mp2->taste == NULL)
			break;
		LIST_FOREACH(mp, &g_methods, method) {
			if (mp2 == mp)
				continue;
			LIST_FOREACH(gp, &mp->geom, geom) {
				LIST_FOREACH(pp, &gp->provider, provider) {
					mp2->taste(ep->method, pp, tp, 0);
					g_topology_assert();
				}
			}
		}
		break;
	case EV_NEW_PROVIDER:
		g_trace(G_T_TOPOLOGY, "EV_NEW_PROVIDER(%s)",
		    ep->provider->name);
		LIST_FOREACH(mp, &g_methods, method) {
			if (mp->taste == NULL)
				continue;
			i = 1;
			LIST_FOREACH(cp, &ep->provider->consumers, consumers)
				if(cp->geom->method == mp)
					i = 0;
			if (i) {
				mp->taste(mp, ep->provider, tp, 0);
				g_topology_assert();
			}
		}
		break;
	case EV_SPOILED:
		g_trace(G_T_TOPOLOGY, "EV_SPOILED(%p(%s),%p)",
		    ep->provider, ep->provider->name, ep->consumer);
		cp = LIST_FIRST(&ep->provider->consumers);
		while (cp != NULL) {
			cp2 = LIST_NEXT(cp, consumers);
			if (cp->spoiled) {
				g_trace(G_T_TOPOLOGY, "spoiling %p (%s) (%p)",
				    cp, cp->geom->name, cp->geom->spoiled);
				if (cp->geom->spoiled != NULL)
					cp->geom->spoiled(cp);
				cp->spoiled = 0;
			}
			cp = cp2;
		}
		break;
	case EV_LAST:
	default:
		KASSERT(1 == 0, ("Unknown event %d", ep->event));
	}
}

static int
one_event(struct thread *tp)
{
	struct g_event *ep;
	struct g_provider *pp;

	g_topology_lock();
	for (;;) {
		mtx_lock(&g_doorlock);
		pp = TAILQ_FIRST(&g_doorstep);
		if (pp != NULL)
			TAILQ_REMOVE(&g_doorstep, pp, orphan);
		mtx_unlock(&g_doorlock);
		if (pp == NULL)
			break;
		g_orphan_register(pp, tp);
	}
	ep = TAILQ_FIRST(&g_events);
	if (ep == NULL) {
		g_topology_unlock();
		return (0);
	}
	TAILQ_REMOVE(&g_events, ep, events);
	if (ep->method != NULL)
		ep->method->event = NULL;
	if (ep->geom != NULL)
		ep->geom->event = NULL;
	if (ep->provider != NULL)
		ep->provider->event = NULL;
	if (ep->consumer != NULL)
		ep->consumer->event = NULL;
	g_do_event(ep, tp);
	g_pending_events--;
	if (g_pending_events == 0) {
		mtx_lock(&Giant);
		wakeup(&g_pending_events);
		mtx_unlock(&Giant);
	}
	g_topology_unlock();
	g_destroy_event(ep);
	return (1);
}

void
g_run_events(struct thread *tp)
{

	while (one_event(tp))
		;
}

void
g_post_event(enum g_events ev, struct g_method *mp, struct g_geom *gp, struct g_provider *pp, struct g_consumer *cp)
{
	struct g_event *ep;

	g_trace(G_T_TOPOLOGY, "g_post_event(%d, %p, %p, %p, %p)",
	    ev, mp, gp, pp, cp);
	g_topology_assert();
	ep = g_malloc(sizeof *ep, M_WAITOK | M_ZERO);
	ep->event = ev;
	if (mp != NULL) {
		ep->method = mp;
		KASSERT(mp->event == NULL, ("Double event on method"));
		mp->event = ep;
	}
	if (gp != NULL) {
		ep->geom = gp;
		KASSERT(gp->event == NULL, ("Double event on geom"));
		gp->event = ep;
	}
	if (pp != NULL) {
		ep->provider = pp;
		KASSERT(pp->event == NULL, ("Double event on provider"));
		pp->event = ep;
	}
	if (cp != NULL) {
		ep->consumer = cp;
		KASSERT(cp->event == NULL, ("Double event on consumer"));
		cp->event = ep;
	}
	g_pending_events++;
	TAILQ_INSERT_TAIL(&g_events, ep, events);
	mtx_lock(&Giant);
	wakeup(&g_wait_event);
	mtx_unlock(&Giant);
}

void
g_event_init()
{

	mtx_init(&g_doorlock, "GEOM orphanage", MTX_DEF);
}
