/*-
 * Copyright (c) 1997, 1998, 1999
 *	Nan Yang Computer Services Limited.  All rights reserved.
 *
 *  Parts copyright (c) 1997, 1998 Cybernet Corporation, NetMAX project.
 *
 *  Written by Greg Lehey
 *
 *  This software is distributed under the so-called ``Berkeley
 *  License'':
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Nan Yang Computer
 *      Services Limited.
 * 4. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even if
 * advised of the possibility of such damage.
 *
 * $Id: vinumstate.c,v 2.18 2000/05/10 07:30:50 grog Exp grog $
 * $FreeBSD: src/sys/dev/vinum/vinumstate.c,v 1.28.2.2 2000/06/08 02:00:23 grog Exp $
 */

#include <dev/vinum/vinumhdr.h>
#include <dev/vinum/request.h>

/* Update drive state */
/* Return 1 if the state changes, otherwise 0 */
int
set_drive_state(int driveno, enum drivestate newstate, enum setstateflags flags)
{
    struct drive *drive = &DRIVE[driveno];
    int oldstate = drive->state;
    int sdno;

    if (drive->state == drive_unallocated)		    /* no drive to do anything with, */
	return 0;

    if (newstate == oldstate)				    /* don't change it if it's not different */
	return 1;					    /* all OK */
    if ((newstate == drive_down)			    /* the drive's going down */
    &&(!(flags & setstate_force))
	&& (drive->opencount != 0))			    /* we can't do it */
	return 0;					    /* don't do it */
    drive->state = newstate;				    /* set the state */
    if (drive->label.name[0] != '\0')			    /* we have a name, */
	log(LOG_INFO,
	    "vinum: drive %s is %s\n",
	    drive->label.name,
	    drive_state(drive->state));
    if (drive->state != oldstate) {			    /* state has changed */
	for (sdno = 0; sdno < vinum_conf.subdisks_allocated; sdno++) { /* find this drive's subdisks */
	    if ((SD[sdno].state >= sd_referenced)
		&& (SD[sdno].driveno == driveno))	    /* belongs to this drive */
		update_sd_state(sdno);			    /* update the state */
	}
    }
    if (newstate == drive_up) {				    /* want to bring it up */
	if ((drive->flags & VF_OPEN) == 0)		    /* should be open, but we're not */
	    init_drive(drive, 1);			    /* which changes the state again */
    } else						    /* taking it down or worse */
	queue_daemon_request(daemonrq_closedrive,	    /* get the daemon to close it */
	    (union daemoninfo) drive);
    if ((flags & setstate_configuring) == 0)		    /* configuring? */
	save_config();					    /* no: save the updated configuration now */
    return 1;
}

/*
 * Try to set the subdisk state.  Return 1 if state changed to
 * what we wanted, -1 if it changed to something else, and 0
 * if no change.
 *
 * This routine is called both from the user (up, down states only)
 * and internally.
 *
 * The setstate_force bit in the flags enables the state change even
 * if it could be dangerous to data consistency.  It shouldn't allow
 * nonsense.
 */
int
set_sd_state(int sdno, enum sdstate newstate, enum setstateflags flags)
{
    struct sd *sd = &SD[sdno];
    struct plex *plex;
    struct volume *vol;
    int oldstate = sd->state;
    int status = 1;					    /* status to return */

    if (newstate == oldstate)				    /* already there, */
	return 1;
    else if (sd->state == sd_unallocated)		    /* no subdisk to do anything with, */
	return 0;					    /* can't do it */

    if (sd->driveoffset < 0) {				    /* not allocated space */
	sd->state = sd_down;
	if (newstate != sd_down) {
	    if (sd->plexno >= 0)
		sdstatemap(&PLEX[sd->plexno]);		    /* count up subdisks */
	    return -1;
	}
    } else {						    /* space allocated */
	switch (newstate) {
	case sd_down:					    /* take it down? */
	    /*
	     * If we're attached to a plex, and we're
	     * not reborn, we won't go down without
	     * use of force.
	     */
	    if ((!flags & setstate_force)
		&& (sd->plexno >= 0)
		&& (sd->state != sd_reborn))
		return 0;				    /* don't do it */
	    break;

	case sd_initialized:
	    if ((sd->state == sd_initializing)		    /* we were initializing */
	    ||(flags & setstate_force))			    /* or we forced it */
		break;
	    return 0;					    /* can't do it otherwise */

	case sd_up:
	    if (DRIVE[sd->driveno].state != drive_up)	    /* can't bring the sd up if the drive isn't, */
		return 0;				    /* not even by force */
	    if (flags & setstate_force)			    /* forcing it, */
		break;					    /* just do it, and damn the consequences */
	    switch (sd->state) {
		/*
		 * Perform the necessary tests.  To allow
		 * the state transition, just break out of
		 * the switch.
		 */
	    case sd_crashed:
	    case sd_reborn:
	    case sd_down:				    /* been down, no data lost */
		/*
		 * If we're associated with a plex, and
		 * the plex isn't up, or we're the only
		 * subdisk in the plex, we can do it.
		 */
		if ((sd->plexno >= 0)
		    && (((PLEX[sd->plexno].state < plex_firstup)
			    || (PLEX[sd->plexno].subdisks > 1))))
		    break;				    /* do it */
		if (oldstate != sd_reborn) {
		    sd->state = sd_reborn;		    /* here it is again */
		    log(LOG_INFO,
			"vinum: %s is %s, not %s\n",
			sd->name,
			sd_state(sd->state),
			sd_state(newstate));
		}
		status = -1;
		break;

	    case sd_init:				    /* brand new */
		if (flags & setstate_configuring)	    /* we're doing this while configuring */
		    break;
		/* otherwise it's like being empty */
		/* FALLTHROUGH */

	    case sd_empty:
	    case sd_initialized:
		/*
		 * If we're not part of a plex, or the
		 * plex is not part of a volume with other
		 * plexes which are up, we can come up
		 * without being inconsistent.
		 *
		 * If we're part of a parity plex, we'll
		 * come up if the caller uses force.  This
		 * is the way we bring them up after
		 * initialization.
		 */
		if ((sd->plexno < 0)
		    || ((vpstate(&PLEX[sd->plexno]) & volplex_otherup) == 0)
		    || (isparity((&PLEX[sd->plexno]))
			&& (flags & setstate_force)))
		    break;

		/* Otherwise it's just out of date */
		/* FALLTHROUGH */

	    case sd_stale:				    /* out of date info, need reviving */
	    case sd_obsolete:
		/*

		 * 1.  If the subdisk is not part of a
		 *     plex, bring it up, don't revive.
		 *
		 * 2.  If the subdisk is part of a
		 *     one-plex volume or an unattached
		 *     plex, and it's not RAID-4 or
		 *     RAID-5, we *can't revive*.  The
		 *     subdisk doesn't change its state.
		 *
		 * 3.  If the subdisk is part of a
		 *     one-plex volume or an unattached
		 *     plex, and it's RAID-4 or RAID-5,
		 *     but more than one subdisk is down,
		 *     we *still can't revive*.  The
		 *     subdisk doesn't change its state.
		 *
		 * 4.  If the subdisk is part of a
		 *     multi-plex volume, we'll change to
		 *     reviving and let the revive
		 *     routines find out whether it will
		 *     work or not.  If they don't, the
		 *     revive stops with an error message,
		 *     but the state doesn't change
		 *     (FWIW).
		 */
		if (sd->plexno < 0)			    /* no plex associated, */
		    break;				    /* bring it up */
		plex = &PLEX[sd->plexno];
		if (plex->volno >= 0)			    /* have a volume */
		    vol = &VOL[plex->volno];
		else
		    vol = NULL;
		/*
		 * We can't do it if:
		 *
		 * 1: we don't have a volume
		 * 2: we're the only plex in the volume
		 * 3: we're a RAID-4 or RAID-5 plex, and
		 *    more than one subdisk is down.
		 */
		if (((vol == NULL)
			|| (vol->plexes == 1))
		    && ((!isparity(plex))
			|| (plex->sddowncount > 1))) {
		    if (sd->state == sd_initializing)	    /* it's finished initializing  */
			sd->state = sd_initialized;
		    else
			return 0;			    /* can't do it */
		} else {
		    sd->state = sd_reviving;		    /* put in reviving state */
		    sd->revived = 0;			    /* nothing done yet */
		    status = EAGAIN;			    /* need to repeat */
		}
		break;

	    case sd_reviving:
		if (flags & setstate_force)		    /* insist, */
		    break;
		return EAGAIN;				    /* no, try again */

	    default:					    /* can't do it */
		/*
		 * There's no way to bring subdisks up directly from
		 * other states.  First they need to be initialized
		 * or revived.
		 */
		return 0;
	    }
	    break;

	default:					    /* other ones, only internal with force */
	    if ((flags & setstate_force) == 0)		    /* no force?  What's this? */
		return 0;				    /* don't do it */
	}
    }
    if (status == 1) {					    /* we can do it, */
	sd->state = newstate;
	if (flags & setstate_force)
	    log(LOG_INFO, "vinum: %s is %s by force\n", sd->name, sd_state(sd->state));
	else
	    log(LOG_INFO, "vinum: %s is %s\n", sd->name, sd_state(sd->state));
    } else						    /* we don't get here with status 0 */
	log(LOG_INFO,
	    "vinum: %s is %s, not %s\n",
	    sd->name,
	    sd_state(sd->state),
	    sd_state(newstate));
    if (sd->plexno >= 0)				    /* we belong to a plex */
	update_plex_state(sd->plexno);			    /* update plex state */
    if ((flags & setstate_configuring) == 0)		    /* save config now */
	save_config();
    return status;
}

/*
 * Set the state of a plex dependent on its subdisks.
 * This time round, we'll let plex state just reflect
 * aggregate subdisk state, so this becomes an order of
 * magnitude less complicated.  In particular, ignore
 * the requested state.
 */
int
set_plex_state(int plexno, enum plexstate state, enum setstateflags flags)
{
    struct plex *plex;					    /* point to our plex */
    enum plexstate oldstate;
    enum volplexstate vps;				    /* how do we compare with the other plexes? */

    plex = &PLEX[plexno];				    /* point to our plex */
    oldstate = plex->state;

    /* If the plex isn't allocated, we can't do it. */
    if (plex->state == plex_unallocated)
	return 0;

    /*
     * If it's already in the the state we want,
     * and it's not up, just return.  If it's up,
     * we still need to do some housekeeping.
     */
    if ((state == oldstate)
	&& (state != plex_up))
	return 1;
    vps = vpstate(plex);				    /* how do we compare with the other plexes? */
    switch (state) {
	/*
	 * We can't bring the plex up, even by force,
	 * unless it's ready.  update_plex_state
	 * checks that.
	 */
    case plex_up:					    /* bring the plex up */
	update_plex_state(plex->plexno);		    /* it'll come up if it can */
	break;

    case plex_down:					    /* want to take it down */
	/*
	 * If we're the only one, or the only one
	 * which is up, we need force to do it.
	 */
	if (((vps == volplex_onlyus)
		|| (vps == volplex_onlyusup))
	    && (!(flags & setstate_force)))
	    return 0;					    /* can't do it */
	plex->state = state;				    /* do it */
	invalidate_subdisks(plex, sd_down);		    /* and down all up subdisks */
	break;

	/*
	 * This is only requested internally.
	 * Trust ourselves
	 */
    case plex_faulty:
	plex->state = state;				    /* do it */
	invalidate_subdisks(plex, sd_crashed);		    /* and crash all up subdisks */
	break;

    case plex_initializing:
	/* XXX consider what safeguards we need here */
	if ((flags & setstate_force) == 0)
	    return 0;
	plex->state = state;				    /* do it */
	break;

	/* What's this? */
    default:
	return 0;
    }
    if (plex->state != oldstate)			    /* we've changed, */
	log(LOG_INFO,					    /* tell them about it */
	    "vinum: %s is %s\n",
	    plex->name,
	    plex_state(plex->state));
    /*
     * Now see what we have left, and whether
     * we're taking the volume down
     */
    if (plex->volno >= 0)				    /* we have a volume */
	update_volume_state(plex->volno);		    /* update its state */
    if ((flags & setstate_configuring) == 0)		    /* save config now */
	save_config();					    /* yes: save the updated configuration */
    return 1;
}

/* Update the state of a plex dependent on its plexes. */
int
set_volume_state(int volno, enum volumestate state, enum setstateflags flags)
{
    struct volume *vol = &VOL[volno];			    /* point to our volume */

    if (vol->state == volume_unallocated)		    /* no volume to do anything with, */
	return 0;
    if (vol->state == state)				    /* we're there already */
	return 1;

    if (state == volume_up)				    /* want to come up */
	update_volume_state(volno);
    else if (state == volume_down) {			    /* want to go down */
	if (((vol->flags & VF_OPEN) == 0)		    /* not open */
	||((flags & setstate_force) != 0)) {		    /* or we're forcing */
	    vol->state = volume_down;
	    log(LOG_INFO,
		"vinum: volume %s is %s\n",
		vol->name,
		volume_state(vol->state));
	    if ((flags & setstate_configuring) == 0)	    /* save config now */
		save_config();				    /* yes: save the updated configuration */
	    return 1;
	}
    }
    return 0;						    /* no change */
}

/* Set the state of a subdisk based on its environment */
void
update_sd_state(int sdno)
{
    struct sd *sd;
    struct drive *drive;
    enum sdstate oldstate;

    sd = &SD[sdno];
    oldstate = sd->state;
    drive = &DRIVE[sd->driveno];

    if (drive->state == drive_up) {
	switch (sd->state) {
	case sd_down:
	case sd_crashed:
	    sd->state = sd_reborn;			    /* back up again with no loss */
	    break;

	default:
	    break;
	}
    } else {						    /* down or worse */
	switch (sd->state) {
	case sd_up:
	case sd_reborn:
	case sd_reviving:
	case sd_empty:
	    sd->state = sd_crashed;			    /* lost our drive */
	    break;

	default:
	    break;
	}
    }
    if (sd->state != oldstate)				    /* state has changed, */
	log(LOG_INFO,					    /* say so */
	    "vinum: %s is %s\n",
	    sd->name,
	    sd_state(sd->state));
    if (sd->plexno >= 0)				    /* we're part of a plex, */
	update_plex_state(sd->plexno);			    /* update its state */
}

/*
 * Force a plex and all its subdisks
 * into an 'up' state.  This is a helper
 * for update_plex_state.
 */
void
forceup(int plexno)
{
    struct plex *plex;
    int sdno;

    plex = &PLEX[plexno];				    /* point to the plex */
    plex->state = plex_up;				    /* and bring it up */

    /* change the subdisks to up state */
    for (sdno = 0; sdno < plex->subdisks; sdno++) {
	SD[plex->sdnos[sdno]].state = sd_up;
	log(LOG_INFO,					    /* tell them about it */
	    "vinum: %s is up\n",
	    SD[plex->sdnos[sdno]].name);
    }
}

/* Set the state of a plex based on its environment */
void
update_plex_state(int plexno)
{
    struct plex *plex;					    /* point to our plex */
    enum plexstate oldstate;
    enum sdstates statemap;				    /* get a map of the subdisk states */
    enum volplexstate vps;				    /* how do we compare with the other plexes? */

    plex = &PLEX[plexno];				    /* point to our plex */
    oldstate = plex->state;
    statemap = sdstatemap(plex);			    /* get a map of the subdisk states */
    vps = vpstate(plex);				    /* how do we compare with the other plexes? */

    if (statemap & sd_initstate)			    /* something initializing? */
	plex->state = plex_initializing;		    /* yup, that makes the plex the same */
    else if (statemap == sd_upstate)
	/*
	 * All the subdisks are up.  This also means that
	 * they are consistent, so we can just bring
	 * the plex up
	 */
	plex->state = plex_up;
    else if (isparity(plex)				    /* RAID-4 or RAID-5 plex */
    &&(plex->sddowncount == 1))				    /* and exactly one subdisk down */
	plex->state = plex_degraded;			    /* limping a bit */
    else if (((statemap & ~sd_downstate) == sd_emptystate)  /* all subdisks empty */
    ||((statemap & ~sd_downstate)
	    == (statemap & ~sd_downstate & (sd_initializedstate | sd_upstate)))) {
	if ((vps & volplex_otherup) == 0) {		    /* no other plex is up */
	    struct volume *vol = &VOL[plex->volno];	    /* possible volume to which it points */

	    /*
	     * If we're a striped or concat plex
	     * associated with a volume, none of whose
	     * plexes are up, and we're new and untested,
	     * and the volume has the setupstate bit set,
	     * we can pretend to be in a consistent state.
	     *
	     * We need to do this in one swell foop: on
	     * the next call we will no longer be just
	     * empty.
	     *
	     * This code assumes that all the other plexes
	     * are also capable of coming up (i.e. all the
	     * sds are up), but that's OK: we'll come back
	     * to this function for the remaining plexes
	     * in the volume.
	     */
	    if ((plex->state == plex_init)
		&& (plex->volno >= 0)
		&& (vol->flags & VF_CONFIG_SETUPSTATE)) {
		for (plexno = 0; plexno < vol->plexes; plexno++)
		    forceup(VOL[plex->volno].plex[plexno]);
	    } else if ((statemap == sd_initializedstate)    /* if it's initialized (not empty) */
||(plex->organization == plex_concat)			    /* and we're not RAID-4 or RAID-5 */
	    ||(plex->organization == plex_striped))
		forceup(plexno);			    /* we'll do it */
	    /*
	     * This leaves a case where things don't get
	     * done: the plex is RAID-4 or RAID-5, and
	     * the subdisks are all empty.  They need to
	     * be initialized first.
	     */
	} else {
	    if (statemap == sd_upstate)			    /* all subdisks up */
		plex->state = plex_up;			    /* we can come up too */
	    else
		plex->state = plex_faulty;
	}
    } else if ((statemap & (sd_upstate | sd_rebornstate)) == statemap) /* all up or reborn */
	plex->state = plex_flaky;
    else if (statemap & (sd_upstate | sd_rebornstate))	    /* some up or reborn */
	plex->state = plex_corrupt;			    /* corrupt */
    else if (statemap & (sd_initstate | sd_emptystate))	    /* some subdisks empty or initializing */
	plex->state = plex_initializing;
    else						    /* nothing at all up */
	plex->state = plex_faulty;

    if (plex->state != oldstate)			    /* state has changed, */
	log(LOG_INFO,					    /* tell them about it */
	    "vinum: %s is %s\n",
	    plex->name,
	    plex_state(plex->state));
    if (plex->volno >= 0)				    /* we're part of a volume, */
	update_volume_state(plex->volno);		    /* update its state */
}

/* Set volume state based on its components */
void
update_volume_state(int volno)
{
    struct volume *vol;					    /* our volume */
    int plexno;
    enum volumestate oldstate;

    vol = &VOL[volno];					    /* point to our volume */
    oldstate = vol->state;

    for (plexno = 0; plexno < vol->plexes; plexno++) {
	struct plex *plex = &PLEX[vol->plex[plexno]];	    /* point to the plex */
	if (plex->state >= plex_corrupt) {		    /* something accessible, */
	    vol->state = volume_up;
	    break;
	}
    }
    if (plexno == vol->plexes)				    /* didn't find an up plex */
	vol->state = volume_down;

    if (vol->state != oldstate) {			    /* state changed */
	log(LOG_INFO, "vinum: %s is %s\n", vol->name, volume_state(vol->state));
	save_config();					    /* save the updated configuration */
    }
}

/*
 * Called from request routines when they find
 * a subdisk which is not kosher.  Decide whether
 * it warrants changing the state.  Return
 * REQUEST_DOWN if we can't use the subdisk,
 * REQUEST_OK if we can.
 */
/*
 * A prior version of this function checked the plex
 * state as well.  At the moment, consider plex states
 * information for the user only.  We'll ignore them
 * and use the subdisk state only.  The last version of
 * this file with the old logic was 2.7. XXX
 */
enum requeststatus
checksdstate(struct sd *sd, struct request *rq, daddr_t diskaddr, daddr_t diskend)
{
    struct plex *plex = &PLEX[sd->plexno];
    int writeop = (rq->bp->b_flags & B_READ) == 0;	    /* note if we're writing */

    switch (sd->state) {
	/* We shouldn't get called if the subdisk is up */
    case sd_up:
	return REQUEST_OK;

    case sd_reviving:
	/*
	 * Access to a reviving subdisk depends on the
	 * organization of the plex:
	 *
	 * - If it's concatenated, access the subdisk
	 *   up to its current revive point.  If we
	 *   want to write to the subdisk overlapping
	 *   the current revive block, set the
	 *   conflict flag in the request, asking the
	 *   caller to put the request on the wait
	 *   list, which will be attended to by
	 *   revive_block when it's done.
	 * - if it's striped, we can't do it (we could
	 *   do some hairy calculations, but it's
	 *   unlikely to work).
	 * - if it's RAID-4 or RAID-5, we can do it as
	 *   long as only one subdisk is down
	 */
	if (plex->organization == plex_striped)		    /* plex is striped, */
	    return REQUEST_DOWN;

	else if (isparity(plex)) {			    /* RAID-4 or RAID-5 plex */
	    if (plex->sddowncount > 1)			    /* with more than one sd down, */
		return REQUEST_DOWN;
	    else
		/*
		 * XXX We shouldn't do this if we can find a
		 * better way.  Check the other plexes
		 * first, and return a DOWN if another
		 * plex will do it better
		 */
		return REQUEST_OK;			    /* OK, we'll find a way */
	}
	if (diskaddr > (sd->revived
		+ sd->plexoffset
		+ (sd->revive_blocksize >> DEV_BSHIFT)))    /* we're beyond the end */
	    return REQUEST_DOWN;
	else if (diskend > (sd->revived + sd->plexoffset)) { /* we finish beyond the end */
	    if (writeop) {
		rq->flags |= XFR_REVIVECONFLICT;	    /* note a potential conflict */
		rq->sdno = sd->sdno;			    /* and which sd last caused it */
	    } else
		return REQUEST_DOWN;
	}
	return REQUEST_OK;

    case sd_reborn:
	if (writeop)
	    return REQUEST_OK;				    /* always write to a reborn disk */
	else						    /* don't allow a read */
	    /*
	       * Handle the mapping.  We don't want to reject
	       * a read request to a reborn subdisk if that's
	       * all we have. XXX
	     */
	    return REQUEST_DOWN;

    case sd_down:
	if (writeop)					    /* writing to a consistent down disk */
	    set_sd_state(sd->sdno, sd_obsolete, setstate_force); /* it's not consistent now */
	return REQUEST_DOWN;

    case sd_crashed:
	if (writeop)					    /* writing to a consistent down disk */
	    set_sd_state(sd->sdno, sd_stale, setstate_force); /* it's not consistent now */
	return REQUEST_DOWN;

    default:
	return REQUEST_DOWN;
    }
}

/* return a state map for the subdisks of a plex */
enum sdstates
sdstatemap(struct plex *plex)
{
    int sdno;
    enum sdstates statemap = 0;				    /* note the states we find */

    plex->sddowncount = 0;				    /* no subdisks down yet */
    for (sdno = 0; sdno < plex->subdisks; sdno++) {
	struct sd *sd = &SD[plex->sdnos[sdno]];		    /* point to the subdisk */

	switch (sd->state) {
	case sd_empty:
	    statemap |= sd_emptystate;
	    (plex->sddowncount)++;			    /* another unusable subdisk */
	    break;

	case sd_init:
	    statemap |= sd_initstate;
	    (plex->sddowncount)++;			    /* another unusable subdisk */
	    break;

	case sd_down:
	    statemap |= sd_downstate;
	    (plex->sddowncount)++;			    /* another unusable subdisk */
	    break;

	case sd_crashed:
	    statemap |= sd_crashedstate;
	    (plex->sddowncount)++;			    /* another unusable subdisk */
	    break;

	case sd_obsolete:
	    statemap |= sd_obsoletestate;
	    (plex->sddowncount)++;			    /* another unusable subdisk */
	    break;

	case sd_stale:
	    statemap |= sd_stalestate;
	    (plex->sddowncount)++;			    /* another unusable subdisk */
	    break;

	case sd_reborn:
	    statemap |= sd_rebornstate;
	    break;

	case sd_up:
	    statemap |= sd_upstate;
	    break;

	case sd_initializing:
	    statemap |= sd_initstate;
	    (plex->sddowncount)++;			    /* another unusable subdisk */
	    break;

	case sd_initialized:
	    statemap |= sd_initializedstate;
	    (plex->sddowncount)++;			    /* another unusable subdisk */
	    break;

	case sd_unallocated:
	case sd_uninit:
	case sd_reviving:
	case sd_referenced:
	    statemap |= sd_otherstate;
	    (plex->sddowncount)++;			    /* another unusable subdisk */
	}
    }
    return statemap;
}

/* determine the state of the volume relative to this plex */
enum volplexstate
vpstate(struct plex *plex)
{
    struct volume *vol;
    enum volplexstate state = volplex_onlyusdown;	    /* state to return */
    int plexno;

    if (plex->volno < 0) {				    /* not associated with a volume */
	if (plex->state > plex_degraded)
	    return volplex_onlyus;			    /* just us */
	else
	    return volplex_onlyusdown;			    /* assume the worst */
    }
    vol = &VOL[plex->volno];				    /* point to our volume */
    for (plexno = 0; plexno < vol->plexes; plexno++) {
	if (&PLEX[vol->plex[plexno]] == plex) {		    /* us */
	    if (PLEX[vol->plex[plexno]].state >= plex_degraded)	/* are we up? */
		state |= volplex_onlyus;		    /* yes */
	} else {
	    if (PLEX[vol->plex[plexno]].state >= plex_degraded)	/* not us */
		state |= volplex_otherup;		    /* and when they were up, they were up */
	    else
		state |= volplex_alldown;		    /* and when they were down, they were down */
	}
    }
    return state;					    /* and when they were only halfway up */
}							    /* they were neither up nor down */

/* Check if all bits b are set in a */
int allset(int a, int b);

int
allset(int a, int b)
{
    return (a & b) == b;
}

/* Invalidate the subdisks belonging to a plex */
void
invalidate_subdisks(struct plex *plex, enum sdstate state)
{
    int sdno;

    for (sdno = 0; sdno < plex->subdisks; sdno++) {	    /* for each subdisk */
	struct sd *sd = &SD[plex->sdnos[sdno]];

	switch (sd->state) {
	case sd_unallocated:
	case sd_uninit:
	case sd_init:
	case sd_initializing:
	case sd_initialized:
	case sd_empty:
	case sd_obsolete:
	case sd_stale:
	case sd_crashed:
	case sd_down:
	case sd_referenced:
	    break;

	case sd_reviving:
	case sd_reborn:
	case sd_up:
	    set_sd_state(plex->sdnos[sdno], state, setstate_force);
	}
    }
}

/*
 * Start an object, in other words do what we can to get it up.
 * This is called from vinumioctl (VINUMSTART).
 * Return error indications via ioctl_reply
 */
void
start_object(struct vinum_ioctl_msg *data)
{
    int status;
    int objindex = data->index;				    /* data gets overwritten */
    struct _ioctl_reply *ioctl_reply = (struct _ioctl_reply *) data; /* format for returning replies */
    enum setstateflags flags;

    if (data->force != 0)				    /* are we going to use force? */
	flags = setstate_force;				    /* yes */
    else
	flags = setstate_none;				    /* no */

    switch (data->type) {
    case drive_object:
	status = set_drive_state(objindex, drive_up, flags);
	if (DRIVE[objindex].state != drive_up)		    /* set status on whether we really did it */
	    ioctl_reply->error = EBUSY;
	else
	    ioctl_reply->error = 0;
	break;

    case sd_object:
	if (DRIVE[SD[objindex].driveno].state != drive_up) {
	    ioctl_reply->error = EIO;
	    strcpy(ioctl_reply->msg, "Drive is down");
	    return;
	}
	if (data->blocksize)
	    SD[objindex].revive_blocksize = data->blocksize;
	if ((SD[objindex].state == sd_reviving)		    /* reviving, */
	||(SD[objindex].state == sd_stale)) {		    /* or stale, will revive */
	    SD[objindex].state = sd_reviving;		    /* make sure we're reviving */
	    ioctl_reply->error = revive_block(objindex);    /* revive another block */
	    ioctl_reply->msg[0] = '\0';			    /* no comment */
	    return;
	} else if (SD[objindex].state == sd_initializing) { /* initializing, */
	    if (data->blocksize)
		SD[objindex].init_blocksize = data->blocksize;
	    ioctl_reply->error = initsd(objindex, data->verify); /* initialize another block */
	    ioctl_reply->msg[0] = '\0';			    /* no comment */
	    return;
	}
	status = set_sd_state(objindex, sd_up, flags);	    /* set state */
	if (status != EAGAIN) {				    /* not first revive or initialize, */
	    if (SD[objindex].state != sd_up)		    /* set status on whether we really did it */
		ioctl_reply->error = EBUSY;
	    else
		ioctl_reply->error = 0;
	} else
	    ioctl_reply->error = status;
	break;

    case plex_object:
	status = set_plex_state(objindex, plex_up, flags);
	if (PLEX[objindex].state != plex_up)		    /* set status on whether we really did it */
	    ioctl_reply->error = EBUSY;
	else
	    ioctl_reply->error = 0;
	break;

    case volume_object:
	status = set_volume_state(objindex, volume_up, flags);
	if (VOL[objindex].state != volume_up)		    /* set status on whether we really did it */
	    ioctl_reply->error = EBUSY;
	else
	    ioctl_reply->error = 0;
	break;

    default:
	ioctl_reply->error = EINVAL;
	strcpy(ioctl_reply->msg, "Invalid object type");
	return;
    }
    /*
     * There's no point in saying anything here:
     * the userland program does it better
     */
    ioctl_reply->msg[0] = '\0';
}

/*
 * Stop an object, in other words do what we can to get it down
 * This is called from vinumioctl (VINUMSTOP).
 * Return error indications via ioctl_reply.
 */
void
stop_object(struct vinum_ioctl_msg *data)
{
    int status = 1;
    int objindex = data->index;				    /* save the number from change */
    struct _ioctl_reply *ioctl_reply = (struct _ioctl_reply *) data; /* format for returning replies */

    switch (data->type) {
    case drive_object:
	status = set_drive_state(objindex, drive_down, data->force);
	break;

    case sd_object:
	status = set_sd_state(objindex, sd_down, data->force);
	break;

    case plex_object:
	status = set_plex_state(objindex, plex_down, data->force);
	break;

    case volume_object:
	status = set_volume_state(objindex, volume_down, data->force);
	break;

    default:
	ioctl_reply->error = EINVAL;
	strcpy(ioctl_reply->msg, "Invalid object type");
	return;
    }
    ioctl_reply->msg[0] = '\0';
    if (status == 0)					    /* couldn't do it */
	ioctl_reply->error = EBUSY;
    else
	ioctl_reply->error = 0;
}

/*
 * VINUM_SETSTATE ioctl: set an object state.
 * msg is the message passed by the user.
 */
void
setstate(struct vinum_ioctl_msg *msg)
{
    int sdno;
    struct sd *sd;
    struct plex *plex;
    struct _ioctl_reply *ioctl_reply = (struct _ioctl_reply *) msg; /* format for returning replies */

    switch (msg->state) {
    case object_down:
	stop_object(msg);
	break;

    case object_initializing:
	switch (msg->type) {
	case sd_object:
	    sd = &SD[msg->index];
	    if ((msg->index >= vinum_conf.subdisks_allocated)
		|| (sd->state <= sd_referenced)) {
		sprintf(ioctl_reply->msg, "Invalid subdisk %d", msg->index);
		ioctl_reply->error = EFAULT;
		return;
	    }
	    set_sd_state(msg->index, sd_initializing, msg->force);
	    if (sd->state != sd_initializing) {
		strcpy(ioctl_reply->msg, "Can't set state");
		ioctl_reply->error = EBUSY;
	    } else
		ioctl_reply->error = 0;
	    break;

	case plex_object:
	    plex = &PLEX[msg->index];
	    if ((msg->index >= vinum_conf.plexes_allocated)
		|| (plex->state <= plex_unallocated)) {
		sprintf(ioctl_reply->msg, "Invalid plex %d", msg->index);
		ioctl_reply->error = EFAULT;
		return;
	    }
	    set_plex_state(msg->index, plex_initializing, msg->force);
	    if (plex->state != plex_initializing) {
		strcpy(ioctl_reply->msg, "Can't set state");
		ioctl_reply->error = EBUSY;
	    } else {
		ioctl_reply->error = 0;
		for (sdno = 0; sdno < plex->subdisks; sdno++) {
		    sd = &SD[plex->sdnos[sdno]];
		    set_sd_state(plex->sdnos[sdno], sd_initializing, msg->force);
		    if (sd->state != sd_initializing) {
			strcpy(ioctl_reply->msg, "Can't set state");
			ioctl_reply->error = EBUSY;
			break;
		    }
		}
	    }
	    break;

	default:
	    strcpy(ioctl_reply->msg, "Invalid object");
	    ioctl_reply->error = EINVAL;
	}
	break;

    case object_initialized:
	if (msg->type == sd_object) {
	    sd = &SD[msg->index];
	    if ((msg->index >= vinum_conf.subdisks_allocated)
		|| (sd->state <= sd_referenced)) {
		sprintf(ioctl_reply->msg, "Invalid subdisk %d", msg->index);
		ioctl_reply->error = EFAULT;
		return;
	    }
	    set_sd_state(msg->index, sd_initialized, msg->force);
	    if (sd->state != sd_initializing) {
		strcpy(ioctl_reply->msg, "Can't set state");
		ioctl_reply->error = EBUSY;
	    } else
		ioctl_reply->error = 0;
	} else {
	    strcpy(ioctl_reply->msg, "Invalid object");
	    ioctl_reply->error = EINVAL;
	}
	break;

    case object_up:
	start_object(msg);
    }
}

/*
 * Brute force set state function.  Don't look at
 * any dependencies, just do it.  This is mainly
 * intended for testing and recovery.
 */
void
setstate_by_force(struct vinum_ioctl_msg *msg)
{
    struct _ioctl_reply *ioctl_reply = (struct _ioctl_reply *) msg; /* format for returning replies */

    switch (msg->type) {
    case drive_object:
	DRIVE[msg->index].state = msg->state;
	break;

    case sd_object:
	SD[msg->index].state = msg->state;
	break;

    case plex_object:
	PLEX[msg->index].state = msg->state;
	break;

    case volume_object:
	VOL[msg->index].state = msg->state;
	break;

    default:
    }
    ioctl_reply->error = 0;
}
/* Local Variables: */
/* fill-column: 50 */
/* End: */
