/*
 * LAPB protocol module for the COMX driver 
 * for Linux kernel 2.2.X
 *
 * Original author: Tivadar Szemethy <tiv@itc.hu>
 * Maintainer: Gergely Madarasz <gorgo@itc.hu>
 *
 * Copyright (C) 1997-1999 (C) ITConsult-Pro Co. <info@itc.hu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * Version 0.80 (99/06/14):
 *		- cleaned up the source code a bit
 *		- ported back to kernel, now works as non-module
 *
 * Changed      (00/10/29, Henner Eisen):
 * 		- comx_rx() / comxlapb_data_indication() return status.
 * 
 */

#define VERSION "0.80"

#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <linux/if_arp.h>
#include <linux/inetdevice.h>
#include <asm/uaccess.h>
#include <linux/lapb.h>
#include <linux/init.h>

#include	"comx.h"
#include	"comxhw.h"

static struct proc_dir_entry *create_comxlapb_proc_entry(char *name, int mode,
	int size, struct proc_dir_entry *dir);

static void comxlapb_rx(struct net_device *dev, struct sk_buff *skb) 
{
	if (!dev || !dev->priv) {
		dev_kfree_skb(skb);
	} else {
		lapb_data_received(dev->priv, skb);
	}
}

static int comxlapb_tx(struct net_device *dev) 
{
	netif_wake_queue(dev);
	return 0;
}

static int comxlapb_header(struct sk_buff *skb, struct net_device *dev, 
	unsigned short type, void *daddr, void *saddr, unsigned len) 
{
	return dev->hard_header_len;  
}

static void comxlapb_status(struct net_device *dev, unsigned short status)
{
	struct comx_channel *ch;

	if (!dev || !(ch = dev->priv)) {
		return;
	}
	if (status & LINE_UP) {
		netif_wake_queue(dev);
	}
	comx_status(dev, status);
}

static int comxlapb_open(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	int err = 0;

	if (!(ch->init_status & HW_OPEN)) {
		return -ENODEV;
	}

	err = lapb_connect_request(ch);

	if (ch->debug_flags & DEBUG_COMX_LAPB) {
		comx_debug(dev, "%s: lapb opened, error code: %d\n", 
			dev->name, err);
	}

	if (!err) {
		ch->init_status |= LINE_OPEN;
		MOD_INC_USE_COUNT;
	}
	return err;
}

static int comxlapb_close(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;

	if (!(ch->init_status & HW_OPEN)) {
		return -ENODEV;
	}

	if (ch->debug_flags & DEBUG_COMX_LAPB) {
		comx_debug(dev, "%s: lapb closed\n", dev->name);
	}

	lapb_disconnect_request(ch);

	ch->init_status &= ~LINE_OPEN;
	ch->line_status &= ~PROTO_UP;
	MOD_DEC_USE_COUNT;
	return 0;
}

static int comxlapb_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	struct sk_buff *skb2;

	if (!dev || !(ch = dev->priv) || !(dev->flags & (IFF_UP | IFF_RUNNING))) {
		return -ENODEV;
	}

	if (dev->type == ARPHRD_X25) { // first byte tells what to do 
		switch(skb->data[0]) {
			case 0x00:	
				break;	// transmit
			case 0x01:	
				lapb_connect_request(ch);
				kfree_skb(skb);
				return 0;
			case 0x02:	
				lapb_disconnect_request(ch);
			default:
				kfree_skb(skb);
				return 0;
		}
		skb_pull(skb,1);
	}

	netif_stop_queue(dev);
	
	if ((skb2 = skb_clone(skb, GFP_ATOMIC)) != NULL) {
		lapb_data_request(ch, skb2);
	}

	return FRAME_ACCEPTED;
}

static int comxlapb_statistics(struct net_device *dev, char *page) 
{
	struct lapb_parms_struct parms;
	int len = 0;

	len += sprintf(page + len, "Line status: ");
	if (lapb_getparms(dev->priv, &parms) != LAPB_OK) {
		len += sprintf(page + len, "not initialized\n");
		return len;
	}
	len += sprintf(page + len, "%s (%s), T1: %d/%d, T2: %d/%d, N2: %d/%d, "
		"window: %d\n", parms.mode & LAPB_DCE ? "DCE" : "DTE", 
		parms.mode & LAPB_EXTENDED ? "EXTENDED" : "STANDARD",
		parms.t1timer, parms.t1, parms.t2timer, parms.t2, 
		parms.n2count, parms.n2, parms.window);

	return len;
}

static int comxlapb_read_proc(char *page, char **start, off_t off, int count,
	int *eof, void *data)
{
	struct proc_dir_entry *file = (struct proc_dir_entry *)data;
	struct net_device *dev = file->parent->data;
	struct lapb_parms_struct parms;
	int len = 0;

	if (lapb_getparms(dev->priv, &parms)) {
		return -ENODEV;
	}

	if (strcmp(file->name, FILENAME_T1) == 0) {
		len += sprintf(page + len, "%02u / %02u\n", 
			parms.t1timer, parms.t1);
	} else if (strcmp(file->name, FILENAME_T2) == 0) {
		len += sprintf(page + len, "%02u / %02u\n", 
			parms.t2timer, parms.t2);
	} else if (strcmp(file->name, FILENAME_N2) == 0) {
		len += sprintf(page + len, "%02u / %02u\n", 
			parms.n2count, parms.n2);
	} else if (strcmp(file->name, FILENAME_WINDOW) == 0) {
		len += sprintf(page + len, "%u\n", parms.window);
	} else if (strcmp(file->name, FILENAME_MODE) == 0) {
		len += sprintf(page + len, "%s, %s\n", 
			parms.mode & LAPB_DCE ? "DCE" : "DTE",
			parms.mode & LAPB_EXTENDED ? "EXTENDED" : "STANDARD");
	} else {
		printk(KERN_ERR "comxlapb: internal error, filename %s\n", file->name);
		return -EBADF;
	}

	if (off >= len) {
		*eof = 1;
		return 0;
	}

	*start = page + off;
	if (count >= len - off) {
		*eof = 1;
	}
	return min_t(int, count, len - off);
}

static int comxlapb_write_proc(struct file *file, const char *buffer, 
	u_long count, void *data)
{
	struct proc_dir_entry *entry = (struct proc_dir_entry *)data;
	struct net_device *dev = entry->parent->data;
	struct lapb_parms_struct parms;
	unsigned long parm;
	char *page;

	if (lapb_getparms(dev->priv, &parms)) {
		return -ENODEV;
	}

	if (!(page = (char *)__get_free_page(GFP_KERNEL))) {
		return -ENOMEM;
	}

	copy_from_user(page, buffer, count);
	if (*(page + count - 1) == '\n') {
		*(page + count - 1) = 0;
	}

	if (strcmp(entry->name, FILENAME_T1) == 0) {
		parm=simple_strtoul(page,NULL,10);
		if (parm > 0 && parm < 100) {
			parms.t1=parm;
			lapb_setparms(dev->priv, &parms);
		}
	} else if (strcmp(entry->name, FILENAME_T2) == 0) {
		parm=simple_strtoul(page, NULL, 10);
		if (parm > 0 && parm < 100) {
			parms.t2=parm;
			lapb_setparms(dev->priv, &parms);
		}
	} else if (strcmp(entry->name, FILENAME_N2) == 0) {
		parm=simple_strtoul(page, NULL, 10);
		if (parm > 0 && parm < 100) {
			parms.n2=parm;
			lapb_setparms(dev->priv, &parms);
		}
	} else if (strcmp(entry->name, FILENAME_WINDOW) == 0) {
		parms.window = simple_strtoul(page, NULL, 10);
		lapb_setparms(dev->priv, &parms);
	} else if (strcmp(entry->name, FILENAME_MODE) == 0) {
		if (comx_strcasecmp(page, "dte") == 0) {
			parms.mode &= ~(LAPB_DCE | LAPB_DTE); 
			parms.mode |= LAPB_DTE;
		} else if (comx_strcasecmp(page, "dce") == 0) {
			parms.mode &= ~(LAPB_DTE | LAPB_DCE); 
			parms.mode |= LAPB_DCE;
		} else if (comx_strcasecmp(page, "std") == 0 || 
		    comx_strcasecmp(page, "standard") == 0) {
			parms.mode &= ~LAPB_EXTENDED; 
			parms.mode |= LAPB_STANDARD;
		} else if (comx_strcasecmp(page, "ext") == 0 || 
		    comx_strcasecmp(page, "extended") == 0) {
			parms.mode &= ~LAPB_STANDARD; 
			parms.mode |= LAPB_EXTENDED;
		}
		lapb_setparms(dev->priv, &parms);
	} else {
		printk(KERN_ERR "comxlapb_write_proc: internal error, filename %s\n", 
			entry->name);
		return -EBADF;
	}

	free_page((unsigned long)page);
	return count;
}

static void comxlapb_connected(void *token, int reason)
{
	struct comx_channel *ch = token; 
	struct proc_dir_entry *comxdir = ch->procdir->subdir;

	if (ch->debug_flags & DEBUG_COMX_LAPB) {
		comx_debug(ch->dev, "%s: lapb connected, reason: %d\n", 
			ch->dev->name, reason);
	}

	if (ch->dev->type == ARPHRD_X25) {
		unsigned char *p;
		struct sk_buff *skb;

		if ((skb = dev_alloc_skb(1)) == NULL) {
			printk(KERN_ERR "comxlapb: out of memory!\n");
			return;
		}
		p = skb_put(skb,1);
		*p = 0x01;		// link established
		skb->dev = ch->dev;
		skb->protocol = htons(ETH_P_X25);
		skb->mac.raw = skb->data;
		skb->pkt_type = PACKET_HOST;

		netif_rx(skb);
		ch->dev->last_rx = jiffies;
	}

	for (; comxdir; comxdir = comxdir->next) {
		if (strcmp(comxdir->name, FILENAME_MODE) == 0) {
			comxdir->mode = S_IFREG | 0444;
		}
	}


	ch->line_status |= PROTO_UP;
	comx_status(ch->dev, ch->line_status);
}

static void comxlapb_disconnected(void *token, int reason)
{
	struct comx_channel *ch = token; 
	struct proc_dir_entry *comxdir = ch->procdir->subdir;

	if (ch->debug_flags & DEBUG_COMX_LAPB) {
		comx_debug(ch->dev, "%s: lapb disconnected, reason: %d\n", 
			ch->dev->name, reason);
	}

	if (ch->dev->type == ARPHRD_X25) {
		unsigned char *p;
		struct sk_buff *skb;

		if ((skb = dev_alloc_skb(1)) == NULL) {
			printk(KERN_ERR "comxlapb: out of memory!\n");
			return;
		}
		p = skb_put(skb,1);
		*p = 0x02;		// link disconnected
		skb->dev = ch->dev;
		skb->protocol = htons(ETH_P_X25);
		skb->mac.raw = skb->data;
		skb->pkt_type = PACKET_HOST;

		netif_rx(skb);
		ch->dev->last_rx = jiffies;
	}

	for (; comxdir; comxdir = comxdir->next) {
		if (strcmp(comxdir->name, FILENAME_MODE) == 0) {
			comxdir->mode = S_IFREG | 0644;
		}
	}
	
	ch->line_status &= ~PROTO_UP;
	comx_status(ch->dev, ch->line_status);
}

static int comxlapb_data_indication(void *token, struct sk_buff *skb)
{
	struct comx_channel *ch = token; 

	if (ch->dev->type == ARPHRD_X25) {
		skb_push(skb, 1);
		skb->data[0] = 0;	// indicate data for X25
		skb->protocol = htons(ETH_P_X25);
	} else {
		skb->protocol = htons(ETH_P_IP);
	}

	skb->dev = ch->dev;
	skb->mac.raw = skb->data;
	return comx_rx(ch->dev, skb);
}

static void comxlapb_data_transmit(void *token, struct sk_buff *skb)
{
	struct comx_channel *ch = token; 

	if (ch->HW_send_packet) {
		ch->HW_send_packet(ch->dev, skb);
	}
}

static int comxlapb_exit(struct net_device *dev) 
{
	struct comx_channel *ch = dev->priv;

	dev->flags		= 0;
	dev->type		= 0;
	dev->mtu		= 0;
	dev->hard_header_len    = 0;

	ch->LINE_rx	= NULL;
	ch->LINE_tx	= NULL;
	ch->LINE_status = NULL;
	ch->LINE_open	= NULL;
	ch->LINE_close	= NULL;
	ch->LINE_xmit	= NULL;
	ch->LINE_header	= NULL;
	ch->LINE_statistics = NULL;

	if (ch->debug_flags & DEBUG_COMX_LAPB) {
		comx_debug(dev, "%s: unregistering lapb\n", dev->name);
	}
	lapb_unregister(dev->priv);

	remove_proc_entry(FILENAME_T1, ch->procdir);
	remove_proc_entry(FILENAME_T2, ch->procdir);
	remove_proc_entry(FILENAME_N2, ch->procdir);
	remove_proc_entry(FILENAME_MODE, ch->procdir);
	remove_proc_entry(FILENAME_WINDOW, ch->procdir);

	MOD_DEC_USE_COUNT;
	return 0;
}

static int comxlapb_init(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	struct lapb_register_struct lapbreg;

	dev->mtu		= 1500;
	dev->hard_header_len    = 4;
	dev->addr_len		= 0;

	ch->LINE_rx	= comxlapb_rx;
	ch->LINE_tx	= comxlapb_tx;
	ch->LINE_status = comxlapb_status;
	ch->LINE_open	= comxlapb_open;
	ch->LINE_close	= comxlapb_close;
	ch->LINE_xmit	= comxlapb_xmit;
	ch->LINE_header	= comxlapb_header;
	ch->LINE_statistics = comxlapb_statistics;

	lapbreg.connect_confirmation = comxlapb_connected;
	lapbreg.connect_indication = comxlapb_connected;
	lapbreg.disconnect_confirmation = comxlapb_disconnected;
	lapbreg.disconnect_indication = comxlapb_disconnected;
	lapbreg.data_indication = comxlapb_data_indication;
	lapbreg.data_transmit = comxlapb_data_transmit;
	if (lapb_register(dev->priv, &lapbreg)) {
		return -ENOMEM;
	}
	if (ch->debug_flags & DEBUG_COMX_LAPB) {
		comx_debug(dev, "%s: lapb registered\n", dev->name);
	}

	if (!create_comxlapb_proc_entry(FILENAME_T1, 0644, 8, ch->procdir)) {
		return -ENOMEM;
	}
	if (!create_comxlapb_proc_entry(FILENAME_T2, 0644, 8, ch->procdir)) {
		return -ENOMEM;
	}
	if (!create_comxlapb_proc_entry(FILENAME_N2, 0644, 8, ch->procdir)) {
		return -ENOMEM;
	}
	if (!create_comxlapb_proc_entry(FILENAME_MODE, 0644, 14, ch->procdir)) {
		return -ENOMEM;
	}
	if (!create_comxlapb_proc_entry(FILENAME_WINDOW, 0644, 0, ch->procdir)) {
		return -ENOMEM;
	}

	MOD_INC_USE_COUNT;
	return 0;
}

static int comxlapb_init_lapb(struct net_device *dev) 
{
	dev->flags	= IFF_POINTOPOINT | IFF_NOARP | IFF_MULTICAST;
	dev->type	= ARPHRD_LAPB;

	return(comxlapb_init(dev));
}

static int comxlapb_init_x25(struct net_device *dev)
{
	dev->flags		= IFF_NOARP;
	dev->type		= ARPHRD_X25;

	return(comxlapb_init(dev));
}

static struct proc_dir_entry *create_comxlapb_proc_entry(char *name, int mode,
	int size, struct proc_dir_entry *dir)
{
	struct proc_dir_entry *new_file;

	if ((new_file = create_proc_entry(name, S_IFREG | mode, dir)) != NULL) {
		new_file->data = (void *)new_file;
		new_file->read_proc = &comxlapb_read_proc;
		new_file->write_proc = &comxlapb_write_proc;
		new_file->size = size;
		new_file->nlink = 1;
	}
	return(new_file);
}

static struct comx_protocol comxlapb_protocol = {
	"lapb", 
	VERSION,
	ARPHRD_LAPB, 
	comxlapb_init_lapb, 
	comxlapb_exit, 
	NULL 
};

static struct comx_protocol comx25_protocol = {
	"x25", 
	VERSION,
	ARPHRD_X25, 
	comxlapb_init_x25, 
	comxlapb_exit, 
	NULL 
};

int __init comx_proto_lapb_init(void)
{
	int ret;

	if ((ret = comx_register_protocol(&comxlapb_protocol)) != 0) {
		return ret;
	}
	return comx_register_protocol(&comx25_protocol);
}

static void __exit comx_proto_lapb_exit(void)
{
	comx_unregister_protocol(comxlapb_protocol.name);
	comx_unregister_protocol(comx25_protocol.name);
}

#ifdef MODULE
module_init(comx_proto_lapb_init);
#endif
module_exit(comx_proto_lapb_exit);

MODULE_LICENSE("GPL");
