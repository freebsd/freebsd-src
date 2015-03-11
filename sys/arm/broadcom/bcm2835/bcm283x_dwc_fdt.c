/*
 * Copyright 2015 Andrew Turner.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/condvar.h>
#include <sys/module.h>

#include <dev/ofw/ofw_bus_subr.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>

#include <dev/usb/controller/dwc_otg.h>
#include <dev/usb/controller/dwc_otg_fdt.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pmap.h>

#include <arm/broadcom/bcm2835/bcm2835_mbox.h>
#include <arm/broadcom/bcm2835/bcm2835_mbox_prop.h>
#include <arm/broadcom/bcm2835/bcm2835_vcbus.h>

#include "mbox_if.h"

static device_probe_t bcm283x_dwc_otg_probe;
static device_attach_t bcm283x_dwc_otg_attach;

static int
bcm283x_dwc_otg_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "broadcom,bcm2835-usb"))
		return (ENXIO);

	device_set_desc(dev, "DWC OTG 2.0 integrated USB controller (bcm283x)");

	return (BUS_PROBE_VENDOR);
}

static void
bcm283x_dwc_otg_cb(void *arg, bus_dma_segment_t *segs, int nseg, int err)
{
	bus_addr_t *addr;

	if (err)
		return;
	addr = (bus_addr_t *)arg;
	*addr = PHYS_TO_VCBUS(segs[0].ds_addr);
}

static int
bcm283x_dwc_otg_attach(device_t dev)
{
	struct msg_set_power_state *msg;
	bus_dma_tag_t msg_tag;
	bus_dmamap_t msg_map;
	bus_addr_t msg_phys;
	void *msg_buf;
	uint32_t reg;
	device_t mbox;
	int err;

	/* get mbox device */
	mbox = devclass_get_device(devclass_find("mbox"), 0);
	if (mbox == NULL) {
		device_printf(dev, "can't find mbox\n");
		return (ENXIO);
	}

	err = bus_dma_tag_create(bus_get_dma_tag(dev), 16, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    sizeof(struct msg_set_power_state), 1,
	    sizeof(struct msg_set_power_state), 0,
	    NULL, NULL, &msg_tag);
	if (err != 0) {
		device_printf(dev, "can't create DMA tag\n");
		return (ENXIO);
	}

	err = bus_dmamem_alloc(msg_tag, (void **)&msg_buf, 0, &msg_map);
	if (err != 0) {
		bus_dma_tag_destroy(msg_tag);
		device_printf(dev, "can't allocate dmamem\n");
		return (ENXIO);
	}

	err = bus_dmamap_load(msg_tag, msg_map, msg_buf,
	    sizeof(struct msg_set_power_state), bcm283x_dwc_otg_cb,
	    &msg_phys, 0);
	if (err != 0) {
		bus_dmamem_free(msg_tag, msg_buf, msg_map);
		bus_dma_tag_destroy(msg_tag);
		device_printf(dev, "can't load DMA map\n");
		return (ENXIO);
	}

	msg = msg_buf;

	memset(msg, 0, sizeof(*msg));
	msg->hdr.buf_size = sizeof(*msg);
	msg->hdr.code = BCM2835_MBOX_CODE_REQ;
	msg->tag_hdr.tag = BCM2835_MBOX_TAG_SET_POWER_STATE;
	msg->tag_hdr.val_buf_size = sizeof(msg->body);
	msg->tag_hdr.val_len = sizeof(msg->body.req);
	msg->body.req.device_id = BCM2835_MBOX_POWER_ID_USB_HCD;
	msg->body.req.state = BCM2835_MBOX_POWER_ON | BCM2835_MBOX_POWER_WAIT;
	msg->end_tag = 0;

	bus_dmamap_sync(msg_tag, msg_map,
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	MBOX_WRITE(mbox, BCM2835_MBOX_CHAN_PROP, (uint32_t)msg_phys);
	MBOX_READ(mbox, BCM2835_MBOX_CHAN_PROP, &reg);

	bus_dmamap_unload(msg_tag, msg_map);
	bus_dmamem_free(msg_tag, msg_buf, msg_map);
	bus_dma_tag_destroy(msg_tag);

	return (dwc_otg_attach(dev));
}

static device_method_t bcm283x_dwc_otg_methods[] = {
	/* bus interface */
	DEVMETHOD(device_probe, bcm283x_dwc_otg_probe),
	DEVMETHOD(device_attach, bcm283x_dwc_otg_attach),

	DEVMETHOD_END
};

static devclass_t bcm283x_dwc_otg_devclass;

DEFINE_CLASS_1(bcm283x_dwcotg, bcm283x_dwc_otg_driver, bcm283x_dwc_otg_methods,
    sizeof(struct dwc_otg_fdt_softc), dwc_otg_driver);
DRIVER_MODULE(bcm283x_dwcotg, simplebus, bcm283x_dwc_otg_driver,
    bcm283x_dwc_otg_devclass, 0, 0);
MODULE_DEPEND(bcm283x_dwcotg, usb, 1, 1, 1);
