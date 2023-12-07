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
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <dev/clk/clk.h>
#include <dev/fdt/simplebus.h>
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "dev/mailbox/arm/arm_doorbell.h"

#include "scmi.h"
#include "scmi_protocols.h"
#include "scmi_shmem.h"

struct scmi_mailbox_softc {
	struct scmi_softc	base;
	device_t		a2p_dev;
	struct arm_doorbell	*db;
	int			req_done;
};

static int	scmi_mailbox_transport_init(device_t);
static void	scmi_mailbox_transport_cleanup(device_t);
static int	scmi_mailbox_xfer_msg(device_t, struct scmi_req *);
static int	scmi_mailbox_collect_reply(device_t, struct scmi_req *);
static void	scmi_mailbox_tx_complete(device_t, void *);

static int	scmi_mailbox_probe(device_t);

static void
scmi_mailbox_callback(void *arg)
{
	struct scmi_mailbox_softc *sc;

	sc = arg;

	dprintf("%s sc %p\n", __func__, sc);

	SCMI_LOCK(&sc->base);
	sc->req_done = 1;
	wakeup(sc);
	SCMI_UNLOCK(&sc->base);
}

static int
scmi_mailbox_transport_init(device_t dev)
{
	struct scmi_mailbox_softc *sc;
	phandle_t node;

	sc = device_get_softc(dev);

	node = ofw_bus_get_node(dev);
	if (node == -1)
		return (ENXIO);
	/*
	 * TODO
	 * - Support P2A shmem + IRQ/doorbell
	 * - Support other mailbox devices
	 */
	sc->a2p_dev = scmi_shmem_get(dev, node, SCMI_CHAN_A2P);
	if (sc->a2p_dev == NULL) {
		device_printf(dev, "A2P shmem dev not found.\n");
		return (ENXIO);
	}

	/* TODO: Fix ofw_get...mbox doorbell names NOT required in Linux DT */
	sc->db = arm_doorbell_ofw_get(dev, "tx");
	if (sc->db == NULL) {
		device_printf(dev, "Doorbell device not found.\n");
		return (ENXIO);
	}

	arm_doorbell_set_handler(sc->db, scmi_mailbox_callback, sc);

	return (0);
}

static void
scmi_mailbox_transport_cleanup(device_t dev)
{
	struct scmi_mailbox_softc *sc;

	sc = device_get_softc(dev);

	arm_doorbell_set_handler(sc->db, NULL, NULL);
}

static int
scmi_mailbox_xfer_msg(device_t dev, struct scmi_req *req)
{
	struct scmi_mailbox_softc *sc;
	int ret, timeout;

	sc = device_get_softc(dev);
	SCMI_ASSERT_LOCKED(&sc->base);

	sc->req_done = 0;

	ret = scmi_shmem_prepare_msg(sc->a2p_dev, req, cold);
	if (ret != 0)
		return (ret);

	/* Interrupt SCP firmware. */
	arm_doorbell_set(sc->db);

	timeout = 200;

	dprintf("%s: request\n", __func__);

	do {
		if (cold) {
			if (scmi_shmem_poll_msg(sc->a2p_dev))
				break;
			DELAY(10000);
		} else {
			msleep(sc, &sc->base.mtx, 0, "scmi", hz / 10);
			if (sc->req_done)
				break;
		}
	} while (timeout--);

	if (timeout <= 0)
		return (ETIMEDOUT);

	dprintf("%s: got reply, timeout %d\n", __func__, timeout);

	return (0);
}

static int
scmi_mailbox_collect_reply(device_t dev, struct scmi_req *req)
{
	struct scmi_mailbox_softc *sc;
	uint32_t reply_header;
	int ret;

	sc = device_get_softc(dev);

	/* Read header. */
	ret = scmi_shmem_read_msg_header(sc->a2p_dev, &reply_header);
	if (ret != 0)
		return (ret);

	if (reply_header != req->msg_header)
		return (EPROTO);

	return (scmi_shmem_read_msg_payload(sc->a2p_dev, req->out_buf,
	    req->out_size));
}

static void
scmi_mailbox_tx_complete(device_t dev, void *chan)
{
	struct scmi_mailbox_softc *sc;

	sc = device_get_softc(dev);
	scmi_shmem_tx_complete(sc->a2p_dev);
}

static int
scmi_mailbox_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "arm,scmi"))
		return (ENXIO);

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	device_set_desc(dev, "ARM SCMI interface driver");

	return (BUS_PROBE_DEFAULT);
}

static device_method_t scmi_mailbox_methods[] = {
	DEVMETHOD(device_probe,		scmi_mailbox_probe),

	/* SCMI interface */
	DEVMETHOD(scmi_transport_init,		scmi_mailbox_transport_init),
	DEVMETHOD(scmi_transport_cleanup,	scmi_mailbox_transport_cleanup),
	DEVMETHOD(scmi_xfer_msg,		scmi_mailbox_xfer_msg),
	DEVMETHOD(scmi_collect_reply,		scmi_mailbox_collect_reply),
	DEVMETHOD(scmi_tx_complete,		scmi_mailbox_tx_complete),

	DEVMETHOD_END
};

DEFINE_CLASS_1(scmi_mailbox, scmi_mailbox_driver, scmi_mailbox_methods,
    sizeof(struct scmi_mailbox_softc), scmi_driver);

DRIVER_MODULE(scmi_mailbox, simplebus, scmi_mailbox_driver, 0, 0);
MODULE_VERSION(scmi_mailbox, 1);
