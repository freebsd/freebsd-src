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
 * $Id: if_raymib.h,v 1.3 2000/03/21 14:27:46 dmlb Exp $
 *
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
} __attribute__((__packed__));

struct ray_mib_common_tail {
    u_int8_t	mib_noise_filter_gain;		/*00*/
    u_int8_t	mib_noise_limit_offset;		/*01*/
    u_int8_t	mib_rssi_thresh_offset;		/*02*/
    u_int8_t	mib_busy_thresh_offset;		/*03*/
    u_int8_t	mib_sync_thresh;		/*04*/
    u_int8_t	mib_test_mode;			/*05*/
    u_int8_t	mib_test_min_chan;		/*06*/
    u_int8_t	mib_test_max_chan;		/*07*/
} __attribute__((__packed__));

struct ray_mib_4 {
    struct ray_mib_common_head	mib_head;	/*00*/
    u_int8_t			mib_cw_max;	/*4b*/
    u_int8_t			mib_cw_min;	/*4c*/
    struct ray_mib_common_tail	mib_tail;	/*4d*/
} __attribute__((__packed__));

struct ray_mib_5 {
    struct ray_mib_common_head	mib_head;		/*00*/
    u_int8_t			mib_cw_max[2];		/*4b*/		/*02*/
    u_int8_t			mib_cw_min[2];		/*4d*/		/*02*/
    struct ray_mib_common_tail	mib_tail;		/*4f*/
    u_int8_t			mib_allow_probe_resp;	/*57*/
    u_int8_t			mib_privacy_must_start;	/*58*/
    u_int8_t			mib_privacy_can_join;	/*59*/
    u_int8_t			mib_basic_rate_set[8];	/*5a*/		/*08*/
} __attribute__((__packed__));

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

#ifdef KERNEL
#define	RAY_FAILCAUSE_WAITING	257
#endif

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

#define	RAY_MIB_LASTUSER			45
#define	RAY_MIB_MAX				62

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
	"FRAG_THRESH",			\
	"DWELL_TIME",			\
	"BEACON_PERIOD",		\
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
	"Desired PRIV_JOIN"		\
}

#define RAY_MIB_HELP_STRINGS {			\
	"0 Ad hoc, 1 Infrastructure",		\
	"0 Station, 1 Access Point",		\
	"",					\
	"0 Passive, 1 Active",			\
	"0 Off, 1 On",				\
	"",					\
	"FRAG_THRESH",				\
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
	"N/A",					\
	"N/A",					\
	"Desired DEF_TXRATE",			\
	"Desired ENCRYPT",			\
	"Desired NET_TYPE",			\
	"",					\
	"Desired PRIV_START",			\
	"Desired PRIV_JOIN"			\
}

/*
 * Sizes for each MIB element
 */
#define RAY_MIB_SIZES {					\
	1,	/* RAY_MIB_NET_TYPE */			\
	1,	/* RAY_MIB_AP_STATUS */			\
	IEEE80211_NWID_LEN,	/* RAY_MIB_SSID */	\
	1,	/* RAY_MIB_SCAN_MODE */			\
	1,	/* RAY_MIB_APM_MODE */			\
	ETHER_ADDR_LEN,/* RAY_MIB_MAC_ADDR */		\
	2,	/* RAY_MIB_FRAG_THRESH */		\
	2,	/* RAY_MIB_DWELL_TIME */		\
	2,	/* RAY_MIB_BEACON_PERIOD */		\
	1,	/* RAY_MIB_DTIM_INTERVAL */		\
	1,	/* RAY_MIB_MAX_RETRY */			\
	1,	/* RAY_MIB_ACK_TIMO */			\
	1,	/* RAY_MIB_SIFS */			\
	1,	/* RAY_MIB_DIFS */			\
	1,	/* RAY_MIB_PIFS */			\
	2,	/* RAY_MIB_RTS_THRESH */		\
	2,	/* RAY_MIB_SCAN_DWELL */		\
	2,	/* RAY_MIB_SCAN_MAX_DWELL */		\
	1,	/* RAY_MIB_ASSOC_TIMO */		\
	1,	/* RAY_MIB_ADHOC_SCAN_CYCLE */		\
	1,	/* RAY_MIB_INFRA_SCAN_CYCLE */		\
	1,	/* RAY_MIB_INFRA_SUPER_SCAN_CYCLE */	\
	1,	/* RAY_MIB_PROMISC */			\
	2,	/* RAY_MIB_UNIQ_WORD */			\
	1,	/* RAY_MIB_SLOT_TIME */			\
	1,	/* RAY_MIB_ROAM_LOW_SNR_THRESH */	\
	1,	/* RAY_MIB_LOW_SNR_COUNT */		\
	1,	/* RAY_MIB_INFRA_MISSED_BEACON_COUNT */	\
	1,	/* RAY_MIB_ADHOC_MISSED_BEACON_COUNT */	\
	1,	/* RAY_MIB_COUNTRY_CODE */		\
	1,	/* RAY_MIB_HOP_SEQ */			\
	1,	/* RAY_MIB_HOP_SEQ_LEN */		\
	2,	/* RAY_MIB_CW_MAX */			\
	2,	/* RAY_MIB_CW_MIN */			\
	1,	/* RAY_MIB_NOISE_FILTER_GAIN */		\
	1,	/* RAY_MIB_NOISE_LIMIT_OFFSET */	\
	1,	/* RAY_MIB_RSSI_THRESH_OFFSET */	\
	1,	/* RAY_MIB_BUSY_THRESH_OFFSET */	\
	1,	/* RAY_MIB_SYNC_THRESH */		\
	1,	/* RAY_MIB_TEST_MODE */			\
	1,	/* RAY_MIB_TEST_MIN_CHAN */		\
	1,	/* RAY_MIB_TEST_MAX_CHAN */		\
	1,	/* RAY_MIB_ALLOW_PROBE_RESP */		\
	1,	/* RAY_MIB_PRIVACY_MUST_START */	\
	1,	/* RAY_MIB_PRIVACY_CAN_JOIN */		\
	8,	/* RAY_MIB_BASIC_RATE_SET */		\
	1,	/* RAY_MIB_VERSION */			\
	ETHER_ADDR_LEN,	/* RAY_MIB_CUR_BSSID */		\
	1,	/* RAY_MIB_CUR_INITED */		\
	1,	/* RAY_MIB_CUR_DEF_TXRATE */		\
	1,	/* RAY_MIB_CUR_ENCRYPT */		\
	1,	/* RAY_MIB_CUR_NET_TYPE */		\
	IEEE80211_NWID_LEN, /* RAY_MIB_CUR_SSID */	\
	1,	/* RAY_MIB_CUR_PRIV_START */		\
	1,	/* RAY_MIB_CUR_PRIV_JOIN */		\
	ETHER_ADDR_LEN,	/* RAY_MIB_DES_BSSID */		\
	1,	/* RAY_MIB_DES_INITED */		\
	1,	/* RAY_MIB_DES_DEF_TXRATE */		\
	1,	/* RAY_MIB_DES_ENCRYPT */		\
	1,	/* RAY_MIB_DES_NET_TYPE */		\
	IEEE80211_NWID_LEN, /* RAY_MIB_DES_SSID */	\
	1,	/* RAY_MIB_DES_PRIV_START */		\
	1 	/* RAY_MIB_DES_PRIV_JOIN */		\
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
 * discussion with 802.11 knowledgable people at Symbionics (i.e. me,
 * aps, ifo, hjl).
 *
 * V4 and V5 refer to settings for version 4 and version 5 of
 * the firmware.
 *
 */

/*
 * ADHOC		- I've not got an access point
 */
#define	RAY_MIB_NET_TYPE_ADHOC			0x00
#define	RAY_MIB_NET_TYPE_INFRA			0x01
#define	RAY_MIB_NET_TYPE_DEFAULT		RAY_MIB_NET_TYPE_ADHOC

/*
 * TERMINAL		- but we might play with using the card as an AP
 */
#define	RAY_MIB_AP_STATUS_TERMINAL		0x00
#define	RAY_MIB_AP_STATUS_AP			0x01
#define	RAY_MIB_AP_STATUS_DEFAULT		RAY_MIB_AP_STATUS_TERMINAL

/*
 * 			- windows setting comes from the Aviator software v1.1
 */
#define RAY_MIB_SSID_WINDOWS			"NETWORK_NAME"
#define RAY_MIB_SSID_NOT_WINDOWS		"WIRELESS_NETWORK"
#define RAY_MIB_SSID_DEFAULT			RAY_MIB_SSID_WINDOWS

#define	RAY_MIB_SCAN_MODE_PASSIVE		0x00
#define	RAY_MIB_SCAN_MODE_ACTIVE		0x01
#define	RAY_MIB_SCAN_MODE_DEFAULT		RAY_MIB_SCAN_MODE_ACTIVE

/*
 * NONE			- power saving only works with access points
 */
#define	RAY_MIB_APM_MODE_NONE			0x00
#define	RAY_MIB_APM_MODE_POWERSAVE		0x01
#define	RAY_MIB_APM_MODE_DEFAULT		RAY_MIB_APM_MODE_NONE

/*
 * Linux.h	0x0200
 * Linux.c-V4	0x7fff
 * Linux.c-V5	0x7fff
 * NetBSD.c	0x7fff	- disabled
 * Symb		0xXXXX	- you really should fragment but getting it wrong
 *			  crucifies the performance
 */
#define RAY_MIB_FRAG_THRESH_DISABLE		0x7fff
#define RAY_MIB_FRAG_THRESH_DEFAULT		RAY_MIB_FRAG_THRESH_DISABLE

/*
 * Linux.h		- 16k * 2**n, n=0-4 in Kus
 * Linux.c-V4	0x0200
 * Linux.c-V5	0x0080	- 128 Kus
 * NetBSD-V4	0x0200	- from Linux
 * NetBSD-V4	0x0400	- "divined"
 * NetBSD-V5	0x0080
 * Symb-V4	0xXXXX	- 802.11 dwell time is XXX Kus
 * Symb-V5	0xXXXX	- 802.11 dwell time is XXX Kus
 * XXX			- see init_startup_params in Linux.c
 */
#define	RAY_MIB_DWELL_TIME_V4			0x0400
#define	RAY_MIB_DWELL_TIME_V5			0x0080

/*
 * Linux.h		- n * a_hop_time  in Kus
 * Linux.c-V4	0x0001
 * Linux.c-V5	0x0100	- 256 Kus
 * NetBSD-V4	0x0001	- from Linux
 * NetBSD-V4	0x0000	- "divined"
 * NetBSD-V5	0x0100
 * Symb-V4	0x0001	- best performance is one beacon each dwell XXX
 * Symb-V5	0x0080	- best performance is one beacon each dwell XXX
 * XXX			- see init_startup_params in Linux.c
 */
#define	RAY_MIB_BEACON_PERIOD_V4		0x01
#define	RAY_MIB_BEACON_PERIOD_V5		RAY_MIB_DWELL_TIME_V5

/*
 * Linux.h		- in beacons
 * Linux.c	0x01
 * NetBSD	0x01
 * Symb		0xXX	- need to find out what DTIM is
 */
#define	RAY_MIB_DTIM_INTERVAL_DEFAULT		0x01

/*
 * Linux.c	0x07
 * NetBSD	0x01	- documented default for 5/6
 * NetBSD	0x07	- from Linux
 * NetBSD	0x03	- "divined"
 * Symb		0xXX	- 7 retries seems okay but check with APS
 */
#define	RAY_MIB_MAX_RETRY_DEFAULT		0x07

/*
 * Linux.c	0xa3
 * NetBSD	0x86	- documented default for 5/6
 * NetBSD	0xa3	- from Linux
 * NetBSD	0xa3	- "divined"
 * Symb		0xXX	- this must be a 802.11 defined setting?
 */
#define	RAY_MIB_ACK_TIMO_DEFAULT		0xa3

/*
 * Linux.c	0x1d
 * NetBSD	0x1c	- documented default for 5/6
 * NetBSD	0x1d	- from Linux
 * NetBSD	0x1d	- "divined"
 * Symb		0xXX	- default SIFS for 802.11
 */
#define	RAY_MIB_SIFS_DEFAULT			0x1d

/*
 * Linux.c	0x82
 * NetBSD	0x82	- documented default for 5/6
 * NetBSD	0x82	- from Linux
 * Symb		0xXX	- default DIFS for 802.11
 */
#define	RAY_MIB_DIFS_DEFAULT			0x82

/*
 * Linux.c-V4	0xce
 * Linux.c-V5	0x4e
 * NetBSD	0x00	- documented default for 5/6
 * NetBSD-V4	0xce	- from Linux
 * NetBSD-V5	0x4e	- from Linux
 * Symb		0xXX	- default PIFS for 802.11
 */
#define	RAY_MIB_PIFS_V4				0xce
#define	RAY_MIB_PIFS_V5				0x4e

/*
 * Linux.c	0x7fff
 * NetBSD	0x7fff	- disabled
 * Symb		0xXXXX	- need to set this realistically to get CTS/RTS mode
 *			  working right
 */
#define	RAY_MIB_RTS_THRESH_DISABLE		0x7fff
#define	RAY_MIB_RTS_THRESH_DEFAULT		RAY_MIB_RTS_THRESH_DISABLE

/*
 * Linux.c-V4	0xfb1e
 * Linix.c-V5	0x04e2
 * NetBSD-V4	0xfb1e
 * NetBSD-V5	0x04e2
 * Symb		0xXXXX	- this might be the time to dwell on a channel
 *			  whilst scanning for the n/w. In that case it should
 *			  be tied to the dwell time above.
 *			  V5 numbers could be Kus,
 *			    0x04e2Kus = 1250*1024us = 1.28 seconds
 */
#define	RAY_MIB_SCAN_DWELL_V4			0xfb1e
#define	RAY_MIB_SCAN_DWELL_V5			0x04e2

/*
 * Linux.c-V4	0xc75c
 * Linix.c-V5	0x38a4
 * NetBSD-V4	0xc75c
 * NetBSD-V5	0x38a4
 * Symb		0xXXXX	- see above - this may be total time before giving up
 *			  but 0x38a4 Kus is about 14 seconds
 *			  i.e. not 79*SCAN_DWELL
 */
#define	RAY_MIB_SCAN_MAX_DWELL_V4		0xc75c
#define	RAY_MIB_SCAN_MAX_DWELL_V5		0x38a4

/*
 * Linix.c	0x05
 * NetBSD	0x05
 * Symb		0xXX	- can't be in Kus too short
 */
#define	RAY_MIB_ASSOC_TIMO_DEFAULT		0x05

/*
 * Linix.c-V4	0x04
 * Linux.c-V5	0x08
 * NetBSD-V4	0x04	- Linux
 * NetBSD-V4	0x08	- "divined"
 * NetBSD-V5	0x08
 * Symb		0xXX	- hmm maybe this ties in with the DWELL_SCAN above?
 */
#define	RAY_MIB_ADHOC_SCAN_CYCLE_DEFAULT	0x08

/*
 * Linix.c	0x02
 * NetBSD-V4	0x02	- Linux
 * NetBSD-V4	0x01	- "divined"
 * NetBSD-V5	0x02
 * Symb		0xXX	- hmm maybe this ties in with the DWELL_SCAN above?
 */
#define	RAY_MIB_INFRA_SCAN_CYCLE_DEFAULT	0x02

/*
 * Linix.c-V4	0x04
 * Linux.c-V5	0x08
 * NetBSD-V4	0x04	- Linux
 * NetBSD-V4	0x18	- "divined"
 * NetBSD-V5	0x08
 * Symb		0xXX	- hmm maybe this ties in with the DWELL_SCAN above?
 */
#define	RAY_MIB_INFRA_SUPER_SCAN_CYCLE_DEFAULT	0x08

#define	RAY_MIB_PROMISC_DEFAULT			0x00

#define	RAY_MIB_UNIQ_WORD_DEFAULT		0x0cbd

/*
 * Linux.c-V4	0x4e
 * Linix.c-V5	0x32
 * NetBSD-V4	0x4e	- Linux
 * NetBSD-V4	0x18	- "divined"
 * NetBSD-V5	0x32	- mentions spec. is 50us i.e. 0x32
 * Symb		0xXX	- wtf 0x4e = 78
 */
#define	RAY_MIB_SLOT_TIME_V4			0x4e
#define	RAY_MIB_SLOT_TIME_V5			0x32

/*
 * Linux.c	0xff
 * NetBSD-V4	0xff	- Linux
 * NetBSD-V4	0x30	- "divined"
 * NetBSD-V5	0xff	- disabled
 * NetBSD.h		- if below this inc count
 * Symb		0xXX	- hmm is 0xff really disabled? need this to work
 */
#define	RAY_MIB_ROAM_LOW_SNR_THRESH_DISABLED	0xff
#define	RAY_MIB_ROAM_LOW_SNR_THRESH_DEFAULT	RAY_MIB_ROAM_LOW_SNR_THRESH_DISABLED	

/*
 * Linux.c	0xff
 * NetBSD	0x07	- "divined - check" and marked as disabled
 * NetBSD	0xff	- disabled
 * NetBSD.h		- roam after cnt below thrsh
 * Symb		0xXX	- hmm is 0xff really disabled? need this to work
 */
#define	RAY_MIB_LOW_SNR_COUNT_DISABLED		0xff
#define	RAY_MIB_LOW_SNR_COUNT_DEFAULT		RAY_MIB_LOW_SNR_COUNT_DISABLED

/*
 * Linux.c	0x05
 * NetBSD	0x02	- documented default for 5/6
 * NetBSD	0x05	- Linux
 * NetBSD	0x07	- "divined - check, looks fishy"
 * Symb		0xXX	- 5 missed beacons is probably okay
 */
#define	RAY_MIB_INFRA_MISSED_BEACON_COUNT_DEFAULT	0x05

/*
 * Linux.c	0xff
 * NetBSD	0xff
 * Symb		0xXX	- so what happens in adhoc if the beacon is missed?
 *			  do we create our own beacon
 */
#define	RAY_MIB_ADHOC_MISSED_BEACON_COUNT_DISABLED	0xff
#define	RAY_MIB_ADHOC_MISSED_BEACON_COUNT_DEFAULT	RAY_MIB_ADHOC_MISSED_BEACON_COUNT_DISABLED

#define	RAY_MIB_COUNTRY_CODE_USA		0x1
#define	RAY_MIB_COUNTRY_CODE_EUROPE		0x2
#define	RAY_MIB_COUNTRY_CODE_JAPAN		0x3
#define	RAY_MIB_COUNTRY_CODE_KOREA		0x4
#define	RAY_MIB_COUNTRY_CODE_SPAIN		0x5
#define	RAY_MIB_COUNTRY_CODE_FRANCE		0x6
#define	RAY_MIB_COUNTRY_CODE_ISRAEL		0x7
#define	RAY_MIB_COUNTRY_CODE_AUSTRALIA		0x8
#define	RAY_MIB_COUNTRY_CODE_JAPAN_TEST		0x9
#define	RAY_MIB_COUNTRY_CODE_MAX		0xa
#define	RAY_MIB_COUNTRY_CODE_DEFAULT		RAY_MIB_COUNTRY_CODE_USA

/*
 * NetBSD.h		- no longer supported
 */
#define	RAY_MIB_HOP_SEQ_DEFAULT			0x0b

/*
 * Linux.c-V4	0x4e
 * Linix.c-V5	0x4f
 * NetBSD-V4	0x4e
 * NetBSD-V5	0x4f
 * Symb		0xXX	- 0x4e = 78 so is it a cock up?
 */
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

#define	RAY_MIB_BASIC_RATE_SET_500K		1
#define	RAY_MIB_BASIC_RATE_SET_1000K		2
#define	RAY_MIB_BASIC_RATE_SET_1500K		3
#define	RAY_MIB_BASIC_RATE_SET_2000K		4
#define	RAY_MIB_BASIC_RATE_SET_MAX		5
#define	RAY_MIB_BASIC_RATE_SET_DEFAULT		RAY_MIB_BASIC_RATE_SET_2000K
