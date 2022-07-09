/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018 Emmanuel Vadot <manu@FreeBSD.org>
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

#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/intr.h>

#include <dev/fdt/simplebus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/fdt/fdt_pinctrl.h>

#include <dev/extres/syscon/syscon.h>

#include "gpio_if.h"
#include "syscon_if.h"
#include "fdt_pinctrl_if.h"

struct rk_pinctrl_pin_drive {
	uint32_t	bank;
	uint32_t	subbank;
	uint32_t	offset;
	uint32_t	value;
	uint32_t	ma;
};

struct rk_pinctrl_bank {
	uint32_t	bank;
	uint32_t	subbank;
	uint32_t	offset;
	uint32_t	nbits;
};

struct rk_pinctrl_pin_fixup {
	uint32_t	bank;
	uint32_t	subbank;
	uint32_t	pin;
	uint32_t	reg;
	uint32_t	bit;
	uint32_t	mask;
};

struct rk_pinctrl_gpio {
	uint32_t	bank;
	char		*gpio_name;
	device_t	gpio_dev;
};

struct rk_pinctrl_softc;

struct rk_pinctrl_conf {
	struct rk_pinctrl_bank		*iomux_conf;
	uint32_t			iomux_nbanks;
	struct rk_pinctrl_pin_fixup	*pin_fixup;
	uint32_t			npin_fixup;
	struct rk_pinctrl_pin_drive	*pin_drive;
	uint32_t			npin_drive;
	struct rk_pinctrl_gpio		*gpio_bank;
	uint32_t			ngpio_bank;
	uint32_t	(*get_pd_offset)(struct rk_pinctrl_softc *, uint32_t);
	struct syscon	*(*get_syscon)(struct rk_pinctrl_softc *, uint32_t);
	int		(*parse_bias)(phandle_t, int);
	int		(*resolv_bias_value)(int, int);
	int		(*get_bias_value)(int, int);
};

struct rk_pinctrl_softc {
	struct simplebus_softc	simplebus_sc;
	device_t		dev;
	struct syscon		*grf;
	struct syscon		*pmu;
	struct rk_pinctrl_conf	*conf;
	struct mtx		mtx;
};

#define	RK_PINCTRL_LOCK(_sc)		mtx_lock_spin(&(_sc)->mtx)
#define	RK_PINCTRL_UNLOCK(_sc)		mtx_unlock_spin(&(_sc)->mtx)
#define	RK_PINCTRL_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->mtx, MA_OWNED)

#define	RK_IOMUX(_bank, _subbank, _offset, _nbits)			\
{									\
	.bank = _bank,							\
	.subbank = _subbank,						\
	.offset = _offset,						\
	.nbits = _nbits,						\
}

#define	RK_PINFIX(_bank, _pin, _reg, _bit, _mask)			\
{									\
	.bank = _bank,							\
	.pin = _pin,							\
	.reg = _reg,							\
	.bit = _bit,							\
	.mask = _mask,							\
}

#define	RK_PINDRIVE(_bank, _subbank, _offset, _value, _ma)		\
{									\
	.bank = _bank,							\
	.subbank = _subbank,						\
	.offset = _offset,						\
	.value = _value,						\
	.ma = _ma,							\
}
#define	RK_GPIO(_bank, _name)						\
{									\
	.bank = _bank,							\
	.gpio_name = _name,						\
}

static struct rk_pinctrl_gpio rk3288_gpio_bank[] = {
	RK_GPIO(0, "gpio0"),
	RK_GPIO(1, "gpio1"),
	RK_GPIO(2, "gpio2"),
	RK_GPIO(3, "gpio3"),
	RK_GPIO(4, "gpio4"),
	RK_GPIO(5, "gpio5"),
	RK_GPIO(6, "gpio6"),
	RK_GPIO(7, "gpio7"),
	RK_GPIO(8, "gpio8"),
};

static struct rk_pinctrl_bank rk3288_iomux_bank[] = {
	/*    bank sub  offs   nbits */
	/* PMU */
	RK_IOMUX(0, 0, 0x0084, 2),
	RK_IOMUX(0, 1, 0x0088, 2),
	RK_IOMUX(0, 2, 0x008C, 2),
	/* GFR */
	RK_IOMUX(1, 3, 0x000C, 2),
	RK_IOMUX(2, 0, 0x0010, 2),
	RK_IOMUX(2, 1, 0x0014, 2),
	RK_IOMUX(2, 2, 0x0018, 2),
	RK_IOMUX(2, 3, 0x001C, 2),
	RK_IOMUX(3, 0, 0x0020, 2),
	RK_IOMUX(3, 1, 0x0024, 2),
	RK_IOMUX(3, 2, 0x0028, 2),
	RK_IOMUX(3, 3, 0x002C, 4),
	RK_IOMUX(4, 0, 0x0034, 4),
	RK_IOMUX(4, 1, 0x003C, 4),
	RK_IOMUX(4, 2, 0x0044, 2),
	RK_IOMUX(4, 3, 0x0048, 2),
	/* 5,0 - Empty */
	RK_IOMUX(5, 1, 0x0050, 2),
	RK_IOMUX(5, 2, 0x0054, 2),
	/* 5,3 - Empty */
	RK_IOMUX(6, 0, 0x005C, 2),
	RK_IOMUX(6, 1, 0x0060, 2),
	RK_IOMUX(6, 2, 0x0064, 2),
	/* 6,3 - Empty */
	RK_IOMUX(7, 0, 0x006C, 2),
	RK_IOMUX(7, 1, 0x0070, 2),
	RK_IOMUX(7, 2, 0x0074, 4),
	/* 7,3 - Empty */
	RK_IOMUX(8, 0, 0x0080, 2),
	RK_IOMUX(8, 1, 0x0084, 2),
	/* 8,2 - Empty */
	/* 8,3 - Empty */

};

static struct rk_pinctrl_pin_fixup rk3288_pin_fixup[] = {
};

static struct rk_pinctrl_pin_drive rk3288_pin_drive[] = {
	/*       bank sub offs val ma */
	/* GPIO0A (PMU)*/
	RK_PINDRIVE(0, 0, 0x070, 0, 2),
	RK_PINDRIVE(0, 0, 0x070, 1, 4),
	RK_PINDRIVE(0, 0, 0x070, 2, 8),
	RK_PINDRIVE(0, 0, 0x070, 3, 12),

	/* GPIO0B (PMU)*/
	RK_PINDRIVE(0, 1, 0x074, 0, 2),
	RK_PINDRIVE(0, 1, 0x074, 1, 4),
	RK_PINDRIVE(0, 1, 0x074, 2, 8),
	RK_PINDRIVE(0, 1, 0x074, 3, 12),

	/* GPIO0C (PMU)*/
	RK_PINDRIVE(0, 2, 0x078, 0, 2),
	RK_PINDRIVE(0, 2, 0x078, 1, 4),
	RK_PINDRIVE(0, 2, 0x078, 2, 8),
	RK_PINDRIVE(0, 2, 0x078, 3, 12),

	/* GPIO1D */
	RK_PINDRIVE(1, 3, 0x1CC, 0, 2),
	RK_PINDRIVE(1, 3, 0x1CC, 1, 4),
	RK_PINDRIVE(1, 3, 0x1CC, 2, 8),
	RK_PINDRIVE(1, 3, 0x1CC, 3, 12),

	/* GPIO2A */
	RK_PINDRIVE(2, 0, 0x1D0, 0, 2),
	RK_PINDRIVE(2, 0, 0x1D0, 1, 4),
	RK_PINDRIVE(2, 0, 0x1D0, 2, 8),
	RK_PINDRIVE(2, 0, 0x1D0, 3, 12),

	/* GPIO2B */
	RK_PINDRIVE(2, 1, 0x1D4, 0, 2),
	RK_PINDRIVE(2, 1, 0x1D4, 1, 4),
	RK_PINDRIVE(2, 1, 0x1D4, 2, 8),
	RK_PINDRIVE(2, 1, 0x1D4, 3, 12),

	/* GPIO2C */
	RK_PINDRIVE(2, 2, 0x1D8, 0, 2),
	RK_PINDRIVE(2, 2, 0x1D8, 1, 4),
	RK_PINDRIVE(2, 2, 0x1D8, 2, 8),
	RK_PINDRIVE(2, 2, 0x1D8, 3, 12),

	/* GPIO2D */
	RK_PINDRIVE(2, 3, 0x1DC, 0, 2),
	RK_PINDRIVE(2, 3, 0x1DC, 1, 4),
	RK_PINDRIVE(2, 3, 0x1DC, 2, 8),
	RK_PINDRIVE(2, 3, 0x1DC, 3, 12),

	/* GPIO3A */
	RK_PINDRIVE(3, 0, 0x1E0, 0, 2),
	RK_PINDRIVE(3, 0, 0x1E0, 1, 4),
	RK_PINDRIVE(3, 0, 0x1E0, 2, 8),
	RK_PINDRIVE(3, 0, 0x1E0, 3, 12),

	/* GPIO3B */
	RK_PINDRIVE(3, 1, 0x1E4, 0, 2),
	RK_PINDRIVE(3, 1, 0x1E4, 1, 4),
	RK_PINDRIVE(3, 1, 0x1E4, 2, 8),
	RK_PINDRIVE(3, 1, 0x1E4, 3, 12),

	/* GPIO3C */
	RK_PINDRIVE(3, 2, 0x1E8, 0, 2),
	RK_PINDRIVE(3, 2, 0x1E8, 1, 4),
	RK_PINDRIVE(3, 2, 0x1E8, 2, 8),
	RK_PINDRIVE(3, 2, 0x1E8, 3, 12),

	/* GPIO3D */
	RK_PINDRIVE(3, 3, 0x1EC, 0, 2),
	RK_PINDRIVE(3, 3, 0x1EC, 1, 4),
	RK_PINDRIVE(3, 3, 0x1EC, 2, 8),
	RK_PINDRIVE(3, 3, 0x1EC, 3, 12),

	/* GPIO4A */
	RK_PINDRIVE(4, 0, 0x1F0, 0, 2),
	RK_PINDRIVE(4, 0, 0x1F0, 1, 4),
	RK_PINDRIVE(4, 0, 0x1F0, 2, 8),
	RK_PINDRIVE(4, 0, 0x1F0, 3, 12),

	/* GPIO4B */
	RK_PINDRIVE(4, 1, 0x1F4, 0, 2),
	RK_PINDRIVE(4, 1, 0x1F4, 1, 4),
	RK_PINDRIVE(4, 1, 0x1F4, 2, 8),
	RK_PINDRIVE(4, 1, 0x1F4, 3, 12),

	/* GPIO4C */
	RK_PINDRIVE(4, 2, 0x1F8, 0, 2),
	RK_PINDRIVE(4, 2, 0x1F8, 1, 4),
	RK_PINDRIVE(4, 2, 0x1F8, 2, 8),
	RK_PINDRIVE(4, 2, 0x1F8, 3, 12),

	/* GPIO4D */
	RK_PINDRIVE(4, 3, 0x1FC, 0, 2),
	RK_PINDRIVE(4, 3, 0x1FC, 1, 4),
	RK_PINDRIVE(4, 3, 0x1FC, 2, 8),
	RK_PINDRIVE(4, 3, 0x1FC, 3, 12),

	/* GPIO5B */
	RK_PINDRIVE(5, 1, 0x204, 0, 2),
	RK_PINDRIVE(5, 1, 0x204, 1, 4),
	RK_PINDRIVE(5, 1, 0x204, 2, 8),
	RK_PINDRIVE(5, 1, 0x204, 3, 12),

	/* GPIO5C */
	RK_PINDRIVE(5, 2, 0x208, 0, 2),
	RK_PINDRIVE(5, 2, 0x208, 1, 4),
	RK_PINDRIVE(5, 2, 0x208, 2, 8),
	RK_PINDRIVE(5, 2, 0x208, 3, 12),

	/* GPIO6A */
	RK_PINDRIVE(6, 0, 0x210, 0, 2),
	RK_PINDRIVE(6, 0, 0x210, 1, 4),
	RK_PINDRIVE(6, 0, 0x210, 2, 8),
	RK_PINDRIVE(6, 0, 0x210, 3, 12),

	/* GPIO6B */
	RK_PINDRIVE(6, 1, 0x214, 0, 2),
	RK_PINDRIVE(6, 1, 0x214, 1, 4),
	RK_PINDRIVE(6, 1, 0x214, 2, 8),
	RK_PINDRIVE(6, 1, 0x214, 3, 12),

	/* GPIO6C */
	RK_PINDRIVE(6, 2, 0x218, 0, 2),
	RK_PINDRIVE(6, 2, 0x218, 1, 4),
	RK_PINDRIVE(6, 2, 0x218, 2, 8),
	RK_PINDRIVE(6, 2, 0x218, 3, 12),

	/* GPIO7A */
	RK_PINDRIVE(7, 0, 0x220, 0, 2),
	RK_PINDRIVE(7, 0, 0x220, 1, 4),
	RK_PINDRIVE(7, 0, 0x220, 2, 8),
	RK_PINDRIVE(7, 0, 0x220, 3, 12),

	/* GPIO7B */
	RK_PINDRIVE(7, 1, 0x224, 0, 2),
	RK_PINDRIVE(7, 1, 0x224, 1, 4),
	RK_PINDRIVE(7, 1, 0x224, 2, 8),
	RK_PINDRIVE(7, 1, 0x224, 3, 12),

	/* GPIO7C */
	RK_PINDRIVE(7, 2, 0x228, 0, 2),
	RK_PINDRIVE(7, 2, 0x228, 1, 4),
	RK_PINDRIVE(7, 2, 0x228, 2, 8),
	RK_PINDRIVE(7, 2, 0x228, 3, 12),

	/* GPIO8A */
	RK_PINDRIVE(8, 0, 0x230, 0, 2),
	RK_PINDRIVE(8, 0, 0x230, 1, 4),
	RK_PINDRIVE(8, 0, 0x230, 2, 8),
	RK_PINDRIVE(8, 0, 0x230, 3, 12),

	/* GPIO8B */
	RK_PINDRIVE(8, 1, 0x234, 0, 2),
	RK_PINDRIVE(8, 1, 0x234, 1, 4),
	RK_PINDRIVE(8, 1, 0x234, 2, 8),
	RK_PINDRIVE(8, 1, 0x234, 3, 12),
};

static uint32_t
rk3288_get_pd_offset(struct rk_pinctrl_softc *sc, uint32_t bank)
{
	if (bank == 0)
		return (0x064);		/* PMU */
	return (0x130);
}

static struct syscon *
rk3288_get_syscon(struct rk_pinctrl_softc *sc, uint32_t bank)
{
	if (bank == 0)
		return (sc->pmu);
	return (sc->grf);
}

static int
rk3288_parse_bias(phandle_t node, int bank)
{
	if (OF_hasprop(node, "bias-disable"))
		return (0);
	if (OF_hasprop(node, "bias-pull-up"))
		return (1);
	if (OF_hasprop(node, "bias-pull-down"))
		return (2);

	return (-1);
}

static int
rk3288_resolv_bias_value(int bank, int bias)
{
	int rv = 0;

	if (bias == 1)
		rv = GPIO_PIN_PULLUP;
	else if (bias == 2)
		rv = GPIO_PIN_PULLDOWN;

	return (rv);
}

static int
rk3288_get_bias_value(int bank, int bias)
{
	int rv = 0;

	if (bias & GPIO_PIN_PULLUP)
		rv = 1;
	else if (bias & GPIO_PIN_PULLDOWN)
		rv = 2;

	return (rv);
}

struct rk_pinctrl_conf rk3288_conf = {
	.iomux_conf = rk3288_iomux_bank,
	.iomux_nbanks = nitems(rk3288_iomux_bank),
	.pin_fixup = rk3288_pin_fixup,
	.npin_fixup = nitems(rk3288_pin_fixup),
	.pin_drive = rk3288_pin_drive,
	.npin_drive = nitems(rk3288_pin_drive),
	.gpio_bank = rk3288_gpio_bank,
	.ngpio_bank = nitems(rk3288_gpio_bank),
	.get_pd_offset = rk3288_get_pd_offset,
	.get_syscon = rk3288_get_syscon,
	.parse_bias = rk3288_parse_bias,
	.resolv_bias_value = rk3288_resolv_bias_value,
	.get_bias_value = rk3288_get_bias_value,
};

static struct rk_pinctrl_gpio rk3328_gpio_bank[] = {
	RK_GPIO(0, "gpio0"),
	RK_GPIO(1, "gpio1"),
	RK_GPIO(2, "gpio2"),
	RK_GPIO(3, "gpio3"),
};

static struct rk_pinctrl_bank rk3328_iomux_bank[] = {
	/*    bank sub offs nbits */
	RK_IOMUX(0, 0, 0x0000, 2),
	RK_IOMUX(0, 1, 0x0004, 2),
	RK_IOMUX(0, 2, 0x0008, 2),
	RK_IOMUX(0, 3, 0x000C, 2),
	RK_IOMUX(1, 0, 0x0010, 2),
	RK_IOMUX(1, 1, 0x0014, 2),
	RK_IOMUX(1, 2, 0x0018, 2),
	RK_IOMUX(1, 3, 0x001C, 2),
	RK_IOMUX(2, 0, 0x0020, 2),
	RK_IOMUX(2, 1, 0x0024, 3),
	RK_IOMUX(2, 2, 0x002c, 3),
	RK_IOMUX(2, 3, 0x0034, 2),
	RK_IOMUX(3, 0, 0x0038, 3),
	RK_IOMUX(3, 1, 0x0040, 3),
	RK_IOMUX(3, 2, 0x0048, 2),
	RK_IOMUX(3, 3, 0x004c, 2),
};

static struct rk_pinctrl_pin_fixup rk3328_pin_fixup[] = {
	/*      bank  pin reg  bit  mask */
	RK_PINFIX(2, 12, 0x24,  8, 0x300),
	RK_PINFIX(2, 15, 0x28,  0, 0x7),
	RK_PINFIX(2, 23, 0x30, 14, 0x6000),
};

static struct rk_pinctrl_pin_drive rk3328_pin_drive[] = {
	/*       bank sub  offs val ma */
	RK_PINDRIVE(0, 0, 0x200, 0, 2),
	RK_PINDRIVE(0, 0, 0x200, 1, 4),
	RK_PINDRIVE(0, 0, 0x200, 2, 8),
	RK_PINDRIVE(0, 0, 0x200, 3, 12),

	RK_PINDRIVE(0, 1, 0x204, 0, 2),
	RK_PINDRIVE(0, 1, 0x204, 1, 4),
	RK_PINDRIVE(0, 1, 0x204, 2, 8),
	RK_PINDRIVE(0, 1, 0x204, 3, 12),

	RK_PINDRIVE(0, 2, 0x208, 0, 2),
	RK_PINDRIVE(0, 2, 0x208, 1, 4),
	RK_PINDRIVE(0, 2, 0x208, 2, 8),
	RK_PINDRIVE(0, 2, 0x208, 3, 12),

	RK_PINDRIVE(0, 3, 0x20C, 0, 2),
	RK_PINDRIVE(0, 3, 0x20C, 1, 4),
	RK_PINDRIVE(0, 3, 0x20C, 2, 8),
	RK_PINDRIVE(0, 3, 0x20C, 3, 12),

	RK_PINDRIVE(1, 0, 0x210, 0, 2),
	RK_PINDRIVE(1, 0, 0x210, 1, 4),
	RK_PINDRIVE(1, 0, 0x210, 2, 8),
	RK_PINDRIVE(1, 0, 0x210, 3, 12),

	RK_PINDRIVE(1, 1, 0x214, 0, 2),
	RK_PINDRIVE(1, 1, 0x214, 1, 4),
	RK_PINDRIVE(1, 1, 0x214, 2, 8),
	RK_PINDRIVE(1, 1, 0x214, 3, 12),

	RK_PINDRIVE(1, 2, 0x218, 0, 2),
	RK_PINDRIVE(1, 2, 0x218, 1, 4),
	RK_PINDRIVE(1, 2, 0x218, 2, 8),
	RK_PINDRIVE(1, 2, 0x218, 3, 12),

	RK_PINDRIVE(1, 3, 0x21C, 0, 2),
	RK_PINDRIVE(1, 3, 0x21C, 1, 4),
	RK_PINDRIVE(1, 3, 0x21C, 2, 8),
	RK_PINDRIVE(1, 3, 0x21C, 3, 12),

	RK_PINDRIVE(2, 0, 0x220, 0, 2),
	RK_PINDRIVE(2, 0, 0x220, 1, 4),
	RK_PINDRIVE(2, 0, 0x220, 2, 8),
	RK_PINDRIVE(2, 0, 0x220, 3, 12),

	RK_PINDRIVE(2, 1, 0x224, 0, 2),
	RK_PINDRIVE(2, 1, 0x224, 1, 4),
	RK_PINDRIVE(2, 1, 0x224, 2, 8),
	RK_PINDRIVE(2, 1, 0x224, 3, 12),

	RK_PINDRIVE(2, 2, 0x228, 0, 2),
	RK_PINDRIVE(2, 2, 0x228, 1, 4),
	RK_PINDRIVE(2, 2, 0x228, 2, 8),
	RK_PINDRIVE(2, 2, 0x228, 3, 12),

	RK_PINDRIVE(2, 3, 0x22C, 0, 2),
	RK_PINDRIVE(2, 3, 0x22C, 1, 4),
	RK_PINDRIVE(2, 3, 0x22C, 2, 8),
	RK_PINDRIVE(2, 3, 0x22C, 3, 12),

	RK_PINDRIVE(3, 0, 0x230, 0, 2),
	RK_PINDRIVE(3, 0, 0x230, 1, 4),
	RK_PINDRIVE(3, 0, 0x230, 2, 8),
	RK_PINDRIVE(3, 0, 0x230, 3, 12),

	RK_PINDRIVE(3, 1, 0x234, 0, 2),
	RK_PINDRIVE(3, 1, 0x234, 1, 4),
	RK_PINDRIVE(3, 1, 0x234, 2, 8),
	RK_PINDRIVE(3, 1, 0x234, 3, 12),

	RK_PINDRIVE(3, 2, 0x238, 0, 2),
	RK_PINDRIVE(3, 2, 0x238, 1, 4),
	RK_PINDRIVE(3, 2, 0x238, 2, 8),
	RK_PINDRIVE(3, 2, 0x238, 3, 12),

	RK_PINDRIVE(3, 3, 0x23C, 0, 2),
	RK_PINDRIVE(3, 3, 0x23C, 1, 4),
	RK_PINDRIVE(3, 3, 0x23C, 2, 8),
	RK_PINDRIVE(3, 3, 0x23C, 3, 12),
};

static uint32_t
rk3328_get_pd_offset(struct rk_pinctrl_softc *sc, uint32_t bank)
{
	return (0x100);
}

static struct syscon *
rk3328_get_syscon(struct rk_pinctrl_softc *sc, uint32_t bank)
{
	return (sc->grf);
}

struct rk_pinctrl_conf rk3328_conf = {
	.iomux_conf = rk3328_iomux_bank,
	.iomux_nbanks = nitems(rk3328_iomux_bank),
	.pin_fixup = rk3328_pin_fixup,
	.npin_fixup = nitems(rk3328_pin_fixup),
	.pin_drive = rk3328_pin_drive,
	.npin_drive = nitems(rk3328_pin_drive),
	.gpio_bank = rk3328_gpio_bank,
	.ngpio_bank = nitems(rk3328_gpio_bank),
	.get_pd_offset = rk3328_get_pd_offset,
	.get_syscon = rk3328_get_syscon,
	.parse_bias = rk3288_parse_bias,
	.resolv_bias_value = rk3288_resolv_bias_value,
	.get_bias_value = rk3288_get_bias_value,
};

static struct rk_pinctrl_gpio rk3399_gpio_bank[] = {
	RK_GPIO(0, "gpio0"),
	RK_GPIO(1, "gpio1"),
	RK_GPIO(2, "gpio2"),
	RK_GPIO(3, "gpio3"),
	RK_GPIO(4, "gpio4"),
};

static struct rk_pinctrl_bank rk3399_iomux_bank[] = {
	/*    bank sub  offs   nbits */
	RK_IOMUX(0, 0, 0x0000, 2),
	RK_IOMUX(0, 1, 0x0004, 2),
	RK_IOMUX(0, 2, 0x0008, 2),
	RK_IOMUX(0, 3, 0x000C, 2),
	RK_IOMUX(1, 0, 0x0010, 2),
	RK_IOMUX(1, 1, 0x0014, 2),
	RK_IOMUX(1, 2, 0x0018, 2),
	RK_IOMUX(1, 3, 0x001C, 2),
	RK_IOMUX(2, 0, 0xE000, 2),
	RK_IOMUX(2, 1, 0xE004, 2),
	RK_IOMUX(2, 2, 0xE008, 2),
	RK_IOMUX(2, 3, 0xE00C, 2),
	RK_IOMUX(3, 0, 0xE010, 2),
	RK_IOMUX(3, 1, 0xE014, 2),
	RK_IOMUX(3, 2, 0xE018, 2),
	RK_IOMUX(3, 3, 0xE01C, 2),
	RK_IOMUX(4, 0, 0xE020, 2),
	RK_IOMUX(4, 1, 0xE024, 2),
	RK_IOMUX(4, 2, 0xE028, 2),
	RK_IOMUX(4, 3, 0xE02C, 2),
};

static struct rk_pinctrl_pin_fixup rk3399_pin_fixup[] = {};

static struct rk_pinctrl_pin_drive rk3399_pin_drive[] = {
	/*       bank sub offs val ma */
	/* GPIO0A */
	RK_PINDRIVE(0, 0, 0x80, 0, 5),
	RK_PINDRIVE(0, 0, 0x80, 1, 10),
	RK_PINDRIVE(0, 0, 0x80, 2, 15),
	RK_PINDRIVE(0, 0, 0x80, 3, 20),

	/* GPIOB */
	RK_PINDRIVE(0, 1, 0x88, 0, 5),
	RK_PINDRIVE(0, 1, 0x88, 1, 10),
	RK_PINDRIVE(0, 1, 0x88, 2, 15),
	RK_PINDRIVE(0, 1, 0x88, 3, 20),

	/* GPIO1A */
	RK_PINDRIVE(1, 0, 0xA0, 0, 3),
	RK_PINDRIVE(1, 0, 0xA0, 1, 6),
	RK_PINDRIVE(1, 0, 0xA0, 2, 9),
	RK_PINDRIVE(1, 0, 0xA0, 3, 12),

	/* GPIO1B */
	RK_PINDRIVE(1, 1, 0xA8, 0, 3),
	RK_PINDRIVE(1, 1, 0xA8, 1, 6),
	RK_PINDRIVE(1, 1, 0xA8, 2, 9),
	RK_PINDRIVE(1, 1, 0xA8, 3, 12),

	/* GPIO1C */
	RK_PINDRIVE(1, 2, 0xB0, 0, 3),
	RK_PINDRIVE(1, 2, 0xB0, 1, 6),
	RK_PINDRIVE(1, 2, 0xB0, 2, 9),
	RK_PINDRIVE(1, 2, 0xB0, 3, 12),

	/* GPIO1D */
	RK_PINDRIVE(1, 3, 0xB8, 0, 3),
	RK_PINDRIVE(1, 3, 0xB8, 1, 6),
	RK_PINDRIVE(1, 3, 0xB8, 2, 9),
	RK_PINDRIVE(1, 3, 0xB8, 3, 12),
};

static uint32_t
rk3399_get_pd_offset(struct rk_pinctrl_softc *sc, uint32_t bank)
{
	if (bank < 2)
		return (0x40);

	return (0xE040);
}

static struct syscon *
rk3399_get_syscon(struct rk_pinctrl_softc *sc, uint32_t bank)
{
	if (bank < 2)
		return (sc->pmu);

	return (sc->grf);
}

static int
rk3399_parse_bias(phandle_t node, int bank)
{
	int pullup, pulldown;

	if (OF_hasprop(node, "bias-disable"))
		return (0);

	switch (bank) {
	case 0:
	case 2:
		pullup = 3;
		pulldown = 1;
		break;
	case 1:
	case 3:
	case 4:
		pullup = 1;
		pulldown = 2;
		break;
	}

	if (OF_hasprop(node, "bias-pull-up"))
		return (pullup);
	if (OF_hasprop(node, "bias-pull-down"))
		return (pulldown);

	return (-1);
}

static int
rk3399_resolv_bias_value(int bank, int bias)
{
	int rv = 0;

	switch (bank) {
	case 0:
	case 2:
		if (bias == 3)
			rv = GPIO_PIN_PULLUP;
		else if (bias == 1)
			rv = GPIO_PIN_PULLDOWN;
		break;
	case 1:
	case 3:
	case 4:
		if (bias == 1)
			rv = GPIO_PIN_PULLUP;
		else if (bias == 2)
			rv = GPIO_PIN_PULLDOWN;
		break;
	}

	return (rv);
}

static int
rk3399_get_bias_value(int bank, int bias)
{
	int rv = 0;

	switch (bank) {
	case 0:
	case 2:
		if (bias & GPIO_PIN_PULLUP)
			rv = 3;
		else if (bias & GPIO_PIN_PULLDOWN)
			rv = 1;
		break;
	case 1:
	case 3:
	case 4:
		if (bias & GPIO_PIN_PULLUP)
			rv = 1;
		else if (bias & GPIO_PIN_PULLDOWN)
			rv = 2;
		break;
	}

	return (rv);
}

struct rk_pinctrl_conf rk3399_conf = {
	.iomux_conf = rk3399_iomux_bank,
	.iomux_nbanks = nitems(rk3399_iomux_bank),
	.pin_fixup = rk3399_pin_fixup,
	.npin_fixup = nitems(rk3399_pin_fixup),
	.pin_drive = rk3399_pin_drive,
	.npin_drive = nitems(rk3399_pin_drive),
	.gpio_bank = rk3399_gpio_bank,
	.ngpio_bank = nitems(rk3399_gpio_bank),
	.get_pd_offset = rk3399_get_pd_offset,
	.get_syscon = rk3399_get_syscon,
	.parse_bias = rk3399_parse_bias,
	.resolv_bias_value = rk3399_resolv_bias_value,
	.get_bias_value = rk3399_get_bias_value,
};

static struct rk_pinctrl_gpio rk3568_gpio_bank[] = {
	RK_GPIO(0, "gpio0"),
	RK_GPIO(1, "gpio1"),
	RK_GPIO(2, "gpio2"),
	RK_GPIO(3, "gpio3"),
	RK_GPIO(4, "gpio4"),
};

static struct rk_pinctrl_bank rk3568_iomux_bank[] = {
	/*    bank sub  offs   nbits */
	RK_IOMUX(0, 0, 0x0000, 4),	/* PMU_GRF */
	RK_IOMUX(0, 1, 0x0008, 4),
	RK_IOMUX(0, 2, 0x0010, 4),
	RK_IOMUX(0, 3, 0x0018, 4),

	RK_IOMUX(1, 0, 0x0000, 4),	/* SYS_GRF */
	RK_IOMUX(1, 1, 0x0008, 4),
	RK_IOMUX(1, 2, 0x0010, 4),
	RK_IOMUX(1, 3, 0x0018, 4),
	RK_IOMUX(2, 0, 0x0020, 4),
	RK_IOMUX(2, 1, 0x0028, 4),
	RK_IOMUX(2, 2, 0x0030, 4),
	RK_IOMUX(2, 3, 0x0038, 4),
	RK_IOMUX(3, 0, 0x0040, 4),
	RK_IOMUX(3, 1, 0x0048, 4),
	RK_IOMUX(3, 2, 0x0050, 4),
	RK_IOMUX(3, 3, 0x0058, 4),
	RK_IOMUX(4, 0, 0x0060, 4),
	RK_IOMUX(4, 1, 0x0068, 4),
	RK_IOMUX(4, 2, 0x0070, 4),
	RK_IOMUX(4, 3, 0x0078, 4),
};

static struct rk_pinctrl_pin_fixup rk3568_pin_fixup[] = {};

static struct rk_pinctrl_pin_drive rk3568_pin_drive[] = {
	/*       bank sub offs val ma */
	/* GPIO0A */
	RK_PINDRIVE(0, 0, 0x0020, 0, 2),
	RK_PINDRIVE(0, 0, 0x0020, 1, 4),
	RK_PINDRIVE(0, 0, 0x0020, 2, 8),
	RK_PINDRIVE(0, 0, 0x0020, 3, 12),

	/* GPIO0B */
	RK_PINDRIVE(0, 1, 0x0024, 0, 2),
	RK_PINDRIVE(0, 1, 0x0024, 1, 4),
	RK_PINDRIVE(0, 1, 0x0024, 2, 8),
	RK_PINDRIVE(0, 1, 0x0024, 3, 12),

	/* GPIO0C */
	RK_PINDRIVE(0, 1, 0x0028, 0, 2),
	RK_PINDRIVE(0, 1, 0x0028, 1, 4),
	RK_PINDRIVE(0, 1, 0x0028, 2, 8),
	RK_PINDRIVE(0, 1, 0x0028, 3, 12),

	/* GPIO0D */
	RK_PINDRIVE(0, 1, 0x002c, 0, 2),
	RK_PINDRIVE(0, 1, 0x002c, 1, 4),
	RK_PINDRIVE(0, 1, 0x002c, 2, 8),
	RK_PINDRIVE(0, 1, 0x002c, 3, 12),

	/* GPIO1A */
	RK_PINDRIVE(1, 0, 0x0080, 0, 2),
	RK_PINDRIVE(1, 0, 0x0080, 1, 4),
	RK_PINDRIVE(1, 0, 0x0080, 2, 8),
	RK_PINDRIVE(1, 0, 0x0080, 3, 12),

	/* GPIO1B */
	RK_PINDRIVE(1, 1, 0x0084, 0, 2),
	RK_PINDRIVE(1, 1, 0x0084, 1, 4),
	RK_PINDRIVE(1, 1, 0x0084, 2, 8),
	RK_PINDRIVE(1, 1, 0x0084, 3, 12),

	/* GPIO1C */
	RK_PINDRIVE(1, 2, 0x0088, 0, 2),
	RK_PINDRIVE(1, 2, 0x0088, 1, 4),
	RK_PINDRIVE(1, 2, 0x0088, 2, 8),
	RK_PINDRIVE(1, 2, 0x0088, 3, 12),

	/* GPIO1D */
	RK_PINDRIVE(1, 3, 0x008c, 0, 2),
	RK_PINDRIVE(1, 3, 0x008c, 1, 4),
	RK_PINDRIVE(1, 3, 0x008c, 2, 8),
	RK_PINDRIVE(1, 3, 0x008c, 3, 12),

	/* GPIO2A */
	RK_PINDRIVE(2, 0, 0x0090, 0, 2),
	RK_PINDRIVE(2, 0, 0x0090, 1, 4),
	RK_PINDRIVE(2, 0, 0x0090, 2, 8),
	RK_PINDRIVE(2, 0, 0x0090, 3, 12),

	/* GPIO2B */
	RK_PINDRIVE(2, 1, 0x0094, 0, 2),
	RK_PINDRIVE(2, 1, 0x0094, 1, 4),
	RK_PINDRIVE(2, 1, 0x0094, 2, 8),
	RK_PINDRIVE(2, 1, 0x0094, 3, 12),

	/* GPIO2C */
	RK_PINDRIVE(2, 2, 0x0098, 0, 2),
	RK_PINDRIVE(2, 2, 0x0098, 1, 4),
	RK_PINDRIVE(2, 2, 0x0098, 2, 8),
	RK_PINDRIVE(2, 2, 0x0098, 3, 12),

	/* GPIO2D */
	RK_PINDRIVE(2, 3, 0x009c, 0, 2),
	RK_PINDRIVE(2, 3, 0x009c, 1, 4),
	RK_PINDRIVE(2, 3, 0x009c, 2, 8),
	RK_PINDRIVE(2, 3, 0x009c, 3, 12),

	/* GPIO3A */
	RK_PINDRIVE(3, 0, 0x00a0, 0, 2),
	RK_PINDRIVE(3, 0, 0x00a0, 1, 4),
	RK_PINDRIVE(3, 0, 0x00a0, 2, 8),
	RK_PINDRIVE(3, 0, 0x00a0, 3, 12),

	/* GPIO3B */
	RK_PINDRIVE(3, 1, 0x00a4, 0, 2),
	RK_PINDRIVE(3, 1, 0x00a4, 1, 4),
	RK_PINDRIVE(3, 1, 0x00a4, 2, 8),
	RK_PINDRIVE(3, 1, 0x00a4, 3, 12),

	/* GPIO3C */
	RK_PINDRIVE(3, 2, 0x00a8, 0, 2),
	RK_PINDRIVE(3, 2, 0x00a8, 1, 4),
	RK_PINDRIVE(3, 2, 0x00a8, 2, 8),
	RK_PINDRIVE(3, 2, 0x00a8, 3, 12),

	/* GPIO3D */
	RK_PINDRIVE(3, 3, 0x00ac, 0, 2),
	RK_PINDRIVE(3, 3, 0x00ac, 1, 4),
	RK_PINDRIVE(3, 3, 0x00ac, 2, 8),
	RK_PINDRIVE(3, 3, 0x00ac, 3, 12),

	/* GPIO4A */
	RK_PINDRIVE(4, 0, 0x00b0, 0, 2),
	RK_PINDRIVE(4, 0, 0x00b0, 1, 4),
	RK_PINDRIVE(4, 0, 0x00b0, 2, 8),
	RK_PINDRIVE(4, 0, 0x00b0, 3, 12),

	/* GPIO4B */
	RK_PINDRIVE(4, 1, 0x00b4, 0, 2),
	RK_PINDRIVE(4, 1, 0x00b4, 1, 4),
	RK_PINDRIVE(4, 1, 0x00b4, 2, 8),
	RK_PINDRIVE(4, 1, 0x00b4, 3, 12),

	/* GPIO4C */
	RK_PINDRIVE(4, 2, 0x00b8, 0, 2),
	RK_PINDRIVE(4, 2, 0x00b8, 1, 4),
	RK_PINDRIVE(4, 2, 0x00b8, 2, 8),
	RK_PINDRIVE(4, 2, 0x00b8, 3, 12),

	/* GPIO4D */
	RK_PINDRIVE(4, 3, 0x00bc, 0, 2),
	RK_PINDRIVE(4, 3, 0x00bc, 1, 4),
	RK_PINDRIVE(4, 3, 0x00bc, 2, 8),
	RK_PINDRIVE(4, 3, 0x00bc, 3, 12),
};

static uint32_t
rk3568_get_pd_offset(struct rk_pinctrl_softc *sc, uint32_t bank)
{

	return (0);
}

static struct syscon *
rk3568_get_syscon(struct rk_pinctrl_softc *sc, uint32_t bank)
{

	if (bank)
		return (sc->grf);
	else
		return (sc->pmu);
}

static int
rk3568_parse_bias(phandle_t node, int bank)
{

	if (OF_hasprop(node, "bias-disable"))
		return (0);
	if (OF_hasprop(node, "bias-pull-up"))
		return (1);
	if (OF_hasprop(node, "bias-pull-down"))
		return (2);
	return (-1);
}

static int
rk3568_resolv_bias_value(int bank, int bias)
{

	if (bias == 1)
		return (GPIO_PIN_PULLUP);
	if (bias == 2)
		return (GPIO_PIN_PULLDOWN);
	return (0);
}

static int
rk3568_get_bias_value(int bank, int bias)
{

	if (bias & GPIO_PIN_PULLUP)
		return (1);
	if (bias & GPIO_PIN_PULLDOWN)
		return (2);
	return (0);
}

struct rk_pinctrl_conf rk3568_conf = {
	.iomux_conf = rk3568_iomux_bank,
	.iomux_nbanks = nitems(rk3568_iomux_bank),
	.pin_fixup = rk3568_pin_fixup,
	.npin_fixup = nitems(rk3568_pin_fixup),
	.pin_drive = rk3568_pin_drive,
	.npin_drive = nitems(rk3568_pin_drive),
	.gpio_bank = rk3568_gpio_bank,
	.ngpio_bank = nitems(rk3568_gpio_bank),
	.get_pd_offset = rk3568_get_pd_offset,
	.get_syscon = rk3568_get_syscon,
	.parse_bias = rk3568_parse_bias,
	.resolv_bias_value = rk3568_resolv_bias_value,
	.get_bias_value = rk3568_get_bias_value,
};

static struct ofw_compat_data compat_data[] = {
	{"rockchip,rk3288-pinctrl", (uintptr_t)&rk3288_conf},
	{"rockchip,rk3328-pinctrl", (uintptr_t)&rk3328_conf},
	{"rockchip,rk3399-pinctrl", (uintptr_t)&rk3399_conf},
	{"rockchip,rk3568-pinctrl", (uintptr_t)&rk3568_conf},
	{NULL,             0}
};

static int
rk_pinctrl_parse_drive(struct rk_pinctrl_softc *sc, phandle_t node,
  uint32_t bank, uint32_t subbank, uint32_t *drive, uint32_t *offset)
{
	uint32_t value;
	int i;

	if (OF_getencprop(node, "drive-strength", &value,
	    sizeof(value)) != 0)
		return (-1);

	/* Map to the correct drive value */
	for (i = 0; i < sc->conf->npin_drive; i++) {
		if (sc->conf->pin_drive[i].bank != bank &&
		    sc->conf->pin_drive[i].subbank != subbank)
			continue;
		if (sc->conf->pin_drive[i].ma == value) {
			*drive = sc->conf->pin_drive[i].value;
			return (0);
		}
	}

	return (-1);
}

static void
rk_pinctrl_get_fixup(struct rk_pinctrl_softc *sc, uint32_t bank, uint32_t pin,
    uint32_t *reg, uint32_t *mask, uint32_t *bit)
{
	int i;

	for (i = 0; i < sc->conf->npin_fixup; i++)
		if (sc->conf->pin_fixup[i].bank == bank &&
		    sc->conf->pin_fixup[i].pin == pin) {
			*reg = sc->conf->pin_fixup[i].reg;
			*mask = sc->conf->pin_fixup[i].mask;
			*bit = sc->conf->pin_fixup[i].bit;

			return;
		}
}

static int
rk_pinctrl_handle_io(struct rk_pinctrl_softc *sc, phandle_t node, uint32_t bank,
uint32_t pin)
{
	bool have_cfg, have_direction, have_value;
	uint32_t  direction_value, pin_value;
	struct rk_pinctrl_gpio *gpio;
	int i, rv;

	have_cfg = false;
	have_direction = false;
	have_value = false;

	/* Get (subset of) GPIO pin properties. */
	if (OF_hasprop(node, "output-disable")) {
		have_cfg = true;
		have_direction = true;
		direction_value = GPIO_PIN_INPUT;
	}

	if (OF_hasprop(node, "output-enable")) {
		have_cfg = true;
		have_direction = true;
		direction_value = GPIO_PIN_OUTPUT;
	}

	if (OF_hasprop(node, "output-low")) {
		have_cfg = true;
		have_direction = true;
		direction_value = GPIO_PIN_OUTPUT;
		have_value = true;
		pin_value = 0;
	}

	if (OF_hasprop(node, "output-high")) {
		have_cfg = true;
		have_direction = true;
		direction_value = GPIO_PIN_OUTPUT;
		have_value = true;
		pin_value = 1;
	}

	if (!have_cfg)
		return (0);

	/* Find gpio */
	gpio = NULL;
	for(i = 0; i < sc->conf->ngpio_bank; i++) {
		if (bank ==  sc->conf->gpio_bank[i].bank) {
			gpio = sc->conf->gpio_bank + i;
			break;
		}
	}
	if (gpio == NULL) {
		device_printf(sc->dev, "Cannot find GPIO bank %d\n", bank);
		return (ENXIO);
	}
	if (gpio->gpio_dev == NULL) {
		device_printf(sc->dev,
		    "No GPIO subdevice found for bank %d\n", bank);
		return (ENXIO);
	}

	rv = 0;
	if (have_value) {
		rv = GPIO_PIN_SET(gpio->gpio_dev, pin, pin_value);
		if (rv != 0) {
			device_printf(sc->dev, "Cannot set GPIO value: %d\n",
			    rv);
			return (rv);
		}
	}

	if (have_direction) {
		rv = GPIO_PIN_SETFLAGS(gpio->gpio_dev, pin, direction_value);
		if (rv != 0) {
			device_printf(sc->dev,
			    "Cannot set GPIO direction: %d\n", rv);
			return (rv);
		}
	}

	return (0);
}

static void
rk_pinctrl_configure_pin(struct rk_pinctrl_softc *sc, uint32_t *pindata)
{
	phandle_t pin_conf;
	struct syscon *syscon;
	uint32_t bank, subbank, pin, function, bias;
	uint32_t bit, mask, reg, drive;
	int i, rv;

	bank = pindata[0];
	pin = pindata[1];
	function = pindata[2];
	pin_conf = OF_node_from_xref(pindata[3]);
	subbank = pin / 8;

	for (i = 0; i < sc->conf->iomux_nbanks; i++)
		if (sc->conf->iomux_conf[i].bank == bank &&
		    sc->conf->iomux_conf[i].subbank == subbank)
			break;

	if (i == sc->conf->iomux_nbanks) {
		device_printf(sc->dev, "Unknown pin %d in bank %d\n", pin,
		    bank);
		return;
	}

	/* Find syscon */
	syscon = sc->conf->get_syscon(sc, bank);

	/* Setup GPIO properties first */
	rv = rk_pinctrl_handle_io(sc, pin_conf, bank, pin);

	/* Then pin pull-up/down */
	bias = sc->conf->parse_bias(pin_conf, bank);
	if (bias >= 0) {
		reg = sc->conf->get_pd_offset(sc, bank);
		reg += bank * 0x10 + ((pin / 8) * 0x4);
		bit = (pin % 8) * 2;
		mask = (0x3 << bit);
		SYSCON_MODIFY_4(syscon, reg, mask, bias << bit | (mask << 16));
	}

	/* Then drive strength */
	rv = rk_pinctrl_parse_drive(sc, pin_conf, bank, subbank, &drive, &reg);
	if (rv == 0) {
		bit = (pin % 8) * 2;
		mask = (0x3 << bit);
		SYSCON_MODIFY_4(syscon, reg, mask, drive << bit | (mask << 16));
	}

	/* Finally set the pin function */
	reg = sc->conf->iomux_conf[i].offset;
	switch (sc->conf->iomux_conf[i].nbits) {
	case 4:
		if ((pin % 8) >= 4)
			reg += 0x4;
		bit = (pin % 4) * 4;
		mask = (0xF << bit);
		break;
	case 3:
		if ((pin % 8) >= 5)
			reg += 4;
		bit = (pin % 8 % 5) * 3;
		mask = (0x7 << bit);
		break;
	case 2:
		bit = (pin % 8) * 2;
		mask = (0x3 << bit);
		break;
	default:
		device_printf(sc->dev,
		    "Unknown pin stride width %d in bank %d\n",
		    sc->conf->iomux_conf[i].nbits, bank);
		return;
	}
	rk_pinctrl_get_fixup(sc, bank, pin, &reg, &mask, &bit);

	/*
	 * NOTE: not all syscon registers uses hi-word write mask, thus
	 * register modify method should be used.
	 * XXXX We should not pass write mask to syscon register 
	 * without hi-word write mask.
	 */
	SYSCON_MODIFY_4(syscon, reg, mask, function << bit | (mask << 16));
}

static int
rk_pinctrl_configure_pins(device_t dev, phandle_t cfgxref)
{
	struct rk_pinctrl_softc *sc;
	phandle_t node;
	uint32_t *pins;
	int i, npins;

	sc = device_get_softc(dev);
	node = OF_node_from_xref(cfgxref);

	npins = OF_getencprop_alloc_multi(node, "rockchip,pins",  sizeof(*pins),
	    (void **)&pins);
	if (npins <= 0)
		return (ENOENT);

	for (i = 0; i != npins; i += 4)
		rk_pinctrl_configure_pin(sc, pins + i);

	return (0);
}

static int
rk_pinctrl_is_gpio_locked(struct rk_pinctrl_softc *sc, struct syscon *syscon,
  int bank, uint32_t pin, bool *is_gpio)
{
	uint32_t subbank, bit, mask, reg;
	uint32_t pinfunc;
	int i;

	RK_PINCTRL_LOCK_ASSERT(sc);

	subbank = pin / 8;
	*is_gpio = false;

	for (i = 0; i < sc->conf->iomux_nbanks; i++)
		if (sc->conf->iomux_conf[i].bank == bank &&
		    sc->conf->iomux_conf[i].subbank == subbank)
			break;

	if (i == sc->conf->iomux_nbanks) {
		device_printf(sc->dev, "Unknown pin %d in bank %d\n", pin,
		    bank);
		return (EINVAL);
	}

	syscon = sc->conf->get_syscon(sc, bank);

	/* Parse pin function */
	reg = sc->conf->iomux_conf[i].offset;
	switch (sc->conf->iomux_conf[i].nbits) {
	case 4:
		if ((pin % 8) >= 4)
			reg += 0x4;
		bit = (pin % 4) * 4;
		mask = (0xF << bit);
		break;
	case 3:
		if ((pin % 8) >= 5)
			reg += 4;
		bit = (pin % 8 % 5) * 3;
		mask = (0x7 << bit);
		break;
	case 2:
		bit = (pin % 8) * 2;
		mask = (0x3 << bit);
		break;
	default:
		device_printf(sc->dev,
		    "Unknown pin stride width %d in bank %d\n",
		    sc->conf->iomux_conf[i].nbits, bank);
		return (EINVAL);
	}
	rk_pinctrl_get_fixup(sc, bank, pin, &reg, &mask, &bit);

	reg = SYSCON_READ_4(syscon, reg);
	pinfunc = (reg & mask) >> bit;

	/* Test if the pin is in gpio mode */
	if (pinfunc == 0)
		*is_gpio = true;

	return (0);
}

static int
rk_pinctrl_get_bank(struct rk_pinctrl_softc *sc, device_t gpio, int *bank)
{
	int i;

	for (i = 0; i < sc->conf->ngpio_bank; i++) {
		if (sc->conf->gpio_bank[i].gpio_dev == gpio)
			break;
	}
	if (i == sc->conf->ngpio_bank)
		return (EINVAL);

	*bank = i;
	return (0);
}

static int
rk_pinctrl_is_gpio(device_t pinctrl, device_t gpio, uint32_t pin, bool *is_gpio)
{
	struct rk_pinctrl_softc *sc;
	struct syscon *syscon;
	int bank;
	int rv;

	sc = device_get_softc(pinctrl);
	RK_PINCTRL_LOCK(sc);

	rv = rk_pinctrl_get_bank(sc, gpio, &bank);
	if (rv != 0)
		goto done;
	syscon = sc->conf->get_syscon(sc, bank);
	rv = rk_pinctrl_is_gpio_locked(sc, syscon, bank, pin, is_gpio);

done:
	RK_PINCTRL_UNLOCK(sc);

	return (rv);
}

static int
rk_pinctrl_get_flags(device_t pinctrl, device_t gpio, uint32_t pin,
    uint32_t *flags)
{
	struct rk_pinctrl_softc *sc;
	struct syscon *syscon;
	uint32_t reg, bit;
	uint32_t bias;
	int bank;
	int rv = 0;
	bool is_gpio;

	sc = device_get_softc(pinctrl);
	RK_PINCTRL_LOCK(sc);

	rv = rk_pinctrl_get_bank(sc, gpio, &bank);
	if (rv != 0)
		goto done;
	syscon = sc->conf->get_syscon(sc, bank);
	rv = rk_pinctrl_is_gpio_locked(sc, syscon, bank, pin, &is_gpio);
	if (rv != 0)
		goto done;
	if (!is_gpio) {
		rv = EINVAL;
		goto done;
	}
	/* Get the pullup/pulldown configuration */
	reg = sc->conf->get_pd_offset(sc, bank);
	reg += bank * 0x10 + ((pin / 8) * 0x4);
	bit = (pin % 8) * 2;
	reg = SYSCON_READ_4(syscon, reg);
	reg = (reg >> bit) & 0x3;
	bias = sc->conf->resolv_bias_value(bank, reg);
	*flags = bias;

done:
	RK_PINCTRL_UNLOCK(sc);
	return (rv);
}

static int
rk_pinctrl_set_flags(device_t pinctrl, device_t gpio, uint32_t pin,
    uint32_t flags)
{
	struct rk_pinctrl_softc *sc;
	struct syscon *syscon;
	uint32_t bit, mask, reg;
	uint32_t bias;
	int bank;
	int rv = 0;
	bool is_gpio;

	sc = device_get_softc(pinctrl);
	RK_PINCTRL_LOCK(sc);

	rv = rk_pinctrl_get_bank(sc, gpio, &bank);
	if (rv != 0)
		goto done;
	syscon = sc->conf->get_syscon(sc, bank);
	rv = rk_pinctrl_is_gpio_locked(sc, syscon, bank, pin, &is_gpio);
	if (rv != 0)
		goto done;
	if (!is_gpio) {
		rv = EINVAL;
		goto done;
	}
	/* Get the pullup/pulldown configuration */
	reg = sc->conf->get_pd_offset(sc, bank);
	reg += bank * 0x10 + ((pin / 8) * 0x4);
	bit = (pin % 8) * 2;
	mask = (0x3 << bit);
	bias = sc->conf->get_bias_value(bank, flags);
	SYSCON_MODIFY_4(syscon, reg, mask, bias << bit | (mask << 16));

done:
	RK_PINCTRL_UNLOCK(sc);
	return (rv);
}

static int
rk_pinctrl_register_gpio(struct rk_pinctrl_softc *sc, char *gpio_name,
    device_t gpio_dev)
{
	int i;

	for(i = 0; i < sc->conf->ngpio_bank; i++) {
		if (strcmp(gpio_name, sc->conf->gpio_bank[i].gpio_name) != 0)
			continue;
		sc->conf->gpio_bank[i].gpio_dev = gpio_dev;
		return(0);
	}
	return (ENXIO);
}

static int
rk_pinctrl_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "RockChip Pinctrl controller");
	return (BUS_PROBE_DEFAULT);
}

static int
rk_pinctrl_attach(device_t dev)
{
	struct rk_pinctrl_softc *sc;
	phandle_t node;
	device_t cdev;
	char *gpio_name, *eptr;
	int rv;

	sc = device_get_softc(dev);
	sc->dev = dev;

	node = ofw_bus_get_node(dev);

	if (OF_hasprop(node, "rockchip,grf") &&
	    syscon_get_by_ofw_property(dev, node,
	    "rockchip,grf", &sc->grf) != 0) {
		device_printf(dev, "cannot get grf driver handle\n");
		return (ENXIO);
	}

	/* RK3568,RK3399,RK3288 have banks in PMU. RK3328 doesn't have a PMU. */
	if (ofw_bus_node_is_compatible(node, "rockchip,rk3568-pinctrl") ||
	    ofw_bus_node_is_compatible(node, "rockchip,rk3399-pinctrl") ||
	    ofw_bus_node_is_compatible(node, "rockchip,rk3288-pinctrl")) {
		if (OF_hasprop(node, "rockchip,pmu") &&
		    syscon_get_by_ofw_property(dev, node,
		    "rockchip,pmu", &sc->pmu) != 0) {
			device_printf(dev, "cannot get pmu driver handle\n");
			return (ENXIO);
		}
	}

	mtx_init(&sc->mtx, "rk pinctrl", "pinctrl", MTX_SPIN);

	sc->conf = (struct rk_pinctrl_conf *)ofw_bus_search_compatible(dev,
	    compat_data)->ocd_data;

	fdt_pinctrl_register(dev, "rockchip,pins");

	simplebus_init(dev, node);

	bus_generic_probe(dev);

	/* Attach child devices */
	for (node = OF_child(node); node > 0; node = OF_peer(node)) {
		if (!ofw_bus_node_is_compatible(node, "rockchip,gpio-bank"))
			continue;

		rv = OF_getprop_alloc(node, "name", (void **)&gpio_name);
		if (rv <= 0) {
			device_printf(sc->dev, "Cannot GPIO subdevice name.\n");
			continue;
		}

		cdev = simplebus_add_device(dev, node, 0, NULL, -1, NULL);
		if (cdev == NULL) {
			device_printf(dev, " Cannot add GPIO subdevice: %s\n",
			    gpio_name);
			OF_prop_free(gpio_name);
			continue;
		}

		rv = device_probe_and_attach(cdev);
		if (rv != 0) {
			device_printf(sc->dev,
			    "Cannot attach GPIO subdevice: %s\n", gpio_name);
			OF_prop_free(gpio_name);
			continue;
		}

		/* Grep device name from name property */
		eptr = gpio_name;
		strsep(&eptr, "@");
		if (gpio_name == eptr) {
			device_printf(sc->dev,
			    "Unrecognized format of GPIO subdevice name: %s\n",
			    gpio_name);
			OF_prop_free(gpio_name);
			continue;
		}
		rv =  rk_pinctrl_register_gpio(sc, gpio_name, cdev);
		if (rv != 0) {
			device_printf(sc->dev,
			    "Cannot register GPIO subdevice %s: %d\n",
			    gpio_name, rv);
			OF_prop_free(gpio_name);
			continue;
		}
		OF_prop_free(gpio_name);
	}

	fdt_pinctrl_configure_tree(dev);

	return (bus_generic_attach(dev));
}

static int
rk_pinctrl_detach(device_t dev)
{

	return (EBUSY);
}

static device_method_t rk_pinctrl_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			rk_pinctrl_probe),
	DEVMETHOD(device_attach,		rk_pinctrl_attach),
	DEVMETHOD(device_detach,		rk_pinctrl_detach),

        /* fdt_pinctrl interface */
	DEVMETHOD(fdt_pinctrl_configure,	rk_pinctrl_configure_pins),
	DEVMETHOD(fdt_pinctrl_is_gpio,		rk_pinctrl_is_gpio),
	DEVMETHOD(fdt_pinctrl_get_flags,	rk_pinctrl_get_flags),
	DEVMETHOD(fdt_pinctrl_set_flags,	rk_pinctrl_set_flags),

	DEVMETHOD_END
};

DEFINE_CLASS_1(rk_pinctrl, rk_pinctrl_driver, rk_pinctrl_methods,
    sizeof(struct rk_pinctrl_softc), simplebus_driver);

EARLY_DRIVER_MODULE(rk_pinctrl, simplebus, rk_pinctrl_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(rk_pinctrl, 1);
