/*-
 * All Rights Reserved, Copyright (C) Fujitsu Limited 1995
 *
 * This software may be used, modified, copied, distributed, and sold, in
 * both source and binary form provided that the above copyright, these
 * terms and the following disclaimer are retained.  The name of the author
 * and/or the contributor may not be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND THE CONTRIBUTOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR THE CONTRIBUTOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION.
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
#include <sys/rman.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_mib.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <dev/fe/mb86960.h>
#include <dev/fe/if_fereg.h>
#include <dev/fe/if_fevar.h>

#include <isa/isavar.h>

/*
 *	Cbus specific code.
 */
static int fe_isa_probe(device_t);
static int fe_isa_attach(device_t);

static struct isa_pnp_id fe_ids[] = {
	{ 0x101ee0d,	NULL },		/* CON0101 - Contec C-NET(98)P2-T */
	{ 0,		NULL }
};

static device_method_t fe_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		fe_isa_probe),
	DEVMETHOD(device_attach,	fe_isa_attach),

	{ 0, 0 }
};

static driver_t fe_isa_driver = {
	"fe",
	fe_isa_methods,
	sizeof (struct fe_softc)
};

DRIVER_MODULE(fe, isa, fe_isa_driver, fe_devclass, 0, 0);


static int fe98_alloc_port(device_t, int);

static int fe_probe_re1000(device_t);
static int fe_probe_cnet9ne(device_t);
static int fe_probe_rex(device_t);
static int fe_probe_ssi(device_t);
static int fe_probe_jli(device_t);
static int fe_probe_lnx(device_t);
static int fe_probe_gwy(device_t);
static int fe_probe_ubn(device_t);

/*
 * Determine if the device is present at a specified I/O address.  The
 * main entry to the driver.
 */
static int
fe_isa_probe(device_t dev)
{
	struct fe_softc *sc;
	int error;

	/* Prepare for the softc struct.  */
	sc = device_get_softc(dev);
	sc->sc_unit = device_get_unit(dev);

	/* Check isapnp ids */
	error = ISA_PNP_PROBE(device_get_parent(dev), dev, fe_ids);

	/* If the card had a PnP ID that didn't match any we know about */
	if (error == ENXIO)
		goto end;

	/* If we had some other problem. */
	if (!(error == 0 || error == ENOENT))
		goto end;

	/* Probe for supported boards.  */
	if ((error = fe_probe_re1000(dev)) == 0)
		goto end;
	fe_release_resource(dev);

	if ((error = fe_probe_cnet9ne(dev)) == 0)
		goto end;
	fe_release_resource(dev);

	if ((error = fe_probe_rex(dev)) == 0)
		goto end;
	fe_release_resource(dev);

	if ((error = fe_probe_ssi(dev)) == 0)
		goto end;
	fe_release_resource(dev);

	if ((error = fe_probe_jli(dev)) == 0)
		goto end;
	fe_release_resource(dev);

	if ((error = fe_probe_lnx(dev)) == 0)
		goto end;
	fe_release_resource(dev);

	if ((error = fe_probe_ubn(dev)) == 0)
		goto end;
	fe_release_resource(dev);

	if ((error = fe_probe_gwy(dev)) == 0)
		goto end;
	fe_release_resource(dev);

end:
	if (error == 0)
		error = fe_alloc_irq(dev, 0);

	fe_release_resource(dev);
	return (error);
}

static int
fe_isa_attach(device_t dev)
{
	struct fe_softc *sc = device_get_softc(dev);

	if (sc->port_used)
		fe98_alloc_port(dev, sc->type);
	fe_alloc_irq(dev, 0);

	return fe_attach(dev);
}


/* Generic I/O address table */
static bus_addr_t ioaddr_generic[MAXREGISTERS] = {
	0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007,
	0x008, 0x009, 0x00a, 0x00b, 0x00c, 0x00d, 0x00e, 0x00f,
	0x010, 0x011, 0x012, 0x013, 0x014, 0x015, 0x016, 0x017,
	0x018, 0x019, 0x01a, 0x01b, 0x01c, 0x01d, 0x01e, 0x01f,
};

/* I/O address table for RE1000/1000Plus */
static bus_addr_t ioaddr_re1000[MAXREGISTERS] = {
	0x0000, 0x0001, 0x0200, 0x0201, 0x0400, 0x0401, 0x0600, 0x0601,
	0x0800, 0x0801, 0x0a00, 0x0a01, 0x0c00, 0x0c01, 0x0e00, 0x0e01,
	0x1000, 0x1200, 0x1400, 0x1600, 0x1800, 0x1a00, 0x1c00, 0x1e00,
	0x1001, 0x1201, 0x1401, 0x1601, 0x1801, 0x1a01, 0x1c01, 0x1e01,
};

/* I/O address table for CNET9NE */
static bus_addr_t ioaddr_cnet9ne[MAXREGISTERS] = {
	0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007,
	0x008, 0x009, 0x00a, 0x00b, 0x00c, 0x00d, 0x00e, 0x00f,
	0x400, 0x402, 0x404, 0x406, 0x408, 0x40a, 0x40c, 0x40e,
	0x401, 0x403, 0x405, 0x407, 0x409, 0x40b, 0x40d, 0x40f,
};

/* I/O address table for LAC-98 */
static bus_addr_t ioaddr_lnx[MAXREGISTERS] = {
	0x000, 0x002, 0x004, 0x006, 0x008, 0x00a, 0x00c, 0x00e,
	0x100, 0x102, 0x104, 0x106, 0x108, 0x10a, 0x10c, 0x10e,
	0x200, 0x202, 0x204, 0x206, 0x208, 0x20a, 0x20c, 0x20e,
	0x300, 0x302, 0x304, 0x306, 0x308, 0x30a, 0x30c, 0x30e,
};

/* I/O address table for Access/PC N98C+ */
static bus_addr_t ioaddr_ubn[MAXREGISTERS] = {
	0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007,
	0x008, 0x009, 0x00a, 0x00b, 0x00c, 0x00d, 0x00e, 0x00f,
	0x200, 0x201, 0x202, 0x203, 0x204, 0x205, 0x206, 0x207,
	0x208, 0x209, 0x20a, 0x20b, 0x20c, 0x20d, 0x20e, 0x20f,
};

/* I/O address table for REX-9880 */
static bus_addr_t ioaddr_rex[MAXREGISTERS] = {
	0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007,
	0x008, 0x009, 0x00a, 0x00b, 0x00c, 0x00d, 0x00e, 0x00f,
	0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x106, 0x107,
	0x108, 0x109, 0x10a, 0x10b, 0x10c, 0x10d, 0x10e, 0x10f,
};

static int
fe98_alloc_port(device_t dev, int type)
{
	struct fe_softc *sc = device_get_softc(dev);
	struct resource *res;
	bus_addr_t *iat;
	int size, rid;

	switch (type) {
	case FE_TYPE_RE1000:
		iat = ioaddr_re1000;
		size = MAXREGISTERS;
		break;
	case FE_TYPE_CNET9NE:
		iat = ioaddr_cnet9ne;
		size = MAXREGISTERS;
		break;
	case FE_TYPE_SSI:
		iat = ioaddr_generic;
		size = MAXREGISTERS;
		break;
	case FE_TYPE_LNX:
		iat = ioaddr_lnx;
		size = MAXREGISTERS;
		break;
	case FE_TYPE_GWY:
		iat = ioaddr_generic;
		size = MAXREGISTERS;
		break;
	case FE_TYPE_UBN:
		iat = ioaddr_ubn;
		size = MAXREGISTERS;
		break;
	case FE_TYPE_REX:
		iat = ioaddr_rex;
		size = MAXREGISTERS;
		break;
	default:
		iat = ioaddr_generic;
		size = MAXREGISTERS;
		break;
	}

	rid = 0;
	res = isa_alloc_resourcev(dev, SYS_RES_IOPORT, &rid,
				  iat, size, RF_ACTIVE);
	if (res == NULL)
		return ENOENT;

	isa_load_resourcev(res, iat, size);

	sc->type = type;
	sc->port_used = size;
	sc->port_res = res;
	return (0);
}


/*
 * Probe and initialization for Allied-Telesis RE1000 series.
 */
static void
fe_init_re1000(struct fe_softc *sc)
{
	/* Setup IRQ control register on the ASIC.  */
	fe_outb(sc, FE_RE1000_IRQCONF, sc->priv_info);
}

static int
fe_probe_re1000(device_t dev)
{
	struct fe_softc *sc = device_get_softc(dev);
	int i, n;
	u_long iobase, irq;
	u_char sum;

	static struct fe_simple_probe_struct probe_table [] = {
		{ FE_DLCR2, 0x58, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
		{ 0 }
	};

	/* See if the specified I/O address is possible for RE1000.  */
	/* [01]D[02468ACE] are allowed.  */ 
	if (bus_get_resource(dev, SYS_RES_IOPORT, 0, &iobase, NULL) != 0)
		return ENXIO;
	if ((iobase & ~0x10E) != 0xD0)
		return ENXIO;

	if (fe98_alloc_port(dev, FE_TYPE_RE1000))
		return ENXIO;

	/* Fill the softc struct with default values.  */
	fe_softc_defaults(sc);

	/* See if the card is on its address.  */
	if (!fe_simple_probe(sc, probe_table))
		return ENXIO;

	/* Get our station address from EEPROM.  */
	fe_inblk(sc, 0x18, sc->enaddr, ETHER_ADDR_LEN);

	/* Make sure it is Allied-Telesis's.  */
	if (!fe_valid_Ether_p(sc->enaddr, 0x0000F4))
		return ENXIO;
#if 1
	/* Calculate checksum.  */
	sum = fe_inb(sc, 0x1e);
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sum ^= sc->enaddr[i];
	if (sum != 0)
		return ENXIO;
#endif
	/* Setup the board type.  */
	sc->typestr = "RE1000";

	/* This looks like an RE1000 board.  It requires an
	   explicit IRQ setting in config.  Make sure we have one,
	   determining an appropriate value for the IRQ control
	   register.  */
	irq = 0;
	bus_get_resource(dev, SYS_RES_IRQ, 0, &irq, NULL);
	switch (irq) {
	case 3:  n = 0x10; break;
	case 5:  n = 0x20; break;
	case 6:  n = 0x40; break;
	case 12: n = 0x80; break;
	default:
		fe_irq_failure(sc->typestr, sc->sc_unit, irq, "3/5/6/12");
		return ENXIO;
	}
	sc->priv_info = (fe_inb(sc, FE_RE1000_IRQCONF) & 0x0f) | n;

	/* Setup hooks.  We need a special initialization procedure.  */
	sc->init = fe_init_re1000;

	return 0;
}

/* JLI sub-probe for Allied-Telesis RE1000Plus/ME1500 series.  */
static u_short const *
fe_probe_jli_re1000p(struct fe_softc * sc, u_char const * eeprom)
{
	int i;
	static u_short const irqmaps_re1000p [4] = { 3, 5, 6, 12 };

	/* Make sure the EEPROM contains Allied-Telesis bit pattern.  */
	if (eeprom[1] != 0xFF) return NULL;
	for (i =  2; i <  8; i++) if (eeprom[i] != 0xFF) return NULL;
	for (i = 14; i < 24; i++) if (eeprom[i] != 0xFF) return NULL;

	/* Get our station address from EEPROM, and make sure the
           EEPROM contains Allied-Telesis's address.  */
	bcopy(eeprom + 8, sc->enaddr, ETHER_ADDR_LEN);
	if (!fe_valid_Ether_p(sc->enaddr, 0x0000F4))
		return NULL;

	/* I don't know any sub-model identification.  */
	sc->typestr = "RE1000Plus/ME1500";

	/* Returns the IRQ table for the RE1000Plus.  */
	return irqmaps_re1000p;
}


/*
 * Probe for Allied-Telesis RE1000Plus/ME1500 series.
 */
static int
fe_probe_jli(device_t dev)
{
	struct fe_softc *sc = device_get_softc(dev);
	int i, n, xirq, error;
	u_long iobase, irq;
	u_char eeprom [JLI_EEPROM_SIZE];
	u_short const * irqmap;

	static u_short const baseaddr [8] =
		{ 0x1D6, 0x1D8, 0x1DA, 0x1D4, 0x0D4, 0x0D2, 0x0D8, 0x0D0 };
	static struct fe_simple_probe_struct const probe_table [] = {
	/*	{ FE_DLCR1,  0x20, 0x00 },	Doesn't work. */
		{ FE_DLCR2,  0x50, 0x00 },
		{ FE_DLCR4,  0x08, 0x00 },
	/*	{ FE_DLCR5,  0x80, 0x00 },	Doesn't work. */
#if 0
		{ FE_BMPR16, 0x1B, 0x00 },
		{ FE_BMPR17, 0x7F, 0x00 },
#endif
		{ 0 }
	};

	/*
	 * See if the specified address is possible for MB86965A JLI mode.
	 */
	if (bus_get_resource(dev, SYS_RES_IOPORT, 0, &iobase, NULL) != 0)
		return ENXIO;
	for (i = 0; i < 8; i++) {
		if (baseaddr[i] == iobase)
			break;
	}
	if (i == 8)
		return ENXIO;

	if (fe98_alloc_port(dev, FE_TYPE_RE1000))
		return ENXIO;

	/* Fill the softc struct with default values.  */
	fe_softc_defaults(sc);

	/*
	 * We should test if MB86965A is on the base address now.
	 * Unfortunately, it is very hard to probe it reliably, since
	 * we have no way to reset the chip under software control.
	 * On cold boot, we could check the "signature" bit patterns
	 * described in the Fujitsu document.  On warm boot, however,
	 * we can predict almost nothing about register values.
	 */
	if (!fe_simple_probe(sc, probe_table))
		return ENXIO;

	/* Check if our I/O address matches config info on 86965.  */
	n = (fe_inb(sc, FE_BMPR19) & FE_B19_ADDR) >> FE_B19_ADDR_SHIFT;
	if (baseaddr[n] != iobase)
		return ENXIO;

	/*
	 * We are now almost sure we have an MB86965 at the given
	 * address.  So, read EEPROM through it.  We have to write
	 * into LSI registers to read from EEPROM.  I want to avoid it
	 * at this stage, but I cannot test the presence of the chip
	 * any further without reading EEPROM.  FIXME.
	 */
	fe_read_eeprom_jli(sc, eeprom);

	/* Make sure that config info in EEPROM and 86965 agree.  */
	if (eeprom[FE_EEPROM_CONF] != fe_inb(sc, FE_BMPR19))
		return ENXIO;

	/* Use 86965 media selection scheme, unless othewise
           specified.  It is "AUTO always" and "select with BMPR13".
           This behaviour covers most of the 86965 based board (as
           minimum requirements.)  It is backward compatible with
           previous versions, also.  */
	sc->mbitmap = MB_HA;
	sc->defmedia = MB_HA;
	sc->msel = fe_msel_965;

	/* Perform board-specific probe.  */
	if ((irqmap = fe_probe_jli_re1000p(sc, eeprom)) == NULL)
		return ENXIO;

	/* Find the IRQ read from EEPROM.  */
	n = (fe_inb(sc, FE_BMPR19) & FE_B19_IRQ) >> FE_B19_IRQ_SHIFT;
	xirq = irqmap[n];

	/* Try to determine IRQ setting.  */
	error = bus_get_resource(dev, SYS_RES_IRQ, 0, &irq, NULL);
	if (error && xirq == NO_IRQ) {
		/* The device must be configured with an explicit IRQ.  */
		device_printf(dev, "IRQ auto-detection does not work\n");
		return ENXIO;
	} else if (error && xirq != NO_IRQ) {
		/* Just use the probed IRQ value.  */
		bus_set_resource(dev, SYS_RES_IRQ, 0, xirq, 1);
	} else if (!error && xirq == NO_IRQ) {
		/* No problem.  Go ahead.  */
	} else if (irq == xirq) {
		/* Good.  Go ahead.  */
	} else {
		/* User must be warned in this case.  */
		sc->stability |= UNSTABLE_IRQ;
	}

	/* Setup a hook, which resets te 86965 when the driver is being
           initialized.  This may solve a nasty bug.  FIXME.  */
	sc->init = fe_init_jli;

	return 0;
}


/*
 * Probe and initialization for Contec C-NET(9N)E series.
 */

/* TODO: Should be in "if_fereg.h" */
#define FE_CNET9NE_INTR		0x10		/* Interrupt Mask? */

static void
fe_init_cnet9ne(struct fe_softc *sc)
{
	/* Enable interrupt?  FIXME.  */
	fe_outb(sc, FE_CNET9NE_INTR, 0x10);
}

static int
fe_probe_cnet9ne (device_t dev)
{
	struct fe_softc *sc = device_get_softc(dev);
	u_long iobase, irq;

	static struct fe_simple_probe_struct probe_table [] = {
		{ FE_DLCR2, 0x58, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
		{ 0 }
	};

	/* See if the specified I/O address is possible for C-NET(9N)E.  */
	if (bus_get_resource(dev, SYS_RES_IOPORT, 0, &iobase, NULL) != 0)
		return ENXIO;
	if (iobase != 0x73D0)
		return ENXIO;

	if (fe98_alloc_port(dev, FE_TYPE_CNET9NE))
		return ENXIO;

	/* Fill the softc struct with default values.  */
	fe_softc_defaults(sc);

	/* See if the card is on its address.  */
	if (!fe_simple_probe(sc, probe_table))
		return ENXIO;

	/* Get our station address from EEPROM.  */
	fe_inblk(sc, 0x18, sc->enaddr, ETHER_ADDR_LEN);

	/* Make sure it is Contec's.  */
	if (!fe_valid_Ether_p(sc->enaddr, 0x00804C))
		return ENXIO;

	/* Determine the card type.  */
	if (sc->enaddr[3] == 0x06) {
		sc->typestr = "C-NET(9N)C";

		/* We seems to need our own IDENT bits...  FIXME.  */
		sc->proto_dlcr7 = FE_D7_BYTSWP_LH | FE_D7_IDENT_NICE;

		/* C-NET(9N)C requires an explicit IRQ to work.  */
		if (bus_get_resource(dev, SYS_RES_IRQ, 0, &irq, NULL) != 0) {
			fe_irq_failure(sc->typestr, sc->sc_unit, NO_IRQ, NULL);
			return ENXIO;
		}
	} else {
		sc->typestr = "C-NET(9N)E";

		/* C-NET(9N)E works only IRQ5.  */
		if (bus_get_resource(dev, SYS_RES_IRQ, 0, &irq, NULL) != 0)
			return ENXIO;
		if (irq != 5) {
			fe_irq_failure(sc->typestr, sc->sc_unit, irq, "5");
			return ENXIO;
		}

		/* We need an init hook to initialize ASIC before we start.  */
		sc->init = fe_init_cnet9ne;
	}

	/* C-NET(9N)E has 64KB SRAM.  */
	sc->proto_dlcr6 = FE_D6_BUFSIZ_64KB | FE_D6_TXBSIZ_2x4KB
			| FE_D6_BBW_WORD | FE_D6_SBW_WORD | FE_D6_SRAM;

	return 0;
}


/*
 * Probe for Contec C-NET(98)P2 series.
 * (Logitec LAN-98TP/LAN-98T25P - parhaps)
 */
static int
fe_probe_ssi(device_t dev)
{
	struct fe_softc *sc = device_get_softc(dev);
	u_long iobase, irq;

	u_char eeprom [SSI_EEPROM_SIZE];
	static struct fe_simple_probe_struct probe_table [] = {
		{ FE_DLCR2, 0x08, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
		{ 0 }
	};
	static u_short const irqmap[] = {
		/*                        INT0          INT1    INT2       */
		NO_IRQ, NO_IRQ, NO_IRQ,      3, NO_IRQ,    5,      6, NO_IRQ,
		NO_IRQ,      9,     10, NO_IRQ,     12,   13, NO_IRQ, NO_IRQ,
		/*        INT3   INT41            INT5  INT6               */
	};

	/* See if the specified I/O address is possible for 78Q8377A.  */
	/* [0-D]3D0 are allowed.  */
	if (bus_get_resource(dev, SYS_RES_IOPORT, 0, &iobase, NULL) != 0)
		return ENXIO;
	if ((iobase & 0xFFF) != 0x3D0)
		return ENXIO;
		
	if (fe98_alloc_port(dev, FE_TYPE_SSI))
		return ENXIO;

	/* Fill the softc struct with default values.  */
	fe_softc_defaults(sc);

	/* See if the card is on its address.  */
	if (!fe_simple_probe(sc, probe_table))
		return ENXIO;

	/* We now have to read the config EEPROM.  We should be very
           careful, since doing so destroys a register.  (Remember, we
           are not yet sure we have a C-NET(98)P2 board here.)  Don't
           remember to select BMPRs bofore reading EEPROM, since other
           register bank may be selected before the probe() is called.  */
	fe_read_eeprom_ssi(sc, eeprom);

	/* Make sure the Ethernet (MAC) station address is of Contec's.  */
	if (!fe_valid_Ether_p(eeprom + FE_SSI_EEP_ADDR, 0x00804C))
		return ENXIO;
	bcopy(eeprom + FE_SSI_EEP_ADDR, sc->enaddr, ETHER_ADDR_LEN);

	/* Setup the board type.  */
        sc->typestr = "C-NET(98)P2";

	/* Non-PnP mode, set static resource from eeprom. */
	if (!isa_get_vendorid(dev)) {
		/* Get IRQ configuration from EEPROM.  */
		irq = irqmap[eeprom[FE_SSI_EEP_IRQ]];
		if (irq == NO_IRQ) {
			fe_irq_failure(sc->typestr, sc->sc_unit, irq,
				       "3/5/6/9/10/12/13");
			return ENXIO;
		}
		bus_set_resource(dev, SYS_RES_IRQ, 0, irq, 1);
	}

	/* Get Duplex-mode configuration from EEPROM.  */
	sc->proto_dlcr4 |= (eeprom[FE_SSI_EEP_DUPLEX] & FE_D4_DSC);

	/* Fill softc struct accordingly.  */
	sc->mbitmap = MB_HT;
	sc->defmedia = MB_HT;

	return 0;
}


/*
 * Probe for TDK LAC-98012/013/025/9N011 - parhaps.
 */
static int
fe_probe_lnx(device_t dev)
{
	struct fe_softc *sc = device_get_softc(dev);

	u_long iobase, irq;
	u_char eeprom [LNX_EEPROM_SIZE];

	static struct fe_simple_probe_struct probe_table [] = {
		{ FE_DLCR2, 0x58, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
		{ 0 }
	};

	/* See if the specified I/O address is possible for TDK/LANX boards. */
	/* 0D0, 4D0, 8D0, and CD0 are allowed.  */
	if (bus_get_resource(dev, SYS_RES_IOPORT, 0, &iobase, NULL) != 0)
		return ENXIO;
	if ((iobase & ~0xC00) != 0xD0)
		return ENXIO;

	if (fe98_alloc_port(dev, FE_TYPE_LNX))
		return ENXIO;

	/* Fill the softc struct with default values.  */
	fe_softc_defaults(sc);

	/* See if the card is on its address.  */
	if (!fe_simple_probe(sc, probe_table))
		return ENXIO;

	/* We now have to read the config EEPROM.  We should be very
           careful, since doing so destroys a register.  (Remember, we
           are not yet sure we have a LAC-98012/98013 board here.)  */
	fe_read_eeprom_lnx(sc, eeprom);

	/* Make sure the Ethernet (MAC) station address is of TDK/LANX's.  */
	if (!fe_valid_Ether_p(eeprom, 0x008098))
		return ENXIO;
	bcopy(eeprom, sc->enaddr, ETHER_ADDR_LEN);

	/* Setup the board type.  */
	sc->typestr = "LAC-98012/98013";

	/* This looks like a TDK/LANX board.  It requires an
	   explicit IRQ setting in config.  Make sure we have one,
	   determining an appropriate value for the IRQ control
	   register.  */
	irq = 0;
	if (bus_get_resource(dev, SYS_RES_IRQ, 0, &irq, NULL) != 0)
		return ENXIO;
	switch (irq) {
	case 3 : sc->priv_info = 0x10 | LNX_CLK_LO | LNX_SDA_HI; break;
	case 5 : sc->priv_info = 0x20 | LNX_CLK_LO | LNX_SDA_HI; break;
	case 6 : sc->priv_info = 0x40 | LNX_CLK_LO | LNX_SDA_HI; break;
	case 12: sc->priv_info = 0x80 | LNX_CLK_LO | LNX_SDA_HI; break;
	default:
		fe_irq_failure(sc->typestr, sc->sc_unit, irq, "3/5/6/12");
		return ENXIO;
	}

	/* LAC-98's system bus width is 8-bit.  */ 
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x2KB
			| FE_D6_BBW_BYTE | FE_D6_SBW_BYTE | FE_D6_SRAM_150ns;

	/* Setup hooks.  We need a special initialization procedure.  */
	sc->init = fe_init_lnx;

	return 0;
}


/*
 * Probe for Gateway Communications' old cards.
 * (both as Generic MB86960 probe routine)
 */
static int
fe_probe_gwy(device_t dev)
{
	struct fe_softc *sc = device_get_softc(dev);

	static struct fe_simple_probe_struct probe_table [] = {
	    /*	{ FE_DLCR2, 0x70, 0x00 }, */
		{ FE_DLCR2, 0x58, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
		{ 0 }
	};

	/*
	 * XXX
	 * I'm not sure which address is possible, so accepts any.
	 */

	if (fe98_alloc_port(dev, FE_TYPE_GWY))
		return ENXIO;

	/* Fill the softc struct with default values.  */
	fe_softc_defaults(sc);

	/* See if the card is on its address.  */
	if (!fe_simple_probe(sc, probe_table))
		return ENXIO;

	/* Get our station address from EEPROM. */
	fe_inblk(sc, 0x18, sc->enaddr, ETHER_ADDR_LEN);
	if (!fe_valid_Ether_p(sc->enaddr, 0x000000))
		return ENXIO;

	/* Determine the card type.  */
	sc->typestr = "Generic MB86960 Ethernet";
	if (fe_valid_Ether_p(sc->enaddr, 0x000061))
		sc->typestr = "Gateway Ethernet (Fujitsu chipset)";

	/* Gateway's board requires an explicit IRQ to work, since it
	   is not possible to probe the setting of jumpers.  */
	if (bus_get_resource(dev, SYS_RES_IRQ, 0, NULL, NULL) != 0) {
		fe_irq_failure(sc->typestr, sc->sc_unit, NO_IRQ, NULL);
		return ENXIO;
	}

	return 0;
}


/*
 * Probe for Ungermann-Bass Access/PC N98C+(Model 85152).
 */
static int
fe_probe_ubn(device_t dev)
{
	struct fe_softc *sc = device_get_softc(dev);

	u_char sum, save7;
	u_long iobase, irq;
	int i;
	static struct fe_simple_probe_struct const probe_table [] = {
		{ FE_DLCR2, 0x58, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
		{ 0 }
	};

	/* See if the specified I/O address is possible for Access/PC.  */
	/* [01][048C]D0 are allowed.  */ 
	if (bus_get_resource(dev, SYS_RES_IOPORT, 0, &iobase, NULL) != 0)
		return ENXIO;
	if ((iobase & ~0x1C00) != 0xD0)
		return ENXIO;

	if (fe98_alloc_port(dev, FE_TYPE_UBN))
		return ENXIO;

	/* Fill the softc struct with default values.  */
	fe_softc_defaults(sc);

	/* Simple probe.  */
	if (!fe_simple_probe(sc, probe_table))
		return ENXIO;

	/* NOTE: Access/NOTE N98 sometimes freeze when reading station
	   address.  In case of using it togather with C-NET(9N)C,
	   this problem usually happens.
	   Writing DLCR7 prevents freezing, but I don't know why.  FIXME.  */

	/* Save the current value for the DLCR7 register we are about
	   to destroy.  */
	save7 = fe_inb(sc, FE_DLCR7);
	fe_outb(sc, FE_DLCR7,
		sc->proto_dlcr7 | FE_D7_RBS_BMPR | FE_D7_POWER_UP);

	/* Get our station address form ID ROM and make sure it is UBN's.  */
	fe_inblk(sc, 0x18, sc->enaddr, ETHER_ADDR_LEN);
	if (!fe_valid_Ether_p(sc->enaddr, 0x00DD01))
		goto fail_ubn;
#if 1
	/* Calculate checksum.  */
	sum = fe_inb(sc, 0x1e);
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sum ^= sc->enaddr[i];
	if (sum != 0)
		goto fail_ubn;
#endif

	/* Setup the board type.  */
	sc->typestr = "Access/PC";

	/* This looks like an AccessPC/N98C+ board.  It requires an
	   explicit IRQ setting in config.  Make sure we have one,
	   determining an appropriate value for the IRQ control
	   register.  */
	irq = 0;
	bus_get_resource(dev, SYS_RES_IRQ, 0, &irq, NULL);
	switch (irq) {
	case 3:  sc->priv_info = 0x01; break;
	case 5:  sc->priv_info = 0x02; break;
	case 6:  sc->priv_info = 0x04; break;
	case 12: sc->priv_info = 0x08; break;
	default:
		fe_irq_failure(sc->typestr, sc->sc_unit, irq, "3/5/6/12");
		goto fail_ubn;
	}

	/* Setup hooks.  We need a special initialization procedure.  */
	sc->init = fe_init_ubn;

	return 0;

fail_ubn:
	fe_outb(sc, FE_DLCR7, save7);
	return ENXIO;
}


/*
 * REX boards(non-JLI type) support routine.
 */

#define REX_EEPROM_SIZE	32
#define REX_DAT	0x01

static void
fe_read_eeprom_rex(struct fe_softc *sc, u_char *data)
{
	int i;
	u_char bit, val;
	u_char save16;

	save16 = fe_inb(sc, 0x10);

	/* Issue a start condition.  */
	val = fe_inb(sc, 0x10) & 0xf0;
	fe_outb(sc, 0x10, val);

	(void) fe_inb(sc, 0x10);
	(void) fe_inb(sc, 0x10);
	(void) fe_inb(sc, 0x10);
	(void) fe_inb(sc, 0x10);

	/* Read bytes from EEPROM.  */
	for (i = 0; i < REX_EEPROM_SIZE; i++) {
		/* Read a byte and store it into the buffer.  */
		val = 0x00;
		for (bit = 0x01; bit != 0x00; bit <<= 1)
			if (fe_inb(sc, 0x10) & REX_DAT)
				val |= bit;
		*data++ = val;
	}

	fe_outb(sc, 0x10, save16);

#if 1
	/* Report what we got.  */
	if (bootverbose) {
		data -= REX_EEPROM_SIZE;
		for (i = 0; i < REX_EEPROM_SIZE; i += 16) {
			printf("fe%d: EEPROM(REX):%3x: %16D\n",
			       sc->sc_unit, i, data + i, " ");
		}
	}
#endif
}


static void
fe_init_rex(struct fe_softc *sc)
{
	/* Setup IRQ control register on the ASIC.  */
	fe_outb(sc, 0x10, sc->priv_info);
}

/*
 * Probe for RATOC REX-9880/81/82/83 series.
 */
static int
fe_probe_rex(device_t dev)
{
	struct fe_softc *sc = device_get_softc(dev);

	int i;
	u_long iobase, irq;
	u_char eeprom [REX_EEPROM_SIZE];

	static struct fe_simple_probe_struct probe_table [] = {
		{ FE_DLCR2, 0x58, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
		{ 0 }
	};

	/* See if the specified I/O address is possible for REX-9880.  */
	/* 6[46CE]D0 are allowed.  */ 
	if (bus_get_resource(dev, SYS_RES_IOPORT, 0, &iobase, NULL) != 0)
		return ENXIO;
	if ((iobase & ~0xA00) != 0x64D0)
		return ENXIO;

	if (fe98_alloc_port(dev, FE_TYPE_REX))
		return ENXIO;

	/* Fill the softc struct with default values.  */
	fe_softc_defaults(sc);

	/* See if the card is on its address.  */
	if (!fe_simple_probe(sc, probe_table))
		return ENXIO;

	/* We now have to read the config EEPROM.  We should be very
           careful, since doing so destroys a register.  (Remember, we
           are not yet sure we have a REX-9880 board here.)  */
	fe_read_eeprom_rex(sc, eeprom);
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sc->enaddr[i] = eeprom[7 - i];

	/* Make sure it is RATOC's.  */
	if (!fe_valid_Ether_p(sc->enaddr, 0x00C0D0) &&
	    !fe_valid_Ether_p(sc->enaddr, 0x00803D))
		return 0;

	/* Setup the board type.  */
	sc->typestr = "REX-9880/9883";

	/* This looks like a REX-9880 board.  It requires an
	   explicit IRQ setting in config.  Make sure we have one,
	   determining an appropriate value for the IRQ control
	   register.  */
	irq = 0;
	bus_get_resource(dev, SYS_RES_IRQ, 0, &irq, NULL);
	switch (irq) {
	case 3:  sc->priv_info = 0x10; break;
	case 5:  sc->priv_info = 0x20; break;
	case 6:  sc->priv_info = 0x40; break;
	case 12: sc->priv_info = 0x80; break;
	default:
		fe_irq_failure(sc->typestr, sc->sc_unit, irq, "3/5/6/12");
		return ENXIO;
	}

	/* Setup hooks.  We need a special initialization procedure.  */
	sc->init = fe_init_rex;

	/* REX-9880 has 64KB SRAM.  */
	sc->proto_dlcr6 = FE_D6_BUFSIZ_64KB | FE_D6_TXBSIZ_2x4KB
			| FE_D6_BBW_WORD | FE_D6_SBW_WORD | FE_D6_SRAM;
#if 1
	sc->proto_dlcr7 |= FE_D7_EOPPOL;	/* XXX */
#endif

	return 0;
}
