/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Adrian Chadd <adrian@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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

/*
 * This is the MDIO controller for the IPQ4018/IPQ4019.
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/gpio.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <dev/gpio/gpiobusvar.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "mdio_if.h"

#include <dev/qcom_mdio/qcom_mdio_ipq4018_var.h>
#include <dev/qcom_mdio/qcom_mdio_ipq4018_reg.h>

#include <dev/qcom_mdio/qcom_mdio_debug.h>

static int
qcom_mdio_ipq4018_probe(device_t dev)
{

	if (! ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "qcom,ipq4019-mdio") == 0)
		return (ENXIO);

	device_set_desc(dev,
	    "Qualcomm Atheros IPQ4018/IPQ4019 MDIO driver");
	return (0);
}

static int
qcom_mdio_ipq4018_detach(device_t dev)
{
	struct qcom_mdio_ipq4018_softc *sc = device_get_softc(dev);

	if (sc->sc_mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_mem_rid,
		    sc->sc_mem_res);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static void
qcom_mdio_sysctl_attach(struct qcom_mdio_ipq4018_softc *sc)
{
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(sc->sc_dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(sc->sc_dev);

	SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "debug", CTLFLAG_RW, &sc->sc_debug, 0,
	    "control debugging printfs");
}

static int
qcom_mdio_ipq4018_attach(device_t dev)
{
	phandle_t node;
	struct qcom_mdio_ipq4018_softc *sc = device_get_softc(dev);
	int error = 0;

	node = ofw_bus_get_node(dev);

	sc->sc_dev = dev;
	sc->sc_debug = 0;
	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	/*
	 * Map the MDIO memory region.
	 */
	sc->sc_mem_rid = 0;
	sc->sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->sc_mem_rid, RF_ACTIVE);
	if (sc->sc_mem_res == NULL) {
		error = ENXIO;
		device_printf(dev, "%s: failed to map device memory\n",
		    __func__);
		goto error;
	}
	sc->sc_mem_res_size = (size_t) bus_get_resource_count(dev,
	    SYS_RES_MEMORY, sc->sc_mem_rid);
	if (sc->sc_mem_res_size == 0) {
		error = ENXIO;
		device_printf(dev, "%s: failed to get device memory size\n",
		    __func__);
		goto error;

	}

	qcom_mdio_sysctl_attach(sc);

	OF_device_register_xref(OF_xref_from_node(node), dev);

	return (0);
error:
	if (sc->sc_mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_mem_rid,
		    sc->sc_mem_res);

	mtx_destroy(&sc->sc_mtx);
	return (error);
}

/*
 * Wait for the BUSY flag to become zero.
 *
 * This has to happen before every MDIO transfer.
 *
 * Returns 0 if OK, error if error/timed out.
 */
static int
qcom_mdio_ipq4018_wait(struct qcom_mdio_ipq4018_softc *sc)
{
	int i;
	uint32_t reg;

	MDIO_LOCK_ASSERT(sc);

	for (i = 0; i < QCOM_IPQ4018_MDIO_SLEEP_COUNT; i++) {
		MDIO_BARRIER_READ(sc);

		reg = MDIO_READ(sc, QCOM_IPQ4018_MDIO_REG_CMD);
		if ((reg & QCOM_IPQ4018_MDIO_REG_CMD_ACCESS_BUSY) == 0)
			return (0);
		DELAY(QCOM_IPQ4018_MDIO_SLEEP);
	}
	device_printf(sc->sc_dev, "%s: warning: timeout waiting for bus\n",
	    __func__);
	return (ETIMEDOUT);
}

static void
qcom_mdio_ipq4018_set_phy_reg_addr(struct qcom_mdio_ipq4018_softc *sc,
  int phy, int reg)
{

	MDIO_LOCK_ASSERT(sc);

	MDIO_WRITE(sc, QCOM_IPQ4018_MDIO_REG_ADDR,
	    ((phy & 0xff) << 8) | (reg & 0xff));
	MDIO_BARRIER_WRITE(sc);
}

static int
qcom_mdio_ipq4018_readreg(device_t dev, int phy, int reg)
{
	struct qcom_mdio_ipq4018_softc *sc = device_get_softc(dev);
	uint32_t ret;

	QCOM_MDIO_DPRINTF(sc, QCOM_MDIO_DEBUG_REG_READ,
	    "%s: called; phy=0x%x reg=0x%x\n",
	    __func__, phy, reg);

	MDIO_LOCK(sc);
	if (qcom_mdio_ipq4018_wait(sc) != 0) {
		MDIO_UNLOCK(sc);
		return (-1);
	}

	/* Set phy/reg values */
	qcom_mdio_ipq4018_set_phy_reg_addr(sc, phy, reg);

	/* Issue read command */
	MDIO_WRITE(sc, QCOM_IPQ4018_MDIO_REG_CMD,
	    QCOM_IPQ4018_MDIO_REG_CMD_ACCESS_START |
	    QCOM_IPQ4018_MDIO_REG_CMD_ACCESS_CODE_READ);
	MDIO_BARRIER_WRITE(sc);

	/* Wait for completion */
	if (qcom_mdio_ipq4018_wait(sc) != 0) {
		MDIO_UNLOCK(sc);
		return (-1);
	}

	/* Fetch return register value */
	MDIO_BARRIER_READ(sc);
	ret = MDIO_READ(sc, QCOM_IPQ4018_MDIO_REG_READ);
	MDIO_UNLOCK(sc);

	QCOM_MDIO_DPRINTF(sc, QCOM_MDIO_DEBUG_REG_READ,
	    "%s: -> 0x%x\n", __func__, ret);

	return (ret);
}

static int
qcom_mdio_ipq4018_writereg(device_t dev, int phy, int reg, int value)
{
	struct qcom_mdio_ipq4018_softc *sc = device_get_softc(dev);

	QCOM_MDIO_DPRINTF(sc, QCOM_MDIO_DEBUG_REG_WRITE,
	    "%s: called; phy=0x%x reg=0x%x val=0x%x\n",
	    __func__, phy, reg, value);

	MDIO_LOCK(sc);
	if (qcom_mdio_ipq4018_wait(sc) != 0) {
		MDIO_UNLOCK(sc);
		return (-1);
	}

	/* Set phy/reg values */
	qcom_mdio_ipq4018_set_phy_reg_addr(sc, phy, reg);

	/* Write command */
	MDIO_WRITE(sc, QCOM_IPQ4018_MDIO_REG_WRITE, value);
	MDIO_BARRIER_WRITE(sc);

	/* Issue write command */
	MDIO_WRITE(sc, QCOM_IPQ4018_MDIO_REG_CMD,
	    QCOM_IPQ4018_MDIO_REG_CMD_ACCESS_START |
	    QCOM_IPQ4018_MDIO_REG_CMD_ACCESS_CODE_WRITE);
	MDIO_BARRIER_WRITE(sc);

	/* Wait for completion */
	if (qcom_mdio_ipq4018_wait(sc) != 0) {
		MDIO_UNLOCK(sc);
		return (-1);
	}
	MDIO_UNLOCK(sc);

	return (0);
}

static device_method_t qcom_mdio_ipq4018_methods[] = {
	/* Driver */
	DEVMETHOD(device_probe, qcom_mdio_ipq4018_probe),
	DEVMETHOD(device_attach, qcom_mdio_ipq4018_attach),
	DEVMETHOD(device_detach, qcom_mdio_ipq4018_detach),

	/* Bus interface */
	DEVMETHOD(bus_add_child, bus_generic_add_child),

	/* MDIO interface */
	DEVMETHOD(mdio_readreg, qcom_mdio_ipq4018_readreg),
	DEVMETHOD(mdio_writereg, qcom_mdio_ipq4018_writereg),

	{0, 0},
};

static driver_t qcom_mdio_ipq4018_driver = {
	"mdio",
	qcom_mdio_ipq4018_methods,
	sizeof(struct qcom_mdio_ipq4018_softc),
};

EARLY_DRIVER_MODULE(qcom_mdio_ipq4018, simplebus, qcom_mdio_ipq4018_driver,
    NULL, NULL, BUS_PASS_SUPPORTDEV + BUS_PASS_ORDER_LATE);

MODULE_DEPEND(qcom_mdio_ipq4018, ether, 1, 1, 1);
MODULE_DEPEND(qcom_mdio_ipq4018, mdio, 1, 1, 1);
MODULE_DEPEND(qcom_mdio_ipq4018, miibus, 1, 1, 1);

MODULE_VERSION(qcom_mdio_ipq4018, 1);
