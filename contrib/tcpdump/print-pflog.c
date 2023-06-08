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

/* \summary: *BSD/Darwin packet filter log file printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "extract.h"
#include "af.h"

#include "pflog.h"

static const struct tok pf_reasons[] = {
	{ PFRES_MATCH,		"0(match)" },
	{ PFRES_BADOFF,		"1(bad-offset)" },
	{ PFRES_FRAG,		"2(fragment)" },
	{ PFRES_NORM,		"3(short)" },
	{ PFRES_NORM,		"4(normalize)" },
	{ PFRES_MEMORY,		"5(memory)" },
	{ PFRES_TS,		"6(bad-timestamp)" },
	{ PFRES_CONGEST,	"7(congestion)" },
	{ PFRES_IPOPTIONS,	"8(ip-option)" },
	{ PFRES_PROTCKSUM,	"9(proto-cksum)" },
	{ PFRES_BADSTATE,	"10(state-mismatch)" },
	{ PFRES_STATEINS,	"11(state-insert)" },
	{ PFRES_MAXSTATES,	"12(state-limit)" },
	{ PFRES_SRCLIMIT,	"13(src-limit)" },
	{ PFRES_SYNPROXY,	"14(synproxy)" },
#if defined(__FreeBSD__)
	{ PFRES_MAPFAILED,	"15(map-failed)" },
#elif defined(__NetBSD__)
	{ PFRES_STATELOCKED,	"15(state-locked)" },
#elif defined(__OpenBSD__)
	{ PFRES_TRANSLATE,	"15(translate)" },
	{ PFRES_NOROUTE,	"16(no-route)" },
#elif defined(__APPLE__)
	{ PFRES_DUMMYNET,	"15(dummynet)" },
#endif
	{ 0,	NULL }
};

static const struct tok pf_actions[] = {
	{ PF_PASS,		"pass" },
	{ PF_DROP,		"block" },
	{ PF_SCRUB,		"scrub" },
	{ PF_NOSCRUB,		"scrub" },
	{ PF_NAT,		"nat" },
	{ PF_NONAT,		"nonat" },
	{ PF_BINAT,		"binat" },
	{ PF_NOBINAT,		"nobinat" },
	{ PF_RDR,		"rdr" },
	{ PF_NORDR,		"nordr" },
	{ PF_SYNPROXY_DROP,	"synproxy-drop" },
#if defined(__FreeBSD__)
	{ PF_DEFER,		"defer" },
	{ PF_MATCH,		"match" },
#elif defined(__OpenBSD__)
	{ PF_DEFER,		"defer" },
	{ PF_MATCH,		"match" },
	{ PF_DIVERT,		"divert" },
	{ PF_RT,		"rt" },
	{ PF_AFRT,		"afrt" },
#elif defined(__APPLE__)
	{ PF_DUMMYNET,		"dummynet" },
	{ PF_NODUMMYNET,	"nodummynet" },
	{ PF_NAT64,		"nat64" },
	{ PF_NONAT64,		"nonat64" },
#endif
	{ 0,			NULL }
};

static const struct tok pf_directions[] = {
	{ PF_INOUT,	"in/out" },
	{ PF_IN,	"in" },
	{ PF_OUT,	"out" },
#if defined(__OpenBSD__)
	{ PF_FWD,	"fwd" },
#endif
	{ 0,		NULL }
};

static void
pflog_print(netdissect_options *ndo, const struct pfloghdr *hdr)
{
	uint32_t rulenr, subrulenr, ridentifier;

	ndo->ndo_protocol = "pflog";
	rulenr = GET_BE_U_4(&hdr->rulenr);
	subrulenr = GET_BE_U_4(&hdr->subrulenr);
	ridentifier = GET_BE_U_4(&hdr->ridentifier);
	if (subrulenr == (uint32_t)-1)
		ND_PRINT("rule %u/", rulenr);
	else {
		ND_PRINT("rule %u.", rulenr);
		nd_printjnp(ndo, (const u_char*)hdr->ruleset, PFLOG_RULESET_NAME_SIZE);
		ND_PRINT(".%u/", subrulenr);
	}

	ND_PRINT("%s", tok2str(pf_reasons, "unkn(%u)", hdr->reason));

	if (hdr->uid != UID_MAX)
		ND_PRINT(" [uid %u]", (unsigned)hdr->uid);

	if (ridentifier != 0)
		ND_PRINT(" [ridentifier %u]", ridentifier);

	ND_PRINT(": %s %s on ",
	    tok2str(pf_actions, "unkn(%u)", GET_U_1(&hdr->action)),
	    tok2str(pf_directions, "unkn(%u)", GET_U_1(&hdr->dir)));
	nd_printjnp(ndo, (const u_char*)hdr->ifname, PFLOG_IFNAMSIZ);
	ND_PRINT(": ");
}

void
pflog_if_print(netdissect_options *ndo, const struct pcap_pkthdr *h,
               const u_char *p)
{
	u_int length = h->len;
	u_int hdrlen;
	u_int caplen = h->caplen;
	const struct pfloghdr *hdr;
	uint8_t af;

	ndo->ndo_protocol = "pflog";
	/* check length */
	if (caplen < sizeof(uint8_t)) {
		nd_print_trunc(ndo);
		ndo->ndo_ll_hdr_len += h->caplen;
		return;
	}

#define MIN_PFLOG_HDRLEN	45
	hdr = (const struct pfloghdr *)p;
	if (GET_U_1(&hdr->length) < MIN_PFLOG_HDRLEN) {
		ND_PRINT("[pflog: invalid header length!]");
		ndo->ndo_ll_hdr_len += GET_U_1(&hdr->length);	/* XXX: not really */
		return;
	}
	hdrlen = roundup2(hdr->length, 4);

	if (caplen < hdrlen) {
		nd_print_trunc(ndo);
		ndo->ndo_ll_hdr_len += hdrlen;	/* XXX: true? */
		return;
	}

	/* print what we know */
	ND_TCHECK_SIZE(hdr);
	if (ndo->ndo_eflag)
		pflog_print(ndo, hdr);

	/* skip to the real packet */
	af = GET_U_1(&hdr->af);
	length -= hdrlen;
	caplen -= hdrlen;
	p += hdrlen;
	switch (af) {

		/*
		 * If there's a system that doesn't use the AF_INET
		 * from 4.2BSD, feel free to add its value to af.h
		 * and use it here.
		 *
		 * Hopefully, there isn't.
		 */
		case BSD_AFNUM_INET:
		        ip_print(ndo, p, length);
			break;

		/*
		 * Try all AF_INET6 values for all systems with pflog,
		 * including Darwin.
		 */
		case BSD_AFNUM_INET6_BSD:
		case BSD_AFNUM_INET6_FREEBSD:
		case BSD_AFNUM_INET6_DARWIN:
			ip6_print(ndo, p, length);
			break;

	default:
		/* address family not handled, print raw packet */
		if (!ndo->ndo_eflag)
			pflog_print(ndo, hdr);
		if (!ndo->ndo_suppress_default_print)
			ND_DEFAULTPRINT(p, caplen);
	}

	ndo->ndo_ll_hdr_len += hdrlen;
	return;
trunc:
	nd_print_trunc(ndo);
	ndo->ndo_ll_hdr_len += hdrlen;
}
