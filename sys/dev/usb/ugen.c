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
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#if defined(__NetBSD__)
#include <sys/device.h>
#elif defined(__FreeBSD__)
#include <sys/module.h>
#include <sys/bus.h>
#endif
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/syslog.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	if (usbdebug) printf x
#define DPRINTFN(n,x)	if (usbdebug>(n)) printf x
extern int usbdebug;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

struct ugen_softc {
	bdevice sc_dev;
	usbd_device_handle sc_udev;	/* device */
	usbd_interface_handle sc_iface;	/* interface */
	int sc_ifaceno;
	int sc_bulk;
};

#if defined(__NetBSD__)
int ugen_match __P((struct device *, struct cfdata *, void *));
void ugen_attach __P((struct device *, struct device *, void *));

extern struct cfdriver ugen_cd;

struct cfattach ugen_ca = {
	sizeof(struct ugen_softc), ugen_match, ugen_attach
};
#elif defined(__FreeBSD__)
static device_probe_t ugen_match;
static device_attach_t ugen_attach;
static device_detach_t ugen_detach;

static devclass_t ugen_devclass;

static device_method_t ugen_methods[] = {
        DEVMETHOD(device_probe, ugen_match),
        DEVMETHOD(device_attach, ugen_attach),
	DEVMETHOD(device_detach, ugen_detach),
        {0,0}
};

static driver_t ugen_driver = {
        "ugen",
        ugen_methods,
        DRIVER_TYPE_MISC,
        sizeof(struct ugen_softc)
};
#endif


#if defined(__NetBSD__)
int
ugen_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct usb_attach_arg *uaa = aux;
#elif defined(__FreeBSD__)
static int
ugen_match(device_t device)
{
	struct usb_attach_arg *uaa = device_get_ivars(device);
#endif
	usb_interface_descriptor_t *id;
	
	DPRINTFN(10,("ugen_match\n"));
	if (uaa->usegeneric)
		return UMATCH_GENERIC;
	else 
		return UMATCH_NONE;
}

#if defined(__NetBSD__)
void
ugen_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct ugen_softc *sc = (struct ugen_softc *)self;
	struct usb_attach_arg *uaa = aux;
#elif defined(__FreeBSD__)
static int
ugen_attach(device_t self)
{
	struct ugen_softc *sc = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
#endif
	usbd_device_handle dev = uaa->device;
	usb_device_descriptor_t *udd = &dev->ddesc;
	char devinfo[1024];

	usbd_devinfo(dev, 0, devinfo);
#if defined(__FreeBSD__)
	device_set_desc(self, devinfo);
	printf("%s%d", device_get_name(self), device_get_unit(self));
#endif
	printf(": %s (device class %d/%d)\n", devinfo,
	       udd->bDeviceClass, udd->bDeviceSubClass);
	sc->sc_dev = self;

	ATTACH_SUCCESS_RETURN;
}

#if defined(__FreeBSD__)
static int
ugen_detach(device_t self)
{
	/* we need to cast away the const returned by 
	 * device_get_desc
	 */
	char *devinfo = (char *) device_get_desc(self);

	if (devinfo) {
		device_set_desc(self, NULL);
		free(devinfo, M_USB);
	}

	return 0;
}

DRIVER_MODULE(ugen, usb, ugen_driver, ugen_devclass, usb_driver_load, 0);
#endif
