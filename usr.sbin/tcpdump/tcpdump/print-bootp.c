/*
 * Copyright (c) 1988-1990 The Regents of the University of California.
 * All rights reserved.
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
 *
 * Format and print bootp packets.
 */
#ifndef lint
static char rcsid[] =
    "@(#) $Header: print-bootp.c,v 1.17 91/11/14 22:21:34 leres Exp $ (LBL)";
#endif

#include <stdio.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <strings.h>
#include <ctype.h>

#include "interface.h"
#include "addrtoname.h"
#include "bootp.h"

void rfc1048_print();
void cmu_print();

/*
 * Print bootp requests
 */
void
bootp_print(bp, length, sport, dport)
	register struct bootp *bp;
	int length;
	u_short sport, dport;
{
	static char tstr[] = " [|bootp]";
	static unsigned char vm_cmu[4] = VM_CMU;
	static unsigned char vm_rfc1048[4] = VM_RFC1048;
	u_char *ep;

#define TCHECK(var, l) if ((u_char *)&(var) > ep - l) goto trunc

	/* Note funny sized packets */
	if (length != sizeof(struct bootp))
		(void)printf(" [len=%d]", length);

	/* 'ep' points to the end of avaible data. */
	ep = (u_char *)snapend;

	switch (bp->bp_op) {

	case BOOTREQUEST:
		/* Usually, a request goes from a client to a server */
		if (sport != IPPORT_BOOTPC || dport != IPPORT_BOOTPS)
			printf(" (request)");
		break;

	case BOOTREPLY:
		/* Usually, a reply goes from a server to a client */
		if (sport != IPPORT_BOOTPS || dport != IPPORT_BOOTPC)
			printf(" (reply)");
		break;

	default:
		printf(" bootp-#%d", bp->bp_op);
	}

	NTOHL(bp->bp_xid);
	NTOHS(bp->bp_secs);

	/* The usual hardware address type is 1 (10Mb Ethernet) */
	if (bp->bp_htype != 1)
		printf(" htype-#%d", bp->bp_htype);

	/* The usual length for 10Mb Ethernet address is 6 bytes */
	if (bp->bp_htype != 1 || bp->bp_hlen != 6)
		printf(" hlen:%d", bp->bp_hlen);

	/* Only print interesting fields */
	if (bp->bp_hops)
		printf(" hops:%d", bp->bp_hops);
	if (bp->bp_xid)
		printf(" xid:0x%x", bp->bp_xid);
	if (bp->bp_secs)
		printf(" secs:%d", bp->bp_secs);

	/* Client's ip address */
	TCHECK(bp->bp_ciaddr, sizeof(bp->bp_ciaddr));
	if (bp->bp_ciaddr.s_addr)
		printf(" C:%s", ipaddr_string(&bp->bp_ciaddr));

	/* 'your' ip address (bootp client) */
	TCHECK(bp->bp_yiaddr, sizeof(bp->bp_yiaddr));
	if (bp->bp_yiaddr.s_addr)
		printf(" Y:%s", ipaddr_string(&bp->bp_yiaddr));

	/* Server's ip address */
	TCHECK(bp->bp_siaddr, sizeof(bp->bp_siaddr));
	if (bp->bp_siaddr.s_addr)
		printf(" S:%s", ipaddr_string(&bp->bp_siaddr));

	/* Gateway's ip address */
	TCHECK(bp->bp_giaddr, sizeof(bp->bp_giaddr));
	if (bp->bp_giaddr.s_addr)
		printf(" G:%s", ipaddr_string(&bp->bp_giaddr));

	/* Client's Ethernet address */
	if (bp->bp_htype == 1 && bp->bp_hlen == 6) {
		register struct ether_header *eh;
		register char *e;

		TCHECK(bp->bp_chaddr[0], 6);
		eh = (struct ether_header *)packetp;
		if (bp->bp_op == BOOTREQUEST)
			e = (char *)ESRC(eh);
		else if (bp->bp_op == BOOTREPLY)
			e = (char *)EDST(eh);
		else
			e = 0;
		if (e == 0 || bcmp((char *)bp->bp_chaddr, e, 6) != 0)
			printf(" ether %s", etheraddr_string(bp->bp_chaddr));
	}

	TCHECK(bp->bp_sname[0], sizeof(bp->bp_sname));
	if (*bp->bp_sname) {
		printf(" sname ");
		if (printfn(bp->bp_sname, ep)) {
			fputs(tstr + 1, stdout);
			return;
		}
	}
	TCHECK(bp->bp_file[0], sizeof(bp->bp_file));
	if (*bp->bp_file) {
		printf(" file ");
		if (printfn(bp->bp_file, ep)) {
			fputs(tstr + 1, stdout);
			return;
		}
	}

	/* Don't try to decode the vendor buffer unless we're verbose */
	if (vflag <= 0)
		return;

	TCHECK(bp->bp_vend[0], sizeof(bp->bp_vend));
	printf(" vend");
	if (bcmp(bp->bp_vend, vm_rfc1048, sizeof(u_long)) == 0)
		rfc1048_print(bp->bp_vend, sizeof(bp->bp_vend));
	else if (bcmp(bp->bp_vend, vm_cmu, sizeof(u_long)) == 0)
		cmu_print(bp->bp_vend, sizeof(bp->bp_vend));
	else {
		u_long ul;

		bcopy((char *)bp->bp_vend, (char *)&ul, sizeof(ul));
		printf("-#0x%x", ul);
	}

	return;
trunc:
	fputs(tstr, stdout);
#undef TCHECK
}

void
rfc1048_print(bp, length)
	register u_char *bp;
	int length;
{
	u_char tag;
	u_char *ep;
	register int i;
	u_long ul;

	printf("-rfc1048");

	/* Step over magic cookie */
	bp += sizeof(long);

	/* Setup end pointer */
	ep = bp + length;

	while (bp < ep) {
		tag = *bp++;
		i = *bp++;
		switch (tag) {

		case TAG_PAD:
			/* no-op */
			break;

		case TAG_SUBNET_MASK:
			ul = 0;
			bcopy((char *)bp, (char *)&ul, i);
			printf(" SM:%s", ipaddr_string(&ul));
			break;

		case TAG_TIME_SERVER:
			ul = 0;
			bcopy((char *)bp, (char *)&ul, i);
			printf(" TS:%s", ipaddr_string(&ul));
			break;

		case TAG_GATEWAY:
			ul = 0;
			bcopy((char *)bp, (char *)&ul, i);
			printf(" G:%s", ipaddr_string(&ul));
			break;

		case TAG_TIME_OFFSET:
		case TAG_NAME_SERVER:
		case TAG_DOMAIN_SERVER:
		case TAG_LOG_SERVER:
		case TAG_COOKIE_SERVER:
		case TAG_LPR_SERVER:
		case TAG_IMPRESS_SERVER:
		case TAG_RLP_SERVER:
		case TAG_HOSTNAME:
		case TAG_BOOTSIZE:
			printf(" tag-#%d", tag);
			if (i == sizeof(long)) {
				bcopy((char *)bp, (char *)&ul, sizeof(long));
				printf(":0x%x", ul);
			} else
				printf(":?");
			break;

		case TAG_END:
			return;

		default:
			printf("[tag-#%d]", tag);
			return;
		}
	}
}

void
cmu_print(bp, length)
	register u_char *bp;
	int length;
{
	/* XXX not really implemented */
	printf("-cmu [...]");
}
