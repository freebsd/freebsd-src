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
#include <geom/concat/g_concat.h>


static MALLOC_DEFINE(M_CONCAT, "concat data", "GEOM_CONCAT Data");

SYSCTL_DECL(_kern_geom);
SYSCTL_NODE(_kern_geom, OID_AUTO, concat, CTLFLAG_RW, 0, "GEOM_CONCAT stuff");
static u_int g_concat_debug = 0;
SYSCTL_UINT(_kern_geom_concat, OID_AUTO, debug, CTLFLAG_RW, &g_concat_debug, 0,
    "Debug level");

static int g_concat_destroy(struct g_concat_softc *sc, boolean_t force);
static int g_concat_destroy_geom(struct gctl_req *req, struct g_class *mp,
    struct g_geom *gp);

static g_taste_t g_concat_taste;
static g_ctl_req_t g_concat_config;
static g_dumpconf_t g_concat_dumpconf;

struct g_class g_concat_class = {
	.name = G_CONCAT_CLASS_NAME,
	.ctlreq = g_concat_config,
	.taste = g_concat_taste,
	.destroy_geom = g_concat_destroy_geom
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
g_concat_nvalid(struct g_concat_softc *sc)
{
	u_int i, no;

	no = 0;
	for (i = 0; i < sc->sc_ndisks; i++) {
		if (sc->sc_disks[i].d_consumer != NULL)
			no++;
	}

	return (no);
}

static void
g_concat_remove_disk(struct g_concat_disk *disk)
{
	struct g_consumer *cp;
	struct g_concat_softc *sc;

	KASSERT(disk->d_consumer != NULL, ("Non-valid disk in %s.", __func__));
	sc = disk->d_softc;
	cp = disk->d_consumer;

	G_CONCAT_DEBUG(1, "Removing disk %s from %s.", cp->provider->name,
	    sc->sc_provider->name);

	disk->d_consumer = NULL;

	g_error_provider(sc->sc_provider, ENXIO);

	G_CONCAT_DEBUG(0, "Disk %s removed from %s.", cp->provider->name,
	    sc->sc_provider->name);

	if (cp->acr > 0 || cp->acw > 0 || cp->ace > 0)
		g_access(cp, -cp->acr, -cp->acw, -cp->ace);
	g_detach(cp);
	g_destroy_consumer(cp);
}

static void
g_concat_orphan(struct g_consumer *cp)
{
	struct g_concat_softc *sc;
	struct g_concat_disk *disk;
	struct g_geom *gp;

	g_topology_assert();
	gp = cp->geom;
	sc = gp->softc;
	if (sc == NULL)
		return;

	disk = cp->private;
	if (disk == NULL)	/* Possible? */
		return;
	g_concat_remove_disk(disk);

	if (sc->sc_type == G_CONCAT_TYPE_MANUAL) {
		/*
		 * For manually configured devices, don't remove provider
		 * even is there are no vaild disks at all.
		 */
		return;
	}

	/* If there are no valid disks anymore, remove device. */
	if (g_concat_nvalid(sc) == 0)
		g_concat_destroy(sc, 1);
}

static int
g_concat_access(struct g_provider *pp, int dr, int dw, int de)
{
	struct g_consumer *cp1, *cp2;
	struct g_concat_softc *sc;
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
			G_CONCAT_DEBUG(0, "Device %s definitely destroyed.",
			    gp->name);
		}
		return (0);
	}

	/* On first open, grab an extra "exclusive" bit */
	if (pp->acr == 0 && pp->acw == 0 && pp->ace == 0)
		de++;
	/* ... and let go of it on last close */
	if ((pp->acr + dr) == 0 && (pp->acw + dw) == 0 && (pp->ace + de) == 1)
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
g_concat_start(struct bio *bp)
{
	struct g_concat_softc *sc;
	struct g_concat_disk *disk;
	struct g_provider *pp;
	off_t offset, end, length, off, len;
	struct bio *cbp;
	char *addr;
	u_int no;

	pp = bp->bio_to;
	sc = pp->geom->softc;
	/*
	 * If sc == NULL, provider's error should be set and g_concat_start()
	 * should not be called at all.
	 */
	KASSERT(sc != NULL,
	    ("Provider's error should be set (error=%d)(device=%s).",
	    bp->bio_to->error, bp->bio_to->name));

	G_CONCAT_LOGREQ(bp, "Request received.");

	switch (bp->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
	case BIO_DELETE:
		break;
	case BIO_GETATTR:
		/* To which provider it should be delivered? */
	default:
		g_io_deliver(bp, EOPNOTSUPP);
		return;
	}

	offset = bp->bio_offset;
	length = bp->bio_length;
	addr = bp->bio_data;
	end = offset + length;

	for (no = 0; no < sc->sc_ndisks; no++) {
		disk = &sc->sc_disks[no];
		if (disk->d_end <= offset)
			continue;
		if (disk->d_start >= end)
			break;

		off = offset - disk->d_start;
		len = MIN(length, disk->d_end - offset);
		length -= len;
		offset += len;

		cbp = g_clone_bio(bp);
		if (cbp == NULL) {
			if (bp->bio_error == 0)
				bp->bio_error = ENOMEM;
			return;
		}
		/*
		 * Fill in the component buf structure.
		 */
		cbp->bio_done = g_std_done;
		cbp->bio_offset = off;
		cbp->bio_data = addr;
		addr += len;
		cbp->bio_length = len;
		cbp->bio_to = disk->d_consumer->provider;
		G_CONCAT_LOGREQ(cbp, "Sending request.");
		g_io_request(cbp, disk->d_consumer);

		if (length == 0)
			break;
	}

	KASSERT(length == 0,
	    ("Length is still greater than 0 (class=%s, name=%s).",
	    bp->bio_to->geom->class->name, bp->bio_to->geom->name));
}

static void
g_concat_check_and_run(struct g_concat_softc *sc)
{
	struct g_concat_disk *disk;
	u_int no, sectorsize = 0;
	off_t start;

	if (g_concat_nvalid(sc) != sc->sc_ndisks)
		return;

	start = 0;
	for (no = 0; no < sc->sc_ndisks; no++) {
		disk = &sc->sc_disks[no];
		disk->d_start = start;
		disk->d_end = disk->d_start +
		    disk->d_consumer->provider->mediasize;
		if (sc->sc_type == G_CONCAT_TYPE_AUTOMATIC)
			disk->d_end -= disk->d_consumer->provider->sectorsize;
		start = disk->d_end;
		if (no == 0)
			sectorsize = disk->d_consumer->provider->sectorsize;
		else {
			sectorsize = lcm(sectorsize,
			    disk->d_consumer->provider->sectorsize);
		}
	}
	sc->sc_provider->sectorsize = sectorsize;
	/* We have sc->sc_disks[sc->sc_ndisks - 1].d_end in 'start'. */
	sc->sc_provider->mediasize = start;
	g_error_provider(sc->sc_provider, 0);

	G_CONCAT_DEBUG(0, "Device %s activated.", sc->sc_provider->name);
}

static int
g_concat_read_metadata(struct g_consumer *cp, struct g_concat_metadata *md)
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
	concat_metadata_decode(buf, md);
	g_free(buf);

	return (0);
}

/*
 * Add disk to given device.
 */
static int
g_concat_add_disk(struct g_concat_softc *sc, struct g_provider *pp, u_int no)
{
	struct g_concat_disk *disk;
	struct g_provider *ourpp;
	struct g_consumer *cp;
	struct g_geom *gp;
	int error;

	/* Metadata corrupted? */
	if (no >= sc->sc_ndisks)
		return (EINVAL);

	disk = &sc->sc_disks[no];
	/* Check if disk is not already attached. */
	if (disk->d_consumer != NULL)
		return (EEXIST);

	ourpp = sc->sc_provider;
	gp = ourpp->geom;

	cp = g_new_consumer(gp);
	error = g_attach(cp, pp);
	if (error != 0) {
		g_destroy_consumer(cp);
		return (error);
	}

	if (ourpp->acr > 0 || ourpp->acw > 0 || ourpp->ace > 0) {
		error = g_access(cp, ourpp->acr, ourpp->acw, ourpp->ace);
		if (error != 0) {
			g_detach(cp);
			g_destroy_consumer(cp);
			return (error);
		}
	}
	if (sc->sc_type == G_CONCAT_TYPE_AUTOMATIC) {
		struct g_concat_metadata md;

		/* Re-read metadata. */
		error = g_concat_read_metadata(cp, &md);
		if (error != 0)
			goto fail;

		if (strcmp(md.md_magic, G_CONCAT_MAGIC) != 0 ||
		    strcmp(md.md_name, sc->sc_name) != 0 ||
		    md.md_id != sc->sc_id) {
			G_CONCAT_DEBUG(0, "Metadata on %s changed.", pp->name);
			goto fail;
		}
	}

	cp->private = disk;
	disk->d_consumer = cp;
	disk->d_softc = sc;
	disk->d_start = 0;	/* not yet */
	disk->d_end = 0;	/* not yet */

	G_CONCAT_DEBUG(0, "Disk %s attached to %s.", pp->name, gp->name);

	g_concat_check_and_run(sc);

	return (0);
fail:
	if (ourpp->acr > 0 || ourpp->acw > 0 || ourpp->ace > 0)
		g_access(cp, -ourpp->acr, -ourpp->acw, -ourpp->ace);
	g_detach(cp);
	g_destroy_consumer(cp);
	return (error);
}

static struct g_geom *
g_concat_create(struct g_class *mp, const struct g_concat_metadata *md,
    u_int type)
{
	struct g_provider *pp;
	struct g_concat_softc *sc;
	struct g_geom *gp;
	u_int no;

	G_CONCAT_DEBUG(1, "Creating device %s.concat (id=%u).", md->md_name,
	    md->md_id);

	/* Two disks is minimum. */
	if (md->md_all <= 1)
		return (NULL);

	/* Check for duplicate unit */
	LIST_FOREACH(gp, &mp->geom, geom) {
		sc = gp->softc;
		if (sc != NULL && strcmp(sc->sc_name, md->md_name) == 0) {
			G_CONCAT_DEBUG(0, "Device %s already cofigured.",
			    gp->name);
			return (NULL);
		}
	}
	gp = g_new_geomf(mp, "%s.concat", md->md_name);
	gp->softc = NULL;	/* for a moment */

	sc = malloc(sizeof(*sc), M_CONCAT, M_NOWAIT | M_ZERO);
	if (sc == NULL) {
		G_CONCAT_DEBUG(0, "Can't allocate memory for device %s.",
		    gp->name);
		g_destroy_geom(gp);
		return (NULL);
	}

	gp->start = g_concat_start;
	gp->spoiled = g_concat_orphan;
	gp->orphan = g_concat_orphan;
	gp->access = g_concat_access;
	gp->dumpconf = g_concat_dumpconf;

	strlcpy(sc->sc_name, md->md_name, sizeof(sc->sc_name));
	sc->sc_id = md->md_id;
	sc->sc_ndisks = md->md_all;
	sc->sc_disks = malloc(sizeof(struct g_concat_disk) * sc->sc_ndisks,
	    M_CONCAT, M_WAITOK | M_ZERO);
	for (no = 0; no < sc->sc_ndisks; no++)
		sc->sc_disks[no].d_consumer = NULL;
	sc->sc_type = type;

	gp->softc = sc;

	pp = g_new_providerf(gp, "%s", gp->name);
	sc->sc_provider = pp;
	/*
	 * Don't run provider yet (by setting its error to 0), because we're
	 * not aware of its media and sector size.
	 */

	G_CONCAT_DEBUG(0, "Device %s created (id=%u).", gp->name, sc->sc_id);

	return (gp);
}

static int
g_concat_destroy(struct g_concat_softc *sc, boolean_t force)
{
	struct g_provider *pp;
	struct g_geom *gp;
	u_int no;

	g_topology_assert();

	if (sc == NULL)
		return (ENXIO);

	pp = sc->sc_provider;
	if (pp->acr != 0 || pp->acw != 0 || pp->ace != 0) {
		if (force) {
			G_CONCAT_DEBUG(0, "Device %s is still open, so it "
			    "can't be definitely removed.",
			    sc->sc_provider->name);
		} else {
			G_CONCAT_DEBUG(1,
			    "Device %s is still open (r%dw%de%d).",
			    sc->sc_provider->name, pp->acr, pp->acw, pp->ace);
			return (EBUSY);
		}
	}

	g_error_provider(pp, ENXIO);

	for (no = 0; no < sc->sc_ndisks; no++) {
		if (sc->sc_disks[no].d_consumer != NULL)
			g_concat_remove_disk(&sc->sc_disks[no]);
	}

	if (pp->acr == 0 && pp->acw == 0 && pp->ace == 0)
		G_CONCAT_DEBUG(0, "Device %s removed.", pp->name);

	gp = sc->sc_provider->geom;
	gp->softc = NULL;
	free(sc->sc_disks, M_CONCAT);
	free(sc, M_CONCAT);

	g_wither_geom(gp, ENXIO);

	return (0);
}

static int
g_concat_destroy_geom(struct gctl_req *req __unused,
    struct g_class *mp __unused, struct g_geom *gp)
{
	struct g_concat_softc *sc;

	sc = gp->softc;
	return (g_concat_destroy(sc, 0));
}

static struct g_geom *
g_concat_taste(struct g_class *mp, struct g_provider *pp, int flags __unused)
{
	struct g_concat_metadata md;
	struct g_concat_softc *sc;
	struct g_consumer *cp;
	struct g_geom *gp;
	int error;

	g_trace(G_T_TOPOLOGY, "g_concat_taste(%s, %s)", mp->name, pp->name);
	g_topology_assert();

	G_CONCAT_DEBUG(3, "Tasting %s.", pp->name);

	gp = g_new_geomf(mp, "concat:taste");
	gp->start = g_concat_start;
	gp->access = g_concat_access;
	gp->orphan = g_concat_orphan;
	cp = g_new_consumer(gp);
	g_attach(cp, pp);

	error = g_concat_read_metadata(cp, &md);
	if (error != 0) {
		g_wither_geom(gp, ENXIO);
		return (NULL);
	}
	g_wither_geom(gp, ENXIO);
	gp = NULL;

	if (strcmp(md.md_magic, G_CONCAT_MAGIC) != 0)
		return (NULL);
	if (md.md_version > G_CONCAT_VERSION) {
		printf("geom_concat.ko module is too old to handle %s.\n",
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
		if (sc->sc_type != G_CONCAT_TYPE_AUTOMATIC)
			continue;
		if (strcmp(md.md_name, sc->sc_name) != 0)
			continue;
		if (md.md_id != sc->sc_id)
			continue;
		break;
	}
	if (gp != NULL) {
		G_CONCAT_DEBUG(1, "Adding disk %s to %s.", pp->name, gp->name);
		error = g_concat_add_disk(sc, pp, md.md_no);
		if (error != 0) {
			G_CONCAT_DEBUG(0, "Cannot add disk %s to %s.", pp->name,
			    gp->name);
			return (NULL);
		}
	} else {
		gp = g_concat_create(mp, &md, G_CONCAT_TYPE_AUTOMATIC);
		if (gp == NULL) {
			G_CONCAT_DEBUG(0, "Cannot create device %s.concat.",
			    md.md_name);
			return (NULL);
		}
		sc = gp->softc;
		G_CONCAT_DEBUG(1, "Adding disk %s to %s.", pp->name, gp->name);
		error = g_concat_add_disk(sc, pp, md.md_no);
		if (error != 0) {
			G_CONCAT_DEBUG(0, "Cannot add disk %s to %s.", pp->name,
			    gp->name);
			g_concat_destroy(sc, 1);
			return (NULL);
		}
	}

	return (gp);
}

static void
g_concat_ctl_create(struct gctl_req *req, struct g_class *mp)
{
	u_int attached, no;
	struct g_concat_metadata *md;
	struct g_provider *pp;
	struct g_concat_softc *sc;
	struct g_geom *gp;
	struct sbuf *sb;
	char buf[20];

	g_topology_assert();
	md = gctl_get_paraml(req, "metadata", sizeof(*md));
	if (md == NULL) {
		gctl_error(req, "No '%s' argument.", "metadata");
		return;
	}
	if (md->md_all <= 1) {
		G_CONCAT_DEBUG(1, "Invalid 'md_all' value (= %d).", md->md_all);
		gctl_error(req, "Invalid 'md_all' value (= %d).", md->md_all);
		return;
	}

	/* Check all providers are valid */
	for (no = 0; no < md->md_all; no++) {
		snprintf(buf, sizeof(buf), "disk%u", no);
		pp = gctl_get_provider(req, buf);
		if (pp == NULL) {
			G_CONCAT_DEBUG(1, "Disk %u is invalid.", no + 1);
			gctl_error(req, "Disk %u is invalid.", no + 1);
			return;
		}
	}

	gp = g_concat_create(mp, md, G_CONCAT_TYPE_MANUAL);
	if (gp == NULL) {
		gctl_error(req, "Can't configure %s.concat.", md->md_name);
		return;
	}

	sc = gp->softc;
	sb = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND);
	sbuf_printf(sb, "Can't attach disk(s) to %s:", gp->name);
	for (attached = no = 0; no < md->md_all; no++) {
		snprintf(buf, sizeof(buf), "disk%u", no);
		pp = gctl_get_provider(req, buf);
		if (g_concat_add_disk(sc, pp, no) != 0) {
			G_CONCAT_DEBUG(1, "Disk %u (%s) not attached to %s.",
			    no + 1, pp->name, gp->name);
			sbuf_printf(sb, " %s", pp->name);
			continue;
		}
		attached++;
	}
	sbuf_finish(sb);
	if (md->md_all != attached) {
		g_concat_destroy(gp->softc, 1);
		gctl_error(req, "%s", sbuf_data(sb));
	}
	sbuf_delete(sb);
}

static void
g_concat_ctl_destroy(struct gctl_req *req, struct g_geom *gp)
{
	struct g_concat_softc *sc;
	int *force, error;

	g_topology_assert();

	force = gctl_get_paraml(req, "force", sizeof(*force));
	if (force == NULL) {
		gctl_error(req, "No '%s' argument.", "force");
		return;
	}
	sc = gp->softc;
	if (sc == NULL) {
		gctl_error(req, "Cannot destroy device %s (not configured).",
		    gp->name);
		return;
	}
	error = g_concat_destroy(sc, *force);
	if (error != 0) {
		gctl_error(req, "Cannot destroy device %s (error=%d).",
		    gp->name, error);
		return;
	}
}

static void
g_concat_config(struct gctl_req *req, struct g_class *mp, const char *verb)
{
	struct g_geom *gp;

	g_topology_assert();

	if (strcmp(verb, "create device") == 0) {
		g_concat_ctl_create(req, mp);
		return;
	}

	gp = gctl_get_geom(req, mp, "geom");
	if (gp == NULL || gp->softc == NULL) {
		gctl_error(req, "unknown device");
		return;
	}
	if (strcmp(verb, "destroy device") == 0) {
		g_concat_ctl_destroy(req, gp);
		return;
	}
	gctl_error(req, "Unknown verb.");
}

static void
g_concat_dumpconf(struct sbuf *sb, const char *indent, struct g_geom *gp,
    struct g_consumer *cp, struct g_provider *pp)
{
	struct g_concat_softc *sc;

	sc = gp->softc;
	if (sc == NULL)
		return;
	if (pp == NULL && cp == NULL) {
		sbuf_printf(sb, "%s<id>%u</id>\n", indent, (u_int)sc->sc_id);
		switch (sc->sc_type) {
		case G_CONCAT_TYPE_AUTOMATIC:
			sbuf_printf(sb, "%s<type>%s</type>\n", indent,
			    "automatic");
			break;
		case G_CONCAT_TYPE_MANUAL:
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

DECLARE_GEOM_CLASS(g_concat_class, g_concat);
