/*	$NetBSD: uhub.c,v 1.26 1999/09/05 19:32:18 augustss Exp $	*/
/*	$FreeBSD$	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@carlstedt.se) at
 * Carlstedt Research & Technology.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * USB spec: http://www.usb.org/cgi-usb/mailmerge.cgi/home/usb/docs/developers/cgiform.tpl
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/device.h>
#elif defined(__FreeBSD__)
#include <sys/module.h>
#include <sys/bus.h>
#endif
#include <sys/proc.h>

#include "bus_if.h"

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>

#ifdef UHUB_DEBUG
#define DPRINTF(x)	if (usbdebug) logprintf x
#define DPRINTFN(n,x)	if (usbdebug>(n)) logprintf x
extern int	usbdebug;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

struct uhub_softc {
	USBBASEDEVICE		sc_dev;		/* base device */
	usbd_device_handle	sc_hub;		/* USB device */
	usbd_pipe_handle	sc_ipipe;	/* interrupt pipe */
	u_int8_t		sc_status[1];	/* XXX more ports */
	u_char			sc_running;
};

usbd_status uhub_init_port __P((struct usbd_port *));
void uhub_disconnect_port __P((struct usbd_port *up));
usbd_status uhub_explore __P((usbd_device_handle hub));
void uhub_intr __P((usbd_request_handle, usbd_private_handle, usbd_status));

#if defined(__FreeBSD__)
static bus_child_detached_t uhub_child_detached;
#endif

USB_DECLARE_DRIVER_INIT(uhub, DEVMETHOD(bus_child_detached, uhub_child_detached));

/* We need two attachment points:
 * hub to usb and hub to hub
 * Every other driver only connects to hubs
 */

#if defined(__NetBSD__) || defined(__OpenBSD__)
/* Create the driver instance for the hub connected to hub case */
struct cfattach uhub_uhub_ca = {
	sizeof(struct uhub_softc), uhub_match, uhub_attach,
	uhub_detach, uhub_activate
};
#elif defined(__FreeBSD__)
/* Create the driver instance for the hub connected to usb case. */
devclass_t uhubroot_devclass;

static device_method_t uhubroot_methods[] = {
	DEVMETHOD(device_probe, uhub_match),
	DEVMETHOD(device_attach, uhub_attach),

	/* detach is not allowed for a root hub */
	{0,0}
};

static	driver_t uhubroot_driver = {
	"uhub",
	uhubroot_methods,
	sizeof(struct uhub_softc)
};
#endif


USB_MATCH(uhub)
{
	USB_MATCH_START(uhub, uaa);
	usb_device_descriptor_t *dd = usbd_get_device_descriptor(uaa->device);
	
	DPRINTFN(5,("uhub_match, dd=%p\n", dd));
	/* 
	 * The subclass for hubs seems to be 0 for some and 1 for others,
	 * so we just ignore the subclass.
	 */
	if (uaa->iface == 0 && dd->bDeviceClass == UCLASS_HUB)
		return (UMATCH_DEVCLASS_DEVSUBCLASS);
	return (UMATCH_NONE);
}

USB_ATTACH(uhub)
{
	USB_ATTACH_START(uhub, sc, uaa);
	usbd_device_handle dev = uaa->device;
	char devinfo[1024];
	usbd_status r;
	struct usbd_hub *hub;
	usb_device_request_t req;
	usb_hub_descriptor_t hubdesc;
	int p, port, nports, nremov;
	usbd_interface_handle iface;
	usb_endpoint_descriptor_t *ed;
	
	DPRINTFN(1,("uhub_attach\n"));
	sc->sc_hub = dev;
	usbd_devinfo(dev, 1, devinfo);
	USB_ATTACH_SETUP;
	printf("%s: %s\n", USBDEVNAME(sc->sc_dev), devinfo);

	r = usbd_set_config_index(dev, 0, 1);
	if (r != USBD_NORMAL_COMPLETION) {
		DPRINTF(("%s: configuration failed, error=%s\n",
			 USBDEVNAME(sc->sc_dev), usbd_errstr(r)));
		USB_ATTACH_ERROR_RETURN;
	}

	if (dev->depth > USB_HUB_MAX_DEPTH) {
		printf("%s: hub depth (%d) exceeded, hub ignored\n",
		       USBDEVNAME(sc->sc_dev), USB_HUB_MAX_DEPTH);
		USB_ATTACH_ERROR_RETURN;
	}

	/* Get hub descriptor. */
	req.bmRequestType = UT_READ_CLASS_DEVICE;
	req.bRequest = UR_GET_DESCRIPTOR;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, USB_HUB_DESCRIPTOR_SIZE);
	DPRINTFN(1,("usb_init_hub: getting hub descriptor\n"));
	r = usbd_do_request(dev, &req, &hubdesc);
	nports = hubdesc.bNbrPorts;
	if (r == USBD_NORMAL_COMPLETION && nports > 7) {
		USETW(req.wLength, USB_HUB_DESCRIPTOR_SIZE + (nports+1) / 8);
		r = usbd_do_request(dev, &req, &hubdesc);
	}
	if (r != USBD_NORMAL_COMPLETION) {
		DPRINTF(("%s: getting hub descriptor failed, error=%s\n",
			 USBDEVNAME(sc->sc_dev), usbd_errstr(r)));
		USB_ATTACH_ERROR_RETURN;
	}

	for (nremov = 0, port = 1; port <= nports; port++)
		if (!UHD_NOT_REMOV(&hubdesc, port))
			nremov++;
	printf("%s: %d port%s with %d removable, %s powered\n",
	       USBDEVNAME(sc->sc_dev), nports, nports != 1 ? "s" : "",
	       nremov, dev->self_powered ? "self" : "bus");
	

	hub = malloc(sizeof(*hub) + (nports-1) * sizeof(struct usbd_port),
		     M_USBDEV, M_NOWAIT);
	if (hub == 0)
		USB_ATTACH_ERROR_RETURN;
	dev->hub = hub;
	dev->hub->hubsoftc = sc;
	hub->explore = uhub_explore;
	hub->hubdesc = hubdesc;
	
	DPRINTFN(1,("usbhub_init_hub: selfpowered=%d, parent=%p, "
		    "parent->selfpowered=%d\n",
		 dev->self_powered, dev->powersrc->parent,
		 dev->powersrc->parent ? 
		 dev->powersrc->parent->self_powered : 0));

	if (!dev->self_powered && dev->powersrc->parent &&
	    !dev->powersrc->parent->self_powered) {
		printf("%s: bus powered hub connected to bus powered hub, "
		       "ignored\n", USBDEVNAME(sc->sc_dev));
		goto bad;
	}

	/* Set up interrupt pipe. */
	r = usbd_device2interface_handle(dev, 0, &iface);
	if (r != USBD_NORMAL_COMPLETION) {
		printf("%s: no interface handle\n", USBDEVNAME(sc->sc_dev));
		goto bad;
	}
	ed = usbd_interface2endpoint_descriptor(iface, 0);
	if (ed == 0) {
		printf("%s: no endpoint descriptor\n", USBDEVNAME(sc->sc_dev));
		goto bad;
	}
	if ((ed->bmAttributes & UE_XFERTYPE) != UE_INTERRUPT) {
		printf("%s: bad interrupt endpoint\n", USBDEVNAME(sc->sc_dev));
		goto bad;
	}

	r = usbd_open_pipe_intr(iface, ed->bEndpointAddress,USBD_SHORT_XFER_OK,
				&sc->sc_ipipe, sc, sc->sc_status, 
				sizeof(sc->sc_status),
				uhub_intr);
	if (r != USBD_NORMAL_COMPLETION) {
		printf("%s: cannot open interrupt pipe\n", 
		       USBDEVNAME(sc->sc_dev));
		goto bad;
	}

	/* Wait with power off for a while. */
	usbd_delay_ms(dev, USB_POWER_DOWN_TIME);

	for (p = 0; p < nports; p++) {
		struct usbd_port *up = &hub->ports[p];
		up->device = 0;
		up->parent = dev;
		up->portno = p+1;
		r = uhub_init_port(up);
		if (r != USBD_NORMAL_COMPLETION)
			printf("%s: init of port %d failed\n", 
			       USBDEVNAME(sc->sc_dev), up->portno);
	}
	sc->sc_running = 1;

	USB_ATTACH_SUCCESS_RETURN;

bad:
	free(hub, M_USBDEV);
	dev->hub = 0;
	USB_ATTACH_ERROR_RETURN;
}

usbd_status
uhub_init_port(up)
	struct usbd_port *up;
{
	int port = up->portno;
	usbd_device_handle dev = up->parent;
	usbd_status r;
	u_int16_t pstatus;

	r = usbd_get_port_status(dev, port, &up->status);
	if (r != USBD_NORMAL_COMPLETION)
		return (r);
	pstatus = UGETW(up->status.wPortStatus);
	DPRINTF(("usbd_init_port: adding hub port=%d status=0x%04x "
		 "change=0x%04x\n",
		 port, pstatus, UGETW(up->status.wPortChange)));
	if ((pstatus & UPS_PORT_POWER) == 0) {
		/* Port lacks power, turn it on */

		/* First let the device go through a good power cycle, */
		usbd_delay_ms(dev, USB_PORT_POWER_DOWN_TIME);

#if 0
usbd_clear_hub_feature(dev, UHF_C_HUB_OVER_CURRENT);
usbd_clear_port_feature(dev, port, UHF_C_PORT_OVER_CURRENT);
#endif

		/* then turn the power on. */
		r = usbd_set_port_feature(dev, port, UHF_PORT_POWER);
		if (r != USBD_NORMAL_COMPLETION)
			return (r);
		DPRINTF(("usb_init_port: turn on port %d power status=0x%04x "
			 "change=0x%04x\n",
			 port, UGETW(up->status.wPortStatus),
			 UGETW(up->status.wPortChange)));
		/* Wait for stable power. */
		usbd_delay_ms(dev, dev->hub->hubdesc.bPwrOn2PwrGood * 
			           UHD_PWRON_FACTOR);
		/* Get the port status again. */
		r = usbd_get_port_status(dev, port, &up->status);
		if (r != USBD_NORMAL_COMPLETION)
			return (r);
		DPRINTF(("usb_init_port: after power on status=0x%04x "
			 "change=0x%04x\n",
			 UGETW(up->status.wPortStatus),
			 UGETW(up->status.wPortChange)));

#if 0
usbd_clear_hub_feature(dev, UHF_C_HUB_OVER_CURRENT);
usbd_clear_port_feature(dev, port, UHF_C_PORT_OVER_CURRENT);
usbd_get_port_status(dev, port, &up->status);
#endif

		pstatus = UGETW(up->status.wPortStatus);
		if ((pstatus & UPS_PORT_POWER) == 0)
			printf("%s: port %d did not power up\n",
 USBDEVNAME(((struct uhub_softc *)dev->hub->hubsoftc)->sc_dev), port);

	}
	if (dev->self_powered)
		/* Self powered hub, give ports maximum current. */
		up->power = USB_MAX_POWER;
	else
		up->power = USB_MIN_POWER;
	return (USBD_NORMAL_COMPLETION);
}

usbd_status
uhub_explore(dev)
	usbd_device_handle dev;
{
	usb_hub_descriptor_t *hd = &dev->hub->hubdesc;
	struct uhub_softc *sc = dev->hub->hubsoftc;
	struct usbd_port *up;
	usbd_status r;
	int port;
	int change, status;

	DPRINTFN(10, ("uhub_explore dev=%p addr=%d\n", dev, dev->address));

	if (!sc->sc_running)
		return (USBD_NOT_STARTED);

	/* Ignore hubs that are too deep. */
	if (dev->depth > USB_HUB_MAX_DEPTH)
		return (USBD_TOO_DEEP);

	for(port = 1; port <= hd->bNbrPorts; port++) {
		up = &dev->hub->ports[port-1];
		r = usbd_get_port_status(dev, port, &up->status);
		if (r != USBD_NORMAL_COMPLETION) {
			DPRINTF(("uhub_explore: get port status failed, "
				 "error=%s\n",
				 usbd_errstr(r)));
			continue;
		}
		status = UGETW(up->status.wPortStatus);
		change = UGETW(up->status.wPortChange);
		DPRINTFN(5, ("uhub_explore: port %d status 0x%04x 0x%04x\n",
			     port, status, change));
		if (change & UPS_C_PORT_ENABLED) {
			usbd_clear_port_feature(dev, port, UHF_C_PORT_ENABLE);
			if (status & UPS_PORT_ENABLED) {
				printf("%s: illegal enable change, port %d\n",
				       USBDEVNAME(sc->sc_dev), port);
			} else {
				/* Port error condition. */
				if (up->restartcnt++ < USBD_RESTART_MAX) {
					printf("%s: port error, restarting "
					       "port %d\n",
					       USBDEVNAME(sc->sc_dev), port);
					goto disco;
				} else {
					printf("%s: port error, giving up "
					       "port %d\n",
					       USBDEVNAME(sc->sc_dev), port);
				}
			}
		}
		if (!(change & UPS_C_CONNECT_STATUS)) {
			/* No status change, just do recursive explore. */
			if (up->device && up->device->hub)
				up->device->hub->explore(up->device);
			continue;
		}
		DPRINTF(("uhub_explore: status change hub=%d port=%d\n",
			 dev->address, port));
		usbd_clear_port_feature(dev, port, UHF_C_PORT_CONNECTION);
		usbd_clear_port_feature(dev, port, UHF_C_PORT_ENABLE);
		/*
		 * If there is already a device on the port the change status
		 * must mean that is has disconnected.  Looking at the
		 * current connect status is not enough to figure this out
		 * since a new unit may have been connected before we handle
		 * the disconnect.
		 */
	disco:
		if (up->device) {
			/* Disconnected */
			DPRINTF(("uhub_explore: device %d disappeared "
				 "on port %d\n", 
				 up->device->address, port));
			uhub_disconnect_port(up);

			usbd_clear_port_feature(dev, port, 
						UHF_C_PORT_CONNECTION);
		}
		if (!(status & UPS_CURRENT_CONNECT_STATUS))
			continue;

		/* Connected */
		up->restartcnt = 0;

		/* Wait for maximum device power up time. */
		usbd_delay_ms(dev, USB_PORT_POWERUP_DELAY);

		/* Reset port, which implies enabling it. */
		if (usbd_reset_port(dev, port, &up->status) != 
		    USBD_NORMAL_COMPLETION)
			continue;

		/* Get device info and set its address. */
		r = usbd_new_device(USBDEV(sc->sc_dev), dev->bus, 
				    dev->depth + 1, status & UPS_LOW_SPEED, 
				    port, up);
		/* XXX retry a few times? */
		if (r != USBD_NORMAL_COMPLETION) {
			DPRINTFN(-1,("uhub_explore: usb_new_device failed, "
				     "error=%s\n", usbd_errstr(r)));
			/* Avoid addressing problems by disabling. */
			/* usbd_reset_port(dev, port, &up->status); */
/* XXX
 * What should we do.  The device may or may not be at its
 * assigned address.  In any case we'd like to ignore it.
 */
			if (r == USBD_SET_ADDR_FAILED || 1) {/* XXX */
				/* The unit refused to accept a new
				 * address, and since we cannot leave
				 * at 0 we have to disable the port
				 * instead. */
				printf("%s: device problem, disabling "
				       "port %d\n",
				       USBDEVNAME(sc->sc_dev), port);
				usbd_clear_port_feature(dev, port, 
							UHF_PORT_ENABLE);
				/* Make sure we don't try to restart it. */
				up->restartcnt = USBD_RESTART_MAX;
			}
		} else {
			if (up->device->hub)
				up->device->hub->explore(up->device);
		}
	}
	return (USBD_NORMAL_COMPLETION);
}

/*
 * The general mechanism for detaching drivers works as follows: Each
 * driver is responsible for maintaining a reference count on the
 * number of outstanding references to its softc (e.g.  from
 * processing hanging in a read or write).  The detach method of the
 * driver decrements this counter and flags in the softc that the
 * driver is dying and then wakes any sleepers.  It then sleeps on the
 * softc.  Each place that can sleep must maintain the reference
 * count.  When the reference count drops to -1 (0 is the normal value
 * of the reference count) the a wakeup on the softc is performed
 * signaling to the detach waiter that all references are gone.
 */

/*
 * Called from process context when we discover that a port has
 * been disconnected.
 */
void
uhub_disconnect_port(up)
	struct usbd_port *up;
{
	usbd_device_handle dev = up->device;
	int i;

	DPRINTFN(3,("uhub_disconnect: up=%p dev=%p port=%d\n", 
		    up, dev, up->portno));

	if (!dev)	/* not even generic device was attached */
		return;

	if (!dev->cdesc) {
		/* Partially attached device, just drop it. */
		dev->bus->devices[dev->address] = 0;
		up->device = 0;
		return;
	}

	if (dev->subdevs) {
		for (i = 0; dev->subdevs[i]; i++) {
			if (!dev->subdevs[i])	/* skip empty elements */
				continue;

			printf("%s: at %s port %d (addr %d) disconnected\n",
			       USBDEVPTRNAME(dev->subdevs[i]),
			       USBDEVPTRNAME(up->parent->subdevs[0]),
			       up->portno, dev->address);
#if defined(__NetBSD__) || defined(__OpenBSD__)
			config_detach(dev->subdevs[i], DETACH_FORCE);
#elif defined(__FreeBSD__)
			device_delete_child(device_get_parent(dev->subdevs[i]),
						dev->subdevs[i]);
#endif
		}
	}

	dev->bus->devices[dev->address] = 0;
	up->device = 0;
	usb_free_device(dev);
}

#if defined(__FreeBSD__)
/* Called when a device has been detached from it */
static void
uhub_child_detached(self, child)
	device_t self;
	device_t child;
{
	struct uhub_softc *sc = device_get_softc(self);
	usbd_device_handle dev = sc->sc_hub;
	struct usbd_port *up;
	int nports;
	int port;
	int i;

	if (!dev->hub)	
		/* should never happen; children are only created after init */
		panic("hub not fully initialised, but child deleted?");

	for (port = 0; port < dev->hub->hubdesc.bNbrPorts; port++) {
		up = &dev->hub->ports[port];
		if (up->device && up->device->subdevs) {
			for (i = 0; up->device->subdevs[i]; i++) {
				if (up->device->subdevs[i] == child) {
					up->device->subdevs[i] = NULL;
					return;
				}
			}
		}
	}
}
#endif


#if defined(__NetBSD__)
int
uhub_activate(self, act)
	device_ptr_t self;
	enum devact act;
{
	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		break;
	}
	return (0);
}
#endif

/*
 * Called from process context when the hub is gone.
 * Detach all devices on active ports.
 */
USB_DETACH(uhub)
{
	USB_DETACH_START(uhub, sc);
	usbd_device_handle dev = sc->sc_hub;
	struct usbd_port *up;
	int port, nports;

#if defined(__NetBSD__) || defined(__OpenBSD__)
	DPRINTF(("uhub_detach: sc=%port flags=%d\n", sc, flags));
#elif defined(__FreeBSD__)
	DPRINTF(("uhub_detach: sc=%port\n", sc));
#endif

	if (!dev->hub)	/* Must be partially working */
		return (0);

	usbd_abort_pipe(sc->sc_ipipe);
	usbd_close_pipe(sc->sc_ipipe);

	nports = dev->hub->hubdesc.bNbrPorts;
	for(port = 0; port < nports; port++) {
		up = &dev->hub->ports[port];
		if (up->device) {
			DPRINTF(("uhub_detach: device %d disappeared "
				 "on port %d\n", 
				 up->device->address, port));
			uhub_disconnect_port(up);
		}
	}
	
	free(dev->hub, M_USBDEV);
	dev->hub = 0;

	return (0);
}

/*
 * Hub interrupt.
 * This an indication that some port has changed status.
 * Notify the bus event handler thread that we need
 * to be explored again.
 */
void
uhub_intr(reqh, addr, status)
	usbd_request_handle reqh;
	usbd_private_handle addr;
	usbd_status status;
{
	struct uhub_softc *sc = addr;

	DPRINTFN(5,("uhub_intr: sc=%p\n", sc));
	if (status != USBD_NORMAL_COMPLETION)
		usbd_clear_endpoint_stall_async(sc->sc_ipipe);

	usb_needs_explore(sc->sc_hub->bus);
}

#if defined(__FreeBSD__)
DRIVER_MODULE(uhub, usb, uhubroot_driver, uhubroot_devclass, 0, 0);
DRIVER_MODULE(uhub, uhub, uhub_driver, uhub_devclass, usbd_driver_load, 0);
#endif
