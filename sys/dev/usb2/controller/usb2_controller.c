/* $FreeBSD$ */
/*-
 * Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
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

#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_defs.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_standard.h>

#define	USB_DEBUG_VAR usb2_ctrl_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_dynamic.h>
#include <dev/usb2/core/usb2_device.h>
#include <dev/usb2/core/usb2_hub.h>

#include <dev/usb2/controller/usb2_controller.h>
#include <dev/usb2/controller/usb2_bus.h>

/* function prototypes */

static device_probe_t usb2_probe;
static device_attach_t usb2_attach;
static device_detach_t usb2_detach;

static void usb2_attach_sub(device_t dev, struct usb2_bus *bus);
static void usb2_post_init(void *arg);
static void usb2_bus_mem_flush_all_cb(struct usb2_bus *bus, struct usb2_page_cache *pc, struct usb2_page *pg, uint32_t size, uint32_t align);
static void usb2_bus_mem_alloc_all_cb(struct usb2_bus *bus, struct usb2_page_cache *pc, struct usb2_page *pg, uint32_t size, uint32_t align);
static void usb2_bus_mem_free_all_cb(struct usb2_bus *bus, struct usb2_page_cache *pc, struct usb2_page *pg, uint32_t size, uint32_t align);

/* static variables */

#if USB_DEBUG
static int usb2_ctrl_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, ctrl, CTLFLAG_RW, 0, "USB controller");
SYSCTL_INT(_hw_usb2_ctrl, OID_AUTO, debug, CTLFLAG_RW, &usb2_ctrl_debug, 0,
    "Debug level");
#endif

static uint8_t usb2_post_init_called = 0;

static devclass_t usb2_devclass;

static device_method_t usb2_methods[] = {
	DEVMETHOD(device_probe, usb2_probe),
	DEVMETHOD(device_attach, usb2_attach),
	DEVMETHOD(device_detach, usb2_detach),
	DEVMETHOD(device_suspend, bus_generic_suspend),
	DEVMETHOD(device_resume, bus_generic_resume),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),
	{0, 0}
};

static driver_t usb2_driver = {
	.name = "usbus",
	.methods = usb2_methods,
	.size = 0,
};

DRIVER_MODULE(usbus, ohci, usb2_driver, usb2_devclass, 0, 0);
DRIVER_MODULE(usbus, uhci, usb2_driver, usb2_devclass, 0, 0);
DRIVER_MODULE(usbus, ehci, usb2_driver, usb2_devclass, 0, 0);
DRIVER_MODULE(usbus, at91_udp, usb2_driver, usb2_devclass, 0, 0);
DRIVER_MODULE(usbus, uss820, usb2_driver, usb2_devclass, 0, 0);

MODULE_DEPEND(usb2_controller, usb2_core, 1, 1, 1);
MODULE_VERSION(usb2_controller, 1);

/*------------------------------------------------------------------------*
 *	usb2_probe
 *
 * This function is called from "{ehci,ohci,uhci}_pci_attach()".
 *------------------------------------------------------------------------*/
static int
usb2_probe(device_t dev)
{
	DPRINTF("\n");
	return (0);
}

/*------------------------------------------------------------------------*
 *	usb2_attach
 *------------------------------------------------------------------------*/
static int
usb2_attach(device_t dev)
{
	struct usb2_bus *bus = device_get_ivars(dev);

	DPRINTF("\n");

	if (bus == NULL) {
		DPRINTFN(0, "USB device has no ivars\n");
		return (ENXIO);
	}
	if (usb2_post_init_called) {
		mtx_lock(&Giant);
		usb2_attach_sub(dev, bus);
		mtx_unlock(&Giant);
		usb2_needs_explore(bus, 1);
	}
	return (0);			/* return success */
}

/*------------------------------------------------------------------------*
 *	usb2_detach
 *------------------------------------------------------------------------*/
static int
usb2_detach(device_t dev)
{
	struct usb2_bus *bus = device_get_softc(dev);

	DPRINTF("\n");

	if (bus == NULL) {
		/* was never setup properly */
		return (0);
	}
	/* Let the USB explore process detach all devices. */

	mtx_lock(&bus->mtx);
	if (usb2_proc_msignal(&bus->explore_proc,
	    &bus->detach_msg[0], &bus->detach_msg[1])) {
		/* ignore */
	}
	/* Wait for detach to complete */

	usb2_proc_mwait(&bus->explore_proc,
	    &bus->detach_msg[0], &bus->detach_msg[1]);

	mtx_unlock(&bus->mtx);

	/* Get rid of USB explore process */

	usb2_proc_unsetup(&bus->explore_proc);

	return (0);
}

/*------------------------------------------------------------------------*
 *	usb2_bus_explore
 *
 * This function is used to explore the device tree from the root.
 *------------------------------------------------------------------------*/
static void
usb2_bus_explore(struct usb2_proc_msg *pm)
{
	struct usb2_bus *bus;
	struct usb2_device *udev;

	bus = ((struct usb2_bus_msg *)pm)->bus;
	udev = bus->devices[USB_ROOT_HUB_ADDR];

	if (udev && udev->hub) {

		if (bus->do_probe) {
			bus->do_probe = 0;
			bus->driver_added_refcount++;
		}
		if (bus->driver_added_refcount == 0) {
			/* avoid zero, hence that is memory default */
			bus->driver_added_refcount = 1;
		}
		mtx_unlock(&bus->mtx);

		mtx_lock(&Giant);

		/*
		 * Explore the Root USB HUB. This call can sleep,
		 * exiting Giant, which is actually Giant.
		 */
		(udev->hub->explore) (udev);

		mtx_unlock(&Giant);

		mtx_lock(&bus->mtx);
	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_bus_detach
 *
 * This function is used to detach the device tree from the root.
 *------------------------------------------------------------------------*/
static void
usb2_bus_detach(struct usb2_proc_msg *pm)
{
	struct usb2_bus *bus;
	struct usb2_device *udev;
	device_t dev;

	bus = ((struct usb2_bus_msg *)pm)->bus;
	udev = bus->devices[USB_ROOT_HUB_ADDR];
	dev = bus->bdev;
	/* clear the softc */
	device_set_softc(dev, NULL);
	mtx_unlock(&bus->mtx);

	mtx_lock(&Giant);

	/* detach children first */
	bus_generic_detach(dev);

	/*
	 * Free USB Root device, but not any sub-devices, hence they
	 * are freed by the caller of this function:
	 */
	usb2_detach_device(udev, USB_IFACE_INDEX_ANY, 0);
	usb2_free_device(udev);

	mtx_unlock(&Giant);
	mtx_lock(&bus->mtx);
	/* clear bdev variable last */
	bus->bdev = NULL;
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_attach_sub
 *
 * This function is the real USB bus attach code. It is factored out,
 * hence it can be called at two different places in time. During
 * bootup this function is called from "usb2_post_init". During
 * hot-plug it is called directly from the "usb2_attach()" method.
 *------------------------------------------------------------------------*/
static void
usb2_attach_sub(device_t dev, struct usb2_bus *bus)
{
	struct usb2_device *child;
	usb2_error_t err;
	uint8_t speed;

	DPRINTF("\n");

	mtx_assert(&Giant, MA_OWNED);

	switch (bus->usbrev) {
	case USB_REV_1_0:
		speed = USB_SPEED_FULL;
		device_printf(bus->bdev, "12Mbps Full Speed USB v1.0\n");
		break;

	case USB_REV_1_1:
		speed = USB_SPEED_FULL;
		device_printf(bus->bdev, "12Mbps Full Speed USB v1.1\n");
		break;

	case USB_REV_2_0:
		speed = USB_SPEED_HIGH;
		device_printf(bus->bdev, "480Mbps High Speed USB v2.0\n");
		break;

	case USB_REV_2_5:
		speed = USB_SPEED_VARIABLE;
		device_printf(bus->bdev, "480Mbps Wireless USB v2.5\n");
		break;

	default:
		device_printf(bus->bdev, "Unsupported USB revision!\n");
		return;
	}

	/* Allocate the Root USB device */

	child = usb2_alloc_device(bus->bdev, bus, NULL, 0, 0, 1,
	    speed, USB_MODE_HOST);
	if (child) {
		err = usb2_probe_and_attach(child,
		    USB_IFACE_INDEX_ANY);
		if (!err) {
			if (!bus->devices[USB_ROOT_HUB_ADDR]->hub) {
				err = USB_ERR_NO_ROOT_HUB;
			}
		}
	} else {
		err = USB_ERR_NOMEM;
	}

	if (err) {
		device_printf(bus->bdev, "Root HUB problem, error=%s\n",
		    usb2_errstr(err));
	}
	/* Initialise USB process messages */
	bus->explore_msg[0].hdr.pm_callback = &usb2_bus_explore;
	bus->explore_msg[0].bus = bus;
	bus->explore_msg[1].hdr.pm_callback = &usb2_bus_explore;
	bus->explore_msg[1].bus = bus;

	bus->detach_msg[0].hdr.pm_callback = &usb2_bus_detach;
	bus->detach_msg[0].bus = bus;
	bus->detach_msg[1].hdr.pm_callback = &usb2_bus_detach;
	bus->detach_msg[1].bus = bus;

	/* Create a new USB process */
	if (usb2_proc_setup(&bus->explore_proc,
	    &bus->mtx, USB_PRI_MED)) {
		printf("WARNING: Creation of USB explore process failed.\n");
	}
	/* set softc - we are ready */
	device_set_softc(dev, bus);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_post_init
 *
 * This function is called to attach all USB busses that were found
 * during bootup.
 *------------------------------------------------------------------------*/
static void
usb2_post_init(void *arg)
{
	struct usb2_bus *bus;
	devclass_t dc;
	device_t dev;
	int max;
	int n;

	mtx_lock(&Giant);

	usb2_devclass_ptr = devclass_find("usbus");

	dc = usb2_devclass_ptr;
	if (dc) {
		max = devclass_get_maxunit(dc) + 1;
		for (n = 0; n != max; n++) {
			dev = devclass_get_device(dc, n);
			if (dev && device_is_attached(dev)) {
				bus = device_get_ivars(dev);
				if (bus) {
					mtx_lock(&Giant);
					usb2_attach_sub(dev, bus);
					mtx_unlock(&Giant);
				}
			}
		}
	} else {
		DPRINTFN(0, "no devclass\n");
	}
	usb2_post_init_called = 1;

	/* explore all USB busses in parallell */

	usb2_needs_explore_all();

	mtx_unlock(&Giant);

	return;
}

SYSINIT(usb2_post_init, SI_SUB_KICK_SCHEDULER, SI_ORDER_ANY, usb2_post_init, NULL);
SYSUNINIT(usb2_bus_unload, SI_SUB_KLD, SI_ORDER_ANY, usb2_bus_unload, NULL);

/*------------------------------------------------------------------------*
 *	usb2_bus_mem_flush_all_cb
 *------------------------------------------------------------------------*/
static void
usb2_bus_mem_flush_all_cb(struct usb2_bus *bus, struct usb2_page_cache *pc,
    struct usb2_page *pg, uint32_t size, uint32_t align)
{
	usb2_pc_cpu_flush(pc);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_bus_mem_flush_all - factored out code
 *------------------------------------------------------------------------*/
void
usb2_bus_mem_flush_all(struct usb2_bus *bus, usb2_bus_mem_cb_t *cb)
{
	if (cb) {
		cb(bus, &usb2_bus_mem_flush_all_cb);
	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_bus_mem_alloc_all_cb
 *------------------------------------------------------------------------*/
static void
usb2_bus_mem_alloc_all_cb(struct usb2_bus *bus, struct usb2_page_cache *pc,
    struct usb2_page *pg, uint32_t size, uint32_t align)
{
	/* need to initialize the page cache */
	pc->tag_parent = bus->dma_parent_tag;

	if (usb2_pc_alloc_mem(pc, pg, size, align)) {
		bus->alloc_failed = 1;
	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_bus_mem_alloc_all - factored out code
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
uint8_t
usb2_bus_mem_alloc_all(struct usb2_bus *bus, bus_dma_tag_t dmat,
    usb2_bus_mem_cb_t *cb)
{
	bus->alloc_failed = 0;

	bus->devices_max = USB_MAX_DEVICES;

	mtx_init(&bus->mtx, "USB lock",
	    NULL, MTX_DEF | MTX_RECURSE);

	TAILQ_INIT(&bus->intr_q.head);

	usb2_dma_tag_setup(bus->dma_parent_tag, bus->dma_tags,
	    dmat, &bus->mtx, NULL, NULL, 32, USB_BUS_DMA_TAG_MAX);

	if (cb) {
		cb(bus, &usb2_bus_mem_alloc_all_cb);
	}
	if (bus->alloc_failed) {
		usb2_bus_mem_free_all(bus, cb);
	}
	return (bus->alloc_failed);
}

/*------------------------------------------------------------------------*
 *	usb2_bus_mem_free_all_cb
 *------------------------------------------------------------------------*/
static void
usb2_bus_mem_free_all_cb(struct usb2_bus *bus, struct usb2_page_cache *pc,
    struct usb2_page *pg, uint32_t size, uint32_t align)
{
	usb2_pc_free_mem(pc);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_bus_mem_free_all - factored out code
 *------------------------------------------------------------------------*/
void
usb2_bus_mem_free_all(struct usb2_bus *bus, usb2_bus_mem_cb_t *cb)
{
	if (cb) {
		cb(bus, &usb2_bus_mem_free_all_cb);
	}
	usb2_dma_tag_unsetup(bus->dma_parent_tag);

	mtx_destroy(&bus->mtx);

	return;
}
