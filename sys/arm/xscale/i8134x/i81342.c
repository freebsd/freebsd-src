/*-
 * Copyright (c) 2006 Olivier Houchard
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

#define	_ARM32_BUS_DMA_PRIVATE
#include <machine/armreg.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <arm/xscale/i8134x/i81342reg.h>
#include <arm/xscale/i8134x/i81342var.h>

#define	WDTCR_ENABLE1		0x1e1e1e1e
#define	WDTCR_ENABLE2		0xe1e1e1e1

static volatile int intr_enabled0;
static volatile int intr_enabled1;
static volatile int intr_enabled2;
static volatile int intr_enabled3;

struct bus_space i81342_bs_tag;

/* Read the interrupt pending register */

static __inline
uint32_t intpnd0_read(void)
{
	uint32_t ret;

	__asm __volatile("mrc p6, 0, %0, c0, c3, 0"
	    : "=r" (ret));
	return (ret);
}

static __inline
uint32_t intpnd1_read(void)
{
	uint32_t ret;

	__asm __volatile("mrc p6, 0, %0, c1, c3, 0"
	    : "=r" (ret));
	return (ret);
}

static __inline
uint32_t intpnd2_read(void)
{
	uint32_t ret;

	__asm __volatile("mrc p6, 0, %0, c2, c3, 0"
	    : "=r" (ret));
	return (ret);
}

static __inline
uint32_t intpnd3_read(void)
{
	uint32_t ret;

	__asm __volatile("mrc p6, 0, %0, c3, c3, 0"
	    : "=r" (ret));
	return (ret);
}

/* Read the interrupt control register */
/* 0 masked, 1 unmasked */
static __inline
uint32_t intctl0_read(void)
{
	uint32_t ret;

	__asm __volatile("mrc p6, 0, %0, c0, c4, 0"
	    : "=r" (ret));
	return (ret);
}

static __inline
uint32_t intctl1_read(void)
{
	uint32_t ret;

	__asm __volatile("mrc p6, 0, %0, c1, c4, 0"
	    : "=r" (ret));
	return (ret);
}

static __inline
uint32_t intctl2_read(void)
{
	uint32_t ret;

	__asm __volatile("mrc p6, 0, %0, c2, c4, 0"
	    : "=r" (ret));
	return (ret);
}

static __inline
uint32_t intctl3_read(void)
{
	uint32_t ret;

	__asm __volatile("mrc p6, 0, %0, c3, c4, 0"
	    : "=r" (ret));
	return (ret);
}

/* Write the interrupt control register */

static __inline
void intctl0_write(uint32_t val)
{

	__asm __volatile("mcr p6, 0, %0, c0, c4, 0"
	    : : "r" (val));
}

static __inline
void intctl1_write(uint32_t val)
{

	__asm __volatile("mcr p6, 0, %0, c1, c4, 0"
	    : : "r" (val));
}

static __inline
void intctl2_write(uint32_t val)
{

	__asm __volatile("mcr p6, 0, %0, c2, c4, 0"
	    : : "r" (val));
}

static __inline
void intctl3_write(uint32_t val)
{

	__asm __volatile("mcr p6, 0, %0, c3, c4, 0"
	    : : "r" (val));
}

/* Read the interrupt steering register */
/* 0 IRQ 1 FIQ */
static __inline
uint32_t intstr0_read(void)
{
	uint32_t ret;

	__asm __volatile("mrc p6, 0, %0, c0, c5, 0"
	    : "=r" (ret));
	return (ret);
}

static __inline
uint32_t intstr1_read(void)
{
	uint32_t ret;

	__asm __volatile("mrc p6, 0, %0, c1, c5, 0"
	    : "=r" (ret));
	return (ret);
}

static __inline
uint32_t intstr2_read(void)
{
	uint32_t ret;

	__asm __volatile("mrc p6, 0, %0, c2, c5, 0"
	    : "=r" (ret));
	return (ret);
}

static __inline
uint32_t intstr3_read(void)
{
	uint32_t ret;

	__asm __volatile("mrc p6, 0, %0, c3, c5, 0"
	    : "=r" (ret));
	return (ret);
}

/* Write the interrupt steering register */

static __inline
void intstr0_write(uint32_t val)
{

	__asm __volatile("mcr p6, 0, %0, c0, c5, 0"
	    : : "r" (val));
}

static __inline
void intstr1_write(uint32_t val)
{

	__asm __volatile("mcr p6, 0, %0, c1, c5, 0"
	    : : "r" (val));
}

static __inline
void intstr2_write(uint32_t val)
{

	__asm __volatile("mcr p6, 0, %0, c2, c5, 0"
	    : : "r" (val));
}

static __inline
void intstr3_write(uint32_t val)
{

	__asm __volatile("mcr p6, 0, %0, c3, c5, 0"
	    : : "r" (val));
}

void
cpu_reset(void)
{

	disable_interrupts(PSR_I);
	/* XXX: Use the watchdog to reset for now */
	__asm __volatile("mcr p6, 0, %0, c8, c9, 0\n"
	    		 "mcr p6, 0, %1, c7, c9, 0\n"
			 "mcr p6, 0, %2, c7, c9, 0\n"
	    : : "r" (1), "r" (WDTCR_ENABLE1), "r" (WDTCR_ENABLE2));
	while (1);
}

void
arm_mask_irq(uintptr_t nb)
{

	if (nb < 32) {
		intr_enabled0 &= ~(1 << nb);
		intctl0_write(intr_enabled0);
	} else if (nb < 64) {
		intr_enabled1 &= ~(1 << (nb - 32));
		intctl1_write(intr_enabled1);
	} else if (nb < 96) {
		intr_enabled2 &= ~(1 << (nb - 64));
		intctl2_write(intr_enabled2);
	} else {
		intr_enabled3 &= ~(1 << (nb - 96));
		intctl3_write(intr_enabled3);
	}
}

void
arm_unmask_irq(uintptr_t nb)
{
	if (nb < 32) {
		intr_enabled0 |= (1 << nb);
		intctl0_write(intr_enabled0);
	} else if (nb < 64) {
		intr_enabled1 |= (1 << (nb - 32));
		intctl1_write(intr_enabled1);
	} else if (nb < 96) {
		intr_enabled2 |= (1 << (nb - 64));
		intctl2_write(intr_enabled2);
	} else {
		intr_enabled3 |= (1 << (nb - 96));
		intctl3_write(intr_enabled3);
	}
}

int
arm_get_next_irq(int last __unused)
{
	uint32_t val;
	val = intpnd0_read() & intr_enabled0;
	if (val)
		return (ffs(val) - 1);
	val = intpnd1_read() & intr_enabled1;
	if (val)
		return (32 + ffs(val) - 1);
	val = intpnd2_read() & intr_enabled2;
	if (val)
		return (64 + ffs(val) - 1);
	val = intpnd3_read() & intr_enabled3;
	if (val)
		return (96 + ffs(val) - 1);
	return (-1);
}

int
bus_dma_get_range_nb(void)
{
	return (0);
}

struct arm32_dma_range *
bus_dma_get_range(void)
{
	return (NULL);
}

static int
i81342_probe(device_t dev)
{
	unsigned int freq;

	freq = *(volatile unsigned int *)(IOP34X_VADDR + IOP34X_PFR);

	switch (freq & IOP34X_FREQ_MASK) {
	case IOP34X_FREQ_600:
		device_set_desc(dev, "Intel 81342 600MHz");
		break;
	case IOP34X_FREQ_667:
		device_set_desc(dev, "Intel 81342 667MHz");
		break;
	case IOP34X_FREQ_800:
		device_set_desc(dev, "Intel 81342 800MHz");
		break;
	case IOP34X_FREQ_833:
		device_set_desc(dev, "Intel 81342 833MHz");
		break;
	case IOP34X_FREQ_1000:
		device_set_desc(dev, "Intel 81342 1000MHz");
		break;
	case IOP34X_FREQ_1200:
		device_set_desc(dev, "Intel 81342 1200MHz");
		break;
	default:
		device_set_desc(dev, "Intel 81342 unknown frequency");
		break;
	}
	return (0);
}

static void
i81342_identify(driver_t *driver, device_t parent)
{
	
	BUS_ADD_CHILD(parent, 0, "iq", 0);
}

static int
i81342_attach(device_t dev)
{
	struct i81342_softc *sc = device_get_softc(dev);
	uint32_t esstrsr;

	i81342_bs_init(&i81342_bs_tag, sc);
	sc->sc_st = &i81342_bs_tag;
	sc->sc_sh = IOP34X_VADDR;
	esstrsr = bus_space_read_4(sc->sc_st, sc->sc_sh, IOP34X_ESSTSR0);
	sc->sc_atux_sh = IOP34X_ATUX_ADDR(esstrsr) - IOP34X_HWADDR +
	    IOP34X_VADDR;
	sc->sc_atue_sh = IOP34X_ATUE_ADDR(esstrsr) - IOP34X_HWADDR +
	    IOP34X_VADDR;
	/* Disable all interrupts. */
	intctl0_write(0);
	intctl1_write(0);
	intctl2_write(0);
	intctl3_write(0);
	/* Defaults to IRQ */
	intstr0_write(0);
	intstr1_write(0);
	intstr2_write(0);
	intstr3_write(0);
	sc->sc_irq_rman.rm_type = RMAN_ARRAY;
	sc->sc_irq_rman.rm_descr = "i81342 IRQs";
	if (rman_init(&sc->sc_irq_rman) != 0 ||
	    rman_manage_region(&sc->sc_irq_rman, 0, 127) != 0)
		panic("i81342_attach: failed to set up IRQ rman");

	device_add_child(dev, "obio", 0);
	device_add_child(dev, "itimer", 0);
	device_add_child(dev, "iopwdog", 0);
	device_add_child(dev, "pcib", 0);
	device_add_child(dev, "pcib", 1);
	device_add_child(dev, "iqseg", 0);
	bus_generic_probe(dev);
	bus_generic_attach(dev);
	return (0);
}

static struct resource *
i81342_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct i81342_softc *sc = device_get_softc(dev);
	struct resource *rv;

	if (type == SYS_RES_IRQ) {
		rv = rman_reserve_resource(&sc->sc_irq_rman,
		    start, end, count, flags, child);
		if (rv != NULL)
			rman_set_rid(rv, *rid);
		return (rv);
	}
	
	return (NULL);
}

static int
i81342_setup_intr(device_t dev, device_t child, struct resource *ires,
    int flags, driver_filter_t *filt, driver_intr_t *intr, void *arg,
    void **cookiep)
{
	int error;

	error = BUS_SETUP_INTR(device_get_parent(dev), child, ires, flags,
	    filt, intr, arg, cookiep);
	if (error)
		return (error);
	return (0);
}

static int
i81342_teardown_intr(device_t dev, device_t child, struct resource *res,
    void *cookie)
{
	return (BUS_TEARDOWN_INTR(device_get_parent(dev), child, res, cookie));
}

static device_method_t i81342_methods[] = {
	DEVMETHOD(device_probe, i81342_probe),
	DEVMETHOD(device_attach, i81342_attach),
	DEVMETHOD(device_identify, i81342_identify),
	DEVMETHOD(bus_alloc_resource, i81342_alloc_resource),
	DEVMETHOD(bus_setup_intr, i81342_setup_intr),
	DEVMETHOD(bus_teardown_intr, i81342_teardown_intr),
	{0, 0},
};

static driver_t i81342_driver = {
	"iq",
	i81342_methods,
	sizeof(struct i81342_softc),
};
static devclass_t i81342_devclass;

DRIVER_MODULE(iq, nexus, i81342_driver, i81342_devclass, 0, 0);
