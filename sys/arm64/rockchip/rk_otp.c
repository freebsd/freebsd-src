/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Soren Schmidt <sos@deepcore.dk>
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

#include "rk_otp.h"
#include "rk_otp_if.h"

#define	OTPC_SBPI_CTRL			0x0020
#define	 SBPI_ENABLE_MASK		0x00010000
#define	 SBPI_ENABLE			1
#define	 SBPI_DAP_ADDR_MASK		0xff000000
#define	 SBPI_DAP_ADDR			0x02
#define	 SBPI_DAP_ADDR_SHIFT		8
#define	OTPC_SBPI_CMD_VALID_PRE		0x0024
#define	 SBPI_CMD_VALID_MASK		0xffff0000
#define	OTPC_SBPI_INT_STATUS		0x0304
#define	 OTPC_SBPI_DONE			2
#define	 OTPC_USER_DONE			4
#define	OTPC_USER_CTRL			0x0100
#define	 OTPC_USER_MASK			0xffff0000
#define	 OTPC_USER			1
#define	OTPC_USER_ADDR			0x0104
#define	 OTPC_USER_ADDR_MASK 		0xffff0000
#define	OTPC_USER_ENABLE		0x0108
#define	 OTPC_USER_FSM_ENABLE_MASK	0xffff0000
#define	 OTPC_USER_FSM_ENABLE		1
#define	OTPC_USER_Q			0x0124
#define	OTPC_SBPI_CMD0_OFFSET		0x1000
#define	 SBPI_DAP_CMD_WRF		0xc0
#define	 SBPI_DAP_REG_ECC		0x3a
#define	OTPC_SBPI_CMD1_OFFSET		0x1004
#define	 SBPI_ECC_ENABLE		0x00
#define	 SBPI_ECC_DISABLE		0x09


static struct ofw_compat_data compat_data[] = {
	{"rockchip,rk3568-otp",	1},
	{NULL,			0}
};

static struct rk_otp_softc {
	struct resource *mem;
} rk_otp_sc;


static int
rk_otp_wait(struct rk_otp_softc *sc, uint32_t status)
{
	int retry = 10000;

	while (!(bus_read_4(sc->mem, OTPC_SBPI_INT_STATUS) & status)) {
		DELAY(10);
		if (--retry == 0)
			return (ETIMEDOUT);
	}

	/* clear status */
	bus_write_4(sc->mem, OTPC_SBPI_INT_STATUS, status);

	return (0);
}

static int
rk_otp_ecc(struct rk_otp_softc *sc, int enable)
{
	bus_write_4(sc->mem, OTPC_SBPI_CTRL,
	    SBPI_DAP_ADDR_MASK | (SBPI_DAP_ADDR << SBPI_DAP_ADDR_SHIFT));
	bus_write_4(sc->mem, OTPC_SBPI_CMD_VALID_PRE,
	    SBPI_CMD_VALID_MASK | 0x1);
	bus_write_4(sc->mem, OTPC_SBPI_CMD0_OFFSET,
	    SBPI_DAP_CMD_WRF | SBPI_DAP_REG_ECC);
	if (enable)
		bus_write_4(sc->mem, OTPC_SBPI_CMD1_OFFSET, SBPI_ECC_ENABLE);
	else
		bus_write_4(sc->mem, OTPC_SBPI_CMD1_OFFSET, SBPI_ECC_DISABLE);
	bus_write_4(sc->mem, OTPC_SBPI_CTRL, SBPI_ENABLE_MASK | SBPI_ENABLE);

	return (rk_otp_wait(sc, OTPC_SBPI_DONE));
}

int
rk_otp_read(device_t dev, uint8_t *buffer, int offset, int size)
{
	struct rk_otp_softc *sc = &rk_otp_sc;
	int error;

	/* if not initialized just error out */
	if (!sc->mem)
		return (ENXIO);

	if ((error = rk_otp_ecc(sc, 1))) {
		device_printf(dev, "timeout waiting for OTP ECC status\n");
		return (error);
	}

	bus_write_4(sc->mem, OTPC_USER_CTRL, OTPC_USER | OTPC_USER_MASK);
	DELAY(5);
	while (size--) {
		bus_write_4(sc->mem, OTPC_USER_ADDR,
		    offset++ | OTPC_USER_ADDR_MASK);
		bus_write_4(sc->mem, OTPC_USER_ENABLE,
		    OTPC_USER_FSM_ENABLE | OTPC_USER_FSM_ENABLE_MASK);

		if ((error = rk_otp_wait(sc, OTPC_USER_DONE))) {
			device_printf(dev, "timeout waiting for OTP data\n");
			break;
		}
		*buffer++ = bus_read_4(sc->mem, OTPC_USER_Q);
	}
	bus_write_4(sc->mem, OTPC_USER_CTRL, OTPC_USER_MASK);

	return (error);
}

static int
rk_otp_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);
	device_set_desc(dev, "RockChip OTP");
	return (BUS_PROBE_DEFAULT);
}

static int
rk_otp_attach(device_t dev)
{
	struct rk_otp_softc *sc = &rk_otp_sc;
	int rid = 0;

	sc->mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (!sc->mem) {
		device_printf(dev, "Cannot allocate memory resources\n");
		return (ENXIO);
	}
	return (0);
}


static device_method_t rk_otp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rk_otp_probe),
	DEVMETHOD(device_attach,	rk_otp_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(rk_otp, rk_otp_driver, rk_otp_methods,
    sizeof(struct simple_mfd_softc), simple_mfd_driver);
EARLY_DRIVER_MODULE(rk_otp, simplebus, rk_otp_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
