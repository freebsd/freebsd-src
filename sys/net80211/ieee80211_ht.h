/*-
 * Copyright (c) 2007 Sam Leffler, Errno Consulting
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

#define	IEEE80211_SEND_ACTION(_ni,_cat, _act, _args) \
	((*(_ic)->ic_send_action)(_ni, _cat, _act, _args))

#define	IEEE80211_AGGR_BAWMAX	64	/* max block ack window size */

typedef uint16_t ieee80211_seq;

struct ieee80211_tx_ampdu {
	u_short		txa_flags;
#define	IEEE80211_AGGR_IMMEDIATE	0x0001	/* BA policy */
#define	IEEE80211_AGGR_XCHGPEND		0x0002	/* ADDBA response pending */
#define	IEEE80211_AGGR_RUNNING		0x0004	/* ADDBA response received */
#define	IEEE80211_AGGR_SETUP		0x0008	/* deferred state setup */
	uint8_t		txa_ac;
	uint8_t		txa_token;	/* dialog token */
	int		txa_qbytes;	/* data queued (bytes) */
	short		txa_qframes;	/* data queued (frames) */
	ieee80211_seq	txa_seqstart;
	ieee80211_seq	txa_start;
	uint16_t	txa_wnd;	/* BA window size */
	uint8_t		txa_attempts;	/* # setup attempts */
	int		txa_lastrequest;/* time of last ADDBA request */
	struct ifqueue	txa_q;		/* packet queue */
	struct callout	txa_timer;
};

/* return non-zero if AMPDU tx for the TID is running */
#define	IEEE80211_AMPDU_RUNNING(tap) \
	(((tap)->txa_flags & IEEE80211_AGGR_RUNNING) != 0)

/* return non-zero if AMPDU tx for the TID is running or started */
#define	IEEE80211_AMPDU_REQUESTED(tap) \
	(((tap)->txa_flags & \
	 (IEEE80211_AGGR_RUNNING|IEEE80211_AGGR_XCHGPEND)) != 0)

struct ieee80211_rx_ampdu {
	int		rxa_flags;
	int		rxa_qbytes;	/* data queued (bytes) */
	short		rxa_qframes;	/* data queued (frames) */
	ieee80211_seq	rxa_seqstart;
	ieee80211_seq	rxa_start;	/* start of current BA window */
	ieee80211_seq	rxa_nxt;	/* next seq# in BA window */
	uint16_t	rxa_wnd;	/* BA window size */
	struct mbuf *rxa_m[IEEE80211_AGGR_BAWMAX];
};

void	ieee80211_ht_attach(struct ieee80211com *);
void	ieee80211_ht_detach(struct ieee80211com *);

void	ieee80211_ht_announce(struct ieee80211com *);

extern const int ieee80211_htrates[16];
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
void	ieee80211_parse_htcap(struct ieee80211_node *, const uint8_t *);
void	ieee80211_parse_htinfo(struct ieee80211_node *, const uint8_t *);
void	ieee80211_recv_action(struct ieee80211_node *,
		const uint8_t *, const uint8_t *);
int	ieee80211_ampdu_request(struct ieee80211_node *,
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
void	ieee80211_ht_update_beacon(struct ieee80211com *,
		struct ieee80211_beacon_offsets *);
#endif /* _NET80211_IEEE80211_HT_H_ */
