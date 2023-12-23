/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013 Ganbold Tsagaankhuu <ganbold@freebsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/gpio.h>
#include <sys/proc.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/intr.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/fdt/fdt_pinctrl.h>

#include <arm/allwinner/aw_machdep.h>
#include <arm/allwinner/allwinner_pinctrl.h>
#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>
#include <dev/extres/regulator/regulator.h>

#if defined(__aarch64__)
#include "opt_soc.h"
#endif

#include "pic_if.h"
#include "gpio_if.h"

#define	AW_GPIO_DEFAULT_CAPS	(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT |	\
	  GPIO_PIN_PULLUP | GPIO_PIN_PULLDOWN);

#define	AW_GPIO_INTR_CAPS	(GPIO_INTR_LEVEL_LOW | GPIO_INTR_LEVEL_HIGH |	\
	  GPIO_INTR_EDGE_RISING | GPIO_INTR_EDGE_FALLING | GPIO_INTR_EDGE_BOTH)

#define	AW_GPIO_NONE		0
#define	AW_GPIO_PULLUP		1
#define	AW_GPIO_PULLDOWN	2

#define	AW_GPIO_INPUT		0
#define	AW_GPIO_OUTPUT		1

#define	AW_GPIO_DRV_MASK	0x3
#define	AW_GPIO_PUD_MASK	0x3

#define	AW_PINCTRL	1
#define	AW_R_PINCTRL	2

struct aw_gpio_conf {
	struct allwinner_padconf *padconf;
	const char *banks;
};

/* Defined in aw_padconf.c */
#ifdef SOC_ALLWINNER_A10
extern struct allwinner_padconf a10_padconf;
struct aw_gpio_conf a10_gpio_conf = {
	.padconf = &a10_padconf,
	.banks = "abcdefghi",
};
#endif

/* Defined in a13_padconf.c */
#ifdef SOC_ALLWINNER_A13
extern struct allwinner_padconf a13_padconf;
struct aw_gpio_conf a13_gpio_conf = {
	.padconf = &a13_padconf,
	.banks = "bcdefg",
};
#endif

/* Defined in a20_padconf.c */
#ifdef SOC_ALLWINNER_A20
extern struct allwinner_padconf a20_padconf;
struct aw_gpio_conf a20_gpio_conf = {
	.padconf = &a20_padconf,
	.banks = "abcdefghi",
};
#endif

/* Defined in a31_padconf.c */
#ifdef SOC_ALLWINNER_A31
extern struct allwinner_padconf a31_padconf;
struct aw_gpio_conf a31_gpio_conf = {
	.padconf = &a31_padconf,
	.banks = "abcdefgh",
};
#endif

/* Defined in a31s_padconf.c */
#ifdef SOC_ALLWINNER_A31S
extern struct allwinner_padconf a31s_padconf;
struct aw_gpio_conf a31s_gpio_conf = {
	.padconf = &a31s_padconf,
	.banks = "abcdefgh",
};
#endif

#if defined(SOC_ALLWINNER_A31) || defined(SOC_ALLWINNER_A31S)
extern struct allwinner_padconf a31_r_padconf;
struct aw_gpio_conf a31_r_gpio_conf = {
	.padconf = &a31_r_padconf,
	.banks = "lm",
};
#endif

/* Defined in a33_padconf.c */
#ifdef SOC_ALLWINNER_A33
extern struct allwinner_padconf a33_padconf;
struct aw_gpio_conf a33_gpio_conf = {
	.padconf = &a33_padconf,
	.banks = "bcdefgh",
};
#endif

/* Defined in h3_padconf.c */
#if defined(SOC_ALLWINNER_H3) || defined(SOC_ALLWINNER_H5)
extern struct allwinner_padconf h3_padconf;
extern struct allwinner_padconf h3_r_padconf;
struct aw_gpio_conf h3_gpio_conf = {
	.padconf = &h3_padconf,
	.banks = "acdefg",
};
struct aw_gpio_conf h3_r_gpio_conf = {
	.padconf = &h3_r_padconf,
	.banks = "l",
};
#endif

/* Defined in a83t_padconf.c */
#ifdef SOC_ALLWINNER_A83T
extern struct allwinner_padconf a83t_padconf;
extern struct allwinner_padconf a83t_r_padconf;
struct aw_gpio_conf a83t_gpio_conf = {
	.padconf = &a83t_padconf,
	.banks = "bcdefgh"
};
struct aw_gpio_conf a83t_r_gpio_conf = {
	.padconf = &a83t_r_padconf,
	.banks = "l",
};
#endif

/* Defined in a64_padconf.c */
#ifdef SOC_ALLWINNER_A64
extern struct allwinner_padconf a64_padconf;
extern struct allwinner_padconf a64_r_padconf;
struct aw_gpio_conf a64_gpio_conf = {
	.padconf = &a64_padconf,
	.banks = "bcdefgh",
};
struct aw_gpio_conf a64_r_gpio_conf = {
	.padconf = &a64_r_padconf,
	.banks = "l",
};
#endif

/* Defined in h6_padconf.c */
#ifdef SOC_ALLWINNER_H6
extern struct allwinner_padconf h6_padconf;
extern struct allwinner_padconf h6_r_padconf;
struct aw_gpio_conf h6_gpio_conf = {
	.padconf = &h6_padconf,
	.banks = "cdfgh",
};
struct aw_gpio_conf h6_r_gpio_conf = {
	.padconf = &h6_r_padconf,
	.banks = "lm",
};
#endif

static struct ofw_compat_data compat_data[] = {
#ifdef SOC_ALLWINNER_A10
	{"allwinner,sun4i-a10-pinctrl",		(uintptr_t)&a10_gpio_conf},
#endif
#ifdef SOC_ALLWINNER_A13
	{"allwinner,sun5i-a13-pinctrl",		(uintptr_t)&a13_gpio_conf},
#endif
#ifdef SOC_ALLWINNER_A20
	{"allwinner,sun7i-a20-pinctrl",		(uintptr_t)&a20_gpio_conf},
#endif
#ifdef SOC_ALLWINNER_A31
	{"allwinner,sun6i-a31-pinctrl",		(uintptr_t)&a31_gpio_conf},
#endif
#ifdef SOC_ALLWINNER_A31S
	{"allwinner,sun6i-a31s-pinctrl",	(uintptr_t)&a31s_gpio_conf},
#endif
#if defined(SOC_ALLWINNER_A31) || defined(SOC_ALLWINNER_A31S)
	{"allwinner,sun6i-a31-r-pinctrl",	(uintptr_t)&a31_r_gpio_conf},
#endif
#ifdef SOC_ALLWINNER_A33
	{"allwinner,sun6i-a33-pinctrl",		(uintptr_t)&a33_gpio_conf},
#endif
#ifdef SOC_ALLWINNER_A83T
	{"allwinner,sun8i-a83t-pinctrl",	(uintptr_t)&a83t_gpio_conf},
	{"allwinner,sun8i-a83t-r-pinctrl",	(uintptr_t)&a83t_r_gpio_conf},
#endif
#if defined(SOC_ALLWINNER_H3) || defined(SOC_ALLWINNER_H5)
	{"allwinner,sun8i-h3-pinctrl",		(uintptr_t)&h3_gpio_conf},
	{"allwinner,sun50i-h5-pinctrl",		(uintptr_t)&h3_gpio_conf},
	{"allwinner,sun8i-h3-r-pinctrl",	(uintptr_t)&h3_r_gpio_conf},
#endif
#ifdef SOC_ALLWINNER_A64
	{"allwinner,sun50i-a64-pinctrl",	(uintptr_t)&a64_gpio_conf},
	{"allwinner,sun50i-a64-r-pinctrl",	(uintptr_t)&a64_r_gpio_conf},
#endif
#ifdef SOC_ALLWINNER_H6
	{"allwinner,sun50i-h6-pinctrl",	(uintptr_t)&h6_gpio_conf},
	{"allwinner,sun50i-h6-r-pinctrl",	(uintptr_t)&h6_r_gpio_conf},
#endif
	{NULL,	0}
};

struct clk_list {
	TAILQ_ENTRY(clk_list)	next;
	clk_t			clk;
};

struct gpio_irqsrc {
	struct intr_irqsrc	isrc;
	u_int			irq;
	uint32_t		mode;
	uint32_t		pin;
	uint32_t		bank;
	uint32_t		intnum;
	uint32_t		intfunc;
	uint32_t		oldfunc;
	bool			enabled;
};

#define	AW_GPIO_MEMRES		0
#define	AW_GPIO_IRQRES		1
#define	AW_GPIO_RESSZ		2

struct aw_gpio_softc {
	device_t		sc_dev;
	device_t		sc_busdev;
	struct resource *	sc_res[AW_GPIO_RESSZ];
	struct mtx		sc_mtx;
	struct resource *	sc_mem_res;
	struct resource *	sc_irq_res;
	void *			sc_intrhand;
	struct aw_gpio_conf	*conf;
	TAILQ_HEAD(, clk_list)		clk_list;

	struct gpio_irqsrc 	*gpio_pic_irqsrc;
	int			nirqs;
};

static struct resource_spec aw_gpio_res_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE | RF_SHAREABLE },
	{ -1,			0,	0 }
};

#define	AW_GPIO_LOCK(_sc)		mtx_lock_spin(&(_sc)->sc_mtx)
#define	AW_GPIO_UNLOCK(_sc)		mtx_unlock_spin(&(_sc)->sc_mtx)
#define	AW_GPIO_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->sc_mtx, MA_OWNED)

#define	AW_GPIO_GP_CFG(_bank, _idx)	0x00 + ((_bank) * 0x24) + ((_idx) << 2)
#define	AW_GPIO_GP_DAT(_bank)		0x10 + ((_bank) * 0x24)
#define	AW_GPIO_GP_DRV(_bank, _idx)	0x14 + ((_bank) * 0x24) + ((_idx) << 2)
#define	AW_GPIO_GP_PUL(_bank, _idx)	0x1c + ((_bank) * 0x24) + ((_idx) << 2)

#define	AW_GPIO_GP_INT_BASE(_bank)	(0x200 + 0x20 * _bank)

#define	AW_GPIO_GP_INT_CFG(_bank, _pin)	(AW_GPIO_GP_INT_BASE(_bank) + (0x4 * ((_pin) / 8)))
#define	AW_GPIO_GP_INT_CTL(_bank)	(AW_GPIO_GP_INT_BASE(_bank) + 0x10)
#define	AW_GPIO_GP_INT_STA(_bank)	(AW_GPIO_GP_INT_BASE(_bank) + 0x14)
#define	AW_GPIO_GP_INT_DEB(_bank)	(AW_GPIO_GP_INT_BASE(_bank) + 0x18)

#define	AW_GPIO_INT_EDGE_POSITIVE	0x0
#define	AW_GPIO_INT_EDGE_NEGATIVE	0x1
#define	AW_GPIO_INT_LEVEL_HIGH		0x2
#define	AW_GPIO_INT_LEVEL_LOW		0x3
#define	AW_GPIO_INT_EDGE_BOTH		0x4

static char *aw_gpio_parse_function(phandle_t node);
static const char **aw_gpio_parse_pins(phandle_t node, int *pins_nb);
static uint32_t aw_gpio_parse_bias(phandle_t node);
static int aw_gpio_parse_drive_strength(phandle_t node, uint32_t *drive);

static int aw_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *value);
static int aw_gpio_pin_set(device_t dev, uint32_t pin, unsigned int value);
static int aw_gpio_pin_get_locked(struct aw_gpio_softc *sc, uint32_t pin, unsigned int *value);
static int aw_gpio_pin_set_locked(struct aw_gpio_softc *sc, uint32_t pin, unsigned int value);

static void aw_gpio_intr(void *arg);
static void aw_gpio_pic_disable_intr(device_t dev, struct intr_irqsrc *isrc);
static void aw_gpio_pic_disable_intr_locked(struct aw_gpio_softc *sc, struct intr_irqsrc *isrc);
static void aw_gpio_pic_post_filter(device_t dev, struct intr_irqsrc *isrc);
static int aw_gpio_register_isrcs(struct aw_gpio_softc *sc);

#define	AW_GPIO_WRITE(_sc, _off, _val)		\
	bus_write_4((_sc)->sc_res[AW_GPIO_MEMRES], _off, _val)
#define	AW_GPIO_READ(_sc, _off)		\
	bus_read_4((_sc)->sc_res[AW_GPIO_MEMRES], _off)

static uint32_t
aw_gpio_get_function(struct aw_gpio_softc *sc, uint32_t pin)
{
	uint32_t bank, func, offset;

	/* Must be called with lock held. */
	AW_GPIO_LOCK_ASSERT(sc);

	if (pin > sc->conf->padconf->npins)
		return (0);
	bank = sc->conf->padconf->pins[pin].port;
	pin = sc->conf->padconf->pins[pin].pin;
	offset = ((pin & 0x07) << 2);

	func = AW_GPIO_READ(sc, AW_GPIO_GP_CFG(bank, pin >> 3));

	return ((func >> offset) & 0x7);
}

static int
aw_gpio_set_function(struct aw_gpio_softc *sc, uint32_t pin, uint32_t f)
{
	uint32_t bank, data, offset;

	/* Check if the function exists in the padconf data */
	if (sc->conf->padconf->pins[pin].functions[f] == NULL)
		return (EINVAL);

	/* Must be called with lock held. */
	AW_GPIO_LOCK_ASSERT(sc);

	bank = sc->conf->padconf->pins[pin].port;
	pin = sc->conf->padconf->pins[pin].pin;
	offset = ((pin & 0x07) << 2);

	data = AW_GPIO_READ(sc, AW_GPIO_GP_CFG(bank, pin >> 3));
	data &= ~(7 << offset);
	data |= (f << offset);
	AW_GPIO_WRITE(sc, AW_GPIO_GP_CFG(bank, pin >> 3), data);

	return (0);
}

static uint32_t
aw_gpio_get_pud(struct aw_gpio_softc *sc, uint32_t pin)
{
	uint32_t bank, offset, val;

	/* Must be called with lock held. */
	AW_GPIO_LOCK_ASSERT(sc);

	bank = sc->conf->padconf->pins[pin].port;
	pin = sc->conf->padconf->pins[pin].pin;
	offset = ((pin & 0x0f) << 1);

	val = AW_GPIO_READ(sc, AW_GPIO_GP_PUL(bank, pin >> 4));

	return ((val >> offset) & AW_GPIO_PUD_MASK);
}

static void
aw_gpio_set_pud(struct aw_gpio_softc *sc, uint32_t pin, uint32_t state)
{
	uint32_t bank, offset, val;

	if (aw_gpio_get_pud(sc, pin) == state)
		return;

	/* Must be called with lock held. */
	AW_GPIO_LOCK_ASSERT(sc);

	bank = sc->conf->padconf->pins[pin].port;
	pin = sc->conf->padconf->pins[pin].pin;
	offset = ((pin & 0x0f) << 1);

	val = AW_GPIO_READ(sc, AW_GPIO_GP_PUL(bank, pin >> 4));
	val &= ~(AW_GPIO_PUD_MASK << offset);
	val |= (state << offset);
	AW_GPIO_WRITE(sc, AW_GPIO_GP_PUL(bank, pin >> 4), val);
}

static uint32_t
aw_gpio_get_drv(struct aw_gpio_softc *sc, uint32_t pin)
{
	uint32_t bank, offset, val;

	/* Must be called with lock held. */
	AW_GPIO_LOCK_ASSERT(sc);

	bank = sc->conf->padconf->pins[pin].port;
	pin = sc->conf->padconf->pins[pin].pin;
	offset = ((pin & 0x0f) << 1);

	val = AW_GPIO_READ(sc, AW_GPIO_GP_DRV(bank, pin >> 4));

	return ((val >> offset) & AW_GPIO_DRV_MASK);
}

static void
aw_gpio_set_drv(struct aw_gpio_softc *sc, uint32_t pin, uint32_t drive)
{
	uint32_t bank, offset, val;

	if (aw_gpio_get_drv(sc, pin) == drive)
		return;

	/* Must be called with lock held. */
	AW_GPIO_LOCK_ASSERT(sc);

	bank = sc->conf->padconf->pins[pin].port;
	pin = sc->conf->padconf->pins[pin].pin;
	offset = ((pin & 0x0f) << 1);

	val = AW_GPIO_READ(sc, AW_GPIO_GP_DRV(bank, pin >> 4));
	val &= ~(AW_GPIO_DRV_MASK << offset);
	val |= (drive << offset);
	AW_GPIO_WRITE(sc, AW_GPIO_GP_DRV(bank, pin >> 4), val);
}

static int
aw_gpio_pin_configure(struct aw_gpio_softc *sc, uint32_t pin, uint32_t flags)
{
	u_int val;
	int err = 0;

	/* Must be called with lock held. */
	AW_GPIO_LOCK_ASSERT(sc);

	if (pin > sc->conf->padconf->npins)
		return (EINVAL);

	/* Manage input/output. */
	if (flags & GPIO_PIN_INPUT) {
		err = aw_gpio_set_function(sc, pin, AW_GPIO_INPUT);
	} else if ((flags & GPIO_PIN_OUTPUT) &&
	    aw_gpio_get_function(sc, pin) != AW_GPIO_OUTPUT) {
		if (flags & GPIO_PIN_PRESET_LOW) {
			aw_gpio_pin_set_locked(sc, pin, 0);
		} else if (flags & GPIO_PIN_PRESET_HIGH) {
			aw_gpio_pin_set_locked(sc, pin, 1);
		} else {
			/* Read the pin and preset output to current state. */
			err = aw_gpio_set_function(sc, pin, AW_GPIO_INPUT);
			if (err == 0) {
				aw_gpio_pin_get_locked(sc, pin, &val);
				aw_gpio_pin_set_locked(sc, pin, val);
			}
		}
		if (err == 0)
			err = aw_gpio_set_function(sc, pin, AW_GPIO_OUTPUT);
	}

	if (err)
		return (err);

	/* Manage Pull-up/pull-down. */
	if (flags & GPIO_PIN_PULLUP)
		aw_gpio_set_pud(sc, pin, AW_GPIO_PULLUP);
	else if (flags & GPIO_PIN_PULLDOWN)
		aw_gpio_set_pud(sc, pin, AW_GPIO_PULLDOWN);
	else
		aw_gpio_set_pud(sc, pin, AW_GPIO_NONE);

	return (0);
}

static device_t
aw_gpio_get_bus(device_t dev)
{
	struct aw_gpio_softc *sc;

	sc = device_get_softc(dev);

	return (sc->sc_busdev);
}

static int
aw_gpio_pin_max(device_t dev, int *maxpin)
{
	struct aw_gpio_softc *sc;

	sc = device_get_softc(dev);

	*maxpin = sc->conf->padconf->npins - 1;
	return (0);
}

static int
aw_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct aw_gpio_softc *sc;

	sc = device_get_softc(dev);
	if (pin >= sc->conf->padconf->npins)
		return (EINVAL);

	*caps = AW_GPIO_DEFAULT_CAPS;
	if (sc->conf->padconf->pins[pin].eint_func != 0)
		*caps |= AW_GPIO_INTR_CAPS;

	return (0);
}

static int
aw_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct aw_gpio_softc *sc;
	uint32_t func;
	uint32_t pud;

	sc = device_get_softc(dev);
	if (pin >= sc->conf->padconf->npins)
		return (EINVAL);

	AW_GPIO_LOCK(sc);
	func = aw_gpio_get_function(sc, pin);
	switch (func) {
	case AW_GPIO_INPUT:
		*flags = GPIO_PIN_INPUT;
		break;
	case AW_GPIO_OUTPUT:
		*flags = GPIO_PIN_OUTPUT;
		break;
	default:
		*flags = 0;
		break;
	}

	pud = aw_gpio_get_pud(sc, pin);
	switch (pud) {
	case AW_GPIO_PULLDOWN:
		*flags |= GPIO_PIN_PULLDOWN;
		break;
	case AW_GPIO_PULLUP:
		*flags |= GPIO_PIN_PULLUP;
		break;
	default:
		break;
	}

	AW_GPIO_UNLOCK(sc);

	return (0);
}

static int
aw_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct aw_gpio_softc *sc;

	sc = device_get_softc(dev);
	if (pin >= sc->conf->padconf->npins)
		return (EINVAL);

	snprintf(name, GPIOMAXNAME - 1, "%s",
	    sc->conf->padconf->pins[pin].name);
	name[GPIOMAXNAME - 1] = '\0';

	return (0);
}

static int
aw_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct aw_gpio_softc *sc;
	int err;

	sc = device_get_softc(dev);
	if (pin > sc->conf->padconf->npins)
		return (EINVAL);

	AW_GPIO_LOCK(sc);
	err = aw_gpio_pin_configure(sc, pin, flags);
	AW_GPIO_UNLOCK(sc);

	return (err);
}

static int
aw_gpio_pin_set_locked(struct aw_gpio_softc *sc, uint32_t pin,
    unsigned int value)
{
	uint32_t bank, data;

	AW_GPIO_LOCK_ASSERT(sc);

	if (pin > sc->conf->padconf->npins)
		return (EINVAL);

	bank = sc->conf->padconf->pins[pin].port;
	pin = sc->conf->padconf->pins[pin].pin;

	data = AW_GPIO_READ(sc, AW_GPIO_GP_DAT(bank));
	if (value)
		data |= (1 << pin);
	else
		data &= ~(1 << pin);
	AW_GPIO_WRITE(sc, AW_GPIO_GP_DAT(bank), data);

	return (0);
}

static int
aw_gpio_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct aw_gpio_softc *sc;
	int ret;

	sc = device_get_softc(dev);

	AW_GPIO_LOCK(sc);
	ret = aw_gpio_pin_set_locked(sc, pin, value);
	AW_GPIO_UNLOCK(sc);

	return (ret);
}

static int
aw_gpio_pin_get_locked(struct aw_gpio_softc *sc,uint32_t pin,
    unsigned int *val)
{
	uint32_t bank, reg_data;

	AW_GPIO_LOCK_ASSERT(sc);

	if (pin > sc->conf->padconf->npins)
		return (EINVAL);

	bank = sc->conf->padconf->pins[pin].port;
	pin = sc->conf->padconf->pins[pin].pin;

	reg_data = AW_GPIO_READ(sc, AW_GPIO_GP_DAT(bank));
	*val = (reg_data & (1 << pin)) ? 1 : 0;

	return (0);
}

static char *
aw_gpio_parse_function(phandle_t node)
{
	char *function;

	if (OF_getprop_alloc(node, "function",
	    (void **)&function) != -1)
		return (function);
	if (OF_getprop_alloc(node, "allwinner,function",
	    (void **)&function) != -1)
		return (function);

	return (NULL);
}

static const char **
aw_gpio_parse_pins(phandle_t node, int *pins_nb)
{
	const char **pinlist;

	*pins_nb = ofw_bus_string_list_to_array(node, "pins", &pinlist);
	if (*pins_nb > 0)
		return (pinlist);

	*pins_nb = ofw_bus_string_list_to_array(node, "allwinner,pins",
	    &pinlist);
	if (*pins_nb > 0)
		return (pinlist);

	return (NULL);
}

static uint32_t
aw_gpio_parse_bias(phandle_t node)
{
	uint32_t bias;

	if (OF_getencprop(node, "pull", &bias, sizeof(bias)) != -1)
		return (bias);
	if (OF_getencprop(node, "allwinner,pull", &bias, sizeof(bias)) != -1)
		return (bias);
	if (OF_hasprop(node, "bias-disable"))
		return (AW_GPIO_NONE);
	if (OF_hasprop(node, "bias-pull-up"))
		return (AW_GPIO_PULLUP);
	if (OF_hasprop(node, "bias-pull-down"))
		return (AW_GPIO_PULLDOWN);

	return (AW_GPIO_NONE);
}

static int
aw_gpio_parse_drive_strength(phandle_t node, uint32_t *drive)
{
	uint32_t drive_str;

	if (OF_getencprop(node, "drive", drive, sizeof(*drive)) != -1)
		return (0);
	if (OF_getencprop(node, "allwinner,drive", drive, sizeof(*drive)) != -1)
		return (0);
	if (OF_getencprop(node, "drive-strength", &drive_str,
	    sizeof(drive_str)) != -1) {
		*drive = (drive_str / 10) - 1;
		return (0);
	}

	return (1);
}

static int
aw_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct aw_gpio_softc *sc;
	int ret;

	sc = device_get_softc(dev);

	AW_GPIO_LOCK(sc);
	ret = aw_gpio_pin_get_locked(sc, pin, val);
	AW_GPIO_UNLOCK(sc);

	return (ret);
}

static int
aw_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct aw_gpio_softc *sc;
	uint32_t bank, data;

	sc = device_get_softc(dev);
	if (pin > sc->conf->padconf->npins)
		return (EINVAL);

	bank = sc->conf->padconf->pins[pin].port;
	pin = sc->conf->padconf->pins[pin].pin;

	AW_GPIO_LOCK(sc);
	data = AW_GPIO_READ(sc, AW_GPIO_GP_DAT(bank));
	if (data & (1 << pin))
		data &= ~(1 << pin);
	else
		data |= (1 << pin);
	AW_GPIO_WRITE(sc, AW_GPIO_GP_DAT(bank), data);
	AW_GPIO_UNLOCK(sc);

	return (0);
}

static int
aw_gpio_pin_access_32(device_t dev, uint32_t first_pin, uint32_t clear_pins,
    uint32_t change_pins, uint32_t *orig_pins)
{
	struct aw_gpio_softc *sc;
	uint32_t bank, data, pin;

	sc = device_get_softc(dev);
	if (first_pin > sc->conf->padconf->npins)
		return (EINVAL);

	/*
	 * We require that first_pin refers to the first pin in a bank, because
	 * this API is not about convenience, it's for making a set of pins
	 * change simultaneously (required) with reasonably high performance
	 * (desired); we need to do a read-modify-write on a single register.
	 */
	bank = sc->conf->padconf->pins[first_pin].port;
	pin = sc->conf->padconf->pins[first_pin].pin;
	if (pin != 0)
		return (EINVAL);

	AW_GPIO_LOCK(sc);
	data = AW_GPIO_READ(sc, AW_GPIO_GP_DAT(bank));
	if ((clear_pins | change_pins) != 0) 
		AW_GPIO_WRITE(sc, AW_GPIO_GP_DAT(bank),
		    (data & ~clear_pins) ^ change_pins);
	AW_GPIO_UNLOCK(sc);

	if (orig_pins != NULL)
		*orig_pins = data;

	return (0);
}

static int
aw_gpio_pin_config_32(device_t dev, uint32_t first_pin, uint32_t num_pins,
    uint32_t *pin_flags)
{
	struct aw_gpio_softc *sc;
	uint32_t pin;
	int err;

	sc = device_get_softc(dev);
	if (first_pin > sc->conf->padconf->npins)
		return (EINVAL);

	if (sc->conf->padconf->pins[first_pin].pin != 0)
		return (EINVAL);

	/*
	 * The configuration for a bank of pins is scattered among several
	 * registers; we cannot g'tee to simultaneously change the state of all
	 * the pins in the flags array.  So just loop through the array
	 * configuring each pin for now.  If there was a strong need, it might
	 * be possible to support some limited simultaneous config, such as
	 * adjacent groups of 8 pins that line up the same as the config regs.
	 */
	for (err = 0, pin = first_pin; err == 0 && pin < num_pins; ++pin) {
		if (pin_flags[pin] & (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT))
			err = aw_gpio_pin_configure(sc, pin, pin_flags[pin]);
	}

	return (err);
}

static int
aw_gpio_map_gpios(device_t bus, phandle_t dev, phandle_t gparent, int gcells,
    pcell_t *gpios, uint32_t *pin, uint32_t *flags)
{
	struct aw_gpio_softc *sc;
	int i;

	sc = device_get_softc(bus);

	/* The GPIO pins are mapped as: <gpio-phandle bank pin flags>. */
	for (i = 0; i < sc->conf->padconf->npins; i++)
		if (sc->conf->padconf->pins[i].port == gpios[0] &&
		    sc->conf->padconf->pins[i].pin == gpios[1]) {
			*pin = i;
			break;
		}
	*flags = gpios[gcells - 1];

	return (0);
}

static int
aw_find_pinnum_by_name(struct aw_gpio_softc *sc, const char *pinname)
{
	int i;

	for (i = 0; i < sc->conf->padconf->npins; i++)
		if (!strcmp(pinname, sc->conf->padconf->pins[i].name))
			return i;

	return (-1);
}

static int
aw_find_pin_func(struct aw_gpio_softc *sc, int pin, const char *func)
{
	int i;

	for (i = 0; i < AW_MAX_FUNC_BY_PIN; i++)
		if (sc->conf->padconf->pins[pin].functions[i] &&
		    !strcmp(func, sc->conf->padconf->pins[pin].functions[i]))
			return (i);

	return (-1);
}

static int
aw_fdt_configure_pins(device_t dev, phandle_t cfgxref)
{
	struct aw_gpio_softc *sc;
	phandle_t node;
	const char **pinlist = NULL;
	char *pin_function = NULL;
	uint32_t pin_drive, pin_pull;
	int pins_nb, pin_num, pin_func, i, ret;
	bool set_drive;

	sc = device_get_softc(dev);
	node = OF_node_from_xref(cfgxref);
	ret = 0;
	set_drive = false;

	/* Getting all prop for configuring pins */
	pinlist = aw_gpio_parse_pins(node, &pins_nb);
	if (pinlist == NULL)
		return (ENOENT);

	pin_function = aw_gpio_parse_function(node);
	if (pin_function == NULL) {
		ret = ENOENT;
		goto out;
	}

	if (aw_gpio_parse_drive_strength(node, &pin_drive) == 0)
		set_drive = true;

	pin_pull = aw_gpio_parse_bias(node);

	/* Configure each pin to the correct function, drive and pull */
	for (i = 0; i < pins_nb; i++) {
		pin_num = aw_find_pinnum_by_name(sc, pinlist[i]);
		if (pin_num == -1) {
			ret = ENOENT;
			goto out;
		}
		pin_func = aw_find_pin_func(sc, pin_num, pin_function);
		if (pin_func == -1) {
			ret = ENOENT;
			goto out;
		}

		AW_GPIO_LOCK(sc);

		if (aw_gpio_get_function(sc, pin_num) != pin_func)
			aw_gpio_set_function(sc, pin_num, pin_func);
		if (set_drive)
			aw_gpio_set_drv(sc, pin_num, pin_drive);
		if (pin_pull != AW_GPIO_NONE)
			aw_gpio_set_pud(sc, pin_num, pin_pull);

		AW_GPIO_UNLOCK(sc);
	}

 out:
	OF_prop_free(pinlist);
	OF_prop_free(pin_function);
	return (ret);
}

static void
aw_gpio_enable_bank_supply(void *arg)
{
	struct aw_gpio_softc *sc = arg;
	regulator_t vcc_supply;
	char bank_reg_name[16];
	int i, nbanks;

	nbanks = strlen(sc->conf->banks);
	for (i = 0; i < nbanks; i++) {
		snprintf(bank_reg_name, sizeof(bank_reg_name), "vcc-p%c-supply",
		    sc->conf->banks[i]);

		if (regulator_get_by_ofw_property(sc->sc_dev, 0, bank_reg_name, &vcc_supply) == 0) {
			if (bootverbose)
				device_printf(sc->sc_dev,
				    "Enabling regulator for gpio bank %c\n",
				    sc->conf->banks[i]);
			if (regulator_enable(vcc_supply) != 0) {
				device_printf(sc->sc_dev,
				    "Cannot enable regulator for bank %c\n",
				    sc->conf->banks[i]);
			}
		}
	}
}

static int
aw_gpio_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Allwinner GPIO/Pinmux controller");
	return (BUS_PROBE_DEFAULT);
}

static int
aw_gpio_attach(device_t dev)
{
	int error;
	phandle_t gpio;
	struct aw_gpio_softc *sc;
	struct clk_list *clkp, *clkp_tmp;
	clk_t clk;
	hwreset_t rst = NULL;
	int off, err, clkret;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	mtx_init(&sc->sc_mtx, "aw gpio", "gpio", MTX_SPIN);

	if (bus_alloc_resources(dev, aw_gpio_res_spec, sc->sc_res) != 0) {
		device_printf(dev, "cannot allocate device resources\n");
		return (ENXIO);
	}

	if (bus_setup_intr(dev, sc->sc_res[AW_GPIO_IRQRES],
	    INTR_TYPE_CLK | INTR_MPSAFE, NULL, aw_gpio_intr, sc,
	    &sc->sc_intrhand)) {
		device_printf(dev, "cannot setup interrupt handler\n");
		goto fail;
	}

	/* Find our node. */
	gpio = ofw_bus_get_node(sc->sc_dev);
	if (!OF_hasprop(gpio, "gpio-controller"))
		/* Node is not a GPIO controller. */
		goto fail;

	/* Use the right pin data for the current SoC */
	sc->conf = (struct aw_gpio_conf *)ofw_bus_search_compatible(dev,
	    compat_data)->ocd_data;

	if (hwreset_get_by_ofw_idx(dev, 0, 0, &rst) == 0) {
		error = hwreset_deassert(rst);
		if (error != 0) {
			device_printf(dev, "cannot de-assert reset\n");
			goto fail;
		}
	}

	TAILQ_INIT(&sc->clk_list);
	for (off = 0, clkret = 0; clkret == 0; off++) {
		clkret = clk_get_by_ofw_index(dev, 0, off, &clk);
		if (clkret != 0)
			break;
		err = clk_enable(clk);
		if (err != 0) {
			device_printf(dev, "Could not enable clock %s\n",
			    clk_get_name(clk));
			goto fail;
		}
		clkp = malloc(sizeof(*clkp), M_DEVBUF, M_WAITOK | M_ZERO);
		clkp->clk = clk;
		TAILQ_INSERT_TAIL(&sc->clk_list, clkp, next);
	}
	if (clkret != 0 && clkret != ENOENT) {
		device_printf(dev, "Could not find clock at offset %d (%d)\n",
		    off, clkret);
		goto fail;
	}

	aw_gpio_register_isrcs(sc);
	intr_pic_register(dev, OF_xref_from_node(ofw_bus_get_node(dev)));

	sc->sc_busdev = gpiobus_attach_bus(dev);
	if (sc->sc_busdev == NULL)
		goto fail;

	/*
	 * Register as a pinctrl device
	 */
	fdt_pinctrl_register(dev, "pins");
	fdt_pinctrl_configure_tree(dev);
	fdt_pinctrl_register(dev, "allwinner,pins");
	fdt_pinctrl_configure_tree(dev);

	config_intrhook_oneshot(aw_gpio_enable_bank_supply, sc);

	return (0);

fail:
	if (sc->sc_irq_res)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq_res);
	if (sc->sc_mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);
	mtx_destroy(&sc->sc_mtx);

	/* Disable clock */
	TAILQ_FOREACH_SAFE(clkp, &sc->clk_list, next, clkp_tmp) {
		err = clk_disable(clkp->clk);
		if (err != 0)
			device_printf(dev, "Could not disable clock %s\n",
			    clk_get_name(clkp->clk));
		err = clk_release(clkp->clk);
		if (err != 0)
			device_printf(dev, "Could not release clock %s\n",
			    clk_get_name(clkp->clk));
		TAILQ_REMOVE(&sc->clk_list, clkp, next);
		free(clkp, M_DEVBUF);
	}

	/* Assert resets */
	if (rst) {
		hwreset_assert(rst);
		hwreset_release(rst);
	}

	return (ENXIO);
}

static int
aw_gpio_detach(device_t dev)
{

	return (EBUSY);
}

static void
aw_gpio_intr(void *arg)
{
	struct aw_gpio_softc *sc;
	struct intr_irqsrc *isrc;
	uint32_t reg;
	int irq;

	sc = (struct aw_gpio_softc *)arg;

	AW_GPIO_LOCK(sc);
	for (irq = 0; irq < sc->nirqs; irq++) {
		if (!sc->gpio_pic_irqsrc[irq].enabled)
			continue;

		reg = AW_GPIO_READ(sc, AW_GPIO_GP_INT_STA(sc->gpio_pic_irqsrc[irq].bank));
		if (!(reg & (1 << sc->gpio_pic_irqsrc[irq].intnum)))
			continue;

		isrc = &sc->gpio_pic_irqsrc[irq].isrc;
		if (intr_isrc_dispatch(isrc, curthread->td_intr_frame) != 0) {
			aw_gpio_pic_disable_intr_locked(sc, isrc);
			aw_gpio_pic_post_filter(sc->sc_dev, isrc);
			device_printf(sc->sc_dev, "Stray irq %u disabled\n", irq);
		}
	}
	AW_GPIO_UNLOCK(sc);
}

/*
 * Interrupts support
 */

static int
aw_gpio_register_isrcs(struct aw_gpio_softc *sc)
{
	const char *name;
	int nirqs;
	int pin;
	int err;

	name = device_get_nameunit(sc->sc_dev);

	for (nirqs = 0, pin = 0; pin < sc->conf->padconf->npins; pin++) {
		if (sc->conf->padconf->pins[pin].eint_func == 0)
			continue;

		nirqs++;
	}

	sc->gpio_pic_irqsrc = malloc(sizeof(*sc->gpio_pic_irqsrc) * nirqs,
	    M_DEVBUF, M_WAITOK | M_ZERO);
	for (nirqs = 0, pin = 0; pin < sc->conf->padconf->npins; pin++) {
		if (sc->conf->padconf->pins[pin].eint_func == 0)
			continue;

		sc->gpio_pic_irqsrc[nirqs].pin = pin;
		sc->gpio_pic_irqsrc[nirqs].bank = sc->conf->padconf->pins[pin].eint_bank;
		sc->gpio_pic_irqsrc[nirqs].intnum = sc->conf->padconf->pins[pin].eint_num;
		sc->gpio_pic_irqsrc[nirqs].intfunc = sc->conf->padconf->pins[pin].eint_func;
		sc->gpio_pic_irqsrc[nirqs].irq = nirqs;
		sc->gpio_pic_irqsrc[nirqs].mode = GPIO_INTR_CONFORM;

		err = intr_isrc_register(&sc->gpio_pic_irqsrc[nirqs].isrc,
		    sc->sc_dev, 0, "%s,%s", name,
		    sc->conf->padconf->pins[pin].functions[sc->conf->padconf->pins[pin].eint_func]);
		if (err) {
			device_printf(sc->sc_dev, "intr_isrs_register failed for irq %d\n", nirqs);
		}

		nirqs++;
	}

	sc->nirqs = nirqs;

	return (0);
}

static void
aw_gpio_pic_disable_intr_locked(struct aw_gpio_softc *sc, struct intr_irqsrc *isrc)
{
	u_int irq;
	uint32_t reg;

	AW_GPIO_LOCK_ASSERT(sc);
	irq = ((struct gpio_irqsrc *)isrc)->irq;
	reg = AW_GPIO_READ(sc, AW_GPIO_GP_INT_CTL(sc->gpio_pic_irqsrc[irq].bank));
	reg &= ~(1 << sc->gpio_pic_irqsrc[irq].intnum);
	AW_GPIO_WRITE(sc, AW_GPIO_GP_INT_CTL(sc->gpio_pic_irqsrc[irq].bank), reg);

	sc->gpio_pic_irqsrc[irq].enabled = false;
}

static void
aw_gpio_pic_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct aw_gpio_softc *sc;

	sc = device_get_softc(dev);

	AW_GPIO_LOCK(sc);
	aw_gpio_pic_disable_intr_locked(sc, isrc);
	AW_GPIO_UNLOCK(sc);
}

static void
aw_gpio_pic_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct aw_gpio_softc *sc;
	u_int irq;
	uint32_t reg;

	sc = device_get_softc(dev);
	irq = ((struct gpio_irqsrc *)isrc)->irq;
	AW_GPIO_LOCK(sc);
	reg = AW_GPIO_READ(sc, AW_GPIO_GP_INT_CTL(sc->gpio_pic_irqsrc[irq].bank));
	reg |= 1 << sc->gpio_pic_irqsrc[irq].intnum;
	AW_GPIO_WRITE(sc, AW_GPIO_GP_INT_CTL(sc->gpio_pic_irqsrc[irq].bank), reg);
	AW_GPIO_UNLOCK(sc);

	sc->gpio_pic_irqsrc[irq].enabled = true;
}

static int
aw_gpio_pic_map_gpio(struct aw_gpio_softc *sc, struct intr_map_data_gpio *dag,
    u_int *irqp, u_int *mode)
{
	u_int irq;
	int pin;

	irq = dag->gpio_pin_num;

	for (pin = 0; pin < sc->nirqs; pin++)
		if (sc->gpio_pic_irqsrc[pin].pin == irq)
			break;
	if (pin == sc->nirqs) {
		device_printf(sc->sc_dev, "Invalid interrupt number %u\n", irq);
		return (EINVAL);
	}

	switch (dag->gpio_intr_mode) {
	case GPIO_INTR_LEVEL_LOW:
	case GPIO_INTR_LEVEL_HIGH:
	case GPIO_INTR_EDGE_RISING:
	case GPIO_INTR_EDGE_FALLING:
	case GPIO_INTR_EDGE_BOTH:
		break;
	default:
		device_printf(sc->sc_dev, "Unsupported interrupt mode 0x%8x\n",
		    dag->gpio_intr_mode);
		return (EINVAL);
	}

	*irqp = pin;
	if (mode != NULL)
		*mode = dag->gpio_intr_mode;

	return (0);
}

static int
aw_gpio_pic_map_intr(device_t dev, struct intr_map_data *data,
    struct intr_irqsrc **isrcp)
{
	struct aw_gpio_softc *sc;
	u_int irq;
	int err;

	sc = device_get_softc(dev);
	switch (data->type) {
	case INTR_MAP_DATA_GPIO:
		err = aw_gpio_pic_map_gpio(sc,
		    (struct intr_map_data_gpio *)data,
		  &irq, NULL);
		break;
	default:
		return (ENOTSUP);
	};

	if (err == 0)
		*isrcp = &sc->gpio_pic_irqsrc[irq].isrc;
	return (0);
}

static int
aw_gpio_pic_setup_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct aw_gpio_softc *sc;
	uint32_t irqcfg;
	uint32_t pinidx, reg;
	u_int irq, mode;
	int err;

	sc = device_get_softc(dev);

	err = 0;
	switch (data->type) {
	case INTR_MAP_DATA_GPIO:
		err = aw_gpio_pic_map_gpio(sc,
		    (struct intr_map_data_gpio *)data,
		  &irq, &mode);
		if (err != 0)
			return (err);
		break;
	default:
		return (ENOTSUP);
	};

	pinidx = (sc->gpio_pic_irqsrc[irq].intnum % 8) * 4;

	AW_GPIO_LOCK(sc);
	switch (mode) {
	case GPIO_INTR_LEVEL_LOW:
		irqcfg = AW_GPIO_INT_LEVEL_LOW << pinidx;
		break;
	case GPIO_INTR_LEVEL_HIGH:
		irqcfg = AW_GPIO_INT_LEVEL_HIGH << pinidx;
		break;
	case GPIO_INTR_EDGE_RISING:
		irqcfg = AW_GPIO_INT_EDGE_POSITIVE << pinidx;
		break;
	case GPIO_INTR_EDGE_FALLING:
		irqcfg = AW_GPIO_INT_EDGE_NEGATIVE << pinidx;
		break;
	case GPIO_INTR_EDGE_BOTH:
		irqcfg = AW_GPIO_INT_EDGE_BOTH << pinidx;
		break;
	}

	/* Switch the pin to interrupt mode */
	sc->gpio_pic_irqsrc[irq].oldfunc = aw_gpio_get_function(sc,
	    sc->gpio_pic_irqsrc[irq].pin);
	aw_gpio_set_function(sc, sc->gpio_pic_irqsrc[irq].pin,
	    sc->gpio_pic_irqsrc[irq].intfunc);

	/* Write interrupt mode */
	reg = AW_GPIO_READ(sc, 
	    AW_GPIO_GP_INT_CFG(sc->gpio_pic_irqsrc[irq].bank,
	    sc->gpio_pic_irqsrc[irq].intnum));
	reg &= ~(0xF << pinidx);
	reg |= irqcfg;
	AW_GPIO_WRITE(sc,
	    AW_GPIO_GP_INT_CFG(sc->gpio_pic_irqsrc[irq].bank,
	    sc->gpio_pic_irqsrc[irq].intnum),
	    reg);

	AW_GPIO_UNLOCK(sc);

	return (0);
}

static int
aw_gpio_pic_teardown_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct aw_gpio_softc *sc;
	struct gpio_irqsrc *gi;

	sc = device_get_softc(dev);
	gi = (struct gpio_irqsrc *)isrc;

	/* Switch back the pin to it's original function */
	AW_GPIO_LOCK(sc);
	aw_gpio_set_function(sc, gi->pin, gi->oldfunc);
	AW_GPIO_UNLOCK(sc);

	return (0);
}

static void
aw_gpio_pic_post_filter(device_t dev, struct intr_irqsrc *isrc)
{
	struct aw_gpio_softc *sc;
	struct gpio_irqsrc *gi;

	sc = device_get_softc(dev);
	gi = (struct gpio_irqsrc *)isrc;

	arm_irq_memory_barrier(0);
	AW_GPIO_WRITE(sc, AW_GPIO_GP_INT_STA(gi->bank), 1 << gi->intnum);
}

static void
aw_gpio_pic_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct aw_gpio_softc *sc;
	struct gpio_irqsrc *gi;

	sc = device_get_softc(dev);
	gi = (struct gpio_irqsrc *)isrc;

	arm_irq_memory_barrier(0);
	AW_GPIO_WRITE(sc, AW_GPIO_GP_INT_STA(gi->bank), 1 << gi->intnum);
	aw_gpio_pic_enable_intr(dev, isrc);
}

static void
aw_gpio_pic_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct aw_gpio_softc *sc;

	sc = device_get_softc(dev);
	aw_gpio_pic_disable_intr_locked(sc, isrc);
}

/*
 * OFWBUS Interface
 */
static phandle_t
aw_gpio_get_node(device_t dev, device_t bus)
{

	/* We only have one child, the GPIO bus, which needs our own node. */
	return (ofw_bus_get_node(dev));
}

static device_method_t aw_gpio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		aw_gpio_probe),
	DEVMETHOD(device_attach,	aw_gpio_attach),
	DEVMETHOD(device_detach,	aw_gpio_detach),

	/* Interrupt controller interface */
	DEVMETHOD(pic_disable_intr,	aw_gpio_pic_disable_intr),
	DEVMETHOD(pic_enable_intr,	aw_gpio_pic_enable_intr),
	DEVMETHOD(pic_map_intr,		aw_gpio_pic_map_intr),
	DEVMETHOD(pic_setup_intr,	aw_gpio_pic_setup_intr),
	DEVMETHOD(pic_teardown_intr,	aw_gpio_pic_teardown_intr),
	DEVMETHOD(pic_post_filter,	aw_gpio_pic_post_filter),
	DEVMETHOD(pic_post_ithread,	aw_gpio_pic_post_ithread),
	DEVMETHOD(pic_pre_ithread,	aw_gpio_pic_pre_ithread),

	/* GPIO protocol */
	DEVMETHOD(gpio_get_bus,		aw_gpio_get_bus),
	DEVMETHOD(gpio_pin_max,		aw_gpio_pin_max),
	DEVMETHOD(gpio_pin_getname,	aw_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getflags,	aw_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_getcaps,	aw_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_setflags,	aw_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_get,		aw_gpio_pin_get),
	DEVMETHOD(gpio_pin_set,		aw_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle,	aw_gpio_pin_toggle),
	DEVMETHOD(gpio_pin_access_32,	aw_gpio_pin_access_32),
	DEVMETHOD(gpio_pin_config_32,	aw_gpio_pin_config_32),
	DEVMETHOD(gpio_map_gpios,	aw_gpio_map_gpios),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	aw_gpio_get_node),

        /* fdt_pinctrl interface */
	DEVMETHOD(fdt_pinctrl_configure,aw_fdt_configure_pins),

	DEVMETHOD_END
};

static driver_t aw_gpio_driver = {
	"gpio",
	aw_gpio_methods,
	sizeof(struct aw_gpio_softc),
};

EARLY_DRIVER_MODULE(aw_gpio, simplebus, aw_gpio_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LATE);
