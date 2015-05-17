/*	$NetBSD: ingenic_ehci.c,v 1.3 2015/03/17 09:27:09 macallan Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: ingenic_ehci.c,v 1.3 2015/03/17 09:27:09 macallan Exp $");

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

#include <dev/usb/ehcireg.h>
#include <dev/usb/ehcivar.h>

#include <dev/usb/usbdevs.h>

#include "opt_ingenic.h"
#include "ohci.h"

static int ingenic_ehci_match(device_t, struct cfdata *, void *);
static void ingenic_ehci_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(ingenic_ehci, sizeof(struct ehci_softc),
    ingenic_ehci_match, ingenic_ehci_attach, NULL, NULL);

#if NOHCI > 0
extern device_t ingenic_ohci;
#endif

/* ARGSUSED */
static int
ingenic_ehci_match(device_t parent, struct cfdata *match, void *aux)
{
	struct apbus_attach_args *aa = aux;

	if (strcmp(aa->aa_name, "ehci") != 0)
		return 0;

	return 1;
}

/* ARGSUSED */
static void
ingenic_ehci_attach(device_t parent, device_t self, void *aux)
{
	struct ehci_softc *sc = device_private(self);
	struct apbus_attach_args *aa = aux;
	void *ih;
	int error, status;
	uint32_t reg;

	sc->sc_dev = self;

	sc->iot = aa->aa_bst;
	sc->sc_bus.dmatag = aa->aa_dmat;
	sc->sc_bus.hci_private = sc;
	sc->sc_size = 0x1000;
	sc->sc_bus.usbrev = USBREV_2_0;

	if (aa->aa_addr == 0)
		aa->aa_addr = JZ_EHCI_BASE;

	error = bus_space_map(aa->aa_bst, aa->aa_addr, 0x1000, 0, &sc->ioh);
	if (error) {
		aprint_error_dev(self,
		    "can't map registers for %s: %d\n", aa->aa_name, error);
		return;
	}

	aprint_naive(": EHCI USB controller\n");
	aprint_normal(": EHCI USB controller\n");

	/*
	 * voodoo from the linux driver:
	 * select utmi data bus width of controller to 16bit
	 */
	reg = bus_space_read_4(sc->iot, sc->ioh,  0xb0);
	reg |= 1 << 6;
	bus_space_write_4(sc->iot, sc->ioh,  0xb0, reg);

	/* Disable EHCI interrupts */
	bus_space_write_4(sc->iot, sc->ioh, EHCI_USBINTR, 0);

	ih = evbmips_intr_establish(aa->aa_irq, ehci_intr, sc);
		
	if (ih == NULL) {
		aprint_error_dev(self, "failed to establish interrupt %d\n",
		     aa->aa_irq);
		goto fail;
	}

#if NOHCI > 0
	if (ingenic_ohci != NULL) {
		sc->sc_ncomp = 1;
		sc->sc_comps[0] = ingenic_ohci;
	} else
		sc->sc_ncomp = 0;
#else
	sc->sc_ncomp = 0;
#endif
	sc->sc_id_vendor = USB_VENDOR_INGENIC;
	strlcpy(sc->sc_vendor, "Ingenic", sizeof(sc->sc_vendor));

	status = ehci_init(sc);
	if (status != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(self, "init failed, error=%d\n", status);
		goto fail;
	}

	/* Attach USB device */
	sc->sc_child = config_found(self, &sc->sc_bus, usbctlprint);

	return;

fail:
	if (ih) {
		evbmips_intr_disestablish(ih);
	}
	bus_space_unmap(sc->iot, sc->ioh, 0x1000);
}
