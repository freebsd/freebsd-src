/*
 * Copyright (C) 2000
 * Dr. Duncan McLennan Barclay, dmlb@ragnet.demon.co.uk.
 *
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DUNCAN BARCLAY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DUNCAN BARCLAY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

/*
 * Bit mask definitions for firmware versioning
 */
#define RAY_V4	0x1
#define RAY_V5	0x2

/*
 * MIB stuctures
 */
struct ray_mib_common_head {			/*Offset*/	/*Size*/
    u_int8_t	mib_net_type;			/*00*/ 
    u_int8_t	mib_ap_status;			/*01*/
    u_int8_t	mib_ssid[IEEE80211_NWID_LEN];	/*02*/		/*20*/
    u_int8_t	mib_scan_mode;			/*22*/
    u_int8_t	mib_apm_mode;			/*23*/
    u_int8_t	mib_mac_addr[ETHER_ADDR_LEN];	/*24*/		/*06*/
    u_int8_t	mib_frag_thresh[2];		/*2a*/		/*02*/
    u_int8_t	mib_dwell_time[2];		/*2c*/		/*02*/
    u_int8_t	mib_beacon_period[2];		/*2e*/		/*02*/
    u_int8_t	mib_dtim_interval;		/*30*/
    u_int8_t	mib_max_retry;			/*31*/
    u_int8_t	mib_ack_timo;			/*32*/
    u_int8_t	mib_sifs;			/*33*/
    u_int8_t	mib_difs;			/*34*/
    u_int8_t	mib_pifs;			/*35*/
    u_int8_t	mib_rts_thresh[2];		/*36*/		/*02*/
    u_int8_t	mib_scan_dwell[2];		/*38*/		/*02*/
    u_int8_t	mib_scan_max_dwell[2];		/*3a*/		/*02*/
    u_int8_t	mib_assoc_timo;			/*3c*/
    u_int8_t	mib_adhoc_scan_cycle;		/*3d*/
    u_int8_t	mib_infra_scan_cycle;		/*3e*/
    u_int8_t	mib_infra_super_scan_cycle;	/*3f*/
    u_int8_t	mib_promisc;			/*40*/
    u_int8_t	mib_uniq_word[2];		/*41*/		/*02*/
    u_int8_t	mib_slot_time;			/*43*/
    u_int8_t	mib_roam_low_snr_thresh;	/*44*/
    u_int8_t	mib_low_snr_count;		/*45*/
    u_int8_t	mib_infra_missed_beacon_count;	/*46*/
    u_int8_t	mib_adhoc_missed_beacon_count;	/*47*/
    u_int8_t	mib_country_code;		/*48*/
    u_int8_t	mib_hop_seq;			/*49*/
    u_int8_t	mib_hop_seq_len;		/*4a*/
} __packed;

struct ray_mib_common_tail {
    u_int8_t	mib_noise_filter_gain;		/*00*/
    u_int8_t	mib_noise_limit_offset;		/*01*/
    u_int8_t	mib_rssi_thresh_offset;		/*02*/
    u_int8_t	mib_busy_thresh_offset;		/*03*/
    u_int8_t	mib_sync_thresh;		/*04*/
    u_int8_t	mib_test_mode;			/*05*/
    u_int8_t	mib_test_min_chan;		/*06*/
    u_int8_t	mib_test_max_chan;		/*07*/
} __packed;

struct ray_mib_4 {
    struct ray_mib_common_head	mib_head;	/*00*/
    u_int8_t			mib_cw_max;	/*4b*/
    u_int8_t			mib_cw_min;	/*4c*/
    struct ray_mib_common_tail	mib_tail;	/*4d*/
} __packed;

struct ray_mib_5 {
    struct ray_mib_common_head	mib_head;		/*00*/
    u_int8_t			mib_cw_max[2];		/*4b*/		/*02*/
    u_int8_t			mib_cw_min[2];		/*4d*/		/*02*/
    struct ray_mib_common_tail	mib_tail;		/*4f*/
    u_int8_t			mib_allow_probe_resp;	/*57*/
    u_int8_t			mib_privacy_must_start;	/*58*/
    u_int8_t			mib_privacy_can_join;	/*59*/
    u_int8_t			mib_basic_rate_set[8];	/*5a*/		/*08*/
} __packed;

#define mib_net_type		mib_head.mib_net_type
#define mib_ap_status		mib_head.mib_ap_status
#define mib_ssid		mib_head.mib_ssid
#define mib_scan_mode		mib_head.mib_scan_mode
#define mib_apm_mode		mib_head.mib_apm_mode
#define mib_mac_addr		mib_head.mib_mac_addr
#define mib_frag_thresh		mib_head.mib_frag_thresh
#define mib_dwell_time		mib_head.mib_dwell_time
#define mib_beacon_period	mib_head.mib_beacon_period
#define mib_dtim_interval	mib_head.mib_dtim_interval
#define mib_max_retry		mib_head.mib_max_retry
#define mib_ack_timo		mib_head.mib_ack_timo
#define mib_sifs		mib_head.mib_sifs
#define mib_difs		mib_head.mib_difs
#define mib_pifs		mib_head.mib_pifs
#define mib_rts_thresh		mib_head.mib_rts_thresh
#define mib_scan_dwell		mib_head.mib_scan_dwell
#define mib_scan_max_dwell	mib_head.mib_scan_max_dwell
#define mib_assoc_timo		mib_head.mib_assoc_timo
#define mib_adhoc_scan_cycle	mib_head.mib_adhoc_scan_cycle
#define mib_infra_scan_cycle	mib_head.mib_infra_scan_cycle
#define mib_infra_super_scan_cycle \
				mib_head.mib_infra_super_scan_cycle
#define mib_promisc		mib_head.mib_promisc
#define mib_uniq_word		mib_head.mib_uniq_word
#define mib_slot_time		mib_head.mib_slot_time
#define mib_roam_low_snr_thresh	mib_head.mib_roam_low_snr_thresh
#define mib_low_snr_count	mib_head.mib_low_snr_count
#define mib_infra_missed_beacon_count \
				mib_head.mib_infra_missed_beacon_count
#define mib_adhoc_missed_beacon_count \
				mib_head.mib_adhoc_missed_beacon_count
#define mib_country_code	mib_head.mib_country_code
#define mib_hop_seq		mib_head.mib_hop_seq
#define mib_hop_seq_len		mib_head.mib_hop_seq_len

#define mib_noise_filter_gain	mib_tail.mib_noise_filter_gain
#define mib_noise_limit_offset	mib_tail.mib_noise_limit_offset
#define mib_rssi_thresh_offset	mib_tail.mib_rssi_thresh_offset
#define mib_busy_thresh_offset	mib_tail.mib_busy_thresh_offset
#define mib_sync_thresh		mib_tail.mib_sync_thresh
#define mib_test_mode		mib_tail.mib_test_mode
#define mib_test_min_chan	mib_tail.mib_test_min_chan
#define mib_test_max_chan	mib_tail.mib_test_max_chan

/*
 * MIB IDs for the update/report param commands
 */
#define	RAY_MIB_NET_TYPE			0
#define	RAY_MIB_AP_STATUS			1
#define	RAY_MIB_SSID				2
#define RAY_MIB_SCAN_MODE			3
#define	RAY_MIB_APM_MODE			4
#define	RAY_MIB_MAC_ADDR			5
#define	RAY_MIB_FRAG_THRESH			6
#define	RAY_MIB_DWELL_TIME			7
#define	RAY_MIB_BEACON_PERIOD			8
#define	RAY_MIB_DTIM_INTERVAL			9
#define	RAY_MIB_MAX_RETRY			10
#define	RAY_MIB_ACK_TIMO			11
#define	RAY_MIB_SIFS				12
#define	RAY_MIB_DIFS				13
#define	RAY_MIB_PIFS				14
#define	RAY_MIB_RTS_THRESH			15
#define	RAY_MIB_SCAN_DWELL			16
#define	RAY_MIB_SCAN_MAX_DWELL			17
#define	RAY_MIB_ASSOC_TIMO			18
#define	RAY_MIB_ADHOC_SCAN_CYCLE		19
#define	RAY_MIB_INFRA_SCAN_CYCLE		20
#define	RAY_MIB_INFRA_SUPER_SCAN_CYCLE		21
#define	RAY_MIB_PROMISC				22
#define	RAY_MIB_UNIQ_WORD			23
#define	RAY_MIB_SLOT_TIME			24
#define	RAY_MIB_ROAM_LOW_SNR_THRESH		25
#define	RAY_MIB_LOW_SNR_COUNT			26
#define	RAY_MIB_INFRA_MISSED_BEACON_COUNT	27
#define	RAY_MIB_ADHOC_MISSED_BEACON_COUNT	28
#define	RAY_MIB_COUNTRY_CODE			29
#define	RAY_MIB_HOP_SEQ				30
#define	RAY_MIB_HOP_SEQ_LEN			31
#define	RAY_MIB_CW_MAX				32
#define	RAY_MIB_CW_MIN				33
#define	RAY_MIB_NOISE_FILTER_GAIN		34
#define	RAY_MIB_NOISE_LIMIT_OFFSET		35
#define	RAY_MIB_RSSI_THRESH_OFFSET		36
#define	RAY_MIB_BUSY_THRESH_OFFSET		37
#define	RAY_MIB_SYNC_THRESH			38
#define	RAY_MIB_TEST_MODE			39
#define	RAY_MIB_TEST_MIN_CHAN			40
#define	RAY_MIB_TEST_MAX_CHAN			41
#define	RAY_MIB_ALLOW_PROBE_RESP		42
#define	RAY_MIB_PRIVACY_MUST_START		43
#define	RAY_MIB_PRIVACY_CAN_JOIN		44
#define	RAY_MIB_BASIC_RATE_SET			45
#define RAY_MIB_VERSION				46
#define RAY_MIB_CUR_BSSID			47
#define RAY_MIB_CUR_INITED			48
#define RAY_MIB_CUR_DEF_TXRATE			49
#define RAY_MIB_CUR_ENCRYPT			50
#define RAY_MIB_CUR_NET_TYPE			51
#define RAY_MIB_CUR_SSID			52
#define RAY_MIB_CUR_PRIV_START			53
#define RAY_MIB_CUR_PRIV_JOIN			54
#define RAY_MIB_DES_BSSID			55
#define RAY_MIB_DES_INITED			56
#define RAY_MIB_DES_DEF_TXRATE			57
#define RAY_MIB_DES_ENCRYPT			58
#define RAY_MIB_DES_NET_TYPE			59
#define RAY_MIB_DES_SSID			60
#define RAY_MIB_DES_PRIV_START			61
#define RAY_MIB_DES_PRIV_JOIN			62
#define RAY_MIB_CUR_AP_STATUS			63
#define RAY_MIB_CUR_PROMISC			64
#define RAY_MIB_DES_AP_STATUS			65
#define RAY_MIB_DES_PROMISC			66
#define RAY_MIB_CUR_FRAMING			67
#define RAY_MIB_DES_FRAMING			68

#define	RAY_MIB_LASTUSER			45
#define	RAY_MIB_MAX				68

/*
 * Strings for the MIB
 */
#define RAY_MIB_STRINGS {		\
	"Network type",			\
	"AP status",			\
	"SSID",				\
	"Scan mode",			\
	"APM mode",			\
	"MAC address",			\
	"Fragmentation threshold",	\
	"Dwell time",			\
	"Beacon period",		\
	"DTIM_INTERVAL",		\
	"MAX_RETRY",			\
	"ACK_TIMO",			\
	"SIFS",				\
	"DIFS",				\
	"PIFS",				\
	"RTS_THRESH",			\
	"SCAN_DWELL",			\
	"SCAN_MAX_DWELL",		\
	"ASSOC_TIMO",			\
	"ADHOC_SCAN_CYCLE",		\
	"INFRA_SCAN_CYCLE",		\
	"INFRA_SUPER_SCAN_CYCLE",	\
	"PROMISC",			\
	"UNIQ_WORD",			\
	"SLOT_TIME",			\
	"ROAM_LOW_SNR_THRESH",		\
	"LOW_SNR_COUNT",		\
	"INFRA_MISSED_BEACON_COUNT",	\
	"ADHOC_MISSED_BEACON_COUNT",	\
	"COUNTRY_CODE",			\
	"HOP_SEQ",			\
	"HOP_SEQ_LEN",			\
	"CW_MAX",			\
	"CW_MIN",			\
	"NOISE_FILTER_GAIN",		\
	"NOISE_LIMIT_OFFSET",		\
	"RSSI_THRESH_OFFSET",		\
	"BUSY_THRESH_OFFSET",		\
	"SYNC_THRESH",			\
	"TEST_MODE",			\
	"TEST_MIN_CHAN",		\
	"TEST_MAX_CHAN",		\
	"ALLOW_PROBE_RESP",		\
	"PRIVACY_MUST_START",		\
	"PRIVACY_CAN_JOIN",		\
	"BASIC_RATE_SET",		\
	"Firmware version",		\
	"Current BSS Id",		\
	"Current INITED",		\
	"Current DEF_TXRATE",		\
	"Current ENCRYPT",		\
	"Current NET_TYPE",		\
	"Current SSID",			\
	"Current PRIV_START",		\
	"Current PRIV_JOIN",		\
	"Desired BSSID",		\
	"Desired INITED",		\
	"Desired DEF_TXRATE",		\
	"Desired ENCRYPT",		\
	"Desired NET_TYPE",		\
	"Desired SSID",			\
	"Desired PRIV_START",		\
	"Desired PRIV_JOIN",		\
	"Current AP_STATUS",		\
	"Current PROMISC",		\
	"Desired AP_STATUS",		\
	"Desired PROMISC",		\
	"Current FRAMING",		\
	"Desired FRAMING"		\
}

#define RAY_MIB_HELP_STRINGS {			\
	"0 Ad hoc, 1 Infrastructure",		\
	"0 Station, 1 Access Point",		\
	"",					\
	"0 Passive, 1 Active",			\
	"0 Off, 1 On",				\
	"",					\
	"Bytes",				\
	"DWELL_TIME",				\
	"BEACON_PERIOD",			\
	"DTIM_INTERVAL",			\
	"MAX_RETRY",				\
	"ACK_TIMO",				\
	"SIFS",					\
	"DIFS",					\
	"PIFS",					\
	"RTS_THRESH",				\
	"SCAN_DWELL",				\
	"SCAN_MAX_DWELL",			\
	"ASSOC_TIMO",				\
	"ADHOC_SCAN_CYCLE",			\
	"INFRA_SCAN_CYCLE",			\
	"INFRA_SUPER_SCAN_CYCLE",		\
	"PROMISC",				\
	"UNIQ_WORD",				\
	"SLOT_TIME",				\
	"ROAM_LOW_SNR_THRESH",			\
	"LOW_SNR_COUNT",			\
	"INFRA_MISSED_BEACON_COUNT",		\
	"ADHOC_MISSED_BEACON_COUNT",		\
	"COUNTRY_CODE",				\
	"HOP_SEQ",				\
	"HOP_SEQ_LEN",				\
	"CW_MAX",				\
	"CW_MIN",				\
	"NOISE_FILTER_GAIN",			\
	"NOISE_LIMIT_OFFSET",			\
	"RSSI_THRESH_OFFSET",			\
	"BUSY_THRESH_OFFSET",			\
	"SYNC_THRESH",				\
	"TEST_MODE",				\
	"TEST_MIN_CHAN",			\
	"TEST_MAX_CHAN",			\
	"ALLOW_PROBE_RESP",			\
	"PRIVACY_MUST_START",			\
	"PRIVACY_CAN_JOIN",			\
	"BASIC_RATE_SET",			\
	"",					\
	"",					\
	"0 Joined a net, 1 Created a net",	\
	"Current DEF_TXRATE",			\
	"Current ENCRYPT",			\
	"Current NET_TYPE",			\
	"",					\
	"Current PRIV_START",			\
	"Current PRIV_JOIN",			\
	"",					\
	"N/A",					\
	"Desired DEF_TXRATE",			\
	"Desired ENCRYPT",			\
	"Desired NET_TYPE",			\
	"",					\
	"Desired PRIV_START",			\
	"Desired PRIV_JOIN",			\
	"Current AP_STATUS",			\
	"Current PROMISC",			\
	"Desired AP_STATUS",			\
	"Desired PROMISC",			\
	"Current FRAMING",			\
	"Desired FRAMING"			\
}

/*
 * Applicable versions and work size for each MIB element
 */
#define RAY_MIB_INFO_SIZ4 1
#define RAY_MIB_INFO_SIZ5 2
#define RAY_MIB_SIZE(info, mib, version) \
	info[(mib)][(version & RAY_V4)?RAY_MIB_INFO_SIZ4:RAY_MIB_INFO_SIZ5]
#define RAY_MIB_INFO {							\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_NET_TYPE */			\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_AP_STATUS */			\
{RAY_V4|RAY_V5,	IEEE80211_NWID_LEN, 					\
			IEEE80211_NWID_LEN},/* RAY_MIB_SSID */		\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_SCAN_MODE */			\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_APM_MODE */			\
{RAY_V4|RAY_V5,	ETHER_ADDR_LEN,						\
			ETHER_ADDR_LEN},/* RAY_MIB_MAC_ADDR */		\
{RAY_V4|RAY_V5,	2,	2},	/* RAY_MIB_FRAG_THRESH */		\
{RAY_V4|RAY_V5,	2,	2},	/* RAY_MIB_DWELL_TIME */		\
{RAY_V4|RAY_V5,	2,	2},	/* RAY_MIB_BEACON_PERIOD */		\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_DTIM_INTERVAL */		\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_MAX_RETRY */			\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_ACK_TIMO */			\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_SIFS */			\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_DIFS */			\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_PIFS */			\
{RAY_V4|RAY_V5,	2,	2},	/* RAY_MIB_RTS_THRESH */		\
{RAY_V4|RAY_V5,	2,	2},	/* RAY_MIB_SCAN_DWELL */		\
{RAY_V4|RAY_V5,	2,	2},	/* RAY_MIB_SCAN_MAX_DWELL */		\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_ASSOC_TIMO */		\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_ADHOC_SCAN_CYCLE */		\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_INFRA_SCAN_CYCLE */		\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_INFRA_SUPER_SCAN_CYCLE */	\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_PROMISC */			\
{RAY_V4|RAY_V5,	2,	2},	/* RAY_MIB_UNIQ_WORD */			\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_SLOT_TIME */			\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_ROAM_LOW_SNR_THRESH */	\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_LOW_SNR_COUNT */		\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_INFRA_MISSED_BEACON_COUNT */	\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_ADHOC_MISSED_BEACON_COUNT */	\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_COUNTRY_CODE */		\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_HOP_SEQ */			\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_HOP_SEQ_LEN */		\
{RAY_V4|RAY_V5,	1,	2},	/* RAY_MIB_CW_MAX */			\
{RAY_V4|RAY_V5,	1,	2},	/* RAY_MIB_CW_MIN */			\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_NOISE_FILTER_GAIN */		\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_NOISE_LIMIT_OFFSET */	\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_RSSI_THRESH_OFFSET */	\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_BUSY_THRESH_OFFSET */	\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_SYNC_THRESH */		\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_TEST_MODE */			\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_TEST_MIN_CHAN */		\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_TEST_MAX_CHAN */		\
{       RAY_V5,	0,	1},	/* RAY_MIB_ALLOW_PROBE_RESP */		\
{       RAY_V5,	0,	1},	/* RAY_MIB_PRIVACY_MUST_START */	\
{       RAY_V5,	0,	1},	/* RAY_MIB_PRIVACY_CAN_JOIN */		\
{       RAY_V5,	0,	8},	/* RAY_MIB_BASIC_RATE_SET */		\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_VERSION */			\
{RAY_V4|RAY_V5,	ETHER_ADDR_LEN,						\
			ETHER_ADDR_LEN},/* RAY_MIB_CUR_BSSID */		\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_CUR_INITED */		\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_CUR_DEF_TXRATE */		\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_CUR_ENCRYPT */		\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_CUR_NET_TYPE */		\
{RAY_V4|RAY_V5,	IEEE80211_NWID_LEN,					\
			IEEE80211_NWID_LEN}, /* RAY_MIB_CUR_SSID */	\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_CUR_PRIV_START */		\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_CUR_PRIV_JOIN */		\
{RAY_V4|RAY_V5,	ETHER_ADDR_LEN,						\
			ETHER_ADDR_LEN},/* RAY_MIB_DES_BSSID */		\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_DES_INITED */		\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_DES_DEF_TXRATE */		\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_DES_ENCRYPT */		\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_DES_NET_TYPE */		\
{RAY_V4|RAY_V5,	IEEE80211_NWID_LEN, 					\
			IEEE80211_NWID_LEN}, /* RAY_MIB_DES_SSID */	\
{RAY_V4|RAY_V5,	1,	1},	/* RAY_MIB_DES_PRIV_START */		\
{RAY_V4|RAY_V5,	1, 	1}, 	/* RAY_MIB_DES_PRIV_JOIN */		\
{RAY_V4|RAY_V5,	1, 	1}, 	/* RAY_MIB_CUR_AP_STATUS */		\
{RAY_V4|RAY_V5,	1, 	1}, 	/* RAY_MIB_CUR_PROMISC */		\
{RAY_V4|RAY_V5,	1, 	1}, 	/* RAY_MIB_DES_AP_STATUS */		\
{RAY_V4|RAY_V5,	1, 	1}, 	/* RAY_MIB_DES_PROMISC */		\
{RAY_V4|RAY_V5,	1, 	1}, 	/* RAY_MIB_CUR_FRAMING */		\
{RAY_V4|RAY_V5,	1, 	1} 	/* RAY_MIB_DES_FRAMING */		\
}

/*
 * MIB values
 *
 * I've included comments as to where the numbers have originated
 * from.
 *
 * Linux refers to ray_cs.c and rayctl.h from version 167 of the
 * Linux Raylink driver.
 *
 * NetBSD refers to if_ray.c from version 1.12 of the NetBSD Raylink
 * driver.
 *
 * Symb refers to numbers cleaned from the 802.11 specification,
 * discussion with 802.11 knowledgable people at Symbionics or
 * stuff needed by me (i.e. me, * aps, ifo, hjl).
 *
 * V4 and V5 refer to settings for version 4 and version 5 of
 * the firmware.
 *
 * DOC refers to the
 *	Combined Interface Requirements Specification
 *	and Interface Design Document (IRS/IDD)
 *	for the
 *	WLAN System Interfaces Between the
 *	HOST COMPUTER and the
 *	PCMCIA WLAN INTERFACE CARD
 *	Revision ECF 5.00
 *	17 June, 1998
 */

/* Obtained by raycontrol _before_ downloading
 *
 * WebGear Aviator
 *
 * # raycontrol -i ray0
 * Firmware version                4
 * Network type                    0x01    0 Ad hoc, 1 Infrastructure
 * AP status                       0x00    0 Station, 1 Access Point
 * SSID                                                            
 * Scan mode                       0x01    0 Passive, 1 Active
 * APM mode                        0x00    0 Off, 1 On
 * MAC address                     00:00:8f:48:e4:44
 * Fragmentation threshold         0x0200  FRAG_THRESH
 * Dwell tIME                      0x01    DWELL_TIME
 * Beacon period                   0x01    BEACON_PERIOD
 * DTIM_INTERVAL                   0x05    DTIM_INTERVAL
 * MAX_RETRY                       0x03    MAX_RETRY
 * ACK_TIMO                        0x8c    ACK_TIMO
 * SIFS                            0x1e    SIFS
 * DIFS                            0x82    DIFS
 * PIFS                            0xce    PIFS
 * RTS_THRESH                      0x0100  RTS_THRESH
 * SCAN_DWELL                      0xfc18  SCAN_DWELL
 * SCAN_MAX_DWELL                  0xc180  SCAN_MAX_DWELL
 * ASSOC_TIMO                      0x05    ASSOC_TIMO
 * ADHOC_SCAN_CYCLE                0x04    ADHOC_SCAN_CYCLE
 * INFRA_SCAN_CYCLE                0x02    INFRA_SCAN_CYCLE
 * INFRA_SUPER_SCAN_CYCLE          0x04    INFRA_SUPER_SCAN_CYCLE
 * PROMISC                         0x00    PROMISC
 * UNIQ_WORD                       0x0cbd  UNIQ_WORD
 * SLOT_TIME                       0x4e    SLOT_TIME
 * ROAM_LOW_SNR_THRESH             0x20    ROAM_LOW_SNR_THRESH
 * LOW_SNR_COUNT                   0x04    LOW_SNR_COUNT
 * INFRA_MISSED_BEACON_COUNT       0x04    INFRA_MISSED_BEACON_COUNT
 * ADHOC_MISSED_BEACON_COUNT       0x04    ADHOC_MISSED_BEACON_COUNT
 * COUNTRY_CODE                    0x01    COUNTRY_CODE
 * HOP_SEQ                         0x07    HOP_SEQ
 * HOP_SEQ_LEN                     0x4e    HOP_SEQ_LEN
 * CW_MAX                          0x3f    CW_MAX
 * CW_MIN                          0x0f    CW_MIN
 * NOISE_FILTER_GAIN               0x00    NOISE_FILTER_GAIN
 * NOISE_LIMIT_OFFSET              0x00    NOISE_LIMIT_OFFSET
 * RSSI_THRESH_OFFSET              0x70    RSSI_THRESH_OFFSET
 * BUSY_THRESH_OFFSET              0x70    BUSY_THRESH_OFFSET
 * SYNC_THRESH                     0x07    SYNC_THRESH
 * TEST_MODE                       0x00    TEST_MODE
 * TEST_MIN_CHAN                   0x02    TEST_MIN_CHAN
 * TEST_MAX_CHAN                   0x02    TEST_MAX_CHAN
 *
 * Raylink
 * Firmware version          	5
 * Network type              	0x01	0 Ad hoc, 1 Infrastructure
 * AP status                 	0x00	0 Station, 1 Access Point
 * SSID                      	ESSID1                          	
 * Scan mode                 	0x01	0 Passive, 1 Active
 * APM mode                  	0x00	0 Off, 1 On
 * MAC address               	00:00:8f:a8:17:06	
 * Fragmentation threshold   	0x7fff	Bytes
 * Dwell time                	0x0080	DWELL_TIME
 * Beacon period             	0x0100	BEACON_PERIOD
 * DTIM_INTERVAL             	0x01	DTIM_INTERVAL
 * MAX_RETRY                 	0x1f	MAX_RETRY
 * ACK_TIMO                  	0x86	ACK_TIMO
 * SIFS                      	0x1c	SIFS
 * DIFS                      	0x82	DIFS
 * PIFS                      	0x4e	PIFS
 * RTS_THRESH                	0x7fff	RTS_THRESH
 * SCAN_DWELL                	0x04e2	SCAN_DWELL
 * SCAN_MAX_DWELL            	0x38a4	SCAN_MAX_DWELL
 * ASSOC_TIMO                	0x05	ASSOC_TIMO
 * ADHOC_SCAN_CYCLE          	0x08	ADHOC_SCAN_CYCLE
 * INFRA_SCAN_CYCLE          	0x02	INFRA_SCAN_CYCLE
 * INFRA_SUPER_SCAN_CYCLE    	0x08	INFRA_SUPER_SCAN_CYCLE
 * PROMISC                   	0x00	PROMISC
 * UNIQ_WORD                 	0x0cbd	UNIQ_WORD
 * SLOT_TIME                 	0x32	SLOT_TIME
 * ROAM_LOW_SNR_THRESH       	0xff	ROAM_LOW_SNR_THRESH
 * LOW_SNR_COUNT             	0xff	LOW_SNR_COUNT
 * INFRA_MISSED_BEACON_COUNT 	0x02	INFRA_MISSED_BEACON_COUNT
 * ADHOC_MISSED_BEACON_COUNT 	0xff	ADHOC_MISSED_BEACON_COUNT
 * COUNTRY_CODE              	0x01	COUNTRY_CODE
 * HOP_SEQ                   	0x0b	HOP_SEQ
 * HOP_SEQ_LEN               	0x55	HOP_SEQ_LEN
 * CW_MAX                    	0x003f	CW_MAX
 * CW_MIN                    	0x000f	CW_MIN
 * NOISE_FILTER_GAIN         	0x04	NOISE_FILTER_GAIN
 * NOISE_LIMIT_OFFSET        	0x08	NOISE_LIMIT_OFFSET
 * RSSI_THRESH_OFFSET        	0x28	RSSI_THRESH_OFFSET
 * BUSY_THRESH_OFFSET        	0x28	BUSY_THRESH_OFFSET
 * SYNC_THRESH               	0x07	SYNC_THRESH
 * TEST_MODE                 	0x00	TEST_MODE
 * TEST_MIN_CHAN             	0x02	TEST_MIN_CHAN
 * TEST_MAX_CHAN             	0x02	TEST_MAX_CHAN
 * ALLOW_PROBE_RESP          	0x00	ALLOW_PROBE_RESP
 * PRIVACY_MUST_START        	0x00	PRIVACY_MUST_START
 * PRIVACY_CAN_JOIN          	0x00	PRIVACY_CAN_JOIN
 * BASIC_RATE_SET            	0x02	BASIC_RATE_SET
 */

/*
 * mib_net_type
 *
 * DOC		0x01	- Defines network type for Start and Join
 *			- Network commands.
 *
 * As the V4 cards don't do infra we have to use adhoc. For V5 cards
 * we follow standard FreeBSD practise and use infrastructure mode.
 */
#define	RAY_MIB_NET_TYPE_ADHOC			0x00
#define	RAY_MIB_NET_TYPE_INFRA			0x01
#define	RAY_MIB_NET_TYPE_V4			RAY_MIB_NET_TYPE_ADHOC
#define	RAY_MIB_NET_TYPE_V5			RAY_MIB_NET_TYPE_INFRA

/*
 * mib_ap_status
 *
 * DOC		0x00	- Applicable only when Network Type is
 *			- Infrastructure.
 */
#define	RAY_MIB_AP_STATUS_TERMINAL		0x00
#define	RAY_MIB_AP_STATUS_AP			0x01
#define	RAY_MIB_AP_STATUS_V4			RAY_MIB_AP_STATUS_TERMINAL
#define	RAY_MIB_AP_STATUS_V5			RAY_MIB_AP_STATUS_TERMINAL

/*
 * mib_ssid
 *
 * DOC		ESSID1	- Service Set ID. Can be any ASCII string
 *			- up to 32 bytes in length. If the string is
 *			- less than 32 bytes long, it must be
 *			- followed by a byte of 00h.
 *
 * Symb			- windows setting comes from the Aviator software v1.1
 */
#define RAY_MIB_SSID_WINDOWS			"NETWORK_NAME"
#define RAY_MIB_SSID_RAYLINK			"ESSID1"
#define RAY_MIB_SSID_V4				RAY_MIB_SSID_WINDOWS
#define RAY_MIB_SSID_V5				RAY_MIB_SSID_RAYLINK

/*
 * mib_scan_mode
 *
 * DOC		0x01	- Defines acquisition approach for
 *			- terminals operating in either Ad Hoc or
 *			- Infrastructure Networks. N/A for APs. 
 */
#define	RAY_MIB_SCAN_MODE_PASSIVE		0x00
#define	RAY_MIB_SCAN_MODE_ACTIVE		0x01
#define	RAY_MIB_SCAN_MODE_V4			RAY_MIB_SCAN_MODE_ACTIVE
#define	RAY_MIB_SCAN_MODE_V5			RAY_MIB_SCAN_MODE_ACTIVE

/*
 * mib_apm_mode
 *
 * DOC		0x00	- Defines power management mode for
 *			- stations operating in either Ad Hoc or
 *			- Infrastructure Networks. Must always
 *			- be 0 for APs.
 */
#define	RAY_MIB_APM_MODE_NONE			0x00
#define	RAY_MIB_APM_MODE_POWERSAVE		0x01
#define	RAY_MIB_APM_MODE_V4			RAY_MIB_APM_MODE_NONE
#define	RAY_MIB_APM_MODE_V5			RAY_MIB_APM_MODE_NONE

/*
 * mib_mac_addr
 *
 * DOC			- MAC Address to be used by WIC (For
 *			- format see Figure 3.2.4.1.2-1, MAC
 *			- Address Format). Host may echo card
 *			- supplied address or use locally
 *			- administered address.
 */

/*
 * mib_frag_thresh
 *
 * DOC		0x7fff	- Maximum over-the-air packet size (in
 *			- bytes)
 *
 * Symb		0xXXXX	- you really should fragment when in low signal
 *			- conditions but getting it wrong
 *			  crucifies the performance
 */
#define RAY_MIB_FRAG_THRESH_MINIMUM		0
#define RAY_MIB_FRAG_THRESH_MAXIMUM		2346
#define RAY_MIB_FRAG_THRESH_DISABLE		0x7fff
#define RAY_MIB_FRAG_THRESH_V4			RAY_MIB_FRAG_THRESH_DISABLE
#define RAY_MIB_FRAG_THRESH_V5			RAY_MIB_FRAG_THRESH_DISABLE

/*
 * mib_dwell_time
 *
 * DOC		0x0080	- Defines hop dwell time in Kusec.
 *			- Required only of stations which intend
 *			- to issue a Start Network command.
 *			- Forward Compatible Firmware (Build
 *			- 5) requires that the dwell time be one of
 *			- the set 16, 32, 64, 128, and 256.
 *
 * Linux.h		- 16k * 2**n, n=0-4 in Kus
 * Linux.c-V4	0x0200
 * Linux.c-V5	0x0080	- 128 Kus
 * NetBSD-V4	0x0200	- from Linux
 * NetBSD-V4	0x0400	- "divined"
 * NetBSD-V5	0x0080
 * Symb-V4	0xXXXX	- 802.11 dwell time is XXX Kus
 * Symb-V5	0xXXXX	- 802.11 dwell time is XXX Kus
 *
 * XXX confirm that 1024Kus is okay for windows driver - how? and see
 * XXX how it is over the maximum
 */
#define RAY_MIB_DWELL_TIME_MINIMUM		1
#define RAY_MIB_DWELL_TIME_MAXIMUM		390
#define	RAY_MIB_DWELL_TIME_V4			0x0400
#define	RAY_MIB_DWELL_TIME_V5			0x0080

/*
 * mib_beacon_period
 *
 * DOC		0x0100	- Defines time between target beacon
 *			- transmit times (TBTT) in Kusec.
 *			- Forward Compatible Firmware (Build
 *			- 5) requires that the Beacon Period be an
 *			- integral multiple of the Dwell Time (not
 *			- exceeding 255 hops).
 *			- Required only of stations which intend
 *			- to issue a Start Network command.
 *
 * Linux.h		- n * a_hop_time  in Kus
 * Linux.c-V4	0x0001
 * Linux.c-V5	0x0100	- 256 Kus
 * NetBSD-V4	0x0001	- from Linux
 * NetBSD-V4	0x0000	- "divined"
 * NetBSD-V5	0x0100
 * Symb-V4	0x0001	- best performance is one beacon each dwell XXX
 * Symb-V5	0x0080	- best performance is one beacon each dwell XXX
 *
 * XXX V4 should probably set this to dwell_time
 */
#define	RAY_MIB_BEACON_PERIOD_MINIMUM		1
#define	RAY_MIB_BEACON_PERIOD_MAXIMUM		0xffff
#define	RAY_MIB_BEACON_PERIOD_V4		0x0001
#define	RAY_MIB_BEACON_PERIOD_V5		(2*RAY_MIB_DWELL_TIME_V5)

/*
 * mib_dtim_interval
 *
 * DOC		0x01	- Number of beacons per DTIM period.
 *			- Only APs will use this parameter, to set
 *			- the DTIM period.
 *
 * Linux.h		- in beacons
 * Linux.c	0x01
 * NetBSD	0x01
 * Symb		0xXX	- need to find out what DTIM is
 */
#define	RAY_MIB_DTIM_INTERVAL_MINIMUM		1
#define	RAY_MIB_DTIM_INTERVAL_MAXIMUM		255
#define	RAY_MIB_DTIM_INTERVAL_V4		0x01
#define	RAY_MIB_DTIM_INTERVAL_V5		0x01

/*
 * mib_max_retry
 *
 * DOC		31	- Number of times WIC will attempt to
 *			- retransmit a failed packet.
 *
 * Linux.c	0x07
 * NetBSD	0x01	- "documented default for 5/6"
 * NetBSD	0x07	- from Linux
 * NetBSD	0x03	- "divined"
 * Symb		0xXX	- 7 retries seems okay but check with APS
 */
#define	RAY_MIB_MAX_RETRY_MINIMUM		0
#define	RAY_MIB_MAX_RETRY_MAXIMUM		255
#define	RAY_MIB_MAX_RETRY_V4			0x07
#define	RAY_MIB_MAX_RETRY_V5			0x1f

/*
 * mib_ack_timo
 *
 * DOC		0x86	- Time WIC will wait after completion of
 *			- a transmit before timing out anticipated
 *			- ACK (2 usec steps). Should equal
 *			- SIFS + constant.
 *
 * Linux.c	0xa3
 * NetBSD	0x86	- documented default for 5/6
 * NetBSD	0xa3	- from Linux
 * NetBSD	0xa3	- "divined"
 * Symb		0xXX	- this must be a 802.11 defined setting?
 */
#define	RAY_MIB_ACK_TIMO_MINIMUM		0
#define	RAY_MIB_ACK_TIMO_MAXIMUM		255
#define	RAY_MIB_ACK_TIMO_V4			0xa3
#define	RAY_MIB_ACK_TIMO_V5			0x86

/*
 * mib_sifs
 *
 * DOC		0x1c	- SIFS time in usec.
 *
 * Linux.c	0x1d
 * NetBSD	0x1c	- documented default for 5/6
 * NetBSD	0x1d	- from Linux
 * NetBSD	0x1d	- "divined"
 * Symb		0xXX	- default SIFS for 802.11
 */
#define	RAY_MIB_SIFS_MINIMUM			28
#define	RAY_MIB_SIFS_MAXIMUM			62
#define	RAY_MIB_SIFS_V4				0x1d
#define	RAY_MIB_SIFS_V5				0x1c

/*
 * mib_difs
 *
 * DOC		0x82	- DIFS time in usec.
 */
#define	RAY_MIB_DIFS_MINIMUM			130
#define	RAY_MIB_DIFS_MAXIMUM			255
#define	RAY_MIB_DIFS_V4				0x82
#define	RAY_MIB_DIFS_V5				0x82

/*
 * mib_pifs
 *
 * DOC		78	- PIFS time in usec. (Not currently
 *			- implemented.
 */
#define	RAY_MIB_PIFS_MINIMUM			78
#define	RAY_MIB_PIFS_MAXIMUM			255
#define	RAY_MIB_PIFS_V4				0xce
#define	RAY_MIB_PIFS_V5				0x4e

/*
 * mib_rts_thresh
 *
 * DOC		0x7ffff	- Threshold size in bytes below which
 *			- messages will not require use of RTS
 *			- Protocol.
 *
 * Linux.c	0x7fff
 * NetBSD	0x7fff	- disabled
 * Symb		0xXXXX	- need to set this realistically to get CTS/RTS mode
 *			  working right
 */
#define	RAY_MIB_RTS_THRESH_MINIMUM		0
#define	RAY_MIB_RTS_THRESH_MAXIMUM		2346
#define	RAY_MIB_RTS_THRESH_DISABLE		0x7fff
#define	RAY_MIB_RTS_THRESH_V4			RAY_MIB_RTS_THRESH_DISABLE
#define	RAY_MIB_RTS_THRESH_V5			RAY_MIB_RTS_THRESH_DISABLE

/*
 * mib_scan_dwell
 *
 * DOC		0x04e2	- Time channel remains clear after probe
 *			- transmission prior to hopping to next
 *			- channel. (in 2 msec steps).
 *
 * Linux.c-V4	0xfb1e	- 128572us
 * Linix.c-V5	0x04e2	-   2500us
 * NetBSD-V4	0xfb1e
 * NetBSD-V5	0x04e2
 * Symb		0xXXXX	- Check that v4 h/w can do 2.5ms and default it
 */
#define	RAY_MIB_SCAN_DWELL_MINIMUM		1
#define	RAY_MIB_SCAN_DWELL_MAXIMUM		65535
#define	RAY_MIB_SCAN_DWELL_V4			0xfb1e
#define	RAY_MIB_SCAN_DWELL_V5			0x04e2

/*
 * mib_scan_max_dwell
 *
 * DOC		0x38a4	- Time to remain on a frequency channel
 *			- if CCA is detected after probe
 *			- transmission. (in 2 usec steps).
 *
 * Linux.c-V4	0xc75c	- 102072us
 * Linix.c-V5	0x38a4	-  29000us
 * NetBSD-V4	0xc75c
 * NetBSD-V5	0x38a4
 * Symb		0xXXXX	- see above - this may be total time before giving up
 */
#define	RAY_MIB_SCAN_MAX_DWELL_MINIMUM		1
#define	RAY_MIB_SCAN_MAX_DWELL_MAXIMUM		65535
#define	RAY_MIB_SCAN_MAX_DWELL_V4		0xc75c
#define	RAY_MIB_SCAN_MAX_DWELL_V5		0x38a4

/*
 * mib_assoc_timo
 *
 * DOC		0x05	- Time (in hops) a station waits after
 *			- transmitting an Association Request
 *			- Message before association attempt is
 *			- considered failed. N/A for Ad Hoc
 *			- Networks and for APs in Infrastructure
 */
#define	RAY_MIB_ASSOC_TIMO_MINIMUM		0
#define	RAY_MIB_ASSOC_TIMO_MAXIMUM		255
#define	RAY_MIB_ASSOC_TIMO_V4			0x05
#define	RAY_MIB_ASSOC_TIMO_V5			0x05

/*
 * mib_adhoc_scan_cycle
 *
 * DOC		0x08	- Maximum number of times to cycle
 *			- through frequency hopping pattern as
 *			- part of scanning during Ad Hoc
 *			- Acquisition.
 */
#define	RAY_MIB_ADHOC_SCAN_CYCLE_MINIMUM	1
#define	RAY_MIB_ADHOC_SCAN_CYCLE_MAXIMUM	255
#define	RAY_MIB_ADHOC_SCAN_CYCLE_V4		0x08
#define	RAY_MIB_ADHOC_SCAN_CYCLE_V5		0x08

/*
 * mib_infra_scan_cycle
 *
 * DOC		0x02	- Number of times to cycle through
 *			- frequency hopping pattern as part of
 *			- scanning during Infrastructure Network
 *			- Acquisition.
 */
#define	RAY_MIB_INFRA_SCAN_CYCLE_MINIMUM	1
#define	RAY_MIB_INFRA_SCAN_CYCLE_MAXIMUM	255
#define	RAY_MIB_INFRA_SCAN_CYCLE_V4		0x02
#define	RAY_MIB_INFRA_SCAN_CYCLE_V5		0x02

/*
 * mib_infra_super_scan_cycle
 *
 * DOC		0x08	- Number of times to repeat an
 *			- Infrastructure scan cycle if no APs are
 *			- found before indicating a failure.
 */
#define	RAY_MIB_INFRA_SUPER_SCAN_CYCLE_MINIMUM	1
#define	RAY_MIB_INFRA_SUPER_SCAN_CYCLE_MAXIMUM	255
#define	RAY_MIB_INFRA_SUPER_SCAN_CYCLE_V4		0x08
#define	RAY_MIB_INFRA_SUPER_SCAN_CYCLE_V5		0x08

/*
 * mib_promisc
 *
 * DOC		0x00	- Controls operation of WIC in
 *			- promiscuous mode.
 */
#define	RAY_MIB_PROMISC_DISABLED		0
#define	RAY_MIB_PROMISC_ENABLED			1
#define	RAY_MIB_PROMISC_V4			0x00
#define	RAY_MIB_PROMISC_V5			0x00

/*
 * mib_uniq_word
 *
 * DOC		0x0cdb	- Unique word pattern (Transmitted as
 *			- 0CBDh per 802.11)
 */
#define	RAY_MIB_UNIQ_WORD_MINIMUM		0
#define	RAY_MIB_UNIQ_WORD_MAXIMUM		0xffff
#define	RAY_MIB_UNIQ_WORD_V4			0x0cbd
#define	RAY_MIB_UNIQ_WORD_V5			0x0cbd

/*
 * mib_slot_time
 *
 * DOC		0x32	- Slot time in usec
 *
 * Linux.c-V4	0x4e
 * Linix.c-V5	0x32
 * NetBSD-V4	0x4e	- Linux
 * NetBSD-V4	0x18	- "divined"
 * NetBSD-V5	0x32	- mentions spec. is 50us i.e. 0x32
 * Symb		0xXX	- wtf 0x4e = 78
 */
#define	RAY_MIB_SLOT_TIME_MINIMUM		1
#define	RAY_MIB_SLOT_TIME_MAXIMUM		128
#define	RAY_MIB_SLOT_TIME_V4			0x4e
#define	RAY_MIB_SLOT_TIME_V5			0x32

/*
 * mib_roam_low_snr_thresh
 *
 * DOC		0xff	- SNR Threshold for use by roaming
 *			- algorithm. [Low power count is
 *			- incremented when Beacon is received at
 *			- SNR lower than Roaming Low SNR
 *			- Threshold.] To disable, set to FFh.
 *
 * Linux.c	0xff
 * NetBSD-V4	0xff	- Linux
 * NetBSD-V4	0x30	- "divined"
 * NetBSD-V5	0xff	- disabled
 * NetBSD.h		- if below this inc count
 * Symb		0xXX	- hmm is 0xff really disabled? need this to work
 */
#define	RAY_MIB_ROAM_LOW_SNR_THRESH_MINIMUM	0
#define	RAY_MIB_ROAM_LOW_SNR_THRESH_MAXIMUM	255
#define	RAY_MIB_ROAM_LOW_SNR_THRESH_DISABLED	0xff
#define	RAY_MIB_ROAM_LOW_SNR_THRESH_V4		RAY_MIB_ROAM_LOW_SNR_THRESH_DISABLED	
#define	RAY_MIB_ROAM_LOW_SNR_THRESH_V5		RAY_MIB_ROAM_LOW_SNR_THRESH_DISABLED	

/*
 * mib_low_snr_count
 *
 * DOC		0xff	- Threshold that number of consecutive
 *			- beacons received at SNR < Roaming
 *			- Low SNR Threshold must exceed
 *			- before roaming processing begins. To
 *			- disable, set to FFh.
 *
 * Linux.c	0xff
 * NetBSD	0x07	- "divined - check" and marked as disabled
 * NetBSD	0xff	- disabled
 * NetBSD.h		- roam after cnt below thrsh
 * Symb		0xXX	- hmm is 0xff really disabled? need
 *			- this to work in infrastructure mode with mutliple APs
 */
#define	RAY_MIB_LOW_SNR_COUNT_MINIMUM		0
#define	RAY_MIB_LOW_SNR_COUNT_MAXIMUM		255
#define	RAY_MIB_LOW_SNR_COUNT_DISABLED		0xff
#define	RAY_MIB_LOW_SNR_COUNT_V4		RAY_MIB_LOW_SNR_COUNT_DISABLED
#define	RAY_MIB_LOW_SNR_COUNT_V5		RAY_MIB_LOW_SNR_COUNT_DISABLED

/*
 * mib_infra_missed_beacon_count
 *
 * DOC		0x02	- Threshold that number of consecutive
 *			- beacons not received must exceed
 *			- before roaming processing begins in an
 *			- infrastructure network. To disable, set
 *			- to FFh.
 * Linux.c	0x05
 * NetBSD	0x02	- documented default for 5/6
 * NetBSD	0x05	- Linux
 * NetBSD	0x07	- "divined - check, looks fishy"
 * Symb		0xXX	- 5 missed beacons is probably okay
 */
#define	RAY_MIB_INFRA_MISSED_BEACON_COUNT_MINIMUM	0
#define	RAY_MIB_INFRA_MISSED_BEACON_COUNT_MAXIMUM	255
#define	RAY_MIB_INFRA_MISSED_BEACON_COUNT_V4		0x05
#define	RAY_MIB_INFRA_MISSED_BEACON_COUNT_V5		0x02

/*
 * mib_adhoc_missed_beacon_count
 *
 * DOC		0xff	- Threshold that number of consecutive
 *			- beacons transmitted by a terminal must
 *			- exceed before reacquisition processing
 *			- begins in Ad Hoc Network.
 */
#define	RAY_MIB_ADHOC_MISSED_BEACON_COUNT_MINIMUM	0
#define	RAY_MIB_ADHOC_MISSED_BEACON_COUNT_MAXIMUM	255
#define	RAY_MIB_ADHOC_MISSED_BEACON_COUNT_DISABLED	0xff
#define	RAY_MIB_ADHOC_MISSED_BEACON_COUNT_V4		RAY_MIB_ADHOC_MISSED_BEACON_COUNT_DISABLED
#define	RAY_MIB_ADHOC_MISSED_BEACON_COUNT_V5		RAY_MIB_ADHOC_MISSED_BEACON_COUNT_DISABLED

/*
 * mib_country_code
 *
 * DOC		0x01	- Country set of hopping patterns
 *			- (element value in beacon)
 *			- Note: Japan Test is for a special test
 *			- mode required by the Japanese
 *			- regulatory authorities.
 */
#define	RAY_MIB_COUNTRY_CODE_MIMIMUM		0x01
#define	RAY_MIB_COUNTRY_CODE_MAXIMUM		0x09
#define	RAY_MIB_COUNTRY_CODE_USA		0x01
#define	RAY_MIB_COUNTRY_CODE_EUROPE		0x02
#define	RAY_MIB_COUNTRY_CODE_JAPAN		0x03
#define	RAY_MIB_COUNTRY_CODE_KOREA		0x04
#define	RAY_MIB_COUNTRY_CODE_SPAIN		0x05
#define	RAY_MIB_COUNTRY_CODE_FRANCE		0x06
#define	RAY_MIB_COUNTRY_CODE_ISRAEL		0x07
#define	RAY_MIB_COUNTRY_CODE_AUSTRALIA		0x08
#define	RAY_MIB_COUNTRY_CODE_JAPAN_TEST		0x09
#define	RAY_MIB_COUNTRY_CODE_V4			RAY_MIB_COUNTRY_CODE_USA
#define	RAY_MIB_COUNTRY_CODE_V5			RAY_MIB_COUNTRY_CODE_USA

/*
 * mib_hop_seq
 *
 * DOC		0x0b	- Hop Pattern to use. (Currently 66
 *			- US/Europe plus 12 Japanese).
 *
 * NetBSD.h		- no longer supported
 */
#define	RAY_MIB_HOP_SEQ_MINIMUM			6
#define	RAY_MIB_HOP_SEQ_MAXIMUM			72
#define	RAY_MIB_HOP_SEQ_V4			0x0b
#define	RAY_MIB_HOP_SEQ_V5			0x04

/*
 * mib_hop_seq_len
 *
 * DOC		0x4f	- Number of frequency channels in
 *			- hopping pattern is now set to the value
 *			- defined in IEEE802.11 for the selected
 *			- Current Country Code.
 */
#define	RAY_MIB_HOP_SEQ_LEN_MINIMUM		1
#define	RAY_MIB_HOP_SEQ_LEN_MAXIMUM		79
#define	RAY_MIB_HOP_SEQ_LEN_V4			0x4e
#define	RAY_MIB_HOP_SEQ_LEN_V5			0x4f

/*
 * All from here down are the same in Linux/NetBSD and seem to be sane.
 */
#define	RAY_MIB_CW_MAX_V4			0x3f
#define	RAY_MIB_CW_MAX_V5			0x003f

#define	RAY_MIB_CW_MIN_V4			0x0f
#define	RAY_MIB_CW_MIN_V5			0x000f

/*
 * Symb		0xXX	- these parameters will affect the clear channel
 *			  assesment false triggering
 *
 */
#define	RAY_MIB_NOISE_FILTER_GAIN_DEFAULT	0x04
#define	RAY_MIB_NOISE_LIMIT_OFFSET_DEFAULT	0x08
#define RAY_MIB_RSSI_THRESH_OFFSET_DEFAULT	0x28
#define RAY_MIB_BUSY_THRESH_OFFSET_DEFAULT	0x28
#define RAY_MIB_SYNC_THRESH_DEFAULT		0x07

#define	RAY_MIB_TEST_MODE_NORMAL		0x0
#define	RAY_MIB_TEST_MODE_ANT_1			0x1
#define	RAY_MIB_TEST_MODE_ATN_2			0x2
#define	RAY_MIB_TEST_MODE_ATN_BOTH		0x3
#define RAY_MIB_TEST_MODE_DEFAULT		RAY_MIB_TEST_MODE_NORMAL

#define RAY_MIB_TEST_MIN_CHAN_DEFAULT		0x02
#define RAY_MIB_TEST_MAX_CHAN_DEFAULT		0x02

#define	RAY_MIB_ALLOW_PROBE_RESP_DISALLOW	0x0
#define	RAY_MIB_ALLOW_PROBE_RESP_ALLOW		0x1
#define	RAY_MIB_ALLOW_PROBE_RESP_DEFAULT	RAY_MIB_ALLOW_PROBE_RESP_DISALLOW

#define	RAY_MIB_PRIVACY_MUST_START_NOWEP	0x0
#define	RAY_MIB_PRIVACY_MUST_START_WEP		0x1
#define	RAY_MIB_PRIVACY_MUST_START_DEFAULT	RAY_MIB_PRIVACY_MUST_START_NOWEP

#define	RAY_MIB_PRIVACY_CAN_JOIN_NOWEP		0x0
#define	RAY_MIB_PRIVACY_CAN_JOIN_WEP		0x1
#define	RAY_MIB_PRIVACY_CAN_JOIN_DONT_CARE	0x2
#define	RAY_MIB_PRIVACY_CAN_JOIN_DEFAULT	RAY_MIB_PRIVACY_CAN_JOIN_NOWEP

#define	RAY_MIB_BASIC_RATE_SET_MINIMUM		1
#define	RAY_MIB_BASIC_RATE_SET_MAXIMUM		4
#define	RAY_MIB_BASIC_RATE_SET_500K		1
#define	RAY_MIB_BASIC_RATE_SET_1000K		2
#define	RAY_MIB_BASIC_RATE_SET_1500K		3
#define	RAY_MIB_BASIC_RATE_SET_2000K		4
#define	RAY_MIB_BASIC_RATE_SET_DEFAULT		RAY_MIB_BASIC_RATE_SET_2000K

/*
 * IOCTL support
 */
struct ray_param_req {
	int		r_failcause;
	u_int8_t	r_paramid;
	u_int8_t	r_len;
	u_int8_t	r_data[256];
};
struct ray_stats_req {
	u_int64_t	rxoverflow;	/* Number of rx overflows	*/
	u_int64_t	rxcksum;	/* Number of checksum errors	*/
	u_int64_t	rxhcksum;	/* Number of header checksum errors */
	u_int8_t	rxnoise;	/* Average receiver level	*/
};
#define	RAY_FAILCAUSE_EIDRANGE	1
#define	RAY_FAILCAUSE_ELENGTH	2
/* device can possibly return up to 255 */
#define	RAY_FAILCAUSE_EDEVSTOP	256

/* Get a param the data is a ray_param_req structure */
#define	SIOCSRAYPARAM	SIOCSIFGENERIC
#define	SIOCGRAYPARAM	SIOCGIFGENERIC
/* Get the error counters the data is a ray_stats_req structure */
#define	SIOCGRAYSTATS	_IOWR('i', 201, struct ifreq)
#define SIOCGRAYSIGLEV  _IOWR('i', 202, struct ifreq)

#define RAY_NSIGLEVRECS 8
#define RAY_NSIGLEV 8
#define RAY_NANTENNA 8

struct ray_siglev {
	u_int8_t	rsl_host[ETHER_ADDR_LEN]; /* MAC address */
	u_int8_t	rsl_siglevs[RAY_NSIGLEV]; /* levels, newest in [0] */
	u_int8_t	rsl_antennas[RAY_NANTENNA]; /* best antenna */
	struct timeval	rsl_time; 		  /* time of last packet */
};
