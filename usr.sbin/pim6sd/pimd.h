/*
 *  Copyright (c) 1998 by the University of Southern California.
 *  All rights reserved.
 *
 *  Permission to use, copy, modify, and distribute this software and
 *  its documentation in source and binary forms for lawful
 *  purposes and without fee is hereby granted, provided
 *  that the above copyright notice appear in all copies and that both
 *  the copyright notice and this permission notice appear in supporting
 *  documentation, and that any documentation, advertising materials,
 *  and other materials related to such distribution and use acknowledge
 *  that the software was developed by the University of Southern
 *  California and/or Information Sciences Institute.
 *  The name of the University of Southern California may not
 *  be used to endorse or promote products derived from this software
 *  without specific prior written permission.
 *
 *  THE UNIVERSITY OF SOUTHERN CALIFORNIA DOES NOT MAKE ANY REPRESENTATIONS
 *  ABOUT THE SUITABILITY OF THIS SOFTWARE FOR ANY PURPOSE.  THIS SOFTWARE IS
 *  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
 *  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, TITLE, AND
 *  NON-INFRINGEMENT.
 *
 *  IN NO EVENT SHALL USC, OR ANY OTHER CONTRIBUTOR BE LIABLE FOR ANY
 *  SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES, WHETHER IN CONTRACT,
 *  TORT, OR OTHER FORM OF ACTION, ARISING OUT OF OR IN CONNECTION WITH,
 *  THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *  Other copyrights might apply to parts of this software and are so
 *  noted when applicable.
 */
/*  Questions concerning this software should be directed to
 *  Mickael Hoerdt (hoerdt@clarinet.u-strasbg.fr) LSIIT Strasbourg.
 *
 */
/*
 * This program has been derived from pim6dd.        
 * The pim6dd program is covered by the license in the accompanying file
 * named "LICENSE.pim6dd".
 */
/*
 * This program has been derived from pimd.        
 * The pimd program is covered by the license in the accompanying file
 * named "LICENSE.pimd".
 *
 * $FreeBSD: src/usr.sbin/pim6sd/pimd.h,v 1.1.2.1 2000/07/15 07:36:37 kris Exp $
 */


#ifndef PIMD_H
#define PIMD_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "defs.h"

#define PIM_PROTOCOL_VERSION		2

/* PIM protocol timers (in seconds) */
#define PIM_REGISTER_SUPPRESSION_TIMEOUT 60
#define PIM_REGISTER_PROBE_TIME           5 /* Used to send NULL_REGISTER */
#define PIM_DATA_TIMEOUT                210

#define PIM_TIMER_HELLO_PERIOD           30
#define PIM_JOIN_PRUNE_PERIOD             60

#define PIM_JOIN_PRUNE_HOLDTIME        (3.5 * PIM_JOIN_PRUNE_PERIOD)
#define PIM_RANDOM_DELAY_JOIN_TIMEOUT   4.5

#define PIM_DEFAULT_CAND_RP_ADV_PERIOD   60
#define PIM_DEFAULT_BOOTSTRAP_PERIOD     60

#define PIM_BOOTSTRAP_TIMEOUT          (2.5 * PIM_DEFAULT_BOOTSTRAP_PERIOD + 10)
#define PIM_TIMER_HELLO_HOLDTIME       (3.5 * PIM_TIMER_HELLO_PERIOD)
#define PIM_ASSERT_TIMEOUT              180


/* Misc definitions */
#define PIM_DEFAULT_CAND_RP_PRIORITY      0 /* 0 is the highest. Don't know
                         		     * why this is the default.
                         		     * See the PS version (Mar' 97),
                         		     * pp.22 bottom of the spec.
                         		     */

#define PIM_DEFAULT_BSR_PRIORITY          0  /* 0 is the lowest               */
#define RP_DEFAULT_IPV6_HASHMASKLEN       126 /* the default group msklen used
                         		       *  by the hash function to
					       * calculate the group-to-RP
                         		       * mapping
					       */

#define SINGLE_SRC_MSK6LEN            128 /* the single source mask length */
#define SINGLE_GRP_MSK6LEN            128 /* the single group mask length  */

/* TODO: change? */ 
#define PIM_GROUP_PREFIX_DEFAULT_MASKLEN 8 /* The default group masklen if
                                             * omitted in the config file.
                                             */

/* Datarate related definitions */
/* REG_RATE is used by the RP to switch to the shortest path instead of
 * decapsulating Registers.
 * DATA_RATE is the threshold for the last hop router to initiate
 * switching to the shortest path.
 */
/* TODO: XXX: probably no need for two different intervals.
 */

#define PIM_DEFAULT_REG_RATE            50000 /* max # of register bits/s   */
#define PIM_DEFAULT_REG_RATE_INTERVAL      20 /* regrate probe interval     */
#define PIM_DEFAULT_DATA_RATE           50000 /* max # of data bits/s       */
#define PIM_DEFAULT_DATA_RATE_INTERVAL     20 /* datarate check interval    */

#define DATA_RATE_CHECK_INTERVAL           20 /* Data rate check interval   */
#define REG_RATE_CHECK_INTERVAL            20 /* PIM Reg. rate check interval*/

#define UCAST_ROUTING_CHECK_INTERVAL       20 /* Unfortunately, if the unicast
                                               * routing changes, the kernel
                                               * or any of the existing
                                               * unicast routing daemons
                                               * don't send us a signal.
                                               * Have to ask periodically the
                                               * kernel for any route changes.
                                               * Default: every 20 seconds.
                                               * Sigh.
                                               */

#define DEFAULT_PHY_RATE_LIMIT  0             /* default phyint rate limit  */
#define DEFAULT_REG_RATE_LIMIT  0             /* default register_vif rate limit  */

/**************************************************************************
 * PIM Encoded-Unicast, Encoded-Group and Encoded-Source Address formats  *
 *************************************************************************/
/* Address families definition */
#define ADDRF_RESERVED  0
#define ADDRF_IPv4      1
#define ADDRF_IPv6      2
#define ADDRF_NSAP      3
#define ADDRF_HDLC      4
#define ADDRF_BBN1822   5
#define ADDRF_802       6
#define ADDRF_ETHERNET  ADDRF_802
#define ADDRF_E163      7
#define ADDRF_E164      8
#define ADDRF_SMDS      ADDRF_E164
#define ADDRF_ATM       ADDRF_E164
#define ADDRF_F69       9
#define ADDRF_TELEX     ADDRF_F69
#define ADDRF_X121      10
#define ADDRF_X25       ADDRF_X121
#define ADDRF_IPX       11
#define ADDRF_APPLETALK 12
#define ADDRF_DECNET_IV 13
#define ADDRF_BANYAN    14
#define ADDRF_E164_NSAP 15

/* Addresses Encoding Type (specific for each Address Family */
#define ADDRT_IPv6      0


/* Encoded-Unicast: 18 bytes long */
typedef struct pim6_encod_uni_addr_ {
    u_int8      addr_family;
    u_int8      encod_type;
    struct in6_addr unicast_addr;    /* XXX: Note the 32-bit boundary
                      * misalignment for  the unicast
                      * address when placed in the
                      * memory. Must read it byte-by-byte!
                      */ 
} pim6_encod_uni_addr_t;
/* XXX: sizeof(pim6_encod_uni_addr_t) does not work due to misalignment */
#define PIM6_ENCODE_UNI_ADDR_LEN 18

/* Encoded-Group */
typedef struct pim6_encod_grp_addr_ {
    u_int8      addr_family;
    u_int8      encod_type;
    u_int8      reserved;
    u_int8      masklen;
    struct in6_addr     mcast_addr;
} pim6_encod_grp_addr_t;
/* XXX: sizeof(pim6_encod_grp_addr_t) MAY NOT work due to an alignment problem */
#define PIM6_ENCODE_GRP_ADDR_LEN 20

/* Encoded-Source */
typedef struct pim6_encod_src_addr_ {
    u_int8      addr_family;
    u_int8      encod_type;
    u_int8      flags;
    u_int8      masklen;
    struct in6_addr src_addr;
} pim6_encod_src_addr_t;
/* XXX: sizeof(pim6_encod_src_addr_t) MAY NOT work due to an alignment problem */
#define PIM6_ENCODE_SRC_ADDR_LEN 20

#define USADDR_RP_BIT 0x1
#define USADDR_WC_BIT 0x2
#define USADDR_S_BIT  0x4

/**************************************************************************
 *                       PIM Messages formats                             *
 *************************************************************************/

/* PIM Hello */
typedef struct pim_hello_ {
    u_int16     option_type;   /* Option type */
    u_int16     option_length; /* Length of the Option Value field in bytes */
} pim_hello_t;

/* PIM Register */
typedef struct pim_register_ {
    u_int32     reg_flags;
} pim_register_t;

/* PIM Register-Stop */
typedef struct pim_register_stop_ {
    pim6_encod_grp_addr_t encod_grp;
    pim6_encod_uni_addr_t encod_src; /* XXX: 18 bytes long, misaligned */
} pim_register_stop_t;

/* PIM Join/Prune: XXX: all 128-bit addresses misaligned! */
typedef struct pim_jp_header_ {
    pim6_encod_uni_addr_t encod_upstream_nbr;
    u_int8     reserved;
    u_int8     num_groups;
    u_int16    holdtime;
} pim_jp_header_t;


typedef struct pim_jp_encod_grp_ {
    pim6_encod_grp_addr_t   encod_grp;
    u_int16                number_join_src;
    u_int16                number_prune_src;
} pim_jp_encod_grp_t;


#define PIM_ACTION_NOTHING 0
#define PIM_ACTION_JOIN    1
#define PIM_ACTION_PRUNE   2

#define PIM_IIF_SOURCE     1
#define PIM_IIF_RP         2

#define PIM_ASSERT_RPT_BIT 0x80000000

/* PIM messages type */

#define PIM_HELLO		 	0	
#define PIM_REGISTER_STOP       	2
#define PIM_JOIN_PRUNE          	3
#define PIM_BOOTSTRAP       		4
#define PIM_ASSERT              	5
#define PIM_GRAFT               	6
#define PIM_GRAFT_ACK           	7 
#define PIM_CAND_RP_ADV         	8

#define PIM_V2_HELLO            	PIM_HELLO
#define PIM_V2_REGISTER         	PIM_REGISTER 
#define PIM_V2_REGISTER_STOP    	PIM_REGISTER_STOP
#define PIM_V2_JOIN_PRUNE       	PIM_JOIN_PRUNE
#define PIM_V2_BOOTSTRAP    		PIM_BOOTSTRAP
#define PIM_V2_ASSERT           	PIM_ASSERT 
#define PIM_V2_GRAFT            	PIM_GRAFT
#define PIM_V2_GRAFT_ACK        	PIM_GRAFT_ACK
#define PIM_V2_CAND_RP_ADV      	PIM_CAND_RP_ADV


/* Vartious options from PIM messages definitions */
/* PIM_HELLO definitions */

#define PIM_MESSAGE_HELLO_HOLDTIME              1
#define PIM_MESSAGE_HELLO_HOLDTIME_LENGTH       2
#define PIM_MESSAGE_HELLO_HOLDTIME_FOREVER      0xffff

/* PIM_REGISTER definitions */
#define PIM_MESSAGE_REGISTER_BORDER_BIT         0x80000000
#define PIM_MESSAGE_REGISTER_NULL_REGISTER_BIT  0x40000000

#define MASK_TO_MASKLEN6(mask , masklen) \
do { \
			register u_int32 tmp_mask;							\
			register u_int8  tmp_masklen = sizeof((mask)) <<3;	\
			int i;  \
			int kl; \
			for(i=0;i<4;i++)									\
			{													\
				tmp_mask=ntohl(*(u_int32_t *)&mask.s6_addr[i * 4]); \
				for(kl=32; tmp_masklen >0 && kl>0 ; tmp_masklen--, kl-- , tmp_mask >>=1) \
					if( tmp_mask & 0x1)			\
						break;   	\
			} \
			(masklen) =tmp_masklen;	\
		} while (0) 

#define MASKLEN_TO_MASK6(masklen, mask6) \
     do {\
         u_char maskarray[8] = \
         {0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff}; \
         int bytelen, bitlen, i; \
         memset(&(mask6), 0, sizeof(mask6));\
         bytelen = (masklen) / 8;\
         bitlen = (masklen) % 8;\
         for (i = 0; i < bytelen; i++) \
             (mask6).s6_addr[i] = 0xff;\
         if (bitlen) \
             (mask6).s6_addr[bytelen] = maskarray[bitlen - 1]; \
     }while(0);

/*
 * A bunch of macros because of the lack of 32-bit boundary alignment.
 * All because of one misalligned address format. Hopefully this will be
 * fixed in PIMv3. (cp) must be (u_int8 *) .
 */
/* Originates from Eddy Rusty's (eddy@isi.edu) PIM-SM implementation for
 * gated.
 */

/* PUT_NETLONG puts "network ordered" data to the datastream.
 * PUT_HOSTLONG puts "host ordered" data to the datastream.
 * GET_NETLONG gets the data and keeps it in "network order" in the memory
 * GET_HOSTLONG gets the data, but in the memory it is in "host order"
 * The same for all {PUT,GET}_{NET,HOST}{SHORT,LONG}
 */

#define GET_BYTE(val, cp)       ((val) = *(cp)++)
#define PUT_BYTE(val, cp)       (*(cp)++ = (u_int8)(val))

#define GET_HOSTSHORT(val, cp)                  \
        do {                                    \
                register u_int16 Xv;            \
                Xv = (*(cp)++) << 8;            \
                Xv |= *(cp)++;                  \
                (val) = Xv;                     \
        } while (0)

#define PUT_HOSTSHORT(val, cp)                  \
         do {                                    \
                 register u_int16 Xv;            \
                 Xv = (u_int16)(val);            \
                 *(cp)++ = (u_int8)(Xv >> 8);    \
                 *(cp)++ = (u_int8)Xv;           \
         } while (0)

#if defined(BYTE_ORDER) && (BYTE_ORDER == LITTLE_ENDIAN)
#define GET_NETSHORT(val, cp)                   \
        do {                                    \
                register u_int16 Xv;            \
                Xv = *(cp)++;                   \
                Xv |= (*(cp)++) << 8;           \
                (val) = Xv;                     \
        } while (0)
#define PUT_NETSHORT(val, cp)                   \
        do {                                    \
                register u_int16 Xv;            \
                Xv = (u_int16)(val);            \
                *(cp)++ = (u_int8)Xv;           \
                *(cp)++ = (u_int8)(Xv >> 8);    \
        } while (0)
#else
#define GET_NETSHORT(val, cp) GET_HOSTSHORT(val, cp)
#define PUT_NETSHORT(val, cp) PUT_HOSTSHORT(val, cp)
#endif /* {GET,PUT}_NETSHORT */

#define GET_HOSTLONG(val, cp)                   \
        do {                                    \
                register u_long Xv;             \
                Xv  = (*(cp)++) << 24;          \
                Xv |= (*(cp)++) << 16;          \
                Xv |= (*(cp)++) <<  8;          \
                Xv |= *(cp)++;                  \
                (val) = Xv;                     \
        } while (0)

#define PUT_HOSTLONG(val, cp)                   \
        do {                                    \
                register u_int32 Xv;            \
                Xv = (u_int32)(val);            \
                *(cp)++ = (u_int8)(Xv >> 24);   \
                *(cp)++ = (u_int8)(Xv >> 16);   \
                *(cp)++ = (u_int8)(Xv >>  8);   \
                *(cp)++ = (u_int8)Xv;           \
        } while (0)

#if defined(BYTE_ORDER) && (BYTE_ORDER == LITTLE_ENDIAN)
#define GET_NETLONG(val, cp)                    \
        do {                                    \
                register u_long Xv;             \
                Xv  = *(cp)++;                  \
                Xv |= (*(cp)++) <<  8;          \
                Xv |= (*(cp)++) << 16;          \
                Xv |= (*(cp)++) << 24;          \
                (val) = Xv;                     \
        } while (0)

#define PUT_NETLONG(val, cp)                    \
        do {                                    \
                register u_int32 Xv;            \
                Xv = (u_int32)(val);            \
                *(cp)++ = (u_int8)Xv;           \
                *(cp)++ = (u_int8)(Xv >>  8);   \
                *(cp)++ = (u_int8)(Xv >> 16);   \
                *(cp)++ = (u_int8)(Xv >> 24);   \
        } while (0)
#else
#define GET_NETLONG(val, cp) GET_HOSTLONG(val, cp)
#define PUT_NETLONG(val, cp) PUT_HOSTLONG(val, cp)
#endif /* {GET,PUT}_HOSTLONG */

#define GET_ESADDR6(esa, cp)  /* XXX: hard coding */  \
        do {                                    \
            (esa)->addr_family = *(cp)++;       \
            (esa)->encod_type  = *(cp)++;       \
            (esa)->flags       = *(cp)++;       \
            (esa)->masklen     = *(cp)++;       \
             memcpy(&(esa)->src_addr, (cp), sizeof(struct in6_addr)); \
         (cp) += sizeof(struct in6_addr);   \
        } while(0)

#define PUT_ESADDR6(addr, masklen, flags, cp)    \
        do {                                    \
            int i; \
            struct in6_addr maskaddr; \
        MASKLEN_TO_MASK6(masklen, maskaddr); \
            *(cp)++ = ADDRF_IPv6; /* family */  \
            *(cp)++ = ADDRT_IPv6; /* type   */  \
            *(cp)++ = (flags);    /* flags  */  \
            *(cp)++ = (masklen);                \
            for (i = 0; i < sizeof(struct in6_addr); i++, (cp)++) \
                *(cp) = maskaddr.s6_addr[i] & (addr).s6_addr[i]; \
        } while(0)

#define GET_EGADDR6(ega, cp) /* XXX: hard coding */ \
        do {                                    \
            (ega)->addr_family = *(cp)++;       \
            (ega)->encod_type  = *(cp)++;       \
            (ega)->reserved    = *(cp)++;       \
            (ega)->masklen     = *(cp)++;       \
             memcpy(&(ega)->mcast_addr, (cp), sizeof(struct in6_addr)); \
        (cp) += sizeof(struct in6_addr);    \
        } while(0)

#define PUT_EGADDR6(addr, masklen, reserved, cp) \
         do {                                    \
             int i; \
             struct in6_addr maskaddr; \
         MASKLEN_TO_MASK6(masklen, maskaddr); \
             *(cp)++ = ADDRF_IPv6; /* family */  \
             *(cp)++ = ADDRT_IPv6; /* type   */  \
             *(cp)++ = (reserved); /* reserved; should be 0 */  \
             *(cp)++ = (masklen);                \
             for (i = 0; i < sizeof(struct in6_addr); i++, (cp)++) \
                 *(cp) = maskaddr.s6_addr[i] & (addr).s6_addr[i]; \
         } while(0)


#define GET_EUADDR6(eua, cp)  /* XXX hard conding */    \
        do {                                    \
         (eua)->addr_family = *(cp)++;  \
         (eua)->encod_type  = *(cp)++;  \
             memcpy(&(eua)->unicast_addr, (cp), sizeof(struct in6_addr)); \
         (cp) += sizeof(struct in6_addr);   \
        } while(0)

#define PUT_EUADDR6(addr, cp) \
         do {                                    \
             *(cp)++ = ADDRF_IPv6; /* family */  \
             *(cp)++ = ADDRT_IPv6; /* type   */  \
             memcpy((cp), &(addr), sizeof(struct in6_addr)); \
             (cp) += sizeof(struct in6_addr); \
         } while(0)

/* Used if no relaible unicast routing information available */
#define UCAST_DEFAULT_SOURCE_METRIC     1024
#define UCAST_DEFAULT_SOURCE_PREFERENCE 1024


#define DEFAULT_LOCAL_PREF 101			/* assert pref par defaut */
#define DEFAULT_LOCAL_METRIC 1024		/* assert metrique par default */


/*  
 * TODO: recalculate the messages sizes, probably with regard to the MTU
 * TODO: cleanup
 */

#define MAX_JP_MESSAGE_SIZE     8192
#define MAX_JP_MESSAGE_POOL_NUMBER 8
#define MAX_JOIN_LIST_SIZE      1500 
#define MAX_PRUNE_LIST_SIZE     1500

#define STAR_STAR_RP_MSK6LEN     8 		/* Masklen for
                             			* ff00 ::
                             			* to encode (*,*,RP)
                             			*/ 

/* interface independent statistics */
struct pim6dstat {
	/* incoming PIM6 packets on this interface */
	u_quad_t in_pim6_register;
	u_quad_t in_pim6_register_stop;
	u_quad_t in_pim6_cand_rp;
	u_quad_t in_pim6_graft; /* for dense mode only */
	u_quad_t in_pim6_graft_ack; /* for dense mode only  */
	/* outgoing PIM6 packets on this interface */
	u_quad_t out_pim6_register;
	u_quad_t out_pim6_register_stop;
	u_quad_t out_pim6_cand_rp;
	/* SPT transition */
	u_quad_t pim6_trans_spt_forward;
	u_quad_t pim6_trans_spt_rp;
	/* occurrences of timeouts */
	u_quad_t pim6_bootstrap_timo;/* pim_bootstrap_timer */
	u_quad_t pim6_rpgrp_timo; /* rp_grp_entry_ptr->holdtime */
	u_quad_t pim6_rtentry_timo; /* routing entry */
	/* kernel internals */
	u_quad_t kern_add_cache;
    	u_quad_t kern_add_cache_fail;
	u_quad_t kern_del_cache;
    	u_quad_t kern_del_cache_fail;
	u_quad_t kern_sgcnt_fail;
};

extern struct pim6dstat pim6dstat;
#endif
