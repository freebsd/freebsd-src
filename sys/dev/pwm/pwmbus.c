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
#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sbuf.h>

#include <dev/pwm/pwmbus.h>

#include "pwmbus_if.h"

/*
 * bus_if methods...
 */

static device_t
pwmbus_add_child(device_t dev, u_int order, const char *name, int unit)
{
	device_t child;
	struct pwmbus_ivars *ivars;

	child = device_add_child_ordered(dev, order, name, unit);
	if (child == NULL) 
		return (child);

	ivars = malloc(sizeof(struct pwmbus_ivars), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (ivars == NULL) {
		device_delete_child(dev, child);
		return (NULL);
	}
	device_set_ivars(child, ivars);

	return (child);
}

static int
pwmbus_child_location(device_t dev, device_t child, struct sbuf *sb)
{
	struct pwmbus_ivars *ivars;

	ivars = device_get_ivars(child);
	sbuf_printf(sb, "hwdev=%s channel=%u", 
	    device_get_nameunit(device_get_parent(dev)), ivars->pi_channel);

	return (0);
}

static void
pwmbus_hinted_child(device_t dev, const char *dname, int dunit)
{
	struct pwmbus_ivars *ivars;
	device_t child;

	child = pwmbus_add_child(dev, 0, dname, dunit);

	/*
	 * If there is a channel hint, use it.  Otherwise pi_channel was
	 * initialized to zero, so that's the channel we'll use.
	 */
	ivars = device_get_ivars(child);
	resource_int_value(dname, dunit, "channel", &ivars->pi_channel);
}

static int
pwmbus_print_child(device_t dev, device_t child)
{
	struct pwmbus_ivars *ivars;
	int rv;

	ivars = device_get_ivars(child);

	rv  = bus_print_child_header(dev, child);
	rv += printf(" channel %u", ivars->pi_channel);
	rv += bus_print_child_footer(dev, child);

	return (rv);
}

static void
pwmbus_probe_nomatch(device_t dev, device_t child)
{
	struct pwmbus_ivars *ivars;

	ivars = device_get_ivars(child);
	if (ivars != NULL)
		device_printf(dev, "<unknown> on channel %u\n",
		    ivars->pi_channel);

	return;
}

static int
pwmbus_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct pwmbus_ivars *ivars;

	ivars = device_get_ivars(child);

	switch (which) {
	case PWMBUS_IVAR_CHANNEL:
		*(u_int *)result = ivars->pi_channel;
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

/*
 * device_if methods...
 */

static int
pwmbus_probe(device_t dev)
{
	device_set_desc(dev, "PWM bus");
	return (BUS_PROBE_GENERIC);
}

static int
pwmbus_attach(device_t dev)
{
	struct pwmbus_softc *sc;
	struct pwmbus_ivars *ivars;
	device_t child, parent;
	u_int chan;

	sc = device_get_softc(dev);
	sc->dev = dev;
	parent = device_get_parent(dev);

	if (PWMBUS_CHANNEL_COUNT(parent, &sc->nchannels) != 0 ||
	    sc->nchannels == 0) {
		device_printf(sc->dev, "No channels on parent %s\n",
		    device_get_nameunit(parent));
		return (ENXIO);
	}

	/* Add a pwmc(4) child for each channel. */
	for (chan = 0; chan < sc->nchannels; ++chan) {
		if ((child = pwmbus_add_child(sc->dev, 0, "pwmc", -1)) == NULL) {
			device_printf(dev, "failed to add pwmc child device "
			    "for channel %u\n", chan);
			continue;
		}
		ivars = device_get_ivars(child);
		ivars->pi_channel = chan;
	}

	bus_enumerate_hinted_children(dev);
	bus_generic_probe(dev);

	return (bus_generic_attach(dev));
}

static int
pwmbus_detach(device_t dev)
{
	int rv;

	if ((rv = bus_generic_detach(dev)) == 0)
		rv = device_delete_children(dev);

	return (rv);
}

/*
 * pwmbus_if methods...
 */

static int
pwmbus_channel_config(device_t dev, u_int chan, u_int period, u_int duty)
{
	return (PWMBUS_CHANNEL_CONFIG(device_get_parent(dev), chan, period, duty));
}

static int
pwmbus_channel_get_config(device_t dev, u_int chan, u_int *period, u_int *duty)
{
	return (PWMBUS_CHANNEL_GET_CONFIG(device_get_parent(dev), chan, period, duty));
}

static int
pwmbus_channel_get_flags(device_t dev, u_int chan, uint32_t *flags)
{
	return (PWMBUS_CHANNEL_GET_FLAGS(device_get_parent(dev), chan, flags));
}

static int
pwmbus_channel_enable(device_t dev, u_int chan, bool enable)
{
	return (PWMBUS_CHANNEL_ENABLE(device_get_parent(dev), chan, enable));
}

static int
pwmbus_channel_set_flags(device_t dev, u_int chan, uint32_t flags)
{
	return (PWMBUS_CHANNEL_SET_FLAGS(device_get_parent(dev), chan, flags));
}

static int
pwmbus_channel_is_enabled(device_t dev, u_int chan, bool *enable)
{
	return (PWMBUS_CHANNEL_IS_ENABLED(device_get_parent(dev), chan, enable));
}

static int
pwmbus_channel_count(device_t dev, u_int *nchannel)
{
	return (PWMBUS_CHANNEL_COUNT(device_get_parent(dev), nchannel));
}

static device_method_t pwmbus_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe,  pwmbus_probe),
	DEVMETHOD(device_attach, pwmbus_attach),
	DEVMETHOD(device_detach, pwmbus_detach),

        /* bus_if */
	DEVMETHOD(bus_add_child,		pwmbus_add_child),
	DEVMETHOD(bus_child_location,		pwmbus_child_location),
	DEVMETHOD(bus_hinted_child,		pwmbus_hinted_child),
	DEVMETHOD(bus_print_child,		pwmbus_print_child),
	DEVMETHOD(bus_probe_nomatch,		pwmbus_probe_nomatch),
	DEVMETHOD(bus_read_ivar,		pwmbus_read_ivar),

        /* pwmbus_if  */
	DEVMETHOD(pwmbus_channel_count,		pwmbus_channel_count),
	DEVMETHOD(pwmbus_channel_config,	pwmbus_channel_config),
	DEVMETHOD(pwmbus_channel_get_config,	pwmbus_channel_get_config),
	DEVMETHOD(pwmbus_channel_set_flags,	pwmbus_channel_set_flags),
	DEVMETHOD(pwmbus_channel_get_flags,	pwmbus_channel_get_flags),
	DEVMETHOD(pwmbus_channel_enable,	pwmbus_channel_enable),
	DEVMETHOD(pwmbus_channel_is_enabled,	pwmbus_channel_is_enabled),

	DEVMETHOD_END
};

driver_t pwmbus_driver = {
	"pwmbus",
	pwmbus_methods,
	sizeof(struct pwmbus_softc),
};

EARLY_DRIVER_MODULE(pwmbus, pwm, pwmbus_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(pwmbus, 1);
