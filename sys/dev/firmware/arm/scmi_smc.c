/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Ruslan Bukin <br@bsdpad.com>
 * Copyright (c) 2023 Arm Ltd
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/fdt/simplebus.h>
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/psci/smccc.h>

#include "scmi.h"
#include "scmi_protocols.h"
#include "scmi_shmem.h"

struct scmi_smc_softc {
	struct scmi_softc	base;
	uint32_t		smc_id;
	device_t		a2p_dev;
};

static int	scmi_smc_transport_init(device_t);
static int	scmi_smc_xfer_msg(device_t, struct scmi_msg *);
static int	scmi_smc_poll_msg(device_t, struct scmi_msg *, unsigned int);
static int	scmi_smc_collect_reply(device_t, struct scmi_msg *);
static void	scmi_smc_tx_complete(device_t, void *);

static int	scmi_smc_probe(device_t);

static int
scmi_smc_transport_init(device_t dev)
{
	struct scmi_smc_softc *sc;
	phandle_t node;
	ssize_t len;

	sc = device_get_softc(dev);

	node = ofw_bus_get_node(dev);
	len = OF_getencprop(node, "arm,smc-id", &sc->smc_id,
	    sizeof(sc->smc_id));
	if (len <= 0) {
		device_printf(dev, "No SMC ID found\n");
		return (EINVAL);
	}

	device_printf(dev, "smc id %x\n", sc->smc_id);

	sc->a2p_dev = scmi_shmem_get(dev, node, SCMI_CHAN_A2P);
	if (sc->a2p_dev == NULL) {
		device_printf(dev, "A2P shmem dev not found.\n");
		return (ENXIO);
	}

	sc->base.trs_desc.no_completion_irq = true;
	sc->base.trs_desc.reply_timo_ms = 30;

	return (0);
}

static int
scmi_smc_xfer_msg(device_t dev, struct scmi_msg *msg)
{
	struct scmi_smc_softc *sc;
	int ret;

	sc = device_get_softc(dev);

	ret = scmi_shmem_prepare_msg(sc->a2p_dev, (uint8_t *)&msg->hdr,
	    msg->tx_len, msg->polling);
	if (ret != 0)
		return (ret);

	arm_smccc_invoke_smc(sc->smc_id, NULL);

	return (0);
}

static int
scmi_smc_poll_msg(device_t dev, struct scmi_msg *msg, unsigned int tmo)
{
	struct scmi_smc_softc *sc;

	sc = device_get_softc(dev);

	/*
	 * Nothing to poll since commands are completed as soon as smc
	 * returns ... but did we get back what we were poling for ?
	 */
	scmi_shmem_read_msg_header(sc->a2p_dev, &msg->hdr, &msg->rx_len);

	return (0);
}

static int
scmi_smc_collect_reply(device_t dev, struct scmi_msg *msg)
{
	struct scmi_smc_softc *sc;
	int ret;

	sc = device_get_softc(dev);

	ret = scmi_shmem_read_msg_payload(sc->a2p_dev,
	    msg->payld, msg->rx_len - SCMI_MSG_HDR_SIZE, msg->rx_len);

	return (ret);
}

static void
scmi_smc_tx_complete(device_t dev, void *chan)
{
	struct scmi_smc_softc *sc;

	sc = device_get_softc(dev);
	scmi_shmem_tx_complete(sc->a2p_dev);
}

static int
scmi_smc_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "arm,scmi-smc"))
		return (ENXIO);

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	device_set_desc(dev, "ARM SCMI SMC Transport driver");

	return (BUS_PROBE_DEFAULT);
}

static device_method_t scmi_smc_methods[] = {
	DEVMETHOD(device_probe,		scmi_smc_probe),

	/* SCMI interface */
	DEVMETHOD(scmi_transport_init,		scmi_smc_transport_init),
	DEVMETHOD(scmi_xfer_msg,		scmi_smc_xfer_msg),
	DEVMETHOD(scmi_poll_msg,		scmi_smc_poll_msg),
	DEVMETHOD(scmi_collect_reply,		scmi_smc_collect_reply),
	DEVMETHOD(scmi_tx_complete,		scmi_smc_tx_complete),

	DEVMETHOD_END
};

DEFINE_CLASS_1(scmi_smc, scmi_smc_driver, scmi_smc_methods,
    sizeof(struct scmi_smc_softc), scmi_driver);

/* Needs to be after the mmio_sram driver */
EARLY_DRIVER_MODULE(scmi_smc, simplebus, scmi_smc_driver, 0, 0,
    BUS_PASS_SUPPORTDEV + BUS_PASS_ORDER_LATE);
MODULE_VERSION(scmi_smc, 1);
