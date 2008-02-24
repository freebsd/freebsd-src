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
 * $FreeBSD: src/sys/net80211/ieee80211_var.h,v 1.52.2.2 2007/11/29 21:23:22 sam Exp $
 */
#ifndef _NET80211_IEEE80211_VAR_H_
#define _NET80211_IEEE80211_VAR_H_

/*
 * Definitions for IEEE 802.11 drivers.
 */
#define	IEEE80211_DEBUG
#undef	IEEE80211_DEBUG_REFCNT			/* node refcnt stuff */

/* NB: portability glue must go first */
#ifdef __NetBSD__
#include <net80211/ieee80211_netbsd.h>
#elif __FreeBSD__
#include <net80211/ieee80211_freebsd.h>
#elif __linux__
#include <net80211/ieee80211_linux.h>
#else
#error	"No support for your operating system!"
#endif

#include <net80211/_ieee80211.h>
#include <net80211/ieee80211.h>
#include <net80211/ieee80211_crypto.h>
#include <net80211/ieee80211_ioctl.h>		/* for ieee80211_stats */
#include <net80211/ieee80211_node.h>
#include <net80211/ieee80211_power.h>
#include <net80211/ieee80211_proto.h>
#include <net80211/ieee80211_scan.h>

#define	IEEE80211_TXPOWER_MAX	100	/* .5 dbM (XXX units?) */
#define	IEEE80211_TXPOWER_MIN	0	/* kill radio */

#define	IEEE80211_DTIM_DEFAULT	1	/* default DTIM period */
#define	IEEE80211_BINTVAL_DEFAULT 100	/* default beacon interval (TU's) */

#define	IEEE80211_BMISS_MAX	2	/* maximum consecutive bmiss allowed */
#define	IEEE80211_HWBMISS_DEFAULT 7	/* h/w bmiss threshold (beacons) */

#define	IEEE80211_BGSCAN_INTVAL_MIN	15	/* min bg scan intvl (secs) */
#define	IEEE80211_BGSCAN_INTVAL_DEFAULT	(5*60)	/* default bg scan intvl */

#define	IEEE80211_BGSCAN_IDLE_MIN	100	/* min idle time (ms) */
#define	IEEE80211_BGSCAN_IDLE_DEFAULT	250	/* default idle time (ms) */

#define	IEEE80211_SCAN_VALID_MIN	10	/* min scan valid time (secs) */
#define	IEEE80211_SCAN_VALID_DEFAULT	60	/* default scan valid time */

#define	IEEE80211_PS_SLEEP	0x1	/* STA is in power saving mode */
#define	IEEE80211_PS_MAX_QUEUE	50	/* maximum saved packets */

#define	IEEE80211_FIXED_RATE_NONE	-1
#define	IEEE80211_MCAST_RATE_DEFAULT	(2*1)	/* default mcast rate (1M) */

#define	IEEE80211_RTS_DEFAULT		IEEE80211_RTS_MAX
#define	IEEE80211_FRAG_DEFAULT		IEEE80211_FRAG_MAX

#define	IEEE80211_MS_TO_TU(x)	(((x) * 1000) / 1024)
#define	IEEE80211_TU_TO_MS(x)	(((x) * 1024) / 1000)
#define	IEEE80211_TU_TO_TICKS(x)(((x) * 1024 * hz) / (1000 * 1000))

struct ieee80211_aclator;
struct sysctl_ctx_list;

struct ieee80211com {
	SLIST_ENTRY(ieee80211com) ic_next;
	struct ifnet		*ic_ifp;	/* associated device */
	ieee80211_com_lock_t	ic_comlock;	/* state update lock */
	ieee80211_beacon_lock_t	ic_beaconlock;	/* beacon update lock */
	struct ieee80211_stats	ic_stats;	/* statistics */
	struct sysctl_ctx_list	*ic_sysctl;	/* dynamic sysctl context */
	uint32_t		ic_debug;	/* debug msg flags */
	int			ic_vap;		/* virtual AP index */
	int			ic_headroom;	/* driver tx headroom needs */
	enum ieee80211_phytype	ic_phytype;	/* XXX wrong for multi-mode */
	enum ieee80211_opmode	ic_opmode;	/* operation mode */
	struct ifmedia		ic_media;	/* interface media config */
	uint8_t			ic_myaddr[IEEE80211_ADDR_LEN];

	uint32_t		ic_flags;	/* state flags */
	uint32_t		ic_flags_ext;	/* extended state flags */
	uint32_t		ic_flags_ven;	/* vendor state flags */
	uint32_t		ic_caps;	/* capabilities */
	uint32_t		ic_htcaps;	/* HT capabilities */
	uint8_t			ic_modecaps[2];	/* set of mode capabilities */
	uint16_t		ic_curmode;	/* current mode */
	struct ieee80211_rateset ic_sup_rates[IEEE80211_MODE_MAX];
	uint16_t		ic_bintval;	/* beacon interval */
	uint16_t		ic_lintval;	/* listen interval */
	uint16_t		ic_holdover;	/* PM hold over duration */
	uint16_t		ic_txpowlimit;	/* global tx power limit */
	int			ic_ampdu_rxmax;	/* A-MPDU rx limit (bytes) */
	int			ic_ampdu_density;/* A-MPDU density */
	int			ic_ampdu_limit;	/* A-MPDU tx limit (bytes) */
	int			ic_amsdu_limit;	/* A-MSDU tx limit (bytes) */

	/*
	 * Channel state:
	 *
	 * ic_channels is the set of available channels for the device;
	 *    it is setup by the driver
	 * ic_nchans is the number of valid entries in ic_channels
	 * ic_chan_avail is a bit vector of these channels used to check
	 *    whether a channel is available w/o searching the channel table.
	 * ic_chan_active is a (potentially) constrained subset of
	 *    ic_chan_avail that reflects any mode setting or user-specified
	 *    limit on the set of channels to use/scan
	 * ic_curchan is the current channel the device is set to; it may
	 *    be different from ic_bsschan when we are off-channel scanning
	 *    or otherwise doing background work
	 * ic_bsschan is the channel selected for operation; it may
	 *    be undefined (IEEE80211_CHAN_ANYC)
	 * ic_prevchan is a cached ``previous channel'' used to optimize
	 *    lookups when switching back+forth between two channels
	 *    (e.g. for dynamic turbo)
	 */
	int			ic_nchans;	/* # entries in ic_channels */
	struct ieee80211_channel ic_channels[IEEE80211_CHAN_MAX+1];
	uint8_t			ic_chan_avail[IEEE80211_CHAN_BYTES];
	uint8_t			ic_chan_active[IEEE80211_CHAN_BYTES];
	uint8_t			ic_chan_scan[IEEE80211_CHAN_BYTES];
	struct ieee80211_channel *ic_curchan;	/* current channel */
	struct ieee80211_channel *ic_bsschan;	/* bss channel */
	struct ieee80211_channel *ic_prevchan;	/* previous channel */
	int			ic_countrycode;	/* ISO country code */
	uint16_t		ic_regdomain;	/* regulatory domain */
	uint8_t			ic_location;	/* unknown, indoor, outdoor */

	struct ieee80211_scan_state *ic_scan;	/* scan state */
	enum ieee80211_roamingmode ic_roaming;	/* roaming mode */
	int			ic_lastdata;	/* time of last data frame */
	int			ic_lastscan;	/* time last scan completed */
	int			ic_des_nssid;	/* # desired ssids */
	struct ieee80211_scan_ssid ic_des_ssid[1];/* desired ssid table */
	uint8_t			ic_des_bssid[IEEE80211_ADDR_LEN];
	struct ieee80211_channel *ic_des_chan;	/* desired channel */
	int			ic_des_mode;	/* desired phymode */
	u_int			ic_bgscanidle;	/* bg scan idle threshold */
	u_int			ic_bgscanintvl;	/* bg scan min interval */
	u_int			ic_scanvalid;	/* scan cache valid threshold */
	struct ieee80211_roam	ic_roam;	/* sta-mode roaming state */

	struct ieee80211_node_table ic_sta;	/* stations/neighbors */

	struct ieee80211_wme_state ic_wme;	/* WME/WMM state */
	const struct ieee80211_aclator *ic_acl;	/* aclator glue */
	void			*ic_as;		/* private aclator state */

	enum ieee80211_protmode	ic_protmode;	/* 802.11g protection mode */
	uint16_t		ic_nonerpsta;	/* # non-ERP stations */
	uint16_t		ic_longslotsta;	/* # long slot time stations */
	uint16_t		ic_sta_assoc;	/* stations associated */
	uint16_t		ic_ht_sta_assoc;/* HT stations associated */
	uint16_t		ic_ht40_sta_assoc;/* HT40 stations associated */
	uint8_t			ic_curhtprotmode;/* HTINFO bss state */
	enum ieee80211_protmode	ic_htprotmode;	/* HT protection mode */
	int			ic_lastnonerp;	/* last time non-ERP sta noted*/
	int			ic_lastnonht;	/* last time non-HT sta noted */

	struct ifqueue		ic_mgtq;
	enum ieee80211_state	ic_state;	/* 802.11 state */
	struct callout		ic_mgtsend;	/* mgmt frame response timer */
	uint32_t		*ic_aid_bitmap;	/* association id map */
	uint16_t		ic_max_aid;
	uint16_t		ic_ps_sta;	/* stations in power save */
	uint16_t		ic_ps_pending;	/* ps sta's w/ pending frames */
	uint8_t			*ic_tim_bitmap;	/* power-save stations w/ data*/
	uint16_t		ic_tim_len;	/* ic_tim_bitmap size (bytes) */
	uint8_t			ic_dtim_period;	/* DTIM period */
	uint8_t			ic_dtim_count;	/* DTIM count for last bcn */
	struct bpf_if		*ic_rawbpf;	/* packet filter structure */
	struct ieee80211_node	*ic_bss;	/* information for this node */
	int			ic_fixed_rate;	/* 802.11 rate or -1 */
	int			ic_mcast_rate;	/* rate for mcast frames */
	uint16_t		ic_rtsthreshold;
	uint16_t		ic_fragthreshold;
	uint8_t			ic_bmissthreshold;
	uint8_t			ic_bmiss_count;	/* current beacon miss count */
	int			ic_bmiss_max;	/* max bmiss before scan */
	uint16_t		ic_swbmiss_count;/* beacons in last period */
	uint16_t		ic_swbmiss_period;/* s/w bmiss period */
	struct callout		ic_swbmiss;	/* s/w beacon miss timer */

	uint16_t		ic_txmin;	/* min tx retry count */
	uint16_t		ic_txmax;	/* max tx retry count */
	uint16_t		ic_txlifetime;	/* tx lifetime */
	struct callout		ic_inact;	/* inactivity timer wait */
	void			*ic_opt_ie;	/* user-specified IE's */
	uint16_t		ic_opt_ie_len;	/* length of ni_opt_ie */
	int			ic_inact_init;	/* initial setting */
	int			ic_inact_auth;	/* auth but not assoc setting */
	int			ic_inact_run;	/* authorized setting */
	int			ic_inact_probe;	/* inactive probe time */

	/*
	 * Cipher state/configuration.
	 */
	struct ieee80211_crypto_state ic_crypto;
#define	ic_nw_keys	ic_crypto.cs_nw_keys	/* XXX compatibility */
#define	ic_def_txkey	ic_crypto.cs_def_txkey	/* XXX compatibility */
	/*
	 * 802.1x glue.  When an authenticator attaches it
	 * fills in this section.  We assume that when ic_ec
	 * is setup that the methods are safe to call.
	 */
	const struct ieee80211_authenticator *ic_auth;
	struct eapolcom		*ic_ec;	

	/* send/recv 802.11 management frame */
	int			(*ic_send_mgmt)(struct ieee80211com *,
				    struct ieee80211_node *, int, int);
	void			(*ic_recv_mgmt)(struct ieee80211com *,
				    struct mbuf *, struct ieee80211_node *,
				    int, int, int, uint32_t);
	/* send raw 802.11 frame */
	int			(*ic_raw_xmit)(struct ieee80211_node *,
				    struct mbuf *,
				    const struct ieee80211_bpf_params *);
	/* reset device state after 802.11 parameter/state change */
	int			(*ic_reset)(struct ifnet *);
	/* [schedule] beacon frame update */
	void			(*ic_update_beacon)(struct ieee80211com *, int);
	/* update device state for 802.11 slot time change */
	void			(*ic_updateslot)(struct ifnet *);
	/* new station association callback/notification */
	void			(*ic_newassoc)(struct ieee80211_node *, int);
	/* node state management */
	struct ieee80211_node	*(*ic_node_alloc)(struct ieee80211_node_table*);
	void			(*ic_node_free)(struct ieee80211_node *);
	void			(*ic_node_cleanup)(struct ieee80211_node *);
	int8_t			(*ic_node_getrssi)(const struct ieee80211_node*);
	void			(*ic_node_getsignal)(const struct ieee80211_node*,
				    int8_t *, int8_t *);
	/* scanning support */
	void			(*ic_scan_start)(struct ieee80211com *);
	void			(*ic_scan_end)(struct ieee80211com *);
	void			(*ic_set_channel)(struct ieee80211com *);
	void			(*ic_scan_curchan)(struct ieee80211com *,
				    unsigned long);
	void			(*ic_scan_mindwell)(struct ieee80211com *);
	/* per-vap eventually... */
	int			(*ic_newstate)(struct ieee80211com *,
				    enum ieee80211_state, int);
	void			(*ic_set_tim)(struct ieee80211_node *, int);

	/*
	 * 802.11n ADDBA support.  A simple/generic implementation
	 * of A-MPDU tx aggregation is provided; the driver may
	 * override these methods to provide their own support.
	 * A-MPDU rx re-ordering happens automatically if the
	 * driver passes out-of-order frames to ieee80211_input
	 * from an assocated HT station.
	 */
	void			(*ic_recv_action)(struct ieee80211_node *,
				    const uint8_t *frm, const uint8_t *efrm);
	int			(*ic_send_action)(struct ieee80211_node *,
				    int category, int action,
				    uint16_t args[4]);
	/* start/stop doing A-MPDU tx aggregation for a station */
	int			(*ic_addba_request)(struct ieee80211_node *,
				    struct ieee80211_tx_ampdu *,
				    int dialogtoken, int baparamset,
				    int batimeout);
	int			(*ic_addba_response)(struct ieee80211_node *,
				    struct ieee80211_tx_ampdu *,
				    int status, int baparamset, int batimeout);
	void			(*ic_addba_stop)(struct ieee80211_node *,
				    struct ieee80211_tx_ampdu *);
};

#define	IEEE80211_ADDR_EQ(a1,a2)	(memcmp(a1,a2,IEEE80211_ADDR_LEN) == 0)
#define	IEEE80211_ADDR_COPY(dst,src)	memcpy(dst,src,IEEE80211_ADDR_LEN)

/* ic_flags */
/* NB: bits 0x4c available */
#define	IEEE80211_F_TURBOP	0x00000001	/* CONF: ATH Turbo enabled*/
#define	IEEE80211_F_COMP	0x00000002	/* CONF: ATH comp enabled */
#define	IEEE80211_F_FF		0x00000004	/* CONF: ATH FF enabled */
#define	IEEE80211_F_BURST	0x00000008	/* CONF: bursting enabled */
/* NB: this is intentionally setup to be IEEE80211_CAPINFO_PRIVACY */
#define	IEEE80211_F_PRIVACY	0x00000010	/* CONF: privacy enabled */
#define	IEEE80211_F_PUREG	0x00000020	/* CONF: 11g w/o 11b sta's */
#define	IEEE80211_F_SCAN	0x00000080	/* STATUS: scanning */
#define	IEEE80211_F_ASCAN	0x00000100	/* STATUS: active scan */
#define	IEEE80211_F_SIBSS	0x00000200	/* STATUS: start IBSS */
/* NB: this is intentionally setup to be IEEE80211_CAPINFO_SHORT_SLOTTIME */
#define	IEEE80211_F_SHSLOT	0x00000400	/* STATUS: use short slot time*/
#define	IEEE80211_F_PMGTON	0x00000800	/* CONF: Power mgmt enable */
#define	IEEE80211_F_DESBSSID	0x00001000	/* CONF: des_bssid is set */
#define	IEEE80211_F_WME		0x00002000	/* CONF: enable WME use */
#define	IEEE80211_F_BGSCAN	0x00004000	/* CONF: bg scan enabled (???)*/
#define	IEEE80211_F_SWRETRY	0x00008000	/* CONF: sw tx retry enabled */
#define IEEE80211_F_TXPOW_FIXED	0x00010000	/* TX Power: fixed rate */
#define	IEEE80211_F_IBSSON	0x00020000	/* CONF: IBSS creation enable */
#define	IEEE80211_F_SHPREAMBLE	0x00040000	/* STATUS: use short preamble */
#define	IEEE80211_F_DATAPAD	0x00080000	/* CONF: do alignment pad */
#define	IEEE80211_F_USEPROT	0x00100000	/* STATUS: protection enabled */
#define	IEEE80211_F_USEBARKER	0x00200000	/* STATUS: use barker preamble*/
#define	IEEE80211_F_WPA1	0x00800000	/* CONF: WPA enabled */
#define	IEEE80211_F_WPA2	0x01000000	/* CONF: WPA2 enabled */
#define	IEEE80211_F_WPA		0x01800000	/* CONF: WPA/WPA2 enabled */
#define	IEEE80211_F_DROPUNENC	0x02000000	/* CONF: drop unencrypted */
#define	IEEE80211_F_COUNTERM	0x04000000	/* CONF: TKIP countermeasures */
#define	IEEE80211_F_HIDESSID	0x08000000	/* CONF: hide SSID in beacon */
#define	IEEE80211_F_NOBRIDGE	0x10000000	/* CONF: dis. internal bridge */
#define	IEEE80211_F_DOTH	0x40000000	/* CONF: 11h enabled */

/* Atheros protocol-specific flags */
#define	IEEE80211_F_ATHEROS \
	(IEEE80211_F_FF | IEEE80211_F_COMP | IEEE80211_F_TURBOP)
/* Check if an Atheros capability was negotiated for use */
#define	IEEE80211_ATH_CAP(ic, ni, bit) \
	((ic)->ic_flags & (ni)->ni_ath_flags & (bit))

/* ic_flags_ext */
#define	IEEE80211_FEXT_NONHT_PR	 0x00000001	/* STATUS: non-HT sta present */
#define	IEEE80211_FEXT_INACT	 0x00000002	/* CONF: sta inact handling */
/* 0x00000006 reserved */
#define	IEEE80211_FEXT_BGSCAN	 0x00000008	/* STATUS: complete bgscan */
#define	IEEE80211_FEXT_NONERP_PR 0x00000200	/* STATUS: non-ERP sta present*/
#define	IEEE80211_FEXT_SWBMISS	 0x00000400	/* CONF: do bmiss in s/w */
#define	IEEE80211_FEXT_PROBECHAN 0x00020000	/* CONF: probe passive channel*/
#define	IEEE80211_FEXT_HT	 0x00080000	/* CONF: HT supported */
#define	IEEE80211_FEXT_AMPDU_TX	 0x00100000	/* CONF: A-MPDU tx supported */
#define	IEEE80211_FEXT_AMPDU_RX	 0x00200000	/* CONF: A-MPDU tx supported */
#define	IEEE80211_FEXT_AMSDU_TX	 0x00400000	/* CONF: A-MSDU tx supported */
#define	IEEE80211_FEXT_AMSDU_RX	 0x00800000	/* CONF: A-MSDU tx supported */
#define	IEEE80211_FEXT_USEHT40	 0x01000000	/* CONF: 20/40 use enabled */
#define	IEEE80211_FEXT_PUREN	 0x02000000	/* CONF: 11n w/o legacy sta's */
#define	IEEE80211_FEXT_SHORTGI20 0x04000000	/* CONF: short GI in HT20 */
#define	IEEE80211_FEXT_SHORTGI40 0x08000000	/* CONF: short GI in HT40 */
#define	IEEE80211_FEXT_HTCOMPAT  0x10000000	/* CONF: HT vendor OUI's */

/* ic_caps */
#define	IEEE80211_C_WEP		0x00000001	/* CAPABILITY: WEP available */
#define	IEEE80211_C_TKIP	0x00000002	/* CAPABILITY: TKIP available */
#define	IEEE80211_C_AES		0x00000004	/* CAPABILITY: AES OCB avail */
#define	IEEE80211_C_AES_CCM	0x00000008	/* CAPABILITY: AES CCM avail */
#define	IEEE80211_C_CKIP	0x00000020	/* CAPABILITY: CKIP available */
#define	IEEE80211_C_FF		0x00000040	/* CAPABILITY: ATH FF avail */
#define	IEEE80211_C_TURBOP	0x00000080	/* CAPABILITY: ATH Turbo avail*/
#define	IEEE80211_C_IBSS	0x00000100	/* CAPABILITY: IBSS available */
#define	IEEE80211_C_PMGT	0x00000200	/* CAPABILITY: Power mgmt */
#define	IEEE80211_C_HOSTAP	0x00000400	/* CAPABILITY: HOSTAP avail */
#define	IEEE80211_C_AHDEMO	0x00000800	/* CAPABILITY: Old Adhoc Demo */
#define	IEEE80211_C_SWRETRY	0x00001000	/* CAPABILITY: sw tx retry */
#define	IEEE80211_C_TXPMGT	0x00002000	/* CAPABILITY: tx power mgmt */
#define	IEEE80211_C_SHSLOT	0x00004000	/* CAPABILITY: short slottime */
#define	IEEE80211_C_SHPREAMBLE	0x00008000	/* CAPABILITY: short preamble */
#define	IEEE80211_C_MONITOR	0x00010000	/* CAPABILITY: monitor mode */
#define	IEEE80211_C_TKIPMIC	0x00020000	/* CAPABILITY: TKIP MIC avail */
#define	IEEE80211_C_WPA1	0x00800000	/* CAPABILITY: WPA1 avail */
#define	IEEE80211_C_WPA2	0x01000000	/* CAPABILITY: WPA2 avail */
#define	IEEE80211_C_WPA		0x01800000	/* CAPABILITY: WPA1+WPA2 avail*/
#define	IEEE80211_C_BURST	0x02000000	/* CAPABILITY: frame bursting */
#define	IEEE80211_C_WME		0x04000000	/* CAPABILITY: WME avail */
#define	IEEE80211_C_WDS		0x08000000	/* CAPABILITY: 4-addr support */
/* 0x10000000 reserved */
#define	IEEE80211_C_BGSCAN	0x20000000	/* CAPABILITY: bg scanning */
#define	IEEE80211_C_TXFRAG	0x40000000	/* CAPABILITY: tx fragments */
/* XXX protection/barker? */

#define	IEEE80211_C_CRYPTO	0x0000002f	/* CAPABILITY: crypto alg's */

/*
 * ic_htcaps: HT-specific device/driver capabilities
 *
 * NB: the low 16-bits are the 802.11 definitions, the upper
 *     16-bits are used to define s/w/driver capabilities.
 */
#define	IEEE80211_HTC_AMPDU	0x00010000	/* CAPABILITY: A-MPDU tx */
#define	IEEE80211_HTC_AMSDU	0x00020000	/* CAPABILITY: A-MSDU tx */
/* NB: HT40 is implied by IEEE80211_HTCAP_CHWIDTH40 */
#define	IEEE80211_HTC_HT	0x00040000	/* CAPABILITY: HT operation */

void	ieee80211_ifattach(struct ieee80211com *);
void	ieee80211_ifdetach(struct ieee80211com *);
const struct ieee80211_rateset *ieee80211_get_suprates(struct ieee80211com *ic,
		const struct ieee80211_channel *);
void	ieee80211_announce(struct ieee80211com *);
void	ieee80211_announce_channels(struct ieee80211com *);
void	ieee80211_media_init(struct ieee80211com *,
		ifm_change_cb_t, ifm_stat_cb_t);
struct ieee80211com *ieee80211_find_vap(const uint8_t mac[IEEE80211_ADDR_LEN]);
int	ieee80211_media_change(struct ifnet *);
void	ieee80211_media_status(struct ifnet *, struct ifmediareq *);
int	ieee80211_ioctl(struct ieee80211com *, u_long, caddr_t);
int	ieee80211_cfgget(struct ieee80211com *, u_long, caddr_t);
int	ieee80211_cfgset(struct ieee80211com *, u_long, caddr_t);
int	ieee80211_rate2media(struct ieee80211com *, int,
		enum ieee80211_phymode);
int	ieee80211_media2rate(int);
int	ieee80211_mhz2ieee(u_int, u_int);
int	ieee80211_chan2ieee(struct ieee80211com *,
		const struct ieee80211_channel *);
u_int	ieee80211_ieee2mhz(u_int, u_int);
struct ieee80211_channel *ieee80211_find_channel(struct ieee80211com *,
		int freq, int flags);
struct ieee80211_channel *ieee80211_find_channel_byieee(struct ieee80211com *,
		int ieee, int flags);
int	ieee80211_setmode(struct ieee80211com *, enum ieee80211_phymode);
enum ieee80211_phymode ieee80211_chan2mode(const struct ieee80211_channel *);

/* 
 * Key update synchronization methods.  XXX should not be visible.
 */
static __inline void
ieee80211_key_update_begin(struct ieee80211com *ic)
{
	ic->ic_crypto.cs_key_update_begin(ic);
}
static __inline void
ieee80211_key_update_end(struct ieee80211com *ic)
{
	ic->ic_crypto.cs_key_update_end(ic);
}

/*
 * XXX these need to be here for IEEE80211_F_DATAPAD
 */

/*
 * Return the space occupied by the 802.11 header and any
 * padding required by the driver.  This works for a
 * management or data frame.
 */
static __inline int
ieee80211_hdrspace(struct ieee80211com *ic, const void *data)
{
	int size = ieee80211_hdrsize(data);
	if (ic->ic_flags & IEEE80211_F_DATAPAD)
		size = roundup(size, sizeof(uint32_t));
	return size;
}

/*
 * Like ieee80211_hdrspace, but handles any type of frame.
 */
static __inline int
ieee80211_anyhdrspace(struct ieee80211com *ic, const void *data)
{
	int size = ieee80211_anyhdrsize(data);
	if (ic->ic_flags & IEEE80211_F_DATAPAD)
		size = roundup(size, sizeof(uint32_t));
	return size;
}

/*
 * Notify a driver that beacon state has been updated.
 */
static __inline void
ieee80211_beacon_notify(struct ieee80211com *ic, int what)
{
	if (ic->ic_state == IEEE80211_S_RUN)
		ic->ic_update_beacon(ic, what);
}

/*
 * Debugging facilities compiled in when IEEE80211_DEBUG is defined.
 *
 * The intent is that any problem in the net80211 layer can be
 * diagnosed by inspecting the statistics (dumped by the wlanstats
 * program) and/or the msgs generated by net80211.  Messages are
 * broken into functional classes and can be controlled with the
 * wlandebug program.  Certain of these msg groups are for facilities
 * that are no longer part of net80211 (e.g. IEEE80211_MSG_DOT1X).
 */
#define	IEEE80211_MSG_11N	0x80000000	/* 11n mode debug */
#define	IEEE80211_MSG_DEBUG	0x40000000	/* IFF_DEBUG equivalent */
#define	IEEE80211_MSG_DUMPPKTS	0x20000000	/* IFF_LINK2 equivalant */
#define	IEEE80211_MSG_CRYPTO	0x10000000	/* crypto work */
#define	IEEE80211_MSG_INPUT	0x08000000	/* input handling */
#define	IEEE80211_MSG_XRATE	0x04000000	/* rate set handling */
#define	IEEE80211_MSG_ELEMID	0x02000000	/* element id parsing */
#define	IEEE80211_MSG_NODE	0x01000000	/* node handling */
#define	IEEE80211_MSG_ASSOC	0x00800000	/* association handling */
#define	IEEE80211_MSG_AUTH	0x00400000	/* authentication handling */
#define	IEEE80211_MSG_SCAN	0x00200000	/* scanning */
#define	IEEE80211_MSG_OUTPUT	0x00100000	/* output handling */
#define	IEEE80211_MSG_STATE	0x00080000	/* state machine */
#define	IEEE80211_MSG_POWER	0x00040000	/* power save handling */
#define	IEEE80211_MSG_DOT1X	0x00020000	/* 802.1x authenticator */
#define	IEEE80211_MSG_DOT1XSM	0x00010000	/* 802.1x state machine */
#define	IEEE80211_MSG_RADIUS	0x00008000	/* 802.1x radius client */
#define	IEEE80211_MSG_RADDUMP	0x00004000	/* dump 802.1x radius packets */
#define	IEEE80211_MSG_RADKEYS	0x00002000	/* dump 802.1x keys */
#define	IEEE80211_MSG_WPA	0x00001000	/* WPA/RSN protocol */
#define	IEEE80211_MSG_ACL	0x00000800	/* ACL handling */
#define	IEEE80211_MSG_WME	0x00000400	/* WME protocol */
#define	IEEE80211_MSG_SUPERG	0x00000200	/* Atheros SuperG protocol */
#define	IEEE80211_MSG_DOTH	0x00000100	/* 802.11h support */
#define	IEEE80211_MSG_INACT	0x00000080	/* inactivity handling */
#define	IEEE80211_MSG_ROAM	0x00000040	/* sta-mode roaming */
#define	IEEE80211_MSG_RATECTL	0x00000020	/* tx rate control */
#define	IEEE80211_MSG_ACTION	0x00000010	/* action frame handling */
#define	IEEE80211_MSG_WDS	0x00000008	/* WDS handling */
#define	IEEE80211_MSG_IOCTL	0x00000004	/* ioctl handling */

#define	IEEE80211_MSG_ANY	0xffffffff	/* anything */

#ifdef IEEE80211_DEBUG
#define	ieee80211_msg(_ic, _m)	((_ic)->ic_debug & (_m))
#define	IEEE80211_DPRINTF(_ic, _m, _fmt, ...) do {			\
	if (ieee80211_msg(_ic, _m))					\
		ieee80211_note(_ic, _fmt, __VA_ARGS__);		\
} while (0)
#define	IEEE80211_NOTE(_ic, _m, _ni, _fmt, ...) do {			\
	if (ieee80211_msg(_ic, _m))					\
		ieee80211_note_mac(_ic, (_ni)->ni_macaddr, _fmt, __VA_ARGS__);\
} while (0)
#define	IEEE80211_NOTE_MAC(_ic, _m, _mac, _fmt, ...) do {		\
	if (ieee80211_msg(_ic, _m))					\
		ieee80211_note_mac(_ic, _mac, _fmt, __VA_ARGS__);	\
} while (0)
#define	IEEE80211_NOTE_FRAME(_ic, _m, _wh, _fmt, ...) do {		\
	if (ieee80211_msg(_ic, _m))					\
		ieee80211_note_frame(_ic, _wh, _fmt, __VA_ARGS__);	\
} while (0)
void	ieee80211_note(struct ieee80211com *ic, const char *fmt, ...);
void	ieee80211_note_mac(struct ieee80211com *ic,
		const uint8_t mac[IEEE80211_ADDR_LEN], const char *fmt, ...);
void	ieee80211_note_frame(struct ieee80211com *ic,
		const struct ieee80211_frame *wh, const char *fmt, ...);
#define	ieee80211_msg_debug(_ic) \
	((_ic)->ic_debug & IEEE80211_MSG_DEBUG)
#define	ieee80211_msg_dumppkts(_ic) \
	((_ic)->ic_debug & IEEE80211_MSG_DUMPPKTS)
#define	ieee80211_msg_input(_ic) \
	((_ic)->ic_debug & IEEE80211_MSG_INPUT)
#define	ieee80211_msg_radius(_ic) \
	((_ic)->ic_debug & IEEE80211_MSG_RADIUS)
#define	ieee80211_msg_dumpradius(_ic) \
	((_ic)->ic_debug & IEEE80211_MSG_RADDUMP)
#define	ieee80211_msg_dumpradkeys(_ic) \
	((_ic)->ic_debug & IEEE80211_MSG_RADKEYS)
#define	ieee80211_msg_scan(_ic) \
	((_ic)->ic_debug & IEEE80211_MSG_SCAN)
#define	ieee80211_msg_assoc(_ic) \
	((_ic)->ic_debug & IEEE80211_MSG_ASSOC)

/*
 * Emit a debug message about discarding a frame or information
 * element.  One format is for extracting the mac address from
 * the frame header; the other is for when a header is not
 * available or otherwise appropriate.
 */
#define	IEEE80211_DISCARD(_ic, _m, _wh, _type, _fmt, ...) do {		\
	if ((_ic)->ic_debug & (_m))					\
		ieee80211_discard_frame(_ic, _wh, _type, _fmt, __VA_ARGS__);\
} while (0)
#define	IEEE80211_DISCARD_IE(_ic, _m, _wh, _type, _fmt, ...) do {	\
	if ((_ic)->ic_debug & (_m))					\
		ieee80211_discard_ie(_ic, _wh, _type, _fmt, __VA_ARGS__);\
} while (0)
#define	IEEE80211_DISCARD_MAC(_ic, _m, _mac, _type, _fmt, ...) do {	\
	if ((_ic)->ic_debug & (_m))					\
		ieee80211_discard_mac(_ic, _mac, _type, _fmt, __VA_ARGS__);\
} while (0)

void ieee80211_discard_frame(struct ieee80211com *,
	const struct ieee80211_frame *, const char *type, const char *fmt, ...);
void ieee80211_discard_ie(struct ieee80211com *,
	const struct ieee80211_frame *, const char *type, const char *fmt, ...);
void ieee80211_discard_mac(struct ieee80211com *,
	const uint8_t mac[IEEE80211_ADDR_LEN], const char *type,
	const char *fmt, ...);
#else
#define	IEEE80211_DPRINTF(_ic, _m, _fmt, ...)
#define	IEEE80211_NOTE(_ic, _m, _ni, _fmt, ...)
#define	IEEE80211_NOTE_FRAME(_ic, _m, _wh, _fmt, ...)
#define	IEEE80211_NOTE_MAC(_ic, _m, _mac, _fmt, ...)
#define	ieee80211_msg_dumppkts(_ic)	0
#define	ieee80211_msg(_ic, _m)		0

#define	IEEE80211_DISCARD(_ic, _m, _wh, _type, _fmt, ...)
#define	IEEE80211_DISCARD_IE(_ic, _m, _wh, _type, _fmt, ...)
#define	IEEE80211_DISCARD_MAC(_ic, _m, _mac, _type, _fmt, ...)
#endif

#endif /* _NET80211_IEEE80211_VAR_H_ */
