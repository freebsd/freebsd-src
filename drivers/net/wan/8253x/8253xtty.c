/* -*- linux-c -*- */
/* $Id: 8253xtty.c,v 1.23 2002/02/10 22:17:25 martillo Exp $
 * sab82532.c: ASYNC Driver for the SIEMENS SAB82532 DUSCC.
 *
 * Copyright (C) 1997  Eddie C. Dost  (ecd@skynet.be)
 *
 * Modified by Francois Wautier 2000 (fw@auroratech.com)
 *
 * Extended extensively by Joachim Martillo 2001 (Telford002@aol.com)
 * 	to provide synchronous/asynchronous TTY/Callout/character/network device
 * 	capabilities.
 *
 * Modifications and extensions
 * Copyright (C) 2001 By Joachim Martillo, Telford Tools, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */


/* Standard in kernel modules */
#define DEFINE_VARIABLE
#include <linux/module.h>   /* Specifically, a module */
#include <asm/io.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/mm.h>
#include <linux/version.h>
#include <asm/uaccess.h>
#include <linux/pci.h>
#include "8253xctl.h"
#include "sp502.h"

DECLARE_TASK_QUEUE(tq_8253x_serial); /* this just initializes a list head called */
				     /* tq_8253x_serial*/

struct tty_driver sab8253x_serial_driver, sab8253x_callout_driver, sync_sab8253x_serial_driver;
int sab8253x_refcount;

/* Trace things on serial device, useful for console debugging: */
#undef SERIAL_LOG_DEVICE

#ifdef SERIAL_LOG_DEVICE
static void dprint_init(int tty);
#endif

static void sab8253x_change_speed(struct sab_port *port);

static struct tty_struct **sab8253x_tableASY = 0;	/* make dynamic */
static struct tty_struct **sab8253x_tableCUA = 0;	/* make dynamic */
static struct tty_struct **sab8253x_tableSYN = 0;	/* make dynamic */
static struct termios **sab8253x_termios = 0 ;
static struct termios **sab8253x_termios_locked = 0;

#ifdef MODULE
#undef XCONFIG_SERIAL_CONSOLE	/* leaving out CONFIG_SERIAL_CONSOLE for now */
#endif

#ifdef XCONFIG_SERIAL_CONSOLE	/* not really implemented yet */
extern int serial_console;
struct console sab8253x_console;
int sab8253x_console_init(void);
#endif

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

char sab8253x_serial_version[16];

static void sab8253x_flush_to_ldisc(void *private_)
{
	struct tty_struct *tty = (struct tty_struct *) private_;
	unsigned char	*cp;
	char		*fp;
	int		count;
	struct sab_port *port;
	struct sk_buff *skb;  
	
	if(tty)
	{
		port = (struct sab_port *)tty->driver_data; /* probably a silly check */
	}
	else
	{
		return;
	}
	
	if(!port)
	{
		return;
	}
	
	if (test_bit(TTY_DONT_FLIP, &tty->flags)) 
	{
		queue_task(&tty->flip.tqueue, &tq_timer);
		return;
	}
	/* note that a hangup may have occurred -- perhaps should check for that */
	port->DoingInterrupt = 1;
	while(port->sab8253xc_rcvbuflist && (skb_queue_len(port->sab8253xc_rcvbuflist) > 0))
	{
		skb = skb_dequeue(port->sab8253xc_rcvbuflist);
		count = skb->data_len;
		cp = skb->data;
		fp = skb->data + (count/2);
		(*tty->ldisc.receive_buf)(tty, cp, fp, count/2);
		dev_kfree_skb_any(skb);
	}
	port->DoingInterrupt = 0;
}

/* only used asynchronously */
static void inline sab8253x_tec_wait(struct sab_port *port)
{
	int count = port->tec_timeout;
	
	while((READB(port, star) & SAB82532_STAR_TEC) && --count)
	{
		udelay(1);
	}
}

void sab8253x_start_tx(struct sab_port *port)
{
	unsigned long flags;
	register int count;
	register int total;
	register int offset;
	char temporary[32];
	register unsigned int slopspace;
	register int sendsize;
	unsigned int totaltransmit;
	unsigned fifospace;
	unsigned loadedcount;
	struct tty_struct *tty = port->tty;
	
	fifospace = port->xmit_fifo_size;
	loadedcount = 0;
	
	if(port->sabnext2.transmit == NULL)
	{
		return;
	}
	
	save_flags(flags); 
	cli();			
	
	
	while(count = port->sabnext2.transmit->Count, (count & OWNER) == OWN_SAB)
	{
		count &= ~OWN_SAB; /* OWN_SAB is really 0 but cannot guarantee in the future */
		
		if(port->sabnext2.transmit->HostVaddr)
		{
			total = (port->sabnext2.transmit->HostVaddr->tail - 
				 port->sabnext2.transmit->HostVaddr->data); /* packet size */
		}
		else
		{
			total = 0;		/* the data is only in the crc/trailer */
		}
		
		if(tty && (tty->stopped || tty->hw_stopped))
		{			/* works for frame that only has a trailer (crc) */
			port->interrupt_mask1 |= SAB82532_IMR1_XPR;
			WRITEB(port, imr1, port->interrupt_mask1);
			restore_flags(flags);	/* can't send */
			return;
		}
		
		offset = (total - count);	/* offset to data still to send */
		
		port->interrupt_mask1 &= ~(SAB82532_IMR1_ALLS);
		WRITEB(port, imr1, port->interrupt_mask1);
		port->all_sent = 0;
		
		
		if(READB(port,star) & SAB82532_STAR_XFW)
		{
			if(count <= fifospace)
			{
				port->xmit_cnt = count;
				slopspace = 0;
				sendsize = 0;
				if(port->sabnext2.transmit->sendcrc) 
				/* obviously should not happen for async but might use for
				   priority transmission */
				{
					slopspace = fifospace - count;
				}
				if(slopspace)
				{
					if(count)
					{
						memcpy(temporary, &port->sabnext2.transmit->HostVaddr->data[offset], 
						       count);
					}
					sendsize = MIN(slopspace, (4 - port->sabnext2.transmit->crcindex)); 
				/* how many bytes to send */
					memcpy(&temporary[count], 
					       &((unsigned char*)(&port->sabnext2.transmit->crc))
					       [port->sabnext2.transmit->crcindex], 
					       sendsize);
					port->sabnext2.transmit->crcindex += sendsize;
					if(port->sabnext2.transmit->crcindex >= 4)
					{
						port->sabnext2.transmit->sendcrc = 0;
					}
					port->xmit_buf = temporary;
				}
				else
				{
					port->xmit_buf =	/* set up wrifefifo variables */
						&port->sabnext2.transmit->HostVaddr->data[offset];
				}
				port->xmit_cnt += sendsize;
				count = 0;
			}
			else
			{
				count -= fifospace;
				port->xmit_cnt = fifospace;
				port->xmit_buf =	/* set up wrifefifo variables */
					&port->sabnext2.transmit->HostVaddr->data[offset];
				
			}
			port->xmit_tail= 0;
			loadedcount = port->xmit_cnt;
			(*port->writefifo)(port);
			totaltransmit = Sab8253xCountTransmitDescriptors(port);
			if((sab8253xt_listsize - totaltransmit) > 2) 
			{
				sab8253x_sched_event(port, SAB8253X_EVENT_WRITE_WAKEUP);
			}
			
			if((sab8253xt_listsize - totaltransmit) > (sab8253xt_listsize/2))
			{
				port->buffergreedy = 0;
			}
			else
			{
				port->buffergreedy = 1;
			}
			
			port->xmit_buf = NULL; /* this var is used to indicate whether to call kfree */
			
			fifospace -= loadedcount;
			
			if ((count <= 0) && (port->sabnext2.transmit->sendcrc == 0))
			{
				port->sabnext2.transmit->Count = OWN_DRIVER;
#ifdef FREEININTERRUPT		/* treat this routine as if taking place in interrupt */
				if(port->sabnext2.transmit->HostVaddr)
				{
					skb_unlink(port->sabnext2.transmit->HostVaddr);
					dev_kfree_skb_any(port->sabnext2.transmit->HostVaddr);
					port->sabnext2.transmit->HostVaddr = 0; /* no skb */
				}
				port->sabnext2.transmit->crcindex = 0; /* no single byte */
#endif
				port->sabnext2.transmit = port->sabnext2.transmit->VNext;
				if((port->sabnext2.transmit->Count & OWNER) == OWN_SAB)
				{
					if(fifospace > 0)
					{
						continue;	/* the only place where this code really loops */
					}
					if(fifospace < 0)
					{
						printk(KERN_ALERT "sab8253x:  bad math in interrupt handler.\n");
					}
					port->interrupt_mask1 &= ~(SAB82532_IMR1_XPR);
					WRITEB(port, imr1, port->interrupt_mask1);
				}
				else
				{
					port->interrupt_mask1 |= SAB82532_IMR1_XPR;
					WRITEB(port, imr1, port->interrupt_mask1);
				}
				sab8253x_cec_wait(port);
				/* Issue a Transmit Frame command. */
				WRITEB(port, cmdr, SAB82532_CMDR_XF); 
				/* This could be optimized to load from next skbuff */
				/* SAB82532_CMDR_XF is the same as SAB82532_CMDR_XTF */
				restore_flags(flags);
				return;
			}
			sab8253x_cec_wait(port);
			/* Issue a Transmit Frame command. */
			WRITEB(port, cmdr, SAB82532_CMDR_XF);	/* same as SAB82532_CMDR_XTF */
			port->sabnext2.transmit->Count = (count|OWN_SAB);
		}
		port->interrupt_mask1 &= ~(SAB82532_IMR1_XPR);
		WRITEB(port, imr1, port->interrupt_mask1);
		restore_flags(flags);
		return;
	}
	/*  The While loop only exits via return*/
	/* we get here by skipping the loop  */
	port->interrupt_mask1 |= SAB82532_IMR1_XPR;
	WRITEB(port, imr1, port->interrupt_mask1);
	restore_flags(flags);
	return;
}

/*
 * ------------------------------------------------------------
 * sab8253x_stop() and sab8253x_start()
 *
 * This routines are called before setting or resetting tty->stopped.
 * They enable or disable transmitter interrupts, as necessary.
 * ------------------------------------------------------------
 */

static void sab8253x_stop(struct tty_struct *tty)
{
	struct sab_port *port = (struct sab_port *)tty->driver_data;
	unsigned long flags;
	
	if (sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_stop"))
	{
		return;
	}
	
	save_flags(flags); 
	cli();	/* maybe should turn off ALLS as well
		   but the stop flags are checked
		   so ALLS is probably harmless
		   and I have seen too much evil
		   associated with that interrupt*/
	port->interrupt_mask1 |= SAB82532_IMR1_XPR;
	WRITEB(port, imr1, port->interrupt_mask1);
	restore_flags(flags);
}

static void sab8253x_start(struct tty_struct *tty)
{
	struct sab_port *port = (struct sab_port *)tty->driver_data;
	
	
	if (sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_start"))
	{
		return;
	}
	
	sab8253x_start_tx(port);
}

/*
 * This routine is used by the interrupt handler to schedule
 * processing in the software interrupt portion of the driver.
 */
/* no obvious changes for sync tty */

static void sab8253x_receive_chars(struct sab_port *port,
			    union sab8253x_irq_status *stat)
{
	struct tty_struct *tty = port->tty;
	unsigned char buf[32];
	unsigned char reordered[32];
	unsigned char status;
	int free_fifo = 0;
	int i, count = 0;
	struct sk_buff *skb;
	
	/* Read number of BYTES (Character + Status) available. */
	if (stat->images[ISR0_IDX] & SAB82532_ISR0_RPF) 
	{
		count = port->recv_fifo_size;
		free_fifo++;
	}
	
	if (stat->images[ISR0_IDX] & SAB82532_ISR0_TCD) 
	{
		count = READB(port,rbcl) & (port->recv_fifo_size - 1);
		free_fifo++;
	}
	
	/* Issue a FIFO read command in case we where idle. */
	if (stat->sreg.isr0 & SAB82532_ISR0_TIME) 
	{
		sab8253x_cec_wait(port);
		WRITEB(port, cmdr, SAB82532_CMDR_RFRD);
	}
	
	if (stat->images[ISR0_IDX] & SAB82532_ISR0_RFO) 
	{				/* FIFO overflow */
		free_fifo++;
	}
	
	/* Read the FIFO. */
	(*port->readfifo)(port, buf, count);
	
	/* Issue Receive Message Complete command. */
	if (free_fifo) 
	{
		sab8253x_cec_wait(port);
		WRITEB(port, cmdr, SAB82532_CMDR_RMC);
	}
	
#ifdef CONSOLE_SUPPORT
	if (port->is_console)
	{
		wake_up(&keypress_wait);
	}
#endif
	if (!tty)
	{
		return;
	}
	
	if(!count)
	{
		return;
	}
	
	for(i = 0; i < count; i += 2)
	{
		reordered[i/2] = buf[i];
		status = buf[i+1];
		if (status & SAB82532_RSTAT_PE) 
		{
			status = TTY_PARITY;
			port->icount.parity++;
		} 
		else if (status & SAB82532_RSTAT_FE) 
		{
			status = TTY_FRAME;
			port->icount.frame++;
		}
		else
		{
			status = TTY_NORMAL;
		}
		reordered[(count+i)/2] = status;
	}
	
	if(port->active2.receive == NULL)
	{
		return;
	}
	
	memcpy(port->active2.receive->HostVaddr->tail, reordered, count);
	port->active2.receive->HostVaddr->tail += count;
	port->active2.receive->HostVaddr->data_len = count;
	port->active2.receive->HostVaddr->len = count;
	if(skb = dev_alloc_skb(port->recv_fifo_size), skb == NULL) /* use dev_alloc_skb because at int
								      there is header space but so what*/
	{
		port->icount.buf_overrun++;
		port->active2.receive->HostVaddr->tail = port->active2.receive->HostVaddr->data; /* clear the buffer */
		port->active2.receive->Count = (port->recv_fifo_size|OWN_SAB);
		port->active2.receive->HostVaddr->data_len = 0;
		port->active2.receive->HostVaddr->len = 0;
	}
	else
	{
		skb_unlink(port->active2.receive->HostVaddr);
		skb_queue_tail(port->sab8253xc_rcvbuflist, port->active2.receive->HostVaddr);
		skb_queue_head(port->sab8253xbuflist, skb);
		port->active2.receive->HostVaddr = skb;
		port->active2.receive->Count = (port->recv_fifo_size|OWN_SAB);
	}
	queue_task(&tty->flip.tqueue, &tq_timer);
}

static void sab8253x_transmit_chars(struct sab_port *port,
				    union sab8253x_irq_status *stat)
{
	
	if (stat->sreg.isr1 & SAB82532_ISR1_ALLS) /* got an all sent int? */
	{
		port->interrupt_mask1 |= SAB82532_IMR1_ALLS;
		WRITEB(port, imr1, port->interrupt_mask1);
		port->all_sent = 1;	/* not much else to do */
	} /* a very weird chip -- this int only indicates this int */
	
	sab8253x_start_tx(port);
}

static void sab8253x_check_status(struct sab_port *port,
			   union sab8253x_irq_status *stat)
{
	struct tty_struct *tty = port->tty;
	int modem_change = 0;
	mctlsig_t         *sig;
	struct sk_buff *skb;
	
	if (!tty)
	{
		return;
	}
	
	if(port->active2.receive == NULL)
	{
		goto check_modem;
	}
	
	if (stat->images[ISR1_IDX] & SAB82532_ISR1_BRK) 
	{
#ifdef XCONFIG_SERIAL_CONSOLE
		if (port->is_console) 
		{
			batten_down_hatches(info); /* need to add this function */
			return;
		}
#endif 
		
		port->active2.receive->HostVaddr->tail[0] = 0;
		port->active2.receive->HostVaddr->tail[1] = TTY_PARITY;
		port->active2.receive->HostVaddr->tail += 2;
		port->active2.receive->HostVaddr->data_len = 2;
		port->active2.receive->HostVaddr->len = 2;
		
		if(skb = dev_alloc_skb(port->recv_fifo_size), skb == NULL)
		{
			port->icount.buf_overrun++;
			port->active2.receive->HostVaddr->tail = port->active2.receive->HostVaddr->data; 
				/* clear the buffer */
			port->active2.receive->Count = (port->recv_fifo_size|OWN_SAB);
			port->active2.receive->HostVaddr->data_len = 0;
			port->active2.receive->HostVaddr->len = 0;
		}
		else
		{
			skb_unlink(port->active2.receive->HostVaddr);
			skb_queue_tail(port->sab8253xc_rcvbuflist, port->active2.receive->HostVaddr);
			skb_queue_head(port->sab8253xbuflist, skb);
			port->active2.receive->HostVaddr = skb;
			port->active2.receive->Count = (port->recv_fifo_size|OWN_SAB);
		}
		queue_task(&tty->flip.tqueue, &tq_timer);
		port->icount.brk++;
	}
	
	if (stat->images[ISR0_IDX] & SAB82532_ISR0_RFO) 
	{
		port->active2.receive->HostVaddr->tail[0] = 0;
		port->active2.receive->HostVaddr->tail[1] = TTY_PARITY;
		port->active2.receive->HostVaddr->tail += 2;
		port->active2.receive->HostVaddr->data_len = 2;
		port->active2.receive->HostVaddr->len = 2;
		if(skb = dev_alloc_skb(port->recv_fifo_size), skb == NULL)
		{
			port->icount.buf_overrun++;
			port->active2.receive->HostVaddr->tail = port->active2.receive->HostVaddr->data; 
				/* clear the buffer */
			port->active2.receive->Count = (port->recv_fifo_size|OWN_SAB);
			port->active2.receive->HostVaddr->data_len = 0;
			port->active2.receive->HostVaddr->len = 0;
		}
		else
		{
			skb_unlink(port->active2.receive->HostVaddr);
			skb_queue_tail(port->sab8253xc_rcvbuflist, port->active2.receive->HostVaddr);
			skb_queue_head(port->sab8253xbuflist, skb);
			port->active2.receive->HostVaddr = skb;
			port->active2.receive->Count = (port->recv_fifo_size|OWN_SAB);
		}
		queue_task(&tty->flip.tqueue, &tq_timer);
		port->icount.overrun++;
	}
	
 check_modem:
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
		wake_up_interruptible(&port->delta_msr_wait); /* incase kernel proc level was waiting on modem change */
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
			
			MOD_INC_USE_COUNT;	/* in case a close is already in progress
						   don't want structures to vanish during
						   late processing of hangup */
			if (schedule_task(&port->tqueue_hangup) == 0)
			{
				MOD_DEC_USE_COUNT; /* task schedule failed */
			}
		}
	}
	
	sig = &port->cts;
	if (port->flags & FLAG8253X_CTS_FLOW) 
	{
		if (port->tty->hw_stopped) 
		{
			if (sig->val) 
			{
				
				port->tty->hw_stopped = 0;
				sab8253x_sched_event(port, SAB8253X_EVENT_WRITE_WAKEUP);
				sab8253x_start_tx(port);
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
}


/*
 * This routine is used to handle the "bottom half" processing for the
 * serial driver, known also the "software interrupt" processing.
 * This processing is done at the kernel interrupt level, after the
 * sab8253x_interrupt() has returned, BUT WITH INTERRUPTS TURNED ON.  This
 * is where time-consuming activities which can not be done in the
 * interrupt driver proper are done; the interrupt driver schedules
 * them using sab8253x_sched_event(), and they get done here.
 */
				/* The following routine is installed */
				/* in the bottom half -- just search */
				/* for the init_bh() call */
				/* The logic: sab8253x_sched_event() */
				/* enqueues the tqueue port entry on */
				/* the tq_8253x_serial task list -- */
				/* whenever the bottom half is run */
				/* sab8253x_do_softint is invoked for */
				/* every port that has invoked the bottom */
				/* half via sab8253x_sched_event(). */
				/* currently only a write wakeevent */
				/* wakeup is scheduled -- to tell the */
				/* tty driver to send more chars */
				/* down to the serial driver.*/

static void sab8253x_do_serial_bh(void)
{
	run_task_queue(&tq_8253x_serial);
}

				/* I believe the reason for the */
				/* bottom half processing below is */
				/* the length of time needed to transfer */
				/* characters to the TTY driver. */

static void sab8253x_do_softint(void *private_)
{
	struct sab_port	*port = (struct sab_port *)private_;
	struct tty_struct *tty;
	
	tty = port->tty;
	if (!tty)
	{
		return;
	}
	
	port->DoingInterrupt = 1;
	if (test_and_clear_bit(SAB8253X_EVENT_WRITE_WAKEUP, &port->event)) 
	{
		if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
		    tty->ldisc.write_wakeup)
			(tty->ldisc.write_wakeup)(tty);
		wake_up_interruptible(&tty->write_wait); /* in case tty driver waiting on write */
	}
	port->DoingInterrupt = 0;
}

/*
 * This routine is called from the scheduler tqueue when the interrupt
 * routine has signalled that a hangup has occurred.  The path of
 * hangup processing is:
 *
 * 	serial interrupt routine -> (scheduler tqueue) ->
 * 	do_serial_hangup() -> tty->hangup() -> sab8253x_hangup()
 * 
 */
/* This logic takes place at kernel */
/* process context through the scheduler*/
/* schedule_task(tqueue_hangup) */
/* takes place in the interrupt handler*/
static void sab8253x_do_serial_hangup(void *private_)
{
	struct sab_port *port = (struct sab_port *) private_;
	struct tty_struct *tty;
	
	tty = port->tty;
	if (tty)
	{
		tty_hangup(tty);
	}
	MOD_DEC_USE_COUNT;		/* in case busy waiting to unload module */
}

static void
sab8253x_init_line(struct sab_port *port)
{
	unsigned char stat;

	if(port->chip->c_cim)
	{
		if(port->chip->c_cim->ci_type == CIM_SP502)
		{
			aura_sp502_program(port, SP502_OFF_MODE);
		}
	}
	
	/*
	 * Wait for any commands or immediate characters
	 */
	sab8253x_cec_wait(port);
	sab8253x_tec_wait(port);
	
	/*
	 * Clear the FIFO buffers.
	 */
	
	WRITEB(port, cmdr, SAB82532_CMDR_RRES);
	sab8253x_cec_wait(port);
	WRITEB(port, cmdr, SAB82532_CMDR_XRES);
	
	
	/*
	 * Clear the interrupt registers.
	 */
	stat = READB(port, isr0);
	stat = READB(port, isr1);
	
	/*
	 * Now, initialize the UART 
	 */
	WRITEB(port, ccr0, 0);	  /* power-down */
	WRITEB(port, ccr0,
	       SAB82532_CCR0_MCE | SAB82532_CCR0_SC_NRZ | SAB82532_CCR0_SM_ASYNC);
	WRITEB(port, ccr1,
	       SAB82532_CCR1_ODS | SAB82532_CCR1_BCR | 7);
	WRITEB(port, ccr2,
	       SAB82532_CCR2_BDF | SAB82532_CCR2_SSEL | SAB82532_CCR2_TOE);
	WRITEB(port, ccr3, 0);
	WRITEB(port, ccr4,
	       SAB82532_CCR4_MCK4 | SAB82532_CCR4_EBRG);
	WRITEB(port, mode,
	       SAB82532_MODE_RTS | SAB82532_MODE_FCTS | SAB82532_MODE_RAC);
	WRITEB(port, rfc,
	       SAB82532_RFC_DPS | SAB82532_RFC_RFDF);
	switch (port->recv_fifo_size) 
	{
	case 1:
		SET_REG_BIT(port,rfc,SAB82532_RFC_RFTH_1);
		break;
	case 4:
		SET_REG_BIT(port,rfc,SAB82532_RFC_RFTH_4);
		break;
	case 16:
		SET_REG_BIT(port,rfc,SAB82532_RFC_RFTH_16);
		break;
	default:
		port->recv_fifo_size = 32;
	case 32:
		SET_REG_BIT(port,rfc,SAB82532_RFC_RFTH_32);
		break;
	}
	/* power-up */
	SET_REG_BIT(port, ccr0, SAB82532_CCR0_PU);
	if(port->chip->c_cim)
	{
		if(port->chip->c_cim->ci_type == CIM_SP502)
		{
			aura_sp502_program(port, port->sigmode);
		}
	}
}


static int sab8253x_startup(struct sab_port *port)
{
	unsigned long flags;
	
	int retval = 0;
	
	save_flags(flags); cli();
	
	if (port->flags & FLAG8253X_INITIALIZED) 
	{
		goto errout;
	}
	
	port->msgbufindex = 0;
	port->xmit_buf = NULL;
	port->buffergreedy = 0;
	
	if (!port->regs) 
	{
		if (port->tty)
		{
			set_bit(TTY_IO_ERROR, &port->tty->flags);
		}
		retval = -ENODEV;
		goto errout;
	}
	
	/*
	 * Initialize the Hardware
	 */
	sab8253x_init_line(port);
	
	if (port->tty->termios->c_cflag & CBAUD) 
	{
		/* Activate RTS */
		RAISE(port,rts);
		
		/* Activate DTR */
		RAISE(port,dtr);
	}
	
	/*
	 * Initialize the modem signals values
	 */
	port->dcd.val=ISON(port,dcd);
	port->cts.val=ISON(port,cts);
	port->dsr.val=ISON(port,dsr);
	
	/*
	 * Finally, enable interrupts
	 */
	
	port->interrupt_mask0 = SAB82532_IMR0_PERR | SAB82532_IMR0_FERR |
		SAB82532_IMR0_PLLA;
	WRITEB(port, imr0, port->interrupt_mask0);
	port->interrupt_mask1 = SAB82532_IMR1_BRKT | SAB82532_IMR1_XOFF |
		SAB82532_IMR1_TIN | SAB82532_IMR1_XON |
		SAB82532_IMR1_XPR;
	WRITEB(port, imr1, port->interrupt_mask1);
	port->all_sent = 1;
	
	if (port->tty)
	{
		clear_bit(TTY_IO_ERROR, &port->tty->flags);
	}
	port->xmit_cnt = port->xmit_head = port->xmit_tail = 0;
	
	/*
	 * and set the speed of the serial port
	 */
	sab8253x_change_speed(port);
	
	port->flags |= FLAG8253X_INITIALIZED;
	port->receive_chars = sab8253x_receive_chars;
	port->transmit_chars = sab8253x_transmit_chars;
	port->check_status = sab8253x_check_status;
	port->receive_test = (SAB82532_ISR0_TCD | SAB82532_ISR0_TIME |
			      SAB82532_ISR0_RFO | SAB82532_ISR0_RPF);
	port->transmit_test = (SAB82532_ISR1_ALLS | SAB82532_ISR1_XPR);
	port->check_status_test = SAB82532_ISR1_BRK;
	
	restore_flags(flags);
	return 0;
	
 errout:
	restore_flags(flags);
	return retval;
}


/*
 * This routine will shutdown a serial port; interrupts are disabled, and
 * DTR is dropped if the hangup on close termio flag is on.
 */
static void sab8253x_shutdown(struct sab_port *port)
{
	unsigned long flags;
	
	if (!(port->flags & FLAG8253X_INITIALIZED))
	{
		return;
	}
	
	save_flags(flags); 
	cli(); /* Disable interrupts */
	
	/*
	 * clear delta_msr_wait queue to avoid mem leaks: we may free the irq
	 * here so the queue might never be waken up
	 */
	wake_up_interruptible(&port->delta_msr_wait);	/* shutting down port modem status is pointless */
	
	if (port->xmit_buf) 
	{
		port->xmit_buf = NULL;
	}
	
#ifdef XCONFIG_SERIAL_CONSOLE
	if (port->is_console) 
	{
		port->interrupt_mask0 = 
			SAB82532_IMR0_PERR | SAB82532_IMR0_FERR |
			/*SAB82532_IMR0_TIME |*/
			SAB82532_IMR0_PLLA | SAB82532_IMR0_CDSC;
		WRITEB(port,imr0,port->interrupt_mask0);
		port->interrupt_mask1 = 
			SAB82532_IMR1_BRKT | SAB82532_IMR1_ALLS |
			SAB82532_IMR1_XOFF | SAB82532_IMR1_TIN |
			SAB82532_IMR1_CSC | SAB82532_IMR1_XON |
			SAB82532_IMR1_XPR;
		WRITEB(port,imr1,port->interrupt_mask1);
		if (port->tty)
		{
			set_bit(TTY_IO_ERROR, &port->tty->flags);
		}
		port->flags &= ~FLAG8253X_INITIALIZED;
		restore_flags(flags);
		return;
	}
#endif
	
	/* Disable Interrupts */
	
	port->interrupt_mask0 = 0xff;
	WRITEB(port, imr0, port->interrupt_mask0);
	port->interrupt_mask1 = 0xff;
	WRITEB(port, imr1, port->interrupt_mask1);
	
	if (!port->tty || (port->tty->termios->c_cflag & HUPCL)) 
	{
		LOWER(port,rts);
		LOWER(port,dtr);
	}
	
	/* Disable break condition */
	CLEAR_REG_BIT(port,dafo,SAB82532_DAFO_XBRK);
	
	/* Disable Receiver */	
	CLEAR_REG_BIT(port,mode,SAB82532_MODE_RAC);
	
	/* Power Down */	
	CLEAR_REG_BIT(port,ccr0,SAB82532_CCR0_PU);
	
	if (port->tty)
	{
		set_bit(TTY_IO_ERROR, &port->tty->flags);
	}
	
	port->flags &= ~FLAG8253X_INITIALIZED;
	restore_flags(flags);
}

/*
 * This routine is called to set the UART divisor registers to match
 * the specified baud rate for a serial port.
 */
static void sab8253x_change_speed(struct sab_port *port)
{
	unsigned long	flags,baud;
	tcflag_t	cflag;
	u8	        dafo,ccr2=0,ccr4=0,ebrg=0,mode;
	int		i, bits;
#ifdef DEBUGGING
	printk("Change speed!  ");
#endif
	if (!port->tty || !port->tty->termios) 
	{
#ifdef DEBUGGING
		printk("NOT!\n");
#endif
		return;
	}
	
#ifdef DEBUGGING
	printk(" for real.\n");
#endif
	
	cflag = port->tty->termios->c_cflag;
	
	/* Byte size and parity */
	switch (cflag & CSIZE) 
	{
	case CS5: 
		dafo = SAB82532_DAFO_CHL5; 
		bits = 7; 
		break;
	case CS6: 
		dafo = SAB82532_DAFO_CHL6; 
		bits = 8; 
		break;
	case CS7: 
		dafo = SAB82532_DAFO_CHL7; 
		bits = 9; 
		break;
	default:
	case CS8: 
		dafo = SAB82532_DAFO_CHL8; 
		bits = 10; 
		break;
	}
	
	if (cflag & CSTOPB) 
	{
		dafo |= SAB82532_DAFO_STOP;
		bits++;
	}
	
	if (cflag & PARENB) 
	{
		dafo |= SAB82532_DAFO_PARE;
		bits++;
	}
	
	if (cflag & PARODD) 
	{
#ifdef CMSPAR
		if (cflag & CMSPAR)
			dafo |= SAB82532_DAFO_PAR_MARK;
		else
#endif
			dafo |= SAB82532_DAFO_PAR_ODD;
	} 
	else 
	{
#ifdef CMSPAR
		if (cflag & CMSPAR)
			dafo |= SAB82532_DAFO_PAR_SPACE;
		else
#endif
			dafo |= SAB82532_DAFO_PAR_EVEN;
	}
	
	/* Determine EBRG values based on the "encoded"baud rate */
	i = cflag & CBAUD;
	switch(i)
	{
	case B0:
		baud=0;
		break;
	case  B50:
		baud=100;
		break;
	case  B75:
		baud=150;
		break;
	case  B110:
		baud=220;
		break;
	case  B134:
		baud=269;
		break;
	case  B150:
		baud=300;
		break;
	case  B200:
		baud=400;
		break;
	case B300:
		baud=600;
		break;
	case B600:
		baud=1200;
		break;
	case B1200:
		baud=2400;
		break;
	case B1800:
		baud=3600;
		break;
	case B2400:
		baud=4800;
		break;
	case B4800:
		baud=9600;
		break;
	case B9600:
		baud=19200;
		break;
	case B19200:
		baud=38400;
		break;
	case  B38400:
		if(port->custspeed)
		{
			baud=port->custspeed<<1;
		}
		else
		{
			baud=76800;
		}
		break;
	case B57600:
		baud=115200;
		break;
#ifdef SKIPTHIS
	case B76800:
		baud=153600;
		break;
	case B153600:
		baud=307200;
		break;
#endif
	case B230400:
		baud=460800;
		break;
	case  B460800:
		baud=921600;
		break;
	case B115200:
	default:
		baud=230400;
		break;
	}
	
	if(!sab8253x_baud(port,baud,&ebrg,&ccr2,&ccr4,&(port->baud))) 
	{
		printk("Aurora Warning. baudrate %ld could not be set! Using 115200",baud);
		baud=230400;
		sab8253x_baud(port,baud,&ebrg,&ccr2,&ccr4,&(port->baud));
	}
	
	if (port->baud)
		port->timeout = (port->xmit_fifo_size * HZ * bits) / port->baud;
	else
		port->timeout = 0;
	port->timeout += HZ / 50;		/* Add .02 seconds of slop */
	
	/* CTS flow control flags */
	if (cflag & CRTSCTS)
		port->flags |= FLAG8253X_CTS_FLOW;
	else
		port->flags &= ~(FLAG8253X_CTS_FLOW);
	
	if (cflag & CLOCAL)
		port->flags &= ~(FLAG8253X_CHECK_CD);
	else
		port->flags |= FLAG8253X_CHECK_CD;
	if (port->tty)
		port->tty->hw_stopped = 0;
	
	/*
	 * Set up parity check flag
	 * XXX: not implemented, yet.
	 */
#define RELEVANT_IFLAG(iflag) (iflag & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))
	
	/*
	 * Characters to ignore
	 * XXX: not implemented, yet.
	 */
	
	/*
	 * !!! ignore all characters if CREAD is not set
	 * XXX: not implemented, yet.
	 */
	if ((cflag & CREAD) == 0)
		port->ignore_status_mask |= SAB82532_ISR0_RPF |
			/* SAB82532_ISR0_TIME |*/
			SAB82532_ISR0_TCD ;
	
	save_flags(flags); 
	cli();
	sab8253x_cec_wait(port);
	sab8253x_tec_wait(port);
	WRITEB(port,dafo,dafo);
	WRITEB(port,bgr,ebrg);
	ccr2 |= READB(port,ccr2) & ~(0xc0);
	WRITEB(port,ccr2,ccr2);
	ccr4 |= READB(port,ccr4) & ~(SAB82532_CCR4_EBRG);
	WRITEB(port,ccr4,ccr4);
	
	if (port->flags & FLAG8253X_CTS_FLOW) 
	{
		mode = READB(port,mode) & ~(SAB82532_MODE_RTS);
		mode |= SAB82532_MODE_FRTS;
		mode  &= ~(SAB82532_MODE_FCTS);
	} 
	else 
	{
		mode = READB(port,mode) & ~(SAB82532_MODE_FRTS);
		mode |= SAB82532_MODE_RTS;
		mode |= SAB82532_MODE_FCTS;
	}
	WRITEB(port,mode,mode);
	mode |= SAB82532_MODE_RAC;
	WRITEB(port,mode,mode);
	restore_flags(flags);
}

static void sab8253x_flush_chars(struct tty_struct *tty)
{
	struct sab_port *port = (struct sab_port *)tty->driver_data;
	
	if (sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_flush_chars"))
	{
		return;
	}
	
	if ((Sab8253xCountTransmit(port) <= 0) || tty->stopped || tty->hw_stopped)
	{
		return;
	}
	
	sab8253x_start_tx(port);
}

static int sab8253x_write(struct tty_struct * tty, int from_user,
		   const unsigned char *buf, int count)
{
	struct sab_port *port = (struct sab_port *)tty->driver_data;
	struct sk_buff *skb = NULL;
	int truelength = 0;
	int do_queue = 1;
	
	if (sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_write"))
	{
		return 0;
	}
	
	if(count == 0)
	{
		return 0;
	}
	
	if(port->active2.transmit == NULL)
	{
		return 0;
	}
	
	if((port->active2.transmit->Count & OWNER) == OWN_SAB)
	{
		sab8253x_start_tx(port);
		return 0;
	}
	
#ifndef FREEININTERRUPT
	skb = port->active2.transmit->HostVaddr; /* current slot value */
	
	if(port->buffergreedy == 0)	/* are we avoiding buffer free's */
	{				/* no */
		if((skb != NULL) || /* not OWN_SAB from above */
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
		skb = NULL;		/* buffer was freed */
	}
	
	if(skb != NULL)		/* potentially useful */
	{
		truelength = (skb->end - skb->head);
		if(truelength >= count)
		{
			skb->data = skb->head; /* this buffer is already queued */
			skb->tail = skb->head;
			do_queue = 0;
		}
		else
		{
			skb_unlink(skb);
			dev_kfree_skb_any(skb);
			skb = NULL;
			port->active2.transmit->HostVaddr = NULL;
		}
	}
	/* in all cases the following is allowed */
	port->active2.transmit->sendcrc = 0;
	port->active2.transmit->crcindex = 0;
#endif
	
	if(skb == NULL)
	{
		if(port->DoingInterrupt)
		{
			skb = alloc_skb(count, GFP_ATOMIC);
		}
		else
		{
			skb = alloc_skb(count, GFP_KERNEL);
		}
	}
	
	if(skb == NULL)
	{
		printk(KERN_ALERT "sab8253xt: no skbuffs available.\n");
		return 0;
	}
	if(from_user)
	{
		copy_from_user(skb->data, buf, count);
	}
	else
	{
		memcpy(skb->data, buf, count);
	}
	skb->tail = (skb->data + count);
	skb->data_len = count;
	skb->len = count;
	
	if(do_queue)
	{
		skb_queue_head(port->sab8253xbuflist, skb);
	}
	
	port->active2.transmit->HostVaddr = skb;
	port->active2.transmit->sendcrc = 0;
	port->active2.transmit->crcindex = 0;
	port->active2.transmit->Count = (OWN_SAB|count);
	port->active2.transmit = port->active2.transmit->VNext;
	
	sab8253x_start_tx(port);
	return count;
}

static int sab8253x_write_room(struct tty_struct *tty)
{
	struct sab_port *port = (struct sab_port *)tty->driver_data;
	
	if(sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_write_room"))
	{
		return 0;
	}
	
	if(port->active2.transmit == NULL)
	{
		return 0;
	}
	
	if((port->active2.transmit->Count & OWNER) == OWN_SAB)
	{
		return 0;
	}
	return ((sab8253xt_rbufsize) * /* really should not send buffs bigger than 32 I guess */
		(sab8253xt_listsize - 
		 Sab8253xCountTransmitDescriptors(port)));
}

static int sab8253x_chars_in_buffer(struct tty_struct *tty)
{
	struct sab_port *port = (struct sab_port *)tty->driver_data;
	
	if (sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_chars_in_bufferS"))
	{
		return 0;
	}
	
	return Sab8253xCountTransmit(port);
}

/*
 * This function is used to send a high-priority XON/XOFF character to
 * the device
 */
static void sab8253x_send_xchar(struct tty_struct *tty, char ch)
{
	struct sab_port *port = (struct sab_port *)tty->driver_data;
	unsigned long flags;
	
	if (sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_send_xchar"))
	{
		return;
	}
	
	save_flags(flags); 
	cli();
	sab8253x_tec_wait(port);
	WRITEB(port, tic, ch);
	restore_flags(flags);
}

/*
 * ------------------------------------------------------------
 * sab8253x_throttle()
 * 
 * This routine is called by the upper-layer tty layer to signal that
 * incoming characters should be throttled.
 * ------------------------------------------------------------
 */
static void sab8253x_throttle(struct tty_struct * tty)
{
	struct sab_port *port = (struct sab_port *)tty->driver_data;
	
	if (sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_throttle"))
	{
		return;
	}
	
	if (I_IXOFF(tty))
	{
		sab8253x_send_xchar(tty, STOP_CHAR(tty));
	}
}

static void sab8253x_unthrottle(struct tty_struct * tty)
{
	struct sab_port *port = (struct sab_port *)tty->driver_data;
	
	if (sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_unthrottle"))
	{
		return;
	}
	
	if (I_IXOFF(tty)) 
	{
		if (port->x_char)
		{
			port->x_char = 0;
		}
		else
		{
			sab8253x_send_xchar(tty, START_CHAR(tty));
		}
	}
}

/*
 * ------------------------------------------------------------
 * sab8253x_ioctl() and friends
 * ------------------------------------------------------------
 */

static int sab8253x_get_serial_info(struct sab_port *port,
			     struct serial_struct *retinfo)
{
	struct serial_struct tmp;
	
	if (!retinfo)
	{
		return -EFAULT;
	}
	memset(&tmp, 0, sizeof(tmp));
	tmp.type = port->type;
	tmp.line = port->line;
	tmp.port = (unsigned long)port->regs;
	tmp.irq = port->irq;
	tmp.flags = port->flags;
	tmp.xmit_fifo_size = port->xmit_fifo_size;
	tmp.baud_base = 0;
	tmp.close_delay = port->close_delay;
	tmp.closing_wait = port->closing_wait;
	tmp.custom_divisor = port->custom_divisor;
	tmp.hub6 = 0;
	if (copy_to_user(retinfo, &tmp, sizeof(*retinfo)))
	{
		return -EFAULT;
	}
	return 0;
}

static int sab8253x_set_serial_info(struct sab_port *port,
			     struct serial_struct *new_info)
{
	return 0;
}


/*
 * get_lsr_info - get line status register info
 *
 * Purpose: Let user call ioctl() to get info when the UART physically
 * 	    is emptied.  On bus types like RS485, the transmitter must
 * 	    release the bus after transmitting. This must be done when
 * 	    the transmit shift register is empty, not be done when the
 * 	    transmit holding register is empty.  This functionality
 * 	    allows an RS485 driver to be written in user space. 
 */
static int sab8253x_get_lsr_info(struct sab_port * port, unsigned int *value)
{
	unsigned int result;
	
	result = (((Sab8253xCountTransmit(port) <= 0) && port->all_sent) ? TIOCSER_TEMT : 0);
	return put_user(result, value);
}


static int sab8253x_get_modem_info(struct sab_port * port, unsigned int *value)
{
	unsigned int result;
	
	/* Using the cached values !! After all when changed int occurs
	   and the cache is updated */
	result=  
		((port->dtr.val) ? TIOCM_DTR : 0)
		| ((port->rts.val) ? TIOCM_RTS : 0)
		| ((port->cts.val) ? TIOCM_CTS : 0)
		| ((port->dsr.val) ? TIOCM_DSR : 0)
		| ((port->dcd.val) ? TIOCM_CAR : 0);
	
	return put_user(result,value);
}

static int sab8253x_set_modem_info(struct sab_port * port, unsigned int cmd,
				   unsigned int *value)
{
	int error;
	unsigned int arg;
	unsigned long flags;
	
	error = get_user(arg, value);
	if (error)
	{
		return error;
	}
	
	save_flags(flags);
	cli();
	switch (cmd) 
	{
	case TIOCMBIS: 
		if (arg & TIOCM_RTS) 
		{
			RAISE(port, rts);
		}
		if (arg & TIOCM_DTR) 
		{
			RAISE(port, dtr);
		}
		break;
	case TIOCMBIC:
		if (arg & TIOCM_RTS) 
		{
			LOWER(port,rts);
		}
		if (arg & TIOCM_DTR) 
		{
			LOWER(port,dtr);
		}
		break;
	case TIOCMSET:
		if (arg & TIOCM_RTS) 
		{
			RAISE(port, rts);
		} 
		else 
		{
			LOWER(port,rts);
		}
		if (arg & TIOCM_DTR) 
		{
			RAISE(port, dtr);
		} 
		else 
		{
			LOWER(port,dtr);
		}
		break;
	default:
		restore_flags(flags);
		return -EINVAL;
	}
	restore_flags(flags);
	return 0;
}

/*
 * This routine sends a break character out the serial port.
 */
static void sab8253x_break(struct tty_struct *tty, int break_state)
{
	struct sab_port *port = (struct sab_port *)tty->driver_data;
	unsigned long flags;
	
	if (sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_break"))
	{
		return;
	}
	
	if (!port->regs)
	{
		return;
	}
	
	save_flags(flags); 
	cli();
	if (break_state == -1) 
	{
		SET_REG_BIT(port,dafo,SAB82532_DAFO_XBRK);
	} 
	else 
	{
		CLEAR_REG_BIT(port,dafo,SAB82532_DAFO_XBRK);
	}
	restore_flags(flags);
}

static int sab8253x_ioctl(struct tty_struct *tty, struct file * file,
			  unsigned int cmd, unsigned long arg)
{
	int error;
	unsigned int wordindex;
	unsigned short *wordptr;
	struct sab_port *port = (struct sab_port *)tty->driver_data;
	struct async_icount cprev, cnow;	/* kernel counter temps */
	struct serial_icounter_struct *p_cuser;	/* user space */
	SAB_BOARD *bptr;
	unsigned long flags;
	
	if (sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_ioctl"))
	{
		return -ENODEV;
	}
	
	if ((cmd != TIOCGSERIAL) && (cmd != TIOCSSERIAL) &&
	    (cmd != TIOCSERCONFIG) && (cmd != TIOCSERGWILD)  &&
	    (cmd != TIOCSERSWILD) && (cmd != TIOCSERGSTRUCT) &&
	    (cmd != TIOCMIWAIT) && (cmd != TIOCGICOUNT)) 
	{
		if (tty->flags & (1 << TTY_IO_ERROR))
		{
			return -EIO;
		}
	}
	
	switch (cmd) 
	{
	case ATIS_IOCSPARAMS:
		copy_from_user(&port->ccontrol, (struct channelcontrol*)arg , sizeof(struct channelcontrol));
		break;
	case ATIS_IOCGPARAMS:
		copy_to_user((struct channelcontrol*) arg, &port->ccontrol, sizeof(struct channelcontrol));
		break;
		
	case ATIS_IOCSSPEED:
		copy_from_user(&port->custspeed, (unsigned long*)arg , sizeof(unsigned long));
		break;
	case ATIS_IOCGSPEED:
		copy_to_user((unsigned long*) arg, &port->custspeed, sizeof(unsigned long));
		break;
		
	case ATIS_IOCSSEP9050:
		bptr = port->board;
		if(bptr->b_type == BD_WANMCS)
		{
			return -EINVAL;
		}
		copy_from_user((unsigned char*) bptr->b_eprom, (unsigned char*) arg , sizeof(struct sep9050));
		
		wordptr = (unsigned short*) bptr->b_eprom;
		plx9050_eprom_cmd(&((PLX9050*)(bptr->virtbaseaddress0))->ctrl, 
				  NM93_WENCMD, NM93_WENADDR, 0);
		for(wordindex = 0; wordindex < EPROM9050_SIZE; ++wordindex)
		{
			plx9050_eprom_cmd(&((PLX9050*)(bptr->virtbaseaddress0))->ctrl, 
					  NM93_WRITECMD, 
					  wordindex, wordptr[wordindex]);
		}
		plx9050_eprom_cmd(&((PLX9050*)(bptr->virtbaseaddress0))->ctrl, 
				  NM93_WDSCMD, NM93_WDSADDR, 0);
		break;
	case ATIS_IOCGSEP9050:
		bptr = port->board;
		if(bptr->b_type == BD_WANMCS)
		{
			return -EINVAL;
		}
		if (!plx9050_eprom_read(&((PLX9050*)(bptr->virtbaseaddress0))->ctrl, 
					(unsigned short*) bptr->b_eprom,
					(unsigned char) 0, EPROM9050_SIZE))
		{
			printk(KERN_ALERT "auraXX20n: Could not read serial eprom.\n");
			return -EIO;
		}
		copy_to_user((unsigned char*) arg, (unsigned char*) bptr->b_eprom, sizeof(struct sep9050));
		break;
		
	case TIOCGSOFTCAR:
		return put_user(C_CLOCAL(tty) ? 1 : 0, (int *) arg);
		
	case TIOCSSOFTCAR:
		error = get_user(arg, (unsigned int *) arg);
		if (error)
		{
			return error;
		}
		tty->termios->c_cflag =
			((tty->termios->c_cflag & ~CLOCAL) |
			 (arg ? CLOCAL : 0));
		return 0;
	case TIOCMGET:
		return sab8253x_get_modem_info(port, (unsigned int *) arg);
	case TIOCMBIS:
	case TIOCMBIC:
	case TIOCMSET:
		return sab8253x_set_modem_info(port, cmd, (unsigned int *) arg);
	case TIOCGSERIAL:
		return sab8253x_get_serial_info(port,
						(struct serial_struct *) arg);
	case TIOCSSERIAL:
		return sab8253x_set_serial_info(port,
						(struct serial_struct *) arg);
		
	case TIOCSERGETLSR: /* Get line status register */
		return sab8253x_get_lsr_info(port, (unsigned int *) arg);
		
	case TIOCSERGSTRUCT:
		if (copy_to_user((struct sab_port *) arg,
				 port, sizeof(struct sab_port)))
			return -EFAULT;
		return 0;
		
		/*
		 * Wait for any of the 4 modem inputs (DCD,RI,DSR,CTS) to change
		 * - mask passed in arg for lines of interest
		 *   (use |'ed TIOCM_RNG/DSR/CD/CTS for masking)
		 * Caller should use TIOCGICOUNT to see which one it was
		 */
	case TIOCMIWAIT:
		save_flags(flags);
		cli();
		/* note the counters on entry */
		cprev = port->icount;
		restore_flags(flags);
		while (1) 
		{
			interruptible_sleep_on(&port->delta_msr_wait); /* waits for a modem signal change */
			/* see if a signal did it */
			if (signal_pending(current))
			{
				return -ERESTARTSYS;
			}
			save_flags(flags);
			cli();
			cnow = port->icount; /* atomic copy */
			restore_flags(flags);
			if (cnow.rng == cprev.rng && cnow.dsr == cprev.dsr && 
			    cnow.dcd == cprev.dcd && cnow.cts == cprev.cts)
			{
				return -EIO; /* no change => error */
			}
			if ( ((arg & TIOCM_RNG) && (cnow.rng != cprev.rng)) ||
			     ((arg & TIOCM_DSR) && (cnow.dsr != cprev.dsr)) ||
			     ((arg & TIOCM_CD)  && (cnow.dcd != cprev.dcd)) ||
			     ((arg & TIOCM_CTS) && (cnow.cts != cprev.cts)) ) {
				{
					return 0;
				}
			}
			cprev = cnow;
		}
		/* NOTREACHED */
		break;
		
		/* 
		 * Get counter of input serial line interrupts (DCD,RI,DSR,CTS)
		 * Return: write counters to the user passed counter struct
		 * NB: both 1->0 and 0->1 transitions are counted except for
		 *     RI where only 0->1 is counted.
		 */
	case TIOCGICOUNT:
		save_flags(flags);
		cli();
		cnow = port->icount;
		restore_flags(flags);
		p_cuser = (struct serial_icounter_struct *) arg;
		error = put_user(cnow.cts, &p_cuser->cts);
		if (error) 
		{
			return error;
		}
		error = put_user(cnow.dsr, &p_cuser->dsr);
		if (error) 
		{
			return error;
		}
		error = put_user(cnow.rng, &p_cuser->rng);
		if (error) 
		{
			return error;
		}
		error = put_user(cnow.dcd, &p_cuser->dcd);
		if (error) 
		{
			return error;
		}
		return 0;
		
	case ATIS_IOCSSIGMODE:
		if(port->chip->c_cim)
		{
			if(port->chip->c_cim->ci_type == CIM_SP502)
			{
				copy_from_user(&port->sigmode, (unsigned int*)arg , sizeof(unsigned int));
				return 0;
			}
		}
		return -EINVAL;

	case ATIS_IOCGSIGMODE:
		if(port->chip->c_cim)
		{
			if(port->chip->c_cim->ci_type == CIM_SP502)
			{
				copy_to_user((unsigned int*) arg, &port->sigmode, sizeof(unsigned int));
				return 0;
			}
		}
		return -EINVAL;

	default:
		return -ENOIOCTLCMD;
	}
	return 0;
}

static void sab8253x_set_termios(struct tty_struct *tty,
				 struct termios *old_termios)
{
	struct sab_port *port = (struct sab_port *)tty->driver_data;
	
	if((tty->termios->c_cflag == old_termios->c_cflag) && 
	   (RELEVANT_IFLAG(tty->termios->c_iflag) == RELEVANT_IFLAG(old_termios->c_iflag)))
	{
		return;
	}
	if(!port)
	{
		return;
	}
	sab8253x_change_speed(port);
	
	/* Handle transition to B0 status */
	if ((old_termios->c_cflag & CBAUD) &&
	    !(tty->termios->c_cflag & CBAUD)) 
	{
		LOWER(port,rts);
		LOWER(port,dtr);
	}
	
	/* Handle transition away from B0 status */
	if (!(old_termios->c_cflag & CBAUD) &&
	    (tty->termios->c_cflag & CBAUD)) 
	{
		RAISE(port, dtr);
		if (!tty->hw_stopped ||
		    !(tty->termios->c_cflag & CRTSCTS)) 
		{
			RAISE(port, rts);
		}
	}
	
	/* Handle turning off CRTSCTS */
	if ((old_termios->c_cflag & CRTSCTS) &&
	    !(tty->termios->c_cflag & CRTSCTS)) 
	{
		tty->hw_stopped = 0;
		sab8253x_start(tty);
	}
}

/*
 * ------------------------------------------------------------
 * sab8253x_close()
 * 
 * This routine is called when the serial port gets closed.  First, we
 * wait for the last remaining data to be sent.  Then, we unlink its
 * async structure from the interrupt chain if necessary, and we free
 * that IRQ if nothing is left in the chain.
 * ------------------------------------------------------------
 */
static void sab8253x_close(struct tty_struct *tty, struct file * filp)
{
	struct sab_port *port = (struct sab_port *)tty->driver_data;
	unsigned long flags;
	
	MOD_DEC_USE_COUNT;	
	if (sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_close"))
	{
		return;
	}
	if(port->open_type == OPEN_SYNC_NET)
	{				/* port->tty field should already be NULL */
		/* port count was not incremented */
		return;
	}
	
	--(port->count);		/* have a valid port */
	if (tty_hung_up_p(filp)) 
	{
		
		if(port->count == 0)	/* shutdown took place in hangup context */
		{
			port->open_type = OPEN_NOT;
		}
		else if(port->count < 0)
		{
			printk(KERN_ALERT "XX20: port->count went negative.\n");
			port->count = 0;
			port->open_type = OPEN_NOT;
		}
		return;
	}
	
	if (port->count < 0) 
	{
		printk(KERN_ALERT "sab8253x_close: bad serial port count for ttys%d: %d\n",
		       port->line, port->count);
		port->count = 0;
	}
	
	if (port->count) 
	{
		return;
	}
	
	port->flags |= FLAG8253X_CLOSING;
	
	/*
	 * Save the termios structure, since this port may have
	 * separate termios for callout and dialin.
	 */
	if (port->flags & FLAG8253X_NORMAL_ACTIVE)
	{
		port->normal_termios = *tty->termios;
	}
	if (port->flags & FLAG8253X_CALLOUT_ACTIVE)
	{
		port->callout_termios = *tty->termios;
	}
	/*
	 * Now we wait for the transmit buffer to clear; and we notify 
	 * the line discipline to only process XON/XOFF characters.
	 */
	tty->closing = 1;
	if (port->closing_wait != SAB8253X_CLOSING_WAIT_NONE) 
	{
		tty_wait_until_sent(tty, port->closing_wait); /* wait for drain */
	}
	
	/*
	 * At this point we stop accepting input.  To do this, we
	 * disable the receive line status interrupts, and turn off
	 * the receiver.
	 */
	
	save_flags(flags); 
	cli();
	port->interrupt_mask0 |= SAB82532_IMR0_TCD;
	WRITEB(port,imr0,port->interrupt_mask0);
	
	CLEAR_REG_BIT(port,mode,SAB82532_MODE_RAC); /* ??????? */
	restore_flags(flags);
	
	if (port->flags & FLAG8253X_INITIALIZED) 
	{
		/*
		 * Before we drop DTR, make sure the UART transmitter
		 * has completely drained; this is especially
		 * important if there is a transmit FIFO!
		 */
		sab8253x_wait_until_sent(tty, port->timeout);
	}
	sab8253x_shutdown(port);	/* no more ints on port */
	Sab8253xCleanUpTransceiveN(port); /* should be okay */
	if (tty->driver.flush_buffer)
	{
		tty->driver.flush_buffer(tty);
	}
	if (tty->ldisc.flush_buffer)
	{
		tty->ldisc.flush_buffer(tty);
	}
	tty->closing = 0;
	port->event = 0;
	port->tty = 0;
	if (port->blocked_open) 
	{
		if (port->close_delay) 
		{
			current->state = TASK_INTERRUPTIBLE;
			schedule_timeout(port->close_delay);
		}
		wake_up_interruptible(&port->open_wait); /* deal with open blocks */
	}
	
	if((port->flags & (FLAG8253X_CALLOUT_ACTIVE | FLAG8253X_NETWORK)) ==
	   (FLAG8253X_CALLOUT_ACTIVE | FLAG8253X_NETWORK) &&
	   port->dev)
	{
		port->flags &= ~(FLAG8253X_NORMAL_ACTIVE|FLAG8253X_CALLOUT_ACTIVE|
				 FLAG8253X_CLOSING); /* leave network set */
		netif_carrier_off(port->dev);
		port->open_type = OPEN_SYNC_NET;
		sab8253x_startupN(port);
	}
	else
	{
		port->flags &= ~(FLAG8253X_NORMAL_ACTIVE|FLAG8253X_CALLOUT_ACTIVE|
				 FLAG8253X_CLOSING);
		wake_up_interruptible(&port->close_wait);
		port->open_type = OPEN_NOT;
	}
}

/*
 * sab8253x_hangup() --- called by tty_hangup() when a hangup is signaled.
 */
static void sab8253x_hangup(struct tty_struct *tty)
{
	struct sab_port *port = (struct sab_port *)tty->driver_data;
	
	if (sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_hangup"))
	{
		return;
	}
	
#ifdef XCONFIG_SERIAL_CONSOLE
	if (port->is_console)
	{
		return;
	}
#endif
	
	sab8253x_flush_buffer(tty);
	if(port)
	{
		sab8253x_shutdown(port);
		Sab8253xCleanUpTransceiveN(port); /* this logic is a bit contorted
						     Are we cleaning up the lists
						     because we are waking up a
						     blocked open?  There is possibly
						     an order problem here perhaps the
						     open count should have increased in the
						     int handler so that it could decrease here*/
		port->event = 0;
		port->flags &= ~(FLAG8253X_NORMAL_ACTIVE|FLAG8253X_CALLOUT_ACTIVE);
		port->tty = 0;
		wake_up_interruptible(&port->open_wait); /* deal with blocking open */
	}
}
/*
 * ------------------------------------------------------------
 * sab8253x_open() and friends
 * ------------------------------------------------------------
 */

/*
 * This routine is called whenever a serial port is opened.  It
 * enables interrupts for a serial port, linking in its async structure into
 * the IRQ chain.   It also performs the serial-specific
 * initialization for the tty structure.
 */

static int sab8253x_open(struct tty_struct *tty, struct file * filp)
{
	struct sab_port	*port;
	int retval, line;
	int counter;
	unsigned long flags;
	
	MOD_INC_USE_COUNT;
	line = MINOR(tty->device) - tty->driver.minor_start;
	
	for(counter = 0, port = AuraPortRoot; 
	    (counter < line) && (port != NULL); 
	    ++counter)
	{
		port = port->next;
	}
	
	if (!port) 
	{
		printk(KERN_ALERT "sab8253x_open: can't find structure for line %d\n",
		       line);
		return -ENODEV;
	}
	
	save_flags(flags);		/* Need to protect the port->tty field */
	cli();
	
	if(port->tty == NULL)
	{
		port->tty = tty;		/* may be a standard tty waiting on a call out device */
		tty->flip.tqueue.routine = sab8253x_flush_to_ldisc;
	}
	
	tty->driver_data = port;	/* but the tty devices are unique for each type of open */
	
	if(port->function == FUNCTION_NA)
	{				/* port 2 on 1020s and 1520s */
		++(port->count);
		restore_flags(flags);
		return -ENODEV;
	}
	
	/* Check whether or not the port is open in SYNC mode */
	if(port->open_type == OPEN_SYNC_NET)
	{
		if(port->dev && netif_carrier_ok(port->dev))
		{
			port->tty= NULL;	/* Don't bother with open counting here
						   but make sure the tty field is NULL*/
			restore_flags(flags);
			return -EBUSY;
		}
		sab8253x_flush_buffer(tty); /* don't restore flags here */
		sab8253x_shutdownN(port);
	}
	else if (port->open_type > OPEN_ASYNC) /* can't have a callout or async line
						* if already open in some sync mode */
	{
		++(port->count);
		restore_flags(flags);
		return -EBUSY;
	}
	restore_flags(flags);
	
	if (sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_open"))
	{
		++(port->count);
		return -ENODEV;
	}
	
#ifdef DEBUG_OPEN
	printk("sab8253x_open %s%d, count = %d\n", tty->driver.name, port->line,
	       port->count);
#endif
	
	/*
	 * If the port is in the middle of closing, bail out now.
	 */
	if (tty_hung_up_p(filp) ||
	    (port->flags & FLAG8253X_CLOSING)) 
	{
		
		if (port->flags & FLAG8253X_CLOSING)
		{
			interruptible_sleep_on(&port->close_wait);
		}
#ifdef SERIAL_DO_RESTART
		++(port->count);
		return ((port->flags & FLAG8253X_HUP_NOTIFY) ?
			-EAGAIN : -ERESTARTSYS);
#else
		++(port->count);
		return -EAGAIN;
#endif
	}
	
	if(Sab8253xSetUpLists(port))
	{
		++(port->count);
		return -ENODEV;
	}
	if(Sab8253xInitDescriptors2(port, sab8253xt_listsize, sab8253xt_rbufsize))
	{
		++(port->count);
		return -ENODEV;
	}
	
	retval = sab8253x_startup(port);
	if (retval)
	{
		++(port->count);
		return retval;
	}
	
	retval = sab8253x_block_til_ready(tty, filp, port);
	++(port->count);  
	if (retval) 
	{
		return retval;
	}
	
	port->tty = tty;		/* may change here once through the block */
	/* because now the port belongs to an new tty */
	tty->flip.tqueue.routine = sab8253x_flush_to_ldisc; /* in case it was changed */
	
	if(Sab8253xSetUpLists(port))
	{
		return -ENODEV;
	}
	if(Sab8253xInitDescriptors2(port, sab8253xt_listsize, sab8253xt_rbufsize))
	{
		Sab8253xCleanUpTransceiveN(port);	/* the network functions should be okay -- only difference */
		/* is the crc32 that is appended */
		return -ENODEV;
	}
	
	/*
	 * Start up serial port
	 */
	retval = sab8253x_startup(port); /* just in case closing the cu dev
					  * shutdown the port (but left CD) */
	if (retval)
	{
		return retval;
	}
	
	if ((port->count == 1) &&	/* first open */
	    (port->flags & FLAG8253X_SPLIT_TERMIOS)) 
	{
		if (tty->driver.subtype == SERIAL_TYPE_NORMAL)
		{
			*tty->termios = port->normal_termios;
		}
		else 
		{
			*tty->termios = port->callout_termios;
		}
		sab8253x_change_speed(port);
	}
	
	
#ifdef XCONFIG_SERIAL_CONSOLE
	if (sab8253x_console.cflag && sab8253x_console.index == line) 
	{
		tty->termios->c_cflag = sab8253x_console.cflag;
		sab8253x_console.cflag = 0;
		sab8253x_change_speed(port);
	}
#endif
	
	port->session = current->session;
	port->pgrp = current->pgrp;
	port->open_type = OPEN_ASYNC;
	return 0;
}

static char *signaling[] =
{
	"OFF  ",
	"RS232",
	"RS422",
	"RS485",
	"RS449",
	"RS530",
	"V.35 "
};

static int sab8253x_read_proc(char *page, char **start, off_t off, int count,
			      int *eof, void *data)
{
	extern struct sab_port * AuraPortRoot;
	struct sab_port *port = AuraPortRoot;
	extern char *board_type[];
	off_t begin = 0;
	int len = 0;
	int portno;
	unsigned int typeno;
	extern int sab8253x_rebootflag;
	
#ifdef FREEININTERRUPT
	len += sprintf(page, "serinfo:2.01I driver:%s\n", sab8253x_serial_version);
#else
	len += sprintf(page, "serinfo:2.01N driver:%s\n", sab8253x_serial_version);
#endif
	if(sab8253x_rebootflag)
	{
		len += sprintf(page+len, 
			       "WARNING:  Found %d cards that required reprogramming.  Reboot machine!.\n", 
			       sab8253x_rebootflag);
	}
	len += sprintf(page+len, "TTY MAJOR = %d, CUA MAJOR = %d, STTY MAJOR = %d.\n", 
		       sab8253x_serial_driver.major, sab8253x_callout_driver.major, 
		       sync_sab8253x_serial_driver.major);
	for (portno = 0; port != NULL; port = port->next, ++portno) 
	{
		typeno = port->board->b_type;
		if(typeno > BD_8520P)
		{
			typeno = 0;
		}
		len += sprintf(page+len, 
			       "%d: port %d: %s: v%d: chip %d: ATI %s: bus %d: slot %d: %s: ", 
			       sab8253x_serial_driver.minor_start + portno,
			       port->portno,
			       (port->chip->chip_type == ESCC2) ? "sab82532" : "sab82538",
			       port->type,
			       port->chip->c_chipno,
			       board_type[port->board->b_type],
			       port->board->b_dev.bus->number,
			       PCI_SLOT(port->board->b_dev.devfn),
			       aura_functionality[((port->function > FUNCTION_UN) ? FUNCTION_UN : port->function)]);
		switch(port->open_type)
		{
		case OPEN_ASYNC:
			len += sprintf(page+len, "openA");
			break;
		case OPEN_SYNC:
			len += sprintf(page+len, "openS");
			break;
		case OPEN_SYNC_NET:
			len += sprintf(page+len, "openN");
			break;	  
		case OPEN_SYNC_CHAR:
			len += sprintf(page+len, "openC");
			break;
		case OPEN_NOT:
			len += sprintf(page+len, "close");
			break;
		default:
			len += sprintf(page+len, "open?");
			break;
		}
		if(port->chip->c_cim)
		{
			if(port->chip->c_cim->ci_type == CIM_SP502)
			{
				len += sprintf(page+len, ": %s\n", signaling[port->sigmode]);
			}
			else
			{
				len += sprintf(page+len, ": NOPRG\n");
			}
		}
		else
		{
			len += sprintf(page+len, ": NOPRG\n");
		}

		if (len+begin > off+count)
		{
			goto done;
		}
		if (len+begin < off) 
		{
			begin += len;
			len = 0;
		}
	}
	*eof = 1;
 done:
	if (off >= len+begin)
	{
		return 0;
	}
	*start = page + (off-begin);
	return ((count < begin+len-off) ? count : begin+len-off);
}

/*
 * ---------------------------------------------------------------------
 * sab8253x_init() and friends
 *
 * sab8253x_init() is called at boot-time to initialize the serial driver.
 * ---------------------------------------------------------------------
 */

static void inline show_aurora_version(void)
{
	char *revision = "$Revision: 1.23 $";
	char *version, *p;
	
	version = strchr(revision, ' ');
	strcpy(sab8253x_serial_version, ++version);
	p = strchr(sab8253x_serial_version, ' ');
	*p = '\0';
	printk("Aurora serial driver version %s\n", sab8253x_serial_version);
}

#ifndef MODULE
static int GetMinorStart(void)
{
	struct tty_driver *ttydriver;
	int minor_start = 0;
	kdev_t device;
	
	device = MKDEV(TTY_MAJOR, minor_start);
	while(ttydriver = get_tty_driver(device), ttydriver != NULL)
	{
		minor_start += ttydriver->num;
		device = MKDEV(TTY_MAJOR, minor_start);
	}
	return minor_start;
	
}
#endif

int finish_sab8253x_setup_ttydriver(void) 
{
	extern unsigned int NumSab8253xPorts;
	
	sab8253x_tableASY = (struct tty_struct **) kmalloc(NumSab8253xPorts*sizeof(struct tty_struct *), GFP_KERNEL);
	if(sab8253x_tableASY == NULL)
	{
		printk(KERN_ALERT "auraXX20:  Could not allocate memory for sab8253x_tableASY.\n");
		return -1;
	}
	memset(sab8253x_tableASY, 0, NumSab8253xPorts*sizeof(struct tty_struct *));
	sab8253x_tableCUA = (struct tty_struct **) kmalloc(NumSab8253xPorts*sizeof(struct tty_struct *), GFP_KERNEL);
	if(sab8253x_tableCUA == NULL)
	{
		printk(KERN_ALERT "auraXX20:  Could not allocate memory for sab8253x_tableCUA.\n");
		return -1;
	}
	memset(sab8253x_tableCUA, 0, NumSab8253xPorts*sizeof(struct tty_struct *));
	sab8253x_tableSYN = (struct tty_struct **) kmalloc(NumSab8253xPorts*sizeof(struct tty_struct *), GFP_KERNEL);
	if(sab8253x_tableSYN == NULL)
	{
		printk(KERN_ALERT "auraXX20:  Could not allocate memory for sab8253x_tableSYN.\n");
		return -1;
	}
	memset(sab8253x_tableSYN, 0, NumSab8253xPorts*sizeof(struct tty_struct *));
	
	sab8253x_termios = (struct termios **) kmalloc(NumSab8253xPorts*sizeof(struct termios *), GFP_KERNEL);
	if(sab8253x_termios == NULL)
	{
		printk(KERN_ALERT "auraXX20:  Could not allocate memory for sab8253x_termios.\n");
		return -1;
	}
	memset(sab8253x_termios, 0, NumSab8253xPorts*sizeof(struct termios *));
	sab8253x_termios_locked = (struct termios **) kmalloc(NumSab8253xPorts*sizeof(struct termios *), GFP_KERNEL);
	if(sab8253x_termios_locked == NULL)
	{
		printk(KERN_ALERT "auraXX20:  Could not allocate memory for sab8253x_termios_locked.\n");
		return -1;
	}
	memset(sab8253x_termios_locked, 0, NumSab8253xPorts*sizeof(struct termios *));
	sync_sab8253x_serial_driver.num = sab8253x_callout_driver.num = sab8253x_serial_driver.num = NumSab8253xPorts;
	sab8253x_serial_driver.table = sab8253x_tableASY;
	sab8253x_callout_driver.table = sab8253x_tableCUA;
	sync_sab8253x_serial_driver.table = sab8253x_tableSYN;
	sync_sab8253x_serial_driver.termios = sab8253x_callout_driver.termios = sab8253x_serial_driver.termios = 
		sab8253x_termios;
	sync_sab8253x_serial_driver.termios_locked = sab8253x_callout_driver.termios_locked = 
		sab8253x_serial_driver.termios_locked = sab8253x_termios_locked;
	
	if (tty_register_driver(&sab8253x_serial_driver) < 0)
	{
		printk(KERN_ALERT "auraXX20:  Could not register serial driver.\n");
		return -1;
	}
	if (tty_register_driver(&sab8253x_callout_driver) < 0)
	{
		printk(KERN_ALERT "auraXX20:  Could not register call out device.\n");
		return -1;
	}
	if (tty_register_driver(&sync_sab8253x_serial_driver) < 0)
	{
		printk(KERN_ALERT "auraXX20:  Could not register sync serial device.\n");
		return -1;
	}
	return 0;
	
}

void sab8253x_setup_ttydriver(void) 
{
#ifdef MODULE
	extern int xx20_minorstart;
#endif
	init_bh(AURORA_BH, sab8253x_do_serial_bh);
	
	show_aurora_version();
	
	/* Initialize the tty_driver structure */
	
	memset(&sab8253x_serial_driver, 0, sizeof(struct tty_driver));
	sab8253x_serial_driver.magic = TTY_DRIVER_MAGIC;
	sab8253x_serial_driver.driver_name = "auraserial";
	sab8253x_serial_driver.name = "ttyS";
	sab8253x_serial_driver.major = TTY_MAJOR;
#ifdef MODULE
	sab8253x_serial_driver.minor_start = xx20_minorstart;
#else
	sab8253x_serial_driver.minor_start = GetMinorStart();
#endif
	sab8253x_serial_driver.type = TTY_DRIVER_TYPE_SERIAL;
	sab8253x_serial_driver.subtype = SERIAL_TYPE_NORMAL;
	sab8253x_serial_driver.init_termios = tty_std_termios;
	sab8253x_serial_driver.init_termios.c_cflag =
		B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	sab8253x_serial_driver.flags = TTY_DRIVER_REAL_RAW;
	sab8253x_serial_driver.refcount = &sab8253x_refcount;
	
	sab8253x_serial_driver.open = sab8253x_open;
	sab8253x_serial_driver.close = sab8253x_close;
	sab8253x_serial_driver.write = sab8253x_write;
	sab8253x_serial_driver.put_char = NULL; /*sab8253x_put_char is evil.*/
	sab8253x_serial_driver.flush_chars = sab8253x_flush_chars;
	sab8253x_serial_driver.write_room = sab8253x_write_room;
	sab8253x_serial_driver.chars_in_buffer = sab8253x_chars_in_buffer;
	sab8253x_serial_driver.flush_buffer = sab8253x_flush_buffer;
	sab8253x_serial_driver.ioctl = sab8253x_ioctl;
	sab8253x_serial_driver.throttle = sab8253x_throttle;
	sab8253x_serial_driver.unthrottle = sab8253x_unthrottle;
	sab8253x_serial_driver.send_xchar = sab8253x_send_xchar;
	sab8253x_serial_driver.set_termios = sab8253x_set_termios;
	sab8253x_serial_driver.stop = sab8253x_stop;
	sab8253x_serial_driver.start = sab8253x_start;
	sab8253x_serial_driver.hangup = sab8253x_hangup;
	sab8253x_serial_driver.break_ctl = sab8253x_break;
	sab8253x_serial_driver.wait_until_sent = sab8253x_wait_until_sent;
	sab8253x_serial_driver.read_proc = sab8253x_read_proc;
	
	/*
	 * The callout device is just like normal device except for
	 * major number and the subtype code.
	 */
	sab8253x_callout_driver = sab8253x_serial_driver;
	sab8253x_callout_driver.name = "cua";
	sab8253x_callout_driver.major = TTYAUX_MAJOR;
	sab8253x_callout_driver.subtype = SERIAL_TYPE_CALLOUT;
	sab8253x_callout_driver.read_proc = 0;
	sab8253x_callout_driver.proc_entry = 0;
	
	sync_sab8253x_serial_driver = sab8253x_serial_driver;
	sync_sab8253x_serial_driver.name = "sttyS";
	sync_sab8253x_serial_driver.major = 0;
	sync_sab8253x_serial_driver.subtype = SERIAL_TYPE_SYNCTTY;
	
	sync_sab8253x_serial_driver.open = sab8253x_openS;
	sync_sab8253x_serial_driver.close = sab8253x_closeS;
	sync_sab8253x_serial_driver.write = sab8253x_writeS;
	sync_sab8253x_serial_driver.put_char = NULL; /*sab8253x_put_char logic is evil*/
	sync_sab8253x_serial_driver.flush_chars = sab8253x_flush_charsS;
	sync_sab8253x_serial_driver.write_room = sab8253x_write_room;
	sync_sab8253x_serial_driver.chars_in_buffer = sab8253x_chars_in_buffer;
	sync_sab8253x_serial_driver.flush_buffer = sab8253x_flush_buffer;
	sync_sab8253x_serial_driver.ioctl = sab8253x_ioctl;
	sync_sab8253x_serial_driver.throttle = sab8253x_throttleS;
	sync_sab8253x_serial_driver.unthrottle = sab8253x_unthrottleS;
	sync_sab8253x_serial_driver.send_xchar = sab8253x_send_xcharS;
	sync_sab8253x_serial_driver.set_termios = sab8253x_set_termiosS;
	sync_sab8253x_serial_driver.stop = sab8253x_stopS;
	sync_sab8253x_serial_driver.start = sab8253x_startS;
	sync_sab8253x_serial_driver.hangup = sab8253x_hangupS;
	sync_sab8253x_serial_driver.break_ctl = sab8253x_breakS;
	sync_sab8253x_serial_driver.wait_until_sent = sab8253x_wait_until_sent;
	sync_sab8253x_serial_driver.read_proc = 0;
	sync_sab8253x_serial_driver.proc_entry = 0;
}

void sab8253x_setup_ttyport(struct sab_port *p_port) 
{
	p_port->magic = SAB_MAGIC;
	p_port->custom_divisor = 16;
	p_port->close_delay = 5*HZ/10;
	p_port->closing_wait = 30*HZ;
	p_port->tec_timeout = SAB8253X_MAX_TEC_DELAY;
	p_port->cec_timeout = SAB8253X_MAX_CEC_DELAY;
	p_port->x_char = 0;
	p_port->event = 0;	
	p_port->flags= FLAG8253X_BOOT_AUTOCONF | FLAG8253X_SKIP_TEST;
	p_port->blocked_open = 0;
	
	p_port->all_sent = 1;	/* probably not needed */
	
	p_port->tqueue.sync = 0;	/* for later */
	p_port->tqueue.routine = sab8253x_do_softint;
	p_port->tqueue.data = p_port;
	p_port->tqueue_hangup.sync = 0; /* for later */
	p_port->tqueue_hangup.routine = sab8253x_do_serial_hangup;
	p_port->tqueue_hangup.data = p_port;
	p_port->callout_termios = sab8253x_callout_driver.init_termios;
	p_port->normal_termios = sab8253x_serial_driver.init_termios; /* these are being shared */
	/* between asynchronous and */
	/* asynchronous ttys */
	init_waitqueue_head(&p_port->open_wait);
	init_waitqueue_head(&p_port->close_wait);
	init_waitqueue_head(&p_port->delta_msr_wait);
	init_waitqueue_head(&p_port->write_wait);
	init_waitqueue_head(&p_port->read_wait);
	
	p_port->count = 0;		/* maybe not needed */
	p_port->icount.cts = p_port->icount.dsr = 
		p_port->icount.rng = p_port->icount.dcd = 0;
	p_port->cts.val = p_port->dsr.val = 
		p_port->dcd.val = 0;
	p_port->icount.rx = p_port->icount.tx = 0;
	p_port->icount.frame = p_port->icount.parity = 0;
	p_port->icount.overrun = p_port->icount.brk = 0;
	
	p_port->xmit_fifo_size = 32;
	p_port->recv_fifo_size = 32;
	p_port->xmit_buf = NULL;
	p_port->receive_chars = sab8253x_receive_chars;
	p_port->transmit_chars = sab8253x_transmit_chars;
	p_port->check_status = sab8253x_check_status;
	p_port->receive_test = (SAB82532_ISR0_TCD | SAB82532_ISR0_TIME |
				SAB82532_ISR0_RFO | SAB82532_ISR0_RPF);
	p_port->transmit_test = (SAB82532_ISR1_ALLS | SAB82532_ISR1_XPR);
	p_port->check_status_test = SAB82532_ISR1_BRK;
	
	/* put in default sync control channel values
	 * not needed for async -- I think these work
	 * for bisync*/
	p_port->ccontrol.ccr0 = DEFAULT_CCR0;
	p_port->ccontrol.ccr1 = DEFAULT_CCR1;
	p_port->ccontrol.ccr2 = DEFAULT_CCR2;
	p_port->ccontrol.ccr3 = DEFAULT_CCR3;
	p_port->ccontrol.ccr4 = DEFAULT_CCR4;
	p_port->ccontrol.mode = DEFAULT_MODE;
	p_port->ccontrol.rlcr = DEFAULT_RLCR;
}

void sab8253x_cleanup_ttydriver(void)
{
	unsigned long flags;
	int e1, e2;
	
	save_flags(flags);
	cli();
	
	if(sab8253x_tableASY) 
		kfree(sab8253x_tableASY);
	if(sab8253x_tableCUA) 
		kfree(sab8253x_tableCUA);
	if(sab8253x_tableSYN) 
		kfree(sab8253x_tableSYN);
	if(sab8253x_termios) 
		kfree(sab8253x_termios);
	if(sab8253x_termios_locked) 
		kfree(sab8253x_termios_locked);
	
	remove_bh(AURORA_BH);
	if ((e1 = tty_unregister_driver(&sab8253x_serial_driver)))
	{
		printk("SERIAL: failed to unregister serial driver (%d)\n",
		       e1);
	}
	if ((e2 = tty_unregister_driver(&sab8253x_callout_driver)))
	{
		printk("SERIAL: failed to unregister callout driver (%d)\n", 
		       e2);
	}
	if ((e2 = tty_unregister_driver(&sync_sab8253x_serial_driver)))
	{
		printk("SERIAL: failed to unregister callout driver (%d)\n", 
		       e2);
	}
	restore_flags(flags);  
}

/* THE CODE BELOW HAS NOT YET BEEN MODIFIED!!!! FW */
#ifdef XCONFIG_SERIAL_CONSOLE

static inline void
sab8253x_console_putchar(struct sab8253x *info, char c)
{
	unsigned long flags;
	
	save_flags(flags); cli();
	sab8253x_tec_wait(info);
	WRITEB(port,tic,c);
	restore_flags(flags);
}

static void
sab8253x_console_write(struct console *con, const char *s, unsigned n)
{
	struct sab8253x *info;
	int i;
	
	info = sab8253x_chain;
	for (i = con->index; i; i--) {
		info = info->next;
		if (!info)
			return;
	}
	
	for (i = 0; i < n; i++) {
		if (*s == '\n')
			sab8253x_console_putchar(info, '\r');
		sab8253x_console_putchar(info, *s++);
	}
	sab8253x_tec_wait(info);
}

static int
sab8253x_console_wait_key(struct console *con)
{
	sleep_on(&keypress_wait);
	return 0;
}

static kdev_t
sab8253x_console_device(struct console *con)
{
	return MKDEV(TTY_MAJOR, 64 + con->index);
}

static int
sab8253x_console_setup(struct console *con, char *options)
{
	struct sab8253x *info;
	unsigned int	ebrg;
	tcflag_t	cflag;
	unsigned char	dafo;
	int		i, bits;
	unsigned long	flags;
	
	info = sab8253x_chain;
	for (i = con->index; i; i--) 
	{
		info = info->next;
		if (!info)
			return -ENODEV;
	}
	info->is_console = 1;
	
	/*
	 * Initialize the hardware
	 */
	sab8253x_init_line(info);
	
	/*
	 * Finally, enable interrupts
	 */
	info->interrupt_mask0 = SAB82532_IMR0_PERR | SAB82532_IMR0_FERR |
		/*SAB82532_IMR0_TIME*/ | SAB82532_IMR0_PLLA/*| SAB82532_IMR0_CDSC*/;
	WRITEB(port,imr0,info->interrupt_mask0);
	info->interrupt_mask1 = SAB82532_IMR1_BRKT | SAB82532_IMR1_ALLS |
		SAB82532_IMR1_XOFF | SAB82532_IMR1_TIN |
		SAB82532_IMR1_CSC | SAB82532_IMR1_XON |
		SAB82532_IMR1_XPR;
	WRITEB(port,imr1,info->interrupt_mask1);
	
	printk("Console: ttyS%d (SAB82532)\n", info->line);
	
	sunserial_console_termios(con);
	cflag = con->cflag;
	
	/* Byte size and parity */
	switch (cflag & CSIZE) 
	{
	case CS5: dafo = SAB82532_DAFO_CHL5; bits = 7; break;
	case CS6: dafo = SAB82532_DAFO_CHL6; bits = 8; break;
	case CS7: dafo = SAB82532_DAFO_CHL7; bits = 9; break;
	case CS8: dafo = SAB82532_DAFO_CHL8; bits = 10; break;
		/* Never happens, but GCC is too dumb to figure it out */
	default:  dafo = SAB82532_DAFO_CHL5; bits = 7; break;
	}
	
	if (cflag & CSTOPB) 
	{
		dafo |= SAB82532_DAFO_STOP;
		bits++;
	}
	
	if (cflag & PARENB) 
	{
		dafo |= SAB82532_DAFO_PARE;
		bits++;
	}
	
	if (cflag & PARODD) 
	{
#ifdef CMSPAR
		if (cflag & CMSPAR)
			dafo |= SAB82532_DAFO_PAR_MARK;
		else
#endif
			dafo |= SAB82532_DAFO_PAR_ODD;
	} 
	else 
	{
#ifdef CMSPAR
		if (cflag & CMSPAR)
			dafo |= SAB82532_DAFO_PAR_SPACE;
		else
#endif
			dafo |= SAB82532_DAFO_PAR_EVEN;
	}
	
	/* Determine EBRG values based on baud rate */
	i = cflag & CBAUD;
	if (i & CBAUDEX) 
	{
		i &= ~(CBAUDEX);
		if ((i < 1) || ((i + 15) >= NR_EBRG_VALUES))
			cflag &= ~CBAUDEX;
		else
			i += 15;
	}
	ebrg = ebrg_tabl[i].n;
	ebrg |= (ebrg_table[i].m << 6);
	
	info->baud = ebrg_table[i].baud;
	if (info->baud)
		info->timeout = (info->xmit_fifo_size * HZ * bits) / info->baud;
	else
		info->timeout = 0;
	info->timeout += HZ / 50;		/* Add .02 seconds of slop */
	
	/* CTS flow control flags */
	if (cflag & CRTSCTS)
		info->flags |= FLAG8253X_CTS_FLOW;
	else
		info->flags &= ~(FLAG8253X_CTS_FLOW);
	
	if (cflag & CLOCAL)
		info->flags &= ~(FLAG8253X_CHECK_CD);
	else
		info->flags |= FLAG8253X_CHECK_CD;
	
	save_flags(flags); cli();
	sab8253x_cec_wait(info);
	sab8253x_tec_wait(info);
	WRITEB(port,dafo,dafo);
	WRITEB(port,bgr,ebrg & 0xff);
	info->regs->rw.ccr2 &= ~(0xc0);
	info->regs->rw.ccr2 |= (ebrg >> 2) & 0xc0;
	if (info->flags & FLAG8253X_CTS_FLOW) 
	{
		info->regs->rw.mode &= ~(SAB82532_MODE_RTS);
		info->regs->rw.mode |= SAB82532_MODE_FRTS;
		info->regs->rw.mode &= ~(SAB82532_MODE_FCTS);
	} 
	else 
	{
		info->regs->rw.mode |= SAB82532_MODE_RTS;
		info->regs->rw.mode &= ~(SAB82532_MODE_FRTS);
		info->regs->rw.mode |= SAB82532_MODE_FCTS;
	}
	info->regs->rw.pvr &= ~(info->pvr_dtr_bit);
	info->regs->rw.mode |= SAB82532_MODE_RAC;
	restore_flags(flags);
	
	return 0;
}

static struct console sab8253x_console = 
{
	"ttyS",
	sab8253x_console_write,
	NULL,
	sab8253x_console_device,
	sab8253x_console_wait_key,
	NULL,
	sab8253x_console_setup,
	CON_PRINTBUFFER,
	-1,
	0,
	NULL
};

int sab8253x_console_init(void)
{
	extern int con_is_present(void);
	extern int su_console_registered;
	
	if (con_is_present() || su_console_registered)
		return 0;
	
	if (!sab8253x_chain) 
	{
		prom_printf("sab8253x_console_setup: can't get SAB8253X chain");
		prom_halt();
	}
	
	sab8253x_console.index = serial_console - 1;
	register_console(&sab8253x_console);
	return 0;
}

#ifdef SERIAL_LOG_DEVICE

static int serial_log_device = 0;

static void
dprint_init(int tty)
{
	serial_console = tty + 1;
	sab8253x_console.index = tty;
	sab8253x_console_setup(&sab8253x_console, "");
	serial_console = 0;
	serial_log_device = tty + 1;
}

int
dprintf(const char *fmt, ...)
{
	static char buffer[4096];
	va_list args;
	int i;
	
	if (!serial_log_device)
		return 0;
	
	va_start(args, fmt);
	i = vsprintf(buffer, fmt, args);
	va_end(args);
	sab8253x_console.write(&sab8253x_console, buffer, i);
	return i;
}

#endif /* SERIAL_LOG_DEVICE */
#endif /* XCONFIG_SERIAL_CONSOLE */
