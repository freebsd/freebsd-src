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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/ath/ath_rate/sample/sample.c,v 1.18.2.1 2007/10/16 19:07:26 sam Exp $");

/*
 * John Bicket's SampleRate control algorithm.
 */
#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/sysctl.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/errno.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>

#include <sys/socket.h>
 
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#include <net/ethernet.h>		/* XXX for ether_sprintf */

#include <net80211/ieee80211_var.h>

#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h> 
#include <netinet/if_ether.h>
#endif

#include <dev/ath/if_athvar.h>
#include <dev/ath/ath_rate/sample/sample.h>
#include <contrib/dev/ath/ah_desc.h>

#define	SAMPLE_DEBUG
#ifdef SAMPLE_DEBUG
enum {
	ATH_DEBUG_NODE		= 0x00080000,	/* node management */
	ATH_DEBUG_RATE		= 0x00000010,	/* rate control */
	ATH_DEBUG_ANY		= 0xffffffff
};
#define	DPRINTF(sc, m, fmt, ...) do {				\
	if (sc->sc_debug & (m))					\
		printf(fmt, __VA_ARGS__);			\
} while (0)
#else
#define	DPRINTF(sc, m, fmt, ...) do {				\
	(void) sc;						\
} while (0)
#endif

/*
 * This file is an implementation of the SampleRate algorithm
 * in "Bit-rate Selection in Wireless Networks"
 * (http://www.pdos.lcs.mit.edu/papers/jbicket-ms.ps)
 *
 * SampleRate chooses the bit-rate it predicts will provide the most
 * throughput based on estimates of the expected per-packet
 * transmission time for each bit-rate.  SampleRate periodically sends
 * packets at bit-rates other than the current one to estimate when
 * another bit-rate will provide better performance. SampleRate
 * switches to another bit-rate when its estimated per-packet
 * transmission time becomes smaller than the current bit-rate's.
 * SampleRate reduces the number of bit-rates it must sample by
 * eliminating those that could not perform better than the one
 * currently being used.  SampleRate also stops probing at a bit-rate
 * if it experiences several successive losses.
 *
 * The difference between the algorithm in the thesis and the one in this
 * file is that the one in this file uses a ewma instead of a window.
 *
 * Also, this implementation tracks the average transmission time for
 * a few different packet sizes independently for each link.
 */

#define STALE_FAILURE_TIMEOUT_MS 10000
#define MIN_SWITCH_MS 1000

static void	ath_rate_ctl_reset(struct ath_softc *, struct ieee80211_node *);

static __inline int
size_to_bin(int size) 
{
	int x = 0;
	for (x = 0; x < NUM_PACKET_SIZE_BINS; x++) {
		if (size <= packet_size_bins[x]) {
			return x;
		}
	}
	return NUM_PACKET_SIZE_BINS-1;
}
static __inline int
bin_to_size(int index) {
	return packet_size_bins[index];
}

static __inline int
rate_to_ndx(struct sample_node *sn, int rate) {
	int x = 0;
	for (x = 0; x < sn->num_rates; x++) {
		if (sn->rates[x].rate == rate) {
			return x;
		}      
	}
	return -1;
}

void
ath_rate_node_init(struct ath_softc *sc, struct ath_node *an)
{
	DPRINTF(sc, ATH_DEBUG_NODE, "%s:\n", __func__);
	/* NB: assumed to be zero'd by caller */
}

void
ath_rate_node_cleanup(struct ath_softc *sc, struct ath_node *an)
{
	DPRINTF(sc, ATH_DEBUG_NODE, "%s:\n", __func__);
}


/*
 * returns the ndx with the lowest average_tx_time,
 * or -1 if all the average_tx_times are 0.
 */
static __inline int best_rate_ndx(struct sample_node *sn, int size_bin, 
				  int require_acked_before)
{
	int x = 0;
        int best_rate_ndx = 0;
        int best_rate_tt = 0;
        for (x = 0; x < sn->num_rates; x++) {
		int tt = sn->stats[size_bin][x].average_tx_time;
		if (tt <= 0 || (require_acked_before && 
				!sn->stats[size_bin][x].packets_acked)) {
			continue;
		}

		/* 9 megabits never works better than 12 */
		if (sn->rates[x].rate == 18) 
			continue;

		/* don't use a bit-rate that has been failing */
		if (sn->stats[size_bin][x].successive_failures > 3)
			continue;

		if (!best_rate_tt || best_rate_tt > tt) {
			best_rate_tt = tt;
			best_rate_ndx = x;
		}
        }
        return (best_rate_tt) ? best_rate_ndx : -1;
}

/*
 * pick a good "random" bit-rate to sample other than the current one
 */
static __inline int
pick_sample_ndx(struct sample_node *sn, int size_bin) 
{
	int x = 0;
	int current_ndx = 0;
	unsigned current_tt = 0;
	
	current_ndx = sn->current_rate[size_bin];
	if (current_ndx < 0) {
		/* no successes yet, send at the lowest bit-rate */
		return 0;
	}
	
	current_tt = sn->stats[size_bin][current_ndx].average_tx_time;
	
	for (x = 0; x < sn->num_rates; x++) {
		int ndx = (sn->last_sample_ndx[size_bin]+1+x) % sn->num_rates;

	        /* don't sample the current bit-rate */
		if (ndx == current_ndx) 
			continue;

		/* this bit-rate is always worse than the current one */
		if (sn->stats[size_bin][ndx].perfect_tx_time > current_tt) 
			continue;

		/* rarely sample bit-rates that fail a lot */
		if (ticks - sn->stats[size_bin][ndx].last_tx < ((hz * STALE_FAILURE_TIMEOUT_MS)/1000) &&
		    sn->stats[size_bin][ndx].successive_failures > 3)
			continue;

		/* don't sample more than 2 indexes higher 
		 * for rates higher than 11 megabits
		 */
		if (sn->rates[ndx].rate > 22 && ndx > current_ndx + 2)
			continue;

		/* 9 megabits never works better than 12 */
		if (sn->rates[ndx].rate == 18) 
			continue;

		/* if we're using 11 megabits, only sample up to 12 megabits
		 */
		if (sn->rates[current_ndx].rate == 22 && ndx > current_ndx + 1) 
			continue;

		sn->last_sample_ndx[size_bin] = ndx;
		return ndx;
	}
	return current_ndx;
}

void
ath_rate_findrate(struct ath_softc *sc, struct ath_node *an,
		  int shortPreamble, size_t frameLen,
		  u_int8_t *rix, int *try0, u_int8_t *txrate)
{
	struct sample_node *sn = ATH_NODE_SAMPLE(an);
	struct sample_softc *ssc = ATH_SOFTC_SAMPLE(sc);
	struct ieee80211com *ic = &sc->sc_ic;
	int ndx, size_bin, mrr, best_ndx, change_rates;
	unsigned average_tx_time;

	mrr = sc->sc_mrretry && !(ic->ic_flags & IEEE80211_F_USEPROT);
	size_bin = size_to_bin(frameLen);
	best_ndx = best_rate_ndx(sn, size_bin, !mrr);

	if (best_ndx >= 0) {
		average_tx_time = sn->stats[size_bin][best_ndx].average_tx_time;
	} else {
		average_tx_time = 0;
	}
	
	if (sn->static_rate_ndx != -1) {
		ndx = sn->static_rate_ndx;
		*try0 = ATH_TXMAXTRY;
	} else {
		*try0 = mrr ? 2 : ATH_TXMAXTRY;
		
		if (sn->sample_tt[size_bin] < average_tx_time * (sn->packets_since_sample[size_bin]*ssc->ath_sample_rate/100)) {
			/*
			 * we want to limit the time measuring the performance
			 * of other bit-rates to ath_sample_rate% of the
			 * total transmission time.
			 */
			ndx = pick_sample_ndx(sn, size_bin);
			if (ndx != sn->current_rate[size_bin]) {
				sn->current_sample_ndx[size_bin] = ndx;
			} else {
				sn->current_sample_ndx[size_bin] = -1;
			}
			sn->packets_since_sample[size_bin] = 0;

		} else {
			change_rates = 0;
			if (!sn->packets_sent[size_bin] || best_ndx == -1) {
				/* no packet has been sent successfully yet */
				for (ndx = sn->num_rates-1; ndx > 0; ndx--) {
					/* 
					 * pick the highest rate <= 36 Mbps
					 * that hasn't failed.
					 */
					if (sn->rates[ndx].rate <= 72 && 
					    sn->stats[size_bin][ndx].successive_failures == 0) {
						break;
					}
				}
				change_rates = 1;
				best_ndx = ndx;
			} else if (sn->packets_sent[size_bin] < 20) {
				/* let the bit-rate switch quickly during the first few packets */
				change_rates = 1;
			} else if (ticks - ((hz*MIN_SWITCH_MS)/1000) > sn->ticks_since_switch[size_bin]) {
				/* 2 seconds have gone by */
				change_rates = 1;
			} else if (average_tx_time * 2 < sn->stats[size_bin][sn->current_rate[size_bin]].average_tx_time) {
				/* the current bit-rate is twice as slow as the best one */
				change_rates = 1;
			}

			sn->packets_since_sample[size_bin]++;
			
			if (change_rates) {
				if (best_ndx != sn->current_rate[size_bin]) {
					DPRINTF(sc, ATH_DEBUG_RATE,
"%s: %s size %d switch rate %d (%d/%d) -> %d (%d/%d) after %d packets mrr %d\n",
					    __func__,
					    ether_sprintf(an->an_node.ni_macaddr),
					    packet_size_bins[size_bin],
					    sn->rates[sn->current_rate[size_bin]].rate,
					    sn->stats[size_bin][sn->current_rate[size_bin]].average_tx_time,
					    sn->stats[size_bin][sn->current_rate[size_bin]].perfect_tx_time,
					    sn->rates[best_ndx].rate,
					    sn->stats[size_bin][best_ndx].average_tx_time,
					    sn->stats[size_bin][best_ndx].perfect_tx_time,
					    sn->packets_since_switch[size_bin],
					    mrr);
				}
				sn->packets_since_switch[size_bin] = 0;
				sn->current_rate[size_bin] = best_ndx;
				sn->ticks_since_switch[size_bin] = ticks;
			}
			ndx = sn->current_rate[size_bin];
			sn->packets_since_switch[size_bin]++;
			if (size_bin == 0) {
	    			/* 
	    			 * set the visible txrate for this node
			         * to the rate of small packets
			         */
				an->an_node.ni_txrate = ndx;
			}
		}
	}

	KASSERT(ndx >= 0 && ndx < sn->num_rates, ("ndx is %d", ndx));

	*rix = sn->rates[ndx].rix;
	if (shortPreamble) {
		*txrate = sn->rates[ndx].shortPreambleRateCode;
	} else {
		*txrate = sn->rates[ndx].rateCode;
	}
	sn->packets_sent[size_bin]++;
}

void
ath_rate_setupxtxdesc(struct ath_softc *sc, struct ath_node *an,
		      struct ath_desc *ds, int shortPreamble, u_int8_t rix)
{
	struct sample_node *sn = ATH_NODE_SAMPLE(an);
	int rateCode = -1;
	int frame_size = 0;
	int size_bin = 0;
	int ndx = 0;

	size_bin = size_to_bin(frame_size);	// TODO: it's correct that frame_size alway 0 ?
	ndx = sn->current_rate[size_bin]; /* retry at the current bit-rate */
	
	if (!sn->stats[size_bin][ndx].packets_acked) {
		ndx = 0;  /* use the lowest bit-rate */
	}

	if (shortPreamble) {
		rateCode = sn->rates[ndx].shortPreambleRateCode;
	} else {
		rateCode = sn->rates[ndx].rateCode;
	}
	ath_hal_setupxtxdesc(sc->sc_ah, ds
			     , rateCode, 3	        /* series 1 */
			     , sn->rates[0].rateCode, 3	/* series 2 */
			     , 0, 0	                /* series 3 */
			     );
}

static void
update_stats(struct ath_softc *sc, struct ath_node *an, 
		  int frame_size,
		  int ndx0, int tries0,
		  int ndx1, int tries1,
		  int ndx2, int tries2,
		  int ndx3, int tries3,
		  int short_tries, int tries, int status)
{
	struct sample_node *sn = ATH_NODE_SAMPLE(an);
	struct sample_softc *ssc = ATH_SOFTC_SAMPLE(sc);
	int tt = 0;
	int tries_so_far = 0;
	int size_bin = 0;
	int size = 0;
	int rate = 0;

	size_bin = size_to_bin(frame_size);
	size = bin_to_size(size_bin);

	if (!(0 <= ndx0 && ndx0 < sn->num_rates)) {
		printf("%s: bogus ndx0 %d, max %u, mode %u\n",
		    __func__, ndx0, sn->num_rates, sc->sc_curmode);
		return;
	}
	rate = sn->rates[ndx0].rate;

	tt += calc_usecs_unicast_packet(sc, size, sn->rates[ndx0].rix, 
					short_tries,
					MIN(tries0, tries) - 1);
	tries_so_far += tries0;
	if (tries1 && tries0 < tries) {
		if (!(0 <= ndx1 && ndx1 < sn->num_rates)) {
			printf("%s: bogus ndx1 %d, max %u, mode %u\n",
			    __func__, ndx1, sn->num_rates, sc->sc_curmode);
			return;
		}
		tt += calc_usecs_unicast_packet(sc, size, sn->rates[ndx1].rix, 
						short_tries,
						MIN(tries1 + tries_so_far, tries) - tries_so_far - 1);
	}
	tries_so_far += tries1;

	if (tries2 && tries0 + tries1 < tries) {
		if (!(0 <= ndx2 && ndx2 < sn->num_rates)) {
			printf("%s: bogus ndx2 %d, max %u, mode %u\n",
			    __func__, ndx2, sn->num_rates, sc->sc_curmode);
			return;
		}
		tt += calc_usecs_unicast_packet(sc, size, sn->rates[ndx2].rix, 
					       short_tries,
						MIN(tries2 + tries_so_far, tries) - tries_so_far - 1);
	}

	tries_so_far += tries2;

	if (tries3 && tries0 + tries1 + tries2 < tries) {
		if (!(0 <= ndx3 && ndx3 < sn->num_rates)) {
			printf("%s: bogus ndx3 %d, max %u, mode %u\n",
			    __func__, ndx3, sn->num_rates, sc->sc_curmode);
			return;
		}
		tt += calc_usecs_unicast_packet(sc, size, sn->rates[ndx3].rix, 
						short_tries,
						MIN(tries3 + tries_so_far, tries) - tries_so_far - 1);
	}
	if (sn->stats[size_bin][ndx0].total_packets < (100 / (100 - ssc->ath_smoothing_rate))) {
		/* just average the first few packets */
		int avg_tx = sn->stats[size_bin][ndx0].average_tx_time;
		int packets = sn->stats[size_bin][ndx0].total_packets;
		sn->stats[size_bin][ndx0].average_tx_time = (tt+(avg_tx*packets))/(packets+1);
	} else {
		/* use a ewma */
		sn->stats[size_bin][ndx0].average_tx_time = 
			((sn->stats[size_bin][ndx0].average_tx_time * ssc->ath_smoothing_rate) + 
			 (tt * (100 - ssc->ath_smoothing_rate))) / 100;
	}
	
	if (status) {
		int y;
		sn->stats[size_bin][ndx0].successive_failures++;
		for (y = size_bin+1; y < NUM_PACKET_SIZE_BINS; y++) {
			/* also say larger packets failed since we
			 * assume if a small packet fails at a lower
			 * bit-rate then a larger one will also.
			 */
			sn->stats[y][ndx0].successive_failures++;
			sn->stats[y][ndx0].last_tx = ticks;
			sn->stats[y][ndx0].tries += tries;
			sn->stats[y][ndx0].total_packets++;
		}
	} else {
		sn->stats[size_bin][ndx0].packets_acked++;
		sn->stats[size_bin][ndx0].successive_failures = 0;
	}
	sn->stats[size_bin][ndx0].tries += tries;
	sn->stats[size_bin][ndx0].last_tx = ticks;
	sn->stats[size_bin][ndx0].total_packets++;


	if (ndx0 == sn->current_sample_ndx[size_bin]) {
		DPRINTF(sc, ATH_DEBUG_RATE,
"%s: %s size %d %s sample rate %d tries (%d/%d) tt %d avg_tt (%d/%d)\n", 
		    __func__, ether_sprintf(an->an_node.ni_macaddr), 
		    size,
		    status ? "FAIL" : "OK",
		    rate, short_tries, tries, tt, 
		    sn->stats[size_bin][ndx0].average_tx_time,
		    sn->stats[size_bin][ndx0].perfect_tx_time);
		sn->sample_tt[size_bin] = tt;
		sn->current_sample_ndx[size_bin] = -1;
	}
}

void
ath_rate_tx_complete(struct ath_softc *sc, struct ath_node *an,
	const struct ath_buf *bf)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct sample_node *sn = ATH_NODE_SAMPLE(an);
	const struct ath_tx_status *ts = &bf->bf_status.ds_txstat;
	const struct ath_desc *ds0 = &bf->bf_desc[0];
	int final_rate, short_tries, long_tries, frame_size;
	int mrr;

	final_rate = sc->sc_hwmap[ts->ts_rate &~ HAL_TXSTAT_ALTRATE].ieeerate;
	short_tries = ts->ts_shortretry;
	long_tries = ts->ts_longretry + 1;
	frame_size = ds0->ds_ctl0 & 0x0fff; /* low-order 12 bits of ds_ctl0 */
	if (frame_size == 0)		    /* NB: should not happen */
		frame_size = 1500;

	if (sn->num_rates <= 0) {
		DPRINTF(sc, ATH_DEBUG_RATE,
		    "%s: %s size %d %s rate/try %d/%d no rates yet\n", 
		    __func__, ether_sprintf(an->an_node.ni_macaddr),
		    bin_to_size(size_to_bin(frame_size)),
		    ts->ts_status ? "FAIL" : "OK",
		    short_tries, long_tries);
		return;
	}
	mrr = sc->sc_mrretry && !(ic->ic_flags & IEEE80211_F_USEPROT);
	if (!mrr || !(ts->ts_rate & HAL_TXSTAT_ALTRATE)) {
		int ndx = rate_to_ndx(sn, final_rate);

		/*
		 * Only one rate was used; optimize work.
		 */
		DPRINTF(sc, ATH_DEBUG_RATE,
		    "%s: %s size %d %s rate/try %d/%d/%d\n", 
		     __func__, ether_sprintf(an->an_node.ni_macaddr),
		     bin_to_size(size_to_bin(frame_size)),
		     ts->ts_status ? "FAIL" : "OK",
		     final_rate, short_tries, long_tries);
		update_stats(sc, an, frame_size, 
			     ndx, long_tries,
			     0, 0,
			     0, 0,
			     0, 0,
			     short_tries, long_tries, ts->ts_status);
	} else {
		int hwrate0, rate0, tries0, ndx0;
		int hwrate1, rate1, tries1, ndx1;
		int hwrate2, rate2, tries2, ndx2;
		int hwrate3, rate3, tries3, ndx3;
		int finalTSIdx = ts->ts_finaltsi;

		/*
		 * Process intermediate rates that failed.
		 */
		if (sc->sc_ah->ah_magic != 0x20065416) {
			hwrate0 = MS(ds0->ds_ctl3, AR_XmitRate0);
			hwrate1 = MS(ds0->ds_ctl3, AR_XmitRate1);
			hwrate2 = MS(ds0->ds_ctl3, AR_XmitRate2);
			hwrate3 = MS(ds0->ds_ctl3, AR_XmitRate3);
		} else {
			hwrate0 = MS(ds0->ds_ctl3, AR5416_XmitRate0);
			hwrate1 = MS(ds0->ds_ctl3, AR5416_XmitRate1);
			hwrate2 = MS(ds0->ds_ctl3, AR5416_XmitRate2);
			hwrate3 = MS(ds0->ds_ctl3, AR5416_XmitRate3);
		}

		rate0 = sc->sc_hwmap[hwrate0].ieeerate;
		tries0 = MS(ds0->ds_ctl2, AR_XmitDataTries0);
		ndx0 = rate_to_ndx(sn, rate0);

		rate1 = sc->sc_hwmap[hwrate1].ieeerate;
		tries1 = MS(ds0->ds_ctl2, AR_XmitDataTries1);
		ndx1 = rate_to_ndx(sn, rate1);

		rate2 = sc->sc_hwmap[hwrate2].ieeerate;
		tries2 = MS(ds0->ds_ctl2, AR_XmitDataTries2);
		ndx2 = rate_to_ndx(sn, rate2);

		rate3 = sc->sc_hwmap[hwrate3].ieeerate;
		tries3 = MS(ds0->ds_ctl2, AR_XmitDataTries3);
		ndx3 = rate_to_ndx(sn, rate3);

		DPRINTF(sc, ATH_DEBUG_RATE,
"%s: %s size %d finaltsidx %d tries %d %s rate/try [%d/%d %d/%d %d/%d %d/%d]\n", 
		     __func__, ether_sprintf(an->an_node.ni_macaddr),
		     bin_to_size(size_to_bin(frame_size)),
		     finalTSIdx,
		     long_tries, 
		     ts->ts_status ? "FAIL" : "OK",
		     rate0, tries0,
		     rate1, tries1,
		     rate2, tries2,
		     rate3, tries3);

		/*
		 * NB: series > 0 are not penalized for failure
		 * based on the try counts under the assumption
		 * that losses are often bursty and since we
		 * sample higher rates 1 try at a time doing so
		 * may unfairly penalize them.
		 */
		if (tries0) {
			update_stats(sc, an, frame_size, 
				     ndx0, tries0, 
				     ndx1, tries1, 
				     ndx2, tries2, 
				     ndx3, tries3, 
				     short_tries, long_tries, 
				     long_tries > tries0);
			long_tries -= tries0;
		}
		
		if (tries1 && finalTSIdx > 0) {
			update_stats(sc, an, frame_size, 
				     ndx1, tries1, 
				     ndx2, tries2, 
				     ndx3, tries3, 
				     0, 0, 
				     short_tries, long_tries, 
				     ts->ts_status);
			long_tries -= tries1;
		}

		if (tries2 && finalTSIdx > 1) {
			update_stats(sc, an, frame_size, 
				     ndx2, tries2, 
				     ndx3, tries3, 
				     0, 0,
				     0, 0,
				     short_tries, long_tries, 
				     ts->ts_status);
			long_tries -= tries2;
		}

		if (tries3 && finalTSIdx > 2) {
			update_stats(sc, an, frame_size, 
				     ndx3, tries3, 
				     0, 0,
				     0, 0,
				     0, 0,
				     short_tries, long_tries, 
				     ts->ts_status);
		}
	}
}

void
ath_rate_newassoc(struct ath_softc *sc, struct ath_node *an, int isnew)
{
	DPRINTF(sc, ATH_DEBUG_NODE, "%s: %s isnew %d\n", __func__,
	     ether_sprintf(an->an_node.ni_macaddr), isnew);
	if (isnew)
		ath_rate_ctl_reset(sc, &an->an_node);
}

/*
 * Initialize the tables for a node.
 */
static void
ath_rate_ctl_reset(struct ath_softc *sc, struct ieee80211_node *ni)
{
#define	RATE(_ix)	(ni->ni_rates.rs_rates[(_ix)] & IEEE80211_RATE_VAL)
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_node *an = ATH_NODE(ni);
	struct sample_node *sn = ATH_NODE_SAMPLE(an);
	const HAL_RATE_TABLE *rt = sc->sc_currates;
	int x, y, srate;

	KASSERT(rt != NULL, ("no rate table, mode %u", sc->sc_curmode));
        sn->static_rate_ndx = -1;
	if (ic->ic_fixed_rate != IEEE80211_FIXED_RATE_NONE) {
		/*
		 * A fixed rate is to be used; ic_fixed_rate is the
		 * IEEE code for this rate (sans basic bit).  Convert this
		 * to the index into the negotiated rate set for
		 * the node.
		 */
		/* NB: the rate set is assumed sorted */
		srate = ni->ni_rates.rs_nrates - 1;
		for (; srate >= 0 && RATE(srate) != ic->ic_fixed_rate; srate--)
			;
		/*
		 * The fixed rate may not be available due to races
		 * and mode settings.  Also orphaned nodes created in
		 * adhoc mode may not have any rate set so this lookup
		 * can fail.
		 */
		if (srate >= 0)
			sn->static_rate_ndx = srate;
	}

        DPRINTF(sc, ATH_DEBUG_RATE, "%s: %s size 1600 rate/tt",
	    __func__, ether_sprintf(ni->ni_macaddr));

	sn->num_rates = ni->ni_rates.rs_nrates;
        for (x = 0; x < ni->ni_rates.rs_nrates; x++) {
		sn->rates[x].rate = ni->ni_rates.rs_rates[x] & IEEE80211_RATE_VAL;
		sn->rates[x].rix = sc->sc_rixmap[sn->rates[x].rate];
		if (sn->rates[x].rix == 0xff) {
			DPRINTF(sc, ATH_DEBUG_RATE,
			    "%s: ignore bogus rix at %d\n", __func__, x);
			continue;
		}
		sn->rates[x].rateCode = rt->info[sn->rates[x].rix].rateCode;
		sn->rates[x].shortPreambleRateCode = 
			rt->info[sn->rates[x].rix].rateCode | 
			rt->info[sn->rates[x].rix].shortPreamble;

		DPRINTF(sc, ATH_DEBUG_RATE, " %d/%d", sn->rates[x].rate,
		    calc_usecs_unicast_packet(sc, 1600, sn->rates[x].rix, 0,0));
	}
	DPRINTF(sc, ATH_DEBUG_RATE, "%s\n", "");
	
	/* set the visible bit-rate to the lowest one available */
	ni->ni_txrate = 0;
	sn->num_rates = ni->ni_rates.rs_nrates;
	
	for (y = 0; y < NUM_PACKET_SIZE_BINS; y++) {
		int size = bin_to_size(y);
		int ndx = 0;
		sn->packets_sent[y] = 0;
		sn->current_sample_ndx[y] = -1;
		sn->last_sample_ndx[y] = 0;
		
		for (x = 0; x < ni->ni_rates.rs_nrates; x++) {
			sn->stats[y][x].successive_failures = 0;
			sn->stats[y][x].tries = 0;
			sn->stats[y][x].total_packets = 0;
			sn->stats[y][x].packets_acked = 0;
			sn->stats[y][x].last_tx = 0;
			
			sn->stats[y][x].perfect_tx_time = 
				calc_usecs_unicast_packet(sc, size, 
							  sn->rates[x].rix,
							  0, 0);
			sn->stats[y][x].average_tx_time = sn->stats[y][x].perfect_tx_time;
		}

		/* set the initial rate */
		for (ndx = sn->num_rates-1; ndx > 0; ndx--) {
			if (sn->rates[ndx].rate <= 72) {
				break;
			}
		}
		sn->current_rate[y] = ndx;
	}

	DPRINTF(sc, ATH_DEBUG_RATE,
	    "%s: %s %d rates %d%sMbps (%dus)- %d%sMbps (%dus)\n",
	    __func__, ether_sprintf(ni->ni_macaddr), 
	    sn->num_rates,
	    sn->rates[0].rate/2, sn->rates[0].rate % 0x1 ? ".5" : "",
	    sn->stats[1][0].perfect_tx_time,
	    sn->rates[sn->num_rates-1].rate/2,
		sn->rates[sn->num_rates-1].rate % 0x1 ? ".5" : "",
	    sn->stats[1][sn->num_rates-1].perfect_tx_time
	);

        if (sn->static_rate_ndx != -1)
		ni->ni_txrate = sn->static_rate_ndx;
	else
		ni->ni_txrate = sn->current_rate[0];
#undef RATE
}

static void
rate_cb(void *arg, struct ieee80211_node *ni)
{
	struct ath_softc *sc = arg;

	ath_rate_newassoc(sc, ATH_NODE(ni), 1);
}

/*
 * Reset the rate control state for each 802.11 state transition.
 */
void
ath_rate_newstate(struct ath_softc *sc, enum ieee80211_state state)
{
	struct ieee80211com *ic = &sc->sc_ic;

	if (state == IEEE80211_S_RUN) {
		if (ic->ic_opmode != IEEE80211_M_STA) {
			/*
			 * Sync rates for associated stations and neighbors.
			 */
			ieee80211_iterate_nodes(&ic->ic_sta, rate_cb, sc);
		}
		ath_rate_newassoc(sc, ATH_NODE(ic->ic_bss), 1);
	}
}

static void
ath_rate_sysctlattach(struct ath_softc *sc, struct sample_softc *osc)
{
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(sc->sc_dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(sc->sc_dev);

	/* XXX bounds check [0..100] */
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
		"smoothing_rate", CTLFLAG_RW, &osc->ath_smoothing_rate, 0,
		"rate control: retry threshold to credit rate raise (%%)");
	/* XXX bounds check [2..100] */
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
		"sample_rate", CTLFLAG_RW, &osc->ath_sample_rate,0,
		"rate control: # good periods before raising rate");
}

struct ath_ratectrl *
ath_rate_attach(struct ath_softc *sc)
{
	struct sample_softc *osc;
	
	DPRINTF(sc, ATH_DEBUG_ANY, "%s:\n", __func__);
	osc = malloc(sizeof(struct sample_softc), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (osc == NULL)
		return NULL;
	osc->arc.arc_space = sizeof(struct sample_node);
	osc->ath_smoothing_rate = 95;	/* ewma percentage (out of 100) */
	osc->ath_sample_rate = 10;	/* send a different bit-rate 1/X packets */
	ath_rate_sysctlattach(sc, osc);
	return &osc->arc;
}

void
ath_rate_detach(struct ath_ratectrl *arc)
{
	struct sample_softc *osc = (struct sample_softc *) arc;
	
	free(osc, M_DEVBUF);
}

/*
 * Module glue.
 */
static int
sample_modevent(module_t mod, int type, void *unused)
{
	switch (type) {
	case MOD_LOAD:
		if (bootverbose)
			printf("ath_rate: version 1.2 <SampleRate bit-rate selection algorithm>\n");
		return 0;
	case MOD_UNLOAD:
		return 0;
	}
	return EINVAL;
}

static moduledata_t sample_mod = {
	"ath_rate",
	sample_modevent,
	0
};
DECLARE_MODULE(ath_rate, sample_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);
MODULE_VERSION(ath_rate, 1);
MODULE_DEPEND(ath_rate, ath_hal, 1, 1, 1);	/* Atheros HAL */
MODULE_DEPEND(ath_rate, wlan, 1, 1, 1);
