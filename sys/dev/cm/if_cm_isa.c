/*	$NetBSD: if_bah_zbus.c,v 1.6 2000/01/23 21:06:12 aymeric Exp $ */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/cm/if_cm_isa.c,v 1.10.6.1 2008/11/25 02:59:29 kensmith Exp $");

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
#include <sys/rman.h>
#include <machine/resource.h>

#include <net/if.h>
#include <net/if_arc.h>

#include <dev/cm/smc90cx6reg.h>
#include <dev/cm/smc90cx6var.h>

static int cm_isa_probe		(device_t);
static int cm_isa_attach	(device_t);

static int
cm_isa_probe(dev)
	device_t dev;
{
	struct cm_softc *sc = device_get_softc(dev);
	int rid;

	rid = 0;
	sc->port_res = bus_alloc_resource(
	    dev, SYS_RES_IOPORT, &rid, 0ul, ~0ul, CM_IO_PORTS, RF_ACTIVE);
	if (sc->port_res == NULL)
		return (ENOENT);

	if (GETREG(CMSTAT) == 0xff) {
		cm_release_resources(dev);
		return (ENXIO);
	}

	rid = 0;
	sc->mem_res = bus_alloc_resource(
	    dev, SYS_RES_MEMORY, &rid, 0ul, ~0ul, CM_MEM_SIZE, RF_ACTIVE);
	if (sc->mem_res == NULL) {
		cm_release_resources(dev);
		return (ENOENT);
	}

	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (sc->irq_res == NULL) {
		cm_release_resources(dev);
		return (ENOENT);
	}

	return (0);
}

static int
cm_isa_attach(dev)
	device_t dev;
{
	struct cm_softc *sc = device_get_softc(dev);
	int error;

	/* init mtx and setup interrupt */
	mtx_init(&sc->sc_mtx, device_get_nameunit(dev),
	    MTX_NETWORK_LOCK, MTX_DEF);
	error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, cmintr, sc, &sc->irq_handle);
	if (error)
		goto err;

	/* attach */
	error = cm_attach(dev);
	if (error)
		goto err;

	return 0;

err:
	mtx_destroy(&sc->sc_mtx);
	cm_release_resources(dev);
	return (error);
}

static int
cm_isa_detach(device_t dev)
{
	struct cm_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = sc->sc_ifp;

	/* stop and detach */
	CM_LOCK(sc);
	cm_stop_locked(sc);
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	CM_UNLOCK(sc);

	callout_drain(&sc->sc_recon_ch);
	arc_ifdetach(ifp);

	/* teardown interrupt, destroy mtx and release resources */
	bus_teardown_intr(dev, sc->irq_res, sc->irq_handle);
	mtx_destroy(&sc->sc_mtx);
	if_free(ifp);
	cm_release_resources(dev);

	bus_generic_detach(dev);
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
