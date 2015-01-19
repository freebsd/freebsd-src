/*
 * Copyright (c) 1990, 1991, 1993, 1994, 1995, 1996, 1997
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
 *
 * $FreeBSD$
 */

#define NETDISSECT_REWORKED
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"
#include "bootp.h"

static const char tstr[] = " [|bootp]";

static void rfc1048_print(netdissect_options *, const u_char *);
static void cmu_print(netdissect_options *, const u_char *);
static char *client_fqdn_flags(u_int flags);

static const struct tok bootp_flag_values[] = {
    { 0x8000,                   "Broadcast" },
    { 0, NULL}
};

static const struct tok bootp_op_values[] = {
    { BOOTPREQUEST,             "Request" },
    { BOOTPREPLY,               "Reply" },
    { 0, NULL}
};

/*
 * Print bootp requests
 */
void
bootp_print(netdissect_options *ndo,
            register const u_char *cp, u_int length)
{
	register const struct bootp *bp;
	static const u_char vm_cmu[4] = VM_CMU;
	static const u_char vm_rfc1048[4] = VM_RFC1048;

	bp = (const struct bootp *)cp;
	ND_TCHECK(bp->bp_op);

	ND_PRINT((ndo, "BOOTP/DHCP, %s",
	          tok2str(bootp_op_values, "unknown (0x%02x)", bp->bp_op)));

	if (bp->bp_htype == 1 && bp->bp_hlen == 6 && bp->bp_op == BOOTPREQUEST) {
		ND_TCHECK2(bp->bp_chaddr[0], 6);
		ND_PRINT((ndo, " from %s", etheraddr_string(ndo, bp->bp_chaddr)));
	}

	ND_PRINT((ndo, ", length %u", length));

	if (!ndo->ndo_vflag)
		return;

	ND_TCHECK(bp->bp_secs);

	/* The usual hardware address type is 1 (10Mb Ethernet) */
	if (bp->bp_htype != 1)
		ND_PRINT((ndo, ", htype %d", bp->bp_htype));

	/* The usual length for 10Mb Ethernet address is 6 bytes */
	if (bp->bp_htype != 1 || bp->bp_hlen != 6)
		ND_PRINT((ndo, ", hlen %d", bp->bp_hlen));

	/* Only print interesting fields */
	if (bp->bp_hops)
		ND_PRINT((ndo, ", hops %d", bp->bp_hops));
	if (EXTRACT_32BITS(&bp->bp_xid))
		ND_PRINT((ndo, ", xid 0x%x", EXTRACT_32BITS(&bp->bp_xid)));
	if (EXTRACT_16BITS(&bp->bp_secs))
		ND_PRINT((ndo, ", secs %d", EXTRACT_16BITS(&bp->bp_secs)));

	ND_PRINT((ndo, ", Flags [%s]",
		bittok2str(bootp_flag_values, "none", EXTRACT_16BITS(&bp->bp_flags))));
	if (ndo->ndo_vflag > 1)
		ND_PRINT((ndo, " (0x%04x)", EXTRACT_16BITS(&bp->bp_flags)));

	/* Client's ip address */
	ND_TCHECK(bp->bp_ciaddr);
	if (EXTRACT_32BITS(&bp->bp_ciaddr.s_addr))
		ND_PRINT((ndo, "\n\t  Client-IP %s", ipaddr_string(ndo, &bp->bp_ciaddr)));

	/* 'your' ip address (bootp client) */
	ND_TCHECK(bp->bp_yiaddr);
	if (EXTRACT_32BITS(&bp->bp_yiaddr.s_addr))
		ND_PRINT((ndo, "\n\t  Your-IP %s", ipaddr_string(ndo, &bp->bp_yiaddr)));

	/* Server's ip address */
	ND_TCHECK(bp->bp_siaddr);
	if (EXTRACT_32BITS(&bp->bp_siaddr.s_addr))
		ND_PRINT((ndo, "\n\t  Server-IP %s", ipaddr_string(ndo, &bp->bp_siaddr)));

	/* Gateway's ip address */
	ND_TCHECK(bp->bp_giaddr);
	if (EXTRACT_32BITS(&bp->bp_giaddr.s_addr))
		ND_PRINT((ndo, "\n\t  Gateway-IP %s", ipaddr_string(ndo, &bp->bp_giaddr)));

	/* Client's Ethernet address */
	if (bp->bp_htype == 1 && bp->bp_hlen == 6) {
		ND_TCHECK2(bp->bp_chaddr[0], 6);
		ND_PRINT((ndo, "\n\t  Client-Ethernet-Address %s", etheraddr_string(ndo, bp->bp_chaddr)));
	}

	ND_TCHECK2(bp->bp_sname[0], 1);		/* check first char only */
	if (*bp->bp_sname) {
		ND_PRINT((ndo, "\n\t  sname \""));
		if (fn_print(ndo, bp->bp_sname, ndo->ndo_snapend)) {
			ND_PRINT((ndo, "\""));
			ND_PRINT((ndo, "%s", tstr + 1));
			return;
		}
		ND_PRINT((ndo, "\""));
	}
	ND_TCHECK2(bp->bp_file[0], 1);		/* check first char only */
	if (*bp->bp_file) {
		ND_PRINT((ndo, "\n\t  file \""));
		if (fn_print(ndo, bp->bp_file, ndo->ndo_snapend)) {
			ND_PRINT((ndo, "\""));
			ND_PRINT((ndo, "%s", tstr + 1));
			return;
		}
		ND_PRINT((ndo, "\""));
	}

	/* Decode the vendor buffer */
	ND_TCHECK(bp->bp_vend[0]);
	if (memcmp((const char *)bp->bp_vend, vm_rfc1048,
		 sizeof(uint32_t)) == 0)
		rfc1048_print(ndo, bp->bp_vend);
	else if (memcmp((const char *)bp->bp_vend, vm_cmu,
		      sizeof(uint32_t)) == 0)
		cmu_print(ndo, bp->bp_vend);
	else {
		uint32_t ul;

		ul = EXTRACT_32BITS(&bp->bp_vend);
		if (ul != 0)
			ND_PRINT((ndo, "\n\t  Vendor-#0x%x", ul));
	}

	return;
trunc:
	ND_PRINT((ndo, "%s", tstr));
}

/*
 * The first character specifies the format to print:
 *     i - ip address (32 bits)
 *     p - ip address pairs (32 bits + 32 bits)
 *     l - long (32 bits)
 *     L - unsigned long (32 bits)
 *     s - short (16 bits)
 *     b - period-seperated decimal bytes (variable length)
 *     x - colon-seperated hex bytes (variable length)
 *     a - ascii string (variable length)
 *     B - on/off (8 bits)
 *     $ - special (explicit code to handle)
 */
static const struct tok tag2str[] = {
/* RFC1048 tags */
	{ TAG_PAD,		" PAD" },
	{ TAG_SUBNET_MASK,	"iSubnet-Mask" },	/* subnet mask (RFC950) */
	{ TAG_TIME_OFFSET,	"LTime-Zone" },	/* seconds from UTC */
	{ TAG_GATEWAY,		"iDefault-Gateway" },	/* default gateway */
	{ TAG_TIME_SERVER,	"iTime-Server" },	/* time servers (RFC868) */
	{ TAG_NAME_SERVER,	"iIEN-Name-Server" },	/* IEN name servers (IEN116) */
	{ TAG_DOMAIN_SERVER,	"iDomain-Name-Server" },	/* domain name (RFC1035) */
	{ TAG_LOG_SERVER,	"iLOG" },	/* MIT log servers */
	{ TAG_COOKIE_SERVER,	"iCS" },	/* cookie servers (RFC865) */
	{ TAG_LPR_SERVER,	"iLPR-Server" },	/* lpr server (RFC1179) */
	{ TAG_IMPRESS_SERVER,	"iIM" },	/* impress servers (Imagen) */
	{ TAG_RLP_SERVER,	"iRL" },	/* resource location (RFC887) */
	{ TAG_HOSTNAME,		"aHostname" },	/* ascii hostname */
	{ TAG_BOOTSIZE,		"sBS" },	/* 512 byte blocks */
	{ TAG_END,		" END" },
/* RFC1497 tags */
	{ TAG_DUMPPATH,		"aDP" },
	{ TAG_DOMAINNAME,	"aDomain-Name" },
	{ TAG_SWAP_SERVER,	"iSS" },
	{ TAG_ROOTPATH,		"aRP" },
	{ TAG_EXTPATH,		"aEP" },
/* RFC2132 tags */
	{ TAG_IP_FORWARD,	"BIPF" },
	{ TAG_NL_SRCRT,		"BSRT" },
	{ TAG_PFILTERS,		"pPF" },
	{ TAG_REASS_SIZE,	"sRSZ" },
	{ TAG_DEF_TTL,		"bTTL" },
	{ TAG_MTU_TIMEOUT,	"lMTU-Timeout" },
	{ TAG_MTU_TABLE,	"sMTU-Table" },
	{ TAG_INT_MTU,		"sMTU" },
	{ TAG_LOCAL_SUBNETS,	"BLSN" },
	{ TAG_BROAD_ADDR,	"iBR" },
	{ TAG_DO_MASK_DISC,	"BMD" },
	{ TAG_SUPPLY_MASK,	"BMS" },
	{ TAG_DO_RDISC,		"BRouter-Discovery" },
	{ TAG_RTR_SOL_ADDR,	"iRSA" },
	{ TAG_STATIC_ROUTE,	"pStatic-Route" },
	{ TAG_USE_TRAILERS,	"BUT" },
	{ TAG_ARP_TIMEOUT,	"lAT" },
	{ TAG_ETH_ENCAP,	"BIE" },
	{ TAG_TCP_TTL,		"bTT" },
	{ TAG_TCP_KEEPALIVE,	"lKI" },
	{ TAG_KEEPALIVE_GO,	"BKG" },
	{ TAG_NIS_DOMAIN,	"aYD" },
	{ TAG_NIS_SERVERS,	"iYS" },
	{ TAG_NTP_SERVERS,	"iNTP" },
	{ TAG_VENDOR_OPTS,	"bVendor-Option" },
	{ TAG_NETBIOS_NS,	"iNetbios-Name-Server" },
	{ TAG_NETBIOS_DDS,	"iWDD" },
	{ TAG_NETBIOS_NODE,	"$Netbios-Node" },
	{ TAG_NETBIOS_SCOPE,	"aNetbios-Scope" },
	{ TAG_XWIN_FS,		"iXFS" },
	{ TAG_XWIN_DM,		"iXDM" },
	{ TAG_NIS_P_DOMAIN,	"sN+D" },
	{ TAG_NIS_P_SERVERS,	"iN+S" },
	{ TAG_MOBILE_HOME,	"iMH" },
	{ TAG_SMPT_SERVER,	"iSMTP" },
	{ TAG_POP3_SERVER,	"iPOP3" },
	{ TAG_NNTP_SERVER,	"iNNTP" },
	{ TAG_WWW_SERVER,	"iWWW" },
	{ TAG_FINGER_SERVER,	"iFG" },
	{ TAG_IRC_SERVER,	"iIRC" },
	{ TAG_STREETTALK_SRVR,	"iSTS" },
	{ TAG_STREETTALK_STDA,	"iSTDA" },
	{ TAG_REQUESTED_IP,	"iRequested-IP" },
	{ TAG_IP_LEASE,		"lLease-Time" },
	{ TAG_OPT_OVERLOAD,	"$OO" },
	{ TAG_TFTP_SERVER,	"aTFTP" },
	{ TAG_BOOTFILENAME,	"aBF" },
	{ TAG_DHCP_MESSAGE,	" DHCP-Message" },
	{ TAG_SERVER_ID,	"iServer-ID" },
	{ TAG_PARM_REQUEST,	"bParameter-Request" },
	{ TAG_MESSAGE,		"aMSG" },
	{ TAG_MAX_MSG_SIZE,	"sMSZ" },
	{ TAG_RENEWAL_TIME,	"lRN" },
	{ TAG_REBIND_TIME,	"lRB" },
	{ TAG_VENDOR_CLASS,	"aVendor-Class" },
	{ TAG_CLIENT_ID,	"$Client-ID" },
/* RFC 2485 */
	{ TAG_OPEN_GROUP_UAP,	"aUAP" },
/* RFC 2563 */
	{ TAG_DISABLE_AUTOCONF,	"BNOAUTO" },
/* RFC 2610 */
	{ TAG_SLP_DA,		"bSLP-DA" },	/*"b" is a little wrong */
	{ TAG_SLP_SCOPE,	"bSLP-SCOPE" },	/*"b" is a little wrong */
/* RFC 2937 */
	{ TAG_NS_SEARCH,	"sNSSEARCH" },	/* XXX 's' */
/* RFC 3011 */
	{ TAG_IP4_SUBNET_SELECT, "iSUBNET" },
/* RFC 3442 */
	{ TAG_CLASSLESS_STATIC_RT, "$Classless-Static-Route" },
	{ TAG_CLASSLESS_STA_RT_MS, "$Classless-Static-Route-Microsoft" },
/* http://www.iana.org/assignments/bootp-dhcp-extensions/index.htm */
	{ TAG_USER_CLASS,	"aCLASS" },
	{ TAG_SLP_NAMING_AUTH,	"aSLP-NA" },
	{ TAG_CLIENT_FQDN,	"$FQDN" },
	{ TAG_AGENT_CIRCUIT,	"$Agent-Information" },
	{ TAG_AGENT_REMOTE,	"bARMT" },
	{ TAG_AGENT_MASK,	"bAMSK" },
	{ TAG_TZ_STRING,	"aTZSTR" },
	{ TAG_FQDN_OPTION,	"bFQDNS" },	/* XXX 'b' */
	{ TAG_AUTH,		"bAUTH" },	/* XXX 'b' */
	{ TAG_VINES_SERVERS,	"iVINES" },
	{ TAG_SERVER_RANK,	"sRANK" },
	{ TAG_CLIENT_ARCH,	"sARCH" },
	{ TAG_CLIENT_NDI,	"bNDI" },	/* XXX 'b' */
	{ TAG_CLIENT_GUID,	"bGUID" },	/* XXX 'b' */
	{ TAG_LDAP_URL,		"aLDAP" },
	{ TAG_6OVER4,		"i6o4" },
	{ TAG_PRINTER_NAME,	"aPRTR" },
	{ TAG_MDHCP_SERVER,	"bMDHCP" },	/* XXX 'b' */
	{ TAG_IPX_COMPAT,	"bIPX" },	/* XXX 'b' */
	{ TAG_NETINFO_PARENT,	"iNI" },
	{ TAG_NETINFO_PARENT_TAG, "aNITAG" },
	{ TAG_URL,		"aURL" },
	{ TAG_FAILOVER,		"bFAIL" },	/* XXX 'b' */
	{ 0,			NULL }
};
/* 2-byte extended tags */
static const struct tok xtag2str[] = {
	{ 0,			NULL }
};

/* DHCP "options overload" types */
static const struct tok oo2str[] = {
	{ 1,			"file" },
	{ 2,			"sname" },
	{ 3,			"file+sname" },
	{ 0,			NULL }
};

/* NETBIOS over TCP/IP node type options */
static const struct tok nbo2str[] = {
	{ 0x1,			"b-node" },
	{ 0x2,			"p-node" },
	{ 0x4,			"m-node" },
	{ 0x8,			"h-node" },
	{ 0,			NULL }
};

/* ARP Hardware types, for Client-ID option */
static const struct tok arp2str[] = {
	{ 0x1,			"ether" },
	{ 0x6,			"ieee802" },
	{ 0x7,			"arcnet" },
	{ 0xf,			"frelay" },
	{ 0x17,			"strip" },
	{ 0x18,			"ieee1394" },
	{ 0,			NULL }
};

static const struct tok dhcp_msg_values[] = {
        { DHCPDISCOVER, "Discover" },
        { DHCPOFFER, "Offer" },
        { DHCPREQUEST, "Request" },
        { DHCPDECLINE, "Decline" },
        { DHCPACK, "ACK" },
        { DHCPNAK, "NACK" },
        { DHCPRELEASE, "Release" },
        { DHCPINFORM, "Inform" },
        { 0,			NULL }
};

#define AGENT_SUBOPTION_CIRCUIT_ID 	1	/* RFC 3046 */
#define AGENT_SUBOPTION_REMOTE_ID  	2	/* RFC 3046 */
#define AGENT_SUBOPTION_SUBSCRIBER_ID 	6	/* RFC 3993 */
static const struct tok agent_suboption_values[] = {
        { AGENT_SUBOPTION_CIRCUIT_ID, "Circuit-ID" },
        { AGENT_SUBOPTION_REMOTE_ID, "Remote-ID" },
        { AGENT_SUBOPTION_SUBSCRIBER_ID, "Subscriber-ID" },
        { 0,			NULL }
};


static void
rfc1048_print(netdissect_options *ndo,
              register const u_char *bp)
{
	register uint16_t tag;
	register u_int len;
	register const char *cp;
	register char c;
	int first, idx;
	uint32_t ul;
	uint16_t us;
	uint8_t uc, subopt, suboptlen;

	ND_PRINT((ndo, "\n\t  Vendor-rfc1048 Extensions"));

	/* Step over magic cookie */
	ND_PRINT((ndo, "\n\t    Magic Cookie 0x%08x", EXTRACT_32BITS(bp)));
	bp += sizeof(int32_t);

	/* Loop while we there is a tag left in the buffer */
	while (ND_TTEST2(*bp, 1)) {
		tag = *bp++;
		if (tag == TAG_PAD && ndo->ndo_vflag < 3)
			continue;
		if (tag == TAG_END && ndo->ndo_vflag < 3)
			return;
		if (tag == TAG_EXTENDED_OPTION) {
			ND_TCHECK2(*(bp + 1), 2);
			tag = EXTRACT_16BITS(bp + 1);
			/* XXX we don't know yet if the IANA will
			 * preclude overlap of 1-byte and 2-byte spaces.
			 * If not, we need to offset tag after this step.
			 */
			cp = tok2str(xtag2str, "?xT%u", tag);
		} else
			cp = tok2str(tag2str, "?T%u", tag);
		c = *cp++;

		if (tag == TAG_PAD || tag == TAG_END)
			len = 0;
		else {
			/* Get the length; check for truncation */
			ND_TCHECK2(*bp, 1);
			len = *bp++;
		}

		ND_PRINT((ndo, "\n\t    %s Option %u, length %u%s", cp, tag, len,
		    len > 0 ? ": " : ""));

		if (tag == TAG_PAD && ndo->ndo_vflag > 2) {
			u_int ntag = 1;
			while (ND_TTEST2(*bp, 1) && *bp == TAG_PAD) {
				bp++;
				ntag++;
			}
			if (ntag > 1)
				ND_PRINT((ndo, ", occurs %u", ntag));
		}

		if (!ND_TTEST2(*bp, len)) {
			ND_PRINT((ndo, "[|rfc1048 %u]", len));
			return;
		}

		if (tag == TAG_DHCP_MESSAGE && len == 1) {
			uc = *bp++;
			ND_PRINT((ndo, "%s", tok2str(dhcp_msg_values, "Unknown (%u)", uc)));
			continue;
		}

		if (tag == TAG_PARM_REQUEST) {
			idx = 0;
			while (len-- > 0) {
				uc = *bp++;
				cp = tok2str(tag2str, "?Option %u", uc);
				if (idx % 4 == 0)
					ND_PRINT((ndo, "\n\t      "));
				else
					ND_PRINT((ndo, ", "));
				ND_PRINT((ndo, "%s", cp + 1));
				idx++;
			}
			continue;
		}

		if (tag == TAG_EXTENDED_REQUEST) {
			first = 1;
			while (len > 1) {
				len -= 2;
				us = EXTRACT_16BITS(bp);
				bp += 2;
				cp = tok2str(xtag2str, "?xT%u", us);
				if (!first)
					ND_PRINT((ndo, "+"));
				ND_PRINT((ndo, "%s", cp + 1));
				first = 0;
			}
			continue;
		}

		/* Print data */
		if (c == '?') {
			/* Base default formats for unknown tags on data size */
			if (len & 1)
				c = 'b';
			else if (len & 2)
				c = 's';
			else
				c = 'l';
		}
		first = 1;
		switch (c) {

		case 'a':
			/* ascii strings */
			ND_PRINT((ndo, "\""));
			if (fn_printn(ndo, bp, len, ndo->ndo_snapend)) {
				ND_PRINT((ndo, "\""));
				goto trunc;
			}
			ND_PRINT((ndo, "\""));
			bp += len;
			len = 0;
			break;

		case 'i':
		case 'l':
		case 'L':
			/* ip addresses/32-bit words */
			while (len >= sizeof(ul)) {
				if (!first)
					ND_PRINT((ndo, ","));
				ul = EXTRACT_32BITS(bp);
				if (c == 'i') {
					ul = htonl(ul);
					ND_PRINT((ndo, "%s", ipaddr_string(ndo, &ul)));
				} else if (c == 'L')
					ND_PRINT((ndo, "%d", ul));
				else
					ND_PRINT((ndo, "%u", ul));
				bp += sizeof(ul);
				len -= sizeof(ul);
				first = 0;
			}
			break;

		case 'p':
			/* IP address pairs */
			while (len >= 2*sizeof(ul)) {
				if (!first)
					ND_PRINT((ndo, ","));
				memcpy((char *)&ul, (const char *)bp, sizeof(ul));
				ND_PRINT((ndo, "(%s:", ipaddr_string(ndo, &ul)));
				bp += sizeof(ul);
				memcpy((char *)&ul, (const char *)bp, sizeof(ul));
				ND_PRINT((ndo, "%s)", ipaddr_string(ndo, &ul)));
				bp += sizeof(ul);
				len -= 2*sizeof(ul);
				first = 0;
			}
			break;

		case 's':
			/* shorts */
			while (len >= sizeof(us)) {
				if (!first)
					ND_PRINT((ndo, ","));
				us = EXTRACT_16BITS(bp);
				ND_PRINT((ndo, "%u", us));
				bp += sizeof(us);
				len -= sizeof(us);
				first = 0;
			}
			break;

		case 'B':
			/* boolean */
			while (len > 0) {
				if (!first)
					ND_PRINT((ndo, ","));
				switch (*bp) {
				case 0:
					ND_PRINT((ndo, "N"));
					break;
				case 1:
					ND_PRINT((ndo, "Y"));
					break;
				default:
					ND_PRINT((ndo, "%u?", *bp));
					break;
				}
				++bp;
				--len;
				first = 0;
			}
			break;

		case 'b':
		case 'x':
		default:
			/* Bytes */
			while (len > 0) {
				if (!first)
					ND_PRINT((ndo, c == 'x' ? ":" : "."));
				if (c == 'x')
					ND_PRINT((ndo, "%02x", *bp));
				else
					ND_PRINT((ndo, "%u", *bp));
				++bp;
				--len;
				first = 0;
			}
			break;

		case '$':
			/* Guys we can't handle with one of the usual cases */
			switch (tag) {

			case TAG_NETBIOS_NODE:
				/* this option should be at least 1 byte long */
				if (len < 1)  {
					ND_PRINT((ndo, "ERROR: option %u len %u < 1 bytes",
					    TAG_NETBIOS_NODE, len));
					break;
				}
				tag = *bp++;
				--len;
				ND_PRINT((ndo, "%s", tok2str(nbo2str, NULL, tag)));
				break;

			case TAG_OPT_OVERLOAD:
				/* this option should be at least 1 byte long */
				if (len < 1)  {
					ND_PRINT((ndo, "ERROR: option %u len %u < 1 bytes",
					    TAG_OPT_OVERLOAD, len));
					break;
				}
				tag = *bp++;
				--len;
				ND_PRINT((ndo, "%s", tok2str(oo2str, NULL, tag)));
				break;

			case TAG_CLIENT_FQDN:
				/* this option should be at least 3 bytes long */
				if (len < 3)  {
					ND_PRINT((ndo, "ERROR: option %u len %u < 3 bytes",
					    TAG_CLIENT_FQDN, len));
					bp += len;
					len = 0;
					break;
				}
				if (*bp)
					ND_PRINT((ndo, "[%s] ", client_fqdn_flags(*bp)));
				bp++;
				if (*bp || *(bp+1))
					ND_PRINT((ndo, "%u/%u ", *bp, *(bp+1)));
				bp += 2;
				ND_PRINT((ndo, "\""));
				if (fn_printn(ndo, bp, len - 3, ndo->ndo_snapend)) {
					ND_PRINT((ndo, "\""));
					goto trunc;
				}
				ND_PRINT((ndo, "\""));
				bp += len - 3;
				len = 0;
				break;

			case TAG_CLIENT_ID:
			    {	int type;

				/* this option should be at least 1 byte long */
				if (len < 1)  {
					ND_PRINT((ndo, "ERROR: option %u len %u < 1 bytes",
					    TAG_CLIENT_ID, len));
					break;
				}
				type = *bp++;
				len--;
				if (type == 0) {
					ND_PRINT((ndo, "\""));
					if (fn_printn(ndo, bp, len, ndo->ndo_snapend)) {
						ND_PRINT((ndo, "\""));
						goto trunc;
					}
					ND_PRINT((ndo, "\""));
					bp += len;
					len = 0;
					break;
				} else {
					ND_PRINT((ndo, "%s ", tok2str(arp2str, "hardware-type %u,", type)));
					while (len > 0) {
						if (!first)
							ND_PRINT((ndo, ":"));
						ND_PRINT((ndo, "%02x", *bp));
						++bp;
						--len;
						first = 0;
					}
				}
				break;
			    }

			case TAG_AGENT_CIRCUIT:
				while (len >= 2) {
					subopt = *bp++;
					suboptlen = *bp++;
					len -= 2;
					if (suboptlen > len) {
						ND_PRINT((ndo, "\n\t      %s SubOption %u, length %u: length goes past end of option",
						   tok2str(agent_suboption_values, "Unknown", subopt),
						   subopt,
						   suboptlen));
						bp += len;
						len = 0;
						break;
					}
					ND_PRINT((ndo, "\n\t      %s SubOption %u, length %u: ",
					   tok2str(agent_suboption_values, "Unknown", subopt),
					   subopt,
					   suboptlen));
					switch (subopt) {

					case AGENT_SUBOPTION_CIRCUIT_ID: /* fall through */
					case AGENT_SUBOPTION_REMOTE_ID:
					case AGENT_SUBOPTION_SUBSCRIBER_ID:
						fn_printn(ndo, bp, suboptlen, NULL);
						break;

					default:
						print_unknown_data(ndo, bp, "\n\t\t", suboptlen);
					}

					len -= suboptlen;
					bp += suboptlen;
			    }
			    break;

			case TAG_CLASSLESS_STATIC_RT:
			case TAG_CLASSLESS_STA_RT_MS:
			{
				u_int mask_width, significant_octets, i;

				/* this option should be at least 5 bytes long */
				if (len < 5)  {
					ND_PRINT((ndo, "ERROR: option %u len %u < 5 bytes",
					    TAG_CLASSLESS_STATIC_RT, len));
					bp += len;
					len = 0;
					break;
				}
				while (len > 0) {
					if (!first)
						ND_PRINT((ndo, ","));
					mask_width = *bp++;
					len--;
					/* mask_width <= 32 */
					if (mask_width > 32) {
						ND_PRINT((ndo, "[ERROR: Mask width (%d) > 32]",  mask_width));
						bp += len;
						len = 0;
						break;
					}
					significant_octets = (mask_width + 7) / 8;
					/* significant octets + router(4) */
					if (len < significant_octets + 4) {
						ND_PRINT((ndo, "[ERROR: Remaining length (%u) < %u bytes]",  len, significant_octets + 4));
						bp += len;
						len = 0;
						break;
					}
					ND_PRINT((ndo, "("));
					if (mask_width == 0)
						ND_PRINT((ndo, "default"));
					else {
						for (i = 0; i < significant_octets ; i++) {
							if (i > 0)
								ND_PRINT((ndo, "."));
							ND_PRINT((ndo, "%d", *bp++));
						}
						for (i = significant_octets ; i < 4 ; i++)
							ND_PRINT((ndo, ".0"));
						ND_PRINT((ndo, "/%d", mask_width));
					}
					memcpy((char *)&ul, (const char *)bp, sizeof(ul));
					ND_PRINT((ndo, ":%s)", ipaddr_string(ndo, &ul)));
					bp += sizeof(ul);
					len -= (significant_octets + 4);
					first = 0;
				}
			}
			break;

			default:
				ND_PRINT((ndo, "[unknown special tag %u, size %u]",
				    tag, len));
				bp += len;
				len = 0;
				break;
			}
			break;
		}
		/* Data left over? */
		if (len) {
			ND_PRINT((ndo, "\n\t  trailing data length %u", len));
			bp += len;
		}
	}
	return;
trunc:
	ND_PRINT((ndo, "|[rfc1048]"));
}

static void
cmu_print(netdissect_options *ndo,
          register const u_char *bp)
{
	register const struct cmu_vend *cmu;

#define PRINTCMUADDR(m, s) { ND_TCHECK(cmu->m); \
    if (cmu->m.s_addr != 0) \
	ND_PRINT((ndo, " %s:%s", s, ipaddr_string(ndo, &cmu->m.s_addr))); }

	ND_PRINT((ndo, " vend-cmu"));
	cmu = (const struct cmu_vend *)bp;

	/* Only print if there are unknown bits */
	ND_TCHECK(cmu->v_flags);
	if ((cmu->v_flags & ~(VF_SMASK)) != 0)
		ND_PRINT((ndo, " F:0x%x", cmu->v_flags));
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
	ND_PRINT((ndo, "%s", tstr));
#undef PRINTCMUADDR
}

static char *
client_fqdn_flags(u_int flags)
{
	static char buf[8+1];
	int i = 0;

	if (flags & CLIENT_FQDN_FLAGS_S)
		buf[i++] = 'S';
	if (flags & CLIENT_FQDN_FLAGS_O)
		buf[i++] = 'O';
	if (flags & CLIENT_FQDN_FLAGS_E)
		buf[i++] = 'E';
	if (flags & CLIENT_FQDN_FLAGS_N)
		buf[i++] = 'N';
	buf[i] = '\0';

	return buf;
}
