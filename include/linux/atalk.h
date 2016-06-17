/*
 *	AppleTalk networking structures
 *
 *	The following are directly referenced from the University Of Michigan
 *	netatalk for compatibility reasons.
 */

#ifndef __LINUX_ATALK_H__
#define __LINUX_ATALK_H__

#define ATPORT_FIRST	1
#define ATPORT_RESERVED	128
#define ATPORT_LAST	254 /* 254 is only legal on localtalk */ 
#define ATADDR_ANYNET	(__u16)0
#define ATADDR_ANYNODE	(__u8)0
#define ATADDR_ANYPORT  (__u8)0
#define ATADDR_BCAST	(__u8)255
#define DDP_MAXSZ	587
#define DDP_MAXHOPS     15      /* 4 bits of hop counter */

#define SIOCATALKDIFADDR       (SIOCPROTOPRIVATE + 0)

struct at_addr 
{
	__u16	s_net;
	__u8	s_node;
};

struct sockaddr_at 
{
	sa_family_t	sat_family;
	__u8		sat_port;
	struct at_addr	sat_addr;
	char		sat_zero[ 8 ];
};

struct netrange 
{
	__u8	nr_phase;
	__u16	nr_firstnet;
	__u16	nr_lastnet;
};

struct atalk_route
{
	struct net_device *dev;
	struct at_addr target;
	struct at_addr gateway;
	int flags;
	struct atalk_route *next;
};

struct atalk_iface
{
	struct net_device *dev;
	struct at_addr address;		/* Our address */
	int status;			/* What are we doing? */
#define ATIF_PROBE	1		/* Probing for an address */
#define ATIF_PROBE_FAIL	2		/* Probe collided */
	struct netrange nets;		/* Associated direct netrange */
	struct atalk_iface *next;
};
	
struct atalk_sock
{
	unsigned short dest_net;
	unsigned short src_net;
	unsigned char dest_node;
	unsigned char src_node;
	unsigned char dest_port;
	unsigned char src_port;
};

#ifdef __KERNEL__

#include <asm/byteorder.h>

struct ddpehdr
{
#ifdef __LITTLE_ENDIAN_BITFIELD
	__u16	deh_len:10, deh_hops:4, deh_pad:2;
#else
	__u16	deh_pad:2, deh_hops:4, deh_len:10;
#endif
	__u16	deh_sum;
	__u16	deh_dnet;
	__u16	deh_snet;
	__u8	deh_dnode;
	__u8	deh_snode;
	__u8	deh_dport;
	__u8	deh_sport;
	/* And netatalk apps expect to stick the type in themselves */
};

/*
 *	Don't drop the struct into the struct above.  You'll get some
 *	surprise padding.
 */
 
struct ddpebits
{
#ifdef __LITTLE_ENDIAN_BITFIELD
	__u16	deh_len:10, deh_hops:4, deh_pad:2;
#else
	__u16	deh_pad:2, deh_hops:4, deh_len:10;
#endif
};

/*
 *	Short form header
 */
 
struct ddpshdr
{
#ifdef __LITTLE_ENDIAN_BITFIELD
	__u16	dsh_len:10, dsh_pad:6;
#else
	__u16	dsh_pad:6, dsh_len:10;
#endif
	__u8	dsh_dport;
	__u8	dsh_sport;
	/* And netatalk apps expect to stick the type in themselves */
};

/* AppleTalk AARP headers */

struct elapaarp
{
	__u16	hw_type;
#define AARP_HW_TYPE_ETHERNET		1
#define AARP_HW_TYPE_TOKENRING		2
	__u16	pa_type;
	__u8	hw_len;
	__u8	pa_len;
#define AARP_PA_ALEN			4
	__u16	function;
#define AARP_REQUEST			1
#define AARP_REPLY			2
#define AARP_PROBE			3
	__u8	hw_src[ETH_ALEN]	__attribute__ ((packed));
	__u8	pa_src_zero		__attribute__ ((packed));
	__u16	pa_src_net		__attribute__ ((packed));
	__u8	pa_src_node		__attribute__ ((packed));
	__u8	hw_dst[ETH_ALEN]	__attribute__ ((packed));
	__u8	pa_dst_zero		__attribute__ ((packed));
	__u16	pa_dst_net		__attribute__ ((packed));
	__u8	pa_dst_node		__attribute__ ((packed));	
};

#define AARP_EXPIRY_TIME	(5*60*HZ)	/* Not specified - how long till we drop a resolved entry */
#define AARP_HASH_SIZE		16		/* Size of hash table */
#define AARP_TICK_TIME		(HZ/5)		/* Fast retransmission timer when resolving */
#define AARP_RETRANSMIT_LIMIT	10		/* Send 10 requests then give up (2 seconds) */
#define AARP_RESOLVE_TIME	(10*HZ)		/* Some value bigger than total retransmit time + a bit for last reply to appear and to stop continual requests */

extern struct datalink_proto *ddp_dl, *aarp_dl;
extern void aarp_proto_init(void);
/* Inter module exports */

/*
 *	Give a device find its atif control structure
 */

static inline struct atalk_iface *atalk_find_dev(struct net_device *dev)
{
	return dev->atalk_ptr;
}

extern struct at_addr *atalk_find_dev_addr(struct net_device *dev);
extern struct net_device *atrtr_get_dev(struct at_addr *sa);
extern int aarp_send_ddp(struct net_device *dev,struct sk_buff *skb, struct at_addr *sa, void *hwaddr);
extern void aarp_send_probe(struct net_device *dev, struct at_addr *addr);
extern void aarp_device_down(struct net_device *dev);

#ifdef MODULE
extern void aarp_cleanup_module(void);
#endif /* MODULE */

#endif /* __KERNEL__ */
#endif /* __LINUX_ATALK_H__ */
