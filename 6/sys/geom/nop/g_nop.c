/*-
 * Copyright (c) 2004 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bio.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <geom/geom.h>
#include <geom/nop/g_nop.h>


SYSCTL_DECL(_kern_geom);
SYSCTL_NODE(_kern_geom, OID_AUTO, nop, CTLFLAG_RW, 0, "GEOM_NOP stuff");
static u_int g_nop_debug = 0;
SYSCTL_UINT(_kern_geom_nop, OID_AUTO, debug, CTLFLAG_RW, &g_nop_debug, 0,
    "Debug level");

static int g_nop_destroy(struct g_geom *gp, boolean_t force);
static int g_nop_destroy_geom(struct gctl_req *req, struct g_class *mp,
    struct g_geom *gp);
static void g_nop_config(struct gctl_req *req, struct g_class *mp,
    const char *verb);
static void g_nop_dumpconf(struct sbuf *sb, const char *indent,
    struct g_geom *gp, struct g_consumer *cp, struct g_provider *pp);

struct g_class g_nop_class = {
	.name = G_NOP_CLASS_NAME,
	.version = G_VERSION,
	.ctlreq = g_nop_config,
	.destroy_geom = g_nop_destroy_geom
};


static void
g_nop_orphan(struct g_consumer *cp)
{

	g_topology_assert();
	g_nop_destroy(cp->geom, 1);
}

static void
g_nop_start(struct bio *bp)
{
	struct g_nop_softc *sc;
	struct g_geom *gp;
	struct g_provider *pp;
	struct bio *cbp;

	gp = bp->bio_to->geom;
	sc = gp->softc;
	G_NOP_LOGREQ(bp, "Request received.");
	switch (bp->bio_cmd) {
	case BIO_READ:
		sc->sc_reads++;
		sc->sc_readbytes += bp->bio_length;
		break;
	case BIO_WRITE:
		sc->sc_writes++;
		sc->sc_wrotebytes += bp->bio_length;
		break;
	}
	if (sc->sc_failprob > 0) {
		u_int rval;

		rval = arc4random() % 100;
		if (rval < sc->sc_failprob) {
			G_NOP_LOGREQ(bp, "Returning EIO.");
			g_io_deliver(bp, EIO);
			return;
		}
	}
	cbp = g_clone_bio(bp);
	if (cbp == NULL) {
		g_io_deliver(bp, ENOMEM);
		return;
	}
	cbp->bio_done = g_std_done;
	cbp->bio_offset = bp->bio_offset + sc->sc_offset;
	cbp->bio_data = bp->bio_data;
	cbp->bio_length = bp->bio_length;
	pp = LIST_FIRST(&gp->provider);
	KASSERT(pp != NULL, ("NULL pp"));
	cbp->bio_to = pp;
	G_NOP_LOGREQ(cbp, "Sending request.");
	g_io_request(cbp, LIST_FIRST(&gp->consumer));
}

static int
g_nop_access(struct g_provider *pp, int dr, int dw, int de)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	int error;

	gp = pp->geom;
	cp = LIST_FIRST(&gp->consumer);
	error = g_access(cp, dr, dw, de);

	return (error);
}

static int
g_nop_create(struct gctl_req *req, struct g_class *mp, struct g_provider *pp,
    u_int failprob, off_t offset, off_t size, u_int secsize)
{
	struct g_nop_softc *sc;
	struct g_geom *gp;
	struct g_provider *newpp;
	struct g_consumer *cp;
	char name[64];
	int error;

	g_topology_assert();

	gp = NULL;
	newpp = NULL;
	cp = NULL;

	if ((offset % pp->sectorsize) != 0) {
		gctl_error(req, "Invalid offset for provider %s.", pp->name);
		return (EINVAL);
	}
	if ((size % pp->sectorsize) != 0) {
		gctl_error(req, "Invalid size for provider %s.", pp->name);
		return (EINVAL);
	}
	if (offset >= pp->mediasize) {
		gctl_error(req, "Invalid offset for provider %s.", pp->name);
		return (EINVAL);
	}
	if (size == 0)
		size = pp->mediasize - offset;
	if (offset + size > pp->mediasize) {
		gctl_error(req, "Invalid size for provider %s.", pp->name);
		return (EINVAL);
	}
	if (secsize == 0)
		secsize = pp->sectorsize;
	else if ((secsize % pp->sectorsize) != 0) {
		gctl_error(req, "Invalid secsize for provider %s.", pp->name);
		return (EINVAL);
	}
	snprintf(name, sizeof(name), "%s%s", pp->name, G_NOP_SUFFIX);
	LIST_FOREACH(gp, &mp->geom, geom) {
		if (strcmp(gp->name, name) == 0) {
			gctl_error(req, "Provider %s already exists.", name);
			return (EEXIST);
		}
	}
	gp = g_new_geomf(mp, name);
	if (gp == NULL) {
		gctl_error(req, "Cannot create geom %s.", name);
		return (ENOMEM);
	}
	sc = g_malloc(sizeof(*sc), M_WAITOK);
	sc->sc_offset = offset;
	sc->sc_failprob = failprob;
	sc->sc_reads = 0;
	sc->sc_writes = 0;
	sc->sc_readbytes = 0;
	sc->sc_wrotebytes = 0;
	gp->softc = sc;
	gp->start = g_nop_start;
	gp->orphan = g_nop_orphan;
	gp->access = g_nop_access;
	gp->dumpconf = g_nop_dumpconf;

	newpp = g_new_providerf(gp, gp->name);
	if (newpp == NULL) {
		gctl_error(req, "Cannot create provider %s.", name);
		error = ENOMEM;
		goto fail;
	}
	newpp->mediasize = size;
	newpp->sectorsize = secsize;

	cp = g_new_consumer(gp);
	if (cp == NULL) {
		gctl_error(req, "Cannot create consumer for %s.", gp->name);
		error = ENOMEM;
		goto fail;
	}
	error = g_attach(cp, pp);
	if (error != 0) {
		gctl_error(req, "Cannot attach to provider %s.", pp->name);
		goto fail;
	}

	g_error_provider(newpp, 0);
	G_NOP_DEBUG(0, "Device %s created.", gp->name);
	return (0);
fail:
	if (cp != NULL) {
		if (cp->provider != NULL)
			g_detach(cp);
		g_destroy_consumer(cp);
	}
	if (newpp != NULL)
		g_destroy_provider(newpp);
	if (gp != NULL) {
		if (gp->softc != NULL)
			g_free(gp->softc);
		g_destroy_geom(gp);
	}
	return (error);
}

static int
g_nop_destroy(struct g_geom *gp, boolean_t force)
{
	struct g_provider *pp;

	g_topology_assert();
	if (gp->softc == NULL)
		return (ENXIO);
	pp = LIST_FIRST(&gp->provider);
	if (pp != NULL && (pp->acr != 0 || pp->acw != 0 || pp->ace != 0)) {
		if (force) {
			G_NOP_DEBUG(0, "Device %s is still open, so it "
			    "can't be definitely removed.", pp->name);
		} else {
			G_NOP_DEBUG(1, "Device %s is still open (r%dw%de%d).",
			    pp->name, pp->acr, pp->acw, pp->ace);
			return (EBUSY);
		}
	} else {
		G_NOP_DEBUG(0, "Device %s removed.", gp->name);
	}
	g_free(gp->softc);
	gp->softc = NULL;
	g_wither_geom(gp, ENXIO);

	return (0);
}

static int
g_nop_destroy_geom(struct gctl_req *req, struct g_class *mp, struct g_geom *gp)
{

	return (g_nop_destroy(gp, 0));
}

static void
g_nop_ctl_create(struct gctl_req *req, struct g_class *mp)
{
	struct g_provider *pp;
	intmax_t *failprob, *offset, *secsize, *size;
	const char *name;
	char param[16];
	int i, *nargs;

	g_topology_assert();

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument", "nargs");
		return;
	}
	if (*nargs <= 0) {
		gctl_error(req, "Missing device(s).");
		return;
	}
	failprob = gctl_get_paraml(req, "failprob", sizeof(*failprob));
	if (failprob == NULL) {
		gctl_error(req, "No '%s' argument", "failprob");
		return;
	}
	if (*failprob < 0 || *failprob > 100) {
		gctl_error(req, "Invalid '%s' argument", "failprob");
		return;
	}
	offset = gctl_get_paraml(req, "offset", sizeof(*offset));
	if (offset == NULL) {
		gctl_error(req, "No '%s' argument", "offset");
		return;
	}
	if (*offset < 0) {
		gctl_error(req, "Invalid '%s' argument", "offset");
		return;
	}
	size = gctl_get_paraml(req, "size", sizeof(*size));
	if (size == NULL) {
		gctl_error(req, "No '%s' argument", "size");
		return;
	}
	if (*size < 0) {
		gctl_error(req, "Invalid '%s' argument", "size");
		return;
	}
	secsize = gctl_get_paraml(req, "secsize", sizeof(*secsize));
	if (secsize == NULL) {
		gctl_error(req, "No '%s' argument", "secsize");
		return;
	}
	if (*secsize < 0) {
		gctl_error(req, "Invalid '%s' argument", "secsize");
		return;
	}

	for (i = 0; i < *nargs; i++) {
		snprintf(param, sizeof(param), "arg%d", i);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%d' argument", i);
			return;
		}
		if (strncmp(name, "/dev/", strlen("/dev/")) == 0)
			name += strlen("/dev/");
		pp = g_provider_by_name(name);
		if (pp == NULL) {
			G_NOP_DEBUG(1, "Provider %s is invalid.", name);
			gctl_error(req, "Provider %s is invalid.", name);
			return;
		}
		if (g_nop_create(req, mp, pp, (u_int)*failprob, (off_t)*offset,
		    (off_t)*size, (u_int)*secsize) != 0) {
			return;
		}
	}
}

static void
g_nop_ctl_configure(struct gctl_req *req, struct g_class *mp)
{
	struct g_nop_softc *sc;
	struct g_provider *pp;
	intmax_t *failprob;
	const char *name;
	char param[16];
	int i, *nargs;

	g_topology_assert();

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument", "nargs");
		return;
	}
	if (*nargs <= 0) {
		gctl_error(req, "Missing device(s).");
		return;
	}
	failprob = gctl_get_paraml(req, "failprob", sizeof(*failprob));
	if (failprob == NULL) {
		gctl_error(req, "No '%s' argument", "failprob");
		return;
	}
	if (*failprob < 0 || *failprob > 100) {
		gctl_error(req, "Invalid '%s' argument", "failprob");
		return;
	}

	for (i = 0; i < *nargs; i++) {
		snprintf(param, sizeof(param), "arg%d", i);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%d' argument", i);
			return;
		}
		if (strncmp(name, "/dev/", strlen("/dev/")) == 0)
			name += strlen("/dev/");
		pp = g_provider_by_name(name);
		if (pp == NULL || pp->geom->class != mp) {
			G_NOP_DEBUG(1, "Provider %s is invalid.", name);
			gctl_error(req, "Provider %s is invalid.", name);
			return;
		}
		sc = pp->geom->softc;
		sc->sc_failprob = (u_int)*failprob;
	}
}

static struct g_geom *
g_nop_find_geom(struct g_class *mp, const char *name)
{
	struct g_geom *gp;

	LIST_FOREACH(gp, &mp->geom, geom) {
		if (strcmp(gp->name, name) == 0)
			return (gp);
	}
	return (NULL);
}

static void
g_nop_ctl_destroy(struct gctl_req *req, struct g_class *mp)
{
	int *nargs, *force, error, i;
	struct g_geom *gp;
	const char *name;
	char param[16];

	g_topology_assert();

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument", "nargs");
		return;
	}
	if (*nargs <= 0) {
		gctl_error(req, "Missing device(s).");
		return;
	}
	force = gctl_get_paraml(req, "force", sizeof(*force));
	if (force == NULL) {
		gctl_error(req, "No 'force' argument");
		return;
	}

	for (i = 0; i < *nargs; i++) {
		snprintf(param, sizeof(param), "arg%d", i);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%d' argument", i);
			return;
		}
		if (strncmp(name, "/dev/", strlen("/dev/")) == 0)
			name += strlen("/dev/");
		gp = g_nop_find_geom(mp, name);
		if (gp == NULL) {
			G_NOP_DEBUG(1, "Device %s is invalid.", name);
			gctl_error(req, "Device %s is invalid.", name);
			return;
		}
		error = g_nop_destroy(gp, *force);
		if (error != 0) {
			gctl_error(req, "Cannot destroy device %s (error=%d).",
			    gp->name, error);
			return;
		}
	}
}

static void
g_nop_ctl_reset(struct gctl_req *req, struct g_class *mp)
{
	struct g_nop_softc *sc;
	struct g_provider *pp;
	const char *name;
	char param[16];
	int i, *nargs;

	g_topology_assert();

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument", "nargs");
		return;
	}
	if (*nargs <= 0) {
		gctl_error(req, "Missing device(s).");
		return;
	}

	for (i = 0; i < *nargs; i++) {
		snprintf(param, sizeof(param), "arg%d", i);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%d' argument", i);
			return;
		}
		if (strncmp(name, "/dev/", strlen("/dev/")) == 0)
			name += strlen("/dev/");
		pp = g_provider_by_name(name);
		if (pp == NULL || pp->geom->class != mp) {
			G_NOP_DEBUG(1, "Provider %s is invalid.", name);
			gctl_error(req, "Provider %s is invalid.", name);
			return;
		}
		sc = pp->geom->softc;
		sc->sc_reads = 0;
		sc->sc_writes = 0;
		sc->sc_readbytes = 0;
		sc->sc_wrotebytes = 0;
	}
}

static void
g_nop_config(struct gctl_req *req, struct g_class *mp, const char *verb)
{
	uint32_t *version;

	g_topology_assert();

	version = gctl_get_paraml(req, "version", sizeof(*version));
	if (version == NULL) {
		gctl_error(req, "No '%s' argument.", "version");
		return;
	}
	if (*version != G_NOP_VERSION) {
		gctl_error(req, "Userland and kernel parts are out of sync.");
		return;
	}

	if (strcmp(verb, "create") == 0) {
		g_nop_ctl_create(req, mp);
		return;
	} else if (strcmp(verb, "configure") == 0) {
		g_nop_ctl_configure(req, mp);
		return;
	} else if (strcmp(verb, "destroy") == 0) {
		g_nop_ctl_destroy(req, mp);
		return;
	} else if (strcmp(verb, "reset") == 0) {
		g_nop_ctl_reset(req, mp);
		return;
	}

	gctl_error(req, "Unknown verb.");
}

static void
g_nop_dumpconf(struct sbuf *sb, const char *indent, struct g_geom *gp,
    struct g_consumer *cp, struct g_provider *pp)
{
	struct g_nop_softc *sc;

	if (pp != NULL || cp != NULL)
		return;
	sc = gp->softc;
	sbuf_printf(sb, "%s<Offset>%jd</Offset>\n", indent,
	    (intmax_t)sc->sc_offset);
	sbuf_printf(sb, "%s<FailProb>%u</FailProb>\n", indent, sc->sc_failprob);
	sbuf_printf(sb, "%s<Reads>%ju</Reads>\n", indent, sc->sc_reads);
	sbuf_printf(sb, "%s<Writes>%ju</Writes>\n", indent, sc->sc_writes);
	sbuf_printf(sb, "%s<ReadBytes>%ju</ReadBytes>\n", indent,
	    sc->sc_readbytes);
	sbuf_printf(sb, "%s<WroteBytes>%ju</WroteBytes>\n", indent,
	    sc->sc_wrotebytes);
}

DECLARE_GEOM_CLASS(g_nop_class, g_nop);
