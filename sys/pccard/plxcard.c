/*
 *  PLX pseudo controller
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
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <pccard/meciareg.h>
#include <pccard/cardinfo.h>
#include <pccard/slot.h>
#include <dev/pccard/pccardvar.h>
#include "card_if.h"

#define PLXCARD_DEVICE2SOFTC(dev)	\
	((struct plxcard_slot *) device_get_softc(dev))

/*
 *	Prototypes for interrupt handler.
 */
static driver_intr_t	plxcardintr;
static int		plxcard_ioctl(struct slot *, int, caddr_t);
static int		plxcard_power(struct slot *);
static void		plxcard_mapirq(struct slot *, int);
static timeout_t 	plxcard_reset;
static void		plxcard_resume(struct slot *);
static void		plxcard_disable(struct slot *);
static int		plxcard_memory(struct slot *, int);
static int		plxcard_io(struct slot *, int);

/*
 *	Per-slot data table.
 */
struct plxcard_slot {
	int		unit;		/* Unit number */
	int		slotnum;	/* My slot number */
	struct slot	*slt;		/* Back ptr to slot */
	device_t	dev;		/* My device */
};

static struct slot_ctrl plxcard_cinfo = {
	plxcard_mapirq,
	plxcard_memory,
	plxcard_io,
	plxcard_reset,
	plxcard_disable,
	plxcard_power,
	plxcard_ioctl,
	plxcard_resume,
	1,
	1
};

static int validunits = 0;

/*
 *	For each available slot, allocate a PC-CARD slot.
 */
static int
plxcard_probe(device_t dev)
{
	return (ENXIO);
}

static int
plxcard_attach(device_t dev)
{
	int		error;
	void		*ih;
	device_t	kid;
	struct resource *r;
	int		rid;
	struct slot	*slt;
	struct plxcard_slot *sp;
	
	sp = PLXCARD_DEVICE2SOFTC(dev);
	sp->unit = validunits++;
	kid = device_add_child(dev, NULL, -1);
	if (kid == NULL) {
		device_printf(dev, "Can't add pccard bus slot 0\n");
		return (ENXIO);
	}
	device_probe_and_attach(kid);
	slt = pccard_init_slot(kid, &plxcard_cinfo);
	if (slt == 0) {
		device_printf(dev, "Can't get pccard info slot 0\n");
		return (ENXIO);
	}
	slt->cdata = sp;
	sp->slt = slt;
	validunits++;

	rid = 0;
	r = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1, RF_ACTIVE);
	if (r) {
		error = bus_setup_intr(dev, r, INTR_TYPE_MISC,
		    plxcardintr, (void *) sp, &ih);
		if (error) {
			bus_release_resource(dev, SYS_RES_IRQ, rid, r);
			return (error);
		}
		sp->slt->irq = rman_get_start(r);
	}

	return (bus_generic_attach(dev));
}

/*
 *	ioctl calls - Controller specific ioctls
 */
static int
plxcard_ioctl(struct slot *slt, int cmd, caddr_t data)
{
	return (ENOTTY);
}

/*
 *	PLXCARD Interrupt handler.
 *	Check the slot and report any changes.
 */
static void
plxcardintr(void *arg)
{
}

static int
plxcard_memory(struct slot *slt, int win)
{
	struct mem_desc *mp = &slt->mem[win];

	if (mp->flags & MDF_ACTIVE) {
	} else {  /* !(mp->flags & MDF_ACTIVE) */
	}
	return (0);
}

static int
plxcard_io(struct slot *slt, int win)
{
	struct io_desc *ip = &slt->io[win];

	if (ip->flags & IODF_ACTIVE) {
	} else {
	}
	return (0);
}

static int
plxcard_power(struct slot *slt)
{
	return (0);
}

static void
plxcard_mapirq(struct slot *slt, int irq)
{
}

static void
plxcard_reset(void *chan)
{
	struct slot *slt = chan;
	selwakeup(&slt->selp);
}

static void
plxcard_disable(struct slot *slt)
{
	/* null function */
}

static void
plxcard_resume(struct slot *slt)
{
	/* XXX PLXCARD How ? */
}

static int
plxcard_activate_resource(device_t dev, device_t child, int type, int rid,
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
		err = plxcard_cinfo.mapio(devi->slt, rid);
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
		err = plxcard_cinfo.mapmem(devi->slt, rid);
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
plxcard_deactivate_resource(device_t dev, device_t child, int type, int rid,
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
		err = plxcard_cinfo.mapio(devi->slt, rid);
		if (err)
			return (err);
		break;
	}
	case SYS_RES_IRQ:
		break;
	case SYS_RES_MEMORY: {
		struct mem_desc *mp = &devi->slt->mem[rid];
		mp->flags &= ~(MDF_ACTIVE | MDF_ATTR);
		err = plxcard_cinfo.mapmem(devi->slt, rid);
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
plxcard_setup_intr(device_t dev, device_t child, struct resource *irq,
    int flags, driver_intr_t *intr, void *arg, void **cookiep)
{
	struct pccard_devinfo *devi = device_get_ivars(child);
	int err;

	err = bus_generic_setup_intr(dev, child, irq, flags, intr, arg,
	    cookiep);
	if (err == 0)
		plxcard_cinfo.mapirq(devi->slt, rman_get_start(irq));
	else
		device_printf(dev, "Error %d irq %ld\n", err,
		    rman_get_start(irq));
	return (err);
}

static int
plxcard_teardown_intr(device_t dev, device_t child, struct resource *irq,
    void *cookie)
{
	struct pccard_devinfo *devi = device_get_ivars(child);

	plxcard_cinfo.mapirq(devi->slt, 0);
	return (bus_generic_teardown_intr(dev, child, irq, cookie));
}

static int
plxcard_set_res_flags(device_t bus, device_t child, int restype, int rid,
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
		err = plxcard_cinfo.mapmem(devi->slt, rid);
		break;
	}
	default:
		err = EOPNOTSUPP;
	}
	return (err);
}

static int
plxcard_get_res_flags(device_t bus, device_t child, int restype, int rid,
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
plxcard_set_memory_offset(device_t bus, device_t child, int rid,
    u_int32_t offset, u_int32_t *deltap)
{
	struct pccard_devinfo *devi = device_get_ivars(child);
	struct mem_desc *mp = &devi->slt->mem[rid];

	mp->card = offset;
	if (deltap)
		*deltap = 0;			/* XXX BAD XXX */
	return (plxcard_cinfo.mapmem(devi->slt, rid));
}

static int
plxcard_get_memory_offset(device_t bus, device_t child, int rid,
    u_int32_t *offset)
{
	struct pccard_devinfo *devi = device_get_ivars(child);
	struct mem_desc *mp = &devi->slt->mem[rid];

	if (offset == 0)
		return (ENOMEM);

	*offset = mp->card;

	return (0);
}

static device_method_t plxcard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		plxcard_probe),
	DEVMETHOD(device_attach,	plxcard_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, plxcard_activate_resource),
	DEVMETHOD(bus_deactivate_resource, plxcard_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	plxcard_setup_intr),
	DEVMETHOD(bus_teardown_intr,	plxcard_teardown_intr),

	/* Card interface */
	DEVMETHOD(card_set_res_flags,	plxcard_set_res_flags),
	DEVMETHOD(card_get_res_flags,	plxcard_get_res_flags),
	DEVMETHOD(card_set_memory_offset, plxcard_set_memory_offset),
	DEVMETHOD(card_get_memory_offset, plxcard_get_memory_offset),

	{ 0, 0 }
};

devclass_t	plxcard_devclass;

static driver_t plxcard_driver = {
	"plxcard",
	plxcard_methods,
	sizeof(struct plxcard_slot)
};

DRIVER_MODULE(plxcard, pci, plxcard_driver, plxcard_devclass, 0, 0);
