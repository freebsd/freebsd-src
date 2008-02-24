/*-a
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
 *	from: @(#)igmp_var.h	8.1 (Berkeley) 7/19/93
 * $FreeBSD: src/sys/netinet/igmp_var.h,v 1.22 2007/06/12 16:24:53 bms Exp $
 */

#ifndef _NETINET_IGMP_VAR_H_
#define _NETINET_IGMP_VAR_H_

/*
 * Internet Group Management Protocol (IGMP),
 * implementation-specific definitions.
 *
 * Written by Steve Deering, Stanford, May 1988.
 *
 * MULTICAST Revision: 3.5.1.3
 */

struct igmpstat {
	u_int	igps_rcv_total;		/* total IGMP messages received */
	u_int	igps_rcv_tooshort;	/* received with too few bytes */
	u_int	igps_rcv_badsum;	/* received with bad checksum */
	u_int	igps_rcv_queries;	/* received membership queries */
	u_int	igps_rcv_badqueries;	/* received invalid queries */
	u_int	igps_rcv_reports;	/* received membership reports */
	u_int	igps_rcv_badreports;	/* received invalid reports */
	u_int	igps_rcv_ourreports;	/* received reports for our groups */
	u_int	igps_snd_reports;	/* sent membership reports */
	u_int	igps_rcv_toolong;	/* received with too many bytes */
};

#ifdef _KERNEL
#define IGMP_RANDOM_DELAY(X) (random() % (X) + 1)

/*
 * States for IGMPv2's leave processing
 */
#define IGMP_OTHERMEMBER			0
#define IGMP_IREPORTEDLAST			1

/*
 * State masks for IGMPv3
 */
#define IGMP_V3_NONEXISTENT			0x01
#define IGMP_V3_OTHERMEMBER			0x02
#define IGMP_V3_IREPORTEDLAST			0x04

/*
 * We must remember what version the subnet's querier is.
 * We conveniently use the IGMP message type for the proper
 * membership report to keep this state.
 */
#define IGMP_V1_ROUTER				IGMP_V1_MEMBERSHIP_REPORT
#define IGMP_V2_ROUTER				IGMP_V2_MEMBERSHIP_REPORT
#define IGMP_V3_ROUTER				IGMP_V3_MEMBERSHIP_REPORT

/*
 * Revert to new router if we haven't heard from an old router in
 * this amount of time.
 */
#define IGMP_AGE_THRESHOLD			540

/*
 * IGMPv3 protocol defaults
 */
#define IGMP_INIT_ROBVAR	2	/* Robustness */
#define IGMP_MAX_ROBVAR		7
#define IGMP_INIT_QRYINT	125	/* Querier's Query interval */
#define IGMP_MAX_QRYINT		255
#define IGMP_INIT_QRYRSP	10	/* Query Response interval */
#define IGMP_DEF_QRYMRT		10
#define IGMP_UNSOL_INT		1	/* Unsolicited Report interval */

/*
 * IGMPv3 report types
 */
#define IGMP_REPORT_MODE_IN	1	/* mode-is-include */
#define IGMP_REPORT_MODE_EX	2	/* mode-is-exclude */
#define IGMP_REPORT_TO_IN	3	/* change-to-include */
#define IGMP_REPORT_TO_EX	4	/* change-to-exclude */
#define IGMP_REPORT_ALLOW_NEW	5	/* allow-new-sources */
#define IGMP_REPORT_BLOCK_OLD	6	/* block-old-sources */

/*
 * Report types
 */
#define IGMP_MASK_CUR_STATE	0x01	/* Report current-state */
#define IGMP_MASK_ALLOW_NEW	0x02	/* Report source as allow-new */
#define IGMP_MASK_BLOCK_OLD	0x04	/* Report source as block-old */
#define IGMP_MASK_TO_IN		0x08	/* Report source as to_in */
#define IGMP_MASK_TO_EX		0x10	/* Report source as to_ex */
#define IGMP_MASK_STATE_T1	0x20	/* State at T1 */
#define IGMP_MASK_STATE_T2	0x40	/* State at T2 */
#define IGMP_MASK_IF_STATE	0x80	/* Report current-state per interface */

#define IGMP_MASK_STATE_TX	(IGMP_MASK_STATE_T1 | IGMP_MASK_STATE_T2)
#define IGMP_MASK_PENDING	(IGMP_MASK_CUR_STATE |			\
				 IGMP_MASK_ALLOW_NEW |			\
				 IGMP_MASK_BLOCK_OLD)

/*
 * List identifiers
 */
#define IGMP_EXCLUDE_LIST	1	/* exclude list used to tag report */
#define IGMP_INCLUDE_LIST	2	/* include list used to tag report */
#define IGMP_RECORDED_LIST	3	/* recorded list used to tag report */

void	igmp_init(void);
void	igmp_input(struct mbuf *, int);
void	igmp_joingroup(struct in_multi *);
void	igmp_leavegroup(struct in_multi *);
void	igmp_fasttimo(void);
void	igmp_slowtimo(void);

SYSCTL_DECL(_net_inet_igmp);

#endif

/*
 * Names for IGMP sysctl objects
 */
#define IGMPCTL_STATS		1	/* statistics (read-only) */
#define IGMPCTL_MAXID		2

#define IGMPCTL_NAMES { \
	{ 0, 0 }, \
	{ "stats", CTLTYPE_STRUCT } \
}
#endif
