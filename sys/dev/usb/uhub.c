/*	$NetBSD: uhub.c,v 1.64 2003/02/08 03:32:51 ichiro Exp $	*/
/*	$FreeBSD$	*/

/*
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

/*
 * USB spec: http://www.usb.org/developers/docs/usbspec.zip
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/device.h>
#include <sys/proc.h>
#elif defined(__FreeBSD__)
#include <sys/module.h>
#include <sys/bus.h>
#include "bus_if.h"
#endif
#include <sys/sysctl.h>

#include <machine/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>

#define UHUB_INTR_INTERVAL 255	/* ms */

#ifdef USB_DEBUG
#define DPRINTF(x)	if (uhubdebug) logprintf x
#define DPRINTFN(n,x)	if (uhubdebug>(n)) logprintf x
int	uhubdebug = 0;
SYSCTL_NODE(_hw_usb, OID_AUTO, uhub, CTLFLAG_RW, 0, "USB uhub");
SYSCTL_INT(_hw_usb_uhub, OID_AUTO, debug, CTLFLAG_RW,
	   &uhubdebug, 0, "uhub debug level");
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

Static usbd_status uhub_explore(usbd_device_handle hub);
Static void uhub_intr(usbd_xfer_handle, usbd_private_handle,usbd_status);

#if defined(__FreeBSD__)
Static bus_driver_added_t uhub_driver_added;
Static bus_child_detached_t uhub_child_detached;
#endif


/*
 * We need two attachment points:
 * hub to usb and hub to hub
 * Every other driver only connects to hubs
 */

#if defined(__NetBSD__) || defined(__OpenBSD__)
USB_DECLARE_DRIVER(uhub);

/* Create the driver instance for the hub connected to hub case */
CFATTACH_DECL(uhub_uhub, sizeof(struct uhub_softc),
    uhub_match, uhub_attach, uhub_detach, uhub_activate);
#elif defined(__FreeBSD__)
USB_DECLARE_DRIVER_INIT(uhub,
			DEVMETHOD(bus_driver_added, uhub_driver_added),
			DEVMETHOD(bus_child_detached, uhub_child_detached),
			DEVMETHOD(device_suspend, bus_generic_suspend),
			DEVMETHOD(device_resume, bus_generic_resume),
			DEVMETHOD(device_shutdown, bus_generic_shutdown)
			);

/* Create the driver instance for the hub connected to usb case. */
devclass_t uhubroot_devclass;

Static device_method_t uhubroot_methods[] = {
	DEVMETHOD(device_probe, uhub_match),
	DEVMETHOD(device_attach, uhub_attach),

	/* detach is not allowed for a root hub */
	DEVMETHOD(device_suspend, bus_generic_suspend),
	DEVMETHOD(device_resume, bus_generic_resume),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),
	{0,0}
};

Static	driver_t uhubroot_driver = {
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
	if (uaa->iface == NULL && dd->bDeviceClass == UDCLASS_HUB)
		return (UMATCH_DEVCLASS_DEVSUBCLASS);
	return (UMATCH_NONE);
}

USB_ATTACH(uhub)
{
	USB_ATTACH_START(uhub, sc, uaa);
	usbd_device_handle dev = uaa->device;
	char *devinfo;
	usbd_status err;
	struct usbd_hub *hub;
	usb_device_request_t req;
	usb_hub_descriptor_t hubdesc;
	int p, port, nports, nremov, pwrdly;
	usbd_interface_handle iface;
	usb_endpoint_descriptor_t *ed;

	devinfo = malloc(1024, M_TEMP, M_NOWAIT);
	if (devinfo == NULL) {
		USB_ATTACH_ERROR_RETURN;
	}
	DPRINTFN(1,("uhub_attach\n"));
	sc->sc_hub = dev;
	usbd_devinfo(dev, 1, devinfo);
	USB_ATTACH_SETUP;
	printf("%s: %s\n", USBDEVNAME(sc->sc_dev), devinfo);

	err = usbd_set_config_index(dev, 0, 1);
	if (err) {
		DPRINTF(("%s: configuration failed, error=%s\n",
			 USBDEVNAME(sc->sc_dev), usbd_errstr(err)));
		free(devinfo, M_TEMP);
		USB_ATTACH_ERROR_RETURN;
	}

	if (dev->depth > USB_HUB_MAX_DEPTH) {
		printf("%s: hub depth (%d) exceeded, hub ignored\n",
		       USBDEVNAME(sc->sc_dev), USB_HUB_MAX_DEPTH);
		free(devinfo, M_TEMP);
		USB_ATTACH_ERROR_RETURN;
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
		DPRINTF(("%s: getting hub descriptor failed, error=%s\n",
			 USBDEVNAME(sc->sc_dev), usbd_errstr(err)));
		free(devinfo, M_TEMP);
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
	if (hub == NULL) {
		free(devinfo, M_TEMP);
		USB_ATTACH_ERROR_RETURN;
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
		printf("%s: bus powered hub connected to bus powered hub, "
		       "ignored\n", USBDEVNAME(sc->sc_dev));
		goto bad;
	}

	/* Set up interrupt pipe. */
	err = usbd_device2interface_handle(dev, 0, &iface);
	if (err) {
		printf("%s: no interface handle\n", USBDEVNAME(sc->sc_dev));
		goto bad;
	}
	ed = usbd_interface2endpoint_descriptor(iface, 0);
	if (ed == NULL) {
		printf("%s: no endpoint descriptor\n", USBDEVNAME(sc->sc_dev));
		goto bad;
	}
	if ((ed->bmAttributes & UE_XFERTYPE) != UE_INTERRUPT) {
		printf("%s: bad interrupt endpoint\n", USBDEVNAME(sc->sc_dev));
		goto bad;
	}

	err = usbd_open_pipe_intr(iface, ed->bEndpointAddress,
		  USBD_SHORT_XFER_OK, &sc->sc_ipipe, sc, sc->sc_status,
		  sizeof(sc->sc_status), uhub_intr, UHUB_INTR_INTERVAL);
	if (err) {
		printf("%s: cannot open interrupt pipe\n",
		       USBDEVNAME(sc->sc_dev));
		goto bad;
	}

	/* Wait with power off for a while. */
	usbd_delay_ms(dev, USB_POWER_DOWN_TIME);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, dev, USBDEV(sc->sc_dev));

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

	/* Set up data structures */
	for (p = 0; p < nports; p++) {
		struct usbd_port *up = &hub->ports[p];
		up->device = 0;
		up->parent = dev;
		up->portno = p+1;
		if (dev->self_powered)
			/* Self powered hub, give ports maximum current. */
			up->power = USB_MAX_POWER;
		else
			up->power = USB_MIN_POWER;
	}

	/* XXX should check for none, individual, or ganged power? */

	pwrdly = dev->hub->hubdesc.bPwrOn2PwrGood * UHD_PWRON_FACTOR
	    + USB_EXTRA_POWER_UP_TIME;
	for (port = 1; port <= nports; port++) {
		/* Turn the power on. */
		err = usbd_set_port_feature(dev, port, UHF_PORT_POWER);
		if (err)
			printf("%s: port %d power on failed, %s\n",
			       USBDEVNAME(sc->sc_dev), port,
			       usbd_errstr(err));
		DPRINTF(("usb_init_port: turn on port %d power\n", port));
		/* Wait for stable power. */
		usbd_delay_ms(dev, pwrdly);
	}

	/* The usual exploration will finish the setup. */

	sc->sc_running = 1;

	USB_ATTACH_SUCCESS_RETURN;

 bad:
	free(hub, M_USBDEV);
	free(devinfo, M_TEMP);
	dev->hub = 0;
	USB_ATTACH_ERROR_RETURN;
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
		DPRINTFN(3,("uhub_explore: %s port %d status 0x%04x 0x%04x\n",
			    USBDEVNAME(sc->sc_dev), port, status, change));
		if (change & UPS_C_PORT_ENABLED) {
			DPRINTF(("uhub_explore: C_PORT_ENABLED\n"));
			usbd_clear_port_feature(dev, port, UHF_C_PORT_ENABLE);
			if (status & UPS_PORT_ENABLED) {
				printf("%s: illegal enable change, port %d\n",
				       USBDEVNAME(sc->sc_dev), port);
			} else {
				/* Port error condition. */
				if (up->restartcnt) /* no message first time */
					printf("%s: port error, restarting "
					       "port %d\n",
					       USBDEVNAME(sc->sc_dev), port);

				if (up->restartcnt++ < USBD_RESTART_MAX)
					goto disco;
				else
					printf("%s: port error, giving up "
					       "port %d\n",
					       USBDEVNAME(sc->sc_dev), port);
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
				printf("%s: connected, no device\n",
				       USBDEVNAME(sc->sc_dev));
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
			usb_disconnect_port(up, USBDEV(sc->sc_dev));
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
			printf("%s: strange, connected port %d has no power\n",
			       USBDEVNAME(sc->sc_dev), port);

		/* Wait for maximum device power up time. */
		usbd_delay_ms(dev, USB_PORT_POWERUP_DELAY);

		/* Reset port, which implies enabling it. */
		if (usbd_reset_port(dev, port, &up->status)) {
			printf("%s: port %d reset failed\n",
			       USBDEVNAME(sc->sc_dev), port);
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
			printf("%s: port %d, device disappeared after reset\n",
			       USBDEVNAME(sc->sc_dev), port);
#endif
			continue;
		}

		/* Figure out device speed */
		if (status & UPS_HIGH_SPEED)
			speed = USB_SPEED_HIGH;
		else if (status & UPS_LOW_SPEED)
			speed = USB_SPEED_LOW;
		else
			speed = USB_SPEED_FULL;
		/* Get device info and set its address. */
		err = usbd_new_device(USBDEV(sc->sc_dev), dev->bus,
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
			printf("%s: device problem, disabling port %d\n",
			       USBDEVNAME(sc->sc_dev), port);
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

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
uhub_activate(device_ptr_t self, enum devact act)
{
	struct uhub_softc *sc = (struct uhub_softc *)self;
	struct usbd_hub *hub = sc->sc_hub->hub;
	usbd_device_handle dev;
	int nports, port, i;

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);

	case DVACT_DEACTIVATE:
		if (hub == NULL) /* malfunctioning hub */
			break;
		nports = hub->hubdesc.bNbrPorts;
		for(port = 0; port < nports; port++) {
			dev = hub->ports[port].device;
			if (dev != NULL && dev->subdevs != NULL) {
				for (i = 0; dev->subdevs[i] != NULL; i++)
					config_deactivate(dev->subdevs[i]);
			}
		}
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
	struct usbd_hub *hub = sc->sc_hub->hub;
	struct usbd_port *rup;
	int port, nports;

#if defined(__NetBSD__) || defined(__OpenBSD__)
	DPRINTF(("uhub_detach: sc=%p flags=%d\n", sc, flags));
#elif defined(__FreeBSD__)
	DPRINTF(("uhub_detach: sc=%port\n", sc));
#endif

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

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_hub,
			   USBDEV(sc->sc_dev));

	free(hub, M_USBDEV);
	sc->sc_hub->hub = NULL;

	return (0);
}

#if defined(__FreeBSD__)
/* Called when a device has been detached from it */
Static void
uhub_child_detached(device_t self, device_t child)
{
       struct uhub_softc *sc = device_get_softc(self);
       usbd_device_handle devhub = sc->sc_hub;
       usbd_device_handle dev;
       int nports;
       int port;
       int i;

       if (!devhub->hub)
               /* should never happen; children are only created after init */
               panic("hub not fully initialised, but child deleted?");

       nports = devhub->hub->hubdesc.bNbrPorts;
       for (port = 0; port < nports; port++) {
               dev = devhub->hub->ports[port].device;
               if (dev && dev->subdevs) {
                       for (i = 0; dev->subdevs[i]; i++) {
                               if (dev->subdevs[i] == child) {
                                       dev->subdevs[i] = NULL;
                                       return;
                               }
                       }
               }
       }
}

Static void
uhub_driver_added(device_t _dev, driver_t *_driver)
{
	/* Don't do anything, as reprobing does not work currently. We should
	 * really call through to usbd_new_device or a function along those
	 * lines that reinitialises the device if it is not owned by any
	 * driver. But this is complicated. Manual replugging by the user is
	 * easier.
	 */

	 ;
}
#endif


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

#if defined(__FreeBSD__)
DRIVER_MODULE(uhub, usb, uhubroot_driver, uhubroot_devclass, 0, 0);
DRIVER_MODULE(uhub, uhub, uhub_driver, uhub_devclass, usbd_driver_load, 0);
#endif
