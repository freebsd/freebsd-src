/*-
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD: src/sys/netatm/spans/spans_cls.h,v 1.4 2005/01/07 01:45:38 imp Exp $
 *
 */

/*
 * SPANS Signalling Manager
 * ---------------------------
 *
 * SPANS Connectionless Datagram Service (CLS) control blocks
 *
 */

#ifndef _SPANS_SPANSCLS_H
#define _SPANS_SPANSCLS_H

/*
 * Protocol constants
 */
#define	SPANSARP_AGING		(60 * ATM_HZ)	/* ARP aging timer */
#define	SPANSARP_RETRY		(3 * ATM_HZ)	/* ARP retry timer */
#define	SPANSARP_MAXAGE		20 		/* Max ARP entry age (minutes)*/
#define	SPANSARP_HASHSIZ	19		/* Hash table size */


/*
 * SPANS CLS protocol structure.  There will be one such structure for 
 * each SPANS signalling instance.
 */
struct spanscls {
	struct spanscls	*cls_next;	/* Next attached cls instance */
	u_char		cls_state;	/* Protocol state (see below) */
	struct spans	*cls_spans;	/* Spans signalling instance */
	Atm_connection	*cls_conn;	/* Connection manager token */
	struct ip_nif	*cls_ipnif;	/* IP network interface */
};

/*
 * SPANS CLS Protocol States
 */
#define	CLS_CLOSED	1		/* CLS PVC is closed */
#define	CLS_OPEN	2		/* CLS PVC is open */


/*
 * Structure for SPANS ARP mappings.  Each of these structures will contain 
 * IP address to SPANS hardware address mappings.  There will be one such
 * structure for each IP address currently in use.
 */
struct spansarp {
	struct arpmap	sa_arpmap;	/* Common entry header */
	struct spanscls	*sa_cls;	/* Interface where we learned answer */
	struct spansarp	*sa_next;	/* Hash chain */
	struct spansarp	*sa_rnext;	/* Retry chain */
	u_char		sa_flags;	/* Flags (see below) */
	u_char		sa_origin;	/* Origin (see below) */
	u_short		sa_reftime;	/* Entry reference time (minutes) */
	struct ipvcc	*sa_ivp;	/* IP VCCs waiting for answer */
};
#define	sa_dstip	sa_arpmap.am_dstip
#define	sa_dstatm	sa_arpmap.am_dstatm
#define	sa_dstatmsub	sa_arpmap.am_dstatmsub

/*
 * Entry Flags
 */
#define	SAF_VALID	ARPF_VALID	/* Entry is valid */
#define	SAF_REFRESH	ARPF_REFRESH	/* Entry has been refreshed */
#define	SAF_LOCKED	0x04		/* Entry is locked */

/*
 * Entry Origin
 */
#define	SAO_PERM	ARP_ORIG_PERM	/* Entry is permanently installed */
#define	SAO_LOOKUP	20		/* Learned via lookup */


/*
 * SPANS CLS Packet Header
 */
struct spanscls_hdr {
	/* IEEE 802.6 MAC header */
	spans_addr	ch_dst;		/* Destination SPANS address */
	spans_addr	ch_src;		/* Source SPANS address */
	u_char		ch_proto;	/* */
	u_char		ch_extlen;	/* */
	u_short		ch_bridging;	/* */

	/* LLC SNAP header */
	u_char		ch_dsap;	/* Destination SAP */
	u_char		ch_ssap;	/* Source SAP */
	u_char		ch_ctl;		/* Control field */
	u_char		ch_oui[3];	/* Organizationally Unique Identifier */
	u_short		ch_pid;		/* Protocol Identifier */
};

/*
 * SPANS ARP Packet Format
 */
struct spansarp_hdr {
	u_short		ah_hrd;		/* Hardware type (see below) */
	u_short		ah_pro;		/* Protocol type */
	u_char		ah_hln;		/* Length of hardware address */
	u_char		ah_pln;		/* Length of protocol address */
	u_short		ah_op;		/* Operation code (see below) */
	spans_addr	ah_sha;		/* Source hardware address */
	u_char		ah_spa[4];	/* Source protocol address */
	spans_addr	ah_tha;		/* Target hardware address */
	u_char		ah_tpa[4];	/* Target protocol address */
};

/*
 * Hardware types
 */
#define	ARP_SPANS	0x4040

/*
 * Operation types
 */
#define	ARP_REQUEST	1		/* SPANSARP request */
#define	ARP_REPLY	2		/* SPANSARP response */

#define	ARP_PACKET_LEN	\
	(sizeof(struct spanscls_hdr) + sizeof(struct spansarp_hdr))

#ifdef _KERNEL
/*
 * Macros for manipulating SPANS ARP tables and entries
 */
#define SPANSARP_HASH(ip)	((u_long)(ip) % SPANSARP_HASHSIZ)

#define	SPANSARP_ADD(sa)						\
{								\
	struct spansarp	**h;					\
	h = &spansarp_arptab[SPANSARP_HASH((sa)->sa_dstip.s_addr)];	\
	LINK2TAIL((sa), struct spansarp, *h, sa_next);		\
}

#define	SPANSARP_DELETE(sa)					\
{								\
	struct spansarp	**h;					\
	h = &spansarp_arptab[SPANSARP_HASH((sa)->sa_dstip.s_addr)];	\
	UNLINK((sa), struct spansarp, *h, sa_next);		\
}

#define	SPANSARP_LOOKUP(ip, sa)					\
{								\
	for ((sa) = spansarp_arptab[SPANSARP_HASH(ip)];		\
				(sa); (sa) = (sa)->sa_next) {	\
		if ((sa)->sa_dstip.s_addr == (ip))		\
			break;					\
	}							\
}


/*
 * External variables
 */
extern struct spanscls		*spanscls_head;
extern struct spanscls_hdr	spanscls_hdr;

#endif	/* _KERNEL */

#endif	/* _SPANS_SPANSCLS_H */
