/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/* \summary: IPv6 Internet Control Message Protocol (ICMPv6) printer */

#include <config.h>

#include "netdissect-stdinc.h"

#include <stdio.h>
#include <string.h>

#include "netdissect.h"
#include "addrtoname.h"
#include "addrtostr.h"
#include "extract.h"

#include "ip6.h"
#include "ipproto.h"

#include "udp.h"
#include "ah.h"

/*	NetBSD: icmp6.h,v 1.13 2000/08/03 16:30:37 itojun Exp	*/
/*	$KAME: icmp6.h,v 1.22 2000/08/03 15:25:16 jinmei Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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

struct icmp6_hdr {
	nd_uint8_t	icmp6_type;	/* type field */
	nd_uint8_t	icmp6_code;	/* code field */
	nd_uint16_t	icmp6_cksum;	/* checksum field */
	union {
		nd_uint32_t	icmp6_un_data32[1]; /* type-specific field */
		nd_uint16_t	icmp6_un_data16[2]; /* type-specific field */
		nd_uint8_t	icmp6_un_data8[4];  /* type-specific field */
		nd_byte		icmp6_un_data[1];   /* type-specific field */
	} icmp6_dataun;
};

#define icmp6_data32	icmp6_dataun.icmp6_un_data32
#define icmp6_data16	icmp6_dataun.icmp6_un_data16
#define icmp6_data8	icmp6_dataun.icmp6_un_data8
#define icmp6_data	icmp6_dataun.icmp6_un_data
#define icmp6_pptr	icmp6_data32[0]		/* parameter prob */
#define icmp6_mtu	icmp6_data32[0]		/* packet too big */
#define icmp6_id	icmp6_data16[0]		/* echo request/reply */
#define icmp6_seq	icmp6_data16[1]		/* echo request/reply */
#define icmp6_maxdelay	icmp6_data16[0]		/* mcast group membership */

#define ICMP6_DST_UNREACH		1	/* dest unreachable, codes: */
#define ICMP6_PACKET_TOO_BIG		2	/* packet too big */
#define ICMP6_TIME_EXCEEDED		3	/* time exceeded, code: */
#define ICMP6_PARAM_PROB		4	/* ip6 header bad */

#define ICMP6_ECHO_REQUEST		128	/* echo service */
#define ICMP6_ECHO_REPLY		129	/* echo reply */
#define ICMP6_MEMBERSHIP_QUERY		130	/* group membership query */
#define MLD6_LISTENER_QUERY		130	/* multicast listener query */
#define ICMP6_MEMBERSHIP_REPORT		131	/* group membership report */
#define MLD6_LISTENER_REPORT		131	/* multicast listener report */
#define ICMP6_MEMBERSHIP_REDUCTION	132	/* group membership termination */
#define MLD6_LISTENER_DONE		132	/* multicast listener done */

#define ND_ROUTER_SOLICIT		133	/* router solicitation */
#define ND_ROUTER_ADVERT		134	/* router advertisement */
#define ND_NEIGHBOR_SOLICIT		135	/* neighbor solicitation */
#define ND_NEIGHBOR_ADVERT		136	/* neighbor advertisement */
#define ND_REDIRECT			137	/* redirect */

#define ICMP6_ROUTER_RENUMBERING	138	/* router renumbering */

#define ICMP6_WRUREQUEST		139	/* who are you request */
#define ICMP6_WRUREPLY			140	/* who are you reply */
#define ICMP6_FQDN_QUERY		139	/* FQDN query */
#define ICMP6_FQDN_REPLY		140	/* FQDN reply */
#define ICMP6_NI_QUERY			139	/* node information request - RFC 4620 */
#define ICMP6_NI_REPLY			140	/* node information reply - RFC 4620 */
#define IND_SOLICIT			141	/* inverse neighbor solicitation */
#define IND_ADVERT			142	/* inverse neighbor advertisement */

#define ICMP6_V2_MEMBERSHIP_REPORT	143	/* v2 membership report */
#define MLDV2_LISTENER_REPORT		143	/* v2 multicast listener report */
#define ICMP6_HADISCOV_REQUEST		144
#define ICMP6_HADISCOV_REPLY		145
#define ICMP6_MOBILEPREFIX_SOLICIT	146
#define ICMP6_MOBILEPREFIX_ADVERT	147

#define MLD6_MTRACE_RESP		200	/* mtrace response(to sender) */
#define MLD6_MTRACE			201	/* mtrace messages */

#define ICMP6_MAXTYPE			201

#define ICMP6_DST_UNREACH_NOROUTE	0	/* no route to destination */
#define ICMP6_DST_UNREACH_ADMIN		1	/* administratively prohibited */
#define ICMP6_DST_UNREACH_NOTNEIGHBOR	2	/* not a neighbor(obsolete) */
#define ICMP6_DST_UNREACH_BEYONDSCOPE	2	/* beyond scope of source address */
#define ICMP6_DST_UNREACH_ADDR		3	/* address unreachable */
#define ICMP6_DST_UNREACH_NOPORT	4	/* port unreachable */

#define ICMP6_TIME_EXCEED_TRANSIT	0	/* ttl==0 in transit */
#define ICMP6_TIME_EXCEED_REASSEMBLY	1	/* ttl==0 in reass */

#define ICMP6_PARAMPROB_HEADER		0	/* erroneous header field */
#define ICMP6_PARAMPROB_NEXTHEADER	1	/* unrecognized next header */
#define ICMP6_PARAMPROB_OPTION		2	/* unrecognized option */
#define ICMP6_PARAMPROB_FRAGHDRCHAIN	3	/* incomplete header chain */

#define ICMP6_INFOMSG_MASK		0x80	/* all informational messages */

#define ICMP6_NI_SUBJ_IPV6	0	/* Query Subject is an IPv6 address */
#define ICMP6_NI_SUBJ_FQDN	1	/* Query Subject is a Domain name */
#define ICMP6_NI_SUBJ_IPV4	2	/* Query Subject is an IPv4 address */

#define ICMP6_NI_SUCCESS	0	/* node information successful reply */
#define ICMP6_NI_REFUSED	1	/* node information request is refused */
#define ICMP6_NI_UNKNOWN	2	/* unknown Qtype */

#define ICMP6_ROUTER_RENUMBERING_COMMAND  0	/* rr command */
#define ICMP6_ROUTER_RENUMBERING_RESULT   1	/* rr result */
#define ICMP6_ROUTER_RENUMBERING_SEQNUM_RESET   255	/* rr seq num reset */

/* Used in kernel only */
#define ND_REDIRECT_ONLINK	0	/* redirect to an on-link node */
#define ND_REDIRECT_ROUTER	1	/* redirect to a better router */

/*
 * Multicast Listener Discovery
 */
struct mld6_hdr {
	struct icmp6_hdr	mld6_hdr;
	nd_ipv6			mld6_addr; /* multicast address */
};

#define mld6_type	mld6_hdr.icmp6_type
#define mld6_code	mld6_hdr.icmp6_code
#define mld6_cksum	mld6_hdr.icmp6_cksum
#define mld6_maxdelay	mld6_hdr.icmp6_data16[0]
#define mld6_reserved	mld6_hdr.icmp6_data16[1]

#define MLD_MINLEN	24
#define MLDV2_MINLEN	28

/*
 * Neighbor Discovery
 */

struct nd_router_solicit {	/* router solicitation */
	struct icmp6_hdr	nd_rs_hdr;
	/* could be followed by options */
};

#define nd_rs_type	nd_rs_hdr.icmp6_type
#define nd_rs_code	nd_rs_hdr.icmp6_code
#define nd_rs_cksum	nd_rs_hdr.icmp6_cksum
#define nd_rs_reserved	nd_rs_hdr.icmp6_data32[0]

struct nd_router_advert {	/* router advertisement */
	struct icmp6_hdr	nd_ra_hdr;
	nd_uint32_t		nd_ra_reachable;	/* reachable time */
	nd_uint32_t		nd_ra_retransmit;	/* retransmit timer */
	/* could be followed by options */
};

#define nd_ra_type		nd_ra_hdr.icmp6_type
#define nd_ra_code		nd_ra_hdr.icmp6_code
#define nd_ra_cksum		nd_ra_hdr.icmp6_cksum
#define nd_ra_curhoplimit	nd_ra_hdr.icmp6_data8[0]
#define nd_ra_flags_reserved	nd_ra_hdr.icmp6_data8[1]
#define ND_RA_FLAG_MANAGED	0x80
#define ND_RA_FLAG_OTHER	0x40
#define ND_RA_FLAG_HOME_AGENT	0x20
#define ND_RA_FLAG_IPV6ONLY	0x02

/*
 * Router preference values based on draft-draves-ipngwg-router-selection-01.
 * These are non-standard definitions.
 */
#define ND_RA_FLAG_RTPREF_MASK	0x18 /* 00011000 */

#define ND_RA_FLAG_RTPREF_HIGH	0x08 /* 00001000 */
#define ND_RA_FLAG_RTPREF_MEDIUM	0x00 /* 00000000 */
#define ND_RA_FLAG_RTPREF_LOW	0x18 /* 00011000 */
#define ND_RA_FLAG_RTPREF_RSV	0x10 /* 00010000 */

#define nd_ra_router_lifetime	nd_ra_hdr.icmp6_data16[1]

struct nd_neighbor_solicit {	/* neighbor solicitation */
	struct icmp6_hdr	nd_ns_hdr;
	nd_ipv6			nd_ns_target;	/*target address */
	/* could be followed by options */
};

#define nd_ns_type		nd_ns_hdr.icmp6_type
#define nd_ns_code		nd_ns_hdr.icmp6_code
#define nd_ns_cksum		nd_ns_hdr.icmp6_cksum
#define nd_ns_reserved		nd_ns_hdr.icmp6_data32[0]

struct nd_neighbor_advert {	/* neighbor advertisement */
	struct icmp6_hdr	nd_na_hdr;
	nd_ipv6			nd_na_target;	/* target address */
	/* could be followed by options */
};

#define nd_na_type		nd_na_hdr.icmp6_type
#define nd_na_code		nd_na_hdr.icmp6_code
#define nd_na_cksum		nd_na_hdr.icmp6_cksum
#define nd_na_flags_reserved	nd_na_hdr.icmp6_data32[0]

#define ND_NA_FLAG_ROUTER		0x80000000
#define ND_NA_FLAG_SOLICITED		0x40000000
#define ND_NA_FLAG_OVERRIDE		0x20000000

struct nd_redirect {		/* redirect */
	struct icmp6_hdr	nd_rd_hdr;
	nd_ipv6			nd_rd_target;	/* target address */
	nd_ipv6			nd_rd_dst;	/* destination address */
	/* could be followed by options */
};

#define nd_rd_type		nd_rd_hdr.icmp6_type
#define nd_rd_code		nd_rd_hdr.icmp6_code
#define nd_rd_cksum		nd_rd_hdr.icmp6_cksum
#define nd_rd_reserved		nd_rd_hdr.icmp6_data32[0]

struct nd_opt_hdr {		/* Neighbor discovery option header */
	nd_uint8_t	nd_opt_type;
	nd_uint8_t	nd_opt_len;
	/* followed by option specific data*/
};

#define ND_OPT_SOURCE_LINKADDR		1
#define ND_OPT_TARGET_LINKADDR		2
#define ND_OPT_PREFIX_INFORMATION	3
#define ND_OPT_REDIRECTED_HEADER	4
#define ND_OPT_MTU			5
#define ND_OPT_ADVINTERVAL		7
#define ND_OPT_HOMEAGENT_INFO		8
#define ND_OPT_ROUTE_INFO		24	/* RFC4191 */
#define ND_OPT_RDNSS			25
#define ND_OPT_DNSSL			31

struct nd_opt_prefix_info {	/* prefix information */
	nd_uint8_t	nd_opt_pi_type;
	nd_uint8_t	nd_opt_pi_len;
	nd_uint8_t	nd_opt_pi_prefix_len;
	nd_uint8_t	nd_opt_pi_flags_reserved;
	nd_uint32_t	nd_opt_pi_valid_time;
	nd_uint32_t	nd_opt_pi_preferred_time;
	nd_uint32_t	nd_opt_pi_reserved2;
	nd_ipv6		nd_opt_pi_prefix;
};

#define ND_OPT_PI_FLAG_ONLINK		0x80
#define ND_OPT_PI_FLAG_AUTO		0x40
#define ND_OPT_PI_FLAG_ROUTER		0x20	/*2292bis*/

struct nd_opt_rd_hdr {         /* redirected header */
	nd_uint8_t	nd_opt_rh_type;
	nd_uint8_t	nd_opt_rh_len;
	nd_uint16_t	nd_opt_rh_reserved1;
	nd_uint32_t	nd_opt_rh_reserved2;
	/* followed by IP header and data */
};

struct nd_opt_mtu {		/* MTU option */
	nd_uint8_t	nd_opt_mtu_type;
	nd_uint8_t	nd_opt_mtu_len;
	nd_uint16_t	nd_opt_mtu_reserved;
	nd_uint32_t	nd_opt_mtu_mtu;
};

struct nd_opt_rdnss {		/* RDNSS RFC 6106 5.1 */
	nd_uint8_t	nd_opt_rdnss_type;
	nd_uint8_t	nd_opt_rdnss_len;
	nd_uint16_t	nd_opt_rdnss_reserved;
	nd_uint32_t	nd_opt_rdnss_lifetime;
	nd_ipv6		nd_opt_rdnss_addr[1];	/* variable-length */
};

struct nd_opt_dnssl {		/* DNSSL RFC 6106 5.2 */
	nd_uint8_t  nd_opt_dnssl_type;
	nd_uint8_t  nd_opt_dnssl_len;
	nd_uint16_t nd_opt_dnssl_reserved;
	nd_uint32_t nd_opt_dnssl_lifetime;
	/* followed by list of DNS search domains, variable-length */
};

struct nd_opt_advinterval {	/* Advertisement interval option */
	nd_uint8_t	nd_opt_adv_type;
	nd_uint8_t	nd_opt_adv_len;
	nd_uint16_t	nd_opt_adv_reserved;
	nd_uint32_t	nd_opt_adv_interval;
};

struct nd_opt_homeagent_info {	/* Home Agent info */
	nd_uint8_t	nd_opt_hai_type;
	nd_uint8_t	nd_opt_hai_len;
	nd_uint16_t	nd_opt_hai_reserved;
	nd_uint16_t	nd_opt_hai_preference;
	nd_uint16_t	nd_opt_hai_lifetime;
};

struct nd_opt_route_info {	/* route info */
	nd_uint8_t	nd_opt_rti_type;
	nd_uint8_t	nd_opt_rti_len;
	nd_uint8_t	nd_opt_rti_prefixlen;
	nd_uint8_t	nd_opt_rti_flags;
	nd_uint32_t	nd_opt_rti_lifetime;
	/* prefix follows */
};

/*
 * icmp6 namelookup
 */

struct icmp6_namelookup {
	struct icmp6_hdr	icmp6_nl_hdr;
	nd_byte			icmp6_nl_nonce[8];
	nd_int32_t		icmp6_nl_ttl;
#if 0
	nd_uint8_t		icmp6_nl_len;
	nd_byte			icmp6_nl_name[3];
#endif
	/* could be followed by options */
};

/*
 * icmp6 node information
 */
struct icmp6_nodeinfo {
	struct icmp6_hdr icmp6_ni_hdr;
	nd_byte icmp6_ni_nonce[8];
	/* could be followed by reply data */
};

#define ni_type		icmp6_ni_hdr.icmp6_type
#define ni_code		icmp6_ni_hdr.icmp6_code
#define ni_cksum	icmp6_ni_hdr.icmp6_cksum
#define ni_qtype	icmp6_ni_hdr.icmp6_data16[0]
#define ni_flags	icmp6_ni_hdr.icmp6_data16[1]

#define NI_QTYPE_NOOP		0 /* NOOP  */
#define NI_QTYPE_SUPTYPES	1 /* Supported Qtypes (drafts up to 09) */
#define NI_QTYPE_FQDN		2 /* FQDN (draft 04) */
#define NI_QTYPE_DNSNAME	2 /* DNS Name */
#define NI_QTYPE_NODEADDR	3 /* Node Addresses */
#define NI_QTYPE_IPV4ADDR	4 /* IPv4 Addresses */

#define NI_NODEADDR_FLAG_TRUNCATE	0x0001
#define NI_NODEADDR_FLAG_ALL		0x0002
#define NI_NODEADDR_FLAG_COMPAT		0x0004
#define NI_NODEADDR_FLAG_LINKLOCAL	0x0008
#define NI_NODEADDR_FLAG_SITELOCAL	0x0010
#define NI_NODEADDR_FLAG_GLOBAL		0x0020
#define NI_NODEADDR_FLAG_ANYCAST	0x0040 /* just experimental. not in spec */

struct ni_reply_fqdn {
	nd_uint32_t ni_fqdn_ttl;	/* TTL */
	nd_uint8_t ni_fqdn_namelen; /* length in octets of the FQDN */
	nd_byte ni_fqdn_name[3]; /* XXX: alignment */
};

/*
 * Router Renumbering. as router-renum-08.txt
 */
struct icmp6_router_renum {	/* router renumbering header */
	struct icmp6_hdr	rr_hdr;
	nd_uint8_t		rr_segnum;
	nd_uint8_t		rr_flags;
	nd_uint16_t		rr_maxdelay;
	nd_uint32_t		rr_reserved;
};
#define ICMP6_RR_FLAGS_TEST		0x80
#define ICMP6_RR_FLAGS_REQRESULT	0x40
#define ICMP6_RR_FLAGS_FORCEAPPLY	0x20
#define ICMP6_RR_FLAGS_SPECSITE		0x10
#define ICMP6_RR_FLAGS_PREVDONE		0x08

#define rr_type		rr_hdr.icmp6_type
#define rr_code		rr_hdr.icmp6_code
#define rr_cksum	rr_hdr.icmp6_cksum
#define rr_seqnum	rr_hdr.icmp6_data32[0]

struct rr_pco_match {		/* match prefix part */
	nd_uint8_t		rpm_code;
	nd_uint8_t		rpm_len;
	nd_uint8_t		rpm_ordinal;
	nd_uint8_t		rpm_matchlen;
	nd_uint8_t		rpm_minlen;
	nd_uint8_t		rpm_maxlen;
	nd_uint16_t		rpm_reserved;
	nd_ipv6			rpm_prefix;
};

#define RPM_PCO_ADD		1
#define RPM_PCO_CHANGE		2
#define RPM_PCO_SETGLOBAL	3
#define RPM_PCO_MAX		4

struct rr_pco_use {		/* use prefix part */
	nd_uint8_t	rpu_uselen;
	nd_uint8_t	rpu_keeplen;
	nd_uint8_t	rpu_ramask;
	nd_uint8_t	rpu_raflags;
	nd_uint32_t	rpu_vltime;
	nd_uint32_t	rpu_pltime;
	nd_uint32_t	rpu_flags;
	nd_ipv6		rpu_prefix;
};
#define ICMP6_RR_PCOUSE_RAFLAGS_ONLINK	0x80
#define ICMP6_RR_PCOUSE_RAFLAGS_AUTO	0x40

/* network endian */
#define ICMP6_RR_PCOUSE_FLAGS_DECRVLTIME     ((uint32_t)htonl(0x80000000))
#define ICMP6_RR_PCOUSE_FLAGS_DECRPLTIME     ((uint32_t)htonl(0x40000000))

struct rr_result {		/* router renumbering result message */
	nd_uint16_t	rrr_flags;
	nd_uint8_t	rrr_ordinal;
	nd_uint8_t	rrr_matchedlen;
	nd_uint32_t	rrr_ifid;
	nd_ipv6		rrr_prefix;
};
/* network endian */
#define ICMP6_RR_RESULT_FLAGS_OOB		((uint16_t)htons(0x0002))
#define ICMP6_RR_RESULT_FLAGS_FORBIDDEN		((uint16_t)htons(0x0001))

static const char *get_rtpref(u_int);
static const char *get_lifetime(uint32_t);
static void print_lladdr(netdissect_options *ndo, const u_char *, size_t);
static int icmp6_opt_print(netdissect_options *ndo, const u_char *, int);
static void mld6_print(netdissect_options *ndo, const u_char *);
static void mldv2_report_print(netdissect_options *ndo, const u_char *, u_int);
static void mldv2_query_print(netdissect_options *ndo, const u_char *, u_int);
static const struct udphdr *get_upperlayer(netdissect_options *ndo, const u_char *, u_int *);
static void dnsname_print(netdissect_options *ndo, const u_char *, const u_char *);
static void icmp6_nodeinfo_print(netdissect_options *ndo, u_int, const u_char *, const u_char *);
static void icmp6_rrenum_print(netdissect_options *ndo, const u_char *, const u_char *);

/*
 * DIO: Updated to RFC6550, as published in 2012: section 6. (page 30)
 */

#define ND_RPL_MESSAGE 155  /* 0x9B */

enum ND_RPL_CODE {
    ND_RPL_DAG_IS=0x00,
    ND_RPL_DAG_IO=0x01,
    ND_RPL_DAO   =0x02,
    ND_RPL_DAO_ACK=0x03,
    ND_RPL_SEC_DAG_IS = 0x80,
    ND_RPL_SEC_DAG_IO = 0x81,
    ND_RPL_SEC_DAG    = 0x82,
    ND_RPL_SEC_DAG_ACK= 0x83,
    ND_RPL_SEC_CONSIST= 0x8A
};

enum ND_RPL_DIO_FLAGS {
        ND_RPL_DIO_GROUNDED = 0x80,
        ND_RPL_DIO_DATRIG   = 0x40,
        ND_RPL_DIO_DASUPPORT= 0x20,
        ND_RPL_DIO_RES4     = 0x10,
        ND_RPL_DIO_RES3     = 0x08,
        ND_RPL_DIO_PRF_MASK = 0x07  /* 3-bit preference */
};

#define DAGID_LEN 16

/* section 6 of draft-ietf-roll-rpl-19 */
struct nd_rpl_security {
    nd_uint8_t  rpl_sec_t_reserved;     /* bit 7 is T-bit */
    nd_uint8_t  rpl_sec_algo;
    nd_uint16_t rpl_sec_kim_lvl_flags;  /* bit 15/14, KIM */
                                      /* bit 10-8, LVL, bit 7-0 flags */
    nd_uint32_t rpl_sec_counter;
#if 0
    nd_byte     rpl_sec_ki[0];          /* depends upon kim */
#endif
};

/* section 6.2.1, DODAG Information Solicitation (DIS_IS) */
struct nd_rpl_dis_is {
    nd_uint8_t rpl_dis_flags;
    nd_uint8_t rpl_dis_reserved;
#if 0
    nd_byte    rpl_dis_options[0];
#endif
};

/* section 6.3.1, DODAG Information Object (DIO) */
struct nd_rpl_dio {
    nd_uint8_t  rpl_instanceid;
    nd_uint8_t  rpl_version;
    nd_uint16_t rpl_dagrank;
    nd_uint8_t  rpl_mopprf;   /* bit 7=G, 5-3=MOP, 2-0=PRF */
    nd_uint8_t  rpl_dtsn;     /* Dest. Advertisement Trigger Sequence Number */
    nd_uint8_t  rpl_flags;    /* no flags defined yet */
    nd_uint8_t  rpl_resv1;
    nd_byte     rpl_dagid[DAGID_LEN];
};
#define RPL_DIO_GROUND_FLAG 0x80
#define RPL_DIO_MOP_SHIFT   3
#define RPL_DIO_MOP_MASK    (7 << RPL_DIO_MOP_SHIFT)
#define RPL_DIO_PRF_SHIFT   0
#define RPL_DIO_PRF_MASK    (7 << RPL_DIO_PRF_SHIFT)
#define RPL_DIO_GROUNDED(X) ((X)&RPL_DIO_GROUND_FLAG)
#define RPL_DIO_MOP(X)      (enum RPL_DIO_MOP)(((X)&RPL_DIO_MOP_MASK) >> RPL_DIO_MOP_SHIFT)
#define RPL_DIO_PRF(X)      (((X)&RPL_DIO_PRF_MASK) >> RPL_DIO_PRF_SHIFT)

enum RPL_DIO_MOP {
    RPL_DIO_NONSTORING= 0x0,
    RPL_DIO_STORING   = 0x1,
    RPL_DIO_NONSTORING_MULTICAST = 0x2,
    RPL_DIO_STORING_MULTICAST    = 0x3
};

enum RPL_SUBOPT {
        RPL_OPT_PAD1        = 0,
        RPL_OPT_PADN        = 1,
        RPL_DIO_METRICS     = 2,
        RPL_DIO_ROUTINGINFO = 3,
        RPL_DIO_CONFIG      = 4,
        RPL_DAO_RPLTARGET   = 5,
        RPL_DAO_TRANSITINFO = 6,
        RPL_DIO_DESTPREFIX  = 8,
        RPL_DAO_RPLTARGET_DESC=9
};

struct rpl_genoption {
    nd_uint8_t rpl_dio_type;
    nd_uint8_t rpl_dio_len;        /* suboption length, not including type/len */
};
#define RPL_GENOPTION_LEN	2

#define RPL_DIO_LIFETIME_INFINITE   0xffffffff
#define RPL_DIO_LIFETIME_DISCONNECT 0

struct rpl_dio_destprefix {
    nd_uint8_t rpl_dio_type;
    nd_uint8_t rpl_dio_len;
    nd_uint8_t rpl_dio_prefixlen;        /* in bits */
    nd_uint8_t rpl_dio_prf;              /* flags, including Route Preference */
    nd_uint32_t rpl_dio_prefixlifetime;  /* in seconds */
#if 0
    nd_byte     rpl_dio_prefix[0];       /* variable number of bytes */
#endif
};

/* section 6.4.1, DODAG Information Object (DIO) */
struct nd_rpl_dao {
    nd_uint8_t  rpl_instanceid;
    nd_uint8_t  rpl_flags;      /* bit 7=K, 6=D */
    nd_uint8_t  rpl_resv;
    nd_uint8_t  rpl_daoseq;
    nd_byte     rpl_dagid[DAGID_LEN];   /* present when D set. */
};
#define ND_RPL_DAO_MIN_LEN	4	/* length without DAGID */

/* indicates if this DAO is to be acK'ed */
#define RPL_DAO_K_SHIFT   7
#define RPL_DAO_K_MASK    (1 << RPL_DAO_K_SHIFT)
#define RPL_DAO_K(X)      (((X)&RPL_DAO_K_MASK) >> RPL_DAO_K_SHIFT)

/* indicates if the DAGID is present */
#define RPL_DAO_D_SHIFT   6
#define RPL_DAO_D_MASK    (1 << RPL_DAO_D_SHIFT)
#define RPL_DAO_D(X)      (((X)&RPL_DAO_D_MASK) >> RPL_DAO_D_SHIFT)

struct rpl_dao_target {
    nd_uint8_t rpl_dao_type;
    nd_uint8_t rpl_dao_len;
    nd_uint8_t rpl_dao_flags;            /* unused */
    nd_uint8_t rpl_dao_prefixlen;        /* in bits */
#if 0
    nd_byte    rpl_dao_prefix[0];        /* variable number of bytes */
#endif
};

/* section 6.5.1, Destination Advertisement Object Acknowledgement (DAO-ACK) */
struct nd_rpl_daoack {
    nd_uint8_t  rpl_instanceid;
    nd_uint8_t  rpl_flags;      /* bit 7=D */
    nd_uint8_t  rpl_daoseq;
    nd_uint8_t  rpl_status;
    nd_byte     rpl_dagid[DAGID_LEN];   /* present when D set. */
};
#define ND_RPL_DAOACK_MIN_LEN	4	/* length without DAGID */
/* indicates if the DAGID is present */
#define RPL_DAOACK_D_SHIFT   7
#define RPL_DAOACK_D_MASK    (1 << RPL_DAOACK_D_SHIFT)
#define RPL_DAOACK_D(X)      (((X)&RPL_DAOACK_D_MASK) >> RPL_DAOACK_D_SHIFT)

static const struct tok icmp6_type_values[] = {
    { ICMP6_DST_UNREACH, "destination unreachable"},
    { ICMP6_PACKET_TOO_BIG, "packet too big"},
    { ICMP6_TIME_EXCEEDED, "time exceeded in-transit"},
    { ICMP6_PARAM_PROB, "parameter problem"},
    { ICMP6_ECHO_REQUEST, "echo request"},
    { ICMP6_ECHO_REPLY, "echo reply"},
    { MLD6_LISTENER_QUERY, "multicast listener query"},
    { MLD6_LISTENER_REPORT, "multicast listener report"},
    { MLD6_LISTENER_DONE, "multicast listener done"},
    { ND_ROUTER_SOLICIT, "router solicitation"},
    { ND_ROUTER_ADVERT, "router advertisement"},
    { ND_NEIGHBOR_SOLICIT, "neighbor solicitation"},
    { ND_NEIGHBOR_ADVERT, "neighbor advertisement"},
    { ND_REDIRECT, "redirect"},
    { ICMP6_ROUTER_RENUMBERING, "router renumbering"},
    { IND_SOLICIT, "inverse neighbor solicitation"},
    { IND_ADVERT, "inverse neighbor advertisement"},
    { MLDV2_LISTENER_REPORT, "multicast listener report v2"},
    { ICMP6_HADISCOV_REQUEST, "ha discovery request"},
    { ICMP6_HADISCOV_REPLY, "ha discovery reply"},
    { ICMP6_MOBILEPREFIX_SOLICIT, "mobile router solicitation"},
    { ICMP6_MOBILEPREFIX_ADVERT, "mobile router advertisement"},
    { ICMP6_WRUREQUEST, "who-are-you request"},
    { ICMP6_WRUREPLY, "who-are-you reply"},
    { ICMP6_NI_QUERY, "node information query"},
    { ICMP6_NI_REPLY, "node information reply"},
    { MLD6_MTRACE, "mtrace message"},
    { MLD6_MTRACE_RESP, "mtrace response"},
    { ND_RPL_MESSAGE,   "RPL"},
    { 0,	NULL }
};

static const struct tok icmp6_dst_unreach_code_values[] = {
    { ICMP6_DST_UNREACH_NOROUTE, "unreachable route" },
    { ICMP6_DST_UNREACH_ADMIN, " unreachable prohibited"},
    { ICMP6_DST_UNREACH_BEYONDSCOPE, "beyond scope"},
    { ICMP6_DST_UNREACH_ADDR, "unreachable address"},
    { ICMP6_DST_UNREACH_NOPORT, "unreachable port"},
    { 0,	NULL }
};

static const struct tok icmp6_opt_pi_flag_values[] = {
    { ND_OPT_PI_FLAG_ONLINK, "onlink" },
    { ND_OPT_PI_FLAG_AUTO, "auto" },
    { ND_OPT_PI_FLAG_ROUTER, "router" },
    { 0,	NULL }
};

static const struct tok icmp6_opt_ra_flag_values[] = {
    { ND_RA_FLAG_MANAGED, "managed" },
    { ND_RA_FLAG_OTHER, "other stateful"},
    { ND_RA_FLAG_HOME_AGENT, "home agent"},
    { ND_RA_FLAG_IPV6ONLY, "ipv6 only"},
    { 0,	NULL }
};

static const struct tok icmp6_nd_na_flag_values[] = {
    { ND_NA_FLAG_ROUTER, "router" },
    { ND_NA_FLAG_SOLICITED, "solicited" },
    { ND_NA_FLAG_OVERRIDE, "override" },
    { 0,	NULL }
};

static const struct tok icmp6_opt_values[] = {
   { ND_OPT_SOURCE_LINKADDR, "source link-address"},
   { ND_OPT_TARGET_LINKADDR, "destination link-address"},
   { ND_OPT_PREFIX_INFORMATION, "prefix info"},
   { ND_OPT_REDIRECTED_HEADER, "redirected header"},
   { ND_OPT_MTU, "mtu"},
   { ND_OPT_RDNSS, "rdnss"},
   { ND_OPT_DNSSL, "dnssl"},
   { ND_OPT_ADVINTERVAL, "advertisement interval"},
   { ND_OPT_HOMEAGENT_INFO, "homeagent information"},
   { ND_OPT_ROUTE_INFO, "route info"},
   { 0,	NULL }
};

/* mldv2 report types */
static const struct tok mldv2report2str[] = {
	{ 1,	"is_in" },
	{ 2,	"is_ex" },
	{ 3,	"to_in" },
	{ 4,	"to_ex" },
	{ 5,	"allow" },
	{ 6,	"block" },
	{ 0,	NULL }
};

static const char *
get_rtpref(u_int v)
{
	static const char *rtpref_str[] = {
		"medium",		/* 00 */
		"high",			/* 01 */
		"rsv",			/* 10 */
		"low"			/* 11 */
	};

	return rtpref_str[((v & ND_RA_FLAG_RTPREF_MASK) >> 3) & 0xff];
}

static const char *
get_lifetime(uint32_t v)
{
	static char buf[20];

	if (v == (uint32_t)~0UL)
		return "infinity";
	else {
		snprintf(buf, sizeof(buf), "%us", v);
		return buf;
	}
}

static void
print_lladdr(netdissect_options *ndo, const uint8_t *p, size_t l)
{
	const uint8_t *ep, *q;

	q = p;
	ep = p + l;
	while (l > 0 && q < ep) {
		if (q > p)
                        ND_PRINT(":");
		ND_PRINT("%02x", GET_U_1(q));
		q++;
		l--;
	}
}

static uint16_t icmp6_cksum(netdissect_options *ndo, const struct ip6_hdr *ip6,
	const struct icmp6_hdr *icp, u_int len)
{
	return nextproto6_cksum(ndo, ip6, (const uint8_t *)(const void *)icp, len, len,
				IPPROTO_ICMPV6);
}

static const struct tok rpl_mop_values[] = {
        { RPL_DIO_NONSTORING,         "nonstoring"},
        { RPL_DIO_STORING,            "storing"},
        { RPL_DIO_NONSTORING_MULTICAST, "nonstoring-multicast"},
        { RPL_DIO_STORING_MULTICAST,  "storing-multicast"},
        { 0, NULL},
};

static const struct tok rpl_subopt_values[] = {
        { RPL_OPT_PAD1, "pad1"},
        { RPL_OPT_PADN, "padN"},
        { RPL_DIO_METRICS, "metrics"},
        { RPL_DIO_ROUTINGINFO, "routinginfo"},
        { RPL_DIO_CONFIG,    "config"},
        { RPL_DAO_RPLTARGET, "rpltarget"},
        { RPL_DAO_TRANSITINFO, "transitinfo"},
        { RPL_DIO_DESTPREFIX, "destprefix"},
        { RPL_DAO_RPLTARGET_DESC, "rpltargetdesc"},
        { 0, NULL},
};

static void
rpl_printopts(netdissect_options *ndo, const uint8_t *opts, u_int length)
{
	const struct rpl_genoption *opt;
	uint8_t dio_type;
	u_int optlen;

	while (length != 0) {
		opt = (const struct rpl_genoption *)opts;
		dio_type = GET_U_1(opt->rpl_dio_type);
		if (dio_type == RPL_OPT_PAD1) {
                        optlen = 1;
                        ND_PRINT(" opt:pad1");
                } else {
			if (length < RPL_GENOPTION_LEN)
				goto trunc;
	                optlen = GET_U_1(opt->rpl_dio_len)+RPL_GENOPTION_LEN;
                        ND_PRINT(" opt:%s len:%u ",
                                  tok2str(rpl_subopt_values, "subopt:%u", dio_type),
                                  optlen);
                        ND_TCHECK_LEN(opt, optlen);
                        if (length < optlen)
				goto trunc;
                        if (ndo->ndo_vflag > 2) {
                                hex_print(ndo,
                                          " ",
                                          opts + RPL_GENOPTION_LEN,  /* content of DIO option */
                                          optlen - RPL_GENOPTION_LEN);
                        }
                }
                opts += optlen;
                length -= optlen;
        }
        return;
trunc:
	nd_print_trunc(ndo);
}

static void
rpl_dio_print(netdissect_options *ndo,
              const u_char *bp, u_int length)
{
        const struct nd_rpl_dio *dio = (const struct nd_rpl_dio *)bp;

        ND_ICHECK_ZU(length, <, sizeof(struct nd_rpl_dio));
        ND_PRINT(" [dagid:%s,seq:%u,instance:%u,rank:%u,%smop:%s,prf:%u]",
                  GET_IP6ADDR_STRING(dio->rpl_dagid),
                  GET_U_1(dio->rpl_dtsn),
                  GET_U_1(dio->rpl_instanceid),
                  GET_BE_U_2(dio->rpl_dagrank),
                  RPL_DIO_GROUNDED(GET_U_1(dio->rpl_mopprf)) ? "grounded,":"",
                  tok2str(rpl_mop_values, "mop%u",
                          RPL_DIO_MOP(GET_U_1(dio->rpl_mopprf))),
                  RPL_DIO_PRF(GET_U_1(dio->rpl_mopprf)));

        if(ndo->ndo_vflag > 1) {
                rpl_printopts(ndo, bp + sizeof(struct nd_rpl_dio),
                              length - sizeof(struct nd_rpl_dio));
        }
        return;
invalid:
        nd_print_invalid(ndo);
}

static void
rpl_dao_print(netdissect_options *ndo,
              const u_char *bp, u_int length)
{
        const struct nd_rpl_dao *dao = (const struct nd_rpl_dao *)bp;
        const char *dagid_str = "<elided>";
        uint8_t rpl_flags;

        ND_TCHECK_SIZE(dao);
        if (length < ND_RPL_DAO_MIN_LEN)
		goto tooshort;

        bp += ND_RPL_DAO_MIN_LEN;
        length -= ND_RPL_DAO_MIN_LEN;
        rpl_flags = GET_U_1(dao->rpl_flags);
        if(RPL_DAO_D(rpl_flags)) {
                ND_TCHECK_LEN(dao->rpl_dagid, DAGID_LEN);
                if (length < DAGID_LEN)
                        goto tooshort;
                dagid_str = ip6addr_string (ndo, dao->rpl_dagid);
                bp += DAGID_LEN;
                length -= DAGID_LEN;
        }

        ND_PRINT(" [dagid:%s,seq:%u,instance:%u%s%s,flags:%02x]",
                  dagid_str,
                  GET_U_1(dao->rpl_daoseq),
                  GET_U_1(dao->rpl_instanceid),
                  RPL_DAO_K(rpl_flags) ? ",acK":"",
                  RPL_DAO_D(rpl_flags) ? ",Dagid":"",
                  rpl_flags);

        if(ndo->ndo_vflag > 1) {
                rpl_printopts(ndo, bp, length);
        }
	return;

trunc:
	nd_print_trunc(ndo);
	return;

tooshort:
	ND_PRINT(" [|length too short]");
}

static void
rpl_daoack_print(netdissect_options *ndo,
                 const u_char *bp, u_int length)
{
        const struct nd_rpl_daoack *daoack = (const struct nd_rpl_daoack *)bp;
        const char *dagid_str = "<elided>";

        ND_TCHECK_LEN(daoack, ND_RPL_DAOACK_MIN_LEN);
        if (length < ND_RPL_DAOACK_MIN_LEN)
		goto tooshort;

        bp += ND_RPL_DAOACK_MIN_LEN;
        length -= ND_RPL_DAOACK_MIN_LEN;
        if(RPL_DAOACK_D(GET_U_1(daoack->rpl_flags))) {
                ND_TCHECK_LEN(daoack->rpl_dagid, DAGID_LEN);
                if (length < DAGID_LEN)
                        goto tooshort;
                dagid_str = ip6addr_string (ndo, daoack->rpl_dagid);
                bp += DAGID_LEN;
                length -= DAGID_LEN;
        }

        ND_PRINT(" [dagid:%s,seq:%u,instance:%u,status:%u]",
                  dagid_str,
                  GET_U_1(daoack->rpl_daoseq),
                  GET_U_1(daoack->rpl_instanceid),
                  GET_U_1(daoack->rpl_status));

        /* no officially defined options for DAOACK, but print any we find */
        if(ndo->ndo_vflag > 1) {
                rpl_printopts(ndo, bp, length);
        }
	return;

trunc:
	nd_print_trunc(ndo);
	return;

tooshort:
	ND_PRINT(" [|dao-length too short]");
}

static void
rpl_print(netdissect_options *ndo,
          uint8_t icmp6_code,
          const u_char *bp, u_int length)
{
        int secured = icmp6_code & 0x80;
        int basecode= icmp6_code & 0x7f;

        if(secured) {
                ND_PRINT(", (SEC) [worktodo]");
                /* XXX
                 * the next header pointer needs to move forward to
                 * skip the secure part.
                 */
                return;
        } else {
                ND_PRINT(", (CLR)");
        }

        switch(basecode) {
        case ND_RPL_DAG_IS:
                ND_PRINT("DODAG Information Solicitation");
                if(ndo->ndo_vflag) {
                }
                break;
        case ND_RPL_DAG_IO:
                ND_PRINT("DODAG Information Object");
                if(ndo->ndo_vflag) {
                        rpl_dio_print(ndo, bp, length);
                }
                break;
        case ND_RPL_DAO:
                ND_PRINT("Destination Advertisement Object");
                if(ndo->ndo_vflag) {
                        rpl_dao_print(ndo, bp, length);
                }
                break;
        case ND_RPL_DAO_ACK:
                ND_PRINT("Destination Advertisement Object Ack");
                if(ndo->ndo_vflag) {
                        rpl_daoack_print(ndo, bp, length);
                }
                break;
        default:
                ND_PRINT("RPL message, unknown code %u",icmp6_code);
                break;
        }
	return;

#if 0
trunc:
	nd_print_trunc(ndo);
	return;
#endif

}

void
icmp6_print(netdissect_options *ndo,
            const u_char *bp, u_int length, const u_char *bp2, int fragmented)
{
	const struct icmp6_hdr *dp;
	uint8_t icmp6_type, icmp6_code;
	const struct ip6_hdr *ip;
	const struct ip6_hdr *oip;
	const struct udphdr *ouh;
	uint16_t dport;
	const u_char *ep;
	u_int prot;

	ndo->ndo_protocol = "icmp6";
	dp = (const struct icmp6_hdr *)bp;
	ip = (const struct ip6_hdr *)bp2;
	oip = (const struct ip6_hdr *)(dp + 1);
	/* 'ep' points to the end of available data. */
	ep = ndo->ndo_snapend;
	if (length == 0) {
		ND_PRINT("ICMP6, length 0");
		nd_print_invalid(ndo);
		return;
	}

	if (ndo->ndo_vflag && !fragmented) {
		uint16_t sum, udp_sum;

		if (ND_TTEST_LEN(bp, length)) {
			udp_sum = GET_BE_U_2(dp->icmp6_cksum);
			sum = icmp6_cksum(ndo, ip, dp, length);
			if (sum != 0)
				ND_PRINT("[bad icmp6 cksum 0x%04x -> 0x%04x!] ",
                                                udp_sum,
                                                in_cksum_shouldbe(udp_sum, sum));
			else
				ND_PRINT("[icmp6 sum ok] ");
		}
	}

	icmp6_type = GET_U_1(dp->icmp6_type);
	ND_PRINT("ICMP6, %s", tok2str(icmp6_type_values,"unknown icmp6 type (%u)",icmp6_type));

        /* display cosmetics: print the packet length for printer that use the vflag now */
        if (ndo->ndo_vflag && (icmp6_type == ND_ROUTER_SOLICIT ||
                      icmp6_type == ND_ROUTER_ADVERT ||
                      icmp6_type == ND_NEIGHBOR_ADVERT ||
                      icmp6_type == ND_NEIGHBOR_SOLICIT ||
                      icmp6_type == ND_REDIRECT ||
                      icmp6_type == ICMP6_HADISCOV_REPLY ||
                      icmp6_type == ICMP6_MOBILEPREFIX_ADVERT ))
                ND_PRINT(", length %u", length);

	icmp6_code = GET_U_1(dp->icmp6_code);

	switch (icmp6_type) {
	case ICMP6_DST_UNREACH:
                ND_PRINT(", %s", tok2str(icmp6_dst_unreach_code_values,"unknown unreach code (%u)",icmp6_code));
		switch (icmp6_code) {

		case ICMP6_DST_UNREACH_NOROUTE: /* fall through */
		case ICMP6_DST_UNREACH_ADMIN:
		case ICMP6_DST_UNREACH_ADDR:
                        ND_PRINT(" %s",GET_IP6ADDR_STRING(oip->ip6_dst));
                        break;
		case ICMP6_DST_UNREACH_BEYONDSCOPE:
			ND_PRINT(" %s, source address %s",
			       GET_IP6ADDR_STRING(oip->ip6_dst),
                                  GET_IP6ADDR_STRING(oip->ip6_src));
			break;
		case ICMP6_DST_UNREACH_NOPORT:
			if ((ouh = get_upperlayer(ndo, (const u_char *)oip, &prot))
			    == NULL)
				goto trunc;

			dport = GET_BE_U_2(ouh->uh_dport);
			switch (prot) {
			case IPPROTO_TCP:
				ND_PRINT(", %s tcp port %s",
					GET_IP6ADDR_STRING(oip->ip6_dst),
                                          tcpport_string(ndo, dport));
				break;
			case IPPROTO_UDP:
				ND_PRINT(", %s udp port %s",
					GET_IP6ADDR_STRING(oip->ip6_dst),
                                          udpport_string(ndo, dport));
				break;
			default:
				ND_PRINT(", %s protocol %u port %u unreachable",
					GET_IP6ADDR_STRING(oip->ip6_dst),
                                          prot, dport);
				break;
			}
			break;
		default:
                  if (ndo->ndo_vflag <= 1) {
                    print_unknown_data(ndo, bp,"\n\t",length);
                    return;
                  }
                    break;
		}
		break;
	case ICMP6_PACKET_TOO_BIG:
		ND_PRINT(", mtu %u", GET_BE_U_4(dp->icmp6_mtu));
		break;
	case ICMP6_TIME_EXCEEDED:
		switch (icmp6_code) {
		case ICMP6_TIME_EXCEED_TRANSIT:
			ND_PRINT(" for %s",
                                  GET_IP6ADDR_STRING(oip->ip6_dst));
			break;
		case ICMP6_TIME_EXCEED_REASSEMBLY:
			ND_PRINT(" (reassembly)");
			break;
		default:
                        ND_PRINT(", unknown code (%u)", icmp6_code);
			break;
		}
		break;
	case ICMP6_PARAM_PROB:
		ND_TCHECK_16(oip->ip6_dst);
		switch (icmp6_code) {
		case ICMP6_PARAMPROB_HEADER:
                        ND_PRINT(", erroneous - octet %u",
				 GET_BE_U_4(dp->icmp6_pptr));
                        break;
		case ICMP6_PARAMPROB_NEXTHEADER:
                        ND_PRINT(", next header - octet %u",
				 GET_BE_U_4(dp->icmp6_pptr));
                        break;
		case ICMP6_PARAMPROB_OPTION:
                        ND_PRINT(", option - octet %u",
				 GET_BE_U_4(dp->icmp6_pptr));
                        break;
		case ICMP6_PARAMPROB_FRAGHDRCHAIN:
                        ND_PRINT(", incomplete header chain - octet %u",
				 GET_BE_U_4(dp->icmp6_pptr));
                        break;
		default:
                        ND_PRINT(", code-#%u",
                                  icmp6_code);
                        break;
		}
		break;
	case ICMP6_ECHO_REQUEST:
	case ICMP6_ECHO_REPLY:
                ND_PRINT(", id %u, seq %u", GET_BE_U_2(dp->icmp6_id),
			 GET_BE_U_2(dp->icmp6_seq));
		break;
	case ICMP6_MEMBERSHIP_QUERY:
		if (length == MLD_MINLEN) {
			mld6_print(ndo, (const u_char *)dp);
		} else if (length >= MLDV2_MINLEN) {
			ND_PRINT(" v2");
			mldv2_query_print(ndo, (const u_char *)dp, length);
		} else {
                        ND_PRINT(" unknown-version (len %u) ", length);
		}
		break;
	case ICMP6_MEMBERSHIP_REPORT:
		mld6_print(ndo, (const u_char *)dp);
		break;
	case ICMP6_MEMBERSHIP_REDUCTION:
		mld6_print(ndo, (const u_char *)dp);
		break;
	case ND_ROUTER_SOLICIT:
#define RTSOLLEN 8
		if (ndo->ndo_vflag) {
			if (icmp6_opt_print(ndo, (const u_char *)dp + RTSOLLEN,
					    length - RTSOLLEN) == -1)
				goto trunc;
		}
		break;
	case ND_ROUTER_ADVERT:
#define RTADVLEN 16
		if (ndo->ndo_vflag) {
			const struct nd_router_advert *p;

			p = (const struct nd_router_advert *)dp;
			ND_PRINT("\n\thop limit %u, Flags [%s]"
                                  ", pref %s, router lifetime %us, reachable time %ums, retrans timer %ums",
                                  GET_U_1(p->nd_ra_curhoplimit),
                                  bittok2str(icmp6_opt_ra_flag_values,"none",GET_U_1(p->nd_ra_flags_reserved)),
                                  get_rtpref(GET_U_1(p->nd_ra_flags_reserved)),
                                  GET_BE_U_2(p->nd_ra_router_lifetime),
                                  GET_BE_U_4(p->nd_ra_reachable),
                                  GET_BE_U_4(p->nd_ra_retransmit));

			if (icmp6_opt_print(ndo, (const u_char *)dp + RTADVLEN,
					    length - RTADVLEN) == -1)
				goto trunc;
		}
		break;
	case ND_NEIGHBOR_SOLICIT:
	    {
		const struct nd_neighbor_solicit *p;
		p = (const struct nd_neighbor_solicit *)dp;
		ND_PRINT(", who has %s", GET_IP6ADDR_STRING(p->nd_ns_target));
		if (ndo->ndo_vflag) {
#define NDSOLLEN 24
			if (icmp6_opt_print(ndo, (const u_char *)dp + NDSOLLEN,
					    length - NDSOLLEN) == -1)
				goto trunc;
		}
	    }
		break;
	case ND_NEIGHBOR_ADVERT:
	    {
		const struct nd_neighbor_advert *p;

		p = (const struct nd_neighbor_advert *)dp;
		ND_PRINT(", tgt is %s",
                          GET_IP6ADDR_STRING(p->nd_na_target));
		if (ndo->ndo_vflag) {
                        ND_PRINT(", Flags [%s]",
                                  bittok2str(icmp6_nd_na_flag_values,
                                             "none",
                                             GET_BE_U_4(p->nd_na_flags_reserved)));
#define NDADVLEN 24
			if (icmp6_opt_print(ndo, (const u_char *)dp + NDADVLEN,
					    length - NDADVLEN) == -1)
				goto trunc;
#undef NDADVLEN
		}
	    }
		break;
	case ND_REDIRECT:
	    {
		const struct nd_redirect *p;

		p = (const struct nd_redirect *)dp;
		ND_PRINT(", %s", GET_IP6ADDR_STRING(p->nd_rd_dst));
		ND_PRINT(" to %s", GET_IP6ADDR_STRING(p->nd_rd_target));
#define REDIRECTLEN 40
		if (ndo->ndo_vflag) {
			if (icmp6_opt_print(ndo, (const u_char *)dp + REDIRECTLEN,
					    length - REDIRECTLEN) == -1)
				goto trunc;
#undef REDIRECTLEN
		}
	    }
		break;
	case ICMP6_ROUTER_RENUMBERING:
		icmp6_rrenum_print(ndo, bp, ep);
		break;
	case ICMP6_NI_QUERY:
	case ICMP6_NI_REPLY:
		icmp6_nodeinfo_print(ndo, length, bp, ep);
		break;
	case IND_SOLICIT:
	case IND_ADVERT:
		break;
	case ICMP6_V2_MEMBERSHIP_REPORT:
		mldv2_report_print(ndo, (const u_char *) dp, length);
		break;
	case ICMP6_MOBILEPREFIX_SOLICIT: /* fall through */
	case ICMP6_HADISCOV_REQUEST:
                ND_PRINT(", id 0x%04x", GET_BE_U_2(dp->icmp6_data16[0]));
                break;
	case ICMP6_HADISCOV_REPLY:
		if (ndo->ndo_vflag) {
			const u_char *cp;
			const u_char *p;

			ND_PRINT(", id 0x%04x",
				 GET_BE_U_2(dp->icmp6_data16[0]));
			cp = (const u_char *)dp +
				ND_MIN(length, ND_BYTES_AVAILABLE_AFTER(dp));
			p = (const u_char *)(dp + 1);
			while (p < cp) {
				ND_PRINT(", %s", GET_IP6ADDR_STRING(p));
				p += 16;
			}
		}
		break;
	case ICMP6_MOBILEPREFIX_ADVERT:
		if (ndo->ndo_vflag) {
			uint16_t flags;

			ND_PRINT(", id 0x%04x",
				 GET_BE_U_2(dp->icmp6_data16[0]));
			flags = GET_BE_U_2(dp->icmp6_data16[1]);
			if (flags & 0xc000)
				ND_PRINT(" ");
			if (flags & 0x8000)
				ND_PRINT("M");
			if (flags & 0x4000)
				ND_PRINT("O");
#define MPADVLEN 8
			if (icmp6_opt_print(ndo, (const u_char *)dp + MPADVLEN,
					    length - MPADVLEN) == -1)
				goto trunc;
		}
		break;
        case ND_RPL_MESSAGE:
                /* plus 4, because struct icmp6_hdr contains 4 bytes of icmp payload */
                rpl_print(ndo, icmp6_code, dp->icmp6_data, length-sizeof(struct icmp6_hdr)+4);
                break;
	default:
                ND_PRINT(", length %u", length);
                if (ndo->ndo_vflag <= 1)
                        print_unknown_data(ndo, bp,"\n\t", length);
                return;
        }
        if (!ndo->ndo_vflag)
                ND_PRINT(", length %u", length);
	return;
trunc:
	nd_print_trunc(ndo);
}

static const struct udphdr *
get_upperlayer(netdissect_options *ndo, const u_char *bp, u_int *prot)
{
	const u_char *ep;
	const struct ip6_hdr *ip6 = (const struct ip6_hdr *)bp;
	const struct udphdr *uh;
	const struct ip6_hbh *hbh;
	const struct ip6_frag *fragh;
	const struct ah *ah;
	u_int nh;
	int hlen;

	/* 'ep' points to the end of available data. */
	ep = ndo->ndo_snapend;

	if (!ND_TTEST_1(ip6->ip6_nxt))
		return NULL;

	nh = GET_U_1(ip6->ip6_nxt);
	hlen = sizeof(struct ip6_hdr);

	while (bp < ep) {
		bp += hlen;

		switch(nh) {
		case IPPROTO_UDP:
		case IPPROTO_TCP:
			uh = (const struct udphdr *)bp;
			if (ND_TTEST_2(uh->uh_dport)) {
				*prot = nh;
				return(uh);
			} else
				return(NULL);
			/* NOTREACHED */

		case IPPROTO_HOPOPTS:
		case IPPROTO_DSTOPTS:
		case IPPROTO_ROUTING:
			hbh = (const struct ip6_hbh *)bp;
			if (!ND_TTEST_1(hbh->ip6h_len))
				return(NULL);
			nh = GET_U_1(hbh->ip6h_nxt);
			hlen = (GET_U_1(hbh->ip6h_len) + 1) << 3;
			break;

		case IPPROTO_FRAGMENT: /* this should be odd, but try anyway */
			fragh = (const struct ip6_frag *)bp;
			if (!ND_TTEST_2(fragh->ip6f_offlg))
				return(NULL);
			/* fragments with non-zero offset are meaningless */
			if ((GET_BE_U_2(fragh->ip6f_offlg) & IP6F_OFF_MASK) != 0)
				return(NULL);
			nh = GET_U_1(fragh->ip6f_nxt);
			hlen = sizeof(struct ip6_frag);
			break;

		case IPPROTO_AH:
			ah = (const struct ah *)bp;
			if (!ND_TTEST_1(ah->ah_len))
				return(NULL);
			nh = GET_U_1(ah->ah_nxt);
			hlen = (GET_U_1(ah->ah_len) + 2) << 2;
			break;

		default:	/* unknown or undecodable header */
			*prot = nh; /* meaningless, but set here anyway */
			return(NULL);
		}
	}

	return(NULL);		/* should be notreached, though */
}

static int
icmp6_opt_print(netdissect_options *ndo, const u_char *bp, int resid)
{
	const struct nd_opt_hdr *op;
	uint8_t opt_type;
	u_int opt_len;
	const struct nd_opt_prefix_info *opp;
	const struct nd_opt_mtu *opm;
	const struct nd_opt_rdnss *oprd;
	const struct nd_opt_dnssl *opds;
	const struct nd_opt_advinterval *opa;
	const struct nd_opt_homeagent_info *oph;
	const struct nd_opt_route_info *opri;
	const u_char *cp, *ep, *domp;
	nd_ipv6 in6;
	size_t l;
	u_int i;

	cp = bp;
	/* 'ep' points to the end of available data. */
	ep = ndo->ndo_snapend;

	while (cp < ep) {
		op = (const struct nd_opt_hdr *)cp;

		ND_TCHECK_1(op->nd_opt_len);
		if (resid <= 0)
			return 0;
		opt_type = GET_U_1(op->nd_opt_type);
		opt_len = GET_U_1(op->nd_opt_len);
		if (opt_len == 0)
			goto trunc;
		if (cp + (opt_len << 3) > ep)
			goto trunc;

                ND_PRINT("\n\t  %s option (%u), length %u (%u): ",
                          tok2str(icmp6_opt_values, "unknown", opt_type),
                          opt_type,
                          opt_len << 3,
                          opt_len);

		switch (opt_type) {
		case ND_OPT_SOURCE_LINKADDR:
			l = (opt_len << 3) - 2;
			print_lladdr(ndo, cp + 2, l);
			break;
		case ND_OPT_TARGET_LINKADDR:
			l = (opt_len << 3) - 2;
			print_lladdr(ndo, cp + 2, l);
			break;
		case ND_OPT_PREFIX_INFORMATION:
			opp = (const struct nd_opt_prefix_info *)op;
                        ND_PRINT("%s/%u%s, Flags [%s], valid time %s",
                                  GET_IP6ADDR_STRING(opp->nd_opt_pi_prefix),
                                  GET_U_1(opp->nd_opt_pi_prefix_len),
                                  (opt_len != 4) ? "badlen" : "",
                                  bittok2str(icmp6_opt_pi_flag_values, "none", GET_U_1(opp->nd_opt_pi_flags_reserved)),
                                  get_lifetime(GET_BE_U_4(opp->nd_opt_pi_valid_time)));
                        ND_PRINT(", pref. time %s",
				 get_lifetime(GET_BE_U_4(opp->nd_opt_pi_preferred_time)));
			break;
		case ND_OPT_REDIRECTED_HEADER:
                        print_unknown_data(ndo, bp,"\n\t    ",opt_len<<3);
			/* xxx */
			break;
		case ND_OPT_MTU:
			opm = (const struct nd_opt_mtu *)op;
			ND_PRINT(" %u%s",
                               GET_BE_U_4(opm->nd_opt_mtu_mtu),
                               (opt_len != 1) ? "bad option length" : "" );
                        break;
		case ND_OPT_RDNSS:
			oprd = (const struct nd_opt_rdnss *)op;
			l = (opt_len - 1) / 2;
			ND_PRINT(" lifetime %us,",
                                  GET_BE_U_4(oprd->nd_opt_rdnss_lifetime));
			for (i = 0; i < l; i++) {
				ND_PRINT(" addr: %s",
                                          GET_IP6ADDR_STRING(oprd->nd_opt_rdnss_addr[i]));
			}
			break;
		case ND_OPT_DNSSL:
			opds = (const struct nd_opt_dnssl *)op;
			ND_PRINT(" lifetime %us, domain(s):",
                                  GET_BE_U_4(opds->nd_opt_dnssl_lifetime));
			domp = cp + 8; /* domain names, variable-sized, RFC1035-encoded */
			while (domp < cp + (opt_len << 3) && GET_U_1(domp) != '\0') {
				ND_PRINT(" ");
				if ((domp = fqdn_print(ndo, domp, bp)) == NULL)
					goto trunc;
			}
			break;
		case ND_OPT_ADVINTERVAL:
			opa = (const struct nd_opt_advinterval *)op;
			ND_PRINT(" %ums",
				 GET_BE_U_4(opa->nd_opt_adv_interval));
			break;
                case ND_OPT_HOMEAGENT_INFO:
			oph = (const struct nd_opt_homeagent_info *)op;
			ND_PRINT(" preference %u, lifetime %u",
                                  GET_BE_U_2(oph->nd_opt_hai_preference),
                                  GET_BE_U_2(oph->nd_opt_hai_lifetime));
			break;
		case ND_OPT_ROUTE_INFO:
			opri = (const struct nd_opt_route_info *)op;
			ND_TCHECK_4(opri->nd_opt_rti_lifetime);
			memset(&in6, 0, sizeof(in6));
			switch (opt_len) {
			case 1:
				break;
			case 2:
				GET_CPY_BYTES(&in6, opri + 1, 8);
				break;
			case 3:
				GET_CPY_BYTES(&in6, opri + 1, 16);
				break;
			default:
				goto trunc;
			}
			ND_PRINT(" %s/%u", ip6addr_string(ndo, (const u_char *)&in6), /* local buffer, not packet data; don't use GET_IP6ADDR_STRING() */
                                  GET_U_1(opri->nd_opt_rti_prefixlen));
			ND_PRINT(", pref=%s",
				 get_rtpref(GET_U_1(opri->nd_opt_rti_flags)));
			ND_PRINT(", lifetime=%s",
                                  get_lifetime(GET_BE_U_4(opri->nd_opt_rti_lifetime)));
			break;
		default:
                        if (ndo->ndo_vflag <= 1) {
                                print_unknown_data(ndo,cp+2,"\n\t  ", (opt_len << 3) - 2); /* skip option header */
                            return 0;
                        }
                        break;
		}
                /* do we want to see an additional hexdump ? */
                if (ndo->ndo_vflag> 1)
                        print_unknown_data(ndo, cp+2,"\n\t    ", (opt_len << 3) - 2); /* skip option header */

		cp += opt_len << 3;
		resid -= opt_len << 3;
	}
	return 0;

trunc:
	return -1;
}

static void
mld6_print(netdissect_options *ndo, const u_char *bp)
{
	const struct mld6_hdr *mp = (const struct mld6_hdr *)bp;
	const u_char *ep;

	/* 'ep' points to the end of available data. */
	ep = ndo->ndo_snapend;

	if ((const u_char *)mp + sizeof(*mp) > ep)
		return;

	ND_PRINT("max resp delay: %u ", GET_BE_U_2(mp->mld6_maxdelay));
	ND_PRINT("addr: %s", GET_IP6ADDR_STRING(mp->mld6_addr));
}

static void
mldv2_report_print(netdissect_options *ndo, const u_char *bp, u_int len)
{
    const struct icmp6_hdr *icp = (const struct icmp6_hdr *) bp;
    u_int group, nsrcs, ngroups;
    u_int i, j;

    /* Minimum len is 8 */
    if (len < 8) {
            ND_PRINT(" [invalid len %u]", len);
            return;
    }

    ngroups = GET_BE_U_2(icp->icmp6_data16[1]);
    ND_PRINT(", %u group record(s)", ngroups);
    if (ndo->ndo_vflag > 0) {
	/* Print the group records */
	group = 8;
        for (i = 0; i < ngroups; i++) {
	    /* type(1) + auxlen(1) + numsrc(2) + grp(16) */
	    if (len < group + 20) {
                    ND_PRINT(" [invalid number of groups]");
                    return;
	    }
            ND_PRINT(" [gaddr %s", GET_IP6ADDR_STRING(bp + group + 4));
	    ND_PRINT(" %s", tok2str(mldv2report2str, " [v2-report-#%u]",
                                         GET_U_1(bp + group)));
            nsrcs = GET_BE_U_2(bp + group + 2);
	    /* Check the number of sources and print them */
	    if (len < group + 20 + (nsrcs * sizeof(nd_ipv6))) {
                    ND_PRINT(" [invalid number of sources %u]", nsrcs);
                    return;
	    }
            if (ndo->ndo_vflag == 1)
                    ND_PRINT(", %u source(s)", nsrcs);
            else {
		/* Print the sources */
                    ND_PRINT(" {");
                for (j = 0; j < nsrcs; j++) {
		    ND_PRINT(" %s", GET_IP6ADDR_STRING(bp + group + 20 + (j * sizeof(nd_ipv6))));
		}
                ND_PRINT(" }");
            }
	    /* Next group record */
            group += 20 + nsrcs * sizeof(nd_ipv6);
	    ND_PRINT("]");
        }
    }
}

static void
mldv2_query_print(netdissect_options *ndo, const u_char *bp, u_int len)
{
    const struct icmp6_hdr *icp = (const struct icmp6_hdr *) bp;
    u_int mrc;
    u_int mrt, qqi;
    u_int nsrcs;
    u_int i;

    /* Minimum len is 28 */
    if (len < 28) {
        ND_PRINT(" [invalid len %u]", len);
	return;
    }
    mrc = GET_BE_U_2(icp->icmp6_data16[0]);
    if (mrc < 32768) {
	mrt = mrc;
    } else {
        mrt = ((mrc & 0x0fff) | 0x1000) << (((mrc & 0x7000) >> 12) + 3);
    }
    if (ndo->ndo_vflag) {
            ND_PRINT(" [max resp delay=%u]", mrt);
    }
    ND_PRINT(" [gaddr %s", GET_IP6ADDR_STRING(bp + 8));

    if (ndo->ndo_vflag) {
	if (GET_U_1(bp + 24) & 0x08) {
		ND_PRINT(" sflag");
	}
	if (GET_U_1(bp + 24) & 0x07) {
		ND_PRINT(" robustness=%u", GET_U_1(bp + 24) & 0x07);
	}
	if (GET_U_1(bp + 25) < 128) {
		qqi = GET_U_1(bp + 25);
	} else {
		qqi = ((GET_U_1(bp + 25) & 0x0f) | 0x10) <<
		       (((GET_U_1(bp + 25) & 0x70) >> 4) + 3);
	}
	ND_PRINT(" qqi=%u", qqi);
    }

    nsrcs = GET_BE_U_2(bp + 26);
    if (nsrcs > 0) {
	if (len < 28 + nsrcs * sizeof(nd_ipv6))
	    ND_PRINT(" [invalid number of sources]");
	else if (ndo->ndo_vflag > 1) {
	    ND_PRINT(" {");
	    for (i = 0; i < nsrcs; i++) {
		ND_PRINT(" %s", GET_IP6ADDR_STRING(bp + 28 + (i * sizeof(nd_ipv6))));
	    }
	    ND_PRINT(" }");
	} else
                ND_PRINT(", %u source(s)", nsrcs);
    }
    ND_PRINT("]");
}

static void
dnsname_print(netdissect_options *ndo, const u_char *cp, const u_char *ep)
{
	int i;

	/* DNS name decoding - no decompression */
	ND_PRINT(", \"");
	while (cp < ep) {
		i = GET_U_1(cp);
		cp++;
		if (i) {
			if (i > ep - cp) {
				ND_PRINT("???");
				break;
			}
			while (i-- && cp < ep) {
				fn_print_char(ndo, GET_U_1(cp));
				cp++;
			}
			if (cp + 1 < ep && GET_U_1(cp))
				ND_PRINT(".");
		} else {
			if (cp == ep) {
				/* FQDN */
				ND_PRINT(".");
			} else if (cp + 1 == ep && GET_U_1(cp) == '\0') {
				/* truncated */
			} else {
				/* invalid */
				ND_PRINT("???");
			}
			break;
		}
	}
	ND_PRINT("\"");
}

static void
icmp6_nodeinfo_print(netdissect_options *ndo, u_int icmp6len, const u_char *bp, const u_char *ep)
{
	const struct icmp6_nodeinfo *ni6;
	const struct icmp6_hdr *dp;
	const u_char *cp;
	size_t siz, i;
	int needcomma;

	if (ep < bp)
		return;
	dp = (const struct icmp6_hdr *)bp;
	ni6 = (const struct icmp6_nodeinfo *)bp;
	siz = ep - bp;

	switch (GET_U_1(ni6->ni_type)) {
	case ICMP6_NI_QUERY:
		if (siz == sizeof(*dp) + 4) {
			/* KAME who-are-you */
			ND_PRINT(" who-are-you request");
			break;
		}
		ND_PRINT(" node information query");

		ND_TCHECK_LEN(dp, sizeof(*ni6));
		ni6 = (const struct icmp6_nodeinfo *)dp;
		ND_PRINT(" (");	/*)*/
		switch (GET_BE_U_2(ni6->ni_qtype)) {
		case NI_QTYPE_NOOP:
			ND_PRINT("noop");
			break;
		case NI_QTYPE_SUPTYPES:
			ND_PRINT("supported qtypes");
			i = GET_BE_U_2(ni6->ni_flags);
			if (i)
				ND_PRINT(" [%s]", (i & 0x01) ? "C" : "");
			break;
		case NI_QTYPE_FQDN:
			ND_PRINT("DNS name");
			break;
		case NI_QTYPE_NODEADDR:
			ND_PRINT("node addresses");
			i = GET_BE_U_2(ni6->ni_flags);
			if (!i)
				break;
			/* NI_NODEADDR_FLAG_TRUNCATE undefined for query */
			ND_PRINT(" [%s%s%s%s%s%s]",
			    (i & NI_NODEADDR_FLAG_ANYCAST) ? "a" : "",
			    (i & NI_NODEADDR_FLAG_GLOBAL) ? "G" : "",
			    (i & NI_NODEADDR_FLAG_SITELOCAL) ? "S" : "",
			    (i & NI_NODEADDR_FLAG_LINKLOCAL) ? "L" : "",
			    (i & NI_NODEADDR_FLAG_COMPAT) ? "C" : "",
			    (i & NI_NODEADDR_FLAG_ALL) ? "A" : "");
			break;
		default:
			ND_PRINT("unknown");
			break;
		}

		if (GET_BE_U_2(ni6->ni_qtype) == NI_QTYPE_NOOP ||
		    GET_BE_U_2(ni6->ni_qtype) == NI_QTYPE_SUPTYPES) {
			if (siz != sizeof(*ni6))
				if (ndo->ndo_vflag)
					ND_PRINT(", invalid len");
			/*(*/
			ND_PRINT(")");
			break;
		}

		/* XXX backward compat, icmp-name-lookup-03 */
		if (siz == sizeof(*ni6)) {
			ND_PRINT(", 03 draft");
			/*(*/
			ND_PRINT(")");
			break;
		}

		cp = (const u_char *)(ni6 + 1);
		switch (GET_U_1(ni6->ni_code)) {
		case ICMP6_NI_SUBJ_IPV6:
			if (!ND_TTEST_LEN(dp, sizeof(*ni6) + sizeof(nd_ipv6)))
				break;
			if (siz != sizeof(*ni6) + sizeof(nd_ipv6)) {
				if (ndo->ndo_vflag)
					ND_PRINT(", invalid subject len");
				break;
			}
			ND_PRINT(", subject=%s",
                                  GET_IP6ADDR_STRING(cp));
			break;
		case ICMP6_NI_SUBJ_FQDN:
			ND_PRINT(", subject=DNS name");
			if (GET_U_1(cp) == ep - cp - 1) {
				/* icmp-name-lookup-03, pascal string */
				if (ndo->ndo_vflag)
					ND_PRINT(", 03 draft");
				cp++;
				ND_PRINT(", \"");
				while (cp < ep) {
					fn_print_char(ndo, GET_U_1(cp));
					cp++;
				}
				ND_PRINT("\"");
			} else
				dnsname_print(ndo, cp, ep);
			break;
		case ICMP6_NI_SUBJ_IPV4:
			if (!ND_TTEST_LEN(dp, sizeof(*ni6) + sizeof(nd_ipv4)))
				break;
			if (siz != sizeof(*ni6) + sizeof(nd_ipv4)) {
				if (ndo->ndo_vflag)
					ND_PRINT(", invalid subject len");
				break;
			}
			ND_PRINT(", subject=%s",
                                  GET_IPADDR_STRING(cp));
			break;
		default:
			ND_PRINT(", unknown subject");
			break;
		}

		/*(*/
		ND_PRINT(")");
		break;

	case ICMP6_NI_REPLY:
		if (icmp6len > siz)
			goto trunc;

		needcomma = 0;

		ND_TCHECK_LEN(dp, sizeof(*ni6));
		ni6 = (const struct icmp6_nodeinfo *)dp;
		ND_PRINT(" node information reply");
		ND_PRINT(" (");	/*)*/
		switch (GET_U_1(ni6->ni_code)) {
		case ICMP6_NI_SUCCESS:
			if (ndo->ndo_vflag) {
				ND_PRINT("success");
				needcomma++;
			}
			break;
		case ICMP6_NI_REFUSED:
			ND_PRINT("refused");
			needcomma++;
			if (siz != sizeof(*ni6))
				if (ndo->ndo_vflag)
					ND_PRINT(", invalid length");
			break;
		case ICMP6_NI_UNKNOWN:
			ND_PRINT("unknown");
			needcomma++;
			if (siz != sizeof(*ni6))
				if (ndo->ndo_vflag)
					ND_PRINT(", invalid length");
			break;
		}

		if (GET_U_1(ni6->ni_code) != ICMP6_NI_SUCCESS) {
			/*(*/
			ND_PRINT(")");
			break;
		}

		switch (GET_BE_U_2(ni6->ni_qtype)) {
		case NI_QTYPE_NOOP:
			if (needcomma)
				ND_PRINT(", ");
			ND_PRINT("noop");
			if (siz != sizeof(*ni6))
				if (ndo->ndo_vflag)
					ND_PRINT(", invalid length");
			break;
		case NI_QTYPE_SUPTYPES:
			if (needcomma)
				ND_PRINT(", ");
			ND_PRINT("supported qtypes");
			i = GET_BE_U_2(ni6->ni_flags);
			if (i)
				ND_PRINT(" [%s]", (i & 0x01) ? "C" : "");
			break;
		case NI_QTYPE_FQDN:
			if (needcomma)
				ND_PRINT(", ");
			ND_PRINT("DNS name");
			cp = (const u_char *)(ni6 + 1) + 4;
			if (GET_U_1(cp) == ep - cp - 1) {
				/* icmp-name-lookup-03, pascal string */
				if (ndo->ndo_vflag)
					ND_PRINT(", 03 draft");
				cp++;
				ND_PRINT(", \"");
				while (cp < ep) {
					fn_print_char(ndo, GET_U_1(cp));
					cp++;
				}
				ND_PRINT("\"");
			} else
				dnsname_print(ndo, cp, ep);
			if ((GET_BE_U_2(ni6->ni_flags) & 0x01) != 0)
				ND_PRINT(" [TTL=%u]", GET_BE_U_4(ni6 + 1));
			break;
		case NI_QTYPE_NODEADDR:
			if (needcomma)
				ND_PRINT(", ");
			ND_PRINT("node addresses");
			i = sizeof(*ni6);
			while (i < siz) {
				if (i + sizeof(uint32_t) + sizeof(nd_ipv6) > siz)
					break;
				ND_PRINT(" %s(%u)",
				    GET_IP6ADDR_STRING(bp + i + sizeof(uint32_t)),
				    GET_BE_U_4(bp + i));
				i += sizeof(uint32_t) + sizeof(nd_ipv6);
			}
			i = GET_BE_U_2(ni6->ni_flags);
			if (!i)
				break;
			ND_PRINT(" [%s%s%s%s%s%s%s]",
                                  (i & NI_NODEADDR_FLAG_ANYCAST) ? "a" : "",
                                  (i & NI_NODEADDR_FLAG_GLOBAL) ? "G" : "",
                                  (i & NI_NODEADDR_FLAG_SITELOCAL) ? "S" : "",
                                  (i & NI_NODEADDR_FLAG_LINKLOCAL) ? "L" : "",
                                  (i & NI_NODEADDR_FLAG_COMPAT) ? "C" : "",
                                  (i & NI_NODEADDR_FLAG_ALL) ? "A" : "",
                                  (i & NI_NODEADDR_FLAG_TRUNCATE) ? "T" : "");
			break;
		default:
			if (needcomma)
				ND_PRINT(", ");
			ND_PRINT("unknown");
			break;
		}

		/*(*/
		ND_PRINT(")");
		break;
	}
	return;

trunc:
	nd_print_trunc(ndo);
}

static void
icmp6_rrenum_print(netdissect_options *ndo, const u_char *bp, const u_char *ep)
{
	const struct icmp6_router_renum *rr6;
	const char *cp;
	const struct rr_pco_match *match;
	const struct rr_pco_use *use;
	char hbuf[NI_MAXHOST];
	int n;

	if (ep < bp)
		return;
	rr6 = (const struct icmp6_router_renum *)bp;
	cp = (const char *)(rr6 + 1);

	ND_TCHECK_4(rr6->rr_reserved);
	switch (GET_U_1(rr6->rr_code)) {
	case ICMP6_ROUTER_RENUMBERING_COMMAND:
		ND_PRINT(", command");
		break;
	case ICMP6_ROUTER_RENUMBERING_RESULT:
		ND_PRINT(", result");
		break;
	case ICMP6_ROUTER_RENUMBERING_SEQNUM_RESET:
		ND_PRINT(", sequence number reset");
		break;
	default:
		ND_PRINT(", code-#%u", GET_U_1(rr6->rr_code));
		break;
	}

        ND_PRINT(", seq=%u", GET_BE_U_4(rr6->rr_seqnum));

	if (ndo->ndo_vflag) {
		uint8_t rr_flags = GET_U_1(rr6->rr_flags);
#define F(x, y)	(rr_flags & (x) ? (y) : "")
		ND_PRINT("[");	/*]*/
		if (rr_flags) {
			ND_PRINT("%s%s%s%s%s,", F(ICMP6_RR_FLAGS_TEST, "T"),
                                  F(ICMP6_RR_FLAGS_REQRESULT, "R"),
                                  F(ICMP6_RR_FLAGS_FORCEAPPLY, "A"),
                                  F(ICMP6_RR_FLAGS_SPECSITE, "S"),
                                  F(ICMP6_RR_FLAGS_PREVDONE, "P"));
		}
                ND_PRINT("seg=%u,", GET_U_1(rr6->rr_segnum));
                ND_PRINT("maxdelay=%u", GET_BE_U_2(rr6->rr_maxdelay));
		if (GET_BE_U_4(rr6->rr_reserved))
			ND_PRINT("rsvd=0x%x", GET_BE_U_4(rr6->rr_reserved));
		/*[*/
		ND_PRINT("]");
#undef F
	}

	if (GET_U_1(rr6->rr_code) == ICMP6_ROUTER_RENUMBERING_COMMAND) {
		match = (const struct rr_pco_match *)cp;
		cp = (const char *)(match + 1);

		ND_TCHECK_16(match->rpm_prefix);

		if (ndo->ndo_vflag > 1)
			ND_PRINT("\n\t");
		else
			ND_PRINT(" ");
		ND_PRINT("match(");	/*)*/
		switch (GET_U_1(match->rpm_code)) {
		case RPM_PCO_ADD:	ND_PRINT("add"); break;
		case RPM_PCO_CHANGE:	ND_PRINT("change"); break;
		case RPM_PCO_SETGLOBAL:	ND_PRINT("setglobal"); break;
		default:		ND_PRINT("#%u",
						 GET_U_1(match->rpm_code)); break;
		}

		if (ndo->ndo_vflag) {
			ND_PRINT(",ord=%u", GET_U_1(match->rpm_ordinal));
			ND_PRINT(",min=%u", GET_U_1(match->rpm_minlen));
			ND_PRINT(",max=%u", GET_U_1(match->rpm_maxlen));
		}
		if (addrtostr6(match->rpm_prefix, hbuf, sizeof(hbuf)))
			ND_PRINT(",%s/%u", hbuf, GET_U_1(match->rpm_matchlen));
		else
			ND_PRINT(",?/%u", GET_U_1(match->rpm_matchlen));
		/*(*/
		ND_PRINT(")");

		n = GET_U_1(match->rpm_len) - 3;
		if (n % 4)
			goto trunc;
		n /= 4;
		while (n-- > 0) {
			use = (const struct rr_pco_use *)cp;
			cp = (const char *)(use + 1);

			ND_TCHECK_16(use->rpu_prefix);

			if (ndo->ndo_vflag > 1)
				ND_PRINT("\n\t");
			else
				ND_PRINT(" ");
			ND_PRINT("use(");	/*)*/
			if (GET_U_1(use->rpu_flags)) {
#define F(x, y)	(GET_U_1(use->rpu_flags) & (x) ? (y) : "")
				ND_PRINT("%s%s,",
                                          F(ICMP6_RR_PCOUSE_FLAGS_DECRVLTIME, "V"),
                                          F(ICMP6_RR_PCOUSE_FLAGS_DECRPLTIME, "P"));
#undef F
			}
			if (ndo->ndo_vflag) {
				ND_PRINT("mask=0x%x,",
					 GET_U_1(use->rpu_ramask));
				ND_PRINT("raflags=0x%x,",
					 GET_U_1(use->rpu_raflags));
				if (GET_BE_U_4(use->rpu_vltime) == 0xffffffff)
					ND_PRINT("vltime=infty,");
				else
					ND_PRINT("vltime=%u,",
                                                  GET_BE_U_4(use->rpu_vltime));
				if (GET_BE_U_4(use->rpu_pltime) == 0xffffffff)
					ND_PRINT("pltime=infty,");
				else
					ND_PRINT("pltime=%u,",
                                                  GET_BE_U_4(use->rpu_pltime));
			}
			if (addrtostr6(use->rpu_prefix, hbuf, sizeof(hbuf)))
				ND_PRINT("%s/%u/%u", hbuf,
                                          GET_U_1(use->rpu_uselen),
                                          GET_U_1(use->rpu_keeplen));
			else
				ND_PRINT("?/%u/%u", GET_U_1(use->rpu_uselen),
                                          GET_U_1(use->rpu_keeplen));
			/*(*/
                        ND_PRINT(")");
		}
	}

	return;

trunc:
	nd_print_trunc(ndo);
}
