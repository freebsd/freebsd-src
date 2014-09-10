/*-
 * Copyright (c) 2014 Warner Losh.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_platform.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/resource.h>
#include <sys/systm.h>
#include <sys/rman.h>

#include <machine/armreg.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/frame.h>
#include <machine/intr.h>
#include <machine/resource.h>

#include <arm/at91/at91var.h>
#include <arm/at91/at91_aicreg.h>

#ifdef FDT
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

static struct aic_softc {
	struct resource	*mem_res;	/* Memory resource */
	void		*intrhand;	/* Interrupt handle */
	device_t	sc_dev;
} *sc;

static inline uint32_t
RD4(struct aic_softc *sc, bus_size_t off)
{

	return (bus_read_4(sc->mem_res, off));
}

static inline void
WR4(struct aic_softc *sc, bus_size_t off, uint32_t val)
{

	bus_write_4(sc->mem_res, off, val);
}

void
arm_mask_irq(uintptr_t nb)
{

	WR4(sc, IC_IDCR, 1 << nb);
}

int
arm_get_next_irq(int last __unused)
{
	int status;
	int irq;
	
	irq = RD4(sc, IC_IVR);
	status = RD4(sc, IC_ISR);
	if (status == 0) {
		WR4(sc, IC_EOICR, 1);
		return (-1);
	}
	return (irq);
}

void
arm_unmask_irq(uintptr_t nb)
{
	
	WR4(sc, IC_IECR, 1 << nb);
	WR4(sc, IC_EOICR, 0);
}

static int
at91_aic_probe(device_t dev)
{
#ifdef FDT
	if (!ofw_bus_is_compatible(dev, "atmel,at91rm9200-aic"))
		return (ENXIO);
#endif
	device_set_desc(dev, "AIC");
        return (0);
}

static int
at91_aic_attach(device_t dev)
{
	int i, rid, err = 0;

	device_printf(dev, "Attach %d\n", bus_current_pass);

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);

	if (sc->mem_res == NULL)
		panic("couldn't allocate register resources");

	/*
	 * Setup the interrupt table.
	 */
	if (soc_info.soc_data == NULL || soc_info.soc_data->soc_irq_prio == NULL)
		panic("Interrupt priority table missing\n");
	for (i = 0; i < 32; i++) {
		WR4(sc, IC_SVR + i * 4, i);
		/* Priority. */
		WR4(sc, IC_SMR + i * 4, soc_info.soc_data->soc_irq_prio[i]);
		if (i < 8)
			WR4(sc, IC_EOICR, 1);
	}

	WR4(sc, IC_SPU, 32);
	/* No debug. */
	WR4(sc, IC_DCR, 0);
	/* Disable and clear all interrupts. */
	WR4(sc, IC_IDCR, 0xffffffff);
	WR4(sc, IC_ICCR, 0xffffffff);
	enable_interrupts(PSR_I | PSR_F);

	return (err);
}

static void
at91_aic_new_pass(device_t dev)
{
	device_printf(dev, "Pass %d\n", bus_current_pass);
}

static device_method_t at91_aic_methods[] = {
	DEVMETHOD(device_probe, at91_aic_probe),
	DEVMETHOD(device_attach, at91_aic_attach),
	DEVMETHOD(bus_new_pass, at91_aic_new_pass),
	DEVMETHOD_END
};

static driver_t at91_aic_driver = {
	"at91_aic",
	at91_aic_methods,
	sizeof(struct aic_softc),
};

static devclass_t at91_aic_devclass;

#ifdef FDT
EARLY_DRIVER_MODULE(at91_aic, simplebus, at91_aic_driver, at91_aic_devclass,
    NULL, NULL, BUS_PASS_INTERRUPT);
#else
EARLY_DRIVER_MODULE(at91_aic, atmelarm, at91_aic_driver, at91_aic_devclass,
    NULL, NULL, BUS_PASS_INTERRUPT);
#endif
