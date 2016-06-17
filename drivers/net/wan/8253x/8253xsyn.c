/* -*- linux-c -*- */
/* $Id: 8253xsyn.c,v 1.17 2002/02/10 22:17:25 martillo Exp $
 * 8253xsyn.c: SYNC TTY Driver for the SIEMENS SAB8253X DUSCC.
 *
 * Implementation, modifications and extensions
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
#include "8253xctl.h"
#include "8253x.h"
#include <linux/pci.h>
#include <linux/fs.h>

#ifdef MODULE
#undef XCONFIG_SERIAL_CONSOLE
#endif


static void sab8253x_flush_to_ldiscS(void *private_) /* need a separate version for sync
						 there are no flags associated with
						 received sync TTY data*/
{
	struct tty_struct *tty = (struct tty_struct *) private_;
	unsigned char	*cp;
	int		count;
	struct sab_port *port;
	struct sk_buff *skb;  
	
	if(tty)
	{
		port = (struct sab_port *)tty->driver_data;
	}
	else
	{
		return;
	}
	if(port == NULL)
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
		(*tty->ldisc.receive_buf)(tty, cp, 0, count);
		dev_kfree_skb_any(skb);
	}
	port->DoingInterrupt = 0;
}

void sab8253x_flush_charsS(struct tty_struct *tty)
{
	struct sab_port *port = (struct sab_port *)tty->driver_data;
	
	if (sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_flush_chars"))
	{
		return;
	}
	
	if ((Sab8253xCountTransmit(port) <= 0) || tty->stopped || tty->hw_stopped)
	{				/* can't flush */
		return;
	}
	
	sab8253x_start_txS(port);
}

/*
 * ------------------------------------------------------------
 * sab8253x_stopS() and sab8253x_startS()
 *
 * This routines are called before setting or resetting tty->stopped.
 * They enable or disable transmitter interrupts, as necessary.
 * ------------------------------------------------------------
 */

void sab8253x_stopS(struct tty_struct *tty)
{
	struct sab_port *port = (struct sab_port *)tty->driver_data;
	/* can't do anything here */
	if (sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_stop"))
	{
		return;
	}
	/*  interrupt handles it all*/
	/* turning off XPR is not an option in sync mode */
}

void sab8253x_startS(struct tty_struct *tty)
{
	struct sab_port *port = (struct sab_port *)tty->driver_data;
	
	if (sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_start"))
	{
		return;
	}
	sab8253x_start_txS(port);
}


static void sab8253x_receive_charsS(struct sab_port *port,
			     union sab8253x_irq_status *stat)
{
	struct tty_struct *tty = port->tty;
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
	
#ifdef CONSOLE_SUPPORT
	if (port->is_console)
	{
		wake_up(&keypress_wait);
	}
#endif
	
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
		
		if (!tty)
		{
			return;
		}
		if(skb = dev_alloc_skb(total_size), skb)
		{
			memcpy(skb->data, &port->msgbuf[0], total_size);
			skb->tail = (skb->data + total_size);
			skb->data_len = total_size;
			skb->len = total_size;
			skb_queue_tail(port->sab8253xc_rcvbuflist, skb);
		}
		queue_task(&tty->flip.tqueue, &tq_timer); /* clear out flip buffer as fast as possible
							   * maybe should not be done unconditionally hear
							   * but should be within the above consequence
							   * clause */
	}
}


static void sab8253x_check_statusS(struct sab_port *port,
			    union sab8253x_irq_status *stat)
{
	struct tty_struct *tty = port->tty;
	int modem_change = 0;
	mctlsig_t         *sig;
	
	if (!tty)
	{
		return;
	}
	
	/* check_modem:*/
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
			wake_up_interruptible(&port->open_wait);
		}
		else if (!((port->flags & FLAG8253X_CALLOUT_ACTIVE) &&
			   (port->flags & FLAG8253X_CALLOUT_NOHUP))) 
		{
#if 0				/* requires more investigation */
			MOD_INC_USE_COUNT;
			if (schedule_task(&port->tqueue_hangup) == 0)
			{
				MOD_DEC_USE_COUNT;
			}
#endif
		}
	}
	
	sig = &port->cts;
	if (port->flags & FLAG8253X_CTS_FLOW) 
	{				/* not setting this yet */
		if (port->tty->hw_stopped) 
		{
			if (sig->val) 
			{
				port->tty->hw_stopped = 0;
				sab8253x_sched_event(port, SAB8253X_EVENT_WRITE_WAKEUP);
				sab8253x_start_txS(port);
			}
		} 
		
		else 
		{
			if(!(getccr2configS(port) & SAB82532_CCR2_TOE))
			{
				if (!(sig->val)) 
				{
					port->tty->hw_stopped = 1;
				}
			}
		}
	}
}

/*
 * This routine is called to set the UART divisor registers to match
 * the specified baud rate for a serial port.
 */
static void sab8253x_change_speedS(struct sab_port *port)
{
	unsigned long	flags,baud;
	tcflag_t	cflag;
	u8	        ccr2=0,ccr4=0,ebrg=0;
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
		bits = 7; 
		break;
	case CS6: 
		bits = 8; 
		break;
	case CS7: 
		bits = 9; 
		break;
	default:
	case CS8: 
		bits = 10; 
		break;
	}
	
	if (cflag & CSTOPB) 
	{
		bits++;
	}
	
	if (cflag & PARENB) 
	{
		bits++;
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
		port->ignore_status_mask |= SAB82532_ISR0_RPF;
	
	save_flags(flags); 
	cli();
	sab8253x_cec_wait(port);
	
	WRITEB(port, bgr, ebrg);
	WRITEB(port, ccr2, READB(port, ccr2) & ~(0xc0)); /* clear out current baud rage */
	WRITEB(port, ccr2, READB(port, ccr2) | ccr2);
	WRITEB(port, ccr4, (READB(port,ccr4) & ~SAB82532_CCR4_EBRG) | ccr4);
	
	if (port->flags & FLAG8253X_CTS_FLOW) 
	{
		WRITEB(port, mode, READB(port,mode) & ~(SAB82532_MODE_RTS));
		port->interrupt_mask1 &= ~(SAB82532_IMR1_CSC);
		WRITEB(port, imr1, port->interrupt_mask1);
	} 
	else 
	{
		WRITEB(port, mode, READB(port,mode) | SAB82532_MODE_RTS);
		port->interrupt_mask1 |= SAB82532_IMR1_CSC;
		WRITEB(port, imr1, port->interrupt_mask1);
	}
	WRITEB(port, mode, READB(port, mode) | SAB82532_MODE_RAC);
	restore_flags(flags);
}

void sab8253x_set_termiosS(struct tty_struct *tty,
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
	sab8253x_change_speedS(port);
	
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
		RAISE(port,dtr);
		if (!tty->hw_stopped ||
		    !(tty->termios->c_cflag & CRTSCTS)) 
		{
			RAISE(port,rts);
		}
	}
	
	/* Handle turning off CRTSCTS */
	if ((old_termios->c_cflag & CRTSCTS) &&
	    !(tty->termios->c_cflag & CRTSCTS)) 
	{
		tty->hw_stopped = 0;
		sab8253x_startS(tty);
	}
}

static int sab8253x_startupS(struct sab_port *port)
{
	unsigned long flags;
	int retval = 0;
	
	save_flags(flags); cli();
	
	port->msgbufindex = 0;
	port->xmit_buf = NULL;
	port->buffergreedy = 0;
	
	if (port->flags & FLAG8253X_INITIALIZED) 
	{
		goto errout;
	}
	
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
	sab8253x_init_lineS(port);
	
#if 0				/* maybe should be conditional */
	if (port->tty->termios->c_cflag & CBAUD) 
	{
#endif
		/* Activate RTS */
		RAISE(port,rts);
		/* Activate DTR */
		RAISE(port,dtr);
#if 0
	}
#endif
	
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
	
	if (port->tty)
	{
		clear_bit(TTY_IO_ERROR, &port->tty->flags);
	}
	port->xmit_cnt = port->xmit_head = port->xmit_tail = 0;
	
	/*
	 * and set the speed of the serial port
	 */
	sab8253x_change_speedS(port);
	
	port->flags |= FLAG8253X_INITIALIZED;
	port->receive_chars = sab8253x_receive_charsS;
	port->transmit_chars = sab8253x_transmit_charsS;
	port->check_status = sab8253x_check_statusS;
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

static void sab8253x_shutdownS(struct sab_port *port)
{
	unsigned long flags;
	
	if (!(port->flags & FLAG8253X_INITIALIZED))
	{
		return;
	}
	
	save_flags(flags); cli(); /* Disable interrupts */
	
	/*
	 * clear delta_msr_wait queue to avoid mem leaks: we may free the irq
	 * here so the queue might never be waken up
	 */
	wake_up_interruptible(&port->delta_msr_wait);
	
	if (port->xmit_buf) 
	{
		port->xmit_buf = 0;
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

int sab8253x_writeS(struct tty_struct * tty, int from_user,
		    const unsigned char *buf, int count)
{
	struct sab_port *port = (struct sab_port *)tty->driver_data;
	struct sk_buff *skb;
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
		sab8253x_start_txS(port);	/* no descriptor slot */
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
		printk(KERN_ALERT "sab8253xs: no skbuffs available.\n");
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
	
	sab8253x_start_txS(port);
	return count;
}

void sab8253x_throttleS(struct tty_struct * tty)
{
	struct sab_port *port = (struct sab_port *)tty->driver_data;
	
	if (sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_throttleS"))
	{
		return;
	}
	
	if (!tty)
	{
		return;
	}
	
	if (I_IXOFF(tty))
	{
		sab8253x_send_xcharS(tty, STOP_CHAR(tty));
	}
}

void sab8253x_unthrottleS(struct tty_struct * tty)
{
	struct sab_port *port = (struct sab_port *)tty->driver_data;
	
	if (sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_unthrottle"))
	{
		return;
	}
	
	if (!tty)
	{
		return;
	}
	
	if (I_IXOFF(tty)) 
	{
		sab8253x_send_xcharS(tty, START_CHAR(tty));
	}
}

void sab8253x_send_xcharS(struct tty_struct *tty, char ch)
{
	struct sab_port *port = (struct sab_port *)tty->driver_data;
	unsigned long flags;
	int stopped;
	int hw_stopped;
	
	if (sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_send_xcharS"))
	{
		return;
	}
	
	if (!tty)
	{
		return;
	}
	
	if(port->sabnext2.transmit == NULL)
	{
		return;
	}
	
	save_flags(flags); cli();
	
	if((port->sabnext2.transmit->Count & OWNER) == OWN_SAB) /* may overwrite a character
								 * -- but putting subsequent
								 * XONs or XOFFs later in the
								 * stream could cause problems
								 * with the XON and XOFF protocol */
	{
		port->sabnext2.transmit->sendcrc = 1;
		port->sabnext2.transmit->crcindex = 3;
		port->sabnext2.transmit->crc = (ch << 24); /* LITTLE ENDIAN */
		restore_flags(flags);
	}
	else
	{
		restore_flags(flags);
		sab8253x_writeS(tty, 0, &ch, 1);
	}
	
	stopped = tty->stopped;
	hw_stopped = tty->hw_stopped;
	tty->stopped = 0;
	tty->hw_stopped = 0;
	
	sab8253x_start_txS(port);
	
	tty->stopped = stopped;
	tty->hw_stopped = hw_stopped;
}


void sab8253x_breakS(struct tty_struct *tty, int break_state)
{
	struct sab_port *port = (struct sab_port *) tty->driver_data;
	
	if (sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_breakS"))
	{
		return;
	} /* can't break in sync mode */
}

void sab8253x_closeS(struct tty_struct *tty, struct file * filp)
{
	struct sab_port *port = (struct sab_port *)tty->driver_data;
	unsigned long flags;
	
	MOD_DEC_USE_COUNT;	
	if (sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_closeS"))
	{
		return;
	}

	if(port->open_type == OPEN_SYNC_NET)
	{				/* port->tty field should already be NULL */
		return;
	}
	
	save_flags(flags); cli();
	--(port->count);
	if (tty_hung_up_p(filp)) 
	{
		if(port->count == 0)	/* I think the reason for the weirdness
					   relates to freeing of structures in
					   the tty driver */
		{
			port->open_type = OPEN_NOT;
		}
		else if(port->count < 0)
		{
			printk(KERN_ALERT "XX20: port->count went negative.\n");
			port->count = 0;
			port->open_type = OPEN_NOT;
		}
		restore_flags(flags);
		return;
	}
	
#if 0
	if ((tty->count == 1) && (port->count != 0)) 
	{
		/*
		 * Uh, oh.  tty->count is 1, which means that the tty
		 * structure will be freed.  port->count should always
		 * be one in these conditions.  If it's greater than
		 * one, we've got real problems, since it means the
		 * serial port won't be shutdown.
		 */
		printk("sab8253x_close: bad serial port count; tty->count is 1,"
		       " port->count is %d\n", port->count);
		port->count = 0;
	}
#endif
	
	if (port->count < 0) 
	{
		printk(KERN_ALERT "sab8253x_close: bad serial port count for ttys%d: %d\n",
		       port->line, port->count);
		port->count = 0;
	}
	if (port->count) 
	{
		restore_flags(flags);
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
		tty_wait_until_sent(tty, port->closing_wait);
	}
	
	/*
	 * At this point we stop accepting input.  To do this, we
	 * disable the receive line status interrupts, and turn off
	 * the receiver.
	 */
	
#if 0
	port->interrupt_mask0 |= SAB82532_IMR0_TCD; /* not needed for sync */
#endif
	WRITEB(port,imr0,port->interrupt_mask0);
	
	CLEAR_REG_BIT(port, mode, SAB82532_MODE_RAC); /* turn off receiver */
	
	if (port->flags & FLAG8253X_INITIALIZED) 
	{
		/*
		 * Before we drop DTR, make sure the UART transmitter
		 * has completely drained; this is especially
		 * important if there is a transmit FIFO!
		 */
		sab8253x_wait_until_sent(tty, port->timeout);
	}
	sab8253x_shutdownS(port);
	Sab8253xCleanUpTransceiveN(port);
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
		wake_up_interruptible(&port->open_wait);
	}
	port->flags &= ~(FLAG8253X_NORMAL_ACTIVE|FLAG8253X_CALLOUT_ACTIVE|
			 FLAG8253X_CLOSING);
	wake_up_interruptible(&port->close_wait);
	port->open_type = OPEN_NOT;
	restore_flags(flags);
}


void sab8253x_hangupS(struct tty_struct *tty)
{
	struct sab_port * port = (struct sab_port *)tty->driver_data;
	
	if (sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_hangupS"))
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
		sab8253x_shutdownS(port);
		Sab8253xCleanUpTransceiveN(port);
		port->event = 0;
		port->flags &= ~(FLAG8253X_NORMAL_ACTIVE|FLAG8253X_CALLOUT_ACTIVE);
		port->tty = 0;
		wake_up_interruptible(&port->open_wait);
	}
}

int sab8253x_openS(struct tty_struct *tty, struct file * filp)
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
		printk(KERN_ALERT "sab8253x_openS: can't find structure for line %d\n",
		       line);
		return -ENODEV;
	}
	
	save_flags(flags);		/* Need to protect port->tty element */
	cli();
	
	if(port->tty == 0)
	{
		port->tty = tty;
		tty->flip.tqueue.routine = sab8253x_flush_to_ldiscS;
	}
	tty->driver_data = port;
	
	if(port->function != FUNCTION_NR)
	{
		++(port->count);
		restore_flags(flags);
		return -ENODEV;		/* only allowed if there are no restrictions on the port */
	}
	
	if(port->open_type == OPEN_SYNC_NET)
	{
		port->tty = NULL;	/* Don't bother with open counting here
					   but make sure the tty field is NULL*/
		restore_flags(flags);
		return -EBUSY;
	}
	
	restore_flags(flags);
	
	if (sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_openS"))
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
	
	if(port->flags & FLAG8253X_NORMAL_ACTIVE)
	{
		if(port->open_type == OPEN_ASYNC)
		{
			++(port->count);
			return -EBUSY;	/* can't reopen in sync mode */
		}
	}
	if(port->open_type > OPEN_SYNC) /* can reopen a SYNC_TTY */
	{
		return -EBUSY;
	}
	if(Sab8253xSetUpLists(port))
	{
		++(port->count);
		return -ENODEV;
	}
	if(Sab8253xInitDescriptors2(port, sab8253xs_listsize, sab8253xs_rbufsize))
	{
		++(port->count);
		return -ENODEV;
	}
	
	retval = sab8253x_startupS(port);
	if (retval)
	{
		++(port->count);
		return retval;		/* does not check channel mode */
	}
	
	retval = sab8253x_block_til_ready(tty, filp, port); /* checks channel mode */
	++(port->count);
	if (retval) 
	{
		return retval;
	}
	
	port->tty = tty;		/* may change here once through the block */
	/* because now the port belongs to an new tty */
	tty->flip.tqueue.routine = sab8253x_flush_to_ldiscS;
	if(Sab8253xSetUpLists(port))
	{
		return -ENODEV;
	}
	if(Sab8253xInitDescriptors2(port, sab8253xs_listsize, sab8253xs_rbufsize))
	{
		Sab8253xCleanUpTransceiveN(port);	/* the network functions should be okay -- only difference */
		/* is the crc32 that is appended */
		return -ENODEV;
	}
	
	/*
	 * Start up serial port
	 */
	retval = sab8253x_startupS(port); /* in case cu was running the first time
					   * the function was called*/
	if (retval)
	{
		return retval;		/* does not check channel mode */
	}
	
	if ((port->count == 1) &&
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
		sab8253x_change_speedS(port);
	}
	
	
#ifdef XCONFIG_SERIAL_CONSOLE
	if (sab8253x_console.cflag && sab8253x_console.index == line) 
	{
		tty->termios->c_cflag = sab8253x_console.cflag;
		sab8253x_console.cflag = 0;
		change_speed(port);
	}
#endif
	
	port->session = current->session;
	port->pgrp = current->pgrp;
	port->open_type = OPEN_SYNC;
	return 0;
}




