/*-
 * Copyright (c) 2007-2008 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef _NET80211_IEEE80211_HT_H_
#define _NET80211_IEEE80211_HT_H_

/*
 * 802.11n protocol implementation definitions.
 */

#define	IEEE80211_AGGR_BAWMAX	64	/* max block ack window size */
/* threshold for aging overlapping non-HT bss */
#define	IEEE80211_NONHT_PRESENT_AGE	msecs_to_ticks(60*1000)

typedef uint16_t ieee80211_seq;

struct ieee80211_tx_ampdu {
	u_short		txa_flags;
#define	IEEE80211_AGGR_IMMEDIATE	0x0001	/* BA policy */
#define	IEEE80211_AGGR_XCHGPEND		0x0002	/* ADDBA response pending */
#define	IEEE80211_AGGR_RUNNING		0x0004	/* ADDBA response received */
#define	IEEE80211_AGGR_SETUP		0x0008	/* deferred state setup */
#define	IEEE80211_AGGR_NAK		0x0010	/* peer NAK'd ADDBA request */
	uint8_t		txa_ac;
	uint8_t		txa_token;	/* dialog token */
	int		txa_lastsample;	/* ticks @ last traffic sample */
	int		txa_pkts;	/* packets over last sample interval */
	int		txa_avgpps;	/* filtered traffic over window */
	int		txa_qbytes;	/* data queued (bytes) */
	short		txa_qframes;	/* data queued (frames) */
	ieee80211_seq	txa_start;	/* BA window left edge */
	ieee80211_seq	txa_seqpending;	/* new txa_start pending BAR response */
	uint16_t	txa_wnd;	/* BA window size */
	uint8_t		txa_attempts;	/* # ADDBA requests w/o a response */
	int		txa_nextrequest;/* soonest to make next ADDBA request */
	struct callout	txa_timer;
	void		*txa_private;	/* driver-private storage */
};

/* return non-zero if AMPDU tx for the TID is running */
#define	IEEE80211_AMPDU_RUNNING(tap) \
	(((tap)->txa_flags & IEEE80211_AGGR_RUNNING) != 0)

/* return non-zero if AMPDU tx for the TID is running or started */
#define	IEEE80211_AMPDU_REQUESTED(tap) \
	(((tap)->txa_flags & \
	 (IEEE80211_AGGR_RUNNING|IEEE80211_AGGR_XCHGPEND|IEEE80211_AGGR_NAK)) != 0)

/*
 * Traffic estimator support.  We estimate packets/sec for
 * each AC that is setup for AMPDU or will potentially be
 * setup for AMPDU.  The traffic rate can be used to decide
 * when AMPDU should be setup (according to a threshold)
 * and is available for drivers to do things like cache
 * eviction when only a limited number of BA streams are
 * available and more streams are requested than available.
 */

static __inline void
ieee80211_txampdu_update_pps(struct ieee80211_tx_ampdu *tap)
{
	/* NB: scale factor of 2 was picked heuristically */
	tap->txa_avgpps = ((tap->txa_avgpps << 2) -
	     tap->txa_avgpps + tap->txa_pkts) >> 2;
}

/*
 * Count a packet towards the pps estimate.
 */
static __inline void
ieee80211_txampdu_count_packet(struct ieee80211_tx_ampdu *tap)
{
	/* XXX bound loop/do more crude estimate? */
	while (ticks - tap->txa_lastsample >= hz) {
		ieee80211_txampdu_update_pps(tap);
		/* reset to start new sample interval */
		tap->txa_pkts = 0;
		if (tap->txa_avgpps == 0) {
			tap->txa_lastsample = ticks;
			break;
		}
		tap->txa_lastsample += hz;
	}
	tap->txa_pkts++;
}

/*
 * Get the current pps estimate.  If the average is out of
 * date due to lack of traffic then we decay the estimate
 * to account for the idle time.
 */
static __inline int
ieee80211_txampdu_getpps(struct ieee80211_tx_ampdu *tap)
{
	/* XXX bound loop/do more crude estimate? */
	while (ticks - tap->txa_lastsample >= hz) {
		ieee80211_txampdu_update_pps(tap);
		tap->txa_pkts = 0;
		if (tap->txa_avgpps == 0) {
			tap->txa_lastsample = ticks;
			break;
		}
		tap->txa_lastsample += hz;
	}
	return tap->txa_avgpps;
}

struct ieee80211_rx_ampdu {
	int		rxa_flags;
	int		rxa_qbytes;	/* data queued (bytes) */
	short		rxa_qframes;	/* data queued (frames) */
	ieee80211_seq	rxa_seqstart;
	ieee80211_seq	rxa_start;	/* start of current BA window */
	uint16_t	rxa_wnd;	/* BA window size */
	int		rxa_age;	/* age of oldest frame in window */
	int		rxa_nframes;	/* frames since ADDBA */
	struct mbuf *rxa_m[IEEE80211_AGGR_BAWMAX];
};

void	ieee80211_ht_attach(struct ieee80211com *);
void	ieee80211_ht_detach(struct ieee80211com *);
void	ieee80211_ht_vattach(struct ieee80211vap *);
void	ieee80211_ht_vdetach(struct ieee80211vap *);

void	ieee80211_ht_announce(struct ieee80211com *);

struct ieee80211_mcs_rates {
	uint16_t	ht20_rate_800ns;
	uint16_t	ht20_rate_400ns;
	uint16_t	ht40_rate_800ns;
	uint16_t	ht40_rate_400ns;
};
extern const struct ieee80211_mcs_rates ieee80211_htrates[16];
const struct ieee80211_htrateset *ieee80211_get_suphtrates(
		struct ieee80211com *, const struct ieee80211_channel *);

struct ieee80211_node;
int	ieee80211_setup_htrates(struct ieee80211_node *,
		const uint8_t *htcap, int flags);
void	ieee80211_setup_basic_htrates(struct ieee80211_node *,
		const uint8_t *htinfo);
struct mbuf *ieee80211_decap_amsdu(struct ieee80211_node *, struct mbuf *);
int	ieee80211_ampdu_reorder(struct ieee80211_node *, struct mbuf *);
void	ieee80211_recv_bar(struct ieee80211_node *, struct mbuf *);
void	ieee80211_ht_node_init(struct ieee80211_node *, const uint8_t *);
void	ieee80211_ht_node_cleanup(struct ieee80211_node *);
void	ieee80211_ht_node_age(struct ieee80211_node *);

struct ieee80211_channel *ieee80211_ht_adjust_channel(struct ieee80211com *,
		struct ieee80211_channel *, int);
void	ieee80211_ht_wds_init(struct ieee80211_node *);
void	ieee80211_ht_node_join(struct ieee80211_node *);
void	ieee80211_ht_node_leave(struct ieee80211_node *);
void	ieee80211_htprot_update(struct ieee80211com *, int protmode);
void	ieee80211_ht_timeout(struct ieee80211com *);
void	ieee80211_parse_htcap(struct ieee80211_node *, const uint8_t *);
void	ieee80211_parse_htinfo(struct ieee80211_node *, const uint8_t *);
void	ieee80211_recv_action(struct ieee80211_node *,
		const uint8_t *, const uint8_t *);
int	ieee80211_ampdu_request(struct ieee80211_node *,
		struct ieee80211_tx_ampdu *);
void	ieee80211_ampdu_stop(struct ieee80211_node *,
		struct ieee80211_tx_ampdu *);
int	ieee80211_send_bar(struct ieee80211_node *,
		const struct ieee80211_tx_ampdu *);
int	ieee80211_send_action(struct ieee80211_node *,
		int, int, uint16_t [4]);
uint8_t	*ieee80211_add_htcap(uint8_t *, struct ieee80211_node *);
uint8_t	*ieee80211_add_htcap_vendor(uint8_t *, struct ieee80211_node *);
uint8_t	*ieee80211_add_htinfo(uint8_t *, struct ieee80211_node *);
uint8_t	*ieee80211_add_htinfo_vendor(uint8_t *, struct ieee80211_node *);
struct ieee80211_beacon_offsets;
void	ieee80211_ht_update_beacon(struct ieee80211vap *,
		struct ieee80211_beacon_offsets *);
#endif /* _NET80211_IEEE80211_HT_H_ */
