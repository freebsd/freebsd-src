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
 * 1.15 is the last verion to support MACH and OSF/1
 */
/* $Revision: 1.23 $ */

/*
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 * major changes by Julian Elischer (julian@jules.dialix.oz.au) May 1993
 *
 *	$Id: st.c,v 1.23 93/08/26 21:09:51 julian Exp Locker: julian $
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
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/mtio.h>


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
#define	ST_Q_SNS_HLP		0x00008

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
	{ "Rev 5 of the Archive 2525", "ARCHIVE ", "VIPER 2525 25462","-005",
		0,
		{ {ST_Q_SNS_HLP,0},			/* minor  0,1,2,3*/
		  {ST_Q_SNS_HLP,QIC_525},		/* minor  4,5,6,7*/
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
	{ "Wangdat Dat (1.3GB)", "WangDAT ", "Model 1300","????",
		0,
		{ {ST_Q_FORCE_VAR_MODE,0},		/* minor  0,1,2,3*/
		  {ST_Q_SNS_HLP,0},			/* minor  4,5,6,7*/
		  {ST_Q_FORCE_FIXED_MODE,0},		/* minor  8,9,10,11*/
		  {ST_Q_FORCE_VAR_MODE,0}		/* minor  12,13,14,15*/
		}
	},
	{(char *)0}
};


int	ststrategy();
void	stminphys();

#define ESUCCESS 0

#ifdef	STDEBUG
int	st_debug = 1;
#endif	/*STDEBUG*/

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
	struct	buf *buf_queue;		/* the queue of pending IO operations */
	struct	scsi_xfer scsi_xfer;	/* The scsi xfer struct for this drive*/
	int	xfer_block_wait;	/* whether there is a process waiting */
}*st_data[NST];
#define ST_INITIALIZED	0x01
#define	ST_INFO_VALID	0x02
#define ST_OPEN		0x04
#define	ST_BLOCK_SET	0x08		/* block size, mode set by ioctl      */
#define	ST_WRITTEN	0x10
#define	ST_FIXEDBLOCKS	0x20
#define	ST_AT_FILEMARK	0x40
#define	ST_EIO_PENDING	0x80		/* we couldn't report it then (had data)*/
#define	ST_AT_BOM	0x100		/* ops history suggests Beg of Medium */
#define	ST_READONLY	0x200		/* st_mode_sense says write protected */

#define	ST_PER_ACTION	(ST_AT_FILEMARK | ST_EIO_PENDING)
#define	ST_PER_OPEN	(ST_OPEN | ST_PER_ACTION)
#define	ST_PER_MEDIA	(ST_INFO_VALID | ST_BLOCK_SET | ST_WRITTEN | \
			ST_FIXEDBLOCKS | ST_AT_BOM | ST_READONLY | \
			ST_PER_ACTION)

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

#ifdef	STDEBUG
	if(scsi_debug & PRINTROUTINES) printf("stattach: ");
#endif	/*STDEBUG*/
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
	if(st_data[unit])
	{
		printf("st%d: Already has storage!\n",unit);
		return(0);
	}
	st = st_data[unit] = malloc(sizeof(struct st_data),M_DEVBUF,M_NOWAIT);
	if(!st)
	{
		printf("st%d: malloc failed in st.c\n",unit);
		return(0);
	}
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
		printf("st%d: drive offline\n", unit);
	}
	else
	{
		if(!st_test_ready(unit,SCSI_NOSLEEP | SCSI_NOMASK | SCSI_SILENT))
		{
			printf("st%d: density code 0x%x, ",
			    unit, st->media_density);
			if (st->media_blksiz)
			{
				printf("%d-byte", st->media_blksiz);
			}
			else
			{
				printf("variable");
			}
			printf(" blocks, write-%s\n",
			    st->flags & ST_READONLY ? "protected" : "enabled");
		}
		else
		{
			printf("st%d: drive empty\n", unit);
		}
	}
	/*******************************************************\
	* Set up the bufs for this device			*
	\*******************************************************/
	st->buf_queue	=	0;


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

	struct	st_data *st = st_data[unit];
	struct scsi_inquiry_data        inqbuf;
	struct	rogues *finger;
        char    manu[32];
        char    model[32];
        char    model2[32];
        char    version[32];
	int	model_len;


	/*******************************************************\
	* Get the device type information                       *
	\*******************************************************/
	if (scsi_inquire(st->ctlr, st->targ, st->lu, st->sc_sw, &inqbuf,
		SCSI_NOSLEEP | SCSI_NOMASK | SCSI_SILENT) != COMPLETE)
	{
		printf("st%d: couldn't get device type, using default\n", unit);
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
			&& (strcmp(model2, finger->model) == 0 ||
			   strcmp("????????????????", finger->model) == 0)
			&& (strcmp(version, finger->version) == 0 ||
			   strcmp("????", finger->version) == 0))
		{
			printf("st%d: %s is a known rogue\n", unit,finger->name);
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
	int	errno = 0;
	struct st_data *st;
	unit = UNIT(dev);
	mode = MODE(dev);
	dsty = DSTY(dev);

	/*******************************************************\
	* Check the unit is legal                               *
	\*******************************************************/
	if ( unit >= NST )
	{
		return(ENXIO);
	}
	st = st_data[unit];
	/*******************************************************\
	* Make sure the device has been initialised		*
	\*******************************************************/
	if ((st == NULL) || (!(st->flags & ST_INITIALIZED)))
		return(ENXIO);

	/*******************************************************\
	* Only allow one at a time				*
	\*******************************************************/
	if(st->flags & ST_OPEN)
	{
		return(ENXIO);
	}

	/*******************************************************\
	* Throw out a dummy instruction to catch 'Unit attention*
	* errors (the error handling will invalidate all our	*
	* device info if we get one, but otherwise, ignore it	*
	\*******************************************************/
	st_test_ready(unit, 0); 

	/***************************************************************\
	* Check that the device is ready to use	 (media loaded?)	*
	* This time take notice of the return result			*
	\***************************************************************/
	if(errno = (st_test_ready(unit,0))) 
	{
		printf("st%d: not ready\n",unit);
		return(errno);
	}

	/*******************************************************\
	* Set up the mode flags according to the minor number	*
	* ensure all open flags are in a known state		*
	* if it's a different mode, dump all cached parameters	*
	\*******************************************************/
	if(st->last_dsty != dsty || !(st->flags & ST_INFO_VALID))
	{
		st->flags &= ~ST_INFO_VALID;
		st->last_dsty = dsty;
		st->quirks = st->drive_quirks | st->modes[dsty].quirks;
		st->density = st->modes[dsty].density;
	}
	st->flags &= ~ST_PER_OPEN;

#ifdef	STDEBUG
	if(scsi_debug & (PRINTROUTINES | TRACEOPENS))
		printf("stopen: dev=0x%x (unit %d (of %d))\n"
				,   dev,      unit,   NST);
#endif	/*STDEBUG*/
	/***************************************************************\
	* If the media is new, then make sure we give it a chance to	*
	* to do a 'load' instruction.					*
	\***************************************************************/
	if(!(st->flags & ST_INFO_VALID))		/* is media new? */
	{
		if(errno = st_load(unit,LD_LOAD,0))
		{
			return(errno);
		}
		if(st->quirks & ST_Q_SNS_HLP)
		{
			/***********************************************\
			* The quirk here is that the drive returns some	*
			* value to st_mode_sense incorrectly until the	*
			* tape has actually passed by the head.		*
			*						*
			* The method is to set the drive to fixed-block	*
			* state (user-specified density and 512-byte	*
			* blocks), then read and rewind to get it to	*
			* sense the tape.  If the sense interpretation	*
			* code determines that the tape contains	*
			* variable-length blocks, set to variable-block	*
			* state and try again.  The result will be the	*
			* ability to do an accurate st_mode_sense.	*
			*						*
			* We pretend not to be at beginning of medium	*
			* to keep st_read from calling st_decide_mode.	*
			*						*
			* We know we can do a rewind because we just	*
			* did a load, which implies rewind.  Rewind	*
			* seems preferable to space backward if	we have	*
			* a virgin tape.				*
			* 						*
			* This is really a check fr 512 byte records	*
			* Needs more thought				*
			\***********************************************/
			char	*buf;

			buf = malloc(DEF_FIXED_BSIZE,M_TEMP,M_NOWAIT);
			if(!buf) return(ENOMEM);

			if (errno = st_mode_sense(unit, 0))
			{
				free(buf,M_TEMP);
				return(errno);
			}
			st->blksiz = DEF_FIXED_BSIZE;
			st->flags = ~ST_AT_BOM & st->flags | ST_FIXEDBLOCKS;
			if (errno = st_mode_select(unit, 0))
			{
				free(buf,M_TEMP);
				return(errno);
			}
			st_read(unit, buf, DEF_FIXED_BSIZE, 0);
			if (errno = st_rewind(unit, FALSE, 0))
			{
				free(buf,M_TEMP);
				return(errno);
			}
			if (st->blksiz == 0) /* set it back the way it was */
			{
				st->flags &= ~(ST_FIXEDBLOCKS | ST_AT_BOM);
				if (errno = st_mode_select(unit, 0))
				{
					free(buf,M_TEMP);
					return(errno);
				}
				st_read(unit, buf, 1, 0);
				if (errno = st_rewind(unit, FALSE, 0))
				{
					free(buf,M_TEMP);
					return(errno);
				}
			}
			free(buf,M_TEMP);
		}
	}
#ifdef	removing_this
#endif	removing_this


	/*******************************************************\
	* Load the physical device parameters			*
	* loads: blkmin, blkmax					*
	\*******************************************************/
	if(errno = st_rd_blk_lim(unit,0))
	{
		return(errno);
	}

	/*******************************************************\
	* Load the media dependent parameters			*
	* includes: media_blksiz,media_density,numblks		*
	\*******************************************************/
	if(errno = st_mode_sense(unit,0))
	{
		return(errno);
	}

	st->flags |= ST_INFO_VALID;

	st_prevent(unit,PR_PREVENT,0); /* who cares if it fails? */

#ifdef	STDEBUG
	if(scsi_debug & TRACEOPENS)
		printf("Params loaded ");
#endif	/*STDEBUG*/

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
	st = st_data[unit];

#ifdef	STDEBUG
	if(scsi_debug & TRACEOPENS)
		printf("Closing device");
#endif	/*STDEBUG*/
	if(st->flags & ST_WRITTEN)
	{
		st_write_filemarks(unit,1,0);
	}
	switch(mode)
	{
	case	0:
		st_rewind(unit,FALSE,SCSI_SILENT);
		st_prevent(unit,PR_ALLOW,SCSI_SILENT);
		st->flags &= ~ST_PER_MEDIA;
		break;
	case	1: /*non rewind*/
		/* possibly space forward if not already at EOF? */
		/* (fixed block mode only) */

		break;
	case	2:
		st_rewind(unit,FALSE,SCSI_SILENT);
		st_prevent(unit,PR_ALLOW,SCSI_SILENT);
		st_load(unit,LD_UNLOAD,SCSI_SILENT);
		st->flags &= ~ST_PER_MEDIA;
		break;
	case	3:/* a bit silly really */
		st_prevent(unit,PR_ALLOW,SCSI_SILENT);
		st_load(unit,LD_UNLOAD,SCSI_SILENT);
		st->flags &= ~ST_PER_MEDIA;
		break;
	default:
		printf("st%d: close: Bad mode (minor number)%d how's it open?\n"
				,unit,mode);
		return(EINVAL);
	}
	st->flags &= ~ST_PER_OPEN;
	return(0);
}

/***************************************************************\
* Given all we know about the device, media, mode, 'quirks' and	*
* initial operation, make a decision as to how we should be set	*
* up.  First, choose the density, then variable/fixed blocks.	*
\***************************************************************/
st_decide_mode(unit, first_read)
int	unit, first_read;
{
	int dsty, error;
	struct st_data *st = st_data[unit];

	/***************************************************************\
	* First, if our information about the tape is out of date, get	*
	* new information.						*
	\***************************************************************/
	if (!(st->flags & ST_INFO_VALID))
	{
		if (error = st_mode_sense(unit, 0))
			return (error);
		st->flags |= ST_INFO_VALID;
	}
#ifdef	STDEBUG
	if(st_debug) printf("starting mode decision\n");
#endif	/*STDEBUG*/

	/***************************************************************\
	* Second, set the density, and set the quirk flags as a		*
	* function of the density.					*
	\***************************************************************/
	st->quirks = st->drive_quirks;
	dsty = st->last_dsty; /* set in the open call */
	if (dsty > 0)
	{
		/*******************************************************\
		* If the user specified the density, believe him.	*
		\*******************************************************/
		st->density = st->modes[dsty].density;
		st->quirks |= st->modes[dsty].quirks;
	}
	else
	{
		/*******************************************************\
		* If the user defaulted the density, use the drive's	*
		* opinion of it.					*
		\*******************************************************/
		st->density = st->media_density;
		do {
			if (st->density == st->modes[dsty].density)
			{
				st->quirks |= st->modes[dsty].quirks;
#ifdef	STDEBUG
	if(st_debug) printf("selected density %d\n",dsty);
#endif	/*STDEBUG*/
				break; /* only out of the loop*/
			}
		} while (++dsty < 4);
		/*******************************************************\
		* If dsty got to 4, the drive must have reported a	*
		* density which isn't in our density list (e.g. QIC-24	*
		* for a default drive).  We can handle that, except	*
		* there'd better be no density-specific quirks in the	*
		* drive's behavior.					*
		\*******************************************************/
	}

	/***************************************************************\
	* If the user has already specified fixed or variable-length	*
	* blocks using an ioctl, just believe him. OVERRIDE ALL		*
	\***************************************************************/
	if (st->flags & ST_BLOCK_SET)
	{
#ifdef	STDEBUG
	if(st_debug) printf("user has specified %s\n",
		st->flags & ST_FIXEDBLOCKS ?  "variable mode" : "fixed mode");
#endif	/*STDEBUG*/
		goto done;
	}

	/***************************************************************\
	* If the user hasn't already specified fixed or variable-length	*
	* blocks and the block size (zero if variable-length), we'll	*
	* have to try to figure them out ourselves.			*
	*								*
	* Our first shot at a method is, "The quirks made me do it!"	*
	\***************************************************************/
	switch (st->quirks & (ST_Q_FORCE_FIXED_MODE | ST_Q_FORCE_VAR_MODE))
	{
	case	(ST_Q_FORCE_FIXED_MODE | ST_Q_FORCE_VAR_MODE):
		printf("st%d: bad quirks\n",unit);
		return (EINVAL);
	case	ST_Q_FORCE_FIXED_MODE:
		st->flags |= ST_FIXEDBLOCKS;
		if (st->blkmin && st->blkmin == st->blkmax)
			st->blksiz = st->blkmin;
		else if(st->media_blksiz > 0)
			st->blksiz = st->media_blksiz;
		else
			st->blksiz = DEF_FIXED_BSIZE;
#ifdef	STDEBUG
	if(st_debug) printf("Quirks force fixed mode\n");
#endif	/*STDEBUG*/
		goto done;
	case	ST_Q_FORCE_VAR_MODE:
		st->flags &= ~ST_FIXEDBLOCKS;
		st->blksiz = 0;
#ifdef	STDEBUG
	if(st_debug) printf("Quirks force variable mode\n");
#endif	/*STDEBUG*/
		goto done;
	}

	/***************************************************************\
	* If the drive can only handle fixed-length blocks and only at	*
	* one size, perhaps we should just do that.			*
	\***************************************************************/
	if (st->blkmin && (st->blkmin == st->blkmax))
	{
		st->flags |= ST_FIXEDBLOCKS;
		st->blksiz = st->blkmin;
#ifdef	STDEBUG
	if(st_debug) printf("blkmin == blkmax of %d\n",st->blkmin);
#endif	/*STDEBUG*/
		goto done;
	}

	/***************************************************************\
	* If the tape density mandates use of fixed or variable-length	*
	* blocks, comply.						*
	\***************************************************************/
	switch (st->density)
	{
	case	HALFINCH_800:
	case	HALFINCH_1600:
	case	HALFINCH_6250:
		st->flags &= ~ST_FIXEDBLOCKS;
		st->blksiz = 0;
#ifdef	STDEBUG
	if(st_debug) printf("density specified variable\n");
#endif	/*STDEBUG*/
		goto done;
	case	QIC_11:
	case	QIC_24:
	case	QIC_120:
	case	QIC_150:
		st->flags |= ST_FIXEDBLOCKS;
		if (st->media_blksiz > 0)
			st->blksiz = st->media_blksiz;
		else
			st->blksiz = DEF_FIXED_BSIZE;
#ifdef	STDEBUG
	if(st_debug) printf("density specified fixed\n");
#endif	/*STDEBUG*/
		goto done;
	}

	/***************************************************************\
	* If we're about to read the tape, perhaps we should choose	*
	* fixed or variable-length blocks and block size according to	*
	* what the drive found on the tape.				*
	* ****though it probably hasn't looked yet!****			*
	\***************************************************************/
	if (first_read)
	{
		if (st->media_blksiz == 0)
			st->flags &= ~ST_FIXEDBLOCKS;
		else
			st->flags |= ST_FIXEDBLOCKS;
		st->blksiz = st->media_blksiz;
#ifdef	STDEBUG
	if(st_debug) printf("Used media_blksiz of %d\n",st->media_blksiz);
#endif	/*STDEBUG*/
		goto done;
	}

	/***************************************************************\
	* If the drive says it can handle variable size blocks		*
	* and nothing has been specified until now, hey let's do it	*
	\***************************************************************/
	if (st->blkmin != st->blkmax)
	{
		st->flags &= ~ST_FIXEDBLOCKS;
		st->blksiz = 0;
#ifdef	STDEBUG
	if(st_debug) printf("blkmin != blkmax\n, select variable\n");
#endif	/*STDEBUG*/
		goto done;
	}

	/***************************************************************\
	* We're getting no hints from any direction.  Choose fixed-	*
	* length blocks arbitrarily.					*
	\***************************************************************/
	st->flags |= ST_FIXEDBLOCKS;
	st->blksiz = DEF_FIXED_BSIZE;
#ifdef	STDEBUG
	if(st_debug) printf("Give up and default to fixed mode\n");
#endif	/*STDEBUG*/
done:
	st->flags &= ~ST_AT_BOM;
	return (st_mode_select(unit, 0));
}

/*******************************************************\
* trim the size of the transfer if needed,		*
* called by physio					*
* basically the smaller of our min and the scsi driver's*
* minphys						*
\*******************************************************/
void	stminphys(bp)
struct buf	*bp;
{
	(*(st_data[UNIT(bp->b_dev)]->sc_sw->scsi_minphys))(bp);
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
	struct	buf	**dp;
	unsigned char unit;
	unsigned int opri;
	struct st_data *st;

	ststrats++;
	unit = UNIT((bp->b_dev));
	st = st_data[unit];
#ifdef	STDEBUG
	if(scsi_debug & PRINTROUTINES) printf("\nststrategy ");
	if(scsi_debug & SHOWREQUESTS) printf("st%d: %d bytes @ blk%d\n",
					unit,bp->b_bcount,bp->b_blkno);
#endif	/*STDEBUG*/
	/*******************************************************\
	* If it's a null transfer, return immediatly		*
	\*******************************************************/
	if (bp->b_bcount == 0)
	{
		goto done;
	}

	/*******************************************************\
	* If we're at beginning of medium, now is the time to	*
	* set medium access density, fixed or variable-blocks	*
	* and, if fixed, the block size.			*
	\*******************************************************/
	if (st->flags & ST_AT_BOM &&
	    (bp->b_error = st_decide_mode(unit, (bp->b_flags & B_READ) != 0)))
		goto bad;

	/*******************************************************\
	* Odd sized request on fixed drives are verboten	*
	\*******************************************************/
	if(st->flags & ST_FIXEDBLOCKS)
	{
		if(bp->b_bcount % st->blksiz)
		{
			printf("st%d: bad request, must be multiple of %d\n",
				unit, st->blksiz);
			bp->b_error = EIO;
			goto bad;
		}
	}
	/*******************************************************\
	* as are out-of-range requests on variable drives.	*
	\*******************************************************/
	else if(bp->b_bcount < st->blkmin || bp->b_bcount > st->blkmax)
	{
		printf("st%d: bad request, must be between %d and %d\n",
			unit, st->blkmin, st->blkmax);
		bp->b_error = EIO;
		goto bad;
	}

	stminphys(bp);
	opri = splbio();

	/*******************************************************\
	* Place it in the queue of activities for this tape	*
	* at the end (a bit silly because we only have on user..*
	* (but it could fork() ))				*
	\*******************************************************/
	dp = &(st->buf_queue);
	while (*dp) 
	{
		dp = &((*dp)->b_actf);
	}	
	*dp = bp;
	bp->b_actf = NULL,

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
        st = st_data[unit];

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

#ifdef	STDEBUG
		if (st_debug)
			printf("[sctape_sstatus: %x %x]\n",
				 mt->mt_op, mt->mt_count);
#endif	/*STDEBUG*/



		/* compat: in U*x it is a short */
		number = mt->mt_count;
		switch ((short)(mt->mt_op))
		{
		case MTWEOF:	/* write an end-of-file record */
			errcode = st_write_filemarks(unit,number,flags);
			break;
		case MTFSF:	/* forward space file */
			errcode = st_space(unit,number,SP_FILEMARKS,flags);
			break;
		case MTBSF:	/* backward space file */
			errcode = st_space(unit,-number,SP_FILEMARKS,flags);
			break;
		case MTFSR:	/* forward space record */
			errcode = st_space(unit,number,SP_BLKS,flags);
			break;
		case MTBSR:	/* backward space record */
			errcode = st_space(unit,-number,SP_BLKS,flags);
			break;
		case MTREW:	/* rewind */
			errcode = st_rewind(unit,FALSE,flags);
			break;
		case MTOFFL:	/* rewind and put the drive offline */
			if(st_rewind(unit,FALSE,flags))
			{
				printf("st%d: rewind failed, unit still loaded\n",
					unit);
			}
			else
			{
				st_prevent(unit,PR_ALLOW,0);
				st_load(unit,LD_UNLOAD,flags);
			}
			break;
		case MTNOP:	/* no operation, sets status only */
		case MTCACHE:	/* enable controller cache */
		case MTNOCACHE:	/* disable controller cache */
			break;
                case MTSETBSIZ: /* Set block size for device */
			if (st->blkmin && st->blkmin == st->blkmax ||
			    st->quirks & ST_Q_FORCE_FIXED_MODE)
				/* Per definition in mtio.h, this is a	*/
				/* no-op for a real fixed block device	*/
				break;
			if (!(st->flags & ST_AT_BOM) || number < 0 || number > 0
			    && (number < st->blkmin || number > st->blkmax))
			{
				errcode = EINVAL;
				break;
			}
			if (number == 0)
				st->flags &= ~ST_FIXEDBLOCKS;
			else
				st->flags |= ST_FIXEDBLOCKS;
			st->blksiz = number;
			st->flags |= ST_BLOCK_SET;
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

/*******************************************************\
* Do a synchronous read.				*
\*******************************************************/
int st_read(unit, buf, size, flags)
int	unit, size, flags;
char	*buf;
{
	int error;
	struct scsi_rw_tape scsi_cmd;
	struct st_data *st = st_data[unit];

	/*******************************************************\
	* If it's a null transfer, return immediatly		*
	\*******************************************************/
	if (size == 0)
	{
		return(ESUCCESS);
	}

	/*******************************************************\
	* If we're at beginning of medium, now is the time to	*
	* set medium access density, fixed or variable-blocks	*
	* and, if fixed, the block size.			*
	\*******************************************************/
	if (st->flags & ST_AT_BOM && (error = st_decide_mode(unit, TRUE)))
		return (error);

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = READ_COMMAND_TAPE;
	scsi_cmd.byte2 |= st->flags & ST_FIXEDBLOCKS ? SRWT_FIXED : 0;
	lto3b(scsi_cmd.byte2 & SRWT_FIXED ?
	    size / (st->blksiz ? st->blksiz : DEF_FIXED_BSIZE) : size,
	    scsi_cmd.len);
	return (st_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			buf,
			size,
			100000,
			NULL,
			flags | SCSI_DATA_IN));
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

	return (st_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			0,
			0,
			100000,
			NULL,
			flags));
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
	struct st_data *st = st_data[unit];
	int	errno;

	/*******************************************************\
	* First check if we have it all loaded			*
	\*******************************************************/
	if ((st->flags & ST_INFO_VALID)) return 0;

	/*******************************************************\
	* do a 'Read Block Limits'				*
	\*******************************************************/
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = READ_BLK_LIMITS;

	/*******************************************************\
	* do the command,	update the global values	*
	\*******************************************************/
	if ( errno = st_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			&scsi_blkl,
			sizeof(scsi_blkl),
			5000,
			NULL,
			flags | SCSI_DATA_IN))
	{
		return errno;
	} 
	st->blkmin = b2tol(scsi_blkl.min_length);
	st->blkmax = _3btol(&scsi_blkl.max_length_2);

#ifdef	STDEBUG
	if (st_debug)
	{
		printf("(%d <= blksiz <= %d)\n", st->blkmin, st->blkmax);
	}
#endif	/*STDEBUG*/
	return 0;
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
	int	errno;
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
	struct	st_data *st = st_data[unit];
	
	/*******************************************************\
	* First check if we have it all loaded			*
	\*******************************************************/
	if ((st->flags & ST_INFO_VALID)) return 0;

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
	if (errno = st_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			scsi_sense_ptr,
			scsi_sense_len,
			5000,
			NULL,
			flags | SCSI_DATA_IN) )
	{
		return errno;
	} 
	st->numblks = _3btol(&(((struct scsi_sense *)scsi_sense_ptr)->blk_desc.nblocks));
	st->media_blksiz = _3btol(&(((struct scsi_sense *)scsi_sense_ptr)->blk_desc.blklen));
	st->media_density = ((struct scsi_sense *)scsi_sense_ptr)->blk_desc.density;
	if (((struct scsi_sense *)scsi_sense_ptr)->header.dev_spec &
	    SMH_DSP_WRITE_PROT)
		st->flags |= ST_READONLY;
#ifdef	STDEBUG
	if (st_debug)
	{
		printf("st%d: density code 0x%x, %d-byte blocks, write-%s, ",
		    unit, st->media_density, st->media_blksiz,
		    st->flags & ST_READONLY ? "protected" : "enabled");
		printf("%sbuffered\n",
	    	    ((struct scsi_sense *)scsi_sense_ptr)->header.dev_spec
		    & SMH_DSP_BUFF_MODE ? "" : "un");
	}
#endif	/*STDEBUG*/
	if (st->quirks & ST_Q_NEEDS_PAGE_0)
	{
		bcopy(((struct scsi_sense_page_0 *)scsi_sense_ptr)->sense_data, 
			st->sense_data, 
			sizeof(((struct scsi_sense_page_0 *)scsi_sense_ptr)->sense_data));
	}
	return 0;
}

/*******************************************************\
* Send a filled out parameter structure to the drive to	*
* set it into the desire modes etc.			*
\*******************************************************/
st_mode_select(unit, flags)
int	unit, flags;
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
	struct	st_data *st = st_data[unit];

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
	((struct dat *)dat_ptr)->blk_desc.density = st->density;
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
	return (st_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			dat_ptr,
			dat_len,
			5000,
			NULL,
			flags | SCSI_DATA_OUT) );
}

/*******************************************************\
* skip N blocks/filemarks/seq filemarks/eom		*
\*******************************************************/
st_space(unit,number,what,flags)
int	unit,number,what,flags;
{
	int error;
	struct scsi_space scsi_cmd;
	struct	st_data *st = st_data[unit];

	/*******************************************************\
	* If we're at beginning of medium, now is the time to	*
	* set medium access density, fixed or variable-blocks	*
	* and, if fixed, the block size.			*
	\*******************************************************/
	if (st->flags & ST_AT_BOM &&
	    (error = st_decide_mode(unit, TRUE)))
		return (error);

	/* if we are at a filemark now, we soon won't be*/
	st->flags &= ~ST_PER_ACTION;
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = SPACE;
	scsi_cmd.byte2 = what & SS_CODE;
	lto3b(number,scsi_cmd.number);
	return (st_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			0,
			0,
			600000, /* 10 mins enough? */
			NULL,
			flags));
}
/*******************************************************\
* write N filemarks					*
\*******************************************************/
st_write_filemarks(unit,number,flags)
int	unit,number,flags;
{
	int error;
	struct scsi_write_filemarks scsi_cmd;
	struct	st_data	*st = st_data[unit];

	/*******************************************************\
	* It's hard to write a negative number of file marks.	*
	* Don't try.						*
	\*******************************************************/
	if (number < 0)
		return (EINVAL);

	/*******************************************************\
	* If we're at beginning of medium, now is the time to	*
	* set medium access density, fixed or variable-blocks	*
	* and, if fixed, the block size.			*
	\*******************************************************/
	if (st->flags & ST_AT_BOM &&
	    (error = st_decide_mode(unit, FALSE)))
		return (error);

	st->flags &= ~(ST_AT_FILEMARK | ST_WRITTEN);
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = WRITE_FILEMARKS;
	lto3b(number,scsi_cmd.number);
	return (st_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			0,
			0,
			100000, /* 10 secs.. (may need to repos head )*/
			NULL,
			flags) );
}
/*******************************************************\
* load/unload (with retension if true)			*
\*******************************************************/
st_load(unit,type,flags)
int	unit,type,flags;
{
	struct  scsi_load  scsi_cmd;
	struct	st_data	*st = st_data[unit];

	st->flags &= ~ST_PER_ACTION;
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = LOAD_UNLOAD;
	scsi_cmd.how=type;
	if (type == LD_LOAD)
	{
		/*scsi_cmd.how |= LD_RETEN;*/
		st->flags |= ST_AT_BOM;
	}
	else
		st->flags &= ~ST_INFO_VALID;
	return (st_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			0,
			0,
			300000, /* 5 min */
			NULL,
			flags));
}
/*******************************************************\
* Prevent or allow the user to remove the tape		*
\*******************************************************/
st_prevent(unit,type,flags)
int	unit,type,flags;
{
	struct	scsi_prevent	scsi_cmd;

	if (type == PR_ALLOW)
		st_data[unit]->flags &= ~ST_INFO_VALID;
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = PREVENT_ALLOW;
	scsi_cmd.how=type;
	return (st_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			0,
			0,
			5000,
			NULL,
			flags));
}
/*******************************************************\
*  Rewind the device					*
\*******************************************************/
st_rewind(unit,immed,flags)
int	unit,immed,flags;
{
	struct	scsi_rewind	scsi_cmd;
	struct	st_data *st = st_data[unit];

	st->flags &= ~ST_PER_ACTION;
	st->flags |= ST_AT_BOM;
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = REWIND;
	scsi_cmd.byte2 = immed ? SR_IMMED : 0;
	return (st_scsi_cmd(unit,
			&scsi_cmd,
			sizeof(scsi_cmd),
			0,
			0,
			immed?5000:300000, /* 5 sec or 5 min */
			NULL,
			flags));
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
	struct	scsi_rw_tape	cmd;
	int			blkno, nblk;
	struct	st_data 	*st = st_data[unit];
	int			flags;



#ifdef	STDEBUG
	if(scsi_debug & PRINTROUTINES) printf("ststart%d ",unit);
#endif	/*STDEBUG*/
	/*******************************************************\
	* See if there is a buf to do and we are not already	*
	* doing one						*
	\*******************************************************/
	if(st->scsi_xfer.flags & INUSE)
	{
		return;    /* unit already underway */
	}
trynext:
	if(st->xfer_block_wait) /* a special awaits, let it proceed first */
	{
		wakeup(&(st->xfer_block_wait));
		return;
	}

	if ((bp = st->buf_queue) == NULL)
	{
		return;	/* no work to bother with */
	}
	st->buf_queue = bp->b_actf;



	/*******************************************************\
	* only FIXEDBLOCK devices have pending operations	*
	\*******************************************************/
	if(st->flags & ST_FIXEDBLOCKS)
	{
		/*******************************************************\
		*  If we are at a filemark but have not reported it yet	*
		* then we should report it now				*
		\*******************************************************/
		if(st->flags & ST_AT_FILEMARK)
		{
			bp->b_resid = bp->b_bcount;
			bp->b_error = 0;
			bp->b_flags &= ~B_ERROR;
			st->flags &= ~ST_AT_FILEMARK;
			biodone(bp);
			goto trynext;
		}
		/*******************************************************\
		*  If we are at EIO (e.g. EOM) but have not reported it	*
		* yet then we should report it now			*
		\*******************************************************/
		if(st->flags & ST_EIO_PENDING)
		{
			bp->b_resid = bp->b_bcount;
			bp->b_error = EIO;
			bp->b_flags |= B_ERROR;
			st->flags &= ~ST_EIO_PENDING;
			biodone(bp);
			goto trynext;
		}
	}
	/*******************************************************\
	*  Fill out the scsi command				*
	\*******************************************************/
	bzero(&cmd, sizeof(cmd));
	if((bp->b_flags & B_READ) == B_WRITE)
	{
		cmd.op_code = WRITE_COMMAND_TAPE;
		st->flags |= ST_WRITTEN;
		flags = SCSI_DATA_OUT;
	}
	else
	{
		cmd.op_code = READ_COMMAND_TAPE;
		flags = SCSI_DATA_IN;
	}

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
	* go ask the adapter to do all this for us		*
	\*******************************************************/
	if (st_scsi_cmd(unit,
			&cmd,
			sizeof(cmd),
			(u_char *)bp->b_un.b_addr,
			bp->b_bcount,
			100000,
			bp,
			flags | SCSI_NOSLEEP ) != SUCCESSFULLY_QUEUED)

	{
		printf("st%d: oops not queued\n",unit);
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
		biodone(bp);
		return;
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
	struct	st_data	*st = st_data[unit];

#ifdef	STDEBUG
	if(scsi_debug & PRINTROUTINES) printf("st_done%d ",unit);
#endif	/*STDEBUG*/
#ifdef	PARANOID
	if (! (xs->flags & INUSE))
		panic("scsi_xfer not in use!");
#endif	/*PARANOID*/
	if((bp = xs->bp)== NULL)
	{
		wakeup(xs);
		return;
	}
	switch(xs->error)
	{
	case	XS_NOERROR:
		bp->b_flags &= ~B_ERROR;
		bp->b_error = 0;
		bp->b_resid = 0;
		break;
	case	XS_SENSE:
		retval = (st_interpret_sense(unit,xs));
		bp->b_resid = xs->resid; /* already multiplied by blksiz */
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
			* All status is now suspect		*
			\***************************************/
			bp->b_flags |= B_ERROR;
			bp->b_error = retval;
			bp->b_resid =  bp->b_bcount;
			st->flags &= ~ST_PER_ACTION;
		}
		else
		{
		/***********************************************\
		* The error interpretation code has declared	*
		* that it wasn't a real error, or at least that	*
		* we should be ignoring it if it was.		*
		\***********************************************/
		    if(xs->resid == 0)
		    {
			/***************************************\
			* we apparently had a corrected error	*
			* or something.				*
			* pretend the error never happenned	*
			\***************************************/
			bp->b_flags &= ~B_ERROR;
			bp->b_error = 0;
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
			* UNDER NetBSD:				*
			* To indicate the same as above, we	*
			* need only have a non 0 resid that is	*
			* less than the b_bcount, but the	*
			* ERROR BIT MUST BE CLEAR! (sigh) 	*
			*					*
			*-------for fixed blocksize device------*
			* We read some successful records	*
			* before hitting the EOF or EOT. These	*
			* must be passed to the user, before we	*
			* report the EOx.  We will report the	*
			* EOx NEXT time.			*
			\***************************************/
#ifdef	NETBSD 
			bp->b_flags &= ~B_ERROR;
#else
			bp->b_flags |= B_ERROR;
#endif	
			bp->b_error = 0;
			xs->error = XS_NOERROR;
			break;
		    }
		    else
		    {
			/***************************************\
			* We have come out of the error handler	*
			* with no error code.  We have also not	*
			* transferred any data (would have gone	*
			* to the previous clause).		*
			* This must be an EOF			*
			*  Any caller request to read no	*
			* data would have been short-circuited	*
			* at st_read or ststrategy.		*
			*					*
			* At least all o/s agree that:		*
			* 0 bytes read with no error is EOF	*
			\***************************************/

			bp->b_error = 0;
			bp->b_flags &= ~B_ERROR;
			st->flags &= ~ST_AT_FILEMARK;
			break;
		    }
		}
		break;

	case    XS_TIMEOUT:
		printf("st%d timeout\n",unit);

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
			if ( (*(st->sc_sw->scsi_cmd))(xs)
				== SUCCESSFULLY_QUEUED)
			{       /* don't wake the job, ok? */
				return;
			}
			printf("st%d: device busy\n",unit);
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



/*******************************************************\
* ask the scsi driver to perform a command for us.	*
* Call it through the switch table, and tell it which	*
* sub-unit we want, and what target and lu we wish to	*
* talk to. Also tell it where to find the command	*
* how long int is.					*
* Also tell it where to read/write the data, and how	*
* long the data is supposed to be			*
\*******************************************************/
int	st_scsi_cmd(unit,scsi_cmd,cmdlen,data_addr,datalen,timeout,bp,flags)

int	unit,flags;
struct	scsi_generic *scsi_cmd;
int	cmdlen;
int	timeout;
u_char	*data_addr;
struct	buf *bp;
int	datalen;
{
	struct	scsi_xfer *xs;
	int	retval;
	int	s;
	struct	st_data *st = st_data[unit];

#ifdef	STDEBUG
	if(scsi_debug & PRINTROUTINES) printf("\nst_scsi_cmd%d ",unit);
#endif	/*STDEBUG*/
#ifdef	PARANOID
	if(st->sc_sw == NULL)	/* If we have no scsi driver */
	{
		printf("st%d: not set up\n",unit);
		return(EINVAL);
	}
#endif	/*PARANOID*/

	xs = &(st->scsi_xfer);
	if(!(flags & SCSI_NOMASK))
		s = splbio();
	st->xfer_block_wait++;	/* there is someone waiting */
	while (xs->flags & INUSE)
	{
		if(flags & SCSI_NOSLEEP)
			return EBUSY;
		sleep(&(st->xfer_block_wait),PRIBIO+1);
	}
	st->xfer_block_wait--;
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
	xs->retries	=	bp?0:ST_RETRIES;/*can't retry on IO*/
	xs->timeout	=	timeout;
	xs->cmd		=	scsi_cmd;
	xs->cmdlen	=	cmdlen;
	xs->data	=	data_addr;
	xs->datalen	=	datalen;
	xs->resid	=	datalen;
	xs->when_done	=	st_done;
	xs->done_arg	=	unit;
	xs->done_arg2	=	(int)xs;
	xs->bp		=	bp;
retry:	xs->error	=	XS_NOERROR;

	/***********************************************\
	* Ask the adapter to do the command for us	*
	\***********************************************/
	retval = (*(st->sc_sw->scsi_cmd))(xs);

	/***********************************************\
	* IO operations are handled differently..	*
	* Physio does the sleep, and error handling is	*
	* Done in st_done at interrupt time		*
	\***********************************************/
	if(bp) return retval;	

	/***********************************************\
	* Wait for the result if queued, or handle the	*
	* error if it was rejected..			*
	\***********************************************/
	switch(retval)
	{
	case	SUCCESSFULLY_QUEUED:
		s = splbio();
		while(!(xs->flags & ITSDONE))
			sleep(xs,PRIBIO+1);
		splx(s);
		/*******************************\
		* finished.. check for failure	*
		* Fall through......		*
		\*******************************/
	case	HAD_ERROR:
	case	COMPLETE:
		switch(xs->error)
		{
		case	XS_NOERROR:
			retval = ESUCCESS;
			break;
		case	XS_SENSE:
			retval = (st_interpret_sense(unit,xs));
			/* only useful for reads *//* why did I say that?*/
			if (retval)
			{ /* error... don't care about filemarks */
				st->flags &= ~ST_PER_ACTION;
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
		case    XS_BUSY:
			/* should sleep 1 sec here */
		case    XS_TIMEOUT:
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
			/* should delay here */
			goto retry;
		}
		retval = EIO;
		break;
	default:
		retval = EIO;
	}
	xs->flags = 0;	/* it's free! */
	ststart(unit);
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
	struct	st_data	*st = st_data[unit];
	int	info;
	int	errno = 0;/* default error */

	/***************************************************************\
	* If errors are ok, report a success				*
	\***************************************************************/
	if(xs->flags & SCSI_ERR_OK) return(ESUCCESS);

	/***************************************************************\
	* Get the sense fields and work out what code			*
	\***************************************************************/
	sense = &(xs->sense);
#ifdef	STDEBUG
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
#endif	/*STDEBUG*/
	if(sense->error_code & SSD_ERRCODE_VALID)
	{
		info = ntohl(*((long *)sense->ext.extended.info));
	}
	else
	{
		info = xs->datalen; /* bad choice if fixed blocks */
	}


	switch(sense->error_code & SSD_ERRCODE)
	{
	/***************************************************************\
	* If it's code 70, use the extended stuff and interpret the key	*
	\***************************************************************/
	case 0x70:
	{
		if(st->flags & ST_FIXEDBLOCKS)
		{
			xs->resid = info * st->blksiz;
			if(sense->ext.extended.flags & SSD_EOM)
			{
				st->flags |= ST_EIO_PENDING;
			}
			if(sense->ext.extended.flags & SSD_FILEMARK)
			{
				st->flags |= ST_AT_FILEMARK;
			}
			if(sense->ext.extended.flags & SSD_ILI)
			{
				st->flags |= ST_EIO_PENDING;
		/*******************************************************\
		* The following quirk code deals with the fact that the	*
		* value of media_blksiz returned by the drive in	*
		* response to an st_mode_sense call doesn't reflect the	*
		* tape's format.					*
		*							*
		* The method is to try a fixed-length block read and	*
		* let this code determine whether the tape has fixed or	*
		* variable-length blocks, by seeing if we get an ILI	*
		*							*
		* Since the quirk code is the only place that calls the *
		* board without the INFO_VALID set, catch that case	*
		* and handle it here (no other way of catching it	*
		* because resid info can't get passed without a buf) 	*
		\*******************************************************/

				if ((st->quirks & ST_Q_SNS_HLP)
				&& !(st->flags & ST_INFO_VALID))
				{
					st->blksiz = 0;	
				}
			}
			/***********************************************\
			* If no data was tranfered, do it immediatly	*
			\***********************************************/
			if(xs->resid == xs->datalen)
			{
				if(st->flags & ST_EIO_PENDING) 
				{
					return EIO;
				}
				if(st->flags & ST_AT_FILEMARK) 
				{
					return 0;
				}
			}
		}
		else
		{
			xs->resid = xs->datalen; /* to be sure */
			if(sense->ext.extended.flags & SSD_EOM)
			{
				return(EIO);
			}
			if(sense->ext.extended.flags & SSD_FILEMARK)
			{
				return 0;
			}
			if(sense->ext.extended.flags & SSD_ILI)
			{
				if(info < 0)
				/***************************************\
				* the record was bigger than the read	*
				\***************************************/
				{
					printf("st%d: record of %d bytes too big\n",
						unit, xs->datalen - info);
					return(EIO);
				}
				xs->resid = info;
			}
		}/* there may be some other error. check the rest */

		key=sense->ext.extended.flags & SSD_KEY;
		switch(key)
		{
		case	0x0:
			if(xs->resid == xs->datalen) xs->resid = 0;
			return(ESUCCESS);
		case	0x1:
			if(xs->resid == xs->datalen) xs->resid = 0;
			if(!silent)
			{
				printf("st%d: soft error(corrected)", unit); 
				if(sense->error_code & SSD_ERRCODE_VALID)
				{
			   		printf("info = %d (decimal)",
						info);
				}
		 		printf("\n");
			}
			return(ESUCCESS);
		case	0x2:
			if(!silent) printf("st%d: not ready\n", unit); 
			return(ENODEV);
		case	0x3:
			if(!silent)
			{
				printf("st%d: medium error", unit); 
				if(sense->error_code & SSD_ERRCODE_VALID)
				{
			   		printf(" block no. %d (decimal)",
						info);
				}
		 		printf("\n");
			}
			return(EIO);
		case	0x4:
			if(!silent) printf("st%d: non-media hardware failure\n",
				unit); 
			return(EIO);
		case	0x5:
			if(!silent) printf("st%d: illegal request\n", unit); 
			return(EINVAL);
		case	0x6:
			if(!silent) printf("st%d: Unit attention\n", unit); 
			st->flags &= ~ST_PER_MEDIA;
			if (st->flags & ST_OPEN) /* TEMP!!!! */
				return(EIO);
			else
				return(ESUCCESS);
		case	0x7:
			if(!silent)
				printf("st%d: tape is write protected\n", unit); 
			return(EACCES);
		case	0x8:
			if(!silent)
			{
				printf("st%d: no data found", unit);
				if(sense->error_code & SSD_ERRCODE_VALID)
			   		printf(": requested size: %d (decimal)",
					    info);
		 		printf("\n");
			}
			return(EIO);
		case	0x9:
			if(!silent) printf("st%d: vendor unique\n",
				unit); 
			return(EIO);
		case	0xa:
			if(!silent) printf("st%d: copy aborted\n",
				unit); 
			return(EIO);
		case	0xb:
			if(!silent) printf("st%d: command aborted\n",
				unit); 
			return(EIO);
		case	0xc:
			if(!silent)
			{
				printf("st%d: search returned", unit); 
				if(sense->error_code & SSD_ERRCODE_VALID)
				{
			   		printf(" block no. %d (decimal)",
						info);
				}
		 		printf("\n");
			}
			return(ESUCCESS);
		case	0xd:
			if(!silent) printf("st%d: volume overflow\n",
				unit); 
			return(ENOSPC);
		case	0xe:
			if(!silent)
			{
			 	printf("st%d: verify miscompare", unit); 
				if(sense->error_code & SSD_ERRCODE_VALID)
				{
			   		printf(" block no. %d (decimal)",
						info);
				}
		 		printf("\n");
			}
			return(EIO);
		case	0xf:
			if(!silent) printf("st%d: unknown error key\n",
				unit); 
			return(EIO);
		}
		break;
	}
	/***************************************************************\
	* If it's NOT code 70, just report it.				*
	\***************************************************************/
	default:
		{
			if(!silent) printf("st%d: error code %d",
				unit,
				sense->error_code & SSD_ERRCODE);
			if(sense->error_code & SSD_ERRCODE_VALID)
			{
				if(!silent)
				{
					printf(" block no. %d (decimal)",
					(sense->ext.unextended.blockhi <<16),
					+ (sense->ext.unextended.blockmed <<8),
					+ (sense->ext.unextended.blocklow ));
				}
				printf("\n");
			}
		}
		return(EIO);
	}
}
