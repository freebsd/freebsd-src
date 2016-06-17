/* -*- linux-c -*- */
/* $Id: 8253xutl.c,v 1.3 2002/02/10 22:17:26 martillo Exp $
 * 8253xutl.c: SYNC TTY Driver for the SIEMENS SAB8253X DUSCC.
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
#include "sp502.h"

#ifdef MODULE
#undef XCONFIG_SERIAL_CONSOLE
#endif

void sab8253x_start_txS(struct sab_port *port)
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
	struct tty_struct *tty = port->tty; /* a little gross tty flags whether
					       invoked from a tty or the network */
	
	fifospace = port->xmit_fifo_size; /* This code can handle fragmented frames
					     although currently none are generated*/
	loadedcount = 0;
	
	if(port->sabnext2.transmit == NULL)
	{
		return;
	}
	
	save_flags(flags); 
	cli();			
	
	
	if(count = port->sabnext2.transmit->Count, (count & OWNER) == OWN_SAB)
	{
		count &= ~OWN_SAB; /* OWN_SAB is really 0 but cannot guarantee in the future */
		
		if(port->sabnext2.transmit->HostVaddr)
		{
			total = (port->sabnext2.transmit->HostVaddr->tail - 
				 port->sabnext2.transmit->HostVaddr->data); /* packet size */
		}
		else
		{
			total = 0;		/* the data is only the crc/trailer */
		}
		
		if(tty && (tty->stopped || tty->hw_stopped) && (count == total))
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
			if(tty && (totaltransmit < (sab8253xs_listsize/2))) /* only makes sense on a TTY */
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
			
			/* fifospace -= loadedcount;*/
			/* Here to make mods to handle arbitrarily fragmented frames look to 8253xtty.c for help */
			
			if ((count <= 0) && (port->sabnext2.transmit->sendcrc == 0))
			{
				port->sabnext2.transmit->Count = OWN_DRIVER;
				if(!tty)
				{		/* called by network driver */
					++(port->Counters.transmitpacket);
				}
#ifdef FREEININTERRUPT		/* treat this routine as if taking place in interrupt */
				if(port->sabnext2.transmit->HostVaddr)
				{
					skb_unlink(port->sabnext2.transmit->HostVaddr);
					dev_kfree_skb_any(port->sabnext2.transmit->HostVaddr);
					port->sabnext2.transmit->HostVaddr = 0; /* no skb */
				}
				port->sabnext2.transmit->crcindex = 0; /* no single byte */
#endif
				sab8253x_cec_wait(port);
				WRITEB(port, cmdr, SAB82532_CMDR_XME|SAB82532_CMDR_XTF); /* Terminate the frame */
				
				port->sabnext2.transmit = port->sabnext2.transmit->VNext;
				
				if(!tty && port->tx_full)	/* invoked from the network driver */
				{
					port->tx_full = 0; /* there is a free slot */
					switch(port->open_type)
					{
					case OPEN_SYNC_NET:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
						port->dev->start = 1;
						port->dev->tbusy = 0;	/* maybe need mark_bh here */
#else
						netif_start_queue(port->dev);
#endif
						break;
						
					case OPEN_SYNC_CHAR:
						wake_up_interruptible(&port->write_wait);
						break;
						
					default:
						break;
					}
				}
				
				if((port->sabnext2.transmit->Count & OWNER) == OWN_SAB)
				{		/* new frame to send */
					port->interrupt_mask1 &= ~(SAB82532_IMR1_XPR);
					WRITEB(port, imr1, port->interrupt_mask1);
				}
				else
				{
					port->interrupt_mask1 |= SAB82532_IMR1_XPR;
					WRITEB(port, imr1, port->interrupt_mask1);
					if((port->open_type == OPEN_SYNC_CHAR) && port->async_queue)
					{		/* if indication of transmission is needed by the */
						/* application on a per-frame basis kill_fasync */
						/* can provide it */
						kill_fasync(&port->async_queue, SIGIO, POLL_OUT);
					}
				}
				restore_flags(flags);
				return;
			}
			/* Issue a Transmit FIFO command. */
			sab8253x_cec_wait(port);
			WRITEB(port, cmdr, SAB82532_CMDR_XTF);	
			port->sabnext2.transmit->Count = (count|OWN_SAB);
		}
		port->interrupt_mask1 &= ~(SAB82532_IMR1_XPR); /* more to send */
		WRITEB(port, imr1, port->interrupt_mask1);
	}
	else
	{				/* nothing to send */
		port->interrupt_mask1 |= SAB82532_IMR1_XPR;
		WRITEB(port, imr1, port->interrupt_mask1);
	}
	restore_flags(flags);
	return;
}

void sab8253x_transmit_charsS(struct sab_port *port,
			      union sab8253x_irq_status *stat)
{
	if (stat->sreg.isr1 & SAB82532_ISR1_ALLS) 
	{
		port->interrupt_mask1 |= SAB82532_IMR1_ALLS;
		WRITEB(port, imr1, port->interrupt_mask1);
		port->all_sent = 1;
	}
	sab8253x_start_txS(port);
}

/*
 * This routine is called to set the UART divisor registers to match
 * the specified baud rate for a serial port.
 */

/***************************************************************************
 * sab_baudenh:      Function to compute the "enhanced" baudrate.
 *                
 *
 *     Parameters   : 
 *                  encbaud  2* the baudrate. We use the
 *                           double value so as to support 134.5 (in only)
 *                  clkspeed The board clock speed in Hz.
 *                  bgr      Value of reg BGR for baudrate(output)
 *                  ccr2     Value of reg // CCR2 for baudrate (output)
 *                  ccr4     Value of reg CCR4 for baudrate (output)
 *                  truebaud The actual baudrate achieved (output).
 *
 *
 *     Return value : Return FALSE the parameters could not be computed, 
 *
 *     Prerequisite : The various ports must have been initialized
 *
 *     Remark       : Stolen from the Aurora ase driver.
 *
 *     Author       : fw
 *
 *     Revision     : Oct 9 2000, creation
 ***************************************************************************/
/*
 * Macro to check to see if the high n bits of the given unsigned long
 *  are zero.
 */               
#define HIZERO(x, n)        ( ((unsigned long) ((x) << (n)) >> (n)) == (x))
/* form an n-bit bitmask */
#define NBM(n)                  (~(((~(unsigned long) 0) >> (n)) << (n)))
/* shift x by y bits to right, rounded */
#define ROUND_SHIFT(x, y)       (((unsigned long) (x) + (NBM(y - 1) + 1)) >> (y))
/* perform rounded division */
#define ROUND_DIV(x, y) (((x) + ((y) >> 1)) / (y))
#define ABSDIF(x, y)    ((x) > (y) ? ((x) - (y)) : ((y) - (x))) 
static unsigned int
sab8253x_baudenh(unsigned long encbaud, unsigned long clk_speed,
		 unsigned char *bgr, unsigned char *ccr2,
		 unsigned long *truebaudp)
{
	register unsigned short	 tmp;
	register unsigned char	 ccr2tmp;
	unsigned long		 power2, mant;
	unsigned int			 fastclock;
	
	if (encbaud == 0) {
		return FALSE;
	}
	
	/*
	 * Keep dividing quotien by two until it is between the value of 1 and 64,
	 *  inclusive.
	 */
	
	fastclock = (clk_speed >= 10000000);	/* >= 10 MHz */
	
	for (power2 = 0; power2 < 16; power2++) 
	{
		/* divisor = baud * 2^M * 16 */
		if (!HIZERO(encbaud, power2 + 3)) 
		{
			if (!HIZERO(encbaud, power2)) 
			{	/* baud rate still too big? */
				mant = ROUND_DIV(ROUND_SHIFT(clk_speed, power2 + 3), encbaud);
				
				/* mant = (clk_speed / (8 * 2^M)) / (baud * 2) */
				/*	= clk_speed / (baud * 16 * 2^M) */
			}
			else 
			{
				mant = ROUND_DIV(ROUND_SHIFT(clk_speed, 3), encbaud << power2);
				/* mant = (clk_speed / 8) / (baud * 2 * 2^M) */
				/*	= clk_speed / (baud * 16 * 2^M) */
			}
		}
		else 
		{
			mant = ROUND_DIV(clk_speed, encbaud << (power2 + 3));
			/* mant = clk_speed / (baud * 2 * 8 * 2^M) */
			/*	    = clk_speed / (baud * 16 * 2^M) */
		}
		
		/* mant = clk_speed / (baud * 2^M * 16) */
		
		if (mant < 2
		    || (mant <= 64 && (!fastclock || power2 != 0))) 
		{
			break;
		}
	}
	
	/*
	 * Did we not succeed?  (Baud rate is too small)
	 */
	if (mant > 64) 
	{
		return FALSE;
	}
	
	/*
	 * Now, calculate the true baud rate.
	 */
	
	if (mant < 1 || (mant == 1 && power2 == 0)) 
	{
		/* bgr and ccr2 should be initialized to 0 */
		*truebaudp = ROUND_SHIFT(clk_speed, 4);
	}
	else 
	{
		*truebaudp = ROUND_DIV(clk_speed, mant << (4 + power2));
		/* divisor is not zero because mant is [1, 64] */
		mant--; /* now [0, 63] */
		
		/*
		 * Encode the N and M values into the bgr and ccr2 registers.
		 */
		
		tmp = ((unsigned short) mant) | ((unsigned short) power2 << 6);
		
		ccr2tmp = SAB82532_CCR2_BDF;
		if ((tmp & 0x200) != 0) 
		{
			ccr2tmp |= SAB82532_CCR2_BR9;
		}
		if ((tmp & 0x100) != 0) 
		{
			ccr2tmp |= SAB82532_CCR2_BR8;
		}
		
		*ccr2 = ccr2tmp | (*ccr2 & ~(SAB82532_CCR2_BDF|SAB82532_CCR2_BR8|SAB82532_CCR2_BR9));
		*bgr = (unsigned char) tmp;
	}
	
	return TRUE;
}

/*
 * Calculate the standard mode baud divisor using an integral algorithm.
 */
/***************************************************************************
 * sab_baudstd:      Function to compute the "standard " baudrate.
 *                
 *
 *     Parameters   : 
 *                  encbaud  2* the baudrate. We use the
 *                           double value so as to support 134.5 (in only)
 *                  clkspeed The board clock speed in Hz.
 *                  bgr      Value of reg BGR for baudrate(output)
 *                  ccr2     Value of reg CCR2 for baudrate (output)
 *                  ccr4     Value of reg CCR4 for baudrate (output)
 *                  truebaud The actual baudrate achieved (output).
 *
 *
 *     Return value : Return FALSE the parameters could not be computed, 
 *
 *     Prerequisite : The various ports must have been initialized
 *
 *     Remark       : Stolen from the Aurora ase driver.
 *
 *     Author       : fw
 *
 *     Revision     : Oct 9 2000, creation
 ***************************************************************************/
static unsigned int
sab8253x_baudstd(unsigned long encbaud, unsigned long clk_speed,
		 unsigned char *bgr, unsigned char *ccr2,
		 unsigned long *truebaudp)
{
  register unsigned short	 quot;
  register unsigned char	 ccr2tmp;
  
  if (encbaud == 0) 
  {
	  return FALSE;
  }
  
  /*
   * This divisor algorithm is a little strange.  The
   *  divisors are all multiples of 2, except for the
   *  magic value of 1.
   *
   * What we do is do most of the algorithm for multiples
   *  of 1, and then switch at the last minute to multiples
   *  of 2.
   */
  
  /*
   * Will we lose any information by left shifting encbaud?
   *  If so, then right shift clk_speed instead.
   */
  if (!HIZERO(encbaud, 3)) 
  {
	  quot = (unsigned short) ROUND_DIV(ROUND_SHIFT(clk_speed, 3),
					    encbaud);
	  /* quot = (clk_speed / 8) / (baud * 2) = clk_speed / (16 * baud) */
  }
  else 
  {
	  /* encbaud isn't a multiple of 2^29 (baud not mult. of 2^28) */
	  quot = (unsigned short) ROUND_DIV(clk_speed, encbaud << 3);
  }
  
  /* quot = clk_speed / (baud * 16) */
  if (quot < 2) 
  {
	  /* bgr and ccr2 should be initialized to 0 */
	  *truebaudp = ROUND_SHIFT(clk_speed, 4);
	  return TRUE;
  }
  
  /*
   * Divide the quotient by two.
   */
  quot = ROUND_SHIFT(quot, 1);
  
  if (quot <= 0x400) 
  {
	  /* quot = [1, 0x400]  -> (quot << 5) != 0 */
	  *truebaudp = ROUND_DIV(clk_speed, ((unsigned long) quot << 5));
	  quot--;
	  
	  ccr2tmp = SAB82532_CCR2_BDF;
	  if ((quot & 0x200) != 0) 
	  {
		  ccr2tmp |= SAB82532_CCR2_BR9;
	  }
	  if ((quot & 0x100) != 0) 
	  {
		  ccr2tmp |=SAB82532_CCR2_BR8;
	  }
	  
	  *ccr2 = ccr2tmp | (*ccr2 & ~(SAB82532_CCR2_BDF|SAB82532_CCR2_BR8|SAB82532_CCR2_BR9));
	  *bgr = (unsigned char) quot;
  }
  else 
  {			/* the baud rate is too small. */
	  return FALSE;
  }
  
  return TRUE;
}

/***************************************************************************
 * sab_baud:      Function to compute the best register value to achieve
 *                a given baudrate.
 *                
 *
 *     Parameters   : 
 *                  port:    The port being used  (in only)
 *                  encbaud: 2* the baudrate. We use the
 *                           double value so as to support 134.5 (in only)
 *                  bgr      Value of reg BGR for baudrate(output)
 *                  ccr2     Value of reg CCR2 for baudrate (output)
 *                  ccr4     Value of reg CCR4 for baudrate (output)
 *                  truebaud The actual baudrate achieved (output).
 *
 *
 *     Return value : Return TRUE if the vaudrate can be set, FALSE otherwise 
 *
 *     Prerequisite : The various ports must have been initialized
 *
 *     Remark       : Stolen from the Aurora ase driver.
 *
 *     Author       : fw
 *
 *     Revision     : Oct 9 2000, creation
 ***************************************************************************/
unsigned int 
sab8253x_baud(sab_port_t *port, unsigned long encbaud,
	      unsigned char *bgr, unsigned char *ccr2,
	      unsigned char *ccr4, unsigned long *truebaudp)
{
	unsigned char	 bgr_std, bgr_enh, ccr2_std, ccr2_enh, ccr4_enh;
	unsigned int		 ok_std, ok_enh;
	unsigned long	 truebaud_std, truebaud_enh, truebaud,clkspeed;
	
	bgr_std = bgr_enh = 0;
	ccr2_std = ccr2_enh = 0;
	ccr4_enh = 0;
	
	/*
	 * the port/chip/board structure will tell us:
	 *  1) clock speed
	 *  2) chip revision (to figure out if the enhanced method is
	 *     available.
	 */
	
	clkspeed = port->chip->c_cim ? port->chip->c_cim->ci_clkspeed :  port->board->b_clkspeed;
	
#ifdef NODEBUGGING
	printk("With clk speed %ld, baud rate = %ld\n",clkspeed, encbaud);
#endif
	
	ok_std = sab8253x_baudstd(encbaud, clkspeed, &bgr_std,
				  &ccr2_std, &truebaud_std);
#ifdef NODEBUGGING
	printk("Std gives bgr = 0x%x, ccr2=0x%x for speed %ld\n",bgr_std,ccr2_std,truebaud_std);
#endif
	if(port->chip->c_revision >= SAB82532_VSTR_VN_3_2) 
	{
		ok_enh = sab8253x_baudenh(encbaud, clkspeed,
					  &bgr_enh, &ccr2_enh, &truebaud_enh);
#ifdef NODEBUGGING
		printk("Enh gives bgr = 0x%x, ccr2=0x%x for speed %ld\n",bgr_enh,ccr2_enh,truebaud_enh);
#endif
	} 
	else 
		ok_enh = FALSE;
	
	/*
	 * Did both methods return values?
	 */
	if (ok_std && ok_enh) 
	{
		/*
		 * Find the closest of the two.
		 */
		if (ABSDIF((truebaud_enh<<1), encbaud) <
		    ABSDIF((truebaud_std<<1), encbaud)) 
		{
			ok_std = FALSE;
		}
		else 
		{
			ok_enh = FALSE;
		}
	}
	
	/*
	 * Now return the values.
	 */
	
	if (ok_std || ok_enh) 
	{
		truebaud = ok_std ? truebaud_std : truebaud_enh;
		
		/*
		 * If the true baud rate is off by more than 5%, then
		 *  we don't support it.
		 */
		if (ROUND_DIV(ABSDIF((truebaud<<1), encbaud), encbaud) != 0) 
		{
			/*
			 * We're not even in the right ballpark.  This
			 *  test is here to deal with overflow conditions.
			 */
			return FALSE;
		}
		else if (ROUND_DIV(ABSDIF((truebaud<<1), encbaud) * 100,
				   encbaud) >= 5) 
		{
			return FALSE;
		}
		
		*truebaudp = truebaud;
		
		if (ok_enh) 
		{
			*ccr4 |= SAB82532_CCR4_EBRG;
			*ccr2 = ccr2_enh;
			*bgr = bgr_enh;
#ifdef DEBUGGING
			printk("Enhanced Baud at %ld, ccr4 = 0x%x, ccr2 = 9x%x, bgr = 0x%x\n",
			       truebaud,*ccr4,*ccr2,*bgr);
#endif
		} 
		else 
		{
			*ccr4 &= ~SAB82532_CCR4_EBRG;
			*ccr2 = ccr2_std;
			*bgr = bgr_std;
#ifdef DEBUGGING
			printk("Standard Baud at %ld, ccr4 = 0x%x, ccr2 = 9x%x, bgr = 0x%x\n",
			       truebaud,*ccr4,*ccr2,*bgr);
#endif
		}
		
		return TRUE;
	}
	else 
	{
		return FALSE;
	}
}

int Sab8253xCountTransmit(SAB_PORT *port)
{
	register RING_DESCRIPTOR *rd;
	register int total;
	register int count;
	unsigned long flags;
	RING_DESCRIPTOR *start;
	
	if(port->sabnext2.transmit == NULL)
	{
		return 0;
	}
	
	save_flags(flags);
	cli();
	rd = port->sabnext2.transmit;
	start = rd;
	total = 0;
	while(1)
	{
		count = rd->Count;
		if((count & OWNER) == OWN_DRIVER)
		{
			break;
		}
		total += (count & ~OWNER);
		if(rd->sendcrc)
		{
			total += (4 - rd->crcindex);
		}
		rd = rd->VNext;
		if(rd == start)
		{
			break;
		}
	}
	restore_flags(flags);
	return total;
}

int Sab8253xCountTransmitDescriptors(SAB_PORT *port)
{
	register RING_DESCRIPTOR *rd;
	register int total;
	register int count;
	unsigned long flags;
	RING_DESCRIPTOR *start;
	
	if(port->sabnext2.transmit == NULL)
	{
		return 0;
	}
	
	save_flags(flags);
	cli();
	rd = port->sabnext2.transmit;
	start = rd;
	total = 0;
	while(1)
	{
		count = rd->Count;
		if((count & OWNER) == OWN_DRIVER)
		{
			break;
		}
		++total;
		rd = rd->VNext;
		if(rd == start)
		{
			break;
		}
	}
	restore_flags(flags);
	return total;
}

int getccr0configS(struct sab_port *port)
{
	return port->ccontrol.ccr0;
}

int getccr1configS(struct sab_port *port)
{
	return port->ccontrol.ccr1;
}

int getccr2configS(struct sab_port *port)
{
	return port->ccontrol.ccr2;
}

int getccr3configS(struct sab_port *port)
{
	return port->ccontrol.ccr3;
}

int getccr4configS(struct sab_port *port)
{
	return port->ccontrol.ccr4;
}

int getrlcrconfigS(struct sab_port *port)
{
	return port->ccontrol.rlcr;
}

int getmodeS(struct sab_port *port)
{
	return port->ccontrol.mode;
}

void sab8253x_init_lineS(struct sab_port *port)
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
#if 0
	sab8253x_tec_wait(port);	/* I have to think about this one
					 * should I assume the line was
					 * previously in async mode*/
#endif
	
	/*
	 * Clear the FIFO buffers.
	 */
	
	WRITEB(port, cmdr, SAB82532_CMDR_RHR);
	sab8253x_cec_wait(port);
	WRITEB(port,cmdr,SAB82532_CMDR_XRES);
	
	
	/*
	 * Clear the interrupt registers.
	 */
	stat = READB(port, isr0);	/* acks ints */
	stat = READB(port, isr1);
	
	/*
	 * Now, initialize the UART 
	 */
	WRITEB(port, ccr0, 0);	  /* power-down */
	WRITEB(port, ccr0, getccr0configS(port));
	WRITEB(port, ccr1, getccr1configS(port));
	WRITEB(port, ccr2, getccr2configS(port));
	WRITEB(port, ccr3, getccr3configS(port));
	WRITEB(port, ccr4, getccr4configS(port));	/* 32 byte receive fifo */
	WRITEB(port, mode, getmodeS(port));
	WRITEB(port, tic /* really rlcr */, getrlcrconfigS(port));
	/* power-up */
	
	switch(port->ccontrol.ccr4 & SAB82532_CCR4_RF02)
	{
	case SAB82532_CCR4_RF32:
		port->recv_fifo_size = 32;
		break;
	case SAB82532_CCR4_RF16:
		port->recv_fifo_size = 16;
		break;
	case SAB82532_CCR4_RF04:
		port->recv_fifo_size = 4;
		break;
	case SAB82532_CCR4_RF02:
		port->recv_fifo_size = 2;
		break;
	default:
		port->recv_fifo_size = 32;
		port->ccontrol.ccr4 &= ~SAB82532_CCR4_RF02;
		break;
	}
	
	if(port->ccontrol.ccr2 & SAB82532_CCR2_TOE)
	{
		RAISE(port, txclkdir);
	}
	else
	{
		LOWER(port, txclkdir);
	}
	
	SET_REG_BIT(port,ccr0,SAB82532_CCR0_PU);

	if(port->chip->c_cim)
	{
		if(port->chip->c_cim->ci_type == CIM_SP502)
		{
			aura_sp502_program(port, port->sigmode);
		}
	}
}

/* frees up all skbuffs currently */
/* held by driver */
void Sab8253xFreeAllFreeListSKBUFFS(SAB_PORT* priv) /* empty the skbuffer list */
/* either on failed open */
/* or on close*/
{
	struct sk_buff* skb;
	
	if(priv->sab8253xbuflist == NULL)
	{
		return;
	}
	
	DEBUGPRINT((KERN_ALERT "sab8253x: freeing %i skbuffs.\n", 
		    skb_queue_len(priv->sab8253xbuflist)));
	
	while(skb_queue_len(priv->sab8253xbuflist) > 0)
	{
		skb = skb_dequeue(priv->sab8253xbuflist);
		dev_kfree_skb_any(skb);
	}
	kfree(priv->sab8253xbuflist);
	priv->sab8253xbuflist = NULL;
}

int Sab8253xSetUpLists(SAB_PORT *priv)
{
	if(priv->sab8253xbuflist)
	{
		if(priv->sab8253xc_rcvbuflist)
		{
			return 0;
		}
		else
		{
			return -1;
		}
		return 0;
	}
	else if(priv->sab8253xc_rcvbuflist)
	{
		return -1;
	}
	
	priv->sab8253xbuflist = (struct sk_buff_head*) kmalloc(sizeof(struct sk_buff_head), GFP_KERNEL);
	if(priv->sab8253xbuflist == NULL)
	{
		return -1;
	}
	priv->sab8253xc_rcvbuflist = (struct sk_buff_head*) kmalloc(sizeof(struct sk_buff_head), GFP_KERNEL);  
	if(priv->sab8253xc_rcvbuflist == NULL)
	{
		kfree(priv->sab8253xbuflist);
		return -1;
	}
	skb_queue_head_init(priv->sab8253xbuflist);
	skb_queue_head_init(priv->sab8253xc_rcvbuflist);
	return 0;
}

/* sets up transmit ring and one receive sk_buff */

/* set up transmit and receive
   sk_buff control structures */
int Sab8253xInitDescriptors2(SAB_PORT *priv, int listsize, int rbufsize)
{
	RING_DESCRIPTOR *desc;
	RING_DESCRIPTOR *xdesc;
	
	if(priv->dcontrol2.transmit != NULL)
		
	{
		if(priv->dcontrol2.receive != NULL)
		{
			return 0;
		}
		return -1;
	}
	else if(priv->dcontrol2.receive != NULL)
	{
		return -1;
	}
	
	priv->dcontrol2.transmit = (RING_DESCRIPTOR*) 
		kmalloc(sizeof(RING_DESCRIPTOR) * listsize, GFP_KERNEL);
	/* dcontrol2 is an historical
	   artifact from when the code
	   talked to an intelligent controller */
	if(priv->dcontrol2.transmit == NULL)
	{
		return -1;
	}
	
	priv->dcontrol2.receive = (RING_DESCRIPTOR*)
		kmalloc(sizeof(RING_DESCRIPTOR), GFP_KERNEL); /* only one receive sk_buffer */
	if(priv->dcontrol2.receive == NULL)
	{
		kfree(priv->dcontrol2.transmit);
		priv->dcontrol2.transmit = NULL;
		return -1;
	}
	
	for(xdesc = priv->dcontrol2.transmit; 
	    xdesc < &priv->dcontrol2.transmit[listsize - 1];
	    xdesc = &xdesc[1])	/* set up transmit descriptors */
	{
		xdesc->HostVaddr = NULL;
		xdesc->VNext = &xdesc[1];
		xdesc->Count = 0 | OWN_DRIVER;
		xdesc->crc = 0;
		xdesc->sendcrc = 0;
		xdesc->crcindex = 0;
	}
	xdesc->HostVaddr = NULL;
	xdesc->VNext = priv->dcontrol2.transmit; /* circular list */
	xdesc->Count = 0 | OWN_DRIVER;
	xdesc->crc = 0;
	xdesc->sendcrc = 0;
	xdesc->crcindex = 0;
	
	desc = priv->dcontrol2.receive; /* only need one descriptor for receive */
	desc->HostVaddr = NULL;
	desc->VNext = &desc[0];
	
	desc = priv->dcontrol2.receive;
	desc->HostVaddr = dev_alloc_skb(rbufsize);
	if(desc->HostVaddr == NULL)
	{
		printk(KERN_ALERT "Unable to allocate skb_buffers (rx 0).\n");
		printk(KERN_ALERT "Driver initialization failed.\n");
		kfree(priv->dcontrol2.transmit);
		kfree(priv->dcontrol2.receive);
		priv->dcontrol2.transmit = NULL; /* get rid of descriptor ring */
		priv->dcontrol2.receive = NULL; /* get rid of descriptor */
		/* probably should do some deallocation of sk_buffs*/
		/* but will take place in the open */
		return -1;
	}
	skb_queue_head(priv->sab8253xbuflist, (struct sk_buff*) desc->HostVaddr);
	desc->Count = rbufsize|OWN_SAB;	/* belongs to int handler */
	desc->crc = 0;
	desc->sendcrc = 0;
	desc->crcindex = 0;
	
	/* setup the various pointers */
	priv->active2 = priv->dcontrol2; /* insert new skbuff */
	priv->sabnext2 = priv->dcontrol2; /* transmit from here */
	
	return 0;
}

/* loads program, waits for PPC */
/* and completes initialization*/

void Sab8253xCleanUpTransceiveN(SAB_PORT* priv)
{
	Sab8253xFreeAllFreeListSKBUFFS(priv);	
	Sab8253xFreeAllReceiveListSKBUFFS(priv);
	
	/* these are also cleaned up in the module cleanup routine */
	/* should probably only be done here */
	if(priv->dcontrol2.receive)
	{
		kfree(priv->dcontrol2.receive);
		priv->dcontrol2.receive = NULL;
	}
	if(priv->dcontrol2.transmit)
	{
		kfree(priv->dcontrol2.transmit);
		priv->dcontrol2.transmit = NULL;
	}
	priv->active2 = priv->dcontrol2;
	priv->sabnext2 = priv->dcontrol2; 
}

void Sab8253xFreeAllReceiveListSKBUFFS(SAB_PORT* priv) /* empty the skbuffer list */
/* either on failed open */
/* or on close*/
{
	struct sk_buff* skb;
	
	if(priv->sab8253xc_rcvbuflist == NULL)
	{
		return;
	}
	
	DEBUGPRINT((KERN_ALERT "sab8253x: freeing %i skbuffs.\n", 
		    skb_queue_len(priv->sab8253xc_rcvbuflist)));
	
	while(skb_queue_len(priv->sab8253xc_rcvbuflist) > 0)
	{
		skb = skb_dequeue(priv->sab8253xc_rcvbuflist);
		dev_kfree_skb_any(skb);
	}
	kfree(priv->sab8253xc_rcvbuflist);
	priv->sab8253xc_rcvbuflist = NULL;
}

/*
 * This routine is called to set the UART divisor registers to match
 * the specified baud rate for a serial port.
 */

void sab8253x_change_speedN(struct sab_port *port)
{
	unsigned long	flags;
	unsigned char ccr2=0, ccr4=0, ebrg=0;
	int bits = 8;
	
#ifdef DEBUGGING
	printk("Change speed!  ");
#endif
	
	if(!sab8253x_baud(port, (port->baud)*2, &ebrg, &ccr2, &ccr4, &(port->baud))) 
	{
		printk("Aurora Warning. baudrate %ld could not be set! Using 115200", port->baud);
		port->baud = 115200;
		sab8253x_baud(port, (port->baud*2), &ebrg, &ccr2, &ccr4, &(port->baud));
	}
	
	if (port->baud)
	{
		port->timeout = (port->xmit_fifo_size * HZ * bits) / port->baud;
		port->cec_timeout = port->tec_timeout >> 2;
	}
	else
	{
		port->timeout = 0;
		port->cec_timeout = SAB8253X_MAX_CEC_DELAY;
	}
	port->timeout += HZ / 50;		/* Add .02 seconds of slop */
	
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

void sab8253x_shutdownN(struct sab_port *port)
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
	
	/* Disable Interrupts */
	
	port->interrupt_mask0 = 0xff;
	WRITEB(port, imr0, port->interrupt_mask0);
	port->interrupt_mask1 = 0xff;
	WRITEB(port, imr1, port->interrupt_mask1);
	
	LOWER(port,rts);
	LOWER(port,dtr);
	
	/* Disable Receiver */	
	CLEAR_REG_BIT(port,mode,SAB82532_MODE_RAC);
	
	/* Power Down */	
	CLEAR_REG_BIT(port,ccr0,SAB82532_CCR0_PU);
	
	port->flags &= ~FLAG8253X_INITIALIZED;
	restore_flags(flags);
}

int sab8253x_block_til_ready(struct tty_struct *tty, struct file * filp,
			     struct sab_port *port)
{
	DECLARE_WAITQUEUE(wait, current);
	int retval;
	int do_clocal = 0;
	unsigned long flags;
	
	/*
	 * If the device is in the middle of being closed, then block
	 * until it's done, and then try again.
	 */
	if (tty_hung_up_p(filp) ||
	    (port->flags & FLAG8253X_CLOSING)) 
	{
		if (port->flags & FLAG8253X_CLOSING)
		{
			interruptible_sleep_on(&port->close_wait); /* finish up previous close */
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
	 * If this is a callout device, then just make sure the normal
	 * device isn't being used.
	 */
	if (tty->driver.subtype == SERIAL_TYPE_CALLOUT) 
	{
		if (port->flags & FLAG8253X_NORMAL_ACTIVE) 
		{
			return -EBUSY;	/* async, sync tty or network driver active */
		}
		if ((port->flags & FLAG8253X_CALLOUT_ACTIVE) &&
		    (port->flags & FLAG8253X_SESSION_LOCKOUT) &&
		    (port->session != current->session))
		{
			return -EBUSY;
		}
		if ((port->flags & FLAG8253X_CALLOUT_ACTIVE) &&
		    (port->flags & FLAG8253X_PGRP_LOCKOUT) &&
		    (port->pgrp != current->pgrp))
		{
			return -EBUSY;
		}
		port->flags |= FLAG8253X_CALLOUT_ACTIVE; /* doing a callout */
		return 0;
	}
	
	/* sort out async vs sync tty, not call out */
	/*
	 * If non-blocking mode is set, or the port is not enabled,
	 * then make the check up front and then exit.
	 */
	
	if ((filp->f_flags & O_NONBLOCK) ||
	    (tty->flags & (1 << TTY_IO_ERROR))) 
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
	else if (tty->termios->c_cflag & CLOCAL)
	{
		do_clocal = 1;
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
	while (1) 
	{
		save_flags(flags);
		cli();
		if (!(port->flags & FLAG8253X_CALLOUT_ACTIVE) &&
		    (tty->termios->c_cflag & CBAUD)) 
		{
			RAISE(port, dtr);
			RAISE(port, rts);	/* maybe not correct for sync */
			/*
			 * ??? Why changing the mode here? 
			 *  port->regs->rw.mode |= SAB82532_MODE_FRTS;
			 *  port->regs->rw.mode &= ~(SAB82532_MODE_RTS);
			 */
		}
		restore_flags(flags);
		current->state = TASK_INTERRUPTIBLE;
		if (tty_hung_up_p(filp) ||
		    !(port->flags & FLAG8253X_INITIALIZED)) 
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
		printk("sab8253x_block_til_ready blocking: ttyS%d, count = %d, flags = %x, clocal = %d, vstr = %02x\n",
		       port->line, port->count, port->flags, do_clocal, READB(port,vstr));
#endif
		schedule();
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&port->open_wait, &wait);
	port->blocked_open--;
#ifdef DEBUG_OPEN
	printk("sab8253x_block_til_ready after blocking: ttys%d, count = %d\n",
	       port->line, port->count);
#endif
	if (retval)
	{
		return retval;
	}
	port->flags |= FLAG8253X_NORMAL_ACTIVE;
	return 0;
}

/*
 * sab8253x_wait_until_sent() --- wait until the transmitter is empty
 */
void sab8253x_wait_until_sent(struct tty_struct *tty, int timeout)
{
	struct sab_port *port = (struct sab_port *)tty->driver_data;
	unsigned long orig_jiffies, char_time;
	
	if (sab8253x_serial_paranoia_check(port,tty->device,"sab8253x_wait_until_sent"))
	{
		return;
	}
	
	orig_jiffies = jiffies;
	/*
	 * Set the check interval to be 1/5 of the estimated time to
	 * send a single character, and make it at least 1.  The check
	 * interval should also be less than the timeout.
	 * 
	 * Note: we have to use pretty tight timings here to satisfy
	 * the NIST-PCTS.
	 */
	char_time = (port->timeout - HZ/50) / port->xmit_fifo_size;
	char_time = char_time / 5;
	if (char_time == 0)
	{
		char_time = 1;
	}
	if (timeout)
	{
		char_time = MIN(char_time, timeout);
	}
	while ((Sab8253xCountTransmit(port) > 0) || !port->all_sent) 
	{
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(char_time);
		if (signal_pending(current))
		{
			break;
		}
		if (timeout && time_after(jiffies, orig_jiffies + timeout))
		{
			break;
		}
	}
}

void sab8253x_flush_buffer(struct tty_struct *tty)
{
	struct sab_port *port = (struct sab_port *)tty->driver_data;
	unsigned long flags;
	register RING_DESCRIPTOR *freeme;
	
	if(sab8253x_serial_paranoia_check(port, tty->device, "sab8253x_flush_buffer"))
	{
		return;
	}
	
	if(port->sabnext2.transmit == NULL)
	{
		return;
	}
	
	save_flags(flags); 
	cli();			/* need to turn off ints because mucking
				   with sabnext2 */
#ifndef FREEININTERRUPT
	freeme = port->active2.transmit;
	do				/* just go all around */
	{
		if(freeme->HostVaddr)
		{
			skb_unlink((struct sk_buff*)freeme->HostVaddr);
			dev_kfree_skb_any((struct sk_buff*)freeme->HostVaddr);
			freeme->HostVaddr = NULL;
		}
		freeme->sendcrc = 0;
		freeme->crcindex = 0;
		freeme->Count = OWN_DRIVER;
		freeme = (RING_DESCRIPTOR*) freeme->VNext;
	}
	while(freeme != port->active2.transmit);
#else  /* buffers only from sabnext2.transmit to active2.transmit */
	while((port->sabnext2.transmit->Count & OWNER) == OWN_SAB) /* clear out stuff waiting to be transmitted */
	{
		freeme = port->sabnext2.transmit;
		if(freeme->HostVaddr)
		{
			skb_unlink((struct sk_buff*)freeme->HostVaddr);
			dev_kfree_skb_any((struct sk_buff*)freeme->HostVaddr);
			freeme->HostVaddr = NULL;
		}
		freeme->sendcrc = 0;
		freeme->crcindex = 0;
		freeme->Count = OWN_DRIVER;
		port->sabnext2.transmit = freeme->VNext;
	}
#endif
	port->sabnext2.transmit = port->active2.transmit; /* should already be equal to be sure */
	sab8253x_cec_wait(port);
	WRITEB(port,cmdr,SAB82532_CMDR_XRES);
	restore_flags(flags);
	
	wake_up_interruptible(&tty->write_wait); /* wake up tty driver */
	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
	    tty->ldisc.write_wakeup)
	{
		(*tty->ldisc.write_wakeup)(tty);
	}
}

