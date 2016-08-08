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
#include <sys/systm.h>
#include <sys/taskqueue.h>

#include <dev/hyperv/include/hyperv.h>
#include <dev/hyperv/include/vmbus.h>
#include <dev/hyperv/utilities/hv_utilreg.h>
#include "hv_util.h"
#include "vmbus_if.h"

#define HV_WLTIMEDELTA              116444736000000000L     /* in 100ns unit */
#define HV_ICTIMESYNCFLAG_PROBE     0
#define HV_ICTIMESYNCFLAG_SYNC      1
#define HV_ICTIMESYNCFLAG_SAMPLE    2
#define HV_NANO_SEC_PER_SEC         1000000000

/* Time Sync data */
typedef struct {
	uint64_t data;
} time_sync_data;

        /* Time Synch Service */
static const struct hyperv_guid service_guid = {.hv_guid =
	{0x30, 0xe6, 0x27, 0x95, 0xae, 0xd0, 0x7b, 0x49,
	0xad, 0xce, 0xe8, 0x0a, 0xb0, 0x17, 0x5c, 0xaf } };

struct hv_ictimesync_data {
	uint64_t    parenttime;
	uint64_t    childtime;
	uint64_t    roundtriptime;
	uint8_t     flags;
} __packed;

typedef struct hv_timesync_sc {
	hv_util_sc	util_sc;
	struct task	task;
	time_sync_data	time_msg;
} hv_timesync_sc;

/**
 * Set host time based on time sync message from host
 */
static void
hv_set_host_time(void *context, int pending)
{
	hv_timesync_sc *softc = (hv_timesync_sc*)context;
	uint64_t hosttime = softc->time_msg.data;
	struct timespec guest_ts, host_ts;
	uint64_t host_tns;
	int64_t diff;
	int error;

	host_tns = (hosttime - HV_WLTIMEDELTA) * 100;
	host_ts.tv_sec = (time_t)(host_tns/HV_NANO_SEC_PER_SEC);
	host_ts.tv_nsec = (long)(host_tns%HV_NANO_SEC_PER_SEC);

	nanotime(&guest_ts);

	diff = (int64_t)host_ts.tv_sec - (int64_t)guest_ts.tv_sec;

	/*
	 * If host differs by 5 seconds then make the guest catch up
	 */
	if (diff > 5 || diff < -5) {
		error = kern_clock_settime(curthread, CLOCK_REALTIME,
		    &host_ts);
	}
}

/**
 * @brief Synchronize time with host after reboot, restore, etc.
 *
 * ICTIMESYNCFLAG_SYNC flag bit indicates reboot, restore events of the VM.
 * After reboot the flag ICTIMESYNCFLAG_SYNC is included in the first time
 * message after the timesync channel is opened. Since the hv_utils module is
 * loaded after hv_vmbus, the first message is usually missed. The other
 * thing is, systime is automatically set to emulated hardware clock which may
 * not be UTC time or in the same time zone. So, to override these effects, we
 * use the first 50 time samples for initial system time setting.
 */
static inline
void hv_adj_guesttime(hv_timesync_sc *sc, uint64_t hosttime, uint8_t flags)
{
	sc->time_msg.data = hosttime;

	if (((flags & HV_ICTIMESYNCFLAG_SYNC) != 0) ||
		((flags & HV_ICTIMESYNCFLAG_SAMPLE) != 0)) {
		taskqueue_enqueue(taskqueue_thread, &sc->task);
	}
}

/**
 * Time Sync Channel message handler
 */
static void
hv_timesync_cb(struct vmbus_channel *channel, void *context)
{
	hv_vmbus_icmsg_hdr*	icmsghdrp;
	uint32_t		recvlen;
	uint64_t		requestId;
	int			ret;
	uint8_t*		time_buf;
	struct hv_ictimesync_data* timedatap;
	hv_timesync_sc		*softc;

	softc = (hv_timesync_sc*)context;
	time_buf = softc->util_sc.receive_buffer;

	recvlen = softc->util_sc.ic_buflen;
	ret = vmbus_chan_recv(channel, time_buf, &recvlen, &requestId);
	KASSERT(ret != ENOBUFS, ("hvtimesync recvbuf is not large enough"));
	/* XXX check recvlen to make sure that it contains enough data */

	if ((ret == 0) && recvlen > 0) {
	    icmsghdrp = (struct hv_vmbus_icmsg_hdr *) &time_buf[
		sizeof(struct hv_vmbus_pipe_hdr)];

	    if (icmsghdrp->icmsgtype == HV_ICMSGTYPE_NEGOTIATE) {
		hv_negotiate_version(icmsghdrp, time_buf);
	    } else {
		timedatap = (struct hv_ictimesync_data *) &time_buf[
		    sizeof(struct hv_vmbus_pipe_hdr) +
			sizeof(struct hv_vmbus_icmsg_hdr)];
		hv_adj_guesttime(softc, timedatap->parenttime, timedatap->flags);
	    }

	    icmsghdrp->icflags = HV_ICMSGHDRFLAG_TRANSACTION
		| HV_ICMSGHDRFLAG_RESPONSE;

	    vmbus_chan_send(channel, VMBUS_CHANPKT_TYPE_INBAND, 0,
	        time_buf, recvlen, requestId);
	}
}

static int
hv_timesync_probe(device_t dev)
{
	if (resource_disabled("hvtimesync", 0))
		return ENXIO;

	if (VMBUS_PROBE_GUID(device_get_parent(dev), dev, &service_guid) == 0) {
		device_set_desc(dev, "Hyper-V Time Synch Service");
		return BUS_PROBE_DEFAULT;
	}
	return ENXIO;
}

static int
hv_timesync_attach(device_t dev)
{
	hv_timesync_sc *softc = device_get_softc(dev);

	softc->util_sc.callback = hv_timesync_cb;
	TASK_INIT(&softc->task, 1, hv_set_host_time, softc);

	return hv_util_attach(dev);
}

static int
hv_timesync_detach(device_t dev)
{
	hv_timesync_sc *softc = device_get_softc(dev);
	taskqueue_drain(taskqueue_thread, &softc->task);

	return hv_util_detach(dev);
}

static device_method_t timesync_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, hv_timesync_probe),
	DEVMETHOD(device_attach, hv_timesync_attach),
	DEVMETHOD(device_detach, hv_timesync_detach),
	{ 0, 0 }
};

static driver_t timesync_driver = { "hvtimesync", timesync_methods, sizeof(hv_timesync_sc)};

static devclass_t timesync_devclass;

DRIVER_MODULE(hv_timesync, vmbus, timesync_driver, timesync_devclass, NULL, NULL);
MODULE_VERSION(hv_timesync, 1);
MODULE_DEPEND(hv_timesync, vmbus, 1, 1, 1);
