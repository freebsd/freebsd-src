/*
 * Copyright (c) 1993 Daniel Boulet
 * Copyright (c) 1994 Ugen J.S.Antsilevich
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 *
 *	$FreeBSD$
 */

/*
 * Format of an IP firewall descriptor
 *
 * fw_src, fw_dst, fw_smsk, fw_dmsk are always stored in network byte order.
 * fw_flg and fw_n*p are stored in host byte order (of course).
 * Port numbers are stored in HOST byte order.
 */
#ifndef _IP_FW_H
#define _IP_FW_H

struct ip_fw {
    u_long fw_pcnt,fw_bcnt;		/* Packet and byte counters */
    struct in_addr fw_src, fw_dst;	/* Source and destination IP addr */
    struct in_addr fw_smsk, fw_dmsk;	/* Mask for src and dest IP addr */
    union {
	struct in_addr fu_via_ip;	/* Specified by IP address */
	struct {			/* Specified by interface name */
#define FW_IFNLEN	6		/* To keep structure on 2^x boundary */
		char  fu_via_name[FW_IFNLEN];
		short fu_via_unit;
	} fu_via_if;
    } fu_via_un;
#define fw_via_ip	fu_via_un.fu_via_ip
#define fw_via_name	fu_via_un.fu_via_if.fu_via_name
#define fw_via_unit	fu_via_un.fu_via_if.fu_via_unit
    u_short fw_number;
    u_short fw_flg;			/* Flags word */
    u_short fw_nsp, fw_ndp;             /* N'of src ports and # of dst ports */
    					/* in ports array (dst ports follow */
    					/* src ports; max of 10 ports in all; */
    					/* count of 0 means match all ports) */
#define IP_FW_MAX_PORTS	10      	/* A reasonable maximum */
    u_short fw_pts[IP_FW_MAX_PORTS];    /* Array of port numbers to match */
    u_char fw_ipopt,fw_ipnopt;		/* IP options set/unset */
    u_char fw_tcpf,fw_tcpnf;		/* TCP flags set/unset */
#define IP_FW_ICMPTYPES_DIM (256 / (sizeof(unsigned) * 8))
    unsigned fw_icmptypes[IP_FW_ICMPTYPES_DIM]; /* ICMP types bitmap */
    long timestamp;         		/* timestamp (tv_sec) of last match */
    u_short fw_divert_port;		/* Divert port (options IPDIVERT) */
    u_char fw_prot;			/* IP protocol */
};

struct ip_fw_chain {
        LIST_ENTRY(ip_fw_chain) chain;
        struct ip_fw    *rule;
};

/*
 * Values for "flags" field .
 */
#define IP_FW_F_IN	0x0004	/* Inbound 			      */
#define IP_FW_F_OUT	0x0008	/* Outbound			      */

#define IP_FW_F_COMMAND 0x0030	/* Mask for type of chain entry:      */
#define IP_FW_F_ACCEPT	0x0010	/* This is an accept rule	      */
#define IP_FW_F_COUNT	0x0020	/* This is a count rule		      */
#define IP_FW_F_DIVERT	0x0030	/* This is a divert rule	      */
#define IP_FW_F_DENY	0x0000	/* This is a deny rule	              */

#define IP_FW_F_PRN	0x0040	/* Print if this rule matches	      */
#define IP_FW_F_ICMPRPL	0x0080	/* Send back icmp unreachable packet  */

#define IP_FW_F_SRNG	0x0100	/* The first two src ports are a min  *
				 * and max range (stored in host byte *
				 * order).                            */

#define IP_FW_F_DRNG	0x0200	/* The first two dst ports are a min  *
				 * and max range (stored in host byte *
				 * order).                            */

#define IP_FW_F_IFNAME	0x0400	/* Use interface name/unit (not IP)   */

#define IP_FW_F_FRAG	0x0800	/* Fragment			      */

#define IP_FW_F_ICMPBIT 0x1000	/* ICMP type bitmap is valid          */

#define IP_FW_F_IFUWILD	0x2000	/* Match all interface units          */

#define IP_FW_F_MASK	0x3FFF	/* All possible flag bits mask        */

/*
 * Definitions for IP option names.
 */
#define IP_FW_IPOPT_LSRR	0x01
#define IP_FW_IPOPT_SSRR	0x02
#define IP_FW_IPOPT_RR		0x04
#define IP_FW_IPOPT_TS		0x08

/*
 * Definitions for TCP flags.
 */
#define IP_FW_TCPF_FIN		TH_FIN
#define IP_FW_TCPF_SYN		TH_SYN
#define IP_FW_TCPF_RST		TH_RST
#define IP_FW_TCPF_PSH		TH_PUSH
#define IP_FW_TCPF_ACK		TH_ACK
#define IP_FW_TCPF_URG		TH_URG
#define IP_FW_TCPF_ESTAB	0x40

/*
 * Main firewall chains definitions and global var's definitions.
 */
#ifdef KERNEL

/*
 * Function definitions.
 */
void ip_fw_init(void);

#endif /* KERNEL */

#endif /* _IP_FW_H */
