/*
 *  Intel PCIC or compatible Controller driver
 *  May be built to make a loadable module.
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/select.h>
#include <sys/interrupt.h>

#include <machine/clock.h>

#if 0
#include <i386/isa/icu.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/intr_machdep.h>
#endif

#include <pccard/i82365.h>

#include <pccard/cardinfo.h>
#include <pccard/driver.h>
#include <pccard/slot.h>
#include <pccard/pcic.h>

#ifdef APIC_IO
#include <machine/smp.h>
#endif 

/*
 *	Prototypes for interrupt handler.
 */
static inthand2_t	pcicintr;
static int		pcic_ioctl __P((struct slot *, int, caddr_t));
static int		pcic_power __P((struct slot *));
static timeout_t 	pcic_reset;
static void		pcic_resume(struct slot *);
static void		pcic_disable __P((struct slot *));
static void		pcic_mapirq __P((struct slot *, int));
static timeout_t 	pcictimeout;
static struct callout_handle pcictimeout_ch
    = CALLOUT_HANDLE_INITIALIZER(&pcictimeout_ch);
static int		pcic_memory(struct slot *, int);
static int		pcic_io(struct slot *, int);

/*
 *	Per-slot data table.
 */
static struct pcic_slot {
	int slotnum;			/* My slot number */
	int index;			/* Index register */
	int data;			/* Data register */
	int offset;			/* Offset value for index */
	char controller;		/* Device type */
	char revision;			/* Device Revision */
	struct slot *slt;		/* Back ptr to slot */
	u_char (*getb)(struct pcic_slot *, int);
	void   (*putb)(struct pcic_slot *, int, u_char);
	u_char	*regs;			/* Pointer to regs in mem */
} pcic_slots[PCIC_MAX_SLOTS];

static int		pcic_irq;
static struct slot_ctrl cinfo;


/*
 *	Internal inline functions for accessing the PCIC.
 */
/*
 * Read a register from the PCIC.
 */
static __inline unsigned char
getb1(struct pcic_slot *sp, int reg)
{
	outb(sp->index, sp->offset + reg);
	return inb(sp->data);
}

/*
 * Write a register on the PCIC
 */
static __inline void
putb1(struct pcic_slot *sp, int reg, unsigned char val)
{
	outb(sp->index, sp->offset + reg);
	outb(sp->data, val);
}

/*
 * Clear bit(s) of a register.
 */
static __inline void
clrb(struct pcic_slot *sp, int reg, unsigned char mask)
{
	sp->putb(sp, reg, sp->getb(sp, reg) & ~mask);
}

/*
 * Set bit(s) of a register
 */
static __inline void
setb(struct pcic_slot *sp, int reg, unsigned char mask)
{
	sp->putb(sp, reg, sp->getb(sp, reg) | mask);
}

/*
 * Write a 16 bit value to 2 adjacent PCIC registers
 */
static __inline void
putw(struct pcic_slot *sp, int reg, unsigned short word)
{
	sp->putb(sp, reg, word & 0xFF);
	sp->putb(sp, reg + 1, (word >> 8) & 0xff);
}

/*
 *	entry point from main code to map/unmap memory context.
 */
static int
pcic_memory(struct slot *slt, int win)
{
	struct pcic_slot *sp = slt->cdata;
	struct mem_desc *mp = &slt->mem[win];
	int reg = mp->window * PCIC_MEMSIZE + PCIC_MEMBASE;

	if (mp->flags & MDF_ACTIVE) {
		unsigned long sys_addr = (uintptr_t)(void *)mp->start >> 12;
		/*
		 * Write the addresses, card offsets and length.
		 * The values are all stored as the upper 12 bits of the
		 * 24 bit address i.e everything is allocated as 4 Kb chunks.
		 */
		putw(sp, reg, sys_addr & 0xFFF);
		putw(sp, reg+2, (sys_addr + (mp->size >> 12) - 1) & 0xFFF);
		putw(sp, reg+4, ((mp->card >> 12) - sys_addr) & 0x3FFF);
		/*
		 *	Each 16 bit register has some flags in the upper bits.
		 */
		if (mp->flags & MDF_16BITS)
			setb(sp, reg+1, PCIC_DATA16);
		if (mp->flags & MDF_ZEROWS)
			setb(sp, reg+1, PCIC_ZEROWS);
		if (mp->flags & MDF_WS0)
			setb(sp, reg+3, PCIC_MW0);
		if (mp->flags & MDF_WS1)
			setb(sp, reg+3, PCIC_MW1);
		if (mp->flags & MDF_ATTR)
			setb(sp, reg+5, PCIC_REG);
		if (mp->flags & MDF_WP)
			setb(sp, reg+5, PCIC_WP);
		/*
		 * Enable the memory window. By experiment, we need a delay.
		 */
		setb(sp, PCIC_ADDRWINE, (1<<win) | PCIC_MEMCS16);
		DELAY(50);
	} else {
		clrb(sp, PCIC_ADDRWINE, 1<<win);
		putw(sp, reg, 0);
		putw(sp, reg+2, 0);
		putw(sp, reg+4, 0);
	}
	return(0);
}

/*
 *	pcic_io - map or unmap I/O context
 */
static int
pcic_io(struct slot *slt, int win)
{
	int	mask, reg;
	struct pcic_slot *sp = slt->cdata;
	struct io_desc *ip = &slt->io[win];
	switch (win) {
	case 0:
		mask = PCIC_IO0_EN;
		reg = PCIC_IO0;
		break;
	case 1:
		mask = PCIC_IO1_EN;
		reg = PCIC_IO1;
		break;
	default:
		panic("Illegal PCIC I/O window request!");
	}
	if (ip->flags & IODF_ACTIVE) {
		unsigned char x, ioctlv;

		putw(sp, reg, ip->start);
		putw(sp, reg+2, ip->start+ip->size-1);
		x = 0;
		if (ip->flags & IODF_ZEROWS)
			x |= PCIC_IO_0WS;
		if (ip->flags & IODF_WS)
			x |= PCIC_IO_WS;
		if (ip->flags & IODF_CS16)
			x |= PCIC_IO_CS16;
		if (ip->flags & IODF_16BIT)
			x |= PCIC_IO_16BIT;
		/*
		 * Extract the current flags and merge with new flags.
		 * Flags for window 0 in lower nybble, and in upper nybble
		 * for window 1.
		 */
		ioctlv = sp->getb(sp, PCIC_IOCTL);
		DELAY(100);
		switch (win) {
		case 0:
			sp->putb(sp, PCIC_IOCTL, x | (ioctlv & 0xf0));
			break;
		case 1:
			sp->putb(sp, PCIC_IOCTL, (x << 4) | (ioctlv & 0xf));
			break;
		}
		DELAY(100);
		setb(sp, PCIC_ADDRWINE, mask);
		DELAY(100);
	} else {
		clrb(sp, PCIC_ADDRWINE, mask);
		DELAY(100);
		putw(sp, reg, 0);
		putw(sp, reg + 2, 0);
	}
	return(0);
}

/*
 *	Look for an Intel PCIC (or compatible).
 *	For each available slot, allocate a PC-CARD slot.
 */

/*
 *	VLSI 82C146 has incompatibilities about the I/O address 
 *	of slot 1.  Assume it's the only PCIC whose vendor ID is 0x84,
 *	contact Nate Williams <nate@FreeBSD.org> if incorrect.
 */
static int
pcic_probe(device_t dev)
{
	int slotnum, validslots = 0;
	struct slot *slt;
	struct pcic_slot *sp;
	unsigned char c;
	void		 *ih;
	char *name;
	int i;
	int error;
	struct resource *res;
	int rid;
	static int maybe_vlsi = 0;

	if (device_get_unit(dev) != 0)
		return ENXIO;

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
	cinfo.resume = pcic_resume;
	cinfo.maxmem = PCIC_MEM_WIN;
	cinfo.maxio = PCIC_IO_WIN;

	sp = pcic_slots;
	for (slotnum = 0; slotnum < PCIC_MAX_SLOTS; slotnum++, sp++) {
		/*
		 *	Initialise the PCIC slot table.
		 */
		sp->getb = getb1;
		sp->putb = putb1;
		if (slotnum < 4) {
			sp->index = PCIC_INDEX_0;
			sp->data = PCIC_DATA_0;
			sp->offset = slotnum * PCIC_SLOT_SIZE;
		} else {
			sp->index = PCIC_INDEX_1;
			sp->data = PCIC_DATA_1;
			sp->offset = (slotnum - 4) * PCIC_SLOT_SIZE;
		}
		/* 
		 * XXX - Screwed up slot 1 on the VLSI chips.  According to
		 * the Linux PCMCIA code from David Hinds, working chipsets
		 * return 0x84 from their (correct) ID ports, while the broken
		 * ones would need to be probed at the new offset we set after
		 * we assume it's broken.
		 */
		if (slotnum == 1 && maybe_vlsi && sp->getb(sp, PCIC_ID_REV) != 0x84) {
			sp->index += 4;
			sp->data += 4;
			sp->offset = PCIC_SLOT_SIZE << 1;
		}
		/*
		 * see if there's a PCMCIA controller here
		 * Intel PCMCIA controllers use 0x82 and 0x83
		 * IBM clone chips use 0x88 and 0x89, apparently
		 */
		c = sp->getb(sp, PCIC_ID_REV);
		sp->revision = -1;
		switch(c) {
		/*
		 *	82365 or clones.
		 */
		case 0x82:
		case 0x83:
			sp->controller = PCIC_I82365;
			sp->revision = c & 1;
			/*
			 *	Now check for VADEM chips.
			 */
			outb(sp->index, 0x0E);
			outb(sp->index, 0x37);
			setb(sp, 0x3A, 0x40);
			c = sp->getb(sp, PCIC_ID_REV);
			if (c & 0x08) {
				switch (sp->revision = c & 7) {
				case 1:
					sp->controller = PCIC_VG365;
					break;
				case 2:
					sp->controller = PCIC_VG465;
					break;
				case 3:
					sp->controller = PCIC_VG468;
					break;
				default:
					sp->controller = PCIC_VG469;
					break;
				}
				clrb(sp, 0x3A, 0x40);
			}

			/*
			 * Check for RICOH RF5C396 PCMCIA Controller
			 */
			c = sp->getb(sp, 0x3a);
			if (c == 0xb2) {
				sp->controller = PCIC_RF5C396;
			}

			break;
		/*
		 *	VLSI chips.
		 */
		case 0x84:
			sp->controller = PCIC_VLSI;
			maybe_vlsi = 1;
			break;
		case 0x88:
		case 0x89:
			sp->controller = PCIC_IBM;
			sp->revision = c & 1;
			break;
		case 0x8a:
			sp->controller = PCIC_IBM_KING;
			sp->revision = c & 1;
			break;
		default:
			continue;
		}
		/*
		 *	Check for Cirrus logic chips.
		 */
		sp->putb(sp, 0x1F, 0);
		c = sp->getb(sp, 0x1F);
		if ((c & 0xC0) == 0xC0) {
			c = sp->getb(sp, 0x1F);
			if ((c & 0xC0) == 0) {
				if (c & 0x20)
					sp->controller = PCIC_PD672X;
				else
					sp->controller = PCIC_PD6710;
				sp->revision = 8 - ((c & 0x1F) >> 2);
			}
		}
		switch(sp->controller) {
		case PCIC_I82365:
			name = "Intel i82365";
			break;
		case PCIC_IBM:
			name = "IBM PCIC";
			break;
		case PCIC_IBM_KING:
			name = "IBM KING PCMCIA Controller";
			break;
		case PCIC_PD672X:
			name = "Cirrus Logic PD672X";
			break;
		case PCIC_PD6710:
			name = "Cirrus Logic PD6710";
			break;
		case PCIC_VG365:
			name = "Vadem 365";
			break;
		case PCIC_VG465:
			name = "Vadem 465";
			break;
		case PCIC_VG468:
			name = "Vadem 468";
			break;
		case PCIC_VG469:
			name = "Vadem 469";
			break;
		case PCIC_RF5C396:
			name = "Ricoh RF5C396";
			break;
		case PCIC_VLSI:
			name = "VLSI 82C146";
			break;
		default:
			name = "Unknown!";
			break;
		}
		device_set_desc(dev, name);
		/*
		 *	OK it seems we have a PCIC or lookalike.
		 *	Allocate a slot and initialise the data structures.
		 */
		validslots++;
		sp->slotnum = slotnum;
		slt = pccard_alloc_slot(&cinfo);
		if (slt == 0)
			continue;
		slt->cdata = sp;
		sp->slt = slt;
		/*
		 * If we haven't allocated an interrupt for the controller,
		 * then attempt to get one.
		 */
		if (pcic_irq == 0) {
			/* See if the user has requested a specific IRQ */
			if (!getenv_int("machdep.pccard.pcic_irq", &pcic_irq))
				pcic_irq = 0;
			rid = 0;
			res = bus_alloc_resource(dev, SYS_RES_IRQ, 
			    &rid, pcic_irq, ~0, 1, RF_ACTIVE);
			if (res) {
				error = bus_setup_intr(dev, res, 
				    INTR_TYPE_MISC, pcicintr, NULL, &ih);
				if (error) {
					bus_release_resource(dev, SYS_RES_IRQ,
					    rid, res);
					return error;
				}
				pcic_irq = rman_get_start(res);
			} else {
				printf("pcic: polling, can't alloc %d\n",
				    pcic_irq);
			}
		}
		/*
		 * Modem cards send the speaker audio (dialing noises)
		 * to the host's speaker.  Cirrus Logic PCIC chips must
		 * enable this.  There is also a Low Power Dynamic Mode bit
		 * that claims to reduce power consumption by 30%, so
		 * enable it and hope for the best.
		 */
		if (sp->controller == PCIC_PD672X) {
			setb(sp, PCIC_MISC1, PCIC_SPKR_EN);
			setb(sp, PCIC_MISC2, PCIC_LPDM_EN);
		}
		/*
		 *	Check for a card in this slot.
		 */
		setb(sp, PCIC_POWER, PCIC_PCPWRE| PCIC_DISRST);
		if ((sp->getb(sp, PCIC_STATUS) & PCIC_CD) != PCIC_CD) {
			slt->laststate = slt->state = empty;
		} else {
			slt->laststate = slt->state = filled;
			pccard_event(sp->slt, card_inserted);
		}
		/*
		 *	Assign IRQ for slot changes
		 */
		if (pcic_irq > 0)
			sp->putb(sp, PCIC_STAT_INT, (pcic_irq << 4) | 0xF);
	}
	if (validslots && pcic_irq <= 0)
		pcictimeout_ch = timeout(pcictimeout, 0, hz/2);
	if (validslots) {
		for (i = 0; i < validslots; i++) {
			device_add_child(dev, NULL, -1, NULL);
		}
	}
	return(validslots ? 0 : ENXIO);
}

/*
 *	ioctl calls - Controller specific ioctls
 */
static int
pcic_ioctl(struct slot *slt, int cmd, caddr_t data)
{
	struct pcic_slot *sp = slt->cdata;

	switch(cmd) {
	default:
		return(ENOTTY);
	/*
	 * Get/set PCIC registers
	 */
	case PIOCGREG:
		((struct pcic_reg *)data)->value =
			sp->getb(sp, ((struct pcic_reg *)data)->reg);
		break;
	case PIOCSREG:
		sp->putb(sp, ((struct pcic_reg *)data)->reg,
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
pcic_power(struct slot *slt)
{
	unsigned char reg = PCIC_DISRST|PCIC_PCPWRE;
	struct pcic_slot *sp = slt->cdata;

	switch(sp->controller) {
	case PCIC_PD672X:
	case PCIC_PD6710:
	case PCIC_VG365:
	case PCIC_VG465:
	case PCIC_VG468:
	case PCIC_VG469:
	case PCIC_RF5C396:
	case PCIC_VLSI:
	case PCIC_IBM_KING:
		switch(slt->pwr.vpp) {
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
		switch(slt->pwr.vcc) {
		default:
			return(EINVAL);
		case 0:
			break;
		case 33:
			if (sp->controller == PCIC_IBM_KING) {
				reg |= PCIC_VCC_5V_KING;
				break;
			}
			reg |= PCIC_VCC_3V;
			if ((sp->controller == PCIC_VG468) ||
				(sp->controller == PCIC_VG469) ||
				(sp->controller == PCIC_VG465) ||
				(sp->controller == PCIC_VG365))
				setb(sp, 0x2f, 0x03) ;
			else
				setb(sp, 0x16, 0x02);
			break;
		case 50:
                        if (sp->controller == PCIC_IBM_KING) {
                                reg |= PCIC_VCC_5V_KING;
                                break;
                        }
			reg |= PCIC_VCC_5V;
			if ((sp->controller == PCIC_VG468) ||
				(sp->controller == PCIC_VG469) ||
				(sp->controller == PCIC_VG465) ||
				(sp->controller == PCIC_VG365))
				clrb(sp, 0x2f, 0x03) ;
			else
				clrb(sp, 0x16, 0x02);
			break;
		}
		break;
	}
	sp->putb(sp, PCIC_POWER, reg);
	DELAY(300*1000);
	if (slt->pwr.vcc) {
		reg |= PCIC_OUTENA;
		sp->putb(sp, PCIC_POWER, reg);
		DELAY(100*1000);
	}
	/* Some chips are smarter than us it seems, so if we weren't
	 * allowed to use 5V, try 3.3 instead
	 */
	if (!(sp->getb(sp, PCIC_STATUS) &  0x40) && slt->pwr.vcc == 50) {
		slt->pwr.vcc = 33;
		slt->pwr.vpp = 0;
		return (pcic_power(slt));
	}
	return(0);
}

/*
 * tell the PCIC which irq we want to use.  only the following are legal:
 * 3, 4, 5, 7, 9, 10, 11, 12, 14, 15
 */
static void
pcic_mapirq(struct slot *slt, int irq)
{
	struct pcic_slot *sp = slt->cdata;
	if (irq == 0)
		clrb(sp, PCIC_INT_GEN, 0xF);
	else
		sp->putb(sp, PCIC_INT_GEN, 
		    (sp->getb(sp, PCIC_INT_GEN) & 0xF0) | irq);
}

/*
 *	pcic_reset - Reset the card and enable initial power.
 */
static void
pcic_reset(void *chan)
{
	struct slot *slt = chan;
	struct pcic_slot *sp = slt->cdata;

	switch (slt->insert_seq) {
	    case 0: /* Something funny happended on the way to the pub... */
		return;
	    case 1: /* Assert reset */
		clrb(sp, PCIC_INT_GEN, PCIC_CARDRESET);
		slt->insert_seq = 2;
		timeout(pcic_reset, (void *)slt, hz/4);
		return;
	    case 2: /* Deassert it again */
		setb(sp, PCIC_INT_GEN, PCIC_CARDRESET|PCIC_IOCARD);
		slt->insert_seq = 3;
		timeout(pcic_reset, (void *)slt, hz/4);
		return;
	    case 3: /* Wait if card needs more time */
		if (!sp->getb(sp, PCIC_STATUS) & PCIC_READY) {
			timeout(pcic_reset, (void *)slt, hz/10);
			return;
		}
	}
	slt->insert_seq = 0;
	if (sp->controller == PCIC_PD672X || sp->controller == PCIC_PD6710) {
		sp->putb(sp, PCIC_TIME_SETUP0, 0x1);
		sp->putb(sp, PCIC_TIME_CMD0, 0x6);
		sp->putb(sp, PCIC_TIME_RECOV0, 0x0);
		sp->putb(sp, PCIC_TIME_SETUP1, 1);
		sp->putb(sp, PCIC_TIME_CMD1, 0xf);
		sp->putb(sp, PCIC_TIME_RECOV1, 0);
	}
	selwakeup(&slt->selp);
}

/*
 *	pcic_disable - Disable the slot.
 */
static void
pcic_disable(struct slot *slt)
{
	struct pcic_slot *sp = slt->cdata;

	sp->putb(sp, PCIC_INT_GEN, 0);
	sp->putb(sp, PCIC_POWER, 0);
}

/*
 *	PCIC timer.  If the controller doesn't have a free IRQ to use
 *	or if interrupt steering doesn't work, poll the controller for
 *	insertion/removal events.
 */
static void
pcictimeout(void *chan)
{
	pcicintr(0);
	pcictimeout_ch = timeout(pcictimeout, 0, hz/2);
}

/*
 *	PCIC Interrupt handler.
 *	Check each slot in turn, and read the card status change
 *	register. If this is non-zero, then a change has occurred
 *	on this card, so send an event to the main code.
 */
static void
pcicintr(void *arg)
{
	int	slot, s;
	unsigned char chg;
	struct pcic_slot *sp = pcic_slots;
	static int count = 0;

	if (count++ > 20)
		Debugger("Trapdoor");	

	s = splhigh();
printf("splhigh for pcicintr\n");
	for (slot = 0; slot < PCIC_MAX_SLOTS; slot++, sp++)
		if (sp->slt && (chg = sp->getb(sp, PCIC_STAT_CHG)) != 0)
			if (chg & PCIC_CDTCH) {
				if ((sp->getb(sp, PCIC_STATUS) & PCIC_CD) ==
						PCIC_CD) {
					pccard_event(sp->slt, card_inserted);
				} else {
					pccard_event(sp->slt, card_removed);
				}
			}
printf("splx\n");
	splx(s);
}

/*
 *	pcic_resume - Suspend/resume support for PCIC
 */
static void
pcic_resume(struct slot *slt)
{
	struct pcic_slot *sp = slt->cdata;

	if (pcic_irq > 0)
		sp->putb(sp, PCIC_STAT_INT, (pcic_irq << 4) | 0xF);
	if (sp->controller == PCIC_PD672X) {
		setb(sp, PCIC_MISC1, PCIC_SPKR_EN);
		setb(sp, PCIC_MISC2, PCIC_LPDM_EN);
	}
}

static int
pcic_activate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	struct pccard_devinfo *devi = device_get_ivars(child);
	struct io_desc *ip = &devi->slt->io[rid];
	int err;

	switch (type) {
	case SYS_RES_IOPORT:
#if 0
		if (ip->flags & IODF_ACTIVE)
			return EBUSY;
#endif
		ip->flags |= IODF_ACTIVE;
		ip->start = rman_get_start(r);
		ip->size = rman_get_end(r) - rman_get_start(r) + 1;
		err = pcic_io(devi->slt, rid);
		if (err) {
			return err;
		}
		break;
	case SYS_RES_IRQ:
		pcic_mapirq(devi->slt, rman_get_start(r));
		break;
	case SYS_RES_MEMORY:
	default:
		break;
	}
	err = bus_generic_activate_resource(dev, child, type, rid, r);
	return err;
}

static int
pcic_deactivate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	struct pccard_devinfo *devi = device_get_ivars(child);
	struct io_desc *ip = &devi->slt->io[rid];
	int err;

	switch (type) {
	case SYS_RES_IOPORT:
#if 0
		if (ip->flags & IODF_ACTIVE)
			return EBUSY;
#endif
		ip->flags &= ~IODF_ACTIVE;
		err = pcic_io(devi->slt, rid);
		if (err) {
			return err;
		}
		break;
	case SYS_RES_IRQ:
		pcic_mapirq(devi->slt, 0);
		break;
	case SYS_RES_MEMORY:
	default:
		break;
	}
	err = bus_generic_deactivate_resource(dev, child, type, rid, r);
	return err;
}

static int
pcic_setup_intr(device_t dev, device_t child, struct resource *irq, 
    int flags, driver_intr_t *intr, void *arg, void **cookiep)
{
	return (bus_generic_setup_intr(dev, child, irq, flags, intr, arg, 
	    cookiep));
}
 
static int
pcic_teardown_intr(device_t dev, device_t child, struct resource *irq,
    void *cookie)
{
	return (bus_generic_teardown_intr(dev, child, irq, cookie));
}

static device_method_t pcic_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pcic_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, pcic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, pcic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	pcic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	pcic_teardown_intr),

	{ 0, 0 }
};

devclass_t	pcic_devclass;

static driver_t pcic_driver = {
	"pcic",
	pcic_methods,
	1,			/* no softc */
};

DRIVER_MODULE(pcic, isa, pcic_driver, pcic_devclass, 0, 0);
