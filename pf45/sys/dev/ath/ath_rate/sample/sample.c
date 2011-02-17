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
__FBSDID("$FreeBSD$");

/*
 * John Bicket's SampleRate control algorithm.
 */
#include "opt_inet.h"
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/sysctl.h>
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
#include <dev/ath/ath_hal/ah_desc.h>
#include <dev/ath/ath_rate/sample/tx_schedules.h>

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

static void	ath_rate_ctl_reset(struct ath_softc *, struct ieee80211_node *);

static const int packet_size_bins[NUM_PACKET_SIZE_BINS] = { 250, 1600 };

static __inline int
size_to_bin(int size) 
{
#if NUM_PACKET_SIZE_BINS > 1
	if (size <= packet_size_bins[0])
		return 0;
#endif
#if NUM_PACKET_SIZE_BINS > 2
	if (size <= packet_size_bins[1])
		return 1;
#endif
#if NUM_PACKET_SIZE_BINS > 3
	if (size <= packet_size_bins[2])
		return 2;
#endif
#if NUM_PACKET_SIZE_BINS > 4
#error "add support for more packet sizes"
#endif
	return NUM_PACKET_SIZE_BINS-1;
}

static __inline int
bin_to_size(int index)
{
	return packet_size_bins[index];
}

void
ath_rate_node_init(struct ath_softc *sc, struct ath_node *an)
{
	/* NB: assumed to be zero'd by caller */
}

void
ath_rate_node_cleanup(struct ath_softc *sc, struct ath_node *an)
{
}

static int
dot11rate(const HAL_RATE_TABLE *rt, int rix)
{
	return rt->info[rix].phy == IEEE80211_T_HT ?
	    rt->info[rix].dot11Rate : (rt->info[rix].dot11Rate & IEEE80211_RATE_VAL) / 2;
}

/*
 * Return the rix with the lowest average_tx_time,
 * or -1 if all the average_tx_times are 0.
 */
static __inline int
pick_best_rate(struct sample_node *sn, const HAL_RATE_TABLE *rt,
    int size_bin, int require_acked_before)
{
        int best_rate_rix, best_rate_tt;
	uint32_t mask;
	int rix, tt;

        best_rate_rix = 0;
        best_rate_tt = 0;
	for (mask = sn->ratemask, rix = 0; mask != 0; mask >>= 1, rix++) {
		if ((mask & 1) == 0)		/* not a supported rate */
			continue;

		tt = sn->stats[size_bin][rix].average_tx_time;
		if (tt <= 0 ||
		    (require_acked_before &&
		     !sn->stats[size_bin][rix].packets_acked))
			continue;

		/* don't use a bit-rate that has been failing */
		if (sn->stats[size_bin][rix].successive_failures > 3)
			continue;

		if (best_rate_tt == 0 || tt < best_rate_tt) {
			best_rate_tt = tt;
			best_rate_rix = rix;
		}
        }
        return (best_rate_tt ? best_rate_rix : -1);
}

/*
 * Pick a good "random" bit-rate to sample other than the current one.
 */
static __inline int
pick_sample_rate(struct sample_softc *ssc , struct sample_node *sn,
    const HAL_RATE_TABLE *rt, int size_bin)
{
#define	DOT11RATE(ix)	(rt->info[ix].dot11Rate & IEEE80211_RATE_VAL)
#define	MCS(ix)		(rt->info[ix].dot11Rate | IEEE80211_RATE_MCS)
	int current_rix, rix;
	unsigned current_tt;
	uint32_t mask;
	
	current_rix = sn->current_rix[size_bin];
	if (current_rix < 0) {
		/* no successes yet, send at the lowest bit-rate */
		return 0;
	}

	current_tt = sn->stats[size_bin][current_rix].average_tx_time;

	rix = sn->last_sample_rix[size_bin]+1;	/* next sample rate */
	mask = sn->ratemask &~ (1<<current_rix);/* don't sample current rate */
	while (mask != 0) {
		if ((mask & (1<<rix)) == 0) {	/* not a supported rate */
	nextrate:
			if (++rix >= rt->rateCount)
				rix = 0;
			continue;
		}

		/* this bit-rate is always worse than the current one */
		if (sn->stats[size_bin][rix].perfect_tx_time > current_tt) {
			mask &= ~(1<<rix);
			goto nextrate;
		}

		/* rarely sample bit-rates that fail a lot */
		if (sn->stats[size_bin][rix].successive_failures > ssc->max_successive_failures &&
		    ticks - sn->stats[size_bin][rix].last_tx < ssc->stale_failure_timeout) {
			mask &= ~(1<<rix);
			goto nextrate;
		}

		/* don't sample more than 2 rates higher for rates > 11M */
		if (DOT11RATE(rix) > 2*11 && rix > current_rix + 2) {
			mask &= ~(1<<rix);
			goto nextrate;
		}

		sn->last_sample_rix[size_bin] = rix;
		return rix;
	}
	return current_rix;
#undef DOT11RATE
#undef	MCS
}

void
ath_rate_findrate(struct ath_softc *sc, struct ath_node *an,
		  int shortPreamble, size_t frameLen,
		  u_int8_t *rix0, int *try0, u_int8_t *txrate)
{
#define	DOT11RATE(ix)	(rt->info[ix].dot11Rate & IEEE80211_RATE_VAL)
#define	MCS(ix)		(rt->info[ix].dot11Rate | IEEE80211_RATE_MCS)
#define	RATE(ix)	(DOT11RATE(ix) / 2)
	struct sample_node *sn = ATH_NODE_SAMPLE(an);
	struct sample_softc *ssc = ATH_SOFTC_SAMPLE(sc);
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	const HAL_RATE_TABLE *rt = sc->sc_currates;
	const int size_bin = size_to_bin(frameLen);
	int rix, mrr, best_rix, change_rates;
	unsigned average_tx_time;

	if (sn->static_rix != -1) {
		rix = sn->static_rix;
		*try0 = ATH_TXMAXTRY;
		goto done;
	}

	mrr = sc->sc_mrretry && !(ic->ic_flags & IEEE80211_F_USEPROT);

	best_rix = pick_best_rate(sn, rt, size_bin, !mrr);
	if (best_rix >= 0) {
		average_tx_time = sn->stats[size_bin][best_rix].average_tx_time;
	} else {
		average_tx_time = 0;
	}
	/*
	 * Limit the time measuring the performance of other tx
	 * rates to sample_rate% of the total transmission time.
	 */
	if (sn->sample_tt[size_bin] < average_tx_time * (sn->packets_since_sample[size_bin]*ssc->sample_rate/100)) {
		rix = pick_sample_rate(ssc, sn, rt, size_bin);
		IEEE80211_NOTE(an->an_node.ni_vap, IEEE80211_MSG_RATECTL,
		     &an->an_node, "size %u sample rate %d current rate %d",
		     bin_to_size(size_bin), RATE(rix),
		     RATE(sn->current_rix[size_bin]));
		if (rix != sn->current_rix[size_bin]) {
			sn->current_sample_rix[size_bin] = rix;
		} else {
			sn->current_sample_rix[size_bin] = -1;
		}
		sn->packets_since_sample[size_bin] = 0;
	} else {
		change_rates = 0;
		if (!sn->packets_sent[size_bin] || best_rix == -1) {
			/* no packet has been sent successfully yet */
			for (rix = rt->rateCount-1; rix > 0; rix--) {
				if ((sn->ratemask & (1<<rix)) == 0)
					continue;
				/* 
				 * Pick the highest rate <= 36 Mbps
				 * that hasn't failed.
				 */
				if (DOT11RATE(rix) <= 72 && 
				    sn->stats[size_bin][rix].successive_failures == 0) {
					break;
				}
			}
			change_rates = 1;
			best_rix = rix;
		} else if (sn->packets_sent[size_bin] < 20) {
			/* let the bit-rate switch quickly during the first few packets */
			change_rates = 1;
		} else if (ticks - ssc->min_switch > sn->ticks_since_switch[size_bin]) {
			/* min_switch seconds have gone by */
			change_rates = 1;
		} else if (2*average_tx_time < sn->stats[size_bin][sn->current_rix[size_bin]].average_tx_time) {
			/* the current bit-rate is twice as slow as the best one */
			change_rates = 1;
		}

		sn->packets_since_sample[size_bin]++;
		
		if (change_rates) {
			if (best_rix != sn->current_rix[size_bin]) {
				IEEE80211_NOTE(an->an_node.ni_vap,
				    IEEE80211_MSG_RATECTL,
				    &an->an_node,
"%s: size %d switch rate %d (%d/%d) -> %d (%d/%d) after %d packets mrr %d",
				    __func__,
				    bin_to_size(size_bin),
				    RATE(sn->current_rix[size_bin]),
				    sn->stats[size_bin][sn->current_rix[size_bin]].average_tx_time,
				    sn->stats[size_bin][sn->current_rix[size_bin]].perfect_tx_time,
				    RATE(best_rix),
				    sn->stats[size_bin][best_rix].average_tx_time,
				    sn->stats[size_bin][best_rix].perfect_tx_time,
				    sn->packets_since_switch[size_bin],
				    mrr);
			}
			sn->packets_since_switch[size_bin] = 0;
			sn->current_rix[size_bin] = best_rix;
			sn->ticks_since_switch[size_bin] = ticks;
			/* 
			 * Set the visible txrate for this node.
			 */
			an->an_node.ni_txrate = (rt->info[best_rix].phy == IEEE80211_T_HT) ?  MCS(best_rix) : DOT11RATE(best_rix);
		}
		rix = sn->current_rix[size_bin];
		sn->packets_since_switch[size_bin]++;
	}
	*try0 = mrr ? sn->sched[rix].t0 : ATH_TXMAXTRY;
done:
	KASSERT(rix >= 0 && rix < rt->rateCount, ("rix is %d", rix));

	*rix0 = rix;
	*txrate = rt->info[rix].rateCode
		| (shortPreamble ? rt->info[rix].shortPreamble : 0);
	sn->packets_sent[size_bin]++;
#undef DOT11RATE
#undef MCS
#undef RATE
}

/*
 * Get the TX rates. Don't fiddle with short preamble flags for them;
 * the caller can do that.
 */
void
ath_rate_getxtxrates(struct ath_softc *sc, struct ath_node *an,
    uint8_t rix0, uint8_t *rix, uint8_t *try)
{
	struct sample_node *sn = ATH_NODE_SAMPLE(an);
	const struct txschedule *sched = &sn->sched[rix0];

	KASSERT(rix0 == sched->r0, ("rix0 (%x) != sched->r0 (%x)!\n", rix0, sched->r0));

/*	rix[0] = sched->r0; */
	rix[1] = sched->r1;
	rix[2] = sched->r2;
	rix[3] = sched->r3;

	try[0] = sched->t0;
	try[1] = sched->t1;
	try[2] = sched->t2;
	try[3] = sched->t3;
}

void
ath_rate_setupxtxdesc(struct ath_softc *sc, struct ath_node *an,
		      struct ath_desc *ds, int shortPreamble, u_int8_t rix)
{
	struct sample_node *sn = ATH_NODE_SAMPLE(an);
	const struct txschedule *sched = &sn->sched[rix];
	const HAL_RATE_TABLE *rt = sc->sc_currates;
	uint8_t rix1, s1code, rix2, s2code, rix3, s3code;

	/* XXX precalculate short preamble tables */
	rix1 = sched->r1;
	s1code = rt->info[rix1].rateCode
	       | (shortPreamble ? rt->info[rix1].shortPreamble : 0);
	rix2 = sched->r2;
	s2code = rt->info[rix2].rateCode
	       | (shortPreamble ? rt->info[rix2].shortPreamble : 0);
	rix3 = sched->r3;
	s3code = rt->info[rix3].rateCode
	       | (shortPreamble ? rt->info[rix3].shortPreamble : 0);
	ath_hal_setupxtxdesc(sc->sc_ah, ds,
	    s1code, sched->t1,		/* series 1 */
	    s2code, sched->t2,		/* series 2 */
	    s3code, sched->t3);		/* series 3 */
}

static void
update_stats(struct ath_softc *sc, struct ath_node *an, 
		  int frame_size,
		  int rix0, int tries0,
		  int rix1, int tries1,
		  int rix2, int tries2,
		  int rix3, int tries3,
		  int short_tries, int tries, int status)
{
	struct sample_node *sn = ATH_NODE_SAMPLE(an);
	struct sample_softc *ssc = ATH_SOFTC_SAMPLE(sc);
	const int size_bin = size_to_bin(frame_size);
	const int size = bin_to_size(size_bin);
	int tt, tries_so_far;
	int is_ht40 = (an->an_node.ni_htcap & IEEE80211_HTCAP_CHWIDTH40);

	if (!IS_RATE_DEFINED(sn, rix0))
		return;
	tt = calc_usecs_unicast_packet(sc, size, rix0, short_tries,
		MIN(tries0, tries) - 1, is_ht40);
	tries_so_far = tries0;

	if (tries1 && tries_so_far < tries) {
		if (!IS_RATE_DEFINED(sn, rix1))
			return;
		tt += calc_usecs_unicast_packet(sc, size, rix1, short_tries,
			MIN(tries1 + tries_so_far, tries) - tries_so_far - 1, is_ht40);
		tries_so_far += tries1;
	}

	if (tries2 && tries_so_far < tries) {
		if (!IS_RATE_DEFINED(sn, rix2))
			return;
		tt += calc_usecs_unicast_packet(sc, size, rix2, short_tries,
			MIN(tries2 + tries_so_far, tries) - tries_so_far - 1, is_ht40);
		tries_so_far += tries2;
	}

	if (tries3 && tries_so_far < tries) {
		if (!IS_RATE_DEFINED(sn, rix3))
			return;
		tt += calc_usecs_unicast_packet(sc, size, rix3, short_tries,
			MIN(tries3 + tries_so_far, tries) - tries_so_far - 1, is_ht40);
	}

	if (sn->stats[size_bin][rix0].total_packets < ssc->smoothing_minpackets) {
		/* just average the first few packets */
		int avg_tx = sn->stats[size_bin][rix0].average_tx_time;
		int packets = sn->stats[size_bin][rix0].total_packets;
		sn->stats[size_bin][rix0].average_tx_time = (tt+(avg_tx*packets))/(packets+1);
	} else {
		/* use a ewma */
		sn->stats[size_bin][rix0].average_tx_time = 
			((sn->stats[size_bin][rix0].average_tx_time * ssc->smoothing_rate) + 
			 (tt * (100 - ssc->smoothing_rate))) / 100;
	}
	
	if (status != 0) {
		int y;
		sn->stats[size_bin][rix0].successive_failures++;
		for (y = size_bin+1; y < NUM_PACKET_SIZE_BINS; y++) {
			/*
			 * Also say larger packets failed since we
			 * assume if a small packet fails at a
			 * bit-rate then a larger one will also.
			 */
			sn->stats[y][rix0].successive_failures++;
			sn->stats[y][rix0].last_tx = ticks;
			sn->stats[y][rix0].tries += tries;
			sn->stats[y][rix0].total_packets++;
		}
	} else {
		sn->stats[size_bin][rix0].packets_acked++;
		sn->stats[size_bin][rix0].successive_failures = 0;
	}
	sn->stats[size_bin][rix0].tries += tries;
	sn->stats[size_bin][rix0].last_tx = ticks;
	sn->stats[size_bin][rix0].total_packets++;

	if (rix0 == sn->current_sample_rix[size_bin]) {
		IEEE80211_NOTE(an->an_node.ni_vap, IEEE80211_MSG_RATECTL,
		   &an->an_node,
"%s: size %d %s sample rate %d tries (%d/%d) tt %d avg_tt (%d/%d)", 
		    __func__, 
		    size,
		    status ? "FAIL" : "OK",
		    rix0, short_tries, tries, tt, 
		    sn->stats[size_bin][rix0].average_tx_time,
		    sn->stats[size_bin][rix0].perfect_tx_time);
		sn->sample_tt[size_bin] = tt;
		sn->current_sample_rix[size_bin] = -1;
	}
}

static void
badrate(struct ifnet *ifp, int series, int hwrate, int tries, int status)
{
	if_printf(ifp, "bad series%d hwrate 0x%x, tries %u ts_status 0x%x\n",
	    series, hwrate, tries, status);
}

void
ath_rate_tx_complete(struct ath_softc *sc, struct ath_node *an,
	const struct ath_buf *bf)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct sample_node *sn = ATH_NODE_SAMPLE(an);
	const struct ath_tx_status *ts = &bf->bf_status.ds_txstat;
	const struct ath_desc *ds0 = &bf->bf_desc[0];
	int final_rix, short_tries, long_tries, frame_size;
	const HAL_RATE_TABLE *rt = sc->sc_currates;
	int mrr;

	final_rix = rt->rateCodeToIndex[ts->ts_rate];
	short_tries = ts->ts_shortretry;
	long_tries = ts->ts_longretry + 1;
	frame_size = ds0->ds_ctl0 & 0x0fff; /* low-order 12 bits of ds_ctl0 */
	if (frame_size == 0)		    /* NB: should not happen */
		frame_size = 1500;

	if (sn->ratemask == 0) {
		IEEE80211_NOTE(an->an_node.ni_vap, IEEE80211_MSG_RATECTL,
		    &an->an_node,
		    "%s: size %d %s rate/try %d/%d no rates yet", 
		    __func__,
		    bin_to_size(size_to_bin(frame_size)),
		    ts->ts_status ? "FAIL" : "OK",
		    short_tries, long_tries);
		return;
	}
	mrr = sc->sc_mrretry && !(ic->ic_flags & IEEE80211_F_USEPROT);
	if (!mrr || ts->ts_finaltsi == 0) {
		if (!IS_RATE_DEFINED(sn, final_rix)) {
			badrate(ifp, 0, ts->ts_rate, long_tries, ts->ts_status);
			return;
		}
		/*
		 * Only one rate was used; optimize work.
		 */
		IEEE80211_NOTE(an->an_node.ni_vap, IEEE80211_MSG_RATECTL,
		     &an->an_node, "%s: size %d %s rate/try %d/%d/%d",
		     __func__,
		     bin_to_size(size_to_bin(frame_size)),
		     ts->ts_status ? "FAIL" : "OK",
		     dot11rate(rt, final_rix), short_tries, long_tries);
		update_stats(sc, an, frame_size, 
			     final_rix, long_tries,
			     0, 0,
			     0, 0,
			     0, 0,
			     short_tries, long_tries, ts->ts_status);
	} else {
		int hwrates[4], tries[4], rix[4];
		int finalTSIdx = ts->ts_finaltsi;
		int i;

		/*
		 * Process intermediate rates that failed.
		 */
		ath_hal_gettxcompletionrates(sc->sc_ah, ds0, hwrates, tries);

		for (i = 0; i < 4; i++) {
			rix[i] = rt->rateCodeToIndex[hwrates[i]];
		}

		IEEE80211_NOTE(an->an_node.ni_vap, IEEE80211_MSG_RATECTL,
		    &an->an_node,
"%s: size %d finaltsidx %d tries %d %s rate/try [%d/%d %d/%d %d/%d %d/%d]", 
		     __func__,
		     bin_to_size(size_to_bin(frame_size)),
		     finalTSIdx,
		     long_tries, 
		     ts->ts_status ? "FAIL" : "OK",
		     dot11rate(rt, rix[0]), tries[0],
		     dot11rate(rt, rix[1]), tries[1],
		     dot11rate(rt, rix[2]), tries[2],
		     dot11rate(rt, rix[3]), tries[3]);

		for (i = 0; i < 4; i++) {
			if (tries[i] && !IS_RATE_DEFINED(sn, rix[i]))
				badrate(ifp, 0, hwrates[i], tries[i], ts->ts_status);
		}

		/*
		 * NB: series > 0 are not penalized for failure
		 * based on the try counts under the assumption
		 * that losses are often bursty and since we
		 * sample higher rates 1 try at a time doing so
		 * may unfairly penalize them.
		 */
		if (tries[0]) {
			update_stats(sc, an, frame_size, 
				     rix[0], tries[0], 
				     rix[1], tries[1], 
				     rix[2], tries[2], 
				     rix[3], tries[3], 
				     short_tries, long_tries, 
				     long_tries > tries[0]);
			long_tries -= tries[0];
		}
		
		if (tries[1] && finalTSIdx > 0) {
			update_stats(sc, an, frame_size, 
				     rix[1], tries[1], 
				     rix[2], tries[2], 
				     rix[3], tries[3], 
				     0, 0, 
				     short_tries, long_tries, 
				     ts->ts_status);
			long_tries -= tries[1];
		}

		if (tries[2] && finalTSIdx > 1) {
			update_stats(sc, an, frame_size, 
				     rix[2], tries[2], 
				     rix[3], tries[3], 
				     0, 0,
				     0, 0,
				     short_tries, long_tries, 
				     ts->ts_status);
			long_tries -= tries[2];
		}

		if (tries[3] && finalTSIdx > 2) {
			update_stats(sc, an, frame_size, 
				     rix[3], tries[3],
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
	if (isnew)
		ath_rate_ctl_reset(sc, &an->an_node);
}

static const struct txschedule *mrr_schedules[IEEE80211_MODE_MAX+2] = {
	NULL,		/* IEEE80211_MODE_AUTO */
	series_11a,	/* IEEE80211_MODE_11A */
	series_11g,	/* IEEE80211_MODE_11B */
	series_11g,	/* IEEE80211_MODE_11G */
	NULL,		/* IEEE80211_MODE_FH */
	series_11a,	/* IEEE80211_MODE_TURBO_A */
	series_11g,	/* IEEE80211_MODE_TURBO_G */
	series_11a,	/* IEEE80211_MODE_STURBO_A */
	series_11na,	/* IEEE80211_MODE_11NA */
	series_11ng,	/* IEEE80211_MODE_11NG */
	series_half,	/* IEEE80211_MODE_HALF */
	series_quarter,	/* IEEE80211_MODE_QUARTER */
};

/*
 * Initialize the tables for a node.
 */
static void
ath_rate_ctl_reset(struct ath_softc *sc, struct ieee80211_node *ni)
{
#define	RATE(_ix)	(ni->ni_rates.rs_rates[(_ix)] & IEEE80211_RATE_VAL)
#define	DOT11RATE(_ix)	(rt->info[(_ix)].dot11Rate & IEEE80211_RATE_VAL)
#define	MCS(_ix)	(ni->ni_htrates.rs_rates[_ix] | IEEE80211_RATE_MCS)

	struct ath_node *an = ATH_NODE(ni);
	const struct ieee80211_txparam *tp = ni->ni_txparms;
	struct sample_node *sn = ATH_NODE_SAMPLE(an);
	const HAL_RATE_TABLE *rt = sc->sc_currates;
	int x, y, srate, rix;

	KASSERT(rt != NULL, ("no rate table, mode %u", sc->sc_curmode));

	KASSERT(sc->sc_curmode < IEEE80211_MODE_MAX+2,
	    ("curmode %u", sc->sc_curmode));
	sn->sched = mrr_schedules[sc->sc_curmode];
	KASSERT(sn->sched != NULL,
	    ("no mrr schedule for mode %u", sc->sc_curmode));

        sn->static_rix = -1;
	if (tp != NULL && tp->ucastrate != IEEE80211_FIXED_RATE_NONE) {
		/*
		 * A fixed rate is to be used; ucastrate is the IEEE code
		 * for this rate (sans basic bit).  Check this against the
		 * negotiated rate set for the node.  Note the fixed rate
		 * may not be available for various reasons so we only
		 * setup the static rate index if the lookup is successful.
		 */

		/* XXX todo: check MCS rates */

		/* Check legacy rates */
		for (srate = ni->ni_rates.rs_nrates - 1; srate >= 0; srate--)
			if (RATE(srate) == tp->ucastrate) {
				sn->static_rix = sc->sc_rixmap[tp->ucastrate];
				break;
			}
#ifdef IEEE80211_DEBUG
			if (sn->static_rix == -1) {
				IEEE80211_NOTE(ni->ni_vap,
				    IEEE80211_MSG_RATECTL, ni,
				    "%s: ucastrate %u not found, nrates %u",
				    __func__, tp->ucastrate,
				    ni->ni_rates.rs_nrates);
			}
#endif
	}

	/*
	 * Construct a bitmask of usable rates.  This has all
	 * negotiated rates minus those marked by the hal as
	 * to be ignored for doing rate control.
	 */
	sn->ratemask = 0;
	/* MCS rates */
	if (ni->ni_flags & IEEE80211_NODE_HT) {
		for (x = 0; x < ni->ni_htrates.rs_nrates; x++) {
			rix = sc->sc_rixmap[MCS(x)];
			if (rix == 0xff)
				continue;
			/* skip rates marked broken by hal */
			if (!rt->info[rix].valid)
				continue;
			KASSERT(rix < SAMPLE_MAXRATES,
			    ("mcs %u has rix %d", MCS(x), rix));
			sn->ratemask |= 1<<rix;
		}
	}

	/* Legacy rates */
	for (x = 0; x < ni->ni_rates.rs_nrates; x++) {
		rix = sc->sc_rixmap[RATE(x)];
		if (rix == 0xff)
			continue;
		/* skip rates marked broken by hal */
		if (!rt->info[rix].valid)
			continue;
		KASSERT(rix < SAMPLE_MAXRATES,
		    ("rate %u has rix %d", RATE(x), rix));
		sn->ratemask |= 1<<rix;
	}
#ifdef IEEE80211_DEBUG
	if (ieee80211_msg(ni->ni_vap, IEEE80211_MSG_RATECTL)) {
		uint32_t mask;

		ieee80211_note(ni->ni_vap, "[%6D] %s: size 1600 rate/tt",
		    ni->ni_macaddr, ":", __func__);
		for (mask = sn->ratemask, rix = 0; mask != 0; mask >>= 1, rix++) {
			if ((mask & 1) == 0)
				continue;
			printf(" %d/%d", dot11rate(rt, rix),
			    calc_usecs_unicast_packet(sc, 1600, rix, 0,0,
			        (ni->ni_htcap & IEEE80211_HTCAP_CHWIDTH40)));
		}
		printf("\n");
	}
#endif
	for (y = 0; y < NUM_PACKET_SIZE_BINS; y++) {
		int size = bin_to_size(y);
		uint32_t mask;

		sn->packets_sent[y] = 0;
		sn->current_sample_rix[y] = -1;
		sn->last_sample_rix[y] = 0;
		/* XXX start with first valid rate */
		sn->current_rix[y] = ffs(sn->ratemask)-1;
		
		/*
		 * Initialize the statistics buckets; these are
		 * indexed by the rate code index.
		 */
		for (rix = 0, mask = sn->ratemask; mask != 0; rix++, mask >>= 1) {
			if ((mask & 1) == 0)		/* not a valid rate */
				continue;
			sn->stats[y][rix].successive_failures = 0;
			sn->stats[y][rix].tries = 0;
			sn->stats[y][rix].total_packets = 0;
			sn->stats[y][rix].packets_acked = 0;
			sn->stats[y][rix].last_tx = 0;
			
			sn->stats[y][rix].perfect_tx_time =
			    calc_usecs_unicast_packet(sc, size, rix, 0, 0,
			    (ni->ni_htcap & IEEE80211_HTCAP_CHWIDTH40));
			sn->stats[y][rix].average_tx_time =
			    sn->stats[y][rix].perfect_tx_time;
		}
	}
#if 0
	/* XXX 0, num_rates-1 are wrong */
	IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_RATECTL, ni,
	    "%s: %d rates %d%sMbps (%dus)- %d%sMbps (%dus)", __func__, 
	    sn->num_rates,
	    DOT11RATE(0)/2, DOT11RATE(0) % 1 ? ".5" : "",
	    sn->stats[1][0].perfect_tx_time,
	    DOT11RATE(sn->num_rates-1)/2, DOT11RATE(sn->num_rates-1) % 1 ? ".5" : "",
	    sn->stats[1][sn->num_rates-1].perfect_tx_time
	);
#endif
	/* set the visible bit-rate */
	if (sn->static_rix != -1)
		ni->ni_txrate = DOT11RATE(sn->static_rix);
	else
		ni->ni_txrate = RATE(0);
#undef RATE
#undef DOT11RATE
}

static void
sample_stats(void *arg, struct ieee80211_node *ni)
{
	struct ath_softc *sc = arg;
	const HAL_RATE_TABLE *rt = sc->sc_currates;
	struct sample_node *sn = ATH_NODE_SAMPLE(ATH_NODE(ni));
	uint32_t mask;
	int rix, y;

	printf("\n[%s] refcnt %d static_rix %d ratemask 0x%x\n",
	    ether_sprintf(ni->ni_macaddr), ieee80211_node_refcnt(ni),
	    sn->static_rix, sn->ratemask);
	for (y = 0; y < NUM_PACKET_SIZE_BINS; y++) {
		printf("[%4u] cur rix %d since switch: packets %d ticks %u\n",
		    bin_to_size(y), sn->current_rix[y],
		    sn->packets_since_switch[y], sn->ticks_since_switch[y]);
		printf("[%4u] last sample %d cur sample %d packets sent %d\n",
		    bin_to_size(y), sn->last_sample_rix[y],
		    sn->current_sample_rix[y], sn->packets_sent[y]);
		printf("[%4u] packets since sample %d sample tt %u\n",
		    bin_to_size(y), sn->packets_since_sample[y],
		    sn->sample_tt[y]);
	}
	for (mask = sn->ratemask, rix = 0; mask != 0; mask >>= 1, rix++) {
		if ((mask & 1) == 0)
				continue;
		for (y = 0; y < NUM_PACKET_SIZE_BINS; y++) {
			if (sn->stats[y][rix].total_packets == 0)
				continue;
			printf("[%2u:%4u] %8d:%-8d (%3d%%) T %8d F %4d avg %5u last %u\n",
			    dot11rate(rt, rix),
			    bin_to_size(y),
			    sn->stats[y][rix].total_packets,
			    sn->stats[y][rix].packets_acked,
			    (100*sn->stats[y][rix].packets_acked)/sn->stats[y][rix].total_packets,
			    sn->stats[y][rix].tries,
			    sn->stats[y][rix].successive_failures,
			    sn->stats[y][rix].average_tx_time,
			    ticks - sn->stats[y][rix].last_tx);
		}
	}
}

static int
ath_rate_sysctl_stats(SYSCTL_HANDLER_ARGS)
{
	struct ath_softc *sc = arg1;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	int error, v;

	v = 0;
	error = sysctl_handle_int(oidp, &v, 0, req);
	if (error || !req->newptr)
		return error;
	ieee80211_iterate_nodes(&ic->ic_sta, sample_stats, sc);
	return 0;
}

static int
ath_rate_sysctl_smoothing_rate(SYSCTL_HANDLER_ARGS)
{
	struct sample_softc *ssc = arg1;
	int rate, error;

	rate = ssc->smoothing_rate;
	error = sysctl_handle_int(oidp, &rate, 0, req);
	if (error || !req->newptr)
		return error;
	if (!(0 <= rate && rate < 100))
		return EINVAL;
	ssc->smoothing_rate = rate;
	ssc->smoothing_minpackets = 100 / (100 - rate);
	return 0;
}

static int
ath_rate_sysctl_sample_rate(SYSCTL_HANDLER_ARGS)
{
	struct sample_softc *ssc = arg1;
	int rate, error;

	rate = ssc->sample_rate;
	error = sysctl_handle_int(oidp, &rate, 0, req);
	if (error || !req->newptr)
		return error;
	if (!(2 <= rate && rate <= 100))
		return EINVAL;
	ssc->sample_rate = rate;
	return 0;
}

static void
ath_rate_sysctlattach(struct ath_softc *sc, struct sample_softc *ssc)
{
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(sc->sc_dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(sc->sc_dev);

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "smoothing_rate", CTLTYPE_INT | CTLFLAG_RW, ssc, 0,
	    ath_rate_sysctl_smoothing_rate, "I",
	    "sample: smoothing rate for avg tx time (%%)");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "sample_rate", CTLTYPE_INT | CTLFLAG_RW, ssc, 0,
	    ath_rate_sysctl_sample_rate, "I",
	    "sample: percent air time devoted to sampling new rates (%%)");
	/* XXX max_successive_failures, stale_failure_timeout, min_switch */
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "sample_stats", CTLTYPE_INT | CTLFLAG_RW, sc, 0,
	    ath_rate_sysctl_stats, "I", "sample: print statistics");
}

struct ath_ratectrl *
ath_rate_attach(struct ath_softc *sc)
{
	struct sample_softc *ssc;
	
	ssc = malloc(sizeof(struct sample_softc), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (ssc == NULL)
		return NULL;
	ssc->arc.arc_space = sizeof(struct sample_node);
	ssc->smoothing_rate = 95;		/* ewma percentage ([0..99]) */
	ssc->smoothing_minpackets = 100 / (100 - ssc->smoothing_rate);
	ssc->sample_rate = 10;			/* %time to try diff tx rates */
	ssc->max_successive_failures = 3;	/* threshold for rate sampling*/
	ssc->stale_failure_timeout = 10 * hz;	/* 10 seconds */
	ssc->min_switch = hz;			/* 1 second */
	ath_rate_sysctlattach(sc, ssc);
	return &ssc->arc;
}

void
ath_rate_detach(struct ath_ratectrl *arc)
{
	struct sample_softc *ssc = (struct sample_softc *) arc;
	
	free(ssc, M_DEVBUF);
}
