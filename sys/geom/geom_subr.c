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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/devicestat.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/errno.h>
#include <sys/sbuf.h>
#include <geom/geom.h>
#include <geom/geom_int.h>
#include <machine/stdarg.h>

struct class_list_head g_classes = LIST_HEAD_INITIALIZER(g_classes);
static struct g_tailq_head geoms = TAILQ_HEAD_INITIALIZER(geoms);
char *g_wait_event, *g_wait_up, *g_wait_down, *g_wait_sim;


struct g_hh00 {
	struct g_class	*mp;
	int		error;
};

/*
 * This event offers a new class a chance to taste all preexisting providers.
 */
static void
g_load_class(void *arg, int flag)
{
	struct g_hh00 *hh;
	struct g_class *mp2, *mp;
	struct g_geom *gp;
	struct g_provider *pp;

	g_topology_assert();
	if (flag == EV_CANCEL)	/* XXX: can't happen ? */
		return;
	if (g_shutdown)
		return;

	hh = arg;
	mp = hh->mp;
	g_free(hh);
	g_trace(G_T_TOPOLOGY, "g_load_class(%s)", mp->name);
	LIST_FOREACH(mp2, &g_classes, class) {
		KASSERT(mp2 != mp,
		    ("The GEOM class %s already loaded", mp2->name));
		KASSERT(strcmp(mp2->name, mp->name) != 0,
		    ("A GEOM class named %s is already loaded", mp2->name));
	}

	if (mp->init != NULL)
		mp->init(mp);
	LIST_INIT(&mp->geom);
	LIST_INSERT_HEAD(&g_classes, mp, class);
	if (mp->taste == NULL)
		return;
	LIST_FOREACH(mp2, &g_classes, class) {
		if (mp == mp2)
			continue;
		LIST_FOREACH(gp, &mp2->geom, geom) {
			LIST_FOREACH(pp, &gp->provider, provider) {
				mp->taste(mp, pp, 0);
				g_topology_assert();
			}
		}
	}
}

static void
g_unload_class(void *arg, int flag)
{
	struct g_hh00 *hh;
	struct g_class *mp;
	struct g_geom *gp;
	struct g_provider *pp;
	struct g_consumer *cp;
	int error;

	g_topology_assert();
	hh = arg;
	mp = hh->mp;
	g_trace(G_T_TOPOLOGY, "g_unload_class(%s)", mp->name);
	if (mp->destroy_geom == NULL) {
		hh->error = EOPNOTSUPP;
		return;
	}

	/* We refuse to unload if anything is open */
	LIST_FOREACH(gp, &mp->geom, geom) {
		LIST_FOREACH(pp, &gp->provider, provider)
			if (pp->acr || pp->acw || pp->ace) {
				hh->error = EBUSY;
				return;
			}
		LIST_FOREACH(cp, &gp->consumer, consumer)
			if (cp->acr || cp->acw || cp->ace) {
				hh->error = EBUSY;
				return;
			}
	}

	/* Bar new entries */
	mp->taste = NULL;
	mp->config = NULL;

	error = 0;
	LIST_FOREACH(gp, &mp->geom, geom) {
		error = mp->destroy_geom(NULL, mp, gp);
		if (error != 0)
			break;
	}
	if (error == 0) {
		LIST_REMOVE(mp, class);
		if (mp->fini != NULL)
			mp->fini(mp);
	}
	hh->error = error;
	return;
}

int
g_modevent(module_t mod, int type, void *data)
{
	struct g_hh00 *hh;
	int error;
	static int g_ignition;

	if (!g_ignition) {
		g_ignition++;
		g_init();
	}
	hh = g_malloc(sizeof *hh, M_WAITOK | M_ZERO);
	hh->mp = data;
	error = EOPNOTSUPP;
	switch (type) {
	case MOD_LOAD:
		g_trace(G_T_TOPOLOGY, "g_modevent(%s, LOAD)", hh->mp->name);
		g_post_event(g_load_class, hh, M_WAITOK, NULL);
		error = 0;
		break;
	case MOD_UNLOAD:
		g_trace(G_T_TOPOLOGY, "g_modevent(%s, UNLOAD)", hh->mp->name);
		error = g_waitfor_event(g_unload_class, hh, M_WAITOK, NULL);
		if (error == 0)
			error = hh->error;
		if (error == 0) {
			g_waitidle();
			KASSERT(LIST_EMPTY(&hh->mp->geom),
			    ("Unloaded class (%s) still has geom", hh->mp->name));
		}
		g_free(hh);
		break;
	}
	return (error);
}

struct g_geom *
g_new_geomf(struct g_class *mp, const char *fmt, ...)
{
	struct g_geom *gp;
	va_list ap;
	struct sbuf *sb;

	g_topology_assert();
	sb = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND);
	va_start(ap, fmt);
	sbuf_vprintf(sb, fmt, ap);
	va_end(ap);
	sbuf_finish(sb);
	gp = g_malloc(sizeof *gp, M_WAITOK | M_ZERO);
	gp->name = g_malloc(sbuf_len(sb) + 1, M_WAITOK | M_ZERO);
	gp->class = mp;
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
	KASSERT(LIST_EMPTY(&gp->consumer),
	    ("g_destroy_geom(%s) with consumer(s) [%p]",
	    gp->name, LIST_FIRST(&gp->consumer)));
	KASSERT(LIST_EMPTY(&gp->provider),
	    ("g_destroy_geom(%s) with provider(s) [%p]",
	    gp->name, LIST_FIRST(&gp->consumer)));
	g_cancel_event(gp);
	LIST_REMOVE(gp, geom);
	TAILQ_REMOVE(&geoms, gp, geoms);
	g_free(gp->name);
	g_free(gp);
}

/*
 * This function is called (repeatedly) until has withered away.
 */
void
g_wither_geom(struct g_geom *gp, int error)
{
	struct g_provider *pp, *pp2;
	struct g_consumer *cp, *cp2;
	static int once_is_enough;

	if (once_is_enough)
		return;
	once_is_enough = 1;
	g_trace(G_T_TOPOLOGY, "g_wither_geom(%p(%s))", gp, gp->name);
	g_topology_assert();
	if (!(gp->flags & G_GEOM_WITHER)) {
		gp->flags |= G_GEOM_WITHER;
		LIST_FOREACH(pp, &gp->provider, provider)
			g_orphan_provider(pp, error);
	}
	for (pp = LIST_FIRST(&gp->provider); pp != NULL; pp = pp2) {
		pp2 = LIST_NEXT(pp, provider);
		if (!LIST_EMPTY(&pp->consumers))
			continue;
		g_destroy_provider(pp);
	}
	for (cp = LIST_FIRST(&gp->consumer); cp != NULL; cp = cp2) {
		cp2 = LIST_NEXT(cp, consumer);
		if (cp->acr || cp->acw || cp->ace)
			continue;
		g_detach(cp);
		g_destroy_consumer(cp);
	}
	if (LIST_EMPTY(&gp->provider) && LIST_EMPTY(&gp->consumer))
		g_destroy_geom(gp);
	once_is_enough = 0;
}

struct g_consumer *
g_new_consumer(struct g_geom *gp)
{
	struct g_consumer *cp;

	g_topology_assert();
	KASSERT(gp->orphan != NULL,
	    ("g_new_consumer on geom(%s) (class %s) without orphan",
	    gp->name, gp->class->name));

	cp = g_malloc(sizeof *cp, M_WAITOK | M_ZERO);
	cp->geom = gp;
	cp->stat = devstat_new_entry(cp, -1, 0, DEVSTAT_ALL_SUPPORTED,
	    DEVSTAT_TYPE_DIRECT, DEVSTAT_PRIORITY_MAX);
	LIST_INSERT_HEAD(&gp->consumer, cp, consumer);
	return(cp);
}

void
g_destroy_consumer(struct g_consumer *cp)
{
	struct g_geom *gp;

	g_trace(G_T_TOPOLOGY, "g_destroy_consumer(%p)", cp);
	g_topology_assert();
	KASSERT (cp->provider == NULL, ("g_destroy_consumer but attached"));
	KASSERT (cp->acr == 0, ("g_destroy_consumer with acr"));
	KASSERT (cp->acw == 0, ("g_destroy_consumer with acw"));
	KASSERT (cp->ace == 0, ("g_destroy_consumer with ace"));
	g_cancel_event(cp);
	gp = cp->geom;
	LIST_REMOVE(cp, consumer);
	devstat_remove_entry(cp->stat);
	g_free(cp);
	if (gp->flags & G_GEOM_WITHER)
		g_wither_geom(gp, 0);
}

static void
g_new_provider_event(void *arg, int flag)
{
	struct g_class *mp;
	struct g_provider *pp;
	struct g_consumer *cp;
	int i;

	g_topology_assert();
	if (flag == EV_CANCEL)
		return;
	if (g_shutdown)
		return;
	pp = arg;
	LIST_FOREACH(mp, &g_classes, class) {
		if (mp->taste == NULL)
			continue;
		i = 1;
		LIST_FOREACH(cp, &pp->consumers, consumers)
			if (cp->geom->class == mp)
				i = 0;
		if (!i)
			continue;
		mp->taste(mp, pp, 0);
		g_topology_assert();
	}
}


struct g_provider *
g_new_providerf(struct g_geom *gp, const char *fmt, ...)
{
	struct g_provider *pp;
	struct sbuf *sb;
	va_list ap;

	g_topology_assert();
	sb = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND);
	va_start(ap, fmt);
	sbuf_vprintf(sb, fmt, ap);
	va_end(ap);
	sbuf_finish(sb);
	pp = g_malloc(sizeof *pp + sbuf_len(sb) + 1, M_WAITOK | M_ZERO);
	pp->name = (char *)(pp + 1);
	strcpy(pp->name, sbuf_data(sb));
	sbuf_delete(sb);
	LIST_INIT(&pp->consumers);
	pp->error = ENXIO;
	pp->geom = gp;
	pp->stat = devstat_new_entry(pp, -1, 0, DEVSTAT_ALL_SUPPORTED,
	    DEVSTAT_TYPE_DIRECT, DEVSTAT_PRIORITY_MAX);
	LIST_INSERT_HEAD(&gp->provider, pp, provider);
	g_post_event(g_new_provider_event, pp, M_WAITOK, pp, NULL);
	return (pp);
}

void
g_error_provider(struct g_provider *pp, int error)
{

	pp->error = error;
}

struct g_provider *
g_provider_by_name(char const *arg)
{
	struct g_class *cp;
	struct g_geom *gp;
	struct g_provider *pp;

	LIST_FOREACH(cp, &g_classes, class) {
		LIST_FOREACH(gp, &cp->geom, geom) {
			LIST_FOREACH(pp, &gp->provider, provider) {
				if (!strcmp(arg, pp->name))
					return (pp);
			}
		}
	}
	return (NULL);
}

void
g_destroy_provider(struct g_provider *pp)
{
	struct g_geom *gp;

	g_topology_assert();
	KASSERT(LIST_EMPTY(&pp->consumers),
	    ("g_destroy_provider but attached"));
	KASSERT (pp->acr == 0, ("g_destroy_provider with acr"));
	KASSERT (pp->acw == 0, ("g_destroy_provider with acw"));
	KASSERT (pp->acw == 0, ("g_destroy_provider with ace"));
	g_cancel_event(pp);
	LIST_REMOVE(pp, provider);
	gp = pp->geom;
	devstat_remove_entry(pp->stat);
	g_free(pp);
	if ((gp->flags & G_GEOM_WITHER))
		g_wither_geom(gp, 0);
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
 * g_attach() which detach again and calls redo_rank again
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
		if (gp == gp1) 
			return (ELOOP);

		/* no rank, put it at the end move on */
		TAILQ_REMOVE(&geoms, gp1, geoms);
		TAILQ_INSERT_TAIL(&geoms, gp1, geoms);
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
g_detach(struct g_consumer *cp)
{
	struct g_provider *pp;

	g_trace(G_T_TOPOLOGY, "g_detach(%p)", cp);
	KASSERT(cp != (void*)0xd0d0d0d0, ("ARGH!"));
	g_topology_assert();
	KASSERT(cp->provider != NULL, ("detach but not attached"));
	KASSERT(cp->acr == 0, ("detach but nonzero acr"));
	KASSERT(cp->acw == 0, ("detach but nonzero acw"));
	KASSERT(cp->ace == 0, ("detach but nonzero ace"));
	KASSERT(cp->nstart == cp->nend,
	    ("detach with active requests"));
	pp = cp->provider;
	LIST_REMOVE(cp, consumers);
	cp->provider = NULL;
	if (pp->geom->flags & G_GEOM_WITHER)
		g_wither_geom(pp->geom, 0);
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
	KASSERT(pp->geom->access != NULL, ("NULL geom->access"));

	/*
	 * If our class cares about being spoiled, and we have been, we
	 * are probably just ahead of the event telling us that.  Fail
	 * now rather than having to unravel this later.
	 */
	if (cp->geom->spoiled != NULL && cp->spoiled) {
		KASSERT(dcr <= 0, ("spoiled but dcr = %d", dcr));
		KASSERT(dcw <= 0, ("spoiled but dce = %d", dcw));
		KASSERT(dce <= 0, ("spoiled but dcw = %d", dce));
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

	/* If foot-shooting is enabled, any open on rank#1 is OK */
	if ((g_debugflags & 16) && pp->geom->rank == 1)
		;
	/* If we try exclusive but already write: fail */
	else if (dce > 0 && pw > 0)
		return (EPERM);
	/* If we try write but already exclusive: fail */
	else if (dcw > 0 && pe > 0)
		return (EPERM);
	/* If we try to open more but provider is error'ed: fail */
	else if ((dcr > 0 || dcw > 0 || dce > 0) && pp->error != 0)
		return (pp->error);

	/* Ok then... */

	error = pp->geom->access(pp, dcr, dcw, dce);
	if (!error) {
		/*
		 * If we open first write, spoil any partner consumers.
		 * If we close last write, trigger re-taste.
		 */
		if (pp->acw == 0 && dcw != 0)
			g_spoil(pp, cp);
		else if (pp->acw != 0 && pp->acw == -dcw && 
		    !(pp->geom->flags & G_GEOM_WITHER))
			g_post_event(g_new_provider_event, pp, M_WAITOK, 
			    pp, NULL);

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
g_handleattr_int(struct bio *bp, const char *attribute, int val)
{

	return (g_handleattr(bp, attribute, &val, sizeof val));
}

int
g_handleattr_off_t(struct bio *bp, const char *attribute, off_t val)
{

	return (g_handleattr(bp, attribute, &val, sizeof val));
}

int
g_handleattr(struct bio *bp, const char *attribute, void *val, int len)
{
	int error;

	if (strcmp(bp->bio_attribute, attribute))
		return (0);
	if (bp->bio_length != len) {
		printf("bio_length %jd len %d -> EFAULT\n",
		    (intmax_t)bp->bio_length, len);
		error = EFAULT;
	} else {
		error = 0;
		bcopy(val, bp->bio_data, len);
		bp->bio_completed = len;
	}
	g_io_deliver(bp, error);
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

	bp2 = bp->bio_parent;
	if (bp2->bio_error == 0)
		bp2->bio_error = bp->bio_error;
	bp2->bio_completed += bp->bio_completed;
	g_destroy_bio(bp);
	bp2->bio_inbed++;
	if (bp2->bio_children == bp2->bio_inbed)
		g_io_deliver(bp2, bp2->bio_error);
}

/* XXX: maybe this is only g_slice_spoiled */

void
g_std_spoiled(struct g_consumer *cp)
{
	struct g_geom *gp;
	struct g_provider *pp;

	g_trace(G_T_TOPOLOGY, "g_std_spoiled(%p)", cp);
	g_topology_assert();
	g_detach(cp);
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

static void
g_spoil_event(void *arg, int flag)
{
	struct g_provider *pp;
	struct g_consumer *cp, *cp2;

	g_topology_assert();
	if (flag == EV_CANCEL)
		return;
	pp = arg;
	for (cp = LIST_FIRST(&pp->consumers); cp != NULL; cp = cp2) {
		cp2 = LIST_NEXT(cp, consumers);
		if (!cp->spoiled)
			continue;
		cp->spoiled = 0;
		if (cp->geom->spoiled == NULL)
			continue;
		cp->geom->spoiled(cp);
		g_topology_assert();
	}
}

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
	g_post_event(g_spoil_event, pp, M_WAITOK, pp, NULL);
}

int
g_getattr__(const char *attr, struct g_consumer *cp, void *var, int len)
{
	int error, i;

	i = len;
	error = g_io_getattr(attr, cp, &i, var);
	if (error)
		return (error);
	if (i != len)
		return (EINVAL);
	return (0);
}

/*
 * Check if the given pointer is a live object
 */

void
g_sanity(void const *ptr)
{
	struct g_class *mp;
	struct g_geom *gp;
	struct g_consumer *cp;
	struct g_provider *pp;

	if (!(g_debugflags & 0x8))
		return;
	LIST_FOREACH(mp, &g_classes, class) {
		KASSERT(mp != ptr, ("Ptr is live class"));
		LIST_FOREACH(gp, &mp->geom, geom) {
			KASSERT(gp != ptr, ("Ptr is live geom"));
			KASSERT(gp->name != ptr, ("Ptr is live geom's name"));
			LIST_FOREACH(cp, &gp->consumer, consumer) {
				KASSERT(cp != ptr, ("Ptr is live consumer"));
			}
			LIST_FOREACH(pp, &gp->provider, provider) {
				KASSERT(pp != ptr, ("Ptr is live provider"));
			}
		}
	}
}

