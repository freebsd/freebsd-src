/*-
 * Copyright (c) 1997, 1998
 *	Nan Yang Computer Services Limited.  All rights reserved.
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
 *      Services Limited.
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
 * $Id: vinumvar.h,v 1.15 1998/08/14 06:36:41 grog Exp grog $
 */

/* XXX gdb can't find our global pointers, so use this kludge to
 * point to them locally.  Remove after testing */
#define BROKEN_GDB struct _vinum_conf *VC = &vinum_conf

#include <sys/time.h>
#include "vinumstate.h"
/* Some configuration maxima.  They're an enum because
 * we can't define global constants.  Sorry about that.
 *
 * These aren't as bad as they look: most of them
 * are soft limits.  Only the MAXCONFIG parameter is set in stone
 */

enum constants {
    VINUM_HEADER = 512,					    /* size of header on disk */
    MAXCONFIGLINE = 1024,				    /* maximum size of a single config line */
    /* XXX Do we still need this? */
    MINVINUMSLICE = 1048576,				    /* minimum size of a slice */

    CDEV_MAJOR = 91,					    /* major number for character device */
    BDEV_MAJOR = 25,					    /* and block device */

    ROUND_ROBIN_READPOL = -1,				    /* round robin read policy */

    /* type field in minor number */
    VINUM_VOLUME_TYPE = 0,
    VINUM_PLEX_TYPE = 1,
    VINUM_SD_TYPE = 2,
    VINUM_DRIVE_TYPE = 3,
    VINUM_SUPERDEV_TYPE = 4,				    /* super device. */

    /* Shifts for the individual fields in the device */
    VINUM_TYPE_SHIFT = 28,
    VINUM_VOL_SHIFT = 0,
    VINUM_PLEX_SHIFT = 16,
    VINUM_SD_SHIFT = 20,
    VINUM_VOL_WIDTH = 8,
    VINUM_PLEX_WIDTH = 3,
    VINUM_SD_WIDTH = 8,
    MAJORDEV_SHIFT = 8,


/* Create a block device number */
#define VINUMBDEV(v,p,s,t)  ((BDEV_MAJOR << MAJORDEV_SHIFT)	\
			     | (v << VINUM_VOL_SHIFT)		\
			     | (p << VINUM_PLEX_SHIFT)		\
			     | (s << VINUM_SD_SHIFT) 		\
			     | (t << VINUM_TYPE_SHIFT) )

/* And a character device number */
#define VINUMCDEV(v,p,s,t)  ((CDEV_MAJOR << MAJORDEV_SHIFT)	\
			     | (v << VINUM_VOL_SHIFT)		\
			     | (p << VINUM_PLEX_SHIFT)		\
			     | (s << VINUM_SD_SHIFT) 		\
			     | (t << VINUM_TYPE_SHIFT) )

/* extract device type */
#define DEVTYPE(x) ((x >> VINUM_TYPE_SHIFT) & 7)

/* extract volume number */
#define VOLNO(x) (x & ((1 << VINUM_VOL_WIDTH) - 1))

/* extract plex number */
#define PLEXNO(x) (VOL [VOLNO (x)].plex [(x >> VINUM_PLEX_SHIFT) & ((1 << VINUM_PLEX_WIDTH) - 1)])

/* extract subdisk number */
#define SDNO(x) (PLEX [PLEXNO (x)].sdnos [(x >> VINUM_SD_SHIFT) & ((1 << VINUM_SD_WIDTH) - 1)])

/* extract drive number */
#define DRIVENO(x) (SD [SDNO (x)].driveno)

    VINUM_SUPERDEV = VINUMBDEV(0, 0, 0, VINUM_SUPERDEV_TYPE), /* superdevice number */

/* the number of object entries to cater for initially, and also the
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
    INITIAL_LOCKS = 8,					    /* number of locks to allocate to a volume */
    DEFAULT_REVIVE_BLOCKSIZE = 32768,			    /* size of block to transfer in one op */
};

/* device numbers */

/*
 *  31 30   28  27                  20  19 18    16  15                 8    7                   0
 * |-----------------------------------------------------------------------------------------------|
 * |X |  Type  |    Subdisk number     | X| Plex   |      Major number     |  volume number        |
 * |-----------------------------------------------------------------------------------------------|
 *
 *    0x2                 03                 1           19                      06
 */
struct devcode {
/* CARE.  These fields assume a big-endian word.  On a
 * little-endian system, they're the wrong way around */
    unsigned volume:8;					    /* up to 256 volumes */
    unsigned major:8;					    /* this is where the major number fits */
    unsigned plex:3;					    /* up to 8 plexes per volume */
    unsigned unused:1;					    /* up for grabs */
    unsigned sd:8;					    /* up to 256 subdisks per plex */
    unsigned type:3;					    /* type of object */
    /* type field
       VINUM_VOLUME = 0,
       VINUM_PLEX = 1,
       VINUM_SUBDISK = 2,
       VINUM_DRIVE = 3,
       VINUM_SUPERDEV = 4, */
    unsigned signbit:1;					    /* to make 32 bits */
};

#define VINUM_DIR   "/dev/vinum"
#define VINUM_RDIR   "/dev/rvinum"
#define VINUM_SUPERDEV_NAME VINUM_DIR"/control"
#define MAXDRIVENAME   32				    /* maximum length of a device name */
#define MAXSDNAME      64				    /* maximum length of a subdisk name */
#define MAXPLEXNAME    64				    /* maximum length of a plex name */
#define MAXVOLNAME     64				    /* maximum length of a volume name */
#define MAXNAME        64				    /* maximum length of any name */
#define MAXVOLPLEX      8				    /* maximum number of plexes in a volume */

/* Flags for all objects.  Most of them only apply to
 * specific objects, but we have space for all in any
 * 32 bit flags word. */
enum objflags {
    VF_LOCKED = 1,					    /* somebody has locked access to this object */
    VF_LOCKING = 2,					    /* we want access to this object */
    VF_WRITETHROUGH = 8,				    /* volume: write through */
    VF_INITED = 0x10,					    /* unit has been initialized */
    VF_WLABEL = 0x20,					    /* label area is writable */
    VF_LABELLING = 0x40,				    /* unit is currently being labelled */
    VF_WANTED = 0x80,					    /* someone is waiting to obtain a lock */
    VF_RAW = 0x100,					    /* raw volume (no file system) */
    VF_LOADED = 0x200,					    /* module is loaded */
    VF_CONFIGURING = 0x400,				    /* somebody is changing the config */
    VF_WILL_CONFIGURE = 0x800,				    /* somebody wants to change the config */
    VF_CONFIG_INCOMPLETE = 0x1000,			    /* haven't finished changing the config */
    VF_CONFIG_SETUPSTATE = 0x2000,			    /* set a volume up if all plexes are empty */
    VF_READING_CONFIG = 0x4000,				    /* we're reading config database from disk */
    VF_KERNELOP = 0x8000,				    /* we're performing ops from kernel space */
};

/* Global configuration information for the vinum subsystem */
struct _vinum_conf {
    /* Pointers to vinum structures */
    struct drive *drive;
    struct sd *sd;
    struct plex *plex;
    struct volume *volume;

    /* the number allocated */
    int drives_allocated;
    int subdisks_allocated;
    int plexes_allocated;
    int volumes_allocated;

    /* and the number currently in use */
    int drives_used;
    int subdisks_used;
    int plexes_used;
    int volumes_used;

    int flags;
    int opencount;					    /* number of times we've been opened */
#if DEBUG
    int lastrq;
    struct buf *lastbuf;
#endif
};

/* Use these defines to simplify code */
#define DRIVE vinum_conf.drive
#define SD vinum_conf.sd
#define PLEX vinum_conf.plex
#define VOL vinum_conf.volume
#define VFLAGS vinum_conf.flags

/* Slice header

 * Vinum drives start with this structure:
 *
 *                                             Sector
 * |--------------------------------------|
 * |   PDP-11 memorial boot block         |      0
 * |--------------------------------------|
 * |   Disk label, maybe                  |      1
 * |--------------------------------------|
 * |   Slice definition  (vinum_hdr)      |      2
 * |--------------------------------------|
 * |                                      |
 * |   Configuration info, first copy     |      3
 * |                                      |
 * |--------------------------------------|
 * |                                      |
 * |   Configuration info, second copy    |      3 + size of config
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

/* hostname is 256 bytes long, but we don't need to shlep
 * multiple copies in vinum.  We use the host name just
 * to identify this system, and 32 bytes should be ample
 * for that purpose */
#define VINUMHOSTNAMELEN 32

struct vinum_label {
    char sysname[VINUMHOSTNAMELEN];			    /* system name at time of creation */
    char name[MAXDRIVENAME];				    /* our name of the drive */
    struct timeval date_of_birth;			    /* the time it was created */
    struct timeval last_update;				    /* and the time of last update */
    off_t drive_size;					    /* total size in bytes of the drive.
							    * This value includes the headers */
};

struct vinum_hdr {
    long long magic;					    /* we're long on magic numbers */
    /* XXX Get these right for big-endian */
#define VINUM_MAGIC    22322600044678729LL		    /* should be this */
#define VINUM_NOMAGIC  22322600044678990LL		    /* becomes this after obliteration */
    int config_length;					    /* size in bytes of each copy of the
							    * configuration info.
							    * This must be a multiple of the sector size. */

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

/*** Drive definitions ***/
/* A drive corresponds to a disk slice.  We use a different term to show
 * the difference in usage: it doesn't have to be a slice, and could
 * theroretically be a complete, unpartitioned disk */

struct drive {
    enum drivestate state;				    /* current state */
    int subdisks_allocated;				    /* number of entries in sd */
    int subdisks_used;					    /* and the number used */
    int blocksize;					    /* size of fs blocks */
    u_int64_t sectors_available;			    /* number of sectors still available */
    int secsperblock;
    int lasterror;					    /* last error on drive */
    int driveno;					    /* index of drive in vinum_conf */
    int opencount;					    /* number of up subdisks */
    u_int64_t reads;					    /* number of reads on this drive */
    u_int64_t writes;					    /* number of writes on this drive */
    u_int64_t bytes_read;				    /* number of bytes read */
    u_int64_t bytes_written;				    /* number of bytes written */
    dev_t dev;						    /* and device number */
    char devicename[MAXDRIVENAME];			    /* name of the slice it's on */
    struct vnode *vp;					    /* vnode pointer */
    struct proc *p;
    struct vinum_label label;				    /* and the label information */
    struct partinfo partinfo;				    /* partition information */
    int freelist_size;					    /* number of entries alloced in free list */
    int freelist_entries;				    /* number of entries used in free list */
    struct drive_freelist {				    /* sorted list of free space on drive */
	u_int64_t offset;
	long sectors;
    } *freelist;
};

/*** Subdisk definitions ***/

struct sd {
    enum sdstate state;					    /* state */
    /* offsets in blocks */
    int64_t driveoffset;				    /* offset on drive */
    int64_t plexoffset;					    /* offset in plex */
    u_int64_t sectors;					    /* and length in sectors */
    int plexno;						    /* index of plex, if it belongs */
    int driveno;					    /* index of the drive on which it is located */
    int sdno;						    /* our index in vinum_conf */
    int pid;						    /* pid of process which opened us */
    u_int64_t reads;					    /* number of reads on this subdisk */
    u_int64_t writes;					    /* number of writes on this subdisk */
    u_int64_t bytes_read;				    /* number of bytes read */
    u_int64_t bytes_written;				    /* number of bytes written */
    char name[MAXSDNAME];				    /* name of subdisk */
};

/*** Plex definitions ***/

/* kinds of plex organization */
enum plexorg {
    plex_disorg,					    /* disorganized */
    plex_concat,					    /* concatenated plex */
    plex_striped,					    /* striped plex */
    plex_raid5						    /* RAID5 plex */
};

/* Region in plex (either defective or unmapped) */
struct plexregion {
    u_int64_t offset;					    /* start of region */
    u_int64_t length;					    /* length */
};

struct plex {
    enum plexorg organization;				    /* Plex organization */
    enum plexstate state;				    /* and current state */
    u_int64_t length;					    /* total length of plex (max offset) */
    int flags;
    int stripesize;					    /* size of stripe or raid band, in sectors */
    int subdisks;					    /* number of associated subdisks */
    int subdisks_allocated;				    /* number of subdisks allocated space for */
    int *sdnos;						    /* list of component subdisks */
    int plexno;						    /* index of plex in vinum_conf */
    int volno;						    /* index of volume */
    int volplexno;					    /* number of plex in volume */
    int pid;						    /* pid of process which opened us */
    /* Lock information */
    int locks;						    /* number of locks used */
    int alloclocks;					    /* number of locks allocated */
    struct rangelock *lock;				    /* ranges of locked addresses */
    /* Statistics */
    u_int64_t reads;					    /* number of reads on this plex */
    u_int64_t writes;					    /* number of writes on this plex */
    u_int64_t bytes_read;				    /* number of bytes read */
    u_int64_t bytes_written;				    /* number of bytes written */
    u_int64_t multiblock;				    /* requests that needed more than one block */
    u_int64_t multistripe;				    /* requests that needed more than one stripe */
    /* revive parameters */
    u_int64_t revived;					    /* block number of current revive request */
    int revive_blocksize;				    /* revive block size (bytes) */
    int revive_interval;				    /* and time to wait between transfers */
    struct request *waitlist;				    /* list of requests waiting on revive op */
    /* geometry control */
    int defective_regions;				    /* number of regions which are defective */
    int defective_region_count;				    /* number of entries in defective_region */
    struct plexregion *defective_region;		    /* list of offset/length pairs: defective sds */
    int unmapped_regions;				    /* number of regions which are missing */
    int unmapped_region_count;				    /* number of entries in unmapped_region */
    struct plexregion *unmapped_region;			    /* list of offset/length pairs: missing sds */
    char name[MAXPLEXNAME];				    /* name of plex */
};

/*** Volume definitions ***/

#define MAXPLEX		8				    /* maximum number of plexes */


struct volume {
    enum volumestate state;				    /* current state */
    int plexes;						    /* number of plexes */
    int preferred_plex;					    /* plex to read from, -1 for round-robin */
    int last_plex_read;					    /* index of plex used for last read,
							    * for round-robin */
    dev_t devno;					    /* device number */
    int flags;						    /* status and configuration flags */
    int opencount;					    /* number of opens (all the same process) */
    int openflags;					    /* flags supplied to last open(2) */
    u_int64_t size;					    /* size of volume */
    int disk;						    /* disk index */
    int blocksize;					    /* logical block size */
    int active;						    /* number of outstanding requests active */
    int subops;						    /* and the number of suboperations */
    pid_t pid;						    /* pid of locker */
    /* Statistics */
    u_int64_t bytes_read;				    /* number of bytes read */
    u_int64_t bytes_written;				    /* number of bytes written */
    u_int64_t reads;					    /* number of reads on this volume */
    u_int64_t writes;					    /* number of writes on this volume */
    u_int64_t recovered_reads;				    /* reads recovered from another plex */
    /* Unlike subdisks in the plex, space for the plex pointers is static */
    int plex[MAXPLEX];					    /* index of plexes */
    char name[MAXVOLNAME];				    /* name of volume */
    struct disklabel label;				    /* for DIOCGPART */
};

/* Table expansion.  Expand table, which contains oldcount
 * entries of type element, by increment entries, and change
 * oldcount accordingly */
#define EXPAND(table, element, oldcount, increment)         \
{							    \
  expand_table ((void **) &table,			    \
		oldcount * sizeof (element),		    \
		(oldcount + increment) * sizeof (element) ); \
  oldcount += increment;				    \
  }

/* Information on vinum's memory usage */
struct meminfo {
    int mallocs;					    /* number of malloced blocks */
    int total_malloced;					    /* total amount malloced */
    int highwater;					    /* maximum number of mallocs */
    struct mc *malloced;				    /* pointer to kernel table */
};

struct mc {
    int seq;
    int size;
    short line;
    short flags;
#define ALLOC_KVA 1					    /* allocated via kva calls */
    int *databuf;					    /* really vm_object_t */
    caddr_t address;
    char file[16];
};

/* These enums are used by the state transition
 * routines.  They're in bit map format:
 *
 * Bit 0: Other plexes in the volume are down
 * Bit 1: Other plexes in the volume are up
 * Bit 2: The current plex is up
 * Maybe they should be local to
 * state.c */
enum volplexstate {
    volplex_onlyusdown = 0,				    /* we're the only plex, and we're down */
    volplex_alldown,					    /* 1: another plex is down, and so are we */
    volplex_otherup,					    /* 2: another plex is up */
    volplex_otherupdown,				    /* other plexes are up and down */
    volplex_onlyus,					    /* 4: we're up and alone */
    volplex_onlyusup,					    /* only we are up, others are down */
    volplex_allup,					    /* all plexes are up */
    volplex_someup					    /* some plexes are up, including us */
};

/* state map for plex */
enum sdstates {
    sd_emptystate = 1,
    sd_downstate = 2,					    /* found an SD which is down */
    sd_crashedstate = 4,				    /* found an SD which is crashed */
    sd_obsoletestate = 8,				    /* found an SD which is obsolete */
    sd_stalestate = 16,					    /* found an SD which is stale */
    sd_rebornstate = 32,				    /* found an SD which is reborn */
    sd_upstate = 64,					    /* found an SD which is up */
    sd_initstate = 128,					    /* found an SD which is init */
    sd_otherstate = 256					    /* found an SD in some other state */
};

/* This is really just a parameter to pass to
 * set_<foo>_state, but since it needs to be known
 * in the external definitions, we need to define
 * it here */
enum setstateflags {
    setstate_none = 0,					    /* no flags */
    setstate_force = 1,					    /* force the state change */
    setstate_configuring = 2,				    /* we're currently configuring, don't save */
    setstate_recursing = 4,				    /* we're called from another setstate function */
    setstate_norecurse = 8				    /* don't call other setstate functions */
};

#ifdef DEBUG
/* Debugging stuff */
#define DEBUG_ADDRESSES 1
#define DEBUG_NUMOUTPUT 2
#endif
