/*
 * Written by Julian Elischer (julian@tfs.com)(now julian@DIALix.oz.au)
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
 * major changes by Julian Elischer (julian@jules.dialix.oz.au) May 1993
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

/* Defines for device specific stuff */
#define		PAGE_0_SENSE_DATA_SIZE	12
#define PAGESIZ 	4096
#define DEF_FIXED_BSIZE  512
#define STQSIZE		4
#define	ST_RETRIES	4


#define MODE(z)		(  (minor(z)       & 0x03) )
#define DSTY(z)         ( ((minor(z) >> 2) & 0x03) )
#define UNIT(z)		(  (minor(z) >> 4) )

#define LOW_DSTY  3
#define MED_DSTY  2
#define HIGH_DSTY  1

#define SCSI_2_MAX_DENSITY_CODE	0x17	/* maximum density code specified
					   in SCSI II spec. */
/***************************************************************\
* Define various devices that we know mis-behave in some way,	*
* and note how they are bad, so we can correct for them		*
\***************************************************************/
struct	modes
{
	int	quirks;		/* same definitions as in rogues */
	char	density;
	char	spare[3];
};
struct	rogues
{
	char	*name;
        char    *manu;
        char    *model;
        char    *version;
	int	quirks;		/* valid for all modes */
	struct	modes	modes[4];	
};

/* define behaviour codes (quirks) */
#define	ST_Q_NEEDS_PAGE_0	0x00001
#define	ST_Q_FORCE_FIXED_MODE	0x00002
#define	ST_Q_FORCE_VAR_MODE	0x00004

static struct rogues gallery[] = /* ends with an all null entry */
{
	{ "Such an old device ", "pre-scsi", " unknown model  ","????",
		0,
		{ {ST_Q_FORCE_FIXED_MODE,0},		/* minor  0,1,2,3 */
		  {ST_Q_FORCE_FIXED_MODE,QIC_24},	/* minor  4,5,6,7 */
		  {ST_Q_FORCE_VAR_MODE,HALFINCH_1600},	/* minor  8,9,10,11*/
		  {ST_Q_FORCE_VAR_MODE,HALFINCH_6250}	/* minor  12,13,14,15*/
		}
	},
	{ "Tandberg tdc3600", "TANDBERG", " TDC 3600","????",
		ST_Q_NEEDS_PAGE_0,
		{ {0,0},				/* minor  0,1,2,3*/
		  {ST_Q_FORCE_VAR_MODE,QIC_525},	/* minor  4,5,6,7*/
		  {0,QIC_150},				/* minor  8,9,10,11*/
		  {0,QIC_120}				/* minor  12,13,14,15*/
		}
	},
	{ "Archive  Viper 150", "ARCHIVE ", "VIPER 150","????",
		ST_Q_NEEDS_PAGE_0,
		{ {0,0},				/* minor  0,1,2,3*/
		  {0,QIC_150},				/* minor  4,5,6,7*/
		  {0,QIC_120},				/* minor  8,9,10,11*/
		  {0,QIC_24}				/* minor  12,13,14,15*/
		}
	},
	{(char *)0}
};


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

int	st_debug = 0;

int stattach();
int st_done();

struct	st_data
{
/*--------------------present operating parameters, flags etc.----------------*/
	int	flags;			/* see below 			      */
	int	blksiz;			/* blksiz we are using		      */
	int	density;		/* present density 		      */
	int	quirks;			/* quirks for the open mode	      */
	int	last_dsty;		/* last density used		      */
/*--------------------device/scsi parameters----------------------------------*/
	struct	scsi_switch *sc_sw;	/* address of scsi low level switch   */
	int	ctlr;			/* so they know which one we want     */
	int	targ;			/* our scsi target ID 		      */
	int	lu;			/* our scsi lu 			      */
/*--------------------parameters reported by the device ----------------------*/
	int	blkmin;			/* min blk size 		      */
	int	blkmax;			/* max blk size 		      */
/*--------------------parameters reported by the device for this media--------*/
	int	numblks;		/* nominal blocks capacity 	      */
	int	media_blksiz;		/* 0 if not ST_FIXEDBLOCKS 	      */
	int	media_density;		/* this is what it said when asked    */
/*--------------------quirks for the whole drive------------------------------*/
	int	drive_quirks;		/* quirks of this drive		      */
/*--------------------How we should set up when openning each minor device----*/
	struct	modes	modes[4];	/* plus more for each mode 	      */
/*--------------------storage for sense data returned by the drive------------*/
        unsigned char   sense_data[12]; /* additional sense data needed       */
                                        /* for mode sense/select. 	      */
}st_data[NST];
#define ST_INITIALIZED	0x01
#define	ST_INFO_VALID	0x02
#define ST_OPEN		0x04
#define	ST_WRITTEN	0x10
#define	ST_FIXEDBLOCKS	0x20
#define	ST_AT_FILEMARK	0x40
#define	ST_AT_EOM	0x80

#define	ST_PER_ACTION	(ST_AT_FILEMARK | ST_AT_EOM)
#define	ST_PER_OPEN	(ST_OPEN | ST_WRITTEN | ST_PER_ACTION)
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
	struct st_data *st;

	if(scsi_debug & PRINTROUTINES) printf("stattach: ");
	/*******************************************************\
	* Check we have the resources for another drive		*
	\*******************************************************/
	unit = next_st_unit++;
	if( unit >= NST)
	{
		printf("Too many scsi tapes..(%d > %d) reconfigure kernel\n",
				(unit + 1),NST);
		return(0);
	}
	st = st_data + unit;
	/*******************************************************\
	* Store information needed to contact our base driver	*
	\*******************************************************/
	st->sc_sw	=	scsi_switch;
	st->ctlr	=	ctlr;
	st->targ	=	targ;
	st->lu		=	lu;

	/*******************************************************\
	* Store information about default densities 		*
	\*******************************************************/
	st->modes[HIGH_DSTY].density	=	QIC_525;
	st->modes[MED_DSTY].density	=	QIC_150;
	st->modes[LOW_DSTY].density	=	QIC_120;

	/*******************************************************\
	* Check if the drive is a known criminal and take	*
	* Any steps needed to bring it into line		*
	\*******************************************************/
	st_identify_drive(unit);

	/*******************************************************\
	* Use the subdriver to request information regarding	*
	* the drive. We cannot use interrupts yet, so the	*
	* request must specify this.				*
	\*******************************************************/
	if(st_mode_sense(unit, SCSI_NOSLEEP |  SCSI_NOMASK | SCSI_SILENT))
	{
		if(st_test_ready(unit,SCSI_NOSLEEP | SCSI_NOMASK | SCSI_SILENT))
		{
			printf("\tst%d: tape present: %d blocks of %d bytes\n",
				unit, st->numblks, st->media_blksiz);
		}
		else
		{
			printf("\tst%d: drive empty\n", unit);
		}
	}
	else
	{
		printf("\tst%d: drive offline\n", unit);
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

#if defined(OSF)
  	st_window[unit] = (caddr_t)alloc_kva(SECSIZE*256+PAGESIZ);
#endif /* defined(OSF) */

	st->flags |= ST_INITIALIZED;
	return;

}

/***********************************************************************\
* Use the identify routine in 'scsiconf' to get drive info so we can	*
* Further tailor our behaviour.						*
\***********************************************************************/

st_identify_drive(unit)
int	unit;
{

	struct	st_data *st;
	struct scsi_inquiry_data        inqbuf;
	struct	rogues *finger;
        char    manu[32];
        char    model[32];
        char    model2[32];
        char    version[32];
	int	model_len;


	st = st_data + unit;
	/*******************************************************\
	* Get the device type information                       *
	\*******************************************************/
	if (scsi_inquire(st->ctlr, st->targ, st->lu, st->sc_sw, &inqbuf,
		SCSI_NOSLEEP | SCSI_NOMASK | SCSI_SILENT) != COMPLETE)
	{
		printf("	st%d: couldn't get device type, using default\n", unit);
		return;
	}
	if((inqbuf.version & SID_ANSII) == 0)
	{
		/***********************************************\
		* If not advanced enough, use default values    *
		\***********************************************/
		strncpy(manu,"pre-scsi",8);manu[8]=0;
		strncpy(model," unknown model  ",16);model[16]=0;
		strncpy(version,"????",4);version[4]=0;
	}
	else
	{
		strncpy(manu,inqbuf.vendor,8);manu[8]=0;
		strncpy(model,inqbuf.product,16);model[16]=0;
		strncpy(version,inqbuf.revision,4);version[4]=0;
	}
	 
	/*******************************************************\
	* Load the parameters for this kind of device, so we	*
	* treat it as appropriate for each operating mode 	*
	* Only check the number of characters in the array's	*
	* model entry, not the entire model string returned.	*
	\*******************************************************/
	finger = gallery;
	while(finger->name)
	{
		model_len = 0;
		while(finger->model[model_len] && (model_len < 32))
		{
			model2[model_len] = model[model_len];
			model_len++;
		}
		model2[model_len] = 0;
		if ((strcmp(manu, finger->manu) == 0 )
		&& (strcmp(model2, finger->model) == 0 ))
		{
			printf("	st%d: %s is a known rogue\n", unit,finger->name);
			st->modes[0]	=	finger->modes[0];
			st->modes[1]	=	finger->modes[1];
			st->modes[2]	=	finger->modes[2];
			st->modes[3]	=	finger->modes[3];
			st->drive_quirks=	finger->quirks;
			st->quirks	=	finger->quirks; /*start value*/
			break;
		}
		else
		{
			finger++;	/* go to next suspect */
		}
	}
}

/*******************************************************\
*	open the device.				*
\*******************************************************/
stopen(dev)
{
	int unit,mode,dsty;
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
		return(ENXIO);
	}
	/*******************************************************\
	* Only allow one at a time				*
	\*******************************************************/
	if(st->flags & ST_OPEN)
	{
		return(ENXIO);
	}
	/*******************************************************\
	* Set up the mode flags according to the minor number	*
	* ensure all open flags are in a known state		*
	* if it's a different mode, dump all cached parameters	*
	\*******************************************************/
	if(st->last_dsty != dsty)
		st->flags &= ~ST_INFO_VALID;
	st->last_dsty = dsty;
	st->flags &= ~ST_PER_OPEN;
	st->quirks = st->drive_quirks | st->modes[dsty].quirks;
	st->density = st->modes[dsty].density;

	if(scsi_debug & (PRINTROUTINES | TRACEOPENS))
		printf("stopen: dev=0x%x (unit %d (of %d))\n"
				,   dev,      unit,   NST);
	/*******************************************************\
	* Make sure the device has been initialised		*
	\*******************************************************/
	if (!(st->flags & ST_INITIALIZED))
		return(ENXIO);

	/*******************************************************\
	* Check that it is still responding and ok.		*
	\*******************************************************/
#ifdef	removing_this
	if(scsi_debug & TRACEOPENS)
		printf("device is ");
	if (!(st_req_sense(unit, 0)))	/* may get a 'unit attention' if new */
	{
		if(scsi_debug & TRACEOPENS)
			printf("not responding\n");
		return(ENXIO);
	}
	if(scsi_debug & TRACEOPENS)
		printf("ok\n");
#endif
	if(!(st_test_ready(unit,0)))
	{
		printf("st%d not ready\n",unit);
		return(EIO);
	}
	if(!(st_test_ready(unit,0))) /* first may get 'unit attn' */
	{
		printf("st%d not ready\n",unit);
		return(EIO);
	}

	/***************************************************************\
	* If the media is new, then make sure we give it a chance to	*
	* to do a 'load' instruction. Possibly the test ready 		*
	* may not read true until this is done.. check this! XXX	*
	\***************************************************************/
	if(!(st->flags & ST_INFO_VALID))		/* is media new? */
	{
		if(!st_load(unit,LD_LOAD,0))
		{
			return(EIO);
		}
	}

	/*******************************************************\
	* Load the physical device parameters			*
	* loads: blkmin, blkmax					*
	\*******************************************************/
	if(!st_rd_blk_lim(unit,0))
	{
		return(EIO);
	}

	/*******************************************************\
	* Load the media dependent parameters			*
	* includes: media_blksiz,media_density,numblks		*
	\*******************************************************/
	if(!st_mode_sense(unit,0))
	{
		return(EIO);
	}

	/*******************************************************\
	* From media parameters, device parameters and quirks,	*
	* work out how we should be setting outselves up	*
	\*******************************************************/
	if(! st_decide_mode(unit))
		return(ENXIO);

	if(!st_mode_select(unit,0,st->density))
	{
		return(EIO);
	}

	st->flags |= ST_INFO_VALID;

	st_prevent(unit,PR_PREVENT,0); /* who cares if it fails? */

	if(scsi_debug & TRACEOPENS)
		printf("Params loaded ");


	st->flags |= ST_OPEN;
	return(0);
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

/***************************************************************\
* Given all we know about the device, media, mode and 'quirks',	*
* make a decision as to how we should be set up.		*
\***************************************************************/
st_decide_mode(unit)
int	unit;
{
	struct st_data *st = st_data + unit;

	if(st->flags & ST_INFO_VALID) return TRUE;

	switch(st->quirks & (ST_Q_FORCE_FIXED_MODE | ST_Q_FORCE_VAR_MODE))
	{
	case	(ST_Q_FORCE_FIXED_MODE | ST_Q_FORCE_VAR_MODE):
		printf("st%d: bad quirks\n",unit);
		return FALSE;
	case 0:
		switch(st->density)
		{
		case	QIC_120:
		case	QIC_150:
			goto fixed;
		case	HALFINCH_800:
		case	HALFINCH_1600:
		case	HALFINCH_6250:
		case	QIC_525:
			goto var;
		default:
			break;
		}
		/*******************************************************\
		* There was once a reason that the tests below were	*
		* deemed insufficient, but I can't remember why..	*
		* If your drive needs these added to, let me know	*
		* and/or add a rogue entry.				*
		\*******************************************************/
		if(st->blkmin && (st->blkmin == st->blkmax))
			goto fixed;
		if(st->blkmin != st->blkmax)
			goto var;

	case	ST_Q_FORCE_FIXED_MODE:
fixed:
		st->flags |= ST_FIXEDBLOCKS;
		if(st->media_blksiz)
		{
			st->blksiz = st->media_blksiz;
		}
		else
		{
			if(st->blkmin)
			{
                		st->blksiz = st->blkmin;        /* just to make sure */
			}
			else
			{
				st->blksiz = DEF_FIXED_BSIZE;
			}
		}
		break;
	case	ST_Q_FORCE_VAR_MODE:
var:
		st->flags &= ~ST_FIXEDBLOCKS;
		st->blksiz = 0;
		break;
	}
	return(TRUE);
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
	if (bp->b_bcount == 0)
	{
		goto done;
	}

	/*******************************************************\
	* Odd sized request on fixed drives are verboten	*
	\*******************************************************/
	if(st_data[unit].flags & ST_FIXEDBLOCKS)
	{
		if(bp->b_bcount % st_data[unit].blksiz)
		{
			printf("st%d: bad request, must be multiple of %d\n",
				unit, st_data[unit].blksiz);
			bp->b_error = EIO;
			goto bad;
		}
	}
	/*******************************************************\
	* as are too-short requests on variable length drives.	*
	\*******************************************************/
	else if(bp->b_bcount < st_data[unit].blkmin)
	{
		printf("st%d: bad request, must not be less than %d\n",
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
	* (All a bit silly if we're only allowing 1 open but..) *
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
		cmd.byte2 |= SRWT_FIXED;
		lto3b(bp->b_bcount/st->blksiz,cmd.len);
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
			    if(!(xs->resid))
			    {
				/***************************************\
				* we apparently had a corrected error	*
				* or something.				*
				* pretend the error never happenned	*
				\***************************************/
				bp->b_flags &= ~B_ERROR;
				bp->b_error = 0;
				bp->b_resid = 0;
				break;
			    }
			    if ( xs->resid != xs->datalen )
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
				* UNDER MACH (CMU) and NetBSD:		*
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
/* how do I distinguish NetBSD? at present it's wrong for NetBsd */
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
					bp->b_resid *= st_data[unit].blksiz;
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
	int number,flags;
        struct  st_data *st;

	/*******************************************************\
	* Find the device that the user is talking about	*
	\*******************************************************/
	flags = 0;	/* give error messages, act on errors etc. */
	unit = UNIT(dev);
        st = st_data + unit;

	switch(cmd)
	{

	case MTIOCGET:
	    {
		struct mtget *g = (struct mtget *) arg;

		bzero(g, sizeof(struct mtget));
		g->mt_type = 0x7;	/* Ultrix compat */ /*?*/
                if (st->flags & ST_FIXEDBLOCKS) { 
                        g->mt_bsiz = st->blksiz;
                } else {
                        g->mt_bsiz = 0;
                }
		g->mt_dns_high		= st->modes[HIGH_DSTY].density;
		g->mt_dns_medium 	= st->modes[MED_DSTY].density;
		g->mt_dns_low		= st->modes[LOW_DSTY].density;
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
			if(!st_write_filemarks(unit,number,flags)) errcode = EIO;
			st_data[unit].flags &= ~ST_WRITTEN;
			break;
		case MTFSF:	/* forward space file */
			if(!st_space(unit,number,SP_FILEMARKS,flags)) errcode = EIO;
			break;
		case MTBSF:	/* backward space file */
			if(!st_space(unit,-number,SP_FILEMARKS,flags)) errcode = EIO;
			break;
		case MTFSR:	/* forward space record */
			if(!st_space(unit,number,SP_BLKS,flags)) errcode = EIO;
			break;
		case MTBSR:	/* backward space record */
			if(!st_space(unit,-number,SP_BLKS,flags)) errcode = EIO;
			break;
		case MTREW:	/* rewind */
			if(!st_rewind(unit,FALSE,flags)) errcode = EIO;
			break;
		case MTOFFL:	/* rewind and put the drive offline */
			if(st_rewind(unit,FALSE,flags))
			{
				st_prevent(unit,PR_ALLOW,0);
				st_load(unit,LD_UNLOAD,flags);
			}
			else
			{
				printf("rewind failed, unit still loaded\n");
			}
			break;
		case MTNOP:	/* no operation, sets status only */
		case MTCACHE:	/* enable controller cache */
		case MTNOCACHE:	/* disable controller cache */
			break;
                case MTSETBSIZ: /* Set block size for device */
                        if (st->blkmin == st->blkmax)
			{
                                /* This doesn't make sense for a */
                                /* real fixed block device */
				errcode = EINVAL;
                        }
			else
			{
                                if ( number == 0 )
				{
                                        /* Restoring original block size */
                                        st->flags &= ~ST_FIXEDBLOCKS; /*XXX*/
					st->blksiz = st->media_blksiz;
                                }
				else
				{
                                        if (number < st->blkmin || number > st->blkmax)
					{ 
						errcode = EINVAL;
                                        }
					else
					{
                                                st->blksiz = number;
                                                st->flags |= ST_FIXEDBLOCKS;
                                        }
                                }
                        }
                        break;

			/* How do we check that the drive can handle
			   the requested density ? */

                case MTSETHDNSTY: /* Set high density defaults for device */
			if (number < 0 || number > SCSI_2_MAX_DENSITY_CODE)
			{
				errcode = EINVAL;
			}
			else
			{ 
				st->modes[HIGH_DSTY].density = number;
			}
			break;

                case MTSETMDNSTY: /* Set medium density defaults for device */
			if (number < 0 || number > SCSI_2_MAX_DENSITY_CODE)
			{
				errcode = EINVAL;
			}
			else
			{
				st->modes[MED_DSTY].density = number;
			}
			break;

                case MTSETLDNSTY: /* Set low density defaults for device */
			if (number < 0 || number > SCSI_2_MAX_DENSITY_CODE)
			{
				errcode = EINVAL;
			}
			else
			{
				st->modes[LOW_DSTY].density = number;
			}
                        break;

		default:
			errcode = EINVAL;
		}
		break;
	    }
	case MTIOCIEOT:
	case MTIOCEEOT:
		break;
	default:
		errcode = EINVAL;
	}

	return errcode;
}


#ifdef removing_this
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
	return(TRUE);
}

#endif
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
			flags) != 0)
	{
		return(FALSE);
	}
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
	if ((st->flags & ST_INFO_VALID)) return TRUE;

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
		st->flags &= ~ST_INFO_VALID;
		return(FALSE);
	} 
	st->blkmin = b2tol(scsi_blkl.min_length);
	st->blkmax = _3btol(&scsi_blkl.max_length_2);

	if (st_debug)
	{
		printf(" (%d <= blksiz <= %d\n) ",st->blkmin,st->blkmax);
	}
}

/*******************************************************\
* Get the scsi driver to send a full inquiry to the	*
* device and use the results to fill out the global 	*
* parameter structure.					*
*							*
* called from:						*
* attach						*
* open							*
* ioctl (to reset original blksize)			*
\*******************************************************/
st_mode_sense(unit, flags)
int	unit,flags;
{
	int	scsi_sense_len;
	char	*scsi_sense_ptr;
	struct scsi_mode_sense		scsi_cmd;
	struct scsi_sense
	{
		struct	scsi_mode_header header;
		struct	blk_desc	blk_desc;
	}scsi_sense;

	struct scsi_sense_page_0
	{
		struct	scsi_mode_header header;
		struct	blk_desc	blk_desc;
                unsigned char   sense_data[PAGE_0_SENSE_DATA_SIZE];
                        /* Tandberg tape drives returns page 00 */
                        /* with the sense data, whether or not */
                        /* you want it( ie the don't like you  */
                        /* saying you want anything less!!!!!  */
                        /* They also expect page 00 */
                        /* back when you issue a mode select */
	}scsi_sense_page_0;
	struct	st_data *st = st_data + unit;
	
	/*******************************************************\
	* First check if we have it all loaded			*
	\*******************************************************/
	if ((st->flags & ST_INFO_VALID)) return(TRUE);

	/*******************************************************\
	* Define what sort of structure we're working with	*
	\*******************************************************/
	if (st->quirks & ST_Q_NEEDS_PAGE_0)
	{
		scsi_sense_len = sizeof(scsi_sense_page_0);
		scsi_sense_ptr = (char *) &scsi_sense_page_0;
	}
	else
	{
		scsi_sense_len = sizeof(scsi_sense);
		scsi_sense_ptr = (char *) &scsi_sense;
	}

	/*******************************************************\
	* Set up a mode sense 					*
	\*******************************************************/
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = MODE_SENSE;
	scsi_cmd.length = scsi_sense_len;

	/*******************************************************\
	* do the command, but we don't need the results		*
	* just print them for our interest's sake, if asked,	*
	* or if we need it as a template for the mode select	*
	* store it away.					*
	\*******************************************************/
	if (st_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			scsi_sense_ptr,
			scsi_sense_len,
			5000,
			flags | SCSI_DATA_IN) != 0)
	{
		if(!(flags & SCSI_SILENT))
			printf("could not mode sense for unit %d\n", unit);
		st->flags &= ~ST_INFO_VALID;
		return(FALSE);
	} 
	st->numblks = _3btol(&(((struct scsi_sense *)scsi_sense_ptr)->blk_desc.nblocks));
	st->media_blksiz = _3btol(&(((struct scsi_sense *)scsi_sense_ptr)->blk_desc.blklen));
	st->media_density = ((struct scsi_sense *)scsi_sense_ptr)->blk_desc.density;
	if (st_debug)
	{
		printf("unit %d: %d blocks of %d bytes, write %s, %sbuffered",
			unit,
			st->numblks,
			st->media_blksiz,
			((((struct scsi_sense *)scsi_sense_ptr)->header.dev_spec
				& SMH_DSP_WRITE_PROT) ?
				"protected" : "enabled"),
			((((struct scsi_sense *)scsi_sense_ptr)->header.dev_spec
				 & SMH_DSP_BUFF_MODE)?
				"" : "un")
			);
	}
	if (st->quirks & ST_Q_NEEDS_PAGE_0)
	{
		bcopy(((struct scsi_sense_page_0 *)scsi_sense_ptr)->sense_data, 
			st->sense_data, 
			sizeof(((struct scsi_sense_page_0 *)scsi_sense_ptr)->sense_data));
	}
	return(TRUE);
}

/*******************************************************\
* Send a filled out parameter structure to the drive to	*
* set it into the desire modes etc.			*
\*******************************************************/
st_mode_select(unit, flags, dsty_code)
int	unit,flags,dsty_code;
{
	int	dat_len;
	char	*dat_ptr;
	struct scsi_mode_select scsi_cmd;
	struct dat
	{
		struct	scsi_mode_header header;
		struct	blk_desc	blk_desc;
	}dat;
	struct dat_page_0
	{
		struct	scsi_mode_header header;
		struct	blk_desc	blk_desc;
                unsigned char   sense_data[PAGE_0_SENSE_DATA_SIZE];
	}dat_page_0;
	struct	st_data *st = st_data + unit;

	/*******************************************************\
	* Define what sort of structure we're working with	*
	\*******************************************************/
	if (st->quirks & ST_Q_NEEDS_PAGE_0)
	{
		dat_len = sizeof(dat_page_0);
		dat_ptr = (char *) &dat_page_0;
	}
	else
	{
		dat_len = sizeof(dat);
		dat_ptr = (char *) &dat;
	}
		
	/*******************************************************\
	* Set up for a mode select				*
	\*******************************************************/
	bzero(dat_ptr, dat_len);
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = MODE_SELECT;
	scsi_cmd.length = dat_len;
	((struct dat *)dat_ptr)->header.blk_desc_len = sizeof(struct  blk_desc);
	((struct dat *)dat_ptr)->header.dev_spec |= SMH_DSP_BUFF_MODE_ON;
        ((struct dat *)dat_ptr)->blk_desc.density = dsty_code;
	if(st->flags & ST_FIXEDBLOCKS)
	{
		lto3b( st->blksiz , ((struct dat *)dat_ptr)->blk_desc.blklen);
	}
	if (st->quirks & ST_Q_NEEDS_PAGE_0)
	{
		bcopy(st->sense_data, ((struct dat_page_0 *)dat_ptr)->sense_data, 
			sizeof(((struct dat_page_0 *)dat_ptr)->sense_data));
			/* the Tandberg tapes need the block size to */
			/* be set on each mode sense/select. */
	}
	/*******************************************************\
	* do the command					*
	\*******************************************************/
	if (st_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			dat_ptr,
			dat_len,
			5000,
			flags | SCSI_DATA_OUT) != 0)
	{
		if(!(flags & SCSI_SILENT))
			printf("could not mode select for unit %d\n", unit);
		st->flags &= ~ST_INFO_VALID;
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
	scsi_cmd.byte2 = what & SS_CODE;
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
		st_data[unit].flags &= ~ST_INFO_VALID;
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
		st_data[unit].flags &= ~ST_INFO_VALID;
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
	scsi_cmd.how=type;
	if (type == LD_LOAD)
	{
		/*scsi_cmd.how |= LD_RETEN;*/
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
		return(FALSE);
		st_data[unit].flags &= ~ST_INFO_VALID;
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
	scsi_cmd.how=type;
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
		st_data[unit].flags &= ~ST_INFO_VALID;
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
	scsi_cmd.byte2=immed?SR_IMMED:0?SR_IMMED:0;
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
		st_data[unit].flags &= ~ST_INFO_VALID;
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
	switch(sense->error_code & SSD_ERRCODE)
	{
	/***************************************************************\
	* If it's class 7, use the extended stuff and interpret the key	*
	\***************************************************************/
	case 0x70:
	{
		if(sense->ext.extended.flags & SSD_EOM)
		{
			st_data[unit].flags |= ST_AT_EOM;
		}

		if(sense->ext.extended.flags & SSD_FILEMARK)
		{
			st_data[unit].flags |= ST_AT_FILEMARK;
		}

		if(sense->ext.extended.flags & SSD_ILI)
		{
			if(sense->error_code & SSD_ERRCODE_VALID)
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

		key=sense->ext.extended.flags & SSD_KEY;
		switch(key)
		{
		case	0x0:
			if(!(sense->ext.extended.flags & SSD_ILI))
				xs->resid = 0; /* XXX check this */
			return(ESUCCESS);
		case	0x1:
			if(!silent)
			{
				printf("st%d: soft error(corrected) ", unit); 
				if(sense->error_code & SSD_ERRCODE_VALID)
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
			if(!(sense->ext.extended.flags & SSD_ILI))
				xs->resid = 0; /* XXX check this */
			return(ESUCCESS);
		case	0x2:
			if(!silent) printf("st%d: not ready\n ", unit); 
			return(ENODEV);
		case	0x3:
			if(!silent)
			{
				printf("st%d: medium error ", unit); 
				if(sense->error_code & SSD_ERRCODE_VALID)
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
			st_data[unit].flags &= ~ST_INFO_VALID;
			if (st_data[unit].flags & ST_OPEN) /* TEMP!!!! */
				return(EIO);
			else
				return(ESUCCESS);
		case	0x7:
			if(!silent)
			{
				printf("st%d: attempted protection violation "
								, unit); 
				if(sense->error_code & SSD_ERRCODE_VALID)
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
				printf("st%d: fixed block wrong size \n "
							, unit); 
				if(sense->error_code & SSD_ERRCODE_VALID)
				{
			   		printf("requested size: %d (decimal)\n",
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
				if(sense->error_code & SSD_ERRCODE_VALID)
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
				if(sense->error_code & SSD_ERRCODE_VALID)
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
	default:
		{
			if(!silent) printf("st%d: error code %d\n",
				unit,
				sense->error_code & SSD_ERRCODE);
		if(sense->error_code & SSD_ERRCODE_VALID)
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

