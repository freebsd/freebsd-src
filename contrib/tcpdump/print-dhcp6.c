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
 * draft-ietf-dhc-dhcpv6-22.txt
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/print-dhcp6.c,v 1.14.4.2 2002/06/01 23:51:12 guy Exp $";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "interface.h"
#include "addrtoname.h"

/* Error Values */
#define DH6ERR_FAILURE		16
#define DH6ERR_AUTHFAIL		17
#define DH6ERR_POORLYFORMED	18
#define DH6ERR_UNAVAIL		19
#define DH6ERR_OPTUNAVAIL	20

/* Message type */
#define DH6_REPLY	7
#define DH6_INFORM_REQ	11

/* DHCP6 base packet format */
struct dhcp6 {
	union {
		u_int8_t m;
		u_int32_t x;
	} dh6_msgtypexid;
	struct in6_addr dh6_servaddr;
	/* options follow */
} __attribute__ ((__packed__));
#define dh6_msgtype	dh6_msgtypexid.m
#define dh6_xid		dh6_msgtypexid.x
#define DH6_XIDMASK	0x00ffffff

/* option */
#define DH6OPT_DUID	1	/* TBD */
#define DH6OPT_DNS	11	/* TBD */
struct dhcp6opt {
	u_int16_t dh6opt_type;
	u_int16_t dh6opt_len;
	/* type-dependent data follows */
} __attribute__ ((__packed__));

static void
dhcp6opt_print(u_char *cp, u_char *ep)
{
	struct dhcp6opt *dh6o;
	u_char *tp;
	int i;
	size_t optlen;

	if (cp == ep)
		return;
	while (cp < ep) {
		if (ep - cp < sizeof(*dh6o))
			goto trunc;
		dh6o = (struct dhcp6opt *)cp;
		optlen = ntohs(dh6o->dh6opt_len);
		if (ep - cp < sizeof(*dh6o) + optlen)
			goto trunc;
		switch (ntohs(dh6o->dh6opt_type)) {
		case DH6OPT_DUID:
			printf(" (duid");	/*)*/
			if (optlen < 2) {
				/*(*/
				printf(" ??)");
				break;
			}
			tp = (u_char *)(dh6o + 1);
			switch (ntohs(*(u_int16_t *)tp)) {
			case 1:
				if (optlen >= 2 + 6) {
					printf(" hwaddr/time time %u type %u ",
					    ntohl(*(u_int32_t *)&tp[2]),
					    ntohs(*(u_int16_t *)&tp[6]));
					for (i = 8; i < optlen; i++)
						printf("%02x", tp[i]);
					/*(*/
					printf(")");
				} else {
					/*(*/
					printf(" ??)");
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
					printf(" ??)");
				}
				break;
			case 3:
				if (optlen >= 2 + 2) {
					printf(" hwaddr type %u ",
					    ntohs(*(u_int16_t *)&tp[2]));
					for (i = 4; i < optlen; i++)
						printf("%02x", tp[i]);
					/*(*/
					printf(")");
				} else {
					/*(*/
					printf(" ??)");
				}
			}
			break;
		case DH6OPT_DNS:
			printf(" (dnsserver");	/*)*/
			if (optlen % 16) {
				/*(*/
				printf(" ??)");
				break;
			}
			tp = (u_char *)(dh6o + 1);
			for (i = 0; i < optlen; i += 16)
				printf(" %s", ip6addr_string(&tp[i]));
			/*(*/
			printf(")");
		default:
			printf(" (opt-%u)", ntohs(dh6o->dh6opt_type));
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
dhcp6_print(register const u_char *cp, u_int length,
	    u_int16_t sport, u_int16_t dport)
{
	struct dhcp6 *dh6;
	u_char *ep;
	u_char *extp;
	const char *name;

	printf("dhcp6");

	ep = (u_char *)snapend;

	dh6 = (struct dhcp6 *)cp;
	TCHECK(dh6->dh6_servaddr);
	switch (dh6->dh6_msgtype) {
	case DH6_REPLY:
		name = "reply";
		break;
	case DH6_INFORM_REQ:
		name= "inf-req";
		break;
	default:
		name = NULL;
		break;
	}

	if (!vflag) {
		if (name)
			printf(" %s", name);
		else
			printf(" msgtype-%u", dh6->dh6_msgtype);
		return;
	}

	/* XXX relay agent messages have to be handled differently */

	if (name)
		printf(" %s (", name);	/*)*/
	else
		printf(" msgtype-%u (", dh6->dh6_msgtype);	/*)*/
	printf("xid=%x", ntohl(dh6->dh6_xid) & DH6_XIDMASK);
	printf(" server=%s", ip6addr_string(&dh6->dh6_servaddr));
	extp = (u_char *)(dh6 + 1);
	dhcp6opt_print(extp, ep);
	/*(*/
	printf(")");
	return;

trunc:
	printf("[|dhcp6]");
}
