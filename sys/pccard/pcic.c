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
#include <pccard/pcicvar.h>

/* Get pnp IDs */
#include <isa/isavar.h>
#include <dev/pcic/i82365reg.h>

#include <dev/pccard/pccardvar.h>
#include "card_if.h"

/*
 *	Prototypes for interrupt handler.
 */
static driver_intr_t	pcicintr;
static int		pcicintr1(void *);
static int		pcic_ioctl(struct slot *, int, caddr_t);
static int		pcic_power(struct slot *);
static void		pcic_mapirq(struct slot *, int);
static timeout_t 	pcic_reset;
static void		pcic_resume(struct slot *);
static void		pcic_disable(struct slot *);
static timeout_t 	pcictimeout;
static int		pcic_memory(struct slot *, int);
static int		pcic_io(struct slot *, int);

devclass_t	pcic_devclass;

static struct slot_ctrl pcic_cinfo = {
	pcic_mapirq,
	pcic_memory,
	pcic_io,
	pcic_reset,
	pcic_disable,
	pcic_power,
	pcic_ioctl,
	pcic_resume,
	PCIC_MEM_WIN,
	PCIC_IO_WIN
};

/*
 * Read a register from the PCIC.
 */
unsigned char
pcic_getb_io(struct pcic_slot *sp, int reg)
{
	bus_space_write_1(sp->bst, sp->bsh, PCIC_INDEX, sp->offset + reg);
	return (bus_space_read_1(sp->bst, sp->bsh, PCIC_DATA));
}

/*
 * Write a register on the PCIC
 */
void
pcic_putb_io(struct pcic_slot *sp, int reg, unsigned char val)
{
	/*
	 * Many datasheets recommend using outw rather than outb to save 
	 * a microsecond.  Maybe we should do this, but we'd likely only
	 * save 20-30us on card activation.
	 */
	bus_space_write_1(sp->bst, sp->bsh, PCIC_INDEX, sp->offset + reg);
	bus_space_write_1(sp->bst, sp->bsh, PCIC_DATA, val);
}

/*
 * Clear bit(s) of a register.
 */
__inline void
pcic_clrb(struct pcic_slot *sp, int reg, unsigned char mask)
{
	sp->putb(sp, reg, sp->getb(sp, reg) & ~mask);
}

/*
 * Set bit(s) of a register
 */
__inline void
pcic_setb(struct pcic_slot *sp, int reg, unsigned char mask)
{
	sp->putb(sp, reg, sp->getb(sp, reg) | mask);
}

/*
 * Write a 16 bit value to 2 adjacent PCIC registers
 */
static __inline void
pcic_putw(struct pcic_slot *sp, int reg, unsigned short word)
{
	sp->putb(sp, reg, word & 0xFF);
	sp->putb(sp, reg + 1, (word >> 8) & 0xff);
}

/*
 * Free up resources allocated so far.
 */
void
pcic_dealloc(device_t dev)
{
	struct pcic_softc *sc;

	sc = (struct pcic_softc *) device_get_softc(dev);
	if (sc->iores)
		bus_release_resource(dev, SYS_RES_IOPORT, sc->iorid,
		    sc->iores);
	if (sc->memres)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->memrid,
		    sc->memres);
	untimeout(pcictimeout, sc, sc->timeout_ch);
	if (sc->ih)
		bus_teardown_intr(dev, sc->irqres, sc->ih);
	if (sc->irqres)
		bus_release_resource(dev, SYS_RES_IRQ, sc->irqrid, sc->irqres);
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
		pcic_putw(sp, reg, sys_addr & 0xFFF);
		pcic_putw(sp, reg+2, (sys_addr + (mp->size >> 12) - 1) & 0xFFF);
		pcic_putw(sp, reg+4, ((mp->card >> 12) - sys_addr) & 0x3FFF);
		/*
		 *	Each 16 bit register has some flags in the upper bits.
		 */
		if (mp->flags & MDF_16BITS)
			pcic_setb(sp, reg+1, PCIC_DATA16);
		if (mp->flags & MDF_ZEROWS)
			pcic_setb(sp, reg+1, PCIC_ZEROWS);
		if (mp->flags & MDF_WS0)
			pcic_setb(sp, reg+3, PCIC_MW0);
		if (mp->flags & MDF_WS1)
			pcic_setb(sp, reg+3, PCIC_MW1);
		if (mp->flags & MDF_ATTR)
			pcic_setb(sp, reg+5, PCIC_REG);
		if (mp->flags & MDF_WP)
			pcic_setb(sp, reg+5, PCIC_WP);
		/*
		 * Enable the memory window. By experiment, we need a delay.
		 */
		pcic_setb(sp, PCIC_ADDRWINE, (1<<win) | PCIC_MEMCS16);
		DELAY(50);
	} else {
		pcic_clrb(sp, PCIC_ADDRWINE, 1<<win);
		pcic_putw(sp, reg, 0);
		pcic_putw(sp, reg+2, 0);
		pcic_putw(sp, reg+4, 0);
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

		pcic_putw(sp, reg, ip->start);
		pcic_putw(sp, reg+2, ip->start+ip->size-1);
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
		pcic_setb(sp, PCIC_ADDRWINE, mask);
		DELAY(100);
	} else {
		pcic_clrb(sp, PCIC_ADDRWINE, mask);
		DELAY(100);
		pcic_putw(sp, reg, 0);
		pcic_putw(sp, reg + 2, 0);
	}
	return (0);
}

static void
pcic_do_mgt_irq(struct pcic_slot *sp, int irq)
{
	/* Management IRQ changes */
	pcic_clrb(sp, PCIC_INT_GEN, PCIC_INTR_ENA);
	sp->putb(sp, PCIC_STAT_INT, (irq << 4) | 0xF);
}

int
pcic_attach(device_t dev)
{
	int		error;
	int		irq;
	int		i;
	device_t	kid;
	struct resource *r;
	int		rid;
	struct pcic_softc *sc;
	struct slot	*slt;
	struct pcic_slot *sp;
	int		stat;
	
	sc = (struct pcic_softc *) device_get_softc(dev);
	callout_handle_init(&sc->timeout_ch);
	sp = &sc->slots[0];
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
		slt = pccard_init_slot(kid, &pcic_cinfo);
		if (slt == 0) {
			device_printf(dev, "Can't get pccard info slot %d", i);
			return (ENXIO);
		}
		sc->slotmask |= (1 << i);
		slt->cdata = sp;
		sp->slt = slt;
		sp->sc = sc;
	}

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
	sc->irqrid = rid;
	sc->irqres = r;
	if (r) {
		error = bus_setup_intr(dev, r, INTR_TYPE_MISC,
		    pcicintr, (void *) sc, &sc->ih);
		if (error) {
			pcic_dealloc(dev);
			return (error);
		}
		irq = rman_get_start(r);
		device_printf(dev, "management irq %d\n", irq);
	} else {
		irq = 0;
	}
	if (irq == 0) {
		sc->timeout_ch = timeout(pcictimeout, (void *) sc, hz/2);
		device_printf(dev, "Polling mode\n");
	}

	sp = &sc->slots[0];
	for (i = 0; i < PCIC_CARD_SLOTS; i++, sp++) {
		if (sp->slt == NULL)
			continue;

		pcic_do_mgt_irq(sp, irq);

		/* Check for changes */
		pcic_setb(sp, PCIC_POWER, PCIC_PCPWRE | PCIC_DISRST);
		stat = sp->getb(sp, PCIC_STATUS);
		if (bootverbose)
			printf("stat is %x\n", stat);
		if ((stat & PCIC_CD) != PCIC_CD) {
			sp->slt->laststate = sp->slt->state = empty;
		} else {
			sp->slt->laststate = sp->slt->state = filled;
			pccard_event(sp->slt, card_inserted);
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
	unsigned char c;
	unsigned char reg = PCIC_DISRST | PCIC_PCPWRE;
	struct pcic_slot *sp = slt->cdata;
	struct pcic_softc *sc = sp->sc;

	if (sc->flags & (PCIC_DF_POWER | PCIC_AB_POWER)) {
		/* 
		 * Look at the VS[12]# bits on the card.  If VS1 is clear
		 * then we should apply 3.3 volts.
		 */
		c = sp->getb(sp, PCIC_CDGC);
		if ((c & PCIC_VS1STAT) == 0)
			slt->pwr.vcc = 33;
	}

	/*
	 * XXX Note: The Vpp controls varies quit a bit between bridge chips
	 * and the following might not be right in all cases.  The Linux
	 * code and wildboar code bases are more complex.  However, most
	 * applications want vpp == vcc and the following code does appear
	 * to do that for all bridge sets.
	 */
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

	if (slt->pwr.vcc)
		reg |= PCIC_VCC_ON;		/* Turn on Vcc */
	switch(slt->pwr.vcc) {
	default:
		return (EINVAL);
	case 0:
		break;
	case 33:
		/*
		 * The wildboar code has comments that state that
		 * the IBM KING controller doesn't support 3.3V
		 * on the "IBM Smart PC card drive".  The code
		 * intemates that's the only place they have seen
		 * it used and that there's a boatload of issues
		 * with it.  I'm not even sure this is right because
		 * the only docs I've been able to find say this is for
		 * 5V power.  Of course, this "doc" is just code comments
		 * so who knows for sure.
		 */
		if (sc->flags & PCIC_KING_POWER) {
			reg |= PCIC_VCC_5V_KING;
			break;
		}
		if (sc->flags & PCIC_VG_POWER) {
			pcic_setb(sp, PCIC_CVSR, PCIC_CVSR_VS);
			break;
		}
		if (sc->flags & PCIC_PD_POWER) {
			pcic_setb(sp, PCIC_MISC1, PCIC_MISC1_VCC_33);
			break;
		}
		if (sc->flags & PCIC_RICOH_POWER) {
			pcic_setb(sp, PCIC_RICOH_MCR2, PCIC_MCR2_VCC_33);
			break;
		}

		/*
		 * Technically, The A, B, C stepping didn't support
		 * the 3.3V cards.  However, many cardbus bridges are
		 * identified as B step cards by our probe routine, so
		 * we do both.  It won't hurt the A, B, C bridges that
		 * don't support this bit since it is one of the
		 * reserved bits.
		 */
		if (sc->flags & (PCIC_AB_POWER | PCIC_DF_POWER))
			reg |= PCIC_VCC_3V;
		break;
	case 50:
		if (sc->flags & PCIC_KING_POWER)
			reg |= PCIC_VCC_5V_KING;
		/*
		 * For either of the two variant power schemes for 3.3V
		 * go ahead and turn off the 3.3V magic.  For all
		 * bridges, the setting the Vcc on bit does the rest.
		 */
		if (sc->flags & PCIC_VG_POWER)
			pcic_clrb(sp, PCIC_CVSR, PCIC_CVSR_VS);
		else if (sc->flags & PCIC_PD_POWER)
			pcic_clrb(sp, PCIC_MISC1, PCIC_MISC1_VCC_33);
		else if (sc->flags & PCIC_RICOH_POWER)
			pcic_clrb(sp, PCIC_RICOH_MCR2, PCIC_MCR2_VCC_33);
		break;
	}
	sp->putb(sp, PCIC_POWER, reg);
	DELAY(300*1000);
	if (slt->pwr.vcc) {
		reg |= PCIC_OUTENA;
		sp->putb(sp, PCIC_POWER, reg);
		DELAY(100*1000);
	}

	/*
	 * Some chips are smarter than us it seems, so if we weren't
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
		pcic_clrb(sp, PCIC_INT_GEN, 0xF);
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
		pcic_clrb(sp, PCIC_INT_GEN, PCIC_CARDRESET);
		slt->insert_seq = 2;
		timeout(pcic_reset, (void *)slt, hz/4);
		return;
	    case 2: /* Deassert it again */
		pcic_setb(sp, PCIC_INT_GEN, PCIC_CARDRESET|PCIC_IOCARD);
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
 * Wrapper function for pcicintr so that signatures match.
 */
static void
pcicintr(void *arg)
{
	pcicintr1(arg);
}

/*
 *	PCIC timer.  If the controller doesn't have a free IRQ to use
 *	or if interrupt steering doesn't work, poll the controller for
 *	insertion/removal events.
 */
static void
pcictimeout(void *chan)
{
	struct pcic_softc *sc = (struct pcic_softc *) chan;

	if (pcicintr1(chan) != 0) {
		printf("pcic%d: Static bug detected, ignoring hardware.\n",
		    ((struct pcic_softc *)chan)->unit);
		return;
	}
	sc->timeout_ch = timeout(pcictimeout, chan, hz/2);
}

/*
 *	PCIC Interrupt handler.
 *	Check each slot in turn, and read the card status change
 *	register. If this is non-zero, then a change has occurred
 *	on this card, so send an event to the main code.
 */
static int
pcicintr1(void *arg)
{
	int	slot, s;
	unsigned char chg;
	struct pcic_softc *sc = (struct pcic_softc *) arg;
	struct pcic_slot *sp = &sc->slots[0];

	s = splhigh();
	for (slot = 0; slot < PCIC_CARD_SLOTS; slot++, sp++) {
		if (sp->slt && (chg = sp->getb(sp, PCIC_STAT_CHG)) != 0) {
			/*
			 * if chg is 0xff, then we know that we've hit
			 * the famous "static bug" for some desktop
			 * pcmcia cards.  This is caused by static
			 * discharge frying the poor card's mind and
			 * it starts return 0xff forever.  We return
			 * an error and stop polling the card.  When
			 * we're interrupt based, we never see this.
			 * The card just goes away silently.
			 */
			if (chg == 0xff) {
				splx(s);
				return (EIO);
			}
			if (chg & PCIC_CDTCH) {
				if ((sp->getb(sp, PCIC_STATUS) & PCIC_CD) ==
				    PCIC_CD) {
					pccard_event(sp->slt, card_inserted);
				} else {
					pccard_event(sp->slt, card_removed);
					pcic_disable(sp->slt);
				}
			}
		}
	}
	splx(s);
	return (0);
}

/*
 *	pcic_resume - Suspend/resume support for PCIC
 */
static void
pcic_resume(struct slot *slt)
{
	struct pcic_slot *sp = slt->cdata;

	pcic_do_mgt_irq(sp, slt->irq);
	if (sp->controller == PCIC_PD672X) {
		pcic_setb(sp, PCIC_MISC1, PCIC_MISC1_SPEAKER);
		pcic_setb(sp, PCIC_MISC2, PCIC_LPDM_EN);
	}
}

int
pcic_activate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	struct pccard_devinfo *devi = device_get_ivars(child);
	int err;

	if (dev != device_get_parent(device_get_parent(child)) || devi == NULL)
		return (bus_generic_activate_resource(dev, child, type,
		    rid, r));

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
		err = pcic_io(devi->slt, rid);
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
		err = pcic_memory(devi->slt, rid);
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

int
pcic_deactivate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	struct pccard_devinfo *devi = device_get_ivars(child);
	int err;

	if (dev != device_get_parent(device_get_parent(child)) || devi == NULL)
		return (bus_generic_deactivate_resource(dev, child, type,
		    rid, r));

	switch (type) {
	case SYS_RES_IOPORT: {
		struct io_desc *ip = &devi->slt->io[rid];
		ip->flags &= ~IODF_ACTIVE;
		err = pcic_io(devi->slt, rid);
		if (err)
			return (err);
		break;
	}
	case SYS_RES_IRQ:
		break;
	case SYS_RES_MEMORY: {
		struct mem_desc *mp = &devi->slt->mem[rid];
		mp->flags &= ~(MDF_ACTIVE | MDF_ATTR);
		err = pcic_memory(devi->slt, rid);
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

int
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
		pcic_mapirq(devi->slt, rman_get_start(irq));
	else
		device_printf(dev, "Error %d irq %ld\n", err,
		    rman_get_start(irq));
	return (err);
}

int
pcic_teardown_intr(device_t dev, device_t child, struct resource *irq,
    void *cookie)
{
	struct pccard_devinfo *devi = device_get_ivars(child);

	pcic_mapirq(devi->slt, 0);
	return (bus_generic_teardown_intr(dev, child, irq, cookie));
}

int
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
		err = pcic_memory(devi->slt, rid);
		break;
	}
	default:
		err = EOPNOTSUPP;
	}
	return (err);
}

int
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
	return (err);
}

int
pcic_set_memory_offset(device_t bus, device_t child, int rid, u_int32_t offset,
    u_int32_t *deltap)
{
	struct pccard_devinfo *devi = device_get_ivars(child);
	struct mem_desc *mp = &devi->slt->mem[rid];

	mp->card = offset;
	if (deltap)
		*deltap = 0;			/* XXX BAD XXX */
	return (pcic_memory(devi->slt, rid));
}

int
pcic_get_memory_offset(device_t bus, device_t child, int rid, u_int32_t *offset)
{
	struct pccard_devinfo *devi = device_get_ivars(child);
	struct mem_desc *mp = &devi->slt->mem[rid];

	if (offset == 0)
		return (ENOMEM);

	*offset = mp->card;

	return (0);
}
