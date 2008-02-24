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
__FBSDID("$FreeBSD: src/sys/geom/vinum/geom_vinum.c,v 1.21 2006/03/30 14:01:25 le Exp $");

#include <sys/param.h>
#include <sys/bio.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/systm.h>

#include <geom/geom.h>
#include <geom/vinum/geom_vinum_var.h>
#include <geom/vinum/geom_vinum.h>
#include <geom/vinum/geom_vinum_share.h>

#if 0
SYSCTL_DECL(_kern_geom);
SYSCTL_NODE(_kern_geom, OID_AUTO, vinum, CTLFLAG_RW, 0, "GEOM_VINUM stuff");
SYSCTL_UINT(_kern_geom_vinum, OID_AUTO, debug, CTLFLAG_RW, &gv_debug, 0,
    "Debug level");
#endif

int	gv_create(struct g_geom *, struct gctl_req *);

static void
gv_orphan(struct g_consumer *cp)
{
	struct g_geom *gp;
	struct gv_softc *sc;
	int error;
	
	g_topology_assert();

	KASSERT(cp != NULL, ("gv_orphan: null cp"));
	gp = cp->geom;
	KASSERT(gp != NULL, ("gv_orphan: null gp"));
	sc = gp->softc;

	g_trace(G_T_TOPOLOGY, "gv_orphan(%s)", gp->name);

	if (cp->acr != 0 || cp->acw != 0 || cp->ace != 0)
		g_access(cp, -cp->acr, -cp->acw, -cp->ace);
	error = cp->provider->error;
	if (error == 0)
		error = ENXIO;
	g_detach(cp);
	g_destroy_consumer(cp);
	if (!LIST_EMPTY(&gp->consumer))
		return;
	g_free(sc);
	g_wither_geom(gp, error);
}

static void
gv_start(struct bio *bp)
{
	struct bio *bp2;
	struct g_geom *gp;
	
	gp = bp->bio_to->geom;
	switch(bp->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
	case BIO_DELETE:
		bp2 = g_clone_bio(bp);
		if (bp2 == NULL)
			g_io_deliver(bp, ENOMEM);
		else {
			bp2->bio_done = g_std_done;
			g_io_request(bp2, LIST_FIRST(&gp->consumer));
		}
		return;
	default:
		g_io_deliver(bp, EOPNOTSUPP);
		return;
	}
}

static int
gv_access(struct g_provider *pp, int dr, int dw, int de)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	int error;
	
	gp = pp->geom;
	error = ENXIO;
	cp = LIST_FIRST(&gp->consumer);
	error = g_access(cp, dr, dw, de);
	return (error);
}

static void
gv_init(struct g_class *mp)
{
	struct g_geom *gp;
	struct gv_softc *sc;

	g_trace(G_T_TOPOLOGY, "gv_init(%p)", mp);

	gp = g_new_geomf(mp, "VINUM");
	gp->spoiled = gv_orphan;
	gp->orphan = gv_orphan;
	gp->access = gv_access;
	gp->start = gv_start;
	gp->softc = g_malloc(sizeof(struct gv_softc), M_WAITOK | M_ZERO);
	sc = gp->softc;
	sc->geom = gp;
	LIST_INIT(&sc->drives);
	LIST_INIT(&sc->subdisks);
	LIST_INIT(&sc->plexes);
	LIST_INIT(&sc->volumes);
}

/* Handle userland requests for creating new objects. */
int
gv_create(struct g_geom *gp, struct gctl_req *req)
{
	struct gv_softc *sc;
	struct gv_drive *d, *d2;
	struct gv_plex *p, *p2;
	struct gv_sd *s, *s2;
	struct gv_volume *v, *v2;
	struct g_consumer *cp;
	struct g_provider *pp;
	int error, i, *drives, *plexes, *subdisks, *volumes;
	char buf[20], errstr[ERRBUFSIZ];

	g_topology_assert();

	sc = gp->softc;

	/* Find out how many of each object have been passed in. */
	volumes = gctl_get_paraml(req, "volumes", sizeof(*volumes));
	plexes = gctl_get_paraml(req, "plexes", sizeof(*plexes));
	subdisks = gctl_get_paraml(req, "subdisks", sizeof(*subdisks));
	drives = gctl_get_paraml(req, "drives", sizeof(*drives));

	/* First, handle drive definitions ... */
	for (i = 0; i < *drives; i++) {
		snprintf(buf, sizeof(buf), "drive%d", i);
		d2 = gctl_get_paraml(req, buf, sizeof(*d2));

		d = gv_find_drive(sc, d2->name);
		if (d != NULL) {
			gctl_error(req, "drive '%s' is already known",
			    d->name);
			continue;
		}

		d = g_malloc(sizeof(*d), M_WAITOK | M_ZERO);
		bcopy(d2, d, sizeof(*d));

		/*
		 * Make sure that the provider specified in the drive
		 * specification is an active GEOM provider.
		 */
		pp = g_provider_by_name(d->device);
		if (pp == NULL) {
			gctl_error(req, "%s: drive not found", d->device);
			g_free(d);
			return (-1);
		}
		d->size = pp->mediasize - GV_DATA_START;
		d->avail = d->size;

		gv_config_new_drive(d);

		d->flags |= GV_DRIVE_NEWBORN;
		LIST_INSERT_HEAD(&sc->drives, d, drive);
	}

	/* ... then volume definitions ... */
	for (i = 0; i < *volumes; i++) {
		error = 0;
		snprintf(buf, sizeof(buf), "volume%d", i);
		v2 = gctl_get_paraml(req, buf, sizeof(*v2));

		v = gv_find_vol(sc, v2->name);
		if (v != NULL) {
			gctl_error(req, "volume '%s' is already known",
			    v->name);
			return (-1);
		}

		v = g_malloc(sizeof(*v), M_WAITOK | M_ZERO);
		bcopy(v2, v, sizeof(*v));

		v->vinumconf = sc;
		LIST_INIT(&v->plexes);
		LIST_INSERT_HEAD(&sc->volumes, v, volume);
	}

	/* ... then plex definitions ... */
	for (i = 0; i < *plexes; i++) {
		error = 0;
		snprintf(buf, sizeof(buf), "plex%d", i);
		p2 = gctl_get_paraml(req, buf, sizeof(*p2));

		p = gv_find_plex(sc, p2->name);
		if (p != NULL) {
			gctl_error(req, "plex '%s' is already known", p->name);
			return (-1);
		}

		p = g_malloc(sizeof(*p), M_WAITOK | M_ZERO);
		bcopy(p2, p, sizeof(*p));

		/* Find the volume this plex should be attached to. */
		v = gv_find_vol(sc, p->volume);
		if (v == NULL) {
			gctl_error(req, "volume '%s' not found", p->volume);
			g_free(p);
			continue;
		}
		if (v->plexcount)
			p->flags |= GV_PLEX_ADDED;
		p->vol_sc = v;
		v->plexcount++;
		LIST_INSERT_HEAD(&v->plexes, p, in_volume);

		p->vinumconf = sc;
		p->flags |= GV_PLEX_NEWBORN;
		LIST_INIT(&p->subdisks);
		LIST_INSERT_HEAD(&sc->plexes, p, plex);
	}

	/* ... and finally, subdisk definitions. */
	for (i = 0; i < *subdisks; i++) {
		error = 0;
		snprintf(buf, sizeof(buf), "sd%d", i);
		s2 = gctl_get_paraml(req, buf, sizeof(*s2));

		s = gv_find_sd(sc, s2->name);
		if (s != NULL) {
			gctl_error(req, "subdisk '%s' is already known",
			    s->name);
			return (-1);
		}

		s = g_malloc(sizeof(*s), M_WAITOK | M_ZERO);
		bcopy(s2, s, sizeof(*s));

		/* Find the drive where this subdisk should be put on. */
		d = gv_find_drive(sc, s->drive);

		/* drive not found - XXX */
		if (d == NULL) {
			gctl_error(req, "drive '%s' not found", s->drive);
			g_free(s);
			continue;
		}

		/* Find the plex where this subdisk belongs to. */
		p = gv_find_plex(sc, s->plex);

		/* plex not found - XXX */
		if (p == NULL) {
			gctl_error(req, "plex '%s' not found\n", s->plex);
			g_free(s);
			continue;
		}

		/*
		 * First we give the subdisk to the drive, to handle autosized
		 * values ...
		 */
		error = gv_sd_to_drive(sc, d, s, errstr, sizeof(errstr));
		if (error) {
			gctl_error(req, errstr);
			g_free(s);
			continue;
		}

		/*
		 * Then, we give the subdisk to the plex; we check if the
		 * given values are correct and maybe adjust them.
		 */
		error = gv_sd_to_plex(p, s, 1);
		if (error) {
			gctl_error(req, "GEOM_VINUM: couldn't give sd '%s' "
			    "to plex '%s'\n", s->name, p->name);
			if (s->drive_sc)
				LIST_REMOVE(s, from_drive);
			gv_free_sd(s);
			g_free(s);
			/*
			 * If this subdisk can't be created, we won't create
			 * the attached plex either, if it is also a new one.
			 */
			if (!(p->flags & GV_PLEX_NEWBORN))
				continue;
			LIST_FOREACH_SAFE(s, &p->subdisks, in_plex, s2) {
				if (s->drive_sc)
					LIST_REMOVE(s, from_drive);
				p->sdcount--;
				LIST_REMOVE(s, in_plex);
				LIST_REMOVE(s, sd);
				gv_free_sd(s);
				g_free(s);
			}
			if (p->vol_sc != NULL) {
				LIST_REMOVE(p, in_volume);
				p->vol_sc->plexcount--;
			}
			LIST_REMOVE(p, plex);
			g_free(p);
			continue;
		}
		s->flags |= GV_SD_NEWBORN;

		s->vinumconf = sc;
		LIST_INSERT_HEAD(&sc->subdisks, s, sd);
	}

	LIST_FOREACH(s, &sc->subdisks, sd)
		gv_update_sd_state(s);
	LIST_FOREACH(p, &sc->plexes, plex)
		gv_update_plex_config(p);
	LIST_FOREACH(v, &sc->volumes, volume)
		gv_update_vol_state(v);

	/*
	 * Write out the configuration to each drive.  If the drive doesn't
	 * have a valid geom_slice geom yet, attach it temporarily to our VINUM
	 * geom.
	 */
	LIST_FOREACH(d, &sc->drives, drive) {
		if (d->geom == NULL) {
			/*
			 * XXX if the provider disapears before we get a chance
			 * to write the config out to the drive, should this
			 * be handled any differently?
			 */
			pp = g_provider_by_name(d->device);
			if (pp == NULL) {
				printf("geom_vinum: %s: drive disapeared?\n",
				    d->device);
				continue;
			}
			cp = g_new_consumer(gp);
			g_attach(cp, pp);
			gv_save_config(cp, d, sc);
			g_detach(cp);
			g_destroy_consumer(cp);
		} else
			gv_save_config(NULL, d, sc);
		d->flags &= ~GV_DRIVE_NEWBORN;
	}

	return (0);
}

static void
gv_config(struct gctl_req *req, struct g_class *mp, char const *verb)
{
	struct g_geom *gp;
	struct gv_softc *sc;
	struct sbuf *sb;
	char *comment;

	g_topology_assert();

	gp = LIST_FIRST(&mp->geom);
	sc = gp->softc;

	if (!strcmp(verb, "list")) {
		gv_list(gp, req);

	/* Save our configuration back to disk. */
	} else if (!strcmp(verb, "saveconfig")) {

		gv_save_config_all(sc);

	/* Return configuration in string form. */
	} else if (!strcmp(verb, "getconfig")) {
		comment = gctl_get_param(req, "comment", NULL);

		sb = sbuf_new(NULL, NULL, GV_CFG_LEN, SBUF_FIXEDLEN);
		gv_format_config(sc, sb, 0, comment);
		sbuf_finish(sb);
		gctl_set_param(req, "config", sbuf_data(sb), sbuf_len(sb) + 1);
		sbuf_delete(sb);

	} else if (!strcmp(verb, "create")) {
		gv_create(gp, req);

	} else if (!strcmp(verb, "move")) {
		gv_move(gp, req);

	} else if (!strcmp(verb, "parityop")) {
		gv_parityop(gp, req);

	} else if (!strcmp(verb, "remove")) {
		gv_remove(gp, req);

	} else if (!strcmp(verb, "rename")) {
		gv_rename(gp, req);
	
	} else if (!strcmp(verb, "resetconfig")) {
		gv_resetconfig(gp, req);

	} else if (!strcmp(verb, "start")) {
		gv_start_obj(gp, req);

	} else if (!strcmp(verb, "setstate")) {
		gv_setstate(gp, req);

	} else
		gctl_error(req, "Unknown verb parameter");
}

#if 0
static int
gv_destroy_geom(struct gctl_req *req, struct g_class *mp, struct g_geom *gp)
{
	struct g_geom *gp2;
	struct g_consumer *cp;
	struct gv_softc *sc;
	struct gv_drive *d, *d2;
	struct gv_plex *p, *p2;
	struct gv_sd *s, *s2;
	struct gv_volume *v, *v2;
	struct gv_freelist *fl, *fl2;

	g_trace(G_T_TOPOLOGY, "gv_destroy_geom: %s", gp->name);
	g_topology_assert();

	KASSERT(gp != NULL, ("gv_destroy_geom: null gp"));
	KASSERT(gp->softc != NULL, ("gv_destroy_geom: null sc"));

	sc = gp->softc;

	/*
	 * Check if any of our drives is still open; if so, refuse destruction.
	 */
	LIST_FOREACH(d, &sc->drives, drive) {
		gp2 = d->geom;
		cp = LIST_FIRST(&gp2->consumer);
		if (cp != NULL)
			g_access(cp, -1, -1, -1);
		if (gv_is_open(gp2))
			return (EBUSY);
	}

	/* Clean up and deallocate what we allocated. */
	LIST_FOREACH_SAFE(d, &sc->drives, drive, d2) {
		LIST_REMOVE(d, drive);
		g_free(d->hdr);
		d->hdr = NULL;
		LIST_FOREACH_SAFE(fl, &d->freelist, freelist, fl2) {
			d->freelist_entries--;
			LIST_REMOVE(fl, freelist);
			g_free(fl);
			fl = NULL;
		}
		d->geom->softc = NULL;
		g_free(d);
	}

	LIST_FOREACH_SAFE(s, &sc->subdisks, sd, s2) {
		LIST_REMOVE(s, sd);
		s->drive_sc = NULL;
		s->plex_sc = NULL;
		s->provider = NULL;
		s->consumer = NULL;
		g_free(s);
	}

	LIST_FOREACH_SAFE(p, &sc->plexes, plex, p2) {
		LIST_REMOVE(p, plex);
		gv_kill_thread(p);
		p->vol_sc = NULL;
		p->geom->softc = NULL;
		p->provider = NULL;
		p->consumer = NULL;
		if (p->org == GV_PLEX_RAID5) {
			mtx_destroy(&p->worklist_mtx);
		}
		g_free(p);
	}

	LIST_FOREACH_SAFE(v, &sc->volumes, volume, v2) {
		LIST_REMOVE(v, volume);
		v->geom->softc = NULL;
		g_free(v);
	}

	gp->softc = NULL;
	g_free(sc);
	g_wither_geom(gp, ENXIO);
	return (0);
}
#endif

#define	VINUM_CLASS_NAME "VINUM"

static struct g_class g_vinum_class	= {
	.name = VINUM_CLASS_NAME,
	.version = G_VERSION,
	.init = gv_init,
	/*.destroy_geom = gv_destroy_geom,*/
	.ctlreq = gv_config,
};

DECLARE_GEOM_CLASS(g_vinum_class, g_vinum);
