/*
 * Copyright (c) 1990, 1991, 1993, 1994, 1995, 1996
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

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: print-ppp.c,v 1.24 96/12/10 23:23:12 leres Exp $ (LBL)";
#endif

#ifdef PPP
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>

#if __STDC__
struct mbuf;
struct rtentry;
#endif
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <ctype.h>
#include <netdb.h>
#include <pcap.h>
#include <signal.h>
#include <stdio.h>

#include <net/ethernet.h>
#include "ethertype.h"

#include <net/ppp_defs.h>
#include "interface.h"
#include "addrtoname.h"

struct protonames {
	u_short protocol;
	char *name;
};

static struct protonames protonames[] = {
	/*
	 * Protocol field values.
	 */
	PPP_IP,		"IP",		/* Internet Protocol */
	PPP_XNS,	"XNS",		/* Xerox NS */
	PPP_IPX,	"IPX",		/* IPX Datagram (RFC1552) */
	PPP_VJC_COMP,	"VJC_UNCOMP",	/* VJ compressed TCP */
	PPP_VJC_UNCOMP,	"VJC_UNCOMP",	/* VJ uncompressed TCP */
	PPP_COMP,	"COMP",		/* compressed packet */
	PPP_IPCP,	"IPCP",		/* IP Control Protocol */
	PPP_IPXCP,	"IPXCP",	/* IPX Control Protocol (RFC1552) */
	PPP_CCP,	"CCP",		/* Compression Control Protocol */
	PPP_LCP,	"LCP",		/* Link Control Protocol */
	PPP_PAP,	"PAP",		/* Password Authentication Protocol */
	PPP_LQR,	"LQR",		/* Link Quality Report protocol */
	PPP_CHAP,	"CHAP",		/* Cryptographic Handshake Auth. Proto*/
};

/* LCP */

#define LCP_CONF_REQ	1
#define LCP_CONF_ACK	2
#define LCP_CONF_NAK	3
#define LCP_CONF_REJ	4
#define LCP_TERM_REQ	5
#define LCP_TERM_ACK	6
#define LCP_CODE_REJ	7
#define LCP_PROT_REJ	8
#define LCP_ECHO_REQ	9
#define LCP_ECHO_RPL	10
#define LCP_DISC_REQ	11

#define LCP_MIN	LCP_CONF_REQ
#define LCP_MAX LCP_DISC_REQ

static char *lcpcodes[] = {
	/*
	 * LCP code values (RFC1661, pp26)
	 */
	"Configure-Request",
	"Configure-Ack",
	"Configure-Nak",
	"Configure-Reject",
	"Terminate-Request",
	"Terminate-Ack",
 	"Code-Reject",
	"Protocol-Reject",
	"Echo-Request",
	"Echo-Reply",
	"Discard-Request",
};

#define LCPOPT_VEXT	0
#define LCPOPT_MRU	1
#define LCPOPT_ACCM	2
#define LCPOPT_AP	3
#define LCPOPT_QP	4
#define LCPOPT_MN	5
#define LCPOPT_PFC	7
#define LCPOPT_ACFC	8

#define LCPOPT_MIN 0
#define LCPOPT_MAX 24

static char *lcpconfopts[] = {
	"Vendor-Ext",
	"Max-Rx-Unit",
	"Async-Ctrl-Char-Map",
	"Auth-Prot",
	"Quality-Prot",
	"Magic-Number",
	"unassigned (6)",	
	"Prot-Field-Compr",
	"Add-Ctrl-Field-Compr",
	"FCS-Alternatives",
	"Self-Describing-Pad",
	"Numbered-Mode",
	"Multi-Link-Procedure",
	"Call-Back",
	"Connect-Time"
	"Compund-Frames",
	"Nominal-Data-Encap",
	"Multilink-MRRU",
	"Multilink-SSNHF",
	"Multilink-ED",
	"Proprietary",
	"DCE-Identifier",
	"Multilink-Plus-Proc",
	"Link-Discriminator",
	"LCP-Auth-Option",
};

/* CHAP */

#define CHAP_CHAL	1
#define CHAP_RESP	2
#define CHAP_SUCC	3
#define CHAP_FAIL	4

#define CHAP_CODEMIN 1
#define CHAP_CODEMAX 4

static char *chapcode[] = {
	"Challenge",
	"Response",
	"Success",
	"Failure",	
};

/* PAP */

#define PAP_AREQ	1
#define PAP_AACK	2
#define PAP_ANAK	3

#define PAP_CODEMIN	1
#define PAP_CODEMAX	3

static char *papcode[] = {
	"Authenticate-Request",
	"Authenticate-Ack",
	"Authenticate-Nak",
};

/* IPCP */

#define IPCP_2ADDR	1
#define IPCP_CP		2
#define IPCP_ADDR	3

static int handle_lcp(const u_char *p, int length);
static int print_lcp_config_options(u_char *p);
static int handle_chap(const u_char *p, int length);
static int handle_ipcp(const u_char *p, int length);
static int handle_pap(const u_char *p, int length);

void
ppp_hdlc_print(const u_char *p, int length)
{
	int proto = PPP_PROTOCOL(p);
	int i, j, x;
	u_char *ptr;

	printf("ID-%03d ", *(p+5));
			
	for (i = (sizeof(protonames) / sizeof(protonames[0])) - 1; i >= 0; --i)
	{
		if (proto == protonames[i].protocol)
		{
			printf("%s: ", protonames[i].name);

			switch(proto)
			{
				case PPP_LCP:
					handle_lcp(p, length);
					break;
				case PPP_CHAP:
					handle_chap(p, length);
					break;
				case PPP_PAP:
					handle_pap(p, length);
					break;
				case PPP_IPCP:
					handle_ipcp(p, length);
					break;
			}
			break;
		}
	}
	if (i < 0)
	{
		printf("%04x: ", proto);
	}
}

/* print LCP frame */

static int
handle_lcp(const u_char *p, int length)
{
	int x, j;
	u_char *ptr;

	x = *(p+4);

	if((x >= LCP_MIN) && (x <= LCP_MAX))
	{
		printf("%s", lcpcodes[x-1]);
	}
	else
	{
		printf("0x%02x", x);
		return;
	}

	length -= 4;
	
	switch(x)
	{
		case LCP_CONF_REQ:
		case LCP_CONF_ACK:
		case LCP_CONF_NAK:
		case LCP_CONF_REJ:
			x = length;
			ptr = (u_char *)p+8;
			do
			{
				if((j = print_lcp_config_options(ptr)) == 0)
					break;
				x -= j;
				ptr += j;
			}
			while(x > 0);
			break;

		case LCP_ECHO_REQ:
		case LCP_ECHO_RPL:
			printf(", Magic-Number=%d", ((*(p+8) << 24) + (*(p+9) << 16) + (*(p+10) << 8) + (*(p+11))));
			break;
		case LCP_TERM_REQ:
		case LCP_TERM_ACK:
		case LCP_CODE_REJ:
		case LCP_PROT_REJ:
		case LCP_DISC_REQ:
		default:
			break;
	}
}

/* LCP config options */

static int
print_lcp_config_options(u_char *p)
{
	int len	= *(p+1);
	int opt = *p;
	
	if((opt >= LCPOPT_MIN) && (opt <= LCPOPT_MAX))
		printf(", %s", lcpconfopts[opt]);

	switch(opt)
	{
		case LCPOPT_MRU:
			if(len == 4)
				printf("=%d", (*(p+2) << 8) + *(p+3));
			break;
		case LCPOPT_AP:
			if(len >= 4)
			{
				if(*(p+2) == 0xc0 && *(p+3) == 0x23)
				{
					printf(" PAP");
				}
				else if(*(p+2) == 0xc2 && *(p+3) == 0x23)
				{
					printf(" CHAP/");
					switch(*(p+4))
					{
						default:
							printf("unknown-algorithm-%d", *(p+4));
							break;
						case 5:
							printf("MD5");
							break;
						case 0x80:
							printf("Microsoft");
							break;
					}
				}
				else if(*(p+2) == 0xc2 && *(p+3) == 0x27)
				{
					printf(" EAP");
				}
				else if(*(p+2) == 0xc0 && *(p+3) == 0x27)
				{
					printf(" SPAP");
				}
				else if(*(p+2) == 0xc1 && *(p+3) == 0x23)
				{
					printf(" Old-SPAP");
				}
				else
				{
					printf("unknown");
				}
			}
			break;
		case LCPOPT_QP:
			if(len >= 4)
			{
				if(*(p+2) == 0xc0 && *(p+3) == 0x25)
					printf(" LQR");
				else
					printf(" unknown");
			}
			break;
		case LCPOPT_MN:
			if(len == 6)
			{
				printf("=%d", ((*(p+2) << 24) + (*(p+3) << 16) + (*(p+4) << 8) + (*(p+5))));
			}
			break;
		case LCPOPT_PFC:
			printf(" PFC");
			break;
		case LCPOPT_ACFC:
			printf(" ACFC");
			break;
	}
	return(len);
}

/* CHAP */

static int
handle_chap(const u_char *p, int length)
{
	int x, j;
	u_char *ptr;

	x = *(p+4);

	if((x >= CHAP_CODEMIN) && (x <= CHAP_CODEMAX))
	{
		printf("%s", chapcode[x-1]);
	}
	else
	{
		printf("0x%02x", x);
		return;
	}

	length -= 4;
	
	switch(x)
	{
		case CHAP_CHAL:
		case CHAP_RESP:
			printf(", Value=");
			x = *(p+8);	/* value size */
			ptr = (u_char *)p+9;
			while(--x >= 0)
				printf("%02x", *ptr++);
			x = length - *(p+8) - 1;
			printf(", Name=");
			while(--x >= 0)
				printf("%c", *ptr++);
			break;
	}
}

/* PAP */

static int
handle_pap(const u_char *p, int length)
{
	int x, j;
	u_char *ptr;

	x = *(p+4);

	if((x >= PAP_CODEMIN) && (x <= PAP_CODEMAX))
	{
		printf("%s", papcode[x-1]);
	}
	else
	{
		printf("0x%02x", x);
		return;
	}

	length -= 4;
	
	switch(x)
	{
		case PAP_AREQ:
			printf(", Peer-Id=");
			x = *(p+8);	/* peerid size */
			ptr = (u_char *)p+9;
			while(--x >= 0)
				printf("%c", *ptr++);
			x = *ptr++;
			printf(", Passwd=");
			while(--x >= 0)
				printf("%c", *ptr++);
			break;
		case PAP_AACK:
		case PAP_ANAK:		
			break;			
	}
}

/* IPCP */
	
static int
handle_ipcp(const u_char *p, int length)
{
	int x, j;

	x = *(p+8);

	length -= 4;
	
	switch(x)
	{
		case IPCP_2ADDR:
			printf("IP-Addresses");
			printf(", Src=%d.%d.%d.%d", *(p+10), *(p+11), *(p+12), *(p+13));
			printf(", Dst=%d.%d.%d.%d", *(p+14), *(p+15), *(p+16), *(p+17));
			break;
			
		case IPCP_CP:
			printf("IP-Compression-Protocol");
			break;

		case IPCP_ADDR:
			printf("IP-Address=%d.%d.%d.%d", *(p+10), *(p+11), *(p+12), *(p+13));
			break;
	}
}
	
void
ppp_if_print(u_char *user, const struct pcap_pkthdr *h,
	     register const u_char *p)
{
	register u_int length = h->len;
	register u_int caplen = h->caplen;

	ts_print(&h->ts);

	if (caplen < PPP_HDRLEN) {
		printf("[|ppp]");
		goto out;
	}

	/*
	 * Some printers want to get back at the link level addresses,
	 * and/or check that they're not walking off the end of the packet.
	 * Rather than pass them all the way down, we set these globals.
	 */
	packetp = p;
	snapend = p + caplen;

	if (eflag)
		ppp_hdlc_print(p, length);

	length -= PPP_HDRLEN;

	switch(PPP_PROTOCOL(p)) {
	case PPP_IP:
	case ETHERTYPE_IP:
		ip_print((const u_char *)(p + PPP_HDRLEN), length);
		break;
	case PPP_IPX:
	case ETHERTYPE_IPX:
		ipx_print((const u_char *)(p + PPP_HDRLEN), length);
		break;

	default:
		if(!eflag)
			ppp_hdlc_print(p, length);
		if(!xflag)
			default_print((const u_char *)(p + PPP_HDRLEN),
					caplen - PPP_HDRLEN);
	}

	if (xflag)
		default_print((const u_char *)(p + PPP_HDRLEN),
				caplen - PPP_HDRLEN);
out:
	putchar('\n');
}
#else
#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>

#include "interface.h"
void
ppp_if_print(u_char *user, const struct pcap_pkthdr *h, const u_char *p)
{
	error("not configured for ppp");
	/* NOTREACHED */
}
#endif
