/*-
 * Copyright (c) 2006-2007 Matthew Jacob <mjacob@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Based upon work by Pawel Jakub Dawidek <pjd@FreeBSD.org> for all of the
 * fine geom examples, and by Poul Henning Kamp <phk@FreeBSD.org> for GEOM
 * itself, all of which is most gratefully acknowledged.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/geom/multipath/g_multipath.c,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $");
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bio.h>
#include <sys/sysctl.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <geom/geom.h>
#include <geom/multipath/g_multipath.h>


SYSCTL_DECL(_kern_geom);
SYSCTL_NODE(_kern_geom, OID_AUTO, multipath, CTLFLAG_RW, 0,
    "GEOM_MULTIPATH tunables");
static u_int g_multipath_debug = 0;
SYSCTL_UINT(_kern_geom_multipath, OID_AUTO, debug, CTLFLAG_RW,
    &g_multipath_debug, 0, "Debug level");

static enum {
	GKT_NIL,
	GKT_RUN,
	GKT_DIE
} g_multipath_kt_state;
static struct bio_queue_head gmtbq;
static struct mtx gmtbq_mtx;

static void g_multipath_orphan(struct g_consumer *);
static void g_multipath_start(struct bio *);
static void g_multipath_done(struct bio *);
static void g_multipath_done_error(struct bio *);
static void g_multipath_kt(void *);

static int g_multipath_destroy(struct g_geom *);
static int
g_multipath_destroy_geom(struct gctl_req *, struct g_class *, struct g_geom *);

static g_taste_t g_multipath_taste;
static g_ctl_req_t g_multipath_config;
static g_init_t g_multipath_init;
static g_fini_t g_multipath_fini;

struct g_class g_multipath_class = {
	.name		= G_MULTIPATH_CLASS_NAME,
	.version	= G_VERSION,
	.ctlreq		= g_multipath_config,
	.taste		= g_multipath_taste,
	.destroy_geom	= g_multipath_destroy_geom,
	.init		= g_multipath_init,
	.fini		= g_multipath_fini
};

#define	MP_BAD		0x1
#define	MP_POSTED	0x2

static void
g_mpd(void *arg, int flags __unused)
{
	struct g_consumer *cp;

	g_topology_assert();
	cp = arg;
	if (cp->acr > 0 || cp->acw > 0 || cp->ace > 0) {
		g_access(cp, -cp->acr, -cp->acw, -cp->ace);
	}
	if (cp->provider) {
		printf("GEOM_MULTIPATH: %s removed from %s\n",
		    cp->provider->name, cp->geom->name);
		g_detach(cp);
	}
	g_destroy_consumer(cp);
}

static void
g_multipath_orphan(struct g_consumer *cp)
{
	if ((cp->index & MP_POSTED) == 0) {
		cp->index |= MP_POSTED;
		printf("GEOM_MULTIPATH: %s orphaned in %s\n",
		    cp->provider->name, cp->geom->name);
		g_mpd(cp, 0);
	}
}

static void
g_multipath_start(struct bio *bp)
{
	struct g_multipath_softc *sc;
	struct g_geom *gp;
	struct g_consumer *cp;
	struct bio *cbp;

	gp = bp->bio_to->geom;
	sc = gp->softc;
	KASSERT(sc != NULL, ("NULL sc"));
	cp = sc->cp_active;
	if (cp == NULL) {
		g_io_deliver(bp, ENXIO);
		return;
	}
	cbp = g_clone_bio(bp);
	if (cbp == NULL) {
		g_io_deliver(bp, ENOMEM);
		return;
	}
	cbp->bio_done = g_multipath_done;
	g_io_request(cbp, cp);
}

static void
g_multipath_done(struct bio *bp)
{
	if (bp->bio_error == ENXIO || bp->bio_error == EIO) {
		mtx_lock(&gmtbq_mtx);
		bioq_insert_tail(&gmtbq, bp);
		wakeup(&g_multipath_kt_state);
		mtx_unlock(&gmtbq_mtx);
	} else {
		g_std_done(bp);
	}
}

static void
g_multipath_done_error(struct bio *bp)
{
	struct bio *pbp;
	struct g_geom *gp;
	struct g_multipath_softc *sc;
	struct g_consumer *cp;
	struct g_provider *pp;

	/*
	 * If we had a failure, we have to check first to see
	 * whether the consumer it failed on was the currently
	 * active consumer (i.e., this is the first in perhaps
	 * a number of failures). If so, we then switch consumers
	 * to the next available consumer.
	 */

	g_topology_lock();
	pbp = bp->bio_parent;
	gp = pbp->bio_to->geom;
	sc = gp->softc;
	cp = bp->bio_from;
	pp = cp->provider;

	cp->index |= MP_BAD;
	if (cp->nend == cp->nstart && pp->nend == pp->nstart) {
		cp->index |= MP_POSTED;
		g_post_event(g_mpd, cp, M_NOWAIT, NULL);
	}
	if (cp == sc->cp_active) {
		struct g_consumer *lcp;
		printf("GEOM_MULTIPATH: %s failed in %s\n",
		    pp->name, sc->sc_name);
		sc->cp_active = NULL;
		LIST_FOREACH(lcp, &gp->consumer, consumer) {
			if ((lcp->index & MP_BAD) == 0) {
				sc->cp_active = lcp;
				break;
			}
		}
		if (sc->cp_active == NULL) {
			printf("GEOM_MULTIPATH: out of providers for %s\n",
			    sc->sc_name);
			return;
		} else {
			printf("GEOM_MULTIPATH: %s now active path in %s\n",
			    sc->cp_active->provider->name, sc->sc_name);
		}
	}
	g_topology_unlock();

	/*
	 * If we can fruitfully restart the I/O, do so.
	 */
	if (sc->cp_active) {
		g_destroy_bio(bp);
		pbp->bio_children--;
		g_multipath_start(pbp);
	} else {
		g_std_done(bp);
	}
}

static void
g_multipath_kt(void *arg)
{
	g_multipath_kt_state = GKT_RUN;
	mtx_lock(&gmtbq_mtx);
	while (g_multipath_kt_state == GKT_RUN) {
		for (;;) {
			struct bio *bp;
			bp = bioq_takefirst(&gmtbq);
			if (bp == NULL) {
				break;
			}
			mtx_unlock(&gmtbq_mtx);
			g_multipath_done_error(bp);
			mtx_lock(&gmtbq_mtx);
		}
		msleep(&g_multipath_kt_state, &gmtbq_mtx, PRIBIO,
		    "gkt:wait", hz / 10);
	}
	mtx_unlock(&gmtbq_mtx);
	wakeup(&g_multipath_kt_state);
	kthread_exit(0);
}


static int
g_multipath_access(struct g_provider *pp, int dr, int dw, int de)
{
	struct g_geom *gp;
	struct g_consumer *cp, *badcp = NULL;
	int error;

	gp = pp->geom;

	LIST_FOREACH(cp, &gp->consumer, consumer) {
		error = g_access(cp, dr, dw, de);
		if (error) {
			badcp = cp;
			goto fail;
		}
	}
	return (0);

fail:
	LIST_FOREACH(cp, &gp->consumer, consumer) {
		if (cp == badcp) {
			break;
		}
		(void) g_access(cp, -dr, -dw, -de);
	}
	return (error);
}

static struct g_geom *
g_multipath_create(struct g_class *mp, struct g_multipath_metadata *md)
{
	struct g_multipath_softc *sc;
	struct g_geom *gp;
	struct g_provider *pp;

	g_topology_assert();

	LIST_FOREACH(gp, &mp->geom, geom) {
		if (strcmp(gp->name, md->md_name) == 0) {
			printf("GEOM_MULTIPATH: name %s already exists\n",
			    md->md_name);
			return (NULL);
		}
	}

	gp = g_new_geomf(mp, md->md_name);
	if (gp == NULL) {
		goto fail;
	}

	sc = g_malloc(sizeof(*sc), M_WAITOK | M_ZERO);
	if (sc == NULL) {
		goto fail;
	}

	gp->softc = sc;
	gp->start = g_multipath_start;
	gp->orphan = g_multipath_orphan;
	gp->access = g_multipath_access;
	memcpy(sc->sc_uuid, md->md_uuid, sizeof (sc->sc_uuid));
	memcpy(sc->sc_name, md->md_name, sizeof (sc->sc_name));

	pp = g_new_providerf(gp, "multipath/%s", md->md_name);
	if (pp == NULL) {
		goto fail;
	}
	/* limit the provider to not have it stomp on metadata */
	pp->mediasize = md->md_size - md->md_sectorsize;
	pp->sectorsize = md->md_sectorsize;
	sc->pp = pp;
	g_error_provider(pp, 0);
	return (gp);
fail:
	if (gp != NULL) {
		if (gp->softc != NULL) {
			g_free(gp->softc);
		}
		g_destroy_geom(gp);
	}
	return (NULL);
}

static int
g_multipath_add_disk(struct g_geom *gp, struct g_provider *pp)
{
	struct g_multipath_softc *sc;
	struct g_consumer *cp, *nxtcp;
	int error;

	g_topology_assert();

	sc = gp->softc;
	KASSERT(sc, ("no softc"));

	/*
	 * Make sure that the passed provider isn't already attached
	 */
	LIST_FOREACH(cp, &gp->consumer, consumer) {
		if (cp->provider == pp) {
			break;
		}
	}
	if (cp) {
		printf("GEOM_MULTIPATH: provider %s already attached to %s\n",
		    pp->name, gp->name);
		return (EEXIST);
	}
	nxtcp = LIST_FIRST(&gp->consumer);
	cp = g_new_consumer(gp);
	if (cp == NULL) {
		return (ENOMEM);
	}
	error = g_attach(cp, pp);
	if (error != 0) {
		printf("GEOM_MULTIPATH: cannot attach %s to %s",
		    pp->name, sc->sc_name);
		g_destroy_consumer(cp);
		return (error);
	}
	cp->private = sc;
	cp->index = 0;

	/*
	 * Set access permissions on new consumer to match other consumers
	 */
	if (nxtcp && (nxtcp->acr + nxtcp->acw +  nxtcp->ace)) {
		error = g_access(cp, nxtcp->acr, nxtcp->acw, nxtcp->ace);
		if (error) {
			printf("GEOM_MULTIPATH: cannot set access in "
			    "attaching %s to %s/%s (%d)\n",
			    pp->name, sc->sc_name, sc->sc_uuid, error);
			g_detach(cp);
			g_destroy_consumer(cp);
			return (error);
		}
	}
	printf("GEOM_MULTIPATH: adding %s to %s/%s\n",
	    pp->name, sc->sc_name, sc->sc_uuid);
	if (sc->cp_active == NULL) {
		sc->cp_active = cp;
		printf("GEOM_MULTIPATH: %s now active path in %s\n",
		    pp->name, sc->sc_name);
	}
	return (0);
}

static int
g_multipath_destroy(struct g_geom *gp)
{
	struct g_provider *pp;

	g_topology_assert();
	if (gp->softc == NULL) {
		return (ENXIO);
	}
	pp = LIST_FIRST(&gp->provider);
	if (pp != NULL && (pp->acr != 0 || pp->acw != 0 || pp->ace != 0)) {
		return (EBUSY);
	}
	printf("GEOM_MULTIPATH: destroying %s\n", gp->name);
	g_free(gp->softc);
	gp->softc = NULL;
	g_wither_geom(gp, ENXIO);
	return (0);
}

static int
g_multipath_destroy_geom(struct gctl_req *req, struct g_class *mp,
    struct g_geom *gp)
{
	return (g_multipath_destroy(gp));
}

static void
g_multipath_init(struct g_class *mp)
{
	bioq_init(&gmtbq);
	mtx_init(&gmtbq_mtx, "gmtbq", NULL, MTX_DEF);
	if (kthread_create(g_multipath_kt, mp, NULL, 0, 0, "g_mp_kt") == 0) {
		g_multipath_kt_state = GKT_RUN;
	}
}

static void
g_multipath_fini(struct g_class *mp)
{
	if (g_multipath_kt_state == GKT_RUN) {
		mtx_lock(&gmtbq_mtx);
		g_multipath_kt_state = GKT_DIE;
		wakeup(&g_multipath_kt_state);
		msleep(&g_multipath_kt_state, &gmtbq_mtx, PRIBIO,
		    "gmp:fini", 0);
		mtx_unlock(&gmtbq_mtx);
	}
}

static int
g_multipath_read_metadata(struct g_consumer *cp,
    struct g_multipath_metadata *md)
{
	struct g_provider *pp;
	u_char *buf;
	int error;

	g_topology_assert();
	error = g_access(cp, 1, 0, 0);
	if (error != 0) {
		return (error);
	}
	pp = cp->provider;
	g_topology_unlock();
	buf = g_read_data(cp, pp->mediasize - pp->sectorsize,
	    pp->sectorsize, &error);
	g_topology_lock();
	g_access(cp, -1, 0, 0);
	if (buf == NULL) {
		return (error);
	}
	multipath_metadata_decode(buf, md);
	g_free(buf);
	return (0);
}

static struct g_geom *
g_multipath_taste(struct g_class *mp, struct g_provider *pp, int flags __unused)
{
	struct g_multipath_metadata md;
	struct g_multipath_softc *sc;
	struct g_consumer *cp;
	struct g_geom *gp, *gp1;
	int error, isnew;

	g_topology_assert();

	gp = g_new_geomf(mp, "multipath:taste");
	gp->start = g_multipath_start;
	gp->access = g_multipath_access;
	gp->orphan = g_multipath_orphan;
	cp = g_new_consumer(gp);
	g_attach(cp, pp);
	error = g_multipath_read_metadata(cp, &md);
	g_detach(cp);
	g_destroy_consumer(cp);
	g_destroy_geom(gp);
	if (error != 0) {
		return (NULL);
	}
	gp = NULL;

	if (strcmp(md.md_magic, G_MULTIPATH_MAGIC) != 0) {
		if (g_multipath_debug) {
			printf("%s is not MULTIPATH\n", pp->name);
		}
		return (NULL);
	}
	if (md.md_version != G_MULTIPATH_VERSION) {
		printf("%s has version %d multipath id- this module is version "
		    " %d: rejecting\n", pp->name, md.md_version,
		    G_MULTIPATH_VERSION);
		return (NULL);
	}
	if (g_multipath_debug) {
		printf("MULTIPATH: %s/%s\n", md.md_name, md.md_uuid);
	}

	/*
	 * Let's check if such a device already is present. We check against
	 * uuid alone first because that's the true distinguishor. If that
	 * passes, then we check for name conflicts. If there are conflicts, 
	 * modify the name.
	 *
	 * The whole purpose of this is to solve the problem that people don't
	 * pick good unique names, but good unique names (like uuids) are a
	 * pain to use. So, we allow people to build GEOMs with friendly names
	 * and uuids, and modify the names in case there's a collision.
	 */
	sc = NULL;
	LIST_FOREACH(gp, &mp->geom, geom) {
		sc = gp->softc;
		if (sc == NULL) {
			continue;
		}
		if (strncmp(md.md_uuid, sc->sc_uuid, sizeof(md.md_uuid)) == 0) {
			break;
		}
	}

	LIST_FOREACH(gp1, &mp->geom, geom) {
		if (gp1 == gp) {
			continue;
		}
		sc = gp1->softc;
		if (sc == NULL) {
			continue;
		}
		if (strncmp(md.md_name, sc->sc_name, sizeof(md.md_name)) == 0) {
			break;
		}
	}

	/*
	 * If gp is NULL, we had no extant MULTIPATH geom with this uuid.
	 *
	 * If gp1 is *not* NULL, that means we have a MULTIPATH geom extant
	 * with the same name (but a different UUID).
	 *
	 * If gp is NULL, then modify the name with a random number and
  	 * complain, but allow the creation of the geom to continue.
	 *
	 * If gp is *not* NULL, just use the geom's name as we're attaching
	 * this disk to the (previously generated) name.
	 */

	if (gp1) {
		sc = gp1->softc;
		if (gp == NULL) {
			char buf[16];
			u_long rand = random();

			snprintf(buf, sizeof (buf), "%s-%lu", md.md_name, rand);
			printf("GEOM_MULTIPATH: geom %s/%s exists already\n",
			    sc->sc_name, sc->sc_uuid);
			printf("GEOM_MULTIPATH: %s will be (temporarily) %s\n",
			    md.md_uuid, buf);
			strlcpy(md.md_name, buf, sizeof (md.md_name));
		} else {
			strlcpy(md.md_name, sc->sc_name, sizeof (md.md_name));
		}
	}

	if (gp == NULL) {
		gp = g_multipath_create(mp, &md);
		if (gp == NULL) {
			printf("GEOM_MULTIPATH: cannot create geom %s/%s\n",
			    md.md_name, md.md_uuid);
			return (NULL);
		}
		isnew = 1;
	} else {
		isnew = 0;
	}

	sc = gp->softc;
	KASSERT(sc != NULL, ("sc is NULL"));
	error = g_multipath_add_disk(gp, pp);
	if (error != 0) {
		if (isnew) {
			g_multipath_destroy(gp);
		}
		return (NULL);
	}
	return (gp);
}

static void
g_multipath_ctl_create(struct gctl_req *req, struct g_class *mp)
{
	struct g_geom *gp;
	struct g_provider *pp0, *pp1;
	struct g_multipath_metadata md;
	const char *name, *mpname, *uuid;
	static const char devpf[6] = "/dev/";
	int *nargs, error;

	g_topology_assert();

	mpname = gctl_get_asciiparam(req, "arg0");
        if (mpname == NULL) {
                gctl_error(req, "No 'arg0' argument");
                return;
        }

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No 'nargs' argument");
		return;
	}
	if (*nargs != 4) {
		gctl_error(req, "missing device or uuid arguments");
		return;
	}

	name = gctl_get_asciiparam(req, "arg1");
	if (name == NULL) {
		gctl_error(req, "No 'arg1' argument");
		return;
	}
	if (strncmp(name, devpf, 5) == 0) {
		name += 5;
	}
	pp0 = g_provider_by_name(name);
	if (pp0 == NULL) {
		gctl_error(req, "Provider %s is invalid", name);
		return;
	}

	name = gctl_get_asciiparam(req, "arg2");
	if (name == NULL) {
		gctl_error(req, "No 'arg2' argument");
		return;
	}
	if (strncmp(name, devpf, 5) == 0) {
		name += 5;
	}
	pp1 = g_provider_by_name(name);
	if (pp1 == NULL) {
		gctl_error(req, "Provider %s is invalid", name);
		return;
	}

	uuid = gctl_get_asciiparam(req, "arg3");
	if (uuid == NULL) {
		gctl_error(req, "No uuid argument");
		return;
	}
	if (strlen(uuid) != 36) {
		gctl_error(req, "Malformed uuid argument");
		return;
	}

	/*
	 * Check to make sure parameters from the two providers are the same
	 */
	if (pp0 == pp1) {
		gctl_error(req, "providers %s and %s are the same",
		    pp0->name, pp1->name);
		return;
	}
    	if (pp0->mediasize != pp1->mediasize) {
		gctl_error(req, "Provider %s is %jd; Provider %s is %jd",
		    pp0->name, (intmax_t) pp0->mediasize,
		    pp1->name, (intmax_t) pp1->mediasize);
		return;
	}
    	if (pp0->sectorsize != pp1->sectorsize) {
		gctl_error(req, "Provider %s has sectorsize %u; Provider %s "
		    "has sectorsize %u", pp0->name, pp0->sectorsize,
		    pp1->name, pp1->sectorsize);
		return;
	}

	/*
	 * cons up enough of a metadata structure to use.
	 */
	memset(&md, 0, sizeof(md));
	md.md_size = pp0->mediasize;
	md.md_sectorsize = pp0->sectorsize;
	strncpy(md.md_name, mpname, sizeof (md.md_name));
	strncpy(md.md_uuid, uuid, sizeof (md.md_uuid));

	gp = g_multipath_create(mp, &md);
	if (gp == NULL) {
		return;
	}
	error = g_multipath_add_disk(gp, pp0);
	if (error) {
		g_multipath_destroy(gp);
		return;
	}
	error = g_multipath_add_disk(gp, pp1);
	if (error) {
		g_multipath_destroy(gp);
		return;
	}
}

static struct g_geom *
g_multipath_find_geom(struct g_class *mp, const char *name)
{
	struct g_geom *gp;

	LIST_FOREACH(gp, &mp->geom, geom) {
		if (strcmp(gp->name, name) == 0) {
			return (gp);
		}
	}
	return (NULL);
}

static void
g_multipath_ctl_destroy(struct gctl_req *req, struct g_class *mp)
{
	struct g_geom *gp;
	const char *name;
	int error;

	g_topology_assert();

	name = gctl_get_asciiparam(req, "arg0");
        if (name == NULL) {
                gctl_error(req, "No 'arg0' argument");
                return;
        }
	gp = g_multipath_find_geom(mp, name);
	if (gp == NULL) {
		gctl_error(req, "Device %s is invalid", name);
		return;
	}
	error = g_multipath_destroy(gp);
	if (error != 0) {
		gctl_error(req, "failed to destroy %s (err=%d)", name, error);
	}
}

static void
g_multipath_config(struct gctl_req *req, struct g_class *mp, const char *verb)
{
	uint32_t *version;
	g_topology_assert();
	version = gctl_get_paraml(req, "version", sizeof(*version));
	if (version == NULL) {
		gctl_error(req, "No 'version' argument");
	} else if (*version != G_MULTIPATH_VERSION) {
		gctl_error(req, "Userland and kernel parts are out of sync");
	} else if (strcmp(verb, "create") == 0) {
		g_multipath_ctl_create(req, mp);
	} else if (strcmp(verb, "destroy") == 0) {
		g_multipath_ctl_destroy(req, mp);
	} else {
		gctl_error(req, "Unknown verb %s", verb);
	}
}
DECLARE_GEOM_CLASS(g_multipath_class, g_multipath);
