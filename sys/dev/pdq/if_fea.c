/*-
 * Copyright (c) 1995, 1996 Matt Thomas <matt@3am-software.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/pdq/if_fea.c,v 1.19 2000/01/14 07:14:03 peter Exp $
 */

/*
 * DEC PDQ FDDI Controller
 *
 *	This module support the DEFEA EISA FDDI Controller.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_arp.h>

#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h> 
#include <dev/eisa/eisaconf.h>
#include <dev/pdq/pdqvar.h>
#include <dev/pdq/pdqreg.h>

static void		pdq_eisa_subprobe	__P((pdq_bus_t, u_int32_t, u_int32_t *, u_int32_t *, u_int32_t *));
static void		pdq_eisa_devinit	__P((pdq_softc_t *));
static const char *	pdq_eisa_match		__P((eisa_id_t));
static int 		pdq_eisa_probe		__P((device_t));
static int		pdq_eisa_attach		__P((device_t));
void			pdq_eisa_intr		__P((void *));
static int		pdq_eisa_shutdown	__P((device_t));

#define	DEFEA_IRQS			0x0000FBA9U

#define	DEFEA_INTRENABLE		0x8	/* level interrupt */
#define	DEFEA_DECODE_IRQ(n)		((DEFEA_IRQS >> ((n) << 2)) & 0x0f)

#define EISA_DEVICE_ID_DEC_DEC3001	0x10a33001
#define EISA_DEVICE_ID_DEC_DEC3002	0x10a33002
#define EISA_DEVICE_ID_DEC_DEC3003	0x10a33003
#define EISA_DEVICE_ID_DEC_DEC3004	0x10a33004

static void
pdq_eisa_subprobe(bc, iobase, maddr, msize, irq)
	pdq_bus_t	bc;
	u_int32_t	iobase;
	u_int32_t	*maddr;
	u_int32_t	*msize;
	u_int32_t	*irq;
{
	if (irq != NULL)
		*irq = DEFEA_DECODE_IRQ(PDQ_OS_IORD_8(bc, iobase, PDQ_EISA_IO_CONFIG_STAT_0) & 3);
	*maddr = (PDQ_OS_IORD_8(bc, iobase, PDQ_EISA_MEM_ADD_CMP_0) << 8)
		 | (PDQ_OS_IORD_8(bc, iobase, PDQ_EISA_MEM_ADD_CMP_1) << 16);
	*msize = (PDQ_OS_IORD_8(bc, iobase, PDQ_EISA_MEM_ADD_MASK_0) + 4) << 8;

	return;
}

static void
pdq_eisa_devinit (sc)
	pdq_softc_t	*sc;
{
	pdq_uint8_t	data;

	/*
	 * Do the standard initialization for the DEFEA registers.
	 */
	PDQ_OS_IOWR_8(sc->sc_bc, sc->sc_iobase, PDQ_EISA_FUNCTION_CTRL, 0x23);
	PDQ_OS_IOWR_8(sc->sc_bc, sc->sc_iobase, PDQ_EISA_IO_CMP_1_1, (sc->sc_iobase >> 8) & 0xF0);
	PDQ_OS_IOWR_8(sc->sc_bc, sc->sc_iobase, PDQ_EISA_IO_CMP_0_1, (sc->sc_iobase >> 8) & 0xF0);
	PDQ_OS_IOWR_8(sc->sc_bc, sc->sc_iobase, PDQ_EISA_SLOT_CTRL, 0x01);
	data = PDQ_OS_IORD_8(sc->sc_bc, sc->sc_iobase, PDQ_EISA_BURST_HOLDOFF);
#if defined(PDQ_IOMAPPED)
	PDQ_OS_IOWR_8(sc->sc_bc, sc->sc_iobase, PDQ_EISA_BURST_HOLDOFF, data & ~1);
#else
	PDQ_OS_IOWR_8(sc->sc_bc, sc->sc_iobase, PDQ_EISA_BURST_HOLDOFF, data | 1);
#endif
	data = PDQ_OS_IORD_8(sc->sc_bc, sc->sc_iobase, PDQ_EISA_IO_CONFIG_STAT_0);
	PDQ_OS_IOWR_8(sc->sc_bc, sc->sc_iobase, PDQ_EISA_IO_CONFIG_STAT_0, data | DEFEA_INTRENABLE);

	return;
}

static const char *
pdq_eisa_match (type)
	eisa_id_t	type;
{
	switch (type) {
		case EISA_DEVICE_ID_DEC_DEC3001:
		case EISA_DEVICE_ID_DEC_DEC3002:
		case EISA_DEVICE_ID_DEC_DEC3003:
		case EISA_DEVICE_ID_DEC_DEC3004:
			return ("DEC FDDIcontroller/EISA Adapter");
			break;
		 default:
			break;
	}
	return (NULL);
}

static int
pdq_eisa_probe (dev)
	device_t	dev;
{
	const char	*desc;
	u_int32_t	iobase;
	u_int32_t	irq;
	u_int32_t	maddr;
	u_int32_t	msize;

	u_int32_t	eisa_id = eisa_get_id(dev);;

	desc = pdq_eisa_match(eisa_id);
	if (!desc) {
		return (ENXIO);
	}

	device_set_desc(dev, desc);

	iobase = eisa_get_slot(dev) * EISA_SLOT_SIZE;
	pdq_eisa_subprobe(PDQ_BUS_EISA, iobase, &maddr, &msize, &irq);

	eisa_add_iospace(dev, iobase, 0x200, RESVADDR_NONE);
	eisa_add_mspace(dev, maddr, msize, RESVADDR_NONE);
	eisa_add_intr(dev, irq, EISA_TRIGGER_LEVEL);
	
	return (0);
}

void
pdq_eisa_intr(xdev)
	void		*xdev;
{
	device_t	dev = (device_t) xdev;
	pdq_softc_t	*sc = device_get_softc(dev);
	(void) pdq_interrupt(sc->sc_pdq);

	return;
}

static int
pdq_eisa_attach (dev)
	device_t		dev;
{
	pdq_softc_t		*sc = device_get_softc(dev);
	struct resource		*io = 0;
	struct resource		*irq = 0;
	struct resource		*mspace = 0;
	int			rid;
	void			*ih;
	u_int32_t		m_addr, m_size;

	rid = 0;
	io = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
				0, ~0, 1, RF_ACTIVE);

	if (!io) {
		device_printf(dev, "No I/O space?!\n");
		goto bad;
	}

	rid = 0;
	mspace = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
				    0, ~0, 1, RF_ACTIVE);

	if (!mspace) {
		device_printf(dev, "No memory space?!\n");
		goto bad;
	}

	rid = 0;
	irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
				 0, ~0, 1, RF_ACTIVE);

	if (!irq) {
		device_printf(dev, "No, irq?!\n");
		goto bad;
	}

	m_addr = rman_get_start(mspace);
 	m_size = (rman_get_end(mspace) - rman_get_start(mspace)) + 1;

	sc->sc_iobase = (pdq_bus_ioport_t) rman_get_start(io);
	sc->sc_membase = (pdq_bus_memaddr_t) pmap_mapdev(m_addr, m_size);
	sc->sc_if.if_name = "fea";
	sc->sc_if.if_unit = device_get_unit(dev);

	pdq_eisa_devinit(sc);
	sc->sc_pdq = pdq_initialize(PDQ_BUS_EISA, sc->sc_membase,
				    sc->sc_if.if_name, sc->sc_if.if_unit,
				    (void *) sc, PDQ_DEFEA);
	if (sc->sc_pdq == NULL) {
		device_printf(dev, "initialization failed\n");
		goto bad;
	}

	if (bus_setup_intr(dev, irq, INTR_TYPE_NET, pdq_eisa_intr, dev, &ih)) {
		goto bad;
	}

	bcopy((caddr_t) sc->sc_pdq->pdq_hwaddr.lanaddr_bytes, sc->sc_ac.ac_enaddr, 6);
	pdq_ifattach(sc, NULL);

	return (0);

bad:
	if (io)
		bus_release_resource(dev, SYS_RES_IOPORT, 0, io);
	if (irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, irq);
	if (mspace)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, mspace);

	return (-1);
}

static int
pdq_eisa_shutdown(dev)
	device_t	dev;
{
	pdq_softc_t	*sc = device_get_softc(dev);

	pdq_hwreset(sc->sc_pdq);

	return (0);
}

static device_method_t pdq_eisa_methods[] = {
	DEVMETHOD(device_probe,		pdq_eisa_probe),
	DEVMETHOD(device_attach,	pdq_eisa_attach),
	DEVMETHOD(device_shutdown,      pdq_eisa_shutdown),

	{ 0, 0 }
};

static driver_t pdq_eisa_driver = {
	"fea",
	pdq_eisa_methods,
	sizeof(pdq_softc_t),
};

static devclass_t pdq_devclass;

DRIVER_MODULE(pdq, eisa, pdq_eisa_driver, pdq_devclass, 0, 0);
