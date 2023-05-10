/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Oskar Holmlund <oskar.holmlund@ohdata.se>
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/malloc.h>

#include <dev/extres/clk/clk.h>

#include <arm/ti/clk/ti_clk_clkctrl.h>

#include "clkdev_if.h"

#if 0
#define DPRINTF(dev, msg...) device_printf(dev, msg)
#else
#define DPRINTF(dev, msg...)
#endif

/*
 * clknode for clkctrl, implements gate and mux (for gpioc)
 */

#define GPIO_X_GDBCLK_MASK	0x00040000
#define IDLEST_MASK		0x00030000
#define MODULEMODE_MASK		0x00000003

#define GPIOX_GDBCLK_ENABLE	0x00040000
#define GPIOX_GDBCLK_DISABLE	0x00000000
#define IDLEST_FUNC		0x00000000
#define IDLEST_TRANS		0x00010000
#define IDLEST_IDLE		0x00020000
#define IDLEST_DISABLE		0x00030000

#define MODULEMODE_DISABLE	0x0
#define MODULEMODE_ENABLE	0x2

struct ti_clkctrl_clknode_sc {
	device_t	dev;
	bool		gdbclk;
	/* omap4-cm range.host + ti,clkctrl reg[0] */
	uint32_t	register_offset;
};

#define	WRITE4(_clk, off, val)						\
	CLKDEV_WRITE_4(clknode_get_device(_clk), off, val)
#define	READ4(_clk, off, val)						\
	CLKDEV_READ_4(clknode_get_device(_clk), off, val)
#define	DEVICE_LOCK(_clk)						\
	CLKDEV_DEVICE_LOCK(clknode_get_device(_clk))
#define	DEVICE_UNLOCK(_clk)						\
	CLKDEV_DEVICE_UNLOCK(clknode_get_device(_clk))

static int
ti_clkctrl_init(struct clknode *clk, device_t dev)
{
	struct ti_clkctrl_clknode_sc *sc;

	sc = clknode_get_softc(clk);
	sc->dev = dev;

	clknode_init_parent_idx(clk, 0);
	return (0);
}

static int
ti_clkctrl_set_gdbclk_gate(struct clknode *clk, bool enable)
{
	struct ti_clkctrl_clknode_sc *sc;
	uint32_t val, gpio_x_gdbclk;
	uint32_t timeout = 100;

	sc = clknode_get_softc(clk);

	READ4(clk, sc->register_offset, &val);
	DPRINTF(sc->dev, "val(%x) & (%x | %x = %x)\n",
	    val, GPIO_X_GDBCLK_MASK, MODULEMODE_MASK,
	    GPIO_X_GDBCLK_MASK | MODULEMODE_MASK);

	if (enable) {
		val = val & MODULEMODE_MASK;
		val |= GPIOX_GDBCLK_ENABLE;
	} else {
		val = val & MODULEMODE_MASK;
		val |= GPIOX_GDBCLK_DISABLE;
	}

	DPRINTF(sc->dev, "val %x\n", val);
	WRITE4(clk, sc->register_offset, val);

	/* Wait */
	while (timeout) {
		READ4(clk, sc->register_offset, &val);
		gpio_x_gdbclk = val & GPIO_X_GDBCLK_MASK;
		if (enable && (gpio_x_gdbclk == GPIOX_GDBCLK_ENABLE))
			break;
		else if (!enable && (gpio_x_gdbclk == GPIOX_GDBCLK_DISABLE))
			break;
		DELAY(10);
		timeout--;
	}
	if (timeout == 0) {
		device_printf(sc->dev, "ti_clkctrl_set_gdbclk_gate: Timeout\n");
		return (1);
	}

	return (0);
}

static int
ti_clkctrl_set_gate(struct clknode *clk, bool enable)
{
	struct ti_clkctrl_clknode_sc *sc;
	uint32_t	val, idlest, module;
	uint32_t timeout=100;
	int err;

	sc = clknode_get_softc(clk);

	if (sc->gdbclk) {
		err = ti_clkctrl_set_gdbclk_gate(clk, enable);
		return (err);
	}

	READ4(clk, sc->register_offset, &val);

	if (enable)
		WRITE4(clk, sc->register_offset, MODULEMODE_ENABLE);
	else
		WRITE4(clk, sc->register_offset, MODULEMODE_DISABLE);

	while (timeout) {
		READ4(clk, sc->register_offset, &val);
		idlest = val & IDLEST_MASK;
		module = val & MODULEMODE_MASK;
		if (enable &&
		    (idlest == IDLEST_FUNC || idlest == IDLEST_TRANS) &&
		    module == MODULEMODE_ENABLE)
			break;
		else if (!enable &&
		    idlest == IDLEST_DISABLE &&
		    module == MODULEMODE_DISABLE)
			break;
		DELAY(10);
		timeout--;
	}

	if (timeout == 0) {
		device_printf(sc->dev, "ti_clkctrl_set_gate: Timeout\n");
		return (1);
	}

	return (0);
}

static clknode_method_t ti_clkctrl_clknode_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,	ti_clkctrl_init),
	CLKNODEMETHOD(clknode_set_gate, ti_clkctrl_set_gate),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(ti_clkctrl_clknode, ti_clkctrl_clknode_class,
    ti_clkctrl_clknode_methods, sizeof(struct ti_clkctrl_clknode_sc),
    clknode_class);

int
ti_clknode_clkctrl_register(struct clkdom *clkdom,
    struct ti_clk_clkctrl_def *clkdef)
{
	struct clknode *clk;
	struct ti_clkctrl_clknode_sc *sc;

	clk = clknode_create(clkdom, &ti_clkctrl_clknode_class,
	    &clkdef->clkdef);

	if (clk == NULL) {
		return (1);
	}

	sc = clknode_get_softc(clk);
	sc->register_offset = clkdef->register_offset;
	sc->gdbclk = clkdef->gdbclk;

	if (clknode_register(clkdom, clk) == NULL) {
		return (2);
	}
	return (0);
}
