/*-
 * Copyright (c) 2004, 2005 Lukas Ertl
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
#include <sys/bio.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sbuf.h>
#include <sys/systm.h>
#include <sys/time.h>

#include <geom/geom.h>
#include <geom/vinum/geom_vinum_var.h>
#include <geom/vinum/geom_vinum.h>
#include <geom/vinum/geom_vinum_share.h>

static void	gv_drive_worker(void *);
void	gv_drive_modify(struct gv_drive *);

void
gv_config_new_drive(struct gv_drive *d)
{
	struct gv_hdr *vhdr;
	struct gv_freelist *fl;

	KASSERT(d != NULL, ("config_new_drive: NULL d"));

	vhdr = g_malloc(sizeof(*vhdr), M_WAITOK | M_ZERO);
	vhdr->magic = GV_MAGIC;
	vhdr->config_length = GV_CFG_LEN;

	bcopy(hostname, vhdr->label.sysname, GV_HOSTNAME_LEN);
	strncpy(vhdr->label.name, d->name, GV_MAXDRIVENAME);
	microtime(&vhdr->label.date_of_birth);

	d->hdr = vhdr;

	LIST_INIT(&d->subdisks);
	LIST_INIT(&d->freelist);

	fl = g_malloc(sizeof(struct gv_freelist), M_WAITOK | M_ZERO);
	fl->offset = GV_DATA_START;
	fl->size = d->avail;
	LIST_INSERT_HEAD(&d->freelist, fl, freelist);
	d->freelist_entries = 1;

	TAILQ_INIT(&d->bqueue);
	mtx_init(&d->bqueue_mtx, "gv_drive", NULL, MTX_DEF);
	kthread_create(gv_drive_worker, d, NULL, 0, 0, "gv_d %s", d->name);
	d->flags |= GV_DRIVE_THREAD_ACTIVE;
}

void
gv_save_config_all(struct gv_softc *sc)
{
	struct gv_drive *d;

	g_topology_assert();

	LIST_FOREACH(d, &sc->drives, drive) {
		if (d->geom == NULL)
			continue;
		gv_save_config(NULL, d, sc);
	}
}

/* Save the vinum configuration back to disk. */
void
gv_save_config(struct g_consumer *cp, struct gv_drive *d, struct gv_softc *sc)
{
	struct g_geom *gp;
	struct g_consumer *cp2;
	struct gv_hdr *vhdr, *hdr;
	struct sbuf *sb;
	int error;

	g_topology_assert();

	KASSERT(d != NULL, ("gv_save_config: null d"));
	KASSERT(sc != NULL, ("gv_save_config: null sc"));

	if (cp == NULL) {
		gp = d->geom;
		KASSERT(gp != NULL, ("gv_save_config: null gp"));
		cp2 = LIST_FIRST(&gp->consumer);
		KASSERT(cp2 != NULL, ("gv_save_config: null cp2"));
	} else
		cp2 = cp;

	vhdr = g_malloc(GV_HDR_LEN, M_WAITOK | M_ZERO);
	vhdr->magic = GV_MAGIC;
	vhdr->config_length = GV_CFG_LEN;

	hdr = d->hdr;
	if (hdr == NULL) {
		printf("NULL hdr!!!\n");
		g_free(vhdr);
		return;
	}
	microtime(&hdr->label.last_update);
	bcopy(&hdr->label, &vhdr->label, sizeof(struct gv_label));

	sb = sbuf_new(NULL, NULL, GV_CFG_LEN, SBUF_FIXEDLEN);
	gv_format_config(sc, sb, 1, NULL);
	sbuf_finish(sb);

	error = g_access(cp2, 0, 1, 0);
	if (error) {
		printf("g_access failed: %d\n", error);
		sbuf_delete(sb);
		return;
	}
	g_topology_unlock();

	do {
		error = g_write_data(cp2, GV_HDR_OFFSET, vhdr, GV_HDR_LEN);
		if (error) {
			printf("writing vhdr failed: %d", error);
			break;
		}

		error = g_write_data(cp2, GV_CFG_OFFSET, sbuf_data(sb),
		    GV_CFG_LEN);
		if (error) {
			printf("writing first config copy failed: %d", error);
			break;
		}
		
		error = g_write_data(cp2, GV_CFG_OFFSET + GV_CFG_LEN,
		    sbuf_data(sb), GV_CFG_LEN);
		if (error)
			printf("writing second config copy failed: %d", error);
	} while (0);

	g_topology_lock();
	g_access(cp2, 0, -1, 0);
	sbuf_delete(sb);
	g_free(vhdr);

	if (d->geom != NULL)
		gv_drive_modify(d);
}

/* This resembles g_slice_access(). */
static int
gv_drive_access(struct g_provider *pp, int dr, int dw, int de)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	struct g_provider *pp2;
	struct gv_drive *d;
	struct gv_sd *s, *s2;
	int error;

	gp = pp->geom;
	cp = LIST_FIRST(&gp->consumer);
	if (cp == NULL)
		return (0);

	d = gp->softc;

	s = pp->private;
	KASSERT(s != NULL, ("gv_drive_access: NULL s"));

	LIST_FOREACH(s2, &d->subdisks, from_drive) {
		if (s == s2)
			continue;
		if (s->drive_offset + s->size <= s2->drive_offset)
			continue;
		if (s2->drive_offset + s2->size <= s->drive_offset)
			continue;

		/* Overlap. */
		pp2 = s2->provider;
		KASSERT(s2 != NULL, ("gv_drive_access: NULL s2"));
		if ((pp->acw + dw) > 0 && pp2->ace > 0) {
			printf("FOOO: permission denied - e\n");
			return (EPERM);
		}
		if ((pp->ace + de) > 0 && pp2->acw > 0) {
			printf("FOOO: permission denied - w\n");
			return (EPERM);
		}
	}

#if 0
	/* On first open, grab an extra "exclusive" bit */
	if (cp->acr == 0 && cp->acw == 0 && cp->ace == 0)
		de++;
	/* ... and let go of it on last close */
	if ((cp->acr + dr) == 0 && (cp->acw + dw) == 0 && (cp->ace + de) == 1)
		de--;
#endif
	error = g_access(cp, dr, dw, de);
	if (error) {
		printf("FOOO: g_access failed: %d\n", error);
	}
	return (error);
}

static void
gv_drive_done(struct bio *bp)
{
	struct gv_drive *d;
	struct gv_bioq *bq;

	/* Put the BIO on the worker queue again. */
	d = bp->bio_from->geom->softc;
	bp->bio_cflags |= GV_BIO_DONE;
	bq = g_malloc(sizeof(*bq), M_NOWAIT | M_ZERO);
	bq->bp = bp;
	mtx_lock(&d->bqueue_mtx);
	TAILQ_INSERT_TAIL(&d->bqueue, bq, queue);
	wakeup(d);
	mtx_unlock(&d->bqueue_mtx);
}


static void
gv_drive_start(struct bio *bp)
{
	struct gv_drive *d;
	struct gv_sd *s;
	struct gv_bioq *bq;

	switch (bp->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
	case BIO_DELETE:
		break;
	case BIO_GETATTR:
	default:
		g_io_deliver(bp, EOPNOTSUPP);
		return;
	}

	s = bp->bio_to->private;
	if ((s->state == GV_SD_DOWN) || (s->state == GV_SD_STALE)) {
		g_io_deliver(bp, ENXIO);
		return;
	}

	d = bp->bio_to->geom->softc;

	/*
	 * Put the BIO on the worker queue, where the worker thread will pick
	 * it up.
	 */
	bq = g_malloc(sizeof(*bq), M_NOWAIT | M_ZERO);
	bq->bp = bp;
	mtx_lock(&d->bqueue_mtx);
	TAILQ_INSERT_TAIL(&d->bqueue, bq, queue);
	wakeup(d);
	mtx_unlock(&d->bqueue_mtx);

}

static void
gv_drive_worker(void *arg)
{
	struct bio *bp, *cbp;
	struct g_geom *gp;
	struct g_provider *pp;
	struct gv_drive *d;
	struct gv_sd *s;
	struct gv_bioq *bq, *bq2;
	int error;

	d = arg;

	mtx_lock(&d->bqueue_mtx);
	for (;;) {
		/* We were signaled to exit. */
		if (d->flags & GV_DRIVE_THREAD_DIE)
			break;

		/* Take the first BIO from out queue. */
		bq = TAILQ_FIRST(&d->bqueue);
		if (bq == NULL) {
			msleep(d, &d->bqueue_mtx, PRIBIO, "-", hz/10);
			continue;
 		}
		TAILQ_REMOVE(&d->bqueue, bq, queue);
		mtx_unlock(&d->bqueue_mtx);
 
		bp = bq->bp;
		g_free(bq);
		pp = bp->bio_to;
		gp = pp->geom;

		/* Completed request. */
		if (bp->bio_cflags & GV_BIO_DONE) {
			error = bp->bio_error;

			/* Deliver the original request. */
			g_std_done(bp);

			/* The request had an error, we need to clean up. */
			if (error != 0) {
				g_topology_lock();
				gv_set_drive_state(d, GV_DRIVE_DOWN,
				    GV_SETSTATE_FORCE | GV_SETSTATE_CONFIG);
				g_wither_geom(d->geom, ENXIO);
				g_topology_unlock();
			}

		/* New request, needs to be sent downwards. */
		} else {
			s = pp->private;

			if ((s->state == GV_SD_DOWN) ||
			    (s->state == GV_SD_STALE)) {
				g_io_deliver(bp, ENXIO);
				mtx_lock(&d->bqueue_mtx);
				continue;
			}
			if (bp->bio_offset > s->size) {
				g_io_deliver(bp, EINVAL);
				mtx_lock(&d->bqueue_mtx);
				continue;
			}

			cbp = g_clone_bio(bp);
			if (cbp == NULL) {
				g_io_deliver(bp, ENOMEM);
				mtx_lock(&d->bqueue_mtx);
				continue;
			}
			if (cbp->bio_offset + cbp->bio_length > s->size)
				cbp->bio_length = s->size -
				    cbp->bio_offset;
			cbp->bio_done = gv_drive_done;
			cbp->bio_offset += s->drive_offset;
			g_io_request(cbp, LIST_FIRST(&gp->consumer));
		}

		mtx_lock(&d->bqueue_mtx);
	}

	TAILQ_FOREACH_SAFE(bq, &d->bqueue, queue, bq2) {
		TAILQ_REMOVE(&d->bqueue, bq, queue);
		mtx_unlock(&d->bqueue_mtx);
		bp = bq->bp;
		g_free(bq);
		if (bp->bio_cflags & GV_BIO_DONE) 
			g_std_done(bp);
		else
			g_io_deliver(bp, ENXIO);
		mtx_lock(&d->bqueue_mtx);
	}
	mtx_unlock(&d->bqueue_mtx);
	d->flags |= GV_DRIVE_THREAD_DEAD;

	kthread_exit(ENXIO);
}


static void
gv_drive_orphan(struct g_consumer *cp)
{
	struct g_geom *gp;
	struct gv_drive *d;
	struct gv_sd *s;
	int error;

	g_topology_assert();
	gp = cp->geom;
	g_trace(G_T_TOPOLOGY, "gv_drive_orphan(%s)", gp->name);
	if (cp->acr != 0 || cp->acw != 0 || cp->ace != 0)
		g_access(cp, -cp->acr, -cp->acw, -cp->ace);
	error = cp->provider->error;
	if (error == 0)
		error = ENXIO;
	g_detach(cp);
	g_destroy_consumer(cp);	
	if (!LIST_EMPTY(&gp->consumer))
		return;
	d = gp->softc;
	if (d != NULL) {
		printf("gvinum: lost drive '%s'\n", d->name);
		d->geom = NULL;
		LIST_FOREACH(s, &d->subdisks, from_drive) {
			s->provider = NULL;
			s->consumer = NULL;
		}
		gv_kill_drive_thread(d);
		gv_set_drive_state(d, GV_DRIVE_DOWN,
		    GV_SETSTATE_FORCE | GV_SETSTATE_CONFIG);
	}
	gp->softc = NULL;
	g_wither_geom(gp, error);
}

static struct g_geom *
gv_drive_taste(struct g_class *mp, struct g_provider *pp, int flags __unused)
{
	struct g_geom *gp, *gp2;
	struct g_consumer *cp;
	struct gv_drive *d;
	struct gv_sd *s;
	struct gv_softc *sc;
	struct gv_freelist *fl;
	struct gv_hdr *vhdr;
	int error;
	char *buf, errstr[ERRBUFSIZ];

	vhdr = NULL;
	d = NULL;

	g_trace(G_T_TOPOLOGY, "gv_drive_taste(%s, %s)", mp->name, pp->name);
	g_topology_assert();

	if (pp->sectorsize == 0)
		return(NULL);

	/* Find the VINUM class and its associated geom. */
	gp2 = find_vinum_geom();
	if (gp2 == NULL)
		return (NULL);
	sc = gp2->softc;

	gp = g_new_geomf(mp, "%s.vinumdrive", pp->name);
	gp->start = gv_drive_start;
	gp->orphan = gv_drive_orphan;
	gp->access = gv_drive_access;
	gp->start = gv_drive_start;

	cp = g_new_consumer(gp);
	g_attach(cp, pp);
	error = g_access(cp, 1, 0, 0);
	if (error) {
		g_detach(cp);
		g_destroy_consumer(cp);
		g_destroy_geom(gp);
		return (NULL);
	}

	g_topology_unlock();

	/* Now check if the provided slice is a valid vinum drive. */
	do {
		vhdr = g_read_data(cp, GV_HDR_OFFSET, pp->sectorsize, &error);
		if (vhdr == NULL || error != 0)
			break;
		if (vhdr->magic != GV_MAGIC) {
			g_free(vhdr);
			break;
		}

		/*
		 * We have found a valid vinum drive.  Let's see if it is
		 * already known in the configuration.  There's a chance that
		 * the VINUMDRIVE class tastes before the VINUM class could
		 * taste, so parse the configuration here too, just to be on
		 * the safe side.
		 */
		buf = g_read_data(cp, GV_CFG_OFFSET, GV_CFG_LEN, &error);
		if (buf == NULL || error != 0) {
			g_free(vhdr);
			break;
		}
		g_topology_lock();
		gv_parse_config(sc, buf, 1);
		g_free(buf);

		d = gv_find_drive(sc, vhdr->label.name);

		/* We already know about this drive. */
		if (d != NULL) {
			/* Check if this drive already has a geom. */
			if (d->geom != NULL) {
				g_topology_unlock();
				break;
			}
			bcopy(vhdr, d->hdr, sizeof(*vhdr));

		/* This is a new drive. */
		} else {
			d = g_malloc(sizeof(*d), M_WAITOK | M_ZERO);

			/* Initialize all needed variables. */
			d->size = pp->mediasize - GV_DATA_START;
			d->avail = d->size;
			d->hdr = vhdr;
			strncpy(d->name, vhdr->label.name, GV_MAXDRIVENAME);
			LIST_INIT(&d->subdisks);
			LIST_INIT(&d->freelist);

			/* We also need a freelist entry. */
			fl = g_malloc(sizeof(*fl), M_WAITOK | M_ZERO);
			fl->offset = GV_DATA_START;
			fl->size = d->avail;
			LIST_INSERT_HEAD(&d->freelist, fl, freelist);
			d->freelist_entries = 1;

			TAILQ_INIT(&d->bqueue);

			/* Save it into the main configuration. */
			LIST_INSERT_HEAD(&sc->drives, d, drive);
		}

		/*
		 * Create a bio queue mutex and a worker thread, if necessary.
		 */
		if (mtx_initialized(&d->bqueue_mtx) == 0)
			mtx_init(&d->bqueue_mtx, "gv_drive", NULL, MTX_DEF);

		if (!(d->flags & GV_DRIVE_THREAD_ACTIVE)) {
			kthread_create(gv_drive_worker, d, NULL, 0, 0,
			    "gv_d %s", d->name);
			d->flags |= GV_DRIVE_THREAD_ACTIVE;
		}

		g_access(cp, -1, 0, 0);

		gp->softc = d;
		d->geom = gp;
		d->vinumconf = sc;
		strncpy(d->device, pp->name, GV_MAXDRIVENAME);

		/*
		 * Find out which subdisks belong to this drive and crosslink
		 * them.
		 */
		LIST_FOREACH(s, &sc->subdisks, sd) {
			if (!strncmp(s->drive, d->name, GV_MAXDRIVENAME))
				/* XXX: errors ignored */
				gv_sd_to_drive(sc, d, s, errstr,
				    sizeof(errstr));
		}

		/* This drive is now up for sure. */
		gv_set_drive_state(d, GV_DRIVE_UP, 0);

		/*
		 * If there are subdisks on this drive, we need to create
		 * providers for them.
		 */ 
		if (d->sdcount)
			gv_drive_modify(d);

		return (gp);

	} while (0);

	g_topology_lock();
	g_access(cp, -1, 0, 0);

	g_detach(cp);
	g_destroy_consumer(cp);
	g_destroy_geom(gp);
	return (NULL);
}

/*
 * Modify the providers for the given drive 'd'.  It is assumed that the
 * subdisk list of 'd' is already correctly set up.
 */
void
gv_drive_modify(struct gv_drive *d)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	struct g_provider *pp, *pp2;
	struct gv_sd *s;
	int nsd;

	KASSERT(d != NULL, ("gv_drive_modify: null d"));
	gp = d->geom;
	KASSERT(gp != NULL, ("gv_drive_modify: null gp"));
	cp = LIST_FIRST(&gp->consumer);
	KASSERT(cp != NULL, ("gv_drive_modify: null cp"));
	pp = cp->provider;
	KASSERT(pp != NULL, ("gv_drive_modify: null pp"));

	g_topology_assert();

	nsd = 0;
	LIST_FOREACH(s, &d->subdisks, from_drive) {
		/* This subdisk already has a provider. */
		if (s->provider != NULL)
			continue;
		pp2 = g_new_providerf(gp, "gvinum/sd/%s", s->name);
		pp2->mediasize = s->size;
		pp2->sectorsize = pp->sectorsize;
		g_error_provider(pp2, 0);
		s->provider = pp2;
		pp2->private = s;
	}
}

static int
gv_drive_destroy_geom(struct gctl_req *req, struct g_class *mp,
    struct g_geom *gp)
{
	struct gv_drive *d;

	g_trace(G_T_TOPOLOGY, "gv_drive_destroy_geom: %s", gp->name);
	g_topology_assert();

	d = gp->softc;
	gv_kill_drive_thread(d);

	g_wither_geom(gp, ENXIO);
	return (0);
}

#define	VINUMDRIVE_CLASS_NAME "VINUMDRIVE"

static struct g_class g_vinum_drive_class = {
	.name = VINUMDRIVE_CLASS_NAME,
	.version = G_VERSION,
	.taste = gv_drive_taste,
	.destroy_geom = gv_drive_destroy_geom
};

DECLARE_GEOM_CLASS(g_vinum_drive_class, g_vinum_drive);
