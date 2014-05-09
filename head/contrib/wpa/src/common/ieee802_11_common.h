/*
 * IEEE 802.11 Common routines
 * Copyright (c) 2002-2012, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef IEEE802_11_COMMON_H
#define IEEE802_11_COMMON_H

/* Parsed Information Elements */
struct ieee802_11_elems {
	const u8 *ssid;
	const u8 *supp_rates;
	const u8 *fh_params;
	const u8 *ds_params;
	const u8 *cf_params;
	const u8 *tim;
	const u8 *ibss_params;
	const u8 *challenge;
	const u8 *erp_info;
	const u8 *ext_supp_rates;
	const u8 *wpa_ie;
	const u8 *rsn_ie;
	const u8 *wmm; /* WMM Information or Parameter Element */
	const u8 *wmm_tspec;
	const u8 *wps_ie;
	const u8 *power_cap;
	const u8 *supp_channels;
	const u8 *mdie;
	const u8 *ftie;
	const u8 *timeout_int;
	const u8 *ht_capabilities;
	const u8 *ht_operation;
	const u8 *vht_capabilities;
	const u8 *vht_operation;
	const u8 *vendor_ht_cap;
	const u8 *p2p;
	const u8 *wfd;
	const u8 *link_id;
	const u8 *interworking;
	const u8 *hs20;
	const u8 *ext_capab;
	const u8 *bss_max_idle_period;
	const u8 *ssid_list;

	u8 ssid_len;
	u8 supp_rates_len;
	u8 fh_params_len;
	u8 ds_params_len;
	u8 cf_params_len;
	u8 tim_len;
	u8 ibss_params_len;
	u8 challenge_len;
	u8 erp_info_len;
	u8 ext_supp_rates_len;
	u8 wpa_ie_len;
	u8 rsn_ie_len;
	u8 wmm_len; /* 7 = WMM Information; 24 = WMM Parameter */
	u8 wmm_tspec_len;
	u8 wps_ie_len;
	u8 power_cap_len;
	u8 supp_channels_len;
	u8 mdie_len;
	u8 ftie_len;
	u8 timeout_int_len;
	u8 ht_capabilities_len;
	u8 ht_operation_len;
	u8 vht_capabilities_len;
	u8 vht_operation_len;
	u8 vendor_ht_cap_len;
	u8 p2p_len;
	u8 wfd_len;
	u8 interworking_len;
	u8 hs20_len;
	u8 ext_capab_len;
	u8 ssid_list_len;
};

typedef enum { ParseOK = 0, ParseUnknown = 1, ParseFailed = -1 } ParseRes;

ParseRes ieee802_11_parse_elems(const u8 *start, size_t len,
				struct ieee802_11_elems *elems,
				int show_errors);
int ieee802_11_ie_count(const u8 *ies, size_t ies_len);
struct wpabuf * ieee802_11_vendor_ie_concat(const u8 *ies, size_t ies_len,
					    u32 oui_type);
struct ieee80211_hdr;
const u8 * get_hdr_bssid(const struct ieee80211_hdr *hdr, size_t len);

struct hostapd_wmm_ac_params {
	int cwmin;
	int cwmax;
	int aifs;
	int txop_limit; /* in units of 32us */
	int admission_control_mandatory;
};

int hostapd_config_wmm_ac(struct hostapd_wmm_ac_params wmm_ac_params[],
			  const char *name, const char *val);

#endif /* IEEE802_11_COMMON_H */
