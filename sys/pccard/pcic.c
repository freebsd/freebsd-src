/*
 *  Intel PCIC or compatible Controller driver
 *  May be built using LKM to make a loadable module.
 *-------------------------------------------------------------------------
 *
 * Copyright (c) 1995 Andrew McRae.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef	LKM
#define	NPCIC 1
#else
#include "pcic.h"
#endif

#if	NPCIC > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/sysent.h>
#include <sys/exec.h>
#include <sys/lkm.h>

#include <machine/clock.h>

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/icu.h>

#include <pccard/i82365.h>
#include <pccard/card.h>
#include <pccard/slot.h>

/*
 *	Prototypes for interrupt handler.
 */
static void pcicintr	__P((int unit));
static int pcic_ioctl __P((struct slot *, int, caddr_t));
static int pcic_power __P((struct slot *));
static void pcic_reset __P((struct slot *));
static void pcic_disable __P((struct slot *));
static void pcic_mapirq __P((struct slot *, int));

/*
 *	Per-slot data table.
 */
static struct pcic_slot
	{
	int slot;			/* My slot number */
	int index;			/* Index register */
	int data;			/* Data register */
	int offset;			/* Offset value for index */
	char	controller;		/* Device type */
	char	revision;		/* Device Revision */
	struct slot *sp;		/* Back ptr to slot */
	} pcic_slots[PCIC_MAX_SLOTS];
static int pcic_irq;
static unsigned long pcic_imask;
static struct slot_cont cinfo;

static int pcic_memory(struct slot *, int);
static int pcic_io(struct slot *, int);
int pcic_probe();

/*
 *	Internal inline functions for accessing the PCIC.
 */
/*
 * Read a register from the PCIC.
 */
static inline unsigned char
getb (struct pcic_slot *sp, int reg)
{
	outb (sp->index, sp->offset + reg);
	return inb (sp->data);
}

/*
 * Write a register on the PCIC
 */
static inline void
putb (struct pcic_slot *sp, int reg, unsigned char val)
{
	outb (sp->index, sp->offset + reg);
	outb (sp->data, val);
}
/*
 * Clear bit(s) of a register.
 */
static inline void
clrb(struct pcic_slot *sp, int reg, unsigned char mask)
{
	putb (sp, reg, getb (sp, reg) & ~mask);
}
/*
 * Set bit(s) of a register
 */
static inline void
setb(struct pcic_slot *sp, int reg, unsigned char mask)
{
	putb (sp, reg, getb (sp, reg) | mask);
}

/*
 * Write a 16 bit value to 2 adjacent PCIC registers
 */
static inline void
putw (struct pcic_slot *sp, int reg, unsigned short word)
{
	putb (sp, reg, word & 0xFF);
	putb (sp, reg + 1, (word >> 8) & 0xff);
}


/*
 *	Loadable kernel module interface.
 */
#ifdef	LKM
/*
 *	This defines the lkm_misc module use by modload
 *	to define the module name.
 */
 MOD_MISC( "pcic")


static int pcic_unload();
/*
 *	Module handler that processes loads and unloads.
 *	Once the module is loaded, the probe routine
 *	is called to install the slots (if any).
 */

static int
pcic_handle( lkmtp, cmd)
struct lkm_table	*lkmtp;
int			cmd;
{
	int			err = 0;	/* default = success*/

	switch( cmd) {
	case LKM_E_LOAD:

		/*
		 * Don't load twice! (lkmexists() is exported by kern_lkm.c)
		 */
		if( lkmexists( lkmtp))
			return( EEXIST);
/*
 *	Call the probe routine to find the slots. If
 *	no slots exist, then don't bother loading the module.
 */
		if (pcic_probe() == 0)
			return(ENODEV);
		break;		/* Success*/
/*
 *	Attempt to unload the slot driver.
 */
	case LKM_E_UNLOAD:
		printf("Unloading PCIC driver\n");
		err = pcic_unload();
		break;		/* Success*/

	default:	/* we only understand load/unload*/
		err = EINVAL;
		break;
	}

	return( err);
}

/*
 * External entry point; should generally match name of .o file.  The
 * arguments are always the same for all loaded modules.  The "load",
 * "unload", and "stat" functions in "DISPATCH" will be called under
 * their respective circumstances unless their value is "nosys".  If
 * called, they are called with the same arguments (cmd is included to
 * allow the use of a single function, ver is included for version
 * matching between modules and the kernel loader for the modules).
 *
 * Since we expect to link in the kernel and add external symbols to
 * the kernel symbol name space in a future version, generally all
 * functions used in the implementation of a particular module should
 * be static unless they are expected to be seen in other modules or
 * to resolve unresolved symbols alread existing in the kernel (the
 * second case is not likely to ever occur).
 *
 * The entry point should return 0 unless it is refusing load (in which
 * case it should return an errno from errno.h).
 */
int
pcic_mod(lkmtp, cmd, ver)
struct lkm_table	*lkmtp;	
int			cmd;
int			ver;
{
	DISPATCH(lkmtp,cmd,ver,pcic_handle,pcic_handle,nosys)
}
/*
 *	pcic_unload - Called when unloading a LKM.
 *	Disables interrupts and resets PCIC.
 */
static int
pcic_unload()
{
int	slot;
struct pcic_slot *cp = pcic_slots;

	if (pcic_irq)
		{
		for (slot = 0; slot < PCIC_MAX_SLOTS; slot++, cp++)
			if (cp->sp)
				putb(cp, PCIC_STAT_INT, 0);
		unregister_intr(pcic_irq, pcicintr);
		}
	pccard_remove_controller(&cinfo);
	return(0);
}
#endif LKM

#if 0
static void
pcic_dump_attributes (unsigned char *scratch, int maxlen)
{
	int i,j,k;

	i = 0;
	while (scratch[i] != 0xff && i < maxlen) {
	unsigned char link = scratch[i+2];

/*
 *	Dump attribute memory
 */
	if (scratch[i])
		{
		printf ("[%02x] ", i);
		for (j = 0; j < 2 * link + 4 && j < 128; j += 2)
			printf ("%02x ", scratch[j + i]);
		printf ("\n");
		}
	i += 4 + 2 * link;
	}
}
#endif

/*
 *	entry point from main code to map/unmap memory context.
 */
static int
pcic_memory(struct slot *sp, int win)
{
struct pcic_slot *cp = sp->cdata;
struct mem_desc *mp = &sp->mem[win];
int reg = mp->window * PCIC_MEMSIZE + PCIC_MEMBASE;

	if (mp->flags & MDF_ACTIVE)
		{
		unsigned long sys_addr = (unsigned long)mp->start >> 12;
/*
 *	Write the addresses, card offsets and length.
 *	The values are all stored as the upper 12 bits of the
 *	24 bit address i.e everything is allocated as 4 Kb chunks.
 */
		putw (cp, reg, sys_addr & 0xFFF);
		putw (cp, reg+2, (sys_addr + (mp->size >> 12) - 1) & 0xFFF);
		putw (cp, reg+4, ((mp->card >> 12) - sys_addr) & 0x3FFF);
#if 0
		printf("card offs = card_adr = 0x%x 0x%x, sys_addr = 0x%x\n", 
			mp->card, ((mp->card >> 12) - sys_addr) & 0x3FFF,
			sys_addr);
#endif
/*
 *	Each 16 bit register has some flags in the upper bits.
 */
		if (mp->flags & MDF_16BITS)
			setb(cp, reg+1, PCIC_DATA16);
		if (mp->flags & MDF_ZEROWS)
			setb(cp, reg+1, PCIC_ZEROWS);
		if (mp->flags & MDF_WS0)
			setb(cp, reg+3, PCIC_MW0);
		if (mp->flags & MDF_WS1)
			setb(cp, reg+3, PCIC_MW1);
		if (mp->flags & MDF_ATTR)
			setb(cp, reg+5, PCIC_REG);
		if (mp->flags & MDF_WP)
			setb(cp, reg+5, PCIC_WP);
#if 0
	printf("Slot number %d, reg 0x%x, offs 0x%x\n",
		cp->slot, reg, cp->offset);
	printf("Map window to sys addr 0x%x for %d bytes, card 0x%x\n",
		mp->start, mp->size, mp->card);
	printf("regs are: 0x%02x%02x 0x%02x%02x 0x%02x%02x flags 0x%x\n",
		getb(cp, reg), getb(cp, reg+1),
		getb(cp, reg+2), getb(cp, reg+3),
		getb(cp, reg+4), getb(cp, reg+5),
		mp->flags);
#endif
/*
 *	Enable the memory window. By experiment, we need a delay.
 */
		setb (cp, PCIC_ADDRWINE, (1<<win) | PCIC_MEMCS16);
		DELAY(50);
		}
	else
		{
#if 0
		printf("Unmapping window %d\n", win);
#endif
		clrb (cp, PCIC_ADDRWINE, 1<<win);
		putw (cp, reg, 0);
		putw (cp, reg+2, 0);
		putw (cp, reg+4, 0);
		}
	return(0);
}
/*
 *	pcic_io - map or unmap I/O context
 */
static int
pcic_io(struct slot *sp, int win)
{
int	mask, reg;
struct pcic_slot *cp = sp->cdata;
struct io_desc *ip = &sp->io[win];

	if (win)
		{
		mask = PCIC_IO0_EN;
		reg = PCIC_IO0;
		}
	else
		{
		mask = PCIC_IO1_EN;
		reg = PCIC_IO1;
		}
	if (ip->flags & IODF_ACTIVE)
		{
		unsigned char x = 0;

		putw (cp, reg, ip->start);
		putw (cp, reg+2, ip->start+ip->size-1);
		if (ip->flags & IODF_ZEROWS)
			x = PCIC_IO_0WS;
		if (ip->flags & IODF_WS)
			x |= PCIC_IO_WS;
		if (ip->flags & IODF_CS16)
			x |= PCIC_IO_CS16;
		else if (ip->flags & IODF_16BIT)
			x |= PCIC_IO_16BIT;
/*
 *	Extract the current flags and merge with new flags.
 *	Flags for window 0 in lower nybble, and in upper nybble
 *	for window 1.
 */
		if (win)
			putb(cp, PCIC_IOCTL, (x << 4) |
				(getb(cp, PCIC_IOCTL) & 0xF));
		else
			putb(cp, PCIC_IOCTL, x | (getb(cp, PCIC_IOCTL) & 0xF0));
		setb (cp, PCIC_ADDRWINE, mask);
		DELAY(100);
		}
	else
		{
		clrb (cp, PCIC_ADDRWINE, mask);
		putw (cp, reg, 0);
		putw (cp, reg + 2, 0);
		}
	return(0);
}
/*
 *	Look for an Intel PCIC (or compatible).
 *	For each available slot, allocate a PC-CARD slot.
 */

int
pcic_probe ()
{
int slot, i, validslots = 0;
struct slot *sp;
struct pcic_slot *cp;
unsigned char c;

/*
 *	Initialise controller information structure.
 */
	cinfo.mapmem = pcic_memory;
	cinfo.mapio = pcic_io;
	cinfo.ioctl = pcic_ioctl;
	cinfo.power = pcic_power;
	cinfo.mapirq = pcic_mapirq;
	cinfo.reset = pcic_reset;
	cinfo.disable = pcic_disable;
	cinfo.maxmem = PCIC_MEM_WIN;
	cinfo.maxio = PCIC_IO_WIN;
	cinfo.irqs = PCIC_INT_MASK_ALLOWED;

#ifdef	LKM
	bzero(pcic_slots, sizeof(pcic_slots));
#endif
	cp = pcic_slots;
	for (slot = 0; slot < PCIC_MAX_SLOTS; slot++, cp++) {
/*
 *	Initialise the PCIC slot table.
 */
	if (slot < 4)
		{
		cp->index = PCIC_INDEX_0;
		cp->data = PCIC_DATA_0;
		cp->offset = slot * PCIC_SLOT_SIZE;
		}
	else
		{
		cp->index = PCIC_INDEX_1;
		cp->data = PCIC_DATA_1;
		cp->offset = (slot - 4) * PCIC_SLOT_SIZE;
		}
	/*
	 * see if there's a PCMCIA controller here
	 * Intel PCMCIA controllers use 0x82 and 0x83
	 * IBM clone chips use 0x88 and 0x89, apparently
	 */
	c = getb (cp, PCIC_ID_REV);
	cp->revision = -1;
	switch(c)
		{
/*
 *	82365 or clones.
 */
	case 0x82:
	case 0x83:
		cp->controller = PCIC_I82365;
		cp->revision = c & 1;
/*
 *	Now check for VADEM chips.
 */
		outb(cp->index, 0x0E);
		outb(cp->index, 0x37);
		setb(cp, 0x3A, 0x40);
		c = getb (cp, PCIC_ID_REV);
		if (c & 0x08)
			{
			cp->controller = PCIC_VG468;
			cp->revision = c & 7;
			clrb(cp, 0x3A, 0x40);
			}
		break;
/*
 *	VLSI chips.
 */
	case 0x84:
		cp->controller = PCIC_VLSI;
		break;
	case 0x88:
	case 0x89:
		cp->controller = PCIC_IBM;
		cp->revision = c & 1;
		break;
	default:
		continue;
		}
/*
 *	Check for Cirrus logic chips.
 */
	putb(cp, 0x1F, 0);
	c = getb(cp, 0x1F);
	if ((c & 0xC0) == 0xC0)
		{
		c = getb(cp, 0x1F);
		if ((c & 0xC0) == 0)
			{
			if (c & 0x20)
				cp->controller = PCIC_PD672X;
			else
				cp->controller = PCIC_PD6710;
			cp->revision = 8 - ((c & 0x1F) >> 2);
			}
		}
	switch(cp->controller)
		{
	case PCIC_I82365:
		cinfo.name = "Intel 82365";
		break;
	case PCIC_IBM:
		cinfo.name = "IBM PCIC";
		break;
	case PCIC_PD672X:
		cinfo.name = "Cirrus Logic PD672X";
		break;
	case PCIC_PD6710:
		cinfo.name = "Cirrus Logic PD6710";
		break;
	case PCIC_VG468:
		cinfo.name = "Vadem 468";
		break;
	default:
		cinfo.name = "Unknown!";
		break;
		}
/*
 *	clear out the registers.
 */
	for (i = 2; i < 0x40; i++)
		putb(cp, i, 0);
/*
 *	OK it seems we have a PCIC or lookalike.
 *	Allocate a slot and initialise the data structures.
 */
	validslots++;
	cp->slot = slot;
	sp = pccard_alloc_slot(&cinfo);
	if (sp == 0)
		continue;
	sp->cdata = cp;
	cp->sp = sp;
/*
 *	If we haven't allocated an interrupt for the controller,
 *	then attempt to get one.
 */
	if (pcic_irq == 0)
		{
		pcic_irq = pccard_alloc_intr(PCIC_INT_MASK_ALLOWED,
			pcicintr, 0, &pcic_imask);
#if 0
		for (try = 0; try < 16; try++)
			if (((1 << try) & PCIC_INT_MASK_ALLOWED) &&
			    !pccard_alloc_intr(try, pcicintr, 0, &tty_imask))
				{
				pcic_irq = try;
				break;
				}
#endif
		if (pcic_irq < 0)
			printf("pcic: failed to allocate IRQ\n");
		}
	INTREN (1 << pcic_irq);
/*
 *	Check for a card in this slot.
 */
	setb (cp, PCIC_POWER, PCIC_APSENA | PCIC_DISRST);
	if ((getb (cp, PCIC_STATUS) & PCIC_CD) != PCIC_CD)
		sp->laststate = sp->state = empty;
	else
		{
		sp->laststate = sp->state = filled;
		pccard_event(cp->sp, card_inserted);
		}
/*
 *	Assign IRQ for slot changes
 */
	if (pcic_irq > 0)
		putb(cp, PCIC_STAT_INT, (pcic_irq << 4) | 0xF);
	}
	return(validslots);
}
/*
 *	ioctl calls - Controller specific ioctls
 */
static int
pcic_ioctl(struct slot *sp, int cmd, caddr_t data)
{

	switch(cmd)
		{
	default:
		return(EINVAL);
/*
 * Get/set PCIC registers
 */
	case PIOCGREG:
		((struct pcic_reg *)data)->value =
			getb(sp->cdata, ((struct pcic_reg *)data)->reg);
		break;
	case PIOCSREG:
		putb(sp->cdata, ((struct pcic_reg *)data)->reg,
			((struct pcic_reg *)data)->value);
		break;
		}
	return(0);
}
/*
 *	pcic_power - Enable the power of the slot according to
 *	the parameters in the power structure(s).
 */
static int
pcic_power(struct slot *slotp)
{
unsigned char reg = PCIC_DISRST|PCIC_APSENA;
struct pcic_slot *sp = slotp->cdata;

	switch(sp->controller)
		{
	case PCIC_PD672X:
	case PCIC_PD6710:
		switch(slotp->pwr.vpp)
			{
		default:
			return(EINVAL);
		case 0:
			break;
		case 50:
		case 33:
			reg |= PCIC_VPP_5V;
			break;
		case 120:
			reg |= PCIC_VPP_12V;
			break;
			}
		switch(slotp->pwr.vcc)
			{
		default:
			return(EINVAL);
		case 0:
			break;
		case 33:
			reg |= PCIC_VCC_5V;
			setb(sp, 0x16, 0x02);
			break;
		case 50:
			reg |= PCIC_VCC_5V;
			clrb(sp, 0x16, 0x02);
			break;
			}
		}
	putb (sp, PCIC_POWER, reg);
	DELAY(300*1000);
	if (slotp->pwr.vcc)
		{
		reg |= PCIC_OUTENA;
		putb (sp, PCIC_POWER, reg);
		DELAY (100*1000);
		}
	return(0);
}

/*
 * tell the PCIC which irq we want to use.  only the following are legal:
 * 3, 4, 5, 7, 9, 10, 11, 12, 14, 15
 */
static void
pcic_mapirq (struct slot *slotp, int irq)
{
struct pcic_slot *sp = slotp->cdata;

	if (irq == 0)
		clrb(sp, PCIC_INT_GEN, 0xF);
	else
		putb (sp, PCIC_INT_GEN, (getb (sp, PCIC_INT_GEN) & 0xF0) | irq);
}
/*
 *	pcic_reset - Reset the card and enable initial power.
 *	Allow
 */
static void
pcic_reset(struct slot *slotp)
{
struct pcic_slot *sp = slotp->cdata;

	clrb (sp, PCIC_INT_GEN, PCIC_CARDRESET);
	DELAY (200*1000);
	setb (sp, PCIC_INT_GEN, PCIC_CARDRESET|PCIC_IOCARD);
	DELAY (200*1000);
	if (sp->controller == PCIC_PD672X ||
	    sp->controller == PCIC_PD6710)
		{
		putb(sp, PCIC_TIME_SETUP0, 0x1);
		putb(sp, PCIC_TIME_CMD0, 0x6);
		putb(sp, PCIC_TIME_RECOV0, 0x0);
		putb(sp, PCIC_TIME_SETUP1, 1);
		putb(sp, PCIC_TIME_CMD1, 0x5F);
		putb(sp, PCIC_TIME_RECOV1, 0);
		}
}
/*
 *	pcic_disable - Disable the slot.
 */
static void
pcic_disable(struct slot *slotp)
{
struct pcic_slot *sp = slotp->cdata;

	putb(sp, PCIC_INT_GEN, 0);
	putb(sp, PCIC_POWER, 0);
}

/*
 *	PCIC Interrupt handler.
 *	Check each slot in turn, and read the card status change
 *	register. If this is non-zero, then a change has occurred
 *	on this card, so send an event to the main code.
 */
static void
pcicintr(int unit)
{
int	slot, s;
unsigned char chg;
struct pcic_slot *cp = pcic_slots;

	s = splhigh();
	for (slot = 0; slot < PCIC_MAX_SLOTS; slot++, cp++)
		if (cp->sp)
			if ((chg = getb(cp, PCIC_STAT_CHG)) != 0)
				if (chg & PCIC_CDTCH)
					{
					if ((getb(cp, PCIC_STATUS) & PCIC_CD) ==
							PCIC_CD)
						pccard_event(cp->sp,
							card_inserted);
					else
						pccard_event(cp->sp,
							card_removed);
					}
	splx(s);
}
#endif
