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
 *
 * Format and print bootp packets.
 */
#ifndef lint
static char rcsid[] =
    "@(#) $Header: print-bootp.c,v 1.42 96/07/23 14:17:22 leres Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#if __STDC__
struct mbuf;
struct rtentry;
#endif
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "bootp.h"

static void rfc1048_print(const u_char *, u_int);
static void cmu_print(const u_char *, u_int);

static char tstr[] = " [|bootp]";

/*
 * Print bootp requests
 */
void
bootp_print(register const u_char *cp, u_int length,
	    u_short sport, u_short dport)
{
	register const struct bootp *bp;
	static u_char vm_cmu[4] = VM_CMU;
	static u_char vm_rfc1048[4] = VM_RFC1048;

	bp = (struct bootp *)cp;
	TCHECK(bp->bp_op);
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

	TCHECK(bp->bp_secs);

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
		printf(" xid:0x%x", (u_int32_t)ntohl(bp->bp_xid));
	if (bp->bp_secs)
		printf(" secs:%d", ntohs(bp->bp_secs));

	/* Client's ip address */
	TCHECK(bp->bp_ciaddr);
	if (bp->bp_ciaddr.s_addr)
		printf(" C:%s", ipaddr_string(&bp->bp_ciaddr));

	/* 'your' ip address (bootp client) */
	TCHECK(bp->bp_yiaddr);
	if (bp->bp_yiaddr.s_addr)
		printf(" Y:%s", ipaddr_string(&bp->bp_yiaddr));

	/* Server's ip address */
	TCHECK(bp->bp_siaddr);
	if (bp->bp_siaddr.s_addr)
		printf(" S:%s", ipaddr_string(&bp->bp_siaddr));

	/* Gateway's ip address */
	TCHECK(bp->bp_giaddr);
	if (bp->bp_giaddr.s_addr)
		printf(" G:%s", ipaddr_string(&bp->bp_giaddr));

	/* Client's Ethernet address */
	if (bp->bp_htype == 1 && bp->bp_hlen == 6) {
		register const struct ether_header *eh;
		register const char *e;

		TCHECK2(bp->bp_chaddr[0], 6);
		eh = (struct ether_header *)packetp;
		if (bp->bp_op == BOOTREQUEST)
			e = (const char *)ESRC(eh);
		else if (bp->bp_op == BOOTREPLY)
			e = (const char *)EDST(eh);
		else
			e = 0;
		if (e == 0 || memcmp((char *)bp->bp_chaddr, e, 6) != 0)
			printf(" ether %s", etheraddr_string(bp->bp_chaddr));
	}

	TCHECK2(bp->bp_sname[0], 1);		/* check first char only */
	if (*bp->bp_sname) {
		printf(" sname \"");
		if (fn_print(bp->bp_sname, snapend)) {
			putchar('"');
			fputs(tstr + 1, stdout);
			return;
		}
	}
	TCHECK2(bp->bp_sname[0], 1);		/* check first char only */
	if (*bp->bp_file) {
		printf(" file \"");
		if (fn_print(bp->bp_file, snapend)) {
			putchar('"');
			fputs(tstr + 1, stdout);
			return;
		}
	}

	/* Decode the vendor buffer */
	TCHECK(bp->bp_vend[0]);
	length -= sizeof(*bp) - sizeof(bp->bp_vend);
	if (memcmp((char *)bp->bp_vend, (char *)vm_rfc1048,
		 sizeof(u_int32_t)) == 0)
		rfc1048_print(bp->bp_vend, length);
	else if (memcmp((char *)bp->bp_vend, (char *)vm_cmu,
		      sizeof(u_int32_t)) == 0)
		cmu_print(bp->bp_vend, length);
	else {
		u_int32_t ul;

		memcpy((char *)&ul, (char *)bp->bp_vend, sizeof(ul));
		if (ul != 0)
			printf("vend-#0x%x", ul);
	}

	return;
trunc:
	fputs(tstr, stdout);
}

/* The first character specifies the format to print */
static struct tok tag2str[] = {
/* RFC1048 tags */
	{ TAG_PAD,		" PAD" },
	{ TAG_SUBNET_MASK,	"iSM" },	/* subnet mask (RFC950) */
	{ TAG_TIME_OFFSET,	"lTZ" },	/* seconds from UTC */
	{ TAG_GATEWAY,		"iDG" },	/* default gateway */
	{ TAG_TIME_SERVER,	"iTS" },	/* time servers (RFC868) */
	{ TAG_NAME_SERVER,	"iIEN" },	/* IEN name servers (IEN116) */
	{ TAG_DOMAIN_SERVER,	"iNS" },	/* domain name (RFC1035) */
	{ TAG_LOG_SERVER,	"iLOG" },	/* MIT log servers */
	{ TAG_COOKIE_SERVER,	"iCS" },	/* cookie servers (RFC865) */
	{ TAG_LPR_SERVER,	"iLPR" },	/* lpr server (RFC1179) */
	{ TAG_IMPRESS_SERVER,	"iIM" },	/* impress servers (Imagen) */
	{ TAG_RLP_SERVER,	"iRL" },	/* resource location (RFC887) */
	{ TAG_HOSTNAME,		"aHN" },	/* ascii hostname */
	{ TAG_BOOTSIZE,		"sBS" },	/* 512 byte blocks */
	{ TAG_END,		" END" },
/* RFC1497 tags */
	{ TAG_DUMPPATH,		"aDP" },
	{ TAG_DOMAINNAME,	"aDN" },
	{ TAG_SWAP_SERVER,	"iSS" },
	{ TAG_ROOTPATH,		"aRP" },
	{ TAG_EXTPATH,		"aEP" },
	{ 0,			NULL }
};

static void
rfc1048_print(register const u_char *bp, register u_int length)
{
	register u_char tag;
	register u_int len, size;
	register const char *cp;
	register char c;
	int first;
	u_int32_t ul;
	u_short us;

	printf(" vend-rfc1048");

	/* Step over magic cookie */
	bp += sizeof(int32_t);

	/* Loop while we there is a tag left in the buffer */
	while (bp + 1 < snapend) {
		tag = *bp++;
		if (tag == TAG_PAD)
			continue;
		if (tag == TAG_END)
			return;
		cp = tok2str(tag2str, "?T%d", tag);
		c = *cp++;
		printf(" %s:", cp);

		/* Get the length; check for truncation */
		if (bp + 1 >= snapend) {
			fputs(tstr, stdout);
			return;
		}
		len = *bp++;
		if (bp + len >= snapend) {
			fputs(tstr, stdout);
			return;
		}

		/* Print data */
		size = len;
		if (c == '?') {
			/* Base default formats for unknown tags on data size */
			if (size & 1)
				c = 'b';
			else if (size & 2)
				c = 's';
			else
				c = 'l';
		}
		first = 1;
		switch (c) {

		case 'a':
			/* ascii strings */
			putchar('"');
			(void)fn_printn(bp, size, NULL);
			putchar('"');
			bp += size;
			size = 0;
			break;

		case 'i':
		case 'l':
			/* ip addresses/32-bit words */
			while (size >= sizeof(ul)) {
				if (!first)
					putchar(',');
				memcpy((char *)&ul, (char *)bp, sizeof(ul));
				if (c == 'i')
					printf("%s", ipaddr_string(&ul));
				else
					printf("%u", ul);
				bp += sizeof(ul);
				size -= sizeof(ul);
				first = 0;
			}
			break;

		case 's':
			/* shorts */
			while (size >= sizeof(us)) {
				if (!first)
					putchar(',');
				memcpy((char *)&us, (char *)bp, sizeof(us));
				printf("%d", us);
				bp += sizeof(us);
				size -= sizeof(us);
				first = 0;
			}
			break;

		case 'b':
		default:
			/* Bytes */
			while (size > 0) {
				if (!first)
					putchar('.');
				printf("%d", *bp);
				++bp;
				--size;
				first = 0;
			}
			break;
		}
		/* Data left over? */
		if (size)
			printf("[len %d]", len);
	}
}

static void
cmu_print(register const u_char *bp, register u_int length)
{
	register const struct cmu_vend *cmu;
	char *fmt = " %s:%s";

#define PRINTCMUADDR(m, s) { TCHECK(cmu->m); \
    if (cmu->m.s_addr != 0) \
	printf(fmt, s, ipaddr_string(&cmu->m.s_addr)); }

	printf(" vend-cmu");
	cmu = (struct cmu_vend *)bp;

	/* Only print if there are unknown bits */
	TCHECK(cmu->v_flags);
	if ((cmu->v_flags & ~(VF_SMASK)) != 0)
		printf(" F:0x%x", cmu->v_flags);
	PRINTCMUADDR(v_dgate, "DG");
	PRINTCMUADDR(v_smask, cmu->v_flags & VF_SMASK ? "SM" : "SM*");
	PRINTCMUADDR(v_dns1, "NS1");
	PRINTCMUADDR(v_dns2, "NS2");
	PRINTCMUADDR(v_ins1, "IEN1");
	PRINTCMUADDR(v_ins2, "IEN2");
	PRINTCMUADDR(v_ts1, "TS1");
	PRINTCMUADDR(v_ts2, "TS2");
	return;

trunc:
	fputs(tstr, stdout);
#undef PRINTCMUADDR
}
