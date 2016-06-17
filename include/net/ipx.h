/*
 *	The following information is in its entirety obtained from:
 *
 *	Novell 'IPX Router Specification' Version 1.10 
 *		Part No. 107-000029-001
 *
 *	Which is available from ftp.novell.com
 */

#ifndef _NET_INET_IPX_H_
#define _NET_INET_IPX_H_

#include <linux/netdevice.h>
#include <net/datalink.h>
#include <linux/ipx.h>

typedef struct
{
	__u32   net;
	__u8    node[IPX_NODE_LEN]; 
	__u16   sock;
} ipx_address;

#define ipx_broadcast_node	"\377\377\377\377\377\377"
#define ipx_this_node           "\0\0\0\0\0\0"

#define IPX_MAX_PPROP_HOPS 8

struct ipxhdr
{
	__u16           ipx_checksum __attribute__ ((packed));
#define IPX_NO_CHECKSUM	0xFFFF
	__u16           ipx_pktsize __attribute__ ((packed));
	__u8            ipx_tctrl;
	__u8            ipx_type;
#define IPX_TYPE_UNKNOWN	0x00
#define IPX_TYPE_RIP		0x01	/* may also be 0 */
#define IPX_TYPE_SAP		0x04	/* may also be 0 */
#define IPX_TYPE_SPX		0x05	/* SPX protocol */
#define IPX_TYPE_NCP		0x11	/* $lots for docs on this (SPIT) */
#define IPX_TYPE_PPROP		0x14	/* complicated flood fill brdcast */
	ipx_address	ipx_dest __attribute__ ((packed));
	ipx_address	ipx_source __attribute__ ((packed));
};

typedef struct ipx_interface {
	/* IPX address */
	__u32           if_netnum;
	unsigned char	if_node[IPX_NODE_LEN];
	atomic_t        refcnt;

	/* physical device info */
	struct net_device	*if_dev;
	struct datalink_proto	*if_dlink;
	unsigned short	if_dlink_type;

	/* socket support */
	unsigned short	if_sknum;
	struct sock	*if_sklist;
	spinlock_t      if_sklist_lock;

	/* administrative overhead */
	int		if_ipx_offset;
	unsigned char	if_internal;
	unsigned char	if_primary;
	
	struct ipx_interface	*if_next;
}	ipx_interface;

typedef struct ipx_route {
	__u32         ir_net;
	ipx_interface *ir_intrfc;
	unsigned char ir_routed;
	unsigned char ir_router_node[IPX_NODE_LEN];
	struct ipx_route *ir_next;
	atomic_t      refcnt;
}	ipx_route;

#ifdef __KERNEL__
struct ipx_cb {
	u8 ipx_tctrl;
	u32 ipx_dest_net;
	u32 ipx_source_net;
	struct {
		u32 netnum;
		int index;
	} last_hop;
};
#endif
#define IPX_MIN_EPHEMERAL_SOCKET	0x4000
#define IPX_MAX_EPHEMERAL_SOCKET	0x7fff

extern int ipx_register_spx(struct proto_ops **, struct net_proto_family *);
extern int ipx_unregister_spx(void);

#endif /* def _NET_INET_IPX_H_ */
