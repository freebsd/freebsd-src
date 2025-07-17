/*-
 * Copyright (c) 2015 Nathan Whitehorn
 * All rights reserved.
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
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/clock.h>
#include <sys/cpu.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/reboot.h>
#include <sys/sysctl.h>
#include <sys/endian.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include "clock_if.h"
#include "opal.h"

static int	opaldev_probe(device_t);
static int	opaldev_attach(device_t);
/* clock interface */
static int	opal_gettime(device_t dev, struct timespec *ts);
static int	opal_settime(device_t dev, struct timespec *ts);
/* ofw bus interface */
static const struct ofw_bus_devinfo *opaldev_get_devinfo(device_t dev,
    device_t child);

static void	opal_shutdown(void *arg, int howto);
static void	opal_handle_shutdown_message(void *unused,
    struct opal_msg *msg);
static void	opal_intr(void *);

static device_method_t  opaldev_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		opaldev_probe),
	DEVMETHOD(device_attach,	opaldev_attach),

	/* clock interface */
	DEVMETHOD(clock_gettime,	opal_gettime),
	DEVMETHOD(clock_settime,	opal_settime),

	/* Bus interface */
	DEVMETHOD(bus_child_pnpinfo,	ofw_bus_gen_child_pnpinfo),

        /* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,	opaldev_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	DEVMETHOD_END
};

static driver_t opaldev_driver = {
	"opal",
	opaldev_methods,
	0
};

EARLY_DRIVER_MODULE(opaldev, ofwbus, opaldev_driver, 0, 0, BUS_PASS_BUS);

static void opal_heartbeat(void);
static void opal_handle_messages(void);

static struct proc *opal_hb_proc;
static struct kproc_desc opal_heartbeat_kp = {
	"opal_heartbeat",
	opal_heartbeat,
	&opal_hb_proc
};

SYSINIT(opal_heartbeat_setup, SI_SUB_KTHREAD_IDLE, SI_ORDER_ANY, kproc_start,
    &opal_heartbeat_kp);

static int opal_heartbeat_ms;
EVENTHANDLER_LIST_DEFINE(OPAL_ASYNC_COMP);
EVENTHANDLER_LIST_DEFINE(OPAL_EPOW);
EVENTHANDLER_LIST_DEFINE(OPAL_SHUTDOWN);
EVENTHANDLER_LIST_DEFINE(OPAL_HMI_EVT);
EVENTHANDLER_LIST_DEFINE(OPAL_DPO);
EVENTHANDLER_LIST_DEFINE(OPAL_OCC);

#define	OPAL_SOFT_OFF		0
#define	OPAL_SOFT_REBOOT	1

static void
opal_heartbeat(void)
{
	uint64_t events;

	if (opal_heartbeat_ms == 0)
		kproc_exit(0);

	while (1) {
		events = 0;
		/* Turn the OPAL state crank */
		opal_call(OPAL_POLL_EVENTS, vtophys(&events));
		if (be64toh(events) & OPAL_EVENT_MSG_PENDING)
			opal_handle_messages();
		tsleep(opal_hb_proc, 0, "opal",
		    MSEC_2_TICKS(opal_heartbeat_ms));
	}
}

static int
opaldev_probe(device_t dev)
{
	phandle_t iparent;
	pcell_t *irqs;
	int i, n_irqs;

	if (!ofw_bus_is_compatible(dev, "ibm,opal-v3"))
		return (ENXIO);
	if (opal_check() != 0)
		return (ENXIO);

	device_set_desc(dev, "OPAL Abstraction Firmware");

	/* Manually add IRQs before attaching */
	if (OF_hasprop(ofw_bus_get_node(dev), "opal-interrupts")) {
		iparent = OF_finddevice("/interrupt-controller@0");
		iparent = OF_xref_from_node(iparent);

		n_irqs = OF_getproplen(ofw_bus_get_node(dev),
                    "opal-interrupts") / sizeof(*irqs);
		irqs = malloc(n_irqs * sizeof(*irqs), M_DEVBUF, M_WAITOK);
		OF_getencprop(ofw_bus_get_node(dev), "opal-interrupts", irqs,
		    n_irqs * sizeof(*irqs));
		for (i = 0; i < n_irqs; i++)
			bus_set_resource(dev, SYS_RES_IRQ, i,
			    ofw_bus_map_intr(dev, iparent, 1, &irqs[i]), 1);
		free(irqs, M_DEVBUF);
	}

	return (BUS_PROBE_SPECIFIC);
}

static int
opaldev_attach(device_t dev)
{
	phandle_t child;
	device_t cdev;
	uint64_t junk;
	int i, rv;
	uint32_t async_count;
	struct ofw_bus_devinfo *dinfo;
	struct resource *irq;

	/* Test for RTC support and register clock if it works */
	rv = opal_call(OPAL_RTC_READ, vtophys(&junk), vtophys(&junk));
	do {
		rv = opal_call(OPAL_RTC_READ, vtophys(&junk), vtophys(&junk));
		if (rv == OPAL_BUSY_EVENT)
			rv = opal_call(OPAL_POLL_EVENTS, 0);
	} while (rv == OPAL_BUSY_EVENT);

	if (rv == OPAL_SUCCESS)
		clock_register(dev, 2000);

	EVENTHANDLER_REGISTER(OPAL_SHUTDOWN, opal_handle_shutdown_message,
	    NULL, EVENTHANDLER_PRI_ANY);
	EVENTHANDLER_REGISTER(shutdown_final, opal_shutdown, NULL,
	    SHUTDOWN_PRI_LAST);

	OF_getencprop(ofw_bus_get_node(dev), "ibm,heartbeat-ms",
	    &opal_heartbeat_ms, sizeof(opal_heartbeat_ms));
	/* Bind to interrupts */
	for (i = 0; (irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &i,
	    RF_ACTIVE)) != NULL; i++)
		bus_setup_intr(dev, irq, INTR_TYPE_TTY | INTR_MPSAFE |
		    INTR_ENTROPY, NULL, opal_intr, (void *)rman_get_start(irq),
		    NULL);

	OF_getencprop(ofw_bus_get_node(dev), "opal-msg-async-num",
	    &async_count, sizeof(async_count));
	opal_init_async_tokens(async_count);

	for (child = OF_child(ofw_bus_get_node(dev)); child != 0;
	    child = OF_peer(child)) {
		dinfo = malloc(sizeof(*dinfo), M_DEVBUF, M_WAITOK | M_ZERO);
		if (ofw_bus_gen_setup_devinfo(dinfo, child) != 0) {
			free(dinfo, M_DEVBUF);
			continue;
		}
		cdev = device_add_child(dev, NULL, DEVICE_UNIT_ANY);
		if (cdev == NULL) {
			device_printf(dev, "<%s>: device_add_child failed\n",
			    dinfo->obd_name);
			ofw_bus_gen_destroy_devinfo(dinfo);
			free(dinfo, M_DEVBUF);
			continue;
		}
		device_set_ivars(cdev, dinfo);
	}

	bus_attach_children(dev);
	return (0);
}

static int
bcd2bin32(int bcd)
{
	int out = 0;

	out += bcd2bin(bcd & 0xff);
	out += 100*bcd2bin((bcd & 0x0000ff00) >> 8);
	out += 10000*bcd2bin((bcd & 0x00ff0000) >> 16);
	out += 1000000*bcd2bin((bcd & 0xffff0000) >> 24);

	return (out);
}

static int
bin2bcd32(int bin)
{
	int out = 0;
	int tmp;

	tmp = bin % 100;
	out += bin2bcd(tmp) * 0x1;
	bin = bin / 100;

	tmp = bin % 100;
	out += bin2bcd(tmp) * 0x100;
	bin = bin / 100;

	tmp = bin % 100;
	out += bin2bcd(tmp) * 0x10000;

	return (out);
}

static int
opal_gettime(device_t dev, struct timespec *ts)
{
	int rv;
	struct clocktime ct;
	uint32_t ymd;
	uint64_t hmsm;

	rv = opal_call(OPAL_RTC_READ, vtophys(&ymd), vtophys(&hmsm));
	while (rv == OPAL_BUSY_EVENT)  {
		opal_call(OPAL_POLL_EVENTS, 0);
		pause("opalrtc", 1);
		rv = opal_call(OPAL_RTC_READ, vtophys(&ymd), vtophys(&hmsm));
	}

	if (rv != OPAL_SUCCESS)
		return (ENXIO);

	hmsm = be64toh(hmsm);
	ymd = be32toh(ymd);

	ct.nsec	= bcd2bin32((hmsm & 0x000000ffffff0000) >> 16) * 1000;
	ct.sec	= bcd2bin((hmsm & 0x0000ff0000000000) >> 40);
	ct.min	= bcd2bin((hmsm & 0x00ff000000000000) >> 48);
	ct.hour	= bcd2bin((hmsm & 0xff00000000000000) >> 56);

	ct.day	= bcd2bin((ymd & 0x000000ff) >> 0);
	ct.mon	= bcd2bin((ymd & 0x0000ff00) >> 8);
	ct.year = bcd2bin32((ymd & 0xffff0000) >> 16);

	return (clock_ct_to_ts(&ct, ts));
}

static int
opal_settime(device_t dev, struct timespec *ts)
{
	int rv;
	struct clocktime ct;
	uint32_t ymd = 0;
	uint64_t hmsm = 0;

	clock_ts_to_ct(ts, &ct);

	ymd |= (uint32_t)bin2bcd(ct.day);
	ymd |= ((uint32_t)bin2bcd(ct.mon) << 8);
	ymd |= ((uint32_t)bin2bcd32(ct.year) << 16);

	hmsm |= ((uint64_t)bin2bcd32(ct.nsec/1000) << 16);
	hmsm |= ((uint64_t)bin2bcd(ct.sec) << 40);
	hmsm |= ((uint64_t)bin2bcd(ct.min) << 48);
	hmsm |= ((uint64_t)bin2bcd(ct.hour) << 56);

	/*
	 * We do NOT swap endian here, because the values are being sent
	 * via registers instead of indirect via memory.
	 */
	do {
		rv = opal_call(OPAL_RTC_WRITE, ymd, hmsm);
		if (rv == OPAL_BUSY_EVENT) {
			rv = opal_call(OPAL_POLL_EVENTS, 0);
			pause("opalrtc", 1);
		}
	} while (rv == OPAL_BUSY_EVENT);

	if (rv != OPAL_SUCCESS)
		return (ENXIO);

	return (0);
}

static const struct ofw_bus_devinfo *
opaldev_get_devinfo(device_t dev, device_t child)
{
	return (device_get_ivars(child));
}

static void
opal_shutdown(void *arg, int howto)
{

	if ((howto & RB_POWEROFF) != 0)
		opal_call(OPAL_CEC_POWER_DOWN, 0 /* Normal power off */);
	else if ((howto & RB_HALT) == 0)
		opal_call(OPAL_CEC_REBOOT);
	else
		return;

	opal_call(OPAL_RETURN_CPU);
}

static void
opal_handle_shutdown_message(void *unused, struct opal_msg *msg)
{
	int howto;

	switch (be64toh(msg->params[0])) {
	case OPAL_SOFT_OFF:
		howto = RB_POWEROFF;
		break;
	case OPAL_SOFT_REBOOT:
		howto = RB_REROOT;
		break;
	}
	shutdown_nice(howto);
}

static void
opal_handle_messages(void)
{
	static struct opal_msg msg;
	uint64_t rv;
	uint32_t type;

	rv = opal_call(OPAL_GET_MSG, vtophys(&msg), sizeof(msg));

	switch (rv) {
	case OPAL_SUCCESS:
		break;
	case OPAL_RESOURCE:
		/* no available messages - return */
		return;
	case OPAL_PARAMETER:
		printf("%s error: invalid buffer. Please file a bug report.\n", __func__);
		return;
	case OPAL_PARTIAL:
		printf("%s error: buffer is too small and messages was discarded. Please file a bug report.\n", __func__);
		return;
	default:
		printf("%s opal_call returned unknown result <%lu>\n", __func__, rv);
		return;
	}

	type = be32toh(msg.msg_type);
	switch (type) {
	case OPAL_MSG_ASYNC_COMP:
		EVENTHANDLER_DIRECT_INVOKE(OPAL_ASYNC_COMP, &msg);
		break;
	case OPAL_MSG_EPOW:
		EVENTHANDLER_DIRECT_INVOKE(OPAL_EPOW, &msg);
		break;
	case OPAL_MSG_SHUTDOWN:
		EVENTHANDLER_DIRECT_INVOKE(OPAL_SHUTDOWN, &msg);
		break;
	case OPAL_MSG_HMI_EVT:
		EVENTHANDLER_DIRECT_INVOKE(OPAL_HMI_EVT, &msg);
		break;
	case OPAL_MSG_DPO:
		EVENTHANDLER_DIRECT_INVOKE(OPAL_DPO, &msg);
		break;
	case OPAL_MSG_OCC:
		EVENTHANDLER_DIRECT_INVOKE(OPAL_OCC, &msg);
		break;
	default:
		printf("%s Unknown OPAL message type %d\n", __func__, type);
	}
}

static void
opal_intr(void *xintr)
{
	uint64_t events = 0;

	opal_call(OPAL_HANDLE_INTERRUPT, (uint32_t)(uint64_t)xintr,
	    vtophys(&events));
	/* Wake up the heartbeat, if it's been setup. */
	if (be64toh(events) != 0 && opal_hb_proc != NULL)
		wakeup(opal_hb_proc);

}
