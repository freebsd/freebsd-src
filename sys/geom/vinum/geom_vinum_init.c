/*-
 * Copyright (c) 2004 Lukas Ertl
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
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/libkern.h>
#include <sys/malloc.h>
#include <sys/queue.h>

#include <geom/geom.h>
#include <geom/vinum/geom_vinum_var.h>
#include <geom/vinum/geom_vinum.h>
#include <geom/vinum/geom_vinum_share.h>

static int	gv_init_plex(struct gv_plex *);
void	gv_init_td(void *);
static int	gv_rebuild_plex(struct gv_plex *);
void	gv_rebuild_td(void *);
static int	gv_start_plex(struct gv_plex *);
static int	gv_start_vol(struct gv_volume *);
static int	gv_sync(struct gv_volume *);
void	gv_sync_td(void *);

struct gv_sync_args {
	struct gv_volume *v;
	struct gv_plex *from;
	struct gv_plex *to;
	off_t syncsize;
};

void
gv_parityop(struct g_geom *gp, struct gctl_req *req)
{
	struct gv_softc *sc;
	struct gv_plex *p;
	struct bio *bp;
	struct g_consumer *cp;
	int error, *flags, type, *rebuild, rv;
	char *plex;

	rv = -1;

	plex = gctl_get_param(req, "plex", NULL);
	if (plex == NULL) {
		gctl_error(req, "no plex given");
		goto out;
	}

	flags = gctl_get_paraml(req, "flags", sizeof(*flags));
	if (flags == NULL) {
		gctl_error(req, "no flags given");
		goto out;
	}

	rebuild = gctl_get_paraml(req, "rebuild", sizeof(*rebuild));
	if (rebuild == NULL) {
		gctl_error(req, "no rebuild op given");
		goto out;
	}

	sc = gp->softc;
	type = gv_object_type(sc, plex);
	switch (type) {
	case GV_TYPE_PLEX:
		break;
	case GV_TYPE_VOL:
	case GV_TYPE_SD:
	case GV_TYPE_DRIVE:
	default:
		gctl_error(req, "'%s' is not a plex", plex);
		goto out;
	}

	p = gv_find_plex(sc, plex);
	if (p->state != GV_PLEX_UP) {
		gctl_error(req, "plex %s is not completely accessible",
		    p->name);
		goto out;
	}
	if (p->org != GV_PLEX_RAID5) {
		gctl_error(req, "plex %s is not a RAID5 plex", p->name);
		goto out;
	}

	cp = p->consumer;
	error = g_access(cp, 1, 1, 0);
	if (error) {
		gctl_error(req, "cannot access consumer");
		goto out;
	}
	g_topology_unlock();

	/* Reset the check pointer when using -f. */
	if (*flags & GV_FLAG_F)
		p->synced = 0;

	bp = g_new_bio();
	if (bp == NULL) {
		gctl_error(req, "cannot create BIO - out of memory");
		g_topology_lock();
		error = g_access(cp, -1, -1, 0);
		goto out;
	}
	bp->bio_cmd = BIO_WRITE;
	bp->bio_done = NULL;
	bp->bio_data = g_malloc(p->stripesize, M_WAITOK | M_ZERO);
	bp->bio_cflags |= GV_BIO_CHECK;
	if (*rebuild)
		bp->bio_cflags |= GV_BIO_PARITY;
	bp->bio_offset = p->synced;
	bp->bio_length = p->stripesize;

	/* Schedule it down ... */
	g_io_request(bp, cp);

	/* ... and wait for the result. */
	error = biowait(bp, "gwrite");
	g_free(bp->bio_data);
	g_destroy_bio(bp);

	if (error) {
		/* Incorrect parity. */
		if (error == EAGAIN)
			rv = 1;

		/* Some other error happened. */
		else
			gctl_error(req, "Parity check failed at offset 0x%jx, "
			    "errno %d", (intmax_t)p->synced, error);

	/* Correct parity. */
	} else
		rv = 0;

	gctl_set_param(req, "offset", &p->synced, sizeof(p->synced));

	/* Advance the checkpointer if there was no error. */
	if (rv == 0)
		p->synced += p->stripesize;

	/* End of plex; reset the check pointer and signal it to the caller. */
	if (p->synced >= p->size) {
		p->synced = 0;
		rv = -2;
	}

	g_topology_lock();
	error = g_access(cp, -1, -1, 0);

out:
	gctl_set_param(req, "rv", &rv, sizeof(rv));
}

void
gv_start_obj(struct g_geom *gp, struct gctl_req *req)
{
	struct gv_softc *sc;
	struct gv_volume *v;
	struct gv_plex *p;
	int *argc, *initsize;
	char *argv, buf[20];
	int err, i, type;

	argc = gctl_get_paraml(req, "argc", sizeof(*argc));
	initsize = gctl_get_paraml(req, "initsize", sizeof(*initsize));

	if (argc == NULL || *argc == 0) {
		gctl_error(req, "no arguments given");
		return;
	}

	sc = gp->softc;

	for (i = 0; i < *argc; i++) {
		snprintf(buf, sizeof(buf), "argv%d", i);
		argv = gctl_get_param(req, buf, NULL);
		if (argv == NULL)
			continue;
		type = gv_object_type(sc, argv);
		switch (type) {
		case GV_TYPE_VOL:
			v = gv_find_vol(sc, argv);
			err = gv_start_vol(v);
			if (err) {
				if (err == EINPROGRESS) {
					gctl_error(req, "cannot start volume "
					    "'%s': already in progress", argv);
				} else {
					gctl_error(req, "cannot start volume "
					    "'%s'; errno: %d", argv, err);
				}
				return;
			}
			break;

		case GV_TYPE_PLEX:
			p = gv_find_plex(sc, argv);
			err = gv_start_plex(p);
			if (err) {
				if (err == EINPROGRESS) {
					gctl_error(req, "cannot start plex "
					    "'%s': already in progress", argv);
				} else {
					gctl_error(req, "cannot start plex "
					    "'%s'; errno: %d", argv, err);
				}
				return;
			}
			break;

		case GV_TYPE_SD:
		case GV_TYPE_DRIVE:
			/* XXX not yet */
			gctl_error(req, "cannot start '%s' - not yet supported",
			    argv);
			return;
		default:
			gctl_error(req, "unknown object '%s'", argv);
			return;
		}
	}
}

static int
gv_start_plex(struct gv_plex *p)
{
	struct gv_volume *v;
	int error;

	KASSERT(p != NULL, ("gv_start_plex: NULL p"));

	if (p->state == GV_PLEX_UP)
		return (0);

	error = 0;
	v = p->vol_sc;
	if ((v != NULL) && (v->plexcount > 1))
		error = gv_sync(v);
	else if (p->org == GV_PLEX_RAID5) {
		if (p->state == GV_PLEX_DEGRADED)
			error = gv_rebuild_plex(p);
		else
			error = gv_init_plex(p);
	}

	return (error);
}

static int
gv_start_vol(struct gv_volume *v)
{
	struct gv_plex *p;
	struct gv_sd *s;
	int error;

	KASSERT(v != NULL, ("gv_start_vol: NULL v"));

	error = 0;

	if (v->plexcount == 0)
		return (ENXIO);

	else if (v->plexcount == 1) {
		p = LIST_FIRST(&v->plexes);
		KASSERT(p != NULL, ("gv_start_vol: NULL p on %s", v->name));
		if (p->org == GV_PLEX_RAID5) {
			switch (p->state) {
			case GV_PLEX_DOWN:
				error = gv_init_plex(p);
				break;
			case GV_PLEX_DEGRADED:
				error = gv_rebuild_plex(p);
				break;
			default:
				return (0);
			}
		} else {
			LIST_FOREACH(s, &p->subdisks, in_plex) {
				gv_set_sd_state(s, GV_SD_UP,
				    GV_SETSTATE_CONFIG);
			}
		}
	} else
		error = gv_sync(v);

	return (error);
}

static int
gv_sync(struct gv_volume *v)
{
	struct gv_softc *sc;
	struct gv_plex *p, *up;
	struct gv_sync_args *sync;

	KASSERT(v != NULL, ("gv_sync: NULL v"));
	sc = v->vinumconf;
	KASSERT(sc != NULL, ("gv_sync: NULL sc on %s", v->name));

	/* Find the plex that's up. */
	up = NULL;
	LIST_FOREACH(up, &v->plexes, in_volume) {
		if (up->state == GV_PLEX_UP)
			break;
	}

	/* Didn't find a good plex. */
	if (up == NULL)
		return (ENXIO);

	LIST_FOREACH(p, &v->plexes, in_volume) {
		if ((p == up) || (p->state == GV_PLEX_UP))
			continue;
		if (p->flags & GV_PLEX_SYNCING) {
			return (EINPROGRESS);
		}
		p->flags |= GV_PLEX_SYNCING;
		sync = g_malloc(sizeof(*sync), M_WAITOK | M_ZERO);
		sync->v = v;
		sync->from = up;
		sync->to = p;
		sync->syncsize = GV_DFLT_SYNCSIZE;
		kproc_create(gv_sync_td, sync, NULL, 0, 0, "gv_sync '%s'",
		    p->name);
	}

	return (0);
}

static int
gv_rebuild_plex(struct gv_plex *p)
{
	struct gv_sync_args *sync;

	if (gv_is_open(p->geom))
		return (EBUSY);

	if (p->flags & GV_PLEX_SYNCING)
		return (EINPROGRESS);
	p->flags |= GV_PLEX_SYNCING;

	sync = g_malloc(sizeof(*sync), M_WAITOK | M_ZERO);
	sync->to = p;
	sync->syncsize = GV_DFLT_SYNCSIZE;

	kproc_create(gv_rebuild_td, sync, NULL, 0, 0, "gv_rebuild %s",
	    p->name);

	return (0);
}

static int
gv_init_plex(struct gv_plex *p)
{
	struct gv_sd *s;

	KASSERT(p != NULL, ("gv_init_plex: NULL p"));

	LIST_FOREACH(s, &p->subdisks, in_plex) {
		if (s->state == GV_SD_INITIALIZING)
			return (EINPROGRESS);
		gv_set_sd_state(s, GV_SD_INITIALIZING, GV_SETSTATE_FORCE);
		s->init_size = GV_DFLT_SYNCSIZE;
		kproc_create(gv_init_td, s, NULL, 0, 0, "gv_init %s",
		    s->name);
	}

	return (0);
}

/* This thread is responsible for rebuilding a degraded RAID5 plex. */
void
gv_rebuild_td(void *arg)
{
	struct bio *bp;
	struct gv_plex *p;
	struct g_consumer *cp;
	struct gv_sync_args *sync;
	u_char *buf;
	off_t i;
	int error;

	buf = NULL;
	bp = NULL;

	sync = arg;
	p = sync->to;
	p->synced = 0;
	cp = p->consumer;

	g_topology_lock();
	error = g_access(cp, 1, 1, 0);
	if (error) {
		g_topology_unlock();
		G_VINUM_DEBUG(0, "rebuild of %s failed to access consumer: "
		    "%d", p->name, error);
		kproc_exit(error);
	}
	g_topology_unlock();

	buf = g_malloc(sync->syncsize, M_WAITOK);

	G_VINUM_DEBUG(1, "rebuild of %s started", p->name);
	i = 0;
	for (i = 0; i < p->size; i += (p->stripesize * (p->sdcount - 1))) {
/*
		if (i + sync->syncsize > p->size)
			sync->syncsize = p->size - i;
*/
		bp = g_new_bio();
		if (bp == NULL) {
			G_VINUM_DEBUG(0, "rebuild of %s failed creating bio: "
			    "out of memory", p->name);
			break;
		}
		bp->bio_cmd = BIO_WRITE;
		bp->bio_done = NULL;
		bp->bio_data = buf;
		bp->bio_cflags |= GV_BIO_REBUILD;
		bp->bio_offset = i;
		bp->bio_length = p->stripesize;

		/* Schedule it down ... */
		g_io_request(bp, cp);

		/* ... and wait for the result. */
		error = biowait(bp, "gwrite");
		if (error) {
			G_VINUM_DEBUG(0, "rebuild of %s failed at offset %jd "
			    "errno: %d", p->name, i, error);
			break;
		}
		g_destroy_bio(bp);
		bp = NULL;
	}

	if (bp != NULL)
		g_destroy_bio(bp);
	if (buf != NULL)
		g_free(buf);

	g_topology_lock();
	g_access(cp, -1, -1, 0);
	gv_save_config_all(p->vinumconf);
	g_topology_unlock();

	p->flags &= ~GV_PLEX_SYNCING;
	p->synced = 0;

	/* Successful initialization. */
	if (!error)
		G_VINUM_DEBUG(1, "rebuild of %s finished", p->name);

	g_free(sync);
	kproc_exit(error);
}

void
gv_sync_td(void *arg)
{
	struct bio *bp;
	struct gv_plex *p;
	struct g_consumer *from, *to;
	struct gv_sync_args *sync;
	u_char *buf;
	off_t i;
	int error;

	sync = arg;

	from = sync->from->consumer;
	to = sync->to->consumer;

	p = sync->to;
	p->synced = 0;

	error = 0;

	g_topology_lock();
	error = g_access(from, 1, 0, 0);
	if (error) {
		g_topology_unlock();
		G_VINUM_DEBUG(0, "sync from '%s' failed to access "
		    "consumer: %d", sync->from->name, error);
		g_free(sync);
		kproc_exit(error);
	}
	error = g_access(to, 0, 1, 0);
	if (error) {
		g_access(from, -1, 0, 0);
		g_topology_unlock();
		G_VINUM_DEBUG(0, "sync to '%s' failed to access "
		    "consumer: %d", p->name, error);
		g_free(sync);
		kproc_exit(error);
	}
	g_topology_unlock();

	G_VINUM_DEBUG(1, "plex sync %s -> %s started", sync->from->name,
	    sync->to->name);
	for (i = 0; i < p->size; i+= sync->syncsize) {
		/* Read some bits from the good plex. */
		buf = g_read_data(from, i, sync->syncsize, &error);
		if (buf == NULL) {
			G_VINUM_DEBUG(0, "sync read from '%s' failed at "
			    "offset %jd; errno: %d", sync->from->name, i,
			    error);
			break;
		}

		/*
		 * Create a bio and schedule it down on the 'bad' plex.  We
		 * cannot simply use g_write_data() because we have to let the
		 * lower parts know that we are an initialization process and
		 * not a 'normal' request.
		 */
		bp = g_new_bio();
		if (bp == NULL) {
			G_VINUM_DEBUG(0, "sync write to '%s' failed at "
			    "offset %jd; out of memory", p->name, i);
			g_free(buf);
			break;
		}
		bp->bio_cmd = BIO_WRITE;
		bp->bio_offset = i;
		bp->bio_length = sync->syncsize;
		bp->bio_data = buf;
		bp->bio_done = NULL;

		/*
		 * This hack declare this bio as part of an initialization
		 * process, so that the lower levels allow it to get through.
		 */
		bp->bio_cflags |= GV_BIO_SYNCREQ;

		/* Schedule it down ... */
		g_io_request(bp, to);

		/* ... and wait for the result. */
		error = biowait(bp, "gwrite");
		g_destroy_bio(bp);
		g_free(buf);
		if (error) {
			G_VINUM_DEBUG(0, "sync write to '%s' failed at "
			    "offset %jd; errno: %d\n", p->name, i, error);
			break;
		}

		/* Note that we have synced a little bit more. */
		p->synced += sync->syncsize;
	}

	g_topology_lock();
	g_access(from, -1, 0, 0);
	g_access(to, 0, -1, 0);
	gv_save_config_all(p->vinumconf);
	g_topology_unlock();

	/* Successful initialization. */
	if (!error)
		G_VINUM_DEBUG(1, "plex sync %s -> %s finished",
		    sync->from->name, sync->to->name);

	p->flags &= ~GV_PLEX_SYNCING;
	p->synced = 0;

	g_free(sync);
	kproc_exit(error);
}

void
gv_init_td(void *arg)
{
	struct gv_sd *s;
	struct gv_drive *d;
	struct g_geom *gp;
	struct g_consumer *cp;
	int error;
	off_t i, init_size, start, offset, length;
	u_char *buf;

	s = arg;
	KASSERT(s != NULL, ("gv_init_td: NULL s"));
	d = s->drive_sc;
	KASSERT(d != NULL, ("gv_init_td: NULL d"));
	gp = d->geom;
	KASSERT(gp != NULL, ("gv_init_td: NULL gp"));

	cp = LIST_FIRST(&gp->consumer);
	KASSERT(cp != NULL, ("gv_init_td: NULL cp"));

	s->init_error = 0;
	init_size = s->init_size;
	start = s->drive_offset + s->initialized;
	offset = s->drive_offset;
	length = s->size;

	buf = g_malloc(s->init_size, M_WAITOK | M_ZERO);

	g_topology_lock();
	error = g_access(cp, 0, 1, 0);
	if (error) {
		s->init_error = error;
		g_topology_unlock();
		G_VINUM_DEBUG(0, "subdisk '%s' init: failed to access "
		    "consumer; error: %d", s->name, error);
		kproc_exit(error);
	}
	g_topology_unlock();

	for (i = start; i < offset + length; i += init_size) {
		error = g_write_data(cp, i, buf, init_size);
		if (error) {
			G_VINUM_DEBUG(0, "subdisk '%s' init: write failed"
			    " at offset %jd (drive offset %jd); error %d",
			    s->name, (intmax_t)s->initialized, (intmax_t)i,
			    error);
			break;
		}
		s->initialized += init_size;
	}

	g_free(buf);

	g_topology_lock();
	g_access(cp, 0, -1, 0);
	g_topology_unlock();
	if (error) {
		s->init_error = error;
		g_topology_lock();
		gv_set_sd_state(s, GV_SD_STALE,
		    GV_SETSTATE_FORCE | GV_SETSTATE_CONFIG);
		g_topology_unlock();
	} else {
		g_topology_lock();
		gv_set_sd_state(s, GV_SD_UP, GV_SETSTATE_CONFIG);
		g_topology_unlock();
		s->initialized = 0;
		G_VINUM_DEBUG(1, "subdisk '%s' init: finished successfully",
		    s->name);
	}
	kproc_exit(error);
}
