/*
 *  Intel PCIC or compatible Controller driver
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
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <pccard/i82365.h>
#include <pccard/cardinfo.h>
#include <pccard/slot.h>
#ifdef	PC98
#include <pccard/pcic98reg.h>
#ifndef PCIC98_IOBASE
#define PCIC98_IOBASE   0x80d0
#endif
#endif /* PC98 */

/* Get pnp IDs */
#include <isa/isavar.h>
#include <dev/pcic/i82365reg.h>

#include <dev/pccard/pccardvar.h>
#include "card_if.h"

/*
 *	Prototypes for interrupt handler.
 */
static driver_intr_t	pcicintr;
static int		pcic_ioctl(struct slot *, int, caddr_t);
static int		pcic_power(struct slot *);
static void		pcic_mapirq(struct slot *, int);
static timeout_t 	pcic_reset;
static void		pcic_resume(struct slot *);
static void		pcic_disable(struct slot *);
static timeout_t 	pcictimeout;
static struct callout_handle pcictimeout_ch
    = CALLOUT_HANDLE_INITIALIZER(&pcictimeout_ch);
static int		pcic_memory(struct slot *, int);
static int		pcic_io(struct slot *, int);
#ifdef PC98
/* local functions for PC-98 Original PC-Card controller */
static int		pcic98_power(struct slot *);
static void		pcic98_mapirq(struct slot *, int);
static int		pcic98_memory(struct slot *, int);
static int		pcic98_io(struct slot *, int);
static timeout_t 	pcic98_reset;
static void		pcic98_disable(struct slot *);
static void		pcic98_resume(struct slot *);
#endif /* PC98 */

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

static struct slot_ctrl cinfo;

static struct isa_pnp_id pcic_ids[] = {
	{PCIC_PNP_ACTIONTEC,            NULL},          /* AEI0218 */
	{PCIC_PNP_IBM3765,		NULL},		/* IBM3765 */
	{PCIC_PNP_82365,		NULL},		/* PNP0E00 */
	{PCIC_PNP_CL_PD6720,		NULL},		/* PNP0E01 */
	{PCIC_PNP_VLSI_82C146,		NULL},		/* PNP0E02 */
	{PCIC_PNP_82365_CARDBUS,	NULL},		/* PNP0E03 */
	{PCIC_PNP_SCM_SWAPBOX,		NULL},		/* SCM0469 */ 
	{0}
};

static int validunits = 0;
#ifdef PC98
static	u_char		pcic98_last_reg1;
#endif /* PC98 */

#define GET_UNIT(d)	*(int *)device_get_softc(d)
#define SET_UNIT(d,u)	*(int *)device_get_softc(d) = (u)

static char *bridges[] =
{
	"Intel i82365",
	"IBM PCIC",
	"VLSI 82C146",
	"Cirrus logic 672x",
	"Cirrus logic 6710",
	"Vadem 365",
	"Vadem 465",
	"Vadem 468",
	"Vadem 469",
	"Ricoh RF5C396",
	"IBM KING PCMCIA Controller",
	"PC-98 MECIA Controller"
};

/*
 * Read a register from the PCIC.
 */
static unsigned char
getb1(struct pcic_slot *sp, int reg)
{
	outb(sp->index, sp->offset + reg);
	return (inb(sp->data));
}

/*
 * Write a register on the PCIC
 */
static void
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
	int reg = win * PCIC_MEMSIZE + PCIC_MEMBASE;

	if (win < 0 || win >= slt->ctrl->maxmem) {
		printf("Illegal PCIC MEMORY window request %d\n", win);
		return (ENXIO);
	}
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
	return (0);
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
	if (bootverbose) {
		printf("pcic: I/O win %d flags %x %x-%x\n", win, ip->flags,
		    ip->start, ip->start+ip->size-1);
	}

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
		printf("Illegal PCIC I/O window request %d\n", win);
		return (ENXIO);
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
	return (0);
}

/*
 *	Look for an Intel PCIC (or compatible).
 *	For each available slot, allocate a PC-CARD slot.
 */

/*
 *	VLSI 82C146 has incompatibilities about the I/O address of slot 1.
 *	Assume it's the only PCIC whose vendor ID is 0x84,
 *	contact Warner Losh <imp@freebsd.org> if correct.
 */
static int
pcic_probe(device_t dev)
{
	int slotnum, validslots = 0;
	struct pcic_slot *sp;
	unsigned char c;
	int error;
	struct resource *r;
	int rid;
	static int maybe_vlsi = 0;

	/* Check isapnp ids */
	error = ISA_PNP_PROBE(device_get_parent(dev), dev, pcic_ids);
	if (error == ENXIO)
		return (ENXIO);

	/*
	 *	Initialise controller information structure.
	 */
	cinfo.mapirq = pcic_mapirq;
	cinfo.mapmem = pcic_memory;
	cinfo.mapio = pcic_io;
	cinfo.ioctl = pcic_ioctl;
	cinfo.power = pcic_power;
	cinfo.reset = pcic_reset;
	cinfo.disable = pcic_disable;
	cinfo.resume = pcic_resume;
	cinfo.maxmem = PCIC_MEM_WIN;
	cinfo.maxio = PCIC_IO_WIN;

	if (bus_get_resource_start(dev, SYS_RES_IOPORT, 0) == 0)
		bus_set_resource(dev, SYS_RES_IOPORT, 0, PCIC_INDEX0, 2);
	rid = 0;
	r = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1, RF_ACTIVE);
	if (!r) {
		if (bootverbose)
			device_printf(dev, "Cannot get I/O range\n");
		return (ENOMEM);
	}

	sp = &pcic_slots[validunits * PCIC_CARD_SLOTS];
	for (slotnum = 0; slotnum < PCIC_CARD_SLOTS; slotnum++, sp++) {
		/*
		 *	Initialise the PCIC slot table.
		 */
		sp->getb = getb1;
		sp->putb = putb1;
		sp->index = rman_get_start(r);
		sp->data = sp->index + 1;
		sp->offset = slotnum * PCIC_SLOT_SIZE;
		/*
		 * XXX - Screwed up slot 1 on the VLSI chips.  According to
		 * the Linux PCMCIA code from David Hinds, working chipsets
		 * return 0x84 from their (correct) ID ports, while the broken
		 * ones would need to be probed at the new offset we set after
		 * we assume it's broken.
		 */
		if (slotnum == 1 && maybe_vlsi &&
		    sp->getb(sp, PCIC_ID_REV) != PCIC_VLSI82C146) {
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
		case PCIC_INTEL0:
		case PCIC_INTEL1:
			sp->controller = PCIC_I82365;
			sp->revision = c & 1;
			/*
			 *	Now check for VADEM chips.
			 */
			outb(sp->index, 0x0E);	/* Unlock VADEM's extra regs */
			outb(sp->index, 0x37);
			setb(sp, PCIC_VMISC, PCIC_VADEMREV);
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
				clrb(sp, PCIC_VMISC, PCIC_VADEMREV);
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
		case PCIC_VLSI82C146:
			sp->controller = PCIC_VLSI;
			maybe_vlsi = 1;
			break;
		case PCIC_IBM1:
		case PCIC_IBM2:
			sp->controller = PCIC_IBM;
			sp->revision = c & 1;
			break;
		case PCIC_IBM3:
			sp->controller = PCIC_IBM_KING;
			sp->revision = c & 1;
			break;
		default:
			continue;
		}
		/*
		 *	Check for Cirrus logic chips.
		 */
		sp->putb(sp, PCIC_CLCHIP, 0);
		c = sp->getb(sp, PCIC_CLCHIP);
		if ((c & PCIC_CLC_TOGGLE) == PCIC_CLC_TOGGLE) {
			c = sp->getb(sp, PCIC_CLCHIP);
			if ((c & PCIC_CLC_TOGGLE) == 0) {
				if (c & PCIC_CLC_DUAL)
					sp->controller = PCIC_PD672X;
				else
					sp->controller = PCIC_PD6710;
				sp->revision = 8 - ((c & 0x1F) >> 2);
			}
		}
		device_set_desc(dev, bridges[(int) sp->controller]);
		/*
		 *	OK it seems we have a PCIC or lookalike.
		 *	Allocate a slot and initialise the data structures.
		 */
		validslots++;
		sp->slotnum = slotnum + validunits * PCIC_CARD_SLOTS;
		sp->slt = (struct slot *) 1;
		/*
		 * Modem cards send the speaker audio (dialing noises)
		 * to the host's speaker.  Cirrus Logic PCIC chips must
		 * enable this.  There is also a Low Power Dynamic Mode bit
		 * that claims to reduce power consumption by 30%, so
		 * enable it and hope for the best.
		 */
		if (sp->controller == PCIC_PD672X) {
			setb(sp, PCIC_MISC1, PCIC_MISC1_SPEAKER);
			setb(sp, PCIC_MISC2, PCIC_LPDM_EN);
		}
	}
	bus_release_resource(dev, SYS_RES_IOPORT, rid, r);
	if (validslots != 0)
		return (0);
#ifdef  PC98    
	sp = &pcic_slots[validunits * PCIC_CARD_SLOTS];
	if (inb(PCIC98_REG0) != 0xff) {
		sp->controller	= PCIC_PC98;
		sp->revision	= 0;
		cinfo.mapmem	= pcic98_memory;
		cinfo.mapio	= pcic98_io;
		cinfo.power	= pcic98_power;
		cinfo.mapirq	= pcic98_mapirq;
		cinfo.reset	= pcic98_reset;
		cinfo.disable	= pcic98_disable;
		cinfo.resume	= pcic98_resume;
		cinfo.maxmem	= 1;
#if 0   
		cinfo.maxio	= 1;
#else   
		cinfo.maxio	= 2;	/* fake for UE2212 LAN card */
#endif  
		validslots++;
		sp->slt = (struct slot *) 1;
		/* XXX need to allocated the port resources */
		device_set_desc(dev, "MECIA PC98 Original PCMCIA Controller");
	}
#endif  /* PC98 */
	return (validslots ? 0 : ENXIO);
}

static void
do_mgt_irq(struct pcic_slot *sp, int irq)
{
	/* Management IRQ changes */
	clrb(sp, PCIC_INT_GEN, PCIC_INTR_ENA);
	sp->putb(sp, PCIC_STAT_INT, (irq << 4) | 0xF);
}

static int
pcic_attach(device_t dev)
{
	int		error;
	int		irq;
	int		i;
	void		*ih;
	device_t	kid;
	struct resource *r;
	int		rid;
	struct slot	*slt;
	struct pcic_slot *sp;
	int		stat;
	
	SET_UNIT(dev, validunits);
	sp = &pcic_slots[GET_UNIT(dev) * PCIC_CARD_SLOTS];
	for (i = 0; i < PCIC_CARD_SLOTS; i++, sp++) {
		if (!sp->slt)
			continue;
		sp->slt = 0;
		kid = device_add_child(dev, NULL, -1);
		if (kid == NULL) {
			device_printf(dev, "Can't add pccard bus slot %d", i);
			return (ENXIO);
		}
		device_probe_and_attach(kid);
		slt = pccard_init_slot(kid, &cinfo);
		if (slt == 0) {
			device_printf(dev, "Can't get pccard info slot %d", i);
			return (ENXIO);
		}
		slt->cdata = sp;
		sp->slt = slt;
	}
	validunits++;

	rid = 0;
	r = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1, RF_ACTIVE);
	if (!r)
		return (ENXIO);

	irq = bus_get_resource_start(dev, SYS_RES_IRQ, 0);
	if (irq == 0) {
		/* See if the user has requested a specific IRQ */
		if (!getenv_int("machdep.pccard.pcic_irq", &irq))
			irq = 0;
	}
	rid = 0;
	r = 0;
	if (irq > 0) {
		r = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, irq,
		    irq, 1, RF_ACTIVE);
	}
	if (r && ((1 << (rman_get_start(r))) & PCIC_INT_MASK_ALLOWED) == 0) {
		device_printf(dev,
		    "Hardware does not support irq %d, trying polling.\n",
		    irq);
		bus_release_resource(dev, SYS_RES_IRQ, rid, r);
		r = 0;
		irq = 0;
	}
	if (r) {
		error = bus_setup_intr(dev, r, INTR_TYPE_MISC,
		    pcicintr, (void *) GET_UNIT(dev), &ih);
		if (error) {
			bus_release_resource(dev, SYS_RES_IRQ, rid, r);
			return (error);
		}
		irq = rman_get_start(r);
		device_printf(dev, "management irq %d\n", irq);
	} else {
		irq = 0;
	}
	if (irq == 0) {
		pcictimeout_ch = timeout(pcictimeout, (void *) GET_UNIT(dev),
		    hz/2);
		device_printf(dev, "Polling mode\n");
	}

	sp = &pcic_slots[GET_UNIT(dev) * PCIC_CARD_SLOTS];
	for (i = 0; i < PCIC_CARD_SLOTS; i++, sp++) {
		if (sp->slt == NULL)
			continue;

#ifdef PC98
		if (sp->controller == PCIC_PC98) {
			pcic98_last_reg1 = inb(PCIC98_REG1);
			if (pcic98_last_reg1 & PCIC98_CARDEXIST) {
				/* PCMCIA card exist */
				sp->slt->laststate = sp->slt->state = filled;
				pccard_event(sp->slt, card_inserted);
			} else {
				sp->slt->laststate = sp->slt->state = empty;
			}
		} else
#endif /* PC98 */
		{
			do_mgt_irq(sp, irq);

			/* Check for changes */
			setb(sp, PCIC_POWER, PCIC_PCPWRE| PCIC_DISRST);
			stat = sp->getb(sp, PCIC_STATUS);
			if (bootverbose)
			    printf("stat is %x\n", stat);
			if ((stat & PCIC_CD) != PCIC_CD) {
			    sp->slt->laststate = sp->slt->state = empty;
			} else {
			    sp->slt->laststate = sp->slt->state = filled;
			    pccard_event(sp->slt, card_inserted);
			}
		}
		sp->slt->irq = irq;
	}

	return (bus_generic_attach(dev));
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
		return (ENOTTY);
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
	return (0);
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
	case PCIC_I82365:
		switch(slt->pwr.vpp) {
		default:
			return (EINVAL);
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
			return (EINVAL);
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
				setb(sp, PCIC_CVSR, PCIC_CVSR_VS);
			else
				setb(sp, PCIC_MISC1, PCIC_MISC1_VCC_33);
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
				clrb(sp, PCIC_CVSR, PCIC_CVSR_VS);
			else
				clrb(sp, PCIC_MISC1, PCIC_MISC1_VCC_33);
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
	if (!(sp->getb(sp, PCIC_STATUS) & PCIC_POW) && slt->pwr.vcc == 50) {
		slt->pwr.vcc = 33;
		slt->pwr.vpp = 0;
		return (pcic_power(slt));
	}
	return (0);
}

/*
 * tell the PCIC which irq we want to use.  only the following are legal:
 * 3, 4, 5, 7, 9, 10, 11, 12, 14, 15.  We require the callers of this
 * routine to do the check for legality.
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
		timeout(cinfo.reset, (void *)slt, hz/4);
		return;
	    case 2: /* Deassert it again */
		setb(sp, PCIC_INT_GEN, PCIC_CARDRESET|PCIC_IOCARD);
		slt->insert_seq = 3;
		timeout(cinfo.reset, (void *)slt, hz/4);
		return;
	    case 3: /* Wait if card needs more time */
		if (!sp->getb(sp, PCIC_STATUS) & PCIC_READY) {
			timeout(cinfo.reset, (void *)slt, hz/10);
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
	pcicintr(chan);
	pcictimeout_ch = timeout(pcictimeout, chan, hz/2);
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
	int unit = (int) arg;
	struct pcic_slot *sp = &pcic_slots[unit * PCIC_CARD_SLOTS];

	s = splhigh();
#ifdef	PC98
	if (sp->controller == PCIC_PC98) {
	    	u_char reg1;
		/* Check for a card in this slot */
		reg1 = inb(PCIC98_REG1);
		if ((pcic98_last_reg1 ^ reg1) & PCIC98_CARDEXIST) {
			pcic98_last_reg1 = reg1;
			if (reg1 & PCIC98_CARDEXIST)
				pccard_event(sp->slt, card_inserted);
			else
				pccard_event(sp->slt, card_removed);
		}
	} else
#endif	/* PC98 */
	{
		for (slot = 0; slot < PCIC_CARD_SLOTS; slot++, sp++) {
			if (sp->slt &&
			    (chg = sp->getb(sp, PCIC_STAT_CHG)) != 0) {
				if (bootverbose)
					printf("Slot %d chg = 0x%x\n", slot,
					    chg);
				if (chg & PCIC_CDTCH) {
					if ((sp->getb(sp, PCIC_STATUS) &
					    PCIC_CD) == PCIC_CD) {
						pccard_event(sp->slt,
						    card_inserted);
					} else {
						pccard_event(sp->slt,
						    card_removed);
						cinfo.disable(sp->slt);
					}
				}
			}
		}
	}
	splx(s);
}

/*
 *	pcic_resume - Suspend/resume support for PCIC
 */
static void
pcic_resume(struct slot *slt)
{
	struct pcic_slot *sp = slt->cdata;

	do_mgt_irq(sp, slt->irq);
	if (sp->controller == PCIC_PD672X) {
		setb(sp, PCIC_MISC1, PCIC_MISC1_SPEAKER);
		setb(sp, PCIC_MISC2, PCIC_LPDM_EN);
	}
}

#ifdef PC98
/*
 * local functions for PC-98 Original PC-Card controller
 */
#define	PCIC98_ALWAYS_128MAPPING	1	/* trick for using UE2212  */

int pcic98_mode = 0;	/* almost the same as the value in PCIC98_REG2 */

static unsigned char reg_winsel = PCIC98_UNMAPWIN;
static unsigned short reg_pagofs = 0;

static int
pcic98_memory(struct slot *slt, int win)
{
	struct mem_desc *mp = &slt->mem[win];
	unsigned char x;

	if (mp->flags & MDF_ACTIVE) {
		/* slot = 0, window = 0, sys_addr = 0xda000, length = 8KB */
		if ((unsigned long)mp->start != 0xda000) {
			printf(
			"sys_addr must be 0xda000. requested address = %p\n",
			mp->start);
			return (EINVAL);
		}

		/* omajinai ??? */
		outb(PCIC98_REG0, 0);
		x = inb(PCIC98_REG1);
		x &= 0xfc;
		x |= 0x02;
		outb(PCIC98_REG1, x);
		reg_winsel = inb(PCIC98_REG_WINSEL);
		reg_pagofs = inw(PCIC98_REG_PAGOFS);
		outb(PCIC98_REG_WINSEL, PCIC98_MAPWIN);
		outw(PCIC98_REG_PAGOFS, (mp->card >> 13)); /* 8KB */

		if (mp->flags & MDF_ATTR) {
			outb(PCIC98_REG7, inb(PCIC98_REG7) | PCIC98_ATTRMEM);
		}else{
			outb(PCIC98_REG7, inb(PCIC98_REG7) & (~PCIC98_ATTRMEM));
		}

		outb(PCIC98_REG_WINSEL, PCIC98_MAPWIN);
#if 0
		if ((mp->flags & MDF_16BITS) == 1) {	/* 16bit */
			outb(PCIC98_REG2, inb(PCIC98_REG2) & (~PCIC98_8BIT));
		}else{					/* 8bit */
			outb(PCIC98_REG2, inb(PCIC98_REG2) | PCIC98_8BIT);
		}
#endif
	} else {  /* !(mp->flags & MDF_ACTIVE) */
		outb(PCIC98_REG0, 0);
		x = inb(PCIC98_REG1);
		x &= 0xfc;
		x |= 0x02;
		outb(PCIC98_REG1, x);
#if 0
		outb(PCIC98_REG_WINSEL, PCIC98_UNMAPWIN);
		outw(PCIC98_REG_PAGOFS, 0);
#else
		outb(PCIC98_REG_WINSEL, reg_winsel);
		outw(PCIC98_REG_PAGOFS, reg_pagofs);
#endif
	}
	return (0);
}

static int
pcic98_io(struct slot *slt, int win)
{
	struct io_desc *ip = &slt->io[win];
	unsigned char x;
	unsigned short cardbase;
	u_short ofst;

	if (win != 0) {
		/* ignore for UE2212 */
		printf(
		"pcic98:Illegal PCIC I/O window(%d) request! Ignored.\n", win);
/*		return (EINVAL);*/
		return (0);
	}

	if (ip->flags & IODF_ACTIVE) {
		x = inb(PCIC98_REG2) & 0x0f;
#if 0
		if (! (ip->flags & IODF_CS16))
			x |= PCIC98_8BIT;
#else
		if (! (ip->flags & IODF_16BIT)) {
			x |= PCIC98_8BIT;
			pcic98_mode |= PCIC98_8BIT;
		}
#endif

		ofst = ip->start & 0xf;
		cardbase = ip->start & ~0xf;
#ifndef PCIC98_ALWAYS_128MAPPING
		if (ip->size + ofst > 16)
#endif
		{	/* 128bytes mapping */
			x |= PCIC98_MAP128;
			pcic98_mode |= PCIC98_MAP128;
			ofst |= ((cardbase & 0x70) << 4);
			cardbase &= ~0x70;
		}

		x |= PCIC98_MAPIO;
		outb(PCIC98_REG2, x);
    
		outw(PCIC98_REG4, PCIC98_IOBASE);	/* 98side I/O base */
		outw(PCIC98_REG5, cardbase);		/* card side I/O base */

		if (bootverbose) {
			printf("pcic98: I/O mapped 0x%04x(98) -> "
			       "0x%04x(Card) and width %d bytes\n",
				PCIC98_IOBASE+ofst, ip->start, ip->size);
			printf("pcic98: reg2=0x%02x reg3=0x%02x reg7=0x%02x\n",
				inb(PCIC98_REG2), inb(PCIC98_REG3),
				inb(PCIC98_REG7));
			printf("pcic98: mode=%d\n", pcic98_mode);
		}

		ip->start = PCIC98_IOBASE + ofst;
	} else {
		outb(PCIC98_REG2, inb(PCIC98_REG2) & (~PCIC98_MAPIO));
		pcic98_mode = 0;
	}
	return (0);
}

static int
pcic98_power(struct slot *slt)
{
	unsigned char reg;

	reg = inb(PCIC98_REG7) & (~PCIC98_VPP12V);
	switch(slt->pwr.vpp) {
	default:
		return (EINVAL);
	case 50:
		break;
	case 120:
		reg |= PCIC98_VPP12V;
		break;
	}
	outb(PCIC98_REG7, reg);
	DELAY(100*1000);

	reg = inb(PCIC98_REG2) & (~PCIC98_VCC3P3V);
	switch(slt->pwr.vcc) {
	default:
		return (EINVAL);
	case 33:
		reg |= PCIC98_VCC3P3V;
		break;
	case 50:
		break;
	}
	outb(PCIC98_REG2, reg);
	DELAY(100*1000);
	return (0);
}

static void
pcic98_mapirq(struct slot *slt, int irq)
{
	u_char x;

	switch (irq) {
	case 3:
		x = PCIC98_INT0;
		break;
	case 5:
		x = PCIC98_INT1;
		break;
	case 6:
		x = PCIC98_INT2;
		break;
	case 10:
		x = PCIC98_INT4;
		break;
	case 12:
		x = PCIC98_INT5;
		break;
	case 0:		/* disable */
		x = PCIC98_INTDISABLE;
		break;
	default:
		printf("pcic98: illegal irq %d\n", irq);
		return;
	}
#ifdef	PCIC_DEBUG
	printf("pcic98: irq=%d mapped.\n", irq);
#endif
	outb(PCIC98_REG3, x);
}

static void
pcic98_reset(void *chan)
{
	struct slot *slt = chan;

	outb(PCIC98_REG0, 0);
	outb(PCIC98_REG2, inb(PCIC98_REG2) & (~PCIC98_MAPIO));
	outb(PCIC98_REG3, PCIC98_INTDISABLE);
#if 0
/* pcic98_reset() is called after pcic98_power() */
	outb(PCIC98_REG2, inb(PCIC98_REG2) & (~PCIC98_VCC3P3V));
	outb(PCIC98_REG7, inb(PCIC98_REG7) & (~PCIC98_VPP12V));
#endif
	outb(PCIC98_REG1, 0);

	selwakeup(&slt->selp);
}

static void
pcic98_disable(struct slot *slt)
{
	/* null function */
}

static void
pcic98_resume(struct slot *slt)
{
	/* XXX PCIC98 How ? */
}
#endif	/* PC98 */
/* end of local functions for PC-98 Original PC-Card controller */

static int
pcic_activate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	struct pccard_devinfo *devi = device_get_ivars(child);
	int err;

	switch (type) {
	case SYS_RES_IOPORT: {
		struct io_desc *ip;
		ip = &devi->slt->io[rid];
		if (ip->flags == 0) {
			if (rid == 0)
				ip->flags = IODF_WS | IODF_16BIT | IODF_CS16;
			else
				ip->flags = devi->slt->io[0].flags;
		}
		ip->flags |= IODF_ACTIVE;
		ip->start = rman_get_start(r);
		ip->size = rman_get_end(r) - rman_get_start(r) + 1;
		err = cinfo.mapio(devi->slt, rid);
		if (err)
			return (err);
		break;
	}
	case SYS_RES_IRQ:
		/*
		 * We actually defer the activation of the IRQ resource
		 * until the interrupt is registered to avoid stray
		 * interrupt messages.
		 */
		break;
	case SYS_RES_MEMORY: {
		struct mem_desc *mp;
		if (rid >= NUM_MEM_WINDOWS)
			return (EINVAL);
		mp = &devi->slt->mem[rid];
		mp->flags |= MDF_ACTIVE;
		mp->start = (caddr_t) rman_get_start(r);
		mp->size = rman_get_end(r) - rman_get_start(r) + 1;
		err = cinfo.mapmem(devi->slt, rid);
		if (err)
			return (err);
		break;
	}
	default:
		break;
	}
	err = bus_generic_activate_resource(dev, child, type, rid, r);
	return (err);
}

static int
pcic_deactivate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	struct pccard_devinfo *devi = device_get_ivars(child);
	int err;

	switch (type) {
	case SYS_RES_IOPORT: {
		struct io_desc *ip = &devi->slt->io[rid];
		ip->flags &= ~IODF_ACTIVE;
		err = cinfo.mapio(devi->slt, rid);
		if (err)
			return (err);
		break;
	}
	case SYS_RES_IRQ:
		break;
	case SYS_RES_MEMORY: {
		struct mem_desc *mp = &devi->slt->mem[rid];
		mp->flags &= ~(MDF_ACTIVE | MDF_ATTR);
		err = cinfo.mapmem(devi->slt, rid);
		if (err)
			return (err);
		break;
	}
	default:
		break;
	}
	err = bus_generic_deactivate_resource(dev, child, type, rid, r);
	return (err);
}

static int
pcic_setup_intr(device_t dev, device_t child, struct resource *irq,
    int flags, driver_intr_t *intr, void *arg, void **cookiep)
{
	struct pccard_devinfo *devi = device_get_ivars(child);
	int err;

	if (((1 << rman_get_start(irq)) & PCIC_INT_MASK_ALLOWED) == 0) {
		device_printf(dev, "Hardware does not support irq %ld.\n",
		    rman_get_start(irq));
		return (EINVAL);
	}

	err = bus_generic_setup_intr(dev, child, irq, flags, intr, arg,
	    cookiep);
	if (err == 0)
		cinfo.mapirq(devi->slt, rman_get_start(irq));
	else
		device_printf(dev, "Error %d irq %ld\n", err,
		    rman_get_start(irq));
	return (err);
}

static int
pcic_teardown_intr(device_t dev, device_t child, struct resource *irq,
    void *cookie)
{
	struct pccard_devinfo *devi = device_get_ivars(child);

	cinfo.mapirq(devi->slt, 0);
	return (bus_generic_teardown_intr(dev, child, irq, cookie));
}

static int
pcic_set_res_flags(device_t bus, device_t child, int restype, int rid,
    u_long value)
{
	struct pccard_devinfo *devi = device_get_ivars(child);
	int err = 0;

	switch (restype) {
	case SYS_RES_MEMORY: {
		struct mem_desc *mp = &devi->slt->mem[rid];
		switch (value) {
		case PCCARD_A_MEM_COM:
			mp->flags &= ~MDF_ATTR;
			break;
		case PCCARD_A_MEM_ATTR:
			mp->flags |= MDF_ATTR;
			break;
		case PCCARD_A_MEM_8BIT:
			mp->flags &= ~MDF_16BITS;
			break;
		case PCCARD_A_MEM_16BIT:
			mp->flags |= MDF_16BITS;
			break;
		}
		err = cinfo.mapmem(devi->slt, rid);
		break;
	}
	default:
		err = EOPNOTSUPP;
	}
	return (err);
}

static int
pcic_get_res_flags(device_t bus, device_t child, int restype, int rid,
    u_long *value)
{
	struct pccard_devinfo *devi = device_get_ivars(child);
	int err = 0;

	if (value == 0)
		return (ENOMEM);

	switch (restype) {
	case SYS_RES_IOPORT: {
		struct io_desc *ip = &devi->slt->io[rid];
		*value = ip->flags;
		break;
	}
	case SYS_RES_MEMORY: {
		struct mem_desc *mp = &devi->slt->mem[rid];
		*value = mp->flags;
		break;
	}
	default:
		err = EOPNOTSUPP;
	}
	return (0);
}

static int
pcic_set_memory_offset(device_t bus, device_t child, int rid, u_int32_t offset,
    u_int32_t *deltap)
{
	struct pccard_devinfo *devi = device_get_ivars(child);
	struct mem_desc *mp = &devi->slt->mem[rid];

	mp->card = offset;
	if (deltap)
		*deltap = 0;			/* XXX BAD XXX */
	return (cinfo.mapmem(devi->slt, rid));
}

static int
pcic_get_memory_offset(device_t bus, device_t child, int rid, u_int32_t *offset)
{
	struct pccard_devinfo *devi = device_get_ivars(child);
	struct mem_desc *mp = &devi->slt->mem[rid];

	if (offset == 0)
		return (ENOMEM);

	*offset = mp->card;

	return (0);
}

static device_method_t pcic_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pcic_probe),
	DEVMETHOD(device_attach,	pcic_attach),
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

	/* Card interface */
	DEVMETHOD(card_set_res_flags,	pcic_set_res_flags),
	DEVMETHOD(card_get_res_flags,	pcic_get_res_flags),
	DEVMETHOD(card_set_memory_offset, pcic_set_memory_offset),
	DEVMETHOD(card_get_memory_offset, pcic_get_memory_offset),

	{ 0, 0 }
};

devclass_t	pcic_devclass;

static driver_t pcic_driver = {
	"pcic",
	pcic_methods,
	sizeof(int)
};

DRIVER_MODULE(pcic, isa, pcic_driver, pcic_devclass, 0, 0);
