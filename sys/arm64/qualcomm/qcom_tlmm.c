/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 UOS Project Contributors
 * Derived from OpenBSD qcgpio.c (ISC licence)
 *
 * Qualcomm Snapdragon TLMM (Top Level Mode Multiplexer) GPIO/Pinctrl Driver
 *
 * Supports: SM8450 (SD 8 Gen 1), SM8550 (SD 8 Gen 2), SM8650 (SD 8 Gen 3)
 *           SC8280XP (SD 8cx Gen 3), SM7450, SM6375
 *
 * Compatible strings:
 *   qcom,sm8450-tlmm, qcom,sm8550-tlmm, qcom,sm8650-tlmm
 *   qcom,sc8280xp-tlmm, qcom,sm7450-tlmm
 *
 * The TLMM block controls:
 *   - Pin direction (input/output)
 *   - Pin function mux (up to 16 functions per pin)
 *   - Pull-up/down configuration
 *   - Drive strength (2mA..16mA in 2mA steps)
 *   - Output value
 *   - Input value readback
 *
 * Register layout per pin:
 *   TLMM_GPIO_CFG(n)      = base + n*0x1000 + 0x00
 *   TLMM_GPIO_IN_OUT(n)   = base + n*0x1000 + 0x04
 *   TLMM_GPIO_INTR_CFG(n) = base + n*0x1000 + 0x08
 *   TLMM_GPIO_INTR_STATUS(n) = base + n*0x1000 + 0x10
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/gpio.h>
#include <sys/interrupt.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/intr.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/gpio/gpiobusvar.h>

/* TLMM per-pin register offsets (stride 0x1000 per pin) */
#define TLMM_PIN_STRIDE		0x1000
#define TLMM_GPIO_CFG_OFF	0x000
#define TLMM_GPIO_IN_OUT_OFF	0x004
#define TLMM_GPIO_INTR_CFG_OFF	0x008
#define TLMM_GPIO_INTR_ST_OFF	0x010

/* CFG register fields */
#define TLMM_GPIO_CFG_PULL_MASK		0x3
#define TLMM_GPIO_CFG_PULL_NONE		0x0
#define TLMM_GPIO_CFG_PULL_DOWN		0x1
#define TLMM_GPIO_CFG_PULL_KEEPER	0x2
#define TLMM_GPIO_CFG_PULL_UP		0x3
#define TLMM_GPIO_CFG_FUNC_SHIFT	2
#define TLMM_GPIO_CFG_FUNC_MASK		(0xF << 2)	/* bits [5:2] */
#define TLMM_GPIO_CFG_DRV_SHIFT		6
#define TLMM_GPIO_CFG_DRV_MASK		(0x7 << 6)	/* bits [8:6]: 0=2mA..7=16mA */
#define TLMM_GPIO_CFG_OE		(1 << 9)	/* output enable */

/* IN_OUT register fields */
#define TLMM_GPIO_IN			(1 << 0)	/* input value */
#define TLMM_GPIO_OUT			(1 << 1)	/* output value */

/* SM8x50/SM8550 specific pin counts */
#define SM8450_GPIO_NUM		211
#define SM8550_GPIO_NUM		211
#define SM8650_GPIO_NUM		211
#define SC8280XP_GPIO_NUM	230
#define SM7450_GPIO_NUM		175

struct qcom_tlmm_softc {
	device_t	 dev;
	device_t	 busdev;
	struct resource	*mem_res;
	struct mtx	 mtx;
	uint32_t	 npins;
};

#define TLMM_LOCK(sc)		mtx_lock(&(sc)->mtx)
#define TLMM_UNLOCK(sc)		mtx_unlock(&(sc)->mtx)

/* Register access helpers */
static inline uint32_t
tlmm_read_cfg(struct qcom_tlmm_softc *sc, uint32_t pin)
{
	return bus_read_4(sc->mem_res,
	    (bus_size_t)pin * TLMM_PIN_STRIDE + TLMM_GPIO_CFG_OFF);
}

static inline void
tlmm_write_cfg(struct qcom_tlmm_softc *sc, uint32_t pin, uint32_t val)
{
	bus_write_4(sc->mem_res,
	    (bus_size_t)pin * TLMM_PIN_STRIDE + TLMM_GPIO_CFG_OFF, val);
}

static inline uint32_t
tlmm_read_in_out(struct qcom_tlmm_softc *sc, uint32_t pin)
{
	return bus_read_4(sc->mem_res,
	    (bus_size_t)pin * TLMM_PIN_STRIDE + TLMM_GPIO_IN_OUT_OFF);
}

static inline void
tlmm_write_in_out(struct qcom_tlmm_softc *sc, uint32_t pin, uint32_t val)
{
	bus_write_4(sc->mem_res,
	    (bus_size_t)pin * TLMM_PIN_STRIDE + TLMM_GPIO_IN_OUT_OFF, val);
}

/* ---------- GPIO bus interface ---------- */

static int
qcom_tlmm_pin_max(device_t dev, int *maxpin)
{
	struct qcom_tlmm_softc *sc = device_get_softc(dev);
	*maxpin = (int)sc->npins - 1;
	return (0);
}

static int
qcom_tlmm_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct qcom_tlmm_softc *sc = device_get_softc(dev);
	if (pin >= sc->npins)
		return (EINVAL);
	snprintf(name, GPIOMAXNAME, "GPIO%u", pin);
	return (0);
}

static int
qcom_tlmm_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct qcom_tlmm_softc *sc = device_get_softc(dev);
	if (pin >= sc->npins)
		return (EINVAL);
	*caps = GPIO_PIN_INPUT | GPIO_PIN_OUTPUT |
	    GPIO_PIN_PULLUP | GPIO_PIN_PULLDOWN;
	return (0);
}

static int
qcom_tlmm_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct qcom_tlmm_softc *sc = device_get_softc(dev);
	uint32_t cfg, pull;

	if (pin >= sc->npins)
		return (EINVAL);

	TLMM_LOCK(sc);
	cfg = tlmm_read_cfg(sc, pin);
	TLMM_UNLOCK(sc);

	*flags = (cfg & TLMM_GPIO_CFG_OE) ? GPIO_PIN_OUTPUT : GPIO_PIN_INPUT;

	pull = cfg & TLMM_GPIO_CFG_PULL_MASK;
	if (pull == TLMM_GPIO_CFG_PULL_UP)
		*flags |= GPIO_PIN_PULLUP;
	else if (pull == TLMM_GPIO_CFG_PULL_DOWN)
		*flags |= GPIO_PIN_PULLDOWN;

	return (0);
}

static int
qcom_tlmm_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct qcom_tlmm_softc *sc = device_get_softc(dev);
	uint32_t cfg;

	if (pin >= sc->npins)
		return (EINVAL);

	TLMM_LOCK(sc);
	cfg = tlmm_read_cfg(sc, pin);

	/* Direction */
	if (flags & GPIO_PIN_OUTPUT)
		cfg |= TLMM_GPIO_CFG_OE;
	else
		cfg &= ~TLMM_GPIO_CFG_OE;

	/* Pull */
	cfg &= ~TLMM_GPIO_CFG_PULL_MASK;
	if (flags & GPIO_PIN_PULLUP)
		cfg |= TLMM_GPIO_CFG_PULL_UP;
	else if (flags & GPIO_PIN_PULLDOWN)
		cfg |= TLMM_GPIO_CFG_PULL_DOWN;

	tlmm_write_cfg(sc, pin, cfg);
	TLMM_UNLOCK(sc);
	return (0);
}

static int
qcom_tlmm_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct qcom_tlmm_softc *sc = device_get_softc(dev);
	uint32_t in_out;

	if (pin >= sc->npins)
		return (EINVAL);

	TLMM_LOCK(sc);
	in_out = tlmm_read_in_out(sc, pin);
	TLMM_UNLOCK(sc);

	*val = (in_out & TLMM_GPIO_IN) ? 1 : 0;
	return (0);
}

static int
qcom_tlmm_pin_set(device_t dev, uint32_t pin, unsigned int val)
{
	struct qcom_tlmm_softc *sc = device_get_softc(dev);
	uint32_t in_out;

	if (pin >= sc->npins)
		return (EINVAL);

	TLMM_LOCK(sc);
	in_out = tlmm_read_in_out(sc, pin);
	if (val)
		in_out |= TLMM_GPIO_OUT;
	else
		in_out &= ~TLMM_GPIO_OUT;
	tlmm_write_in_out(sc, pin, in_out);
	TLMM_UNLOCK(sc);
	return (0);
}

static int
qcom_tlmm_pin_toggle(device_t dev, uint32_t pin)
{
	struct qcom_tlmm_softc *sc = device_get_softc(dev);
	uint32_t in_out;

	if (pin >= sc->npins)
		return (EINVAL);

	TLMM_LOCK(sc);
	in_out = tlmm_read_in_out(sc, pin);
	in_out ^= TLMM_GPIO_OUT;
	tlmm_write_in_out(sc, pin, in_out);
	TLMM_UNLOCK(sc);
	return (0);
}

/* Set pin mux function (0 = GPIO, 1-15 = peripheral functions) */
static int
qcom_tlmm_pinmux_set(device_t dev, uint32_t pin, uint32_t func)
{
	struct qcom_tlmm_softc *sc = device_get_softc(dev);
	uint32_t cfg;

	if (pin >= sc->npins || func > 15)
		return (EINVAL);

	TLMM_LOCK(sc);
	cfg = tlmm_read_cfg(sc, pin);
	cfg &= ~TLMM_GPIO_CFG_FUNC_MASK;
	cfg |= (func << TLMM_GPIO_CFG_FUNC_SHIFT) & TLMM_GPIO_CFG_FUNC_MASK;
	tlmm_write_cfg(sc, pin, cfg);
	TLMM_UNLOCK(sc);
	return (0);
}

/* ---------- Device probe/attach ---------- */

static struct ofw_compat_data qcom_tlmm_compat[] = {
	/* Latest Snapdragon mobile SoCs */
	{ "qcom,sm8650-tlmm",	SM8650_GPIO_NUM },	/* SD 8 Gen 3 */
	{ "qcom,sm8550-tlmm",	SM8550_GPIO_NUM },	/* SD 8 Gen 2 */
	{ "qcom,sm8450-tlmm",	SM8450_GPIO_NUM },	/* SD 8 Gen 1 */
	/* Compute/laptop SoCs */
	{ "qcom,sc8280xp-tlmm",	SC8280XP_GPIO_NUM },	/* SD 8cx Gen 3 */
	/* Mid-range */
	{ "qcom,sm7450-tlmm",	SM7450_GPIO_NUM },	/* SD 7s Gen 1 */
	{ NULL, 0 },
};

static int
qcom_tlmm_probe(device_t dev)
{
	const struct ofw_compat_data *cd;

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	cd = ofw_bus_search_compatible(dev, qcom_tlmm_compat);
	if (cd->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Qualcomm Snapdragon TLMM GPIO/Pinctrl");
	return (BUS_PROBE_DEFAULT);
}

static int
qcom_tlmm_attach(device_t dev)
{
	struct qcom_tlmm_softc *sc;
	const struct ofw_compat_data *cd;
	int rid;

	sc = device_get_softc(dev);
	sc->dev = dev;

	cd = ofw_bus_search_compatible(dev, qcom_tlmm_compat);
	sc->npins = (uint32_t)cd->ocd_data;

	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "Cannot allocate memory resource\n");
		return (ENXIO);
	}

	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	sc->busdev = gpiobus_attach_bus(dev);
	if (sc->busdev == NULL) {
		device_printf(dev, "Cannot attach gpiobus\n");
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->mem_res);
		mtx_destroy(&sc->mtx);
		return (ENXIO);
	}

	device_printf(dev, "Qualcomm TLMM GPIO: %u pins\n", sc->npins);
	return (0);
}

static int
qcom_tlmm_detach(device_t dev)
{
	struct qcom_tlmm_softc *sc = device_get_softc(dev);

	gpiobus_detach_bus(dev);
	if (sc->mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->mem_res);
	mtx_destroy(&sc->mtx);
	return (0);
}

static device_method_t qcom_tlmm_methods[] = {
	DEVMETHOD(device_probe,		qcom_tlmm_probe),
	DEVMETHOD(device_attach,	qcom_tlmm_attach),
	DEVMETHOD(device_detach,	qcom_tlmm_detach),

	DEVMETHOD(gpio_pin_max,		qcom_tlmm_pin_max),
	DEVMETHOD(gpio_pin_getname,	qcom_tlmm_pin_getname),
	DEVMETHOD(gpio_pin_getcaps,	qcom_tlmm_pin_getcaps),
	DEVMETHOD(gpio_pin_getflags,	qcom_tlmm_pin_getflags),
	DEVMETHOD(gpio_pin_setflags,	qcom_tlmm_pin_setflags),
	DEVMETHOD(gpio_pin_get,		qcom_tlmm_pin_get),
	DEVMETHOD(gpio_pin_set,		qcom_tlmm_pin_set),
	DEVMETHOD(gpio_pin_toggle,	qcom_tlmm_pin_toggle),

	DEVMETHOD_END
};

static driver_t qcom_tlmm_driver = {
	"gpio",
	qcom_tlmm_methods,
	sizeof(struct qcom_tlmm_softc),
};

EARLY_DRIVER_MODULE(qcom_tlmm, simplebus, qcom_tlmm_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LAST);
MODULE_VERSION(qcom_tlmm, 1);
