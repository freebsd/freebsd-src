/*
 *	G8BPQ compatible "AX.25 via ethernet" driver release 004
 *
 *	This code REQUIRES 2.0.0 or higher/ NET3.029
 *
 *	This module:
 *		This module is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *	This is a "pseudo" network driver to allow AX.25 over Ethernet
 *	using G8BPQ encapsulation. It has been extracted from the protocol
 *	implementation because
 *
 *		- things got unreadable within the protocol stack
 *		- to cure the protocol stack from "feature-ism"
 *		- a protocol implementation shouldn't need to know on
 *		  which hardware it is running
 *		- user-level programs like the AX.25 utilities shouldn't
 *		  need to know about the hardware.
 *		- IP over ethernet encapsulated AX.25 was impossible
 *		- rxecho.c did not work
 *		- to have room for extensions
 *		- it just deserves to "live" as an own driver
 *
 *	This driver can use any ethernet destination address, and can be
 *	limited to accept frames from one dedicated ethernet card only.
 *
 *	Note that the driver sets up the BPQ devices automagically on
 *	startup or (if started before the "insmod" of an ethernet device)
 *	on "ifconfig up". It hopefully will remove the BPQ on "rmmod"ing
 *	the ethernet device (in fact: as soon as another ethernet or bpq
 *	device gets "ifconfig"ured).
 *
 *	I have heard that several people are thinking of experiments
 *	with highspeed packet radio using existing ethernet cards.
 *	Well, this driver is prepared for this purpose, just add
 *	your tx key control and a txdelay / tailtime algorithm,
 *	probably some buffering, and /voila/...
 *
 *	History
 *	BPQ   001	Joerg(DL1BKE)		Extracted BPQ code from AX.25
 *						protocol stack and added my own
 *						yet existing patches
 *	BPQ   002	Joerg(DL1BKE)		Scan network device list on
 *						startup.
 *	BPQ   003	Joerg(DL1BKE)		Ethernet destination address
 *						and accepted source address
 *						can be configured by an ioctl()
 *						call.
 *						Fixed to match Linux networking
 *						changes - 2.1.15.
 *	BPQ   004	Joerg(DL1BKE)		Fixed to not lock up on ifconfig.
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/net.h>
#include <net/ax25.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <asm/segment.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/netfilter.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/rtnetlink.h>

#include <net/ip.h>
#include <net/arp.h>

#include <linux/bpqether.h>

static char banner[] __initdata = KERN_INFO "AX.25: bpqether driver version 004\n";

static unsigned char ax25_bcast[AX25_ADDR_LEN] =
	{'Q' << 1, 'S' << 1, 'T' << 1, ' ' << 1, ' ' << 1, ' ' << 1, '0' << 1};
static unsigned char ax25_defaddr[AX25_ADDR_LEN] =
	{'L' << 1, 'I' << 1, 'N' << 1, 'U' << 1, 'X' << 1, ' ' << 1, '1' << 1};

static char bcast_addr[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

static char bpq_eth_addr[6];

static int bpq_rcv(struct sk_buff *, struct net_device *, struct packet_type *);
static int bpq_device_event(struct notifier_block *, unsigned long, void *);
static char *bpq_print_ethaddr(unsigned char *);

static struct packet_type bpq_packet_type = {
	type:	__constant_htons(ETH_P_BPQ),
	func:	bpq_rcv,
};

static struct notifier_block bpq_dev_notifier = {
	notifier_call:	bpq_device_event,
};


#define MAXBPQDEV 100

static struct bpqdev {
	struct bpqdev *next;
	char   ethname[14];		/* ether device name */
	struct net_device *ethdev;		/* link to ethernet device */
	struct net_device axdev;		/* bpq device (bpq#) */
	struct net_device_stats stats;	/* some statistics */
	char   dest_addr[6];		/* ether destination address */
	char   acpt_addr[6];		/* accept ether frames from this address only */
} *bpq_devices;


/* ------------------------------------------------------------------------ */


/*
 *	Get the ethernet device for a BPQ device
 */
static inline struct net_device *bpq_get_ether_dev(struct net_device *dev)
{
	struct bpqdev *bpq = (struct bpqdev *) dev->priv;

	return bpq ? bpq->ethdev : NULL;
}

/*
 *	Get the BPQ device for the ethernet device
 */
static inline struct net_device *bpq_get_ax25_dev(struct net_device *dev)
{
	struct bpqdev *bpq;

	for (bpq = bpq_devices; bpq != NULL; bpq = bpq->next)
		if (bpq->ethdev == dev)
			return &bpq->axdev;

	return NULL;
}

static inline int dev_is_ethdev(struct net_device *dev)
{
	return (
			dev->type == ARPHRD_ETHER
			&& strncmp(dev->name, "dummy", 5)
	);
}

/*
 *	Sanity check: remove all devices that ceased to exists and
 *	return '1' if the given BPQ device was affected.
 */
static int bpq_check_devices(struct net_device *dev)
{
	struct bpqdev *bpq, *bpq_prev, *bpq_next;
	int result = 0;
	unsigned long flags;

	save_flags(flags);
	cli();

	bpq_prev = NULL;

	for (bpq = bpq_devices; bpq != NULL; bpq = bpq_next) {
		bpq_next = bpq->next;
		if (!dev_get(bpq->ethname)) {
			if (bpq_prev)
				bpq_prev->next = bpq->next;
			else
				bpq_devices = bpq->next;

			if (&bpq->axdev == dev)
				result = 1;

			/* We should be locked, call 
			 * unregister_netdevice directly 
			 */

			unregister_netdevice(&bpq->axdev);
			kfree(bpq);
		}
		else
			bpq_prev = bpq;
	}

	restore_flags(flags);

	return result;
}


/* ------------------------------------------------------------------------ */


/*
 *	Receive an AX.25 frame via an ethernet interface.
 */
static int bpq_rcv(struct sk_buff *skb, struct net_device *dev, struct packet_type *ptype)
{
	int len;
	char * ptr;
	struct ethhdr *eth = (struct ethhdr *)skb->mac.raw;
	struct bpqdev *bpq;

	skb->sk = NULL;		/* Initially we don't know who it's for */

	dev = bpq_get_ax25_dev(dev);

	if (dev == NULL || !netif_running(dev)) {
		kfree_skb(skb);
		return 0;
	}

	/*
	 * if we want to accept frames from just one ethernet device
	 * we check the source address of the sender.
	 */

	bpq = (struct bpqdev *)dev->priv;

	if (!(bpq->acpt_addr[0] & 0x01) && memcmp(eth->h_source, bpq->acpt_addr, ETH_ALEN)) {
		printk(KERN_DEBUG "bpqether: wrong dest %s\n", bpq_print_ethaddr(eth->h_source));
		kfree_skb(skb);
		return 0;
	}

	len = skb->data[0] + skb->data[1] * 256 - 5;

	skb_pull(skb, 2);	/* Remove the length bytes */
	skb_trim(skb, len);	/* Set the length of the data */

	bpq->stats.rx_packets++;
	bpq->stats.rx_bytes += len;

	ptr = skb_push(skb, 1);
	*ptr = 0;

	skb->dev = dev;
	skb->protocol = htons(ETH_P_AX25);
	skb->mac.raw = skb->data;
	skb->pkt_type = PACKET_HOST;

	netif_rx(skb);

	return 0;
}

/*
 * 	Send an AX.25 frame via an ethernet interface
 */
static int bpq_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct sk_buff *newskb;
	unsigned char *ptr;
	struct bpqdev *bpq;
	int size;

	/*
	 * Just to be *really* sure not to send anything if the interface
	 * is down, the ethernet device may have gone.
	 */
	if (!netif_running(dev)) {
		bpq_check_devices(dev);
		kfree_skb(skb);
		return -ENODEV;
	}

	skb_pull(skb, 1);
	size = skb->len;

	/*
	 * The AX.25 code leaves enough room for the ethernet header, but
	 * sendto() does not.
	 */
	if (skb_headroom(skb) < AX25_BPQ_HEADER_LEN) {	/* Ough! */
		if ((newskb = skb_realloc_headroom(skb, AX25_BPQ_HEADER_LEN)) == NULL) {
			printk(KERN_WARNING "bpqether: out of memory\n");
			kfree_skb(skb);
			return -ENOMEM;
		}

		if (skb->sk != NULL)
			skb_set_owner_w(newskb, skb->sk);

		kfree_skb(skb);
		skb = newskb;
	}

	skb->protocol = htons(ETH_P_AX25);

	ptr = skb_push(skb, 2);

	*ptr++ = (size + 5) % 256;
	*ptr++ = (size + 5) / 256;

	bpq = (struct bpqdev *)dev->priv;

	if ((dev = bpq_get_ether_dev(dev)) == NULL) {
		bpq->stats.tx_dropped++;
		kfree_skb(skb);
		return -ENODEV;
	}

	skb->dev = dev;
	skb->nh.raw = skb->data;
	dev->hard_header(skb, dev, ETH_P_BPQ, bpq->dest_addr, NULL, 0);
	bpq->stats.tx_packets++;
	bpq->stats.tx_bytes+=skb->len;
  
	dev_queue_xmit(skb);
	netif_wake_queue(dev);
	return 0;
}

/*
 *	Statistics
 */
static struct net_device_stats *bpq_get_stats(struct net_device *dev)
{
	struct bpqdev *bpq = (struct bpqdev *) dev->priv;

	return &bpq->stats;
}

/*
 *	Set AX.25 callsign
 */
static int bpq_set_mac_address(struct net_device *dev, void *addr)
{
    struct sockaddr *sa = (struct sockaddr *)addr;

    memcpy(dev->dev_addr, sa->sa_data, dev->addr_len);

    return 0;
}

/*	Ioctl commands
 *
 *		SIOCSBPQETHOPT		reserved for enhancements
 *		SIOCSBPQETHADDR		set the destination and accepted
 *					source ethernet address (broadcast
 *					or multicast: accept all)
 */
static int bpq_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct bpq_ethaddr *ethaddr = (struct bpq_ethaddr *)ifr->ifr_data;
	struct bpqdev *bpq = dev->priv;
	struct bpq_req req;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (bpq == NULL)		/* woops! */
		return -ENODEV;

	switch (cmd) {
		case SIOCSBPQETHOPT:
			if (copy_from_user(&req, ifr->ifr_data, sizeof(struct bpq_req)))
				return -EFAULT;
			switch (req.cmd) {
				case SIOCGBPQETHPARAM:
				case SIOCSBPQETHPARAM:
				default:
					return -EINVAL;
			}

			break;

		case SIOCSBPQETHADDR:
			if (copy_from_user(bpq->dest_addr, ethaddr->destination, ETH_ALEN))
				return -EFAULT;
			if (copy_from_user(bpq->acpt_addr, ethaddr->accept, ETH_ALEN))
				return -EFAULT;
			break;

		default:
			return -EINVAL;
	}

	return 0;
}

/*
 * open/close a device
 */
static int bpq_open(struct net_device *dev)
{
	if (bpq_check_devices(dev))
		return -ENODEV;		/* oops, it's gone */
	
	MOD_INC_USE_COUNT;

	netif_start_queue(dev);
	return 0;
}

static int bpq_close(struct net_device *dev)
{
	netif_stop_queue(dev);
	MOD_DEC_USE_COUNT;
	return 0;
}

/*
 * currently unused
 */
static int bpq_dev_init(struct net_device *dev)
{
	return 0;
}


/* ------------------------------------------------------------------------ */


/*
 *	Proc filesystem
 */
static char * bpq_print_ethaddr(unsigned char *e)
{
	static char buf[18];

	sprintf(buf, "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
		e[0], e[1], e[2], e[3], e[4], e[5]);

	return buf;
}

static int bpq_get_info(char *buffer, char **start, off_t offset, int length)
{
	struct bpqdev *bpqdev;
	int len     = 0;
	off_t pos   = 0;
	off_t begin = 0;

	cli();

	len += sprintf(buffer, "dev   ether      destination        accept from\n");

	for (bpqdev = bpq_devices; bpqdev != NULL; bpqdev = bpqdev->next) {
		len += sprintf(buffer + len, "%-5s %-10s %s  ",
			bpqdev->axdev.name, bpqdev->ethname,
			bpq_print_ethaddr(bpqdev->dest_addr));

		len += sprintf(buffer + len, "%s\n",
			(bpqdev->acpt_addr[0] & 0x01) ? "*" : bpq_print_ethaddr(bpqdev->acpt_addr));

		pos = begin + len;

		if (pos < offset) {
			len   = 0;
			begin = pos;
		}

		if (pos > offset + length)
			break;
	}

	sti();

	*start = buffer + (offset - begin);
	len   -= (offset - begin);

	if (len > length) len = length;

	return len;
}


/* ------------------------------------------------------------------------ */


/*
 *	Setup a new device.
 */
static int bpq_new_device(struct net_device *dev)
{
	int k;
	struct bpqdev *bpq, *bpq2;

	if ((bpq = kmalloc(sizeof(struct bpqdev), GFP_KERNEL)) == NULL)
		return -ENOMEM;

	memset(bpq, 0, sizeof(struct bpqdev));

	bpq->ethdev = dev;

	bpq->ethname[sizeof(bpq->ethname)-1] = '\0';
	strncpy(bpq->ethname, dev->name, sizeof(bpq->ethname)-1);

	memcpy(bpq->dest_addr, bcast_addr, sizeof(bpq_eth_addr));
	memcpy(bpq->acpt_addr, bcast_addr, sizeof(bpq_eth_addr));

	dev = &bpq->axdev;

	for (k = 0; k < MAXBPQDEV; k++) {
		struct net_device *odev;

		sprintf(dev->name, "bpq%d", k);

		if ((odev = __dev_get_by_name(dev->name)) == NULL || bpq_check_devices(odev))
			break;
	}

	if (k == MAXBPQDEV) {
		kfree(bpq);
		return -ENODEV;
	}

	dev->priv = (void *)bpq;	/* pointer back */
	dev->init = bpq_dev_init;

	/* We should be locked, call register_netdevice() directly. */

	if (register_netdevice(dev) != 0) {
		kfree(bpq);
                return -EIO;
        }

	dev->hard_start_xmit = bpq_xmit;
	dev->open	     = bpq_open;
	dev->stop	     = bpq_close;
	dev->set_mac_address = bpq_set_mac_address;
	dev->get_stats	     = bpq_get_stats;
	dev->do_ioctl	     = bpq_ioctl;

	memcpy(dev->broadcast, ax25_bcast, AX25_ADDR_LEN);
	memcpy(dev->dev_addr,  ax25_defaddr, AX25_ADDR_LEN);

	dev->flags      = 0;

#if defined(CONFIG_AX25) || defined(CONFIG_AX25_MODULE)
	dev->hard_header     = ax25_encapsulate;
	dev->rebuild_header  = ax25_rebuild_header;
#endif

	dev->type            = ARPHRD_AX25;
	dev->hard_header_len = AX25_MAX_HEADER_LEN + AX25_BPQ_HEADER_LEN;
	dev->mtu             = AX25_DEF_PACLEN;
	dev->addr_len        = AX25_ADDR_LEN;

	cli();

	if (bpq_devices == NULL) {
		bpq_devices = bpq;
	} else {
		for (bpq2 = bpq_devices; bpq2->next != NULL; bpq2 = bpq2->next);
		bpq2->next = bpq;
	}

	sti();

	return 0;
}


/*
 *	Handle device status changes.
 */
static int bpq_device_event(struct notifier_block *this,unsigned long event, void *ptr)
{
	struct net_device *dev = (struct net_device *)ptr;

	if (!dev_is_ethdev(dev))
		return NOTIFY_DONE;

	bpq_check_devices(NULL);

	switch (event) {
		case NETDEV_UP:		/* new ethernet device -> new BPQ interface */
			if (bpq_get_ax25_dev(dev) == NULL)
				bpq_new_device(dev);
			break;

		case NETDEV_DOWN:	/* ethernet device closed -> close BPQ interface */
			if ((dev = bpq_get_ax25_dev(dev)) != NULL)
				dev_close(dev);
			break;

		default:
			break;
	}

	return NOTIFY_DONE;
}


/* ------------------------------------------------------------------------ */

/*
 * Initialize driver. To be called from af_ax25 if not compiled as a
 * module
 */
static int __init bpq_init_driver(void)
{
	struct net_device *dev;

	dev_add_pack(&bpq_packet_type);

	register_netdevice_notifier(&bpq_dev_notifier);

	printk(banner);

	proc_net_create("bpqether", 0, bpq_get_info);

	read_lock_bh(&dev_base_lock);
	for (dev = dev_base; dev != NULL; dev = dev->next) {
		if (dev_is_ethdev(dev)) {
			read_unlock_bh(&dev_base_lock);
			bpq_new_device(dev);
			read_lock_bh(&dev_base_lock);
		}
	}
	read_unlock_bh(&dev_base_lock);
	return 0;
}

static void __exit bpq_cleanup_driver(void)
{
	struct bpqdev *bpq;

	dev_remove_pack(&bpq_packet_type);

	unregister_netdevice_notifier(&bpq_dev_notifier);

	proc_net_remove("bpqether");

	for (bpq = bpq_devices; bpq != NULL; bpq = bpq->next)
		unregister_netdev(&bpq->axdev);
}

MODULE_AUTHOR("Joerg Reuter DL1BKE <jreuter@yaina.de>");
MODULE_DESCRIPTION("Transmit and receive AX.25 packets over Ethernet");
MODULE_LICENSE("GPL");
module_init(bpq_init_driver);
module_exit(bpq_cleanup_driver);
