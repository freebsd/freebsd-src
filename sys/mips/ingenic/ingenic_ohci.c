/*	$NetBSD: ingenic_ohci.c,v 1.3 2015/03/17 09:27:09 macallan Exp $ */

/*-
 * Copyright (c) 2015 Michael Lorenz
 * All rights reserved.
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
__KERNEL_RCSID(0, "$NetBSD: ingenic_ohci.c,v 1.3 2015/03/17 09:27:09 macallan Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/workqueue.h>

#include <mips/ingenic/ingenic_var.h>
#include <mips/ingenic/ingenic_regs.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/ohcireg.h>
#include <dev/usb/ohcivar.h>

#include "opt_ingenic.h"

static int ingenic_ohci_match(device_t, struct cfdata *, void *);
static void ingenic_ohci_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(ingenic_ohci, sizeof(struct ohci_softc),
    ingenic_ohci_match, ingenic_ohci_attach, NULL, NULL);

device_t ingenic_ohci = NULL;

/* ARGSUSED */
static int
ingenic_ohci_match(device_t parent, struct cfdata *match, void *aux)
{
	struct apbus_attach_args *aa = aux;

	if (strcmp(aa->aa_name, "ohci") != 0)
		return 0;

	return 1;
}

/* ARGSUSED */
static void
ingenic_ohci_attach(device_t parent, device_t self, void *aux)
{
	struct ohci_softc *sc = device_private(self);
	struct apbus_attach_args *aa = aux;
	void *ih;
	int error, status;

	sc->sc_dev = self;

	sc->iot = aa->aa_bst;
	sc->sc_bus.dmatag = aa->aa_dmat;
	sc->sc_bus.hci_private = sc;
	sc->sc_size = 0x1000;

	if (aa->aa_addr == 0)
		aa->aa_addr = JZ_OHCI_BASE;

	error = bus_space_map(aa->aa_bst, aa->aa_addr, 0x1000, 0, &sc->ioh);
	if (error) {
		aprint_error_dev(self,
		    "can't map registers for %s: %d\n", aa->aa_name, error);
		return;
	}

	aprint_naive(": OHCI USB controller\n");
	aprint_normal(": OHCI USB controller\n");

	/* Disable OHCI interrupts */
	bus_space_write_4(sc->iot, sc->ioh, OHCI_INTERRUPT_DISABLE,
	    OHCI_ALL_INTRS);

	ih = evbmips_intr_establish(aa->aa_irq, ohci_intr, sc);

	if (ih == NULL) {
		aprint_error_dev(self, "failed to establish interrupt %d\n",
		     aa->aa_irq);
		goto fail;
	}

	sc->sc_endian = OHCI_LITTLE_ENDIAN;

	sc->sc_id_vendor = USB_VENDOR_INGENIC;
	strlcpy(sc->sc_vendor, "Ingenic", sizeof(sc->sc_vendor));

	status = ohci_init(sc);
	if (status != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(self, "init failed, error=%d\n", status);
		goto fail;
	}

	/* Attach USB device */
	sc->sc_child = config_found(self, &sc->sc_bus, usbctlprint);
	ingenic_ohci = self;
	return;

fail:
	if (ih) {
		evbmips_intr_disestablish(ih);
	}
	bus_space_unmap(sc->iot, sc->ioh, 0x1000);
}
