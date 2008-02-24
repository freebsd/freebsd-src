/*-
 *  Copyright (c) 2005 Chris Jones
 *  All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Chris Jones
 * thanks to the support of Google's Summer of Code program and
 * mentoring by Lukas Ertl.
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
__FBSDID("$FreeBSD: src/sys/geom/vinum/geom_vinum_move.c,v 1.3 2006/02/08 21:32:45 le Exp $");

#include <sys/param.h>
#include <sys/libkern.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <geom/geom.h>
#include <geom/vinum/geom_vinum_var.h>
#include <geom/vinum/geom_vinum.h>
#include <geom/vinum/geom_vinum_share.h>

static int      gv_move_sd(struct gv_softc *, struct gctl_req *,
		    struct gv_sd *, char *, int);

void
gv_move(struct g_geom *gp, struct gctl_req *req)
{
	struct gv_softc *sc;
	struct gv_sd *s;
	char buf[20], *destination, *object;
	int *argc, err, *flags, i, type;

	sc = gp->softc;

	argc = gctl_get_paraml(req, "argc", sizeof(*argc));
	flags = gctl_get_paraml(req, "flags", sizeof(*flags));
	destination = gctl_get_param(req, "destination", NULL);
	if (destination == NULL) {
		gctl_error(req, "no destination given");
		return;
	}
	if (gv_object_type(sc, destination) != GV_TYPE_DRIVE) {
		gctl_error(req, "destination '%s' is not a drive", destination);
		return;
	}

	/*
	 * We start with 1 here, because argv[0] on the command line is the
	 * destination drive.
	 */
	for (i = 1; i < *argc; i++) {
		snprintf(buf, sizeof(buf), "argv%d", i);
		object = gctl_get_param(req, buf, NULL);
		if (object == NULL)
			continue;

		type = gv_object_type(sc, object);
		if (type != GV_TYPE_SD) {
			gctl_error(req, "you can only move subdisks; "
			    "'%s' isn't one", object);
			return;
		}

		s = gv_find_sd(sc, object);
		if (s == NULL) {
			gctl_error(req, "unknown subdisk '%s'", object);
			return;
		}
		err = gv_move_sd(sc, req, s, destination, *flags);
		if (err)
			return;
	}

	gv_save_config_all(sc);
}

/* Move a subdisk. */
static int
gv_move_sd(struct gv_softc *sc, struct gctl_req *req, struct gv_sd *cursd, char *destination, int flags)
{
	struct gv_drive *d;
	struct gv_sd *newsd, *s, *s2;
	struct gv_plex *p;
	struct g_consumer *cp;
	char errstr[ERRBUFSIZ];
	int err;

	g_topology_assert();
	KASSERT(cursd != NULL, ("gv_move_sd: NULL cursd"));

	cp = cursd->consumer;

	if (cp != NULL && (cp->acr || cp->acw || cp->ace)) {
		gctl_error(req, "subdisk '%s' is busy", cursd->name);
		return (-1);
	}

	if (!(flags && GV_FLAG_F)) {
		gctl_error(req, "-f flag not passed; move would be "
		    "destructive");
		return (-1);
	}

	d = gv_find_drive(sc, destination);
	if (d == NULL) {
		gctl_error(req, "destination drive '%s' not found",
		    destination);
		return (-1);
	}

	if (d == cursd->drive_sc) {
		gctl_error(req, "subdisk '%s' already on drive '%s'",
		    cursd->name, destination);
		return (-1);
	}

	/* XXX: Does it have to be part of a plex? */
	p = gv_find_plex(sc, cursd->plex);
	if (p == NULL) {
		gctl_error(req, "subdisk '%s' is not part of a plex",
		    cursd->name);
		return (-1);
	}
	
	/* Stale the old subdisk. */
	err = gv_set_sd_state(cursd, GV_SD_STALE,
	    GV_SETSTATE_FORCE | GV_SETSTATE_CONFIG);
	if (err) {
		gctl_error(req, "could not set the subdisk '%s' to state "
		    "'stale'", cursd->name);
		return (err);
	}

	/*
	 * Create new subdisk. Ideally, we'd use gv_new_sd, but that requires
	 * us to create a string for it to parse, which is silly.
	 * TODO: maybe refactor gv_new_sd such that this is no longer the case.
	 */
	newsd = g_malloc(sizeof(struct gv_sd), M_WAITOK | M_ZERO);
	newsd->plex_offset = cursd->plex_offset;
	newsd->size = cursd->size;
	newsd->drive_offset = -1;
	strncpy(newsd->name, cursd->name, GV_MAXSDNAME);
	strncpy(newsd->drive, destination, GV_MAXDRIVENAME);
	strncpy(newsd->plex, cursd->plex, GV_MAXPLEXNAME);
	newsd->state = GV_SD_STALE;
	newsd->vinumconf = cursd->vinumconf;

	err = gv_sd_to_drive(sc, d, newsd, errstr, ERRBUFSIZ);
	if (err) {
		/* XXX not enough free space? */
		gctl_error(req, errstr);
		g_free(newsd);
		return (err);
	}

	/* Replace the old sd by the new one. */
	if (cp != NULL)
		g_detach(cp);
	LIST_FOREACH_SAFE(s, &p->subdisks, in_plex, s2) {
		if (s == cursd) {
			p->sdcount--;
			p->size -= s->size;
			err = gv_rm_sd(sc, req, s, 0);
			if (err)
				return (err);
			
		}
	}

	gv_sd_to_plex(p, newsd, 1);

	/* Creates the new providers.... */
	gv_drive_modify(d);

	/* And reconnect the consumer ... */
	if (cp != NULL) {
		newsd->consumer = cp;
		err = g_attach(cp, newsd->provider);
		if (err) {
			g_destroy_consumer(cp);
			gctl_error(req, "proposed move would create a loop "
			    "in GEOM config");
			return (err);
		}
	}

	LIST_INSERT_HEAD(&sc->subdisks, newsd, sd);

	gv_save_config_all(sc);

	return (0);
}
