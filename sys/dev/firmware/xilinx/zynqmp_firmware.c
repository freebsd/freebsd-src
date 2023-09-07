/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Beckhoff Automation GmbH & Co. KG
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <machine/bus.h>

#include <dev/fdt/simplebus.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/psci/smccc.h>

#include <dev/firmware/xilinx/pm_defs.h>

#include "zynqmp_firmware_if.h"

enum {
	IOCTL_GET_RPU_OPER_MODE = 0,
	IOCTL_SET_RPU_OPER_MODE = 1,
	IOCTL_RPU_BOOT_ADDR_CONFIG = 2,
	IOCTL_TCM_COMB_CONFIG = 3,
	IOCTL_SET_TAPDELAY_BYPASS = 4,
	IOCTL_SET_SGMII_MODE = 5,
	IOCTL_SD_DLL_RESET = 6,
	IOCTL_SET_SD_TAPDELAY = 7,
	 /* Ioctl for clock driver */
	IOCTL_SET_PLL_FRAC_MODE = 8,
	IOCTL_GET_PLL_FRAC_MODE = 9,
	IOCTL_SET_PLL_FRAC_DATA = 10,
	IOCTL_GET_PLL_FRAC_DATA = 11,
	IOCTL_WRITE_GGS = 12,
	IOCTL_READ_GGS = 13,
	IOCTL_WRITE_PGGS = 14,
	IOCTL_READ_PGGS = 15,
	/* IOCTL for ULPI reset */
	IOCTL_ULPI_RESET = 16,
	/* Set healthy bit value */
	IOCTL_SET_BOOT_HEALTH_STATUS = 17,
	IOCTL_AFI = 18,
	/* Probe counter read/write */
	IOCTL_PROBE_COUNTER_READ = 19,
	IOCTL_PROBE_COUNTER_WRITE = 20,
	IOCTL_OSPI_MUX_SELECT = 21,
	/* IOCTL for USB power request */
	IOCTL_USB_SET_STATE = 22,
	/* IOCTL to get last reset reason */
	IOCTL_GET_LAST_RESET_REASON = 23,
	/* AI engine NPI ISR clear */
	IOCTL_AIE_ISR_CLEAR = 24,
	/* Register SGI to ATF */
	IOCTL_REGISTER_SGI = 25,
};

typedef int (*zynqmp_callfn_t)(register_t, register_t, register_t, uint32_t *payload);

struct zynqmp_firmware_softc {
	struct simplebus_softc	sc;
	device_t		dev;
	zynqmp_callfn_t		callfn;
};

/* SMC calling methods */
#define	PM_SIP_SVC	0xC2000000

static int
zynqmp_call_smc(uint32_t id, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t *payload, bool ignore_error)
{
	struct arm_smccc_res res;
	uint64_t args[3];

	args[0] = id | PM_SIP_SVC;
	args[1] = ((uint64_t)a1 << 32) | a0;
	args[2] = ((uint64_t)a3 << 32) | a2;
	arm_smccc_smc(args[0], args[1], args[2], 0, 0, 0, 0, 0, &res);
	if (payload != NULL) {
		payload[0] = res.a0 & 0xFFFFFFFF;
		payload[1] = res.a0 >> 32;
		payload[2] = res.a1 & 0xFFFFFFFF;
		payload[3] = res.a1 >> 32;
		if (!ignore_error && payload[0] != PM_RET_SUCCESS) {
			printf("%s: fail %x\n", __func__, payload[0]);
			return (EINVAL);
		}
	}
	return (0);
}

/* Firmware methods */
static int
zynqmp_get_api_version(struct zynqmp_firmware_softc *sc)
{
	uint32_t payload[4];
	int rv;

	rv = zynqmp_call_smc(PM_GET_API_VERSION, 0, 0, 0, 0, payload, false);
	if (rv != 0) {
		device_printf(sc->dev, "SMC Call fail %d\n", rv);
		goto out;
	}
	device_printf(sc->dev, "API version = %d.%d\n",
	    payload[1] >> 16, payload[1] & 0xFFFF);
out:
	return (rv);
}

static int
zynqmp_get_chipid(struct zynqmp_firmware_softc *sc)
{
	uint32_t payload[4];
	int rv;

	rv = zynqmp_call_smc(PM_GET_CHIPID, 0, 0, 0, 0, payload, false);
	if (rv != 0) {
		device_printf(sc->dev, "SMC Call fail %d\n", rv);
		goto out;
	}
	device_printf(sc->dev, "ID Code = %x Version = %x\n",
	    payload[1], payload[2]);
out:
	return (rv);
}

static int
zynqmp_get_trustzone_version(struct zynqmp_firmware_softc *sc)
{
	uint32_t payload[4];
	int rv;

	rv = zynqmp_call_smc(PM_GET_TRUSTZONE_VERSION, 0, 0, 0, 0, payload, false);
	if (rv != 0) {
		device_printf(sc->dev, "SMC Call fail %d\n", rv);
		goto out;
	}
	device_printf(sc->dev, "Trustzone Version = %x\n",
	    payload[1]);
out:
	return (rv);
}

/* zynqmp_firmware methods */
static int
zynqmp_firmware_clock_enable(device_t dev, uint32_t clkid)
{
	struct zynqmp_firmware_softc *sc;
	uint32_t payload[4];
	int rv;

	sc = device_get_softc(dev);
	rv = zynqmp_call_smc(PM_CLOCK_ENABLE, clkid, 0, 0, 0, payload, false);
	if (rv != 0)
		device_printf(sc->dev, "SMC Call fail %d\n", rv);
	return (rv);
}

static int
zynqmp_firmware_clock_disable(device_t dev, uint32_t clkid)
{
	struct zynqmp_firmware_softc *sc;
	uint32_t payload[4];
	int rv;

	sc = device_get_softc(dev);
	rv = zynqmp_call_smc(PM_CLOCK_DISABLE, clkid, 0, 0, 0, payload, false);
	if (rv != 0)
		device_printf(sc->dev, "SMC Call fail %d\n", rv);
	return (rv);
}

static int
zynqmp_firmware_clock_getstate(device_t dev, uint32_t clkid, bool *enabled)
{
	struct zynqmp_firmware_softc *sc;
	uint32_t payload[4];
	int rv;

	sc = device_get_softc(dev);
	rv = zynqmp_call_smc(PM_CLOCK_GETSTATE, clkid, 0, 0, 0, payload, false);
	if (rv != 0) {
		device_printf(sc->dev, "SMC Call fail %d\n", rv);
		goto out;
	}
	*enabled = payload[1] == 1 ? true : false;

out:
	return (rv);
}

static int
zynqmp_firmware_clock_setdivider(device_t dev, uint32_t clkid, uint32_t div)
{
	struct zynqmp_firmware_softc *sc;
	uint32_t payload[4];
	int rv;

	sc = device_get_softc(dev);
	rv = zynqmp_call_smc(PM_CLOCK_SETDIVIDER, clkid, div, 0, 0, payload, false);
	if (rv != 0)
		device_printf(sc->dev, "SMC Call fail %d\n", rv);
	return (rv);
}

static int
zynqmp_firmware_clock_getdivider(device_t dev, uint32_t clkid, uint32_t *div)
{
	struct zynqmp_firmware_softc *sc;
	uint32_t payload[4];
	int rv;

	sc = device_get_softc(dev);
	rv = zynqmp_call_smc(PM_CLOCK_GETDIVIDER, clkid, 0, 0, 0, payload, false);
	if (rv != 0) {
		device_printf(sc->dev, "SMC Call fail %d\n", rv);
		goto out;
	}
	*div = payload[1];

out:
	return (rv);
}

static int
zynqmp_firmware_clock_setparent(device_t dev, uint32_t clkid, uint32_t parentid)
{
	struct zynqmp_firmware_softc *sc;
	uint32_t payload[4];
	int rv;

	sc = device_get_softc(dev);
	rv = zynqmp_call_smc(PM_CLOCK_SETPARENT, clkid, parentid, 0, 0, payload, false);
	if (rv != 0)
		device_printf(sc->dev, "SMC Call fail %d\n", rv);
	return (rv);
}

static int
zynqmp_firmware_clock_getparent(device_t dev, uint32_t clkid, uint32_t *parentid)
{
	struct zynqmp_firmware_softc *sc;
	uint32_t payload[4];
	int rv;

	sc = device_get_softc(dev);
	rv = zynqmp_call_smc(PM_CLOCK_GETPARENT, clkid, 0, 0, 0, payload, false);
	if (rv != 0) {
		device_printf(sc->dev, "SMC Call fail %d\n", rv);
		goto out;
	}
	*parentid = payload[1];
out:
	return (rv);
}

static int
zynqmp_firmware_pll_get_mode(device_t dev, uint32_t pllid, uint32_t *mode)
{
	struct zynqmp_firmware_softc *sc;
	uint32_t payload[4];
	int rv;

	sc = device_get_softc(dev);
	rv = zynqmp_call_smc(PM_IOCTL, 0, IOCTL_GET_PLL_FRAC_MODE, pllid, 0, payload, false);
	if (rv != 0) {
		device_printf(sc->dev, "SMC Call fail %d\n", rv);
		goto out;
	}
	*mode = payload[1];
out:
	return (rv);
}

static int
zynqmp_firmware_pll_get_frac_data(device_t dev, uint32_t pllid, uint32_t *data)
{
	struct zynqmp_firmware_softc *sc;
	uint32_t payload[4];
	int rv;

	sc = device_get_softc(dev);
	rv = zynqmp_call_smc(PM_IOCTL, 0, IOCTL_GET_PLL_FRAC_DATA, pllid, 0, payload, false);
	if (rv != 0) {
		device_printf(sc->dev, "SMC Call fail %d\n", rv);
		goto out;
	}
	*data = payload[1];
out:
	return (rv);
}

static int
zynqmp_firmware_clock_get_fixedfactor(device_t dev, uint32_t clkid, uint32_t *mult, uint32_t *div)
{
	struct zynqmp_firmware_softc *sc;
	uint32_t payload[4];
	int rv;

	sc = device_get_softc(dev);
	rv = zynqmp_call_smc(PM_QUERY_DATA, PM_QID_CLOCK_GET_FIXEDFACTOR_PARAMS, clkid, 0, 0, payload, true);
	if (rv != 0) {
		device_printf(sc->dev, "SMC Call fail %d\n", rv);
		goto out;
	}
	*mult = payload[1];
	*div = payload[2];
out:
	return (rv);
}

static int
zynqmp_firmware_query_data(device_t dev, uint32_t qid, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t *data)
{
	struct zynqmp_firmware_softc *sc;
	int rv;

	sc = device_get_softc(dev);
	rv = zynqmp_call_smc(PM_QUERY_DATA, qid, arg1, arg2, arg3, data, true);
	/*
	 * PM_QID_CLOCK_GET_NAME always success and if the clock name couldn't
	 * be found the clock name will be all null byte
	 */
	if (qid == 1)
		rv = 0;
	if (rv != 0)
		device_printf(sc->dev, "SMC Call fail %d\n", rv);
	return (rv);
}

static int
zynqmp_firmware_reset_assert(device_t dev, uint32_t resetid, bool enable)
{
	struct zynqmp_firmware_softc *sc;
	int rv;

	sc = device_get_softc(dev);
	rv = zynqmp_call_smc(PM_RESET_ASSERT, resetid, enable, 0, 0, NULL, true);
	if (rv != 0)
		device_printf(sc->dev, "SMC Call fail %d\n", rv);

	return (rv);
}

static int
zynqmp_firmware_reset_get_status(device_t dev, uint32_t resetid, bool *status)
{
	struct zynqmp_firmware_softc *sc;
	uint32_t payload[4];
	int rv;

	sc = device_get_softc(dev);
	rv = zynqmp_call_smc(PM_RESET_GET_STATUS, resetid, 0, 0, 0, payload, true);
	if (rv != 0) {
		device_printf(sc->dev, "SMC Call fail %d\n", rv);
		return (rv);
	}
	*status = payload[1];

	return (rv);
}

/* Simplebus methods */
static struct simplebus_devinfo *
zynqmp_firmware_setup_dinfo(device_t dev, phandle_t node,
    struct simplebus_devinfo *di)
{
	struct simplebus_softc *sc;
	struct simplebus_devinfo *ndi;

	sc = device_get_softc(dev);
	if (di == NULL)
		ndi = malloc(sizeof(*ndi), M_DEVBUF, M_WAITOK | M_ZERO);
	else
		ndi = di;
	if (ofw_bus_gen_setup_devinfo(&ndi->obdinfo, node) != 0) {
		if (di == NULL)
			free(ndi, M_DEVBUF);
		return (NULL);
	}

	/* reg resources is from the parent but interrupts is on the node itself */
	resource_list_init(&ndi->rl);
	ofw_bus_reg_to_rl(dev, OF_parent(node), sc->acells, sc->scells, &ndi->rl);
	ofw_bus_intr_to_rl(dev, node, &ndi->rl, NULL);

	return (ndi);
}

static device_t
zynqmp_firmware_add_device(device_t dev, phandle_t node, u_int order,
    const char *name, int unit, struct simplebus_devinfo *di)
{
	struct simplebus_devinfo *ndi;
	device_t cdev;

	if ((ndi = zynqmp_firmware_setup_dinfo(dev, node, di)) == NULL)
		return (NULL);
	cdev = device_add_child_ordered(dev, order, name, unit);
	if (cdev == NULL) {
		device_printf(dev, "<%s>: device_add_child failed\n",
		    ndi->obdinfo.obd_name);
		resource_list_free(&ndi->rl);
		ofw_bus_gen_destroy_devinfo(&ndi->obdinfo);
		if (di == NULL)
			free(ndi, M_DEVBUF);
		return (NULL);
	}
	device_set_ivars(cdev, ndi);

	return(cdev);
}

static int
zynqmp_firmware_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (!ofw_bus_is_compatible(dev, "xlnx,zynqmp-firmware"))
		return (ENXIO);
	device_set_desc(dev, "ZynqMP Firmware");
	return (0);
}

static int
zynqmp_firmware_attach(device_t dev)
{
	struct zynqmp_firmware_softc *sc;
	phandle_t node, child;
	device_t cdev;

	sc = device_get_softc(dev);
	sc->dev = dev;

	if (bootverbose) {
		zynqmp_get_api_version(sc);
		zynqmp_get_chipid(sc);
		zynqmp_get_trustzone_version(sc);
	}

	/* Attach children */
	node = ofw_bus_get_node(dev);
	for (child = OF_child(node); child != 0; child = OF_peer(child)) {
		cdev = zynqmp_firmware_add_device(dev, child, 0, NULL, -1, NULL);
		if (cdev != NULL)
			device_probe_and_attach(cdev);
	}

	return (bus_generic_attach(dev));
}

static device_method_t zynqmp_firmware_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe, 	zynqmp_firmware_probe),
	DEVMETHOD(device_attach, 	zynqmp_firmware_attach),

	/* zynqmp_firmware_if */
	DEVMETHOD(zynqmp_firmware_clock_enable, zynqmp_firmware_clock_enable),
	DEVMETHOD(zynqmp_firmware_clock_disable, zynqmp_firmware_clock_disable),
	DEVMETHOD(zynqmp_firmware_clock_getstate, zynqmp_firmware_clock_getstate),
	DEVMETHOD(zynqmp_firmware_clock_setdivider, zynqmp_firmware_clock_setdivider),
	DEVMETHOD(zynqmp_firmware_clock_getdivider, zynqmp_firmware_clock_getdivider),
	DEVMETHOD(zynqmp_firmware_clock_setparent, zynqmp_firmware_clock_setparent),
	DEVMETHOD(zynqmp_firmware_clock_getparent, zynqmp_firmware_clock_getparent),
	DEVMETHOD(zynqmp_firmware_pll_get_mode, zynqmp_firmware_pll_get_mode),
	DEVMETHOD(zynqmp_firmware_pll_get_frac_data, zynqmp_firmware_pll_get_frac_data),
	DEVMETHOD(zynqmp_firmware_clock_get_fixedfactor, zynqmp_firmware_clock_get_fixedfactor),
	DEVMETHOD(zynqmp_firmware_query_data, zynqmp_firmware_query_data),
	DEVMETHOD(zynqmp_firmware_reset_assert, zynqmp_firmware_reset_assert),
	DEVMETHOD(zynqmp_firmware_reset_get_status, zynqmp_firmware_reset_get_status),

	DEVMETHOD_END
};

DEFINE_CLASS_1(zynqmp_firmware, zynqmp_firmware_driver, zynqmp_firmware_methods,
  sizeof(struct zynqmp_firmware_softc), simplebus_driver);

EARLY_DRIVER_MODULE(zynqmp_firmware, simplebus, zynqmp_firmware_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_LATE);
MODULE_VERSION(zynqmp_firmware, 1);
