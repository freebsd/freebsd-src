/*
 * Written by Julian Elischer (julian@dialix.oz.au)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * Ported to run under 386BSD by Julian Elischer (julian@dialix.oz.au) Sept 1992
 *
 *      $Id: sd.c,v 1.55 1995/03/16 18:15:52 bde Exp $
 */

#define SPLSD splbio
#define ESUCCESS 0
#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/dkbad.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/dkstat.h>
#include <sys/disklabel.h>
#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>
#include <vm/vm.h>
#include <sys/devconf.h>
#include <sys/dkstat.h>
#include <machine/md_var.h>
#include <i386/i386/cons.h>		/* XXX */

u_int32 sdstrats, sdqueues;

#define SECSIZE 512
#define	SDOUTSTANDING	4
#define	SD_RETRIES	4
#define	MAXTRANSFER	8		/* 1 page at a time */

#define SDUNITSHIFT          3
#define SDUNIT(DEV)         SH3_UNIT(DEV)
#define SDSETUNIT(DEV, U)   SH3SETUNIT((DEV), (U))

#define MAKESDDEV(maj, unit, part)	(makedev(maj,((unit<<SDUNITSHIFT)+part)))
#define PARTITION(z)	(minor(z) & 0x07)

errval	sdgetdisklabel __P((unsigned char unit));
errval	sd_get_parms __P((int unit, int flags));
void	sdstrategy __P((struct buf *bp));

int		sd_sense_handler __P((struct scsi_xfer *));
void    sdstart __P((u_int32));

struct scsi_data {
	u_int32 flags;
#define	SDINIT		0x04	/* device has been init'd */
#define SDHAVELABEL	0x10	/* have read the label */
#define SDDOSPART	0x20	/* Have read the DOS partition table */
#define SDWRITEPROT	0x40	/* Device in readonly mode (S/W) */
	boolean wlabel;		/* label is writable */
	struct disk_parms {
		u_char  heads;	/* Number of heads */
		u_int16 cyls;	/* Number of cylinders */
		u_char  sectors;	/*dubious *//* Number of sectors/track */
		u_int16 secsiz;	/* Number of bytes/sector */
		u_int32 disksize;	/* total number sectors */
	} params;
	struct disklabel disklabel;
#ifdef NetBSD
	struct cpu_disklabel cpudisklabel;
#else
	struct dos_partition dosparts[NDOSPART];	/* DOS view of disk */
#endif /* NetBSD */
	u_int32 partflags[MAXPARTITIONS];	/* per partition flags */
#define SDOPEN	0x01
	u_int32 openparts;		/* one bit for each open partition */
	u_int32 sd_start_of_unix;	/* unix vs dos partitions */
	struct buf buf_queue;
	u_int32 xfer_block_wait;
	int dkunit;		/* disk stats unit number */
};

static int sdunit(dev_t dev) { return SDUNIT(dev); }
static dev_t sdsetunit(dev_t dev, int unit) { return SDSETUNIT(dev, unit); }

errval sd_open(dev_t dev, int flags, int fmt, struct proc *p,
struct scsi_link *sc_link);
errval sd_ioctl(dev_t dev, int cmd, caddr_t addr, int flag,
		struct proc *p, struct scsi_link *sc_link);
errval sd_close(dev_t dev, int flag, int fmt, struct proc *p,
        struct scsi_link *sc_link);
void sd_strategy(struct buf *bp, struct scsi_link *sc_link);

SCSI_DEVICE_ENTRIES(sd)

struct scsi_device sd_switch =
{
    sd_sense_handler,
    sdstart,			/* have a queue, served by this */
    NULL,			/* have no async handler */
    NULL,			/* Use default 'done' routine */
    "sd",
    0,
	{0, 0},
	0,				/* Link flags */
	sdattach,
	"Direct-Access",
	sdopen,
    sizeof(struct scsi_data),
	T_DIRECT,
	sdunit,
	sdsetunit,
	sd_open,
	sd_ioctl,
	sd_close,
	sd_strategy,
};

static struct scsi_xfer sx;

static int
sd_externalize(struct proc *p, struct kern_devconf *kdc, void *userp, 
	       size_t len)
{
	return scsi_externalize(SCSI_LINK(&sd_switch, kdc->kdc_unit),
				userp, &len);
}

static struct kern_devconf kdc_sd_template = {
	0, 0, 0,		/* filled in by dev_attach */
	"sd", 0, MDDC_SCSI,
	sd_externalize, 0, scsi_goaway, SCSI_EXTERNALLEN,
	&kdc_scbus0,		/* XXX parent */
	0,			/* parentdata */
	DC_UNKNOWN,		/* not supported */
};

static inline void
sd_registerdev(int unit)
{
	struct kern_devconf *kdc;

	MALLOC(kdc, struct kern_devconf *, sizeof *kdc, M_TEMP, M_NOWAIT);
	if(!kdc) return;
	*kdc = kdc_sd_template;
	kdc->kdc_unit = unit;
	kdc->kdc_description = sd_switch.desc;
	dev_attach(kdc);
	if(dk_ndrive < DK_NDRIVE) {
		sprintf(dk_names[dk_ndrive], "sd%d", unit);
		dk_wpms[dk_ndrive] = (8*1024*1024/2);
		SCSI_DATA(&sd_switch, unit)->dkunit = dk_ndrive++;
	} else {
		SCSI_DATA(&sd_switch, unit)->dkunit = -1;
	}
}


/*
 * The routine called by the low level scsi routine when it discovers
 * a device suitable for this driver.
 */
errval 
sdattach(struct scsi_link *sc_link)
{
	u_int32 unit;
	struct disk_parms *dp;

	struct scsi_data *sd = sc_link->sd;

	unit = sc_link->dev_unit;

	dp = &(sd->params);

	if (sc_link->opennings > SDOUTSTANDING) 
		sc_link->opennings = SDOUTSTANDING;
	/*
	 * Use the subdriver to request information regarding
	 * the drive. We cannot use interrupts yet, so the
	 * request must specify this.
	 */
	sd_get_parms(unit, SCSI_NOSLEEP | SCSI_NOMASK);
	/*
	 * if we don't have actual parameters, assume 512 bytes/sec
	 * (could happen on removable media - MOD)
	 * -- this avoids the division below from falling over
	 */
	if(dp->secsiz == 0) dp->secsiz = 512;
	printf("%ldMB (%ld %d byte sectors)",
	    dp->disksize / ((1024L * 1024L) / dp->secsiz),
	    dp->disksize,
	    dp->secsiz);

	if ( (sc_link->flags & SDEV_BOOTVERBOSE) )
	{
		printf("\n");
		sc_print_addr(sc_link);
		printf("with %d cyls, %d heads, and an average %d sectors/track",
	   	dp->cyls, dp->heads, dp->sectors);
	}

	sd->flags |= SDINIT;
	sd_registerdev(unit);

	return 0;
}

/*
 * open the device. Make sure the partition info is a up-to-date as can be.
 */
errval
sd_open(dev_t dev, int flags, int fmt, struct proc *p,
struct scsi_link *sc_link)
{
	errval  errcode = 0;
	u_int32 unit, part;
	struct scsi_data *sd;

	unit = SDUNIT(dev);
	part = PARTITION(dev);
	sd = sc_link->sd;

	/*
	 * Make sure the disk has been initialised
	 * At some point in the future, get the scsi driver
	 * to look for a new device if we are not initted
	 */
	if ((!sd) || (!(sd->flags & SDINIT))) {
		return (ENXIO);
	}

	SC_DEBUG(sc_link, SDEV_DB1,
	    ("sdopen: dev=0x%x (unit %d, partition %d)\n",
		dev, unit, part));

	/*
	 * "unit attention" errors should occur here if the 
	 * drive has been restarted or the pack changed.
	 * just ingnore the result, it's a decoy instruction
	 * The error code will act on the error though
	 * and invalidate any media information we had.
	 */
	scsi_test_unit_ready(sc_link, 0);

	/*
	 * If it's been invalidated, then forget the label
	 */
	sc_link->flags |= SDEV_OPEN;	/* unit attn becomes an err now */
	if (!(sc_link->flags & SDEV_MEDIA_LOADED)) {
		sd->flags &= ~SDHAVELABEL;

		/*
		 * If somebody still has it open, then forbid re-entry.
		 */
		if (sd->openparts) {
			errcode = ENXIO;
			goto bad;
		}
	}
	/*
	 * In case it is a funny one, tell it to start
	 * not needed for  most hard drives (ignore failure)
	 */
	scsi_start_unit(sc_link, SCSI_ERR_OK | SCSI_SILENT);

	/*
	 * Check that it is still responding and ok.
	 */
	if (scsi_test_unit_ready(sc_link, 0)) {
		SC_DEBUG(sc_link, SDEV_DB3, ("device not reponding\n"));
		errcode = ENXIO;
		goto bad;
	}
	SC_DEBUG(sc_link, SDEV_DB3, ("device ok\n"));

	/*
	 * Load the physical device parameters 
	 */
	sd_get_parms(unit, 0);	/* sets SDEV_MEDIA_LOADED */
	if (sd->params.secsiz != SECSIZE) {	/* XXX One day... */
		printf("sd%ld: Can't deal with %d bytes logical blocks\n",
		    unit, sd->params.secsiz);
		Debugger("sd");
		errcode = ENXIO;
		goto bad;
	}
	SC_DEBUG(sc_link, SDEV_DB3, ("Params loaded "));

	/* Lock the pack in. */
	scsi_prevent(sc_link, PR_PREVENT, SCSI_ERR_OK | SCSI_SILENT);

	/*
	 * Load the partition info if not already loaded.
	 */
	if ((errcode = sdgetdisklabel(unit)) && (part != RAWPART)) {
		goto bad;
	}
	SC_DEBUG(sc_link, SDEV_DB3, ("Disklabel loaded "));
	/*
	 * Check the partition is legal
	 */
	if (part >= MAXPARTITIONS) {
		errcode = ENXIO;
		goto bad;
	}
	SC_DEBUG(sc_link, SDEV_DB3, ("partition ok"));

	/*
	 *  Check that the partition exists
	 */
	if ((sd->disklabel.d_partitions[part].p_size == 0)
	    && (part != RAWPART)) {
		errcode = ENXIO;
		goto bad;
	}
	sd->partflags[part] |= SDOPEN;
	sd->openparts |= (1 << part);
	SC_DEBUG(sc_link, SDEV_DB3, ("open %d %d\n", sdstrats, sdqueues));

	return 0;

bad:
	if (!(sd->openparts)) {
		scsi_prevent(sc_link, PR_ALLOW, SCSI_ERR_OK | SCSI_SILENT);
		sc_link->flags &= ~SDEV_OPEN;
	}
	return errcode;
}

/*
 * close the device.. only called if we are the LAST occurence of an open
 * device.  Convenient now but usually a pain.
 */
errval 
sd_close(dev_t dev, int flag, int fmt, struct proc *p,
        struct scsi_link *sc_link)
{
	unsigned char unit, part;
	struct scsi_data *sd;

	unit = SDUNIT(dev);
	part = PARTITION(dev);
	sd = sc_link->sd;
	sd->partflags[part] &= ~SDOPEN;
	sd->openparts &= ~(1 << part);
	scsi_prevent(sc_link, PR_ALLOW, SCSI_SILENT | SCSI_ERR_OK);
	if (!(sd->openparts))
		sc_link->flags &= ~SDEV_OPEN;
	return 0;
}

/*
 * Actually translate the requested transfer into one the physical driver
 * can understand.  The transfer is described by a buf and will include
 * only one physical transfer.
 */
void
sd_strategy(struct buf *bp, struct scsi_link *sc_link)
{
	struct buf *dp;
	u_int32 opri;
	struct scsi_data *sd;
	u_int32 unit;

	sdstrats++;
	unit = SDUNIT((bp->b_dev));
	sd = sc_link->sd;
	/*
	 * If the device has been made invalid, error out
	 */
	if (!(sc_link->flags & SDEV_MEDIA_LOADED)) {
		sd->flags &= ~SDHAVELABEL;
		bp->b_error = EIO;
		goto bad;
	}
	/*
	 * "soft" write protect check
	 */
	if ((sd->flags & SDWRITEPROT) && (bp->b_flags & B_READ) == 0) {
		bp->b_error = EROFS;
		goto bad;
	}
	/*
	 * If it's a null transfer, return immediatly
	 */
	if (bp->b_bcount == 0) {
		goto done;
	}
	/*
	 * Odd number of bytes
	 */
	if (bp->b_bcount % DEV_BSIZE != 0) {
		bp->b_error = EINVAL;
		goto bad;
	}
	/*
	 * Decide which unit and partition we are talking about
	 * only raw is ok if no label
	 */
	if (PARTITION(bp->b_dev) != RAWPART) {
		if (!(sd->flags & SDHAVELABEL)) {
			bp->b_error = EIO;
			goto bad;
		}
		/*
		 * do bounds checking, adjust transfer. if error, process.
		 * if end of partition, just return
		 */
		if (bounds_check_with_label(bp, &sd->disklabel, sd->wlabel) <= 0)
			goto done;
		/* otherwise, process transfer request */
	} else {
		bp->b_pblkno = bp->b_blkno;
		bp->b_resid = 0;
	}
	opri = SPLSD();
	dp = &sd->buf_queue;

	/*      
	 * Use a bounce buffer if necessary
	 */      
#ifdef BOUNCE_BUFFERS
	if (sc_link->flags & SDEV_BOUNCE)
		vm_bounce_alloc(bp);
#endif

	/*
	 * Place it in the queue of disk activities for this disk
	 */
	disksort(dp, bp);

	/*
	 * Tell the device to get going on the transfer if it's
	 * not doing anything, otherwise just wait for completion
	 */
	sdstart(unit);

	splx(opri);
	return /*0*/;
bad:
	bp->b_flags |= B_ERROR;
done:

	/*
	 * Correctly set the buf to indicate a completed xfer
	 */
	bp->b_resid = bp->b_bcount;
	biodone(bp);
	return /*0*/;
}

/*
 * sdstart looks to see if there is a buf waiting for the device
 * and that the device is not already busy. If both are true,
 * It dequeues the buf and creates a scsi command to perform the
 * transfer in the buf. The transfer request will call scsi_done
 * on completion, which will in turn call this routine again
 * so that the next queued transfer is performed.
 * The bufs are queued by the strategy routine (sdstrategy)
 *
 * This routine is also called after other non-queued requests
 * have been made of the scsi driver, to ensure that the queue
 * continues to be drained.
 *
 * must be called at the correct (highish) spl level
 * sdstart() is called at SPLSD  from sdstrategy and scsi_done
 */
void 
sdstart(u_int32 unit)
{
	register struct	scsi_link *sc_link = SCSI_LINK(&sd_switch, unit);
	register struct scsi_data *sd = sc_link->sd;
	struct buf *bp = 0;
	struct buf *dp;
	struct scsi_rw_big cmd;
	u_int32 blkno, nblk;
	struct partition *p;

	SC_DEBUG(sc_link, SDEV_DB2, ("sdstart "));
	/*
	 * Check if the device has room for another command
	 */
	while (sc_link->opennings) {

		/*
		 * there is excess capacity, but a special waits 
		 * It'll need the adapter as soon as we clear out of the
		 * way and let it run (user level wait).
		 */
		if (sc_link->flags & SDEV_WAITING) {
			return;
		}
		/*
		 * See if there is a buf with work for us to do..
		 */
		dp = &sd->buf_queue;
		if ((bp = dp->b_actf) == NULL) {	/* yes, an assign */
			return;
		}
		dp->b_actf = bp->b_actf;

		/*
		 *  If the device has become invalid, abort all the
		 * reads and writes until all files have been closed and
		 * re-openned
		 */
		if (!(sc_link->flags & SDEV_MEDIA_LOADED)) {
			sd->flags &= ~SDHAVELABEL;
			goto bad;
		}
		/*
		 * We have a buf, now we know we are going to go through
		 * With this thing..
		 *
		 *  First, translate the block to absolute
		 */
		p = sd->disklabel.d_partitions + PARTITION(bp->b_dev);
		blkno = bp->b_blkno + p->p_offset;
		if (bp->b_bcount & 511) 
		{
		    goto bad;
		}
		nblk = bp->b_bcount >> 9;

		/*
		 *  Fill out the scsi command
		 */
		bzero(&cmd, sizeof(cmd));
		cmd.op_code = (bp->b_flags & B_READ)
		    ? READ_BIG : WRITE_BIG;
		cmd.addr_3 = (blkno & 0xff000000UL) >> 24;
		cmd.addr_2 = (blkno & 0xff0000) >> 16;
		cmd.addr_1 = (blkno & 0xff00) >> 8;
		cmd.addr_0 = blkno & 0xff;
		cmd.length2 = (nblk & 0xff00) >> 8;
		cmd.length1 = (nblk & 0xff);
		/*
		 * Call the routine that chats with the adapter.
		 * Note: we cannot sleep as we may be an interrupt
		 */
		if (scsi_scsi_cmd(sc_link,
			(struct scsi_generic *) &cmd,
			sizeof(cmd),
			(u_char *) bp->b_un.b_addr,
			bp->b_bcount,
			SD_RETRIES,
			10000,
			bp,
			SCSI_NOSLEEP | ((bp->b_flags & B_READ) ?
			    SCSI_DATA_IN : SCSI_DATA_OUT))
		    == SUCCESSFULLY_QUEUED) {
			sdqueues++;
			if(sd->dkunit >= 0) {
				dk_xfer[sd->dkunit]++;
				dk_seek[sd->dkunit]++; /* don't know */
				dk_wds[sd->dkunit] += bp->b_bcount >> 6;
			}
		} else {
bad:
			printf("sd%ld: oops not queued\n", unit);
			bp->b_error = EIO;
			bp->b_flags |= B_ERROR;
			biodone(bp);
		}
	}
}

/*
 * Perform special action on behalf of the user
 * Knows about the internals of this device
 */
errval 
sd_ioctl(dev_t dev, int cmd, caddr_t addr, int flag,
struct proc *p, struct scsi_link *sc_link)
{
	/* struct sd_cmd_buf *args; */
	errval  error = 0;
	unsigned char unit, part;
	register struct scsi_data *sd;

	/*
	 * Find the device that the user is talking about
	 */
	unit = SDUNIT(dev);
	part = PARTITION(dev);
	sd = sc_link->sd;
	SC_DEBUG(sc_link, SDEV_DB1, ("sdioctl (0x%x)", cmd));

	/*
	 * If the device is not valid.. abandon ship
	 */
	if (!(sc_link->flags & SDEV_MEDIA_LOADED))
		return (EIO);
	switch (cmd) {

	case DIOCSBAD:
		error = EINVAL;
		break;

	case DIOCGDINFO:
		*(struct disklabel *) addr = sd->disklabel;
		break;

	case DIOCGPART:
		((struct partinfo *) addr)->disklab = &sd->disklabel;
		((struct partinfo *) addr)->part =
		    &sd->disklabel.d_partitions[PARTITION(dev)];
		break;

	case DIOCSDINFO:
		if ((flag & FWRITE) == 0)
			error = EBADF;
		else
			error = setdisklabel(&sd->disklabel,
			    (struct disklabel *)addr,
			/*(sd->flags & DKFL_BSDLABEL) ? sd->openparts : */ 0
#ifdef NetBSD
			    ,&sd->cpudisklabel
#else
#if 0
			    ,sd->dosparts
#endif
#endif
			    );
		if (error == 0) {
			sd->flags |= SDHAVELABEL;
		}
		break;

	case DIOCWLABEL:
		sd->flags &= ~SDWRITEPROT;
		if ((flag & FWRITE) == 0)
			error = EBADF;
		else
			sd->wlabel = *(boolean *) addr;
		break;

	case DIOCWDINFO:
		sd->flags &= ~SDWRITEPROT;
		if ((flag & FWRITE) == 0)
			error = EBADF;
		else {
			error = setdisklabel(&sd->disklabel,
			    (struct disklabel *)addr,
			    /*(sd->flags & SDHAVELABEL) ? sd->openparts : */ 0
#ifdef NetBSD
			    ,&sd->cpudisklabel
#else
#if 0
			    ,sd->dosparts
#endif
#endif
			    );
			if (!error) {
				boolean wlab;

				/* ok - write will succeed */
				sd->flags |= SDHAVELABEL;

				/* simulate opening partition 0 so write succeeds */
				sd->openparts |= (1 << 0);	/* XXX */
				wlab = sd->wlabel;
				sd->wlabel = 1;
				error = writedisklabel(dev, sdstrategy,
				    &sd->disklabel
#ifdef NetBSD
				    ,&sd->cpudisklabel
#else
#if 0
				    ,sd->dosparts
#endif
#endif
				    );
				sd->wlabel = wlab;
			}
		} 
		break;

	default:
		if (part == RAWPART || SCSI_SUPER(dev) )
			error = scsi_do_ioctl(dev, cmd, addr, flag, p, sc_link);
		else
			error = ENOTTY;
		break;
	}
	return error;
}

/*
 * Load the label information on the named device
 */
errval 
sdgetdisklabel(unsigned char unit)
{
	char   *errstring;
	struct scsi_data *sd = SCSI_DATA(&sd_switch, unit);
	dev_t dev;

	dev = makedev(0, (unit << SDUNITSHIFT) + RAWPART);
	/*
	 * If the inflo is already loaded, use it
	 */
	if (sd->flags & SDHAVELABEL)
		return (ESUCCESS);

	bzero(&sd->disklabel, sizeof(struct disklabel));
	/*
	 * make raw partition the whole disk in case of failure then get pdinfo
	 * for historical reasons, make part a same as raw part
	 */
	sd->disklabel.d_partitions[0].p_offset = 0;
	sd->disklabel.d_partitions[0].p_size = sd->params.disksize;
	sd->disklabel.d_partitions[RAWPART].p_offset = 0;
	sd->disklabel.d_partitions[RAWPART].p_size = sd->params.disksize;
	sd->disklabel.d_secperunit= sd->params.disksize;
	sd->disklabel.d_npartitions = MAXPARTITIONS;
	sd->disklabel.d_secsize = SECSIZE;	/* as long as it's not 0 */
	sd->disklabel.d_ntracks = sd->params.heads;
	sd->disklabel.d_nsectors = sd->params.sectors;
	sd->disklabel.d_ncylinders = sd->params.cyls;
	sd->disklabel.d_secpercyl = sd->params.heads * sd->params.sectors;
	if (sd->disklabel.d_secpercyl == 0) {
		sd->disklabel.d_secpercyl = 100;
		/* as long as it's not 0 - readdisklabel divides by it (?) */
	}
	/*
	 * Call the generic disklabel extraction routine
	 */
	sd->flags |= SDHAVELABEL;	/* chicken and egg problem */
					/* we need to pretend this disklabel */
					/* is real before we can read */
					/* real disklabel */
	errstring = readdisklabel(makedev(0, (unit << SDUNITSHIFT) + RAWPART),
	    sdstrategy,
	    &sd->disklabel
#ifdef NetBSD
	    ,&sd->cpu_disklabel,
#else
	    ,sd->dosparts, 0
#endif
	    ); 
	if (errstring) {
		sd->flags &= ~SDHAVELABEL;	/* not now we don't */
		printf("sd%d: %s\n", unit, errstring);
		return ENXIO;
	}
	sd->disklabel.d_partitions[RAWPART].p_offset = 0;
	sd->disklabel.d_partitions[RAWPART].p_size = sd->params.disksize;
	return ESUCCESS;
}

/*
 * Find out from the device what it's capacity is
 */
u_int32 
sd_size(unit, flags)
	int	unit, flags;
{
	struct scsi_read_cap_data rdcap;
	struct scsi_read_capacity scsi_cmd;
	u_int32 size;
	struct scsi_link *sc_link = SCSI_LINK(&sd_switch, unit);

	/*
	 * make up a scsi command and ask the scsi driver to do
	 * it for you.
	 */
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = READ_CAPACITY;

	/*
	 * If the command works, interpret the result as a 4 byte
	 * number of blocks
	 */
	if (scsi_scsi_cmd(sc_link,
		(struct scsi_generic *) &scsi_cmd,
		sizeof(scsi_cmd),
		(u_char *) & rdcap,
		sizeof(rdcap),
		SD_RETRIES,
		2000,
		NULL,
		flags | SCSI_DATA_IN) != 0) {
		printf("sd%d: could not get size\n", unit);
		return (0);
	} else {
		size = rdcap.addr_0 + 1;
		size += rdcap.addr_1 << 8;
		size += rdcap.addr_2 << 16;
		size += rdcap.addr_3 << 24;
	}
	return (size);
}

/*
 * Tell the device to map out a defective block
 */
errval 
sd_reassign_blocks(unit, block)
	int	unit, block;
{
	struct scsi_reassign_blocks scsi_cmd;
	struct scsi_reassign_blocks_data rbdata;
	struct scsi_link *sc_link = SCSI_LINK(&sd_switch, unit);

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	bzero(&rbdata, sizeof(rbdata));
	scsi_cmd.op_code = REASSIGN_BLOCKS;

	rbdata.length_msb = 0;
	rbdata.length_lsb = sizeof(rbdata.defect_descriptor[0]);
	rbdata.defect_descriptor[0].dlbaddr_3 = ((block >> 24) & 0xff);
	rbdata.defect_descriptor[0].dlbaddr_2 = ((block >> 16) & 0xff);
	rbdata.defect_descriptor[0].dlbaddr_1 = ((block >> 8) & 0xff);
	rbdata.defect_descriptor[0].dlbaddr_0 = ((block) & 0xff);

	return (scsi_scsi_cmd(sc_link,
		(struct scsi_generic *) &scsi_cmd,
		sizeof(scsi_cmd),
		(u_char *) & rbdata,
		sizeof(rbdata),
		SD_RETRIES,
		5000,
		NULL,
		SCSI_DATA_OUT));
}
#define b2tol(a)	(((unsigned)(a##_1) << 8) + (unsigned)a##_0 )

/*
 * Get the scsi driver to send a full inquiry to the
 * device and use the results to fill out the disk 
 * parameter structure.
 */
errval 
sd_get_parms(unit, flags)
	int	unit, flags;
{
	struct scsi_link *sc_link = SCSI_LINK(&sd_switch, unit);
	struct scsi_data *sd = sc_link->sd;
	struct disk_parms *disk_parms = &sd->params;
	struct scsi_mode_sense scsi_cmd;
	struct scsi_mode_sense_data {
		struct scsi_mode_header header;
		struct blk_desc blk_desc;
		union disk_pages pages;
	} scsi_sense;
	u_int32 sectors;

	/*
	 * First check if we have it all loaded
	 */
	if (sc_link->flags & SDEV_MEDIA_LOADED)
		return 0;

	/*
	 * do a "mode sense page 4"
	 */
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = MODE_SENSE;
	scsi_cmd.page = 4;
	scsi_cmd.length = 0x20;
	/*
	 * If the command worked, use the results to fill out
	 * the parameter structure
	 */
	if (scsi_scsi_cmd(sc_link,
		(struct scsi_generic *) &scsi_cmd,
		sizeof(scsi_cmd),
		(u_char *) & scsi_sense,
		sizeof(scsi_sense),
		SD_RETRIES,
		4000,
		NULL,
		flags | SCSI_DATA_IN) != 0) {

		printf("sd%d could not mode sense (4).", unit);
		printf(" Using ficticious geometry\n");
		/*
		 * use adaptec standard ficticious geometry
		 * this depends on which controller (e.g. 1542C is
		 * different. but we have to put SOMETHING here..)
		 */
		sectors = sd_size(unit, flags);
		disk_parms->heads = 64;
		disk_parms->sectors = 32;
		disk_parms->cyls = sectors / (64 * 32);
		disk_parms->secsiz = SECSIZE;
		disk_parms->disksize = sectors;
	} else {

		SC_DEBUG(sc_link, SDEV_DB3,
		    ("%d cyls, %d heads, %d precomp, %d red_write, %d land_zone\n",
			scsi_3btou(&scsi_sense.pages.rigid_geometry.ncyl_2),
			scsi_sense.pages.rigid_geometry.nheads,
			b2tol(scsi_sense.pages.rigid_geometry.st_cyl_wp),
			b2tol(scsi_sense.pages.rigid_geometry.st_cyl_rwc),
			b2tol(scsi_sense.pages.rigid_geometry.land_zone)));

		/*
		 * KLUDGE!!(for zone recorded disks)
		 * give a number of sectors so that sec * trks * cyls
		 * is <= disk_size 
		 * can lead to wasted space! THINK ABOUT THIS !
		 */
		disk_parms->heads = scsi_sense.pages.rigid_geometry.nheads;
		disk_parms->cyls = scsi_3btou(&scsi_sense.pages.rigid_geometry.ncyl_2);
		disk_parms->secsiz = scsi_3btou(scsi_sense.blk_desc.blklen);

		sectors = sd_size(unit, flags);
		disk_parms->disksize = sectors;
		/* Check if none of these values are zero */
		if(disk_parms->heads && disk_parms->cyls) {
			sectors /= (disk_parms->heads * disk_parms->cyls);
		}
		else {
			/* set it to something reasonable */
			disk_parms->heads = 64;
			disk_parms->cyls = sectors / (64 * 32);
			sectors = 32;
		}
		/* keep secsiz sane too - we may divide by it later */
		if(disk_parms->secsiz == 0)
			disk_parms->secsiz = SECSIZE;
		disk_parms->sectors = sectors;	/* dubious on SCSI *//*XXX */
	}
	sc_link->flags |= SDEV_MEDIA_LOADED;
	return 0;
}

int
sdsize(dev_t dev)
{
	u_int32 unit = SDUNIT(dev), part = PARTITION(dev), val;
	struct scsi_data *sd;

	if ( (sd = SCSI_DATA(&sd_switch, unit)) == 0)
		return -1;

	if ((sd->flags & SDINIT) == 0)
		return -1;
	if (sd == 0 || (sd->flags & SDHAVELABEL) == 0) {
		val = sdopen(MAKESDDEV(major(dev), unit, RAWPART), FREAD, S_IFBLK, 0);
		if (val != 0)
			return -1;
	}
	if (sd->flags & SDWRITEPROT)
		return -1;

	return (int)sd->disklabel.d_partitions[part].p_size;
}

/*
 * sense handler: Called to determine what to do when the
 * device returns a CHECK CONDITION.
 *
 * This will issue a retry when the device returns a
 * non-media hardware failure.  The CDC-WREN IV does this
 * when you access it during thermal calibrarion, so the drive
 * is pretty useless without this.  
 *
 * In general, you probably almost always would like to issue a retry
 * for your disk I/O.  It can't hurt too much (the caller only retries
 * so many times) and it may save your butt.
 */

int sd_sense_handler(struct scsi_xfer *xs)
{
	struct scsi_sense_data *sense;
	struct scsi_inquiry_data *inqbuf;

	sense = &(xs->sense);

	/* I don't know what the heck to do with a deferred error,
	 * so I'll just kick it back to the caller.
	 */
	if ((sense->error_code & SSD_ERRCODE) == 0x71)
		return SCSIRET_CONTINUE;

	inqbuf = &(xs->sc_link->inqbuf);

	/* It is dangerous to retry on removable drives without
	 * looking carefully at the additional sense code
	 * and sense code qualifier and ensuring the disk hasn't changed:
	 */
	if (inqbuf->dev_qual2 & SID_REMOVABLE)
		return SCSIRET_CONTINUE;

	/* Retry all disk errors.
	 */
	scsi_sense_print(xs);
	if (xs->retries)
		printf(", retries:%d\n", xs->retries);
	else
		printf(", FAILURE\n");

	return SCSIRET_DO_RETRY;
}

/*
 * dump all of physical memory into the partition specified, starting
 * at offset 'dumplo' into the partition.
 */
errval
sddump(dev_t dev)
{				/* dump core after a system crash */
	register struct scsi_data *sd;	/* disk unit to do the IO */
	struct scsi_link *sc_link;
	int32	num;		/* number of sectors to write */
	u_int32	unit, part;
	int32	blkoff, blknum, blkcnt = MAXTRANSFER;
	int32	nblocks;
	char	*addr;
	struct	scsi_rw_big cmd;
	static	int sddoingadump = 0;
	struct	scsi_xfer *xs = &sx;
	errval	retval;
	int	c;

	addr = (char *) 0;	/* starting address */

	/* toss any characters present prior to dump */
	while (cncheckc())
		;

	/* size of memory to dump */
	num = Maxmem;
	unit = SDUNIT(dev);	/* eventually support floppies? */
	part = PARTITION(dev);	/* file system */

	sc_link = SCSI_LINK(&sd_switch, unit);

	if (!sc_link)
		return ENXIO;

	sd = sc_link->sd;

	/* was it ever initialized etc. ? */
	if (!(sd->flags & SDINIT))
		return (ENXIO);
	if (sc_link->flags & SDEV_MEDIA_LOADED != SDEV_MEDIA_LOADED)
		return (ENXIO);
	if (sd->flags & SDWRITEPROT)
		return (ENXIO);

	/* Convert to disk sectors */
	num = (u_int32) num * NBPG / sd->disklabel.d_secsize;

	/* check if controller active */
	if (sddoingadump)
		return (EFAULT);

	nblocks = sd->disklabel.d_partitions[part].p_size;
	blkoff = sd->disklabel.d_partitions[part].p_offset;

	/* check transfer bounds against partition size */
	if ((dumplo < 0) || ((dumplo + num) > nblocks))
		return (EINVAL);

	sddoingadump = 1;

	blknum = dumplo + blkoff;
	while (num > 0) {
                *(int *)CMAP1 =		/* XXX use pmap_enter() */
			PG_V | PG_KW | trunc_page(addr);
                pmap_update();
		/*
		 *  Fill out the scsi command
		 */
		bzero(&cmd, sizeof(cmd));
		cmd.op_code = WRITE_BIG;
		cmd.addr_3 = (blknum & 0xff000000) >> 24;
		cmd.addr_2 = (blknum & 0xff0000) >> 16;
		cmd.addr_1 = (blknum & 0xff00) >> 8;
		cmd.addr_0 = blknum & 0xff;
		cmd.length2 = (blkcnt & 0xff00) >> 8;
		cmd.length1 = (blkcnt & 0xff);
		/*
		 * Fill out the scsi_xfer structure
		 *    Note: we cannot sleep as we may be an interrupt
		 * don't use scsi_scsi_cmd() as it may want 
		 * to wait for an xs.
		 */
		bzero(xs, sizeof(sx));
		xs->flags |= SCSI_NOMASK | SCSI_NOSLEEP | INUSE | SCSI_DATA_OUT;
		xs->sc_link = sc_link;
		xs->retries = SD_RETRIES;
		xs->timeout = 10000;	/* 10000 millisecs for a disk ! */
		xs->cmd = (struct scsi_generic *) &cmd;
		xs->cmdlen = sizeof(cmd);
		xs->resid = blkcnt * 512;
		xs->error = XS_NOERROR;
		xs->bp = 0;
		xs->data = (u_char *) CADDR1;	/* XXX use pmap_enter() */
		xs->datalen = blkcnt * 512;

		/*
		 * Pass all this info to the scsi driver.
		 */
		retval = (*(sc_link->adapter->scsi_cmd)) (xs);
		switch (retval) {
		case SUCCESSFULLY_QUEUED:
		case HAD_ERROR:
			return (ENXIO);		/* we said not to sleep! */
		case COMPLETE:
			break;
		default:
			return (ENXIO);		/* we said not to sleep! */
		}

		if ((unsigned) addr % (1024 * 1024) == 0)
			printf("%ld ", num / 2048);
		/* update block count */
		num -= blkcnt;
		blknum += blkcnt;
		(int) addr += 512 * blkcnt;

		/* operator aborting dump? */
		if (cncheckc())
			return (EINTR);
	}
	return (0);
}
