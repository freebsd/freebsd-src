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
 *      $Id: sd.c,v 2.7 93/10/24 12:44:40 julian Exp Locker: julian $
 */

#define SPLSD splbio
#define ESUCCESS 0
#include <sd.h>
#include <sys/types.h>
#include <sys/param.h>
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
#include <sys/disklabel.h>
#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

u_int32 sdstrats, sdqueues;

#ifdef NetBSD
#ifdef	DDB
int     Debugger();
#else	/* DDB */
#define Debugger()
#endif	/* DDB */
#else /* NetBSD */
#include <ddb.h>
#if	NDDB > 0
int     Debugger();
#else	/* NDDB > 0 */
#define Debugger()
#endif	/* NDDB > 0 */
#endif

#define PAGESIZ 	4096
#define SECSIZE 512
#define PDLOCATION	29
#define BOOTRECORDSIGNATURE			(0x55aa & 0x00ff)
#define	SDOUTSTANDING	2
#define SDQSIZE		4
#define	SD_RETRIES	4

#define MAKESDDEV(maj, unit, part)	(makedev(maj,((unit<<3)+part)))
#define	UNITSHIFT	3
#define PARTITION(z)	(minor(z) & 0x07)
#define	RAW_PART	3
#define UNIT(z)		(  (minor(z) >> UNITSHIFT) )

#define WHOLE_DISK(unit) ( (unit << UNITSHIFT) + RAW_PART )

errval	sdgetdisklabel __P((unsigned char unit));
errval	sd_get_parms __P((int unit, int flags));
errval  sdstrategy();
void    sdstart();

struct scsi_device sd_switch =
{
    NULL,			/* Use default error handler */
    sdstart,			/* have a queue, served by this */
    NULL,			/* have no async handler */
    NULL,			/* Use default 'done' routine */
    "sd",
    0,
    { 0, 0 }
};

struct sd_data {
	u_int32 flags;
#define	SDINIT		0x04	/* device has been init'd */
#define SDHAVELABEL	0x10	/* have read the label */
#define SDDOSPART	0x20	/* Have read the DOS partition table */
#define SDWRITEPROT	0x40	/* Device in readonly mode (S/W) */
	struct scsi_link *sc_link;	/* contains our targ, lun etc. */
	u_int32 ad_info;	/* info about the adapter */
	u_int32 cmdscount;	/* cmds allowed outstanding by board */
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
}      *sd_data[NSD];

static u_int32 next_sd_unit = 0;

/*
 * The routine called by the low level scsi routine when it discovers
 * a device suitable for this driver.
 */
errval 
sdattach(sc_link)
	struct scsi_link *sc_link;
{
	u_int32 unit;
	struct sd_data *sd;
	struct disk_parms *dp;

	unit = next_sd_unit++;
	SC_DEBUG(sc_link, SDEV_DB2, ("sdattach: "));
	/*
	 * Check we have the resources for another drive
	 */
	if (unit >= NSD) {
		printf("Too many scsi disks..(%d > %d) reconfigure kernel\n",
			(unit + 1), NSD);
		return 0;
	}
	if (sd_data[unit]) {
		printf("sd%d: unit already has storage allocated!\n", unit);
		return 0;
	}
	sd = sd_data[unit] = malloc(sizeof(struct sd_data), M_DEVBUF, M_NOWAIT);
	if (!sd) {
		printf("malloc failed in sd.c\n");
		return (0);
	}
	bzero(sd, sizeof(struct sd_data));

	dp = &(sd->params);
	/*
	 * Store information needed to contact our base driver
	 */
	sd->sc_link = sc_link;
	sc_link->device = &sd_switch;
	sc_link->dev_unit = unit;

	if (sd->sc_link->adapter->adapter_info) {
		sd->ad_info = ((*(sd->sc_link->adapter->adapter_info)) (sc_link->adapter_unit));
		sd->cmdscount = sd->ad_info & AD_INF_MAX_CMDS;
		if (sd->cmdscount > SDOUTSTANDING) {
			sd->cmdscount = SDOUTSTANDING;
		}
	} else {
		sd->ad_info = 1;
		sd->cmdscount = 1;
	} 
	sc_link->opennings = sd->cmdscount;
	/*
	 * Use the subdriver to request information regarding
	 * the drive. We cannot use interrupts yet, so the
	 * request must specify this.
	 */
	sd_get_parms(unit, SCSI_NOSLEEP | SCSI_NOMASK);
	printf("sd%d: %dMB (%d total sec), %d cyl, %d head, %d sec, bytes/sec %d\n",
	    unit,
	    dp->disksize / ((1024L * 1024L) / dp->secsiz),
	    dp->disksize,
	    dp->cyls,
	    dp->heads,
	    dp->sectors,
	    dp->secsiz);
	sd->flags |= SDINIT;
	return 0;
}

/*
 * open the device. Make sure the partition info is a up-to-date as can be.
 */
errval
sdopen(dev)
	int	dev;	/* XXX should be dev_t, but avoid promotion problems for now */
{
	errval  errcode = 0;
	u_int32 unit, part;
	struct sd_data *sd;
	struct scsi_link *sc_link;

	unit = UNIT(dev);
	part = PARTITION(dev);
	sd = sd_data[unit];
	/*
	 * Check the unit is legal
	 */
	if (unit >= NSD) {
		return (ENXIO);
	}
	/*
	 * Make sure the disk has been initialised
	 * At some point in the future, get the scsi driver
	 * to look for a new device if we are not initted
	 */
	if ((!sd) || (!(sd->flags & SDINIT))) {
		return (ENXIO);
	}
	sc_link = sd->sc_link;

	SC_DEBUG(sc_link, SDEV_DB1,
	    ("sdopen: dev=0x%x (unit %d (of %d),partition %d)\n"
		,dev, unit, NSD, part));

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
		 * If somebody still has      it open, then forbid re-entry.
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
		printf("sd%d: Can't deal with %d bytes logical blocks\n",
		    unit, sd->params.secsiz);
		Debugger();
		errcode = ENXIO;
		goto bad;
	}
	SC_DEBUG(sc_link, SDEV_DB3, ("Params loaded "));

	/* Lock the pack in. */
	scsi_prevent(sc_link, PR_PREVENT, SCSI_ERR_OK | SCSI_SILENT);

	/*
	 * Load the partition info if not already loaded.
	 */
	if ((errcode = sdgetdisklabel(unit)) && (part != RAW_PART)) {
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
	    && (part != RAW_PART)) {
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
sdclose(dev)
	dev_t   dev;
{
	unsigned char unit, part;
	struct sd_data *sd;

	unit = UNIT(dev);
	part = PARTITION(dev);
	sd = sd_data[unit];
	sd->partflags[part] &= ~SDOPEN;
	sd->openparts &= ~(1 << part);
	scsi_prevent(sd->sc_link, PR_ALLOW, SCSI_SILENT | SCSI_ERR_OK);
	if (!(sd->openparts))
		sd->sc_link->flags &= ~SDEV_OPEN;
	return 0;
}

/*
 * trim the size of the transfer if needed, called by physio
 * basically the smaller of our max and the scsi driver's
 * minphys (note we have no max)
 *
 * Trim buffer length if buffer-size is bigger than page size
 */
void 
sdminphys(bp)
	struct buf *bp;
{
	(*(sd_data[UNIT(bp->b_dev)]->sc_link->adapter->scsi_minphys)) (bp);
}

/*
 * Actually translate the requested transfer into one the physical driver
 * can understand.  The transfer is described by a buf and will include
 * only one physical transfer.
 */
errval 
sdstrategy(bp)
	struct buf *bp;
{
	struct buf *dp;
	u_int32 opri;
	struct sd_data *sd;
	u_int32 unit;

	sdstrats++;
	unit = UNIT((bp->b_dev));
	sd = sd_data[unit];
	SC_DEBUG(sd->sc_link, SDEV_DB2, ("sdstrategy "));
	SC_DEBUG(sd->sc_link, SDEV_DB1,
	    (" %d bytes @ blk%d\n", bp->b_bcount, bp->b_blkno));
	sdminphys(bp);
	/*
	 * If the device has been made invalid, error out
	 */
	if (!(sd->sc_link->flags & SDEV_MEDIA_LOADED)) {
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
	 * Decide which unit and partition we are talking about
	 * only raw is ok if no label
	 */
	if (PARTITION(bp->b_dev) != RAW_PART) {
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
	}
	opri = SPLSD();
	dp = &sd->buf_queue;

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
	return 0;
bad:
	bp->b_flags |= B_ERROR;
done:

	/*
	 * Correctly set the buf to indicate a completed xfer
	 */
	bp->b_resid = bp->b_bcount;
	biodone(bp);
	return 0;
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
sdstart(unit)
	u_int32 unit;
{
	register struct sd_data *sd = sd_data[unit];
	register struct	scsi_link *sc_link = sd->sc_link;
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
		dp->b_actf = bp->av_forw;

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
		nblk = (bp->b_bcount + 511) >> 9;

		/*
		 *  Fill out the scsi command
		 */
		bzero(&cmd, sizeof(cmd));
		cmd.op_code = (bp->b_flags & B_READ)
		    ? READ_BIG : WRITE_BIG;
		cmd.addr_3 = (blkno & 0xff000000) >> 24;
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
		} else {
bad:
			printf("sd%d: oops not queued", unit);
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
sdioctl(dev_t dev, int cmd, caddr_t addr, int flag)
{
	/* struct sd_cmd_buf *args; */
	errval  error = 0;
	unsigned char unit, part;
	register struct sd_data *sd;

	/*
	 * Find the device that the user is talking about
	 */
	unit = UNIT(dev);
	part = PARTITION(dev);
	sd = sd_data[unit];
	SC_DEBUG(sd->sc_link, SDEV_DB1, ("sdioctl (0x%x)", cmd));

	/*
	 * If the device is not valid.. abandon ship
	 */
	if (!(sd->sc_link->flags & SDEV_MEDIA_LOADED))
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
			/*(sd->flags & DKFL_BSDLABEL) ? sd->openparts : */ 0,
#ifdef NetBSD
			    &sd->cpudisklabel
#else
			    sd->dosparts
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
			    /*(sd->flags & SDHAVELABEL) ? sd->openparts : */ 0,
#ifdef NetBSD
			    &sd->cpudisklabel
#else
			    sd->dosparts
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
				    &sd->disklabel,
#ifdef NetBSD
				    &sd->cpudisklabel
#else
				    sd->dosparts
#endif
				    );
				sd->wlabel = wlab;
			}
		} 
		break;

	default:
		if (part == RAW_PART)
			error = scsi_do_ioctl(sd->sc_link, cmd, addr, flag);
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
	struct sd_data *sd = sd_data[unit];

	/*
	 * If the inflo is already loaded, use it
	 */
	if (sd->flags & SDHAVELABEL)
		return (ESUCCESS);

	bzero(&sd->disklabel, sizeof(struct disklabel));
	/*
	 * make partition 3 the whole disk in case of failure then get pdinfo
	 * for historical reasons, make part a same as raw part
	 */
	sd->disklabel.d_partitions[0].p_offset = 0;
	sd->disklabel.d_partitions[0].p_size = sd->params.disksize;
	sd->disklabel.d_partitions[RAW_PART].p_offset = 0;
	sd->disklabel.d_partitions[RAW_PART].p_size = sd->params.disksize;
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
	if (errstring = readdisklabel(makedev(0, (unit << UNITSHIFT) + 3),
	    sdstrategy,
	    &sd->disklabel,
#ifdef NetBSD
	    &sd->cpudisklabel
#else
	    sd->dosparts,
	    0,
	    0
#endif
	    )) {
		printf("sd%d: %s\n", unit, errstring);
		return ENXIO;
	}
	sd->flags |= SDHAVELABEL;	/* WE HAVE IT ALL NOW */
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
	if (scsi_scsi_cmd(sd_data[unit]->sc_link,
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

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	bzero(&rbdata, sizeof(rbdata));
	scsi_cmd.op_code = REASSIGN_BLOCKS;

	rbdata.length_msb = 0;
	rbdata.length_lsb = sizeof(rbdata.defect_descriptor[0]);
	rbdata.defect_descriptor[0].dlbaddr_3 = ((block >> 24) & 0xff);
	rbdata.defect_descriptor[0].dlbaddr_2 = ((block >> 16) & 0xff);
	rbdata.defect_descriptor[0].dlbaddr_1 = ((block >> 8) & 0xff);
	rbdata.defect_descriptor[0].dlbaddr_0 = ((block) & 0xff);

	return (scsi_scsi_cmd(sd_data[unit]->sc_link,
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
	struct sd_data *sd = sd_data[unit];
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
	if (sd->flags & SDEV_MEDIA_LOADED)
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
	if (scsi_scsi_cmd(sd->sc_link,
		(struct scsi_generic *) &scsi_cmd,
		sizeof(scsi_cmd),
		(u_char *) & scsi_sense,
		sizeof(scsi_sense),
		SD_RETRIES,
		2000,
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

		SC_DEBUG(sd->sc_link, SDEV_DB3,
		    ("%d cyls, %d heads, %d precomp, %d red_write, %d land_zone\n",
			_3btol(&scsi_sense.pages.rigid_geometry.ncyl_2),
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
		disk_parms->cyls = _3btol(&scsi_sense.pages.rigid_geometry.ncyl_2);
		disk_parms->secsiz = _3btol(scsi_sense.blk_desc.blklen);

		sectors = sd_size(unit, flags);
		disk_parms->disksize = sectors;
		sectors /= (disk_parms->heads * disk_parms->cyls);
		disk_parms->sectors = sectors;	/* dubious on SCSI *//*XXX */
	}
	sd->sc_link->flags |= SDEV_MEDIA_LOADED;
	return 0;
}

int
sdsize(dev_t dev)
{
	u_int32 unit = UNIT(dev), part = PARTITION(dev), val;
	struct sd_data *sd;

	if (unit >= NSD)
		return -1;

	sd = sd_data[unit];
	if (!sd)
		return -1;
	if ((sd->flags & SDINIT) == 0)
		return -1;
	if (sd == 0 || (sd->flags & SDHAVELABEL) == 0) {
		val = sdopen(MAKESDDEV(major(dev), unit, RAW_PART), FREAD, S_IFBLK, 0);
		if (val != 0)
			return -1;
	}
	if (sd->flags & SDWRITEPROT)
		return -1;

	return (int)sd->disklabel.d_partitions[part].p_size;
}


#define SCSIDUMP 1
#undef	SCSIDUMP
#define NOT_TRUSTED 1

#ifdef SCSIDUMP
#include <vm/vm.h>

static struct scsi_xfer sx;
#define	MAXTRANSFER 8		/* 1 page at a time */

/*
 * dump all of physical memory into the partition specified, starting
 * at offset 'dumplo' into the partition.
 */
errval
sddump(dev_t dev)
{				/* dump core after a system crash */
	register struct sd_data *sd;	/* disk unit to do the IO */
	int32	num;		/* number of sectors to write */
	u_int32	unit, part;
	int32	blkoff, blknum, blkcnt = MAXTRANSFER;
	int32	nblocks;
	char	*addr;
	struct	scsi_rw_big cmd;
	extern	int Maxmem;
	static	int sddoingadump = 0;
#define MAPTO CADDR1
	extern	caddr_t MAPTO;	/* map the page we are about to write, here */
	struct	scsi_xfer *xs = &sx;
	errval	retval;
	int	c;

	addr = (char *) 0;	/* starting address */

	/* toss any characters present prior to dump */
	while ((c = sgetc(1)) && (c != 0x100)); /*syscons and pccons differ */

	/* size of memory to dump */
	num = Maxmem;
	unit = UNIT(dev);	/* eventually support floppies? */
	part = PARTITION(dev);	/* file system */
	/* check for acceptable drive number */
	if (unit >= NSD)
		return (ENXIO);	/* 31 Jul 92 */

	sd = sd_data[unit];
	if (!sd)
		return (ENXIO);
	/* was it ever initialized etc. ? */
	if (!(sd->flags & SDINIT))
		return (ENXIO);
	if (sd->sc_link->flags & SDEV_MEDIA_LOADED != SDEV_MEDIA_LOADED)
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
	/* blkcnt = initialise_me; */
	while (num > 0) {
		pmap_enter(kernel_pmap,
		    MAPTO,
		    trunc_page(addr),
		    VM_PROT_READ,
		    TRUE);
#ifndef	NOT_TRUSTED
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
		xs->flags |= SCSI_NOMASK | SCSI_NOSLEEP | INUSE;
		xs->sc_link = sd->sc_link;
		xs->retries = SD_RETRIES;
		xs->timeout = 10000;	/* 10000 millisecs for a disk ! */
		xs->cmd = (struct scsi_generic *) &cmd;
		xs->cmdlen = sizeof(cmd);
		xs->resid = blkcnt * 512;
		xs->error = XS_NOERROR;
		xs->bp = 0;
		xs->data = (u_char *) MAPTO;
		xs->datalen = blkcnt * 512;

		/*
		 * Pass all this info to the scsi driver.
		 */
		retval = (*(sd->sc_link->adapter->scsi_cmd)) (xs);
		switch (retval) {
		case SUCCESSFULLY_QUEUED:
		case HAD_ERROR:
			return (ENXIO);		/* we said not to sleep! */
		case COMPLETE:
			break;
		default:
			return (ENXIO);		/* we said not to sleep! */
		}
#else	/* NOT_TRUSTED */
		/* lets just talk about this first... */
		printf("sd%d: dump addr 0x%x, blk %d\n", unit, addr, blknum);
#endif	/* NOT_TRUSTED */

		if ((unsigned) addr % (1024 * 1024) == 0)
			printf("%d ", num / 2048);
		/* update block count */
		num -= blkcnt;
		blknum += blkcnt;
		(int) addr += 512 * blkcnt;

		/* operator aborting dump? */
		if ((c = sgetc(1)) && (c != 0x100))
			return (EINTR);
	}
	return (0);
}
#else	/* SCSIDUMP */
errval
sddump()
{
	printf("\nsddump()        -- not implemented\n");
	DELAY(60000000);	/* 60 seconds */
	return -1;
}
#endif	/* SCSIDUMP */
