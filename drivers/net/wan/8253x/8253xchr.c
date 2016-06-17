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
#include <linux/poll.h>
#include "Reg9050.h"
#include "8253xctl.h"
#include "ring.h"
#include "8253x.h"
#include "crc32dcl.h"

/* a raw character driver  -- theoretically for implementing custom protocols,
 * async interrupts can be used for getting indication that a packet has
 * been successfully transmitted.
 */


				/* the application read routine, can block according */
				/* to flag, returns one packet at a time */
int sab8253xc_read(struct file *filep, char *cptr, size_t cnt, loff_t *loffp)
{
	unsigned int length;
	unsigned long flags;
	SAB_PORT *port = filep->private_data;
	struct sk_buff *skb;
	
	DEBUGPRINT((KERN_ALERT "Attempting to read %i bytes.\n", cnt));
	
	
	if(port->sab8253xc_rcvbuflist == NULL)
	{
		return -ENOMEM;
	}
	
	save_flags(flags); cli();  
	if(skb_queue_len(port->sab8253xc_rcvbuflist) == 0)
	{
		port->rx_empty = 1;
		if(filep->f_flags & O_NONBLOCK)
		{
			restore_flags(flags);
			return -EAGAIN;
		}
		restore_flags(flags);
		interruptible_sleep_on(&port->read_wait);
	}
	else
	{
		restore_flags(flags);
	}
	
	skb = skb_peek(port->sab8253xc_rcvbuflist);
	length = skb->tail - skb->data;
	if(cnt < length)
	{
		return -ENOMEM;
	}
	
	skb = skb_dequeue(port->sab8253xc_rcvbuflist);
	
	save_flags(flags); cli();  
	if(skb_queue_len(port->sab8253xc_rcvbuflist) <= 0)
	{
		port->rx_empty = 1;
	}
	restore_flags(flags);
	
	DEBUGPRINT((KERN_ALERT "Copying to user space %s.\n", skb->data));
	copy_to_user(cptr, skb->data, length);
	dev_kfree_skb_any(skb);
	return length;
}

/* application write */

int sab8253xc_write(struct file *filep, const char *cptr, size_t cnt, loff_t *loffp)
{
	struct sk_buff *skb;
	unsigned long flags;
	SAB_PORT *port = filep->private_data;
	
	if(cnt > sab8253xc_rbufsize)	/* should not send bigger than can be received */
	{
		return -ENOMEM;
	}
	
	if(port->active2.transmit == NULL)
	{
		return -ENOMEM;
	}
	
	save_flags(flags); cli();	/* can block on write when */
	/* no space in transmit circular */
	/* array. */
	if((port->active2.transmit->Count & OWNER) == OWN_SAB)
	{
		++(port->Counters.tx_drops);
		port->tx_full = 1;
		restore_flags(flags);
		if(filep->f_flags & O_NONBLOCK)
		{
			return -EAGAIN;
		}
		interruptible_sleep_on(&port->write_wait);
	}
	else
	{
		restore_flags(flags);
	}
	
#ifndef FREEINTERRUPT
	if((port->active2.transmit->HostVaddr != NULL) || /* not OWN_SAB from above */
	   (port->active2.transmit->crcindex != 0))
	{
		register RING_DESCRIPTOR *freeme;
		
		freeme = port->active2.transmit;
		do
		{
			if((freeme->crcindex == 0) && (freeme->HostVaddr == NULL))
			{
				break;
			}
			if(freeme->HostVaddr)
			{
				skb_unlink((struct sk_buff*)freeme->HostVaddr);
				dev_kfree_skb_any((struct sk_buff*)freeme->HostVaddr);
				freeme->HostVaddr = NULL;
			}
			freeme->sendcrc = 0;
			freeme->crcindex = 0;
			freeme = (RING_DESCRIPTOR*) freeme->VNext;
		}
		while((freeme->Count & OWNER) != OWN_SAB);
	}
#endif
	
	skb = alloc_skb(cnt, GFP_KERNEL); /* not called from int as with tty */
	if(skb == NULL)
	{
		return -ENOMEM;
	}
	copy_from_user(skb->data, cptr, cnt);
	skb->tail = (skb->data + cnt);
	skb->len = cnt;
	skb->data_len = cnt;
	
	skb_queue_head(port->sab8253xbuflist, skb);
	port->active2.transmit->HostVaddr = skb;
	port->active2.transmit->sendcrc = 0;
	port->active2.transmit->crcindex = 0;
	port->active2.transmit->Count = (OWN_SAB|cnt); /* must be this order */
	port->active2.transmit = 
		(RING_DESCRIPTOR*) port->active2.transmit->VNext;
	port->Counters.transmitbytes += cnt;
	sab8253x_start_txS(port);
	return cnt;
}

static void sab8253x_receive_charsC(struct sab_port *port,
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
			count = READB(port, rbcl);
			count &= (port->recv_fifo_size - 1);
			++msg_done;
			++free_fifo;
			
			total_size = READB(port, rbch);
			if(total_size & SAB82532_RBCH_OV) /* need to revisit for 4096 byte frames */
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
	
	if(msg_bad)
	{
		port->msgbufindex = 0;
		return;
	}
	
	memcpy(&port->msgbuf[port->msgbufindex], buf, count);
	port->msgbufindex += count;
	
	if(msg_done)
	{
		
		if(port->msgbufindex <= 3) /* min is 1 char + 2 CRC + status byte */
		{
			port->msgbufindex = 0;
			return;
		}
		
		total_size = port->msgbufindex - 3; /* strip off the crc16 and the status byte */
		port->msgbufindex = 0;
		
		/* ignore the receive buffer waiting -- we know the correct size here */
		
		if(skb = dev_alloc_skb(total_size), skb)
		{
			memcpy(skb->data, &port->msgbuf[0], total_size);
			skb->tail = (skb->data + total_size);
			skb->data_len = total_size;
			skb->len = total_size;
			skb_queue_tail(port->sab8253xc_rcvbuflist, skb);
			if(port->rx_empty)
			{
				port->rx_empty = 0;
				wake_up_interruptible(&port->read_wait);
			}
			if(port->async_queue)
			{
				kill_fasync(&port->async_queue, SIGIO, POLL_IN);
			}
		}
	}
}

static void sab8253x_check_statusC(struct sab_port *port,
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
			wake_up_interruptible(&port->open_wait); /* in case waiting in block_til_ready */
		}
		else if (!((port->flags & FLAG8253X_CALLOUT_ACTIVE) &&
			   (port->flags & FLAG8253X_CALLOUT_NOHUP))) 
		{
			/* I think the code needs to walk through all the proces that have opened this
			 * port and send a SIGHUP to them -- need to investigate somewhat more*/
		}
	}
}

static int sab8253x_startupC(struct sab_port *port)
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
#if 0
	((port->ccontrol.ccr2 & SAB82532_CCR2_TOE) ? SAB82532_IMR0_CDSC : 0); /* the weird way the cards work
									       * when clocking CD seems to
									       *  monitor txclk*/
#endif
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
	port->receive_chars = sab8253x_receive_charsC;
	port->transmit_chars = sab8253x_transmit_charsS;
	port->check_status = sab8253x_check_statusC;
	port->receive_test = (SAB82532_ISR0_RME | SAB82532_ISR0_RFO | SAB82532_ISR0_RPF);
	port->transmit_test = (SAB82532_ISR1_ALLS | SAB82532_ISR1_RDO | SAB82532_ISR1_XPR |
			       SAB82532_ISR1_XDU | SAB82532_ISR1_CSC);
	port->check_status_test = SAB82532_ISR1_CSC;
	
	restore_flags(flags);
	return 0;
	
 errout:
	restore_flags(flags);
	return retval;
}

static int sab8253x_block_til_readyC(struct file* filp, struct sab_port *port)
{
	DECLARE_WAITQUEUE(wait, current);
	int retval;
	int do_clocal = 1;		/* cheating -- I need to understand how
					   signals behave synchronously better*/
	unsigned long flags;
	
	/*
	 * If the device is in the middle of being closed, then block
	 * until it's done, and then try again.
	 */
	if (port->flags & FLAG8253X_CLOSING)
	{
		interruptible_sleep_on(&port->close_wait); /* finish up previous close */
		
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
	
	/* sort out async vs sync tty, not call out */
	/*
	 * If non-blocking mode is set, or the port is not enabled,
	 * then make the check up front and then exit.
	 */
	
	if (filp->f_flags & O_NONBLOCK) 
	{
		if (port->flags & FLAG8253X_CALLOUT_ACTIVE)
		{
			return -EBUSY;
		}
		port->flags |= FLAG8253X_NORMAL_ACTIVE;
		return 0;
	}
	
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
	
	/* The port decrement logic is probably */
	/* broken -- hence if def'd out -- it does*/
	retval = 0;
	add_wait_queue(&port->open_wait, &wait); /* starts the wait but does not block here */
	port->blocked_open++;
	while (1)			/* on some devices when providing clock have to just assume connection */
	{
		save_flags(flags);
		cli();
		if (!(port->flags & FLAG8253X_CALLOUT_ACTIVE))
		{
			RAISE(port, dtr);
			RAISE(port, rts);	/* maybe not correct for sync */
			/*
			 * ??? Why changing the mode here? 
			 *  port->regs->rw.mode |= SAB82532_MODE_FRTS;
			 *  port->regs->rw.mode &= ~(SAB82532_MODE_RTS);
			 */
		}
		restore_flags(flags);;
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
		printk("sab8253x_block_til_ready:2 flags = 0x%x\n",port->flags);
#endif
		if (signal_pending(current)) 
		{
			retval = -ERESTARTSYS;
			break;
		}
#ifdef DEBUG_OPEN
		printk("sab8253x_block_til_readyC blocking: ttyS%d, count = %d, flags = %x, clocal = %d, vstr = %02x\n",
		       port->line, port->count, port->flags, do_clocal, READB(port,vstr));
#endif
		schedule();
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&port->open_wait, &wait);
	port->blocked_open--;
	
#ifdef DEBUG_OPEN
	printk("sab8253x_block_til_ready after blockingC: ttys%d, count = %d\n",
	       port->line, port->count);
#endif
	
	if (retval)
	{
		return retval;
	}
	port->flags |= FLAG8253X_NORMAL_ACTIVE;
	return 0;
}


int sab8253xc_open(struct inode *inodep, struct file *filep)
{
	unsigned int line;
	unsigned int retval;
	unsigned int counter;
	SAB_PORT *port;
	
	line = MINOR(inodep->i_rdev);	/* let's find which physical device to use */
	/* minor dev number indexes through the port */
	/* list */
	
	for(counter = 0, port = AuraPortRoot; 
	    (counter < line) && (port != NULL); 
	    ++counter)
	{
		port = port->next;
	}
	
	
	if (!port) 
	{
		printk(KERN_ALERT "sab8253xc_open: can't find structure for line %d\n",
		       line);
		return -ENODEV;
	}
	
	if(port->function == FUNCTION_NA)
	{				/* port 2 on 1020s and 1520s */
		return -ENODEV;
	}
	
	switch(port->open_type)
	{
	case OPEN_ASYNC:
		if(!(port->flags & FLAG8253X_CALLOUT_ACTIVE))
		{
			return -EBUSY;
		}
		break;
		
	case OPEN_SYNC_CHAR:
	case OPEN_NOT:
		port->tty = NULL;
		port->open_type = OPEN_SYNC_CHAR;
		break;
		
	default:
		return -EBUSY;
	}
	
	/*
	 * Maybe start up serial port -- may already be running in callout mode
	 */
	
	if(Sab8253xSetUpLists(port))
	{
		if(port->open_type == OPEN_SYNC_CHAR)
		{
			port->open_type = OPEN_NOT;
		}
		return -ENODEV;
	}
	if(Sab8253xInitDescriptors2(port, sab8253xc_listsize, sab8253xc_rbufsize))
	{
		Sab8253xCleanUpTransceiveN(port);	/* the network functions should be okay -- only difference */
		/* is the crc32 that is appended */
		if(port->open_type == OPEN_SYNC_CHAR)
		{
			port->open_type = OPEN_NOT;
		}
		return -ENODEV;
	}
	retval = sab8253x_startupC(port); /* does not do anything if call out active */
	if (retval)
	{
		if(port->open_type == OPEN_SYNC_CHAR)
		{
			port->open_type = OPEN_NOT;
		}
		return retval;		
	}
	
	MOD_INC_USE_COUNT;		/* might block */
	/* note logic different from tty
	   open failure does not call the
	   close routine */
	retval = sab8253x_block_til_readyC(filep, port);	/* need to wait for completion of callout */
	if(retval)
	{
		if(port->open_type == OPEN_SYNC_CHAR)
		{
			port->open_type = OPEN_NOT;
		}
		MOD_DEC_USE_COUNT;	/* something went wrong */
		return retval;
	}
	
	port->tty = NULL;
	port->open_type = OPEN_SYNC_CHAR;
	if(Sab8253xSetUpLists(port))
	{
		port->open_type = OPEN_NOT;
		return -ENODEV;
	}
	if(Sab8253xInitDescriptors2(port, sab8253xc_listsize, sab8253xc_rbufsize))
	{
		port->open_type = OPEN_NOT;
		Sab8253xCleanUpTransceiveN(port);	/* the network functions should be okay -- only difference */
		/* is the crc32 that is appended */
		return -ENODEV;
	}
	retval = sab8253x_startupC(port); /* ditto */
	if (retval)
	{
		port->open_type = OPEN_NOT;
		Sab8253xCleanUpTransceiveN(port);
		return retval;		
	}
	port->tx_full = 0;
	port->rx_empty = 1;
	port->count++;
	port->session = current->session;
	port->pgrp = current->pgrp;
	filep->private_data = port;
	MOD_INC_USE_COUNT;
	return 0;			/* success */
}

int sab8253xc_release(struct inode *inodep, struct file *filep)
{
	SAB_PORT *port = (SAB_PORT*) filep->private_data;
	unsigned long flags;
	
	save_flags(flags); cli();
	
	--(port->count);
	if(port->count <= 0)
	{
		sab8253x_shutdownN(port);
		Sab8253xCleanUpTransceiveN(port);
		port->count = 0;
		port->open_type = OPEN_NOT;
	}
	sab8253xc_fasync(-1, filep, 0);
	MOD_DEC_USE_COUNT;
	restore_flags(flags);
	return 0;
}

unsigned int sab8253xc_poll(struct file *fileobj, struct poll_table_struct *polltab)
{
	SAB_PORT *port = fileobj->private_data;
	unsigned int mask = 0;
	
	poll_wait(fileobj, &port->write_wait, polltab);
	poll_wait(fileobj, &port->read_wait, polltab);
	if(port->rx_empty == 0)
	{
		mask |= POLLIN | POLLRDNORM;
	}
	if(port->tx_full == 0)
	{
		mask |= POLLOUT | POLLWRNORM;
	}
	return mask;
}

int sab8253xc_ioctl(struct inode *iobj, struct file *fileobj, unsigned int cmd, unsigned long length)
{
	return 0;
}

int sab8253xc_fasync(int fd, struct file * fileobj, int mode)
{
	SAB_PORT *port = fileobj->private_data;
	
	return fasync_helper(fd, fileobj, mode, &port->async_queue); /* I am a little baffled -- does async_helper */
				/* work on the basis of a port or on an open */
				/* basis*/
}

