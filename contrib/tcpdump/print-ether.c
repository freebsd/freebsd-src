/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 2000
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

/* \summary: Ethernet printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "extract.h"
#include "addrtoname.h"
#include "ethertype.h"

/*
 * Structure of an Ethernet header.
 */
struct	ether_header {
	nd_mac_addr	ether_dhost;
	nd_mac_addr	ether_shost;
	nd_uint16_t	ether_length_type;
};

/*
 * Length of an Ethernet header; note that some compilers may pad
 * "struct ether_header" to a multiple of 4 bytes, for example, so
 * "sizeof (struct ether_header)" may not give the right answer.
 */
#define ETHER_HDRLEN		14

const struct tok ethertype_values[] = {
    { ETHERTYPE_IP,		"IPv4" },
    { ETHERTYPE_MPLS,		"MPLS unicast" },
    { ETHERTYPE_MPLS_MULTI,	"MPLS multicast" },
    { ETHERTYPE_IPV6,		"IPv6" },
    { ETHERTYPE_8021Q,		"802.1Q" },
    { ETHERTYPE_8021Q9100,	"802.1Q-9100" },
    { ETHERTYPE_8021QinQ,	"802.1Q-QinQ" },
    { ETHERTYPE_8021Q9200,	"802.1Q-9200" },
    { ETHERTYPE_MACSEC,		"802.1AE MACsec" },
    { ETHERTYPE_VMAN,		"VMAN" },
    { ETHERTYPE_PUP,            "PUP" },
    { ETHERTYPE_ARP,            "ARP"},
    { ETHERTYPE_REVARP,         "Reverse ARP"},
    { ETHERTYPE_NS,             "NS" },
    { ETHERTYPE_SPRITE,         "Sprite" },
    { ETHERTYPE_TRAIL,          "Trail" },
    { ETHERTYPE_MOPDL,          "MOP DL" },
    { ETHERTYPE_MOPRC,          "MOP RC" },
    { ETHERTYPE_DN,             "DN" },
    { ETHERTYPE_LAT,            "LAT" },
    { ETHERTYPE_SCA,            "SCA" },
    { ETHERTYPE_TEB,            "TEB" },
    { ETHERTYPE_LANBRIDGE,      "Lanbridge" },
    { ETHERTYPE_DECDNS,         "DEC DNS" },
    { ETHERTYPE_DECDTS,         "DEC DTS" },
    { ETHERTYPE_VEXP,           "VEXP" },
    { ETHERTYPE_VPROD,          "VPROD" },
    { ETHERTYPE_ATALK,          "Appletalk" },
    { ETHERTYPE_AARP,           "Appletalk ARP" },
    { ETHERTYPE_IPX,            "IPX" },
    { ETHERTYPE_PPP,            "PPP" },
    { ETHERTYPE_MPCP,           "MPCP" },
    { ETHERTYPE_SLOW,           "Slow Protocols" },
    { ETHERTYPE_PPPOED,         "PPPoE D" },
    { ETHERTYPE_PPPOES,         "PPPoE S" },
    { ETHERTYPE_EAPOL,          "EAPOL" },
    { ETHERTYPE_REALTEK,        "Realtek protocols" },
    { ETHERTYPE_MS_NLB_HB,      "MS NLB heartbeat" },
    { ETHERTYPE_JUMBO,          "Jumbo" },
    { ETHERTYPE_NSH,            "NSH" },
    { ETHERTYPE_LOOPBACK,       "Loopback" },
    { ETHERTYPE_ISO,            "OSI" },
    { ETHERTYPE_GRE_ISO,        "GRE-OSI" },
    { ETHERTYPE_CFM_OLD,        "CFM (old)" },
    { ETHERTYPE_CFM,            "CFM" },
    { ETHERTYPE_IEEE1905_1,     "IEEE1905.1" },
    { ETHERTYPE_LLDP,           "LLDP" },
    { ETHERTYPE_TIPC,           "TIPC"},
    { ETHERTYPE_GEONET_OLD,     "GeoNet (old)"},
    { ETHERTYPE_GEONET,         "GeoNet"},
    { ETHERTYPE_CALM_FAST,      "CALM FAST"},
    { ETHERTYPE_AOE,            "AoE" },
    { ETHERTYPE_PTP,            "PTP" },
    { ETHERTYPE_ARISTA,         "Arista Vendor Specific Protocol" },
    { 0, NULL}
};

static void
ether_addresses_print(netdissect_options *ndo, const u_char *src,
		      const u_char *dst)
{
	ND_PRINT("%s > %s, ",
		 GET_ETHERADDR_STRING(src), GET_ETHERADDR_STRING(dst));
}

static void
ether_type_print(netdissect_options *ndo, uint16_t type)
{
	if (!ndo->ndo_qflag)
		ND_PRINT("ethertype %s (0x%04x)",
			 tok2str(ethertype_values, "Unknown", type), type);
	else
		ND_PRINT("%s",
			 tok2str(ethertype_values, "Unknown Ethertype (0x%04x)", type));
}

/*
 * Common code for printing Ethernet frames.
 *
 * It can handle Ethernet headers with extra tag information inserted
 * after the destination and source addresses, as is inserted by some
 * switch chips, and extra encapsulation header information before
 * printing Ethernet header information (such as a LANE ID for ATM LANE).
 */
static u_int
ether_common_print(netdissect_options *ndo, const u_char *p, u_int length,
    u_int caplen,
    void (*print_switch_tag)(netdissect_options *ndo, const u_char *),
    u_int switch_tag_len,
    void (*print_encap_header)(netdissect_options *ndo, const u_char *),
    const u_char *encap_header_arg)
{
	const struct ether_header *ehp;
	u_int orig_length;
	u_int hdrlen;
	u_short length_type;
	int printed_length;
	int llc_hdrlen;
	struct lladdr_info src, dst;

	if (length < caplen) {
		ND_PRINT("[length %u < caplen %u]", length, caplen);
		nd_print_invalid(ndo);
		return length;
	}
	if (caplen < ETHER_HDRLEN + switch_tag_len) {
		nd_print_trunc(ndo);
		return caplen;
	}

	if (print_encap_header != NULL)
		(*print_encap_header)(ndo, encap_header_arg);

	orig_length = length;

	/*
	 * Get the source and destination addresses, skip past them,
	 * and print them if we're printing the link-layer header.
	 */
	ehp = (const struct ether_header *)p;
	src.addr = ehp->ether_shost;
	src.addr_string = etheraddr_string;
	dst.addr = ehp->ether_dhost;
	dst.addr_string = etheraddr_string;

	length -= 2*MAC_ADDR_LEN;
	caplen -= 2*MAC_ADDR_LEN;
	p += 2*MAC_ADDR_LEN;
	hdrlen = 2*MAC_ADDR_LEN;

	if (ndo->ndo_eflag)
		ether_addresses_print(ndo, src.addr, dst.addr);

	/*
	 * Print the switch tag, if we have one, and skip past it.
	 */
	if (print_switch_tag != NULL)
		(*print_switch_tag)(ndo, p);

	length -= switch_tag_len;
	caplen -= switch_tag_len;
	p += switch_tag_len;
	hdrlen += switch_tag_len;

	/*
	 * Get the length/type field, skip past it, and print it
	 * if we're printing the link-layer header.
	 */
recurse:
	length_type = GET_BE_U_2(p);

	length -= 2;
	caplen -= 2;
	p += 2;
	hdrlen += 2;

	/*
	 * Process 802.1AE MACsec headers.
	 */
	printed_length = 0;
	if (length_type == ETHERTYPE_MACSEC) {
		/*
		 * MACsec, aka IEEE 802.1AE-2006
		 * Print the header, and try to print the payload if it's not encrypted
		 */
		if (ndo->ndo_eflag) {
			ether_type_print(ndo, length_type);
			ND_PRINT(", length %u: ", orig_length);
			printed_length = 1;
		}

		int ret = macsec_print(ndo, &p, &length, &caplen, &hdrlen,
				       &src, &dst);

		if (ret == 0) {
			/* Payload is encrypted; print it as raw data. */
			if (!ndo->ndo_suppress_default_print)
				ND_DEFAULTPRINT(p, caplen);
			return hdrlen;
		} else if (ret > 0) {
			/* Problem printing the header; just quit. */
			return ret;
		} else {
			/*
			 * Keep processing type/length fields.
			 */
			length_type = GET_BE_U_2(p);

			ND_LCHECK_U(caplen, 2);
			length -= 2;
			caplen -= 2;
			p += 2;
			hdrlen += 2;
		}
	}

	/*
	 * Process VLAN tag types.
	 */
	while (length_type == ETHERTYPE_8021Q  ||
		length_type == ETHERTYPE_8021Q9100 ||
		length_type == ETHERTYPE_8021Q9200 ||
		length_type == ETHERTYPE_8021QinQ) {
		/*
		 * It has a VLAN tag.
		 * Print VLAN information, and then go back and process
		 * the enclosed type field.
		 */
		if (caplen < 4) {
			ndo->ndo_protocol = "vlan";
			nd_print_trunc(ndo);
			return hdrlen + caplen;
		}
		if (length < 4) {
			ndo->ndo_protocol = "vlan";
			nd_print_trunc(ndo);
			return hdrlen + length;
		}
		if (ndo->ndo_eflag) {
			uint16_t tag = GET_BE_U_2(p);

			ether_type_print(ndo, length_type);
			if (!printed_length) {
				ND_PRINT(", length %u: ", orig_length);
				printed_length = 1;
			} else
				ND_PRINT(", ");
			ND_PRINT("%s, ", ieee8021q_tci_string(tag));
		}

		length_type = GET_BE_U_2(p + 2);
		p += 4;
		length -= 4;
		caplen -= 4;
		hdrlen += 4;
	}

	/*
	 * We now have the final length/type field.
	 */
	if (length_type <= MAX_ETHERNET_LENGTH_VAL) {
		/*
		 * It's a length field, containing the length of the
		 * remaining payload; use it as such, as long as
		 * it's not too large (bigger than the actual payload).
		 */
		if (length_type < length) {
			length = length_type;
			if (caplen > length)
				caplen = length;
		}

		/*
		 * Cut off the snapshot length to the end of the
		 * payload.
		 */
		if (!nd_push_snaplen(ndo, p, length)) {
			(*ndo->ndo_error)(ndo, S_ERR_ND_MEM_ALLOC,
				"%s: can't push snaplen on buffer stack", __func__);
		}

		if (ndo->ndo_eflag) {
			ND_PRINT("802.3");
			if (!printed_length)
				ND_PRINT(", length %u: ", length);
		}

		/*
		 * An LLC header follows the length.  Print that and
		 * higher layers.
		 */
		llc_hdrlen = llc_print(ndo, p, length, caplen, &src, &dst);
		if (llc_hdrlen < 0) {
			/* packet type not known, print raw packet */
			if (!ndo->ndo_suppress_default_print)
				ND_DEFAULTPRINT(p, caplen);
			llc_hdrlen = -llc_hdrlen;
		}
		hdrlen += llc_hdrlen;
		nd_pop_packet_info(ndo);
	} else if (length_type == ETHERTYPE_JUMBO) {
		/*
		 * It's a type field, with the type for Alteon jumbo frames.
		 * See
		 *
		 *	https://tools.ietf.org/html/draft-ietf-isis-ext-eth-01
		 *
		 * which indicates that, following the type field,
		 * there's an LLC header and payload.
		 */
		/* Try to print the LLC-layer header & higher layers */
		llc_hdrlen = llc_print(ndo, p, length, caplen, &src, &dst);
		if (llc_hdrlen < 0) {
			/* packet type not known, print raw packet */
			if (!ndo->ndo_suppress_default_print)
				ND_DEFAULTPRINT(p, caplen);
			llc_hdrlen = -llc_hdrlen;
		}
		hdrlen += llc_hdrlen;
	} else if (length_type == ETHERTYPE_ARISTA) {
		if (caplen < 2) {
			ND_PRINT("[|arista]");
			return hdrlen + caplen;
		}
		if (length < 2) {
			ND_PRINT("[|arista]");
			return hdrlen + length;
		}
		ether_type_print(ndo, length_type);
		ND_PRINT(", length %u: ", orig_length);
		int bytesConsumed = arista_ethertype_print(ndo, p, length);
		if (bytesConsumed > 0) {
			p += bytesConsumed;
			length -= bytesConsumed;
			caplen -= bytesConsumed;
			hdrlen += bytesConsumed;
			goto recurse;
		} else {
			/* subtype/version not known, print raw packet */
			if (!ndo->ndo_eflag && length_type > MAX_ETHERNET_LENGTH_VAL) {
				ether_addresses_print(ndo, src.addr, dst.addr);
				ether_type_print(ndo, length_type);
				ND_PRINT(", length %u: ", orig_length);
			}
			 if (!ndo->ndo_suppress_default_print)
				 ND_DEFAULTPRINT(p, caplen);
		}
	} else {
		/*
		 * It's a type field with some other value.
		 */
		if (ndo->ndo_eflag) {
			ether_type_print(ndo, length_type);
			if (!printed_length)
				ND_PRINT(", length %u: ", orig_length);
			else
				ND_PRINT(", ");
		}
		if (ethertype_print(ndo, length_type, p, length, caplen, &src, &dst) == 0) {
			/* type not known, print raw packet */
			if (!ndo->ndo_eflag) {
				/*
				 * We didn't print the full link-layer
				 * header, as -e wasn't specified, so
				 * print only the source and destination
				 * MAC addresses and the final Ethernet
				 * type.
				 */
				ether_addresses_print(ndo, src.addr, dst.addr);
				ether_type_print(ndo, length_type);
				ND_PRINT(", length %u: ", orig_length);
			}

			if (!ndo->ndo_suppress_default_print)
				ND_DEFAULTPRINT(p, caplen);
		}
	}
invalid:
	return hdrlen;
}

/*
 * Print an Ethernet frame while specyfing a non-standard Ethernet header
 * length.
 * This might be encapsulated within another frame; we might be passed
 * a pointer to a function that can print header information for that
 * frame's protocol, and an argument to pass to that function.
 *
 * FIXME: caplen can and should be derived from ndo->ndo_snapend and p.
 */
u_int
ether_switch_tag_print(netdissect_options *ndo, const u_char *p, u_int length,
    u_int caplen,
    void (*print_switch_tag)(netdissect_options *, const u_char *),
    u_int switch_tag_len)
{
	return ether_common_print(ndo, p, length, caplen, print_switch_tag,
				  switch_tag_len, NULL, NULL);
}

/*
 * Print an Ethernet frame.
 * This might be encapsulated within another frame; we might be passed
 * a pointer to a function that can print header information for that
 * frame's protocol, and an argument to pass to that function.
 *
 * FIXME: caplen can and should be derived from ndo->ndo_snapend and p.
 */
u_int
ether_print(netdissect_options *ndo,
	    const u_char *p, u_int length, u_int caplen,
	    void (*print_encap_header)(netdissect_options *ndo, const u_char *),
	    const u_char *encap_header_arg)
{
	ndo->ndo_protocol = "ether";
	return ether_common_print(ndo, p, length, caplen, NULL, 0,
				  print_encap_header, encap_header_arg);
}

/*
 * This is the top level routine of the printer.  'p' points
 * to the ether header of the packet, 'h->len' is the length
 * of the packet off the wire, and 'h->caplen' is the number
 * of bytes actually captured.
 */
void
ether_if_print(netdissect_options *ndo, const struct pcap_pkthdr *h,
	       const u_char *p)
{
	ndo->ndo_protocol = "ether";
	ndo->ndo_ll_hdr_len +=
		ether_print(ndo, p, h->len, h->caplen, NULL, NULL);
}

/*
 * This is the top level routine of the printer.  'p' points
 * to the ether header of the packet, 'h->len' is the length
 * of the packet off the wire, and 'h->caplen' is the number
 * of bytes actually captured.
 *
 * This is for DLT_NETANALYZER, which has a 4-byte pseudo-header
 * before the Ethernet header.
 */
void
netanalyzer_if_print(netdissect_options *ndo, const struct pcap_pkthdr *h,
		     const u_char *p)
{
	/*
	 * Fail if we don't have enough data for the Hilscher pseudo-header.
	 */
	ndo->ndo_protocol = "netanalyzer";
	ND_TCHECK_LEN(p, 4);

	/* Skip the pseudo-header. */
	ndo->ndo_ll_hdr_len += 4;
	ndo->ndo_ll_hdr_len +=
		ether_print(ndo, p + 4, h->len - 4, h->caplen - 4, NULL, NULL);
}

/*
 * This is the top level routine of the printer.  'p' points
 * to the ether header of the packet, 'h->len' is the length
 * of the packet off the wire, and 'h->caplen' is the number
 * of bytes actually captured.
 *
 * This is for DLT_NETANALYZER_TRANSPARENT, which has a 4-byte
 * pseudo-header, a 7-byte Ethernet preamble, and a 1-byte Ethernet SOF
 * before the Ethernet header.
 */
void
netanalyzer_transparent_if_print(netdissect_options *ndo,
				 const struct pcap_pkthdr *h,
				 const u_char *p)
{
	/*
	 * Fail if we don't have enough data for the Hilscher pseudo-header,
	 * preamble, and SOF.
	 */
	ndo->ndo_protocol = "netanalyzer_transparent";
	ND_TCHECK_LEN(p, 12);

	/* Skip the pseudo-header, preamble, and SOF. */
	ndo->ndo_ll_hdr_len += 12;
	ndo->ndo_ll_hdr_len +=
		ether_print(ndo, p + 12, h->len - 12, h->caplen - 12, NULL, NULL);
}

/*
 * Prints the packet payload, given an Ethernet type code for the payload's
 * protocol.
 *
 * Returns non-zero if it can do so, zero if the ethertype is unknown.
 */

int
ethertype_print(netdissect_options *ndo,
		u_short ether_type, const u_char *p,
		u_int length, u_int caplen,
		const struct lladdr_info *src, const struct lladdr_info *dst)
{
	switch (ether_type) {

	case ETHERTYPE_IP:
		ip_print(ndo, p, length);
		return (1);

	case ETHERTYPE_IPV6:
		ip6_print(ndo, p, length);
		return (1);

	case ETHERTYPE_ARP:
	case ETHERTYPE_REVARP:
		arp_print(ndo, p, length, caplen);
		return (1);

	case ETHERTYPE_DN:
		decnet_print(ndo, p, length, caplen);
		return (1);

	case ETHERTYPE_ATALK:
		if (ndo->ndo_vflag)
			ND_PRINT("et1 ");
		atalk_print(ndo, p, length);
		return (1);

	case ETHERTYPE_AARP:
		aarp_print(ndo, p, length);
		return (1);

	case ETHERTYPE_IPX:
		ND_PRINT("(NOV-ETHII) ");
		ipx_print(ndo, p, length);
		return (1);

	case ETHERTYPE_ISO:
		if (length == 0 || caplen == 0) {
			ndo->ndo_protocol = "isoclns";
			nd_print_trunc(ndo);
			return (1);
		}
		/* At least one byte is required */
		/* FIXME: Reference for this byte? */
		ND_TCHECK_LEN(p, 1);
		isoclns_print(ndo, p + 1, length - 1);
		return(1);

	case ETHERTYPE_PPPOED:
	case ETHERTYPE_PPPOES:
	case ETHERTYPE_PPPOED2:
	case ETHERTYPE_PPPOES2:
		pppoe_print(ndo, p, length);
		return (1);

	case ETHERTYPE_EAPOL:
		eapol_print(ndo, p);
		return (1);

	case ETHERTYPE_REALTEK:
		rtl_print(ndo, p, length, src, dst);
		return (1);

	case ETHERTYPE_PPP:
		if (length) {
			ND_PRINT(": ");
			ppp_print(ndo, p, length);
		}
		return (1);

	case ETHERTYPE_MPCP:
		mpcp_print(ndo, p, length);
		return (1);

	case ETHERTYPE_SLOW:
		slow_print(ndo, p, length);
		return (1);

	case ETHERTYPE_CFM:
	case ETHERTYPE_CFM_OLD:
		cfm_print(ndo, p, length);
		return (1);

	case ETHERTYPE_LLDP:
		lldp_print(ndo, p, length);
		return (1);

	case ETHERTYPE_NSH:
		nsh_print(ndo, p, length);
		return (1);

	case ETHERTYPE_LOOPBACK:
		loopback_print(ndo, p, length);
		return (1);

	case ETHERTYPE_MPLS:
	case ETHERTYPE_MPLS_MULTI:
		mpls_print(ndo, p, length);
		return (1);

	case ETHERTYPE_TIPC:
		tipc_print(ndo, p, length, caplen);
		return (1);

	case ETHERTYPE_MS_NLB_HB:
		msnlb_print(ndo, p);
		return (1);

	case ETHERTYPE_GEONET_OLD:
	case ETHERTYPE_GEONET:
		geonet_print(ndo, p, length, src);
		return (1);

	case ETHERTYPE_CALM_FAST:
		calm_fast_print(ndo, p, length, src);
		return (1);

	case ETHERTYPE_AOE:
		aoe_print(ndo, p, length);
		return (1);

	case ETHERTYPE_PTP:
		ptp_print(ndo, p, length);
		return (1);

	case ETHERTYPE_LAT:
	case ETHERTYPE_SCA:
	case ETHERTYPE_MOPRC:
	case ETHERTYPE_MOPDL:
	case ETHERTYPE_IEEE1905_1:
		/* default_print for now */
	default:
		return (0);
	}
}
