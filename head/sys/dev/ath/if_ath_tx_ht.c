/*-
 * Copyright (c) 2011 Adrian Chadd, Xenion Pty Ltd.
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

#include "opt_inet.h"
#include "opt_ath.h"
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

#ifdef ATH_TX99_DIAG
#include <dev/ath/ath_tx99/ath_tx99.h>
#endif

#include <dev/ath/if_ath_tx_ht.h>

/*
 * Setup a 11n rate series structure
 *
 * This should be called for both legacy and MCS rates.
 */
static void
ath_rateseries_setup(struct ath_softc *sc, struct ieee80211_node *ni,
    HAL_11N_RATE_SERIES *series, unsigned int pktlen, uint8_t *rix,
    uint8_t *try, int flags)
{
#define	HT_RC_2_STREAMS(_rc)	((((_rc) & 0x78) >> 3) + 1)
	struct ieee80211com *ic = ni->ni_ic;
	struct ath_hal *ah = sc->sc_ah;
	HAL_BOOL shortPreamble = AH_FALSE;
	const HAL_RATE_TABLE *rt = sc->sc_currates;
	int i;

	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
	    (ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE))
		shortPreamble = AH_TRUE;

	memset(series, 0, sizeof(HAL_11N_RATE_SERIES) * 4);
	for (i = 0; i < 4;  i++) {
		/* Only set flags for actual TX attempts */
		if (try[i] == 0)
			continue;

		series[i].Tries = try[i];

		/*
		 * XXX this isn't strictly correct - sc_txchainmask
		 * XXX isn't the currently active chainmask;
		 * XXX it's the interface chainmask at startup.
		 * XXX It's overridden in the HAL rate scenario function
		 * XXX for now.
		 */
		series[i].ChSel = sc->sc_txchainmask;

		if (flags & (HAL_TXDESC_RTSENA | HAL_TXDESC_CTSENA))
			series[i].RateFlags |= HAL_RATESERIES_RTS_CTS;

		/*
		 * Transmit 40MHz frames only if the node has negotiated
		 * it rather than whether the node is capable of it or not.
	 	 * It's subtly different in the hostap case.
	 	 */
		if (ni->ni_chw == 40)
			series[i].RateFlags |= HAL_RATESERIES_2040;

		/*
		 * Set short-GI only if the node has advertised it
		 * the channel width is suitable, and we support it.
		 * We don't currently have a "negotiated" set of bits -
		 * ni_htcap is what the remote end sends, not what this
		 * node is capable of.
		 */
		if (ni->ni_chw == 40 &&
		    ic->ic_htcaps & IEEE80211_HTCAP_SHORTGI40 &&
		    ni->ni_htcap & IEEE80211_HTCAP_SHORTGI40)
			series[i].RateFlags |= HAL_RATESERIES_HALFGI;

		if (ni->ni_chw == 20 &&
		    ic->ic_htcaps & IEEE80211_HTCAP_SHORTGI20 &&
		    ni->ni_htcap & IEEE80211_HTCAP_SHORTGI20)
			series[i].RateFlags |= HAL_RATESERIES_HALFGI;

		series[i].Rate = rt->info[rix[i]].rateCode;

		/* PktDuration doesn't include slot, ACK, RTS, etc timing - it's just the packet duration */
		if (series[i].Rate & IEEE80211_RATE_MCS) {
			series[i].PktDuration =
			    ath_computedur_ht(pktlen
				, series[i].Rate
				, HT_RC_2_STREAMS(series[i].Rate)
				, series[i].RateFlags & HAL_RATESERIES_2040
				, series[i].RateFlags & HAL_RATESERIES_HALFGI);
		} else {
			if (shortPreamble)
				series[i].Rate |= rt->info[rix[i]].shortPreamble;
			series[i].PktDuration = ath_hal_computetxtime(ah,
			    rt, pktlen, rix[i], shortPreamble);
		}
	}
#undef	HT_RC_2_STREAMS
}

#if 0
static void
ath_rateseries_print(HAL_11N_RATE_SERIES *series)
{
	int i;
	for (i = 0; i < 4; i++) {
		printf("series %d: rate %x; tries %d; pktDuration %d; chSel %d; rateFlags %x\n",
		    i,
		    series[i].Rate,
		    series[i].Tries,
		    series[i].PktDuration,
		    series[i].ChSel,
		    series[i].RateFlags);
	}
}
#endif

/*
 * Setup the 11n rate scenario and burst duration for the given TX descriptor
 * list.
 *
 * This isn't useful for sending beacon frames, which has different needs
 * wrt what's passed into the rate scenario function.
 */

void
ath_buf_set_rate(struct ath_softc *sc, struct ieee80211_node *ni, struct ath_buf *bf,
    int pktlen, int flags, uint8_t ctsrate, int is_pspoll, uint8_t *rix, uint8_t *try)
{
	HAL_11N_RATE_SERIES series[4];
	struct ath_desc *ds = bf->bf_desc;
	struct ath_desc *lastds = NULL;
	struct ath_hal *ah = sc->sc_ah;

	/* Setup rate scenario */
	memset(&series, 0, sizeof(series));

	ath_rateseries_setup(sc, ni, series, pktlen, rix, try, flags);

	/* Enforce AR5416 aggregate limit - can't do RTS w/ an agg frame > 8k */

	/* Enforce RTS and CTS are mutually exclusive */

	/* Get a pointer to the last tx descriptor in the list */
	lastds = &bf->bf_desc[bf->bf_nseg - 1];

#if 0
	printf("pktlen: %d; flags 0x%x\n", pktlen, flags);
	ath_rateseries_print(series);
#endif

	/* Set rate scenario */
	ath_hal_set11nratescenario(ah, ds,
	    !is_pspoll,	/* whether to override the duration or not */
			/* don't allow hardware to override the duration on ps-poll packets */
	    ctsrate,	/* rts/cts rate */
	    series,	/* 11n rate series */
	    4,		/* number of series */
	    flags);

	/* Setup the last descriptor in the chain */
	ath_hal_setuplasttxdesc(ah, lastds, ds);

	/* Set burst duration */
	/* This should only be done if aggregate protection is enabled */
	//ath_hal_set11nburstduration(ah, ds, 8192);
}
