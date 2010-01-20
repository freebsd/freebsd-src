/*-
 * Copyright (c) 2005, M. Warner Losh
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

/*
 * Notes for adding media support.  Each chipset is somewhat different
 * from the others.  Linux has a table of OIDs that it uses to see what
 * supports the misc register of the NS83903.  But a sampling of datasheets
 * I could dig up on cards I own paints a different picture.
 *
 * Chipset specific details:
 * NS 83903/902A paired
 *    ccr base 0x1020
 *    id register at 0x1000: 7-3 = 0, 2-0 = 1.
 *	(maybe this test is too week)
 *    misc register at 0x018:
 *	6 WAIT_TOUTENABLE enable watchdog timeout
 *	3 AUI/TPI 1 AUX, 0 TPI
 *	2 loopback
 *      1 gdlink (tpi mode only) 1 tp good, 0 tp bad
 *	0 0-no mam, 1 mam connected
 * NS83926 appears to be a NS pcmcia glue chip used on the IBM Ethernet II
 * and the NEC PC9801N-J12 ccr base 0x2000!
 *
 * winbond 289c926
 *    ccr base 0xfd0
 *    cfb (am 0xff2):
 *	0-1 PHY01	00 TPI, 01 10B2, 10 10B5, 11 TPI (reduced squ)
 *	2 LNKEN		0 - enable link and auto switch, 1 disable
 *	3 LNKSTS	TPI + LNKEN=0 + link good == 1, else 0
 *    sr (am 0xff4)
 *	88 00 88 00 88 00, etc
 *
 * TMI tc3299a (cr PHY01 == 0)
 *    ccr base 0x3f8
 *    cra (io 0xa)
 *    crb (io 0xb)
 *	0-1 PHY01	00 auto, 01 res, 10 10B5, 11 TPI
 *	2 GDLINK	1 disable checking of link
 *	6 LINK		0 bad link, 1 good link
 * TMI tc5299 10/100 chip, has a different MII interaction than
 * dl100xx and ax88x90.
 *
 * EN5017A, EN5020	no data, but very popular
 * Other chips?
 * NetBSD supports RTL8019, but none have surfaced that I can see
 */

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
#include <dev/ed/ax88x90reg.h>
#include <dev/ed/dl100xxreg.h>
#include <dev/ed/tc5299jreg.h>
#include <dev/pccard/pccardvar.h>
#include <dev/pccard/pccardreg.h>
#include <dev/pccard/pccard_cis.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include "card_if.h"
/* "device miibus" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"
#include "pccarddevs.h"

/*
 * NE-2000 based PC Cards have a number of ways to get the MAC address.
 * Some cards encode this as a FUNCE.  Others have this in the ROMs the
 * same way that ISA cards do.  Some have it encoded in the attribute
 * memory somewhere that isn't in the CIS.  Some new chipsets have it
 * in special registers in the ASIC part of the chip.
 *
 * For those cards that have the MAC adress stored in attribute memory,
 * nearly all of them have it at a fixed offset (0xff0).  We use that
 * offset as a source of last resource if other offsets have failed.
 */
#define ED_DEFAULT_MAC_OFFSET	0xff0

static const struct ed_product {
	struct pccard_product	prod;
	int flags;
#define	NE2000DVF_DL100XX	0x0001		/* chip is D-Link DL10019/22 */
#define	NE2000DVF_AX88X90	0x0002		/* chip is ASIX AX88[17]90 */
#define NE2000DVF_TC5299J	0x0004		/* chip is Tamarack TC5299J */
#define NE2000DVF_ENADDR	0x0100		/* Get MAC from attr mem */
#define NE2000DVF_ANYFUNC	0x0200		/* Allow any function type */
#define NE2000DVF_MODEM		0x0400		/* Has a modem/serial */
	int enoff;
} ed_pccard_products[] = {
	{ PCMCIA_CARD(ACCTON, EN2212), 0},
	{ PCMCIA_CARD(ACCTON, EN2216), 0},
	{ PCMCIA_CARD(ALLIEDTELESIS, LA_PCM), 0},
	{ PCMCIA_CARD(AMBICOM, AMB8002T), 0},
	{ PCMCIA_CARD(BILLIONTON, CFLT10N), 0},
	{ PCMCIA_CARD(BILLIONTON, LNA100B), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(BILLIONTON, LNT10TN), 0},
	{ PCMCIA_CARD(BROMAX, IPORT), 0},
	{ PCMCIA_CARD(BROMAX, IPORT2), 0},
	{ PCMCIA_CARD(BUFFALO, LPC2_CLT), 0},
	{ PCMCIA_CARD(BUFFALO, LPC3_CLT), 0},
	{ PCMCIA_CARD(BUFFALO, LPC3_CLX), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(BUFFALO, LPC4_TX), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(BUFFALO, LPC4_CLX), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(BUFFALO, LPC_CF_CLT), 0},
	{ PCMCIA_CARD(CNET, NE2000), 0},
	{ PCMCIA_CARD(COMPEX, AX88190), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(COMPEX, LANMODEM), 0},
	{ PCMCIA_CARD(COMPEX, LINKPORT_ENET_B), 0},
	{ PCMCIA_CARD(COREGA, ETHER_II_PCC_T), 0},
	{ PCMCIA_CARD(COREGA, ETHER_II_PCC_TD), 0},
	{ PCMCIA_CARD(COREGA, ETHER_PCC_T), 0},
	{ PCMCIA_CARD(COREGA, ETHER_PCC_TD), 0},
	{ PCMCIA_CARD(COREGA, FAST_ETHER_PCC_TX), NE2000DVF_DL100XX},
	{ PCMCIA_CARD(COREGA, FETHER_PCC_TXD), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(COREGA, FETHER_PCC_TXF), NE2000DVF_DL100XX},
	{ PCMCIA_CARD(DAYNA, COMMUNICARD_E_1), 0},
	{ PCMCIA_CARD(DAYNA, COMMUNICARD_E_2), 0},
	{ PCMCIA_CARD(DLINK, DE650), 0 },
	{ PCMCIA_CARD(DLINK, DE660), 0 },
	{ PCMCIA_CARD(DLINK, DE660PLUS), 0},
	{ PCMCIA_CARD(DYNALINK, L10C), 0},
	{ PCMCIA_CARD(EDIMAX, EP4000A), 0},
	{ PCMCIA_CARD(EPSON, EEN10B), NE2000DVF_ENADDR, 0xff0},
	{ PCMCIA_CARD(EXP, THINLANCOMBO), 0},
	{ PCMCIA_CARD(GLOBALVILLAGE, LANMODEM), 0},
	{ PCMCIA_CARD(GREY_CELL, TDK3000), 0},
	{ PCMCIA_CARD(GREY_CELL, DMF650TX),
	    NE2000DVF_ANYFUNC | NE2000DVF_DL100XX | NE2000DVF_MODEM},
	{ PCMCIA_CARD(IBM, HOME_AND_AWAY), 0},
	{ PCMCIA_CARD(IBM, INFOMOVER), NE2000DVF_ENADDR, 0xff0},
	{ PCMCIA_CARD(IODATA3, PCLAT), 0},
	{ PCMCIA_CARD(KINGSTON, CIO10T), 0},
	{ PCMCIA_CARD(KINGSTON, KNE2), 0},
	{ PCMCIA_CARD(LANTECH, FASTNETTX), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(LINKSYS, COMBO_ECARD),
	    NE2000DVF_DL100XX | NE2000DVF_AX88X90},
	{ PCMCIA_CARD(LINKSYS, ECARD_1), 0},
	{ PCMCIA_CARD(LINKSYS, ECARD_2), 0},
	{ PCMCIA_CARD(LINKSYS, ETHERFAST), NE2000DVF_DL100XX},
	{ PCMCIA_CARD(LINKSYS, TRUST_COMBO_ECARD), 0},
	{ PCMCIA_CARD(MACNICA, ME1_JEIDA), 0},
	{ PCMCIA_CARD(MAGICRAM, ETHER), 0},
	{ PCMCIA_CARD(MELCO, LPC3_CLX), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(MELCO, LPC3_TX), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(MICRORESEARCH, MR10TPC), 0},
	{ PCMCIA_CARD(NDC, ND5100_E), 0},
	{ PCMCIA_CARD(NETGEAR, FA410TXC), NE2000DVF_DL100XX},
	/* Same ID as DLINK DFE-670TXD.  670 has DL10022, fa411 has ax88790 */
	{ PCMCIA_CARD(NETGEAR, FA411), NE2000DVF_AX88X90 | NE2000DVF_DL100XX},
	{ PCMCIA_CARD(NEXTCOM, NEXTHAWK), 0},
	{ PCMCIA_CARD(NEWMEDIA, LANSURFER), 0},
	{ PCMCIA_CARD(OEM2, ETHERNET), 0},
	{ PCMCIA_CARD(OEM2, FAST_ETHERNET), NE2000DVF_AX88X90 },
	{ PCMCIA_CARD(OEM2, NE2000), 0},
	{ PCMCIA_CARD(PLANET, SMARTCOM2000), 0 },
	{ PCMCIA_CARD(PREMAX, PE200), 0},
	{ PCMCIA_CARD(PSION, LANGLOBAL),
	    NE2000DVF_ANYFUNC | NE2000DVF_AX88X90 | NE2000DVF_MODEM},
	{ PCMCIA_CARD(RACORE, ETHERNET), 0},
	{ PCMCIA_CARD(RACORE, FASTENET), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(RACORE, 8041TX), NE2000DVF_AX88X90 | NE2000DVF_TC5299J},
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
	{ PCMCIA_CARD(TELECOMDEVICE, TCD_HPC100), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(ZONET, ZEN), 0},
	{ { NULL } }
};

/*
 *      PC Card (PCMCIA) specific code.
 */
static int	ed_pccard_probe(device_t);
static int	ed_pccard_attach(device_t);

static int	ed_pccard_dl100xx(device_t dev, const struct ed_product *);
static void	ed_pccard_dl100xx_mii_reset(struct ed_softc *sc);
static u_int	ed_pccard_dl100xx_mii_readbits(struct ed_softc *sc, int nbits);
static void	ed_pccard_dl100xx_mii_writebits(struct ed_softc *sc, u_int val,
    int nbits);

static int	ed_pccard_ax88x90(device_t dev, const struct ed_product *);
static void	ed_pccard_ax88x90_mii_reset(struct ed_softc *sc);
static u_int	ed_pccard_ax88x90_mii_readbits(struct ed_softc *sc, int nbits);
static void	ed_pccard_ax88x90_mii_writebits(struct ed_softc *sc, u_int val,
    int nbits);

static int	ed_miibus_readreg(device_t dev, int phy, int reg);
static int	ed_ifmedia_upd(struct ifnet *);
static void	ed_ifmedia_sts(struct ifnet *, struct ifmediareq *);

static int	ed_pccard_tc5299j(device_t dev, const struct ed_product *);
static void	ed_pccard_tc5299j_mii_reset(struct ed_softc *sc);
static u_int	ed_pccard_tc5299j_mii_readbits(struct ed_softc *sc, int nbits);
static void	ed_pccard_tc5299j_mii_writebits(struct ed_softc *sc, u_int val,
    int nbits);

static void
ed_pccard_print_entry(const struct ed_product *pp)
{
	int i;

	printf("Product entry: ");
	if (pp->prod.pp_name)
		printf("name='%s',", pp->prod.pp_name);
	printf("vendor=%#x,product=%#x", pp->prod.pp_vendor,
	    pp->prod.pp_product);
	for (i = 0; i < 4; i++)
		if (pp->prod.pp_cis[i])
			printf(",CIS%d='%s'", i, pp->prod.pp_cis[i]);
	printf("\n");
}

static int
ed_pccard_probe(device_t dev)
{
	const struct ed_product *pp, *pp2;
	int		error, first = 1;
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
		/*
		 * Some devices match multiple entries.  Report that
		 * as a warning to help cull the table
		 */
		pp2 = pp;
		while ((pp2 = (const struct ed_product *)pccard_product_lookup(
		    dev, (const struct pccard_product *)(pp2 + 1),
		    sizeof(ed_pccard_products[0]), NULL)) != NULL) {
			if (first) {
				device_printf(dev,
    "Warning: card matches multiple entries.  Report to imp@freebsd.org\n");
				ed_pccard_print_entry(pp);
				first = 0;
			}
			ed_pccard_print_entry(pp2);
		}
		
		return (0);
	}
	return (ENXIO);
}

static int
ed_pccard_rom_mac(device_t dev, uint8_t *enaddr)
{
	struct ed_softc *sc = device_get_softc(dev);
	uint8_t romdata[32];
	int i;

	/*
	 * Read in the rom data at location 0.  Since there are no
	 * NE-1000 based PC Card devices, we'll assume we're 16-bit.
	 *
	 * In researching what format this takes, I've found that the
	 * following appears to be true for multiple cards based on
	 * observation as well as datasheet digging.
	 *
	 * Data is stored in some ROM and is copied out 8 bits at a time
	 * into 16-bit wide locations.  This means that the odd locations
	 * of the ROM are not used (and can be either 0 or ff).
	 *
	 * The contents appears to be as follows:
	 * PROM   RAM
	 * Offset Offset	What
	 *  0      0	ENETADDR 0
	 *  1      2	ENETADDR 1
	 *  2      4	ENETADDR 2
	 *  3      6	ENETADDR 3
	 *  4      8	ENETADDR 4
	 *  5     10	ENETADDR 5
	 *  6-13  12-26 Reserved (varies by manufacturer)
	 * 14     28	0x57
	 * 15     30    0x57
	 *
	 * Some manufacturers have another image of enetaddr from
	 * PROM offset 0x10 to 0x15 with 0x42 in 0x1e and 0x1f, but
	 * this doesn't appear to be universally documented in the
	 * datasheets.  Some manufactuers have a card type, card config
	 * checksums, etc encoded into PROM offset 6-13, but deciphering it
	 * requires more knowledge about the exact underlying chipset than
	 * we possess (and maybe can possess).
	 */
	ed_pio_readmem(sc, 0, romdata, 32);
	if (bootverbose)
		printf("ROM DATA: %32D\n", romdata, " ");
	if (romdata[28] != 0x57 || romdata[30] != 0x57)
		return (0);
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		enaddr[i] = romdata[i * 2];
	return (1);
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
ed_pccard_media_ioctl(struct ed_softc *sc, struct ifreq *ifr, u_long command)
{
	struct mii_data *mii;

	if (sc->miibus == NULL)
		return (EINVAL);
	mii = device_get_softc(sc->miibus);
	return (ifmedia_ioctl(sc->ifp, ifr, &mii->mii_media, command));
}


static void
ed_pccard_mediachg(struct ed_softc *sc)
{
	struct mii_data *mii;

	if (sc->miibus == NULL)
		return;
	mii = device_get_softc(sc->miibus);
	mii_mediachg(mii);
}

static void
ed_pccard_tick(void *arg)
{
	struct ed_softc *sc = arg;
	struct mii_data *mii;
	int media = 0;

	ED_ASSERT_LOCKED(sc);
	if (sc->miibus != NULL) {
		mii = device_get_softc(sc->miibus);
		media = mii->mii_media_status;
		mii_tick(mii);
		if (mii->mii_media_status & IFM_ACTIVE &&
		    media != mii->mii_media_status && 0 &&
		    sc->chip_type == ED_CHIP_TYPE_DL10022) {
			ed_asic_outb(sc, ED_DL100XX_DIAG,
			    (mii->mii_media_active & IFM_FDX) ?
			    ED_DL100XX_COLLISON_DIS : 0);
		}
		
	}
	callout_reset(&sc->tick_ch, hz, ed_pccard_tick, sc);
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
	if (error != 0)
		error = ed_pccard_ax88x90(dev, pp);
	if (error != 0)
		error = ed_pccard_tc5299j(dev, pp);
	if (error != 0)
		error = ed_probe_Novell_generic(dev, device_get_flags(dev));
	if (error)
		goto bad;

	error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, edintr, sc, &sc->irq_handle);
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
				pccard_attr_read_1(dev, pp->enoff + i * 2,
				    enaddr + i);
				sum |= enaddr[i];
			}
			if (bootverbose)
				device_printf(dev, "Hint %x MAC %6D\n",
				    pp->enoff, enaddr, ":");
		}
		if (sum == 0) {
			for (i = 0; i < ETHER_ADDR_LEN; i++) {
				pccard_attr_read_1(dev, ED_DEFAULT_MAC_OFFSET +
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
 	if (sc->chip_type == ED_CHIP_TYPE_DL10019 ||
	    sc->chip_type == ED_CHIP_TYPE_DL10022) {
		/* Probe for an MII bus, but ignore errors. */
		ed_pccard_dl100xx_mii_reset(sc);
		(void)mii_phy_probe(dev, &sc->miibus, ed_ifmedia_upd,
		    ed_ifmedia_sts);
	} else if (sc->chip_type == ED_CHIP_TYPE_AX88190) {
		ed_pccard_ax88x90_mii_reset(sc);
		if ((error = mii_phy_probe(dev, &sc->miibus, ed_ifmedia_upd,
		     ed_ifmedia_sts)) != 0) {
			device_printf(dev, "Missing mii!\n");
			goto bad;
		}
		    
	} else if (sc->chip_type == ED_CHIP_TYPE_TC5299J) {
		ed_pccard_tc5299j_mii_reset(sc);
		if ((error = mii_phy_probe(dev, &sc->miibus, ed_ifmedia_upd,
		     ed_ifmedia_sts)) != 0) {
			device_printf(dev, "Missing mii!\n");
			goto bad;
		}
		    
	}
	if (sc->miibus != NULL) {
		sc->sc_tick = ed_pccard_tick;
		sc->sc_mediachg = ed_pccard_mediachg;
		sc->sc_media_ioctl = ed_pccard_media_ioctl;
	}
	if (sc->modem_rid != -1)
		ed_pccard_add_modem(dev);
	return (0);
bad:
	ed_release_resources(dev);
	return (error);
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

	if (!(pp->flags & NE2000DVF_DL100XX))
		return (ENXIO);
	if (bootverbose)
		device_printf(dev, "Trying DL100xx probing\n");
	error = ed_probe_Novell_generic(dev, device_get_flags(dev));
	if (bootverbose && error)
		device_printf(dev, "Novell generic probe failed: %d\n", error);
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
	if (sum != 0xff) {
		if (bootverbose)
			device_printf(dev, "Bad checksum %#x\n", sum);
		return (ENXIO);		/* invalid DL10019C */
	}
	if (bootverbose)
		device_printf(dev, "CIR is %d\n", ed_asic_inb(sc, 0xa));
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
	sc->mii_readbits = ed_pccard_dl100xx_mii_readbits;
	sc->mii_writebits = ed_pccard_dl100xx_mii_writebits;
	return (0);
}

/* MII bit-twiddling routines for cards using Dlink chipset */
#define DL100XX_MIISET(sc, x) ed_asic_outb(sc, ED_DL100XX_MIIBUS, \
    ed_asic_inb(sc, ED_DL100XX_MIIBUS) | (x))
#define DL100XX_MIICLR(sc, x) ed_asic_outb(sc, ED_DL100XX_MIIBUS, \
    ed_asic_inb(sc, ED_DL100XX_MIIBUS) & ~(x))

static void
ed_pccard_dl100xx_mii_reset(struct ed_softc *sc)
{
	if (sc->chip_type != ED_CHIP_TYPE_DL10022)
		return;

	ed_asic_outb(sc, ED_DL100XX_MIIBUS, ED_DL100XX_MII_RESET2);
	DELAY(10);
	ed_asic_outb(sc, ED_DL100XX_MIIBUS,
	    ED_DL100XX_MII_RESET2 | ED_DL100XX_MII_RESET1);
	DELAY(10);
	ed_asic_outb(sc, ED_DL100XX_MIIBUS, ED_DL100XX_MII_RESET2);
	DELAY(10);
	ed_asic_outb(sc, ED_DL100XX_MIIBUS,
	    ED_DL100XX_MII_RESET2 | ED_DL100XX_MII_RESET1);
	DELAY(10);
	ed_asic_outb(sc, ED_DL100XX_MIIBUS, 0);
}

static void
ed_pccard_dl100xx_mii_writebits(struct ed_softc *sc, u_int val, int nbits)
{
	int i;

	if (sc->chip_type == ED_CHIP_TYPE_DL10022)
		DL100XX_MIISET(sc, ED_DL100XX_MII_DIROUT_22);
	else
		DL100XX_MIISET(sc, ED_DL100XX_MII_DIROUT_19);

	for (i = nbits - 1; i >= 0; i--) {
		if ((val >> i) & 1)
			DL100XX_MIISET(sc, ED_DL100XX_MII_DATAOUT);
		else
			DL100XX_MIICLR(sc, ED_DL100XX_MII_DATAOUT);
		DELAY(10);
		DL100XX_MIISET(sc, ED_DL100XX_MII_CLK);
		DELAY(10);
		DL100XX_MIICLR(sc, ED_DL100XX_MII_CLK);
		DELAY(10);
	}
}

static u_int
ed_pccard_dl100xx_mii_readbits(struct ed_softc *sc, int nbits)
{
	int i;
	u_int val = 0;

	if (sc->chip_type == ED_CHIP_TYPE_DL10022)
		DL100XX_MIICLR(sc, ED_DL100XX_MII_DIROUT_22);
	else
		DL100XX_MIICLR(sc, ED_DL100XX_MII_DIROUT_19);

	for (i = nbits - 1; i >= 0; i--) {
		DL100XX_MIISET(sc, ED_DL100XX_MII_CLK);
		DELAY(10);
		val <<= 1;
		if (ed_asic_inb(sc, ED_DL100XX_MIIBUS) & ED_DL100XX_MII_DATAIN)
			val++;
		DL100XX_MIICLR(sc, ED_DL100XX_MII_CLK);
		DELAY(10);
	}
	return val;
}

static int
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
	ed_nic_outb(sc, ED_P0_CR, ED_CR_RD2 | ED_CR_STP | ED_CR_PAGE_0);
	DELAY(5000);

	/* Card Settings */
	for (i = 0; i < sizeof(pg_seq) / sizeof(pg_seq[0]); i++)
		ed_nic_outb(sc, pg_seq[i].offset, pg_seq[i].value);

	/* Get Data */
	for (i = 0; i < ETHER_ADDR_LEN / 2; i++)
		prom[i] = ed_asic_inw(sc, 0);
	for (i = 0; i < ETHER_ADDR_LEN; i += 2) {
		sc->enaddr[i] = prom[i / 2] & 0xff;
		sc->enaddr[i + 1] = (prom[i / 2] >> 8) & 0xff;
	}
	return (0);
}

/*
 * Special setup for AX88[17]90
 */
static int
ed_pccard_ax88x90(device_t dev, const struct ed_product *pp)
{
	int	error, iobase, i, id;
	char *ts;
	struct	ed_softc *sc = device_get_softc(dev);

	if (!(pp->flags & NE2000DVF_AX88X90))
		return (ENXIO);

	if (bootverbose)
		device_printf(dev, "Checking AX88x90\n");

	/*
	 * Set the IOBASE Register.  The AX88x90 cards are potentially
	 * multifunction cards, and thus requires a slight workaround.
	 * We write the address the card is at.
	 */
	iobase = rman_get_start(sc->port_res);
	pccard_ccr_write_1(dev, PCCARD_CCR_IOBASE0, iobase & 0xff);
	pccard_ccr_write_1(dev, PCCARD_CCR_IOBASE1, (iobase >> 8) & 0xff);

	ts = "AX88190";
	if (ed_asic_inb(sc, ED_AX88X90_TEST) != 0) {
		/*
		 * AX88790 (and I think AX88190A) chips need to be
		 * powered down.  There's an erratum that says we should
		 * power down the PHY for 2.5s, but this seems to power
		 * down the whole card.  I'm unsure why this was done, but
		 * appears to be required for proper operation.
		 */
		pccard_ccr_write_1(dev, PCCARD_CCR_STATUS,
		    PCCARD_CCR_STATUS_PWRDWN);
		/*
		 * Linux axnet driver selects the internal phy for the ax88790
		 */
		ed_asic_outb(sc, ED_AX88X90_GPIO, ED_AX88X90_GPIO_INT_PHY);
		ts = "AX88790";
	}

	/*
	 * Check to see if we have a MII PHY ID at any of the first 17
	 * locations.  All AX88x90 devices have MII and a PHY, so we use
	 * this to weed out chips that would otherwise make it through
	 * the tests we have after this point.
	 */
	sc->mii_readbits = ed_pccard_ax88x90_mii_readbits;
	sc->mii_writebits = ed_pccard_ax88x90_mii_writebits;
	for (i = 0; i < 17; i++) {
		id = ed_miibus_readreg(dev, i, MII_PHYIDR1);
		if (id != 0 && id != 0xffff)
			break;
	}
	if (i == 17) {
		sc->mii_readbits = 0;
		sc->mii_writebits = 0;
		return (ENXIO);
	}
	
	sc->chip_type = ED_CHIP_TYPE_AX88190;
	error = ed_pccard_ax88x90_geteprom(sc);
	if (error)
		return (error);
	error = ed_probe_Novell_generic(dev, device_get_flags(dev));
	if (bootverbose)
		device_printf(dev, "probe novel returns %d\n", error);
	if (error == 0) {
		sc->vendor = ED_VENDOR_NOVELL;
		sc->type = ED_TYPE_NE2000;
		sc->chip_type = ED_CHIP_TYPE_AX88190;
		sc->type_str = ts;
	}
	return (error);
}

/* MII bit-twiddling routines for cards using AX88x90 chipset */
#define AX88X90_MIISET(sc, x) ed_asic_outb(sc, ED_AX88X90_MIIBUS, \
    ed_asic_inb(sc, ED_AX88X90_MIIBUS) | (x))
#define AX88X90_MIICLR(sc, x) ed_asic_outb(sc, ED_AX88X90_MIIBUS, \
    ed_asic_inb(sc, ED_AX88X90_MIIBUS) & ~(x))

static void
ed_pccard_ax88x90_mii_reset(struct ed_softc *sc)
{
	/* Do nothing! */
}

static void
ed_pccard_ax88x90_mii_writebits(struct ed_softc *sc, u_int val, int nbits)
{
	int i;

	AX88X90_MIICLR(sc, ED_AX88X90_MII_DIROUT);
	for (i = nbits - 1; i >= 0; i--) {
		if ((val >> i) & 1)
			AX88X90_MIISET(sc, ED_AX88X90_MII_DATAOUT);
		else
			AX88X90_MIICLR(sc, ED_AX88X90_MII_DATAOUT);
		DELAY(10);
		AX88X90_MIISET(sc, ED_AX88X90_MII_CLK);
		DELAY(10);
		AX88X90_MIICLR(sc, ED_AX88X90_MII_CLK);
		DELAY(10);
	}
}

static u_int
ed_pccard_ax88x90_mii_readbits(struct ed_softc *sc, int nbits)
{
	int i;
	u_int val = 0;

	AX88X90_MIISET(sc, ED_AX88X90_MII_DIROUT);
	for (i = nbits - 1; i >= 0; i--) {
		AX88X90_MIISET(sc, ED_AX88X90_MII_CLK);
		DELAY(10);
		val <<= 1;
		if (ed_asic_inb(sc, ED_AX88X90_MIIBUS) & ED_AX88X90_MII_DATAIN)
			val++;
		AX88X90_MIICLR(sc, ED_AX88X90_MII_CLK);
		DELAY(10);
	}
	return val;
}

/*
 * Special setup for TC5299J
 */
static int
ed_pccard_tc5299j(device_t dev, const struct ed_product *pp)
{
	int	error, i, id;
	char *ts;
	struct	ed_softc *sc = device_get_softc(dev);

	if (!(pp->flags & NE2000DVF_TC5299J))
		return (ENXIO);

	if (bootverbose)
		device_printf(dev, "Checking Tc5299j\n");

	/*
	 * Check to see if we have a MII PHY ID at any of the first 32
	 * locations.  All TC5299J devices have MII and a PHY, so we use
	 * this to weed out chips that would otherwise make it through
	 * the tests we have after this point.
	 */
	sc->mii_readbits = ed_pccard_tc5299j_mii_readbits;
	sc->mii_writebits = ed_pccard_tc5299j_mii_writebits;
	for (i = 0; i < 32; i++) {
		id = ed_miibus_readreg(dev, i, MII_PHYIDR1);
		if (id != 0 && id != 0xffff)
			break;
	}
	if (i == 32) {
		sc->mii_readbits = 0;
		sc->mii_writebits = 0;
		return (ENXIO);
	}

	ts = "TC5299J";
	error = ed_probe_Novell_generic(dev, device_get_flags(dev));
	if (bootverbose)
		device_printf(dev, "probe novel returns %d\n", error);
	if (error != 0) {
		sc->mii_readbits = 0;
		sc->mii_writebits = 0;
		return (error);
	}
	if (ed_pccard_rom_mac(dev, sc->enaddr) == 0) {
		sc->mii_readbits = 0;
		sc->mii_writebits = 0;
		return (ENXIO);
	}
	sc->vendor = ED_VENDOR_NOVELL;
	sc->type = ED_TYPE_NE2000;
	sc->chip_type = ED_CHIP_TYPE_TC5299J;
	sc->type_str = ts;
	return (0);
}

/* MII bit-twiddling routines for cards using TC5299J chipset */
#define TC5299J_MIISET(sc, x) ed_nic_outb(sc, ED_TC5299J_MIIBUS, \
    ed_nic_inb(sc, ED_TC5299J_MIIBUS) | (x))
#define TC5299J_MIICLR(sc, x) ed_nic_outb(sc, ED_TC5299J_MIIBUS, \
    ed_nic_inb(sc, ED_TC5299J_MIIBUS) & ~(x))

static void
ed_pccard_tc5299j_mii_reset(struct ed_softc *sc)
{
	/* Do nothing! */
}

static void
ed_pccard_tc5299j_mii_writebits(struct ed_softc *sc, u_int val, int nbits)
{
	int i;
	uint8_t cr;

	cr = ed_nic_inb(sc, ED_P0_CR);
	ed_nic_outb(sc, ED_P0_CR, cr | ED_CR_PAGE_3);

	TC5299J_MIICLR(sc, ED_TC5299J_MII_DIROUT);
	for (i = nbits - 1; i >= 0; i--) {
		if ((val >> i) & 1)
			TC5299J_MIISET(sc, ED_TC5299J_MII_DATAOUT);
		else
			TC5299J_MIICLR(sc, ED_TC5299J_MII_DATAOUT);
		TC5299J_MIISET(sc, ED_TC5299J_MII_CLK);
		TC5299J_MIICLR(sc, ED_TC5299J_MII_CLK);
	}
	ed_nic_outb(sc, ED_P0_CR, cr);
}

static u_int
ed_pccard_tc5299j_mii_readbits(struct ed_softc *sc, int nbits)
{
	int i;
	u_int val = 0;
	uint8_t cr;

	cr = ed_nic_inb(sc, ED_P0_CR);
	ed_nic_outb(sc, ED_P0_CR, cr | ED_CR_PAGE_3);

	TC5299J_MIISET(sc, ED_TC5299J_MII_DIROUT);
	for (i = nbits - 1; i >= 0; i--) {
		TC5299J_MIISET(sc, ED_TC5299J_MII_CLK);
		val <<= 1;
		if (ed_nic_inb(sc, ED_TC5299J_MIIBUS) & ED_TC5299J_MII_DATAIN)
			val++;
		TC5299J_MIICLR(sc, ED_TC5299J_MII_CLK);
	}
	ed_nic_outb(sc, ED_P0_CR, cr);
	return val;
}

/*
 * MII bus support routines.
 */
static int
ed_miibus_readreg(device_t dev, int phy, int reg)
{
	struct ed_softc *sc;
	int failed, val;

	/*
	 * The AX88790 seem to have phy 0..f external, and 0x10 internal.
	 * but they also seem to have a bogus one that shows up at phy
	 * 0x11 through 0x1f.
	 */
	if (phy >= 0x11)
		return (0);

	sc = device_get_softc(dev);
	(*sc->mii_writebits)(sc, 0xffffffff, 32);
	(*sc->mii_writebits)(sc, ED_MII_STARTDELIM, ED_MII_STARTDELIM_BITS);
	(*sc->mii_writebits)(sc, ED_MII_READOP, ED_MII_OP_BITS);
	(*sc->mii_writebits)(sc, phy, ED_MII_PHY_BITS);
	(*sc->mii_writebits)(sc, reg, ED_MII_REG_BITS);
	failed = (*sc->mii_readbits)(sc, ED_MII_ACK_BITS);
	val = (*sc->mii_readbits)(sc, ED_MII_DATA_BITS);
	(*sc->mii_writebits)(sc, ED_MII_IDLE, ED_MII_IDLE_BITS);
	return (failed ? 0 : val);
}

static void
ed_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct ed_softc *sc;

	/*
	 * The AX88790 seem to have phy 0..f external, and 0x10 internal.
	 * but they also seem to have a bogus one that shows up at phy
	 * 0x11 through 0x1f.
	 */
	if (phy >= 0x11)
		return;

	sc = device_get_softc(dev);
	(*sc->mii_writebits)(sc, 0xffffffff, 32);
	(*sc->mii_writebits)(sc, ED_MII_STARTDELIM, ED_MII_STARTDELIM_BITS);
	(*sc->mii_writebits)(sc, ED_MII_WRITEOP, ED_MII_OP_BITS);
	(*sc->mii_writebits)(sc, phy, ED_MII_PHY_BITS);
	(*sc->mii_writebits)(sc, reg, ED_MII_REG_BITS);
	(*sc->mii_writebits)(sc, ED_MII_TURNAROUND, ED_MII_TURNAROUND_BITS);
	(*sc->mii_writebits)(sc, data, ED_MII_DATA_BITS);
	(*sc->mii_writebits)(sc, ED_MII_IDLE, ED_MII_IDLE_BITS);
}

static int
ed_ifmedia_upd(struct ifnet *ifp)
{
	struct ed_softc *sc;
	struct mii_data *mii;

	sc = ifp->if_softc;
	if (sc->miibus == NULL)
		return (ENXIO);
	
	mii = device_get_softc(sc->miibus);
	return mii_mediachg(mii);
}

static void
ed_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct ed_softc *sc;
	struct mii_data *mii;

	sc = ifp->if_softc;
	if (sc->miibus == NULL)
		return;

	mii = device_get_softc(sc->miibus);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

static void
ed_child_detached(device_t dev, device_t child)
{
	struct ed_softc *sc;

	sc = device_get_softc(dev);
	if (child == sc->miibus)
		sc->miibus = NULL;
}

static device_method_t ed_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ed_pccard_probe),
	DEVMETHOD(device_attach,	ed_pccard_attach),
	DEVMETHOD(device_detach,	ed_detach),

	/* Bus interface */
	DEVMETHOD(bus_child_detached,	ed_child_detached),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	ed_miibus_readreg),
	DEVMETHOD(miibus_writereg,	ed_miibus_writereg),

	{ 0, 0 }
};

static driver_t ed_pccard_driver = {
	"ed",
	ed_pccard_methods,
	sizeof(struct ed_softc)
};

DRIVER_MODULE(ed, pccard, ed_pccard_driver, ed_devclass, 0, 0);
DRIVER_MODULE(miibus, ed, miibus_driver, miibus_devclass, 0, 0);
MODULE_DEPEND(ed, miibus, 1, 1, 1);
MODULE_DEPEND(ed, ether, 1, 1, 1);
