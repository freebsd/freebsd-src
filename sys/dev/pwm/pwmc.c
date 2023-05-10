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

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/time.h>

#include <dev/pwm/pwmbus.h>
#include <dev/pwm/pwmc.h>

#include "pwmbus_if.h"

#ifdef FDT
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

static struct ofw_compat_data compat_data[] = {
	{"freebsd,pwmc", true},
	{NULL,           false},
};

PWMBUS_FDT_PNP_INFO(compat_data);

#endif

struct pwmc_softc {
	device_t	dev;
	struct cdev	*cdev;
	u_int		chan;
};

static int
pwm_ioctl(struct cdev *dev, u_long cmd, caddr_t data,
    int fflag, struct thread *td)
{
	struct pwmc_softc *sc;
	struct pwm_state state;
	device_t bus;
	int rv = 0;

	sc = dev->si_drv1;
	bus = device_get_parent(sc->dev);

	switch (cmd) {
	case PWMSETSTATE:
		bcopy(data, &state, sizeof(state));
		rv = PWMBUS_CHANNEL_CONFIG(bus, sc->chan,
		    state.period, state.duty);
		if (rv != 0)
			return (rv);

		rv = PWMBUS_CHANNEL_SET_FLAGS(bus,
		    sc->chan, state.flags);
		if (rv != 0 && rv != EOPNOTSUPP)
			return (rv);

		rv = PWMBUS_CHANNEL_ENABLE(bus, sc->chan,
		    state.enable);
		break;
	case PWMGETSTATE:
		bcopy(data, &state, sizeof(state));
		rv = PWMBUS_CHANNEL_GET_CONFIG(bus, sc->chan,
		    &state.period, &state.duty);
		if (rv != 0)
			return (rv);

		rv = PWMBUS_CHANNEL_GET_FLAGS(bus, sc->chan,
		    &state.flags);
		if (rv != 0)
			return (rv);

		rv = PWMBUS_CHANNEL_IS_ENABLED(bus, sc->chan,
		    &state.enable);
		if (rv != 0)
			return (rv);
		bcopy(&state, data, sizeof(state));
		break;
	}

	return (rv);
}

static struct cdevsw pwm_cdevsw = {
	.d_version	= D_VERSION,
	.d_name		= "pwmc",
	.d_ioctl	= pwm_ioctl
};

static void
pwmc_setup_label(struct pwmc_softc *sc)
{
	const char *hintlabel;
#ifdef FDT
	void *label;

	if (OF_getprop_alloc(ofw_bus_get_node(sc->dev), "label", &label) > 0) {
		make_dev_alias(sc->cdev, "pwm/%s", (char *)label);
		OF_prop_free(label);
	}
#endif

	if (resource_string_value(device_get_name(sc->dev),
	    device_get_unit(sc->dev), "label", &hintlabel) == 0) {
		make_dev_alias(sc->cdev, "pwm/%s", hintlabel);
	}
}

static int
pwmc_probe(device_t dev)
{
	int rv;

	rv = BUS_PROBE_NOWILDCARD;

#ifdef FDT
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data != 0) {
		rv = BUS_PROBE_DEFAULT;
	}
#endif

	device_set_desc(dev, "PWM Control");
	return (rv);
}

static int
pwmc_attach(device_t dev)
{
	struct pwmc_softc *sc;
	struct make_dev_args args;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;

	if ((error = pwmbus_get_channel(dev, &sc->chan)) != 0)
		return (error);

	make_dev_args_init(&args);
	args.mda_flags = MAKEDEV_CHECKNAME | MAKEDEV_WAITOK;
	args.mda_devsw = &pwm_cdevsw;
	args.mda_uid = UID_ROOT;
	args.mda_gid = GID_OPERATOR;
	args.mda_mode = 0660;
	args.mda_si_drv1 = sc;
	error = make_dev_s(&args, &sc->cdev, "pwm/pwmc%d.%d",
	    device_get_unit(device_get_parent(dev)), sc->chan);
	if (error != 0) {
		device_printf(dev, "Failed to make PWM device\n");
		return (error);
	}

	pwmc_setup_label(sc);

	return (0);
}

static int
pwmc_detach(device_t dev)
{
	struct pwmc_softc *sc;

	sc = device_get_softc(dev);
	destroy_dev(sc->cdev);

	return (0);
}

static device_method_t pwmc_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe, pwmc_probe),
	DEVMETHOD(device_attach, pwmc_attach),
	DEVMETHOD(device_detach, pwmc_detach),

	DEVMETHOD_END
};

static driver_t pwmc_driver = {
	"pwmc",
	pwmc_methods,
	sizeof(struct pwmc_softc),
};

DRIVER_MODULE(pwmc, pwmbus, pwmc_driver, 0, 0);
MODULE_DEPEND(pwmc, pwmbus, 1, 1, 1);
MODULE_VERSION(pwmc, 1);
