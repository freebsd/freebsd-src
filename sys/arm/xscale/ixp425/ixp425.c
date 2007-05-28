/*	$NetBSD: ixp425.c,v 1.10 2005/12/11 12:16:51 christos Exp $ */

/*
 * Copyright (c) 2003
 *	Ichiro FUKUHARA <ichiro@ichiro.org>.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Ichiro FUKUHARA.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ICHIRO FUKUHARA ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ICHIRO FUKUHARA OR THE VOICES IN HIS HEAD BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define _ARM32_BUS_DMA_PRIVATE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <arm/xscale/ixp425/ixp425reg.h>
#include <arm/xscale/ixp425/ixp425var.h>
#include <arm/xscale/ixp425/ixp425_intr.h>

#include <dev/pci/pcireg.h>

volatile uint32_t intr_enabled;
uint32_t intr_steer = 0;

struct	ixp425_softc *ixp425_softc = NULL;

static int	ixp425_probe(device_t);
static void	ixp425_identify(driver_t *, device_t);
static int	ixp425_attach(device_t);

static struct {
	uint32_t	hwbase;
	uint32_t	size;
	uint32_t	vbase;
} hwvtrans[] = {
	{ IXP425_IO_HWBASE,	IXP425_IO_SIZE,		IXP425_IO_VBASE },
	{ IXP425_EXP_HWBASE,	IXP425_EXP_SIZE,	IXP425_EXP_VBASE },
	{ IXP425_PCI_HWBASE,	IXP425_PCI_SIZE,	IXP425_PCI_VBASE },
	{ IXP425_PCI_MEM_HWBASE,IXP425_PCI_MEM_SIZE,	IXP425_PCI_MEM_VBASE },
#if 0
	{ IXP425_PCI_IO_HWBASE,	IXP425_PCI_IO_SIZE,	IXP425_PCI_IO_VBASE },
#endif
	{ IXP425_MCU_HWBASE,	IXP425_MCU_SIZE,	IXP425_MCU_VBASE },
	{ IXP425_QMGR_HWBASE,	IXP425_QMGR_SIZE,	IXP425_QMGR_VBASE },
	{ IXP425_NPE_A_HWBASE,	IXP425_NPE_A_SIZE,	IXP425_NPE_A_VBASE },
	{ IXP425_NPE_B_HWBASE,	IXP425_NPE_B_SIZE,	IXP425_NPE_B_VBASE },
	{ IXP425_NPE_C_HWBASE,	IXP425_NPE_C_SIZE,	IXP425_NPE_C_VBASE },
	{ IXP425_MAC_A_HWBASE,	IXP425_MAC_A_SIZE,	IXP425_MAC_A_VBASE },
	{ IXP425_MAC_B_HWBASE,	IXP425_MAC_B_SIZE,	IXP425_MAC_B_VBASE },
	/* Gateworks Avila IDE/CF is mapped here */
	{ IXP425_EXP_BUS_CS1_HWBASE, IXP425_EXP_BUS_SIZE,
	  IXP425_EXP_BUS_CS1_VBASE },
	{ IXP425_EXP_BUS_CS2_HWBASE, IXP425_EXP_BUS_SIZE,
	  IXP425_EXP_BUS_CS2_VBASE },
	/* ADI Pronghorn Metro IDE/CF is mapped here */
	{ IXP425_EXP_BUS_CS3_HWBASE, IXP425_EXP_BUS_SIZE,
	  IXP425_EXP_BUS_CS3_VBASE },
	{ IXP425_EXP_BUS_CS4_HWBASE, IXP425_EXP_BUS_SIZE,
	  IXP425_EXP_BUS_CS4_VBASE },
};

static int
getvbase(uint32_t hwbase, uint32_t size, uint32_t *vbase)
{
	int i;

	for (i = 0; i < sizeof hwvtrans / sizeof *hwvtrans; i++) {
		if (hwbase >= hwvtrans[i].hwbase &&
		    hwbase + size <= hwvtrans[i].hwbase + hwvtrans[i].size) {
			*vbase = hwbase - hwvtrans[i].hwbase + hwvtrans[i].vbase;
			return (0);
		}
	}

	return (ENOENT);
}

struct arm32_dma_range *
bus_dma_get_range(void)
{
	return (NULL);
}

int
bus_dma_get_range_nb(void)
{
	return (0);
}

static __inline u_int32_t
ixp425_irq2gpio_bit(int irq)
{

	static const uint8_t int2gpio[32] __attribute__ ((aligned(32))) = {
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	/* INT#0 -> INT#5 */
		0x00, 0x01,				/* GPIO#0 -> GPIO#1 */
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	/* INT#8 -> INT#13 */
		0xff, 0xff, 0xff, 0xff, 0xff,		/* INT#14 -> INT#18 */
		0x02, 0x03, 0x04, 0x05, 0x06, 0x07,	/* GPIO#2 -> GPIO#7 */
		0x08, 0x09, 0x0a, 0x0b, 0x0c,		/* GPIO#8 -> GPIO#12 */
		0xff, 0xff				/* INT#30 -> INT#31 */
	};

	return (1U << int2gpio[irq]);
}

void
arm_mask_irq(uintptr_t nb)
{
	intr_enabled &= ~(1 << nb);
	ixp425_set_intrmask();
	/*XXX; If it's a GPIO interrupt, ACK it know. Can it be a problem ?*/
	if ((1 << nb) & IXP425_INT_GPIOMASK)
		IXPREG(IXP425_GPIO_VBASE + IXP425_GPIO_GPISR) =
		    ixp425_irq2gpio_bit(nb);

		
}

void
arm_unmask_irq(uintptr_t nb)
{

	intr_enabled |= (1 << nb);
	ixp425_set_intrmask();
}

static __inline uint32_t
ixp425_irq_read(void)
{
	return IXPREG(IXP425_INT_STATUS) & intr_enabled;
}

int
arm_get_next_irq(void)
{
	int irq;

	if ((irq = ixp425_irq_read()))
		return (ffs(irq) - 1);
	return (-1);
}

void
cpu_reset(void)
{

	bus_space_write_4(&ixp425_bs_tag, IXP425_TIMER_VBASE,
	    IXP425_OST_WDOG_KEY, OST_WDOG_KEY_MAJICK);
	bus_space_write_4(&ixp425_bs_tag, IXP425_TIMER_VBASE,
	    IXP425_OST_WDOG, 0);
	bus_space_write_4(&ixp425_bs_tag, IXP425_TIMER_VBASE,
	    IXP425_OST_WDOG_ENAB, OST_WDOG_ENAB_RST_ENA |
	    OST_WDOG_ENAB_CNT_ENA);
	printf("Reset failed!\n");
	for(;;);
}

static void
ixp425_identify(driver_t *driver, device_t parent)
{
	BUS_ADD_CHILD(parent, 0, "ixp", 0);
}

static int
ixp425_probe(device_t dev)
{
	device_set_desc(dev, "Intel IXP425");
	return (0);
}

static int
ixp425_attach(device_t dev)
{
	struct ixp425_softc *sc;

	sc = device_get_softc(dev);
	sc->sc_iot = &ixp425_bs_tag;
	KASSERT(ixp425_softc == NULL, ("ixp425_attach called twice?"));
	ixp425_softc = sc;

	intr_enabled = 0;
	ixp425_set_intrmask();
	ixp425_set_intrsteer();

	if (bus_dma_tag_create(NULL, 1, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL,  0xffffffff, 0xff, 0xffffffff, 0, 
	    NULL, NULL, &sc->sc_dmat))
		panic("couldn't create the IXP425 dma tag !");

	sc->sc_irq_rman.rm_type = RMAN_ARRAY;
	sc->sc_irq_rman.rm_descr = "IXP425 IRQs";
	if (rman_init(&sc->sc_irq_rman) != 0 ||
	    rman_manage_region(&sc->sc_irq_rman, 0, 31) != 0)
		panic("ixp425_attach: failed to set up IRQ rman");

	sc->sc_mem_rman.rm_type = RMAN_ARRAY;
	sc->sc_mem_rman.rm_descr = "IXP425 Memory";
	if (rman_init(&sc->sc_mem_rman) != 0 ||
	    rman_manage_region(&sc->sc_mem_rman, 0, ~0) != 0)
		panic("ixp425_attach: failed to set up IRQ rman");

	BUS_ADD_CHILD(dev, 0, "pcib", 0);
	BUS_ADD_CHILD(dev, 0, "ixpclk", 0);
	BUS_ADD_CHILD(dev, 0, "ixpiic", 0);
	/* XXX move to hints? */
	BUS_ADD_CHILD(dev, 0, "ixpwdog", 0);

	/* attach wired devices via hints */
	bus_enumerate_hinted_children(dev);

	if (bus_space_map(sc->sc_iot, IXP425_GPIO_HWBASE, IXP425_GPIO_SIZE,
	    0, &sc->sc_gpio_ioh))
		panic("ixp425_attach: unable to map GPIO registers");
	if (bus_space_map(sc->sc_iot, IXP425_EXP_HWBASE, IXP425_EXP_SIZE,
	    0, &sc->sc_exp_ioh))
		panic("ixp425_attach: unable to map Expansion Bus registers");

	bus_generic_probe(dev);
	bus_generic_attach(dev);

	return (0);
}

static void
ixp425_hinted_child(device_t bus, const char *dname, int dunit)
{
	device_t child;
	struct ixp425_ivar *ivar;

	child = BUS_ADD_CHILD(bus, 0, dname, dunit);
	ivar = IXP425_IVAR(child);
	resource_int_value(dname, dunit, "addr", &ivar->addr);
	resource_int_value(dname, dunit, "irq", &ivar->irq);
}

static device_t
ixp425_add_child(device_t dev, int order, const char *name, int unit)
{
	device_t child;
	struct ixp425_ivar *ivar;

	child = device_add_child_ordered(dev, order, name, unit);
	if (child == NULL)
		return NULL;
	ivar = malloc(sizeof(struct ixp425_ivar), M_DEVBUF, M_NOWAIT);
	if (ivar == NULL) {
		device_delete_child(dev, child);
		return NULL;
	}
	ivar->addr = 0;
	ivar->irq = -1;
	device_set_ivars(child, ivar);
	return child;
}

static int
ixp425_read_ivar(device_t bus, device_t child, int which, u_char *result)
{
	struct ixp425_ivar *ivar = IXP425_IVAR(child);

	switch (which) {
	case IXP425_IVAR_ADDR:
		if (ivar->addr != 0) {
			*(uint32_t *)result = ivar->addr;
			return 0;
		}
		break;
	case IXP425_IVAR_IRQ:
		if (ivar->irq != -1) {
			*(int *)result = ivar->irq;
			return 0;
		}
		break;
	}
	return EINVAL;
}

static struct resource *
ixp425_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct ixp425_softc *sc = device_get_softc(dev);
	struct rman *rmanp;
	struct resource *rv;
	uint32_t vbase, addr;
	int irq;

	switch (type) {
	case SYS_RES_IRQ:
		rmanp = &sc->sc_irq_rman;
		/* override per hints */
		if (BUS_READ_IVAR(dev, child, IXP425_IVAR_IRQ, &irq) == 0)
			start = end = irq;
		rv = rman_reserve_resource(rmanp, start, end, count,
			flags, child);
		if (rv != NULL)
			rman_set_rid(rv, *rid);
		break;

	case SYS_RES_MEMORY:
		rmanp = &sc->sc_mem_rman;
		/* override per hints */
		if (BUS_READ_IVAR(dev, child, IXP425_IVAR_ADDR, &addr) == 0) {
			start = addr;
			end = start + 0x1000;	/* XXX */
		}
		if (getvbase(start, end - start, &vbase))
			return NULL;
		rv = rman_reserve_resource(rmanp, start, end, count,
			flags, child);
		if (rv != NULL) {
			rman_set_rid(rv, *rid);
			if (strcmp(device_get_name(child), "uart") == 0)
				rman_set_bustag(rv, &ixp425_a4x_bs_tag);
			else
				rman_set_bustag(rv, sc->sc_iot);
			rman_set_bushandle(rv, vbase);
		}
		break;
	default:
		rv = NULL;
		break;
	}
	return rv;
}

static int
ixp425_setup_intr(device_t dev, device_t child,
    struct resource *ires, int flags, driver_filter_t *filt, 
    driver_intr_t *intr, void *arg, void **cookiep)    
{
	uint32_t mask;
	int i;

	BUS_SETUP_INTR(device_get_parent(dev), child, ires, flags, filt, intr,
	     arg, cookiep);

	mask = 0;
	for (i = rman_get_start(ires); i <= rman_get_end(ires); i++)
		mask |= 1 << i;
	intr_enabled |= mask;
	ixp425_set_intrmask();

	return (0);
}

static int
ixp425_teardown_intr(device_t dev, device_t child, struct resource *res,
    void *cookie)
{
	uint32_t mask;
	int i;

	mask = 0;
	for (i = rman_get_start(res); i <= rman_get_end(res); i++)
		mask |= 1 << i;
	intr_enabled &= ~mask;
	ixp425_set_intrmask();

	return (BUS_TEARDOWN_INTR(device_get_parent(dev), child, res, cookie));
}

static device_method_t ixp425_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, ixp425_probe),
	DEVMETHOD(device_attach, ixp425_attach),
	DEVMETHOD(device_identify, ixp425_identify),

	/* Bus interface */
	DEVMETHOD(bus_add_child, ixp425_add_child),
	DEVMETHOD(bus_hinted_child, ixp425_hinted_child),
	DEVMETHOD(bus_read_ivar, ixp425_read_ivar),

	DEVMETHOD(bus_alloc_resource, ixp425_alloc_resource),
	DEVMETHOD(bus_setup_intr, ixp425_setup_intr),
	DEVMETHOD(bus_teardown_intr, ixp425_teardown_intr),

	{0, 0},
};

static driver_t ixp425_driver = {
	"ixp",
	ixp425_methods,
	sizeof(struct ixp425_softc),
};
static devclass_t ixp425_devclass;

DRIVER_MODULE(ixp, nexus, ixp425_driver, ixp425_devclass, 0, 0);
