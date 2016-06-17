/*
 * Synchronous PPP / Cisco-HDLC driver for the COMX boards
 *
 * Author: Gergely Madarasz <gorgo@itc.hu>
 *
 * based on skeleton code by Tivadar Szemethy <tiv@itc.hu>
 *
 * Copyright (C) 1999 ITConsult-Pro Co. <info@itc.hu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 *
 * Version 0.10 (99/06/10):
 *		- written the first code :)
 *
 * Version 0.20 (99/06/16):
 *		- added hdlc protocol 
 *		- protocol up is IFF_RUNNING
 *
 * Version 0.21 (99/07/15):
 *		- some small fixes with the line status
 *
 * Version 0.22 (99/08/05):
 *		- don't test IFF_RUNNING but the pp_link_state of the sppp
 * 
 * Version 0.23 (99/12/02):
 *		- tbusy fixes
 *
 */

#define VERSION "0.23"

#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <linux/if_arp.h>
#include <linux/inetdevice.h>
#include <asm/uaccess.h>
#include <linux/init.h>

#include <net/syncppp.h>
#include	"comx.h"

MODULE_AUTHOR("Author: Gergely Madarasz <gorgo@itc.hu>");
MODULE_DESCRIPTION("Cisco-HDLC / Synchronous PPP driver for the COMX sync serial boards");
MODULE_LICENSE("GPL");

static struct comx_protocol syncppp_protocol;
static struct comx_protocol hdlc_protocol;

struct syncppp_data {
	struct timer_list status_timer;
};

static void syncppp_status_timerfun(unsigned long d) {
	struct net_device *dev=(struct net_device *)d;
	struct comx_channel *ch=dev->priv;
	struct syncppp_data *spch=ch->LINE_privdata;
	struct sppp *sp = (struct sppp *)sppp_of(dev);
        
	if(!(ch->line_status & PROTO_UP) && 
	    (sp->pp_link_state==SPPP_LINK_UP)) {
    		comx_status(dev, ch->line_status | PROTO_UP);
	}
	if((ch->line_status & PROTO_UP) &&
	    (sp->pp_link_state==SPPP_LINK_DOWN)) {
	    	comx_status(dev, ch->line_status & ~PROTO_UP);
	}
	mod_timer(&spch->status_timer,jiffies + HZ*3);
}

static int syncppp_tx(struct net_device *dev) 
{
	struct comx_channel *ch=dev->priv;
	
	if(ch->line_status & LINE_UP) {
		netif_wake_queue(dev);
	}
	return 0;
}

static void syncppp_status(struct net_device *dev, unsigned short status)
{
	status &= ~(PROTO_UP | PROTO_LOOP);
	if(status & LINE_UP) {
		netif_wake_queue(dev);
		sppp_open(dev);
	} else 	{
		/* Line went down */
		netif_stop_queue(dev);
		sppp_close(dev);
	}
	comx_status(dev, status);
}

static int syncppp_open(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	struct syncppp_data *spch = ch->LINE_privdata;

	if (!(ch->init_status & HW_OPEN)) return -ENODEV;

	ch->init_status |= LINE_OPEN;
	ch->line_status &= ~(PROTO_UP | PROTO_LOOP);

	if(ch->line_status & LINE_UP) {
		sppp_open(dev);
	}

	init_timer(&spch->status_timer);
	spch->status_timer.function=syncppp_status_timerfun;
	spch->status_timer.data=(unsigned long)dev;
	spch->status_timer.expires=jiffies + HZ*3;
	add_timer(&spch->status_timer);
	
	return 0;
}

static int syncppp_close(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	struct syncppp_data *spch = ch->LINE_privdata;

	if (!(ch->init_status & HW_OPEN)) return -ENODEV;
	del_timer(&spch->status_timer);
	
	sppp_close(dev);

	ch->init_status &= ~LINE_OPEN;
	ch->line_status &= ~(PROTO_UP | PROTO_LOOP);

	return 0;
}

static int syncppp_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;

	netif_stop_queue(dev);
	switch(ch->HW_send_packet(dev, skb)) {
		case FRAME_QUEUED:
			netif_wake_queue(dev);
			break;
		case FRAME_ACCEPTED:
		case FRAME_DROPPED:
			break;
		case FRAME_ERROR:
			printk(KERN_ERR "%s: Transmit frame error (len %d)\n", 
				dev->name, skb->len);
		break;
	}
	return 0;
}


static int syncppp_statistics(struct net_device *dev, char *page) 
{
	int len = 0;

	len += sprintf(page + len, " ");
	return len;
}


static int syncppp_exit(struct net_device *dev) 
{
	struct comx_channel *ch = dev->priv;

	sppp_detach(dev);

	dev->flags = 0;
	dev->type = 0;
	dev->mtu = 0;

	ch->LINE_rx = NULL;
	ch->LINE_tx = NULL;
	ch->LINE_status = NULL;
	ch->LINE_open = NULL;
	ch->LINE_close = NULL;
	ch->LINE_xmit = NULL;
	ch->LINE_header	= NULL;
	ch->LINE_rebuild_header	= NULL;
	ch->LINE_statistics = NULL;

	kfree(ch->LINE_privdata);
	ch->LINE_privdata = NULL;

	MOD_DEC_USE_COUNT;
	return 0;
}

static int syncppp_init(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	struct ppp_device *pppdev = (struct ppp_device *)ch->if_ptr;

	ch->LINE_privdata = kmalloc(sizeof(struct syncppp_data), GFP_KERNEL);
	if (!ch->LINE_privdata)
		return -ENOMEM;

	pppdev->dev = dev;
	sppp_attach(pppdev);

	if(ch->protocol == &hdlc_protocol) {
		pppdev->sppp.pp_flags |= PP_CISCO;
		dev->type = ARPHRD_HDLC;
	} else {
		pppdev->sppp.pp_flags &= ~PP_CISCO;
		dev->type = ARPHRD_PPP;
	}

	ch->LINE_rx = sppp_input;
	ch->LINE_tx = syncppp_tx;
	ch->LINE_status = syncppp_status;
	ch->LINE_open = syncppp_open;
	ch->LINE_close = syncppp_close;
	ch->LINE_xmit = syncppp_xmit;
	ch->LINE_header	= NULL;
	ch->LINE_statistics = syncppp_statistics;


	MOD_INC_USE_COUNT;
	return 0;
}

static struct comx_protocol syncppp_protocol = {
	"ppp", 
	VERSION,
	ARPHRD_PPP, 
	syncppp_init, 
	syncppp_exit, 
	NULL 
};

static struct comx_protocol hdlc_protocol = {
	"hdlc", 
	VERSION,
	ARPHRD_PPP, 
	syncppp_init, 
	syncppp_exit, 
	NULL 
};


#ifdef MODULE
#define comx_proto_ppp_init init_module
#endif

int __init comx_proto_ppp_init(void)
{
	int ret;

	if(0!=(ret=comx_register_protocol(&hdlc_protocol))) {
		return ret;
	}
	return comx_register_protocol(&syncppp_protocol);
}

#ifdef MODULE
void cleanup_module(void)
{
	comx_unregister_protocol(syncppp_protocol.name);
	comx_unregister_protocol(hdlc_protocol.name);
}
#endif /* MODULE */

