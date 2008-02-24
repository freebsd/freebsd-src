/*-
 * Copyright (c) 2005
 *      Bill Paul <wpaul@windriver.com>.  All rights reserved.
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
 *      This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/if_ndis/if_ndis_usb.c,v 1.9 2007/06/18 22:24:32 imp Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include "usbdevs.h"

#include <net80211/ieee80211_var.h>

#include <compat/ndis/pe_var.h>
#include <compat/ndis/cfg_var.h>
#include <compat/ndis/resource_var.h>
#include <compat/ndis/ntoskrnl_var.h>
#include <compat/ndis/ndis_var.h>
#include <dev/if_ndis/if_ndisvar.h>

MODULE_DEPEND(ndis, usb, 1, 1, 1);

static device_probe_t ndisusb_match;
static device_attach_t ndisusb_attach;
static bus_get_resource_list_t ndis_get_resource_list;

extern int ndisdrv_modevent     (module_t, int, void *);
extern int ndis_attach          (device_t);
extern int ndis_shutdown        (device_t);
extern int ndis_detach          (device_t);
extern int ndis_suspend         (device_t);
extern int ndis_resume          (device_t);

extern unsigned char drv_data[];

static device_method_t ndis_methods[] = {
        /* Device interface */
	DEVMETHOD(device_probe,		ndisusb_match),
	DEVMETHOD(device_attach,	ndisusb_attach),
	DEVMETHOD(device_detach,	ndis_detach),
	DEVMETHOD(device_shutdown,	ndis_shutdown),

        /* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
	DEVMETHOD(bus_get_resource_list, ndis_get_resource_list),

	{ 0, 0 }
};

static driver_t ndis_driver = {
	"ndis",
	ndis_methods,
	sizeof(struct ndis_softc)
};

static devclass_t ndis_devclass;

DRIVER_MODULE(ndis, uhub, ndis_driver, ndis_devclass, ndisdrv_modevent, 0);

static int
ndisusb_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);

	if (windrv_lookup(0, "USB Bus") == NULL)
		return (UMATCH_NONE);

	if (uaa->iface != NULL)
		return (UMATCH_NONE);

	return (UMATCH_NONE);
}

static int
ndisusb_attach(device_t self)
{
	struct ndisusb_softc *dummy = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
	struct ndis_softc	*sc;
	driver_object		*drv;

	sc = (struct ndis_softc *)dummy;

	if (uaa->device == NULL)
		return ENXIO;

	sc->ndis_dev = self;

	/* Create PDO for this device instance */

	drv = windrv_lookup(0, "USB Bus");
	windrv_create_pdo(drv, self);

	if (ndis_attach(self) != 0)
		return ENXIO;

	return 0;
}

static struct resource_list *
ndis_get_resource_list(device_t dev, device_t child)
{
	struct ndis_softc       *sc;

	sc = device_get_softc(dev);
	return (BUS_GET_RESOURCE_LIST(device_get_parent(sc->ndis_dev), dev));
}
