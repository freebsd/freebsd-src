/*	$NetBSD: usb.c,v 1.12 1999/01/10 19:13:16 augustss Exp $	*/
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
 * USB specifications and other documentation can be found at
 * http://www.usb.org/developers/data/ and
 * http://www.usb.org/developers/index.html .
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

#include "usb_if.h"
#include "uhci.h"
#include "ohci.h"
#endif /* defined(__FreeBSD__) */

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_quirks.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	if (usbdebug) logprintf x
#define DPRINTFN(n,x)	if (usbdebug>(n)) logprintf x
int	usbdebug = 1;

#if NUHCI > 0
extern int uhcidebug;
#endif
#if NOHCI > 0
extern int ohcidebug;
#endif
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define USBUNIT(dev) (minor(dev))

struct usb_softc {
	bdevice sc_dev;			/* base device */
	usbd_bus_handle sc_bus;		/* USB controller */
	struct usbd_port sc_port;	/* dummy port for root hub */
	char sc_running;
	char sc_exploring;
	struct selinfo sc_consel;	/* waiting for connect change */
};

#if defined(__NetBSD__)
int usbopen __P((dev_t, int, int, struct proc *));
int usbclose __P((dev_t, int, int, struct proc *));
int usbioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
int usbpoll __P((dev_t, int, struct proc *));

#elif defined(__FreeBSD__)
d_open_t  usbopen; 
d_close_t usbclose;
d_ioctl_t usbioctl;
int usbpoll __P((dev_t, int, struct proc *));

struct cdevsw usb_cdevsw = {
	/* open */	usbopen,
	/* close */	usbclose,
	/* read */	noread,
	/* write */	nowrite,
	/* ioctl */	usbioctl,
	/* stop */	nostop,
	/* reset */	noreset,
	/* devtotty */	nodevtotty,
	/* poll */	usbpoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"usb",
	/* parms */	noparms,
	/* maj */	USB_CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
	/* maxio */	0,
	/* bmaj */	-1
};
#endif

usbd_status usb_discover __P((struct usb_softc *));

USB_DECLARE_DRIVER_INIT(usb, DEVMETHOD(bus_print_child, usbd_print_child));

USB_MATCH(usb)
{
	DPRINTF(("usbd_match\n"));
	return (UMATCH_GENERIC);
}

USB_ATTACH(usb)
{
#if defined(__NetBSD__)
	struct usb_softc *sc = (struct usb_softc *)self;
#elif defined(__FreeBSD__)
	struct usb_softc *sc = device_get_softc(self);
	void *aux = device_get_ivars(self);
#endif
	usbd_device_handle dev;
	usbd_status r;
	
#if defined(__NetBSD__)
	printf("\n");
#elif defined(__FreeBSD__)
	sc->sc_dev = self;
#endif

	DPRINTF(("usbd_attach\n"));
	usbd_init();
	sc->sc_bus = aux;
	sc->sc_bus->usbctl = sc;
	sc->sc_running = 1;
	sc->sc_bus->use_polling = 1;
	sc->sc_port.power = USB_MAX_POWER;
	r = usbd_new_device(&sc->sc_dev, sc->sc_bus, 0, 0, 0, &sc->sc_port);

	if (r == USBD_NORMAL_COMPLETION) {
		dev = sc->sc_port.device;
		if (!dev->hub) {
			sc->sc_running = 0;
			printf("%s: root device is not a hub\n", 
			       USBDEVNAME(sc->sc_dev));
			USB_ATTACH_ERROR_RETURN;
		}
		sc->sc_bus->root_hub = dev;
		dev->hub->explore(sc->sc_bus->root_hub);
	} else {
		printf("%s: root hub problem, error=%d\n", 
		       USBDEVNAME(sc->sc_dev), r); 
		sc->sc_running = 0;
	}
	sc->sc_bus->use_polling = 0;

	USB_ATTACH_SUCCESS_RETURN;
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
#endif

int
usbopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	USB_GET_SC_OPEN(usb, USBUNIT(dev), sc);

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
	USB_GET_SC(usb, USBUNIT(dev), sc);

	if (sc == 0 || !sc->sc_running)
		return (ENXIO);
	switch (cmd) {
#ifdef USB_DEBUG
	case USB_SETDEBUG:
		usbdebug = *(int *)data;
#if NUHCI > 0
		uhcidebug = *(int *)data;
#endif
#if NOHCI > 0
		ohcidebug = *(int *)data;
#endif
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

		DPRINTF(("usbioctl: USB_REQUEST addr=%d len=%d\n", addr, len));
		if (len < 0 || len > 32768)
			return (EINVAL);
		if (addr < 0 || addr >= USB_MAX_DEVICES || 
		    sc->sc_bus->devices[addr] == 0)
			return (EINVAL);
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
		r = usbd_do_request_flags(sc->sc_bus->devices[addr],
					  &ur->request, ptr,
					  ur->flags, &ur->actlen);
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
	}

	case USB_DEVICEINFO:
	{
		struct usb_device_info *di = (void *)data;
		int addr = di->addr;
		usbd_device_handle dev;

		if (addr < 1 || addr >= USB_MAX_DEVICES)
			return (EINVAL);
		dev = sc->sc_bus->devices[addr];
		if (dev == 0)
			return (ENXIO);
		usbd_fill_deviceinfo(dev, di);
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
	USB_GET_SC(usb, USBUNIT(dev), sc);

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

#if 0
int
usb_bus_count()
{
	int i, n;

	for (i = n = 0; i < usb_cd.cd_ndevs; i++)
		if (usb_cd.cd_devs[i])
			n++;
	return (n);
}
#endif

#if 0
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
int
usb_detach(device_t self)
{
	DPRINTF(("%s: unload, prevented\n", USBDEVNAME(self)));

	return (EINVAL);
}

DRIVER_MODULE(usb, uhci, usb_driver, usb_devclass, 0, 0);
DRIVER_MODULE(usb, ohci, usb_driver, usb_devclass, 0, 0);
#endif
