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
 * RFC3315: DHCPv6
 * supported DHCPv6 options: 
 *  RFC3319,
 *  draft-ietf-dhc-dhcpv6-opt-dnsconfig-04.txt,
 *  draft-ietf-dhc-dhcpv6-opt-prefix-delegation-05.txt
 *  draft-ietf-dhc-dhcpv6-opt-timeconfig-02.txt,
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/print-dhcp6.c,v 1.27.2.4 2003/11/18 23:26:14 guy Exp $";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"

/* lease duration */
#define DHCP6_DURATITION_INFINITE 0xffffffff

/* Error Values */
#define DH6ERR_FAILURE		16
#define DH6ERR_AUTHFAIL		17
#define DH6ERR_POORLYFORMED	18
#define DH6ERR_UNAVAIL		19
#define DH6ERR_OPTUNAVAIL	20

/* Message type */
#define DH6_SOLICIT	1
#define DH6_ADVERTISE	2
#define DH6_REQUEST	3
#define DH6_CONFIRM	4
#define DH6_RENEW	5
#define DH6_REBIND	6
#define DH6_REPLY	7
#define DH6_RELEASE	8
#define DH6_DECLINE	9
#define DH6_RECONFIGURE	10
#define DH6_INFORM_REQ	11
#define DH6_RELAY_FORW	12
#define DH6_RELAY_REPLY	13

/* DHCP6 base packet format */
struct dhcp6 {
	union {
		u_int8_t m;
		u_int32_t x;
	} dh6_msgtypexid;
	/* options follow */
};
#define dh6_msgtype	dh6_msgtypexid.m
#define dh6_xid		dh6_msgtypexid.x
#define DH6_XIDMASK	0x00ffffff

/* DHCPv6 relay messages */
struct dhcp6_relay {
	u_int8_t dh6relay_msgtype;
	u_int8_t dh6relay_hcnt;
	u_int8_t dh6relay_linkaddr[16];	/* XXX: badly aligned */
	u_int8_t dh6relay_peeraddr[16];
	/* options follow */
};

/* options */
#define DH6OPT_CLIENTID	1
#define DH6OPT_SERVERID	2
#define DH6OPT_IA_NA 3
#define DH6OPT_IA_TMP 4
#define DH6OPT_IADDR 5
#define DH6OPT_ORO 6
#define DH6OPT_PREFERENCE 7
#  define DH6OPT_PREF_UNDEF -1
#  define DH6OPT_PREF_MAX 255
#define DH6OPT_ELAPSED_TIME 8
#define DH6OPT_RELAY_MSG 9
/*#define DH6OPT_SERVER_MSG 10 deprecated */
#define DH6OPT_AUTH 11
#define DH6OPT_UNICAST 12
#define DH6OPT_STATUS_CODE 13
#  define DH6OPT_STCODE_SUCCESS 0
#  define DH6OPT_STCODE_UNSPECFAIL 1
#  define DH6OPT_STCODE_NOADDRAVAIL 2
#  define DH6OPT_STCODE_NOBINDING 3
#  define DH6OPT_STCODE_NOTONLINK 4
#  define DH6OPT_STCODE_USEMULTICAST 5
#  define DH6OPT_STCODE_NOPREFIXAVAIL 6
#define DH6OPT_RAPID_COMMIT 14
#define DH6OPT_USER_CLASS 15
#define DH6OPT_VENDOR_CLASS 16
#define DH6OPT_VENDOR_OPTS 17
#define DH6OPT_INTERFACE_ID 18
#define DH6OPT_RECONF_MSG 19
#define DH6OPT_RECONF_ACCEPT 20
#define DH6OPT_SIP_SERVER_D 21
#define DH6OPT_SIP_SERVER_A 22
#define DH6OPT_DNS 23
#define DH6OPT_DNSNAME 24

/*
 * The option type has not been assigned for the following options.
 * We temporarily adopt values used in the service specification document
 * (200206xx version) by NTT Communications.
 * Note that we'll change the following definitions if different type values
 * are officially assigned.
 */
#define DH6OPT_PREFIX_DELEGATION 30
#define DH6OPT_PREFIX_INFORMATION 31
#define DH6OPT_PREFIX_REQUEST 32

/*
 * The followings are also unassigned numbers.
 * We temporarily use values as of KAME snap 20031013.
 */
#define DH6OPT_IA_PD 33
#define DH6OPT_IA_PD_PREFIX 34
#define DH6OPT_NTP_SERVERS 35

struct dhcp6opt {
	u_int16_t dh6opt_type;
	u_int16_t dh6opt_len;
	/* type-dependent data follows */
};

struct dhcp6_ia {
	u_int16_t dh6opt_ia_type;
	u_int16_t dh6opt_ia_len;
	u_int32_t dh6opt_ia_iaid;
	u_int32_t dh6opt_ia_t1;
	u_int32_t dh6opt_ia_t2;
};

struct dhcp6_ia_prefix {
	u_int16_t dh6opt_ia_prefix_type;
	u_int16_t dh6opt_ia_prefix_len;
	u_int32_t dh6opt_ia_prefix_pltime;
	u_int32_t dh6opt_ia_prefix_vltime;
	u_int8_t dh6opt_ia_prefix_plen;
	struct in6_addr dh6opt_ia_prefix_addr;
}  __attribute__ ((__packed__));

static const char *
dhcp6opt_name(int type)
{
	static char genstr[sizeof("opt_65535") + 1]; /* XXX thread unsafe */

	if (type > 65535)
		return "INVALID option";

	switch(type) {
	case DH6OPT_CLIENTID:
		return "client ID";
	case DH6OPT_SERVERID:
		return "server ID";
	case DH6OPT_IA_NA:
		return "IA_NA";
	case DH6OPT_ORO:
		return "option request";
	case DH6OPT_PREFERENCE:
		return "preference";
	case DH6OPT_ELAPSED_TIME:
		return "elapsed time";
	case DH6OPT_RELAY_MSG:
		return "relay message";
	case DH6OPT_STATUS_CODE:
		return "status code";
	case DH6OPT_RAPID_COMMIT:
		return "rapid commit";
	case DH6OPT_INTERFACE_ID:
		return "interface ID";
	case DH6OPT_RECONF_MSG:
		return "reconfigure message";
	case DH6OPT_RECONF_ACCEPT:
		return "reconfigure accept";
	case DH6OPT_SIP_SERVER_D:
		return "SIP Servers Domain";
	case DH6OPT_SIP_SERVER_A:
		return "SIP Servers Address";
	case DH6OPT_DNS:
		return "DNS";
	case DH6OPT_PREFIX_DELEGATION:
		return "prefix delegation";
	case DH6OPT_PREFIX_INFORMATION:
		return "prefix information";
	case DH6OPT_IA_PD:
		return "IA_PD";
	case DH6OPT_IA_PD_PREFIX:
		return "IA_PD prefix";
	case DH6OPT_NTP_SERVERS:
		return "NTP Server";
	default:
		snprintf(genstr, sizeof(genstr), "opt_%d", type);
		return(genstr);
	}
}

static const char *
dhcp6stcode(int code)
{
	static char genstr[sizeof("code255") + 1]; /* XXX thread unsafe */

	if (code > 255)
		return "INVALID code";

	switch(code) {
	case DH6OPT_STCODE_SUCCESS:
		return "success";
	case DH6OPT_STCODE_UNSPECFAIL:
		return "unspec failure";
	case DH6OPT_STCODE_NOADDRAVAIL:
		return "no addresses";
	case DH6OPT_STCODE_NOBINDING:
		return "no binding";
	case DH6OPT_STCODE_NOTONLINK:
		return "not on-link";
	case DH6OPT_STCODE_USEMULTICAST:
		return "use multicast";
	case DH6OPT_STCODE_NOPREFIXAVAIL:
		return "no prefixes";
	default:
		snprintf(genstr, sizeof(genstr), "code%d", code);
		return(genstr);
	}
}

static void
dhcp6opt_print(const u_char *cp, const u_char *ep)
{
	struct dhcp6opt *dh6o;
	u_char *tp;
	size_t i;
	u_int16_t opttype;
	size_t optlen;
	u_int16_t val16;
	u_int32_t val32;
	struct in6_addr addr6;
	struct dhcp6_ia ia;
	struct dhcp6_ia_prefix ia_prefix;

	if (cp == ep)
		return;
	while (cp < ep) {
		if (ep < cp + sizeof(*dh6o))
			goto trunc;
		dh6o = (struct dhcp6opt *)cp;
		optlen = EXTRACT_16BITS(&dh6o->dh6opt_len);
		if (ep < cp + sizeof(*dh6o) + optlen)
			goto trunc;
		opttype = EXTRACT_16BITS(&dh6o->dh6opt_type);
		printf(" (%s", dhcp6opt_name(opttype));
		switch (opttype) {
		case DH6OPT_CLIENTID:
		case DH6OPT_SERVERID:
			if (optlen < 2) {
				/*(*/
				printf(" ?)");
				break;
			}
			tp = (u_char *)(dh6o + 1);
			switch (EXTRACT_16BITS(tp)) {
			case 1:
				if (optlen >= 2 + 6) {
					printf(" hwaddr/time type %u time %u ",
					    EXTRACT_16BITS(&tp[2]),
					    EXTRACT_32BITS(&tp[4]));
					for (i = 8; i < optlen; i++)
						printf("%02x", tp[i]);
					/*(*/
					printf(")");
				} else {
					/*(*/
					printf(" ?)");
				}
				break;
			case 2:
				if (optlen >= 2 + 8) {
					printf(" vid ");
					for (i = 2; i < 2 + 8; i++)
						printf("%02x", tp[i]);
					/*(*/
					printf(")");
				} else {
					/*(*/
					printf(" ?)");
				}
				break;
			case 3:
				if (optlen >= 2 + 2) {
					printf(" hwaddr type %u ",
					    EXTRACT_16BITS(&tp[2]));
					for (i = 4; i < optlen; i++)
						printf("%02x", tp[i]);
					/*(*/
					printf(")");
				} else {
					/*(*/
					printf(" ?)");
				}
				break;
			default:
				printf(" type %d)", EXTRACT_16BITS(tp));
				break;
			}
			break;
		case DH6OPT_ORO:
			if (optlen % 2) {
				printf(" ?)");
				break;
			}
			tp = (u_char *)(dh6o + 1);
			for (i = 0; i < optlen; i += 2) {
				u_int16_t opt;

				memcpy(&opt, &tp[i], sizeof(opt));
				printf(" %s", dhcp6opt_name(ntohs(opt)));
			}
			printf(")");
			break;
		case DH6OPT_PREFERENCE:
			if (optlen != 1) {
				printf(" ?)");
				break;
			}
			printf(" %d)", *((u_char *)(dh6o + 1) + 1));
			break;
		case DH6OPT_ELAPSED_TIME:
			if (optlen != 2) {
				printf(" ?)");
				break;
			}
			memcpy(&val16, dh6o + 1, sizeof(val16));
			val16 = ntohs(val16);
			printf(" %d)", (int)val16);
			break;
		case DH6OPT_RELAY_MSG:
			printf(" (");
			dhcp6_print((const u_char *)(dh6o + 1), optlen);
			printf(")");
			break;
		case DH6OPT_RAPID_COMMIT: /* nothing todo */
			printf(")");
			break;
		case DH6OPT_INTERFACE_ID:
			/*
			 * Since we cannot predict the encoding, print hex dump
			 * at most 10 characters.
			 */
			for (i = 0; i < optlen && i < 10; i++)
				printf("%02x", ((u_char *)(dh6o + 1))[i]);
			break;
		case DH6OPT_RECONF_MSG:
			tp = (u_char *)(dh6o + 1);
			switch (*tp) {
			case DH6_RENEW:
				printf(" for renew)");
				break;
			case DH6_INFORM_REQ:
				printf(" for inf-req)");
				break;
			default:
				printf(" for ?\?\?(%02x))", *tp);
				break;
			}
			break;
		case DH6OPT_RECONF_ACCEPT: /* nothing todo */
			printf(")");
			break;
		case DH6OPT_SIP_SERVER_A:
		case DH6OPT_DNS:
		case DH6OPT_NTP_SERVERS:
			if (optlen % 16) {
				printf(" ?)");
				break;
			}
			tp = (u_char *)(dh6o + 1);
			for (i = 0; i < optlen; i += 16)
				printf(" %s", ip6addr_string(&tp[i]));
			printf(")");
			break;
		case DH6OPT_PREFIX_DELEGATION:
			dhcp6opt_print((u_char *)(dh6o + 1),
			    (u_char *)(dh6o + 1) + optlen);
			printf(")");
			break;
		case DH6OPT_PREFIX_INFORMATION:
			if (optlen % 21)
				printf(" ?)");
			memcpy(&addr6, (u_char *)(dh6o + 1) + 5,
			    sizeof(addr6));
			printf(" %s/%d", ip6addr_string(&addr6),
			    (int)*((u_char *)(dh6o + 1) + 4));
			memcpy(&val32, dh6o + 1, sizeof(val32));
			val32 = ntohl(val32);
			if (val32 == DHCP6_DURATITION_INFINITE)
				printf(" lease-duration: infinite)");
			else
				printf(" lease-duration: %u)", val32);
			break;
		case DH6OPT_STATUS_CODE:
			if (optlen < 2) {
				printf(" ?)");
				break;
			}
			memcpy(&val16, (u_char *)(dh6o + 1), sizeof(val16));
			val16 = ntohs(val16);
			printf(" %s)", dhcp6stcode(val16));
			break;
		case DH6OPT_IA_NA:
		case DH6OPT_IA_PD:
			if (optlen < sizeof(ia) - 4) {
				printf(" ?)");
				break;
			}
			memcpy(&ia, (u_char *)dh6o, sizeof(ia));
			ia.dh6opt_ia_iaid = ntohl(ia.dh6opt_ia_iaid);
			ia.dh6opt_ia_t1 = ntohl(ia.dh6opt_ia_t1);
			ia.dh6opt_ia_t2 = ntohl(ia.dh6opt_ia_t2);
			printf(" IAID:%lu T1:%lu T2:%lu",
			    (unsigned long)ia.dh6opt_ia_iaid,
			    (unsigned long)ia.dh6opt_ia_t1,
			    (unsigned long)ia.dh6opt_ia_t2);
			if (optlen > sizeof(ia) - 4) {
				/* there are sub-options */
				dhcp6opt_print((u_char *)dh6o + sizeof(ia),
				    (u_char *)(dh6o + 1) + optlen);
			}
			printf(")");
			break;
		case DH6OPT_IA_PD_PREFIX:
			if (optlen < sizeof(ia_prefix) - 4) {
				printf(" ?)");
				break;
			}
			memcpy(&ia_prefix, (u_char *)dh6o, sizeof(ia_prefix));
			printf(" %s/%d",
			    ip6addr_string(&ia_prefix.dh6opt_ia_prefix_addr),
			    ia_prefix.dh6opt_ia_prefix_plen);
			ia_prefix.dh6opt_ia_prefix_pltime =
			    ntohl(ia_prefix.dh6opt_ia_prefix_pltime);
			ia_prefix.dh6opt_ia_prefix_vltime =
			    ntohl(ia_prefix.dh6opt_ia_prefix_vltime);
			printf(" pltime:%lu vltime:%lu",
			    (unsigned long)ia_prefix.dh6opt_ia_prefix_pltime,
			    (unsigned long)ia_prefix.dh6opt_ia_prefix_vltime);
			if (optlen > sizeof(ia_prefix) - 4) {
				/* there are sub-options */
				dhcp6opt_print((u_char *)dh6o +
				    sizeof(ia_prefix),
				    (u_char *)(dh6o + 1) + optlen);
			}
			printf(")");
			break;
		default:
			printf(")");
			break;
		}

		cp += sizeof(*dh6o) + optlen;
	}
	return;

trunc:
	printf("[|dhcp6ext]");
}

/*
 * Print dhcp6 packets
 */
void
dhcp6_print(const u_char *cp, u_int length)
{
	struct dhcp6 *dh6;
	struct dhcp6_relay *dh6relay;
	const u_char *ep;
	u_char *extp;
	const char *name;

	printf("dhcp6");

	ep = (u_char *)snapend;
	if (cp + length < ep)
		ep = cp + length;

	dh6 = (struct dhcp6 *)cp;
	dh6relay = (struct dhcp6_relay *)cp;
	TCHECK(dh6->dh6_xid);
	switch (dh6->dh6_msgtype) {
	case DH6_SOLICIT:
		name = "solicit";
		break;
	case DH6_ADVERTISE:
		name = "advertise";
		break;
	case DH6_REQUEST:
		name = "request";
		break;
	case DH6_CONFIRM:
		name = "confirm";
		break;
	case DH6_RENEW:
		name = "renew";
		break;
	case DH6_REBIND:
		name = "rebind";
		break;
	case DH6_REPLY:
		name = "reply";
		break;
	case DH6_RELEASE:
		name = "release";
		break;
	case DH6_DECLINE:
		name = "decline";
		break;
	case DH6_RECONFIGURE:
		name = "reconfigure";
		break;
	case DH6_INFORM_REQ:
		name= "inf-req";
		break;
	case DH6_RELAY_FORW:
		name= "relay-fwd";
		break;
	case DH6_RELAY_REPLY:
		name= "relay-reply";
		break;
	default:
		name = NULL;
		break;
	}

	if (!vflag) {
		if (name)
			printf(" %s", name);
		else if (dh6->dh6_msgtype != DH6_RELAY_FORW &&
		    dh6->dh6_msgtype != DH6_RELAY_REPLY) {
			printf(" msgtype-%u", dh6->dh6_msgtype);
		}
		return;
	}

	/* XXX relay agent messages have to be handled differently */

	if (name)
		printf(" %s (", name);	/*)*/
	else
		printf(" msgtype-%u (", dh6->dh6_msgtype);	/*)*/
	if (dh6->dh6_msgtype != DH6_RELAY_FORW &&
	    dh6->dh6_msgtype != DH6_RELAY_REPLY) {
		printf("xid=%x", EXTRACT_32BITS(&dh6->dh6_xid) & DH6_XIDMASK);
		extp = (u_char *)(dh6 + 1);
		dhcp6opt_print(extp, ep);
	} else {		/* relay messages */
		struct in6_addr addr6;

		TCHECK(dh6relay->dh6relay_peeraddr);

		memcpy(&addr6, dh6relay->dh6relay_linkaddr, sizeof (addr6));
		printf("linkaddr=%s", ip6addr_string(&addr6));

		memcpy(&addr6, dh6relay->dh6relay_peeraddr, sizeof (addr6));
		printf(" peeraddr=%s", ip6addr_string(&addr6));

		dhcp6opt_print((u_char *)(dh6relay + 1), ep);
	}
	/*(*/
	printf(")");
	return;

trunc:
	printf("[|dhcp6]");
}
