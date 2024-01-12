/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/resource.h>
#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/clk/clk.h>

#include "pwmbus_if.h"

#define	AW_PWM_CTRL			0x00
#define	 AW_PWM_CTRL_PRESCALE_MASK	0xF
#define	 AW_PWM_CTRL_EN			(1 << 4)
#define	 AW_PWM_CTRL_ACTIVE_LEVEL_HIGH	(1 << 5)
#define	 AW_PWM_CTRL_GATE		(1 << 6)
#define	 AW_PWM_CTRL_MODE_MASK		0x80
#define	 AW_PWM_CTRL_PULSE_MODE		(1 << 7)
#define	 AW_PWM_CTRL_CYCLE_MODE		(0 << 7)
#define	 AW_PWM_CTRL_PULSE_START	(1 << 8)
#define	 AW_PWM_CTRL_CLK_BYPASS		(1 << 9)
#define	 AW_PWM_CTRL_PERIOD_BUSY	(1 << 28)

#define	AW_PWM_PERIOD			0x04
#define	AW_PWM_PERIOD_TOTAL_MASK	0xFFFF
#define	AW_PWM_PERIOD_TOTAL_SHIFT	16
#define	AW_PWM_PERIOD_ACTIVE_MASK	0xFFFF
#define	AW_PWM_PERIOD_ACTIVE_SHIFT	0

#define	AW_PWM_MAX_FREQ			24000000

#define	NS_PER_SEC	1000000000

static struct ofw_compat_data compat_data[] = {
	{ "allwinner,sun5i-a13-pwm",		1 },
	{ "allwinner,sun8i-h3-pwm",		1 },
	{ NULL,					0 }
};

static struct resource_spec aw_pwm_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

struct aw_pwm_softc {
	device_t	dev;
	device_t	busdev;
	clk_t		clk;
	struct resource	*res;

	uint64_t	clk_freq;
	unsigned int	period;
	unsigned int	duty;
	uint32_t	flags;
	bool		enabled;
};

static uint32_t aw_pwm_clk_prescaler[] = {
	120,
	180,
	240,
	360,
	480,
	0,
	0,
	0,
	12000,
	24000,
	36000,
	48000,
	72000,
	0,
	0,
	1,
};

#define	AW_PWM_READ(sc, reg)		bus_read_4((sc)->res, (reg))
#define	AW_PWM_WRITE(sc, reg, val)	bus_write_4((sc)->res, (reg), (val))

static int aw_pwm_probe(device_t dev);
static int aw_pwm_attach(device_t dev);
static int aw_pwm_detach(device_t dev);

static int
aw_pwm_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "Allwinner PWM");
	return (BUS_PROBE_DEFAULT);
}

static int
aw_pwm_attach(device_t dev)
{
	struct aw_pwm_softc *sc;
	uint64_t clk_freq;
	uint32_t reg;
	phandle_t node;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;

	error = clk_get_by_ofw_index(dev, 0, 0, &sc->clk);
	if (error != 0) {
		device_printf(dev, "cannot get clock\n");
		goto fail;
	}
	error = clk_enable(sc->clk);
	if (error != 0) {
		device_printf(dev, "cannot enable clock\n");
		goto fail;
	}

	error = clk_get_freq(sc->clk, &sc->clk_freq);
	if (error != 0) {
		device_printf(dev, "cannot get clock frequency\n");
		goto fail;
	}

	if (bus_alloc_resources(dev, aw_pwm_spec, &sc->res) != 0) {
		device_printf(dev, "cannot allocate resources for device\n");
		error = ENXIO;
		goto fail;
	}

	/* Read the configuration left by U-Boot */
	reg = AW_PWM_READ(sc, AW_PWM_CTRL);
	if (reg & (AW_PWM_CTRL_GATE | AW_PWM_CTRL_EN))
		sc->enabled = true;

	reg = AW_PWM_READ(sc, AW_PWM_CTRL);
	reg &= AW_PWM_CTRL_PRESCALE_MASK;
	if (reg > nitems(aw_pwm_clk_prescaler)) {
		device_printf(dev, "Bad prescaler %x, cannot guess current settings\n", reg);
		goto skipcfg;
	}
	clk_freq = sc->clk_freq / aw_pwm_clk_prescaler[reg];

	reg = AW_PWM_READ(sc, AW_PWM_PERIOD);
	sc->period = NS_PER_SEC /
		(clk_freq / ((reg >> AW_PWM_PERIOD_TOTAL_SHIFT) & AW_PWM_PERIOD_TOTAL_MASK));
	sc->duty = NS_PER_SEC /
		(clk_freq / ((reg >> AW_PWM_PERIOD_ACTIVE_SHIFT) & AW_PWM_PERIOD_ACTIVE_MASK));

skipcfg:
	/*
	 * Note that we don't check for failure to attach pwmbus -- even without
	 * it we can still service clients who connect via fdt xref data.
	 */
	node = ofw_bus_get_node(dev);
	OF_device_register_xref(OF_xref_from_node(node), dev);

	sc->busdev = device_add_child(dev, "pwmbus", -1);

	return (bus_generic_attach(dev));

fail:
	aw_pwm_detach(dev);
	return (error);
}

static int
aw_pwm_detach(device_t dev)
{
	struct aw_pwm_softc *sc;
	int error;

	sc = device_get_softc(dev);

	if ((error = bus_generic_detach(sc->dev)) != 0) {
		device_printf(sc->dev, "cannot detach child devices\n");
		return (error);
	}

	if (sc->busdev != NULL)
		device_delete_child(dev, sc->busdev);

	if (sc->res != NULL)
		bus_release_resources(dev, aw_pwm_spec, &sc->res);

	return (0);
}

static phandle_t
aw_pwm_get_node(device_t bus, device_t dev)
{

	/*
	 * Share our controller node with our pwmbus child; it instantiates
	 * devices by walking the children contained within our node.
	 */
	return ofw_bus_get_node(bus);
}

static int
aw_pwm_channel_count(device_t dev, u_int *nchannel)
{

	*nchannel = 1;

	return (0);
}

static int
aw_pwm_channel_config(device_t dev, u_int channel, u_int period, u_int duty)
{
	struct aw_pwm_softc *sc;
	uint64_t period_freq, duty_freq;
	uint64_t clk_rate, div;
	uint32_t reg;
	int prescaler;
	int i;

	sc = device_get_softc(dev);

	period_freq = NS_PER_SEC / period;
	if (period_freq > AW_PWM_MAX_FREQ)
		return (EINVAL);

	/*
	 * FIXME.  The hardware is capable of sub-Hz frequencies, that is,
	 * periods longer than a second.  But the current code cannot deal
	 * with those properly.
	 */
	if (period_freq == 0)
		return (EINVAL);

	/*
	 * FIXME.  There is a great loss of precision when the period and the
	 * duty are near 1 second.  In some cases period_freq and duty_freq can
	 * be equal even if the period and the duty are significantly different.
	 */
	duty_freq = NS_PER_SEC / duty;
	if (duty_freq < period_freq) {
		device_printf(sc->dev, "duty < period\n");
		return (EINVAL);
	}

	/* First test without prescaler */
	clk_rate = AW_PWM_MAX_FREQ;
	prescaler = AW_PWM_CTRL_PRESCALE_MASK;
	div = AW_PWM_MAX_FREQ / period_freq;
	if ((div - 1) > AW_PWM_PERIOD_TOTAL_MASK) {
		/* Test all prescaler */
		for (i = 0; i < nitems(aw_pwm_clk_prescaler); i++) {
			if (aw_pwm_clk_prescaler[i] == 0)
				continue;
			div = AW_PWM_MAX_FREQ / aw_pwm_clk_prescaler[i] / period_freq;
			if ((div - 1) < AW_PWM_PERIOD_TOTAL_MASK ) {
				prescaler = i;
				clk_rate = AW_PWM_MAX_FREQ / aw_pwm_clk_prescaler[i];
				break;
			}
		}
		if (prescaler == AW_PWM_CTRL_PRESCALE_MASK)
			return (EINVAL);
	}

	reg = AW_PWM_READ(sc, AW_PWM_CTRL);

	/* Write the prescalar */
	reg &= ~AW_PWM_CTRL_PRESCALE_MASK;
	reg |= prescaler;

	reg &= ~AW_PWM_CTRL_MODE_MASK;
	reg |= AW_PWM_CTRL_CYCLE_MODE;

	reg &= ~AW_PWM_CTRL_PULSE_START;
	reg &= ~AW_PWM_CTRL_CLK_BYPASS;

	AW_PWM_WRITE(sc, AW_PWM_CTRL, reg);

	/* Write the total/active cycles */
	reg = ((clk_rate / period_freq - 1) << AW_PWM_PERIOD_TOTAL_SHIFT) |
	  ((clk_rate / duty_freq) << AW_PWM_PERIOD_ACTIVE_SHIFT);
	AW_PWM_WRITE(sc, AW_PWM_PERIOD, reg);

	sc->period = period;
	sc->duty = duty;

	return (0);
}

static int
aw_pwm_channel_get_config(device_t dev, u_int channel, u_int *period, u_int *duty)
{
	struct aw_pwm_softc *sc;

	sc = device_get_softc(dev);

	*period = sc->period;
	*duty = sc->duty;

	return (0);
}

static int
aw_pwm_channel_enable(device_t dev, u_int channel, bool enable)
{
	struct aw_pwm_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	if (enable && sc->enabled)
		return (0);

	reg = AW_PWM_READ(sc, AW_PWM_CTRL);
	if (enable)
		reg |= AW_PWM_CTRL_GATE | AW_PWM_CTRL_EN;
	else
		reg &= ~(AW_PWM_CTRL_GATE | AW_PWM_CTRL_EN);

	AW_PWM_WRITE(sc, AW_PWM_CTRL, reg);

	sc->enabled = enable;

	return (0);
}

static int
aw_pwm_channel_is_enabled(device_t dev, u_int channel, bool *enabled)
{
	struct aw_pwm_softc *sc;

	sc = device_get_softc(dev);

	*enabled = sc->enabled;

	return (0);
}

static device_method_t aw_pwm_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		aw_pwm_probe),
	DEVMETHOD(device_attach,	aw_pwm_attach),
	DEVMETHOD(device_detach,	aw_pwm_detach),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	aw_pwm_get_node),

	/* pwmbus interface */
	DEVMETHOD(pwmbus_channel_count,		aw_pwm_channel_count),
	DEVMETHOD(pwmbus_channel_config,	aw_pwm_channel_config),
	DEVMETHOD(pwmbus_channel_get_config,	aw_pwm_channel_get_config),
	DEVMETHOD(pwmbus_channel_enable,	aw_pwm_channel_enable),
	DEVMETHOD(pwmbus_channel_is_enabled,	aw_pwm_channel_is_enabled),

	DEVMETHOD_END
};

static driver_t aw_pwm_driver = {
	"pwm",
	aw_pwm_methods,
	sizeof(struct aw_pwm_softc),
};

DRIVER_MODULE(aw_pwm, simplebus, aw_pwm_driver, 0, 0);
MODULE_VERSION(aw_pwm, 1);
SIMPLEBUS_PNP_INFO(compat_data);
