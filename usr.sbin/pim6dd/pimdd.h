/*
 *  Copyright (c) 1998 by the University of Oregon.
 *  All rights reserved.
 *
 *  Permission to use, copy, modify, and distribute this software and
 *  its documentation in source and binary forms for lawful
 *  purposes and without fee is hereby granted, provided
 *  that the above copyright notice appear in all copies and that both
 *  the copyright notice and this permission notice appear in supporting
 *  documentation, and that any documentation, advertising materials,
 *  and other materials related to such distribution and use acknowledge
 *  that the software was developed by the University of Oregon.
 *  The name of the University of Oregon may not be used to endorse or 
 *  promote products derived from this software without specific prior 
 *  written permission.
 *
 *  THE UNIVERSITY OF OREGON DOES NOT MAKE ANY REPRESENTATIONS
 *  ABOUT THE SUITABILITY OF THIS SOFTWARE FOR ANY PURPOSE.  THIS SOFTWARE IS
 *  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
 *  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, TITLE, AND 
 *  NON-INFRINGEMENT.
 *
 *  IN NO EVENT SHALL UO, OR ANY OTHER CONTRIBUTOR BE LIABLE FOR ANY
 *  SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES, WHETHER IN CONTRACT,
 *  TORT, OR OTHER FORM OF ACTION, ARISING OUT OF OR IN CONNECTION WITH,
 *  THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *  Other copyrights might apply to parts of this software and are so
 *  noted when applicable.
 */
/*
 *  Questions concerning this software should be directed to 
 *  Kurt Windisch (kurtw@antc.uoregon.edu)
 *
 *  $Id: pimdd.h,v 1.1.1.1 1999/08/08 23:30:53 itojun Exp $
 */
/*
 * Part of this program has been derived from PIM sparse-mode pimd.
 * The pimd program is covered by the license in the accompanying file
 * named "LICENSE.pimd".
 *  
 * The pimd program is COPYRIGHT 1998 by University of Southern California.
 *
 * Part of this program has been derived from mrouted.
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE.mrouted".
 * 
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 * $FreeBSD: src/usr.sbin/pim6dd/pimdd.h,v 1.1.2.1 2000/07/15 07:36:30 kris Exp $
 */

#include <netinet6/pim6.h>

#define PIM_PROTOCOL_VERSION	2
#define PIMD_VERSION		PIM_PROTOCOL_VERSION
#define PIMD_SUBVERSION         1
#if 0
#define PIM_CONSTANT            0x000eff00      /* constant portion of 'group' field */
#endif
#define PIM_CONSTANT            0
#define PIMD_LEVEL (PIM_CONSTANT | PIMD_VERSION | (PIMD_SUBVERSION << 8))

#define INADDR_ALL_PIM_ROUTERS  (u_int32)0xe000000D	     /* 224.0.0.13 */


/* PIM protocol timers (in seconds) */
#ifndef TIMER_INTERVAL
#define TIMER_INTERVAL		          5 /* virtual timer granularity */
#endif /* TIMER_INTERVAL */

#define PIM_DATA_TIMEOUT                210
#define PIM_TIMER_HELLO_PERIOD 	         30
#define PIM_JOIN_PRUNE_HOLDTIME         210
#define PIM_RANDOM_DELAY_JOIN_TIMEOUT     3
#define PIM_GRAFT_RETRANS_PERIOD          3
#define PIM_TIMER_HELLO_HOLDTIME       (3.5 * PIM_TIMER_HELLO_PERIOD)
#define PIM_ASSERT_TIMEOUT              210


/* Misc definitions */
#define SINGLE_SRC_MSKLEN	         32 /* the single source mask length */
#define SINGLE_GRP_MSKLEN	         32 /* the single group mask length  */

#define SINGLE_SRC_MSK6LEN        128 /* the single source mask length for IPv6*/
#define SINGLE_GRP_MSK6LEN        128 /* the single group mask length  for IPv6*/

/* TODO: change? */
#define PIM_GROUP_PREFIX_DEFAULT_MASKLEN 16 /* The default group masklen if
					     * omitted in the config file.
					     */

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

#define DEFAULT_LOCAL_PREF      101           /* Default assert preference */
#define DEFAULT_LOCAL_METRIC    1024          /* Default assert metric */

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
#define ADDRT_IPv4      0
#define ADDRT_IPv6      0


#if 0				/* XXX: the definition is for IPv4 only */
/* Encoded-Unicast: 6 bytes long */
typedef struct pim_encod_uni_addr_ {
    u_int8      addr_family;
    u_int8      encod_type;
    u_int32     unicast_addr;        /* XXX: Note the 32-bit boundary
				      * misalignment for  the unicast
				      * address when placed in the
				      * memory. Must read it byte-by-byte!
				      */
} pim_encod_uni_addr_t;
#endif
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

#if 0				/* XXX: the definition is for IPv4 only */
/* Encoded-Group */
typedef struct pim_encod_grp_addr_ {
    u_int8      addr_family;
    u_int8      encod_type;
    u_int8      reserved;
    u_int8      masklen;
    u_int32     mcast_addr;
} pim_encod_grp_addr_t;
#endif
/* Encoded-Group */
typedef struct pim6_encod_grp_addr_ {
	u_int8      addr_family;
	u_int8      encod_type;
	u_int8      reserved;
	u_int8      masklen;
	struct in6_addr     mcast_addr;
} pim6_encod_grp_addr_t;

#if 0				/* XXX: the definition is for IPv4 only */
/* Encoded-Source */
typedef struct pim_encod_src_addr_ {
    u_int8      addr_family;
    u_int8      encod_type;
    u_int8      flags;
    u_int8      masklen;
    u_int32     src_addr;
} pim_encod_src_addr_t;
#endif
/* Encoded-Source */
typedef struct pim6_encod_src_addr_ {
	u_int8      addr_family;
	u_int8      encod_type;
	u_int8      flags;
	u_int8      masklen;
	struct in6_addr src_addr;
} pim6_encod_src_addr_t;
#define USADDR_RP_BIT 0x1
#define USADDR_WC_BIT 0x2
#define USADDR_S_BIT  0x4

/**************************************************************************
 *                       PIM Messages formats                             *
 *************************************************************************/
/* TODO: XXX: some structures are probably not used at all */

typedef struct pim pim_header_t;

/* PIM Hello */
typedef struct pim_hello_ {
    u_int16     option_type;   /* Option type */
    u_int16     option_length; /* Length of the Option Value field in bytes */
} pim_hello_t;

#if 0
/* PIM Join/Prune: XXX: all 32-bit addresses misaligned! */
typedef struct pim_jp_header_ {
    pim_encod_uni_addr_t encod_upstream_nbr;
    u_int8     reserved;
    u_int8     num_groups;
    u_int16    holdtime;
} pim_jp_header_t;

typedef struct pim_jp_encod_grp_ {
    pim_encod_grp_addr_t   encod_grp;
    u_int16                number_join_src;
    u_int16                number_prune_src;
} pim_jp_encod_grp_t;
#endif

#define PIM_ACTION_NOTHING 0
#define PIM_ACTION_JOIN    1
#define PIM_ACTION_PRUNE   2

#define PIM_IIF_SOURCE     1
#define PIM_IIF_RP         2

#define PIM_ASSERT_RPT_BIT 0x80000000


/* PIM messages type */
#define PIM_HELLO               0
#ifndef PIM_REGISTER
#define PIM_REGISTER            1
#endif
#define PIM_REGISTER_STOP       2
#define PIM_JOIN_PRUNE          3
#define PIM_BOOTSTRAP		4
#define PIM_ASSERT              5
#define PIM_GRAFT               6
#define PIM_GRAFT_ACK           7
#define PIM_CAND_RP_ADV         8

#define PIM_V2_HELLO            PIM_HELLO
#define PIM_V2_REGISTER         PIM_REGISTER
#define PIM_V2_REGISTER_STOP    PIM_REGISTER_STOP
#define PIM_V2_JOIN_PRUNE       PIM_JOIN_PRUNE
#define PIM_V2_BOOTSTRAP	PIM_BOOTSTRAP
#define PIM_V2_ASSERT           PIM_ASSERT
#define PIM_V2_GRAFT            PIM_GRAFT
#define PIM_V2_GRAFT_ACK        PIM_GRAFT_ACK
#define PIM_V2_CAND_RP_ADV      PIM_CAND_RP_ADV

#define PIM_V1_QUERY            0
#define PIM_V1_REGISTER         1
#define PIM_V1_REGISTER_STOP    2
#define PIM_V1_JOIN_PRUNE       3
#define PIM_V1_RP_REACHABILITY  4
#define PIM_V1_ASSERT           5
#define PIM_V1_GRAFT            6
#define PIM_V1_GRAFT_ACK        7

/* Vartious options from PIM messages definitions */
/* PIM_HELLO definitions */
#define PIM_MESSAGE_HELLO_HOLDTIME              1
#define PIM_MESSAGE_HELLO_HOLDTIME_LENGTH       2
#define PIM_MESSAGE_HELLO_HOLDTIME_FOREVER      0xffff


#define MASK_TO_MASKLEN(mask, masklen)                           \
    do {                                                         \
        register u_int32 tmp_mask = ntohl((mask));               \
        register u_int8  tmp_masklen = sizeof((mask)) << 3;      \
        for ( ; tmp_masklen > 0; tmp_masklen--, tmp_mask >>= 1)  \
            if (tmp_mask & 0x1)                                  \
                break;                                           \
        (masklen) = tmp_masklen;                                 \
    } while (0)

#define MASKLEN_TO_MASK(masklen, mask)                                       \
do {                                                                         \
    (mask) = (masklen)? htonl(~0 << ((sizeof((mask)) << 3) - (masklen))) : 0;\
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


#define GET_ESADDR(esa, cp)                     \
        do {                                    \
            (esa)->addr_family = *(cp)++;       \
            (esa)->encod_type  = *(cp)++;       \
            (esa)->flags       = *(cp)++;       \
            (esa)->masklen     = *(cp)++;       \
            GET_NETLONG((esa)->src_addr, (cp)); \
        } while(0)

#define GET_ESADDR6(esa, cp)  /* XXX: hard coding */  \
        do {                                    \
            (esa)->addr_family = *(cp)++;       \
            (esa)->encod_type  = *(cp)++;       \
            (esa)->flags       = *(cp)++;       \
            (esa)->masklen     = *(cp)++;       \
             memcpy(&(esa)->src_addr, (cp), sizeof(struct in6_addr)); \
	     (cp) += sizeof(struct in6_addr);	\
        } while(0)

#define PUT_ESADDR(addr, masklen, flags, cp)    \
        do {                                    \
            u_int32 mask;                       \
            MASKLEN_TO_MASK((masklen), mask);   \
            *(cp)++ = ADDRF_IPv4; /* family */  \
            *(cp)++ = ADDRT_IPv4; /* type   */  \
            *(cp)++ = (flags);    /* flags  */  \
            *(cp)++ = (masklen);                \
            PUT_NETLONG((addr) & mask, (cp));   \
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

#define GET_EGADDR(ega, cp)                     \
        do {                                    \
            (ega)->addr_family = *(cp)++;       \
            (ega)->encod_type  = *(cp)++;       \
            (ega)->reserved    = *(cp)++;       \
            (ega)->masklen     = *(cp)++;       \
            GET_NETLONG((ega)->mcast_addr, (cp)); \
        } while(0)

#define GET_EGADDR6(ega, cp) /* XXX: hard coding */	\
        do {                                    \
            (ega)->addr_family = *(cp)++;       \
            (ega)->encod_type  = *(cp)++;       \
            (ega)->reserved    = *(cp)++;       \
            (ega)->masklen     = *(cp)++;       \
             memcpy(&(ega)->mcast_addr, (cp), sizeof(struct in6_addr)); \
	    (cp) += sizeof(struct in6_addr);	\
        } while(0)

#define PUT_EGADDR(addr, masklen, reserved, cp) \
        do {                                    \
            u_int32 mask;                       \
            MASKLEN_TO_MASK((masklen), mask);   \
            *(cp)++ = ADDRF_IPv4; /* family */  \
            *(cp)++ = ADDRT_IPv4; /* type   */  \
            *(cp)++ = (reserved); /* reserved; should be 0 */  \
            *(cp)++ = (masklen);                \
            PUT_NETLONG((addr) & mask, (cp)); \
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

#define GET_EUADDR(eua, cp)                     \
        do {                                    \
            (eua)->addr_family = *(cp)++;       \
            (eua)->encod_type  = *(cp)++;       \
            GET_NETLONG((eua)->unicast_addr, (cp)); \
        } while(0)

#define GET_EUADDR6(eua, cp)  /* XXX hard conding */	\
        do {                                    \
	     (eua)->addr_family = *(cp)++;	\
	     (eua)->encod_type  = *(cp)++;	\
             memcpy(&(eua)->unicast_addr, (cp), sizeof(struct in6_addr)); \
	     (cp) += sizeof(struct in6_addr);	\
        } while(0)

#define PUT_EUADDR(addr, cp)                    \
        do {                                    \
            *(cp)++ = ADDRF_IPv4; /* family */  \
            *(cp)++ = ADDRT_IPv4; /* type   */  \
            PUT_NETLONG((addr), (cp));          \
        } while(0)

#define PUT_EUADDR6(addr, cp) \
        do {                                    \
            *(cp)++ = ADDRF_IPv6; /* family */  \
            *(cp)++ = ADDRT_IPv6; /* type   */  \
            memcpy((cp), &(addr), sizeof(struct in6_addr)); \
            (cp) += sizeof(struct in6_addr); \
        } while(0)

/* TODO: Currently not used. Probably not need at all. Delete! */
#ifdef NOSUCHDEF
/* This is completely IGMP related stuff? */
#define PIM_LEAF_TIMEOUT               (3.5 * IGMP_QUERY_INTERVAL)
#endif /* NOSUCHDEF */

#if defined(__bsdi__) || defined(__NetBSD__)
/*
 * Struct used to communicate from kernel to multicast router
 * note the convenient similarity to an IP packet
 */ 
struct igmpmsg {
    u_long          unused1;
    u_long          unused2;
    u_char          im_msgtype;                 /* what type of message     */
#define IGMPMSG_NOCACHE         1
#define IGMPMSG_WRONGVIF        2
#define IGMPMSG_WHOLEPKT        3               /* used for user level encap*/
    u_char          im_mbz;                     /* must be zero             */
    u_char          im_vif;                     /* vif rec'd on             */
    u_char          unused3;
    struct in_addr  im_src, im_dst;
};
#endif
