/*
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Lucent WaveLAN/IEEE 802.11 PCMCIA driver for FreeBSD.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

#include "opt_wi.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_ieee80211.h>

#include <dev/pccard/pccardvar.h>
#if __FreeBSD_version >= 500000
#include <dev/pccard/pccarddevs.h>
#endif

#include <dev/wi/if_wavelan_ieee.h>
#include <dev/wi/wi_hostap.h>
#include <dev/wi/if_wivar.h>
#include <dev/wi/if_wireg.h>
#ifdef WI_SYMBOL_FIRMWARE
#include <dev/wi/spectrum24t_cf.h>
#endif

#include "card_if.h"

#if !defined(lint)
static const char rcsid[] =
  "$FreeBSD$";
#endif

static int wi_pccard_probe(device_t);
static int wi_pccard_attach(device_t);

#ifdef WI_SYMBOL_FIRMWARE
/* support to download firmware for symbol CF card */
static int wi_pcmcia_load_firm(struct wi_softc *, const void *, int, const void *, int);
static int wi_pcmcia_write_firm(struct wi_softc *, const void *, int, const void *, int);
static int wi_pcmcia_set_hcr(struct wi_softc *, int);
#endif

#if __FreeBSD_version < 500000
static device_method_t wi_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		wi_pccard_probe),
	DEVMETHOD(device_attach,	wi_pccard_attach),
	DEVMETHOD(device_detach,	wi_generic_detach),
	DEVMETHOD(device_shutdown,	wi_shutdown),

	{ 0, 0 }
};

#else
static int wi_pccard_match(device_t);

static device_method_t wi_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pccard_compat_probe),
	DEVMETHOD(device_attach,	pccard_compat_attach),
	DEVMETHOD(device_detach,	wi_generic_detach),
	DEVMETHOD(device_shutdown,	wi_shutdown),

	/* Card interface */
	DEVMETHOD(card_compat_match,	wi_pccard_match),
	DEVMETHOD(card_compat_probe,	wi_pccard_probe),
	DEVMETHOD(card_compat_attach,	wi_pccard_attach),

	{ 0, 0 }
};

#endif

static driver_t wi_pccard_driver = {
	"wi",
	wi_pccard_methods,
	sizeof(struct wi_softc)
};

DRIVER_MODULE(if_wi, pccard, wi_pccard_driver, wi_devclass, 0, 0);

#if __FreeBSD_version >= 500000
static const struct pccard_product wi_pccard_products[] = {
	PCMCIA_CARD(3COM, 3CRWE737A, 0),
	PCMCIA_CARD(3COM, 3CRWE777A, 0),
	PCMCIA_CARD(ACTIONTEC, HWC01170, 0),
	PCMCIA_CARD(ADDTRON, AWP100, 0),
	PCMCIA_CARD(BUFFALO, WLI_PCM_S11, 0),
	PCMCIA_CARD(BUFFALO, WLI_CF_S11G, 0),
	PCMCIA_CARD(COMPAQ, NC5004, 0),
	PCMCIA_CARD(CONTEC, FX_DS110_PCC, 0),
	PCMCIA_CARD(COREGA, WIRELESS_LAN_PCC_11, 0),
	PCMCIA_CARD(COREGA, WIRELESS_LAN_PCCA_11, 0),
	PCMCIA_CARD(COREGA, WIRELESS_LAN_PCCB_11, 0),
	PCMCIA_CARD(ELSA, XI300_IEEE, 0),
	PCMCIA_CARD(ELSA, XI325_IEEE, 0),
	PCMCIA_CARD(ELSA, XI800_IEEE, 0),
	PCMCIA_CARD(EMTAC, WLAN, 0),
	PCMCIA_CARD(ERICSSON, WIRELESSLAN, 0),
	PCMCIA_CARD(GEMTEK, WLAN, 0),
	PCMCIA_CARD(HWN, AIRWAY80211, 0), 
	PCMCIA_CARD(INTEL, PRO_WLAN_2011, 0),
	PCMCIA_CARD(INTERSIL, PRISM2, 0),
	PCMCIA_CARD(IODATA2, WNB11PCM, 0),
	PCMCIA_CARD(LINKSYS2, IWN, 0),
	PCMCIA_CARD(LINKSYS2, IWN2, 0),
	/* Now that we do PRISM detection, I don't think we need these - imp */
	PCMCIA_CARD2(LUCENT, WAVELAN_IEEE, NANOSPEED_PRISM2, 0),
	PCMCIA_CARD2(LUCENT, WAVELAN_IEEE, NEC_CMZ_RT_WP, 0),
	PCMCIA_CARD2(LUCENT, WAVELAN_IEEE, NTT_ME_WLAN, 0),
	PCMCIA_CARD2(LUCENT, WAVELAN_IEEE, SMC_2632W, 0),
	/* Must be after other LUCENT ones because it is less specific */
	PCMCIA_CARD(LUCENT, WAVELAN_IEEE, 0),
	PCMCIA_CARD(NETGEAR2, MA401RA, 0),
	PCMCIA_CARD(NOKIA, C110_WLAN, 0),
	PCMCIA_CARD(PROXIM, RANGELANDS_8430, 0),
	PCMCIA_CARD(SAMSUNG, SWL_2000N, 0),
	PCMCIA_CARD(SIMPLETECH, SPECTRUM24_ALT, 0),
	PCMCIA_CARD(SOCKET, LP_WLAN_CF, 0),
	PCMCIA_CARD(SYMBOL, LA4100, 0),
	PCMCIA_CARD(TDK, LAK_CD011WL, 0),
	{ NULL }
};

static int
wi_pccard_match(dev)
	device_t	dev;
{
	const struct pccard_product *pp;

	if ((pp = pccard_product_lookup(dev, wi_pccard_products,
	    sizeof(wi_pccard_products[0]), NULL)) != NULL) {
		device_set_desc(dev, pp->pp_name);
		return 0;
	}
	return ENXIO;
}
#endif

static int
wi_pccard_probe(dev)
	device_t	dev;
{
	struct wi_softc	*sc;
	int		error;

	sc = device_get_softc(dev);
	sc->wi_gone = 0;
	sc->wi_bus_type = WI_BUS_PCCARD;

	error = wi_alloc(dev, 0);
	if (error)
		return (error);

	wi_free(dev);

	/* Make sure interrupts are disabled. */
	CSR_WRITE_2(sc, WI_INT_EN, 0);
	CSR_WRITE_2(sc, WI_EVENT_ACK, 0xFFFF);

	return (0);
}

static int
wi_pccard_attach(device_t dev)
{
	struct wi_softc		*sc;
	int			error;
	uint32_t		vendor;
	uint32_t		product;

	sc = device_get_softc(dev);

	error = wi_alloc(dev, 0);
	if (error) {
		device_printf(dev, "wi_alloc() failed! (%d)\n", error);
		return (error);
	}

	/*
	 * The cute little Symbol LA4100-series CF cards need to have
	 * code downloaded to them.
	 */
	pccard_get_vendor(dev, &vendor);
	pccard_get_product(dev, &product);
	if (vendor == PCMCIA_VENDOR_SYMBOL &&
	    product == PCMCIA_PRODUCT_SYMBOL_LA4100) {
#ifdef WI_SYMBOL_FIRMWARE
		if (wi_pcmcia_load_firm(sc,
		    spectrum24t_primsym, sizeof(spectrum24t_primsym),
		    spectrum24t_secsym, sizeof(spectrum24t_secsym))) {
			device_printf(dev, "couldn't load firmware\n");
		}
#else
		device_printf(dev, 
		    "Symbol LA4100 needs 'option WI_SYMBOL_FIRMWARE'\n");
		return (ENXIO);
#endif
	}

	return (wi_generic_attach(dev));
}

#ifdef WI_SYMBOL_FIRMWARE

/*
 * Special routines to download firmware for Symbol CF card.
 * XXX: This should be modified generic into any PRISM-2 based card.
 */

#define	WI_SBCF_PDIADDR		0x3100

/* unaligned load little endian */
#define	GETLE32(p)	((p)[0] | ((p)[1]<<8) | ((p)[2]<<16) | ((p)[3]<<24))
#define	GETLE16(p)	((p)[0] | ((p)[1]<<8))

static int
wi_pcmcia_load_firm(struct wi_softc *sc, const void *primsym, int primlen,
    const void *secsym, int seclen)
{
	uint8_t ebuf[256];
	int i;

	/* load primary code and run it */
	wi_pcmcia_set_hcr(sc, WI_HCR_EEHOLD);
	if (wi_pcmcia_write_firm(sc, primsym, primlen, NULL, 0))
		return EIO;
	wi_pcmcia_set_hcr(sc, WI_HCR_RUN);
	for (i = 0; ; i++) {
		if (i == 10)
			return ETIMEDOUT;
		tsleep(sc, PWAIT, "wiinit", 1);
		if (CSR_READ_2(sc, WI_CNTL) == WI_CNTL_AUX_ENA_STAT)
			break;
		/* write the magic key value to unlock aux port */
		CSR_WRITE_2(sc, WI_PARAM0, WI_AUX_KEY0);
		CSR_WRITE_2(sc, WI_PARAM1, WI_AUX_KEY1);
		CSR_WRITE_2(sc, WI_PARAM2, WI_AUX_KEY2);
		CSR_WRITE_2(sc, WI_CNTL, WI_CNTL_AUX_ENA_CNTL);
	}

	/* issue read EEPROM command: XXX copied from wi_cmd() */
	CSR_WRITE_2(sc, WI_PARAM0, 0);
	CSR_WRITE_2(sc, WI_PARAM1, 0);
	CSR_WRITE_2(sc, WI_PARAM2, 0);
	CSR_WRITE_2(sc, WI_COMMAND, WI_CMD_READEE);
        for (i = 0; i < WI_TIMEOUT; i++) {
                if (CSR_READ_2(sc, WI_EVENT_STAT) & WI_EV_CMD)
                        break;
                DELAY(1);
        }
        CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_CMD);

	CSR_WRITE_2(sc, WI_AUX_PAGE, WI_SBCF_PDIADDR / WI_AUX_PGSZ);
	CSR_WRITE_2(sc, WI_AUX_OFFSET, WI_SBCF_PDIADDR % WI_AUX_PGSZ);
	CSR_READ_MULTI_STREAM_2(sc, WI_AUX_DATA,
	    (uint16_t *)ebuf, sizeof(ebuf) / 2);
	if (GETLE16(ebuf) > sizeof(ebuf))
		return EIO;
	if (wi_pcmcia_write_firm(sc, secsym, seclen, ebuf + 4, GETLE16(ebuf)))
		return EIO;
	return 0;
}

static int
wi_pcmcia_write_firm(struct wi_softc *sc, const void *buf, int buflen,
    const void *ebuf, int ebuflen)
{
	const uint8_t *p, *ep, *q, *eq;
	char *tp;
	uint32_t addr, id, eid;
	int i, len, elen, nblk, pdrlen;

	/*
	 * Parse the header of the firmware image.
	 */
	p = buf;
	ep = p + buflen;
	while (p < ep && *p++ != ' ');	/* FILE: */
	while (p < ep && *p++ != ' ');	/* filename */
	while (p < ep && *p++ != ' ');	/* type of the firmware */
	nblk = strtoul(p, &tp, 10);
	p = tp;
	pdrlen = strtoul(p + 1, &tp, 10);
	p = tp;
	while (p < ep && *p++ != 0x1a);	/* skip rest of header */

	/*
	 * Block records: address[4], length[2], data[length];
	 */
	for (i = 0; i < nblk; i++) {
		addr = GETLE32(p);	p += 4;
		len  = GETLE16(p);	p += 2;
		CSR_WRITE_2(sc, WI_AUX_PAGE, addr / WI_AUX_PGSZ);
		CSR_WRITE_2(sc, WI_AUX_OFFSET, addr % WI_AUX_PGSZ);
		CSR_WRITE_MULTI_STREAM_2(sc, WI_AUX_DATA,
		    (const uint16_t *)p, len / 2);
		p += len;
	}
	
	/*
	 * PDR: id[4], address[4], length[4];
	 */
	for (i = 0; i < pdrlen; ) {
		id   = GETLE32(p);	p += 4; i += 4;
		addr = GETLE32(p);	p += 4; i += 4;
		len  = GETLE32(p);	p += 4; i += 4;
		/* replace PDR entry with the values from EEPROM, if any */
		for (q = ebuf, eq = q + ebuflen; q < eq; q += elen * 2) {
			elen = GETLE16(q);	q += 2;
			eid  = GETLE16(q);	q += 2;
			elen--;		/* elen includes eid */
			if (eid == 0)
				break;
			if (eid != id)
				continue;
			CSR_WRITE_2(sc, WI_AUX_PAGE, addr / WI_AUX_PGSZ);
			CSR_WRITE_2(sc, WI_AUX_OFFSET, addr % WI_AUX_PGSZ);
			CSR_WRITE_MULTI_STREAM_2(sc, WI_AUX_DATA,
			    (const uint16_t *)q, len / 2);
			break;
		}
	}
	return 0;
}

static int
wi_pcmcia_set_hcr(struct wi_softc *sc, int mode)
{
	uint16_t hcr;

	CSR_WRITE_2(sc, WI_COR, WI_COR_RESET);
	tsleep(sc, PWAIT, "wiinit", 1);
	hcr = CSR_READ_2(sc, WI_HCR);
	hcr = (hcr & WI_HCR_4WIRE) | (mode & ~WI_HCR_4WIRE);
	CSR_WRITE_2(sc, WI_HCR, hcr);
	tsleep(sc, PWAIT, "wiinit", 1);
	CSR_WRITE_2(sc, WI_COR, WI_COR_IOMODE);
	tsleep(sc, PWAIT, "wiinit", 1);
	return 0;
}
#endif
