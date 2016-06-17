/* 
 * Hardware driver for the MixCom synchronous serial board 
 *
 * Author: Gergely Madarasz <gorgo@itc.hu>
 *
 * based on skeleton driver code and a preliminary hscx driver by 
 * Tivadar Szemethy <tiv@itc.hu>
 *
 * Copyright (C) 1998-1999 ITConsult-Pro Co. <info@itc.hu>
 *
 * Contributors:
 * Arnaldo Carvalho de Melo <acme@conectiva.com.br> (0.65)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * Version 0.60 (99/06/11):
 *		- ported to the kernel, now works as builtin code
 *
 * Version 0.61 (99/06/11):
 *		- recognize the one-channel MixCOM card (id byte = 0x13)
 *		- printk fixes
 * 
 * Version 0.62 (99/07/15):
 *		- fixes according to the new hw docs 
 *		- report line status when open
 *
 * Version 0.63 (99/09/21):
 *		- line status report fixes
 *
 * Version 0.64 (99/12/01):
 *		- some more cosmetical fixes
 *
 * Version 0.65 (00/08/15)
 *		- resource release on failure at MIXCOM_init
 */

#define VERSION "0.65"

#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <asm/types.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/init.h>

#include "comx.h"
#include "mixcom.h"
#include "hscx.h"

MODULE_AUTHOR("Gergely Madarasz <gorgo@itc.hu>");
MODULE_DESCRIPTION("Hardware-level driver for the serial port of the MixCom board");
MODULE_LICENSE("GPL");

#define MIXCOM_DATA(d) ((struct mixcom_privdata *)(COMX_CHANNEL(d)-> \
	HW_privdata))

#define MIXCOM_BOARD_BASE(d) (d->base_addr - MIXCOM_SERIAL_OFFSET - \
	(1 - MIXCOM_DATA(d)->channel) * MIXCOM_CHANNEL_OFFSET)

#define MIXCOM_DEV_BASE(port,channel) (port + MIXCOM_SERIAL_OFFSET + \
	(1 - channel) * MIXCOM_CHANNEL_OFFSET)

/* Values used to set the IRQ line */
static unsigned char mixcom_set_irq[]={0xFF, 0xFF, 0xFF, 0x0, 0xFF, 0x2, 0x4, 0x6, 0xFF, 0xFF, 0x8, 0xA, 0xC, 0xFF, 0xE, 0xFF};

static unsigned char* hscx_versions[]={"A1", NULL, "A2", NULL, "A3", "2.1"};

struct mixcom_privdata {
	u16	clock;
	char	channel;
	long	txbusy;
	struct sk_buff *sending;
	unsigned tx_ptr;
	struct sk_buff *recving;
	unsigned rx_ptr;
	unsigned char status;
	char	card_has_status;
};

static inline void wr_hscx(struct net_device *dev, int reg, unsigned char val) 
{
	outb(val, dev->base_addr + reg);
}

static inline unsigned char rd_hscx(struct net_device *dev, int reg)
{
	return inb(dev->base_addr + reg);
}

static inline void hscx_cmd(struct net_device *dev, int cmd)
{
	unsigned long jiffs = jiffies;
	unsigned char cec;
	unsigned delay = 0;

	while ((cec = (rd_hscx(dev, HSCX_STAR) & HSCX_CEC)) != 0 && 
	    (jiffs + HZ > jiffies)) {
		udelay(1);
		if (++delay > (100000 / HZ)) break;
	}
	if (cec) {
		printk(KERN_WARNING "%s: CEC stuck, probably no clock!\n",dev->name);
	} else {
		wr_hscx(dev, HSCX_CMDR, cmd);
	}
}

static inline void hscx_fill_fifo(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	struct mixcom_privdata *hw = ch->HW_privdata;
	register word to_send = hw->sending->len - hw->tx_ptr;


	outsb(dev->base_addr + HSCX_FIFO,
        	&(hw->sending->data[hw->tx_ptr]), min_t(unsigned int, to_send, 32));
	if (to_send <= 32) {
        	hscx_cmd(dev, HSCX_XTF | HSCX_XME);
	        kfree_skb(hw->sending);
        	hw->sending = NULL; 
        	hw->tx_ptr = 0;
        } else {
	        hscx_cmd(dev, HSCX_XTF);
        	hw->tx_ptr += 32;
        }
}

static inline void hscx_empty_fifo(struct net_device *dev, int cnt)
{
	struct comx_channel *ch = dev->priv;
	struct mixcom_privdata *hw = ch->HW_privdata;

	if (hw->recving == NULL) {
        	if (!(hw->recving = dev_alloc_skb(HSCX_MTU + 16))) {
	                ch->stats.rx_dropped++;
        	        hscx_cmd(dev, HSCX_RHR);
                } else {
	                skb_reserve(hw->recving, 16);
        	        skb_put(hw->recving, HSCX_MTU);
                }
	        hw->rx_ptr = 0;
        }
	if (cnt > 32 || !cnt || hw->recving == NULL) {
        	printk(KERN_ERR "hscx_empty_fifo: cnt is %d, hw->recving %p\n",
		        cnt, (void *)hw->recving);
	        return;
        }
        
	insb(dev->base_addr + HSCX_FIFO, &(hw->recving->data[hw->rx_ptr]),cnt);
	hw->rx_ptr += cnt;
	hscx_cmd(dev, HSCX_RMC);
}


static int MIXCOM_txe(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	struct mixcom_privdata *hw = ch->HW_privdata;

	return !test_bit(0, &hw->txbusy);
}

static int mixcom_probe(struct net_device *dev)
{
	unsigned long flags;
	int id, vstr, ret=0;

	save_flags(flags); cli();

	id=inb_p(MIXCOM_BOARD_BASE(dev) + MIXCOM_ID_OFFSET) & 0x7f;

 	if (id != MIXCOM_ID ) {
		ret=-ENODEV;
		printk(KERN_WARNING "%s: no MixCOM board found at 0x%04lx\n",dev->name, dev->base_addr);
		goto out;
	}

	vstr=inb_p(dev->base_addr + HSCX_VSTR) & 0x0f;
	if(vstr>=sizeof(hscx_versions)/sizeof(char*) || 
	    hscx_versions[vstr]==NULL) {
		printk(KERN_WARNING "%s: board found but no HSCX chip detected at 0x%4lx (vstr = 0x%1x)\n",dev->name,dev->base_addr,vstr);
		ret = -ENODEV;
	} else {
		printk(KERN_INFO "%s: HSCX chip version %s\n",dev->name,hscx_versions[vstr]);
		ret = 0;
	}

out:

	restore_flags(flags);
	return ret;
}

#if 0
static void MIXCOM_set_clock(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	struct mixcom_privdata *hw = ch->HW_privdata;

	if (hw->clock) {
		;
	} else {
		;
	}
}
#endif

static void mixcom_board_on(struct net_device *dev)
{
	outb_p(MIXCOM_OFF , MIXCOM_BOARD_BASE(dev) + MIXCOM_IT_OFFSET);
	udelay(1000);
	outb_p(mixcom_set_irq[dev->irq] | MIXCOM_ON, 
		MIXCOM_BOARD_BASE(dev) + MIXCOM_IT_OFFSET);
	udelay(1000);
}

static void mixcom_board_off(struct net_device *dev)
{
	outb_p(MIXCOM_OFF , MIXCOM_BOARD_BASE(dev) + MIXCOM_IT_OFFSET);
	udelay(1000);
}

static void mixcom_off(struct net_device *dev)
{
	wr_hscx(dev, HSCX_CCR1, 0x0);
}

static void mixcom_on(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;

	wr_hscx(dev, HSCX_CCR1, HSCX_PU | HSCX_ODS | HSCX_ITF); // power up, push-pull
	wr_hscx(dev, HSCX_CCR2, HSCX_CIE /* | HSCX_RIE */ );
	wr_hscx(dev, HSCX_MODE, HSCX_TRANS | HSCX_ADM8 | HSCX_RAC | HSCX_RTS );
	wr_hscx(dev, HSCX_RLCR, HSCX_RC | 47); // 1504 bytes
	wr_hscx(dev, HSCX_MASK, HSCX_RSC | HSCX_TIN );
	hscx_cmd(dev, HSCX_XRES | HSCX_RHR);

	if (ch->HW_set_clock) ch->HW_set_clock(dev);

}

static int MIXCOM_send_packet(struct net_device *dev, struct sk_buff *skb) 
{
	struct comx_channel *ch = dev->priv;
	struct mixcom_privdata *hw = ch->HW_privdata;
	unsigned long flags;

	if (ch->debug_flags & DEBUG_HW_TX) {
		comx_debug_bytes(dev, skb->data, skb->len, "MIXCOM_send_packet");
	}

	if (!(ch->line_status & LINE_UP)) {
		return FRAME_DROPPED;
	}

	if (skb->len > HSCX_MTU) {
		ch->stats.tx_errors++;	
		return FRAME_ERROR;
	}

	save_flags(flags); cli();

	if (test_and_set_bit(0, &hw->txbusy)) {
		printk(KERN_ERR "%s: transmitter called while busy... dropping frame (length %d)\n", dev->name, skb->len);
		restore_flags(flags);
		return FRAME_DROPPED;
	}


	hw->sending = skb;
	hw->tx_ptr = 0;
	hw->txbusy = 1;
//	atomic_inc(&skb->users);	// save it
	hscx_fill_fifo(dev);
	restore_flags(flags);

	ch->stats.tx_packets++;
	ch->stats.tx_bytes += skb->len; 

	if (ch->debug_flags & DEBUG_HW_TX) {
		comx_debug(dev, "MIXCOM_send_packet was successful\n\n");
	}

	return FRAME_ACCEPTED;
}

static inline void mixcom_receive_frame(struct net_device *dev) 
{
	struct comx_channel *ch=dev->priv;
	struct mixcom_privdata *hw=ch->HW_privdata;
	register byte rsta;
	register word length;

	rsta = rd_hscx(dev, HSCX_RSTA) & (HSCX_VFR | HSCX_RDO | 
		HSCX_CRC | HSCX_RAB);
	length = ((rd_hscx(dev, HSCX_RBCH) & 0x0f) << 8) | 
		rd_hscx(dev, HSCX_RBCL);

	if ( length > hw->rx_ptr ) {
		hscx_empty_fifo(dev, length - hw->rx_ptr);
	}
	
	if (!(rsta & HSCX_VFR)) {
		ch->stats.rx_length_errors++;
	}
	if (rsta & HSCX_RDO) {
		ch->stats.rx_over_errors++;
	}
	if (!(rsta & HSCX_CRC)) {
		ch->stats.rx_crc_errors++;
	}
	if (rsta & HSCX_RAB) {
		ch->stats.rx_frame_errors++;
	}
	ch->stats.rx_packets++; 
	ch->stats.rx_bytes += length;

	if (rsta == (HSCX_VFR | HSCX_CRC) && hw->recving) {
		skb_trim(hw->recving, hw->rx_ptr - 1);
		if (ch->debug_flags & DEBUG_HW_RX) {
			comx_debug_skb(dev, hw->recving,
				"MIXCOM_interrupt receiving");
		}
		hw->recving->dev = dev;
		if (ch->LINE_rx) {
			ch->LINE_rx(dev, hw->recving);
		}
	}
	else if(hw->recving) {
		kfree_skb(hw->recving);
	}
	hw->recving = NULL; 
	hw->rx_ptr = 0;
}


static inline void mixcom_extended_interrupt(struct net_device *dev) 
{
	struct comx_channel *ch=dev->priv;
	struct mixcom_privdata *hw=ch->HW_privdata;
	register byte exir;

	exir = rd_hscx(dev, HSCX_EXIR) & (HSCX_XDU | HSCX_RFO | HSCX_CSC );

	if (exir & HSCX_RFO) {
		ch->stats.rx_over_errors++;
		if (hw->rx_ptr) {
			kfree_skb(hw->recving);
			hw->recving = NULL; hw->rx_ptr = 0;
		}
		printk(KERN_ERR "MIXCOM: rx overrun\n");
		hscx_cmd(dev, HSCX_RHR);
	}

	if (exir & HSCX_XDU) { // xmit underrun
		ch->stats.tx_errors++;
		ch->stats.tx_aborted_errors++;
		if (hw->tx_ptr) {
			kfree_skb(hw->sending);
			hw->sending = NULL; 
			hw->tx_ptr = 0;
		}
		hscx_cmd(dev, HSCX_XRES);
		clear_bit(0, &hw->txbusy);
		if (ch->LINE_tx) {
			ch->LINE_tx(dev);
		}
		printk(KERN_ERR "MIXCOM: tx underrun\n");
	}

	if (exir & HSCX_CSC) {        
		ch->stats.tx_carrier_errors++;
		if ((rd_hscx(dev, HSCX_STAR) & HSCX_CTS) == 0) { // Vonal le
			if (test_and_clear_bit(0, &ch->lineup_pending)) {
               			del_timer(&ch->lineup_timer);
			} else if (ch->line_status & LINE_UP) {
        		       	ch->line_status &= ~LINE_UP;
                		if (ch->LINE_status) {
                      			ch->LINE_status(dev,ch->line_status);
                      		}
		      	}
		}
		if (!(ch->line_status & LINE_UP) && (rd_hscx(dev, HSCX_STAR) & 
		    HSCX_CTS)) { // Vonal fol
			if (!test_and_set_bit(0,&ch->lineup_pending)) {
				ch->lineup_timer.function = comx_lineup_func;
	        	        ch->lineup_timer.data = (unsigned long)dev;
        	        	ch->lineup_timer.expires = jiffies + HZ * 
        	        		ch->lineup_delay;
	                	add_timer(&ch->lineup_timer);
		                hscx_cmd(dev, HSCX_XRES);
        		        clear_bit(0, &hw->txbusy);
                		if (hw->sending) {
					kfree_skb(hw->sending);
				}
				hw->sending=NULL;
				hw->tx_ptr = 0;
			}
		}
	}
}


static void MIXCOM_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long flags;
	struct net_device *dev = (struct net_device *)dev_id;
	struct comx_channel *ch, *twin_ch;
	struct mixcom_privdata *hw, *twin_hw;
	register unsigned char ista;

	if (dev==NULL) {
		printk(KERN_ERR "comx_interrupt: irq %d for unknown device\n",irq);
		return;
	}

	ch = dev->priv; 
	hw = ch->HW_privdata;

	save_flags(flags); cli(); 

	while((ista = (rd_hscx(dev, HSCX_ISTA) & (HSCX_RME | HSCX_RPF | 
	    HSCX_XPR | HSCX_EXB | HSCX_EXA | HSCX_ICA)))) {
		register byte ista2 = 0;

		if (ista & HSCX_RME) {
			mixcom_receive_frame(dev);
		}
		if (ista & HSCX_RPF) {
			hscx_empty_fifo(dev, 32);
		}
		if (ista & HSCX_XPR) {
			if (hw->tx_ptr) {
				hscx_fill_fifo(dev);
			} else {
				clear_bit(0, &hw->txbusy);
               			ch->LINE_tx(dev);
			}
		}
		
		if (ista & HSCX_EXB) {
			mixcom_extended_interrupt(dev);
		}
		
		if ((ista & HSCX_EXA) && ch->twin)  {
			mixcom_extended_interrupt(ch->twin);
		}
	
		if ((ista & HSCX_ICA) && ch->twin &&
		    (ista2 = rd_hscx(ch->twin, HSCX_ISTA) &
		    (HSCX_RME | HSCX_RPF | HSCX_XPR ))) {
			if (ista2 & HSCX_RME) {
				mixcom_receive_frame(ch->twin);
			}
			if (ista2 & HSCX_RPF) {
				hscx_empty_fifo(ch->twin, 32);
			}
			if (ista2 & HSCX_XPR) {
				twin_ch=ch->twin->priv;
				twin_hw=twin_ch->HW_privdata;
				if (twin_hw->tx_ptr) {
					hscx_fill_fifo(ch->twin);
				} else {
					clear_bit(0, &twin_hw->txbusy);
					ch->LINE_tx(ch->twin);
				}
			}
		}
	}

	restore_flags(flags);
	return;
}

static int MIXCOM_open(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	struct mixcom_privdata *hw = ch->HW_privdata;
	struct proc_dir_entry *procfile = ch->procdir->subdir;
	unsigned long flags; 
	int ret = -ENODEV;

	if (!dev->base_addr || !dev->irq)
		goto err_ret;


	if(hw->channel==1) {
		if(!TWIN(dev) || !(COMX_CHANNEL(TWIN(dev))->init_status & 
		    IRQ_ALLOCATED)) {
			printk(KERN_ERR "%s: channel 0 not yet initialized\n",dev->name);
			ret = -EAGAIN;
			goto err_ret;
		}
	}


	/* Is our hw present at all ? Not checking for channel 0 if it is already 
	   open */
	if(hw->channel!=0 || !(ch->init_status & IRQ_ALLOCATED)) {
		if (!request_region(dev->base_addr, MIXCOM_IO_EXTENT, dev->name)) {
			ret = -EAGAIN;
			goto err_ret;
		}
		if (mixcom_probe(dev)) {
			ret = -ENODEV;
			goto err_release_region;
		}
	}

	if(hw->channel==0 && !(ch->init_status & IRQ_ALLOCATED)) {
		if (request_irq(dev->irq, MIXCOM_interrupt, 0, 
		    dev->name, (void *)dev)) {
			printk(KERN_ERR "MIXCOM: unable to obtain irq %d\n", dev->irq);
			ret = -EAGAIN;
			goto err_release_region;
		}
	}

	save_flags(flags); cli();

	if(hw->channel==0 && !(ch->init_status & IRQ_ALLOCATED)) {
		ch->init_status|=IRQ_ALLOCATED;
		mixcom_board_on(dev);
	}

	mixcom_on(dev);


	hw->status=inb(MIXCOM_BOARD_BASE(dev) + MIXCOM_STATUS_OFFSET);
	if(hw->status != 0xff) {
		printk(KERN_DEBUG "%s: board has status register, good\n", dev->name);
		hw->card_has_status=1;
	}

	hw->txbusy = 0;
	ch->init_status |= HW_OPEN;
	
	if (rd_hscx(dev, HSCX_STAR) & HSCX_CTS) {
		ch->line_status |= LINE_UP;
	} else {
		ch->line_status &= ~LINE_UP;
	}

	restore_flags(flags);

	ch->LINE_status(dev, ch->line_status);

	for (; procfile ; procfile = procfile->next) {
		if (strcmp(procfile->name, FILENAME_IO) == 0 ||
		    strcmp(procfile->name, FILENAME_CHANNEL) == 0 ||
		    strcmp(procfile->name, FILENAME_CLOCK) == 0 ||
		    strcmp(procfile->name, FILENAME_IRQ) == 0) {
			procfile->mode = S_IFREG |  0444;
		}
	}

	return 0;
	
err_release_region:
	release_region(dev->base_addr, MIXCOM_IO_EXTENT);
err_ret:
	return ret;
}

static int MIXCOM_close(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	struct mixcom_privdata *hw = ch->HW_privdata;
	struct proc_dir_entry *procfile = ch->procdir->subdir;
	unsigned long flags;


	save_flags(flags); cli();

	mixcom_off(dev);

	/* This is channel 0, twin is not open, we can safely turn off everything */
	if(hw->channel==0 && (!(TWIN(dev)) || 
	    !(COMX_CHANNEL(TWIN(dev))->init_status & HW_OPEN))) {
		mixcom_board_off(dev);
		free_irq(dev->irq, dev);
		release_region(dev->base_addr, MIXCOM_IO_EXTENT);
		ch->init_status &= ~IRQ_ALLOCATED;
	}

	/* This is channel 1, channel 0 has already been shutdown, we can release
	   this one too */
	if(hw->channel==1 && !(COMX_CHANNEL(TWIN(dev))->init_status & HW_OPEN)) {
		if(COMX_CHANNEL(TWIN(dev))->init_status & IRQ_ALLOCATED) {
			mixcom_board_off(TWIN(dev));
			free_irq(TWIN(dev)->irq, TWIN(dev));
			release_region(TWIN(dev)->base_addr, MIXCOM_IO_EXTENT);
			COMX_CHANNEL(TWIN(dev))->init_status &= ~IRQ_ALLOCATED;
		}
	}

	/* the ioports for channel 1 can be safely released */
	if(hw->channel==1) {
		release_region(dev->base_addr, MIXCOM_IO_EXTENT);
	}

	restore_flags(flags);

	/* If we don't hold any hardware open */
	if(!(ch->init_status & IRQ_ALLOCATED)) {
		for (; procfile ; procfile = procfile->next) {
			if (strcmp(procfile->name, FILENAME_IO) == 0 ||
			    strcmp(procfile->name, FILENAME_CHANNEL) == 0 ||
			    strcmp(procfile->name, FILENAME_CLOCK) == 0 ||
			    strcmp(procfile->name, FILENAME_IRQ) == 0) {
				procfile->mode = S_IFREG |  0644;
			}
		}
	}

	/* channel 0 was only waiting for us to close channel 1 
	   close it completely */
   
	if(hw->channel==1 && !(COMX_CHANNEL(TWIN(dev))->init_status & HW_OPEN)) {
		for (procfile=COMX_CHANNEL(TWIN(dev))->procdir->subdir; 
		    procfile ; procfile = procfile->next) {
			if (strcmp(procfile->name, FILENAME_IO) == 0 ||
			    strcmp(procfile->name, FILENAME_CHANNEL) == 0 ||
			    strcmp(procfile->name, FILENAME_CLOCK) == 0 ||
			    strcmp(procfile->name, FILENAME_IRQ) == 0) {
				procfile->mode = S_IFREG |  0644;
			}
		}
	}
	
	ch->init_status &= ~HW_OPEN;
	return 0;
}

static int MIXCOM_statistics(struct net_device *dev,char *page)
{
	struct comx_channel *ch = dev->priv;
	// struct mixcom_privdata *hw = ch->HW_privdata;
	int len = 0;

	if(ch->init_status && IRQ_ALLOCATED) {
		len += sprintf(page + len, "Mixcom board: hardware open\n");
	}

	return len;
}

static int MIXCOM_dump(struct net_device *dev) {
	return 0;
}

static int mixcom_read_proc(char *page, char **start, off_t off, int count,
	int *eof, void *data)
{
	struct proc_dir_entry *file = (struct proc_dir_entry *)data;
	struct net_device *dev = file->parent->data;
	struct comx_channel *ch = dev->priv;
	struct mixcom_privdata *hw = ch->HW_privdata;
	int len = 0;

	if (strcmp(file->name, FILENAME_IO) == 0) {
		len = sprintf(page, "0x%x\n", 
			(unsigned int)MIXCOM_BOARD_BASE(dev));
	} else if (strcmp(file->name, FILENAME_IRQ) == 0) {
		len = sprintf(page, "%d\n", (unsigned int)dev->irq);
	} else if (strcmp(file->name, FILENAME_CLOCK) == 0) {
		if (hw->clock) len = sprintf(page, "%d\n", hw->clock);
			else len = sprintf(page, "external\n");
	} else if (strcmp(file->name, FILENAME_CHANNEL) == 0) {
		len = sprintf(page, "%01d\n", hw->channel);
	} else if (strcmp(file->name, FILENAME_TWIN) == 0) {
		if (ch->twin) {
			len = sprintf(page, "%s\n",ch->twin->name);
		} else {
			len = sprintf(page, "none\n");
		}
	} else {
		printk(KERN_ERR "mixcom_read_proc: internal error, filename %s\n", file->name);
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


static struct net_device *mixcom_twin_check(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	struct proc_dir_entry *procfile = ch->procdir->parent->subdir;
	struct mixcom_privdata *hw = ch->HW_privdata;

	struct net_device *twin;
	struct comx_channel *ch_twin;
	struct mixcom_privdata *hw_twin;


	for ( ; procfile ; procfile = procfile->next) {
		if(!S_ISDIR(procfile->mode)) continue;
                
        	twin = procfile->data;
	        ch_twin = twin->priv;
        	hw_twin = ch_twin->HW_privdata;


	        if (twin != dev && dev->irq && dev->base_addr && 
        	    dev->irq == twin->irq && 
        	    ch->hardware == ch_twin->hardware &&
		    dev->base_addr == twin->base_addr + 
		    (1-2*hw->channel)*MIXCOM_CHANNEL_OFFSET &&
		    hw->channel == (1 - hw_twin->channel)) {
	        	if  (!TWIN(twin) || TWIN(twin)==dev) {
	        		return twin;
	        	}
		}
        }
	return NULL;
}


static void setup_twin(struct net_device* dev) 
{

	if(TWIN(dev) && TWIN(TWIN(dev))) {
		TWIN(TWIN(dev))=NULL;
	}
	if ((TWIN(dev) = mixcom_twin_check(dev)) != NULL) {
		if (TWIN(TWIN(dev)) && TWIN(TWIN(dev)) != dev) {
			TWIN(dev)=NULL;
		} else {
			TWIN(TWIN(dev))=dev;
		}
	}	
}

static int mixcom_write_proc(struct file *file, const char *buffer,
	u_long count, void *data)
{
	struct proc_dir_entry *entry = (struct proc_dir_entry *)data;
	struct net_device *dev = (struct net_device *)entry->parent->data;
	struct comx_channel *ch = dev->priv;
	struct mixcom_privdata *hw = ch->HW_privdata;
	char *page;
	int value;

	if (!(page = (char *)__get_free_page(GFP_KERNEL))) {
		return -ENOMEM;
	}

	copy_from_user(page, buffer, count = min_t(unsigned long, count, PAGE_SIZE));
	if (*(page + count - 1) == '\n') {
		*(page + count - 1) = 0;
	}

	if (strcmp(entry->name, FILENAME_IO) == 0) {
		value = simple_strtoul(page, NULL, 0);
		if (value != 0x180 && value != 0x280 && value != 0x380) {
			printk(KERN_ERR "MIXCOM: incorrect io address!\n");
		} else {
			dev->base_addr = MIXCOM_DEV_BASE(value,hw->channel);
		}
	} else if (strcmp(entry->name, FILENAME_IRQ) == 0) {
		value = simple_strtoul(page, NULL, 0); 
		if (value < 0 || value > 15 || mixcom_set_irq[value]==0xFF) {
			printk(KERN_ERR "MIXCOM: incorrect irq value!\n");
		} else {
			dev->irq = value;	
		}
	} else if (strcmp(entry->name, FILENAME_CLOCK) == 0) {
		if (strncmp("ext", page, 3) == 0) {
			hw->clock = 0;
		} else {
			int kbps;

			kbps = simple_strtoul(page, NULL, 0);
			if (!kbps) {
				hw->clock = 0;
			} else {
				hw->clock = kbps;
			}
			if (hw->clock < 32 || hw->clock > 2000) {
				hw->clock = 0;
				printk(KERN_ERR "MIXCOM: invalid clock rate!\n");
			}
		}
		if (ch->init_status & HW_OPEN && ch->HW_set_clock) {
			ch->HW_set_clock(dev);
		}
	} else if (strcmp(entry->name, FILENAME_CHANNEL) == 0) {
		value = simple_strtoul(page, NULL, 0);
        	if (value > 2) {
                	printk(KERN_ERR "Invalid channel number\n");
	        } else {
        		dev->base_addr+=(hw->channel - value) * MIXCOM_CHANNEL_OFFSET;
	        	hw->channel = value;
		}	        
	} else {
		printk(KERN_ERR "hw_read_proc: internal error, filename %s\n", 
			entry->name);
		return -EBADF;
	}

	setup_twin(dev);

	free_page((unsigned long)page);
	return count;
}

static int MIXCOM_init(struct net_device *dev) {
	struct comx_channel *ch = dev->priv;
	struct mixcom_privdata *hw;
	struct proc_dir_entry *new_file;

	if ((ch->HW_privdata = kmalloc(sizeof(struct mixcom_privdata), 
	    GFP_KERNEL)) == NULL) {
	    	return -ENOMEM;
	}

	memset(hw = ch->HW_privdata, 0, sizeof(struct mixcom_privdata));

	if ((new_file = create_proc_entry(FILENAME_IO, S_IFREG | 0644, 
	    ch->procdir)) == NULL) {
		goto cleanup_HW_privdata;
	}
	new_file->data = (void *)new_file;
	new_file->read_proc = &mixcom_read_proc;
	new_file->write_proc = &mixcom_write_proc;
	new_file->nlink = 1;

	if ((new_file = create_proc_entry(FILENAME_IRQ, S_IFREG | 0644, 
	    ch->procdir)) == NULL) {
	    	goto cleanup_filename_io;
	}
	new_file->data = (void *)new_file;
	new_file->read_proc = &mixcom_read_proc;
	new_file->write_proc = &mixcom_write_proc;
	new_file->nlink = 1;

#if 0
	if ((new_file = create_proc_entry(FILENAME_CLOCK, S_IFREG | 0644, 
	    ch->procdir)) == NULL) {
	    	return -EIO;
	}
	new_file->data = (void *)new_file;
	new_file->read_proc = &mixcom_read_proc;
	new_file->write_proc = &mixcom_write_proc;
	new_file->nlink = 1;
#endif

	if ((new_file = create_proc_entry(FILENAME_CHANNEL, S_IFREG | 0644, 
	    ch->procdir)) == NULL) {
	    	goto cleanup_filename_irq;
	}
	new_file->data = (void *)new_file;
	new_file->read_proc = &mixcom_read_proc;
	new_file->write_proc = &mixcom_write_proc;
	new_file->nlink = 1;

	if ((new_file = create_proc_entry(FILENAME_TWIN, S_IFREG | 0444, 
	    ch->procdir)) == NULL) {
	    	goto cleanup_filename_channel;
	}
	new_file->data = (void *)new_file;
	new_file->read_proc = &mixcom_read_proc;
	new_file->write_proc = &mixcom_write_proc;
	new_file->nlink = 1;

	setup_twin(dev);

	/* Fill in ch_struct hw specific pointers */
	ch->HW_access_board = NULL;
	ch->HW_release_board = NULL;
	ch->HW_txe = MIXCOM_txe;
	ch->HW_open = MIXCOM_open;
	ch->HW_close = MIXCOM_close;
	ch->HW_send_packet = MIXCOM_send_packet;
	ch->HW_statistics = MIXCOM_statistics;
	ch->HW_set_clock = NULL;

	dev->base_addr = MIXCOM_DEV_BASE(MIXCOM_DEFAULT_IO,0);
	dev->irq = MIXCOM_DEFAULT_IRQ;

	MOD_INC_USE_COUNT;
	return 0;
cleanup_filename_channel:
	remove_proc_entry(FILENAME_CHANNEL, ch->procdir);
cleanup_filename_irq:
	remove_proc_entry(FILENAME_IRQ, ch->procdir);
cleanup_filename_io:
	remove_proc_entry(FILENAME_IO, ch->procdir);
cleanup_HW_privdata:
	kfree(ch->HW_privdata);
	return -EIO;
}

static int MIXCOM_exit(struct net_device *dev)
{
	struct comx_channel *ch = dev->priv;
	struct mixcom_privdata *hw = ch->HW_privdata;

	if(hw->channel==0 && TWIN(dev)) {
		return -EBUSY;
	}

	if(hw->channel==1 && TWIN(dev)) {
		TWIN(TWIN(dev))=NULL;
	}

	kfree(ch->HW_privdata);
	remove_proc_entry(FILENAME_IO, ch->procdir);
	remove_proc_entry(FILENAME_IRQ, ch->procdir);
#if 0
	remove_proc_entry(FILENAME_CLOCK, ch->procdir);
#endif
	remove_proc_entry(FILENAME_CHANNEL, ch->procdir);
	remove_proc_entry(FILENAME_TWIN, ch->procdir);

	MOD_DEC_USE_COUNT;
	return 0;
}

static struct comx_hardware mixcomhw = {
	"mixcom",
	VERSION,
	MIXCOM_init, 
	MIXCOM_exit,
	MIXCOM_dump,
	NULL
};
	
/* Module management */

#ifdef MODULE
#define comx_hw_mixcom_init init_module
#endif

int __init comx_hw_mixcom_init(void)
{
	return(comx_register_hardware(&mixcomhw));
}

#ifdef MODULE
void
cleanup_module(void)
{
	comx_unregister_hardware("mixcom");
}
#endif
