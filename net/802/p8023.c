/*
 *	NET3:	802.3 data link hooks used for IPX 802.3
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *	802.3 isn't really a protocol data link layer. Some old IPX stuff
 *	uses it however. Note that there is only one 802.3 protocol layer
 *	in the system. We don't currently support different protocols
 *	running raw 802.3 on different devices. Thankfully nobody else
 *	has done anything like the old IPX.
 */
 
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/datalink.h>
#include <linux/mm.h>
#include <linux/in.h>

/*
 *	Place an 802.3 header on a packet. The driver will do the mac
 *	addresses, we just need to give it the buffer length.
 */
 
static void p8023_datalink_header(struct datalink_proto *dl, 
		struct sk_buff *skb, unsigned char *dest_node)
{
	struct net_device	*dev = skb->dev;
	dev->hard_header(skb, dev, ETH_P_802_3, dest_node, NULL, skb->len);
}

/*
 *	Create an 802.3 client. Note there can be only one 802.3 client
 */
 
struct datalink_proto *make_8023_client(void)
{
	struct datalink_proto	*proto;

	proto = (struct datalink_proto *) kmalloc(sizeof(*proto), GFP_ATOMIC);
	if (proto != NULL) 
	{
		proto->type_len = 0;
		proto->header_length = 0;
		proto->datalink_header = p8023_datalink_header;
		proto->string_name = "802.3";
	}
	return proto;
}

/*
 *	Destroy the 802.3 client.
 */
 
void destroy_8023_client(struct datalink_proto *dl)
{
	if (dl)
		kfree(dl);
}

