/*	$NetBSD: uhub.c,v 1.68 2004/06/29 06:30:05 mycroft Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * USB spec: http://www.usb.org/developers/docs/usbspec.zip
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>

#include <machine/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>

#define UHUB_INTR_INTERVAL 255	/* ms */

#ifdef USB_DEBUG
#define DPRINTF(x)	if (uhubdebug) printf x
#define DPRINTFN(n,x)	if (uhubdebug > (n)) printf x
#define DEVPRINTF(x)	if (uhubdebug) device_printf x
#define DEVPRINTFN(n, x)if (uhubdebug > (n)) device_printf x
int	uhubdebug = 0;
SYSCTL_NODE(_hw_usb, OID_AUTO, uhub, CTLFLAG_RW, 0, "USB uhub");
SYSCTL_INT(_hw_usb_uhub, OID_AUTO, debug, CTLFLAG_RW,
	   &uhubdebug, 0, "uhub debug level");
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#define DEVPRINTF(x)
#define DEVPRINTFN(n,x)
#endif

struct uhub_softc {
	device_t		sc_dev;		/* base device */
	usbd_device_handle	sc_hub;		/* USB device */
	usbd_pipe_handle	sc_ipipe;	/* interrupt pipe */
	u_int8_t		sc_status[1];	/* XXX more ports */
	u_char			sc_running;
};
#define UHUB_PROTO(sc) ((sc)->sc_hub->ddesc.bDeviceProtocol)
#define UHUB_IS_HIGH_SPEED(sc) (UHUB_PROTO(sc) != UDPROTO_FSHUB)
#define UHUB_IS_SINGLE_TT(sc) (UHUB_PROTO(sc) == UDPROTO_HSHUBSTT)

static usbd_status uhub_explore(usbd_device_handle hub);
static void uhub_intr(usbd_xfer_handle, usbd_private_handle,usbd_status);

static bus_child_location_str_t uhub_child_location_str;
static bus_child_pnpinfo_str_t uhub_child_pnpinfo_str;

/*
 * We need two attachment points:
 * hub to usb and hub to hub
 * Every other driver only connects to hubs
 */

/* XXX driver_added needs special care */
USB_DECLARE_DRIVER_INIT(uhub,
	DEVMETHOD(bus_child_pnpinfo_str, uhub_child_pnpinfo_str),
	DEVMETHOD(bus_child_location_str, uhub_child_location_str),
	DEVMETHOD(bus_driver_added, bus_generic_driver_added),
	DEVMETHOD(device_suspend, bus_generic_suspend),
	DEVMETHOD(device_resume, bus_generic_resume),
	DEVMETHOD(device_shutdown, bus_generic_shutdown)
	);

/* Create the driver instance for the hub connected to usb case. */
devclass_t uhubroot_devclass;

/* XXX driver_added needs special care */
static device_method_t uhubroot_methods[] = {
	DEVMETHOD(bus_child_location_str, uhub_child_location_str),
	DEVMETHOD(bus_child_pnpinfo_str, uhub_child_pnpinfo_str),
	DEVMETHOD(bus_driver_added, bus_generic_driver_added),

	DEVMETHOD(device_probe, uhub_match),
	DEVMETHOD(device_attach, uhub_attach),
	DEVMETHOD(device_detach, uhub_detach),
	DEVMETHOD(device_suspend, bus_generic_suspend),
	DEVMETHOD(device_resume, bus_generic_resume),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),
	{0,0}
};

static	driver_t uhubroot_driver = {
	"uhub",
	uhubroot_methods,
	sizeof(struct uhub_softc)
};

USB_MATCH(uhub)
{
	USB_MATCH_START(uhub, uaa);
	usb_device_descriptor_t *dd = usbd_get_device_descriptor(uaa->device);

	DPRINTFN(5,("uhub_match, dd=%p\n", dd));
	/*
	 * The subclass for hubs seems to be 0 for some and 1 for others,
	 * so we just ignore the subclass.
	 */
	if (uaa->iface == NULL && dd->bDeviceClass == UDCLASS_HUB)
		return (UMATCH_DEVCLASS_DEVSUBCLASS);
	return (UMATCH_NONE);
}

int
uhub_attach(device_t self)
{
	struct uhub_softc *sc = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
	usbd_device_handle dev = uaa->device;
	usbd_status err;
	struct usbd_hub *hub = NULL;
	usb_device_request_t req;
	usb_hub_descriptor_t hubdesc;
	int p, port, nports, nremov, pwrdly;
	usbd_interface_handle iface;
	usb_endpoint_descriptor_t *ed;
	struct usbd_tt *tts = NULL;

	DPRINTFN(1,("uhub_attach\n"));
	sc->sc_hub = dev;
	sc->sc_dev = self;

	if (dev->depth > 0 && UHUB_IS_HIGH_SPEED(sc)) {
		device_printf(sc->sc_dev, "%s transaction translator%s\n",
		    UHUB_IS_SINGLE_TT(sc) ? "single" : "multiple",
		    UHUB_IS_SINGLE_TT(sc) ? "" : "s");
	}
	err = usbd_set_config_index(dev, 0, 1);
	if (err) {
		DEVPRINTF((sc->sc_dev, "configuration failed, error=%s\n",
		    usbd_errstr(err)));
		return (ENXIO);
	}

	if (dev->depth > USB_HUB_MAX_DEPTH) {
		device_printf(sc->sc_dev, "hub depth (%d) exceeded, hub ignored\n",
		    USB_HUB_MAX_DEPTH);
		return (ENXIO);
	}

	/* Get hub descriptor. */
	req.bmRequestType = UT_READ_CLASS_DEVICE;
	req.bRequest = UR_GET_DESCRIPTOR;
	USETW2(req.wValue, (dev->address > 1 ? UDESC_HUB : 0), 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, USB_HUB_DESCRIPTOR_SIZE);
	DPRINTFN(1,("usb_init_hub: getting hub descriptor\n"));
	err = usbd_do_request(dev, &req, &hubdesc);
	nports = hubdesc.bNbrPorts;
	if (!err && nports > 7) {
		USETW(req.wLength, USB_HUB_DESCRIPTOR_SIZE + (nports+1) / 8);
		err = usbd_do_request(dev, &req, &hubdesc);
	}
	if (err) {
		DEVPRINTF((sc->sc_dev, "getting hub descriptor failed: %s\n",
		    usbd_errstr(err)));
		return (ENXIO);
	}

	for (nremov = 0, port = 1; port <= nports; port++)
		if (!UHD_NOT_REMOV(&hubdesc, port))
			nremov++;
	device_printf(sc->sc_dev, "%d port%s with %d removable, %s powered\n",
	    nports, nports != 1 ? "s" : "", nremov,
	    dev->self_powered ? "self" : "bus");

	if (nports == 0) {
		device_printf(sc->sc_dev, "no ports, hub ignored\n");
		goto bad;
	}

	hub = malloc(sizeof(*hub) + (nports-1) * sizeof(struct usbd_port),
		     M_USBDEV, M_NOWAIT);
	if (hub == NULL) {
		return (ENXIO);
	}
	dev->hub = hub;
	dev->hub->hubsoftc = sc;
	hub->explore = uhub_explore;
	hub->hubdesc = hubdesc;

	DPRINTFN(1,("usbhub_init_hub: selfpowered=%d, parent=%p, "
		    "parent->selfpowered=%d\n",
		 dev->self_powered, dev->powersrc->parent,
		 dev->powersrc->parent ?
		 dev->powersrc->parent->self_powered : 0));

	if (!dev->self_powered && dev->powersrc->parent != NULL &&
	    !dev->powersrc->parent->self_powered) {
		device_printf(sc->sc_dev, "bus powered hub connected to bus "
		    "powered hub, ignored\n");
		goto bad;
	}

	/* Set up interrupt pipe. */
	err = usbd_device2interface_handle(dev, 0, &iface);
	if (err) {
		device_printf(sc->sc_dev, "no interface handle\n");
		goto bad;
	}
	ed = usbd_interface2endpoint_descriptor(iface, 0);
	if (ed == NULL) {
		device_printf(sc->sc_dev, "no endpoint descriptor\n");
		goto bad;
	}
	if ((ed->bmAttributes & UE_XFERTYPE) != UE_INTERRUPT) {
		device_printf(sc->sc_dev, "bad interrupt endpoint\n");
		goto bad;
	}

	err = usbd_open_pipe_intr(iface, ed->bEndpointAddress,
		  USBD_SHORT_XFER_OK, &sc->sc_ipipe, sc, sc->sc_status,
		  sizeof(sc->sc_status), uhub_intr, UHUB_INTR_INTERVAL);
	if (err) {
		device_printf(sc->sc_dev, "cannot open interrupt pipe\n");
		goto bad;
	}

	/* Wait with power off for a while. */
	usbd_delay_ms(dev, USB_POWER_DOWN_TIME);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, dev, sc->sc_dev);

	/*
	 * To have the best chance of success we do things in the exact same
	 * order as Windoze98.  This should not be necessary, but some
	 * devices do not follow the USB specs to the letter.
	 *
	 * These are the events on the bus when a hub is attached:
	 *  Get device and config descriptors (see attach code)
	 *  Get hub descriptor (see above)
	 *  For all ports
	 *     turn on power
	 *     wait for power to become stable
	 * (all below happens in explore code)
	 *  For all ports
	 *     clear C_PORT_CONNECTION
	 *  For all ports
	 *     get port status
	 *     if device connected
	 *        wait 100 ms
	 *        turn on reset
	 *        wait
	 *        clear C_PORT_RESET
	 *        get port status
	 *        proceed with device attachment
	 */

	if (UHUB_IS_HIGH_SPEED(sc)) {
		tts = malloc((UHUB_IS_SINGLE_TT(sc) ? 1 : nports) *
		    sizeof (struct usbd_tt), M_USBDEV, M_NOWAIT);
		if (!tts)
			goto bad;
	}

	/* Set up data structures */
	for (p = 0; p < nports; p++) {
		struct usbd_port *up = &hub->ports[p];
		up->device = NULL;
		up->parent = dev;
		up->portno = p+1;
		if (dev->self_powered)
			/* Self powered hub, give ports maximum current. */
			up->power = USB_MAX_POWER;
		else
			up->power = USB_MIN_POWER;
		up->restartcnt = 0;
		if (UHUB_IS_HIGH_SPEED(sc)) {
			up->tt = &tts[UHUB_IS_SINGLE_TT(sc) ? 0 : p];
			up->tt->hub = hub;
		} else {
			up->tt = NULL;
		}
	}

	/* XXX should check for none, individual, or ganged power? */

	pwrdly = dev->hub->hubdesc.bPwrOn2PwrGood * UHD_PWRON_FACTOR
	    + USB_EXTRA_POWER_UP_TIME;
	for (port = 1; port <= nports; port++) {
		/* Turn the power on. */
		err = usbd_set_port_feature(dev, port, UHF_PORT_POWER);
		if (err)
			device_printf(sc->sc_dev,
			    "port %d power on failed, %s\n", port,
			    usbd_errstr(err));
		DPRINTF(("usb_init_port: turn on port %d power\n", port));
		/* Wait for stable power. */
		usbd_delay_ms(dev, pwrdly);
	}

	/* The usual exploration will finish the setup. */

	sc->sc_running = 1;
	return (0);
 bad:
	if (hub)
		free(hub, M_USBDEV);
	dev->hub = NULL;
	return (ENXIO);
}

usbd_status
uhub_explore(usbd_device_handle dev)
{
	usb_hub_descriptor_t *hd = &dev->hub->hubdesc;
	struct uhub_softc *sc = dev->hub->hubsoftc;
	struct usbd_port *up;
	usbd_status err;
	int speed;
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
		err = usbd_get_port_status(dev, port, &up->status);
		if (err) {
			DPRINTF(("uhub_explore: get port status failed, "
				 "error=%s\n", usbd_errstr(err)));
			continue;
		}
		status = UGETW(up->status.wPortStatus);
		change = UGETW(up->status.wPortChange);
		DEVPRINTFN(3,(sc->sc_dev,
		    "uhub_explore: port %d status 0x%04x 0x%04x\n", port,
		    status, change));
		if (change & UPS_C_PORT_ENABLED) {
			DPRINTF(("uhub_explore: C_PORT_ENABLED 0x%x\n", change));
			usbd_clear_port_feature(dev, port, UHF_C_PORT_ENABLE);
			if (change & UPS_C_CONNECT_STATUS) {
				/* Ignore the port error if the device
				   vanished. */
			} else if (status & UPS_PORT_ENABLED) {
				device_printf(sc->sc_dev,
				    "illegal enable change, port %d\n", port);
			} else {
				/* Port error condition. */
				if (up->restartcnt) /* no message first time */
					device_printf(sc->sc_dev,
					    "port error, restarting port %d\n",
					    port);

				if (up->restartcnt++ < USBD_RESTART_MAX)
					goto disco;
				else
					device_printf(sc->sc_dev,
					    "port error, giving up port %d\n",
					    port);
			}
		}
		if (!(change & UPS_C_CONNECT_STATUS)) {
			DPRINTFN(3,("uhub_explore: port=%d !C_CONNECT_"
				    "STATUS\n", port));
			/* No status change, just do recursive explore. */
			if (up->device != NULL && up->device->hub != NULL)
				up->device->hub->explore(up->device);
#if 0 && defined(DIAGNOSTIC)
			if (up->device == NULL &&
			    (status & UPS_CURRENT_CONNECT_STATUS))
				deivce_printf(sc->sc_dev,
				    "connected, no device\n");
#endif
			continue;
		}

		/* We have a connect status change, handle it. */

		DPRINTF(("uhub_explore: status change hub=%d port=%d\n",
			 dev->address, port));
		usbd_clear_port_feature(dev, port, UHF_C_PORT_CONNECTION);
		/*usbd_clear_port_feature(dev, port, UHF_C_PORT_ENABLE);*/
		/*
		 * If there is already a device on the port the change status
		 * must mean that is has disconnected.  Looking at the
		 * current connect status is not enough to figure this out
		 * since a new unit may have been connected before we handle
		 * the disconnect.
		 */
	disco:
		if (up->device != NULL) {
			/* Disconnected */
			DPRINTF(("uhub_explore: device addr=%d disappeared "
				 "on port %d\n", up->device->address, port));
			usb_disconnect_port(up, sc->sc_dev);
			usbd_clear_port_feature(dev, port,
						UHF_C_PORT_CONNECTION);
		}
		if (!(status & UPS_CURRENT_CONNECT_STATUS)) {
			/* Nothing connected, just ignore it. */
			DPRINTFN(3,("uhub_explore: port=%d !CURRENT_CONNECT"
				    "_STATUS\n", port));
			continue;
		}

		/* Connected */

		if (!(status & UPS_PORT_POWER))
			device_printf(sc->sc_dev,
			    "strange, connected port %d has no power\n", port);

		/* Wait for maximum device power up time. */
		usbd_delay_ms(dev, USB_PORT_POWERUP_DELAY);

		/* Reset port, which implies enabling it. */
		if (usbd_reset_port(dev, port, &up->status)) {
			device_printf(sc->sc_dev, "port %d reset failed\n",
			    port);
			continue;
		}
		/* Get port status again, it might have changed during reset */
		err = usbd_get_port_status(dev, port, &up->status);
		if (err) {
			DPRINTF(("uhub_explore: get port status failed, "
				 "error=%s\n", usbd_errstr(err)));
			continue;
		}
		status = UGETW(up->status.wPortStatus);
		change = UGETW(up->status.wPortChange);
		if (!(status & UPS_CURRENT_CONNECT_STATUS)) {
			/* Nothing connected, just ignore it. */
#ifdef DIAGNOSTIC
			device_printf(sc->sc_dev,
			    "port %d, device disappeared after reset\n", port);
#endif
			continue;
		}

#if 0
		if (UHUB_IS_HIGH_SPEED(sc) && !(status & UPS_HIGH_SPEED)) {
			device_printf(sc->sc_dev,
			    "port %d, transaction translation not implemented,"
			    " low/full speed device ignored\n", port);
			continue;
		}
#endif

		/* Figure out device speed */
		if (status & UPS_HIGH_SPEED)
			speed = USB_SPEED_HIGH;
		else if (status & UPS_LOW_SPEED)
			speed = USB_SPEED_LOW;
		else
			speed = USB_SPEED_FULL;
		/* Get device info and set its address. */
		err = usbd_new_device(sc->sc_dev, dev->bus,
		    dev->depth + 1, speed, port, up);
		/* XXX retry a few times? */
		if (err) {
			DPRINTFN(-1,("uhub_explore: usb_new_device failed, "
				     "error=%s\n", usbd_errstr(err)));
			/* Avoid addressing problems by disabling. */
			/* usbd_reset_port(dev, port, &up->status); */

			/*
			 * The unit refused to accept a new address, or had
			 * some other serious problem.  Since we cannot leave
			 * at 0 we have to disable the port instead.
			 */
			device_printf(sc->sc_dev,
			    "device problem (%s), disabling port %d\n",
			    usbd_errstr(err), port);
			usbd_clear_port_feature(dev, port, UHF_PORT_ENABLE);
		} else {
			/* The port set up succeeded, reset error count. */
			up->restartcnt = 0;

			if (up->device->hub)
				up->device->hub->explore(up->device);
		}
	}
	return (USBD_NORMAL_COMPLETION);
}

/*
 * Called from process context when the hub is gone.
 * Detach all devices on active ports.
 */
USB_DETACH(uhub)
{
	USB_DETACH_START(uhub, sc);
	struct usbd_hub *hub = sc->sc_hub->hub;
	struct usbd_port *rup;
	int port, nports;

	DPRINTF(("uhub_detach: sc=%port\n", sc));
	if (hub == NULL)		/* Must be partially working */
		return (0);

	usbd_abort_pipe(sc->sc_ipipe);
	usbd_close_pipe(sc->sc_ipipe);

	nports = hub->hubdesc.bNbrPorts;
	for(port = 0; port < nports; port++) {
		rup = &hub->ports[port];
		if (rup->device)
			usb_disconnect_port(rup, self);
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_hub, sc->sc_dev);

	if (hub->ports[0].tt)
		free(hub->ports[0].tt, M_USBDEV);
	free(hub, M_USBDEV);
	sc->sc_hub->hub = NULL;

	return (0);
}

int
uhub_child_location_str(device_t cbdev, device_t child, char *buf,
    size_t buflen)
{
	struct uhub_softc *sc = device_get_softc(cbdev);
	usbd_device_handle devhub = sc->sc_hub;
	usbd_device_handle dev;
	int nports;
	int port;
	int i;

	mtx_lock(&Giant);
	nports = devhub->hub->hubdesc.bNbrPorts;
	for (port = 0; port < nports; port++) {
		dev = devhub->hub->ports[port].device;
		if (dev && dev->subdevs) {
			for (i = 0; dev->subdevs[i]; i++) {
				if (dev->subdevs[i] == child) {
					if (dev->ifacenums == NULL) {
						snprintf(buf, buflen,
						    "port=%i", port);
					} else {
						snprintf(buf, buflen,
						    "port=%i interface=%i",
						    port, dev->ifacenums[i]);
					}
					goto found_dev;
				}
			}
		}
	}
	DPRINTFN(0,("uhub_child_location_str: device not on hub\n"));
	buf[0] = '\0';
found_dev:
	mtx_unlock(&Giant);
	return (0);
}

int
uhub_child_pnpinfo_str(device_t cbdev, device_t child, char *buf,
    size_t buflen)
{
	struct uhub_softc *sc = device_get_softc(cbdev);
	usbd_device_handle devhub = sc->sc_hub;
	usbd_device_handle dev;
	struct usbd_interface *iface;
	char serial[128];
	int nports;
	int port;
	int i;

	mtx_lock(&Giant);
	nports = devhub->hub->hubdesc.bNbrPorts;
	for (port = 0; port < nports; port++) {
		dev = devhub->hub->ports[port].device;
		if (dev && dev->subdevs) {
			for (i = 0; dev->subdevs[i]; i++) {
				if (dev->subdevs[i] == child) {
					goto found_dev;
				}
			}
		}
	}
	DPRINTFN(0,("uhub_child_pnpinfo_str: device not on hub\n"));
	buf[0] = '\0';
	mtx_unlock(&Giant);
	return (0);

found_dev:
	/* XXX can sleep */
	(void)usbd_get_string(dev, dev->ddesc.iSerialNumber, &serial[0]);
	if (dev->ifacenums == NULL) {
		snprintf(buf, buflen, "vendor=0x%04x product=0x%04x "
		    "devclass=0x%02x devsubclass=0x%02x "
		    "release=0x%04x sernum=\"%s\"",
		    UGETW(dev->ddesc.idVendor), UGETW(dev->ddesc.idProduct),
		    dev->ddesc.bDeviceClass, dev->ddesc.bDeviceSubClass,
		    UGETW(dev->ddesc.bcdDevice), serial);
	} else {
		iface = &dev->ifaces[dev->ifacenums[i]];
		snprintf(buf, buflen, "vendor=0x%04x product=0x%04x "
		    "devclass=0x%02x devsubclass=0x%02x "
		    "release=0x%04x sernum=\"%s\" "
		    "intclass=0x%02x intsubclass=0x%02x",
		    UGETW(dev->ddesc.idVendor), UGETW(dev->ddesc.idProduct),
		    dev->ddesc.bDeviceClass, dev->ddesc.bDeviceSubClass,
		    UGETW(dev->ddesc.bcdDevice), serial,
		    iface->idesc->bInterfaceClass,
		    iface->idesc->bInterfaceSubClass);
	}
	mtx_unlock(&Giant);
	return (0);
}

/*
 * Hub interrupt.
 * This an indication that some port has changed status.
 * Notify the bus event handler thread that we need
 * to be explored again.
 */
void
uhub_intr(usbd_xfer_handle xfer, usbd_private_handle addr, usbd_status status)
{
	struct uhub_softc *sc = addr;

	DPRINTFN(5,("uhub_intr: sc=%p\n", sc));
	if (status == USBD_STALLED)
		usbd_clear_endpoint_stall_async(sc->sc_ipipe);
	else if (status == USBD_NORMAL_COMPLETION)
		usb_needs_explore(sc->sc_hub);
}

DRIVER_MODULE(uhub, usb, uhubroot_driver, uhubroot_devclass, 0, 0);
DRIVER_MODULE(uhub, uhub, uhub_driver, uhub_devclass, usbd_driver_load, 0);
