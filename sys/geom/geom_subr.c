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
#ifndef _KERNEL
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <err.h>
#else
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#endif
#include <sys/errno.h>
#include <sys/sbuf.h>
#include <geom/geom.h>
#include <machine/stdarg.h>

struct method_list_head g_methods = LIST_HEAD_INITIALIZER(g_methods);
static struct g_tailq_head geoms = TAILQ_HEAD_INITIALIZER(geoms);
static int g_nproviders;
char *g_wait_event, *g_wait_up, *g_wait_down, *g_wait_sim;

static int g_ignition;

void
g_add_method(struct g_method *mp)
{

	if (!g_ignition) {
		g_ignition++;
		g_init();
	}
	g_topology_lock();
	g_trace(G_T_TOPOLOGY, "g_add_method(%s)", mp->name);
	LIST_INIT(&mp->geom);
	LIST_INSERT_HEAD(&g_methods, mp, method);
	if (g_nproviders > 0)
		g_post_event(EV_NEW_METHOD, mp, NULL, NULL, NULL);
	g_topology_unlock();
}

struct g_geom *
g_new_geomf(struct g_method *mp, char *fmt, ...)
{
	struct g_geom *gp;
	va_list ap;
	struct sbuf *sb;

	g_topology_assert();
	va_start(ap, fmt);
	mtx_lock(&Giant);
	sb = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND);
	sbuf_vprintf(sb, fmt, ap);
	sbuf_finish(sb);
	mtx_unlock(&Giant);
	gp = g_malloc(sizeof *gp + sbuf_len(sb) + 1, M_WAITOK | M_ZERO);
	gp->name = (char *)(gp + 1);
	gp->method = mp;
	gp->rank = 1;
	LIST_INIT(&gp->consumer);
	LIST_INIT(&gp->provider);
	LIST_INSERT_HEAD(&mp->geom, gp, geom);
	TAILQ_INSERT_HEAD(&geoms, gp, geoms);
	strcpy(gp->name, sbuf_data(sb));
	sbuf_delete(sb);
	return (gp);
}

void
g_destroy_geom(struct g_geom *gp)
{

	g_trace(G_T_TOPOLOGY, "g_destroy_geom(%p(%s))", gp, gp->name);
	g_topology_assert();
	KASSERT(gp->event == NULL, ("g_destroy_geom() with event"));
	KASSERT(LIST_EMPTY(&gp->consumer),
	    ("g_destroy_geom(%s) with consumer(s) [%p]",
	    gp->name, LIST_FIRST(&gp->consumer)));
	KASSERT(LIST_EMPTY(&gp->provider),
	    ("g_destroy_geom(%s) with provider(s) [%p]",
	    gp->name, LIST_FIRST(&gp->consumer)));
	LIST_REMOVE(gp, geom);
	TAILQ_REMOVE(&geoms, gp, geoms);
	g_free(gp);
}

struct g_consumer *
g_new_consumer(struct g_geom *gp)
{
	struct g_consumer *cp;

	g_topology_assert();
	KASSERT(gp->method->orphan != NULL,
	    ("g_new_consumer on method(%s) without orphan", gp->method->name));

	cp = g_malloc(sizeof *cp, M_WAITOK | M_ZERO);
	cp->geom = gp;
	LIST_INSERT_HEAD(&gp->consumer, cp, consumer);
	return(cp);
}

void
g_destroy_consumer(struct g_consumer *cp)
{

	g_trace(G_T_TOPOLOGY, "g_destroy_consumer(%p)", cp);
	g_topology_assert();
	KASSERT(cp->event == NULL, ("g_destroy_consumer() with event"));
	KASSERT (cp->provider == NULL, ("g_destroy_consumer but attached"));
	KASSERT (cp->acr == 0, ("g_destroy_consumer with acr"));
	KASSERT (cp->acw == 0, ("g_destroy_consumer with acw"));
	KASSERT (cp->ace == 0, ("g_destroy_consumer with ace"));
	LIST_REMOVE(cp, consumer);
	g_free(cp);
}

struct g_provider *
g_new_providerf(struct g_geom *gp, char *fmt, ...)
{
	struct g_provider *pp;
	struct sbuf *sb;
	va_list ap;

	g_topology_assert();
	va_start(ap, fmt);
	mtx_lock(&Giant);
	sb = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND);
	sbuf_vprintf(sb, fmt, ap);
	sbuf_finish(sb);
	mtx_unlock(&Giant);
	pp = g_malloc(sizeof *pp + sbuf_len(sb) + 1, M_WAITOK | M_ZERO);
	pp->name = (char *)(pp + 1);
	strcpy(pp->name, sbuf_data(sb));
	sbuf_delete(sb);
	LIST_INIT(&pp->consumers);
	pp->error = ENXIO;
	pp->geom = gp;
	LIST_INSERT_HEAD(&gp->provider, pp, provider);
	g_nproviders++;
	g_post_event(EV_NEW_PROVIDER, NULL, NULL, pp, NULL);
	return (pp);
}

void
g_error_provider(struct g_provider *pp, int error)
{

	pp->error = error;
}


void
g_destroy_provider(struct g_provider *pp)
{
	struct g_geom *gp;
	struct g_consumer *cp;

	g_topology_assert();
	KASSERT(pp->event == NULL, ("g_destroy_provider() with event"));
	KASSERT(LIST_EMPTY(&pp->consumers),
	    ("g_destroy_provider but attached"));
	KASSERT (pp->acr == 0, ("g_destroy_provider with acr"));
	KASSERT (pp->acw == 0, ("g_destroy_provider with acw"));
	KASSERT (pp->acw == 0, ("g_destroy_provider with ace"));
	g_nproviders--;
	LIST_REMOVE(pp, provider);
	gp = pp->geom;
	g_free(pp);
	if (!(gp->flags & G_GEOM_WITHER))
		return;
	if (!LIST_EMPTY(&gp->provider))
		return;
	for (;;) {
		cp = LIST_FIRST(&gp->consumer);
		if (cp == NULL)
			break;
		g_dettach(cp);
		g_destroy_consumer(cp);
	}
	g_destroy_geom(gp);
}

/*
 * We keep the "geoms" list sorted by topological order (== increasing
 * numerical rank) at all times.
 * When an attach is done, the attaching geoms rank is invalidated
 * and it is moved to the tail of the list.
 * All geoms later in the sequence has their ranks reevaluated in
 * sequence.  If we cannot assign rank to a geom because it's
 * prerequisites do not have rank, we move that element to the tail
 * of the sequence with invalid rank as well.
 * At some point we encounter our original geom and if we stil fail
 * to assign it a rank, there must be a loop and we fail back to
 * g_attach() which dettach again and calls redo_rank again
 * to fix up the damage.
 * It would be much simpler code wise to do it recursively, but we
 * can't risk that on the kernel stack.
 */

static int
redo_rank(struct g_geom *gp)
{
	struct g_consumer *cp;
	struct g_geom *gp1, *gp2;
	int n, m;

	g_topology_assert();

	/* Invalidate this geoms rank and move it to the tail */
	gp1 = TAILQ_NEXT(gp, geoms);
	if (gp1 != NULL) {
		gp->rank = 0;
		TAILQ_REMOVE(&geoms, gp, geoms);
		TAILQ_INSERT_TAIL(&geoms, gp, geoms);
	} else {
		gp1 = gp;
	}

	/* re-rank the rest of the sequence */
	for (; gp1 != NULL; gp1 = gp2) {
		gp1->rank = 0;
		m = 1;
		LIST_FOREACH(cp, &gp1->consumer, consumer) {
			if (cp->provider == NULL)
				continue;
			n = cp->provider->geom->rank;
			if (n == 0) {
				m = 0;
				break;
			} else if (n >= m)
				m = n + 1;
		}
		gp1->rank = m;
		gp2 = TAILQ_NEXT(gp1, geoms);

		/* got a rank, moving on */
		if (m != 0)
			continue;

		/* no rank to original geom means loop */
		if (gp == gp1) {
			return (ELOOP);

		/* no rank, put it at the end move on */
		TAILQ_REMOVE(&geoms, gp1, geoms);
		TAILQ_INSERT_TAIL(&geoms, gp1, geoms);
		}
	}
	return (0);
}

int
g_attach(struct g_consumer *cp, struct g_provider *pp)
{
	int error;

	g_topology_assert();
	KASSERT(cp->provider == NULL, ("attach but attached"));
	cp->provider = pp;
	LIST_INSERT_HEAD(&pp->consumers, cp, consumers);
	error = redo_rank(cp->geom);
	if (error) {
		LIST_REMOVE(cp, consumers);
		cp->provider = NULL;
		redo_rank(cp->geom);
	}
	return (error);
}

void
g_dettach(struct g_consumer *cp)
{
	struct g_provider *pp;

	g_trace(G_T_TOPOLOGY, "g_dettach(%p)", cp);
	KASSERT(cp != (void*)0xd0d0d0d0, ("ARGH!"));
	g_topology_assert();
	KASSERT(cp->provider != NULL, ("dettach but not attached"));
	KASSERT(cp->acr == 0, ("dettach but nonzero acr"));
	KASSERT(cp->acw == 0, ("dettach but nonzero acw"));
	KASSERT(cp->ace == 0, ("dettach but nonzero ace"));
	KASSERT(cp->biocount == 0, ("dettach but nonzero biocount"));
	pp = cp->provider;
	LIST_REMOVE(cp, consumers);
	cp->provider = NULL;
	if (LIST_EMPTY(&pp->consumers)) {
		if (pp->geom->flags & G_GEOM_WITHER)
			g_destroy_provider(pp);
	}
	redo_rank(cp->geom);
}


/*
 * g_access_abs()
 *
 * Access-check with absolute new values:  Just fall through
 * and use the relative version.
 */
int
g_access_abs(struct g_consumer *cp, int acr, int acw, int ace)
{

	g_topology_assert();
	return(g_access_rel(cp,
		acr - cp->acr,
		acw - cp->acw,
		ace - cp->ace));
}

/*
 * g_access_rel()
 *
 * Access-check with delta values.  The question asked is "can provider
 * "cp" change the access counters by the relative amounts dc[rwe] ?"
 */

int
g_access_rel(struct g_consumer *cp, int dcr, int dcw, int dce)
{
	struct g_provider *pp;
	int pr,pw,pe;
	int error;

	pp = cp->provider;

	g_trace(G_T_ACCESS, "g_access_rel(%p(%s), %d, %d, %d)",
	    cp, pp->name, dcr, dcw, dce);

	g_topology_assert();
	KASSERT(cp->provider != NULL, ("access but not attached"));
	KASSERT(cp->acr + dcr >= 0, ("access resulting in negative acr"));
	KASSERT(cp->acw + dcw >= 0, ("access resulting in negative acw"));
	KASSERT(cp->ace + dce >= 0, ("access resulting in negative ace"));
	KASSERT(pp->geom->method->access != NULL, ("NULL method->access"));

	/*
	 * If our method cares about being spoiled, and we have been, we
	 * are probably just ahead of the event telling us that.  Fail
	 * now rather than having to unravel this later.
	 */
	if (cp->geom->spoiled != NULL && cp->spoiled) {
		KASSERT(dcr >= 0, ("spoiled but dcr = %d", dcr));
		KASSERT(dcw >= 0, ("spoiled but dce = %d", dcw));
		KASSERT(dce >= 0, ("spoiled but dcw = %d", dce));
		KASSERT(cp->acr == 0, ("spoiled but cp->acr = %d", cp->acr));
		KASSERT(cp->acw == 0, ("spoiled but cp->acw = %d", cp->acw));
		KASSERT(cp->ace == 0, ("spoiled but cp->ace = %d", cp->ace));
		return(ENXIO);
	}

	/*
	 * Figure out what counts the provider would have had, if this
	 * consumer had (r0w0e0) at this time.
	 */
	pr = pp->acr - cp->acr;
	pw = pp->acw - cp->acw;
	pe = pp->ace - cp->ace;

	g_trace(G_T_ACCESS,
    "open delta:[r%dw%de%d] old:[r%dw%de%d] provider:[r%dw%de%d] %p(%s)",
	    dcr, dcw, dce,
	    cp->acr, cp->acw, cp->ace,
	    pp->acr, pp->acw, pp->ace,
	    pp, pp->name);

	/* If we try exclusive but already write: fail */
	if (dce > 0 && pw > 0)
		return (EPERM);
	/* If we try write but already exclusive: fail */
	if (dcw > 0 && pe > 0)
		return (EPERM);
	/* If we try to open more but provider is error'ed: fail */
	if ((dcr > 0 || dcw > 0 || dce > 0) && pp->error != 0)
		return (pp->error);

	/* Ok then... */

	/*
	 * If we open first write, spoil any partner consumers.
	 * If we close last write, trigger re-taste.
	 */
	if (pp->acw == 0 && dcw != 0)
		g_spoil(pp, cp);
	else if (pp->acw != 0 && pp->acw == -dcw && !(pp->geom->flags & G_GEOM_WITHER))
		g_post_event(EV_NEW_PROVIDER, NULL, NULL, pp, NULL);

	error = pp->geom->method->access(pp, dcr, dcw, dce);
	if (!error) {
		pp->acr += dcr;
		pp->acw += dcw;
		pp->ace += dce;
		cp->acr += dcr;
		cp->acw += dcw;
		cp->ace += dce;
	}
	return (error);
}

int
g_haveattr_int(struct bio *bp, char *attribute, int val)
{

	return (g_haveattr(bp, attribute, &val, sizeof val));
}

int
g_haveattr_off_t(struct bio *bp, char *attribute, off_t val)
{

	return (g_haveattr(bp, attribute, &val, sizeof val));
}


int
g_haveattr(struct bio *bp, char *attribute, void *val, int len)
{
	int error;

	if (strcmp(bp->bio_attribute, attribute))
		return (0);
	if (bp->bio_length != len) {
		printf("bio_length %lld len %d -> EFAULT\n", bp->bio_length, len);
		error = EFAULT;
	} else {
		error = 0;
		bcopy(val, bp->bio_data, len);
		bp->bio_completed = len;
	}
	bp->bio_error = error;
	g_io_deliver(bp);
	return (1);
}

int
g_std_access(struct g_provider *pp __unused,
	int dr __unused, int dw __unused, int de __unused)
{

        return (0);
}

void
g_std_done(struct bio *bp)
{
	struct bio *bp2;

	bp2 = bp->bio_linkage;
	bp2->bio_error = bp->bio_error;
	bp2->bio_completed = bp->bio_completed;
	g_destroy_bio(bp);
	g_io_deliver(bp2);
}

/* XXX: maybe this is only g_slice_spoiled */

void
g_std_spoiled(struct g_consumer *cp)
{
	struct g_geom *gp;
	struct g_provider *pp;

	g_trace(G_T_TOPOLOGY, "g_std_spoiled(%p)", cp);
	g_topology_assert();
	g_dettach(cp);
	gp = cp->geom;
	LIST_FOREACH(pp, &gp->provider, provider)
		g_orphan_provider(pp, ENXIO);
	g_destroy_consumer(cp);
	if (LIST_EMPTY(&gp->provider) && LIST_EMPTY(&gp->consumer))
		g_destroy_geom(gp);
	else
		gp->flags |= G_GEOM_WITHER;
}

/*
 * Spoiling happens when a provider is opened for writing, but consumers
 * which are configured by in-band data are attached (slicers for instance).
 * Since the write might potentially change the in-band data, such consumers
 * need to re-evaluate their existence after the writing session closes.
 * We do this by (offering to) tear them down when the open for write happens
 * in return for a re-taste when it closes again.
 * Together with the fact that such consumers grab an 'e' bit whenever they
 * are open, regardless of mode, this ends up DTRT.
 */

void
g_spoil(struct g_provider *pp, struct g_consumer *cp)
{
	struct g_consumer *cp2;

	g_topology_assert();

	LIST_FOREACH(cp2, &pp->consumers, consumers) {
		if (cp2 == cp)
			continue;
/*
		KASSERT(cp2->acr == 0, ("spoiling cp->acr = %d", cp2->acr));
		KASSERT(cp2->acw == 0, ("spoiling cp->acw = %d", cp2->acw));
*/
		KASSERT(cp2->ace == 0, ("spoiling cp->ace = %d", cp2->ace));
		cp2->spoiled++;
	}
	g_post_event(EV_SPOILED, NULL, NULL, pp, cp);
}

static struct g_method *
g_method_by_name(char *name)
{
	struct g_method *mp;

	g_trace(G_T_TOPOLOGY, "g_method_by_name(%s)", name);
	g_topology_assert();
	LIST_FOREACH(mp, &g_methods, method)
		if (!strcmp(mp->name, name))
			return (mp);
	return (NULL);
}

struct g_geom *
g_create_geomf(char *method, struct g_provider *pp, char *fmt, ...)
{
	va_list ap;
	struct sbuf *sb;
	char *s;
	struct g_method *mp;
	struct g_geom *gp;

	g_trace(G_T_TOPOLOGY, "g_create_geom(%s, %p(%s))", method,
		pp, pp == NULL ? "" : pp->name);
	g_topology_assert();
	gp = NULL;
	mp = g_method_by_name(method);
	if (mp == NULL)
		return (NULL);
	if (fmt != NULL) {
		va_start(ap, fmt);
		mtx_lock(&Giant);
		sb = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND);
		sbuf_vprintf(sb, fmt, ap);
		sbuf_finish(sb);
		mtx_unlock(&Giant);
		s = sbuf_data(sb);
	} else {
		s = NULL;
	}
	if (pp != NULL)
		gp = mp->taste(mp, pp, NULL, G_TF_INSIST);
	if (gp == NULL && mp->create_geom == NULL)
		return (NULL);
	if (gp == NULL)
		gp = mp->create_geom(mp, pp, s);
	/* XXX: delete sbuf  */
	return (gp);
}

struct g_geom *
g_insert_geom(char *method, struct g_consumer *cp)
{
	struct g_method *mp;
	struct g_geom *gp;
	struct g_provider *pp, *pp2;
	struct g_consumer *cp2;
	int error;

	g_trace(G_T_TOPOLOGY, "g_insert_geomf(%s, %p)", method, cp);
	g_topology_assert();
	KASSERT(cp->provider != NULL, ("g_insert_geomf but not attached"));
	/* XXX: check for events ?? */
	mp = g_method_by_name(method);
	if (mp == NULL)
		return (NULL);
	if (mp->create_geom == NULL)
		return (NULL);
	pp = cp->provider;
	gp = mp->taste(mp, pp, NULL, G_TF_TRANSPARENT);
	if (gp == NULL)
		return (NULL);
	pp2 = LIST_FIRST(&gp->provider);
	cp2 = LIST_FIRST(&gp->consumer);
	cp2->acr += pp->acr;
	cp2->acw += pp->acw;
	cp2->ace += pp->ace;
	pp2->acr += pp->acr;
	pp2->acw += pp->acw;
	pp2->ace += pp->ace;
	LIST_REMOVE(cp, consumers);
	LIST_INSERT_HEAD(&pp2->consumers, cp, consumers);
	cp->provider = pp2;
	error = redo_rank(gp);
	KASSERT(error == 0, ("redo_rank failed in g_insert_geom"));
	return (gp);
}

