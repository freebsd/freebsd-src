/*-
 * Copyright (c) 1999 Kazutaka YOKOTA <yokota@zodiac.mech.utsunomiya-u.ac.jp>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 */

#include "atkbd.h"
#include "opt_kbd.h"

#if NATKBD > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <machine/resource.h>

#include <dev/kbd/kbdreg.h>
#include <dev/kbd/atkbdreg.h>
#include <dev/kbd/atkbdcreg.h>

#include <isa/isareg.h>
#include <isa/isavar.h>

devclass_t	atkbd_devclass;

static int	atkbdprobe(device_t dev);
static int	atkbdattach(device_t dev);
static void	atkbd_isa_intr(void *arg);

static device_method_t atkbd_methods[] = {
	DEVMETHOD(device_probe,		atkbdprobe),
	DEVMETHOD(device_attach,	atkbdattach),
	{ 0, 0 }
};

static driver_t atkbd_driver = {
	ATKBD_DRIVER_NAME,
	atkbd_methods,
	DRIVER_TYPE_TTY,
	sizeof(atkbd_softc_t),
};

static int
atkbdprobe(device_t dev)
{
	u_long port;
	u_long irq;
	u_long flags;

	device_set_desc(dev, "AT Keyboard");

	/* obtain parameters */
	BUS_READ_IVAR(device_get_parent(dev), dev, KBDC_IVAR_PORT, &port);
	BUS_READ_IVAR(device_get_parent(dev), dev, KBDC_IVAR_IRQ, &irq);
	BUS_READ_IVAR(device_get_parent(dev), dev, KBDC_IVAR_FLAGS, &flags);

	/* probe the device */
	return atkbd_probe_unit(device_get_unit(dev), port, irq, flags);
}

static int
atkbdattach(device_t dev)
{
	atkbd_softc_t *sc;
	u_long port;
	u_long irq;
	u_long flags;
	struct resource *res;
	void *ih;
	int zero = 0;
	int error;

	sc = (atkbd_softc_t *)device_get_softc(dev);

	BUS_READ_IVAR(device_get_parent(dev), dev, KBDC_IVAR_PORT, &port);
	BUS_READ_IVAR(device_get_parent(dev), dev, KBDC_IVAR_IRQ, &irq);
	BUS_READ_IVAR(device_get_parent(dev), dev, KBDC_IVAR_FLAGS, &flags);

	error = atkbd_attach_unit(device_get_unit(dev), sc, port, irq, flags);
	if (error)
		return error;

	/* declare our interrupt handler */
	res = bus_alloc_resource(dev, SYS_RES_IRQ, &zero, irq, irq, 1,
				 RF_SHAREABLE | RF_ACTIVE);
	BUS_SETUP_INTR(device_get_parent(dev), dev, res, atkbd_isa_intr, sc,
		       &ih);

	return 0;
}

static void
atkbd_isa_intr(void *arg)
{
	atkbd_softc_t *sc;

	sc = (atkbd_softc_t *)arg;
	(*kbdsw[sc->kbd->kb_index]->intr)(sc->kbd, NULL);
}

DRIVER_MODULE(atkbd, atkbdc, atkbd_driver, atkbd_devclass, 0, 0);

#endif /* NATKBD > 0 */
