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
#include <dev/hyperv/include/vmbus.h>
#include <dev/hyperv/utilities/hv_utilreg.h>
#include "hv_util.h"

#define VMBUS_IC_BRSIZE		(4 * PAGE_SIZE)

void
hv_negotiate_version(struct hv_vmbus_icmsg_hdr *icmsghdrp, uint8_t *buf)
{
	struct hv_vmbus_icmsg_negotiate *negop;

	icmsghdrp->icmsgsize = 0x10;

	negop = (struct hv_vmbus_icmsg_negotiate *)&buf[
		sizeof(struct hv_vmbus_pipe_hdr) +
		sizeof(struct hv_vmbus_icmsg_hdr)];

	if (negop->icframe_vercnt >= 2 &&
	    negop->icversion_data[1].major == 3) {
		negop->icversion_data[0].major = 3;
		negop->icversion_data[0].minor = 0;
		negop->icversion_data[1].major = 3;
		negop->icversion_data[1].minor = 0;
	} else {
		negop->icversion_data[0].major = 1;
		negop->icversion_data[0].minor = 0;
		negop->icversion_data[1].major = 1;
		negop->icversion_data[1].minor = 0;
	}

	negop->icframe_vercnt = 1;
	negop->icmsg_vercnt = 1;
}

int
hv_util_attach(device_t dev, vmbus_chan_callback_t cb)
{
	struct hv_util_sc *sc = device_get_softc(dev);
	struct vmbus_channel *chan = vmbus_get_channel(dev);
	int error;

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
