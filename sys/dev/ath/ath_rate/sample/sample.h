/*-
 * Copyright (c) 2005 John Bicket
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
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
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
 *
 * $FreeBSD$
 */

/*
 * Defintions for the Atheros Wireless LAN controller driver.
 */
#ifndef _DEV_ATH_RATE_SAMPLE_H
#define _DEV_ATH_RATE_SAMPLE_H

/* per-device state */
struct sample_softc {
	struct ath_ratectrl arc;	/* base state */
	int	ath_smoothing_rate;	/* ewma percentage (out of 100) */
	int	ath_sample_rate;	/* send a different bit-rate 1/X packets */
};
#define	ATH_SOFTC_SAMPLE(sc)	((struct sample_softc *)sc->sc_rc)

struct rate_info {
	int rate;
	int rix;
	int rateCode;
	int shortPreambleRateCode;
};


struct rate_stats {	
	int average_tx_time;
	int successive_failures;
	int tries;
	int total_packets;
	int packets_acked;
	int perfect_tx_time; /* transmit time for 0 retries */
	int last_tx;
};

/*
 * for now, we track performance for three different packet
 * size buckets
 */
#define NUM_PACKET_SIZE_BINS 3
static int packet_size_bins[NUM_PACKET_SIZE_BINS] = {250, 1600, 3000};

/* per-node state */
struct sample_node {
	int static_rate_ndx;
	int num_rates;

	struct rate_info rates[IEEE80211_RATE_MAXSIZE];
	
	struct rate_stats stats[NUM_PACKET_SIZE_BINS][IEEE80211_RATE_MAXSIZE];
	int sample_num[NUM_PACKET_SIZE_BINS];       
	int packets_sent[NUM_PACKET_SIZE_BINS];

};
#define	ATH_NODE_SAMPLE(an)	((struct sample_node *)&an[1])

#ifndef MIN
#define	MIN(a,b)	((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define	MAX(a,b)	((a) > (b) ? (a) : (b))
#endif

#define WIFI_CW_MIN 31
#define WIFI_CW_MAX 1023

/*
 * Calculate the transmit duration of a frame.
 */
static unsigned calc_usecs_unicast_packet(struct ath_softc *sc,
				int length, 
				int rix, int retries) {
	const HAL_RATE_TABLE *rt = sc->sc_currates;

	
	/* pg 205 ieee.802.11.pdf */
	unsigned t_slot = 20;
	unsigned t_difs = 50; 
	unsigned t_sifs = 10; 
	int tt = 0;
	int x = 0;
	int cw = WIFI_CW_MIN;
	int cix = rt->info[rix].controlRate;
	KASSERT(rt != NULL, ("no rate table, mode %u", sc->sc_curmode));
	
	if (rt->info[rix].phy == IEEE80211_T_OFDM) {
		t_slot = 9;
		t_sifs = 9;
		t_difs = 28;
	}
	tt += t_difs;
	tt += (retries+1)*(t_sifs + rt->info[cix].spAckDuration);
	tt += (retries+1)*ath_hal_computetxtime(sc->sc_ah, rt, length, 
						rix, AH_TRUE);
	for (x = 0; x <= retries; x++) {
		cw = MIN(WIFI_CW_MAX, (cw + 1) * 2);
		tt += (t_slot * cw/2);
	}
	return tt;
}
#endif /* _DEV_ATH_RATE_SAMPLE_H */
