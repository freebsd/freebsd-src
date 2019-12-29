/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2004-2006 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * Copyright (c) 2019 Mariusz Zaborski <oshogbo@FreeBSD.org>
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
#include <sys/ctype.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bio.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <geom/geom.h>
#include <geom/geom_dbg.h>
#include <geom/nop/g_nop.h>


SYSCTL_DECL(_kern_geom);
static SYSCTL_NODE(_kern_geom, OID_AUTO, nop, CTLFLAG_RW, 0, "GEOM_NOP stuff");
static u_int g_nop_debug = 0;
SYSCTL_UINT(_kern_geom_nop, OID_AUTO, debug, CTLFLAG_RW, &g_nop_debug, 0,
    "Debug level");

static int g_nop_destroy(struct g_geom *gp, boolean_t force);
static int g_nop_destroy_geom(struct gctl_req *req, struct g_class *mp,
    struct g_geom *gp);
static void g_nop_config(struct gctl_req *req, struct g_class *mp,
    const char *verb);
static g_access_t g_nop_access;
static g_dumpconf_t g_nop_dumpconf;
static g_orphan_t g_nop_orphan;
static g_provgone_t g_nop_providergone;
static g_resize_t g_nop_resize;
static g_start_t g_nop_start;

struct g_class g_nop_class = {
	.name = G_NOP_CLASS_NAME,
	.version = G_VERSION,
	.ctlreq = g_nop_config,
	.destroy_geom = g_nop_destroy_geom,
	.access = g_nop_access,
	.dumpconf = g_nop_dumpconf,
	.orphan = g_nop_orphan,
	.providergone = g_nop_providergone,
	.resize = g_nop_resize,
	.start = g_nop_start,
};

struct g_nop_delay {
	struct callout			 dl_cal;
	struct bio			*dl_bio;
	TAILQ_ENTRY(g_nop_delay)	 dl_next;
};

static bool
g_nop_verify_nprefix(const char *name)
{
	int i;

	for (i = 0; i < strlen(name); i++) {
		if (isalpha(name[i]) == 0 && isdigit(name[i]) == 0) {
			return (false);
		}
	}

	return (true);
}

static void
g_nop_orphan(struct g_consumer *cp)
{

	g_topology_assert();
	g_nop_destroy(cp->geom, 1);
}

static void
g_nop_resize(struct g_consumer *cp)
{
	struct g_nop_softc *sc;
	struct g_geom *gp;
	struct g_provider *pp;
	off_t size;

	g_topology_assert();

	gp = cp->geom;
	sc = gp->softc;

	if (sc->sc_explicitsize != 0)
		return;
	if (cp->provider->mediasize < sc->sc_offset) {
		g_nop_destroy(gp, 1);
		return;
	}
	size = cp->provider->mediasize - sc->sc_offset;
	LIST_FOREACH(pp, &gp->provider, provider)
		g_resize_provider(pp, size);
}

static int
g_nop_dumper(void *priv, void *virtual, vm_offset_t physical, off_t offset,
    size_t length)
{

	return (0);
}

static void
g_nop_kerneldump(struct bio *bp, struct g_nop_softc *sc)
{
	struct g_kerneldump *gkd;
	struct g_geom *gp;
	struct g_provider *pp;

	gkd = (struct g_kerneldump *)bp->bio_data;
	gp = bp->bio_to->geom;
	g_trace(G_T_TOPOLOGY, "%s(%s, %jd, %jd)", __func__, gp->name,
	    (intmax_t)gkd->offset, (intmax_t)gkd->length);

	pp = LIST_FIRST(&gp->provider);

	gkd->di.dumper = g_nop_dumper;
	gkd->di.priv = sc;
	gkd->di.blocksize = pp->sectorsize;
	gkd->di.maxiosize = DFLTPHYS;
	gkd->di.mediaoffset = sc->sc_offset + gkd->offset;
	if (gkd->offset > sc->sc_explicitsize) {
		g_io_deliver(bp, ENODEV);
		return;
	}
	if (gkd->offset + gkd->length > sc->sc_explicitsize)
		gkd->length = sc->sc_explicitsize - gkd->offset;
	gkd->di.mediasize = gkd->length;
	g_io_deliver(bp, 0);
}

static void
g_nop_pass(struct bio *cbp, struct g_geom *gp)
{

	G_NOP_LOGREQ(cbp, "Sending request.");
	g_io_request(cbp, LIST_FIRST(&gp->consumer));
}

static void
g_nop_pass_timeout(void *data)
{
	struct g_nop_softc *sc;
	struct g_geom *gp;
	struct g_nop_delay *gndelay;

	gndelay = (struct g_nop_delay *)data;

	gp = gndelay->dl_bio->bio_to->geom;
	sc = gp->softc;

	mtx_lock(&sc->sc_lock);
	TAILQ_REMOVE(&sc->sc_head_delay, gndelay, dl_next);
	mtx_unlock(&sc->sc_lock);

	g_nop_pass(gndelay->dl_bio, gp);

	g_free(data);
}

static void
g_nop_start(struct bio *bp)
{
	struct g_nop_softc *sc;
	struct g_geom *gp;
	struct g_provider *pp;
	struct bio *cbp;
	u_int failprob, delayprob, delaytime;

	failprob = delayprob = 0;

	gp = bp->bio_to->geom;
	sc = gp->softc;

	G_NOP_LOGREQ(bp, "Request received.");
	mtx_lock(&sc->sc_lock);
	if (sc->sc_count_until_fail != 0 && --sc->sc_count_until_fail == 0) {
		sc->sc_rfailprob = 100;
		sc->sc_wfailprob = 100;
	}
	switch (bp->bio_cmd) {
	case BIO_READ:
		sc->sc_reads++;
		sc->sc_readbytes += bp->bio_length;
		failprob = sc->sc_rfailprob;
		delayprob = sc->sc_rdelayprob;
		delaytime = sc->sc_delaymsec;
		break;
	case BIO_WRITE:
		sc->sc_writes++;
		sc->sc_wrotebytes += bp->bio_length;
		failprob = sc->sc_wfailprob;
		delayprob = sc->sc_wdelayprob;
		delaytime = sc->sc_delaymsec;
		break;
	case BIO_DELETE:
		sc->sc_deletes++;
		break;
	case BIO_GETATTR:
		sc->sc_getattrs++;
		if (sc->sc_physpath &&
		    g_handleattr_str(bp, "GEOM::physpath", sc->sc_physpath))
			;
		else if (strcmp(bp->bio_attribute, "GEOM::kerneldump") == 0)
			g_nop_kerneldump(bp, sc);
		else
			/*
			 * Fallthrough to forwarding the GETATTR down to the
			 * lower level device.
			 */
			break;
		mtx_unlock(&sc->sc_lock);
		return;
	case BIO_FLUSH:
		sc->sc_flushes++;
		break;
	case BIO_CMD0:
		sc->sc_cmd0s++;
		break;
	case BIO_CMD1:
		sc->sc_cmd1s++;
		break;
	case BIO_CMD2:
		sc->sc_cmd2s++;
		break;
	}
	mtx_unlock(&sc->sc_lock);
	if (failprob > 0) {
		u_int rval;

		rval = arc4random() % 100;
		if (rval < failprob) {
			G_NOP_LOGREQLVL(1, bp, "Returning error=%d.", sc->sc_error);
			g_io_deliver(bp, sc->sc_error);
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
	pp = LIST_FIRST(&gp->provider);
	KASSERT(pp != NULL, ("NULL pp"));
	cbp->bio_to = pp;

	if (delayprob > 0) {
		struct g_nop_delay *gndelay;
		u_int rval;

		rval = arc4random() % 100;
		if (rval < delayprob) {
			gndelay = g_malloc(sizeof(*gndelay), M_NOWAIT | M_ZERO);
			if (gndelay != NULL) {
				callout_init(&gndelay->dl_cal, 1);

				gndelay->dl_bio = cbp;

				mtx_lock(&sc->sc_lock);
				TAILQ_INSERT_TAIL(&sc->sc_head_delay, gndelay,
				    dl_next);
				mtx_unlock(&sc->sc_lock);

				callout_reset(&gndelay->dl_cal,
				    MSEC_2_TICKS(delaytime), g_nop_pass_timeout,
				    gndelay);
				return;
			}
		}
	}

	g_nop_pass(cbp, gp);
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
    const char *gnopname, int ioerror, u_int count_until_fail,
    u_int rfailprob, u_int wfailprob, u_int delaymsec, u_int rdelayprob,
    u_int wdelayprob, off_t offset, off_t size, u_int secsize, off_t stripesize,
    off_t stripeoffset, const char *physpath)
{
	struct g_nop_softc *sc;
	struct g_geom *gp;
	struct g_provider *newpp;
	struct g_consumer *cp;
	char name[64];
	int error, n;
	off_t explicitsize;

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
	explicitsize = size;
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
	if (secsize > MAXPHYS) {
		gctl_error(req, "secsize is too big.");
		return (EINVAL);
	}
	size -= size % secsize;
	if ((stripesize % pp->sectorsize) != 0) {
		gctl_error(req, "Invalid stripesize for provider %s.", pp->name);
		return (EINVAL);
	}
	if ((stripeoffset % pp->sectorsize) != 0) {
		gctl_error(req, "Invalid stripeoffset for provider %s.", pp->name);
		return (EINVAL);
	}
	if (stripesize != 0 && stripeoffset >= stripesize) {
		gctl_error(req, "stripeoffset is too big.");
		return (EINVAL);
	}
	if (gnopname != NULL && !g_nop_verify_nprefix(gnopname)) {
		gctl_error(req, "Name %s is invalid.", gnopname);
		return (EINVAL);
	}

	if (gnopname != NULL) {
		n = snprintf(name, sizeof(name), "%s%s", gnopname,
		    G_NOP_SUFFIX);
	} else {
		n = snprintf(name, sizeof(name), "%s%s", pp->name,
		    G_NOP_SUFFIX);
	}
	if (n <= 0 || n >= sizeof(name)) {
		gctl_error(req, "Invalid provider name.");
		return (EINVAL);
	}
	LIST_FOREACH(gp, &mp->geom, geom) {
		if (strcmp(gp->name, name) == 0) {
			gctl_error(req, "Provider %s already exists.", name);
			return (EEXIST);
		}
	}
	gp = g_new_geomf(mp, "%s", name);
	sc = g_malloc(sizeof(*sc), M_WAITOK | M_ZERO);
	sc->sc_offset = offset;
	sc->sc_explicitsize = explicitsize;
	sc->sc_stripesize = stripesize;
	sc->sc_stripeoffset = stripeoffset;
	if (physpath && strcmp(physpath, G_NOP_PHYSPATH_PASSTHROUGH)) {
		sc->sc_physpath = strndup(physpath, MAXPATHLEN, M_GEOM);
	} else
		sc->sc_physpath = NULL;
	sc->sc_error = ioerror;
	sc->sc_count_until_fail = count_until_fail;
	sc->sc_rfailprob = rfailprob;
	sc->sc_wfailprob = wfailprob;
	sc->sc_delaymsec = delaymsec;
	sc->sc_rdelayprob = rdelayprob;
	sc->sc_wdelayprob = wdelayprob;
	sc->sc_reads = 0;
	sc->sc_writes = 0;
	sc->sc_deletes = 0;
	sc->sc_getattrs = 0;
	sc->sc_flushes = 0;
	sc->sc_cmd0s = 0;
	sc->sc_cmd1s = 0;
	sc->sc_cmd2s = 0;
	sc->sc_readbytes = 0;
	sc->sc_wrotebytes = 0;
	TAILQ_INIT(&sc->sc_head_delay);
	mtx_init(&sc->sc_lock, "gnop lock", NULL, MTX_DEF);
	gp->softc = sc;

	newpp = g_new_providerf(gp, "%s", gp->name);
	newpp->flags |= G_PF_DIRECT_SEND | G_PF_DIRECT_RECEIVE;
	newpp->mediasize = size;
	newpp->sectorsize = secsize;
	newpp->stripesize = stripesize;
	newpp->stripeoffset = stripeoffset;

	cp = g_new_consumer(gp);
	cp->flags |= G_CF_DIRECT_SEND | G_CF_DIRECT_RECEIVE;
	error = g_attach(cp, pp);
	if (error != 0) {
		gctl_error(req, "Cannot attach to provider %s.", pp->name);
		goto fail;
	}

	newpp->flags |= pp->flags & G_PF_ACCEPT_UNMAPPED;
	g_error_provider(newpp, 0);
	G_NOP_DEBUG(0, "Device %s created.", gp->name);
	return (0);
fail:
	if (cp->provider != NULL)
		g_detach(cp);
	g_destroy_consumer(cp);
	g_destroy_provider(newpp);
	mtx_destroy(&sc->sc_lock);
	free(sc->sc_physpath, M_GEOM);
	g_free(gp->softc);
	g_destroy_geom(gp);
	return (error);
}

static void
g_nop_providergone(struct g_provider *pp)
{
	struct g_geom *gp = pp->geom;
	struct g_nop_softc *sc = gp->softc;

	KASSERT(TAILQ_EMPTY(&sc->sc_head_delay),
	    ("delayed request list is not empty"));

	gp->softc = NULL;
	free(sc->sc_physpath, M_GEOM);
	mtx_destroy(&sc->sc_lock);
	g_free(sc);
}

static int
g_nop_destroy(struct g_geom *gp, boolean_t force)
{
	struct g_nop_softc *sc;
	struct g_provider *pp;

	g_topology_assert();
	sc = gp->softc;
	if (sc == NULL)
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
	intmax_t *val, error, rfailprob, wfailprob, count_until_fail, offset,
	    secsize, size, stripesize, stripeoffset, delaymsec,
	    rdelayprob, wdelayprob;
	const char *name, *physpath, *gnopname;
	char param[16];
	int i, *nargs;

	g_topology_assert();

	error = -1;
	rfailprob = -1;
	wfailprob = -1;
	count_until_fail = -1;
	offset = 0;
	secsize = 0;
	size = 0;
	stripesize = 0;
	stripeoffset = 0;
	delaymsec = -1;
	rdelayprob = -1;
	wdelayprob = -1;

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument", "nargs");
		return;
	}
	if (*nargs <= 0) {
		gctl_error(req, "Missing device(s).");
		return;
	}
	val = gctl_get_paraml_opt(req, "error", sizeof(*val));
	if (val != NULL) {
		error = *val;
	}
	val = gctl_get_paraml_opt(req, "rfailprob", sizeof(*val));
	if (val != NULL) {
		rfailprob = *val;
		if (rfailprob < -1 || rfailprob > 100) {
			gctl_error(req, "Invalid '%s' argument", "rfailprob");
			return;
		}
	}
	val = gctl_get_paraml_opt(req, "wfailprob", sizeof(*val));
	if (val != NULL) {
		wfailprob = *val;
		if (wfailprob < -1 || wfailprob > 100) {
			gctl_error(req, "Invalid '%s' argument", "wfailprob");
			return;
		}
	}
	val = gctl_get_paraml_opt(req, "delaymsec", sizeof(*val));
	if (val != NULL) {
		delaymsec = *val;
		if (delaymsec < 1 && delaymsec != -1) {
			gctl_error(req, "Invalid '%s' argument", "delaymsec");
			return;
		}
	}
	val = gctl_get_paraml_opt(req, "rdelayprob", sizeof(*val));
	if (val != NULL) {
		rdelayprob = *val;
		if (rdelayprob < -1 || rdelayprob > 100) {
			gctl_error(req, "Invalid '%s' argument", "rdelayprob");
			return;
		}
	}
	val = gctl_get_paraml_opt(req, "wdelayprob", sizeof(*val));
	if (val != NULL) {
		wdelayprob = *val;
		if (wdelayprob < -1 || wdelayprob > 100) {
			gctl_error(req, "Invalid '%s' argument", "wdelayprob");
			return;
		}
	}
	val = gctl_get_paraml_opt(req, "count_until_fail", sizeof(*val));
	if (val != NULL) {
		count_until_fail = *val;
		if (count_until_fail < -1) {
			gctl_error(req, "Invalid '%s' argument",
			    "count_until_fail");
			return;
		}
	}
	val = gctl_get_paraml_opt(req, "offset", sizeof(*val));
	if (val != NULL) {
		offset = *val;
		if (offset < 0) {
			gctl_error(req, "Invalid '%s' argument", "offset");
			return;
		}
	}
	val = gctl_get_paraml_opt(req, "size", sizeof(*val));
	if (val != NULL) {
		size = *val;
		if (size < 0) {
			gctl_error(req, "Invalid '%s' argument", "size");
			return;
		}
	}
	val = gctl_get_paraml_opt(req, "secsize", sizeof(*val));
	if (val != NULL) {
		secsize = *val;
		if (secsize < 0) {
			gctl_error(req, "Invalid '%s' argument", "secsize");
			return;
		}
	}
	val = gctl_get_paraml_opt(req, "stripesize", sizeof(*val));
	if (val != NULL) {
		stripesize = *val;
		if (stripesize < 0) {
			gctl_error(req, "Invalid '%s' argument", "stripesize");
			return;
		}
	}
	val = gctl_get_paraml_opt(req, "stripeoffset", sizeof(*val));
	if (val != NULL) {
		stripeoffset = *val;
		if (stripeoffset < 0) {
			gctl_error(req, "Invalid '%s' argument",
			    "stripeoffset");
			return;
		}
	}
	physpath = gctl_get_asciiparam(req, "physpath");
	gnopname = gctl_get_asciiparam(req, "gnopname");

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
		if (g_nop_create(req, mp, pp,
		    gnopname,
		    error == -1 ? EIO : (int)error,
		    count_until_fail == -1 ? 0 : (u_int)count_until_fail,
		    rfailprob == -1 ? 0 : (u_int)rfailprob,
		    wfailprob == -1 ? 0 : (u_int)wfailprob,
		    delaymsec == -1 ? 1 : (u_int)delaymsec,
		    rdelayprob == -1 ? 0 : (u_int)rdelayprob,
		    wdelayprob == -1 ? 0 : (u_int)wdelayprob,
		    (off_t)offset, (off_t)size, (u_int)secsize,
		    (off_t)stripesize, (off_t)stripeoffset,
		    physpath) != 0) {
			return;
		}
	}
}

static void
g_nop_ctl_configure(struct gctl_req *req, struct g_class *mp)
{
	struct g_nop_softc *sc;
	struct g_provider *pp;
	intmax_t *val, delaymsec, error, rdelayprob, rfailprob, wdelayprob,
	    wfailprob, count_until_fail;
	const char *name;
	char param[16];
	int i, *nargs;

	g_topology_assert();

	count_until_fail = -1;
	delaymsec = -1;
	error = -1;
	rdelayprob = -1;
	rfailprob = -1;
	wdelayprob = -1;
	wfailprob = -1;

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument", "nargs");
		return;
	}
	if (*nargs <= 0) {
		gctl_error(req, "Missing device(s).");
		return;
	}
	val = gctl_get_paraml_opt(req, "error", sizeof(*val));
	if (val != NULL) {
		error = *val;
	}
	val = gctl_get_paraml_opt(req, "count_until_fail", sizeof(*val));
	if (val != NULL) {
		count_until_fail = *val;
	}
	val = gctl_get_paraml_opt(req, "rfailprob", sizeof(*val));
	if (val != NULL) {
		rfailprob = *val;
		if (rfailprob < -1 || rfailprob > 100) {
			gctl_error(req, "Invalid '%s' argument", "rfailprob");
			return;
		}
	}
	val = gctl_get_paraml_opt(req, "wfailprob", sizeof(*val));
	if (val != NULL) {
		wfailprob = *val;
		if (wfailprob < -1 || wfailprob > 100) {
			gctl_error(req, "Invalid '%s' argument", "wfailprob");
			return;
		}
	}
	val = gctl_get_paraml_opt(req, "delaymsec", sizeof(*val));
	if (val != NULL) {
		delaymsec = *val;
		if (delaymsec < 1 && delaymsec != -1) {
			gctl_error(req, "Invalid '%s' argument", "delaymsec");
			return;
		}
	}
	val = gctl_get_paraml_opt(req, "rdelayprob", sizeof(*val));
	if (val != NULL) {
		rdelayprob = *val;
		if (rdelayprob < -1 || rdelayprob > 100) {
			gctl_error(req, "Invalid '%s' argument", "rdelayprob");
			return;
		}
	}
	val = gctl_get_paraml_opt(req, "wdelayprob", sizeof(*val));
	if (val != NULL) {
		wdelayprob = *val;
		if (wdelayprob < -1 || wdelayprob > 100) {
			gctl_error(req, "Invalid '%s' argument", "wdelayprob");
			return;
		}
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
		if (error != -1)
			sc->sc_error = (int)error;
		if (rfailprob != -1)
			sc->sc_rfailprob = (u_int)rfailprob;
		if (wfailprob != -1)
			sc->sc_wfailprob = (u_int)wfailprob;
		if (rdelayprob != -1)
			sc->sc_rdelayprob = (u_int)rdelayprob;
		if (wdelayprob != -1)
			sc->sc_wdelayprob = (u_int)wdelayprob;
		if (delaymsec != -1)
			sc->sc_delaymsec = (u_int)delaymsec;
		if (count_until_fail != -1)
			sc->sc_count_until_fail = (u_int)count_until_fail;
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
		sc->sc_deletes = 0;
		sc->sc_getattrs = 0;
		sc->sc_flushes = 0;
		sc->sc_cmd0s = 0;
		sc->sc_cmd1s = 0;
		sc->sc_cmd2s = 0;
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
	sbuf_printf(sb, "%s<ReadFailProb>%u</ReadFailProb>\n", indent,
	    sc->sc_rfailprob);
	sbuf_printf(sb, "%s<WriteFailProb>%u</WriteFailProb>\n", indent,
	    sc->sc_wfailprob);
	sbuf_printf(sb, "%s<ReadDelayedProb>%u</ReadDelayedProb>\n", indent,
	    sc->sc_rdelayprob);
	sbuf_printf(sb, "%s<WriteDelayedProb>%u</WriteDelayedProb>\n", indent,
	    sc->sc_wdelayprob);
	sbuf_printf(sb, "%s<Delay>%d</Delay>\n", indent, sc->sc_delaymsec);
	sbuf_printf(sb, "%s<CountUntilFail>%u</CountUntilFail>\n", indent,
	    sc->sc_count_until_fail);
	sbuf_printf(sb, "%s<Error>%d</Error>\n", indent, sc->sc_error);
	sbuf_printf(sb, "%s<Reads>%ju</Reads>\n", indent, sc->sc_reads);
	sbuf_printf(sb, "%s<Writes>%ju</Writes>\n", indent, sc->sc_writes);
	sbuf_printf(sb, "%s<Deletes>%ju</Deletes>\n", indent, sc->sc_deletes);
	sbuf_printf(sb, "%s<Getattrs>%ju</Getattrs>\n", indent, sc->sc_getattrs);
	sbuf_printf(sb, "%s<Flushes>%ju</Flushes>\n", indent, sc->sc_flushes);
	sbuf_printf(sb, "%s<Cmd0s>%ju</Cmd0s>\n", indent, sc->sc_cmd0s);
	sbuf_printf(sb, "%s<Cmd1s>%ju</Cmd1s>\n", indent, sc->sc_cmd1s);
	sbuf_printf(sb, "%s<Cmd2s>%ju</Cmd2s>\n", indent, sc->sc_cmd2s);
	sbuf_printf(sb, "%s<ReadBytes>%ju</ReadBytes>\n", indent,
	    sc->sc_readbytes);
	sbuf_printf(sb, "%s<WroteBytes>%ju</WroteBytes>\n", indent,
	    sc->sc_wrotebytes);
}

DECLARE_GEOM_CLASS(g_nop_class, g_nop);
MODULE_VERSION(geom_nop, 0);
