/*
 * hostapd / Initialization and configuration
 * Host AP kernel driver
 * Copyright (c) 2002-2006, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#ifndef HOSTAPD_H
#define HOSTAPD_H

#include "common.h"
#include "ap.h"

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif
#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif
#ifndef ETH_P_ALL
#define ETH_P_ALL 0x0003
#endif
#ifndef ETH_P_PAE
#define ETH_P_PAE 0x888E /* Port Access Entity (IEEE 802.1X) */
#endif /* ETH_P_PAE */

#ifndef BIT
#define BIT(x) (1 << (x))
#endif

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

#include "config.h"

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif /* _MSC_VER */

#define MAX_VLAN_ID 4094

struct ieee8023_hdr {
	u8 dest[6];
	u8 src[6];
	u16 ethertype;
} STRUCT_PACKED;


struct ieee80211_hdr {
	u16 frame_control;
	u16 duration_id;
	u8 addr1[6];
	u8 addr2[6];
	u8 addr3[6];
	u16 seq_ctrl;
	/* followed by 'u8 addr4[6];' if ToDS and FromDS is set in data frame
	 */
} STRUCT_PACKED;

#ifdef _MSC_VER
#pragma pack(pop)
#endif /* _MSC_VER */

#define IEEE80211_DA_FROMDS addr1
#define IEEE80211_BSSID_FROMDS addr2
#define IEEE80211_SA_FROMDS addr3

#define IEEE80211_HDRLEN (sizeof(struct ieee80211_hdr))

#define IEEE80211_FC(type, stype) host_to_le16((type << 2) | (stype << 4))

/* MTU to be set for the wlan#ap device; this is mainly needed for IEEE 802.1X
 * frames that might be longer than normal default MTU and they are not
 * fragmented */
#define HOSTAPD_MTU 2290

extern unsigned char rfc1042_header[6];

struct hostap_sta_driver_data {
	unsigned long rx_packets, tx_packets, rx_bytes, tx_bytes;
	unsigned long current_tx_rate;
	unsigned long inactive_msec;
	unsigned long flags;
	unsigned long num_ps_buf_frames;
	unsigned long tx_retry_failed;
	unsigned long tx_retry_count;
	int last_rssi;
	int last_ack_rssi;
};

struct driver_ops;
struct wpa_ctrl_dst;
struct radius_server_data;

#ifdef CONFIG_FULL_DYNAMIC_VLAN
struct full_dynamic_vlan;
#endif /* CONFIG_FULL_DYNAMIC_VLAN */

/**
 * struct hostapd_data - hostapd per-BSS data structure
 */
struct hostapd_data {
	struct hostapd_iface *iface;
	struct hostapd_config *iconf;
	struct hostapd_bss_config *conf;
	int interface_added; /* virtual interface added for this BSS */

	u8 own_addr[ETH_ALEN];

	int num_sta; /* number of entries in sta_list */
	struct sta_info *sta_list; /* STA info list head */
	struct sta_info *sta_hash[STA_HASH_SIZE];

	/* pointers to STA info; based on allocated AID or NULL if AID free
	 * AID is in the range 1-2007, so sta_aid[0] corresponders to AID 1
	 * and so on
	 */
	struct sta_info *sta_aid[MAX_AID_TABLE_SIZE];

	struct driver_ops *driver;

	u8 *default_wep_key;
	u8 default_wep_key_idx;

	struct radius_client_data *radius;
	int radius_client_reconfigured;
	u32 acct_session_id_hi, acct_session_id_lo;

	struct iapp_data *iapp;

	enum { DO_NOT_ASSOC = 0, WAIT_BEACON, AUTHENTICATE, ASSOCIATE,
	       ASSOCIATED } assoc_ap_state;
	char assoc_ap_ssid[33];
	int assoc_ap_ssid_len;
	u16 assoc_ap_aid;

	struct hostapd_cached_radius_acl *acl_cache;
	struct hostapd_acl_query_data *acl_queries;

	struct wpa_authenticator *wpa_auth;

	struct rsn_preauth_interface *preauth_iface;
	time_t michael_mic_failure;
	int michael_mic_failures;
	int tkip_countermeasures;

	int ctrl_sock;
	struct wpa_ctrl_dst *ctrl_dst;

	void *ssl_ctx;
	void *eap_sim_db_priv;
	struct radius_server_data *radius_srv;

	int parameter_set_count;

#ifdef CONFIG_FULL_DYNAMIC_VLAN
	struct full_dynamic_vlan *full_dynamic_vlan;
#endif /* CONFIG_FULL_DYNAMIC_VLAN */
};


/**
 * hostapd_iface_cb - Generic callback type for per-iface asynchronous requests
 * @iface: the interface the event occured on.
 * @status: 0 if the request succeeded; -1 if the request failed.
 */
typedef void (*hostapd_iface_cb)(struct hostapd_iface *iface, int status);


struct hostapd_config_change;

/**
 * struct hostapd_iface - hostapd per-interface data structure
 */
struct hostapd_iface {
	char *config_fname;
	struct hostapd_config *conf;

	hostapd_iface_cb setup_cb;

	size_t num_bss;
	struct hostapd_data **bss;

	int num_ap; /* number of entries in ap_list */
	struct ap_info *ap_list; /* AP info list head */
	struct ap_info *ap_hash[STA_HASH_SIZE];
	struct ap_info *ap_iter_list;

	struct hostapd_hw_modes *hw_features;
	int num_hw_features;
	struct hostapd_hw_modes *current_mode;
	/* Rates that are currently used (i.e., filtered copy of
	 * current_mode->channels */
	int num_rates;
	struct hostapd_rate_data *current_rates;
	hostapd_iface_cb hw_mode_sel_cb;

	u16 hw_flags;

	/* Number of associated Non-ERP stations (i.e., stations using 802.11b
	 * in 802.11g BSS) */
	int num_sta_non_erp;

	/* Number of associated stations that do not support Short Slot Time */
	int num_sta_no_short_slot_time;

	/* Number of associated stations that do not support Short Preamble */
	int num_sta_no_short_preamble;

	int olbc; /* Overlapping Legacy BSS Condition */

	int dfs_enable;
	u8 pwr_const;
	unsigned int tx_power;
	unsigned int sta_max_power;

	unsigned int channel_switch;

	struct hostapd_config_change *change;
	hostapd_iface_cb reload_iface_cb;
	hostapd_iface_cb config_reload_cb;
};

void hostapd_new_assoc_sta(struct hostapd_data *hapd, struct sta_info *sta,
			   int reassoc);
void hostapd_logger(struct hostapd_data *hapd, const u8 *addr,
		    unsigned int module, int level, const char *fmt,
		    ...) PRINTF_FORMAT(5, 6);


#ifndef _MSC_VER
#define HOSTAPD_DEBUG(level, args...) \
do { \
	if (hapd->conf == NULL || hapd->conf->debug >= (level)) \
		printf(args); \
} while (0)
#endif /* _MSC_VER */

#define HOSTAPD_DEBUG_COND(level) (hapd->conf->debug >= (level))

const char * hostapd_ip_txt(const struct hostapd_ip_addr *addr, char *buf,
			    size_t buflen);
int hostapd_ip_diff(struct hostapd_ip_addr *a, struct hostapd_ip_addr *b);

#endif /* HOSTAPD_H */
