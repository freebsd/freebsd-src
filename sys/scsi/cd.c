/*
 * Written by Julian Elischer (julian@tfs.com)
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
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 *      $Id: cd.c,v 1.18 1994/04/20 07:06:51 davidg Exp $
 */

#define SPLCD splbio
#define ESUCCESS 0
#include <cd.h>
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
#include <sys/cdio.h>

#include <sys/errno.h>
#include <sys/disklabel.h>
#include <scsi/scsi_all.h>
#include <scsi/scsi_cd.h>
#include <scsi/scsi_disk.h>	/* rw_big and start_stop come from there */
#include <scsi/scsiconf.h>

/* static function prototypes */
static errval cd_get_parms(int, int);
static errval cd_get_mode(u_int32, struct cd_mode_data *, u_int32);
static errval cd_set_mode(u_int32 unit, struct cd_mode_data *);
static errval cd_read_toc(u_int32, u_int32, u_int32, struct cd_toc_entry *,
			  u_int32);


int32   cdstrats, cdqueues;

#include <ddb.h>
#if	NDDB > 0
#else	/* NDDB > 0 */
#define Debugger()
#endif	/* NDDB > 0 */

#define PAGESIZ 	4096
#define SECSIZE 2048	/* XXX */	/* default only */
#define	CDOUTSTANDING	2
#define	CDRETRIES	1

#define	UNITSHIFT	3
#define PARTITION(z)	(minor(z) & 0x07)
#define	RAW_PART	3
#define UNIT(z)		(  (minor(z) >> UNITSHIFT) )

errval  cdstrategy();

void    cdstart();
struct scsi_device cd_switch =
{
    NULL,			/* use default error handler */
    cdstart,			/* we have a queue, which is started by this */
    NULL,			/* we do not have an async handler */
    NULL,			/* use default 'done' routine */
    "cd",			/* we are to be refered to by this name */
    0,				/* no device specific flags */
    0, 0			/* spares not used */
};

struct cd_data {
	u_int32 flags;
#define	CDINIT		0x04	/* device has been init'd */
	struct scsi_link *sc_link;	/* address of scsi low level switch */
	u_int32 cmdscount;	/* cmds allowed outstanding by board */
	struct cd_parms {
		u_int32 blksize;
		u_long  disksize;	/* total number sectors */
	} params;
	struct disklabel disklabel;
	u_int32 partflags[MAXPARTITIONS];	/* per partition flags */
#define CDOPEN	0x01
	u_int32 openparts;	/* one bit for each open partition */
	u_int32 xfer_block_wait;
	struct buf buf_queue;
};

#define CD_STOP		0
#define CD_START	1
#define CD_EJECT	-2

struct cd_driver {
	u_int32 size;
	struct cd_data **cd_data;
} cd_driver;

static u_int32 next_cd_unit = 0;

/*
 * The routine called by the low level scsi routine when it discovers
 * A device suitable for this driver
 */
int 
cdattach(sc_link)
	struct scsi_link *sc_link;
{
	u_int32 unit, i;
	unsigned char *tbl;
	struct cd_data *cd, **cdrealloc;
	struct cd_parms *dp;

	SC_DEBUG(sc_link, SDEV_DB2, ("cdattach "));

	/*
	 * Fill out any more info in the
	 * Link structure that we can
	 */
	unit = next_cd_unit++;
	sc_link->device = &cd_switch;
	sc_link->dev_unit = unit;
	/*
	 * allocate the resources for another drive
	 * if we have already allocate a cd_data pointer we must
	 * copy the old pointers into a new region that is
	 * larger and release the old region, aka realloc
	 */
	/* XXX
	 * This if will always be true for now, but future code may
	 * preallocate more units to reduce overhead.  This would be
	 * done by changing the malloc to be (next_cd_unit * x) and
	 * the cd_driver.size++ to be +x
	 */
	if (unit >= cd_driver.size) {
		cdrealloc =
		    malloc(sizeof(cd_driver.cd_data) * next_cd_unit,
		    M_DEVBUF, M_NOWAIT);
		if (!cdrealloc) {
			printf("cd%d: malloc failed for cdrealloc\n", unit);
			return (0);
		}
		/* Make sure we have something to copy before we copy it */
		bzero(cdrealloc, sizeof(cd_driver.cd_data) * next_cd_unit);
		if (cd_driver.size) {
			bcopy(cd_driver.cd_data, cdrealloc,
			    sizeof(cd_driver.cd_data) * cd_driver.size);
			free(cd_driver.cd_data, M_DEVBUF);
		}
		cd_driver.cd_data = cdrealloc;
		cd_driver.cd_data[unit] = NULL;
		cd_driver.size++;
	}
	if (cd_driver.cd_data[unit]) {
		printf("cd%d: Already has storage!\n", unit);
		return (0);
	}
	/*
	 * allocate the per drive data area
	 */
	cd = cd_driver.cd_data[unit] =
	    malloc(sizeof(struct cd_data), M_DEVBUF, M_NOWAIT);
	if (!cd) {
		printf("cd%d: malloc failed for cd_data\n", unit);
		return (0);
	}
	bzero(cd, sizeof(struct cd_data));
	dp = &(cd->params);
	/*
	 * Store information needed to contact our base driver
	 */
	cd->sc_link = sc_link;
	/* only allow 1 outstanding command on tapes */
	sc_link->opennings = cd->cmdscount = CDOUTSTANDING;

	/*
	 * Use the subdriver to request information regarding
	 * the drive. We cannot use interrupts yet, so the
	 * request must specify this.
	 */
	cd_get_parms(unit, SCSI_NOSLEEP | SCSI_NOMASK);
	if (dp->disksize) {
		printf("cd%d: cd present.[%d x %d byte records]\n",
		    unit,
		    cd->params.disksize,
		    cd->params.blksize);
	} else {
		printf("cd%d: drive empty\n", unit);
	}
	cd->flags |= CDINIT;
	return (1);
}

/*
 * open the device. Make sure the partition info is a up-to-date as can be.
 */
errval 
cdopen(dev)
	dev_t dev;
{
	errval  errcode = 0;
	u_int32 unit, part;
	struct cd_parms cd_parms;
	struct cd_data *cd;
	struct scsi_link *sc_link;
	u_int32 heldflags;

	unit = UNIT(dev);
	part = PARTITION(dev);

	/*
	 * Check the unit is legal
	 */
	if (unit >= cd_driver.size) {
		return (ENXIO);
	}
	cd = cd_driver.cd_data[unit];
	/*
	 * Make sure the device has been initialised
	 */
	if ((cd == NULL) || (!(cd->flags & CDINIT)))
		return (ENXIO);

	sc_link = cd->sc_link;
	SC_DEBUG(sc_link, SDEV_DB1,
	    ("cdopen: dev=0x%x (unit %d (of %d),partition %d)\n",
		dev, unit, cd_driver.size, part));
	/*
	 * If it's been invalidated, and not everybody has closed it then
	 * forbid re-entry.  (may have changed media)
	 */
	if ((!(sc_link->flags & SDEV_MEDIA_LOADED))
	    && (cd->openparts))
		return (ENXIO);

	/*
	 * Check that it is still responding and ok.
	 * if the media has been changed this will result in a
	 * "unit attention" error which the error code will
	 * disregard because the SDEV_MEDIA_LOADED flag is not yet set
	 */
	scsi_test_unit_ready(sc_link, SCSI_SILENT);

	/*
	 * Next time actually take notice of error returns
	 */
	sc_link->flags |= SDEV_OPEN;	/* unit attn errors are now errors */
	if (scsi_test_unit_ready(sc_link, SCSI_SILENT) != 0) {
		SC_DEBUG(sc_link, SDEV_DB3, ("not ready\n"));
		errcode = ENXIO;
		goto bad;
	}
	SC_DEBUG(sc_link, SDEV_DB3, ("Device present\n"));
	/*
	 * In case it is a funny one, tell it to start
	 * not needed for some drives
	 */
	scsi_start_unit(sc_link, CD_START);
	scsi_prevent(sc_link, PR_PREVENT, SCSI_SILENT);
	SC_DEBUG(sc_link, SDEV_DB3, ("started "));
	/*
	 * Load the physical device parameters 
	 */
	if (cd_get_parms(unit, 0)) {
		errcode = ENXIO;
		goto bad;
	}
	SC_DEBUG(sc_link, SDEV_DB3, ("Params loaded "));
	/*
	 * Make up some partition information
	 */
	cdgetdisklabel(unit);
	SC_DEBUG(sc_link, SDEV_DB3, ("Disklabel fabricated "));
	/*
	 * Check the partition is legal
	 */
	if ((part >= cd->disklabel.d_npartitions)
	    && (part != RAW_PART)) {
		SC_DEBUG(sc_link, SDEV_DB3, ("partition %d > %d\n", part
			,cd->disklabel.d_npartitions));
		errcode = ENXIO;
		goto bad;
	}
	/*
	 *  Check that the partition exists
	 */
	if ((cd->disklabel.d_partitions[part].p_fstype == FS_UNUSED)
	    && (part != RAW_PART)) {
		SC_DEBUG(sc_link, SDEV_DB3, ("part %d type UNUSED\n", part));
		errcode = ENXIO;
		goto bad;
	}
	cd->partflags[part] |= CDOPEN;
	cd->openparts |= (1 << part);
	SC_DEBUG(sc_link, SDEV_DB3, ("open complete\n"));
	sc_link->flags |= SDEV_MEDIA_LOADED;
	return (0);
      bad:

	/*
	 *  if we would have been the only open
	 * then leave things back as they were
	 */
	if (!(cd->openparts)) {
		sc_link->flags &= ~SDEV_OPEN;
		scsi_prevent(sc_link, PR_ALLOW, SCSI_SILENT);
	}
	return (errcode);
}

/*
 * close the device.. only called if we are the LAST
 * occurence of an open device
 */
errval 
cdclose(dev)
	dev_t   dev;
{
	u_int8  unit, part;
	u_int32 old_priority;
	struct cd_data *cd;
	struct scsi_link *sc_link;

	unit = UNIT(dev);
	part = PARTITION(dev);
	cd = cd_driver.cd_data[unit];
	sc_link = cd->sc_link;
	SC_DEBUG(sc_link, SDEV_DB2, ("cd%d: closing part %d\n", unit, part));
	cd->partflags[part] &= ~CDOPEN;
	cd->openparts &= ~(1 << part);

	/*
	 * If we were the last open of the entire device, release it.
	 */
	if (!(cd->openparts)) {
		scsi_prevent(sc_link, PR_ALLOW, SCSI_SILENT);
		cd->sc_link->flags &= ~SDEV_OPEN;
	}
	return (0);
}

/*
 * trim the size of the transfer if needed,
 * called by physio
 * basically the smaller of our max and the scsi driver's
 * minphys (note we have no max ourselves)
 *
 * Trim buffer length if buffer-size is bigger than page size
 */
void 
cdminphys(bp)
	struct buf *bp;
{
	(*(cd_driver.cd_data[UNIT(bp->b_dev)]->sc_link->adapter->scsi_minphys)) (bp);
}

/*
 * Actually translate the requested transfer into one the physical driver can
 * understand.  The transfer is described by a buf and will include only one
 * physical transfer.
 */
errval 
cdstrategy(bp)
	struct buf *bp;
{
	struct buf *dp;
	u_int32 opri;
	u_int32 unit = UNIT((bp->b_dev));
	struct cd_data *cd = cd_driver.cd_data[unit];

	cdstrats++;
	SC_DEBUG(cd->sc_link, SDEV_DB2, ("\ncdstrategy "));
	SC_DEBUG(cd->sc_link, SDEV_DB1, ("cd%d: %d bytes @ blk%d\n",
		unit, bp->b_bcount, bp->b_blkno));
	cdminphys(bp);
	/*
	 * If the device has been made invalid, error out
	 * maybe the media changed
	 */
	if (!(cd->sc_link->flags & SDEV_MEDIA_LOADED)) {
		bp->b_error = EIO;
		goto bad;
	}
	/*
	 * can't ever write to a CD
	 */
	if ((bp->b_flags & B_READ) == 0) {
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
	 */
	if (PARTITION(bp->b_dev) != RAW_PART) {
		/*
		 * do bounds checking, adjust transfer. if error, process.
		 * if end of partition, just return
		 */
		if (bounds_check_with_label(bp, &cd->disklabel, 1) <= 0)
			goto done;
		/* otherwise, process transfer request */
	} else {
		bp->b_pblkno = bp->b_blkno;
		bp->b_resid = 0;
	}
	opri = SPLCD();
	dp = &cd->buf_queue;

	/*
	 * Use a bounce buffer if necessary
	 */
#ifndef NOBOUNCE
	if (cd->sc_link->flags & SDEV_BOUNCE)
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
	cdstart(unit);

	splx(opri);
	return 0;		/* XXX ??? is this the right return? */
      bad:
	bp->b_flags |= B_ERROR;
      done:

	/*
	 * Correctly set the buf to indicate a completed xfer
	 */
	bp->b_resid = bp->b_bcount;
	biodone(bp);
	return (0);
}

/*
 * cdstart looks to see if there is a buf waiting for the device
 * and that the device is not already busy. If both are true,
 * It deques the buf and creates a scsi command to perform the
 * transfer in the buf. The transfer request will call scsi_done
 * on completion, which will in turn call this routine again
 * so that the next queued transfer is performed.
 * The bufs are queued by the strategy routine (cdstrategy)
 *
 * This routine is also called after other non-queued requests
 * have been made of the scsi driver, to ensure that the queue
 * continues to be drained.
 *
 * must be called at the correct (highish) spl level
 * cdstart() is called at SPLCD  from cdstrategy and scsi_done
 */
void 
cdstart(unit)
	u_int32 unit;
{
	register struct buf *bp = 0;
	register struct buf *dp;
	struct scsi_rw_big cmd;
	u_int32 blkno, nblk;
	struct partition *p;
	struct cd_data *cd = cd_driver.cd_data[unit];
	struct scsi_link *sc_link = cd->sc_link;

	SC_DEBUG(sc_link, SDEV_DB2, ("cdstart%d ", unit));
	/*
	 * See if there is a buf to do and we are not already
	 * doing one
	 */
	if (!sc_link->opennings) {
		return;		/* no room for us, unit already underway */
	}
	if (sc_link->flags & SDEV_WAITING) {	/* is room, but a special waits */
		return;		/* give the special that's waiting a chance to run */
	}
	dp = &cd->buf_queue;
	if ((bp = dp->b_actf) != NULL) {	/* yes, an assign */
		dp->b_actf = bp->av_forw;
	} else {
		return;
	}
	/*
	 * Should reject all queued entries if SDEV_MEDIA_LOADED is not true.
	 */
	if (!(sc_link->flags & SDEV_MEDIA_LOADED)) {
		goto bad;	/* no I/O.. media changed or something */
	}
	/*
	 * We have a buf, now we should make a command 
	 *
	 * First, translate the block to absolute and put it in terms of the
	 * logical blocksize of the device.  Really a bit silly until we have
	 * real partitions, but.
	 */
	blkno = bp->b_blkno / (cd->params.blksize / 512);
	if (PARTITION(bp->b_dev) != RAW_PART) {
		p = cd->disklabel.d_partitions + PARTITION(bp->b_dev);
		blkno += p->p_offset;
	}
	nblk = (bp->b_bcount + (cd->params.blksize - 1)) / (cd->params.blksize);
	/* what if something asks for 512 bytes not on a 2k boundary? *//*XXX */

	/*
	 *  Fill out the scsi command
	 */
	bzero(&cmd, sizeof(cmd));
	cmd.op_code = READ_BIG;
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
		CDRETRIES,
		30000,
		bp,
		SCSI_NOSLEEP | ((bp->b_flags & B_READ) ?
		    SCSI_DATA_IN : SCSI_DATA_OUT))
	    != SUCCESSFULLY_QUEUED) {
	      bad:
		printf("cd%d: oops not queued", unit);
		bp->b_error = EIO;
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return;
	}
	cdqueues++;
}

/*
 * Perform special action on behalf of the user.
 * Knows about the internals of this device
 */
errval 
cdioctl(dev_t dev, int cmd, caddr_t addr, int flag)
{
	errval  error = 0;
	u_int32 opri;
	u_int8  unit, part;
	register struct cd_data *cd;

	/*
	 * Find the device that the user is talking about
	 */
	unit = UNIT(dev);
	part = PARTITION(dev);
	cd = cd_driver.cd_data[unit];
	SC_DEBUG(cd->sc_link, SDEV_DB2, ("cdioctl 0x%x ", cmd));

	/*
	 * If the device is not valid.. abandon ship
	 */
	if (!(cd->sc_link->flags & SDEV_MEDIA_LOADED))
		return (EIO);
	switch (cmd) {

	case DIOCSBAD:
		error = EINVAL;
		break;

	case DIOCGDINFO:
		*(struct disklabel *) addr = cd->disklabel;
		break;

	case DIOCGPART:
		((struct partinfo *) addr)->disklab = &cd->disklabel;
		((struct partinfo *) addr)->part =
		    &cd->disklabel.d_partitions[PARTITION(dev)];
		break;

		/*
		 * a bit silly, but someone might want to test something on a 
		 * section of cdrom.
		 */
	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((flag & FWRITE) == 0)
			error = EBADF;
		else
			error = setdisklabel(&cd->disklabel,
			    (struct disklabel *) addr,
			    0,
			    0);
		if (error == 0)
			break;

	case DIOCWLABEL:
		error = EBADF;
		break;

	case CDIOCPLAYTRACKS:
		{
			struct ioc_play_track *args
			= (struct ioc_play_track *) addr;
			struct cd_mode_data data;
			if (error = cd_get_mode(unit, &data, AUDIO_PAGE))
				break;
			data.page.audio.flags &= ~CD_PA_SOTC;
			data.page.audio.flags |= CD_PA_IMMED;
			if (error = cd_set_mode(unit, &data))
				break;
			return (cd_play_tracks(unit
				,args->start_track
				,args->start_index
				,args->end_track
				,args->end_index
			    ));
		}
		break;
	case CDIOCPLAYMSF:
		{
			struct ioc_play_msf *args
			= (struct ioc_play_msf *) addr;
			struct cd_mode_data data;
			if (error = cd_get_mode(unit, &data, AUDIO_PAGE))
				break;
			data.page.audio.flags &= ~CD_PA_SOTC;
			data.page.audio.flags |= CD_PA_IMMED;
			if (error = cd_set_mode(unit, &data))
				break;
			return (cd_play_msf(unit
				,args->start_m
				,args->start_s
				,args->start_f
				,args->end_m
				,args->end_s
				,args->end_f
			    ));
		}
		break;
	case CDIOCPLAYBLOCKS:
		{
			struct ioc_play_blocks *args
			= (struct ioc_play_blocks *) addr;
			struct cd_mode_data data;
			if (error = cd_get_mode(unit, &data, AUDIO_PAGE))
				break;
			data.page.audio.flags &= ~CD_PA_SOTC;
			data.page.audio.flags |= CD_PA_IMMED;
			if (error = cd_set_mode(unit, &data))
				break;
			return (cd_play(unit, args->blk, args->len));

		}
		break;
	case CDIOCREADSUBCHANNEL:
		{
			struct ioc_read_subchannel *args
			= (struct ioc_read_subchannel *) addr;
			struct cd_sub_channel_info data;
			u_int32 len = args->data_len;
			if (len > sizeof(data) ||
			    len < sizeof(struct cd_sub_channel_header)) {
				error = EINVAL;
				break;
			}
			if (error = cd_read_subchannel(unit, args->address_format,
				args->data_format, args->track, &data, len)) {
				break;
			}
			len = MIN(len, ((data.header.data_len[0] << 8) + data.header.data_len[1] +
				sizeof(struct cd_sub_channel_header)));
			if (copyout(&data, args->data, len) != 0) {
				error = EFAULT;
			}
		}
		break;
	case CDIOREADTOCHEADER:
		{		/* ??? useless bcopy? XXX */
			struct ioc_toc_header th;
			if (error = cd_read_toc(unit, 0, 0, 
						(struct cd_toc_entry *)&th,
						sizeof th))
				break;
			th.len = (th.len & 0xff) << 8 + ((th.len >> 8) & 0xff);
			bcopy(&th, addr, sizeof th);
		}
		break;
	case CDIOREADTOCENTRYS:
		{
			struct cd_toc {
				struct ioc_toc_header header;
				struct cd_toc_entry entries[65];
			} data;
			struct ioc_read_toc_entry *te =
			(struct ioc_read_toc_entry *) addr;
			struct ioc_toc_header *th;
			u_int32 len = te->data_len;
			th = &data.header;

			if (len > sizeof(data.entries) || len < sizeof(struct cd_toc_entry)) {
				error = EINVAL;
				break;
			}
			if (error = cd_read_toc(unit, te->address_format,
				te->starting_track,
				(struct cd_toc_entry *)&data,
				len + sizeof(struct ioc_toc_header)))
				break;
			len = MIN(len, ((((th->len & 0xff) << 8) + ((th->len >> 8))) - (sizeof(th->starting_track) + sizeof(th->ending_track))));
			if (copyout(data.entries, te->data, len) != 0) {
				error = EFAULT;
			}
		}
		break;
	case CDIOCSETPATCH:
		{
			struct ioc_patch *arg = (struct ioc_patch *) addr;
			struct cd_mode_data data;
			if (error = cd_get_mode(unit, &data, AUDIO_PAGE))
				break;
			data.page.audio.port[LEFT_PORT].channels = arg->patch[0];
			data.page.audio.port[RIGHT_PORT].channels = arg->patch[1];
			data.page.audio.port[2].channels = arg->patch[2];
			data.page.audio.port[3].channels = arg->patch[3];
			if (error = cd_set_mode(unit, &data))
				break; /* eh? */
		}
		break;
	case CDIOCGETVOL:
		{
			struct ioc_vol *arg = (struct ioc_vol *) addr;
			struct cd_mode_data data;
			if (error = cd_get_mode(unit, &data, AUDIO_PAGE))
				break;
			arg->vol[LEFT_PORT] = data.page.audio.port[LEFT_PORT].volume;
			arg->vol[RIGHT_PORT] = data.page.audio.port[RIGHT_PORT].volume;
			arg->vol[2] = data.page.audio.port[2].volume;
			arg->vol[3] = data.page.audio.port[3].volume;
		}
		break;
	case CDIOCSETVOL:
		{
			struct ioc_vol *arg = (struct ioc_vol *) addr;
			struct cd_mode_data data;
			if (error = cd_get_mode(unit, &data, AUDIO_PAGE))
				break;
			data.page.audio.port[LEFT_PORT].channels = CHANNEL_0;
			data.page.audio.port[LEFT_PORT].volume = arg->vol[LEFT_PORT];
			data.page.audio.port[RIGHT_PORT].channels = CHANNEL_1;
			data.page.audio.port[RIGHT_PORT].volume = arg->vol[RIGHT_PORT];
			data.page.audio.port[2].volume = arg->vol[2];
			data.page.audio.port[3].volume = arg->vol[3];
			if (error = cd_set_mode(unit, &data))
				break;
		}
		break;
	case CDIOCSETMONO:
		{
			struct ioc_vol *arg = (struct ioc_vol *) addr;
			struct cd_mode_data data;
			if (error = cd_get_mode(unit, &data, AUDIO_PAGE))
				break;
			data.page.audio.port[LEFT_PORT].channels = LEFT_CHANNEL | RIGHT_CHANNEL | 4 | 8;
			data.page.audio.port[RIGHT_PORT].channels = LEFT_CHANNEL | RIGHT_CHANNEL;
			data.page.audio.port[2].channels = 0;
			data.page.audio.port[3].channels = 0;
			if (error = cd_set_mode(unit, &data))
				break;
		}
		break;
	case CDIOCSETSTERIO:
		{
			struct ioc_vol *arg = (struct ioc_vol *) addr;
			struct cd_mode_data data;
			if (error = cd_get_mode(unit, &data, AUDIO_PAGE))
				break;
			data.page.audio.port[LEFT_PORT].channels = LEFT_CHANNEL;
			data.page.audio.port[RIGHT_PORT].channels = RIGHT_CHANNEL;
			data.page.audio.port[2].channels = 0;
			data.page.audio.port[3].channels = 0;
			if (error = cd_set_mode(unit, &data))
				break;
		}
		break;
	case CDIOCSETMUTE:
		{
			struct ioc_vol *arg = (struct ioc_vol *) addr;
			struct cd_mode_data data;
			if (error = cd_get_mode(unit, &data, AUDIO_PAGE))
				break;
			data.page.audio.port[LEFT_PORT].channels = 0;
			data.page.audio.port[RIGHT_PORT].channels = 0;
			data.page.audio.port[2].channels = 0;
			data.page.audio.port[3].channels = 0;
			if (error = cd_set_mode(unit, &data))
				break;
		}
		break;
	case CDIOCSETLEFT:
		{
			struct ioc_vol *arg = (struct ioc_vol *) addr;
			struct cd_mode_data data;
			if (error = cd_get_mode(unit, &data, AUDIO_PAGE))
				break;
			data.page.audio.port[LEFT_PORT].channels = LEFT_CHANNEL;
			data.page.audio.port[RIGHT_PORT].channels = LEFT_CHANNEL;
			data.page.audio.port[2].channels = 0;
			data.page.audio.port[3].channels = 0;
			if (error = cd_set_mode(unit, &data))
				break;
		}
		break;
	case CDIOCSETRIGHT:
		{
			struct ioc_vol *arg = (struct ioc_vol *) addr;
			struct cd_mode_data data;
			if (error = cd_get_mode(unit, &data, AUDIO_PAGE))
				break;
			data.page.audio.port[LEFT_PORT].channels = RIGHT_CHANNEL;
			data.page.audio.port[RIGHT_PORT].channels = RIGHT_CHANNEL;
			data.page.audio.port[2].channels = 0;
			data.page.audio.port[3].channels = 0;
			if (error = cd_set_mode(unit, &data))
				break;
		}
		break;
	case CDIOCRESUME:
		error = cd_pause(unit, 1);
		break;
	case CDIOCPAUSE:
		error = cd_pause(unit, 0);
		break;
	case CDIOCSTART:
		error = scsi_start_unit(cd->sc_link, 0);
		break;
	case CDIOCSTOP:
		error = scsi_stop_unit(cd->sc_link, 0, 0);
		break;
	case CDIOCEJECT:
		error = scsi_stop_unit(cd->sc_link, 1, 0);
		break;
	case CDIOCALLOW:
		error = scsi_prevent(cd->sc_link, PR_ALLOW, 0);
		break;
	case CDIOCPREVENT:
		error = scsi_prevent(cd->sc_link, PR_PREVENT, 0);
		break;
	case CDIOCSETDEBUG:
		cd->sc_link->flags |= (SDEV_DB1 | SDEV_DB2);
		break;
	case CDIOCCLRDEBUG:
		cd->sc_link->flags &= ~(SDEV_DB1 | SDEV_DB2);
		break;
	case CDIOCRESET:
		return (cd_reset(unit));
		break;
	default:
		if(part == RAW_PART)
			error = scsi_do_ioctl(cd->sc_link,cmd,addr,flag);
		else
			error = ENOTTY;
		break;
	}
	return (error);
}

/*
 * Load the label information on the named device
 * Actually fabricate a disklabel
 * 
 * EVENTUALLY take information about different
 * data tracks from the TOC and put it in the disklabel
 */
errval 
cdgetdisklabel(unit)
	u_int8  unit;
{
	/*unsigned int n, m; */
	char   *errstring;
	struct cd_data *cd;

	cd = cd_driver.cd_data[unit];

	bzero(&cd->disklabel, sizeof(struct disklabel));
	/*
	 * make partition 0 the whole disk
	 */
	strncpy(cd->disklabel.d_typename, "scsi cd_rom", 16);
	strncpy(cd->disklabel.d_packname, "ficticious", 16);
	cd->disklabel.d_secsize = cd->params.blksize;	/* as long as it's not 0 */
	cd->disklabel.d_nsectors = 100;
	cd->disklabel.d_ntracks = 1;
	cd->disklabel.d_ncylinders = (cd->params.disksize / 100) + 1;
	cd->disklabel.d_secpercyl = 100;
	cd->disklabel.d_secperunit = cd->params.disksize;
	cd->disklabel.d_rpm = 300;
	cd->disklabel.d_interleave = 1;
	cd->disklabel.d_flags = D_REMOVABLE;

	/*
	 * remember that comparisons with the partition are done
	 * assuming the blocks are 512 bytes so fudge it.
	 */
	cd->disklabel.d_npartitions = 1;
	cd->disklabel.d_partitions[0].p_offset = 0;
	cd->disklabel.d_partitions[0].p_size
	    = cd->params.disksize * (cd->params.blksize / 512);
	cd->disklabel.d_partitions[0].p_fstype = 9;

	cd->disklabel.d_magic = DISKMAGIC;
	cd->disklabel.d_magic2 = DISKMAGIC;
	cd->disklabel.d_checksum = dkcksum(&(cd->disklabel));

	/*
	 * Signal to other users and routines that we now have a
	 * disklabel that represents the media (maybe)
	 */
	return (ESUCCESS);
}

/*
 * Find out from the device what it's capacity is
 */
u_int32 
cd_size(unit, flags)
	int unit;
	int flags;
{
	struct scsi_read_cd_cap_data rdcap;
	struct scsi_read_cd_capacity scsi_cmd;
	u_int32 size;
	u_int32 blksize;
	struct cd_data *cd = cd_driver.cd_data[unit];

	/*
	 * make up a scsi command and ask the scsi driver to do
	 * it for you.
	 */
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = READ_CD_CAPACITY;

	/*
	 * If the command works, interpret the result as a 4 byte
	 * number of blocks and a blocksize
	 */
	if (scsi_scsi_cmd(cd->sc_link,
		(struct scsi_generic *) &scsi_cmd,
		sizeof(scsi_cmd),
		(u_char *) & rdcap,
		sizeof(rdcap),
		CDRETRIES,
		20000,		/* might be a disk-changer */
		NULL,
		SCSI_DATA_IN | flags) != 0) {
		printf("cd%d: could not get size\n", unit);
		return (0);
	} else {
		size = rdcap.addr_0 + 1;
		size += rdcap.addr_1 << 8;
		size += rdcap.addr_2 << 16;
		size += rdcap.addr_3 << 24;
		blksize = rdcap.length_0;
		blksize += rdcap.length_1 << 8;
		blksize += rdcap.length_2 << 16;
		blksize += rdcap.length_3 << 24;
	}
	if (blksize < 512)
		blksize = 2048;	/* some drives lie ! */
	if (size < 100)
		size = 400000;	/* ditto */
	SC_DEBUG(cd->sc_link, SDEV_DB3, ("cd%d: %d %d byte blocks\n"
		,unit, size, blksize));
	cd->params.disksize = size;
	cd->params.blksize = blksize;
	return (size);
}

/*
 * Get the requested page into the buffer given
 */
static errval 
cd_get_mode(unit, data, page)
	u_int32 unit;
	struct cd_mode_data *data;
	u_int32 page;
{
	struct scsi_mode_sense scsi_cmd;
	errval  retval;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	bzero(data, sizeof(*data));
	scsi_cmd.op_code = MODE_SENSE;
	scsi_cmd.page = page;
	scsi_cmd.length = sizeof(*data) & 0xff;
	retval = scsi_scsi_cmd(cd_driver.cd_data[unit]->sc_link,
	    (struct scsi_generic *) &scsi_cmd,
	    sizeof(scsi_cmd),
	    (u_char *) data,
	    sizeof(*data),
	    CDRETRIES,
	    20000,		/* should be immed */
	    NULL,
	    SCSI_DATA_IN);
	return (retval);
}

/*
 * Get the requested page into the buffer given
 */
errval 
cd_set_mode(unit, data)
	u_int32 unit;
	struct cd_mode_data *data;
{
	struct scsi_mode_select scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = MODE_SELECT;
	scsi_cmd.byte2 |= SMS_PF;
	scsi_cmd.length = sizeof(*data) & 0xff;
	data->header.data_length = 0;
	return (scsi_scsi_cmd(cd_driver.cd_data[unit]->sc_link,
		(struct scsi_generic *) &scsi_cmd,
		sizeof(scsi_cmd),
		(u_char *) data,
		sizeof(*data),
		CDRETRIES,
		20000,		/* should be immed */
		NULL,
		SCSI_DATA_OUT));
}

/*
 * Get scsi driver to send a "start playing" command
 */
errval 
cd_play(unit, blk, len)
	u_int32 unit, blk, len;
{
	struct scsi_play scsi_cmd;
	errval  retval;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = PLAY;
	scsi_cmd.blk_addr[0] = (blk >> 24) & 0xff;
	scsi_cmd.blk_addr[1] = (blk >> 16) & 0xff;
	scsi_cmd.blk_addr[2] = (blk >> 8) & 0xff;
	scsi_cmd.blk_addr[3] = blk & 0xff;
	scsi_cmd.xfer_len[0] = (len >> 8) & 0xff;
	scsi_cmd.xfer_len[1] = len & 0xff;
	return (scsi_scsi_cmd(cd_driver.cd_data[unit]->sc_link,
		(struct scsi_generic *) &scsi_cmd,
		sizeof(scsi_cmd),
		0,
		0,
		CDRETRIES,
		200000,		/* should be immed */
		NULL,
		0));
}

/*
 * Get scsi driver to send a "start playing" command
 */
errval 
cd_play_big(unit, blk, len)
	u_int32 unit, blk, len;
{
	struct scsi_play_big scsi_cmd;
	errval  retval;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = PLAY_BIG;
	scsi_cmd.blk_addr[0] = (blk >> 24) & 0xff;
	scsi_cmd.blk_addr[1] = (blk >> 16) & 0xff;
	scsi_cmd.blk_addr[2] = (blk >> 8) & 0xff;
	scsi_cmd.blk_addr[3] = blk & 0xff;
	scsi_cmd.xfer_len[0] = (len >> 24) & 0xff;
	scsi_cmd.xfer_len[1] = (len >> 16) & 0xff;
	scsi_cmd.xfer_len[2] = (len >> 8) & 0xff;
	scsi_cmd.xfer_len[3] = len & 0xff;
	return (scsi_scsi_cmd(cd_driver.cd_data[unit]->sc_link,
		(struct scsi_generic *) &scsi_cmd,
		sizeof(scsi_cmd),
		0,
		0,
		CDRETRIES,
		20000,		/* should be immed */
		NULL,
		0));
}

/*
 * Get scsi driver to send a "start playing" command
 */
errval 
cd_play_tracks(unit, strack, sindex, etrack, eindex)
	u_int32 unit, strack, sindex, etrack, eindex;
{
	struct scsi_play_track scsi_cmd;
	errval  retval;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = PLAY_TRACK;
	scsi_cmd.start_track = strack;
	scsi_cmd.start_index = sindex;
	scsi_cmd.end_track = etrack;
	scsi_cmd.end_index = eindex;
	return (scsi_scsi_cmd(cd_driver.cd_data[unit]->sc_link,
		(struct scsi_generic *) &scsi_cmd,
		sizeof(scsi_cmd),
		0,
		0,
		CDRETRIES,
		20000,		/* should be immed */
		NULL,
		0));
}

/*
 * Get scsi driver to send a "play msf" command
 */
errval 
cd_play_msf(unit, startm, starts, startf, endm, ends, endf)
	u_int32 unit, startm, starts, startf, endm, ends, endf;
{
	struct scsi_play_msf scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = PLAY_MSF;
	scsi_cmd.start_m = startm;
	scsi_cmd.start_s = starts;
	scsi_cmd.start_f = startf;
	scsi_cmd.end_m = endm;
	scsi_cmd.end_s = ends;
	scsi_cmd.end_f = endf;

	return (scsi_scsi_cmd(cd_driver.cd_data[unit]->sc_link,
		(struct scsi_generic *) &scsi_cmd,
		sizeof(scsi_cmd),
		0,
		0,
		CDRETRIES,
		2000,
		NULL,
		0));
}

/*
 * Get scsi driver to send a "start up" command
 */
errval 
cd_pause(unit, go)
	u_int32 unit, go;
{
	struct scsi_pause scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = PAUSE;
	scsi_cmd.resume = go;

	return (scsi_scsi_cmd(cd_driver.cd_data[unit]->sc_link,
		(struct scsi_generic *) &scsi_cmd,
		sizeof(scsi_cmd),
		0,
		0,
		CDRETRIES,
		2000,
		NULL,
		0));
}

/*
 * Get scsi driver to send a "RESET" command
 */
errval 
cd_reset(unit)
	u_int32 unit;
{
	return (scsi_scsi_cmd(cd_driver.cd_data[unit]->sc_link,
		0,
		0,
		0,
		0,
		CDRETRIES,
		2000,
		NULL,
		SCSI_RESET));
}

/*
 * Read subchannel
 */
errval 
cd_read_subchannel(unit, mode, format, track, data, len)
	u_int32 unit, mode, format;
	int track;
	struct cd_sub_channel_info *data;
	u_int32 len;
{
	struct scsi_read_subchannel scsi_cmd;
	errval  error;

	bzero(&scsi_cmd, sizeof(scsi_cmd));

	scsi_cmd.op_code = READ_SUBCHANNEL;
	if (mode == CD_MSF_FORMAT)
		scsi_cmd.byte2 |= CD_MSF;
	scsi_cmd.byte3 = SRS_SUBQ;
	scsi_cmd.subchan_format = format;
	scsi_cmd.track = track;
	scsi_cmd.data_len[0] = (len) >> 8;
	scsi_cmd.data_len[1] = (len) & 0xff;
	return (scsi_scsi_cmd(cd_driver.cd_data[unit]->sc_link,
		(struct scsi_generic *) &scsi_cmd,
		sizeof(struct scsi_read_subchannel),
		        (u_char *) data,
		len,
		CDRETRIES,
		5000,
		NULL,
		SCSI_DATA_IN));
}

/*
 * Read table of contents
 */
static errval 
cd_read_toc(unit, mode, start, data, len)
	u_int32 unit, mode, start;
	struct cd_toc_entry *data;
	u_int32 len;
{
	struct scsi_read_toc scsi_cmd;
	errval  error;
	u_int32 ntoc;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	/*if(len!=sizeof(struct ioc_toc_header))
	 * ntoc=((len)-sizeof(struct ioc_toc_header))/sizeof(struct cd_toc_entry);
	 * else */
	ntoc = len;

	scsi_cmd.op_code = READ_TOC;
	if (mode == CD_MSF_FORMAT)
		scsi_cmd.byte2 |= CD_MSF;
	scsi_cmd.from_track = start;
	scsi_cmd.data_len[0] = (ntoc) >> 8;
	scsi_cmd.data_len[1] = (ntoc) & 0xff;
	return (scsi_scsi_cmd(cd_driver.cd_data[unit]->sc_link,
		(struct scsi_generic *) &scsi_cmd,
		sizeof(struct scsi_read_toc),
		        (u_char *) data,
		len,
		CDRETRIES,
		5000,
		NULL,
		SCSI_DATA_IN));
}

#define b2tol(a)	(((unsigned)(a##_1) << 8) + (unsigned)a##_0 )

/*
 * Get the scsi driver to send a full inquiry to the device and use the
 * results to fill out the disk parameter structure.
 */
static errval 
cd_get_parms(unit, flags)
	int unit;
	int flags;
{
	struct cd_data *cd = cd_driver.cd_data[unit];

	/*
	 * First check if we have it all loaded
	 */
	if (cd->sc_link->flags & SDEV_MEDIA_LOADED)
		return (0);
	/*
	 * give a number of sectors so that sec * trks * cyls
	 * is <= disk_size 
	 */
	if (cd_size(unit, flags)) {
		cd->sc_link->flags |= SDEV_MEDIA_LOADED;
		return (0);
	} else {
		return (ENXIO);
	}
}

int
cdsize(dev_t dev)
{
	return (-1);
}
