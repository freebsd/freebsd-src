/*	$NetBSD: uhub.c,v 1.16 1999/01/10 19:13:15 augustss Exp $	*/
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
#if defined(__NetBSD__)
#include <sys/device.h>
#elif defined(__FreeBSD__)
#include <sys/module.h>
#include <sys/bus.h>
#endif
#include <sys/proc.h>

#include <dev/usb/usb.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	if (usbdebug) logprintf x
#define DPRINTFN(n,x)	if (usbdebug>(n)) logprintf x
extern int	usbdebug;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

struct uhub_softc {
	bdevice			sc_dev;		/* base device */
	usbd_device_handle	sc_hub;		/* USB device */
	usbd_pipe_handle	sc_ipipe;	/* interrupt pipe */
	u_int8_t		sc_status[1];	/* XXX more ports */
	u_char			sc_running;
};

usbd_status uhub_init_port __P((struct usbd_port *));
void uhub_disconnect_port __P((struct usbd_port *up));
usbd_status uhub_explore __P((usbd_device_handle hub));
void uhub_intr __P((usbd_request_handle, usbd_private_handle, usbd_status));
#ifdef __FreeBSD__
#include "usb_if.h"
static void uhub_disconnected __P((device_t));
#endif

/* void uhub_disco __P((void *)); */

USB_DECLARE_DRIVER_INIT(uhub,
			DEVMETHOD(usb_disconnected, uhub_disconnected));

#if defined(__FreeBSD__)
devclass_t uhubroot_devclass;

static device_method_t uhubroot_methods[] = {
        DEVMETHOD(device_probe, uhub_match),
        DEVMETHOD(device_attach, uhub_attach),
	/* detach is not allowed for a root hub */
        {0,0}
};

static driver_t uhubroot_driver = {
        "uhub",
        uhubroot_methods,
        DRIVER_TYPE_MISC,
        sizeof(struct uhub_softc)
};
#endif

#if defined(__NetBSD__)
struct cfattach uhub_uhub_ca = {
	sizeof(struct uhub_softc), uhub_match, uhub_attach
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
		DPRINTF(("%s: configuration failed, %s\n",
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
		DPRINTF(("%s: getting hub descriptor failed, %s\n",
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
		     M_USB, M_NOWAIT);
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
		       "ignored\n",
		       USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	/* Set up interrupt pipe. */
	r = usbd_device2interface_handle(dev, 0, &iface);
	if (r != USBD_NORMAL_COMPLETION) {
		printf("%s: no interface handle\n", USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}
	ed = usbd_interface2endpoint_descriptor(iface, 0);
	if (ed == 0) {
		printf("%s: no endpoint descriptor\n", USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}
	if ((ed->bmAttributes & UE_XFERTYPE) != UE_INTERRUPT) {
		printf("%s: bad interrupt endpoint\n", USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	r = usbd_open_pipe_intr(iface, ed->bEndpointAddress,USBD_SHORT_XFER_OK,
				&sc->sc_ipipe, sc, sc->sc_status, 
				sizeof(sc->sc_status),
				uhub_intr);
	if (r != USBD_NORMAL_COMPLETION) {
		printf("%s: cannot open interrupt pipe\n", 
		       USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
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
}

#if defined(__FreeBSD__)
static void
uhub_disconnected(device_t self)
{
	struct uhub_softc *sc = device_get_softc(self);
	struct usbd_port *up;
	usbd_device_handle dev = sc->sc_hub;
	int nports;
	int p;

	DPRINTF(("%s: disconnected\n", USBDEVNAME(self)));
	nports = dev->hub->hubdesc.bNbrPorts;
	for (p = 0; p < nports; p++) {
		up = &sc->sc_hub->hub->ports[p];
		if (up->device) {
			uhub_disconnect_port(up);
		}
	}

	return;
}

static int
uhub_detach(device_t self)
{
	struct uhub_softc *sc = device_get_softc(self);
	DPRINTF(("%s: disconnected\n", USBDEVNAME(self)));
	free(sc->sc_hub->hub, M_USB);
	return 0;
}
#endif

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

		/* then turn the power on. */
		r = usbd_set_port_feature(dev, port, UHF_PORT_POWER);
		if (r != USBD_NORMAL_COMPLETION)
			return (r);
		r = usbd_get_port_status(dev, port, &up->status);
		if (r != USBD_NORMAL_COMPLETION)
			return (r);
		DPRINTF(("usb_init_port: turn on port %d power status=0x%04x "
			 "change=0x%04x\n",
			 port, UGETW(up->status.wPortStatus),
			 UGETW(up->status.wPortChange)));
		/* Wait for stable power. */
		usbd_delay_ms(dev, dev->hub->hubdesc.bPwrOn2PwrGood * 
			           UHD_PWRON_FACTOR);
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
			DPRINTF(("uhub_explore: get port %d status failed, %s\n",
				 port, usbd_errstr(r)));
			continue;
		}
		status = UGETW(up->status.wPortStatus);
		change = UGETW(up->status.wPortChange);
		DPRINTFN(5, ("uhub_explore: port %d status 0x%04x 0x%04x\n",
			     port, status, change));
		if (change & UPS_C_PORT_ENABLED) {
			usbd_clear_port_feature(dev, port, UHF_C_PORT_ENABLE);
			if (status & UPS_PORT_ENABLED) {
				printf("%s: port %d illegal enable change\n",
				       USBDEVNAME(sc->sc_dev), port);
			} else {
				/* Port error condition. */
				if (up->restartcnt++ < USBD_RESTART_MAX) {
					printf("%s: port %d error, restarting\n",
					       USBDEVNAME(sc->sc_dev), port);
					goto disco;
				} else {
					printf("%s: port %d error, giving up\n",
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
		r = usbd_new_device(&sc->sc_dev, dev->bus, 
				    dev->depth + 1, status & UPS_LOW_SPEED, 
				    port, up);
		/* XXX retry a few times? */
		if (r != USBD_NORMAL_COMPLETION) {
			DPRINTFN(-1,("uhub_explore: usb_new_device failed, %s\n",
				     usbd_errstr(r)));
			/* Avoid addressing problems by disabling. */
			/* usbd_reset_port(dev, port, &up->status); */
/* XXX
 * What should we do.  The device may or may not be at its
 * assigned address.  In any case we'd like to ignore it.
 * Maybe the port should be disabled until the device is
 * disconnected.
 */
			if (r == USBD_SET_ADDR_FAILED || 1) {/* XXX */
				/* The unit refused to accept a new
				 * address, and since we cannot leave
				 * it at 0 we have to disable the port
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

void
uhub_disconnect_port(up)
	struct usbd_port *up;
{
	usbd_device_handle dev = up->device;
	usbd_pipe_handle p, n;
	int i;
	struct softc {		/* all softc begin like this */
		bdevice sc_dev;
	};
	struct softc *sc;
	struct softc *scp;

	if (!dev) 		/* no device driver attached at port */
		return;

	sc = (struct softc *)dev->softc;
	scp = (struct softc *)up->parent->softc;

	DPRINTFN(3,("uhub_disconnect_port: up=%p dev=%p port=%d\n", 
		    up, dev, up->portno));

	printf("%s: at %s port %d (addr %d) disconnected\n",
	       USBDEVNAME(sc->sc_dev), USBDEVNAME(scp->sc_dev),
	       up->portno, dev->address);

	if (!dev->cdesc) {
		/* Partially attached device, just drop it. */
		dev->bus->devices[dev->address] = 0;
		up->device = 0;
		return;
	}

	/* Remove the device */
	for (i = 0; i < dev->cdesc->bNumInterface; i++) {
		for (p = LIST_FIRST(&dev->ifaces[i].pipes); p; p = n) {
			n = LIST_NEXT(p, next);
			if (p->disco)
				p->disco(p->discoarg);
		}
	}

	/* XXX Free all data structures and disable further I/O. */
	if (dev->hub) {
		struct usbd_port *rup;
		int p, nports;

		DPRINTFN(3,("usb_disconnect: hub, recursing\n"));
		nports = dev->hub->hubdesc.bNbrPorts;
		for(p = 0; p < nports; p++) {
			rup = &dev->hub->ports[p];
			if (rup->device)
				uhub_disconnect_port(rup);
		}
	}
#if defined(__FreeBSD__)
	USB_DISCONNECTED(sc->sc_dev);
	device_delete_child(scp->sc_dev, sc->sc_dev);
#endif

	/* clean up the kitchen */
	for (i = 0; i < dev->cdesc->bNumInterface; i++) {
		for (p = LIST_FIRST(&dev->ifaces[i].pipes); p; p = n) {
			n = LIST_NEXT(p, next);
			usbd_abort_pipe(p);
			usbd_close_pipe(p);
		}
	}

	dev->bus->devices[dev->address] = 0;
	up->device = 0;
	/* XXX free */
}

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
	else
		usb_needs_explore(sc->sc_hub->bus);
}

#if defined(__FreeBSD__)
DRIVER_MODULE(uhub, usb, uhubroot_driver, uhubroot_devclass, 0, 0);
DRIVER_MODULE(uhub, uhub, uhub_driver, uhub_devclass, usbd_driver_load, 0);
#endif
