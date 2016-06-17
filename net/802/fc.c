/*
 * NET3:	Fibre Channel device handling subroutines
 * 
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *		Vineet Abraham <vma@iol.unh.edu>
 *		v 1.0 03/22/99
 */

#include <linux/config.h>
#include <linux/module.h>

#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/fcdevice.h>
#include <linux/skbuff.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/net.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <net/arp.h>

/*
 *	Put the headers on a Fibre Channel packet. 
 */
 
int fc_header(struct sk_buff *skb, struct net_device *dev, unsigned short type,
              void *daddr, void *saddr, unsigned len) 
{
	struct fch_hdr *fch;
	int hdr_len;

	/* 
	 * Add the 802.2 SNAP header if IP as the IPv4 code calls  
	 * dev->hard_header directly.
	 */
	if (type == ETH_P_IP || type == ETH_P_ARP)
	{
		struct fcllc *fcllc=(struct fcllc *)(fch+1);

		hdr_len = sizeof(struct fch_hdr) + sizeof(struct fcllc);
		fch = (struct fch_hdr *)skb_push(skb, hdr_len);
		fcllc = (struct fcllc *)(fch+1);
		fcllc->dsap = fcllc->ssap = EXTENDED_SAP;
		fcllc->llc = UI_CMD;
		fcllc->protid[0] = fcllc->protid[1] = fcllc->protid[2] = 0x00;
		fcllc->ethertype = htons(type);
	}
	else
	{
		hdr_len = sizeof(struct fch_hdr);
		fch = (struct fch_hdr *)skb_push(skb, hdr_len);	
	}

	if(saddr)
		memcpy(fch->saddr,saddr,dev->addr_len);
	else
		memcpy(fch->saddr,dev->dev_addr,dev->addr_len);

	if(daddr) 
	{
		memcpy(fch->daddr,daddr,dev->addr_len);
		return(hdr_len);
	}
	return -hdr_len;
}
	
/*
 *	A neighbour discovery of some species (eg arp) has completed. We
 *	can now send the packet.
 */
 
int fc_rebuild_header(struct sk_buff *skb) 
{
	struct fch_hdr *fch=(struct fch_hdr *)skb->data;
	struct fcllc *fcllc=(struct fcllc *)(skb->data+sizeof(struct fch_hdr));
	if(fcllc->ethertype != htons(ETH_P_IP)) {
		printk("fc_rebuild_header: Don't know how to resolve type %04X addresses ?\n",(unsigned int)htons(fcllc->ethertype));
		return 0;
	}
#ifdef CONFIG_INET
	return arp_find(fch->daddr, skb);
#else
	return 0;
#endif
}

EXPORT_SYMBOL(fc_type_trans);

unsigned short
fc_type_trans(struct sk_buff *skb, struct net_device *dev)
{
	struct fch_hdr *fch = (struct fch_hdr *)skb->data;
	struct fcllc *fcllc;

	skb->mac.raw = skb->data;
	fcllc = (struct fcllc *)(skb->data + sizeof (struct fch_hdr) + 2);
	skb_pull(skb, sizeof (struct fch_hdr) + 2);

	if (*fch->daddr & 1) {
		if (!memcmp(fch->daddr, dev->broadcast, FC_ALEN))
			skb->pkt_type = PACKET_BROADCAST;
		else
			skb->pkt_type = PACKET_MULTICAST;
	} else if (dev->flags & IFF_PROMISC) {
		if (memcmp(fch->daddr, dev->dev_addr, FC_ALEN))
			skb->pkt_type = PACKET_OTHERHOST;
	}

	/*
	 * Strip the SNAP header from ARP packets since we don't pass
	 * them through to the 802.2/SNAP layers.
	 */
	if (fcllc->dsap == EXTENDED_SAP &&
	    (fcllc->ethertype == ntohs(ETH_P_IP) ||
	     fcllc->ethertype == ntohs(ETH_P_ARP))) {
		skb_pull(skb, sizeof (struct fcllc));
		return fcllc->ethertype;
	}

	return ntohs(ETH_P_802_2);
}
