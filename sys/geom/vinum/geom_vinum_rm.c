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

static int	gv_rm_drive(struct gv_softc *, struct gctl_req *,
		    struct gv_drive *, int);
static int	gv_rm_plex(struct gv_softc *, struct gctl_req *,
		    struct gv_plex *, int);
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

/* Resets configuration */
int
gv_resetconfig(struct g_geom *gp, struct gctl_req *req)
{
	struct gv_softc *sc;
	struct gv_drive *d, *d2;
	struct gv_volume *v, *v2;
	struct gv_plex *p, *p2;
	struct gv_sd *s, *s2;
	int flags;

	d = NULL;
	d2 = NULL;
	p = NULL;
	p2 = NULL;
	s = NULL;
	s2 = NULL;
	flags = GV_FLAG_R;
	sc = gp->softc;
	/* First loop through to make sure no volumes are up */
        LIST_FOREACH_SAFE(v, &sc->volumes, volume, v2) {
		if (gv_is_open(v->geom)) {
			gctl_error(req, "volume '%s' is busy", v->name);
			return (-1);
		}
	}
	/* Then if not, we remove everything. */
	LIST_FOREACH_SAFE(v, &sc->volumes, volume, v2)
		gv_rm_vol(sc, req, v, flags);
	LIST_FOREACH_SAFE(p, &sc->plexes, plex, p2)
		gv_rm_plex(sc, req, p, flags);
	LIST_FOREACH_SAFE(s, &sc->subdisks, sd, s2)
		gv_rm_sd(sc, req, s, flags);
	LIST_FOREACH_SAFE(d, &sc->drives, drive, d2)
		gv_rm_drive(sc, req, d, flags);
	gv_save_config_all(sc);
	return (0);
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
	struct gv_volume *v;
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
#if 0
		LIST_REMOVE(s, in_plex);
		s->plex_sc = NULL;
#endif

		err = gv_rm_sd(sc, req, s, flags);
		if (err)
			return (err);
	}

	v = p->vol_sc;
	/* Clean up and let our geom fade away. */
	LIST_REMOVE(p, plex);
	if (p->vol_sc != NULL) {
		p->vol_sc->plexcount--;
		LIST_REMOVE(p, in_volume);
		p->vol_sc = NULL;
		/* Correctly update the volume size. */
		gv_update_vol_size(v, gv_vol_size(v));
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
int
gv_rm_sd(struct gv_softc *sc, struct gctl_req *req, struct gv_sd *s, int flags)
{
	struct g_provider *pp;
	struct gv_plex *p;
	struct gv_volume *v;

	KASSERT(s != NULL, ("gv_rm_sd: NULL s"));

	pp = s->provider;
	p = s->plex_sc;
	v = NULL;

	/* Clean up. */
	if (p != NULL) {
		LIST_REMOVE(s, in_plex);

		p->sdcount--;
		/* Update the plexsize. */
		p->size = gv_plex_size(p);
		v = p->vol_sc;
		if (v != NULL) {
			/* Update the size of our plex' volume. */
			gv_update_vol_size(v, gv_vol_size(v));
		}
	}
	if (s->drive_sc)
		LIST_REMOVE(s, from_drive);
	LIST_REMOVE(s, sd);
	gv_free_sd(s);
	g_free(s);

	/* If the subdisk has a provider we need to clean up this one too. */
	if (pp != NULL) {
		pp->flags |= G_PF_WITHER;
		g_orphan_provider(pp, ENXIO);
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
	err = gv_write_header(cp, d->hdr);
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
