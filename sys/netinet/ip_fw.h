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
 *	$Id: ip_fw.h,v 1.29 1997/09/16 11:43:57 bde Exp $
 */

#ifndef _IP_FW_H
#define _IP_FW_H

/*
 * This union structure identifies an interface, either explicitly
 * by name or implicitly by IP address. The flags IP_FW_F_IIFNAME
 * and IP_FW_F_OIFNAME say how to interpret this structure. An
 * interface unit number of -1 matches any unit number, while an
 * IP address of 0.0.0.0 indicates matches any interface.
 *
 * The receive and transmit interfaces are only compared against the
 * the packet if the corresponding bit (IP_FW_F_IIFACE or IP_FW_F_OIFACE)
 * is set. Note some packets lack a receive or transmit interface
 * (in which case the missing "interface" never matches).
 */

union ip_fw_if {
    struct in_addr fu_via_ip;	/* Specified by IP address */
    struct {			/* Specified by interface name */
#define FW_IFNLEN     IFNAMSIZ
	    char  name[FW_IFNLEN];
	    short unit;		/* -1 means match any unit */
    } fu_via_if;
};

/*
 * Format of an IP firewall descriptor
 *
 * fw_src, fw_dst, fw_smsk, fw_dmsk are always stored in network byte order.
 * fw_flg and fw_n*p are stored in host byte order (of course).
 * Port numbers are stored in HOST byte order.
 * Warning: setsockopt() will fail if sizeof(struct ip_fw) > MLEN (108)
 */

struct ip_fw {
    u_long fw_pcnt,fw_bcnt;		/* Packet and byte counters */
    struct in_addr fw_src, fw_dst;	/* Source and destination IP addr */
    struct in_addr fw_smsk, fw_dmsk;	/* Mask for src and dest IP addr */
    u_short fw_number;			/* Rule number */
    u_short fw_flg;			/* Flags word */
#define IP_FW_MAX_PORTS	10		/* A reasonable maximum */
    u_short fw_pts[IP_FW_MAX_PORTS];	/* Array of port numbers to match */
    u_char fw_ipopt,fw_ipnopt;		/* IP options set/unset */
    u_char fw_tcpf,fw_tcpnf;		/* TCP flags set/unset */
#define IP_FW_ICMPTYPES_DIM (32 / (sizeof(unsigned) * 8))
    unsigned fw_icmptypes[IP_FW_ICMPTYPES_DIM]; /* ICMP types bitmap */
    long timestamp;			/* timestamp (tv_sec) of last match */
    union ip_fw_if fw_in_if, fw_out_if;	/* Incoming and outgoing interfaces */
    union {
	u_short fu_divert_port;		/* Divert/tee port (options IPDIVERT) */
	u_short fu_skipto_rule;		/* SKIPTO command rule number */
	u_short fu_reject_code;		/* REJECT response code */
    } fw_un;
    u_char fw_prot;			/* IP protocol */
    u_char fw_nports;			/* N'of src ports and # of dst ports */
					/* in ports array (dst ports follow */
					/* src ports; max of 10 ports in all; */
					/* count of 0 means match all ports) */
};

#define IP_FW_GETNSRCP(rule)		((rule)->fw_nports & 0x0f)
#define IP_FW_SETNSRCP(rule, n)		do {				\
					  (rule)->fw_nports &= ~0x0f;	\
					  (rule)->fw_nports |= (n);	\
					} while (0)
#define IP_FW_GETNDSTP(rule)		((rule)->fw_nports >> 4)
#define IP_FW_SETNDSTP(rule, n)		do {				\
					  (rule)->fw_nports &= ~0xf0;	\
					  (rule)->fw_nports |= (n) << 4;\
					} while (0)

#define fw_divert_port	fw_un.fu_divert_port
#define fw_skipto_rule	fw_un.fu_skipto_rule
#define fw_reject_code	fw_un.fu_reject_code

struct ip_fw_chain {
        LIST_ENTRY(ip_fw_chain) chain;
        struct ip_fw    *rule;
};

/*
 * Values for "flags" field .
 */
#define IP_FW_F_IN	0x0001	/* Check inbound packets		*/
#define IP_FW_F_OUT	0x0002	/* Check outbound packets		*/
#define IP_FW_F_IIFACE	0x0004	/* Apply inbound interface test		*/
#define IP_FW_F_OIFACE	0x0008	/* Apply outbound interface test	*/

#define IP_FW_F_COMMAND 0x0070	/* Mask for type of chain entry:	*/
#define IP_FW_F_DENY	0x0000	/* This is a deny rule			*/
#define IP_FW_F_REJECT	0x0010	/* Deny and send a response packet	*/
#define IP_FW_F_ACCEPT	0x0020	/* This is an accept rule		*/
#define IP_FW_F_COUNT	0x0030	/* This is a count rule			*/
#define IP_FW_F_DIVERT	0x0040	/* This is a divert rule		*/
#define IP_FW_F_TEE	0x0050	/* This is a tee rule			*/
#define IP_FW_F_SKIPTO	0x0060	/* This is a skipto rule		*/

#define IP_FW_F_PRN	0x0080	/* Print if this rule matches		*/

#define IP_FW_F_SRNG	0x0100	/* The first two src ports are a min	*
				 * and max range (stored in host byte	*
				 * order).				*/

#define IP_FW_F_DRNG	0x0200	/* The first two dst ports are a min	*
				 * and max range (stored in host byte	*
				 * order).				*/

#define IP_FW_F_IIFNAME	0x0400	/* In interface by name/unit (not IP)	*/
#define IP_FW_F_OIFNAME	0x0800	/* Out interface by name/unit (not IP)	*/

#define IP_FW_F_INVSRC	0x1000	/* Invert sense of src check		*/
#define IP_FW_F_INVDST	0x2000	/* Invert sense of dst check		*/

#define IP_FW_F_FRAG	0x4000	/* Fragment				*/

#define IP_FW_F_ICMPBIT 0x8000	/* ICMP type bitmap is valid		*/

#define IP_FW_F_MASK	0xFFFF	/* All possible flag bits mask		*/

/*
 * For backwards compatibility with rules specifying "via iface" but
 * not restricted to only "in" or "out" packets, we define this combination
 * of bits to represent this configuration.
 */

#define IF_FW_F_VIAHACK	(IP_FW_F_IN|IP_FW_F_OUT|IP_FW_F_IIFACE|IP_FW_F_OIFACE)

/*
 * Definitions for REJECT response codes.
 * Values less than 256 correspond to ICMP unreachable codes.
 */
#define IP_FW_REJECT_RST	0x0100		/* TCP packets: send RST */

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
void ip_fw_init __P((void));

#endif /* KERNEL */

#endif /* _IP_FW_H */
