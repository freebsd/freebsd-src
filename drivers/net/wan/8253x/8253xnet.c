/* -*- linux-c -*- */
/* 
 * Copyright (C) 2001 By Joachim Martillo, Telford Tools, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 **/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/sockios.h>
#include <asm/io.h>
#include <asm/byteorder.h>
#include <asm/pgtable.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/version.h>
#include <linux/etherdevice.h>
#include "Reg9050.h"
#include "8253xctl.h"
#include "ring.h"
#include "8253x.h"
#include "crc32dcl.h"

				/* turns network packet into a pseudoethernet */
				/* frame -- does ethernet stuff that 8253x does */
				/* not do -- makes minimum  64 bytes add crc, etc*/
int 
sab8253xn_write2(struct sk_buff *skb, struct net_device *dev)
{
	size_t cnt;
	unsigned int flags;
	SAB_PORT *priv = (SAB_PORT*) dev->priv;
	struct sk_buff *substitute;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
	if(dev->tbusy != 0)		/* something of an error */
	{
		++(priv->Counters.tx_drops);
		dev_kfree_skb_any(skb);
		return -EBUSY;		/* only during release */
	}
#endif

	if(priv->active2.transmit == NULL)
	{
		return -ENOMEM;
	}
	
	DEBUGPRINT((KERN_ALERT "sab8253x: sending IP packet(bytes):\n"));

	DEBUGPRINT((KERN_ALERT "sab8253x: start address is %p.\n", skb->data));
	
	cnt = skb->tail - skb->data;
	cnt = MIN(cnt, sab8253xn_rbufsize);
	if(cnt < ETH_ZLEN)
	{
		if((skb->end - skb->data) >= ETH_ZLEN)
		{
			skb->tail = (skb->data + ETH_ZLEN);
			cnt = ETH_ZLEN;
		}
		else
		{
			substitute = dev_alloc_skb(ETH_ZLEN);
			if(substitute == NULL)
			{
				dev_kfree_skb_any(skb);
				return 0;
			}
			substitute->tail = (substitute->data + ETH_ZLEN);
			memcpy(substitute->data, skb->data, cnt);
			cnt = ETH_ZLEN;
			dev_kfree_skb_any(skb);
			skb = substitute;
		}
	}
	
	save_flags(flags); cli();
	if((priv->active2.transmit->Count & OWNER) == OWN_SAB)
	{
		++(priv->Counters.tx_drops);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
		dev->tbusy = 1;
#else
		netif_stop_queue (dev);
#endif
		priv->tx_full = 1;
		restore_flags(flags);
		return 1;
	}
	restore_flags(flags);
#ifndef FREEINTERRUPT
	if(priv->active2.transmit->HostVaddr != NULL)
	{
		register RING_DESCRIPTOR *freeme;
		
		freeme = priv->active2.transmit;
		do
		{
			skb_unlink((struct sk_buff*)freeme->HostVaddr);
			dev_kfree_skb_any((struct sk_buff*)freeme->HostVaddr);
			freeme->HostVaddr = NULL;
			freeme = (RING_DESCRIPTOR*) freeme->VNext;
		}
		while(((freeme->Count & OWNER) != OWN_SAB) &&
		      (freeme->HostVaddr != NULL));
	}
#endif
	dev->trans_start = jiffies;
	skb_queue_head(priv->sab8253xbuflist, skb);
	priv->active2.transmit->HostVaddr = skb;
	priv->active2.transmit->sendcrc = 1;
	priv->active2.transmit->crcindex = 0;
	priv->active2.transmit->crc = fn_calc_memory_crc32(skb->data, cnt);
	priv->active2.transmit->Count = (OWN_SAB|cnt); /* must be this order */
	priv->active2.transmit = 
		(RING_DESCRIPTOR*) priv->active2.transmit->VNext;
	priv->Counters.transmitbytes += cnt;
	sab8253x_start_txS(priv);
	return 0;
}

	/* packetizes the received character */
	/* stream */
static void sab8253x_receive_charsN(struct sab_port *port,
				    union sab8253x_irq_status *stat)
{
	unsigned char buf[32];
	int free_fifo = 0;
	int reset_fifo = 0;
	int msg_done = 0;
	int msg_bad = 0;
	int count = 0;
	int total_size = 0;
	int rstatus = 0;
	struct sk_buff *skb;
	
	/* Read number of BYTES (Character + Status) available. */
	
	if((stat->images[ISR1_IDX] & SAB82532_ISR1_RDO) || (stat->images[ISR0_IDX] & SAB82532_ISR0_RFO) )
	{
		++msg_bad;
		++free_fifo;
		++reset_fifo;
	}
	else
	{
		if (stat->images[ISR0_IDX] & SAB82532_ISR0_RPF) 
		{
			count = port->recv_fifo_size;
			++free_fifo;
		}
		
		if (stat->images[ISR0_IDX] & SAB82532_ISR0_RME) 
		{
			count = READB(port,rbcl);
			count &= (port->recv_fifo_size - 1);
			++msg_done;
			++free_fifo;
			
			total_size = READB(port, rbch);
			if(total_size & SAB82532_RBCH_OV)
			{
				msg_bad++;
			}
			
			rstatus = READB(port, rsta);
			if((rstatus & SAB82532_RSTA_VFR) == 0)
			{
				msg_bad++;
			}
			if(rstatus & SAB82532_RSTA_RDO)
			{
				msg_bad++;
			}
			if((rstatus & SAB82532_RSTA_CRC) == 0)
			{
				msg_bad++;
			}
			if(rstatus & SAB82532_RSTA_RAB)
			{
				msg_bad++;
			}
		}
	}
	
	/* Read the FIFO. */
	(*port->readfifo)(port, buf, count);
	
	/* Issue Receive Message Complete command. */
	
	if (free_fifo) 
	{
		sab8253x_cec_wait(port);
		WRITEB(port, cmdr, SAB82532_CMDR_RMC);
	}
	
	if(reset_fifo)
	{
		sab8253x_cec_wait(port);
		WRITEB(port, cmdr, SAB82532_CMDR_RHR);
	}
	
	if(port->active2.receive == NULL)
	{
		return;
	}
	
	if(msg_bad)
	{
		++(port->Counters.rx_drops);
		port->active2.receive->HostVaddr->tail = port->active2.receive->HostVaddr->data; /* clear the buffer */
		port->active2.receive->Count = sab8253xn_rbufsize|OWN_SAB;
		return;
	}
	
	memcpy(port->active2.receive->HostVaddr->tail, buf, count);
	port->active2.receive->HostVaddr->tail += count;
	
	if(msg_done)
	{
		port->active2.receive->Count = 
			(port->active2.receive->HostVaddr->tail - port->active2.receive->HostVaddr->data);
		if((port->active2.receive->Count < (ETH_ZLEN+4+3)) || /* 4 is the CRC32 size 3 bytes from the SAB part */
		   (skb = dev_alloc_skb(sab8253xn_rbufsize), skb == NULL))
		{
			++(port->Counters.rx_drops);
			port->active2.receive->HostVaddr->tail = port->active2.receive->HostVaddr->data; 
				/* clear the buffer */
			port->active2.receive->Count = sab8253xn_rbufsize|OWN_SAB;
		}
		else
		{
			port->active2.receive->Count -= 3;
			port->active2.receive->HostVaddr->len = port->active2.receive->Count;
			port->active2.receive->HostVaddr->pkt_type = PACKET_HOST;
			port->active2.receive->HostVaddr->dev = port->dev;
			port->active2.receive->HostVaddr->protocol = 
				eth_type_trans(port->active2.receive->HostVaddr, port->dev);
			port->active2.receive->HostVaddr->tail -= 3;
			++(port->Counters.receivepacket);
			port->Counters.receivebytes += port->active2.receive->Count;
			skb_unlink(port->active2.receive->HostVaddr);
			
			netif_rx(port->active2.receive->HostVaddr);
			
			skb_queue_head(port->sab8253xbuflist, skb);
			port->active2.receive->HostVaddr = skb;
			port->active2.receive->Count = sab8253xn_rbufsize|OWN_SAB;
		}
	}
}

static void sab8253x_check_statusN(struct sab_port *port,
				   union sab8253x_irq_status *stat)
{
	int modem_change = 0;
	mctlsig_t         *sig;
	
	
	if (stat->images[ISR0_IDX] & SAB82532_ISR0_RFO) 
	{
		port->icount.buf_overrun++;
	}
	
	/* Checking DCD */
	sig = &port->dcd;
	if (stat->images[sig->irq] & sig->irqmask) 
	{
		sig->val = ISON(port,dcd);
		port->icount.dcd++;
		modem_change++;
	}
	/* Checking CTS */
	sig = &port->cts;
	if (stat->images[sig->irq] & sig->irqmask) 
	{
		sig->val = ISON(port,cts);
		port->icount.cts++;
		modem_change++;
	}
	/* Checking DSR */
	sig = &port->dsr;
	if (stat->images[sig->irq] & sig->irqmask) 
	{
		sig->val = ISON(port,dsr);
		port->icount.dsr++;
		modem_change++;
	}
	if (modem_change)
	{
		wake_up_interruptible(&port->delta_msr_wait);
	}
	
	sig = &port->dcd;
	if ((port->flags & FLAG8253X_CHECK_CD) &&
	    (stat->images[sig->irq] & sig->irqmask)) 
	{
		
		if (sig->val)
		{
			netif_carrier_on(port->dev);
		}
		else if (!((port->flags & FLAG8253X_CALLOUT_ACTIVE) &&
			   (port->flags & FLAG8253X_CALLOUT_NOHUP))) 
		{
			netif_carrier_off(port->dev);
		}
	}
#if 0				/* need to think about CTS/RTS stuff for a network driver */
	sig = &port->cts;
	if (port->flags & FLAG8253X_CTS_FLOW) 
	{				/* not setting this yet */
		if (port->tty->hw_stopped) 
		{
			if (sig->val) 
			{
				
				port->tty->hw_stopped = 0;
				sab8253x_sched_event(port, RS_EVENT_WRITE_WAKEUP);
				port->interrupt_mask1 &= ~(SAB82532_IMR1_XPR);
				WRITEB(port, imr1, port->interrupt_mask1);
				sab8253x_start_txS(port);
			}
		} 
		else 
		{
			if (!(sig->val)) 
			{
				port->tty->hw_stopped = 1;
			}
		}
	}
#endif
}

static void Sab8253xCollectStats(struct net_device *dev)
{
	
	struct net_device_stats *statsp = 
		&((SAB_PORT*) dev->priv)->stats;
	
	memset(statsp, 0, sizeof(struct net_device_stats));
	
	statsp->rx_packets +=
		((SAB_PORT*)dev->priv)->Counters.receivepacket;
	statsp->tx_packets +=
		((SAB_PORT*)dev->priv)->Counters.transmitpacket;
	statsp->tx_dropped += 
		((SAB_PORT*)dev->priv)->Counters.tx_drops;
	statsp->rx_dropped += 
		((SAB_PORT*)dev->priv)->Counters.rx_drops;
}

struct net_device_stats *sab8253xn_stats(struct net_device *dev)
{
	SAB_PORT *priv = (SAB_PORT*) dev->priv;
	
	Sab8253xCollectStats(dev);
	return &priv->stats;
}

/* minimal ioctls -- more to be added later */
int sab8253xn_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	
	SAB_PORT *priv = (SAB_PORT*) dev->priv;
	
	switch(cmd)
	{
	case SAB8253XCLEARCOUNTERS:
		memset(&priv->Counters, 0, sizeof(struct counters));
		break;
		
	default:
		break;
	}
	return 0;
}

#if 0
static int sab8253x_block_til_readyN(SAB_PORT *port)
{
	DECLARE_WAITQUEUE(wait, current);
	int retval;
	int do_clocal = 0;
	unsigned long	flags;
	
	/*
	 * If the device is in the middle of being closed, then block
	 * until it's done, and then try again.
	 */
	if (port->flags & FLAG8253X_CLOSING)
	{
		if (port->flags & FLAG8253X_CLOSING)
		{
			interruptible_sleep_on(&port->close_wait);
		}
#ifdef SERIAL_DO_RESTART
		if (port->flags & FLAG8253X_HUP_NOTIFY)
		{
			return -EAGAIN;
		}
		else
		{
			return -ERESTARTSYS;
		}
#else
		return -EAGAIN;
#endif
	}
	
	/*
	 * this is not a callout device
	 */
	
	/* suppose callout active */
	if (port->flags & FLAG8253X_CALLOUT_ACTIVE) 
	{
		if (port->normal_termios.c_cflag & CLOCAL)
		{
			do_clocal = 1;
		}
	} 
	
	/*
	 * Block waiting for the carrier detect and the line to become
	 * free (i.e., not in use by the callout).  While we are in
	 * this loop, port->count is dropped by one, so that
	 * sab8253x_close() knows when to free things.  We restore it upon
	 * exit, either normal or abnormal.
	 */
	retval = 0;
	add_wait_queue(&port->open_wait, &wait);
	port->blocked_open++;
	while (1) 
	{
		save_flags(flags); cli();
		if (!(port->flags & FLAG8253X_CALLOUT_ACTIVE))
		{
			RAISE(port,dtr);
			RAISE(port,rts);	/* maybe not correct for sync */
			/*
			 * ??? Why changing the mode here? 
			 *  port->regs->rw.mode |= SAB82532_MODE_FRTS;
			 *  port->regs->rw.mode &= ~(SAB82532_MODE_RTS);
			 */
		}
		restore_flags(flags);
		current->state = TASK_INTERRUPTIBLE;
		if (!(port->flags & FLAG8253X_INITIALIZED)) 
		{
#ifdef SERIAL_DO_RESTART
			if (port->flags & FLAG8253X_HUP_NOTIFY)
			{
				retval = -EAGAIN;
			}
			else
			{
				retval = -ERESTARTSYS;	
			}
#else
			retval = -EAGAIN;
#endif
			break;
		}
		if (!(port->flags & FLAG8253X_CALLOUT_ACTIVE) &&
		    !(port->flags & FLAG8253X_CLOSING) &&
		    (do_clocal || ISON(port,dcd))) 
		{
			break;
		}
#ifdef DEBUG_OPEN
		printk("block_til_readyN:2 flags = 0x%x\n",port->flags);
#endif
		if (signal_pending(current)) 
		{
			retval = -ERESTARTSYS;
			break;
		}
		schedule();
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&port->open_wait, &wait);
	port->blocked_open--;
	if (retval)
	{
		return retval;
	}
	port->flags |= FLAG8253X_NORMAL_ACTIVE; /* is this a good flag? */
	return 0;
}
#endif

int sab8253x_startupN(struct sab_port *port)
{
	unsigned long flags;
	int retval = 0;
	
	save_flags(flags); cli();
	
	if (port->flags & FLAG8253X_INITIALIZED) 
	{
		goto errout;
	}
	
	if (!port->regs) 
	{
		retval = -ENODEV;
		goto errout;
	}
	/*
	 * Initialize the Hardware
	 */
	sab8253x_init_lineS(port);	/* nothing in this function
					 * refers to tty structure */
	
	/* Activate RTS */
	RAISE(port,rts);
	/* Activate DTR */
	RAISE(port,dtr);
	/*
	 * Initialize the modem signals values
	 */
	port->dcd.val=ISON(port,dcd);
	port->cts.val=ISON(port,cts);
	port->dsr.val=ISON(port,dsr);
	/*
	 * Finally, enable interrupts
	 */
	
	port->interrupt_mask0 = SAB82532_IMR0_RFS | SAB82532_IMR0_PCE |
		SAB82532_IMR0_PLLA | SAB82532_IMR0_RSC | SAB82532_IMR0_CDSC;
	/*((port->ccontrol.ccr2 & SAB82532_CCR2_TOE) ? SAB82532_IMR0_CDSC : 0); */
	
	WRITEB(port,imr0,port->interrupt_mask0);
	port->interrupt_mask1 = SAB82532_IMR1_EOP | SAB82532_IMR1_XMR |
		SAB82532_IMR1_TIN | SAB82532_IMR1_XPR;
	WRITEB(port, imr1, port->interrupt_mask1);
	port->all_sent = 1;
	
	
	/*
	 * and set the speed of the serial port
	 */
	sab8253x_change_speedN(port);
	
	port->flags |= FLAG8253X_INITIALIZED; /* bad name for indicating to other functionalities status */
	port->receive_chars = sab8253x_receive_charsN;
	port->transmit_chars = sab8253x_transmit_charsS;
	port->check_status = sab8253x_check_statusN;
	port->receive_test = (SAB82532_ISR0_RME | SAB82532_ISR0_RFO | SAB82532_ISR0_RPF);
	port->transmit_test = (SAB82532_ISR1_ALLS | SAB82532_ISR1_RDO | SAB82532_ISR1_XPR |
			       SAB82532_ISR1_XDU | SAB82532_ISR1_CSC);
	port->check_status_test = (SAB82532_ISR1_CSC);
	
	/*((port->ccontrol.ccr2 & SAB82532_CCR2_TOE) ? 0 : SAB82532_ISR0_CDSC));*/
	
	restore_flags(flags);
	return 0;
	
 errout:
	restore_flags(flags);
	return retval;
}

int sab8253xn_open(struct net_device *dev)
{
	unsigned int retval;
	SAB_PORT *priv = (SAB_PORT*) dev->priv;
	
	if(priv->function != FUNCTION_NR)
	{
		return -ENODEV;		/* only allowed if there are no restrictions on the port */
	}
	
	
	if(priv->flags & FLAG8253X_CLOSING) /* try again after the TTY close finishes */
	{	
#ifdef SERIAL_DO_RESTART
		return ((priv->flags & FLAG8253X_HUP_NOTIFY) ?
			-EAGAIN : -ERESTARTSYS); /* The ifconfig UP will just fail */
#else
		return -EAGAIN;
#endif
	}
	
	/*
	 * Maybe start up serial port -- may already be running a TTY
	 */
	if(priv->flags & FLAG8253X_NORMAL_ACTIVE) /* probably should be a test open at all */
	{
		return -EBUSY;	/* can't reopen in NET */
	}
	
	if(Sab8253xSetUpLists(priv))
	{
		return -ENODEV;
	}
	
	if(Sab8253xInitDescriptors2(priv, sab8253xn_listsize, sab8253xn_rbufsize))
	{
		Sab8253xCleanUpTransceiveN(priv);
		return -ENODEV;
	}
	netif_carrier_off(dev);
	
	priv->open_type = OPEN_SYNC_NET;
	priv->tty = 0;
	
	retval = sab8253x_startupN(priv);
	if (retval)
	{
		Sab8253xCleanUpTransceiveN(priv);
		return retval;		
	}
	
	priv->flags |= FLAG8253X_NETWORK; /* flag the call out driver that it has to reinitialize the port */
	priv->tx_full = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
	dev->start = 1;
	dev->tbusy = 0;
#else
	netif_start_queue(dev);
#endif

	priv->flags |= FLAG8253X_NORMAL_ACTIVE; /* is this a good flag? */
	MOD_INC_USE_COUNT;
	return 0;			/* success */
}
/* stop the PPC, free all skbuffers */
int sab8253xn_release(struct net_device *dev)	/* stop */
{
	SAB_PORT *priv = (SAB_PORT*) dev->priv;
	unsigned long flags;
	
	printk(KERN_ALERT "sab8253xn: network interface going down.\n");
	save_flags(flags); cli();
	
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
	dev->start = 0;
	dev->tbusy = 1;
#else
	netif_stop_queue (dev);
#endif
	
	sab8253x_shutdownN(priv);
	Sab8253xCleanUpTransceiveN(priv);
	netif_carrier_off(dev);
	priv->flags &= ~FLAG8253X_NETWORK; 
	priv->flags &= ~(FLAG8253X_NORMAL_ACTIVE|/*FLAG8253X_CALLOUT_ACTIVE|*/
			 FLAG8253X_CLOSING);
	priv->open_type = OPEN_NOT;
	MOD_DEC_USE_COUNT;
	restore_flags(flags);
	return 0;
}

SAB_PORT *current_sab_port = NULL;

int sab8253xn_init(struct net_device *dev)
{
	
	SAB_PORT *priv;
	
	printk(KERN_ALERT "sab8253xn: initializing SAB8253X network driver instance.\n");
	
	priv = current_sab_port;
	dev->priv = priv;
	
	if(dev->priv == NULL)
	{
		printk(KERN_ALERT "sab8253xn: could not find active port!\n");
		return -ENOMEM;
	}
	priv->dev = dev;
	
	ether_setup(dev);
	
	dev->irq = priv->irq;
	dev->hard_start_xmit = sab8253xn_write2;
	dev->do_ioctl = sab8253xn_ioctl;
	dev->open = sab8253xn_open;
	dev->stop = sab8253xn_release;
	dev->get_stats = sab8253xn_stats;
	dev->base_addr = (unsigned) priv->regs;
	/* should I do a request region here */
	priv->next_dev = Sab8253xRoot;
	Sab8253xRoot = dev;
	return 0;
}

