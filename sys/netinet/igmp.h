/*
 * Copyright (c) 1988 Stephen Deering.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Stephen Deering of Stanford University.
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
 *	@(#)igmp.h	8.1 (Berkeley) 6/10/93
 * $Id: igmp.h,v 1.3 1994/08/21 05:27:25 paul Exp $
 */

#ifndef _NETINET_IGMP_H_
#define _NETINET_IGMP_H_

/*
 * Internet Group Management Protocol (IGMP) definitions.
 *
 * Written by Steve Deering, Stanford, May 1988.
 *
 * MULTICAST 1.2
 */

/*
 * IGMP packet format.
 */
struct igmp {
	u_char		igmp_type;	/* version & type of IGMP message  */
	u_char		igmp_code;	/* unused, should be zero          */
	u_short		igmp_cksum;	/* IP-style checksum               */
	struct in_addr	igmp_group;	/* group address being reported    */
};					/*  (zero for queries)             */

#define IGMP_MINLEN		     8

#define IGMP_HOST_MEMBERSHIP_QUERY   0x11  /* message types, incl. version */
#define IGMP_HOST_MEMBERSHIP_REPORT  0x12
#define IGMP_DVMRP		     0x13  /* for experimental multicast   */
					   /*  routing protocol            */
#define IGMP_HOST_NEW_MEMBERSHIP_REPORT 0x16
#define IGMP_HOST_LEAVE_MESSAGE      0x17
#define IGMP_MTRACE		     0x1f  /* mcast traceroute messages    */
#define IGMP_MTRACE_RESP	     0x1e  /* traceroute resp. (to sender) */

#define IGMP_MAX_HOST_REPORT_DELAY   10    /* max delay for response to    */
#define IGMP_TIMER_SCALE     10	    /* denotes that the igmp->timer filed */
				    /*specifies time in 10th os seconds */

#define IGMP_DELAYING_MEMBER                     1
#define IGMP_IDLE_MEMBER                         2
#define IGMP_LAZY_MEMBER                         3 
#define IGMP_SLEEPING_MEMBER                     4 
#define IGMP_AWAKENING_MEMBER                    5 


#define IGMP_OLD_ROUTER                          0
#define IGMP_NEW_ROUTER                          1

#define IGMP_AGE_THRESHOLD		         540 

#ifdef IGMP_STATES
static char *tostate[]={"","DELAYING_MEMBER","IDLE","LAZY","SLEEPING",
			"AWAKENING" };
#endif /* IGMP_STATES */
#endif /* _NETINET_IGMP_H_ */
