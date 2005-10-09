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
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/malloc.h>

#include <geom/geom.h>
#include <geom/vinum/geom_vinum_var.h>
#include <geom/vinum/geom_vinum.h>
#include <geom/vinum/geom_vinum_share.h>

void
gv_setstate(struct g_geom *gp, struct gctl_req *req)
{
	struct gv_softc *sc;
	struct gv_sd *s;
	struct gv_drive *d;
	char *obj, *state;
	int err, f, *flags, newstate, type;

	f = 0;
	obj = gctl_get_param(req, "object", NULL);
	if (obj == NULL) {
		gctl_error(req, "no object given");
		return;
	}

	state = gctl_get_param(req, "state", NULL);
	if (state == NULL) {
		gctl_error(req, "no state given");
		return;
	}

	flags = gctl_get_paraml(req, "flags", sizeof(*flags));
	if (flags == NULL) {
		gctl_error(req, "no flags given");
		return;
	}

	if (*flags & GV_FLAG_F)
		f = GV_SETSTATE_FORCE;

	sc = gp->softc;
	type = gv_object_type(sc, obj);
	switch (type) {
	case GV_TYPE_VOL:
	case GV_TYPE_PLEX:
		gctl_error(req, "volume or plex state cannot be set currently");
		break;

	case GV_TYPE_SD:
		newstate = gv_sdstatei(state);
		if (newstate < 0) {
			gctl_error(req, "invalid subdisk state '%s'", state);
			break;
		}
		s = gv_find_sd(sc, obj);
		err = gv_set_sd_state(s, newstate, f);
		if (err)
			gctl_error(req, "cannot set subdisk state");
		break;

	case GV_TYPE_DRIVE:
		newstate = gv_drivestatei(state);
		if (newstate < 0) {
			gctl_error(req, "invalid drive state '%s'", state);
			break;
		}
		d = gv_find_drive(sc, obj);
		err = gv_set_drive_state(d, newstate, f);
		if (err)
			gctl_error(req, "cannot set drive state");
		break;

	default:
		gctl_error(req, "unknown object '%s'", obj);
		break;
	}

	return;
}

/* Update drive state; return 0 if the state changes, otherwise -1. */
int
gv_set_drive_state(struct gv_drive *d, int newstate, int flags)
{
	struct gv_sd *s;
	int oldstate;

	KASSERT(d != NULL, ("gv_set_drive_state: NULL d"));

	oldstate = d->state;
	
	if (newstate == oldstate)
		return (0);

	/* We allow to take down an open drive only with force. */
	if ((newstate == GV_DRIVE_DOWN) && gv_is_open(d->geom) &&
	    (!(flags & GV_SETSTATE_FORCE)))
		return (-1);

	d->state = newstate;

	if (d->state != oldstate) {
		LIST_FOREACH(s, &d->subdisks, from_drive)
			gv_update_sd_state(s);
	}

	/* Save the config back to disk. */
	if (flags & GV_SETSTATE_CONFIG)
		gv_save_config_all(d->vinumconf);

	return (0);
}

int
gv_set_sd_state(struct gv_sd *s, int newstate, int flags)
{
	struct gv_drive *d;
	struct gv_plex *p;
	int oldstate, status;

	KASSERT(s != NULL, ("gv_set_sd_state: NULL s"));

	oldstate = s->state;

	/* We are optimistic and assume it will work. */
	status = 0;
	
	if (newstate == oldstate)
		return (0);

	switch (newstate) {
	case GV_SD_DOWN:
		/*
		 * If we're attached to a plex, we won't go down without use of
		 * force.
		 */
		if ((s->plex_sc != NULL) && !(flags & GV_SETSTATE_FORCE))
			return (-1);
		break;

	case GV_SD_UP:
		/* We can't bring the subdisk up if our drive is dead. */
		d = s->drive_sc;
		if ((d == NULL) || (d->state != GV_DRIVE_UP))
			return (-1);

		/* Check from where we want to be brought up. */
		switch (s->state) {
		case GV_SD_REVIVING:
		case GV_SD_INITIALIZING:
			/*
			 * The subdisk was initializing.  We allow it to be
			 * brought up.
			 */
			break;

		case GV_SD_DOWN:
			/*
			 * The subdisk is currently down.  We allow it to be
			 * brought up if it is not attached to a plex.
			 */
			p = s->plex_sc;
			if (p == NULL)
				break;

			/*
			 * If this subdisk is attached to a plex, we allow it
			 * to be brought up if the plex if it's not a RAID5
			 * plex, otherwise it's made 'stale'.
			 */

			if (p->org != GV_PLEX_RAID5)
				break;
			else if (flags & GV_SETSTATE_FORCE)
				break;
			else
				s->state = GV_SD_STALE;

			status = -1;
			break;

		case GV_SD_STALE:
			/*
			 * A stale subdisk can be brought up only if it's part
			 * of a concat or striped plex that's the only one in a
			 * volume, or if the subdisk isn't attached to a plex.
			 * Otherwise it needs to be revived or initialized
			 * first.
			 */
			p = s->plex_sc;
			if (p == NULL || flags & GV_SETSTATE_FORCE)
				break;

			if ((p->org != GV_PLEX_RAID5) &&
			    (p->vol_sc->plexcount == 1))
				break;
			else
				return (-1);

		default:
			return (-1);
		}
		break;

	/* Other state transitions are only possible with force. */
	default:
		if (!(flags & GV_SETSTATE_FORCE))
			return (-1);
	}

	/* We can change the state and do it. */
	if (status == 0)
		s->state = newstate;

	/* Update our plex, if we're attached to one. */
	if (s->plex_sc != NULL)
		gv_update_plex_state(s->plex_sc);

	/* Save the config back to disk. */
	if (flags & GV_SETSTATE_CONFIG)
		gv_save_config_all(s->vinumconf);

	return (status);
}


/* Update the state of a subdisk based on its environment. */
void
gv_update_sd_state(struct gv_sd *s)
{
	struct gv_drive *d;
	int oldstate;

	KASSERT(s != NULL, ("gv_update_sd_state: NULL s"));
	d = s->drive_sc;
	KASSERT(d != NULL, ("gv_update_sd_state: NULL d"));

	oldstate = s->state;
	
	/* If our drive isn't up we cannot be up either. */
	if (d->state != GV_DRIVE_UP)
		s->state = GV_SD_DOWN;
	/* If this subdisk was just created, we assume it is good.*/
	else if (s->flags & GV_SD_NEWBORN) {
		s->state = GV_SD_UP;
		s->flags &= ~GV_SD_NEWBORN;
	} else if (s->state != GV_SD_UP)
		s->state = GV_SD_STALE;
	else
		s->state = GV_SD_UP;
	
	if (s->state != oldstate)
		printf("GEOM_VINUM: subdisk %s state change: %s -> %s\n",
		    s->name, gv_sdstate(oldstate), gv_sdstate(s->state));

	/* Update the plex, if we have one. */
	if (s->plex_sc != NULL)
		gv_update_plex_state(s->plex_sc);
}

/* Update the state of a plex based on its environment. */
void
gv_update_plex_state(struct gv_plex *p)
{
	int sdstates;
	int oldstate;

	KASSERT(p != NULL, ("gv_update_plex_state: NULL p"));

	oldstate = p->state;

	/* First, check the state of our subdisks. */
	sdstates = gv_sdstatemap(p);
	
	/* If all subdisks are up, our plex can be up, too. */
	if (sdstates == GV_SD_UPSTATE)
		p->state = GV_PLEX_UP;

	/* One or more of our subdisks are down. */
	else if (sdstates & GV_SD_DOWNSTATE) {
		/* A RAID5 plex can handle one dead subdisk. */
		if ((p->org == GV_PLEX_RAID5) && (p->sddown == 1))
			p->state = GV_PLEX_DEGRADED;
		else
			p->state = GV_PLEX_DOWN;

	/* Some of our subdisks are initializing. */
	} else if (sdstates & GV_SD_INITSTATE) {
		if (p->flags & GV_PLEX_SYNCING)
			p->state = GV_PLEX_DEGRADED;
		else
			p->state = GV_PLEX_DOWN;
	} else
		p->state = GV_PLEX_DOWN;

	if (p->state != oldstate)
		printf("GEOM_VINUM: plex %s state change: %s -> %s\n", p->name,
		    gv_plexstate(oldstate), gv_plexstate(p->state));

	/* Update our volume, if we have one. */
	if (p->vol_sc != NULL)
		gv_update_vol_state(p->vol_sc);
}

/* Update the volume state based on its plexes. */
void
gv_update_vol_state(struct gv_volume *v)
{
	struct gv_plex *p;

	KASSERT(v != NULL, ("gv_update_vol_state: NULL v"));

	LIST_FOREACH(p, &v->plexes, in_volume) {
		/* One of our plexes is accessible, and so are we. */
		if (p->state > GV_PLEX_DEGRADED) {
			v->state = GV_VOL_UP;
			return;

		/* We can handle a RAID5 plex with one dead subdisk as well. */
		} else if ((p->org == GV_PLEX_RAID5) &&
		    (p->state == GV_PLEX_DEGRADED)) {
			v->state = GV_VOL_UP;
			return;
		}
	}

	/* Not one of our plexes is up, so we can't be either. */
	v->state = GV_VOL_DOWN;
}

/* Return a state map for the subdisks of a plex. */
int
gv_sdstatemap(struct gv_plex *p)
{
	struct gv_sd *s;
	int statemap;

	KASSERT(p != NULL, ("gv_sdstatemap: NULL p"));
	
	statemap = 0;
	p->sddown = 0;	/* No subdisks down yet. */

	LIST_FOREACH(s, &p->subdisks, in_plex) {
		switch (s->state) {
		case GV_SD_DOWN:
		case GV_SD_STALE:
			statemap |= GV_SD_DOWNSTATE;
			p->sddown++;	/* Another unusable subdisk. */
			break;

		case GV_SD_UP:
			statemap |= GV_SD_UPSTATE;
			break;

		case GV_SD_INITIALIZING:
			statemap |= GV_SD_INITSTATE;
			break;

		case GV_SD_REVIVING:
			statemap |= GV_SD_INITSTATE;
			p->sddown++;	/* XXX: Another unusable subdisk? */
			break;
		}
	}
	return (statemap);
}
