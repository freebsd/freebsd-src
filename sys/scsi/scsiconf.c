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
 *      $Id: scsiconf.c,v 1.8 1993/12/19 00:54:54 wollman Exp $
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>

#include <sys/malloc.h>
#include "st.h"
#include "sd.h"
#include "ch.h"
#include "cd.h"
#include "uk.h"
#include "su.h"
#ifndef	NSCBUS
#define	NSCBUS	8
#endif	/* NSCBUS */

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#ifdef TFS
#include "bll.h"
#include "cals.h"
#include "kil.h"
#include "scan.h"
#else /* TFS */
#define	NBLL 0
#define	NCALS 0
#define	NKIL 0
#define	NSCAN 0
#endif /* TFS */

#if NSD > 0
extern  sdattach();
#endif	/* NSD */
#if NST > 0
extern  stattach();
#endif	/* NST */
#if NCH > 0
extern  chattach();
#endif	/* NCH */
#if NCD > 0
extern  cdattach();
#endif	/* NCD */
#if NBLL > 0
extern  bllattach();
#endif	/* NBLL */
#if NCALS > 0
extern  calsattach();
#endif	/* NCALS */
#if NKIL > 0
extern  kil_attach();
#endif	/* NKIL */
#if NUK > 0
extern  ukattach();
#endif	/* NUK */

/*
 * One of these is allocated and filled in for each scsi bus.
 * it holds pointers to allow the scsi bus to get to the driver
 * That is running each LUN on the bus
 * it also has a template entry which is the prototype struct
 * supplied by the adapter driver, this is used to initialise
 * the others, before they have the rest of the fields filled in
 */
struct scsibus_data      *scbus_data[NSCBUS];

/*
 * The structure of pre-configured devices that might be turned
 * off and therefore may not show up
 */
struct predefined {
	u_char  scsibus;
	u_char  dev;
	u_char  lu;
	        errval(*attach_rtn) ();
	char   *devname;
	char    flags;
} pd[] =

{
#ifdef EXAMPLE_PREDEFINE
#if NSD > 0
	{
		0, 0, 0, sdattach, "sd", 0
	},			/* define a disk at scsibus=0 dev=0 lu=0 */
#endif	/* NSD */
#endif	/* EXAMPLE_PREDEFINE */
	{
		0, 9, 9
	} /*illegal dummy end entry */
};

/*
 * The structure of known drivers for autoconfiguration
 */
struct scsidevs {
	u_int32 type;
	boolean removable;
	char   *manufacturer;
	char   *model;
	char   *version;
	        errval(*attach_rtn) ();
	char   *devname;
	char    flags;		/* 1 show my comparisons during boot(debug) */
};

#define SC_SHOWME	0x01
#define	SC_ONE_LU	0x00
#define	SC_MORE_LUS	0x02
#if	NUK > 0

static struct scsidevs unknowndev = {
	-1, 0, "standard", "any"
	    ,"any", ukattach, "uk", SC_MORE_LUS
};
#endif 	/*NUK*/
static struct scsidevs knowndevs[] =
{
#if NSD > 0
	{
		T_DIRECT, T_FIXED, "standard", "any"
		    ,"any", sdattach, "sd", SC_ONE_LU
	},
	{
		T_DIRECT, T_FIXED, "MAXTOR  ", "XT-4170S        "
		    ,"B5A ", sdattach, "mx1", SC_ONE_LU
	},
#endif	/* NSD */
#if NST > 0
	{
		T_SEQUENTIAL, T_REMOV, "standard", "any"
		    ,"any", stattach, "st", SC_ONE_LU
	},
#endif	/* NST */
#if NCALS > 0
	{
		T_PROCESSOR, T_FIXED, "standard", "any"
		    ,"any", calsattach, "cals", SC_MORE_LUS
	},
#endif	/* NCALS */
#if NCH > 0
	{
		T_CHANGER, T_REMOV, "standard", "any"
		    ,"any", chattach, "ch", SC_ONE_LU
	},
#endif	/* NCH */
#if NCD > 0
#ifndef UKTEST	/* make cdroms unrecognised to test the uk driver */
	{
		T_READONLY, T_REMOV, "SONY    ", "CD-ROM CDU-8012 "
		    ,"3.1a", cdattach, "cd", SC_ONE_LU
	},
	{
		T_READONLY, T_REMOV, "PIONEER ", "CD-ROM DRM-600  "
		    ,"any", cdattach, "cd", SC_MORE_LUS
	},
#endif
#endif	/* NCD */
#if NBLL > 0
	{
		T_PROCESSOR, T_FIXED, "AEG     ", "READER          "
		    ,"V1.0", bllattach, "bll", SC_MORE_LUS
	},
#endif	/* NBLL */
#if NKIL > 0
	{
		T_SCANNER, T_FIXED, "KODAK   ", "IL Scanner 900  "
		    ,"any", kil_attach, "kil", SC_ONE_LU
	},
#endif	/* NKIL */

	{
		0
	}
};

/*
 * Declarations
 */
struct predefined *scsi_get_predef();
struct scsidevs *scsi_probedev();
struct scsidevs *selectdev();
errval scsi_probe_bus __P((int bus, int targ, int lun));

struct scsi_device probe_switch =
{
    NULL,
    NULL,
    NULL,
    NULL,
    "probe",
    0,
    { 0, 0 }
};

/*
 * controls debug level within the scsi subsystem -
 * see scsiconf.h for values
 */
int32 scsibus = 0x0;		/* This is the Nth scsibus we've seen */

/*
 * The routine called by the adapter boards to get all their
 * devices configured in.
 */
void
scsi_attachdevs(sc_link_proto)
	struct scsi_link *sc_link_proto;
{

	if(scsibus >= NSCBUS) {
		printf("too many scsi busses, reconfigure the kernel\n");
		return;
	}
	sc_link_proto->scsibus = scsibus;
	scbus_data[scsibus] = malloc(sizeof(struct scsibus_data), M_TEMP, M_NOWAIT);
	if(!scbus_data[scsibus]) {
		panic("scsi_attachdevs: malloc\n");
	}
	bzero(scbus_data[scsibus], sizeof(struct scsibus_data));
	scbus_data[scsibus]->adapter_link = sc_link_proto;
#if defined(SCSI_DELAY) && SCSI_DELAY > 2
	printf("%s%d waiting for scsi devices to settle\n",
	    sc_link_proto->adapter->name, sc_link_proto->adapter_unit);
#else	/* SCSI_DELAY > 2 */
#undef	SCSI_DELAY
#define SCSI_DELAY 2
#endif	/* SCSI_DELAY */
	DELAY(1000000 * SCSI_DELAY);
	scsibus++;
	scsi_probe_bus(scsibus - 1,-1,-1);
}

/*
 * Probe the requested scsi bus. It must be already set up.
 * -1 requests all set up scsi busses.
 * targ and lun optionally narrow the search if not -1
 */
errval
scsi_probe_busses(int bus, int targ, int lun)
{
	if (bus == -1) {
		for(bus = 0; bus < scsibus; bus++) {
			scsi_probe_bus(bus, targ, lun);
		}
		return 0;
	} else {
		return scsi_probe_bus(bus, targ, lun);
	}
}

/*
 * Probe the requested scsi bus. It must be already set up.
 * targ and lun optionally narrow the search if not -1
 */
errval
scsi_probe_bus(int bus, int targ, int lun)
{
	struct scsibus_data *scsi ;
	int	maxtarg,mintarg,maxlun,minlun;
	struct scsi_link *sc_link_proto;
	u_int8  scsi_addr ;
	struct scsidevs *bestmatch = NULL;
	struct predefined *predef = NULL;
	struct scsi_link *sc_link = NULL;
	boolean maybe_more;

	if ((bus < 0 ) || ( bus >= scsibus)) {
		return ENXIO;
	}
	scsi = scbus_data[bus];
	if(!scsi) return ENXIO;
	sc_link_proto = scsi->adapter_link;
	scsi_addr = sc_link_proto->adapter_targ;
	if(targ == -1){
		maxtarg = 7;
		mintarg = 0;
	} else {
		if((targ < 0 ) || (targ > 7)) return EINVAL;
		maxtarg = mintarg = targ;
	}

	if(lun == -1){
		maxlun = 7;
		minlun = 0;
	} else {
		if((lun < 0 ) || (lun > 7)) return EINVAL;
		maxlun = minlun = lun;
	}


	for ( targ = mintarg;targ <= maxtarg; targ++) {
		maybe_more = 0;	/* by default only check 1 lun */
		if (targ == scsi_addr) {
			continue;
		}
		for ( lun = minlun; lun <= maxlun ;lun++) {
			/*
			 * The spot appears to already have something
			 * linked in, skip past it. Must be doing a 'reprobe'
			 */
			if(scsi->sc_link[targ][lun])
			{/* don't do this one, but check other luns */
				maybe_more = 1;
				continue;
			}
			/*
			 * If we presently don't have a link block
			 * then allocate one to use while probing
			 */
			if (!sc_link) {
				sc_link = malloc(sizeof(*sc_link), M_TEMP, M_NOWAIT);
				*sc_link = *sc_link_proto;	/* struct copy */
				sc_link->opennings = 1;
				sc_link->device = &probe_switch;
			}
			sc_link->target = targ;
			sc_link->lun = lun;
			predef = scsi_get_predef(sc_link, &maybe_more);
			bestmatch = scsi_probedev(sc_link, &maybe_more);
			if ((bestmatch) && (predef)) {	/* both exist */
				if (bestmatch->attach_rtn
				    != predef->attach_rtn) {
					printf("Clash in found/expected devices\n");
#if NUK > 0
					if(bestmatch == &unknowndev) {
						printf("will link in PREDEFINED\n");
						(*(predef->attach_rtn)) (sc_link);
					} else 
#endif	/*NUK*/
					{
						printf("will link in FOUND\n");
						(*(bestmatch->attach_rtn)) (sc_link);
					}
				} else {
					(*(bestmatch->attach_rtn)) (sc_link);
				}
			}
			if ((bestmatch) && (!predef)) {		/* just FOUND */
				(*(bestmatch->attach_rtn)) (sc_link);
			}
			if ((!bestmatch) && (predef)) {		/* just predef */
				(*(predef->attach_rtn)) (sc_link);
			}
			if ((bestmatch) || (predef)) {	/* one exists */
				scsi->sc_link[targ][lun] = sc_link;
				sc_link = NULL;		/* it's been used */
			}
			if (!(maybe_more)) {	/* nothing suggests we'll find more */
				break;	/* nothing here, skip to next targ */
			}
			/* otherwise something says we should look further */
		}
	}
	if (sc_link) {
		free(sc_link, M_TEMP);
	}
	return 0;
}

/*
 * given a target and lu, check if there is a predefined device for
 * that address
 */
struct predefined *
scsi_get_predef(sc_link, maybe_more)
	struct scsi_link *sc_link;
	boolean *maybe_more;
{
	u_int8  unit = sc_link->scsibus;
	u_int8  target = sc_link->target;
	u_int8  lu = sc_link->lun;
	struct scsi_adapter *scsi_adapter = sc_link->adapter;
	u_int32 upto, numents;

	numents = (sizeof(pd) / sizeof(struct predefined)) - 1;

	for (upto = 0; upto < numents; upto++) {
		if (pd[upto].scsibus != unit)
			continue;
		if (pd[upto].dev != target)
			continue;
		if (pd[upto].lu != lu)
			continue;

		printf("%s%d targ %d lun %d: <%s> - PRECONFIGURED -\n"
		    ,scsi_adapter->name
		    ,unit
		    ,target
		    ,lu
		    ,pd[upto].devname);
		*maybe_more = pd[upto].flags & SC_MORE_LUS;
		return (&(pd[upto]));
	}
	return ((struct predefined *) 0);
}

/*
 * given a target and lu, ask the device what
 * it is, and find the correct driver table
 * entry.
 */
struct scsidevs *
scsi_probedev(sc_link, maybe_more)
	boolean *maybe_more;
	struct scsi_link *sc_link;
{
	u_int8  unit = sc_link->adapter_unit;
	u_int8  target = sc_link->target;
	u_int8  lu = sc_link->lun;
	struct scsi_adapter *scsi_adapter = sc_link->adapter;
	struct scsidevs *bestmatch = (struct scsidevs *) 0;
	char   *dtype = (char *) 0, *desc;
	char   *qtype;
	static struct scsi_inquiry_data inqbuf;
	u_int32 len, qualifier, type;
	boolean remov;
	char    manu[32];
	char    model[32];
	char    version[32];

	bzero(&inqbuf, sizeof(inqbuf));
	/*
	 * Ask the device what it is
	 */
#ifdef	SCSIDEBUG
	if ((target == DEBUGTARG) && (lu == DEBUGLUN))
		sc_link->flags |= (DEBUGLEVEL);
	else
		sc_link->flags &= ~(SDEV_DB1 | SDEV_DB2 | SDEV_DB3 | SDEV_DB4);
#endif	/* SCSIDEBUG */
	/* catch unit attn */
	scsi_test_unit_ready(sc_link, SCSI_NOSLEEP | SCSI_NOMASK | SCSI_SILENT);
#ifdef	DOUBTFULL
	switch (scsi_test_unit_ready(sc_link, SCSI_NOSLEEP | SCSI_NOMASK | SCSI_SILENT)) {
	case 0:		/* said it WAS ready */
	case EBUSY:		/* replied 'NOT READY' but WAS present, continue */
	case ENXIO:
		break;
	case EIO:		/* device timed out */
	case EINVAL:		/* Lun not supported */
	default:
		return (struct scsidevs *) 0;

	}
#endif	/*DOUBTFULL*/
#ifdef	SCSI_2_DEF
	/* some devices need to be told to go to SCSI2 */
	/* However some just explode if you tell them this.. leave it out */
	scsi_change_def(sc_link, SCSI_NOSLEEP | SCSI_NOMASK | SCSI_SILENT);
#endif /*SCSI_2_DEF */

	/* Now go ask the device all about itself */
	if (scsi_inquire(sc_link, &inqbuf, SCSI_NOSLEEP | SCSI_NOMASK) != 0) {
		return (struct scsidevs *) 0;
	}

	/*
	 * note what BASIC type of device it is
	 */
	type = inqbuf.device & SID_TYPE;
	qualifier = inqbuf.device & SID_QUAL;
	remov = inqbuf.dev_qual2 & SID_REMOVABLE;

	/*
	 * Any device qualifier that has the top bit set (qualifier&4 != 0)
	 * is vendor specific and won't match in this switch.
	 */

	switch ((int)qualifier) {
	case SID_QUAL_LU_OK:
		qtype = "";
		break;

	case SID_QUAL_LU_OFFLINE:
		qtype = ", Unit not Connected!";
		break;

	case SID_QUAL_RSVD:
		qtype = ", Reserved Peripheral Qualifier!";
		*maybe_more = 1;
		return (struct scsidevs *) 0;
		break;

	case SID_QUAL_BAD_LU:
		/*
		 * Check for a non-existent unit.  If the device is returning
		 * this much, then we must set the flag that has
		 * the searchers keep looking on other luns.
		 */
		qtype = ", The Target can't support this Unit!";
		*maybe_more = 1;
		return (struct scsidevs *) 0;

	default:
		dtype = "vendor specific";
		qtype = "";
		*maybe_more = 1;
		break;
	}
	if (dtype == 0) {
		switch ((int)type) {
		case T_DIRECT:
			dtype = "direct";
			break;
		case T_SEQUENTIAL:
			dtype = "sequential";
			break;
		case T_PRINTER:
			dtype = "printer";
			break;
		case T_PROCESSOR:
			dtype = "processor";
			break;
		case T_READONLY:
			dtype = "readonly";
			break;
		case T_WORM:
			dtype = "worm";
			break;
		case T_SCANNER:
			dtype = "scanner";
			break;
		case T_OPTICAL:
			dtype = "optical";
			break;
		case T_CHANGER:
			dtype = "changer";
			break;
		case T_COMM:
			dtype = "communication";
			break;
		case T_NODEVICE:
			*maybe_more = 1;
			return (struct scsidevs *) 0;
		default:
			dtype = "unknown";
			break;
		}
	}
	/*
	 * Then if it's advanced enough, more detailed
	 * information
	 */
	if ((inqbuf.version & SID_ANSII) > 0) {
		if ((len = inqbuf.additional_length
			+ ((char *) inqbuf.unused
			    - (char *) &inqbuf))
		    > (sizeof(struct scsi_inquiry_data) - 1))
			        len = sizeof(struct scsi_inquiry_data) - 1;
		desc = inqbuf.vendor;
		desc[len - (desc - (char *) &inqbuf)] = 0;
		strncpy(manu, inqbuf.vendor, 8);
		manu[8] = 0;
		strncpy(model, inqbuf.product, 16);
		model[16] = 0;
		strncpy(version, inqbuf.revision, 4);
		version[4] = 0;
	} else
		/*
		 * If not advanced enough, use default values
		 */
	{
		desc = "early protocol device";
		strncpy(manu, "unknown", 8);
		strncpy(model, "unknown", 16);
		strncpy(version, "????", 4);
	}
	printf("%s%d targ %d lun %d: type %d(%s) %s SCSI%d\n"
	    ,scsi_adapter->name
	    ,unit
	    ,target
	    ,lu
	    ,type
	    ,dtype
	    ,remov ? "removable" : "fixed"
	    ,inqbuf.version & SID_ANSII
	    );
	printf("%s%d targ %d lun %d: <%s%s%s>\n"
	    ,scsi_adapter->name
	    ,unit
	    ,target
	    ,lu
	    ,manu
	    ,model
	    ,version
	    );
	if (qtype[0]) {
		printf("%s%d targ %d lun %d: qualifier %d(%s)\n"
		    ,scsi_adapter->name
		    ,unit
		    ,target
		    ,lu
		    ,qualifier
		    ,qtype
		    );
	}
	/*
	 * Try make as good a match as possible with
	 * available sub drivers       
	 */
	bestmatch = (selectdev(
		qualifier, type, remov ? T_REMOV : T_FIXED, manu, model, version));
	if ((bestmatch) && (bestmatch->flags & SC_MORE_LUS)) {
		*maybe_more = 1;
	}
	return (bestmatch);
}
/*
 * Try make as good a match as possible with
 * available sub drivers       
 */
struct scsidevs *
selectdev(qualifier, type, remov, manu, model, rev)
	u_int32 qualifier, type;
	boolean remov;
	char   *manu, *model, *rev;
{
	u_int32 numents = (sizeof(knowndevs) / sizeof(struct scsidevs)) - 1;
	u_int32 count = 0;
	u_int32 bestmatches = 0;
	struct scsidevs *bestmatch = (struct scsidevs *) 0;
	struct scsidevs *thisentry = knowndevs;

	type |= qualifier;	/* why? */

	thisentry--;
	while (count++ < numents) {
		thisentry++;
		if (type != thisentry->type) {
			continue;
		}
		if (bestmatches < 1) {
			bestmatches = 1;
			bestmatch = thisentry;
		}
		if (remov != thisentry->removable) {
			continue;
		}
		if (bestmatches < 2) {
			bestmatches = 2;
			bestmatch = thisentry;
		}
		if (thisentry->flags & SC_SHOWME)
			printf("\n%s-\n%s-", thisentry->manufacturer, manu);
		if (strcmp(thisentry->manufacturer, manu)) {
			continue;
		}
		if (bestmatches < 3) {
			bestmatches = 3;
			bestmatch = thisentry;
		}
		if (thisentry->flags & SC_SHOWME)
			printf("\n%s-\n%s-", thisentry->model, model);
		if (strcmp(thisentry->model, model)) {
			continue;
		}
		if (bestmatches < 4) {
			bestmatches = 4;
			bestmatch = thisentry;
		}
		if (thisentry->flags & SC_SHOWME)
			printf("\n%s-\n%s-", thisentry->version, rev);
		if (strcmp(thisentry->version, rev)) {
			continue;
		}
		if (bestmatches < 5) {
			bestmatches = 5;
			bestmatch = thisentry;
			break;
		}
	}
	if (bestmatch == (struct scsidevs *) 0) {
#if NUK > 0
		bestmatch = &unknowndev;
#else
		printf("No explicit device driver match.\n");
#endif
	}
	return (bestmatch);
}
