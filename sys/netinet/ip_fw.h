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
 */

/*
 * Format of an IP firewall descriptor
 *
 * src, dst, src_mask, dst_mask are always stored in network byte order.
 * flags and num_*_ports are stored in host byte order (of course).
 * Port numbers are stored in HOST byte order.
 */
#ifndef _IP_FW_H
#define _IP_FW_H

struct ip_fw {
    struct ip_fw *next;			/* Next firewall on chain */
    struct in_addr src, dst;		/* Source and destination IP addr */
    struct in_addr src_mask, dst_mask;	/* Mask for src and dest IP addr */
    struct in_addr via;			/* IP addr of interface "via" */
    u_short flags;			/* Flags word */
    u_short n_src_p, n_dst_p;           /* # of src ports and # of dst ports */
    					/* in ports array (dst ports follow */
    					/* src ports; max of 10 ports in all; */
    					/* count of 0 means match all ports) */
#define IP_FW_MAX_PORTS	10      	/* A reasonable maximum */
    u_short ports[IP_FW_MAX_PORTS];     /* Array of port numbers to match */
    u_long p_cnt,b_cnt;			/* Packet and byte counters */
};

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
#define IP_FW_F_TCPSYN	0x080	/* For tcp packets-check SYN only     */
#define IP_FW_F_ICMPRPL	0x100	/* Send back icmp unreachable packet  */
#define IP_FW_F_MASK	0x1FF	/* All possible flag bits mask        */

/*    
 * New IP firewall options for [gs]etsockopt at the RAW IP level.
 */     
#define IP_FW_BASE_CTL	53

#define IP_FW_ADD_BLK (IP_FW_BASE_CTL)
#define IP_FW_ADD_FWD (IP_FW_BASE_CTL+1)   
#define IP_FW_DEL_BLK (IP_FW_BASE_CTL+4)
#define IP_FW_DEL_FWD (IP_FW_BASE_CTL+5)
#define IP_FW_FLUSH   (IP_FW_BASE_CTL+6)
#define IP_FW_POLICY  (IP_FW_BASE_CTL+7) 

#define IP_ACCT_ADD   (IP_FW_BASE_CTL+10)
#define IP_ACCT_DEL   (IP_FW_BASE_CTL+11)
#define IP_ACCT_FLUSH (IP_FW_BASE_CTL+12)
#define IP_ACCT_ZERO  (IP_FW_BASE_CTL+13)

/*
 * Policy flags...
 */
#define IP_FW_P_DENY		0x01
#define IP_FW_P_ICMP		0x02
#define IP_FW_P_MASK		0x03


/*
 * Main firewall chains definitions and global var's definitions.
 */
#ifdef KERNEL
#ifdef IPFIREWALL
extern struct ip_fw *ip_fw_blk_chain;
extern struct ip_fw *ip_fw_fwd_chain;
extern u_short ip_fw_policy;
#endif
#ifdef IPACCT
extern struct ip_fw *ip_acct_chain;
#endif
#endif /* KERNEL */

#endif /* _IP_FW_H */
