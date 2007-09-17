/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002-2007 Sam Leffler, Errno Consulting
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
#ifndef _NET80211_IEEE80211_NODE_H_
#define _NET80211_IEEE80211_NODE_H_

#include <net80211/ieee80211_ioctl.h>		/* for ieee80211_nodestats */
#include <net80211/ieee80211_ht.h>		/* for aggregation state */

/*
 * Each ieee80211com instance has a single timer that fires once a
 * second.  This is used to initiate various work depending on the
 * state of the instance: scanning (passive or active), ``transition''
 * (waiting for a response to a management frame when operating
 * as a station), and node inactivity processing (when operating
 * as an AP).  For inactivity processing each node has a timeout
 * set in it's ni_inact field that is decremented on each timeout
 * and the node is reclaimed when the counter goes to zero.  We
 * use different inactivity timeout values depending on whether
 * the node is associated and authorized (either by 802.1x or
 * open/shared key authentication) or associated but yet to be
 * authorized.  The latter timeout is shorter to more aggressively
 * reclaim nodes that leave part way through the 802.1x exchange.
 */
#define	IEEE80211_INACT_WAIT	15		/* inactivity interval (secs) */
#define	IEEE80211_INACT_INIT	(30/IEEE80211_INACT_WAIT)	/* initial */
#define	IEEE80211_INACT_AUTH	(180/IEEE80211_INACT_WAIT)	/* associated but not authorized */
#define	IEEE80211_INACT_RUN	(300/IEEE80211_INACT_WAIT)	/* authorized */
#define	IEEE80211_INACT_PROBE	(30/IEEE80211_INACT_WAIT)	/* probe */
#define	IEEE80211_INACT_SCAN	(300/IEEE80211_INACT_WAIT)	/* scanned */

#define	IEEE80211_TRANS_WAIT 	2		/* mgt frame tx timer (secs) */

/* threshold for aging overlapping non-ERP bss */
#define	IEEE80211_NONERP_PRESENT_AGE	msecs_to_ticks(60*1000)

#define	IEEE80211_NODE_HASHSIZE	32
/* simple hash is enough for variation of macaddr */
#define	IEEE80211_NODE_HASH(addr)	\
	(((const uint8_t *)(addr))[IEEE80211_ADDR_LEN - 1] % \
		IEEE80211_NODE_HASHSIZE)

struct ieee80211_rsnparms {
	uint8_t		rsn_mcastcipher;	/* mcast/group cipher */
	uint8_t		rsn_mcastkeylen;	/* mcast key length */
	uint8_t		rsn_ucastcipherset;	/* unicast cipher set */
	uint8_t		rsn_ucastcipher;	/* selected unicast cipher */
	uint8_t		rsn_ucastkeylen;	/* unicast key length */
	uint8_t		rsn_keymgmtset;		/* key mangement algorithms */
	uint8_t		rsn_keymgmt;		/* selected key mgmt algo */
	uint16_t	rsn_caps;		/* capabilities */
};

struct ieee80211_node_table;
struct ieee80211com;

/*
 * Node specific information.  Note that drivers are expected
 * to derive from this structure to add device-specific per-node
 * state.  This is done by overriding the ic_node_* methods in
 * the ieee80211com structure.
 */
struct ieee80211_node {
	struct ieee80211com	*ni_ic;
	struct ieee80211_node_table *ni_table;
	TAILQ_ENTRY(ieee80211_node)	ni_list;
	LIST_ENTRY(ieee80211_node)	ni_hash;
	u_int			ni_refcnt;
	u_int			ni_scangen;	/* gen# for timeout scan */
	uint8_t			ni_authmode;	/* authentication algorithm */
	uint8_t			ni_ath_flags;	/* Atheros feature flags */
	/* NB: These must have the same values as IEEE80211_ATHC_* */
#define IEEE80211_NODE_TURBOP	0x0001		/* Turbo prime enable */
#define IEEE80211_NODE_COMP	0x0002		/* Compresssion enable */
#define IEEE80211_NODE_FF	0x0004          /* Fast Frame capable */
#define IEEE80211_NODE_XR	0x0008		/* Atheros WME enable */
#define IEEE80211_NODE_AR	0x0010		/* AR capable */
#define IEEE80211_NODE_BOOST	0x0080 
#define IEEE80211_NODE_PSUPDATE	0x0200		/* power save state changed */ 
#define	IEEE80211_NODE_CHWUPDATE 0x0400		/* 11n channel width change */
	uint16_t		ni_flags;	/* special-purpose state */
#define	IEEE80211_NODE_AUTH	0x0001		/* authorized for data */
#define	IEEE80211_NODE_QOS	0x0002		/* QoS enabled */
#define	IEEE80211_NODE_ERP	0x0004		/* ERP enabled */
/* NB: this must have the same value as IEEE80211_FC1_PWR_MGT */
#define	IEEE80211_NODE_PWR_MGT	0x0010		/* power save mode enabled */
#define	IEEE80211_NODE_AREF	0x0020		/* authentication ref held */
#define	IEEE80211_NODE_HT	0x0040		/* HT enabled */
#define	IEEE80211_NODE_HTCOMPAT	0x0080		/* HT setup w/ vendor OUI's */
	uint16_t		ni_ath_defkeyix;/* Atheros def key index */
	uint16_t		ni_associd;	/* assoc response */
	uint16_t		ni_txpower;	/* current transmit power */
	uint16_t		ni_vlan;	/* vlan tag */
	uint32_t		*ni_challenge;	/* shared-key challenge */
	uint8_t			*ni_wpa_ie;	/* captured WPA ie */
	uint8_t			*ni_rsn_ie;	/* captured RSN ie */
	uint8_t			*ni_wme_ie;	/* captured WME ie */
	uint8_t			*ni_ath_ie;	/* captured Atheros ie */
#define	IEEE80211_NONQOS_TID	16		/* index for non-QoS sta */
	uint16_t		ni_txseqs[17];	/* tx seq per-tid */
	uint16_t		ni_rxseqs[17];	/* rx seq previous per-tid*/
	uint32_t		ni_rxfragstamp;	/* time stamp of last rx frag */
	struct mbuf		*ni_rxfrag[3];	/* rx frag reassembly */
	struct ieee80211_rsnparms ni_rsn;	/* RSN/WPA parameters */
	struct ieee80211_key	ni_ucastkey;	/* unicast key */

	/* hardware */
	uint32_t		ni_rstamp;	/* recv timestamp */
	int8_t			ni_rssi;	/* recv ssi */
	int8_t			ni_noise;	/* noise floor */

	/* header */
	uint8_t			ni_macaddr[IEEE80211_ADDR_LEN];
	uint8_t			ni_bssid[IEEE80211_ADDR_LEN];

	/* beacon, probe response */
	union {
		uint8_t		data[8];
		uint64_t	tsf;
	} ni_tstamp;				/* from last rcv'd beacon */
	uint16_t		ni_intval;	/* beacon interval */
	uint16_t		ni_capinfo;	/* capabilities */
	uint8_t			ni_esslen;
	uint8_t			ni_essid[IEEE80211_NWID_LEN];
	struct ieee80211_rateset ni_rates;	/* negotiated rate set */
	struct ieee80211_channel *ni_chan;
	uint16_t		ni_fhdwell;	/* FH only */
	uint8_t			ni_fhindex;	/* FH only */
	uint8_t			ni_erp;		/* ERP from beacon/probe resp */
	uint16_t		ni_timoff;	/* byte offset to TIM ie */
	uint8_t			ni_dtim_period;	/* DTIM period */
	uint8_t			ni_dtim_count;	/* DTIM count for last bcn */

	/* 11n state */
	uint16_t		ni_htcap;	/* HT capabilities */
	uint8_t			ni_htparam;	/* HT params */
	uint8_t			ni_htctlchan;	/* HT control channel */
	uint8_t			ni_ht2ndchan;	/* HT 2nd channel */
	uint8_t			ni_htopmode;	/* HT operating mode */
	uint8_t			ni_htstbc;	/* HT */
	uint8_t			ni_reqcw;	/* requested tx channel width */
	uint8_t			ni_chw;		/* negotiated channel width */
	struct ieee80211_htrateset ni_htrates;	/* negotiated ht rate set */
	struct ieee80211_tx_ampdu ni_tx_ampdu[WME_NUM_AC];
	struct ieee80211_rx_ampdu ni_rx_ampdu[WME_NUM_TID];

	/* others */
	int			ni_fails;	/* failure count to associate */
	short			ni_inact;	/* inactivity mark count */
	short			ni_inact_reload;/* inactivity reload value */
	int			ni_txrate;	/* index to ni_rates[] */
	struct	ifqueue		ni_savedq;	/* ps-poll queue */
	struct ieee80211_nodestats ni_stats;	/* per-node statistics */
};
MALLOC_DECLARE(M_80211_NODE);

#define	IEEE80211_NODE_ATH	(IEEE80211_NODE_FF | IEEE80211_NODE_TURBOP)

#define	IEEE80211_NODE_AID(ni)	IEEE80211_AID(ni->ni_associd)

#define	IEEE80211_NODE_STAT(ni,stat)	(ni->ni_stats.ns_##stat++)
#define	IEEE80211_NODE_STAT_ADD(ni,stat,v)	(ni->ni_stats.ns_##stat += v)
#define	IEEE80211_NODE_STAT_SET(ni,stat,v)	(ni->ni_stats.ns_##stat = v)

static __inline struct ieee80211_node *
ieee80211_ref_node(struct ieee80211_node *ni)
{
	ieee80211_node_incref(ni);
	return ni;
}

static __inline void
ieee80211_unref_node(struct ieee80211_node **ni)
{
	ieee80211_node_decref(*ni);
	*ni = NULL;			/* guard against use */
}

struct ieee80211com;

void	ieee80211_node_attach(struct ieee80211com *);
void	ieee80211_node_lateattach(struct ieee80211com *);
void	ieee80211_node_detach(struct ieee80211com *);

static __inline int
ieee80211_node_is_authorized(const struct ieee80211_node *ni)
{
	return (ni->ni_flags & IEEE80211_NODE_AUTH);
}

void	ieee80211_node_authorize(struct ieee80211_node *);
void	ieee80211_node_unauthorize(struct ieee80211_node *);

void	ieee80211_probe_curchan(struct ieee80211com *, int);
void	ieee80211_create_ibss(struct ieee80211com*, struct ieee80211_channel *);
void	ieee80211_reset_bss(struct ieee80211com *);
int	ieee80211_ibss_merge(struct ieee80211_node *);
struct ieee80211_scan_entry;
int	ieee80211_sta_join(struct ieee80211com *,
		const struct ieee80211_scan_entry *);
void	ieee80211_sta_leave(struct ieee80211com *, struct ieee80211_node *);

/*
 * Table of ieee80211_node instances.  Each ieee80211com
 * has at least one for holding the scan candidates.
 * When operating as an access point or in ibss mode there
 * is a second table for associated stations or neighbors.
 */
struct ieee80211_node_table {
	struct ieee80211com	*nt_ic;		/* back reference */
	ieee80211_node_lock_t	nt_nodelock;	/* on node table */
	TAILQ_HEAD(, ieee80211_node) nt_node;	/* information of all nodes */
	LIST_HEAD(, ieee80211_node) nt_hash[IEEE80211_NODE_HASHSIZE];
	struct ieee80211_node	**nt_keyixmap;	/* key ix -> node map */
	int			nt_keyixmax;	/* keyixmap size */
	const char		*nt_name;	/* for debugging */
	ieee80211_scan_lock_t	nt_scanlock;	/* on nt_scangen */
	u_int			nt_scangen;	/* gen# for timeout scan */
	int			nt_inact_init;	/* initial node inact setting */
};

struct ieee80211_node *ieee80211_alloc_node(
		struct ieee80211_node_table *, const uint8_t *);
struct ieee80211_node *ieee80211_tmp_node(struct ieee80211com *,
		const uint8_t *macaddr);
struct ieee80211_node *ieee80211_dup_bss(struct ieee80211_node_table *,
		const uint8_t *);
#ifdef IEEE80211_DEBUG_REFCNT
void	ieee80211_free_node_debug(struct ieee80211_node *,
		const char *func, int line);
struct ieee80211_node *ieee80211_find_node_debug(struct ieee80211_node_table *,
		const uint8_t *,
		const char *func, int line);
struct ieee80211_node * ieee80211_find_rxnode_debug(struct ieee80211com *,
		const struct ieee80211_frame_min *,
		const char *func, int line);
struct ieee80211_node * ieee80211_find_rxnode_withkey_debug(
		struct ieee80211com *,
		const struct ieee80211_frame_min *, uint16_t keyix,
		const char *func, int line);
struct ieee80211_node * ieee80211_find_rxnode_withkey_debug(
		struct ieee80211com *,
		const struct ieee80211_frame_min *, uint16_t keyix,
		const char *func, int line);
struct ieee80211_node *ieee80211_find_txnode_debug(struct ieee80211com *,
		const uint8_t *,
		const char *func, int line);
struct ieee80211_node *ieee80211_find_node_with_ssid_debug(
		struct ieee80211_node_table *, const uint8_t *macaddr,
		u_int ssidlen, const uint8_t *ssid,
		const char *func, int line);
#define	ieee80211_free_node(ni) \
	ieee80211_free_node_debug(ni, __func__, __LINE__)
#define	ieee80211_find_node(nt, mac) \
	ieee80211_find_node_debug(nt, mac, __func__, __LINE__)
#define	ieee80211_find_rxnode(nt, wh) \
	ieee80211_find_rxnode_debug(nt, wh, __func__, __LINE__)
#define	ieee80211_find_rxnode_withkey(nt, wh, keyix) \
	ieee80211_find_rxnode_withkey_debug(nt, wh, keyix, __func__, __LINE__)
#define	ieee80211_find_txnode(nt, mac) \
	ieee80211_find_txnode_debug(nt, mac, __func__, __LINE__)
#define	ieee80211_find_node_with_ssid(nt, mac, sl, ss) \
	ieee80211_find_node_with_ssid_debug(nt, mac, sl, ss, __func__, __LINE__)
#else
void	ieee80211_free_node(struct ieee80211_node *);
struct ieee80211_node *ieee80211_find_node(struct ieee80211_node_table *,
		const uint8_t *);
struct ieee80211_node * ieee80211_find_rxnode(struct ieee80211com *,
		const struct ieee80211_frame_min *);
struct ieee80211_node * ieee80211_find_rxnode_withkey(struct ieee80211com *,
		const struct ieee80211_frame_min *, uint16_t keyix);
struct ieee80211_node *ieee80211_find_txnode(struct ieee80211com *,
		const uint8_t *);
struct ieee80211_node *ieee80211_find_node_with_ssid(
		struct ieee80211_node_table *, const uint8_t *macaddr,
		u_int ssidlen, const uint8_t *ssid);
#endif
int	ieee80211_node_delucastkey(struct ieee80211_node *);
void	ieee80211_node_timeout(void *arg);

typedef void ieee80211_iter_func(void *, struct ieee80211_node *);
void	ieee80211_iterate_nodes(struct ieee80211_node_table *,
		ieee80211_iter_func *, void *);

void	ieee80211_dump_node(struct ieee80211_node_table *,
		struct ieee80211_node *);
void	ieee80211_dump_nodes(struct ieee80211_node_table *);

void	ieee80211_notify_erp(struct ieee80211com *);

struct ieee80211_node *ieee80211_fakeup_adhoc_node(
		struct ieee80211_node_table *, const uint8_t macaddr[]);
struct ieee80211_scanparams;
void	ieee80211_init_neighbor(struct ieee80211_node *,
		const struct ieee80211_frame *,
		const struct ieee80211_scanparams *);
struct ieee80211_node *ieee80211_add_neighbor(struct ieee80211com *,
		const struct ieee80211_frame *,
		const struct ieee80211_scanparams *);
void	ieee80211_node_join(struct ieee80211com *, struct ieee80211_node *,int);
void	ieee80211_node_leave(struct ieee80211com *, struct ieee80211_node *);
int8_t	ieee80211_getrssi(struct ieee80211com *);
void	ieee80211_getsignal(struct ieee80211com *, int8_t *, int8_t *);
#endif /* _NET80211_IEEE80211_NODE_H_ */
