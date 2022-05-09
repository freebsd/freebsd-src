/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018 Emmanuel Vadot <manu@FreeBSD.org>
 * Copyright (c) 2019 Brandon Bergren <git@bdragon.rtk0.net>
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
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/resource.h>
#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk.h>

#include "pwmbus_if.h"

/* Register offsets. */
#define	RK_PWM_COUNTER			0x00
#define	RK_PWM_PERIOD			0x04
#define	RK_PWM_DUTY			0x08
#define	RK_PWM_CTRL			0x0c

#define	SET(reg,mask,val)		reg = ((reg & ~mask) | val)

#define	RK_PWM_CTRL_ENABLE_MASK		(1 << 0)
#define	 RK_PWM_CTRL_ENABLED		(1 << 0)
#define	 RK_PWM_CTRL_DISABLED		(0)

#define	RK_PWM_CTRL_MODE_MASK		(3 << 1)
#define	 RK_PWM_CTRL_MODE_ONESHOT	(0)
#define	 RK_PWM_CTRL_MODE_CONTINUOUS	(1 << 1)
#define	 RK_PWM_CTRL_MODE_CAPTURE	(1 << 2)

#define	RK_PWM_CTRL_DUTY_MASK		(1 << 3)
#define	 RK_PWM_CTRL_DUTY_POSITIVE	(1 << 3)
#define	 RK_PWM_CTRL_DUTY_NEGATIVE	(0)

#define	RK_PWM_CTRL_INACTIVE_MASK	(1 << 4)
#define	 RK_PWM_CTRL_INACTIVE_POSITIVE	(1 << 4)
#define	 RK_PWM_CTRL_INACTIVE_NEGATIVE	(0)

/* PWM Output Alignment */
#define	RK_PWM_CTRL_ALIGN_MASK		(1 << 5)
#define	 RK_PWM_CTRL_ALIGN_CENTER	(1 << 5)
#define	 RK_PWM_CTRL_ALIGN_LEFT		(0)

/* Low power mode: disable prescaler when inactive */
#define	RK_PWM_CTRL_LP_MASK		(1 << 8)
#define	 RK_PWM_CTRL_LP_ENABLE		(1 << 8)
#define	 RK_PWM_CTRL_LP_DISABLE		(0)

/* Clock source: bypass the scaler or not */
#define	RK_PWM_CTRL_CLOCKSRC_MASK	(1 << 9)
#define	 RK_PWM_CTRL_CLOCKSRC_NONSCALED	(0)
#define	 RK_PWM_CTRL_CLOCKSRC_SCALED	(1 << 9)

#define	RK_PWM_CTRL_PRESCALE_MASK	(7 << 12)
#define	RK_PWM_CTRL_PRESCALE_SHIFT	12

#define	RK_PWM_CTRL_SCALE_MASK		(0xFF << 16)
#define	RK_PWM_CTRL_SCALE_SHIFT		16

#define	RK_PWM_CTRL_REPEAT_MASK		(0xFF << 24)
#define	RK_PWM_CTRL_REPEAT_SHIFT	24

#define	NS_PER_SEC	1000000000

static struct ofw_compat_data compat_data[] = {
	{ "rockchip,rk3288-pwm",		1 },
	{ "rockchip,rk3399-pwm",		1 },
	{ NULL,					0 }
};

static struct resource_spec rk_pwm_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

struct rk_pwm_softc {
	device_t	dev;
	device_t	busdev;
	clk_t		clk;
	struct resource	*res;

	uint64_t	clk_freq;
	unsigned int	period;
	unsigned int	duty;
	uint32_t	flags;
	uint8_t		prescaler;
	uint8_t		scaler;
	bool		using_scaler;
	bool		enabled;
};

#define	RK_PWM_READ(sc, reg)		bus_read_4((sc)->res, (reg))
#define	RK_PWM_WRITE(sc, reg, val)	bus_write_4((sc)->res, (reg), (val))

static int rk_pwm_probe(device_t dev);
static int rk_pwm_attach(device_t dev);
static int rk_pwm_detach(device_t dev);

static int
rk_pwm_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "Rockchip PWM");
	return (BUS_PROBE_DEFAULT);
}

static int
rk_pwm_attach(device_t dev)
{
	struct rk_pwm_softc *sc;
	phandle_t node;
	uint64_t clk_freq;
	uint32_t reg;
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
		device_printf(dev, "cannot get base frequency\n");
		goto fail;
	}

	if (bus_alloc_resources(dev, rk_pwm_spec, &sc->res) != 0) {
		device_printf(dev, "cannot allocate resources for device\n");
		error = ENXIO;
		goto fail;
	}

	/* Read the configuration left by U-Boot */
	reg = RK_PWM_READ(sc, RK_PWM_CTRL);
	if ((reg & RK_PWM_CTRL_ENABLE_MASK) == RK_PWM_CTRL_ENABLED)
		sc->enabled = true;

	reg = RK_PWM_READ(sc, RK_PWM_CTRL);
	reg &= RK_PWM_CTRL_PRESCALE_MASK;
	sc->prescaler = reg >> RK_PWM_CTRL_PRESCALE_SHIFT;

	reg = RK_PWM_READ(sc, RK_PWM_CTRL);
	reg &= RK_PWM_CTRL_SCALE_MASK;
	sc->scaler = reg >> RK_PWM_CTRL_SCALE_SHIFT;

	reg = RK_PWM_READ(sc, RK_PWM_CTRL);
	if ((reg & RK_PWM_CTRL_CLOCKSRC_MASK) == RK_PWM_CTRL_CLOCKSRC_SCALED)
		sc->using_scaler = true;
	else
		sc->using_scaler = false;

	clk_freq = sc->clk_freq / (2 ^ sc->prescaler);

	if (sc->using_scaler) {
		if (sc->scaler == 0)
			clk_freq /= 512;
		else
			clk_freq /= (sc->scaler * 2);
	}

	reg = RK_PWM_READ(sc, RK_PWM_PERIOD);
	sc->period = NS_PER_SEC /
		(clk_freq / reg);
	reg = RK_PWM_READ(sc, RK_PWM_DUTY);
	sc->duty = NS_PER_SEC /
		(clk_freq / reg);

	node = ofw_bus_get_node(dev);
	OF_device_register_xref(OF_xref_from_node(node), dev);

	sc->busdev = device_add_child(dev, "pwmbus", -1);

	return (bus_generic_attach(dev));

fail:
	rk_pwm_detach(dev);
	return (error);
}

static int
rk_pwm_detach(device_t dev)
{
	struct rk_pwm_softc *sc;

	sc = device_get_softc(dev);

	bus_generic_detach(sc->dev);

	bus_release_resources(dev, rk_pwm_spec, &sc->res);

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
rk_pwm_channel_count(device_t dev, u_int *nchannel)
{
	/* The device supports 4 channels, but attaches multiple times in the
	 * device tree. This interferes with advanced usage though, as
	 * the interrupt capability and channel 3 FIFO register offsets
	 * don't work right in this situation.
	 * But since we don't support those yet, pretend we are singlechannel.
	 */
	*nchannel = 1;

	return (0);
}

static int
rk_pwm_channel_config(device_t dev, u_int channel, u_int period, u_int duty)
{
	struct rk_pwm_softc *sc;
	uint64_t period_freq, duty_freq;
	uint32_t reg;
	uint32_t period_out;
	uint32_t duty_out;
	uint8_t prescaler;
	uint8_t scaler;
	bool using_scaler;

	sc = device_get_softc(dev);

	period_freq = NS_PER_SEC / period;
	/* Datasheet doesn't define, so use Nyquist frequency. */
	if (period_freq > (sc->clk_freq / 2))
		return (EINVAL);
	duty_freq = NS_PER_SEC / duty;
	if (duty_freq < period_freq) {
		device_printf(sc->dev, "duty < period\n");
		return (EINVAL);
	}

	/* Assuming 24 MHz reference, we should never actually have
           to use the divider due to pwm API limitations. */
	prescaler = 0;
	scaler = 0;
	using_scaler = false;

	/* XXX Expand API to allow for 64 bit period/duty. */
	period_out = (sc->clk_freq * period) / NS_PER_SEC;
	duty_out = (sc->clk_freq * duty) / NS_PER_SEC;

	reg = RK_PWM_READ(sc, RK_PWM_CTRL);

	if ((reg & RK_PWM_CTRL_MODE_MASK) != RK_PWM_CTRL_MODE_CONTINUOUS) {
		/* Switching modes, disable just in case. */
		SET(reg, RK_PWM_CTRL_ENABLE_MASK, RK_PWM_CTRL_DISABLED);
		RK_PWM_WRITE(sc, RK_PWM_CTRL, reg);
	}

	RK_PWM_WRITE(sc, RK_PWM_PERIOD, period_out);
	RK_PWM_WRITE(sc, RK_PWM_DUTY, duty_out);

	SET(reg, RK_PWM_CTRL_ENABLE_MASK, RK_PWM_CTRL_ENABLED);
	SET(reg, RK_PWM_CTRL_MODE_MASK, RK_PWM_CTRL_MODE_CONTINUOUS);
	SET(reg, RK_PWM_CTRL_ALIGN_MASK, RK_PWM_CTRL_ALIGN_LEFT);
	SET(reg, RK_PWM_CTRL_CLOCKSRC_MASK, using_scaler);
	SET(reg, RK_PWM_CTRL_PRESCALE_MASK,
		prescaler <<  RK_PWM_CTRL_PRESCALE_SHIFT);
	SET(reg, RK_PWM_CTRL_SCALE_MASK,
		scaler << RK_PWM_CTRL_SCALE_SHIFT);

	RK_PWM_WRITE(sc, RK_PWM_CTRL, reg);

	sc->period = period;
	sc->duty = duty;

	return (0);
}

static int
rk_pwm_channel_get_config(device_t dev, u_int channel, u_int *period, u_int *duty)
{
	struct rk_pwm_softc *sc;

	sc = device_get_softc(dev);

	*period = sc->period;
	*duty = sc->duty;

	return (0);
}

static int
rk_pwm_channel_enable(device_t dev, u_int channel, bool enable)
{
	struct rk_pwm_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	if (enable && sc->enabled)
		return (0);

	reg = RK_PWM_READ(sc, RK_PWM_CTRL);
	SET(reg, RK_PWM_CTRL_ENABLE_MASK, enable);

	RK_PWM_WRITE(sc, RK_PWM_CTRL, reg);

	sc->enabled = enable;

	return (0);
}

static int
rk_pwm_channel_is_enabled(device_t dev, u_int channel, bool *enabled)
{
	struct rk_pwm_softc *sc;

	sc = device_get_softc(dev);

	*enabled = sc->enabled;

	return (0);
}

static device_method_t rk_pwm_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rk_pwm_probe),
	DEVMETHOD(device_attach,	rk_pwm_attach),
	DEVMETHOD(device_detach,	rk_pwm_detach),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	aw_pwm_get_node),

	/* pwm interface */
	DEVMETHOD(pwmbus_channel_count,		rk_pwm_channel_count),
	DEVMETHOD(pwmbus_channel_config,	rk_pwm_channel_config),
	DEVMETHOD(pwmbus_channel_get_config,	rk_pwm_channel_get_config),
	DEVMETHOD(pwmbus_channel_enable,	rk_pwm_channel_enable),
	DEVMETHOD(pwmbus_channel_is_enabled,	rk_pwm_channel_is_enabled),

	DEVMETHOD_END
};

static driver_t rk_pwm_driver = {
	"pwm",
	rk_pwm_methods,
	sizeof(struct rk_pwm_softc),
};

DRIVER_MODULE(rk_pwm, simplebus, rk_pwm_driver, 0, 0);
SIMPLEBUS_PNP_INFO(compat_data);
