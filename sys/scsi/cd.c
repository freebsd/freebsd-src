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
 *	$Id: cd.c,v 1.10 1993/09/21 05:30:35 rgrimes Exp $
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

long int cdstrats,cdqueues;


#include <ddb.h>
#if	NDDB > 0
int	Debugger();
#else	NDDB > 0
#define Debugger()
#endif	NDDB > 0


#define PAGESIZ 	4096
#define SECSIZE 2048	/* XXX */ /* default only */
#define	CDOUTSTANDING	2
#define CDQSIZE		4
#define	CD_RETRIES	4

#define	UNITSHIFT	3
#define PARTITION(z)	(minor(z) & 0x07)
#define	RAW_PART	3
#define UNIT(z)		(  (minor(z) >> UNITSHIFT) )


extern	int hz;
int	cd_done();
int	cdstrategy();
int	cd_debug = 0;


struct	cd_data
{
	int	flags;
#define	CDVALID		0x02		/* PARAMS LOADED	*/
#define	CDINIT		0x04		/* device has been init'd */
#define	CDWAIT		0x08		/* device has someone waiting */
#define CDHAVELABEL	0x10		/* have read the label */
	struct	scsi_switch *sc_sw;	/* address of scsi low level switch */
	int	ctlr;			/* so they know which one we want */
	int	targ;			/* our scsi target ID */
	int	lu;			/* out scsi lu */
	int	cmdscount;		/* cmds allowed outstanding by board*/
	struct  cd_parms
	{
		int	blksize;
		u_long	disksize;		/* total number sectors */
	}params;
	struct	disklabel	disklabel;
	int	partflags[MAXPARTITIONS];	/* per partition flags */
#define CDOPEN	0x01
	int		openparts;		/* one bit for each open partition */
	int		xfer_block_wait;
	struct	scsi_xfer	*free_xfer;
	struct	scsi_xfer	scsi_xfer[CDOUTSTANDING]; /* XXX */
	struct	buf		buf_queue;
};

#define CD_STOP		0
#define CD_START	1
#define CD_EJECT	-2

struct	cd_driver
{
	int	size;
	struct	cd_data	**cd_data;
}*cd_driver;

static	int	next_cd_unit = 0;
/***********************************************************************\
* The routine called by the low level scsi routine when it discovers	*
* A device suitable for this driver					*
\***********************************************************************/
int	cdattach(ctlr,targ,lu,scsi_switch)
struct	scsi_switch *scsi_switch;
{
	int		unit,i;
	unsigned char	*tbl;
	struct cd_data	*cd, **cdrealloc;
	struct cd_parms	*dp;

#ifdef	CDDEBUG
	if(scsi_debug & PRINTROUTINES) printf("cdattach: "); 
#endif	/*CDDEBUG*/
	/*******************************************************\
	* Check if we have resources allocated yet, if not	*
	* allocate and initialize them				*
	\*******************************************************/
	if (next_cd_unit == 0)
	{
		cd_driver =
			malloc(sizeof(struct cd_driver),M_DEVBUF,M_NOWAIT);
		if(!cd_driver)
		{
			printf("cd%d: malloc failed for cd_driver\n",unit);
			return(0);
		}
		bzero(cd_driver,sizeof(cd_driver));
		cd_driver->size = 0;
	}
	/*******************************************************\
	* allocate the resources for another drive		*
	* if we have already allocate a cd_data pointer we must	*
	* copy the old pointers into a new region that is	*
	* larger and release the old region, aka realloc	*
	\*******************************************************/
	unit = next_cd_unit++;
	/* XXX
	 * This if will always be true for now, but future code may
	 * preallocate more units to reduce overhead.  This would be
	 * done by changing the malloc to be (next_cd_unit * x) and
	 * the cd_driver->size++ to be +x
	 */
	if(unit >= cd_driver->size)
	{
		cdrealloc =
			malloc(sizeof(cd_driver->cd_data) * next_cd_unit,
				M_DEVBUF,M_NOWAIT);
		if(!cdrealloc)
		{
			printf("cd%d: malloc failed for cdrealloc\n",unit);
			return(0);
		}
		/* Make sure we have something to copy before we copy it */
		bzero(cdrealloc,sizeof(cd_driver->cd_data) * next_cd_unit);
		if(cd_driver->size)
		{
			bcopy(cd_driver->cd_data,cdrealloc,
				sizeof(cd_driver->cd_data) * cd_driver->size);
			free(cd_driver->cd_data,M_DEVBUF);
		}
		cd_driver->cd_data = cdrealloc;
		cd_driver->cd_data[unit] = NULL;
		cd_driver->size++;
	}
	if(cd_driver->cd_data[unit])
	{
		printf("cd%d: Already has storage!\n",unit);
		return(0);
	}
	/*******************************************************\
	* allocate the per drive data area			*
	\*******************************************************/
	cd = cd_driver->cd_data[unit] =
		malloc(sizeof(struct cd_data),M_DEVBUF,M_NOWAIT);
	if(!cd)
	{
		printf("cd%d: malloc failed for cd_data\n",unit);
		return(0);
	}
	bzero(cd,sizeof(struct cd_data));
	dp  = &(cd->params);
	/*******************************************************\
	* Store information needed to contact our base driver	*
	\*******************************************************/
	cd->sc_sw	=	scsi_switch;
	cd->ctlr	=	ctlr;
	cd->targ	=	targ;
	cd->lu		=	lu;
	cd->cmdscount =	CDOUTSTANDING; /* XXX (ask the board) */

	i = cd->cmdscount;
	while(i-- )
	{
		cd->scsi_xfer[i].next = cd->free_xfer;
		cd->free_xfer = &cd->scsi_xfer[i];
	}
	/*******************************************************\
	* Use the subdriver to request information regarding	*
	* the drive. We cannot use interrupts yet, so the	*
	* request must specify this.				*
	\*******************************************************/
	cd_get_parms(unit,  SCSI_NOSLEEP |  SCSI_NOMASK);
	if(dp->disksize)
	{
		printf("cd%d: cd present.[%d x %d byte records]\n",
				unit,
				cd->params.disksize,
				cd->params.blksize);
	}
	else
	{
		printf("cd%d: drive empty\n", unit);
	}
	cd->flags |= CDINIT;
	return;
}

/*******************************************************\
*	open the device. Make sure the partition info	*
* is a up-to-date as can be.				*
\*******************************************************/
cdopen(dev)
{
	int errcode = 0;
	int unit, part;
	struct cd_parms cd_parms;
	struct cd_data *cd;

	unit = UNIT(dev);
	part = PARTITION(dev);

#ifdef	CDDEBUG
	if(scsi_debug & (PRINTROUTINES | TRACEOPENS))
		printf("cdopen: dev=0x%x (unit %d (of %d),partition %d)\n"
			,dev,unit,cd_driver->size,part);
#endif	/*CDDEBUG*/
	/*******************************************************\
	* Check the unit is legal				*
	\*******************************************************/
	if ( unit >= cd_driver->size )
	{
		return(ENXIO);
	}
	cd = cd_driver->cd_data[unit];
	/*******************************************************\
	* Make sure the device has been initialised		*
	\*******************************************************/
	if ((cd == NULL) || (!(cd->flags & CDINIT)))
		return(ENXIO);

	/*******************************************************\
	* If it's been invalidated, and not everybody has	*
	* closed it then forbid re-entry.			*
	* 	(may have changed media)			*
	\*******************************************************/
	if ((! (cd->flags & CDVALID))
	   && ( cd->openparts))
		return(ENXIO);

	/*******************************************************\
	* Check that it is still responding and ok.		*
	* if the media has been changed this will result in a	*
	* "unit attention" error which the error code will	*
	* disregard because the CDVALID flag is not yet set	*
	\*******************************************************/
	cd_test_ready(unit, SCSI_SILENT);

	/*******************************************************\
	* Next time actually take notice of error returns	*
	\*******************************************************/
	if (cd_test_ready(unit, SCSI_SILENT) != 0) {
#ifdef	CDDEBUG
		if(scsi_debug & TRACEOPENS)
			printf("not ready\n");
#endif	/*CDDEBUG*/
		return(ENXIO);
	}
#ifdef	CDDEBUG
	if(scsi_debug & TRACEOPENS)
		printf("Device present\n");
#endif	/*CDDEBUG*/
	/*******************************************************\
	* In case it is a funny one, tell it to start		*
	* not needed for some drives				*
	\*******************************************************/
	cd_start_unit(unit,part,CD_START);
        cd_prevent_unit(unit,PR_PREVENT,SCSI_SILENT);
#ifdef	CDDEBUG
	if(scsi_debug & TRACEOPENS)
		printf("started ");
#endif	/*CDDEBUG*/
	/*******************************************************\
	* Load the physical device parameters 			*
	\*******************************************************/
	cd_get_parms(unit, 0);
#ifdef	CDDEBUG
	if(scsi_debug & TRACEOPENS)
		printf("Params loaded ");
#endif	/*CDDEBUG*/
	/*******************************************************\
	* Load the partition info if not already loaded		*
	\*******************************************************/
	cdgetdisklabel(unit);
#ifdef	CDDEBUG
	if(scsi_debug & TRACEOPENS)
		printf("Disklabel fabricated ");
#endif	/*CDDEBUG*/
	/*******************************************************\
	* Check the partition is legal				*
	\*******************************************************/
	if (( part >= cd->disklabel.d_npartitions ) 
		&& (part != RAW_PART))
	{
#ifdef	CDDEBUG
		if(scsi_debug & TRACEOPENS)
			printf("partition %d > %d\n",part
				,cd->disklabel.d_npartitions);
#endif	/*CDDEBUG*/
        	cd_prevent_unit(unit,PR_ALLOW,SCSI_SILENT);
		return(ENXIO);
	}
	/*******************************************************\
	*  Check that the partition exists			*
	\*******************************************************/
	if (( cd->disklabel.d_partitions[part].p_fstype != FS_UNUSED )
		|| (part == RAW_PART))
	{
		cd->partflags[part] |= CDOPEN;
		cd->openparts |= (1 << part);
#ifdef	CDDEBUG
		if(scsi_debug & TRACEOPENS)
			printf("open complete\n");
#endif	/*CDDEBUG*/
		cd->flags |= CDVALID;
	}
	else
	{
#ifdef	CDDEBUG
		if(scsi_debug & TRACEOPENS)
			printf("part %d type UNUSED\n",part);
#endif	/*CDDEBUG*/
        	cd_prevent_unit(unit,PR_ALLOW,SCSI_SILENT);
		return(ENXIO);
	}
	return(0);
}

/*******************************************************\
* Get ownership of a scsi_xfer structure		*
* If need be, sleep on it, until it comes free		*
\*******************************************************/
struct scsi_xfer *cd_get_xs(unit,flags)
int	flags;
int	unit;
{
	struct scsi_xfer *xs;
	struct cd_data *cd;
	int	s;

	cd = cd_driver->cd_data[unit];
	if(flags & (SCSI_NOSLEEP |  SCSI_NOMASK))
	{
		if (xs = cd->free_xfer)
		{
			cd->free_xfer = xs->next;
			xs->flags = 0;
		}
	}
	else
	{
		s = SPLCD();
		while (!(xs = cd->free_xfer))
		{
			cd->xfer_block_wait++;  /* someone waiting! */
			sleep((caddr_t)&cd->free_xfer, PRIBIO+1);
			cd->xfer_block_wait--;
		}
		cd->free_xfer = xs->next;
		splx(s);
		xs->flags = 0;
	}
	return(xs);
}

/*******************************************************\
* Free a scsi_xfer, wake processes waiting for it	*
\*******************************************************/
cd_free_xs(unit,xs,flags)
struct scsi_xfer *xs;
int	unit;
int	flags;
{
	struct cd_data *cd;
	int	s;
	
	cd = cd_driver->cd_data[unit];
	if(flags & SCSI_NOMASK)
	{
		if (cd->xfer_block_wait)
		{
			printf("cd%d: doing a wakeup from NOMASK mode\n", unit);
			wakeup((caddr_t)&cd->free_xfer);
		}
		xs->next = cd->free_xfer;
		cd->free_xfer = xs;
	}
	else
	{
		s = SPLCD();
		if (cd->xfer_block_wait)
			wakeup((caddr_t)&cd->free_xfer);
		xs->next = cd->free_xfer;
		cd->free_xfer = xs;
		splx(s);
	}
}

/*******************************************************\
* trim the size of the transfer if needed,		*
* called by physio					*
* basically the smaller of our max and the scsi driver's*
* minphys (note we have no max ourselves)		*
\*******************************************************/
/* Trim buffer length if buffer-size is bigger than page size */
void	cdminphys(bp)
struct buf	*bp;
{
	(*(cd_driver->cd_data[UNIT(bp->b_dev)]->sc_sw->scsi_minphys))(bp);
}

/*******************************************************\
* Actually translate the requested transfer into	*
* one the physical driver can understand		*
* The transfer is described by a buf and will include	*
* only one physical transfer.				*
\*******************************************************/

int	cdstrategy(bp)
struct	buf	*bp;
{
	struct	buf	*dp;
	unsigned int opri;
	struct cd_data *cd;
	int	unit;

	cdstrats++;
	unit = UNIT((bp->b_dev));
	cd = cd_driver->cd_data[unit];
#ifdef	CDDEBUG
	if(scsi_debug & PRINTROUTINES) printf("\ncdstrategy ");
	if(scsi_debug & SHOWREQUESTS) printf("cd%d: %d bytes @ blk%d\n",
					unit,bp->b_bcount,bp->b_blkno);
#endif	/*CDDEBUG*/
	cdminphys(bp);
	/*******************************************************\
	* If the device has been made invalid, error out	*
	* maybe the media changed				*
	\*******************************************************/
	if(!(cd->flags & CDVALID))
	{
		bp->b_error = EIO;
		goto bad;
	}
	/*******************************************************\
	* can't ever write to a CD				*
	\*******************************************************/
	if ((bp->b_flags & B_READ) == 0) {
		bp->b_error = EROFS;
		goto bad;
	}
	/*******************************************************\
	* If it's a null transfer, return immediatly		*
	\*******************************************************/
	if (bp->b_bcount == 0) {
		goto done;
	}

	/*******************************************************\
	* Decide which unit and partition we are talking about	*
	\*******************************************************/
 	if(PARTITION(bp->b_dev) != RAW_PART)
	{
		if (!(cd->flags & CDHAVELABEL))
		{
			bp->b_error = EIO;
			goto bad;
		}
		/*
		 * do bounds checking, adjust transfer. if error, process.
		 * if end of partition, just return
		 */
		if (bounds_check_with_label(bp,&cd->disklabel,1) <= 0)
			goto done;
		/* otherwise, process transfer request */
	}

	opri = SPLCD();
	dp = &cd->buf_queue;

	/*******************************************************\
	* Place it in the queue of disk activities for this disk*
	\*******************************************************/
	disksort(dp, bp);

	/*******************************************************\
	* Tell the device to get going on the transfer if it's	*
	* not doing anything, otherwise just wait for completion*
	\*******************************************************/
	cdstart(unit);

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
* cdstart looks to see if there is a buf waiting for the device	*
* and that the device is not already busy. If both are true,	*
* It deques the buf and creates a scsi command to perform the	*
* transfer in the buf. The transfer request will call cd_done	*
* on completion, which will in turn call this routine again	*
* so that the next queued transfer is performed.		*
* The bufs are queued by the strategy routine (cdstrategy)	*
*								*
* This routine is also called after other non-queued requests	*
* have been made of the scsi driver, to ensure that the queue	*
* continues to be drained.					*
*								*
* must be called at the correct (highish) spl level		*
\***************************************************************/
/* cdstart() is called at SPLCD  from cdstrategy and cd_done*/
cdstart(unit)
int	unit;
{
	register struct buf	*bp = 0;
	register struct buf	*dp;
	struct	scsi_xfer	*xs;
	struct	scsi_rw_big	cmd;
	int			blkno, nblk;
	struct cd_data *cd;
	struct partition *p ;

#ifdef	CDDEBUG
	if(scsi_debug & PRINTROUTINES) printf("cdstart%d ",unit);
#endif	/*CDDEBUG*/
	cd = cd_driver->cd_data[unit];
	/*******************************************************\
	* See if there is a buf to do and we are not already	*
	* doing one						*
	\*******************************************************/
	if(!cd->free_xfer)
	{
		return;    /* none for us, unit already underway */
	}

	if(cd->xfer_block_wait)    /* there is one, but a special waits */
	{
		return;	/* give the special that's waiting a chance to run */
	}


	dp = &cd->buf_queue;
	if ((bp = dp->b_actf) != NULL)	/* yes, an assign */
	{
		dp->b_actf = bp->av_forw;
	}
	else
	{ 
		return;
	}

	xs=cd_get_xs(unit,0);	/* ok we can grab it */
	xs->flags = INUSE;    /* Now ours */
	/***************************************************************\
	* Should reject all queued entries if CDVALID is not true	*
	\***************************************************************/
	if(!(cd->flags & CDVALID))
	{
		goto bad; /* no I/O.. media changed or something */
	}

	/*******************************************************\
	* We have a buf, now we should move the data into	*
	* a scsi_xfer definition and try start it		*
	*							*
	*  First, translate the block to absolute		*
	* and put it in terms of the logical blocksize of the	*
	* device..						*
	* really a bit silly until we have real partitions, but.*
	\*******************************************************/
	blkno = bp->b_blkno / (cd->params.blksize/512);
	if(PARTITION(bp->b_dev) != RAW_PART)
	{
		p = cd->disklabel.d_partitions + PARTITION(bp->b_dev);
		blkno += p->p_offset;
	}
	nblk = (bp->b_bcount + (cd->params.blksize - 1)) / (cd->params.blksize);
	/* what if something asks for 512 bytes not on a 2k boundary? *//*XXX*/

	/*******************************************************\
	*  Fill out the scsi command				*
	\*******************************************************/
	bzero(&cmd, sizeof(cmd));
	cmd.op_code	=	READ_BIG;
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
	xs->adapter	=	cd->ctlr;
	xs->targ	=	cd->targ;
	xs->lu		=	cd->lu;
	xs->retries	=	CD_RETRIES;
	xs->timeout	=	10000;/* 10000 millisecs for a disk !*/
	xs->cmd		=	(struct	scsi_generic *)&cmd;
	xs->cmdlen	=	sizeof(cmd);
	xs->resid	=	bp->b_bcount;
	xs->when_done	=	cd_done;
	xs->done_arg	=	unit;
	xs->done_arg2	=	(int)xs;
	xs->error	=	XS_NOERROR;
	xs->bp		=	bp;
	xs->data	=	(u_char *)bp->b_un.b_addr;
	xs->datalen	=	bp->b_bcount;

	/*******************************************************\
	* Pass all this info to the scsi driver.		*
	\*******************************************************/
	if ( (*(cd->sc_sw->scsi_cmd))(xs) != SUCCESSFULLY_QUEUED)
	{
		printf("cd%d: oops not queued",unit);
		goto bad;
	}	
	cdqueues++;
	return;
bad:	xs->error = XS_DRIVER_STUFFUP;
	cd_done(unit,xs);
}

/*******************************************************\
* This routine is called by the scsi interrupt when	*
* the transfer is complete. (or failed)			*
\*******************************************************/
int	cd_done(unit,xs)
int	unit;
struct	scsi_xfer	*xs;
{
	struct	buf		*bp;
	int	retval;
	struct cd_data *cd = cd_driver->cd_data[unit];

#ifdef	CDDEBUG
	if(scsi_debug & PRINTROUTINES) printf("cd_done%d ",unit);
#endif	/*CDDEBUG*/
#ifdef	PARANOIA
	if (! (xs->flags & INUSE)) 	/* paranoia always pays off */
		panic("scsi_xfer not in use!");
#endif	/*PARANOIA*/
	if(!(bp = xs->bp))
	{
		wakeup(xs);
		return 0;
	}
	switch(xs->error)
	{
	case	XS_NOERROR:
		bp->b_error = 0;
		bp->b_resid = 0;
		break;

	case	XS_SENSE:
		retval = (cd_interpret_sense(unit,xs));
		if(retval)
		{
			bp->b_flags |= B_ERROR;
			bp->b_error = retval;
		}
		break;

	case	XS_TIMEOUT:
		printf("cd%d timeout\n",unit);

	case	XS_BUSY:	
		/***********************************\
		* Just resubmit it straight back to *
		* the SCSI driver to try it again   *
		\***********************************/
		if(xs->retries--)
		{
			xs->error = XS_NOERROR;
			xs->flags &= ~ITSDONE;
			if ((*(cd->sc_sw->scsi_cmd))(xs)
				== SUCCESSFULLY_QUEUED)
			{	/* shhh! don't wake the job, ok? */
				/* don't tell cdstart either, */
				return 0;
			}
			/* xs->error is set by the scsi driver */
		} /* Fall through */

	case	XS_DRIVER_STUFFUP:
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
		break;
	default:
		printf("cd%d: unknown error category from scsi driver\n"
			,unit);
	}	
	biodone(bp);
	cd_free_xs(unit,xs,0);
	cdstart(unit);	/* If there's anything waiting.. do it */
	return 0;
}

/*******************************************************\
* Perform special action on behalf of the user		*
* Knows about the internals of this device		*
\*******************************************************/
cdioctl(dev_t dev, int cmd, caddr_t addr, int flag)
{
	int error = 0;
	unsigned int opri;
	unsigned char unit, part;
	register struct cd_data *cd;


	/*******************************************************\
	* Find the device that the user is talking about	*
	\*******************************************************/
	unit = UNIT(dev);
	part = PARTITION(dev);
	cd = cd_driver->cd_data[unit];
#ifdef	CDDEBUG
	if(scsi_debug & PRINTROUTINES) printf("cdioctl%d ",unit);
#endif	/*CDDEBUG*/

	/*******************************************************\
	* If the device is not valid.. abandon ship		*
	\*******************************************************/
	if (!(cd_driver->cd_data[unit]->flags & CDVALID))
		return(EIO);
	switch(cmd)
	{

	case DIOCSBAD:
                        error = EINVAL;
		break;

	case DIOCGDINFO:
		*(struct disklabel *)addr = cd->disklabel;
		break;

        case DIOCGPART:
                ((struct partinfo *)addr)->disklab = &cd->disklabel;
                ((struct partinfo *)addr)->part =
                    &cd->disklabel.d_partitions[PARTITION(dev)];
                break;

        case DIOCWDINFO:
        case DIOCSDINFO:
                if ((flag & FWRITE) == 0)
                        error = EBADF;
                else
                        error = setdisklabel(&cd->disklabel,
					(struct disklabel *)addr,
                         /*(cd->flags & DKFL_BSDLABEL) ? cd->openparts : */0,
				0);
                if (error == 0) {
			cd->flags |= CDHAVELABEL;
		}
                break;

        case DIOCWLABEL:
                error = EBADF;
                break;

	case CDIOCPLAYTRACKS:
		{
			struct	ioc_play_track *args
					= (struct  ioc_play_track *)addr;
			struct	cd_mode_data data;
			if(error = cd_get_mode(unit,&data,AUDIO_PAGE))
				break;
			data.page.audio.flags &= ~CD_PA_SOTC;
			data.page.audio.flags |= CD_PA_IMMED;
			if(error = cd_set_mode(unit,&data))
				break;
			return(cd_play_tracks(unit
						,args->start_track
						,args->start_index
						,args->end_track
						,args->end_index
						));
		}
		break;
	case CDIOCPLAYMSF:
		{
			struct	ioc_play_msf *args
					= (struct  ioc_play_msf *)addr;
			struct	cd_mode_data data;
			if(error = cd_get_mode(unit,&data,AUDIO_PAGE))
				break;
			data.page.audio.flags &= ~CD_PA_SOTC;
			data.page.audio.flags |= CD_PA_IMMED;
			if(error = cd_set_mode(unit,&data))
				break;
			return(cd_play_msf(unit
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
			struct	ioc_play_blocks *args
					= (struct  ioc_play_blocks *)addr;
			struct	cd_mode_data data;
			if(error = cd_get_mode(unit,&data,AUDIO_PAGE))
				break;
			data.page.audio.flags &= ~CD_PA_SOTC;
			data.page.audio.flags |= CD_PA_IMMED;
			if(error = cd_set_mode(unit,&data))
				break;
			return(cd_play(unit,args->blk,args->len));


		}
		break;
	case CDIOCREADSUBCHANNEL:
		{
			struct ioc_read_subchannel *args
					= (struct ioc_read_subchannel *)addr;
			struct cd_sub_channel_info data;
			int len=args->data_len;
			if(len>sizeof(data)||
			   len<sizeof(struct cd_sub_channel_header)) {
				error=EINVAL;
				break;
			}
			if(error = cd_read_subchannel(unit,args->address_format,
					args->data_format,args->track,&data,len)) {
				break;
			}
			len=MIN(len,((data.header.data_len[0]<<8)+data.header.data_len[1]+
					sizeof(struct cd_sub_channel_header)));
			if(copyout(&data,args->data,len)!=0) {
				error=EFAULT;
			}
		}
		break;
	case CDIOREADTOCHEADER:
		{
			struct ioc_toc_header th;
			if( error = cd_read_toc(unit,0,0,&th,sizeof(th)))
				break;
			th.len=(th.len&0xff)<<8+((th.len>>8)&0xff);
			bcopy(&th,addr,sizeof(th));
		}
		break;
	case CDIOREADTOCENTRYS:
		{
			struct ioc_read_toc_entry *te=	
					(struct ioc_read_toc_entry *)addr;
			struct cd_toc_entry data[65];
			struct ioc_toc_header *th;
			int len=te->data_len;
			th=(struct ioc_toc_header *)data;

                        if(len>sizeof(data) || len<sizeof(struct cd_toc_entry)) {
                                error=EINVAL;
                                break;
                        }
			if(error = cd_read_toc(unit,te->address_format,
						    te->starting_track,
						    data,
						    len))
				break;
			len=MIN(len,((((th->len&0xff)<<8)+((th->len>>8)))+
								sizeof(*th)));
			if(copyout(th,te->data,len)!=0) {
				error=EFAULT;
			}
			
		}
		break;
	case CDIOCSETPATCH:
		{
			struct ioc_patch *arg = (struct ioc_patch *)addr;
			struct	cd_mode_data data;
			if(error = cd_get_mode(unit,&data,AUDIO_PAGE))
				break;
			data.page.audio.port[LEFT_PORT].channels = arg->patch[0];
			data.page.audio.port[RIGHT_PORT].channels = arg->patch[1];
			data.page.audio.port[2].channels = arg->patch[2];
			data.page.audio.port[3].channels = arg->patch[3];
			if(error = cd_set_mode(unit,&data))
				break;
		}
		break;
	case CDIOCGETVOL:
		{
			struct ioc_vol *arg = (struct ioc_vol *)addr;
			struct	cd_mode_data data;
			if(error = cd_get_mode(unit,&data,AUDIO_PAGE))
				break;
			arg->vol[LEFT_PORT] = data.page.audio.port[LEFT_PORT].volume;
			arg->vol[RIGHT_PORT] = data.page.audio.port[RIGHT_PORT].volume;
			arg->vol[2] = data.page.audio.port[2].volume;
			arg->vol[3] = data.page.audio.port[3].volume;
		}
		break;
	case CDIOCSETVOL:
		{
			struct ioc_vol *arg = (struct ioc_vol *)addr;
			struct	cd_mode_data data;
			if(error = cd_get_mode(unit,&data,AUDIO_PAGE))
				break;
			data.page.audio.port[LEFT_PORT].volume = arg->vol[LEFT_PORT];
			data.page.audio.port[RIGHT_PORT].volume = arg->vol[RIGHT_PORT];
			data.page.audio.port[2].volume = arg->vol[2];
			data.page.audio.port[3].volume = arg->vol[3];
			if(error = cd_set_mode(unit,&data))
				break;
		}
		break;
	case CDIOCSETMONO:
		{
			struct ioc_vol *arg = (struct ioc_vol *)addr;
			struct	cd_mode_data data;
			if(error = cd_get_mode(unit,&data,AUDIO_PAGE))
				break;
			data.page.audio.port[LEFT_PORT].channels = LEFT_CHANNEL|RIGHT_CHANNEL|4|8;
			data.page.audio.port[RIGHT_PORT].channels = LEFT_CHANNEL|RIGHT_CHANNEL;
			data.page.audio.port[2].channels = 0;
			data.page.audio.port[3].channels = 0;
			if(error = cd_set_mode(unit,&data))
				break;
		}
		break;
	case CDIOCSETSTERIO:
		{
			struct ioc_vol *arg = (struct ioc_vol *)addr;
			struct	cd_mode_data data;
			if(error = cd_get_mode(unit,&data,AUDIO_PAGE))
				break;
			data.page.audio.port[LEFT_PORT].channels = LEFT_CHANNEL;
			data.page.audio.port[RIGHT_PORT].channels = RIGHT_CHANNEL;
			data.page.audio.port[2].channels = 0;
			data.page.audio.port[3].channels = 0;
			if(error = cd_set_mode(unit,&data))
				break;
		}
		break;
	case CDIOCSETMUTE:
		{
			struct ioc_vol *arg = (struct ioc_vol *)addr;
			struct	cd_mode_data data;
			if(error = cd_get_mode(unit,&data,AUDIO_PAGE))
				break;
			data.page.audio.port[LEFT_PORT].channels = 0;
			data.page.audio.port[RIGHT_PORT].channels = 0;
			data.page.audio.port[2].channels = 0;
			data.page.audio.port[3].channels = 0;
			if(error = cd_set_mode(unit,&data))
				break;
		}
		break;
	case CDIOCSETLEFT:
		{
			struct ioc_vol *arg = (struct ioc_vol *)addr;
			struct	cd_mode_data data;
			if(error = cd_get_mode(unit,&data,AUDIO_PAGE))
				break;
			data.page.audio.port[LEFT_PORT].channels = LEFT_CHANNEL;
			data.page.audio.port[RIGHT_PORT].channels = LEFT_CHANNEL;
			data.page.audio.port[2].channels = 0;
			data.page.audio.port[3].channels = 0;
			if(error = cd_set_mode(unit,&data))
				break;
		}
		break;
	case CDIOCSETRIGHT:
		{
			struct ioc_vol *arg = (struct ioc_vol *)addr;
			struct	cd_mode_data data;
			if(error = cd_get_mode(unit,&data,AUDIO_PAGE))
				break;
			data.page.audio.port[LEFT_PORT].channels = RIGHT_CHANNEL;
			data.page.audio.port[RIGHT_PORT].channels = RIGHT_CHANNEL;
			data.page.audio.port[2].channels = 0;
			data.page.audio.port[3].channels = 0;
			if(error = cd_set_mode(unit,&data))
				break;
		}
		break;
	case CDIOCRESUME:
		error = cd_pause(unit,1);
		break;
	case CDIOCPAUSE:
		error = cd_pause(unit,0);
		break;
	case CDIOCSTART:
		error = cd_start_unit(unit,part,CD_START);
		break;
	case CDIOCSTOP:
		error = cd_start_unit(unit,part,CD_STOP);
		break;
	case CDIOCEJECT:
		error = cd_start_unit(unit,part,CD_EJECT);
		break;
	case CDIOCSETDEBUG:
		scsi_debug = 0xfff; cd_debug = 0xfff;
		break;
	case CDIOCCLRDEBUG:
		scsi_debug = 0; cd_debug = 0;
		break;
	case CDIOCRESET:
		return(cd_reset(unit));
		break;
	default:
		error = ENOTTY;
		break;
	}
	return (error);
}


/*******************************************************\
* Load the label information on the named device	*
* 							*
* EVENTUALLY take information about different		*
* data tracks from the TOC and put it in the disklabel	*
\*******************************************************/
int cdgetdisklabel(unit)
unsigned char	unit;
{
	/*unsigned int n, m;*/
	char *errstring;
	struct dos_partition *dos_partition_p;
	struct cd_data *cd;

	cd = cd_driver->cd_data[unit];
	/*******************************************************\
	* If the info is already loaded, use it			*
	\*******************************************************/
	if(cd->flags & CDHAVELABEL) return;

	bzero(&cd->disklabel,sizeof(struct disklabel));
	/*******************************************************\
	* make partition 3 the whole disk in case of failure	*
  	*   then get pdinfo 					*
	\*******************************************************/
	strncpy(cd->disklabel.d_typename,"scsi cd_rom",16);
	strncpy(cd->disklabel.d_packname,"ficticious",16);
	cd->disklabel.d_secsize = cd->params.blksize; /* as long as it's not 0 */
	cd->disklabel.d_nsectors = 100;
	cd->disklabel.d_ntracks = 1;
	cd->disklabel.d_ncylinders = (cd->params.disksize / 100) + 1;
	cd->disklabel.d_secpercyl = 100;
	cd->disklabel.d_secperunit = cd->params.disksize;
	cd->disklabel.d_rpm = 300;
	cd->disklabel.d_interleave = 1;
	cd->disklabel.d_flags = D_REMOVABLE;

	cd->disklabel.d_npartitions = 1;
	 cd->disklabel.d_partitions[0].p_offset = 0;
	 cd->disklabel.d_partitions[0].p_size 
	 	= cd->params.disksize * (cd->params.blksize / 512);
	 cd->disklabel.d_partitions[0].p_fstype = 9;

	cd->disklabel.d_magic = DISKMAGIC;
	cd->disklabel.d_magic2 = DISKMAGIC;
	cd->disklabel.d_checksum = dkcksum(&(cd->disklabel));

	/*******************************************************\
	* Signal to other users and routines that we now have a *
	* disklabel that represents the media (maybe)		*
	\*******************************************************/
	cd->flags |= CDHAVELABEL;
	return(ESUCCESS);
}

/*******************************************************\
* Get scsi driver to send a "are you ready" command	*
\*******************************************************/
cd_test_ready(unit,flags)
int	unit,flags;
{
	struct	scsi_test_unit_ready scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = TEST_UNIT_READY;

	return (cd_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			0,
			0,
			100000,
			NULL,
			flags));
}


/*******************************************************\
* Find out form the device what it's capacity is	*
\*******************************************************/
cd_size(unit, flags)
{
	struct	scsi_read_cd_cap_data	rdcap;
	struct	scsi_read_cd_capacity	scsi_cmd;
	int size;
	int	blksize;

	/*******************************************************\
	* make up a scsi command and ask the scsi driver to do	*
	* it for you.						*
	\*******************************************************/
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = READ_CD_CAPACITY;

	/*******************************************************\
	* If the command works, interpret the result as a 4 byte*
	* number of blocks					*
	\*******************************************************/
	if (cd_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			&rdcap,
			sizeof(rdcap),  
			2000,
			NULL,
			flags) != 0)
	{
		printf("cd%d: could not get size\n", unit);
		return(0);
	} else {
		size = rdcap.addr_0 + 1 ;
		size += rdcap.addr_1 << 8;
		size += rdcap.addr_2 << 16;
		size += rdcap.addr_3 << 24;
		blksize  = rdcap.length_0 ;
		blksize += rdcap.length_1 << 8;
		blksize += rdcap.length_2 << 16;
		blksize += rdcap.length_3 << 24;
	}
#ifdef	CDDEBUG
	if(cd_debug)printf("cd%d: %d %d byte blocks\n",unit,size,blksize);
#endif	/*CDDEBUG*/
	cd_driver->cd_data[unit]->params.disksize = size;
	cd_driver->cd_data[unit]->params.blksize = blksize;
	return(size);
}
	

/*******************************************************\
* Get the requested page into the buffer given		*
\*******************************************************/
cd_get_mode(unit,data,page)
int	unit;
struct	cd_mode_data *data;
int	page;
{
	struct scsi_mode_sense scsi_cmd;
	int	retval;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	bzero(data,sizeof(*data));
	scsi_cmd.op_code = MODE_SENSE;
	scsi_cmd.page = page;
	scsi_cmd.length = sizeof(*data) & 0xff;
	retval = cd_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			data,
			sizeof(*data),
			20000,	/* should be immed */
			NULL,
			0);
	return (retval);
}
/*******************************************************\
* Get the requested page into the buffer given		*
\*******************************************************/
cd_set_mode(unit,data)
int	unit;
struct	cd_mode_data *data;
{
	struct scsi_mode_select scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = MODE_SELECT;
	scsi_cmd.byte2 |= SMS_PF;
	scsi_cmd.length = sizeof(*data) & 0xff;
	data->header.data_length = 0;
	/*show_mem(data,sizeof(*data));*/
	return (cd_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			data,
			sizeof(*data),
			20000,	/* should be immed */
			NULL,
			0)
	); 
}
/*******************************************************\
* Get scsi driver to send a "start playing" command	*
\*******************************************************/
cd_play(unit,blk,len)
int	unit,blk,len;
{
	struct scsi_play scsi_cmd;
	int	retval;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = PLAY;
	scsi_cmd.blk_addr[0] = (blk >> 24) & 0xff;
	scsi_cmd.blk_addr[1] = (blk >> 16) & 0xff;
	scsi_cmd.blk_addr[2] = (blk >> 8) & 0xff;
	scsi_cmd.blk_addr[3] = blk & 0xff;
	scsi_cmd.xfer_len[0] = (len >> 8) & 0xff;
	scsi_cmd.xfer_len[1] = len & 0xff;
	retval = cd_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			0,
			0,
			200000,	/* should be immed */
			NULL,
			0);
	return(retval);
}
/*******************************************************\
* Get scsi driver to send a "start playing" command	*
\*******************************************************/
cd_play_big(unit,blk,len)
int	unit,blk,len;
{
	struct scsi_play_big scsi_cmd;
	int	retval;

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
	retval = cd_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			0,
			0,
			20000,	/* should be immed */
			NULL,
			0);
	return(retval);
}
/*******************************************************\
* Get scsi driver to send a "start playing" command	*
\*******************************************************/
cd_play_tracks(unit,strack,sindex,etrack,eindex)
int	unit,strack,sindex,etrack,eindex;
{
	struct scsi_play_track scsi_cmd;
	int	retval;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = PLAY_TRACK;
	scsi_cmd.start_track = strack;
	scsi_cmd.start_index = sindex;
	scsi_cmd.end_track = etrack;
	scsi_cmd.end_index = eindex;
	retval = cd_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			0,
			0,
			20000,	/* should be immed */
			NULL,
			0);
	return(retval);
}
/*******************************************************\
* Get scsi driver to send a "play msf" command		*
\*******************************************************/
cd_play_msf(unit,startm,starts,startf,endm,ends,endf)
int	unit,startm,starts,startf,endm,ends,endf;
{
	struct scsi_play_msf scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = PLAY_MSF;
	scsi_cmd.start_m=startm;
	scsi_cmd.start_s=starts;
	scsi_cmd.start_f=startf;
	scsi_cmd.end_m=endm;
	scsi_cmd.end_s=ends;
	scsi_cmd.end_f=endf;

	return (cd_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			0,
			0,
			2000,
			NULL,
			0));
}
/*******************************************************\
* Get scsi driver to send a "start up" command		*
\*******************************************************/
cd_pause(unit,go)
int	unit,go;
{
	struct scsi_pause scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = PAUSE;
	scsi_cmd.resume = go;

	return (cd_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			0,
			0,
			2000,
			NULL,
			0));
}
/*******************************************************\
* Get scsi driver to send a "RESET" command		*
\*******************************************************/
cd_reset(unit)
int	unit;
{
	return(cd_scsi_cmd(unit,
			0,
			0,
			0,
			0,
			2000,
			NULL,
			SCSI_RESET));
}
/*******************************************************\
* Get scsi driver to send a "start up" command		*
\*******************************************************/
cd_start_unit(unit,part,type)
{
	struct scsi_start_stop scsi_cmd;
	struct cd_data *cd = cd_driver->cd_data[unit];

        if(type==CD_EJECT
	/*&& (cd->openparts  == 0)*/)/* trouble is WE have it open *//*XXX*/
	{
		cd_prevent_unit(unit,PR_ALLOW,0);
	}

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = START_STOP;
	scsi_cmd.how |= (type==CD_START)?SSS_START:0;
	scsi_cmd.how |= (type==CD_EJECT)?SSS_LOEJ:0;

	if (cd_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			0,
			0,
			2000,
			NULL,
			0) != 0) {
		return(ENXIO);
	} else 
		return(0);
}
/*******************************************************\
* Prevent or allow the user to remove the disk          *
\*******************************************************/
cd_prevent_unit(unit,type,flags)
int     unit,type,flags;
{
        struct  scsi_prevent    scsi_cmd;
	struct cd_data *cd = cd_driver->cd_data[unit];

        if(type==PR_PREVENT
	|| ( type==PR_ALLOW && cd->openparts == 0 ))/*XXX*/
	{
                bzero(&scsi_cmd, sizeof(scsi_cmd));
                scsi_cmd.op_code = PREVENT_ALLOW;
                scsi_cmd.how = type;
                if (cd_scsi_cmd(unit,
                        &scsi_cmd,
                        sizeof(struct   scsi_prevent),
                        0,
                        0,
                        5000,
			NULL,
                        0) != 0)
                {
                 if(!(flags & SCSI_SILENT))
                         printf("cd%d: cannot prevent/allow\n", unit);
                 return(0);
                }
        }
        return(1);
}

/******************************************************\
* Read Subchannel				       *
\******************************************************/

cd_read_subchannel(unit,mode,format,track,data,len)
int unit,mode,format,len;
struct cd_sub_channel_info *data;
{
	struct scsi_read_subchannel scsi_cmd;
	int error;

	bzero(&scsi_cmd,sizeof(scsi_cmd));

	scsi_cmd.op_code=READ_SUBCHANNEL;
        if(mode==CD_MSF_FORMAT)
		scsi_cmd.byte2 |= CD_MSF;
	scsi_cmd.byte3=SRS_SUBQ;
	scsi_cmd.subchan_format=format;
	scsi_cmd.track=track;
	scsi_cmd.data_len[0]=(len)>>8;
	scsi_cmd.data_len[1]=(len)&0xff;
	return cd_scsi_cmd(unit,
		&scsi_cmd,
		sizeof(struct   scsi_read_subchannel),
		data,
		len,
		5000,
		NULL,
		0);
}

/*******************************************************\
* Read Table of contents                                *
\*******************************************************/
cd_read_toc(unit,mode,start,data,len)
int unit,mode,start,len;
struct cd_toc_entry *data;
{
	struct scsi_read_toc scsi_cmd;
	int error;
	int ntoc;
	
	bzero(&scsi_cmd,sizeof(scsi_cmd));
	/*if(len!=sizeof(struct ioc_toc_header))
	   ntoc=((len)-sizeof(struct ioc_toc_header))/sizeof(struct cd_toc_entry);
	  else*/
	   ntoc=len;

	scsi_cmd.op_code=READ_TOC;
        if(mode==CD_MSF_FORMAT)
                scsi_cmd.byte2 |= CD_MSF;
	scsi_cmd.from_track=start;
	scsi_cmd.data_len[0]=(ntoc)>>8;
	scsi_cmd.data_len[1]=(ntoc)&0xff;
        return cd_scsi_cmd(unit,
                &scsi_cmd,
                sizeof(struct   scsi_read_toc),
                data,
                len,
                5000,
		NULL,
                0);
}


#define b2tol(a)	(((unsigned)(a##_1) << 8) + (unsigned)a##_0 )

/*******************************************************\
* Get the scsi driver to send a full inquiry to the	*
* device and use the results to fill out the disk 	*
* parameter structure.					*
\*******************************************************/

int	cd_get_parms(unit, flags)
{
	struct cd_data *cd = cd_driver->cd_data[unit];

	/*******************************************************\
	* First check if we have it all loaded			*
	\*******************************************************/
		if(cd->flags & CDVALID) return(0);
	/*******************************************************\
	* give a number of sectors so that sec * trks * cyls	*
	* is <= disk_size 					*
	\*******************************************************/
	if(cd_size(unit, flags))
	{
		cd->flags |= CDVALID;
		return(0);
	}
	else
	{
		return(ENXIO);
	}
}

/*******************************************************\
* close the device.. only called if we are the LAST	*
* occurence of an open device				*
\*******************************************************/
cdclose(dev)
dev_t dev;
{
	unsigned char unit, part;
	unsigned int old_priority;

	unit = UNIT(dev);
	part = PARTITION(dev);
#ifdef	CDDEBUG
	if(scsi_debug & TRACEOPENS)
		printf("cd%d: closing part %d\n",unit,part);
#endif
	cd_driver->cd_data[unit]->partflags[part] &= ~CDOPEN;
	cd_driver->cd_data[unit]->openparts &= ~(1 << part);
       	cd_prevent_unit(unit,PR_ALLOW,SCSI_SILENT);
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
int	cd_scsi_cmd(unit,scsi_cmd,cmdlen,data_addr,datalen,timeout,bp,flags)

int	unit,flags;
struct	scsi_generic *scsi_cmd;
int	cmdlen;
int	timeout;
u_char	*data_addr;
int	datalen;
struct	buf	*bp;
{
	struct	scsi_xfer *xs;
	int	retval;
	int	s;
	struct cd_data *cd = cd_driver->cd_data[unit];

#ifdef	CDDEBUG
	if(scsi_debug & PRINTROUTINES) printf("\ncd_scsi_cmd%d ",unit);
#endif	/*CDDEBUG*/
#ifdef	PARANOID
	if(!(cd->sc_sw))	/* If we have a scsi driver */
	{
		/* !? how'd we GET here? */
		panic("attempt to run bad cd device");
	}
#endif	/*PARANOID*/
	xs = cd_get_xs(unit,flags); /* should wait unless booting */
	if(!xs)
	{
		printf("cd%d: scsi_cmd controller busy"
 				" (this should never happen)\n",unit); 
			return(EBUSY);
	}
	xs->flags |= INUSE;
	/*******************************************************\
	* Fill out the scsi_xfer structure			*
	\*******************************************************/
	xs->flags	|=	flags;
	xs->adapter	=	cd->ctlr;
	xs->targ	=	cd->targ;
	xs->lu		=	cd->lu;
	xs->retries	=	CD_RETRIES;
	xs->timeout	=	timeout;
	xs->cmd		=	scsi_cmd;
	xs->cmdlen	=	cmdlen;
	xs->data	=	data_addr;
	xs->datalen	=	datalen;
	xs->resid	=	datalen;
	xs->when_done	=	(flags & SCSI_NOMASK)
				?(int (*)())0
				:cd_done;
	xs->done_arg	=	unit;
	xs->done_arg2	=	(int)xs;
	xs->bp		=	bp;
retry:	xs->error	=	XS_NOERROR;
	/*******************************************************\
	* Do the transfer. If we are polling we will return:	*
	* COMPLETE,	Was poll, and cd_done has been called	*
	* HAD_ERROR,	Was poll and an error was encountered	*
	* TRY_AGAIN_LATER, Adapter short resources, try again	*
	*							*
	* if under full steam (interrupts) it will return:	*
	* SUCCESSFULLY_QUEUED, will do a wakeup when complete	*
	* HAD_ERROR,	had an erro before it could queue	*
	* TRY_AGAIN_LATER, (as for polling)			*
	* After the wakeup, we must still check if it succeeded	*
	*							*
	* If we have a bp however, all the error proccessing	*
	* and the buffer code both expect us to return straight	*
	* to them, so as soon as the command is queued, return	*
	\*******************************************************/
	retval = (*(cd->sc_sw->scsi_cmd))(xs);
	if(bp) return retval;	/* will sleep (or not) elsewhere */

	/*******************************************************\
	* Only here for non I/O cmds. It's cheaper to process	*
	* the error status here than at interrupt time so	*
	* sd_done will have done nothing except wake us up.	*
	\*******************************************************/
	switch(retval)
	{
	case	SUCCESSFULLY_QUEUED:
		s = splbio();
		while(!(xs->flags & ITSDONE))
			sleep(xs,PRIBIO+1);
		splx(s);
		/* Fall through to check the result */

	case	HAD_ERROR:
		switch(xs->error)
		{
		case	XS_NOERROR:	/* usually this one */
			retval = ESUCCESS;
			break;

		case	XS_SENSE:
			retval = (cd_interpret_sense(unit,xs));
			break;
		case	XS_BUSY:
			/* should sleep here 1 sec */
			/* fall through */
		case	XS_TIMEOUT:
			if(xs->retries-- )
			{
				xs->flags &= ~ITSDONE;
				goto retry;
			}
			/* fall through */
		case	XS_DRIVER_STUFFUP:
			retval = EIO;
			break;
		default:
			retval = EIO;
			printf("cd%d: unknown error category from scsi driver\n"
				,unit);
		}	
		break;
	case	COMPLETE:
		retval = ESUCCESS;
		break;

	case 	TRY_AGAIN_LATER:
		if(xs->retries-- )
		{
			if(tsleep( 0,PRIBIO + 2,"retry",hz * 2))
			{
				xs->flags &= ~ITSDONE;
				goto retry;
			}
		}
		/* fall through */
	default:
		retval = EIO;
	}

	/*******************************************************\
	* we have finished doing the  command, free the struct	*
	* and check if anyone else needs it			*
	\*******************************************************/
	cd_free_xs(unit,xs,flags);
	cdstart(unit);		/* check if anything is waiting for the xs */
	return(retval);
}
/***************************************************************\
* Look at the returned sense and act on the error and detirmine	*
* The unix error number to pass back... (0 = report no error)	*
\***************************************************************/

int	cd_interpret_sense(unit,xs)
int	unit;
struct	scsi_xfer *xs;
{
	struct	scsi_sense_data *sense;
	int	key;
	int	silent;
	int	info;
	struct cd_data *cd = cd_driver->cd_data[unit];

	static char *error_mes[] = {		"soft error (corrected)",
		"not ready",			"medium error",
		"non-media hardware failure",	"illegal request",
		"unit attention",		"readonly device",
		"no data found",		"vendor unique",
		"copy aborted",			"command aborted",
		"search returned equal",	"volume overflow",
		"verify miscompare",		"unknown error key"
	};

	/***************************************************************\
	* If the flags say errs are ok, then always return ok.		*
	\***************************************************************/
	if (xs->flags & SCSI_ERR_OK) return(ESUCCESS);
	silent = (xs->flags & SCSI_SILENT);

	sense = &(xs->sense);
#ifdef	CDDEBUG
	if(cd_debug)
	{
		int count = 0;
		printf("code%x valid%x\n"
				,sense->error_code & SSD_ERRCODE
				,sense->error_code & SSD_ERRCODE_VALID ? 1 : 0);
		printf("seg%x key%x ili%x eom%x fmark%x\n"
				,sense->ext.extended.segment
				,sense->ext.extended.flags & SSD_KEY
				,sense->ext.extended.flags & SSD_ILI ? 1 : 0
				,sense->ext.extended.flags & SSD_EOM ? 1 : 0
				,sense->ext.extended.flags & SSD_FILEMARK ? 1 : 0);
		printf("info: %x %x %x %x followed by %d extra bytes\n"
				,sense->ext.extended.info[0]
				,sense->ext.extended.info[1]
				,sense->ext.extended.info[2]
				,sense->ext.extended.info[3]
				,sense->ext.extended.extra_len);
		printf("extra: ");
		while(count < sense->ext.extended.extra_len)
		{
			printf ("%x ",sense->ext.extended.extra_bytes[count++]);
		}
		printf("\n");
	}
#endif	/*CDDEBUG*/
	switch(sense->error_code & SSD_ERRCODE)
	{
	/***************************************************************\
	* If it's code 70, use the extended stuff and interpret the key	*
	\***************************************************************/
	case 0x71:/* delayed error */
		printf("cd%d: DELAYED ERROR, key = 0x%x\n",unit,key);
	case 0x70:
		if(sense->error_code & SSD_ERRCODE_VALID)
		{
			info = ntohl(*((long *)sense->ext.extended.info));
		}
		else
		{
			info = 0;
		}
	
		key=sense->ext.extended.flags & SSD_KEY;
	
		if (!silent)
		{
			printf("cd%d: %s", unit, error_mes[key - 1]);
			if(sense->error_code & SSD_ERRCODE_VALID)
			{
				switch (key)
				{
				case	0x2:		/* NOT READY */
				case	0x5:		/* ILLEGAL REQUEST */
				case	0x6:		/* UNIT ATTENTION */
				case	0x7:		/* DATA PROTECT */
					break;
				case	0x8:		/* BLANK CHECK */
		   			printf(", requested size: %d (decimal)",
				    	info);
					break;
				default:
		   			printf(", info = %d (decimal)", info);
				}
			}
			printf("\n");
		}
	
		switch (key)
		{
		case	0x0:				/* NO SENSE */
		case	0x1:				/* RECOVERED ERROR */
			xs->resid = 0;
		case	0xc:				/* EQUAL */
			return(ESUCCESS);
		case	0x2:				/* NOT READY */
			cd->flags &= ~(CDVALID | CDHAVELABEL);
			return(ENODEV);
		case	0x5:				/* ILLEGAL REQUEST */
			return(EINVAL);
		case	0x6:				/* UNIT ATTENTION */
			cd->flags &= ~(CDVALID | CDHAVELABEL);
			if (cd->openparts)
			{
				return(EIO);
			}
			return(ESUCCESS);
		case	0x7:				/* DATA PROTECT */
			return(EACCES);
		case	0xd:				/* VOLUME OVERFLOW */
			return(ENOSPC);
		case	0x8:				/* BLANK CHECK */
			return(ESUCCESS);
		default:
			return(EIO);
		}
	/*******************************\
	* Not code 70, just report it	*
	\*******************************/
	default:
		if(!silent)
		{
			printf("cd%d: error code %d", unit,
				sense->error_code & SSD_ERRCODE);
			if(sense->error_code & SSD_ERRCODE_VALID)
			{
				printf(" at block no. %d (decimal)",
					(sense->ext.unextended.blockhi <<16)
					+ (sense->ext.unextended.blockmed <<8)
					+ (sense->ext.unextended.blocklow ));
			}	
			printf("\n");
		}
		return(EIO);
	}
}




int
cdsize(dev_t dev)
{
	return (-1);
}

#if 0
show_mem(address,num)
unsigned char   *address;
int     num;
{
	int x,y;
	printf("------------------------------");
	for (y = 0; y<num; y += 1)
	{
		if(!(y % 16))
			printf("\n%03d: ",y);
		printf("%02x ",*address++);
	}
	printf("\n------------------------------\n");
}
#endif
