/*
 * XXX replace all the checks on object validity with
 * calls to valid<object>
 */
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
 * $Id: vinumioctl.c,v 1.14 2000/10/27 03:07:53 grog Exp grog $
 * $FreeBSD$
 */

#include <dev/vinum/vinumhdr.h>
#include <dev/vinum/request.h>

#ifdef VINUMDEBUG
#include <sys/reboot.h>
#endif

void attachobject(struct vinum_ioctl_msg *);
void detachobject(struct vinum_ioctl_msg *);
void renameobject(struct vinum_rename_msg *);
void replaceobject(struct vinum_ioctl_msg *);
void moveobject(struct vinum_ioctl_msg *);

jmp_buf command_fail;					    /* return on a failed command */

/* ioctl routine */
int
vinumioctl(dev_t dev,
    u_long cmd,
    caddr_t data,
    int flag,
    struct proc *p)
{
    unsigned int objno;
    int error = 0;
    struct sd *sd;
    struct plex *plex;
    struct volume *vol;
    unsigned int index;					    /* for transferring config info */
    unsigned int sdno;					    /* for transferring config info */
    int fe;						    /* free list element number */
    struct _ioctl_reply *ioctl_reply = (struct _ioctl_reply *) data; /* struct to return */

    /* First, decide what we're looking at */
    switch (DEVTYPE(dev)) {
    case VINUM_SUPERDEV_TYPE:				    /* ordinary super device */
	ioctl_reply = (struct _ioctl_reply *) data;	    /* save the address to reply to */
	error = setjmp(command_fail);			    /* come back here on error */
	if (error)					    /* bombed out */
	    return 0;					    /* the reply will contain meaningful info */
	switch (cmd) {
#ifdef VINUMDEBUG
	case VINUM_DEBUG:
	    if (((struct debuginfo *) data)->changeit)	    /* change debug settings */
		debug = (((struct debuginfo *) data)->param);
	    else {
		if (debug & DEBUG_REMOTEGDB)
		    boothowto |= RB_GDB;		    /* serial debug line */
		else
		    boothowto &= ~RB_GDB;		    /* local ddb */
		Debugger("vinum debug");
	    }
	    ioctl_reply = (struct _ioctl_reply *) data;	    /* reinstate the address to reply to */
	    ioctl_reply->error = 0;
	    return 0;
#endif

	case VINUM_CREATE:				    /* create a vinum object */
	    error = lock_config();			    /* get the config for us alone */
	    if (error)					    /* can't do it, */
		return error;				    /* give up */
	    error = setjmp(command_fail);		    /* come back here on error */
	    if (error == 0)				    /* first time, */
		ioctl_reply->error = parse_user_config((char *) data, /* update the config */
		    &keyword_set);
	    else if (ioctl_reply->error == 0) {		    /* longjmp, but no error status */
		ioctl_reply->error = EINVAL;		    /* note that something's up */
		ioctl_reply->msg[0] = '\0';		    /* no message? */
	    }
	    unlock_config();
	    return 0;					    /* must be 0 to return the real error info */

	case VINUM_GETCONFIG:				    /* get the configuration information */
	    bcopy(&vinum_conf, data, sizeof(vinum_conf));
	    return 0;

	    /* start configuring the subsystem */
	case VINUM_STARTCONFIG:
	    return start_config(*(int *) data);		    /* just lock it.  Parameter is 'force' */

	    /*
	     * Move the individual parts of the config to user space.
	     *
	     * Specify the index of the object in the first word of data,
	     * and return the object there
	     */
	case VINUM_DRIVECONFIG:
	    index = *(int *) data;			    /* get the index */
	    if (index >= (unsigned) vinum_conf.drives_allocated) /* can't do it */
		return ENXIO;				    /* bang */
	    bcopy(&DRIVE[index], data, sizeof(struct drive)); /* copy the config item out */
	    return 0;

	case VINUM_SDCONFIG:
	    index = *(int *) data;			    /* get the index */
	    if (index >= (unsigned) vinum_conf.subdisks_allocated) /* can't do it */
		return ENXIO;				    /* bang */
	    bcopy(&SD[index], data, sizeof(struct sd));	    /* copy the config item out */
	    return 0;

	case VINUM_PLEXCONFIG:
	    index = *(int *) data;			    /* get the index */
	    if (index >= (unsigned) vinum_conf.plexes_allocated) /* can't do it */
		return ENXIO;				    /* bang */
	    bcopy(&PLEX[index], data, sizeof(struct plex)); /* copy the config item out */
	    return 0;

	case VINUM_VOLCONFIG:
	    index = *(int *) data;			    /* get the index */
	    if (index >= (unsigned) vinum_conf.volumes_allocated) /* can't do it */
		return ENXIO;				    /* bang */
	    bcopy(&VOL[index], data, sizeof(struct volume)); /* copy the config item out */
	    return 0;

	case VINUM_PLEXSDCONFIG:
	    index = *(int *) data;			    /* get the plex index */
	    sdno = ((int *) data)[1];			    /* and the sd index */
	    if ((index >= (unsigned) vinum_conf.plexes_allocated) /* plex doesn't exist */
	    ||(sdno >= PLEX[index].subdisks))		    /* or it doesn't have this many subdisks */
		return ENXIO;				    /* bang */
	    bcopy(&SD[PLEX[index].sdnos[sdno]],		    /* copy the config item out */
		data,
		sizeof(struct sd));
	    return 0;

	    /*
	     * We get called in two places: one from the
	     * userland config routines, which call us
	     * to complete the config and save it.  This
	     * call supplies the value 0 as a parameter.
	     *
	     * The other place is from the user "saveconfig"
	     * routine, which can only work if we're *not*
	     * configuring.  In this case, supply parameter 1.
	     */
	case VINUM_SAVECONFIG:
	    if (VFLAGS & VF_CONFIGURING) {		    /* must be us, the others are asleep */
		if (*(int *) data == 0)			    /* finish config */
		    finish_config(1);			    /* finish the configuration and update it */
		else
		    return EBUSY;			    /* can't do it now */
	    }
	    save_config();				    /* save configuration to disk */
	    return 0;

	case VINUM_RELEASECONFIG:			    /* release the config */
	    if (VFLAGS & VF_CONFIGURING) {		    /* must be us, the others are asleep */
		finish_config(0);			    /* finish the configuration, don't change it */
		save_config();				    /* save configuration to disk */
	    } else
		error = EINVAL;				    /* release what config? */
	    return error;

	case VINUM_INIT:
	    ioctl_reply = (struct _ioctl_reply *) data;	    /* reinstate the address to reply to */
	    ioctl_reply->error = 0;
	    return 0;

	case VINUM_RESETCONFIG:
	    if (vinum_inactive(0)) {			    /* if the volumes are not active */
		/*
		 * Note the open count.  We may be called from v, so we'll be open.
		 * Keep the count so we don't underflow
		 */
		free_vinum(1);				    /* clean up everything */
		log(LOG_NOTICE, "vinum: CONFIGURATION OBLITERATED\n");
		ioctl_reply = (struct _ioctl_reply *) data; /* reinstate the address to reply to */
		ioctl_reply->error = 0;
		return 0;
	    }
	    return EBUSY;

	case VINUM_SETSTATE:
	    setstate((struct vinum_ioctl_msg *) data);	    /* set an object state */
	    return 0;

	    /*
	     * Set state by force, without changing
	     * anything else.
	     */
	case VINUM_SETSTATE_FORCE:
	    setstate_by_force((struct vinum_ioctl_msg *) data);	/* set an object state */
	    return 0;

#ifdef VINUMDEBUG
	case VINUM_MEMINFO:
	    vinum_meminfo(data);
	    return 0;

	case VINUM_MALLOCINFO:
	    return vinum_mallocinfo(data);

	case VINUM_RQINFO:
	    return vinum_rqinfo(data);
#endif

	case VINUM_LABEL:				    /* label a volume */
	    ioctl_reply->error = write_volume_label(*(int *) data); /* index of the volume to label */
	    ioctl_reply->msg[0] = '\0';			    /* no message */
	    return 0;

	case VINUM_REMOVE:
	    remove((struct vinum_ioctl_msg *) data);	    /* remove an object */
	    return 0;

	case VINUM_GETFREELIST:				    /* get a drive free list element */
	    index = *(int *) data;			    /* get the drive index */
	    fe = ((int *) data)[1];			    /* and the free list element */
	    if ((index >= (unsigned) vinum_conf.drives_allocated) /* plex doesn't exist */
	    ||(DRIVE[index].state == drive_unallocated))
		return ENODEV;
	    if (fe >= DRIVE[index].freelist_entries)	    /* no such entry */
		return ENOENT;
	    bcopy(&DRIVE[index].freelist[fe],
		data,
		sizeof(struct drive_freelist));
	    return 0;

	case VINUM_RESETSTATS:
	    resetstats((struct vinum_ioctl_msg *) data);    /* reset object stats */
	    return 0;

	    /* attach an object to a superordinate object */
	case VINUM_ATTACH:
	    attachobject((struct vinum_ioctl_msg *) data);
	    return 0;

	    /* detach an object from a superordinate object */
	case VINUM_DETACH:
	    detachobject((struct vinum_ioctl_msg *) data);
	    return 0;

	    /* rename an object */
	case VINUM_RENAME:
	    renameobject((struct vinum_rename_msg *) data);
	    return 0;

	    /* replace an object */
	case VINUM_REPLACE:
	    replaceobject((struct vinum_ioctl_msg *) data);
	    return 0;

	case VINUM_DAEMON:
	    vinum_daemon();				    /* perform the daemon */
	    return 0;

	case VINUM_FINDDAEMON:				    /* check for presence of daemon */
	    return vinum_finddaemon();
	    return 0;

	case VINUM_SETDAEMON:				    /* set daemon flags */
	    return vinum_setdaemonopts(*(int *) data);

	case VINUM_GETDAEMON:				    /* get daemon flags */
	    *(int *) data = daemon_options;
	    return 0;

	case VINUM_PARITYOP:				    /* check/rebuild RAID-4/5 parity */
	    parityops((struct vinum_ioctl_msg *) data);
	    return 0;

	    /* move an object */
	case VINUM_MOVE:
	    moveobject((struct vinum_ioctl_msg *) data);
	    return 0;

	default:
	    /* FALLTHROUGH */
	}

    case VINUM_DRIVE_TYPE:
    default:
	log(LOG_WARNING,
	    "vinumioctl: invalid ioctl from process %d (%s): %lx\n",
	    curproc->p_pid,
	    curproc->p_comm,
	    cmd);
	return EINVAL;

    case VINUM_SD_TYPE:
    case VINUM_RAWSD_TYPE:
	objno = Sdno(dev);

	sd = &SD[objno];

	switch (cmd) {
	case DIOCGDINFO:				    /* get disk label */
	    get_volume_label(sd->name, 1, sd->sectors, (struct disklabel *) data);
	    break;

	    /*
	     * We don't have this stuff on hardware,
	     * so just pretend to do it so that
	     * utilities don't get upset.
	     */
	case DIOCWDINFO:				    /* write partition info */
	case DIOCSDINFO:				    /* set partition info */
	    return 0;					    /* not a titty */

	default:
	    return ENOTTY;				    /* not my kind of ioctl */
	}

	return 0;					    /* pretend we did it */

    case VINUM_RAWPLEX_TYPE:
    case VINUM_PLEX_TYPE:
	objno = Plexno(dev);

	plex = &PLEX[objno];

	switch (cmd) {
	case DIOCGDINFO:				    /* get disk label */
	    get_volume_label(plex->name, 1, plex->length, (struct disklabel *) data);
	    break;

	    /*
	     * We don't have this stuff on hardware,
	     * so just pretend to do it so that
	     * utilities don't get upset.
	     */
	case DIOCWDINFO:				    /* write partition info */
	case DIOCSDINFO:				    /* set partition info */
	    return 0;					    /* not a titty */

	default:
	    return ENOTTY;				    /* not my kind of ioctl */
	}

	return 0;					    /* pretend we did it */

    case VINUM_VOLUME_TYPE:
	objno = Volno(dev);

	if ((unsigned) objno >= (unsigned) vinum_conf.volumes_allocated) /* not a valid volume */
	    return ENXIO;
	vol = &VOL[objno];
	if (vol->state != volume_up)			    /* not up, */
	    return EIO;					    /* I/O error */

	switch (cmd) {
	case DIOCGDINFO:				    /* get disk label */
	    get_volume_label(vol->name, vol->plexes, vol->size, (struct disklabel *) data);
	    break;

	    /*
	     * Care!  DIOCGPART returns *pointers* to
	     * the caller, so we need to store this crap
	     * as well.  And yes, we need it.
	     */
	case DIOCGPART:					    /* get partition information */
	    get_volume_label(vol->name, vol->plexes, vol->size, &vol->label);
	    ((struct partinfo *) data)->disklab = &vol->label;
	    ((struct partinfo *) data)->part = &vol->label.d_partitions[0];
	    break;

	    /*
	     * We don't have this stuff on hardware,
	     * so just pretend to do it so that
	     * utilities don't get upset.
	     */
	case DIOCWDINFO:				    /* write partition info */
	case DIOCSDINFO:				    /* set partition info */
	    return 0;					    /* not a titty */

	case DIOCWLABEL:				    /* set or reset label writeable */
	    if ((flag & FWRITE) == 0)			    /* not writeable? */
		return EACCES;				    /* no, die */
	    if (*(int *) data != 0)			    /* set it? */
		vol->flags |= VF_WLABEL;		    /* yes */
	    else
		vol->flags &= ~VF_WLABEL;		    /* no, reset */
	    break;

	default:
	    return ENOTTY;				    /* not my kind of ioctl */
	}
	break;
    }
    return 0;						    /* XXX */
}

/*
 * The following four functions check the supplied
 * object index and return a pointer to the object
 * if it exists.  Otherwise they longjump out via
 * throw_rude_remark.
 */
struct drive *
validdrive(int driveno, struct _ioctl_reply *reply)
{
    if ((driveno < vinum_conf.drives_allocated)
	&& (DRIVE[driveno].state > drive_referenced))
	return &DRIVE[driveno];
    strcpy(reply->msg, "No such drive");
    reply->error = ENOENT;
    return NULL;
}

struct sd *
validsd(int sdno, struct _ioctl_reply *reply)
{
    if ((sdno < vinum_conf.subdisks_allocated)
	&& (SD[sdno].state > sd_referenced))
	return &SD[sdno];
    strcpy(reply->msg, "No such subdisk");
    reply->error = ENOENT;
    return NULL;
}

struct plex *
validplex(int plexno, struct _ioctl_reply *reply)
{
    if ((plexno < vinum_conf.plexes_allocated)
	&& (PLEX[plexno].state > plex_referenced))
	return &PLEX[plexno];
    strcpy(reply->msg, "No such plex");
    reply->error = ENOENT;
    return NULL;
}

struct volume *
validvol(int volno, struct _ioctl_reply *reply)
{
    if ((volno < vinum_conf.volumes_allocated)
	&& (VOL[volno].state > volume_uninit))
	return &VOL[volno];
    strcpy(reply->msg, "No such volume");
    reply->error = ENOENT;
    return NULL;
}

/* reset an object's stats */
void
resetstats(struct vinum_ioctl_msg *msg)
{
    struct _ioctl_reply *reply = (struct _ioctl_reply *) msg;

    switch (msg->type) {
    case drive_object:
	if (msg->index < vinum_conf.drives_allocated) {
	    struct drive *drive = &DRIVE[msg->index];
	    if (drive->state > drive_referenced) {
		drive->reads = 0;			    /* number of reads on this drive */
		drive->writes = 0;			    /* number of writes on this drive */
		drive->bytes_read = 0;			    /* number of bytes read */
		drive->bytes_written = 0;		    /* number of bytes written */
		reply->error = 0;
		return;
	    }
	    reply->error = EINVAL;
	    return;
	}
    case sd_object:
	if (msg->index < vinum_conf.subdisks_allocated) {
	    struct sd *sd = &SD[msg->index];
	    if (sd->state > sd_referenced) {
		sd->reads = 0;				    /* number of reads on this subdisk */
		sd->writes = 0;				    /* number of writes on this subdisk */
		sd->bytes_read = 0;			    /* number of bytes read */
		sd->bytes_written = 0;			    /* number of bytes written */
		reply->error = 0;
		return;
	    }
	    reply->error = EINVAL;
	    return;
	}
	break;

    case plex_object:
	if (msg->index < vinum_conf.plexes_allocated) {
	    struct plex *plex = &PLEX[msg->index];
	    if (plex->state > plex_referenced) {
		plex->reads = 0;
		plex->writes = 0;			    /* number of writes on this plex */
		plex->bytes_read = 0;			    /* number of bytes read */
		plex->bytes_written = 0;		    /* number of bytes written */
		plex->recovered_reads = 0;		    /* number of recovered read operations */
		plex->degraded_writes = 0;		    /* number of degraded writes */
		plex->parityless_writes = 0;		    /* number of parityless writes */
		plex->multiblock = 0;			    /* requests that needed more than one block */
		plex->multistripe = 0;			    /* requests that needed more than one stripe */
		reply->error = 0;
		return;
	    }
	    reply->error = EINVAL;
	    return;
	}
	break;

    case volume_object:
	if (msg->index < vinum_conf.volumes_allocated) {
	    struct volume *vol = &VOL[msg->index];
	    if (vol->state > volume_uninit) {
		vol->bytes_read = 0;			    /* number of bytes read */
		vol->bytes_written = 0;			    /* number of bytes written */
		vol->reads = 0;				    /* number of reads on this volume */
		vol->writes = 0;			    /* number of writes on this volume */
		vol->recovered_reads = 0;		    /* reads recovered from another plex */
		reply->error = 0;
		return;
	    }
	    reply->error = EINVAL;
	    return;
	}
    case invalid_object:				    /* can't get this */
	reply->error = EINVAL;
	return;
    }
}

/* attach an object to a superior object */
void
attachobject(struct vinum_ioctl_msg *msg)
{
    struct _ioctl_reply *reply = (struct _ioctl_reply *) msg;
    int sdno;
    struct sd *sd;
    struct plex *plex;
    struct volume *vol;

    switch (msg->type) {
    case drive_object:					    /* you can't attach a drive to anything */
    case volume_object:					    /* nor a volume */
    case invalid_object:				    /* "this can't happen" */
	reply->error = EINVAL;
	reply->msg[0] = '\0';				    /* vinum(8) doesn't do this */
	return;

    case sd_object:
	sd = validsd(msg->index, reply);
	if (sd == NULL)					    /* not a valid subdisk  */
	    return;
	plex = validplex(msg->otherobject, reply);
	if (plex) {
	    /*
	     * We should be more intelligent about this.
	     * We should be able to reattach a dead
	     * subdisk, but if we want to increase the total
	     * number of subdisks, we have a lot of reshuffling
	     * to do. XXX
	     */
	    if ((plex->organization != plex_concat)	    /* can't attach to striped and RAID-4/5 */
	    &&(!msg->force)) {				    /* without using force */
		reply->error = EINVAL;			    /* no message, the user should check */
		strcpy(reply->msg, "Can't attach to this plex organization");
		return;
	    }
	    if (sd->plexno >= 0) {			    /* already belong to a plex */
		reply->error = EBUSY;			    /* no message, the user should check */
		reply->msg[0] = '\0';
		return;
	    }
	    sd->plexoffset = msg->offset;		    /* this is where we want it */
	    set_sd_state(sd->sdno, sd_stale, setstate_force); /* make sure it's stale */
	    give_sd_to_plex(plex->plexno, sd->sdno);	    /* and give it to the plex */
	    update_sd_config(sd->sdno, 0);
	    save_config();
	}
	if (sd->state == sd_reviving)
	    reply->error = EAGAIN;			    /* need to revive it */
	else
	    reply->error = 0;
	break;

    case plex_object:
	plex = validplex(msg->index, reply);		    /* get plex */
	if (plex == NULL)
	    return;
	vol = validvol(msg->otherobject, reply);	    /* and volume information */
	if (vol) {
	    if ((vol->plexes == MAXPLEX)		    /* we have too many already */
	    ||(plex->volno >= 0)) {			    /* or the plex has an owner */
		reply->error = EINVAL;			    /* no message, the user should check */
		reply->msg[0] = '\0';
		return;
	    }
	    for (sdno = 0; sdno < plex->subdisks; sdno++) {
		sd = &SD[plex->sdnos[sdno]];

		if (sd->state > sd_down)		    /* real subdisk, vaguely accessible */
		    set_sd_state(plex->sdnos[sdno], sd_stale, setstate_force); /* make it stale */
	    }
	    set_plex_state(plex->plexno, plex_up, setstate_none); /* update plex state */
	    give_plex_to_volume(msg->otherobject, msg->index); /* and give it to the volume */
	    update_plex_config(plex->plexno, 0);
	    save_config();
	}
    }
}

/* detach an object from a superior object */
void
detachobject(struct vinum_ioctl_msg *msg)
{
    struct _ioctl_reply *reply = (struct _ioctl_reply *) msg;
    struct sd *sd;
    struct plex *plex;
    struct volume *vol;
    int sdno;
    int plexno;

    switch (msg->type) {
    case drive_object:					    /* you can't detach a drive from anything */
    case volume_object:					    /* nor a volume */
    case invalid_object:				    /* "this can't happen" */
	reply->error = EINVAL;
	reply->msg[0] = '\0';				    /* vinum(8) doesn't do this */
	return;

    case sd_object:
	sd = validsd(msg->index, reply);
	if (sd == NULL)
	    return;
	if (sd->plexno < 0) {				    /* doesn't belong to a plex */
	    reply->error = ENOENT;
	    strcpy(reply->msg, "Subdisk is not attached");
	    return;
	} else {					    /* valid plex number */
	    plex = &PLEX[sd->plexno];
	    if ((!msg->force)				    /* don't force things */
	    &&((plex->state == plex_up)			    /* and the plex is up */
	    ||((plex->state == plex_flaky) && sd->state == sd_up))) { /* or flaky with this sd up */
		reply->error = EBUSY;			    /* we need this sd */
		reply->msg[0] = '\0';
		return;
	    }
	    sd->plexno = -1;				    /* anonymous sd */
	    if (plex->subdisks == 1) {			    /* this was the only subdisk */
		Free(plex->sdnos);			    /* free the subdisk array */
		plex->sdnos = NULL;			    /* and note the fact */
		plex->subdisks_allocated = 0;		    /* no subdisk space */
	    } else {
		for (sdno = 0; sdno < plex->subdisks; sdno++) {
		    if (plex->sdnos[sdno] == msg->index)    /* found our subdisk */
			break;
		}
		if (sdno < (plex->subdisks - 1))	    /* not the last one, compact */
		    bcopy(&plex->sdnos[sdno + 1],
			&plex->sdnos[sdno],
			(plex->subdisks - 1 - sdno) * sizeof(int));
	    }
	    plex->subdisks--;
	    if (!bcmp(plex->name, sd->name, strlen(plex->name) + 1))
		/* this subdisk is named after the plex */
	    {
		bcopy(sd->name,
		    &sd->name[3],
		    min(strlen(sd->name) + 1, MAXSDNAME - 3));
		bcopy("ex-", sd->name, 3);
		sd->name[MAXSDNAME - 1] = '\0';
	    }
	    update_plex_config(plex->plexno, 0);
	    if (isstriped(plex))			    /* we've just mutilated our plex, */
		set_plex_state(plex->plexno,
		    plex_down,
		    setstate_force | setstate_configuring);
	    save_config();
	    reply->error = 0;
	}
	return;

    case plex_object:
	plex = validplex(msg->index, reply);		    /* get plex */
	if (plex == NULL)
	    return;
	if (plex->volno >= 0) {
	    int volno = plex->volno;

	    vol = &VOL[volno];
	    if ((!msg->force)				    /* don't force things */
	    &&((vol->state == volume_up)		    /* and the volume is up */
	    &&(vol->plexes == 1))) {			    /* and this is the last plex */
		/*
		   * XXX As elsewhere, check whether we will lose
		   * mapping by removing this plex
		 */
		reply->error = EBUSY;			    /* we need this plex */
		reply->msg[0] = '\0';
		return;
	    }
	    plex->volno = -1;				    /* anonymous plex */
	    for (plexno = 0; plexno < vol->plexes; plexno++) {
		if (vol->plex[plexno] == msg->index)	    /* found our plex */
		    break;
	    }
	    if (plexno < (vol->plexes - 1))		    /* not the last one, compact */
		bcopy(&vol->plex[plexno + 1],
		    &vol->plex[plexno],
		    (vol->plexes - 1 - plexno) * sizeof(int));
	    vol->plexes--;
	    vol->last_plex_read = 0;			    /* don't go beyond the end */
	    if (!bcmp(vol->name, plex->name, strlen(vol->name) + 1))
		/* this plex is named after the volume */
	    {
		/* First, check if the subdisks are the same */
		if (msg->recurse) {
		    int sdno;

		    for (sdno = 0; sdno < plex->subdisks; sdno++) {
			struct sd *sd = &SD[plex->sdnos[sdno]];

			if (!bcmp(plex->name, sd->name, strlen(plex->name) + 1))
							    /* subdisk is named after the plex */
			{
			    bcopy(sd->name,
				&sd->name[3],
				min(strlen(sd->name) + 1, MAXSDNAME - 3));
			    bcopy("ex-", sd->name, 3);
			    sd->name[MAXSDNAME - 1] = '\0';
			}
		    }
		}
		bcopy(plex->name,
		    &plex->name[3],
		    min(strlen(plex->name) + 1, MAXPLEXNAME - 3));
		bcopy("ex-", plex->name, 3);
		plex->name[MAXPLEXNAME - 1] = '\0';
	    }
	    update_volume_config(volno, 0);
	    save_config();
	    reply->error = 0;
	} else {
	    reply->error = ENOENT;
	    strcpy(reply->msg, "Plex is not attached");
	}
    }
}

void
renameobject(struct vinum_rename_msg *msg)
{
    struct _ioctl_reply *reply = (struct _ioctl_reply *) msg;
    struct drive *drive;
    struct sd *sd;
    struct plex *plex;
    struct volume *vol;

    switch (msg->type) {
    case drive_object:					    /* you can't attach a drive to anything */
	if (find_drive(msg->newname, 0) >= 0) {		    /* we have that name already, */
	    reply->error = EEXIST;
	    reply->msg[0] = '\0';
	    return;
	}
	drive = validdrive(msg->index, reply);
	if (drive) {
	    bcopy(msg->newname, drive->label.name, MAXDRIVENAME);
	    save_config();
	    reply->error = 0;
	}
	return;

    case sd_object:					    /* you can't attach a subdisk to anything */
	if (find_subdisk(msg->newname, 0) >= 0) {	    /* we have that name already, */
	    reply->error = EEXIST;
	    reply->msg[0] = '\0';
	    return;
	}
	sd = validsd(msg->index, reply);
	if (sd) {
	    bcopy(msg->newname, sd->name, MAXSDNAME);
	    update_sd_config(sd->sdno, 0);
	    save_config();
	    reply->error = 0;
	}
	return;

    case plex_object:					    /* you can't attach a plex to anything */
	if (find_plex(msg->newname, 0) >= 0) {		    /* we have that name already, */
	    reply->error = EEXIST;
	    reply->msg[0] = '\0';
	    return;
	}
	plex = validplex(msg->index, reply);
	if (plex) {
	    bcopy(msg->newname, plex->name, MAXPLEXNAME);
	    update_plex_config(plex->plexno, 0);
	    save_config();
	    reply->error = 0;
	}
	return;

    case volume_object:					    /* you can't attach a volume to anything */
	if (find_volume(msg->newname, 0) >= 0) {	    /* we have that name already, */
	    reply->error = EEXIST;
	    reply->msg[0] = '\0';
	    return;
	}
	vol = validvol(msg->index, reply);
	if (vol) {
	    bcopy(msg->newname, vol->name, MAXVOLNAME);
	    update_volume_config(msg->index, 0);
	    save_config();
	    reply->error = 0;
	}
	return;

    case invalid_object:
	reply->error = EINVAL;
	reply->msg[0] = '\0';
    }
}

/*
 * Replace one object with another.
 * Currently only for drives.
 * message->index is the drive number of the old drive
 * message->otherobject is the drive number of the new drive
 */
void
replaceobject(struct vinum_ioctl_msg *msg)
{
    struct _ioctl_reply *reply = (struct _ioctl_reply *) msg;

    reply->error = ENODEV;				    /* until I know how to do this */
    strcpy(reply->msg, "replace not implemented yet");
/*      save_config (); */
}

void
moveobject(struct vinum_ioctl_msg *msg)
{
    struct _ioctl_reply *reply = (struct _ioctl_reply *) msg;
    struct drive *drive;
    struct sd *sd;

    /* Check that our objects are valid (i.e. they exist) */
    drive = validdrive(msg->index, (struct _ioctl_reply *) msg);
    if (drive == NULL)
	return;
    sd = validsd(msg->otherobject, (struct _ioctl_reply *) msg);
    if (sd == NULL)
	return;
    if (sd->driveno == msg->index)			    /* sd already belongs to drive */
	return;

    if (sd->state > sd_stale)
	set_sd_state(sd->sdno, sd_stale, setstate_force);   /* make the subdisk stale */
    else
	sd->state = sd_empty;
    if (sd->plexno >= 0)				    /* part of a plex, */
	update_plex_state(sd->plexno);			    /* update its state */

    /* Return the space on the old drive */
    if ((sd->driveno >= 0)				    /* we have a drive, */
    &&(sd->sectors > 0))				    /* and some space on it */
	return_drive_space(sd->driveno,			    /* return the space */
	    sd->driveoffset,
	    sd->sectors);

    /* Reassign the old subdisk */
    sd->driveno = msg->index;
    sd->driveoffset = -1;				    /* let the drive decide where to put us */
    give_sd_to_drive(sd->sdno);
    reply->error = 0;
}

/* Local Variables: */
/* fill-column: 50 */
/* End: */
