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
 * $Id: vinum.c,v 1.44 2003/05/23 00:50:55 grog Exp grog $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define STATIC static					    /* nothing while we're testing */

#include <dev/vinum/vinumhdr.h>
#include <sys/sysproto.h>				    /* for sync(2) */
#ifdef VINUMDEBUG
#include <sys/reboot.h>
int debug = 0;						    /* debug flags */
extern int total_malloced;
extern int malloccount;
extern struct mc malloced[];
#endif
#include <dev/vinum/request.h>

struct cdevsw vinum_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	vinumopen,
	.d_close =	vinumclose,
	.d_read =	physread,
	.d_write =	physwrite,
	.d_ioctl =	vinumioctl,
	.d_strategy =	vinumstrategy,
	.d_name =	"vinum",
	.d_flags =	D_DISK | D_NEEDGIANT
};

/* Called by main() during pseudo-device attachment. */
void vinumattach(void *);
STATIC int vinum_modevent(module_t mod, modeventtype_t type, void *unused);
STATIC void vinum_clone(void *arg, char *name, int namelen, struct cdev ** dev);

struct _vinum_conf vinum_conf;				    /* configuration information */

struct cdev *vinum_daemon_dev;
struct cdev *vinum_super_dev;

static eventhandler_tag dev_clone_tag;

/*
 * Mutexes for plex synchronization.  Ideally each plex
 * should have its own mutex, but the fact that the plex
 * struct can move makes that very complicated.  Instead,
 * have plexes use share these mutexes based on modulo plex
 * number.
 */
struct mtx plexmutex[PLEXMUTEXES];

/*
 * Called by main() during pseudo-device attachment.  All we need
 * to do is allocate enough space for devices to be configured later, and
 * add devsw entries.
 */
void
vinumattach(void *dummy)
{
    char *envp;
    int i;
#define MUTEXNAMELEN 16
    char mutexname[MUTEXNAMELEN];
#if PLEXMUTEXES > 10000
#error Increase size of MUTEXNAMELEN
#endif
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
	VINUM_DAEMON_MINOR,
	UID_ROOT,
	GID_WHEEL,
	S_IRUSR | S_IWUSR,
	"vinum/controld");
    vinum_super_dev = make_dev(&vinum_cdevsw,
	VINUM_SUPERDEV_MINOR,
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

    for (i = 0; i < PLEXMUTEXES; i++) {
	snprintf(mutexname, MUTEXNAMELEN, "vinumplex%d", i);
	mtx_init(&plexmutex[i], mutexname, "plex", MTX_DEF);
    }

    /* and subdisks */
    SD = (struct sd *) Malloc(sizeof(struct sd) * INITIAL_SUBDISKS);
    CHECKALLOC(SD, "vinum: no memory\n");
    bzero(SD, sizeof(struct sd) * INITIAL_SUBDISKS);
    vinum_conf.subdisks_allocated = INITIAL_SUBDISKS;	    /* number of sd slots allocated */
    vinum_conf.subdisks_used = 0;			    /* and number in use */
    dev_clone_tag = EVENTHANDLER_REGISTER(dev_clone, vinum_clone, 0, 1000);

    /*
     * See if the loader has passed us any of the autostart
     * options.
     */
    envp = NULL;
    if ((envp = getenv("vinum.autostart")) != NULL) {	    /* start all drives now */
	vinum_scandisk(NULL);
	freeenv(envp);
    } else if ((envp = getenv("vinum.drives")) != NULL) {
	vinum_scandisk(envp);
	freeenv(envp);
    }
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

    while ((vinum_conf.flags & (VF_STOPPING | VF_DAEMONOPEN))
	== (VF_STOPPING | VF_DAEMONOPEN)) {		    /* at least one daemon open, we're stopping */
	queue_daemon_request(daemonrq_return, (union daemoninfo) 0); /* stop the daemon */
	tsleep(&vinumclose, PUSER, "vstop", 1);		    /* and wait for it */
    }
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
    vinum_conf.version = VINUMVERSION;			    /* reinstate version number */
}

STATIC int
vinum_modevent(module_t mod, modeventtype_t type, void *unused)
{
    struct sync_args dummyarg =
    {0};
    int i;

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
	for (i = 0; i < PLEXMUTEXES; i++)
	    mtx_destroy(&plexmutex[i]);
	log(LOG_INFO, "vinum: unloaded\n");		    /* tell the world */
	EVENTHANDLER_DEREGISTER(dev_clone, dev_clone_tag);
	return 0;
    default:
	return EOPNOTSUPP;
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
DECLARE_MODULE(vinum, vinum_mod, SI_SUB_RAID, SI_ORDER_MIDDLE);

/* ARGSUSED */
/* Open a vinum object */
int
vinumopen(struct cdev *dev,
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
	/*
	 * The super device and daemon device are the last two
	 * volume numbers, so check for them first.
	 */
	if ((devminor == VINUM_DAEMON_MINOR)		    /* daemon device */
	||(devminor == VINUM_SUPERDEV_MINOR)) {		    /* or normal super device */
	    error = suser(td);				    /* are we root? */

	    if (error == 0) {				    /* yes, can do */
		if (devminor == VINUM_DAEMON_MINOR)	    /* daemon device */
		    vinum_conf.flags |= VF_DAEMONOPEN;	    /* we're open */
		else					    /* superdev */
		    vinum_conf.flags |= VF_OPEN;	    /* we're open */
	    }
	    return error;
	}
	/* Must be a real volume.  Check. */
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
	index = Plexno(dev);				    /* get plex index in vinum_conf */
	if (index >= vinum_conf.plexes_allocated)
	    return ENXIO;				    /* no such device */
	plex = &PLEX[index];

	switch (plex->state) {
	case plex_unallocated:
	    return ENXIO;

	case plex_referenced:
	    return EINVAL;

	default:
	    plex->flags |= VF_OPEN;			    /* note we're open */
	    return 0;
	}

    case VINUM_SD_TYPE:
    case VINUM_SD2_TYPE:
	index = Sdno(dev);				    /* get the subdisk number */
	if (index >= vinum_conf.subdisks_allocated)	    /* not a valid SD entry */
	    return ENXIO;				    /* no such device */
	sd = &SD[index];

	/*
	 * Opening a subdisk is always a special operation, so
	 * we ignore the state as long as it represents a real
	 * subdisk.
	 */
	switch (sd->state) {
	case sd_unallocated:
	    return ENXIO;

	case sd_uninit:
	case sd_referenced:
	    return EINVAL;

	default:
	    sd->flags |= VF_OPEN;			    /* note we're open */
	    return 0;
	}
    }
    return 0;						    /* to keep the compiler happy */
}

/* ARGSUSED */
int
vinumclose(struct cdev *dev,
    int flags,
    int fmt,
    struct thread *td)
{
    unsigned int index;
    struct volume *vol;
    int devminor;

    devminor = minor(dev);
    /* First, decide what we're looking at */
    switch (DEVTYPE(dev)) {
    case VINUM_VOLUME_TYPE:
	/*
	 * The super device and daemon device are the last two
	 * volume numbers, so check for them first.
	 */
	if ((devminor == VINUM_DAEMON_MINOR)		    /* daemon device */
	||(devminor == VINUM_SUPERDEV_MINOR)) {		    /* or normal super device */
	    /*
	     * don't worry about whether we're root:
	     * nobody else would get this far.
	     */
	    if (devminor == VINUM_SUPERDEV_MINOR)	    /* normal superdev */
		vinum_conf.flags &= ~VF_OPEN;		    /* no longer open */
	    else {					    /* the daemon device */
		vinum_conf.flags &= ~VF_DAEMONOPEN;	    /* no longer open */
		if (vinum_conf.flags & VF_STOPPING)	    /* we're trying to stop, */
		    wakeup(&vinumclose);		    /* we can continue now */
	    }
	    return 0;
	}
	/* Real volume */
	index = Volno(dev);
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
	index = Plexno (dev);
	if (index >= vinum_conf.plexes_allocated)	    /* no such plex */
	  return ENXIO;
	PLEX [index].flags &= ~VF_OPEN;			    /* no longer open */
	return 0;

    case VINUM_SD_TYPE:
	if ((Volno(dev) >= vinum_conf.volumes_allocated) || /* no such volume */
	    (Plexno(dev) >= vinum_conf.plexes_allocated))   /* or no such plex */
	    return ENXIO;				    /* no such device */
	index = Sdno (dev);
	if (index >= vinum_conf.subdisks_allocated)	    /* no such sd */
	  return ENXIO;
	SD [index].flags &= ~VF_OPEN;			    /* no longer open */
	return 0;


    default:
	return ENODEV;					    /* don't know what to do with these */
    }
}

void
vinum_clone(void *arg, char *name, int namelen, struct cdev ** dev)
{
    struct volume *vol;
    int i;

    if (*dev != NULL)
	return;
    if (strncmp(name, "vinum/", sizeof("vinum/") - 1) != 0)
	return;

    name += sizeof("vinum/") - 1;
    if ((i = find_volume(name, 0)) == -1)
	return;

    vol = &VOL[i];
    *dev = vol->dev;
}


/* Local Variables: */
/* fill-column: 60 */
/* End: */
