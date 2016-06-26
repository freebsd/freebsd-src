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

#include "opt_ddb.h"

#define _ARM32_BUS_DMA_PRIVATE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <machine/armreg.h>
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

/* ixp43x et. al have +32 IRQ's */
volatile uint32_t intr_enabled2;
uint32_t intr_steer2 = 0;

struct	ixp425_softc *ixp425_softc = NULL;

struct mtx ixp425_gpio_mtx;

static int	ixp425_probe(device_t);
static void	ixp425_identify(driver_t *, device_t);
static int	ixp425_attach(device_t);

/*
 * Return a mask of the "fuse" bits that identify
 * which h/w features are present.
 * NB: assumes the expansion bus is mapped.
 */
uint32_t
ixp4xx_read_feature_bits(void)
{
	uint32_t bits = ~IXPREG(IXP425_EXP_VBASE + EXP_FCTRL_OFFSET);
	bits &= ~EXP_FCTRL_RESVD;
	if (!cpu_is_ixp46x())
		bits &= ~EXP_FCTRL_IXP46X_ONLY;
	return bits;
}

void
ixp4xx_write_feature_bits(uint32_t v)
{
	IXPREG(IXP425_EXP_VBASE + EXP_FCTRL_OFFSET) = ~v;
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

static const uint8_t int2gpio[32] __attribute__ ((aligned(32))) = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	/* INT#0 -> INT#5 */
	0x00, 0x01,				/* GPIO#0 -> GPIO#1 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	/* INT#8 -> INT#13 */
	0xff, 0xff, 0xff, 0xff, 0xff,		/* INT#14 -> INT#18 */
	0x02, 0x03, 0x04, 0x05, 0x06, 0x07,	/* GPIO#2 -> GPIO#7 */
	0x08, 0x09, 0x0a, 0x0b, 0x0c,		/* GPIO#8 -> GPIO#12 */
	0xff, 0xff				/* INT#30 -> INT#31 */
};

static __inline uint32_t
ixp425_irq2gpio_bit(int irq)
{
	return (1U << int2gpio[irq]);
}

#ifdef DDB
#include <ddb/ddb.h>

DB_SHOW_COMMAND(gpio, db_show_gpio)
{
	static const char *itype[8] = {
		[GPIO_TYPE_ACT_HIGH]	= "act-high",
		[GPIO_TYPE_ACT_LOW]	= "act-low",
		[GPIO_TYPE_EDG_RISING]	= "edge-rising",
		[GPIO_TYPE_EDG_FALLING]	= "edge-falling",
		[GPIO_TYPE_TRANSITIONAL]= "transitional",
		[5] = "type-5", [6] = "type-6", [7] = "type-7"
	};
	uint32_t gpoutr = GPIO_CONF_READ_4(ixp425_softc, IXP425_GPIO_GPOUTR);
	uint32_t gpoer = GPIO_CONF_READ_4(ixp425_softc, IXP425_GPIO_GPOER);
	uint32_t gpinr = GPIO_CONF_READ_4(ixp425_softc, IXP425_GPIO_GPINR);
	uint32_t gpit1r = GPIO_CONF_READ_4(ixp425_softc, IXP425_GPIO_GPIT1R);
	uint32_t gpit2r = GPIO_CONF_READ_4(ixp425_softc, IXP425_GPIO_GPIT2R);
	int i, j;

	db_printf("GPOUTR %08x GPINR  %08x GPOER  %08x GPISR %08x\n",
	   gpoutr, gpinr, gpoer,
	   GPIO_CONF_READ_4(ixp425_softc, IXP425_GPIO_GPISR));
	db_printf("GPIT1R %08x GPIT2R %08x GPCLKR %08x\n",
	   gpit1r, gpit2r, GPIO_CONF_READ_4(ixp425_softc, IXP425_GPIO_GPCLKR));
	for (i = 0; i < 16; i++) {
		db_printf("[%2d] out %u in %u %-3s", i,
		    (gpoutr>>i)&1, (gpinr>>i)&1, (gpoer>>i)&1 ? "in" : "out");
		for (j = 0; j < 32; j++)
			if (int2gpio[j] == i) {
				db_printf(" irq %2u %s", j, itype[
				    (((i & 8) ? gpit2r : gpit1r) >> (3*(i&7)))
					& 7]);
				break;
			}
		db_printf("\n");
	}
}
#endif

void
ixp425_set_gpio(struct ixp425_softc *sc, int pin, int type)
{
	uint32_t gpiotr = GPIO_CONF_READ_4(sc, GPIO_TYPE_REG(pin));

	IXP4XX_GPIO_LOCK();
	/* clear interrupt type */
	GPIO_CONF_WRITE_4(sc, GPIO_TYPE_REG(pin),
	    gpiotr &~ GPIO_TYPE(pin, GPIO_TYPE_MASK));
	/* clear any pending interrupt */
	GPIO_CONF_WRITE_4(sc, IXP425_GPIO_GPISR, (1<<pin));
	/* set new interrupt type */
	GPIO_CONF_WRITE_4(sc, GPIO_TYPE_REG(pin),
	    gpiotr | GPIO_TYPE(pin, type));

	/* configure gpio line as an input */
	GPIO_CONF_WRITE_4(sc, IXP425_GPIO_GPOER,
	    GPIO_CONF_READ_4(sc, IXP425_GPIO_GPOER) | (1<<pin));
	IXP4XX_GPIO_UNLOCK();
}

static __inline void
ixp425_gpio_ack(int irq)
{
	if (irq < 32 && ((1 << irq) & IXP425_INT_GPIOMASK))
		IXPREG(IXP425_GPIO_VBASE + IXP425_GPIO_GPISR) =
		    ixp425_irq2gpio_bit(irq);
}

static void
ixp425_post_filter(void *arg)
{
	uintptr_t irq = (uintptr_t) arg;
	ixp425_gpio_ack(irq);
}

void
arm_mask_irq(uintptr_t nb)
{
	int i;

	i = disable_interrupts(PSR_I);
	if (nb < 32) {
		intr_enabled &= ~(1 << nb);
		ixp425_set_intrmask();
	} else {
		intr_enabled2 &= ~(1 << (nb - 32));
		ixp435_set_intrmask();
	}
	restore_interrupts(i);
	/*XXX; If it's a GPIO interrupt, ACK it know. Can it be a problem ?*/
	ixp425_gpio_ack(nb);
}

void
arm_unmask_irq(uintptr_t nb)
{
	int i;

	i = disable_interrupts(PSR_I);
	if (nb < 32) {
		intr_enabled |= (1 << nb);
		ixp425_set_intrmask();
	} else {
		intr_enabled2 |= (1 << (nb - 32));
		ixp435_set_intrmask();
	}
	restore_interrupts(i);
}

static __inline uint32_t
ixp425_irq_read(void)
{
	return IXPREG(IXP425_INT_STATUS) & intr_enabled;
}

static __inline uint32_t
ixp435_irq_read(void)
{
	return IXPREG(IXP435_INT_STATUS2) & intr_enabled2;
}

int
arm_get_next_irq(int last)
{
	uint32_t mask;

	last += 1;		/* always advance fwd, NB: handles -1 */
	if (last < 32) {
		mask = ixp425_irq_read() >> last;
		for (; mask != 0; mask >>= 1, last++) {
			if (mask & 1)
				return last;
		}
		last = 32;
	}
	if (cpu_is_ixp43x()) {
		mask = ixp435_irq_read() >> (32-last);
		for (; mask != 0; mask >>= 1, last++) {
			if (mask & 1)
				return last;
		}
	}
	return -1;
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
	device_set_desc(dev, "Intel IXP4XX");
	return (0);
}

static int
ixp425_attach(device_t dev)
{
	struct ixp425_softc *sc;

	device_printf(dev, "%b\n", ixp4xx_read_feature_bits(), EXP_FCTRL_BITS);

	sc = device_get_softc(dev);
	sc->sc_iot = &ixp425_bs_tag;
	KASSERT(ixp425_softc == NULL, ("%s called twice?", __func__));
	ixp425_softc = sc;

	intr_enabled = 0;
	ixp425_set_intrmask();
	ixp425_set_intrsteer();
	if (cpu_is_ixp43x()) {
		intr_enabled2 = 0;
		ixp435_set_intrmask();
		ixp435_set_intrsteer();
	}
	arm_post_filter = ixp425_post_filter;

	mtx_init(&ixp425_gpio_mtx, "gpio", NULL, MTX_DEF);
	if (bus_space_map(sc->sc_iot, IXP425_GPIO_HWBASE, IXP425_GPIO_SIZE,
	    0, &sc->sc_gpio_ioh))
		panic("%s: unable to map GPIO registers", __func__);
	if (bus_space_map(sc->sc_iot, IXP425_EXP_HWBASE, IXP425_EXP_SIZE,
	    0, &sc->sc_exp_ioh))
		panic("%s: unable to map Expansion Bus registers", __func__);

	/* XXX belongs in platform init */
	if (cpu_is_ixp43x())
		cambria_exp_bus_init(sc);

	if (bus_dma_tag_create(NULL, 1, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL,  0xffffffff, 0xff, 0xffffffff, 0,
	    NULL, NULL, &sc->sc_dmat))
		panic("%s: failed to create dma tag", __func__);

	sc->sc_irq_rman.rm_type = RMAN_ARRAY;
	sc->sc_irq_rman.rm_descr = "IXP4XX IRQs";
	if (rman_init(&sc->sc_irq_rman) != 0 ||
	    rman_manage_region(&sc->sc_irq_rman, 0, cpu_is_ixp43x() ? 63 : 31) != 0)
		panic("%s: failed to set up IRQ rman", __func__);

	sc->sc_mem_rman.rm_type = RMAN_ARRAY;
	sc->sc_mem_rman.rm_descr = "IXP4XX Memory";
	if (rman_init(&sc->sc_mem_rman) != 0 ||
	    rman_manage_region(&sc->sc_mem_rman, 0, ~0) != 0)
		panic("%s: failed to set up memory rman", __func__);

	BUS_ADD_CHILD(dev, 0, "pcib", 0);
	BUS_ADD_CHILD(dev, 0, "ixpclk", 0);
	BUS_ADD_CHILD(dev, 0, "ixpiic", 0);
	/* XXX move to hints? */
	BUS_ADD_CHILD(dev, 0, "ixpwdog", 0);

	/* attach wired devices via hints */
	bus_enumerate_hinted_children(dev);

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
ixp425_add_child(device_t dev, u_int order, const char *name, int unit)
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
ixp425_read_ivar(device_t bus, device_t child, int which, uintptr_t *result)
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

/*
 * NB: This table handles P->V translations for regions setup with
 * static mappings in initarm.  This is used solely for calls to
 * bus_alloc_resource_any; anything done with bus_space_map is
 * handled elsewhere and does not require an entry here.
 *
 * XXX this table is also used by uart_cpu_getdev via getvbase
 *    (hence the public api)
 */
struct hwvtrans {
	uint32_t	hwbase;
	uint32_t	size;
	uint32_t	vbase;
	int		isa4x;	/* XXX needs special bus space tag */
	int		isslow;	/* XXX needs special bus space tag */
};

static const struct hwvtrans *
gethwvtrans(uint32_t hwbase, uint32_t size)
{
	static const struct hwvtrans hwvtrans[] = {
	    /* NB: needed only for uart_cpu_getdev */
	    { .hwbase	= IXP425_UART0_HWBASE,
	      .size 	= IXP425_REG_SIZE,
	      .vbase	= IXP425_UART0_VBASE,
	      .isa4x	= 1 },
	    { .hwbase	= IXP425_UART1_HWBASE,
	      .size 	= IXP425_REG_SIZE,
	      .vbase	= IXP425_UART1_VBASE,
	      .isa4x	= 1 },
	    { .hwbase	= IXP425_PCI_HWBASE,
	      .size 	= IXP425_PCI_SIZE,
	      .vbase	= IXP425_PCI_VBASE },
	    { .hwbase	= IXP425_PCI_MEM_HWBASE,
	      .size 	= IXP425_PCI_MEM_SIZE,
	      .vbase	= IXP425_PCI_MEM_VBASE },
	    { .hwbase	= IXP425_EXP_BUS_CS0_HWBASE,
	      .size 	= IXP425_EXP_BUS_CS0_SIZE,
	      .vbase	= IXP425_EXP_BUS_CS0_VBASE },
	    /* NB: needed for ixp435 ehci controllers */
	    { .hwbase	= IXP435_USB1_HWBASE,
	      .size 	= IXP435_USB1_SIZE,
	      .vbase	= IXP435_USB1_VBASE },
	    { .hwbase	= IXP435_USB2_HWBASE,
	      .size 	= IXP435_USB2_SIZE,
	      .vbase	= IXP435_USB2_VBASE },
	    { .hwbase	= CAMBRIA_GPS_HWBASE,
	      .size 	= CAMBRIA_GPS_SIZE,
	      .vbase	= CAMBRIA_GPS_VBASE,
	      .isslow	= 1 },
	    { .hwbase	= CAMBRIA_RS485_HWBASE,
	      .size 	= CAMBRIA_RS485_SIZE,
	      .vbase	= CAMBRIA_RS485_VBASE,
	      .isslow	= 1 },
	};
	int i;

	for (i = 0; i < nitems(hwvtrans); i++) {
		if (hwbase >= hwvtrans[i].hwbase &&
		    hwbase + size <= hwvtrans[i].hwbase + hwvtrans[i].size)
			return &hwvtrans[i];
	}
	return NULL;
}

/* XXX for uart_cpu_getdev */
int
getvbase(uint32_t hwbase, uint32_t size, uint32_t *vbase)
{
	const struct hwvtrans *hw;

	hw = gethwvtrans(hwbase, size);
	if (hw == NULL)
		return (ENOENT);
	*vbase = hwbase - hw->hwbase + hw->vbase;
	return (0);
}

static struct resource *
ixp425_alloc_resource(device_t dev, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct ixp425_softc *sc = device_get_softc(dev);
	const struct hwvtrans *vtrans;
	struct resource *rv;
	uint32_t addr;
	int needactivate = flags & RF_ACTIVE;
	int irq;

	flags &= ~RF_ACTIVE;
	switch (type) {
	case SYS_RES_IRQ:
		/* override per hints */
		if (BUS_READ_IVAR(dev, child, IXP425_IVAR_IRQ, &irq) == 0)
			start = end = irq;
		rv = rman_reserve_resource(&sc->sc_irq_rman, start, end, count,
		    flags, child);
		if (rv != NULL)
			rman_set_rid(rv, *rid);
		break;

	case SYS_RES_MEMORY:
		/* override per hints */
		if (BUS_READ_IVAR(dev, child, IXP425_IVAR_ADDR, &addr) == 0) {
			start = addr;
			/* XXX use nominal window to check for mapping */
			vtrans = gethwvtrans(start, 0x1000);
			if (vtrans != NULL) {
				/*
				 * Assign the entire mapped region; this may
				 * not be correct but without more info from
				 * the caller we cannot tell.
				 */
				end = start + vtrans->size -
				    (start - vtrans->hwbase);
				if (bootverbose)
					device_printf(child,
					    "%s: assign 0x%jx:0x%jx%s\n",
					    __func__, start, end - start,
					    vtrans->isa4x ? " A4X" :
					    vtrans->isslow ? " SLOW" : "");
			}
		} else
			vtrans = gethwvtrans(start, end - start);
		if (vtrans == NULL) {
			/* likely means above table needs to be updated */
			device_printf(child, "%s: no mapping for 0x%jx:0x%jx\n",
			    __func__, start, end - start);
			return NULL;
		}
		rv = rman_reserve_resource(&sc->sc_mem_rman, start, end,
		    end - start, flags, child);
		if (rv == NULL) {
			device_printf(child, "%s: cannot reserve 0x%jx:0x%jx\n",
			    __func__, start, end - start);
			return NULL;
		}
		rman_set_rid(rv, *rid);
		break;
	default:
		rv = NULL;
		break;
	}
	if (rv != NULL && needactivate) {
		if (bus_activate_resource(child, type, *rid, rv)) {
			rman_release_resource(rv);
			return (NULL);
		}
	}
	return (rv);
}

static int
ixp425_release_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{
	/* NB: no private resources, just release */
	return rman_release_resource(r);
}

static int
ixp425_activate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	struct ixp425_softc *sc = device_get_softc(dev);
	const struct hwvtrans *vtrans;

	if (type == SYS_RES_MEMORY) {
		vtrans = gethwvtrans(rman_get_start(r), rman_get_size(r));
		if (vtrans == NULL) {		/* NB: should not happen */
			device_printf(child, "%s: no mapping for 0x%jx:0x%jx\n",
			    __func__, rman_get_start(r), rman_get_size(r));
			return (ENOENT);
		}
		if (vtrans->isa4x)
			rman_set_bustag(r, &ixp425_a4x_bs_tag);
		else if (vtrans->isslow)
			rman_set_bustag(r, &cambria_exp_bs_tag);
		else
			rman_set_bustag(r, sc->sc_iot);
		rman_set_bushandle(r, vtrans->vbase);
	}
	return (rman_activate_resource(r));
}

static int
ixp425_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{
	/* NB: no private resources, just deactive */
	return (rman_deactivate_resource(r));
}

static __inline void
get_masks(struct resource *res, uint32_t *mask, uint32_t *mask2)
{
	int i;

	*mask = 0;
	for (i = rman_get_start(res); i < 32 && i <= rman_get_end(res); i++)
		*mask |= 1 << i;
	*mask2 = 0;
	for (; i <= rman_get_end(res); i++)
		*mask2 |= 1 << (i - 32);
}

static __inline void
update_masks(uint32_t mask, uint32_t mask2)
{

	intr_enabled = mask;
	ixp425_set_intrmask();
	if (cpu_is_ixp43x()) {
		intr_enabled2 = mask2;
		ixp435_set_intrmask();
	}
}

static int
ixp425_setup_intr(device_t dev, device_t child,
    struct resource *res, int flags, driver_filter_t *filt,
    driver_intr_t *intr, void *arg, void **cookiep)
{
	uint32_t mask, mask2;
	int error;

	error = BUS_SETUP_INTR(device_get_parent(dev), child, res, flags,
	    filt, intr, arg, cookiep);
	if (error)
		return (error);

	get_masks(res, &mask, &mask2);
	update_masks(intr_enabled | mask, intr_enabled2 | mask2);

	return (0);
}

static int
ixp425_teardown_intr(device_t dev, device_t child, struct resource *res,
    void *cookie)
{
	uint32_t mask, mask2;

	get_masks(res, &mask, &mask2);
	update_masks(intr_enabled &~ mask, intr_enabled2 &~ mask2);

	return (BUS_TEARDOWN_INTR(device_get_parent(dev), child, res, cookie));
}

static device_method_t ixp425_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			ixp425_probe),
	DEVMETHOD(device_attach,		ixp425_attach),
	DEVMETHOD(device_identify,		ixp425_identify),

	/* Bus interface */
	DEVMETHOD(bus_add_child,		ixp425_add_child),
	DEVMETHOD(bus_hinted_child,		ixp425_hinted_child),
	DEVMETHOD(bus_read_ivar,		ixp425_read_ivar),

	DEVMETHOD(bus_alloc_resource,		ixp425_alloc_resource),
	DEVMETHOD(bus_release_resource,		ixp425_release_resource),
	DEVMETHOD(bus_activate_resource,	ixp425_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	ixp425_deactivate_resource),
	DEVMETHOD(bus_setup_intr,		ixp425_setup_intr),
	DEVMETHOD(bus_teardown_intr,		ixp425_teardown_intr),

	{0, 0},
};

static driver_t ixp425_driver = {
	"ixp",
	ixp425_methods,
	sizeof(struct ixp425_softc),
};
static devclass_t ixp425_devclass;

DRIVER_MODULE(ixp, nexus, ixp425_driver, ixp425_devclass, 0, 0);
