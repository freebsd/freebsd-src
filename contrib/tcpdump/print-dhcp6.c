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

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/print-dhcp6.c,v 1.12 2000/10/24 00:56:50 fenner Exp $";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

struct mbuf;
struct rtentry;

#include <netinet/in.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "interface.h"
#include "addrtoname.h"
#include "dhcp6.h"
#include "dhcp6opt.h"

#if 0
static void dhcp6opttab_init (void);
static struct dhcp6_opt *dhcp6opttab_byname (char *);
#endif
static struct dhcp6_opt *dhcp6opttab_bycode (u_int);

static char tstr[] = " [|dhcp6]";

static struct dhcp6_opt dh6opttab[] = {
	/* IP Address Extension */
	{ 1, OL6_N,	"IP Address",			OT6_NONE, },

	/* General Extension */
	{ 8193, OL6_N,	"IEEE 1003.1 POSIX Timezone",	OT6_STR, },
	{ 8194, OL6_16N, "Domain Name Server",		OT6_V6, },
	{ 8195, OL6_N,	"Domain Name",			OT6_STR, },

	{ 8196, OL6_N,	"SLP Agent",			OT6_NONE, },
	{ 8197, OL6_N,	"SLP Scope"	,		OT6_NONE, },
	{ 8198, OL6_16N, "Network Time Protocol Servers", OT6_V6, },
	{ 8199, OL6_N,	"NIS Domain",			OT6_STR, },
	{ 8200, OL6_16N, "NIS Servers",			OT6_V6, },
	{ 8201, OL6_N,	"NIS+ Domain",			OT6_STR, },
	{ 8202, OL6_16N, "NIS+ Servers",		OT6_V6, },

	/* TCP Parameters */
	{ 8203, 4,	"TCP Keepalive Interval",	OT6_NUM, },

	/* DHCPv6 Extensions */
	{ 8204, 4,	"Maximum DHCPv6 Message Size",	OT6_NUM, },
	{ 8205, OL6_N,	"DHCP Retransmission and Configuration Parameter",
							OT6_NONE, },
	{ 8206, OL6_N,	"Extension Request",		OT6_NONE, },
	{ 8207, OL6_N,	"Subnet Prefix",		OT6_NONE, },
	{ 8208, OL6_N,	"Platform Specific Information", OT6_NONE, },
	{ 8209, OL6_N,	"Platform Class Identifier",	OT6_STR, },
	{ 8210, OL6_N,	"Class Identifier",		OT6_STR, },
	{ 8211, 16,	"Reconfigure Multicast Address", OT6_V6, },
	{ 8212, 16,	"Renumber DHCPv6 Server Address",
							OT6_V6, },
	{ 8213, OL6_N,	"Client-Server Authentication",	OT6_NONE, },
	{ 8214, 4,	"Client Key Selection",		OT6_NUM, },

	/* End Extension */
	{ 65536, OL6_Z,	"End",				OT6_NONE, },

	{ 0 },
};

#if 0
static struct dhcp6_opt *dh6o_pad;
static struct dhcp6_opt *dh6o_end;

static void
dhcp6opttab_init()
{
	dh6o_pad = dhcp6opttab_bycode(0);
	dh6o_end = dhcp6opttab_bycode(65536);
}
#endif

#if 0
static struct dhcp6_opt *
dhcp6opttab_byname(name)
	char *name;
{
	struct dhcp6_opt *p;

	for (p = dh6opttab; p->code; p++)
		if (strcmp(name, p->name) == 0)
			return p;
	return NULL;
}
#endif

static struct dhcp6_opt *
dhcp6opttab_bycode(code)
	u_int code;
{
	struct dhcp6_opt *p;

	for (p = dh6opttab; p->code; p++)
		if (p->code == code)
			return p;
	return NULL;
}

static void
dhcp6ext_print(u_char *cp, u_char *ep)
{
	u_int16_t code, len;
	struct dhcp6_opt *p;
	char buf[BUFSIZ];
	int i;

	if (cp == ep)
		return;
	while (cp < ep) {
		if (ep - cp < sizeof(u_int16_t))
			break;
		code = ntohs(*(u_int16_t *)&cp[0]);
		if (ep - cp < sizeof(u_int16_t) * 2)
			break;
		if (code != 65535)
			len = ntohs(*(u_int16_t *)&cp[2]);
		else
			len = 0;
		if (ep - cp < len + 4)
			break;
		p = dhcp6opttab_bycode(code);
		if (p == NULL) {
			printf("(unknown, len=%d)", len);
			cp += len + 4;
			continue;
		}

		/* sanity check on length */
		switch (p->len) {
		case OL6_N:
			break;
		case OL6_16N:
			if (len % 16 != 0)
				goto trunc;
			break;
		case OL6_Z:
			if (len != 0)
				goto trunc;
			break;
		default:
			if (len != p->len)
				goto trunc;
			break;
		}
		if (cp + 4 + len > ep) {
			printf(" [|%s]", p->name);
			return;
		}

		printf(" (%s, ", p->name);
		switch (p->type) {
		case OT6_V6:
			for (i = 0; i < len; i += 16) {
				inet_ntop(AF_INET6, &cp[4 + i], buf,
					sizeof(buf));
				if (i != 0)
					printf(",");
				printf("%s", buf);
			}
			break;
		case OT6_STR:
			memset(&buf, 0, sizeof(buf));
			strncpy(buf, &cp[4], len);
			printf("%s", buf);
			break;
		case OT6_NUM:
			printf("%d", (u_int32_t)ntohl(*(u_int32_t *)&cp[4]));
			break;
		default:
			for (i = 0; i < len; i++)
				printf("%02x", cp[4 + i] & 0xff);
		}
		printf(")");
		cp += len + 4;
	}
	return;

trunc:
	printf("[|dhcp6ext]");
}

/*
 * Print dhcp6 requests
 */
void
dhcp6_print(register const u_char *cp, u_int length,
	    u_int16_t sport, u_int16_t dport)
{
	union dhcp6 *dh6;
	u_char *ep;
	u_char *extp;
	u_int16_t field16;

	printf("dhcp6");

	ep = (u_char *)snapend;

	dh6 = (union dhcp6 *)cp;
	TCHECK(dh6->dh6_msgtype);
	switch (dh6->dh6_msgtype) {
	case DH6_SOLICIT:
		if (!(vflag && TTEST(dh6->dh6_sol.dh6sol_relayaddr))) {
			printf(" solicit");
			break;
		}

		printf(" solicit (");	/*)*/
		if (dh6->dh6_sol.dh6sol_flags != 0) {
			u_int8_t f = dh6->dh6_sol.dh6sol_flags;
			printf("%s%s ",
			   (f & DH6SOL_PREFIX) ? "P" : "",
			   (f & DH6SOL_CLOSE) ? "C" : "");
		}

		memcpy(&field16, &dh6->dh6_sol.dh6sol_plen_id,
		       sizeof(field16));
		field16 = ntohs(field16);
		if (field16 & ~DH6SOL_SOLICIT_PLEN_MASK)
			printf("plen=%d ", DH6SOL_SOLICIT_PLEN(field16));
		printf("solicit-ID=%d", DH6SOL_SOLICIT_ID(field16));
		
		printf(" cliaddr=%s",
			ip6addr_string(&dh6->dh6_sol.dh6sol_cliaddr));
		printf(" relayaddr=%s", 
			ip6addr_string(&dh6->dh6_sol.dh6sol_relayaddr));
		/*(*/
		printf(")");
		break;
	case DH6_ADVERT:
		if (!(vflag && TTEST(dh6->dh6_adv.dh6adv_serveraddr))) {
			printf(" advert");
			break;
		}
		printf(" advert (");	/*)*/
		memcpy(&field16, &dh6->dh6_adv.dh6adv_rsv_id, sizeof(field16));
		printf("solicit-ID=%d",
		       ntohs(field16) & DH6SOL_SOLICIT_ID_MASK); 
		printf(" pref=%u", dh6->dh6_adv.dh6adv_pref);
		printf(" cliaddr=%s",
			ip6addr_string(&dh6->dh6_adv.dh6adv_cliaddr));
		printf(" relayaddr=%s", 
			ip6addr_string(&dh6->dh6_adv.dh6adv_relayaddr));
		printf(" servaddr=%s", 
			ip6addr_string(&dh6->dh6_adv.dh6adv_serveraddr));
		extp = (u_char *)((&dh6->dh6_adv) + 1);
		dhcp6ext_print(extp, ep);
		/*(*/
		printf(")");
		break;
	case DH6_REQUEST:
		if (!(vflag && TTEST(dh6->dh6_req.dh6req_relayaddr))) {
			printf(" request");
			break;
		}
		printf(" request (");	/*)*/
		if (dh6->dh6_req.dh6req_flags != 0) {
			u_int8_t f = dh6->dh6_req.dh6req_flags;
			printf("%s%s ",
			   (f & DH6REQ_CLOSE) ? "C" : "",
			   (f & DH6REQ_REBOOT) ? "R" : "");
		}
		printf("xid=0x%04x", dh6->dh6_req.dh6req_xid);
		printf(" cliaddr=%s",
		       ip6addr_string(&dh6->dh6_req.dh6req_cliaddr));
		printf(" relayaddr=%s", 
		       ip6addr_string(&dh6->dh6_req.dh6req_relayaddr));
		printf(" servaddr=%s",
		       ip6addr_string(&dh6->dh6_req.dh6req_serveraddr));
		dhcp6ext_print((char *)(&dh6->dh6_req + 1), ep);
		/*(*/
		printf(")");
		break;
	case DH6_REPLY:
		if (!(vflag && TTEST(dh6->dh6_rep.dh6rep_xid))) {
			printf(" reply");
			break;
		}
		printf(" reply (");	/*)*/
		if ((dh6->dh6_rep.dh6rep_flagandstat & DH6REP_RELAYPRESENT) != 0)
			printf("R ");
		printf("stat=0x%02x",
			dh6->dh6_rep.dh6rep_flagandstat & DH6REP_STATMASK);
		printf(" xid=0x%04x", dh6->dh6_rep.dh6rep_xid);
		printf(" cliaddr=%s",
		       ip6addr_string(&dh6->dh6_rep.dh6rep_cliaddr));
		extp = (u_char *)((&dh6->dh6_rep) + 1);
		if ((dh6->dh6_rep.dh6rep_flagandstat & DH6REP_RELAYPRESENT) !=
		    0) {
			printf(" relayaddr=%s", ip6addr_string(extp));
			extp += sizeof(struct in6_addr);
		}
		dhcp6ext_print(extp, ep);
		/*(*/
		printf(")");
		break;
	case DH6_RELEASE:
		printf(" release");
		break;
	case DH6_RECONFIG:
		printf(" reconfig");
		break;
	}
	return;

trunc:
	printf("%s", tstr);
}
