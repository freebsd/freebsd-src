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

/* Standard in kernel modules */
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
#include "8253xmcs.h"

/*
 * ----------------------------------------------------------------------
 *
 * Here starts the interrupt handling routines.  All of the following
 * subroutines are declared as inline and are folded into
 * sab8253x_interrupt().  They were separated out for readability's sake.
 *
 * Note: sab8253x_interrupt() is a "fast" interrupt, which means that it
 * runs with interrupts turned off.  People who may want to modify
 * sab8253x_interrupt() should try to keep the interrupt handler as fast as
 * possible.  After you are done making modifications, it is not a bad
 * idea to do:
 * 
 * gcc -S -DKERNEL -Wall -Wstrict-prototypes -O6 -fomit-frame-pointer serial.c
 *
 * and look at the resulting assemble code in serial.s.
 *
 * 				- Ted Ts'o (tytso@mit.edu), 7-Mar-93
 * -----------------------------------------------------------------------
 */

/* Note:  the inline interrupt routines constitute the smallest hardware
   unit that must be examined when an interrupt comes up.  On the 4520
   type cards, two ESCC2s must be examined.  Because the ESCC2s are in
   a null terminated list the sab82532_interrupt also works for 2 port/1 port
   single ESCC2 cards.
   
   On an 8520 type card there is but one ESCC8 thus the sab82538_interrupt
   routine does not walk through a list.  But this requires some contortion
   in dealing with the multichannel server.  The multichannel server has
   at most 4 channel interface modules (CIM) 1/EB.  Each CIM has at most
   two ESCC8s, thus the host card can have a list of 8 ESCC8s.  But by
   walking the CIMs the exact ESCC8 that is interrupting can be identified.
   Thus despite the complexity, really the MCS is a collection of 8520 type
   cards multiplexed on one interrupt.  Thus after making some temporary
   modifications of the board structure, the generic interrupt handler invokes
   sab82538_interrupt handler just as for an 8520 type card.
*/

/* static forces inline compilation */
static void inline sab82532_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct sab_port *port;
	struct sab_chip *chip=NULL;
	struct sab_board *bptr = (struct sab_board*) dev_id;
	union sab8253x_irq_status status;
	unsigned char gis;
	
	for(chip = bptr->board_chipbase; chip != NULL; chip = chip->next_by_board) 
	{
		port= chip->c_portbase;
		gis = READB(port, gis); /* Global! */
		status.stat=0;
		
		/* Since the PORT interrupt are global, 
		 * we do check all the ports for this chip
		 */
		
		/* A 2 ports chip */
		
		if(!(gis & SAB82532_GIS_MASK)) 
		{
			continue; /* no interrupt on this chip */
		}
		
		if (gis & SAB82532_GIS_ISA0)
		{
			status.sreg.isr0 = READB(port, isr0);
		}
		else 
		{
			status.sreg.isr0 = 0;
		}
		if (gis & SAB82532_GIS_ISA1)
		{
			status.sreg.isr1 = READB(port, isr1);
		}
		else
		{
			status.sreg.isr1 = 0;
		}
		
		if (gis & SAB82532_GIS_PI)
		{
			status.sreg.pis = READB(port, pis);
		}
		else
		{
			status.sreg.pis = 0;
		}
		
		if (status.stat) 
		{
			if (status.images[ISR0_IDX] & port->receive_test)
			{
				(*port->receive_chars)(port, &status);	/* when the fifo is full */
				/* no time to schedule thread*/
			}
			
			if ((status.images[port->dcd.irq] & port->dcd.irqmask) || 
			    (status.images[port->cts.irq] & port->cts.irqmask) ||
			    (status.images[port->dsr.irq] & port->dsr.irqmask) ||
			    (status.images[ISR1_IDX] & port->check_status_test))
			{
				(*port->check_status)(port, &status); /* this stuff should be */
				/* be moveable to scheduler */
				/* thread*/
			}
			
			if (status.images[ISR1_IDX] & port->transmit_test)
			{
				(*port->transmit_chars)(port, &status); /* needs to be moved to task */
			}
		}
		
		/* Get to next port on chip */
		port = port->next_by_chip;
		/* Port B */
		if (gis & SAB82532_GIS_ISB0)
		{
			status.images[ISR0_IDX] = READB(port, isr0);
		}
		else 
		{
			status.images[ISR0_IDX] = 0;
		}
		if (gis & SAB82532_GIS_ISB1)
		{
			status.images[ISR1_IDX] = READB(port,isr1);
		}
		else
		{
			status.images[ISR1_IDX] = 0;
		}
		/* DO NOT SET PIS. IT was reset! */
		
		
		if (status.stat) 
		{
			if (status.images[ISR0_IDX] & port->receive_test)
			{
				(*port->receive_chars)(port, &status);
			}
			if ((status.images[port->dcd.irq] & port->dcd.irqmask) || 
			    (status.images[port->cts.irq] & port->cts.irqmask) ||
			    (status.images[port->dsr.irq] & port->dsr.irqmask) ||
			    (status.images[ISR1_IDX] & port->check_status_test))
			{
				(*port->check_status)(port, &status);
			}
			if (status.images[ISR1_IDX] & port->transmit_test)
			{
				(*port->transmit_chars)(port, &status);
			}
		}
	}
}

static void inline sab82538_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct sab_port *port;
	struct sab_chip *chip=NULL;
	struct sab_board *bptr = (struct sab_board*) dev_id;
	union sab8253x_irq_status status;
	unsigned char gis,i;
	
	chip = bptr->board_chipbase;
	port = chip->c_portbase;
	
	gis = READB(port, gis); /* Global! */
	status.stat=0;
	
	/* Since the PORT interrupt are global, 
	 * we do check all the ports for this chip
	 */
	
	/* 8 ports chip */
	if(!(gis & SAB82538_GIS_MASK)) 
	{
		return;
	}
	
	if(gis & SAB82538_GIS_CII) 
	{ /* A port interrupt! */
		/* Get the port */
		int portindex;
		
		portindex = (gis & SAB82538_GIS_CHNL_MASK);
		
		port = chip->c_portbase;
		
		while(portindex)
		{
			port = port->next_by_chip;
			--portindex;
		}
		
		status.images[ISR0_IDX] = READB(port,isr0);
		status.images[ISR1_IDX] = READB(port,isr1);
		if (gis & SAB82538_GIS_PIC)
		{
			status.images[PIS_IDX] = 
				(*port->readbyte)(port,
						  ((unsigned char *)(port->regs)) +
						  SAB82538_REG_PIS_C);
		}
		else
		{
			status.images[PIS_IDX] = 0;
		}
		
		if (status.stat) 
		{
			if (status.images[ISR0_IDX] & port->receive_test)
			{
				(*port->receive_chars)(port, &status);
			}
			if ((status.images[port->dcd.irq] & port->dcd.irqmask) ||
			    (status.images[port->cts.irq] & port->cts.irqmask) ||
			    (status.images[port->dsr.irq] & port->dsr.irqmask) ||
			    (status.images[ISR1_IDX] & port->check_status_test))
			{
				(*port->check_status)(port, &status);
			}
			/*
			 * We know that with 8 ports chip, the bit corresponding to channel 
			 * number is used in the parallel port... So we clear it
			 * Not too elegant!
			 */
			status.images[PIS_IDX] &= ~(1 << (gis&SAB82538_GIS_CHNL_MASK));
			if (status.images[ISR1_IDX] & port->transmit_test)
			{
				(*port->transmit_chars)(port, &status);
			}
		}
	}
	
	/* 
	 * Now we handle the "channel interrupt" case. The chip manual for the
	 * 8 ports chip states that "channel" and "port" interrupt are set
	 * independently so we still must check the parrallel port
	 *
	 * We should probably redesign the whole thing to be less AD HOC that we 
	 * are now... We know that port C is used for DSR so we only check that one.
	 * PIS for port C was already recorded in  status.images[PIS_IDX], so we
	 * check the ports that are set
	 */
	
	if (status.images[PIS_IDX]) 
	{
		for(i=0, port = chip->c_portbase;
		    i < chip->c_nports;
		    i++, port=port->next_by_chip) 
		{
			if(status.images[PIS_IDX] & (0x1 << i)) 
			{ /* Match */
				/* Checking DSR */
				if(port->dsr.inverted)
				{
					port->dsr.val = (((*port->readbyte)
							  (port, port->dsr.reg) & 
							  port->dsr.mask) ? 0 : 1);
				}
				else
				{
					port->dsr.val = ((*port->readbyte)(port, port->dsr.reg) & 
							 port->dsr.mask);
				}
				
				port->icount.dsr++;
				wake_up_interruptible(&port->delta_msr_wait); /* in case waiting on modem change */
			}
		}
	}
}

/*
 * This is the serial driver's generic interrupt routine
 */

void sab8253x_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	extern SAB_BOARD *AuraBoardESCC2IrqRoot[];
	extern SAB_BOARD *AuraBoardESCC8IrqRoot[];
	extern SAB_BOARD *AuraBoardMCSIrqRoot[];
	AURA_CIM *cim;
	SAB_CHIP *chip;
	SAB_PORT *port;
	register SAB_BOARD *boardptr;
	register unsigned char intrmask;
	unsigned char stat;
	SAB_CHIP *save_chiplist;
	SAB_PORT *save_portlist;
	
	if((irq < 0) || (irq >= NUMINTS))
	{
		printk(KERN_ALERT "sab8253x: bad interrupt value %i.\n", irq);
		return;
	}
	/* walk through all the cards on the interrupt that occurred. */
	for(boardptr = AuraBoardESCC2IrqRoot[irq]; boardptr != NULL; boardptr = boardptr->next_on_interrupt)
	{
		sab82532_interrupt(irq, boardptr, regs);
	}
	
	for(boardptr = AuraBoardESCC8IrqRoot[irq]; boardptr != NULL; boardptr = boardptr->next_on_interrupt)
	{
		sab82538_interrupt(irq, boardptr, regs);
	}  
	
	for(boardptr = AuraBoardMCSIrqRoot[irq]; boardptr != NULL; boardptr = boardptr->next_on_interrupt)
	{
		
		while(1)
		{
			writeb(0, (unsigned char*)(boardptr->CIMCMD_REG + CIMCMD_WRINTDIS)); /* prevent EBs from raising
											      * any more ints through the
											      * host card */
			stat = ~(unsigned char) /* active low !!!!! */
				readw((unsigned short*) 
				      (((unsigned char*)boardptr->CIMCMD_REG) + CIMCMD_RDINT)); /* read out the ints */
				/* write to the MIC csr to reset the PCI interrupt */
			writeb(0, (unsigned char*)(boardptr->MICCMD_REG + MICCMD_MICCSR)); 
				/* reset the interrupt generation
				 * hardware on the host card*/
			/* now, write to the CIM interrupt ena to re-enable interrupt generation */
			writeb(0, (unsigned char*)(boardptr->CIMCMD_REG + CIMCMD_WRINTENA)); /* allow EBs to request ints
											      * through the host card */
			if(!stat)
			{
				break;
			}
			cim = boardptr->b_cimbase; /* cims in reverse order */
			for(intrmask = boardptr->b_intrmask;
			    intrmask != 0; 
			    intrmask <<= 2, stat <<=2)
			{
				if(cim == NULL)
				{
					break;	/* no cim no ports */
				}
				if((intrmask & 0xc0) == 0) /* means no cim for these ints */
				{		/* cim not on list do not go to next */
					continue;
				}
				save_portlist = boardptr->board_portbase;
				save_chiplist = boardptr->board_chipbase;
				/* the goal is temporarily to make the structures
				 * look like 8x20 structures -- thus if I find
				 * a bug related to escc8s I need fix it in
				 * only one place. */
				switch(stat & 0xc0) /* possible ints */
				{
				default:
					break;
					
				case 0x80:	/* esccB */
					chip = cim->ci_chipbase;
					if(!chip)
					{
						printk(KERN_ALERT "aura mcs: missing cim.\n");
						break;
					}
					chip = chip->next_by_cim;
					if(!chip)
					{
						printk(KERN_ALERT "aura mcs: missing 2nd cim.\n");
						break;
					}
					port = chip->c_portbase;
					boardptr->board_portbase = port;
					boardptr->board_chipbase = chip;
					sab82538_interrupt(irq, boardptr, regs);		  
					break;
					
				case 0x40:	/* esccA */
					chip = cim->ci_chipbase;
					if(!chip)
					{
						printk(KERN_ALERT "aura mcs: missing cim.\n");
						break;
					}
					port = chip->c_portbase;
					boardptr->board_portbase = port;
					boardptr->board_chipbase = chip;
					sab82538_interrupt(irq, boardptr, regs);		  
					break;
					
				case 0xc0:	/* esccB and esccA */
					chip = cim->ci_chipbase;
					if(!chip)
					{
						printk(KERN_ALERT "aura mcs: missing cim.\n");
						break;
					}
					port = chip->c_portbase;
					boardptr->board_portbase = port;
					boardptr->board_chipbase = chip;
					sab82538_interrupt(irq, boardptr, regs);		  
					
					chip = cim->ci_chipbase;
					if(!chip)
					{
						printk(KERN_ALERT "aura mcs: missing cim.\n");
						break;
					}
					chip = chip->next_by_cim;
					if(!chip)
					{
						printk(KERN_ALERT "aura mcs: missing 2nd cim.\n");
						break;
					}
					port = chip->c_portbase;
					boardptr->board_portbase = port;
					boardptr->board_chipbase = chip;
					sab82538_interrupt(irq, boardptr, regs);		  
					break;
				}
				boardptr->board_portbase = save_portlist;
				boardptr->board_chipbase = save_chiplist;
				cim = cim->next_by_mcs;
			}
		}
	}
}

/*
 * -------------------------------------------------------------------
 * Here ends the serial interrupt routines.
 * -------------------------------------------------------------------
 */

