/*
 * Copyright (c) 1992, 1993, 1994, 1995, 1996, 1997
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
 * Code by Matt Thomas, Digital Equipment Corporation
 *	with an awful lot of hacking by Jeffrey Mogul, DECWRL
 *
 * $FreeBSD$
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/print-llc.c,v 1.53.2.3 2003/12/29 22:33:18 hannes Exp $";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"			/* must come after interface.h */

#include "llc.h"
#include "ethertype.h"

static struct tok llc_values[] = {
        { LLCSAP_NULL,     "Null" },
        { LLCSAP_GLOBAL,   "Global" },
        { LLCSAP_8021B_I,  "802.1B I" },
        { LLCSAP_8021B_G,  "802.1B G" },
        { LLCSAP_IP,       "IP" },
        { LLCSAP_PROWAYNM, "ProWay NM" },
        { LLCSAP_8021D,    "STP" },
        { LLCSAP_RS511,    "RS511" },
        { LLCSAP_ISO8208,  "ISO8208" },
        { LLCSAP_PROWAY,   "ProWay" },
        { LLCSAP_SNAP,     "SNAP" },
        { LLCSAP_IPX,      "IPX" },
        { LLCSAP_NETBEUI,  "NetBeui" },
        { LLCSAP_ISONS,    "OSI" },
};

static struct tok cmd2str[] = {
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
llc_print(const u_char *p, u_int length, u_int caplen,
	  const u_char *esrc, const u_char *edst, u_short *extracted_ethertype)
{
	struct llc llc;
	register u_short et;
	u_int16_t control;
	register int ret;

	if (caplen < 3) {
		(void)printf("[|llc]");
		default_print((u_char *)p, caplen);
		return(0);
	}

	/* Watch out for possible alignment problems */
	memcpy((char *)&llc, (char *)p, min(caplen, sizeof(llc)));

	if (eflag)
	  printf("LLC, dsap %s (0x%02x), ssap %s (0x%02x), cmd 0x%02x, ",
                 tok2str(llc_values,"Unknown",llc.dsap),
		 llc.dsap,
                 tok2str(llc_values,"Unknown",llc.ssap),
		 llc.ssap,
		 llc.llcu);

	if (llc.ssap == LLCSAP_GLOBAL && llc.dsap == LLCSAP_GLOBAL) {
		/*
		 * This is an Ethernet_802.3 IPX frame; it has an
		 * 802.3 header (i.e., an Ethernet header where the
		 * type/length field is <= ETHERMTU, i.e. it's a length
		 * field, not a type field), but has no 802.2 header -
		 * the IPX packet starts right after the Ethernet header,
		 * with a signature of two bytes of 0xFF (which is
		 * LLCSAP_GLOBAL).
		 *
		 * (It might also have been an Ethernet_802.3 IPX at
		 * one time, but got bridged onto another network,
		 * such as an 802.11 network; this has appeared in at
		 * least one capture file.)
		 */
		printf("(NOV-802.3) ");
		ipx_print(p, length);
		return (1);
	}

	if (llc.ssap == LLCSAP_8021D && llc.dsap == LLCSAP_8021D) {
		stp_print(p, length);
		return (1);
	}

	if (llc.ssap == LLCSAP_IP && llc.dsap == LLCSAP_IP) {
		ip_print(p+4, length-4);
		return (1);
	}

	if (llc.ssap == LLCSAP_IPX && llc.dsap == LLCSAP_IPX &&
	    llc.llcui == LLC_UI) {
		/*
		 * This is an Ethernet_802.2 IPX frame, with an 802.3
		 * header and an 802.2 LLC header with the source and
		 * destination SAPs being the IPX SAP.
		 *
		 * Skip DSAP, LSAP, and control field.
		 */
		printf("(NOV-802.2) ");
		p += 3;
		length -= 3;
		caplen -= 3;
		ipx_print(p, length);
		return (1);
	}

#ifdef TCPDUMP_DO_SMB
	if (llc.ssap == LLCSAP_NETBEUI && llc.dsap == LLCSAP_NETBEUI
	    && (!(llc.llcu & LLC_S_FMT) || llc.llcu == LLC_U_FMT)) {
		/*
		 * we don't actually have a full netbeui parser yet, but the
		 * smb parser can handle many smb-in-netbeui packets, which
		 * is very useful, so we call that
		 *
		 * We don't call it for S frames, however, just I frames
		 * (which are frames that don't have the low-order bit,
		 * LLC_S_FMT, set in the first byte of the control field)
		 * and UI frames (whose control field is just 3, LLC_U_FMT).
		 */

		/*
		 * Skip the DSAP and LSAP.
		 */
		p += 2;
		length -= 2;
		caplen -= 2;

		/*
		 * OK, what type of LLC frame is this?  The length
		 * of the control field depends on that - I frames
		 * have a two-byte control field, and U frames have
		 * a one-byte control field.
		 */
		if (llc.llcu == LLC_U_FMT) {
			control = llc.llcu;
			p += 1;
			length -= 1;
			caplen -= 1;
		} else {
			/*
			 * The control field in I and S frames is
			 * little-endian.
			 */
			control = EXTRACT_LE_16BITS(&llc.llcu);
			p += 2;
			length -= 2;
			caplen -= 2;
		}
		netbeui_print(control, p, length);
		return (1);
	}
#endif
	if (llc.ssap == LLCSAP_ISONS && llc.dsap == LLCSAP_ISONS
	    && llc.llcui == LLC_UI) {
		isoclns_print(p + 3, length - 3, caplen - 3);
		return (1);
	}

	if (llc.ssap == LLCSAP_SNAP && llc.dsap == LLCSAP_SNAP
	    && llc.llcui == LLC_UI) {
		u_int32_t orgcode;

		if (caplen < sizeof(llc)) {
			(void)printf("[|llc-snap]");
			default_print((u_char *)p, caplen);
			return (0);
		}

		caplen -= sizeof(llc);
		length -= sizeof(llc);
		p += sizeof(llc);

		orgcode = EXTRACT_24BITS(&llc.llc_orgcode[0]);
		et = EXTRACT_16BITS(&llc.llc_ethertype[0]);
		/*
		 * XXX - what *is* the right bridge pad value here?
		 * Does anybody ever bridge one form of LAN traffic
		 * over a networking type that uses 802.2 LLC?
		 */
		ret = snap_print(p, length, caplen, extracted_ethertype,
		    orgcode, et, 2);
		if (ret)
			return (ret);
	}

	if ((llc.ssap & ~LLC_GSAP) == llc.dsap) {
		if (eflag || esrc == NULL || edst == NULL)
			(void)printf("%s ", llcsap_string(llc.dsap));
		else
			(void)printf("%s > %s %s ",
					etheraddr_string(esrc),
					etheraddr_string(edst),
					llcsap_string(llc.dsap));
	} else {
		if (eflag || esrc == NULL || edst == NULL)
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
		u_int16_t cmd;
		const char *m;
		char f;

		cmd = LLC_U_CMD(llc.llcu);
		m = tok2str(cmd2str, "%02x", cmd);
		switch ((llc.ssap & LLC_GSAP) | (llc.llcu & LLC_U_POLL)) {
			case 0:			f = 'C'; break;
			case LLC_GSAP:		f = 'R'; break;
			case LLC_U_POLL:	f = 'P'; break;
			case LLC_GSAP|LLC_U_POLL: f = 'F'; break;
			default:		f = '?'; break;
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

		/*
		 * The control field in I and S frames is little-endian.
		 */
		control = EXTRACT_LE_16BITS(&llc.llcu);
		switch ((llc.ssap & LLC_GSAP) | (control & LLC_IS_POLL)) {
			case 0:			f = 'C'; break;
			case LLC_GSAP:		f = 'R'; break;
			case LLC_IS_POLL:	f = 'P'; break;
			case LLC_GSAP|LLC_IS_POLL: f = 'F'; break;
			default:		f = '?'; break;
		}

		if ((control & LLC_S_FMT) == LLC_S_FMT) {
			static const char *llc_s[] = { "rr", "rej", "rnr", "03" };
			(void)printf("%s (r=%d,%c)",
				llc_s[LLC_S_CMD(control)],
				LLC_IS_NR(control),
				f);
		} else {
			(void)printf("I (s=%d,r=%d,%c)",
				LLC_I_NS(control),
				LLC_IS_NR(control),
				f);
		}
		p += 4;
		length -= 4;
		caplen -= 4;
	}
	(void)printf(" len=%d", length);
	return(1);
}

int
snap_print(const u_char *p, u_int length, u_int caplen,
    u_short *extracted_ethertype, u_int32_t orgcode, u_short et,
    u_int bridge_pad)
{
	register int ret;

	switch (orgcode) {
	case OUI_ENCAP_ETHER:
	case OUI_CISCO_90:
		/*
		 * This is an encapsulated Ethernet packet,
		 * or a packet bridged by some piece of
		 * Cisco hardware; the protocol ID is
		 * an Ethernet protocol type.
		 */
		ret = ether_encap_print(et, p, length, caplen,
		    extracted_ethertype);
		if (ret)
			return (ret);
		break;

	case OUI_APPLETALK:
		if (et == ETHERTYPE_ATALK) {
			/*
			 * No, I have no idea why Apple used one
			 * of their own OUIs, rather than
			 * 0x000000, and an Ethernet packet
			 * type, for Appletalk data packets,
			 * but used 0x000000 and an Ethernet
			 * packet type for AARP packets.
			 */
			ret = ether_encap_print(et, p, length, caplen,
			    extracted_ethertype);
			if (ret)
				return (ret);
		}
		break;

	case OUI_CISCO:
		if (et == PID_CISCO_CDP) {
			cdp_print(p, length, caplen);
			return (1);
		}
		break;

	case OUI_RFC2684:
		switch (et) {

		case PID_RFC2684_ETH_FCS:
		case PID_RFC2684_ETH_NOFCS:
			/*
			 * XXX - remove the last two bytes for
			 * PID_RFC2684_ETH_FCS?
			 */
			/*
			 * Skip the padding.
			 */
			caplen -= bridge_pad;
			length -= bridge_pad;
			p += bridge_pad;

			/*
			 * What remains is an Ethernet packet.
			 */
			ether_print(p, length, caplen);
			return (1);

		case PID_RFC2684_802_5_FCS:
		case PID_RFC2684_802_5_NOFCS:
			/*
			 * XXX - remove the last two bytes for
			 * PID_RFC2684_ETH_FCS?
			 */
			/*
			 * Skip the padding, but not the Access
			 * Control field.
			 */
			caplen -= bridge_pad;
			length -= bridge_pad;
			p += bridge_pad;

			/*
			 * What remains is an 802.5 Token Ring
			 * packet.
			 */
			token_print(p, length, caplen);
			return (1);

		case PID_RFC2684_FDDI_FCS:
		case PID_RFC2684_FDDI_NOFCS:
			/*
			 * XXX - remove the last two bytes for
			 * PID_RFC2684_ETH_FCS?
			 */
			/*
			 * Skip the padding.
			 */
			caplen -= bridge_pad + 1;
			length -= bridge_pad + 1;
			p += bridge_pad + 1;

			/*
			 * What remains is an FDDI packet.
			 */
			fddi_print(p, length, caplen);
			return (1);

		case PID_RFC2684_BPDU:
			stp_print(p, length);
			return (1);
		}
	}
	return (0);
}
