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
 *
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         1       00098
 * --------------------         -----   ----------------------
 *
 * 16 Feb 93	Julian Elischer		ADDED for SCSI system
 */

/*
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 */


/*
 * To do:
 * work out some better way of guessing what a good timeout is going
 * to be depending on whether we expect to retension or not.
 *
 */

#include	<sys/types.h>
#include	<st.h>

#include <sys/param.h>
#include <sys/systm.h>

#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/mtio.h>

#if defined(OSF)
#define SECSIZE	512
#endif /* defined(OSF) */

#include <scsi/scsi_all.h>
#include <scsi/scsi_tape.h>
#include <scsi/scsiconf.h>


long int ststrats,stqueues;


#define PAGESIZ 	4096
#define STQSIZE		4
#define	ST_RETRIES	4


#define MODE(z)		(  (minor(z)       & 0x03) )
#define DSTY(z)         ( ((minor(z) >> 2) & 0x03) )
#define UNIT(z)		(  (minor(z) >> 4) )

#define DSTY_QIC120  3
#define DSTY_QIC150  2
#define DSTY_QIC525  1

#define QIC120     0x0f
#define QIC150     0x10
#define QIC525     0x11




#ifndef	__386BSD__
struct buf stbuf[NST][STQSIZE]; 	/* buffer for raw io (one per device) */
struct buf *stbuf_free[NST]; 	/* queue of free buffers for raw io */
#endif	__386BSD__
struct buf st_buf_queue[NST];
int	ststrategy();
void	stminphys();
struct	scsi_xfer st_scsi_xfer[NST];
int	st_xfer_block_wait[NST];

#if defined(OSF)
caddr_t		st_window[NST];
#endif /* defined(OSF) */
#ifndef	MACH
#define ESUCCESS 0
#endif	MACH

int	st_info_valid[NST];	/* the info about the device is valid */
int	st_initialized[NST] ;
int	st_debug = 0;

int stattach();
int st_done();
struct	st_data
{
	int	flags;
	struct	scsi_switch *sc_sw;	/* address of scsi low level switch */
	int	ctlr;			/* so they know which one we want */
	int	targ;			/* our scsi target ID */
	int	lu;			/* out scsi lu */
	int	blkmin;			/* min blk size */
	int	blkmax;			/* max blk size */
	int	numblks;		/* nominal blocks capacity */
	int	blksiz;			/* nominal block size */
}st_data[NST];
#define ST_OPEN		0x01
#define	ST_NOREWIND	0x02
#define	ST_WRITTEN	0x04
#define	ST_FIXEDBLOCKS	0x10
#define	ST_AT_FILEMARK	0x20
#define	ST_AT_EOM	0x40

#define	ST_PER_ACTION	(ST_AT_FILEMARK | ST_AT_EOM)
#define	ST_PER_OPEN	(ST_OPEN | ST_NOREWIND | ST_WRITTEN | ST_PER_ACTION)
#define	ST_PER_MEDIA	ST_FIXEDBLOCKS

static	int	next_st_unit = 0;
/***********************************************************************\
* The routine called by the low level scsi routine when it discovers	*
* A device suitable for this driver					*
\***********************************************************************/

int	stattach(ctlr,targ,lu,scsi_switch)
struct	scsi_switch *scsi_switch;
{
	int	unit,i;
	unsigned char *tbl;
	struct st_data *st;

	if(scsi_debug & PRINTROUTINES) printf("stattach: ");
	/*******************************************************\
	* Check we have the resources for another drive		*
	\*******************************************************/
	unit = next_st_unit++;
	if( unit >= NST)
	{
		printf("Too many scsi tapes..(%d > %d) reconfigure kernel",(unit + 1),NST);
		return(0);
	}
	st = st_data + unit;
	/*******************************************************\
	* Store information needed to contact our base driver	*
	\*******************************************************/
	st->sc_sw	=	scsi_switch;
	st->ctlr	=	ctlr;
	st->targ	=	targ;
	st->lu	=	lu;

	/*******************************************************\
	* Use the subdriver to request information regarding	*
	* the drive. We cannot use interrupts yet, so the	*
	* request must specify this.				*
	\*******************************************************/
	if((st_mode_sense(unit,  SCSI_NOSLEEP |  SCSI_NOMASK | SCSI_SILENT)))
	{
		printf("	st%d: scsi tape drive, %d blocks of %d bytes\n",
			unit, st->numblks, st->blksiz);
	}
	else
	{
		printf("	st%d: scsi tape drive :- offline\n", unit);
	}
	/*******************************************************\
	* Set up the bufs for this device			*
	\*******************************************************/
#ifndef	__386BSD__
	stbuf_free[unit] = (struct buf *)0;
	for (i = 1; i < STQSIZE; i++)
	{
		stbuf[unit][i].b_forw = stbuf_free[unit];
		stbuf_free[unit]=&stbuf[unit][i];
	}
#endif	__386BSD__
	st_buf_queue[unit].b_active	=	0;
	st_buf_queue[unit].b_actf	=	st_buf_queue[unit].b_actl = 0;
	st_initialized[unit] = 1;

#if defined(OSF)
  	st_window[unit] = (caddr_t)alloc_kva(SECSIZE*256+PAGESIZ);
#endif /* defined(OSF) */

	return;

}



/*******************************************************\
*	open the device.				*
\*******************************************************/
stopen(dev)
{
	int errcode = 0;
	int unit,mode,dsty;
        int dsty_code;
	struct st_data *st;
	unit = UNIT(dev);
	mode = MODE(dev);
	dsty = DSTY(dev);
	st = st_data + unit;

	/*******************************************************\
	* Check the unit is legal                               *
	\*******************************************************/
	if ( unit >= NST )
	{
		errcode = ENXIO;
		return(errcode);
	}
	/*******************************************************\
	* Only allow one at a time				*
	\*******************************************************/
	if(st->flags & ST_OPEN)
	{
		errcode = ENXIO;
		goto bad;
	}
	/*******************************************************\
	* Set up the mode flags according to the minor number	*
	* ensure all open flags are in a known state		*
	\*******************************************************/
	st->flags &= ~ST_PER_OPEN;
	switch(mode)
	{
	case	2:
	case	0:
		st->flags &= ~ST_NOREWIND;
		break;
	case	3:
	case	1:
		st->flags |= ST_NOREWIND;
		break;
	default:
		printf("st%d: Bad mode (minor number)%d\n",unit,mode);
		return(EINVAL);
	}
        /*******************************************************\
        * Check density code: 0 is drive default		*
        \*******************************************************/
        switch(dsty)
	{
	case	0:		dsty_code = 0;		break;
	case	DSTY_QIC120:	dsty_code = QIC120;	break;
	case	DSTY_QIC150:	dsty_code = QIC150;	break;
	case	DSTY_QIC525:	dsty_code = QIC525;	break;
	default:
		printf("st%d: Bad density (minor number)%d\n",unit,dsty);
		return(EINVAL);
	}
	if(scsi_debug & (PRINTROUTINES | TRACEOPENS))
		printf("stopen: dev=0x%x (unit %d (of %d))\n"
				,   dev,      unit,   NST);
	/*******************************************************\
	* Make sure the device has been initialised		*
	\*******************************************************/

	if (!st_initialized[unit])
		return(ENXIO);
	/*******************************************************\
	* Check that it is still responding and ok.		*
	\*******************************************************/

	if(scsi_debug & TRACEOPENS)
		printf("device is ");
	if (!(st_req_sense(unit, 0)))
	{
		errcode = ENXIO;
		if(scsi_debug & TRACEOPENS)
			printf("not responding\n");
		goto bad;
	}
	if(scsi_debug & TRACEOPENS)
		printf("ok\n");

	if(!(st_test_ready(unit,0)))
	{
		printf("st%d not ready\n",unit);
		return(EIO);
	}

	if(!st_info_valid[unit])		/* is media new? */
		if(!st_load(unit,LD_LOAD,0))
		{
			return(EIO);
		}

	if(!st_rd_blk_lim(unit,0))
	{
		return(EIO);
	}

	if(!st_mode_sense(unit,0))
	{
		return(EIO);
	}

	if(!st_mode_select(unit,0,dsty_code))
	{
		return(EIO);
	}

	st_info_valid[unit] = TRUE;

	st_prevent(unit,PR_PREVENT,0); /* who cares if it fails? */

	/*******************************************************\
	* Load the physical device parameters			*
	\*******************************************************/
	if(scsi_debug & TRACEOPENS)
		printf("Params loaded ");


	st->flags |= ST_OPEN;

bad:
	return(errcode);
}

/*******************************************************\
* close the device.. only called if we are the LAST	*
* occurence of an open device				*
\*******************************************************/
stclose(dev)
{
	unsigned char unit,mode;
	struct	st_data *st;

	unit = UNIT(dev);
	mode = MODE(dev);
	st = st_data + unit;

	if(scsi_debug & TRACEOPENS)
		printf("Closing device");
	if(st->flags & ST_WRITTEN)
	{
		st_write_filemarks(unit,1,0);
	}
	st->flags &= ~ST_WRITTEN;
	switch(mode)
	{
	case	0:
		st_rewind(unit,FALSE,SCSI_SILENT);
		st_prevent(unit,PR_ALLOW,SCSI_SILENT);
		break;
	case	1:
		st_prevent(unit,PR_ALLOW,SCSI_SILENT);
		break;
	case	2:
		st_rewind(unit,FALSE,SCSI_SILENT);
		st_prevent(unit,PR_ALLOW,SCSI_SILENT);
		st_load(unit,LD_UNLOAD,SCSI_SILENT);
		break;
	case	3:
		st_prevent(unit,PR_ALLOW,SCSI_SILENT);
		st_load(unit,LD_UNLOAD,SCSI_SILENT);
		break;
	default:
		printf("st%d:close: Bad mode (minor number)%d how's it open?\n"
				,unit,mode);
		return(EINVAL);
	}
	st->flags &= ~ST_PER_OPEN;
	return(0);
}

#ifndef	__386BSD__
/*******************************************************\
* Get ownership of this unit's buf			*
* If need be, sleep on it, until it comes free		*
\*******************************************************/
struct buf *
st_get_buf(unit) {
	struct buf *rc;

	while (!(rc = stbuf_free[unit]))
		sleep((caddr_t)&stbuf_free[unit], PRIBIO+1);
	stbuf_free[unit] = stbuf_free[unit]->b_forw;
	rc->b_error = 0;
	rc->b_resid = 0;
	rc->b_flags = 0;
	return(rc);
}

/*******************************************************\
* Free this unit's buf, wake processes waiting for it	*
\*******************************************************/
st_free_buf(unit,bp)
struct buf *bp;
{
	if (!stbuf_free[unit])
		wakeup((caddr_t)&stbuf_free[unit]);
	bp->b_forw = stbuf_free[unit];
	stbuf_free[unit] = bp;
}
	

/*******************************************************\
* Get the buf for this unit and use physio to do it	*
\*******************************************************/
stread(dev,uio)
register short  dev;
struct uio 	*uio;
{
	int	unit = UNIT(dev);
	struct buf *bp = st_get_buf(unit);
	int rc;
	rc = physio(ststrategy, bp, dev, B_READ, stminphys, uio);
	st_free_buf(unit,bp);
	return(rc);
}

/*******************************************************\
* Get the buf for this unit and use physio to do it	*
\*******************************************************/
stwrite(dev,uio)
dev_t	 	dev;
struct uio	*uio;
{
	int	unit = UNIT(dev);
	struct buf *bp = st_get_buf(unit);
	int rc;

	rc = physio(ststrategy, bp, dev, B_WRITE, stminphys, uio);
	st_free_buf(unit,bp);
	return(rc);
}


#endif	__386BSD__
/*******************************************************\
* trim the size of the transfer if needed,		*
* called by physio					*
* basically the smaller of our min and the scsi driver's*
* minphys						*
\*******************************************************/
void	stminphys(bp)
struct buf	*bp;
{
	(*(st_data[UNIT(bp->b_dev)].sc_sw->scsi_minphys))(bp);
}

/*******************************************************\
* Actually translate the requested transfer into	*
* one the physical driver can understand		*
* The transfer is described by a buf and will include	*
* only one physical transfer.				*
\*******************************************************/

int	ststrategy(bp)
struct	buf	*bp;
{
	struct	buf	*dp;
	unsigned char unit;
	unsigned int opri;

	ststrats++;
	unit = UNIT((bp->b_dev));
	if(scsi_debug & PRINTROUTINES) printf("\nststrategy ");
	if(scsi_debug & SHOWREQUESTS) printf("st%d: %d bytes @ blk%d\n",
					unit,bp->b_bcount,bp->b_blkno);
	/*******************************************************\
	* If it's a null transfer, return immediatly		*
	\*******************************************************/
	if (bp->b_bcount == 0) {
		goto done;
	}

	/*******************************************************\
	* Odd sized request on fixed drives are verboten	*
	\*******************************************************/
	if((st_data[unit].flags & ST_FIXEDBLOCKS)
		&& bp->b_bcount % st_data[unit].blkmin)
	{
		printf("st%d: bad request, must be multiple of %d\n",
			unit, st_data[unit].blkmin);
		bp->b_error = EIO;
		goto bad;
	}

#ifdef	__386BSD__
	stminphys(bp);
#endif	__386BSD__
	opri = splbio();
	dp = &st_buf_queue[unit];

	/*******************************************************\
	* Place it in the queue of disk activities for this tape*
	* at the end						*
	\*******************************************************/
	while ( dp->b_actf) 
	{
		dp = dp->b_actf;
	}	
	dp->b_actf = bp;
	bp->b_actf = NULL;

	/*******************************************************\
	* Tell the device to get going on the transfer if it's	*
	* not doing anything, otherwise just wait for completion*
	\*******************************************************/
	ststart(unit);

	splx(opri);
	return;
bad:
	bp->b_flags |= B_ERROR;
done:
	/*******************************************************\
	* Correctly set the buf to indicate a completed xfer	*
	\*******************************************************/
	iodone(bp);
	return;
}


/***************************************************************\
* ststart looks to see if there is a buf waiting for the device	*
* and that the device is not already busy. If both are true,	*
* It deques the buf and creates a scsi command to perform the	*
* transfer in the buf. The transfer request will call st_done	*
* on completion, which will in turn call this routine again	*
* so that the next queued transfer is performed.		*
* The bufs are queued by the strategy routine (ststrategy)	*
*								*
* This routine is also called after other non-queued requests	*
* have been made of the scsi driver, to ensure that the queue	*
* continues to be drained.					*
\***************************************************************/
/* ststart() is called at splbio */
ststart(unit)
{
	int			drivecount;
	register struct buf	*bp = 0;
	register struct buf	*dp;
	struct	scsi_xfer	*xs;
	struct	scsi_rw_tape	cmd;
	int			blkno, nblk;
	struct	st_data *st;


	st = st_data + unit;

	if(scsi_debug & PRINTROUTINES) printf("ststart%d ",unit);
	/*******************************************************\
	* See if there is a buf to do and we are not already	*
	* doing one						*
	\*******************************************************/
	xs=&st_scsi_xfer[unit];
	if(xs->flags & INUSE)
	{
		return;    /* unit already underway */
	}
trynext:
	if(st_xfer_block_wait[unit]) /* a special awaits, let it proceed first */
	{
		wakeup(&st_xfer_block_wait[unit]);
		return;
	}

	dp = &st_buf_queue[unit];
	if ((bp = dp->b_actf) != NULL)
	{
		dp->b_actf = bp->b_actf;
	}
	else /* no work to do */
	{
		return;
	}
	xs->flags = INUSE;    /* Now ours */


	/*******************************************************\
	* We have a buf, now we should move the data into	*
	* a scsi_xfer definition and try start it		*
	\*******************************************************/

	/*******************************************************\
	*  If we are at a filemark but have not reported it yet	*
	* then we should report it now				*
	\*******************************************************/
	if(st->flags & ST_AT_FILEMARK)
	{
		bp->b_error = 0;
		bp->b_flags |= B_ERROR;	/* EOF*/
		st->flags &= ~ST_AT_FILEMARK;
		biodone(bp);
		xs->flags = 0; /* won't need it now */
		goto trynext;
	}
	/*******************************************************\
	*  If we are at EOM but have not reported it yet	*
	* then we should report it now				*
	\*******************************************************/
	if(st->flags & ST_AT_EOM)
	{
		bp->b_error = EIO;
		bp->b_flags |= B_ERROR;
		st->flags &= ~ST_AT_EOM;
		biodone(bp);
		xs->flags = 0; /* won't need it now */
		goto trynext;
	}
	/*******************************************************\
	*  Fill out the scsi command				*
	\*******************************************************/
	bzero(&cmd, sizeof(cmd));
	if((bp->b_flags & B_READ) == B_WRITE)
	{
		st->flags |= ST_WRITTEN;
		xs->flags |= SCSI_DATA_OUT;
	}
	else
	{
		xs->flags |= SCSI_DATA_IN;
	}
	cmd.op_code = (bp->b_flags & B_READ) 
					? READ_COMMAND_TAPE
					: WRITE_COMMAND_TAPE;

	/*******************************************************\
	* Handle "fixed-block-mode" tape drives by using the    *
	* block count instead of the length.			*
	\*******************************************************/
	if(st->flags & ST_FIXEDBLOCKS)
	{
		cmd.fixed = 1;
		lto3b(bp->b_bcount/st->blkmin,cmd.len);
	}
	else
	{
		lto3b(bp->b_bcount,cmd.len);
	}

	/*******************************************************\
	* Fill out the scsi_xfer structure			*
	*	Note: we cannot sleep as we may be an interrupt	*
	\*******************************************************/
	xs->flags	|=	SCSI_NOSLEEP;
	xs->adapter	=	st->ctlr;
	xs->targ	=	st->targ;
	xs->lu		=	st->lu;
	xs->retries	=	1;	/* can't retry on tape*/
	xs->timeout	=	100000; /* allow 100 secs for retension */
	xs->cmd		=	(struct	scsi_generic *)&cmd;
	xs->cmdlen	=	sizeof(cmd);
	xs->data	=	(u_char *)bp->b_un.b_addr;
	xs->datalen	=	bp->b_bcount;
	xs->resid	=	bp->b_bcount;
	xs->when_done	=	st_done;
	xs->done_arg	=	unit;
	xs->done_arg2	=	(int)xs;
	xs->error	=	XS_NOERROR;
	xs->bp		=	bp;
	/*******************************************************\
	* Pass all this info to the scsi driver.		*
	\*******************************************************/


#if defined(OSF)||defined(FIX_ME)
	if (bp->b_flags & B_PHYS) {
	 	xs->data = (u_char*)map_pva_kva(bp->b_proc, bp->b_un.b_addr,
			bp->b_bcount, st_window[unit],
			(bp->b_flags&B_READ)?B_WRITE:B_READ);
	} else {
		xs->data = (u_char*)bp->b_un.b_addr;
	}
#endif /* defined(OSF) */

	if ( (*(st->sc_sw->scsi_cmd))(xs) != SUCCESSFULLY_QUEUED)
	{
		printf("st%d: oops not queued",unit);
		xs->error = XS_DRIVER_STUFFUP;
		st_done(unit,xs);
	}
	stqueues++;
}

/*******************************************************\
* This routine is called by the scsi interrupt when	*
* the transfer is complete.
\*******************************************************/
int	st_done(unit,xs)
int	unit;
struct	scsi_xfer	*xs;
{
	struct	buf		*bp;
	int	retval;

	if(scsi_debug & PRINTROUTINES) printf("st_done%d ",unit);
	if (! (xs->flags & INUSE))
		panic("scsi_xfer not in use!");
	if(bp = xs->bp)
	{
		switch(xs->error)
		{
		case	XS_NOERROR:
			bp->b_flags &= ~B_ERROR;
			bp->b_error = 0;
			bp->b_resid = 0;
			break;
		case	XS_SENSE:
			retval = (st_interpret_sense(unit,xs));
			if(retval)
			{
				/***************************************\
				* We have a real error, the bit should	*
				* be set to indicate this. The return	*
				* value will contain the unix error code*
				* that the error interpretation routine	*
				* thought was suitable, so pass this	*
				* value back in the buf structure.	*
				* Furthermore we return information	*
				* saying that no data was transferred	*
				\***************************************/
				bp->b_flags |= B_ERROR;
				bp->b_error = retval;
				bp->b_resid =  bp->b_bcount;
				st_data[unit].flags 
					&= ~(ST_AT_FILEMARK|ST_AT_EOM);
			}
			else
			{
			/***********************************************\
			* The error interpretation code has declared	*
			* that it wasn't a real error, or at least that	*
			* we should be ignoring it if it was.		*
			\***********************************************/
			    if(xs->resid && ( xs->resid != xs->datalen ))
			    {
				/***************************************\
				* Here we have the tricky part..	*
				* We successfully read less data than	*
				* we requested. (but not 0)		*
				*------for variable blocksize tapes:----*
				* UNDER 386BSD:				*
				* We should legitimatly have the error	*
				* bit set, with the error value set to 	*
				* zero.. This is to indicate to the	*
				* physio code that while we didn't get	*
				* as much information as was requested,	*
				* we did reach the end of the record	*
				* and so physio should not call us	*
				* again for more data... we have it all	*
				* SO SET THE ERROR BIT!			*
				*					*
				* UNDER MACH:(CMU)			*
				* To indicate the same as above, we	*
				* need only have a non 0 resid that is	*
				* less than the b_bcount, but the	*
				* ERROR BIT MUST BE CLEAR! (sigh) 	*
				*					*
				* UNDER OSF1:				*
				* To indicate the same as above, we	*
				* need to have a non 0 resid that is	*
				* less than the b_bcount, but the	*
				* ERROR BIT MUST BE SET! (gasp)(sigh) 	*
				*					*
				*-------for fixed blocksize device------*
				* We could have read some successful	*
				* records before hitting		*
				* the EOF or EOT. These must be passed	*
				* to the user, before we report the 	*
				* EOx. Only if there is no data for the	*
				* user do we report it now. (via an EIO	*
				* for EOM and resid == count for EOF).	*
				* We will report the EOx NEXT time..	*
				\***************************************/
#ifdef	MACH /*osf and cmu varieties */
#ifdef	OSF
				bp->b_flags |= B_ERROR;
#else	OSF
				bp->b_flags &= ~B_ERROR;
#endif	OSF
#endif	MACH
#ifdef	__386BSD__
				bp->b_flags |= B_ERROR;
#endif	__386BSD__
				bp->b_error = 0;
				bp->b_resid = xs->resid;
				if((st_data[unit].flags & ST_FIXEDBLOCKS))
				{
					bp->b_resid *= st_data[unit].blkmin;
					if(  (st_data[unit].flags & ST_AT_EOM)
				 	  && (bp->b_resid == bp->b_bcount))
					{
						bp->b_error = EIO;
						st_data[unit].flags
							&= ~ST_AT_EOM;
					}
				}
				xs->error = XS_NOERROR;
				break;
			    }
			    else
			    {
				/***************************************\
				* We have come out of the error handler	*
				* with no error code.. we have also 	*
				* not had an ili (would have gone to	*
				* the previous clause). Now we need to	*
				* distiguish between succesful read of	*
				* no data (EOF or EOM) and successfull	*
				* read of all requested data.		*
				* At least all o/s agree that:		*
				* 0 bytes read with no error is EOF	*
				* 0 bytes read with an EIO is EOM	*
				\***************************************/

				bp->b_resid = bp->b_bcount;
				if(st_data[unit].flags & ST_AT_FILEMARK)
				{
					st_data[unit].flags &= ~ST_AT_FILEMARK;
					bp->b_flags &= ~B_ERROR;
					bp->b_error = 0;
					break;
				}
				if(st_data[unit].flags & ST_AT_EOM)
				{
					bp->b_flags |= B_ERROR;
					bp->b_error = EIO;
					st_data[unit].flags &= ~ST_AT_EOM;
					break;
				}
				printf("st%d:error ignored\n" ,unit);
			    }
			}
			break;

		case    XS_TIMEOUT:
			printf("st%d timeout\n",unit);
			break;

		case    XS_BUSY:        /* should retry */ /* how? */
			/************************************************/
			/* SHOULD put buf back at head of queue         */
			/* and decrement retry count in (*xs)           */
			/* HOWEVER, this should work as a kludge        */
			/************************************************/
			if(xs->retries--)
			{
				xs->flags &= ~ITSDONE;
				xs->error = XS_NOERROR;
				if ( (*(st_data[unit].sc_sw->scsi_cmd))(xs)
					== SUCCESSFULLY_QUEUED)
				{       /* don't wake the job, ok? */
					return;
				}
				printf("device busy");
				xs->flags |= ITSDONE;
			}

		case	XS_DRIVER_STUFFUP:
			bp->b_flags |= B_ERROR;
			bp->b_error = EIO;
			break;
		default:
			printf("st%d: unknown error category from scsi driver\n"
				,unit);
		}	
		biodone(bp);
		xs->flags = 0;	/* no longer in use */
		ststart(unit);		/* If there's another waiting.. do it */
	}
	else
	{
		wakeup(xs);
	}
}
/*******************************************************\
* Perform special action on behalf of the user		*
* Knows about the internals of this device		*
\*******************************************************/
stioctl(dev, cmd, arg, mode)
dev_t dev;
int cmd;
caddr_t arg;
{
	register i,j;
	unsigned int opri;
	int errcode = 0;
	unsigned char unit;
	int number,flags,ret;

	/*******************************************************\
	* Find the device that the user is talking about	*
	\*******************************************************/
	flags = 0;	/* give error messages, act on errors etc. */
	unit = UNIT(dev);

	switch(cmd)
	{

	case MTIOCGET:
	    {
		struct mtget *g = (struct mtget *) arg;

		bzero(g, sizeof(struct mtget));
		g->mt_type = 0x7;	/* Ultrix compat */ /*?*/
		ret=TRUE;
		break;
	    }


	case MTIOCTOP:
	    {
		struct mtop *mt = (struct mtop *) arg;

		if (st_debug)
			printf("[sctape_sstatus: %x %x]\n",
				 mt->mt_op, mt->mt_count);



		/* compat: in U*x it is a short */
		number = mt->mt_count;
		switch ((short)(mt->mt_op))
		{
		case MTWEOF:	/* write an end-of-file record */
			ret = st_write_filemarks(unit,number,flags);
			st_data[unit].flags &= ~ST_WRITTEN;
			break;
		case MTFSF:	/* forward space file */
			ret = st_space(unit,number,SP_FILEMARKS,flags);
			break;
		case MTBSF:	/* backward space file */
			ret = st_space(unit,-number,SP_FILEMARKS,flags);
			break;
		case MTFSR:	/* forward space record */
			ret = st_space(unit,number,SP_BLKS,flags);
			break;
		case MTBSR:	/* backward space record */
			ret = st_space(unit,-number,SP_BLKS,flags);
			break;
		case MTREW:	/* rewind */
			ret = st_rewind(unit,FALSE,flags);
			break;
		case MTOFFL:	/* rewind and put the drive offline */
			if((ret = st_rewind(unit,FALSE,flags)))
			{
				st_prevent(unit,PR_ALLOW,0);
				ret = st_load(unit,LD_UNLOAD,flags);
			}
			else
			{
				printf("rewind failed, unit still loaded\n");
			}
			break;
		case MTNOP:	/* no operation, sets status only */
		case MTCACHE:	/* enable controller cache */
		case MTNOCACHE:	/* disable controller cache */
			ret = TRUE;;
			break;
		default:
			return EINVAL;
		}
		break;
	    }
	case MTIOCIEOT:
	case MTIOCEEOT:
		ret=TRUE;
		break;
	}

	return(ret?ESUCCESS:EIO);
}


/*******************************************************\
* Check with the device that it is ok, (via scsi driver)*
\*******************************************************/
st_req_sense(unit, flags)
int	flags;
{
	struct	scsi_sense_data sense;
	struct	scsi_sense	scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = REQUEST_SENSE;
	scsi_cmd.length = sizeof(sense);

	if (st_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			&sense,
			sizeof(sense),
			100000,
			flags | SCSI_DATA_IN) != 0)
	{
		return(FALSE);
	}
	else 
		return(TRUE);
}

/*******************************************************\
* Get scsi driver to send a "are you ready" command	*
\*******************************************************/
st_test_ready(unit,flags)
int	unit,flags;
{
	struct	scsi_test_unit_ready scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = TEST_UNIT_READY;

	if (st_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			0,
			0,
			100000,
			flags) != 0) {
		return(FALSE);
	} else 
		return(TRUE);
}


#ifdef	__STDC__
#define b2tol(a)	(((unsigned)(a##_1) << 8) + (unsigned)a##_0 )
#else
#define b2tol(a)	(((unsigned)(a/**/_1) << 8) + (unsigned)a/**/_0 )
#endif

/*******************************************************\
* Ask the drive what it's min and max blk sizes are.	*
\*******************************************************/
st_rd_blk_lim(unit, flags)
int	unit,flags;
{
	struct	scsi_blk_limits scsi_cmd;
	struct scsi_blk_limits_data scsi_blkl;
	struct st_data *st = st_data + unit;
	/*******************************************************\
	* First check if we have it all loaded			*
	\*******************************************************/
	if (st_info_valid[unit]) goto done;

	/*******************************************************\
	* do a 'Read Block Limits'				*
	\*******************************************************/
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = READ_BLK_LIMITS;

	/*******************************************************\
	* do the command,	update the global values	*
	\*******************************************************/
	if (st_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			&scsi_blkl,
			sizeof(scsi_blkl),
			5000,
			flags | SCSI_DATA_IN) != 0)
	{
		if(!(flags & SCSI_SILENT))
			printf("could not get blk limits for unit %d\n", unit);
		st_info_valid[unit] = FALSE;
		return(FALSE);
	} 
	if (st_debug)
	{
		printf(" (%d <= blksiz <= %d\n) ",
			b2tol(scsi_blkl.min_length),
			_3btol(&scsi_blkl.max_length_2));
	}
	st->blkmin = b2tol(scsi_blkl.min_length);
	st->blkmax = _3btol(&scsi_blkl.max_length_2);

done:
	if(st->blkmin && (st->blkmin == st->blkmax))
	{
		st->flags |= ST_FIXEDBLOCKS;
	}
	return(TRUE);
}
/*******************************************************\
* Get the scsi driver to send a full inquiry to the	*
* device and use the results to fill out the global 	*
* parameter structure.					*
\*******************************************************/
st_mode_sense(unit, flags)
int	unit,flags;
{
	struct scsi_mode_sense		scsi_cmd;
	struct 
	{
		struct	scsi_mode_header_tape header;
		struct	blk_desc	blk_desc;
	}scsi_sense;
	struct	st_data *st = st_data + unit;

	/*******************************************************\
	* First check if we have it all loaded			*
	\*******************************************************/
	if (st_info_valid[unit]) return(TRUE);
	/*******************************************************\
	* First do a mode sense 				*
	\*******************************************************/
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = MODE_SENSE;
	scsi_cmd.length = sizeof(scsi_sense);
	/*******************************************************\
	* do the command, but we don't need the results		*
	* just print them for our interest's sake		*
	\*******************************************************/
	if (st_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			&scsi_sense,
			sizeof(scsi_sense),
			5000,
			flags | SCSI_DATA_IN) != 0)
	{
		if(!(flags & SCSI_SILENT))
			printf("could not mode sense for unit %d\n", unit);
		st_info_valid[unit] = FALSE;
		return(FALSE);
	} 
	if (st_debug)
	{
		printf("unit %d: %d blocks of %d bytes, write %s, %sbuffered",
			unit,
			_3btol(&scsi_sense.blk_desc.nblocks),
			_3btol(&scsi_sense.blk_desc.blklen),
			(scsi_sense.header.write_protected ?
				"protected" : "enabled"),
			(scsi_sense.header.buf_mode ?
				"" : "un")
			);
	}
	st->numblks = _3btol(&scsi_sense.blk_desc.nblocks);
	st->blksiz = _3btol(&scsi_sense.blk_desc.blklen);
	return(TRUE);
}

/*******************************************************\
* Get the scsi driver to send a full inquiry to the	*
* device and use the results to fill out the global 	*
* parameter structure.					*
\*******************************************************/
st_mode_select(unit, flags, dsty_code)
int	unit,flags,dsty_code;
{
	struct scsi_mode_select scsi_cmd;
	struct
	{
		struct	scsi_mode_header_tape header;
		struct	blk_desc	blk_desc;
	}dat;
	struct	st_data *st = st_data + unit;

	/*******************************************************\
	* Set up for a mode select				*
	\*******************************************************/
	bzero(&dat, sizeof(dat));
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = MODE_SELECT;
	scsi_cmd.length = sizeof(dat);
	dat.header.blk_desc_len = sizeof(struct  blk_desc);
	dat.header.buf_mode = 1;
        dat.blk_desc.density = dsty_code;
	if(st->flags & ST_FIXEDBLOCKS)
	{
		lto3b( st->blkmin , dat.blk_desc.blklen);
	}
/*	lto3b( st->numblks , dat.blk_desc.nblocks); use defaults!!!!
	lto3b( st->blksiz , dat.blk_desc.blklen);
*/
	/*******************************************************\
	* do the command					*
	\*******************************************************/
	if (st_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			&dat,
			sizeof(dat),
			5000,
			flags | SCSI_DATA_OUT) != 0)
	{
		if(!(flags & SCSI_SILENT))
			printf("could not mode select for unit %d\n", unit);
		st_info_valid[unit] = FALSE;
		return(FALSE);
	} 
	return(TRUE);
}

/*******************************************************\
* skip N blocks/filemarks/seq filemarks/eom		*
\*******************************************************/
st_space(unit,number,what,flags)
int	unit,number,what,flags;
{
	struct scsi_space scsi_cmd;

	/* if we are at a filemark now, we soon won't be*/
	st_data[unit].flags &= ~(ST_AT_FILEMARK | ST_AT_EOM);
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = SPACE;
	scsi_cmd.code = what;
	lto3b(number,scsi_cmd.number);
	if (st_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			0,
			0,
			600000, /* 10 mins enough? */
			flags) != 0)
	{
		if(!(flags & SCSI_SILENT))
			printf("could not space st%d\n", unit);
		st_info_valid[unit] = FALSE;
		return(FALSE);
	}
	return(TRUE);
}
/*******************************************************\
* write N filemarks					*
\*******************************************************/
st_write_filemarks(unit,number,flags)
int	unit,number,flags;
{
	struct scsi_write_filemarks scsi_cmd;

	st_data[unit].flags &= ~(ST_AT_FILEMARK);
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = WRITE_FILEMARKS;
	lto3b(number,scsi_cmd.number);
	if (st_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			0,
			0,
			100000, /* 10 secs.. (may need to repos head )*/
			flags) != 0)
	{
		if(!(flags & SCSI_SILENT))
			printf("could not write_filemarks st%d\n", unit);
		st_info_valid[unit] = FALSE;
		return(FALSE);
	}
	return(TRUE);
}
/*******************************************************\
* load /unload (with retension if true)			*
\*******************************************************/
st_load(unit,type,flags)
int	unit,type,flags;
{
	struct  scsi_load  scsi_cmd;

	st_data[unit].flags &= ~(ST_AT_FILEMARK | ST_AT_EOM);
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = LOAD_UNLOAD;
	scsi_cmd.load=type;
	if (type == LD_LOAD)
	{
		/*scsi_cmd.reten=TRUE;*/
		scsi_cmd.reten=FALSE;
	}
	else
	{
		scsi_cmd.reten=FALSE;
	}
	if (st_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			0,
			0,
			30000, /* 30 secs */
			flags) != 0)
	{
		if(!(flags & SCSI_SILENT))
			printf("cannot load/unload  st%d\n", unit);
		st_info_valid[unit] = FALSE;
		return(FALSE);
	}
	return(TRUE);
}
/*******************************************************\
* Prevent or allow the user to remove the tape		*
\*******************************************************/
st_prevent(unit,type,flags)
int	unit,type,flags;
{
	struct	scsi_prevent	scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = PREVENT_ALLOW;
	scsi_cmd.prevent=type;
	if (st_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			0,
			0,
			5000,
			flags) != 0)
	{
		if(!(flags & SCSI_SILENT))
			printf("cannot prevent/allow on st%d\n", unit);
		st_info_valid[unit] = FALSE;
		return(FALSE);
	}
	return(TRUE);
}
/*******************************************************\
*  Rewind the device					*
\*******************************************************/
st_rewind(unit,immed,flags)
int	unit,immed,flags;
{
	struct	scsi_rewind	scsi_cmd;

	st_data[unit].flags &= ~(ST_AT_FILEMARK | ST_AT_EOM);
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = REWIND;
	scsi_cmd.immed=immed;
	if (st_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			0,
			0,
			immed?5000:300000, /* 5 sec or 5 min */
			flags) != 0)
	{
		if(!(flags & SCSI_SILENT))
			printf("could not rewind st%d\n", unit);
		st_info_valid[unit] = FALSE;
		return(FALSE);
	}
	return(TRUE);
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
int	st_scsi_cmd(unit,scsi_cmd,cmdlen,data_addr,datalen,timeout,flags)

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
	struct	st_data *st = st_data + unit;

	if(scsi_debug & PRINTROUTINES) printf("\nst_scsi_cmd%d ",unit);
	if(st->sc_sw)	/* If we have a scsi driver */
	{

		xs = &(st_scsi_xfer[unit]);
		if(!(flags & SCSI_NOMASK))
			s = splbio();
		st_xfer_block_wait[unit]++;	/* there is someone waiting */
		while (xs->flags & INUSE)
		{
			sleep(&st_xfer_block_wait[unit],PRIBIO+1);
		}
		st_xfer_block_wait[unit]--;
		xs->flags = INUSE;
		if(!(flags & SCSI_NOMASK))
			splx(s);

		/*******************************************************\
		* Fill out the scsi_xfer structure			*
		\*******************************************************/
		xs->flags	|=	flags;
		xs->adapter	=	st->ctlr;
		xs->targ	=	st->targ;
		xs->lu		=	st->lu;
		xs->retries	=	ST_RETRIES;
		xs->timeout	=	timeout;
		xs->cmd		=	scsi_cmd;
		xs->cmdlen	=	cmdlen;
		xs->data	=	data_addr;
		xs->datalen	=	datalen;
		xs->resid	=	datalen;
		xs->when_done	=	(flags & SCSI_NOMASK)
					?(int (*)())0
					:st_done;
		xs->done_arg	=	unit;
		xs->done_arg2	=	(int)xs;
retry:		xs->error	=	XS_NOERROR;
		xs->bp		=	0;
		retval = (*(st->sc_sw->scsi_cmd))(xs);
		switch(retval)
		{
		case	SUCCESSFULLY_QUEUED:
			s = splbio();
			while(!(xs->flags & ITSDONE))
				sleep(xs,PRIBIO+1);
			splx(s);

		case	HAD_ERROR:
		case	COMPLETE:
			switch(xs->error)
			{
			case	XS_NOERROR:
				retval = ESUCCESS;
				break;
			case	XS_SENSE:
				retval = (st_interpret_sense(unit,xs));
				/* only useful for reads */
				if (retval)
				{ /* error... don't care about filemarks */
					st->flags &= ~(ST_AT_FILEMARK
							| ST_AT_EOM);
				}
				else
				{
					xs->error = XS_NOERROR;
					retval = ESUCCESS;
				}
				break;
			case	XS_DRIVER_STUFFUP:
				retval = EIO;
				break;
			case    XS_TIMEOUT:
				if(xs->retries-- )
				{
					xs->flags &= ~ITSDONE;
					goto retry;
				}
				retval = EIO;
				break;
			case    XS_BUSY:
				if(xs->retries-- )
				{
					xs->flags &= ~ITSDONE;
					goto retry;
				}
				retval = EIO;
				break;
			default:
				retval = EIO;
				printf("st%d: unknown error category from scsi driver\n"
					,unit);
				break;
			}	
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
		xs->flags = 0;	/* it's free! */
		ststart(unit);
	}
	else
	{
		printf("st%d: not set up\n",unit);
		return(EINVAL);
	}
	return(retval);
}
/***************************************************************\
* Look at the returned sense and act on the error and detirmine	*
* The unix error number to pass back... (0 = report no error)	*
\***************************************************************/

int	st_interpret_sense(unit,xs)
int	unit;
struct	scsi_xfer	*xs;
{
	struct	scsi_sense_data *sense;
	int	key;
	int	silent = xs->flags & SCSI_SILENT;

	/***************************************************************\
	* If errors are ok, report a success				*
	\***************************************************************/
	if(xs->flags & SCSI_ERR_OK) return(ESUCCESS);

	/***************************************************************\
	* Get the sense fields and work out what CLASS			*
	\***************************************************************/
	sense = &(xs->sense);
	if(st_debug)
	{
		int count = 0;
		printf("code%x class%x valid%x\n"
				,sense->error_code
				,sense->error_class
				,sense->valid);
		printf("seg%x key%x ili%x eom%x fmark%x\n"
				,sense->ext.extended.segment
				,sense->ext.extended.sense_key
				,sense->ext.extended.ili
				,sense->ext.extended.eom
				,sense->ext.extended.filemark);
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
	switch(sense->error_class)
	{
	/***************************************************************\
	* If it's class 7, use the extended stuff and interpret the key	*
	\***************************************************************/
	case 7:
	{
		if(sense->ext.extended.eom)
		{
			st_data[unit].flags |= ST_AT_EOM;
		}

		if(sense->ext.extended.filemark)
		{
			st_data[unit].flags |= ST_AT_FILEMARK;
		}

		if(sense->ext.extended.ili)
		{
			if(sense->valid)
			{
				/*******************************\
				* In all ili cases, note that	*
				* the resid is non-0 AND not 	*
				* unchanged.			*
				\*******************************/
				xs->resid
				   = ntohl(*((long *)sense->ext.extended.info));
				if(xs->bp)
				{
					if(xs->resid < 0)
					{ /* never on block devices */
						/***********************\
						* it's only really bad	*
						* if we have lost data	*
						* (the record was 	*
						* bigger than the read)	*
						\***********************/
						return(EIO);
					}
				}	
			}
			else
			{ /* makes no sense.. complain */
				printf("BAD length error?");
			}
		}/* there may be some other error. check the rest */

		key=sense->ext.extended.sense_key;
		switch(key)
		{
		case	0x0:
			return(ESUCCESS);
		case	0x1:
			if(!silent)
			{
				printf("st%d: soft error(corrected) ", unit); 
				if(sense->valid)
				{
			   		printf("block no. %d (decimal)\n",
			  		(sense->ext.extended.info[0] <<24)|
			  		(sense->ext.extended.info[1] <<16)|
			  		(sense->ext.extended.info[2] <<8)|
			  		(sense->ext.extended.info[3] ));
				}
			 	else
				{
			 		printf("\n");
				}
			}
			return(ESUCCESS);
		case	0x2:
			if(!silent) printf("st%d: not ready\n ", unit); 
			return(ENODEV);
		case	0x3:
			if(!silent)
			{
				printf("st%d: medium error ", unit); 
				if(sense->valid)
				{
			   		printf("block no. %d (decimal)\n",
			  		(sense->ext.extended.info[0] <<24)|
			  		(sense->ext.extended.info[1] <<16)|
			  		(sense->ext.extended.info[2] <<8)|
			  		(sense->ext.extended.info[3] ));
				}
			 	else
				{
			 		printf("\n");
				}
			}
			return(EIO);
		case	0x4:
			if(!silent) printf("st%d: non-media hardware failure\n ",
				unit); 
			return(EIO);
		case	0x5:
			if(!silent) printf("st%d: illegal request\n ", unit); 
			return(EINVAL);
		case	0x6:
			if(!silent) printf("st%d: Unit attention.\n ", unit); 
			st_data[unit].flags &= ~(ST_AT_FILEMARK|ST_AT_EOM);
			st_info_valid[unit] = FALSE;
			if (st_data[unit].flags & ST_OPEN) /* TEMP!!!! */
				return(EIO);
			else
				return(ESUCCESS);
		case	0x7:
			if(!silent)
			{
				printf("st%d: attempted protection violation "
								, unit); 
				if(sense->valid)
				{
			   		printf("block no. %d (decimal)\n",
			  		(sense->ext.extended.info[0] <<24)|
			  		(sense->ext.extended.info[1] <<16)|
			  		(sense->ext.extended.info[2] <<8)|
			  		(sense->ext.extended.info[3] ));
				}
			 	else
				{
			 		printf("\n");
				}
			}
			return(EACCES);
		case	0x8:
			if(!silent)
			{
				printf("st%d: block wrong state (worm)\n "
							, unit); 
				if(sense->valid)
				{
			   		printf("block no. %d (decimal)\n",
			  		(sense->ext.extended.info[0] <<24)|
			  		(sense->ext.extended.info[1] <<16)|
			  		(sense->ext.extended.info[2] <<8)|
			  		(sense->ext.extended.info[3] ));
				}
			 	else
				{
			 		printf("\n");
				}
			}
			return(EIO);
		case	0x9:
			if(!silent) printf("st%d: vendor unique\n",
				unit); 
			return(EIO);
		case	0xa:
			if(!silent) printf("st%d: copy aborted\n ",
				unit); 
			return(EIO);
		case	0xb:
			if(!silent) printf("st%d: command aborted\n ",
				unit); 
			return(EIO);
		case	0xc:
			if(!silent)
			{
				printf("st%d: search returned\n ", unit); 
				if(sense->valid)
				{
			   		printf("block no. %d (decimal)\n",
			  		(sense->ext.extended.info[0] <<24)|
			  		(sense->ext.extended.info[1] <<16)|
			  		(sense->ext.extended.info[2] <<8)|
			  		(sense->ext.extended.info[3] ));
				}
			 	else
				{
			 		printf("\n");
				}
			}
			return(ESUCCESS);
		case	0xd:
			if(!silent) printf("st%d: volume overflow\n ",
				unit); 
			return(ENOSPC);
		case	0xe:
			if(!silent)
			{
			 	printf("st%d: verify miscompare\n ", unit); 
				if(sense->valid)
				{
			   		printf("block no. %d (decimal)\n",
			  		(sense->ext.extended.info[0] <<24)|
			  		(sense->ext.extended.info[1] <<16)|
			  		(sense->ext.extended.info[2] <<8)|
			  		(sense->ext.extended.info[3] ));
				}
			 	else
				{
			 		printf("\n");
				}
			}
			return(EIO);
		case	0xf:
			if(!silent) printf("st%d: unknown error key\n ",
				unit); 
			return(EIO);
		}
		break;
	}
	/***************************************************************\
	* If it's NOT class 7, just report it.				*
	\***************************************************************/
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
		{
			if(!silent) printf("st%d: error class %d code %d\n",
				unit,
				sense->error_class,
				sense->error_code);
		if(sense->valid)
			if(!silent) printf("block no. %d (decimal)\n",
			(sense->ext.unextended.blockhi <<16),
			+ (sense->ext.unextended.blockmed <<8),
			+ (sense->ext.unextended.blocklow ));
		}
		return(EIO);
	}
}

#if defined(OSF)

stsize(dev_t dev)
{
    printf("stsize()        -- not implemented\n");
    return(0);
}

stdump()
{
    printf("stdump()        -- not implemented\n");
    return(-1);
}

#endif /* defined(OSF) */

