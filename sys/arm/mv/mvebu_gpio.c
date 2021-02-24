/*-
 * Copyright (c) 2020 Michal Meloun <mmel@FreeBSD.org>
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
 * ARMADA 8040 GPIO driver.
 */
#include "opt_platform.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/resource.h>

#include <dev/extres/syscon/syscon.h>

#include <dev/gpio/gpiobusvar.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "pic_if.h"
#include "syscon_if.h"

#define	GPIO_LOCK(_sc)		mtx_lock(&(_sc)->mtx)
#define	GPIO_UNLOCK(_sc)	mtx_unlock(&(_sc)->mtx)
#define	GPIO_LOCK_INIT(_sc)	mtx_init(&_sc->mtx, 			\
	    device_get_nameunit(_sc->dev), "mvebu_gpio", MTX_DEF)
#define	GPIO_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->mtx);
#define	GPIO_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->mtx, MA_OWNED);
#define	GPIO_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->mtx, MA_NOTOWNED);

#define	GPIO_DATA_OUT		0x00
#define	GPIO_CONTROL		0x04
#define	GPIO_BLINK_ENA		0x08
#define	GPIO_DATA_IN_POL	0x0C
#define	GPIO_DATA_IN		0x10
#define	GPIO_INT_CAUSE		0x14
#define	GPIO_INT_MASK		0x18
#define	GPIO_INT_LEVEL_MASK	0x1C
#define	GPIO_CONTROL_SET	0x28
#define	GPIO_CONTROL_CLR	0x2C
#define	GPIO_DATA_SET		0x30
#define	GPIO_DATA_CLR		0x34

#define	GPIO_BIT(_p)		((_p) % 32)
#define	GPIO_REGNUM(_p)		((_p) / 32)

#define	MV_GPIO_MAX_NIRQS	4
#define	MV_GPIO_MAX_NPINS	32

#define	RD4(sc, reg)		SYSCON_READ_4((sc)->syscon, (reg))
#define	WR4(sc, reg, val)	SYSCON_WRITE_4((sc)->syscon, (reg), (val))

struct mvebu_gpio_irqsrc {
	struct intr_irqsrc	isrc;
	u_int			irq;
	bool			is_level;
	bool			is_inverted;
};

struct mvebu_gpio_softc;
struct mvebu_gpio_irq_cookie {
	struct mvebu_gpio_softc	*sc;
	int			bank_num;
};

struct mvebu_gpio_softc {
	device_t		dev;
	device_t		busdev;
	struct mtx		mtx;
	struct syscon		*syscon;
	uint32_t		offset;
	struct resource		*irq_res[MV_GPIO_MAX_NIRQS];
	void			*irq_ih[MV_GPIO_MAX_NIRQS];
	struct mvebu_gpio_irq_cookie irq_cookies[MV_GPIO_MAX_NIRQS];
	int			gpio_npins;
	struct gpio_pin		gpio_pins[MV_GPIO_MAX_NPINS];
	struct mvebu_gpio_irqsrc *isrcs;
};

static struct ofw_compat_data compat_data[] = {
	{"marvell,armada-8k-gpio", 1},
	{NULL, 0}
};

/* --------------------------------------------------------------------------
 *
 * GPIO
 *
 */
static inline void
gpio_write(struct mvebu_gpio_softc *sc, bus_size_t reg,
    struct gpio_pin *pin, uint32_t val)
{
	uint32_t tmp;
	int bit;

	bit = GPIO_BIT(pin->gp_pin);
	tmp = 0x100 << bit;		/* mask */
	tmp |= (val & 1) << bit;	/* value */
	SYSCON_WRITE_4(sc->syscon, sc->offset + GPIO_REGNUM(pin->gp_pin) + reg,
	    tmp);
}

static inline uint32_t
gpio_read(struct mvebu_gpio_softc *sc, bus_size_t reg, struct gpio_pin *pin)
{
	int bit;
	uint32_t val;

	bit = GPIO_BIT(pin->gp_pin);
	val = SYSCON_READ_4(sc->syscon,
	    sc->offset + GPIO_REGNUM(pin->gp_pin) + reg);
	return (val >> bit) & 1;
}

static void
mvebu_gpio_pin_configure(struct mvebu_gpio_softc *sc, struct gpio_pin *pin,
    unsigned int flags)
{

	if ((flags & (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)) == 0)
		return;

	/* Manage input/output */
	pin->gp_flags &= ~(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT);
	if (flags & GPIO_PIN_OUTPUT) {
		pin->gp_flags |= GPIO_PIN_OUTPUT;
		gpio_write(sc, GPIO_CONTROL_SET, pin, 1);
	} else {
		pin->gp_flags |= GPIO_PIN_INPUT;
		gpio_write(sc, GPIO_CONTROL_CLR, pin, 1);
	}
}

static device_t
mvebu_gpio_get_bus(device_t dev)
{
	struct mvebu_gpio_softc *sc;

	sc = device_get_softc(dev);
	return (sc->busdev);
}

static int
mvebu_gpio_pin_max(device_t dev, int *maxpin)
{
	struct mvebu_gpio_softc *sc;

	sc = device_get_softc(dev);
	*maxpin = sc->gpio_npins - 1;
	return (0);
}

static int
mvebu_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct mvebu_gpio_softc *sc;

	sc = device_get_softc(dev);
	if (pin >= sc->gpio_npins)
		return (EINVAL);

	*caps = sc->gpio_pins[pin].gp_caps;

	return (0);
}

static int
mvebu_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct mvebu_gpio_softc *sc;

	sc = device_get_softc(dev);
	if (pin >= sc->gpio_npins)
		return (EINVAL);

	*flags = sc->gpio_pins[pin].gp_flags;

	return (0);
}

static int
mvebu_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct mvebu_gpio_softc *sc;

	sc = device_get_softc(dev);
	if (pin >= sc->gpio_npins)
		return (EINVAL);

	memcpy(name, sc->gpio_pins[pin].gp_name, GPIOMAXNAME);

	return (0);
}

static int
mvebu_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct mvebu_gpio_softc *sc;

	sc = device_get_softc(dev);
	if (pin >= sc->gpio_npins)
		return (EINVAL);

	mvebu_gpio_pin_configure(sc, &sc->gpio_pins[pin], flags);

	return (0);
}

static int
mvebu_gpio_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct mvebu_gpio_softc *sc;

	sc = device_get_softc(dev);
	if (pin >= sc->gpio_npins)
		return (EINVAL);

	if (value != 0)
		gpio_write(sc, GPIO_DATA_SET, &sc->gpio_pins[pin], 1);
	else
		gpio_write(sc, GPIO_DATA_CLR, &sc->gpio_pins[pin], 1);

	return (0);
}

static int
mvebu_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct mvebu_gpio_softc *sc;

	sc = device_get_softc(dev);
	if (pin >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	*val = gpio_read(sc, GPIO_DATA_IN, &sc->gpio_pins[pin]);
	*val ^= gpio_read(sc, GPIO_DATA_IN_POL, &sc->gpio_pins[pin]);
	GPIO_UNLOCK(sc);

	return (0);
}

static int
mvebu_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct mvebu_gpio_softc *sc;
	uint32_t val;

	sc = device_get_softc(dev);
	if (pin >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	mvebu_gpio_pin_get(sc->dev, pin, &val);
	if (val != 0)
		gpio_write(sc, GPIO_DATA_CLR, &sc->gpio_pins[pin], 1);
	else
		gpio_write(sc, GPIO_DATA_SET, &sc->gpio_pins[pin], 1);
	GPIO_UNLOCK(sc);

	return (0);
}

/* --------------------------------------------------------------------------
 *
 * Interrupts
 *
 */
static inline void
intr_modify(struct mvebu_gpio_softc *sc, bus_addr_t reg,
    struct mvebu_gpio_irqsrc *mgi, uint32_t val, uint32_t mask)
{
	int bit;

	bit = GPIO_BIT(mgi->irq);
	GPIO_LOCK(sc);
	val = SYSCON_MODIFY_4(sc->syscon,
	    sc->offset + GPIO_REGNUM(mgi->irq) + reg, val, mask);
	GPIO_UNLOCK(sc);
}

static inline void
mvebu_gpio_isrc_mask(struct mvebu_gpio_softc *sc,
     struct mvebu_gpio_irqsrc *mgi, uint32_t val)
{

	if (mgi->is_level)
		intr_modify(sc, GPIO_INT_LEVEL_MASK, mgi, val, 1);
	else
		intr_modify(sc, GPIO_INT_MASK, mgi, val, 1);
}

static inline void
mvebu_gpio_isrc_eoi(struct mvebu_gpio_softc *sc,
     struct mvebu_gpio_irqsrc *mgi)
{

	if (!mgi->is_level)
		intr_modify(sc, GPIO_INT_CAUSE, mgi, 0, 1);
}

static int
mvebu_gpio_pic_attach(struct mvebu_gpio_softc *sc)
{
	int rv;
	uint32_t irq;
	const char *name;

	sc->isrcs = malloc(sizeof(*sc->isrcs) * sc->gpio_npins, M_DEVBUF,
	    M_WAITOK | M_ZERO);

	name = device_get_nameunit(sc->dev);
	for (irq = 0; irq < sc->gpio_npins; irq++) {
		sc->isrcs[irq].irq = irq;
		sc->isrcs[irq].is_level = false;
		sc->isrcs[irq].is_inverted = false;
		rv = intr_isrc_register(&sc->isrcs[irq].isrc,
		    sc->dev, 0, "%s,%u", name, irq);
		if (rv != 0)
			return (rv); /* XXX deregister ISRCs */
	}
	if (intr_pic_register(sc->dev,
	    OF_xref_from_node(ofw_bus_get_node(sc->dev))) == NULL)
		return (ENXIO);

	return (0);
}

static int
mvebu_gpio_pic_detach(struct mvebu_gpio_softc *sc)
{

	/*
	 *  There has not been established any procedure yet
	 *  how to detach PIC from living system correctly.
	 */
	device_printf(sc->dev, "%s: not implemented yet\n", __func__);
	return (EBUSY);
}

static void
mvebu_gpio_pic_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct mvebu_gpio_softc *sc;
	struct mvebu_gpio_irqsrc *mgi;

	sc = device_get_softc(dev);
	mgi = (struct mvebu_gpio_irqsrc *)isrc;
	mvebu_gpio_isrc_mask(sc, mgi, 0);
}

static void
mvebu_gpio_pic_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct mvebu_gpio_softc *sc;
	struct mvebu_gpio_irqsrc *mgi;

	sc = device_get_softc(dev);
	mgi = (struct mvebu_gpio_irqsrc *)isrc;
	mvebu_gpio_isrc_mask(sc, mgi, 1);
}

static int
mvebu_gpio_pic_map_fdt(struct mvebu_gpio_softc *sc, u_int ncells,
    pcell_t *cells, u_int *irqp, bool *invertedp, bool *levelp)
{
	bool inverted, level;

	/*
	 * The first cell is the interrupt number.
	 * The second cell is used to specify flags:
	 *	bits[3:0] trigger type and level flags:
	 *		1 = low-to-high edge triggered.
	 *		2 = high-to-low edge triggered.
	 *		4 = active high level-sensitive.
	 *		8 = active low level-sensitive.
	 */
	if (ncells != 2 || cells[0] >= sc->gpio_npins)
		return (EINVAL);

	switch (cells[1]) {
	case 1:
		inverted  = false;
		level  = false;
		break;
	case 2:
		inverted  = true;
		level  = false;
		break;
	case 4:
		inverted  = false;
		level  = true;
		break;
	case 8:
		inverted  = true;
		level  = true;
		break;
	default:
		return (EINVAL);
	}
	*irqp = cells[0];
	if (invertedp != NULL)
		*invertedp = inverted;
	if (levelp != NULL)
		*levelp = level;
	return (0);
}

static int
mvebu_gpio_pic_map_gpio(struct mvebu_gpio_softc *sc, u_int gpio_pin_num,
    u_int gpio_pin_flags, u_int intr_mode, u_int *irqp, bool *invertedp,
    bool *levelp)
{
	bool inverted, level;

	if (gpio_pin_num >= sc->gpio_npins)
		return (EINVAL);

	switch (intr_mode) {
	case GPIO_INTR_LEVEL_LOW:
		inverted  = true;
		level = true;
		break;
	case GPIO_INTR_LEVEL_HIGH:
		inverted  = false;
		level = true;
		break;
	case GPIO_INTR_CONFORM:
	case GPIO_INTR_EDGE_RISING:
		inverted  = false;
		level = false;
		break;
	case GPIO_INTR_EDGE_FALLING:
		inverted  = true;
		level = false;
		break;
	default:
		return (EINVAL);
	}
	*irqp = gpio_pin_num;
	if (invertedp != NULL)
		*invertedp = inverted;
	if (levelp != NULL)
		*levelp = level;
	return (0);
}

static int
mvebu_gpio_pic_map_intr(device_t dev, struct intr_map_data *data,
    struct intr_irqsrc **isrcp)
{
	int rv;
	u_int irq;
	struct mvebu_gpio_softc *sc;

	sc = device_get_softc(dev);

	if (data->type == INTR_MAP_DATA_FDT) {
		struct intr_map_data_fdt *daf;

		daf = (struct intr_map_data_fdt *)data;
		rv = mvebu_gpio_pic_map_fdt(sc, daf->ncells, daf->cells, &irq,
		    NULL, NULL);
	} else if (data->type == INTR_MAP_DATA_GPIO) {
		struct intr_map_data_gpio *dag;

		dag = (struct intr_map_data_gpio *)data;
		rv = mvebu_gpio_pic_map_gpio(sc, dag->gpio_pin_num,
		   dag->gpio_pin_flags, dag->gpio_intr_mode, &irq, NULL, NULL);
	} else
		return (ENOTSUP);

	if (rv == 0)
		*isrcp = &sc->isrcs[irq].isrc;
	return (rv);
}

static void
mvebu_gpio_pic_post_filter(device_t dev, struct intr_irqsrc *isrc)
{
	struct mvebu_gpio_softc *sc;
	struct mvebu_gpio_irqsrc *mgi;

	sc = device_get_softc(dev);
	mgi = (struct mvebu_gpio_irqsrc *)isrc;
	if (mgi->is_level)
		mvebu_gpio_isrc_eoi(sc, mgi);
}

static void
mvebu_gpio_pic_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct mvebu_gpio_softc *sc;
	struct mvebu_gpio_irqsrc *mgi;

	sc = device_get_softc(dev);
	mgi = (struct mvebu_gpio_irqsrc *)isrc;
	mvebu_gpio_isrc_mask(sc, mgi, 1);
}

static void
mvebu_gpio_pic_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct mvebu_gpio_softc *sc;
	struct mvebu_gpio_irqsrc *mgi;

	sc = device_get_softc(dev);
	mgi = (struct mvebu_gpio_irqsrc *)isrc;

	mvebu_gpio_isrc_mask(sc, mgi, 0);
	if (mgi->is_level)
		mvebu_gpio_isrc_eoi(sc, mgi);
}

static int
mvebu_gpio_pic_setup_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	u_int irq;
	bool inverted, level;
	int rv;
	struct mvebu_gpio_softc *sc;
	struct mvebu_gpio_irqsrc *mgi;

	sc = device_get_softc(dev);
	mgi = (struct mvebu_gpio_irqsrc *)isrc;

	if (data == NULL)
		return (ENOTSUP);

	/* Get and check config for an interrupt. */
	if (data->type == INTR_MAP_DATA_FDT) {
		struct intr_map_data_fdt *daf;

		daf = (struct intr_map_data_fdt *)data;
		rv = mvebu_gpio_pic_map_fdt(sc, daf->ncells, daf->cells, &irq,
		    &inverted, &level);
	} else if (data->type == INTR_MAP_DATA_GPIO) {
		struct intr_map_data_gpio *dag;

		dag = (struct intr_map_data_gpio *)data;
		rv = mvebu_gpio_pic_map_gpio(sc, dag->gpio_pin_num,
		   dag->gpio_pin_flags, dag->gpio_intr_mode, &irq,
		   &inverted, &level);
	} else
		return (ENOTSUP);

	if (rv != 0)
		return (EINVAL);

	/*
	 * If this is a setup for another handler,
	 * only check that its configuration match.
	 */
	if (isrc->isrc_handlers != 0)
		return (
		    mgi->is_level == level && mgi->is_inverted == inverted ?
		    0 : EINVAL);

	mgi->is_level = level;
	mgi->is_inverted = inverted;
	intr_modify(sc, GPIO_DATA_IN_POL, mgi, inverted ? 1 : 0, 1);
	mvebu_gpio_pic_enable_intr(dev, isrc);

	return (0);
}

static int
mvebu_gpio_pic_teardown_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct mvebu_gpio_softc *sc;
	struct mvebu_gpio_irqsrc *mgi;

	sc = device_get_softc(dev);
	mgi = (struct mvebu_gpio_irqsrc *)isrc;

	if (isrc->isrc_handlers == 0)
		mvebu_gpio_isrc_mask(sc, mgi, 0);
	return (0);
}

/* --------------------------------------------------------------------------
 *
 * Bus
 *
 */

static int
mvebu_gpio_intr(void *arg)
{
	u_int i, lvl, edge;
	struct mvebu_gpio_softc *sc;
	struct trapframe *tf;
	struct mvebu_gpio_irqsrc *mgi;
	struct mvebu_gpio_irq_cookie *cookie;

	cookie = (struct mvebu_gpio_irq_cookie *)arg;
	sc = cookie->sc;
	tf = curthread->td_intr_frame;

	for (i = 0; i < sc->gpio_npins; i++) {
		lvl = gpio_read(sc, GPIO_DATA_IN, &sc->gpio_pins[i]);
		lvl &= gpio_read(sc, GPIO_INT_LEVEL_MASK, &sc->gpio_pins[i]);
		edge = gpio_read(sc, GPIO_DATA_IN, &sc->gpio_pins[i]);
		edge &= gpio_read(sc, GPIO_INT_LEVEL_MASK, &sc->gpio_pins[i]);
		if (edge == 0  || lvl == 0)
			continue;

		mgi = &sc->isrcs[i];
		if (!mgi->is_level)
			mvebu_gpio_isrc_eoi(sc, mgi);
		if (intr_isrc_dispatch(&mgi->isrc, tf) != 0) {
			mvebu_gpio_isrc_mask(sc, mgi, 0);
			if (mgi->is_level)
				mvebu_gpio_isrc_eoi(sc, mgi);
			device_printf(sc->dev,
			    "Stray irq %u disabled\n", mgi->irq);
		}
	}
	return (FILTER_HANDLED);
}

static int
mvebu_gpio_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Marvell Integrated GPIO Controller");
	return (0);
}

static int
mvebu_gpio_detach(device_t dev)
{
	struct mvebu_gpio_softc *sc;
	int i;

	sc = device_get_softc(dev);

	KASSERT(mtx_initialized(&sc->mtx), ("gpio mutex not initialized"));

	for (i = 0; i < MV_GPIO_MAX_NIRQS; i++) {
		if (sc->irq_ih[i] != NULL)
			bus_teardown_intr(dev, sc->irq_res[i], sc->irq_ih[i]);
	}

	if (sc->isrcs != NULL)
		mvebu_gpio_pic_detach(sc);

	gpiobus_detach_bus(dev);

	for (i = 0; i < MV_GPIO_MAX_NIRQS; i++) {
		if (sc->irq_res[i] != NULL)
			bus_release_resource(dev, SYS_RES_IRQ, 0,
			     sc->irq_res[i]);
	}
	GPIO_LOCK_DESTROY(sc);

	return(0);
}

static int
mvebu_gpio_attach(device_t dev)
{
	struct mvebu_gpio_softc *sc;
	phandle_t node;
	struct gpio_pin *pin;
	pcell_t pincnt;
	int i, rv, rid;

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(dev);

	GPIO_LOCK_INIT(sc);

	pincnt = 0;
	rv = OF_getencprop(node, "ngpios", &pincnt, sizeof(pcell_t));
	if (rv < 0) {
		device_printf(dev,
		    "ERROR: no pin-count or ngpios entry found!\n");
		return (ENXIO);
	}

	sc->gpio_npins = MIN(pincnt, MV_GPIO_MAX_NPINS);
	if (bootverbose)
		device_printf(dev,
		    "%d pins available\n", sc->gpio_npins);

	rv = OF_getencprop(node, "offset", &sc->offset, sizeof(sc->offset));
	if (rv == -1) {
		device_printf(dev, "ERROR: no 'offset' property found!\n");
		return (ENXIO);
	}

	if (SYSCON_GET_HANDLE(sc->dev, &sc->syscon) != 0 ||
	    sc->syscon == NULL) {
		device_printf(dev, "ERROR: cannot get syscon handle!\n");
		return (ENXIO);
	}

	/* Allocate interrupts. */
	for (i = 0; i < MV_GPIO_MAX_NIRQS; i++) {
		sc->irq_cookies[i].sc = sc;
		sc->irq_cookies[i].bank_num = i;
		rid = i;
		sc->irq_res[i] = bus_alloc_resource_any(dev, SYS_RES_IRQ,
		    &rid, RF_ACTIVE);
		if (sc->irq_res[i] == NULL)
			break;
		if ((bus_setup_intr(dev, sc->irq_res[i],
		    INTR_TYPE_MISC | INTR_MPSAFE, mvebu_gpio_intr, NULL,
		    &sc->irq_cookies[i], &sc->irq_ih[i]))) {
			device_printf(dev,
			    "WARNING: unable to register interrupt handler\n");
			mvebu_gpio_detach(dev);
			return (ENXIO);
		}
	}

	/* Init GPIO pins */
	for (i = 0; i < sc->gpio_npins; i++) {
		pin = sc->gpio_pins + i;
		pin->gp_pin = i;
		if (sc->irq_res[0] != NULL)
			pin->gp_caps = GPIO_PIN_INPUT | GPIO_PIN_OUTPUT |
			    GPIO_INTR_LEVEL_LOW | GPIO_INTR_LEVEL_HIGH |
			    GPIO_INTR_EDGE_RISING | GPIO_INTR_EDGE_FALLING;
		else
			pin->gp_caps = GPIO_PIN_INPUT | GPIO_PIN_OUTPUT;
		pin->gp_flags =
		    gpio_read(sc, GPIO_CONTROL, &sc->gpio_pins[i]) != 0 ?
		    GPIO_PIN_OUTPUT : GPIO_PIN_INPUT;
		snprintf(pin->gp_name, GPIOMAXNAME, "gpio%d", i);

		/* Init HW */
		gpio_write(sc, GPIO_INT_MASK, pin, 0);
		gpio_write(sc, GPIO_INT_LEVEL_MASK, pin, 0);
		gpio_write(sc, GPIO_INT_CAUSE, pin, 0);
		gpio_write(sc, GPIO_DATA_IN_POL, pin, 1);
		gpio_write(sc, GPIO_BLINK_ENA, pin, 0);
	}

	if (sc->irq_res[0] != NULL) {
		rv = mvebu_gpio_pic_attach(sc);
		if (rv != 0) {
			device_printf(dev, "WARNING: unable to attach PIC\n");
			mvebu_gpio_detach(dev);
			return (rv);
		}
	}

	sc->busdev = gpiobus_attach_bus(dev);
	if (sc->busdev == NULL) {
		mvebu_gpio_detach(dev);
		return (ENXIO);
	}

	return (bus_generic_attach(dev));
}

static int
mvebu_gpio_map_gpios(device_t dev, phandle_t pdev, phandle_t gparent,
    int gcells, pcell_t *gpios, uint32_t *pin, uint32_t *flags)
{

	if (gcells != 2)
		return (ERANGE);
	*pin = gpios[0];
	*flags= gpios[1];
	return (0);
}

static phandle_t
mvebu_gpio_get_node(device_t bus, device_t dev)
{

	/* We only have one child, the GPIO bus, which needs our own node. */
	return (ofw_bus_get_node(bus));
}

static device_method_t mvebu_gpio_methods[] = {
	DEVMETHOD(device_probe,		mvebu_gpio_probe),
	DEVMETHOD(device_attach,	mvebu_gpio_attach),
	DEVMETHOD(device_detach,	mvebu_gpio_detach),

	/* Interrupt controller interface */
	DEVMETHOD(pic_disable_intr,	mvebu_gpio_pic_disable_intr),
	DEVMETHOD(pic_enable_intr,	mvebu_gpio_pic_enable_intr),
	DEVMETHOD(pic_map_intr,		mvebu_gpio_pic_map_intr),
	DEVMETHOD(pic_setup_intr,	mvebu_gpio_pic_setup_intr),
	DEVMETHOD(pic_teardown_intr,	mvebu_gpio_pic_teardown_intr),
	DEVMETHOD(pic_post_filter,	mvebu_gpio_pic_post_filter),
	DEVMETHOD(pic_post_ithread,	mvebu_gpio_pic_post_ithread),
	DEVMETHOD(pic_pre_ithread,	mvebu_gpio_pic_pre_ithread),

	/* GPIO protocol */
	DEVMETHOD(gpio_get_bus,		mvebu_gpio_get_bus),
	DEVMETHOD(gpio_pin_max,		mvebu_gpio_pin_max),
	DEVMETHOD(gpio_pin_getname,	mvebu_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getflags,	mvebu_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_getcaps,	mvebu_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_setflags,	mvebu_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_get,		mvebu_gpio_pin_get),
	DEVMETHOD(gpio_pin_set,		mvebu_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle,	mvebu_gpio_pin_toggle),
	DEVMETHOD(gpio_map_gpios,	mvebu_gpio_map_gpios),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	mvebu_gpio_get_node),

	DEVMETHOD_END
};

static devclass_t mvebu_gpio_devclass;
static DEFINE_CLASS_0(gpio, mvebu_gpio_driver, mvebu_gpio_methods,
    sizeof(struct mvebu_gpio_softc));
EARLY_DRIVER_MODULE(mvebu_gpio, simplebus, mvebu_gpio_driver,
     mvebu_gpio_devclass, NULL, NULL,
     BUS_PASS_TIMER + BUS_PASS_ORDER_LAST);
