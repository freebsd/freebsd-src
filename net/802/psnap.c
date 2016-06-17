/*
 *	SNAP data link layer. Derived from 802.2
 *
 *		Alan Cox <Alan.Cox@linux.org>, from the 802.2 layer by Greg Page.
 *		Merged in additions from Greg Page's psnap.c.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/datalink.h>
#include <net/p8022.h>
#include <net/psnap.h>
#include <linux/mm.h>
#include <linux/in.h>
#include <linux/init.h>

static struct datalink_proto *snap_list = NULL;
static struct datalink_proto *snap_dl = NULL;		/* 802.2 DL for SNAP */

/*
 *	Find a snap client by matching the 5 bytes.
 */

static struct datalink_proto *find_snap_client(unsigned char *desc)
{
	struct datalink_proto	*proto;

	for (proto = snap_list; proto != NULL && memcmp(proto->type, desc, 5) ; proto = proto->next);
	return proto;
}

/*
 *	A SNAP packet has arrived
 */

int snap_rcv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt)
{
	static struct packet_type psnap_packet_type =
	{
		0,
		NULL,		/* All Devices */
		snap_rcv,
		NULL,
		NULL,
	};

	struct datalink_proto	*proto;

	proto = find_snap_client(skb->h.raw);
	if (proto != NULL)
	{
		/*
		 *	Pass the frame on.
		 */

		skb->h.raw += 5;
		skb->nh.raw += 5;
		skb_pull(skb,5);
		if (psnap_packet_type.type == 0)
			psnap_packet_type.type=htons(ETH_P_SNAP);

		return proto->rcvfunc(skb, dev, &psnap_packet_type);
	}
	skb->sk = NULL;
	kfree_skb(skb);
	return 0;
}

/*
 *	Put a SNAP header on a frame and pass to 802.2
 */

static void snap_datalink_header(struct datalink_proto *dl, struct sk_buff *skb, unsigned char *dest_node)
{
	memcpy(skb_push(skb,5),dl->type,5);
	snap_dl->datalink_header(snap_dl, skb, dest_node);
}

/*
 *	Set up the SNAP layer
 */

EXPORT_SYMBOL(register_snap_client);
EXPORT_SYMBOL(unregister_snap_client);

static int __init snap_init(void)
{
	snap_dl=register_8022_client(0xAA, snap_rcv);
	if(snap_dl==NULL)
		printk("SNAP - unable to register with 802.2\n");
	return 0;
}
module_init(snap_init);

/*
 *	Register SNAP clients. We don't yet use this for IP.
 */

struct datalink_proto *register_snap_client(unsigned char *desc, int (*rcvfunc)(struct sk_buff *, struct net_device *, struct packet_type *))
{
	struct datalink_proto	*proto;

	if (find_snap_client(desc) != NULL)
		return NULL;

	proto = (struct datalink_proto *) kmalloc(sizeof(*proto), GFP_ATOMIC);
	if (proto != NULL)
	{
		memcpy(proto->type, desc,5);
		proto->type_len = 5;
		proto->rcvfunc = rcvfunc;
		proto->header_length = 5+snap_dl->header_length;
		proto->datalink_header = snap_datalink_header;
		proto->string_name = "SNAP";
		proto->next = snap_list;
		snap_list = proto;
	}

	return proto;
}

/*
 *	Unregister SNAP clients. Protocols no longer want to play with us ...
 */

void unregister_snap_client(unsigned char *desc)
{
	struct datalink_proto **clients = &snap_list;
	struct datalink_proto *tmp;
	unsigned long flags;

	save_flags(flags);
	cli();

	while ((tmp = *clients) != NULL)
	{
		if (memcmp(tmp->type,desc,5) == 0)
		{
			*clients = tmp->next;
			kfree(tmp);
			break;
		}
		else
			clients = &tmp->next;
	}

	restore_flags(flags);
}
