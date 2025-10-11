/*-
 * Copyright (c) 2020-2025 The FreeBSD Foundation
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
 */

#ifndef	_LINUXKPI_NET_CFG80211_H
#define	_LINUXKPI_NET_CFG80211_H

#include <linux/types.h>
#include <linux/nl80211.h>
#include <linux/ieee80211.h>
#include <linux/mutex.h>
#include <linux/if_ether.h>
#include <linux/ethtool.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/netdevice.h>
#include <linux/random.h>
#include <linux/skbuff.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <net/regulatory.h>

#include <net80211/ieee80211.h>

/* linux_80211.c */
extern int linuxkpi_debug_80211;
#ifndef	D80211_TODO
#define	D80211_TODO		0x1
#endif
#ifndef	D80211_IMPROVE
#define	D80211_IMPROVE		0x2
#endif
#define	TODO(fmt, ...)		if (linuxkpi_debug_80211 & D80211_TODO)	\
    printf("%s:%d: XXX LKPI80211 TODO " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#define	IMPROVE(fmt, ...)	if (linuxkpi_debug_80211 & D80211_IMPROVE)	\
    printf("%s:%d: XXX LKPI80211 IMPROVE " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)

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
	RATE_INFO_FLAGS_MCS		= BIT(0),
	RATE_INFO_FLAGS_VHT_MCS		= BIT(1),
	RATE_INFO_FLAGS_SHORT_GI	= BIT(2),
	RATE_INFO_FLAGS_HE_MCS		= BIT(4),
	RATE_INFO_FLAGS_EHT_MCS		= BIT(7),
	/* Max 8 bits as used in struct rate_info. */
};

#define	CFG80211_RATE_INFO_FLAGS_BITS					\
    "\20\1MCS\2VHT_MCS\3SGI\5HE_MCS\10EHT_MCS"

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
	IEEE80211_CHAN_NO_6GHZ_VLP_CLIENT	= BIT(10),
	IEEE80211_CHAN_NO_6GHZ_AFC_CLIENT	= BIT(11),
	IEEE80211_CHAN_PSD			= BIT(12),
	IEEE80211_CHAN_ALLOW_6GHZ_VLP_AP	= BIT(13),
	IEEE80211_CHAN_CAN_MONITOR		= BIT(14),
};
#define	IEEE80211_CHAN_NO_HT40	(IEEE80211_CHAN_NO_HT40MINUS|IEEE80211_CHAN_NO_HT40PLUS)

struct ieee80211_txrx_stypes {
	uint16_t	tx;
	uint16_t	rx;
};

/*
 * net80211 has an ieee80211_channel as well; we use the linuxkpi_ version
 * interally in LinuxKPI and re-define ieee80211_channel for the drivers
 * at the end of the file.
 */
struct linuxkpi_ieee80211_channel {
	uint32_t				center_freq;
	uint16_t				hw_value;
	enum ieee80211_channel_flags		flags;
	enum nl80211_band			band;
	bool					beacon_found;
	enum nl80211_dfs_state			dfs_state;
	unsigned int				dfs_cac_ms;
	int					max_antenna_gain;
	int					max_power;
	int					max_reg_power;
	uint32_t				orig_flags;
	int					orig_mpwr;
};

struct cfg80211_bitrate_mask {
	/* TODO FIXME */
	struct {
		uint32_t			legacy;
		uint8_t				ht_mcs[IEEE80211_HT_MCS_MASK_LEN];
		uint16_t			vht_mcs[8];
		uint16_t			he_mcs[8];
		enum nl80211_txrate_gi		gi;
		enum nl80211_he_gi		he_gi;
		uint8_t				he_ltf;		/* XXX enum? */
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
	RATE_INFO_BW_320,
	RATE_INFO_BW_EHT_RU,
};

struct rate_info {
	uint8_t					flags;			/* enum cfg80211_rate_info_flags */
	uint8_t					bw;			/* enum rate_info_bw */
	uint16_t				legacy;
	uint8_t					mcs;
	uint8_t					nss;
	uint8_t					he_dcm;
	uint8_t					he_gi;
	uint8_t					he_ru_alloc;
	uint8_t					eht_gi;
};

struct ieee80211_rate {
	uint32_t		flags;					/* enum ieee80211_rate_flags */
	uint16_t		bitrate;
	uint16_t		hw_value;
	uint16_t		hw_value_short;
};

struct ieee80211_sta_ht_cap {
	bool					ht_supported;
	uint8_t					ampdu_density;
	uint8_t					ampdu_factor;
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
#define	IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_MASK			IEEE80211_VHTCAP_SUPP_CHAN_WIDTH_MASK

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

#define	IEEE80211_VHT_CAP_EXT_NSS_BW_MASK	IEEE80211_VHTCAP_EXT_NSS_BW
#define	IEEE80211_VHT_CAP_EXT_NSS_BW_SHIFT	IEEE80211_VHTCAP_EXT_NSS_BW_S

struct ieee80211_sta_vht_cap {
		/* TODO FIXME */
	bool				vht_supported;
	uint32_t			cap;
	struct ieee80211_vht_mcs_info	vht_mcs;
};

enum ieee80211_vht_opmode {
	IEEE80211_OPMODE_NOTIF_RX_NSS_SHIFT	= 4,
};

struct cfg80211_bss_ies {
	uint8_t				*data;
	size_t				len;
};

struct cfg80211_bss {
		/* XXX TODO */
	struct cfg80211_bss_ies		*ies;
	struct cfg80211_bss_ies		*beacon_ies;
	uint64_t			ts_boottime;
	int32_t				signal;
};

struct cfg80211_connect_resp_params {
		/* XXX TODO */
	uint8_t				*bssid;
	const uint8_t			*req_ie;
	const uint8_t			*resp_ie;
	uint32_t			req_ie_len;
	uint32_t			resp_ie_len;
	int				status;
	struct {
		const uint8_t			*addr;
		const uint8_t			*bssid;
		struct cfg80211_bss		*bss;
		uint16_t			status;
	} links[IEEE80211_MLD_MAX_NUM_LINKS];
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
	struct {
		const uint8_t			*addr;
		const uint8_t			*bssid;
		struct cfg80211_bss		*bss;
		struct linuxkpi_ieee80211_channel *channel;
	} links[IEEE80211_MLD_MAX_NUM_LINKS];
};

struct cfg80211_chan_def {
		/* XXX TODO */
	struct linuxkpi_ieee80211_channel	*chan;
	enum nl80211_chan_width		width;
	uint32_t			center_freq1;
	uint32_t			center_freq2;
	uint16_t			punctured;
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
	int	channel_idx, psc_no_listen, short_ssid, short_ssid_valid, unsolicited_probe, psd_20;
};

struct cfg80211_match_set {
	uint8_t					bssid[ETH_ALEN];
	struct cfg80211_ssid	ssid;
	int			rssi_thold;
};

struct cfg80211_scan_request {
		/* XXX TODO */
	bool					no_cck;
	bool					scan_6ghz;
	bool					duration_mandatory;
	bool					first_part;
	int8_t					tsf_report_link_id;
	uint16_t				duration;
	uint32_t				flags;
	struct wireless_dev			*wdev;
	struct wiphy				*wiphy;
	uint64_t				scan_start;
	uint32_t				rates[NUM_NL80211_BANDS];
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

struct cfg80211_ap_update {
	/* XXX TODO */
	struct cfg80211_beacon_data		beacon;
};

struct cfg80211_crypto_settings {
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

struct cfg80211_ap_settings {
	/* XXX TODO */
	int     auth_type, beacon_interval, dtim_period, hidden_ssid, inactivity_timeout;
	const uint8_t				*ssid;
	size_t					ssid_len;
	struct cfg80211_beacon_data		beacon;
	struct cfg80211_chan_def		chandef;
	struct cfg80211_crypto_settings		crypto;
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

struct cfg80211_connect_params {
	/* XXX TODO */
	struct linuxkpi_ieee80211_channel	*channel;
	struct linuxkpi_ieee80211_channel	*channel_hint;
	uint8_t					*bssid;
	uint8_t					*bssid_hint;
	const uint8_t				*ie;
	const uint8_t				*ssid;
	uint32_t				ie_len;
	uint32_t				ssid_len;
	const void				*key;
	uint32_t				key_len;
	int     auth_type, key_idx, privacy, want_1x;
	struct cfg80211_bss_selection		bss_select;
	struct cfg80211_crypto_settings		crypto;
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

struct cfg80211_external_auth_params {
	uint8_t					bssid[ETH_ALEN];
        uint16_t				status;
        enum nl80211_external_auth_action	action;
        unsigned int				key_mgmt_suite;
        struct cfg80211_ssid			ssid;
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
	const uint8_t			*ssid;
	size_t				ssid_len;
};

struct station_del_parameters {
	/* XXX TODO */
	const uint8_t				*mac;
	uint32_t				reason_code;	/* elsewhere uint16_t? */
};

struct station_info {
	uint64_t				filled;		/* enum nl80211_sta_info */
	uint32_t				connected_time;
	uint32_t				inactive_time;

	uint64_t				rx_bytes;
	uint32_t				rx_packets;
	uint32_t				rx_dropped_misc;

	uint64_t				rx_duration;
	uint32_t				rx_beacon;
	uint8_t					rx_beacon_signal_avg;

	int8_t					signal;
	int8_t					signal_avg;
	int8_t					ack_signal;
	int8_t					avg_ack_signal;

	/* gap */
	int					generation;

	uint64_t				tx_bytes;
	uint32_t				tx_packets;
	uint32_t				tx_failed;
	uint64_t				tx_duration;
	uint32_t				tx_retries;

	int					chains;
	uint8_t					chain_signal[IEEE80211_MAX_CHAINS];
	uint8_t					chain_signal_avg[IEEE80211_MAX_CHAINS];

	uint8_t					*assoc_req_ies;
	size_t					assoc_req_ies_len;

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
	int	dfs_cac_ms;
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

#define	IEEE80211_EHT_MAC_CAP0_EPCS_PRIO_ACCESS			0x01
#define	IEEE80211_EHT_MAC_CAP0_MAX_MPDU_LEN_11454		0x02
#define	IEEE80211_EHT_MAC_CAP0_MAX_MPDU_LEN_MASK		0x03
#define	IEEE80211_EHT_MAC_CAP0_OM_CONTROL			0x04
#define	IEEE80211_EHT_MAC_CAP0_TRIG_TXOP_SHARING_MODE1		0x05
#define	IEEE80211_EHT_MAC_CAP0_TRIG_TXOP_SHARING_MODE2		0x06
#define	IEEE80211_EHT_MAC_CAP0_MAX_MPDU_LEN_7991		0x07
#define	IEEE80211_EHT_MAC_CAP0_SCS_TRAFFIC_DESC			0x08

#define	IEEE80211_EHT_MAC_CAP1_MAX_AMPDU_LEN_MASK		0x01

#define	IEEE80211_EHT_MCS_NSS_RX				0x01
#define	IEEE80211_EHT_MCS_NSS_TX				0x02

#define	IEEE80211_EHT_PHY_CAP0_242_TONE_RU_GT20MHZ		0x01
#define	IEEE80211_EHT_PHY_CAP0_320MHZ_IN_6GHZ			0x02
#define	IEEE80211_EHT_PHY_CAP0_BEAMFORMEE_SS_80MHZ_MASK		0x03
#define	IEEE80211_EHT_PHY_CAP0_NDP_4_EHT_LFT_32_GI		0x04
#define	IEEE80211_EHT_PHY_CAP0_PARTIAL_BW_UL_MU_MIMO		0x05
#define	IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMEE			0x06
#define	IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMER			0x07

#define	IEEE80211_EHT_PHY_CAP1_BEAMFORMEE_SS_160MHZ_MASK	0x01
#define	IEEE80211_EHT_PHY_CAP1_BEAMFORMEE_SS_320MHZ_MASK	0x02
#define	IEEE80211_EHT_PHY_CAP1_BEAMFORMEE_SS_80MHZ_MASK		0x03

#define	IEEE80211_EHT_PHY_CAP2_SOUNDING_DIM_80MHZ_MASK		0x01
#define	IEEE80211_EHT_PHY_CAP2_SOUNDING_DIM_160MHZ_MASK		0x02
#define	IEEE80211_EHT_PHY_CAP2_SOUNDING_DIM_320MHZ_MASK		0x03

#define	IEEE80211_EHT_PHY_CAP3_CODEBOOK_4_2_SU_FDBK		0x01
#define	IEEE80211_EHT_PHY_CAP3_CODEBOOK_7_5_MU_FDBK		0x02
#define	IEEE80211_EHT_PHY_CAP3_NG_16_MU_FEEDBACK		0x03
#define	IEEE80211_EHT_PHY_CAP3_NG_16_SU_FEEDBACK		0x04
#define	IEEE80211_EHT_PHY_CAP3_TRIG_CQI_FDBK			0x05
#define	IEEE80211_EHT_PHY_CAP3_TRIG_MU_BF_PART_BW_FDBK		0x06
#define	IEEE80211_EHT_PHY_CAP3_TRIG_SU_BF_FDBK			0x07
#define	IEEE80211_EHT_PHY_CAP3_SOUNDING_DIM_320MHZ_MASK		0x08

#define	IEEE80211_EHT_PHY_CAP4_EHT_MU_PPDU_4_EHT_LTF_08_GI	0x01
#define	IEEE80211_EHT_PHY_CAP4_PART_BW_DL_MU_MIMO		0x02
#define	IEEE80211_EHT_PHY_CAP4_POWER_BOOST_FACT_SUPP		0x03
#define	IEEE80211_EHT_PHY_CAP4_MAX_NC_MASK			0x04

#define	IEEE80211_EHT_PHY_CAP5_COMMON_NOMINAL_PKT_PAD_0US	0x01
#define	IEEE80211_EHT_PHY_CAP5_COMMON_NOMINAL_PKT_PAD_16US	0x02
#define	IEEE80211_EHT_PHY_CAP5_COMMON_NOMINAL_PKT_PAD_20US	0x03
#define	IEEE80211_EHT_PHY_CAP5_COMMON_NOMINAL_PKT_PAD_8US	0x04
#define	IEEE80211_EHT_PHY_CAP5_COMMON_NOMINAL_PKT_PAD_MASK	0x05
#define	IEEE80211_EHT_PHY_CAP5_NON_TRIG_CQI_FEEDBACK		0x06
#define	IEEE80211_EHT_PHY_CAP5_PPE_THRESHOLD_PRESENT		0x07
#define	IEEE80211_EHT_PHY_CAP5_RX_LESS_242_TONE_RU_SUPP		0x08
#define	IEEE80211_EHT_PHY_CAP5_TX_LESS_242_TONE_RU_SUPP		0x09
#define	IEEE80211_EHT_PHY_CAP5_MAX_NUM_SUPP_EHT_LTF_MASK	0x0a
#define	IEEE80211_EHT_PHY_CAP5_SUPP_EXTRA_EHT_LTF		0x0b

#define	IEEE80211_EHT_PHY_CAP6_EHT_DUP_6GHZ_SUPP		0x01
#define	IEEE80211_EHT_PHY_CAP6_MCS15_SUPP_MASK			0x02
#define	IEEE80211_EHT_PHY_CAP6_MAX_NUM_SUPP_EHT_LTF_MASK	0x03

#define	IEEE80211_EHT_PHY_CAP7_MU_BEAMFORMER_80MHZ		0x01
#define	IEEE80211_EHT_PHY_CAP7_MU_BEAMFORMER_160MHZ		0x02
#define	IEEE80211_EHT_PHY_CAP7_MU_BEAMFORMER_320MHZ		0x03
#define	IEEE80211_EHT_PHY_CAP7_NON_OFDMA_UL_MU_MIMO_80MHZ	0x04
#define	IEEE80211_EHT_PHY_CAP7_NON_OFDMA_UL_MU_MIMO_160MHZ	0x05
#define	IEEE80211_EHT_PHY_CAP7_NON_OFDMA_UL_MU_MIMO_320MHZ	0x06

#define	IEEE80211_EHT_PHY_CAP8_RX_1024QAM_WIDER_BW_DL_OFDMA	0x01
#define	IEEE80211_EHT_PHY_CAP8_RX_4096QAM_WIDER_BW_DL_OFDMA	0x02

#define	IEEE80211_EHT_PPE_THRES_INFO_HEADER_SIZE		0x01
#define	IEEE80211_EHT_PPE_THRES_NSS_MASK			0x02
#define	IEEE80211_EHT_PPE_THRES_RU_INDEX_BITMASK_MASK		0x03
#define	IEEE80211_EHT_PPE_THRES_INFO_PPET_SIZE			0x04

#define	IEEE80211_EML_CAP_EMLSR_SUPP				0x01
#define	IEEE80211_EML_CAP_TRANSITION_TIMEOUT			0x02
#define	IEEE80211_EML_CAP_TRANSITION_TIMEOUT_128TU		0x04
#define	IEEE80211_EML_CAP_EMLSR_PADDING_DELAY			0x08
#define	IEEE80211_EML_CAP_EMLSR_PADDING_DELAY_32US		0x10
#define	IEEE80211_EML_CAP_EMLSR_PADDING_DELAY_256US		0x10
#define	IEEE80211_EML_CAP_EMLSR_TRANSITION_DELAY		0x20
#define	IEEE80211_EML_CAP_EMLSR_TRANSITION_DELAY_64US		0x40
#define	IEEE80211_EML_CAP_EMLSR_TRANSITION_DELAY_256US		0x40

#define	VENDOR_CMD_RAW_DATA	(void *)(uintptr_t)(-ENOENT)

/* net80211::net80211_he_cap */
struct ieee80211_sta_he_cap {
	bool					has_he;
	struct ieee80211_he_cap_elem		he_cap_elem;
	struct ieee80211_he_mcs_nss_supp	he_mcs_nss_supp;
	uint8_t					ppe_thres[IEEE80211_HE_CAP_PPE_THRES_MAX];
};

struct cfg80211_he_bss_color {
	int	color, enabled;
};

struct ieee80211_he_obss_pd {
	bool					enable;
	uint8_t					min_offset;
	uint8_t					max_offset;
	uint8_t					non_srg_max_offset;
	uint8_t					sr_ctrl;
	uint8_t					bss_color_bitmap[8];
	uint8_t					partial_bssid_bitmap[8];
};

struct ieee80211_eht_mcs_nss_supp_20mhz_only {
	union {
		struct {
			uint8_t				rx_tx_mcs7_max_nss;
			uint8_t				rx_tx_mcs9_max_nss;
			uint8_t				rx_tx_mcs11_max_nss;
			uint8_t				rx_tx_mcs13_max_nss;
		};
		uint8_t					rx_tx_max_nss[4];
	};
};

struct ieee80211_eht_mcs_nss_supp_bw {
	union {
		struct {
			uint8_t				rx_tx_mcs9_max_nss;
			uint8_t				rx_tx_mcs11_max_nss;
			uint8_t				rx_tx_mcs13_max_nss;
		};
		uint8_t					rx_tx_max_nss[3];
	};
};

struct ieee80211_eht_cap_elem_fixed {
	uint8_t					mac_cap_info[2];
	uint8_t					phy_cap_info[9];
};

struct ieee80211_eht_mcs_nss_supp {
	/* TODO FIXME */
	/* Can only have either or... */
	union {
		struct ieee80211_eht_mcs_nss_supp_20mhz_only		only_20mhz;
		struct {
			struct ieee80211_eht_mcs_nss_supp_bw		_80;
			struct ieee80211_eht_mcs_nss_supp_bw		_160;
			struct ieee80211_eht_mcs_nss_supp_bw		_320;
		} bw;
	};
};

#define	IEEE80211_STA_EHT_PPE_THRES_MAX		32
struct ieee80211_sta_eht_cap {
	bool					has_eht;
	struct ieee80211_eht_cap_elem_fixed	eht_cap_elem;
	struct ieee80211_eht_mcs_nss_supp	eht_mcs_nss_supp;
	uint8_t					eht_ppe_thres[IEEE80211_STA_EHT_PPE_THRES_MAX];
};

struct ieee80211_sband_iftype_data {
	/* TODO FIXME */
	enum nl80211_iftype			types_mask;
	struct ieee80211_sta_he_cap		he_cap;
	struct ieee80211_he_6ghz_capa		he_6ghz_capa;
	struct ieee80211_sta_eht_cap		eht_cap;
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
	WIPHY_WOWLAN_MAGIC_PKT,
	WIPHY_WOWLAN_SUPPORTS_GTK_REKEY,
	WIPHY_WOWLAN_GTK_REKEY_FAILURE,
	WIPHY_WOWLAN_EAP_IDENTITY_REQ,
	WIPHY_WOWLAN_4WAY_HANDSHAKE,
	WIPHY_WOWLAN_RFKILL_RELEASE,
	WIPHY_WOWLAN_NET_DETECT,
};

struct wiphy_wowlan_support {
	/* XXX TODO */
	enum wiphy_wowlan_support_flags		flags;
	int	max_nd_match_sets, max_pkt_offset, n_patterns, pattern_max_len, pattern_min_len;
};

struct cfg80211_wowlan_wakeup {
	/* XXX TODO */
	uint16_t				pattern_idx;
	bool					disconnect;
	bool					unprot_deauth_disassoc;
	bool					eap_identity_req;
	bool					four_way_handshake;
	bool					gtk_rekey_failure;
	bool					magic_pkt;
	bool					rfkill_release;
	bool					tcp_connlost;
	bool					tcp_nomoretokens;
	bool					tcp_match;
	bool					packet_80211;
	struct cfg80211_wowlan_nd_info		*net_detect;
	uint8_t					*packet;
	uint16_t				packet_len;
	uint16_t				packet_present_len;
};

struct cfg80211_wowlan {
	/* XXX TODO */
	bool					any;
	bool					disconnect;
	bool					magic_pkt;
	bool					gtk_rekey_failure;
	bool					eap_identity_req;
	bool					four_way_handshake;
	bool					rfkill_release;

	/* Magic packet patterns. */
	int					n_patterns;
	struct cfg80211_pkt_pattern		*patterns;

	/* netdetect? if not assoc? */
	struct cfg80211_sched_scan_request	*nd_config;

	void					*tcp;		/* XXX ? */
};

struct cfg80211_gtk_rekey_data {
	/* XXX TODO */
	const uint8_t				*kck, *kek, *replay_ctr;
	uint32_t				akm;
	uint8_t					kck_len, kek_len;
};

struct cfg80211_tid_cfg {
	/* XXX TODO */
	int	mask, noack, retry_long, rtscts, tids, amsdu, ampdu;
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
	enum environment_cap			country_ie_env;
	int	initiator, dfs_region;
	int	user_reg_hint_type;
};

struct cfg80211_set_hw_timestamp {
	const uint8_t				*macaddr;
	bool					enable;
};

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
	struct linuxkpi_ieee80211_channel *channel;
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
	uint16_t				eml_capabilities;
	uint16_t				mld_capa_and_ops;
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

struct wiphy_radio_freq_range {
	uint32_t				start_freq;
	uint32_t				end_freq;
};

struct wiphy_radio {
	int					n_freq_range;
	int					n_iface_combinations;
	const struct wiphy_radio_freq_range	*freq_range;
	const struct ieee80211_iface_combination *iface_combinations;
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
	WIPHY_FLAG_SUPPORTS_MLO			= BIT(16),
	WIPHY_FLAG_DISABLE_WEXT			= BIT(17),
};

struct wiphy_work;
typedef void (*wiphy_work_fn)(struct wiphy *, struct wiphy_work *);
struct wiphy_work {
	struct list_head			entry;
	wiphy_work_fn				fn;
};
struct wiphy_delayed_work {
	struct wiphy_work			work;
	struct wiphy				*wiphy;
	struct timer_list			timer;
};

struct wiphy {
	struct mutex				mtx;
	struct device				*dev;
	struct mac_address			*addresses;
	int					n_addresses;
	uint32_t				flags;
	struct ieee80211_supported_band		*bands[NUM_NL80211_BANDS];
	uint8_t					perm_addr[ETH_ALEN];
	uint16_t				max_scan_ie_len;

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
	uint8_t					available_antennas_rx;
	uint8_t					available_antennas_tx;

	int					n_radio;
	const struct wiphy_radio		*radio;

	int	features, hw_version;
	int	interface_modes, max_match_sets, max_remain_on_channel_duration, max_scan_ssids, max_sched_scan_ie_len, max_sched_scan_plan_interval, max_sched_scan_plan_iterations, max_sched_scan_plans, max_sched_scan_reqs, max_sched_scan_ssids;
	int	num_iftype_ext_capab;
	int	max_ap_assoc_sta, probe_resp_offload, software_iftypes;
	int     bss_select_support, max_num_pmkids, retry_long, retry_short, signal_type;
	int	max_data_retry_count;
	int     tx_queue_len, rfkill;
	int	mbssid_max_interfaces;
	int	hw_timestamp_max_peers;
	int	ema_max_profile_periodicity;

	unsigned long				ext_features[BITS_TO_LONGS(NUM_NL80211_EXT_FEATURES)];
	struct dentry				*debugfsdir;

	const struct wiphy_wowlan_support	*wowlan;
	struct cfg80211_wowlan			*wowlan_config;
	/* Lower layer (driver/mac80211) specific data. */
	/* Must stay last. */
	uint8_t					priv[0] __aligned(CACHE_LINE_SIZE);
};

#define	lockdep_assert_wiphy(wiphy)					\
    lockdep_assert_held(&(wiphy)->mtx)

struct wireless_dev {
		/* XXX TODO, like ic? */
	enum nl80211_iftype			iftype;
	uint32_t				radio_mask;
	uint8_t					address[ETH_ALEN];
	struct net_device			*netdev;
	struct wiphy				*wiphy;
};

struct cfg80211_ops {
	/* XXX TODO */
	struct wireless_dev *(*add_virtual_intf)(struct wiphy *, const char *, unsigned char, enum nl80211_iftype, struct vif_params *);
	int (*del_virtual_intf)(struct wiphy *, struct wireless_dev *);
	int (*change_virtual_intf)(struct wiphy *, struct net_device *, enum nl80211_iftype, struct vif_params *);
	int (*scan)(struct wiphy *, struct cfg80211_scan_request *);
	int (*set_wiphy_params)(struct wiphy *, int, uint32_t);
	int (*join_ibss)(struct wiphy *, struct net_device *, struct cfg80211_ibss_params *);
	int (*leave_ibss)(struct wiphy *, struct net_device *);
	int (*get_station)(struct wiphy *, struct net_device *, const uint8_t *, struct station_info *);
	int (*dump_station)(struct wiphy *, struct net_device *, int, uint8_t *, struct station_info *);
	int (*set_tx_power)(struct wiphy *, struct wireless_dev *, int, enum nl80211_tx_power_setting, int);
	int (*get_tx_power)(struct wiphy *, struct wireless_dev *, int, unsigned int, int *);
	int (*add_key)(struct wiphy *, struct net_device *, int, uint8_t, bool, const uint8_t *, struct key_params *);
	int (*del_key)(struct wiphy *, struct net_device *, int, uint8_t, bool, const uint8_t *);
	int (*get_key)(struct wiphy *, struct net_device *, int, uint8_t, bool, const uint8_t *, void *, void(*)(void *, struct key_params *));
	int (*set_default_key)(struct wiphy *, struct net_device *, int, uint8_t, bool, bool);
	int (*set_default_mgmt_key)(struct wiphy *, struct net_device *, int, uint8_t);
	int (*set_power_mgmt)(struct wiphy *, struct net_device *, bool, int);
	int (*connect)(struct wiphy *, struct net_device *, struct cfg80211_connect_params *);
	int (*disconnect)(struct wiphy *, struct net_device *, uint16_t);
	int (*suspend)(struct wiphy *, struct cfg80211_wowlan *);
	int (*resume)(struct wiphy *);
	int (*set_pmksa)(struct wiphy *, struct net_device *, struct cfg80211_pmksa *);
	int (*del_pmksa)(struct wiphy *, struct net_device *, struct cfg80211_pmksa *);
	int (*flush_pmksa)(struct wiphy *, struct net_device *);
	int (*start_ap)(struct wiphy *, struct net_device *, struct cfg80211_ap_settings *);
	int (*stop_ap)(struct wiphy *, struct net_device *, unsigned int);
	int (*change_beacon)(struct wiphy *, struct net_device *, struct cfg80211_ap_update *);
	int (*del_station)(struct wiphy *, struct net_device *, struct station_del_parameters *);
	int (*change_station)(struct wiphy *, struct net_device *, const uint8_t *, struct station_parameters *);
	int (*sched_scan_start)(struct wiphy *, struct net_device *, struct cfg80211_sched_scan_request *);
	int (*sched_scan_stop)(struct wiphy *, struct net_device *, uint64_t);
	void (*update_mgmt_frame_registrations)(struct wiphy *, struct wireless_dev *, struct mgmt_frame_regs *);
	int (*mgmt_tx)(struct wiphy *, struct wireless_dev *, struct cfg80211_mgmt_tx_params *, uint64_t *);
	int (*cancel_remain_on_channel)(struct wiphy *, struct wireless_dev *, uint64_t);
	int (*get_channel)(struct wiphy *, struct wireless_dev *, unsigned int, struct cfg80211_chan_def *);
	int (*crit_proto_start)(struct wiphy *, struct wireless_dev *, enum nl80211_crit_proto_id, uint16_t);
	void (*crit_proto_stop)(struct wiphy *, struct wireless_dev *);
	int (*tdls_oper)(struct wiphy *, struct net_device *, const uint8_t *, enum nl80211_tdls_operation);
	int (*update_connect_params)(struct wiphy *, struct net_device *, struct cfg80211_connect_params *, uint32_t);
	int (*set_pmk)(struct wiphy *, struct net_device *, const struct cfg80211_pmk_conf *);
	int (*del_pmk)(struct wiphy *, struct net_device *, const uint8_t *);
	int (*remain_on_channel)(struct wiphy *, struct wireless_dev *, struct linuxkpi_ieee80211_channel *, unsigned int, uint64_t *);
	int (*start_p2p_device)(struct wiphy *, struct wireless_dev *);
	void (*stop_p2p_device)(struct wiphy *, struct wireless_dev *);
	int (*dump_survey)(struct wiphy *, struct net_device *, int, struct survey_info *);
	int (*external_auth)(struct wiphy *, struct net_device *, struct cfg80211_external_auth_params *);
        int (*set_cqm_rssi_range_config)(struct wiphy *, struct net_device *, int, int);

};


/* -------------------------------------------------------------------------- */

/* linux_80211.c */

struct wiphy *linuxkpi_wiphy_new(const struct cfg80211_ops *, size_t);
void linuxkpi_wiphy_free(struct wiphy *wiphy);

void linuxkpi_wiphy_work_queue(struct wiphy *, struct wiphy_work *);
void linuxkpi_wiphy_work_cancel(struct wiphy *, struct wiphy_work *);
void linuxkpi_wiphy_work_flush(struct wiphy *, struct wiphy_work *);
void lkpi_wiphy_delayed_work_timer(struct timer_list *);
void linuxkpi_wiphy_delayed_work_queue(struct wiphy *,
    struct wiphy_delayed_work *, unsigned long);
void linuxkpi_wiphy_delayed_work_cancel(struct wiphy *,
    struct wiphy_delayed_work *);
void linuxkpi_wiphy_delayed_work_flush(struct wiphy *,
    struct wiphy_delayed_work *);

int linuxkpi_regulatory_set_wiphy_regd_sync(struct wiphy *wiphy,
    struct linuxkpi_ieee80211_regdomain *regd);
uint32_t linuxkpi_cfg80211_calculate_bitrate(struct rate_info *);
uint32_t linuxkpi_ieee80211_channel_to_frequency(uint32_t, enum nl80211_band);
uint32_t linuxkpi_ieee80211_frequency_to_channel(uint32_t, uint32_t);
struct linuxkpi_ieee80211_channel *
    linuxkpi_ieee80211_get_channel(struct wiphy *, uint32_t);
struct cfg80211_bss *linuxkpi_cfg80211_get_bss(struct wiphy *,
    struct linuxkpi_ieee80211_channel *, const uint8_t *,
    const uint8_t *, size_t, enum ieee80211_bss_type, enum ieee80211_privacy);
void linuxkpi_cfg80211_put_bss(struct wiphy *, struct cfg80211_bss *);
void linuxkpi_cfg80211_bss_flush(struct wiphy *);
struct linuxkpi_ieee80211_regdomain *
    lkpi_get_linuxkpi_ieee80211_regdomain(size_t);

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

#define	wiphy_dereference(_w, p)					\
    rcu_dereference_check(p, lockdep_is_held(&(_w)->mtx))

#define	wiphy_lock(_w)		mutex_lock(&(_w)->mtx)
#define	wiphy_unlock(_w)	mutex_unlock(&(_w)->mtx)

static __inline void
wiphy_rfkill_set_hw_state_reason(struct wiphy *wiphy, bool blocked,
    enum rfkill_hard_block_reasons reason)
{
	TODO();
}

/* -------------------------------------------------------------------------- */

static inline int
cfg80211_register_netdevice(struct net_device *ndev)
{
	TODO();
	return (-ENXIO);
}

static inline void
cfg80211_unregister_netdevice(struct net_device *ndev)
{
	TODO();
}

/* -------------------------------------------------------------------------- */

static inline struct cfg80211_bss *
cfg80211_get_bss(struct wiphy *wiphy, struct linuxkpi_ieee80211_channel *chan,
    const uint8_t *bssid, const uint8_t *ssid, size_t ssid_len,
    enum ieee80211_bss_type bss_type, enum ieee80211_privacy privacy)
{

	return (linuxkpi_cfg80211_get_bss(wiphy, chan, bssid, ssid, ssid_len,
	    bss_type, privacy));
}

static inline void
cfg80211_put_bss(struct wiphy *wiphy, struct cfg80211_bss *bss)
{

	linuxkpi_cfg80211_put_bss(wiphy, bss);
}

static inline void
cfg80211_bss_flush(struct wiphy *wiphy)
{

	linuxkpi_cfg80211_bss_flush(wiphy);
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

static __inline void
wiphy_rfkill_start_polling(struct wiphy *wiphy)
{
	TODO();
}

static __inline void
wiphy_rfkill_stop_polling(struct wiphy *wiphy)
{
	TODO();
}

static __inline int
reg_query_regdb_wmm(uint8_t *alpha2, uint32_t center_freq,
    struct ieee80211_reg_rule *rule)
{

	IMPROVE("regdomain.xml needs to grow wmm information for at least ETSI");

	return (-ENODATA);
}

static __inline const uint8_t *
cfg80211_find_ie_match(uint32_t f, const uint8_t *ies, size_t ies_len,
    const uint8_t *match, int x, int y)
{
	TODO();
	return (NULL);
}

static __inline const uint8_t *
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

static inline int
nl80211_chan_width_to_mhz(enum nl80211_chan_width width)
{
	switch (width) {
	case NL80211_CHAN_WIDTH_5:
		return (5);
		break;
	case NL80211_CHAN_WIDTH_10:
		return (10);
		break;
	case NL80211_CHAN_WIDTH_20_NOHT:
	case NL80211_CHAN_WIDTH_20:
		return (20);
		break;
	case NL80211_CHAN_WIDTH_40:
		return (40);
		break;
	case NL80211_CHAN_WIDTH_80:
	case NL80211_CHAN_WIDTH_80P80:
		return (80);
		break;
	case NL80211_CHAN_WIDTH_160:
		return (160);
		break;
	case NL80211_CHAN_WIDTH_320:
		return (320);
		break;
	}
}

static inline void
cfg80211_chandef_create(struct cfg80211_chan_def *chandef,
    struct linuxkpi_ieee80211_channel *chan, enum nl80211_channel_type chan_type)
{

	KASSERT(chandef != NULL, ("%s: chandef is NULL\n", __func__));
	KASSERT(chan != NULL, ("%s: chan is NULL\n", __func__));

	/* memset(chandef, 0, sizeof(*chandef)); */
	chandef->chan = chan;
	chandef->center_freq1 = chan->center_freq;
	/* chandef->width, center_freq2, punctured */

	switch (chan_type) {
	case NL80211_CHAN_NO_HT:
		chandef->width = NL80211_CHAN_WIDTH_20_NOHT;
		break;
	case NL80211_CHAN_HT20:
		chandef->width = NL80211_CHAN_WIDTH_20;
		break;
	case NL80211_CHAN_HT40MINUS:
		chandef->width = NL80211_CHAN_WIDTH_40;
		chandef->center_freq1 -= 10;
		break;
	case NL80211_CHAN_HT40PLUS:
		chandef->width = NL80211_CHAN_WIDTH_40;
		chandef->center_freq1 += 10;
		break;
	};
}

static __inline bool
cfg80211_chandef_valid(const struct cfg80211_chan_def *chandef)
{
	TODO();
	return (false);
}

static inline int
cfg80211_chandef_get_width(const struct cfg80211_chan_def *chandef)
{
	return (nl80211_chan_width_to_mhz(chandef->width));
}

static __inline bool
cfg80211_chandef_dfs_usable(struct wiphy *wiphy, const struct cfg80211_chan_def *chandef)
{
	TODO();
	return (false);
}

static __inline unsigned int
cfg80211_chandef_dfs_cac_time(struct wiphy *wiphy, const struct cfg80211_chan_def *chandef)
{
	TODO();
	return (0);
}

static __inline bool
cfg80211_chandef_identical(const struct cfg80211_chan_def *chandef_1,
    const struct cfg80211_chan_def *chandef_2)
{
	TODO();
	return (false);
}

static __inline bool
cfg80211_chandef_usable(struct wiphy *wiphy,
    const struct cfg80211_chan_def *chandef, uint32_t flags)
{
	TODO();
	return (false);
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

static inline const struct element *
lkpi_cfg80211_find_elem_pattern(enum ieee80211_eid eid,
    const uint8_t *data, size_t len, uint8_t *pattern, size_t plen)
{
	const struct element *elem;
	const uint8_t *p;
	size_t ielen;

	p = data;
	elem = (const struct element *)p;
	ielen = len;
	while (elem != NULL && ielen > 1) {
		if ((2 + elem->datalen) > ielen)
			/* Element overruns our memory. */
			return (NULL);
		if (elem->id == eid) {
			if (pattern == NULL)
				return (elem);
			if (elem->datalen >= plen &&
			    memcmp(elem->data, pattern, plen) == 0)
				return (elem);
		}
		ielen -= 2 + elem->datalen;
		p += 2 + elem->datalen;
		elem = (const struct element *)p;
	}

	return (NULL);
}

static inline const struct element *
cfg80211_find_elem(enum ieee80211_eid eid, const uint8_t *data, size_t len)
{

	return (lkpi_cfg80211_find_elem_pattern(eid, data, len, NULL, 0));
}

static inline const struct element *
ieee80211_bss_get_elem(struct cfg80211_bss *bss, uint32_t eid)
{

	if (bss->ies == NULL)
		return (NULL);
	return (cfg80211_find_elem(eid, bss->ies->data, bss->ies->len));
}

static inline const uint8_t *
ieee80211_bss_get_ie(struct cfg80211_bss *bss, uint32_t eid)
{

	return ((const uint8_t *)ieee80211_bss_get_elem(bss, eid));
}

static inline uint8_t *
cfg80211_find_vendor_ie(unsigned int oui, int oui_type,
    uint8_t *data, size_t len)
{
	const struct element *elem;
	uint8_t pattern[4] = { oui << 16, oui << 8, oui, oui_type };
	uint8_t plen = 4;		/* >= 3? oui_type always part of this? */
	IMPROVE("plen currently always incl. oui_type");

	elem = lkpi_cfg80211_find_elem_pattern(IEEE80211_ELEMID_VENDOR,
	    data, len, pattern, plen);
	if (elem == NULL)
		return (NULL);
	return (__DECONST(uint8_t *, elem));
}

static inline uint32_t
cfg80211_calculate_bitrate(struct rate_info *rate)
{
	return (linuxkpi_cfg80211_calculate_bitrate(rate));
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
	struct linuxkpi_ieee80211_regdomain *regd;

	if (wiphy->regd != NULL)
		return (-EBUSY);

	regd = lkpi_get_linuxkpi_ieee80211_regdomain(0);
	if (regd == NULL)
		return (-ENOMEM);

	regd->alpha2[0] = alpha2[0];
	regd->alpha2[1] = alpha2[1];
	wiphy->regd = regd;

	IMPROVE("are there flags who is managing? update net8011?");

	return (0);
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

static __inline void
wiphy_ext_feature_set(struct wiphy *wiphy, enum nl80211_ext_feature ef)
{

	set_bit(ef, wiphy->ext_features);
}

static inline bool
wiphy_ext_feature_isset(struct wiphy *wiphy, enum nl80211_ext_feature ef)
{
	return (test_bit(ef, wiphy->ext_features));
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
cfg80211_get_p2p_attr(const uint8_t *ie, uint32_t ie_len,
    enum ieee80211_p2p_attr_ids attr, uint8_t *p, size_t p_len)
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
    const uint8_t *buf, size_t len, bool ack, gfp_t gfp)
{
	TODO();
}

static __inline void
cfg80211_michael_mic_failure(struct net_device *ndev, const uint8_t addr[ETH_ALEN],
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
cfg80211_port_authorized(struct net_device *ndev, const uint8_t *addr,
    const uint8_t *bitmap, uint8_t len, gfp_t gfp)
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

static inline size_t
ieee80211_get_hdrlen_from_skb(struct sk_buff *skb)
{
	const struct ieee80211_hdr *hdr;
	size_t len;

	if (skb->len < 10)	/* sizeof(ieee80211_frame_[ack,cts]) */
		return (0);

	hdr = (const struct ieee80211_hdr *)skb->data;
	len = ieee80211_hdrlen(hdr->frame_control);

	/* If larger than what is in the skb return. */
	if (len > skb->len)
		return (0);

	return (len);
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

static inline int
cfg80211_get_ies_channel_number(const uint8_t *ie, size_t len,
    enum nl80211_band band)
{
	const struct element *elem;

	switch (band) {
	case NL80211_BAND_6GHZ:
		TODO();
		break;
	case NL80211_BAND_5GHZ:
	case NL80211_BAND_2GHZ:
		/* DSPARAMS has the channel number. */
		elem = cfg80211_find_elem(IEEE80211_ELEMID_DSPARMS, ie, len);
		if (elem != NULL && elem->datalen == 1)
			return (elem->data[0]);
		/* HTINFO has the primary center channel. */
		elem = cfg80211_find_elem(IEEE80211_ELEMID_HTINFO, ie, len);
		if (elem != NULL &&
		    elem->datalen >= (sizeof(struct ieee80211_ie_htinfo) - 2)) {
			const struct ieee80211_ie_htinfo *htinfo;
			htinfo = (const struct ieee80211_ie_htinfo *)elem;
			return (htinfo->hi_ctrlchannel);
		}
		/* What else? */
		break;
	default:
		IMPROVE("Unsupported");
		break;
	}
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

static __inline const uint8_t *
cfg80211_find_ext_ie(uint8_t eid, const uint8_t *p, size_t len)
{
	TODO();
	return (NULL);
}

static inline void
_ieee80211_set_sband_iftype_data(struct ieee80211_supported_band *band,
    struct ieee80211_sband_iftype_data *iftype_data, size_t nitems)
{
	band->iftype_data = iftype_data;
	band->n_iftype_data = nitems;
}

static inline const struct ieee80211_sband_iftype_data *
ieee80211_get_sband_iftype_data(const struct ieee80211_supported_band *band,
    enum nl80211_iftype iftype)
{
	const struct ieee80211_sband_iftype_data *iftype_data;
	int i;

	for (i = 0; i < band->n_iftype_data; i++) {
		iftype_data = (const void *)&band->iftype_data[i];
		if (iftype_data->types_mask & BIT(iftype))
			return (iftype_data);
	}

	return (NULL);
}

static inline const struct ieee80211_sta_he_cap *
ieee80211_get_he_iftype_cap(const struct ieee80211_supported_band *band,
    enum nl80211_iftype iftype)
{
	const struct ieee80211_sband_iftype_data *iftype_data;
	const struct ieee80211_sta_he_cap *he_cap;

	iftype_data = ieee80211_get_sband_iftype_data(band, iftype);
	if (iftype_data == NULL)
		return (NULL);

	he_cap = NULL;
	if (iftype_data->he_cap.has_he)
		he_cap = &iftype_data->he_cap;

	return (he_cap);
}

static inline const struct ieee80211_sta_eht_cap *
ieee80211_get_eht_iftype_cap(const struct ieee80211_supported_band *band,
    enum nl80211_iftype iftype)
{
	const struct ieee80211_sband_iftype_data *iftype_data;
	const struct ieee80211_sta_eht_cap *eht_cap;

	iftype_data = ieee80211_get_sband_iftype_data(band, iftype);
	if (iftype_data == NULL)
		return (NULL);

	eht_cap = NULL;
	if (iftype_data->eht_cap.has_eht)
		eht_cap = &iftype_data->eht_cap;

	return (eht_cap);
}

static inline bool
cfg80211_ssid_eq(struct cfg80211_ssid *ssid1, struct cfg80211_ssid *ssid2)
{
	int error;

	if (ssid1 == NULL || ssid2 == NULL)	/* Can we KASSERT this? */
		return (false);

	if (ssid1->ssid_len != ssid2->ssid_len)
		return (false);
	error = memcmp(ssid1->ssid, ssid2->ssid, ssid2->ssid_len);
	if (error != 0)
		return (false);
	return (true);
}

static inline void
cfg80211_rx_unprot_mlme_mgmt(struct net_device *ndev, const uint8_t *hdr,
    uint32_t len)
{
	TODO();
}

static inline const struct wiphy_iftype_ext_capab *
cfg80211_get_iftype_ext_capa(struct wiphy *wiphy, enum nl80211_iftype iftype)
{

	TODO();
	return (NULL);
}

static inline int
cfg80211_external_auth_request(struct net_device *ndev,
    struct cfg80211_external_auth_params *params, gfp_t gfp)
{
	TODO();
	return (-ENXIO);
}

static inline uint16_t
ieee80211_get_he_6ghz_capa(const struct ieee80211_supported_band *sband,
    enum nl80211_iftype iftype)
{
	TODO();
	return (0);
}

static __inline ssize_t
wiphy_locked_debugfs_read(struct wiphy *wiphy, struct file *file,
    char *buf, size_t bufsize, const char __user *userbuf, size_t count,
    loff_t *ppos,
    ssize_t (*handler)(struct wiphy *, struct file *, char *, size_t, void *),
    void *data)
{
	TODO();
	return (-ENXIO);
}


static __inline ssize_t
wiphy_locked_debugfs_write(struct wiphy *wiphy, struct file *file,
    char *buf, size_t bufsize, const char __user *userbuf, size_t count,
    ssize_t (*handler)(struct wiphy *, struct file *, char *, size_t, void *),
    void *data)
{
	TODO();
	return (-ENXIO);
}

static inline void
cfg80211_cqm_rssi_notify(struct net_device *dev,
    enum nl80211_cqm_rssi_threshold_event rssi_te, int32_t rssi, gfp_t gfp)
{
	TODO();
}

/* -------------------------------------------------------------------------- */

static inline void
wiphy_work_init(struct wiphy_work *wwk, wiphy_work_fn fn)
{
	INIT_LIST_HEAD(&wwk->entry);
	wwk->fn = fn;
}

static inline void
wiphy_work_queue(struct wiphy *wiphy, struct wiphy_work *wwk)
{
	linuxkpi_wiphy_work_queue(wiphy, wwk);
}

static inline void
wiphy_work_cancel(struct wiphy *wiphy, struct wiphy_work *wwk)
{
	linuxkpi_wiphy_work_cancel(wiphy, wwk);
}

static inline void
wiphy_work_flush(struct wiphy *wiphy, struct wiphy_work *wwk)
{
	linuxkpi_wiphy_work_flush(wiphy, wwk);
}

static inline void
wiphy_delayed_work_init(struct wiphy_delayed_work *wdwk, wiphy_work_fn fn)
{
	wiphy_work_init(&wdwk->work, fn);
	timer_setup(&wdwk->timer, lkpi_wiphy_delayed_work_timer, 0);
}

static inline void
wiphy_delayed_work_queue(struct wiphy *wiphy, struct wiphy_delayed_work *wdwk,
    unsigned long delay)
{
	linuxkpi_wiphy_delayed_work_queue(wiphy, wdwk, delay);
}

static inline void
wiphy_delayed_work_cancel(struct wiphy *wiphy, struct wiphy_delayed_work *wdwk)
{
	linuxkpi_wiphy_delayed_work_cancel(wiphy, wdwk);
}

static inline void
wiphy_delayed_work_flush(struct wiphy *wiphy, struct wiphy_delayed_work *wdwk)
{
	linuxkpi_wiphy_delayed_work_flush(wiphy, wdwk);
}

/* -------------------------------------------------------------------------- */

#define	wiphy_err(_wiphy, _fmt, ...)					\
    dev_err((_wiphy)->dev, _fmt, __VA_ARGS__)
#define	wiphy_info(wiphy, fmt, ...)					\
    dev_info((wiphy)->dev, fmt, ##__VA_ARGS__)
#define	wiphy_info_once(wiphy, fmt, ...)				\
    dev_info_once((wiphy)->dev, fmt, ##__VA_ARGS__)

#ifndef LINUXKPI_NET80211
#define	ieee80211_channel		linuxkpi_ieee80211_channel
#define	ieee80211_regdomain		linuxkpi_ieee80211_regdomain
#endif

#include <net/mac80211.h>

#endif	/* _LINUXKPI_NET_CFG80211_H */
