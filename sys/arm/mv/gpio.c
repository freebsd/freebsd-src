/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2006 Benno Rice.
 * Copyright (C) 2008 MARVELL INTERNATIONAL LTD.
 * Copyright (c) 2017 Semihalf.
 * All rights reserved.
 *
 * Adapted and extended for Marvell SoCs by Semihalf.
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
 * from: FreeBSD: //depot/projects/arm/src/sys/arm/xscale/pxa2x0/pxa2x0_gpio.c, rev 1
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
#include <sys/timetc.h>
#include <sys/callout.h>
#include <sys/gpio.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/mv/mvvar.h>
#include <arm/mv/mvreg.h>

#define GPIO_MAX_INTR_COUNT	8
#define GPIO_PINS_PER_REG	32

#define DEBOUNCE_CHECK_MS	1
#define DEBOUNCE_LO_HI_MS	2
#define DEBOUNCE_HI_LO_MS	2
#define DEBOUNCE_CHECK_TICKS	((hz / 1000) * DEBOUNCE_CHECK_MS)

struct mv_gpio_softc {
	struct resource	*	mem_res;
	int			mem_rid;
	struct resource	*	irq_res[GPIO_MAX_INTR_COUNT];
	int			irq_rid[GPIO_MAX_INTR_COUNT];
	void			*ih_cookie[GPIO_MAX_INTR_COUNT];
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	struct mtx		mutex;
	uint8_t			pin_num;	/* number of GPIO pins */
	uint8_t			irq_num;	/* number of real IRQs occupied by GPIO controller */
	uint8_t			use_high;

	/* Used for debouncing. */
	uint32_t		debounced_state_lo;
	uint32_t		debounced_state_hi;
	struct callout		**debounce_callouts;
	int			*debounce_counters;
};

static struct mv_gpio_softc *mv_gpio_softc = NULL;
static uint32_t	gpio_setup[MV_GPIO_MAX_NPINS];

static int	mv_gpio_probe(device_t);
static int	mv_gpio_attach(device_t);
static int	mv_gpio_intr(void *);
static int	mv_gpio_init(void);

static void	mv_gpio_double_edge_init(int pin);

static int	mv_gpio_debounce_setup(int pin);
static int	mv_gpio_debounce_init(int pin);
static void	mv_gpio_debounce_start(int pin);
static int	mv_gpio_debounce_prepare(int pin);
static void	mv_gpio_debounce(void *arg);
static void	mv_gpio_debounced_state_set(int pin, uint8_t new_state);
static uint32_t	mv_gpio_debounced_state_get(int pin);

static void	mv_gpio_exec_intr_handlers(uint32_t status, int high);
static void	mv_gpio_intr_handler(int pin);
static uint32_t	mv_gpio_reg_read(uint32_t reg);
static void	mv_gpio_reg_write(uint32_t reg, uint32_t val);
static void	mv_gpio_reg_set(uint32_t reg, uint32_t val);
static void	mv_gpio_reg_clear(uint32_t reg, uint32_t val);

static void	mv_gpio_blink(uint32_t pin, uint8_t enable);
static void	mv_gpio_polarity(uint32_t pin, uint8_t enable, uint8_t toggle);
static void	mv_gpio_level(uint32_t pin, uint8_t enable);
static void	mv_gpio_edge(uint32_t pin, uint8_t enable);
static void	mv_gpio_out_en(uint32_t pin, uint8_t enable);
static void	mv_gpio_int_ack(uint32_t pin);
static void	mv_gpio_value_set(uint32_t pin, uint8_t val);
static uint32_t	mv_gpio_value_get(uint32_t pin, uint8_t exclude_polar);

static void 	mv_gpio_intr_mask(int pin);
static void 	mv_gpio_intr_unmask(int pin);
int mv_gpio_setup_intrhandler(const char *name, driver_filter_t *filt,
    void (*hand)(void *), void *arg, int pin, int flags, void **cookiep);

int mv_gpio_configure(uint32_t pin, uint32_t flags, uint32_t mask);
void mv_gpio_out(uint32_t pin, uint8_t val, uint8_t enable);
uint8_t mv_gpio_in(uint32_t pin);

#define MV_GPIO_LOCK()		mtx_lock_spin(&mv_gpio_softc->mutex)
#define MV_GPIO_UNLOCK()	mtx_unlock_spin(&mv_gpio_softc->mutex)
#define MV_GPIO_ASSERT_LOCKED()	mtx_assert(&mv_gpio_softc->mutex, MA_OWNED)

static device_method_t mv_gpio_methods[] = {
	DEVMETHOD(device_probe,		mv_gpio_probe),
	DEVMETHOD(device_attach,	mv_gpio_attach),
	{ 0, 0 }
};

static driver_t mv_gpio_driver = {
	"gpio",
	mv_gpio_methods,
	sizeof(struct mv_gpio_softc),
};

static devclass_t mv_gpio_devclass;

DRIVER_MODULE(gpio, simplebus, mv_gpio_driver, mv_gpio_devclass, 0, 0);

typedef int (*gpios_phandler_t)(phandle_t, pcell_t *, int);

struct gpio_ctrl_entry {
	const char		*compat;
	gpios_phandler_t	handler;
};

static int mv_handle_gpios_prop(phandle_t ctrl, pcell_t *gpios, int len);
int gpio_get_config_from_dt(void);

struct gpio_ctrl_entry gpio_controllers[] = {
	{ "mrvl,gpio", &mv_handle_gpios_prop },
	{ NULL, NULL }
};

static int
mv_gpio_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "mrvl,gpio"))
		return (ENXIO);

	device_set_desc(dev, "Marvell Integrated GPIO Controller");
	return (0);
}

static int
mv_gpio_attach(device_t dev)
{
	int error, i, size;
	struct mv_gpio_softc *sc;
	uint32_t dev_id, rev_id;
	pcell_t pincnt = 0;
	pcell_t irq_cells = 0;
	phandle_t iparent;

	sc = (struct mv_gpio_softc *)device_get_softc(dev);
	if (sc == NULL)
		return (ENXIO);

	if (mv_gpio_softc != NULL)
		return (ENXIO);
	mv_gpio_softc = sc;

	/* Get chip id and revision */
	soc_id(&dev_id, &rev_id);

	if (dev_id == MV_DEV_88F5182 ||
	    dev_id == MV_DEV_88F5281 ||
	    dev_id == MV_DEV_MV78100 ||
	    dev_id == MV_DEV_MV78100_Z0 ) {
		sc->pin_num = 32;
		sc->irq_num = 4;
		sc->use_high = 0;

	} else if (dev_id == MV_DEV_88F6281 ||
	    dev_id == MV_DEV_88F6282) {
		sc->pin_num = 50;
		sc->irq_num = 7;
		sc->use_high = 1;

	} else {
		if (OF_getencprop(ofw_bus_get_node(dev), "pin-count", &pincnt,
		    sizeof(pcell_t)) >= 0 ||
		    OF_getencprop(ofw_bus_get_node(dev), "ngpios", &pincnt,
		    sizeof(pcell_t)) >= 0) {
			sc->pin_num = pincnt;
			device_printf(dev, "%d pins available\n", sc->pin_num);
		} else {
			device_printf(dev, "ERROR: no pin-count entry found!\n");
			return (ENXIO);
		}
	}

	/* Find root interrupt controller */
	iparent = ofw_bus_find_iparent(ofw_bus_get_node(dev));
	if (iparent == 0) {
		device_printf(dev, "No interrupt-parrent found. "
				"Error in DTB\n");
		return (ENXIO);
	} else {
		/* While at parent - store interrupt cells prop */
		if (OF_searchencprop(OF_node_from_xref(iparent),
		    "#interrupt-cells", &irq_cells, sizeof(irq_cells)) == -1) {
			device_printf(dev, "DTB: Missing #interrupt-cells "
			    "property in interrupt parent node\n");
			return (ENXIO);
		}
	}

	size = OF_getproplen(ofw_bus_get_node(dev), "interrupts");
	if (size != -1) {
		size = size / sizeof(pcell_t);
		size = size / irq_cells;
		sc->irq_num = size;
		device_printf(dev, "%d IRQs available\n", sc->irq_num);
	} else {
		device_printf(dev, "ERROR: no interrupts entry found!\n");
		return (ENXIO);
	}

	sc->debounce_callouts = (struct callout **)malloc(sc->pin_num *
	    sizeof(struct callout *), M_DEVBUF, M_WAITOK | M_ZERO);
	if (sc->debounce_callouts == NULL)
		return (ENOMEM);

	sc->debounce_counters = (int *)malloc(sc->pin_num * sizeof(int),
	    M_DEVBUF, M_WAITOK);
	if (sc->debounce_counters == NULL)
		return (ENOMEM);

	mtx_init(&sc->mutex, device_get_nameunit(dev), NULL, MTX_SPIN);

	sc->mem_rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->mem_rid,
		 RF_ACTIVE);

	if (!sc->mem_res) {
		mtx_destroy(&sc->mutex);
		device_printf(dev, "could not allocate memory window\n");
		return (ENXIO);
	}

	sc->bst = rman_get_bustag(sc->mem_res);
	sc->bsh = rman_get_bushandle(sc->mem_res);

	for (i = 0; i < sc->irq_num; i++) {
		sc->irq_rid[i] = i;
		sc->irq_res[i] = bus_alloc_resource_any(dev, SYS_RES_IRQ,
			&sc->irq_rid[i], RF_ACTIVE);
		if (!sc->irq_res[i]) {
			mtx_destroy(&sc->mutex);
			device_printf(dev,
			    "could not allocate gpio%d interrupt\n", i+1);
			return (ENXIO);
		}
	}

	/* Disable all interrupts */
	bus_space_write_4(sc->bst, sc->bsh, GPIO_INT_EDGE_MASK, 0);
	bus_space_write_4(sc->bst, sc->bsh, GPIO_INT_LEV_MASK, 0);

	if (sc->use_high) {
		bus_space_write_4(sc->bst, sc->bsh,
		    GPIO_HI_INT_EDGE_MASK, 0);
		bus_space_write_4(sc->bst, sc->bsh,
		    GPIO_HI_INT_LEV_MASK, 0);
		bus_space_write_4(sc->bst, sc->bsh,
		    GPIO_HI_INT_CAUSE, 0);
	}

	for (i = 0; i < sc->irq_num; i++) {
		if (bus_setup_intr(dev, sc->irq_res[i],
		    INTR_TYPE_MISC,
		    (driver_filter_t *)mv_gpio_intr, NULL,
		    sc, &sc->ih_cookie[i]) != 0) {
			mtx_destroy(&sc->mutex);
			bus_release_resource(dev, SYS_RES_IRQ,
				sc->irq_rid[i], sc->irq_res[i]);
			device_printf(dev, "could not set up intr %d\n", i);
			return (ENXIO);
		}
	}

	error = mv_gpio_init();
	if (error) {
		device_printf(dev, "WARNING: failed to initialize GPIO pins, "
		    "error = %d\n", error);
	}

	/* Clear interrupt status. */
	bus_space_write_4(sc->bst, sc->bsh, GPIO_INT_CAUSE, 0);

	return (0);
}

static int
mv_gpio_intr(void *arg)
{
	uint32_t int_cause, gpio_val;
	uint32_t int_cause_hi, gpio_val_hi;

	MV_GPIO_LOCK();

	/*
	 * According to documentation, edge sensitive interrupts are asserted
	 * when unmasked GPIO_INT_CAUSE register bits are set.
	 */
	int_cause = mv_gpio_reg_read(GPIO_INT_CAUSE);
	int_cause &= mv_gpio_reg_read(GPIO_INT_EDGE_MASK);

	/*
	 * Level sensitive interrupts are asserted when unmasked GPIO_DATA_IN
	 * register bits are set.
	 */
	gpio_val = mv_gpio_reg_read(GPIO_DATA_IN);
	gpio_val &= mv_gpio_reg_read(GPIO_INT_LEV_MASK);

	int_cause_hi = 0;
	gpio_val_hi = 0;
	if (mv_gpio_softc->use_high) {
		int_cause_hi = mv_gpio_reg_read(GPIO_HI_INT_CAUSE);
		int_cause_hi &= mv_gpio_reg_read(GPIO_HI_INT_EDGE_MASK);

		gpio_val_hi = mv_gpio_reg_read(GPIO_HI_DATA_IN);
		gpio_val_hi &= mv_gpio_reg_read(GPIO_HI_INT_LEV_MASK);
	}

	mv_gpio_exec_intr_handlers(int_cause | gpio_val, 0);

	if (mv_gpio_softc->use_high)
		mv_gpio_exec_intr_handlers(int_cause_hi | gpio_val_hi, 1);

	MV_GPIO_UNLOCK();

	return (FILTER_HANDLED);
}

/*
 * GPIO interrupt handling
 */

static struct intr_event *gpio_events[MV_GPIO_MAX_NPINS];

int
mv_gpio_setup_intrhandler(const char *name, driver_filter_t *filt,
    void (*hand)(void *), void *arg, int pin, int flags, void **cookiep)
{
	struct	intr_event *event;
	int	error;

	if (pin < 0 || pin >= mv_gpio_softc->pin_num)
		return (ENXIO);
	event = gpio_events[pin];
	if (event == NULL) {
		MV_GPIO_LOCK();
		if (gpio_setup[pin] & MV_GPIO_IN_DEBOUNCE) {
			error = mv_gpio_debounce_init(pin);
			if (error != 0) {
				MV_GPIO_UNLOCK();
				return (error);
			}
		} else if (gpio_setup[pin] & MV_GPIO_IN_IRQ_DOUBLE_EDGE)
			mv_gpio_double_edge_init(pin);
		MV_GPIO_UNLOCK();

		error = intr_event_create(&event, (void *)pin, 0, pin,
		    (void (*)(void *))mv_gpio_intr_mask,
		    (void (*)(void *))mv_gpio_intr_unmask,
		    (void (*)(void *))mv_gpio_int_ack,
		    NULL,
		    "gpio%d:", pin);
		if (error != 0)
			return (error);
		gpio_events[pin] = event;
	}

	intr_event_add_handler(event, name, filt, hand, arg,
	    intr_priority(flags), flags, cookiep);
	return (0);
}

void
mv_gpio_intr_mask(int pin)
{

	if (pin >= mv_gpio_softc->pin_num)
		return;

	MV_GPIO_LOCK();

	if (gpio_setup[pin] & (MV_GPIO_IN_IRQ_EDGE | MV_GPIO_IN_IRQ_DOUBLE_EDGE))
		mv_gpio_edge(pin, 0);
	else
		mv_gpio_level(pin, 0);

	/*
	 * The interrupt has to be acknowledged before scheduling an interrupt
	 * thread. This way we allow for interrupt source to trigger again
	 * (which can happen with shared IRQs e.g. PCI) while processing the
	 * current event.
	 */
	mv_gpio_int_ack(pin);

	MV_GPIO_UNLOCK();
}

void
mv_gpio_intr_unmask(int pin)
{

	if (pin >= mv_gpio_softc->pin_num)
		return;

	MV_GPIO_LOCK();

	if (gpio_setup[pin] & (MV_GPIO_IN_IRQ_EDGE | MV_GPIO_IN_IRQ_DOUBLE_EDGE))
		mv_gpio_edge(pin, 1);
	else
		mv_gpio_level(pin, 1);

	MV_GPIO_UNLOCK();
}

static void
mv_gpio_exec_intr_handlers(uint32_t status, int high)
{
	int i, pin;

	MV_GPIO_ASSERT_LOCKED();

	i = 0;
	while (status != 0) {
		if (status & 1) {
			pin = (high ? (i + GPIO_PINS_PER_REG) : i);
			if (gpio_setup[pin] & MV_GPIO_IN_DEBOUNCE)
				mv_gpio_debounce_start(pin);
			else if (gpio_setup[pin] & MV_GPIO_IN_IRQ_DOUBLE_EDGE) {
				mv_gpio_polarity(pin, 0, 1);
				mv_gpio_intr_handler(pin);
			} else
				mv_gpio_intr_handler(pin);
		}
		status >>= 1;
		i++;
	}
}

static void
mv_gpio_intr_handler(int pin)
{
#ifdef INTRNG
	struct intr_irqsrc isrc;

	MV_GPIO_ASSERT_LOCKED();

#ifdef INTR_SOLO
	isrc.isrc_filter = NULL;
#endif
	isrc.isrc_event = gpio_events[pin];

	if (isrc.isrc_event == NULL || TAILQ_EMPTY(&isrc.isrc_event->ie_handlers))
		return;

	intr_isrc_dispatch(&isrc, NULL);
#endif
}

int
mv_gpio_configure(uint32_t pin, uint32_t flags, uint32_t mask)
{
	int error;

	if (pin >= mv_gpio_softc->pin_num)
		return (EINVAL);

	/* check flags consistency */
	if (((flags & mask) & (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)) ==
	    (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT))
		return (EINVAL);

	if (mask & MV_GPIO_IN_DEBOUNCE) {
		error = mv_gpio_debounce_prepare(pin);
		if (error != 0)
			return (error);
	}

	MV_GPIO_LOCK();

	if (mask & MV_GPIO_OUT_BLINK)
		mv_gpio_blink(pin, flags & MV_GPIO_OUT_BLINK);
	if (mask & MV_GPIO_IN_POL_LOW)
		mv_gpio_polarity(pin, flags & MV_GPIO_IN_POL_LOW, 0);
	if (mask & MV_GPIO_IN_DEBOUNCE) {
		error = mv_gpio_debounce_setup(pin);
		if (error) {
			MV_GPIO_UNLOCK();
			return (error);
		}
	}

	gpio_setup[pin] &= ~(mask);
	gpio_setup[pin] |= (flags & mask);

	MV_GPIO_UNLOCK();

	return (0);
}

static void
mv_gpio_double_edge_init(int pin)
{
	uint8_t raw_read;

	MV_GPIO_ASSERT_LOCKED();

	raw_read = (mv_gpio_value_get(pin, 1) ? 1 : 0);

	if (raw_read)
		mv_gpio_polarity(pin, 1, 0);
	else
		mv_gpio_polarity(pin, 0, 0);
}

static int
mv_gpio_debounce_setup(int pin)
{
	struct callout *c;

	MV_GPIO_ASSERT_LOCKED();

	c = mv_gpio_softc->debounce_callouts[pin];
	if (c == NULL)
		return (ENXIO);

	if (callout_active(c))
		callout_deactivate(c);

	callout_stop(c);

	return (0);
}

static int
mv_gpio_debounce_prepare(int pin)
{
	struct callout *c;
	struct mv_gpio_softc *sc;

	sc = (struct mv_gpio_softc *)mv_gpio_softc;

	c = sc->debounce_callouts[pin];
	if (c == NULL) {
		c = (struct callout *)malloc(sizeof(struct callout),
		    M_DEVBUF, M_WAITOK);
		sc->debounce_callouts[pin] = c;
		if (c == NULL)
			return (ENOMEM);
		callout_init(c, 1);
	}

	return (0);
}

static int
mv_gpio_debounce_init(int pin)
{
	uint8_t raw_read;
	int *cnt;

	MV_GPIO_ASSERT_LOCKED();

	cnt = &mv_gpio_softc->debounce_counters[pin];

	raw_read = (mv_gpio_value_get(pin, 1) ? 1 : 0);
	if (raw_read) {
		mv_gpio_polarity(pin, 1, 0);
		*cnt = DEBOUNCE_HI_LO_MS / DEBOUNCE_CHECK_MS;
	} else {
		mv_gpio_polarity(pin, 0, 0);
		*cnt = DEBOUNCE_LO_HI_MS / DEBOUNCE_CHECK_MS;
	}

	mv_gpio_debounced_state_set(pin, raw_read);

	return (0);
}

static void
mv_gpio_debounce_start(int pin)
{
	struct callout *c;
	int *debounced_pin;

	MV_GPIO_ASSERT_LOCKED();

	c = mv_gpio_softc->debounce_callouts[pin];
	if (c == NULL) {
		mv_gpio_int_ack(pin);
		return;
	}

	if (callout_pending(c) || callout_active(c)) {
		mv_gpio_int_ack(pin);
		return;
	}

	debounced_pin = (int *)malloc(sizeof(int), M_DEVBUF,
	    M_WAITOK);
	if (debounced_pin == NULL) {
		mv_gpio_int_ack(pin);
		return;
	}
	*debounced_pin = pin;

	callout_reset(c, DEBOUNCE_CHECK_TICKS, mv_gpio_debounce,
	    debounced_pin);
}

static void
mv_gpio_debounce(void *arg)
{
	uint8_t raw_read, last_state;
	int pin;
	int *debounce_counter;

	pin = *((int *)arg);

	MV_GPIO_LOCK();

	raw_read = (mv_gpio_value_get(pin, 1) ? 1 : 0);
	last_state = (mv_gpio_debounced_state_get(pin) ? 1 : 0);
	debounce_counter = &mv_gpio_softc->debounce_counters[pin];

	if (raw_read == last_state) {
		if (last_state)
			*debounce_counter = DEBOUNCE_HI_LO_MS /
			    DEBOUNCE_CHECK_MS;
		else
			*debounce_counter = DEBOUNCE_LO_HI_MS /
			    DEBOUNCE_CHECK_MS;

		callout_reset(mv_gpio_softc->debounce_callouts[pin],
		    DEBOUNCE_CHECK_TICKS, mv_gpio_debounce, arg);
	} else {
		*debounce_counter = *debounce_counter - 1;
		if (*debounce_counter != 0)
			callout_reset(mv_gpio_softc->debounce_callouts[pin],
			    DEBOUNCE_CHECK_TICKS, mv_gpio_debounce, arg);
		else {
			mv_gpio_debounced_state_set(pin, raw_read);

			if (last_state)
				*debounce_counter = DEBOUNCE_HI_LO_MS /
				    DEBOUNCE_CHECK_MS;
			else
				*debounce_counter = DEBOUNCE_LO_HI_MS /
				    DEBOUNCE_CHECK_MS;

			if (((gpio_setup[pin] & MV_GPIO_IN_POL_LOW) &&
			    (raw_read == 0)) ||
			    (((gpio_setup[pin] & MV_GPIO_IN_POL_LOW) == 0) &&
			    raw_read) ||
			    (gpio_setup[pin] & MV_GPIO_IN_IRQ_DOUBLE_EDGE))
				mv_gpio_intr_handler(pin);

			/* Toggle polarity for next edge. */
			mv_gpio_polarity(pin, 0, 1);

			free(arg, M_DEVBUF);
			callout_deactivate(mv_gpio_softc->
			    debounce_callouts[pin]);
		}
	}

	MV_GPIO_UNLOCK();
}

static void
mv_gpio_debounced_state_set(int pin, uint8_t new_state)
{
	uint32_t *old_state;

	MV_GPIO_ASSERT_LOCKED();

	if (pin >= GPIO_PINS_PER_REG) {
		old_state = &mv_gpio_softc->debounced_state_hi;
		pin -= GPIO_PINS_PER_REG;
	} else
		old_state = &mv_gpio_softc->debounced_state_lo;

	if (new_state)
		*old_state |= (1 << pin);
	else
		*old_state &= ~(1 << pin);
}

static uint32_t
mv_gpio_debounced_state_get(int pin)
{
	uint32_t *state;

	MV_GPIO_ASSERT_LOCKED();

	if (pin >= GPIO_PINS_PER_REG) {
		state = &mv_gpio_softc->debounced_state_hi;
		pin -= GPIO_PINS_PER_REG;
	} else
		state = &mv_gpio_softc->debounced_state_lo;

	return (*state & (1 << pin));
}

void
mv_gpio_out(uint32_t pin, uint8_t val, uint8_t enable)
{

	MV_GPIO_LOCK();

	mv_gpio_value_set(pin, val);
	mv_gpio_out_en(pin, enable);

	MV_GPIO_UNLOCK();
}

uint8_t
mv_gpio_in(uint32_t pin)
{
	uint8_t state;

	MV_GPIO_LOCK();

	if (gpio_setup[pin] & MV_GPIO_IN_DEBOUNCE) {
		if (gpio_setup[pin] & MV_GPIO_IN_POL_LOW)
			state = (mv_gpio_debounced_state_get(pin) ? 0 : 1);
		else
			state = (mv_gpio_debounced_state_get(pin) ? 1 : 0);
	} else if (gpio_setup[pin] & MV_GPIO_IN_IRQ_DOUBLE_EDGE) {
		if (gpio_setup[pin] & MV_GPIO_IN_POL_LOW)
			state = (mv_gpio_value_get(pin, 1) ? 0 : 1);
		else
			state = (mv_gpio_value_get(pin, 1) ? 1 : 0);
	} else
		state = (mv_gpio_value_get(pin, 0) ? 1 : 0);

	MV_GPIO_UNLOCK();

	return (state);
}

static uint32_t
mv_gpio_reg_read(uint32_t reg)
{

	return (bus_space_read_4(mv_gpio_softc->bst,
	    mv_gpio_softc->bsh, reg));
}

static void
mv_gpio_reg_write(uint32_t reg, uint32_t val)
{

	bus_space_write_4(mv_gpio_softc->bst,
	    mv_gpio_softc->bsh, reg, val);
}

static void
mv_gpio_reg_set(uint32_t reg, uint32_t pin)
{
	uint32_t reg_val;

	reg_val = mv_gpio_reg_read(reg);
	reg_val |= GPIO(pin);
	mv_gpio_reg_write(reg, reg_val);
}

static void
mv_gpio_reg_clear(uint32_t reg, uint32_t pin)
{
	uint32_t reg_val;

	reg_val = mv_gpio_reg_read(reg);
	reg_val &= ~(GPIO(pin));
	mv_gpio_reg_write(reg, reg_val);
}

static void
mv_gpio_out_en(uint32_t pin, uint8_t enable)
{
	uint32_t reg;

	if (pin >= mv_gpio_softc->pin_num)
		return;

	if (pin >= GPIO_PINS_PER_REG) {
		reg = GPIO_HI_DATA_OUT_EN_CTRL;
		pin -= GPIO_PINS_PER_REG;
	} else
		reg = GPIO_DATA_OUT_EN_CTRL;

	if (enable)
		mv_gpio_reg_clear(reg, pin);
	else
		mv_gpio_reg_set(reg, pin);
}

static void
mv_gpio_blink(uint32_t pin, uint8_t enable)
{
	uint32_t reg;

	if (pin >= mv_gpio_softc->pin_num)
		return;

	if (pin >= GPIO_PINS_PER_REG) {
		reg = GPIO_HI_BLINK_EN;
		pin -= GPIO_PINS_PER_REG;
	} else
		reg = GPIO_BLINK_EN;

	if (enable)
		mv_gpio_reg_set(reg, pin);
	else
		mv_gpio_reg_clear(reg, pin);
}

static void
mv_gpio_polarity(uint32_t pin, uint8_t enable, uint8_t toggle)
{
	uint32_t reg, reg_val;

	if (pin >= mv_gpio_softc->pin_num)
		return;

	if (pin >= GPIO_PINS_PER_REG) {
		reg = GPIO_HI_DATA_IN_POLAR;
		pin -= GPIO_PINS_PER_REG;
	} else
		reg = GPIO_DATA_IN_POLAR;

	if (toggle) {
		reg_val = mv_gpio_reg_read(reg) & GPIO(pin);
		if (reg_val)
			mv_gpio_reg_clear(reg, pin);
		else
			mv_gpio_reg_set(reg, pin);
	} else if (enable)
		mv_gpio_reg_set(reg, pin);
	else
		mv_gpio_reg_clear(reg, pin);
}

static void
mv_gpio_level(uint32_t pin, uint8_t enable)
{
	uint32_t reg;

	if (pin >= mv_gpio_softc->pin_num)
		return;

	if (pin >= GPIO_PINS_PER_REG) {
		reg = GPIO_HI_INT_LEV_MASK;
		pin -= GPIO_PINS_PER_REG;
	} else
		reg = GPIO_INT_LEV_MASK;

	if (enable)
		mv_gpio_reg_set(reg, pin);
	else
		mv_gpio_reg_clear(reg, pin);
}

static void
mv_gpio_edge(uint32_t pin, uint8_t enable)
{
	uint32_t reg;

	if (pin >= mv_gpio_softc->pin_num)
		return;

	if (pin >= GPIO_PINS_PER_REG) {
		reg = GPIO_HI_INT_EDGE_MASK;
		pin -= GPIO_PINS_PER_REG;
	} else
		reg = GPIO_INT_EDGE_MASK;

	if (enable)
		mv_gpio_reg_set(reg, pin);
	else
		mv_gpio_reg_clear(reg, pin);
}

static void
mv_gpio_int_ack(uint32_t pin)
{
	uint32_t reg;

	if (pin >= mv_gpio_softc->pin_num)
		return;

	if (pin >= GPIO_PINS_PER_REG) {
		reg = GPIO_HI_INT_CAUSE;
		pin -= GPIO_PINS_PER_REG;
	} else
		reg = GPIO_INT_CAUSE;

	mv_gpio_reg_clear(reg, pin);
}

static uint32_t
mv_gpio_value_get(uint32_t pin, uint8_t exclude_polar)
{
	uint32_t reg, polar_reg, reg_val, polar_reg_val;

	if (pin >= mv_gpio_softc->pin_num)
		return (0);

	if (pin >= GPIO_PINS_PER_REG) {
		reg = GPIO_HI_DATA_IN;
		pin -= GPIO_PINS_PER_REG;
		polar_reg = GPIO_HI_DATA_IN_POLAR;
	} else {
		reg = GPIO_DATA_IN;
		polar_reg = GPIO_DATA_IN_POLAR;
	}

	reg_val = mv_gpio_reg_read(reg);

	if (exclude_polar) {
		polar_reg_val = mv_gpio_reg_read(polar_reg);
		return ((reg_val & GPIO(pin)) ^ (polar_reg_val & GPIO(pin)));
	} else
		return (reg_val & GPIO(pin));
}

static void
mv_gpio_value_set(uint32_t pin, uint8_t val)
{
	uint32_t reg;

	if (pin >= mv_gpio_softc->pin_num)
		return;

	if (pin >= GPIO_PINS_PER_REG) {
		reg = GPIO_HI_DATA_OUT;
		pin -= GPIO_PINS_PER_REG;
	} else
		reg = GPIO_DATA_OUT;

	if (val)
		mv_gpio_reg_set(reg, pin);
	else
		mv_gpio_reg_clear(reg, pin);
}

static int
mv_handle_gpios_prop(phandle_t ctrl, pcell_t *gpios, int len)
{
	pcell_t gpio_cells, pincnt;
	int inc, t, tuples, tuple_size;
	int dir, flags, pin;
	u_long gpio_ctrl, size;
	struct mv_gpio_softc sc;

	pincnt = 0;
	if (!OF_hasprop(ctrl, "gpio-controller"))
		/* Node is not a GPIO controller. */
		return (ENXIO);

	if (OF_getencprop(ctrl, "#gpio-cells", &gpio_cells, sizeof(pcell_t)) < 0)
		return (ENXIO);
	if (gpio_cells != 3)
		return (ENXIO);

	tuple_size = gpio_cells * sizeof(pcell_t) + sizeof(phandle_t);
	tuples = len / tuple_size;

	if (fdt_regsize(ctrl, &gpio_ctrl, &size))
		return (ENXIO);

	if (OF_getencprop(ctrl, "pin-count", &pincnt, sizeof(pcell_t)) < 0)
		return (ENXIO);
	sc.pin_num = pincnt;

	/*
	 * Skip controller reference, since controller's phandle is given
	 * explicitly (in a function argument).
	 */
	inc = sizeof(ihandle_t) / sizeof(pcell_t);
	gpios += inc;

	for (t = 0; t < tuples; t++) {
		pin = gpios[0];
		dir = gpios[1];
		flags = gpios[2];

		mv_gpio_configure(pin, flags, ~0);

		if (dir == 1)
			/* Input. */
			mv_gpio_out_en(pin, 0);
		else {
			/* Output. */
			if (flags & MV_GPIO_OUT_OPEN_DRAIN)
				mv_gpio_out(pin, 0, 1);

			if (flags & MV_GPIO_OUT_OPEN_SRC)
				mv_gpio_out(pin, 1, 1);
		}
		gpios += gpio_cells + inc;
	}

	return (0);
}

#define MAX_PINS_PER_NODE	5
#define GPIOS_PROP_CELLS	4
static int
mv_gpio_init(void)
{
	phandle_t child, parent, root, ctrl;
	pcell_t gpios[MAX_PINS_PER_NODE * GPIOS_PROP_CELLS];
	struct gpio_ctrl_entry *e;
	int len, rv;

	root = OF_finddevice("/");
	len = 0;
	parent = root;

	/* Traverse through entire tree to find nodes with 'gpios' prop */
	for (child = OF_child(parent); child != 0; child = OF_peer(child)) {

		/* Find a 'leaf'. Start the search from this node. */
		while (OF_child(child)) {
			parent = child;
			child = OF_child(child);
		}
		if ((len = OF_getproplen(child, "gpios")) > 0) {

			if (len > sizeof(gpios))
				return (ENXIO);

			/* Get 'gpios' property. */
			OF_getencprop(child, "gpios", gpios, len);

			e = (struct gpio_ctrl_entry *)&gpio_controllers;

			/* Find and call a handler. */
			for (; e->compat; e++) {
				/*
				 * First cell of 'gpios' property should
				 * contain a ref. to a node defining GPIO
				 * controller.
				 */
				ctrl = OF_node_from_xref(gpios[0]);

				if (ofw_bus_node_is_compatible(ctrl, e->compat))
					/* Call a handler. */
					if ((rv = e->handler(ctrl,
					    (pcell_t *)&gpios, len)))
						return (rv);
			}
		}

		if (OF_peer(child) == 0) {
			/* No more siblings. */
			child = parent;
			parent = OF_parent(child);
		}
	}
	return (0);
}
