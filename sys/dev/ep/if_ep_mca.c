/*-
 * Copyright (c) 1999 Matthew N. Dodd <winter@jurai.net>
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
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <vm/vm.h>
#include <vm/vm_param.h> 
#include <vm/pmap.h>

#include <machine/clock.h>
#include <machine/cpufunc.h>
#include <machine/md_var.h>

#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
 
#include <net/if.h> 
#include <net/if_mib.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <dev/mca/mca_busreg.h>
#include <dev/mca/mca_busvar.h>

#include <dev/ep/if_epreg.h>
#include <dev/ep/if_epvar.h>

#define EP_MCA_627C	0x627C
#define EP_MCA_627D	0x627D
#define EP_MCA_62DB	0x62db
#define EP_MCA_62F6	0x62f6
#define EP_MCA_62F7	0x62f7

static struct mca_ident ep_mca_devs[] = {
	{ EP_MCA_627C, "3Com 3C529 Network Adapter" },
	{ EP_MCA_627D, "3Com 3C529-TP Network Adapter" },

	/*
	 * These are from the linux 3c509 driver.
	 * I have not seen the ADFs for them and have 
	 * not tested or even seen the hardware.
 	 * Someone with the ADFs should replace the names with
	 * whatever is in the AdapterName field of the ADF.
	 * (and fix the media setup for the cards as well.)
	 */
	{ EP_MCA_62DB, "3Com 3c529 EtherLink III (test mode)" },
	{ EP_MCA_62F6, "3Com 3c529 EtherLink III (TP or coax)" },
	{ EP_MCA_62F7, "3Com 3c529 EtherLink III (TP)" },

	{ 0, NULL },
};

#define EP_MCA_IOPORT_POS	MCA_ADP_POS(MCA_POS2)
#define EP_MCA_IOPORT_MASK	0xfc
#define EP_MCA_IOPORT_SIZE	0x0f
#define EP_MCA_IOPORT(pos)	((((u_int32_t)pos & EP_MCA_IOPORT_MASK) \
					| 0x02) << 8)

#define EP_MCA_IRQ_POS		MCA_ADP_POS(MCA_POS3)
#define EP_MCA_IRQ_MASK		0x0f
#define EP_MCA_IRQ(pos)		(pos & EP_MCA_IRQ_MASK)

#define EP_MCA_MEDIA_POS	MCA_ADP_POS(MCA_POS2)
#define EP_MCA_MEDIA_MASK	0x03
#define EP_MCA_MEDIA(pos)	(pos & EP_MCA_MEDIA_MASK)

static int
ep_mca_probe (device_t dev)
{
	const char *	desc;
	u_int32_t       iobase = 0;
	u_int8_t	irq = 0;
	u_int8_t	pos;

	desc = mca_match_id(mca_get_id(dev), ep_mca_devs);
	if (!desc)
		return (ENXIO);
	device_set_desc(dev, desc);

	pos = mca_pos_read(dev, EP_MCA_IOPORT_POS);
	iobase = EP_MCA_IOPORT(pos);

	pos = mca_pos_read(dev, EP_MCA_IRQ_POS);
	irq = EP_MCA_IRQ(pos);

	mca_add_iospace(dev, iobase, EP_MCA_IOPORT_SIZE);
	mca_add_irq(dev, irq);

	return (0);
}

static int
ep_mca_attach (device_t dev)
{
	struct ep_softc *	sc;
	struct ep_board *	epb;
	struct resource *	io = 0;
	struct resource *	irq = 0;
	u_int8_t		pos;
	int			unit = device_get_unit(dev);
	int			i, rid;
	void *			ih;

	rid = 0;
	io = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
				0, ~0, 1, RF_ACTIVE);
	if (!io) {
		device_printf(dev, "No I/O space?!\n");
		goto bad;
	}

	rid = 0;
	irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
				 0, ~0, 1, RF_ACTIVE);
	if (!irq) {
		device_printf(dev, "No irq?!\n");
		goto bad;
	}

	epb = &ep_board[ep_boards];

	epb->epb_addr = rman_get_start(io);
	epb->epb_used = 1;

	if(!(sc = ep_alloc(unit, epb)))
		goto bad;

	ep_boards++;

	sc->stat = F_ACCESS_32_BITS;

	switch(mca_get_id(dev)) {
		case EP_MCA_627C:
			sc->ep_connectors = BNC|AUI;
			break;
		case EP_MCA_627D:
			sc->ep_connectors = UTP|AUI;
			break;
		default:
			break;
	}
	pos = mca_pos_read(dev, EP_MCA_MEDIA_POS);
	sc->ep_connector = EP_MCA_MEDIA(pos);

	/*
	 * Retrieve our ethernet address
	 */
	GO_WINDOW(0);
	for(i = 0; i < 3; i++)
		sc->epb->eth_addr[i] = get_e(sc, i);

	GO_WINDOW(0);
	SET_IRQ(BASE, rman_get_start(irq));

	ep_attach(sc);

	bus_setup_intr(dev, irq, INTR_TYPE_NET, ep_intr, sc, &ih);

	return (0);

bad:
	if (io)
		bus_release_resource(dev, SYS_RES_IOPORT, 0, io);
	if (irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, irq);

	return (-1);
}

static device_method_t ep_mca_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ep_mca_probe),
	DEVMETHOD(device_attach,	ep_mca_attach),

	{ 0, 0 }
};

static driver_t ep_mca_driver = {
	"ep",
	ep_mca_methods,
	1,			      /* unusep */
};

static devclass_t ep_devclass;

DRIVER_MODULE(ep, mca, ep_mca_driver, ep_devclass, 0, 0);
