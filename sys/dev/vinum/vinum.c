/*-
 * Copyright (c) 1997, 1998
 *	Nan Yang Computer Services Limited.  All rights reserved.
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
 * $Id: vinum.c,v 1.34 2001/05/22 04:07:22 grog Exp grog $
 * $FreeBSD$
 */

#define STATIC static					    /* nothing while we're testing XXX */

#include <dev/vinum/vinumhdr.h>
#include <sys/sysproto.h>				    /* for sync(2) */
#include <sys/devicestat.h>
#ifdef VINUMDEBUG
#include <sys/reboot.h>
int debug = 0;
extern int total_malloced;
extern int malloccount;
extern struct mc malloced[];
#endif
#include <dev/vinum/request.h>

struct cdevsw vinum_cdevsw =
{
    vinumopen, vinumclose, physread, physwrite,
    vinumioctl, seltrue, nommap, vinumstrategy,
    "vinum", VINUM_CDEV_MAJOR, vinumdump, vinumsize,
    D_DISK
};

/* Called by main() during pseudo-device attachment. */
STATIC void vinumattach(void *);

STATIC int vinum_modevent(module_t mod, modeventtype_t type, void *unused);

struct _vinum_conf vinum_conf;				    /* configuration information */

dev_t vinum_daemon_dev;
dev_t vinum_super_dev;

/*
 * Called by main() during pseudo-device attachment.  All we need
 * to do is allocate enough space for devices to be configured later, and
 * add devsw entries.
 */
void
vinumattach(void *dummy)
{
    /* modload should prevent multiple loads, so this is worth a panic */
    if ((vinum_conf.flags & VF_LOADED) != 0)
	panic("vinum: already loaded");

    log(LOG_INFO, "vinum: loaded\n");
#ifdef VINUMDEBUG
    vinum_conf.flags |= VF_LOADED | VF_HASDEBUG;	    /* we're loaded now, and we support debug */
#else
    vinum_conf.flags |= VF_LOADED;			    /* we're loaded now */
#endif

    daemonq = NULL;					    /* initialize daemon's work queue */
    dqend = NULL;

    vinum_daemon_dev = make_dev(&vinum_cdevsw,
	VINUM_DAEMON_DEV,
	UID_ROOT,
	GID_WHEEL,
	S_IRUSR | S_IWUSR,
	"vinum/controld");
    vinum_super_dev = make_dev(&vinum_cdevsw,
	VINUM_SUPERDEV,
	UID_ROOT,
	GID_WHEEL,
	S_IRUSR | S_IWUSR,
	"vinum/control");

    vinum_conf.version = VINUMVERSION;			    /* note what version we are */

    /* allocate space: drives... */
    DRIVE = (struct drive *) Malloc(sizeof(struct drive) * INITIAL_DRIVES);
    CHECKALLOC(DRIVE, "vinum: no memory\n");
    bzero(DRIVE, sizeof(struct drive) * INITIAL_DRIVES);
    vinum_conf.drives_allocated = INITIAL_DRIVES;	    /* number of drive slots allocated */
    vinum_conf.drives_used = 0;				    /* and number in use */

    /* volumes, ... */
    VOL = (struct volume *) Malloc(sizeof(struct volume) * INITIAL_VOLUMES);
    CHECKALLOC(VOL, "vinum: no memory\n");
    bzero(VOL, sizeof(struct volume) * INITIAL_VOLUMES);
    vinum_conf.volumes_allocated = INITIAL_VOLUMES;	    /* number of volume slots allocated */
    vinum_conf.volumes_used = 0;			    /* and number in use */

    /* plexes, ... */
    PLEX = (struct plex *) Malloc(sizeof(struct plex) * INITIAL_PLEXES);
    CHECKALLOC(PLEX, "vinum: no memory\n");
    bzero(PLEX, sizeof(struct plex) * INITIAL_PLEXES);
    vinum_conf.plexes_allocated = INITIAL_PLEXES;	    /* number of plex slots allocated */
    vinum_conf.plexes_used = 0;				    /* and number in use */

    /* and subdisks */
    SD = (struct sd *) Malloc(sizeof(struct sd) * INITIAL_SUBDISKS);
    CHECKALLOC(SD, "vinum: no memory\n");
    bzero(SD, sizeof(struct sd) * INITIAL_SUBDISKS);
    vinum_conf.subdisks_allocated = INITIAL_SUBDISKS;	    /* number of sd slots allocated */
    vinum_conf.subdisks_used = 0;			    /* and number in use */
}

/*
 * Check if we have anything open.  If confopen is != 0,
 * that goes for the super device as well, otherwise
 * only for volumes.
 *
 * Return 0 if not inactive, 1 if inactive.
 */
int
vinum_inactive(int confopen)
{
    int i;
    int can_do = 1;					    /* assume we can do it */

    if (confopen && (vinum_conf.flags & VF_OPEN))	    /* open by vinum(8)? */
	return 0;					    /* can't do it while we're open */
    lock_config();
    for (i = 0; i < vinum_conf.volumes_allocated; i++) {
	if ((VOL[i].state > volume_down)
	    && (VOL[i].flags & VF_OPEN)) {		    /* volume is open */
	    can_do = 0;
	    break;
	}
    }
    unlock_config();
    return can_do;
}

/*
 * Free all structures.
 * If cleardrive is 0, save the configuration; otherwise
 * remove the configuration from the drive.
 *
 * Before coming here, ensure that no volumes are open.
 */
void
free_vinum(int cleardrive)
{
    int i;
    int drives_allocated = vinum_conf.drives_allocated;

    if (DRIVE != NULL) {
	if (cleardrive) {				    /* remove the vinum config */
	    for (i = 0; i < drives_allocated; i++)
		remove_drive(i);			    /* remove the drive */
	} else {					    /* keep the config */
	    for (i = 0; i < drives_allocated; i++)
		free_drive(&DRIVE[i]);			    /* close files and things */
	}
	Free(DRIVE);
    }
    while ((vinum_conf.flags & (VF_STOPPING | VF_DAEMONOPEN))
	== (VF_STOPPING | VF_DAEMONOPEN)) {		    /* at least one daemon open, we're stopping */
	queue_daemon_request(daemonrq_return, (union daemoninfo) 0); /* stop the daemon */
	tsleep(&vinumclose, PUSER, "vstop", 1);		    /* and wait for it */
    }
    if (SD != NULL) {
	for (i = 0; i < vinum_conf.subdisks_allocated; i++) {
	    struct sd *sd = &SD[i];

	    if (sd->state != sd_unallocated)
		free_sd(i);
	}
	Free(SD);
    }
    if (PLEX != NULL) {
	for (i = 0; i < vinum_conf.plexes_allocated; i++) {
	    struct plex *plex = &PLEX[i];

	    if (plex->state != plex_unallocated)	    /* we have real data there */
		free_plex(i);
	}
	Free(PLEX);
    }
    if (VOL != NULL) {
	for (i = 0; i < vinum_conf.volumes_allocated; i++) {
	    struct volume *volume = &VOL[i];

	    if (volume->state != volume_unallocated)
		free_volume(i);
	}
	Free(VOL);
    }
    bzero(&vinum_conf, sizeof(vinum_conf));
}

STATIC int
vinum_modevent(module_t mod, modeventtype_t type, void *unused)
{
    struct sync_args dummyarg =
    {0};

    switch (type) {
    case MOD_LOAD:
	vinumattach(NULL);
	return 0;					    /* OK */
    case MOD_UNLOAD:
	if (!vinum_inactive(1))				    /* is anything open? */
	    return EBUSY;				    /* yes, we can't do it */
	vinum_conf.flags |= VF_STOPPING;		    /* note that we want to stop */
	sync(curthread, &dummyarg);			    /* write out buffers */
	free_vinum(0);					    /* clean up */
#ifdef VINUMDEBUG
	if (total_malloced) {
	    int i;
#ifdef INVARIANTS
	    int *poke;
#endif

	    for (i = 0; i < malloccount; i++) {
		if (debug & DEBUG_WARNINGS)		    /* want to hear about them */
		    log(LOG_WARNING,
			"vinum: exiting with %d bytes malloced from %s:%d\n",
			malloced[i].size,
			malloced[i].file,
			malloced[i].line);
#ifdef INVARIANTS
		poke = &((int *) malloced[i].address)
		    [malloced[i].size / (2 * sizeof(int))]; /* middle of the area */
		if (*poke == 0xdeadc0de)		    /* already freed */
		    log(LOG_ERR,
			"vinum: exiting with malloc table inconsistency at %p from %s:%d\n",
			malloced[i].address,
			malloced[i].file,
			malloced[i].line);
#endif
		Free(malloced[i].address);
	    }
	}
#endif
	destroy_dev(vinum_daemon_dev);			    /* daemon device */
	destroy_dev(vinum_super_dev);
	log(LOG_INFO, "vinum: unloaded\n");		    /* tell the world */
	return 0;
    default:
	break;
    }
    return 0;
}

static moduledata_t vinum_mod =
{
    "vinum",
    (modeventhand_t) vinum_modevent,
    0
};
DECLARE_MODULE(vinum, vinum_mod, SI_SUB_VINUM, SI_ORDER_MIDDLE);

/* ARGSUSED */
/* Open a vinum object */
int
vinumopen(dev_t dev,
    int flags,
    int fmt,
    struct thread *td)
{
    int error;
    unsigned int index;
    struct volume *vol;
    struct plex *plex;
    struct sd *sd;
    int devminor;					    /* minor number */

    devminor = minor(dev);
    error = 0;
    /* First, decide what we're looking at */
    switch (DEVTYPE(dev)) {
    case VINUM_VOLUME_TYPE:
	index = Volno(dev);
	if (index >= vinum_conf.volumes_allocated)
	    return ENXIO;				    /* no such device */
	vol = &VOL[index];

	switch (vol->state) {
	case volume_unallocated:
	case volume_uninit:
	    return ENXIO;

	case volume_up:
	    vol->flags |= VF_OPEN;			    /* note we're open */
	    return 0;

	case volume_down:
	    return EIO;

	default:
	    return EINVAL;
	}

    case VINUM_PLEX_TYPE:
	if (Volno(dev) >= vinum_conf.volumes_allocated)
	    return ENXIO;
	/* FALLTHROUGH */

    case VINUM_RAWPLEX_TYPE:
	index = Plexno(dev);				    /* get plex index in vinum_conf */
	if (index >= vinum_conf.plexes_allocated)
	    return ENXIO;				    /* no such device */
	plex = &PLEX[index];

	switch (plex->state) {
	case plex_referenced:
	case plex_unallocated:
	    return EINVAL;

	default:
	    plex->flags |= VF_OPEN;			    /* note we're open */
	    return 0;
	}

    case VINUM_SD_TYPE:
	if ((Volno(dev) >= vinum_conf.volumes_allocated)    /* no such volume */
	||(Plexno(dev) >= vinum_conf.plexes_allocated))	    /* or no such plex */
	    return ENXIO;				    /* no such device */

	/* FALLTHROUGH */

    case VINUM_RAWSD_TYPE:
	index = Sdno(dev);				    /* get the subdisk number */
	if ((index >= vinum_conf.subdisks_allocated)	    /* not a valid SD entry */
	||(SD[index].state < sd_init))			    /* or SD is not real */
	    return ENXIO;				    /* no such device */
	sd = &SD[index];

	/*
	 * Opening a subdisk is always a special operation, so we
	 * ignore the state as long as it represents a real subdisk
	 */
	switch (sd->state) {
	case sd_unallocated:
	case sd_uninit:
	    return EINVAL;

	default:
	    sd->flags |= VF_OPEN;			    /* note we're open */
	    return 0;
	}

    case VINUM_SUPERDEV_TYPE:
	error = suser_td(td);				    /* are we root? */
	if (error == 0) {				    /* yes, can do */
	    if (devminor == VINUM_DAEMON_DEV)		    /* daemon device */
		vinum_conf.flags |= VF_DAEMONOPEN;	    /* we're open */
	    else if (devminor == VINUM_SUPERDEV)
		vinum_conf.flags |= VF_OPEN;		    /* we're open */
	    else
		error = ENODEV;				    /* nothing, maybe a debug mismatch */
	}
	return error;

	/* Vinum drives are disks.  We already have a disk
	 * driver, so don't handle them here */
    case VINUM_DRIVE_TYPE:
    default:
	return ENODEV;					    /* don't know what to do with these */
    }
}

/* ARGSUSED */
int
vinumclose(dev_t dev,
    int flags,
    int fmt,
    struct thread *td)
{
    unsigned int index;
    struct volume *vol;
    int devminor;

    devminor = minor(dev);
    index = Volno(dev);
    /* First, decide what we're looking at */
    switch (DEVTYPE(dev)) {
    case VINUM_VOLUME_TYPE:
	if (index >= vinum_conf.volumes_allocated)
	    return ENXIO;				    /* no such device */
	vol = &VOL[index];

	switch (vol->state) {
	case volume_unallocated:
	case volume_uninit:
	    return ENXIO;

	case volume_up:
	    vol->flags &= ~VF_OPEN;			    /* reset our flags */
	    return 0;

	case volume_down:
	    return EIO;

	default:
	    return EINVAL;
	}

    case VINUM_PLEX_TYPE:
	if (Volno(dev) >= vinum_conf.volumes_allocated)
	    return ENXIO;
	/* FALLTHROUGH */

    case VINUM_RAWPLEX_TYPE:
	index = Plexno(dev);				    /* get plex index in vinum_conf */
	if (index >= vinum_conf.plexes_allocated)
	    return ENXIO;				    /* no such device */
	PLEX[index].flags &= ~VF_OPEN;			    /* reset our flags */
	return 0;

    case VINUM_SD_TYPE:
	if ((Volno(dev) >= vinum_conf.volumes_allocated) || /* no such volume */
	    (Plexno(dev) >= vinum_conf.plexes_allocated))   /* or no such plex */
	    return ENXIO;				    /* no such device */
	/* FALLTHROUGH */

    case VINUM_RAWSD_TYPE:
	index = Sdno(dev);				    /* get the subdisk number */
	if (index >= vinum_conf.subdisks_allocated)
	    return ENXIO;				    /* no such device */
	SD[index].flags &= ~VF_OPEN;			    /* reset our flags */
	return 0;

    case VINUM_SUPERDEV_TYPE:
	/*
	 * don't worry about whether we're root:
	 * nobody else would get this far.
	 */
	if (devminor == VINUM_SUPERDEV)			    /* normal superdev */
	    vinum_conf.flags &= ~VF_OPEN;		    /* no longer open */
	else if (devminor == VINUM_DAEMON_DEV) {	    /* the daemon device */
	    vinum_conf.flags &= ~VF_DAEMONOPEN;		    /* no longer open */
	    if (vinum_conf.flags & VF_STOPPING)		    /* we're stopping, */
		wakeup(&vinumclose);			    /* we can continue stopping now */
	}
	return 0;

    case VINUM_DRIVE_TYPE:
    default:
	return ENODEV;					    /* don't know what to do with these */
    }
}

/* size routine */
int
vinumsize(dev_t dev)
{
    struct volume *vol;
    int size;

    vol = &VOL[Volno(dev)];

    if (vol->state == volume_up)
	size = vol->size;
    else
	return 0;					    /* err on the size of conservatism */

    return size;
}

int
vinumdump(dev_t dev)
{
    /* Not implemented. */
    return ENXIO;
}

/* Local Variables: */
/* fill-column: 50 */
/* End: */
