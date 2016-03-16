/*-
 * Copyright (c) 2016 Michal Meloun <mmel@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Tegra GPIO driver.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/gpio.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/fdt/fdt_common.h>
#include <dev/gpio/gpiobusvar.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>


#define	GPIO_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	GPIO_UNLOCK(_sc)	mtx_unlock(&(_sc)->sc_mtx)
#define	GPIO_LOCK_INIT(_sc)	mtx_init(&_sc->sc_mtx, 			\
	    device_get_nameunit(_sc->sc_dev), "tegra_gpio", MTX_DEF)
#define	GPIO_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define	GPIO_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define	GPIO_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

#define	WR4(_sc, _r, _v)	bus_write_4((_sc)->mem_res, (_r), (_v))
#define	RD4(_sc, _r)		bus_read_4((_sc)->mem_res, (_r))

#define	GPIO_BANK_OFFS		0x100	/* Bank offset */
#define	GPIO_NUM_BANKS		8	/* Total number per bank */
#define	GPIO_REGS_IN_BANK	4	/* Total registers in bank */
#define	GPIO_PINS_IN_REG	8	/* Total pin in register */

#define	GPIO_BANKNUM(n)		((n) / (GPIO_REGS_IN_BANK * GPIO_PINS_IN_REG))
#define	GPIO_PORTNUM(n)		(((n) / GPIO_PINS_IN_REG) % GPIO_REGS_IN_BANK)
#define	GPIO_BIT(n)		((n) % GPIO_PINS_IN_REG)

#define	GPIO_REGNUM(n)		(GPIO_BANKNUM(n) * GPIO_BANK_OFFS + \
				    GPIO_PORTNUM(n) * 4)

#define	NGPIO	((GPIO_NUM_BANKS * GPIO_REGS_IN_BANK * GPIO_PINS_IN_REG) - 8)

/* Register offsets */
#define	GPIO_CNF		0x00
#define	GPIO_OE			0x10
#define	GPIO_OUT		0x20
#define	GPIO_IN			0x30
#define	GPIO_INT_STA		0x40
#define	GPIO_INT_ENB		0x50
#define	GPIO_INT_LVL		0x60
#define	GPIO_INT_CLR		0x70
#define	GPIO_MSK_CNF		0x80
#define	GPIO_MSK_OE		0x90
#define	GPIO_MSK_OUT		0xA0
#define	GPIO_MSK_INT_STA	0xC0
#define	GPIO_MSK_INT_ENB	0xD0
#define	GPIO_MSK_INT_LVL	0xE0

char *tegra_gpio_port_names[] = {
	 "A",  "B",  "C",  "D", /* Bank 0 */
	 "E",  "F",  "G",  "H", /* Bank 1 */
	 "I",  "J",  "K",  "L", /* Bank 2 */
	 "M",  "N",  "O",  "P", /* Bank 3 */
	 "Q",  "R",  "S",  "T", /* Bank 4 */
	 "U",  "V",  "W",  "X", /* Bank 5 */
	 "Y",  "Z", "AA", "BB", /* Bank 5 */
	"CC", "DD", "EE"	/* Bank 5 */
};

struct tegra_gpio_softc {
	device_t		dev;
	device_t		sc_busdev;
	struct mtx		sc_mtx;
	struct resource		*mem_res;
	struct resource		*irq_res;
	void			*gpio_ih;
	int			gpio_npins;
	struct gpio_pin		gpio_pins[NGPIO];
};

static struct ofw_compat_data compat_data[] = {
	{"nvidia,tegra124-gpio", 1},
	{NULL,			0}
};

static inline void
gpio_write_masked(struct tegra_gpio_softc *sc, bus_size_t reg,
    struct gpio_pin *pin, uint32_t val)
{
	uint32_t tmp;
	int bit;

	bit = GPIO_BIT(pin->gp_pin);
	tmp = 0x100 << bit;		/* mask */
	tmp |= (val & 1) << bit;	/* value */
	bus_write_4(sc->mem_res, reg + GPIO_REGNUM(pin->gp_pin), tmp);
}
static inline uint32_t
gpio_read(struct tegra_gpio_softc *sc, bus_size_t reg, struct gpio_pin *pin)
{
	int bit;
	uint32_t val;

	bit = GPIO_BIT(pin->gp_pin);
	val = bus_read_4(sc->mem_res, reg + GPIO_REGNUM(pin->gp_pin));
	return (val >> bit) & 1;
}

static void
tegra_gpio_pin_configure(struct tegra_gpio_softc *sc, struct gpio_pin *pin,
    unsigned int flags)
{

	if ((flags & (GPIO_PIN_INPUT|GPIO_PIN_OUTPUT)) == 0)
		return;

	/* Manage input/output */
	pin->gp_flags &= ~(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT);
	if (flags & GPIO_PIN_OUTPUT) {
		pin->gp_flags |= GPIO_PIN_OUTPUT;
		gpio_write_masked(sc, GPIO_MSK_OE, pin, 1);
	} else {
		pin->gp_flags |= GPIO_PIN_INPUT;
		gpio_write_masked(sc, GPIO_MSK_OE, pin, 0);
	}
}

static device_t
tegra_gpio_get_bus(device_t dev)
{
	struct tegra_gpio_softc *sc;

	sc = device_get_softc(dev);
	return (sc->sc_busdev);
}

static int
tegra_gpio_pin_max(device_t dev, int *maxpin)
{

	*maxpin = NGPIO - 1;
	return (0);
}

static int
tegra_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct tegra_gpio_softc *sc;

	sc = device_get_softc(dev);
	if (pin >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	*caps = sc->gpio_pins[pin].gp_caps;
	GPIO_UNLOCK(sc);

	return (0);
}

static int
tegra_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct tegra_gpio_softc *sc;
	int cnf;

	sc = device_get_softc(dev);
	if (pin >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	cnf = gpio_read(sc, GPIO_CNF, &sc->gpio_pins[pin]);
	if (cnf == 0) {
		GPIO_UNLOCK(sc);
		return (ENXIO);
	}
	*flags = sc->gpio_pins[pin].gp_flags;
	GPIO_UNLOCK(sc);

	return (0);
}

static int
tegra_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct tegra_gpio_softc *sc;

	sc = device_get_softc(dev);
	if (pin >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	memcpy(name, sc->gpio_pins[pin].gp_name, GPIOMAXNAME);
	GPIO_UNLOCK(sc);

	return (0);
}

static int
tegra_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct tegra_gpio_softc *sc;
	int cnf;

	sc = device_get_softc(dev);
	if (pin >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	cnf = gpio_read(sc, GPIO_CNF,  &sc->gpio_pins[pin]);
	if (cnf == 0) {
		/* XXX - allow this for while ....
		GPIO_UNLOCK(sc);
		return (ENXIO);
		*/
		gpio_write_masked(sc, GPIO_MSK_CNF,  &sc->gpio_pins[pin], 1);
	}
	tegra_gpio_pin_configure(sc, &sc->gpio_pins[pin], flags);
	GPIO_UNLOCK(sc);

	return (0);
}

static int
tegra_gpio_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct tegra_gpio_softc *sc;

	sc = device_get_softc(dev);
	if (pin >= sc->gpio_npins)
		return (EINVAL);
	GPIO_LOCK(sc);
	gpio_write_masked(sc, GPIO_MSK_OUT, &sc->gpio_pins[pin], value);
	GPIO_UNLOCK(sc);

	return (0);
}

static int
tegra_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct tegra_gpio_softc *sc;

	sc = device_get_softc(dev);
	if (pin >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	*val = gpio_read(sc, GPIO_IN, &sc->gpio_pins[pin]);
	GPIO_UNLOCK(sc);

	return (0);
}

static int
tegra_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct tegra_gpio_softc *sc;

	sc = device_get_softc(dev);
	if (pin >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	gpio_write_masked(sc, GPIO_MSK_OE, &sc->gpio_pins[pin],
	     gpio_read(sc, GPIO_IN, &sc->gpio_pins[pin]) ^ 1);
	GPIO_UNLOCK(sc);

	return (0);
}

static int
tegra_gpio_intr(void *arg)
{
	struct tegra_gpio_softc *sc;
	uint32_t val;
	int i;

	sc = arg;
	for (i = 0; i < NGPIO; i += GPIO_PINS_IN_REG) {
		/* Clear interrupt */
		val = bus_read_4(sc->mem_res, GPIO_INT_STA + GPIO_REGNUM(i));
		val &= bus_read_4(sc->mem_res, GPIO_INT_ENB + GPIO_REGNUM(i));
		bus_write_4(sc->mem_res, GPIO_INT_CLR + GPIO_REGNUM(i), val);
		/* Interrupt handling */
#ifdef not_yet
		for (j = 0; j < GPIO_PINS_IN_REG; j++) {
			if (val & (1 << j))
				handle_irq(i + j);
		}
		*/
#endif
	}
	return (FILTER_HANDLED);
}

static int
tegra_gpio_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data != 0) {
		device_set_desc(dev, "Tegra GPIO Controller");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
tegra_gpio_detach(device_t dev)
{
	struct tegra_gpio_softc *sc;

	sc = device_get_softc(dev);

	KASSERT(mtx_initialized(&sc->sc_mtx), ("gpio mutex not initialized"));

	gpiobus_detach_bus(dev);
	if (sc->gpio_ih != NULL)
		bus_teardown_intr(dev, sc->irq_res, sc->gpio_ih);
	if (sc->irq_res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq_res);
	if (sc->mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->mem_res);
	mtx_destroy(&sc->sc_mtx);

	return(0);
}

static int
tegra_gpio_attach(device_t dev)
{
	struct tegra_gpio_softc *sc;
	int i, rid;

	sc = device_get_softc(dev);
	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	/* Allocate bus_space resources. */
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "Cannot allocate memory resources\n");
		tegra_gpio_detach(dev);
		return (ENXIO);
	}

	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "Cannot allocate IRQ resources\n");
		tegra_gpio_detach(dev);
		return (ENXIO);
	}

	sc->dev = dev;
	sc->gpio_npins = NGPIO;

	if ((bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC,
	    tegra_gpio_intr, NULL, sc, &sc->gpio_ih))) {
		device_printf(dev,
		    "WARNING: unable to register interrupt handler\n");
		tegra_gpio_detach(dev);
		return (ENXIO);
	}

	for (i = 0; i < sc->gpio_npins; i++) {
		sc->gpio_pins[i].gp_pin = i;
		sc->gpio_pins[i].gp_caps = GPIO_PIN_INPUT | GPIO_PIN_OUTPUT;
		snprintf(sc->gpio_pins[i].gp_name, GPIOMAXNAME, "gpio_%s.%d",
		    tegra_gpio_port_names[ i / GPIO_PINS_IN_REG],
		    i % GPIO_PINS_IN_REG);
		sc->gpio_pins[i].gp_flags =
		    gpio_read(sc, GPIO_OE, &sc->gpio_pins[i]) != 0 ?
		    GPIO_PIN_OUTPUT : GPIO_PIN_INPUT;
	}

	sc->sc_busdev = gpiobus_attach_bus(dev);
	if (sc->sc_busdev == NULL) {
		tegra_gpio_detach(dev);
		return (ENXIO);
	}

	return (bus_generic_attach(dev));
}

static int
tegra_map_gpios(device_t dev, phandle_t pdev, phandle_t gparent,
    int gcells, pcell_t *gpios, uint32_t *pin, uint32_t *flags)
{

	if (gcells != 2)
		return (ERANGE);
	*pin = gpios[0];
	*flags= gpios[1];
	return (0);
}

static phandle_t
tegra_gpio_get_node(device_t bus, device_t dev)
{

	/* We only have one child, the GPIO bus, which needs our own node. */
	return (ofw_bus_get_node(bus));
}

static device_method_t tegra_gpio_methods[] = {
	DEVMETHOD(device_probe,		tegra_gpio_probe),
	DEVMETHOD(device_attach,	tegra_gpio_attach),
	DEVMETHOD(device_detach,	tegra_gpio_detach),

	/* GPIO protocol */
	DEVMETHOD(gpio_get_bus,		tegra_gpio_get_bus),
	DEVMETHOD(gpio_pin_max,		tegra_gpio_pin_max),
	DEVMETHOD(gpio_pin_getname,	tegra_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getflags,	tegra_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_getcaps,	tegra_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_setflags,	tegra_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_get,		tegra_gpio_pin_get),
	DEVMETHOD(gpio_pin_set,		tegra_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle,	tegra_gpio_pin_toggle),
	DEVMETHOD(gpio_map_gpios,	tegra_map_gpios),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	tegra_gpio_get_node),

	DEVMETHOD_END
};

static driver_t tegra_gpio_driver = {
	"gpio",
	tegra_gpio_methods,
	sizeof(struct tegra_gpio_softc),
};
static devclass_t tegra_gpio_devclass;

EARLY_DRIVER_MODULE(tegra_gpio, simplebus, tegra_gpio_driver,
    tegra_gpio_devclass, 0, 0, 70);
