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
 *      $Id: cd.c,v 1.57 1996/01/30 12:59:00 ache Exp $
 */

#include "opt_bounce.h"

#define SPLCD splbio
#define ESUCCESS 0
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
#include <sys/devconf.h>
#include <sys/dkstat.h>
#include <sys/kernel.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/

#include <scsi/scsi_all.h>
#include <scsi/scsi_cd.h>
#include <scsi/scsi_disk.h>	/* rw_big and start_stop come from there */
#include <scsi/scsiconf.h>

static errval cd_get_parms __P((int, int));
static u_int32 cd_size __P((int unit, int flags));
static errval cd_get_mode __P((u_int32, struct cd_mode_data *, u_int32));
static errval cd_set_mode __P((u_int32 unit, struct cd_mode_data *));
static errval cd_read_toc __P((u_int32, u_int32, u_int32, struct cd_toc_entry *,
			  u_int32));

static errval cd_pause __P((u_int32, u_int32));
static errval cd_reset __P((u_int32));
static errval cd_play_msf __P((u_int32, u_int32, u_int32, u_int32, u_int32, u_int32, u_int32));
static errval cd_play __P((u_int32, u_int32, u_int32));
static errval cd_play_big __P((u_int32 unit, u_int32 blk, u_int32 len));
static errval cd_play_tracks __P((u_int32, u_int32, u_int32, u_int32, u_int32));
static errval cd_read_subchannel __P((u_int32, u_int32, u_int32, int, struct cd_sub_channel_info *, u_int32));
static errval cd_getdisklabel __P((u_int8));

static	d_open_t	cdopen;
static	d_close_t	cdclose;
static	d_ioctl_t	cdioctl;
static	d_strategy_t	cdstrategy;

#define CDEV_MAJOR 15
#define BDEV_MAJOR 6
extern	struct	cdevsw	cd_cdevsw;
static struct bdevsw cd_bdevsw = 
	{ cdopen,	cdclose,	cdstrategy,	cdioctl,	/*6*/
	  nodump,	nopsize,	0,	"cd",	&cd_cdevsw,	-1 };

static struct cdevsw cd_cdevsw = 
	{ cdopen,	cdclose,	rawread,	nowrite,	/*15*/
	  cdioctl,	nostop,		nullreset,	nodevtotty,/* cd */
	  seltrue,	nommap,		cdstrategy,	"cd",
	  &cd_bdevsw,	-1 };


static int32   cdstrats, cdqueues;

#define CDUNIT(DEV)      ((minor(DEV)&0xF8) >> 3)    /* 5 bit unit */
#define CDSETUNIT(DEV, U) makedev(major(DEV), ((U) << 3))

#define PAGESIZ 	4096
#define SECSIZE 2048	/* XXX */	/* default only */
#define	CDOUTSTANDING	2
#define	CDRETRIES	1

#define PARTITION(z)	(minor(z) & 0x07)
#define RAW_PART        2

static void	cdstart(u_int32 unit, u_int32 flags);

struct scsi_data {
	u_int32 flags;
#define	CDINIT		0x04	/* device has been init'd */
	struct cd_parms {
		u_int32 blksize;
		u_long  disksize;	/* total number sectors */
	} params;
	struct disklabel disklabel;
	u_int32 partflags[MAXPARTITIONS];	/* per partition flags */
#define CDOPEN	0x01
	u_int32 openparts;	/* one bit for each open partition */
	u_int32 xfer_block_wait;
	struct buf_queue_head buf_queue;
	int dkunit;
#ifdef	DEVFS
	void	*ra_devfs_token;
	void	*rc_devfs_token;
	void	*a_devfs_token;
	void	*c_devfs_token;
#endif
};

static int cdunit(dev_t dev) { return CDUNIT(dev); }
static dev_t cdsetunit(dev_t dev, int unit) { return CDSETUNIT(dev, unit); }

static errval cd_open(dev_t dev, int flags, int fmt, struct proc *p,
		struct scsi_link *sc_link);
static errval cd_ioctl(dev_t dev, int cmd, caddr_t addr, int flag,
		struct proc *p, struct scsi_link *sc_link);
static errval cd_close(dev_t dev, int flag, int fmt, struct proc *p,
		struct scsi_link *sc_link);
static void cd_strategy(struct buf *bp, struct scsi_link *sc_link);

SCSI_DEVICE_ENTRIES(cd)

static struct scsi_device cd_switch =
{
	NULL,			/* use default error handler */
	cdstart,		/* we have a queue, which is started by this */
	NULL,			/* we do not have an async handler */
	NULL,			/* use default 'done' routine */
	"cd",			/* we are to be refered to by this name */
	0,			/* no device specific flags */
	{0, 0},
	0,			/* Link flags */
	cdattach,
	"CD-ROM",
	cdopen,
	sizeof(struct scsi_data),
	T_READONLY,
	cdunit,
	cdsetunit,
	cd_open,
	cd_ioctl,
	cd_close,
	cd_strategy,
};

#define CD_STOP		0
#define CD_START	1
#define CD_EJECT	-2

static int
cd_externalize(struct kern_devconf *kdc, struct sysctl_req *req)
{
	return scsi_externalize(SCSI_LINK(&cd_switch, kdc->kdc_unit), req);
}

static struct kern_devconf kdc_cd_template = {
	0, 0, 0,		/* filled in by dev_attach */
	"cd", 0, MDDC_SCSI,
	cd_externalize, 0, scsi_goaway, SCSI_EXTERNALLEN,
	&kdc_scbus0,		/* parent - XXX should be host adapter*/
	0,			/* parentdata */
	DC_UNKNOWN,		/* not supported */
};

static inline void
cd_registerdev(int unit)
{
	struct kern_devconf *kdc;

	MALLOC(kdc, struct kern_devconf *, sizeof *kdc, M_TEMP, M_NOWAIT);
	if(!kdc) return;
	*kdc = kdc_cd_template;
	kdc->kdc_unit = unit;
	kdc->kdc_description = cd_switch.desc;
	/* XXX should set parentdata */
	dev_attach(kdc);
	if(dk_ndrive < DK_NDRIVE) {
		sprintf(dk_names[dk_ndrive], "cd%d", unit);
		dk_wpms[dk_ndrive] = (150*1024/2);
		SCSI_DATA(&cd_switch, unit)->dkunit = dk_ndrive++;
	} else {
		SCSI_DATA(&cd_switch, unit)->dkunit = -1;
	}
}


/*
 * The routine called by the low level scsi routine when it discovers
 * A device suitable for this driver
 */
static int
cdattach(struct scsi_link *sc_link)
{
	u_int32 unit;
	struct cd_parms *dp;
	struct scsi_data *cd = sc_link->sd;
	char	name[32];

	unit = sc_link->dev_unit;
	dp = &(cd->params);

	TAILQ_INIT(&cd->buf_queue);
	if (sc_link->opennings > CDOUTSTANDING)
		sc_link->opennings = CDOUTSTANDING;
	/*
	 * Use the subdriver to request information regarding
	 * the drive. We cannot use interrupts yet, so the
	 * request must specify this.
	 *
	 * XXX dufault@hda.com:
	 * Need to handle this better in the case of no record.  Rather than
	 * a state driven sense handler I think we should make it so that
	 * the command can get the sense back so that it can selectively log
	 * errors.
	 */
	if (sc_link->quirks & CD_Q_NO_TOUCH) {
		dp->disksize = 0;
	} else {
		cd_get_parms(unit, SCSI_NOSLEEP | SCSI_NOMASK);
	}
	if (dp->disksize) {
		printf("cd present [%ld x %ld byte records]",
		    cd->params.disksize,
		    cd->params.blksize);
	} else {
		printf("can't get the size");
	}

	cd->flags |= CDINIT;
	cd_registerdev(unit);
#ifdef DEVFS
#define CD_UID 0
#define CD_GID 13
	sprintf(name, "rcd%da",unit);
	cd->ra_devfs_token = devfs_add_devsw(
		"/", name, &cd_cdevsw, unit * 8,
		DV_CHR,	CD_UID,  CD_GID, 0660);

	sprintf(name, "rcd%dc",unit);
	cd->rc_devfs_token = devfs_add_devsw(
		"/", name, &cd_cdevsw, (unit * 8 ) + RAW_PART,
		DV_CHR,	CD_UID,  CD_GID, 0600);

	sprintf(name, "cd%da",unit);
	cd->a_devfs_token = devfs_add_devsw(
		"/", name, &cd_bdevsw, (unit * 8 ) + 0,
		DV_BLK,	CD_UID,  CD_GID, 0660);

	sprintf(name, "cd%dc",unit);
	cd->c_devfs_token = devfs_add_devsw(
		"/", name, &cd_bdevsw, (unit * 8 ) + RAW_PART,
		DV_BLK,	CD_UID,  CD_GID, 0600);
#endif

	return 0;
}

/*
 * open the device. Make sure the partition info is a up-to-date as can be.
 */
static errval
cd_open(dev_t dev, int flags, int fmt, struct proc *p,
	struct scsi_link *sc_link)
{
	errval  errcode = 0;
	u_int32 unit, part;
	struct scsi_data *cd;

	unit = CDUNIT(dev);
	part = PARTITION(dev);

	cd = sc_link->sd;
	/*
	 * Make sure the device has been initialised
	 */
	if ((cd == NULL) || (!(cd->flags & CDINIT)))
		return (ENXIO);

	SC_DEBUG(sc_link, SDEV_DB1,
	    ("cd_open: dev=0x%lx (unit %ld,partition %ld)\n",
		dev, unit, part));
	/*
	 * Check that it is still responding and ok.
	 * if the media has been changed this will result in a
	 * "unit attention" error which the error code will
	 * disregard because the SDEV_OPEN flag is not yet set.
	 * Makes sure that we know it if the media has been changed..
	 */
	scsi_test_unit_ready(sc_link, SCSI_SILENT);

	/*
	 * If it's been invalidated, and not everybody has closed it then
	 * forbid re-entry.  (may have changed media)
	 */
	if ((!(sc_link->flags & SDEV_MEDIA_LOADED))
	    && (cd->openparts))
		return (ENXIO);

	/*
	 * This time actually take notice of error returns
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
	 * failure here is ignored.
	 */
	scsi_start_unit(sc_link, CD_START);
	scsi_prevent(sc_link, PR_PREVENT, SCSI_SILENT);
	SC_DEBUG(sc_link, SDEV_DB3, ("'start' attempted "));
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
	cd_getdisklabel(unit);
	SC_DEBUG(sc_link, SDEV_DB3, ("Disklabel fabricated "));
	/*
	 * Check the partition is legal
	 */
	if(part != RAW_PART) {
		/*
	 	 *  Check that the partition CAN exist
	 	 */
		if (part >= cd->disklabel.d_npartitions) {
			SC_DEBUG(sc_link, SDEV_DB3, ("partition %ld > %d\n", part
				,cd->disklabel.d_npartitions));
			errcode = ENXIO;
			goto bad;
		}
		/*
	 	 *  and that it DOES exist
	 	 */
		if (cd->disklabel.d_partitions[part].p_fstype == FS_UNUSED) {
			SC_DEBUG(sc_link, SDEV_DB3,
					("part %ld type UNUSED\n", part));
			errcode = ENXIO;
			goto bad;
		}
	}
	cd->partflags[part] |= CDOPEN;
	cd->openparts |= (1 << part);
	SC_DEBUG(sc_link, SDEV_DB3, ("open complete\n"));
	sc_link->flags |= SDEV_MEDIA_LOADED;
	return 0;
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
static errval
cd_close(dev_t dev, int flag, int fmt, struct proc *p,
	 struct scsi_link *sc_link)
{
	u_int8  unit, part;
	struct scsi_data *cd;

	unit = CDUNIT(dev);
	part = PARTITION(dev);
	cd = sc_link->sd;

	SC_DEBUG(sc_link, SDEV_DB2, ("cd%d: closing part %d\n", unit, part));
	cd->partflags[part] &= ~CDOPEN;
	cd->openparts &= ~(1 << part);

	/*
	 * If we were the last open of the entire device, release it.
	 */
	if (!(cd->openparts)) {
		scsi_prevent(sc_link, PR_ALLOW, SCSI_SILENT);
		sc_link->flags &= ~SDEV_OPEN;
	}
	return (0);
}


/*
 * Actually translate the requested transfer into one the physical driver can
 * understand.  The transfer is described by a buf and will include only one
 * physical transfer.
 */
static void
cd_strategy(struct buf *bp, struct scsi_link *sc_link)
{
	u_int32 opri;
	u_int32 unit = CDUNIT((bp->b_dev));
	struct scsi_data *cd = sc_link->sd;

	cdstrats++;
	/*
	 * If the device has been made invalid, error out
	 * maybe the media changed
	 */
	if (!(sc_link->flags & SDEV_MEDIA_LOADED)) {
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
	TAILQ_INSERT_TAIL(&cd->buf_queue, bp, b_act);

	/*
	 * Tell the device to get going on the transfer if it's
	 * not doing anything, otherwise just wait for completion
	 */
	cdstart(unit, 0);

	splx(opri);
	return;
      bad:
	bp->b_flags |= B_ERROR;
      done:

	/*
	 * Correctly set the buf to indicate a completed xfer
	 */
	bp->b_resid = bp->b_bcount;
	biodone(bp);
	return;
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
static void
cdstart(unit, flags)
	u_int32 unit;
	u_int32 flags;
{
	register struct buf *bp = 0;
	struct scsi_rw_big cmd;
	u_int32 blkno, nblk;
	struct partition *p;
	struct scsi_link *sc_link = SCSI_LINK(&cd_switch, unit);
	struct scsi_data *cd = sc_link->sd;

	SC_DEBUG(sc_link, SDEV_DB2, ("cdstart%ld ", unit));
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

	bp = cd->buf_queue.tqh_first;
	if (bp == NULL) {	/* yes, an assign */
		return;
	}
	TAILQ_REMOVE( &cd->buf_queue, bp, b_act);

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
		flags | ((bp->b_flags & B_READ) ?
		    SCSI_DATA_IN : SCSI_DATA_OUT))
	    != SUCCESSFULLY_QUEUED) {
	      bad:
		printf("cd%ld: oops not queued\n", unit);
		bp->b_error = EIO;
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return;
	}
	cdqueues++;
	if(cd->dkunit >= 0) {
		dk_xfer[cd->dkunit]++;
		dk_seek[cd->dkunit]++; /* don't know */
		dk_wds[cd->dkunit] += bp->b_bcount >> 6;
	}
}

/*
 * Perform special action on behalf of the user.
 * Knows about the internals of this device
 */
static errval
cd_ioctl(dev_t dev, int cmd, caddr_t addr, int flag, struct proc *p,
	 struct scsi_link *sc_link)
{
	errval  error = 0;
	u_int8  unit, part;
	register struct scsi_data *cd;

	/*
	 * Find the device that the user is talking about
	 */
	unit = CDUNIT(dev);
	part = PARTITION(dev);
	cd = sc_link->sd;
	SC_DEBUG(sc_link, SDEV_DB2, ("cdioctl 0x%x ", cmd));

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
			    (struct disklabel *) addr, 0);
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
			error = cd_get_mode(unit, &data, AUDIO_PAGE);
			if (error)
				break;
			data.page.audio.flags &= ~CD_PA_SOTC;
			data.page.audio.flags |= CD_PA_IMMED;
			error = cd_set_mode(unit, &data);
			if (error)
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
			error = cd_get_mode(unit, &data, AUDIO_PAGE);
			if (error)
				break;
			data.page.audio.flags &= ~CD_PA_SOTC;
			data.page.audio.flags |= CD_PA_IMMED;
			error = cd_set_mode(unit, &data);
			if (error)
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
			error = cd_get_mode(unit, &data, AUDIO_PAGE);
			if (error)
				break;
			data.page.audio.flags &= ~CD_PA_SOTC;
			data.page.audio.flags |= CD_PA_IMMED;
			error = cd_set_mode(unit, &data);
			if (error)
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
			error = cd_read_subchannel(unit, args->address_format,
				args->data_format, args->track, &data, len);
			if (error)
				break;
			len = min(len, ((data.header.data_len[0] << 8) +
				data.header.data_len[1] +
				sizeof(struct cd_sub_channel_header)));
			if (copyout(&data, args->data, len) != 0) {
				error = EFAULT;
			}
		}
		break;
	case CDIOREADTOCHEADER:
		{
			struct ioc_toc_header th;
			error = cd_read_toc(unit, 0, 0,
					(struct cd_toc_entry *)&th, sizeof th);
			if (error)
				break;
			th.len = ((th.len & 0xff) << 8) + ((th.len >> 8) & 0xff);
			bcopy(&th, addr, sizeof th);
		}
		break;
	case CDIOREADTOCENTRYS:
		{
			struct cd_toc {
				struct ioc_toc_header header;
				struct cd_toc_entry entries[100];
			} data;
			struct ioc_read_toc_entry *te =
			(struct ioc_read_toc_entry *) addr;
			struct ioc_toc_header *th;
			u_int32 len, readlen, idx;
			u_int32 starting_track = te->starting_track;

			if (   te->data_len < sizeof(struct cd_toc_entry)
			    || (te->data_len % sizeof(struct cd_toc_entry)) != 0
			    || te->address_format != CD_MSF_FORMAT
			    && te->address_format != CD_LBA_FORMAT
			   ) {
				error = EINVAL;
				break;
			}
			th = &data.header;
			error = cd_read_toc(unit, 0, 0,
					(struct cd_toc_entry *)th, sizeof *th);
			if (error)
				break;
			if (starting_track == 0)
				starting_track = th->starting_track;
			else if (starting_track == 170)
				starting_track = th->ending_track + 1;
			else if (starting_track < th->starting_track ||
				 starting_track > th->ending_track + 1) {
				error = EINVAL;
				break;
			}
			len = ((th->ending_track + 1 - starting_track) + 1) *
				sizeof(struct cd_toc_entry);
			if (te->data_len < len)
				len = te->data_len;
			if (len > sizeof(data.entries)) {
				error = EINVAL;
				break;
			}

			/* calculate reading length without leadout entry */
			readlen = ((int)th->ending_track - starting_track) + 1;
			if (readlen < 1)        /* read at least one entry */
				readlen = 1;
			readlen *= sizeof(struct cd_toc_entry);
			if (readlen > len)
				readlen = len;

			error = cd_read_toc(unit, te->address_format,
				starting_track,
				(struct cd_toc_entry *)&data,
				readlen + sizeof(struct ioc_toc_header));
			if (error)
				break;
			th->len = ((th->len & 0xff) << 8) + ((th->len >> 8) & 0xff);

			/* make fake leadout entry if needed */
			idx = starting_track + len / sizeof(struct cd_toc_entry) - 1;
			if (idx == th->ending_track + 1) {
				idx -= starting_track; /* now offset in the entries */
				if (idx > 0) {
					data.entries[idx].control = data.entries[idx-1].control;
					data.entries[idx].addr_type = data.entries[idx-1].addr_type;
				}
				data.entries[idx].track = 170; /* magic */
				data.entries[idx].addr.lba = th->len;
			}

			error = copyout(data.entries, te->data, len);
		}
		break;
	case CDIOCSETPATCH:
		{
			struct ioc_patch *arg = (struct ioc_patch *) addr;
			struct cd_mode_data data;
			error = cd_get_mode(unit, &data, AUDIO_PAGE);
			if (error)
				break;
			data.page.audio.port[LEFT_PORT].channels = arg->patch[0];
			data.page.audio.port[RIGHT_PORT].channels = arg->patch[1];
			data.page.audio.port[2].channels = arg->patch[2];
			data.page.audio.port[3].channels = arg->patch[3];
			error = cd_set_mode(unit, &data);
			if (error)
				break; /* eh? */
		}
		break;
	case CDIOCGETVOL:
		{
			struct ioc_vol *arg = (struct ioc_vol *) addr;
			struct cd_mode_data data;
			error = cd_get_mode(unit, &data, AUDIO_PAGE);
			if (error)
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
			error = cd_get_mode(unit, &data, AUDIO_PAGE);
			if (error)
				break;
			data.page.audio.port[LEFT_PORT].channels = CHANNEL_0;
			data.page.audio.port[LEFT_PORT].volume = arg->vol[LEFT_PORT];
			data.page.audio.port[RIGHT_PORT].channels = CHANNEL_1;
			data.page.audio.port[RIGHT_PORT].volume = arg->vol[RIGHT_PORT];
			data.page.audio.port[2].volume = arg->vol[2];
			data.page.audio.port[3].volume = arg->vol[3];
			error = cd_set_mode(unit, &data);
			if (error)
				break;
		}
		break;
	case CDIOCSETMONO:
		{
			struct cd_mode_data data;
			error = cd_get_mode(unit, &data, AUDIO_PAGE);
			if (error)
				break;
			data.page.audio.port[LEFT_PORT].channels = LEFT_CHANNEL | RIGHT_CHANNEL;
			data.page.audio.port[RIGHT_PORT].channels = LEFT_CHANNEL | RIGHT_CHANNEL;
			data.page.audio.port[2].channels = 0;
			data.page.audio.port[3].channels = 0;
			error = cd_set_mode(unit, &data);
			if (error)
				break;
		}
		break;
	case CDIOCSETSTEREO:
		{
			struct cd_mode_data data;
			error = cd_get_mode(unit, &data, AUDIO_PAGE);
			if (error)
				break;
			data.page.audio.port[LEFT_PORT].channels = LEFT_CHANNEL;
			data.page.audio.port[RIGHT_PORT].channels = RIGHT_CHANNEL;
			data.page.audio.port[2].channels = 0;
			data.page.audio.port[3].channels = 0;
			error = cd_set_mode(unit, &data);
			if (error)
				break;
		}
		break;
	case CDIOCSETMUTE:
		{
			struct cd_mode_data data;
			error = cd_get_mode(unit, &data, AUDIO_PAGE);
			if (error)
				break;
			data.page.audio.port[LEFT_PORT].channels = 0;
			data.page.audio.port[RIGHT_PORT].channels = 0;
			data.page.audio.port[2].channels = 0;
			data.page.audio.port[3].channels = 0;
			error = cd_set_mode(unit, &data);
			if (error)
				break;
		}
		break;
	case CDIOCSETLEFT:
		{
			struct cd_mode_data data;
			error = cd_get_mode(unit, &data, AUDIO_PAGE);
			if (error)
				break;
			data.page.audio.port[LEFT_PORT].channels = LEFT_CHANNEL;
			data.page.audio.port[RIGHT_PORT].channels = LEFT_CHANNEL;
			data.page.audio.port[2].channels = 0;
			data.page.audio.port[3].channels = 0;
			error = cd_set_mode(unit, &data);
			if (error)
				break;
		}
		break;
	case CDIOCSETRIGHT:
		{
			struct cd_mode_data data;
			error = cd_get_mode(unit, &data, AUDIO_PAGE);
			if (error)
				break;
			data.page.audio.port[LEFT_PORT].channels = RIGHT_CHANNEL;
			data.page.audio.port[RIGHT_PORT].channels = RIGHT_CHANNEL;
			data.page.audio.port[2].channels = 0;
			data.page.audio.port[3].channels = 0;
			error = cd_set_mode(unit, &data);
			if (error)
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
		error = scsi_start_unit(sc_link, 0);
		break;
	case CDIOCSTOP:
		error = scsi_stop_unit(sc_link, 0, 0);
		break;
	case CDIOCEJECT:
		error = scsi_stop_unit(sc_link, 1, 0);
		break;
	case CDIOCALLOW:
		error = scsi_prevent(sc_link, PR_ALLOW, 0);
		break;
	case CDIOCPREVENT:
		error = scsi_prevent(sc_link, PR_PREVENT, 0);
		break;
	case CDIOCSETDEBUG:
		sc_link->flags |= (SDEV_DB1 | SDEV_DB2);
		break;
	case CDIOCCLRDEBUG:
		sc_link->flags &= ~(SDEV_DB1 | SDEV_DB2);
		break;
	case CDIOCRESET:
		return (cd_reset(unit));
		break;
	default:
		if(part == RAW_PART)
			error = scsi_do_ioctl(dev, cmd, addr, flag, p, sc_link);
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
static errval
cd_getdisklabel(unit)
	u_int8  unit;
{
	/*unsigned int n, m; */
	struct scsi_data *cd;

	cd = SCSI_DATA(&cd_switch, unit);

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
static u_int32
cd_size(unit, flags)
	int unit;
	int flags;
{
	struct scsi_read_cd_cap_data rdcap;
	struct scsi_read_cd_capacity scsi_cmd;
	u_int32 size;
	u_int32 blksize;
	struct scsi_link *sc_link = SCSI_LINK(&cd_switch, unit);
	struct scsi_data *cd = sc_link->sd;

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
	if (scsi_scsi_cmd(sc_link,
		(struct scsi_generic *) &scsi_cmd,
		sizeof(scsi_cmd),
		(u_char *) & rdcap,
		sizeof(rdcap),
		CDRETRIES,
		20000,		/* might be a disk-changer */
		NULL,
		SCSI_DATA_IN | flags) != 0) {
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
	SC_DEBUG(sc_link, SDEV_DB3, ("cd%d: %ld %ld byte blocks\n"
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
	retval = scsi_scsi_cmd(SCSI_LINK(&cd_switch, unit),
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
static errval
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
	/*
	 * SONY drives do not allow a mode select with a medium_type
	 * value that has just been returned by a mode sense; use a
	 * medium_type of 0 (Default) instead.
	 */
	data->header.medium_type = 0;
	return (scsi_scsi_cmd(SCSI_LINK(&cd_switch, unit),
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
static errval
cd_play(unit, blk, len)
	u_int32 unit, blk, len;
{
	struct scsi_play scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = PLAY;
	scsi_cmd.blk_addr[0] = (blk >> 24) & 0xff;
	scsi_cmd.blk_addr[1] = (blk >> 16) & 0xff;
	scsi_cmd.blk_addr[2] = (blk >> 8) & 0xff;
	scsi_cmd.blk_addr[3] = blk & 0xff;
	scsi_cmd.xfer_len[0] = (len >> 8) & 0xff;
	scsi_cmd.xfer_len[1] = len & 0xff;
	return (scsi_scsi_cmd(SCSI_LINK(&cd_switch, unit),
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
static errval
cd_play_big(unit, blk, len)
	u_int32 unit, blk, len;
{
	struct scsi_play_big scsi_cmd;

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
	return (scsi_scsi_cmd(SCSI_LINK(&cd_switch, unit),
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
static errval
cd_play_tracks(unit, strack, sindex, etrack, eindex)
	u_int32 unit, strack, sindex, etrack, eindex;
{
	struct scsi_play_track scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = PLAY_TRACK;
	scsi_cmd.start_track = strack;
	scsi_cmd.start_index = sindex;
	scsi_cmd.end_track = etrack;
	scsi_cmd.end_index = eindex;
	return (scsi_scsi_cmd(SCSI_LINK(&cd_switch, unit),
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
static errval
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

	return (scsi_scsi_cmd(SCSI_LINK(&cd_switch, unit),
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
static errval
cd_pause(unit, go)
	u_int32 unit, go;
{
	struct scsi_pause scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = PAUSE;
	scsi_cmd.resume = go;

	return (scsi_scsi_cmd(SCSI_LINK(&cd_switch, unit),
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
static errval
cd_reset(unit)
	u_int32 unit;
{
	return scsi_reset_target(SCSI_LINK(&cd_switch, unit));
}

/*
 * Read subchannel
 */
static errval
cd_read_subchannel(unit, mode, format, track, data, len)
	u_int32 unit, mode, format;
	int track;
	struct cd_sub_channel_info *data;
	u_int32 len;
{
	struct scsi_read_subchannel scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));

	scsi_cmd.op_code = READ_SUBCHANNEL;
	if (mode == CD_MSF_FORMAT)
		scsi_cmd.byte2 |= CD_MSF;
	scsi_cmd.byte3 = SRS_SUBQ;
	scsi_cmd.subchan_format = format;
	scsi_cmd.track = track;
	scsi_cmd.data_len[0] = (len) >> 8;
	scsi_cmd.data_len[1] = (len) & 0xff;
	return (scsi_scsi_cmd(SCSI_LINK(&cd_switch, unit),
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
	return (scsi_scsi_cmd(SCSI_LINK(&cd_switch, unit),
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
	struct scsi_link *sc_link = SCSI_LINK(&cd_switch, unit);

	/*
	 * First check if we have it all loaded
	 */
	if (sc_link->flags & SDEV_MEDIA_LOADED)
		return (0);
	/*
	 * give a number of sectors so that sec * trks * cyls
	 * is <= disk_size
	 */
	if (cd_size(unit, flags)) {
		sc_link->flags |= SDEV_MEDIA_LOADED;
		return (0);
	} else {
		return (ENXIO);
	}
}

static cd_devsw_installed = 0;

static void 	cd_drvinit(void *unused)
{
	dev_t dev;

	if( ! cd_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&cd_cdevsw, NULL);
		dev = makedev(BDEV_MAJOR, 0);
		bdevsw_add(&dev,&cd_bdevsw, NULL);
		cd_devsw_installed = 1;
    	}
}

SYSINIT(cddev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,cd_drvinit,NULL)


