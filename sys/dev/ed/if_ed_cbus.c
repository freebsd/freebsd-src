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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/kernel.h>

#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#ifdef PC98
#include <sys/rman.h>
#include <machine/resource.h>
#include <machine/clock.h>
#include <machine/md_var.h>
#endif

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_mib.h>

#include <isa/isavar.h>
#include <isa/pnpvar.h>

#include <dev/ed/if_edvar.h>
#ifdef PC98
#include <dev/ed/if_edreg.h>
#include <dev/ed/if_ed98.h>

static int ed98_alloc_port	__P((device_t, int));
static int ed98_alloc_memory	__P((device_t, int));
static int ed_pio_testmem	__P((struct ed_softc *, int, int, int));
static int ed_probe_SIC98	__P((device_t, int, int));
static int ed_probe_CNET98	__P((device_t, int, int));
static int ed_probe_CNET98EL	__P((device_t, int, int));
static int ed_probe_NEC77	__P((device_t, int, int));
static int ed_probe_NW98X	__P((device_t, int, int));
static int ed_probe_SB98	__P((device_t, int, int));
static int ed_probe_EZ98	__P((device_t, int, int));
static int ed98_probe_Novell	__P((device_t, int, int));
static int ed98_probe_generic8390	__P((struct ed_softc *));
static void ed_reset_CNET98	__P((struct ed_softc *, int));
static void ed_winsel_CNET98	__P((struct ed_softc *, u_short));
static void ed_get_SB98		__P((struct ed_softc *));
#endif

static int ed_isa_probe		__P((device_t));
static int ed_isa_attach	__P((device_t));

static struct isa_pnp_id ed_ids[] = {
#ifdef PC98
/* TODO - list up PnP boards for PC-98 */
	{ 0,		NULL }
#endif
};

static int
ed_isa_probe(dev)
	device_t dev;
{
	struct ed_softc *sc = device_get_softc(dev);
	int flags = device_get_flags(dev);
	int error = 0;

	bzero(sc, sizeof(struct ed_softc));
#ifdef PC98
	sc->type = ED_TYPE98(flags);
#ifdef ED_DEBUG
	device_printf(dev, "ed_isa_probe: sc->type=%x\n", sc->type);
#endif
#endif

	/* Check isapnp ids */
	error = ISA_PNP_PROBE(device_get_parent(dev), dev, ed_ids);
#ifdef ED_DEBUG
	device_printf(dev, "ed_isa_probe: ISA_PNP_PROBE returns %d\n", error);
#endif

	/* If the card had a PnP ID that didn't match any we know about */
	if (error == ENXIO) {
		goto end;
	}

	/* If we had some other problem. */
	if (!(error == 0 || error == ENOENT)) {
		goto end;
	}

	/* Heuristic probes */
#ifdef ED_DEBUG
	device_printf(dev, "ed_isa_probe: Heuristic probes start\n");
#endif
#ifdef PC98
	switch (sc->type) {
	case ED_TYPE98_GENERIC:
		/*
		 * CAUTION!
		 * sc->type of these boards are overwritten by PC/AT's value.
		 */

		/*
		 * SMC EtherEZ98
		 */
		error = ed_probe_EZ98(dev, 0, flags);
		if (error == 0) {
			goto end;
		}

		ed_release_resources(dev);

		/*
		 * Allied Telesis CenterCom LA-98-T
		 */
		error = ed_probe_Novell(dev, 0, flags);
		if (error == 0) {
			goto end;
		}

		break;

	/*
	 * NE2000-like boards probe routine
	 */
	case ED_TYPE98_BDN:
		/*
		 * ELECOM LANEED LD-BDN
		 * PLANET SMART COM 98 EN-2298
		 */
	case ED_TYPE98_LGY:
		/*
		 * MELCO LGY-98, IND-SP, IND-SS
		 * MACNICA NE2098
		 */
	case ED_TYPE98_ICM:
		/*
		 * ICM DT-ET-25, DT-ET-T5, IF-2766ET, IF-2771ET
		 * D-Link DE-298P, DE-298
		 */
	case ED_TYPE98_EGY:
		/*
		 * MELCO EGY-98
		 * Contec C-NET(98)E-A, C-NET(98)L-A
		 */
	case ED_TYPE98_108:
		/*
		 * NEC PC-9801-107,108
		 */
	case ED_TYPE98_NC5098:
		/*
		 * NextCom NC5098
		 */

		error = ed98_probe_Novell(dev, 0, flags);

		break;

	/*
	 * other boards with special probe routine
	 */
	case ED_TYPE98_SIC:
		/*
		 * Allied Telesis SIC-98
		 */
		error = ed_probe_SIC98(dev, 0, flags);

		break;

	case ED_TYPE98_CNET98EL:
		/*
		 * Contec C-NET(98)E/L
		 */
		error = ed_probe_CNET98EL(dev, 0, flags);

		break;

	case ED_TYPE98_CNET98:
		/*
		 * Contec C-NET(98)
		 */
		error = ed_probe_CNET98(dev, 0, flags);

		break;

	case ED_TYPE98_LA98:
		/*
		 * IO-DATA LA/T-98
		 * NEC PC-9801-77,78
		 */
		error = ed_probe_NEC77(dev, 0, flags);

		break;

	case ED_TYPE98_NW98X:
		/*
		 * Networld EC/EP-98X
		 */
		error = ed_probe_NW98X(dev, 0, flags);

		break;

	case ED_TYPE98_SB98:
		/*
		 * Soliton SB-9801
		 * Fujikura FN-9801
		 */

		error = ed_probe_SB98(dev, 0, flags);

		break;
	}
#endif

end:
#ifdef ED_DEBUG
	device_printf(dev, "ed_isa_probe: end, error=%d\n", error);
#endif
	if (error == 0)
		error = ed_alloc_irq(dev, 0, 0);

	ed_release_resources(dev);
	return (error);
}

static int
ed_isa_attach(dev)
	device_t dev;
{
	struct ed_softc *sc = device_get_softc(dev);
	int flags = device_get_flags(dev);
	int error;

	if (sc->port_used > 0) {
#ifdef PC98
		if (ED_TYPE98(flags) == ED_TYPE98_GENERIC) {
			ed_alloc_port(dev, sc->port_rid, sc->port_used);
		} else {
			ed98_alloc_port(dev, sc->port_rid);
		}
#endif
	}
	if (sc->mem_used)
		ed_alloc_memory(dev, sc->mem_rid, sc->mem_used);

	ed_alloc_irq(dev, sc->irq_rid, 0);

	error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET,
			       edintr, sc, &sc->irq_handle);
	if (error) {
		ed_release_resources(dev);
		return (error);
	}

	return ed_attach(sc, device_get_unit(dev), flags);
}

static device_method_t ed_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ed_isa_probe),
	DEVMETHOD(device_attach,	ed_isa_attach),

	{ 0, 0 }
};

static driver_t ed_isa_driver = {
	"ed",
	ed_isa_methods,
	sizeof(struct ed_softc)
};

static devclass_t ed_isa_devclass;

DRIVER_MODULE(ed, isa, ed_isa_driver, ed_isa_devclass, 0, 0);

#ifdef PC98
/*
 * Interrupt conversion table for EtherEZ98
 */
static unsigned short ed_EZ98_intr_val[] = {
	0,
	3,
	5,
	6,
	0,
	9,
	12,
	13
};

static int
ed_probe_EZ98(dev, port_rid, flags)
	device_t dev;
	int port_rid;
	int flags;
{
	struct ed_softc *sc = device_get_softc(dev);
	int error;
	static unsigned short *intr_vals[] = {NULL, ed_EZ98_intr_val};

	error = ed_alloc_port(dev, port_rid, ED_EZ98_IO_PORTS);
	if (error) {
		return (error);
	}

	sc->asic_offset = ED_EZ98_ASIC_OFFSET;
	sc->nic_offset  = ED_EZ98_NIC_OFFSET;

	return ed_probe_WD80x3_generic(dev, flags, intr_vals);
}

/*
 * I/O conversion tables
 */

/* LGY-98, ICM, C-NET(98)E/L */
static	bus_addr_t ed98_ioaddr_generic[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

/*
 *		Definitions for Contec C-NET(98)E/L
 */
#define	ED_CNET98EL_ICR         2	/* Interrupt Configuration Register */

#define	ED_CNET98EL_ICR_IRQ3	0x01
#define	ED_CNET98EL_ICR_IRQ5	0x02
#define	ED_CNET98EL_ICR_IRQ6	0x04
#define	ED_CNET98EL_ICR_IRQ12	0x20

#define	ED_CNET98EL_IMR         4	/* Interrupt Mask Register	*/
#define	ED_CNET98EL_ISR         5	/* Interrupt Status Register	*/

/* EGY-98 */
static	bus_addr_t ed98_ioaddr_egy98[] = {
	0,     0x02,  0x04,  0x06,  0x08,  0x0a,  0x0c,  0x0e,
	0x100, 0x102, 0x104, 0x106, 0x108, 0x10a, 0x10c, 0x10e
};

/* SIC-98 */
static	bus_addr_t ed98_ioaddr_sic98[] = {
	0x0000, 0x0200, 0x0400, 0x0600, 0x0800, 0x0a00, 0x0c00, 0x0e00,
	0x1000, 0x1200, 0x1400, 0x1600, 0x1800, 0x1a00, 0x1c00, 0x1e00
};

/* LA/T-98, LD-BDN, PC-9801-77, SB-9801 */
static	bus_addr_t ed98_ioaddr_la98[] = {
	0x0000, 0x1000, 0x2000, 0x3000, 0x4000, 0x5000, 0x6000, 0x7000,
	0x8000, 0x9000, 0xa000, 0xb000, 0xc000, 0xd000, 0xe000, 0xf000,
	0x0100	/* for NEC 77(see below) */
};

/*
 *		Definitions for NEC PC-9801-77
 */
#define	ED_NEC77_IRQ		16	/* Interrupt Configuration Register */

#define	ED_NEC77_IRQ3		0x04
#define	ED_NEC77_IRQ5		0x06
#define	ED_NEC77_IRQ6		0x08
#define	ED_NEC77_IRQ12		0x0a
#define	ED_NEC77_IRQ13		0x02

/*
 *		Definitions for Soliton SB-9801
 */
#define	ED_SB98_CFG		1	/* Board configuration		*/

#define	ED_SB98_CFG_IRQ3	0x00
#define	ED_SB98_CFG_IRQ5	0x04
#define	ED_SB98_CFG_IRQ6	0x08
#define	ED_SB98_CFG_IRQ12	0x0c
#define	ED_SB98_CFG_ALTPORT	0x40		/* use EXTERNAL media	*/
#define	ED_SB98_CFG_ENABLE	0xa0		/* enable configuration	*/

#define	ED_SB98_EEPENA		2	/* EEPROM access enable		*/

#define	ED_SB98_EEPENA_DISABLE	0x00
#define	ED_SB98_EEPENA_ENABLE	0x01

#define	ED_SB98_EEP		3	/* EEPROM access		*/

#define	ED_SB98_EEP_SDA		0x01		/* Serial Data	*/
#define	ED_SB98_EEP_SCL		0x02		/* Serial Clock	*/
#define	ED_SB98_EEP_READ	0x01		/* Read Command	*/

#define	ED_SB98_EEP_DELAY	300

#define	ED_SB98_ADDRESS		0x01		/* Station Address(1-6)	*/

#define	ED_SB98_POLARITY	4	/* Polarity			*/

/* PC-9801-108 */
static	bus_addr_t ed98_ioaddr_nec108[] = {
	0x0000, 0x0002, 0x0004, 0x0006, 0x0008, 0x000a, 0x000c, 0x000e,
	0x1000, 0x1002, 0x1004, 0x1006, 0x1008, 0x100a, 0x100c, 0x100e
};

/* C-NET(98) */
static	bus_addr_t ed98_ioaddr_cnet98[] = {
	0x0000, 0x0002, 0x0004, 0x0006, 0x0008, 0x000a, 0x000c, 0x000e,
	0x0400, 0x0402, 0x0404, 0x0406, 0x0408, 0x040a, 0x040c, 0x040e
};

/*
 *		Definitions for Contec C-NET(98)
 */
#define	ED_CNET98_MAP_REG0L	0	/* MAPPING register0 Low	*/
#define	ED_CNET98_MAP_REG1L	1	/* MAPPING register1 Low	*/
#define	ED_CNET98_MAP_REG2L	2	/* MAPPING register2 Low	*/
#define	ED_CNET98_MAP_REG3L	3	/* MAPPING register3 Low	*/
#define	ED_CNET98_MAP_REG0H	4	/* MAPPING register0 Hi		*/
#define	ED_CNET98_MAP_REG1H	5	/* MAPPING register1 Hi		*/
#define	ED_CNET98_MAP_REG2H	6	/* MAPPING register2 Hi		*/
#define	ED_CNET98_MAP_REG3H	7	/* MAPPING register3 Hi		*/
#define	ED_CNET98_WIN_REG	8	/* Window register		*/
#define	ED_CNET98_INT_LEV	9	/* Init level register		*/

#define	ED_CNET98_INT_IRQ3	0x01		/* INT 0 */
#define	ED_CNET98_INT_IRQ5	0x02		/* INT 1 */
#define	ED_CNET98_INT_IRQ6	0x04		/* INT 2 */
#define	ED_CNET98_INT_IRQ9	0x08		/* INT 3 */
#define	ED_CNET98_INT_IRQ12	0x20		/* INT 5 */
#define	ED_CNET98_INT_IRQ13	0x40		/* INT 6 */

#define	ED_CNET98_INT_REQ	10	/* Init request register	*/
#define	ED_CNET98_INT_MASK	11	/* Init mask register		*/
#define	ED_CNET98_INT_STAT	12	/* Init status register		*/
#define	ED_CNET98_INT_CLR	12	/* Init clear register		*/
#define	ED_CNET98_RESERVE1	13
#define	ED_CNET98_RESERVE2	14
#define	ED_CNET98_RESERVE3	15

/* EC/EP-98X, NC5098 */
static	bus_addr_t ed98_ioaddr_nw98x[] = {
	0x0000, 0x0100, 0x0200, 0x0300, 0x0400, 0x0500, 0x0600, 0x0700,
	0x0800, 0x0900, 0x0a00, 0x0b00, 0x0c00, 0x0d00, 0x0e00, 0x0f00,
	0x1000	/* for EC/EP-98X(see below) */
};

/*
 *		Definitions for Networld EC/EP-98X
 */
#define	ED_NW98X_IRQ            16	/* Interrupt Configuration Register */

#define	ED_NW98X_IRQ3           0x04
#define	ED_NW98X_IRQ5           0x06
#define	ED_NW98X_IRQ6           0x08
#define	ED_NW98X_IRQ12          0x0a
#define	ED_NW98X_IRQ13          0x02

/* NC5098 ASIC */
static bus_addr_t ed98_asic_nc5098[] = {
/*	DATA    ENADDR						RESET	*/
	0x0000, 0x2000, 0x2100, 0x2200, 0x2300, 0x2400, 0x2500, 0x4000,
	     0,      0,      0,      0,      0,      0,      0,      0 
};

/*
 *		Definitions for NextCom NC5098
 */
#define	ED_NC5098_ENADDR	1	/* Station Address(1-6)		*/

/*
 * Allocate a port resource with the given resource id.
 */
static int
ed98_alloc_port(dev, rid)
	device_t dev;
	int rid;
{
	struct ed_softc *sc = device_get_softc(dev);
	struct resource *res;
	int error;
	bus_addr_t *io_nic, *io_asic, adj;
	static bus_addr_t io_res[ED_NOVELL_IO_PORTS + 1];
	int i, n;
	int offset, reset, data;

	/* Set i/o table for resource manager */
	io_nic = io_asic = ed98_ioaddr_generic;
	offset = ED_NOVELL_ASIC_OFFSET;
	reset = ED_NOVELL_RESET;
	data  = ED_NOVELL_DATA;
	n = ED_NOVELL_IO_PORTS;

	switch (sc->type) {
	case ED_TYPE98_LGY:
		io_asic = ed98_ioaddr_egy98; /* XXX - Yes, we use egy98 */
		offset = 0x0200;
		reset = 8;
		break;

	case ED_TYPE98_EGY:
		io_nic = io_asic = ed98_ioaddr_egy98;
		offset = 0x0200;
		reset = 8;
		break;

	case ED_TYPE98_ICM:
		offset = 0x0100;
		break;

	case ED_TYPE98_BDN:
		io_nic = io_asic = ed98_ioaddr_la98;
		offset = 0x0100;
		reset = 0x0c;
		break;

	case ED_TYPE98_SIC:
		io_nic = io_asic = ed98_ioaddr_sic98;
		offset = 0x2000;
		n = 16+1;
		break;

	case ED_TYPE98_108:
		io_nic = io_asic = ed98_ioaddr_nec108;
		offset = 0x0888;	/* XXX - overwritten after */
		reset = 1;
		n = 16;	/* XXX - does not set ASIC i/o here */
		break;

	case ED_TYPE98_LA98:
		io_nic = io_asic = ed98_ioaddr_la98;
		offset = 0x0100;
		break;

	case ED_TYPE98_CNET98EL:
		offset = 0x0400;
		data = 0x0e;
		break;

	case ED_TYPE98_CNET98:
		/* XXX - Yes, we use generic i/o here */
		offset = 0x0400;
		break;

	case ED_TYPE98_NW98X:
		io_nic = io_asic = ed98_ioaddr_nw98x;
		offset = 0x1000;
		break;

	case ED_TYPE98_SB98:
		io_nic = io_asic = ed98_ioaddr_la98;
		offset = 0x0400;
		reset = 7;
		break;

	case ED_TYPE98_NC5098:
		io_nic  = ed98_ioaddr_nw98x;
		io_asic = ed98_asic_nc5098;
		offset = 0x2000;
		reset = 7;
		n = 16+8;	/* XXX */
		break;
	}

	bcopy(io_nic, io_res, sizeof(io_nic[0]) * ED_NOVELL_ASIC_OFFSET);
	for (i = ED_NOVELL_ASIC_OFFSET; i < ED_NOVELL_IO_PORTS; i++) {
		io_res[i] = io_asic[i - ED_NOVELL_ASIC_OFFSET] + offset;
	}

	res = isa_alloc_resourcev(dev, SYS_RES_IOPORT, &rid,
				  io_res, n, RF_ACTIVE);
	if (!res) {
		return (ENOENT);
	}

	sc->port_rid = rid;
	sc->port_res = res;
	sc->port_used = n;

	/* Re-map i/o table if needed */
	switch (sc->type) {
	case ED_TYPE98_LA98:
	case ED_TYPE98_NW98X:
		io_res[n] = io_asic[n - ED_NOVELL_ASIC_OFFSET] + offset;
		n++;
		break;

	case ED_TYPE98_108:
		adj = (rman_get_start(res) & 0xf000) / 2;
		offset = (offset | adj) - rman_get_start(res);

		for (n = ED_NOVELL_ASIC_OFFSET; n < ED_NOVELL_IO_PORTS; n++) {
			io_res[n] = io_asic[n - ED_NOVELL_ASIC_OFFSET] + offset;
		}
		break;

	case ED_TYPE98_CNET98:
		io_nic = io_asic = ed98_ioaddr_cnet98;
		offset = 1;

		bcopy(io_nic, io_res, sizeof(io_nic[0]) * ED_NOVELL_ASIC_OFFSET);
		for (n = ED_NOVELL_ASIC_OFFSET; n < ED_NOVELL_IO_PORTS; n++) {
			io_res[n] = io_asic[n - ED_NOVELL_ASIC_OFFSET] + offset;
		}
		break;

	case ED_TYPE98_NC5098:
		n = ED_NOVELL_IO_PORTS;
		break;
	}

	if (reset != ED_NOVELL_RESET) {
		io_res[ED_NOVELL_ASIC_OFFSET + ED_NOVELL_RESET] =
			io_res[ED_NOVELL_ASIC_OFFSET + reset];
	}
	if (data  != ED_NOVELL_DATA) {
		io_res[ED_NOVELL_ASIC_OFFSET + ED_NOVELL_DATA] =
			io_res[ED_NOVELL_ASIC_OFFSET + data];
#if 0
		io_res[ED_NOVELL_ASIC_OFFSET + ED_NOVELL_DATA + 1] =
			io_res[ED_NOVELL_ASIC_OFFSET + data + 1];
#endif
	}

	error = isa_load_resourcev(res, io_res, n);
	if (error != 0) {
		return (ENOENT);
	}
#ifdef ED_DEBUG
	device_printf(dev, "ed98_alloc_port: i/o ports = %d\n", n);
	for (i = 0; i < n; i++) {
		printf("%x,", io_res[i]);
	}
	printf("\n");
#endif
	return (0);
}

static int
ed98_alloc_memory(dev, rid)
	device_t dev;
	int rid;
{
	struct ed_softc *sc = device_get_softc(dev);
	int error;
	u_long conf_maddr, conf_msize;

	error = bus_get_resource(dev, SYS_RES_MEMORY, 0,
				 &conf_maddr, &conf_msize);
	if (error) {
		return (error);
	}

	if ((conf_maddr == 0) || (conf_msize == 0)) {
		return (ENXIO);
	}

	error = ed_alloc_memory(dev, rid, (int) conf_msize);
	if (error) {
		return (error);
	}

	sc->mem_start = (caddr_t) rman_get_virtual(sc->mem_res);
	sc->mem_size  = conf_msize;

	return (0);
}

/*
 * Generic probe routine for testing for the existance of a DS8390.
 *	Must be called after the NIC has just been reset. This routine
 *	works by looking at certain register values that are guaranteed
 *	to be initialized a certain way after power-up or reset. Seems
 *	not to currently work on the 83C690.
 *
 * Specifically:
 *
 *	Register			reset bits	set bits
 *	Command Register (CR)		TXP, STA	RD2, STP
 *	Interrupt Status (ISR)				RST
 *	Interrupt Mask (IMR)		All bits
 *	Data Control (DCR)				LAS
 *	Transmit Config. (TCR)		LB1, LB0
 *
 * XXX - We only check the CR register.
 *
 * Return 1 if 8390 was found, 0 if not.
 */

static int
ed98_probe_generic8390(sc)
	struct ed_softc *sc;
{
	u_char tmp = ed_nic_inb(sc, ED_P0_CR);
#ifdef DIAGNOSTIC
	printf("ed?: inb(ED_P0_CR)=%x\n", tmp);
#endif
	if ((tmp & (ED_CR_RD2 | ED_CR_TXP | ED_CR_STA | ED_CR_STP)) !=
	    (ED_CR_RD2 | ED_CR_STP)) {
		return (0);
	}

	(void) ed_nic_inb(sc, ED_P0_ISR);

	return (1);
}

static int
ed98_probe_Novell(dev, port_rid, flags)
	device_t dev;
	int port_rid;
	int flags;
{
	struct ed_softc *sc = device_get_softc(dev);
	int error;
	int n;
	u_char romdata[ETHER_ADDR_LEN * 2], tmp;

#ifdef ED_DEBUG
	device_printf(dev, "ed98_probe_Novell: start\n");
#endif
	error = ed98_alloc_port(dev, port_rid);
	if (error) {
		return (error);
	}

	sc->asic_offset = ED_NOVELL_ASIC_OFFSET;
	sc->nic_offset  = ED_NOVELL_NIC_OFFSET;

	/* Reset the board */
#ifdef ED_DEBUG
	device_printf(dev, "ed98_probe_Novell: reset\n");
#endif
	switch (sc->type) {
#if 1	/* XXX - I'm not sure this is really necessary... */
	case ED_TYPE98_BDN:
		tmp = ed_asic_inb(sc, ED_NOVELL_RESET);
		ed_asic_outb(sc, ED_NOVELL_RESET, (tmp & 0xf0) | 0x08);
		ed_nic_outb(sc, 0x04, tmp);
		(void) ed_asic_inb(sc, 0x08);
		ed_asic_outb(sc, 0x08, tmp);
		ed_asic_outb(sc, 0x08, tmp & 0x7f);
		break;
#endif
	case ED_TYPE98_NC5098:
		ed_asic_outb(sc, ED_NOVELL_RESET, 0x00);
		DELAY(5000);
		ed_asic_outb(sc, ED_NOVELL_RESET, 0x01);
		break;

	default:
		tmp = ed_asic_inb(sc, ED_NOVELL_RESET);

	/*
	 * I don't know if this is necessary; probably cruft leftover from
	 * Clarkson packet driver code. Doesn't do a thing on the boards I've
	 * tested. -DG [note that a outb(0x84, 0) seems to work here, and is
	 * non-invasive...but some boards don't seem to reset and I don't have
	 * complete documentation on what the 'right' thing to do is...so we
	 * do the invasive thing for now. Yuck.]
	 */
		ed_asic_outb(sc, ED_NOVELL_RESET, tmp);
		break;
	}
	DELAY(5000);

	/*
	 * This is needed because some NE clones apparently don't reset the
	 * NIC properly (or the NIC chip doesn't reset fully on power-up) XXX
	 * - this makes the probe invasive! ...Done against my better
	 * judgement. -DLG
	 */
	ed_nic_outb(sc, ED_P0_CR, ED_CR_RD2 | ED_CR_STP);
	DELAY(5000);

	/* Make sure that we really have an 8390 based board */
	if (!ed98_probe_generic8390(sc)) {
		return (ENXIO);
	}

	/* Test memory via PIO */
#ifdef ED_DEBUG
	device_printf(dev, "ed98_probe_Novell: test memory\n");
#endif
	sc->cr_proto = ED_CR_RD2;
	if (!ed_pio_testmem(sc,  8192, 0, flags)
	&&  !ed_pio_testmem(sc, 16384, 1, flags)) {
		return (ENXIO);
	}

	/* Setup the board type */
#ifdef ED_DEBUG
	device_printf(dev, "ed98_probe_Novell: board type\n");
#endif
	switch (sc->type) {
	case ED_TYPE98_BDN:
		sc->type_str = "LD-BDN";
		break;
	case ED_TYPE98_EGY:
		sc->type_str = "EGY-98";
		break;
	case ED_TYPE98_LGY:
		sc->type_str = "LGY-98";
		break;
	case ED_TYPE98_ICM:
		sc->type_str = "ICM";
		break;
	case ED_TYPE98_108:
		sc->type_str = "PC-9801-108";
		break;
	case ED_TYPE98_LA98:
		sc->type_str = "LA-98";
		break;
	case ED_TYPE98_NW98X:
		sc->type_str = "NW98X";
		break;
	case ED_TYPE98_NC5098:
		sc->type_str = "NC5098";
		break;
	default:
		sc->type_str = NULL;
		break;
	}

	/* Get station address */
	switch (sc->type) {
	case ED_TYPE98_NC5098:
		for (n = 0; n < ETHER_ADDR_LEN; n++) {
			sc->arpcom.ac_enaddr[n] =
				ed_asic_inb(sc, ED_NC5098_ENADDR + n);
		}
		break;

	default:
		ed_pio_readmem(sc, 0, romdata, sizeof(romdata));
		for (n = 0; n < ETHER_ADDR_LEN; n++) {
			sc->arpcom.ac_enaddr[n] =
				romdata[n * (sc->isa16bit + 1)];
		}
		break;
	}

	/* clear any pending interrupts that might have occurred above */
	ed_nic_outb(sc, ED_P0_ISR, 0xff);

	return (0);
}

/*
 * Probe and vendor-specific initialization routine for SIC-98 boards
 */
static int
ed_probe_SIC98(dev, port_rid, flags)
	device_t dev;
	int port_rid;
	int flags;
{
	struct ed_softc *sc = device_get_softc(dev);
	int error;
	int i;
	u_char sum;

	/*
	 * Setup card RAM and I/O address
	 * Kernel Virtual to segment C0000-DFFFF????
	 */
	error = ed98_alloc_port(dev, port_rid);
	if (error) {
		return (error);
	}

	sc->asic_offset = ED_NOVELL_ASIC_OFFSET;
	sc->nic_offset  = ED_NOVELL_NIC_OFFSET;

	error = ed98_alloc_memory(dev, 0);
	if (error) {
		return (error);
	}

	/* Reset card to force it into a known state. */
	ed_asic_outb(sc, 0, 0x00);
	DELAY(100);
	if (ED_TYPE98SUB(flags) == 0) {
		/* SIC-98/SIU-98 */
		ed_asic_outb(sc, 0, 0x94);
		DELAY(100);
		ed_asic_outb(sc, 0, 0x94);
	} else {
		/* SIU-98-D */
		ed_asic_outb(sc, 0, 0x80);
		DELAY(100);
		ed_asic_outb(sc, 0, 0x94);
		DELAY(100);
		ed_asic_outb(sc, 0, 0x9e);
	}
	DELAY(100);

	/*
	 * Here we check the card ROM, if the checksum passes, and the
	 * type code and ethernet address check out, then we know we have
	 * an SIC card.
	 */
	sum = sc->mem_start[6 * 2];
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		sum ^= (sc->arpcom.ac_enaddr[i] = sc->mem_start[i * 2]);
	}
#ifdef ED_DEBUG
	device_printf(dev, "ed_probe_sic98: got address %6D\n",
		      sc->arpcom.ac_enaddr, ":");
#endif
	if (sum != 0) {
		return (ENXIO);
	}
	if ((sc->arpcom.ac_enaddr[0] | sc->arpcom.ac_enaddr[1] |
	     sc->arpcom.ac_enaddr[2]) == 0) {
		return (ENXIO);
	}

	sc->vendor   = ED_VENDOR_MISC;
	sc->type_str = "SIC98";
	sc->isa16bit = 1;
	sc->cr_proto = 0;

	/*
	 * SIC RAM page 0x0000-0x3fff(or 0x7fff)
	 */
	if (ED_TYPE98SUB(flags) == 0) {
		ed_asic_outb(sc, 0, 0x90);
	} else {
		ed_asic_outb(sc, 0, 0x8e);
	}
	DELAY(100);

	/*
	 * clear interface memory, then sum to make sure its valid
	 */
	bzero(sc->mem_start, sc->mem_size);

	for (i = 0; i < sc->mem_size; i++) {
		if (sc->mem_start[i]) {
			device_printf(dev, "failed to clear shared memory "
				"at %lx - check configuration\n",
				kvtop(sc->mem_start + i));

			return (ENXIO);
		}
	}

	sc->mem_shared = 1;
	sc->mem_end = sc->mem_start + sc->mem_size;

	/*
	 * allocate one xmit buffer if < 16k, two buffers otherwise
	 */
	if ((sc->mem_size < 16384) || (flags & ED_FLAGS_NO_MULTI_BUFFERING)) {
		sc->txb_cnt = 1;
	} else {
		sc->txb_cnt = 2;
	}
	sc->tx_page_start = 0;

	sc->rec_page_start = sc->tx_page_start + ED_TXBUF_SIZE * sc->txb_cnt;
	sc->rec_page_stop = sc->tx_page_start + sc->mem_size / ED_PAGE_SIZE;

	sc->mem_ring = sc->mem_start + sc->txb_cnt * ED_PAGE_SIZE * ED_TXBUF_SIZE;

	return (0);
}

/*
 * Contec C-NET(98) series support routines
 */
static void
ed_reset_CNET98(sc, flags)
	struct ed_softc *sc;
	int flags;
{
	u_short	init_addr = ED_CNET98_INIT;
	u_char tmp;

	/* Choose initial register address */
	if (ED_TYPE98SUB(flags) != 0) {
		init_addr = ED_CNET98_INIT2;
	}
#ifdef ED_DEBUG
	printf("ed?: initial register=%x\n", init_addr);
#endif
	/*
	 * Reset the board to force it into a known state.
	 */
	outb(init_addr, 0x00);	/* request */
	DELAY(5000);
	outb(init_addr, 0x01);	/* cancel */
	DELAY(5000);

	/*
	 * Set I/O address(A15-12) and cpu type
	 *
	 *   AAAAIXXC(8bit)
	 *   AAAA: A15-A12,  I: I/O enable, XX: reserved, C: CPU type
	 *
	 * CPU type is 1:80286 or higher, 0:not.
	 * But FreeBSD runs under i386 or higher, thus it must be 1.
	 */
	tmp = (rman_get_start(sc->port_res) & 0xf000) >> 8;
	tmp |= (0x08 | 0x01);
#ifdef ED_DEBUG
	printf("ed?: outb(%x, %x)\n", init_addr + 2, tmp);
#endif
	outb(init_addr + 2, tmp);
	DELAY(5000);

	/*
	 * This is needed because some NE clones apparently don't reset the
	 * NIC properly (or the NIC chip doesn't reset fully on power-up) XXX
	 * - this makes the probe invasive! ...Done against my better
	 * judgement. -DLG
	 */
	ed_nic_outb(sc, ED_P0_CR, ED_CR_RD2 | ED_CR_STP);
	DELAY(5000);
}

static void
ed_winsel_CNET98(sc, bank)
	struct ed_softc *sc;
	u_short bank;
{
	u_char mem = (kvtop(sc->mem_start) >> 12) & 0xff;

	/*
	 * Disable window memory
	 *    bit7 is 0:disable
	 */
	ed_asic_outb(sc, ED_CNET98_WIN_REG, mem & 0x7f);
	DELAY(10);

	/*
	 * Select window address
	 *    FreeBSD address 0xf00xxxxx
	 */
	ed_asic_outb(sc, ED_CNET98_MAP_REG0L, bank & 0xff);
	DELAY(10);
	ed_asic_outb(sc, ED_CNET98_MAP_REG0H, (bank >> 8) & 0xff);
	DELAY(10);
	ed_asic_outb(sc, ED_CNET98_MAP_REG1L, 0x00);
	DELAY(10);
	ed_asic_outb(sc, ED_CNET98_MAP_REG1H, 0x41);
	DELAY(10);
	ed_asic_outb(sc, ED_CNET98_MAP_REG2L, 0x00);
	DELAY(10);
	ed_asic_outb(sc, ED_CNET98_MAP_REG2H, 0x42);
	DELAY(10);
	ed_asic_outb(sc, ED_CNET98_MAP_REG3L, 0x00);
	DELAY(10);
	ed_asic_outb(sc, ED_CNET98_MAP_REG3H, 0x43);
	DELAY(10);

	/*
	 * Enable window memory(16Kbyte)
	 *    bit7 is 1:enable
	 */
#ifdef ED_DEBUG
	printf("ed?: window start address=%x\n", mem);
#endif
	ed_asic_outb(sc, ED_CNET98_WIN_REG, mem);
	DELAY(10);
}

/*
 * Probe and vendor-specific initialization routine for C-NET(98) boards
 */
static int
ed_probe_CNET98(dev, port_rid, flags)
	device_t dev;
	int port_rid;
	int flags;
{
	struct ed_softc *sc = device_get_softc(dev);
	int error;
	u_char tmp;
	u_long conf_irq, junk;
	int i;
#ifdef DIAGNOSTIC
	u_char tmp_s;
#endif

	error = ed98_alloc_port(dev, port_rid);
	if (error) {
		return (error);
	}

	sc->asic_offset = ED_NOVELL_ASIC_OFFSET;
	sc->nic_offset  = ED_NOVELL_NIC_OFFSET;

	error = ed98_alloc_memory(dev, 0);
	if (error) {
		return (error);
	}

	/* Check I/O address. 0x[a-f]3d0 are allowed. */
	if (((rman_get_start(sc->port_res) & 0x0fff) != 0x03d0)
	||  ((rman_get_start(sc->port_res) & 0xf000) < (u_short) 0xa000)) {
#ifdef DIAGNOSTIC
		device_printf(dev, "Invalid i/o port configuration (0x%x) "
			"must be %s for %s\n", rman_get_start(sc->port_res),
			"0x[a-f]3d0", "CNET98");
#endif
		return (ENXIO);
	}

#ifdef DIAGNOSTIC
	/* Check window area address */
	tmp_s = kvtop(sc->mem_start) >> 12;
	if (tmp_s < 0x80) {
		device_printf(dev, "Please change window address(0x%x)\n",
			kvtop(sc->mem_start));
		return (ENXIO);
	}

	tmp_s &= 0x0f;
	tmp    = rman_get_start(sc->port_res) >> 12;
	if ((tmp_s <= tmp) && (tmp < (tmp_s + 4))) {
		device_printf(dev, "Please change iobase address(0x%x) "
			"or window address(0x%x)\n",
	   		rman_get_start(sc->port_res), kvtop(sc->mem_start));
		return (ENXIO);
	}
#endif
	/* Reset the board */
	ed_reset_CNET98(sc, flags);

	/*
	 * This is needed because some NE clones apparently don't reset the
	 * NIC properly (or the NIC chip doesn't reset fully on power-up) XXX
	 * - this makes the probe invasive! ...Done against my better
	 * judgement. -DLG
	 */
	ed_nic_outb(sc, ED_P0_CR, ED_CR_RD2 | ED_CR_STP);
	DELAY(5000);

	/* Make sure that we really have an 8390 based board */
	if (!ed98_probe_generic8390(sc)) {
		return (ENXIO);
	}

	/*
	 *  Set window ethernet address area
	 *    board memory base 0x480000  data 256byte
	 */
	ed_winsel_CNET98(sc, 0x4800);

	/*
	 * Get station address from on-board ROM
	 */
	bcopy(sc->mem_start, sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	sc->vendor    = ED_VENDOR_MISC;
	sc->type_str  = "CNET98";
	sc->isa16bit  = 0;
	sc->cr_proto  = ED_CR_RD2;

	/*
	 * Set window buffer memory area
	 *    board memory base 0x400000  data 16kbyte
	 */
	ed_winsel_CNET98(sc, 0x4000);

	/*
	 * clear interface memory, then sum to make sure its valid
	 */
	bzero(sc->mem_start, sc->mem_size);

	for (i = 0; i < sc->mem_size; i++) {
		if (sc->mem_start[i]) {
			device_printf(dev, "failed to clear shared memory "
				"at %lx - check configuration\n",
				kvtop(sc->mem_start + i));

			return (ENXIO);
		}
	}

	sc->mem_shared = 1;
	sc->mem_end = sc->mem_start + sc->mem_size;

	sc->txb_cnt = 1;	/* XXX */
	sc->tx_page_start = 0;

	sc->rec_page_start = sc->tx_page_start + ED_TXBUF_SIZE;
	sc->rec_page_stop = sc->tx_page_start + sc->mem_size / ED_PAGE_SIZE;

	sc->mem_ring = sc->mem_start + ED_PAGE_SIZE * ED_TXBUF_SIZE;

	/*
	 *   Set interrupt level
	 */
	error = bus_get_resource(dev, SYS_RES_IRQ, 0,
				 &conf_irq, &junk);
	if (error)
		return (error);

	switch (conf_irq) {
	case 3:
		tmp = ED_CNET98_INT_IRQ3;
		break;
	case 5:
		tmp = ED_CNET98_INT_IRQ5;
		break;
	case 6:
		tmp = ED_CNET98_INT_IRQ6;
		break;
	case 9:
		tmp = ED_CNET98_INT_IRQ9;
		break;
	case 12:
		tmp = ED_CNET98_INT_IRQ12;
		break;
	case 13:
		tmp = ED_CNET98_INT_IRQ13;
		break;
	default:
		device_printf(dev, "Invalid irq configuration (%ld) must be "
			"%s for %s\n", conf_irq, "3,5,6,9,12,13", "CNET98");
		return (ENXIO);
	}
	ed_asic_outb(sc, ED_CNET98_INT_LEV, tmp);
	DELAY(1000);
	/*
	 *   Set interrupt mask.
	 *     bit7:1 all interrupt mask
	 *     bit1:1 timer interrupt mask
	 *     bit0:0 NS controler interrupt enable
	 */
	ed_asic_outb(sc, ED_CNET98_INT_MASK, 0x7e);
	DELAY(1000);

	return (0);
}

/*
 * Probe and vendor-specific initialization routine for C-NET(98)E/L boards
 */
static int
ed_probe_CNET98EL(dev, port_rid, flags)
	device_t dev;
	int port_rid;
	int flags;
{
	struct ed_softc *sc = device_get_softc(dev);
	int error;
	int i;
	u_char romdata[ETHER_ADDR_LEN * 2], tmp;
	u_long conf_irq, junk;

	error = ed98_alloc_port(dev, port_rid);
	if (error) {
		return (error);
	}

	sc->asic_offset = ED_NOVELL_ASIC_OFFSET;
	sc->nic_offset  = ED_NOVELL_NIC_OFFSET;

	/* Check I/O address. 0x[0-f]3d0 are allowed. */
	if ((rman_get_start(sc->port_res) & 0x0fff) != 0x03d0) {
#ifdef DIAGNOSTIC
		device_printf(dev, "Invalid i/o port configuration (0x%x) "
			"must be %s for %s\n", rman_get_start(sc->port_res),
			"0x?3d0", "CNET98E/L");
#endif
		return (ENXIO);
	}

	/* Reset the board */
	ed_reset_CNET98(sc, flags);

	/*
	 * This is needed because some NE clones apparently don't reset the
	 * NIC properly (or the NIC chip doesn't reset fully on power-up) XXX
	 * - this makes the probe invasive! ...Done against my better
	 * judgement. -DLG
	 */
	ed_nic_outb(sc, ED_P0_CR, ED_CR_RD2 | ED_CR_STP);
	DELAY(5000);

	/* Make sure that we really have an 8390 based board */
	if (!ed98_probe_generic8390(sc)) {
		return (ENXIO);
	}

	/* Test memory via PIO */
	sc->cr_proto = ED_CR_RD2;
	if (!ed_pio_testmem(sc, ED_CNET98EL_PAGE_OFFSET, 1, flags)) {
		return (ENXIO);
	}

	/* This looks like a C-NET(98)E/L board. */
	sc->type_str = "CNET98E/L";

	/*
	 * Set IRQ. C-NET(98)E/L only allows a choice of irq 3,5,6.
	 */
	error = bus_get_resource(dev, SYS_RES_IRQ, 0,
				 &conf_irq, &junk);
	if (error) {
		return (error);
	}

	switch (conf_irq) {
	case 3:
		tmp = ED_CNET98EL_ICR_IRQ3;
		break;
	case 5:
		tmp = ED_CNET98EL_ICR_IRQ5;
		break;
	case 6:
		tmp = ED_CNET98EL_ICR_IRQ6;
		break;
#if 0
	case 12:
		tmp = ED_CNET98EL_ICR_IRQ12;
		break;
#endif
	default:
		device_printf(dev, "Invalid irq configuration (%ld) must be "
			"%s for %s\n", conf_irq, "3,5,6", "CNET98E/L");
		return (ENXIO);
	}
	ed_asic_outb(sc, ED_CNET98EL_ICR, tmp);
	ed_asic_outb(sc, ED_CNET98EL_IMR, 0x7e);

	/* Get station address from on-board ROM */
	ed_pio_readmem(sc, 16384, romdata, sizeof(romdata));
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		sc->arpcom.ac_enaddr[i] = romdata[i * 2];
	}

	/* clear any pending interrupts that might have occurred above */
	ed_nic_outb(sc, ED_P0_ISR, 0xff);

	return (0);
}

/*
 * Probe and vendor-specific initialization routine for PC-9801-77 boards
 */
static int
ed_probe_NEC77(dev, port_rid, flags)
	device_t dev;
	int port_rid;
	int flags;
{
	struct ed_softc *sc = device_get_softc(dev);
	int error;
	u_char tmp;
	u_long conf_irq, junk;

	error = ed98_probe_Novell(dev, port_rid, flags);
	if (error) {
		return (error);
	}

	/* LA/T-98 does not need IRQ setting. */
	if (ED_TYPE98SUB(flags) == 0) {
		return (0);
	}

	/*
	 * Set IRQ. PC-9801-77 only allows a choice of irq 3,5,6,12,13.
	 */
	error = bus_get_resource(dev, SYS_RES_IRQ, 0,
				 &conf_irq, &junk);
	if (error) {
		return (error);
	}

	switch (conf_irq) {
	case 3:
		tmp = ED_NEC77_IRQ3;
		break;
	case 5:
		tmp = ED_NEC77_IRQ5;
		break;
	case 6:
		tmp = ED_NEC77_IRQ6;
		break;
	case 12:
		tmp = ED_NEC77_IRQ12;
		break;
	case 13:
		tmp = ED_NEC77_IRQ13;
		break;
	default:
		device_printf(dev, "Invalid irq configuration (%ld) must be "
			"%s for %s\n", conf_irq, "3,5,6,12,13", "PC-9801-77");
		return (ENXIO);
	}
	ed_asic_outb(sc, ED_NEC77_IRQ, tmp);

	return (0);
}

/*
 * Probe and vendor-specific initialization routine for EC/EP-98X boards
 */
static int
ed_probe_NW98X(dev, port_rid, flags)
	device_t dev;
	int port_rid;
	int flags;
{
	struct ed_softc *sc = device_get_softc(dev);
	int error;
	u_char tmp;
	u_long conf_irq, junk;

	error = ed98_probe_Novell(dev, port_rid, flags);
	if (error) {
		return (error);
	}

	/* Networld 98X3 does not need IRQ setting. */
	if (ED_TYPE98SUB(flags) == 0) {
		return (0);
	}

	/*
	 * Set IRQ. EC/EP-98X only allows a choice of irq 3,5,6,12,13.
	 */
	error = bus_get_resource(dev, SYS_RES_IRQ, 0,
				 &conf_irq, &junk);
	if (error) {
		return (error);
	}

	switch (conf_irq) {
	case 3:
		tmp = ED_NW98X_IRQ3;
		break;
	case 5:
		tmp = ED_NW98X_IRQ5;
		break;
	case 6:
		tmp = ED_NW98X_IRQ6;
		break;
	case 12:
		tmp = ED_NW98X_IRQ12;
		break;
	case 13:
		tmp = ED_NW98X_IRQ13;
		break;
	default:
		device_printf(dev, "Invalid irq configuration (%ld) must be "
			"%s for %s\n", conf_irq, "3,5,6,12,13", "EC/EP-98X");
		return (ENXIO);
	}
	ed_asic_outb(sc, ED_NW98X_IRQ, tmp);

	return (0);
}

/*
 * Read SB-9801 station address from Serial Two-Wire EEPROM
 */
static void
ed_get_SB98(sc)
	struct ed_softc *sc;
{
	int i, j;
	u_char mask, val;

        /* enable EEPROM acceess */
        ed_asic_outb(sc, ED_SB98_EEPENA, ED_SB98_EEPENA_ENABLE);

	/* output start command */
	ed_asic_outb(sc, ED_SB98_EEP, ED_SB98_EEP_SDA | ED_SB98_EEP_SCL);
	DELAY(ED_SB98_EEP_DELAY);
	ed_asic_outb(sc, ED_SB98_EEP, ED_SB98_EEP_SCL);
	DELAY(ED_SB98_EEP_DELAY);

       	/* output address (7bit) */
	for (mask = 0x40; mask != 0; mask >>= 1) {
		val = 0;
		if (ED_SB98_ADDRESS & mask)
			val = ED_SB98_EEP_SDA;
		ed_asic_outb(sc, ED_SB98_EEP, val);
		DELAY(ED_SB98_EEP_DELAY);
		ed_asic_outb(sc, ED_SB98_EEP, val | ED_SB98_EEP_SCL);
		DELAY(ED_SB98_EEP_DELAY);
	}

	/* output READ command */
	ed_asic_outb(sc, ED_SB98_EEP, ED_SB98_EEP_READ);
	DELAY(ED_SB98_EEP_DELAY);
	ed_asic_outb(sc, ED_SB98_EEP, ED_SB98_EEP_READ | ED_SB98_EEP_SCL);
	DELAY(ED_SB98_EEP_DELAY);

	/* read station address */
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		/* output ACK */
		ed_asic_outb(sc, ED_SB98_EEP, 0);
		DELAY(ED_SB98_EEP_DELAY);
		ed_asic_outb(sc, ED_SB98_EEP, ED_SB98_EEP_SCL);
		DELAY(ED_SB98_EEP_DELAY);

		val = 0;
		for (j = 0; j < 8; j++) {
			ed_asic_outb(sc, ED_SB98_EEP, ED_SB98_EEP_SDA);
			DELAY(ED_SB98_EEP_DELAY);
			ed_asic_outb(sc, ED_SB98_EEP, ED_SB98_EEP_SDA | ED_SB98_EEP_SCL);
			DELAY(ED_SB98_EEP_DELAY);
			val <<= 1;
			val |= (ed_asic_inb(sc, ED_SB98_EEP) & ED_SB98_EEP_SDA);
			DELAY(ED_SB98_EEP_DELAY);
	  	}
		sc->arpcom.ac_enaddr[i] = val;
	}

	/* output Last ACK */
        ed_asic_outb(sc, ED_SB98_EEP, ED_SB98_EEP_SDA);
        DELAY(ED_SB98_EEP_DELAY);
        ed_asic_outb(sc, ED_SB98_EEP, ED_SB98_EEP_SDA | ED_SB98_EEP_SCL);
        DELAY(ED_SB98_EEP_DELAY);

	/* output stop command */
	ed_asic_outb(sc, ED_SB98_EEP, ED_SB98_EEP_SCL);
	DELAY(ED_SB98_EEP_DELAY);
	ed_asic_outb(sc, ED_SB98_EEP, ED_SB98_EEP_SDA | ED_SB98_EEP_SCL);
	DELAY(ED_SB98_EEP_DELAY);

	/* disable EEPROM access */
	ed_asic_outb(sc, ED_SB98_EEPENA, ED_SB98_EEPENA_DISABLE);
}

/*
 * Probe and vendor-specific initialization routine for SB-9801 boards
 */
static int
ed_probe_SB98(dev, port_rid, flags)
	device_t dev;
	int port_rid;
	int flags;
{
	struct ed_softc *sc = device_get_softc(dev);
	int error;
	u_char tmp;
	u_long conf_irq, junk;

	error = ed98_alloc_port(dev, port_rid);
	if (error) {
		return (error);
	}

	sc->asic_offset = ED_NOVELL_ASIC_OFFSET;
	sc->nic_offset  = ED_NOVELL_NIC_OFFSET;

	/* Check I/O address. 00d[02468ace] are allowed. */
	if ((rman_get_start(sc->port_res) & ~0x000e) != 0x00d0) {
#ifdef DIAGNOSTIC
		device_printf(dev, "Invalid i/o port configuration (0x%x) "
			"must be %s for %s\n", rman_get_start(sc->port_res),
			"0xd?", "SB9801");
#endif
		return (ENXIO);
	}

	/* Write I/O port address and read 4 times */
	outb(ED_SB98_IO_INHIBIT, rman_get_start(sc->port_res) & 0xff);
	(void) inb(ED_SB98_IO_INHIBIT); DELAY(300);
	(void) inb(ED_SB98_IO_INHIBIT); DELAY(300);
	(void) inb(ED_SB98_IO_INHIBIT); DELAY(300);
	(void) inb(ED_SB98_IO_INHIBIT); DELAY(300);

	/*
	 * Check IRQ. Soliton SB-9801 only allows a choice of
	 * irq 3,5,6,12
	 */
	error = bus_get_resource(dev, SYS_RES_IRQ, 0,
				 &conf_irq, &junk);
	if (error) {
		return (error);
	}

	switch (conf_irq) {
	case 3:
		tmp = ED_SB98_CFG_IRQ3;
		break;
	case 5:
		tmp = ED_SB98_CFG_IRQ5;
		break;
	case 6:
		tmp = ED_SB98_CFG_IRQ6;
		break;
	case 12:
		tmp = ED_SB98_CFG_IRQ12;
		break;
	default:
		device_printf(dev, "Invalid irq configuration (%ld) must be "
			"%s for %s\n", conf_irq, "3,5,6,12", "SB9801");
		return (ENXIO);
	}

	if (flags & ED_FLAGS_DISABLE_TRANCEIVER) {
		tmp |= ED_SB98_CFG_ALTPORT;
	}
	ed_asic_outb(sc, ED_SB98_CFG, ED_SB98_CFG_ENABLE | tmp);
	ed_asic_outb(sc, ED_SB98_POLARITY, 0x01);

	/* Reset the board. */
	ed_asic_outb(sc, ED_NOVELL_RESET, 0x7a);
	DELAY(300);
	ed_asic_outb(sc, ED_NOVELL_RESET, 0x79);
	DELAY(300);

	/*
	 * This is needed because some NE clones apparently don't reset the
	 * NIC properly (or the NIC chip doesn't reset fully on power-up) XXX
	 * - this makes the probe invasive! ...Done against my better
	 * judgement. -DLG
	 */
	ed_nic_outb(sc, ED_P0_CR, ED_CR_RD2 | ED_CR_STP);
	DELAY(5000);

	/* Make sure that we really have an 8390 based board */
	if (!ed98_probe_generic8390(sc)) {
		return (ENXIO);
	}

	/* Test memory via PIO */
	sc->cr_proto = ED_CR_RD2;
	if (!ed_pio_testmem(sc, 16384, 1, flags)) {
		return (ENXIO);
	}

	/* This looks like an SB9801 board. */
	sc->type_str = "SB9801";

	/* Get station address */
	ed_get_SB98(sc);

	/* clear any pending interrupts that might have occurred above */
	ed_nic_outb(sc, ED_P0_ISR, 0xff);

	return (0);
}

/*
 * Test the ability to read and write to the NIC memory.
 */
static int
ed_pio_testmem(sc, page_offset, isa16bit, flags)
	struct ed_softc *sc;
	int page_offset;
	int isa16bit;
	int flags;
{
	u_long memsize;
	static char test_pattern[32] = "THIS is A memory TEST pattern";
	char test_buffer[32];
#ifdef DIAGNOSTIC
	int page_end;
#endif

	sc->vendor = ED_VENDOR_NOVELL;
	sc->mem_shared = 0;
	sc->isa16bit = isa16bit;

	/* 8k of memory plus an additional 8k if 16bit */
	memsize = (isa16bit ? 16384 : 8192);

	/*
	 * This prevents packets from being stored in the NIC memory when the
	 * readmem routine turns on the start bit in the CR.
	 */
	ed_nic_outb(sc, ED_P0_RCR, ED_RCR_MON);

	/* Initialize DCR for byte/word operations */
	if (isa16bit) {
		ed_nic_outb(sc, ED_P0_DCR, ED_DCR_WTS | ED_DCR_FT1 | ED_DCR_LS);
	} else {
		ed_nic_outb(sc, ED_P0_DCR, ED_DCR_FT1 | ED_DCR_LS);
	}
	ed_nic_outb(sc, ED_P0_PSTART, page_offset / ED_PAGE_SIZE);
	ed_nic_outb(sc, ED_P0_PSTOP, (page_offset + memsize) / ED_PAGE_SIZE);
#ifdef ED_DEBUG
	printf("ed?: ed_pio_testmem: page start=%x, end=%x",
		      page_offset, page_offset + memsize);
#endif

	/*
	 * Write a test pattern. If this fails, then we don't know
	 * what this board is.
	 */
	ed_pio_writemem(sc, test_pattern, page_offset, sizeof(test_pattern));
	ed_pio_readmem(sc, page_offset, test_buffer, sizeof(test_pattern));

	if (bcmp(test_pattern, test_buffer, sizeof(test_pattern))) {
#ifdef ED_DEBUG
		printf("ed?: ed_pio_testmem: bcmp(page %x) NG",
			      page_offset);
#endif
		return (0);
	}

#ifdef DIAGNOSTIC
	/* Check the bottom. */
	page_end = page_offset + memsize - ED_PAGE_SIZE;
	ed_pio_writemem(sc, test_pattern, page_end, sizeof(test_pattern));
	ed_pio_readmem(sc, page_end, test_buffer, sizeof(test_pattern));

	if (bcmp(test_pattern, test_buffer, sizeof(test_pattern))) {
#ifdef ED_DEBUG
		printf("ed?: ed_pio_testmem: bcmp(page %x) NG",
			      page_end);
#endif
		return (0);
	}
#endif
	sc->mem_size = memsize;
	sc->mem_start = (char *) page_offset;
	sc->mem_end   = sc->mem_start + memsize;
	sc->tx_page_start = page_offset / ED_PAGE_SIZE;

	/*
	 * Use one xmit buffer if < 16k, two buffers otherwise (if not told
	 * otherwise).
	 */
	if ((memsize < 16384) || (flags & ED_FLAGS_NO_MULTI_BUFFERING)) {
		sc->txb_cnt = 1;
	} else {
		sc->txb_cnt = 2;
	}

	sc->rec_page_start = sc->tx_page_start + sc->txb_cnt * ED_TXBUF_SIZE;
	sc->rec_page_stop  = sc->tx_page_start + memsize / ED_PAGE_SIZE;

	sc->mem_ring = sc->mem_start + sc->txb_cnt * ED_PAGE_SIZE * ED_TXBUF_SIZE;

	return (1);
}
#endif	/* PC98 */
