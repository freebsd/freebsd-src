/*
 * Copyright (c) 1985, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 1995 John Hay.  All rights reserved.
 *
 * This file includes significant work done at Cornell University by
 * Bill Nesheim.  That work included by permission.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)protocol.h	8.1 (Berkeley) 6/5/93
 *
 * $FreeBSD$
 */

/*
 * IPX Routing Information Protocol
 *
 */

struct netinfo {
	union ipx_net	rip_dst;		/* destination net */
	u_short		rip_metric;		/* cost of route */
	u_short		rip_ticks;		/* cost of route */
};

struct rip {
	u_short	rip_cmd;		/* request/response */
	struct netinfo rip_nets[1];	/* variable length */
};
 
/*
 * Packet types.
 */
#define	RIPCMD_REQUEST		1	/* want info */
#define	RIPCMD_RESPONSE		2	/* responding to request */

#define	RIPCMD_MAX		3
#ifdef RIPCMDS
char *ripcmds[RIPCMD_MAX] =
  { "#0", "REQUEST", "RESPONSE" };
#endif

#define	HOPCNT_INFINITY		16		/* per IPX */
#define	DSTNETS_ALL		0xffffffff	/* per IPX */
#define	MAXRXPACKETSIZE		1500		/* max rx broadcast size */
#define MAXRIPNETS		50		/* max nets in tx packet */

extern union ipx_net ipx_anynet;
extern union ipx_net ipx_zeronet;

/*
 * Timer values used in managing the routing table.
 * Every update forces an entry's timer to be reset.  After
 * EXPIRE_TIME without updates, the entry is marked invalid,
 * but held onto until GARBAGE_TIME so that others may
 * see it "be deleted".
 */
#define	TIMER_RATE		30	/* alarm clocks every 30 seconds */

#define	SUPPLY_INTERVAL		30	/* time to supply tables */
#define	RIP_INTERVAL		60	/* time to supply rip tables */

#define	EXPIRE_TIME		180	/* time to mark entry invalid */
#define	GARBAGE_TIME		240	/* time to garbage collect */
