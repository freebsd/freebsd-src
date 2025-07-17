/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2004-2005 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/bio.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <geom/geom.h>
#include <geom/geom_dbg.h>
#include <geom/concat/g_concat.h>

FEATURE(geom_concat, "GEOM concatenation support");

static MALLOC_DEFINE(M_CONCAT, "concat_data", "GEOM_CONCAT Data");

SYSCTL_DECL(_kern_geom);
static SYSCTL_NODE(_kern_geom, OID_AUTO, concat, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "GEOM_CONCAT stuff");
static u_int g_concat_debug = 0;
SYSCTL_UINT(_kern_geom_concat, OID_AUTO, debug, CTLFLAG_RWTUN, &g_concat_debug, 0,
    "Debug level");

static int g_concat_destroy(struct g_concat_softc *sc, boolean_t force);
static int g_concat_destroy_geom(struct gctl_req *req, struct g_class *mp,
    struct g_geom *gp);

static g_taste_t g_concat_taste;
static g_ctl_req_t g_concat_config;
static g_dumpconf_t g_concat_dumpconf;

struct g_class g_concat_class = {
	.name = G_CONCAT_CLASS_NAME,
	.version = G_VERSION,
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
	u_int no;
	struct g_concat_disk *disk;

	sx_assert(&sc->sc_disks_lock, SA_LOCKED);

	no = 0;
	TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
		if (disk->d_consumer != NULL)
			no++;
	}

	return (no);
}

static void
g_concat_remove_disk(struct g_concat_disk *disk)
{
	struct g_consumer *cp;
	struct g_concat_softc *sc;

	g_topology_assert();
	KASSERT(disk->d_consumer != NULL, ("Non-valid disk in %s.", __func__));
	sc = disk->d_softc;
	cp = disk->d_consumer;

	if (!disk->d_removed) {
		G_CONCAT_DEBUG(0, "Disk %s removed from %s.",
		    cp->provider->name, sc->sc_name);
		disk->d_removed = 1;
	}

	if (sc->sc_provider != NULL) {
		G_CONCAT_DEBUG(0, "Device %s deactivated.",
		    sc->sc_provider->name);
		g_wither_provider(sc->sc_provider, ENXIO);
		sc->sc_provider = NULL;
	}

	if (cp->acr > 0 || cp->acw > 0 || cp->ace > 0)
		return;
	disk->d_consumer = NULL;
	g_detach(cp);
	g_destroy_consumer(cp);
	/* If there are no valid disks anymore, remove device. */
	if (LIST_EMPTY(&sc->sc_geom->consumer))
		g_concat_destroy(sc, 1);
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
}

static int
g_concat_access(struct g_provider *pp, int dr, int dw, int de)
{
	struct g_consumer *cp1, *cp2, *tmp;
	struct g_concat_disk *disk;
	struct g_geom *gp;
	struct g_concat_softc *sc;
	int error;

	g_topology_assert();
	gp = pp->geom;
	sc = gp->softc;

	/* On first open, grab an extra "exclusive" bit */
	if (pp->acr == 0 && pp->acw == 0 && pp->ace == 0)
		de++;
	/* ... and let go of it on last close */
	if ((pp->acr + dr) == 0 && (pp->acw + dw) == 0 && (pp->ace + de) == 0)
		de--;

	sx_slock(&sc->sc_disks_lock);
	LIST_FOREACH_SAFE(cp1, &gp->consumer, consumer, tmp) {
		error = g_access(cp1, dr, dw, de);
		if (error != 0)
			goto fail;
		disk = cp1->private;
		if (cp1->acr == 0 && cp1->acw == 0 && cp1->ace == 0 &&
		    disk->d_removed) {
			g_concat_remove_disk(disk); /* May destroy geom. */
		}
	}
	sx_sunlock(&sc->sc_disks_lock);
	return (0);

fail:
	sx_sunlock(&sc->sc_disks_lock);
	LIST_FOREACH(cp2, &gp->consumer, consumer) {
		if (cp1 == cp2)
			break;
		g_access(cp2, -dr, -dw, -de);
	}
	return (error);
}

static void
g_concat_candelete(struct bio *bp)
{
	struct g_concat_softc *sc;
	struct g_concat_disk *disk;
	int val;

	sc = bp->bio_to->geom->softc;
	sx_assert(&sc->sc_disks_lock, SX_LOCKED);
	TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
		if (!disk->d_removed && disk->d_candelete)
			break;
	}
	val = disk != NULL;
	g_handleattr(bp, "GEOM::candelete", &val, sizeof(val));
}

static void
g_concat_kernel_dump(struct bio *bp)
{
	struct g_concat_softc *sc;
	struct g_concat_disk *disk;
	struct bio *cbp;
	struct g_kerneldump *gkd;

	sc = bp->bio_to->geom->softc;
	gkd = (struct g_kerneldump *)bp->bio_data;
	TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
		if (disk->d_start <= gkd->offset &&
		    disk->d_end > gkd->offset)
			break;
	}
	if (disk == NULL) {
		g_io_deliver(bp, EOPNOTSUPP);
		return;
	}

	gkd->offset -= disk->d_start;
	if (gkd->length > disk->d_end - disk->d_start - gkd->offset)
		gkd->length = disk->d_end - disk->d_start - gkd->offset;
	cbp = g_clone_bio(bp);
	if (cbp == NULL) {
		g_io_deliver(bp, ENOMEM);
		return;
	}
	cbp->bio_done = g_std_done;
	g_io_request(cbp, disk->d_consumer);
	G_CONCAT_DEBUG(1, "Kernel dump will go to %s.",
	    disk->d_consumer->provider->name);
}

static void
g_concat_done(struct bio *bp)
{
	struct g_concat_softc *sc;
	struct bio *pbp;

	pbp = bp->bio_parent;
	sc = pbp->bio_to->geom->softc;
	mtx_lock(&sc->sc_completion_lock);
	if (pbp->bio_error == 0)
		pbp->bio_error = bp->bio_error;
	pbp->bio_completed += bp->bio_completed;
	pbp->bio_inbed++;
	if (pbp->bio_children == pbp->bio_inbed) {
		mtx_unlock(&sc->sc_completion_lock);
		g_io_deliver(pbp, pbp->bio_error);
	} else
		mtx_unlock(&sc->sc_completion_lock);
	g_destroy_bio(bp);
}

/*
 * Called for both BIO_FLUSH and BIO_SPEEDUP. Just pass the call down
 */
static void
g_concat_passdown(struct g_concat_softc *sc, struct bio *bp)
{
	struct bio_queue_head queue;
	struct g_consumer *cp;
	struct bio *cbp;
	struct g_concat_disk *disk;

	sx_assert(&sc->sc_disks_lock, SX_LOCKED);

	bioq_init(&queue);
	TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
		cbp = g_clone_bio(bp);
		if (cbp == NULL) {
			while ((cbp = bioq_takefirst(&queue)) != NULL)
				g_destroy_bio(cbp);
			if (bp->bio_error == 0)
				bp->bio_error = ENOMEM;
			g_io_deliver(bp, bp->bio_error);
			return;
		}
		bioq_insert_tail(&queue, cbp);
		cbp->bio_done = g_concat_done;
		cbp->bio_caller1 = disk->d_consumer;
		cbp->bio_to = disk->d_consumer->provider;
	}
	while ((cbp = bioq_takefirst(&queue)) != NULL) {
		G_CONCAT_LOGREQ(cbp, "Sending request.");
		cp = cbp->bio_caller1;
		cbp->bio_caller1 = NULL;
		g_io_request(cbp, cp);
	}
}

static void
g_concat_start(struct bio *bp)
{
	struct bio_queue_head queue;
	struct g_concat_softc *sc;
	struct g_concat_disk *disk;
	struct g_provider *pp;
	off_t offset, end, length, off, len;
	struct bio *cbp;
	char *addr;

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
	sx_slock(&sc->sc_disks_lock);

	switch (bp->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
	case BIO_DELETE:
		break;
	case BIO_SPEEDUP:
	case BIO_FLUSH:
		g_concat_passdown(sc, bp);
		goto end;
	case BIO_GETATTR:
		if (strcmp("GEOM::kerneldump", bp->bio_attribute) == 0) {
			g_concat_kernel_dump(bp);
			goto end;
		} else if (strcmp("GEOM::candelete", bp->bio_attribute) == 0) {
			g_concat_candelete(bp);
			goto end;
		}
		/* To which provider it should be delivered? */
		/* FALLTHROUGH */
	default:
		g_io_deliver(bp, EOPNOTSUPP);
		goto end;
	}

	offset = bp->bio_offset;
	length = bp->bio_length;
	if ((bp->bio_flags & BIO_UNMAPPED) != 0)
		addr = NULL;
	else
		addr = bp->bio_data;
	end = offset + length;

	bioq_init(&queue);
	TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
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
			while ((cbp = bioq_takefirst(&queue)) != NULL)
				g_destroy_bio(cbp);
			if (bp->bio_error == 0)
				bp->bio_error = ENOMEM;
			g_io_deliver(bp, bp->bio_error);
			goto end;
		}
		bioq_insert_tail(&queue, cbp);
		/*
		 * Fill in the component buf structure.
		 */
		if (len == bp->bio_length)
			cbp->bio_done = g_std_done;
		else
			cbp->bio_done = g_concat_done;
		cbp->bio_offset = off;
		cbp->bio_length = len;
		if ((bp->bio_flags & BIO_UNMAPPED) != 0) {
			cbp->bio_ma_offset += (uintptr_t)addr;
			cbp->bio_ma += cbp->bio_ma_offset / PAGE_SIZE;
			cbp->bio_ma_offset %= PAGE_SIZE;
			cbp->bio_ma_n = round_page(cbp->bio_ma_offset +
			    cbp->bio_length) / PAGE_SIZE;
		} else
			cbp->bio_data = addr;
		addr += len;
		cbp->bio_to = disk->d_consumer->provider;
		cbp->bio_caller1 = disk;

		if (length == 0)
			break;
	}
	KASSERT(length == 0,
	    ("Length is still greater than 0 (class=%s, name=%s).",
	    bp->bio_to->geom->class->name, bp->bio_to->geom->name));
	while ((cbp = bioq_takefirst(&queue)) != NULL) {
		G_CONCAT_LOGREQ(cbp, "Sending request.");
		disk = cbp->bio_caller1;
		cbp->bio_caller1 = NULL;
		g_io_request(cbp, disk->d_consumer);
	}
end:
	sx_sunlock(&sc->sc_disks_lock);
}

static void
g_concat_check_and_run(struct g_concat_softc *sc)
{
	struct g_concat_disk *disk;
	struct g_provider *dp, *pp;
	u_int sectorsize = 0;
	off_t start;
	int error;

	g_topology_assert();
	if (g_concat_nvalid(sc) != sc->sc_ndisks)
		return;

	pp = g_new_providerf(sc->sc_geom, "concat/%s", sc->sc_name);
	pp->flags |= G_PF_DIRECT_SEND | G_PF_DIRECT_RECEIVE |
	    G_PF_ACCEPT_UNMAPPED;
	start = 0;
	TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
		dp = disk->d_consumer->provider;
		disk->d_start = start;
		disk->d_end = disk->d_start + dp->mediasize;
		if (sc->sc_type == G_CONCAT_TYPE_AUTOMATIC)
			disk->d_end -= dp->sectorsize;
		start = disk->d_end;
		error = g_access(disk->d_consumer, 1, 0, 0);
		if (error == 0) {
			error = g_getattr("GEOM::candelete", disk->d_consumer,
			    &disk->d_candelete);
			if (error != 0)
				disk->d_candelete = 0;
			(void)g_access(disk->d_consumer, -1, 0, 0);
		} else
			G_CONCAT_DEBUG(1, "Failed to access disk %s, error %d.",
			    dp->name, error);
		if (disk == TAILQ_FIRST(&sc->sc_disks))
			sectorsize = dp->sectorsize;
		else
			sectorsize = lcm(sectorsize, dp->sectorsize);

		/* A provider underneath us doesn't support unmapped */
		if ((dp->flags & G_PF_ACCEPT_UNMAPPED) == 0) {
			G_CONCAT_DEBUG(1, "Cancelling unmapped "
			    "because of %s.", dp->name);
			pp->flags &= ~G_PF_ACCEPT_UNMAPPED;
		}
	}
	pp->sectorsize = sectorsize;
	/* We have sc->sc_disks[sc->sc_ndisks - 1].d_end in 'start'. */
	pp->mediasize = start;
	dp = TAILQ_FIRST(&sc->sc_disks)->d_consumer->provider;
	pp->stripesize = dp->stripesize;
	pp->stripeoffset = dp->stripeoffset;
	sc->sc_provider = pp;
	g_error_provider(pp, 0);

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
	struct g_consumer *cp, *fcp;
	struct g_geom *gp;
	int error;

	g_topology_assert();

	sx_slock(&sc->sc_disks_lock);

	/* Metadata corrupted? */
	if (no >= sc->sc_ndisks) {
		sx_sunlock(&sc->sc_disks_lock);
		return (EINVAL);
	}

	for (disk = TAILQ_FIRST(&sc->sc_disks); no > 0; no--) {
		disk = TAILQ_NEXT(disk, d_next);
	}

	/* Check if disk is not already attached. */
	if (disk->d_consumer != NULL) {
		sx_sunlock(&sc->sc_disks_lock);
		return (EEXIST);
	}

	gp = sc->sc_geom;
	fcp = LIST_FIRST(&gp->consumer);

	cp = g_new_consumer(gp);
	cp->flags |= G_CF_DIRECT_SEND | G_CF_DIRECT_RECEIVE;
	error = g_attach(cp, pp);
	if (error != 0) {
		sx_sunlock(&sc->sc_disks_lock);
		g_destroy_consumer(cp);
		return (error);
	}

	if (fcp != NULL && (fcp->acr > 0 || fcp->acw > 0 || fcp->ace > 0)) {
		error = g_access(cp, fcp->acr, fcp->acw, fcp->ace);
		if (error != 0) {
			sx_sunlock(&sc->sc_disks_lock);
			g_detach(cp);
			g_destroy_consumer(cp);
			return (error);
		}
	}
	if (sc->sc_type == G_CONCAT_TYPE_AUTOMATIC) {
		struct g_concat_metadata md;

		// temporarily give up the lock to avoid lock order violation
		// due to topology unlock in g_concat_read_metadata
		sx_sunlock(&sc->sc_disks_lock);
		/* Re-read metadata. */
		error = g_concat_read_metadata(cp, &md);
		sx_slock(&sc->sc_disks_lock);

		if (error != 0)
			goto fail;

		if (strcmp(md.md_magic, G_CONCAT_MAGIC) != 0 ||
		    strcmp(md.md_name, sc->sc_name) != 0 ||
		    md.md_id != sc->sc_id) {
			G_CONCAT_DEBUG(0, "Metadata on %s changed.", pp->name);
			goto fail;
		}

		disk->d_hardcoded = md.md_provider[0] != '\0';
	} else {
		disk->d_hardcoded = false;
	}

	cp->private = disk;
	disk->d_consumer = cp;
	disk->d_softc = sc;
	disk->d_start = 0;	/* not yet */
	disk->d_end = 0;	/* not yet */
	disk->d_removed = 0;

	G_CONCAT_DEBUG(0, "Disk %s attached to %s.", pp->name, sc->sc_name);

	g_concat_check_and_run(sc);
	sx_sunlock(&sc->sc_disks_lock); // need lock for check_and_run

	return (0);
fail:
	sx_sunlock(&sc->sc_disks_lock);
	if (fcp != NULL && (fcp->acr > 0 || fcp->acw > 0 || fcp->ace > 0))
		g_access(cp, -fcp->acr, -fcp->acw, -fcp->ace);
	g_detach(cp);
	g_destroy_consumer(cp);
	return (error);
}

static struct g_geom *
g_concat_create(struct g_class *mp, const struct g_concat_metadata *md,
    u_int type)
{
	struct g_concat_softc *sc;
	struct g_concat_disk *disk;
	struct g_geom *gp;
	u_int no;

	G_CONCAT_DEBUG(1, "Creating device %s (id=%u).", md->md_name,
	    md->md_id);

	/* One disks is minimum. */
	if (md->md_all < 1)
		return (NULL);

	/* Check for duplicate unit */
	LIST_FOREACH(gp, &mp->geom, geom) {
		sc = gp->softc;
		if (sc != NULL && strcmp(sc->sc_name, md->md_name) == 0) {
			G_CONCAT_DEBUG(0, "Device %s already configured.",
			    gp->name);
			return (NULL);
		}
	}
	gp = g_new_geomf(mp, "%s", md->md_name);
	sc = malloc(sizeof(*sc), M_CONCAT, M_WAITOK | M_ZERO);
	gp->start = g_concat_start;
	gp->spoiled = g_concat_orphan;
	gp->orphan = g_concat_orphan;
	gp->access = g_concat_access;
	gp->dumpconf = g_concat_dumpconf;

	sc->sc_id = md->md_id;
	sc->sc_ndisks = md->md_all;
	TAILQ_INIT(&sc->sc_disks);
	for (no = 0; no < sc->sc_ndisks; no++) {
		disk = malloc(sizeof(*disk), M_CONCAT, M_WAITOK | M_ZERO);
		TAILQ_INSERT_TAIL(&sc->sc_disks, disk, d_next);
	}
	sc->sc_type = type;
	mtx_init(&sc->sc_completion_lock, "gconcat lock", NULL, MTX_DEF);
	sx_init(&sc->sc_disks_lock, "gconcat append lock");

	gp->softc = sc;
	sc->sc_geom = gp;
	sc->sc_provider = NULL;

	G_CONCAT_DEBUG(0, "Device %s created (id=%u).", sc->sc_name, sc->sc_id);

	return (gp);
}

static int
g_concat_destroy(struct g_concat_softc *sc, boolean_t force)
{
	struct g_provider *pp;
	struct g_consumer *cp, *cp1;
	struct g_geom *gp;
	struct g_concat_disk *disk;

	g_topology_assert();

	if (sc == NULL)
		return (ENXIO);

	pp = sc->sc_provider;
	if (pp != NULL && (pp->acr != 0 || pp->acw != 0 || pp->ace != 0)) {
		if (force) {
			G_CONCAT_DEBUG(0, "Device %s is still open, so it "
			    "can't be definitely removed.", pp->name);
		} else {
			G_CONCAT_DEBUG(1,
			    "Device %s is still open (r%dw%de%d).", pp->name,
			    pp->acr, pp->acw, pp->ace);
			return (EBUSY);
		}
	}

	gp = sc->sc_geom;
	LIST_FOREACH_SAFE(cp, &gp->consumer, consumer, cp1) {
		g_concat_remove_disk(cp->private);
		if (cp1 == NULL)
			return (0);	/* Recursion happened. */
	}
	if (!LIST_EMPTY(&gp->consumer))
		return (EINPROGRESS);

	gp->softc = NULL;
	KASSERT(sc->sc_provider == NULL, ("Provider still exists? (device=%s)",
	    gp->name));
	while ((disk = TAILQ_FIRST(&sc->sc_disks)) != NULL) {
		TAILQ_REMOVE(&sc->sc_disks, disk, d_next);
		free(disk, M_CONCAT);
	}
	mtx_destroy(&sc->sc_completion_lock);
	sx_destroy(&sc->sc_disks_lock);
	free(sc, M_CONCAT);

	G_CONCAT_DEBUG(0, "Device %s destroyed.", gp->name);
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

	g_trace(G_T_TOPOLOGY, "%s(%s, %s)", __func__, mp->name, pp->name);
	g_topology_assert();

	/* Skip providers that are already open for writing. */
	if (pp->acw > 0)
		return (NULL);

	G_CONCAT_DEBUG(3, "Tasting %s.", pp->name);

	gp = g_new_geomf(mp, "concat:taste");
	gp->start = g_concat_start;
	gp->access = g_concat_access;
	gp->orphan = g_concat_orphan;
	cp = g_new_consumer(gp);
	cp->flags |= G_CF_DIRECT_SEND | G_CF_DIRECT_RECEIVE;
	error = g_attach(cp, pp);
	if (error == 0) {
		error = g_concat_read_metadata(cp, &md);
		g_detach(cp);
	}
	g_destroy_consumer(cp);
	g_destroy_geom(gp);
	if (error != 0)
		return (NULL);
	gp = NULL;

	if (strcmp(md.md_magic, G_CONCAT_MAGIC) != 0)
		return (NULL);
	if (md.md_version > G_CONCAT_VERSION) {
		printf("geom_concat.ko module is too old to handle %s.\n",
		    pp->name);
		return (NULL);
	}
	/*
	 * Backward compatibility:
	 */
	/* There was no md_provider field in earlier versions of metadata. */
	if (md.md_version < 3)
		bzero(md.md_provider, sizeof(md.md_provider));
	/* There was no md_provsize field in earlier versions of metadata. */
	if (md.md_version < 4)
		md.md_provsize = pp->mediasize;

	if (md.md_provider[0] != '\0' &&
	    !g_compare_names(md.md_provider, pp->name))
		return (NULL);
	if (md.md_provsize != pp->mediasize)
		return (NULL);

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
			G_CONCAT_DEBUG(0,
			    "Cannot add disk %s to %s (error=%d).", pp->name,
			    gp->name, error);
			return (NULL);
		}
	} else {
		gp = g_concat_create(mp, &md, G_CONCAT_TYPE_AUTOMATIC);
		if (gp == NULL) {
			G_CONCAT_DEBUG(0, "Cannot create device %s.",
			    md.md_name);
			return (NULL);
		}
		sc = gp->softc;
		G_CONCAT_DEBUG(1, "Adding disk %s to %s.", pp->name, gp->name);
		error = g_concat_add_disk(sc, pp, md.md_no);
		if (error != 0) {
			G_CONCAT_DEBUG(0,
			    "Cannot add disk %s to %s (error=%d).", pp->name,
			    gp->name, error);
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
	struct g_concat_metadata md;
	struct g_provider *pp;
	struct g_concat_softc *sc;
	struct g_geom *gp;
	struct sbuf *sb;
	const char *name;
	char param[16];
	int *nargs;

	g_topology_assert();
	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs < 2) {
		gctl_error(req, "Too few arguments.");
		return;
	}

	bzero(&md, sizeof(md));
	strlcpy(md.md_magic, G_CONCAT_MAGIC, sizeof(md.md_magic));
	md.md_version = G_CONCAT_VERSION;
	name = gctl_get_asciiparam(req, "arg0");
	if (name == NULL) {
		gctl_error(req, "No 'arg%u' argument.", 0);
		return;
	}
	strlcpy(md.md_name, name, sizeof(md.md_name));
	md.md_id = arc4random();
	md.md_no = 0;
	md.md_all = *nargs - 1;
	/* This field is not important here. */
	md.md_provsize = 0;

	/* Check all providers are valid */
	for (no = 1; no < *nargs; no++) {
		snprintf(param, sizeof(param), "arg%u", no);
		pp = gctl_get_provider(req, param);
		if (pp == NULL)
			return;
	}

	gp = g_concat_create(mp, &md, G_CONCAT_TYPE_MANUAL);
	if (gp == NULL) {
		gctl_error(req, "Can't configure %s.", md.md_name);
		return;
	}

	sc = gp->softc;
	sb = sbuf_new_auto();
	sbuf_printf(sb, "Can't attach disk(s) to %s:", gp->name);
	for (attached = 0, no = 1; no < *nargs; no++) {
		snprintf(param, sizeof(param), "arg%u", no);
		pp = gctl_get_provider(req, param);
		if (pp == NULL) {
			name = gctl_get_asciiparam(req, param);
			MPASS(name != NULL);
			sbuf_printf(sb, " %s", name);
			continue;
		}
		if (g_concat_add_disk(sc, pp, no - 1) != 0) {
			G_CONCAT_DEBUG(1, "Disk %u (%s) not attached to %s.",
			    no, pp->name, gp->name);
			sbuf_printf(sb, " %s", pp->name);
			continue;
		}
		attached++;
	}
	sbuf_finish(sb);
	if (md.md_all != attached) {
		g_concat_destroy(gp->softc, 1);
		gctl_error(req, "%s", sbuf_data(sb));
	}
	sbuf_delete(sb);
}

static struct g_concat_softc *
g_concat_find_device(struct g_class *mp, const char *name)
{
	struct g_concat_softc *sc;
	struct g_geom *gp;

	if (strncmp(name, _PATH_DEV, strlen(_PATH_DEV)) == 0)
		name += strlen(_PATH_DEV);

	LIST_FOREACH(gp, &mp->geom, geom) {
		sc = gp->softc;
		if (sc == NULL)
			continue;
		if (strcmp(sc->sc_name, name) == 0)
			return (sc);
	}
	return (NULL);
}

static void
g_concat_ctl_destroy(struct gctl_req *req, struct g_class *mp)
{
	struct g_concat_softc *sc;
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
		sc = g_concat_find_device(mp, name);
		if (sc == NULL) {
			gctl_error(req, "No such device: %s.", name);
			return;
		}
		error = g_concat_destroy(sc, *force);
		if (error != 0) {
			gctl_error(req, "Cannot destroy device %s (error=%d).",
			    sc->sc_name, error);
			return;
		}
	}
}

static struct g_concat_disk *
g_concat_find_disk(struct g_concat_softc *sc, const char *name)
{
	struct g_concat_disk *disk;

	sx_assert(&sc->sc_disks_lock, SX_LOCKED);
	if (strncmp(name, "/dev/", 5) == 0)
		name += 5;
	TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
		if (disk->d_consumer == NULL)
			continue;
		if (disk->d_consumer->provider == NULL)
			continue;
		if (strcmp(disk->d_consumer->provider->name, name) == 0)
			return (disk);
	}
	return (NULL);
}

static void
g_concat_write_metadata(struct gctl_req *req, struct g_concat_softc *sc)
{
	u_int no = 0;
	struct g_concat_disk *disk;
	struct g_concat_metadata md;
	struct g_provider *pp;
	u_char *sector;
	int error;

	bzero(&md, sizeof(md));
	strlcpy(md.md_magic, G_CONCAT_MAGIC, sizeof(md.md_magic));
	md.md_version = G_CONCAT_VERSION;
	strlcpy(md.md_name, sc->sc_name, sizeof(md.md_name));
	md.md_id = sc->sc_id;
	md.md_all = sc->sc_ndisks;
	TAILQ_FOREACH(disk, &sc->sc_disks, d_next) {
		pp = disk->d_consumer->provider;

		md.md_no = no;
		if (disk->d_hardcoded)
			strlcpy(md.md_provider, pp->name,
			    sizeof(md.md_provider));
		md.md_provsize = disk->d_consumer->provider->mediasize;

		sector = g_malloc(pp->sectorsize, M_WAITOK | M_ZERO);
		concat_metadata_encode(&md, sector);
		error = g_access(disk->d_consumer, 0, 1, 0);
		if (error == 0) {
			error = g_write_data(disk->d_consumer,
			    pp->mediasize - pp->sectorsize, sector,
			    pp->sectorsize);
			(void)g_access(disk->d_consumer, 0, -1, 0);
		}
		g_free(sector);
		if (error != 0)
			gctl_error(req, "Cannot store metadata on %s: %d",
			    pp->name, error);

		no++;
	}
}

static void
g_concat_ctl_append(struct gctl_req *req, struct g_class *mp)
{
	struct g_concat_softc *sc;
	struct g_consumer *cp, *fcp;
	struct g_provider *pp;
	struct g_geom *gp;
	const char *name, *cname;
	struct g_concat_disk *disk;
	int *nargs, *hardcode;
	int error;
	int disk_candelete;

	g_topology_assert();

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs != 2) {
		gctl_error(req, "Invalid number of arguments.");
		return;
	}
	hardcode = gctl_get_paraml(req, "hardcode", sizeof(*hardcode));
	if (hardcode == NULL) {
		gctl_error(req, "No '%s' argument.", "hardcode");
		return;
	}

	cname = gctl_get_asciiparam(req, "arg0");
	if (cname == NULL) {
		gctl_error(req, "No 'arg%u' argument.", 0);
		return;
	}
	sc = g_concat_find_device(mp, cname);
	if (sc == NULL) {
		gctl_error(req, "No such device: %s.", cname);
		return;
	}
	if (sc->sc_provider == NULL) {
		/*
		 * this won't race with g_concat_remove_disk as both
		 * are holding the topology lock
		 */
		gctl_error(req, "Device not active, can't append: %s.", cname);
		return;
	}
	G_CONCAT_DEBUG(1, "Appending to %s:", cname);
	sx_xlock(&sc->sc_disks_lock);
	gp = sc->sc_geom;
	fcp = LIST_FIRST(&gp->consumer);

	name = gctl_get_asciiparam(req, "arg1");
	if (name == NULL) {
		gctl_error(req, "No 'arg%u' argument.", 1);
		goto fail;
	}
	if (strncmp(name, "/dev/", strlen("/dev/")) == 0)
		name += strlen("/dev/");
	pp = g_provider_by_name(name);
	if (pp == NULL) {
		G_CONCAT_DEBUG(1, "Disk %s is invalid.", name);
		gctl_error(req, "Disk %s is invalid.", name);
		goto fail;
	}
	G_CONCAT_DEBUG(1, "Appending %s to this", name);

	if (g_concat_find_disk(sc, name) != NULL) {
		gctl_error(req, "Disk %s already appended.", name);
		goto fail;
	}

	if ((sc->sc_provider->sectorsize % pp->sectorsize) != 0) {
		gctl_error(req, "Providers sectorsize mismatch: %u vs %u",
			   sc->sc_provider->sectorsize, pp->sectorsize);
		goto fail;
	}

	cp = g_new_consumer(gp);
	cp->flags |= G_CF_DIRECT_SEND | G_CF_DIRECT_RECEIVE;
	error = g_attach(cp, pp);
	if (error != 0) {
		g_destroy_consumer(cp);
		gctl_error(req, "Cannot open device %s (error=%d).",
		    name, error);
		goto fail;
	}

	error = g_access(cp, 1, 0, 0);
	if (error == 0) {
		error = g_getattr("GEOM::candelete", cp, &disk_candelete);
		if (error != 0)
			disk_candelete = 0;
		(void)g_access(cp, -1, 0, 0);
	} else
		G_CONCAT_DEBUG(1, "Failed to access disk %s, error %d.", name, error);

	/* invoke g_access exactly as deep as all the other members currently are */
	if (fcp != NULL && (fcp->acr > 0 || fcp->acw > 0 || fcp->ace > 0)) {
		error = g_access(cp, fcp->acr, fcp->acw, fcp->ace);
		if (error != 0) {
			g_detach(cp);
			g_destroy_consumer(cp);
			gctl_error(req, "Failed to access disk %s (error=%d).", name, error);
			goto fail;
		}
	}

	disk = malloc(sizeof(*disk), M_CONCAT, M_WAITOK | M_ZERO);
	disk->d_consumer = cp;
	disk->d_softc = sc;
	disk->d_start = TAILQ_LAST(&sc->sc_disks, g_concat_disks)->d_end;
	disk->d_end = disk->d_start + cp->provider->mediasize;
	disk->d_candelete = disk_candelete;
	disk->d_removed = 0;
	disk->d_hardcoded = *hardcode;
	cp->private = disk;
	TAILQ_INSERT_TAIL(&sc->sc_disks, disk, d_next);
	sc->sc_ndisks++;

	if (sc->sc_type == G_CONCAT_TYPE_AUTOMATIC) {
		/* last sector is for metadata */
		disk->d_end -= cp->provider->sectorsize;

		/* update metadata on all parts */
		g_concat_write_metadata(req, sc);
	}

	g_resize_provider(sc->sc_provider, disk->d_end);

fail:
	sx_xunlock(&sc->sc_disks_lock);
}

static void
g_concat_config(struct gctl_req *req, struct g_class *mp, const char *verb)
{
	uint32_t *version;

	g_topology_assert();

	version = gctl_get_paraml(req, "version", sizeof(*version));
	if (version == NULL) {
		gctl_error(req, "No '%s' argument.", "version");
		return;
	}
	if (*version != G_CONCAT_VERSION) {
		gctl_error(req, "Userland and kernel parts are out of sync.");
		return;
	}

	if (strcmp(verb, "create") == 0) {
		g_concat_ctl_create(req, mp);
		return;
	} else if (strcmp(verb, "destroy") == 0 ||
	    strcmp(verb, "stop") == 0) {
		g_concat_ctl_destroy(req, mp);
		return;
	} else if (strcmp(verb, "append") == 0) {
		g_concat_ctl_append(req, mp);
		return;
	}
	gctl_error(req, "Unknown verb.");
}

static void
g_concat_dumpconf(struct sbuf *sb, const char *indent, struct g_geom *gp,
    struct g_consumer *cp, struct g_provider *pp)
{
	struct g_concat_softc *sc;

	g_topology_assert();
	sc = gp->softc;
	if (sc == NULL)
		return;

	sx_slock(&sc->sc_disks_lock);
	if (pp != NULL) {
		/* Nothing here. */
	} else if (cp != NULL) {
		struct g_concat_disk *disk;

		disk = cp->private;
		if (disk == NULL)
			goto end;
		sbuf_printf(sb, "%s<End>%jd</End>\n", indent,
		    (intmax_t)disk->d_end);
		sbuf_printf(sb, "%s<Start>%jd</Start>\n", indent,
		    (intmax_t)disk->d_start);
	} else {
		sbuf_printf(sb, "%s<ID>%u</ID>\n", indent, (u_int)sc->sc_id);
		sbuf_printf(sb, "%s<Type>", indent);
		switch (sc->sc_type) {
		case G_CONCAT_TYPE_AUTOMATIC:
			sbuf_cat(sb, "AUTOMATIC");
			break;
		case G_CONCAT_TYPE_MANUAL:
			sbuf_cat(sb, "MANUAL");
			break;
		default:
			sbuf_cat(sb, "UNKNOWN");
			break;
		}
		sbuf_cat(sb, "</Type>\n");
		sbuf_printf(sb, "%s<Status>Total=%u, Online=%u</Status>\n",
		    indent, sc->sc_ndisks, g_concat_nvalid(sc));
		sbuf_printf(sb, "%s<State>", indent);
		if (sc->sc_provider != NULL && sc->sc_provider->error == 0)
			sbuf_cat(sb, "UP");
		else
			sbuf_cat(sb, "DOWN");
		sbuf_cat(sb, "</State>\n");
	}
end:
	sx_sunlock(&sc->sc_disks_lock);
}

DECLARE_GEOM_CLASS(g_concat_class, g_concat);
MODULE_VERSION(geom_concat, 0);
