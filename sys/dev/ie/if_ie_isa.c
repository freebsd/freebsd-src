/*-
 * Copyright (c) 2003 Matthew N. Dodd
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
 * Portions: 
 * Copyright (c) 1992, 1993, University of Vermont and State
 *  Agricultural College.
 * Copyright (c) 1992, 1993, Garrett A. Wollman.
 * Copyright (c) 1990, 1991, William F. Jolitz
 * Copyright (c) 1990, The Regents of the University of California
 * Copyright (c) 1993, 1994, Charles M. Hannum
 * Copyright (c) 1993, 1994, 1995, Rodney W. Grimes
 * Copyright (c) 1997, Aaron C. Smith
 *
 * See if_ie.c for applicable license.
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

#include <machine/md_var.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_media.h>

#include <isa/isavar.h>
#include <isa/pnpvar.h>

#include <i386/isa/elink.h>

#include <dev/ic/i82586.h>
#include <dev/ie/if_ie507.h>
#include <dev/ie/if_iee16.h>
#include <dev/ie/if_iereg.h>
#include <dev/ie/if_ievar.h>

static int		ie_modevent		(module_t, int, void *);

static void		ie_isa_3C507_identify	(driver_t *, device_t);
static int		ie_isa_3C507_probe	(device_t);
static int		ie_isa_3C507_attach	(device_t);
static int		ie_3C507_port_check	(u_int32_t);

static void		ie_isa_ee16_identify	(driver_t *, device_t);
static int		ie_isa_ee16_probe	(device_t);
static int		ie_isa_ee16_attach	(device_t);
static int		ie_isa_ee16_shutdown	(device_t);
static int		ie_ee16_port_check	(u_int32_t port);
static u_int16_t	ie_ee16_hw_read_eeprom	(u_int32_t port, int loc);

static int		ie_isa_sl_probe		(device_t);
static int		ie_isa_sl_attach	(device_t);
static enum ie_hardware	ie_isa_sl_get_hard_type	(u_int32_t);

/*
 * 3Com 3C507 Etherlink 16
 */
#define IE_3C507_IOBASE_LOW	0x200
#define IE_3C507_IOBASE_HIGH	0x3e0
#define IE_3C507_IOSIZE		16

#define IE_3C507_IRQ_MASK	0x0f

#define IE_3C507_MADDR_HIGH	0x20
#define IE_3C507_MADDR_MASK	0x1c
#define IE_3C507_MADDR_BASE	0xc0000
#define IE_3C507_MADDR_SHIFT	12

#define IE_3C507_MSIZE_MASK	3
#define IE_3C507_MSIZE_SHIFT	14

static void
ie_isa_3C507_identify (driver_t *driver, device_t parent)
{
	char *		desc = "3Com 3C507 Etherlink 16";
	device_t	child;
	u_int32_t	port, maddr, msize;
	u_int8_t	irq, data;
	int		error;

	/* Reset and put card in CONFIG state without changing address. */
	elink_reset();
	elink_idseq(ELINK_507_POLY);
	elink_idseq(ELINK_507_POLY);
	outb(ELINK_ID_PORT, 0xff);

	for (port = IE_3C507_IOBASE_LOW;
	     port <= IE_3C507_IOBASE_HIGH;
	     port += IE_3C507_IOSIZE) {

		if (ie_3C507_port_check(port)) {
#ifdef DEBUG 
			if (bootverbose) {
				device_printf(parent,
					"(if_ie) (3C507) not found at port %#x\n",
					port);
			}
#endif
			continue;
		}

		outb(port + IE507_CTRL, EL_CTRL_NRST);

		data = inb(port + IE507_IRQ);
		irq = data & IE_3C507_IRQ_MASK;

		data = inb(port + IE507_MADDR);

		if (data & IE_3C507_MADDR_HIGH) {
			if (bootverbose) {
				device_printf(parent,
					"(if_ie) can't map 3C507 RAM in high memory\n");
			}
			continue;
		}

		maddr = IE_3C507_MADDR_BASE +
			((data & IE_3C507_MADDR_MASK)
			<< IE_3C507_MADDR_SHIFT);
		msize = ((data & IE_3C507_MSIZE_MASK) + 1)
			<< IE_3C507_MSIZE_SHIFT;

		child = BUS_ADD_CHILD(parent, ISA_ORDER_SPECULATIVE, "ie", -1);
		device_set_desc_copy(child, desc);
		device_set_driver(child, driver);

		error = bus_set_resource(child, SYS_RES_IRQ, 0, irq, 1);
		if (error) {
			device_printf(parent, "(if_ie) Unable to set IRQ resource %d.\n",
					irq);
			error = device_delete_child(parent, child);
			continue;
		}

		error = bus_set_resource(child, SYS_RES_IOPORT, 0, port, IE_3C507_IOSIZE);
		if (error) {
			device_printf(parent, "(if_ie) Unable to set IOPORT resource %#x-%#x.\n",
					port, port+IE_3C507_IOSIZE);
			error = device_delete_child(parent, child);
			continue;
		}

		error = bus_set_resource(child, SYS_RES_MEMORY, 0, maddr, msize);
		if (error) {
			device_printf(parent, "(if_ie) Unable to set MEMORY resource %#x-%#x.\n",
					maddr, maddr+msize);
			error = device_delete_child(parent, child);
			continue;
		}

		if (bootverbose) {
			device_printf(parent,
				"(if_ie) <%s> at port %#x-%#x irq %d iomem %#lx-%#lx (%dKB)\n",
				desc,
				port, (port + IE_3C507_IOSIZE) - 1,
				irq,
				(u_long)maddr, (u_long)(maddr + msize) - 1,
				(msize / 1024));
		}
	}

	/* go to RUN state */
	outb(ELINK_ID_PORT, 0x00);
	elink_idseq(ELINK_507_POLY);
	outb(ELINK_ID_PORT, 0x00);

	return;
}

static int
ie_isa_3C507_probe (device_t dev)
{
	u_int32_t	iobase;

	/* No ISA-PnP support */
	if (isa_get_vendorid(dev)) {
		return (ENXIO);
	}

	/* No ISA-HINT support */
	if (!device_get_desc(dev)) {
		return (EBUSY);
	}

	/* Have we at least an ioport? */
	if ((iobase = bus_get_resource_start(dev, SYS_RES_IOPORT, 0)) == 0) {
		return (ENXIO);
	}

	/* Is this thing really a 3c507? */
	if (ie_3C507_port_check(iobase)) {
		return (ENXIO);
	}

	return (0);
}

static int
ie_isa_3C507_attach (device_t dev)
{
	struct ie_softc *	sc;
	int			error;

	sc = device_get_softc(dev);

	sc->io_rid = 0;
	sc->irq_rid = 0;
	sc->mem_rid = 0;

	error = ie_alloc_resources(dev);
	if (error) {
		goto bad;
	}

	sc->bus_use = 0;
	sc->ie_reset_586 = el_reset_586;
	sc->ie_chan_attn = el_chan_attn;
	sc->hard_type = IE_3C507;
	sc->hard_vers = 0;

	outb(PORT(sc) + IE507_CTRL, EL_CTRL_NORMAL);

	if (!check_ie_present(sc)) {
		error = ENXIO;
		goto bad;
	}

	sl_read_ether(sc, sc->enaddr);

	/* Clear the interrupt latch just in case. */
	outb(PORT(sc) + IE507_ICTRL, 1);

	error = ie_attach(dev);
	if (error) {
		device_printf(dev, "ie_attach() failed.\n");
		goto bad;
	}

	return (0);
bad:
	ie_release_resources(dev);

	return (error);
}

/*
 * If a 3c507 is present, return 0
 * else, return 1.
 */
static int
ie_3C507_port_check (u_int32_t port)
{
	u_char *	signature = "*3COM*";
	int		i;

	for (i = 0; i < 6; i++)
		if (inb(port + i) != signature[i])
			return (ENXIO);

	return (0);
}

/*
 * Intel EtherExpress 16
 */
#define IE_EE16_ID_PORT			0x0f
#define IE_EE16_ID			0xbaba
#define IE_EE16_EEPROM_CONFIG1		0x00
#define IE_EE16_EEPROM_IRQ_MASK		0xe000
#define IE_EE16_EEPROM_IRQ_SHIFT	13
#define IE_EE16_EEPROM_MEMCFG		0x06
#define IE_EE16_IOSIZE			16

/*
 * TODO:
 *		Test for 8/16 bit mode.
 *		Test for invalid mem sizes.
 */
static void
ie_isa_ee16_identify (driver_t *driver, device_t parent)
{
	char *		desc = "Intel EtherExpress 16";
	device_t	child;
	u_int16_t	ports[] = {
				0x300, 0x310, 0x320, 0x330,
				0x340, 0x350, 0x360, 0x370,
				0x200, 0x210, 0x220, 0x230,
				0x240, 0x250, 0x260, 0x270,
				0
			};
	u_int16_t	irqs[] = { 0, 0x09, 0x03, 0x04, 0x05, 0x0a, 0x0b, 0 };
	u_int32_t	port, maddr, msize;
	u_int8_t	irq;
	u_int16_t	data;
	int		i, error;

	for (i = 0; ports[i]; i++) {
		port = ports[i];

		if (ie_ee16_port_check(port)) {
#ifdef DEBUG
			if (bootverbose) {
				device_printf(parent,
					"if_ie: (EE16) not found at port %#x\n",
					port);
			}
#endif
			continue;
		}

		/* reset any ee16 at the current iobase */
		outb(port + IEE16_ECTRL, IEE16_RESET_ASIC);
		outb(port + IEE16_ECTRL, 0);
		DELAY(240);

		data = ie_ee16_hw_read_eeprom(port, IE_EE16_EEPROM_CONFIG1);
		irq = irqs[((data & IE_EE16_EEPROM_IRQ_MASK)
			   >> IE_EE16_EEPROM_IRQ_SHIFT)];

		data = ie_ee16_hw_read_eeprom(port, IE_EE16_EEPROM_MEMCFG);
		maddr = 0xc0000 + ((ffs(data & 0x00ff) - 1) * 0x4000);
		msize = (fls((data & 0x00ff) >> (ffs(data & 0x00ff) - 1)))
			* 0x4000;

		child = BUS_ADD_CHILD(parent, ISA_ORDER_SPECULATIVE, "ie", -1);
		device_set_desc_copy(child, desc);
		device_set_driver(child, driver);

		error = bus_set_resource(child, SYS_RES_IRQ, 0, irq, 1);
		if (error) {
			device_printf(parent, "(if_ie) Unable to set IRQ resource %d.\n",
					irq);
			error = device_delete_child(parent, child);
			continue;
		}

		error = bus_set_resource(child, SYS_RES_IOPORT, 0, port, IE_EE16_IOSIZE);
		if (error) {
			device_printf(parent, "(if_ie) Unable to set IOPORT resource %#x-%#x.\n",
					port, port+IE_EE16_IOSIZE);
			error = device_delete_child(parent, child);
			continue;
		}

		error = bus_set_resource(child, SYS_RES_MEMORY, 0, maddr, msize);
		if (error) {
			device_printf(parent, "(if_ie) Unable to set MEMORY resource %#x-%#x.\n",
					maddr, maddr+msize);
			error = device_delete_child(parent, child);
			continue;
		}

		if (bootverbose) {
			device_printf(parent,
				"if_ie: <%s> at port %#x-%#x irq %d iomem %#lx-%#lx (%dKB)\n",
				desc,
				port, (port + IE_EE16_IOSIZE) - 1,
				irq,
				(u_long)maddr, (u_long)(maddr + msize) - 1,
				(msize / 1024));
		}
	}

	return;
}

static int
ie_isa_ee16_probe (device_t dev)
{
	u_int32_t	iobase;

	/* No ISA-PnP support */
	if (isa_get_vendorid(dev))
		return (ENXIO);

	/* No ISA-HINT support */
	if (!device_get_desc(dev))
		return (EBUSY);

	/* Have we at least an ioport? */
	if ((iobase = bus_get_resource_start(dev, SYS_RES_IOPORT, 0)) == 0)
		return (ENXIO);

	/* Is this really an EE16? */
	if (ie_ee16_port_check(iobase))
		return (ENXIO);

	return (0);
}

static int
ie_isa_ee16_attach (device_t dev)
{
	struct ie_softc *	sc;
	int			i, error;
	u_int16_t		checksum;
	u_short			eaddrtemp, pg, adjust, decode, edecode;
	u_char			bart_config;
        
	sc = device_get_softc(dev);

	sc->io_rid = 0;
	sc->irq_rid = 0;
	sc->mem_rid = 0;

	error = ie_alloc_resources(dev);
	if (error) {
		goto bad;
	}

	sc->bus_use = 0;
	sc->ie_reset_586 = ee16_reset_586;
	sc->ie_chan_attn = ee16_chan_attn;
	sc->hard_type = IE_EE16;
	sc->hard_vers = 0;
	sc->iomem = 0;

	/* reset any ee16 at the current iobase */
	outb(PORT(sc) + IEE16_ECTRL, IEE16_RESET_ASIC);
	outb(PORT(sc) + IEE16_ECTRL, 0);
	DELAY(240);

	/* Is this really an EE16? */
	if (ie_ee16_port_check(PORT(sc))) {
		device_printf(dev, "ie_ee16_port_check() failed\n");
		error = ENXIO;
		goto bad;
	}

	/* need to put the 586 in RESET while we access the eeprom. */
	outb(PORT(sc) + IEE16_ECTRL, IEE16_RESET_586);

	/* read the eeprom and checksum it, should == IE_E16_ID */
	checksum = 0;
	for (i = 0; i < 0x40; i++)
		checksum += ie_ee16_hw_read_eeprom(PORT(sc), i);

	if (checksum != IE_EE16_ID) {
		device_printf(dev, "invalid eeprom checksum: %x\n", checksum);
		error = ENXIO;
		goto bad;
	}

	if ((kvtop(sc->iomembot) < 0xC0000) ||
	     (kvtop(sc->iomembot) + sc->iosize > 0xF0000)) {
		device_printf(sc->dev, "mapped memory location %p out of range\n",
			(void *)sc->iomembot);
		error = ENXIO;
		goto bad;
	}

	pg = ((kvtop(sc->iomembot)) & 0x3C000) >> 14;
	adjust = IEE16_MCTRL_FMCS16 | (pg & 0x3) << 2;
	decode = ((1 << (sc->iosize / 16384)) - 1) << pg;
	edecode = ((~decode >> 4) & 0xF0) | (decode >> 8);

	/* ZZZ This should be checked against eeprom location 6, low byte */
	outb(PORT(sc) + IEE16_MEMDEC, decode & 0xFF);
	/* ZZZ This should be checked against eeprom location 1, low byte */
	outb(PORT(sc) + IEE16_MCTRL, adjust);
	/* ZZZ Now if I could find this one I would have it made */
	outb(PORT(sc) + IEE16_MPCTRL, (~decode & 0xFF));
	/* ZZZ I think this is location 6, high byte */
	outb(PORT(sc) + IEE16_MECTRL, edecode); /* XXX disable Exxx */

#if 0
	(void) kvtop(sc->iomembot);
#endif

	/*
	 * first prime the stupid bart DRAM controller so that it works,
	 * then zero out all of memory.
	 */
	bzero(sc->iomembot, 32);
	bzero(sc->iomembot, sc->iosize);

	/* Get the encoded interrupt number from the EEPROM */
	sc->irq_encoded = ie_ee16_hw_read_eeprom(PORT(sc),
						 IE_EE16_EEPROM_CONFIG1);
	sc->irq_encoded = (sc->irq_encoded & IE_EE16_EEPROM_IRQ_MASK) >>
			   IE_EE16_EEPROM_IRQ_SHIFT;

	/*
	 * Get the hardware ethernet address from the EEPROM and save it in
	 * the softc for use by the 586 setup code.
	 */
	eaddrtemp = ie_ee16_hw_read_eeprom(PORT(sc), IEE16_EEPROM_ENET_HIGH);
	sc->enaddr[1] = eaddrtemp & 0xFF;
	sc->enaddr[0] = eaddrtemp >> 8;
	eaddrtemp = ie_ee16_hw_read_eeprom(PORT(sc), IEE16_EEPROM_ENET_MID);
	sc->enaddr[3] = eaddrtemp & 0xFF;
	sc->enaddr[2] = eaddrtemp >> 8;
	eaddrtemp = ie_ee16_hw_read_eeprom(PORT(sc), IEE16_EEPROM_ENET_LOW);
	sc->enaddr[5] = eaddrtemp & 0xFF;
	sc->enaddr[4] = eaddrtemp >> 8;

	/* disable the board interrupts */
	outb(PORT(sc) + IEE16_IRQ, sc->irq_encoded);

	/* enable loopback to keep bad packets off the wire */
	bart_config = inb(PORT(sc) + IEE16_CONFIG);
	bart_config |= IEE16_BART_LOOPBACK;
	bart_config |= IEE16_BART_MCS16_TEST;/* inb doesn't get bit! */
	outb(PORT(sc) + IEE16_CONFIG, bart_config);
	bart_config = inb(PORT(sc) + IEE16_CONFIG);

	/* take the board out of reset state */
	outb(PORT(sc) + IEE16_ECTRL, 0);
	DELAY(100);

	if (!check_ie_present(sc)) {
		device_printf(dev, "check_ie_present() returned false.\n");
		error = ENXIO;
		goto bad;
	}

	error = ie_attach(dev);
	if (error) {
		device_printf(dev, "ie_attach() failed.\n");
		goto bad;
	}

	return (0);
bad:
	ie_release_resources(dev);

	return (error);
}

static int
ie_isa_ee16_shutdown(device_t dev)
{
	struct ie_softc *	sc;

	sc = device_get_softc(dev);
	IE_LOCK(sc);
	ee16_shutdown(sc);
	IE_UNLOCK(sc);

	return (0);
}

/*
 * If an EE16 is present, return 0
 * else, return 1.
 */
static int
ie_ee16_port_check (u_int32_t port)
{
	int		i;
	u_int16_t	board_id;
	u_int8_t	data;

	board_id = 0;
	for (i = 0; i < 4; i++) {
		data = inb(port + IE_EE16_ID_PORT);
		board_id |= ((data >> 4) << ((data & 0x03) << 2));
	}

	if (board_id != IE_EE16_ID)
		return (1);

	return (0);
}

static void
ie_ee16_hw_eeprom_clock (u_int32_t port, int state)
{
	u_int8_t	ectrl;

	ectrl = inb(port + IEE16_ECTRL);
	ectrl &= ~(IEE16_RESET_ASIC | IEE16_ECTRL_EESK);

	if (state) {
		ectrl |= IEE16_ECTRL_EESK;
	}
	outb(port + IEE16_ECTRL, ectrl);
	DELAY(9);		/* EESK must be stable for 8.38 uSec */
}

static void
ie_ee16_hw_eeprom_out (u_int32_t port, u_int16_t edata, int count)
{
	u_int8_t	ectrl;
	int		i;

	ectrl = inb(port + IEE16_ECTRL);
	ectrl &= ~IEE16_RESET_ASIC;

	for (i = count - 1; i >= 0; i--) {
		ectrl &= ~IEE16_ECTRL_EEDI;
		if (edata & (1 << i)) {
			ectrl |= IEE16_ECTRL_EEDI;
		}
		outb(port + IEE16_ECTRL, ectrl);
		DELAY(1);       /* eeprom data must be setup for 0.4 uSec */
		ie_ee16_hw_eeprom_clock(port, 1);
		ie_ee16_hw_eeprom_clock(port, 0);
	}
	ectrl &= ~IEE16_ECTRL_EEDI;
	outb(port + IEE16_ECTRL, ectrl);
	DELAY(1);               /* eeprom data must be held for 0.4 uSec */

	return;
}

static u_int16_t
ie_ee16_hw_eeprom_in (u_int32_t port)
{
	u_int8_t	ectrl;
	u_int16_t	edata;
	int		i;

	ectrl = inb(port + IEE16_ECTRL);
	ectrl &= ~IEE16_RESET_ASIC;

	for (edata = 0, i = 0; i < 16; i++) {
		edata = edata << 1;
		ie_ee16_hw_eeprom_clock(port, 1);
		ectrl = inb(port + IEE16_ECTRL);
		if (ectrl & IEE16_ECTRL_EEDO) {
			edata |= 1;
		}
		ie_ee16_hw_eeprom_clock(port, 0);
	}
	return (edata);
}

static u_int16_t
ie_ee16_hw_read_eeprom (u_int32_t port, int loc)
{
	u_int8_t	ectrl;
	u_int16_t	edata;

	ectrl = inb(port + IEE16_ECTRL);
	ectrl &= IEE16_ECTRL_MASK;
	ectrl |= IEE16_ECTRL_EECS;
	outb(port + IEE16_ECTRL, ectrl);

	ie_ee16_hw_eeprom_out(port, IEE16_EEPROM_READ, IEE16_EEPROM_OPSIZE1);
	ie_ee16_hw_eeprom_out(port, loc, IEE16_EEPROM_ADDR_SIZE);
	edata = ie_ee16_hw_eeprom_in(port);

	ectrl = inb(port + IEE16_ECTRL);
	ectrl &= ~(IEE16_RESET_ASIC | IEE16_ECTRL_EEDI | IEE16_ECTRL_EECS);
	outb(port + IEE16_ECTRL, ectrl);

	ie_ee16_hw_eeprom_clock(port, 1);
	ie_ee16_hw_eeprom_clock(port, 0);

	return (edata);
}

/*
 * AT&T StarLan/
 */

static int
ie_isa_sl_probe (device_t dev)
{
	u_int32_t	iobase;

	/* No ISA-PnP support */
	if (isa_get_vendorid(dev))
		return (ENXIO);

	/* ISA-HINT support only! */
	if (device_get_desc(dev))
		return (EBUSY);

	/* Have we at least an ioport? */
	if ((iobase = bus_get_resource_start(dev, SYS_RES_IOPORT, 0)) == 0)
		return (ENXIO);

	/* Is this really an SL board? */
	if (ie_isa_sl_get_hard_type(iobase) == IE_NONE)
		return (ENXIO);

	return (ENXIO);
}

static int
ie_isa_sl_attach (device_t dev)
{
	struct ie_softc *	sc;
	int			error;

	sc = device_get_softc(dev);

	sc->io_rid = 0;
	sc->irq_rid = 0;
	sc->mem_rid = 0;

	error = ie_alloc_resources(dev);
	if (error) {
		goto bad;
	}

	/* Is this really an SL board? */
	if ((sc->hard_type = ie_isa_sl_get_hard_type(PORT(sc))) == IE_NONE) {
		error = ENXIO;
		goto bad;
	}

	sc->hard_vers = SL_REV(inb(PORT(sc) + IEATT_REVISION));
	if (sc->hard_type == IE_NI5210) {
		sc->bus_use = 1;
	} else {
		sc->bus_use = 0;
	}

	sc->ie_reset_586 = sl_reset_586;
	sc->ie_chan_attn = sl_chan_attn;

	if (!check_ie_present(sc)) {
		error = ENXIO;
		goto bad;
	}

	switch (sc->hard_type) {
		case IE_EN100:
		case IE_STARLAN10:
		case IE_SLFIBER:
		case IE_NI5210:
			sl_read_ether(sc, sc->enaddr);
			break;
		default:
			if (bootverbose)
				device_printf(sc->dev, "unknown AT&T board type code %d\n", sc->hard_type);
			error = ENXIO;
			goto bad;
			break;
	}

	error = ie_attach(dev);
	if (error) {
		device_printf(dev, "ie_attach() failed.\n");
		goto bad;
	}

	return (0);
bad:
	ie_release_resources(dev);

	return (error);
}

static enum ie_hardware
ie_isa_sl_get_hard_type (u_int32_t port)
{
	u_char			c;
	enum ie_hardware	retval;

	c = inb(port + IEATT_REVISION);
	switch (SL_BOARD(c)) {
		case SL1_BOARD:
			if (inb(port + IEATT_ATTRIB) != NI5210_BOARD)
				retval = IE_NONE;
			retval = IE_NI5210;
			break;
		case SL10_BOARD:
			retval = IE_STARLAN10;
			break;
		case EN100_BOARD:
			retval = IE_EN100;
			break;
		case SLFIBER_BOARD:
			retval = IE_SLFIBER;
			break;
		default:
			retval = IE_NONE;
	}
	return (retval);
}

static devclass_t ie_devclass;

static device_method_t ie_isa_3C507_methods[] = {
	DEVMETHOD(device_identify,	ie_isa_3C507_identify),
	DEVMETHOD(device_probe,		ie_isa_3C507_probe),
	DEVMETHOD(device_attach,	ie_isa_3C507_attach),
	DEVMETHOD(device_detach,	ie_detach),
	{ 0, 0 }
};

static driver_t ie_isa_3C507_driver = {
	"ie",
	ie_isa_3C507_methods,
	sizeof(struct ie_softc), 
};

DRIVER_MODULE(ie_3C507, isa, ie_isa_3C507_driver, ie_devclass, ie_modevent, 0);
MODULE_DEPEND(ie_3C507, elink, 1, 1, 1);

static device_method_t ie_isa_ee16_methods[] = {
	DEVMETHOD(device_identify,	ie_isa_ee16_identify),
	DEVMETHOD(device_probe,		ie_isa_ee16_probe),
	DEVMETHOD(device_attach,	ie_isa_ee16_attach),
	DEVMETHOD(device_shutdown,	ie_isa_ee16_shutdown),
	DEVMETHOD(device_detach,	ie_detach),
	{ 0, 0 }
};

static driver_t ie_isa_ee16_driver = {
	"ie",
	ie_isa_ee16_methods,
	sizeof(struct ie_softc), 
};

DRIVER_MODULE(ie, isa, ie_isa_ee16_driver, ie_devclass, ie_modevent, 0);

static device_method_t ie_isa_sl_methods[] = {
	DEVMETHOD(device_probe,		ie_isa_sl_probe),
	DEVMETHOD(device_attach,	ie_isa_sl_attach),
	DEVMETHOD(device_detach,	ie_detach),
	{ 0, 0 }
};

static driver_t ie_isa_sl_driver = {
	"ie",
	ie_isa_sl_methods,
	sizeof(struct ie_softc), 
};

DRIVER_MODULE(ie_SL, isa, ie_isa_sl_driver, ie_devclass, ie_modevent, 0);

static int
ie_modevent (mod, what, arg)
	module_t	mod;
	int		what;
	void *		arg;
{
	device_t *	devs;
	int		count;
	int		i;

	switch (what) {
	case MOD_LOAD:
		break;
	case MOD_UNLOAD:
		devclass_get_devices(ie_devclass, &devs, &count);
		for (i = 0; i < count; i++)
			device_delete_child(device_get_parent(devs[i]), devs[i]);
		free(devs, M_TEMP);
		break;
	default:
		break;
	};

	return (0);
}
