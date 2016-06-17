/* -*- linux-c -*- */

/* plx9050.c 
 * Copyright (C) 2000 by Francois Wautier
 * based on code from Bjorn Davis
 * 
 * Read and write command for the eprom attached to
 * the PLX9050 
 */

/* Modifications and extensions
 * Copyright (C) 2001 By Joachim Martillo, Telford Tools, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 **/

/* We handle PCI devices */
#include <linux/pci.h>      

/* We need to use ioremap */ 
#include <asm/io.h>

#include <linux/delay.h>

				/* Joachim Martillo modified this file */
				/* so that it had no dependencies on specific */
				/* Aurora adapter card or ESSC* structures*/
				/* The original file use TRUE for 1 and */
				/* FALSE for 0.  This convention conflicted */
				/* with other conventions throughout LINUX */
				/* also TRUE was used for setting an eprom */
				/* bit which is a slight semantic confusion. */
				/* I just used 0 and 1 */
#include "Reg9050.h"

/*
 * Write a single bit to the serial EPROM interface.
 */

				/* eprom_ctl is the */
				/* address of the 9050 */
				/* eprom control register */
				/* The original & operation */
				/* looks wrong.  I am surprised */
				/* the code worked */ 
				/* but I left the parentheses */
				/* because readl, writel etc */
				/* are macros*/

				/* The following function */
				/* assumes the proper bit */
				/* in the serial eprom */
				/* has already been selected*/

				/* The 9050 registers are 32 bits */
				/* hence the readl and writel */
				/* macros are invoked*/

				/* eprom_ctl must be a virtual */
				/* address*/

static void plx9050_eprom_wbit(unsigned int* eprom_ctl, unsigned int val)
{
	unsigned int	 ctrl;
	
	/* get the initial value of the CTRL register */
	ctrl = readl((eprom_ctl));
	
	/* set or clear the data bit */
	if (val) 
	{
		ctrl |= PLX_CTRL_SEPWD;
	}
	else 
	{
		ctrl &= ~PLX_CTRL_SEPWD;
	}
	
	writel(ctrl, (eprom_ctl));
	
	udelay(1);
	
	/* Toggle the clock line */
	/* gets to the next bit */
	/* in the serial eprom */
	ctrl |= PLX_CTRL_SEPCLK;
	writel(ctrl, (eprom_ctl));
	
	udelay(1);
	
	/* Toggle the clock line */
	ctrl &= ~PLX_CTRL_SEPCLK;
	writel(ctrl, (eprom_ctl));
	udelay(1);
}

/*
 * Run a serial EPROM command.  Returns 1 on success,
 *  0 otherwise.
 */

/* This routine does the write of data but only sets up */
/* for a read*/
/* the write goes from most significant to least significant */
unsigned int plx9050_eprom_cmd(unsigned int* eprom_ctl, unsigned char cmd, unsigned char addr, unsigned short data)
{
	unsigned int ctrl;
	unsigned char shiftb;
	unsigned short shiftw;
	unsigned int l, v;
	unsigned char ret;
	int i;
	
	ret = 1;
	shiftb = addr << (NM93_BITS_PER_BYTE - NM93_ADDRBITS); /* looks a bizarre way to mask out unused bits */
	
	ctrl = readl((eprom_ctl));
	
	ctrl &= ~(PLX_CTRL_SEPCLK | PLX_CTRL_SEPWD);
	writel(ctrl, (eprom_ctl));
	udelay(1);
	
	ctrl |= PLX_CTRL_SEPCS;
	writel(ctrl, (eprom_ctl));
	
	plx9050_eprom_wbit(eprom_ctl, 1);
	
	/*
	 * Clock out the command
	 */
	
	plx9050_eprom_wbit(eprom_ctl, (cmd & 0x02) != 0);
	plx9050_eprom_wbit(eprom_ctl, (cmd & 0x01) != 0);
	
	/*
	 * Clock out the address
	 */
	
	i = NM93_ADDRBITS;
	while (i != 0)		/* here we get to the correct */
				/* short in the serial eprom*/
	{
		/* printf("Loop #1\n"); */
		plx9050_eprom_wbit(eprom_ctl, (shiftb & 0x80) != 0);
		
		shiftb <<= 1;
		i--;
	}
	
	if (cmd == NM93_WRITECMD)	/* now do the write if */
		/* a write is to be done*/
	{	
		/* write data? */
		/*
		 * Clock out the data
		 */
		
		shiftw = data;
		
		i = NM93_BITS_PER_WORD;
		while (i != 0) {
			/* printf("Loop #2\n"); */
			plx9050_eprom_wbit(eprom_ctl, (shiftw & 0x8000) != 0);
			
			shiftw <<= 1;
			i--;
		}
		
		/*
		 * De-assert chip select for a short period of time
		 */
		ctrl = readl((eprom_ctl));
		
		ctrl &= ~PLX_CTRL_SEPCS;
		writel(ctrl, (eprom_ctl));
		udelay(2);
		
		/*
		 * Re-assert chip select
		 */
		ctrl |= PLX_CTRL_SEPCS;
		writel(ctrl, (eprom_ctl));
		
		/*
		 * Wait for a low to high transition of DO
		 */
		
		i = 20000;
		ctrl = readl((eprom_ctl));
		l = (ctrl & PLX_CTRL_SEPRD);
		
		while (i != 0) 
		{
			/* printf("Loop #3\n"); */
			ctrl = readl((eprom_ctl));
			v = (ctrl & PLX_CTRL_SEPRD);
			if (v != 0 && l == 0) 
			{
				break;
			}
			l = v;
			udelay(1);
			i--;
		}
		
		if (i == 0) 
		{
			printk("plx9050: eprom didn't go low to high");
			ret = 0;
		}
	}
	
	if (cmd != NM93_READCMD)	/* not a read -- terminate */
	{
		/*
		 * De-assert the chip select.
		 */
		
		ctrl = readl((eprom_ctl));
		ctrl &= ~PLX_CTRL_SEPCS;
		writel(ctrl,(eprom_ctl));
	}
	/* otherwise left in read state */
	return ret;
}

/*
 * Read the serial EPROM.  Returns 1 on success, 0 on failure.
 * reads in shorts (i.e., 16 bits at a time.)
 *
 */

unsigned int
plx9050_eprom_read(unsigned int* eprom_ctl, unsigned short *ptr, unsigned char addr, unsigned short len)
{
	unsigned short shiftw;
	int i;
	unsigned int ctrl;
	
	if (!plx9050_eprom_cmd(eprom_ctl, NM93_READCMD, addr, (unsigned short) 0x0)) /* set up read */
	{
		return 0;
	}
	
	ctrl = readl((eprom_ctl));	/* synchronize */
	
	while (len-- > 0)		/* now read one word at a time */
	{
		shiftw = 0;
		
		ctrl &= ~PLX_CTRL_SEPCLK;
		writel(ctrl, (eprom_ctl));
		
		udelay(1);
		
		i = NM93_BITS_PER_WORD;
		while (1)			/* now read one bit at a time, */
			/* left shifting each bit */
		{
			ctrl |= PLX_CTRL_SEPCLK;
			writel(ctrl, (eprom_ctl));
			
			udelay(1);
			
			ctrl = readl((eprom_ctl));
			
			
			if ((ctrl & PLX_CTRL_SEPRD) != 0) 
			{
				shiftw |= 0x1;
			}
			
			i--;
			if (i == 0) 
			{
				break;
			}
			shiftw <<= 1;
			
			ctrl &= ~PLX_CTRL_SEPCLK;
			writel(ctrl, (eprom_ctl));
			udelay(1);
		}
		
		*ptr++ = shiftw;
	}
	
	ctrl &= ~PLX_CTRL_SEPCS;
	writel(ctrl, (eprom_ctl));
	
	udelay(1);
	ctrl &= ~PLX_CTRL_SEPCLK;
	writel(ctrl, (eprom_ctl));
	
	return 1;
}
