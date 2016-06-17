/*
 * Hardware driver for the LoCOMX card, using the generic z85230
 * functions
 *
 * Author: Gergely Madarasz <gorgo@itc.hu>
 *
 * Based on skeleton code and old LoCOMX driver by Tivadar Szemethy <tiv@itc.hu> 
 * and the hostess_sv11 driver
 *
 * Contributors:
 * Arnaldo Carvalho de Melo <acme@conectiva.com.br> (0.14)
 *
 * Copyright (C) 1999 ITConsult-Pro Co. <info@itc.hu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * Version 0.10 (99/06/17):
 *		- rewritten for the z85230 layer
 *
 * Version 0.11 (99/06/21):
 *		- some printk's fixed
 *		- get rid of a memory leak (it was impossible though :))
 * 
 * Version 0.12 (99/07/07):
 *		- check CTS for modem lines, not DCD (which is always high
 *		  in case of this board)
 * Version 0.13 (99/07/08):
 *		- Fix the transmitter status check
 *		- Handle the net device statistics better
 * Version 0.14 (00/08/15):
 * 		- resource release on failure at LOCOMX_init
 */

#define VERSION "0.14"

#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <asm/types.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <linux/ioport.h>
#include <linux/init.h>

#include "comx.h"
#include "z85230.h"

MODULE_AUTHOR("Gergely Madarasz <gorgo@itc.hu>");
MODULE_DESCRIPTION("Hardware driver for the LoCOMX board");
MODULE_LICENSE("GPL");

#define RX_DMA 3
#define TX_DMA 1
#define LOCOMX_ID 0x33
#define LOCOMX_IO_EXTENT 8
#define LOCOMX_DEFAULT_IO 0x368
#define LOCOMX_DEFAULT_IRQ 7

u8 z8530_locomx[] = {
	11,     TCRTxCP,
	14,     DTRREQ,
	255
};

struct locomx_data {
	int	io_extent;
	struct	z8530_dev board;
	struct timer_list status_timer;
};

static int LOCOMX_txe(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	struct locomx_data *hw = ch->HW_privdata;

	return (!hw->board.chanA.tx_next_skb);
}


static void locomx_rx(struct z8530_channel *c, struct sk_buff *skb)
{
	struct net_device *dev=c->netdevice;
	struct comx_channel *ch=dev->priv;
	
	if (ch->debug_flags & DEBUG_HW_RX) {
		comx_debug_skb(dev, skb, "locomx_rx receiving");
	}
	ch->LINE_rx(dev,skb);
}

static int LOCOMX_send_packet(struct net_device *dev, struct sk_buff *skb) 
{
	struct comx_channel *ch = (struct comx_channel *)dev->priv;
	struct locomx_data *hw = ch->HW_privdata;

	if (ch->debug_flags & DEBUG_HW_TX) {
		comx_debug_bytes(dev, skb->data, skb->len, "LOCOMX_send_packet");
	}

	if (!(ch->line_status & LINE_UP)) {
		return FRAME_DROPPED;
	}

	if(z8530_queue_xmit(&hw->board.chanA,skb)) {
		printk(KERN_WARNING "%s: FRAME_DROPPED\n",dev->name);
		return FRAME_DROPPED;
	}

	if (ch->debug_flags & DEBUG_HW_TX) {
		comx_debug(dev, "%s: LOCOMX_send_packet was successful\n\n", dev->name);
	}

	if(!hw->board.chanA.tx_next_skb) {
		return FRAME_QUEUED;
	} else {
		return FRAME_ACCEPTED;
	}
}

static void locomx_status_timerfun(unsigned long d)
{
	struct net_device *dev=(struct net_device *)d;
	struct comx_channel *ch=dev->priv;
	struct locomx_data *hw=ch->HW_privdata;

	if(!(ch->line_status & LINE_UP) &&
	    (hw->board.chanA.status & CTS)) {
		ch->LINE_status(dev, ch->line_status | LINE_UP);
	}
	if((ch->line_status & LINE_UP) &&
	    !(hw->board.chanA.status & CTS)) {
		ch->LINE_status(dev, ch->line_status & ~LINE_UP);
	}
	mod_timer(&hw->status_timer,jiffies + ch->lineup_delay * HZ);
}


static int LOCOMX_open(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	struct locomx_data *hw = ch->HW_privdata;
	struct proc_dir_entry *procfile = ch->procdir->subdir;
	unsigned long flags;
	int ret;

	if (!dev->base_addr || !dev->irq) {
		return -ENODEV;
	}

	if (!request_region(dev->base_addr, hw->io_extent, dev->name)) {
		return -EAGAIN;
	}

	hw->board.chanA.ctrlio=dev->base_addr + 5;
	hw->board.chanA.dataio=dev->base_addr + 7;
	
	hw->board.irq=dev->irq;
	hw->board.chanA.netdevice=dev;
	hw->board.chanA.dev=&hw->board;
	hw->board.name=dev->name;
	hw->board.chanA.txdma=TX_DMA;
	hw->board.chanA.rxdma=RX_DMA;
	hw->board.chanA.irqs=&z8530_nop;
	hw->board.chanB.irqs=&z8530_nop;

	if(request_irq(dev->irq, z8530_interrupt, SA_INTERRUPT, 
	    dev->name, &hw->board)) {
		printk(KERN_ERR "%s: unable to obtain irq %d\n", dev->name, 
			dev->irq);
		ret=-EAGAIN;
		goto irq_fail;
	}
	if(request_dma(TX_DMA,"LoCOMX (TX)")) {
		printk(KERN_ERR "%s: unable to obtain TX DMA (DMA channel %d)\n", 
			dev->name, TX_DMA);
		ret=-EAGAIN;
		goto dma1_fail;
	}

	if(request_dma(RX_DMA,"LoCOMX (RX)")) {
		printk(KERN_ERR "%s: unable to obtain RX DMA (DMA channel %d)\n", 
			dev->name, RX_DMA);
		ret=-EAGAIN;
		goto dma2_fail;
	}
	
	save_flags(flags); 
	cli();

	if(z8530_init(&hw->board)!=0)
	{
		printk(KERN_ERR "%s: Z8530 device not found.\n",dev->name);
		ret=-ENODEV;
		goto z8530_fail;
	}

	hw->board.chanA.dcdcheck=CTS;

	z8530_channel_load(&hw->board.chanA, z8530_hdlc_kilostream_85230);
	z8530_channel_load(&hw->board.chanA, z8530_locomx);
	z8530_channel_load(&hw->board.chanB, z8530_dead_port);

	z8530_describe(&hw->board, "I/O", dev->base_addr);

	if((ret=z8530_sync_dma_open(dev, &hw->board.chanA))!=0) {
		goto z8530_fail;
	}

	restore_flags(flags);


	hw->board.active=1;
	hw->board.chanA.rx_function=locomx_rx;

	ch->init_status |= HW_OPEN;
	if (hw->board.chanA.status & DCD) {
		ch->line_status |= LINE_UP;
	} else {
		ch->line_status &= ~LINE_UP;
	}

	comx_status(dev, ch->line_status);

	init_timer(&hw->status_timer);
	hw->status_timer.function=locomx_status_timerfun;
	hw->status_timer.data=(unsigned long)dev;
	hw->status_timer.expires=jiffies + ch->lineup_delay * HZ;
	add_timer(&hw->status_timer);

	for (; procfile ; procfile = procfile->next) {
		if (strcmp(procfile->name, FILENAME_IO) == 0 ||
		     strcmp(procfile->name, FILENAME_IRQ) == 0) {
			procfile->mode = S_IFREG |  0444;
		}
	}
	return 0;

z8530_fail:
	restore_flags(flags);
	free_dma(RX_DMA);
dma2_fail:
	free_dma(TX_DMA);
dma1_fail:
	free_irq(dev->irq, &hw->board);
irq_fail:
	release_region(dev->base_addr, hw->io_extent);
	return ret;
}

static int LOCOMX_close(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	struct locomx_data *hw = ch->HW_privdata;
	struct proc_dir_entry *procfile = ch->procdir->subdir;

	hw->board.chanA.rx_function=z8530_null_rx;
	netif_stop_queue(dev);
	z8530_sync_dma_close(dev, &hw->board.chanA);

	z8530_shutdown(&hw->board);

	del_timer(&hw->status_timer);
	free_dma(RX_DMA);
	free_dma(TX_DMA);
	free_irq(dev->irq,&hw->board);
	release_region(dev->base_addr,8);

	for (; procfile ; procfile = procfile->next) {
		if (strcmp(procfile->name, FILENAME_IO) == 0 ||
		    strcmp(procfile->name, FILENAME_IRQ) == 0) {
			procfile->mode = S_IFREG |  0644;
		}
	}

	ch->init_status &= ~HW_OPEN;
	return 0;
}

static int LOCOMX_statistics(struct net_device *dev,char *page)
{
	int len = 0;

	len += sprintf(page + len, "Hello\n");

	return len;
}

static int LOCOMX_dump(struct net_device *dev) {
	printk(KERN_INFO "LOCOMX_dump called\n");
	return(-1);
}

static int locomx_read_proc(char *page, char **start, off_t off, int count,
	int *eof, void *data)
{
	struct proc_dir_entry *file = (struct proc_dir_entry *)data;
	struct net_device *dev = file->parent->data;
	int len = 0;

	if (strcmp(file->name, FILENAME_IO) == 0) {
		len = sprintf(page, "0x%x\n", (unsigned int)dev->base_addr);
	} else if (strcmp(file->name, FILENAME_IRQ) == 0) {
		len = sprintf(page, "%d\n", (unsigned int)dev->irq);
	} else {
		printk(KERN_ERR "hw_read_proc: internal error, filename %s\n", 
			file->name);
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

static int locomx_write_proc(struct file *file, const char *buffer,
	u_long count, void *data)
{
	struct proc_dir_entry *entry = (struct proc_dir_entry *)data;
	struct net_device *dev = (struct net_device *)entry->parent->data;
	int val;
	char *page;

	if (!(page = (char *)__get_free_page(GFP_KERNEL))) {
		return -ENOMEM;
	}

	copy_from_user(page, buffer, count = min_t(unsigned long, count, PAGE_SIZE));
	if (*(page + count - 1) == '\n') {
		*(page + count - 1) = 0;
	}

	if (strcmp(entry->name, FILENAME_IO) == 0) {
		val = simple_strtoul(page, NULL, 0);
		if (val != 0x360 && val != 0x368 && val != 0x370 && 
		   val != 0x378) {
			printk(KERN_ERR "LoCOMX: incorrect io address!\n");	
		} else {
			dev->base_addr = val;
		}
	} else if (strcmp(entry->name, FILENAME_IRQ) == 0) {
		val = simple_strtoul(page, NULL, 0);
		if (val != 3 && val != 4 && val != 5 && val != 6 && val != 7) {
			printk(KERN_ERR "LoCOMX: incorrect irq value!\n");
		} else {
			dev->irq = val;
		}	
	} else {
		printk(KERN_ERR "locomx_write_proc: internal error, filename %s\n", 
			entry->name);
		free_page((unsigned long)page);
		return -EBADF;
	}

	free_page((unsigned long)page);
	return count;
}



static int LOCOMX_init(struct net_device *dev) 
{
	struct comx_channel *ch = (struct comx_channel *)dev->priv;
	struct locomx_data *hw;
	struct proc_dir_entry *new_file;

	/* Alloc data for private structure */
	if ((ch->HW_privdata = kmalloc(sizeof(struct locomx_data), 
	   GFP_KERNEL)) == NULL) {
	   	return -ENOMEM;
	}

	memset(hw = ch->HW_privdata, 0, sizeof(struct locomx_data));
	hw->io_extent = LOCOMX_IO_EXTENT;

	/* Register /proc files */
	if ((new_file = create_proc_entry(FILENAME_IO, S_IFREG | 0644, 
	    ch->procdir)) == NULL) {
		goto cleanup_HW_privdata;
	}
	new_file->data = (void *)new_file;
	new_file->read_proc = &locomx_read_proc;
	new_file->write_proc = &locomx_write_proc;
	new_file->nlink = 1;

	if ((new_file = create_proc_entry(FILENAME_IRQ, S_IFREG | 0644, 
	    ch->procdir)) == NULL)  {
		goto cleanup_filename_io;
	}
	new_file->data = (void *)new_file;
	new_file->read_proc = &locomx_read_proc;
	new_file->write_proc = &locomx_write_proc;
	new_file->nlink = 1;

/* 	No clock yet */
/*
	if ((new_file = create_proc_entry(FILENAME_CLOCK, S_IFREG | 0644, 
	    ch->procdir)) == NULL) {
		return -EIO;
	}
	new_file->data = (void *)new_file;
	new_file->read_proc = &locomx_read_proc;
	new_file->write_proc = &locomx_write_proc;
	new_file->nlink = 1;
*/

	ch->HW_access_board = NULL;
	ch->HW_release_board = NULL;
	ch->HW_txe = LOCOMX_txe;
	ch->HW_open = LOCOMX_open;
	ch->HW_close = LOCOMX_close;
	ch->HW_send_packet = LOCOMX_send_packet;
	ch->HW_statistics = LOCOMX_statistics;
	ch->HW_set_clock = NULL;

	ch->current_stats = &hw->board.chanA.stats;
	memcpy(ch->current_stats, &ch->stats, sizeof(struct net_device_stats));

	dev->base_addr = LOCOMX_DEFAULT_IO;
	dev->irq = LOCOMX_DEFAULT_IRQ;
	
	
	/* O.K. Count one more user on this module */
	MOD_INC_USE_COUNT;
	return 0;
cleanup_filename_io:
	remove_proc_entry(FILENAME_IO, ch->procdir);
cleanup_HW_privdata:
	kfree(ch->HW_privdata);
	return -EIO;
}


static int LOCOMX_exit(struct net_device *dev)
{
	struct comx_channel *ch = (struct comx_channel *)dev->priv;

	ch->HW_access_board = NULL;
	ch->HW_release_board = NULL;
	ch->HW_txe = NULL;
	ch->HW_open = NULL;
	ch->HW_close = NULL;
	ch->HW_send_packet = NULL;
	ch->HW_statistics = NULL;
	ch->HW_set_clock = NULL;
	memcpy(&ch->stats, ch->current_stats, sizeof(struct net_device_stats));
	ch->current_stats = &ch->stats;

	kfree(ch->HW_privdata);

	remove_proc_entry(FILENAME_IO, ch->procdir);
	remove_proc_entry(FILENAME_IRQ, ch->procdir);
//	remove_proc_entry(FILENAME_CLOCK, ch->procdir);

	MOD_DEC_USE_COUNT;
	return 0;
}

static struct comx_hardware locomx_hw = {
	"locomx",
	VERSION,
	LOCOMX_init, 
	LOCOMX_exit,
	LOCOMX_dump,
	NULL
};
	
#ifdef MODULE
#define comx_hw_locomx_init init_module
#endif

int __init comx_hw_locomx_init(void)
{
	comx_register_hardware(&locomx_hw);
	return 0;
}

#ifdef MODULE
void cleanup_module(void)
{
	comx_unregister_hardware("locomx");
	return;
}
#endif
