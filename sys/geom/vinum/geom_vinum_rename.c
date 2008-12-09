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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/libkern.h>
#include <sys/malloc.h>

#include <geom/geom.h>
#include <geom/vinum/geom_vinum_var.h>
#include <geom/vinum/geom_vinum.h>

void
gv_rename(struct g_geom *gp, struct gctl_req *req)
{
	struct gv_softc *sc;
	struct gv_volume *v;
	struct gv_plex *p;
	struct gv_sd *s;
	struct gv_drive *d;
	char *newname, *object, *name;
	int *flags, type;

	sc = gp->softc;

	flags = gctl_get_paraml(req, "flags", sizeof(*flags));
	if (flags == NULL) {
		gctl_error(req, "no flags given");
		return;
	}

	newname = gctl_get_param(req, "newname", NULL);
	if (newname == NULL) {
		gctl_error(req, "no new name given");
		return;
	}

	object = gctl_get_param(req, "object", NULL);
	if (object == NULL) {
		gctl_error(req, "no object given");
		return;
	}

	type = gv_object_type(sc, object);
	switch (type) {
	case GV_TYPE_VOL:
		v = gv_find_vol(sc, object);
		if (v == NULL) 	{
			gctl_error(req, "unknown volume '%s'", object);
			return;
		}
		name = g_malloc(GV_MAXVOLNAME, M_WAITOK | M_ZERO);
		strlcpy(name, newname, GV_MAXVOLNAME);
		gv_post_event(sc, GV_EVENT_RENAME_VOL, v, name, *flags, 0);
		break;
	case GV_TYPE_PLEX:
		p = gv_find_plex(sc, object);
		if (p == NULL) {
			gctl_error(req, "unknown plex '%s'", object);
			return;
		}
		name = g_malloc(GV_MAXPLEXNAME, M_WAITOK | M_ZERO);
		strlcpy(name, newname, GV_MAXPLEXNAME);
		gv_post_event(sc, GV_EVENT_RENAME_PLEX, p, name, *flags, 0);
		break;
	case GV_TYPE_SD:
		s = gv_find_sd(sc, object);
		if (s == NULL) {
			gctl_error(req, "unknown subdisk '%s'", object);
			return;
		}
		name = g_malloc(GV_MAXSDNAME, M_WAITOK | M_ZERO);
		strlcpy(name, newname, GV_MAXSDNAME);
		gv_post_event(sc, GV_EVENT_RENAME_SD, s, name, *flags, 0);
		break;
	case GV_TYPE_DRIVE:
		d = gv_find_drive(sc, object);
		if (d == NULL) {
			gctl_error(req, "unknown drive '%s'", object);
			return;
		}
		name = g_malloc(GV_MAXDRIVENAME, M_WAITOK | M_ZERO);
		strlcpy(name, newname, GV_MAXDRIVENAME);
		gv_post_event(sc, GV_EVENT_RENAME_DRIVE, d, name, *flags, 0);
		break;
	default:
		gctl_error(req, "unknown object '%s'", object);
		return;
	}
}

int
gv_rename_drive(struct gv_softc *sc, struct gv_drive *d, char *newname,
    int flags)
{
	struct gv_sd *s;

	g_topology_assert();
	KASSERT(d != NULL, ("gv_rename_drive: NULL d"));

	if (gv_object_type(sc, newname) != GV_ERR_NOTFOUND) {
		G_VINUM_DEBUG(1, "drive name '%s' already in use", newname);
		return (GV_ERR_NAMETAKEN);
	}

	strlcpy(d->name, newname, sizeof(d->name));
	if (d->hdr != NULL)
		strlcpy(d->hdr->label.name, newname, sizeof(d->hdr->label.name));

	LIST_FOREACH(s, &d->subdisks, from_drive)
		strlcpy(s->drive, d->name, sizeof(s->drive));

	return (0);
}

int
gv_rename_plex(struct gv_softc *sc, struct gv_plex *p, char *newname, int flags)
{
	struct gv_sd *s;
	char *plexnum, *plexnump, *oldplex, *oldplexp;
	char *newsd, *oldsd, *oldsdp;
	int err;

	g_topology_assert();
	KASSERT(p != NULL, ("gv_rename_plex: NULL p"));

	err = 0;

	if (gv_object_type(sc, newname) != GV_ERR_NOTFOUND) {
		G_VINUM_DEBUG(1, "plex name '%s' already in use", newname);
		return (GV_ERR_NAMETAKEN);
	}

	/* Needed for sanity checking. */
	plexnum = g_malloc(GV_MAXPLEXNAME, M_WAITOK | M_ZERO);
	strlcpy(plexnum, newname, GV_MAXPLEXNAME);
	plexnump = plexnum;

	oldplex = g_malloc(GV_MAXPLEXNAME, M_WAITOK | M_ZERO);
	strlcpy(oldplex, p->name, GV_MAXPLEXNAME);
	oldplexp = oldplex;

	/*
	 * Locate the plex number part of the plex names.
	 *
	 * XXX: can we be sure that the current plex name has the format
	 * 'foo.pX'?
	 */
	strsep(&oldplexp, ".");
	strsep(&plexnump, ".");
	if (plexnump == NULL || *plexnump == '\0') {
		G_VINUM_DEBUG(0, "proposed plex name '%s' is not a valid plex "
		    "name", newname);
		err = GV_ERR_INVNAME;
		goto failure;
	}

	strlcpy(p->name, newname, sizeof(p->name));

	/* XXX can we rename providers here? */

	/* Fix up references and potentially rename subdisks. */
	LIST_FOREACH(s, &p->subdisks, in_plex) {
		strlcpy(s->plex, p->name, sizeof(s->plex));
		if (flags && GV_FLAG_R) {
			newsd = g_malloc(GV_MAXSDNAME, M_WAITOK | M_ZERO);
			oldsd = g_malloc(GV_MAXSDNAME, M_WAITOK | M_ZERO);
			oldsdp = oldsd;
			strlcpy(oldsd, s->name, GV_MAXSDNAME);
			/*
			 * XXX: can we be sure that the current sd name has the
			 * format 'foo.pX.sY'?
			 */
			strsep(&oldsdp, ".");
			strsep(&oldsdp, ".");
			snprintf(newsd, GV_MAXSDNAME, "%s.%s", p->name, oldsdp);
			err = gv_rename_sd(sc, s, newsd, flags);
			g_free(newsd);
			g_free(oldsd);
			if (err)
				goto failure;
		}
	}

failure:
	g_free(plexnum);
	g_free(oldplex);

	return (err);
}

/*
 * gv_rename_sd: renames a subdisk.  Note that the 'flags' argument is ignored,
 * since there are no structures below a subdisk.  Similarly, we don't have to
 * clean up any references elsewhere to the subdisk's name.
 */
int
gv_rename_sd(struct gv_softc *sc, struct gv_sd *s, char *newname, int flags)
{
	char *new, *newp, *old, *oldp;
	int err;

	g_topology_assert();
	KASSERT(s != NULL, ("gv_rename_sd: NULL s"));

	err = 0;

	if (gv_object_type(sc, newname) != GV_ERR_NOTFOUND) {
		G_VINUM_DEBUG(1, "subdisk name %s already in use", newname);
		return (GV_ERR_NAMETAKEN);
	}

	/* Needed for sanity checking. */
	new = g_malloc(GV_MAXSDNAME, M_WAITOK | M_ZERO);
	strlcpy(new, newname, GV_MAXSDNAME);
	newp = new;

	old = g_malloc(GV_MAXSDNAME, M_WAITOK | M_ZERO);
	strlcpy(old, s->name, GV_MAXSDNAME);
	oldp = old;

	/*
	 * Locate the sd number part of the sd names.
	 *
	 * XXX: can we be sure that the current sd name has the format
	 * 'foo.pX.sY'?
	 */
	strsep(&oldp, ".");
	strsep(&oldp, ".");
	strsep(&newp, ".");
	if (newp == NULL || *newp == '\0') {
		G_VINUM_DEBUG(0, "proposed sd name '%s' is not a valid sd name",
		    newname);
		err = GV_ERR_INVNAME;
		goto fail;
	}
	strsep(&newp, ".");
	if (newp == NULL || *newp == '\0') {
		G_VINUM_DEBUG(0, "proposed sd name '%s' is not a valid sd name",
		    newname);
		err = GV_ERR_INVNAME;
		goto fail;
	}

	strlcpy(s->name, newname, sizeof(s->name));

fail:
	g_free(new);
	g_free(old);

	return (err);
}

int
gv_rename_vol(struct gv_softc *sc, struct gv_volume *v, char *newname,
    int flags)
{
	struct gv_plex *p;
	char *new, *old, *oldp;
	int err;

	g_topology_assert();
	KASSERT(v != NULL, ("gv_rename_vol: NULL v"));

	if (gv_object_type(sc, newname) != GV_ERR_NOTFOUND) {
		G_VINUM_DEBUG(1, "volume name %s already in use", newname);
		return (GV_ERR_NAMETAKEN);
	}

	/* Rename the volume. */
	strlcpy(v->name, newname, sizeof(v->name));

	/* Fix up references and potentially rename plexes. */
	LIST_FOREACH(p, &v->plexes, in_volume) {
		strlcpy(p->volume, v->name, sizeof(p->volume));
		if (flags && GV_FLAG_R) {
			new = g_malloc(GV_MAXPLEXNAME, M_WAITOK | M_ZERO);
			old = g_malloc(GV_MAXPLEXNAME, M_WAITOK | M_ZERO);
			oldp = old;
			strlcpy(old, p->name, GV_MAXPLEXNAME);
			/*
			 * XXX: can we be sure that the current plex name has
			 * the format 'foo.pX'?
			 */
			strsep(&oldp, ".");
			snprintf(new, GV_MAXPLEXNAME, "%s.%s", v->name, oldp);
			err = gv_rename_plex(sc, p, new, flags);
			g_free(new);
			g_free(old);
			if (err)
				return (err);
		}
	}

	return (0);
}
