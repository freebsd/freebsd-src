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

int	gv_init_plex(struct gv_plex *);
int	gv_init_sd(struct gv_sd *);
void	gv_init_td(void *);
void	gv_start_plex(struct gv_plex *);
void	gv_start_vol(struct gv_volume *);
void	gv_sync(struct gv_volume *);
void	gv_sync_td(void *);

struct gv_sync_args {
	struct gv_volume *v;
	struct gv_plex *from;
	struct gv_plex *to;
	off_t syncsize;
};

void
gv_start_obj(struct g_geom *gp, struct gctl_req *req)
{
	struct gv_softc *sc;
	struct gv_volume *v;
	struct gv_plex *p;
	int *argc, *initsize;
	char *argv, buf[20];
	int i, type;

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
			gv_start_vol(v);
			break;

		case GV_TYPE_PLEX:
			p = gv_find_plex(sc, argv);
			gv_start_plex(p);
			break;

		case GV_TYPE_SD:
		case GV_TYPE_DRIVE:
			/* XXX not yet */
			gctl_error(req, "cannot start '%s'", argv);
			return;
		default:
			gctl_error(req, "unknown object '%s'", argv);
			return;
		}
	}
}

void
gv_start_plex(struct gv_plex *p)
{
	struct gv_volume *v;

	KASSERT(p != NULL, ("gv_start_plex: NULL p"));

	if (p->state == GV_PLEX_UP)
		return;

	v = p->vol_sc;
	if ((v != NULL) && (v->plexcount > 1))
		gv_sync(v);
	else if (p->org == GV_PLEX_RAID5)
		gv_init_plex(p);

	return;
}

void
gv_start_vol(struct gv_volume *v)
{
	struct gv_plex *p;
	struct gv_sd *s;

	KASSERT(v != NULL, ("gv_start_vol: NULL v"));

	if (v->plexcount == 0)
		return;

	else if (v->plexcount == 1) {
		p = LIST_FIRST(&v->plexes);
		KASSERT(p != NULL, ("gv_start_vol: NULL p on %s", v->name));
		if (p->org == GV_PLEX_RAID5) {
			switch (p->state) {
			case GV_PLEX_DOWN:
				gv_init_plex(p);
				break;
			case GV_PLEX_DEGRADED:  /* XXX not yet */
			default:
				return;
			}
		} else {
			LIST_FOREACH(s, &p->subdisks, in_plex) {
				gv_set_sd_state(s, GV_SD_UP,
				    GV_SETSTATE_CONFIG);
			}
		}
	} else
		gv_sync(v);
}

void
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
		return;

	LIST_FOREACH(p, &v->plexes, in_volume) {
		if ((p == up) || (p->state == GV_PLEX_UP))
			continue;
		sync = g_malloc(sizeof(*sync), M_WAITOK | M_ZERO);
		sync->v = v;
		sync->from = up;
		sync->to = p;
		sync->syncsize = GV_DFLT_SYNCSIZE;
		kthread_create(gv_sync_td, sync, NULL, 0, 0, "sync_p '%s'",
		    p->name);
	}
}

int
gv_init_plex(struct gv_plex *p)
{
	struct gv_sd *s;
	int err;

	KASSERT(p != NULL, ("gv_init_plex: NULL p"));

	LIST_FOREACH(s, &p->subdisks, in_plex) {
		err = gv_init_sd(s);
		if (err)
			return (err);
	}

	return (0);
}

int
gv_init_sd(struct gv_sd *s)
{
	KASSERT(s != NULL, ("gv_init_sd: NULL s"));

	if (gv_set_sd_state(s, GV_SD_INITIALIZING, GV_SETSTATE_FORCE))
		return (-1);

	s->init_size = GV_DFLT_SYNCSIZE;
	s->flags &= ~GV_SD_INITCANCEL;

	/* Spawn the thread that does the work for us. */
	kthread_create(gv_init_td, s, NULL, 0, 0, "init_sd %s", s->name);

	return (0);
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
	p->flags |= GV_PLEX_SYNCING;

	error = 0;

	g_topology_lock();
	error = g_access(from, 1, 0, 0);
	if (error) {
		g_topology_unlock();
		printf("gvinum: sync from '%s' failed to access consumer: %d\n",
		    sync->from->name, error);
		kthread_exit(error);
	}
	error = g_access(to, 0, 1, 0);
	if (error) {
		g_access(from, -1, 0, 0);
		g_topology_unlock();
		printf("gvinum: sync to '%s' failed to access consumer: %d\n",
		    p->name, error);
		kthread_exit(error);
	}
	g_topology_unlock();

	printf("GEOM_VINUM: plex sync %s -> %s started\n", sync->from->name,
	    sync->to->name);
	for (i = 0; i < p->size; i+= sync->syncsize) {
		/* Read some bits from the good plex. */
		buf = g_read_data(from, i, sync->syncsize, &error);
		if (buf == NULL) {
			printf("gvinum: sync read from '%s' failed at offset "
			    "%jd, errno: %d\n", sync->from->name, i, error);
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
			printf("gvinum: sync write to '%s' failed at offset "
			    "%jd, out of memory\n", p->name, i);
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
			printf("gvinum: sync write to '%s' failed at offset "
			    "%jd, errno: %d\n", p->name, i, error);
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
	if (!error) {
		p->flags &= ~GV_PLEX_SYNCING;
		printf("GEOM_VINUM: plex sync %s -> %s finished\n",
		    sync->from->name, sync->to->name);
	}

	g_free(sync);
	kthread_exit(error);
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
		printf("geom_vinum: init '%s' failed to access consumer: %d\n",
		    s->name, error);
		kthread_exit(error);
	}
	g_topology_unlock();

	for (i = start; i < offset + length; i += init_size) {
		if (s->flags & GV_SD_INITCANCEL) {
			printf("geom_vinum: subdisk '%s' init: cancelled at"
			    " offset %jd (drive offset %jd)\n", s->name,
			    (intmax_t)s->initialized, (intmax_t)i);
			error = EAGAIN;
			break;
		}
		error = g_write_data(cp, i, buf, init_size);
		if (error) {
			printf("geom_vinum: subdisk '%s' init: write failed"
			    " at offset %jd (drive offset %jd)\n", s->name,
			    (intmax_t)s->initialized, (intmax_t)i);
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
		printf("geom_vinum: init '%s' finished\n", s->name);
	}
	kthread_exit(error);
}
