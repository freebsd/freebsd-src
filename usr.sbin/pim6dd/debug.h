/*
 *  Copyright (c) 1998 by the University of Southern California.
 *  All rights reserved.
 *
 *  Permission to use, copy, modify, and distribute this software and
 *  its documentation in source and binary forms for lawful
 *  purposes and without fee is hereby granted, provided
 *  that the above copyright notice appear in all copies and that both
 *  the copyright notice and this permission notice appear in supporting
 *  documentation, and that any documentation, advertising materials,
 *  and other materials related to such distribution and use acknowledge
 *  that the software was developed by the University of Southern
 *  California and/or Information Sciences Institute.
 *  The name of the University of Southern California may not
 *  be used to endorse or promote products derived from this software
 *  without specific prior written permission.
 *
 *  THE UNIVERSITY OF SOUTHERN CALIFORNIA DOES NOT MAKE ANY REPRESENTATIONS
 *  ABOUT THE SUITABILITY OF THIS SOFTWARE FOR ANY PURPOSE.  THIS SOFTWARE IS
 *  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
 *  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, TITLE, AND 
 *  NON-INFRINGEMENT.
 *
 *  IN NO EVENT SHALL USC, OR ANY OTHER CONTRIBUTOR BE LIABLE FOR ANY
 *  SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES, WHETHER IN CONTRACT,
 *  TORT, OR OTHER FORM OF ACTION, ARISING OUT OF OR IN CONNECTION WITH,
 *  THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *  Other copyrights might apply to parts of this software and are so
 *  noted when applicable.
 */
/*
 *  Questions concerning this software should be directed to 
 *  Pavlin Ivanov Radoslavov (pavlin@catarina.usc.edu)
 *
 *  $Id: debug.h,v 1.1.1.1 1999/08/08 23:30:52 itojun Exp $
 */
/*
 * Part of this program has been derived from mrouted.
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE.mrouted".
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 * $FreeBSD: src/usr.sbin/pim6dd/debug.h,v 1.1.2.1 2000/07/15 07:36:29 kris Exp $
 */

extern unsigned long	debug;
extern int log_nmsgs;
#define IF_DEBUG(l)	if (debug && debug & (l))

#define LOG_MAX_MSGS	20	/* if > 20/minute then shut up for a while */
#define LOG_SHUT_UP	600	/* shut up for 10 minutes */


/* Debug values definition */
/* DVMRP reserved for future use */
#define DEBUG_DVMRP_PRUNE     0x00000001
#define DEBUG_DVMRP_ROUTE     0x00000002
#define DEBUG_DVMRP_PEER      0x00000004
#define DEBUG_DVMRP_TIMER     0x00000008
#define DEBUG_DVMRP_DETAIL    0x01000000
#define DEBUG_DVMRP           ( DEBUG_DVMRP_PRUNE | DEBUG_DVMRP_ROUTE | \
				DEBUG_DVMRP_PEER )

/* MLD related */
#define DEBUG_MLD_PROTO      0x00000010
#define DEBUG_MLD_TIMER      0x00000020
#define DEBUG_MLD_MEMBER     0x00000040
#define DEBUG_MEMBER          DEBUG_MLD_MEMBER
#define DEBUG_MLD             ( DEBUG_MLD_PROTO | DEBUG_MLD_TIMER | \
				DEBUG_MLD_MEMBER )

/* Misc */
#define DEBUG_TRACE           0x00000080
#define DEBUG_TIMEOUT         0x00000100
#define DEBUG_PKT             0x00000200


/* Kernel related */
#define DEBUG_IF              0x00000400
#define DEBUG_KERN            0x00000800
#define DEBUG_MFC             0x00001000
#define DEBUG_RSRR            0x00002000

/* PIM related */
#define DEBUG_PIM_GRAFT       0x02000000
#define DEBUG_PIM_HELLO       0x00004000
#define DEBUG_PIM_REGISTER    0x00008000
#define DEBUG_PIM_JOIN_PRUNE  0x00010000
#define DEBUG_PIM_BOOTSTRAP   0x00020000
#define DEBUG_PIM_ASSERT      0x00040000
#define DEBUG_PIM_CAND_RP     0x00080000
#define DEBUG_PIM_MRT         0x00100000
#define DEBUG_PIM_TIMER       0x00200000
#define DEBUG_PIM_RPF         0x00400000
#define DEBUG_RPF             DEBUG_PIM_RPF
#define DEBUG_PIM_DETAIL      0x00800000
#define DEBUG_PIM             ( DEBUG_PIM_HELLO | DEBUG_PIM_REGISTER | \
				DEBUG_PIM_JOIN_PRUNE | DEBUG_PIM_BOOTSTRAP | \
				DEBUG_PIM_ASSERT | DEBUG_PIM_CAND_RP | \
				DEBUG_PIM_MRT | DEBUG_PIM_TIMER | \
				DEBUG_PIM_RPF | DEBUG_PIM_GRAFT ) 

#define DEBUG_MRT             ( DEBUG_DVMRP_ROUTE | DEBUG_PIM_MRT )
#define DEBUG_NEIGHBORS       ( DEBUG_DVMRP_PEER | DEBUG_PIM_HELLO )
#define DEBUG_TIMER           ( DEBUG_MLD_TIMER | DEBUG_DVMRP_TIMER | \
				DEBUG_PIM_TIMER )
#define DEBUG_ASSERT          ( DEBUG_PIM_ASSERT )
#define DEBUG_ALL             0xffffffff


#define DEBUG_DEFAULT   0xffffffff/*  default if "-d" given without value */






