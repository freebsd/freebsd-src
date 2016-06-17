/*
 *	ROSE release 003
 *
 *	This code REQUIRES 2.1.15 or higher/ NET3.038
 *
 *	This module:
 *		This module is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *	History
 *	ROSE 001	Jonathan(G4KLX)	Cloned from nr_dev.c.
 *			Hans(PE1AYX)	Fixed interface to IP layer.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/sysctl.h>
#include <linux/string.h>
#include <linux/socket.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/in.h>
#include <linux/if_ether.h>	/* For the statistics structure. */

#include <asm/system.h>
#include <asm/segment.h>
#include <asm/io.h>

#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_arp.h>
#include <linux/skbuff.h>

#include <net/ip.h>
#include <net/arp.h>

#include <net/ax25.h>
#include <net/rose.h>

/*
 *	Only allow IP over ROSE frames through if the netrom device is up.
 */

int rose_rx_ip(struct sk_buff *skb, struct net_device *dev)
{
	struct net_device_stats *stats = (struct net_device_stats *)dev->priv;

#ifdef CONFIG_INET
	if (!netif_running(dev)) {
		stats->rx_errors++;
		return 0;
	}

	stats->rx_packets++;
	stats->rx_bytes += skb->len;

	skb->protocol = htons(ETH_P_IP);

	/* Spoof incoming device */
	skb->dev      = dev;
	skb->h.raw    = skb->data;
	skb->nh.raw   = skb->data;
	skb->pkt_type = PACKET_HOST;

	ip_rcv(skb, skb->dev, NULL);
#else
	kfree_skb(skb);
#endif
	return 1;
}

static int rose_header(struct sk_buff *skb, struct net_device *dev, unsigned short type,
	void *daddr, void *saddr, unsigned len)
{
	unsigned char *buff = skb_push(skb, ROSE_MIN_LEN + 2);

	*buff++ = ROSE_GFI | ROSE_Q_BIT;
	*buff++ = 0x00;
	*buff++ = ROSE_DATA;
	*buff++ = 0x7F;
	*buff++ = AX25_P_IP;

	if (daddr != NULL)
		return 37;

	return -37;
}

static int rose_rebuild_header(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;
	struct net_device_stats *stats = (struct net_device_stats *)dev->priv;
	unsigned char *bp = (unsigned char *)skb->data;
	struct sk_buff *skbn;

#ifdef CONFIG_INET
	if (arp_find(bp + 7, skb)) {
		return 1;
	}

	if ((skbn = skb_clone(skb, GFP_ATOMIC)) == NULL) {
		kfree_skb(skb);
		return 1;
	}

	if (skb->sk != NULL)
		skb_set_owner_w(skbn, skb->sk);

	kfree_skb(skb);

	if (!rose_route_frame(skbn, NULL)) {
		kfree_skb(skbn);
		stats->tx_errors++;
		return 1;
	}

	stats->tx_packets++;
	stats->tx_bytes += skbn->len;
#endif
	return 1;
}

static int rose_set_mac_address(struct net_device *dev, void *addr)
{
	struct sockaddr *sa = addr;

	rose_del_loopback_node((rose_address *)dev->dev_addr);

	memcpy(dev->dev_addr, sa->sa_data, dev->addr_len);

	rose_add_loopback_node((rose_address *)dev->dev_addr);

	return 0;
}

static int rose_open(struct net_device *dev)
{
	MOD_INC_USE_COUNT;
	netif_start_queue(dev);
	rose_add_loopback_node((rose_address *)dev->dev_addr);
	return 0;
}

static int rose_close(struct net_device *dev)
{
	netif_stop_queue(dev);
	rose_del_loopback_node((rose_address *)dev->dev_addr);
	MOD_DEC_USE_COUNT;
	return 0;
}

static int rose_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct net_device_stats *stats = (struct net_device_stats *)dev->priv;

	if (!netif_running(dev)) {
		printk(KERN_ERR "ROSE: rose_xmit - called when iface is down\n");
		return 1;
	}
	dev_kfree_skb(skb);
	stats->tx_errors++;
	return 0;
}

static struct net_device_stats *rose_get_stats(struct net_device *dev)
{
	return (struct net_device_stats *)dev->priv;
}

int rose_init(struct net_device *dev)
{
	dev->mtu		= ROSE_MAX_PACKET_SIZE - 2;
	dev->hard_start_xmit	= rose_xmit;
	dev->open		= rose_open;
	dev->stop		= rose_close;

	dev->hard_header	= rose_header;
	dev->hard_header_len	= AX25_BPQ_HEADER_LEN + AX25_MAX_HEADER_LEN + ROSE_MIN_LEN;
	dev->addr_len		= ROSE_ADDR_LEN;
	dev->type		= ARPHRD_ROSE;
	dev->rebuild_header	= rose_rebuild_header;
	dev->set_mac_address    = rose_set_mac_address;

	/* New-style flags. */
	dev->flags		= 0;

	if ((dev->priv = kmalloc(sizeof(struct net_device_stats), GFP_KERNEL)) == NULL)
		return -ENOMEM;

	memset(dev->priv, 0, sizeof(struct net_device_stats));

	dev->get_stats = rose_get_stats;

	return 0;
};
