/*-
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/pccard/pccardvar.h>
#include <dev/pccard/pccard_cis.h>

#include <dev/wi/if_wavelan_ieee.h>
#include <dev/wi/if_wireg.h>
#include <dev/wi/if_wivar.h>
#ifdef WI_SYMBOL_FIRMWARE
#include <dev/wi/spectrum24t_cf.h>
#endif

#include "card_if.h"
#include "pccarddevs.h"

static int wi_pccard_probe(device_t);
static int wi_pccard_attach(device_t);

static int wi_pccard_match(device_t);

static device_method_t wi_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pccard_compat_probe),
	DEVMETHOD(device_attach,	pccard_compat_attach),
	DEVMETHOD(device_detach,	wi_detach),
	DEVMETHOD(device_shutdown,	wi_shutdown),

	/* Card interface */
	DEVMETHOD(card_compat_match,	wi_pccard_match),
	DEVMETHOD(card_compat_probe,	wi_pccard_probe),
	DEVMETHOD(card_compat_attach,	wi_pccard_attach),

	{ 0, 0 }
};

static driver_t wi_pccard_driver = {
	"wi",
	wi_pccard_methods,
	sizeof(struct wi_softc)
};

DRIVER_MODULE(wi, pccard, wi_pccard_driver, wi_devclass, 0, 0);
MODULE_DEPEND(wi, wlan, 1, 1, 1);

static const struct pccard_product wi_pccard_products[] = {
	PCMCIA_CARD(3COM, 3CRWE737A, 0),
	PCMCIA_CARD(3COM, 3CRWE777A, 0),
	PCMCIA_CARD(ACTIONTEC, PRISM, 0),
	PCMCIA_CARD(ADDTRON, AWP100, 0),
	PCMCIA_CARD(AIRVAST, WN_100, 0),
	PCMCIA_CARD(ALLIEDTELESIS, WR211PCM, 0),
	PCMCIA_CARD(ARTEM, ONAIR, 0),
 	PCMCIA_CARD(ASUS, WL100, 0),
	PCMCIA_CARD(BAY, EMOBILITY_11B, 0),
	PCMCIA_CARD(BROMAX, IWN, 0),
	PCMCIA_CARD(BROMAX, IWN3, 0),
	PCMCIA_CARD(BROMAX, WCF11, 0),
	PCMCIA_CARD(BUFFALO, WLI_CF_S11G, 0),
	PCMCIA_CARD(BUFFALO, WLI_PCM_S11, 0),
	PCMCIA_CARD(COMPAQ, NC5004, 0),
	PCMCIA_CARD(CONTEC, FX_DS110_PCC, 0),
	PCMCIA_CARD(COREGA, WIRELESS_LAN_PCC_11, 0),
	PCMCIA_CARD(COREGA, WIRELESS_LAN_PCCA_11, 0),
	PCMCIA_CARD(COREGA, WIRELESS_LAN_PCCB_11, 0),
	PCMCIA_CARD(COREGA, WIRELESS_LAN_PCCL_11, 0),
	PCMCIA_CARD(DLINK, DWL650H, 0),
	PCMCIA_CARD(ELSA, XI300_IEEE, 0),
	PCMCIA_CARD(ELSA, XI325_IEEE, 0),
	PCMCIA_CARD(ELSA, XI800_IEEE, 0),
	PCMCIA_CARD(EMTAC, WLAN, 0),
	PCMCIA_CARD(ERICSSON, WIRELESSLAN, 0),
	PCMCIA_CARD(GEMTEK, WLAN, 0),
	PCMCIA_CARD(HWN, AIRWAY80211, 0), 
	PCMCIA_CARD(INTEL, PRO_WLAN_2011, 0),
	PCMCIA_CARD(INTERSIL, ISL37100P, 0),
	PCMCIA_CARD(INTERSIL, ISL37110P, 0),
	PCMCIA_CARD(INTERSIL, ISL37300P, 0),
	PCMCIA_CARD(INTERSIL2, PRISM2, 0),
	PCMCIA_CARD(IODATA2, WCF12, 0),
	PCMCIA_CARD(IODATA2, WNB11PCM, 0),
	PCMCIA_CARD(FUJITSU, WL110, 0),
	PCMCIA_CARD(LUCENT, WAVELAN_IEEE, 0),
	PCMCIA_CARD(MICROSOFT, MN_520, 0),
	PCMCIA_CARD(NOKIA, C020_WLAN, 0),
	PCMCIA_CARD(NOKIA, C110_WLAN, 0),
	PCMCIA_CARD(PLANEX, GWNS11H, 0),
	PCMCIA_CARD(PROXIM, HARMONY, 0),
	PCMCIA_CARD(PROXIM, RANGELANDS_8430, 0),
	PCMCIA_CARD(SAMSUNG, SWL_2000N, 0),
	PCMCIA_CARD(SIEMENS, SS1021, 0),
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
	u_int32_t fcn = PCCARD_FUNCTION_UNSPEC;
	int error;

	/* Make sure we're a network driver */
	error = pccard_get_function(dev, &fcn);
	if (error != 0)
		return (error);
	if (fcn != PCCARD_FUNCTION_NETWORK)
		return (ENXIO);

	if ((pp = pccard_product_lookup(dev, wi_pccard_products,
	    sizeof(wi_pccard_products[0]), NULL)) != NULL) {
		if (pp->pp_name != NULL)
			device_set_desc(dev, pp->pp_name);
		return (0);
	}
	return (ENXIO);
}

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

	/* Make sure interrupts are disabled. */
	CSR_WRITE_2(sc, WI_INT_EN, 0);
	CSR_WRITE_2(sc, WI_EVENT_ACK, 0xFFFF);

	wi_free(dev);

	return (0);
}

static int
wi_pccard_attach(device_t dev)
{
	int			error;
	uint32_t		vendor;
	uint32_t		product;
	int			retval;

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
		struct wi_softc *sc;

		sc = device_get_softc(dev);
		if (wi_symbol_load_firm(sc,
		    spectrum24t_primsym, sizeof(spectrum24t_primsym),
		    spectrum24t_secsym, sizeof(spectrum24t_secsym))) {
			device_printf(dev, "couldn't load firmware\n");
		}
#else
		device_printf(dev, 
		    "Symbol LA4100 needs 'option WI_SYMBOL_FIRMWARE'\n");
		wi_free(dev);
		return (ENXIO);
#endif
	}
	retval = wi_attach(dev);
	if (retval != 0)
		wi_free(dev);
	return (retval);
}
