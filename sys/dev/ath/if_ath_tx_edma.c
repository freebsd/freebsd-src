/*-
 * Copyright (c) 2012 Adrian Chadd <adrian@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Driver for the Atheros Wireless LAN controller.
 *
 * This software is derived from work of Atsushi Onoe; his contribution
 * is greatly appreciated.
 */

#include "opt_inet.h"
#include "opt_ath.h"
/*
 * This is needed for register operations which are performed
 * by the driver - eg, calls to ath_hal_gettsf32().
 *
 * It's also required for any AH_DEBUG checks in here, eg the
 * module dependencies.
 */
#include "opt_ah.h"
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/errno.h>
#include <sys/callout.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kthread.h>
#include <sys/taskqueue.h>
#include <sys/priv.h>
#include <sys/module.h>
#include <sys/ktr.h>
#include <sys/smp.h>	/* for mp_ncpus */

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_llc.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_regdomain.h>
#ifdef IEEE80211_SUPPORT_SUPERG
#include <net80211/ieee80211_superg.h>
#endif
#ifdef IEEE80211_SUPPORT_TDMA
#include <net80211/ieee80211_tdma.h>
#endif

#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <dev/ath/if_athvar.h>
#include <dev/ath/ath_hal/ah_devid.h>		/* XXX for softled */
#include <dev/ath/ath_hal/ah_diagcodes.h>

#include <dev/ath/if_ath_debug.h>
#include <dev/ath/if_ath_misc.h>
#include <dev/ath/if_ath_tsf.h>
#include <dev/ath/if_ath_tx.h>
#include <dev/ath/if_ath_sysctl.h>
#include <dev/ath/if_ath_led.h>
#include <dev/ath/if_ath_keycache.h>
#include <dev/ath/if_ath_rx.h>
#include <dev/ath/if_ath_beacon.h>
#include <dev/ath/if_athdfs.h>

#ifdef ATH_TX99_DIAG
#include <dev/ath/ath_tx99/ath_tx99.h>
#endif

#include <dev/ath/if_ath_tx_edma.h>

/*
 * some general macros
 */
#define	INCR(_l, _sz)		(_l) ++; (_l) &= ((_sz) - 1)
#define	DECR(_l, _sz)		(_l) --; (_l) &= ((_sz) - 1)

/*
 * XXX doesn't belong here, and should be tunable
 */
#define	ATH_TXSTATUS_RING_SIZE	512

MALLOC_DECLARE(M_ATHDEV);

static int
ath_edma_dma_txsetup(struct ath_softc *sc)
{
	int error;

	error = ath_descdma_alloc_desc(sc, &sc->sc_txsdma,
	    NULL, "txcomp", sc->sc_tx_statuslen, ATH_TXSTATUS_RING_SIZE);
	if (error != 0)
		return (error);

	ath_hal_setuptxstatusring(sc->sc_ah,
	    (void *) sc->sc_txsdma.dd_desc,
	    sc->sc_txsdma.dd_desc_paddr,
	    ATH_TXSTATUS_RING_SIZE);


	return (0);
}

static int
ath_edma_dma_txteardown(struct ath_softc *sc)
{

	ath_descdma_cleanup(sc, &sc->sc_txsdma, NULL);
	return (0);
}

void
ath_xmit_setup_edma(struct ath_softc *sc)
{

	/* Fetch EDMA field and buffer sizes */
	(void) ath_hal_gettxdesclen(sc->sc_ah, &sc->sc_tx_desclen);
	(void) ath_hal_gettxstatuslen(sc->sc_ah, &sc->sc_tx_statuslen);
	(void) ath_hal_getntxmaps(sc->sc_ah, &sc->sc_tx_nmaps);

	device_printf(sc->sc_dev, "TX descriptor length: %d\n",
	    sc->sc_tx_desclen);
	device_printf(sc->sc_dev, "TX status length: %d\n",
	    sc->sc_tx_statuslen);
	device_printf(sc->sc_dev, "TX buffers per descriptor: %d\n",
	    sc->sc_tx_nmaps);

	sc->sc_tx.xmit_setup = ath_edma_dma_txsetup;
	sc->sc_tx.xmit_teardown = ath_edma_dma_txteardown;
}
