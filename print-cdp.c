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
 * Code by Gert Doering, SpaceNet GmbH, gert@space.net
 *
 * Reference documentation:
 *    https://web.archive.org/web/20000914194913/http://www.cisco.com/univercd/cc/td/doc/product/lan/trsrb/frames.pdf
 */

/* \summary: Cisco Discovery Protocol (CDP) printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include <string.h>

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"
#include "nlpid.h"


#define CDP_HEADER_LEN             4
#define CDP_HEADER_VERSION_OFFSET  0
#define CDP_HEADER_TTL_OFFSET      1
#define CDP_HEADER_CHECKSUM_OFFSET 2

#define CDP_TLV_HEADER_LEN  4
#define CDP_TLV_TYPE_OFFSET 0
#define CDP_TLV_LEN_OFFSET  2

static const struct tok cdp_capability_values[] = {
    { 0x01,             "Router" },
    { 0x02,             "Transparent Bridge" },
    { 0x04,             "Source Route Bridge" },
    { 0x08,             "L2 Switch" },
    { 0x10,             "L3 capable" },
    { 0x20,             "IGMP snooping" },
    { 0x40,             "L1 capable" },
    { 0, NULL }
};

static void cdp_print_addr(netdissect_options *, const u_char *, u_int);
static void cdp_print_prefixes(netdissect_options *, const u_char *, u_int);

static void
cdp_print_string(netdissect_options *ndo,
                 const u_char *cp, const u_int len)
{
	ND_PRINT("'");
	(void)nd_printn(ndo, cp, len, NULL);
	ND_PRINT("'");
}

static void
cdp_print_power(netdissect_options *ndo,
                const u_char *cp, const u_int len)
{
	u_int val = 0;

	switch (len) {
	case 1:
		val = GET_U_1(cp);
		break;
	case 2:
		val = GET_BE_U_2(cp);
		break;
	case 3:
		val = GET_BE_U_3(cp);
		break;
	}
	ND_PRINT("%1.2fW", val / 1000.0);
}

static void
cdp_print_capability(netdissect_options *ndo,
                     const u_char *cp, const u_int len _U_)
{
	uint32_t val = GET_BE_U_4(cp);

	ND_PRINT("(0x%08x): %s", val,
	         bittok2str(cdp_capability_values, "none", val));
}

/* Rework the version string to get a nice indentation. */
static void
cdp_print_version(netdissect_options *ndo,
                  const u_char *cp, const u_int len)
{
	unsigned i;

	ND_PRINT("\n\t  ");
	for (i = 0; i < len; i++) {
		u_char c = GET_U_1(cp + i);

		if (c == '\n')
			ND_PRINT("\n\t  ");
		else
			fn_print_char(ndo, c);
	}
}

static void
cdp_print_uint16(netdissect_options *ndo,
                 const u_char *cp, const u_int len _U_)
{
	ND_PRINT("%u", GET_BE_U_2(cp));
}

static void
cdp_print_duplex(netdissect_options *ndo,
                 const u_char *cp, const u_int len _U_)
{
	ND_PRINT("%s", GET_U_1(cp) ? "full": "half");
}

/* https://www.cisco.com/c/en/us/td/docs/voice_ip_comm/cata/186/2_12_m/english/release/notes/186rn21m.html
* plus more details from other sources
*
* There are apparently versions of the request with both
* 2 bytes and 3 bytes of value.  The 3 bytes of value
* appear to be a 1-byte application type followed by a
* 2-byte VLAN ID; the 2 bytes of value are unknown
* (they're 0x20 0x00 in some captures I've seen; that
* is not a valid VLAN ID, as VLAN IDs are 12 bits).
*
* The replies all appear to be 3 bytes long.
*/
static void
cdp_print_ata186(netdissect_options *ndo,
                 const u_char *cp, const u_int len)
{
	if (len == 2)
		ND_PRINT("unknown 0x%04x", GET_BE_U_2(cp));
	else
		ND_PRINT("app %u, vlan %u", GET_U_1(cp), GET_BE_U_2(cp + 1));
}

static void
cdp_print_mtu(netdissect_options *ndo,
              const u_char *cp, const u_int len _U_)
{
	ND_PRINT("%u bytes", GET_BE_U_4(cp));
}

static void
cdp_print_uint8x(netdissect_options *ndo,
                 const u_char *cp, const u_int len _U_)
{
	ND_PRINT("0x%02x", GET_U_1(cp));
}

static void
cdp_print_phys_loc(netdissect_options *ndo,
                   const u_char *cp, const u_int len)
{
	ND_PRINT("0x%02x", GET_U_1(cp));
	if (len > 1) {
		ND_PRINT("/");
		(void)nd_printn(ndo, cp + 1, len - 1, NULL);
	}
}

struct cdp_tlvinfo {
	const char *name;
	void (*printer)(netdissect_options *ndo, const u_char *, u_int);
	int min_len, max_len;
};

#define T_DEV_ID 0x01
#define T_MAX 0x17
static const struct cdp_tlvinfo cdptlvs[T_MAX + 1] = {
	/* 0x00 */
	[ T_DEV_ID ] = { "Device-ID", cdp_print_string, -1, -1 },
	[ 0x02 ] = { "Address", cdp_print_addr, -1, -1 },
	[ 0x03 ] = { "Port-ID", cdp_print_string, -1, -1 },
	[ 0x04 ] = { "Capability", cdp_print_capability, 4, 4 },
	[ 0x05 ] = { "Version String", cdp_print_version, -1, -1 },
	[ 0x06 ] = { "Platform", cdp_print_string, -1, -1 },
	[ 0x07 ] = { "Prefixes", cdp_print_prefixes, -1, -1 },
	/* not documented */
	[ 0x08 ] = { "Protocol-Hello option", NULL, -1, -1 },
	/* CDPv2 */
	[ 0x09 ] = { "VTP Management Domain", cdp_print_string, -1, -1 },
	/* CDPv2 */
	[ 0x0a ] = { "Native VLAN ID", cdp_print_uint16, 2, 2 },
	/* CDPv2 */
	[ 0x0b ] = { "Duplex", cdp_print_duplex, 1, 1 },
	/* 0x0c */
	/* 0x0d */
	/* incomplete doc. */
	[ 0x0e ] = { "ATA-186 VoIP VLAN assignment", cdp_print_ata186, 3, 3 },
	/* incomplete doc. */
	[ 0x0f ] = { "ATA-186 VoIP VLAN request", cdp_print_ata186, 2, 3 },
	/* not documented */
	[ 0x10 ] = { "power consumption", cdp_print_power, 1, 3 },
	/* not documented */
	[ 0x11 ] = { "MTU", cdp_print_mtu, 4, 4 },
	/* not documented */
	[ 0x12 ] = { "AVVID trust bitmap", cdp_print_uint8x, 1, 1 },
	/* not documented */
	[ 0x13 ] = { "AVVID untrusted ports CoS", cdp_print_uint8x, 1, 1 },
	/* not documented */
	[ 0x14 ] = { "System Name", cdp_print_string, -1, -1 },
	/* not documented */
	[ 0x15 ] = { "System Object ID (not decoded)", NULL, -1, -1 },
	[ 0x16 ] = { "Management Addresses", cdp_print_addr, 4, -1 },
	/* not documented */
	[ 0x17 ] = { "Physical Location", cdp_print_phys_loc, 1, -1 },
};

void
cdp_print(netdissect_options *ndo,
          const u_char *tptr, u_int length)
{
	u_int orig_length = length;
	uint16_t checksum;

	ndo->ndo_protocol = "cdp";

	if (length < CDP_HEADER_LEN) {
		ND_PRINT(" (packet length %u < %u)", length, CDP_HEADER_LEN);
		goto invalid;
	}
	ND_PRINT("CDPv%u, ttl: %us",
	         GET_U_1(tptr + CDP_HEADER_VERSION_OFFSET),
	         GET_U_1(tptr + CDP_HEADER_TTL_OFFSET));
	checksum = GET_BE_U_2(tptr + CDP_HEADER_CHECKSUM_OFFSET);
	if (ndo->ndo_vflag)
		ND_PRINT(", checksum: 0x%04x (unverified), length %u",
		         checksum, orig_length);
	tptr += CDP_HEADER_LEN;
	length -= CDP_HEADER_LEN;

	while (length) {
		u_int type, len;
		const struct cdp_tlvinfo *info;
		const char *name;
		u_char covered = 0;

		if (length < CDP_TLV_HEADER_LEN) {
			ND_PRINT(" (remaining packet length %u < %u)",
			         length, CDP_TLV_HEADER_LEN);
			goto invalid;
		}
		type = GET_BE_U_2(tptr + CDP_TLV_TYPE_OFFSET);
		len  = GET_BE_U_2(tptr + CDP_TLV_LEN_OFFSET); /* object length includes the 4 bytes header length */
		info = type <= T_MAX ? &cdptlvs[type] : NULL;
		name = (info && info->name) ? info->name : "unknown field type";
		if (len < CDP_TLV_HEADER_LEN) {
			if (ndo->ndo_vflag)
				ND_PRINT("\n\t%s (0x%02x), TLV length: %u byte%s (too short)",
				         name, type, len, PLURAL_SUFFIX(len));
			else
				ND_PRINT(", %s TLV length %u too short",
				         name, len);
			goto invalid;
		}
		if (len > length) {
			ND_PRINT(" (TLV length %u > %u)", len, length);
			goto invalid;
		}
		tptr += CDP_TLV_HEADER_LEN;
		length -= CDP_TLV_HEADER_LEN;
		len -= CDP_TLV_HEADER_LEN;

		/* In non-verbose mode just print Device-ID. */
		if (!ndo->ndo_vflag && type == T_DEV_ID)
			ND_PRINT(", Device-ID ");
		else if (ndo->ndo_vflag)
			ND_PRINT("\n\t%s (0x%02x), value length: %u byte%s: ",
			         name, type, len, PLURAL_SUFFIX(len));

		if (info) {
			if ((info->min_len > 0 && len < (unsigned)info->min_len) ||
			    (info->max_len > 0 && len > (unsigned)info->max_len))
				ND_PRINT(" (malformed TLV)");
			else if (ndo->ndo_vflag || type == T_DEV_ID) {
				if (info->printer)
					info->printer(ndo, tptr, len);
				else
					ND_TCHECK_LEN(tptr, len);
				/*
				 * When the type is defined without a printer,
				 * do not print the hex dump.
				 */
				covered = 1;
			}
		}

		if (!covered) {
			ND_TCHECK_LEN(tptr, len);
			print_unknown_data(ndo, tptr, "\n\t  ", len);
		}
		tptr += len;
		length -= len;
	}
	if (ndo->ndo_vflag < 1)
		ND_PRINT(", length %u", orig_length);

	return;
invalid:
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(tptr, length);
}

/*
 * Protocol type values.
 *
 * PT_NLPID means that the protocol type field contains an OSI NLPID.
 *
 * PT_IEEE_802_2 means that the protocol type field contains an IEEE 802.2
 * LLC header that specifies that the payload is for that protocol.
 */
#define PT_NLPID		1	/* OSI NLPID */
#define PT_IEEE_802_2		2	/* IEEE 802.2 LLC header */

static void
cdp_print_addr(netdissect_options *ndo,
               const u_char * p, u_int l)
{
	u_int num;
	static const u_char prot_ipv6[] = {
		0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00, 0x86, 0xdd
	};

	if (l < 4) {
		ND_PRINT(" (not enough space for num)");
		goto invalid;
	}
	num = GET_BE_U_4(p);
	p += 4;
	l -= 4;

	while (num) {
		u_int pt, pl, al;

		if (l < 2) {
			ND_PRINT(" (not enough space for PT+PL)");
			goto invalid;
		}
		pt = GET_U_1(p);		/* type of "protocol" field */
		pl = GET_U_1(p + 1);	/* length of "protocol" field */
		p += 2;
		l -= 2;

		if (l < pl + 2) {
			ND_PRINT(" (not enough space for P+AL)");
			goto invalid;
		}
		/* Skip the protocol for now. */
		al = GET_BE_U_2(p + pl);	/* address length */

		if (pt == PT_NLPID && pl == 1 && GET_U_1(p) == NLPID_IP &&
		    al == 4) {
			/*
			 * IPv4: protocol type = NLPID, protocol length = 1
			 * (1-byte NLPID), protocol = 0xcc (NLPID for IPv4),
			 * address length = 4
			 */
			p += pl + 2;
			l -= pl + 2;
			/* p is just beyond al now. */
			if (l < al) {
				ND_PRINT(" (not enough space for A)");
				goto invalid;
			}
			ND_PRINT("IPv4 (%u) %s", num, GET_IPADDR_STRING(p));
			p += al;
			l -= al;
		}
		else if (pt == PT_IEEE_802_2 && pl == 8 &&
		         memcmp(p, prot_ipv6, 8) == 0 && al == 16) {
			/*
			 * IPv6: protocol type = IEEE 802.2 header,
			 * protocol length = 8 (size of LLC+SNAP header),
			 * protocol = LLC+SNAP header with the IPv6
			 * Ethertype, address length = 16
			 */
			p += pl + 2;
			l -= pl + 2;
			/* p is just beyond al now. */
			if (l < al) {
				ND_PRINT(" (not enough space for A)");
				goto invalid;
			}
			ND_PRINT("IPv6 (%u) %s", num, GET_IP6ADDR_STRING(p));
			p += al;
			l -= al;
		}
		else {
			/*
			 * Generic case: just print raw data
			 */
			ND_PRINT("pt=0x%02x, pl=%u, pb=", pt, pl);
			while (pl != 0) {
				ND_PRINT(" %02x", GET_U_1(p));
				p++;
				l--;
				pl--;
			}
			ND_PRINT(", al=%u, a=", al);
			p += 2;
			l -= 2;
			/* p is just beyond al now. */
			if (l < al) {
				ND_PRINT(" (not enough space for A)");
				goto invalid;
			}
			while (al != 0) {
				ND_PRINT(" %02x", GET_U_1(p));
				p++;
				l--;
				al--;
			}
		}
		num--;
		if (num)
			ND_PRINT(" ");
	}
	if (l)
		ND_PRINT(" (%u bytes of stray data)", l);
	return;

invalid:
	ND_TCHECK_LEN(p, l);
}

static void
cdp_print_prefixes(netdissect_options *ndo,
                   const u_char * p, u_int l)
{
	if (l % 5) {
		ND_PRINT(" [length %u is not a multiple of 5]", l);
		goto invalid;
	}

	ND_PRINT(" IPv4 Prefixes (%u):", l / 5);

	while (l > 0) {
		ND_PRINT(" %u.%u.%u.%u/%u",
		         GET_U_1(p), GET_U_1(p + 1), GET_U_1(p + 2),
		         GET_U_1(p + 3), GET_U_1(p + 4));
		l -= 5;
		p += 5;
	}
	return;

invalid:
	ND_TCHECK_LEN(p, l);
}
