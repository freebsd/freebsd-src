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
	uint8_t		bp_op;		/* packet opcode type */
	uint8_t		bp_htype;	/* hardware addr type */
	uint8_t		bp_hlen;	/* hardware addr length */
	uint8_t		bp_hops;	/* gateway hops */
	uint32_t	bp_xid;		/* transaction ID */
	uint16_t	bp_secs;	/* seconds since boot began */
	uint16_t	bp_flags;	/* flags - see bootp_flag_values[]
					   in print-bootp.c */
	struct in_addr	bp_ciaddr;	/* client IP address */
	struct in_addr	bp_yiaddr;	/* 'your' IP address */
	struct in_addr	bp_siaddr;	/* server IP address */
	struct in_addr	bp_giaddr;	/* gateway IP address */
	uint8_t		bp_chaddr[16];	/* client hardware address */
	uint8_t		bp_sname[64];	/* server host name */
	uint8_t		bp_file[128];	/* boot file name */
	uint8_t		bp_vend[64];	/* vendor-specific area */
} UNALIGNED;

/*
 * UDP port numbers, server and client.
 */
#define	IPPORT_BOOTPS		67
#define	IPPORT_BOOTPC		68

#define BOOTPREPLY		2
#define BOOTPREQUEST		1

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
#define	TAG_SMPT_SERVER		((uint8_t)  69)
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
/* RFC 3011 */
#define	TAG_IP4_SUBNET_SELECT	((uint8_t) 118)
/* RFC 3442 */
#define TAG_CLASSLESS_STATIC_RT	((uint8_t) 121)
#define TAG_CLASSLESS_STA_RT_MS	((uint8_t) 249)
/* ftp://ftp.isi.edu/.../assignments/bootp-dhcp-extensions */
#define	TAG_USER_CLASS		((uint8_t)  77)
#define	TAG_SLP_NAMING_AUTH	((uint8_t)  80)
#define	TAG_CLIENT_FQDN		((uint8_t)  81)
#define	TAG_AGENT_CIRCUIT	((uint8_t)  82)
#define	TAG_AGENT_REMOTE	((uint8_t)  83)
#define	TAG_AGENT_MASK		((uint8_t)  84)
#define	TAG_TZ_STRING		((uint8_t)  88)
#define	TAG_FQDN_OPTION		((uint8_t)  89)
#define	TAG_AUTH		((uint8_t)  90)
#define	TAG_VINES_SERVERS	((uint8_t)  91)
#define	TAG_SERVER_RANK		((uint8_t)  92)
#define	TAG_CLIENT_ARCH		((uint8_t)  93)
#define	TAG_CLIENT_NDI		((uint8_t)  94)
#define	TAG_CLIENT_GUID		((uint8_t)  97)
#define	TAG_LDAP_URL		((uint8_t)  95)
#define	TAG_6OVER4		((uint8_t)  96)
#define	TAG_PRINTER_NAME	((uint8_t) 100)
#define	TAG_MDHCP_SERVER	((uint8_t) 101)
#define	TAG_IPX_COMPAT		((uint8_t) 110)
#define	TAG_NETINFO_PARENT	((uint8_t) 112)
#define	TAG_NETINFO_PARENT_TAG	((uint8_t) 113)
#define	TAG_URL			((uint8_t) 114)
#define	TAG_FAILOVER		((uint8_t) 115)
#define	TAG_EXTENDED_REQUEST	((uint8_t) 126)
#define	TAG_EXTENDED_OPTION	((uint8_t) 127)


/* DHCP Message types (values for TAG_DHCP_MESSAGE option) */
#define		DHCPDISCOVER	1
#define		DHCPOFFER	2
#define		DHCPREQUEST	3
#define		DHCPDECLINE	4
#define		DHCPACK		5
#define		DHCPNAK		6
#define		DHCPRELEASE	7
#define		DHCPINFORM	8


/*
 * "vendor" data permitted for CMU bootp clients.
 */

struct cmu_vend {
	uint8_t		v_magic[4];	/* magic number */
	uint32_t	v_flags;	/* flags/opcodes, etc. */
	struct in_addr	v_smask;	/* Subnet mask */
	struct in_addr	v_dgate;	/* Default gateway */
	struct in_addr	v_dns1, v_dns2; /* Domain name servers */
	struct in_addr	v_ins1, v_ins2; /* IEN-116 name servers */
	struct in_addr	v_ts1, v_ts2;	/* Time servers */
	uint8_t		v_unused[24];	/* currently unused */
} UNALIGNED;


/* v_flags values */
#define VF_SMASK	1	/* Subnet mask field contains valid data */

/* RFC 4702 DHCP Client FQDN Option */

#define CLIENT_FQDN_FLAGS_S	0x01
#define CLIENT_FQDN_FLAGS_O	0x02
#define CLIENT_FQDN_FLAGS_E	0x04
#define CLIENT_FQDN_FLAGS_N	0x08
