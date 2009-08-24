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

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/linker_set.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/unistd.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/priv.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#define	USB_DEBUG_VAR usb_ctrl_debug

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_dynamic.h>
#include <dev/usb/usb_device.h>
#include <dev/usb/usb_hub.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>

/* function prototypes  */

static device_probe_t usb_probe;
static device_attach_t usb_attach;
static device_detach_t usb_detach;

static void	usb_attach_sub(device_t, struct usb_bus *);

/* static variables */

#ifdef USB_DEBUG
static int usb_ctrl_debug = 0;

SYSCTL_NODE(_hw_usb, OID_AUTO, ctrl, CTLFLAG_RW, 0, "USB controller");
SYSCTL_INT(_hw_usb_ctrl, OID_AUTO, debug, CTLFLAG_RW, &usb_ctrl_debug, 0,
    "Debug level");
#endif

static int usb_no_boot_wait = 0;
TUNABLE_INT("hw.usb.no_boot_wait", &usb_no_boot_wait);
SYSCTL_INT(_hw_usb, OID_AUTO, no_boot_wait, CTLFLAG_RDTUN, &usb_no_boot_wait, 0,
    "No device enumerate waiting at boot.");

static devclass_t usb_devclass;

static device_method_t usb_methods[] = {
	DEVMETHOD(device_probe, usb_probe),
	DEVMETHOD(device_attach, usb_attach),
	DEVMETHOD(device_detach, usb_detach),
	DEVMETHOD(device_suspend, bus_generic_suspend),
	DEVMETHOD(device_resume, bus_generic_resume),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),
	{0, 0}
};

static driver_t usb_driver = {
	.name = "usbus",
	.methods = usb_methods,
	.size = 0,
};

DRIVER_MODULE(usbus, ohci, usb_driver, usb_devclass, 0, 0);
DRIVER_MODULE(usbus, uhci, usb_driver, usb_devclass, 0, 0);
DRIVER_MODULE(usbus, ehci, usb_driver, usb_devclass, 0, 0);
DRIVER_MODULE(usbus, at91_udp, usb_driver, usb_devclass, 0, 0);
DRIVER_MODULE(usbus, uss820, usb_driver, usb_devclass, 0, 0);

/*------------------------------------------------------------------------*
 *	usb_probe
 *
 * This function is called from "{ehci,ohci,uhci}_pci_attach()".
 *------------------------------------------------------------------------*/
static int
usb_probe(device_t dev)
{
	DPRINTF("\n");
	return (0);
}

/*------------------------------------------------------------------------*
 *	usb_attach
 *------------------------------------------------------------------------*/
static int
usb_attach(device_t dev)
{
	struct usb_bus *bus = device_get_ivars(dev);

	DPRINTF("\n");

	if (bus == NULL) {
		DPRINTFN(0, "USB device has no ivars\n");
		return (ENXIO);
	}

	if (usb_no_boot_wait == 0) {
		/* delay vfs_mountroot until the bus is explored */
		bus->bus_roothold = root_mount_hold(device_get_nameunit(dev));
	}

	usb_attach_sub(dev, bus);

	return (0);			/* return success */
}

/*------------------------------------------------------------------------*
 *	usb_detach
 *------------------------------------------------------------------------*/
static int
usb_detach(device_t dev)
{
	struct usb_bus *bus = device_get_softc(dev);

	DPRINTF("\n");

	if (bus == NULL) {
		/* was never setup properly */
		return (0);
	}
	/* Stop power watchdog */
	usb_callout_drain(&bus->power_wdog);

	/* Let the USB explore process detach all devices. */
	if (bus->bus_roothold != NULL) {
		root_mount_rel(bus->bus_roothold);
		bus->bus_roothold = NULL;
	}

	USB_BUS_LOCK(bus);
	if (usb_proc_msignal(&bus->explore_proc,
	    &bus->detach_msg[0], &bus->detach_msg[1])) {
		/* ignore */
	}
	/* Wait for detach to complete */

	usb_proc_mwait(&bus->explore_proc,
	    &bus->detach_msg[0], &bus->detach_msg[1]);

	USB_BUS_UNLOCK(bus);

	/* Get rid of USB callback processes */

	usb_proc_free(&bus->giant_callback_proc);
	usb_proc_free(&bus->non_giant_callback_proc);

	/* Get rid of USB explore process */

	usb_proc_free(&bus->explore_proc);

	/* Get rid of control transfer process */

	usb_proc_free(&bus->control_xfer_proc);

	return (0);
}

/*------------------------------------------------------------------------*
 *	usb_bus_explore
 *
 * This function is used to explore the device tree from the root.
 *------------------------------------------------------------------------*/
static void
usb_bus_explore(struct usb_proc_msg *pm)
{
	struct usb_bus *bus;
	struct usb_device *udev;

	bus = ((struct usb_bus_msg *)pm)->bus;
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

		/*
		 * The following three lines of code are only here to
		 * recover from DDB:
		 */
		usb_proc_rewakeup(&bus->control_xfer_proc);
		usb_proc_rewakeup(&bus->giant_callback_proc);
		usb_proc_rewakeup(&bus->non_giant_callback_proc);

		USB_BUS_UNLOCK(bus);

		/*
		 * First update the USB power state!
		 */
		usb_bus_powerd(bus);

		 /* Explore the Root USB HUB. */
		(udev->hub->explore) (udev);
		USB_BUS_LOCK(bus);
	}
	if (bus->bus_roothold != NULL) {
		root_mount_rel(bus->bus_roothold);
		bus->bus_roothold = NULL;
	}
}

/*------------------------------------------------------------------------*
 *	usb_bus_detach
 *
 * This function is used to detach the device tree from the root.
 *------------------------------------------------------------------------*/
static void
usb_bus_detach(struct usb_proc_msg *pm)
{
	struct usb_bus *bus;
	struct usb_device *udev;
	device_t dev;

	bus = ((struct usb_bus_msg *)pm)->bus;
	udev = bus->devices[USB_ROOT_HUB_ADDR];
	dev = bus->bdev;
	/* clear the softc */
	device_set_softc(dev, NULL);
	USB_BUS_UNLOCK(bus);

	/* detach children first */
	mtx_lock(&Giant);
	bus_generic_detach(dev);
	mtx_unlock(&Giant);

	/*
	 * Free USB Root device, but not any sub-devices, hence they
	 * are freed by the caller of this function:
	 */
	usb_free_device(udev,
	    USB_UNCFG_FLAG_FREE_EP0);

	USB_BUS_LOCK(bus);
	/* clear bdev variable last */
	bus->bdev = NULL;
}

static void
usb_power_wdog(void *arg)
{
	struct usb_bus *bus = arg;

	USB_BUS_LOCK_ASSERT(bus, MA_OWNED);

	usb_callout_reset(&bus->power_wdog,
	    4 * hz, usb_power_wdog, arg);

	/*
	 * The following line of code is only here to recover from
	 * DDB:
	 */
	usb_proc_rewakeup(&bus->explore_proc);	/* recover from DDB */

	USB_BUS_UNLOCK(bus);

	usb_bus_power_update(bus);

	USB_BUS_LOCK(bus);
}

/*------------------------------------------------------------------------*
 *	usb_bus_attach
 *
 * This function attaches USB in context of the explore thread.
 *------------------------------------------------------------------------*/
static void
usb_bus_attach(struct usb_proc_msg *pm)
{
	struct usb_bus *bus;
	struct usb_device *child;
	device_t dev;
	usb_error_t err;
	enum usb_dev_speed speed;

	bus = ((struct usb_bus_msg *)pm)->bus;
	dev = bus->bdev;

	DPRINTF("\n");

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

	USB_BUS_UNLOCK(bus);

	/* default power_mask value */
	bus->hw_power_state =
	  USB_HW_POWER_CONTROL |
	  USB_HW_POWER_BULK |
	  USB_HW_POWER_INTERRUPT |
	  USB_HW_POWER_ISOC |
	  USB_HW_POWER_NON_ROOT_HUB;

	/* make sure power is set at least once */

	if (bus->methods->set_hw_power != NULL) {
		(bus->methods->set_hw_power) (bus);
	}

	/* Allocate the Root USB device */

	child = usb_alloc_device(bus->bdev, bus, NULL, 0, 0, 1,
	    speed, USB_MODE_HOST);
	if (child) {
		err = usb_probe_and_attach(child,
		    USB_IFACE_INDEX_ANY);
		if (!err) {
			if ((bus->devices[USB_ROOT_HUB_ADDR] == NULL) ||
			    (bus->devices[USB_ROOT_HUB_ADDR]->hub == NULL)) {
				err = USB_ERR_NO_ROOT_HUB;
			}
		}
	} else {
		err = USB_ERR_NOMEM;
	}

	USB_BUS_LOCK(bus);

	if (err) {
		device_printf(bus->bdev, "Root HUB problem, error=%s\n",
		    usbd_errstr(err));
	}

	/* set softc - we are ready */
	device_set_softc(dev, bus);

	/* start watchdog */
	usb_power_wdog(bus);
}

/*------------------------------------------------------------------------*
 *	usb_attach_sub
 *
 * This function creates a thread which runs the USB attach code.
 *------------------------------------------------------------------------*/
static void
usb_attach_sub(device_t dev, struct usb_bus *bus)
{
	const char *pname = device_get_nameunit(dev);

	mtx_lock(&Giant);
	if (usb_devclass_ptr == NULL)
		usb_devclass_ptr = devclass_find("usbus");
	mtx_unlock(&Giant);

	/* Initialise USB process messages */
	bus->explore_msg[0].hdr.pm_callback = &usb_bus_explore;
	bus->explore_msg[0].bus = bus;
	bus->explore_msg[1].hdr.pm_callback = &usb_bus_explore;
	bus->explore_msg[1].bus = bus;

	bus->detach_msg[0].hdr.pm_callback = &usb_bus_detach;
	bus->detach_msg[0].bus = bus;
	bus->detach_msg[1].hdr.pm_callback = &usb_bus_detach;
	bus->detach_msg[1].bus = bus;

	bus->attach_msg[0].hdr.pm_callback = &usb_bus_attach;
	bus->attach_msg[0].bus = bus;
	bus->attach_msg[1].hdr.pm_callback = &usb_bus_attach;
	bus->attach_msg[1].bus = bus;

	/* Create USB explore and callback processes */

	if (usb_proc_create(&bus->giant_callback_proc,
	    &bus->bus_mtx, pname, USB_PRI_MED)) {
		printf("WARNING: Creation of USB Giant "
		    "callback process failed.\n");
	} else if (usb_proc_create(&bus->non_giant_callback_proc,
	    &bus->bus_mtx, pname, USB_PRI_HIGH)) {
		printf("WARNING: Creation of USB non-Giant "
		    "callback process failed.\n");
	} else if (usb_proc_create(&bus->explore_proc,
	    &bus->bus_mtx, pname, USB_PRI_MED)) {
		printf("WARNING: Creation of USB explore "
		    "process failed.\n");
	} else if (usb_proc_create(&bus->control_xfer_proc,
	    &bus->bus_mtx, pname, USB_PRI_MED)) {
		printf("WARNING: Creation of USB control transfer "
		    "process failed.\n");
	} else {
		/* Get final attach going */
		USB_BUS_LOCK(bus);
		if (usb_proc_msignal(&bus->explore_proc,
		    &bus->attach_msg[0], &bus->attach_msg[1])) {
			/* ignore */
		}
		USB_BUS_UNLOCK(bus);

		/* Do initial explore */
		usb_needs_explore(bus, 1);
	}
}

SYSUNINIT(usb_bus_unload, SI_SUB_KLD, SI_ORDER_ANY, usb_bus_unload, NULL);

/*------------------------------------------------------------------------*
 *	usb_bus_mem_flush_all_cb
 *------------------------------------------------------------------------*/
#if USB_HAVE_BUSDMA
static void
usb_bus_mem_flush_all_cb(struct usb_bus *bus, struct usb_page_cache *pc,
    struct usb_page *pg, usb_size_t size, usb_size_t align)
{
	usb_pc_cpu_flush(pc);
}
#endif

/*------------------------------------------------------------------------*
 *	usb_bus_mem_flush_all - factored out code
 *------------------------------------------------------------------------*/
#if USB_HAVE_BUSDMA
void
usb_bus_mem_flush_all(struct usb_bus *bus, usb_bus_mem_cb_t *cb)
{
	if (cb) {
		cb(bus, &usb_bus_mem_flush_all_cb);
	}
}
#endif

/*------------------------------------------------------------------------*
 *	usb_bus_mem_alloc_all_cb
 *------------------------------------------------------------------------*/
#if USB_HAVE_BUSDMA
static void
usb_bus_mem_alloc_all_cb(struct usb_bus *bus, struct usb_page_cache *pc,
    struct usb_page *pg, usb_size_t size, usb_size_t align)
{
	/* need to initialize the page cache */
	pc->tag_parent = bus->dma_parent_tag;

	if (usb_pc_alloc_mem(pc, pg, size, align)) {
		bus->alloc_failed = 1;
	}
}
#endif

/*------------------------------------------------------------------------*
 *	usb_bus_mem_alloc_all - factored out code
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
uint8_t
usb_bus_mem_alloc_all(struct usb_bus *bus, bus_dma_tag_t dmat,
    usb_bus_mem_cb_t *cb)
{
	bus->alloc_failed = 0;

	mtx_init(&bus->bus_mtx, device_get_nameunit(bus->parent),
	    NULL, MTX_DEF | MTX_RECURSE);

	usb_callout_init_mtx(&bus->power_wdog,
	    &bus->bus_mtx, 0);

	TAILQ_INIT(&bus->intr_q.head);

#if USB_HAVE_BUSDMA
	usb_dma_tag_setup(bus->dma_parent_tag, bus->dma_tags,
	    dmat, &bus->bus_mtx, NULL, 32, USB_BUS_DMA_TAG_MAX);
#endif
	if ((bus->devices_max > USB_MAX_DEVICES) ||
	    (bus->devices_max < USB_MIN_DEVICES) ||
	    (bus->devices == NULL)) {
		DPRINTFN(0, "Devices field has not been "
		    "initialised properly!\n");
		bus->alloc_failed = 1;		/* failure */
	}
#if USB_HAVE_BUSDMA
	if (cb) {
		cb(bus, &usb_bus_mem_alloc_all_cb);
	}
#endif
	if (bus->alloc_failed) {
		usb_bus_mem_free_all(bus, cb);
	}
	return (bus->alloc_failed);
}

/*------------------------------------------------------------------------*
 *	usb_bus_mem_free_all_cb
 *------------------------------------------------------------------------*/
#if USB_HAVE_BUSDMA
static void
usb_bus_mem_free_all_cb(struct usb_bus *bus, struct usb_page_cache *pc,
    struct usb_page *pg, usb_size_t size, usb_size_t align)
{
	usb_pc_free_mem(pc);
}
#endif

/*------------------------------------------------------------------------*
 *	usb_bus_mem_free_all - factored out code
 *------------------------------------------------------------------------*/
void
usb_bus_mem_free_all(struct usb_bus *bus, usb_bus_mem_cb_t *cb)
{
#if USB_HAVE_BUSDMA
	if (cb) {
		cb(bus, &usb_bus_mem_free_all_cb);
	}
	usb_dma_tag_unsetup(bus->dma_parent_tag);
#endif

	mtx_destroy(&bus->bus_mtx);
}
