/*
 * Copyright (C) 1999 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
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
 *  $Id: trace.h,v 1.2 1999/09/12 17:00:10 jinmei Exp $
 */
/*
 * Part of this program has been derived from mrouted.
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE.mrouted".
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 * $FreeBSD: src/usr.sbin/pim6dd/trace.h,v 1.1.2.1 2000/07/15 07:36:30 kris Exp $
 */


/*
 * The packet format for a traceroute request.
 */
struct tr6_query {
    struct in6_addr  tr_src;		/* traceroute source */
    struct in6_addr  tr_dst;		/* traceroute destination */
    struct in6_addr  tr_raddr;		/* traceroute response address */
#if defined(BYTE_ORDER) && (BYTE_ORDER == LITTLE_ENDIAN)
    struct {
	u_int32_t qid : 24;	/* traceroute query id */
	u_int32_t rhlim : 8;	/* traceroute response ttl */
    } q;
#else
    struct {
	u_int32_t rhlim : 8;	/* traceroute response ttl */
	u_int32_t qid : 24;	/* traceroute query id */
    } q;
#endif /* BYTE_ORDER */
};

#define tr_rhlim q.rhlim
#define tr_qid  q.qid

/*
 * Traceroute response format.  A traceroute response has a tr_query at the
 * beginning, followed by one tr_resp for each hop taken.
 */
struct tr6_resp {
	u_int32_t tr_qarr;	/* query arrival time */
#if 0
	struct in6_addr tr_inaddr; /* incoming interface address */
	struct in6_addr tr_outaddr; /* outgoing interface address */
#endif
	u_int32_t tr_inifid;	/* incoming interface identifier */
	u_int32_t tr_outifid;	/* outgoing interface identifier */
	struct in6_addr tr_lcladdr; /* router's address(must have largest scope) */
	struct in6_addr tr_rmtaddr; /* parent address in source tree */
	u_int32_t tr_vifin;	/* input packet count on interface */
	u_int32_t tr_vifout;	/* output packet count on interface */
	u_int32_t tr_pktcnt;	/* total incoming packets for src-grp */
	u_char  tr_rproto;	/* routing protocol deployed on router */
#if 0
	u_char  tr_fhlim;	/* hop limit required to forward on outvif */
#endif
	u_char	tr_flags;	/* flags */
	u_char  tr_plen;	/* prefix length for src addr */
	u_char  tr_rflags;	/* forwarding error codes */
};

/* defs within mtrace */
#define QUERY	1
#define RESP	2
#define QLEN	sizeof(struct tr6_query)
#define RLEN	sizeof(struct tr6_resp)

/* fields for tr_inifid and tr_outifid */
#define TR_NO_VIF	0xffffffff/* interface can't be determined */

/* fields for tr_rflags (forwarding error codes) */
#define TR_NO_ERR	0       /* No error */
#define TR_WRONG_IF	1       /* traceroute arrived on non-oif */
#define TR_PRUNED	2       /* router has sent a prune upstream */
#define TR_OPRUNED	3       /* stop forw. after request from next hop rtr*/
#define TR_SCOPED	4       /* group adm. scoped at this hop */
#define TR_NO_RTE	5       /* no route for the source */
#define TR_NO_LHR       6       /* not the last-hop router */
#define TR_NO_FWD	7       /* not forwarding for this (S,G). Reason = ? */
#define TR_RP           8       /* I am the RP/Core */
#define TR_IIF          9       /* request arrived on the iif */
#define TR_NO_MULTI     0x0a    /* multicast disabled on that interface */
#define TR_NO_SPACE	0x81    /* no space to insert responce data block */
#define TR_OLD_ROUTER	0x82    /* previous hop does not support traceroute */
#define TR_ADMIN_PROHIB 0x83    /* traceroute adm. prohibited */

/* fields for tr_flags */
#define TR_SUBNET_COUNT 0x80    /* pkt count for (S,G) is for source network */

/* fields for r_plen */
#define TR_GROUP_ONLY   0xff    /* forwarding solely on group state */

/* fields for packets count */
#define TR_CANT_COUNT   0xffffffff  /* no count can be reported */

/* fields for tr_rproto (routing protocol) */
#define PROTO_DVMRP	   1
#define PROTO_MOSPF	   2
#define PROTO_PIM	   3
#define PROTO_CBT 	   4
#define PROTO_PIM_SPECIAL  5
#define PROTO_PIM_STATIC   6
#define PROTO_DVMRP_STATIC 7

#define MASK_TO_VAL(x, i) { \
			u_int32_t _x = ntohl(x); \
			(i) = 1; \
			while ((_x) <<= 1) \
				(i)++; \
			};

#define VAL_TO_MASK(x, i) { \
			x = htonl(~((1 << (32 - (i))) - 1)); \
			};

#define MASKLEN_TO_MASK6(masklen, mask6) \
	do {\
		u_char maskarray[8] = \
		{0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff}; \
		int bytelen, bitlen, i; \
		memset(&(mask6), 0, sizeof(mask6));\
		bytelen = (masklen) / 8;\
		bitlen = (masklen) % 8;\
		for (i = 0; i < bytelen; i++) \
			(mask6).s6_addr[i] = 0xff;\
		if (bitlen) \
			(mask6).s6_addr[bytelen] = maskarray[bitlen - 1]; \
	}while(0);

/* obnoxious gcc gives an extraneous warning about this constant... */
#if defined(__STDC__) || defined(__GNUC__)
#define JAN_1970        2208988800UL    /* 1970 - 1900 in seconds */
#else
#define JAN_1970        2208988800L     /* 1970 - 1900 in seconds */
#define const           /**/
#endif

#define NBR_VERS(n)	(((n)->al_pv << 8) + (n)->al_mv)
