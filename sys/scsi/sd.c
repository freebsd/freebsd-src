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
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         2       00149
 * --------------------         -----   ----------------------
 *
 * 16 Feb 93	Julian Elischer		ADDED for SCSI system
 * 20 Apr 93	Julian Elischer		Fixed error reporting
 *
 */

static	char rev[] = "$Revision: 1.3 $";

/*
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
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

long int sdstrats,sdqueues;


#include <ddb.h>
#if	NDDB > 0
int	Debugger();
#else	NDDB > 0
#define Debugger()
#endif	NDDB > 0


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

struct buf sd_buf_queue[NSD];
int	sd_done();
int	sdstrategy();

int	sd_debug = 0;

struct	scsi_xfer	*sd_free_xfer[NSD];
int			sd_xfer_block_wait[NSD];

struct	sd_data
{
	int	flags;
#define	SDVALID		0x02		/* PARAMS LOADED	*/
#define	SDINIT		0x04		/* device has been init'd */
#define	SDWAIT		0x08		/* device has someone waiting */
#define SDHAVELABEL	0x10		/* have read the label */
#define SDDOSPART	0x20		/* Have read the DOS partition table */
#define SDWRITEPROT	0x40		/* Device in readonly mode (S/W)*/
	struct	scsi_switch *sc_sw;	/* address of scsi low level switch */
	int	ctlr;			/* so they know which one we want */
	int	targ;			/* our scsi target ID */
	int	lu;			/* out scsi lu */
	long int	ad_info;	/* info about the adapter */
	int	cmdscount;		/* cmds allowed outstanding by board*/
	int	wlabel;			/* label is writable */
	struct  disk_parms
	{
		u_char	heads;		/* Number of heads */
		u_short	cyls;		/* Number of cylinders */
		u_char	sectors;/*dubious*/	/* Number of sectors/track */
		u_short	secsiz;		/* Number of bytes/sector */
		u_long	disksize;		/* total number sectors */
	}params;
	struct	disklabel	disklabel;
	struct  dos_partition dosparts[NDOSPART]; /* DOS view of disk */
	int	partflags[MAXPARTITIONS];	/* per partition flags */
#define SDOPEN	0x01
	int		openparts;		/* one bit for each open partition */
	unsigned int	sd_start_of_unix;	/* unix vs dos partitions */
}sd_data[NSD];


static	int	next_sd_unit = 0;
/***********************************************************************\
* The routine called by the low level scsi routine when it discovers	*
* A device suitable for this driver					*
\***********************************************************************/

int	sdattach(ctlr,targ,lu,scsi_switch)
struct	scsi_switch *scsi_switch;
{
	int	unit,i;
	unsigned char *tbl;
	struct sd_data *sd;
	struct disk_parms *dp;
	long int	ad_info;
	struct	scsi_xfer	*sd_scsi_xfer;

	unit = next_sd_unit++;
	sd = sd_data + unit;
	dp  = &(sd->params);
	if(scsi_debug & PRINTROUTINES) printf("sdattach: "); 
	/*******************************************************\
	* Check we have the resources for another drive		*
	\*******************************************************/
	if( unit >= NSD)
	{
		printf("Too many scsi disks..(%d > %d) reconfigure kernel",(unit + 1),NSD);
		return(0);
	}
	/*******************************************************\
	* Store information needed to contact our base driver	*
	\*******************************************************/
	sd->sc_sw	=	scsi_switch;
	sd->ctlr	=	ctlr;
	sd->targ	=	targ;
	sd->lu		=	lu;
	if(sd->sc_sw->adapter_info)
	{
		sd->ad_info = ( (*(sd->sc_sw->adapter_info))(ctlr));
		sd->cmdscount =	sd->ad_info & AD_INF_MAX_CMDS;
		if(sd->cmdscount > SDOUTSTANDING)
		{
			sd->cmdscount = SDOUTSTANDING;
		}
	}
	else
	{
		sd->ad_info = 1;
		sd->cmdscount =	1;
	}

	i = sd->cmdscount;
	sd_scsi_xfer = (struct scsi_xfer *)malloc(sizeof(struct scsi_xfer) * i
				,M_TEMP, M_NOWAIT);
	while(i-- )
	{
		sd_scsi_xfer->next = sd_free_xfer[unit];
		sd_free_xfer[unit] = sd_scsi_xfer;
		sd_scsi_xfer++;
	}
	/*******************************************************\
	* Use the subdriver to request information regarding	*
	* the drive. We cannot use interrupts yet, so the	*
	* request must specify this.				*
	\*******************************************************/
	sd_get_parms(unit,  SCSI_NOSLEEP |  SCSI_NOMASK);
	printf("	sd%d: %dMB, cyls %d, heads %d, secs %d, bytes/sec %d\n",
		unit,
		(	  dp->cyls 
			* dp->heads 
			* dp->sectors
			* dp->secsiz
		)
		/ (1024 * 1024),
		dp->cyls,
		dp->heads,
		dp->sectors,
		dp->secsiz);
	/*******************************************************\
	* Set up the bufs for this device			*
	\*******************************************************/
	sd->flags |= SDINIT;
	return;

}



/*******************************************************\
*	open the device. Make sure the partition info	*
* is a up-to-date as can be.				*
\*******************************************************/
sdopen(dev)
{
	int errcode = 0;
	int unit, part;
	struct disk_parms disk_parms;
	struct sd_data *sd ;

	unit = UNIT(dev);
	part = PARTITION(dev);
	sd = sd_data + unit;
	if(scsi_debug & (PRINTROUTINES | TRACEOPENS))
		printf("sdopen: dev=0x%x (unit %d (of %d),partition %d)\n"
				,   dev,      unit,   NSD,         part);
	/*******************************************************\
	* Check the unit is legal				*
	\*******************************************************/
	if ( unit >= NSD )
	{
		return(ENXIO);
	}
	/*******************************************************\
	* Make sure the disk has been initialised		*
	* At some point in the future, get the scsi driver	*
	* to look for a new device if we are not initted	*
	\*******************************************************/
	if (! (sd->flags & SDINIT))
	{
		return(ENXIO);
	}

	/*******************************************************\
	* If it's been invalidated, and not everybody has	*
	* closed it then forbid re-entry.			*
	\*******************************************************/
	if ((! (sd->flags & SDVALID))
	   && ( sd->openparts))
		return(ENXIO);
	/*******************************************************\
	* Check that it is still responding and ok.		*
	* "unit attention errors should occur here if the drive	*
	* has been restarted or the pack changed		*
	\*******************************************************/

	if(scsi_debug & TRACEOPENS)
		printf("device is ");
	if (sd_test_unit_ready(unit,0))
	{
		if(scsi_debug & TRACEOPENS) printf("not reponding\n");
		return(ENXIO);
	}
	if(scsi_debug & TRACEOPENS)
		printf("ok\n");
	/*******************************************************\
	* In case it is a funny one, tell it to start		*
	* not needed for  most hard drives (ignore failure)	*
	\*******************************************************/
	sd_start_unit(unit,SCSI_ERR_OK|SCSI_SILENT);
	if(scsi_debug & TRACEOPENS)
		printf("started ");
	/*******************************************************\
	* Load the physical device parameters 			*
	\*******************************************************/
	sd_get_parms(unit, 0);	/* sets SDVALID */
	if (sd->params.secsiz != SECSIZE)
	{
		printf("sd%d: Can't deal with %d bytes logical blocks\n"
			,unit, sd->params.secsiz);
		Debugger();
		return(ENXIO);
	}
	if(scsi_debug & TRACEOPENS)
		printf("Params loaded ");
	/*******************************************************\
	* Load the partition info if not already loaded		*
	\*******************************************************/
	sd_prevent(unit,PR_PREVENT,SCSI_ERR_OK|SCSI_SILENT); /* who cares if it fails? */
	if((errcode = sdgetdisklabel(unit)) && (part != RAW_PART))
	{
		sd_prevent(unit,PR_ALLOW,SCSI_ERR_OK|SCSI_SILENT); /* who cares if it fails? */
		return(errcode);
	}
	if(scsi_debug & TRACEOPENS)
		printf("Disklabel loaded ");
	/*******************************************************\
	* Check the partition is legal				*
	\*******************************************************/
	if ( part >= MAXPARTITIONS ) {
		sd_prevent(unit,PR_ALLOW,SCSI_ERR_OK|SCSI_SILENT); /* who cares if it fails? */
		return(ENXIO);
	}
	if(scsi_debug & TRACEOPENS)
		printf("ok");
	/*******************************************************\
	*  Check that the partition exists			*
	\*******************************************************/
	if (( sd->disklabel.d_partitions[part].p_size == 0 )
		&& (part != RAW_PART))
	{
		sd_prevent(unit,PR_ALLOW,SCSI_ERR_OK|SCSI_SILENT); /* who cares if it fails? */
		return(ENXIO);
	}
	sd->partflags[part] |= SDOPEN;
	sd->openparts |= (1 << part);
	if(scsi_debug & TRACEOPENS)
		printf("open %d %d\n",sdstrats,sdqueues);
	return(0);
}

/*******************************************************\
* Get ownership of a scsi_xfer				*
* If need be, sleep on it, until it comes free		*
\*******************************************************/
struct scsi_xfer *sd_get_xs(unit,flags)
int	flags;
int	unit;
{
	struct scsi_xfer *xs;
	int	s;

	if(flags & (SCSI_NOSLEEP |  SCSI_NOMASK))
	{
		if (xs = sd_free_xfer[unit])
		{
			sd_free_xfer[unit] = xs->next;
			xs->flags = 0;
		}
	}
	else
	{
		s = SPLSD();
		while (!(xs = sd_free_xfer[unit]))
		{
			sd_xfer_block_wait[unit]++;  /* someone waiting! */
			sleep((caddr_t)&sd_free_xfer[unit], PRIBIO+1);
			sd_xfer_block_wait[unit]--;
		}
		sd_free_xfer[unit] = xs->next;
		splx(s);
		xs->flags = 0;
	}
	return(xs);
}

/*******************************************************\
* Free a scsi_xfer, wake processes waiting for it	*
\*******************************************************/
sd_free_xs(unit,xs,flags)
struct scsi_xfer *xs;
int	unit;
int	flags;
{
	int	s;
	
	if(flags & SCSI_NOMASK)
	{
		if (sd_xfer_block_wait[unit])
		{
			printf("doing a wakeup from NOMASK mode\n");
			wakeup((caddr_t)&sd_free_xfer[unit]);
		}
		xs->next = sd_free_xfer[unit];
		sd_free_xfer[unit] = xs;
	}
	else
	{
		s = SPLSD();
		if (sd_xfer_block_wait[unit])
			wakeup((caddr_t)&sd_free_xfer[unit]);
		xs->next = sd_free_xfer[unit];
		sd_free_xfer[unit] = xs;
		splx(s);
	}
}

/*******************************************************\
* trim the size of the transfer if needed,		*
* called by physio					*
* basically the smaller of our max and the scsi driver's*
* minphys (note we have no max)				*
\*******************************************************/
/* Trim buffer length if buffer-size is bigger than page size */
void	sdminphys(bp)
struct buf	*bp;
{
	(*(sd_data[UNIT(bp->b_dev)].sc_sw->scsi_minphys))(bp);
}

/*******************************************************\
* Actually translate the requested transfer into	*
* one the physical driver can understand		*
* The transfer is described by a buf and will include	*
* only one physical transfer.				*
\*******************************************************/

int	sdstrategy(bp)
struct	buf	*bp;
{
	struct	buf	*dp;
	unsigned int opri;
	struct sd_data *sd ;
	int	unit;

	sdstrats++;
	unit = UNIT((bp->b_dev));
	sd = sd_data + unit;
	if(scsi_debug & PRINTROUTINES) printf("\nsdstrategy ");
	if(scsi_debug & SHOWREQUESTS) printf("sd%d: %d bytes @ blk%d\n",
					unit,bp->b_bcount,bp->b_blkno);
	sdminphys(bp);
	/*******************************************************\
	* If the device has been made invalid, error out	*
	\*******************************************************/
	if(!(sd->flags & SDVALID))
	{
		bp->b_error = EIO;
		goto bad;
	}
	/*******************************************************\
	* "soft" write protect check				*
	\*******************************************************/
	if ((sd->flags & SDWRITEPROT) && (bp->b_flags & B_READ) == 0) {
		bp->b_error = EROFS;
		goto bad;
	}
	/*******************************************************\
	* If it's a null transfer, return immediatly		*
	\*******************************************************/
	if (bp->b_bcount == 0)
	{
		goto done;
	}

	/*******************************************************\
	* Decide which unit and partition we are talking about	*
	* only raw is ok if no label				*
	\*******************************************************/
	if(PARTITION(bp->b_dev) != RAW_PART)
	{
		if (!(sd->flags & SDHAVELABEL))
		{
			bp->b_error = EIO;
			goto bad;
		}

		/*
		 * do bounds checking, adjust transfer. if error, process.
		 * if end of partition, just return
		 */
		if (bounds_check_with_label(bp,&sd->disklabel,sd->wlabel) <= 0)
			goto done;
		/* otherwise, process transfer request */
	}

	opri = SPLSD();
	dp = &sd_buf_queue[unit];

	/*******************************************************\
	* Place it in the queue of disk activities for this disk*
	\*******************************************************/
	disksort(dp, bp);

	/*******************************************************\
	* Tell the device to get going on the transfer if it's	*
	* not doing anything, otherwise just wait for completion*
	\*******************************************************/
	sdstart(unit);

	splx(opri);
	return;
bad:
	bp->b_flags |= B_ERROR;
done:

	/*******************************************************\
	* Correctly set the buf to indicate a completed xfer	*
	\*******************************************************/
  	bp->b_resid = bp->b_bcount;
	biodone(bp);
	return;
}

/***************************************************************\
* sdstart looks to see if there is a buf waiting for the device	*
* and that the device is not already busy. If both are true,	*
* It deques the buf and creates a scsi command to perform the	*
* transfer in the buf. The transfer request will call sd_done	*
* on completion, which will in turn call this routine again	*
* so that the next queued transfer is performed.		*
* The bufs are queued by the strategy routine (sdstrategy)	*
*								*
* This routine is also called after other non-queued requests	*
* have been made of the scsi driver, to ensure that the queue	*
* continues to be drained.					*
*								*
* must be called at the correct (highish) spl level		*
\***************************************************************/
/* sdstart() is called at SPLSD  from sdstrategy and sd_done*/
sdstart(unit)
int	unit;
{
	int			drivecount;
	register struct buf	*bp = 0;
	register struct buf	*dp;
	struct	scsi_xfer	*xs;
	struct	scsi_rw_big	cmd;
	int			blkno, nblk;
	struct sd_data *sd = sd_data + unit;
	struct partition *p ;

	if(scsi_debug & PRINTROUTINES) printf("sdstart%d ",unit);
	/*******************************************************\
	* See if there is a buf to do and we are not already	*
	* doing one						*
	\*******************************************************/
	if(!sd_free_xfer[unit])
	{
		return;    /* none for us, unit already underway */
	}

	if(sd_xfer_block_wait[unit])    /* there is one, but a special waits */
	{
		return;	/* give the special that's waiting a chance to run */
	}


	dp = &sd_buf_queue[unit];
	if ((bp = dp->b_actf) != NULL)	/* yes, an assign */
	{
		dp->b_actf = bp->av_forw;
	}
	else
	{ 
		return;
	}

	xs=sd_get_xs(unit,0);	/* ok we can grab it */
	xs->flags = INUSE;    /* Now ours */
	/*******************************************************\
	*  If the device has become invalid, abort all the	*
	* reads and writes until all files have been closed and	*
	* re-openned						*
	\*******************************************************/
	if(!(sd->flags & SDVALID))
	{
		xs->error = XS_DRIVER_STUFFUP;
		sd_done(unit,xs);  /* clean up (calls sdstart) */
		return ;
	}
	/*******************************************************\
	* We have a buf, now we should move the data into	*
	* a scsi_xfer definition and try start it		*
	\*******************************************************/
	/*******************************************************\
	*  First, translate the block to absolute		*
	\*******************************************************/
	p = sd->disklabel.d_partitions + PARTITION(bp->b_dev);
	blkno = bp->b_blkno + p->p_offset;
	nblk = (bp->b_bcount + 511) >> 9;

	/*******************************************************\
	*  Fill out the scsi command				*
	\*******************************************************/
	bzero(&cmd, sizeof(cmd));
	cmd.op_code		=	(bp->b_flags & B_READ) 
						? READ_BIG : WRITE_BIG;
	cmd.addr_3	=	(blkno & 0xff000000) >> 24;
	cmd.addr_2	=	(blkno & 0xff0000) >> 16;
	cmd.addr_1	=	(blkno & 0xff00) >> 8;
	cmd.addr_0	=	blkno & 0xff;
	cmd.length2	=	(nblk & 0xff00) >> 8;
	cmd.length1	=	(nblk & 0xff);
	/*******************************************************\
	* Fill out the scsi_xfer structure			*
	*	Note: we cannot sleep as we may be an interrupt	*
	\*******************************************************/
	xs->flags	|=	SCSI_NOSLEEP;
	xs->adapter	=	sd->ctlr;
	xs->targ	=	sd->targ;
	xs->lu		=	sd->lu;
	xs->retries	=	SD_RETRIES;
	xs->timeout	=	10000;/* 10000 millisecs for a disk !*/
	xs->cmd		=	(struct scsi_generic *)&cmd;
	xs->cmdlen	=	sizeof(cmd);
	xs->resid	=	bp->b_bcount;
	xs->when_done	=	sd_done;
	xs->done_arg	=	unit;
	xs->done_arg2	=	(int)xs;
	xs->error	=	XS_NOERROR;
	xs->bp		=	bp;
	xs->data	=	(u_char *)bp->b_un.b_addr;
	xs->datalen	=	bp->b_bcount;
	/*******************************************************\
	* Pass all this info to the scsi driver.		*
	\*******************************************************/



	if ( (*(sd->sc_sw->scsi_cmd))(xs) != SUCCESSFULLY_QUEUED)
	{
		printf("sd%d: oops not queued",unit);
		xs->error = XS_DRIVER_STUFFUP;
		sd_done(unit,xs);  /* clean up (calls sdstart) */
	}
	sdqueues++;
}

/*******************************************************\
* This routine is called by the scsi interrupt when	*
* the transfer is complete.
\*******************************************************/
int	sd_done(unit,xs)
int	unit;
struct	scsi_xfer	*xs;
{
	struct	buf		*bp;
	int	retval;
	int	retries = 0;

	if(scsi_debug & PRINTROUTINES) printf("sd_done%d ",unit);
	if (! (xs->flags & INUSE))
		panic("scsi_xfer not in use!");
	if(bp = xs->bp)
	{
		switch(xs->error)
		{
		case	XS_NOERROR:
			bp->b_error = 0;
			bp->b_resid = 0;
			break;

		case	XS_SENSE:
			retval = (sd_interpret_sense(unit,xs));
			if(retval)
			{
				bp->b_flags |= B_ERROR;
				bp->b_error = retval;
			}
			break;

		case	XS_TIMEOUT:
			printf("sd%d timeout\n",unit);

		case	XS_BUSY:	/* should retry */ /* how? */
			/************************************************/
			/* SHOULD put buf back at head of queue		*/
			/* and decrement retry count in (*xs)		*/
			/* HOWEVER, this should work as a kludge	*/
			/************************************************/
			if(xs->retries--)
			{
				xs->error = XS_NOERROR;
				xs->flags &= ~ITSDONE;
				if ( (*(sd_data[unit].sc_sw->scsi_cmd))(xs)
					== SUCCESSFULLY_QUEUED)
				{	/* don't wake the job, ok? */
					return;
				}
				xs->flags |= ITSDONE;
			} /* fall through */

		case	XS_DRIVER_STUFFUP:
			bp->b_flags |= B_ERROR;
			bp->b_error = EIO;
			break;
		default:
			printf("sd%d: unknown error category from scsi driver\n"
				,unit);
		}	
		biodone(bp);
		sd_free_xs(unit,xs,0);
		sdstart(unit);	/* If there's anything waiting.. do it */
	}
	else /* special has finished */
	{
		wakeup(xs);
	}
}
/*******************************************************\
* Perform special action on behalf of the user		*
* Knows about the internals of this device		*
\*******************************************************/
sdioctl(dev_t dev, int cmd, caddr_t addr, int flag)
{
	/* struct sd_cmd_buf *args;*/
	int error = 0;
	unsigned int opri;
	unsigned char unit, part;
	register struct sd_data *sd;


	/*******************************************************\
	* Find the device that the user is talking about	*
	\*******************************************************/
	unit = UNIT(dev);
	part = PARTITION(dev);
	sd = &sd_data[unit];
	if(scsi_debug & PRINTROUTINES) printf("sdioctl%d ",unit);

	/*******************************************************\
	* If the device is not valid.. abandon ship		*
	\*******************************************************/
	if (!(sd_data[unit].flags & SDVALID))
		return(EIO);
	switch(cmd)
	{

	case DIOCSBAD:
                        error = EINVAL;
		break;

	case DIOCGDINFO:
		*(struct disklabel *)addr = sd->disklabel;
		break;

        case DIOCGPART:
                ((struct partinfo *)addr)->disklab = &sd->disklabel;
                ((struct partinfo *)addr)->part =
                    &sd->disklabel.d_partitions[PARTITION(dev)];
                break;

        case DIOCSDINFO:
                if ((flag & FWRITE) == 0)
                        error = EBADF;
                else
                        error = setdisklabel(&sd->disklabel,
					(struct disklabel *)addr,
                         /*(sd->flags & DKFL_BSDLABEL) ? sd->openparts : */0,
				sd->dosparts);
                if (error == 0) {
			sd->flags |= SDHAVELABEL;
		}
                break;

        case DIOCWLABEL:
		sd->flags &= ~SDWRITEPROT;
                if ((flag & FWRITE) == 0)
                        error = EBADF;
                else
                        sd->wlabel = *(int *)addr;
                break;

        case DIOCWDINFO:
		sd->flags &= ~SDWRITEPROT;
                if ((flag & FWRITE) == 0)
                        error = EBADF;
                else
		{
			if ((error = setdisklabel(&sd->disklabel
						, (struct disklabel *)addr
			, /*(sd->flags & SDHAVELABEL) ? sd->openparts :*/ 0
						, sd->dosparts)) == 0)
			{
                        	int wlab;

				sd->flags |= SDHAVELABEL; /* ok write will succeed */

                        	/* simulate opening partition 0 so write succeeds */
                        	sd->openparts |= (1 << 0);            /* XXX */
                        	wlab = sd->wlabel;
                        	sd->wlabel = 1;
                        	error = writedisklabel(dev, sdstrategy,
					&sd->disklabel, sd->dosparts);
                        	sd->wlabel = wlab;
                	}
		}
                break;


	default:
		error = ENOTTY;
		break;
	}
	return (error);
}


/*******************************************************\
* Load the label information on the named device	*
\*******************************************************/
int sdgetdisklabel(unit)
unsigned char	unit;
{
	/*unsigned int n, m;*/
	char *errstring;
	struct dos_partition *dos_partition_p;
	struct sd_data *sd = sd_data + unit;

	/*******************************************************\
	* If the inflo is already loaded, use it		*
	\*******************************************************/
	if(sd->flags & SDHAVELABEL) return(ESUCCESS);

	bzero(&sd->disklabel,sizeof(struct disklabel));
	/*******************************************************\
	* make partition 3 the whole disk in case of failure	*
  	*   then get pdinfo 					*
	\*******************************************************/
	sd->disklabel.d_partitions[0].p_offset = 0;
	sd->disklabel.d_partitions[0].p_size = sd->params.disksize;
	sd->disklabel.d_partitions[RAW_PART].p_offset = 0;
	sd->disklabel.d_partitions[RAW_PART].p_size = sd->params.disksize;
	sd->disklabel.d_npartitions = MAXPARTITIONS;
	sd->disklabel.d_secsize = 512; /* as long as it's not 0 */
	sd->disklabel.d_ntracks = sd->params.heads;
	sd->disklabel.d_nsectors = sd->params.sectors;
	sd->disklabel.d_ncylinders = sd->params.cyls;
	sd->disklabel.d_secpercyl = sd->params.heads * sd->params.sectors;
	if (sd->disklabel.d_secpercyl == 0)
	{
		sd->disklabel.d_secpercyl = 100;
					/* as long as it's not 0 */
					/* readdisklabel divides by it */
	}

	/*******************************************************\
	* all the generic bisklabel extraction routine		*
	\*******************************************************/
	if(errstring = readdisklabel(makedev(0 ,(unit<<UNITSHIFT )+3)
					, sdstrategy
					, &sd->disklabel
					, sd->dosparts
					, 0
					, 0))
	{
		printf("sd%d: %s\n",unit, errstring);
		return(ENXIO);
	}
	/*******************************************************\
	* leave partition 2 "open" for raw I/O 			*
	\*******************************************************/

	sd->flags |= SDHAVELABEL; /* WE HAVE IT ALL NOW */
	return(ESUCCESS);
}

/*******************************************************\
* Find out from the device what it's capacity is	*
\*******************************************************/
sd_size(unit, flags)
{
	struct scsi_read_cap_data rdcap;
	struct scsi_read_capacity scsi_cmd;
	int size;

	/*******************************************************\
	* make up a scsi command and ask the scsi driver to do	*
	* it for you.						*
	\*******************************************************/
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = READ_CAPACITY;

	/*******************************************************\
	* If the command works, interpret the result as a 4 byte*
	* number of blocks					*
	\*******************************************************/
	if (sd_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			&rdcap,
			sizeof(rdcap),  
			2000,
			flags) != 0)
	{
		printf("could not get size of unit %d\n", unit);
		return(0);
	} else {
		size = rdcap.addr_0 + 1 ;
		size += rdcap.addr_1 << 8;
		size += rdcap.addr_2 << 16;
		size += rdcap.addr_3 << 24;
	}
	return(size);
}
	
/*******************************************************\
* Get scsi driver to send a "are you ready?" command	*
\*******************************************************/
sd_test_unit_ready(unit,flags)
int	unit,flags;
{
	struct	scsi_test_unit_ready scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = TEST_UNIT_READY;

	return (sd_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			0,
			0,
			100000,
			flags));
}

/*******************************************************\
* Prevent or allow the user to remove the tape		*
\*******************************************************/
sd_prevent(unit,type,flags)
int	unit,type,flags;
{
	struct	scsi_prevent	scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = PREVENT_ALLOW;
	scsi_cmd.prevent=type;
	return (sd_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			0,
			0,
			5000,
			flags) );
}
/*******************************************************\
* Get scsi driver to send a "start up" command		*
\*******************************************************/
sd_start_unit(unit,flags)
int	unit,flags;
{
	struct scsi_start_stop scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = START_STOP;
	scsi_cmd.start = 1;

	return (sd_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			0,
			0,
			2000,
			flags));
}

/*******************************************************\
* Tell the device to map out a defective block		*
\*******************************************************/
sd_reassign_blocks(unit,block)
{
	struct	scsi_reassign_blocks		scsi_cmd;
	struct	scsi_reassign_blocks_data	rbdata;


	bzero(&scsi_cmd, sizeof(scsi_cmd));
	bzero(&rbdata, sizeof(rbdata));
	scsi_cmd.op_code = REASSIGN_BLOCKS;

	rbdata.length_msb = 0;
	rbdata.length_lsb = sizeof(rbdata.defect_descriptor[0]);
	rbdata.defect_descriptor[0].dlbaddr_3 = ((block >> 24) & 0xff);
	rbdata.defect_descriptor[0].dlbaddr_2 = ((block >> 16) & 0xff);
	rbdata.defect_descriptor[0].dlbaddr_1 = ((block >>  8) & 0xff);
	rbdata.defect_descriptor[0].dlbaddr_0 = ((block      ) & 0xff);

	return(sd_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			&rbdata,
			sizeof(rbdata),
			5000,
			0));
}

#define b2tol(a)	(((unsigned)(a##_1) << 8) + (unsigned)a##_0 )

/*******************************************************\
* Get the scsi driver to send a full inquiry to the	*
* device and use the results to fill out the disk 	*
* parameter structure.					*
\*******************************************************/

int	sd_get_parms(unit, flags)
{
	struct sd_data *sd = sd_data + unit;
	struct disk_parms *disk_parms = &sd->params;
	struct	scsi_mode_sense		scsi_cmd;
	struct	scsi_mode_sense_data
	{
		struct	scsi_mode_header	header;
		struct	blk_desc		blk_desc;
		union	disk_pages		pages;
	}scsi_sense;
	int sectors;

	/*******************************************************\
	* First check if we have it all loaded			*
	\*******************************************************/
		if(sd->flags & SDVALID) return(0);
	/*******************************************************\
	* First do a mode sense page 3				*
	\*******************************************************/
	if (sd_debug)
	{
		bzero(&scsi_cmd, sizeof(scsi_cmd));
		scsi_cmd.op_code = MODE_SENSE;
		scsi_cmd.page_code = 3;
		scsi_cmd.length = 0x24;
		/*******************************************************\
		* do the command, but we don't need the results		*
		* just print them for our interest's sake		*
		\*******************************************************/
		if (sd_scsi_cmd(unit,
				&scsi_cmd,
				sizeof(scsi_cmd),
				&scsi_sense,
				sizeof(scsi_sense),
				2000,
				flags) != 0)
		{
			printf("could not mode sense (3) for unit %d\n", unit);
			return(ENXIO);
		} 
		printf("unit %d: %d trk/zone, %d alt_sec/zone, %d alt_trk/zone, %d alt_trk/lun\n",
			unit,
			b2tol(scsi_sense.pages.disk_format.trk_z),
			b2tol(scsi_sense.pages.disk_format.alt_sec),
			b2tol(scsi_sense.pages.disk_format.alt_trk_z),
			b2tol(scsi_sense.pages.disk_format.alt_trk_v));
		printf("         %d sec/trk, %d bytes/sec, %d interleave, %d %d bytes/log_blk\n",
			b2tol(scsi_sense.pages.disk_format.ph_sec_t),
			b2tol(scsi_sense.pages.disk_format.bytes_s),
			b2tol(scsi_sense.pages.disk_format.interleave),
			sd_size(unit, flags),
			_3btol(scsi_sense.blk_desc.blklen));
	}


	/*******************************************************\
	* do a "mode sense page 4"				*
	\*******************************************************/
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = MODE_SENSE;
	scsi_cmd.page_code = 4;
	scsi_cmd.length = 0x20;
	/*******************************************************\
	* If the command worked, use the results to fill out	*
	* the parameter structure				*
	\*******************************************************/
	if (sd_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			&scsi_sense,
			sizeof(scsi_sense),
			2000,
			flags) != 0)
	{
		printf("could not mode sense (4) for unit %d\n", unit);
		printf(" using ficticious geometry\n");
		/* use adaptec standard ficticious geometry */
		sectors = sd_size(unit, flags);
		disk_parms->heads = 64;
		disk_parms->sectors = 32;
		disk_parms->cyls = sectors/(64 * 32);
		disk_parms->secsiz = SECSIZE;
	} 
	else
	{

		if (sd_debug)
		{
		printf("         %d cyls, %d heads, %d precomp, %d red_write, %d land_zone\n",
			_3btol(&scsi_sense.pages.rigid_geometry.ncyl_2),
			scsi_sense.pages.rigid_geometry.nheads,
			b2tol(scsi_sense.pages.rigid_geometry.st_cyl_wp),
			b2tol(scsi_sense.pages.rigid_geometry.st_cyl_rwc),
			b2tol(scsi_sense.pages.rigid_geometry.land_zone));
		}

		/*******************************************************\
		* KLUDGE!!(for zone recorded disks)			*
		* give a number of sectors so that sec * trks * cyls	*
		* is <= disk_size 					*
		\*******************************************************/
		disk_parms->heads = scsi_sense.pages.rigid_geometry.nheads;
		disk_parms->cyls = _3btol(&scsi_sense.pages.rigid_geometry.ncyl_2);
		disk_parms->secsiz = _3btol(&scsi_sense.blk_desc.blklen);

		sectors = sd_size(unit, flags);
		sectors /= disk_parms->cyls;
		sectors /= disk_parms->heads;
		disk_parms->sectors = sectors; /* dubious on SCSI*/
	}

	sd->flags |= SDVALID;
	return(0);
}

/*******************************************************\
* close the device.. only called if we are the LAST	*
* occurence of an open device				*
\*******************************************************/
sdclose(dev)
dev_t dev;
{
	unsigned char unit, part;
	unsigned int old_priority;

	unit = UNIT(dev);
	part = PARTITION(dev);
	sd_data[unit].partflags[part] &= ~SDOPEN;
	sd_data[unit].openparts &= ~(1 << part);
	if(sd_data[unit].openparts == 0) /* if all partitions closed */
	{
		sd_prevent(unit,PR_ALLOW,SCSI_SILENT|SCSI_ERR_OK);
	}
	return(0);
}

/*******************************************************\
* ask the scsi driver to perform a command for us.	*
* Call it through the switch table, and tell it which	*
* sub-unit we want, and what target and lu we wish to	*
* talk to. Also tell it where to find the command	*
* how long int is.					*
* Also tell it where to read/write the data, and how	*
* long the data is supposed to be			*
\*******************************************************/
int	sd_scsi_cmd(unit,scsi_cmd,cmdlen,data_addr,datalen,timeout,flags)

int	unit,flags;
struct	scsi_generic *scsi_cmd;
int	cmdlen;
int	timeout;
u_char	*data_addr;
int	datalen;
{
	struct	scsi_xfer *xs;
	int	retval;
	int	s;
	struct sd_data *sd = sd_data + unit;

	if(scsi_debug & PRINTROUTINES) printf("\nsd_scsi_cmd%d ",unit);
	if(sd->sc_sw)	/* If we have a scsi driver */
	{
		xs = sd_get_xs(unit,flags); /* should wait unless booting */
		if(!xs)
		{
			printf("sd_scsi_cmd%d: controller busy"
 					" (this should never happen)\n",unit); 
				return(EBUSY);
		}
		xs->flags |= INUSE;
		/*******************************************************\
		* Fill out the scsi_xfer structure			*
		\*******************************************************/
		xs->flags	|=	flags;
		xs->adapter	=	sd->ctlr;
		xs->targ	=	sd->targ;
		xs->lu		=	sd->lu;
		xs->retries	=	SD_RETRIES;
		xs->timeout	=	timeout;
		xs->cmd		=	scsi_cmd;
		xs->cmdlen	=	cmdlen;
		xs->data	=	data_addr;
		xs->datalen	=	datalen;
		xs->resid	=	datalen;
		xs->when_done	=	(flags & SCSI_NOMASK)
					?(int (*)())0
					:sd_done;
		xs->done_arg	=	unit;
		xs->done_arg2	=	(int)xs;
retry:		xs->error	=	XS_NOERROR;
		xs->bp		=	0;
		retval = (*(sd->sc_sw->scsi_cmd))(xs);
		switch(retval)
		{
		case	SUCCESSFULLY_QUEUED:
			s = splbio();
			while(!(xs->flags & ITSDONE))
				sleep(xs,PRIBIO+1);
			splx(s);

		case	HAD_ERROR:
			/*printf("err = %d ",xs->error);*/
			switch(xs->error)
			{
			case	XS_NOERROR:
				retval = ESUCCESS;
				break;
			case	XS_SENSE:
				retval = (sd_interpret_sense(unit,xs));
				break;
			case	XS_DRIVER_STUFFUP:
				retval = EIO;
				break;
			case	XS_TIMEOUT:
				if(xs->retries-- )
				{
					xs->flags &= ~ITSDONE;
					goto retry;
				}
				retval = EIO;
				break;
			case	XS_BUSY:
				if(xs->retries-- )
				{
					xs->flags &= ~ITSDONE;
					goto retry;
				}
				retval = EIO;
				break;
			default:
				retval = EIO;
				printf("sd%d: unknown error category from scsi driver\n"
					,unit);
			}	
			break;
		case	COMPLETE:
			retval = ESUCCESS;
			break;
		case 	TRY_AGAIN_LATER:
			if(xs->retries-- )
			{
				xs->flags &= ~ITSDONE;
				goto retry;
			}
			retval = EIO;
			break;
		default:
			retval = EIO;
		}
		sd_free_xs(unit,xs,flags);
		sdstart(unit);		/* check if anything is waiting fr the xs */
	}
	else
	{
		printf("sd%d: not set up\n",unit);
		return(EINVAL);
	}
	return(retval);
}
/***************************************************************\
* Look at the returned sense and act on the error and detirmine	*
* The unix error number to pass back... (0 = report no error)	*
\***************************************************************/

int	sd_interpret_sense(unit,xs)
int	unit;
struct	scsi_xfer *xs;
{
	struct	scsi_sense_data *sense;
	int	key;
	int	silent;

	/***************************************************************\
	* If the flags say errs are ok, then always return ok.		*
	\***************************************************************/
	if (xs->flags & SCSI_ERR_OK) return(ESUCCESS);
	silent = (xs->flags & SCSI_SILENT);

	sense = &(xs->sense);
	switch(sense->error_class)
	{
	case 7:
		{
		key=sense->ext.extended.sense_key;
		switch(key)
		{
		case	0x0:
			return(ESUCCESS);
		case	0x1:
			if(!silent)
			{
				printf("sd%d: soft error(corrected) ", unit); 
				if(sense->valid)
				{
			  		printf("block no. %d (decimal)",
			  		(sense->ext.extended.info[0] <<24)|
			  		(sense->ext.extended.info[1] <<16)|
			  		(sense->ext.extended.info[2] <<8)|
			  		(sense->ext.extended.info[3] ));
				}
				printf("\n");
			}
			return(ESUCCESS);
		case	0x2:
			if(!silent)printf("sd%d: not ready\n ",
				unit); 
			return(ENODEV);
		case	0x3:
			if(!silent)
			{
				printf("sd%d: medium error ", unit); 
				if(sense->valid)
				{
			  		printf("block no. %d (decimal)",
			  		(sense->ext.extended.info[0] <<24)|
			  		(sense->ext.extended.info[1] <<16)|
			  		(sense->ext.extended.info[2] <<8)|
			  		(sense->ext.extended.info[3] ));
				}
				printf("\n");
			}
			return(EIO);
		case	0x4:
			if(!silent)printf("sd%d: non-media hardware failure\n ",
				unit); 
			return(EIO);
		case	0x5:
			if(!silent)printf("sd%d: illegal request\n ",
				unit); 
			return(EINVAL);
		case	0x6:
			/***********************************************\
			* If we are not open, then this is not an error	*
			* as we don't have state yet. Either way, make	*
			* sure that we don't have any residual state	*
			\***********************************************/
			if(!silent)printf("sd%d: Unit attention.\n ", unit); 
			sd_data[unit].flags &= ~(SDVALID | SDHAVELABEL);
			if (sd_data[unit].openparts)
			{
				return(EIO);
			}
			return(ESUCCESS); /* not an error if nothing's open */
		case	0x7:
			if(!silent)
			{
				printf("sd%d: attempted protection violation ",
						unit); 
				if(sense->valid)
			  	{
					printf("block no. %d (decimal)\n",
			  		(sense->ext.extended.info[0] <<24)|
			  		(sense->ext.extended.info[1] <<16)|
			  		(sense->ext.extended.info[2] <<8)|
			  		(sense->ext.extended.info[3] ));
				}
				printf("\n");
			}
			return(EACCES);
		case	0x8:
			if(!silent)
			{
				printf("sd%d: block wrong state (worm)\n ",
				unit); 
				if(sense->valid)
				{
			  		printf("block no. %d (decimal)\n",
			  		(sense->ext.extended.info[0] <<24)|
			  		(sense->ext.extended.info[1] <<16)|
			  		(sense->ext.extended.info[2] <<8)|
			  		(sense->ext.extended.info[3] ));
				}
				printf("\n");
			}
			return(EIO);
		case	0x9:
			if(!silent)printf("sd%d: vendor unique\n",
				unit); 
			return(EIO);
		case	0xa:
			if(!silent)printf("sd%d: copy aborted\n ",
				unit); 
			return(EIO);
		case	0xb:
			if(!silent)printf("sd%d: command aborted\n ",
				unit); 
			return(EIO);
		case	0xc:
			if(!silent)
			{
				printf("sd%d: search returned\n ",
					unit); 
				if(sense->valid)
				{
			  		printf("block no. %d (decimal)\n",
			  		(sense->ext.extended.info[0] <<24)|
			  		(sense->ext.extended.info[1] <<16)|
			  		(sense->ext.extended.info[2] <<8)|
			  		(sense->ext.extended.info[3] ));
				}
				printf("\n");
			}
			return(ESUCCESS);
		case	0xd:
			if(!silent)printf("sd%d: volume overflow\n ",
				unit); 
			return(ENOSPC);
		case	0xe:
			if(!silent)
			{
				printf("sd%d: verify miscompare\n ",
				unit); 
				if(sense->valid)
				{
			  		printf("block no. %d (decimal)\n",
			  		(sense->ext.extended.info[0] <<24)|
			  		(sense->ext.extended.info[1] <<16)|
			  		(sense->ext.extended.info[2] <<8)|
			  		(sense->ext.extended.info[3] ));
				}
				printf("\n");
			}
			return(EIO);
		case	0xf:
			if(!silent)printf("sd%d: unknown error key\n ",
				unit); 
			return(EIO);
		}
		break;
	}
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
		{
			if(!silent)printf("sd%d: error class %d code %d\n",
				unit,
				sense->error_class,
				sense->error_code);
		if(sense->valid)
			if(!silent)printf("block no. %d (decimal)\n",
			(sense->ext.unextended.blockhi <<16)
			+ (sense->ext.unextended.blockmed <<8)
			+ (sense->ext.unextended.blocklow ));
		}
		return(EIO);
	}
}




int
sdsize(dev_t dev)
{
	int unit = UNIT(dev), part = PARTITION(dev), val;
	struct sd_data *sd;

	if (unit >= NSD)
		return(-1);

	sd = &sd_data[unit];
	if((sd->flags & SDINIT) == 0) return(-1);
	if (sd == 0 || (sd->flags & SDHAVELABEL) == 0)
		val = sdopen (MAKESDDEV(major(dev), unit, RAW_PART), FREAD, S_IFBLK, 0);
	if ( val != 0 || sd->flags & SDWRITEPROT)
		return (-1);

	return((int)sd->disklabel.d_partitions[part].p_size);
}

sddump()
{
    printf("sddump()        -- not implemented\n");
    return(-1);
}





