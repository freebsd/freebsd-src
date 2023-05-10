/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997
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

/* \summary: AppleTalk printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include <stdio.h>
#include <string.h>

#include "netdissect.h"
#include "addrtoname.h"
#include "ethertype.h"
#include "extract.h"
#include "appletalk.h"


static const struct tok type2str[] = {
	{ ddpRTMP,		"rtmp" },
	{ ddpRTMPrequest,	"rtmpReq" },
	{ ddpECHO,		"echo" },
	{ ddpIP,		"IP" },
	{ ddpARP,		"ARP" },
	{ ddpKLAP,		"KLAP" },
	{ 0,			NULL }
};

struct aarp {
	nd_uint16_t	htype, ptype;
	nd_uint8_t	halen, palen;
	nd_uint16_t	op;
	nd_mac_addr	hsaddr;
	uint8_t		psaddr[4];
	nd_mac_addr	hdaddr;
	uint8_t		pdaddr[4];
};

static void atp_print(netdissect_options *, const struct atATP *, u_int);
static void atp_bitmap_print(netdissect_options *, u_char);
static void nbp_print(netdissect_options *, const struct atNBP *, u_int, u_short, u_char, u_char);
static const struct atNBPtuple *nbp_tuple_print(netdissect_options *ndo, const struct atNBPtuple *,
						const u_char *,
						u_short, u_char, u_char);
static const struct atNBPtuple *nbp_name_print(netdissect_options *, const struct atNBPtuple *,
					       const u_char *);
static const char *ataddr_string(netdissect_options *, u_short, u_char);
static void ddp_print(netdissect_options *, const u_char *, u_int, u_int, u_short, u_char, u_char);
static const char *ddpskt_string(netdissect_options *, u_int);

/*
 * Print LLAP packets received on a physical LocalTalk interface.
 */
void
ltalk_if_print(netdissect_options *ndo,
               const struct pcap_pkthdr *h, const u_char *p)
{
	u_int hdrlen;

	ndo->ndo_protocol = "ltalk";
	hdrlen = llap_print(ndo, p, h->len);
	if (hdrlen == 0) {
		/* Cut short by the snapshot length. */
		ndo->ndo_ll_hdr_len += h->caplen;
		return;
	}
	ndo->ndo_ll_hdr_len += hdrlen;
}

/*
 * Print AppleTalk LLAP packets.
 */
u_int
llap_print(netdissect_options *ndo,
           const u_char *bp, u_int length)
{
	const struct LAP *lp;
	const struct atDDP *dp;
	const struct atShortDDP *sdp;
	u_short snet;
	u_int hdrlen;

	ndo->ndo_protocol = "llap";
	if (length < sizeof(*lp)) {
		ND_PRINT(" [|llap %u]", length);
		return (length);
	}
	if (!ND_TTEST_LEN(bp, sizeof(*lp))) {
		nd_print_trunc(ndo);
		return (0);	/* cut short by the snapshot length */
	}
	lp = (const struct LAP *)bp;
	bp += sizeof(*lp);
	length -= sizeof(*lp);
	hdrlen = sizeof(*lp);
	switch (GET_U_1(lp->type)) {

	case lapShortDDP:
		if (length < ddpSSize) {
			ND_PRINT(" [|sddp %u]", length);
			return (length);
		}
		if (!ND_TTEST_LEN(bp, ddpSSize)) {
			ND_PRINT(" [|sddp]");
			return (0);	/* cut short by the snapshot length */
		}
		sdp = (const struct atShortDDP *)bp;
		ND_PRINT("%s.%s",
		    ataddr_string(ndo, 0, GET_U_1(lp->src)),
		    ddpskt_string(ndo, GET_U_1(sdp->srcSkt)));
		ND_PRINT(" > %s.%s:",
		    ataddr_string(ndo, 0, GET_U_1(lp->dst)),
		    ddpskt_string(ndo, GET_U_1(sdp->dstSkt)));
		bp += ddpSSize;
		length -= ddpSSize;
		hdrlen += ddpSSize;
		ddp_print(ndo, bp, length, GET_U_1(sdp->type), 0,
			  GET_U_1(lp->src), GET_U_1(sdp->srcSkt));
		break;

	case lapDDP:
		if (length < ddpSize) {
			ND_PRINT(" [|ddp %u]", length);
			return (length);
		}
		if (!ND_TTEST_LEN(bp, ddpSize)) {
			ND_PRINT(" [|ddp]");
			return (0);	/* cut short by the snapshot length */
		}
		dp = (const struct atDDP *)bp;
		snet = GET_BE_U_2(dp->srcNet);
		ND_PRINT("%s.%s",
			 ataddr_string(ndo, snet, GET_U_1(dp->srcNode)),
			 ddpskt_string(ndo, GET_U_1(dp->srcSkt)));
		ND_PRINT(" > %s.%s:",
		    ataddr_string(ndo, GET_BE_U_2(dp->dstNet), GET_U_1(dp->dstNode)),
		    ddpskt_string(ndo, GET_U_1(dp->dstSkt)));
		bp += ddpSize;
		length -= ddpSize;
		hdrlen += ddpSize;
		ddp_print(ndo, bp, length, GET_U_1(dp->type), snet,
			  GET_U_1(dp->srcNode), GET_U_1(dp->srcSkt));
		break;

#ifdef notdef
	case lapKLAP:
		klap_print(bp, length);
		break;
#endif

	default:
		ND_PRINT("%u > %u at-lap#%u %u",
		    GET_U_1(lp->src), GET_U_1(lp->dst), GET_U_1(lp->type),
		    length);
		break;
	}
	return (hdrlen);
}

/*
 * Print EtherTalk/TokenTalk packets (or FDDITalk, or whatever it's called
 * when it runs over FDDI; yes, I've seen FDDI captures with AppleTalk
 * packets in them).
 */
void
atalk_print(netdissect_options *ndo,
            const u_char *bp, u_int length)
{
	const struct atDDP *dp;
	u_short snet;

	ndo->ndo_protocol = "atalk";
        if(!ndo->ndo_eflag)
            ND_PRINT("AT ");

	if (length < ddpSize) {
		ND_PRINT(" [|ddp %u]", length);
		return;
	}
	if (!ND_TTEST_LEN(bp, ddpSize)) {
		ND_PRINT(" [|ddp]");
		return;
	}
	dp = (const struct atDDP *)bp;
	snet = GET_BE_U_2(dp->srcNet);
	ND_PRINT("%s.%s", ataddr_string(ndo, snet, GET_U_1(dp->srcNode)),
		 ddpskt_string(ndo, GET_U_1(dp->srcSkt)));
	ND_PRINT(" > %s.%s: ",
	       ataddr_string(ndo, GET_BE_U_2(dp->dstNet), GET_U_1(dp->dstNode)),
	       ddpskt_string(ndo, GET_U_1(dp->dstSkt)));
	bp += ddpSize;
	length -= ddpSize;
	ddp_print(ndo, bp, length, GET_U_1(dp->type), snet,
		  GET_U_1(dp->srcNode), GET_U_1(dp->srcSkt));
}

/* XXX should probably pass in the snap header and do checks like arp_print() */
void
aarp_print(netdissect_options *ndo,
           const u_char *bp, u_int length)
{
	const struct aarp *ap;

#define AT(member) ataddr_string(ndo, (ap->member[1]<<8)|ap->member[2],ap->member[3])

	ndo->ndo_protocol = "aarp";
	ND_PRINT("aarp ");
	ap = (const struct aarp *)bp;
	if (!ND_TTEST_SIZE(ap)) {
		/* Just bail if we don't have the whole chunk. */
		nd_print_trunc(ndo);
		return;
	}
	if (length < sizeof(*ap)) {
		ND_PRINT(" [|aarp %u]", length);
		return;
	}
	if (GET_BE_U_2(ap->htype) == 1 &&
	    GET_BE_U_2(ap->ptype) == ETHERTYPE_ATALK &&
	    GET_U_1(ap->halen) == MAC_ADDR_LEN && GET_U_1(ap->palen) == 4)
		switch (GET_BE_U_2(ap->op)) {

		case 1:				/* request */
			ND_PRINT("who-has %s tell %s", AT(pdaddr), AT(psaddr));
			return;

		case 2:				/* response */
			ND_PRINT("reply %s is-at %s", AT(psaddr), GET_ETHERADDR_STRING(ap->hsaddr));
			return;

		case 3:				/* probe (oy!) */
			ND_PRINT("probe %s tell %s", AT(pdaddr), AT(psaddr));
			return;
		}
	ND_PRINT("len %u op %u htype %u ptype %#x halen %u palen %u",
	    length, GET_BE_U_2(ap->op), GET_BE_U_2(ap->htype),
	    GET_BE_U_2(ap->ptype), GET_U_1(ap->halen), GET_U_1(ap->palen));
}

/*
 * Print AppleTalk Datagram Delivery Protocol packets.
 */
static void
ddp_print(netdissect_options *ndo,
          const u_char *bp, u_int length, u_int t,
          u_short snet, u_char snode, u_char skt)
{

	switch (t) {

	case ddpNBP:
		nbp_print(ndo, (const struct atNBP *)bp, length, snet, snode, skt);
		break;

	case ddpATP:
		atp_print(ndo, (const struct atATP *)bp, length);
		break;

	case ddpEIGRP:
		eigrp_print(ndo, bp, length);
		break;

	default:
		ND_PRINT(" at-%s %u", tok2str(type2str, NULL, t), length);
		break;
	}
}

static void
atp_print(netdissect_options *ndo,
          const struct atATP *ap, u_int length)
{
	uint8_t control;
	uint32_t data;

	if ((const u_char *)(ap + 1) > ndo->ndo_snapend) {
		/* Just bail if we don't have the whole chunk. */
		nd_print_trunc(ndo);
		return;
	}
	if (length < sizeof(*ap)) {
		ND_PRINT(" [|atp %u]", length);
		return;
	}
	length -= sizeof(*ap);
	control = GET_U_1(ap->control);
	switch (control & 0xc0) {

	case atpReqCode:
		ND_PRINT(" atp-req%s %u",
			     control & atpXO? " " : "*",
			     GET_BE_U_2(ap->transID));

		atp_bitmap_print(ndo, GET_U_1(ap->bitmap));

		if (length != 0)
			ND_PRINT(" [len=%u]", length);

		switch (control & (atpEOM|atpSTS)) {
		case atpEOM:
			ND_PRINT(" [EOM]");
			break;
		case atpSTS:
			ND_PRINT(" [STS]");
			break;
		case atpEOM|atpSTS:
			ND_PRINT(" [EOM,STS]");
			break;
		}
		break;

	case atpRspCode:
		ND_PRINT(" atp-resp%s%u:%u (%u)",
			     control & atpEOM? "*" : " ",
			     GET_BE_U_2(ap->transID), GET_U_1(ap->bitmap),
			     length);
		switch (control & (atpXO|atpSTS)) {
		case atpXO:
			ND_PRINT(" [XO]");
			break;
		case atpSTS:
			ND_PRINT(" [STS]");
			break;
		case atpXO|atpSTS:
			ND_PRINT(" [XO,STS]");
			break;
		}
		break;

	case atpRelCode:
		ND_PRINT(" atp-rel  %u", GET_BE_U_2(ap->transID));

		atp_bitmap_print(ndo, GET_U_1(ap->bitmap));

		/* length should be zero */
		if (length)
			ND_PRINT(" [len=%u]", length);

		/* there shouldn't be any control flags */
		if (control & (atpXO|atpEOM|atpSTS)) {
			char c = '[';
			if (control & atpXO) {
				ND_PRINT("%cXO", c);
				c = ',';
			}
			if (control & atpEOM) {
				ND_PRINT("%cEOM", c);
				c = ',';
			}
			if (control & atpSTS) {
				ND_PRINT("%cSTS", c);
			}
			ND_PRINT("]");
		}
		break;

	default:
		ND_PRINT(" atp-0x%x  %u (%u)", control,
			     GET_BE_U_2(ap->transID), length);
		break;
	}
	data = GET_BE_U_4(ap->userData);
	if (data != 0)
		ND_PRINT(" 0x%x", data);
}

static void
atp_bitmap_print(netdissect_options *ndo,
                 u_char bm)
{
	u_int i;

	/*
	 * The '& 0xff' below is needed for compilers that want to sign
	 * extend a u_char, which is the case with the Ultrix compiler.
	 * (gcc is smart enough to eliminate it, at least on the Sparc).
	 */
	if ((bm + 1) & (bm & 0xff)) {
		char c = '<';
		for (i = 0; bm; ++i) {
			if (bm & 1) {
				ND_PRINT("%c%u", c, i);
				c = ',';
			}
			bm >>= 1;
		}
		ND_PRINT(">");
	} else {
		for (i = 0; bm; ++i)
			bm >>= 1;
		if (i > 1)
			ND_PRINT("<0-%u>", i - 1);
		else
			ND_PRINT("<0>");
	}
}

static void
nbp_print(netdissect_options *ndo,
          const struct atNBP *np, u_int length, u_short snet,
          u_char snode, u_char skt)
{
	const struct atNBPtuple *tp =
		(const struct atNBPtuple *)((const u_char *)np + nbpHeaderSize);
	uint8_t control;
	u_int i;
	const u_char *ep;

	if (length < nbpHeaderSize) {
		ND_PRINT(" truncated-nbp %u", length);
		return;
	}

	length -= nbpHeaderSize;
	if (length < 8) {
		/* must be room for at least one tuple */
		ND_PRINT(" truncated-nbp %u", length + nbpHeaderSize);
		return;
	}
	/* ep points to end of available data */
	ep = ndo->ndo_snapend;
	if ((const u_char *)tp > ep) {
		nd_print_trunc(ndo);
		return;
	}
	control = GET_U_1(np->control);
	switch (i = (control & 0xf0)) {

	case nbpBrRq:
	case nbpLkUp:
		ND_PRINT(i == nbpLkUp? " nbp-lkup %u:":" nbp-brRq %u:",
			 GET_U_1(np->id));
		if ((const u_char *)(tp + 1) > ep) {
			nd_print_trunc(ndo);
			return;
		}
		(void)nbp_name_print(ndo, tp, ep);
		/*
		 * look for anomalies: the spec says there can only
		 * be one tuple, the address must match the source
		 * address and the enumerator should be zero.
		 */
		if ((control & 0xf) != 1)
			ND_PRINT(" [ntup=%u]", control & 0xf);
		if (GET_U_1(tp->enumerator))
			ND_PRINT(" [enum=%u]", GET_U_1(tp->enumerator));
		if (GET_BE_U_2(tp->net) != snet ||
		    GET_U_1(tp->node) != snode ||
		    GET_U_1(tp->skt) != skt)
			ND_PRINT(" [addr=%s.%u]",
			    ataddr_string(ndo, GET_BE_U_2(tp->net),
					  GET_U_1(tp->node)),
			    GET_U_1(tp->skt));
		break;

	case nbpLkUpReply:
		ND_PRINT(" nbp-reply %u:", GET_U_1(np->id));

		/* print each of the tuples in the reply */
		for (i = control & 0xf; i != 0 && tp; i--)
			tp = nbp_tuple_print(ndo, tp, ep, snet, snode, skt);
		break;

	default:
		ND_PRINT(" nbp-0x%x  %u (%u)", control, GET_U_1(np->id),
			 length);
		break;
	}
}

/* print a counted string */
static const u_char *
print_cstring(netdissect_options *ndo,
              const u_char *cp, const u_char *ep)
{
	u_int length;

	if (cp >= ep) {
		nd_print_trunc(ndo);
		return (0);
	}
	length = GET_U_1(cp);
	cp++;

	/* Spec says string can be at most 32 bytes long */
	if (length > 32) {
		ND_PRINT("[len=%u]", length);
		return (0);
	}
	while (length != 0) {
		if (cp >= ep) {
			nd_print_trunc(ndo);
			return (0);
		}
		fn_print_char(ndo, GET_U_1(cp));
		cp++;
		length--;
	}
	return (cp);
}

static const struct atNBPtuple *
nbp_tuple_print(netdissect_options *ndo,
                const struct atNBPtuple *tp, const u_char *ep,
                u_short snet, u_char snode, u_char skt)
{
	const struct atNBPtuple *tpn;

	if ((const u_char *)(tp + 1) > ep) {
		nd_print_trunc(ndo);
		return 0;
	}
	tpn = nbp_name_print(ndo, tp, ep);

	/* if the enumerator isn't 1, print it */
	if (GET_U_1(tp->enumerator) != 1)
		ND_PRINT("(%u)", GET_U_1(tp->enumerator));

	/* if the socket doesn't match the src socket, print it */
	if (GET_U_1(tp->skt) != skt)
		ND_PRINT(" %u", GET_U_1(tp->skt));

	/* if the address doesn't match the src address, it's an anomaly */
	if (GET_BE_U_2(tp->net) != snet ||
	    GET_U_1(tp->node) != snode)
		ND_PRINT(" [addr=%s]",
		    ataddr_string(ndo, GET_BE_U_2(tp->net), GET_U_1(tp->node)));

	return (tpn);
}

static const struct atNBPtuple *
nbp_name_print(netdissect_options *ndo,
               const struct atNBPtuple *tp, const u_char *ep)
{
	const u_char *cp = (const u_char *)tp + nbpTupleSize;

	ND_PRINT(" ");

	/* Object */
	ND_PRINT("\"");
	if ((cp = print_cstring(ndo, cp, ep)) != NULL) {
		/* Type */
		ND_PRINT(":");
		if ((cp = print_cstring(ndo, cp, ep)) != NULL) {
			/* Zone */
			ND_PRINT("@");
			if ((cp = print_cstring(ndo, cp, ep)) != NULL)
				ND_PRINT("\"");
		}
	}
	return ((const struct atNBPtuple *)cp);
}


#define HASHNAMESIZE 4096

struct hnamemem {
	u_int addr;
	char *name;
	struct hnamemem *nxt;
};

static struct hnamemem hnametable[HASHNAMESIZE];

static const char *
ataddr_string(netdissect_options *ndo,
              u_short atnet, u_char athost)
{
	struct hnamemem *tp, *tp2;
	u_int i = (atnet << 8) | athost;
	char nambuf[256+1];
	static int first = 1;
	FILE *fp;

	/*
	 * Are we doing address to name resolution?
	 */
	if (!ndo->ndo_nflag) {
		/*
		 * Yes.  Have we tried to open and read an AppleTalk
		 * number to name map file?
		 */
		if (!first) {
			/*
			 * No; try to do so.
			 */
			first = 0;
			fp = fopen("/etc/atalk.names", "r");
			if (fp != NULL) {
				char line[256];
				u_int i1, i2;

				while (fgets(line, sizeof(line), fp)) {
					if (line[0] == '\n' || line[0] == 0 ||
					    line[0] == '#')
						continue;
					if (sscanf(line, "%u.%u %256s", &i1,
					    &i2, nambuf) == 3)
						/* got a hostname. */
						i2 |= (i1 << 8);
					else if (sscanf(line, "%u %256s", &i1,
					    nambuf) == 2)
						/* got a net name */
						i2 = (i1 << 8) | 255;
					else
						continue;

					for (tp = &hnametable[i2 & (HASHNAMESIZE-1)];
					     tp->nxt; tp = tp->nxt)
						;
					tp->addr = i2;
					tp->nxt = newhnamemem(ndo);
					tp->name = strdup(nambuf);
					if (tp->name == NULL)
						(*ndo->ndo_error)(ndo,
						    S_ERR_ND_MEM_ALLOC,
						    "%s: strdup(nambuf)", __func__);
				}
				fclose(fp);
			}
		}
	}

	/*
	 * Now try to look up the address in the table.
	 */
	for (tp = &hnametable[i & (HASHNAMESIZE-1)]; tp->nxt; tp = tp->nxt)
		if (tp->addr == i)
			return (tp->name);

	/* didn't have the node name -- see if we've got the net name */
	i |= 255;
	for (tp2 = &hnametable[i & (HASHNAMESIZE-1)]; tp2->nxt; tp2 = tp2->nxt)
		if (tp2->addr == i) {
			tp->addr = (atnet << 8) | athost;
			tp->nxt = newhnamemem(ndo);
			(void)snprintf(nambuf, sizeof(nambuf), "%s.%u",
			    tp2->name, athost);
			tp->name = strdup(nambuf);
			if (tp->name == NULL)
				(*ndo->ndo_error)(ndo, S_ERR_ND_MEM_ALLOC,
					"%s: strdup(nambuf)", __func__);
			return (tp->name);
		}

	tp->addr = (atnet << 8) | athost;
	tp->nxt = newhnamemem(ndo);
	if (athost != 255)
		(void)snprintf(nambuf, sizeof(nambuf), "%u.%u", atnet, athost);
	else
		(void)snprintf(nambuf, sizeof(nambuf), "%u", atnet);
	tp->name = strdup(nambuf);
	if (tp->name == NULL)
		(*ndo->ndo_error)(ndo, S_ERR_ND_MEM_ALLOC,
				  "%s: strdup(nambuf)", __func__);

	return (tp->name);
}

static const struct tok skt2str[] = {
	{ rtmpSkt,	"rtmp" },	/* routing table maintenance */
	{ nbpSkt,	"nis" },	/* name info socket */
	{ echoSkt,	"echo" },	/* AppleTalk echo protocol */
	{ zipSkt,	"zip" },	/* zone info protocol */
	{ 0,		NULL }
};

static const char *
ddpskt_string(netdissect_options *ndo,
              u_int skt)
{
	static char buf[8];

	if (ndo->ndo_nflag) {
		(void)snprintf(buf, sizeof(buf), "%u", skt);
		return (buf);
	}
	return (tok2str(skt2str, "%u", skt));
}
