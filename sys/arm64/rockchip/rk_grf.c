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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <machine/bus.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/syscon/syscon.h>
#include <dev/fdt/simple_mfd.h>

static struct ofw_compat_data compat_data[] = {
	{"rockchip,rk3288-grf", 1},
	{"rockchip,rk3328-grf", 1},
	{"rockchip,rk3399-grf", 1},
	{"rockchip,rk3399-pmugrf", 1},
	{"rockchip,rk3568-grf", 1},
	{"rockchip,rk3568-pmugrf", 1},
	{"rockchip,rk3568-usb2phy-grf", 1},
	{"rockchip,rk3566-pipe-grf", 1},
	{"rockchip,rk3568-pipe-grf", 1},
	{"rockchip,rk3568-pipe-phy-grf", 1},
	{"rockchip,rk3568-pcie3-phy-grf", 1},
	{NULL,             0}
};

static int
rk_grf_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "RockChip General Register Files");
	return (BUS_PROBE_DEFAULT);
}

static device_method_t rk_grf_methods[] = {
	DEVMETHOD(device_probe, rk_grf_probe),

	DEVMETHOD_END
};

DEFINE_CLASS_1(rk_grf, rk_grf_driver, rk_grf_methods,
    sizeof(struct simple_mfd_softc), simple_mfd_driver);

EARLY_DRIVER_MODULE(rk_grf, simplebus, rk_grf_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(rk_grf, 1);
