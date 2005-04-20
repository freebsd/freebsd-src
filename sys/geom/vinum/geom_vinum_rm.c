/*-
 *  Copyright (c) 2004 Lukas Ertl
 *  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/libkern.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <geom/geom.h>
#include <geom/vinum/geom_vinum_var.h>
#include <geom/vinum/geom_vinum.h>
#include <geom/vinum/geom_vinum_share.h>

static void	gv_cleanup_pp(void *, int);
static void	gv_free_sd(struct gv_sd *);
static int	gv_rm_drive(struct gv_softc *, struct gctl_req *,
		    struct gv_drive *, int);
static int	gv_rm_plex(struct gv_softc *, struct gctl_req *,
		    struct gv_plex *, int);
static int	gv_rm_sd(struct gv_softc *, struct gctl_req *, struct gv_sd *,
		    int);
static int	gv_rm_vol(struct gv_softc *, struct gctl_req *,
		    struct gv_volume *, int);

/* General 'remove' routine. */
void
gv_remove(struct g_geom *gp, struct gctl_req *req)
{
	struct gv_softc *sc;
	struct gv_volume *v;
	struct gv_plex *p;
	struct gv_sd *s;
	struct gv_drive *d;
	int *argc, *flags;
	char *argv, buf[20];
	int i, type, err;

	argc = gctl_get_paraml(req, "argc", sizeof(*argc));
	flags = gctl_get_paraml(req, "flags", sizeof(*flags));

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
			if (v == NULL) {
				gctl_error(req, "unknown volume '%s'", argv);
				return;
			}
			err = gv_rm_vol(sc, req, v, *flags);
			if (err)
				return;
			break;
		case GV_TYPE_PLEX:
			p = gv_find_plex(sc, argv);
			if (p == NULL) {
				gctl_error(req, "unknown plex '%s'", argv);
				return;
			}
			err = gv_rm_plex(sc, req, p, *flags);
			if (err)
				return;
			break;
		case GV_TYPE_SD:
			s = gv_find_sd(sc, argv);
			if (s == NULL) {
				gctl_error(req, "unknown subdisk '%s'", argv);
				return;
			}
			err = gv_rm_sd(sc, req, s, *flags);
			if (err)
				return;
			break;
		case GV_TYPE_DRIVE:
			d = gv_find_drive(sc, argv);
			if (d == NULL) {
				gctl_error(req, "unknown drive '%s'", argv);
				return;
			}
			err = gv_rm_drive(sc, req, d, *flags);
			if (err)
				return;
			break;
		default:
			gctl_error(req, "unknown object '%s'", argv);
			return;
		}
	}

	gv_save_config_all(sc);
}

/* Remove a volume. */
static int
gv_rm_vol(struct gv_softc *sc, struct gctl_req *req, struct gv_volume *v, int flags)
{
	struct g_geom *gp;
	struct gv_plex *p, *p2;
	int err;

	g_topology_assert();
	KASSERT(v != NULL, ("gv_rm_vol: NULL v"));

	/* If this volume has plexes, we want a recursive removal. */
	if (!LIST_EMPTY(&v->plexes) && !(flags & GV_FLAG_R)) {
		gctl_error(req, "volume '%s' has attached plexes", v->name);
		return (-1);
	}

	gp = v->geom;

	/* Check if any of our consumers is open. */
	if (gp != NULL && gv_is_open(gp)) {
		gctl_error(req, "volume '%s' is busy", v->name);
		return (-1);
	}

	/* Remove the plexes our volume has. */
	LIST_FOREACH_SAFE(p, &v->plexes, in_volume, p2) {
		v->plexcount--;
		LIST_REMOVE(p, in_volume);
		p->vol_sc = NULL;

		err = gv_rm_plex(sc, req, p, flags);
		if (err)
			return (err);
	}

	/* Clean up and let our geom fade away. */
	LIST_REMOVE(v, volume);
	gv_kill_vol_thread(v);
	g_free(v);
	if (gp != NULL) {
		gp->softc = NULL;
		g_wither_geom(gp, ENXIO);
	}

	return (0);
}

/* Remove a plex. */
static int
gv_rm_plex(struct gv_softc *sc, struct gctl_req *req, struct gv_plex *p, int flags)
{
	struct g_geom *gp;
	struct gv_sd *s, *s2;
	int err;

	g_topology_assert();

	KASSERT(p != NULL, ("gv_rm_plex: NULL p"));

	/* If this plex has subdisks, we want a recursive removal. */
	if (!LIST_EMPTY(&p->subdisks) && !(flags & GV_FLAG_R)) {
		gctl_error(req, "plex '%s' has attached subdisks", p->name);
		return (-1);
	}

	if (p->vol_sc != NULL && p->vol_sc->plexcount == 1) {
		gctl_error(req, "plex '%s' is still attached to volume '%s'",
		    p->name, p->volume);
		return (-1);
	}

	gp = p->geom;

	/* Check if any of our consumers is open. */
	if (gp != NULL && gv_is_open(gp)) {
		gctl_error(req, "plex '%s' is busy", p->name);
		return (-1);
	}

	/* Remove the subdisks our plex has. */
	LIST_FOREACH_SAFE(s, &p->subdisks, in_plex, s2) {
		p->sdcount--;
#if 0
		LIST_REMOVE(s, in_plex);
		s->plex_sc = NULL;
#endif

		err = gv_rm_sd(sc, req, s, flags);
		if (err)
			return (err);
	}

	/* Clean up and let our geom fade away. */
	LIST_REMOVE(p, plex);
	if (p->vol_sc != NULL) {
		p->vol_sc->plexcount--;
		LIST_REMOVE(p, in_volume);
		p->vol_sc = NULL;
	}

	gv_kill_plex_thread(p);
	g_free(p);

	if (gp != NULL) {
		gp->softc = NULL;
		g_wither_geom(gp, ENXIO);
	}

	return (0);
}

/* Remove a subdisk. */
static int
gv_rm_sd(struct gv_softc *sc, struct gctl_req *req, struct gv_sd *s, int flags)
{
	struct gv_drive *d;
	struct g_geom *gp;
	struct g_provider *pp;

	KASSERT(s != NULL, ("gv_rm_sd: NULL s"));
	d = s->drive_sc;
	KASSERT(d != NULL, ("gv_rm_sd: NULL d"));
	gp = d->geom;
	KASSERT(gp != NULL, ("gv_rm_sd: NULL gp"));

	pp = s->provider;

	/* Clean up. */
	LIST_REMOVE(s, in_plex);
	LIST_REMOVE(s, from_drive);
	LIST_REMOVE(s, sd);
	gv_free_sd(s);
	g_free(s);

	/* If the subdisk has a provider we need to clean up this one too. */
	if (pp != NULL) {
		g_orphan_provider(pp, ENXIO);
		if (LIST_EMPTY(&pp->consumers))
			g_destroy_provider(pp);
		else
			/* Schedule this left-over provider for destruction. */
			g_post_event(gv_cleanup_pp, pp, M_WAITOK, pp, NULL);
	}

	return (0);
}

/* Remove a drive. */
static int
gv_rm_drive(struct gv_softc *sc, struct gctl_req *req, struct gv_drive *d, int flags)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	struct gv_freelist *fl, *fl2;
	struct gv_plex *p;
	struct gv_sd *s, *s2;
	struct gv_volume *v;
	int err;

	KASSERT(d != NULL, ("gv_rm_drive: NULL d"));
	gp = d->geom;
	KASSERT(gp != NULL, ("gv_rm_drive: NULL gp"));

	/* We don't allow to remove open drives. */
	if (gv_is_open(gp)) {
		gctl_error(req, "drive '%s' is open", d->name);
		return (-1);
	}

	/* A drive with subdisks needs a recursive removal. */
	if (!LIST_EMPTY(&d->subdisks) && !(flags & GV_FLAG_R)) {
		gctl_error(req, "drive '%s' still has subdisks", d->name);
		return (-1);
	}

	cp = LIST_FIRST(&gp->consumer);
	err = g_access(cp, 0, 1, 0);
	if (err) {
		printf("GEOM_VINUM: gv_rm_drive: couldn't access '%s', errno: "
		    "%d\n", cp->provider->name, err);
		return (err);
	}

	/* Clear the Vinum Magic. */
	d->hdr->magic = GV_NOMAGIC;
	g_topology_unlock();
	err = g_write_data(cp, GV_HDR_OFFSET, d->hdr, GV_HDR_LEN);
	if (err) {
		printf("GEOM_VINUM: gv_rm_drive: couldn't write header to '%s'"
		    ", errno: %d\n", cp->provider->name, err);
		d->hdr->magic = GV_MAGIC;
	}
	g_topology_lock();
	g_access(cp, 0, -1, 0);

	/* Remove all associated subdisks, plexes, volumes. */
	if (!LIST_EMPTY(&d->subdisks)) {
		LIST_FOREACH_SAFE(s, &d->subdisks, from_drive, s2) {
			p = s->plex_sc;
			if (p != NULL) {
				v = p->vol_sc;
				if (v != NULL)
					gv_rm_vol(sc, req, v, flags);
			}
		}
	}

	/* Clean up. */
	LIST_FOREACH_SAFE(fl, &d->freelist, freelist, fl2) {
		LIST_REMOVE(fl, freelist);
		g_free(fl);
	}
	LIST_REMOVE(d, drive);

	gv_kill_drive_thread(d);
	gp = d->geom;
	d->geom = NULL;
	g_free(d->hdr);
	g_free(d);
	gv_save_config_all(sc);
	g_wither_geom(gp, ENXIO);

	return (err);
}

/*
 * This function is called from the event queue to clean up left-over subdisk
 * providers.
 */
static void
gv_cleanup_pp(void *arg, int flag)
{
	struct g_provider *pp;

	g_topology_assert();

	if (flag == EV_CANCEL)
		return;

	pp = arg;
	if (pp == NULL) {
		printf("gv_cleanup_pp: provider has gone\n");
		return;
	}

	if (!LIST_EMPTY(&pp->consumers)) {
		printf("gv_cleanup_pp: provider still not empty\n");
		return;
	}

	g_destroy_provider(pp);
}

static void
gv_free_sd(struct gv_sd *s)
{
	struct gv_drive *d;
	struct gv_freelist *fl, *fl2;

	KASSERT(s != NULL, ("gv_free_sd: NULL s"));
	d = s->drive_sc;
	KASSERT(d != NULL, ("gv_free_sd: NULL d"));

	/*
	 * First, find the free slot that's immediately before or after this
	 * subdisk.
	 */
	fl = NULL;
	LIST_FOREACH(fl, &d->freelist, freelist) {
		if (fl->offset == s->drive_offset + s->size)
			break;
		if (fl->offset + fl->size == s->drive_offset)
			break;
	}

	/* If there is no free slot behind this subdisk, so create one. */
	if (fl == NULL) {

		fl = g_malloc(sizeof(*fl), M_WAITOK | M_ZERO);
		fl->size = s->size;
		fl->offset = s->drive_offset;

		if (d->freelist_entries == 0) {
			LIST_INSERT_HEAD(&d->freelist, fl, freelist);
		} else {
			LIST_FOREACH(fl2, &d->freelist, freelist) {
				if (fl->offset < fl2->offset) {
					LIST_INSERT_BEFORE(fl2, fl, freelist);
					break;
				} else if (LIST_NEXT(fl2, freelist) == NULL) {
					LIST_INSERT_AFTER(fl2, fl, freelist);
					break;
				}
			}
		}

		d->freelist_entries++;

	/* Expand the free slot we just found. */
	} else {
		fl->size += s->size;
		if (fl->offset > s->drive_offset)
			fl->offset = s->drive_offset;
	}

	d->avail += s->size;
}
