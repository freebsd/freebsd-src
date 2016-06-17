/*
 * Generic HDLC support routines for Linux
 * Point-to-point protocol support
 *
 * Copyright (C) 1999 - 2003 Krzysztof Halasa <khc@pm.waw.pl>
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


static int ppp_open(hdlc_device *hdlc)
{
	struct net_device *dev = hdlc_to_dev(hdlc);
	void *old_ioctl;
	int result;

	dev->priv = &hdlc->state.ppp.syncppp_ptr;
	hdlc->state.ppp.syncppp_ptr = &hdlc->state.ppp.pppdev;
	hdlc->state.ppp.pppdev.dev = dev;

	old_ioctl = dev->do_ioctl;
	hdlc->state.ppp.old_change_mtu = dev->change_mtu;
	sppp_attach(&hdlc->state.ppp.pppdev);
	/* sppp_attach nukes them. We don't need syncppp's ioctl */
	dev->do_ioctl = old_ioctl;
	hdlc->state.ppp.pppdev.sppp.pp_flags &= ~PP_CISCO;
	dev->type = ARPHRD_PPP;
	result = sppp_open(dev);
	if (result) {
		sppp_detach(dev);
		return result;
	}

	return 0;
}



static void ppp_close(hdlc_device *hdlc)
{
	struct net_device *dev = hdlc_to_dev(hdlc);

	sppp_close(dev);
	sppp_detach(dev);
	dev->rebuild_header = NULL;
	dev->change_mtu = hdlc->state.ppp.old_change_mtu;
	dev->mtu = HDLC_MAX_MTU;
	dev->hard_header_len = 16;
}



static unsigned short ppp_type_trans(struct sk_buff *skb,
				     struct net_device *dev)
{
	return __constant_htons(ETH_P_WAN_PPP);
}



int hdlc_ppp_ioctl(hdlc_device *hdlc, struct ifreq *ifr)
{
	struct net_device *dev = hdlc_to_dev(hdlc);
	int result;

	switch (ifr->ifr_settings.type) {
	case IF_GET_PROTO:
		ifr->ifr_settings.type = IF_PROTO_PPP;
		return 0; /* return protocol only, no settable parameters */

	case IF_PROTO_PPP:
		if(!capable(CAP_NET_ADMIN))
			return -EPERM;

		if(dev->flags & IFF_UP)
			return -EBUSY;

		/* no settable parameters */

		result=hdlc->attach(hdlc, ENCODING_NRZ,PARITY_CRC16_PR1_CCITT);
		if (result)
			return result;

		hdlc_proto_detach(hdlc);

		hdlc->open = ppp_open;
		hdlc->stop = ppp_close;
		hdlc->netif_rx = NULL;
		hdlc->type_trans = ppp_type_trans;
		hdlc->proto = IF_PROTO_PPP;
		dev->hard_start_xmit = hdlc->xmit;
		dev->hard_header = NULL;
		dev->type = ARPHRD_PPP;
		dev->addr_len = 0;
		return 0;
	}

	return -EINVAL;
}
