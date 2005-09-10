/*-
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

#include "opt_ed.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/uio.h>

#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_mib.h>
#include <net/if_media.h>

#include <dev/ed/if_edreg.h>
#include <dev/ed/if_edvar.h>
#include <dev/pccard/pccardvar.h>
#include <dev/pccard/pccard_cis.h>
#ifndef ED_NO_MIIBUS
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#endif

#include "card_if.h"
#ifndef ED_NO_MIIBUS
/* "device miibus" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"
#endif
#include "pccarddevs.h"

#ifndef ED_NO_MIIBUS
MODULE_DEPEND(ed, miibus, 1, 1, 1);
#endif
MODULE_DEPEND(ed, ether, 1, 1, 1);

/*
 * PC Cards should be using a network specific FUNCE in the CIS to
 * communicate their MAC address to the driver.  However, there are a
 * large number of NE-2000ish PC Cards that don't do this.  Nearly all
 * of them store the MAC address at a fixed offset into attribute
 * memory, without any reference at all appearing in the CIS.  And
 * nearly all of those store it at the same location.
 *
 * This applies only to the older, NE-2000 compatbile cards.  The newer
 * cards based on the AX88x90 or DL100XX chipsets have a specific place
 * to look for MAC information.
 */
#define ED_DEFAULT_MAC_OFFSET	0xff0

static const struct ed_product {
	struct pccard_product	prod;
	int flags;
#define	NE2000DVF_DL100XX	0x0001		/* chip is D-Link DL10019/22 */
#define	NE2000DVF_AX88X90	0x0002		/* chip is ASIX AX88[17]90 */
#define NE2000DVF_ENADDR	0x0004		/* Get MAC from attr mem */
#define NE2000DVF_ANYFUNC	0x0008		/* Allow any function type */
#define NE2000DVF_MODEM		0x0010		/* Has a modem/serial */
	int enoff;
} ed_pccard_products[] = {
	{ PCMCIA_CARD(ACCTON, EN2212), 0},
	{ PCMCIA_CARD(ACCTON, EN2216), 0},
	{ PCMCIA_CARD(ALLIEDTELESIS, LA_PCM), 0},
	{ PCMCIA_CARD(AMBICOM, AMB8002T), 0},
	{ PCMCIA_CARD(BILLIONTON, LNT10TN), 0},
	{ PCMCIA_CARD(BILLIONTON, CFLT10N), 0},
	{ PCMCIA_CARD(BROMAX, IPORT), 0},
	{ PCMCIA_CARD(BROMAX, IPORT2), 0},
	{ PCMCIA_CARD(BUFFALO, LPC2_CLT), 0},
	{ PCMCIA_CARD(BUFFALO, LPC3_CLT), 0},
	{ PCMCIA_CARD(BUFFALO, LPC3_CLX), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(BUFFALO, LPC4_TX), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(BUFFALO, LPC_CF_CLT), 0},
	{ PCMCIA_CARD(CNET, NE2000), 0},
	{ PCMCIA_CARD(COMPEX, AX88190), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(COMPEX, LANMODEM), 0},
	{ PCMCIA_CARD(COMPEX, LINKPORT_ENET_B), 0},
	{ PCMCIA_CARD(COREGA, ETHER_II_PCC_T), 0},
	{ PCMCIA_CARD(COREGA, ETHER_II_PCC_TD), 0},
	{ PCMCIA_CARD(COREGA, ETHER_PCC_T), 0},
	{ PCMCIA_CARD(COREGA, ETHER_PCC_TD), 0},
	{ PCMCIA_CARD(COREGA, FAST_ETHER_PCC_TX), NE2000DVF_DL100XX },
	{ PCMCIA_CARD(COREGA, FETHER_PCC_TXD), NE2000DVF_AX88X90 },
	{ PCMCIA_CARD(COREGA, FETHER_PCC_TXF), NE2000DVF_DL100XX },
	{ PCMCIA_CARD(DAYNA, COMMUNICARD_E_1), 0},
	{ PCMCIA_CARD(DAYNA, COMMUNICARD_E_2), 0},
	{ PCMCIA_CARD(DLINK, DE650), 0},
	{ PCMCIA_CARD(DLINK, DE660), 0 },
	{ PCMCIA_CARD(DLINK, DE660PLUS), 0},
	{ PCMCIA_CARD(DYNALINK, L10C), 0},
	{ PCMCIA_CARD(EDIMAX, EP4000A), 0},
	{ PCMCIA_CARD(EPSON, EEN10B), 0},
	{ PCMCIA_CARD(EXP, THINLANCOMBO), 0},
	{ PCMCIA_CARD(GREY_CELL, TDK3000), 0},
	{ PCMCIA_CARD(GREY_CELL, DMF650TX),
	    NE2000DVF_ANYFUNC | NE2000DVF_DL100XX | NE2000DVF_MODEM},
	{ PCMCIA_CARD(IBM, HOME_AND_AWAY), 0},
	{ PCMCIA_CARD(IBM, INFOMOVER), NE2000DVF_ENADDR, 0xff0},
	{ PCMCIA_CARD(IODATA3, PCLAT), 0},
	{ PCMCIA_CARD(KINGSTON, CIO10T), 0},
	{ PCMCIA_CARD(KINGSTON, KNE2), 0},
	{ PCMCIA_CARD(LANTECH, FASTNETTX),NE2000DVF_AX88X90 },
	{ PCMCIA_CARD(LINKSYS, COMBO_ECARD),
	    NE2000DVF_DL100XX | NE2000DVF_AX88X90 },
	{ PCMCIA_CARD(LINKSYS, ECARD_1), 0},
	{ PCMCIA_CARD(LINKSYS, ECARD_2), 0},
	{ PCMCIA_CARD(LINKSYS, ETHERFAST), NE2000DVF_DL100XX },
	{ PCMCIA_CARD(LINKSYS, TRUST_COMBO_ECARD), 0},
	{ PCMCIA_CARD(MACNICA, ME1_JEIDA), 0},
	{ PCMCIA_CARD(MAGICRAM, ETHER), 0},
	{ PCMCIA_CARD(MELCO, LPC3_CLX), NE2000DVF_AX88X90 },
	{ PCMCIA_CARD(MELCO, LPC3_TX), NE2000DVF_AX88X90 },
	{ PCMCIA_CARD(NDC, ND5100_E), 0},
	{ PCMCIA_CARD(NETGEAR, FA410TXC), NE2000DVF_DL100XX},
	/* Same ID as DLINK DFE-670TXD.  670 has DL10022, fa411 has ax88790 */
	{ PCMCIA_CARD(NETGEAR, FA411), NE2000DVF_AX88X90 | NE2000DVF_DL100XX},
	{ PCMCIA_CARD(NEXTCOM, NEXTHAWK), 0},
	{ PCMCIA_CARD(NEWMEDIA, LANSURFER), 0},
	{ PCMCIA_CARD(OEM2, ETHERNET), 0},
	{ PCMCIA_CARD(OEM2, NE2000), 0},
	{ PCMCIA_CARD(PLANET, SMARTCOM2000), 0 },
	{ PCMCIA_CARD(PREMAX, PE200), 0},
	{ PCMCIA_CARD(PSION, LANGLOBAL), 0},
	{ PCMCIA_CARD(RACORE, ETHERNET), 0},
	{ PCMCIA_CARD(RACORE, FASTENET), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(RACORE, 8041TX), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(RELIA, COMBO), 0},
	{ PCMCIA_CARD(RPTI, EP400), 0},
	{ PCMCIA_CARD(RPTI, EP401), 0},
	{ PCMCIA_CARD(SMC, EZCARD), 0},
	{ PCMCIA_CARD(SOCKET, EA_ETHER), 0},
	{ PCMCIA_CARD(SOCKET, ES_1000), 0},
	{ PCMCIA_CARD(SOCKET, LP_ETHER), 0},
	{ PCMCIA_CARD(SOCKET, LP_ETHER_CF), 0},
	{ PCMCIA_CARD(SOCKET, LP_ETH_10_100_CF), NE2000DVF_DL100XX},
	{ PCMCIA_CARD(SVEC, COMBOCARD), 0},
	{ PCMCIA_CARD(SVEC, LANCARD), 0},
	{ PCMCIA_CARD(TAMARACK, ETHERNET), 0},
	{ PCMCIA_CARD(TDK, CFE_10), 0},
	{ PCMCIA_CARD(TDK, LAK_CD031), 0},
	{ PCMCIA_CARD(TDK, DFL5610WS), 0},
	{ PCMCIA_CARD(TELECOMDEVICE, LM5LT), 0 },
	{ PCMCIA_CARD(TELECOMDEVICE, TCD_HPC100), NE2000DVF_AX88X90 },
	{ PCMCIA_CARD(ZONET, ZEN), 0},
	{ { NULL } }
};

/*
 *      PC Card (PCMCIA) specific code.
 */
static int	ed_pccard_probe(device_t);
static int	ed_pccard_attach(device_t);

static int	ed_pccard_memread(device_t dev, off_t offset, u_char *byte);
static int	ed_pccard_memwrite(device_t dev, off_t offset, u_char byte);

static int	ed_pccard_dl100xx(device_t dev, const struct ed_product *);
#ifndef ED_NO_MIIBUS
static void	ed_pccard_dl10xx_mii_reset(struct ed_softc *sc);
static u_int	ed_pccard_dl10xx_mii_readbits(struct ed_softc *sc, int nbits);
static void	ed_pccard_dl10xx_mii_writebits(struct ed_softc *sc, u_int val,
    int nbits);
#endif

static int	ed_pccard_ax88x90(device_t dev, const struct ed_product *);

static int
ed_pccard_probe(device_t dev)
{
	const struct ed_product *pp;
	int		error;
	uint32_t	fcn = PCCARD_FUNCTION_UNSPEC;

	/* Make sure we're a network function */
	error = pccard_get_function(dev, &fcn);
	if (error != 0)
		return (error);

	if ((pp = (const struct ed_product *) pccard_product_lookup(dev, 
	    (const struct pccard_product *) ed_pccard_products,
	    sizeof(ed_pccard_products[0]), NULL)) != NULL) {
		if (pp->prod.pp_name != NULL)
			device_set_desc(dev, pp->prod.pp_name);
		/*
		 * Some devices don't ID themselves as network, but
		 * that's OK if the flags say so.
		 */
		if (!(pp->flags & NE2000DVF_ANYFUNC) &&
		    fcn != PCCARD_FUNCTION_NETWORK)
			return (ENXIO);
		return (0);
	}
	return (ENXIO);
}

static int
ed_pccard_rom_mac(device_t dev, uint8_t *enaddr)
{
	struct ed_softc *sc = device_get_softc(dev);
	uint8_t romdata[16];
	int i;

	/*
	 * Read in the rom data at location 0.  We should see one of
	 * two patterns.  Either you'll see odd locations 0xff and
	 * even locations data, or you'll see odd and even locations
	 * mirror each other.  In addition, the last two even locations
	 * should be 0.  If they aren't 0, then we'll assume that
	 * there's no valid ROM data on this card and try another method
	 * to recover the MAC address.  Since there are no NE-1000
	 * based PC Card devices, we'll assume we're 16-bit.
	 */
	ed_pio_readmem(sc, 0, romdata, 16);
	if (romdata[12] != 0 || romdata[14] != 0)
		return 0;
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		enaddr[i] = romdata[i * 2];
	return 1;
}

static int
ed_pccard_add_modem(device_t dev)
{
	struct ed_softc *sc = device_get_softc(dev);

	device_printf(dev, "Need to write this code: modem rid is %d\n",
	    sc->modem_rid);
	return 0;
}

static int
ed_pccard_attach(device_t dev)
{
	u_char sum;
	u_char enaddr[ETHER_ADDR_LEN];
	const struct ed_product *pp;
	int	error, i;
	struct ed_softc *sc = device_get_softc(dev);
	u_long size;

	if ((pp = (const struct ed_product *) pccard_product_lookup(dev, 
	    (const struct pccard_product *) ed_pccard_products,
	    sizeof(ed_pccard_products[0]), NULL)) == NULL)
		return (ENXIO);
	sc->modem_rid = -1;
	if (pp->flags & NE2000DVF_MODEM) {
		sc->port_rid = -1;
		for (i = 0; i < 4; i++) {
			size = bus_get_resource_count(dev, SYS_RES_IOPORT, i);
			if (size == ED_NOVELL_IO_PORTS)
				sc->port_rid = i;
			else if (size == 8)
				sc->modem_rid = i;
		}
		if (sc->port_rid == -1) {
			device_printf(dev, "Cannot locate my ports!\n");
			return (ENXIO);
		}
	} else {
		sc->port_rid = 0;
	}
	/* Allocate the port resource during setup. */
	error = ed_alloc_port(dev, sc->port_rid, ED_NOVELL_IO_PORTS);
	if (error)
		return (error);
	error = ed_alloc_irq(dev, 0, 0);
	if (error)
		goto bad;

	/*
	 * Determine which chipset we are.  All the PC Card chipsets have the
	 * ASIC and NIC offsets in the same place.
	 */
	sc->asic_offset = ED_NOVELL_ASIC_OFFSET;
	sc->nic_offset  = ED_NOVELL_NIC_OFFSET;
	error = ENXIO;
	if (error != 0)
		error = ed_pccard_dl100xx(dev, pp);
	if (error != 0 && pp->flags & NE2000DVF_AX88X90)
		error = ed_pccard_ax88x90(dev, pp);
	if (error != 0)
		error = ed_probe_Novell_generic(dev, device_get_flags(dev));
	if (error)
		goto bad;

	error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET | INTR_MPSAFE,
	    edintr, sc, &sc->irq_handle);
	if (error) {
		device_printf(dev, "setup intr failed %d \n", error);
		goto bad;
	}	      

	/*
	 * For the older cards, we have to get the MAC address from
	 * the card in some way.  Let's try the standard PCMCIA way
	 * first.  If that fails, then check to see if we have valid
	 * data from the standard NE-2000 data roms.  If that fails,
	 * check to see if the card has a hint about where to look in
	 * its CIS.  If that fails, maybe we should look at some
	 * default value.  In all fails, we should fail the attach,
	 * but don't right now.
	 */
	if (sc->chip_type == ED_CHIP_TYPE_DP8390) {
		pccard_get_ether(dev, enaddr);
		if (bootverbose)
			device_printf(dev, "CIS MAC %6D\n", enaddr, ":");
		for (i = 0, sum = 0; i < ETHER_ADDR_LEN; i++)
			sum |= enaddr[i];
		if (sum == 0 && ed_pccard_rom_mac(dev, enaddr)) {
			if (bootverbose)
				device_printf(dev, "ROM mac %6D\n", enaddr,
				    ":");
			sum++;
		}
		if (sum == 0 && pp->flags & NE2000DVF_ENADDR) {
			for (i = 0; i < ETHER_ADDR_LEN; i++) {
				ed_pccard_memread(dev, pp->enoff + i * 2,
				    enaddr + i);
				sum |= enaddr[i];
			}
			if (bootverbose)
				device_printf(dev, "Hint MAC %6D\n", enaddr,
				    ":");
		}
		if (sum == 0) {
			for (i = 0; i < ETHER_ADDR_LEN; i++) {
				ed_pccard_memread(dev, ED_DEFAULT_MAC_OFFSET +
				    i * 2, enaddr + i);
				sum |= enaddr[i];
			}
			if (bootverbose)
				device_printf(dev, "Fallback MAC %6D\n",
				    enaddr, ":");
		}
		if (sum == 0) {
			device_printf(dev, "Cannot extract MAC address.\n");
			ed_release_resources(dev);
			return (ENXIO);
		}
		bcopy(enaddr, sc->enaddr, ETHER_ADDR_LEN);
	}

	error = ed_attach(dev);
	if (error)
		goto bad;
#ifndef ED_NO_MIIBUS
 	if (sc->chip_type == ED_CHIP_TYPE_DL10019 ||
	    sc->chip_type == ED_CHIP_TYPE_DL10022) {
		/* Probe for an MII bus, but ignore errors. */
		ed_pccard_dl10xx_mii_reset(sc);
		sc->mii_readbits = ed_pccard_dl10xx_mii_readbits;
		sc->mii_writebits = ed_pccard_dl10xx_mii_writebits;
		mii_phy_probe(dev, &sc->miibus, ed_ifmedia_upd,
		    ed_ifmedia_sts);
	}
#endif
	if (sc->modem_rid != -1)
		ed_pccard_add_modem(dev);
	return (0);
bad:
	ed_release_resources(dev);
	return (error);
}

static int
ed_pccard_memwrite(device_t dev, off_t offset, u_char byte)
{
	int cis_rid;
	struct resource *cis;

	cis_rid = 0;
	cis = bus_alloc_resource(dev, SYS_RES_MEMORY, &cis_rid, 0, ~0, 
	    4 << 10, RF_ACTIVE | RF_SHAREABLE);
	if (cis == NULL)
		return (ENXIO);
	CARD_SET_RES_FLAGS(device_get_parent(dev), dev, SYS_RES_MEMORY,
	    cis_rid, PCCARD_A_MEM_ATTR);

	bus_space_write_1(rman_get_bustag(cis), rman_get_bushandle(cis),
	    offset, byte);

	bus_deactivate_resource(dev, SYS_RES_MEMORY, cis_rid, cis);
	bus_release_resource(dev, SYS_RES_MEMORY, cis_rid, cis);

	return (0);
}

static int
ed_pccard_memread(device_t dev, off_t offset, u_char *byte)
{
	int cis_rid;
	struct resource *cis;

	cis_rid = 0;
	cis = bus_alloc_resource(dev, SYS_RES_MEMORY, &cis_rid, 0, ~0, 
	    4 << 10, RF_ACTIVE | RF_SHAREABLE);
	if (cis == NULL)
		return (ENXIO);
	CARD_SET_RES_FLAGS(device_get_parent(dev), dev, SYS_RES_MEMORY,
	    cis_rid, PCCARD_A_MEM_ATTR);

	*byte = bus_space_read_1(rman_get_bustag(cis), rman_get_bushandle(cis),
	    offset);

	bus_deactivate_resource(dev, SYS_RES_MEMORY, cis_rid, cis);
	bus_release_resource(dev, SYS_RES_MEMORY, cis_rid, cis);

	return (0);
}

/*
 * Probe the Ethernet MAC addrees for PCMCIA Linksys EtherFast 10/100 
 * and compatible cards (DL10019C Ethernet controller).
 *
 * Note: The PAO patches try to use more memory for the card, but that
 * seems to fail for my card.  A future optimization would add this back
 * conditionally.
 */
static int
ed_pccard_dl100xx(device_t dev, const struct ed_product *pp)
{
	struct ed_softc *sc = device_get_softc(dev);
	u_char sum;
	uint8_t id;
	int i, error;

	if (!pp->flags & NE2000DVF_DL100XX)
		return (ENXIO);
	error = ed_probe_Novell_generic(dev, device_get_flags(dev));
	if (error != 0)
		return (error);

	/*
	 * Linksys registers(offset from ASIC base)
	 *
	 * 0x04-0x09 : Physical Address Register 0-5 (PAR0-PAR5)
	 * 0x0A      : Card ID Register (CIR)
	 * 0x0B      : Check Sum Register (SR)
	 */
	for (sum = 0, i = 0x04; i < 0x0c; i++)
		sum += ed_asic_inb(sc, i);
	if (sum != 0xff)
		return (ENXIO);		/* invalid DL10019C */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sc->enaddr[i] = ed_asic_inb(sc, 0x04 + i);
	ed_nic_outb(sc, ED_P0_DCR, ED_DCR_WTS | ED_DCR_FT1 | ED_DCR_LS);
	id = ed_asic_inb(sc, 0xf);
	sc->isa16bit = 1;
	sc->vendor = ED_VENDOR_NOVELL;
	sc->type = ED_TYPE_NE2000;
	sc->chip_type = (id & 0x90) == 0x90 ?
	    ED_CHIP_TYPE_DL10022 : ED_CHIP_TYPE_DL10019;
	sc->type_str = ((id & 0x90) == 0x90) ? "DL10022" : "DL10019";
	return (0);
}

#ifndef ED_NO_MIIBUS
/* MII bit-twiddling routines for cards using Dlink chipset */
#define DL10XX_MIISET(sc, x) ed_asic_outb(sc, ED_DL10XX_MIIBUS, \
    ed_asic_inb(sc, ED_DL10XX_MIIBUS) | (x))
#define DL10XX_MIICLR(sc, x) ed_asic_outb(sc, ED_DL10XX_MIIBUS, \
    ed_asic_inb(sc, ED_DL10XX_MIIBUS) & ~(x))

static void
ed_pccard_dl10xx_mii_reset(struct ed_softc *sc)
{
	if (sc->chip_type != ED_CHIP_TYPE_DL10022)
		return;

	ed_asic_outb(sc, ED_DL10XX_MIIBUS, ED_DL10XX_MII_RESET2);
	DELAY(10);
	ed_asic_outb(sc, ED_DL10XX_MIIBUS,
	    ED_DL10XX_MII_RESET2 | ED_DL10XX_MII_RESET1);
	DELAY(10);
	ed_asic_outb(sc, ED_DL10XX_MIIBUS, ED_DL10XX_MII_RESET2);
	DELAY(10);
	ed_asic_outb(sc, ED_DL10XX_MIIBUS,
	    ED_DL10XX_MII_RESET2 | ED_DL10XX_MII_RESET1);
	DELAY(10);
	ed_asic_outb(sc, ED_DL10XX_MIIBUS, 0);
}

static void
ed_pccard_dl10xx_mii_writebits(struct ed_softc *sc, u_int val, int nbits)
{
	int i;

	if (sc->chip_type == ED_CHIP_TYPE_DL10022)
		DL10XX_MIISET(sc, ED_DL10XX_MII_DIROUT_22);
	else
		DL10XX_MIISET(sc, ED_DL10XX_MII_DIROUT_19);

	for (i = nbits - 1; i >= 0; i--) {
		if ((val >> i) & 1)
			DL10XX_MIISET(sc, ED_DL10XX_MII_DATAOUT);
		else
			DL10XX_MIICLR(sc, ED_DL10XX_MII_DATAOUT);
		DELAY(10);
		DL10XX_MIISET(sc, ED_DL10XX_MII_CLK);
		DELAY(10);
		DL10XX_MIICLR(sc, ED_DL10XX_MII_CLK);
		DELAY(10);
	}
}

static u_int
ed_pccard_dl10xx_mii_readbits(struct ed_softc *sc, int nbits)
{
	int i;
	u_int val = 0;

	if (sc->chip_type == ED_CHIP_TYPE_DL10022)
		DL10XX_MIICLR(sc, ED_DL10XX_MII_DIROUT_22);
	else
		DL10XX_MIICLR(sc, ED_DL10XX_MII_DIROUT_19);

	for (i = nbits - 1; i >= 0; i--) {
		DL10XX_MIISET(sc, ED_DL10XX_MII_CLK);
		DELAY(10);
		val <<= 1;
		if (ed_asic_inb(sc, ED_DL10XX_MIIBUS) & ED_DL10XX_MII_DATATIN)
			val++;
		DL10XX_MIICLR(sc, ED_DL10XX_MII_CLK);
		DELAY(10);
	}
	return val;
}
#endif

static void
ed_pccard_ax88x90_geteprom(struct ed_softc *sc)
{
	int prom[16],i;
	u_char tmp;
	struct {
		unsigned char offset, value;
	} pg_seq[] = {
						/* Select Page0 */
		{ED_P0_CR, ED_CR_RD2 | ED_CR_STP | ED_CR_PAGE_0},
		{ED_P0_DCR, 0x01},
		{ED_P0_RBCR0, 0x00},		/* Clear the count regs. */
		{ED_P0_RBCR1, 0x00},
		{ED_P0_IMR, 0x00},		/* Mask completion irq. */
		{ED_P0_ISR, 0xff},
		{ED_P0_RCR, ED_RCR_MON | ED_RCR_INTT}, /* Set To Monitor */
		{ED_P0_TCR, ED_TCR_LB0},	/* loopback mode. */
		{ED_P0_RBCR0, 32},
		{ED_P0_RBCR1, 0x00},
		{ED_P0_RSAR0, 0x00},
		{ED_P0_RSAR1, 0x04},
		{ED_P0_CR, ED_CR_RD0 | ED_CR_STA | ED_CR_PAGE_0},
	};

	/* Reset Card */
	tmp = ed_asic_inb(sc, ED_NOVELL_RESET);
	ed_asic_outb(sc, ED_NOVELL_RESET, tmp);
	DELAY(5000);
	ed_asic_outb(sc, ED_P0_CR, ED_CR_RD2 | ED_CR_STP | ED_CR_PAGE_0);
	DELAY(5000);

	/* Card Settings */
	for (i = 0; i < sizeof(pg_seq) / sizeof(pg_seq[0]); i++)
		ed_nic_outb(sc, pg_seq[i].offset, pg_seq[i].value);

	/* Get Data */
	for (i = 0; i < 16; i++)
		prom[i] = ed_asic_inw(sc, 0);

	/*
	 * Work around a bug I've seen on Linksys EC2T cards.  On
	 * these cards, the node address is contained in the low order
	 * bytes of the prom, with the upper byte being 0.  On other
	 * cards, the bytes are packed two per word.  I'm unsure why
	 * this is the case, and why no other open source OS has a
	 * similar workaround.  The Linksys EC2T card is still extremely
	 * popular on E-Bay, fetching way more than any other 10Mbps
	 * only card.  I might be able to test to see if prom[7] and
	 * prom[15] == 0x5757, since that appears to be a reliable
	 * test.  On the EC2T cards, I get 0x0057 in prom[14,15] instead.
	 */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		if (prom[i] & 0xff00)
			break;
	if (i == ETHER_ADDR_LEN) {
		for (i = 0; i < ETHER_ADDR_LEN; i++)
			sc->enaddr[i] = prom[i] & 0xff;
	} else {
		for (i = 0; i < ETHER_ADDR_LEN; i += 2) {
			sc->enaddr[i] = prom[i / 2] & 0xff;
			sc->enaddr[i + 1] = (prom[i / 2] >> 8) & 0xff;
		}
	}
}

/*
 * Special setup for AX88[17]90
 */
static int
ed_pccard_ax88x90(device_t dev, const struct ed_product *pp)
{
	int	error;
	int	iobase;
	char *ts;
	struct	ed_softc *sc = device_get_softc(dev);

	if (!pp->flags & NE2000DVF_AX88X90)
		return (ENXIO);

	/* XXX
	 * Set Attribute Memory IOBASE Register.  Is this a deficiency in
	 * the PC Card layer, or an ax88190 specific issue?  The card
	 * definitely doesn't work without it.
	 */
	iobase = rman_get_start(sc->port_res);
	ed_pccard_memwrite(dev, ED_AX88190_IOBASE0, iobase & 0xff);
	ed_pccard_memwrite(dev, ED_AX88190_IOBASE1, (iobase >> 8) & 0xff);
	ts = "AX88190";
	if (ed_asic_inb(sc, ED_ASIX_TEST) != 0) {
		ed_pccard_memwrite(dev, ED_AX88790_CSR, ED_AX88790_CSR_PWRDWN);
		ts = "AX88790";
	}
	sc->chip_type = ED_CHIP_TYPE_AX88190;
	ed_pccard_ax88x90_geteprom(sc);
	error = ed_probe_Novell_generic(dev, device_get_flags(dev));
	if (error == 0) {
		sc->vendor = ED_VENDOR_NOVELL;
		sc->type = ED_TYPE_NE2000;
		sc->chip_type = ED_CHIP_TYPE_AX88190;
		sc->type_str = ts;
	}
	return (error);
}

static device_method_t ed_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ed_pccard_probe),
	DEVMETHOD(device_attach,	ed_pccard_attach),
	DEVMETHOD(device_detach,	ed_detach),

#ifndef ED_NO_MIIBUS
	/* Bus interface */
	DEVMETHOD(bus_child_detached,	ed_child_detached),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	ed_miibus_readreg),
	DEVMETHOD(miibus_writereg,	ed_miibus_writereg),
#endif

	{ 0, 0 }
};

static driver_t ed_pccard_driver = {
	"ed",
	ed_pccard_methods,
	sizeof(struct ed_softc)
};

DRIVER_MODULE(ed, pccard, ed_pccard_driver, ed_devclass, 0, 0);
#ifndef ED_NO_MIIBUS
DRIVER_MODULE(miibus, ed, miibus_driver, miibus_devclass, 0, 0);
#endif
