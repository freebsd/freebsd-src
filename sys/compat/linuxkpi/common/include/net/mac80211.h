/*-
 * Copyright (c) 2020-2025 The FreeBSD Foundation
 * Copyright (c) 2020-2025 Bjoern A. Zeeb
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
 */

#ifndef	_LINUXKPI_NET_MAC80211_H
#define	_LINUXKPI_NET_MAC80211_H

#include <sys/types.h>

#include <asm/atomic64.h>
#include <linux/bitops.h>
#include <linux/bitfield.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/workqueue.h>
#include <linux/dcache.h>
#include <linux/ieee80211.h>
#include <net/cfg80211.h>
#include <net/if_inet6.h>

#define	ARPHRD_IEEE80211_RADIOTAP		__LINE__ /* XXX TODO brcmfmac */

#define	WLAN_OUI_MICROSOFT			(0x0050F2)
#define	WLAN_OUI_TYPE_MICROSOFT_WPA		(1)
#define	WLAN_OUI_TYPE_MICROSOFT_TPC		(8)
#define	WLAN_OUI_TYPE_WFA_P2P			(9)
#define	WLAN_OUI_WFA				(0x506F9A)

#define	IEEE80211_LINK_UNSPECIFIED		0x0f

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
	FIF_MCAST_ACTION		= BIT(7),

	/* Must stay last. */
	FIF_FLAGS_MASK			= BIT(8)-1,
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
	BSS_CHANGED_EHT_PUNCTURING	= BIT(31),
	BSS_CHANGED_MLD_VALID_LINKS	= BIT_ULL(32),
	BSS_CHANGED_MLD_TTLM		= BIT_ULL(33),
	BSS_CHANGED_TPE			= BIT_ULL(34),
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
#define	WLAN_AKM_SUITE_FT_OVER_SAE	WLAN_AKM_SUITE(9)
/* AP peer key				10 */
/* 802.1x suite B			11 */
/* 802.1x suite B 384			12 */
/* FTo802.1x 384			13 */
/* Reserved				14-255 */
/* Apparently 11ax defines more. Seen (19,20) mentioned. */

#define	TKIP_PN_TO_IV16(_x)		((uint16_t)(_x & 0xffff))
#define	TKIP_PN_TO_IV32(_x)		((uint32_t)((_x >> 16) & 0xffffffff))

enum ieee80211_neg_ttlm_res {
	NEG_TTLM_RES_ACCEPT,
	NEG_TTLM_RES_REJECT,
};

#define	IEEE80211_TTLM_NUM_TIDS	8
struct ieee80211_neg_ttlm {
	uint16_t downlink[IEEE80211_TTLM_NUM_TIDS];
	uint16_t uplink[IEEE80211_TTLM_NUM_TIDS];
};

/* 802.11-2020 9.4.2.55.3 A-MPDU Parameters field */
#define	IEEE80211_HT_AMPDU_PARM_FACTOR		0x3
#define	IEEE80211_HT_AMPDU_PARM_DENSITY_SHIFT	2
#define	IEEE80211_HT_AMPDU_PARM_DENSITY		(0x7 << IEEE80211_HT_AMPDU_PARM_DENSITY_SHIFT)

struct ieee80211_sta;

struct ieee80211_ampdu_params {
	struct ieee80211_sta			*sta;
	enum ieee80211_ampdu_mlme_action	action;
	uint16_t				buf_size;
	uint16_t				timeout;
	uint16_t				ssn;
	uint8_t					tid;
	bool					amsdu;
};

struct ieee80211_bar {
	/* TODO FIXME */
	int		control, start_seq_num;
	uint8_t		*ra;
	uint16_t	frame_control;
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

struct ieee80211_chanctx_conf {
	struct cfg80211_chan_def		def;
	struct cfg80211_chan_def		min_def;
	struct cfg80211_chan_def		ap;

	uint8_t					rx_chains_dynamic;
	uint8_t					rx_chains_static;
	bool					radar_enabled;

	/* Must stay last. */
	uint8_t					drv_priv[0] __aligned(CACHE_LINE_SIZE);
};

struct ieee80211_rate_status {
	struct rate_info			rate_idx;
	uint8_t					try_count;
};

struct ieee80211_ema_beacons {
	uint8_t					cnt;
	struct {
		struct sk_buff			*skb;
		struct ieee80211_mutable_offsets offs;
	} bcn[0];
};

struct ieee80211_chanreq {
	struct cfg80211_chan_def oper;
};

#define	WLAN_MEMBERSHIP_LEN			(8)
#define	WLAN_USER_POSITION_LEN			(16)

/*
 * 802.11ac-2013, 8.4.2.164 VHT Transmit Power Envelope element
 * 802.11-???? ?
 */
struct ieee80211_parsed_tpe_eirp {
	int8_t					power[5];
	uint8_t					count;
	bool					valid;
};
struct ieee80211_parsed_tpe_psd {
	int8_t					power[16];
	uint8_t					count;
	bool					valid;
};
struct ieee80211_parsed_tpe {
	/* We see access to [0] so assume at least 2. */
	struct ieee80211_parsed_tpe_eirp	max_local[2];
	struct ieee80211_parsed_tpe_eirp	max_reg_client[2];
	struct ieee80211_parsed_tpe_psd		psd_local[2];
	struct ieee80211_parsed_tpe_psd		psd_reg_client[2];
};

struct ieee80211_bss_conf {
	/* TODO FIXME */
	struct ieee80211_vif			*vif;
	struct cfg80211_bss			*bss;
	const uint8_t				*bssid;
	uint8_t					addr[ETH_ALEN];
	uint8_t					link_id;
	uint8_t					_pad0;
	uint8_t					transmitter_bssid[ETH_ALEN];
	struct ieee80211_ftm_responder_params	*ftmr_params;
	struct ieee80211_p2p_noa_attr		p2p_noa_attr;
	struct ieee80211_chanreq		chanreq;
	__be32					arp_addr_list[1];	/* XXX TODO */
	struct ieee80211_rate			*beacon_rate;
	struct {
		uint8_t membership[WLAN_MEMBERSHIP_LEN];
		uint8_t position[WLAN_USER_POSITION_LEN];
	}  mu_group;
	struct {
		uint32_t			params;
		/* single field struct? */
	} he_oper;
	struct cfg80211_he_bss_color		he_bss_color;
	struct ieee80211_he_obss_pd		he_obss_pd;

	bool					ht_ldpc;
	bool					vht_ldpc;
	bool					he_ldpc;
	bool					vht_mu_beamformee;
	bool					vht_mu_beamformer;
	bool					vht_su_beamformee;
	bool					vht_su_beamformer;
	bool					he_mu_beamformer;
	bool					he_su_beamformee;
	bool					he_su_beamformer;
	bool					he_full_ul_mumimo;
	bool					eht_su_beamformee;
	bool					eht_su_beamformer;
	bool					eht_mu_beamformer;

	uint16_t				ht_operation_mode;
	int					arp_addr_cnt;
	uint16_t				eht_puncturing;

	uint8_t					dtim_period;
	uint8_t					sync_dtim_count;
	uint8_t					bss_param_ch_cnt_link_id;
	bool					qos;
	bool					twt_broadcast;
	bool					use_cts_prot;
	bool					use_short_preamble;
	bool					use_short_slot;
	bool					he_support;
	bool					eht_support;
	bool					csa_active;
	bool					mu_mimo_owner;
	bool					color_change_active;
	uint32_t				sync_device_ts;
	uint64_t				sync_tsf;
	uint16_t				beacon_int;
	int16_t					txpower;
	uint32_t				basic_rates;
	int					mcast_rate[NUM_NL80211_BANDS];
	enum ieee80211_ap_reg_power 		power_type;
	struct cfg80211_bitrate_mask		beacon_tx_rate;
	struct mac80211_fils_discovery		fils_discovery;
	struct ieee80211_chanctx_conf		*chanctx_conf;
	struct ieee80211_vif			*mbssid_tx_vif;
	struct ieee80211_parsed_tpe		tpe;

	int		ack_enabled, bssid_index, bssid_indicator, cqm_rssi_hyst, cqm_rssi_thold, ema_ap, frame_time_rts_th, ftm_responder;
	int		htc_trig_based_pkt_ext;
	int		multi_sta_back_32bit, nontransmitted;
	int		profile_periodicity;
	int		twt_requester, uora_exists, uora_ocw_range;
	int		assoc_capability, enable_beacon, hidden_ssid, ibss_joined, twt_protected;
	int		twt_responder, unsol_bcast_probe_resp_interval;
};

struct ieee80211_channel_switch {
	/* TODO FIXME */
	int		block_tx, count, delay, device_timestamp, timestamp;
	uint8_t					link_id;
	struct cfg80211_chan_def		chandef;
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
	IEEE80211_HW_DETECTS_COLOR_COLLISION,
	IEEE80211_HW_DISALLOW_PUNCTURING,
	IEEE80211_HW_DISALLOW_PUNCTURING_5GHZ,
	IEEE80211_HW_TX_STATUS_NO_AMPDU_LEN,
	IEEE80211_HW_HANDLES_QUIET_CSA,
	IEEE80211_HW_NO_VIRTUAL_MONITOR,

	/* Keep last. */
	NUM_IEEE80211_HW_FLAGS
};

struct ieee80211_hw {

	struct wiphy			*wiphy;

	/* TODO FIXME */
	int		extra_tx_headroom, weight_multiplier;
	int		max_rate_tries, max_rates, max_report_rates;
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
	uint16_t			max_rx_aggregation_subframes;
	uint16_t			max_tx_aggregation_subframes;
	uint16_t			max_tx_fragments;
	uint16_t			max_listen_interval;
	uint32_t			extra_beacon_tailroom;
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
	IEEE80211_KEY_FLAG_RESERVE_TAILROOM	= BIT(8),
	IEEE80211_KEY_FLAG_SPP_AMSDU		= BIT(9),
};

#define	IEEE80211_KEY_FLAG_BITS						\
	"\20\1GENERATE_IV\2GENERATE_MMIC\3PAIRWISE\4PUT_IV_SPACE"	\
	"\5PUT_MIC_SPACE\6SW_MGMT_TX\7GENERATE_IV_MGMT\10GENERATE_MMIE"	\
	"\11RESERVE_TAILROOM\12SPP_AMSDU"

struct ieee80211_key_conf {
#if defined(__FreeBSD__)
	const struct ieee80211_key	*_k;		/* backpointer to net80211 */
#endif
	atomic64_t			tx_pn;
	uint32_t			cipher;
	uint8_t				icv_len;	/* __unused nowadays? */
	uint8_t				iv_len;
	uint8_t				hw_key_idx;	/* Set by drv. */
	uint8_t				keyidx;
	uint16_t			flags;
	int8_t				link_id;	/* signed! */
	uint8_t				keylen;
	uint8_t				key[0];		/* Must stay last! */
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
			uint8_t		pn[IEEE80211_GCMP_PN_LEN];
		} gcmp;
		struct {
			uint8_t		pn[IEEE80211_CMAC_PN_LEN];
		} aes_cmac;
		struct {
			uint8_t		pn[IEEE80211_GMAC_PN_LEN];
		} aes_gmac;
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
	RX_FLAG_MACTIME			= BIT(8) | BIT(9),
	RX_FLAG_MACTIME_PLCP_START	= 1 << 8,
	RX_FLAG_MACTIME_START		= 2 << 8,
	RX_FLAG_MACTIME_END		= 3 << 8,
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
				/*	= BIT(24), */
	RX_FLAG_ONLY_MONITOR		= BIT(25),
	RX_FLAG_SKIP_MONITOR		= BIT(26),
	RX_FLAG_8023			= BIT(27),
	RX_FLAG_RADIOTAP_TLV_AT_END	= BIT(28),
				/*	= BIT(29), */
	RX_FLAG_MACTIME_IS_RTAP_TS64	= BIT(30),
	RX_FLAG_FAILED_PLCP_CRC		= BIT(31),
};

#define	IEEE80211_RX_STATUS_FLAGS_BITS					\
	"\20\1ALLOW_SAME_PN\2AMPDU_DETAILS\3AMPDU_EOF_BIT\4AMPDU_EOF_BIT_KNOWN" \
	"\5DECRYPTED\6DUP_VALIDATED\7FAILED_FCS_CRC\10ICV_STRIPPED" \
	"\11MACTIME_PLCP_START\12MACTIME_START\13MIC_STRIPPED" \
	"\14MMIC_ERROR\15MMIC_STRIPPED\16NO_PSDU\17PN_VALIDATED" \
	"\20RADIOTAP_HE\21RADIOTAP_HE_MU\22RADIOTAP_LSIG\23RADIOTAP_VENDOR_DATA" \
	"\24NO_SIGNAL_VAL\25IV_STRIPPED\26AMPDU_IS_LAST\27AMPDU_LAST_KNOWN" \
	"\30AMSDU_MORE\31MACTIME_END\32ONLY_MONITOR\33SKIP_MONITOR" \
	"\348023\35RADIOTAP_TLV_AT_END\36MACTIME\37MACTIME_IS_RTAP_TS64" \
	"\40FAILED_PLCP_CRC"

enum mac80211_rx_encoding {
	RX_ENC_LEGACY		= 0,
	RX_ENC_HT,
	RX_ENC_VHT,
	RX_ENC_HE,
	RX_ENC_EHT,
};

struct ieee80211_rx_status {
	/* TODO FIXME, this is too large. Over-reduce types to u8 where possible. */
	union {
		uint64_t			boottime_ns;
		int64_t				ack_tx_hwtstamp;
	};
	uint64_t			mactime;
	uint32_t			device_timestamp;
	enum ieee80211_rx_status_flags	flag;
	uint16_t			freq;
	uint8_t				encoding:3, bw:4;	/* enum mac80211_rx_encoding, rate_info_bw */	/* See mt76.h */
	uint8_t				ampdu_reference;
	uint8_t				band;
	uint8_t				chains;
	int8_t				chain_signal[IEEE80211_MAX_CHAINS];
	int8_t				signal;
	uint8_t				enc_flags;
	union {
		struct {
			uint8_t		he_ru:3;	/* nl80211::enum nl80211_he_ru_alloc */
			uint8_t		he_gi:2;	/* nl80211::enum nl80211_he_gi */
			uint8_t		he_dcm:1;
		};
		struct {
			uint8_t		ru:4;		/* nl80211::enum nl80211_eht_ru_alloc */
			uint8_t		gi:2;		/* nl80211::enum nl80211_eht_gi */
		} eht;
	};
	bool				link_valid;
	uint8_t				link_id;	/* very incosistent sizes? */
	uint8_t				zero_length_psdu_type;
	uint8_t				nss;
	uint8_t				rate_idx;
};

struct ieee80211_tx_status {
	struct ieee80211_sta		*sta;
	struct ieee80211_tx_info	*info;
	int64_t				ack_hwtstamp;

	u8				n_rates;
	struct ieee80211_rate_status	*rates;

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
		uint8_t			idx;
		uint8_t			count;
		uint16_t		flags;
	} rate[4];		/* XXX what is the real number? */
};

struct ieee80211_sta_txpwr {
	/* XXX TODO */
	enum nl80211_tx_power_setting	type;
	short				power;
};

#define	IEEE80211_NUM_TIDS			16	/* net80211::WME_NUM_TID */
struct ieee80211_sta_agg {
	uint16_t				max_amsdu_len;
	uint16_t				max_rc_amsdu_len;
	uint16_t				max_tid_amsdu_len[IEEE80211_NUM_TIDS];
};

struct ieee80211_link_sta {
	struct ieee80211_sta			*sta;
	uint8_t					addr[ETH_ALEN];
	uint8_t					link_id;
	uint32_t				supp_rates[NUM_NL80211_BANDS];
	struct ieee80211_sta_ht_cap		ht_cap;
	struct ieee80211_sta_vht_cap		vht_cap;
	struct ieee80211_sta_he_cap		he_cap;
	struct ieee80211_he_6ghz_capa		he_6ghz_capa;
	struct ieee80211_sta_eht_cap		eht_cap;
	uint8_t					rx_nss;
	enum ieee80211_sta_rx_bandwidth		bandwidth;
	enum ieee80211_smps_mode		smps_mode;
	struct ieee80211_sta_agg		agg;
	struct ieee80211_sta_txpwr		txpwr;
};

struct ieee80211_sta {
	/* TODO FIXME */
	int		max_amsdu_subframes;
	int		mfp, smps_mode, tdls, tdls_initiator;
	struct ieee80211_txq			*txq[IEEE80211_NUM_TIDS + 1];	/* iwlwifi: 8 and adds +1 to tid_data, net80211::IEEE80211_TID_SIZE */
	struct ieee80211_sta_rates		*rates;	/* some rcu thing? */
	uint8_t					addr[ETH_ALEN];
	uint16_t				aid;
	bool					wme;
	bool					mlo;
	uint8_t					max_sp;
	uint8_t					uapsd_queues;
	uint16_t				valid_links;

	struct ieee80211_link_sta		deflink;
	struct ieee80211_link_sta		*link[IEEE80211_MLD_MAX_NUM_LINKS];	/* rcu? */

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
#if defined(LINUXKPI_VERSION) && (LINUXKPI_VERSION < 60600)	/* v6.6 */
	IEEE80211_VIF_DISABLE_SMPS_OVERRIDE	= BIT(3),	/* Renamed to IEEE80211_VIF_EML_ACTIVE. */
#endif
	IEEE80211_VIF_EML_ACTIVE		= BIT(4),
	IEEE80211_VIF_IGNORE_OFDMA_WIDER_BW	= BIT(5),
	IEEE80211_VIF_REMOVE_AP_AFTER_DISASSOC	= BIT(6),
};

#define	IEEE80211_BSS_ARP_ADDR_LIST_LEN		4

struct ieee80211_vif_cfg {
	uint16_t				aid;
	uint16_t				eml_cap;
	uint16_t				eml_med_sync_delay;
	bool					assoc;
	bool					ps;
	bool					idle;
	bool					ibss_joined;
	int					arp_addr_cnt;
	size_t					ssid_len;
	uint32_t				arp_addr_list[IEEE80211_BSS_ARP_ADDR_LIST_LEN];		/* big endian */
	uint8_t					ssid[IEEE80211_NWID_LEN];
	uint8_t					ap_addr[ETH_ALEN];
};

struct ieee80211_vif {
	/* TODO FIXME */
	enum nl80211_iftype		type;
	int		cab_queue;
	int		offload_flags;
	enum ieee80211_vif_driver_flags	driver_flags;
	bool				p2p;
	bool				probe_req_reg;
	uint8_t				addr[ETH_ALEN];
	struct ieee80211_vif_cfg	cfg;
	struct ieee80211_txq		*txq;
	struct ieee80211_bss_conf	bss_conf;
	struct ieee80211_bss_conf	*link_conf[IEEE80211_MLD_MAX_NUM_LINKS];	/* rcu? */
	uint8_t				hw_queue[IEEE80211_NUM_ACS];
	uint16_t			active_links;
	uint16_t			valid_links;
	struct ieee80211_vif		*mbssid_tx_vif;

/* #ifdef CONFIG_MAC80211_DEBUGFS */	/* Do not change structure depending on compile-time option. */
	struct dentry			*debugfs_dir;
/* #endif */

	/* Must stay last. */
	uint8_t				drv_priv[0] __aligned(CACHE_LINE_SIZE);
};

struct ieee80211_vif_chanctx_switch {
	struct ieee80211_chanctx_conf	*old_ctx, *new_ctx;
	struct ieee80211_vif		*vif;
	struct ieee80211_bss_conf	*link_conf;
};

struct ieee80211_prep_tx_info {
	uint16_t			duration;
	uint16_t			subtype;
	bool				success;
	bool				was_assoc;
	int				link_id;
};

/* XXX-BZ too big, over-reduce size to u8, and array sizes to minuimum to fit in skb->cb. */
/* Also warning: some sizes change by pointer size!  This is 64bit only. */
struct ieee80211_tx_info {
	enum ieee80211_tx_info_flags		flags;		/* 32 bits */
	/* TODO FIXME */
	enum nl80211_band			band;		/* 3 bits */
	uint16_t	hw_queue:4,				/* 4 bits */
			tx_time_est:10;				/* 10 bits */
	union {
		struct {
			struct ieee80211_tx_rate	rates[4];
			bool				use_rts;
			uint8_t				antennas:2;
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
			uint8_t				flags;
			void				*status_driver_data[16 / sizeof(void *)];		/* XXX TODO */
		} status;
#define	IEEE80211_TX_INFO_DRIVER_DATA_SIZE	40
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

enum ieee80211_iface_iter {
	IEEE80211_IFACE_ITER_NORMAL	= BIT(0),
	IEEE80211_IFACE_ITER_RESUME_ALL	= BIT(1),
	IEEE80211_IFACE_SKIP_SDATA_NOT_IN_DRIVER = BIT(2),	/* seems to be an iter flag */
	IEEE80211_IFACE_ITER_ACTIVE	= BIT(3),

	/* Internal flags only. */
	IEEE80211_IFACE_ITER__ATOMIC	= BIT(6),
	IEEE80211_IFACE_ITER__MTX	= BIT(8),
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
	IEEE80211_OFFLOAD_ENCAP_ENABLED,
	IEEE80211_OFFLOAD_DECAP_ENABLED,
};

struct ieee80211_ops {
	/* TODO FIXME */
	int  (*start)(struct ieee80211_hw *);
	void (*stop)(struct ieee80211_hw *, bool);

	int  (*config)(struct ieee80211_hw *, int, u32);
	void (*reconfig_complete)(struct ieee80211_hw *, enum ieee80211_reconfig_type);

	void (*prep_add_interface)(struct ieee80211_hw *, enum nl80211_iftype);
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
	void (*mgd_protect_tdls_discover)(struct ieee80211_hw *, struct ieee80211_vif *, unsigned int);

	void (*flush)(struct ieee80211_hw *, struct ieee80211_vif *, u32, bool);
	void (*flush_sta)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_sta *);

	int  (*set_frag_threshold)(struct ieee80211_hw *, int, u32);

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
	void (*link_sta_rc_update)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_link_sta *, u32);
	void (*sta_rate_tbl_update)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_sta *);
	void (*sta_set_4addr)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_sta *, bool);
	void (*sta_set_decap_offload)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_sta *, bool);

	u64  (*prepare_multicast)(struct ieee80211_hw *, struct netdev_hw_addr_list *);

	int  (*ampdu_action)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_ampdu_params *);

	bool (*can_aggregate_in_amsdu)(struct ieee80211_hw *, struct sk_buff *, struct sk_buff *);

	int  (*pre_channel_switch)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_channel_switch *);
	int  (*post_channel_switch)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_bss_conf *);
	void (*channel_switch)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_channel_switch *);
	void (*channel_switch_beacon)(struct ieee80211_hw *, struct ieee80211_vif *, struct cfg80211_chan_def *);
	void (*abort_channel_switch)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_bss_conf *);
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

	int  (*get_antenna)(struct ieee80211_hw *, int, u32 *, u32 *);
	int  (*set_antenna)(struct ieee80211_hw *, int, u32, u32);

	int  (*remain_on_channel)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_channel *, int, enum ieee80211_roc_type);
	int  (*cancel_remain_on_channel)(struct ieee80211_hw *, struct ieee80211_vif *);

	void (*configure_filter)(struct ieee80211_hw *, unsigned int, unsigned int *, u64);
	void (*config_iface_filter)(struct ieee80211_hw *, struct ieee80211_vif *, unsigned int, unsigned int);

	void (*bss_info_changed)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_bss_conf *, u64);
        void (*link_info_changed)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_bss_conf *, u64);

	int  (*set_rts_threshold)(struct ieee80211_hw *, int, u32);
	void (*event_callback)(struct ieee80211_hw *, struct ieee80211_vif *, const struct ieee80211_event *);
	int  (*get_survey)(struct ieee80211_hw *, int, struct survey_info *);
	int  (*get_ftm_responder_stats)(struct ieee80211_hw *, struct ieee80211_vif *, struct cfg80211_ftm_responder_stats *);

        uint64_t (*get_tsf)(struct ieee80211_hw *, struct ieee80211_vif *);
        void (*set_tsf)(struct ieee80211_hw *, struct ieee80211_vif *, uint64_t);
	void (*offset_tsf)(struct ieee80211_hw *, struct ieee80211_vif *, s64);

	int  (*set_bitrate_mask)(struct ieee80211_hw *, struct ieee80211_vif *, const struct cfg80211_bitrate_mask *);
	void (*set_coverage_class)(struct ieee80211_hw *, int, s16);
	int  (*set_tim)(struct ieee80211_hw *, struct ieee80211_sta *, bool);

	int  (*set_key)(struct ieee80211_hw *, enum set_key_cmd, struct ieee80211_vif *, struct ieee80211_sta *, struct ieee80211_key_conf *);
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

	int  (*get_txpower)(struct ieee80211_hw *, struct ieee80211_vif *, unsigned int, int *);
	int  (*get_stats)(struct ieee80211_hw *, struct ieee80211_low_level_stats *);

	int  (*set_radar_background)(struct ieee80211_hw *, struct cfg80211_chan_def *);

	void (*add_twt_setup)(struct ieee80211_hw *, struct ieee80211_sta *, struct ieee80211_twt_setup *);
	void (*twt_teardown_request)(struct ieee80211_hw *, struct ieee80211_sta *, u8);

	int (*set_hw_timestamp)(struct ieee80211_hw *, struct ieee80211_vif *, struct cfg80211_set_hw_timestamp *);

        void (*vif_cfg_changed)(struct ieee80211_hw *, struct ieee80211_vif *, u64);

	int (*change_vif_links)(struct ieee80211_hw *, struct ieee80211_vif *, u16, u16, struct ieee80211_bss_conf *[IEEE80211_MLD_MAX_NUM_LINKS]);
	int (*change_sta_links)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_sta *, u16, u16);
	bool (*can_activate_links)(struct ieee80211_hw *, struct ieee80211_vif *, u16);
	enum ieee80211_neg_ttlm_res (*can_neg_ttlm)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_neg_ttlm *);

	void (*rfkill_poll)(struct ieee80211_hw *);

/* #ifdef CONFIG_MAC80211_DEBUGFS */	/* Do not change depending on compile-time option. */
	void (*sta_add_debugfs)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_sta *, struct dentry *);
	void (*vif_add_debugfs)(struct ieee80211_hw *, struct ieee80211_vif *);
	void (*link_sta_add_debugfs)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_link_sta *, struct dentry *);
	void (*link_add_debugfs)(struct ieee80211_hw *, struct ieee80211_vif *, struct ieee80211_bss_conf *, struct dentry *);
/* #endif */
/* #ifdef CONFIG_PM_SLEEP */		/* Do not change depending on compile-time option. */
	int (*suspend)(struct ieee80211_hw *, struct cfg80211_wowlan *);
	int (*resume)(struct ieee80211_hw *);
	void (*set_wakeup)(struct ieee80211_hw *, bool);
	void (*set_rekey_data)(struct ieee80211_hw *, struct ieee80211_vif *, struct cfg80211_gtk_rekey_data *);
	void (*set_default_unicast_key)(struct ieee80211_hw *, struct ieee80211_vif *, int);
/* #if IS_ENABLED(CONFIG_IPV6) */
	void (*ipv6_addr_change)(struct ieee80211_hw *, struct ieee80211_vif *, struct inet6_dev *);
/* #endif */
/* #endif CONFIG_PM_SLEEP */
};

/* -------------------------------------------------------------------------- */

/* linux_80211.c */
extern const struct cfg80211_ops linuxkpi_mac80211cfgops;

struct ieee80211_hw *linuxkpi_ieee80211_alloc_hw(size_t,
    const struct ieee80211_ops *);
void linuxkpi_ieee80211_iffree(struct ieee80211_hw *);
void linuxkpi_set_ieee80211_dev(struct ieee80211_hw *);
int linuxkpi_ieee80211_ifattach(struct ieee80211_hw *);
void linuxkpi_ieee80211_ifdetach(struct ieee80211_hw *);
void linuxkpi_ieee80211_unregister_hw(struct ieee80211_hw *);
struct ieee80211_hw * linuxkpi_wiphy_to_ieee80211_hw(struct wiphy *);
void linuxkpi_ieee80211_restart_hw(struct ieee80211_hw *);
void linuxkpi_ieee80211_iterate_interfaces(
    struct ieee80211_hw *hw, enum ieee80211_iface_iter flags,
    void(*iterfunc)(void *, uint8_t *, struct ieee80211_vif *),
    void *);
void linuxkpi_ieee80211_iterate_keys(struct ieee80211_hw *,
    struct ieee80211_vif *,
    void(*iterfunc)(struct ieee80211_hw *, struct ieee80211_vif *,
        struct ieee80211_sta *, struct ieee80211_key_conf *, void *),
    void *, bool);
void linuxkpi_ieee80211_iterate_chan_contexts(struct ieee80211_hw *,
    void(*iterfunc)(struct ieee80211_hw *,
	struct ieee80211_chanctx_conf *, void *),
    void *);
void linuxkpi_ieee80211_iterate_stations_atomic(struct ieee80211_hw *,
   void (*iterfunc)(void *, struct ieee80211_sta *), void *);
void linuxkpi_ieee80211_scan_completed(struct ieee80211_hw *,
    struct cfg80211_scan_info *);
void linuxkpi_ieee80211_rx(struct ieee80211_hw *, struct sk_buff *,
    struct ieee80211_sta *, struct napi_struct *, struct list_head *);
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
    struct ieee80211_vif *, int, bool);
void linuxkpi_ieee80211_txq_get_depth(struct ieee80211_txq *, unsigned long *,
    unsigned long *);
struct wireless_dev *linuxkpi_ieee80211_vif_to_wdev(struct ieee80211_vif *);
void linuxkpi_ieee80211_connection_loss(struct ieee80211_vif *);
void linuxkpi_ieee80211_beacon_loss(struct ieee80211_vif *);
struct sk_buff *linuxkpi_ieee80211_probereq_get(struct ieee80211_hw *,
    const uint8_t *, const uint8_t *, size_t, size_t);
void linuxkpi_ieee80211_tx_status(struct ieee80211_hw *, struct sk_buff *);
void linuxkpi_ieee80211_tx_status_ext(struct ieee80211_hw *,
    struct ieee80211_tx_status *);
void linuxkpi_ieee80211_stop_queues(struct ieee80211_hw *);
void linuxkpi_ieee80211_wake_queues(struct ieee80211_hw *);
void linuxkpi_ieee80211_stop_queue(struct ieee80211_hw *, int);
void linuxkpi_ieee80211_wake_queue(struct ieee80211_hw *, int);
void linuxkpi_ieee80211_txq_schedule_start(struct ieee80211_hw *, uint8_t);
struct ieee80211_txq *linuxkpi_ieee80211_next_txq(struct ieee80211_hw *, uint8_t);
void linuxkpi_ieee80211_schedule_txq(struct ieee80211_hw *,
    struct ieee80211_txq *, bool);
void linuxkpi_ieee80211_handle_wake_tx_queue(struct ieee80211_hw *,
	struct ieee80211_txq *);

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
	linuxkpi_set_ieee80211_dev(hw);

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

static inline void
ieee80211_unregister_hw(struct ieee80211_hw *hw)
{

	linuxkpi_ieee80211_unregister_hw(hw);
}

static __inline struct ieee80211_hw *
wiphy_to_ieee80211_hw(struct wiphy *wiphy)
{

	return (linuxkpi_wiphy_to_ieee80211_hw(wiphy));
}

static inline void
ieee80211_restart_hw(struct ieee80211_hw *hw)
{
	linuxkpi_ieee80211_restart_hw(hw);
}

static inline void
ieee80211_hw_restart_disconnect(struct ieee80211_vif *vif)
{
	TODO();
}

/* -------------------------------------------------------------------------- */

#define	link_conf_dereference_check(_vif, _linkid)			\
    rcu_dereference_check((_vif)->link_conf[_linkid], true)

#define	link_conf_dereference_protected(_vif, _linkid)			\
    rcu_dereference_protected((_vif)->link_conf[_linkid], true)

#define	link_sta_dereference_check(_sta, _linkid)			\
    rcu_dereference_check((_sta)->link[_linkid], true)

#define	link_sta_dereference_protected(_sta, _linkid)			\
    rcu_dereference_protected((_sta)->link[_linkid], true)

#define	for_each_vif_active_link(_vif, _link, _linkid)			\
    for (_linkid = 0; _linkid < nitems((_vif)->link_conf); _linkid++)	\
	if ( ((_vif)->active_links == 0 /* no MLO */ ||			\
	    ((_vif)->active_links & BIT(_linkid)) != 0) &&		\
	    (_link = rcu_dereference((_vif)->link_conf[_linkid])) )

#define	for_each_sta_active_link(_vif, _sta, _linksta, _linkid)		\
    for (_linkid = 0; _linkid < nitems((_sta)->link); _linkid++)	\
	if ( ((_vif)->active_links == 0 /* no MLO */ ||			\
	    ((_vif)->active_links & BIT(_linkid)) != 0) &&		\
	    (_linksta = link_sta_dereference_protected((_sta), (_linkid))) )

/* -------------------------------------------------------------------------- */

static __inline bool
ieee80211_vif_is_mesh(struct ieee80211_vif *vif)
{
	TODO();
	return (false);
}


/* -------------------------------------------------------------------------- */
/* Receive functions (air/driver to mac80211/net80211). */


static __inline void
ieee80211_rx_napi(struct ieee80211_hw *hw, struct ieee80211_sta *sta,
    struct sk_buff *skb, struct napi_struct *napi)
{

	linuxkpi_ieee80211_rx(hw, skb, sta, napi, NULL);
}

static __inline void
ieee80211_rx_list(struct ieee80211_hw *hw, struct ieee80211_sta *sta,
    struct sk_buff *skb, struct list_head *list)
{

	linuxkpi_ieee80211_rx(hw, skb, sta, NULL, list);
}

static __inline void
ieee80211_rx_ni(struct ieee80211_hw *hw, struct sk_buff *skb)
{

	linuxkpi_ieee80211_rx(hw, skb, NULL, NULL, NULL);
}

static __inline void
ieee80211_rx_irqsafe(struct ieee80211_hw *hw, struct sk_buff *skb)
{

	linuxkpi_ieee80211_rx(hw, skb, NULL, NULL, NULL);
}

static __inline void
ieee80211_rx(struct ieee80211_hw *hw, struct sk_buff *skb)
{

	linuxkpi_ieee80211_rx(hw, skb, NULL, NULL, NULL);
}

/* -------------------------------------------------------------------------- */

static inline void
ieee80211_stop_queues(struct ieee80211_hw *hw)
{
	linuxkpi_ieee80211_stop_queues(hw);
}

static inline void
ieee80211_wake_queues(struct ieee80211_hw *hw)
{
	linuxkpi_ieee80211_wake_queues(hw);
}

static inline void
ieee80211_stop_queue(struct ieee80211_hw *hw, int qnum)
{
	linuxkpi_ieee80211_stop_queue(hw, qnum);
}

static inline void
ieee80211_wake_queue(struct ieee80211_hw *hw, int qnum)
{
	linuxkpi_ieee80211_wake_queue(hw, qnum);
}

static inline void
ieee80211_schedule_txq(struct ieee80211_hw *hw, struct ieee80211_txq *txq)
{
	linuxkpi_ieee80211_schedule_txq(hw, txq, true);
}

static inline void
ieee80211_return_txq(struct ieee80211_hw *hw, struct ieee80211_txq *txq,
    bool withoutpkts)
{
	linuxkpi_ieee80211_schedule_txq(hw, txq, withoutpkts);
}

static inline void
ieee80211_txq_schedule_start(struct ieee80211_hw *hw, uint8_t ac)
{
	linuxkpi_ieee80211_txq_schedule_start(hw, ac);
}

static inline void
ieee80211_txq_schedule_end(struct ieee80211_hw *hw, uint8_t ac)
{
	/* DO_NADA; */
}

static inline struct ieee80211_txq *
ieee80211_next_txq(struct ieee80211_hw *hw, uint8_t ac)
{
	return (linuxkpi_ieee80211_next_txq(hw, ac));
}

static inline void
ieee80211_handle_wake_tx_queue(struct ieee80211_hw *hw,
    struct ieee80211_txq *txq)
{
	linuxkpi_ieee80211_handle_wake_tx_queue(hw, txq);
}

static inline void
ieee80211_purge_tx_queue(struct ieee80211_hw *hw,
    struct sk_buff_head *skbs)
{
	TODO();
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
	flags |= IEEE80211_IFACE_ITER_ACTIVE;
	linuxkpi_ieee80211_iterate_interfaces(hw, flags, iterfunc, arg);
}

static __inline void
ieee80211_iterate_active_interfaces(struct ieee80211_hw *hw,
    enum ieee80211_iface_iter flags,
    void(*iterfunc)(void *, uint8_t *, struct ieee80211_vif *),
    void *arg)
{

	flags |= IEEE80211_IFACE_ITER_ACTIVE;
	linuxkpi_ieee80211_iterate_interfaces(hw, flags, iterfunc, arg);
}

static __inline void
ieee80211_iterate_active_interfaces_mtx(struct ieee80211_hw *hw,
    enum ieee80211_iface_iter flags,
    void(*iterfunc)(void *, uint8_t *, struct ieee80211_vif *),
    void *arg)
{
	flags |= IEEE80211_IFACE_ITER_ACTIVE;
	flags |= IEEE80211_IFACE_ITER__MTX;
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

static inline void
ieee80211_iter_keys(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    void(*iterfunc)(struct ieee80211_hw *, struct ieee80211_vif *,
        struct ieee80211_sta *, struct ieee80211_key_conf *, void *),
    void *arg)
{
	linuxkpi_ieee80211_iterate_keys(hw, vif, iterfunc, arg, false);
}

static inline void
ieee80211_iter_keys_rcu(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    void(*iterfunc)(struct ieee80211_hw *, struct ieee80211_vif *,
        struct ieee80211_sta *, struct ieee80211_key_conf *, void *),
    void *arg)
{
	linuxkpi_ieee80211_iterate_keys(hw, vif, iterfunc, arg, true);
}

static __inline void
ieee80211_iter_chan_contexts_atomic(struct ieee80211_hw *hw,
    void(*iterfunc)(struct ieee80211_hw *, struct ieee80211_chanctx_conf *, void *),
    void *arg)
{

	linuxkpi_ieee80211_iterate_chan_contexts(hw, iterfunc, arg);
}

static __inline void
ieee80211_iter_chan_contexts_mtx(struct ieee80211_hw *hw,
    void (*iterfunc)(struct ieee80211_hw *, struct ieee80211_chanctx_conf *, void *),
    void *arg)
{
	IMPROVE("XXX LKPI80211 TODO MTX\n");
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
ieee80211_chswitch_done(struct ieee80211_vif *vif, bool t, uint32_t link_id)
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
ieee80211_csa_finish(struct ieee80211_vif *vif, uint32_t link_id)
{
	TODO();
}

static inline enum nl80211_iftype
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

/*
 * Below we assume that the two values from different emums are the same.
 * Make sure this does not accidentally change.
 */
CTASSERT((int)IEEE80211_ACTION_SM_TPCREP == (int)IEEE80211_ACTION_RADIO_MEASUREMENT_LMREP);

static __inline bool
ieee80211_action_contains_tpc(struct sk_buff *skb)
{
	struct ieee80211_mgmt *mgmt;

	mgmt = (struct ieee80211_mgmt *)skb->data;

	/* Check that this is a mgmt/action frame? */
	if (!ieee80211_is_action(mgmt->frame_control))
		return (false);

	/*
	 * This is a bit convoluted but according to docs both actions
	 * are checked for this.  Kind-of makes sense for the only consumer
	 * (iwlwifi) I am aware off given the txpower fields are at the
	 * same location so firmware can update the value.
	 */
	/* 80211-2020 9.6.2 Spectrum Management Action frames */
	/* 80211-2020 9.6.2.5 TPC Report frame format */
	/* 80211-2020 9.6.6 Radio Measurement action details */
	/* 80211-2020 9.6.6.4 Link Measurement Report frame format */
	/* Check that it is Spectrum Management or Radio Measurement? */
	if (mgmt->u.action.category != IEEE80211_ACTION_CAT_SM &&
	    mgmt->u.action.category != IEEE80211_ACTION_CAT_RADIO_MEASUREMENT)
		return (false);

	/*
	 * Check that it is TPC Report or Link Measurement Report?
	 * The values of each are the same (see CTASSERT above function).
	 */
	if (mgmt->u.action.u.tpc_report.spec_mgmt != IEEE80211_ACTION_SM_TPCREP)
		return (false);

	/* 80211-2020 9.4.2.16 TPC Report element */
	/* Check that the ELEMID and length are correct? */
	if (mgmt->u.action.u.tpc_report.tpc_elem_id != IEEE80211_ELEMID_TPCREP ||
	    mgmt->u.action.u.tpc_report.tpc_elem_length != 4)
		return (false);

	/* All the right fields in the right place. */
	return (true);
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
ieee80211_request_smps(struct ieee80211_vif *vif, u_int link_id,
    enum ieee80211_smps_mode smps)
{
	static const char *smps_mode_name[] = {
		"SMPS_OFF",
		"SMPS_STATIC",
		"SMPS_DYNAMIC",
		"SMPS_AUTOMATIC",
	};

	if (vif->type != NL80211_IFTYPE_STATION)
		return;

	if (smps >= nitems(smps_mode_name))
		panic("%s: unsupported smps value: %d\n", __func__, smps);

	IMPROVE("XXX LKPI80211 TODO smps %d %s\n", smps, smps_mode_name[smps]);
}

static __inline void
ieee80211_tdls_oper_request(struct ieee80211_vif *vif, uint8_t *addr,
    enum nl80211_tdls_operation oper, enum ieee80211_reason_code code,
    gfp_t gfp)
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

/* -------------------------------------------------------------------------- */

static inline bool
ieee80211_sn_less(uint16_t sn1, uint16_t sn2)
{
	return (IEEE80211_SEQ_BA_BEFORE(sn1, sn2));
}

static inline uint16_t
ieee80211_sn_inc(uint16_t sn)
{
	return (IEEE80211_SEQ_INC(sn));
}

static inline uint16_t
ieee80211_sn_add(uint16_t sn, uint16_t a)
{
	return (IEEE80211_SEQ_ADD(sn, a));
}

static inline uint16_t
ieee80211_sn_sub(uint16_t sa, uint16_t sb)
{
	return (IEEE80211_SEQ_SUB(sa, sb));
}

static __inline void
ieee80211_mark_rx_ba_filtered_frames(struct ieee80211_sta *sta, uint8_t tid,
    uint32_t ssn, uint64_t bitmap, uint16_t received_mpdu)
{
	TODO();
}

static __inline void
ieee80211_stop_rx_ba_session(struct ieee80211_vif *vif, uint32_t x, uint8_t *addr)
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

/* -------------------------------------------------------------------------- */

static inline void
ieee80211_rate_set_vht(struct ieee80211_tx_rate *r, uint8_t mcs, uint8_t nss)
{

	/* XXX-BZ make it KASSERTS? */
	if (((mcs & 0xF0) != 0) || (((nss - 1) & 0xf8) != 0)) {
		printf("%s:%d: mcs %#04x nss %#04x invalid\n",
		     __func__, __LINE__, mcs, nss);
		return;
	}

	r->idx = mcs;
	r->idx |= ((nss - 1) << 4);
}

static inline uint8_t
ieee80211_rate_get_vht_nss(const struct ieee80211_tx_rate *r)
{
	return (((r->idx >> 4) & 0x07) + 1);
}

static inline uint8_t
ieee80211_rate_get_vht_mcs(const struct ieee80211_tx_rate *r)
{
	return (r->idx & 0x0f);
}

static inline int
ieee80211_get_vht_max_nss(struct ieee80211_vht_cap *vht_cap,
    enum ieee80211_vht_chanwidth chanwidth,		/* defined in net80211. */
    int mcs /* always 0 */, bool ext_nss_bw_cap /* always true */, int max_nss)
{
	enum ieee80211_vht_mcs_support mcs_s;
	uint32_t supp_cw, ext_nss_bw;

	switch (mcs) {
	case 0 ... 7:
		mcs_s = IEEE80211_VHT_MCS_SUPPORT_0_7;
		break;
	case 8:
		mcs_s = IEEE80211_VHT_MCS_SUPPORT_0_8;
		break;
	case 9:
		mcs_s = IEEE80211_VHT_MCS_SUPPORT_0_9;
		break;
	default:
		printf("%s: unsupported mcs value %d\n", __func__, mcs);
		return (0);
	}

	if (max_nss == 0) {
		uint16_t map;

		map = le16toh(vht_cap->supp_mcs.rx_mcs_map);
		for (int i = 7; i >= 0; i--) {
			uint8_t val;

			val = (map >> (2 * i)) & 0x03;
			if (val == IEEE80211_VHT_MCS_NOT_SUPPORTED)
				continue;
			if (val >= mcs_s) {
				max_nss = i + 1;
				break;
			}
		}
	}

	if (max_nss == 0)
		return (0);

	if ((le16toh(vht_cap->supp_mcs.tx_mcs_map) &
	    IEEE80211_VHT_EXT_NSS_BW_CAPABLE) == 0)
		return (max_nss);

	supp_cw = le32_get_bits(vht_cap->vht_cap_info,
	    IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_MASK);
	ext_nss_bw = le32_get_bits(vht_cap->vht_cap_info,
	    IEEE80211_VHT_CAP_EXT_NSS_BW_MASK);

	/* If requested as ext nss not supported assume ext_nss_bw 0. */
	if (!ext_nss_bw_cap)
		ext_nss_bw = 0;

	/*
	 * Cover 802.11-2016, Table 9-250.
	 */

	/* Unsupported settings. */
	if (supp_cw == 3)
		return (0);
	if (supp_cw == 2 && (ext_nss_bw == 1 || ext_nss_bw == 2))
		return (0);

	/* Settings with factor != 1 or unsupported. */
	switch (chanwidth) {
	case IEEE80211_VHT_CHANWIDTH_80P80MHZ:
		if (supp_cw == 0 && (ext_nss_bw == 0 || ext_nss_bw == 1))
			return (0);
		if (supp_cw == 1 && ext_nss_bw == 0)
			return (0);
		if ((supp_cw == 0 || supp_cw == 1) && ext_nss_bw == 2)
			return (max_nss / 2);
		if ((supp_cw == 0 || supp_cw == 1) && ext_nss_bw == 3)
			return (3 * max_nss / 4);
		break;
	case IEEE80211_VHT_CHANWIDTH_160MHZ:
		if (supp_cw == 0 && ext_nss_bw == 0)
			return (0);
		if (supp_cw == 0 && (ext_nss_bw == 1 || ext_nss_bw == 2))
			return (max_nss / 2);
		if (supp_cw == 0 && ext_nss_bw == 3)
			return (3 * max_nss / 4);
		if (supp_cw == 1 && ext_nss_bw == 3)
			return (2 * max_nss);
		break;
	case IEEE80211_VHT_CHANWIDTH_80MHZ:
	case IEEE80211_VHT_CHANWIDTH_USE_HT:
		if ((supp_cw == 1 || supp_cw == 2) && ext_nss_bw == 3)
			return (2 * max_nss);
		break;
	}

	/* Everything else has a factor of 1. */
	return (max_nss);
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
ieee80211_send_eosp_nullfunc(struct ieee80211_sta *sta, uint8_t tid)
{
	TODO();
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

static inline void
ieee80211_sta_recalc_aggregates(struct ieee80211_sta *sta)
{
	if (sta->valid_links) {
		TODO();
	}
}

static __inline void
ieee80211_sta_uapsd_trigger(struct ieee80211_sta *sta, int ntids)
{
	TODO();
}

static inline struct sk_buff *
ieee80211_tx_dequeue(struct ieee80211_hw *hw, struct ieee80211_txq *txq)
{

	return (linuxkpi_ieee80211_tx_dequeue(hw, txq));
}

static inline struct sk_buff *
ieee80211_tx_dequeue_ni(struct ieee80211_hw *hw, struct ieee80211_txq *txq)
{
	struct sk_buff *skb;

	local_bh_disable();
	skb = linuxkpi_ieee80211_tx_dequeue(hw, txq);
	local_bh_enable();

	return (skb);
}

static __inline void
ieee80211_update_mu_groups(struct ieee80211_vif *vif,
    u_int link_id, const uint8_t *ms, const uint8_t *up)
{
	TODO();
}

static __inline void
ieee80211_sta_set_buffered(struct ieee80211_sta *sta, uint8_t tid, bool t)
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

static __inline int
ieee80211_start_tx_ba_session(struct ieee80211_sta *sta, uint8_t tid, int x)
{
	TODO("rtw8x");
	return (-EINVAL);
}

static __inline int
ieee80211_stop_tx_ba_session(struct ieee80211_sta *sta, uint8_t tid)
{
	TODO("rtw89");
	return (-EINVAL);
}

static __inline void
ieee80211_start_tx_ba_cb_irqsafe(struct ieee80211_vif *vif, uint8_t *addr,
    uint8_t tid)
{
	TODO("iwlwifi");
}

static __inline void
ieee80211_stop_tx_ba_cb_irqsafe(struct ieee80211_vif *vif, uint8_t *addr,
    uint8_t tid)
{
	TODO("iwlwifi/rtw8x/...");
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
ieee80211_beacon_get(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    uint32_t link_id)
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
    int linkid, bool qos)
{

	/* Only STA needs this.  Otherwise return NULL and panic bad drivers. */
	if (vif->type != NL80211_IFTYPE_STATION)
		return (NULL);

	return (linuxkpi_ieee80211_nullfunc_get(hw, vif, linkid, qos));
}

static __inline struct sk_buff *
ieee80211_probereq_get(struct ieee80211_hw *hw, const uint8_t *addr,
    const uint8_t *ssid, size_t ssid_len, size_t tailroom)
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

static __inline bool
ieee80211_tx_prepare_skb(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    struct sk_buff *skb, enum nl80211_band band, struct ieee80211_sta **sta)
{
	TODO();
	return (false);
}

static __inline void
ieee80211_tx_status_skb(struct ieee80211_hw *hw, struct sk_buff *skb)
{
	linuxkpi_ieee80211_tx_status(hw, skb);
}

static inline void
ieee80211_tx_status_noskb(struct ieee80211_hw *hw, struct ieee80211_sta *sta,
    struct ieee80211_tx_info *info)
{
	TODO();
}

static __inline void
ieee80211_tx_status_irqsafe(struct ieee80211_hw *hw, struct sk_buff *skb)
{
	IMPROVE();
	linuxkpi_ieee80211_tx_status(hw, skb);
}

static __inline void
ieee80211_tx_status_ni(struct ieee80211_hw *hw, struct sk_buff *skb)
{
	IMPROVE();
	linuxkpi_ieee80211_tx_status(hw, skb);
}

static __inline void
ieee80211_tx_status_ext(struct ieee80211_hw *hw,
    struct ieee80211_tx_status *txstat)
{

	linuxkpi_ieee80211_tx_status_ext(hw, txstat);
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

static __inline void
SET_IEEE80211_PERM_ADDR	(struct ieee80211_hw *hw, uint8_t *addr)
{

	ether_addr_copy(hw->wiphy->perm_addr, addr);
}

static __inline void
ieee80211_report_low_ack(struct ieee80211_sta *sta, int x)
{
	TODO();
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

static __inline void
ieee80211_radar_detected(struct ieee80211_hw *hw,
    struct ieee80211_chanctx_conf *chanctx_conf)
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
ieee80211_beacon_set_cntdwn(struct ieee80211_vif *vif, u8 counter)
{
	TODO();
}

static __inline int
ieee80211_beacon_update_cntdwn(struct ieee80211_vif *vif, uint32_t link_id)
{
	TODO();
	return (-1);
}

static __inline bool
ieee80211_beacon_cntdwn_is_complete(struct ieee80211_vif *vif, uint32_t link_id)
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
ieee80211_channel_switch_disconnect(struct ieee80211_vif *vif)
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
ieee80211_color_change_finish(struct ieee80211_vif *vif, uint8_t link_id)
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

static __inline void
ieee80211_resume_disconnect(struct ieee80211_vif *vif)
{
        TODO();
}

static __inline int
ieee80211_data_to_8023(struct sk_buff *skb, const uint8_t *addr,
     enum nl80211_iftype iftype)
{
        TODO();
        return (-1);
}

/* -------------------------------------------------------------------------- */

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

static __inline void
ieee80211_remove_key(struct ieee80211_key_conf *key)
{
        TODO();
}

static __inline struct ieee80211_key_conf *
ieee80211_gtk_rekey_add(struct ieee80211_vif *vif,
    uint16_t keyidx, uint8_t *key, size_t keylen, int link_id)
{
        TODO();
        return (NULL);
}

static __inline void
ieee80211_gtk_rekey_notify(struct ieee80211_vif *vif, const uint8_t *bssid,
    const uint8_t *replay_ctr, gfp_t gfp)
{
        TODO();
}

static __inline void
ieee80211_tkip_add_iv(u8 *crypto_hdr, struct ieee80211_key_conf *keyconf,
    uint64_t pn)
{
	TODO();
}

static __inline void
ieee80211_get_tkip_rx_p1k(struct ieee80211_key_conf *keyconf,
    const u8 *addr, uint32_t iv32, u16 *p1k)
{

	KASSERT(keyconf != NULL && addr != NULL && p1k != NULL,
	    ("%s: keyconf %p addr %p p1k %p\n", __func__, keyconf, addr, p1k));

	TODO();
	memset(p1k, 0xfa, 5 * sizeof(*p1k));	/* Just initializing. */
}

static __inline void
ieee80211_get_tkip_p1k_iv(struct ieee80211_key_conf *key,
    uint32_t iv32, uint16_t *p1k)
{
        TODO();
}

static __inline void
ieee80211_get_tkip_p2k(struct ieee80211_key_conf *keyconf,
    struct sk_buff *skb_frag, u8 *key)
{
	TODO();
}

static inline void
ieee80211_get_key_rx_seq(struct ieee80211_key_conf *keyconf, int8_t tid,
    struct ieee80211_key_seq *seq)
{
	const struct ieee80211_key *k;
	const uint8_t *p;

	KASSERT(keyconf != NULL && seq != NULL, ("%s: keyconf %p seq %p\n",
	    __func__, keyconf, seq));
	k = keyconf->_k;
	KASSERT(k != NULL, ("%s: keyconf %p ieee80211_key is NULL\n", __func__, keyconf));

	switch (keyconf->cipher) {
	case WLAN_CIPHER_SUITE_TKIP:
		if (tid < 0 || tid >= IEEE80211_NUM_TIDS)
			return;
		/* See net80211::tkip_decrypt() */
		seq->tkip.iv32 = TKIP_PN_TO_IV32(k->wk_keyrsc[tid]);
		seq->tkip.iv16 = TKIP_PN_TO_IV16(k->wk_keyrsc[tid]);
		break;
	case WLAN_CIPHER_SUITE_CCMP:
	case WLAN_CIPHER_SUITE_CCMP_256:
		if (tid < -1 || tid >= IEEE80211_NUM_TIDS)
			return;
		if (tid == -1)
			p = (const uint8_t *)&k->wk_keyrsc[IEEE80211_NUM_TIDS];	/* IEEE80211_NONQOS_TID */
		else
			p = (const uint8_t *)&k->wk_keyrsc[tid];
		memcpy(seq->ccmp.pn, p, sizeof(seq->ccmp.pn));
		break;
	case WLAN_CIPHER_SUITE_GCMP:
	case WLAN_CIPHER_SUITE_GCMP_256:
		if (tid < -1 || tid >= IEEE80211_NUM_TIDS)
			return;
		if (tid == -1)
			p = (const uint8_t *)&k->wk_keyrsc[IEEE80211_NUM_TIDS];	/* IEEE80211_NONQOS_TID */
		else
			p = (const uint8_t *)&k->wk_keyrsc[tid];
		memcpy(seq->gcmp.pn, p, sizeof(seq->gcmp.pn));
		break;
	case WLAN_CIPHER_SUITE_AES_CMAC:
	case WLAN_CIPHER_SUITE_BIP_CMAC_256:
		TODO();
		memset(seq->aes_cmac.pn, 0xfa, sizeof(seq->aes_cmac.pn));	/* XXX TODO */
		break;
	case WLAN_CIPHER_SUITE_BIP_GMAC_128:
	case WLAN_CIPHER_SUITE_BIP_GMAC_256:
		TODO();
		memset(seq->aes_gmac.pn, 0xfa, sizeof(seq->aes_gmac.pn));	/* XXX TODO */
		break;
	default:
		pr_debug("%s: unsupported cipher suite %d\n", __func__, keyconf->cipher);
		break;
	}
}

static __inline void
ieee80211_set_key_rx_seq(struct ieee80211_key_conf *key, int tid,
    struct ieee80211_key_seq *seq)
{
        TODO();
}

/* -------------------------------------------------------------------------- */

static __inline void
ieee80211_report_wowlan_wakeup(struct ieee80211_vif *vif,
    struct cfg80211_wowlan_wakeup *wakeup, gfp_t gfp)
{
        TODO();
}

static __inline void
ieee80211_obss_color_collision_notify(struct ieee80211_vif *vif,
    uint64_t obss_color_bitmap, gfp_t gfp)
{
	TODO();
}

static __inline void
ieee80211_refresh_tx_agg_session_timer(struct ieee80211_sta *sta,
    uint8_t tid)
{
	TODO();
}

static __inline struct ieee80211_ema_beacons *
ieee80211_beacon_get_template_ema_list(struct ieee80211_hw *hw,
    struct ieee80211_vif *vif, uint32_t link_id)
{
	TODO();
	return (NULL);
}

static __inline void
ieee80211_beacon_free_ema_list(struct ieee80211_ema_beacons *bcns)
{
	TODO();
}

static inline bool
ieee80211_vif_is_mld(const struct ieee80211_vif *vif)
{

	/* If valid_links is non-zero, the vif is an MLD. */
	return (vif->valid_links != 0);
}

static inline const struct ieee80211_sta_he_cap *
ieee80211_get_he_iftype_cap_vif(const struct ieee80211_supported_band *band,
    struct ieee80211_vif *vif)
{
	enum nl80211_iftype iftype;

	iftype = ieee80211_vif_type_p2p(vif);
	return (ieee80211_get_he_iftype_cap(band, iftype));
}

static inline const struct ieee80211_sta_eht_cap *
ieee80211_get_eht_iftype_cap_vif(const struct ieee80211_supported_band *band,
    struct ieee80211_vif *vif)
{
	enum nl80211_iftype iftype;

	iftype = ieee80211_vif_type_p2p(vif);
	return (ieee80211_get_eht_iftype_cap(band, iftype));
}

static inline uint32_t
ieee80211_vif_usable_links(const struct ieee80211_vif *vif)
{
	IMPROVE("MLO usable links likely are not just valid");
	return (vif->valid_links);
}

static inline bool
ieee80211_vif_link_active(const struct ieee80211_vif *vif, uint8_t link_id)
{
	if (ieee80211_vif_is_mld(vif))
		return (vif->active_links & BIT(link_id));
	return (link_id == 0);
}

static inline void
ieee80211_set_active_links_async(struct ieee80211_vif *vif,
    uint32_t new_active_links)
{
	TODO();
}

static inline int
ieee80211_set_active_links(struct ieee80211_vif *vif,
    uint32_t active_links)
{
	TODO();
	return (-ENXIO);
}

static inline void
ieee80211_cqm_beacon_loss_notify(struct ieee80211_vif *vif, gfp_t gfp __unused)
{
	IMPROVE("we notify user space by a vap state change eventually");
	linuxkpi_ieee80211_beacon_loss(vif);
}

#define	ieee80211_send_bar(_v, _r, _t, _s)				\
    linuxkpi_ieee80211_send_bar(_v, _r, _t, _s)

/* -------------------------------------------------------------------------- */

int lkpi_80211_update_chandef(struct ieee80211_hw *,
    struct ieee80211_chanctx_conf *);

static inline int
ieee80211_emulate_add_chanctx(struct ieee80211_hw *hw,
    struct ieee80211_chanctx_conf *chanctx_conf)
{
	int error;

	hw->conf.radar_enabled = chanctx_conf->radar_enabled;
	error = lkpi_80211_update_chandef(hw, chanctx_conf);
	return (error);
}

static inline void
ieee80211_emulate_remove_chanctx(struct ieee80211_hw *hw,
    struct ieee80211_chanctx_conf *chanctx_conf __unused)
{
	hw->conf.radar_enabled = false;
	lkpi_80211_update_chandef(hw, NULL);
}

static inline void
ieee80211_emulate_change_chanctx(struct ieee80211_hw *hw,
    struct ieee80211_chanctx_conf *chanctx_conf, uint32_t changed __unused)
{
	hw->conf.radar_enabled = chanctx_conf->radar_enabled;
	lkpi_80211_update_chandef(hw, chanctx_conf);
}

static inline int
ieee80211_emulate_switch_vif_chanctx(struct ieee80211_hw *hw,
    struct ieee80211_vif_chanctx_switch *vifs, int n_vifs,
    enum ieee80211_chanctx_switch_mode mode __unused)
{
	struct ieee80211_chanctx_conf *chanctx_conf;
	int error;

	/* Sanity check. */
	if (n_vifs <= 0)
		return (-EINVAL);
	if (vifs == NULL || vifs[0].new_ctx == NULL)
		return (-EINVAL);

	/*
	 * What to do if n_vifs > 1?
	 * Does that make sense for drivers not supporting chanctx?
	 */
	hw->conf.radar_enabled = vifs[0].new_ctx->radar_enabled;
	chanctx_conf = vifs[0].new_ctx;
	error = lkpi_80211_update_chandef(hw, chanctx_conf);
	return (error);
}

/* -------------------------------------------------------------------------- */

#endif	/* _LINUXKPI_NET_MAC80211_H */
