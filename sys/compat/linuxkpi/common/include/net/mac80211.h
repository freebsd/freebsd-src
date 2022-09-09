/*-
 * Copyright (c) 2020-2022 The FreeBSD Foundation
 * Copyright (c) 2020-2022 Bjoern A. Zeeb
 *
 * This software was developed by Björn Zeeb under sponsorship from
 * the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_LINUXKPI_NET_MAC80211_H
#define	_LINUXKPI_NET_MAC80211_H

#include <sys/types.h>

#include <asm/atomic64.h>
#include <linux/bitops.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/workqueue.h>
#include <net/cfg80211.h>

#define	ARPHRD_IEEE80211_RADIOTAP		__LINE__ /* XXX TODO brcmfmac */

#define	WLAN_OUI_MICROSOFT			(0x0050F2)
#define	WLAN_OUI_TYPE_MICROSOFT_WPA		(1)
#define	WLAN_OUI_TYPE_MICROSOFT_TPC		(8)
#define	WLAN_OUI_TYPE_WFA_P2P			(9)
#define	WLAN_OUI_WFA				(0x506F9A)

/* hw->conf.flags */
enum ieee80211_hw_conf_flags {
	IEEE80211_CONF_IDLE			= BIT(0),
	IEEE80211_CONF_PS			= BIT(1),
	IEEE80211_CONF_MONITOR			= BIT(2),
	IEEE80211_CONF_OFFCHANNEL		= BIT(3),
};

/* (*ops->config()) */
enum ieee80211_hw_conf_changed_flags {
	IEEE80211_CONF_CHANGE_CHANNEL		= BIT(0),
	IEEE80211_CONF_CHANGE_IDLE		= BIT(1),
	IEEE80211_CONF_CHANGE_PS		= BIT(2),
	IEEE80211_CONF_CHANGE_MONITOR		= BIT(3),
	IEEE80211_CONF_CHANGE_POWER		= BIT(4),
};

#define	CFG80211_TESTMODE_CMD(_x)	/* XXX TODO */
#define	CFG80211_TESTMODE_DUMP(_x)	/* XXX TODO */

#define	FCS_LEN				4

/* ops.configure_filter() */
enum mcast_filter_flags {
	FIF_ALLMULTI			= BIT(0),
	FIF_PROBE_REQ			= BIT(1),
	FIF_BCN_PRBRESP_PROMISC		= BIT(2),
	FIF_FCSFAIL			= BIT(3),
	FIF_OTHER_BSS			= BIT(4),
	FIF_PSPOLL			= BIT(5),
	FIF_CONTROL			= BIT(6),
};

enum ieee80211_bss_changed {
	BSS_CHANGED_ARP_FILTER		= BIT(0),
	BSS_CHANGED_ASSOC		= BIT(1),
	BSS_CHANGED_BANDWIDTH		= BIT(2),
	BSS_CHANGED_BEACON		= BIT(3),
	BSS_CHANGED_BEACON_ENABLED	= BIT(4),
	BSS_CHANGED_BEACON_INFO		= BIT(5),
	BSS_CHANGED_BEACON_INT		= BIT(6),
	BSS_CHANGED_BSSID		= BIT(7),
	BSS_CHANGED_CQM			= BIT(8),
	BSS_CHANGED_ERP_CTS_PROT	= BIT(9),
	BSS_CHANGED_ERP_SLOT		= BIT(10),
	BSS_CHANGED_FTM_RESPONDER	= BIT(11),
	BSS_CHANGED_HT			= BIT(12),
	BSS_CHANGED_IDLE		= BIT(13),
	BSS_CHANGED_MU_GROUPS		= BIT(14),
	BSS_CHANGED_P2P_PS		= BIT(15),
	BSS_CHANGED_PS			= BIT(16),
	BSS_CHANGED_QOS			= BIT(17),
	BSS_CHANGED_TXPOWER		= BIT(18),
	BSS_CHANGED_HE_BSS_COLOR	= BIT(19),
	BSS_CHANGED_AP_PROBE_RESP	= BIT(20),
	BSS_CHANGED_BASIC_RATES		= BIT(21),
	BSS_CHANGED_ERP_PREAMBLE	= BIT(22),
	BSS_CHANGED_IBSS		= BIT(23),
	BSS_CHANGED_MCAST_RATE		= BIT(24),
	BSS_CHANGED_SSID		= BIT(25),
	BSS_CHANGED_FILS_DISCOVERY	= BIT(26),
	BSS_CHANGED_HE_OBSS_PD		= BIT(27),
	BSS_CHANGED_TWT			= BIT(28),
	BSS_CHANGED_UNSOL_BCAST_PROBE_RESP = BIT(30),
};

/* 802.11 Figure 9-256 Suite selector format. [OUI(3), SUITE TYPE(1)] */
#define	WLAN_CIPHER_SUITE_OUI(_oui, _x)	(((_oui) << 8) | ((_x) & 0xff))

/* 802.11 Table 9-131 Cipher suite selectors. */
/* 802.1x suite B			11 */
#define	WLAN_CIPHER_SUITE(_x)		WLAN_CIPHER_SUITE_OUI(0x000fac, _x)
/* Use group				0 */
#define	WLAN_CIPHER_SUITE_WEP40		WLAN_CIPHER_SUITE(1)
#define	WLAN_CIPHER_SUITE_TKIP		WLAN_CIPHER_SUITE(2)
/* Reserved				3 */
#define	WLAN_CIPHER_SUITE_CCMP		WLAN_CIPHER_SUITE(4)	/* CCMP-128 */
#define	WLAN_CIPHER_SUITE_WEP104	WLAN_CIPHER_SUITE(5)
#define	WLAN_CIPHER_SUITE_AES_CMAC	WLAN_CIPHER_SUITE(6)	/* BIP-CMAC-128 */
/* Group addressed traffic not allowed	7 */
#define	WLAN_CIPHER_SUITE_GCMP		WLAN_CIPHER_SUITE(8)
#define	WLAN_CIPHER_SUITE_GCMP_256	WLAN_CIPHER_SUITE(9)
#define	WLAN_CIPHER_SUITE_CCMP_256	WLAN_CIPHER_SUITE(10)
#define	WLAN_CIPHER_SUITE_BIP_GMAC_128	WLAN_CIPHER_SUITE(11)
#define	WLAN_CIPHER_SUITE_BIP_GMAC_256	WLAN_CIPHER_SUITE(12)
#define	WLAN_CIPHER_SUITE_BIP_CMAC_256	WLAN_CIPHER_SUITE(13)
/* Reserved				14-255 */

/* See ISO/IEC JTC 1 N 9880 Table 11 */
#define	WLAN_CIPHER_SUITE_SMS4		WLAN_CIPHER_SUITE_OUI(0x001472, 1)


/* 802.11 Table 9-133 AKM suite selectors. */
#define	WLAN_AKM_SUITE(_x)		WLAN_CIPHER_SUITE_OUI(0x000fac, _x)
/* Reserved				0 */
#define	WLAN_AKM_SUITE_8021X		WLAN_AKM_SUITE(1)
#define	WLAN_AKM_SUITE_PSK		WLAN_AKM_SUITE(2)
#define	WLAN_AKM_SUITE_FT_8021X		WLAN_AKM_SUITE(3)
#define	WLAN_AKM_SUITE_FT_PSK		WLAN_AKM_SUITE(4)
#define	WLAN_AKM_SUITE_8021X_SHA256	WLAN_AKM_SUITE(5)
#define	WLAN_AKM_SUITE_PSK_SHA256	WLAN_AKM_SUITE(6)
/* TDLS					7 */
#define	WLAN_AKM_SUITE_SAE		WLAN_AKM_SUITE(8)
/* FToSAE				9 */
/* AP peer key				10 */
/* 802.1x suite B			11 */
/* 802.1x suite B 384			12 */
/* FTo802.1x 384			13 */
/* Reserved				14-255 */
/* Apparently 11ax defines more. Seen (19,20) mentioned. */


struct ieee80211_sta;

/* 802.11-2020 9.4.2.55.3 A-MPDU Parameters field */
#define	IEEE80211_HT_AMPDU_PARM_FACTOR		0x3
#define	IEEE80211_HT_AMPDU_PARM_DENSITY_SHIFT	2
#define	IEEE80211_HT_AMPDU_PARM_DENSITY		(0x7 << IEEE80211_HT_AMPDU_PARM_DENSITY_SHIFT)

struct ieee80211_ampdu_params {
	/* TODO FIXME */
	struct ieee80211_sta			*sta;
	uint8_t					tid;
	uint16_t				ssn;
	int		action, amsdu, buf_size, timeout;
};

struct ieee80211_bar {
	/* TODO FIXME */
	int		control, start_seq_num;
	uint8_t		*ra;
	uint16_t	frame_control;
};

struct ieee80211_p2p_noa_desc {
	uint32_t				count;		/* uint8_t ? */
	uint32_t				duration;
	uint32_t				interval;
	uint32_t				start_time;
};

struct ieee80211_p2p_noa_attr {
	uint8_t					index;
	uint8_t					oppps_ctwindow;
	struct ieee80211_p2p_noa_desc		desc[4];
};

struct ieee80211_mutable_offsets {
	/* TODO FIXME */
	uint16_t				tim_offset;
	uint16_t				cntdwn_counter_offs[2];

	int	mbssid_off;
};

struct mac80211_fils_discovery {
	uint32_t				max_interval;
};

#define	WLAN_MEMBERSHIP_LEN			(8)
#define	WLAN_USER_POSITION_LEN			(16)

struct ieee80211_bss_conf {
	/* TODO FIXME */
	const uint8_t				*bssid;
	uint8_t					transmitter_bssid[ETH_ALEN];
	struct ieee80211_ftm_responder_params	*ftmr_params;
	struct ieee80211_p2p_noa_attr		p2p_noa_attr;
	struct cfg80211_chan_def		chandef;
	__be32					arp_addr_list[1];	/* XXX TODO */
	struct ieee80211_rate			*beacon_rate;
	struct {
		uint8_t membership[WLAN_MEMBERSHIP_LEN];
		uint8_t position[WLAN_USER_POSITION_LEN];
	}  mu_group;
	struct cfg80211_he_bss_color		he_bss_color;
	struct ieee80211_he_obss_pd		he_obss_pd;
	size_t					ssid_len;
	uint8_t					ssid[IEEE80211_NWID_LEN];
	uint16_t				aid;
	uint16_t				ht_operation_mode;
	int					arp_addr_cnt;

	uint8_t					dtim_period;
	uint8_t					sync_dtim_count;
	bool					assoc;
	bool					idle;
	bool					qos;
	bool					ps;
	bool					twt_broadcast;
	bool					use_cts_prot;
	bool					use_short_preamble;
	bool					use_short_slot;
	bool					he_support;
	bool					csa_active;
	uint32_t				sync_device_ts;
	uint64_t				sync_tsf;
	uint16_t				beacon_int;
	int16_t					txpower;
	uint32_t				basic_rates;
	int					mcast_rate[NUM_NL80211_BANDS];
	struct cfg80211_bitrate_mask		beacon_tx_rate;
	struct mac80211_fils_discovery		fils_discovery;

	int		ack_enabled, bssid_index, bssid_indicator, cqm_rssi_hyst, cqm_rssi_thold, ema_ap, frame_time_rts_th, ftm_responder;
	int		htc_trig_based_pkt_ext;
	int		multi_sta_back_32bit, nontransmitted;
	int		profile_periodicity;
	int		twt_requester, uora_exists, uora_ocw_range;
	int		assoc_capability, enable_beacon, hidden_ssid, ibss_joined, twt_protected;
	int		 he_oper, twt_responder, unsol_bcast_probe_resp_interval;
	int		color_change_active;
};

struct ieee80211_chanctx_conf {
	/* TODO FIXME */
	int		rx_chains_dynamic, rx_chains_static;
	bool					radar_enabled;
	struct cfg80211_chan_def		def;
	struct cfg80211_chan_def		min_def;

	/* Must stay last. */
	uint8_t					drv_priv[0] __aligned(CACHE_LINE_SIZE);
};

struct ieee80211_channel_switch {
	/* TODO FIXME */
	int		block_tx, count, delay, device_timestamp, timestamp;
	struct cfg80211_chan_def		chandef;
};

struct ieee80211_cipher_scheme {
	uint32_t	cipher;
	uint8_t		iftype;		/* We do not know the size of this. */
	uint8_t		hdr_len;
	uint8_t		pn_len;
	uint8_t		pn_off;
	uint8_t		key_idx_off;
	uint8_t		key_idx_mask;
	uint8_t		key_idx_shift;
	uint8_t		mic_len;
};

enum ieee80211_event_type {
	BA_FRAME_TIMEOUT,
	BAR_RX_EVENT,
	MLME_EVENT,
	RSSI_EVENT,
};

enum ieee80211_rssi_event_data {
	RSSI_EVENT_LOW,
	RSSI_EVENT_HIGH,
};

enum ieee80211_mlme_event_data {
	ASSOC_EVENT,
	AUTH_EVENT,
	DEAUTH_RX_EVENT,
	DEAUTH_TX_EVENT,
};

enum ieee80211_mlme_event_status {
	MLME_DENIED,
	MLME_TIMEOUT,
};

struct ieee80211_mlme_event {
	enum ieee80211_mlme_event_data		data;
	enum ieee80211_mlme_event_status	status;
	int					reason;
};

struct ieee80211_event {
	/* TODO FIXME */
	enum ieee80211_event_type		type;
	union {
		struct {
			int     ssn;
			struct ieee80211_sta	*sta;
			uint8_t		 	tid;
		} ba;
		struct ieee80211_mlme_event	mlme;
	} u;
};

struct ieee80211_ftm_responder_params {
	/* TODO FIXME */
	uint8_t					*lci;
	uint8_t					*civicloc;
	int					lci_len;
	int					civicloc_len;
};

struct ieee80211_he_mu_edca_param_ac_rec {
	/* TODO FIXME */
	int		aifsn, ecw_min_max, mu_edca_timer;
};

struct ieee80211_conf {
	int					dynamic_ps_timeout;
	int					power_level;
	uint32_t				listen_interval;
	bool					radar_enabled;
	enum ieee80211_hw_conf_flags		flags;
	struct cfg80211_chan_def		chandef;
};

enum ieee80211_hw_flags {
	IEEE80211_HW_AMPDU_AGGREGATION,
	IEEE80211_HW_AP_LINK_PS,
	IEEE80211_HW_BUFF_MMPDU_TXQ,
	IEEE80211_HW_CHANCTX_STA_CSA,
	IEEE80211_HW_CONNECTION_MONITOR,
	IEEE80211_HW_DEAUTH_NEED_MGD_TX_PREP,
	IEEE80211_HW_HAS_RATE_CONTROL,
	IEEE80211_HW_MFP_CAPABLE,
	IEEE80211_HW_NEEDS_UNIQUE_STA_ADDR,
	IEEE80211_HW_REPORTS_TX_ACK_STATUS,
	IEEE80211_HW_RX_INCLUDES_FCS,
	IEEE80211_HW_SIGNAL_DBM,
	IEEE80211_HW_SINGLE_SCAN_ON_ALL_BANDS,
	IEEE80211_HW_SPECTRUM_MGMT,
	IEEE80211_HW_STA_MMPDU_TXQ,
	IEEE80211_HW_SUPPORTS_AMSDU_IN_AMPDU,
	IEEE80211_HW_SUPPORTS_CLONED_SKBS,
	IEEE80211_HW_SUPPORTS_DYNAMIC_PS,
	IEEE80211_HW_SUPPORTS_MULTI_BSSID,
	IEEE80211_HW_SUPPORTS_ONLY_HE_MULTI_BSSID,
	IEEE80211_HW_SUPPORTS_PS,
	IEEE80211_HW_SUPPORTS_REORDERING_BUFFER,
	IEEE80211_HW_SUPPORTS_VHT_EXT_NSS_BW,
	IEEE80211_HW_SUPPORT_FAST_XMIT,
	IEEE80211_HW_TDLS_WIDER_BW,
	IEEE80211_HW_TIMING_BEACON_ONLY,
	IEEE80211_HW_TX_AMPDU_SETUP_IN_HW,
	IEEE80211_HW_TX_AMSDU,
	IEEE80211_HW_TX_FRAG_LIST,
	IEEE80211_HW_USES_RSS,
	IEEE80211_HW_WANT_MONITOR_VIF,
	IEEE80211_HW_SW_CRYPTO_CONTROL,
	IEEE80211_HW_SUPPORTS_TX_FRAG,
	IEEE80211_HW_SUPPORTS_TDLS_BUFFER_STA,
	IEEE80211_HW_SUPPORTS_PER_STA_GTK,
	IEEE80211_HW_REPORTS_LOW_ACK,
	IEEE80211_HW_QUEUE_CONTROL,
	IEEE80211_HW_SUPPORTS_RX_DECAP_OFFLOAD,
	IEEE80211_HW_SUPPORTS_TX_ENCAP_OFFLOAD,
	IEEE80211_HW_SUPPORTS_RC_TABLE,

	/* Keep last. */
	NUM_IEEE80211_HW_FLAGS
};

struct ieee80211_hw {

	struct wiphy			*wiphy;

	/* TODO FIXME */
	int		max_rx_aggregation_subframes, max_tx_aggregation_subframes;
	int		extra_tx_headroom, weight_multiplier;
	int		max_rate_tries, max_rates, max_report_rates;
	struct ieee80211_cipher_scheme	*cipher_schemes;
	int				n_cipher_schemes;
	const char			*rate_control_algorithm;
	struct {
		uint16_t units_pos;	/* radiotap "spec" is .. inconsistent. */
		uint16_t accuracy;
	} radiotap_timestamp;
	size_t				sta_data_size;
	size_t				vif_data_size;
	size_t				chanctx_data_size;
	size_t				txq_data_size;
	uint16_t			radiotap_mcs_details;
	uint16_t			radiotap_vht_details;
	uint16_t			queues;
	uint16_t			offchannel_tx_hw_queue;
	uint16_t			uapsd_max_sp_len;
	uint16_t			uapsd_queues;
	uint16_t			max_tx_fragments;
	uint16_t			max_listen_interval;
	netdev_features_t		netdev_features;
	unsigned long			flags[BITS_TO_LONGS(NUM_IEEE80211_HW_FLAGS)];
	struct ieee80211_conf		conf;

#if 0	/* leave here for documentation purposes.  This does NOT work. */
	/* Must stay last. */
	uint8_t				priv[0] __aligned(CACHE_LINE_SIZE);
#else
	void				*priv;
#endif
};

enum ieee802111_key_flag {
	IEEE80211_KEY_FLAG_GENERATE_IV		= BIT(0),
	IEEE80211_KEY_FLAG_GENERATE_MMIC	= BIT(1),
	IEEE80211_KEY_FLAG_PAIRWISE		= BIT(2),
	IEEE80211_KEY_FLAG_PUT_IV_SPACE		= BIT(3),
	IEEE80211_KEY_FLAG_PUT_MIC_SPACE	= BIT(4),
	IEEE80211_KEY_FLAG_SW_MGMT_TX		= BIT(5),
	IEEE80211_KEY_FLAG_GENERATE_IV_MGMT	= BIT(6),
	IEEE80211_KEY_FLAG_GENERATE_MMIE	= BIT(7),
};

struct ieee80211_key_conf {
	atomic64_t			tx_pn;
	uint32_t			cipher;
	uint8_t				icv_len;	/* __unused nowadays? */
	uint8_t				iv_len;
	uint8_t				hw_key_idx;	/* Set by drv. */
	uint8_t				keyidx;
	uint16_t			flags;
	uint8_t				keylen;
	uint8_t				key[0];
};

struct ieee80211_key_seq {
	/* TODO FIXME */
	union {
		struct {
			uint8_t		seq[IEEE80211_MAX_PN_LEN];
			uint8_t		seq_len;
		} hw;
		struct {
			uint8_t		pn[IEEE80211_CCMP_PN_LEN];
		} ccmp;
		struct {
			uint8_t		pn[IEEE80211_CCMP_PN_LEN];
		} aes_cmac;
		struct {
			uint32_t	iv32;
			uint16_t	iv16;
		} tkip;
	};
};


enum ieee80211_rx_status_flags {
	RX_FLAG_ALLOW_SAME_PN		= BIT(0),
	RX_FLAG_AMPDU_DETAILS		= BIT(1),
	RX_FLAG_AMPDU_EOF_BIT		= BIT(2),
	RX_FLAG_AMPDU_EOF_BIT_KNOWN	= BIT(3),
	RX_FLAG_DECRYPTED		= BIT(4),
	RX_FLAG_DUP_VALIDATED		= BIT(5),
	RX_FLAG_FAILED_FCS_CRC		= BIT(6),
	RX_FLAG_ICV_STRIPPED		= BIT(7),
	RX_FLAG_MACTIME_PLCP_START	= BIT(8),
	RX_FLAG_MACTIME_START		= BIT(9),
	RX_FLAG_MIC_STRIPPED		= BIT(10),
	RX_FLAG_MMIC_ERROR		= BIT(11),
	RX_FLAG_MMIC_STRIPPED		= BIT(12),
	RX_FLAG_NO_PSDU			= BIT(13),
	RX_FLAG_PN_VALIDATED		= BIT(14),
	RX_FLAG_RADIOTAP_HE		= BIT(15),
	RX_FLAG_RADIOTAP_HE_MU		= BIT(16),
	RX_FLAG_RADIOTAP_LSIG		= BIT(17),
	RX_FLAG_RADIOTAP_VENDOR_DATA	= BIT(18),
	RX_FLAG_NO_SIGNAL_VAL		= BIT(19),
	RX_FLAG_IV_STRIPPED		= BIT(20),
	RX_FLAG_AMPDU_IS_LAST		= BIT(21),
	RX_FLAG_AMPDU_LAST_KNOWN	= BIT(22),
	RX_FLAG_AMSDU_MORE		= BIT(23),
	RX_FLAG_MACTIME_END		= BIT(24),
	RX_FLAG_ONLY_MONITOR		= BIT(25),
	RX_FLAG_SKIP_MONITOR		= BIT(26),
	RX_FLAG_8023			= BIT(27),
};

enum mac80211_rx_encoding {
	RX_ENC_LEGACY		= 0,
	RX_ENC_HT,
	RX_ENC_VHT,
	RX_ENC_HE
};

struct ieee80211_rx_status {
	/* TODO FIXME, this is too large. Over-reduce types to u8 where possible. */
	uint64_t			boottime_ns;
	uint64_t			mactime;
	uint32_t			device_timestamp;
	enum ieee80211_rx_status_flags	flag;
	uint16_t			freq;
	uint8_t				encoding:2, bw:3, he_ru:3;	/* enum mac80211_rx_encoding, rate_info_bw */	/* See mt76.h */
	uint8_t				ampdu_reference;
	uint8_t				band;
	uint8_t				chains;
	int8_t				chain_signal[IEEE80211_MAX_CHAINS];
	int8_t				signal;
	uint8_t				enc_flags;
	uint8_t				he_dcm;
	uint8_t				he_gi;
	uint8_t				zero_length_psdu_type;
	uint8_t				nss;
	uint8_t				rate_idx;
};

struct ieee80211_tx_rate_status {
};

struct ieee80211_tx_status {
	struct ieee80211_sta		*sta;
	struct ieee80211_tx_info	*info;

	u8				n_rates;
	struct ieee80211_tx_rate_status	*rates;

	struct sk_buff			*skb;
	struct list_head		*free_list;
};

struct ieee80211_scan_ies {
	/* TODO FIXME */
	int		common_ie_len;
	int		len[NUM_NL80211_BANDS];
	uint8_t		*common_ies;
	uint8_t		*ies[NUM_NL80211_BANDS];
};

struct ieee80211_scan_request {
	struct ieee80211_scan_ies	ies;
	struct cfg80211_scan_request	req;
};

struct ieee80211_txq {
	struct ieee80211_sta		*sta;
	struct ieee80211_vif		*vif;
	int				ac;
	uint8_t				tid;

	/* Must stay last. */
	uint8_t				drv_priv[0] __aligned(CACHE_LINE_SIZE);
};

struct ieee80211_sta_rates {
	/* XXX TODO */
	/* XXX some _rcu thing */
	struct {
		int	idx;
		int	flags;
	} rate[1];		/* XXX what is the real number? */
};

struct ieee80211_sta_txpwr {
	/* XXX TODO */
	enum nl80211_tx_power_setting	type;
	short				power;
};

struct ieee80211_link_sta {
	uint32_t				supp_rates[NUM_NL80211_BANDS];
	struct ieee80211_sta_ht_cap		ht_cap;
	struct ieee80211_sta_vht_cap		vht_cap;
	struct ieee80211_sta_he_cap		he_cap;
	struct ieee80211_sta_he_6ghz_capa	he_6ghz_capa;
	uint8_t					rx_nss;
	enum ieee80211_sta_rx_bw		bandwidth;
	struct ieee80211_sta_txpwr		txpwr;
};

#define	IEEE80211_NUM_TIDS			16	/* net80211::WME_NUM_TID */
struct ieee80211_sta {
	/* TODO FIXME */
	int		max_amsdu_len, max_amsdu_subframes, max_rc_amsdu_len, max_sp;
	int		mfp, smps_mode, tdls, tdls_initiator, uapsd_queues, wme;
	struct ieee80211_txq			*txq[IEEE80211_NUM_TIDS + 1];	/* iwlwifi: 8 and adds +1 to tid_data, net80211::IEEE80211_TID_SIZE */
	struct ieee80211_sta_rates		*rates;	/* some rcu thing? */
	uint32_t				max_tid_amsdu_len[IEEE80211_NUM_TIDS];
	uint8_t					addr[ETH_ALEN];
	uint16_t				aid;

	struct ieee80211_link_sta		deflink;

	/* Must stay last. */
	uint8_t					drv_priv[0] __aligned(CACHE_LINE_SIZE);
};

struct ieee80211_tdls_ch_sw_params {
	/* TODO FIXME */
	int		action_code, ch_sw_tm_ie, status, switch_time, switch_timeout, timestamp;
	struct ieee80211_sta			*sta;
	struct cfg80211_chan_def		*chandef;
	struct sk_buff				*tmpl_skb;
};

struct ieee80211_tx_control {
	/* TODO FIXME */
	struct ieee80211_sta			*sta;
};

struct ieee80211_tx_queue_params {
	/* These types are based on iwlwifi FW structs. */
	uint16_t	cw_min;
	uint16_t	cw_max;
	uint16_t	txop;
	uint8_t		aifs;

	/* TODO FIXME */
	int		acm, mu_edca, uapsd;
	struct ieee80211_he_mu_edca_param_ac_rec	mu_edca_param_rec;
};

struct ieee80211_tx_rate {
	uint8_t		idx;
	uint16_t	count:5,
			flags:11;
};

enum ieee80211_vif_driver_flags {
	IEEE80211_VIF_BEACON_FILTER		= BIT(0),
	IEEE80211_VIF_SUPPORTS_CQM_RSSI		= BIT(1),
	IEEE80211_VIF_SUPPORTS_UAPSD		= BIT(2),
};

#define	IEEE80211_BSS_ARP_ADDR_LIST_LEN		4

struct ieee80211_vif_cfg {
	uint16_t				aid;
	bool					assoc;
	int					arp_addr_cnt;
	uint32_t				arp_addr_list[IEEE80211_BSS_ARP_ADDR_LIST_LEN];		/* big endian */
};

struct ieee80211_vif {
	/* TODO FIXME */
	enum nl80211_iftype		type;
	int		csa_active, mu_mimo_owner;
	int		cab_queue;
	int     color_change_active, offload_flags;
	enum ieee80211_vif_driver_flags	driver_flags;
	bool				p2p;
	bool				probe_req_reg;
	uint8_t				addr[ETH_ALEN];
	struct ieee80211_vif_cfg	cfg;
	struct ieee80211_chanctx_conf	*chanctx_conf;
	struct ieee80211_txq		*txq;
	struct ieee80211_bss_conf	bss_conf;
	uint8_t				hw_queue[IEEE80211_NUM_ACS];

	/* Must stay last. */
	uint8_t				drv_priv[0] __aligned(CACHE_LINE_SIZE);
};

struct ieee80211_vif_chanctx_switch {
	struct ieee80211_chanctx_conf	*old_ctx, *new_ctx;
	struct ieee80211_vif		*vif;
};

struct ieee80211_prep_tx_info {
	u16				duration;
	bool				success;
};

/* XXX-BZ too big, over-reduce size to u8, and array sizes to minuimum to fit in skb->cb. */
/* Also warning: some sizes change by pointer size!  This is 64bit only. */
struct ieee80211_tx_info {
	enum ieee80211_tx_info_flags		flags;
	/* TODO FIXME */
	u8		band;
	u8		hw_queue;
	bool		tx_time_est;
	union {
		struct {
			struct ieee80211_tx_rate	rates[4];
			bool				use_rts;
			struct ieee80211_vif		*vif;
			struct ieee80211_key_conf	*hw_key;
			enum ieee80211_tx_control_flags	flags;
		} control;
		struct {
			struct ieee80211_tx_rate	rates[4];
			uint32_t			ack_signal;
			uint8_t				ampdu_ack_len;
			uint8_t				ampdu_len;
			uint8_t				antenna;
			uint16_t			tx_time;
			bool				is_valid_ack_signal;
			void				*status_driver_data[16 / sizeof(void *)];		/* XXX TODO */
		} status;
#define	IEEE80211_TX_INFO_DRIVER_DATA_SIZE	(5 * sizeof(void *))			/* XXX TODO 5? */
		void					*driver_data[IEEE80211_TX_INFO_DRIVER_DATA_SIZE / sizeof(void *)];
	};
};

/* net80211 conflict */
struct linuxkpi_ieee80211_tim_ie {
	uint8_t				dtim_count;
	uint8_t				dtim_period;
	uint8_t				bitmap_ctrl;
	uint8_t				*virtual_map;
};
#define	ieee80211_tim_ie	linuxkpi_ieee80211_tim_ie

struct survey_info {		/* net80211::struct ieee80211_channel_survey */
	/* TODO FIXME */
	uint32_t			filled;
#define	SURVEY_INFO_TIME		0x0001
#define	SURVEY_INFO_TIME_RX		0x0002
#define	SURVEY_INFO_TIME_SCAN		0x0004
#define	SURVEY_INFO_TIME_TX		0x0008
#define	SURVEY_INFO_TIME_BSS_RX		0x0010
#define	SURVEY_INFO_TIME_BUSY		0x0020
#define	SURVEY_INFO_IN_USE		0x0040
#define	SURVEY_INFO_NOISE_DBM		0x0080
	uint32_t			noise;
	uint64_t			time;
	uint64_t			time_bss_rx;
	uint64_t			time_busy;
	uint64_t			time_rx;
	uint64_t			time_scan;
	uint64_t			time_tx;
	struct ieee80211_channel	*channel;
};

enum ieee80211_iface_iter {
	IEEE80211_IFACE_ITER_NORMAL	= BIT(0),
	IEEE80211_IFACE_ITER_RESUME_ALL	= BIT(1),
	IEEE80211_IFACE_SKIP_SDATA_NOT_IN_DRIVER = BIT(2),	/* seems to be an iter flag */

	/* Internal flags only. */
	/* ieee80211_iterate_active_interfaces*(). */
	IEEE80211_IFACE_ITER__ATOMIC	= BIT(6),
	IEEE80211_IFACE_ITER__ACTIVE	= BIT(7),
};

enum set_key_cmd {
	SET_KEY,
	DISABLE_KEY,
};

/* 802.11-2020, 9.4.2.55.2 HT Capability Information field. */
enum rx_enc_flags {
	RX_ENC_FLAG_SHORTPRE	=	BIT(0),
	RX_ENC_FLAG_SHORT_GI	=	BIT(2),
	RX_ENC_FLAG_HT_GF	=	BIT(3),
	RX_ENC_FLAG_STBC_MASK	=	BIT(4) | BIT(5),
#define	RX_ENC_FLAG_STBC_SHIFT		4
	RX_ENC_FLAG_LDPC	=	BIT(6),
	RX_ENC_FLAG_BF		=	BIT(7),
};

enum sta_notify_cmd {
	STA_NOTIFY_AWAKE,
	STA_NOTIFY_SLEEP,
};

struct ieee80211_low_level_stats {
	/* Can we make them uint64_t? */
	uint32_t dot11ACKFailureCount;
	uint32_t dot11FCSErrorCount;
	uint32_t dot11RTSFailureCount;
	uint32_t dot11RTSSuccessCount;
};

enum ieee80211_offload_flags {
	IEEE80211_OFFLOAD_ENCAP_4ADDR,
};

struct ieee80211_ops {
	/* TODO FIXME */
	int  (*start)(struct ieee80211_hw *);
	void (*stop)(struct ieee80211_hw *);

	int  (*config)(struct ieee80211_hw *, u32);
	void (*reconfig_complete)(struct ieee80211_hw *, enum ieee80211_reconfig_type);

	int  (*add_interface)(struct ieee80211_hw *, struct ieee80211_vif *);
	void (*remove_interface)(struct ieee80211_hw *, struct ieee80211_vif *);
	int  (*change_interface)(struct ieee80211_hw *, struct ieee80211_vif *, enum nl80211_iftype, bool);

	void (*sw_scan_start)(struct ieee80211_hw *, struct ieee80211_vif *, const u8 *);
	void (*sw_scan_complete)(struct ieee80211_hw *, struct ieee80211_vif *);
	int  (*sched_scan_start)(struct ieee80211_hw *, struct ieee80211_vif *, struct cfg80211_sched_scan_request *, struct ieee80211_scan_ies *);
	int  (*sched_scan_stop)(struct ieee80211_hw *, struct ieee80211_vif *);
	int  (*hw_scan)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_scan_request *);
	void (*cancel_hw_scan)(struct ieee80211_hw *, struct ieee80211_vif *);

	int  (*conf_tx)(struct ieee80211_hw *, struct ieee80211_vif *, u32, u16, const struct ieee80211_tx_queue_params *);
	void (*tx)(struct ieee80211_hw *, struct ieee80211_tx_control *, struct sk_buff *);
	int  (*tx_last_beacon)(struct ieee80211_hw *);
	void (*wake_tx_queue)(struct ieee80211_hw *, struct ieee80211_txq *);

	void (*mgd_prepare_tx)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_prep_tx_info *);
	void (*mgd_complete_tx)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_prep_tx_info *);
	void (*mgd_protect_tdls_discover)(struct ieee80211_hw *, struct ieee80211_vif *);

	void (*flush)(struct ieee80211_hw *, struct ieee80211_vif *, u32, bool);

	int  (*set_frag_threshold)(struct ieee80211_hw *, u32);

	void (*sync_rx_queues)(struct ieee80211_hw *);

	void (*allow_buffered_frames)(struct ieee80211_hw *, struct ieee80211_sta *, u16, int, enum ieee80211_frame_release_type, bool);
	void (*release_buffered_frames)(struct ieee80211_hw *, struct ieee80211_sta *, u16, int, enum ieee80211_frame_release_type, bool);

	int  (*sta_add)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_sta *);
	int  (*sta_remove)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_sta *);
	int  (*sta_set_txpwr)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_sta *);
	void (*sta_statistics)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_sta *, struct station_info *);
	void (*sta_pre_rcu_remove)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_sta *);
	int  (*sta_state)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_sta *, enum ieee80211_sta_state, enum ieee80211_sta_state);
	void (*sta_notify)(struct ieee80211_hw *, struct ieee80211_vif *, enum sta_notify_cmd, struct ieee80211_sta *);
	void (*sta_rc_update)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_sta *, u32);
	void (*sta_rate_tbl_update)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_sta *);
	void (*sta_set_4addr)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_sta *, bool);
	void (*sta_set_decap_offload)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_sta *, bool);

	u64  (*prepare_multicast)(struct ieee80211_hw *, struct netdev_hw_addr_list *);

	int  (*ampdu_action)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_ampdu_params *);

	bool (*can_aggregate_in_amsdu)(struct ieee80211_hw *, struct sk_buff *, struct sk_buff *);

	int  (*pre_channel_switch)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_channel_switch *);
	int  (*post_channel_switch)(struct ieee80211_hw *, struct ieee80211_vif *);
	void (*channel_switch)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_channel_switch *);
	void (*channel_switch_beacon)(struct ieee80211_hw *, struct ieee80211_vif *, struct cfg80211_chan_def *);
	void (*abort_channel_switch)(struct ieee80211_hw *, struct ieee80211_vif *);
	void (*channel_switch_rx_beacon)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_channel_switch *);
	int  (*tdls_channel_switch)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_sta *, u8, struct cfg80211_chan_def *, struct sk_buff *, u32);
	void (*tdls_cancel_channel_switch)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_sta *);
	void (*tdls_recv_channel_switch)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_tdls_ch_sw_params *);

	int  (*add_chanctx)(struct ieee80211_hw *, struct ieee80211_chanctx_conf *);
	void (*remove_chanctx)(struct ieee80211_hw *, struct ieee80211_chanctx_conf *);
	void (*change_chanctx)(struct ieee80211_hw *, struct ieee80211_chanctx_conf *, u32);
	int  (*assign_vif_chanctx)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_bss_conf *, struct ieee80211_chanctx_conf *);
	void (*unassign_vif_chanctx)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_bss_conf *, struct ieee80211_chanctx_conf *);
	int  (*switch_vif_chanctx)(struct ieee80211_hw *, struct ieee80211_vif_chanctx_switch *, int, enum ieee80211_chanctx_switch_mode);

	int  (*get_antenna)(struct ieee80211_hw *, u32 *, u32 *);
	int  (*set_antenna)(struct ieee80211_hw *, u32, u32);

	int  (*remain_on_channel)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_channel *, int, enum ieee80211_roc_type);
	int  (*cancel_remain_on_channel)(struct ieee80211_hw *, struct ieee80211_vif *);

	void (*configure_filter)(struct ieee80211_hw *, unsigned int, unsigned int *, u64);
	void (*config_iface_filter)(struct ieee80211_hw *, struct ieee80211_vif *, unsigned int, unsigned int);

	void (*bss_info_changed)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_bss_conf *, u64);
	int  (*set_rts_threshold)(struct ieee80211_hw *, u32);
	void (*event_callback)(struct ieee80211_hw *, struct ieee80211_vif *, const struct ieee80211_event *);
	int  (*get_survey)(struct ieee80211_hw *, int, struct survey_info *);
	int  (*get_ftm_responder_stats)(struct ieee80211_hw *, struct ieee80211_vif *, struct cfg80211_ftm_responder_stats *);

        uint64_t (*get_tsf)(struct ieee80211_hw *, struct ieee80211_vif *);
        void (*set_tsf)(struct ieee80211_hw *, struct ieee80211_vif *, uint64_t);
	void (*offset_tsf)(struct ieee80211_hw *, struct ieee80211_vif *, s64);

	int  (*set_bitrate_mask)(struct ieee80211_hw *, struct ieee80211_vif *, const struct cfg80211_bitrate_mask *);
	void (*set_coverage_class)(struct ieee80211_hw *, s16);
	int  (*set_tim)(struct ieee80211_hw *, struct ieee80211_sta *, bool);

	int  (*set_key)(struct ieee80211_hw *, enum set_key_cmd, struct ieee80211_vif *, struct ieee80211_sta *, struct ieee80211_key_conf *);
	void (*set_default_unicast_key)(struct ieee80211_hw *, struct ieee80211_vif *, int);
	void (*update_tkip_key)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_key_conf *, struct ieee80211_sta *, u32, u16 *);

	int  (*start_pmsr)(struct ieee80211_hw *, struct ieee80211_vif *, struct cfg80211_pmsr_request *);
	void (*abort_pmsr)(struct ieee80211_hw *, struct ieee80211_vif *, struct cfg80211_pmsr_request *);

	int  (*start_ap)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_bss_conf *link_conf);
	void (*stop_ap)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_bss_conf *link_conf);
	int  (*join_ibss)(struct ieee80211_hw *, struct ieee80211_vif *);
	void (*leave_ibss)(struct ieee80211_hw *, struct ieee80211_vif *);

	int  (*set_sar_specs)(struct ieee80211_hw *, const struct cfg80211_sar_specs *);

	int  (*set_tid_config)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_sta *, struct cfg80211_tid_config *);
	int  (*reset_tid_config)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_sta *, u8);

	int  (*get_et_sset_count)(struct ieee80211_hw *, struct ieee80211_vif *, int);
	void (*get_et_stats)(struct ieee80211_hw *, struct ieee80211_vif *, struct ethtool_stats *, u64 *);
	void (*get_et_strings)(struct ieee80211_hw *, struct ieee80211_vif *, u32, u8 *);

	void (*update_vif_offload)(struct ieee80211_hw *, struct ieee80211_vif *);

	int  (*get_txpower)(struct ieee80211_hw *, struct ieee80211_vif *, int *);
	int  (*get_stats)(struct ieee80211_hw *, struct ieee80211_low_level_stats *);

	int  (*set_radar_background)(struct ieee80211_hw *, struct cfg80211_chan_def *);

	void (*add_twt_setup)(struct ieee80211_hw *, struct ieee80211_sta *, struct ieee80211_twt_setup *);
	void (*twt_teardown_request)(struct ieee80211_hw *, struct ieee80211_sta *, u8);
};


/* -------------------------------------------------------------------------- */

/* linux_80211.c */
extern const struct cfg80211_ops linuxkpi_mac80211cfgops;

struct ieee80211_hw *linuxkpi_ieee80211_alloc_hw(size_t,
    const struct ieee80211_ops *);
void linuxkpi_ieee80211_iffree(struct ieee80211_hw *);
void linuxkpi_set_ieee80211_dev(struct ieee80211_hw *, char *);
int linuxkpi_ieee80211_ifattach(struct ieee80211_hw *);
void linuxkpi_ieee80211_ifdetach(struct ieee80211_hw *);
struct ieee80211_hw * linuxkpi_wiphy_to_ieee80211_hw(struct wiphy *);
void linuxkpi_ieee80211_iterate_interfaces(
    struct ieee80211_hw *hw, enum ieee80211_iface_iter flags,
    void(*iterfunc)(void *, uint8_t *, struct ieee80211_vif *),
    void *);
void linuxkpi_ieee80211_iterate_keys(struct ieee80211_hw *,
    struct ieee80211_vif *,
    void(*iterfunc)(struct ieee80211_hw *, struct ieee80211_vif *,
        struct ieee80211_sta *, struct ieee80211_key_conf *, void *),
    void *);
void linuxkpi_ieee80211_iterate_chan_contexts(struct ieee80211_hw *,
    void(*iterfunc)(struct ieee80211_hw *,
	struct ieee80211_chanctx_conf *, void *),
    void *);
void linuxkpi_ieee80211_iterate_stations_atomic(struct ieee80211_hw *,
   void (*iterfunc)(void *, struct ieee80211_sta *), void *);
void linuxkpi_ieee80211_scan_completed(struct ieee80211_hw *,
    struct cfg80211_scan_info *);
void linuxkpi_ieee80211_rx(struct ieee80211_hw *, struct sk_buff *,
    struct ieee80211_sta *, struct napi_struct *);
uint8_t linuxkpi_ieee80211_get_tid(struct ieee80211_hdr *, bool);
struct ieee80211_sta *linuxkpi_ieee80211_find_sta(struct ieee80211_vif *,
    const u8 *);
struct ieee80211_sta *linuxkpi_ieee80211_find_sta_by_ifaddr(
    struct ieee80211_hw *, const uint8_t *, const uint8_t *);
struct sk_buff *linuxkpi_ieee80211_tx_dequeue(struct ieee80211_hw *,
    struct ieee80211_txq *);
bool linuxkpi_ieee80211_is_ie_id_in_ie_buf(const u8, const u8 *, size_t);
bool linuxkpi_ieee80211_ie_advance(size_t *, const u8 *, size_t);
void linuxkpi_ieee80211_free_txskb(struct ieee80211_hw *, struct sk_buff *,
    int);
void linuxkpi_ieee80211_queue_delayed_work(struct ieee80211_hw *,
    struct delayed_work *, int);
void linuxkpi_ieee80211_queue_work(struct ieee80211_hw *, struct work_struct *);
struct sk_buff *linuxkpi_ieee80211_pspoll_get(struct ieee80211_hw *,
    struct ieee80211_vif *);
struct sk_buff *linuxkpi_ieee80211_nullfunc_get(struct ieee80211_hw *,
    struct ieee80211_vif *, bool);
void linuxkpi_ieee80211_txq_get_depth(struct ieee80211_txq *, unsigned long *,
    unsigned long *);
struct wireless_dev *linuxkpi_ieee80211_vif_to_wdev(struct ieee80211_vif *);
void linuxkpi_ieee80211_connection_loss(struct ieee80211_vif *);
void linuxkpi_ieee80211_beacon_loss(struct ieee80211_vif *);
struct sk_buff *linuxkpi_ieee80211_probereq_get(struct ieee80211_hw *,
    uint8_t *, uint8_t *, size_t, size_t);
void linuxkpi_ieee80211_tx_status(struct ieee80211_hw *, struct sk_buff *);

/* -------------------------------------------------------------------------- */

static __inline void
_ieee80211_hw_set(struct ieee80211_hw *hw, enum ieee80211_hw_flags flag)
{

	set_bit(flag, hw->flags);
}

static __inline bool
__ieee80211_hw_check(struct ieee80211_hw *hw, enum ieee80211_hw_flags flag)
{

	return (test_bit(flag, hw->flags));
}

/* They pass in shortened flag names; how confusingly inconsistent. */
#define	ieee80211_hw_set(_hw, _flag)					\
	_ieee80211_hw_set(_hw, IEEE80211_HW_ ## _flag)
#define	ieee80211_hw_check(_hw, _flag)					\
	__ieee80211_hw_check(_hw, IEEE80211_HW_ ## _flag)

/* XXX-BZ add CTASSERTS that size of struct is <= sizeof skb->cb. */
CTASSERT(sizeof(struct ieee80211_tx_info) <= sizeof(((struct sk_buff *)0)->cb));
#define	IEEE80211_SKB_CB(_skb)						\
	((struct ieee80211_tx_info *)((_skb)->cb))

CTASSERT(sizeof(struct ieee80211_rx_status) <= sizeof(((struct sk_buff *)0)->cb));
#define	IEEE80211_SKB_RXCB(_skb)					\
	((struct ieee80211_rx_status *)((_skb)->cb))

static __inline void
ieee80211_free_hw(struct ieee80211_hw *hw)
{

	linuxkpi_ieee80211_iffree(hw);

	if (hw->wiphy != NULL)
		wiphy_free(hw->wiphy);
	/* Note that *hw is not valid any longer after this. */

	IMPROVE();
}

static __inline struct ieee80211_hw *
ieee80211_alloc_hw(size_t priv_len, const struct ieee80211_ops *ops)
{

	return (linuxkpi_ieee80211_alloc_hw(priv_len, ops));
}

static __inline void
SET_IEEE80211_DEV(struct ieee80211_hw *hw, struct device *dev)
{

	set_wiphy_dev(hw->wiphy, dev);
	linuxkpi_set_ieee80211_dev(hw, dev_name(dev));

	IMPROVE();
}

static __inline int
ieee80211_register_hw(struct ieee80211_hw *hw)
{
	int error;

	error = wiphy_register(hw->wiphy);
	if (error != 0)
		return (error);

	/*
	 * At this point the driver has set all the options, flags, bands,
	 * ciphers, hw address(es), ... basically mac80211/cfg80211 hw/wiphy
	 * setup is done.
	 * We need to replicate a lot of information from here into net80211.
	 */
	error = linuxkpi_ieee80211_ifattach(hw);

	IMPROVE();

	return (error);
}

static __inline void
ieee80211_unregister_hw(struct ieee80211_hw *hw)
{

	wiphy_unregister(hw->wiphy);
	linuxkpi_ieee80211_ifdetach(hw);

	IMPROVE();
}

static __inline struct ieee80211_hw *
wiphy_to_ieee80211_hw(struct wiphy *wiphy)
{

	return (linuxkpi_wiphy_to_ieee80211_hw(wiphy));
}

/* -------------------------------------------------------------------------- */

static __inline bool
ieee80211_is_action(__le16 fc)
{
	__le16 v;

	fc &= htole16(IEEE80211_FC0_SUBTYPE_MASK | IEEE80211_FC0_TYPE_MASK);
	v = htole16(IEEE80211_FC0_SUBTYPE_ACTION | IEEE80211_FC0_TYPE_MGT);

	return (fc == v);
}

static __inline bool
ieee80211_is_probe_resp(__le16 fc)
{
	__le16 v;

	fc &= htole16(IEEE80211_FC0_SUBTYPE_MASK | IEEE80211_FC0_TYPE_MASK);
	v = htole16(IEEE80211_FC0_SUBTYPE_PROBE_RESP | IEEE80211_FC0_TYPE_MGT);

	return (fc == v);
}

static __inline bool
ieee80211_is_auth(__le16 fc)
{
	__le16 v;

	fc &= htole16(IEEE80211_FC0_SUBTYPE_MASK | IEEE80211_FC0_TYPE_MASK);
	v = htole16(IEEE80211_FC0_SUBTYPE_AUTH | IEEE80211_FC0_TYPE_MGT);

	return (fc == v);
}

static __inline bool
ieee80211_is_assoc_req(__le16 fc)
{
	__le16 v;

	fc &= htole16(IEEE80211_FC0_SUBTYPE_MASK | IEEE80211_FC0_TYPE_MASK);
	v = htole16(IEEE80211_FC0_SUBTYPE_ASSOC_REQ | IEEE80211_FC0_TYPE_MGT);

	return (fc == v);
}

static __inline bool
ieee80211_is_assoc_resp(__le16 fc)
{
	__le16 v;

	fc &= htole16(IEEE80211_FC0_SUBTYPE_MASK | IEEE80211_FC0_TYPE_MASK);
	v = htole16(IEEE80211_FC0_SUBTYPE_ASSOC_RESP | IEEE80211_FC0_TYPE_MGT);

	return (fc == v);
}

static __inline bool
ieee80211_is_reassoc_req(__le16 fc)
{
	__le16 v;

	fc &= htole16(IEEE80211_FC0_SUBTYPE_MASK | IEEE80211_FC0_TYPE_MASK);
	v = htole16(IEEE80211_FC0_SUBTYPE_REASSOC_REQ | IEEE80211_FC0_TYPE_MGT);

	return (fc == v);
}

static __inline bool
ieee80211_is_reassoc_resp(__le16 fc)
{
	__le16 v;

	fc &= htole16(IEEE80211_FC0_SUBTYPE_MASK | IEEE80211_FC0_TYPE_MASK);
	v = htole16(IEEE80211_FC0_SUBTYPE_REASSOC_RESP | IEEE80211_FC0_TYPE_MGT);

	return (fc == v);
}

static __inline bool
ieee80211_is_disassoc(__le16 fc)
{
	__le16 v;

	fc &= htole16(IEEE80211_FC0_SUBTYPE_MASK | IEEE80211_FC0_TYPE_MASK);
	v = htole16(IEEE80211_FC0_SUBTYPE_DISASSOC | IEEE80211_FC0_TYPE_MGT);

	return (fc == v);
}

static __inline bool
ieee80211_is_data_present(__le16 fc)
{
	__le16 v;

	/* If it is a data frame and NODATA is not present. */
	fc &= htole16(IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_NODATA);
	v = htole16(IEEE80211_FC0_TYPE_DATA);

	return (fc == v);
}

static __inline bool
ieee80211_is_deauth(__le16 fc)
{
	__le16 v;

	fc &= htole16(IEEE80211_FC0_SUBTYPE_MASK | IEEE80211_FC0_TYPE_MASK);
	v = htole16(IEEE80211_FC0_SUBTYPE_DEAUTH | IEEE80211_FC0_TYPE_MGT);

	return (fc == v);
}

static __inline bool
ieee80211_is_beacon(__le16 fc)
{
	__le16 v;

	/*
	 * For as much as I get it this comes in LE and unlike FreeBSD
	 * where we get the entire frame header and u8[], here we get the
	 * 9.2.4.1 Frame Control field only. Mask and compare.
	 */
	fc &= htole16(IEEE80211_FC0_SUBTYPE_MASK | IEEE80211_FC0_TYPE_MASK);
	v = htole16(IEEE80211_FC0_SUBTYPE_BEACON | IEEE80211_FC0_TYPE_MGT);

	return (fc == v);
}


static __inline bool
ieee80211_is_probe_req(__le16 fc)
{
	__le16 v;

	fc &= htole16(IEEE80211_FC0_SUBTYPE_MASK | IEEE80211_FC0_TYPE_MASK);
	v = htole16(IEEE80211_FC0_SUBTYPE_PROBE_REQ | IEEE80211_FC0_TYPE_MGT);

	return (fc == v);
}

static __inline bool
ieee80211_has_protected(__le16 fc)
{

	return (fc & htole16(IEEE80211_FC1_PROTECTED << 8));
}

static __inline bool
ieee80211_is_back_req(__le16 fc)
{
	__le16 v;

	fc &= htole16(IEEE80211_FC0_SUBTYPE_MASK | IEEE80211_FC0_TYPE_MASK);
	v = htole16(IEEE80211_FC0_SUBTYPE_BAR | IEEE80211_FC0_TYPE_CTL);

	return (fc == v);
}

static __inline bool
ieee80211_is_bufferable_mmpdu(__le16 fc)
{

	/* 11.2.2 Bufferable MMPDUs, 80211-2020. */
	/* XXX we do not care about IBSS yet. */

	if (!ieee80211_is_mgmt(fc))
		return (false);
	if (ieee80211_is_action(fc))		/* XXX FTM? */
		return (true);
	if (ieee80211_is_disassoc(fc))
		return (true);
	if (ieee80211_is_deauth(fc))
		return (true);

	return (false);
}

static __inline bool
ieee80211_is_nullfunc(__le16 fc)
{
	__le16 v;

	fc &= htole16(IEEE80211_FC0_SUBTYPE_MASK | IEEE80211_FC0_TYPE_MASK);
	v = htole16(IEEE80211_FC0_SUBTYPE_NODATA | IEEE80211_FC0_TYPE_DATA);

	return (fc == v);
}

static __inline bool
ieee80211_is_qos_nullfunc(__le16 fc)
{
	__le16 v;

	fc &= htole16(IEEE80211_FC0_SUBTYPE_MASK | IEEE80211_FC0_TYPE_MASK);
	v = htole16(IEEE80211_FC0_SUBTYPE_QOS_NULL | IEEE80211_FC0_TYPE_DATA);

	return (fc == v);
}

static __inline bool
ieee80211_is_any_nullfunc(__le16 fc)
{

	return (ieee80211_is_nullfunc(fc) || ieee80211_is_qos_nullfunc(fc));
}

static __inline bool
ieee80211_vif_is_mesh(struct ieee80211_vif *vif)
{
	TODO();
	return (false);
}

static __inline bool
ieee80211_is_frag(struct ieee80211_hdr *hdr)
{
	TODO();
	return (false);
}

static __inline bool
ieee80211_is_first_frag(__le16 fc)
{
	TODO();
	return (false);
}

static __inline bool
ieee80211_is_pspoll(__le16 fc)
{
	TODO();
	return (false);
}

static __inline bool
ieee80211_is_robust_mgmt_frame(struct sk_buff *skb)
{
	TODO();
	return (false);
}

static __inline bool
ieee80211_has_pm(__le16 fc)
{
	TODO();
	return (false);
}

static __inline bool
ieee80211_has_a4(__le16 fc)
{
	__le16 v;

	fc &= htole16((IEEE80211_FC1_DIR_TODS | IEEE80211_FC1_DIR_FROMDS) << 8);
	v = htole16((IEEE80211_FC1_DIR_TODS | IEEE80211_FC1_DIR_FROMDS) << 8);

	return (fc == v);
}

static __inline bool
ieee80211_has_order(__le16 fc)
{

	return (fc & htole16(IEEE80211_FC1_ORDER << 8));
}

static __inline bool
ieee80211_has_retry(__le16 fc)
{

	return (fc & htole16(IEEE80211_FC1_RETRY << 8));
}


static __inline bool
ieee80211_has_fromds(__le16 fc)
{

	return (fc & htole16(IEEE80211_FC1_DIR_FROMDS << 8));
}

static __inline bool
ieee80211_has_tods(__le16 fc)
{

	return (fc & htole16(IEEE80211_FC1_DIR_TODS << 8));
}

static __inline uint8_t *
ieee80211_get_SA(struct ieee80211_hdr *hdr)
{

	if (ieee80211_has_a4(hdr->frame_control))
		return (hdr->addr4);
	if (ieee80211_has_fromds(hdr->frame_control))
		return (hdr->addr3);
	return (hdr->addr2);
}

static __inline uint8_t *
ieee80211_get_DA(struct ieee80211_hdr *hdr)
{

	if (ieee80211_has_tods(hdr->frame_control))
		return (hdr->addr3);
	return (hdr->addr1);
}

static __inline bool
ieee80211_has_morefrags(__le16 fc)
{

	fc &= htole16(IEEE80211_FC1_MORE_FRAG << 8);
	return (fc != 0);
}

static __inline u8 *
ieee80211_get_qos_ctl(struct ieee80211_hdr *hdr)
{
        if (ieee80211_has_a4(hdr->frame_control))
                return (u8 *)hdr + 30;
        else
                return (u8 *)hdr + 24;
}

/* -------------------------------------------------------------------------- */
/* Receive functions (air/driver to mac80211/net80211). */


static __inline void
ieee80211_rx_napi(struct ieee80211_hw *hw, struct ieee80211_sta *sta,
    struct sk_buff *skb, struct napi_struct *napi)
{

	linuxkpi_ieee80211_rx(hw, skb, sta, napi);
}

static __inline void
ieee80211_rx_ni(struct ieee80211_hw *hw, struct sk_buff *skb)
{

	linuxkpi_ieee80211_rx(hw, skb, NULL, NULL);
}

static __inline void
ieee80211_rx_irqsafe(struct ieee80211_hw *hw, struct sk_buff *skb)
{

	linuxkpi_ieee80211_rx(hw, skb, NULL, NULL);
}

/* -------------------------------------------------------------------------- */

static __inline uint8_t
ieee80211_get_tid(struct ieee80211_hdr *hdr)
{

	return (linuxkpi_ieee80211_get_tid(hdr, false));
}

static __inline struct sk_buff *
ieee80211_beacon_get_tim(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    uint16_t *tim_offset, uint16_t *tim_len, uint32_t link_id)
{

	if (tim_offset != NULL)
		*tim_offset = 0;
	if (tim_len != NULL)
		*tim_len = 0;
	TODO();
	return (NULL);
}

static __inline void
ieee80211_iterate_active_interfaces_atomic(struct ieee80211_hw *hw,
    enum ieee80211_iface_iter flags,
    void(*iterfunc)(void *, uint8_t *, struct ieee80211_vif *),
    void *arg)
{

	flags |= IEEE80211_IFACE_ITER__ATOMIC;
	flags |= IEEE80211_IFACE_ITER__ACTIVE;
	linuxkpi_ieee80211_iterate_interfaces(hw, flags, iterfunc, arg);
}

static __inline void
ieee80211_iterate_active_interfaces(struct ieee80211_hw *hw,
    enum ieee80211_iface_iter flags,
    void(*iterfunc)(void *, uint8_t *, struct ieee80211_vif *),
    void *arg)
{

	flags |= IEEE80211_IFACE_ITER__ACTIVE;
	linuxkpi_ieee80211_iterate_interfaces(hw, flags, iterfunc, arg);
}

static __inline void
ieee80211_iterate_interfaces(struct ieee80211_hw *hw,
   enum ieee80211_iface_iter flags,
   void (*iterfunc)(void *, uint8_t *, struct ieee80211_vif *),
   void *arg)
{

	linuxkpi_ieee80211_iterate_interfaces(hw, flags, iterfunc, arg);
}

static __inline void
ieee80211_iter_keys(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    void(*iterfunc)(struct ieee80211_hw *, struct ieee80211_vif *,
        struct ieee80211_sta *, struct ieee80211_key_conf *, void *),
    void *arg)
{

	linuxkpi_ieee80211_iterate_keys(hw, vif, iterfunc, arg);
}

static __inline void
ieee80211_iter_keys_rcu(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    void(*iterfunc)(struct ieee80211_hw *, struct ieee80211_vif *,
        struct ieee80211_sta *, struct ieee80211_key_conf *, void *),
    void *arg)
{

	IMPROVE();	/* "rcu" */
	linuxkpi_ieee80211_iterate_keys(hw, vif, iterfunc, arg);
}

static __inline void
ieee80211_iter_chan_contexts_atomic(struct ieee80211_hw *hw,
    void(*iterfunc)(struct ieee80211_hw *, struct ieee80211_chanctx_conf *, void *),
    void *arg)
{

	linuxkpi_ieee80211_iterate_chan_contexts(hw, iterfunc, arg);
}

static __inline void
ieee80211_iterate_stations_atomic(struct ieee80211_hw *hw,
   void (*iterfunc)(void *, struct ieee80211_sta *), void *arg)
{

	linuxkpi_ieee80211_iterate_stations_atomic(hw, iterfunc, arg);
}

static __inline struct wireless_dev *
ieee80211_vif_to_wdev(struct ieee80211_vif *vif)
{

	return (linuxkpi_ieee80211_vif_to_wdev(vif));
}

static __inline struct sk_buff *
ieee80211_beacon_get_template(struct ieee80211_hw *hw,
    struct ieee80211_vif *vif, struct ieee80211_mutable_offsets *offs,
    uint32_t link_id)
{
	TODO();
	return (NULL);
}

static __inline void
ieee80211_beacon_loss(struct ieee80211_vif *vif)
{
	linuxkpi_ieee80211_beacon_loss(vif);
}

static __inline void
ieee80211_chswitch_done(struct ieee80211_vif *vif, bool t)
{
	TODO();
}

static __inline bool
ieee80211_csa_is_complete(struct ieee80211_vif *vif)
{
	TODO();
	return (false);
}

static __inline void
ieee80211_csa_set_counter(struct ieee80211_vif *vif, uint8_t counter)
{
	TODO();
}

static __inline int
ieee80211_csa_update_counter(struct ieee80211_vif *vif)
{
	TODO();
	return (-1);
}

static __inline void
ieee80211_csa_finish(struct ieee80211_vif *vif)
{
	TODO();
}

static __inline enum nl80211_iftype
ieee80211_vif_type_p2p(struct ieee80211_vif *vif)
{

	/* If we are not p2p enabled, just return the type. */
	if (!vif->p2p)
		return (vif->type);

	/* If we are p2p, depending on side, return type. */
	switch (vif->type) {
	case NL80211_IFTYPE_AP:
		return (NL80211_IFTYPE_P2P_GO);
	case NL80211_IFTYPE_STATION:
		return (NL80211_IFTYPE_P2P_CLIENT);
	default:
		fallthrough;
	}
	return (vif->type);
}

static __inline unsigned long
ieee80211_tu_to_usec(unsigned long tu)
{

	return (tu * IEEE80211_DUR_TU);
}


static __inline int
ieee80211_action_contains_tpc(struct sk_buff *skb)
{
	TODO();
	return (0);
}

static __inline void
ieee80211_connection_loss(struct ieee80211_vif *vif)
{

	linuxkpi_ieee80211_connection_loss(vif);
}

static __inline struct ieee80211_sta *
ieee80211_find_sta(struct ieee80211_vif *vif, const u8 *peer)
{

	return (linuxkpi_ieee80211_find_sta(vif, peer));
}

static __inline struct ieee80211_sta *
ieee80211_find_sta_by_ifaddr(struct ieee80211_hw *hw, const uint8_t *addr,
    const uint8_t *ourvifaddr)
{

	return (linuxkpi_ieee80211_find_sta_by_ifaddr(hw, addr, ourvifaddr));
}


static __inline void
ieee80211_get_tkip_p2k(struct ieee80211_key_conf *keyconf,
    struct sk_buff *skb_frag, u8 *key)
{
	TODO();
}

static __inline void
ieee80211_get_tkip_rx_p1k(struct ieee80211_key_conf *keyconf,
    const u8 *addr, uint32_t iv32, u16 *p1k)
{
	TODO();
}

static __inline size_t
ieee80211_ie_split(const u8 *ies, size_t ies_len,
    const u8 *ie_ids, size_t ie_ids_len, size_t start)
{
	size_t x;

	x = start;

	/* XXX FIXME, we need to deal with "Element ID Extension" */
	while (x < ies_len) {

		/* Is this IE[s] one of the ie_ids? */
		if (!linuxkpi_ieee80211_is_ie_id_in_ie_buf(ies[x],
		    ie_ids, ie_ids_len))
			break;

		if (!linuxkpi_ieee80211_ie_advance(&x, ies, ies_len))
			break;
	}

	return (x);
}

static __inline void
ieee80211_request_smps(struct ieee80211_vif *vif, enum ieee80211_smps_mode smps)
{
	static const char *smps_mode_name[] = {
		"SMPS_OFF",
		"SMPS_STATIC",
		"SMPS_DYNAMIC",
		"SMPS_AUTOMATIC",
		"SMPS_NUM_MODES"
	};

	if (linuxkpi_debug_80211 & D80211_TODO)
		printf("%s:%d: XXX LKPI80211 TODO smps %d %s\n",
		    __func__, __LINE__, smps, smps_mode_name[smps]);
}

static __inline void
ieee80211_tdls_oper_request(struct ieee80211_vif *vif, uint8_t *addr,
    enum nl80211_tdls_operation oper, enum ieee80211_reason_code code,
    gfp_t gfp)
{
	TODO();
}

static __inline void
ieee80211_stop_queues(struct ieee80211_hw *hw)
{
	TODO();
}

static __inline void
ieee80211_wake_queues(struct ieee80211_hw *hw)
{
	TODO();
}

static __inline void
wiphy_rfkill_set_hw_state(struct wiphy *wiphy, bool state)
{
	TODO();
}

static __inline void
ieee80211_free_txskb(struct ieee80211_hw *hw, struct sk_buff *skb)
{
	IMPROVE();

	/*
	 * This is called on transmit failure.
	 * Use a not-so-random random high status error so we can distinguish
	 * it from normal low values flying around in net80211 ("ETX").
	 */
	linuxkpi_ieee80211_free_txskb(hw, skb, 0x455458);
}

static __inline void
ieee80211_restart_hw(struct ieee80211_hw *hw)
{
	TODO();
}

static __inline void
ieee80211_ready_on_channel(struct ieee80211_hw *hw)
{
	TODO();
/* XXX-BZ We need to see that. */
}

static __inline void
ieee80211_remain_on_channel_expired(struct ieee80211_hw *hw)
{
	TODO();
}

static __inline void
ieee80211_cqm_rssi_notify(struct ieee80211_vif *vif,
    enum nl80211_cqm_rssi_threshold_event crte, int sig, gfp_t gfp)
{
	TODO();
}

static __inline void
ieee80211_mark_rx_ba_filtered_frames(struct ieee80211_sta *sta, uint8_t tid,
    uint32_t ssn, uint64_t bitmap, uint16_t received_mpdu)
{
	TODO();
}

static __inline bool
ieee80211_sn_less(uint16_t sn1, uint16_t sn2)
{
	TODO();
	return (false);
}

static __inline uint16_t
ieee80211_sn_inc(uint16_t sn)
{
	TODO();
	return (sn + 1);
}

static __inline uint16_t
ieee80211_sn_add(uint16_t sn, uint16_t a)
{
	TODO();
	return (sn + a);
}

static __inline void
ieee80211_stop_rx_ba_session(struct ieee80211_vif *vif, uint32_t x, uint8_t *addr)
{
	TODO();
}

static __inline void
ieee80211_rate_set_vht(struct ieee80211_tx_rate *r, uint32_t f1, uint32_t f2)
{
	TODO();
}

static __inline void
ieee80211_reserve_tid(struct ieee80211_sta *sta, uint8_t tid)
{
	TODO();
}

static __inline void
ieee80211_unreserve_tid(struct ieee80211_sta *sta, uint8_t tid)
{
	TODO();
}

static __inline void
ieee80211_rx_ba_timer_expired(struct ieee80211_vif *vif, uint8_t *addr,
    uint8_t tid)
{
	TODO();
}

static __inline void
ieee80211_send_eosp_nullfunc(struct ieee80211_sta *sta, uint8_t tid)
{
	TODO();
}

static __inline uint16_t
ieee80211_sn_sub(uint16_t sa, uint16_t sb)
{

	return ((sa - sb) &
	    (IEEE80211_SEQ_SEQ_MASK >> IEEE80211_SEQ_SEQ_SHIFT));
}

static __inline void
ieee80211_sta_block_awake(struct ieee80211_hw *hw, struct ieee80211_sta *sta,
    bool disable)
{
	TODO();
}

static __inline void
ieee80211_sta_ps_transition(struct ieee80211_sta *sta, bool sleeping)
{
	TODO();
}

static __inline void
ieee80211_sta_pspoll(struct ieee80211_sta *sta)
{
	TODO();
}

static __inline void
ieee80211_sta_uapsd_trigger(struct ieee80211_sta *sta, int ntids)
{
	TODO();
}

static __inline void
ieee80211_start_tx_ba_cb_irqsafe(struct ieee80211_vif *vif, uint8_t *addr,
    uint8_t tid)
{
	TODO();
}

static __inline void
ieee80211_tkip_add_iv(u8 *crypto_hdr, struct ieee80211_key_conf *keyconf,
    uint64_t pn)
{
	TODO();
}

static __inline struct sk_buff *
ieee80211_tx_dequeue(struct ieee80211_hw *hw, struct ieee80211_txq *txq)
{

	return (linuxkpi_ieee80211_tx_dequeue(hw, txq));
}

static __inline void
ieee80211_update_mu_groups(struct ieee80211_vif *vif, uint8_t *ms, uint8_t *up)
{
	TODO();
}

static __inline void
ieee80211_sta_set_buffered(struct ieee80211_sta *sta, uint8_t tid, bool t)
{
	TODO();
}

static __inline void
ieee80211_tx_status(struct ieee80211_hw *hw, struct sk_buff *skb)
{

	linuxkpi_ieee80211_tx_status(hw, skb);
}

static __inline void
ieee80211_get_key_rx_seq(struct ieee80211_key_conf *keyconf, uint8_t tid,
    struct ieee80211_key_seq *seq)
{
	TODO();
}

static __inline void
ieee80211_sched_scan_results(struct ieee80211_hw *hw)
{
	TODO();
}

static __inline void
ieee80211_sta_eosp(struct ieee80211_sta *sta)
{
	TODO();
}

static __inline void
ieee80211_stop_tx_ba_cb_irqsafe(struct ieee80211_vif *vif, uint8_t *addr,
    uint8_t tid)
{
	TODO();
}

static __inline void
ieee80211_sched_scan_stopped(struct ieee80211_hw *hw)
{
	TODO();
}

static __inline void
ieee80211_scan_completed(struct ieee80211_hw *hw,
    struct cfg80211_scan_info *info)
{

	linuxkpi_ieee80211_scan_completed(hw, info);
}

static __inline struct sk_buff *
ieee80211_beacon_get(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{
	TODO();
	return (NULL);
}

static __inline struct sk_buff *
ieee80211_pspoll_get(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{

	/* Only STA needs this.  Otherwise return NULL and panic bad drivers. */
	if (vif->type != NL80211_IFTYPE_STATION)
		return (NULL);

	return (linuxkpi_ieee80211_pspoll_get(hw, vif));
}

static __inline struct sk_buff *
ieee80211_proberesp_get(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{
	TODO();
	return (NULL);
}

static __inline struct sk_buff *
ieee80211_nullfunc_get(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    bool qos)
{

	/* Only STA needs this.  Otherwise return NULL and panic bad drivers. */
	if (vif->type != NL80211_IFTYPE_STATION)
		return (NULL);

	return (linuxkpi_ieee80211_nullfunc_get(hw, vif, qos));
}

static __inline struct sk_buff *
ieee80211_probereq_get(struct ieee80211_hw *hw, uint8_t *addr,
    uint8_t *ssid, size_t ssid_len, size_t tailroom)
{

	return (linuxkpi_ieee80211_probereq_get(hw, addr, ssid, ssid_len,
	    tailroom));
}

static __inline void
ieee80211_queue_delayed_work(struct ieee80211_hw *hw, struct delayed_work *w,
    int delay)
{

	linuxkpi_ieee80211_queue_delayed_work(hw, w, delay);
}

static __inline void
ieee80211_queue_work(struct ieee80211_hw *hw, struct work_struct *w)
{

	linuxkpi_ieee80211_queue_work(hw, w);
}

static __inline void
ieee80211_stop_queue(struct ieee80211_hw *hw, uint16_t q)
{
	TODO();
}

static __inline void
ieee80211_wake_queue(struct ieee80211_hw *hw, uint16_t q)
{
	TODO();
}

static __inline void
ieee80211_tx_status_irqsafe(struct ieee80211_hw *hw, struct sk_buff *skb)
{
	IMPROVE();
	ieee80211_tx_status(hw, skb);
}

static __inline void
ieee80211_tx_status_ni(struct ieee80211_hw *hw, struct sk_buff *skb)
{
	IMPROVE();
	ieee80211_tx_status(hw, skb);
}

static __inline int
ieee80211_start_tx_ba_session(struct ieee80211_sta *sta, uint8_t tid, int x)
{
	TODO();
	return (-EINVAL);
}

static __inline int
ieee80211_stop_tx_ba_session(struct ieee80211_sta *sta, uint8_t tid)
{
	TODO();
	return (-EINVAL);
}

static __inline void
ieee80211_tx_info_clear_status(struct ieee80211_tx_info *info)
{
	int i;

	/*
	 * Apparently clearing flags and some other fields is not right.
	 * Given the function is called "status" we work on that part of
	 * the union.
	 */
	for (i = 0; i < nitems(info->status.rates); i++)
		info->status.rates[i].count = 0;
	/*
	 * Unclear if ack_signal should be included or not but we clear the
	 * "valid" bool so this field is no longer valid.
	 */
	memset(&info->status.ack_signal, 0, sizeof(*info) -
	    offsetof(struct ieee80211_tx_info, status.ack_signal));
}

static __inline void
ieee80211_txq_get_depth(struct ieee80211_txq *txq, unsigned long *frame_cnt,
    unsigned long *byte_cnt)
{

	if (frame_cnt == NULL && byte_cnt == NULL)
		return;

	linuxkpi_ieee80211_txq_get_depth(txq, frame_cnt, byte_cnt);
}

static __inline int
rate_lowest_index(struct ieee80211_supported_band *band,
    struct ieee80211_sta *sta)
{
	IMPROVE();
	return (0);
}


static __inline void
SET_IEEE80211_PERM_ADDR	(struct ieee80211_hw *hw, uint8_t *addr)
{

	ether_addr_copy(hw->wiphy->perm_addr, addr);
}

static __inline uint8_t *
ieee80211_bss_get_ie(struct cfg80211_bss *bss, uint32_t eid)
{
	TODO();
	return (NULL);
}

static __inline void
ieee80211_report_low_ack(struct ieee80211_sta *sta, int x)
{
	TODO();
}

static __inline void
ieee80211_start_rx_ba_session_offl(struct ieee80211_vif *vif, uint8_t *addr,
    uint8_t tid)
{
	TODO();
}

static __inline void
ieee80211_stop_rx_ba_session_offl(struct ieee80211_vif *vif, uint8_t *addr,
    uint8_t tid)
{
	TODO();
}

static __inline struct sk_buff *
ieee80211_tx_dequeue_ni(struct ieee80211_hw *hw, struct ieee80211_txq *txq)
{
	TODO();
	return (NULL);
}

static __inline void
ieee80211_tx_rate_update(struct ieee80211_hw *hw, struct ieee80211_sta *sta,
    struct ieee80211_tx_info *info)
{
	TODO();
}

static __inline bool
ieee80211_txq_may_transmit(struct ieee80211_hw *hw, struct ieee80211_txq *txq)
{
	TODO();
	return (false);
}

static __inline struct ieee80211_txq *
ieee80211_next_txq(struct ieee80211_hw *hw, uint32_t ac)
{
	TODO();
	return (NULL);
}

static __inline void
ieee80211_radar_detected(struct ieee80211_hw *hw)
{
	TODO();
}

static __inline void
ieee80211_sta_register_airtime(struct ieee80211_sta *sta,
    uint8_t tid, uint32_t duration, int x)
{
	TODO();
}


static __inline void
ieee80211_return_txq(struct ieee80211_hw *hw,
    struct ieee80211_txq *txq, bool _t)
{
	TODO();
}

static __inline void
ieee80211_txq_schedule_end(struct ieee80211_hw *hw, uint32_t ac)
{
	TODO();
}

static __inline void
ieee80211_txq_schedule_start(struct ieee80211_hw *hw, uint32_t ac)
{
	TODO();
}

static __inline void
ieee80211_schedule_txq(struct ieee80211_hw *hw, struct ieee80211_txq *txq)
{
	TODO();
}

static __inline void
ieee80211_beacon_set_cntdwn(struct ieee80211_vif *vif, u8 counter)
{
	TODO();
}

static __inline int
ieee80211_beacon_update_cntdwn(struct ieee80211_vif *vif)
{
	TODO();
	return (-1);
}

static __inline int
ieee80211_get_vht_max_nss(struct ieee80211_vht_cap *vht_cap, uint32_t chanwidth,
    int x, bool t, int nss)
{
	TODO();
	return (-1);
}

static __inline bool
ieee80211_beacon_cntdwn_is_complete(struct ieee80211_vif *vif)
{
	TODO();
	return (true);
}

static __inline void
ieee80211_disconnect(struct ieee80211_vif *vif, bool _x)
{
	TODO();
}

static __inline void
ieee80211_channel_switch_disconnect(struct ieee80211_vif *vif, bool _x)
{
	TODO();
}

static __inline const struct ieee80211_sta_he_cap *
ieee80211_get_he_iftype_cap(const struct ieee80211_supported_band *band,
    enum nl80211_iftype type)
{
	TODO();
        return (NULL);
}

static __inline void
ieee80211_key_mic_failure(struct ieee80211_key_conf *key)
{
	TODO();
}

static __inline void
ieee80211_key_replay(struct ieee80211_key_conf *key)
{
	TODO();
}

static __inline uint32_t
ieee80211_calc_rx_airtime(struct ieee80211_hw *hw,
    struct ieee80211_rx_status *rxstat, int len)
{
	TODO();
	return (0);
}

static __inline void
ieee80211_get_tx_rates(struct ieee80211_vif *vif, struct ieee80211_sta *sta,
    struct sk_buff *skb, struct ieee80211_tx_rate *txrate, int nrates)
{
	TODO();
}

static __inline void
ieee80211_rx_list(struct ieee80211_hw *hw, struct ieee80211_sta *sta,
    struct sk_buff *skb, struct list_head *list)
{
	TODO();
}

static __inline void
ieee80211_tx_status_ext(struct ieee80211_hw *hw,
    struct ieee80211_tx_status *txstat)
{
	TODO();
}

static __inline const struct element *
ieee80211_bss_get_elem(struct cfg80211_bss *bss, uint32_t eid)
{
	TODO();
	return (NULL);
}

static __inline void
ieee80211_color_change_finish(struct ieee80211_vif *vif)
{
	TODO();
}

static __inline struct sk_buff *
ieee80211_get_fils_discovery_tmpl(struct ieee80211_hw *hw,
    struct ieee80211_vif *vif)
{
	TODO();
	return (NULL);
}

static __inline struct sk_buff *
ieee80211_get_unsol_bcast_probe_resp_tmpl(struct ieee80211_hw *hw,
    struct ieee80211_vif *vif)
{
	TODO();
	return (NULL);
}

static __inline void
linuxkpi_ieee80211_send_bar(struct ieee80211_vif *vif, uint8_t *ra, uint16_t tid,
    uint16_t ssn)
{
	TODO();
}

#define	ieee80211_send_bar(_v, _r, _t, _s)				\
    linuxkpi_ieee80211_send_bar(_v, _r, _t, _s)

#endif	/* _LINUXKPI_NET_MAC80211_H */
