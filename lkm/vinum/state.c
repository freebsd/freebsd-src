/*-
 * Copyright (c) 1997, 1998
 *	Nan Yang Computer Services Limited.  All rights reserved.
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
 * $Id: state.c,v 2.6 1998/08/19 08:04:47 grog Exp grog $
 */

#define REALLYKERNEL
#include "vinumhdr.h"
#include "request.h"

/* Update drive state */
/* Return 1 if the state changes, otherwise 0 */
int 
set_drive_state(int driveno, enum drivestate state, int flags)
{
    struct drive *drive = &DRIVE[driveno];
    int oldstate = drive->state;
    int sdno;

    if (drive->state == drive_unallocated)		    /* no drive to do anything with, */
	return 0;

    if (state != oldstate) {				    /* don't change it if it's not different */
	if (state == drive_down) {			    /* the drive's going down */
	    if (flags || (drive->opencount == 0)) {	    /* we can do it */
		close_drive(drive);
		drive->state = state;
		printf("vinum: drive %s is %s\n", drive->label.name, drive_state(drive->state));
	    } else
		return 0;				    /* don't do it */
	}
	drive->state = state;				    /* set the state */
	if (((drive->state == drive_up)
		|| ((drive->state == drive_coming_up)))
	    && (drive->vp == NULL))			    /* should be open, but we're not */
	    init_drive(drive);				    /* which changes the state again */
	if ((state != oldstate)				    /* state has changed */
	&&((flags & setstate_norecurse) == 0)) {	    /* and we want to recurse, */
	    for (sdno = 0; sdno < vinum_conf.subdisks_used; sdno++) { /* find this drive's subdisks */
		if (SD[sdno].driveno == driveno)	    /* belongs to this drive */
		    set_sd_state(sdno, sd_down, setstate_force | setstate_recursing); /* take it down */
	    }
	    save_config();				    /* and save the updated configuration */
	    return 1;
	}
    }
    return 0;
}

/* Try to set the subdisk state.  Return 1 if state changed to
 * what we wanted, -1 if it changed to something else, and 0
 * if no change.
 *
 * This routine is called both from the user (up, down states
 * only) and internally.
 */
int 
set_sd_state(int sdno, enum sdstate state, enum setstateflags flags)
{
    struct sd *sd = &SD[sdno];
    int oldstate = sd->state;
    int status = 1;					    /* status to return */

    if (state == oldstate)
	return 0;					    /* no change */

    if (sd->state == sd_unallocated)			    /* no subdisk to do anything with, */
	return 0;

    if (sd->driveoffset < 0) {				    /* not allocated space */
	sd->state = sd_down;
	if (state != sd_down)
	    return -1;
    } else {						    /*  space allocated */
	switch (state) {
	case sd_down:
	    if ((!flags & setstate_force)		    /* but gently */
	    &&(sd->plexno >= 0))			    /* and we're attached to a plex, */
		return 0;				    /* don't do it */
	    break;

	case sd_up:
	    if (DRIVE[sd->driveno].state != drive_up)	    /* can't bring the sd up if the drive isn't, */
		return 0;				    /* not even by force */
	    switch (sd->state) {
	    case sd_obsolete:
	    case sd_down:				    /* been down, no data lost */
		if ((sd->plexno)			    /* we're associated with a plex */
		&&(((PLEX[sd->plexno].state < plex_firstup) /* and it's not up */
		||(PLEX[sd->plexno].subdisks > 1))))	    /* or it's the only one */
		    break;
		/* XXX Get this right: make sure that other plexes in
		 * the volume cover this address space, otherwise
		 * we make this one sd_up */
		sd->state = sd_reborn;			    /* here it is again */
		printf("vinum: subdisk %s is %s, not %s\n", sd->name, sd_state(sd->state), sd_state(state));
		status = -1;
		break;

	    case sd_init:				    /* brand new */
		if (flags & setstate_configuring)	    /* we're doing this while configuring */
		    break;
		sd->state = sd_empty;			    /* nothing in it */
		printf("vinum: subdisk %s is %s, not %s\n", sd->name, sd_state(sd->state), sd_state(state));
		status = -1;
		break;

	    case sd_initializing:
		break;					    /* go on and do it */

	    case sd_empty:
		if ((sd->plexno)			    /* we're associated with a plex */
		&&(((PLEX[sd->plexno].state < plex_firstup) /* and it's not up */
		||(PLEX[sd->plexno].subdisks > 1))))	    /* or it's the only one */
		    break;
		return 0;				    /* can't do it */

	    default:					    /* can't do it */
		/* There's no way to bring subdisks up directly from
		 * other states.  First they need to be initialized
		 * or revived */
		return 0;
	    }
	    break;

	default:					    /* other ones, only internal with force */
	    if (flags & setstate_force == 0)		    /* no force?  What's this? */
		return 0;				    /* don't do it */
	}
    }
    sd->state = state;
    printf("vinum: subdisk %s is %s\n", sd->name, sd_state(sd->state));
    if ((flags & setstate_norecurse) == 0)
	set_plex_state(sd->plexno, plex_up, setstate_recursing); /* update plex state */
    if ((flags & (setstate_configuring | setstate_recursing)) == 0) /* save config now */
	save_config();
    return status;
}

/* Called from request routines when they find
 * a subdisk which is not kosher.  Decide whether
 * it warrants changing the state.  Return
 * REQUEST_DOWN if we can't use the subdisk,
 * REQUEST_OK if we can. */
enum requeststatus 
checksdstate(struct sd *sd, struct request *rq, daddr_t diskaddr, daddr_t diskend)
{
    struct plex *plex = &PLEX[sd->plexno];
    int writeop = (rq->bp->b_flags & B_READ) == 0;	    /* note if we're writing */

    /* first, see if the plex wants to be accessed */
    switch (plex->state) {
    case plex_reviving:
	/* When writing, we'll write anything that starts
	 * up to the current revive pointer, but we'll
	 * only accept a read which finishes before the
	 * current revive pointer.
	 */
	if ((writeop && (diskaddr > plex->revived))	    /* write starts after current revive pointer */
	||((!writeop) && (diskend >= plex->revived))) {	    /* or read ends after current revive pointer */
	    if (writeop) {				    /* writing to a consistent down disk */
		if (DRIVE[sd->driveno].state == drive_up)
		    set_sd_state(sd->sdno, sd_stale, setstate_force); /* it's not consistent now */
		else
		    set_sd_state(sd->sdno, sd_obsolete, setstate_force); /* it's not consistent now */
	    }
	    return REQUEST_DOWN;			    /* that part of the plex is still down */
	} else if (diskend >= plex->revived)		    /* write finishes beyond revive pointer */
	    rq->flags |= XFR_REVIVECONFLICT;		    /* note a potential conflict */
	/* FALLTHROUGH */

    case plex_up:
    case plex_degraded:
    case plex_flaky:
	/* We can access the plex: let's see
	 * how the subdisk feels */
	switch (sd->state) {
	case sd_up:
	    return REQUEST_OK;

	case sd_reborn:
	    if (writeop)
		return REQUEST_OK;			    /* always write to a reborn disk */
	    /* Handle the mapping.  We don't want to reject
	     * a read request to a reborn subdisk if that's
	     * all we have. XXX */
	    return REQUEST_DOWN;

	case sd_down:
	case sd_crashed:
	    if (writeop) {				    /* writing to a consistent down disk */
		if (DRIVE[sd->driveno].state == drive_up)
		    set_sd_state(sd->sdno, sd_stale, setstate_force); /* it's not consistent now */
		else
		    set_sd_state(sd->sdno, sd_obsolete, setstate_force); /* it's not consistent now */
	    }
	    return REQUEST_DOWN;			    /* and it's down one way or another */

	default:
	    return REQUEST_DOWN;
	}

    default:
	return REQUEST_DOWN;
    }
}

void 
add_defective_region(struct plex *plex, off_t offset, size_t length)
{
/* XXX get this ordered, and coalesce regions if necessary */
    if (++plex->defective_regions > plex->defective_region_count)
	EXPAND(plex->defective_region,
	    struct plexregion,
	    plex->defective_region_count,
	    PLEX_REGION_TABLE_SIZE);
    plex->defective_region[plex->defective_regions - 1].offset = offset;
    plex->defective_region[plex->defective_regions - 1].length = length;
}

void 
add_unmapped_region(struct plex *plex, off_t offset, size_t length)
{
    if (++plex->unmapped_regions > plex->unmapped_region_count)
	EXPAND(plex->unmapped_region,
	    struct plexregion,
	    plex->unmapped_region_count,
	    PLEX_REGION_TABLE_SIZE);
    plex->unmapped_region[plex->unmapped_regions - 1].offset = offset;
    plex->unmapped_region[plex->unmapped_regions - 1].length = length;
}

/* Rebuild a plex free list and set state if
 * we have a configuration error */
void 
rebuild_plex_unmappedlist(struct plex *plex)
{
    int sdno;
    struct sd *sd;
    int lastsdend = 0;					    /* end offset of last subdisk */

    if (plex->unmapped_region != NULL) {		    /* we're going to rebuild it */
	Free(plex->unmapped_region);
	plex->unmapped_region = NULL;
	plex->unmapped_regions = 0;
	plex->unmapped_region_count = 0;
    }
    if (plex->defective_region != NULL) {
	Free(plex->defective_region);
	plex->defective_region = NULL;
	plex->defective_regions = 0;
	plex->defective_region_count = 0;
    }
    for (sdno = 0; sdno < plex->subdisks; sdno++) {
	sd = &SD[plex->sdnos[sdno]];
	if (sd->plexoffset < lastsdend) {		    /* overlap */
	    printf("vinum: Plex %s, subdisk %s overlaps previous\n", plex->name, sd->name);
	    set_plex_state(plex->plexno, plex_down, setstate_force); /* don't allow that */
	} else if (sd->plexoffset > lastsdend)		    /* gap */
	    add_unmapped_region(plex, lastsdend, sd->plexoffset - lastsdend);
	else if (sd->state < sd_reborn)			    /* this part defective */
	    add_defective_region(plex, sd->plexoffset, sd->sectors);
	lastsdend = sd->plexoffset + sd->sectors;
    }
}

/* return a state map for the subdisks of a plex */
enum sdstates 
sdstatemap(struct plex *plex, int *sddowncount)
{
    int sdno;
    enum sdstates statemap = 0;				    /* note the states we find */

    *sddowncount = 0;					    /* no subdisks down yet */
    for (sdno = 0; sdno < plex->subdisks; sdno++) {
	struct sd *sd = &SD[plex->sdnos[sdno]];		    /* point to the subdisk */

	switch (sd->state) {
	case sd_empty:
	    statemap |= sd_emptystate;
	    (*sddowncount)++;				    /* another unusable subdisk */
	    break;

	case sd_init:
	    statemap |= sd_initstate;
	    (*sddowncount)++;				    /* another unusable subdisk */
	    break;

	case sd_down:
	    statemap |= sd_downstate;
	    (*sddowncount)++;				    /* another unusable subdisk */
	    break;

	case sd_crashed:
	    statemap |= sd_crashedstate;
	    (*sddowncount)++;				    /* another unusable subdisk */
	    break;

	case sd_obsolete:
	    statemap |= sd_obsolete;
	    (*sddowncount)++;				    /* another unusable subdisk */
	    break;

	case sd_stale:
	    statemap |= sd_stalestate;
	    (*sddowncount)++;				    /* another unusable subdisk */
	    break;

	case sd_reborn:
	    statemap |= sd_rebornstate;
	    break;

	case sd_up:
	    statemap |= sd_upstate;
	    break;

	default:
	    statemap |= sd_otherstate;
	    break;
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

    if (plex->volno < 0)				    /* not associated with a volume */
	return volplex_onlyusdown;			    /* assume the worst */

    vol = &VOL[plex->volno];				    /* point to our volume */
    for (plexno = 0; plexno < vol->plexes; plexno++) {
	if (&PLEX[vol->plex[plexno]] == plex) {		    /* us */
	    if (PLEX[vol->plex[plexno]].state == plex_up)   /* are we up? */
		state |= volplex_onlyus;		    /* yes */
	} else {
	    if (PLEX[vol->plex[plexno]].state == plex_up)   /* not us */
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

/* Update the state of a plex dependent on its subdisks.
 * Also rebuild the unmapped_region and defective_region table */
int 
set_plex_state(int plexno, enum plexstate state, enum setstateflags flags)
{
    int sddowncount = 0;				    /* number of down subdisks */
    struct plex *plex = &PLEX[plexno];			    /* point to our plex */
    enum plexstate oldstate = plex->state;
    enum volplexstate vps = vpstate(plex);		    /* how do we compare with the other plexes? */
    enum sdstates statemap = sdstatemap(plex, &sddowncount); /* get a map of the subdisk states */

    if ((flags & setstate_force) && (oldstate == state))    /* we're there already, */
	return 0;					    /* no change */

    if (plex->state == plex_unallocated)		    /* no plex to do anything with, */
	return 0;

    switch (state) {
    case plex_up:
	if ((plex->state == plex_initializing)		    /* we're initializing */
	&&(statemap != sd_upstate))			    /* but SDs aren't up yet */
	    return 0;					    /* do nothing */

	/* We don't really care what our state was before
	 * if we want to come up.  We rely entirely on the
	 * state of our subdisks and our volume */
	switch (vps) {
	case volplex_onlyusdown:
	case volplex_alldown:				    /* another plex is down, and so are we */
	    if (statemap == sd_upstate) {		    /* all subdisks ready for action */
		if ((plex->state == plex_init)		    /* we're brand spanking new */
		&&(VOL[plex->volno].flags & VF_CONFIG_SETUPSTATE)) { /* and we consider that up */
							    /* Conceptually, an empty plex does not contain valid data,
		     * but normally we'll see this state when we have just
		     * created a plex, and it's either consistent from earlier,
		     * or we don't care about the previous contents (we're going
		     * to create a file system or use it for swap).
		     *
		     * We need to do this in one swell foop: on the next call
		     * we will no longer be just empty.
		     *
		     * We'll still come back to this function for the remaining
		     * plexes in the volume.  They'll be up already, so that
		     * doesn't change anything, but it's not worth the additional
		     * code to stop doing it. */
		    struct volume *vol = &VOL[plex->volno];
		    int plexno;

		    for (plexno = 0; plexno < vol->plexes; plexno++)
			PLEX[vol->plex[plexno]].state = plex_up;
		}
		plex->state = plex_up;			    /* bring up up, anyway */
	    } else
		plex->state = plex_down;
	    break;

	case volplex_onlyusup:				    /* only we are up: others are down */
	case volplex_onlyus:				    /* we're up and alone */
	    if ((statemap == sd_upstate)		    /* subdisks all up */
	    ||(statemap == sd_emptystate))		    /* or all empty */
		plex->state = plex_up;			    /* go for it */
	    else if ((statemap & (sd_upstate | sd_reborn)) == statemap)	/* all up or reborn, */
		plex->state = plex_flaky;
	    else if (statemap & (sd_upstate | sd_reborn))   /* some up or reborn, */
		plex->state = plex_degraded;		    /* so far no corruption */
	    else
		plex->state = plex_faulty;
	    break;

	case volplex_otherup:				    /* another plex is up */
	case volplex_otherupdown:			    /* other plexes are up and down */
	    if ((statemap == sd_upstate)		    /* subdisks all up */
	    ||(statemap == sd_emptystate)		    /* or all empty */
	    ) {
		/* Is the data in all subdisks valid? */
		if (statemap == statemap & (sd_downstate | sd_rebornstate | sd_upstate))
		    break;				    /* yes, we can bring the plex up */
		plex->state = plex_reviving;		    /* we need reviving */
		return EAGAIN;
	    } else
		plex->state = plex_faulty;		    /* still in error */
	    break;

	case volplex_allup:				    /* all plexes are up */
	case volplex_someup:
	    if ((statemap & (sd_upstate | sd_reborn)) == statemap) /* all up or reborn, */
		break;					    /* no change */
	    else
		plex->state = plex_degraded;		    /* we're not all there */
	}

	if (plex->state != oldstate)
	    break;
	return 0;					    /* no change */

    case plex_down:					    /* want to take it down */
	if (((vps == volplex_onlyus)			    /* we're the only one up */
	||(vps == volplex_onlyusup))			    /* we're the only one up */
	&&(!(flags & setstate_force)))			    /* and we don't want to use force */
	    return 0;					    /* can't do it */
	plex->state = state;				    /* do it */
	break;

	/* This is only requested by the driver.
	 * Trust ourselves */
    case plex_faulty:
	plex->state = state;				    /* do it */
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
    printf("vinum: plex %s is %s\n", plex->name, plex_state(plex->state));
    /* Now see what we have left, and whether
     * we're taking the volume down */
    if (plex->volno >= 0) {				    /* we have a volume */
	struct volume *vol = &VOL[plex->volno];

	vps = vpstate(plex);				    /* get our combined state again */
	if ((flags & setstate_norecurse) == 0) {	    /* we can recurse */
	    if ((vol->state == volume_up)
		&& (vps == volplex_alldown))		    /* and we're all down */
		set_volume_state(plex->volno, volume_down, setstate_recursing);	/* take our volume down */
	    else if ((vol->state == volume_down)
		&& (vps & (volplex_otherup | volplex_onlyusup))) /* and at least one is up */
		set_volume_state(plex->volno, volume_up, setstate_recursing); /* bring our volume up */
	}
    }
    if ((flags & (setstate_configuring | setstate_recursing)) == 0) /* save config now */
	save_config();
    return 1;
}

/* Update the state of a plex dependent on its plexes.
 * Also rebuild the unmapped_region and defective_region table */
int 
set_volume_state(int volno, enum volumestate state, enum setstateflags flags)
{
    int plexno;
    enum plexstates {
	plex_downstate = 1,				    /* found a plex which is down */
	plex_degradedstate = 2,				    /* found a plex which is halfway up */
	plex_upstate = 4				    /* found a plex which is completely up */
    };

    int plexstatemap = 0;				    /* note the states we find */
    struct volume *vol = &VOL[volno];			    /* point to our volume */

    if (vol->state == state)				    /* we're there already */
	return 0;					    /* no change */
    if (vol->state == volume_unallocated)		    /* no volume to do anything with, */
	return 0;

    for (plexno = 0; plexno < vol->plexes; plexno++) {
	struct plex *plex = &PLEX[vol->plex[plexno]];	    /* point to the plex */
	switch (plex->state) {
	case plex_degraded:
	case plex_flaky:
	case plex_reviving:
	    plexstatemap |= plex_degradedstate;
	    break;

	case plex_up:
	    plexstatemap |= plex_upstate;
	    break;

	default:
	    plexstatemap |= plex_downstate;
	    break;
	}
    }

    if (state == volume_up) {				    /* want to come up */
	if (plexstatemap & plex_upstate) {		    /* we have a plex which is completely up */
	    vol->state = volume_up;			    /* did it */
	    printf("vinum: volume %s is %s\n", vol->name, volume_state(vol->state));
	    if ((flags & (setstate_configuring | setstate_recursing)) == 0) /* save config now */
		save_config();
	    return 1;
	}
	/* Here we should check whether we have enough
	 * coverage for the complete volume.  Writeme XXX */
    } else if (state == volume_down) {			    /* want to go down */
	if ((vol->opencount == 0)			    /* not open */
	||(flags & setstate_force != 0)) {		    /* or we're forcing */
	    vol->state = volume_down;
	    printf("vinum: volume %s is %s\n", vol->name, volume_state(vol->state));
	    if ((flags & (setstate_configuring | setstate_recursing)) == 0) /* save config now */
		save_config();
	    return 1;
	}
    }
    return 0;						    /* no change */
}

/* Start an object, in other words do what we can to get it up.
 * This is called from vinumioctl (VINUMSTART).
 * Return error indications via ioctl_reply
 */
void 
start_object(struct vinum_ioctl_msg *data)
{
    int status;
    int realstatus;					    /* what we really have */
    int objindex = data->index;				    /* data gets overwritten */
    struct _ioctl_reply *ioctl_reply = (struct _ioctl_reply *) data; /* format for returning replies */

    switch (data->type) {
    case drive_object:
	status = set_drive_state(objindex, drive_up, setstate_none);
	realstatus = DRIVE[objindex].state == drive_up;	    /* set status on whether we really did it */
	break;

    case sd_object:
	status = set_sd_state(objindex, sd_up, setstate_none); /* set state */
	realstatus = SD[objindex].state == sd_up;	    /* set status on whether we really did it */
	break;

    case plex_object:
	if (PLEX[objindex].state == plex_reviving) {	    /* reviving, */
	    ioctl_reply->error = revive_block(objindex);    /* revive another block */
	    ioctl_reply->msg[0] = '\0';			    /* no comment */
	    return;
	}
	status = set_plex_state(objindex, plex_up, setstate_none);
	realstatus = PLEX[objindex].state == plex_up;	    /* set status on whether we really did it */
	break;

    case volume_object:
	status = set_volume_state(objindex, volume_up, setstate_none);
	realstatus = VOL[objindex].state == volume_up;	    /* set status on whether we really did it */
	break;

    default:
	ioctl_reply->error = EINVAL;
	strcpy(ioctl_reply->msg, "Invalid object type");
	return;
    }
    /* There's no point in saying anything here:
     * the userland program does it better */
    ioctl_reply->msg[0] = '\0';
    if (realstatus == 0)				    /* couldn't do it */
	ioctl_reply->error = EINVAL;
    else
	ioctl_reply->error = 0;
}

/* Stop an object, in other words do what we can to get it down
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
	ioctl_reply->error = EINVAL;
    else
	ioctl_reply->error = 0;
}

/* VINUM_SETSTATE ioctl: set an object state
 * msg is the message passed by the user */
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
	    if ((msg->index >= vinum_conf.subdisks_used)
		|| (sd->state == sd_unallocated)) {
		sprintf(ioctl_reply->msg, "Invalid subdisk %d", msg->index);
		ioctl_reply->error = EFAULT;
		return;
	    }
	    set_sd_state(msg->index, sd_initializing, msg->force);
	    if (sd->state != sd_initializing) {
		strcpy(ioctl_reply->msg, "Can't set state");
		ioctl_reply->error = EINVAL;
	    } else
		ioctl_reply->error = 0;
	    break;

	case plex_object:
	    plex = &PLEX[msg->index];
	    if ((msg->index >= vinum_conf.plexes_used)
		|| (plex->state == plex_unallocated)) {
		sprintf(ioctl_reply->msg, "Invalid subdisk %d", msg->index);
		ioctl_reply->error = EFAULT;
		return;
	    }
	    set_plex_state(msg->index, plex_initializing, msg->force);
	    if (plex->state != plex_initializing) {
		strcpy(ioctl_reply->msg, "Can't set state");
		ioctl_reply->error = EINVAL;
	    } else {
		ioctl_reply->error = 0;
		for (sdno = 0; sdno < plex->subdisks; sdno++) {
		    sd = &SD[plex->sdnos[sdno]];
		    set_sd_state(plex->sdnos[sdno], sd_initializing, msg->force);
		    if (sd->state != sd_initializing) {
			strcpy(ioctl_reply->msg, "Can't set state");
			ioctl_reply->error = EINVAL;
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

    case object_up:
	start_object(msg);
    }
}
