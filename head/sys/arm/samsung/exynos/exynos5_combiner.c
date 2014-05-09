/*-
 * Copyright (c) 2014 Ruslan Bukin <br@bsdpad.com>
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
 */

/*
 * Samsung Exynos 5 Interrupt Combiner
 * Chapter 7, Exynos 5 Dual User's Manual Public Rev 1.00
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
#include <sys/timeet.h>
#include <sys/timetc.h>
#include <sys/watchdog.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>
#include <machine/fdt.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include <arm/samsung/exynos/exynos5_common.h>
#include <arm/samsung/exynos/exynos5_combiner.h>

#define NGRP		32
#define ITABLE_LEN	24

#define	IESR(n)	(0x10 * n + 0x0)	/* Interrupt enable set */
#define	IECR(n)	(0x10 * n + 0x4)	/* Interrupt enable clear */
#define	ISTR(n)	(0x10 * n + 0x8)	/* Interrupt status */
#define	IMSR(n)	(0x10 * n + 0xC)	/* Interrupt masked status */
#define	CIPSR	0x100			/* Combined interrupt pending */

struct combiner_softc {
	struct resource		*res[1 + NGRP];
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	void			*ih[NGRP];
	device_t		dev;
};

struct combiner_softc *combiner_sc;

static struct resource_spec combiner_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		1,	RF_ACTIVE },
	{ SYS_RES_IRQ,		2,	RF_ACTIVE },
	{ SYS_RES_IRQ,		3,	RF_ACTIVE },
	{ SYS_RES_IRQ,		4,	RF_ACTIVE },
	{ SYS_RES_IRQ,		5,	RF_ACTIVE },
	{ SYS_RES_IRQ,		6,	RF_ACTIVE },
	{ SYS_RES_IRQ,		7,	RF_ACTIVE },
	{ SYS_RES_IRQ,		8,	RF_ACTIVE },
	{ SYS_RES_IRQ,		9,	RF_ACTIVE },
	{ SYS_RES_IRQ,		10,	RF_ACTIVE },
	{ SYS_RES_IRQ,		11,	RF_ACTIVE },
	{ SYS_RES_IRQ,		12,	RF_ACTIVE },
	{ SYS_RES_IRQ,		13,	RF_ACTIVE },
	{ SYS_RES_IRQ,		14,	RF_ACTIVE },
	{ SYS_RES_IRQ,		15,	RF_ACTIVE },
	{ SYS_RES_IRQ,		16,	RF_ACTIVE },
	{ SYS_RES_IRQ,		17,	RF_ACTIVE },
	{ SYS_RES_IRQ,		18,	RF_ACTIVE },
	{ SYS_RES_IRQ,		19,	RF_ACTIVE },
	{ SYS_RES_IRQ,		20,	RF_ACTIVE },
	{ SYS_RES_IRQ,		21,	RF_ACTIVE },
	{ SYS_RES_IRQ,		22,	RF_ACTIVE },
	{ SYS_RES_IRQ,		23,	RF_ACTIVE },
	{ SYS_RES_IRQ,		24,	RF_ACTIVE },
	{ SYS_RES_IRQ,		25,	RF_ACTIVE },
	{ SYS_RES_IRQ,		26,	RF_ACTIVE },
	{ SYS_RES_IRQ,		27,	RF_ACTIVE },
	{ SYS_RES_IRQ,		28,	RF_ACTIVE },
	{ SYS_RES_IRQ,		29,	RF_ACTIVE },
	{ SYS_RES_IRQ,		30,	RF_ACTIVE },
	{ SYS_RES_IRQ,		31,	RF_ACTIVE },
	{ -1, 0 }
};

struct combiner_entry {
	int combiner_id;
	int bit;
	char *source_name;
};

static struct combiner_entry interrupt_table[ITABLE_LEN] = {
	{ 63, 1, "EINT[15]" },
	{ 63, 0, "EINT[14]" },
	{ 62, 1, "EINT[13]" },
	{ 62, 0, "EINT[12]" },
	{ 61, 1, "EINT[11]" },
	{ 61, 0, "EINT[10]" },
	{ 60, 1, "EINT[9]" },
	{ 60, 0, "EINT[8]" },
	{ 59, 1, "EINT[7]" },
	{ 59, 0, "EINT[6]" },
	{ 58, 1, "EINT[5]" },
	{ 58, 0, "EINT[4]" },
	{ 57, 3, "MCT_G3" },
	{ 57, 2, "MCT_G2" },
	{ 57, 1, "EINT[3]" },
	{ 57, 0, "EINT[2]" },
	{ 56, 6, "SYSMMU_G2D[1]" },
	{ 56, 5, "SYSMMU_G2D[0]" },
	{ 56, 2, "SYSMMU_FIMC_LITE1[1]" },
	{ 56, 1, "SYSMMU_FIMC_LITE1[0]" },
	{ 56, 0, "EINT[1]" },
	{ 55, 4, "MCT_G1" },
	{ 55, 3, "MCT_G0" },
	{ 55, 0, "EINT[0]" },

	/* TODO: add groups 54-32 */
};

struct combined_intr {
	uint32_t	enabled;
	void		(*ih) (void *);
	void		*ih_user;
};

static struct combined_intr intr_map[32][8];

static void
combiner_intr(void *arg)
{
	struct combiner_softc *sc;
	void (*ih) (void *);
	void *ih_user;
	int enabled;
	int intrs;
	int shift;
	int cirq;
	int grp;
	int i,n;

	sc = arg;

	intrs = READ4(sc, CIPSR);
	for (grp = 0; grp < 32; grp++) {
		if (intrs & (1 << grp)) {
			n = (grp / 4);
			shift = (grp % 4) * 8;

			cirq = READ4(sc, ISTR(n));
			for (i = 0; i < 8; i++) {
				if (cirq & (1 << (i + shift))) {
					ih = intr_map[grp][i].ih;
					ih_user = intr_map[grp][i].ih_user;
					enabled = intr_map[grp][i].enabled;
					if (enabled && (ih != NULL)) {
						ih(ih_user);
					}
				}
			}
		}
	}
}

void
combiner_setup_intr(char *source_name, void (*ih)(void *), void *ih_user)
{
	struct combiner_entry *entry;
	struct combined_intr *cirq;
	struct combiner_softc *sc;
	int shift;
	int reg;
	int grp;
	int n;
	int i;

	sc = combiner_sc;

	if (sc == NULL) {
		device_printf(sc->dev, "Error: combiner is not attached\n");
		return;
	}

	entry = NULL;

	for (i = 0; i < ITABLE_LEN; i++) {
		if (strcmp(interrupt_table[i].source_name, source_name) == 0) {
			entry = &interrupt_table[i];
		}
	}

	if (entry == NULL) {
		device_printf(sc->dev, "Can't find interrupt name %s\n",
		    source_name);
		return;
	}

#if 0
	device_printf(sc->dev, "Setting up interrupt %s\n", source_name);
#endif

	grp = entry->combiner_id - 32;

	cirq = &intr_map[grp][entry->bit];
	cirq->enabled = 1;
	cirq->ih = ih;
	cirq->ih_user = ih_user;

	n = grp / 4;
	shift = (grp % 4) * 8 + entry->bit;

	reg = (1 << shift);
	WRITE4(sc, IESR(n), reg);
}

static int
combiner_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "exynos,combiner"))
		return (ENXIO);

	device_set_desc(dev, "Samsung Exynos 5 Interrupt Combiner");
	return (BUS_PROBE_DEFAULT);
}

static int
combiner_attach(device_t dev)
{
	struct combiner_softc *sc;
	int err;
	int i;

	sc = device_get_softc(dev);
	sc->dev = dev;

	if (bus_alloc_resources(dev, combiner_spec, sc->res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	/* Memory interface */
	sc->bst = rman_get_bustag(sc->res[0]);
	sc->bsh = rman_get_bushandle(sc->res[0]);

	combiner_sc = sc;

        /* Setup interrupt handler */
	for (i = 0; i < NGRP; i++) {
		err = bus_setup_intr(dev, sc->res[1+i], INTR_TYPE_BIO | \
		    INTR_MPSAFE, NULL, combiner_intr, sc, &sc->ih[i]);
		if (err) {
			device_printf(dev, "Unable to alloc int resource.\n");
			return (ENXIO);
		}
	}

	return (0);
}

static device_method_t combiner_methods[] = {
	DEVMETHOD(device_probe,		combiner_probe),
	DEVMETHOD(device_attach,	combiner_attach),
	{ 0, 0 }
};

static driver_t combiner_driver = {
	"combiner",
	combiner_methods,
	sizeof(struct combiner_softc),
};

static devclass_t combiner_devclass;

DRIVER_MODULE(combiner, simplebus, combiner_driver, combiner_devclass, 0, 0);
