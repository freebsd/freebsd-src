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
 * $Id: vinum.c,v 1.19 1998/08/13 05:24:02 grog Exp grog $
 */

#define STATIC						    /* nothing while we're testing XXX */

#define REALLYKERNEL
#include "vinumhdr.h"
#include "sys/sysproto.h"				    /* for sync(2) */
#ifdef DEBUG
#include <sys/reboot.h>
int debug = 0;
#endif

/* pointer to ioctl p parameter, to save passing it around */
struct proc *myproc;

#if __FreeBSD__ < 3
STATIC struct cdevsw vinum_cdevsw;
STATIC struct bdevsw vinum_bdevsw =
{
    vinumopen, vinumclose, vinumstrategy, vinumioctl,
    vinumdump, vinumsize, 0,
    "vinum", &vinum_cdevsw, -1
};
#else /* goodbye, bdevsw */
STATIC struct cdevsw vinum_cdevsw =
{
    vinumopen, vinumclose, vinumread, vinumwrite,
    vinumioctl, nostop, nullreset, nodevtotty,
    seltrue, nommap, vinumstrategy, "vinum",
    NULL, -1, vinumdump, vinumsize,
    D_DISK, 0, -1
};
#endif

/* Called by main() during pseudo-device attachment. */
STATIC void vinumattach(void *);

STATIC void vinumgetdisklabel(dev_t);
void vinum_scandisk(void);
int vinum_inactive(void);
void free_vinum(int);

#if __FreeBSD__ >= 3
/* Why aren't these declared anywhere? XXX */
int setjmp(jmp_buf);
void longjmp(jmp_buf, int);
#endif

extern jmp_buf command_fail;				    /* return here if config fails */

struct _vinum_conf vinum_conf;				    /* configuration information */

STATIC int vinum_devsw_installed = 0;

/*
 * Called by main() during pseudo-device attachment.  All we need
 * to do is allocate enough space for devices to be configured later, and
 * add devsw entries.
 */
void
vinumattach(void *dummy)
{
    BROKEN_GDB;
    char *buf;						    /* pointer to temporary buffer */
    struct _ioctl_reply *ioctl_reply;			    /* struct to return */
    struct uio uio;
    struct iovec iovec;

    /* modload should prevent multiple loads, so this is worth a panic */
    if ((vinum_conf.flags & VF_LOADED) != NULL)
	panic("vinum: already loaded");

    printf("vinum: loaded\n");
    vinum_conf.flags |= VF_LOADED;			    /* we're loaded now */

    /* We don't have a p pointer here, so take it from curproc */
    myproc = curproc;
#if __FreeBSD__ < 3
    bdevsw_add_generic(BDEV_MAJOR, CDEV_MAJOR, &vinum_bdevsw);
#else
    cdevsw_add_generic(BDEV_MAJOR, CDEV_MAJOR, &vinum_cdevsw);
#endif
#ifdef DEVFS
#error DEVFS not finished yet
#endif

    uio.uio_iov = &iovec;
    uio.uio_iovcnt = 1;					    /* just one buffer */
    uio.uio_offset = 0;					    /* start at the beginning */
    uio.uio_resid = 512;				    /* one sector */
    uio.uio_segflg = UIO_SYSSPACE;			    /* we're in system space */
    uio.uio_rw = UIO_READ;				    /* do we need this? */
    uio.uio_procp = curproc;				    /* do it for our own process */

    iovec.iov_len = 512;
    buf = (char *) Malloc(iovec.iov_len);		    /* get a buffer */
    CHECKALLOC(buf, "vinum: no memory\n");		    /* can't get 512 bytes? */
    iovec.iov_base = buf;				    /* read into buf */

    /* allocate space: drives... */
    DRIVE = (struct drive *) Malloc(sizeof(struct drive) * INITIAL_DRIVES);
    CHECKALLOC(DRIVE, "vinum: no memory\n");
    vinum_conf.drives_allocated = INITIAL_DRIVES;	    /* number of drive slots allocated */
    vinum_conf.drives_used = 0;				    /* and number in use */

    /* volumes, ... */
    VOL = (struct volume *) Malloc(sizeof(struct volume) * INITIAL_VOLUMES);
    CHECKALLOC(VOL, "vinum: no memory\n");
    vinum_conf.volumes_allocated = INITIAL_VOLUMES;	    /* number of volume slots allocated */
    vinum_conf.volumes_used = 0;			    /* and number in use */

    /* plexes, ... */
    PLEX = (struct plex *) Malloc(sizeof(struct plex) * INITIAL_PLEXES);
    CHECKALLOC(PLEX, "vinum: no memory\n");
    vinum_conf.plexes_allocated = INITIAL_PLEXES;	    /* number of plex slots allocated */
    vinum_conf.plexes_used = 0;				    /* and number in use */

    /* and subdisks */
    SD = (struct sd *) Malloc(sizeof(struct sd) * INITIAL_SUBDISKS);
    CHECKALLOC(SD, "vinum: no memory\n");
    vinum_conf.subdisks_allocated = INITIAL_SUBDISKS;	    /* number of sd slots allocated */
    vinum_conf.subdisks_used = 0;			    /* and number in use */

    ioctl_reply = NULL;					    /* no reply on longjmp */
}


#ifdef ACTUALLY_LKM_NOT_KERNEL				    /* stuff for LKMs */

/* Check if we have anything open.  If so, return 0 (not inactive),
 * otherwise 1 (inactive) */
int 
vinum_inactive(void)
{
    BROKEN_GDB;
    int i;
    int can_do = 1;					    /* assume we can do it */

    lock_config();
    for (i = 0; i < vinum_conf.volumes_used; i++) {
	if (VOL[i].pid != NULL) {			    /* volume is open */
	    can_do = 0;
	    break;
	}
    }
    unlock_config();
    return can_do;
}

/* Free all structures.
 * If cleardrive is 0, save the configuration; otherwise
 * remove the configuration from the drive.
 *
 * Before coming here, ensure that no volumes are open.
 */
void 
free_vinum(int cleardrive)
{
    BROKEN_GDB;
    int i;

    if (cleardrive) {
	for (i = 0; i < vinum_conf.drives_used; i++)
	    remove_drive(i);				    /* remove the drive */
    } else {						    /* keep the config */
	save_config();
	if (DRIVE != NULL) {
	    for (i = 0; i < vinum_conf.drives_used; i++)
		free_drive(&DRIVE[i]);			    /* close files and things */
	    Free(DRIVE);
	}
    }
    if (SD != NULL)
	Free(SD);
    if (PLEX != NULL) {
	for (i = 0; i < vinum_conf.plexes_used; i++) {
	    struct plex *plex = &vinum_conf.plex[i];

	    if (plex->state != plex_unallocated) {	    /* we have real data there */
		if (plex->sdnos)
		    Free(plex->sdnos);
		if (plex->unmapped_regions)
		    Free(plex->unmapped_region);
		if (plex->defective_regions)
		    Free(plex->defective_region);
	    }
	}
	Free(PLEX);
    }
    if (VOL != NULL)
	Free(VOL);
    bzero(&vinum_conf, sizeof(vinum_conf));
}

MOD_MISC(vinum);

/*
 * Function called when loading the driver.
 */
STATIC int 
vinum_load(struct lkm_table *lkmtp, int cmd)
{
    BROKEN_GDB;
/*   Debugger ("vinum_load"); */
    vinumattach(NULL);
    return 0;						    /* OK */
}

/*
 * Function called when unloading the driver.
 */
STATIC int 
vinum_unload(struct lkm_table *lkmtp, int cmd)
{
    BROKEN_GDB;
    if (vinum_inactive()) {				    /* is anything open? */
	struct sync_args dummyarg =
	{0};
#if __FreeBSD__ < 3
	int retval;
#endif

	printf("vinum: unloaded\n");
#if __FreeBSD__ < 3
	sync(curproc, &dummyarg, &retval);		    /* write out buffers */
#else
	sync(curproc, &dummyarg);			    /* write out buffers */
#endif
	free_vinum(0);					    /* no: clean up */
#if __FreeBSD__ < 3
	bdevsw[BDEV_MAJOR] = NULL;			    /* clear bdevsw */
#endif
	cdevsw[CDEV_MAJOR] = NULL;			    /* and cdevsw */
	return 0;
    } else
	return EBUSY;
}

/*
 * Dispatcher function for the module (load/unload/stat).
 */
int 
vinum_mod(struct lkm_table *lkmtp, int cmd, int ver)
{
    BROKEN_GDB;
    MOD_DISPATCH(vinum,					    /* module name */
	lkmtp,						    /* LKM table */
	cmd,						    /* command */
	ver,
	vinum_load,					    /* load with this function */
	vinum_unload,					    /* and unload with this */
	lkm_nullcmd);
}

#else /* not LKM */
#error "This driver must be compiled as a loadable kernel module"
#endif /* LKM */

/* ARGSUSED */
/* Open a vinum object
 * At the moment, we only open volumes and the
 * super device.  It's a nice concept to be
 * able to open drives, subdisks and plexes, but
 * I can't think what good it could be */
int 
vinumopen(dev_t dev,
    int flags,
    int fmt,
    struct proc *p)
{
    BROKEN_GDB;
    int s;						    /* spl */
    int error;
    unsigned int index;
    struct volume *vol;
    struct plex *plex;
    struct sd *sd;
    struct devcode *device;

    device = (struct devcode *) &dev;

    error = 0;
    /* First, decide what we're looking at */
    switch (device->type) {
    case VINUM_VOLUME_TYPE:
	index = VOLNO(dev);
	if (index >= vinum_conf.volumes_used)
	    return ENXIO;				    /* no such device */
	vol = &VOL[index];

	switch (vol->state) {
	case volume_unallocated:
	case volume_uninit:
	    return ENXIO;

	case volume_up:
	    s = splhigh();				    /* quick lock */
	    if (error)
		return error;
	    if (vol->opencount == 0)
		vol->openflags = flags;			    /* set our flags */
	    vol->opencount++;
	    vol->pid = p->p_pid;			    /* and say who we are (do we need this? XXX) */
	    splx(s);
	    return 0;

	case volume_down:
	    return EIO;

	default:
	    return EINVAL;
	}

    case VINUM_PLEX_TYPE:
	if (VOLNO(dev) >= vinum_conf.volumes_used)
	    return ENXIO;
	index = PLEXNO(dev);				    /* get plex index in vinum_conf */
	if (index >= vinum_conf.plexes_used)
	    return ENXIO;				    /* no such device */
	plex = &PLEX[index];

	switch (plex->state) {
	case plex_unallocated:
	    return EINVAL;

	default:
	    s = splhigh();
	    if (plex->pid				    /* it's open already */
		&& (plex->pid != p->p_pid)) {		    /* and not by us, */
		splx(s);
		return EBUSY;				    /* one at a time, please */
	    }
	    plex->pid = p->p_pid;			    /* and say who we are (do we need this? XXX) */
	    splx(s);
	    return 0;
	}

    case VINUM_SD_TYPE:
	if ((VOLNO(dev) >= vinum_conf.volumes_used) ||	    /* no such volume */
	    (PLEXNO(dev) >= vinum_conf.plexes_used))	    /* or no such plex */
	    return ENXIO;				    /* no such device */
	index = SDNO(dev);				    /* get the subdisk number */
	if (index >= vinum_conf.subdisks_used)
	    return ENXIO;				    /* no such device */
	sd = &SD[index];

	/* Opening a subdisk is always a special operation, so we 
	 * ignore the state as long as it represents a real subdisk */
	switch (sd->state) {
	case sd_unallocated:
	case sd_uninit:
	    return EINVAL;

	default:
	    s = splhigh();
	    if (sd->pid					    /* it's open already */
		&& (sd->pid != p->p_pid)) {		    /* and not by us, */
		splx(s);
		return EBUSY;				    /* one at a time, please */
	    }
	    sd->pid = p->p_pid;				    /* and say who we are (do we need this? XXX) */
	    splx(s);
	    return 0;
	}

    case VINUM_DRIVE_TYPE:
    default:
	return ENODEV;					    /* don't know what to do with these */

    case VINUM_SUPERDEV_TYPE:
	if (p->p_ucred->cr_uid == 0) {			    /* root calling, */
	    vinum_conf.opencount++;			    /* one more opener */
	    return 0;					    /* no worries opening super dev */
	} else
	    return EPERM;				    /* you can't do that! */
    }
}

/* ARGSUSED */
int 
vinumclose(dev_t dev,
    int flags,
    int fmt,
    struct proc *p)
{
    BROKEN_GDB;
    unsigned int index;
    struct volume *vol;
    struct plex *plex;
    struct sd *sd;
    struct devcode *device = (struct devcode *) &dev;

    index = VOLNO(dev);
    /* First, decide what we're looking at */
    switch (device->type) {
    case VINUM_VOLUME_TYPE:
	if (index >= vinum_conf.volumes_used)
	    return ENXIO;				    /* no such device */
	vol = &VOL[index];

	switch (vol->state) {
	case volume_unallocated:
	case volume_uninit:
	    return ENXIO;

	case volume_up:
	    vol->opencount = 0;				    /* reset our flags */
	    vol->pid = NULL;				    /* and forget who owned us */
	    return 0;

	case volume_down:
	    return EIO;

	default:
	    return EINVAL;
	}

    case VINUM_PLEX_TYPE:
	if (VOLNO(dev) >= vinum_conf.volumes_used)
	    return ENXIO;
	index = PLEXNO(dev);				    /* get plex index in vinum_conf */
	if (index >= vinum_conf.plexes_used)
	    return ENXIO;				    /* no such device */
	plex = &PLEX[index];
	plex->pid = 0;
	return 0;

    case VINUM_SD_TYPE:
	if ((VOLNO(dev) >= vinum_conf.volumes_used) ||	    /* no such volume */
	    (PLEXNO(dev) >= vinum_conf.plexes_used))	    /* or no such plex */
	    return ENXIO;				    /* no such device */
	index = SDNO(dev);				    /* get the subdisk number */
	if (index >= vinum_conf.subdisks_used)
	    return ENXIO;				    /* no such device */
	sd = &SD[index];
	sd->pid = 0;
	return 0;

    case VINUM_SUPERDEV_TYPE:
	if (p->p_ucred->cr_uid == 0)			    /* root calling, */
	    vinum_conf.opencount--;			    /* one less opener */
	return 0;					    /* no worries closing super dev */

    case VINUM_DRIVE_TYPE:
    default:
	return ENODEV;					    /* don't know what to do with these */
    }
}

/* size routine */
int 
vinumsize(dev_t dev)
{
    BROKEN_GDB;
    struct volume *vol;
    int size;

/* XXX This is bogus.  We don't need to open
 * a device to find its size */
    vol = &VOL[VOLNO(dev)];

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
