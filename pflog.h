/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

/*
 * pflog headers, at least as they exist now.
 */
#define PFLOG_IFNAMSIZ		16
#define PFLOG_RULESET_NAME_SIZE	16

/*
 * Direction values.
 */
#define PF_INOUT	0
#define PF_IN		1
#define PF_OUT		2
#if defined(__OpenBSD__)
#define PF_FWD		3
#endif

/*
 * Reason values.
 */
#define PFRES_MATCH	0
#define PFRES_BADOFF	1
#define PFRES_FRAG	2
#define PFRES_SHORT	3
#define PFRES_NORM	4
#define PFRES_MEMORY	5
#define PFRES_TS	6
#define PFRES_CONGEST	7
#define PFRES_IPOPTIONS 8
#define PFRES_PROTCKSUM 9
#define PFRES_BADSTATE	10
#define PFRES_STATEINS	11
#define PFRES_MAXSTATES	12
#define PFRES_SRCLIMIT	13
#define PFRES_SYNPROXY	14
#if defined(__FreeBSD__)
#define PFRES_MAPFAILED	15
#elif defined(__NetBSD__)
#define PFRES_STATELOCKED 15
#elif defined(__OpenBSD__)
#define PFRES_TRANSLATE	15
#define PFRES_NOROUTE	16
#elif defined(__APPLE__)
#define PFRES_DUMMYNET  15
#endif

/*
 * Action values.
 */
#define PF_PASS			0
#define PF_DROP			1
#define PF_SCRUB		2
#define PF_NOSCRUB		3
#define PF_NAT			4
#define PF_NONAT		5
#define PF_BINAT		6
#define PF_NOBINAT		7
#define PF_RDR			8
#define PF_NORDR		9
#define PF_SYNPROXY_DROP	10
#if defined(__FreeBSD__)
#define PF_DEFER		11
#elif defined(__OpenBSD__)
#define PF_DEFER		11
#define PF_MATCH		12
#define PF_DIVERT		13
#define PF_RT			14
#define PF_AFRT			15
#elif defined(__APPLE__)
#define PF_DUMMYNET		11
#define PF_NODUMMYNET		12
#define PF_NAT64		13
#define PF_NONAT64		14
#endif

struct pf_addr {
	union {
		nd_ipv4		v4;
		nd_ipv6		v6;
	} pfa;		    /* 128-bit address */
#define v4	pfa.v4
#define v6	pfa.v6
};

struct pfloghdr {
	nd_uint8_t	length;
	nd_uint8_t	af;
	nd_uint8_t	action;
	nd_uint8_t	reason;
	char		ifname[PFLOG_IFNAMSIZ];
	char		ruleset[PFLOG_RULESET_NAME_SIZE];
	nd_uint32_t	rulenr;
	nd_uint32_t	subrulenr;
	nd_uint32_t	uid;
	nd_int32_t	pid;
	nd_uint32_t	rule_uid;
	nd_int32_t	rule_pid;
	nd_uint8_t	dir;
/* Minimum header length (without padding): 61 */
#define MIN_PFLOG_HDRLEN 61
#if defined(__OpenBSD__)
	nd_uint8_t	rewritten;
	nd_uint8_t	naf;
	nd_uint8_t	pad[1];
#else
	nd_uint8_t	pad[3];
#endif
#if defined(__FreeBSD__)
	nd_uint32_t	ridentifier;
	nd_uint8_t	reserve;
	nd_uint8_t	pad2[3];
#elif defined(__OpenBSD__)
	struct pf_addr	saddr;
	struct pf_addr	daddr;
	nd_uint16_t	sport;
	nd_uint16_t	dport;
#endif
};
