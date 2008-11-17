/*-
 * Copyright (c) 2006 Benno Rice.
 * Copyright (C) 2007-2008 MARVELL INTERNATIONAL LTD.
 * All rights reserved.
 *
 * Adapted and extended to Marvell SoCs by Semihalf.
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
 * from: FreeBSD: //depot/projects/arm/src/sys/arm/xscale/pxa2x0/pxa2x0_icu.c, rev 1
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <arm/mv/mvreg.h>
#include <arm/mv/mvvar.h>

struct mv_ic_softc {
	struct resource	*	ic_res[1];
	bus_space_tag_t		ic_bst;
	bus_space_handle_t	ic_bsh;
	int			ic_high_regs;
	int			ic_error_regs;
};

static struct resource_spec mv_ic_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

static struct mv_ic_softc *mv_ic_sc = NULL;

static int	mv_ic_probe(device_t);
static int	mv_ic_attach(device_t);

uint32_t	mv_ic_get_cause(void);
uint32_t	mv_ic_get_mask(void);
void		mv_ic_set_mask(uint32_t);
uint32_t	mv_ic_get_cause_hi(void);
uint32_t	mv_ic_get_mask_hi(void);
void		mv_ic_set_mask_hi(uint32_t);
uint32_t	mv_ic_get_cause_error(void);
uint32_t	mv_ic_get_mask_error(void);
void		mv_ic_set_mask_error(uint32_t);
static void	arm_mask_irq_all(void);

static int
mv_ic_probe(device_t dev)
{

	device_set_desc(dev, "Marvell Integrated Interrupt Controller");
	return (0);
}

static int
mv_ic_attach(device_t dev)
{
	struct mv_ic_softc *sc;
	uint32_t dev_id, rev_id;
	int error;

	sc = (struct mv_ic_softc *)device_get_softc(dev);

	if (mv_ic_sc != NULL)
		return (ENXIO);
	mv_ic_sc = sc;

	soc_id(&dev_id, &rev_id);

	sc->ic_high_regs = 0;
	sc->ic_error_regs = 0;

	if (dev_id == MV_DEV_88F6281 || dev_id == MV_DEV_MV78100)
		sc->ic_high_regs = 1;

	if (dev_id == MV_DEV_MV78100)
		sc->ic_error_regs = 1;

	error = bus_alloc_resources(dev, mv_ic_spec, sc->ic_res);
	if (error) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	sc->ic_bst = rman_get_bustag(sc->ic_res[0]);
	sc->ic_bsh = rman_get_bushandle(sc->ic_res[0]);

	/* Mask all interrupts */
	arm_mask_irq_all();

	return (0);
}

static device_method_t mv_ic_methods[] = {
	DEVMETHOD(device_probe,		mv_ic_probe),
	DEVMETHOD(device_attach,	mv_ic_attach),
	{ 0, 0 }
};

static driver_t mv_ic_driver = {
	"ic",
	mv_ic_methods,
	sizeof(struct mv_ic_softc),
};

static devclass_t mv_ic_devclass;

DRIVER_MODULE(ic, mbus, mv_ic_driver, mv_ic_devclass, 0, 0);

int
arm_get_next_irq(void)
{
	int irq;

	irq = mv_ic_get_cause() & mv_ic_get_mask();
	if (irq)
		return (ffs(irq) - 1);

	if (mv_ic_sc->ic_high_regs) {
		irq = mv_ic_get_cause_hi() & mv_ic_get_mask_hi();
		if (irq)
			return (ffs(irq) + 31);
	}

	if (mv_ic_sc->ic_error_regs) {
		irq = mv_ic_get_cause_error() & mv_ic_get_mask_error();
		if (irq)
			return (ffs(irq) + 63);
	}

	return (-1);
}

static void
arm_mask_irq_all(void)
{

	mv_ic_set_mask(0);

	if (mv_ic_sc->ic_high_regs)
		mv_ic_set_mask_hi(0);

	if (mv_ic_sc->ic_error_regs)
		mv_ic_set_mask_error(0);
}

void
arm_mask_irq(uintptr_t nb)
{
	uint32_t	mr;

	if (nb < 32) {
		mr = mv_ic_get_mask();
		mr &= ~(1 << nb);
		mv_ic_set_mask(mr);

	} else if ((nb < 64) && mv_ic_sc->ic_high_regs) {
		mr = mv_ic_get_mask_hi();
		mr &= ~(1 << (nb - 32));
		mv_ic_set_mask_hi(mr);

	} else if ((nb < 96) && mv_ic_sc->ic_error_regs) {
		mr = mv_ic_get_mask_error();
		mr &= ~(1 << (nb - 64));
		mv_ic_set_mask_error(mr);
	}
}

void
arm_unmask_irq(uintptr_t nb)
{
	uint32_t	mr;

	if (nb < 32) {
		mr = mv_ic_get_mask();
		mr |= (1 << nb);
		mv_ic_set_mask(mr);

	} else if ((nb < 64) && mv_ic_sc->ic_high_regs) {
		mr = mv_ic_get_mask_hi();
		mr |= (1 << (nb - 32));
		mv_ic_set_mask_hi(mr);

	} else if ((nb < 96) && mv_ic_sc->ic_error_regs) {
		mr = mv_ic_get_mask_error();
		mr |= (1 << (nb - 64));
		mv_ic_set_mask_error(mr);
	}
}

void
mv_ic_set_mask(uint32_t val)
{

	bus_space_write_4(mv_ic_sc->ic_bst, mv_ic_sc->ic_bsh,
	    IRQ_MASK, val);
}

uint32_t
mv_ic_get_mask(void)
{

	return (bus_space_read_4(mv_ic_sc->ic_bst,
	    mv_ic_sc->ic_bsh, IRQ_MASK));
}

uint32_t
mv_ic_get_cause(void)
{

	return (bus_space_read_4(mv_ic_sc->ic_bst,
	    mv_ic_sc->ic_bsh, IRQ_CAUSE));
}

void
mv_ic_set_mask_hi(uint32_t val)
{

	bus_space_write_4(mv_ic_sc->ic_bst, mv_ic_sc->ic_bsh,
	    IRQ_MASK_HI, val);
}

uint32_t
mv_ic_get_mask_hi(void)
{

	return (bus_space_read_4(mv_ic_sc->ic_bst,
	    mv_ic_sc->ic_bsh, IRQ_MASK_HI));
}

uint32_t
mv_ic_get_cause_hi(void)
{

	return (bus_space_read_4(mv_ic_sc->ic_bst,
	    mv_ic_sc->ic_bsh, IRQ_CAUSE_HI));
}

void
mv_ic_set_mask_error(uint32_t val)
{

	bus_space_write_4(mv_ic_sc->ic_bst, mv_ic_sc->ic_bsh,
	    IRQ_MASK_ERROR, val);
}

uint32_t
mv_ic_get_mask_error(void)
{

	return (bus_space_read_4(mv_ic_sc->ic_bst,
	    mv_ic_sc->ic_bsh, IRQ_MASK_ERROR));
}

uint32_t
mv_ic_get_cause_error(void)
{

	return (bus_space_read_4(mv_ic_sc->ic_bst,
	    mv_ic_sc->ic_bsh, IRQ_CAUSE_ERROR));
}
