/*
 * Copyright (c) 1992, 1993, 1994
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

/*
 * Code by Matt Thomas, Digital Equipment Corporation
 *	with an awful lot of hacking by Jeffrey Mogul, DECWRL
 */

#ifndef lint
static  char rcsid[] =
	"@(#)$Header: /pub/FreeBSD/FreeBSD-CVS/src/usr.sbin/tcpdump/tcpdump/print-llc.c,v 1.1 1995/03/08 12:52:36 olah Exp $";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>

#include <netinet/in.h>

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"			/* must come after interface.h */

#include "llc.h"

static struct token cmd2str[] = {
	{ LLC_UI,	"ui" },
	{ LLC_TEST,	"test" },
	{ LLC_XID,	"xid" },
	{ LLC_UA,	"ua" },
	{ LLC_DISC,	"disc" },
	{ LLC_DM,	"dm" },
	{ LLC_SABME,	"sabme" },
	{ LLC_FRMR,	"frmr" },
	{ 0,		NULL }
};

/*
 * Returns non-zero IFF it succeeds in printing the header
 */
int
llc_print(const u_char *p, int length, int caplen,
	  const u_char *esrc, const u_char *edst)
{
	struct llc llc;
	register u_short et;
	register int ret;

	if (caplen < 3) {
		(void)printf("[|llc]");
		default_print((u_char *)p, caplen);
		return(0);
	}

	/* Watch out for possible alignment problems */
	bcopy((char *)p, (char *)&llc, min(caplen, sizeof(llc)));

	if (llc.ssap == LLCSAP_GLOBAL && llc.dsap == LLCSAP_GLOBAL) {
		ipx_print(p, length);
		return (1);
	}
#ifdef notyet
	else if (p[0] == 0xf0 && p[1] == 0xf0)
		netbios_print(p, length);
#endif
	if (llc.ssap == LLCSAP_ISONS && llc.dsap == LLCSAP_ISONS
	    && llc.llcui == LLC_UI) {
		isoclns_print(p + 3, length - 3, caplen - 3, esrc, edst);
		return (1);
	}

	if (llc.ssap == LLCSAP_SNAP && llc.dsap == LLCSAP_SNAP
	    && llc.llcui == LLC_UI) {
		if (caplen < sizeof(llc)) {
		    (void)printf("[|llc-snap]");
		    default_print((u_char *)p, caplen);
		    return (0);
		}
		if (vflag)
			(void)printf("snap %s ", protoid_string(llc.llcpi));

		caplen -= sizeof(llc);
		length -= sizeof(llc);
		p += sizeof(llc);

		/* This is an encapsulated Ethernet packet */
		et = EXTRACT_SHORT(&llc.ethertype[0]);
		ret = ether_encap_print(et, p, length, caplen);
		if (ret)
			return (ret);
	}

	if ((llc.ssap & ~LLC_GSAP) == llc.dsap) {
		if (eflag)
			(void)printf("%s ", llcsap_string(llc.dsap));
		else
			(void)printf("%s > %s %s ",
					etheraddr_string(esrc),
					etheraddr_string(edst),
					llcsap_string(llc.dsap));
	} else {
		if (eflag)
			(void)printf("%s > %s ",
				llcsap_string(llc.ssap & ~LLC_GSAP),
				llcsap_string(llc.dsap));
		else
			(void)printf("%s %s > %s %s ",
				etheraddr_string(esrc),
				llcsap_string(llc.ssap & ~LLC_GSAP),
				etheraddr_string(edst),
				llcsap_string(llc.dsap));
	}

	if ((llc.llcu & LLC_U_FMT) == LLC_U_FMT) {
		const char *m;
		char f;
		m = tok2str(cmd2str, "%02x", LLC_U_CMD(llc.llcu));
		switch ((llc.ssap & LLC_GSAP) | (llc.llcu & LLC_U_POLL)) {
		    case 0:			f = 'C'; break;
		    case LLC_GSAP:		f = 'R'; break;
		    case LLC_U_POLL:		f = 'P'; break;
		    case LLC_GSAP|LLC_U_POLL:	f = 'F'; break;
		    default:			f = '?'; break;
		}

		printf("%s/%c", m, f);

		p += 3;
		length -= 3;
		caplen -= 3;

		if ((llc.llcu & ~LLC_U_POLL) == LLC_XID) {
		    if (*p == LLC_XID_FI) {
			printf(": %02x %02x", p[1], p[2]);
			p += 3;
			length -= 3;
			caplen -= 3;
		    }
		}
	} else {
		char f;
		llc.llcis = ntohs(llc.llcis);
		switch ((llc.ssap & LLC_GSAP) | (llc.llcu & LLC_U_POLL)) {
		    case 0:			f = 'C'; break;
		    case LLC_GSAP:		f = 'R'; break;
		    case LLC_U_POLL:		f = 'P'; break;
		    case LLC_GSAP|LLC_U_POLL:	f = 'F'; break;
		    default:			f = '?'; break;
		}

		if ((llc.llcu & LLC_S_FMT) == LLC_S_FMT) {
			static char *llc_s[] = { "rr", "rej", "rnr", "03" };
			(void)printf("%s (r=%d,%c)",
				llc_s[LLC_S_CMD(llc.llcis)],
				LLC_IS_NR(llc.llcis),
				f);
		} else {
			(void)printf("I (s=%d,r=%d,%c)",
				LLC_I_NS(llc.llcis),
				LLC_IS_NR(llc.llcis),
				f);
		}
		p += 4;
		length -= 4;
		caplen -= 4;
	}
	(void)printf(" len=%d", length);
	if (caplen > 0) {
		default_print_unaligned(p, caplen);
	}
	return(1);
}
