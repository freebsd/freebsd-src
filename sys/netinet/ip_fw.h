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

struct ip_firewall {
    struct ip_firewall *next;		/* Next firewall on chain */
    struct in_addr src, dst;		/* Source and destination IP addr */
    struct in_addr src_mask, dst_mask;	/* Mask for src and dest IP addr */
    u_short flags;
#define IP_FIREWALL_UNIVERSAL	0	/* This is a universal packet firewall*/
#define IP_FIREWALL_TCP		1	/* This is a TCP packet firewall */
#define IP_FIREWALL_UDP		2	/* This is a UDP packet firewall */
#define IP_FIREWALL_ICMP	3	/* This is a ICMP packet firewall */
#define IP_FIREWALL_KIND	3	/* Mask to isolate firewall kind */
#define IP_FIREWALL_ACCEPT	4	/* This is an accept firewall (as */
					/* opposed to a deny firewall) */
#define IP_FIREWALL_SRC_RANGE	8	/* The first two src ports are a min 
					 * and max range (stored in host byte 
					 * order). 
					 */
#define IP_FIREWALL_DST_RANGE	16	/* The first two dst ports are a min 
					 * and max range (stored in host byte 
					 * order). 
					 * (ports[0] <= port <= ports[1]) 
					 */
#define IP_FIREWALL_PRINT	32	/* In verbos mode print this firewall */
#define IP_FIREWALL_FLAG_BITS	0x2f	/* All possible flag bits */
    u_short num_src_ports, num_dst_ports;/* # of src ports and # of dst ports */
    					/* in ports array (dst ports follow */
    					/* src ports; max of 10 ports in all; */
    					/* count of 0 means match all ports) */
#define IP_FIREWALL_MAX_PORTS	10	/* A reasonable maximum */
    u_short ports[IP_FIREWALL_MAX_PORTS]; /* Array of port numbers to match */
};

/*    
 * New IP firewall options for [gs]etsockopt at the RAW IP level.
 */     
#define IP_FW_BASE_CTL	53

#define IP_FW_ADD_BLK (IP_FW_BASE_CTL)
#define IP_FW_ADD_FWD (IP_FW_BASE_CTL+1)   
#define IP_FW_CHK_BLK (IP_FW_BASE_CTL+2)
#define IP_FW_CHK_FWD (IP_FW_BASE_CTL+3)
#define IP_FW_DEL_BLK (IP_FW_BASE_CTL+4)
#define IP_FW_DEL_FWD (IP_FW_BASE_CTL+5)
#define IP_FW_FLUSH   (IP_FW_BASE_CTL+6)
#define IP_FW_POLICY  (IP_FW_BASE_CTL+7) 


extern struct ip_firewall *ip_fw_blk_chain;
extern struct ip_firewall *ip_fw_fwd_chain;
extern int ip_fw_policy;

#endif
