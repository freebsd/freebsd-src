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

#define	SCMI_HDR_TOKEN_S		18
#define SCMI_HDR_TOKEN_BF		(0x3fff)
#define	SCMI_HDR_TOKEN_M		(SCMI_HDR_TOKEN_BF << SCMI_HDR_TOKEN_S)

#define	SCMI_HDR_PROTOCOL_ID_S		10
#define	SCMI_HDR_PROTOCOL_ID_BF		(0xff)
#define	SCMI_HDR_PROTOCOL_ID_M		\
    (SCMI_HDR_PROTOCOL_ID_BF << SCMI_HDR_PROTOCOL_ID_S)

#define	SCMI_HDR_MESSAGE_TYPE_S		8
#define	SCMI_HDR_MESSAGE_TYPE_BF	(0x3)
#define	SCMI_HDR_MESSAGE_TYPE_M		\
    (SCMI_HDR_MESSAGE_TYPE_BF << SCMI_HDR_MESSAGE_TYPE_S)

#define	SCMI_HDR_MESSAGE_ID_S		0
#define	SCMI_HDR_MESSAGE_ID_BF		(0xff)
#define	SCMI_HDR_MESSAGE_ID_M		\
    (SCMI_HDR_MESSAGE_ID_BF << SCMI_HDR_MESSAGE_ID_S)

#define SCMI_MSG_TYPE_CMD	0
#define SCMI_MSG_TYPE_DRESP	2
#define SCMI_MSG_TYPE_NOTIF	3

static int
scmi_request_locked(struct scmi_softc *sc, struct scmi_req *req)
{
	uint32_t reply_header;
	int ret;

	SCMI_ASSERT_LOCKED(sc);

	req->msg_header = req->message_id << SCMI_HDR_MESSAGE_ID_S;
	/* TODO: Allocate a token */
	req->msg_header |= SCMI_MSG_TYPE_CMD << SCMI_HDR_MESSAGE_TYPE_S;
	req->msg_header |= req->protocol_id << SCMI_HDR_PROTOCOL_ID_S;

	ret = scmi_shmem_prepare_msg(sc->a2p_dev, req, cold);
	if (ret != 0)
		return (ret);

	ret = SCMI_XFER_MSG(sc->dev);
	if (ret != 0)
		return (ret);

	/* Read header. */
	ret = scmi_shmem_read_msg_header(sc->a2p_dev, &reply_header);
	if (ret != 0)
		return (ret);

	if (reply_header != req->msg_header)
		return (EPROTO);

	return (scmi_shmem_read_msg_payload(sc->a2p_dev, req->out_buf,
	    req->out_size));
}

int
scmi_request(device_t dev, struct scmi_req *req)
{
	struct scmi_softc *sc;
	int error;

	sc = device_get_softc(dev);

	SCMI_LOCK(sc);
	error = scmi_request_locked(sc, req);
	SCMI_UNLOCK(sc);

	return (error);
}

int
scmi_attach(device_t dev)
{
	struct scmi_softc *sc;
	phandle_t node;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;

	node = ofw_bus_get_node(dev);
	if (node == -1)
		return (ENXIO);

	sc->a2p_dev = scmi_shmem_get(dev, node, SCMI_CHAN_A2P);
	if (sc->a2p_dev == NULL) {
		device_printf(dev, "A2P shmem dev not found.\n");
		return (ENXIO);
	}

	mtx_init(&sc->mtx, device_get_nameunit(dev), "SCMI", MTX_DEF);

	simplebus_init(dev, node);

	/*
	 * Allow devices to identify.
	 */
	bus_generic_probe(dev);

	/*
	 * Now walk the OFW tree and attach top-level devices.
	 */
	for (node = OF_child(node); node > 0; node = OF_peer(node))
		simplebus_add_device(dev, node, 0, NULL, -1, NULL);

	error = bus_generic_attach(dev);

	return (error);
}

static int
scmi_detach(device_t dev)
{

	return (0);
}

static device_method_t scmi_methods[] = {
	DEVMETHOD(device_attach,	scmi_attach),
	DEVMETHOD(device_detach,	scmi_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(scmi, scmi_driver, scmi_methods, sizeof(struct scmi_softc),
    simplebus_driver);

DRIVER_MODULE(scmi, simplebus, scmi_driver, 0, 0);
MODULE_VERSION(scmi, 1);
