/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Ruslan Bukin <br@bsdpad.com>
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

static device_t
scmi_get_shmem(struct scmi_softc *sc, int index)
{
	phandle_t *shmems;
	phandle_t node;
	device_t dev;
	size_t len;

	node = ofw_bus_get_node(sc->dev);
	if (node <= 0)
		return (NULL);

	len = OF_getencprop_alloc_multi(node, "shmem", sizeof(*shmems),
	    (void **)&shmems);
	if (len <= 0) {
		device_printf(sc->dev, "%s: Can't get shmem node.\n", __func__);
		return (NULL);
	}

	if (index >= len) {
		OF_prop_free(shmems);
		return (NULL);
	}

	dev = OF_device_from_xref(shmems[index]);
	if (dev == NULL)
		device_printf(sc->dev, "%s: Can't get shmem device.\n",
		    __func__);

	OF_prop_free(shmems);

	return (dev);
}

static int
scmi_request_locked(struct scmi_softc *sc, struct scmi_req *req)
{
	struct scmi_smt_header hdr;
	int ret;

	bzero(&hdr, sizeof(struct scmi_smt_header));

	SCMI_ASSERT_LOCKED(sc);

	/* Read header */
	scmi_shmem_read(sc->tx_shmem, 0, &hdr, SMT_HEADER_SIZE);

	if ((hdr.channel_status & SCMI_SHMEM_CHAN_STAT_CHANNEL_FREE) == 0)
		return (1);

	/* Update header */
	hdr.channel_status &= ~SCMI_SHMEM_CHAN_STAT_CHANNEL_FREE;
	hdr.msg_header = req->protocol_id << SMT_HEADER_PROTOCOL_ID_S;
	hdr.msg_header |= req->message_id << SMT_HEADER_MESSAGE_ID_S;
	/* TODO: Allocate a token */
	hdr.length = sizeof(hdr.msg_header) + req->in_size;
	hdr.flags |= SCMI_SHMEM_FLAG_INTR_ENABLED;

	/* Write header */
	scmi_shmem_write(sc->tx_shmem, 0, &hdr, SMT_HEADER_SIZE);

	/* Write request */
	scmi_shmem_write(sc->tx_shmem, SMT_HEADER_SIZE, req->in_buf,
	    req->in_size);

	ret = SCMI_XFER_MSG(sc->dev);
	if (ret != 0)
		return (ret);

	/* Read header. */
	scmi_shmem_read(sc->tx_shmem, 0, &hdr, SMT_HEADER_SIZE);

	/* Read response */
	scmi_shmem_read(sc->tx_shmem, SMT_HEADER_SIZE, req->out_buf,
	    req->out_size);

	return (0);
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

	sc->tx_shmem = scmi_get_shmem(sc, 0);
	if (sc->tx_shmem == NULL) {
		device_printf(dev, "TX shmem dev not found.\n");
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
