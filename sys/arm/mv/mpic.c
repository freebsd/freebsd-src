/*-
 * Copyright (c) 2006 Benno Rice.
 * Copyright (C) 2007-2011 MARVELL INTERNATIONAL LTD.
 * All rights reserved.
 *
 * Developed by Semihalf.
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
 * from: FreeBSD: src/sys/arm/mv/ic.c,v 1.5 2011/02/08 01:49:30
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/cpuset.h>
#include <sys/ktr.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/cpufunc.h>
#include <machine/smp.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#define IRQ_ERR			4
#define MAIN_IRQS		116

#define IRQ_MASK		0x3ff

#define MPIC_CTRL		0x0
#define MPIC_SOFT_INT		0x4
#define MPIC_ERR_CAUSE		0x20
#define MPIC_ISE		0x30
#define MPIC_ICE		0x34


#define MPIC_IN_DOORBELL	0x78
#define MPIC_IN_DOORBELL_MASK	0x7c
#define MPIC_CTP		0xb0
#define MPIC_CTP		0xb0
#define MPIC_IIACK		0xb4
#define MPIC_ISM		0xb8
#define MPIC_ICM		0xbc
#define MPIC_ERR_MASK		0xec0

struct mv_mpic_softc {
	struct resource	*	mpic_res[2];
	bus_space_tag_t		mpic_bst;
	bus_space_handle_t	mpic_bsh;
	bus_space_tag_t		cpu_bst;
	bus_space_handle_t	cpu_bsh;
	int			mpic_high_regs;
	int			mpic_error_regs;
};

static struct resource_spec mv_mpic_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_MEMORY,	1,	RF_ACTIVE },
	{ -1, 0 }
};

static struct mv_mpic_softc *mv_mpic_sc = NULL;

void mpic_send_ipi(int cpus, u_int ipi);

static int	mv_mpic_probe(device_t);
static int	mv_mpic_attach(device_t);
uint32_t	mv_mpic_get_cause(void);
uint32_t	mv_mpic_get_cause_err(void);
static void	arm_mask_irq_err(uintptr_t);
static void	arm_unmask_irq_err(uintptr_t);

#define MPIC_CPU_WRITE(softc, reg, val) \
    bus_space_write_4((softc)->cpu_bst, (softc)->cpu_bsh, (reg), (val))
#define MPIC_CPU_READ(softc, reg) \
    bus_space_read_4((softc)->cpu_bst, (softc)->cpu_bsh, (reg))

static int
mv_mpic_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "mrvl,mpic"))
		return (ENXIO);

	device_set_desc(dev, "Marvell Integrated Interrupt Controller");
	return (0);
}

static int
mv_mpic_attach(device_t dev)
{
	struct mv_mpic_softc *sc;
	int error;

	sc = (struct mv_mpic_softc *)device_get_softc(dev);

	if (mv_mpic_sc != NULL)
		return (ENXIO);
	mv_mpic_sc = sc;

	error = bus_alloc_resources(dev, mv_mpic_spec, sc->mpic_res);
	if (error) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	sc->mpic_bst = rman_get_bustag(sc->mpic_res[0]);
	sc->mpic_bsh = rman_get_bushandle(sc->mpic_res[0]);

	sc->cpu_bst = rman_get_bustag(sc->mpic_res[1]);
	sc->cpu_bsh = rman_get_bushandle(sc->mpic_res[1]);

	bus_space_write_4(mv_mpic_sc->mpic_bst, mv_mpic_sc->mpic_bsh,
	    MPIC_CTRL, 1);
	MPIC_CPU_WRITE(mv_mpic_sc, MPIC_CTP, 0);

	return (0);
}

static device_method_t mv_mpic_methods[] = {
	DEVMETHOD(device_probe,		mv_mpic_probe),
	DEVMETHOD(device_attach,	mv_mpic_attach),
	{ 0, 0 }
};

static driver_t mv_mpic_driver = {
	"mpic",
	mv_mpic_methods,
	sizeof(struct mv_mpic_softc),
};

static devclass_t mv_mpic_devclass;

DRIVER_MODULE(mpic, simplebus, mv_mpic_driver, mv_mpic_devclass, 0, 0);

int
arm_get_next_irq(int last)
{
	u_int irq, next = -1;

	irq = mv_mpic_get_cause() & IRQ_MASK;
	CTR2(KTR_INTR, "%s: irq:%#x", __func__, irq);

	if (irq != IRQ_MASK) {
		if (irq == IRQ_ERR)
			irq = mv_mpic_get_cause_err();
		next = irq;
	}

	CTR3(KTR_INTR, "%s: last=%d, next=%d", __func__, last, next);
	return (next);
}

/*
 * XXX We can make arm_enable_irq to operate on ICE and then mask/unmask only
 * by ISM/ICM and remove access to ICE in masking operation
 */
void
arm_mask_irq(uintptr_t nb)
{

	MPIC_CPU_WRITE(mv_mpic_sc, MPIC_CTP, 1);

	if (nb < MAIN_IRQS) {
		bus_space_write_4(mv_mpic_sc->mpic_bst, mv_mpic_sc->mpic_bsh,
		    MPIC_ICE, nb);
		MPIC_CPU_WRITE(mv_mpic_sc, MPIC_ISM, nb);
	} else
		arm_mask_irq_err(nb);
}


static void
arm_mask_irq_err(uintptr_t nb)
{
	uint32_t mask;
	uint8_t bit_off;

	bit_off = nb - MAIN_IRQS;
	mask = MPIC_CPU_READ(mv_mpic_sc, MPIC_ERR_MASK);
	mask &= ~(1 << bit_off);
	MPIC_CPU_WRITE(mv_mpic_sc, MPIC_ERR_MASK, mask);
}

void
arm_unmask_irq(uintptr_t nb)
{

	MPIC_CPU_WRITE(mv_mpic_sc, MPIC_CTP, 0);

	if (nb < MAIN_IRQS) {
		bus_space_write_4(mv_mpic_sc->mpic_bst, mv_mpic_sc->mpic_bsh,
		    MPIC_ISE, nb);
		MPIC_CPU_WRITE(mv_mpic_sc, MPIC_ICM, nb);
	} else
		arm_unmask_irq_err(nb);

	if (nb == 0)
		MPIC_CPU_WRITE(mv_mpic_sc, MPIC_IN_DOORBELL_MASK, 0xffffffff);
}

void
arm_unmask_irq_err(uintptr_t nb)
{
	uint32_t mask;
	uint8_t bit_off;

	bus_space_write_4(mv_mpic_sc->mpic_bst, mv_mpic_sc->mpic_bsh,
	    MPIC_ISE, IRQ_ERR);
	MPIC_CPU_WRITE(mv_mpic_sc, MPIC_ICM, IRQ_ERR);

	bit_off = nb - MAIN_IRQS;
	mask = MPIC_CPU_READ(mv_mpic_sc, MPIC_ERR_MASK);
	mask |= (1 << bit_off);
	MPIC_CPU_WRITE(mv_mpic_sc, MPIC_ERR_MASK, mask);
}

uint32_t
mv_mpic_get_cause(void)
{

	return (MPIC_CPU_READ(mv_mpic_sc, MPIC_IIACK));
}

uint32_t
mv_mpic_get_cause_err(void)
{
	uint32_t err_cause;
	uint8_t bit_off;

	err_cause = bus_space_read_4(mv_mpic_sc->mpic_bst,
	    mv_mpic_sc->mpic_bsh, MPIC_ERR_CAUSE);

	if (err_cause)
		bit_off = ffs(err_cause) - 1;
	else
		return (-1);
	return (MAIN_IRQS + bit_off);
}

#if defined(SMP)
void
pic_ipi_send(cpuset_t cpus, u_int ipi)
{
	uint32_t val, i;

	val = 0x00000000;
	for (i = 0; i < MAXCPU; i++)
		if (CPU_ISSET(i, &cpus))
			val |= (1 << (8 + i));
	val |= ipi;
	bus_space_write_4(mv_mpic_sc->mpic_bst, mv_mpic_sc->mpic_bsh,
	    MPIC_SOFT_INT, val);
}

int
pic_ipi_get(int i __unused)
{
	uint32_t val;

	val = MPIC_CPU_READ(mv_mpic_sc, MPIC_IN_DOORBELL);
	if (val)
		return (ffs(val) - 1);

	return (0x3ff);
}

void
pic_ipi_clear(int ipi)
{
	uint32_t val;

	val = ~(1 << ipi);
	MPIC_CPU_WRITE(mv_mpic_sc, MPIC_IN_DOORBELL, val);
}

#endif
