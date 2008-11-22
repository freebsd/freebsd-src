/*	$NetBSD: if_bah_zbus.c,v 1.6 2000/01/23 21:06:12 aymeric Exp $ */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*-
 * Copyright (c) 1994, 1995, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ignatios Souvatzis.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/kernel.h>

#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_arc.h>

#include <dev/cm/smc90cx6var.h>

static int cm_isa_probe		(device_t);
static int cm_isa_attach	(device_t);

static int
cm_isa_probe(dev)
	device_t dev;
{
	struct cm_softc *sc = device_get_softc(dev);
	int error;

	bzero(sc, sizeof(struct cm_softc));

	error = cm_probe(dev);
	if (error == 0)
		goto end;

end:
	if (error == 0)
		error = cm_alloc_irq(dev, 0);

	cm_release_resources(dev);
	return (error);
}

static int
cm_isa_attach(dev)
	device_t dev;
{
	struct cm_softc *sc = device_get_softc(dev);
	int error;

	cm_alloc_port(dev, sc->port_rid, sc->port_used);
	cm_alloc_memory(dev, sc->mem_rid, sc->mem_used);
	cm_alloc_irq(dev, sc->irq_rid);

	error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET,
			       cmintr, sc, &sc->irq_handle);
	if (error) {
		cm_release_resources(dev);
		return (error);
	}

	return cm_attach(dev);
}

static int
cm_isa_detach(device_t dev)
{
	struct cm_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = sc->sc_ifp;
	int s;

	cm_stop(sc);
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;

	s = splimp();
	arc_ifdetach(ifp);
	if_free(ifp);
	splx(s);

	bus_teardown_intr(dev, sc->irq_res, sc->irq_handle);
	cm_release_resources(dev);

	return (0);
}

static device_method_t cm_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		cm_isa_probe),
	DEVMETHOD(device_attach,	cm_isa_attach),
	DEVMETHOD(device_detach,	cm_isa_detach),

	{ 0, 0 }
};

static driver_t cm_isa_driver = {
	"cm",
	cm_isa_methods,
	sizeof(struct cm_softc)
};

DRIVER_MODULE(cm, isa, cm_isa_driver, cm_devclass, 0, 0);
MODULE_DEPEND(cm, isa, 1, 1, 1);
