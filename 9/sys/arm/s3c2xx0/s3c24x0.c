/*	$NetBSD: s3c2410.c,v 1.4 2003/08/27 03:46:05 bsh Exp $ */

/*
 * Copyright (c) 2003  Genetec corporation.  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Genetec corporation may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORP. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORP.
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
#include <sys/kernel.h>
#include <sys/reboot.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/cpu.h>
#include <machine/bus.h>

#include <machine/cpufunc.h>
#include <machine/intr.h>
#include <arm/s3c2xx0/s3c2410reg.h>
#include <arm/s3c2xx0/s3c2440reg.h>
#include <arm/s3c2xx0/s3c24x0var.h>
#include <sys/rman.h>

#define S3C2XX0_XTAL_CLK 12000000

#define IPL_LEVELS 13
u_int irqmasks[IPL_LEVELS];

static struct {
	uint32_t	idcode;
	const char	*name;
	s3c2xx0_cpu	cpu;
} s3c2x0_cpu_id[] = {
	{ CHIPID_S3C2410A, "S3C2410A", CPU_S3C2410 },
	{ CHIPID_S3C2440A, "S3C2440A", CPU_S3C2440 },
	{ CHIPID_S3C2442B, "S3C2442B", CPU_S3C2440 },

	{ 0, NULL }
};

static struct {
	const char *name;
	int prio;
	int unit;
	struct {
		int type;
		u_long start;
		u_long count;
	} res[2];
} s3c24x0_children[] = {
	{ "rtc", 0, -1, {
		{ SYS_RES_IOPORT, S3C24X0_RTC_PA_BASE, S3C24X0_RTC_SIZE },
		{ 0 },
	} },
	{ "timer", 0, -1, { { 0 }, } },
	{ "uart", 1, 0, {
		{ SYS_RES_IRQ, S3C24X0_INT_UART0, 1 },
		{ SYS_RES_IOPORT, S3C24X0_UART_PA_BASE(0),
		  S3C24X0_UART_BASE(1) - S3C24X0_UART_BASE(0) },
	} },
	{ "uart", 1, 1, {
		{ SYS_RES_IRQ, S3C24X0_INT_UART1, 1 },
		{ SYS_RES_IOPORT, S3C24X0_UART_PA_BASE(1),
		  S3C24X0_UART_BASE(2) - S3C24X0_UART_BASE(1) },
	} },
	{ "uart", 1, 2, {
		{ SYS_RES_IRQ, S3C24X0_INT_UART2, 1 },
		{ SYS_RES_IOPORT, S3C24X0_UART_PA_BASE(2),
		  S3C24X0_UART_BASE(3) - S3C24X0_UART_BASE(2) },
	} },
	{ "ohci", 0, -1, {
		{ SYS_RES_IRQ, S3C24X0_INT_USBH, 0 },
		{ SYS_RES_IOPORT, S3C24X0_USBHC_PA_BASE, S3C24X0_USBHC_SIZE },
	} },
	{ NULL },
};


/* prototypes */
static device_t s3c24x0_add_child(device_t, int, const char *, int);

static int	s3c24x0_probe(device_t);
static int	s3c24x0_attach(device_t);
static void	s3c24x0_identify(driver_t *, device_t);
static int	s3c24x0_setup_intr(device_t, device_t, struct resource *, int,
        driver_filter_t *, driver_intr_t *, void *, void **);
static int	s3c24x0_teardown_intr(device_t, device_t, struct resource *,
	void *);
static int	s3c24x0_config_intr(device_t, int, enum intr_trigger,
	enum intr_polarity);
static struct resource *s3c24x0_alloc_resource(device_t, device_t, int, int *,
        u_long, u_long, u_long, u_int);
static int s3c24x0_activate_resource(device_t, device_t, int, int,
        struct resource *);
static int s3c24x0_release_resource(device_t, device_t, int, int,
        struct resource *);
static struct resource_list *s3c24x0_get_resource_list(device_t, device_t);

static void s3c24x0_identify_cpu(device_t);

static device_method_t s3c24x0_methods[] = {
	DEVMETHOD(device_probe, s3c24x0_probe),
	DEVMETHOD(device_attach, s3c24x0_attach),
	DEVMETHOD(device_identify, s3c24x0_identify),
	DEVMETHOD(bus_setup_intr, s3c24x0_setup_intr),
	DEVMETHOD(bus_teardown_intr, s3c24x0_teardown_intr),
	DEVMETHOD(bus_config_intr, s3c24x0_config_intr),
	DEVMETHOD(bus_alloc_resource, s3c24x0_alloc_resource),
	DEVMETHOD(bus_activate_resource, s3c24x0_activate_resource),
	DEVMETHOD(bus_release_resource,	s3c24x0_release_resource),
	DEVMETHOD(bus_get_resource_list,s3c24x0_get_resource_list),
	DEVMETHOD(bus_set_resource,	bus_generic_rl_set_resource),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),
	{0, 0},
};

static driver_t s3c24x0_driver = {
	"s3c24x0",
	s3c24x0_methods,
	sizeof(struct s3c24x0_softc),
};
static devclass_t s3c24x0_devclass;

DRIVER_MODULE(s3c24x0, nexus, s3c24x0_driver, s3c24x0_devclass, 0, 0);

struct s3c2xx0_softc *s3c2xx0_softc = NULL;

static device_t
s3c24x0_add_child(device_t bus, int prio, const char *name, int unit)
{
	device_t child;
	struct s3c2xx0_ivar *ivar;

	child = device_add_child_ordered(bus, prio, name, unit);
	if (child == NULL)
		return (NULL);

	ivar = malloc(sizeof(*ivar), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (ivar == NULL) {
		device_delete_child(bus, child);
		printf("Can't add alloc ivar\n");
		return (NULL);
	}
	device_set_ivars(child, ivar);
	resource_list_init(&ivar->resources);

	return (child);
}

static void
s3c24x0_enable_ext_intr(unsigned int irq)
{
	uint32_t reg, value;
	int offset;

	if (irq <= 7) {
		reg = GPIO_PFCON;
		offset = irq * 2;
	} else if (irq <= 23) {
		reg = GPIO_PGCON;
		offset = (irq - 8) * 2;
	} else
		return;

	/* Make the pin an interrupt source */
	value = bus_space_read_4(s3c2xx0_softc->sc_iot,
	    s3c2xx0_softc->sc_gpio_ioh, reg);
	value &= ~(3 << offset);
	value |= 2 << offset;
	bus_space_write_4(s3c2xx0_softc->sc_iot, s3c2xx0_softc->sc_gpio_ioh,
	    reg, value);
}

static int
s3c24x0_setup_intr(device_t dev, device_t child,
        struct resource *ires,  int flags, driver_filter_t *filt,
	driver_intr_t *intr, void *arg, void **cookiep)
{
	int error, irq;

	error = BUS_SETUP_INTR(device_get_parent(dev), child, ires, flags, filt,
	    intr, arg, cookiep);
	if (error != 0)
		return (error);

	for (irq = rman_get_start(ires); irq <= rman_get_end(ires); irq++) {
		if (irq >= S3C24X0_EXTIRQ_MIN && irq <= S3C24X0_EXTIRQ_MAX) {
			/* Enable the external interrupt pin */
			s3c24x0_enable_ext_intr(irq - S3C24X0_EXTIRQ_MIN);
		}
		arm_unmask_irq(irq);
	}
	return (0);
}

static int
s3c24x0_teardown_intr(device_t dev, device_t child, struct resource *res,
	void *cookie)
{
	return (BUS_TEARDOWN_INTR(device_get_parent(dev), child, res, cookie));
}

static int
s3c24x0_config_intr(device_t dev, int irq, enum intr_trigger trig,
	enum intr_polarity pol)
{
	uint32_t mask, reg, value;
	int offset;

	/* Only external interrupts can be configured */
	if (irq < S3C24X0_EXTIRQ_MIN || irq > S3C24X0_EXTIRQ_MAX)
		return (EINVAL);

	/* There is no standard trigger or polarity for the bus */
	if (trig == INTR_TRIGGER_CONFORM || pol == INTR_POLARITY_CONFORM)
		return (EINVAL);

	irq -= S3C24X0_EXTIRQ_MIN;

	/* Get the bits to set */
	mask = 0;
	if (pol == INTR_POLARITY_LOW) {
		mask = 2;
	} else if (pol == INTR_POLARITY_HIGH) {
		mask = 4;
	}
	if (trig == INTR_TRIGGER_LEVEL) {
		mask >>= 2;
	}

	/* Get the register to set */
	if (irq <= 7) {
		reg = GPIO_EXTINT(0);
		offset = irq * 4;
	} else if (irq <= 15) {
		reg = GPIO_EXTINT(1);
		offset = (irq - 8) * 4;
	} else if (irq <= 23) {
		reg = GPIO_EXTINT(2);
		offset = (irq - 16) * 4;
	} else {
		return (EINVAL);
	}

	/* Set the new signaling method */
	value = bus_space_read_4(s3c2xx0_softc->sc_iot,
	    s3c2xx0_softc->sc_gpio_ioh, reg);
	value &= ~(7 << offset);
	value |= mask << offset;
	bus_space_write_4(s3c2xx0_softc->sc_iot,
	    s3c2xx0_softc->sc_gpio_ioh, reg, value);

	return (0);
} 

static struct resource *
s3c24x0_alloc_resource(device_t bus, device_t child, int type, int *rid,
        u_long start, u_long end, u_long count, u_int flags)
{
	struct resource_list_entry *rle;
	struct s3c2xx0_ivar *ivar = device_get_ivars(child);
	struct resource_list *rl = &ivar->resources;
	struct resource *res = NULL;

	if (device_get_parent(child) != bus)
		return (BUS_ALLOC_RESOURCE(device_get_parent(bus), child,
		    type, rid, start, end, count, flags));

	rle = resource_list_find(rl, type, *rid);
	if (rle != NULL) {
		/* There is a resource list. Use it */
		if (rle->res)
			panic("Resource rid %d type %d already in use", *rid,
			    type);
		if (start == 0UL && end == ~0UL) {
			start = rle->start;
			count = ulmax(count, rle->count);
			end = ulmax(rle->end, start + count - 1);
		}
		/*
		 * When allocating an irq with children irq's really
		 * allocate the children as it is those we are interested
		 * in receiving, not the parent.
		 */
		if (type == SYS_RES_IRQ && start == end) {
			switch (start) {
			case S3C24X0_INT_ADCTC:
				start = S3C24X0_INT_TC;
				end = S3C24X0_INT_ADC;
				break;
#ifdef S3C2440_INT_CAM
			case S3C2440_INT_CAM:
				start = S3C2440_INT_CAM_C;
				end = S3C2440_INT_CAM_P;
				break;
#endif
			default:
				break;
			}
			count = end - start + 1;
		}
	}

	switch (type) {
	case SYS_RES_IRQ:
		res = rman_reserve_resource(
		    &s3c2xx0_softc->s3c2xx0_irq_rman, start, end,
		    count, flags, child);
		break;

	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
		res = rman_reserve_resource(
		    &s3c2xx0_softc->s3c2xx0_mem_rman,
		    start, end, count, flags, child);
		if (res == NULL)
			panic("Unable to map address space %#lX-%#lX", start,
			    end);

		rman_set_bustag(res, &s3c2xx0_bs_tag);
		rman_set_bushandle(res, start);
		if (flags & RF_ACTIVE) {
			if (bus_activate_resource(child, type, *rid, res)) {
				rman_release_resource(res);
				return (NULL);
			}
		} 
		break;
	}

	if (res != NULL) {
		rman_set_rid(res, *rid);
		if (rle != NULL) {
			rle->res = res;
			rle->start = rman_get_start(res);
			rle->end = rman_get_end(res);
			rle->count = count;
		}
	}

	return (res);
}

static int
s3c24x0_activate_resource(device_t bus, device_t child, int type, int rid,
        struct resource *r)
{
	bus_space_handle_t p;
	int error;

	if (type == SYS_RES_MEMORY || type == SYS_RES_IOPORT) {
		error = bus_space_map(rman_get_bustag(r),
		    rman_get_bushandle(r), rman_get_size(r), 0, &p);
		if (error)
			return (error);
		rman_set_bushandle(r, p);
	}
	return (rman_activate_resource(r));
}

static int
s3c24x0_release_resource(device_t bus, device_t child, int type, int rid,
        struct resource *r)
{
	struct s3c2xx0_ivar *ivar = device_get_ivars(child);
	struct resource_list *rl = &ivar->resources;
	struct resource_list_entry *rle;

	if (rl == NULL)
		return (EINVAL);

	rle = resource_list_find(rl, type, rid);
	if (rle == NULL)
		return (EINVAL);

	rman_release_resource(r);
	rle->res = NULL;

	return 0;
}

static struct resource_list *
s3c24x0_get_resource_list(device_t dev, device_t child)
{
	struct s3c2xx0_ivar *ivar;

	ivar = device_get_ivars(child);
	return (&(ivar->resources));
}

void
s3c24x0_identify(driver_t *driver, device_t parent)
{
	
	BUS_ADD_CHILD(parent, 0, "s3c24x0", 0);
}

int
s3c24x0_probe(device_t dev)
{
	return 0;
}

int
s3c24x0_attach(device_t dev)
{
	struct s3c24x0_softc *sc = device_get_softc(dev);
	bus_space_tag_t iot;
	device_t child;
	unsigned int i, j;
	u_long irqmax;

	s3c2xx0_softc = &(sc->sc_sx);
	sc->sc_sx.sc_iot = iot = &s3c2xx0_bs_tag;
	s3c2xx0_softc->s3c2xx0_irq_rman.rm_type = RMAN_ARRAY;
	s3c2xx0_softc->s3c2xx0_irq_rman.rm_descr = "S3C24X0 IRQs";
	s3c2xx0_softc->s3c2xx0_mem_rman.rm_type = RMAN_ARRAY;
	s3c2xx0_softc->s3c2xx0_mem_rman.rm_descr = "S3C24X0 Device Registers";
	/* Manage the registor memory space */
	if ((rman_init(&s3c2xx0_softc->s3c2xx0_mem_rman) != 0) ||
	    (rman_manage_region(&s3c2xx0_softc->s3c2xx0_mem_rman,
	      S3C24X0_DEV_VA_OFFSET,
	      S3C24X0_DEV_VA_OFFSET + S3C24X0_DEV_VA_SIZE) != 0) ||
	    (rman_manage_region(&s3c2xx0_softc->s3c2xx0_mem_rman,
	      S3C24X0_DEV_START, S3C24X0_DEV_STOP) != 0))
		panic("s3c24x0_attach: failed to set up register rman");

	/* These are needed for things without a proper device to attach to */
	sc->sc_sx.sc_intctl_ioh = S3C24X0_INTCTL_BASE;
	sc->sc_sx.sc_gpio_ioh = S3C24X0_GPIO_BASE;
	sc->sc_sx.sc_clkman_ioh = S3C24X0_CLKMAN_BASE;
	sc->sc_sx.sc_wdt_ioh = S3C24X0_WDT_BASE;
	sc->sc_timer_ioh = S3C24X0_TIMER_BASE;

	/*
	 * Identify the CPU
	 */
	s3c24x0_identify_cpu(dev);

	/*
	 * Manage the interrupt space.
	 * We need to put this after s3c24x0_identify_cpu as the avaliable
	 * interrupts change depending on which CPU we have.
	 */
	if (sc->sc_sx.sc_cpu == CPU_S3C2410)
		irqmax = S3C2410_SUBIRQ_MAX;
	else
		irqmax = S3C2440_SUBIRQ_MAX;
	if (rman_init(&s3c2xx0_softc->s3c2xx0_irq_rman) != 0 ||
	    rman_manage_region(&s3c2xx0_softc->s3c2xx0_irq_rman, 0,
	    irqmax) != 0 ||
	    rman_manage_region(&s3c2xx0_softc->s3c2xx0_irq_rman,
	    S3C24X0_EXTIRQ_MIN, S3C24X0_EXTIRQ_MAX))
		panic("s3c24x0_attach: failed to set up IRQ rman");

	/* calculate current clock frequency */
	s3c24x0_clock_freq(&sc->sc_sx);
	device_printf(dev, "fclk %d MHz hclk %d MHz pclk %d MHz\n",
	       sc->sc_sx.sc_fclk / 1000000, sc->sc_sx.sc_hclk / 1000000,
	       sc->sc_sx.sc_pclk / 1000000);

	/*
	 * Attach children devices
	 */

	for (i = 0; s3c24x0_children[i].name != NULL; i++) {
		child = s3c24x0_add_child(dev, s3c24x0_children[i].prio,
		    s3c24x0_children[i].name, s3c24x0_children[i].unit);
		for (j = 0; j < sizeof(s3c24x0_children[i].res) /
		     sizeof(s3c24x0_children[i].res[0]) &&
		     s3c24x0_children[i].res[j].type != 0; j++) {
			bus_set_resource(child,
			    s3c24x0_children[i].res[j].type, 0,
			    s3c24x0_children[i].res[j].start,
			    s3c24x0_children[i].res[j].count);
		}
	}

	bus_generic_probe(dev);
	bus_generic_attach(dev);

	return (0);
}

static void
s3c24x0_identify_cpu(device_t dev)
{
	struct s3c24x0_softc *sc = device_get_softc(dev);
	uint32_t idcode;
	int i;

	idcode = bus_space_read_4(sc->sc_sx.sc_iot, sc->sc_sx.sc_gpio_ioh,
	    GPIO_GSTATUS1);

	for (i = 0; s3c2x0_cpu_id[i].name != NULL; i++) {
		if (s3c2x0_cpu_id[i].idcode == idcode)
			break;
	}
	if (s3c2x0_cpu_id[i].name == NULL)
		panic("Unknown CPU detected ((Chip ID: %#X)", idcode);
	device_printf(dev, "Found %s CPU (Chip ID: %#X)\n",
	    s3c2x0_cpu_id[i].name, idcode);
	sc->sc_sx.sc_cpu = s3c2x0_cpu_id[i].cpu;
}

/*
 * fill sc_pclk, sc_hclk, sc_fclk from values of clock controller register.
 *
 * s3c24{1,4}0_clock_freq2() is meant to be called from kernel startup routines.
 * s3c24x0_clock_freq() is for after kernel initialization is done.
 *
 * Because they can be called before bus_space is available we need to use
 * volatile pointers rather than bus_space_read.
 */
void
s3c2410_clock_freq2(vm_offset_t clkman_base, int *fclk, int *hclk, int *pclk)
{
	uint32_t pllcon, divn;
	unsigned int mdiv, pdiv, sdiv;
	unsigned int f, h, p;

	pllcon = *(volatile uint32_t *)(clkman_base + CLKMAN_MPLLCON);
	divn = *(volatile uint32_t *)(clkman_base + CLKMAN_CLKDIVN);

	mdiv = (pllcon & PLLCON_MDIV_MASK) >> PLLCON_MDIV_SHIFT;
	pdiv = (pllcon & PLLCON_PDIV_MASK) >> PLLCON_PDIV_SHIFT;
	sdiv = (pllcon & PLLCON_SDIV_MASK) >> PLLCON_SDIV_SHIFT;

	f = ((mdiv + 8) * S3C2XX0_XTAL_CLK) / ((pdiv + 2) * (1 << sdiv));
	h = f;
	if (divn & S3C2410_CLKDIVN_HDIVN)
		h /= 2;
	p = h;
	if (divn & CLKDIVN_PDIVN)
		p /= 2;

	if (fclk) *fclk = f;
	if (hclk) *hclk = h;
	if (pclk) *pclk = p;
}

void
s3c2440_clock_freq2(vm_offset_t clkman_base, int *fclk, int *hclk, int *pclk)
{
	uint32_t pllcon, divn, camdivn;
	unsigned int mdiv, pdiv, sdiv;
	unsigned int f, h, p;

	pllcon = *(volatile uint32_t *)(clkman_base + CLKMAN_MPLLCON);
	divn = *(volatile uint32_t *)(clkman_base + CLKMAN_CLKDIVN);
	camdivn = *(volatile uint32_t *)(clkman_base + S3C2440_CLKMAN_CAMDIVN);

	mdiv = (pllcon & PLLCON_MDIV_MASK) >> PLLCON_MDIV_SHIFT;
	pdiv = (pllcon & PLLCON_PDIV_MASK) >> PLLCON_PDIV_SHIFT;
	sdiv = (pllcon & PLLCON_SDIV_MASK) >> PLLCON_SDIV_SHIFT;

	f = (2 * (mdiv + 8) * S3C2XX0_XTAL_CLK) / ((pdiv + 2) * (1 << sdiv));
	h = f;
	switch((divn >> 1) & 3) {
	case 0:
		break;
	case 1:
		h /= 2;
		break;
	case 2:
		if ((camdivn & S3C2440_CAMDIVN_HCLK4_HALF) ==
		    S3C2440_CAMDIVN_HCLK4_HALF)
			h /= 8;
		else
			h /= 4;
		break;
	case 3:
		if ((camdivn & S3C2440_CAMDIVN_HCLK3_HALF) ==
		    S3C2440_CAMDIVN_HCLK3_HALF)
			h /= 6;
		else
			h /= 3;
		break;
	}
	p = h;
	if (divn & CLKDIVN_PDIVN)
		p /= 2;

	if (fclk) *fclk = f;
	if (hclk) *hclk = h;
	if (pclk) *pclk = p;
}

void
s3c24x0_clock_freq(struct s3c2xx0_softc *sc)
{
	vm_offset_t va;
	
	va = sc->sc_clkman_ioh;
	switch(sc->sc_cpu) {
	case CPU_S3C2410:
		s3c2410_clock_freq2(va, &sc->sc_fclk, &sc->sc_hclk,
		    &sc->sc_pclk);
		break;
	case CPU_S3C2440:
		s3c2440_clock_freq2(va, &sc->sc_fclk, &sc->sc_hclk,
		    &sc->sc_pclk);
		break;
	}
}

void
cpu_reset(void)
{
	(void) disable_interrupts(I32_bit|F32_bit);

	bus_space_write_4(&s3c2xx0_bs_tag, s3c2xx0_softc->sc_wdt_ioh, WDT_WTCON,
	    WTCON_ENABLE | WTCON_CLKSEL_16 | WTCON_ENRST);
	for(;;);
}

void
s3c24x0_sleep(int mode __unused)
{
	int reg;

	reg = bus_space_read_4(&s3c2xx0_bs_tag, s3c2xx0_softc->sc_clkman_ioh,
	    CLKMAN_CLKCON);
	bus_space_write_4(&s3c2xx0_bs_tag, s3c2xx0_softc->sc_clkman_ioh,
	    CLKMAN_CLKCON, reg | CLKCON_IDLE);
}


int
arm_get_next_irq(int last __unused)
{
	uint32_t intpnd;
	int irq, subirq;

	if ((irq = bus_space_read_4(&s3c2xx0_bs_tag,
	    s3c2xx0_softc->sc_intctl_ioh, INTCTL_INTOFFSET)) != 0) {

		/* Clear the pending bit */
		intpnd = bus_space_read_4(&s3c2xx0_bs_tag,
		    s3c2xx0_softc->sc_intctl_ioh, INTCTL_INTPND);
		bus_space_write_4(&s3c2xx0_bs_tag, s3c2xx0_softc->sc_intctl_ioh,
		    INTCTL_SRCPND, intpnd);
		bus_space_write_4(&s3c2xx0_bs_tag, s3c2xx0_softc->sc_intctl_ioh,
		    INTCTL_INTPND, intpnd);

		switch (irq) {
		case S3C24X0_INT_ADCTC:
		case S3C24X0_INT_UART0:
		case S3C24X0_INT_UART1:
		case S3C24X0_INT_UART2:
			/* Find the sub IRQ */
			subirq = 0x7ff;
			subirq &= bus_space_read_4(&s3c2xx0_bs_tag,
			    s3c2xx0_softc->sc_intctl_ioh, INTCTL_SUBSRCPND);
			subirq &= ~(bus_space_read_4(&s3c2xx0_bs_tag,
			    s3c2xx0_softc->sc_intctl_ioh, INTCTL_INTSUBMSK));
			if (subirq == 0)
				return (irq);

			subirq = ffs(subirq) - 1;

			/* Clear the sub irq pending bit */
			bus_space_write_4(&s3c2xx0_bs_tag,
			    s3c2xx0_softc->sc_intctl_ioh, INTCTL_SUBSRCPND,
			    (1 << subirq));

			/*
			 * Return the parent IRQ for UART
			 * as it is all we ever need
			 */
			if (subirq <= 8)
				return (irq);

			return (S3C24X0_SUBIRQ_MIN + subirq);

		case S3C24X0_INT_0:
		case S3C24X0_INT_1:
		case S3C24X0_INT_2:
		case S3C24X0_INT_3:
			/* There is a 1:1 mapping to the IRQ we are handling */
			return S3C24X0_INT_EXT(irq);

		case S3C24X0_INT_4_7:
		case S3C24X0_INT_8_23:
			/* Find the external interrupt being called */
			subirq = 0x7fffff;
			subirq &= bus_space_read_4(&s3c2xx0_bs_tag,
			    s3c2xx0_softc->sc_gpio_ioh, GPIO_EINTPEND);
			subirq &= ~bus_space_read_4(&s3c2xx0_bs_tag,
			    s3c2xx0_softc->sc_gpio_ioh, GPIO_EINTMASK);
			if (subirq == 0)
				return (irq);

			subirq = ffs(subirq) - 1;

			/* Clear the external irq pending bit */
			bus_space_write_4(&s3c2xx0_bs_tag,
			    s3c2xx0_softc->sc_gpio_ioh, GPIO_EINTPEND,
			    (1 << subirq));

			return S3C24X0_INT_EXT(subirq);
		}

		return (irq);
	}
	return (-1);
}

void
arm_mask_irq(uintptr_t irq)
{
	u_int32_t mask;

	if (irq >= S3C24X0_INT_EXT(0) && irq <= S3C24X0_INT_EXT(3)) {
		/* External interrupt 0..3 are directly mapped to irq 0..3 */
		irq -= S3C24X0_EXTIRQ_MIN;
	}
	if (irq < S3C24X0_SUBIRQ_MIN) {
		mask = bus_space_read_4(&s3c2xx0_bs_tag,
		    s3c2xx0_softc->sc_intctl_ioh, INTCTL_INTMSK);
		mask |= (1 << irq);
		bus_space_write_4(&s3c2xx0_bs_tag, 
		    s3c2xx0_softc->sc_intctl_ioh, INTCTL_INTMSK, mask);
	} else if (irq < S3C24X0_EXTIRQ_MIN) {
		mask = bus_space_read_4(&s3c2xx0_bs_tag,
		    s3c2xx0_softc->sc_intctl_ioh, INTCTL_INTSUBMSK);
		mask |= (1 << (irq - S3C24X0_SUBIRQ_MIN));
		bus_space_write_4(&s3c2xx0_bs_tag, 
		    s3c2xx0_softc->sc_intctl_ioh, INTCTL_INTSUBMSK, mask);
	} else {
		mask = bus_space_read_4(&s3c2xx0_bs_tag,
		    s3c2xx0_softc->sc_gpio_ioh, GPIO_EINTMASK);
		mask |= (1 << (irq - S3C24X0_EXTIRQ_MIN));
		bus_space_write_4(&s3c2xx0_bs_tag, 
		    s3c2xx0_softc->sc_intctl_ioh, GPIO_EINTMASK, mask);
	}
}

void
arm_unmask_irq(uintptr_t irq)
{
	u_int32_t mask;

	if (irq >= S3C24X0_INT_EXT(0) && irq <= S3C24X0_INT_EXT(3)) {
		/* External interrupt 0..3 are directly mapped to irq 0..3 */
		irq -= S3C24X0_EXTIRQ_MIN;
	}
	if (irq < S3C24X0_SUBIRQ_MIN) {
		mask = bus_space_read_4(&s3c2xx0_bs_tag,
		    s3c2xx0_softc->sc_intctl_ioh, INTCTL_INTMSK);
		mask &= ~(1 << irq);
		bus_space_write_4(&s3c2xx0_bs_tag,
		    s3c2xx0_softc->sc_intctl_ioh, INTCTL_INTMSK, mask);
	} else if (irq < S3C24X0_EXTIRQ_MIN) {
		mask = bus_space_read_4(&s3c2xx0_bs_tag,
		    s3c2xx0_softc->sc_intctl_ioh, INTCTL_INTSUBMSK);
		mask &= ~(1 << (irq - S3C24X0_SUBIRQ_MIN));
		bus_space_write_4(&s3c2xx0_bs_tag, 
		    s3c2xx0_softc->sc_intctl_ioh, INTCTL_INTSUBMSK, mask);
	} else {
		mask = bus_space_read_4(&s3c2xx0_bs_tag,
		    s3c2xx0_softc->sc_gpio_ioh, GPIO_EINTMASK);
		mask &= ~(1 << (irq - S3C24X0_EXTIRQ_MIN));
		bus_space_write_4(&s3c2xx0_bs_tag, 
		    s3c2xx0_softc->sc_intctl_ioh, GPIO_EINTMASK, mask);
	}
}
