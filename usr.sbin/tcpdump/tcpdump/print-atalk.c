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
 * Format and print AppleTalk packets.
 */
#ifndef lint
static  char rcsid[] =
	"@(#)$Header: print-atalk.c,v 1.22 92/03/26 14:15:34 mccanne Exp $ (LBL)";
#endif

#ifdef __STDC__
#include <stdlib.h>
#endif
#include <stdio.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>

#include "interface.h"
#include "addrtoname.h"
#include "appletalk.h"
#include <strings.h>
#include "extract.h"

static char *ataddr_string();
static struct atNBPtuple *nbp_tuple_print();
static struct atNBPtuple *nbp_name_print();
static void atp_print();
static void nbp_print();
static void atp_bitmap_print();

/*
 * Print AppleTalk Datagram Delivery Protocol packets.
 */
void
ddp_print(dp, length)
	register struct atDDP *dp;
	int length;
{
	if (length < ddpSize) {
		(void)printf(" truncated-ddp %d", length);
		return;
	}
	(void)printf("%s.%d > %s.%d:",
		     ataddr_string(EXTRACT_SHORT(&dp->srcNet), dp->srcNode),
		     dp->srcSkt,
		     ataddr_string(EXTRACT_SHORT(&dp->dstNet), dp->dstNode),
		     dp->dstSkt);

	/* 'type' is the last field of 'dp' so we need the whole thing.
	   If we cannot determine the type, bail out.  (This last byte
	   happens to be *one* byte past the end of tcpdump's minimum
	   snapshot length.) */
	if ((u_char *)(dp + 1) > snapend) {
		printf(" [|atalk]");
		return;
	}

	length -= ddpSize;
	switch (dp->type) {

	case ddpRTMP:
		(void)printf(" at-rtmp %d", length);
		break;
	case ddpRTMPrequest:
		(void)printf(" at-rtmpReq %d", length);
		break;
	case ddpNBP:
		nbp_print((struct atNBP *)((u_char *)dp + ddpSize),
			  length, dp);
		break;
	case ddpATP:
		atp_print((struct atATP *)((u_char *)dp + ddpSize), length);
		break;
	case ddpECHO:
		(void)printf(" at-echo %d", length);
		break;
	case ddpIP:
		(void)printf(" at-IP %d", length);
		break;
	case ddpARP:
		(void)printf(" at-ARP %d", length);
		break;
	case ddpKLAP:
		(void)printf(" at-KLAP %d", length);
		break;
	default:
		(void)printf(" at-#%d %d", length);
		break;
	}
}

static void
atp_print(ap, length)
	register struct atATP *ap;
	int length;
{
	char c;
	long data;

	if ((u_char *)(ap + 1) > snapend) {
		/* Just bail if we don't have the whole chunk. */
		printf(" [|atalk]");
		return;
	}
	length -= sizeof(*ap);
	switch (ap->control & 0xc0) {

	case atpReqCode:
		(void)printf(" atp-req%s %d",
			     ap->control & atpXO? " " : "*",
			     EXTRACT_SHORT(&ap->transID));

		atp_bitmap_print(ap->bitmap);

		if (length != 0)
			(void)printf(" [len=%d]", length);

		switch (ap->control & (atpEOM|atpSTS)) {
		case atpEOM:
			(void)printf(" [EOM]");
			break;
		case atpSTS:
			(void)printf(" [STS]");
			break;
		case atpEOM|atpSTS:
			(void)printf(" [EOM,STS]");
			break;
		}
		break;

	case atpRspCode:
		(void)printf(" atp-resp%s%d:%d (%d)",
			     ap->control & atpEOM? "*" : " ",
			     EXTRACT_SHORT(&ap->transID), ap->bitmap, length);
		switch (ap->control & (atpXO|atpSTS)) {
		case atpXO:
			(void)printf(" [XO]");
			break;
		case atpSTS:
			(void)printf(" [STS]");
			break;
		case atpXO|atpSTS:
			(void)printf(" [XO,STS]");
			break;
		}
		break;

	case atpRelCode:
		(void)printf(" atp-rel  %d", EXTRACT_SHORT(&ap->transID));

		atp_bitmap_print(ap->bitmap);

		/* length should be zero */
		if (length)
			(void)printf(" [len=%d]", length);

		/* there shouldn't be any control flags */
		if (ap->control & (atpXO|atpEOM|atpSTS)) {
			c = '[';
			if (ap->control & atpXO) {
				(void)printf("%cXO", c);
				c = ',';
			}
			if (ap->control & atpEOM) {
				(void)printf("%cEOM", c);
				c = ',';
			}
			if (ap->control & atpSTS) {
				(void)printf("%cSTS", c);
				c = ',';
			}
			(void)printf("]");
		}
		break;

	default:
		(void)printf(" atp-0x%x  %d (%d)", ap->control, 
			     EXTRACT_SHORT(&ap->transID), length);
		break;
	}
	data = EXTRACT_LONG(&ap->userData);
	if (data != 0)
		(void)printf(" 0x%x", data);
}

static void
atp_bitmap_print(bm)
	register u_char bm;
{
	register char c;
	register int i;

	/* 
	 * The '& 0xff' below is needed for compilers that want to sign
	 * extend a u_char, which is the case with the Ultrix compiler.
	 * (gcc is smart enough to eliminate it, at least on the Sparc).
	 */
	if ((bm + 1) & (bm & 0xff)) {
		c = '<';
		for (i = 0; bm; ++i) {
			if (bm & 1) {
				(void)printf("%c%d", c, i);
				c = ',';
			}
			bm >>= 1;
		}
		(void)printf(">");
	} else {
		for (i = 0; bm; ++i)
			bm >>= 1;
		if (i > 1)
			(void)printf("<0-%d>", i - 1);
		else
			(void)printf("<0>");
	}
}

static void
nbp_print(np, length, dp)
	register struct atNBP *np;
	int length;
	register struct atDDP *dp;
{
	register struct atNBPtuple *tp =
			(struct atNBPtuple *)((u_char *)np + nbpHeaderSize);
	int i = length;
	u_char *ep;

	length -= nbpHeaderSize;
	if (length < 8) {
		/* must be room for at least one tuple */
		(void)printf(" truncated-nbp %d", length + nbpHeaderSize);
		return;
	}
	/* ep points to end of available data */
	ep = snapend;
	if ((u_char *)tp > ep) {
		printf(" [|atalk]");
		return;
	}
	switch (i = np->control & 0xf0) {

	case nbpBrRq:
	case nbpLkUp:
		(void)printf(i == nbpLkUp? " nbp-lkup %d:":" nbp-brRq %d:",
			     np->id);
		if ((u_char *)(tp + 1) > ep) {
			printf(" [|atalk]");
			return;
		}
		(void)nbp_name_print(tp, ep);
		/*
		 * look for anomalies: the spec says there can only
		 * be one tuple, the address must match the source
		 * address and the enumerator should be zero.
		 */
		if ((np->control & 0xf) != 1)
			(void)printf(" [ntup=%d]", np->control & 0xf);
		if (tp->enumerator)
			(void)printf(" [enum=%d]", tp->enumerator);
		if (EXTRACT_SHORT(&tp->net) != EXTRACT_SHORT(&dp->srcNet) ||
		    tp->node != dp->srcNode || tp->skt != dp->srcSkt)
			(void)printf(" [addr=%s.%d]",
				     ataddr_string(EXTRACT_SHORT(&tp->net), 
						   tp->node), 
				     tp->skt);
		break;

	case nbpLkUpReply:
		(void)printf(" nbp-reply %d:", np->id);

		/* print each of the tuples in the reply */
		for (i = np->control & 0xf; --i >= 0 && tp; )
			tp = nbp_tuple_print(tp, ep, dp);
		break;

	default:
		(void)printf(" nbp-0x%x  %d (%d)", np->control, np->id,
				length);
		break;
	}
}

/* print a counted string */
static char *
print_cstring(cp, ep)
	register char *cp;
	register u_char *ep;
{
	register int length;

	if (cp >= (char *)ep) {
		(void)printf("[|atalk]");
		return (0);
	}
	length = *cp++;

	/* Spec says string can be at most 32 bytes long */
	if (length < 0 || length > 32) {
		(void)printf("[len=%d]", length);
		return (0);
	}
	while (--length >= 0) {
		if (cp >= (char *)ep) {
			(void)printf("[|atalk]");
			return (0);
		}
		putchar(*cp++);
	}
	return (cp);
}

static struct atNBPtuple *
nbp_tuple_print(tp, ep, dp)
	register struct atNBPtuple *tp;
	register u_char *ep;
	register struct atDDP *dp;
{
	register struct atNBPtuple *tpn;

	if ((u_char *)(tp + 1) > ep) {
		printf(" [|atalk]");
		return 0;
	}
	tpn = nbp_name_print(tp, ep);

	/* if the enumerator isn't 1, print it */
	if (tp->enumerator != 1)
		(void)printf("(%d)", tp->enumerator);

	/* if the socket doesn't match the src socket, print it */
	if (tp->skt != dp->srcSkt)
		(void)printf(" %d", tp->skt);

	/* if the address doesn't match the src address, it's an anomaly */
	if (EXTRACT_SHORT(&tp->net) != EXTRACT_SHORT(&dp->srcNet) ||
	    tp->node != dp->srcNode)
		(void)printf(" [addr=%s]",
			     ataddr_string(EXTRACT_SHORT(&tp->net), tp->node));

	return (tpn);
}

static struct atNBPtuple *
nbp_name_print(tp, ep)
	struct atNBPtuple *tp;
	register u_char *ep;
{
	register char *cp = (char *)tp + nbpTupleSize;

	putchar(' ');

	/* Object */
	putchar('"');
	if (cp = print_cstring(cp, ep)) {
		/* Type */
		putchar(':');
		if (cp = print_cstring(cp, ep)) {
			/* Zone */
			putchar('@');
			if (cp = print_cstring(cp, ep))
				putchar('"');
		}
	}
	return ((struct atNBPtuple *)cp);
}


#define HASHNAMESIZE 4096

struct hnamemem {
	int addr;
	char *name;
	struct hnamemem *nxt;
};

static struct hnamemem hnametable[HASHNAMESIZE];

static char *
ataddr_string(atnet, athost)
	u_short atnet;
	u_char athost;
{
	register struct hnamemem *tp, *tp2;
	register int i = (atnet << 8) | athost;
	char nambuf[256];
	static int first = 1;
	FILE *fp;

	/*
	 * if this is the first call, see if there's an AppleTalk
	 * number to name map file.
	 */
	if (first && (first = 0, !nflag)
	    && (fp = fopen("/etc/atalk.names", "r"))) {
		char line[256];
		int i1, i2, i3;

		while (fgets(line, sizeof(line), fp)) {
			if (line[0] == '\n' || line[0] == 0 || line[0] == '#')
				continue;
			if (sscanf(line, "%d.%d.%d %s", &i1, &i2, &i3,
				     nambuf) == 4)
				/* got a hostname. */
				i3 |= ((i1 << 8) | i2) << 8;
			else if (sscanf(line, "%d.%d %s", &i1, &i2,
					nambuf) == 3)
				/* got a net name */
				i3 = (((i1 << 8) | i2) << 8) | 255;
			else
				continue;

			for (tp = &hnametable[i3 & (HASHNAMESIZE-1)];
			     tp->nxt; tp = tp->nxt)
				;
			tp->addr = i3;
			tp->nxt = (struct hnamemem *)calloc(1, sizeof(*tp));
			i3 = strlen(nambuf) + 1;
			tp->name = strcpy(malloc((unsigned) i3), nambuf);
		}
		fclose(fp);
	}

	for (tp = &hnametable[i & (HASHNAMESIZE-1)]; tp->nxt; tp = tp->nxt)
		if (tp->addr == i)
			return (tp->name);

	/* didn't have the node name -- see if we've got the net name */
	i |= 255;
	for (tp2 = &hnametable[i & (HASHNAMESIZE-1)]; tp2->nxt; tp2 = tp2->nxt)
		if (tp2->addr == i) {
			tp->addr = (atnet << 8) | athost;
			tp->nxt = (struct hnamemem *)calloc(1, sizeof(*tp));
			(void)sprintf(nambuf, "%s.%d", tp2->name, athost);
			i = strlen(nambuf) + 1;
			tp->name = strcpy(malloc((unsigned) i), nambuf);
			return (tp->name);
		}

	tp->addr = (atnet << 8) | athost;
	tp->nxt = (struct hnamemem *)calloc(1, sizeof(*tp));
	if (athost != 255)
		(void)sprintf(nambuf, "%d.%d.%d",
		    atnet >> 8, atnet & 0xff, athost);
	else
		(void)sprintf(nambuf, "%d.%d", atnet >> 8, atnet & 0xff);
	i = strlen(nambuf) + 1;
	tp->name = strcpy(malloc((unsigned) i), nambuf);

	return (tp->name);
}
