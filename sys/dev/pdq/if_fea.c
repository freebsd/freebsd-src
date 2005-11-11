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
 *    derived from this software without specific prior written permission
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
 * $FreeBSD$
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

#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h> 

#include <net/if.h>
#include <net/if_media.h>
#include <net/fddi.h>

#include <dev/eisa/eisaconf.h>

#include <dev/pdq/pdq_freebsd.h>
#include <dev/pdq/pdqreg.h>

static void		pdq_eisa_subprobe	(pdq_bus_t, u_int32_t, u_int32_t *, u_int32_t *, u_int32_t *);
static void		pdq_eisa_devinit	(pdq_softc_t *);
static const char *	pdq_eisa_match		(eisa_id_t);

static int 		pdq_eisa_probe		(device_t);
static int		pdq_eisa_attach		(device_t);
static int		pdq_eisa_detach		(device_t);
static int		pdq_eisa_shutdown	(device_t);
static void		pdq_eisa_ifintr		(void *);

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
	PDQ_OS_IOWR_8(sc->io_bst, sc->io_bsh, PDQ_EISA_FUNCTION_CTRL, 0x23);
	PDQ_OS_IOWR_8(sc->io_bst, sc->io_bsh, PDQ_EISA_IO_CMP_1_1, (sc->io_bsh >> 8) & 0xF0);
	PDQ_OS_IOWR_8(sc->io_bst, sc->io_bsh, PDQ_EISA_IO_CMP_0_1, (sc->io_bsh >> 8) & 0xF0);
	PDQ_OS_IOWR_8(sc->io_bst, sc->io_bsh, PDQ_EISA_SLOT_CTRL, 0x01);
	data = PDQ_OS_IORD_8(sc->io_bst, sc->io_bsh, PDQ_EISA_BURST_HOLDOFF);
#if defined(PDQ_IOMAPPED)
	PDQ_OS_IOWR_8(sc->io_bst, sc->io_bsh, PDQ_EISA_BURST_HOLDOFF, data & ~1);
#else
	PDQ_OS_IOWR_8(sc->io_bst, sc->io_bsh, PDQ_EISA_BURST_HOLDOFF, data | 1);
#endif
	data = PDQ_OS_IORD_8(sc->io_bst, sc->io_bsh, PDQ_EISA_IO_CONFIG_STAT_0);
	PDQ_OS_IOWR_8(sc->io_bst, sc->io_bsh, PDQ_EISA_IO_CONFIG_STAT_0, data | DEFEA_INTRENABLE);

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
	pdq_eisa_subprobe((pdq_bus_t)SYS_RES_IOPORT, iobase, &maddr, &msize, &irq);

	eisa_add_iospace(dev, iobase, 0x200, RESVADDR_NONE);
	eisa_add_mspace(dev, maddr, msize, RESVADDR_NONE);
	eisa_add_intr(dev, irq, EISA_TRIGGER_LEVEL);
	
	return (0);
}

static void
pdq_eisa_ifintr(arg)
	void *		arg;
{
	device_t	dev;
	pdq_softc_t *	sc;

	dev = (device_t)arg;
	sc = device_get_softc(dev);

	PDQ_LOCK(sc);
	(void) pdq_interrupt(sc->sc_pdq);
	PDQ_LOCK(sc);

	return;
}

static int
pdq_eisa_attach (dev)
	device_t	dev;
{
	pdq_softc_t *	sc;
	struct ifnet *	ifp;
	int		error;

	sc = device_get_softc(dev);
	ifp = sc->ifp;

	sc->dev = dev;

	sc->io_rid = 0;
	sc->io_type = SYS_RES_IOPORT;
	sc->io = bus_alloc_resource_any(dev, sc->io_type, &sc->io_rid,
					RF_ACTIVE);
	if (!sc->io) {
		device_printf(dev, "Unable to allocate I/O space resource.\n");
		error = ENXIO;
		goto bad;
	}
	sc->io_bsh = rman_get_bushandle(sc->io);
	sc->io_bst = rman_get_bustag(sc->io);

	sc->mem_rid = 0;
	sc->mem_type = SYS_RES_MEMORY;
	sc->mem = bus_alloc_resource_any(dev, sc->mem_type, &sc->mem_rid,
					 RF_ACTIVE);
	if (!sc->mem) {
		device_printf(dev, "Unable to allocate memory resource.\n");
		error = ENXIO;
		goto bad;
	}
	sc->mem_bsh = rman_get_bushandle(sc->mem);
	sc->mem_bst = rman_get_bustag(sc->mem);

	sc->irq_rid = 0;
	sc->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->irq_rid,
					 RF_SHAREABLE | RF_ACTIVE);
	if (!sc->irq) {
		device_printf(dev, "Unable to allocate interrupt resource.\n");
		error = ENXIO;
		goto bad;
	}

	if_initname(ifp, device_get_name(dev), device_get_unit(dev));

	pdq_eisa_devinit(sc);
	sc->sc_pdq = pdq_initialize(sc->mem_bst, sc->mem_bsh,
				    ifp->if_xname, -1,
				    (void *)sc, PDQ_DEFEA);
	if (sc->sc_pdq == NULL) {
		device_printf(dev, "Initialization failed.\n");
		error = ENXIO;
		goto bad;
	}

	error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET,
		               pdq_eisa_ifintr, dev, &sc->irq_ih);
	if (error) {
		device_printf(dev, "Failed to setup interrupt handler.\n");
		error = ENXIO;
		goto bad;
	}

	pdq_ifattach(sc, sc->sc_pdq->pdq_hwaddr.lanaddr_bytes);

	return (0);
bad:
	pdq_free(dev);
	return (error);
}

static int
pdq_eisa_detach (dev)
	device_t	dev;
{
	pdq_softc_t *	sc;

	sc = device_get_softc(dev);
	pdq_ifdetach(sc);

	return (0);
}

static int
pdq_eisa_shutdown(dev)
	device_t	dev;
{
	pdq_softc_t *	sc;

	sc = device_get_softc(dev);
	pdq_hwreset(sc->sc_pdq);

	return (0);
}

static device_method_t pdq_eisa_methods[] = {
	DEVMETHOD(device_probe,		pdq_eisa_probe),
	DEVMETHOD(device_attach,	pdq_eisa_attach),
	DEVMETHOD(device_attach,	pdq_eisa_detach),
	DEVMETHOD(device_shutdown,      pdq_eisa_shutdown),

	{ 0, 0 }
};

static driver_t pdq_eisa_driver = {
	"fea",
	pdq_eisa_methods,
	sizeof(pdq_softc_t),
};

DRIVER_MODULE(fea, eisa, pdq_eisa_driver, pdq_devclass, 0, 0);
/* MODULE_DEPEND(fea, eisa, 1, 1, 1); */
MODULE_DEPEND(fea, fddi, 1, 1, 1);
