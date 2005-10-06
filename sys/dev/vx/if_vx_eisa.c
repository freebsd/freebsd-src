/*-
 * Copyright (C) 1996 Naoki Hamada <nao@tom-yam.or.jp>
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>

#include <dev/eisa/eisaconf.h>

#include <dev/vx/if_vxreg.h>
#include <dev/vx/if_vxvar.h>

#define EISA_DEVICE_ID_3COM_3C592	0x506d5920
#define EISA_DEVICE_ID_3COM_3C597_TX	0x506d5970
#define EISA_DEVICE_ID_3COM_3C597_T4	0x506d5971
#define EISA_DEVICE_ID_3COM_3C597_MII	0x506d5972


#define	VX_EISA_SLOT_OFFSET		0x0c80
#define	VX_EISA_IOSIZE			0x000a
#define VX_RESOURCE_CONFIG		0x0008


static const char *vx_match(eisa_id_t type);

static const char *
vx_match(eisa_id_t type)
{
	switch (type) {
	case EISA_DEVICE_ID_3COM_3C592:
		return "3Com 3C592 Network Adapter";
	case EISA_DEVICE_ID_3COM_3C597_TX:
		return "3Com 3C597-TX Network Adapter";
	case EISA_DEVICE_ID_3COM_3C597_T4:
		return "3Com 3C597-T4 Network Adapter";
	case EISA_DEVICE_ID_3COM_3C597_MII:
		return "3Com 3C597-MII Network Adapter";
	default:
		break;
	}
	return (NULL);
}

static int
vx_eisa_probe(device_t dev)
{
	const char *desc;
	u_long iobase;
	u_long port;

	desc = vx_match(eisa_get_id(dev));
	if (!desc)
		return (ENXIO);
	device_set_desc(dev, desc);

	port = eisa_get_slot(dev) * EISA_SLOT_SIZE;
	iobase = port + VX_EISA_SLOT_OFFSET;

	eisa_add_iospace(dev, iobase, VX_EISA_IOSIZE, RESVADDR_NONE);
	eisa_add_iospace(dev, port, VX_IOSIZE, RESVADDR_NONE);

	/* Set irq */
	eisa_add_intr(dev, inw(iobase + VX_RESOURCE_CONFIG) >> 12,
	    EISA_TRIGGER_EDGE);

	return (0);
}

static int
vx_eisa_attach(device_t dev)
{
	struct vx_softc *sc;
	struct resource *io = 0;
	struct resource *eisa_io = 0;
	struct resource *irq = 0;
	int rid;

	/*
         * The addresses are sorted in increasing order
         * so we know the port to pass to the core ep
         * driver comes first.
         */
	rid = 0;
	io = bus_alloc_resource_any(dev, SYS_RES_IOPORT, &rid, RF_ACTIVE);
	if (!io) {
		device_printf(dev, "No I/O space?!\n");
		goto bad;
	}
	rid = 1;
	eisa_io = bus_alloc_resource_any(dev, SYS_RES_IOPORT, &rid, RF_ACTIVE);
	if (!eisa_io) {
		device_printf(dev, "No I/O space?!\n");
		goto bad;
	}
	sc = device_get_softc(dev);

	sc->vx_res = io;
	sc->vx_bst = rman_get_bustag(io);
	sc->vx_bsh = rman_get_bushandle(io);

	rid = 0;
	irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (!irq) {
		device_printf(dev, "No irq?!\n");
		goto bad;
	}
	sc->vx_irq = irq;

	/* Now the registers are availible through the lower ioport */

	if (vx_attach(dev) == 0)
		goto bad;

	if (bus_setup_intr(dev, irq, INTR_TYPE_NET | INTR_MPSAFE, vx_intr, sc,
		&sc->vx_intrhand))
		goto bad_mtx;

	return (0);

bad_mtx:
	mtx_destroy(&sc->vx_mtx);
	ether_ifdetach(sc->vx_ifp);
	if_free(sc->vx_ifp);
bad:
	if (io)
		bus_release_resource(dev, SYS_RES_IOPORT, 0, io);
	if (eisa_io)
		bus_release_resource(dev, SYS_RES_IOPORT, 0, eisa_io);
	if (irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, irq);
	return (ENXIO);
}

static device_method_t vx_eisa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, vx_eisa_probe),
	DEVMETHOD(device_attach, vx_eisa_attach),

	{0, 0}
};

static driver_t vx_eisa_driver = {
	"vx",
	vx_eisa_methods,
	sizeof(struct vx_softc)
};

static devclass_t vx_devclass;

DRIVER_MODULE(vx, eisa, vx_eisa_driver, vx_devclass, 0, 0);
