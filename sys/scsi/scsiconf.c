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
 */

/*
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 */

/*
 * $Log:
 * 23 May 93  Rodney W. Grimes        ADDED Pioneer DRM-600 cd changer
 *
 */

#include <sys/types.h>
#include "st.h"
#include "sd.h"
#include "ch.h"
#include "cd.h"

#ifdef	MACH
#include <i386/machparam.h>
#endif	MACH
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#if !defined(OSF) && !defined(__386BSD__)
#include "bll.h"
#include "cals.h"
#include "kil.h"
#else
#define	NBLL 0
#define	NCALS 0
#define	NKIL 0
#endif /* !defined(OSF) && !defined(__386BSD__) */

#if NSD > 0
extern	sdattach();
#endif NSD
#if NST > 0
extern	stattach();
#endif NST
#if NCH > 0
extern	chattach();
#endif NCH
#if NCD > 0
extern	cdattach();
#endif NCD
#if NBLL > 0
extern	bllattach();
#endif NBLL
#if NCALS > 0
extern	calsattach();
#endif NCALS
#if NKIL > 0
extern	kil_attach();
#endif NKIL

/***************************************************************\
* The structure of pre-configured devices that might be turned	*
* off and therefore may not show up				*
\***************************************************************/
struct	predefined
{
	u_char	scsibus;
	u_char	dev;
	u_char	lu;
	int	(*attach_rtn)();
	char	*devname;
	char	flags;
}
pd[] = 
{
#ifdef EXAMPLE_PREDEFINE
#if NSD > 0
	{0,0,0,sdattach,"sd",0},/* define a disk at scsibus=0 dev=0 lu=0 */
#endif NSD
#endif EXAMPLE_PREDEFINE
	{0,9,9}			/*illegal dummy end entry */
};


/***************************************************************\
* The structure of known drivers for autoconfiguration		*
\***************************************************************/
static struct scsidevs 
{
	int type;
	int removable;
	char	*manufacturer;
	char	*model;
	char	*version;
	int	(*attach_rtn)();
	char	*devname;
	char	flags;		/* 1 show my comparisons during boot(debug) */
}
#define SC_SHOWME	0x01
#define	SC_ONE_LU	0x00
#define	SC_MORE_LUS	0x02
knowndevs[] = {
#if NSD > 0
	{ T_DIRECT,T_FIXED,"standard","any"
			,"any",sdattach,"sd",SC_ONE_LU },
	{ T_DIRECT,T_FIXED,"MAXTOR  ","XT-4170S        "
			,"B5A ",sdattach,"mx1",SC_ONE_LU },
#endif NSD
#if NST > 0
	{ T_SEQUENTIAL,T_REMOV,"standard","any"
			,"any",stattach,"st",SC_ONE_LU },
#endif NST
#if NCALS > 0
	{ T_PROCESSOR,T_FIXED,"standard","any"
			,"any",calsattach,"cals",SC_MORE_LUS },
#endif NCALS
#if NCH > 0
	{ T_CHANGER,T_REMOV,"standard","any"
			,"any",chattach,"ch",SC_ONE_LU },
#endif NCH
#if NCD > 0
	{ T_READONLY,T_REMOV,"SONY    ","CD-ROM CDU-8012 "
			,"3.1a",cdattach,"cd",SC_ONE_LU },
	{ T_READONLY,T_REMOV,"PIONEER ","CD-ROM DRM-600  "
			,"any",cdattach,"cd",SC_MORE_LUS },
#endif NCD
#if NBLL > 0
	{ T_PROCESSOR,T_FIXED,"AEG     ","READER          "
			,"V1.0",bllattach,"bll",SC_MORE_LUS },
#endif NBLL
#if NKIL > 0
	{ T_SCANNER,T_FIXED,"KODAK   ","IL Scanner 900  "
			,"any",kil_attach,"kil",SC_ONE_LU },
#endif NKIL

{0}
};
/***************************************************************\
* Declarations							*
\***************************************************************/
struct	predefined	*scsi_get_predef();
struct	scsidevs	*scsi_probedev();
struct	scsidevs	*selectdev();

/* controls debug level within the scsi subsystem */
/* see scsiconf.h for values			  */
int	scsi_debug	=	0x0;
int	scsibus		=	0x0; /* This is the Nth scsibus */

/***************************************************************\
* The routine called by the adapter boards to get all their	*
* devices configured in.					*
\***************************************************************/
scsi_attachdevs( unit, scsi_addr, scsi_switch)
int	unit,scsi_addr;
struct	scsi_switch	*scsi_switch;
{
	int	targ,lun;
	struct	scsidevs	*bestmatch = (struct scsidevs *)0;
	struct	predefined *predef;
	int maybe_more;

#ifdef	SCSI_DELAY
#if 	SCSI_DELAY > 2
	printf("waiting for scsi devices to settle\n");
#else	SCSI_DELAY > 2
#define	SCSI_DELAY 15
#endif	SCSI_DELAY > 2
#else
#define SCSI_DELAY 2
#endif	SCSI_DELAY
	spinwait(1000 * SCSI_DELAY);
	targ = 0;
	while(targ < 8)
	{
		maybe_more = 0; /* by default only check 1 lun */
		if (targ == scsi_addr) 
		{
			targ++;
			continue;
		}
		lun = 0;
		while(lun < 8)
		{
			predef = scsi_get_predef(scsibus
						,targ
						,lun
						,&maybe_more);
			bestmatch = scsi_probedev(unit
						,targ
						,lun 
						,scsi_switch
						,&maybe_more);
			if((bestmatch) && (predef)) /* both exist */
			{
				if(bestmatch->attach_rtn 
				    != predef->attach_rtn)
				{
				    printf("Clash in found/expected devices\n");
				    printf("will link in FOUND\n");
				}
				(*(bestmatch->attach_rtn))(unit,
						targ,
						lun,
						scsi_switch);
			}
			if((bestmatch) && (!predef)) /* just FOUND */
			{
				(*(bestmatch->attach_rtn))(unit,
						targ,
						lun,
						scsi_switch);
			}
			if((!bestmatch) && (predef)) /* just predef */
			{
				(*(predef->attach_rtn))(unit,
						targ,
						lun,
						scsi_switch);
			}
			if(!(maybe_more)) /* nothing suggests we'll find more */
			{
				break;	/* nothing here, skip to next targ */
			}
			/* otherwise something says we should look further*/
			lun++;
		}
		targ++;
	}
#if NGENSCSI > 0
	/***************************************************************\
	* If available hook up the generic scsi driver, letting it	*
	* know which target is US. (i.e. illegal or at least special)	*
	\***************************************************************/
	genscsi_attach(unit,scsi_addr,0,scsi_switch);
#endif
	scsibus++;	/* next time we are on the NEXT scsi bus */
}

/***********************************************\
* given a target and lu, check if there is a	*
* predefined device for that address		*
\***********************************************/
struct	predefined	*scsi_get_predef(unit,target,lu,maybe_more)
int	unit,target,lu,*maybe_more;
{
	int upto,numents;

	numents = (sizeof(pd)/sizeof(struct predefined)) - 1;
	
	for(upto = 0;upto < numents;upto++)
	{
		if(pd[upto].scsibus != unit)
			continue;
		if(pd[upto].dev != target)
			continue;
		if(pd[upto].lu != lu)
			continue;
		
		printf("  dev%d,lu%d: %s - PRECONFIGURED -\n"
			,target
			,lu
			,pd[upto].devname);
		*maybe_more = pd[upto].flags & SC_MORE_LUS;
		return(&(pd[upto]));
	}
	return((struct predefined *)0);
}

/***********************************************\
* given a target and lu, ask the device what	*
* it is, and find the correct driver table	*
* entry.					*
\***********************************************/
struct	scsidevs	*scsi_probedev(unit,target,lu,scsi_switch, maybe_more)

struct	scsi_switch *scsi_switch;
int	unit,target,lu;
int *maybe_more;
{
	struct	scsidevs	*bestmatch = (struct scsidevs *)0;
	char	*dtype=(char *)0,*desc;
	char	*qtype;
	static	struct scsi_inquiry_data	inqbuf; 
	int	len,qualifier,type,remov;
	char	manu[32];
	char	model[32];
	char	version[32];


	bzero(&inqbuf,sizeof(inqbuf));
	/***********************************************\
	* Ask the device what it is			*
	\***********************************************/
#ifdef	DEBUG
	if((target == 0) && (lu == 0))
		scsi_debug = 0xfff;
	else
		scsi_debug = 0;
#endif	DEBUG
	if(scsi_ready(	unit,
			target,
			lu,
			scsi_switch,
			SCSI_NOSLEEP | SCSI_NOMASK) != COMPLETE)
	{
		return(struct scsidevs *)0;
	}
	if(scsi_inquire(unit,
			target,
			lu,
			scsi_switch,
			&inqbuf,
			SCSI_NOSLEEP | SCSI_NOMASK) != COMPLETE)
	{
		return(struct scsidevs *)0;
	}

	/***********************************************\
	* note what BASIC type of device it is		*
	\***********************************************/
	if(scsi_debug & SHOWINQUIRY)
	{
		desc=(char *)&inqbuf;
		printf("inq: %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
		desc[0], desc[1], desc[2], desc[3],
		desc[4], desc[5], desc[6], desc[7],
		desc[8], desc[9], desc[10], desc[11],
		desc[12]);
	}

	type = inqbuf.device_type;
	qualifier = inqbuf.device_qualifier;
	remov = inqbuf.removable;

	/* Check for a non-existent unit.  If the device is returning
	 * this much, then we must set the flag that has
	 * the searcher keep looking on other luns.
	 */
	if (qualifier == 3 && type == T_NODEVICE)
	{
		*maybe_more = 1;
		return (struct scsidevs *)0;
	}

	/* Any device qualifier that has
	 * the top bit set (qualifier&4 != 0) is vendor specific and
	 * won't match in this switch.
	 */

	switch(qualifier)
	{
		case 0:
		qtype="";
		break;
		case 1:
		qtype=", Unit not Connected!";
		break;
		case 2:
		qtype=", Reserved Peripheral Qualifier!";
		break;
		case 3:
		qtype=", The Target can't support this Unit!";
		break;

		default:
		dtype="vendor specific";
		qtype="";
		*maybe_more = 1;
		break;
	}

	if (dtype == 0)
		switch(type)
		{
			case T_DIRECT:
				dtype="direct";
				break;
			case T_SEQUENTIAL:
				dtype="sequential";
				break;
			case T_PRINTER:
				dtype="printer";
				break;
			case T_PROCESSOR:
				dtype="processor";
				break;
			case T_READONLY:
				dtype="readonly";
				break;
			case T_WORM:
				dtype="worm";
				break;
			case T_SCANNER:
				dtype="scanner";
				break;
			case T_OPTICAL:
				dtype="optical";
				break;
			case T_CHANGER:
				dtype="changer";
				break;
			case T_COMM:
				dtype="communication";
				break;
			default:
				dtype="unknown";
				break;
		}

	/***********************************************\
	* Then if it's advanced enough, more detailed	*
	* information					*
	\***********************************************/
	if(inqbuf.ansii_version > 0) 
	{
		if ((len = inqbuf.additional_length 
				+ ( (char *)inqbuf.unused
				  - (char *)&inqbuf))
			> (sizeof(struct scsi_inquiry_data) - 1))
			len = sizeof(struct scsi_inquiry_data) - 1;
		desc=inqbuf.vendor;
		desc[len-(desc - (char *)&inqbuf)] = 0;
		strncpy(manu,inqbuf.vendor,8);manu[8]=0;
		strncpy(model,inqbuf.product,16);model[16]=0;
		strncpy(version,inqbuf.revision,4);version[4]=0;
	}
	else
	/***********************************************\
	* If not advanced enough, use default values	*
	\***********************************************/
	{
		desc="early protocol device";
		strncpy(manu,"unknown",8);
		strncpy(model,"unknown",16);
		strncpy(version,"????",4);
	}
	printf("  dev%d,lu%d: type %d:%d(%s%s),%s '%s%s%s' scsi%d\n"
		,target
		,lu
		,qualifier,type
		,dtype,qtype
		,remov?"removable":"fixed"
		,manu
		,model
		,version
		,inqbuf.ansii_version
	);
	/***********************************************\
	* Try make as good a match as possible with	*
	* available sub drivers	 			*
	\***********************************************/
	bestmatch = (selectdev(unit,target,lu,&scsi_switch,
		qualifier,type,remov,manu,model,version));
	if((bestmatch) && (bestmatch->flags & SC_MORE_LUS))
	{
		*maybe_more = 1;
	}
	return(bestmatch);
}

/***********************************************\
* Try make as good a match as possible with	*
* available sub drivers	 			*
\***********************************************/
struct	scsidevs	
*selectdev(unit,target,lu,dvr_switch,qualifier,type,remov,manu,model,rev)
int	unit,target,lu;
struct	scsi_switch *dvr_switch;
int	qualifier,type,remov;
char	*manu,*model,*rev;
{
	int	numents = (sizeof(knowndevs)/sizeof(struct scsidevs)) - 1;
	int	count = 0;
	int			bestmatches = 0;
	struct	scsidevs	*bestmatch = (struct scsidevs *)0;
	struct	scsidevs	*thisentry = knowndevs;

	type |= (qualifier << 5);

	thisentry--;
	while( count++ < numents)
	{
		thisentry++;
		if(type != thisentry->type)
		{
			continue;
		}
		if(bestmatches < 1)
		{
			bestmatches = 1;
			bestmatch = thisentry;
		}
		if(remov != thisentry->removable)
		{
			continue;
		}
		if(bestmatches < 2)
		{
			bestmatches = 2;
			bestmatch = thisentry;
		}
		if(thisentry->flags & SC_SHOWME)
			printf("\n%s-\n%s-",thisentry->manufacturer, manu);
		if(strcmp(thisentry->manufacturer, manu))
		{
			continue;
		}
		if(bestmatches < 3)
		{
			bestmatches = 3;
			bestmatch = thisentry;
		}
		if(thisentry->flags & SC_SHOWME)
			printf("\n%s-\n%s-",thisentry->model, model);
		if(strcmp(thisentry->model, model))
		{
			continue;
		}
		if(bestmatches < 4)
		{
			bestmatches = 4;
			bestmatch = thisentry;
		}
		if(thisentry->flags & SC_SHOWME)
			printf("\n%s-\n%s-",thisentry->version, rev);
		if(strcmp(thisentry->version, rev))
		{
			continue;
		}
		if(bestmatches < 5)
		{
			bestmatches = 5;
			bestmatch = thisentry;
			break;
		}
	}

	if (bestmatch == (struct scsidevs *)0)
		printf("	No explicit device driver match for \"%s %s\".\n",
		manu, model);

	return(bestmatch);
}

static	int recurse = 0;
/***********************************************\
* Do a scsi operation asking a device if it is	*
* ready. Use the scsi_cmd routine in the switch *
* table.					*
\***********************************************/
scsi_ready(unit,target,lu,scsi_switch, flags)
struct	scsi_switch *scsi_switch;
{
	struct	scsi_test_unit_ready scsi_cmd;
	struct	scsi_xfer scsi_xfer;
	volatile int rval;
	int	key;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	bzero(&scsi_xfer, sizeof(scsi_xfer));
	scsi_cmd.op_code = TEST_UNIT_READY;

	scsi_xfer.flags=flags | INUSE;
	scsi_xfer.adapter=unit;
	scsi_xfer.targ=target;
	scsi_xfer.lu=lu;
	scsi_xfer.cmd=(struct scsi_generic *)&scsi_cmd;
	scsi_xfer.retries=8;
	scsi_xfer.timeout=10000;
	scsi_xfer.cmdlen=sizeof(scsi_cmd);
	scsi_xfer.data=0;
	scsi_xfer.datalen=0;
	scsi_xfer.resid=0;
	scsi_xfer.when_done=0;
	scsi_xfer.done_arg=0;
retry:	scsi_xfer.error=0;
	/*******************************************************\
	* do not use interrupts					*
	\*******************************************************/
	rval = (*(scsi_switch->scsi_cmd))(&scsi_xfer);
	if (rval != COMPLETE)
	{
		if(scsi_debug)
		{
			printf("scsi error, rval = 0x%x\n",rval);
			printf("code from driver: 0x%x\n",scsi_xfer.error);
		}
		switch(scsi_xfer.error)
		{
		case	XS_SENSE:
		/*******************************************************\
		* Any sense value is illegal except UNIT ATTENTION	*
		* In which case we need to check again to get the	*
		* correct response.					*
		*( especially exabytes)					*
		\*******************************************************/
			if(scsi_xfer.sense.error_class == 7 )
			{
				key = scsi_xfer.sense.ext.extended.sense_key ;
				switch(key)
				{ 
				case	2:	/* not ready BUT PRESENT! */
						return(COMPLETE);
				case	6:
					spinwait(1000);
					if(scsi_xfer.retries--)
					{
						scsi_xfer.flags &= ~ITSDONE;
						goto retry;
					}
					return(COMPLETE);
				default:
					if(scsi_debug)
						printf("%d:%d,key=%x.",
						target,lu,key);
				}
			}
			return(HAD_ERROR);
		case	XS_BUSY:
			spinwait(1000);
			if(scsi_xfer.retries--)
			{
				scsi_xfer.flags &= ~ITSDONE;
				goto retry;
			}
			return(COMPLETE);	/* it's busy so it's there */
		case	XS_TIMEOUT:
		default:
			return(HAD_ERROR);
		}
	}
	return(COMPLETE);
}
/***********************************************\
* Do a scsi operation asking a device what it is*
* Use the scsi_cmd routine in the switch table.	*
\***********************************************/
scsi_inquire(unit,target,lu,scsi_switch,inqbuf, flags)
struct	scsi_switch *scsi_switch;
u_char	*inqbuf;
{
	struct	scsi_inquiry scsi_cmd;
	struct	scsi_xfer scsi_xfer;
	volatile int rval;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	bzero(&scsi_xfer, sizeof(scsi_xfer));
	scsi_cmd.op_code = INQUIRY;
	scsi_cmd.length = sizeof(struct scsi_inquiry_data);

	scsi_xfer.flags=flags | SCSI_DATA_IN | INUSE;
	scsi_xfer.adapter=unit;
	scsi_xfer.targ=target;
	scsi_xfer.lu=lu;
	scsi_xfer.retries=8;
	scsi_xfer.timeout=10000;
	scsi_xfer.cmd=(struct scsi_generic *)&scsi_cmd;
	scsi_xfer.cmdlen= sizeof(struct scsi_inquiry);
	scsi_xfer.data=inqbuf;
	scsi_xfer.datalen=sizeof(struct scsi_inquiry_data);
	scsi_xfer.resid=sizeof(struct scsi_inquiry_data);
	scsi_xfer.when_done=0;
	scsi_xfer.done_arg=0;
retry:	scsi_xfer.error=0;
	/*******************************************************\
	* do not use interrupts					*
	\*******************************************************/
	if ((*(scsi_switch->scsi_cmd))(&scsi_xfer) != COMPLETE)
	{
		if(scsi_debug) printf("inquiry had error(0x%x) ",scsi_xfer.error);
		switch(scsi_xfer.error)
		{
		case	XS_NOERROR:
			break;
		case	XS_SENSE:
		/*******************************************************\
		* Any sense value is illegal except UNIT ATTENTION	*
		* In which case we need to check again to get the	*
		* correct response.					*
		*( especially exabytes)					*
		\*******************************************************/
			if((scsi_xfer.sense.error_class == 7 )
			 && (scsi_xfer.sense.ext.extended.sense_key == 6))
			{ /* it's changed so it's there */
				spinwait(1000);
				{
					if(scsi_xfer.retries--)
					{
						scsi_xfer.flags &= ~ITSDONE;
						goto retry;
					}
				}
				return( COMPLETE);
			}
			return(HAD_ERROR);
		case	XS_BUSY:
			spinwait(1000);
			if(scsi_xfer.retries--)
			{
				scsi_xfer.flags &= ~ITSDONE;
				goto retry;
			}
		case	XS_TIMEOUT:
		default:
			return(HAD_ERROR);
		}
	}
	return(COMPLETE);
}




/***********************************************\
* Utility routines often used in SCSI stuff	*
\***********************************************/

/***********************************************\
* convert a physical address to 3 bytes, 	*
* MSB at the lowest address,			*
* LSB at the highest.				*
\***********************************************/

lto3b(val, bytes)
u_char *bytes;
{
	*bytes++ = (val&0xff0000)>>16;
	*bytes++ = (val&0xff00)>>8;
	*bytes = val&0xff;
}

/***********************************************\
* The reverse of lto3b				*
\***********************************************/
_3btol(bytes)
u_char *bytes;
{
	int rc;
	rc = (*bytes++ << 16);
	rc += (*bytes++ << 8);
	rc += *bytes;
	return(rc);
}

