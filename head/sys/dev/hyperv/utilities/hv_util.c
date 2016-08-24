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
#include <sys/systm.h>
#include <sys/timetc.h>

#include <dev/hyperv/include/hyperv.h>
#include <dev/hyperv/include/vmbus.h>
#include <dev/hyperv/utilities/hv_util.h>
#include <dev/hyperv/utilities/vmbus_icreg.h>

#include "vmbus_if.h"

#define VMBUS_IC_BRSIZE		(4 * PAGE_SIZE)

CTASSERT(sizeof(struct vmbus_icmsg_negotiate) < VMBUS_IC_BRSIZE);

int
vmbus_ic_negomsg(struct hv_util_sc *sc, void *data, int dlen)
{
	struct vmbus_icmsg_negotiate *nego;
	int cnt, major;

	/*
	 * Preliminary message size verification
	 */
	if (dlen < sizeof(*nego)) {
		device_printf(sc->ic_dev, "truncated ic negotiate, len %d\n",
		    dlen);
		return EINVAL;
	}
	nego = data;

	cnt = nego->ic_fwver_cnt + nego->ic_msgver_cnt;
	if (dlen < __offsetof(struct vmbus_icmsg_negotiate, ic_ver[cnt])) {
		device_printf(sc->ic_dev, "ic negotiate does not contain "
		    "versions %d\n", dlen);
		return EINVAL;
	}

	/* Select major version; XXX looks wrong. */
	if (nego->ic_fwver_cnt >= 2 && VMBUS_ICVER_MAJOR(nego->ic_ver[1]) == 3)
		major = 3;
	else
		major = 1;

	/* One framework version */
	nego->ic_fwver_cnt = 1;
	nego->ic_ver[0] = VMBUS_IC_VERSION(major, 0);

	/* One message version */
	nego->ic_msgver_cnt = 1;
	nego->ic_ver[1] = VMBUS_IC_VERSION(major, 0);

	/* Data contains two versions */
	nego->ic_hdr.ic_dsize = __offsetof(struct vmbus_icmsg_negotiate,
	    ic_ver[2]) - sizeof(struct vmbus_icmsg_hdr);

	return 0;
}

int
vmbus_ic_probe(device_t dev, const struct vmbus_ic_desc descs[])
{
	device_t bus = device_get_parent(dev);
	const struct vmbus_ic_desc *d;

	if (resource_disabled(device_get_name(dev), 0))
		return (ENXIO);

	for (d = descs; d->ic_desc != NULL; ++d) {
		if (VMBUS_PROBE_GUID(bus, dev, &d->ic_guid) == 0) {
			device_set_desc(dev, d->ic_desc);
			return (BUS_PROBE_DEFAULT);
		}
	}
	return (ENXIO);
}

int
hv_util_attach(device_t dev, vmbus_chan_callback_t cb)
{
	struct hv_util_sc *sc = device_get_softc(dev);
	struct vmbus_channel *chan = vmbus_get_channel(dev);
	int error;

	sc->ic_dev = dev;
	sc->ic_buflen = VMBUS_IC_BRSIZE;
	sc->receive_buffer = malloc(VMBUS_IC_BRSIZE, M_DEVBUF,
	    M_WAITOK | M_ZERO);

	/*
	 * These services are not performance critical and do not need
	 * batched reading. Furthermore, some services such as KVP can
	 * only handle one message from the host at a time.
	 * Turn off batched reading for all util drivers before we open the
	 * channel.
	 */
	vmbus_chan_set_readbatch(chan, false);

	error = vmbus_chan_open(chan, VMBUS_IC_BRSIZE, VMBUS_IC_BRSIZE, NULL, 0,
	    cb, sc);
	if (error) {
		free(sc->receive_buffer, M_DEVBUF);
		return (error);
	}
	return (0);
}

int
hv_util_detach(device_t dev)
{
	struct hv_util_sc *sc = device_get_softc(dev);

	vmbus_chan_close(vmbus_get_channel(dev));
	free(sc->receive_buffer, M_DEVBUF);

	return (0);
}
