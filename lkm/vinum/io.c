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
 * $Id: io.c,v 1.16 1998/08/10 23:47:21 grog Exp grog $
 */

#define STATIC						    /* nothing while we're testing XXX */

#if __FreeBSD__ < 3					    /* this is in sys/disklabel.h in 3.0 and on */
#define DTYPE_VINUM		12			    /* vinum volume */
#endif

#define REALLYKERNEL
#include "vinumhdr.h"
#include <miscfs/specfs/specdev.h>

extern jmp_buf command_fail;				    /* return on a failed command */
struct _ioctl_reply *ioctl_reply;			    /* data pointer, for returning error messages */

#if __FreeBSD__ >= 3
/* Why aren't these declared anywhere? XXX */
int setjmp(jmp_buf);
void longjmp(jmp_buf, int);
#endif

/* pointer to ioctl p parameter, to save passing it around */
extern struct proc *myproc;

/* Open the device associated with the drive, and set drive's vp */
int
open_drive(struct drive *drive, struct proc *p)
{
    BROKEN_GDB;
    struct nameidata nd;
    struct vattr va;
    int error;

    if (drive->devicename[0] == '\0')			    /* no device name */
	sprintf(drive->devicename, "/dev/%s", drive->label.name); /* get it from the drive name */
    NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, drive->devicename, p);
    error = vn_open(&nd, FREAD | FWRITE, 0);		    /* open the device */
    if (error != 0) {					    /* can't open? */
	set_drive_state(drive->driveno, drive_down, 1);
	drive->lasterror = error;
	printf("vinum open_drive %s: failed with error %d\n", drive->devicename, error); /* XXX */
	return error;
    }
    drive->vp = nd.ni_vp;
    drive->p = p;

    if (drive->vp->v_usecount > 1) {			    /* already in use? */
#if __FreeBSD__ == 2					    /* pre-4.4BSD Lite/2 parameters */
	VOP_UNLOCK(drive->vp);
#else
	VOP_UNLOCK(drive->vp, 0, p);
#endif
	close_drive(drive);
	set_drive_state(drive->driveno, drive_down, 1);
	drive->lasterror = EBUSY;
	printf("vinum open_drive %s: Drive in use\n", drive->devicename); /* XXX */
	return EBUSY;
    }
    error = VOP_GETATTR(drive->vp, &va, NOCRED, p);
    if (error) {
#if __FreeBSD__ == 2					    /* pre-4.4BSD Lite/2 parameters */
	VOP_UNLOCK(drive->vp);
#else
	VOP_UNLOCK(drive->vp, 0, p);
#endif
	close_drive(drive);
	set_drive_state(drive->driveno, drive_down, 1);
	drive->lasterror = error;
	printf("vinum open_drive %s: GETAATTR returns error %d\n", drive->devicename, error); /* XXX */
	return error;
    }
    drive->dev = va.va_rdev;				    /* device */

    if (va.va_type != VBLK) {				    /* only consider block devices */
#if __FreeBSD__ == 2					    /* pre-4.4BSD Lite/2 parameters */
	VOP_UNLOCK(drive->vp);
#else
	VOP_UNLOCK(drive->vp, 0, p);
#endif
	close_drive(drive);
	set_drive_state(drive->driveno, drive_down, 1);	    /* this also closes the drive */
	drive->lasterror = ENOTBLK;
	printf("vinum open_drive %s: Not a block device\n", drive->devicename);	/* XXX */
	return ENOTBLK;
    }
    drive->vp->v_numoutput = 0;
#if __FreeBSD__ == 2					    /* pre-4.4BSD Lite/2 parameters */
    VOP_UNLOCK(drive->vp);
#else
    VOP_UNLOCK(drive->vp, 0, p);
#endif
    return 0;
}

/* Set some variables in the drive struct
 * in more convenient form.  Return error indication */
int 
set_drive_parms(struct drive *drive)
{
    drive->blocksize = BLKDEV_IOSIZE;			    /* XXX do we need this? */
    drive->secsperblock = drive->blocksize		    /* number of sectors per block */
	/ drive->partinfo.disklab->d_secsize;

    /* Now update the label part */
    bcopy(hostname, drive->label.sysname, VINUMHOSTNAMELEN); /* put in host name */
#if __FreeBSD__ >= 3
    getmicrotime(&drive->label.date_of_birth);		    /* and current time */
#else
    drive->label.date_of_birth = time;			    /* and current time */
#endif
    drive->label.drive_size = ((u_int64_t) drive->partinfo.part->p_size) /* size of the drive in bytes */
    *((u_int64_t) drive->partinfo.disklab->d_secsize);

    /* number of sectors available for subdisks */
    drive->sectors_available = drive->label.drive_size / DEV_BSIZE - DATASTART;

    /* XXX Bug in 3.0 as of January 1998: you can open
     * non-existent slices.  They have a length of 0 */
    if (drive->label.drive_size < MINVINUMSLICE) {	    /* too small to worry about */
	set_drive_state(drive->driveno, drive_down, 1);
	printf("vinum open_drive %s: Drive too small\n", drive->devicename); /* XXX */
	drive->lasterror = ENOSPC;
	return ENOSPC;
    }
    drive->freelist_size = INITIAL_DRIVE_FREELIST;	    /* initial number of entries */
    drive->freelist = (struct drive_freelist *)
	Malloc(INITIAL_DRIVE_FREELIST * sizeof(struct drive_freelist));
    if (drive->freelist == NULL)			    /* can't malloc, dammit */
	return ENOSPC;
    drive->freelist_entries = 1;			    /* just (almost) the complete drive */
    drive->freelist[0].offset = DATASTART;		    /* starts here */
    drive->freelist[0].sectors = (drive->label.drive_size >> DEV_BSHIFT) - DATASTART; /* and it's this long */
    set_drive_state(drive->driveno, drive_up, 1);	    /* our drive is accessible */
    return 0;
}

/* Initialize a drive: open the device and add device
 * information */
int 
init_drive(struct drive *drive)
{
    BROKEN_GDB;
    int error;

    if (drive->devicename[0] == '\0') {			    /* no device name yet, default to drive name */
	drive->lasterror = EINVAL;
	printf("vinum: Can't open drive without drive name\n");	/* XXX */
	return EINVAL;
    }
    error = open_drive(drive, myproc);			    /* open the drive */
    if (error)
	return error;

    error = VOP_IOCTL(drive->vp,			    /* get the partition information */
	DIOCGPART,
	(caddr_t) & drive->partinfo,
	FREAD,
	NOCRED,
	myproc);
    if (error) {
	printf("vinum open_drive %s: Can't get partition information, error %d\n",
	    drive->devicename,
	    error);					    /* XXX */
	close_drive(drive);
	drive->lasterror = error;
	set_drive_state(drive->driveno, drive_down, 1);
	return error;
    }
    if (drive->partinfo.part->p_fstype != 0) {		    /* not plain */
	drive->lasterror = EFTYPE;
	printf("vinum open_drive %s: Wrong partition type for vinum\n", drive->devicename); /* XXX */
	close_drive(drive);
	set_drive_state(drive->driveno, drive_down, 1);
	return EFTYPE;
    }
    return set_drive_parms(drive);			    /* set various odds and ends */
}

/* Close a drive if it's open.  No errors */
void 
close_drive(struct drive *drive)
{
    if (drive->vp) {
	vn_close(drive->vp, FREAD | FWRITE, NOCRED, drive->p);
	drive->vp = NULL;
    }
}

/* Remove drive from the configuration.
 * Caller must ensure that it isn't active
 */
void 
remove_drive(int driveno)
{
    BROKEN_GDB;
    struct drive *drive = &vinum_conf.drive[driveno];
    long long int nomagic = VINUM_NOMAGIC;		    /* no magic number */

    write_drive(drive,					    /* obliterate the magic, but leave a hint */
	(char *) &nomagic,
	8,
	VINUM_LABEL_OFFSET);
    close_drive(drive);					    /* and close it */
    drive->state = drive_unallocated;			    /* and forget everything we knew about it */
    save_config();					    /* and save the updated configuration */
}

/* Transfer drive data.  Usually called from one of these defines;

 * #define read_drive(a, b, c, d) driveio (a, b, c, d, B_READ)
 * #define write_drive(a, b, c, d) driveio (a, b, c, d, B_WRITE)
 *
 * Return error number
 */
int
driveio(struct drive *drive, void *buf, size_t length, off_t offset, int flag)
{
    BROKEN_GDB;
    int error;
    struct buf *bp;
    int spl;

    error = 0;

    /* Get a buffer */
    bp = (struct buf *) Malloc(sizeof(struct buf));	    /* get a buffer */
    CHECKALLOC(bp, "Can't allocate memory");

    bzero(&buf, sizeof(buf));
    bp->b_flags = B_BUSY | flag;			    /* tell us when it's done */
    bp->b_iodone = drive_io_done;			    /* here */
    bp->b_proc = myproc;				    /* process */
    bp->b_dev = drive->vp->v_un.vu_specinfo->si_rdev;	    /* device */
    if (offset & (drive->partinfo.disklab->d_secsize - 1))  /* not on a block boundary */
	bp->b_blkno = offset / drive->partinfo.disklab->d_secsize; /* block number */
    bp->b_data = buf;
    bp->b_vp = drive->vp;				    /* vnode */
    bp->b_bcount = length;
    bp->b_bufsize = length;

    (*bdevsw[major(bp->b_dev)]->d_strategy) (bp);	    /* initiate the transfer */

    spl = splbio();
    while ((bp->b_flags & B_DONE) == 0) {
	bp->b_flags |= B_CALL;				    /* wake me again */
	tsleep((caddr_t) bp, PRIBIO, "driveio", 0);	    /* and wait for it to complete */
    }
    splx(spl);
    if (bp->b_flags & B_ERROR)				    /* didn't work */
	error = bp->b_error;				    /* get the error return */
    Free(bp);						    /* then return the buffer */
    return error;
}

/* Read data from a drive

 * Return error number
 */
int
read_drive(struct drive *drive, void *buf, size_t length, off_t offset)
{
    BROKEN_GDB;
    int error;
    struct buf *bp;
    daddr_t nextbn;
    long bscale;

    struct uio uio;
    struct iovec iov;
    daddr_t blocknum;					    /* block number */
    int blockoff;					    /* offset in block */
    int count;						    /* amount to transfer */

    iov.iov_base = buf;
    iov.iov_len = length;

    uio.uio_iov = &iov;
    uio.uio_iovcnt = length;
    uio.uio_offset = offset;
    uio.uio_resid = length;
    uio.uio_segflg = UIO_SYSSPACE;
    uio.uio_rw = UIO_READ;
    uio.uio_procp = myproc;

    bscale = btodb(drive->blocksize);			    /* mask off offset from block number */
    do {
	blocknum = btodb(uio.uio_offset) & ~(bscale - 1);   /* get the block number */
	blockoff = uio.uio_offset % drive->blocksize;	    /* offset in block */
	count = min((unsigned) (drive->blocksize - blockoff), /* amount to transfer in this block */
	    uio.uio_resid);

	/* XXX Check this.  I think the test is wrong */
	if (drive->vp->v_lastr + bscale == blocknum) {	    /* did our last read finish in this block? */
	    nextbn = blocknum + bscale;			    /* note the end of the transfer */
	    error = breadn(drive->vp,			    /* and read with read-ahead */
		blocknum,
		(int) drive->blocksize,
		&nextbn,
		(int *) &drive->blocksize,
		1,
		NOCRED,
		&bp);
	} else						    /* random read: just read this block */
	    error = bread(drive->vp, blocknum, (int) drive->blocksize, NOCRED, &bp);
	drive->vp->v_lastr = blocknum;			    /* note the last block we read */
	count = min(count, drive->blocksize - bp->b_resid);
	if (error) {
	    brelse(bp);
	    return error;
	}
	error = uiomove((char *) bp->b_data + blockoff, count, &uio); /* move the data */
	brelse(bp);
    }
    while (error == 0 && uio.uio_resid > 0 && count != 0);
    return error;
}

/* Write data to a drive

 * Return error number
 */
int 
write_drive(struct drive *drive, void *buf, size_t length, off_t offset)
{
    BROKEN_GDB;
    int error;
    struct buf *bp;
    struct uio uio;
    struct iovec iov;
    daddr_t blocknum;					    /* block number */
    int blockoff;					    /* offset in block */
    int count;						    /* amount to transfer */
    int blockshift;

    if (drive->state == drive_down)			    /* currently down */
	return 0;					    /* ignore */
    if (drive->vp == NULL) {
	drive->lasterror = ENODEV;
	return ENODEV;					    /* not configured yet */
    }
    iov.iov_base = buf;
    iov.iov_len = length;

    uio.uio_iov = &iov;
    uio.uio_iovcnt = length;
    uio.uio_offset = offset;
    uio.uio_resid = length;
    uio.uio_segflg = UIO_SYSSPACE;
    uio.uio_rw = UIO_WRITE;
    uio.uio_procp = myproc;

    error = 0;
    blockshift = btodb(drive->blocksize) - 1;		    /* amount to shift block number
							    * to get sector number */
    do {
	blocknum = btodb(uio.uio_offset) & ~blockshift;	    /* get the block number */
	blockoff = uio.uio_offset % drive->blocksize;	    /* offset in block */
	count = min((unsigned) (drive->blocksize - blockoff), /* amount to transfer in this block */
	    uio.uio_resid);
	if (count == drive->blocksize)			    /* the whole block */
	    bp = getblk(drive->vp, blocknum, drive->blocksize, 0, 0); /* just get it */
	else						    /* partial block: */
	    error = bread(drive->vp,			    /* read it first */
		blocknum,
		drive->blocksize,
		NOCRED,
		&bp);
	count = min(count, drive->blocksize - bp->b_resid); /* how much will we transfer now? */
	if (error == 0)
	    error = uiomove((char *) bp->b_data + blockoff, /* move the data to the block */
		count,
		&uio);
	if (error) {
	    brelse(bp);
	    drive->lasterror = error;
	    switch (error) {
	    case EIO:
		set_drive_state(drive->driveno, drive_down, 1);
		break;

		/* XXX Add other possibilities here */
	    default:
	    }
	    return error;
	}
	if (count + blockoff == drive->blocksize)
	    /* The transfer goes to the end of the block.  There's
	     * no need to wait for any more data to arrive. */
	    bawrite(bp);				    /* start the write now */
	else
	    bdwrite(bp);				    /* do a delayed write */
    }
    while (error == 0 && uio.uio_resid > 0 && count != 0);
    if (error)
	drive->lasterror = error;
    return error;					    /* OK */
}

/* Wake up on completion */
void 
drive_io_done(struct buf *bp)
{
    BROKEN_GDB;
    wakeup((caddr_t) bp);				    /* Wachet auf! */
    bp->b_flags &= ~B_CALL;				    /* don't do this again */
}

/* Check a drive for a vinum header.  If found, 
 * update the drive information.  We come here
 * with a partially populated drive structure
 * which includes the device name.
 *
 * Return information on what we found
 */
enum drive_label_info 
read_drive_label(struct drive *drive)
{
    BROKEN_GDB;
    int error;
    int result;						    /* result of our search */
    struct vinum_hdr *vhdr;				    /* and as header */

    error = init_drive(drive);				    /* find the drive */
    if (error)						    /* find the drive */
	return DL_CANT_OPEN;				    /* not ours */

    vhdr = (struct vinum_hdr *) Malloc(VINUMHEADERLEN);	    /* allocate buffers */
    CHECKALLOC(vhdr, "Can't allocate memory");

    error = read_drive(drive, (void *) vhdr, VINUMHEADERLEN, VINUM_LABEL_OFFSET);
    if (vhdr->magic == VINUM_MAGIC) {			    /* ours! */
	if (drive->label.name[0]			    /* we have a name for this drive */
	&&(strcmp(drive->label.name, vhdr->label.name))) {  /* but it doesn't match the real name */
	    drive->lasterror = EINVAL;
	    result = DL_WRONG_DRIVE;			    /* it's the wrong drive */
	} else {
	    set_drive_parms(drive);			    /* and set other parameters */
	    result = DL_OURS;
	}
	/* We copy the drive anyway so that we have
	 * the correct name in the drive info.  This
	 * may not be the name specified */
	drive->label = vhdr->label;			    /* put in the label information */
    } else if (vhdr->magic == VINUM_NOMAGIC)		    /* was ours, but we gave it away */
	result = DL_DELETED_LABEL;
    else
	result = DL_NOT_OURS;				    /* we could have it, but we don't yet */
    Free(vhdr);						    /* that's all. */
    return result;
}

/* Check a drive for a vinum header.  If found, 
 * read configuration information from the drive and
 * incorporate the data into the configuration.
 *
 * Return error number
 */
int 
check_drive(char *drivename)
{
    BROKEN_GDB;
    int error;
    struct nameidata nd;				    /* mount point credentials */
    char *config_text;					    /* read the config info from disk into here */
    volatile char *cptr;				    /* pointer into config information */
    char *eptr;						    /* end pointer into config information */
    int driveno;
    struct drive *drive;
    char *config_line;					    /* copy the config line to */

    driveno = find_drive_by_dev(drivename, 1);		    /* doesn't exist, create it */
    drive = &vinum_conf.drive[driveno];			    /* and get a pointer */
    strcpy(drive->devicename, drivename);		    /* put in device name */

    if (read_drive_label(drive) == DL_OURS) {		    /* ours! */
	config_text = (char *) Malloc(MAXCONFIG * 2);	    /* allocate buffers */
	CHECKALLOC(config_text, "Can't allocate memory");
	config_line = (char *) Malloc(MAXCONFIGLINE * 2);   /* allocate buffers */
	CHECKALLOC(config_line, "Can't allocate memory");

	/* Read in both copies of the configuration information */
	error = read_drive(drive, config_text, MAXCONFIG * 2, VINUM_CONFIG_OFFSET);

	if (error != 0) {
	    printf("vinum: Can't read device %s, error %d\n", drive->devicename, error);
	    Free(config_text);
	    Free(config_line);
	    free_drive(drive);				    /* give it back */
	    return error;
	}
	/* XXX At this point, check that the two copies are the same, and do something useful if not.
	 * In particular, consider which is newer, and what this means for the integrity of the
	 * data on the drive */

	/* Parse the configuration, and add it to the global configuration */
	for (cptr = config_text; *cptr != '\0';) {	    /* love this style(9) */
	    volatile int parse_status;			    /* return value from parse_config */

	    for (eptr = config_line; (*cptr != '\n') && (*cptr != '\0');) /* until the end of the line */
		*eptr++ = *cptr++;
	    *eptr = '\0';				    /* and delimit */
	    if (setjmp(command_fail) == 0) {		    /* come back here on error and continue */
		parse_status = parse_config(config_line, &keyword_set);	/* parse the config line */
		if (parse_status < 0) {			    /* error in config */
							    /* This config should have been parsed in user
		       * space.  If we run into problems here, something
		       * serious is afoot.  Complain and let the user
		       * snarf the config to see what's wrong */
		    printf("vinum: Config error on drive %s, aborting integration\n", nd.ni_dirp);
		    Free(config_text);
		    Free(config_line);
		    free_drive(drive);			    /* give it back */
		    return EINVAL;
		}
	    }
	    while (*cptr == '\n')
		cptr++;					    /* skip to next line */
	}
	Free(config_text);
	if ((vinum_conf.flags & VF_READING_CONFIG) == 0)    /* not reading config */
	    updateconfig(0);				    /* update object states */
	printf("vinum: read configuration from %s\n", drivename);
	return 0;					    /* it all worked */
    } else {						    /* no vinum label found */
	if (drive->lasterror) {
	    set_drive_state(drive->driveno, drive_down, 1);
	    return drive->lasterror;
	} else
	    return ENODEV;				    /* not our device */
    }
}

/* Kludge: kernel printf doesn't handle longs correctly XXX */
static char *lltoa(long long l, char *s);
static char *sappend(char *txt, char *s);

static char *
lltoa(long long l, char *s)
{
    if (l < 0) {
	*s++ = '-';
	l = -l;
    }
    if (l > 9) {
	s = lltoa(l / 10, s);
	l %= 10;
    }
    *s++ = l + '0';
    return s;
}

static char *
sappend(char *txt, char *s)
{
    while (*s++ = *txt++);
    return s - 1;
}

/* Format the configuration in text form into the buffer
 * at config.  Don't go beyond len bytes
 * XXX this stinks.  Fix soon. */
void 
format_config(char *config, int len)
{
    BROKEN_GDB;
    int i;
    int j;
    char *s = config;

    bzero(config, len);

    /* First write the drive configuration */
    for (i = 0; i < vinum_conf.drives_used; i++) {
	struct drive *drive;

	drive = &vinum_conf.drive[i];
	if (drive->state != drive_unallocated) {
	    sprintf(s,
		"drive %s state %s device %s\n",
		drive->label.name,
		drive_state(drive->state),
		drive->devicename);
	    while (*s)
		s++;					    /* find the end */
	    if (s > &config[len - 80]) {
		printf("vinum: configuration data overflow\n");
		return;
	    }
	}
    }

    /* Then the volume configuration */
    for (i = 0; i < vinum_conf.volumes_used; i++) {
	struct volume *vol;

	vol = &vinum_conf.volume[i];
	if (vol->state != volume_unallocated) {
	    if (vol->preferred_plex >= 0)		    /* preferences, */
		sprintf(s,
		    "volume %s state %s readpol prefer %s",
		    vol->name,
		    volume_state(vol->state),
		    vinum_conf.plex[vol->preferred_plex].name);
	    else					    /* default round-robin */
		sprintf(s,
		    "volume %s state %s",
		    vol->name,
		    volume_state(vol->state));
	    while (*s)
		s++;					    /* find the end */
	    s = sappend("\n", s);
	    if (s > &config[len - 80]) {
		printf("vinum: configuration data overflow\n");
		return;
	    }
	}
    }

    /* Then the plex configuration */
    for (i = 0; i < vinum_conf.plexes_used; i++) {
	struct plex *plex;

	plex = &vinum_conf.plex[i];
	if (plex->state != plex_unallocated) {
	    sprintf(s, "plex name %s state %s org %s ",
		plex->name,
		plex_state(plex->state),
		plex_org(plex->organization));
	    while (*s)
		s++;					    /* find the end */
	    if ((plex->organization == plex_striped)
		) {
		sprintf(s, "%db ", (int) plex->stripesize);
		while (*s)
		    s++;				    /* find the end */
	    }
	    if (plex->volno >= 0)			    /* we have a volume */
		sprintf(s, "vol %s ", vinum_conf.volume[plex->volno].name);
	    while (*s)
		s++;					    /* find the end */
	    for (j = 0; j < plex->subdisks; j++) {
		sprintf(s, " sd %s", vinum_conf.sd[plex->sdnos[j]].name);
	    }
	    s = sappend("\n", s);
	    if (s > &config[len - 80]) {
		printf("vinum: configuration data overflow\n");
		return;
	    }
	}
    }

    /* And finally the subdisk configuration */
    for (i = 0; i < vinum_conf.subdisks_used; i++) {
	struct sd *sd = &vinum_conf.sd[i];		    /* XXX */
	if (vinum_conf.sd[i].state != sd_unallocated) {
	    sprintf(s,
		"sd name %s drive %s plex %s state %s len ",
		sd->name,
		vinum_conf.drive[sd->driveno].label.name,
		vinum_conf.plex[sd->plexno].name,
		sd_state(sd->state));
	    while (*s)
		s++;					    /* find the end */
	    s = lltoa(sd->sectors, s);
	    s = sappend("b driveoffset ", s);
	    s = lltoa(sd->driveoffset, s);
	    s = sappend("b plexoffset ", s);
	    s = lltoa(sd->plexoffset, s);
	    s = sappend("b\n", s);
	    if (s > &config[len - 80]) {
		printf("vinum: configuration data overflow\n");
		return;
	    }
	}
    }
}

/* Write the configuration to all vinum slices */
int
save_config(void)
{
    BROKEN_GDB;
    int error;
    int written_config;					    /* set when we firstnwrite the config to disk */
    int driveno;
    struct drive *drive;				    /* point to current drive info */
    struct vinum_hdr *vhdr;				    /* and as header */
    char *config;					    /* point to config data */
    int wlabel_on;					    /* to set writing label on/off */

    /* don't save the configuration while we're still working on it */
    if (vinum_conf.flags & VF_CONFIGURING)
	return 0;
    written_config = 0;					    /* no config written yet */
    /* Build a volume header */
    vhdr = (struct vinum_hdr *) Malloc(VINUMHEADERLEN);	    /* get space for the config data */
    CHECKALLOC(vhdr, "Can't allocate config data");
    vhdr->magic = VINUM_MAGIC;				    /* magic number */
    vhdr->config_length = MAXCONFIG;			    /* length of following config info */

    config = Malloc(MAXCONFIG);				    /* get space for the config data */
    CHECKALLOC(config, "Can't allocate config data");

    format_config(config, MAXCONFIG);
    error = 0;						    /* no errors yet */
    for (driveno = 0; driveno < vinum_conf.drives_used; driveno++) {
	drive = &vinum_conf.drive[driveno];		    /* point to drive */

	if (drive->state != drive_down) {
#if (__FreeBSD__ >= 3)
	    getmicrotime(&drive->label.last_update);	    /* time of last update is now */
#else
	    drive->label.last_update = time;		    /* time of last update is now */
#endif
	    bcopy((char *) &drive->label,		    /* and the label info from the drive structure */
		(char *) &vhdr->label,
		sizeof(vhdr->label));
	    if ((drive->state != drive_unallocated)
		&& (drive->state != drive_uninit)) {
		wlabel_on = 1;				    /* enable writing the label */
		error = VOP_IOCTL(drive->vp,		    /* make the label writeable */
		    DIOCWLABEL,
		    (caddr_t) & wlabel_on,
		    FWRITE,
		    NOCRED,
		    myproc);
		if (error == 0)
		    error = write_drive(drive, vhdr, VINUMHEADERLEN, VINUM_LABEL_OFFSET);
		if (error == 0)
		    error = write_drive(drive, config, MAXCONFIG, VINUM_CONFIG_OFFSET);
		wlabel_on = 0;				    /* enable writing the label */
		VOP_IOCTL(drive->vp,			    /* make the label non-writeable again */
		    DIOCWLABEL,
		    (caddr_t) & wlabel_on,
		    FWRITE,
		    NOCRED,
		    myproc);
		if (error) {
		    printf("vinum: Can't write config to %s, error %d\n", drive->devicename, error);
		    set_drive_state(drive->driveno, drive_down, 1);
		} else
		    written_config = 1;			    /* we've written it on at least one drive */
	    }
	}
    }
    Free(vhdr);
    Free(config);
    return written_config == 0;				    /* return 1 if we failed to write config */
}

/* Disk labels are a mess.  The correct way to access them
 * is with the DIOC[GSW]DINFO ioctls, but some programs, such
 * as newfs, access the disk directly, so we have to write
 * things there.  We do this only on request.  If a user
 * request tries to read it directly, we fake up one on the fly.
 */

/* get_volume_label returns a label structure to lp, which
 * is allocated by the caller */
void 
get_volume_label(struct volume *vol, struct disklabel *lp)
{
    bzero(lp, sizeof(struct disklabel));

    strncpy(lp->d_typename, "vinum", sizeof(lp->d_typename));
    lp->d_type = DTYPE_VINUM;
    strncpy(lp->d_packname, vol->name, min(sizeof(lp->d_packname), sizeof(vol->name)));
    lp->d_rpm = 14400 * vol->plexes;			    /* to keep them guessing */
    lp->d_interleave = 1;
    lp->d_flags = 0;

    /* Fitting unto the vine, a vinum has a single
     *  track with all its sectors */
    lp->d_secsize = DEV_BSIZE;				    /* bytes per sector */
    lp->d_nsectors = vol->size;				    /* data sectors per track */
    lp->d_ntracks = 1;					    /* tracks per cylinder */
    lp->d_ncylinders = 1;				    /* data cylinders per unit */
    lp->d_secpercyl = vol->size;			    /* data sectors per cylinder */
    lp->d_secperunit = vol->size;			    /* data sectors per unit */

    lp->d_bbsize = BBSIZE;
    lp->d_sbsize = SBSIZE;

    lp->d_magic = DISKMAGIC;
    lp->d_magic2 = DISKMAGIC;

    /* Set up partitions a, b and c to be identical
     * and the size of the volume.  a is UFS, b is
     * swap, c is nothing */
    lp->d_partitions[0].p_size = vol->size;
    lp->d_partitions[0].p_fsize = 1024;
    lp->d_partitions[0].p_fstype = FS_BSDFFS;		    /* FreeBSD File System :-) */
    lp->d_partitions[0].p_fsize = 1024;			    /* FS fragment size */
    lp->d_partitions[0].p_frag = 8;			    /* and fragments per block */
    lp->d_partitions[SWAP_PART].p_size = vol->size;
    lp->d_partitions[SWAP_PART].p_fstype = FS_SWAP;	    /* swap partition */
    lp->d_partitions[LABEL_PART].p_size = vol->size;
    lp->d_npartitions = LABEL_PART + 1;
    strncpy(lp->d_packname, vol->name, min(sizeof(lp->d_packname), sizeof(vol->name)));
    lp->d_checksum = dkcksum(lp);
}

int 
write_volume_label(int volno)
{
    struct disklabel *lp;
    struct buf *bp;
    struct disklabel *dlp;
    struct volume *vol;
    int error;

    lp = (struct disklabel *) Malloc((sizeof(struct disklabel) + (DEV_BSIZE - 1)) & (DEV_BSIZE - 1));
    if (lp == 0)
	return ENOMEM;

    if ((unsigned) (volno) >= (unsigned) vinum_conf.volumes_used) /* invalid volume */
	return ENOENT;

    vol = &VOL[volno];					    /* volume in question */
    if (vol->state == volume_unallocated)		    /* nothing there */
	return ENOENT;

    get_volume_label(vol, lp);				    /* get the label */

    /* Now write to disk.  This code is derived from the
     * system writedisklabel (), which does silly things
     * like reading the label and refusing to write
     * unless it's already there. */
    bp = geteblk((int) lp->d_secsize);			    /* get a buffer */
    bp->b_dev = minor(vol->devno) | (CDEV_MAJOR << MAJORDEV_SHIFT); /* our own raw volume */
    bp->b_blkno = LABELSECTOR * ((int) lp->d_secsize / DEV_BSIZE);
    bp->b_bcount = lp->d_secsize;
    bzero(bp->b_data, lp->d_secsize);
    dlp = (struct disklabel *) bp->b_data;
    *dlp = *lp;
    bp->b_flags &= ~B_INVAL;
    bp->b_flags |= B_BUSY | B_WRITE;
    vinumstrategy(bp);					    /* write it out */
    error = biowait(bp);
    bp->b_flags |= B_INVAL | B_AGE;
    brelse(bp);
    return error;
}

/* Initialize a subdisk */
int 
initsd(int sdno)
{
    return 0;
}
