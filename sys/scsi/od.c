/*
 * Copyright (c) 1995 Shunsuke Akiyama.  All rights reserved.
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
 *	This product includes software developed by Shunsuke Akiyama.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Shunsuke Akiyama AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: od.c,v 1.1 1995/10/31 17:25:58 joerg Exp $
 */

/*
 * TODO:
 *   1. Add optical disk specific ioctl functions, such as eject etc.
 */

#define SPLOD splbio
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
#include <sys/diskslice.h>
#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>
#include <vm/vm.h>
#include <sys/devconf.h>
#include <sys/dkstat.h>
#include <machine/md_var.h>
#include <i386/i386/cons.h>		/* XXX */

u_int32 odstrats, odqueues;

#define SECSIZE 512
#define	ODOUTSTANDING	4
#define	OD_RETRIES	4
#define	MAXTRANSFER	8		/* 1 page at a time */

#define	PARTITION(dev)	dkpart(dev)
#define	ODUNIT(dev)	dkunit(dev)

/* XXX introduce a dkmodunit() macro for this. */
#define ODSETUNIT(DEV, U) \
 makedev(major(DEV), dkmakeminor((U), dkslice(DEV), dkpart(DEV)))

errval	od_get_parms __P((int unit, int flags));
static	void	odstrategy1 __P((struct buf *));

int	od_sense_handler __P((struct scsi_xfer *));
void    odstart __P((u_int32, u_int32));

struct scsi_data {
	u_int32 flags;
#define	ODINIT		0x04	/* device has been init'd */
	struct disk_parms {
		u_char  heads;	/* Number of heads */
		u_int16 cyls;	/* Number of cylinders */
		u_char  sectors; /* dubious *//* Number of sectors/track */
		u_int16 secsiz;	/* Number of bytes/sector */
		u_int32 disksize;	/* total number sectors */
	} params;
	struct diskslices *dk_slices;	/* virtual drives */
	struct buf_queue_head buf_queue;
	int dkunit;		/* disk stats unit number */
};

static int odunit(dev_t dev) { return ODUNIT(dev); }
static dev_t odsetunit(dev_t dev, int unit) { return ODSETUNIT(dev, unit); }

errval od_open __P((dev_t dev, int mode, int fmt, struct proc *p,
		    struct scsi_link *sc_link));
errval od_ioctl(dev_t dev, int cmd, caddr_t addr, int flag,
		struct proc *p, struct scsi_link *sc_link);
errval od_close __P((dev_t dev, int fflag, int fmt, struct proc *p,
		     struct scsi_link *sc_link));
void od_strategy(struct buf *bp, struct scsi_link *sc_link);

SCSI_DEVICE_ENTRIES(od)

struct scsi_device od_switch =
{
    od_sense_handler,
    odstart,			/* have a queue, served by this */
    NULL,			/* have no async handler */
    NULL,			/* Use default 'done' routine */
    "od",
    0,
	{0, 0},
	0,				/* Link flags */
	odattach,
	"Optical",
	odopen,
    sizeof(struct scsi_data),
	T_OPTICAL,
	odunit,
	odsetunit,
	od_open,
	od_ioctl,
	od_close,
	od_strategy,
};

static struct scsi_xfer sx;

static int
od_externalize(struct proc *p, struct kern_devconf *kdc, void *userp,
	       size_t len)
{
	return scsi_externalize(SCSI_LINK(&od_switch, kdc->kdc_unit),
				userp, &len);
}

static struct kern_devconf kdc_od_template = {
	0, 0, 0,		/* filled in by dev_attach */
	"od", 0, MDDC_SCSI,
	od_externalize, 0, scsi_goaway, SCSI_EXTERNALLEN,
	&kdc_scbus0,		/* XXX parent */
	0,			/* parentdata */
	DC_UNKNOWN,		/* not supported */
};

static inline void
od_registerdev(int unit)
{
	struct kern_devconf *kdc;

	MALLOC(kdc, struct kern_devconf *, sizeof *kdc, M_TEMP, M_NOWAIT);
	if(!kdc) return;
	*kdc = kdc_od_template;
	kdc->kdc_unit = unit;
	kdc->kdc_description = od_switch.desc;
	dev_attach(kdc);
	if(dk_ndrive < DK_NDRIVE) {
		sprintf(dk_names[dk_ndrive], "od%d", unit);
		dk_wpms[dk_ndrive] = (8*1024*1024/2);
		SCSI_DATA(&od_switch, unit)->dkunit = dk_ndrive++;
	} else {
		SCSI_DATA(&od_switch, unit)->dkunit = -1;
	}
}


/*
 * The routine called by the low level scsi routine when it discovers
 * a device suitable for this driver.
 */
errval
odattach(struct scsi_link *sc_link)
{
	u_int32 unit;
	struct disk_parms *dp;

	struct scsi_data *od = sc_link->sd;

	unit = sc_link->dev_unit;

	dp = &(od->params);

	if (sc_link->opennings > ODOUTSTANDING)
		sc_link->opennings = ODOUTSTANDING;

	TAILQ_INIT(&od->buf_queue);
	/*
	 * Use the subdriver to request information regarding
	 * the drive. We cannot use interrupts yet, so the
	 * request must specify this.
	 */
	od_get_parms(unit, SCSI_NOSLEEP | SCSI_NOMASK | SCSI_SILENT);
	/*
	 * if we don't have actual parameters, assume 512 bytes/sec
	 * (could happen on removable media - MOD)
	 * -- this avoids the division below from falling over
	 */
	if(dp->secsiz == 0) dp->secsiz = SECSIZE;
	if (dp->disksize != 0) {
		printf("%ldMB (%ld %d byte sectors)",
		    dp->disksize / ((1024L * 1024L) / dp->secsiz),
		    dp->disksize,
		    dp->secsiz);
	} else {
		printf("od not present");
	}

#ifndef SCSI_REPORT_GEOMETRY
	if ( (sc_link->flags & SDEV_BOOTVERBOSE) )
#endif
	{
		sc_print_addr(sc_link);
		printf("with %d cyls, %d heads, and an average %d sectors/track",
	   	dp->cyls, dp->heads, dp->sectors);
	}

	od->flags |= ODINIT;
	od_registerdev(unit);

	return 0;
}

/*
 * open the device. Make sure the partition info is a up-to-date as can be.
 */
errval
od_open(dev, mode, fmt, p, sc_link)
	dev_t	dev;
	int	mode;
	int	fmt;
	struct proc *p;
	struct scsi_link *sc_link;
{
	errval  errcode = 0;
	u_int32 unit;
	struct disklabel label;
	struct scsi_data *od;

	unit = ODUNIT(dev);
	od = sc_link->sd;

	/*
	 * Make sure the disk has been initialized
	 * At some point in the future, get the scsi driver
	 * to look for a new device if we are not initted
	 */
	if ((!od) || (!(od->flags & ODINIT))) {
		return (ENXIO);
	}

	SC_DEBUG(sc_link, SDEV_DB1,
	    ("od_open: dev=0x%lx (unit %ld, partition %d)\n",
		dev, unit, PARTITION(dev)));

	/*
	 * Check that it is still responding and ok.  if the media has
	 * been changed this will result in a "unit attention" error
	 * which the error code will disregard because the SDEV_OPEN
	 * or SDEV_MEDIA_LOADED flag is not yet set.  Makes sure that
	 * we know it if the media has been changed..
	 */
	scsi_test_unit_ready(sc_link, SCSI_SILENT);

	/*
	 * If it's been invalidated, then forget the label
	 */
	sc_link->flags |= SDEV_OPEN;	/* unit attn becomes an err now */
	if (!(sc_link->flags & SDEV_MEDIA_LOADED)) {
		/*
		 * If somebody still has it open, then forbid re-entry.
		 */
		if (dsisopen(od->dk_slices)) {
			errcode = ENXIO;
			goto bad;
		}

		if (od->dk_slices != NULL)
			dsgone(&od->dk_slices);
	}

	/*
	 * This time actually take notice of error returns
	 */
	if (scsi_test_unit_ready(sc_link, SCSI_SILENT) != 0) {
		SC_DEBUG(sc_link, SDEV_DB3, ("not ready\n"));
		errcode = ENXIO;
		goto bad;
	}
	SC_DEBUG(sc_link, SDEV_DB3, ("device present\n"));

	/*
	 * In case it is a funny one, tell it to start
	 * not needed for  most hard drives (ignore failure)
	 */
	scsi_start_unit(sc_link, SCSI_ERR_OK | SCSI_SILENT);
	scsi_prevent(sc_link, PR_PREVENT, SCSI_ERR_OK | SCSI_SILENT);
	SC_DEBUG(sc_link, SDEV_DB3, ("'start' attempted "));

	/*
	 * Load the physical device parameters
	 */
	errcode = od_get_parms(unit, 0);	/* sets SDEV_MEDIA_LOADED */
	if (errcode) {
		goto bad;
	}
	if (od->params.secsiz != SECSIZE) {	/* XXX One day... */
		printf("od%ld: Can't deal with %d bytes logical blocks\n",
		    unit, od->params.secsiz);
		Debugger("od");
		errcode = ENXIO;
		goto bad;
	}
	SC_DEBUG(sc_link, SDEV_DB3, ("params loaded "));

	/* Build label for whole disk. */
	bzero(&label, sizeof label);
	label.d_secsize = od->params.secsiz;
	label.d_nsectors = od->params.sectors;
	label.d_ntracks = od->params.heads;
	label.d_ncylinders = od->params.cyls;
	label.d_secpercyl = od->params.heads * od->params.sectors;
	if (label.d_secpercyl == 0)
		label.d_secpercyl = 100;
		/* XXX as long as it's not 0 - readdisklabel divides by it (?) */
	label.d_secperunit = od->params.disksize;

	/* Initialize slice tables. */
	errcode = dsopen("od", dev, fmt, &od->dk_slices, &label, odstrategy1,
			 (ds_setgeom_t *)NULL);
	if (errcode != 0)
		goto bad;
	SC_DEBUG(sc_link, SDEV_DB3, ("Slice tables initialized "));

	SC_DEBUG(sc_link, SDEV_DB3, ("open %ld %ld\n", odstrats, odqueues));

	return 0;

bad:
	if (!dsisopen(od->dk_slices)) {
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
od_close(dev, fflag, fmt, p, sc_link)
	dev_t	dev;
	int	fflag;
	int	fmt;
	struct proc *p;
	struct scsi_link *sc_link;
{
	struct scsi_data *od;

	od = sc_link->sd;
	dsclose(dev, fmt, od->dk_slices);
	if (!dsisopen(od->dk_slices)) {
		scsi_prevent(sc_link, PR_ALLOW, SCSI_SILENT | SCSI_ERR_OK);
		sc_link->flags &= ~SDEV_OPEN;
	}
	return (0);
}

/*
 * Actually translate the requested transfer into one the physical driver
 * can understand.  The transfer is described by a buf and will include
 * only one physical transfer.
 */
void
od_strategy(struct buf *bp, struct scsi_link *sc_link)
{
	u_int32 opri;
	struct scsi_data *od;
	u_int32 unit;

	odstrats++;
	unit = ODUNIT((bp->b_dev));
	od = sc_link->sd;

	/*
	 * If the device has been made invalid, error out
	 */
	if (!(sc_link->flags & SDEV_MEDIA_LOADED)) {
		bp->b_error = EIO;
		goto bad;
	}

	/*
	 * Odd number of bytes
	 */
	if (bp->b_bcount % DEV_BSIZE != 0) {
		bp->b_error = EINVAL;
		goto bad;
	}
	/*
	 * Do bounds checking, adjust transfer, set b_cylin and b_pbklno.
	 */
	if (dscheck(bp, od->dk_slices) <= 0)
		goto done;	/* XXX check b_resid */

	opri = SPLOD();

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
	TAILQ_INSERT_TAIL(&od->buf_queue, bp, b_act);

	/*
	 * Tell the device to get going on the transfer if it's
	 * not doing anything, otherwise just wait for completion
	 */
	odstart(unit, 0);

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

static void
odstrategy1(struct buf *bp)
{
	/*
	 * XXX - do something to make odstrategy() but not this block while
	 * we're doing dsinit() and dsioctl().
	 */
	odstrategy(bp);
}

/*
 * odstart looks to see if there is a buf waiting for the device
 * and that the device is not already busy. If both are true,
 * It dequeues the buf and creates a scsi command to perform the
 * transfer in the buf. The transfer request will call scsi_done
 * on completion, which will in turn call this routine again
 * so that the next queued transfer is performed.
 * The bufs are queued by the strategy routine (odstrategy)
 *
 * This routine is also called after other non-queued requests
 * have been made of the scsi driver, to ensure that the queue
 * continues to be drained.
 *
 * must be called at the correct (highish) spl level
 * odstart() is called at SPLOD  from odstrategy and scsi_done
 */
void
odstart(u_int32 unit, u_int32 flags)
{
	register struct	scsi_link *sc_link = SCSI_LINK(&od_switch, unit);
	register struct scsi_data *od = sc_link->sd;
	struct buf *bp = 0;
	struct scsi_rw_big cmd;
	u_int32 blkno, nblk;

	SC_DEBUG(sc_link, SDEV_DB2, ("odstart "));
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
		bp = od->buf_queue.tqh_first;
		if (bp == NULL) {	/* yes, an assign */
			return;
		}
		TAILQ_REMOVE( &od->buf_queue, bp, b_act);

		/*
		 *  If the device has become invalid, abort all the
		 * reads and writes until all files have been closed and
		 * re-openned
		 */
		if (!(sc_link->flags & SDEV_MEDIA_LOADED)) {
			goto bad;
		}
		/*
		 * We have a buf, now we know we are going to go through
		 * With this thing..
		 */
		blkno = bp->b_pblkno;
		if (bp->b_bcount & (SECSIZE - 1))
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
			OD_RETRIES,
			100000,
			bp,
			flags | ((bp->b_flags & B_READ) ?
			    SCSI_DATA_IN : SCSI_DATA_OUT))
		    == SUCCESSFULLY_QUEUED) {
			odqueues++;
			if(od->dkunit >= 0) {
				dk_xfer[od->dkunit]++;
				dk_seek[od->dkunit]++; /* don't know */
				dk_wds[od->dkunit] += bp->b_bcount >> 6;
			}
		} else {
bad:
			printf("od%ld: oops not queued\n", unit);
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
od_ioctl(dev_t dev, int cmd, caddr_t addr, int flag, struct proc *p,
	 struct scsi_link *sc_link)
{
	/* struct od_cmd_buf *args; */
	errval  error;
	struct scsi_data *od;

	/*
	 * Find the device that the user is talking about
	 */
	od = sc_link->sd;
	SC_DEBUG(sc_link, SDEV_DB1, ("odioctl (0x%x)", cmd));

#if 0
	/* Wait until we have exclusive access to the device. */
	/* XXX this is how wd does it.  How did we work without this? */
	wdsleep(du->dk_ctrlr, "wdioct");
#endif

	/*
	 * If the device is not valid.. abandon ship
	 */
	if (!(sc_link->flags & SDEV_MEDIA_LOADED))
		return (EIO);

	if (cmd ==  DIOCSBAD)
		return (EINVAL);	/* XXX */
	error = dsioctl("od", dev, cmd, addr, flag, &od->dk_slices,
			odstrategy1, (ds_setgeom_t *)NULL);
	if (error != -1)
		return (error);
	if (PARTITION(dev) != RAW_PART)
		return (ENOTTY);
	return (scsi_do_ioctl(dev, cmd, addr, flag, p, sc_link));
}

/*
 * Find out from the device what it's capacity is
 */
u_int32
od_size(unit, flags)
	int	unit, flags;
{
	struct scsi_read_cap_data rdcap;
	struct scsi_read_capacity scsi_cmd;
	u_int32 size;
	struct scsi_link *sc_link = SCSI_LINK(&od_switch, unit);

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
		OD_RETRIES,
		20000,
		NULL,
		flags | SCSI_DATA_IN) != 0) {
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
od_reassign_blocks(unit, block)
	int	unit, block;
{
	struct scsi_reassign_blocks scsi_cmd;
	struct scsi_reassign_blocks_data rbdata;
	struct scsi_link *sc_link = SCSI_LINK(&od_switch, unit);

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
		OD_RETRIES,
		20000,
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
od_get_parms(unit, flags)
	int	unit, flags;
{
	struct scsi_link *sc_link = SCSI_LINK(&od_switch, unit);
	struct scsi_data *od = sc_link->sd;
	struct disk_parms *disk_parms = &od->params;
	u_int32 sectors;
	errval retval;

	/*
	 * First check if we have it all loaded
	 */
	if (sc_link->flags & SDEV_MEDIA_LOADED)
		return 0;

	/*
	 * use adaptec standard ficticious geometry
	 * this depends on which controller (e.g. 1542C is
	 * different. but we have to put SOMETHING here..)
	 */
	sectors = od_size(unit, flags);
	disk_parms->heads = 64;
	disk_parms->sectors = 32;
	disk_parms->cyls = sectors / (64 * 32);
	disk_parms->secsiz = SECSIZE;
	disk_parms->disksize = sectors;

	if (sectors != 0) {
		sc_link->flags |= SDEV_MEDIA_LOADED;
		retval = 0;
	} else {
		retval = ENXIO;
	}
	return retval;
}

int
odsize(dev_t dev)
{
	return -1;
}

/*
 * sense handler: Called to determine what to do when the
 * device returns a CHECK CONDITION.
 */

int
od_sense_handler(struct scsi_xfer *xs)
{
	struct scsi_sense_data *sense;
	struct scsi_sense_extended *ext;
	int asc, ascq;

	sense = &(xs->sense);
	ext = (struct scsi_sense_extended *)&(sense->ext.extended);

	/* I don't know what the heck to do with a deferred error,
	 * so I'll just kick it back to the caller.
	 */
	if ((sense->error_code & SSD_ERRCODE) == 0x71)
		return SCSIRET_CONTINUE;

	if (((sense->error_code & SSD_ERRCODE) == 0x70) &&
		((sense->ext.extended.flags & SSD_KEY) == 0x04))
		/* No point in retrying Hardware Failure */
			return SCSIRET_CONTINUE;

	if (((sense->error_code & SSD_ERRCODE) == 0x70) &&
		((sense->ext.extended.flags & SSD_KEY) == 0x05))
		/* No point in retrying Illegal Requests */
			return SCSIRET_CONTINUE;

	asc = (ext->extra_len >= 5) ? ext->add_sense_code : 0;
	ascq = (ext->extra_len >= 6) ? ext->add_sense_code_qual : 0;

	if (asc == 0x11 || asc == 0x30 || asc == 0x31 || asc == 0x53
	    || asc == 0x5a) {
		/* Unrecovered errors */
		return SCSIRET_CONTINUE;
	}
	if (asc == 0x21 && ascq == 0) {
		/* Logical block address out of range */
		return SCSIRET_CONTINUE;
	}
	if (asc == 0x27 && ascq == 0) {
		/* Write protected */
		return SCSIRET_CONTINUE;
	}
	if (asc == 0x28 && ascq == 0) {
		/* Not ready to ready transition */
		/* (medium may have changed) */
		return SCSIRET_CONTINUE;
	}
	if (asc == 0x3a && ascq == 0) {
		/* Medium not present */
		return SCSIRET_CONTINUE;
	}

	/* Retry all disk errors.
	 */
	scsi_sense_print(xs);
	if (xs->retries)
		printf(", retries:%d\n", xs->retries);
	else
		printf(", FAILURE\n");

	return SCSIRET_DO_RETRY;
}
