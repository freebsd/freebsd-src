/*-
 * Copyright (c) 1997, 1998, 1999
 *	Nan Yang Computer Services Limited.  All rights reserved.
 *
 *  Parts copyright (c) 1997, 1998 Cybernet Corporation, NetMAX project.
 *
 *  Written by Greg Lehey
 *
 *  This software is distributed under the so-called ``Berkeley
 *  License'':
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Nan Yang Computer
 *	Services Limited.
 * 4. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even if
 * advised of the possibility of such damage.
 *
 * $Id: vinumvar.h,v 1.33 2003/05/23 01:09:23 grog Exp $
 * $FreeBSD$
 */

#include <sys/time.h>
#include <dev/vinum/vinumstate.h>
#include <sys/mutex.h>

/* Directory for device nodes. */
#define VINUM_DIR   "/dev/vinum"

/*
 * Some configuration maxima.  They're an enum because
 * we can't define global constants.  Sorry about that.
 *
 * These aren't as bad as they look: most of them are soft limits.
 */

#define VINUMROOT
enum constants {
    /*
     * Current version of the data structures.  This
     * is used to ensure synchronization between
     * kernel module and userland vinum(8).
     */
    VINUMVERSION = 1,
    VINUM_HEADER = 512,					    /* size of header on disk */
    MAXCONFIGLINE = 1024,				    /* maximum size of a single config line */
    MINVINUMSLICE = 1048576,				    /* minimum size of a slice */

    VINUM_CDEV_MAJOR = 91,				    /* major number for character device */

    ROUND_ROBIN_READPOL = -1,				    /* round robin read policy */

    /*
     * Type field in high-order two bits of minor
     * number.  Subdisks are in fact both type 2 and
     * type 3, giving twice the number of subdisks.
     * This causes some ugliness in the code.
     */
    VINUM_VOLUME_TYPE = 0,
    VINUM_PLEX_TYPE = 1,
    VINUM_SD_TYPE = 2,
    VINUM_SD2_TYPE = 3,


    /*
     * Define a minor device number.
     * This is not used directly; instead, it's
     * called by the other macros.
     */
#define VINUMMINOR(o,t)  ((o & 0xff) | ((o & 0x3fff00) << 8) | (t << VINUM_TYPE_SHIFT))

    VINUM_TYPE_SHIFT = 30,
    VINUM_MAXVOL = 0x3ffffd,				    /* highest numbered volume */

    /*
     * The super device and the daemon device are
     * magic: they're the two highest-numbered
     * volumes.
     */
    VINUM_SUPERDEV_VOL = 0x3ffffe,
    VINUM_DAEMON_VOL = 0x3fffff,
    VINUM_MAXPLEX = 0x3fffff,
    VINUM_MAXSD = 0x7fffff,

#define VINUM_SUPERDEV_MINOR VINUMMINOR (VINUM_SUPERDEV_VOL, VINUM_VOLUME_TYPE)
#define VINUM_DAEMON_MINOR   VINUMMINOR (VINUM_DAEMON_VOL, VINUM_VOLUME_TYPE)

    /*
     * Mask for the number part of each object.
     * Plexes and volumes are the same, subdisks use
     * the low-order bit of the type field and thus
     * have twice the number.
     */

    MAJORDEV_SHIFT = 8,

    MAXPLEX = 8,					    /* maximum number of plexes in a volume */
    MAXSD = 256,					    /* maximum number of subdisks in a plex */
    MAXDRIVENAME = 32,					    /* maximum length of a device name */
    MAXSDNAME = 64,					    /* maximum length of a subdisk name */
    MAXPLEXNAME = 64,					    /* maximum length of a plex name */
    MAXVOLNAME = 64,					    /* maximum length of a volume name */
    MAXNAME = 64,					    /* maximum length of any name */


#define OBJTYPE(x)	((minor(x) >> VINUM_TYPE_SHIFT) & 3)

    /* Create device minor numbers */
#define VINUMDEV(o, t)		makedev (VINUM_CDEV_MAJOR, VINUMMINOR (o, t))

#define VINUM_VOL(v)		makedev (VINUM_CDEV_MAJOR, \
					 VINUMMINOR (v, VINUM_VOLUME_TYPE))
#define VINUM_PLEX(p)		makedev (VINUM_CDEV_MAJOR, \
					 VINUMMINOR (p, VINUM_PLEX_TYPE))
#define VINUM_SD(s)		makedev (VINUM_CDEV_MAJOR, \
					 VINUMMINOR (s, VINUM_SD_TYPE))

    /* extract device type */
#define DEVTYPE(x) ((minor (x) >> VINUM_TYPE_SHIFT) & 3)

#define VINUM_SUPERDEV_NAME VINUM_DIR"/control"		    /* normal super device */
#define VINUM_DAEMON_DEV_NAME VINUM_DIR"/controld"	    /* super device for daemon only */

    /*
     * the number of object entries to cater for initially, and also the
     * value by which they are incremented.  It doesn't take long
     * to extend them, so theoretically we could start with 1 of each, but
     * it's untidy to allocate such small areas.  These values are
     * probably too small.
     */

    INITIAL_DRIVES = 4,
    INITIAL_VOLUMES = 4,
    INITIAL_PLEXES = 8,
    INITIAL_SUBDISKS = 16,
    INITIAL_SUBDISKS_IN_PLEX = 4,			    /* number of subdisks to allocate to a plex */
    INITIAL_SUBDISKS_IN_DRIVE = 4,			    /* number of subdisks to allocate to a drive */
    INITIAL_DRIVE_FREELIST = 16,			    /* number of entries in drive freelist */
    PLEX_REGION_TABLE_SIZE = 8,				    /* number of entries in plex region tables */
    PLEX_LOCKS = 256,					    /* number of locks to allocate to a plex */
    PLEXMUTEXES = 32,
    MAX_REVIVE_BLOCKSIZE = MAXPHYS,			    /* maximum revive block size */
    DEFAULT_REVIVE_BLOCKSIZE = 65536,			    /* default revive block size */
    VINUMHOSTNAMELEN = 32,				    /* host name field in label */
};

/*
 * Slice header
 *
 * Vinum drives start with this structure:
 *
 *\                                            Sector
 * |--------------------------------------|
 * |   PDP-11 memorial boot block         |      0
 * |--------------------------------------|
 * |   Disk label, maybe                  |      1
 * |--------------------------------------|
 * |   Slice definition  (vinum_hdr)      |      8
 * |--------------------------------------|
 * |                                      |
 * |   Configuration info, first copy     |      9
 * |                                      |
 * |--------------------------------------|
 * |                                      |
 * |   Configuration info, second copy    |      9 + size of config
 * |                                      |
 * |--------------------------------------|
 */

/* Sizes and offsets of our information */
enum {
    VINUM_LABEL_OFFSET = 4096,				    /* offset of vinum label */
    VINUMHEADERLEN = 512,				    /* size of vinum label */
    VINUM_CONFIG_OFFSET = 4608,				    /* offset of first config copy */
    MAXCONFIG = 65536,					    /* and size of config copy */
    DATASTART = (MAXCONFIG * 2 + VINUM_CONFIG_OFFSET) / DEV_BSIZE /* this is where the data starts */
};

/*
 * hostname is 256 bytes long, but we don't need to shlep
 * multiple copies in vinum.  We use the host name just
 * to identify this system, and 32 bytes should be ample
 * for that purpose
 */

struct vinum_label {
    char sysname[VINUMHOSTNAMELEN];			    /* system name at time of creation */
    char name[MAXDRIVENAME];				    /* our name of the drive */
    struct timeval date_of_birth;			    /* the time it was created */
    struct timeval last_update;				    /* and the time of last update */
    /*
     * total size in bytes of the drive.  This value
     * includes the headers.
     */
    off_t drive_size;
};

struct vinum_hdr {
    uint64_t magic;					    /* we're long on magic numbers */
#define VINUM_MAGIC    22322600044678729LL		    /* should be this */
#define VINUM_NOMAGIC  22322600044678990LL		    /* becomes this after obliteration */
    /*
     * Size in bytes of each copy of the
     * configuration info.  This must be a multiple
     * of the sector size.
     */
    int config_length;
    struct vinum_label label;				    /* unique label */
};

/* Information returned from read_drive_label */
enum drive_label_info {
    DL_CANT_OPEN,					    /* invalid partition */
    DL_NOT_OURS,					    /* valid partition, but no vinum label */
    DL_DELETED_LABEL,					    /* valid partition, deleted label found */
    DL_WRONG_DRIVE,					    /* drive name doesn't match */
    DL_OURS						    /* valid partition and label found */
};

/* kinds of plex organization */
enum plexorg {
    plex_disorg,					    /* disorganized */
    plex_concat,					    /* concatenated plex */
    plex_striped,					    /* striped plex */
    plex_raid4,						    /* RAID4 plex */
    plex_raid5						    /* RAID5 plex */
};

/* Recognize plex organizations */
#define isstriped(p) (p->organization >= plex_striped)	    /* RAID 1, 4 or 5 */
#define isparity(p) (p->organization >= plex_raid4)	    /* RAID 4 or 5 */

/* Address range definitions, for locking volumes */
struct rangelock {
    daddr_t stripe;					    /* address + 1 of the range being locked  */
    struct buf *bp;					    /* user's buffer pointer */
};

struct drive_freelist {					    /* sorted list of free space on drive */
    u_int64_t offset;					    /* offset of entry */
    u_int64_t sectors;					    /* and length in sectors */
};

/*
 * Include the structure definitions shared
 * between userland and kernel.
 */

#ifdef _KERNEL
#include <dev/vinum/vinumobj.h>
#undef _KERNEL
#include <dev/vinum/vinumobj.h>
#define _KERNEL
#else
#include <dev/vinum/vinumobj.h>
#endif

/*
 * Table expansion.  Expand table, which contains oldcount
 * entries of type element, by increment entries, and change
 * oldcount accordingly
 */
#ifdef VINUMDEBUG
#define EXPAND(table, element, oldcount, increment)         \
{							    \
  expand_table ((void **) &table,			    \
		oldcount * sizeof (element),		    \
		(oldcount + increment) * sizeof (element),  \
		__FILE__,				    \
		__LINE__ );				    \
  oldcount += increment;				    \
  }
#else
#define EXPAND(table, element, oldcount, increment)         \
{							    \
  expand_table ((void **) &table,			    \
		oldcount * sizeof (element),		    \
		(oldcount + increment) * sizeof (element)); \
  oldcount += increment;				    \
  }
#endif

/* Information on vinum's memory usage */
struct meminfo {
    int mallocs;					    /* number of malloced blocks */
    int total_malloced;					    /* total amount malloced */
    int highwater;					    /* maximum number of mallocs */
    struct mc *malloced;				    /* pointer to kernel table */
};

#define MCFILENAMELEN	16
struct mc {
    struct timeval time;
    int seq;
    int size;
    short line;
    caddr_t address;
    char file[MCFILENAMELEN];
};

/*
 * These enums are used by the state transition
 * routines.  They're in bit map format:
 *
 * Bit 0: Other plexes in the volume are down
 * Bit 1: Other plexes in the volume are up
 * Bit 2: The current plex is up
 * Maybe they should be local to
 * state.c
 */
enum volplexstate {
    volplex_onlyusdown = 0,				    /* 0: we're the only plex, and we're down */
    volplex_alldown,					    /* 1: another plex is down, and so are we */
    volplex_otherup,					    /* 2: another plex is up */
    volplex_otherupdown,				    /* 3: other plexes are up and down */
    volplex_onlyus,					    /* 4: we're up and alone */
    volplex_onlyusup,					    /* 5: only we are up, others are down */
    volplex_allup,					    /* 6: all plexes are up */
    volplex_someup					    /* 7: some plexes are up, including us */
};

/* state map for plex */
enum sdstates {
    sd_emptystate = 1,
    sd_downstate = 2,					    /* SD is down */
    sd_crashedstate = 4,				    /* SD is crashed */
    sd_obsoletestate = 8,				    /* SD is obsolete */
    sd_stalestate = 16,					    /* SD is stale */
    sd_rebornstate = 32,				    /* SD is reborn */
    sd_upstate = 64,					    /* SD is up */
    sd_initstate = 128,					    /* SD is initializing */
    sd_initializedstate = 256,				    /* SD is initialized */
    sd_otherstate = 512,				    /* SD is in some other state */
};

/*
 * This is really just a parameter to pass to
 * set_<foo>_state, but since it needs to be known
 * in the external definitions, we need to define
 * it here
 */
enum setstateflags {
    setstate_none = 0,					    /* no flags */
    setstate_force = 1,					    /* force the state change */
    setstate_configuring = 2,				    /* we're currently configuring, don't save */
};

/* Operations for parityops to perform. */
enum parityop {
    checkparity,
    rebuildparity,
    rebuildandcheckparity,				    /* rebuildparity with the -v option */
};

#ifdef VINUMDEBUG
/* Debugging stuff */
enum debugflags {
    DEBUG_ADDRESSES = 1,				    /* show buffer information during requests */
    DEBUG_NUMOUTPUT = 2,				    /* show the value of vp->v_numoutput */
    DEBUG_RESID = 4,					    /* go into debugger in complete_rqe */
    DEBUG_LASTREQS = 8,					    /* keep a circular buffer of last requests */
    DEBUG_REVIVECONFLICT = 16,				    /* print info about revive conflicts */
    DEBUG_EOFINFO = 32,					    /* print info about EOF detection */
    DEBUG_MEMFREE = 64,					    /* keep info about Frees */
    DEBUG_BIGDRIVE = 128,				    /* pretend our drives are 100 times the size */
    DEBUG_REMOTEGDB = 256,				    /* go into remote gdb */
    DEBUG_WARNINGS = 512,				    /* log various relatively harmless warnings  */
    DEBUG_LOCKREQS = 1024,				    /* log locking requests  */
};

#ifdef _KERNEL
#ifdef __i386__
#define longjmp LongJmp					    /* test our longjmps */
#endif
#endif
#endif
/* Local Variables: */
/* fill-column: 50 */
/* End: */
