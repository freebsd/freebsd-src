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
 * New configuration setup: dufault@hda.com
 *
 *      $Id: scsiconf.c,v 1.83 1997/04/01 19:28:03 joerg Exp $
 */

#include "opt_scsi.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/stat.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#ifdef PC98
#include <sys/device.h>
#endif

#include <machine/clock.h>

#include "scbus.h"

#include "sd.h"
#include "st.h"
#include "cd.h"
#include "ch.h"
#include "od.h"
#include "worm.h"

#include "su.h"
#include "sctarg.h"

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_debug.h>
#include <scsi/scsi_driver.h>

static struct extend_array *extend_new __P((void));
static void extend_release __P((struct extend_array *ea, int index));
static void *extend_set __P((struct extend_array *ea, int index, void *value));

/*
 * XXX SCSI_DEVICE_ENTRIES() generates extern switches but it should
 * generate static switches except for this.  Separate macros are
 * probably required for the extern and static parts.
 */
extern struct scsi_device uk_switch;

/***********************************************************************
 * Extensible arrays: Use a realloc like implementation to permit
 * the arrays to be extend.  These are set up to be moved out
 * of this file if needed elsewhere.
 */
struct extend_array
{
	int nelem;
	void **ps;
};

static void make_readable __P((char *to, char *from, size_t n));
static int match __P((char *pattern, char *name));
static int scsi_bus_conf __P((struct scsi_link *sc_link_proto));

static void *
extend_alloc(size_t s)
{
	void *p = malloc(s, M_DEVBUF, M_NOWAIT);
	if (!p)
		panic("extend_alloc: malloc failed.");
	return p;
}

static void
extend_free(void *p) { free(p, M_DEVBUF); }

/* EXTEND_CHUNK: Number of extend slots to allocate whenever we need a new
 * one.
 */
#ifndef EXTEND_CHUNK
	#define EXTEND_CHUNK 8
#endif

static struct extend_array *
extend_new(void)
{
	struct extend_array *p = extend_alloc(sizeof(*p));
	if (p) {
		p->nelem = 0;
		p->ps = 0;
	}

	return p;
}

static void *
extend_set(struct extend_array *ea, int index, void *value)
{
	if (index >= ea->nelem) {
		void **space;
		space = extend_alloc(sizeof(void *) * (index + EXTEND_CHUNK));
		bzero(space, sizeof(void *) * (index + EXTEND_CHUNK));

		/* Make sure we have something to copy before we copy it */
		if (ea->nelem) {
			bcopy(ea->ps, space, sizeof(void *) * ea->nelem);
			extend_free(ea->ps);
		}

		ea->ps = space;
		ea->nelem = index + EXTEND_CHUNK;
	}
	if (ea->ps[index]) {
		printf("extend_set: entry %d already has storage.\n", index);
		return 0;
	}
	else
		ea->ps[index] = value;

	return value;
}

void *
extend_get(struct extend_array *ea, int index)
{
	if (ea == NULL || index >= ea->nelem || index < 0)
		return NULL;
	return ea->ps[index];
}

static void
extend_release(struct extend_array *ea, int index)
{
	void *p = extend_get(ea, index);
	if (p) {
		ea->ps[index] = 0;
	}
}

/***********************************************************************
 * This extend_array holds an array of "scsibus_data" pointers.
 * One of these is allocated and filled in for each scsi bus.
 * it holds pointers to allow the scsi bus to get to the driver
 * that is running each LUN on the bus
 * it also has a template entry which is the prototype struct
 * supplied by the adapter driver, this is used to initialise
 * the others, before they have the rest of the fields filled in
 */

static struct extend_array *scbusses;

/*
 * The structure of known drivers for autoconfiguration
 */
struct scsidevs {
	u_int32_t type;
	u_int32_t driver;		/* normally the same as type */
	boolean removable;
	char   *manufacturer;
	char   *model;
	char   *version;
	char   *devname;
	char    flags;		/* 1 show my comparisons during boot(debug) */
	u_int16_t	quirks;
	void   *devmodes;
};

#define SC_SHOWME	0x01
#define	SC_ONE_LU	0x00
#define	SC_MORE_LUS	0x02

static struct scsidevs unknowndev =
	{
		T_UNKNOWN, T_UNKNOWN, 0, "*", "*", "*",
		"uk", SC_MORE_LUS
	};
static st_modes mode_tandberg3600 =
	{
	    {0, 0, 0},					/* minor 0,1,2,3 */
	    {0, ST_Q_FORCE_VAR_MODE, QIC_525},		/* minor 4,5,6,7 */
	    {0, 0, QIC_150},				/* minor 8,9,10,11 */
	    {0, 0, QIC_120}				/* minor 12,13,14,15 */
	};
static st_modes mode_tandberg4200 =
	{
	    {0, 0, 0},					/* minor 0,1,2,3 */
	    {0, ST_Q_FORCE_VAR_MODE, 0},		/* minor 4,5,6,7 */
	    {0, 0, QIC_150},				/* minor 8,9,10,11 */
	    {0, 0, QIC_120}				/* minor 12,13,14,15 */
	};
static st_modes mode_archive2525 =
	{
	    {0, ST_Q_SNS_HLP, 0},			/* minor 0,1,2,3 */
	    {0, ST_Q_SNS_HLP, QIC_525},			/* minor 4,5,6,7 */
	    {0, 0, QIC_150},				/* minor 8,9,10,11 */
	    {0, 0, QIC_120}				/* minor 12,13,14,15 */
	};
static st_modes mode_archive150 =
	{
	    {0, 0, 0},					/* minor 0,1,2,3 */
	    {0, 0, QIC_150},				/* minor 4,5,6,7 */
	    {0, 0, QIC_120},				/* minor 8,9,10,11 */
	    {0, 0, QIC_24}				/* minor 12,13,14,15 */
	};
static st_modes mode_wangtek5525 =
	{
	    {0, 0, 0},					/* minor 0,1,2,3 */
	    {0, ST_Q_BLKSIZ, QIC_525},			/* minor 4,5,6,7 */
	    {0, 0, QIC_150},				/* minor 8,9,10,11 */
	    {0, 0, QIC_120}				/* minor 12,13,14,15 */
	};
static st_modes mode_wangdat1300 =
	{
	    {0, 0, 0},					/* minor 0,1,2,3 */
	    {512, ST_Q_FORCE_FIXED_MODE, DDS},		/* minor 4,5,6,7 */
	    {1024, ST_Q_FORCE_FIXED_MODE, DDS},		/* minor 8,9,10,11 */
	    {0, ST_Q_FORCE_VAR_MODE, DDS}		/* minor 12,13,14,15 */
	};
static st_modes mode_unktape =
	{
	    {0, 0, 0},					/* minor 0,1,2,3 */
	    {512, ST_Q_FORCE_FIXED_MODE, QIC_24},	/* minor 4,5,6,7 */
	    {0, ST_Q_FORCE_VAR_MODE, HALFINCH_1600},	/* minor 8,9,10,11 */
	    {0, ST_Q_FORCE_VAR_MODE, HALFINCH_6250}	/* minor 12,13,14,15 */
	};

/***********************************************************************
 * A list of known devices and their "quirks".  Matching is based
 * first on device type, then on the manufacturer, model, and revision
 * strings returned by the device.  The returned strings are fixed lengths
 * of 8, 16 and 4 bytes respectively.  In the matching pattern, a
 * question mark (?) matches any single character and a trailing 
 * asterisk (*) matches remaining characters.  For patterns shorter 
 * than their respective fields, trailing spaces are implied.
 */

static struct scsidevs knowndevs[] =
{
#if NOD > 0
	{
		T_OPTICAL, T_OPTICAL, T_REMOV, "MATSHITA", "PD-1 LF-100*", "*",
		"od", SC_MORE_LUS
	},
	{
		T_DIRECT, T_OPTICAL, T_REMOV, "SONY", "SMO-*", "*",
		"od", SC_MORE_LUS
	},
	{
		T_DIRECT, T_OPTICAL, T_REMOV, "MOST", "RMD-5200-S", "*",
		"od", SC_ONE_LU
	},
	{
		T_DIRECT, T_OPTICAL, T_REMOV, "RICOH", "RO-*", "*",
		"od", SC_ONE_LU
	},
#endif	/* NOD */
#if NSD > 0
	{
		T_DIRECT, T_DIRECT, T_FIXED, "EMULEX", "MD21*" , "*",
		"sd", SC_MORE_LUS
	},
#endif	/* NSD */
#if NST > 0
	{
		T_SEQUENTIAL, T_SEQUENTIAL, T_REMOV, "TANDBERG", " TDC 3600", "*",
		"st", SC_ONE_LU, ST_Q_NEEDS_PAGE_0, mode_tandberg3600
	},
	{
		T_SEQUENTIAL, T_SEQUENTIAL, T_REMOV, "TANDBERG", " TDC 42*", "*",
		"st", SC_ONE_LU, ST_Q_SNS_HLP|ST_Q_NO_1024, mode_tandberg4200
	},
	{
		T_SEQUENTIAL, T_SEQUENTIAL, T_REMOV, "ARCHIVE", "VIPER 2525*", "-005",
		"st", SC_ONE_LU, 0, mode_archive2525
	},
	{
		T_SEQUENTIAL, T_SEQUENTIAL, T_REMOV, "ARCHIVE", "VIPER 150 *", "*",
		"st", SC_ONE_LU, ST_Q_NEEDS_PAGE_0, mode_archive150
	},
	{
		T_SEQUENTIAL, T_SEQUENTIAL, T_REMOV, "WANGTEK", "5525ES*", "*",
		"st", SC_ONE_LU, 0, mode_wangtek5525
	},
	{
		T_SEQUENTIAL, T_SEQUENTIAL, T_REMOV, "WangDAT", "Model 1300", "*",
		"st", SC_ONE_LU, 0, mode_wangdat1300
	},
	{
		T_SEQUENTIAL, T_SEQUENTIAL, T_REMOV, "DEC", "DLT2700", "*",
		"st", SC_MORE_LUS, 0
	},
	{
		T_SEQUENTIAL, T_SEQUENTIAL, T_REMOV, "Quantum", "DLT*", "*",
		"st", SC_MORE_LUS, 0
	},
	{
		T_SEQUENTIAL, T_SEQUENTIAL, T_REMOV, "HP", "C1553A", "*",
		"st", SC_MORE_LUS, 0
	},
	{
		T_SEQUENTIAL, T_SEQUENTIAL, T_REMOV, "ARCHIVE", "Python 28849-*", "*",
		"st", SC_MORE_LUS, 0
	},
#endif	/* NST */
#if NCH > 0
	/*
	 * The <ARCHIVE, Python 28849-XXX, 4.98> is a SCSI changer device
	 * with an Archive Python DAT drive built-in.  The tape appears
	 * at LUN 0 and the changer at LUN 1.
	 * This entry should not be needed at all.
	 */
	{
		T_CHANGER, T_CHANGER, T_REMOV, "ARCHIVE", "Python 28849-*", "*",
		"ch", SC_MORE_LUS
	},
#endif /* NCH */
#if NCD > 0
#ifndef UKTEST	/* make cdroms unrecognised to test the uk driver */
	/*
	* CDU-8003A aka Apple CDROM-300.
	*/
	{
		T_READONLY, T_READONLY, T_REMOV, "SONY",    "CD-ROM CDU-8003A", "1.9a",
		"cd", SC_ONE_LU
	},
	{
		T_READONLY, T_READONLY, T_REMOV, "SONY",    "CD-ROM CDU-8012", "3.1a",
		"cd", SC_ONE_LU
	},
	{
		T_READONLY, T_READONLY, T_REMOV, "PIONEER", "CD-ROM DRM-6??*" ,"*",
		"cd", SC_MORE_LUS, CD_Q_NO_TOUCH
	},
	{
		T_READONLY, T_READONLY, T_REMOV, "NRC", "MBR-7*" ,"*",
		"cd", SC_MORE_LUS
	},
	{
		T_READONLY, T_READONLY, T_REMOV, "CHINON",  "CD-ROM CDS-535","*",
		"cd", SC_ONE_LU, CD_Q_BCD_TRACKS
	},
	/*
	 * Note: My drive with v1.0 firmware "forgets" to generate scsi parity
	 * when answering probes.. :-( EVIL!!  You need to disable scsi parity
	 * checking in order to find out that it answers to all 7 LUNS. :-(
	 * -Peter
	 */
	{
		T_READONLY, T_READONLY, T_REMOV, "NEC",  "CD-ROM DRIVE:55","*",
		"cd", SC_ONE_LU
	},
	/*
	 * Same with the OEM version of this drive (1.0 firmware).
	 * -Paul
	 */
	{
		T_READONLY, T_READONLY, T_REMOV, "NEC",  "CD-ROM DRIVE:210","*",
		"cd", SC_ONE_LU
	},
	/*
	 * Doobe-doo-be doooo
	 * -Mary
	 */
	{
		T_READONLY, T_READONLY, T_REMOV, "NAKAMICH", "MJ-4*" ,"*",
		"cd", SC_MORE_LUS
	},
#endif /* !UKTEST */
#endif	/* NCD */
#if NWORM > 0
	{
		T_READONLY, T_WORM, T_REMOV, "HP", "C4324/C4325", "*",
		"worm", SC_ONE_LU
	},
	{
		T_READONLY, T_WORM, T_REMOV, "HP", "CD-Writer 6020", "*",
		"worm", SC_ONE_LU
	},
	{
		/* That's the Philips drive, in case anybody wonders... */
		T_READONLY, T_WORM, T_REMOV, "IMS", "CDD2000*", "*",
		"worm", SC_ONE_LU
	},
	{
		/* Here's another Philips drive... */
		T_READONLY, T_WORM, T_REMOV, "PHILIPS", "CDD2*", "*",
		"worm", SC_ONE_LU
	},
	/*
	 * The Plasmon's are dual-faced: they appear as T_WORM if the
	 * drive is empty, or a CD-R medium is in the drive, and they
	 * announce theirselves as T_READONLY if a CD-ROM (or fixated
	 * CD-R) is there.  This record catches the latter case, while
	 * the former one falls under the terms of the generic T_WORM
	 * below.
	 */
	{
		T_READONLY, T_WORM, T_REMOV, "PLASMON", "RF41*", "*",
		"worm", SC_ONE_LU
	},
#endif /* NWORM */

	/*
	 * Wildcard entries.  Keep them down here below all device
	 * specific entries, so the above ones can override the type
	 * driver if necessary.
	 */
#if NOD > 0
	{
		T_OPTICAL, T_OPTICAL, T_REMOV, "*", "*", "*",
		"od", SC_ONE_LU
	},
#endif /* NOD */
#if NSD > 0
	{
		T_DIRECT, T_DIRECT, T_FIXED, "*", "*", "*",
		"sd", SC_ONE_LU
	},
#endif /* NSD */
#if NST > 0
	{
		T_SEQUENTIAL, T_SEQUENTIAL, T_REMOV, "*", "*", "*",
		"st", SC_ONE_LU, 0, mode_unktape
	},
#endif /* NST */
#if NCH > 0
	/*
	 * Due to the way media changers are working, they are most
	 * likely always on a different LUN than the transfer element
	 * device.  Thus, it should be safe to always probe all LUNs
	 * on them.
	 */
	{
		T_CHANGER, T_CHANGER, T_REMOV, "*", "*", "*",
		"ch", SC_MORE_LUS
	},
#endif	/* NCH */
#if NCD > 0 && !defined(UKTEST)
	{
		T_READONLY, T_READONLY, T_REMOV, "*", "*", "*",
		"cd", SC_ONE_LU
	},
#endif /* NCD */
#if NWORM > 0
	{
		T_WORM, T_WORM, T_REMOV, "*", "*", "*",
		"worm", SC_ONE_LU
	},
#endif /* NWORM */
	{
		0
	}
};

/*
 * Declarations
 */
static struct scsidevs *scsi_probedev __P((struct scsi_link *sc_link,
				    boolean *maybe_more, int *type_p));
static struct scsidevs *scsi_selectdev __P((u_int32_t qualifier, u_int32_t type,
				     boolean remov, char *manu, char *model,
				     char *rev));

/* XXX dufault@hda.com
 * This scsi_device doesn't have the scsi_data_size.
 * This is used during probe.
 */
static struct scsi_device probe_switch =
{
    NULL,
    NULL,
    NULL,
    NULL,
    "probe",
};

static int free_bus;			/* First bus not wired down */

static struct scsi_device *device_list;
static int next_free_type = T_NTYPES;

/* Register new functions at the head of the list.  That allows
 * you to replace a standard driver with a new one.
 *
 * You can't register the exact device (the same in memory structure)
 * more than once - the list links are part of the structure.  That is
 * prevented.
 *
 * Custom devices should always be registered as type "-1".  Then
 * the next available type number will be allocated for it.
 *
 * Be careful not to register a type as 0 unless you really mean to
 * replace the disk driver.
 *
 * This is usually called only by the "device_init" function generated
 * automatically in the SCSI_DEVICE_ENTRIES macro.
 */

void
scsi_device_register(struct scsi_device *sd)
{
	/* Not only is it pointless to add the same device more than once
	 * but it will also screw up the list.
	 */
	struct scsi_device *is_there;
	for (is_there = device_list; is_there; is_there = is_there->next)
		if (is_there == sd)
			return;

	if (sd->type == -1)
		sd->type = next_free_type++;

	sd->next = device_list;
	device_list = sd;

	if (sd->links == 0)
		sd->links = extend_new();
}

static struct scsi_device *
scsi_device_lookup(int type)
{
	struct scsi_device *sd;

	for (sd = device_list; sd; sd = sd->next)
		if (sd->type == type)
			return sd;

	return &uk_switch;
}

static struct scsi_device *
scsi_device_lookup_by_name(char *name)
{
	struct scsi_device *sd;

	for (sd = device_list; sd; sd = sd->next)
		if (strcmp(sd->name, name) == 0)
			return sd;

	return &uk_switch;
}

/* Macro that lets us know something is specified.
 */
#define IS_SPECIFIED(ARG) (ARG != SCCONF_UNSPEC && ARG != SCCONF_ANY)

/* scsi_init: Do all the one time processing.  This initializes the
 * type drivers and initializes the configuration.
 */
static void
scsi_init(void)
{
	static int done = 0;
	if(!done) {
		int i;

		done = 1;

		scbusses = extend_new();

		/* First call all type initialization functions.
		 */
		ukinit();	/* We always have the unknown device. */

		for (i = 0; scsi_tinit[i]; i++)
			(*scsi_tinit[i])();

		/* Lowest free bus for auto-configure is one
		 * more than the first one not
		 * specified in config:
		 */
		for (i = 0; scsi_cinit[i].driver; i++)
			if (IS_SPECIFIED(scsi_cinit[i].scbus) &&
			  free_bus <= scsi_cinit[i].scbus)
				free_bus = scsi_cinit[i].scbus + 1;

		/* Lowest free unit for each type for auto-configure is one
		 * more than the first one not specified in the config file:
		 */
	 	for (i = 0; scsi_dinit[i].name; i++) {
			struct scsi_device_config *sdc = scsi_dinit + i;
			struct scsi_device *sd =
			 scsi_device_lookup_by_name(sdc->name);

			/* This is a little tricky: We don't want "sd 4" to match as
			 * a wired down device, but we do want "sd 4 target 5" or
			 * even "sd 4 scbus 1" to match.
			 */
			if (IS_SPECIFIED(sdc->unit) &&
			  (IS_SPECIFIED(sdc->target) || IS_SPECIFIED(sdc->cunit)) &&
			  sd->free_unit <= sdc->unit)
				sd->free_unit = sdc->unit + 1;
	 	}
	}
}

/* scsi_bus_conf: Figure out which bus this is.  If it is wired in config
 * use that.  Otherwise use the next free one.
 */
static int
scsi_bus_conf(sc_link_proto)
	struct scsi_link *sc_link_proto;
{
	int i;
	int bus;

	/* Which bus is this?  Try to find a match in the "scsi_cinit"
	 * table.  If it isn't wired down auto-configure it at the
	 * next available bus.
	 */

	bus = SCCONF_UNSPEC;
	for (i = 0; scsi_cinit[i].driver; i++) {
		if (IS_SPECIFIED(scsi_cinit[i].scbus))
		{
			if (!strcmp(sc_link_proto->adapter->name, scsi_cinit[i].driver)
			  &&(sc_link_proto->adapter_unit == scsi_cinit[i].unit))
			{
			  if (IS_SPECIFIED(scsi_cinit[i].bus)) {
			     if (sc_link_proto->adapter_bus==scsi_cinit[i].bus){
				bus = scsi_cinit[i].scbus;
			   	break;
			     }
			  }
			  else if (sc_link_proto->adapter_bus == 0) {
			     /* Backwards compatibility for single bus cards */
			     bus = scsi_cinit[i].scbus;
			     break;
			  }
			  else {
			     printf("Ambiguous scbus configuration for %s%d "
				    "bus %d, cannot wire down.  The kernel "
				    "config entry for scbus%d should specify "
				    "a controller bus.\n"
				    "Scbus will be assigned dynamically.\n",
				    sc_link_proto->adapter->name,
				    sc_link_proto->adapter_unit,
				    sc_link_proto->adapter_bus,
				    sc_link_proto->adapter_bus );
			     break;
			  }
			}
		}
	}

			
	if (bus == SCCONF_UNSPEC)
		bus = free_bus++;
	else if (bootverbose)
		printf("Choosing drivers for scbus configured at %d\n", bus);

	return bus;
}

/* scsi_assign_unit: Look through the structure generated by config.
 * See if there is a fixed assignment for this unit.  If there isn't,
 * assign the next free unit.
 */
static int
scsi_assign_unit(struct scsi_link *sc_link)
{
	int i;
	int found;
#ifdef PC98
	struct cfdata cf;
	cf.cf_flags = 0;
#endif

	found = 0;
 	for (i = 0; scsi_dinit[i].name; i++) {
		if ((strcmp(sc_link->device->name, scsi_dinit[i].name) == 0) &&
		sc_link->target == scsi_dinit[i].target &&
		(
		 (sc_link->lun == scsi_dinit[i].lun) ||
		 (sc_link->lun == 0 && scsi_dinit[i].lun == SCCONF_UNSPEC)
		) &&
		sc_link->scsibus == scsi_dinit[i].cunit) {
			sc_link->dev_unit = scsi_dinit[i].unit;
			found = 1;
#ifdef PC98
			cf.cf_flags = scsi_dinit[i].flags;
#endif
			if (bootverbose)
				printf("%s is configured at %d\n",
				sc_link->device->name, sc_link->dev_unit);
			break;
		}
	}

	if (!found)
		sc_link->dev_unit = sc_link->device->free_unit++;

#ifdef PC98
	if (!found) {
		for (i = 0; scsi_dinit[i].name; i++) {
			if ((strcmp(sc_link->device->name, scsi_dinit[i].name) == 0) &&
				(scsi_dinit[i].target == SCCONF_UNSPEC))
				cf.cf_flags = scsi_dinit[i].flags;
		}
	}
	if (sc_link->adapter->open_target_lu)
		(*(sc_link->adapter->open_target_lu))(sc_link, &cf);
#endif

	return sc_link->dev_unit;
}


#if NSCTARG > 0
/* The SCSI target configuration is simpler.  If an entry is present
 * we just return the bus, target and lun for that unit.
 */
static void
scsi_sctarg_lookup(char *name, int unit, int *target, int *lun, int *bus)
{
	int i;

	*bus = SCCONF_UNSPEC;
	*target = SCCONF_UNSPEC;
	*lun = SCCONF_UNSPEC;

	for (i = 0; scsi_dinit[i].name; i++) {
		if ((strcmp(name, scsi_dinit[i].name) == 0) &&
		unit == scsi_dinit[i].unit)
		{
			*bus = scsi_dinit[i].cunit;
			*target = scsi_dinit[i].target;
			*lun = scsi_dinit[i].lun;
		}
	}
}
#endif /* NSCTARG > 0 */

void
scsi_configure_start(void)
{
	scsi_init();
}

#if NSCTARG > 0
static errval scsi_attach_sctarg __P((void));
#endif

void
scsi_configure_finish(void)
{

#if NSCTARG > 0
	scsi_attach_sctarg();
#endif

}

/*
 * scsi_attachdevs is the routine called by the adapter boards
 * to get all their devices configured in.
 */
void
scsi_attachdevs(scbus)
	struct scsibus_data *scbus;
{
	int scsibus;
	struct scsi_link *sc_link_proto = scbus->adapter_link;

	if ( (scsibus = scsi_bus_conf(sc_link_proto)) == -1) {
		return;
	}
	/*
	 * if the adapter didn't give us this, set a default
	 * (compatibility with old adapter drivers)
	 */
	if(!(sc_link_proto->opennings)) {
		sc_link_proto->opennings = 1;
	}
	sc_link_proto->scsibus = scsibus;
	/*
	 * Allocate our target-lun space.
	 */
	scbus->sc_link = (struct scsi_link *(*)[][8])malloc(
		sizeof(struct scsi_link *[scbus->maxtarg + 1][8]),
		M_TEMP, M_NOWAIT);
	if(scbus == 0 || scbus->sc_link == 0
	   || extend_set(scbusses, scsibus, scbus) == 0) {
		panic("scsi_attachdevs: malloc");
	}
	bzero(scbus->sc_link, sizeof(struct scsi_link*[scbus->maxtarg + 1][8]));
#if defined(SCSI_DELAY) && SCSI_DELAY > 2
	printf("%s%d: waiting for scsi devices to settle\n",
	    sc_link_proto->adapter->name, sc_link_proto->adapter_unit);
#else	/* SCSI_DELAY > 2 */
#undef	SCSI_DELAY
#define SCSI_DELAY 2
#endif	/* SCSI_DELAY */
	DELAY(1000000 * SCSI_DELAY);
	scsi_probe_bus(scsibus,-1,-1);
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
		for(bus = 0; bus < scbusses->nelem; bus++) {
			scsi_probe_bus(bus, targ, lun);
		}
		return 0;
	} else {
		return scsi_probe_bus(bus, targ, lun);
	}
}

/* scsi_alloc_unit: Register a scsi_data pointer for a given
 * unit in a given scsi_device structure.
 *
 * XXX dufault@hda.com: I still don't like the way this reallocs stuff -
 * but at least now it is collected in one place instead of existing
 * in multiple type drivers.  I'd like it better if we had it do a
 * second pass after it knew the sizes of everything and set up everything
 * at once.
 */
static int
scsi_alloc_unit(struct scsi_link *sc_link)
{
	u_int32_t unit;
	struct scsi_data *sd;
	struct scsi_device *dsw;

	unit = sc_link->dev_unit;
	dsw = sc_link->device;

	/*
	 * allocate the per unit data area
	 */
	if (dsw->sizeof_scsi_data)
	{
		sd = malloc(dsw->sizeof_scsi_data, M_DEVBUF, M_NOWAIT);
		if (!sd) {
			printf("%s%ld: malloc failed for scsi_data\n",
				sc_link->device->name, unit);
			return 0;
		}
		bzero(sd, dsw->sizeof_scsi_data);
	}
	else
		sd = 0;

	sc_link->sd = sd;

	if (extend_set(dsw->links, unit, (void *)sc_link) == 0) {
		printf("%s%ld: Can't store link pointer.\n",
		sc_link->device->name, unit);
		free(sd, M_DEVBUF);
		return 0;
	}

	return 1;
}

static void
scsi_free_unit(struct scsi_link *sc_link)
{
	if (sc_link->sd)
	{
		free(sc_link->sd, M_DEVBUF);
		sc_link->sd = 0;
	}
	extend_release(sc_link->device->links, sc_link->dev_unit);
}

#if NSCTARG > 0

/* XXX: It is a bug that the sc_link has this information
 *      about the adapter in it.  The sc_link should refer to
 *      a structure that is host adpater specific.  That will also
 *      pull all knowledge of an sc_link out of the adapter drivers.
 */

errval
scsi_set_bus(int bus, struct scsi_link *sc_link)
{
	struct scsi_link *ad_link;
	struct scsibus_data *scsibus_data;

	if (bus < 0 || bus > scbusses->nelem) {
		return ENXIO;
	}

	scsibus_data = (struct scsibus_data *)extend_get(scbusses, bus);

	if(!scsibus_data) {
		return ENXIO;
	}

	ad_link = scsibus_data->adapter_link;

	sc_link->adapter_unit = ad_link->adapter_unit;
	sc_link->adapter_targ = ad_link->adapter_targ;
	sc_link->adapter = ad_link->adapter;
	sc_link->device = ad_link->device;
	sc_link->flags = ad_link->flags;

	return 0;
}

/*
 * Allocate and attach as many SCSI target devices as configured.
 * There are two ways that you can configure the target device:
 * 1. In the configuration file.  That is handled here.
 * 2. Via the minor number.  That takes precedence over the config file.
 */
static errval
scsi_attach_sctarg()
{
	struct scsi_link *sc_link = NULL;
	int dev_unit;
	struct scsi_device *sctarg = scsi_device_lookup(T_TARGET);

	if (sctarg == 0) {
		return ENXIO;
	}

	for (dev_unit = 0; dev_unit < NSCTARG; dev_unit++) {

		int target, lun, bus;

		/* If we don't have a link block allocate one.
		 */
		if (!sc_link) {
			sc_link = malloc(sizeof(*sc_link), M_TEMP, M_NOWAIT);
		}

		scsi_sctarg_lookup(sctarg->name, dev_unit, &target, &lun, &bus);

		if (IS_SPECIFIED(bus)) {
			struct scsibus_data *scsibus_data;

			if (bus < 0 || bus > scbusses->nelem) {
				printf("%s%d: configured on illegal bus %d.\n",
				sctarg->name, dev_unit, bus);
				continue;
			}

			scsibus_data = (struct scsibus_data *)extend_get(scbusses, bus);

			if(!scsibus_data) {
				printf("%s%d: no bus %d.\n", sctarg->name, dev_unit, bus);
				continue;
			}

			*sc_link = *scsibus_data->adapter_link;	/* struct copy */
			sc_link->target = target;
			sc_link->lun = lun;
		}
		else {
			/* This will be configured in the open routine.
			 */
			sc_link->scsibus = SCCONF_UNSPEC;
			sc_link->target = SCCONF_UNSPEC;
			sc_link->lun = SCCONF_UNSPEC;
		}

		sc_link->quirks = 0;
		sc_link->device = sctarg;
		sc_link->dev_unit = dev_unit;

		if (scsi_alloc_unit(sc_link)) {

			if (scsi_device_attach(sc_link) == 0) {
				sc_link = NULL;		/* it's been used */
			}
			else
				scsi_free_unit(sc_link);
		}
	}

	if (sc_link) {
		free(sc_link, M_TEMP);
	}

	return 0;
}
#endif /* NSCTARG > 0 */

/*
 * Allocate a scsibus_data structure
 * The target/lun area is dynamically allocated in scsi_attachdevs after
 * the controller driver has a chance to update the maxtarg field.
 */
struct scsibus_data*
scsi_alloc_bus()
{
	struct scsibus_data *scbus;
	/*
	 * Prepare the scsibus_data area for the upperlevel
	 * scsi code.
	 */
	scbus = malloc(sizeof(struct scsibus_data), M_TEMP, M_NOWAIT);
	if(!scbus) {
                printf("scsi_alloc_bus: - cannot malloc!\n");
                return NULL;
	}
	bzero(scbus, sizeof(struct scsibus_data));
	/* Setup the defaults */
	scbus->maxtarg = 7;
	scbus->maxlun = 7;
	return scbus;
}

/*
 * Probe the requested scsi bus. It must be already set up.
 * targ and lun optionally narrow the search if not -1
 */
errval
scsi_probe_bus(int bus, int targ, int lun)
{
	struct scsibus_data *scsibus_data ;
	int	maxtarg,mintarg,maxlun,minlun;
	struct scsi_link *sc_link_proto;
	u_int8_t  scsi_addr ;
	struct scsidevs *bestmatch = NULL;
	struct scsi_link *sc_link = NULL;
	boolean maybe_more;
	int type;

	if ((bus < 0 ) || ( bus >= scbusses->nelem)) {
		return ENXIO;
	}
	scsibus_data = (struct scsibus_data *)extend_get(scbusses, bus);
	if(!scsibus_data) return ENXIO;
	sc_link_proto = scsibus_data->adapter_link;
	scsi_addr = sc_link_proto->adapter_targ;
	if(targ == -1){
		maxtarg = scsibus_data->maxtarg;
		mintarg = 0;
	} else {
		if((targ < 0 ) || (targ > scsibus_data->maxtarg)) return EINVAL;
		maxtarg = mintarg = targ;
	}

	if(lun == -1){
		maxlun = scsibus_data->maxlun;
		minlun = 0;
	} else {
		if((lun < 0 ) || (lun > scsibus_data->maxlun)) return EINVAL;
		maxlun = minlun = lun;
	}

	printf("scbus%d at %s%d bus %d\n",
		sc_link_proto->scsibus, sc_link_proto->adapter->name,
		sc_link_proto->adapter_unit, sc_link_proto->adapter_bus);

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
			if((*scsibus_data->sc_link)[targ][lun])
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
			}
			*sc_link = *sc_link_proto;	/* struct copy */
			sc_link->device = &probe_switch;
			sc_link->target = targ;
			sc_link->lun = lun;
			sc_link->quirks = 0;
			bestmatch = scsi_probedev(sc_link, &maybe_more, &type);
			if (bestmatch) {
			    sc_link->quirks = bestmatch->quirks;
			    sc_link->devmodes = bestmatch->devmodes;
			} else {
			    sc_link->quirks = 0;
			    sc_link->devmodes = NULL;
			}
			if (bestmatch) {		/* FOUND */
				sc_link->device = scsi_device_lookup(type);

				(void)scsi_assign_unit(sc_link);

				if (scsi_alloc_unit(sc_link)) {

					if (scsi_device_attach(sc_link) == 0) {
						(*scsibus_data->sc_link)[targ][lun] = sc_link;
						sc_link = NULL;		/* it's been used */
					}
					else
						scsi_free_unit(sc_link);
				}
			}

			if (!(maybe_more)) {	/* nothing suggests we'll find more */
				break;				/* nothing here, skip to next targ */
			}
			/* otherwise something says we should look further */
		}
	}
	if (sc_link) {
		free(sc_link, M_TEMP);
	}
	return 0;
}

/* Return the scsi_link for this device, if any.
 */
struct scsi_link *
scsi_link_get(bus, targ, lun)
	int bus;
	int targ;
	int lun;
{
	struct scsibus_data *scsibus_data =
	 (struct scsibus_data *)extend_get(scbusses, bus);
	return (scsibus_data) ? (*scsibus_data->sc_link)[targ][lun] : 0;
}

/* make_readable: Make the inquiry data readable.  Anything less than a ' '
 * is made a '?' and trailing spaces are removed.
 */
static void
make_readable(to, from, n)
	char *to;
	char *from;
	size_t n;
{
	int i;

	for (i = 0; from[i] && i < n - 1; i++) {
		if (from[i] < ' ')
			to[i]='?';
		else
			to[i] = from[i];
	}

	while (i && to[i - 1] == ' ')
		i--;

	to[i] = 0;
}

#ifndef SCSIDEBUG
void scsi_print_info(sc_link)
	struct scsi_link *sc_link;
{
	int    dtype = 0;
	char   *desc;
	char   *qtype;
	struct scsi_inquiry_data *inqbuf;
	u_int32_t len, qualifier, type;
	boolean remov;
	char    manu[8 + 1];
	char    model[16 + 1];
	char    version[4 + 1];

 	inqbuf = &sc_link->inqbuf;

	type = inqbuf->device & SID_TYPE;
	qualifier = inqbuf->device & SID_QUAL;
	remov = inqbuf->dev_qual2 & SID_REMOVABLE;

	switch ((int)qualifier) {
	case SID_QUAL_LU_OK:
		qtype = "";
		break;

	case SID_QUAL_LU_OFFLINE:
		qtype = "Supported device currently not connected";
		break;

	default:
		dtype = 1;
		qtype = "Vendor specific peripheral qualifier";
		break;
	}

	if ((inqbuf->version & SID_ANSII) > 0) {
		if ((len = inqbuf->additional_length
			+ ((char *) inqbuf->unused
			    - (char *) inqbuf))
		    > (sizeof(struct scsi_inquiry_data) - 1))
			        len = sizeof(struct scsi_inquiry_data) - 1;
		desc = inqbuf->vendor;
		desc[len - (desc - (char *) inqbuf)] = 0;
		make_readable(manu, inqbuf->vendor, sizeof(manu));
		make_readable(model, inqbuf->product, sizeof(model));
		make_readable(version, inqbuf->revision, sizeof(version));
	} else {
		/*
		 * If not advanced enough, use default values
		 */
		desc = "early protocol device";
		make_readable(manu, "unknown", sizeof(manu));
		make_readable(model, "unknown", sizeof(model));
		make_readable(version, "????", sizeof(version));
	}

	printf("%s%d: ", sc_link->device->name,
			 sc_link->dev_unit);
	printf("<%s %s %s> ", manu, model, version );
	printf("type %ld %sSCSI %d"
	    ,type
	    ,remov ? "removable " : "fixed "
	    ,inqbuf->version & SID_ANSII
	    );
	if (qtype[0]) {
		sc_print_addr(sc_link);
		printf(" qualifier %ld: %s" ,qualifier ,qtype);
	}

	printf("\n");
}
#endif

/*
 * given a target and lu, ask the device what
 * it is, and find the correct driver table
 * entry.
 */
static struct scsidevs *
scsi_probedev(sc_link, maybe_more, type_p)
	boolean *maybe_more;
	struct scsi_link *sc_link;
	int *type_p;
{
	u_int8_t  target = sc_link->target;
	u_int8_t  lu = sc_link->lun;
	struct scsidevs *bestmatch = (struct scsidevs *) 0;
	int    dtype = 0;
	char   *desc;
	char   *qtype;
	struct scsi_inquiry_data *inqbuf;
	u_int32_t len, qualifier, type;
	boolean remov;
	char    manu[8 + 1];
	char    model[16 + 1];
	char    version[4 + 1];

 	inqbuf = &sc_link->inqbuf;

 	bzero(inqbuf, sizeof(*inqbuf));
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
	if (scsi_inquire(sc_link, inqbuf, SCSI_NOSLEEP | SCSI_NOMASK) != 0) {
		return (struct scsidevs *) 0;
	}

	/*
	 * note what BASIC type of device it is
	 */
	type = inqbuf->device & SID_TYPE;
	qualifier = inqbuf->device & SID_QUAL;
	remov = inqbuf->dev_qual2 & SID_REMOVABLE;

	/*
	 * Any device qualifier that has the top bit set (qualifier&4 != 0)
	 * is vendor specific and will match in the default of this switch.
	 */

	switch ((int)qualifier) {
	case SID_QUAL_LU_OK:
		qtype = "";
		break;

	case SID_QUAL_LU_OFFLINE:
		qtype = "Supported device currently not connected";
		break;

	case SID_QUAL_RSVD:	/* Peripheral qualifier reserved in SCSI-2 spec */
		*maybe_more = 1;
		return (struct scsidevs *) 0;

	case SID_QUAL_BAD_LU:	/* Target can not support a device on this unit */
		/*
		 * Check for a non-existent unit.  If the device is returning
		 * this much, then we must set the flag that has
		 * the searchers keep looking on other luns.
		 */
		*maybe_more = 1;
		return (struct scsidevs *) 0;

	default:
		dtype = 1;
		qtype = "Vendor specific peripheral qualifier";
		*maybe_more = 1;
		break;
	}

	if (dtype == 0) {
		if (type == T_NODEVICE) {
			*maybe_more = 1;
			return (struct scsidevs *) 0;
		}
		dtype = 1;
	}
	/*
	 * Then if it's advanced enough, more detailed
	 * information
	 */
	if ((inqbuf->version & SID_ANSII) > 0) {
		if ((len = inqbuf->additional_length
			+ ((char *) inqbuf->unused
			    - (char *) inqbuf))
		    > (sizeof(struct scsi_inquiry_data) - 1))
			        len = sizeof(struct scsi_inquiry_data) - 1;
		desc = inqbuf->vendor;
		desc[len - (desc - (char *) inqbuf)] = 0;
		make_readable(manu, inqbuf->vendor, sizeof(manu));
		make_readable(model, inqbuf->product, sizeof(model));
		make_readable(version, inqbuf->revision, sizeof(version));
	} else {
		/*
		 * If not advanced enough, use default values
		 */
		desc = "early protocol device";
		make_readable(manu, "unknown", sizeof(manu));
		make_readable(model, "unknown", sizeof(model));
		make_readable(version, "????", sizeof(version));
		type = T_UNKNOWN;
	}

#ifdef SCSIDEBUG
	sc_print_start(sc_link);

	printf("<%s %s %s> ", manu, model, version );
	printf("type %ld %sSCSI %d"
	    ,type
	    ,remov ? "removable " : "fixed "
	    ,inqbuf->version & SID_ANSII
	    );
	if (qtype[0]) {
		sc_print_addr(sc_link);
		printf(" qualifier %ld: %s" ,qualifier ,qtype);
	}

	printf("\n");
	sc_print_finish();
#endif
	/*
	 * Try make as good a match as possible with
	 * available sub drivers
	 */
	bestmatch = (scsi_selectdev(
		qualifier, type, remov ? T_REMOV : T_FIXED, manu, model, version));
	if ((bestmatch) && (bestmatch->flags & SC_MORE_LUS)) {
		*maybe_more = 1;
	}

	/* If the device is unknown then we should be trying to look up a
	 * type driver based on the inquiry type.
	 */
	if (bestmatch == &unknowndev)
		*type_p = type;
	else
		*type_p =
			bestmatch->driver;
	return bestmatch;
}

/* Try to find the major number for a device during attach.
 */
dev_t
scsi_dev_lookup(d_open)
	d_open_t *d_open;
{
	int i;

	dev_t d = NODEV;

	for (i = 0; i < nchrdev; i++)
		if (cdevsw[i] && cdevsw[i]->d_open == d_open)
		{
			d = makedev(i, 0);
			break;
		}

	return d;
}

/*
 * Compare name with pattern, return 0 on match.
 * Short pattern matches trailing blanks in name,
 * wildcard '*' in pattern matches rest of name
 */
static int
match(pattern, name)
	char *pattern;
	char *name;
{
	char c;
	while (c = *pattern++)
	{
		if (c == '*') return 0;
		if ((c == '?') && (*name > ' ')) continue;
		if (c != *name++) return 1;
	}
	while (c = *name++)
	{
		if (c != ' ') return 1;
	}
	return 0;
}

/*
 * Try make as good a match as possible with
 * available sub drivers
 */
static struct scsidevs *
scsi_selectdev(qualifier, type, remov, manu, model, rev)
	u_int32_t qualifier, type;
	boolean remov;
	char   *manu, *model, *rev;
{
	struct scsidevs *bestmatch = NULL;
	struct scsidevs *thisentry;

	type |= qualifier;	/* why? */

	for ( thisentry = knowndevs; thisentry->manufacturer; thisentry++ )
	{
		if (type != thisentry->type) {
			continue;
		}
		if (remov != thisentry->removable) {
			continue;
		}

		if (thisentry->flags & SC_SHOWME)
			printf("\n%s-\n%s-", thisentry->manufacturer, manu);
		if (match(thisentry->manufacturer, manu)) {
			continue;
		}
		if (thisentry->flags & SC_SHOWME)
			printf("\n%s-\n%s-", thisentry->model, model);
		if (match(thisentry->model, model)) {
			continue;
		}
		if (thisentry->flags & SC_SHOWME)
			printf("\n%s-\n%s-", thisentry->version, rev);
		if (match(thisentry->version, rev)) {
			continue;
		}
		bestmatch = thisentry;
		break;
	}
	if (bestmatch == (struct scsidevs *) 0) {
		bestmatch = &unknowndev;
	}
	return (bestmatch);
}
