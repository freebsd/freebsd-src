/*-
 * Copyright (c) 2011 The FreeBSD Foundation
 * All rights reserved.
 *
 * Developed by Damjan Marion <damjan.marion@gmail.com>
 *
 * Based on OMAP4 GIC code by Ben Gray
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/cpuset.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/smp.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

/* We are using GICv2 register naming */

/* Distributor Registers */
#define GICD_CTLR		0x000			/* v1 ICDDCR */
#define GICD_TYPER		0x004			/* v1 ICDICTR */
#define GICD_IIDR		0x008			/* v1 ICDIIDR */
#define GICD_IGROUPR(n)		(0x0080 + ((n) * 4))	/* v1 ICDISER */
#define GICD_ISENABLER(n)	(0x0100 + ((n) * 4))	/* v1 ICDISER */
#define GICD_ICENABLER(n)	(0x0180 + ((n) * 4))	/* v1 ICDICER */
#define GICD_ISPENDR(n)		(0x0200 + ((n) * 4))	/* v1 ICDISPR */
#define GICD_ICPENDR(n)		(0x0280 + ((n) * 4))	/* v1 ICDICPR */
#define GICD_ICACTIVER(n)	(0x0380 + ((n) * 4))	/* v1 ICDABR */
#define GICD_IPRIORITYR(n)	(0x0400 + ((n) * 4))	/* v1 ICDIPR */
#define GICD_ITARGETSR(n)	(0x0800 + ((n) * 4))	/* v1 ICDIPTR */
#define GICD_ICFGR(n)		(0x0C00 + ((n) * 4))	/* v1 ICDICFR */
#define GICD_SGIR(n)		(0x0F00 + ((n) * 4))	/* v1 ICDSGIR */

/* CPU Registers */
#define GICC_CTLR		0x0000			/* v1 ICCICR */
#define GICC_PMR		0x0004			/* v1 ICCPMR */
#define GICC_BPR		0x0008			/* v1 ICCBPR */
#define GICC_IAR		0x000C			/* v1 ICCIAR */
#define GICC_EOIR		0x0010			/* v1 ICCEOIR */
#define GICC_RPR		0x0014			/* v1 ICCRPR */
#define GICC_HPPIR		0x0018			/* v1 ICCHPIR */
#define GICC_ABPR		0x001C			/* v1 ICCABPR */
#define GICC_IIDR		0x00FC			/* v1 ICCIIDR*/

/* First bit is a polarity bit (0 - low, 1 - high) */
#define GICD_ICFGR_POL_LOW	(0 << 0)
#define GICD_ICFGR_POL_HIGH	(1 << 0)
#define GICD_ICFGR_POL_MASK	0x1
/* Second bit is a trigger bit (0 - level, 1 - edge) */
#define GICD_ICFGR_TRIG_LVL	(0 << 1)
#define GICD_ICFGR_TRIG_EDGE	(1 << 1)
#define GICD_ICFGR_TRIG_MASK	0x2

struct arm_gic_softc {
	struct resource *	gic_res[3];
	bus_space_tag_t		gic_c_bst;
	bus_space_tag_t		gic_d_bst;
	bus_space_handle_t	gic_c_bsh;
	bus_space_handle_t	gic_d_bsh;
	uint8_t			ver;
	device_t		dev;
	struct mtx		mutex;
	uint32_t		nirqs;
};

static struct resource_spec arm_gic_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },	/* Distributor registers */
	{ SYS_RES_MEMORY,	1,	RF_ACTIVE },	/* CPU Interrupt Intf. registers */
	{ -1, 0 }
};

static struct arm_gic_softc *arm_gic_sc = NULL;

#define	gic_c_read_4(reg)		\
    bus_space_read_4(arm_gic_sc->gic_c_bst, arm_gic_sc->gic_c_bsh, reg)
#define	gic_c_write_4(reg, val)		\
    bus_space_write_4(arm_gic_sc->gic_c_bst, arm_gic_sc->gic_c_bsh, reg, val)
#define	gic_d_read_4(reg)		\
    bus_space_read_4(arm_gic_sc->gic_d_bst, arm_gic_sc->gic_d_bsh, reg)
#define	gic_d_write_4(reg, val)		\
    bus_space_write_4(arm_gic_sc->gic_d_bst, arm_gic_sc->gic_d_bsh, reg, val)

static int gic_config_irq(int irq, enum intr_trigger trig,
    enum intr_polarity pol);
static void gic_post_filter(void *);

static int
arm_gic_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "arm,gic"))
		return (ENXIO);
	device_set_desc(dev, "ARM Generic Interrupt Controller");
	return (BUS_PROBE_DEFAULT);
}

void
gic_init_secondary(void)
{
	int i, nirqs;

  	/* Get the number of interrupts */
	nirqs = gic_d_read_4(GICD_TYPER);
	nirqs = 32 * ((nirqs & 0x1f) + 1);

	for (i = 0; i < nirqs; i += 4)
		gic_d_write_4(GICD_IPRIORITYR(i >> 2), 0);

	/* Set all the interrupts to be in Group 0 (secure) */
	for (i = 0; i < nirqs; i += 32) {
		gic_d_write_4(GICD_IGROUPR(i >> 5), 0);
	}

	/* Enable CPU interface */
	gic_c_write_4(GICC_CTLR, 1);

	/* Set priority mask register. */
	gic_c_write_4(GICC_PMR, 0xff);

	/* Enable interrupt distribution */
	gic_d_write_4(GICD_CTLR, 0x01);

	/* Activate IRQ 29, ie private timer IRQ*/
	gic_d_write_4(GICD_ISENABLER(29 >> 5), (1UL << (29 & 0x1F)));
}

static int
arm_gic_attach(device_t dev)
{
	struct		arm_gic_softc *sc;
	int		i;
	uint32_t	icciidr;

	if (arm_gic_sc)
		return (ENXIO);

	sc = device_get_softc(dev);
	sc->dev = dev;

	if (bus_alloc_resources(dev, arm_gic_spec, sc->gic_res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	/* Initialize mutex */
	mtx_init(&sc->mutex, "GIC lock", "", MTX_SPIN);

	/* Distributor Interface */
	sc->gic_d_bst = rman_get_bustag(sc->gic_res[0]);
	sc->gic_d_bsh = rman_get_bushandle(sc->gic_res[0]);

	/* CPU Interface */
	sc->gic_c_bst = rman_get_bustag(sc->gic_res[1]);
	sc->gic_c_bsh = rman_get_bushandle(sc->gic_res[1]);

	arm_gic_sc = sc;

	/* Disable interrupt forwarding to the CPU interface */
	gic_d_write_4(GICD_CTLR, 0x00);

	/* Get the number of interrupts */
	sc->nirqs = gic_d_read_4(GICD_TYPER);
	sc->nirqs = 32 * ((sc->nirqs & 0x1f) + 1);

	/* Set up function pointers */
	arm_post_filter = gic_post_filter;
	arm_config_irq = gic_config_irq;

	icciidr = gic_c_read_4(GICC_IIDR);
	device_printf(dev,"pn 0x%x, arch 0x%x, rev 0x%x, implementer 0x%x sc->nirqs %u\n",
			icciidr>>20, (icciidr>>16) & 0xF, (icciidr>>12) & 0xf,
			(icciidr & 0xfff), sc->nirqs);

	/* Set all global interrupts to be level triggered, active low. */
	for (i = 32; i < sc->nirqs; i += 16) {
		gic_d_write_4(GICD_ICFGR(i >> 4), 0x00000000);
	}

	/* Disable all interrupts. */
	for (i = 32; i < sc->nirqs; i += 32) {
		gic_d_write_4(GICD_ICENABLER(i >> 5), 0xFFFFFFFF);
	}

	for (i = 0; i < sc->nirqs; i += 4) {
		gic_d_write_4(GICD_IPRIORITYR(i >> 2), 0);
		gic_d_write_4(GICD_ITARGETSR(i >> 2), 1 << 0 | 1 << 8 | 1 << 16 | 1 << 24);
	}

	/* Set all the interrupts to be in Group 0 (secure) */
	for (i = 0; i < sc->nirqs; i += 32) {
		gic_d_write_4(GICD_IGROUPR(i >> 5), 0);
	}

	/* Enable CPU interface */
	gic_c_write_4(GICC_CTLR, 1);

	/* Set priority mask register. */
	gic_c_write_4(GICC_PMR, 0xff);

	/* Enable interrupt distribution */
	gic_d_write_4(GICD_CTLR, 0x01);

	return (0);
}

static device_method_t arm_gic_methods[] = {
	DEVMETHOD(device_probe,		arm_gic_probe),
	DEVMETHOD(device_attach,	arm_gic_attach),
	{ 0, 0 }
};

static driver_t arm_gic_driver = {
	"gic",
	arm_gic_methods,
	sizeof(struct arm_gic_softc),
};

static devclass_t arm_gic_devclass;

DRIVER_MODULE(gic, simplebus, arm_gic_driver, arm_gic_devclass, 0, 0);

static void
gic_post_filter(void *arg)
{
	uintptr_t irq = (uintptr_t) arg;

	gic_c_write_4(GICC_EOIR, irq);
}

int
arm_get_next_irq(int last_irq)
{
	uint32_t active_irq;

	active_irq = gic_c_read_4(GICC_IAR);

	/*
	 * Immediatly EOIR the SGIs, because doing so requires the other
	 * bits (ie CPU number), not just the IRQ number, and we do not
	 * have this information later.
	 */

	if ((active_irq & 0x3ff) < 16)
		gic_c_write_4(GICC_EOIR, active_irq);
	active_irq &= 0x3FF;

	if (active_irq == 0x3FF) {
		if (last_irq == -1)
			printf("Spurious interrupt detected [0x%08x]\n", active_irq);
		return -1;
	}

	return active_irq;
}

void
arm_mask_irq(uintptr_t nb)
{

	gic_d_write_4(GICD_ICENABLER(nb >> 5), (1UL << (nb & 0x1F)));
	gic_c_write_4(GICC_EOIR, nb);
}

void
arm_unmask_irq(uintptr_t nb)
{

	gic_d_write_4(GICD_ISENABLER(nb >> 5), (1UL << (nb & 0x1F)));
}

static int
gic_config_irq(int irq, enum intr_trigger trig,
    enum intr_polarity pol)
{
	uint32_t reg;
	uint32_t mask;

	/* Function is public-accessible, so validate input arguments */
	if ((irq < 0) || (irq >= arm_gic_sc->nirqs))
		goto invalid_args;
	if ((trig != INTR_TRIGGER_EDGE) && (trig != INTR_TRIGGER_LEVEL) &&
	    (trig != INTR_TRIGGER_CONFORM))
		goto invalid_args;
	if ((pol != INTR_POLARITY_HIGH) && (pol != INTR_POLARITY_LOW) &&
	    (pol != INTR_POLARITY_CONFORM))
		goto invalid_args;

	mtx_lock_spin(&arm_gic_sc->mutex);

	reg = gic_d_read_4(GICD_ICFGR(irq >> 4));
	mask = (reg >> 2*(irq % 16)) & 0x3;

	if (pol == INTR_POLARITY_LOW) {
		mask &= ~GICD_ICFGR_POL_MASK;
		mask |= GICD_ICFGR_POL_LOW;
	} else if (pol == INTR_POLARITY_HIGH) {
		mask &= ~GICD_ICFGR_POL_MASK;
		mask |= GICD_ICFGR_POL_HIGH;
	}

	if (trig == INTR_TRIGGER_LEVEL) {
		mask &= ~GICD_ICFGR_TRIG_MASK;
		mask |= GICD_ICFGR_TRIG_LVL;
	} else if (trig == INTR_TRIGGER_EDGE) {
		mask &= ~GICD_ICFGR_TRIG_MASK;
		mask |= GICD_ICFGR_TRIG_EDGE;
	}

	/* Set mask */
	reg = reg & ~(0x3 << 2*(irq % 16));
	reg = reg | (mask << 2*(irq % 16));
	gic_d_write_4(GICD_ICFGR(irq >> 4), reg);

	mtx_unlock_spin(&arm_gic_sc->mutex);

	return (0);

invalid_args:
	device_printf(arm_gic_sc->dev, "gic_config_irg, invalid parameters\n");
	return (EINVAL);
}

#ifdef SMP
void
pic_ipi_send(cpuset_t cpus, u_int ipi)
{
	uint32_t val = 0, i;

	for (i = 0; i < MAXCPU; i++)
		if (CPU_ISSET(i, &cpus))
			val |= 1 << (16 + i);
	gic_d_write_4(GICD_SGIR(0), val | ipi);

}

int
pic_ipi_get(int i)
{

	if (i != -1) {
		/*
		 * The intr code will automagically give the frame pointer
		 * if the interrupt argument is 0.
		 */
		if ((unsigned int)i > 16)
			return (0);
		return (i);
	}
	return (0x3ff);
}

void
pic_ipi_clear(int ipi)
{
}
#endif

