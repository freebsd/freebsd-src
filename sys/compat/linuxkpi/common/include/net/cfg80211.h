/*-
 * Copyright (c) 2020-2021 The FreeBSD Foundation
 * Copyright (c) 2021-2022 Bjoern A. Zeeb
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

#ifndef	_LINUXKPI_NET_CFG80211_H
#define	_LINUXKPI_NET_CFG80211_H

#include <linux/types.h>
#include <linux/nl80211.h>
#include <linux/ieee80211.h>
#include <linux/if_ether.h>
#include <linux/ethtool.h>
#include <linux/device.h>
#include <linux/netdevice.h>
#include <linux/random.h>
#include <linux/skbuff.h>
#include <net/regulatory.h>

/* linux_80211.c */
extern int linuxkpi_debug_80211;
#ifndef	D80211_TODO
#define	D80211_TODO		0x1
#endif
#ifndef	D80211_IMPROVE
#define	D80211_IMPROVE		0x2
#endif
#define	TODO()		if (linuxkpi_debug_80211 & D80211_TODO)		\
    printf("%s:%d: XXX LKPI80211 TODO\n", __func__, __LINE__)
#define	IMPROVE(...)	if (linuxkpi_debug_80211 & D80211_IMPROVE)	\
    printf("%s:%d: XXX LKPI80211 IMPROVE\n", __func__, __LINE__)

enum rfkill_hard_block_reasons {
	RFKILL_HARD_BLOCK_NOT_OWNER		= BIT(0),
};

#define	WIPHY_PARAM_FRAG_THRESHOLD			__LINE__ /* TODO FIXME brcmfmac */
#define	WIPHY_PARAM_RETRY_LONG				__LINE__ /* TODO FIXME brcmfmac */
#define	WIPHY_PARAM_RETRY_SHORT				__LINE__ /* TODO FIXME brcmfmac */
#define	WIPHY_PARAM_RTS_THRESHOLD			__LINE__ /* TODO FIXME brcmfmac */

#define	CFG80211_SIGNAL_TYPE_MBM			__LINE__ /* TODO FIXME brcmfmac */

#define	UPDATE_ASSOC_IES	1

#define	IEEE80211_MAX_CHAINS	4		/* net80211: IEEE80211_MAX_CHAINS copied */

enum cfg80211_rate_info_flags {
	RATE_INFO_FLAGS_SHORT_GI	= BIT(0),
	RATE_INFO_FLAGS_MCS		= BIT(1),
	RATE_INFO_FLAGS_VHT_MCS		= BIT(2),
	RATE_INFO_FLAGS_HE_MCS		= BIT(3),
};

extern const uint8_t rfc1042_header[6];
extern const uint8_t bridge_tunnel_header[6];

enum ieee80211_privacy {
	IEEE80211_PRIVACY_ANY,
};

enum ieee80211_bss_type {
	IEEE80211_BSS_TYPE_ANY,
};

enum cfg80211_bss_frame_type {
	CFG80211_BSS_FTYPE_UNKNOWN,
	CFG80211_BSS_FTYPE_BEACON,
	CFG80211_BSS_FTYPE_PRESP,
};

enum ieee80211_channel_flags {
	IEEE80211_CHAN_DISABLED			= BIT(0),
	IEEE80211_CHAN_INDOOR_ONLY		= BIT(1),
	IEEE80211_CHAN_IR_CONCURRENT		= BIT(2),
	IEEE80211_CHAN_RADAR			= BIT(3),
	IEEE80211_CHAN_NO_IR			= BIT(4),
	IEEE80211_CHAN_NO_HT40MINUS		= BIT(5),
	IEEE80211_CHAN_NO_HT40PLUS		= BIT(6),
	IEEE80211_CHAN_NO_80MHZ			= BIT(7),
	IEEE80211_CHAN_NO_160MHZ		= BIT(8),
	IEEE80211_CHAN_NO_OFDM			= BIT(9),
};
#define	IEEE80211_CHAN_NO_HT40	(IEEE80211_CHAN_NO_HT40MINUS|IEEE80211_CHAN_NO_HT40PLUS)

struct ieee80211_txrx_stypes {
	uint16_t	tx;
	uint16_t	rx;
};

/* XXX net80211 has an ieee80211_channel as well. */
struct linuxkpi_ieee80211_channel {
	/* TODO FIXME */
	uint32_t				hw_value;	/* ic_ieee */
	uint32_t				center_freq;	/* ic_freq */
	enum ieee80211_channel_flags		flags;		/* ic_flags */
	enum nl80211_band			band;
	int8_t					max_power;	/* ic_maxpower */
	bool					beacon_found;
	int     max_antenna_gain, max_reg_power;
	int     orig_flags;
	int	dfs_cac_ms, dfs_state;
	int	orig_mpwr;
};

enum ieee80211_vht_mcs_support {
	LKPI_IEEE80211_VHT_MCS_SUPPORT_0_7,
	LKPI_IEEE80211_VHT_MCS_SUPPORT_0_8,
	LKPI_IEEE80211_VHT_MCS_SUPPORT_0_9,
};

struct cfg80211_bitrate_mask {
	/* TODO FIXME */
	/* This is so weird but nothing else works out...*/
	struct {
		uint64_t			legacy;		/* XXX? */
		uint8_t				ht_mcs[IEEE80211_HT_MCS_MASK_LEN];
		uint16_t			vht_mcs[16];	/* XXX? */
		uint16_t			he_mcs[16];	/* XXX? */
		enum nl80211_txrate_gi		gi;
		enum nl80211_he_gi		he_gi;
		uint8_t				he_ltf;
	} control[NUM_NL80211_BANDS];
};

enum rate_info_bw {
	RATE_INFO_BW_20		= 0,
	RATE_INFO_BW_5,
	RATE_INFO_BW_10,
	RATE_INFO_BW_40,
	RATE_INFO_BW_80,
	RATE_INFO_BW_160,
	RATE_INFO_BW_HE_RU,
};

struct rate_info {
	/* TODO FIXME */
	int	bw, flags, he_dcm, he_gi, he_ru_alloc, legacy, mcs, nss;
};

struct ieee80211_rate {
	/* TODO FIXME */
	uint32_t		bitrate;
	uint32_t		hw_value;
	uint32_t		hw_value_short;
	uint32_t		flags;
};

struct ieee80211_sta_ht_cap {
		/* TODO FIXME */
	int	ampdu_density, ampdu_factor;
	bool					ht_supported;
	uint16_t				cap;
	struct ieee80211_mcs_info		mcs;
};

/* XXX net80211 calls these IEEE80211_VHTCAP_* */
#define	IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_3895	0x00000000	/* IEEE80211_VHTCAP_MAX_MPDU_LENGTH_3895 */
#define	IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_7991	0x00000001	/* IEEE80211_VHTCAP_MAX_MPDU_LENGTH_7991 */
#define	IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_11454	0x00000002	/* IEEE80211_VHTCAP_MAX_MPDU_LENGTH_11454 */
#define	IEEE80211_VHT_CAP_MAX_MPDU_MASK		0x00000003	/* IEEE80211_VHTCAP_MAX_MPDU_MASK */

#define	IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160MHZ		(IEEE80211_VHTCAP_SUPP_CHAN_WIDTH_160MHZ << IEEE80211_VHTCAP_SUPP_CHAN_WIDTH_MASK_S)
#define	IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ	(IEEE80211_VHTCAP_SUPP_CHAN_WIDTH_160_80P80MHZ << IEEE80211_VHTCAP_SUPP_CHAN_WIDTH_MASK_S)
#define	IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_MASK	0x0000000c	/* IEEE80211_VHTCAP_SUPP_CHAN_WIDTH_MASK */

#define	IEEE80211_VHT_CAP_RXLDPC		0x00000010	/* IEEE80211_VHTCAP_RXLDPC */

#define	IEEE80211_VHT_CAP_SHORT_GI_80		0x00000020	/* IEEE80211_VHTCAP_SHORT_GI_80 */
#define	IEEE80211_VHT_CAP_SHORT_GI_160		0x00000040	/* IEEE80211_VHTCAP_SHORT_GI_160 */

#define	IEEE80211_VHT_CAP_TXSTBC		0x00000080	/* IEEE80211_VHTCAP_TXSTBC */

#define	IEEE80211_VHT_CAP_RXSTBC_1		0x00000100	/* IEEE80211_VHTCAP_RXSTBC_1 */
#define	IEEE80211_VHT_CAP_RXSTBC_MASK		0x00000700	/* IEEE80211_VHTCAP_RXSTBC_MASK */

#define	IEEE80211_VHT_CAP_SU_BEAMFORMER_CAPABLE	0x00000800	/* IEEE80211_VHTCAP_SU_BEAMFORMER_CAPABLE */

#define	IEEE80211_VHT_CAP_SU_BEAMFORMEE_CAPABLE	0x00001000	/* IEEE80211_VHTCAP_SU_BEAMFORMEE_CAPABLE */

#define	IEEE80211_VHT_CAP_MU_BEAMFORMER_CAPABLE	0x00080000	/* IEEE80211_VHTCAP_MU_BEAMFORMER_CAPABLE */

#define	IEEE80211_VHT_CAP_MU_BEAMFORMEE_CAPABLE	0x00100000	/* IEEE80211_VHTCAP_MU_BEAMFORMEE_CAPABLE */

#define	IEEE80211_VHT_CAP_BEAMFORMEE_STS_SHIFT		13	/* IEEE80211_VHTCAP_BEAMFORMEE_STS_SHIFT */
#define	IEEE80211_VHT_CAP_BEAMFORMEE_STS_MASK		(7 << IEEE80211_VHT_CAP_BEAMFORMEE_STS_SHIFT)	/* IEEE80211_VHTCAP_BEAMFORMEE_STS_MASK */

#define	IEEE80211_VHT_CAP_HTC_VHT		0x00400000	/* IEEE80211_VHTCAP_HTC_VHT */

#define	IEEE80211_VHT_CAP_RX_ANTENNA_PATTERN	0x10000000	/* IEEE80211_VHTCAP_RX_ANTENNA_PATTERN */
#define	IEEE80211_VHT_CAP_TX_ANTENNA_PATTERN	0x20000000	/* IEEE80211_VHTCAP_TX_ANTENNA_PATTERN */

#define	IEEE80211_VHT_CAP_VHT_LINK_ADAPTATION_VHT_MRQ_MFB	0x0c000000	/* IEEE80211_VHTCAP_VHT_LINK_ADAPTATION_VHT_MRQ_MFB */

#define	IEEE80211_VHT_CAP_SOUNDING_DIMENSIONS_SHIFT	16	/* IEEE80211_VHTCAP_SOUNDING_DIMENSIONS_SHIFT */
#define	IEEE80211_VHT_CAP_SOUNDING_DIMENSIONS_MASK		\
	(7 << IEEE80211_VHTCAP_SOUNDING_DIMENSIONS_SHIFT)	/* IEEE80211_VHTCAP_SOUNDING_DIMENSIONS_MASK */

#define	IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_SHIFT	23	/* IEEE80211_VHTCAP_MAX_A_MPDU_LENGTH_EXPONENT_SHIFT */
#define	IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK	\
	(7 << IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_SHIFT)	/* IEEE80211_VHTCAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK */


struct ieee80211_sta_vht_cap {
		/* TODO FIXME */
	bool			vht_supported;
	uint32_t		cap;
	struct vht_mcs		vht_mcs;
};

enum ieee80211_vht_opmode {
	IEEE80211_OPMODE_NOTIF_RX_NSS_SHIFT	= 4,
};

struct cfg80211_connect_resp_params {
		/* XXX TODO */
	uint8_t				*bssid;
	const uint8_t			*req_ie;
	const uint8_t			*resp_ie;
	uint32_t			req_ie_len;
	uint32_t			resp_ie_len;
	int	status;
};

struct cfg80211_inform_bss {
		/* XXX TODO */
	int     boottime_ns, scan_width, signal;
	struct linuxkpi_ieee80211_channel	*chan;
};

struct cfg80211_roam_info {
		/* XXX TODO */
	uint8_t				*bssid;
	const uint8_t			*req_ie;
	const uint8_t			*resp_ie;
	uint32_t			req_ie_len;
	uint32_t			resp_ie_len;
	struct linuxkpi_ieee80211_channel	*channel;
};

struct cfg80211_bss_ies {
		/* XXX TODO, type is best guess. Fix if more info. */
	uint8_t				*data;
	int				len;
};

struct cfg80211_bss {
		/* XXX TODO */
	struct cfg80211_bss_ies		*ies;
};

struct cfg80211_chan_def {
		/* XXX TODO */
	struct linuxkpi_ieee80211_channel	*chan;
	enum nl80211_chan_width		width;
	uint32_t			center_freq1;
	uint32_t			center_freq2;
};

struct cfg80211_ftm_responder_stats {
		/* XXX TODO */
	int	asap_num, failed_num, filled, non_asap_num, out_of_window_triggers_num, partial_num, reschedule_requests_num, success_num, total_duration_ms, unknown_triggers_num;
};

struct cfg80211_pmsr_capabilities {
		/* XXX TODO */
	int	max_peers, randomize_mac_addr, report_ap_tsf;
	struct {
		int	 asap, bandwidths, max_bursts_exponent, max_ftms_per_burst, non_asap, non_trigger_based, preambles, request_civicloc, request_lci, supported, trigger_based;
	} ftm;
};

struct cfg80211_pmsr_ftm_request {
		/* XXX TODO */
	int     asap, burst_period, ftmr_retries, ftms_per_burst, non_trigger_based, num_bursts_exp, request_civicloc, request_lci, trigger_based;
	uint8_t					bss_color;
	bool					lmr_feedback;
};

struct cfg80211_pmsr_request_peer {
		/* XXX TODO */
	struct cfg80211_chan_def		chandef;
	struct cfg80211_pmsr_ftm_request	ftm;
	uint8_t					addr[ETH_ALEN];
	int	report_ap_tsf;
};

struct cfg80211_pmsr_request {
		/* XXX TODO */
	int	cookie, n_peers, timeout;
	uint8_t					mac_addr[ETH_ALEN], mac_addr_mask[ETH_ALEN];
	struct cfg80211_pmsr_request_peer	peers[];
};

struct cfg80211_pmsr_ftm_result {
		/* XXX TODO */
	int	burst_index, busy_retry_time, failure_reason;
	int	num_ftmr_successes, rssi_avg, rssi_avg_valid, rssi_spread, rssi_spread_valid, rtt_avg, rtt_avg_valid, rtt_spread, rtt_spread_valid, rtt_variance, rtt_variance_valid;
	uint8_t					*lci;
	uint8_t					*civicloc;
	int					lci_len;
	int					civicloc_len;
};

struct cfg80211_pmsr_result {
		/* XXX TODO */
	int	ap_tsf, ap_tsf_valid, final, host_time, status, type;
	uint8_t					addr[ETH_ALEN];
	struct cfg80211_pmsr_ftm_result		ftm;
};

struct cfg80211_sar_freq_ranges {
	uint32_t				start_freq;
	uint32_t				end_freq;
};

struct cfg80211_sar_sub_specs {
	uint32_t				freq_range_index;
	int					power;
};

struct cfg80211_sar_specs {
	enum nl80211_sar_type			type;
	uint32_t				num_sub_specs;
	struct cfg80211_sar_sub_specs		sub_specs[];
};

struct cfg80211_sar_capa {
	enum nl80211_sar_type			type;
	uint32_t				num_freq_ranges;
	const struct cfg80211_sar_freq_ranges	*freq_ranges;
};

struct cfg80211_ssid {
	int	ssid_len;
	uint8_t	ssid[IEEE80211_MAX_SSID_LEN];
};

struct cfg80211_scan_6ghz_params {
	/* XXX TODO */
	uint8_t				*bssid;
	int	channel_idx, psc_no_listen, short_ssid, short_ssid_valid, unsolicited_probe;
};

struct cfg80211_match_set {
	uint8_t					bssid[ETH_ALEN];
	struct cfg80211_ssid	ssid;
	int			rssi_thold;
};

struct cfg80211_scan_request {
		/* XXX TODO */
	int	duration, duration_mandatory, flags;
	bool					no_cck;
	bool					scan_6ghz;
	struct wireless_dev			*wdev;
	struct wiphy				*wiphy;
	int					ie_len;
	uint8_t					*ie;
	uint8_t					mac_addr[ETH_ALEN], mac_addr_mask[ETH_ALEN];
	uint8_t					bssid[ETH_ALEN];
	int					n_ssids;
	int					n_6ghz_params;
	int					n_channels;
	struct cfg80211_ssid			*ssids;
	struct cfg80211_scan_6ghz_params 	*scan_6ghz_params;
	struct linuxkpi_ieee80211_channel	*channels[0];
};

struct cfg80211_sched_scan_plan {
		/* XXX TODO */
	int	interval, iterations;
};

struct cfg80211_sched_scan_request {
		/* XXX TODO */
	int	delay, flags;
	uint8_t					mac_addr[ETH_ALEN], mac_addr_mask[ETH_ALEN];
	uint64_t				reqid;
	int					n_match_sets;
	int					n_scan_plans;
	int					n_ssids;
	int					n_channels;
	int					ie_len;
	uint8_t					*ie;
	struct cfg80211_match_set		*match_sets;
	struct cfg80211_sched_scan_plan		*scan_plans;
	struct cfg80211_ssid			*ssids;
	struct linuxkpi_ieee80211_channel	*channels[0];
};

struct cfg80211_scan_info {
	uint64_t				scan_start_tsf;
	uint8_t					tsf_bssid[ETH_ALEN];
	bool					aborted;
};

struct cfg80211_beacon_data {
	/* XXX TODO */
	const uint8_t				*head;
	const uint8_t				*tail;
	uint32_t				head_len;
	uint32_t				tail_len;
	const uint8_t				*proberesp_ies;
	const uint8_t				*assocresp_ies;
	uint32_t				proberesp_ies_len;
	uint32_t				assocresp_ies_len;
};

struct cfg80211_ap_settings {
	/* XXX TODO */
	int     auth_type, beacon_interval, dtim_period, hidden_ssid, inactivity_timeout;
	const uint8_t				*ssid;
	size_t					ssid_len;
	struct cfg80211_beacon_data		beacon;
	struct cfg80211_chan_def		chandef;
};

struct cfg80211_bss_selection {
	/* XXX TODO */
	enum nl80211_bss_select_attr		behaviour;
	union {
		enum nl80211_band		band_pref;
		struct {
			enum nl80211_band	band;
			uint8_t			delta;
		} adjust;
	} param;
};

struct cfg80211_crypto {		/* XXX made up name */
	/* XXX TODO */
	enum nl80211_wpa_versions		wpa_versions;
	uint32_t				cipher_group;	/* WLAN_CIPHER_SUITE_* */
	uint32_t				*akm_suites;
	uint32_t				*ciphers_pairwise;
	const uint8_t				*sae_pwd;
	const uint8_t				*psk;
	int					n_akm_suites;
	int					n_ciphers_pairwise;
	int					sae_pwd_len;
};

struct cfg80211_connect_params {
	/* XXX TODO */
	struct linuxkpi_ieee80211_channel	*channel;
	uint8_t					*bssid;
	const uint8_t				*ie;
	const uint8_t				*ssid;
	uint32_t				ie_len;
	uint32_t				ssid_len;
	const void				*key;
	uint32_t				key_len;
	int     auth_type, key_idx, privacy, want_1x;
	struct cfg80211_bss_selection		bss_select;
	struct cfg80211_crypto			crypto;
};

enum bss_param_flags {		/* Used as bitflags. XXX FIXME values? */
	BSS_PARAM_FLAGS_CTS_PROT	= 0x01,
	BSS_PARAM_FLAGS_SHORT_PREAMBLE	= 0x02,
	BSS_PARAM_FLAGS_SHORT_SLOT_TIME = 0x04,
};

struct cfg80211_ibss_params {
	/* XXX TODO */
	int     basic_rates, beacon_interval;
	int	channel_fixed, ie, ie_len, privacy;
	int	dtim_period;
	uint8_t					*ssid;
	uint8_t					*bssid;
	int					ssid_len;
	struct cfg80211_chan_def		chandef;
	enum bss_param_flags			flags;
};

struct cfg80211_mgmt_tx_params {
	/* XXX TODO */
	struct linuxkpi_ieee80211_channel	*chan;
	const uint8_t				*buf;
	size_t					len;
	int	wait;
};

struct cfg80211_pmk_conf {
	/* XXX TODO */
	const uint8_t			*pmk;
	uint8_t				pmk_len;
};

struct cfg80211_pmksa {
	/* XXX TODO */
	const uint8_t			*bssid;
	const uint8_t			*pmkid;
};

struct cfg80211_wowlan_nd_match {
	/* XXX TODO */
	struct cfg80211_ssid		ssid;
	int				n_channels;
	uint32_t			channels[0];	/* freq! = ieee80211_channel_to_frequency() */
};

struct cfg80211_wowlan_nd_info {
	/* XXX TODO */
	int				n_matches;
	struct cfg80211_wowlan_nd_match	*matches[0];
};

enum wiphy_wowlan_support_flags {
	WIPHY_WOWLAN_DISCONNECT,
	WIPHY_WOWLAN_GTK_REKEY_FAILURE,
	WIPHY_WOWLAN_MAGIC_PKT,
	WIPHY_WOWLAN_SUPPORTS_GTK_REKEY,
	WIPHY_WOWLAN_NET_DETECT,
};

struct wiphy_wowlan_support {
	/* XXX TODO */
	enum wiphy_wowlan_support_flags		flags;
	int	max_nd_match_sets, max_pkt_offset, n_patterns, pattern_max_len, pattern_min_len;
};

struct station_del_parameters {
	/* XXX TODO */
	const uint8_t				*mac;
	uint32_t				reason_code;	/* elsewhere uint16_t? */
};

struct station_info {
	/* TODO FIXME */
	int     assoc_req_ies_len, connected_time;
	int	generation, inactive_time, rx_bytes, rx_dropped_misc, rx_packets, signal, tx_bytes, tx_packets;
	int     filled, rx_beacon, rx_beacon_signal_avg, signal_avg;
	int	rx_duration, tx_failed, tx_retries;

	int					chains;
	uint8_t					chain_signal[IEEE80211_MAX_CHAINS];
	uint8_t					chain_signal_avg[IEEE80211_MAX_CHAINS];

	uint8_t					*assoc_req_ies;
	struct rate_info			rxrate;
	struct rate_info			txrate;
	struct cfg80211_ibss_params		bss_param;
	struct nl80211_sta_flag_update		sta_flags;
};

struct station_parameters {
	/* XXX TODO */
	int     sta_flags_mask, sta_flags_set;
};

struct key_params {
	/* XXX TODO */
	const uint8_t	*key;
	const uint8_t	*seq;
	int		key_len;
	int		seq_len;
	uint32_t	cipher;			/* WLAN_CIPHER_SUITE_* */
};

struct mgmt_frame_regs {
	/* XXX TODO */
	int	interface_stypes;
};

struct vif_params {
	/* XXX TODO */
	uint8_t			macaddr[ETH_ALEN];
};

/* That the world needs so many different structs for this is amazing. */
struct mac_address {
	uint8_t	addr[ETH_ALEN];
};

struct ieee80211_reg_rule {
	/* TODO FIXME */
	uint32_t	flags;
	struct freq_range {
		int	start_freq_khz;
		int	end_freq_khz;
		int	max_bandwidth_khz;
	} freq_range;
	struct power_rule {
		int	max_antenna_gain;
		int	max_eirp;
	} power_rule;
};

struct linuxkpi_ieee80211_regdomain {
	/* TODO FIXME */
	uint8_t					alpha2[2];
	int	dfs_region;
	int					n_reg_rules;
	struct ieee80211_reg_rule		reg_rules[];
};

/* XXX-BZ this are insensible values probably ... */
#define	IEEE80211_HE_MAC_CAP0_HTC_HE			0x1
#define	IEEE80211_HE_MAC_CAP0_TWT_REQ			0x2
#define	IEEE80211_HE_MAC_CAP0_TWT_RES			0x4

#define	IEEE80211_HE_MAC_CAP1_LINK_ADAPTATION		0x1
#define	IEEE80211_HE_MAC_CAP1_MULTI_TID_AGG_RX_QOS_8	0x2
#define	IEEE80211_HE_MAC_CAP1_TF_MAC_PAD_DUR_16US	0x4
#define	IEEE80211_HE_MAC_CAP1_TF_MAC_PAD_DUR_MASK	0x8

#define	IEEE80211_HE_MAC_CAP2_32BIT_BA_BITMAP		0x1
#define	IEEE80211_HE_MAC_CAP2_ACK_EN			0x2
#define	IEEE80211_HE_MAC_CAP2_BSR			0x4
#define	IEEE80211_HE_MAC_CAP2_LINK_ADAPTATION		0x8
#define	IEEE80211_HE_MAC_CAP2_BCAST_TWT			0x10
#define	IEEE80211_HE_MAC_CAP2_ALL_ACK			0x20
#define	IEEE80211_HE_MAC_CAP2_MU_CASCADING		0x40
#define	IEEE80211_HE_MAC_CAP2_TRS			0x80

#define	IEEE80211_HE_MAC_CAP3_MAX_AMPDU_LEN_EXP_VHT_2	0x1
#define	IEEE80211_HE_MAC_CAP3_OMI_CONTROL		0x2
#define	IEEE80211_HE_MAC_CAP3_MAX_AMPDU_LEN_EXP_EXT_1	0x10
#define	IEEE80211_HE_MAC_CAP3_MAX_AMPDU_LEN_EXP_EXT_2	0x20
#define	IEEE80211_HE_MAC_CAP3_MAX_AMPDU_LEN_EXP_EXT_3	0x40
#define	IEEE80211_HE_MAC_CAP3_MAX_AMPDU_LEN_EXP_MASK	0x70
#define	IEEE80211_HE_MAC_CAP3_RX_CTRL_FRAME_TO_MULTIBSS	0x80
#define	IEEE80211_HE_MAC_CAP3_FLEX_TWT_SCHED		0x80
#define	IEEE80211_HE_MAC_CAP3_OFDMA_RA			0x80

#define	IEEE80211_HE_MAC_CAP4_AMDSU_IN_AMPDU		0x1
#define	IEEE80211_HE_MAC_CAP4_BQR			0x2
#define	IEEE80211_HE_MAC_CAP4_MULTI_TID_AGG_TX_QOS_B39	0x4
#define	IEEE80211_HE_MAC_CAP4_AMSDU_IN_AMPDU		0x8
#define	IEEE80211_HE_MAC_CAP4_OPS			0x10
#define	IEEE80211_HE_MAC_CAP4_BSRP_BQRP_A_MPDU_AGG	0x20

#define	IEEE80211_HE_MAC_CAP5_HE_DYNAMIC_SM_PS		0x1
#define	IEEE80211_HE_MAC_CAP5_HT_VHT_TRIG_FRAME_RX	0x2
#define	IEEE80211_HE_MAC_CAP5_MULTI_TID_AGG_TX_QOS_B40	0x4
#define	IEEE80211_HE_MAC_CAP5_MULTI_TID_AGG_TX_QOS_B41	0x8
#define	IEEE80211_HE_MAC_CAP5_UL_2x996_TONE_RU		0x10
#define	IEEE80211_HE_MAC_CAP5_OM_CTRL_UL_MU_DATA_DIS_RX	0x20
#define	IEEE80211_HE_MAC_CAP5_PUNCTURED_SOUNDING	0x40
#define	IEEE80211_HE_MAC_CAP5_SUBCHAN_SELECTIVE_TRANSMISSION	0x80

#define	IEEE80211_HE_MCS_NOT_SUPPORTED			0x0
#define	IEEE80211_HE_MCS_SUPPORT_0_7			0x1
#define	IEEE80211_HE_MCS_SUPPORT_0_9			0x2
#define	IEEE80211_HE_MCS_SUPPORT_0_11			0x4

#define	IEEE80211_HE_6GHZ_CAP_TX_ANTPAT_CONS		0x01
#define	IEEE80211_HE_6GHZ_CAP_RX_ANTPAT_CONS		0x02
#define	IEEE80211_HE_6GHZ_CAP_MIN_MPDU_START		0x04
#define	IEEE80211_HE_6GHZ_CAP_MAX_MPDU_LEN		0x08
#define	IEEE80211_HE_6GHZ_CAP_MAX_AMPDU_LEN_EXP		0x10
#define	IEEE80211_HE_6GHZ_CAP_SM_PS			0x20
#define	IEEE80211_HE_6GHZ_MAX_AMPDU_FACTOR		0x40

#define	IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_160MHZ_IN_5G		0x1
#define	IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_80MHZ_IN_5G	0x2
#define	IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_IN_2G		0x4
#define	IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_80PLUS80_MHZ_IN_5G	0x8
#define	IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_RU_MAPPING_IN_2G	0x10
#define	IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_RU_MAPPING_IN_5G	0x20
#define	IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_MASK			0x40

#define	IEEE80211_HE_PHY_CAP1_DEVICE_CLASS_A		0x1
#define	IEEE80211_HE_PHY_CAP1_LDPC_CODING_IN_PAYLOAD	0x2
#define	IEEE80211_HE_PHY_CAP1_MIDAMBLE_RX_TX_MAX_NSTS	0x4
#define	IEEE80211_HE_PHY_CAP1_PREAMBLE_PUNC_RX_MASK	0x8
#define	IEEE80211_HE_PHY_CAP1_HE_LTF_AND_GI_FOR_HE_PPDUS_0_8US	0x10

#define	IEEE80211_HE_PHY_CAP2_MIDAMBLE_RX_TX_MAX_NSTS	0x1
#define	IEEE80211_HE_PHY_CAP2_NDP_4x_LTF_AND_3_2US	0x2
#define	IEEE80211_HE_PHY_CAP2_STBC_TX_UNDER_80MHZ	0x4
#define	IEEE80211_HE_PHY_CAP2_STBC_RX_UNDER_80MHZ	0x8
#define	IEEE80211_HE_PHY_CAP2_DOPPLER_TX		0x10
#define	IEEE80211_HE_PHY_CAP2_UL_MU_PARTIAL_MU_MIMO	0x20
#define	IEEE80211_HE_PHY_CAP2_UL_MU_FULL_MU_MIMO	0x40

#define	IEEE80211_HE_PHY_CAP3_DCM_MAX_CONST_RX_MASK	0x1
#define	IEEE80211_HE_PHY_CAP3_DCM_MAX_CONST_RX_NO_DCM	0x2
#define	IEEE80211_HE_PHY_CAP3_DCM_MAX_CONST_TX_NO_DCM	0x4
#define	IEEE80211_HE_PHY_CAP3_DCM_MAX_RX_NSS_1		0x8
#define	IEEE80211_HE_PHY_CAP3_DCM_MAX_TX_NSS_1		0x10
#define	IEEE80211_HE_PHY_CAP3_SU_BEAMFORMER		0x20
#define	IEEE80211_HE_PHY_CAP3_DCM_MAX_CONST_RX_16_QAM	0x40
#define	IEEE80211_HE_PHY_CAP3_DCM_MAX_CONST_TX_16_QAM	0x80
#define	IEEE80211_HE_PHY_CAP3_DCM_MAX_TX_NSS_2		0x10
#define	IEEE80211_HE_PHY_CAP3_RX_PARTIAL_BW_SU_IN_20MHZ_MU	0x20
#define	IEEE80211_HE_PHY_CAP3_DCM_MAX_CONST_RX_BPSK	0x40
#define	IEEE80211_HE_PHY_CAP3_DCM_MAX_CONST_TX_BPSK	0x80
#define	IEEE80211_HE_PHY_CAP3_DCM_MAX_CONST_TX_MASK	0x80
#define	IEEE80211_HE_PHY_CAP3_DCM_MAX_CONST_RX_QPSK	0x80
#define	IEEE80211_HE_PHY_CAP3_DCM_MAX_CONST_TX_QPSK	0x80
#define	IEEE80211_HE_PHY_CAP3_DCM_MAX_RX_NSS_2		0x80

#define	IEEE80211_HE_PHY_CAP4_BEAMFORMEE_MAX_STS_UNDER_80MHZ_8	0x1
#define	IEEE80211_HE_PHY_CAP4_BEAMFORMEE_MAX_STS_ABOVE_80MHZ_8	0x2
#define	IEEE80211_HE_PHY_CAP4_SU_BEAMFORMEE			0x4
#define	IEEE80211_HE_PHY_CAP4_MU_BEAMFORMER			0x8
#define	IEEE80211_HE_PHY_CAP4_BEAMFORMEE_MAX_STS_UNDER_80MHZ_4	0x10
#define	IEEE80211_HE_PHY_CAP4_BEAMFORMEE_MAX_STS_ABOVE_80MHZ_4	0x20
#define	IEEE80211_HE_PHY_CAP4_BEAMFORMEE_MAX_STS_ABOVE_80MHZ_MASK	0x40
#define	IEEE80211_HE_PHY_CAP4_BEAMFORMEE_MAX_STS_UNDER_80MHZ_MASK	0x80

#define	IEEE80211_HE_PHY_CAP5_BEAMFORMEE_NUM_SND_DIM_ABOVE_80MHZ_2	0x1
#define	IEEE80211_HE_PHY_CAP5_BEAMFORMEE_NUM_SND_DIM_UNDER_80MHZ_2	0x2
#define	IEEE80211_HE_PHY_CAP5_BEAMFORMEE_NUM_SND_DIM_UNDER_80MHZ_MASK	0x4
#define	IEEE80211_HE_PHY_CAP5_NG16_MU_FEEDBACK				0x8
#define	IEEE80211_HE_PHY_CAP5_NG16_SU_FEEDBACK				0x10
#define	IEEE80211_HE_PHY_CAP5_BEAMFORMEE_NUM_SND_DIM_ABOVE_80MHZ_MASK	0x20

#define	IEEE80211_HE_PHY_CAP6_PPE_THRESHOLD_PRESENT	0x1
#define	IEEE80211_HE_PHY_CAP6_TRIG_MU_BEAMFORMER_FB	0x2
#define	IEEE80211_HE_PHY_CAP6_TRIG_SU_BEAMFORMER_FB	0x4
#define	IEEE80211_HE_PHY_CAP6_TRIG_SU_BEAMFORMING_FB	0x8
#define	IEEE80211_HE_PHY_CAP6_TRIG_MU_BEAMFORMING_PARTIAL_BW_FB	0x20
#define	IEEE80211_HE_PHY_CAP6_CODEBOOK_SIZE_42_SU	0x40
#define	IEEE80211_HE_PHY_CAP6_CODEBOOK_SIZE_75_MU	0x80
#define	IEEE80211_HE_PHY_CAP6_PARTIAL_BW_EXT_RANGE	0x80
#define	IEEE80211_HE_PHY_CAP6_TRIG_CQI_FB		0x80
#define	IEEE80211_HE_PHY_CAP6_PARTIAL_BANDWIDTH_DL_MUMIMO	0x80

#define	IEEE80211_HE_PHY_CAP7_HE_SU_MU_PPDU_4XLTF_AND_08_US_GI	0x1
#define	IEEE80211_HE_PHY_CAP7_MAX_NC_1				0x2
#define	IEEE80211_HE_PHY_CAP7_MAX_NC_2				0x4
#define	IEEE80211_HE_PHY_CAP7_MAX_NC_MASK			0x6
#define	IEEE80211_HE_PHY_CAP7_POWER_BOOST_FACTOR_AR		0x8
#define	IEEE80211_HE_PHY_CAP7_POWER_BOOST_FACTOR_SUPP		0x10
#define	IEEE80211_HE_PHY_CAP7_STBC_RX_ABOVE_80MHZ		0x20
#define	IEEE80211_HE_PHY_CAP7_STBC_TX_ABOVE_80MHZ		0x40
#define	IEEE80211_HE_PHY_CAP7_PSR_BASED_SR			0x80

#define	IEEE80211_HE_PHY_CAP8_20MHZ_IN_160MHZ_HE_PPDU		0x1
#define	IEEE80211_HE_PHY_CAP8_20MHZ_IN_40MHZ_HE_PPDU_IN_2G	0x2
#define	IEEE80211_HE_PHY_CAP8_80MHZ_IN_160MHZ_HE_PPDU		0x4
#define	IEEE80211_HE_PHY_CAP8_DCM_MAX_RU_242			0x8
#define	IEEE80211_HE_PHY_CAP8_DCM_MAX_RU_484			0x10
#define	IEEE80211_HE_PHY_CAP8_DCM_MAX_RU_996			0x18
#define	IEEE80211_HE_PHY_CAP8_DCM_MAX_RU_2x996			0x20
#define	IEEE80211_HE_PHY_CAP8_DCM_MAX_RU_MASK			0x28
#define	IEEE80211_HE_PHY_CAP8_HE_ER_SU_PPDU_4XLTF_AND_08_US_GI	0x40
#define	IEEE80211_HE_PHY_CAP8_HE_ER_SU_1XLTF_AND_08_US_GI	0x80

#define	IEEE80211_HE_PHY_CAP9_NOMINAL_PKT_PADDING_0US		0x1
#define	IEEE80211_HE_PHY_CAP9_NOMINAL_PKT_PADDING_16US		0x2
#define	IEEE80211_HE_PHY_CAP9_NOMINAL_PKT_PADDING_8US		0x4
#define	IEEE80211_HE_PHY_CAP9_NOMINAL_PKT_PADDING_MASK		0x8
#define	IEEE80211_HE_PHY_CAP9_NOMINAL_PKT_PADDING_RESERVED	0x10
#define	IEEE80211_HE_PHY_CAP9_NOMINAL_PKT_PADDING_POS		0x0
#define	IEEE80211_HE_PHY_CAP9_NON_TRIGGERED_CQI_FEEDBACK	0x20
#define	IEEE80211_HE_PHY_CAP9_RX_FULL_BW_SU_USING_MU_WITH_COMP_SIGB	0x4
#define	IEEE80211_HE_PHY_CAP9_RX_FULL_BW_SU_USING_MU_WITH_NON_COMP_SIGB	0x8
#define	IEEE80211_HE_PHY_CAP9_RX_1024_QAM_LESS_THAN_242_TONE_RU	0x10
#define	IEEE80211_HE_PHY_CAP9_TX_1024_QAM_LESS_THAN_242_TONE_RU	0x20
#define	IEEE80211_HE_PHY_CAP9_LONGER_THAN_16_SIGB_OFDM_SYM	0x40

#define	IEEE80211_HE_PHY_CAP10_HE_MU_M1RU_MAX_LTF		0x1

#define	VENDOR_CMD_RAW_DATA	(void *)(uintptr_t)(-ENOENT)

struct ieee80211_he_cap_elem {
	u8 mac_cap_info[6];
	u8 phy_cap_info[11];
} __packed;

struct ieee80211_he_mcs_nss_supp {
	/* TODO FIXME */
	uint32_t	rx_mcs_80;
	uint32_t	tx_mcs_80;
	uint32_t	rx_mcs_160;
	uint32_t	tx_mcs_160;
	uint32_t	rx_mcs_80p80;
	uint32_t	tx_mcs_80p80;
};

#define	IEEE80211_STA_HE_CAP_PPE_THRES_MAX	32
struct ieee80211_sta_he_cap {
	/* TODO FIXME */
	int					has_he;
	struct ieee80211_he_cap_elem		he_cap_elem;
	struct ieee80211_he_mcs_nss_supp	he_mcs_nss_supp;
	uint8_t					ppe_thres[IEEE80211_STA_HE_CAP_PPE_THRES_MAX];
};

struct cfg80211_he_bss_color {
	int	color, enabled;
};

struct ieee80211_he_obss_pd {
	bool					enable;
};

struct ieee80211_sta_he_6ghz_capa {
	/* TODO FIXME */
	int	capa;
};

struct ieee80211_sband_iftype_data {
	/* TODO FIXME */
	enum nl80211_iftype			types_mask;
	struct ieee80211_sta_he_cap		he_cap;
	struct ieee80211_sta_he_6ghz_capa	he_6ghz_capa;
	struct {
		const uint8_t			*data;
		size_t				len;
	} vendor_elems;
};

struct ieee80211_supported_band {
	/* TODO FIXME */
	struct linuxkpi_ieee80211_channel	*channels;
	struct ieee80211_rate			*bitrates;
	struct ieee80211_sband_iftype_data	*iftype_data;
	int					n_channels;
	int					n_bitrates;
	int					n_iftype_data;
	enum nl80211_band			band;
	struct ieee80211_sta_ht_cap		ht_cap;
	struct ieee80211_sta_vht_cap		vht_cap;
};

struct cfg80211_pkt_pattern {
	/* XXX TODO */
	uint8_t					*mask;
	uint8_t					*pattern;
	int					pattern_len;
	int					pkt_offset;
};

struct cfg80211_wowlan {
	/* XXX TODO */
	int	disconnect, gtk_rekey_failure, magic_pkt;
	int					n_patterns;
	struct cfg80211_sched_scan_request	*nd_config;
	struct cfg80211_pkt_pattern		*patterns;
};

struct cfg80211_gtk_rekey_data {
	/* XXX TODO */
	int     kck, kek, replay_ctr;
};

struct cfg80211_tid_cfg {
	/* XXX TODO */
	int	mask, noack, retry_long, rtscts, tids;
	enum nl80211_tx_rate_setting		txrate_type;
	struct cfg80211_bitrate_mask		txrate_mask;
};

struct cfg80211_tid_config {
	/* XXX TODO */
	int	n_tid_conf;
	struct cfg80211_tid_cfg			tid_conf[0];
};

struct ieee80211_iface_limit {
	/* TODO FIXME */
	int		max, types;
};

struct ieee80211_iface_combination {
	/* TODO FIXME */
	const struct ieee80211_iface_limit	*limits;
	int					n_limits;
	int		max_interfaces, num_different_channels;
	int		beacon_int_infra_match, beacon_int_min_gcd;
	int		radar_detect_widths;
};

struct iface_combination_params {
	int num_different_channels;
	int iftype_num[NUM_NL80211_IFTYPES];
};

struct regulatory_request {
		/* XXX TODO */
	uint8_t					alpha2[2];
	int	initiator, dfs_region;
};

enum wiphy_vendor_cmd_need_flags {
	WIPHY_VENDOR_CMD_NEED_NETDEV		= 0x01,
	WIPHY_VENDOR_CMD_NEED_RUNNING		= 0x02,
	WIPHY_VENDOR_CMD_NEED_WDEV		= 0x04,
};

struct wiphy_vendor_command {
	struct {
		uint32_t	vendor_id;
		uint32_t	subcmd;
	};
	uint32_t		flags;
	void			*policy;
	int (*doit)(struct wiphy *, struct wireless_dev *, const void *, int);
};

struct wiphy_iftype_ext_capab {
	/* TODO FIXME */
	enum nl80211_iftype			iftype;
	const uint8_t				*extended_capabilities;
	const uint8_t				*extended_capabilities_mask;
	uint8_t					extended_capabilities_len;

};

struct tid_config_support {
	/* TODO FIXME */
	uint64_t				vif;	/* enum nl80211_tid_cfg_attr */
	uint64_t		 		peer;	/* enum nl80211_tid_cfg_attr */
};

enum cfg80211_regulatory {
	REGULATORY_CUSTOM_REG			= BIT(0),
	REGULATORY_STRICT_REG			= BIT(1),
	REGULATORY_DISABLE_BEACON_HINTS		= BIT(2),
	REGULATORY_ENABLE_RELAX_NO_IR		= BIT(3),
	REGULATORY_WIPHY_SELF_MANAGED		= BIT(4),
	REGULATORY_COUNTRY_IE_IGNORE		= BIT(5),
	REGULATORY_COUNTRY_IE_FOLLOW_POWER	= BIT(6),
};

enum wiphy_flags {
	WIPHY_FLAG_AP_UAPSD			= BIT(0),
	WIPHY_FLAG_HAS_CHANNEL_SWITCH		= BIT(1),
	WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL	= BIT(2),
	WIPHY_FLAG_HAVE_AP_SME			= BIT(3),
	WIPHY_FLAG_IBSS_RSN			= BIT(4),
	WIPHY_FLAG_NETNS_OK			= BIT(5),
	WIPHY_FLAG_OFFCHAN_TX			= BIT(6),
	WIPHY_FLAG_PS_ON_BY_DEFAULT		= BIT(7),
	WIPHY_FLAG_SPLIT_SCAN_6GHZ		= BIT(8),
	WIPHY_FLAG_SUPPORTS_EXT_KEK_KCK		= BIT(9),
	WIPHY_FLAG_SUPPORTS_FW_ROAM		= BIT(10),
	WIPHY_FLAG_SUPPORTS_TDLS		= BIT(11),
	WIPHY_FLAG_TDLS_EXTERNAL_SETUP		= BIT(12),
	WIPHY_FLAG_AP_PROBE_RESP_OFFLOAD	= BIT(13),
	WIPHY_FLAG_4ADDR_AP			= BIT(14),
	WIPHY_FLAG_4ADDR_STATION		= BIT(15),
};

struct wiphy {

	struct device				*dev;
	struct mac_address			*addresses;
	int					n_addresses;
	uint32_t				flags;
	struct ieee80211_supported_band		*bands[NUM_NL80211_BANDS];
	uint8_t					perm_addr[ETH_ALEN];

	/* XXX TODO */
	const struct cfg80211_pmsr_capabilities	*pmsr_capa;
	const struct cfg80211_sar_capa		*sar_capa;
	const struct wiphy_iftype_ext_capab	*iftype_ext_capab;
	const struct linuxkpi_ieee80211_regdomain *regd;
	char					fw_version[ETHTOOL_FWVERS_LEN];
	const struct ieee80211_iface_combination *iface_combinations;
	const uint32_t				*cipher_suites;
	int					n_iface_combinations;
	int					n_cipher_suites;
	void(*reg_notifier)(struct wiphy *, struct regulatory_request *);
	enum cfg80211_regulatory		regulatory_flags;
	int					n_vendor_commands;
	const struct wiphy_vendor_command	*vendor_commands;
	const struct ieee80211_txrx_stypes	*mgmt_stypes;
	uint32_t				rts_threshold;
	uint32_t				frag_threshold;
	struct tid_config_support		tid_config_support;

	int	available_antennas_rx, available_antennas_tx;
	int	features, hw_version;
	int	interface_modes, max_match_sets, max_remain_on_channel_duration, max_scan_ie_len, max_scan_ssids, max_sched_scan_ie_len, max_sched_scan_plan_interval, max_sched_scan_plan_iterations, max_sched_scan_plans, max_sched_scan_reqs, max_sched_scan_ssids;
	int	num_iftype_ext_capab;
	int	max_ap_assoc_sta, probe_resp_offload, software_iftypes;
	int     bss_select_support, max_num_pmkids, retry_long, retry_short, signal_type;
	int	max_data_retry_count;
	int     tx_queue_len, rfkill;
	int	mbssid_max_interfaces;

	unsigned long				ext_features[BITS_TO_LONGS(NUM_NL80211_EXT_FEATURES)];
	struct dentry				*debugfsdir;
	struct cfg80211_wowlan_support		*wowlan;
	/* Lower layer (driver/mac80211) specific data. */
	/* Must stay last. */
	uint8_t					priv[0] __aligned(CACHE_LINE_SIZE);
};

struct wireless_dev {
		/* XXX TODO, like ic? */
	int		iftype;
	int     address;
	struct net_device			*netdev;
	struct wiphy				*wiphy;
};

struct cfg80211_ops {
	/* XXX TODO */
	struct wireless_dev *(*add_virtual_intf)(struct wiphy *, const char *, unsigned char, enum nl80211_iftype, struct vif_params *);
	int (*del_virtual_intf)(struct wiphy *,  struct wireless_dev *);
	s32 (*change_virtual_intf)(struct wiphy *,  struct net_device *, enum nl80211_iftype, struct vif_params *);
	s32 (*scan)(struct wiphy *,  struct cfg80211_scan_request *);
	s32 (*set_wiphy_params)(struct wiphy *,  u32);
	s32 (*join_ibss)(struct wiphy *,  struct net_device *, struct cfg80211_ibss_params *);
	s32 (*leave_ibss)(struct wiphy *,  struct net_device *);
	s32 (*get_station)(struct wiphy *, struct net_device *, const u8 *, struct station_info *);
	int (*dump_station)(struct wiphy *,  struct net_device *, int,  u8 *,  struct station_info *);
	s32 (*set_tx_power)(struct wiphy *,  struct wireless_dev *, enum nl80211_tx_power_setting,  s32);
	s32 (*get_tx_power)(struct wiphy *,  struct wireless_dev *, s32 *);
	s32 (*add_key)(struct wiphy *,  struct net_device *, u8,  bool,  const u8 *, struct key_params *);
	s32 (*del_key)(struct wiphy *,  struct net_device *, u8,  bool,  const u8 *);
	s32 (*get_key)(struct wiphy *,  struct net_device *,  u8, bool,  const u8 *,  void *, void(*)(void *, struct key_params *));
	s32 (*set_default_key)(struct wiphy *,  struct net_device *, u8,  bool,  bool);
	s32 (*set_default_mgmt_key)(struct wiphy *, struct net_device *,  u8);
	s32 (*set_power_mgmt)(struct wiphy *,  struct net_device *, bool,  s32);
	s32 (*connect)(struct wiphy *,  struct net_device *, struct cfg80211_connect_params *);
	s32 (*disconnect)(struct wiphy *,  struct net_device *, u16);
	s32 (*suspend)(struct wiphy *, struct cfg80211_wowlan *);
	s32 (*resume)(struct wiphy *);
	s32 (*set_pmksa)(struct wiphy *, struct net_device *, struct cfg80211_pmksa *);
	s32 (*del_pmksa)(struct wiphy *, struct net_device *, struct cfg80211_pmksa *);
	s32 (*flush_pmksa)(struct wiphy *,  struct net_device *);
	s32 (*start_ap)(struct wiphy *,  struct net_device *, struct cfg80211_ap_settings *);
	int (*stop_ap)(struct wiphy *,  struct net_device *);
	s32 (*change_beacon)(struct wiphy *,  struct net_device *, struct cfg80211_beacon_data *);
	int (*del_station)(struct wiphy *,  struct net_device *, struct station_del_parameters *);
	int (*change_station)(struct wiphy *,  struct net_device *, const u8 *,  struct station_parameters *);
	int (*sched_scan_start)(struct wiphy *, struct net_device *, struct cfg80211_sched_scan_request *);
	int (*sched_scan_stop)(struct wiphy *, struct net_device *,  u64);
	void (*update_mgmt_frame_registrations)(struct wiphy *, struct wireless_dev *, struct mgmt_frame_regs *);
	int (*mgmt_tx)(struct wiphy *,  struct wireless_dev *, struct cfg80211_mgmt_tx_params *,  u64 *);
	int (*cancel_remain_on_channel)(struct wiphy *, struct wireless_dev *, u64);
	int (*get_channel)(struct wiphy *, struct wireless_dev *, struct cfg80211_chan_def *);
	int (*crit_proto_start)(struct wiphy *, struct wireless_dev *, enum nl80211_crit_proto_id, u16);
	void (*crit_proto_stop)(struct wiphy *, struct wireless_dev *);
	int (*tdls_oper)(struct wiphy *, struct net_device *,  const u8 *, enum nl80211_tdls_operation);
	int (*update_connect_params)(struct wiphy *, struct net_device *, struct cfg80211_connect_params *, u32);
	int (*set_pmk)(struct wiphy *,  struct net_device *, const struct cfg80211_pmk_conf *);
	int (*del_pmk)(struct wiphy *,  struct net_device *, const u8 *);
	int (*remain_on_channel)(struct wiphy *,  struct wireless_dev *, struct linuxkpi_ieee80211_channel *, unsigned int,  u64 *);
	int (*start_p2p_device)(struct wiphy *,  struct wireless_dev *);
	void (*stop_p2p_device)(struct wiphy *,  struct wireless_dev *);
};


/* -------------------------------------------------------------------------- */

/* linux_80211.c */

struct wiphy *linuxkpi_wiphy_new(const struct cfg80211_ops *, size_t);
void linuxkpi_wiphy_free(struct wiphy *wiphy);

int linuxkpi_regulatory_set_wiphy_regd_sync(struct wiphy *wiphy,
    struct linuxkpi_ieee80211_regdomain *regd);
uint32_t linuxkpi_ieee80211_channel_to_frequency(uint32_t, enum nl80211_band);
uint32_t linuxkpi_ieee80211_frequency_to_channel(uint32_t, uint32_t);
struct linuxkpi_ieee80211_channel *
    linuxkpi_ieee80211_get_channel(struct wiphy *, uint32_t);
void linuxkpi_cfg80211_bss_flush(struct wiphy *);

/* -------------------------------------------------------------------------- */

static __inline struct wiphy *
wiphy_new(const struct cfg80211_ops *ops, size_t priv_len)
{

	return (linuxkpi_wiphy_new(ops, priv_len));
}

static __inline void
wiphy_free(struct wiphy *wiphy)
{

	linuxkpi_wiphy_free(wiphy);
}

static __inline void *
wiphy_priv(struct wiphy *wiphy)
{

	return (wiphy->priv);
}

static __inline void
set_wiphy_dev(struct wiphy *wiphy, struct device *dev)
{

	wiphy->dev = dev;
}

static __inline struct device *
wiphy_dev(struct wiphy *wiphy)
{

	return (wiphy->dev);
}

#define	wiphy_err(_wiphy, _fmt, ...)					\
    dev_err((_wiphy)->dev, _fmt, __VA_ARGS__)

static __inline const struct linuxkpi_ieee80211_regdomain *
wiphy_dereference(struct wiphy *wiphy,
    const struct linuxkpi_ieee80211_regdomain *regd)
{
	TODO();
        return (NULL);
}

static __inline void
wiphy_lock(struct wiphy *wiphy)
{
	TODO();
}

static __inline void
wiphy_unlock(struct wiphy *wiphy)
{
	TODO();
}

static __inline void
wiphy_rfkill_set_hw_state_reason(struct wiphy *wiphy, bool blocked,
    enum rfkill_hard_block_reasons reason)
{
	TODO();
}

/* -------------------------------------------------------------------------- */

static __inline bool
rfkill_blocked(int rfkill)		/* argument type? */
{
	TODO();
	return (false);
}

static __inline bool
rfkill_soft_blocked(int rfkill)
{
	TODO();
	return (false);
}

static __inline int
reg_query_regdb_wmm(uint8_t *alpha2, uint32_t center_freq,
    struct ieee80211_reg_rule *rule)
{

	/* ETSI has special rules. FreeBSD regdb needs to learn about them. */
	TODO();

	return (-ENXIO);
}

static __inline const u8 *
cfg80211_find_ie_match(uint32_t f, const u8 *ies, size_t ies_len,
    const u8 *match, int x, int y)
{
	TODO();
	return (NULL);
}

static __inline const u8 *
cfg80211_find_ie(uint8_t eid, const uint8_t *ie, uint32_t ielen)
{
	TODO();
	return (NULL);
}

static __inline void
cfg80211_pmsr_complete(struct wireless_dev *wdev,
    struct cfg80211_pmsr_request *req, gfp_t gfp)
{
	TODO();
}

static __inline void
cfg80211_pmsr_report(struct wireless_dev *wdev,
    struct cfg80211_pmsr_request *req,
    struct cfg80211_pmsr_result *result, gfp_t gfp)
{
	TODO();
}

static __inline void
cfg80211_chandef_create(struct cfg80211_chan_def *chandef,
    struct linuxkpi_ieee80211_channel *chan, enum nl80211_chan_flags chan_flag)
{

	KASSERT(chandef != NULL, ("%s: chandef is NULL\n", __func__));
	KASSERT(chan != NULL, ("%s: chan is NULL\n", __func__));

	memset(chandef, 0, sizeof(*chandef));
	chandef->chan = chan;
	chandef->center_freq2 = 0;	/* Set here and only overwrite if needed. */

	switch (chan_flag) {
	case NL80211_CHAN_NO_HT:
		chandef->width = NL80211_CHAN_WIDTH_20_NOHT;
		chandef->center_freq1 = chan->center_freq;
		break;
	default:
		if (chan->flags & IEEE80211_CHAN_NO_HT40)
			chandef->width = NL80211_CHAN_WIDTH_20;
		else if (chan->flags & IEEE80211_CHAN_NO_80MHZ)
			chandef->width = NL80211_CHAN_WIDTH_40;
		else if (chan->flags & IEEE80211_CHAN_NO_160MHZ)
			chandef->width = NL80211_CHAN_WIDTH_80;
		else {
			chandef->width = NL80211_CHAN_WIDTH_160;
			IMPROVE("80P80 and 320 ...");
		}
		chandef->center_freq1 = chan->center_freq;
		break;
	};
}

static __inline void
cfg80211_bss_iter(struct wiphy *wiphy, struct cfg80211_chan_def *chandef,
    void (*iterfunc)(struct wiphy *, struct cfg80211_bss *, void *), void *data)
{
	TODO();
}

struct element {
	uint8_t					id;
	uint8_t					datalen;
	uint8_t					data[0];
} __packed;

static __inline const struct element *
cfg80211_find_elem(enum ieee80211_eid eid, uint8_t *data, size_t len)
{
	TODO();
	return (NULL);
}

static __inline uint32_t
cfg80211_calculate_bitrate(struct rate_info *rate)
{
	TODO();
	return (-1);
}

static __inline uint32_t
ieee80211_channel_to_frequency(uint32_t channel, enum nl80211_band band)
{

	return (linuxkpi_ieee80211_channel_to_frequency(channel, band));
}

static __inline uint32_t
ieee80211_frequency_to_channel(uint32_t freq)
{

	return (linuxkpi_ieee80211_frequency_to_channel(freq, 0));
}

static __inline int
regulatory_set_wiphy_regd_sync(struct wiphy *wiphy,
    struct linuxkpi_ieee80211_regdomain *regd)
{
	IMPROVE();
	return (linuxkpi_regulatory_set_wiphy_regd_sync(wiphy, regd));
}

static __inline int
regulatory_set_wiphy_regd_sync_rtnl(struct wiphy *wiphy,
    struct linuxkpi_ieee80211_regdomain *regd)
{

	IMPROVE();
	return (linuxkpi_regulatory_set_wiphy_regd_sync(wiphy, regd));
}

static __inline int
regulatory_set_wiphy_regd(struct wiphy *wiphy,
    struct linuxkpi_ieee80211_regdomain *regd)
{

	IMPROVE();
	if (regd == NULL)
		return (EINVAL);

	/* XXX-BZ wild guessing here based on brcmfmac. */
	if (wiphy->regulatory_flags & REGULATORY_WIPHY_SELF_MANAGED)
		wiphy->regd = regd;
	else
		return (EPERM);

	/* XXX FIXME, do we have to do anything with reg_notifier? */
	return (0);
}

static __inline int
regulatory_hint(struct wiphy *wiphy, const uint8_t *alpha2)
{
	TODO();
	return (-ENXIO);
}

static __inline const char *
reg_initiator_name(enum nl80211_reg_initiator initiator)
{
	TODO();
	return (NULL);
}

static __inline struct linuxkpi_ieee80211_regdomain *
rtnl_dereference(const struct linuxkpi_ieee80211_regdomain *regd)
{
	TODO();
	return (NULL);
}

static __inline struct ieee80211_reg_rule *
freq_reg_info(struct wiphy *wiphy, uint32_t center_freq)
{
	TODO();
	return (NULL);
}

static __inline struct cfg80211_bss *
cfg80211_get_bss(struct wiphy *wiphy, struct linuxkpi_ieee80211_channel *chan,
    uint8_t *bssid, void *p, int x, uint32_t f1, uint32_t f2)
{
	TODO();
	return (NULL);
}

static __inline void
cfg80211_put_bss(struct wiphy *wiphy, struct cfg80211_bss *bss)
{
	TODO();
}

static __inline void
wiphy_apply_custom_regulatory(struct wiphy *wiphy,
    const struct linuxkpi_ieee80211_regdomain *regd)
{
	TODO();
}

static __inline char *
wiphy_name(struct wiphy *wiphy)
{
	if (wiphy != NULL && wiphy->dev != NULL)
		return dev_name(wiphy->dev);
	else {
		IMPROVE("wlanNA");
		return ("wlanNA");
	}
}

static __inline void
wiphy_read_of_freq_limits(struct wiphy *wiphy)
{
#ifdef FDT
	TODO();
#endif
}

static __inline uint8_t *
cfg80211_find_vendor_ie(unsigned int oui, u8 oui_type,
    uint8_t *data, size_t len)
{
	TODO();
	return (NULL);
}

static __inline void
wiphy_ext_feature_set(struct wiphy *wiphy, enum nl80211_ext_feature ef)
{

	set_bit(ef, wiphy->ext_features);
}

static __inline void *
wiphy_net(struct wiphy *wiphy)
{
	TODO();
	return (NULL);	/* XXX passed to dev_net_set() */
}

static __inline int
wiphy_register(struct wiphy *wiphy)
{
	TODO();
	return (0);
}

static __inline void
wiphy_unregister(struct wiphy *wiphy)
{
	TODO();
}

static __inline void
wiphy_warn(struct wiphy *wiphy, const char *fmt, ...)
{
	TODO();
}

static __inline int
cfg80211_check_combinations(struct wiphy *wiphy,
    struct iface_combination_params *params)
{
	TODO();
	return (-ENOENT);
}

static __inline uint8_t
cfg80211_classify8021d(struct sk_buff *skb, void *p)
{
	TODO();
	return (0);
}

static __inline void
cfg80211_connect_done(struct net_device *ndev,
    struct cfg80211_connect_resp_params *conn_params, gfp_t gfp)
{
	TODO();
}

static __inline void
cfg80211_crit_proto_stopped(struct wireless_dev *wdev, gfp_t gfp)
{
	TODO();
}

static __inline void
cfg80211_disconnected(struct net_device *ndev, uint16_t reason,
    void *p, int x, bool locally_generated, gfp_t gfp)
{
	TODO();
}

static __inline int
cfg80211_get_p2p_attr(const u8 *ie, u32 ie_len,
    enum ieee80211_p2p_attr_ids attr, u8 *p, size_t p_len)
{
	TODO();
	return (-1);
}

static __inline void
cfg80211_ibss_joined(struct net_device *ndev, const uint8_t *addr,
    struct linuxkpi_ieee80211_channel *chan, gfp_t gfp)
{
	TODO();
}

static __inline struct cfg80211_bss *
cfg80211_inform_bss(struct wiphy *wiphy,
    struct linuxkpi_ieee80211_channel *channel,
    enum cfg80211_bss_frame_type bss_ftype, const uint8_t *bss, int _x,
    uint16_t cap, uint16_t intvl, const uint8_t *ie, size_t ie_len,
    int signal, gfp_t gfp)
{
	TODO();
	return (NULL);
}

static __inline struct cfg80211_bss *
cfg80211_inform_bss_data(struct wiphy *wiphy,
    struct cfg80211_inform_bss *bss_data,
    enum cfg80211_bss_frame_type bss_ftype, const uint8_t *bss, int _x,
    uint16_t cap, uint16_t intvl, const uint8_t *ie, size_t ie_len, gfp_t gfp)
{
	TODO();
	return (NULL);
}

static __inline void
cfg80211_mgmt_tx_status(struct wireless_dev *wdev, uint64_t cookie,
    const u8 *buf, size_t len, bool ack, gfp_t gfp)
{
	TODO();
}

static __inline void
cfg80211_michael_mic_failure(struct net_device *ndev, const uint8_t *addr,
    enum nl80211_key_type key_type, int _x, void *p, gfp_t gfp)
{
	TODO();
}

static __inline void
cfg80211_new_sta(struct net_device *ndev, const uint8_t *addr,
    struct station_info *sinfo, gfp_t gfp)
{
	TODO();
}

static __inline void
cfg80211_del_sta(struct net_device *ndev, const uint8_t *addr, gfp_t gfp)
{
	TODO();
}

static __inline void
cfg80211_port_authorized(struct net_device *ndev, const uint8_t *bssid,
    gfp_t gfp)
{
	TODO();
}

static __inline void
cfg80211_ready_on_channel(struct wireless_dev *wdev, uint64_t cookie,
    struct linuxkpi_ieee80211_channel *channel, unsigned int duration,
    gfp_t gfp)
{
	TODO();
}

static __inline void
cfg80211_remain_on_channel_expired(struct wireless_dev *wdev,
    uint64_t cookie, struct linuxkpi_ieee80211_channel *channel, gfp_t gfp)
{
	TODO();
}

static __inline void
cfg80211_report_wowlan_wakeup(void)
{
	TODO();
}

static __inline void
cfg80211_roamed(struct net_device *ndev, struct cfg80211_roam_info *roam_info,
    gfp_t gfp)
{
	TODO();
}

static __inline void
cfg80211_rx_mgmt(struct wireless_dev *wdev, int freq, int _x,
    uint8_t *p, size_t p_len, int _x2)
{
	TODO();
}

static __inline void
cfg80211_scan_done(struct cfg80211_scan_request *scan_request,
    struct cfg80211_scan_info *info)
{
	TODO();
}

static __inline void
cfg80211_sched_scan_results(struct wiphy *wiphy, uint64_t reqid)
{
	TODO();
}

static __inline void
cfg80211_sched_scan_stopped(struct wiphy *wiphy, int _x)
{
	TODO();
}

static __inline void
cfg80211_unregister_wdev(struct wireless_dev *wdev)
{
	TODO();
}

static __inline struct sk_buff *
cfg80211_vendor_cmd_alloc_reply_skb(struct wiphy *wiphy, unsigned int len)
{
	TODO();
	return (NULL);
}

static __inline int
cfg80211_vendor_cmd_reply(struct sk_buff *skb)
{
	TODO();
	return (-ENXIO);
}

static __inline struct linuxkpi_ieee80211_channel *
ieee80211_get_channel(struct wiphy *wiphy, uint32_t freq)
{

	return (linuxkpi_ieee80211_get_channel(wiphy, freq));
}

static __inline size_t
ieee80211_get_hdrlen_from_skb(struct sk_buff *skb)
{

	TODO();
	return (-1);
}

static __inline void
cfg80211_bss_flush(struct wiphy *wiphy)
{

	linuxkpi_cfg80211_bss_flush(wiphy);
}

static __inline bool
cfg80211_channel_is_psc(struct linuxkpi_ieee80211_channel *channel)
{

	/* Only 6Ghz. */
	if (channel->band != NL80211_BAND_6GHZ)
		return (false);

	TODO();
	return (false);
}

static __inline int
cfg80211_get_ies_channel_number(const uint8_t *ie, size_t len,
    enum nl80211_band band, enum cfg80211_bss_frame_type ftype)
{

	TODO();
	return (-1);
}

/* Used for scanning at least. */
static __inline void
get_random_mask_addr(uint8_t *dst, const uint8_t *addr, const uint8_t *mask)
{
	int i;

	/* Get a completely random address and then overlay what we want. */
	get_random_bytes(dst, ETH_ALEN);
	for (i = 0; i < ETH_ALEN; i++)
		dst[i] = (dst[i] & ~(mask[i])) | (addr[i] & mask[i]);
}

static __inline void
cfg80211_shutdown_all_interfaces(struct wiphy *wiphy)
{
	TODO();
}

static __inline bool
cfg80211_reg_can_beacon(struct wiphy *wiphy, struct cfg80211_chan_def *chandef,
    enum nl80211_iftype iftype)
{
	TODO();
	return (false);
}

static __inline void
cfg80211_background_radar_event(struct wiphy *wiphy,
    struct cfg80211_chan_def *chandef, gfp_t gfp)
{
	TODO();
}

static __inline const u8 *
cfg80211_find_ext_ie(uint8_t eid, uint8_t *p, size_t len)
{
	TODO();
	return (NULL);
}

static __inline bool
cfg80211_chandef_valid(const struct cfg80211_chan_def *chandef)
{
	TODO();
	return (false);
}

#define	wiphy_info(wiphy, fmt, ...)					\
	printf("%s:%d XXX TODO " fmt, __func__, __LINE__, __VA_ARGS__)

#ifndef LINUXKPI_NET80211
#define	ieee80211_channel		linuxkpi_ieee80211_channel
#define	ieee80211_regdomain		linuxkpi_ieee80211_regdomain
/* net80211::IEEE80211_VHT_MCS_SUPPORT_0_n() conflicts */
#if defined(IEEE80211_VHT_MCS_SUPPORT_0_7)
#undef	IEEE80211_VHT_MCS_SUPPORT_0_7
#endif
#if defined(IEEE80211_VHT_MCS_SUPPORT_0_8)
#undef	IEEE80211_VHT_MCS_SUPPORT_0_8
#endif
#if defined(IEEE80211_VHT_MCS_SUPPORT_0_9)
#undef	IEEE80211_VHT_MCS_SUPPORT_0_9
#endif
#define	IEEE80211_VHT_MCS_SUPPORT_0_7	LKPI_IEEE80211_VHT_MCS_SUPPORT_0_7
#define	IEEE80211_VHT_MCS_SUPPORT_0_8	LKPI_IEEE80211_VHT_MCS_SUPPORT_0_8
#define	IEEE80211_VHT_MCS_SUPPORT_0_9	LKPI_IEEE80211_VHT_MCS_SUPPORT_0_9
#endif

#endif	/* _LINUXKPI_NET_CFG80211_H */
