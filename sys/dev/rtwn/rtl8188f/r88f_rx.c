/*
 * Copyright (c) 2026 Mir Saheb Ali <mirsahebali@pm.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/cdefs.h>
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/linker.h>

#include <net/if.h>
#include <net/ethernet.h>
#include <net/if_media.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/rtwn/if_rtwnreg.h>
#include <dev/rtwn/if_rtwnvar.h>
#include <dev/rtwn/if_rtwn_ridx.h>

#include <dev/rtwn/rtl8188e/r88e_rx_desc.h>

#include <dev/rtwn/rtl8192c/r92c.h>
#include <dev/rtwn/rtl8192c/r92c_rx_desc.h>
#include <dev/rtwn/rtl8192e/r92e_reg.h>

#include <dev/rtwn/rtl8188f/r88f.h>

int8_t
r88f_get_rssi_cck(struct rtwn_softc *sc, void *physt)
{
	struct r88e_rx_phystat *phy = (struct r88e_rx_phystat *)physt;
	int8_t lna_idx, vga_idx, rssi;

	lna_idx = (phy->agc_rpt & 0xe0) >> 5;
	vga_idx = (phy->agc_rpt & 0x1f);
	rssi = -(2 * vga_idx);

	switch (lna_idx) {
	case 7:
		if (vga_idx > 27)
			rssi = -100;
		else
			rssi += -46;
		break;
	case 5:
		rssi += -32;
		break;
	case 3:
		rssi += -20;
		break;
	case 1:
		rssi += -6;
		break;
	default:
		rssi = 0;
		break;
	}

	return (rssi);
}

int8_t
r88f_get_rssi_ofdm(struct rtwn_softc *sc, void *physt)
{
	struct r88e_rx_phystat *phy = (struct r88e_rx_phystat *)physt;
	int rssi;

	/* Get average RSSI. */
	rssi = ((le32toh(phy->sig_qual) >> 1) & 0x7f) - 110;

	return (rssi);
}

void
r88f_get_rx_stats(struct rtwn_softc *sc, struct ieee80211_rx_stats *rxs,
    const void *desc, const void *physt_ptr)
{
	const struct r92c_rx_stat *stat = desc;
	uint32_t rxdw1, rxdw3;
	uint8_t rate;

	rxdw1 = le32toh(stat->rxdw1);
	rxdw3 = le32toh(stat->rxdw3);
	rate = MS(rxdw3, R92E_RXDW3_RATE);

	if (rxdw1 & R92C_RXDW1_AMPDU)
		rxs->c_pktflags |= IEEE80211_RX_F_AMPDU;
	else if (rxdw1 & R92C_RXDW1_AMPDU_MORE)
		rxs->c_pktflags |= IEEE80211_RX_F_AMPDU_MORE;
	if ((rxdw3 & R92C_RXDW3_SPLCP) && RTWN_RATE_IS_HT(rate))
		rxs->c_pktflags |= IEEE80211_RX_F_SHORTGI;

	if (rxdw3 & R92C_RXDW3_HT40)
		rxs->c_width = IEEE80211_RX_FW_40MHZ;
	else
		rxs->c_width = IEEE80211_RX_FW_20MHZ;

	if (RTWN_RATE_IS_CCK(rate))
		rxs->c_phytype = IEEE80211_RX_FP_11B;
	else if (RTWN_RATE_IS_OFDM(rate))
		rxs->c_phytype = IEEE80211_RX_FP_11G;
	else
		rxs->c_phytype = IEEE80211_RX_FP_11NG;

	/* Map HW rate index to 802.11 rate. */
	if (RTWN_RATE_IS_CCK(rate) || RTWN_RATE_IS_OFDM(rate)) {
		rxs->c_rate = ridx2rate[rate];
		if (RTWN_RATE_IS_CCK(rate))
			rxs->c_pktflags |= IEEE80211_RX_F_CCK;
		else
			rxs->c_pktflags |= IEEE80211_RX_F_OFDM;
	} else {	/* MCS0~15. */
		rxs->c_rate =
		    IEEE80211_RATE_MCS | (RTWN_RIDX_TO_MCS(rate));
		rxs->c_pktflags |= IEEE80211_RX_F_HT;
	}
}
