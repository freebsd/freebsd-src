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
	unsigned average_tx_time;
	int successive_failures;
	int tries;
	int total_packets;
	int packets_acked;
	unsigned perfect_tx_time; /* transmit time for 0 retries */
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
	int last_sample_ndx[NUM_PACKET_SIZE_BINS];

	int current_sample_ndx[NUM_PACKET_SIZE_BINS];       
	int packets_sent[NUM_PACKET_SIZE_BINS];

	int current_rate[NUM_PACKET_SIZE_BINS];
	int packets_since_switch[NUM_PACKET_SIZE_BINS];
	unsigned jiffies_since_switch[NUM_PACKET_SIZE_BINS];

	int packets_since_sample[NUM_PACKET_SIZE_BINS];
	unsigned sample_tt[NUM_PACKET_SIZE_BINS];
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

struct ar5212_desc {
	/*
	 * tx_control_0
	 */
	u_int32_t	frame_len:12;
	u_int32_t	reserved_12_15:4;
	u_int32_t	xmit_power:6;
	u_int32_t	rts_cts_enable:1;
	u_int32_t	veol:1;
	u_int32_t	clear_dest_mask:1;
	u_int32_t	ant_mode_xmit:4;
	u_int32_t	inter_req:1;
	u_int32_t	encrypt_key_valid:1;
	u_int32_t	cts_enable:1;

	/*
	 * tx_control_1
	 */
	u_int32_t	buf_len:12;
	u_int32_t	more:1;
	u_int32_t	encrypt_key_index:7;
	u_int32_t	frame_type:4;
	u_int32_t	no_ack:1;
	u_int32_t	comp_proc:2;
	u_int32_t	comp_iv_len:2;
	u_int32_t	comp_icv_len:2;
	u_int32_t	reserved_31:1;

	/*
	 * tx_control_2
	 */
	u_int32_t	rts_duration:15;
	u_int32_t	duration_update_enable:1;
	u_int32_t	xmit_tries0:4;
	u_int32_t	xmit_tries1:4;
	u_int32_t	xmit_tries2:4;
	u_int32_t	xmit_tries3:4;

	/*
	 * tx_control_3
	 */
	u_int32_t	xmit_rate0:5;
	u_int32_t	xmit_rate1:5;
	u_int32_t	xmit_rate2:5;
	u_int32_t	xmit_rate3:5;
	u_int32_t	rts_cts_rate:5;
	u_int32_t	reserved_25_31:7;

	/*
	 * tx_status_0
	 */
	u_int32_t	frame_xmit_ok:1;
	u_int32_t	excessive_retries:1;
	u_int32_t	fifo_underrun:1;
	u_int32_t	filtered:1;
	u_int32_t	rts_fail_count:4;
	u_int32_t	data_fail_count:4;
	u_int32_t	virt_coll_count:4;
	u_int32_t	send_timestamp:16;

	/*
	 * tx_status_1
	 */
	u_int32_t	done:1;
	u_int32_t	seq_num:12;
	u_int32_t	ack_sig_strength:8;
	u_int32_t	final_ts_index:2;
	u_int32_t	comp_success:1;
	u_int32_t	xmit_antenna:1;
	u_int32_t	reserved_25_31_x:7;
} __packed;

/*
 * Calculate the transmit duration of a frame.
 */
static unsigned calc_usecs_unicast_packet(struct ath_softc *sc,
				int length, 
				int rix, int short_retries, int long_retries) {
	const HAL_RATE_TABLE *rt = sc->sc_currates;

	
	/* pg 205 ieee.802.11.pdf */
	unsigned t_slot = 20;
	unsigned t_difs = 50; 
	unsigned t_sifs = 10; 
	struct ieee80211com *ic = &sc->sc_ic;
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

	int rts = 0;
	int cts = 0;

	if ((ic->ic_flags & IEEE80211_F_USEPROT) &&
	    rt->info[rix].phy == IEEE80211_T_OFDM) {
		if (ic->ic_protmode == IEEE80211_PROT_RTSCTS)
			rts = 1;
		else if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
			cts = 1;

		cix = rt->info[sc->sc_protrix].controlRate;

	}

	if (length > ic->ic_rtsthreshold) {
		rts = 1;
	}

	if (rts || cts) {
		int ctsrate = rt->info[cix].rateCode;
		int ctsduration = 0;
		ctsrate |= rt->info[cix].shortPreamble;
		if (rts)		/* SIFS + CTS */
			ctsduration += rt->info[cix].spAckDuration;

		ctsduration += ath_hal_computetxtime(sc->sc_ah,
						     rt, length, rix, AH_TRUE);

		if (cts)	/* SIFS + ACK */
			ctsduration += rt->info[cix].spAckDuration;

		tt += (short_retries + 1) * ctsduration;
	}
	tt += t_difs;
	tt += (long_retries+1)*(t_sifs + rt->info[cix].spAckDuration);
	tt += (long_retries+1)*ath_hal_computetxtime(sc->sc_ah, rt, length, 
						rix, AH_TRUE);
	for (x = 0; x <= short_retries + long_retries; x++) {
		cw = MIN(WIFI_CW_MAX, (cw + 1) * 2);
		tt += (t_slot * cw/2);
	}
	return tt;
}
#endif /* _DEV_ATH_RATE_SAMPLE_H */
