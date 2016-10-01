/*-
 * Copyright (c) 2014,2016 Microsoft Corp.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/reboot.h>
#include <sys/systm.h>

#include <dev/hyperv/include/hyperv.h>
#include <dev/hyperv/include/vmbus.h>
#include <dev/hyperv/utilities/hv_util.h>
#include <dev/hyperv/utilities/vmbus_icreg.h>

#include "vmbus_if.h"

static const struct vmbus_ic_desc vmbus_shutdown_descs[] = {
	{
		.ic_guid = { .hv_guid = {
		    0x31, 0x60, 0x0b, 0x0e, 0x13, 0x52, 0x34, 0x49,
		    0x81, 0x8b, 0x38, 0xd9, 0x0c, 0xed, 0x39, 0xdb } },
		.ic_desc = "Hyper-V Shutdown"
	},
	VMBUS_IC_DESC_END
};

static void
vmbus_shutdown_cb(struct vmbus_channel *chan, void *xsc)
{
	struct hv_util_sc *sc = xsc;
	struct vmbus_icmsg_hdr *hdr;
	struct vmbus_icmsg_shutdown *msg;
	int dlen, error, do_shutdown = 0;
	uint64_t xactid;
	void *data;

	/*
	 * Receive request.
	 */
	data = sc->receive_buffer;
	dlen = sc->ic_buflen;
	error = vmbus_chan_recv(chan, data, &dlen, &xactid);
	KASSERT(error != ENOBUFS, ("icbuf is not large enough"));
	if (error)
		return;

	if (dlen < sizeof(*hdr)) {
		device_printf(sc->ic_dev, "invalid data len %d\n", dlen);
		return;
	}
	hdr = data;

	/*
	 * Update request, which will be echoed back as response.
	 */
	switch (hdr->ic_type) {
	case VMBUS_ICMSG_TYPE_NEGOTIATE:
		error = vmbus_ic_negomsg(sc, data, &dlen);
		if (error)
			return;
		break;

	case VMBUS_ICMSG_TYPE_SHUTDOWN:
		if (dlen < VMBUS_ICMSG_SHUTDOWN_SIZE_MIN) {
			device_printf(sc->ic_dev, "invalid shutdown len %d\n",
			    dlen);
			return;
		}
		msg = data;

		/* XXX ic_flags definition? */
		if (msg->ic_haltflags == 0 || msg->ic_haltflags == 1) {
			device_printf(sc->ic_dev, "shutdown requested\n");
			hdr->ic_status = VMBUS_ICMSG_STATUS_OK;
			do_shutdown = 1;
		} else {
			device_printf(sc->ic_dev, "unknown shutdown flags "
			    "0x%08x\n", msg->ic_haltflags);
			hdr->ic_status = VMBUS_ICMSG_STATUS_FAIL;
		}
		break;

	default:
		device_printf(sc->ic_dev, "got 0x%08x icmsg\n", hdr->ic_type);
		break;
	}

	/*
	 * Send response by echoing the updated request back.
	 */
	hdr->ic_flags = VMBUS_ICMSG_FLAG_XACT | VMBUS_ICMSG_FLAG_RESP;
	error = vmbus_chan_send(chan, VMBUS_CHANPKT_TYPE_INBAND, 0,
	    data, dlen, xactid);
	if (error)
		device_printf(sc->ic_dev, "resp send failed: %d\n", error);

	if (do_shutdown)
		shutdown_nice(RB_POWEROFF);
}

static int
hv_shutdown_probe(device_t dev)
{

	return (vmbus_ic_probe(dev, vmbus_shutdown_descs));
}

static int
hv_shutdown_attach(device_t dev)
{

	return (hv_util_attach(dev, vmbus_shutdown_cb));
}

static device_method_t shutdown_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, hv_shutdown_probe),
	DEVMETHOD(device_attach, hv_shutdown_attach),
	DEVMETHOD(device_detach, hv_util_detach),
	{ 0, 0 }
};

static driver_t shutdown_driver = { "hvshutdown", shutdown_methods, sizeof(hv_util_sc)};

static devclass_t shutdown_devclass;

DRIVER_MODULE(hv_shutdown, vmbus, shutdown_driver, shutdown_devclass, NULL, NULL);
MODULE_VERSION(hv_shutdown, 1);
MODULE_DEPEND(hv_shutdown, vmbus, 1, 1, 1);
