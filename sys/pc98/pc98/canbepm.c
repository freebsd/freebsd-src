/*-
 * Copyright (c) 2000 KIYOHARA Takashi <kiyohara@kk.iij4u.ne.jp>
 * Copyright (c) 2000 Takanori Watanabe <takawata@jp.FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/pc98/pc98/canbepm.c,v 1.3.32.1 2008/11/25 02:59:29 kensmith Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/reboot.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <pc98/pc98/canbusvars.h>
#include "canbus_if.h"


/* canbepm softc */
struct canbepm_softc {
	device_t canbepm_dev;			/* canbepm device */

	eventhandler_tag canbepm_tag;		/* event handler tag */
};


static void	canbepm_soft_off (void *, int);
static void	canbepm_identify (driver_t *, device_t);
static int	canbepm_probe (device_t);
static int	canbepm_attach (device_t);
static int	canbepm_detach (device_t);


static device_method_t canbepm_methods[] = { 
	DEVMETHOD(device_identify,	canbepm_identify),
	DEVMETHOD(device_probe,		canbepm_probe),
	DEVMETHOD(device_attach,	canbepm_attach),
	DEVMETHOD(device_detach,	canbepm_detach),
	{0, 0}
};

static driver_t canbepm_driver = {
	"canbepm",
	canbepm_methods,
	sizeof(struct canbepm_softc),
};

devclass_t canbepm_devclass;
DRIVER_MODULE(canbepm, canbus, canbepm_driver, canbepm_devclass, 0, 0);
MODULE_DEPEND(canbepm, canbus, 1, 1, 1);


static void
canbepm_soft_off (void *data, int howto)
{
	struct canbepm_softc *sc = data;
	u_int8_t poweroff_data[] = CANBE_POWEROFF_DATA;

	if (!(howto & RB_POWEROFF))
		return;

	CANBUS_WRITE_MULTI(device_get_parent(sc->canbepm_dev), sc->canbepm_dev,
	    CANBE_POWER_CTRL, sizeof (poweroff_data), poweroff_data);
}


static void
canbepm_identify(driver_t *drv, device_t parent)
{
	if (device_find_child(parent, "canbepm", 0) == NULL) {
		if (BUS_ADD_CHILD(parent, 33, "canbepm", 0) == NULL)
			device_printf(parent, "canbepm cannot attach\n");
	}
}


static int
canbepm_probe(device_t dev)
{
	device_set_desc(dev, "CanBe Power Management Controller");

	return (0);	
}

static int
canbepm_attach(device_t dev)
{
	struct canbepm_softc *sc = device_get_softc(dev);

	/* eventhandler regist */
	sc->canbepm_tag = EVENTHANDLER_REGISTER(
	    shutdown_final, canbepm_soft_off, sc, SHUTDOWN_PRI_LAST);

	sc->canbepm_dev = dev;

	return (0);
}


static int
canbepm_detach(device_t dev)
{
	struct canbepm_softc *sc = device_get_softc(dev);

	/* eventhandler deregist */
	EVENTHANDLER_DEREGISTER(shutdown_final, sc->canbepm_tag);
	BUS_CHILD_DETACHED(device_get_parent(dev), dev);

	return (0);
}
