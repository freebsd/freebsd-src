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
 * $Id: request.h,v 1.10 1998/08/03 07:15:26 grog Exp grog $
 */

/* Information needed to set up a transfer */

/* struct buf is surprisingly big (about 300
 * bytes), and it's part of the request, so this
 * value is really important.  Most requests
 * don't need more than 2 subrequests per
 * plex. The table is automatically extended if
 * this value is too small. */
#define RQELTS 2					    /* default of 2 requests per transfer */

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
#if DEBUG
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

/* Describe one low-level request, part
 * of a high-level request.  This is an
 * extended struct buf buffer, and the first
 * element *must* be a struct buf.  We pass this structure
 * to the I/O routines instead of a struct buf in oder
 * to be able to locate the high-level request when it
 * completes.
 *
 * All offsets and lengths are in "blocks", i.e. sectors */
struct rqelement {
    struct buf b;					    /* buf structure */
    struct rqgroup *rqg;				    /* pointer to our group */
    /* Information about the transfer */
    daddr_t sdoffset;					    /* offset in subdisk */
    int useroffset;					    /* offset in user buffer of normal data */
    /* dataoffset and datalen refer to "individual"
     * data transfers (normal read, parityless write)
     * and also degraded write.
     *
     * groupoffset and grouplen refer to the other
     * "group" operations (normal write, recovery read)
     * Both the offsets are relative to the start of the
     * local buffer */
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

/* A group of requests built to satisfy a certain
 * component of a user request */
struct rqgroup {
    struct rqgroup *next;				    /* pointer to next group */
    struct request *rq;					    /* pointer to the request */
    short count;					    /* number of requests in this group */
    short active;					    /* and number active */
    short plexno;					    /* index of plex */
    int badsdno;					    /* index of bad subdisk or -1 */
    enum xferinfo flags;				    /* description of transfer */
    struct rqelement rqe[0];				    /* and the elements of this request */
};

/* Describe one high-level request and the
 * work we have to do to satisfy it */
struct request {
    struct buf *bp;					    /* pointer to the high-level request */
    int flags;
    union {
	int volno;					    /* volume index */
	int plexno;					    /* or plex index */
    } volplex;
    int error;						    /* current error indication */
    short isplex;					    /* set if this is a plex request */
    short active;					    /* number of subrequests still active */
    struct rqgroup *rqg;				    /* pointer to the first group of requests */
    struct rqgroup *lrqg;				    /* and to the first group of requests */
    struct request *next;				    /* link of waiting requests */
};

/* Extended buffer header for subdisk I/O.  Includes
 * a pointer to the user I/O request. */
struct sdbuf {
    struct buf b;					    /* our buffer */
    struct buf *bp;					    /* and pointer to parent */
    short driveno;					    /* drive index */
    short sdno;						    /* and subdisk index */
};

/* Values returned by rqe and friends.
 * Be careful with these: they are in order of increasing
 * seriousness.  Some routines check for > REQUEST_RECOVERED
 * to indicate a completely failed request. */
enum requeststatus {
    REQUEST_OK,						    /* request built OK */
    REQUEST_RECOVERED,					    /* request OK, but involves RAID5 recovery */
    REQUEST_EOF,					    /* request failed: outside plex */
    REQUEST_DOWN,					    /* request failed: subdisk down  */
    REQUEST_ENOMEM					    /* ran out of memory */
};
