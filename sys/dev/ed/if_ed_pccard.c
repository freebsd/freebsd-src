/*
 * Copyright (c) 1995, David Greenman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 * $FreeBSD: src/sys/dev/ed/if_ed_pccard.c,v 1.9.2.4 2000/09/10 08:45:11 nyan Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/select.h>
#include <machine/clock.h>

#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_mib.h>

#include <dev/ed/if_edreg.h>
#include <dev/ed/if_edvar.h>
#include <dev/pccard/pccardvar.h>
#include <pccard/cardinfo.h>
#include <pccard/slot.h>

#define CARD_MAJOR	50

/*
 *      PC-Card (PCMCIA) specific code.
 */
static int	ed_pccard_probe(device_t);
static int	ed_pccard_attach(device_t);
static int	ed_pccard_detach(device_t);

static void	ax88190_geteprom(struct ed_softc *);
static int	ed_pccard_memwrite(device_t dev, off_t offset, u_char byte);
static int	ed_pccard_memread(device_t dev, off_t offset, u_char *buf, int size);

static device_method_t ed_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ed_pccard_probe),
	DEVMETHOD(device_attach,	ed_pccard_attach),
	DEVMETHOD(device_detach,	ed_pccard_detach),

	{ 0, 0 }
};

static driver_t ed_pccard_driver = {
	"ed",
	ed_pccard_methods,
	sizeof(struct ed_softc)
};

static devclass_t ed_pccard_devclass;

DRIVER_MODULE(ed, pccard, ed_pccard_driver, ed_pccard_devclass, 0, 0);

/*
 *      ed_pccard_detach - unload the driver and clear the table.
 *      XXX TODO:
 *      This is usually called when the card is ejected, but
 *      can be caused by a modunload of a controller driver.
 *      The idea is to reset the driver's view of the device
 *      and ensure that any driver entry points such as
 *      read and write do not hang.
 */
static int
ed_pccard_detach(device_t dev)
{
	struct ed_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = &sc->arpcom.ac_if;

	if (sc->gone) {
		device_printf(dev, "already unloaded\n");
		return (0);
	}
	ed_stop(sc);
	ifp->if_flags &= ~IFF_RUNNING;
	ether_ifdetach(ifp, ETHER_BPF_SUPPORTED);
	sc->gone = 1;
	bus_teardown_intr(dev, sc->irq_res, sc->irq_handle);
	ed_release_resources(dev);
	return (0);
}

/* 
 * Probe framework for pccards.  Replicates the standard framework,
 * minus the pccard driver registration and ignores the ether address
 * supplied (from the CIS), relying on the probe to find it instead.
 */
static int
ed_pccard_probe(device_t dev)
{
	struct	ed_softc *sc = device_get_softc(dev);
	int	flags = device_get_flags(dev);
	int	error;

	if (ED_FLAGS_GETTYPE(flags) == ED_FLAGS_AX88190) {
		/* Special setup for AX88190 */
		u_char rdbuf[4];
		int iobase;
		int attr_ioport;

		/* XXX Allocate the port resource during setup. */
		error = ed_alloc_port(dev, 0, ED_NOVELL_IO_PORTS);
		if (error)
			return (error);

		sc->asic_offset = ED_NOVELL_ASIC_OFFSET;
		sc->nic_offset  = ED_NOVELL_NIC_OFFSET;

		sc->chip_type = ED_CHIP_TYPE_AX88190;

		/*
		 * Check & Set Attribute Memory IOBASE Register
		 */
		ed_pccard_memread(dev, ED_AX88190_IOBASE0, rdbuf, 4);
		attr_ioport = rdbuf[2] << 8 | rdbuf[0];
		iobase = rman_get_start(sc->port_res);
		if (attr_ioport != iobase) {
#if notdef
			printf("AX88190 IOBASE MISMATCH %04x -> %04x Setting\n", attr_ioport, iobase);
#endif /* notdef */
			ed_pccard_memwrite(dev, ED_AX88190_IOBASE0,
					   iobase & 0xff);
			ed_pccard_memwrite(dev, ED_AX88190_IOBASE1,
					   (iobase >> 8) & 0xff);
		}
		ax88190_geteprom(sc);

		ed_release_resources(dev);
	}

	error = ed_probe_Novell(dev, 0, flags);
	if (error == 0)
		goto end;
	ed_release_resources(dev);

	error = ed_probe_WD80x3(dev, 0, flags);
	if (error == 0)
		goto end;
	ed_release_resources(dev);

end:
	if (error == 0)
		error = ed_alloc_irq(dev, 0, 0);

	ed_release_resources(dev);
	return (error);
}

static int
ed_pccard_attach(device_t dev)
{
	struct ed_softc *sc = device_get_softc(dev);
	int flags = device_get_flags(dev);
	int error;
	int i;
	u_char sum;
	u_char ether_addr[ETHER_ADDR_LEN];
	
	if (sc->port_used > 0)
		ed_alloc_port(dev, sc->port_rid, sc->port_used);
	if (sc->mem_used)
		ed_alloc_memory(dev, sc->mem_rid, sc->mem_used);
	ed_alloc_irq(dev, sc->irq_rid, 0);
		
	error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET,
			       edintr, sc, &sc->irq_handle);
	if (error) {
		printf("setup intr failed %d \n", error);
		ed_release_resources(dev);
		return (error);
	}	      

	if (ed_get_Linksys(sc) == 0) {
		pccard_get_ether(dev, ether_addr);
		for (i = 0, sum = 0; i < ETHER_ADDR_LEN; i++)
			sum |= ether_addr[i];
		if (sum)
			bcopy(ether_addr, sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);
	}

	error = ed_attach(sc, device_get_unit(dev), flags);
	return (error);
}

static void
ax88190_geteprom(struct ed_softc *sc)
{
	int prom[16],i;
	u_char tmp;
	struct {
		unsigned char offset, value;
	} pg_seq[] = {
		{ED_P0_CR, ED_CR_RD2|ED_CR_STP},	/* Select Page0 */
		{ED_P0_DCR, 0x01},
		{ED_P0_RBCR0, 0x00},	/* Clear the count regs. */
		{ED_P0_RBCR1, 0x00},
		{ED_P0_IMR, 0x00},	/* Mask completion irq. */
		{ED_P0_ISR, 0xff},
		{ED_P0_RCR, ED_RCR_MON | ED_RCR_INTT},	/* Set To Monitor */
		{ED_P0_TCR, ED_TCR_LB0},	/* loopback mode. */
		{ED_P0_RBCR0, 32},
		{ED_P0_RBCR1, 0x00},
		{ED_P0_RSAR0, 0x00},
		{ED_P0_RSAR1, 0x04},
		{ED_P0_CR ,ED_CR_RD0 | ED_CR_STA},
	};

	/* Reset Card */
	tmp = ed_asic_inb(sc, ED_NOVELL_RESET);
	ed_asic_outb(sc, ED_NOVELL_RESET, tmp);
	DELAY(5000);
	ed_asic_outb(sc, ED_P0_CR, ED_CR_RD2 | ED_CR_STP);
	DELAY(5000);

	/* Card Settings */
	for (i = 0; i < sizeof(pg_seq) / sizeof(pg_seq[0]); i++)
		ed_nic_outb(sc, pg_seq[i].offset, pg_seq[i].value);

	/* Get Data */
	for (i = 0; i < 16; i++)
		prom[i] = ed_asic_inb(sc, 0);
/*
	for (i = 0; i < 16; i++)
		printf("ax88190 eprom [%02d] %02x %02x\n",
			i,prom[i] & 0xff,prom[i] >> 8);
*/
	sc->arpcom.ac_enaddr[0] = prom[0] & 0xff;
	sc->arpcom.ac_enaddr[1] = prom[0] >> 8;
	sc->arpcom.ac_enaddr[2] = prom[1] & 0xff;
	sc->arpcom.ac_enaddr[3] = prom[1] >> 8;
	sc->arpcom.ac_enaddr[4] = prom[2] & 0xff;
	sc->arpcom.ac_enaddr[5] = prom[2] >> 8;
}

/* XXX: Warner-san, any plan to provide access to the attribute memory? */
static int
ed_pccard_memwrite(device_t dev, off_t offset, u_char byte)
{
	struct pccard_devinfo *devi;
	dev_t d;
	struct iovec iov;
	struct uio uios;

	devi = device_get_ivars(dev);

	iov.iov_base = &byte;
	iov.iov_len = sizeof(byte);

	uios.uio_iov = &iov;
	uios.uio_iovcnt = 1;
	uios.uio_offset = offset;
	uios.uio_resid = sizeof(byte);
	uios.uio_segflg = UIO_SYSSPACE;
	uios.uio_rw = UIO_WRITE;
	uios.uio_procp = 0;

	d = makedev(CARD_MAJOR, devi->slt->slotnum);
	return devsw(d)->d_write(d, &uios, 0);
}

static int
ed_pccard_memread(device_t dev, off_t offset, u_char *buf, int size)
{
	struct pccard_devinfo *devi;
	dev_t d;
	struct iovec iov;
	struct uio uios;

	devi = device_get_ivars(dev);

	iov.iov_base = buf;
	iov.iov_len = size;

	uios.uio_iov = &iov;
	uios.uio_iovcnt = 1;
	uios.uio_offset = offset;
	uios.uio_resid = size;
	uios.uio_segflg = UIO_SYSSPACE;
	uios.uio_rw = UIO_READ;
	uios.uio_procp = 0;

	d = makedev(CARD_MAJOR, devi->slt->slotnum);
	return devsw(d)->d_read(d, &uios, 0);
}
