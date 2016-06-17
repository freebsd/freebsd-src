/*
 *	NET3:	Support for 802.2 demultiplexing off Ethernet (Token ring
 *		is kept separate see p8022tr.c)
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *		Demultiplex 802.2 encoded protocols. We match the entry by the
 *		SSAP/DSAP pair and then deliver to the registered datalink that
 *		matches. The control byte is ignored and handling of such items
 *		is up to the routine passed the frame.
 *
 *		Unlike the 802.3 datalink we have a list of 802.2 entries as there
 *		are multiple protocols to demux. The list is currently short (3 or
 *		4 entries at most). The current demux assumes this.
 */

#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/datalink.h>
#include <linux/mm.h>
#include <linux/in.h>
#include <linux/init.h>
#include <net/p8022.h>

static struct datalink_proto *p8022_list = NULL;

/*
 *	We don't handle the loopback SAP stuff, the extended
 *	802.2 command set, multicast SAP identifiers and non UI
 *	frames. We have the absolute minimum needed for IPX,
 *	IP and Appletalk phase 2. See the llc_* routines for
 *	support libraries if your protocol needs these.
 */

static struct datalink_proto *find_8022_client(unsigned char type)
{
	struct datalink_proto	*proto;

	for (proto = p8022_list;
		((proto != NULL) && (*(proto->type) != type));
		proto = proto->next)
		;

	return proto;
}

int p8022_rcv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt)
{
	struct datalink_proto	*proto;

	proto = find_8022_client(*(skb->h.raw));
	if (proto != NULL) 
	{
		skb->h.raw += 3;
		skb->nh.raw += 3;
		skb_pull(skb,3);
		return proto->rcvfunc(skb, dev, pt);
	}

	skb->sk = NULL;
	kfree_skb(skb);
	return 0;
}

static void p8022_datalink_header(struct datalink_proto *dl,
		struct sk_buff *skb, unsigned char *dest_node)
{
	struct net_device	*dev = skb->dev;
	unsigned char	*rawp;

	rawp = skb_push(skb,3);
	*rawp++ = dl->type[0];
	*rawp++ = dl->type[0];
	*rawp = 0x03;	/* UI */
	dev->hard_header(skb, dev, ETH_P_802_3, dest_node, NULL, skb->len);
}

static struct packet_type p8022_packet_type =
{
	0,	/* MUTTER ntohs(ETH_P_8022),*/
	NULL,		/* All devices */
	p8022_rcv,
	NULL,
	NULL,
};

EXPORT_SYMBOL(register_8022_client);
EXPORT_SYMBOL(unregister_8022_client);

static int __init p8022_init(void)
{
	p8022_packet_type.type=htons(ETH_P_802_2);
	dev_add_pack(&p8022_packet_type);
	return 0;
}

module_init(p8022_init);

struct datalink_proto *register_8022_client(unsigned char type, int (*rcvfunc)(struct sk_buff *, struct net_device *, struct packet_type *))
{
	struct datalink_proto	*proto;

	if (find_8022_client(type) != NULL)
		return NULL;

	proto = (struct datalink_proto *) kmalloc(sizeof(*proto), GFP_ATOMIC);
	if (proto != NULL) {
		proto->type[0] = type;
		proto->type_len = 1;
		proto->rcvfunc = rcvfunc;
		proto->header_length = 3;
		proto->datalink_header = p8022_datalink_header;
		proto->string_name = "802.2";
		proto->next = p8022_list;
		p8022_list = proto;
	}

	return proto;
}

void unregister_8022_client(unsigned char type)
{
	struct datalink_proto *tmp, **clients = &p8022_list;
	unsigned long flags;

	save_flags(flags);
	cli();

	while ((tmp = *clients) != NULL)
	{
		if (tmp->type[0] == type) {
			*clients = tmp->next;
			kfree(tmp);
			break;
		} else {
			clients = &tmp->next;
		}
	}

	restore_flags(flags);
}
