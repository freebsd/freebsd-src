/*	$NetBSD: sa11x0.c,v 1.14 2003/07/15 00:24:50 lukem Exp $	*/

/*-
 * Copyright (c) 2001, The NetBSD Foundation, Inc.  All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by IWAMOTO Toshihiro and Ichiro FUKUHARA.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 */
/*-
 * Copyright (c) 1999
 *         Shin Takemura and PocketBSD Project. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the PocketBSD project
 *	and its contributors.
 * 4. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/reboot.h>
#include <sys/malloc.h>
#include <sys/interrupt.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>
#include <arm/sa11x0/sa11x0_reg.h>
#include <arm/sa11x0/sa11x0_var.h>
#include <arm/sa11x0/sa11x0_dmacreg.h>
#include <arm/sa11x0/sa11x0_ppcreg.h>
#include <arm/sa11x0/sa11x0_gpioreg.h>

extern void sa11x0_activateirqs(void);

static struct resource *sa1110_alloc_resource(device_t, device_t, int, int *,
        u_long, u_long, u_long, u_int);

static int sa1110_activate_resource(device_t, device_t, int, int,
        struct resource *);
static int sa1110_setup_intr(device_t, device_t, struct resource *, int,
        driver_filter_t *, driver_intr_t *, void *, void **);

struct sa11x0_softc *sa11x0_softc; /* There can be only one. */

static int
sa1110_setup_intr(device_t dev, device_t child,
        struct resource *ires,  int flags, driver_filter_t *filt,
	driver_intr_t *intr, void *arg, void **cookiep)
{
	int saved_cpsr;
	
	if (flags & INTR_TYPE_TTY)
		rman_set_start(ires, 15);
	else if (flags & INTR_TYPE_CLK) {
		if (rman_get_start(ires) == 0)
			rman_set_start(ires, 26);
		else
			rman_set_start(ires, 27);
	}
	saved_cpsr = SetCPSR(I32_bit, I32_bit);

	SetCPSR(I32_bit, saved_cpsr & I32_bit);
	BUS_SETUP_INTR(device_get_parent(dev), child, ires, flags, filt,
	    intr, arg, cookiep);
	return (0);
}

static struct resource *
sa1110_alloc_resource(device_t bus, device_t child, int type, int *rid,
        u_long start, u_long end, u_long count, u_int flags)
{
	struct resource *res;
	
	res = rman_reserve_resource(&sa11x0_softc->sa11x0_rman, *rid, *rid,
	    count, flags, child);
	if (res != NULL)
		rman_set_rid(res, *rid);

	return (res);
}
static int
sa1110_activate_resource(device_t bus, device_t child, int type, int rid,
        struct resource *r)
{
	return (0);
}
/* prototypes */
static int	sa11x0_probe(device_t);
static int	sa11x0_attach(device_t);
static void	sa11x0_identify(driver_t *, device_t);

extern vm_offset_t saipic_base;


int
sa11x0_probe(device_t dev)
{
	return (BUS_PROBE_NOWILDCARD);
}

void
sa11x0_identify(driver_t *driver, device_t parent)
{
	
	BUS_ADD_CHILD(parent, 0, "saip", 0);
}

int
sa11x0_attach(device_t dev)
{
	struct sa11x0_softc *sc = device_get_softc(dev);
	int unit = device_get_unit(dev);
	sc->sc_iot = &sa11x0_bs_tag;

	sa11x0_softc = sc;

	/* Map the SAIP */

	if (bus_space_map(sc->sc_iot, SAIPIC_BASE, SAIPIC_NPORTS,
			0, &sc->sc_ioh))
		panic("saip%d: Cannot map registers", unit);
	saipic_base = sc->sc_ioh;

	/* Map the GPIO registers */
	if (bus_space_map(sc->sc_iot, SAGPIO_BASE, SAGPIO_NPORTS,
			  0, &sc->sc_gpioh))
		panic("saip%d: unable to map GPIO registers", unit);
	bus_space_write_4(sc->sc_iot, sc->sc_gpioh, SAGPIO_EDR, 0xffffffff);

	/* Map the PPC registers */
	if (bus_space_map(sc->sc_iot, SAPPC_BASE, SAPPC_NPORTS,
			  0, &sc->sc_ppch))
		panic("saip%d: unable to map PPC registers", unit);

#if 0
	/* Map the DMA controller registers */
	if (bus_space_map(sc->sc_iot, SADMAC_BASE, SADMAC_NPORTS,
			  0, &sc->sc_dmach))
		panic("saip%d: unable to map DMAC registers", unit);
#endif
	/* Map the reset controller registers */
	if (bus_space_map(sc->sc_iot, SARCR_BASE, PAGE_SIZE,
			  0, &sc->sc_reseth))
		panic("saip%d: unable to map reset registers", unit);
	printf("\n");

	
	/*
	 *  Mask all interrupts.
	 *  They are later unmasked at each device's attach routine.
	 */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, SAIPIC_MR, 0);

	/* Route all bits to IRQ */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, SAIPIC_LR, 0);

	/* Exit idle mode only when unmasked intr is received */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, SAIPIC_CR, 1);
#if 0
	/* disable all DMAC channels */
	bus_space_write_4(sc->sc_iot, sc->sc_dmach, SADMAC_DCR0_CLR, 1);
	bus_space_write_4(sc->sc_iot, sc->sc_dmach, SADMAC_DCR1_CLR, 1);
	bus_space_write_4(sc->sc_iot, sc->sc_dmach, SADMAC_DCR2_CLR, 1);
	bus_space_write_4(sc->sc_iot, sc->sc_dmach, SADMAC_DCR3_CLR, 1);
	bus_space_write_4(sc->sc_iot, sc->sc_dmach, SADMAC_DCR4_CLR, 1);
	bus_space_write_4(sc->sc_iot, sc->sc_dmach, SADMAC_DCR5_CLR, 1);
#endif
	/*
	 * XXX this is probably a bad place, but intr bit shouldn't be
	 * XXX enabled before intr mask is set.
	 * XXX Having sane imask[] suffice??
	 */
#if 0
	SetCPSR(I32_bit, 0);
#endif
	/*
	 *  Attach each devices
	 */
	sc->sa11x0_rman.rm_type = RMAN_ARRAY;
	sc->sa11x0_rman.rm_descr = "SA11x0 IRQs";
	if (rman_init(&sc->sa11x0_rman) != 0 ||
	    rman_manage_region(&sc->sa11x0_rman, 0, 32) != 0)
		panic("sa11x0_attach: failed to set up rman");
	device_add_child(dev, "uart", 0);
	device_add_child(dev, "saost", 0);
	bus_generic_probe(dev);
	bus_generic_attach(dev);
	sa11x0_activateirqs();
	return (0);
}

static device_method_t saip_methods[] = {
	DEVMETHOD(device_probe, sa11x0_probe),
	DEVMETHOD(device_attach, sa11x0_attach),
	DEVMETHOD(device_identify, sa11x0_identify),
	DEVMETHOD(bus_alloc_resource, sa1110_alloc_resource),
	DEVMETHOD(bus_activate_resource, sa1110_activate_resource),
	DEVMETHOD(bus_setup_intr, sa1110_setup_intr),
	{0, 0},
};

static driver_t saip_driver = {
	"saip",
	saip_methods,
	sizeof(struct sa11x0_softc),
};
static devclass_t saip_devclass;

DRIVER_MODULE(saip, nexus, saip_driver, saip_devclass, 0, 0);
