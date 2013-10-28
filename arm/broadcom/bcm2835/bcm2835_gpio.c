/*-
 * Copyright (c) 2012 Oleksandr Tymoshenko <gonzo@freebsd.org>
 * Copyright (c) 2012 Luiz Otavio O Souza.
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
#include <sys/sysctl.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/resource.h>
#include <machine/fdt.h>
#include <machine/intr.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/broadcom/bcm2835/bcm2835_gpio.h>

#include "gpio_if.h"

#ifdef DEBUG
#define dprintf(fmt, args...) do { printf("%s(): ", __func__);   \
    printf(fmt,##args); } while (0)
#else
#define dprintf(fmt, args...)
#endif

#define	BCM_GPIO_PINS		54
#define	BCM_GPIO_DEFAULT_CAPS	(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT |	\
    GPIO_PIN_PULLUP | GPIO_PIN_PULLDOWN)

struct bcm_gpio_sysctl {
	struct bcm_gpio_softc	*sc;
	uint32_t		pin;
};

struct bcm_gpio_softc {
	device_t		sc_dev;
	struct mtx		sc_mtx;
	struct resource *	sc_mem_res;
	struct resource *	sc_irq_res;
	bus_space_tag_t		sc_bst;
	bus_space_handle_t	sc_bsh;
	void *			sc_intrhand;
	int			sc_gpio_npins;
	int			sc_ro_npins;
	int			sc_ro_pins[BCM_GPIO_PINS];
	struct gpio_pin		sc_gpio_pins[BCM_GPIO_PINS];
	struct bcm_gpio_sysctl	sc_sysctl[BCM_GPIO_PINS];
};

enum bcm_gpio_pud {
	BCM_GPIO_NONE,
	BCM_GPIO_PULLDOWN,
	BCM_GPIO_PULLUP,
};

#define	BCM_GPIO_LOCK(_sc)	mtx_lock(&_sc->sc_mtx)
#define	BCM_GPIO_UNLOCK(_sc)	mtx_unlock(&_sc->sc_mtx)
#define	BCM_GPIO_LOCK_ASSERT(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED)

#define	BCM_GPIO_GPFSEL(_bank)	0x00 + _bank * 4
#define	BCM_GPIO_GPSET(_bank)	0x1c + _bank * 4
#define	BCM_GPIO_GPCLR(_bank)	0x28 + _bank * 4
#define	BCM_GPIO_GPLEV(_bank)	0x34 + _bank * 4
#define	BCM_GPIO_GPPUD(_bank)	0x94
#define	BCM_GPIO_GPPUDCLK(_bank)	0x98 + _bank * 4

#define	BCM_GPIO_WRITE(_sc, _off, _val)		\
    bus_space_write_4(_sc->sc_bst, _sc->sc_bsh, _off, _val)
#define	BCM_GPIO_READ(_sc, _off)		\
    bus_space_read_4(_sc->sc_bst, _sc->sc_bsh, _off)

static int
bcm_gpio_pin_is_ro(struct bcm_gpio_softc *sc, int pin)
{
	int i;

	for (i = 0; i < sc->sc_ro_npins; i++)
		if (pin == sc->sc_ro_pins[i])
			return (1);
	return (0);
}

static uint32_t
bcm_gpio_get_function(struct bcm_gpio_softc *sc, uint32_t pin)
{
	uint32_t bank, func, offset;

	/* Five banks, 10 pins per bank, 3 bits per pin. */
	bank = pin / 10;
	offset = (pin - bank * 10) * 3;

	BCM_GPIO_LOCK(sc);
	func = (BCM_GPIO_READ(sc, BCM_GPIO_GPFSEL(bank)) >> offset) & 7;
	BCM_GPIO_UNLOCK(sc);

	return (func);
}

static void
bcm_gpio_func_str(uint32_t nfunc, char *buf, int bufsize)
{

	switch (nfunc) {
	case BCM_GPIO_INPUT:
		strncpy(buf, "input", bufsize);
		break;
	case BCM_GPIO_OUTPUT:
		strncpy(buf, "output", bufsize);
		break;
	case BCM_GPIO_ALT0:
		strncpy(buf, "alt0", bufsize);
		break;
	case BCM_GPIO_ALT1:
		strncpy(buf, "alt1", bufsize);
		break;
	case BCM_GPIO_ALT2:
		strncpy(buf, "alt2", bufsize);
		break;
	case BCM_GPIO_ALT3:
		strncpy(buf, "alt3", bufsize);
		break;
	case BCM_GPIO_ALT4:
		strncpy(buf, "alt4", bufsize);
		break;
	case BCM_GPIO_ALT5:
		strncpy(buf, "alt5", bufsize);
		break;
	default:
		strncpy(buf, "invalid", bufsize);
	}
}

static int
bcm_gpio_str_func(char *func, uint32_t *nfunc)
{

	if (strcasecmp(func, "input") == 0)
		*nfunc = BCM_GPIO_INPUT;
	else if (strcasecmp(func, "output") == 0)
		*nfunc = BCM_GPIO_OUTPUT;
	else if (strcasecmp(func, "alt0") == 0)
		*nfunc = BCM_GPIO_ALT0;
	else if (strcasecmp(func, "alt1") == 0)
		*nfunc = BCM_GPIO_ALT1;
	else if (strcasecmp(func, "alt2") == 0)
		*nfunc = BCM_GPIO_ALT2;
	else if (strcasecmp(func, "alt3") == 0)
		*nfunc = BCM_GPIO_ALT3;
	else if (strcasecmp(func, "alt4") == 0)
		*nfunc = BCM_GPIO_ALT4;
	else if (strcasecmp(func, "alt5") == 0)
		*nfunc = BCM_GPIO_ALT5;
	else
		return (-1);

	return (0);
}

static uint32_t
bcm_gpio_func_flag(uint32_t nfunc)
{

	switch (nfunc) {
	case BCM_GPIO_INPUT:
		return (GPIO_PIN_INPUT);
	case BCM_GPIO_OUTPUT:
		return (GPIO_PIN_OUTPUT);
	}
	return (0);
}

static void
bcm_gpio_set_function(struct bcm_gpio_softc *sc, uint32_t pin, uint32_t f)
{
	uint32_t bank, data, offset;

	/* Must be called with lock held. */
	BCM_GPIO_LOCK_ASSERT(sc);

	/* Five banks, 10 pins per bank, 3 bits per pin. */
	bank = pin / 10;
	offset = (pin - bank * 10) * 3;

	data = BCM_GPIO_READ(sc, BCM_GPIO_GPFSEL(bank));
	data &= ~(7 << offset);
	data |= (f << offset);
	BCM_GPIO_WRITE(sc, BCM_GPIO_GPFSEL(bank), data);
}

static void
bcm_gpio_set_pud(struct bcm_gpio_softc *sc, uint32_t pin, uint32_t state)
{
	uint32_t bank, offset;

	/* Must be called with lock held. */
	BCM_GPIO_LOCK_ASSERT(sc);

	bank = pin / 32;
	offset = pin - 32 * bank;

	BCM_GPIO_WRITE(sc, BCM_GPIO_GPPUD(0), state);
	DELAY(10);
	BCM_GPIO_WRITE(sc, BCM_GPIO_GPPUDCLK(bank), (1 << offset));
	DELAY(10);
	BCM_GPIO_WRITE(sc, BCM_GPIO_GPPUD(0), 0);
	BCM_GPIO_WRITE(sc, BCM_GPIO_GPPUDCLK(bank), 0);
}

void
bcm_gpio_set_alternate(device_t dev, uint32_t pin, uint32_t nfunc)
{
	struct bcm_gpio_softc *sc;
	int i;

	sc = device_get_softc(dev);
	BCM_GPIO_LOCK(sc);

	/* Disable pull-up or pull-down on pin. */
	bcm_gpio_set_pud(sc, pin, BCM_GPIO_NONE);

	/* And now set the pin function. */
	bcm_gpio_set_function(sc, pin, nfunc);

	/* Update the pin flags. */
	for (i = 0; i < sc->sc_gpio_npins; i++) {
		if (sc->sc_gpio_pins[i].gp_pin == pin)
			break;
	}
	if (i < sc->sc_gpio_npins)
		sc->sc_gpio_pins[i].gp_flags = bcm_gpio_func_flag(nfunc);

        BCM_GPIO_UNLOCK(sc);
}

static void
bcm_gpio_pin_configure(struct bcm_gpio_softc *sc, struct gpio_pin *pin,
    unsigned int flags)
{

	BCM_GPIO_LOCK(sc);

	/*
	 * Manage input/output.
	 */
	if (flags & (GPIO_PIN_INPUT|GPIO_PIN_OUTPUT)) {
		pin->gp_flags &= ~(GPIO_PIN_INPUT|GPIO_PIN_OUTPUT);
		if (flags & GPIO_PIN_OUTPUT) {
			pin->gp_flags |= GPIO_PIN_OUTPUT;
			bcm_gpio_set_function(sc, pin->gp_pin,
			    BCM_GPIO_OUTPUT);
		} else {
			pin->gp_flags |= GPIO_PIN_INPUT;
			bcm_gpio_set_function(sc, pin->gp_pin,
			    BCM_GPIO_INPUT);
		}
	}

	/* Manage Pull-up/pull-down. */
	pin->gp_flags &= ~(GPIO_PIN_PULLUP|GPIO_PIN_PULLDOWN);
	if (flags & (GPIO_PIN_PULLUP|GPIO_PIN_PULLDOWN)) {
		if (flags & GPIO_PIN_PULLUP) {
			pin->gp_flags |= GPIO_PIN_PULLUP;
			bcm_gpio_set_pud(sc, pin->gp_pin, BCM_GPIO_PULLUP);
		} else {
			pin->gp_flags |= GPIO_PIN_PULLDOWN;
			bcm_gpio_set_pud(sc, pin->gp_pin, BCM_GPIO_PULLDOWN);
		}
	} else 
		bcm_gpio_set_pud(sc, pin->gp_pin, BCM_GPIO_NONE);

	BCM_GPIO_UNLOCK(sc);
}

static int
bcm_gpio_pin_max(device_t dev, int *maxpin)
{

	*maxpin = BCM_GPIO_PINS - 1;
	return (0);
}

static int
bcm_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct bcm_gpio_softc *sc = device_get_softc(dev);
	int i;

	for (i = 0; i < sc->sc_gpio_npins; i++) {
		if (sc->sc_gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->sc_gpio_npins)
		return (EINVAL);

	BCM_GPIO_LOCK(sc);
	*caps = sc->sc_gpio_pins[i].gp_caps;
	BCM_GPIO_UNLOCK(sc);

	return (0);
}

static int
bcm_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct bcm_gpio_softc *sc = device_get_softc(dev);
	int i;

	for (i = 0; i < sc->sc_gpio_npins; i++) {
		if (sc->sc_gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->sc_gpio_npins)
		return (EINVAL);

	BCM_GPIO_LOCK(sc);
	*flags = sc->sc_gpio_pins[i].gp_flags;
	BCM_GPIO_UNLOCK(sc);

	return (0);
}

static int
bcm_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct bcm_gpio_softc *sc = device_get_softc(dev);
	int i;

	for (i = 0; i < sc->sc_gpio_npins; i++) {
		if (sc->sc_gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->sc_gpio_npins)
		return (EINVAL);

	BCM_GPIO_LOCK(sc);
	memcpy(name, sc->sc_gpio_pins[i].gp_name, GPIOMAXNAME);
	BCM_GPIO_UNLOCK(sc);

	return (0);
}

static int
bcm_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct bcm_gpio_softc *sc = device_get_softc(dev);
	int i;

	for (i = 0; i < sc->sc_gpio_npins; i++) {
		if (sc->sc_gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->sc_gpio_npins)
		return (EINVAL);

	/* We never touch on read-only/reserved pins. */
	if (bcm_gpio_pin_is_ro(sc, pin))
		return (EINVAL);

	/* Check for unwanted flags. */
	if ((flags & sc->sc_gpio_pins[i].gp_caps) != flags)
		return (EINVAL);

	/* Can't mix input/output together. */
	if ((flags & (GPIO_PIN_INPUT|GPIO_PIN_OUTPUT)) ==
	    (GPIO_PIN_INPUT|GPIO_PIN_OUTPUT))
		return (EINVAL);

	/* Can't mix pull-up/pull-down together. */
	if ((flags & (GPIO_PIN_PULLUP|GPIO_PIN_PULLDOWN)) ==
	    (GPIO_PIN_PULLUP|GPIO_PIN_PULLDOWN))
		return (EINVAL);

	bcm_gpio_pin_configure(sc, &sc->sc_gpio_pins[i], flags);

	return (0);
}

static int
bcm_gpio_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct bcm_gpio_softc *sc = device_get_softc(dev);
	uint32_t bank, offset;
	int i;

	for (i = 0; i < sc->sc_gpio_npins; i++) {
		if (sc->sc_gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->sc_gpio_npins)
		return (EINVAL);

	/* We never write to read-only/reserved pins. */
	if (bcm_gpio_pin_is_ro(sc, pin))
		return (EINVAL);

	bank = pin / 32;
	offset = pin - 32 * bank;

	BCM_GPIO_LOCK(sc);
	if (value)
		BCM_GPIO_WRITE(sc, BCM_GPIO_GPSET(bank), (1 << offset));
	else
		BCM_GPIO_WRITE(sc, BCM_GPIO_GPCLR(bank), (1 << offset));
	BCM_GPIO_UNLOCK(sc);

	return (0);
}

static int
bcm_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct bcm_gpio_softc *sc = device_get_softc(dev);
	uint32_t bank, offset, reg_data;
	int i;

	for (i = 0; i < sc->sc_gpio_npins; i++) {
		if (sc->sc_gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->sc_gpio_npins)
		return (EINVAL);

	bank = pin / 32;
	offset = pin - 32 * bank;

	BCM_GPIO_LOCK(sc);
	reg_data = BCM_GPIO_READ(sc, BCM_GPIO_GPLEV(bank));
	BCM_GPIO_UNLOCK(sc);
	*val = (reg_data & (1 << offset)) ? 1 : 0;

	return (0);
}

static int
bcm_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct bcm_gpio_softc *sc = device_get_softc(dev);
	uint32_t bank, data, offset;
	int i;

	for (i = 0; i < sc->sc_gpio_npins; i++) {
		if (sc->sc_gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->sc_gpio_npins)
		return (EINVAL);

	/* We never write to read-only/reserved pins. */
	if (bcm_gpio_pin_is_ro(sc, pin))
		return (EINVAL);

	bank = pin / 32;
	offset = pin - 32 * bank;

	BCM_GPIO_LOCK(sc);
	data = BCM_GPIO_READ(sc, BCM_GPIO_GPLEV(bank));
	if (data & (1 << offset))
		BCM_GPIO_WRITE(sc, BCM_GPIO_GPCLR(bank), (1 << offset));
	else
		BCM_GPIO_WRITE(sc, BCM_GPIO_GPSET(bank), (1 << offset));
	BCM_GPIO_UNLOCK(sc);

	return (0);
}

static int
bcm_gpio_get_ro_pins(struct bcm_gpio_softc *sc)
{
	int i, len;
	pcell_t pins[BCM_GPIO_PINS];
	phandle_t gpio;

	/* Find the gpio node to start. */
	gpio = ofw_bus_get_node(sc->sc_dev);

	len = OF_getproplen(gpio, "broadcom,read-only");
	if (len < 0 || len > sizeof(pins))
		return (-1);

	if (OF_getprop(gpio, "broadcom,read-only", &pins, len) < 0)
		return (-1);

	sc->sc_ro_npins = len / sizeof(pcell_t);

	device_printf(sc->sc_dev, "read-only pins: ");
	for (i = 0; i < sc->sc_ro_npins; i++) {
		sc->sc_ro_pins[i] = fdt32_to_cpu(pins[i]);
		if (i > 0)
			printf(",");
		printf("%d", sc->sc_ro_pins[i]);
	}
	if (i > 0)
		printf(".");
	printf("\n");

	return (0);
}

static int
bcm_gpio_func_proc(SYSCTL_HANDLER_ARGS)
{
	char buf[16];
	struct bcm_gpio_softc *sc;
	struct bcm_gpio_sysctl *sc_sysctl;
	uint32_t nfunc;
	int error;

	sc_sysctl = arg1;
	sc = sc_sysctl->sc;

	/* Get the current pin function. */
	nfunc = bcm_gpio_get_function(sc, sc_sysctl->pin);
	bcm_gpio_func_str(nfunc, buf, sizeof(buf));

	error = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	/* Parse the user supplied string and check for a valid pin function. */
	if (bcm_gpio_str_func(buf, &nfunc) != 0)
		return (EINVAL);

	/* Update the pin alternate function. */
	bcm_gpio_set_alternate(sc->sc_dev, sc_sysctl->pin, nfunc);

	return (0);
}

static void
bcm_gpio_sysctl_init(struct bcm_gpio_softc *sc)
{
	char pinbuf[3];
	struct bcm_gpio_sysctl *sc_sysctl;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree_node, *pin_node, *pinN_node;
	struct sysctl_oid_list *tree, *pin_tree, *pinN_tree;
	int i;

	/*
	 * Add per-pin sysctl tree/handlers.
	 */
	ctx = device_get_sysctl_ctx(sc->sc_dev);
 	tree_node = device_get_sysctl_tree(sc->sc_dev);
 	tree = SYSCTL_CHILDREN(tree_node);
	pin_node = SYSCTL_ADD_NODE(ctx, tree, OID_AUTO, "pin",
	    CTLFLAG_RW, NULL, "GPIO Pins");
	pin_tree = SYSCTL_CHILDREN(pin_node);

	for (i = 0; i < sc->sc_gpio_npins; i++) {

		snprintf(pinbuf, sizeof(pinbuf), "%d", i);
		pinN_node = SYSCTL_ADD_NODE(ctx, pin_tree, OID_AUTO, pinbuf,
		    CTLFLAG_RD, NULL, "GPIO Pin");
		pinN_tree = SYSCTL_CHILDREN(pinN_node);

		sc->sc_sysctl[i].sc = sc;
		sc_sysctl = &sc->sc_sysctl[i];
		sc_sysctl->sc = sc;
		sc_sysctl->pin = sc->sc_gpio_pins[i].gp_pin;
		SYSCTL_ADD_PROC(ctx, pinN_tree, OID_AUTO, "function",
		    CTLFLAG_RW | CTLTYPE_STRING, sc_sysctl,
		    sizeof(struct bcm_gpio_sysctl), bcm_gpio_func_proc,
		    "A", "Pin Function");
	}
}

static int
bcm_gpio_get_reserved_pins(struct bcm_gpio_softc *sc)
{
	int i, j, len, npins;
	pcell_t pins[BCM_GPIO_PINS];
	phandle_t gpio, node, reserved;
	char name[32];

	/* Get read-only pins. */
	if (bcm_gpio_get_ro_pins(sc) != 0)
		return (-1);

	/* Find the gpio/reserved pins node to start. */
	gpio = ofw_bus_get_node(sc->sc_dev);
	node = OF_child(gpio);
	
	/*
	 * Find reserved node
	 */
	reserved = 0;
	while ((node != 0) && (reserved == 0)) {
		len = OF_getprop(node, "name", name,
		    sizeof(name) - 1);
		name[len] = 0;
		if (strcmp(name, "reserved") == 0)
			reserved = node;
		node = OF_peer(node);
	}

	if (reserved == 0)
		return (-1);

	/* Get the reserved pins. */
	len = OF_getproplen(reserved, "broadcom,pins");
	if (len < 0 || len > sizeof(pins))
		return (-1);

	if (OF_getprop(reserved, "broadcom,pins", &pins, len) < 0)
		return (-1);

	npins = len / sizeof(pcell_t);

	j = 0;
	device_printf(sc->sc_dev, "reserved pins: ");
	for (i = 0; i < npins; i++) {
		if (i > 0)
			printf(",");
		printf("%d", fdt32_to_cpu(pins[i]));
		/* Some pins maybe already on the list of read-only pins. */
		if (bcm_gpio_pin_is_ro(sc, fdt32_to_cpu(pins[i])))
			continue;
		sc->sc_ro_pins[j++ + sc->sc_ro_npins] = fdt32_to_cpu(pins[i]);
	}
	sc->sc_ro_npins += j;
	if (i > 0)
		printf(".");
	printf("\n");

	return (0);
}

static int
bcm_gpio_probe(device_t dev)
{
	if (!ofw_bus_is_compatible(dev, "broadcom,bcm2835-gpio"))
		return (ENXIO);

	device_set_desc(dev, "BCM2708/2835 GPIO controller");
	return (BUS_PROBE_DEFAULT);
}

static int
bcm_gpio_attach(device_t dev)
{
	struct bcm_gpio_softc *sc = device_get_softc(dev);
	uint32_t func;
	int i, j, rid;
	phandle_t gpio;

	sc->sc_dev = dev;

	mtx_init(&sc->sc_mtx, "bcm gpio", "gpio", MTX_DEF);

	rid = 0;
	sc->sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (!sc->sc_mem_res) {
		device_printf(dev, "cannot allocate memory window\n");
		return (ENXIO);
	}

	sc->sc_bst = rman_get_bustag(sc->sc_mem_res);
	sc->sc_bsh = rman_get_bushandle(sc->sc_mem_res);

	rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (!sc->sc_irq_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);
		device_printf(dev, "cannot allocate interrupt\n");
		return (ENXIO);
	}

	/* Find our node. */
	gpio = ofw_bus_get_node(sc->sc_dev);

	if (!OF_hasprop(gpio, "gpio-controller"))
		/* Node is not a GPIO controller. */
		goto fail;

	/*
	 * Find the read-only pins.  These are pins we never touch or bad
	 * things could happen.
	 */
	if (bcm_gpio_get_reserved_pins(sc) == -1)
		goto fail;

	/* Initialize the software controlled pins. */
	for (i = 0, j = 0; j < BCM_GPIO_PINS; j++) {
		if (bcm_gpio_pin_is_ro(sc, j))
			continue;
		snprintf(sc->sc_gpio_pins[i].gp_name, GPIOMAXNAME,
		    "pin %d", j);
		func = bcm_gpio_get_function(sc, j);
		sc->sc_gpio_pins[i].gp_pin = j;
		sc->sc_gpio_pins[i].gp_caps = BCM_GPIO_DEFAULT_CAPS;
		sc->sc_gpio_pins[i].gp_flags = bcm_gpio_func_flag(func);
		i++;
	}
	sc->sc_gpio_npins = i;

	bcm_gpio_sysctl_init(sc);

	device_add_child(dev, "gpioc", device_get_unit(dev));
	device_add_child(dev, "gpiobus", device_get_unit(dev));
	return (bus_generic_attach(dev));

fail:
	if (sc->sc_irq_res)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq_res);
	if (sc->sc_mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);
	return (ENXIO);
}

static int
bcm_gpio_detach(device_t dev)
{

	return (EBUSY);
}

static device_method_t bcm_gpio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bcm_gpio_probe),
	DEVMETHOD(device_attach,	bcm_gpio_attach),
	DEVMETHOD(device_detach,	bcm_gpio_detach),

	/* GPIO protocol */
	DEVMETHOD(gpio_pin_max,		bcm_gpio_pin_max),
	DEVMETHOD(gpio_pin_getname,	bcm_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getflags,	bcm_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_getcaps,	bcm_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_setflags,	bcm_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_get,		bcm_gpio_pin_get),
	DEVMETHOD(gpio_pin_set,		bcm_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle,	bcm_gpio_pin_toggle),

	DEVMETHOD_END
};

static devclass_t bcm_gpio_devclass;

static driver_t bcm_gpio_driver = {
	"gpio",
	bcm_gpio_methods,
	sizeof(struct bcm_gpio_softc),
};

DRIVER_MODULE(bcm_gpio, simplebus, bcm_gpio_driver, bcm_gpio_devclass, 0, 0);
