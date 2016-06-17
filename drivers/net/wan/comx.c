/*
 * Device driver framework for the COMX line of synchronous serial boards
 * 
 * for Linux kernel 2.2.X / 2.4.X
 *
 * Original authors:  Arpad Bakay <bakay.arpad@synergon.hu>,
 *                    Peter Bajan <bajan.peter@synergon.hu>,
 * Previous maintainer: Tivadar Szemethy <tiv@itc.hu>
 * Current maintainer: Gergely Madarasz <gorgo@itc.hu>
 *
 * Copyright (C) 1995-1999 ITConsult-Pro Co.
 *
 * Contributors:
 * Arnaldo Carvalho de Melo <acme@conectiva.com.br> (0.85)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * Version 0.80 (99/06/11):
 *		- clean up source code (playing a bit of indent)
 *		- port back to kernel, add support for non-module versions
 *		- add support for board resets when channel protocol is down
 *		- reset the device structure after protocol exit
 *		  the syncppp driver needs it
 *		- add support for /proc/comx/protocols and 
 *		  /proc/comx/boardtypes
 *
 * Version 0.81 (99/06/21):
 *		- comment out the board reset support code, the locomx
 *		  driver seems not buggy now
 *		- printk() levels fixed
 *
 * Version 0.82 (99/07/08):
 *		- Handle stats correctly if the lowlevel driver is
 *		  is not a comx one (locomx - z85230)
 *
 * Version 0.83 (99/07/15):
 *		- reset line_status when interface is down
 *
 * Version 0.84 (99/12/01):
 *		- comx_status should not check for IFF_UP (to report
 *		  line status from dev->open())
 *
 * Version 0.85 (00/08/15):
 * 		- resource release on failure in comx_mkdir
 * 		- fix return value on failure at comx_write_proc
 *
 * Changed      (00/10/29, Henner Eisen):
 * 		- comx_rx() / comxlapb_data_indication() return status.
 */

#define VERSION "0.85"

#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/ctype.h>
#include <linux/init.h>

#ifdef CONFIG_KMOD
#include <linux/kmod.h>
#endif

#ifndef CONFIG_PROC_FS
#error For now, COMX really needs the /proc filesystem
#endif

#include <net/syncppp.h>
#include "comx.h"

MODULE_AUTHOR("Gergely Madarasz <gorgo@itc.hu>");
MODULE_DESCRIPTION("Common code for the COMX synchronous serial adapters");
MODULE_LICENSE("GPL");

extern int comx_hw_comx_init(void);
extern int comx_hw_locomx_init(void);
extern int comx_hw_mixcom_init(void);
extern int comx_proto_hdlc_init(void);
extern int comx_proto_ppp_init(void);
extern int comx_proto_syncppp_init(void);
extern int comx_proto_lapb_init(void);
extern int comx_proto_fr_init(void);

static struct comx_hardware *comx_channels = NULL;
static struct comx_protocol *comx_lines = NULL;

static int comx_mkdir(struct inode *, struct dentry *, int);
static int comx_rmdir(struct inode *, struct dentry *);
static struct dentry *comx_lookup(struct inode *, struct dentry *);

static struct inode_operations comx_root_inode_ops = {
	lookup:	comx_lookup,
	mkdir: comx_mkdir,
	rmdir: comx_rmdir,
};

static int comx_delete_dentry(struct dentry *dentry);
static struct proc_dir_entry *create_comx_proc_entry(char *name, int mode,
	int size, struct proc_dir_entry *dir);

static struct dentry_operations comx_dentry_operations = {
	d_delete:	comx_delete_dentry,
};


static struct proc_dir_entry * comx_root_dir;

struct comx_debugflags_struct	comx_debugflags[] = {
	{ "comx_rx",		DEBUG_COMX_RX		},
	{ "comx_tx", 		DEBUG_COMX_TX		},
	{ "hw_tx",		DEBUG_HW_TX		},
	{ "hw_rx", 		DEBUG_HW_RX		},
	{ "hdlc_keepalive",	DEBUG_HDLC_KEEPALIVE	},
	{ "comxppp",		DEBUG_COMX_PPP		},
	{ "comxlapb",		DEBUG_COMX_LAPB		},
	{ "dlci",		DEBUG_COMX_DLCI		},
	{ NULL,			0			} 
};


int comx_debug(struct net_device *dev, char *fmt, ...)
{
	struct comx_channel *ch = dev->priv;
	char *page,*str;
	va_list args;
	int len;

	if (!ch->debug_area) return 0;

	if (!(page = (char *)__get_free_page(GFP_ATOMIC))) return -ENOMEM;

	va_start(args, fmt);
	len = vsprintf(str = page, fmt, args);
	va_end(args);

	if (len >= PAGE_SIZE) {
		printk(KERN_ERR "comx_debug: PANIC! len = %d !!!\n", len);
		free_page((unsigned long)page);
		return -EINVAL;
	}

	while (len) {
		int to_copy;
		int free = (ch->debug_start - ch->debug_end + ch->debug_size) 
			% ch->debug_size;

		to_copy = min_t(int, free ? free : ch->debug_size, 
			      min_t(int, ch->debug_size - ch->debug_end, len));
		memcpy(ch->debug_area + ch->debug_end, str, to_copy);
		str += to_copy;
		len -= to_copy;
		ch->debug_end = (ch->debug_end + to_copy) % ch->debug_size;
		if (ch->debug_start == ch->debug_end) // Full ? push start away
			ch->debug_start = (ch->debug_start + len + 1) % 
					ch->debug_size;
		ch->debug_file->size = (ch->debug_end - ch->debug_start +
					ch->debug_size) % ch->debug_size;
	} 

	free_page((unsigned long)page);
	return 0;
}

int comx_debug_skb(struct net_device *dev, struct sk_buff *skb, char *msg)
{
	struct comx_channel *ch = dev->priv;

	if (!ch->debug_area) return 0;
	if (!skb) comx_debug(dev, "%s: %s NULL skb\n\n", dev->name, msg);
	if (!skb->len) comx_debug(dev, "%s: %s empty skb\n\n", dev->name, msg);

	return comx_debug_bytes(dev, skb->data, skb->len, msg);
}

int comx_debug_bytes(struct net_device *dev, unsigned char *bytes, int len, 
		char *msg)
{
	int pos = 0;
	struct comx_channel *ch = dev->priv;

	if (!ch->debug_area) return 0;

	comx_debug(dev, "%s: %s len %d\n", dev->name, msg, len);

	while (pos != len) {
		char line[80];
		int i = 0;

		memset(line, 0, 80);
		sprintf(line,"%04d ", pos);
		do {
			sprintf(line + 5 + (pos % 16) * 3, "%02x", bytes[pos]);
			sprintf(line + 60 + (pos % 16), "%c", 
				isprint(bytes[pos]) ? bytes[pos] : '.');
			pos++;
		} while (pos != len && pos % 16);

		while ( i++ != 78 ) if (line[i] == 0) line[i] = ' ';
		line[77] = '\n';
		line[78] = 0;
	
		comx_debug(dev, "%s", line);
	}
	comx_debug(dev, "\n");
	return 0;
}

static void comx_loadavg_timerfun(unsigned long d)
{
	struct net_device *dev = (struct net_device *)d;
	struct comx_channel *ch = dev->priv;

	ch->avg_bytes[ch->loadavg_counter] = ch->current_stats->rx_bytes;
	ch->avg_bytes[ch->loadavg_counter + ch->loadavg_size] = 
		ch->current_stats->tx_bytes;

	ch->loadavg_counter = (ch->loadavg_counter + 1) % ch->loadavg_size;

	mod_timer(&ch->loadavg_timer,jiffies + HZ * ch->loadavg[0]);
}

#if 0
static void comx_reset_timerfun(unsigned long d)
{ 
	struct net_device *dev = (struct net_device *)d;
	struct comx_channel *ch = dev->priv;

	if(!(ch->line_status & (PROTO_LOOP | PROTO_UP))) {
		if(test_and_set_bit(0,&ch->reset_pending) && ch->HW_reset) {
			ch->HW_reset(dev);
		}
	}

	mod_timer(&ch->reset_timer, jiffies + HZ * ch->reset_timeout);
}
#endif                                            

static int comx_open(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	struct proc_dir_entry *comxdir = ch->procdir->subdir;
	int ret=0;

	if (!ch->protocol || !ch->hardware) return -ENODEV;

	if ((ret = ch->HW_open(dev))) return ret;
	if ((ret = ch->LINE_open(dev))) { 
		ch->HW_close(dev); 
		return ret; 
	};

	for (; comxdir ; comxdir = comxdir->next) {
		if (strcmp(comxdir->name, FILENAME_HARDWARE) == 0 ||
		   strcmp(comxdir->name, FILENAME_PROTOCOL) == 0)
			comxdir->mode = S_IFREG | 0444;
	}

#if 0
	ch->reset_pending = 1;
	ch->reset_timeout = 30;
	ch->reset_timer.function = comx_reset_timerfun;
	ch->reset_timer.data = (unsigned long)dev;
	ch->reset_timer.expires = jiffies + HZ * ch->reset_timeout;
	add_timer(&ch->reset_timer);
#endif

	return 0;
}

static int comx_close(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	struct proc_dir_entry *comxdir = ch->procdir->subdir;
	int ret = -ENODEV;

	if (test_and_clear_bit(0, &ch->lineup_pending)) {
		del_timer(&ch->lineup_timer);
	}

#if 0	
	del_timer(&ch->reset_timer);
#endif

	if (ch->init_status & LINE_OPEN && ch->protocol && ch->LINE_close) {
		ret = ch->LINE_close(dev);
	}

	if (ret) return ret;

	if (ch->init_status & HW_OPEN && ch->hardware && ch->HW_close) {
		ret = ch->HW_close(dev);
	}
	
	ch->line_status=0;

	for (; comxdir ; comxdir = comxdir->next) {
		if (strcmp(comxdir->name, FILENAME_HARDWARE) == 0 ||
		    strcmp(comxdir->name, FILENAME_PROTOCOL) == 0)
			comxdir->mode = S_IFREG | 0644;
	}

	return ret;
}

void comx_status(struct net_device *dev, int status)
{
	struct comx_channel *ch = dev->priv;

#if 0
	if(status & (PROTO_UP | PROTO_LOOP)) {
		clear_bit(0,&ch->reset_pending);
	}
#endif

	printk(KERN_NOTICE "Interface %s: modem status %s, line protocol %s\n",
		    dev->name, status & LINE_UP ? "UP" : "DOWN", 
		    status & PROTO_LOOP ? "LOOP" : status & PROTO_UP ? 
		    "UP" : "DOWN");
	
	ch->line_status = status;
}

static int comx_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	int rc;

	if (skb->len > dev->mtu + dev->hard_header_len) {
		printk(KERN_ERR "comx_xmit: %s: skb->len %d > dev->mtu %d\n", dev->name,
		(int)skb->len, dev->mtu);
	}
	
	if (ch->debug_flags & DEBUG_COMX_TX) {
		comx_debug_skb(dev, skb, "comx_xmit skb");
	}
	
	rc=ch->LINE_xmit(skb, dev);
//	if (!rc) dev_kfree_skb(skb);

	return rc;
}

static int comx_header(struct sk_buff *skb, struct net_device *dev, 
	unsigned short type, void *daddr, void *saddr, unsigned len) 
{
	struct comx_channel *ch = dev->priv;

	if (ch->LINE_header) {
		return (ch->LINE_header(skb, dev, type, daddr, saddr, len));
	} else {
		return 0;
	}
}

static int comx_rebuild_header(struct sk_buff *skb) 
{
	struct net_device *dev = skb->dev;
	struct comx_channel *ch = dev->priv;

	if (ch->LINE_rebuild_header) {
		return(ch->LINE_rebuild_header(skb));
	} else {
		return 0;
	}
}

int comx_rx(struct net_device *dev, struct sk_buff *skb)
{
	struct comx_channel *ch = dev->priv;

	if (ch->debug_flags & DEBUG_COMX_RX) {
		comx_debug_skb(dev, skb, "comx_rx skb");
	}
	if (skb) {
		netif_rx(skb);
		dev->last_rx = jiffies;
	}
	return 0;
}

static struct net_device_stats *comx_stats(struct net_device *dev)
{
	struct comx_channel *ch = (struct comx_channel *)dev->priv;

	return ch->current_stats;
}

void comx_lineup_func(unsigned long d)
{
	struct net_device *dev = (struct net_device *)d;
	struct comx_channel *ch = dev->priv;

	del_timer(&ch->lineup_timer);
	clear_bit(0, &ch->lineup_pending);

	if (ch->LINE_status) {
		ch->LINE_status(dev, ch->line_status |= LINE_UP);
	}
}

#define LOADAVG(avg, off) (int) \
	((ch->avg_bytes[(ch->loadavg_counter - 1 + ch->loadavg_size * 2) \
	% ch->loadavg_size + off] -  ch->avg_bytes[(ch->loadavg_counter - 1 \
		- ch->loadavg[avg] / ch->loadavg[0] + ch->loadavg_size * 2) \
		% ch->loadavg_size + off]) / ch->loadavg[avg] * 8)

static int comx_statistics(struct net_device *dev, char *page)
{
	struct comx_channel *ch = dev->priv;
	int len = 0;
	int tmp;
	int i = 0;
	char tmpstr[20];
	int tmpstrlen = 0;

	len += sprintf(page + len, "Interface administrative status is %s, "
		"modem status is %s, protocol is %s\n", 
		dev->flags & IFF_UP ? "UP" : "DOWN",
		ch->line_status & LINE_UP ? "UP" : "DOWN",
		ch->line_status & PROTO_LOOP ? "LOOP" : 
		ch->line_status & PROTO_UP ? "UP" : "DOWN");
	len += sprintf(page + len, "Modem status changes: %lu, Transmitter status "
		"is %s, tbusy: %d\n", ch->current_stats->tx_carrier_errors, ch->HW_txe ? 
		ch->HW_txe(dev) ? "IDLE" : "BUSY" : "NOT READY", netif_running(dev));
	len += sprintf(page + len, "Interface load (input): %d / %d / %d bits/s (",
		LOADAVG(0,0), LOADAVG(1, 0), LOADAVG(2, 0));
	tmpstr[0] = 0;
	for (i=0; i != 3; i++) {
		char tf;

		tf = ch->loadavg[i] % 60 == 0 && 
			ch->loadavg[i] / 60 > 0 ? 'm' : 's';
		tmpstrlen += sprintf(tmpstr + tmpstrlen, "%d%c%s", 
			ch->loadavg[i] / (tf == 'm' ? 60 : 1), tf, 
			i == 2 ? ")\n" : "/");
	}
	len += sprintf(page + len, 
		"%s              (output): %d / %d / %d bits/s (%s", tmpstr, 
		LOADAVG(0,ch->loadavg_size), LOADAVG(1, ch->loadavg_size), 
		LOADAVG(2, ch->loadavg_size), tmpstr);

	len += sprintf(page + len, "Debug flags: ");
	tmp = len; i = 0;
	while (comx_debugflags[i].name) {
		if (ch->debug_flags & comx_debugflags[i].value) 
			len += sprintf(page + len, "%s ", 
				comx_debugflags[i].name);
		i++;
	}
	len += sprintf(page + len, "%s\n", tmp == len ? "none" : "");

	len += sprintf(page + len, "RX errors: len: %lu, overrun: %lu, crc: %lu, "
		"aborts: %lu\n           buffer overrun: %lu, pbuffer overrun: %lu\n"
		"TX errors: underrun: %lu\n",
		ch->current_stats->rx_length_errors, ch->current_stats->rx_over_errors, 
		ch->current_stats->rx_crc_errors, ch->current_stats->rx_frame_errors, 
		ch->current_stats->rx_missed_errors, ch->current_stats->rx_fifo_errors,
		ch->current_stats->tx_fifo_errors);

	if (ch->LINE_statistics && (ch->init_status & LINE_OPEN)) {
		len += ch->LINE_statistics(dev, page + len);
	} else {
		len += sprintf(page+len, "Line status: driver not initialized\n");
	}
	if (ch->HW_statistics && (ch->init_status & HW_OPEN)) {
		len += ch->HW_statistics(dev, page + len);
	} else {
		len += sprintf(page+len, "Board status: driver not initialized\n");
	}

	return len;
}

static int comx_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct comx_channel *ch = dev->priv;

	if (ch->LINE_ioctl) {
		return(ch->LINE_ioctl(dev, ifr, cmd));
	}
	return -EINVAL;
}

static void comx_reset_dev(struct net_device *dev)
{
	dev->open = comx_open;
	dev->stop = comx_close;
	dev->hard_start_xmit = comx_xmit;
	dev->hard_header = comx_header;
	dev->rebuild_header = comx_rebuild_header;
	dev->get_stats = comx_stats;
	dev->do_ioctl = comx_ioctl;
	dev->change_mtu = NULL;
	dev->tx_queue_len = 20;
	dev->flags = IFF_NOARP;
}

static int comx_init_dev(struct net_device *dev)
{
	struct comx_channel *ch;

	if ((ch = kmalloc(sizeof(struct comx_channel), GFP_KERNEL)) == NULL) {
		return -ENOMEM;
	}
	memset(ch, 0, sizeof(struct comx_channel));

	ch->loadavg[0] = 5;
	ch->loadavg[1] = 300;
	ch->loadavg[2] = 900;
	ch->loadavg_size = ch->loadavg[2] / ch->loadavg[0] + 1; 
	if ((ch->avg_bytes = kmalloc(ch->loadavg_size * 
		sizeof(unsigned long) * 2, GFP_KERNEL)) == NULL) {
		kfree(ch);
		return -ENOMEM;
	}

	memset(ch->avg_bytes, 0, ch->loadavg_size * sizeof(unsigned long) * 2);
	ch->loadavg_counter = 0;
	ch->loadavg_timer.function = comx_loadavg_timerfun;
	ch->loadavg_timer.data = (unsigned long)dev;
	ch->loadavg_timer.expires = jiffies + HZ * ch->loadavg[0];
	add_timer(&ch->loadavg_timer);

	dev->priv = (void *)ch;
	ch->dev = dev;
	ch->line_status &= ~LINE_UP;

	ch->current_stats = &ch->stats;

	comx_reset_dev(dev);
	return 0;
}

static int comx_read_proc(char *page, char **start, off_t off, int count, 
	int *eof, void *data)
{
	struct proc_dir_entry *file = (struct proc_dir_entry *)data;
	struct net_device *dev = file->parent->data;
	struct comx_channel *ch=(struct comx_channel *)dev->priv;
	int len = 0;

	if (strcmp(file->name, FILENAME_STATUS) == 0) {
		len = comx_statistics(dev, page);
	} else if (strcmp(file->name, FILENAME_HARDWARE) == 0) {
		len = sprintf(page, "%s\n", ch->hardware ? 
			ch->hardware->name : HWNAME_NONE);
	} else if (strcmp(file->name, FILENAME_PROTOCOL) == 0) {
		len = sprintf(page, "%s\n", ch->protocol ? 
			ch->protocol->name : PROTONAME_NONE);
	} else if (strcmp(file->name, FILENAME_LINEUPDELAY) == 0) {
		len = sprintf(page, "%01d\n", ch->lineup_delay);
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


static int comx_root_read_proc(char *page, char **start, off_t off, int count, 
	int *eof, void *data)
{
	struct proc_dir_entry *file = (struct proc_dir_entry *)data;
	struct comx_hardware *hw;
	struct comx_protocol *line;

	int len = 0;

	if (strcmp(file->name, FILENAME_HARDWARELIST) == 0) {
		for(hw=comx_channels;hw;hw=hw->next) 
			len+=sprintf(page+len, "%s\n", hw->name);
	} else if (strcmp(file->name, FILENAME_PROTOCOLLIST) == 0) {
		for(line=comx_lines;line;line=line->next)
			len+=sprintf(page+len, "%s\n", line->name);
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



static int comx_write_proc(struct file *file, const char *buffer, u_long count,
	void *data)
{
	struct proc_dir_entry *entry = (struct proc_dir_entry *)data;
	struct net_device *dev = (struct net_device *)entry->parent->data;
	struct comx_channel *ch=(struct comx_channel *)dev->priv;
	char *page;
	struct comx_hardware *hw = comx_channels;
	struct comx_protocol *line = comx_lines;
	char str[30];
	int ret=0;

	if (count > PAGE_SIZE) {
		printk(KERN_ERR "count is %lu > %d!!!\n", count, (int)PAGE_SIZE);
		return -ENOSPC;
	}

	if (!(page = (char *)__get_free_page(GFP_KERNEL))) return -ENOMEM;

	if(copy_from_user(page, buffer, count))
	{
		count = -EFAULT;
		goto out;
	}

	if (page[count-1] == '\n')
		page[count-1] = '\0';
	else if (count < PAGE_SIZE)
		page[count] = '\0';
	else if (page[count]) {
		count = -EINVAL;
		goto out;
	}

	if (strcmp(entry->name, FILENAME_DEBUG) == 0) {
		int i;
		int ret = 0;

		if ((i = simple_strtoul(page, NULL, 10)) != 0) {
			unsigned long flags;

			save_flags(flags); cli();
			if (ch->debug_area) kfree(ch->debug_area);
			if ((ch->debug_area = kmalloc(ch->debug_size = i, 
				GFP_KERNEL)) == NULL) {
				ret = -ENOMEM;
			}
			ch->debug_start = ch->debug_end = 0;
			restore_flags(flags);
			free_page((unsigned long)page);
			return ret ? ret : count;
		}
		
		if (*page != '+' && *page != '-') {
			free_page((unsigned long)page);
			return -EINVAL;
		}
		while (comx_debugflags[i].value && 
			strncmp(comx_debugflags[i].name, page + 1, 
			strlen(comx_debugflags[i].name))) {
			i++;
		}
	
		if (comx_debugflags[i].value == 0) {
			printk(KERN_ERR "Invalid debug option\n");
			free_page((unsigned long)page);
			return -EINVAL;
		}
		if (*page == '+') {
			ch->debug_flags |= comx_debugflags[i].value;
		} else {
			ch->debug_flags &= ~comx_debugflags[i].value;
		}
	} else if (strcmp(entry->name, FILENAME_HARDWARE) == 0) {
		if(strlen(page)>10) {
			free_page((unsigned long)page);
			return -EINVAL;
		}
		while (hw) { 
			if (strcmp(hw->name, page) == 0) {
				break;
			} else {
				hw = hw->next;
			}
		}
#ifdef CONFIG_KMOD
		if(!hw && comx_strcasecmp(HWNAME_NONE,page) != 0){
			sprintf(str,"comx-hw-%s",page);
			request_module(str);
		}		
		hw=comx_channels;
		while (hw) {
			if (comx_strcasecmp(hw->name, page) == 0) {
				break;
			} else {
				hw = hw->next;
			}
		}
#endif

		if (comx_strcasecmp(HWNAME_NONE, page) != 0 && !hw)  {
			free_page((unsigned long)page);
			return -ENODEV;
		}
		if (ch->init_status & HW_OPEN) {
			free_page((unsigned long)page);
			return -EBUSY;
		}
		if (ch->hardware && ch->hardware->hw_exit && 
		   (ret=ch->hardware->hw_exit(dev))) {
			free_page((unsigned long)page);
			return ret;
		}
		ch->hardware = hw;
		entry->size = strlen(page) + 1;
		if (hw && hw->hw_init) hw->hw_init(dev);
	} else if (strcmp(entry->name, FILENAME_PROTOCOL) == 0) {
		if(strlen(page)>10) {
			free_page((unsigned long)page);
			return -EINVAL;
		}
		while (line) {
			if (comx_strcasecmp(line->name, page) == 0) {
				break;
			} else {
				line = line->next;
			}
		}
#ifdef CONFIG_KMOD
		if(!line && comx_strcasecmp(PROTONAME_NONE, page) != 0) {
			sprintf(str,"comx-proto-%s",page);
			request_module(str);
		}		
		line=comx_lines;
		while (line) {
			if (comx_strcasecmp(line->name, page) == 0) {
				break;
			} else {
				line = line->next;
			}
		}
#endif
		
		if (comx_strcasecmp(PROTONAME_NONE, page) != 0 && !line) {
			free_page((unsigned long)page);
			return -ENODEV;
		}
		
		if (ch->init_status & LINE_OPEN) {
			free_page((unsigned long)page);
			return -EBUSY;
		}
		
		if (ch->protocol && ch->protocol->line_exit && 
		    (ret=ch->protocol->line_exit(dev))) {
			free_page((unsigned long)page);
			return ret;
		}
		ch->protocol = line;
		entry->size = strlen(page) + 1;
		comx_reset_dev(dev);
		if (line && line->line_init) line->line_init(dev);
	} else if (strcmp(entry->name, FILENAME_LINEUPDELAY) == 0) {
		int i;

		if ((i = simple_strtoul(page, NULL, 10)) != 0) {
			if (i >=0 && i < 10) { 
				ch->lineup_delay = i; 
			} else {
				printk(KERN_ERR "comx: invalid lineup_delay value\n");
			}
		}
	}
out:
	free_page((unsigned long)page);
	return count;
}

static int comx_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
	struct proc_dir_entry *new_dir, *debug_file;
	struct net_device *dev;
	struct comx_channel *ch;
	int ret = -EIO;

	if ((dev = kmalloc(sizeof(struct net_device), GFP_KERNEL)) == NULL) {
		return -ENOMEM;
	}
	memset(dev, 0, sizeof(struct net_device));

	if ((new_dir = create_proc_entry(dentry->d_name.name, mode | S_IFDIR, 
		comx_root_dir)) == NULL) {
		goto cleanup_dev;
	}

	new_dir->nlink = 2;
	new_dir->data = NULL; // ide jon majd a struct dev

	/* Ezek kellenek */
	if (!create_comx_proc_entry(FILENAME_HARDWARE, 0644, 
	    strlen(HWNAME_NONE) + 1, new_dir)) {
		goto cleanup_new_dir;
	}
	if (!create_comx_proc_entry(FILENAME_PROTOCOL, 0644, 
	    strlen(PROTONAME_NONE) + 1, new_dir)) {
		goto cleanup_filename_hardware;
	}
	if (!create_comx_proc_entry(FILENAME_STATUS, 0444, 0, new_dir)) {
		goto cleanup_filename_protocol;
	}
	if (!create_comx_proc_entry(FILENAME_LINEUPDELAY, 0644, 2, new_dir)) {
		goto cleanup_filename_status;
	}

	if ((debug_file = create_proc_entry(FILENAME_DEBUG, 
	    S_IFREG | 0644, new_dir)) == NULL) {
		goto cleanup_filename_lineupdelay;
	}
	debug_file->data = (void *)debug_file; 
	debug_file->read_proc = NULL; // see below
	debug_file->write_proc = &comx_write_proc;
	debug_file->nlink = 1;

	strcpy(dev->name, (char *)new_dir->name);
	dev->init = comx_init_dev;

	if (register_netdevice(dev)) {
		goto cleanup_filename_debug;
	}
	ch=dev->priv;
	if((ch->if_ptr = (void *)kmalloc(sizeof(struct ppp_device), 
				 GFP_KERNEL)) == NULL) {
		goto cleanup_register;
	}
	memset(ch->if_ptr, 0, sizeof(struct ppp_device));
	ch->debug_file = debug_file; 
	ch->procdir = new_dir;
	new_dir->data = dev;

	ch->debug_start = ch->debug_end = 0;
	if ((ch->debug_area = kmalloc(ch->debug_size = DEFAULT_DEBUG_SIZE, 
	    GFP_KERNEL)) == NULL) {
		ret = -ENOMEM;
		goto cleanup_if_ptr;
	}

	ch->lineup_delay = DEFAULT_LINEUP_DELAY;

	MOD_INC_USE_COUNT;
	return 0;
cleanup_if_ptr:
	kfree(ch->if_ptr);
cleanup_register:
	unregister_netdevice(dev);
cleanup_filename_debug:
	remove_proc_entry(FILENAME_DEBUG, new_dir);
cleanup_filename_lineupdelay:
	remove_proc_entry(FILENAME_LINEUPDELAY, new_dir);
cleanup_filename_status:
	remove_proc_entry(FILENAME_STATUS, new_dir);
cleanup_filename_protocol:
	remove_proc_entry(FILENAME_PROTOCOL, new_dir);
cleanup_filename_hardware:
	remove_proc_entry(FILENAME_HARDWARE, new_dir);
cleanup_new_dir:
	remove_proc_entry(dentry->d_name.name, comx_root_dir);
cleanup_dev:
	kfree(dev);
	return ret;
}

static int comx_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct proc_dir_entry *entry = dentry->d_inode->u.generic_ip;
	struct net_device *dev = entry->data;
	struct comx_channel *ch = dev->priv;
	int ret;

	if (dev->flags & IFF_UP) {
		printk(KERN_ERR "%s: down interface before removing it\n", dev->name);
		return -EBUSY;
	}

	if (ch->protocol && ch->protocol->line_exit && 
	    (ret=ch->protocol->line_exit(dev))) {
		return ret;
	}
	if (ch->hardware && ch->hardware->hw_exit && 
	   (ret=ch->hardware->hw_exit(dev))) { 
		if(ch->protocol && ch->protocol->line_init) {
			ch->protocol->line_init(dev);
		}
		return ret;
	}
	ch->protocol = NULL;
	ch->hardware = NULL;

	del_timer(&ch->loadavg_timer);
	kfree(ch->avg_bytes);

	unregister_netdev(dev);
	if (ch->debug_area) {
		kfree(ch->debug_area);
	}
	if (dev->priv) {
		kfree(dev->priv);
	}
	kfree(dev);

	remove_proc_entry(FILENAME_DEBUG, entry);
	remove_proc_entry(FILENAME_LINEUPDELAY, entry);
	remove_proc_entry(FILENAME_STATUS, entry);
	remove_proc_entry(FILENAME_HARDWARE, entry);
	remove_proc_entry(FILENAME_PROTOCOL, entry);
	remove_proc_entry(dentry->d_name.name, comx_root_dir);

	MOD_DEC_USE_COUNT;
	return 0;
}

static struct dentry *comx_lookup(struct inode *dir, struct dentry *dentry)
{
	struct proc_dir_entry *de;
	struct inode *inode = NULL;

	if ((de = (struct proc_dir_entry *) dir->u.generic_ip) != NULL) {
		for (de = de->subdir ; de ; de = de->next) {
			if ((de && de->low_ino) && 
			    (de->namelen == dentry->d_name.len) &&
			    (memcmp(dentry->d_name.name, de->name, 
			    de->namelen) == 0))	{
			 	if ((inode = proc_get_inode(dir->i_sb, 
			 	    de->low_ino, de)) == NULL) { 
			 		printk(KERN_ERR "COMX: lookup error\n"); 
			 		return ERR_PTR(-EINVAL); 
			 	}
				break;
			}
		}
	}
	dentry->d_op = &comx_dentry_operations;
	d_add(dentry, inode);
	return NULL;
}

int comx_strcasecmp(const char *cs, const char *ct)
{
	register signed char __res;

	while (1) {
		if ((__res = toupper(*cs) - toupper(*ct++)) != 0 || !*cs++) {
			break;
		}
	}
	return __res;
}

static int comx_delete_dentry(struct dentry *dentry)
{
	return 1;
}

static struct proc_dir_entry *create_comx_proc_entry(char *name, int mode,
	int size, struct proc_dir_entry *dir)
{
	struct proc_dir_entry *new_file;

	if ((new_file = create_proc_entry(name, S_IFREG | mode, dir)) != NULL) {
		new_file->data = (void *)new_file;
		new_file->read_proc = &comx_read_proc;
		new_file->write_proc = &comx_write_proc;
		new_file->size = size;
		new_file->nlink = 1;
	}
	return(new_file);
}

int comx_register_hardware(struct comx_hardware *comx_hw)
{
	struct comx_hardware *hw = comx_channels;

	if (!hw) {
		comx_channels = comx_hw;
	} else {
		while (hw->next != NULL && strcmp(comx_hw->name, hw->name) != 0) {
			hw = hw->next;
		}
		if (strcmp(comx_hw->name, hw->name) == 0) {
			return -1;
		}
		hw->next = comx_hw;
	}

	printk(KERN_INFO "COMX: driver for hardware type %s, version %s\n", comx_hw->name, comx_hw->version);
	return 0;
}

int comx_unregister_hardware(char *name)
{
	struct comx_hardware *hw = comx_channels;

	if (!hw) {
		return -1;
	}

	if (strcmp(hw->name, name) == 0) {
		comx_channels = comx_channels->next;
		return 0;
	}

	while (hw->next != NULL && strcmp(hw->next->name,name) != 0) {
		hw = hw->next;
	}

	if (hw->next != NULL && strcmp(hw->next->name, name) == 0) {
		hw->next = hw->next->next;
		return 0;
	}
	return -1;
}

int comx_register_protocol(struct comx_protocol *comx_line)
{
	struct comx_protocol *pr = comx_lines;

	if (!pr) {
		comx_lines = comx_line;
	} else {
		while (pr->next != NULL && strcmp(comx_line->name, pr->name) !=0) {
			pr = pr->next;
		}
		if (strcmp(comx_line->name, pr->name) == 0) {
			return -1;
		}
		pr->next = comx_line;
	}

	printk(KERN_INFO "COMX: driver for protocol type %s, version %s\n", comx_line->name, comx_line->version);
	return 0;
}

int comx_unregister_protocol(char *name)
{
	struct comx_protocol *pr = comx_lines;

	if (!pr) {
		return -1;
	}

	if (strcmp(pr->name, name) == 0) {
		comx_lines = comx_lines->next;
		return 0;
	}

	while (pr->next != NULL && strcmp(pr->next->name,name) != 0) {
		pr = pr->next;
	}

	if (pr->next != NULL && strcmp(pr->next->name, name) == 0) {
		pr->next = pr->next->next;
		return 0;
	}
	return -1;
}

#ifdef MODULE
#define comx_init init_module
#endif

int __init comx_init(void)
{
	struct proc_dir_entry *new_file;

	comx_root_dir = create_proc_entry("comx", 
		S_IFDIR | S_IWUSR | S_IRUGO | S_IXUGO, &proc_root);
	if (!comx_root_dir)
		return -ENOMEM;
	comx_root_dir->proc_iops = &comx_root_inode_ops;

	if ((new_file = create_proc_entry(FILENAME_HARDWARELIST, 
	   S_IFREG | 0444, comx_root_dir)) == NULL) {
		return -ENOMEM;
	}
	
	new_file->data = new_file;
	new_file->read_proc = &comx_root_read_proc;
	new_file->write_proc = NULL;
	new_file->nlink = 1;

	if ((new_file = create_proc_entry(FILENAME_PROTOCOLLIST, 
	   S_IFREG | 0444, comx_root_dir)) == NULL) {
		return -ENOMEM;
	}
	
	new_file->data = new_file;
	new_file->read_proc = &comx_root_read_proc;
	new_file->write_proc = NULL;
	new_file->nlink = 1;


	printk(KERN_INFO "COMX: driver version %s (C) 1995-1999 ITConsult-Pro Co. <info@itc.hu>\n", 
		VERSION);

#ifndef MODULE
#ifdef CONFIG_COMX_HW_COMX
	comx_hw_comx_init();
#endif
#ifdef CONFIG_COMX_HW_LOCOMX
	comx_hw_locomx_init();
#endif
#ifdef CONFIG_COMX_HW_MIXCOM
	comx_hw_mixcom_init();
#endif
#ifdef CONFIG_COMX_PROTO_HDLC
	comx_proto_hdlc_init();
#endif
#ifdef CONFIG_COMX_PROTO_PPP
	comx_proto_ppp_init();
#endif
#ifdef CONFIG_COMX_PROTO_LAPB
	comx_proto_lapb_init();
#endif
#ifdef CONFIG_COMX_PROTO_FR
	comx_proto_fr_init();
#endif
#endif

	return 0;
}

#ifdef MODULE
void cleanup_module(void)
{
	remove_proc_entry(FILENAME_HARDWARELIST, comx_root_dir);
	remove_proc_entry(FILENAME_PROTOCOLLIST, comx_root_dir);
	remove_proc_entry(comx_root_dir->name, &proc_root);
}
#endif

EXPORT_SYMBOL(comx_register_hardware);
EXPORT_SYMBOL(comx_unregister_hardware);
EXPORT_SYMBOL(comx_register_protocol);
EXPORT_SYMBOL(comx_unregister_protocol);
EXPORT_SYMBOL(comx_debug_skb);
EXPORT_SYMBOL(comx_debug_bytes);
EXPORT_SYMBOL(comx_debug);
EXPORT_SYMBOL(comx_lineup_func);
EXPORT_SYMBOL(comx_status);
EXPORT_SYMBOL(comx_rx);
EXPORT_SYMBOL(comx_strcasecmp);
EXPORT_SYMBOL(comx_root_dir);
