/*-
 * Copyright (c) 2010 Jakub Wojciech Klama <jceel@FreeBSD.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/timetc.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/lpc/lpcreg.h>

struct lpc_intc_softc {
	struct resource *	li_res;
	bus_space_tag_t		li_bst;
	bus_space_handle_t	li_bsh;
};

static int lpc_intc_probe(device_t);
static int lpc_intc_attach(device_t);
static void lpc_intc_eoi(void *);

static struct lpc_intc_softc *intc_softc = NULL;

#define	intc_read_4(reg)		\
    bus_space_read_4(intc_softc->li_bst, intc_softc->li_bsh, reg)
#define	intc_write_4(reg, val)		\
    bus_space_write_4(intc_softc->li_bst, intc_softc->li_bsh, reg, val)

static int
lpc_intc_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "lpc,pic"))
		return (ENXIO);

	device_set_desc(dev, "LPC32x0 Interrupt Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
lpc_intc_attach(device_t dev)
{
	struct lpc_intc_softc *sc = device_get_softc(dev);
	int rid = 0;

	if (intc_softc)
		return (ENXIO);

	sc->li_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, 
	    RF_ACTIVE);
	if (!sc->li_res) {
		device_printf(dev, "could not alloc resources\n");
		return (ENXIO);
	}

	sc->li_bst = rman_get_bustag(sc->li_res);
	sc->li_bsh = rman_get_bushandle(sc->li_res);
	intc_softc = sc;
	arm_post_filter = lpc_intc_eoi;

	/* Clear interrupt status registers and disable all interrupts */
	intc_write_4(LPC_INTC_MIC_ER, 0);
	intc_write_4(LPC_INTC_SIC1_ER, 0);
	intc_write_4(LPC_INTC_SIC2_ER, 0);
	intc_write_4(LPC_INTC_MIC_RSR, ~0);
	intc_write_4(LPC_INTC_SIC1_RSR, ~0);
	intc_write_4(LPC_INTC_SIC2_RSR, ~0);
	return (0);
}

static device_method_t lpc_intc_methods[] = {
	DEVMETHOD(device_probe,		lpc_intc_probe),
	DEVMETHOD(device_attach,	lpc_intc_attach),
	{ 0, 0 }
};

static driver_t lpc_intc_driver = {
	"pic",
	lpc_intc_methods,
	sizeof(struct lpc_intc_softc),
};

static devclass_t lpc_intc_devclass;

DRIVER_MODULE(pic, simplebus, lpc_intc_driver, lpc_intc_devclass, 0, 0);

int
arm_get_next_irq(int last)
{
	uint32_t value;
	int i;

	/* IRQs 0-31 are mapped to LPC_INTC_MIC_SR */
	value = intc_read_4(LPC_INTC_MIC_SR);
	for (i = 0; i < 32; i++) {
		if (value & (1 << i))
			return (i);
	}

	/* IRQs 32-63 are mapped to LPC_INTC_SIC1_SR */
	value = intc_read_4(LPC_INTC_SIC1_SR);
	for (i = 0; i < 32; i++) {
		if (value & (1 << i))
			return (i + 32);
	}

	/* IRQs 64-95 are mapped to LPC_INTC_SIC2_SR */
	value = intc_read_4(LPC_INTC_SIC2_SR);
	for (i = 0; i < 32; i++) {
		if (value & (1 << i))
			return (i + 64);
	}

	return (-1);
}

void
arm_mask_irq(uintptr_t nb)
{
	int reg;
	uint32_t value;

	/* Make sure that interrupt isn't active already */
	lpc_intc_eoi((void *)nb);

	if (nb > 63) {
		nb -= 64;
		reg = LPC_INTC_SIC2_ER;
	} else if (nb > 31) {
		nb -= 32;
		reg = LPC_INTC_SIC1_ER;
	} else
		reg = LPC_INTC_MIC_ER;

	/* Clear bit in ER register */
	value = intc_read_4(reg);
	value &= ~(1 << nb);
	intc_write_4(reg, value);
}

void
arm_unmask_irq(uintptr_t nb)
{
	int reg;
	uint32_t value;

	if (nb > 63) {
		nb -= 64;
		reg = LPC_INTC_SIC2_ER;
	} else if (nb > 31) {
		nb -= 32;
		reg = LPC_INTC_SIC1_ER;
	} else
		reg = LPC_INTC_MIC_ER;

	/* Set bit in ER register */
	value = intc_read_4(reg);
	value |= (1 << nb);
	intc_write_4(reg, value);
}

static void
lpc_intc_eoi(void *data)
{
	int reg;
	int nb = (int)data;
	uint32_t value;

	if (nb > 63) {
		nb -= 64;
		reg = LPC_INTC_SIC2_RSR;
	} else if (nb > 31) {
		nb -= 32;
		reg = LPC_INTC_SIC1_RSR;
	} else
		reg = LPC_INTC_MIC_RSR;

	/* Set bit in RSR register */
	value = intc_read_4(reg);
	value |= (1 << nb);
	intc_write_4(reg, value);

}

struct fdt_fixup_entry fdt_fixup_table[] = {
	{ NULL, NULL }
};

static int
fdt_pic_decode_ic(phandle_t node, pcell_t *intr, int *interrupt, int *trig,
    int *pol)
{
	if (!fdt_is_compatible(node, "lpc,pic"))
		return (ENXIO);

	*interrupt = fdt32_to_cpu(intr[0]);
	*trig = INTR_TRIGGER_CONFORM;
	*pol = INTR_POLARITY_CONFORM;
	return (0);
}

fdt_pic_decode_t fdt_pic_table[] = {
	&fdt_pic_decode_ic,
	NULL
};
