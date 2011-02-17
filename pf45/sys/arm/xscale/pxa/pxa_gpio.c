/*-
 * Copyright (c) 2006 Benno Rice.  All rights reserved.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/interrupt.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/timetc.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <arm/xscale/pxa/pxavar.h>
#include <arm/xscale/pxa/pxareg.h>

struct pxa_gpio_softc {
	struct resource *	pg_res[4];
	bus_space_tag_t		pg_bst;
	bus_space_handle_t	pg_bsh;
	struct mtx		pg_mtx;

	uint32_t		pg_intr[3];
};

static struct resource_spec pxa_gpio_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		1,	RF_ACTIVE },
	{ SYS_RES_IRQ,		2,	RF_ACTIVE },
	{ -1, 0 }
};

static struct pxa_gpio_softc *pxa_gpio_softc = NULL;

static int	pxa_gpio_probe(device_t);
static int	pxa_gpio_attach(device_t);

static driver_filter_t	pxa_gpio_intr0;
static driver_filter_t	pxa_gpio_intr1;
static driver_filter_t	pxa_gpio_intrN;

static int
pxa_gpio_probe(device_t dev)
{
	
	device_set_desc(dev, "GPIO Controller");
	return (0);
}

static int
pxa_gpio_attach(device_t dev)
{
	int	error;
	void	*ihl;
	struct	pxa_gpio_softc *sc;
	
	sc = (struct pxa_gpio_softc *)device_get_softc(dev);
	
	if (pxa_gpio_softc != NULL)
		return (ENXIO);
	pxa_gpio_softc = sc;
	
	error = bus_alloc_resources(dev, pxa_gpio_spec, sc->pg_res);
	if (error) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}
	
	sc->pg_bst = rman_get_bustag(sc->pg_res[0]);
	sc->pg_bsh = rman_get_bushandle(sc->pg_res[0]);
	
	/* Disable and clear all interrupts. */
	bus_space_write_4(sc->pg_bst, sc->pg_bsh, GPIO_GRER0, 0);
	bus_space_write_4(sc->pg_bst, sc->pg_bsh, GPIO_GRER1, 0);
	bus_space_write_4(sc->pg_bst, sc->pg_bsh, GPIO_GRER2, 0);
	bus_space_write_4(sc->pg_bst, sc->pg_bsh, GPIO_GFER0, 0);
	bus_space_write_4(sc->pg_bst, sc->pg_bsh, GPIO_GFER1, 0);
	bus_space_write_4(sc->pg_bst, sc->pg_bsh, GPIO_GFER2, 0);
	bus_space_write_4(sc->pg_bst, sc->pg_bsh, GPIO_GEDR0, ~0);
	bus_space_write_4(sc->pg_bst, sc->pg_bsh, GPIO_GEDR1, ~0);
	bus_space_write_4(sc->pg_bst, sc->pg_bsh, GPIO_GEDR2, ~0);
	
	mtx_init(&sc->pg_mtx, "GPIO mutex", NULL, MTX_SPIN);
	
	if (bus_setup_intr(dev, sc->pg_res[1], INTR_TYPE_MISC|INTR_MPSAFE,
	    pxa_gpio_intr0, NULL, sc, &ihl) != 0) {
		bus_release_resources(dev, pxa_gpio_spec, sc->pg_res);
		device_printf(dev, "could not set up intr0\n");
		return (ENXIO);
	}
	
	if (bus_setup_intr(dev, sc->pg_res[2], INTR_TYPE_MISC|INTR_MPSAFE,
	    pxa_gpio_intr1, NULL, sc, &ihl) != 0) {
		bus_release_resources(dev, pxa_gpio_spec, sc->pg_res);
		device_printf(dev, "could not set up intr1\n");
		return (ENXIO);
	}
	
	if (bus_setup_intr(dev, sc->pg_res[3], INTR_TYPE_MISC|INTR_MPSAFE,
	    pxa_gpio_intrN, NULL, sc, &ihl) != 0) {
		bus_release_resources(dev, pxa_gpio_spec, sc->pg_res);
		device_printf(dev, "could not set up intrN\n");
		return (ENXIO);
	}
	
	return (0);
}

static int
pxa_gpio_intr0(void *arg)
{
	struct	pxa_gpio_softc *sc;
	
	sc = (struct pxa_gpio_softc *)arg;
	
	bus_space_write_4(sc->pg_bst, sc->pg_bsh, GPIO_GEDR0, 0x1);
	sc->pg_intr[0] |= 1;
	
	return (FILTER_HANDLED);
}

static int
pxa_gpio_intr1(void *arg)
{
	struct	pxa_gpio_softc *sc;
	
	sc = (struct pxa_gpio_softc *)arg;
	
	bus_space_write_4(sc->pg_bst, sc->pg_bsh, GPIO_GEDR0, 0x2);
	sc->pg_intr[1] |= 2;
	
	return (FILTER_HANDLED);
}

static int
pxa_gpio_intrN(void *arg)
{
	uint32_t	gedr0, gedr1, gedr2;
	struct		pxa_gpio_softc *sc;
	
	sc = (struct pxa_gpio_softc *)arg;
	
	gedr0 = bus_space_read_4(sc->pg_bst, sc->pg_bsh, GPIO_GEDR0);
	gedr0 &= 0xfffffffc;
	bus_space_write_4(sc->pg_bst, sc->pg_bsh, GPIO_GEDR0, gedr0);
	
	gedr1 = bus_space_read_4(sc->pg_bst, sc->pg_bsh, GPIO_GEDR1);
	bus_space_write_4(sc->pg_bst, sc->pg_bsh, GPIO_GEDR1, gedr1);
	
	gedr2 = bus_space_read_4(sc->pg_bst, sc->pg_bsh, GPIO_GEDR2);
	gedr2 &= 0x001fffff;
	bus_space_write_4(sc->pg_bst, sc->pg_bsh, GPIO_GEDR2, gedr2);
	
	sc->pg_intr[0] |= gedr0;
	sc->pg_intr[1] |= gedr1;
	sc->pg_intr[2] |= gedr2;
	
	return (FILTER_HANDLED);
}

static device_method_t pxa_gpio_methods[] = {
	DEVMETHOD(device_probe, pxa_gpio_probe),
	DEVMETHOD(device_attach, pxa_gpio_attach),
	
	{0, 0}
};

static driver_t pxa_gpio_driver = {
	"gpio",
	pxa_gpio_methods,
	sizeof(struct pxa_gpio_softc),
};

static devclass_t pxa_gpio_devclass;

DRIVER_MODULE(pxagpio, pxa, pxa_gpio_driver, pxa_gpio_devclass, 0, 0);

#define	pxagpio_reg_read(softc, reg)		\
	bus_space_read_4(sc->pg_bst, sc->pg_bsh, reg)
#define	pxagpio_reg_write(softc, reg, val)	\
	bus_space_write_4(sc->pg_bst, sc->pg_bsh, reg, val)

uint32_t
pxa_gpio_get_function(int gpio)
{
	struct		pxa_gpio_softc *sc;
	uint32_t	rv, io;
	
	sc = pxa_gpio_softc;
	
	rv = pxagpio_reg_read(sc, GPIO_FN_REG(gpio)) >> GPIO_FN_SHIFT(gpio);
	rv = GPIO_FN(rv);
	
	io = pxagpio_reg_read(sc, PXA250_GPIO_REG(GPIO_GPDR0, gpio));
	if (io & GPIO_BIT(gpio))
		rv |= GPIO_OUT;
	
	io = pxagpio_reg_read(sc, PXA250_GPIO_REG(GPIO_GPLR0, gpio));
	if (io & GPIO_BIT(gpio))
		rv |= GPIO_SET;
	
	return (rv);
}

uint32_t
pxa_gpio_set_function(int gpio, uint32_t fn)
{
	struct		pxa_gpio_softc *sc;
	uint32_t	rv, bit, oldfn;
	
	sc = pxa_gpio_softc;
	
	oldfn = pxa_gpio_get_function(gpio);
	
	if (GPIO_FN(fn) == GPIO_FN(oldfn) &&
	    GPIO_FN_IS_OUT(fn) == GPIO_FN_IS_OUT(oldfn)) {
		/*
		 * The pin's function is not changing.
		 * For Alternate Functions and GPIO input, we can just
		 * return now.
		 * For GPIO output pins, check the initial state is
		 * the same.
		 *
		 * Return 'fn' instead of 'oldfn' so the caller can
		 * reliably detect that we didn't change anything.
		 * (The initial state might be different for non-
		 * GPIO output pins).
		 */
		if (!GPIO_IS_GPIO_OUT(fn) ||
		    GPIO_FN_IS_SET(fn) == GPIO_FN_IS_SET(oldfn))
			return (fn);
	}
	
	/*
	 * See section 4.1.3.7 of the PXA2x0 Developer's Manual for
	 * the correct procedure for changing GPIO pin functions.
	 */
	
	bit = GPIO_BIT(gpio);
	
	/*
	 * 1. Configure the correct set/clear state of the pin
	 */
	if (GPIO_FN_IS_SET(fn))
		pxagpio_reg_write(sc, PXA250_GPIO_REG(GPIO_GPSR0, gpio), bit);
	else
		pxagpio_reg_write(sc, PXA250_GPIO_REG(GPIO_GPCR0, gpio), bit);
	
	/*
	 * 2. Configure the pin as an input or output as appropriate
	 */
	rv = pxagpio_reg_read(sc, PXA250_GPIO_REG(GPIO_GPDR0, gpio)) & ~bit;
	if (GPIO_FN_IS_OUT(fn))
		rv |= bit;
	pxagpio_reg_write(sc, PXA250_GPIO_REG(GPIO_GPDR0, gpio), rv);
	
	/*
	 * 3. Configure the pin's function
	 */
	bit = GPIO_FN_MASK << GPIO_FN_SHIFT(gpio);
	fn = GPIO_FN(fn) << GPIO_FN_SHIFT(gpio);
	rv = pxagpio_reg_read(sc, GPIO_FN_REG(gpio)) & ~bit;
	pxagpio_reg_write(sc, GPIO_FN_REG(gpio), rv | fn);
	
	return (oldfn);
}

/*
 * GPIO "interrupt" handling.
 */

void
pxa_gpio_mask_irq(int irq)
{
	uint32_t	val;
	struct		pxa_gpio_softc *sc;
	int		gpio;
	
	sc = pxa_gpio_softc;
	gpio = IRQ_TO_GPIO(irq);
	
	val = pxagpio_reg_read(sc, PXA250_GPIO_REG(GPIO_GRER0, gpio));
	val &= ~GPIO_BIT(gpio);
	pxagpio_reg_write(sc, PXA250_GPIO_REG(GPIO_GRER0, gpio), val);
}

void
pxa_gpio_unmask_irq(int irq)
{
	uint32_t	val;
	struct		pxa_gpio_softc *sc;
	int		gpio;
	
	sc = pxa_gpio_softc;
	gpio = IRQ_TO_GPIO(irq);
	
	val = pxagpio_reg_read(sc, PXA250_GPIO_REG(GPIO_GRER0, gpio));
	val |= GPIO_BIT(gpio);
	pxagpio_reg_write(sc, PXA250_GPIO_REG(GPIO_GRER0, gpio), val);
}

int
pxa_gpio_get_next_irq()
{
	struct  pxa_gpio_softc *sc;
	int     gpio;
	
	sc = pxa_gpio_softc;
	
	if (sc->pg_intr[0] != 0) {
		gpio = ffs(sc->pg_intr[0]) - 1;
		sc->pg_intr[0] &= ~(1 << gpio);
		return (GPIO_TO_IRQ(gpio));
	}
	if (sc->pg_intr[1] != 0) {
		gpio = ffs(sc->pg_intr[1]) - 1;
		sc->pg_intr[1] &= ~(1 << gpio);
		return (GPIO_TO_IRQ(gpio + 32));
	}
	if (sc->pg_intr[2] != 0) {
		gpio = ffs(sc->pg_intr[2]) - 1;
		sc->pg_intr[2] &= ~(1 << gpio);
		return (GPIO_TO_IRQ(gpio + 64));
	}

	return (-1);
}
