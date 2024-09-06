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
 */

/* \summary: BOOTP and IPv4 DHCP printer */

#include <config.h>

#include "netdissect-stdinc.h"

#include <string.h>

#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"


/*
 * Bootstrap Protocol (BOOTP).  RFC951 and RFC1048.
 *
 * This file specifies the "implementation-independent" BOOTP protocol
 * information which is common to both client and server.
 *
 * Copyright 1988 by Carnegie Mellon.
 *
 * Permission to use, copy, modify, and distribute this program for any
 * purpose and without fee is hereby granted, provided that this copyright
 * and permission notice appear on all copies and supporting documentation,
 * the name of Carnegie Mellon not be used in advertising or publicity
 * pertaining to distribution of the program without specific prior
 * permission, and notice be given in supporting documentation that copying
 * and distribution is by permission of Carnegie Mellon and Stanford
 * University.  Carnegie Mellon makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */

struct bootp {
	nd_uint8_t	bp_op;		/* packet opcode type */
	nd_uint8_t	bp_htype;	/* hardware addr type */
	nd_uint8_t	bp_hlen;	/* hardware addr length */
	nd_uint8_t	bp_hops;	/* gateway hops */
	nd_uint32_t	bp_xid;		/* transaction ID */
	nd_uint16_t	bp_secs;	/* seconds since boot began */
	nd_uint16_t	bp_flags;	/* flags - see bootp_flag_values[]
					   in print-bootp.c */
	nd_ipv4		bp_ciaddr;	/* client IP address */
	nd_ipv4		bp_yiaddr;	/* 'your' IP address */
	nd_ipv4		bp_siaddr;	/* server IP address */
	nd_ipv4		bp_giaddr;	/* gateway IP address */
	nd_byte		bp_chaddr[16];	/* client hardware address */
	nd_byte		bp_sname[64];	/* server host name */
	nd_byte		bp_file[128];	/* boot file name */
	nd_byte		bp_vend[64];	/* vendor-specific area */
};

#define BOOTPREPLY	2
#define BOOTPREQUEST	1

/*
 * Vendor magic cookie (v_magic) for CMU
 */
#define VM_CMU		"CMU"

/*
 * Vendor magic cookie (v_magic) for RFC1048
 */
#define VM_RFC1048	{ 99, 130, 83, 99 }

/*
 * RFC1048 tag values used to specify what information is being supplied in
 * the vendor field of the packet.
 */

#define TAG_PAD			((uint8_t)   0)
#define TAG_SUBNET_MASK		((uint8_t)   1)
#define TAG_TIME_OFFSET		((uint8_t)   2)
#define TAG_GATEWAY		((uint8_t)   3)
#define TAG_TIME_SERVER		((uint8_t)   4)
#define TAG_NAME_SERVER		((uint8_t)   5)
#define TAG_DOMAIN_SERVER	((uint8_t)   6)
#define TAG_LOG_SERVER		((uint8_t)   7)
#define TAG_COOKIE_SERVER	((uint8_t)   8)
#define TAG_LPR_SERVER		((uint8_t)   9)
#define TAG_IMPRESS_SERVER	((uint8_t)  10)
#define TAG_RLP_SERVER		((uint8_t)  11)
#define TAG_HOSTNAME		((uint8_t)  12)
#define TAG_BOOTSIZE		((uint8_t)  13)
#define TAG_END			((uint8_t) 255)
/* RFC1497 tags */
#define	TAG_DUMPPATH		((uint8_t)  14)
#define	TAG_DOMAINNAME		((uint8_t)  15)
#define	TAG_SWAP_SERVER		((uint8_t)  16)
#define	TAG_ROOTPATH		((uint8_t)  17)
#define	TAG_EXTPATH		((uint8_t)  18)
/* RFC2132 */
#define	TAG_IP_FORWARD		((uint8_t)  19)
#define	TAG_NL_SRCRT		((uint8_t)  20)
#define	TAG_PFILTERS		((uint8_t)  21)
#define	TAG_REASS_SIZE		((uint8_t)  22)
#define	TAG_DEF_TTL		((uint8_t)  23)
#define	TAG_MTU_TIMEOUT		((uint8_t)  24)
#define	TAG_MTU_TABLE		((uint8_t)  25)
#define	TAG_INT_MTU		((uint8_t)  26)
#define	TAG_LOCAL_SUBNETS	((uint8_t)  27)
#define	TAG_BROAD_ADDR		((uint8_t)  28)
#define	TAG_DO_MASK_DISC	((uint8_t)  29)
#define	TAG_SUPPLY_MASK		((uint8_t)  30)
#define	TAG_DO_RDISC		((uint8_t)  31)
#define	TAG_RTR_SOL_ADDR	((uint8_t)  32)
#define	TAG_STATIC_ROUTE	((uint8_t)  33)
#define	TAG_USE_TRAILERS	((uint8_t)  34)
#define	TAG_ARP_TIMEOUT		((uint8_t)  35)
#define	TAG_ETH_ENCAP		((uint8_t)  36)
#define	TAG_TCP_TTL		((uint8_t)  37)
#define	TAG_TCP_KEEPALIVE	((uint8_t)  38)
#define	TAG_KEEPALIVE_GO	((uint8_t)  39)
#define	TAG_NIS_DOMAIN		((uint8_t)  40)
#define	TAG_NIS_SERVERS		((uint8_t)  41)
#define	TAG_NTP_SERVERS		((uint8_t)  42)
#define	TAG_VENDOR_OPTS		((uint8_t)  43)
#define	TAG_NETBIOS_NS		((uint8_t)  44)
#define	TAG_NETBIOS_DDS		((uint8_t)  45)
#define	TAG_NETBIOS_NODE	((uint8_t)  46)
#define	TAG_NETBIOS_SCOPE	((uint8_t)  47)
#define	TAG_XWIN_FS		((uint8_t)  48)
#define	TAG_XWIN_DM		((uint8_t)  49)
#define	TAG_NIS_P_DOMAIN	((uint8_t)  64)
#define	TAG_NIS_P_SERVERS	((uint8_t)  65)
#define	TAG_MOBILE_HOME		((uint8_t)  68)
#define	TAG_SMTP_SERVER		((uint8_t)  69)
#define	TAG_POP3_SERVER		((uint8_t)  70)
#define	TAG_NNTP_SERVER		((uint8_t)  71)
#define	TAG_WWW_SERVER		((uint8_t)  72)
#define	TAG_FINGER_SERVER	((uint8_t)  73)
#define	TAG_IRC_SERVER		((uint8_t)  74)
#define	TAG_STREETTALK_SRVR	((uint8_t)  75)
#define	TAG_STREETTALK_STDA	((uint8_t)  76)
/* DHCP options */
#define	TAG_REQUESTED_IP	((uint8_t)  50)
#define	TAG_IP_LEASE		((uint8_t)  51)
#define	TAG_OPT_OVERLOAD	((uint8_t)  52)
#define	TAG_TFTP_SERVER		((uint8_t)  66)
#define	TAG_BOOTFILENAME	((uint8_t)  67)
#define	TAG_DHCP_MESSAGE	((uint8_t)  53)
#define	TAG_SERVER_ID		((uint8_t)  54)
#define	TAG_PARM_REQUEST	((uint8_t)  55)
#define	TAG_MESSAGE		((uint8_t)  56)
#define	TAG_MAX_MSG_SIZE	((uint8_t)  57)
#define	TAG_RENEWAL_TIME	((uint8_t)  58)
#define	TAG_REBIND_TIME		((uint8_t)  59)
#define	TAG_VENDOR_CLASS	((uint8_t)  60)
#define	TAG_CLIENT_ID		((uint8_t)  61)
/* RFC 2241 */
#define	TAG_NDS_SERVERS		((uint8_t)  85)
#define	TAG_NDS_TREE_NAME	((uint8_t)  86)
#define	TAG_NDS_CONTEXT		((uint8_t)  87)
/* RFC 2242 */
#define	TAG_NDS_IPDOMAIN	((uint8_t)  62)
#define	TAG_NDS_IPINFO		((uint8_t)  63)
/* RFC 2485 */
#define	TAG_OPEN_GROUP_UAP	((uint8_t)  98)
/* RFC 2563 */
#define	TAG_DISABLE_AUTOCONF	((uint8_t) 116)
/* RFC 2610 */
#define	TAG_SLP_DA		((uint8_t)  78)
#define	TAG_SLP_SCOPE		((uint8_t)  79)
/* RFC 2937 */
#define	TAG_NS_SEARCH		((uint8_t) 117)
/* RFC 3004 - The User Class Option for DHCP */
#define	TAG_USER_CLASS		((uint8_t)  77)
/* RFC 3011 */
#define	TAG_IP4_SUBNET_SELECT	((uint8_t) 118)
/* RFC 3442 */
#define TAG_CLASSLESS_STATIC_RT	((uint8_t) 121)
#define TAG_CLASSLESS_STA_RT_MS	((uint8_t) 249)
/* RFC8572 */
#define TAG_SZTP_REDIRECT	((uint8_t) 143)
/* RFC 5859 - TFTP Server Address Option for DHCPv4 */
#define	TAG_TFTP_SERVER_ADDRESS	((uint8_t) 150)
/* https://www.iana.org/assignments/bootp-dhcp-parameters/bootp-dhcp-parameters.xhtml */
#define	TAG_SLP_NAMING_AUTH	((uint8_t)  80)
#define	TAG_CLIENT_FQDN		((uint8_t)  81)
#define	TAG_AGENT_CIRCUIT	((uint8_t)  82)
#define	TAG_AGENT_REMOTE	((uint8_t)  83)
#define	TAG_TZ_STRING		((uint8_t)  88)
#define	TAG_FQDN_OPTION		((uint8_t)  89)
#define	TAG_AUTH		((uint8_t)  90)
#define	TAG_CLIENT_LAST_TRANSACTION_TIME	((uint8_t)  91)
#define	TAG_ASSOCIATED_IP			((uint8_t)  92)
#define	TAG_CLIENT_ARCH		((uint8_t)  93)
#define	TAG_CLIENT_NDI		((uint8_t)  94)
#define	TAG_CLIENT_GUID		((uint8_t)  97)
#define	TAG_LDAP_URL		((uint8_t)  95)
/* RFC 4833, TZ codes */
#define	TAG_TZ_PCODE		((uint8_t) 100)
#define	TAG_TZ_TCODE		((uint8_t) 101)
#define	TAG_NETINFO_PARENT	((uint8_t) 112)
#define	TAG_NETINFO_PARENT_TAG	((uint8_t) 113)
#define	TAG_URL			((uint8_t) 114)
#define TAG_MUDURL              ((uint8_t) 161)

/* DHCP Message types (values for TAG_DHCP_MESSAGE option) */
#define DHCPDISCOVER	1
#define DHCPOFFER	2
#define DHCPREQUEST	3
#define DHCPDECLINE	4
#define DHCPACK		5
#define DHCPNAK		6
#define DHCPRELEASE	7
#define DHCPINFORM	8
/* Defined in RFC4388 */
#define DHCPLEASEQUERY       10
#define DHCPLEASEUNASSIGNED  11
#define DHCPLEASEUNKNOWN     12
#define DHCPLEASEACTIVE      13


/*
 * "vendor" data permitted for CMU bootp clients.
 */

struct cmu_vend {
	nd_byte		v_magic[4];	/* magic number */
	nd_uint32_t	v_flags;	/* flags/opcodes, etc. */
	nd_ipv4		v_smask;	/* Subnet mask */
	nd_ipv4		v_dgate;	/* Default gateway */
	nd_ipv4		v_dns1, v_dns2; /* Domain name servers */
	nd_ipv4		v_ins1, v_ins2; /* IEN-116 name servers */
	nd_ipv4		v_ts1, v_ts2;	/* Time servers */
	nd_byte		v_unused[24];	/* currently unused */
};


/* v_flags values */
#define VF_SMASK	1	/* Subnet mask field contains valid data */

/* RFC 4702 DHCP Client FQDN Option */

#define CLIENT_FQDN_FLAGS_S	0x01
#define CLIENT_FQDN_FLAGS_O	0x02
#define CLIENT_FQDN_FLAGS_E	0x04
#define CLIENT_FQDN_FLAGS_N	0x08
/* end of original bootp.h */

static void rfc1048_print(netdissect_options *, const u_char *);
static void cmu_print(netdissect_options *, const u_char *);
static char *client_fqdn_flags(u_int flags);

static const struct tok bootp_flag_values[] = {
	{ 0x8000,	"Broadcast" },
	{ 0, NULL}
};

static const struct tok bootp_op_values[] = {
	{ BOOTPREQUEST,	"Request" },
	{ BOOTPREPLY,	"Reply" },
	{ 0, NULL}
};

/*
 * Print bootp requests
 */
void
bootp_print(netdissect_options *ndo,
	    const u_char *cp, u_int length)
{
	const struct bootp *bp;
	static const u_char vm_cmu[4] = VM_CMU;
	static const u_char vm_rfc1048[4] = VM_RFC1048;
	uint8_t bp_op, bp_htype, bp_hlen;

	ndo->ndo_protocol = "bootp";
	bp = (const struct bootp *)cp;
	bp_op = GET_U_1(bp->bp_op);
	ND_PRINT("BOOTP/DHCP, %s",
		  tok2str(bootp_op_values, "unknown (0x%02x)", bp_op));

	bp_htype = GET_U_1(bp->bp_htype);
	bp_hlen = GET_U_1(bp->bp_hlen);
	if (bp_htype == 1 && bp_hlen == MAC_ADDR_LEN && bp_op == BOOTPREQUEST) {
		ND_PRINT(" from %s", GET_ETHERADDR_STRING(bp->bp_chaddr));
	}

	ND_PRINT(", length %u", length);

	if (!ndo->ndo_vflag)
		return;

	ND_TCHECK_2(bp->bp_secs);

	/* The usual hardware address type is 1 (10Mb Ethernet) */
	if (bp_htype != 1)
		ND_PRINT(", htype %u", bp_htype);

	/* The usual length for 10Mb Ethernet address is 6 bytes */
	if (bp_htype != 1 || bp_hlen != MAC_ADDR_LEN)
		ND_PRINT(", hlen %u", bp_hlen);

	/* Only print interesting fields */
	if (GET_U_1(bp->bp_hops))
		ND_PRINT(", hops %u", GET_U_1(bp->bp_hops));
	if (GET_BE_U_4(bp->bp_xid))
		ND_PRINT(", xid 0x%x", GET_BE_U_4(bp->bp_xid));
	if (GET_BE_U_2(bp->bp_secs))
		ND_PRINT(", secs %u", GET_BE_U_2(bp->bp_secs));

	ND_PRINT(", Flags [%s]",
		  bittok2str(bootp_flag_values, "none", GET_BE_U_2(bp->bp_flags)));
	if (ndo->ndo_vflag > 1)
		ND_PRINT(" (0x%04x)", GET_BE_U_2(bp->bp_flags));

	/* Client's ip address */
	if (GET_IPV4_TO_NETWORK_ORDER(bp->bp_ciaddr))
		ND_PRINT("\n\t  Client-IP %s", GET_IPADDR_STRING(bp->bp_ciaddr));

	/* 'your' ip address (bootp client) */
	if (GET_IPV4_TO_NETWORK_ORDER(bp->bp_yiaddr))
		ND_PRINT("\n\t  Your-IP %s", GET_IPADDR_STRING(bp->bp_yiaddr));

	/* Server's ip address */
	if (GET_IPV4_TO_NETWORK_ORDER(bp->bp_siaddr))
		ND_PRINT("\n\t  Server-IP %s", GET_IPADDR_STRING(bp->bp_siaddr));

	/* Gateway's ip address */
	if (GET_IPV4_TO_NETWORK_ORDER(bp->bp_giaddr))
		ND_PRINT("\n\t  Gateway-IP %s", GET_IPADDR_STRING(bp->bp_giaddr));

	/* Client's Ethernet address */
	if (bp_htype == 1 && bp_hlen == MAC_ADDR_LEN) {
		ND_PRINT("\n\t  Client-Ethernet-Address %s", GET_ETHERADDR_STRING(bp->bp_chaddr));
	}

	if (GET_U_1(bp->bp_sname)) {	/* get first char only */
		ND_PRINT("\n\t  sname \"");
		if (nd_printztn(ndo, bp->bp_sname, (u_int)sizeof(bp->bp_sname),
				ndo->ndo_snapend) == 0) {
			ND_PRINT("\"");
			nd_print_trunc(ndo);
			return;
		}
		ND_PRINT("\"");
	}
	if (GET_U_1(bp->bp_file)) {	/* get first char only */
		ND_PRINT("\n\t  file \"");
		if (nd_printztn(ndo, bp->bp_file, (u_int)sizeof(bp->bp_file),
				ndo->ndo_snapend) == 0) {
			ND_PRINT("\"");
			nd_print_trunc(ndo);
			return;
		}
		ND_PRINT("\"");
	}

	/* Decode the vendor buffer */
	ND_TCHECK_4(bp->bp_vend);
	if (memcmp((const char *)bp->bp_vend, vm_rfc1048,
		    sizeof(uint32_t)) == 0)
		rfc1048_print(ndo, bp->bp_vend);
	else if (memcmp((const char *)bp->bp_vend, vm_cmu,
			sizeof(uint32_t)) == 0)
		cmu_print(ndo, bp->bp_vend);
	else {
		uint32_t ul;

		ul = GET_BE_U_4(bp->bp_vend);
		if (ul != 0)
			ND_PRINT("\n\t  Vendor-#0x%x", ul);
	}

	return;
trunc:
	nd_print_trunc(ndo);
}

/*
 * The first character specifies the format to print:
 *     i - ip address (32 bits)
 *     p - ip address pairs (32 bits + 32 bits)
 *     l - long (32 bits)
 *     L - unsigned long (32 bits)
 *     s - short (16 bits)
 *     b - period-separated decimal bytes (variable length)
 *     x - colon-separated hex bytes (variable length)
 *     a - ASCII string (variable length)
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
	{ TAG_HOSTNAME,		"aHostname" },	/* ASCII hostname */
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
	{ TAG_SMTP_SERVER,	"iSMTP" },
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
/* RFC 3004 - The User Class Option for DHCP */
	{ TAG_USER_CLASS,	"$User-Class" },
/* RFC 3011 */
	{ TAG_IP4_SUBNET_SELECT, "iSUBNET" },
/* RFC 3442 */
	{ TAG_CLASSLESS_STATIC_RT, "$Classless-Static-Route" },
	{ TAG_CLASSLESS_STA_RT_MS, "$Classless-Static-Route-Microsoft" },
/* RFC 8572 */
	{ TAG_SZTP_REDIRECT,	"$SZTP-Redirect" },
/* RFC 5859 - TFTP Server Address Option for DHCPv4 */
	{ TAG_TFTP_SERVER_ADDRESS, "iTFTP-Server-Address" },
/* https://www.iana.org/assignments/bootp-dhcp-parameters/bootp-dhcp-parameters.xhtml#options */
	{ TAG_SLP_NAMING_AUTH,	"aSLP-NA" },
	{ TAG_CLIENT_FQDN,	"$FQDN" },
	{ TAG_AGENT_CIRCUIT,	"$Agent-Information" },
	{ TAG_AGENT_REMOTE,	"bARMT" },
	{ TAG_TZ_STRING,	"aTZSTR" },
	{ TAG_FQDN_OPTION,	"bFQDNS" },	/* XXX 'b' */
	{ TAG_AUTH,		"bAUTH" },	/* XXX 'b' */
	{ TAG_CLIENT_LAST_TRANSACTION_TIME, "LLast-Transaction-Time" },
	{ TAG_ASSOCIATED_IP,	"iAssociated-IP" },
	{ TAG_CLIENT_ARCH,	"sARCH" },
	{ TAG_CLIENT_NDI,	"bNDI" },	/* XXX 'b' */
	{ TAG_CLIENT_GUID,	"bGUID" },	/* XXX 'b' */
	{ TAG_LDAP_URL,		"aLDAP" },
	{ TAG_TZ_PCODE,		"aPOSIX-TZ" },
	{ TAG_TZ_TCODE,		"aTZ-Name" },
	{ TAG_NETINFO_PARENT,	"iNI" },
	{ TAG_NETINFO_PARENT_TAG, "aNITAG" },
	{ TAG_URL,		"aURL" },
	{ TAG_MUDURL,           "aMUD-URL" },
	{ 0, NULL }
};

/* DHCP "options overload" types */
static const struct tok oo2str[] = {
	{ 1,	"file" },
	{ 2,	"sname" },
	{ 3,	"file+sname" },
	{ 0, NULL }
};

/* NETBIOS over TCP/IP node type options */
static const struct tok nbo2str[] = {
	{ 0x1,	"b-node" },
	{ 0x2,	"p-node" },
	{ 0x4,	"m-node" },
	{ 0x8,	"h-node" },
	{ 0, NULL }
};

/* ARP Hardware types, for Client-ID option */
static const struct tok arp2str[] = {
	{ 0x1,	"ether" },
	{ 0x6,	"ieee802" },
	{ 0x7,	"arcnet" },
	{ 0xf,	"frelay" },
	{ 0x17,	"strip" },
	{ 0x18,	"ieee1394" },
	{ 0, NULL }
};

static const struct tok dhcp_msg_values[] = {
	{ DHCPDISCOVER,	       "Discover" },
	{ DHCPOFFER,	       "Offer" },
	{ DHCPREQUEST,	       "Request" },
	{ DHCPDECLINE,	       "Decline" },
	{ DHCPACK,	       "ACK" },
	{ DHCPNAK,	       "NACK" },
	{ DHCPRELEASE,	       "Release" },
	{ DHCPINFORM,	       "Inform" },
	{ DHCPLEASEQUERY,      "LeaseQuery" },
	{ DHCPLEASEUNASSIGNED, "LeaseUnassigned" },
	{ DHCPLEASEUNKNOWN,    "LeaseUnknown" },
	{ DHCPLEASEACTIVE,     "LeaseActive" },
	{ 0, NULL }
};

#define AGENT_SUBOPTION_CIRCUIT_ID	1	/* RFC 3046 */
#define AGENT_SUBOPTION_REMOTE_ID	2	/* RFC 3046 */
#define AGENT_SUBOPTION_SUBSCRIBER_ID	6	/* RFC 3993 */
static const struct tok agent_suboption_values[] = {
	{ AGENT_SUBOPTION_CIRCUIT_ID,    "Circuit-ID" },
	{ AGENT_SUBOPTION_REMOTE_ID,     "Remote-ID" },
	{ AGENT_SUBOPTION_SUBSCRIBER_ID, "Subscriber-ID" },
	{ 0, NULL }
};


static void
rfc1048_print(netdissect_options *ndo,
	      const u_char *bp)
{
	uint16_t tag;
	u_int len;
	const char *cp;
	char c;
	int first, idx;
	uint8_t subopt, suboptlen;

	ND_PRINT("\n\t  Vendor-rfc1048 Extensions");

	/* Step over magic cookie */
	ND_PRINT("\n\t    Magic Cookie 0x%08x", GET_BE_U_4(bp));
	bp += sizeof(int32_t);

	/* Loop while we there is a tag left in the buffer */
	while (ND_TTEST_1(bp)) {
		tag = GET_U_1(bp);
		bp++;
		if (tag == TAG_PAD && ndo->ndo_vflag < 3)
			continue;
		if (tag == TAG_END && ndo->ndo_vflag < 3)
			return;
		cp = tok2str(tag2str, "?Unknown", tag);
		c = *cp++;

		if (tag == TAG_PAD || tag == TAG_END)
			len = 0;
		else {
			/* Get the length; check for truncation */
			len = GET_U_1(bp);
			bp++;
		}

		ND_PRINT("\n\t    %s (%u), length %u%s", cp, tag, len,
			  len > 0 ? ": " : "");

		if (tag == TAG_PAD && ndo->ndo_vflag > 2) {
			u_int ntag = 1;
			while (ND_TTEST_1(bp) &&
			       GET_U_1(bp) == TAG_PAD) {
				bp++;
				ntag++;
			}
			if (ntag > 1)
				ND_PRINT(", occurs %u", ntag);
		}

		ND_TCHECK_LEN(bp, len);

		if (tag == TAG_DHCP_MESSAGE && len == 1) {
			ND_PRINT("%s",
				 tok2str(dhcp_msg_values, "Unknown (%u)", GET_U_1(bp)));
			bp++;
			continue;
		}

		if (tag == TAG_PARM_REQUEST) {
			idx = 0;
			while (len > 0) {
				uint8_t innertag = GET_U_1(bp);
				bp++;
				len--;
				cp = tok2str(tag2str, "?Unknown", innertag);
				if (idx % 4 == 0)
					ND_PRINT("\n\t      ");
				else
					ND_PRINT(", ");
				ND_PRINT("%s (%u)", cp + 1, innertag);
				idx++;
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
			/* ASCII strings */
			ND_PRINT("\"");
			if (nd_printn(ndo, bp, len, ndo->ndo_snapend)) {
				ND_PRINT("\"");
				goto trunc;
			}
			ND_PRINT("\"");
			bp += len;
			len = 0;
			break;

		case 'i':
		case 'l':
		case 'L':
			/* ip addresses/32-bit words */
			while (len >= 4) {
				if (!first)
					ND_PRINT(",");
				if (c == 'i')
					ND_PRINT("%s", GET_IPADDR_STRING(bp));
				else if (c == 'L')
					ND_PRINT("%d", GET_BE_S_4(bp));
				else
					ND_PRINT("%u", GET_BE_U_4(bp));
				bp += 4;
				len -= 4;
				first = 0;
			}
			break;

		case 'p':
			/* IP address pairs */
			while (len >= 2*4) {
				if (!first)
					ND_PRINT(",");
				ND_PRINT("(%s:", GET_IPADDR_STRING(bp));
				bp += 4;
				len -= 4;
				ND_PRINT("%s)", GET_IPADDR_STRING(bp));
				bp += 4;
				len -= 4;
				first = 0;
			}
			break;

		case 's':
			/* shorts */
			while (len >= 2) {
				if (!first)
					ND_PRINT(",");
				ND_PRINT("%u", GET_BE_U_2(bp));
				bp += 2;
				len -= 2;
				first = 0;
			}
			break;

		case 'B':
			/* boolean */
			while (len > 0) {
				uint8_t bool_value;
				if (!first)
					ND_PRINT(",");
				bool_value = GET_U_1(bp);
				switch (bool_value) {
				case 0:
					ND_PRINT("N");
					break;
				case 1:
					ND_PRINT("Y");
					break;
				default:
					ND_PRINT("%u?", bool_value);
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
				uint8_t byte_value;
				if (!first)
					ND_PRINT(c == 'x' ? ":" : ".");
				byte_value = GET_U_1(bp);
				if (c == 'x')
					ND_PRINT("%02x", byte_value);
				else
					ND_PRINT("%u", byte_value);
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
				if (len < 1) {
					ND_PRINT("[ERROR: length < 1 bytes]");
					break;
				}
				tag = GET_U_1(bp);
				++bp;
				--len;
				ND_PRINT("%s", tok2str(nbo2str, NULL, tag));
				break;

			case TAG_OPT_OVERLOAD:
				/* this option should be at least 1 byte long */
				if (len < 1) {
					ND_PRINT("[ERROR: length < 1 bytes]");
					break;
				}
				tag = GET_U_1(bp);
				++bp;
				--len;
				ND_PRINT("%s", tok2str(oo2str, NULL, tag));
				break;

			case TAG_CLIENT_FQDN:
				/* this option should be at least 3 bytes long */
				if (len < 3) {
					ND_PRINT("[ERROR: length < 3 bytes]");
					bp += len;
					len = 0;
					break;
				}
				if (GET_U_1(bp) & 0xf0) {
					ND_PRINT("[ERROR: MBZ nibble 0x%x != 0] ",
						 (GET_U_1(bp) & 0xf0) >> 4);
				}
				if (GET_U_1(bp) & 0x0f)
					ND_PRINT("[%s] ",
						 client_fqdn_flags(GET_U_1(bp)));
				bp++;
				if (GET_U_1(bp) || GET_U_1(bp + 1))
					ND_PRINT("%u/%u ", GET_U_1(bp),
						 GET_U_1(bp + 1));
				bp += 2;
				ND_PRINT("\"");
				if (nd_printn(ndo, bp, len - 3, ndo->ndo_snapend)) {
					ND_PRINT("\"");
					goto trunc;
				}
				ND_PRINT("\"");
				bp += len - 3;
				len = 0;
				break;

			case TAG_CLIENT_ID:
			    {
				int type;

				/* this option should be at least 1 byte long */
				if (len < 1) {
					ND_PRINT("[ERROR: length < 1 bytes]");
					break;
				}
				type = GET_U_1(bp);
				bp++;
				len--;
				if (type == 0) {
					ND_PRINT("\"");
					if (nd_printn(ndo, bp, len, ndo->ndo_snapend)) {
						ND_PRINT("\"");
						goto trunc;
					}
					ND_PRINT("\"");
					bp += len;
					len = 0;
					break;
				} else {
					ND_PRINT("%s ", tok2str(arp2str, "hardware-type %u,", type));
					while (len > 0) {
						if (!first)
							ND_PRINT(":");
						ND_PRINT("%02x", GET_U_1(bp));
						++bp;
						--len;
						first = 0;
					}
				}
				break;
			    }

			case TAG_AGENT_CIRCUIT:
				while (len >= 2) {
					subopt = GET_U_1(bp);
					suboptlen = GET_U_1(bp + 1);
					bp += 2;
					len -= 2;
					if (suboptlen > len) {
						ND_PRINT("\n\t      %s SubOption %u, length %u: length goes past end of option",
							  tok2str(agent_suboption_values, "Unknown", subopt),
							  subopt,
							  suboptlen);
						bp += len;
						len = 0;
						break;
					}
					ND_PRINT("\n\t      %s SubOption %u, length %u: ",
						  tok2str(agent_suboption_values, "Unknown", subopt),
						  subopt,
						  suboptlen);
					switch (subopt) {

					case AGENT_SUBOPTION_CIRCUIT_ID: /* fall through */
					case AGENT_SUBOPTION_REMOTE_ID:
					case AGENT_SUBOPTION_SUBSCRIBER_ID:
						if (nd_printn(ndo, bp, suboptlen, ndo->ndo_snapend))
							goto trunc;
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
				if (len < 5) {
					ND_PRINT("[ERROR: length < 5 bytes]");
					bp += len;
					len = 0;
					break;
				}
				while (len > 0) {
					if (!first)
						ND_PRINT(",");
					mask_width = GET_U_1(bp);
					bp++;
					len--;
					/* mask_width <= 32 */
					if (mask_width > 32) {
						ND_PRINT("[ERROR: Mask width (%u) > 32]", mask_width);
						bp += len;
						len = 0;
						break;
					}
					significant_octets = (mask_width + 7) / 8;
					/* significant octets + router(4) */
					if (len < significant_octets + 4) {
						ND_PRINT("[ERROR: Remaining length (%u) < %u bytes]", len, significant_octets + 4);
						bp += len;
						len = 0;
						break;
					}
					ND_PRINT("(");
					if (mask_width == 0)
						ND_PRINT("default");
					else {
						for (i = 0; i < significant_octets ; i++) {
							if (i > 0)
								ND_PRINT(".");
							ND_PRINT("%u",
								 GET_U_1(bp));
							bp++;
						}
						for (i = significant_octets ; i < 4 ; i++)
							ND_PRINT(".0");
						ND_PRINT("/%u", mask_width);
					}
					ND_PRINT(":%s)", GET_IPADDR_STRING(bp));
					bp += 4;
					len -= (significant_octets + 4);
					first = 0;
				}
				break;
			    }

			case TAG_USER_CLASS:
			    {
				u_int suboptnumber = 1;

				first = 1;
				if (len < 2) {
					ND_PRINT("[ERROR: length < 2 bytes]");
					bp += len;
					len = 0;
					break;
				}
				while (len > 0) {
					suboptlen = GET_U_1(bp);
					bp++;
					len--;
					ND_PRINT("\n\t      ");
					ND_PRINT("instance#%u: ", suboptnumber);
					if (suboptlen == 0) {
						ND_PRINT("[ERROR: suboption length must be non-zero]");
						bp += len;
						len = 0;
						break;
					}
					if (len < suboptlen) {
						ND_PRINT("[ERROR: invalid option]");
						bp += len;
						len = 0;
						break;
					}
					ND_PRINT("\"");
					if (nd_printn(ndo, bp, suboptlen, ndo->ndo_snapend)) {
						ND_PRINT("\"");
						goto trunc;
					}
					ND_PRINT("\"");
					ND_PRINT(", length %u", suboptlen);
					suboptnumber++;
					len -= suboptlen;
					bp += suboptlen;
				}
				break;
			    }


			case TAG_SZTP_REDIRECT:
				/* as per https://datatracker.ietf.org/doc/html/rfc8572#section-8.3
				 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-...-+-+-+-+-+-+-+
				 |        uri-length             |          URI                  |
				 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-...-+-+-+-+-+-+-+

				 * uri-length: 2 octets long; specifies the length of the URI data.
				 * URI: URI of the SZTP bootstrap server.
				 */
				while (len >= 2) {
					suboptlen = GET_BE_U_2(bp);
					bp += 2;
					len -= 2;
					ND_PRINT("\n\t	    ");
					ND_PRINT("length %u: ", suboptlen);
					if (len < suboptlen) {
						ND_PRINT("length goes past end of option");
						bp += len;
						len = 0;
						break;
					}
					ND_PRINT("\"");
					nd_printjn(ndo, bp, suboptlen);
					ND_PRINT("\"");
					len -= suboptlen;
					bp += suboptlen;
				}
				if (len != 0) {
					ND_PRINT("[ERROR: length < 2 bytes]");
				}
				break;

			default:
				ND_PRINT("[unknown special tag %u, size %u]",
					  tag, len);
				bp += len;
				len = 0;
				break;
			}
			break;
		}
		/* Data left over? */
		if (len) {
			ND_PRINT("\n\t  trailing data length %u", len);
			bp += len;
		}
	}
	return;
trunc:
	nd_print_trunc(ndo);
}

#define PRINTCMUADDR(m, s) { ND_TCHECK_4(cmu->m); \
    if (GET_IPV4_TO_NETWORK_ORDER(cmu->m) != 0) \
	ND_PRINT(" %s:%s", s, GET_IPADDR_STRING(cmu->m)); }

static void
cmu_print(netdissect_options *ndo,
	  const u_char *bp)
{
	const struct cmu_vend *cmu;
	uint8_t v_flags;

	ND_PRINT(" vend-cmu");
	cmu = (const struct cmu_vend *)bp;

	/* Only print if there are unknown bits */
	ND_TCHECK_4(cmu->v_flags);
	v_flags = GET_U_1(cmu->v_flags);
	if ((v_flags & ~(VF_SMASK)) != 0)
		ND_PRINT(" F:0x%x", v_flags);
	PRINTCMUADDR(v_dgate, "DG");
	PRINTCMUADDR(v_smask, v_flags & VF_SMASK ? "SM" : "SM*");
	PRINTCMUADDR(v_dns1, "NS1");
	PRINTCMUADDR(v_dns2, "NS2");
	PRINTCMUADDR(v_ins1, "IEN1");
	PRINTCMUADDR(v_ins2, "IEN2");
	PRINTCMUADDR(v_ts1, "TS1");
	PRINTCMUADDR(v_ts2, "TS2");
	return;

trunc:
	nd_print_trunc(ndo);
}

#undef PRINTCMUADDR

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
