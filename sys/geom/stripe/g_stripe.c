/*-
 * Copyright (c) 2003 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
#include <geom/stripe/g_stripe.h>


static MALLOC_DEFINE(M_STRIPE, "stripe data", "GEOM_STRIPE Data");

SYSCTL_DECL(_kern_geom);
SYSCTL_NODE(_kern_geom, OID_AUTO, stripe, CTLFLAG_RW, 0, "GEOM_STRIPE stuff");
static u_int g_stripe_debug = 0;
SYSCTL_UINT(_kern_geom_stripe, OID_AUTO, debug, CTLFLAG_RW, &g_stripe_debug, 0,
    "Debug level");

static int g_stripe_destroy(struct g_stripe_softc *sc, boolean_t force);
static int g_stripe_destroy_geom(struct gctl_req *req, struct g_class *mp,
    struct g_geom *gp);

static g_taste_t g_stripe_taste;
static g_ctl_req_t g_stripe_config;
static g_dumpconf_t g_stripe_dumpconf;

struct g_class g_stripe_class = {
	.name = G_STRIPE_CLASS_NAME,
	.ctlreq = g_stripe_config,
	.taste = g_stripe_taste,
	.destroy_geom = g_stripe_destroy_geom
};


/*
 * Greatest Common Divisor.
 */
static u_int
gcd(u_int a, u_int b)
{
	u_int c;

	while (b != 0) {
		c = a;
		a = b;
		b = (c % b);
	}
	return (a);
}

/*
 * Least Common Multiple.
 */
static u_int
lcm(u_int a, u_int b)
{

	return ((a * b) / gcd(a, b));
}

/*
 * Return the number of valid disks.
 */
static u_int
g_stripe_nvalid(struct g_stripe_softc *sc)
{
	u_int i, no;

	no = 0;
	for (i = 0; i < sc->sc_ndisks; i++) {
		if (sc->sc_disks[i] != NULL)
			no++;
	}

	return (no);
}

static void
g_stripe_remove_disk(struct g_consumer *cp)
{
	struct g_stripe_softc *sc;
	u_int no;

	KASSERT(cp != NULL, ("Non-valid disk in %s.", __func__));
	sc = (struct g_stripe_softc *)cp->private;
	KASSERT(sc != NULL, ("NULL sc in %s.", __func__));
	no = cp->index;

	G_STRIPE_DEBUG(0, "Disk %s removed from %s.", cp->provider->name,
	    sc->sc_geom->name);

	sc->sc_disks[no] = NULL;
	if (sc->sc_provider != NULL) {
		g_orphan_provider(sc->sc_provider, ENXIO);
		sc->sc_provider = NULL;
		G_STRIPE_DEBUG(0, "Device %s removed.", sc->sc_geom->name);
	}

	if (cp->acr > 0 || cp->acw > 0 || cp->ace > 0)
		g_access(cp, -cp->acr, -cp->acw, -cp->ace);
	g_detach(cp);
	g_destroy_consumer(cp);
}

static void
g_stripe_orphan(struct g_consumer *cp)
{
	struct g_stripe_softc *sc;
	struct g_geom *gp;

	g_topology_assert();
	gp = cp->geom;
	sc = gp->softc;
	if (sc == NULL)
		return;

	g_stripe_remove_disk(cp);
	/* If there are no valid disks anymore, remove device. */
	if (g_stripe_nvalid(sc) == 0)
		g_stripe_destroy(sc, 1);
}

static int
g_stripe_access(struct g_provider *pp, int dr, int dw, int de)
{
	struct g_consumer *cp1, *cp2;
	struct g_stripe_softc *sc;
	struct g_geom *gp;
	int error;

	gp = pp->geom;
	sc = gp->softc;

	if (sc == NULL) {
		/*
		 * It looks like geom is being withered.
		 * In that case we allow only negative requests.
		 */
		KASSERT(dr <= 0 && dw <= 0 && de <= 0,
		    ("Positive access request (device=%s).", pp->name));
		if ((pp->acr + dr) == 0 && (pp->acw + dw) == 0 &&
		    (pp->ace + de) == 0) {
			G_STRIPE_DEBUG(0, "Device %s definitely destroyed.",
			    gp->name);
		}
		return (0);
	}

	/* On first open, grab an extra "exclusive" bit */
	if (pp->acr == 0 && pp->acw == 0 && pp->ace == 0)
		de++;
	/* ... and let go of it on last close */
	if ((pp->acr + dr) == 0 && (pp->acw + dw) == 0 && (pp->ace + de) == 0)
		de--;

	error = ENXIO;
	LIST_FOREACH(cp1, &gp->consumer, consumer) {
		error = g_access(cp1, dr, dw, de);
		if (error == 0)
			continue;
		/*
		 * If we fail here, backout all previous changes.
		 */
		LIST_FOREACH(cp2, &gp->consumer, consumer) {
			if (cp1 == cp2)
				return (error);
			g_access(cp2, -dr, -dw, -de);
		}
		/* NOTREACHED */
	}

	return (error);
}

static void
g_stripe_start(struct bio *bp)
{
	struct g_provider *pp;
	struct g_stripe_softc *sc;
	off_t off, start, length, nstripe;
	struct bio *cbp;
	u_int sectorsize;
	uint32_t stripesize;
	uint16_t no;
	char *addr;

	pp = bp->bio_to;
	sc = pp->geom->softc;
	/*
	 * If sc == NULL, provider's error should be set and g_stripe_start()
	 * should not be called at all.
	 */
	KASSERT(sc != NULL,
	    ("Provider's error should be set (error=%d)(device=%s).",
	    bp->bio_to->error, bp->bio_to->name));

	G_STRIPE_LOGREQ(bp, "Request received.");

	switch (bp->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
	case BIO_DELETE:
		/*
		 * Only those requests are supported.
		 */
		break;
	case BIO_GETATTR:
		/* To which provider it should be delivered? */
	default:
		g_io_deliver(bp, EOPNOTSUPP);
		return;
	}

	addr = bp->bio_data;
	sectorsize = sc->sc_provider->sectorsize;
	stripesize = sc->sc_stripesize;

	/*
	 * Calcucations are quite messy, but fast I hope.
	 */

	/* Stripe number. */
	/* nstripe = bp->bio_offset / stripesize; */
	nstripe = bp->bio_offset >> (off_t)sc->sc_stripebits;
	/* Disk number. */
	no = nstripe % sc->sc_ndisks;
	/* Start position in stripe. */
	/* start = bp->bio_offset % stripesize; */
	start = bp->bio_offset & (stripesize - 1);
	/* Start position in disk. */
	/* off = (nstripe / sc->sc_ndisks) * stripesize + start; */
	off = ((nstripe / sc->sc_ndisks) << sc->sc_stripebits) + start;
	/* Length of data to operate. */
	length = MIN(bp->bio_length, stripesize - start);

	cbp = g_clone_bio(bp);
	if (cbp == NULL) {
		/*
		 * Deny all request. This is pointless
		 * to split rest of the request, bacause
		 * we're setting bio_error here, so all
		 * request will be denied, anyway.
		 */
		bp->bio_completed = bp->bio_length;
		if (bp->bio_error == 0)
			bp->bio_error = ENOMEM;
		g_io_deliver(bp, bp->bio_error);
		return;
	}
	/*
	 * Fill in the component buf structure.
	 */
	cbp->bio_done = g_std_done;
	cbp->bio_offset = off;
	cbp->bio_data = addr;
	cbp->bio_length = length;
	cbp->bio_to = sc->sc_disks[no]->provider;
	G_STRIPE_LOGREQ(cbp, "Sending request.");
	g_io_request(cbp, sc->sc_disks[no]);

	/* off -= off % stripesize; */
	off -= off & (stripesize - 1);
	addr += length;
	length = bp->bio_length - length;
	for (no++; length > 0; no++, length -= stripesize, addr += stripesize) {
		if (no > sc->sc_ndisks - 1) {
			no = 0;
			off += stripesize;
		}
		cbp = g_clone_bio(bp);
		if (cbp == NULL) {
			/*
			 * Deny remaining part. This is pointless
			 * to split rest of the request, bacause
			 * we're setting bio_error here, so all
			 * request will be denied, anyway.
			 */
			bp->bio_completed += length;
			if (bp->bio_error == 0)
				bp->bio_error = ENOMEM;
			if (bp->bio_completed == bp->bio_length)
				g_io_deliver(bp, bp->bio_error);
			return;
		}

		/*
		 * Fill in the component buf structure.
		 */
		cbp->bio_done = g_std_done;
		cbp->bio_offset = off;
		cbp->bio_data = addr;
		/*
		 * MIN() is in case when
		 * (bp->bio_length % sc->sc_stripesize) != 0.
		 */
		cbp->bio_length = MIN(stripesize, length);

		cbp->bio_to = sc->sc_disks[no]->provider;
		G_STRIPE_LOGREQ(cbp, "Sending request.");
		g_io_request(cbp, sc->sc_disks[no]);
	}
}

static void
g_stripe_check_and_run(struct g_stripe_softc *sc)
{
	off_t mediasize, ms;
	u_int no, sectorsize = 0;

	if (g_stripe_nvalid(sc) != sc->sc_ndisks)
		return;

	sc->sc_provider = g_new_providerf(sc->sc_geom, "%s", sc->sc_geom->name);
	/*
	 * Find the smallest disk.
	 */
	mediasize = sc->sc_disks[0]->provider->mediasize;
	if (sc->sc_type == G_STRIPE_TYPE_AUTOMATIC)
		mediasize -= sc->sc_disks[0]->provider->sectorsize;
	mediasize -= mediasize % sc->sc_stripesize;
	sectorsize = sc->sc_disks[0]->provider->sectorsize;
	for (no = 1; no < sc->sc_ndisks; no++) {
		ms = sc->sc_disks[no]->provider->mediasize;
		if (sc->sc_type == G_STRIPE_TYPE_AUTOMATIC)
			ms -= sc->sc_disks[no]->provider->sectorsize;
		ms -= ms % sc->sc_stripesize;
		if (ms < mediasize)
			mediasize = ms;
		sectorsize = lcm(sectorsize,
		    sc->sc_disks[no]->provider->sectorsize);
	}
	sc->sc_provider->sectorsize = sectorsize;
	sc->sc_provider->mediasize = mediasize * sc->sc_ndisks;
	g_error_provider(sc->sc_provider, 0);

	G_STRIPE_DEBUG(0, "Device %s activated.", sc->sc_geom->name);
}

static int
g_stripe_read_metadata(struct g_consumer *cp, struct g_stripe_metadata *md)
{
	struct g_provider *pp;
	u_char *buf;
	int error;

	g_topology_assert();

	error = g_access(cp, 1, 0, 0);
	if (error != 0)
		return (error);
	pp = cp->provider;
	g_topology_unlock();
	buf = g_read_data(cp, pp->mediasize - pp->sectorsize, pp->sectorsize,
	    &error);
	g_topology_lock();
	g_access(cp, -1, 0, 0);
	if (buf == NULL)
		return (error);

	/* Decode metadata. */
	stripe_metadata_decode(buf, md);
	g_free(buf);

	return (0);
}

/*
 * Add disk to given device.
 */
static int
g_stripe_add_disk(struct g_stripe_softc *sc, struct g_provider *pp, u_int no)
{
	struct g_consumer *cp, *fcp;
	struct g_geom *gp;
	int error;

	/* Metadata corrupted? */
	if (no >= sc->sc_ndisks)
		return (EINVAL);

	/* Check if disk is not already attached. */
	if (sc->sc_disks[no] != NULL)
		return (EEXIST);

	gp = sc->sc_geom;
	fcp = LIST_FIRST(&gp->consumer);

	cp = g_new_consumer(gp);
	error = g_attach(cp, pp);
	if (error != 0) {
		g_destroy_consumer(cp);
		return (error);
	}

	if (fcp != NULL && (fcp->acr > 0 || fcp->acw > 0 || fcp->ace > 0)) {
		error = g_access(cp, fcp->acr, fcp->acw, fcp->ace);
		if (error != 0) {
			g_detach(cp);
			g_destroy_consumer(cp);
			return (error);
		}
	}
	if (sc->sc_type == G_STRIPE_TYPE_AUTOMATIC) {
		struct g_stripe_metadata md;

		/* Reread metadata. */
		error = g_stripe_read_metadata(cp, &md);
		if (error != 0)
			goto fail;

		if (strcmp(md.md_magic, G_STRIPE_MAGIC) != 0 ||
		    strcmp(md.md_name, sc->sc_name) != 0 ||
		    md.md_id != sc->sc_id) {
			G_STRIPE_DEBUG(0, "Metadata on %s changed.", pp->name);
			goto fail;
		}
	}

	cp->private = sc;
	cp->index = no;
	sc->sc_disks[no] = cp;

	G_STRIPE_DEBUG(0, "Disk %s attached to %s.", pp->name, gp->name);

	g_stripe_check_and_run(sc);

	return (0);
fail:
	if (fcp != NULL && (fcp->acr > 0 || fcp->acw > 0 || fcp->ace > 0))
		g_access(cp, -fcp->acr, -fcp->acw, -fcp->ace);
	g_detach(cp);
	g_destroy_consumer(cp);
	return (error);
}

static struct g_geom *
g_stripe_create(struct g_class *mp, const struct g_stripe_metadata *md,
    u_int type)
{
	struct g_stripe_softc *sc;
	struct g_geom *gp;
	u_int no;

	G_STRIPE_DEBUG(1, "Creating device %s.stripe (id=%u).", md->md_name,
	    md->md_id);

	/* Two disks is minimum. */
	if (md->md_all <= 1) {
		G_STRIPE_DEBUG(0, "Too few disks defined for %s.stripe.",
		    md->md_name);
		return (NULL);
	}
#if 0
	/* Stripe size have to be grater than or equal to sector size. */
	if (md->md_stripesize < sectorsize) {
		G_STRIPE_DEBUG(0, "Invalid stripe size for %s.stripe.",
		    md->md_name);
		return (NULL);
	}
#endif
	/* Stripe size have to be power of 2. */
	if (!powerof2(md->md_stripesize)) {
		G_STRIPE_DEBUG(0, "Invalid stripe size for %s.stripe.",
		    md->md_name);
		return (NULL);
	}

	/* Check for duplicate unit */
	LIST_FOREACH(gp, &mp->geom, geom) {
		sc = gp->softc;
		if (sc != NULL && strcmp(sc->sc_name, md->md_name) == 0) {
			G_STRIPE_DEBUG(0, "Device %s already configured.",
			    gp->name);
			return (NULL);
		}
	}
	gp = g_new_geomf(mp, "%s.stripe", md->md_name);
	gp->softc = NULL;	/* for a moment */

	sc = malloc(sizeof(*sc), M_STRIPE, M_NOWAIT | M_ZERO);
	if (sc == NULL) {
		G_STRIPE_DEBUG(0, "Can't allocate memory for device %s.",
		    gp->name);
		g_destroy_geom(gp);
		return (NULL);
	}

	gp->start = g_stripe_start;
	gp->spoiled = g_stripe_orphan;
	gp->orphan = g_stripe_orphan;
	gp->access = g_stripe_access;
	gp->dumpconf = g_stripe_dumpconf;

	strlcpy(sc->sc_name, md->md_name, sizeof(sc->sc_name));
	sc->sc_id = md->md_id;
	sc->sc_stripesize = md->md_stripesize;
	sc->sc_stripebits = BITCOUNT(sc->sc_stripesize - 1);
	sc->sc_ndisks = md->md_all;
	sc->sc_disks = malloc(sizeof(struct g_consumer *) * sc->sc_ndisks,
	    M_STRIPE, M_WAITOK | M_ZERO);
	for (no = 0; no < sc->sc_ndisks; no++)
		sc->sc_disks[no] = NULL;
	sc->sc_type = type;

	gp->softc = sc;
	sc->sc_geom = gp;
	sc->sc_provider = NULL;

	G_STRIPE_DEBUG(0, "Device %s created (id=%u).", gp->name, sc->sc_id);

	return (gp);
}

static int
g_stripe_destroy(struct g_stripe_softc *sc, boolean_t force)
{
	struct g_provider *pp;
	struct g_geom *gp;
	u_int no;

	g_topology_assert();

	if (sc == NULL)
		return (ENXIO);

	pp = sc->sc_provider;
	if (pp != NULL && (pp->acr != 0 || pp->acw != 0 || pp->ace != 0)) {
		if (force) {
			G_STRIPE_DEBUG(0, "Device %s is still open, so it "
			    "can't be definitely removed.", pp->name);
		} else {
			G_STRIPE_DEBUG(1,
			    "Device %s is still open (r%dw%de%d).", pp->name,
			    pp->acr, pp->acw, pp->ace);
			return (EBUSY);
		}
	}

	for (no = 0; no < sc->sc_ndisks; no++) {
		if (sc->sc_disks[no] != NULL)
			g_stripe_remove_disk(sc->sc_disks[no]);
	}

	gp = sc->sc_geom;
	gp->softc = NULL;
	KASSERT(sc->sc_provider == NULL, ("Provider still exists? (device=%s)",
	    gp->name));
	free(sc->sc_disks, M_STRIPE);
	free(sc, M_STRIPE);

	pp = LIST_FIRST(&gp->provider); 
	if (pp == NULL || (pp->acr == 0 && pp->acw == 0 && pp->ace == 0))
		G_STRIPE_DEBUG(0, "Device %s destroyed.", gp->name);

	g_wither_geom(gp, ENXIO);

	return (0);
}

static int
g_stripe_destroy_geom(struct gctl_req *req __unused,
    struct g_class *mp __unused, struct g_geom *gp)
{
	struct g_stripe_softc *sc;

	sc = gp->softc;
	return (g_stripe_destroy(sc, 0));
}

static struct g_geom *
g_stripe_taste(struct g_class *mp, struct g_provider *pp, int flags __unused)
{
	struct g_stripe_metadata md;
	struct g_stripe_softc *sc;
	struct g_consumer *cp;
	struct g_geom *gp;
	int error;

	g_trace(G_T_TOPOLOGY, "%s(%s, %s)", __func__, mp->name, pp->name);
	g_topology_assert();

	G_STRIPE_DEBUG(3, "Tasting %s.", pp->name);

	gp = g_new_geomf(mp, "stripe:taste");
	gp->start = g_stripe_start;
	gp->access = g_stripe_access;
	gp->orphan = g_stripe_orphan;
	cp = g_new_consumer(gp);
	g_attach(cp, pp);

	error = g_stripe_read_metadata(cp, &md);
	g_wither_geom(gp, ENXIO);
	if (error != 0)
		return (NULL);
	gp = NULL;

	if (strcmp(md.md_magic, G_STRIPE_MAGIC) != 0)
		return (NULL);
	if (md.md_version > G_STRIPE_VERSION) {
		printf("geom_stripe.ko module is too old to handle %s.\n",
		    pp->name);
		return (NULL);
	}

	/*
	 * Let's check if device already exists.
	 */
	sc = NULL;
	LIST_FOREACH(gp, &mp->geom, geom) {
		sc = gp->softc;
		if (sc == NULL)
			continue;
		if (sc->sc_type != G_STRIPE_TYPE_AUTOMATIC)
			continue;
		if (strcmp(md.md_name, sc->sc_name) != 0)
			continue;
		if (md.md_id != sc->sc_id)
			continue;
		break;
	}
	if (gp != NULL) {
		G_STRIPE_DEBUG(1, "Adding disk %s to %s.", pp->name, gp->name);
		error = g_stripe_add_disk(sc, pp, md.md_no);
		if (error != 0) {
			G_STRIPE_DEBUG(0,
			    "Cannot add disk %s to %s (error=%d).", pp->name,
			    gp->name, error);
			return (NULL);
		}
	} else {
		gp = g_stripe_create(mp, &md, G_STRIPE_TYPE_AUTOMATIC);
		if (gp == NULL) {
			G_STRIPE_DEBUG(0, "Cannot create device %s.stripe.",
			    md.md_name);
			return (NULL);
		}
		sc = gp->softc;
		G_STRIPE_DEBUG(1, "Adding disk %s to %s.", pp->name, gp->name);
		error = g_stripe_add_disk(sc, pp, md.md_no);
		if (error != 0) {
			G_STRIPE_DEBUG(0,
			    "Cannot add disk %s to %s (error=%d).", pp->name,
			    gp->name, error);
			g_stripe_destroy(sc, 1);
			return (NULL);
		}
	}

	return (gp);
}

static void
g_stripe_ctl_create(struct gctl_req *req, struct g_class *mp)
{
	u_int attached, no;
	struct g_stripe_metadata md;
	struct g_provider *pp;
	struct g_stripe_softc *sc;
	struct g_geom *gp;
	struct sbuf *sb;
	intmax_t *stripesize;
	const char *name;
	char param[16];
	int *nargs;

	g_topology_assert();
	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs <= 2) {
		gctl_error(req, "Too few arguments.");
		return;
	}

	strlcpy(md.md_magic, G_STRIPE_MAGIC, sizeof(md.md_magic));
	md.md_version = G_STRIPE_VERSION;
	name = gctl_get_asciiparam(req, "arg0");
	if (name == NULL) {
		gctl_error(req, "No 'arg%u' argument.", 0);
		return;
	}
	strlcpy(md.md_name, name, sizeof(md.md_name));
	md.md_id = arc4random();
	md.md_no = 0;
	md.md_all = *nargs - 1;
	stripesize = gctl_get_paraml(req, "stripesize", sizeof(*stripesize));
	if (stripesize == NULL) {
		gctl_error(req, "No '%s' argument.", "stripesize");
		return;
	}
	md.md_stripesize = *stripesize;

	/* Check all providers are valid */
	for (no = 1; no < *nargs; no++) {
		snprintf(param, sizeof(param), "arg%u", no);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%u' argument.", no);
			return;
		}
		if (strncmp(name, "/dev/", strlen("/dev/")) == 0)
			name += strlen("/dev/");
		pp = g_provider_by_name(name);
		if (pp == NULL) {
			G_STRIPE_DEBUG(1, "Disk %s is invalid.", name);
			gctl_error(req, "Disk %s is invalid.", name);
			return;
		}
	}

	gp = g_stripe_create(mp, &md, G_STRIPE_TYPE_MANUAL);
	if (gp == NULL) {
		gctl_error(req, "Can't configure %s.stripe.", md.md_name);
		return;
	}

	sc = gp->softc;
	sb = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND);
	sbuf_printf(sb, "Can't attach disk(s) to %s:", gp->name);
	for (attached = 0, no = 1; no < *nargs; no++) {
		snprintf(param, sizeof(param), "arg%u", no);
		name = gctl_get_asciiparam(req, param);
		if (strncmp(name, "/dev/", strlen("/dev/")) == 0)
			name += strlen("/dev/");
		pp = g_provider_by_name(name);
		KASSERT(pp != NULL, ("Provider %s disappear?!", name));
		if (g_stripe_add_disk(sc, pp, no - 1) != 0) {
			G_STRIPE_DEBUG(1, "Disk %u (%s) not attached to %s.",
			    no, pp->name, gp->name);
			sbuf_printf(sb, " %s", pp->name);
			continue;
		}
		attached++;
	}
	sbuf_finish(sb);
	if (md.md_all != attached) {
		g_stripe_destroy(gp->softc, 1);
		gctl_error(req, "%s", sbuf_data(sb));
	}
	sbuf_delete(sb);
}

static struct g_stripe_softc *
g_stripe_find_device(struct g_class *mp, const char *name)
{
	struct g_stripe_softc *sc;
	struct g_geom *gp;

	LIST_FOREACH(gp, &mp->geom, geom) {
		sc = gp->softc;
		if (sc == NULL)
			continue;
		if (strcmp(gp->name, name) == 0 ||
		    strcmp(sc->sc_name, name) == 0) {
			return (sc);
		}
	}
	return (NULL);
}

static void
g_stripe_ctl_destroy(struct gctl_req *req, struct g_class *mp)
{
	struct g_stripe_softc *sc;
	int *force, *nargs, error;
	const char *name;
	char param[16];
	u_int i;

	g_topology_assert();

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs <= 0) {
		gctl_error(req, "Missing device(s).");
		return;
	}
	force = gctl_get_paraml(req, "force", sizeof(*force));
	if (force == NULL) {
		gctl_error(req, "No '%s' argument.", "force");
		return;
	}

	for (i = 0; i < (u_int)*nargs; i++) {
		snprintf(param, sizeof(param), "arg%u", i);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%u' argument.", i);
			return;
		}
		sc = g_stripe_find_device(mp, name);
		if (sc == NULL) {
			gctl_error(req, "No such device: %s.", name);
			return;
		}
		error = g_stripe_destroy(sc, *force);
		if (error != 0) {
			gctl_error(req, "Cannot destroy device %s (error=%d).",
			    sc->sc_geom->name, error);
			return;
		}
	}
}

static void
g_stripe_config(struct gctl_req *req, struct g_class *mp, const char *verb)
{
	uint32_t *version;

	g_topology_assert();

	version = gctl_get_paraml(req, "version", sizeof(*version));
	if (version == NULL) {
		gctl_error(req, "No '%s' argument.", "version");
		return;
	}
	if (*version != G_STRIPE_VERSION) {
		gctl_error(req, "Userland and kernel parts are out of sync.");
		return;
	}

	if (strcmp(verb, "create") == 0) {
		g_stripe_ctl_create(req, mp);
		return;
	} else if (strcmp(verb, "destroy") == 0) {
		g_stripe_ctl_destroy(req, mp);
		return;
	}

	gctl_error(req, "Unknown verb.");
}

static void
g_stripe_dumpconf(struct sbuf *sb, const char *indent, struct g_geom *gp,
    struct g_consumer *cp, struct g_provider *pp)
{
	struct g_stripe_softc *sc;

	sc = gp->softc;
	if (sc == NULL)
		return;
	if (gp != NULL) {
		sbuf_printf(sb, "%s<id>%u</id>\n", indent, (u_int)sc->sc_id);
		sbuf_printf(sb, "%s<stripesize>%u</stripesize>\n", indent,
		    (u_int)sc->sc_stripesize);
		switch (sc->sc_type) {
		case G_STRIPE_TYPE_AUTOMATIC:
			sbuf_printf(sb, "%s<type>%s</type>\n", indent,
			    "automatic");
			break;
		case G_STRIPE_TYPE_MANUAL:
			sbuf_printf(sb, "%s<type>%s</type>\n", indent,
			    "manual");
			break;
		default:
			sbuf_printf(sb, "%s<type>%s</type>\n", indent,
			    "unknown");
			break;
		}
	}
}

DECLARE_GEOM_CLASS(g_stripe_class, g_stripe);
