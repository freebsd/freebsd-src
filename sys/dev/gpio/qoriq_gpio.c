/*-
 * Copyright (c) 2020 Alstom Group.
 * Copyright (c) 2020 Semihalf.
 * Copyright (c) 2015 Justin Hibbits
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
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/gpio.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/stdarg.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dt-bindings/interrupt-controller/irq.h>

#include "gpio_if.h"
#include "pic_if.h"

#define	BIT(x)		(1 << (x))
#define MAXPIN		(31)

#define VALID_PIN(u)	((u) >= 0 && (u) <= MAXPIN)
#define DEFAULT_CAPS	(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT | \
			 GPIO_PIN_OPENDRAIN | GPIO_PIN_PUSHPULL | \
			 GPIO_INTR_EDGE_FALLING | GPIO_INTR_EDGE_BOTH | \
			 GPIO_PIN_PULLUP)

#define	GPIO_LOCK(sc)	mtx_lock_spin(&(sc)->sc_mtx)
#define	GPIO_UNLOCK(sc)	mtx_unlock_spin(&(sc)->sc_mtx)
#define GPIO_LOCK_INIT(sc) \
	mtx_init(&(sc)->sc_mtx, device_get_nameunit((sc)->dev),	\
	    "gpio", MTX_SPIN)
#define GPIO_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);

#define	GPIO_GPDIR	0x0
#define	GPIO_GPODR	0x4
#define	GPIO_GPDAT	0x8
#define	GPIO_GPIER	0xc
#define	GPIO_GPIMR	0x10
#define	GPIO_GPICR	0x14
#define	GPIO_GPIBE	0x18

struct qoriq_gpio_irqsrc {
	struct intr_irqsrc	isrc;
	int			pin;
};

struct qoriq_gpio_softc {
	device_t	dev;
	device_t	busdev;
	struct mtx	sc_mtx;
	struct resource *sc_mem;	/* Memory resource */
	struct resource	*sc_intr;
	void		*intr_cookie;
	struct gpio_pin	 sc_pins[MAXPIN + 1];
	struct qoriq_gpio_irqsrc sc_isrcs[MAXPIN + 1];
	struct intr_map_data_gpio gdata;
};

static device_t
qoriq_gpio_get_bus(device_t dev)
{
	struct qoriq_gpio_softc *sc;

	sc = device_get_softc(dev);

	return (sc->busdev);
}

static int
qoriq_gpio_pin_max(device_t dev, int *maxpin)
{

	*maxpin = MAXPIN;
	return (0);
}

/* Get a specific pin's capabilities. */
static int
qoriq_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct qoriq_gpio_softc *sc;

	sc = device_get_softc(dev);

	if (!VALID_PIN(pin))
		return (EINVAL);

	GPIO_LOCK(sc);
	*caps = sc->sc_pins[pin].gp_caps;
	GPIO_UNLOCK(sc);

	return (0);
}

/* Get a specific pin's name. */
static int
qoriq_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{

	if (!VALID_PIN(pin))
		return (EINVAL);

	snprintf(name, GPIOMAXNAME, "qoriq_gpio%d.%d",
	    device_get_unit(dev), pin);
	name[GPIOMAXNAME-1] = '\0';

	return (0);
}

static int
qoriq_gpio_pin_configure(device_t dev, uint32_t pin, uint32_t flags)
{
	struct qoriq_gpio_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	if ((flags & sc->sc_pins[pin].gp_caps) != flags) {
		return (EINVAL);
	}

	if (flags & GPIO_PIN_INPUT) {
		reg = bus_read_4(sc->sc_mem, GPIO_GPDIR);
		reg &= ~(1 << (31 - pin));
		bus_write_4(sc->sc_mem, GPIO_GPDIR, reg);
	}
	else if (flags & GPIO_PIN_OUTPUT) {
		reg = bus_read_4(sc->sc_mem, GPIO_GPDIR);
		reg |= (1 << (31 - pin));
		bus_write_4(sc->sc_mem, GPIO_GPDIR, reg);
		reg = bus_read_4(sc->sc_mem, GPIO_GPODR);
		if (flags & GPIO_PIN_OPENDRAIN)
			reg |= (1 << (31 - pin));
		else
			reg &= ~(1 << (31 - pin));
		bus_write_4(sc->sc_mem, GPIO_GPODR, reg);
	}
	sc->sc_pins[pin].gp_flags = flags;

	return (0);
}

/* Set flags for the pin. */
static int
qoriq_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct qoriq_gpio_softc *sc = device_get_softc(dev);
	uint32_t ret;

	if (!VALID_PIN(pin))
		return (EINVAL);

	if ((flags & (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)) ==
	    (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT))
		return (EINVAL);

	GPIO_LOCK(sc);
	ret = qoriq_gpio_pin_configure(dev, pin, flags);
	GPIO_UNLOCK(sc);
	return (0);
}

static int
qoriq_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *pflags)
{
	struct qoriq_gpio_softc *sc;

	if (!VALID_PIN(pin))
		return (EINVAL);

	sc = device_get_softc(dev);

	GPIO_LOCK(sc);
	*pflags = sc->sc_pins[pin].gp_flags;
	GPIO_UNLOCK(sc);

	return (0);
}

/* Set a specific output pin's value. */
static int
qoriq_gpio_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct qoriq_gpio_softc *sc = device_get_softc(dev);
	uint32_t outvals;
	uint8_t pinbit;

	if (!VALID_PIN(pin) || value > 1)
		return (EINVAL);

	GPIO_LOCK(sc);
	pinbit = 31 - pin;

	outvals = bus_read_4(sc->sc_mem, GPIO_GPDAT);
	outvals &= ~(1 << pinbit);
	outvals |= (value << pinbit);
	bus_write_4(sc->sc_mem, GPIO_GPDAT, outvals);

	GPIO_UNLOCK(sc);

	return (0);
}

/* Get a specific pin's input value. */
static int
qoriq_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *value)
{
	struct qoriq_gpio_softc *sc = device_get_softc(dev);

	if (!VALID_PIN(pin))
		return (EINVAL);

	*value = (bus_read_4(sc->sc_mem, GPIO_GPDAT) >> (31 - pin)) & 1;

	return (0);
}

/* Toggle a pin's output value. */
static int
qoriq_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct qoriq_gpio_softc *sc = device_get_softc(dev);
	uint32_t val;

	if (!VALID_PIN(pin))
		return (EINVAL);

	GPIO_LOCK(sc);

	val = bus_read_4(sc->sc_mem, GPIO_GPDAT);
	val ^= (1 << (31 - pin));
	bus_write_4(sc->sc_mem, GPIO_GPDAT, val);

	GPIO_UNLOCK(sc);

	return (0);
}

static void
qoriq_gpio_set_intr(struct qoriq_gpio_softc *sc, int pin, bool enable)
{
	uint32_t reg;

	reg = bus_read_4(sc->sc_mem, GPIO_GPIMR);
	if (enable)
		reg |= BIT(31 - pin);
	else
		reg &= ~BIT(31 - pin);
	bus_write_4(sc->sc_mem, GPIO_GPIMR, reg);
}

static void
qoriq_gpio_ack_intr(struct qoriq_gpio_softc *sc, int pin)
{
	uint32_t reg;

	reg = BIT(31 - pin);
	bus_write_4(sc->sc_mem, GPIO_GPIER, reg);
}

static int
qoriq_gpio_intr(void *arg)
{
	struct qoriq_gpio_softc *sc;
	struct trapframe *tf;
	uint32_t status;
	int pin;

	sc = (struct qoriq_gpio_softc *)arg;
	tf = curthread->td_intr_frame;

	status = bus_read_4(sc->sc_mem, GPIO_GPIER);
	status &= bus_read_4(sc->sc_mem, GPIO_GPIMR);
	while (status != 0) {
		pin = ffs(status) - 1;
		status &= ~BIT(pin);
		pin = 31 - pin;

		if (intr_isrc_dispatch(&sc->sc_isrcs[pin].isrc, tf) != 0) {
			GPIO_LOCK(sc);
			qoriq_gpio_set_intr(sc, pin, false);
			qoriq_gpio_ack_intr(sc, pin);
			GPIO_UNLOCK(sc);
			device_printf(sc->dev,
			    "Masking spurious pin interrupt %d\n",
			    pin);
		}
	}

	return (FILTER_HANDLED);
}

static void
qoriq_gpio_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct qoriq_gpio_softc *sc;
	struct qoriq_gpio_irqsrc *qisrc;

	sc = device_get_softc(dev);
	qisrc = (struct qoriq_gpio_irqsrc *)isrc;

	GPIO_LOCK(sc);
	qoriq_gpio_set_intr(sc, qisrc->pin, false);
	GPIO_UNLOCK(sc);
}

static void
qoriq_gpio_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct qoriq_gpio_softc *sc;
	struct qoriq_gpio_irqsrc *qisrc;

	sc = device_get_softc(dev);
	qisrc = (struct qoriq_gpio_irqsrc *)isrc;

	GPIO_LOCK(sc);
	qoriq_gpio_set_intr(sc, qisrc->pin, true);
	GPIO_UNLOCK(sc);
}

static struct intr_map_data_gpio*
qoriq_gpio_convert_map_data(struct qoriq_gpio_softc *sc, struct intr_map_data *data)
{
	struct intr_map_data_gpio *gdata;
	struct intr_map_data_fdt *daf;

	switch (data->type) {
	case INTR_MAP_DATA_GPIO:
		gdata = (struct intr_map_data_gpio *)data;
		break;
	case INTR_MAP_DATA_FDT:
		daf = (struct intr_map_data_fdt *)data;
		if (daf->ncells != 2)
			return (NULL);

		gdata = &sc->gdata;
		gdata->gpio_pin_num = daf->cells[0];
		switch (daf->cells[1]) {
		case IRQ_TYPE_LEVEL_LOW:
			gdata->gpio_intr_mode = GPIO_INTR_LEVEL_LOW;
			break;
		case IRQ_TYPE_LEVEL_HIGH:
			gdata->gpio_intr_mode = GPIO_INTR_LEVEL_HIGH;
			break;
		case IRQ_TYPE_EDGE_RISING:
			gdata->gpio_intr_mode = GPIO_INTR_EDGE_RISING;
			break;
		case IRQ_TYPE_EDGE_FALLING:
			gdata->gpio_intr_mode = GPIO_INTR_EDGE_FALLING;
			break;
		case IRQ_TYPE_EDGE_BOTH:
			gdata->gpio_intr_mode = GPIO_INTR_EDGE_BOTH;
			break;
		default:
			return (NULL);
		}
		break;
	default:
		return (NULL);
	}

	return (gdata);
}


static int
qoriq_gpio_map_intr(device_t dev, struct intr_map_data *data,
    struct intr_irqsrc **isrcp)
{
	struct qoriq_gpio_softc *sc;
	struct intr_map_data_gpio *gdata;
	int pin;

	sc = device_get_softc(dev);

	gdata = qoriq_gpio_convert_map_data(sc, data);
	if (gdata == NULL)
		return (EINVAL);

	pin = gdata->gpio_pin_num;
	if (pin > MAXPIN)
		return (EINVAL);

	*isrcp = &sc->sc_isrcs[pin].isrc;
	return (0);
}

static int
qoriq_gpio_setup_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct qoriq_gpio_softc *sc;
	struct intr_map_data_gpio *gdata;
	struct qoriq_gpio_irqsrc *qisrc;
	bool falling;
	uint32_t reg;

	sc = device_get_softc(dev);
	qisrc = (struct qoriq_gpio_irqsrc *)isrc;

	gdata = qoriq_gpio_convert_map_data(sc, data);
	if (gdata == NULL)
		return (EINVAL);

	if (gdata->gpio_intr_mode & GPIO_INTR_EDGE_BOTH)
		falling = false;
	else if (gdata->gpio_intr_mode & GPIO_INTR_EDGE_FALLING)
		falling = true;
	else
		return (EOPNOTSUPP);

	GPIO_LOCK(sc);
	reg = bus_read_4(sc->sc_mem, GPIO_GPICR);
	if (falling)
		reg |= BIT(31 - qisrc->pin);
	else
		reg &= ~BIT(31 - qisrc->pin);
	bus_write_4(sc->sc_mem, GPIO_GPICR, reg);
	GPIO_UNLOCK(sc);

	return (0);
}

static int
qoriq_gpio_teardown_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct qoriq_gpio_softc *sc;
	struct qoriq_gpio_irqsrc *qisrc;

	sc = device_get_softc(dev);
	qisrc = (struct qoriq_gpio_irqsrc *)isrc;

	if (isrc->isrc_handlers > 0)
		return (0);

	GPIO_LOCK(sc);
	qoriq_gpio_set_intr(sc, qisrc->pin, false);
	GPIO_UNLOCK(sc);
	return (0);
}

static void
qoriq_gpio_post_filter(device_t dev, struct intr_irqsrc *isrc)
{
	struct qoriq_gpio_softc *sc;
	struct qoriq_gpio_irqsrc *qisrc;

	sc = device_get_softc(dev);
	qisrc = (struct qoriq_gpio_irqsrc *)isrc;

	GPIO_LOCK(sc);
	qoriq_gpio_ack_intr(sc, qisrc->pin);
	GPIO_UNLOCK(sc);
}


static void
qoriq_gpio_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct qoriq_gpio_softc *sc;
	struct qoriq_gpio_irqsrc *qisrc;

	sc = device_get_softc(dev);
	qisrc = (struct qoriq_gpio_irqsrc *)isrc;

	GPIO_LOCK(sc);
	qoriq_gpio_ack_intr(sc, qisrc->pin);
	qoriq_gpio_set_intr(sc, qisrc->pin, true);
	GPIO_UNLOCK(sc);
}

static void
qoriq_gpio_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct qoriq_gpio_softc *sc;
	struct qoriq_gpio_irqsrc *qisrc;

	sc = device_get_softc(dev);
	qisrc = (struct qoriq_gpio_irqsrc *)isrc;

	GPIO_LOCK(sc);
	qoriq_gpio_set_intr(sc, qisrc->pin, false);
	GPIO_UNLOCK(sc);
}

static struct ofw_compat_data gpio_matches[] = {
    {"fsl,qoriq-gpio", 1},
    {"fsl,pq3-gpio", 1},
    {"fsl,mpc8572-gpio", 1},
    {0, 0}
};

static int
qoriq_gpio_probe(device_t dev)
{

	if (ofw_bus_search_compatible(dev, gpio_matches)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Freescale QorIQ GPIO driver");

	return (0);
}

static int
qoriq_gpio_pin_access_32(device_t dev, uint32_t first_pin, uint32_t clear_pins,
    uint32_t change_pins, uint32_t *orig_pins)
{
	struct qoriq_gpio_softc *sc;
	uint32_t hwstate;

	sc = device_get_softc(dev);

	if (first_pin != 0)
		return (EINVAL);

	GPIO_LOCK(sc);
	hwstate = bus_read_4(sc->sc_mem, GPIO_GPDAT);
	bus_write_4(sc->sc_mem, GPIO_GPDAT,
	    (hwstate & ~clear_pins) ^ change_pins);
	GPIO_UNLOCK(sc);

	if (orig_pins != NULL)
		*orig_pins = hwstate;

	return (0);
}

static int
qoriq_gpio_pin_config_32(device_t dev, uint32_t first_pin, uint32_t num_pins,
    uint32_t *pin_flags)
{
	uint32_t dir, odr, mask, reg;
	struct qoriq_gpio_softc *sc;
	uint32_t newflags[32];
	int i;

	if (first_pin != 0 || !VALID_PIN(num_pins))
		return (EINVAL);

	sc = device_get_softc(dev);

	dir = odr = mask = 0;

	for (i = 0; i < num_pins; i++) {
		newflags[i] = 0;
		mask |= (1 << i);

		if (pin_flags[i] & GPIO_PIN_INPUT) {
			newflags[i] = GPIO_PIN_INPUT;
			dir &= ~(1 << i);
		} else {
			newflags[i] = GPIO_PIN_OUTPUT;
			dir |= (1 << i);

			if (pin_flags[i] & GPIO_PIN_OPENDRAIN) {
				newflags[i] |= GPIO_PIN_OPENDRAIN;
				odr |= (1 << i);
			} else {
				newflags[i] |= GPIO_PIN_PUSHPULL;
				odr &= ~(1 << i);
			}
		}
	}

	GPIO_LOCK(sc);

	reg = (bus_read_4(sc->sc_mem, GPIO_GPDIR) & ~mask) | dir;
	bus_write_4(sc->sc_mem, GPIO_GPDIR, reg);

	reg = (bus_read_4(sc->sc_mem, GPIO_GPODR) & ~mask) | odr;
	bus_write_4(sc->sc_mem, GPIO_GPODR, reg);

	for (i = 0; i < num_pins; i++)
		sc->sc_pins[i].gp_flags = newflags[i];

	GPIO_UNLOCK(sc);

	return (0);
}

static int
qoriq_gpio_map_gpios(device_t bus, phandle_t dev, phandle_t gparent, int gcells,
    pcell_t *gpios, uint32_t *pin, uint32_t *flags)
{
	struct qoriq_gpio_softc *sc;
	int err;

	if (!VALID_PIN(gpios[0]))
		return (EINVAL);

	sc = device_get_softc(bus);
	GPIO_LOCK(sc);
	err = qoriq_gpio_pin_configure(bus, gpios[0], gpios[1]);
	GPIO_UNLOCK(sc);

	if (err == 0) {
		*pin = gpios[0];
		*flags = gpios[1];
	}

	return (err);
}

static int qoriq_gpio_detach(device_t dev);

static int
qoriq_gpio_attach(device_t dev)
{
	struct qoriq_gpio_softc *sc = device_get_softc(dev);
	int i, rid, error;
	const char *name;
	intptr_t xref;

	sc->dev = dev;

	GPIO_LOCK_INIT(sc);

	/* Allocate memory. */
	rid = 0;
	sc->sc_mem = bus_alloc_resource_any(dev,
		     SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (sc->sc_mem == NULL) {
		device_printf(dev, "Can't allocate memory for device output port");
		error = ENOMEM;
		goto fail;
	}

	rid = 0;
	sc->sc_intr = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->sc_intr == NULL) {
		device_printf(dev, "Can't allocate interrupt resource.\n");
		error = ENOMEM;
		goto fail;
	}

	error = bus_setup_intr(dev, sc->sc_intr, INTR_TYPE_MISC | INTR_MPSAFE,
	    qoriq_gpio_intr, NULL, sc, &sc->intr_cookie);
	if (error != 0) {
		device_printf(dev, "Failed to setup interrupt.\n");
		goto fail;
	}

	name = device_get_nameunit(dev);
	for (i = 0; i <= MAXPIN; i++) {
		sc->sc_pins[i].gp_caps = DEFAULT_CAPS;
		sc->sc_isrcs[i].pin = i;
		error = intr_isrc_register(&sc->sc_isrcs[i].isrc,
		    dev, 0, "%s,%u", name, i);
		if (error != 0)
			goto fail;
	}

	xref = OF_xref_from_node(ofw_bus_get_node(dev));
	if (intr_pic_register(dev, xref) == NULL) {
		error = ENXIO;
		goto fail;
	}

	sc->busdev = gpiobus_attach_bus(dev);
	if (sc->busdev == NULL) {
		error = ENXIO;
		goto fail;
	}
	/*
	 * Enable the GPIO Input Buffer for all GPIOs.
	 * This is safe on devices without a GPIBE register, because those
	 * devices ignore writes and read 0's in undefined portions of the map.
	 */
	if (ofw_bus_is_compatible(dev, "fsl,qoriq-gpio"))
		bus_write_4(sc->sc_mem, GPIO_GPIBE, 0xffffffff);

	OF_device_register_xref(OF_xref_from_node(ofw_bus_get_node(dev)), dev);

	bus_write_4(sc->sc_mem, GPIO_GPIER, 0xffffffff);
	bus_write_4(sc->sc_mem, GPIO_GPIMR, 0);

	return (0);
fail:
	qoriq_gpio_detach(dev);
	return (error);
}

static int
qoriq_gpio_detach(device_t dev)
{
	struct qoriq_gpio_softc *sc = device_get_softc(dev);

	gpiobus_detach_bus(dev);

	if (sc->sc_mem != NULL) {
		/* Release output port resource. */
		bus_release_resource(dev, SYS_RES_MEMORY,
				     rman_get_rid(sc->sc_mem), sc->sc_mem);
	}

	if (sc->intr_cookie != NULL)
		bus_teardown_intr(dev, sc->sc_intr, sc->intr_cookie);

	if (sc->sc_intr != NULL)
		bus_release_resource(dev, SYS_RES_IRQ,
		    rman_get_rid(sc->sc_intr), sc->sc_intr);

	GPIO_LOCK_DESTROY(sc);

	return (0);
}

static device_method_t qoriq_gpio_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe, 	qoriq_gpio_probe),
	DEVMETHOD(device_attach, 	qoriq_gpio_attach),
	DEVMETHOD(device_detach, 	qoriq_gpio_detach),

	/* Bus interface */
	DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
	DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),

	/* GPIO protocol */
	DEVMETHOD(gpio_get_bus, 	qoriq_gpio_get_bus),
	DEVMETHOD(gpio_pin_max, 	qoriq_gpio_pin_max),
	DEVMETHOD(gpio_pin_getname, 	qoriq_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getcaps, 	qoriq_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_get, 	qoriq_gpio_pin_get),
	DEVMETHOD(gpio_pin_set, 	qoriq_gpio_pin_set),
	DEVMETHOD(gpio_pin_getflags, 	qoriq_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_setflags, 	qoriq_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_toggle, 	qoriq_gpio_pin_toggle),

	DEVMETHOD(gpio_map_gpios,	qoriq_gpio_map_gpios),
	DEVMETHOD(gpio_pin_access_32,	qoriq_gpio_pin_access_32),
	DEVMETHOD(gpio_pin_config_32,	qoriq_gpio_pin_config_32),

	/* Interrupt controller */
	DEVMETHOD(pic_disable_intr,	qoriq_gpio_disable_intr),
	DEVMETHOD(pic_enable_intr,	qoriq_gpio_enable_intr),
	DEVMETHOD(pic_map_intr,		qoriq_gpio_map_intr),
	DEVMETHOD(pic_setup_intr,	qoriq_gpio_setup_intr),
	DEVMETHOD(pic_teardown_intr,	qoriq_gpio_teardown_intr),
	DEVMETHOD(pic_post_filter,	qoriq_gpio_post_filter),
	DEVMETHOD(pic_post_ithread,	qoriq_gpio_post_ithread),
	DEVMETHOD(pic_pre_ithread,	qoriq_gpio_pre_ithread),

	DEVMETHOD_END
};

static driver_t qoriq_gpio_driver = {
	"gpio",
	qoriq_gpio_methods,
	sizeof(struct qoriq_gpio_softc),
};
static devclass_t qoriq_gpio_devclass;

/*
 * This needs to be loaded after interrupts are available and
 * before consumers need it.
 */
EARLY_DRIVER_MODULE(qoriq_gpio, simplebus, qoriq_gpio_driver, qoriq_gpio_devclass,
    NULL, NULL, BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LATE);
