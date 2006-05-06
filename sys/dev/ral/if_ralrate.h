/*	$FreeBSD: src/sys/dev/ral/if_ralrate.h,v 1.1 2005/04/18 18:47:36 damien Exp $	*/
/* $NetBSD: ieee80211_rssadapt.h,v 1.4 2005/02/26 22:45:09 perry Exp $ */
/*-
 * Copyright (c) 2003, 2004 David Young.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of David Young may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY David Young ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL David
 * Young BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

/* Data-rate adaptation loosely based on "Link Adaptation Strategy
 * for IEEE 802.11 WLAN via Received Signal Strength Measurement"
 * by Javier del Prado Pavon and Sunghyun Choi.
 */

/* Buckets for frames 0-128 bytes long, 129-1024, 1025-maximum. */
#define	RAL_RSSADAPT_BKTS		3
#define RAL_RSSADAPT_BKT0		128
#define	RAL_RSSADAPT_BKTPOWER	3	/* 2**_BKTPOWER */

#define	ral_rssadapt_thresh_new \
    (ral_rssadapt_thresh_denom - ral_rssadapt_thresh_old)
#define	ral_rssadapt_decay_new \
    (ral_rssadapt_decay_denom - ral_rssadapt_decay_old)
#define	ral_rssadapt_avgrssi_new \
    (ral_rssadapt_avgrssi_denom - ral_rssadapt_avgrssi_old)

struct ral_rssadapt_expavgctl {
	/* RSS threshold decay. */
	u_int rc_decay_denom;
	u_int rc_decay_old;
	/* RSS threshold update. */
	u_int rc_thresh_denom;
	u_int rc_thresh_old;
	/* RSS average update. */
	u_int rc_avgrssi_denom;
	u_int rc_avgrssi_old;
};

struct ral_rssadapt {
	/* exponential average RSSI << 8 */
	u_int16_t		ra_avg_rssi;
	/* Tx failures in this update interval */
	u_int32_t		ra_nfail;
	/* Tx successes in this update interval */
	u_int32_t		ra_nok;
	/* exponential average packets/second */
	u_int32_t		ra_pktrate;
	/* RSSI threshold for each Tx rate */
	u_int16_t		ra_rate_thresh[RAL_RSSADAPT_BKTS]
					      [IEEE80211_RATE_SIZE];
	struct timeval		ra_last_raise;
	struct timeval		ra_raise_interval;
};

/* Properties of a Tx packet, for link adaptation. */
struct ral_rssdesc {
	u_int			 id_len;	/* Tx packet length */
	u_int			 id_rateidx;	/* index into ni->ni_rates */
	struct ieee80211_node	*id_node;	/* destination STA MAC */
	u_int8_t		 id_rssi;	/* destination STA avg RSS @
						 * Tx time
						 */
};

void	ral_rssadapt_updatestats(struct ral_rssadapt *);
void	ral_rssadapt_input(struct ieee80211com *, struct ieee80211_node *,
	    struct ral_rssadapt *, int);
void	ral_rssadapt_lower_rate(struct ieee80211com *,
	    struct ieee80211_node *, struct ral_rssadapt *,
	    struct ral_rssdesc *);
void	ral_rssadapt_raise_rate(struct ieee80211com *,
	    struct ral_rssadapt *, struct ral_rssdesc *);
int	ral_rssadapt_choose(struct ral_rssadapt *,
	    struct ieee80211_rateset *, struct ieee80211_frame *, u_int,
	    const char *, int);
