/*
 * Generic HDLC support routines for Linux
 * Cisco HDLC support
 *
 * Copyright (C) 2000 - 2003 Krzysztof Halasa <khc@pm.waw.pl>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/errno.h>
#include <linux/if_arp.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/pkt_sched.h>
#include <linux/inetdevice.h>
#include <linux/lapb.h>
#include <linux/rtnetlink.h>
#include <linux/hdlc.h>

#undef DEBUG_HARD_HEADER

#define CISCO_MULTICAST		0x8F	/* Cisco multicast address */
#define CISCO_UNICAST		0x0F	/* Cisco unicast address */
#define CISCO_KEEPALIVE		0x8035	/* Cisco keepalive protocol */
#define CISCO_SYS_INFO		0x2000	/* Cisco interface/system info */
#define CISCO_ADDR_REQ		0	/* Cisco address request */
#define CISCO_ADDR_REPLY	1	/* Cisco address reply */
#define CISCO_KEEPALIVE_REQ	2	/* Cisco keepalive request */


static int cisco_hard_header(struct sk_buff *skb, struct net_device *dev,
			     u16 type, void *daddr, void *saddr,
			     unsigned int len)
{
	hdlc_header *data;
#ifdef DEBUG_HARD_HEADER
	printk(KERN_DEBUG "%s: cisco_hard_header called\n", dev->name);
#endif

	skb_push(skb, sizeof(hdlc_header));
	data = (hdlc_header*)skb->data;
	if (type == CISCO_KEEPALIVE)
		data->address = CISCO_MULTICAST;
	else
		data->address = CISCO_UNICAST;
	data->control = 0;
	data->protocol = htons(type);

	return sizeof(hdlc_header);
}



static void cisco_keepalive_send(hdlc_device *hdlc, u32 type,
				 u32 par1, u32 par2)
{
	struct sk_buff *skb;
	cisco_packet *data;

	skb = dev_alloc_skb(sizeof(hdlc_header) + sizeof(cisco_packet));
	if (!skb) {
		printk(KERN_WARNING
		       "%s: Memory squeeze on cisco_keepalive_send()\n",
		       hdlc_to_name(hdlc));
		return;
	}
	skb_reserve(skb, 4);
	cisco_hard_header(skb, hdlc_to_dev(hdlc), CISCO_KEEPALIVE,
			  NULL, NULL, 0);
	data = (cisco_packet*)skb->tail;

	data->type = htonl(type);
	data->par1 = htonl(par1);
	data->par2 = htonl(par2);
	data->rel = 0xFFFF;
	/* we will need do_div here if 1000 % HZ != 0 */
	data->time = htonl(jiffies * (1000 / HZ));

	skb_put(skb, sizeof(cisco_packet));
	skb->priority = TC_PRIO_CONTROL;
	skb->dev = hdlc_to_dev(hdlc);
	skb->nh.raw = skb->data;

	dev_queue_xmit(skb);
}



static unsigned short cisco_type_trans(struct sk_buff *skb,
				       struct net_device *dev)
{
	hdlc_header *data = (hdlc_header*)skb->data;

	if (skb->len < sizeof(hdlc_header))
		return __constant_htons(ETH_P_HDLC);

	if (data->address != CISCO_MULTICAST &&
	    data->address != CISCO_UNICAST)
		return __constant_htons(ETH_P_HDLC);

	switch(data->protocol) {
	case __constant_htons(ETH_P_IP):
	case __constant_htons(ETH_P_IPX):
	case __constant_htons(ETH_P_IPV6):
		skb_pull(skb, sizeof(hdlc_header));
		return data->protocol;
	default:
		return __constant_htons(ETH_P_HDLC);
	}
}


static void cisco_rx(struct sk_buff *skb)
{
	hdlc_device *hdlc = dev_to_hdlc(skb->dev);
	hdlc_header *data = (hdlc_header*)skb->data;
	cisco_packet *cisco_data;
	struct in_device *in_dev;
	u32 addr, mask;

	if (skb->len < sizeof(hdlc_header))
		goto rx_error;

	if (data->address != CISCO_MULTICAST &&
	    data->address != CISCO_UNICAST)
		goto rx_error;

	skb_pull(skb, sizeof(hdlc_header));

	switch(ntohs(data->protocol)) {
	case CISCO_SYS_INFO:
		/* Packet is not needed, drop it. */
		dev_kfree_skb_any(skb);
		return;

	case CISCO_KEEPALIVE:
		if (skb->len != CISCO_PACKET_LEN &&
		    skb->len != CISCO_BIG_PACKET_LEN) {
			printk(KERN_INFO "%s: Invalid length of Cisco "
			       "control packet (%d bytes)\n",
			       hdlc_to_name(hdlc), skb->len);
			goto rx_error;
		}

		cisco_data = (cisco_packet*)skb->data;

		switch(ntohl (cisco_data->type)) {
		case CISCO_ADDR_REQ: /* Stolen from syncppp.c :-) */
			in_dev = hdlc_to_dev(hdlc)->ip_ptr;
			addr = 0;
			mask = ~0; /* is the mask correct? */

			if (in_dev != NULL) {
				struct in_ifaddr **ifap = &in_dev->ifa_list;

				while (*ifap != NULL) {
					if (strcmp(hdlc_to_name(hdlc),
						   (*ifap)->ifa_label) == 0) {
						addr = (*ifap)->ifa_local;
						mask = (*ifap)->ifa_mask;
						break;
					}
					ifap = &(*ifap)->ifa_next;
				}

				cisco_keepalive_send(hdlc, CISCO_ADDR_REPLY,
						     addr, mask);
			}
			dev_kfree_skb_any(skb);
			return;

		case CISCO_ADDR_REPLY:
			printk(KERN_INFO "%s: Unexpected Cisco IP address "
			       "reply\n", hdlc_to_name(hdlc));
			goto rx_error;

		case CISCO_KEEPALIVE_REQ:
			hdlc->state.cisco.rxseq = ntohl(cisco_data->par1);
			if (ntohl(cisco_data->par2)==hdlc->state.cisco.txseq) {
				hdlc->state.cisco.last_poll = jiffies;
				if (!hdlc->state.cisco.up) {
					u32 sec, min, hrs, days;
					sec = ntohl(cisco_data->time) / 1000;
					min = sec / 60; sec -= min * 60;
					hrs = min / 60; min -= hrs * 60;
					days = hrs / 24; hrs -= days * 24;
					printk(KERN_INFO "%s: Link up (peer "
					       "uptime %ud%uh%um%us)\n",
					       hdlc_to_name(hdlc), days, hrs,
					       min, sec);
				}
				hdlc->state.cisco.up = 1;
			}

			dev_kfree_skb_any(skb);
			return;
		} /* switch(keepalive type) */
	} /* switch(protocol) */

	printk(KERN_INFO "%s: Unsupported protocol %x\n", hdlc_to_name(hdlc),
	       data->protocol);
	dev_kfree_skb_any(skb);
	return;

 rx_error:
	hdlc->stats.rx_errors++; /* Mark error */
	dev_kfree_skb_any(skb);
}



static void cisco_timer(unsigned long arg)
{
	hdlc_device *hdlc = (hdlc_device*)arg;

	if (hdlc->state.cisco.up && jiffies - hdlc->state.cisco.last_poll >=
	    hdlc->state.cisco.settings.timeout * HZ) {
		hdlc->state.cisco.up = 0;
		printk(KERN_INFO "%s: Link down\n", hdlc_to_name(hdlc));
	}

	cisco_keepalive_send(hdlc, CISCO_KEEPALIVE_REQ,
			     ++hdlc->state.cisco.txseq,
			     hdlc->state.cisco.rxseq);
	hdlc->state.cisco.timer.expires = jiffies +
		hdlc->state.cisco.settings.interval * HZ;
	hdlc->state.cisco.timer.function = cisco_timer;
	hdlc->state.cisco.timer.data = arg;
	add_timer(&hdlc->state.cisco.timer);
}



static int cisco_open(hdlc_device *hdlc)
{
	hdlc->state.cisco.last_poll = 0;
	hdlc->state.cisco.up = 0;
	hdlc->state.cisco.txseq = hdlc->state.cisco.rxseq = 0;

	init_timer(&hdlc->state.cisco.timer);
	hdlc->state.cisco.timer.expires = jiffies + HZ; /*First poll after 1s*/
	hdlc->state.cisco.timer.function = cisco_timer;
	hdlc->state.cisco.timer.data = (unsigned long)hdlc;
	add_timer(&hdlc->state.cisco.timer);
	return 0;
}



static void cisco_close(hdlc_device *hdlc)
{
	del_timer_sync(&hdlc->state.cisco.timer);
}



int hdlc_cisco_ioctl(hdlc_device *hdlc, struct ifreq *ifr)
{
	cisco_proto *cisco_s = ifr->ifr_settings.ifs_ifsu.cisco;
	const size_t size = sizeof(cisco_proto);
	cisco_proto new_settings;
	struct net_device *dev = hdlc_to_dev(hdlc);
	int result;

	switch (ifr->ifr_settings.type) {
	case IF_GET_PROTO:
		ifr->ifr_settings.type = IF_PROTO_CISCO;
		if (ifr->ifr_settings.size < size) {
			ifr->ifr_settings.size = size; /* data size wanted */
			return -ENOBUFS;
		}
		if (copy_to_user(cisco_s, &hdlc->state.cisco.settings, size))
			return -EFAULT;
		return 0;

	case IF_PROTO_CISCO:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (dev->flags & IFF_UP)
			return -EBUSY;

		if (copy_from_user(&new_settings, cisco_s, size))
			return -EFAULT;

		if (new_settings.interval < 1 ||
		    new_settings.timeout < 2)
			return -EINVAL;

		result=hdlc->attach(hdlc, ENCODING_NRZ,PARITY_CRC16_PR1_CCITT);

		if (result)
			return result;

		hdlc_proto_detach(hdlc);
		memcpy(&hdlc->state.cisco.settings, &new_settings, size);

		hdlc->open = cisco_open;
		hdlc->stop = cisco_close;
		hdlc->netif_rx = cisco_rx;
		hdlc->type_trans = cisco_type_trans;
		hdlc->proto = IF_PROTO_CISCO;
		dev->hard_start_xmit = hdlc->xmit;
		dev->hard_header = cisco_hard_header;
		dev->type = ARPHRD_CISCO;
		dev->addr_len = 0;
		return 0;
	}

	return -EINVAL;
}
