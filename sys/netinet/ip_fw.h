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
 *	$Id: ip_fw.h,v 1.13 1995/07/23 05:36:30 davidg Exp $
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
    struct ip_fw *fw_next;		/* Next firewall on chain */
    struct in_addr fw_src, fw_dst;	/* Source and destination IP addr */
    struct in_addr fw_smsk, fw_dmsk;	/* Mask for src and dest IP addr */
	/*
	 * This union keeps all "via" information.
	 * If ever fu_via_ip is 0,or IP_FW_F_IFNAME set and
	 * fu_via_name[0] is 0 - match any packet.
	 */
    union {
	struct in_addr fu_via_ip;
	struct {
#define FW_IFNLEN	6		/* To keep structure on 2^x boundary */
		char  fu_via_name[FW_IFNLEN];
		short fu_via_unit;
	} fu_via_if;
    } fu_via_un;
    u_short fw_flg;			/* Flags word */
    u_short fw_nsp, fw_ndp;             /* N'of src ports and # of dst ports */
    					/* in ports array (dst ports follow */
    					/* src ports; max of 10 ports in all; */
    					/* count of 0 means match all ports) */
#define IP_FW_MAX_PORTS	10      	/* A reasonable maximum */
    u_short fw_pts[IP_FW_MAX_PORTS];    /* Array of port numbers to match */
    u_long fw_pcnt,fw_bcnt;		/* Packet and byte counters */
    u_char fw_ipopt,fw_ipnopt;		/* IP options set/unset */
    u_char fw_tcpf,fw_tcpnf;		/* TCP flags sen/unset */
};


/*
 * Definitions to make expressions
 * for "via" stuff shorter.
 */
#define fw_via_ip	fu_via_un.fu_via_ip
#define fw_via_name	fu_via_un.fu_via_if.fu_via_name
#define fw_via_unit	fu_via_un.fu_via_if.fu_via_unit

/*
 * Values for "flags" field .
 */

#define IP_FW_F_ALL	0x000	/* This is a universal packet firewall*/
#define IP_FW_F_TCP	0x001	/* This is a TCP packet firewall      */
#define IP_FW_F_UDP	0x002	/* This is a UDP packet firewall      */
#define IP_FW_F_ICMP	0x003	/* This is a ICMP packet firewall     */
#define IP_FW_F_KIND	0x003	/* Mask to isolate firewall kind      */
#define IP_FW_F_ACCEPT	0x004	/* This is an accept firewall (as     *
				 *         opposed to a deny firewall)*
				 *                                    */
#define IP_FW_F_SRNG	0x008	/* The first two src ports are a min  *
				 * and max range (stored in host byte *
				 * order).                            *
				 *                                    */
#define IP_FW_F_DRNG	0x010	/* The first two dst ports are a min  *
				 * and max range (stored in host byte *
				 * order).                            *
				 * (ports[0] <= port <= ports[1])     *
				 *                                    */
#define IP_FW_F_PRN	0x020	/* In verbose mode print this firewall*/
#define IP_FW_F_BIDIR	0x040	/* For accounting-count two way       */
#define IP_FW_F_ICMPRPL	0x100	/* Send back icmp unreachable packet  */
#define IP_FW_F_IFNAME	0x200	/* Use interface name/unit (not IP)   */
#define IP_FW_F_MASK	0x3FF	/* All possible flag bits mask        */

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
#define IP_FW_TCPF_PUSH		TH_PUSH
#define IP_FW_TCPF_ACK		TH_ACK
#define IP_FW_TCPF_URG		TH_URG

/*
 * New IP firewall options for [gs]etsockopt at the RAW IP level.
 */
#define IP_FW_BASE_CTL	53

#define IP_FW_ADD     (IP_FW_BASE_CTL)
#define IP_FW_DEL     (IP_FW_BASE_CTL+4)
#define IP_FW_FLUSH   (IP_FW_BASE_CTL+6)
#define IP_FW_POLICY  (IP_FW_BASE_CTL+7)

#define IP_ACCT_ADD   (IP_FW_BASE_CTL+10)
#define IP_ACCT_DEL   (IP_FW_BASE_CTL+11)
#define IP_ACCT_FLUSH (IP_FW_BASE_CTL+12)
#define IP_ACCT_ZERO  (IP_FW_BASE_CTL+13)
#define IP_ACCT_CLR   (IP_FW_BASE_CTL+14)

/*
 * Policy flags...
 */
#define IP_FW_P_DENY		0x01
#define IP_FW_P_ICMP		0x02
#define IP_FW_P_MBIPO		0x04
#define IP_FW_P_MASK		0x07


/*
 * Main firewall chains definitions and global var's definitions.
 */
#ifdef KERNEL

/*
 * Variables/chain.
 */
extern struct  ip_fw *ip_fw_chain;
extern u_short ip_fw_policy;

extern struct  ip_fw *ip_acct_chain;

/*
 * Function pointers.
 */
extern int (*ip_fw_chk_ptr)(struct mbuf *, struct ip *,struct ifnet *,struct ip_fw *);
extern int (*ip_fw_ctl_ptr)(int,struct mbuf *);

extern void (*ip_acct_cnt_ptr)(struct ip *,struct ifnet *,struct ip_fw *,int);
extern int  (*ip_acct_ctl_ptr)(int,struct mbuf *);

/*
 * Function definitions.
 */
int ip_fw_chk(struct mbuf *, struct ip *,struct ifnet *,struct ip_fw *);
int ip_fw_ctl(int,struct mbuf *);

void ip_acct_cnt(struct ip *,struct ifnet *,struct ip_fw *,int);
int  ip_acct_ctl(int,struct mbuf *);

#endif /* KERNEL */

#endif /* _IP_FW_H */
