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
 *
 * $FreeBSD$
 */

/*
 * A common driver for all hyper-V util services.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/reboot.h>
#include <sys/timetc.h>
#include <sys/syscallsubr.h>

#include <dev/hyperv/include/hyperv.h>
#include "hv_util.h"

static hv_guid service_guid = { .data =
	{0x31, 0x60, 0x0B, 0X0E, 0x13, 0x52, 0x34, 0x49,
	0x81, 0x8B, 0x38, 0XD9, 0x0C, 0xED, 0x39, 0xDB} };

/**
 * Shutdown
 */
static void
hv_shutdown_cb(void *context)
{
	uint8_t*			buf;
	hv_vmbus_channel*		channel;
	uint8_t				execute_shutdown = 0;
	hv_vmbus_icmsg_hdr*		icmsghdrp;
	uint32_t			recv_len;
	uint64_t			request_id;
	int				ret;
	hv_vmbus_shutdown_msg_data*	shutdown_msg;
	hv_util_sc			*softc;

	softc = (hv_util_sc*)context;
	buf = softc->receive_buffer;
	channel = softc->hv_dev->channel;
	ret = hv_vmbus_channel_recv_packet(channel, buf, PAGE_SIZE,
					    &recv_len, &request_id);

	if ((ret == 0) && recv_len > 0) {

	    icmsghdrp = (struct hv_vmbus_icmsg_hdr *)
		&buf[sizeof(struct hv_vmbus_pipe_hdr)];

	    if (icmsghdrp->icmsgtype == HV_ICMSGTYPE_NEGOTIATE) {
		hv_negotiate_version(icmsghdrp, NULL, buf);

	    } else {
		shutdown_msg =
		    (struct hv_vmbus_shutdown_msg_data *)
		    &buf[sizeof(struct hv_vmbus_pipe_hdr) +
			sizeof(struct hv_vmbus_icmsg_hdr)];

		switch (shutdown_msg->flags) {
		    case 0:
		    case 1:
			icmsghdrp->status = HV_S_OK;
			execute_shutdown = 1;
			if(bootverbose)
			    printf("Shutdown request received -"
				    " graceful shutdown initiated\n");
			break;
		    default:
			icmsghdrp->status = HV_E_FAIL;
			execute_shutdown = 0;
			printf("Shutdown request received -"
			    " Invalid request\n");
			break;
		    }
	    }

	icmsghdrp->icflags = HV_ICMSGHDRFLAG_TRANSACTION |
				 HV_ICMSGHDRFLAG_RESPONSE;

	    hv_vmbus_channel_send_packet(channel, buf,
					recv_len, request_id,
					HV_VMBUS_PACKET_TYPE_DATA_IN_BAND, 0);
	}

	if (execute_shutdown)
	    shutdown_nice(RB_POWEROFF);
}

static int
hv_shutdown_probe(device_t dev)
{
	const char *p = vmbus_get_type(dev);

	if (resource_disabled("hvshutdown", 0))
		return ENXIO;

	if (!memcmp(p, &service_guid, sizeof(hv_guid))) {
		device_set_desc(dev, "Hyper-V Shutdown Service");
		return BUS_PROBE_DEFAULT;
	}

	return ENXIO;
}

static int
hv_shutdown_attach(device_t dev)
{
	hv_util_sc *softc = (hv_util_sc*)device_get_softc(dev);

	softc->callback = hv_shutdown_cb;

	return hv_util_attach(dev);
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
