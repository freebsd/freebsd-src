/*	$NetBSD: ingenic_dme.c,v 1.1 2015/03/10 18:15:47 macallan Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: ingenic_dme.c,v 1.1 2015/03/10 18:15:47 macallan Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/workqueue.h>

#include <mips/ingenic/ingenic_var.h>
#include <mips/ingenic/ingenic_regs.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <dev/ic/dm9000var.h>
#include <dev/ic/dm9000reg.h>

#include "opt_ingenic.h"

static int ingenic_dme_match(device_t, struct cfdata *, void *);
static void ingenic_dme_attach(device_t, device_t, void *);
static int ingenic_dme_intr(void *);

CFATTACH_DECL_NEW(ingenic_dme, sizeof(struct dme_softc),
    ingenic_dme_match, ingenic_dme_attach, NULL, NULL);

#define GPIO_DME_INT		19
#define GPIO_DME_INT_MASK	(1 << GPIO_DME_INT)

/* ARGSUSED */
static int
ingenic_dme_match(device_t parent, struct cfdata *match, void *aux)
{
	struct apbus_attach_args *aa = aux;

	if (strcmp(aa->aa_name, "dme") != 0)
		return 0;

	return 1;
}

/* ARGSUSED */
static void
ingenic_dme_attach(device_t parent, device_t self, void *aux)
{
	struct dme_softc *sc = device_private(self);
	struct apbus_attach_args *aa = aux;
	void *ih;
	static uint8_t enaddr[ETHER_ADDR_LEN];
	int error;

	sc->sc_dev = self;

	sc->sc_iot = aa->aa_bst;
	sc->dme_io = JZ_DME_IO;
	sc->dme_data = JZ_DME_DATA;
	sc->sc_phy_initialized = 0;

	if (aa->aa_addr == 0)
		aa->aa_addr = JZ_DME_BASE;

	error = bus_space_map(aa->aa_bst, aa->aa_addr, 4, 0, &sc->sc_ioh);
	if (error) {
		aprint_error_dev(self,
		    "can't map registers for %s: %d\n", aa->aa_name, error);
		return;
	}

	aprint_naive(": DM9000 Ethernet controller\n");
	aprint_normal(": DM9000 Ethernet controller\n");


	/* make sure the chip is powered up and not in reset */
	gpio_as_output(1, 25);
	gpio_set(1, 25, 1);
	gpio_as_output(5, 12);
	gpio_set(5, 12, 1);

	/* setup pins to talk to the chip */
	gpio_as_dev0(1, 1);
	gpio_as_dev0(0, 0);
	gpio_as_dev0(0, 1);
	gpio_as_dev0(0, 2);
	gpio_as_dev0(0, 3);
	gpio_as_dev0(0, 4);
	gpio_as_dev0(0, 5);
	gpio_as_dev0(0, 6);
	gpio_as_dev0(0, 7);

	gpio_as_dev0(0, 16);
	gpio_as_dev0(0, 17);
	gpio_as_dev0(0, 26);

	/* DM9000 interrupt is on GPIO E pin 19 */
	gpio_as_intr_level(4, GPIO_DME_INT);
	ih = evbmips_intr_establish(13, ingenic_dme_intr, sc);

		if (ih == NULL) {
		aprint_error_dev(self, "failed to establish interrupt %d\n",
		     13);
		goto fail;
	}

	/*
	 * XXX grab MAC address set by uboot
	 * I'm not sure uboot will program the MAC address into the chip when
	 * not netbooting, so this needs to go away
	 */
	dme_read_c(sc, DM9000_PAB0, enaddr, 6);
	dme_attach(sc, enaddr);
	return;
fail:
	if (ih) {
		evbmips_intr_disestablish(ih);
	}
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, 4);
}

static int
ingenic_dme_intr(void *arg)
{
	uint32_t reg;	
	int ret = 0;

	/* see if it's us */
	reg = readreg(JZ_GPIO_E_BASE + JZ_GPIO_FLAG);
	if (reg & GPIO_DME_INT_MASK) {
		/* yes, it's ours, handle it... */
		ret = dme_intr(arg);
		/* ... and clear it */
		writereg(JZ_GPIO_E_BASE + JZ_GPIO_FLAGC, GPIO_DME_INT_MASK);
	}
	return ret;
}
