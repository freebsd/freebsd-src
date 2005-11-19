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
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <geom/geom.h>
#include <geom/vinum/geom_vinum_var.h>
#include <geom/vinum/geom_vinum.h>
#include <geom/vinum/geom_vinum_share.h>

static int	gv_rename_drive(struct gv_softc *, struct gctl_req *,
		    struct gv_drive *, char *, int);
static int	gv_rename_plex(struct gv_softc *, struct gctl_req *,
		    struct gv_plex *, char *, int);
static int	gv_rename_sd(struct gv_softc *, struct gctl_req *,
		    struct gv_sd *, char *, int);
static int	gv_rename_vol(struct gv_softc *, struct gctl_req *,
		    struct gv_volume *, char *, int);

void
gv_rename(struct g_geom *gp, struct gctl_req *req)
{
	struct gv_softc *sc;
	struct gv_volume *v;
	struct gv_plex *p;
	struct gv_sd *s;
	struct gv_drive *d;
	char *newname, *object;
	int err, *flags, type;

	sc = gp->softc;

	flags = gctl_get_paraml(req, "flags", sizeof(*flags));

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
		err = gv_rename_vol(sc, req, v, newname, *flags);
		if (err)
			return;
		break;
	case GV_TYPE_PLEX:
		p = gv_find_plex(sc, object);
		if (p == NULL) {
			gctl_error(req, "unknown plex '%s'", object);
			return;
		}
		err = gv_rename_plex(sc, req, p, newname, *flags);
		if (err)
			return;
		break;
	case GV_TYPE_SD:
		s = gv_find_sd(sc, object);
		if (s == NULL) {
			gctl_error(req, "unknown subdisk '%s'", object);
			return;
		}
		err = gv_rename_sd(sc, req, s, newname, *flags);
		if (err)
			return;
		break;
	case GV_TYPE_DRIVE:
		d = gv_find_drive(sc, object);
		if (d == NULL) {
			gctl_error(req, "unknown drive '%s'", object);
			return;
		}
		err = gv_rename_drive(sc, req, d, newname, *flags);
		if (err)
			return;
		break;
	default:
		gctl_error(req, "unknown object '%s'", object);
		return;
	}

	gv_save_config_all(sc);
}

static int 
gv_rename_drive(struct gv_softc *sc, struct gctl_req *req, struct gv_drive *d, char *newname, int flags)
{
	struct gv_sd *s;

	g_topology_assert();
	KASSERT(d != NULL, ("gv_rename_drive: NULL d"));

	if (gv_object_type(sc, newname) != -1) {
		gctl_error(req, "drive name '%s' already in use", newname);
		return (-1);
	}

	strncpy(d->name, newname, GV_MAXDRIVENAME);

	/* XXX can we rename providers here? */

	LIST_FOREACH(s, &d->subdisks, from_drive)
		strncpy(s->drive, d->name, GV_MAXDRIVENAME);

	return (0);
}

static int	
gv_rename_plex(struct gv_softc *sc, struct gctl_req *req, struct gv_plex *p, char *newname, int flags)
{
	struct gv_sd *s;
	char plexnumber[GV_MAXPLEXNAME], *pplexnumber;
	char oldplexname[GV_MAXPLEXNAME], *poldplexname;
	int err;

	pplexnumber = plexnumber;
	poldplexname = oldplexname;

	g_topology_assert();
	KASSERT(p != NULL, ("gv_rename_plex: NULL p"));

	if (gv_object_type(sc, newname) != -1) {
		gctl_error(req, "plex name '%s' already in use", newname);
		return (-1);
	}

	strncpy(oldplexname, p->name, GV_MAXPLEXNAME);
	strsep(&poldplexname, ".");
	strncpy(plexnumber, p->name, GV_MAXPLEXNAME);
	strsep(&pplexnumber, ".");
	if (strcmp(poldplexname, pplexnumber)) {
		gctl_error(req, "current and proposed plex numbers (%s, %s) "
		    "do not match", pplexnumber, poldplexname);
		return (-1);
	}

	strncpy(p->name, newname, GV_MAXPLEXNAME);

	/* XXX can we rename providers here? */

	/* Fix up references and potentially rename subdisks. */
	LIST_FOREACH(s, &p->subdisks, in_plex) {
		strncpy(s->plex, p->name, GV_MAXPLEXNAME); 
		if (flags && GV_FLAG_R) {
			char newsdname[GV_MAXSDNAME];
			char oldsdname[GV_MAXSDNAME];
			char *poldsdname = oldsdname;
			strncpy(oldsdname, s->name, GV_MAXSDNAME);
			strsep(&poldsdname, ".");
			strsep(&poldsdname, ".");
			snprintf(newsdname, GV_MAXSDNAME, "%s.%s", p->name,
			    poldsdname);
			err = gv_rename_sd(sc, req, s, newsdname, flags);
			if (err)
				return (err);
		}
	}

	return (0);
}

/*
 * gv_rename_sd: renames a subdisk.  Note that the 'flags' argument is ignored,
 * since there are no structures below a subdisk.  Similarly, we don't have to
 * clean up any references elsewhere to the subdisk's name.
 */
static int 
gv_rename_sd(struct gv_softc *sc, struct gctl_req *req, struct gv_sd *s, char * newname, int flags)  
{
        char newsdnumber[GV_MAXSDNAME], *pnewsdnumber;
	char oldsdnumber[GV_MAXSDNAME], *poldsdnumber;

	pnewsdnumber = newsdnumber;
	poldsdnumber = oldsdnumber;

	g_topology_assert();
	KASSERT(s != NULL, ("gv_rename_sd: NULL s"));

	if (gv_object_type(sc, newname) != -1) {
		gctl_error(req, "subdisk name %s already in use", newname);
		return (-1);
	}

	strncpy(oldsdnumber, s->name, GV_MAXSDNAME);
	strsep(&poldsdnumber, ".");
	strsep(&poldsdnumber, ".");
	strncpy(newsdnumber, newname, GV_MAXSDNAME);
	strsep(&pnewsdnumber, ".");
	strsep(&pnewsdnumber, ".");
	if (strcmp(pnewsdnumber, poldsdnumber)) {
		gctl_error(req, "current and proposed sd numbers (%s, %s) do "
		    "not match", poldsdnumber, pnewsdnumber);
		return (-1);
	}

	strncpy(s->name, newname, GV_MAXSDNAME);

	/* XXX: can we rename providers here? */

	return (0);
}

static int 
gv_rename_vol(struct gv_softc *sc, struct gctl_req *req, struct gv_volume *v, char *newname, int flags)
{
	struct gv_plex *p;
	int err;

	g_topology_assert();
	KASSERT(v != NULL, ("gv_rename_vol: NULL v"));

	if (gv_object_type(sc, newname) != -1) {
		gctl_error(req, "volume name %s already in use", newname);
		return (-1);
	}

	/* Rename the volume. */
	strncpy(v->name, newname, GV_MAXVOLNAME);

	/* Fix up references and potentially rename plexes. */
	LIST_FOREACH(p, &v->plexes, in_volume) {
		strncpy(p->volume, v->name, GV_MAXVOLNAME);
		if (flags && GV_FLAG_R) {
			char newplexname[GV_MAXPLEXNAME];
			char oldplexname[GV_MAXPLEXNAME];
			char *poldplexname = oldplexname;
			strncpy(oldplexname, p->name, GV_MAXPLEXNAME);
			strsep(&poldplexname, ".");
			snprintf(newplexname, GV_MAXPLEXNAME, "%s.%s",
			    v->name, poldplexname);
			err = gv_rename_plex(sc, req, p, newplexname, flags);
			if (err)
				return err;
		}
	}
       
	return (0);
}
