/*-
 * Copyright (c) 2016 Stanislav Galabov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
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
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/fdt/fdt_pinctrl.h>
#include <mips/mediatek/mtk_sysctl.h>

#include "fdt_pinctrl_if.h"

static const struct ofw_compat_data compat_data[] = {
	{ "ralink,rt2880-pinctrl",	1 },

	/* Sentinel */
	{ NULL,				0 }
};

static int
mtk_pinctrl_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "MTK Pin Controller");

	return (0);
}

static int
mtk_pinctrl_attach(device_t dev)
{

	if (device_get_unit(dev) != 0) {
		device_printf(dev, "Only one pin control allowed\n");
		return (ENXIO);
	}

	fdt_pinctrl_register(dev, "pinctrl-single,bits");
	fdt_pinctrl_configure_tree(dev);

	if (bootverbose)
		device_printf(dev, "GPIO mode: 0x%08x\n",
		    mtk_sysctl_get(SYSCTL_GPIOMODE));

	return (0);
}

static int
mtk_pinctrl_configure(device_t dev, phandle_t cfgxref)
{

	return (EINVAL);
}

static device_method_t mtk_pinctrl_methods[] = {
	DEVMETHOD(device_probe,			mtk_pinctrl_probe),
	DEVMETHOD(device_attach,		mtk_pinctrl_attach),

	/* fdt_pinctrl interface */
	DEVMETHOD(fdt_pinctrl_configure,	mtk_pinctrl_configure),

	DEVMETHOD_END
};

static driver_t mtk_pinctrl_driver = {
	"pinctrl",
	mtk_pinctrl_methods,
	0,
};
static devclass_t mtk_pinctrl_devclass;

EARLY_DRIVER_MODULE(mtk_pinctrl, simplebus, mtk_pinctrl_driver,
    mtk_pinctrl_devclass, 0, 0, BUS_PASS_INTERRUPT + BUS_PASS_ORDER_EARLY);

MODULE_DEPEND(mtk_pinctrl, mtk_sysctl, 1, 1, 1);
