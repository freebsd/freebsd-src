/*
 * Frame-relay protocol module for the COMX driver 
 * for Linux 2.2.X
 *
 * Original author: Tivadar Szemethy <tiv@itc.hu>
 * Maintainer: Gergely Madarasz <gorgo@itc.hu>
 *
 * Copyright (C) 1998-1999 ITConsult-Pro Co. <info@itc.hu>
 * 
 * Contributors:
 * Arnaldo Carvalho de Melo <acme@conectiva.com.br> (0.73)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * Version 0.70 (99/06/14):
 *		- cleaned up the source code a bit
 *		- ported back to kernel, now works as builtin code 
 *
 * Version 0.71 (99/06/25):
 *		- use skb priorities and queues for sending keepalive
 * 		- use device queues for slave->master data transmit
 *		- set IFF_RUNNING only line protocol up
 *		- fixes on slave device flags
 * 
 * Version 0.72 (99/07/09):
 *		- handle slave tbusy with master tbusy (should be fixed)
 *		- fix the keepalive timer addition/deletion
 *
 * Version 0.73 (00/08/15)
 * 		- resource release on failure at fr_master_init and
 *		  fr_slave_init 		  
 */

#define VERSION "0.73"

#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <linux/if_arp.h>
#include <linux/inetdevice.h>
#include <linux/pkt_sched.h>
#include <linux/init.h>
#include <asm/uaccess.h>

#include "comx.h"
#include "comxhw.h"

MODULE_AUTHOR("Author: Tivadar Szemethy <tiv@itc.hu>");
MODULE_DESCRIPTION("Frame Relay protocol implementation for the COMX drivers"
	"for Linux kernel 2.4.X");
MODULE_LICENSE("GPL");

#define	FRAD_UI		0x03
#define	NLPID_IP	0xcc
#define	NLPID_Q933_LMI	0x08
#define	NLPID_CISCO_LMI	0x09	
#define Q933_ENQ	0x75
#define	Q933_LINESTAT	0x51
#define	Q933_COUNTERS	0x53

#define	MAXALIVECNT	3		/* No. of failures */

struct fr_data {
	u16	dlci;
	struct	net_device *master;
	char	keepa_pend;
	char	keepa_freq;
	char	keepalivecnt, keeploopcnt;
	struct	timer_list keepa_timer;
	u8	local_cnt, remote_cnt;
};

static struct comx_protocol fr_master_protocol;
static struct comx_protocol fr_slave_protocol;
static struct comx_hardware fr_dlci;

static void fr_keepalive_send(struct net_device *dev) 
{
	struct comx_channel *ch = dev->priv;
	struct fr_data *fr = ch->LINE_privdata;
	struct sk_buff *skb;
	u8 *fr_packet;
	
	skb=alloc_skb(dev->hard_header_len + 13, GFP_ATOMIC);
	
	if(skb==NULL)
		return;
               
        skb_reserve(skb, dev->hard_header_len);
        
        fr_packet=(u8*)skb_put(skb, 13);
                 
	fr_packet[0] = (fr->dlci & (1024 - 15)) >> 2;
	fr_packet[1] = (fr->dlci & 15) << 4 | 1;	// EA bit 1
	fr_packet[2] = FRAD_UI;
	fr_packet[3] = NLPID_Q933_LMI;
	fr_packet[4] = 0;
	fr_packet[5] = Q933_ENQ;
	fr_packet[6] = Q933_LINESTAT;
	fr_packet[7] = 0x01;
	fr_packet[8] = 0x01;
	fr_packet[9] = Q933_COUNTERS;
	fr_packet[10] = 0x02;
	fr_packet[11] = ++fr->local_cnt;
	fr_packet[12] = fr->remote_cnt;

	skb->dev = dev;
	skb->priority = TC_PRIO_CONTROL;
	dev_queue_xmit(skb);
}

static void fr_keepalive_timerfun(unsigned long d) 
{
	struct net_device *dev = (struct net_device *)d;
	struct comx_channel *ch = dev->priv;
	struct fr_data *fr = ch->LINE_privdata;
	struct proc_dir_entry *dir = ch->procdir->parent->subdir;
	struct comx_channel *sch;
	struct fr_data *sfr;
	struct net_device *sdev;

	if (ch->init_status & LINE_OPEN) {
		if (fr->keepalivecnt == MAXALIVECNT) {
			comx_status(dev, ch->line_status & ~PROTO_UP);
			dev->flags &= ~IFF_RUNNING;
			for (; dir ; dir = dir->next) {
				if(!S_ISDIR(dir->mode)) {
				    continue;
				}
	
				if ((sdev = dir->data) && (sch = sdev->priv) && 
				    (sdev->type == ARPHRD_DLCI) && 
				    (sfr = sch->LINE_privdata) 
				    && (sfr->master == dev) && 
				    (sdev->flags & IFF_UP)) {
					sdev->flags &= ~IFF_RUNNING;
					comx_status(sdev, 
						sch->line_status & ~PROTO_UP);
				}
			}
		}
		if (fr->keepalivecnt <= MAXALIVECNT) {
			++fr->keepalivecnt;
		}
		fr_keepalive_send(dev);
	}
	mod_timer(&fr->keepa_timer, jiffies + HZ * fr->keepa_freq);
}

static void fr_rx_lmi(struct net_device *dev, struct sk_buff *skb, 
	u16 dlci, u8 nlpid) 
{
	struct comx_channel *ch = dev->priv;
	struct fr_data *fr = ch->LINE_privdata;
	struct proc_dir_entry *dir = ch->procdir->parent->subdir;
	struct comx_channel *sch;
	struct fr_data *sfr;
	struct net_device *sdev;

	if (dlci != fr->dlci || nlpid != NLPID_Q933_LMI || !fr->keepa_freq) {
		return;
	}

	fr->remote_cnt = skb->data[7];
	if (skb->data[8] == fr->local_cnt) { // keepalive UP!
		fr->keepalivecnt = 0;
		if ((ch->line_status & LINE_UP) && 
		    !(ch->line_status & PROTO_UP)) {
			comx_status(dev, ch->line_status |= PROTO_UP);
			dev->flags |= IFF_RUNNING;
			for (; dir ; dir = dir->next) {
				if(!S_ISDIR(dir->mode)) {
				    continue;
				}
	
				if ((sdev = dir->data) && (sch = sdev->priv) && 
				    (sdev->type == ARPHRD_DLCI) && 
				    (sfr = sch->LINE_privdata) 
				    && (sfr->master == dev) && 
				    (sdev->flags & IFF_UP)) {
					sdev->flags |= IFF_RUNNING;
					comx_status(sdev, 
						sch->line_status | PROTO_UP);
				}
			}
		}
	}
}

static void fr_set_keepalive(struct net_device *dev, int keepa) 
{
	struct comx_channel *ch = dev->priv;
	struct fr_data *fr = ch->LINE_privdata;

	if (!keepa && fr->keepa_freq) { // switch off
		fr->keepa_freq = 0;
		if (ch->line_status & LINE_UP) {
			comx_status(dev, ch->line_status | PROTO_UP);
			dev->flags |= IFF_RUNNING;
			del_timer(&fr->keepa_timer);
		}
		return;
	}

	if (keepa) { // bekapcs
		if(fr->keepa_freq && (ch->line_status & LINE_UP)) {
			del_timer(&fr->keepa_timer);
		}
		fr->keepa_freq = keepa;
		fr->local_cnt = fr->remote_cnt = 0;
		fr->keepa_timer.expires = jiffies + HZ;
		fr->keepa_timer.function = fr_keepalive_timerfun;
		fr->keepa_timer.data = (unsigned long)dev;
		ch->line_status &= ~(PROTO_UP | PROTO_LOOP);
		dev->flags &= ~IFF_RUNNING;
		comx_status(dev, ch->line_status);
		if(ch->line_status & LINE_UP) {
			add_timer(&fr->keepa_timer);
		}
	}
}

static void fr_rx(struct net_device *dev, struct sk_buff *skb) 
{
	struct comx_channel *ch = dev->priv;
	struct proc_dir_entry *dir = ch->procdir->parent->subdir;
	struct net_device *sdev = dev;
	struct comx_channel *sch;
	struct fr_data *sfr;
	u16 dlci;
	u8 nlpid;

	if(skb->len <= 4 || skb->data[2] != FRAD_UI) {
		kfree_skb(skb);
		return;
	}

	/* Itt majd ki kell talalni, melyik slave kapja a csomagot */
	dlci = ((skb->data[0] & 0xfc) << 2) | ((skb->data[1] & 0xf0) >> 4);
	if ((nlpid = skb->data[3]) == 0) { // Optional padding 
		nlpid = skb->data[4];
		skb_pull(skb, 1);
	}
	skb_pull(skb, 4);	/* DLCI and header throw away */

	if (ch->debug_flags & DEBUG_COMX_DLCI) {
		comx_debug(dev, "Frame received, DLCI: %d, NLPID: 0x%02x\n", 
			dlci, nlpid);
		comx_debug_skb(dev, skb, "Contents");
	}

	/* Megkeressuk, kihez tartozik */
	for (; dir ; dir = dir->next) {
		if(!S_ISDIR(dir->mode)) {
			continue;
		}
		if ((sdev = dir->data) && (sch = sdev->priv) && 
		    (sdev->type == ARPHRD_DLCI) && (sfr = sch->LINE_privdata) &&
		    (sfr->master == dev) && (sfr->dlci == dlci)) {
			skb->dev = sdev;	
			if (ch->debug_flags & DEBUG_COMX_DLCI) {
				comx_debug(dev, "Passing it to %s\n",sdev->name);
			}
			if (dev != sdev) {
				sch->stats.rx_packets++;
				sch->stats.rx_bytes += skb->len;
			}
			break;
		}
	}
	switch(nlpid) {
		case NLPID_IP:
			skb->protocol = htons(ETH_P_IP);
			skb->mac.raw = skb->data;
			comx_rx(sdev, skb);
			break;
		case NLPID_Q933_LMI:
			fr_rx_lmi(dev, skb, dlci, nlpid);
		default:
			kfree_skb(skb);
			break;
	}
}

static int fr_tx(struct net_device *dev) 
{
	struct comx_channel *ch = dev->priv;
	struct proc_dir_entry *dir = ch->procdir->parent->subdir;
	struct net_device *sdev;
	struct comx_channel *sch;
	struct fr_data *sfr;
	int cnt = 1;

	/* Ha minden igaz, 2 helyen fog allni a tbusy: a masternel, 
	   es annal a slave-nel aki eppen kuldott.
	   Egy helyen akkor all, ha a master kuldott.
	   Ez megint jo lesz majd, ha utemezni akarunk */
	   
	/* This should be fixed, the slave tbusy should be set when 
	   the masters queue is full and reset when not */

	for (; dir ; dir = dir->next) {
		if(!S_ISDIR(dir->mode)) {
		    continue;
		}
		if ((sdev = dir->data) && (sch = sdev->priv) && 
		    (sdev->type == ARPHRD_DLCI) && (sfr = sch->LINE_privdata) &&
		    (sfr->master == dev) && (netif_queue_stopped(sdev))) {
		    	netif_wake_queue(sdev);
			cnt++;
		}
	}

	netif_wake_queue(dev);
	return 0;
}

static void fr_status(struct net_device *dev, unsigned short status)
{
	struct comx_channel *ch = dev->priv;
	struct fr_data *fr = ch->LINE_privdata;
	struct proc_dir_entry *dir = ch->procdir->parent->subdir;
	struct net_device *sdev;
	struct comx_channel *sch;
	struct fr_data *sfr;

	if (status & LINE_UP) {
		if (!fr->keepa_freq) {
			status |= PROTO_UP;
		}
	} else {
		status &= ~(PROTO_UP | PROTO_LOOP);
	}

	if (dev == fr->master && fr->keepa_freq) {
		if (status & LINE_UP) {
			fr->keepa_timer.expires = jiffies + HZ;
			add_timer(&fr->keepa_timer);
			fr->keepalivecnt = MAXALIVECNT + 1;
			fr->keeploopcnt = 0;
		} else {
			del_timer(&fr->keepa_timer);
		}
	}
		
	/* Itt a status valtozast vegig kell vinni az osszes slave-n */
	for (; dir ; dir = dir->next) {
		if(!S_ISDIR(dir->mode)) {
		    continue;
		}
	
		if ((sdev = dir->data) && (sch = sdev->priv) && 
		    (sdev->type == ARPHRD_FRAD || sdev->type == ARPHRD_DLCI) && 
		    (sfr = sch->LINE_privdata) && (sfr->master == dev)) {
			if(status & LINE_UP) {
				netif_wake_queue(sdev);
			}
			comx_status(sdev, status);
			if(status & (PROTO_UP | PROTO_LOOP)) {
				dev->flags |= IFF_RUNNING;
			} else {
				dev->flags &= ~IFF_RUNNING;
			}
		}
	}
}

static int fr_open(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	struct fr_data *fr = ch->LINE_privdata;
	struct proc_dir_entry *comxdir = ch->procdir;
	struct comx_channel *mch;

	if (!(ch->init_status & HW_OPEN)) {
		return -ENODEV;
	}

	if ((ch->hardware == &fr_dlci && ch->protocol != &fr_slave_protocol) ||
	    (ch->protocol == &fr_slave_protocol && ch->hardware != &fr_dlci)) {
		printk(KERN_ERR "Trying to open an improperly set FR interface, giving up\n");
		return -EINVAL;
	}

	if (!fr->master) {
		return -ENODEV;
	}
	mch = fr->master->priv;
	if (fr->master != dev && (!(mch->init_status & LINE_OPEN) 
	   || (mch->protocol != &fr_master_protocol))) {
		printk(KERN_ERR "Master %s is inactive, or incorrectly set up, "
			"unable to open %s\n", fr->master->name, dev->name);
		return -ENODEV;
	}

	ch->init_status |= LINE_OPEN;
	ch->line_status &= ~(PROTO_UP | PROTO_LOOP);
	dev->flags &= ~IFF_RUNNING;

	if (fr->master == dev) {
		if (fr->keepa_freq) {
			fr->keepa_timer.function = fr_keepalive_timerfun;
			fr->keepa_timer.data = (unsigned long)dev;
			add_timer(&fr->keepa_timer);
		} else {
			if (ch->line_status & LINE_UP) {
				ch->line_status |= PROTO_UP;
				dev->flags |= IFF_RUNNING;
			}
		}
	} else {
		ch->line_status = mch->line_status;
		if(fr->master->flags & IFF_RUNNING) {
			dev->flags |= IFF_RUNNING;
		}
	}

	for (; comxdir ; comxdir = comxdir->next) {
		if (strcmp(comxdir->name, FILENAME_DLCI) == 0 ||
		   strcmp(comxdir->name, FILENAME_MASTER) == 0 ||
		   strcmp(comxdir->name, FILENAME_KEEPALIVE) == 0) {
			comxdir->mode = S_IFREG | 0444;
		}
	}
//	comx_status(dev, ch->line_status);
	return 0;
}

static int fr_close(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	struct fr_data *fr = ch->LINE_privdata;
	struct proc_dir_entry *comxdir = ch->procdir;

	if (fr->master == dev) { // Ha master 
		struct proc_dir_entry *dir = ch->procdir->parent->subdir;
		struct net_device *sdev = dev;
		struct comx_channel *sch;
		struct fr_data *sfr;

		if (!(ch->init_status & HW_OPEN)) {
			return -ENODEV;
		}

		if (fr->keepa_freq) {
			del_timer(&fr->keepa_timer);
		}
		
		for (; dir ; dir = dir->next) {
			if(!S_ISDIR(dir->mode)) {
				continue;
			}
			if ((sdev = dir->data) && (sch = sdev->priv) && 
			    (sdev->type == ARPHRD_DLCI) && 
			    (sfr = sch->LINE_privdata) &&
			    (sfr->master == dev) && 
			    (sch->init_status & LINE_OPEN)) {
				dev_close(sdev);
			}
		}
	}

	ch->init_status &= ~LINE_OPEN;
	ch->line_status &= ~(PROTO_UP | PROTO_LOOP);
	dev->flags &= ~IFF_RUNNING;

	for (; comxdir ; comxdir = comxdir->next) {
		if (strcmp(comxdir->name, FILENAME_DLCI) == 0 ||
		    strcmp(comxdir->name, FILENAME_MASTER) == 0 ||
		    strcmp(comxdir->name, FILENAME_KEEPALIVE) == 0) {
			comxdir->mode = S_IFREG | 0444;
		}
	}

	return 0;
}

static int fr_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	struct comx_channel *sch, *mch;
	struct fr_data *fr = ch->LINE_privdata;
	struct fr_data *sfr;
	struct net_device *sdev;
	struct proc_dir_entry *dir = ch->procdir->parent->subdir;

	if (!fr->master) {
		printk(KERN_ERR "BUG: fr_xmit without a master!!! dev: %s\n", dev->name);
		return 0;
	}

	mch = fr->master->priv;

	/* Ennek majd a slave utemezeskor lesz igazan jelentosege */
	if (ch->debug_flags & DEBUG_COMX_DLCI) {
		comx_debug_skb(dev, skb, "Sending frame");
	}

	if (dev != fr->master) {
		struct sk_buff *newskb=skb_clone(skb, GFP_ATOMIC);
		if (!newskb)
			return -ENOMEM;
		newskb->dev=fr->master;
		dev_queue_xmit(newskb);
		ch->stats.tx_bytes += skb->len;
		ch->stats.tx_packets++;
		dev_kfree_skb(skb);
	} else {
		netif_stop_queue(dev);
		for (; dir ; dir = dir->next) {
			if(!S_ISDIR(dir->mode)) {
			    continue;
			}
			if ((sdev = dir->data) && (sch = sdev->priv) && 
			    (sdev->type == ARPHRD_DLCI) && (sfr = sch->LINE_privdata) &&
			    (sfr->master == dev) && (netif_queue_stopped(sdev))) {
				netif_stop_queue(sdev);
			}
		}
		 	
		switch(mch->HW_send_packet(dev, skb)) {
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
	}
	return 0;
}

static int fr_header(struct sk_buff *skb, struct net_device *dev, 
	unsigned short type, void *daddr, void *saddr, unsigned len) 
{
	struct comx_channel *ch = dev->priv;
	struct fr_data *fr = ch->LINE_privdata;

	skb_push(skb, dev->hard_header_len);	  
	/* Put in DLCI */
	skb->data[0] = (fr->dlci & (1024 - 15)) >> 2;
	skb->data[1] = (fr->dlci & 15) << 4 | 1;	// EA bit 1
	skb->data[2] = FRAD_UI;
	skb->data[3] = NLPID_IP;

	return dev->hard_header_len;  
}

static int fr_statistics(struct net_device *dev, char *page) 
{
	struct comx_channel *ch = dev->priv;
	struct fr_data *fr = ch->LINE_privdata;
	int len = 0;

	if (fr->master == dev) {
		struct proc_dir_entry *dir = ch->procdir->parent->subdir;
		struct net_device *sdev;
		struct comx_channel *sch;
		struct fr_data *sfr;
		int slaves = 0;

		len += sprintf(page + len, 
			"This is a Frame Relay master device\nSlaves: ");
		for (; dir ; dir = dir->next) {
			if(!S_ISDIR(dir->mode)) {
				continue;
			}
			if ((sdev = dir->data) && (sch = sdev->priv) && 
			    (sdev->type == ARPHRD_DLCI) &&
			    (sfr = sch->LINE_privdata) && 
			    (sfr->master == dev) && (sdev != dev)) {
				slaves++;
				len += sprintf(page + len, "%s ", sdev->name);
			}
		}
		len += sprintf(page + len, "%s\n", slaves ? "" : "(none)");
		if (fr->keepa_freq) {
			len += sprintf(page + len, "Line keepalive (value %d) "
				"status %s [%d]\n", fr->keepa_freq, 
				ch->line_status & PROTO_LOOP ? "LOOP" :
				ch->line_status & PROTO_UP ? "UP" : "DOWN", 
				fr->keepalivecnt);
		} else {
			len += sprintf(page + len, "Line keepalive protocol "
				"is not set\n");
		}
	} else {		// if slave
		len += sprintf(page + len, 
			"This is a Frame Relay slave device, master: %s\n",
			fr->master ? fr->master->name : "(not set)");
	}
	return len;
}

static int fr_read_proc(char *page, char **start, off_t off, int count,
	int *eof, void *data)
{
	struct proc_dir_entry *file = (struct proc_dir_entry *)data;
	struct net_device *dev = file->parent->data;
	struct comx_channel *ch = dev->priv;
	struct fr_data *fr = NULL;
	int len = 0;

	if (ch) {
		fr = ch->LINE_privdata;
	}

	if (strcmp(file->name, FILENAME_DLCI) == 0) {
		len = sprintf(page, "%04d\n", fr->dlci);
	} else if (strcmp(file->name, FILENAME_MASTER) == 0) {
		len = sprintf(page, "%-9s\n", fr->master ? fr->master->name :
			"(none)");
	} else if (strcmp(file->name, FILENAME_KEEPALIVE) == 0) {
		len = fr->keepa_freq ? sprintf(page, "% 3d\n", fr->keepa_freq) 
			: sprintf(page, "off\n");
	} else {
		printk(KERN_ERR "comxfr: internal error, filename %s\n", file->name);
		return -EBADF;
	}

	if (off >= len) {
		*eof = 1;
		return 0;
	}

	*start = page + off;
	if (count >= len - off) *eof = 1;
	return min_t(int, count, len - off);
}

static int fr_write_proc(struct file *file, const char *buffer, 
	u_long count, void *data)
{
	struct proc_dir_entry *entry = (struct proc_dir_entry *)data;
	struct net_device *dev = entry->parent->data;
	struct comx_channel *ch = dev->priv;
	struct fr_data *fr = NULL; 
	char *page;

	if (ch) {
		fr = ch->LINE_privdata;
	}

	if (!(page = (char *)__get_free_page(GFP_KERNEL))) {
		return -ENOMEM;
	}

	copy_from_user(page, buffer, count);
	if (*(page + count - 1) == '\n') {
		*(page + count - 1) = 0;
	}

	if (strcmp(entry->name, FILENAME_DLCI) == 0) {
		u16 dlci_new = simple_strtoul(page, NULL, 10);

		if (dlci_new > 1023) {
			printk(KERN_ERR "Invalid DLCI value\n");
		}
		else fr->dlci = dlci_new;
	} else if (strcmp(entry->name, FILENAME_MASTER) == 0) {
		struct net_device *new_master = dev_get_by_name(page);

		if (new_master && new_master->type == ARPHRD_FRAD) {
			struct comx_channel *sch = new_master->priv;
			struct fr_data *sfr = sch->LINE_privdata;

			if (sfr && sfr->master == new_master) {
				if(fr->master)
					dev_put(fr->master);
				fr->master = new_master;
				/* Megorokli a master statuszat */
				ch->line_status = sch->line_status;
			}
		}
	} else if (strcmp(entry->name, FILENAME_KEEPALIVE) == 0) {
		int keepa_new = -1;

		if (strcmp(page, KEEPALIVE_OFF) == 0) {
			keepa_new = 0;
		} else {
			keepa_new = simple_strtoul(page, NULL, 10);
		}

		if (keepa_new < 0 || keepa_new > 100) {
			printk(KERN_ERR "invalid keepalive\n");
		} else {
			if (fr->keepa_freq && keepa_new != fr->keepa_freq) {
				fr_set_keepalive(dev, 0);
			}
			if (keepa_new) {
				fr_set_keepalive(dev, keepa_new);
			}
		}
	} else {
		printk(KERN_ERR "comxfr_write_proc: internal error, filename %s\n", 
			entry->name);
		count = -EBADF;
	}

	free_page((unsigned long)page);
	return count;
}

static int fr_exit(struct net_device *dev) 
{
	struct comx_channel *ch = dev->priv;
	struct fr_data *fr = ch->LINE_privdata;
	struct net_device *sdev = dev;
	struct comx_channel *sch;
	struct fr_data *sfr;
	struct proc_dir_entry *dir = ch->procdir->parent->subdir;

	/* Ha lezarunk egy master-t, le kell kattintani a slave-eket is */
	if (fr->master && fr->master == dev) {
		for (; dir ; dir = dir->next) {
			if(!S_ISDIR(dir->mode)) {
				continue;
			}
			if ((sdev = dir->data) && (sch = sdev->priv) && 
			    (sdev->type == ARPHRD_DLCI) && 
			    (sfr = sch->LINE_privdata) && (sfr->master == dev)) {
				dev_close(sdev);
				sfr->master = NULL;
			}
		}
	}
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
	ch->LINE_rebuild_header	= NULL;
	ch->LINE_statistics = NULL;

	ch->LINE_status = 0;

	if (fr->master != dev) { // if not master, remove dlci
		if(fr->master)
			dev_put(fr->master);
		remove_proc_entry(FILENAME_DLCI, ch->procdir);
		remove_proc_entry(FILENAME_MASTER, ch->procdir);
	} else {
		if (fr->keepa_freq) {
			fr_set_keepalive(dev, 0);
		}
		remove_proc_entry(FILENAME_KEEPALIVE, ch->procdir);
		remove_proc_entry(FILENAME_DLCI, ch->procdir);
	}

	kfree(fr);
	ch->LINE_privdata = NULL;

	MOD_DEC_USE_COUNT;
	return 0;
}

static int fr_master_init(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	struct fr_data *fr;
	struct proc_dir_entry *new_file;

	if ((fr = ch->LINE_privdata = kmalloc(sizeof(struct fr_data), 
	    GFP_KERNEL)) == NULL) {
		return -ENOMEM;
	}
	memset(fr, 0, sizeof(struct fr_data));
	fr->master = dev;	// this means master
	fr->dlci = 0;		// let's say default

	dev->flags	= IFF_POINTOPOINT | IFF_NOARP | IFF_MULTICAST;
	dev->type	= ARPHRD_FRAD;
	dev->mtu	= 1500;
	dev->hard_header_len    = 4;		
	dev->addr_len	= 0;

	ch->LINE_rx	= fr_rx;
	ch->LINE_tx	= fr_tx;
	ch->LINE_status = fr_status;
	ch->LINE_open	= fr_open;
	ch->LINE_close	= fr_close;
	ch->LINE_xmit	= fr_xmit;
	ch->LINE_header	= fr_header;
	ch->LINE_rebuild_header	= NULL;
	ch->LINE_statistics = fr_statistics;

	if ((new_file = create_proc_entry(FILENAME_DLCI, S_IFREG | 0644, 
	    ch->procdir)) == NULL) {
		goto cleanup_LINE_privdata;
	}
	new_file->data = (void *)new_file;
	new_file->read_proc = &fr_read_proc;
	new_file->write_proc = &fr_write_proc;
	new_file->size = 5;
	new_file->nlink = 1;

	if ((new_file = create_proc_entry(FILENAME_KEEPALIVE, S_IFREG | 0644, 
	    ch->procdir)) == NULL) {
		goto cleanup_filename_dlci;
	}
	new_file->data = (void *)new_file;
	new_file->read_proc = &fr_read_proc;
	new_file->write_proc = &fr_write_proc;
	new_file->size = 4;
	new_file->nlink = 1;

	fr_set_keepalive(dev, 0);

	MOD_INC_USE_COUNT;
	return 0;
cleanup_filename_dlci:
	 remove_proc_entry(FILENAME_DLCI, ch->procdir);
cleanup_LINE_privdata:
	kfree(fr);
	return -EIO;
}

static int fr_slave_init(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	struct fr_data *fr;
	struct proc_dir_entry *new_file;

	if ((fr = ch->LINE_privdata = kmalloc(sizeof(struct fr_data), 
	    GFP_KERNEL)) == NULL) {
		return -ENOMEM;
	}
	memset(fr, 0, sizeof(struct fr_data));

	dev->flags	= IFF_POINTOPOINT | IFF_NOARP | IFF_MULTICAST;
	dev->type	= ARPHRD_DLCI;
	dev->mtu	= 1500;
	dev->hard_header_len    = 4;		
	dev->addr_len	= 0;

	ch->LINE_rx	= fr_rx;
	ch->LINE_tx	= fr_tx;
	ch->LINE_status = fr_status;
	ch->LINE_open	= fr_open;
	ch->LINE_close	= fr_close;
	ch->LINE_xmit	= fr_xmit;
	ch->LINE_header	= fr_header;
	ch->LINE_rebuild_header	= NULL;
	ch->LINE_statistics = fr_statistics;

	if ((new_file = create_proc_entry(FILENAME_DLCI, S_IFREG | 0644, 
	    ch->procdir)) == NULL) {
		goto cleanup_LINE_privdata;
	}
	
	new_file->data = (void *)new_file;
	new_file->read_proc = &fr_read_proc;
	new_file->write_proc = &fr_write_proc;
	new_file->size = 5;
	new_file->nlink = 1;

	if ((new_file = create_proc_entry(FILENAME_MASTER, S_IFREG | 0644, 
	    ch->procdir)) == NULL) {
		goto cleanup_filename_dlci;
	}
	new_file->data = (void *)new_file;
	new_file->read_proc = &fr_read_proc;
	new_file->write_proc = &fr_write_proc;
	new_file->size = 10;
	new_file->nlink = 1;
	MOD_INC_USE_COUNT;
	return 0;
cleanup_filename_dlci:
         remove_proc_entry(FILENAME_DLCI, ch->procdir);
cleanup_LINE_privdata:
	kfree(fr);
	return -EIO;
}

static int dlci_open(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;

	ch->init_status |= HW_OPEN;

	MOD_INC_USE_COUNT;
	return 0;
}

static int dlci_close(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;

	ch->init_status &= ~HW_OPEN;

	MOD_DEC_USE_COUNT;
	return 0;
}

static int dlci_txe(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	struct fr_data *fr = ch->LINE_privdata;

	if (!fr->master) {
		return 0;
	}

	ch = fr->master->priv;
	fr = ch->LINE_privdata;
	return ch->HW_txe(fr->master);
}

static int dlci_statistics(struct net_device *dev, char *page) 
{
	return 0;
}

static int dlci_init(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;

	ch->HW_open = dlci_open;
	ch->HW_close = dlci_close;
	ch->HW_txe = dlci_txe;
	ch->HW_statistics = dlci_statistics;

	/* Nincs egyeb hw info, mert ugyis a fr->master-bol fog minden kiderulni */

	MOD_INC_USE_COUNT;
	return 0;
}

static int dlci_exit(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;

	ch->HW_open = NULL;
	ch->HW_close = NULL;
	ch->HW_txe = NULL;
	ch->HW_statistics = NULL;

	MOD_DEC_USE_COUNT;
	return 0;
}

static int dlci_dump(struct net_device *dev)
{
	printk(KERN_INFO "dlci_dump %s, HOGY MI ???\n", dev->name);
	return -1;
}

static struct comx_protocol fr_master_protocol = {
	name:		"frad", 
	version:	VERSION,
	encap_type:	ARPHRD_FRAD, 
	line_init:	fr_master_init, 
	line_exit:	fr_exit, 
};

static struct comx_protocol fr_slave_protocol = {
	name:		"ietf-ip", 
	version:	VERSION,
	encap_type:	ARPHRD_DLCI, 
	line_init:	fr_slave_init, 
	line_exit:	fr_exit, 
};

static struct comx_hardware fr_dlci = { 
	name:		"dlci", 
	version:	VERSION,
	hw_init:	dlci_init, 
	hw_exit:	dlci_exit, 
	hw_dump:	dlci_dump, 
};

#ifdef MODULE
#define comx_proto_fr_init init_module
#endif

int __init comx_proto_fr_init(void)
{
	int ret; 

	if ((ret = comx_register_hardware(&fr_dlci))) {
		return ret;
	}
	if ((ret = comx_register_protocol(&fr_master_protocol))) {
		return ret;
	}
	return comx_register_protocol(&fr_slave_protocol);
}

#ifdef MODULE
void cleanup_module(void)
{
	comx_unregister_hardware(fr_dlci.name);
	comx_unregister_protocol(fr_master_protocol.name);
	comx_unregister_protocol(fr_slave_protocol.name);
}
#endif /* MODULE */

