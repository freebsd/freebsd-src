/* @(#) $Header: /tcpdump/master/tcpdump/Attic/dhcp6.h,v 1.4 2000/12/17 23:07:48 guy Exp $ (LBL) */
/*
 * Copyright (C) 1998 and 1999 WIDE Project.
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
 * draft-ietf-dhc-dhcpv6-15
 */

#ifndef __DHCP6_H_DEFINED
#define __DHCP6_H_DEFINED

/* Error Values */
#define DH6ERR_FAILURE		16
#define DH6ERR_AUTHFAIL		17
#define DH6ERR_POORLYFORMED	18
#define DH6ERR_UNAVAIL		19
#define DH6ERR_NOBINDING	20
#define DH6ERR_INVALIDSOURCE	21
#define DH6ERR_NOSERVER		23
#define DH6ERR_ICMPERROR	64

/* Message type */
#define DH6_SOLICIT	1
#define DH6_ADVERT	2
#define DH6_REQUEST	3
#define DH6_REPLY	4
#define DH6_RELEASE	5
#define DH6_RECONFIG	6

/* Predefined addresses */
#define DH6ADDR_ALLAGENT	"ff02::1:2"
#define DH6ADDR_ALLSERVER	"ff05::1:3"
#define DH6ADDR_ALLRELAY	"ff05::1:4"
#define DH6PORT_DOWNSTREAM	"546"
#define DH6PORT_UPSTREAM	"547"

/* Protocol constants */
#define ADV_CLIENT_WAIT		2	/* sec */
#define DEFAULT_SOLICIT_HOPCOUNT	4
#define SERVER_MIN_ADV_DELAY	100	/* msec */
#define SERVER_MAX_ADV_DELAY	1000	/* msec */
#define REPLY_MSG_TIMEOUT	2	/* sec */
#define REQUEST_MSG_MIN_RETRANS	10	/* retransmissions */
#define RECONF_MSG_MIN_RETRANS	10	/* retransmissions */
#define RECONF_MSG_RETRANS_INTERVAL	12	/* sec */
#define RECONF_MMSG_MIN_RESP	2	/* sec */
#define RECONF_MMSG_MAX_RESP	10	/* sec */
#define RECONF_MULTICAST_REQUEST_WAIT	120	/* sec */
#define MIN_SOLICIT_DELAY	1	/* sec */
#define MAX_SOLICIT_DELAY	5	/* sec */
#define XID_TIMEOUT		600	/* sec */

/* DHCP6 base packet format */
struct dhcp6_solicit {
	u_int8_t dh6sol_msgtype;		/* DH6_SOLICIT */
	u_int8_t dh6sol_flags;
#define DH6SOL_CLOSE	0x80
#define DH6SOL_PREFIX	0x40
	/* XXX: solicit-ID is a 9-bit field...ugly! */
#define DH6SOL_SOLICIT_ID_MASK 0x01ff
#define DH6SOL_SOLICIT_ID_SHIFT 0
#define DH6SOL_SOLICIT_ID(x) \
    (((x) & DH6SOL_SOLICIT_ID_MASK) >> DH6SOL_SOLICIT_ID_SHIFT)
#define DH6SOL_SOLICIT_PLEN_MASK 0xfe00
#define DH6SOL_SOLICIT_PLEN_SHIFT 9
#define DH6SOL_SOLICIT_PLEN(x) \
    (((x) & DH6SOL_SOLICIT_PLEN_MASK) >> DH6SOL_SOLICIT_PLEN_SHIFT)
	u_int16_t dh6sol_plen_id; /* prefix-len and solict-ID */
	struct in6_addr dh6sol_cliaddr;	/* client's lladdr */
	struct in6_addr dh6sol_relayaddr; /* relay agent's lladdr */
};

struct dhcp6_advert {
	u_int8_t dh6adv_msgtype;		/* DH6_ADVERT */
	u_int8_t dh6adv_rsv_id;	/* reserved and uppermost bit of ID */
	u_int8_t dh6adv_solcit_id; /* lower 8 bits of solicit-ID */
	u_int8_t dh6adv_pref;
	struct in6_addr dh6adv_cliaddr;	/* client's link-local addr */
	struct in6_addr dh6adv_relayaddr; /* relay agent's (non-ll) addr */
	struct in6_addr dh6adv_serveraddr; /* server's addr */
	/* extensions */
};

struct dhcp6_request {
	u_int8_t dh6req_msgtype;		/* DH6_REQUEST */
	u_int8_t dh6req_flags;
#define DH6REQ_CLOSE		0x80
#define DH6REQ_REBOOT		0x40
	u_int16_t dh6req_xid;		/* transaction-ID */
	struct in6_addr dh6req_cliaddr;	/* client's lladdr */
	struct in6_addr dh6req_relayaddr; /* relay agent's (non-ll) addr */
	struct in6_addr dh6req_serveraddr; /* server's addr */
	/* extensions */
};

struct dhcp6_reply {
	u_int8_t dh6rep_msgtype;		/* DH6_REPLY */
	u_int8_t dh6rep_flagandstat;
#define DH6REP_RELAYPRESENT	0x80
#define DH6REP_STATMASK		0x7f
	u_int16_t dh6rep_xid;		/* transaction-ID */
	struct in6_addr dh6rep_cliaddr;	/* client's lladdr */
	/* struct in6_addr dh6rep_relayaddr; optional: relay address */
	/* extensions */
};

/* XXX: followings are based on older drafts */
struct dhcp6_release {
	u_int8_t dh6rel_msgtype;		/* DH6_RELEASE */
	u_int8_t dh6rel_flags;
#define DH6REL_DIRECT	0x80
	u_int16_t dh6rel_xid;		/* transaction-ID */
	struct in6_addr dh6rel_cliaddr;	/* client's lladdr */
	struct in6_addr dh6rel_relayaddr; /* relay agent's (non-ll) addr */
	struct in6_addr dh6rel_reladdr; /* server's addr to be released */
	/* extensions */
};

struct dhcp6_reconfig {
	u_int8_t dh6cfg_msgtype;		/* DH6_RECONFIG */
	u_int8_t dh6cfg_flags;
#define DH6REP_NOREPLY	0x80
	u_int16_t dh6cfg_xid;		/* transaction-ID */
	struct in6_addr dh6cfg_servaddr; /* server's addr */
	/* extensions */
};

union dhcp6 {
	u_int8_t dh6_msgtype;
	struct dhcp6_solicit dh6_sol;
	struct dhcp6_advert dh6_adv;
	struct dhcp6_request dh6_req;
	struct dhcp6_reply dh6_rep;
	struct dhcp6_release dh6_rel;
	struct dhcp6_reconfig dh6_cfg;
};

/* DHCP6 extension */
struct dhcp6e_ipaddr {
	u_int16_t dh6eip_type;
	u_int16_t dh6eip_len;
	u_int8_t dh6eip_status;
#define DH6EX_IP_SUCCESS	0	/* request granted, no errors */
#define DH6EX_IP_SECFAIL	18	/* Security parameters failed */
#define DH6EX_IP_AAAAFAIL	20	/* AAAA Record Parameter Problem */
#define DH6EX_IP_PTRFAIL	21	/* PTR Record Parameter Problem */
#define DH6EX_IP_PARAMFAIL	22	/* Unable to honor required params */
#define DH6EX_IP_DNSNAMEFAIL	23	/* DNS name string error */
#define DH6EX_IP_NODYNDNS	24	/* dynDNS Not Implemented */
#define DH6EX_IP_NOAUTHDNS	25	/* Authoritative DNS Server not found */
#define DH6EX_IP_DNSFORMFAIL	33	/* DNS format error */
#define DH6EX_IP_SERVFAIL	34	/* dynDNS unavailable at this time */
#define DH6EX_IP_NXDOMAIN	35	/* name does not exist */
#define DH6EX_IP_NOTIMP		36	/* DNS does not support the Opcode */
#define DH6EX_IP_REFUSED	37	/* DNS refuses specified operation */
#define DH6EX_IP_YXDOMAIN	38	/* name does not exist */
#define DH6EX_IP_YXRRSET	39	/* RRset does not exist */
#define DH6EX_IP_NXRRSET	40	/* RRset does not exist */
#define DH6EX_IP_NOTAUTH	41	/* non authoritative name server */
#define DH6EX_IP_NOTZONE	42	/* prerequisite out of zone */
	u_int8_t dh6eip_flags;
#define DH6EX_IP_CLIANTADDR	0x80	/* C: cliant's addr */
#define DH6EX_IP_LIFETIME	0x40	/* L: preferred/valid lifetime */
#define DH6EX_IP_FORCEOPTS	0x20	/* Q: options are mandatory */
#define DH6EX_IP_AAAA		0x10	/* A: DNS dynamic update for AAAA */
#define DH6EX_IP_PTR		0x08	/* P: DNS dynamic update for PTR*/
	u_int8_t dh6eip_pad;
	u_int8_t dh6eip_prefixlen;
	/* struct in6_addr: client's address (if C bit = 1) */
	/* u_int: preferred lifetime (if L bit = 1) */
	/* u_int: valid lifetime (if L bit = 1) */
	/* string: DNS name */
};

#endif /*__DHCP6_H_DEFINED*/
