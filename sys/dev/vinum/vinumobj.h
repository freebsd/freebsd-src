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
 * $Id: vinumobj.h,v 1.2 2001/05/23 23:04:18 grog Exp grog $
 * $FreeBSD$
 */

/*
 * Definitions of Vinum objects: drive, subdisk, plex and volume.
 * This file is included both by userland programs and by kernel code.
 * The userland structures are a subset of the kernel structures, and
 * all userland fields are at the beginning, so that a simple copy in
 * the length of the userland structure will be sufficient.  In order
 * to perform this copy, vinumioctl must know both structures, so it
 * includes this file again with _KERNEL reset.
 */

#ifndef _KERNEL
/*
 * Flags for all objects.  Most of them only apply
 * to specific objects, but we currently have
 * space for all in any 32 bit flags word.
 */
enum objflags {
    VF_LOCKED = 1,					    /* somebody has locked access to this object */
    VF_LOCKING = 2,					    /* we want access to this object */
    VF_OPEN = 4,					    /* object has openers */
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
    VF_FORCECONFIG = 0x8000,				    /* configure drives even with different names */
    VF_NEWBORN = 0x10000,				    /* for objects: we've just created it */
    VF_CONFIGURED = 0x20000,				    /* for drives: we read the config */
    VF_STOPPING = 0x40000,				    /* for vinum_conf: stop on last close */
    VF_DAEMONOPEN = 0x80000,				    /* the daemon has us open (only superdev) */
    VF_CREATED = 0x100000,				    /* for volumes: freshly created, more then new */
    VF_HOTSPARE = 0x200000,				    /* for drives: use as hot spare */
    VF_RETRYERRORS = 0x400000,				    /* don't down subdisks on I/O errors */
    VF_HASDEBUG = 0x800000,				    /* set if we support debug */
};

#endif

/* Global configuration information for the vinum subsystem */
#ifdef _KERNEL
struct _vinum_conf
#else
struct __vinum_conf
#endif
{
    int version;					    /* version of structures */
#ifdef _KERNEL
    /* Pointers to vinum structures */
    struct drive *drive;
    struct sd *sd;
    struct plex *plex;
    struct volume *volume;
#else
    /* Pointers to vinum structures */
    struct _drive *drive;
    struct _sd *sd;
    struct _plex *plex;
    struct _volume *volume;
#endif

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

    int flags;						    /* see above */

#define VINUM_MAXACTIVE  30000				    /* maximum number of active requests */
    int active;						    /* current number of requests outstanding */
    int maxactive;					    /* maximum number of requests ever outstanding */
#ifdef _KERNEL
#ifdef VINUMDEBUG
    struct request *lastrq;
    struct buf *lastbuf;
#endif
#endif
};

/* Use these defines to simplify code */
#define DRIVE vinum_conf.drive
#define SD vinum_conf.sd
#define PLEX vinum_conf.plex
#define VOL vinum_conf.volume
#define VFLAGS vinum_conf.flags

/*
 * A drive corresponds to a disk slice.  We use a different term to show
 * the difference in usage: it doesn't have to be a slice, and could
 * theoretically be a complete, unpartitioned disk
 */

#ifdef _KERNEL
struct drive
#else
struct _drive
#endif
{
    char devicename[MAXDRIVENAME];			    /* name of the slice it's on */
    struct vinum_label label;				    /* and the label information */
    enum drivestate state;				    /* current state */
    int flags;						    /* flags */
    int subdisks_allocated;				    /* number of entries in sd */
    int subdisks_used;					    /* and the number used */
    int blocksize;					    /* size of fs blocks */
    int pid;						    /* of locker */
    u_int64_t sectors_available;			    /* number of sectors still available */
    int secsperblock;
    int lasterror;					    /* last error on drive */
    int driveno;					    /* index of drive in vinum_conf */
    int opencount;					    /* number of up subdisks */
    u_int64_t reads;					    /* number of reads on this drive */
    u_int64_t writes;					    /* number of writes on this drive */
    u_int64_t bytes_read;				    /* number of bytes read */
    u_int64_t bytes_written;				    /* number of bytes written */
#define DRIVE_MAXACTIVE  30000				    /* maximum number of active requests */
    int active;						    /* current number of requests outstanding */
    int maxactive;					    /* maximum number of requests ever outstanding */
    int freelist_size;					    /* number of entries alloced in free list */
    int freelist_entries;				    /* number of entries used in free list */
    struct drive_freelist *freelist;			    /* sorted list of free space on drive */
#ifdef _KERNEL
    u_int sectorsize;
    off_t mediasize;
    dev_t dev;						    /* device information */
#ifdef VINUMDEBUG
    char lockfilename[16];				    /* name of file from which we were locked */
    int lockline;					    /* and the line number */
#endif
#endif
};

#ifdef _KERNEL
struct sd
#else
struct _sd
#endif
{
    char name[MAXSDNAME];				    /* name of subdisk */
    enum sdstate state;					    /* state */
    int flags;
    int lasterror;					    /* last error occurred */
    /* offsets in blocks */
    int64_t driveoffset;				    /* offset on drive */
    /*
     * plexoffset is the offset from the beginning
     * of the plex to the very first part of the
     * subdisk, in sectors.  For striped, RAID-4 and
     * RAID-5 plexes, only the first stripe is
     * located at this offset
     */
    int64_t plexoffset;					    /* offset in plex */
    u_int64_t sectors;					    /* and length in sectors */
    int plexno;						    /* index of plex, if it belongs */
    int driveno;					    /* index of the drive on which it is located */
    int sdno;						    /* our index in vinum_conf */
    int plexsdno;					    /* and our number in our plex */
    /* (undefined if no plex) */
    u_int64_t reads;					    /* number of reads on this subdisk */
    u_int64_t writes;					    /* number of writes on this subdisk */
    u_int64_t bytes_read;				    /* number of bytes read */
    u_int64_t bytes_written;				    /* number of bytes written */
    /* revive parameters */
    u_int64_t revived;					    /* block number of current revive request */
    int revive_blocksize;				    /* revive block size (bytes) */
    int revive_interval;				    /* and time to wait between transfers */
    pid_t reviver;					    /* PID of reviving process */
    /* init parameters */
    u_int64_t initialized;				    /* block number of current init request */
    int init_blocksize;					    /* init block size (bytes) */
    int init_interval;					    /* and time to wait between transfers */
#ifdef _KERNEL
    struct request *waitlist;				    /* list of requests waiting on revive op */
    dev_t dev;						    /* associated device */
#endif
};

#ifdef _KERNEL
struct plex
#else
struct _plex
#endif
{
    enum plexorg organization;				    /* Plex organization */
    enum plexstate state;				    /* and current state */
    u_int64_t length;					    /* total length of plex (sectors) */
    int flags;
    int stripesize;					    /* size of stripe or raid band, in sectors */
    int subdisks;					    /* number of associated subdisks */
    int subdisks_allocated;				    /* number of subdisks allocated space for */
    int *sdnos;						    /* list of component subdisks */
    int plexno;						    /* index of plex in vinum_conf */
    int volno;						    /* index of volume */
    int volplexno;					    /* number of plex in volume */
    /* Statistics */
    u_int64_t reads;					    /* number of reads on this plex */
    u_int64_t writes;					    /* number of writes on this plex */
    u_int64_t bytes_read;				    /* number of bytes read */
    u_int64_t bytes_written;				    /* number of bytes written */
    u_int64_t recovered_reads;				    /* number of recovered read operations */
    u_int64_t degraded_writes;				    /* number of degraded writes */
    u_int64_t parityless_writes;			    /* number of parityless writes */
    u_int64_t multiblock;				    /* requests that needed more than one block */
    u_int64_t multistripe;				    /* requests that needed more than one stripe */
    int sddowncount;					    /* number of subdisks down */
    /* Lock information */
    int usedlocks;					    /* number currently in use */
    int lockwaits;					    /* and number of waits for locks */
    off_t checkblock;					    /* block number for parity op */
    char name[MAXPLEXNAME];				    /* name of plex */
#ifdef _KERNEL
    struct rangelock *lock;				    /* ranges of locked addresses */
    struct mtx lockmtx;
    dev_t dev;						    /* associated device */
#endif
};

#ifdef _KERNEL
struct volume
#else
struct _volume
#endif
{
    char name[MAXVOLNAME];				    /* name of volume */
    enum volumestate state;				    /* current state */
    int plexes;						    /* number of plexes */
    int preferred_plex;					    /* plex to read from, -1 for round-robin */
    /*
     * index of plex used for last read, for
     * round-robin.
     */
    int last_plex_read;
    int volno;						    /* volume number */
    int flags;						    /* status and configuration flags */
    int openflags;					    /* flags supplied to last open(2) */
    u_int64_t size;					    /* size of volume */
    int blocksize;					    /* logical block size */
    int active;						    /* number of outstanding requests active */
    int subops;						    /* and the number of suboperations */
    /* Statistics */
    u_int64_t bytes_read;				    /* number of bytes read */
    u_int64_t bytes_written;				    /* number of bytes written */
    u_int64_t reads;					    /* number of reads on this volume */
    u_int64_t writes;					    /* number of writes on this volume */
    u_int64_t recovered_reads;				    /* reads recovered from another plex */
    /*
     * Unlike subdisks in the plex, space for the
     * plex pointers is static.
     */
    int plex[MAXPLEX];					    /* index of plexes */
#ifdef _KERNEL
    struct disklabel label;				    /* for DIOCGPART */
    dev_t dev;						    /* associated device */
#endif
};
