/*	$NetBSD: uhub.c,v 1.5 1998/08/02 22:30:52 augustss Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Lennart Augustsson <augustss@carlstedt.se>
 *         Carlstedt Research & Technology
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

#include <dev/usb/usb_port.h>

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
#define DPRINTF(x)	if (usbdebug) printf x
#define DPRINTFN(n,x)	if (usbdebug>(n)) printf x
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

#if defined(__NetBSD__)
int uhub_match __P((struct device *, struct cfdata *, void *));
void uhub_attach __P((struct device *, struct device *, void *));
void uhub_detach __P((struct device *));
#elif defined(__FreeBSD__)
static device_probe_t uhub_match;
static device_attach_t uhub_attach;
static device_detach_t uhub_detach;
#endif

usbd_status uhub_init_port __P((int, struct usbd_port *, usbd_device_handle));
void uhub_disconnect __P((struct usbd_port *up, int portno));
usbd_status uhub_explore __P((usbd_device_handle hub));
void uhub_intr __P((usbd_request_handle, usbd_private_handle, usbd_status));

/*void uhub_disco __P((void *));*/

#if defined(__NetBSD__)
extern struct cfdriver uhub_cd;

struct cfattach uhub_ca = {
	sizeof(struct uhub_softc), uhub_match, uhub_attach
};

struct cfattach uhub_uhub_ca = {
	sizeof(struct uhub_softc), uhub_match, uhub_attach
};

#elif defined(__FreeBSD__)
static devclass_t uhub_devclass;

static device_method_t uhub_methods[] = {
	DEVMETHOD(device_probe, uhub_match),
	DEVMETHOD(device_attach, uhub_attach),
	DEVMETHOD(device_detach, uhub_detach),
	{0,0}
};

static driver_t uhub_driver = {
	"usb",		/* this is silly, but necessary. The uhub
			 * implements a usb bus on top of a usb bus,
			 * but the problem is that name of the driver
			 * is used a the name of the device class it
			 * implements.
			 */
	uhub_methods,
	DRIVER_TYPE_MISC,
	sizeof(struct uhub_softc)
};
#endif

#if defined(__NetBSD__)
int
uhub_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct usb_attach_arg *uaa = aux;
#elif defined(__FreeBSD__)
static int
uhub_match(device_t device)
{
	struct usb_attach_arg *uaa = device_get_ivars(device);
#endif
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

#if defined(__NetBSD__)
void
uhub_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct uhub_softc *sc = (struct uhub_softc *)self;
	struct usb_attach_arg *uaa = aux;
#elif defined(__FreeBSD__)
static int
uhub_attach(device_t self)
{
	struct uhub_softc *sc = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
#endif
	usbd_device_handle dev = uaa->device;
	char devinfo[1024];
	usbd_status r;
	struct usbd_hub *hub;
	usb_device_request_t req;
	usb_hub_descriptor_t hubdesc;
	int port, nports;
	usbd_interface_handle iface;
	usb_endpoint_descriptor_t *ed;

	DPRINTFN(10,("uhub_attach\n"));
	sc->sc_hub = dev;
	usbd_devinfo(dev, 1, devinfo);
#if defined(__FreeBSD__)
	usb_device_set_desc(self, devinfo);
	printf("%s%d", device_get_name(self), device_get_unit(self));
#endif
	printf(": %s\n", devinfo);
	sc->sc_dev = self;

	r = usbd_set_config_no(dev, 0, 1);
	if (r != USBD_NORMAL_COMPLETION) {
		DEVICE_ERROR(sc->sc_dev, ("configuration failed, error=%d\n", r));
		ATTACH_ERROR_RETURN;
	}

	if (dev->depth > USB_HUB_MAX_DEPTH) {
		DEVICE_ERROR(sc->sc_dev, ("hub depth (%d) exceeded, hub ignored\n",
		       USB_HUB_MAX_DEPTH));
		ATTACH_ERROR_RETURN;
	}

	/* Get hub descriptor. */
	req.bmRequestType = UT_READ_CLASS_DEVICE;
	req.bRequest = UR_GET_DESCRIPTOR;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, USB_HUB_DESCRIPTOR_SIZE);
	DPRINTFN(1,("usb_init_hub: getting hub descriptor\n"));
	/* XXX not correct for hubs with >7 ports */
	r = usbd_do_request(dev, &req, &hubdesc);
	if (r != USBD_NORMAL_COMPLETION) {
		DEVICE_ERROR(sc->sc_dev, ("getting hub descriptor failed, error=%d\n", r));
		ATTACH_ERROR_RETURN;
	}

	/* XXX block should be moved down to avoid memory leaking (or an overdose of free()'s) */
	nports = hubdesc.bNbrPorts;
	hub = malloc(sizeof(*hub) + (nports-1) * sizeof(struct usbd_port),
		     M_USB, M_NOWAIT);
	if (hub == 0)
		ATTACH_ERROR_RETURN;
	dev->hub = hub;
	dev->hub->hubdata = sc;
	hub->explore = uhub_explore;
	hub->hubdesc = hubdesc;
	hub->nports  = nports;

	DPRINTFN(1,("usbhub_init_hub: selfpowered=%d, parent=%p, parent->selfpowered=%d\n",
		 dev->self_powered, dev->powersrc->parent,
		 dev->powersrc->parent ? 
		 dev->powersrc->parent->self_powered : 0));
	if (!dev->self_powered && dev->powersrc->parent &&
	    !dev->powersrc->parent->self_powered) {
		DEVICE_ERROR(sc->sc_dev, ("bus powered hub connected to bus powered hub, ignored\n"));
		ATTACH_ERROR_RETURN;
	}

	/* Set up interrupt pipe. */
	r = usbd_device2interface_handle(dev, 0, &iface);
	if (r != USBD_NORMAL_COMPLETION) {
		DEVICE_ERROR(sc->sc_dev, ("no interface handle\n"));
		ATTACH_ERROR_RETURN;
	}
	ed = usbd_interface2endpoint_descriptor(iface, 0);
	if (ed == 0) {
		DEVICE_ERROR(sc->sc_dev, ("no endpoint descriptor\n"));
		ATTACH_ERROR_RETURN;
	}
	if ((ed->bmAttributes & UE_XFERTYPE) != UE_INTERRUPT) {
		DEVICE_ERROR(sc->sc_dev, ("bad interrupt endpoint\n"));
		ATTACH_ERROR_RETURN;
	}

	r = usbd_open_pipe_intr(iface, ed->bEndpointAddress,USBD_SHORT_XFER_OK,
				&sc->sc_ipipe, sc, sc->sc_status, 
				sizeof(sc->sc_status),
				uhub_intr);
	if (r != USBD_NORMAL_COMPLETION) {
		DEVICE_ERROR(sc->sc_dev, ("cannot open interrupt pipe\n"));
		ATTACH_ERROR_RETURN;
	}

	for (port = 1; port <= nports; port++) {
		r = uhub_init_port(port, &hub->ports[port-1], dev);
		if (r != USBD_NORMAL_COMPLETION)
			DEVICE_ERROR(sc->sc_dev, ("init of port %d failed\n", port));
	}
	sc->sc_running = 1;

	ATTACH_SUCCESS_RETURN;
}

#if defined(__NetBSD__)
static int
uhub_detach(self)
	struct device *self;
{
	struct uhub_softc *sc = (struct uhub_softc *)self;
#elif defined(__FreeBSD__)
static int
uhub_detach(device_t self)
{
	struct uhub_softc *sc = device_get_softc(self);
#endif
	int nports = sc->sc_hub->hub->hubdesc.bNbrPorts;
	int port;

	for (port = 1; port <= nports; port++) {
		if (sc->sc_hub->hub->ports[port-1].device)
			uhub_disconnect(&sc->sc_hub->hub->ports[port-1], port);
	}

	free(sc->sc_hub->hub, M_USB);

	return 0;
}

usbd_status
uhub_init_port(port, uport, dev)
	int port;
	struct usbd_port *uport;
	usbd_device_handle dev;
{
	usbd_status r;
	u_int16_t pstatus;

	uport->device = 0;
	uport->parent = dev;
	r = usbd_get_port_status(dev, port, &uport->status);
	if (r != USBD_NORMAL_COMPLETION)
		return r;
	pstatus = UGETW(uport->status.wPortStatus);
	DPRINTF(("usbd_init_port: adding hub port=%d status=0x%04x change=0x%04x\n",
		 port, pstatus, UGETW(uport->status.wPortChange)));
	if ((pstatus & UPS_PORT_POWER) == 0) {
		/* Port lacks power, turn it on */
		r = usbd_set_port_feature(dev, port, UHF_PORT_POWER);
		if (r != USBD_NORMAL_COMPLETION)
			return (r);
		r = usbd_get_port_status(dev, port, &uport->status);
		if (r != USBD_NORMAL_COMPLETION)
			return (r);
		DPRINTF(("usb_init_port: turn on port %d power status=0x%04x change=0x%04x\n",
			 port, UGETW(uport->status.wPortStatus),
			 UGETW(uport->status.wPortChange)));
		/* Wait for stable power. */
		usbd_delay_ms(dev->bus, dev->hub->hubdesc.bPwrOn2PwrGood * 
			                UHD_PWRON_FACTOR);
	}
	if (dev->self_powered)
		/* Self powered hub, give ports maximum current. */
		uport->power = USB_MAX_POWER;
	else
		uport->power = USB_MIN_POWER;
	return (USBD_NORMAL_COMPLETION);
}

usbd_status
uhub_explore(dev)
	usbd_device_handle dev;
{
	usb_hub_descriptor_t *hd = &dev->hub->hubdesc;
	struct uhub_softc *sc = dev->hub->hubdata;
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
			DPRINTF(("uhub_explore: get port status failed, error=%d\n",
				 r));
			continue;
		}
		status = UGETW(up->status.wPortStatus);
		change = UGETW(up->status.wPortChange);
		DPRINTFN(5, ("uhub_explore: port %d status 0x%04x 0x%04x\n",
			     port, status, change));
		if (!(change & UPS_CURRENT_CONNECT_STATUS)) {
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
		if (up->device) {
			/* Disconnected */
			DPRINTF(("uhub_explore: device %d disappeared on port %d\n", 
				 up->device->address, port));
			uhub_disconnect(up, port);
			usbd_clear_port_feature(dev, port, 
						UHF_C_PORT_CONNECTION);
		}
		if (!(status & UPS_CURRENT_CONNECT_STATUS))
			continue;

		/* Connected */
		/* Wait for maximum device power up time. */
		usbd_delay_ms(dev->bus, USB_PORT_POWERUP_DELAY);
		/* Reset port, which implies enabling it. */
		if (usbd_reset_port(dev, port, &up->status) != 
		    USBD_NORMAL_COMPLETION)
			continue;

		/* Wait for power to settle in device. */
		usbd_delay_ms(dev->bus, USB_POWER_SETTLE);
			
		/* Get device info and set its address. */
		r = usbd_new_device(&sc->sc_dev, dev->bus, 
				    dev->depth + 1, status & UPS_LOW_SPEED, 
				    port, up);
		/* XXX retry a few times? */
		if (r != USBD_NORMAL_COMPLETION) {
			DPRINTFN(-1,("uhub_explore: usb_new_device failed, error=%d\n", r));
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
				 * at 0 we have to disable the port
				 * instead. */
				/*
				DEVICE_ERROR(*parent, ("device problem, disable port %d\n",
						port));
				 */
				usbd_clear_port_feature(dev, port, 
							UHF_PORT_ENABLE);
			}
		} else {
			if (up->device->hub)
				up->device->hub->explore(up->device);
		}
	}
	return (USBD_NORMAL_COMPLETION);
}

void
uhub_disconnect(up, portno)
	struct usbd_port *up;
	int portno;
{
	usbd_device_handle dev = up->device;
	usbd_pipe_handle p, n;
	usb_hub_descriptor_t *hd;
	struct usbd_port *spi;
	int i, port;

	DPRINTFN(3,("uhub_disconnect: up=%p dev=%p port=%d\n", 
		    up, dev, portno));

	DEVICE_MSG(dev->bdev, ("device addr %d%s on hub addr %d, port %d disconnected\n",
	       dev->address, dev->hub ? " (hub)" : "", up->parent->address, portno));

	for (i = 0; i < dev->cdesc->bNumInterface; i++) {
		for (p = LIST_FIRST(&dev->ifaces[i].pipes); p; p = n) {
			n = LIST_NEXT(p, next);
			if (p->disco)
				p->disco(p->discoarg);
			usbd_abort_pipe(p);
			usbd_close_pipe(p);
		}
	}

	/* clean up the kindergarten, get rid of the kids */
	usbd_remove_device(dev, up);
}

void
uhub_intr(reqh, addr, status)
	usbd_request_handle reqh;
	usbd_private_handle addr;
	usbd_status status;
{
	struct uhub_softc *sc = addr;

	DPRINTFN(5,("uhub_intr: sc=%p\n", sc));
#if 0
	if (status != USBD_NORMAL_COMPLETION)
		usbd_clear_endpoint_stall(sc->sc_ipipe);
	else
#endif
		usb_needs_explore(sc->sc_hub->bus);
}

#if defined(__FreeBSD__)
DRIVER_MODULE(uhub, usb, uhub_driver, uhub_devclass, usb_driver_load, 0);
#endif
