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
 * $Id: request.h,v 1.17 1999/12/27 02:18:05 grog Exp grog $
 * $FreeBSD: src/sys/dev/vinum/request.h,v 1.17 2000/01/05 06:01:34 grog Exp $
 */

/* Information needed to set up a transfer */

enum xferinfo {
    XFR_NORMAL_READ = 1,
    XFR_NORMAL_WRITE = 2,				    /* write request in normal mode */
    XFR_RECOVERY_READ = 4,
    XFR_DEGRADED_WRITE = 8,
    XFR_PARITYLESS_WRITE = 0x10,
    XFR_NO_PARITY_STRIPE = 0x20,			    /* parity stripe is not available */
    XFR_DATA_BLOCK = 0x40,				    /* data block in request */
    XFR_PARITY_BLOCK = 0x80,				    /* parity block in request */
    XFR_BAD_SUBDISK = 0x100,				    /* this subdisk is dead */
    XFR_MALLOCED = 0x200,				    /* this buffer is malloced */
#if VINUMDEBUG
    XFR_PHASE2 = 0x800,					    /* documentation only: 2nd phase write */
#endif
    XFR_REVIVECONFLICT = 0x1000,			    /* possible conflict with a revive operation */
    /* operations that need a parity block */
    XFR_PARITYOP = (XFR_NORMAL_WRITE | XFR_RECOVERY_READ | XFR_DEGRADED_WRITE),
    /* operations that use the group parameters */
    XFR_GROUPOP = (XFR_DEGRADED_WRITE | XFR_RECOVERY_READ),
    /* operations that that use the data parameters */
    XFR_DATAOP = (XFR_NORMAL_READ | XFR_NORMAL_WRITE | XFR_PARITYLESS_WRITE),
    /* operations requiring read before write */
    XFR_RBW = (XFR_NORMAL_WRITE | XFR_DEGRADED_WRITE),
    /* operations that need a malloced buffer */
    XFR_NEEDS_MALLOC = (XFR_NORMAL_WRITE | XFR_RECOVERY_READ | XFR_DEGRADED_WRITE)
};

/*
 * Describe one low-level request, part of a
 * high-level request.  This is an extended
 * struct buf buffer, and the first element
 * *must* be a struct buf.  We pass this
 * structure to the I/O routines instead of a
 * struct buf in order to be able to locate the
 * high-level request when it completes.
 *
 * All offsets and lengths are in sectors.
 */

struct rqelement {
    struct buf b;					    /* buf structure */
    struct rqgroup *rqg;				    /* pointer to our group */
    /* Information about the transfer */
    daddr_t sdoffset;					    /* offset in subdisk */
    int useroffset;					    /* offset in user buffer of normal data */
    /*
     * dataoffset and datalen refer to "individual" data
     * transfers which involve only this drive (normal read,
     * parityless write) and also degraded write.
     *
     * groupoffset and grouplen refer to the other "group"
     * operations (normal write, recovery read) which involve
     * more than one drive.  Both the offsets are relative to
     * the start of the local buffer.
     */
    int dataoffset;					    /* offset in buffer of the normal data */
    int groupoffset;					    /* offset in buffer of group data */
    short datalen;					    /* length of normal data (sectors) */
    short grouplen;					    /* length of group data (sectors) */
    short buflen;					    /* total buffer length to allocate */
    short flags;					    /* really enum xferinfo (see above) */
    /* Ways to find other components */
    short sdno;						    /* subdisk number */
    short driveno;					    /* drive number */
};

/*
 * A group of requests built to satisfy an I/O
 * transfer on a single plex.
 */
struct rqgroup {
    struct rqgroup *next;				    /* pointer to next group */
    struct request *rq;					    /* pointer to the request */
    short count;					    /* number of requests in this group */
    short active;					    /* and number active */
    short plexno;					    /* index of plex */
    int badsdno;					    /* index of bad subdisk or -1 */
    enum xferinfo flags;				    /* description of transfer */
    struct rangelock *lock;				    /* lock for this transfer */
    daddr_t lockbase;					    /* and lock address */
    struct rqelement rqe[0];				    /* and the elements of this request */
};

/*
 * Describe one high-level request and the
 * work we have to do to satisfy it.
 */
struct request {
    struct buf *bp;					    /* pointer to the high-level request */
    enum xferinfo flags;
    union {
	int volno;					    /* volume index */
	int plexno;					    /* or plex index */
    } volplex;
    int error;						    /* current error indication */
    int sdno;						    /* reviving subdisk (XFR_REVIVECONFLICT) */
    short isplex;					    /* set if this is a plex request */
    short active;					    /* number of subrequests still active */
    struct rqgroup *rqg;				    /* pointer to the first group of requests */
    struct rqgroup *lrqg;				    /* and to the last group of requests */
    struct request *next;				    /* link of waiting requests */
};

/*
 * Extended buffer header for subdisk I/O.  Includes
 * a pointer to the user I/O request.
 */
struct sdbuf {
    struct buf b;					    /* our buffer */
    struct buf *bp;					    /* and pointer to parent */
    short driveno;					    /* drive index */
    short sdno;						    /* and subdisk index */
};

/*
 * Values returned by rqe and friends.  Be careful
 * with these: they are in order of increasing
 * seriousness.  Some routines check for
 * > REQUEST_RECOVERED to indicate a failed request. XXX
 */
enum requeststatus {
    REQUEST_OK,						    /* request built OK */
    REQUEST_RECOVERED,					    /* request OK, but involves RAID5 recovery */
    REQUEST_DEGRADED,					    /* parts of request failed */
    REQUEST_EOF,					    /* parts of request failed: outside plex */
    REQUEST_DOWN,					    /* all of request failed: subdisk(s) down */
    REQUEST_ENOMEM					    /* all of request failed: ran out of memory */
};

#ifdef VINUMDEBUG
/* Trace entry for request info (DEBUG_LASTREQS) */
enum rqinfo_type {
    loginfo_unused,					    /* never been used */
    loginfo_user_bp,					    /* this is the bp when strategy is called */
    loginfo_user_bpl,					    /* and this is the bp at launch time */
    loginfo_rqe,					    /* user RQE */
    loginfo_iodone,					    /* iodone */
    loginfo_raid5_data,					    /* write RAID-5 data block */
    loginfo_raid5_parity,				    /* write RAID-5 parity block */
    loginfo_sdio,					    /* subdisk I/O */
    loginfo_sdiol,					    /* subdisk I/O launch */
    loginfo_sdiodone,					    /* subdisk iodone */
    loginfo_lockwait,					    /* wait for range lock */
    loginfo_lock,					    /* lock range */
    loginfo_unlock,					    /* unlock range */
};

union rqinfou {						    /* info to pass to logrq */
    struct buf *bp;
    struct rqelement *rqe;				    /* address of request, for correlation */
    struct rangelock *lockinfo;
};

struct rqinfo {
    enum rqinfo_type type;				    /* kind of event */
    struct timeval timestamp;				    /* time it happened */
    struct buf *bp;					    /* point to user buffer */
    int devmajor;					    /* major and minor device info */
    int devminor;
    union {
	struct buf b;					    /* yup, the *whole* buffer header */
	struct rqelement rqe;				    /* and the whole rqe */
	struct rangelock lockinfo;
    } info;
};

#define RQINFO_SIZE 128					    /* number of info slots in buffer */

void logrq(enum rqinfo_type type, union rqinfou info, struct buf *ubp);
#endif

/* Structures for the daemon */

/* types of request to the daemon */
enum daemonrq {
    daemonrq_none,					    /* dummy to catch bugs */
    daemonrq_ioerror,					    /* error occurred on I/O */
    daemonrq_saveconfig,				    /* save configuration */
    daemonrq_return,					    /* return to userland */
    daemonrq_ping,					    /* show sign of life */
    daemonrq_init,					    /* initialize a plex */
    daemonrq_revive,					    /* revive a subdisk */
    daemonrq_closedrive,				    /* close a drive */
};

/* info field for daemon requests */
union daemoninfo {					    /* and the request information */
    struct request *rq;					    /* for daemonrq_ioerror */
    struct sd *sd;					    /* for daemonrq_revive */
    struct plex *plex;					    /* for daemonrq_init */
    struct drive *drive;				    /* for daemonrq_closedrive */
    int nothing;					    /* for passing NULL */
};

struct daemonq {
    struct daemonq *next;				    /* pointer to next element in queue */
    enum daemonrq type;					    /* type of request */
    int privateinuse;					    /* private element, being used */
    union daemoninfo info;				    /* and the request information */
};

void queue_daemon_request(enum daemonrq type, union daemoninfo info);

extern int daemon_options;

enum daemon_option {
    daemon_verbose = 1,					    /* talk about what we're doing */
    daemon_stopped = 2,
    daemon_noupdate = 4,				    /* don't update the disk config, for recovery */
};

void freerq(struct request *rq);
void unlockrange(int plexno, struct rangelock *);
/* Local Variables: */
/* fill-column: 50 */
/* End: */
