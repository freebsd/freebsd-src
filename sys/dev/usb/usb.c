/*	$NetBSD: usb.c,v 1.3 1998/08/01 18:16:20 augustss Exp $	*/
/*	FreeBSD $Id$ */

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

/*
 * USB spec: http://www.teleport.com/cgi-bin/mailmerge.cgi/~usb/cgiform.tpl
 * More USB specs at http://www.usb.org/developers/index.shtml
 */

#include <dev/usb/usb_port.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#if defined(__NetBSD__)
#include <sys/device.h>
#else
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/ioccom.h>
#include <sys/uio.h>
#include <sys/conf.h>
#endif
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/select.h>

#include <dev/usb/usb.h>

#if defined(__FreeBSD__)
MALLOC_DEFINE(M_USB, "USB", "USB");
MALLOC_DEFINE(M_USBDEV, "USBdev", "USB device");
#endif

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_quirks.h>

#if defined(__FreeBSD__)
#include "usb_if.h"
#endif

#ifdef USB_DEBUG
#define DPRINTF(x)	if (usbdebug) printf x
#define DPRINTFN(n,x)	if (usbdebug>(n)) printf x
int	usbdebug = 2;
int	uhcidebug = 2;
int	ohcidebug = 2;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define USBUNIT(dev) (minor(dev))

struct usb_softc {
	bdevice sc_dev;		/* base device */
	usbd_bus_handle sc_bus;		/* USB controller */
	struct usbd_port sc_port;	/* dummy port for root hub */
	char sc_running;
	char sc_exploring;
	struct selinfo sc_consel;	/* waiting for connect change */
};

#if defined(__NetBSD__)
int usb_match __P((struct device *, struct cfdata *, void *));
void usb_attach __P((struct device *, struct device *, void *));

int usbopen __P((dev_t, int, int, struct proc *));
int usbclose __P((dev_t, int, int, struct proc *));
int usbioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
int usbpoll __P((dev_t, int, struct proc *));

#else
static device_probe_t usb_match;
static device_attach_t usb_attach;
static bus_print_child_t usb_print_child;

d_open_t  usbopen; 
d_close_t usbclose;
d_ioctl_t usbioctl;
int usbpoll __P((dev_t, int, struct proc *));

struct cdevsw usb_cdevsw = {
	usbopen,     usbclose,    noread,         nowrite,
	usbioctl,    nullstop,    nullreset,      nodevtotty,
	seltrue,     nommap,      nostrat,
	"usb",        NULL,   -1
};
#endif

usbd_status usb_discover __P((struct usb_softc *));

#if defined(__NetBSD__)
extern struct cfdriver usb_cd;

struct cfattach usb_ca = {
	sizeof(struct usb_softc), usb_match, usb_attach
};
#else
static devclass_t usb_devclass = NULL;

static device_method_t usb_methods[] = {
	DEVMETHOD(device_probe,		usb_match),
	DEVMETHOD(device_attach,	usb_attach),

	DEVMETHOD(bus_print_child,	usb_print_child),
	{0, 0}
};

static driver_t usb_driver = {
	"usb",
	usb_methods,
	DRIVER_TYPE_MISC,
	sizeof(struct usb_softc),
};
#endif

#if defined(__NetBSD__)
int
usb_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
#else
static int
usb_match(device_t device)
#endif
{
	DPRINTF(("usbd_match\n"));
#if defined(__NetBSD__)
        return (1);
#else
	return (0);
#endif
}

#if defined(__NetBSD__)
void
usb_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct usb_softc *sc = (struct usb_softc *)self;
#else
static int
usb_attach(device_t device)
{
	struct usb_softc *sc = device_get_softc(device);
	void *aux = device_get_ivars(device);
#endif
	usbd_device_handle dev;
	usbd_status r;

#if defined(__NetBSD__)
	printf("\n");
#endif

	DPRINTF(("usbd_attach\n"));
	usbd_init();
	sc->sc_bus = aux;
	sc->sc_bus->usbctl = sc;
	sc->sc_running = 1;
	sc->sc_bus->use_polling = 1;
	sc->sc_port.power = USB_MAX_POWER;
#if defined(__FreeBSD__)
	sc->sc_dev = device;
#endif
	r = usbd_new_device(&sc->sc_dev, sc->sc_bus, 0, 0, 0, &sc->sc_port);

	if (r == USBD_NORMAL_COMPLETION) {
		dev = sc->sc_port.device;
		if (!dev->hub) {
			sc->sc_running = 0;
			DEVICE_ERROR(sc->sc_dev, ("root device is not a hub\n"));
			ATTACH_ERROR_RETURN;
		}
		sc->sc_bus->root_hub = dev;
		dev->hub->explore(sc->sc_bus->root_hub);
	} else {
		DEVICE_ERROR(sc->sc_dev, ("root hub problem, error=%d\n", r)); 
		sc->sc_running = 0;
	}
	sc->sc_bus->use_polling = 0;

	ATTACH_SUCCESS_RETURN;
}

#if defined(__NetBSD__)
int
usbctlprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	/* only "usb"es can attach to host controllers */
	if (pnp)
		printf("usb at %s", pnp);

	return (UNCONF);
}

#else
static void
usb_print_child(device_t parent, device_t child)
{
	struct usb_softc *sc = device_get_softc(child);

	printf(" at %s%d", device_get_name(parent), device_get_unit(parent));

	/* How do we get to the usbd_device_handle???
	usbd_device_handle dev = invalidadosch;

	printf(" addr %d", dev->addr);

	if (bootverbose) {
		if (dev->lowspeed)
			printf(", lowspeed");
		if (dev->self_powered)
			printf(", self powered");
		else
			printf(", %dmA", dev->power);
		printf(", config %d", dev->config);
	}
	 */
}

/* Reconfigure all the USB busses in the system
 */

int
usb_driver_load(module_t mod, int what, void *arg)
{
	/* subroutine is there but inactive at the moment
	 * the reconfiguration process has not been thought through yet.
	 */
	devclass_t ugen_devclass = devclass_find("ugen");
	device_t *devlist;
	int devcount;
	int error;

	switch (what) { 
	case MOD_LOAD:
	case MOD_UNLOAD:
		if (!usb_devclass)
			return 0;	/* just ignore call */

		if (ugen_devclass) {
			/* detach devices from generic driver if possible
			 */
			error = devclass_get_devices(ugen_devclass, &devlist,
						     &devcount);
			if (!error)
				for (devcount--; devcount >= 0; devcount--)
					(void) DEVICE_DETACH(devlist[devcount]);
		}

		error = devclass_get_devices(usb_devclass, &devlist, &devcount);
		if (error)
			return 0;	/* XXX maybe transient, or error? */

		for (devcount--; devcount >= 0; devcount--)
			USB_RECONFIGURE(devlist[devcount]);

		free(devlist, M_TEMP);
		return 0;
	}

	return 0;			/* nothing to do by us */
}

/* Set the description of the device including a malloc and copy
 */
void
usb_device_set_desc(device_t device, char *devinfo)
{
	size_t l;
	char *desc;

	if ( devinfo ) {
		l = strlen(devinfo);
		desc = malloc(l+1, M_USB, M_NOWAIT);
		if (desc)
			memcpy(desc, devinfo, l+1);
	} else
		desc = NULL;

	device_set_desc(device, desc);
}
#endif

int
usbopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
#if defined(__NetBSD__)
	int unit = USBUNIT(dev);
	struct usb_softc *sc;

	if (unit >= usb_cd.cd_ndevs)
		return (ENXIO);
	sc = usb_cd.cd_devs[unit];
#else
	device_t device = devclass_get_device(usb_devclass, USBUNIT(dev));
	struct usb_softc *sc = device_get_softc(device);
#endif

	if (sc == 0 || !sc->sc_running)
		return (ENXIO);

	return (0);
}

int
usbclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	return (0);
}

int
usbioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
#if defined(__NetBSD__)
	int unit = USBUNIT(dev);
	struct usb_softc *sc;

	if (unit >= usb_cd.cd_ndevs)
		return (ENXIO);
	sc = usb_cd.cd_devs[unit];
#else
	device_t device = devclass_get_device(usb_devclass, USBUNIT(dev));
	struct usb_softc *sc = device_get_softc(device);
#endif	

	if (sc == 0 || !sc->sc_running)
		return (ENXIO);
	switch (cmd) {
#ifdef USB_DEBUG
	case USB_SETDEBUG:
		usbdebug = uhcidebug = ohcidebug = *(int *)data;
		break;
#endif
	case USB_DISCOVER:
		usb_discover(sc);
		break;
	case USB_REQUEST:
	{
		struct usb_ctl_request *ur = (void *)data;
		int len = UGETW(ur->request.wLength);
		struct iovec iov;
		struct uio uio;
		void *ptr = 0;
		int addr = ur->addr;
		usbd_status r;
		int error = 0;

		if (len < 0 || len > 32768)
			return EINVAL;
		if (addr < 0 || addr >= USB_MAX_DEVICES || 
		    sc->sc_bus->devices[addr] == 0)
			return EINVAL;
		if (len != 0) {
			iov.iov_base = (caddr_t)ur->data;
			iov.iov_len = len;
			uio.uio_iov = &iov;
			uio.uio_iovcnt = 1;
			uio.uio_resid = len;
			uio.uio_offset = 0;
			uio.uio_segflg = UIO_USERSPACE;
			uio.uio_rw =
				ur->request.bmRequestType & UT_READ ? 
				UIO_READ : UIO_WRITE;
			uio.uio_procp = p;
			ptr = malloc(len, M_TEMP, M_WAITOK);
			if (uio.uio_rw == UIO_WRITE) {
				error = uiomove(ptr, len, &uio);
				if (error)
					goto ret;
			}
		}
		r = usbd_do_request(sc->sc_bus->devices[addr],
				    &ur->request, ptr);
		if (r) {
			error = EIO;
			goto ret;
		}
		if (len != 0) {
			if (uio.uio_rw == UIO_READ) {
				error = uiomove(ptr, len, &uio);
				if (error)
					goto ret;
			}
		}
	ret:
		if (ptr)
			free(ptr, M_TEMP);
		return (error);
		break;
	}

	case USB_DEVICEINFO:
	{
		struct usb_device_info *di = (void *)data;
		int addr = di->addr;
		usbd_device_handle dev;
		struct usbd_port *p;
		int i, r, s;

		if (addr < 1 || addr >= USB_MAX_DEVICES)
			return (EINVAL);
		dev = sc->sc_bus->devices[addr];
		if (dev == 0)
			return (ENXIO);
		di->config = dev->config;
		usbd_devinfo_vp(dev, di->product, di->vendor);
		usbd_printBCD(di->revision, UGETW(dev->ddesc.bcdDevice));
		di->class = dev->ddesc.bDeviceClass;
		di->power = dev->self_powered ? 0 : dev->power;
		di->lowspeed = dev->lowspeed;
		if (dev->hub) {
			for (i = 0; 
			     i < sizeof(di->ports) / sizeof(di->ports[0]) &&
				     i < dev->hub->hubdesc.bNbrPorts;
			     i++) {
				p = &dev->hub->ports[i];
				if (p->device)
					r = p->device->address;
				else {
					s = UGETW(p->status.wPortStatus);
					if (s & UPS_PORT_ENABLED)
						r = USB_PORT_ENABLED;
					else if (s & UPS_SUSPEND)
						r = USB_PORT_SUSPENDED;
					else if (s & UPS_PORT_POWER)
						r = USB_PORT_POWERED;
					else
						r = USB_PORT_DISABLED;
				}
				di->ports[i] = r;
			}
			di->nports = dev->hub->hubdesc.bNbrPorts;
		} else
			di->nports = 0;
		break;
	}

	case USB_DEVICESTATS:
		*(struct usb_device_stats *)data = sc->sc_bus->stats;
		break;

	default:
		return (ENXIO);
	}
	return (0);
}

int
usbpoll(dev, events, p)
	dev_t dev;
	int events;
	struct proc *p;
{
	int revents, s;
#if defined(__NetBSD__)
	int unit = USBUNIT(dev);
	struct usb_softc *sc;

	if (unit >= usb_cd.cd_ndevs)
		return (ENXIO);
	sc = usb_cd.cd_devs[unit];
#else
	device_t device = devclass_get_device(usb_devclass, USBUNIT(dev));
	struct usb_softc *sc = device_get_softc(device);
#endif	

	DPRINTFN(2, ("usbpoll: sc=%p events=0x%x\n", sc, events));
	s = splusb();
	revents = 0;
	if (events & (POLLOUT | POLLWRNORM))
		if (sc->sc_bus->needs_explore)
			revents |= events & (POLLOUT | POLLWRNORM);
	DPRINTFN(2, ("usbpoll: revents=0x%x\n", revents));
	if (revents == 0) {
		if (events & (POLLOUT | POLLWRNORM)) {
			DPRINTFN(2, ("usbpoll: selrecord\n"));
			selrecord(p, &sc->sc_consel);
		}
	}
	splx(s);
	return (revents);
}

#if defined(__NetBSD__)
/* See remarks on this in usbdi.c
 */
int
usb_bus_count()
{
	int i, n;

	for (i = n = 0; i < usb_cd.cd_ndevs; i++)
		if (usb_cd.cd_devs[i])
			n++;
	return (n);
}

usbd_status
usb_get_bus_handle(n, h)
	int n;
	usbd_bus_handle *h;
{
	int i;

	for (i = 0; i < usb_cd.cd_ndevs; i++)
		if (usb_cd.cd_devs[i] && n-- == 0) {
			*h = usb_cd.cd_devs[i];
			return (USBD_NORMAL_COMPLETION);
		}
	return (USBD_INVAL);
}
#endif

usbd_status
usb_discover(sc)
	struct usb_softc *sc;
{
	int s;

	/* Explore device tree from the root */
	/* We need mutual exclusion while traversing the device tree. */
	s = splusb();
	while (sc->sc_exploring)
		tsleep(&sc->sc_exploring, PRIBIO, "usbdis", 0);
	sc->sc_exploring = 1;
	sc->sc_bus->needs_explore = 0;
	splx(s);

	sc->sc_bus->root_hub->hub->explore(sc->sc_bus->root_hub);

	s = splusb();
	sc->sc_exploring = 0;
	wakeup(&sc->sc_exploring);
	splx(s);
	/* XXX should we start over if sc_needsexplore is set again? */
	return (0);
}

void
usb_needs_explore(bus)
	usbd_bus_handle bus;
{
	bus->needs_explore = 1;
	selwakeup(&bus->usbctl->sc_consel);
}

#if defined(__FreeBSD__)
DRIVER_MODULE(usb, root, usb_driver, usb_devclass, 0, 0);
#endif
