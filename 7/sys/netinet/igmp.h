/*-
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
 * $FreeBSD$
 */

#ifndef _NETINET_IGMP_H_
#define _NETINET_IGMP_H_

/*
 * Internet Group Management Protocol (IGMP) definitions.
 *
 * Written by Steve Deering, Stanford, May 1988.
 *
 * MULTICAST Revision: 3.5.1.2
 */

/*
 * IGMP packet format.
 */
struct igmp {
	u_char		igmp_type;	/* version & type of IGMP message  */
	u_char		igmp_code;	/* subtype for routing msgs        */
	u_short		igmp_cksum;	/* IP-style checksum               */
	struct in_addr	igmp_group;	/* group address being reported    */
};					/*  (zero for queries)             */

struct igmpv3 {
	u_char		igmp_type;	/* version & type of IGMP message  */
	u_char		igmp_code;	/* subtype for routing msgs        */
	u_short		igmp_cksum;	/* IP-style checksum               */
	struct in_addr	igmp_group;	/* group address being reported    */
					/*  (zero for queries)             */
	u_char		igmp_misc;	/* reserved/suppress/robustness    */
	u_char		igmp_qqi;	/* querier's query interval        */
	u_short		igmp_numsrc;	/* number of sources               */
	/*struct in_addr	igmp_sources[1];*/ /* source addresses */
};

struct igmp_grouprec {
	u_char		ig_type;	/* record type */
	u_char		ig_datalen;	/* length of auxiliary data */
	u_short		ig_numsrc;	/* number of sources */
	struct in_addr	ig_group;	/* group address being reported */
	/*struct in_addr	ig_sources[1];*/ /* source addresses */
};

struct igmp_report {
	u_char		ir_type;	/* record type */
	u_char		ir_rsv1;	/* reserved */
	u_short		ir_cksum;	/* checksum */
	u_short		ir_rsv2;	/* reserved */
	u_short		ir_numgrps;	/* number of group records */
	struct		igmp_grouprec ir_groups[1];	/* group records */
};

#define IGMP_MINLEN			8
#define IGMP_HDRLEN			8
#define IGMP_GRPREC_HDRLEN		8
#define IGMP_PREPEND			0

#if 0
#define IGMP_QRV(pigmp)			((pigmp)->igmp_misc & (0x07)) /* XXX */
#define IGMP_MAXSOURCES(len)		(((len) - 12) >> 2) /* XXX */
#endif

/*
 * Message types, including version number.
 */
#define IGMP_MEMBERSHIP_QUERY		0x11	/* membership query         */
#define IGMP_V1_MEMBERSHIP_REPORT	0x12	/* Ver. 1 membership report */
#define IGMP_V2_MEMBERSHIP_REPORT	0x16	/* Ver. 2 membership report */
#define IGMP_V2_LEAVE_GROUP		0x17	/* Leave-group message	    */

#define IGMP_DVMRP			0x13	/* DVMRP routing message    */
#define IGMP_PIM			0x14	/* PIM routing message	    */

#define IGMP_MTRACE_RESP		0x1e  /* traceroute resp.(to sender)*/
#define IGMP_MTRACE			0x1f  /* mcast traceroute messages  */

#define IGMP_V3_MEMBERSHIP_REPORT	0x22	/* Ver. 3 membership report */

#define IGMP_MAX_HOST_REPORT_DELAY   10    /* max delay for response to     */
					   /*  query (in seconds) according */
					   /*  to RFC1112                   */


#define IGMP_TIMER_SCALE     10		/* denotes that the igmp code field */
					/* specifies time in 10th of seconds*/

/*
 * The following four defininitions are for backwards compatibility.
 * They should be removed as soon as all applications are updated to
 * use the new constant names.
 */
#define IGMP_HOST_MEMBERSHIP_QUERY	IGMP_MEMBERSHIP_QUERY
#define IGMP_HOST_MEMBERSHIP_REPORT	IGMP_V1_MEMBERSHIP_REPORT
#define IGMP_HOST_NEW_MEMBERSHIP_REPORT	IGMP_V2_MEMBERSHIP_REPORT
#define IGMP_HOST_LEAVE_MESSAGE		IGMP_V2_LEAVE_GROUP

#endif /* _NETINET_IGMP_H_ */
