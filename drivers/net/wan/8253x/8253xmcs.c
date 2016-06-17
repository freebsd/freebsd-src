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
#include "8253xmcs.h"
#include "sp502.h"

/* Just to guarantee that strings are null terminated */
#define MEMCPY(dest, src, cnt) \
{ \
	memcpy((dest), (src), (cnt)); \
	(dest)[cnt] = 0; \
}

static unsigned char sp502progbyte[] =
{
	SP502_OFF,
	SP502_RS232,
	SP502_RS422,
	SP502_RS485,
	SP502_RS449,
	SP502_EIA530,
	SP502_V35
};	

/*
 * The following routines are the multichannel server I2C serial EPROM routines.
 */

/*
 * Set the clock and the data lines of the SEP.
 */
static void
mcs_sep_set(mcs_sep_t *msp, unsigned sdavalid, unsigned sda,
	    unsigned sclvalid, unsigned scl)
{
#ifdef MAX
#undef MAX
#endif		/* MAX */
	
#define MAX(x, y)	((x) > (y) ? (x) : (y))
	
	unsigned char csr;
	unsigned int sleeptime;
	
	/*
	 * Ensure sufficient clock
	 */
	
	sleeptime = 0;
	
	if (sclvalid) 
	{
		if (msp->s_scl && !scl) 
		{   /* do we have a downgoing transition? */
			sleeptime = MAX(1, sleeptime);
		}
		else if (!msp->s_scl && scl) 
		{	/* upgoing */
			sleeptime = MAX(2, sleeptime);
		}
		msp->s_scl = scl;
	}
	
	if (sdavalid) 
	{
		if ((msp->s_sda && !sda) || (!msp->s_sda && sda)) 
		{
			sleeptime = MAX(1, sleeptime);
		}
		
		msp->s_sda = sda;
	}
	
	if (sleeptime > 0) 
	{
		udelay(sleeptime);
	}
	
	/*
	 * Construct the CSR byte.
	 */
	csr = 0;
	if (msp->s_sda) 
	{
		csr |= CIMCMD_CIMCSR_SDA;
	}
	
	if (msp->s_scl) 
	{
		csr |= CIMCMD_CIMCSR_SCL;
	}
	
	writeb((unsigned char) csr, msp->s_wrptr);
}

static void
mcs_sep_start(mcs_sep_t *msp)
{
	/*
	 * Generate a START condition
	 */
	mcs_sep_set(msp, TRUE, TRUE, TRUE, TRUE);
	mcs_sep_set(msp, TRUE, FALSE, TRUE, TRUE);
}

static void
mcs_sep_stop(mcs_sep_t *msp)
{
	/*
	 * Generate a STOP condition
	 */
	mcs_sep_set(msp, TRUE, FALSE, TRUE, TRUE);
	mcs_sep_set(msp, TRUE, TRUE, TRUE, TRUE);
}

/*
 * Send out a single byte.
 */
static void
mcs_sep_byte(mcs_sep_t *msp, unsigned char val)
{
	register int	 bitcount;
	
	/* Clock may be high ... lower the clock */
	mcs_sep_set(msp, TRUE, FALSE, TRUE, FALSE);
	
	bitcount = 8;
	
	while (TRUE) 
	{
		mcs_sep_set(msp, TRUE, (val & 0x80) != 0, TRUE, FALSE);
		mcs_sep_set(msp, TRUE, (val & 0x80) != 0, TRUE, TRUE);
		
		bitcount--;
		if (bitcount == 0) 
		{
			break;
		}
		val <<= 1;
	}
	
	/* Clock is high ... lower the clock */
	mcs_sep_set(msp, FALSE, FALSE, TRUE, FALSE);
}

/*
 * Wait for an acknowledge cycle.  Expects the clock to be low.
 */
static unsigned
mcs_sep_waitsep(mcs_sep_t *msp)
{
	int loopcount;
	unsigned char cimcsr;
  
	/* Stop driving SDA */
	mcs_sep_set(msp, TRUE, TRUE, FALSE, FALSE);
	/* Raise the clock */
	mcs_sep_set(msp, FALSE, FALSE, TRUE, TRUE);
	
	loopcount = 1000;
	while (loopcount != 0) 
	{
		cimcsr = readb(msp->s_rdptr);
		
		if ((cimcsr & CIMCMD_CIMCSR_SDA) == 0) 
		{
			break;
		}
		loopcount--;
	}
	
	/* Lower the clock */
	mcs_sep_set(msp, FALSE, FALSE, TRUE, FALSE);
	
	if (loopcount == 0) 
	{
		return FALSE;
	}
	else 
	{
		return TRUE;
	}
}

/*
 * Read the given CIM's SEP, starting at the given address, into
 *  the given buffer, for the given length.
 *
 * Returns -1 if there was a failure, otherwise the byte count.
 */

static int
mcs_sep_read(mcs_sep_t *msp, unsigned short addr, 
	     unsigned char *buf, unsigned int nbytes)
{
	unsigned char cmdaddr, val, cimcsr;
	unsigned int bytecount, bitcount;
	
	mcs_sep_start(msp);
	
	/*
	 * First, send out a dummy WRITE command with no data.
	 */
	
	cmdaddr = 0xa0 | (((addr >> 8) & 0x7) << 1) | 0x0;
	
	mcs_sep_byte(msp, cmdaddr);
	
	if (!mcs_sep_waitsep(msp)) 
	{
		return -1;
	}
	
	/*
	 * Now, send the reset of the address.
	 */
	
	mcs_sep_byte(msp, (unsigned char) addr);
	
	if (!mcs_sep_waitsep(msp)) 
	{
		return -1;
	}
	
	/*
	 * Now, restart with a read command.
	 */
	
	mcs_sep_start(msp);
	
	cmdaddr = 0xa0 | (((addr >> 8) & 0x7) << 1) | 0x1;
	
	mcs_sep_byte(msp, cmdaddr);
	
	if (!mcs_sep_waitsep(msp)) 
	{
		return -1;
	}
	
	/*
	 * Now, start reading the bytes.
	 */
	bytecount = 0;
	while (TRUE) 
	{
		bitcount = 8;
		val = 0;
		while (TRUE) 
		{
			mcs_sep_set(msp, TRUE, TRUE, TRUE, TRUE);
			
			cimcsr = readb(msp->s_rdptr);
			
			if ((cimcsr & CIMCMD_CIMCSR_SDA) != 0) 
			{
				val |= 0x01;
			}
			
			mcs_sep_set(msp, FALSE, FALSE, TRUE, FALSE);
			bitcount--;
			
			if (bitcount == 0) 
			{
				break;
			}
			val <<= 1;
		}
		
		*buf++ = val;
		bytecount++;
		nbytes--;
		
		if (nbytes == 0) 
		{
			break;
		}
		
		/*
		 * Send the acknowledge.
		 */
		
		mcs_sep_set(msp, FALSE, FALSE, TRUE, FALSE);
		mcs_sep_set(msp, TRUE, FALSE, TRUE, TRUE);
		mcs_sep_set(msp, FALSE, FALSE, TRUE, FALSE);
	}
	
	mcs_sep_stop(msp);
	
	return (int) bytecount;
}


unsigned int mcs_ciminit(SAB_BOARD *bptr, AURA_CIM *cim)
{
	mcs_sep_t		 ms;
	
	ms.s_rdptr = (unsigned char *) 
		(bptr->CIMCMD_REG + (CIMCMD_RDCIMCSR | (cim->ci_num << CIMCMD_CIMSHIFT)));
	ms.s_wrptr = (unsigned char *) 
		(bptr->CIMCMD_REG + (CIMCMD_WRCIMCSR | (cim->ci_num << CIMCMD_CIMSHIFT)));
	ms.s_scl = ms.s_sda = FALSE;
	
	if (mcs_sep_read(&ms, (unsigned short) 0, &(cim->ci_sep[0]),
			 sizeof(cim->ci_sep)) != sizeof(cim->ci_sep)
	    || cim->ci_sep[MCS_SEP_MAGIC] != MCS_SEP_MAGICVAL) 
	{
		
		if (cim->ci_sep[MCS_SEP_MAGIC] != MCS_SEP_MAGICVAL) 
		{
			DEBUGPRINT((KERN_ALERT
				    "auraXX20: invalid CIM %d serial EPROM on board %d",
				    cim->ci_num, bptr->board_number));
		}
		else 
		{
			DEBUGPRINT((KERN_ALERT
				    "auraXX20: error reading CIM %d serial EPROM on board %d",
				    cim->ci_num, bptr->board_number));
		}
		
		cim->ci_clkspeed = WANMCS_CLKSPEED;
		cim->ci_clkspdsrc = -1;
		cim->ci_spdgrd = 10;
		cim->ci_spdgrdsrc = -1;
		cim->ci_flags = 0;
		cim->ci_rev[0] = '\0';
		cim->ci_sn[0] = '\0';
		cim->ci_mfgdate[0] = '\0';
		cim->ci_mfgloc[0] = '\0';
		
		/*
		 * Diddle the port setup registers to determine if this
		 *  CIM was built up for RS232 or SP502.
		 */
		
		writew((unsigned short) 0xffff, (unsigned short *) 
		       (bptr->CIMCMD_REG +
			(CIMCMD_WRSETUP | (cim->ci_num << CIMCMD_CIMSHIFT))));
		
#ifdef RICHARD_DELAY
		udelay(1);
#endif		/* RICHARD_DELAY */
		
		if (readw((unsigned short *) 
			  (bptr->CIMCMD_REG +
			   (CIMCMD_RDSETUP | (cim->ci_num << CIMCMD_CIMSHIFT)))) == 0xffff) 
		{
			
			writew(0, (unsigned short *) 
			       (bptr->CIMCMD_REG +
				(CIMCMD_WRSETUP | (cim->ci_num << CIMCMD_CIMSHIFT))));
			
#ifdef RICHARD_DELAY
			udelay(1);
#endif		/* RICHARD_DELAY */
			
			if (readw((unsigned short *) 
				  (bptr->CIMCMD_REG +
				   (CIMCMD_RDSETUP | (cim->ci_num << CIMCMD_CIMSHIFT)))) == 0) 
			{
				
				cim->ci_type = CIM_SP502;
			}
			else 
			{
				cim->ci_type = CIM_RS232;
			}
		}
		else 
		{
			cim->ci_type = CIM_RS232;
		}
		
		if (cim->ci_type == CIM_SP502) 
		{
			cim->ci_flags |= CIM_SYNC;
		}
	}
	else 
	{
		/*
		 * Pick through the serial EPROM contents and derive
		 *  the values we need.
		 */
		MEMCPY(&(cim->ci_rev[0]), &(cim->ci_sep[MCS_SEP_REV]),
		       MCS_SEP_REVLEN);
		MEMCPY(&(cim->ci_sn[0]), &(cim->ci_sep[MCS_SEP_SN]),
		       MCS_SEP_SNLEN);
		MEMCPY(&(cim->ci_mfgdate[0]), &(cim->ci_sep[MCS_SEP_MFGDATE]),
		       MCS_SEP_MFGDATELEN);
		MEMCPY(&(cim->ci_mfgloc[0]), &(cim->ci_sep[MCS_SEP_MFGLOC]),
		       MCS_SEP_MFGLOCLEN);
		
		cim->ci_clkspeed = (unsigned long) cim->ci_sep[MCS_SEP_CLKSPD]
			| ((unsigned long) cim->ci_sep[MCS_SEP_CLKSPD + 1] << 8)
			| ((unsigned long) cim->ci_sep[MCS_SEP_CLKSPD + 2] << 16)
			| ((unsigned long) cim->ci_sep[MCS_SEP_CLKSPD + 3] << 24);
		
		cim->ci_clkspdsrc = SEPROM;
		
		cim->ci_spdgrd = (int) cim->ci_sep[MCS_SEP_SPDGRD];
		cim->ci_spdgrdsrc = SEPROM;
		
		cim->ci_flags = (unsigned long) cim->ci_sep[MCS_SEP_FLAGS];
		
		cim->ci_type = (int) cim->ci_sep[MCS_SEP_TYPE];
	}
	
	/*
	 * Possibly initialize the port setup registers.
	 */
	
	if (cim->ci_type == CIM_SP502) 
	{
		unsigned short alloff;
#ifdef DEBUG_VERBOSE
		unsigned short readback;
#endif
		int offset;
		
		/*
		 * Turn off all of the electrical interfaces.  The
		 *  hardware *should* initialize to this state, but the
		 *  prototype, at least, does not.  Note that this setting
		 *  is reflected in the SIF_OFF setting of l_interface in
		 *  mustard_lineinit, above.
		 */
		
		alloff = (unsigned short) SP502_OFF
			| ((unsigned short) SP502_OFF << 4)
			| ((unsigned short) SP502_OFF << 8)
			| ((unsigned short) SP502_OFF << 12);
		for (offset = 0; offset < 8; offset++) 
		{
#ifdef DEBUG_VERBOSE
			DEBUGPRINT((KERN_ALERT "cim %d setup reg #%d: writing 0x%x to 0x%x",
				    cim->ci_num, offset, (unsigned) alloff,
				    (CIMCMD_WRSETUP | (offset << 1) |
				     (cim->ci_num << CIMCMD_CIMSHIFT))));
#endif		 /* DEBUG_VERBOSE */
			
			writew((unsigned short) alloff, (unsigned short *)
			       (bptr->CIMCMD_REG +
				(CIMCMD_WRSETUP | (offset << 1) |
				 (cim->ci_num << CIMCMD_CIMSHIFT))));
#ifdef RICHARD_DELAY
			udelay(1);
#endif		/* RICHARD_DELAY */
#ifdef DEBUG_VERBOSE
			readback = readw((unsigned short *)
					 (bptr->CIMCMD_REG +
					  (CIMCMD_RDSETUP | (offset << 1) |
					   (cim->ci_num << CIMCMD_CIMSHIFT))));
			if (readback != alloff) 
			{
				DEBUGPRINT((KERN_ALERT "cim %d setup reg #%d: readback (0x%x) should be 0x%x",
					    cim->ci_num, offset, readback, alloff));
			}
#endif		/* DEBUG_VERBOSE */
		}
	}
	
	/*
	 * Clear out the CIM CSR with the exception of the LED.
	 */
	
	writeb((unsigned char) 0, 
	       (unsigned char *) (bptr->CIMCMD_REG +
				  (CIMCMD_WRCIMCSR | (cim->ci_num << CIMCMD_CIMSHIFT))));
	
	return TRUE;
}


int wanmcs_reset(SAB_BOARD* bptr) /* note the board is the host card not the
				   * individual extension boards
				   */
{
	int		 counter;
	
#if 0				/* from the ASE driver */
	/*
	 * Program the AMCC to deactivate the write FIFO.
	 */
	
	ASE_PUT32(cboard->b_bridgehandle,
		  (aseuint32_t *) (cboard->b_bridge + AMCC_PTCR),
		  ((aseuint32_t) (AMCC_PTMODE | AMCC_WRFIFODIS) << 24) |
		  ((aseuint32_t) (AMCC_PTMODE | AMCC_WRFIFODIS) << 16) |
		  ((aseuint32_t) (AMCC_PTMODE | AMCC_WRFIFODIS) << 8) |
		  (aseuint32_t) (AMCC_PTMODE | AMCC_WRFIFODIS));
#endif		/* 0 */
	
	/*
	 * First thing: do a reset of the local bus on the MIC 
	 *  by diddling the Add-On Reset bit in the RCR.
	 */
	
	writel((unsigned int) AMCC_AORESET, 
	       (unsigned int *)(bptr->AMCC_REG + AMCC_RCR));
	
	udelay(10);		/* wait for 10 us. */
	
	writel((unsigned int) 0, 
	       (unsigned int *)(bptr->AMCC_REG + AMCC_RCR));
	
	udelay(10);		/* wait for 10 us. */
	
	/*
	 * Now the PCI bridge is reset.  Try to establish
	 *  a link through the Glink chipset.
	 */
	
	for (counter = 1000; counter != 0; counter--) 
	{
		writeb(0, (unsigned char*) (bptr->MICCMD_REG + MICCMD_MICCSR));
		
		udelay(5);
		
		if((readb((unsigned char*) 
			  (bptr->MICCMD_REG + MICCMD_MICCSR)) & MICCMD_MICCSR_GLE) == 0)
		{
			break;
		}
	}
	
	/*
	 * Did we run out of time?
	 */
	
	if (counter == 0) 
	{
		printk(KERN_ALERT 
		       "AMCC5920: board %p: GLink did not reset -- is the MEB on?",
		       bptr);
		
		return FALSE;
	}
	
	/*
	 * Now, hit the reset in the MEB.
	 */
	
	writeb(0, (unsigned int *) (bptr->CIMCMD_REG + CIMCMD_RESETENA));
	
	udelay(5);
	
	writeb(0, (unsigned int *) (bptr->CIMCMD_REG + CIMCMD_RESETDIS));
	
	/*
	 * And we're done!
	 */
	
	return TRUE;
}

void aura_sp502_program(SAB_PORT *port, register unsigned int sigindex)
{
	register unsigned char prognibble;
	SAB_BOARD *bptr;
	unsigned int cimnum;
	unsigned int chipno;
	unsigned int portno;
	unsigned int rdaddressreceiver;
	unsigned int rdaddresstransmitter;
	unsigned int wraddressreceiver;
	unsigned int wraddresstransmitter;
	unsigned short datareceiver;
	unsigned short datatransmitter;

	bptr = port->board;
	cimnum = port->chip->c_cim->ci_num;
	chipno = (port->chip->c_chipno & 1); /* chip number relative to EB not MCS */
	portno = (port->portno + (8 * chipno)); /* portno on a per EB basis */

	prognibble = (sp502progbyte[sigindex] & 0x0F);

				/* first 4 shorts contain receiver control bits */
	rdaddressreceiver = 
		(((unsigned int)bptr->CIMCMD_REG) + 
		 ((cimnum << CIMCMD_CIMSHIFT) | CIMCMD_RDSETUP | ((portno/4) << CIMCMD_CTRLSHIFT)));
				/* second 4 shorts contain transmitter control bits */
	rdaddresstransmitter = 
		(((unsigned int)bptr->CIMCMD_REG) + 
		 ((cimnum << CIMCMD_CIMSHIFT) | CIMCMD_RDSETUP | ((4+(portno/4)) << CIMCMD_CTRLSHIFT)));

	wraddressreceiver = 
		(((unsigned int)bptr->CIMCMD_REG) + 
		 ((cimnum << CIMCMD_CIMSHIFT) | CIMCMD_WRSETUP | ((portno/4) << CIMCMD_CTRLSHIFT)));
	wraddresstransmitter = 
		(((unsigned int)bptr->CIMCMD_REG) + 
		 ((cimnum << CIMCMD_CIMSHIFT) | CIMCMD_WRSETUP | ((4+(portno/4)) << CIMCMD_CTRLSHIFT)));

				/* read out the current receiver status */
	datareceiver = readw((unsigned short*) rdaddressreceiver);
				/* clear out nibble that corresponds to current port */
	datareceiver &= (unsigned short) ~(0x0F << ((3 - (portno % 4)) * 4));
				/* or in new receiver control field */
	datareceiver |= (prognibble << ((3 - (portno % 4)) * 4));
				/* write back the short that corresponds to 4 ports */
	writew(datareceiver, (unsigned short*) wraddressreceiver);

				/* just as above except that next 4 shorts correspond to transmitters */
	datatransmitter = readw((unsigned short*) rdaddresstransmitter);
	datatransmitter &= (unsigned short) ~(0x0F << ((3 - (portno % 4)) * 4)); 
	datatransmitter |= (prognibble << ((3 - (portno % 4)) * 4));
	writew(datatransmitter, (unsigned short*) wraddresstransmitter);
}
