/*
 *  NEC MECIA controller.
 *-------------------------------------------------------------------------
 *
 * Copyright (c) 2001 M. Warner Losh.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *
 * Based heavily on the FreeBSD pcic driver's pcic98 support, derived
 * from PAO3 tree.  This copyright notice likely needs modification for
 * such a linage.  The only authorship I could find was:
 *
 * PC9801 original PCMCIA controller code for NS/A,Ne,NX/C,NR/L.
 * by Noriyuki Hosobuchi <hoso@ce.mbn.or.jp>
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <pccard/meciareg.h>
#include <pccard/cardinfo.h>
#include <pccard/slot.h>
#ifndef MECIA_IOBASE
#define MECIA_IOBASE   0x80d0
#endif

/* Get pnp IDs */
#include <isa/isavar.h>

#include <dev/pccard/pccardvar.h>
#include "card_if.h"

#define MECIA_DEVICE2SOFTC(dev)	((struct mecia_slot *) device_get_softc(dev))

/*
 *	Prototypes for interrupt handler.
 */
static driver_intr_t	meciaintr;
static int		mecia_ioctl(struct slot *, int, caddr_t);
static int		mecia_power(struct slot *);
static void		mecia_mapirq(struct slot *, int);
static timeout_t 	mecia_reset;
static void		mecia_resume(struct slot *);
static void		mecia_disable(struct slot *);
static timeout_t 	meciatimeout;
static struct callout_handle meciatimeout_ch
    = CALLOUT_HANDLE_INITIALIZER(&meciatimeout_ch);
static int		mecia_memory(struct slot *, int);
static int		mecia_io(struct slot *, int);

/*
 *	Per-slot data table.
 */
struct mecia_slot {
	int		unit;		/* Unit number */
	int		slotnum;	/* My slot number */
	struct slot	*slt;		/* Back ptr to slot */
	device_t	dev;		/* My device */
	u_char		last_reg1;	/* Last value of change reg */
};

static struct slot_ctrl mecia_cinfo = {
	mecia_mapirq,
	mecia_memory,
	mecia_io,
	mecia_reset,
	mecia_disable,
	mecia_power,
	mecia_ioctl,
	mecia_resume,
	1,
#if 0
	1
#else
	2		/* Fake for UE2212 LAN card */
#endif
};

static int validunits = 0;

/*
 *	Look for an NEC MECIA.
 *	For each available slot, allocate a PC-CARD slot.
 */

static int
mecia_probe(device_t dev)
{
	int		validslots = 0;

	/* Check isapnp ids */
	if (isa_get_logicalid(dev))		/* skip PnP probes */
		return (ENXIO);

	if (inb(MECIA_REG0) != 0xff) {
		validslots++;
		/* XXX need to allocated the port resources */
		device_set_desc(dev, "MECIA PC98 Original PCMCIA Controller");
	}
	return (validslots ? 0 : ENXIO);
}

static int
mecia_attach(device_t dev)
{
	int		error;
	int		irq;
	void		*ih;
	device_t	kid;
	struct resource *r;
	int		rid;
	struct slot	*slt;
	struct mecia_slot *sp;
	
	sp = MECIA_DEVICE2SOFTC(dev);
	sp->unit = validunits++;
	kid = device_add_child(dev, NULL, -1);
	if (kid == NULL) {
		device_printf(dev, "Can't add pccard bus slot 0\n");
		return (ENXIO);
	}
	device_probe_and_attach(kid);
	slt = pccard_init_slot(kid, &mecia_cinfo);
	if (slt == 0) {
		device_printf(dev, "Can't get pccard info slot 0\n");
		return (ENXIO);
	}
	slt->cdata = sp;
	sp->slt = slt;
	validunits++;

	rid = 0;
	r = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1, RF_ACTIVE);
	if (!r)
		return (ENXIO);

	irq = bus_get_resource_start(dev, SYS_RES_IRQ, 0);
	if (irq == 0) {
		/* See if the user has requested a specific IRQ */
		if (!getenv_int("machdep.pccard.mecia_irq", &irq))
			irq = 0;
	}
	rid = 0;
	r = 0;
	if (irq > 0) {
		r = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, irq,
		    irq, 1, RF_ACTIVE);
	}
	if (r && ((1 << (rman_get_start(r))) & MECIA_INT_MASK_ALLOWED) == 0) {
		device_printf(dev,
		    "Hardware does not support irq %d, trying polling.\n",
		    irq);
		bus_release_resource(dev, SYS_RES_IRQ, rid, r);
		r = 0;
		irq = 0;
	}
	if (r) {
		error = bus_setup_intr(dev, r, INTR_TYPE_MISC,
		    meciaintr, (void *) sp, &ih);
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
		meciatimeout_ch = timeout(meciatimeout, (void *) sp, hz/2);
		device_printf(dev, "Polling mode\n");
	}

	sp->last_reg1 = inb(MECIA_REG1);
	if (sp->last_reg1 & MECIA_CARDEXIST) {
		/* PCMCIA card exist */
		sp->slt->laststate = sp->slt->state = filled;
		pccard_event(sp->slt, card_inserted);
	} else {
		sp->slt->laststate = sp->slt->state = empty;
	}
	sp->slt->irq = irq;

	return (bus_generic_attach(dev));
}

static int
mecia_sresource(struct slot *slt, caddr_t data)
{
	struct pccard_resource *pr;
	struct resource *r;
	int flags;
	int rid = 0;
	device_t pccarddev = slt->dev;

	pr = (struct pccard_resource *)data;
	pr->resource_addr = ~0ul;
	switch(pr->type) {
	default:
		return (EINVAL);
	case SYS_RES_MEMORY:
	case SYS_RES_IRQ:
	case SYS_RES_IOPORT:
		break;
	}
	flags = rman_make_alignment_flags(pr->size);
	r = bus_alloc_resource(pccarddev, pr->type, &rid, pr->min, pr->max,
	   pr->size, flags);
	if (r != NULL) {
		pr->resource_addr = (u_long)rman_get_start(r);
		bus_release_resource(bridgedev, pr->type, rid, r);
	}
	return (0);
}

/*
 *	ioctl calls - Controller specific ioctls
 */
static int
mecia_ioctl(struct slot *slt, int cmd, caddr_t data)
{
	switch(cmd) {
	default:
		return (ENOTTY);
	case PIOCSRESOURCE:		/* Can I use this resource? */
		mecia_sresource(slt, data);
		break;
	}
	return (0);
}

/*
 *	MECIA timer.  If the controller doesn't have a free IRQ to use
 *	or if interrupt steering doesn't work, poll the controller for
 *	insertion/removal events.
 */
static void
meciatimeout(void *chan)
{
	meciaintr(chan);
	meciatimeout_ch = timeout(meciatimeout, chan, hz/2);
}

/*
 *	MECIA Interrupt handler.
 *	Check the slot and report any changes.
 */
static void
meciaintr(void *arg)
{
	u_char	reg1;
	int	s;
	struct mecia_slot *sp = (struct mecia_slot *) arg;

	s = splhigh();
	/* Check for a card in this slot */
	reg1 = inb(MECIA_REG1);
	if ((sp->last_reg1 ^ reg1) & MECIA_CARDEXIST) {
		sp->last_reg1 = reg1;
		if (reg1 & MECIA_CARDEXIST)
			pccard_event(sp->slt, card_inserted);
		else
			pccard_event(sp->slt, card_removed);
	}
	splx(s);
}

/*
 * local functions for PC-98 Original PC-Card controller
 */
#define	MECIA_ALWAYS_128MAPPING	1	/* trick for using UE2212  */

int mecia_mode = 0;	/* almost the same as the value in MECIA_REG2 */

static unsigned char reg_winsel = MECIA_UNMAPWIN;
static unsigned short reg_pagofs = 0;

static int
mecia_memory(struct slot *slt, int win)
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
		outb(MECIA_REG0, 0);
		x = inb(MECIA_REG1);
		x &= 0xfc;
		x |= 0x02;
		outb(MECIA_REG1, x);
		reg_winsel = inb(MECIA_REG_WINSEL);
		reg_pagofs = inw(MECIA_REG_PAGOFS);
		outb(MECIA_REG_WINSEL, MECIA_MAPWIN);
		outw(MECIA_REG_PAGOFS, (mp->card >> 13)); /* 8KB */

		if (mp->flags & MDF_ATTR)
			outb(MECIA_REG7, inb(MECIA_REG7) | MECIA_ATTRMEM);
		else
			outb(MECIA_REG7, inb(MECIA_REG7) & (~MECIA_ATTRMEM));

		outb(MECIA_REG_WINSEL, MECIA_MAPWIN);
#if 0
		if ((mp->flags & MDF_16BITS) == 1)	/* 16bit */
			outb(MECIA_REG2, inb(MECIA_REG2) & (~MECIA_8BIT));
		else					/* 8bit */
			outb(MECIA_REG2, inb(MECIA_REG2) | MECIA_8BIT);
#endif
	} else {  /* !(mp->flags & MDF_ACTIVE) */
		outb(MECIA_REG0, 0);
		x = inb(MECIA_REG1);
		x &= 0xfc;
		x |= 0x02;
		outb(MECIA_REG1, x);
#if 0
		outb(MECIA_REG_WINSEL, MECIA_UNMAPWIN);
		outw(MECIA_REG_PAGOFS, 0);
#else
		outb(MECIA_REG_WINSEL, reg_winsel);
		outw(MECIA_REG_PAGOFS, reg_pagofs);
#endif
	}
	return (0);
}

static int
mecia_io(struct slot *slt, int win)
{
	struct io_desc *ip = &slt->io[win];
	unsigned char x;
	unsigned short cardbase;
	u_short ofst;

	if (win != 0) {
		/* ignore for UE2212 */
		printf(
		"mecia:Illegal MECIA I/O window(%d) request! Ignored.\n", win);
/*		return (EINVAL);*/
		return (0);
	}

	if (ip->flags & IODF_ACTIVE) {
		x = inb(MECIA_REG2) & 0x0f;
#if 0
		if (! (ip->flags & IODF_CS16))
			x |= MECIA_8BIT;
#else
		if (! (ip->flags & IODF_16BIT)) {
			x |= MECIA_8BIT;
			mecia_mode |= MECIA_8BIT;
		}
#endif

		ofst = ip->start & 0xf;
		cardbase = ip->start & ~0xf;
#ifndef MECIA_ALWAYS_128MAPPING
		if (ip->size + ofst > 16)
#endif
		{	/* 128bytes mapping */
			x |= MECIA_MAP128;
			mecia_mode |= MECIA_MAP128;
			ofst |= ((cardbase & 0x70) << 4);
			cardbase &= ~0x70;
		}

		x |= MECIA_MAPIO;
		outb(MECIA_REG2, x);
    
		outw(MECIA_REG4, MECIA_IOBASE);	/* 98side I/O base */
		outw(MECIA_REG5, cardbase);	/* card side I/O base */

		if (bootverbose) {
			printf("mecia: I/O mapped 0x%04x(98) -> "
			       "0x%04x(Card) and width %d bytes\n",
				MECIA_IOBASE+ofst, ip->start, ip->size);
			printf("mecia: reg2=0x%02x reg3=0x%02x reg7=0x%02x\n",
				inb(MECIA_REG2), inb(MECIA_REG3),
				inb(MECIA_REG7));
			printf("mecia: mode=%d\n", mecia_mode);
		}

		ip->start = MECIA_IOBASE + ofst;
	} else {
		outb(MECIA_REG2, inb(MECIA_REG2) & (~MECIA_MAPIO));
		mecia_mode = 0;
	}
	return (0);
}

static int
mecia_power(struct slot *slt)
{
	unsigned char reg;

	reg = inb(MECIA_REG7) & (~MECIA_VPP12V);
	switch(slt->pwr.vpp) {
	default:
		return (EINVAL);
	case 50:
		break;
	case 120:
		reg |= MECIA_VPP12V;
		break;
	}
	outb(MECIA_REG7, reg);
	DELAY(100*1000);

	reg = inb(MECIA_REG2) & (~MECIA_VCC3P3V);
	switch(slt->pwr.vcc) {
	default:
		return (EINVAL);
	case 33:
		reg |= MECIA_VCC3P3V;
		break;
	case 50:
		break;
	}
	outb(MECIA_REG2, reg);
	DELAY(100*1000);
	return (0);
}

static void
mecia_mapirq(struct slot *slt, int irq)
{
	u_char x;

	switch (irq) {
	case 3:
		x = MECIA_INT0;
		break;
	case 5:
		x = MECIA_INT1;
		break;
	case 6:
		x = MECIA_INT2;
		break;
	case 10:
		x = MECIA_INT4;
		break;
	case 12:
		x = MECIA_INT5;
		break;
	case 0:		/* disable */
		x = MECIA_INTDISABLE;
		break;
	default:
		printf("mecia: illegal irq %d\n", irq);
		return;
	}
#ifdef	MECIA_DEBUG
	printf("mecia: irq=%d mapped.\n", irq);
#endif
	outb(MECIA_REG3, x);
}

static void
mecia_reset(void *chan)
{
	struct slot *slt = chan;

	outb(MECIA_REG0, 0);
	outb(MECIA_REG2, inb(MECIA_REG2) & (~MECIA_MAPIO));
	outb(MECIA_REG3, MECIA_INTDISABLE);
#if 0
/* mecia_reset() is called after mecia_power() */
	outb(MECIA_REG2, inb(MECIA_REG2) & (~MECIA_VCC3P3V));
	outb(MECIA_REG7, inb(MECIA_REG7) & (~MECIA_VPP12V));
#endif
	outb(MECIA_REG1, 0);

	selwakeup(&slt->selp);
}

static void
mecia_disable(struct slot *slt)
{
	/* null function */
}

static void
mecia_resume(struct slot *slt)
{
	/* XXX MECIA How ? */
}

static int
mecia_activate_resource(device_t dev, device_t child, int type, int rid,
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
		err = mecia_cinfo.mapio(devi->slt, rid);
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
		err = mecia_cinfo.mapmem(devi->slt, rid);
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
mecia_deactivate_resource(device_t dev, device_t child, int type, int rid,
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
		err = mecia_cinfo.mapio(devi->slt, rid);
		if (err)
			return (err);
		break;
	}
	case SYS_RES_IRQ:
		break;
	case SYS_RES_MEMORY: {
		struct mem_desc *mp = &devi->slt->mem[rid];
		mp->flags &= ~(MDF_ACTIVE | MDF_ATTR);
		err = mecia_cinfo.mapmem(devi->slt, rid);
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
mecia_setup_intr(device_t dev, device_t child, struct resource *irq,
    int flags, driver_intr_t *intr, void *arg, void **cookiep)
{
	struct pccard_devinfo *devi = device_get_ivars(child);
	int err;

	if (((1 << rman_get_start(irq)) & MECIA_INT_MASK_ALLOWED) == 0) {
		device_printf(dev, "Hardware does not support irq %ld.\n",
		    rman_get_start(irq));
		return (EINVAL);
	}

	err = bus_generic_setup_intr(dev, child, irq, flags, intr, arg,
	    cookiep);
	if (err == 0)
		mecia_cinfo.mapirq(devi->slt, rman_get_start(irq));
	else
		device_printf(dev, "Error %d irq %ld\n", err,
		    rman_get_start(irq));
	return (err);
}

static int
mecia_teardown_intr(device_t dev, device_t child, struct resource *irq,
    void *cookie)
{
	struct pccard_devinfo *devi = device_get_ivars(child);

	mecia_cinfo.mapirq(devi->slt, 0);
	return (bus_generic_teardown_intr(dev, child, irq, cookie));
}

static int
mecia_set_res_flags(device_t bus, device_t child, int restype, int rid,
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
		err = mecia_cinfo.mapmem(devi->slt, rid);
		break;
	}
	default:
		err = EOPNOTSUPP;
	}
	return (err);
}

static int
mecia_get_res_flags(device_t bus, device_t child, int restype, int rid,
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

static int
mecia_set_memory_offset(device_t bus, device_t child, int rid,
    u_int32_t offset, u_int32_t *deltap)
{
	struct pccard_devinfo *devi = device_get_ivars(child);
	struct mem_desc *mp = &devi->slt->mem[rid];

	mp->card = offset;
	if (deltap)
		*deltap = 0;			/* XXX BAD XXX */
	return (mecia_cinfo.mapmem(devi->slt, rid));
}

static int
mecia_get_memory_offset(device_t bus, device_t child, int rid,
    u_int32_t *offset)
{
	struct pccard_devinfo *devi = device_get_ivars(child);
	struct mem_desc *mp = &devi->slt->mem[rid];

	if (offset == 0)
		return (ENOMEM);

	*offset = mp->card;

	return (0);
}

static device_method_t mecia_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		mecia_probe),
	DEVMETHOD(device_attach,	mecia_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, mecia_activate_resource),
	DEVMETHOD(bus_deactivate_resource, mecia_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	mecia_setup_intr),
	DEVMETHOD(bus_teardown_intr,	mecia_teardown_intr),

	/* Card interface */
	DEVMETHOD(card_set_res_flags,	mecia_set_res_flags),
	DEVMETHOD(card_get_res_flags,	mecia_get_res_flags),
	DEVMETHOD(card_set_memory_offset, mecia_set_memory_offset),
	DEVMETHOD(card_get_memory_offset, mecia_get_memory_offset),

	{ 0, 0 }
};

devclass_t	mecia_devclass;

static driver_t mecia_driver = {
	"mecia",
	mecia_methods,
	sizeof(struct mecia_slot)
};

DRIVER_MODULE(mecia, isa, mecia_driver, mecia_devclass, 0, 0);
