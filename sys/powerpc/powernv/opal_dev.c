/*-
 * Copyright (c) 2015 Nathan Whitehorn
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/clock.h>
#include <sys/cpu.h>
#include <sys/kernel.h>
#include <sys/reboot.h>
#include <sys/sysctl.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include "clock_if.h"
#include "opal.h"

static int	opaldev_probe(device_t);
static int	opaldev_attach(device_t);
/* clock interface */
static int	opal_gettime(device_t dev, struct timespec *ts);
static int	opal_settime(device_t dev, struct timespec *ts);
/* ofw bus interface */
static const struct ofw_bus_devinfo *opaldev_get_devinfo(device_t dev,
    device_t child);

static void	opal_shutdown(void *arg, int howto);

static device_method_t  opaldev_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		opaldev_probe),
	DEVMETHOD(device_attach,	opaldev_attach),

	/* clock interface */
	DEVMETHOD(clock_gettime,	opal_gettime),
	DEVMETHOD(clock_settime,	opal_settime),

        /* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,	opaldev_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),
	
	DEVMETHOD_END
};

static driver_t opaldev_driver = {
	"opal",
	opaldev_methods,
	0
};

static devclass_t opaldev_devclass;

DRIVER_MODULE(opaldev, ofwbus, opaldev_driver, opaldev_devclass, 0, 0);

static int
opaldev_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "ibm,opal-v3"))
		return (ENXIO);
	if (opal_check() != 0)
		return (ENXIO);

	device_set_desc(dev, "OPAL Abstraction Firmware");
	return (BUS_PROBE_SPECIFIC);
}

static int
opaldev_attach(device_t dev)
{
	phandle_t child;
	device_t cdev;
	struct ofw_bus_devinfo *dinfo;

	if (0 /* XXX NOT YET TEST FOR RTC */)
		clock_register(dev, 2000);
	
	EVENTHANDLER_REGISTER(shutdown_final, opal_shutdown, NULL,
	    SHUTDOWN_PRI_LAST);

	for (child = OF_child(ofw_bus_get_node(dev)); child != 0;
	    child = OF_peer(child)) {
		dinfo = malloc(sizeof(*dinfo), M_DEVBUF, M_WAITOK | M_ZERO);
		if (ofw_bus_gen_setup_devinfo(dinfo, child) != 0) {
			free(dinfo, M_DEVBUF);
			continue;
		}
		cdev = device_add_child(dev, NULL, -1);
		if (cdev == NULL) {
			device_printf(dev, "<%s>: device_add_child failed\n",
			    dinfo->obd_name);
			ofw_bus_gen_destroy_devinfo(dinfo);
			free(dinfo, M_DEVBUF);
			continue;
		}
		device_set_ivars(cdev, dinfo);
	}

	return (bus_generic_attach(dev));
}

static int
opal_gettime(device_t dev, struct timespec *ts) {
	return (ENXIO);
}

static int
opal_settime(device_t dev, struct timespec *ts)
{
	return (0);
}

static const struct ofw_bus_devinfo *
opaldev_get_devinfo(device_t dev, device_t child)
{
	return (device_get_ivars(child));
}

static void
opal_shutdown(void *arg, int howto)
{

	if (howto & RB_HALT)
		opal_call(OPAL_CEC_POWER_DOWN, 0 /* Normal power off */);
	else
		opal_call(OPAL_CEC_REBOOT);
}

