/*
 * WPA Supplicant - Client mode MLME
 * Copyright (c) 2003-2006, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2004, Instant802 Networks, Inc.
 * Copyright (c) 2005-2006, Devicescape Software, Inc.
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

#include "includes.h"

#include "common.h"
#include "eloop.h"
#include "config.h"
#include "wpa_supplicant.h"
#include "wpa_supplicant_i.h"
#include "wpa.h"
#include "os.h"
#include "l2_packet.h"
#include "driver.h"
#include "mlme.h"


/* Timeouts and intervals in milliseconds */
#define IEEE80211_AUTH_TIMEOUT (200)
#define IEEE80211_AUTH_MAX_TRIES 3
#define IEEE80211_ASSOC_TIMEOUT (200)
#define IEEE80211_ASSOC_MAX_TRIES 3
#define IEEE80211_MONITORING_INTERVAL (2000)
#define IEEE80211_PROBE_INTERVAL (60000)
#define IEEE80211_RETRY_AUTH_INTERVAL (1000)
#define IEEE80211_SCAN_INTERVAL (2000)
#define IEEE80211_SCAN_INTERVAL_SLOW (15000)
#define IEEE80211_IBSS_JOIN_TIMEOUT (20000)

#define IEEE80211_PROBE_DELAY (33)
#define IEEE80211_CHANNEL_TIME (33)
#define IEEE80211_PASSIVE_CHANNEL_TIME (200)
#define IEEE80211_SCAN_RESULT_EXPIRE (10000)
#define IEEE80211_IBSS_MERGE_INTERVAL (30000)
#define IEEE80211_IBSS_INACTIVITY_LIMIT (60000)

#define IEEE80211_IBSS_MAX_STA_ENTRIES 128


/* Information Element IDs */
#define WLAN_EID_SSID 0
#define WLAN_EID_SUPP_RATES 1
#define WLAN_EID_FH_PARAMS 2
#define WLAN_EID_DS_PARAMS 3
#define WLAN_EID_CF_PARAMS 4
#define WLAN_EID_TIM 5
#define WLAN_EID_IBSS_PARAMS 6
#define WLAN_EID_COUNTRY 7
#define WLAN_EID_CHALLENGE 16
/* EIDs defined as part fo 11h - starts */
#define WLAN_EID_PWR_CONSTRAINT	32
#define WLAN_EID_PWR_CAPABILITY	33
#define WLAN_EID_TPC_REQUEST	34
#define WLAN_EID_TPC_REPORT	35
#define WLAN_EID_SUPPORTED_CHANNELS	36
#define WLAN_EID_CHANNEL_SWITCH	37
#define WLAN_EID_MEASURE_REQUEST	38
#define WLAN_EID_MEASURE_REPORT	39
#define WLAN_EID_QUITE	40
#define WLAN_EID_IBSS_DFS	41
/* EIDs defined as part fo 11h - ends */
#define WLAN_EID_ERP_INFO 42
#define WLAN_EID_RSN 48
#define WLAN_EID_EXT_SUPP_RATES 50
#define WLAN_EID_VENDOR_SPECIFIC 221


#ifdef _MSC_VER
#pragma pack(push, 1)
#endif /* _MSC_VER */

struct ieee80211_mgmt {
	u16 frame_control;
	u16 duration;
	u8 da[6];
	u8 sa[6];
	u8 bssid[6];
	u16 seq_ctrl;
	union {
		struct {
			u16 auth_alg;
			u16 auth_transaction;
			u16 status_code;
			/* possibly followed by Challenge text */
			u8 variable[0];
		} STRUCT_PACKED auth;
		struct {
			u16 reason_code;
		} STRUCT_PACKED deauth;
		struct {
			u16 capab_info;
			u16 listen_interval;
			/* followed by SSID and Supported rates */
			u8 variable[0];
		} STRUCT_PACKED assoc_req;
		struct {
			u16 capab_info;
			u16 status_code;
			u16 aid;
			/* followed by Supported rates */
			u8 variable[0];
		} STRUCT_PACKED assoc_resp, reassoc_resp;
		struct {
			u16 capab_info;
			u16 listen_interval;
			u8 current_ap[6];
			/* followed by SSID and Supported rates */
			u8 variable[0];
		} STRUCT_PACKED reassoc_req;
		struct {
			u16 reason_code;
		} STRUCT_PACKED disassoc;
		struct {
			u8 timestamp[8];
			u16 beacon_int;
			u16 capab_info;
			/* followed by some of SSID, Supported rates,
			 * FH Params, DS Params, CF Params, IBSS Params, TIM */
			u8 variable[0];
		} STRUCT_PACKED beacon;
		struct {
			/* only variable items: SSID, Supported rates */
			u8 variable[0];
		} STRUCT_PACKED probe_req;
		struct {
			u8 timestamp[8];
			u16 beacon_int;
			u16 capab_info;
			/* followed by some of SSID, Supported rates,
			 * FH Params, DS Params, CF Params, IBSS Params */
			u8 variable[0];
		} STRUCT_PACKED probe_resp;
		struct {
			u8 category;
			union {
				struct {
					u8 action_code;
					u8 dialog_token;
					u8 status_code;
					u8 variable[0];
				} STRUCT_PACKED wme_action;
				struct{
					u8 action_code;
					u8 element_id;
					u8 length;
					u8 switch_mode;
					u8 new_chan;
					u8 switch_count;
				} __attribute__((packed)) chan_switch;
			} u;
		} STRUCT_PACKED action;
	} u;
} STRUCT_PACKED;

#ifdef _MSC_VER
#pragma pack(pop)
#endif /* _MSC_VER */

/* Authentication algorithms */
#define WLAN_AUTH_OPEN 0
#define WLAN_AUTH_SHARED_KEY 1
#define WLAN_AUTH_LEAP 128

#define WLAN_AUTH_CHALLENGE_LEN 128

#define WLAN_CAPABILITY_ESS BIT(0)
#define WLAN_CAPABILITY_IBSS BIT(1)
#define WLAN_CAPABILITY_CF_POLLABLE BIT(2)
#define WLAN_CAPABILITY_CF_POLL_REQUEST BIT(3)
#define WLAN_CAPABILITY_PRIVACY BIT(4)
#define WLAN_CAPABILITY_SHORT_PREAMBLE BIT(5)
#define WLAN_CAPABILITY_PBCC BIT(6)
#define WLAN_CAPABILITY_CHANNEL_AGILITY BIT(7)
/* 802.11h */
#define WLAN_CAPABILITY_SPECTRUM_MGMT BIT(8)
#define WLAN_CAPABILITY_SHORT_SLOT_TIME BIT(10)
#define WLAN_CAPABILITY_DSSS_OFDM BIT(13)

/* Status codes */
#define WLAN_STATUS_SUCCESS 0
#define WLAN_STATUS_UNSPECIFIED_FAILURE 1
#define WLAN_STATUS_CAPS_UNSUPPORTED 10
#define WLAN_STATUS_REASSOC_NO_ASSOC 11
#define WLAN_STATUS_ASSOC_DENIED_UNSPEC 12
#define WLAN_STATUS_NOT_SUPPORTED_AUTH_ALG 13
#define WLAN_STATUS_UNKNOWN_AUTH_TRANSACTION 14
#define WLAN_STATUS_CHALLENGE_FAIL 15
#define WLAN_STATUS_AUTH_TIMEOUT 16
#define WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA 17
#define WLAN_STATUS_ASSOC_DENIED_RATES 18
/* 802.11b */
#define WLAN_STATUS_ASSOC_DENIED_NOSHORT 19
#define WLAN_STATUS_ASSOC_DENIED_NOPBCC 20
#define WLAN_STATUS_ASSOC_DENIED_NOAGILITY 21
/* 802.11h */
#define WLAN_STATUS_SPEC_MGMT_REQUIRED 22
#define WLAN_STATUS_PWR_CAPABILITY_NOT_VALID 23
#define WLAN_STATUS_SUPPORTED_CHANNEL_NOT_VALID 24
/* 802.11g */
#define WLAN_STATUS_ASSOC_DENOED_NO_SHORT_SLOT_TIME 25
#define WLAN_STATUS_ASSOC_DENOED_NO_ER_PBCC 26
#define WLAN_STATUS_ASSOC_DENOED_NO_DSSS_OFDM 27


/* Reason codes */
#define WLAN_REASON_UNSPECIFIED 1
#define WLAN_REASON_PREV_AUTH_NOT_VALID 2
#define WLAN_REASON_DEAUTH_LEAVING 3
#define WLAN_REASON_DISASSOC_DUE_TO_INACTIVITY 4
#define WLAN_REASON_DISASSOC_AP_BUSY 5
#define WLAN_REASON_CLASS2_FRAME_FROM_NONAUTH_STA 6
#define WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA 7
#define WLAN_REASON_DISASSOC_STA_HAS_LEFT 8
#define WLAN_REASON_STA_REQ_ASSOC_WITHOUT_AUTH 9
/* 802.11h */
#define WLAN_REASON_PWR_CAPABILITY_NOT_VALID 10
#define WLAN_REASON_SUPPORTED_CHANNEL_NOT_VALID 11

#define WLAN_REASON_MIC_FAILURE 14


#define WLAN_FC_PVER		0x0003
#define WLAN_FC_TODS		0x0100
#define WLAN_FC_FROMDS		0x0200
#define WLAN_FC_MOREFRAG	0x0400
#define WLAN_FC_RETRY		0x0800
#define WLAN_FC_PWRMGT		0x1000
#define WLAN_FC_MOREDATA	0x2000
#define WLAN_FC_ISWEP		0x4000
#define WLAN_FC_ORDER		0x8000

#define WLAN_FC_GET_TYPE(fc)	(((fc) & 0x000c) >> 2)
#define WLAN_FC_GET_STYPE(fc)	(((fc) & 0x00f0) >> 4)


#define IEEE80211_FC(type, stype) host_to_le16((type << 2) | (stype << 4))

#define WLAN_FC_TYPE_MGMT	0
#define WLAN_FC_TYPE_CTRL	1
#define WLAN_FC_TYPE_DATA	2

/* management */
#define WLAN_FC_STYPE_ASSOC_REQ		0
#define WLAN_FC_STYPE_ASSOC_RESP	1
#define WLAN_FC_STYPE_REASSOC_REQ	2
#define WLAN_FC_STYPE_REASSOC_RESP	3
#define WLAN_FC_STYPE_PROBE_REQ		4
#define WLAN_FC_STYPE_PROBE_RESP	5
#define WLAN_FC_STYPE_BEACON		8
#define WLAN_FC_STYPE_ATIM		9
#define WLAN_FC_STYPE_DISASSOC		10
#define WLAN_FC_STYPE_AUTH		11
#define WLAN_FC_STYPE_DEAUTH		12
#define WLAN_FC_STYPE_ACTION		13


#define ERP_INFO_USE_PROTECTION BIT(1)


struct ieee80211_sta_bss {
	struct ieee80211_sta_bss *next;
	struct ieee80211_sta_bss *hnext;

	u8 bssid[ETH_ALEN];
	u8 ssid[MAX_SSID_LEN];
	size_t ssid_len;
	u16 capability; /* host byte order */
	int hw_mode;
	int channel;
	int freq;
	int rssi;
	u8 *wpa_ie;
	size_t wpa_ie_len;
	u8 *rsn_ie;
	size_t rsn_ie_len;
	u8 *wmm_ie;
	size_t wmm_ie_len;
#define IEEE80211_MAX_SUPP_RATES 32
	u8 supp_rates[IEEE80211_MAX_SUPP_RATES];
	size_t supp_rates_len;
	int beacon_int;
	u64 timestamp;

	int probe_resp;
	struct os_time last_update;
};


static void ieee80211_send_probe_req(struct wpa_supplicant *wpa_s,
				     const u8 *dst,
				     const u8 *ssid, size_t ssid_len);
static struct ieee80211_sta_bss *
ieee80211_bss_get(struct wpa_supplicant *wpa_s, const u8 *bssid);
static int ieee80211_sta_find_ibss(struct wpa_supplicant *wpa_s);
static int ieee80211_sta_wep_configured(struct wpa_supplicant *wpa_s);
static void ieee80211_sta_timer(void *eloop_ctx, void *timeout_ctx);
static void ieee80211_sta_scan_timer(void *eloop_ctx, void *timeout_ctx);


/* Parsed Information Elements */
struct ieee802_11_elems {
	u8 *ssid;
	u8 ssid_len;
	u8 *supp_rates;
	u8 supp_rates_len;
	u8 *fh_params;
	u8 fh_params_len;
	u8 *ds_params;
	u8 ds_params_len;
	u8 *cf_params;
	u8 cf_params_len;
	u8 *tim;
	u8 tim_len;
	u8 *ibss_params;
	u8 ibss_params_len;
	u8 *challenge;
	u8 challenge_len;
	u8 *wpa;
	u8 wpa_len;
	u8 *rsn;
	u8 rsn_len;
	u8 *erp_info;
	u8 erp_info_len;
	u8 *ext_supp_rates;
	u8 ext_supp_rates_len;
	u8 *wmm_info;
	u8 wmm_info_len;
	u8 *wmm_param;
	u8 wmm_param_len;
};

typedef enum { ParseOK = 0, ParseUnknown = 1, ParseFailed = -1 } ParseRes;


static ParseRes ieee802_11_parse_elems(u8 *start, size_t len,
				       struct ieee802_11_elems *elems)
{
	size_t left = len;
	u8 *pos = start;
	int unknown = 0;

	os_memset(elems, 0, sizeof(*elems));

	while (left >= 2) {
		u8 id, elen;

		id = *pos++;
		elen = *pos++;
		left -= 2;

		if (elen > left) {
#if 0
			wpa_printf(MSG_MSGDUMP, "MLME: IEEE 802.11 element "
				   "parse failed (id=%d elen=%d left=%d)",
				   id, elen, left);
#endif
			return ParseFailed;
		}

		switch (id) {
		case WLAN_EID_SSID:
			elems->ssid = pos;
			elems->ssid_len = elen;
			break;
		case WLAN_EID_SUPP_RATES:
			elems->supp_rates = pos;
			elems->supp_rates_len = elen;
			break;
		case WLAN_EID_FH_PARAMS:
			elems->fh_params = pos;
			elems->fh_params_len = elen;
			break;
		case WLAN_EID_DS_PARAMS:
			elems->ds_params = pos;
			elems->ds_params_len = elen;
			break;
		case WLAN_EID_CF_PARAMS:
			elems->cf_params = pos;
			elems->cf_params_len = elen;
			break;
		case WLAN_EID_TIM:
			elems->tim = pos;
			elems->tim_len = elen;
			break;
		case WLAN_EID_IBSS_PARAMS:
			elems->ibss_params = pos;
			elems->ibss_params_len = elen;
			break;
		case WLAN_EID_CHALLENGE:
			elems->challenge = pos;
			elems->challenge_len = elen;
			break;
		case WLAN_EID_VENDOR_SPECIFIC:
			if (elen >= 4 && pos[0] == 0x00 && pos[1] == 0x50 &&
			    pos[2] == 0xf2) {
				/* Microsoft OUI (00:50:F2) */
				if (pos[3] == 1) {
					/* OUI Type 1 - WPA IE */
					elems->wpa = pos;
					elems->wpa_len = elen;
				} else if (elen >= 5 && pos[3] == 2) {
					if (pos[4] == 0) {
						elems->wmm_info = pos;
						elems->wmm_info_len = elen;
					} else if (pos[4] == 1) {
						elems->wmm_param = pos;
						elems->wmm_param_len = elen;
					}
				}
			}
			break;
		case WLAN_EID_RSN:
			elems->rsn = pos;
			elems->rsn_len = elen;
			break;
		case WLAN_EID_ERP_INFO:
			elems->erp_info = pos;
			elems->erp_info_len = elen;
			break;
		case WLAN_EID_EXT_SUPP_RATES:
			elems->ext_supp_rates = pos;
			elems->ext_supp_rates_len = elen;
			break;
		default:
#if 0
			wpa_printf(MSG_MSGDUMP "MLME: IEEE 802.11 element "
				   "parse ignored unknown element (id=%d "
				   "elen=%d)", id, elen);
#endif
			unknown++;
			break;
		}

		left -= elen;
		pos += elen;
	}

	if (left)
		return ParseFailed;

	return unknown ? ParseUnknown : ParseOK;
}


static int ieee80211_sta_set_channel(struct wpa_supplicant *wpa_s,
				     wpa_hw_mode phymode, int chan,
				     int freq)
{
	size_t i;
	struct wpa_hw_modes *mode;

	for (i = 0; i < wpa_s->mlme.num_modes; i++) {
		mode = &wpa_s->mlme.modes[i];
		if (mode->mode == phymode) {
			wpa_s->mlme.curr_rates = mode->rates;
			wpa_s->mlme.num_curr_rates = mode->num_rates;
			break;
		}
	}

	return wpa_drv_set_channel(wpa_s, phymode, chan, freq);
}



#if 0 /* FIX */
static int ecw2cw(int ecw)
{
	int cw = 1;
	while (ecw > 0) {
		cw <<= 1;
		ecw--;
	}
	return cw - 1;
}
#endif


static void ieee80211_sta_wmm_params(struct wpa_supplicant *wpa_s,
				     u8 *wmm_param, size_t wmm_param_len)
{
	size_t left;
	int count;
	u8 *pos;

	if (wmm_param_len < 8 || wmm_param[5] /* version */ != 1)
		return;
	count = wmm_param[6] & 0x0f;
	if (count == wpa_s->mlme.wmm_last_param_set)
		return;
	wpa_s->mlme.wmm_last_param_set = count;

	pos = wmm_param + 8;
	left = wmm_param_len - 8;

#if 0 /* FIX */
	wmm_acm = 0;
	for (; left >= 4; left -= 4, pos += 4) {
		int aci = (pos[0] >> 5) & 0x03;
		int acm = (pos[0] >> 4) & 0x01;
		int queue;

		switch (aci) {
		case 1:
			queue = IEEE80211_TX_QUEUE_DATA3;
			if (acm)
				wmm_acm |= BIT(1) | BIT(2);
			break;
		case 2:
			queue = IEEE80211_TX_QUEUE_DATA1;
			if (acm)
				wmm_acm |= BIT(4) | BIT(5);
			break;
		case 3:
			queue = IEEE80211_TX_QUEUE_DATA0;
			if (acm)
				wmm_acm |= BIT(6) | BIT(7);
			break;
		case 0:
		default:
			queue = IEEE80211_TX_QUEUE_DATA2;
			if (acm)
				wpa_s->mlme.wmm_acm |= BIT(0) | BIT(3);
			break;
		}

		params.aifs = pos[0] & 0x0f;
		params.cw_max = ecw2cw((pos[1] & 0xf0) >> 4);
		params.cw_min = ecw2cw(pos[1] & 0x0f);
		/* TXOP is in units of 32 usec; burst_time in 0.1 ms */
		params.burst_time = (pos[2] | (pos[3] << 8)) * 32 / 100;
		wpa_printf(MSG_DEBUG, "MLME: WMM queue=%d aci=%d acm=%d "
			   "aifs=%d cWmin=%d cWmax=%d burst=%d",
			   queue, aci, acm, params.aifs, params.cw_min,
			   params.cw_max, params.burst_time);
		/* TODO: handle ACM (block TX, fallback to next lowest allowed
		 * AC for now) */
		if (local->hw->conf_tx(local->mdev, queue, &params)) {
			wpa_printf(MSG_DEBUG, "MLME: failed to set TX queue "
				   "parameters for queue %d", queue);
		}
	}
#endif
}


static void ieee80211_set_associated(struct wpa_supplicant *wpa_s, int assoc)
{
	if (wpa_s->mlme.associated == assoc)
		return;

	wpa_s->mlme.associated = assoc;

	if (assoc) {
		union wpa_event_data data;
		os_memset(&data, 0, sizeof(data));
		wpa_s->mlme.prev_bssid_set = 1;
		os_memcpy(wpa_s->mlme.prev_bssid, wpa_s->bssid, ETH_ALEN);
		data.assoc_info.req_ies = wpa_s->mlme.assocreq_ies;
		data.assoc_info.req_ies_len = wpa_s->mlme.assocreq_ies_len;
		data.assoc_info.resp_ies = wpa_s->mlme.assocresp_ies;
		data.assoc_info.resp_ies_len = wpa_s->mlme.assocresp_ies_len;
		wpa_supplicant_event(wpa_s, EVENT_ASSOC, &data);
	} else {
		wpa_supplicant_event(wpa_s, EVENT_DISASSOC, NULL);
	}
	os_get_time(&wpa_s->mlme.last_probe);
}


static void ieee80211_sta_tx(struct wpa_supplicant *wpa_s, const u8 *buf,
			     size_t len)
{
	wpa_drv_send_mlme(wpa_s, buf, len);
}


static void ieee80211_send_auth(struct wpa_supplicant *wpa_s,
				int transaction, u8 *extra, size_t extra_len,
				int encrypt)
{
	u8 *buf;
	size_t len;
	struct ieee80211_mgmt *mgmt;

	buf = os_malloc(sizeof(*mgmt) + 6 + extra_len);
	if (buf == NULL) {
		wpa_printf(MSG_DEBUG, "MLME: failed to allocate buffer for "
			   "auth frame");
		return;
	}

	mgmt = (struct ieee80211_mgmt *) buf;
	len = 24 + 6;
	os_memset(mgmt, 0, 24 + 6);
	mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_AUTH);
	if (encrypt)
		mgmt->frame_control |= host_to_le16(WLAN_FC_ISWEP);
	os_memcpy(mgmt->da, wpa_s->bssid, ETH_ALEN);
	os_memcpy(mgmt->sa, wpa_s->own_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, wpa_s->bssid, ETH_ALEN);
	mgmt->u.auth.auth_alg = host_to_le16(wpa_s->mlme.auth_alg);
	mgmt->u.auth.auth_transaction = host_to_le16(transaction);
	wpa_s->mlme.auth_transaction = transaction + 1;
	mgmt->u.auth.status_code = host_to_le16(0);
	if (extra) {
		os_memcpy(buf + len, extra, extra_len);
		len += extra_len;
	}

	ieee80211_sta_tx(wpa_s, buf, len);
	os_free(buf);
}


static void ieee80211_reschedule_timer(struct wpa_supplicant *wpa_s, int ms)
{
	eloop_cancel_timeout(ieee80211_sta_timer, wpa_s, NULL);
	eloop_register_timeout(ms / 1000, 1000 * (ms % 1000),
			       ieee80211_sta_timer, wpa_s, NULL);
}


static void ieee80211_authenticate(struct wpa_supplicant *wpa_s)
{
	wpa_s->mlme.auth_tries++;
	if (wpa_s->mlme.auth_tries > IEEE80211_AUTH_MAX_TRIES) {
		wpa_printf(MSG_DEBUG, "MLME: authentication with AP " MACSTR
			   " timed out", MAC2STR(wpa_s->bssid));
		return;
	}

	wpa_s->mlme.state = IEEE80211_AUTHENTICATE;
	wpa_printf(MSG_DEBUG, "MLME: authenticate with AP " MACSTR,
		   MAC2STR(wpa_s->bssid));

	ieee80211_send_auth(wpa_s, 1, NULL, 0, 0);

	ieee80211_reschedule_timer(wpa_s, IEEE80211_AUTH_TIMEOUT);
}


static void ieee80211_send_assoc(struct wpa_supplicant *wpa_s)
{
	struct ieee80211_mgmt *mgmt;
	u8 *pos, *ies, *buf;
	int i, len;
	u16 capab;
	struct ieee80211_sta_bss *bss;
	int wmm = 0;
	size_t blen;

	if (wpa_s->mlme.curr_rates == NULL) {
		wpa_printf(MSG_DEBUG, "MLME: curr_rates not set for assoc");
		return;
	}

	buf = os_malloc(sizeof(*mgmt) + 200 + wpa_s->mlme.extra_ie_len +
			wpa_s->mlme.ssid_len);
	if (buf == NULL) {
		wpa_printf(MSG_DEBUG, "MLME: failed to allocate buffer for "
			   "assoc frame");
		return;
	}
	blen = 0;

	capab = wpa_s->mlme.capab;
	if (wpa_s->mlme.phymode == WPA_MODE_IEEE80211G) {
		capab |= WLAN_CAPABILITY_SHORT_SLOT_TIME |
			WLAN_CAPABILITY_SHORT_PREAMBLE;
	}
	bss = ieee80211_bss_get(wpa_s, wpa_s->bssid);
	if (bss) {
		if (bss->capability & WLAN_CAPABILITY_PRIVACY)
			capab |= WLAN_CAPABILITY_PRIVACY;
		if (bss->wmm_ie) {
			wmm = 1;
		}
	}

	mgmt = (struct ieee80211_mgmt *) buf;
	blen += 24;
	os_memset(mgmt, 0, 24);
	os_memcpy(mgmt->da, wpa_s->bssid, ETH_ALEN);
	os_memcpy(mgmt->sa, wpa_s->own_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, wpa_s->bssid, ETH_ALEN);

	if (wpa_s->mlme.prev_bssid_set) {
		blen += 10;
		mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
						   WLAN_FC_STYPE_REASSOC_REQ);
		mgmt->u.reassoc_req.capab_info = host_to_le16(capab);
		mgmt->u.reassoc_req.listen_interval = host_to_le16(1);
		os_memcpy(mgmt->u.reassoc_req.current_ap,
			  wpa_s->mlme.prev_bssid,
			  ETH_ALEN);
	} else {
		blen += 4;
		mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
						   WLAN_FC_STYPE_ASSOC_REQ);
		mgmt->u.assoc_req.capab_info = host_to_le16(capab);
		mgmt->u.assoc_req.listen_interval = host_to_le16(1);
	}

	/* SSID */
	ies = pos = buf + blen;
	blen += 2 + wpa_s->mlme.ssid_len;
	*pos++ = WLAN_EID_SSID;
	*pos++ = wpa_s->mlme.ssid_len;
	os_memcpy(pos, wpa_s->mlme.ssid, wpa_s->mlme.ssid_len);

	len = wpa_s->mlme.num_curr_rates;
	if (len > 8)
		len = 8;
	pos = buf + blen;
	blen += len + 2;
	*pos++ = WLAN_EID_SUPP_RATES;
	*pos++ = len;
	for (i = 0; i < len; i++) {
		int rate = wpa_s->mlme.curr_rates[i].rate;
		*pos++ = (u8) (rate / 5);
	}

	if (wpa_s->mlme.num_curr_rates > len) {
		pos = buf + blen;
		blen += wpa_s->mlme.num_curr_rates - len + 2;
		*pos++ = WLAN_EID_EXT_SUPP_RATES;
		*pos++ = wpa_s->mlme.num_curr_rates - len;
		for (i = len; i < wpa_s->mlme.num_curr_rates; i++) {
			int rate = wpa_s->mlme.curr_rates[i].rate;
			*pos++ = (u8) (rate / 5);
		}
	}

	if (wpa_s->mlme.extra_ie) {
		pos = buf + blen;
		blen += wpa_s->mlme.extra_ie_len;
		os_memcpy(pos, wpa_s->mlme.extra_ie, wpa_s->mlme.extra_ie_len);
	}

	if (wmm && wpa_s->mlme.wmm_enabled) {
		pos = buf + blen;
		blen += 9;
		*pos++ = WLAN_EID_VENDOR_SPECIFIC;
		*pos++ = 7; /* len */
		*pos++ = 0x00; /* Microsoft OUI 00:50:F2 */
		*pos++ = 0x50;
		*pos++ = 0xf2;
		*pos++ = 2; /* WME */
		*pos++ = 0; /* WME info */
		*pos++ = 1; /* WME ver */
		*pos++ = 0;
	}

	os_free(wpa_s->mlme.assocreq_ies);
	wpa_s->mlme.assocreq_ies_len = (buf + blen) - ies;
	wpa_s->mlme.assocreq_ies = os_malloc(wpa_s->mlme.assocreq_ies_len);
	if (wpa_s->mlme.assocreq_ies) {
		os_memcpy(wpa_s->mlme.assocreq_ies, ies,
			  wpa_s->mlme.assocreq_ies_len);
	}

	ieee80211_sta_tx(wpa_s, buf, blen);
	os_free(buf);
}


static void ieee80211_send_deauth(struct wpa_supplicant *wpa_s, u16 reason)
{
	u8 *buf;
	size_t len;
	struct ieee80211_mgmt *mgmt;

	buf = os_zalloc(sizeof(*mgmt));
	if (buf == NULL) {
		wpa_printf(MSG_DEBUG, "MLME: failed to allocate buffer for "
			   "deauth frame");
		return;
	}

	mgmt = (struct ieee80211_mgmt *) buf;
	len = 24;
	os_memcpy(mgmt->da, wpa_s->bssid, ETH_ALEN);
	os_memcpy(mgmt->sa, wpa_s->own_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, wpa_s->bssid, ETH_ALEN);
	mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_DEAUTH);
	len += 2;
	mgmt->u.deauth.reason_code = host_to_le16(reason);

	ieee80211_sta_tx(wpa_s, buf, len);
	os_free(buf);
}


static void ieee80211_send_disassoc(struct wpa_supplicant *wpa_s, u16 reason)
{
	u8 *buf;
	size_t len;
	struct ieee80211_mgmt *mgmt;

	buf = os_zalloc(sizeof(*mgmt));
	if (buf == NULL) {
		wpa_printf(MSG_DEBUG, "MLME: failed to allocate buffer for "
			   "disassoc frame");
		return;
	}

	mgmt = (struct ieee80211_mgmt *) buf;
	len = 24;
	os_memcpy(mgmt->da, wpa_s->bssid, ETH_ALEN);
	os_memcpy(mgmt->sa, wpa_s->own_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, wpa_s->bssid, ETH_ALEN);
	mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_DISASSOC);
	len += 2;
	mgmt->u.disassoc.reason_code = host_to_le16(reason);

	ieee80211_sta_tx(wpa_s, buf, len);
	os_free(buf);
}


static int ieee80211_privacy_mismatch(struct wpa_supplicant *wpa_s)
{
	struct ieee80211_sta_bss *bss;
	int res = 0;

	if (wpa_s->mlme.mixed_cell ||
	    wpa_s->mlme.key_mgmt != KEY_MGMT_NONE)
		return 0;

	bss = ieee80211_bss_get(wpa_s, wpa_s->bssid);
	if (bss == NULL)
		return 0;

	if (ieee80211_sta_wep_configured(wpa_s) !=
	    !!(bss->capability & WLAN_CAPABILITY_PRIVACY))
		res = 1;

	return res;
}


static void ieee80211_associate(struct wpa_supplicant *wpa_s)
{
	wpa_s->mlme.assoc_tries++;
	if (wpa_s->mlme.assoc_tries > IEEE80211_ASSOC_MAX_TRIES) {
		wpa_printf(MSG_DEBUG, "MLME: association with AP " MACSTR
			   " timed out", MAC2STR(wpa_s->bssid));
		return;
	}

	wpa_s->mlme.state = IEEE80211_ASSOCIATE;
	wpa_printf(MSG_DEBUG, "MLME: associate with AP " MACSTR,
		   MAC2STR(wpa_s->bssid));
	if (ieee80211_privacy_mismatch(wpa_s)) {
		wpa_printf(MSG_DEBUG, "MLME: mismatch in privacy "
			   "configuration and mixed-cell disabled - abort "
			   "association");
		return;
	}

	ieee80211_send_assoc(wpa_s);

	ieee80211_reschedule_timer(wpa_s, IEEE80211_ASSOC_TIMEOUT);
}


static void ieee80211_associated(struct wpa_supplicant *wpa_s)
{
	int disassoc;

	/* TODO: start monitoring current AP signal quality and number of
	 * missed beacons. Scan other channels every now and then and search
	 * for better APs. */
	/* TODO: remove expired BSSes */

	wpa_s->mlme.state = IEEE80211_ASSOCIATED;

#if 0 /* FIX */
	sta = sta_info_get(local, wpa_s->bssid);
	if (sta == NULL) {
		wpa_printf(MSG_DEBUG "MLME: No STA entry for own AP " MACSTR,
			   MAC2STR(wpa_s->bssid));
		disassoc = 1;
	} else {
		disassoc = 0;
		if (time_after(jiffies,
			       sta->last_rx + IEEE80211_MONITORING_INTERVAL)) {
			if (wpa_s->mlme.probereq_poll) {
				wpa_printf(MSG_DEBUG "MLME: No ProbeResp from "
					   "current AP " MACSTR " - assume "
					   "out of range",
					   MAC2STR(wpa_s->bssid));
				disassoc = 1;
			} else {
				ieee80211_send_probe_req(
					wpa_s->bssid,
					wpa_s->mlme.scan_ssid,
					wpa_s->mlme.scan_ssid_len);
				wpa_s->mlme.probereq_poll = 1;
			}
		} else {
			wpa_s->mlme.probereq_poll = 0;
			if (time_after(jiffies, wpa_s->mlme.last_probe +
				       IEEE80211_PROBE_INTERVAL)) {
				wpa_s->mlme.last_probe = jiffies;
				ieee80211_send_probe_req(wpa_s->bssid,
							 wpa_s->mlme.ssid,
							 wpa_s->mlme.ssid_len);
			}
		}
		sta_info_release(local, sta);
	}
#else
	disassoc = 0;
#endif
	if (disassoc) {
		wpa_supplicant_event(wpa_s, EVENT_DISASSOC, NULL);
		ieee80211_reschedule_timer(wpa_s,
					   IEEE80211_MONITORING_INTERVAL +
					   30000);
	} else {
		ieee80211_reschedule_timer(wpa_s,
					   IEEE80211_MONITORING_INTERVAL);
	}
}


static void ieee80211_send_probe_req(struct wpa_supplicant *wpa_s,
				     const u8 *dst,
				     const u8 *ssid, size_t ssid_len)
{
	u8 *buf;
	size_t len;
	struct ieee80211_mgmt *mgmt;
	u8 *pos, *supp_rates;
	u8 *esupp_rates = NULL;
	int i;

	buf = os_malloc(sizeof(*mgmt) + 200);
	if (buf == NULL) {
		wpa_printf(MSG_DEBUG, "MLME: failed to allocate buffer for "
			   "probe request");
		return;
	}

	mgmt = (struct ieee80211_mgmt *) buf;
	len = 24;
	os_memset(mgmt, 0, 24);
	mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_PROBE_REQ);
	os_memcpy(mgmt->sa, wpa_s->own_addr, ETH_ALEN);
	if (dst) {
		os_memcpy(mgmt->da, dst, ETH_ALEN);
		os_memcpy(mgmt->bssid, dst, ETH_ALEN);
	} else {
		os_memset(mgmt->da, 0xff, ETH_ALEN);
		os_memset(mgmt->bssid, 0xff, ETH_ALEN);
	}
	pos = buf + len;
	len += 2 + ssid_len;
	*pos++ = WLAN_EID_SSID;
	*pos++ = ssid_len;
	os_memcpy(pos, ssid, ssid_len);

	supp_rates = buf + len;
	len += 2;
	supp_rates[0] = WLAN_EID_SUPP_RATES;
	supp_rates[1] = 0;
	for (i = 0; i < wpa_s->mlme.num_curr_rates; i++) {
		struct wpa_rate_data *rate = &wpa_s->mlme.curr_rates[i];
		if (!(rate->flags & WPA_RATE_SUPPORTED))
			continue;
		if (esupp_rates) {
			pos = buf + len;
			len++;
			esupp_rates[1]++;
		} else if (supp_rates[1] == 8) {
			esupp_rates = pos;
			esupp_rates[0] = WLAN_EID_EXT_SUPP_RATES;
			esupp_rates[1] = 1;
			pos = &esupp_rates[2];
		} else {
			pos = buf + len;
			len++;
			supp_rates[1]++;
		}
		*pos = rate->rate / 5;
	}

	ieee80211_sta_tx(wpa_s, buf, len);
	os_free(buf);
}


static int ieee80211_sta_wep_configured(struct wpa_supplicant *wpa_s)
{
#if 0 /* FIX */
	if (sdata == NULL || sdata->default_key == NULL ||
	    sdata->default_key->alg != ALG_WEP)
		return 0;
	return 1;
#else
	return 0;
#endif
}


static void ieee80211_auth_completed(struct wpa_supplicant *wpa_s)
{
	wpa_printf(MSG_DEBUG, "MLME: authenticated");
	wpa_s->mlme.authenticated = 1;
	ieee80211_associate(wpa_s);
}


static void ieee80211_auth_challenge(struct wpa_supplicant *wpa_s,
				     struct ieee80211_mgmt *mgmt,
				     size_t len,
				     struct ieee80211_rx_status *rx_status)
{
	u8 *pos;
	struct ieee802_11_elems elems;

	wpa_printf(MSG_DEBUG, "MLME: replying to auth challenge");
	pos = mgmt->u.auth.variable;
	if (ieee802_11_parse_elems(pos, len - (pos - (u8 *) mgmt), &elems)
	    == ParseFailed) {
		wpa_printf(MSG_DEBUG, "MLME: failed to parse Auth(challenge)");
		return;
	}
	if (elems.challenge == NULL) {
		wpa_printf(MSG_DEBUG, "MLME: no challenge IE in shared key "
			   "auth frame");
		return;
	}
	ieee80211_send_auth(wpa_s, 3, elems.challenge - 2,
			    elems.challenge_len + 2, 1);
}


static void ieee80211_rx_mgmt_auth(struct wpa_supplicant *wpa_s,
				   struct ieee80211_mgmt *mgmt,
				   size_t len,
				   struct ieee80211_rx_status *rx_status)
{
	struct wpa_ssid *ssid = wpa_s->current_ssid;
	u16 auth_alg, auth_transaction, status_code;
	int adhoc;

	adhoc = ssid && ssid->mode == 1;

	if (wpa_s->mlme.state != IEEE80211_AUTHENTICATE && !adhoc) {
		wpa_printf(MSG_DEBUG, "MLME: authentication frame received "
			   "from " MACSTR ", but not in authenticate state - "
			   "ignored", MAC2STR(mgmt->sa));
		return;
	}

	if (len < 24 + 6) {
		wpa_printf(MSG_DEBUG, "MLME: too short (%lu) authentication "
			   "frame received from " MACSTR " - ignored",
			   (unsigned long) len, MAC2STR(mgmt->sa));
		return;
	}

	if (!adhoc && os_memcmp(wpa_s->bssid, mgmt->sa, ETH_ALEN) != 0) {
		wpa_printf(MSG_DEBUG, "MLME: authentication frame received "
			   "from unknown AP (SA=" MACSTR " BSSID=" MACSTR
			   ") - ignored",
			   MAC2STR(mgmt->sa), MAC2STR(mgmt->bssid));
		return;
	}

	if (adhoc && os_memcmp(wpa_s->bssid, mgmt->bssid, ETH_ALEN) != 0) {
		wpa_printf(MSG_DEBUG, "MLME: authentication frame received "
			   "from unknown BSSID (SA=" MACSTR " BSSID=" MACSTR
			   ") - ignored",
			   MAC2STR(mgmt->sa), MAC2STR(mgmt->bssid));
		return;
	}

	auth_alg = le_to_host16(mgmt->u.auth.auth_alg);
	auth_transaction = le_to_host16(mgmt->u.auth.auth_transaction);
	status_code = le_to_host16(mgmt->u.auth.status_code);

	wpa_printf(MSG_DEBUG, "MLME: RX authentication from " MACSTR
		   " (alg=%d transaction=%d status=%d)",
		   MAC2STR(mgmt->sa), auth_alg, auth_transaction, status_code);

	if (adhoc) {
		/* IEEE 802.11 standard does not require authentication in IBSS
		 * networks and most implementations do not seem to use it.
		 * However, try to reply to authentication attempts if someone
		 * has actually implemented this.
		 * TODO: Could implement shared key authentication. */
		if (auth_alg != WLAN_AUTH_OPEN || auth_transaction != 1) {
			wpa_printf(MSG_DEBUG, "MLME: unexpected IBSS "
				   "authentication frame (alg=%d "
				   "transaction=%d)",
				   auth_alg, auth_transaction);
			return;
		}
		ieee80211_send_auth(wpa_s, 2, NULL, 0, 0);
	}

	if (auth_alg != wpa_s->mlme.auth_alg ||
	    auth_transaction != wpa_s->mlme.auth_transaction) {
		wpa_printf(MSG_DEBUG, "MLME: unexpected authentication frame "
			   "(alg=%d transaction=%d)",
			   auth_alg, auth_transaction);
		return;
	}

	if (status_code != WLAN_STATUS_SUCCESS) {
		wpa_printf(MSG_DEBUG, "MLME: AP denied authentication "
			   "(auth_alg=%d code=%d)", wpa_s->mlme.auth_alg,
			   status_code);
		if (status_code == WLAN_STATUS_NOT_SUPPORTED_AUTH_ALG) {
			const int num_algs = 3;
			u8 algs[num_algs];
			int i, pos;
			algs[0] = algs[1] = algs[2] = 0xff;
			if (wpa_s->mlme.auth_algs & IEEE80211_AUTH_ALG_OPEN)
				algs[0] = WLAN_AUTH_OPEN;
			if (wpa_s->mlme.auth_algs &
			    IEEE80211_AUTH_ALG_SHARED_KEY)
				algs[1] = WLAN_AUTH_SHARED_KEY;
			if (wpa_s->mlme.auth_algs & IEEE80211_AUTH_ALG_LEAP)
				algs[2] = WLAN_AUTH_LEAP;
			if (wpa_s->mlme.auth_alg == WLAN_AUTH_OPEN)
				pos = 0;
			else if (wpa_s->mlme.auth_alg == WLAN_AUTH_SHARED_KEY)
				pos = 1;
			else
				pos = 2;
			for (i = 0; i < num_algs; i++) {
				pos++;
				if (pos >= num_algs)
					pos = 0;
				if (algs[pos] == wpa_s->mlme.auth_alg ||
				    algs[pos] == 0xff)
					continue;
				if (algs[pos] == WLAN_AUTH_SHARED_KEY &&
				    !ieee80211_sta_wep_configured(wpa_s))
					continue;
				wpa_s->mlme.auth_alg = algs[pos];
				wpa_printf(MSG_DEBUG, "MLME: set auth_alg=%d "
					   "for next try",
					   wpa_s->mlme.auth_alg);
				break;
			}
		}
		return;
	}

	switch (wpa_s->mlme.auth_alg) {
	case WLAN_AUTH_OPEN:
	case WLAN_AUTH_LEAP:
		ieee80211_auth_completed(wpa_s);
		break;
	case WLAN_AUTH_SHARED_KEY:
		if (wpa_s->mlme.auth_transaction == 4)
			ieee80211_auth_completed(wpa_s);
		else
			ieee80211_auth_challenge(wpa_s, mgmt, len,
						 rx_status);
		break;
	}
}


static void ieee80211_rx_mgmt_deauth(struct wpa_supplicant *wpa_s,
				     struct ieee80211_mgmt *mgmt,
				     size_t len,
				     struct ieee80211_rx_status *rx_status)
{
	u16 reason_code;

	if (len < 24 + 2) {
		wpa_printf(MSG_DEBUG, "MLME: too short (%lu) deauthentication "
			   "frame received from " MACSTR " - ignored",
			   (unsigned long) len, MAC2STR(mgmt->sa));
		return;
	}

	if (os_memcmp(wpa_s->bssid, mgmt->sa, ETH_ALEN) != 0) {
		wpa_printf(MSG_DEBUG, "MLME: deauthentication frame received "
			   "from unknown AP (SA=" MACSTR " BSSID=" MACSTR
			   ") - ignored",
			   MAC2STR(mgmt->sa), MAC2STR(mgmt->bssid));
		return;
	}

	reason_code = le_to_host16(mgmt->u.deauth.reason_code);

	wpa_printf(MSG_DEBUG, "MLME: RX deauthentication from " MACSTR
		   " (reason=%d)", MAC2STR(mgmt->sa), reason_code);

	if (wpa_s->mlme.authenticated)
		wpa_printf(MSG_DEBUG, "MLME: deauthenticated");

	if (wpa_s->mlme.state == IEEE80211_AUTHENTICATE ||
	    wpa_s->mlme.state == IEEE80211_ASSOCIATE ||
	    wpa_s->mlme.state == IEEE80211_ASSOCIATED) {
		wpa_s->mlme.state = IEEE80211_AUTHENTICATE;
		ieee80211_reschedule_timer(wpa_s,
					   IEEE80211_RETRY_AUTH_INTERVAL);
	}

	ieee80211_set_associated(wpa_s, 0);
	wpa_s->mlme.authenticated = 0;
}


static void ieee80211_rx_mgmt_disassoc(struct wpa_supplicant *wpa_s,
				       struct ieee80211_mgmt *mgmt,
				       size_t len,
				       struct ieee80211_rx_status *rx_status)
{
	u16 reason_code;

	if (len < 24 + 2) {
		wpa_printf(MSG_DEBUG, "MLME: too short (%lu) disassociation "
			   "frame received from " MACSTR " - ignored",
			   (unsigned long) len, MAC2STR(mgmt->sa));
		return;
	}

	if (os_memcmp(wpa_s->bssid, mgmt->sa, ETH_ALEN) != 0) {
		wpa_printf(MSG_DEBUG, "MLME: disassociation frame received "
			   "from unknown AP (SA=" MACSTR " BSSID=" MACSTR
			   ") - ignored",
			   MAC2STR(mgmt->sa), MAC2STR(mgmt->bssid));
		return;
	}

	reason_code = le_to_host16(mgmt->u.disassoc.reason_code);

	wpa_printf(MSG_DEBUG, "MLME: RX disassociation from " MACSTR
		   " (reason=%d)", MAC2STR(mgmt->sa), reason_code);

	if (wpa_s->mlme.associated)
		wpa_printf(MSG_DEBUG, "MLME: disassociated");

	if (wpa_s->mlme.state == IEEE80211_ASSOCIATED) {
		wpa_s->mlme.state = IEEE80211_ASSOCIATE;
		ieee80211_reschedule_timer(wpa_s,
					   IEEE80211_RETRY_AUTH_INTERVAL);
	}

	ieee80211_set_associated(wpa_s, 0);
}


static void ieee80211_rx_mgmt_assoc_resp(struct wpa_supplicant *wpa_s,
					 struct ieee80211_mgmt *mgmt,
					 size_t len,
					 struct ieee80211_rx_status *rx_status,
					 int reassoc)
{
	u8 rates[32];
	size_t rates_len;
	u16 capab_info, status_code, aid;
	struct ieee802_11_elems elems;
	u8 *pos;

	/* AssocResp and ReassocResp have identical structure, so process both
	 * of them in this function. */

	if (wpa_s->mlme.state != IEEE80211_ASSOCIATE) {
		wpa_printf(MSG_DEBUG, "MLME: association frame received from "
			   MACSTR ", but not in associate state - ignored",
			   MAC2STR(mgmt->sa));
		return;
	}

	if (len < 24 + 6) {
		wpa_printf(MSG_DEBUG, "MLME: too short (%lu) association "
			   "frame received from " MACSTR " - ignored",
			   (unsigned long) len, MAC2STR(mgmt->sa));
		return;
	}

	if (os_memcmp(wpa_s->bssid, mgmt->sa, ETH_ALEN) != 0) {
		wpa_printf(MSG_DEBUG, "MLME: association frame received from "
			   "unknown AP (SA=" MACSTR " BSSID=" MACSTR ") - "
			   "ignored", MAC2STR(mgmt->sa), MAC2STR(mgmt->bssid));
		return;
	}

	capab_info = le_to_host16(mgmt->u.assoc_resp.capab_info);
	status_code = le_to_host16(mgmt->u.assoc_resp.status_code);
	aid = le_to_host16(mgmt->u.assoc_resp.aid);
	if ((aid & (BIT(15) | BIT(14))) != (BIT(15) | BIT(14)))
		wpa_printf(MSG_DEBUG, "MLME: invalid aid value %d; bits 15:14 "
			   "not set", aid);
	aid &= ~(BIT(15) | BIT(14));

	wpa_printf(MSG_DEBUG, "MLME: RX %sssocResp from " MACSTR
		   " (capab=0x%x status=%d aid=%d)",
		   reassoc ? "Rea" : "A", MAC2STR(mgmt->sa),
		   capab_info, status_code, aid);

	if (status_code != WLAN_STATUS_SUCCESS) {
		wpa_printf(MSG_DEBUG, "MLME: AP denied association (code=%d)",
			   status_code);
		return;
	}

	pos = mgmt->u.assoc_resp.variable;
	if (ieee802_11_parse_elems(pos, len - (pos - (u8 *) mgmt), &elems)
	    == ParseFailed) {
		wpa_printf(MSG_DEBUG, "MLME: failed to parse AssocResp");
		return;
	}

	if (elems.supp_rates == NULL) {
		wpa_printf(MSG_DEBUG, "MLME: no SuppRates element in "
			   "AssocResp");
		return;
	}

	wpa_printf(MSG_DEBUG, "MLME: associated");
	wpa_s->mlme.aid = aid;
	wpa_s->mlme.ap_capab = capab_info;

	os_free(wpa_s->mlme.assocresp_ies);
	wpa_s->mlme.assocresp_ies_len = len - (pos - (u8 *) mgmt);
	wpa_s->mlme.assocresp_ies = os_malloc(wpa_s->mlme.assocresp_ies_len);
	if (wpa_s->mlme.assocresp_ies) {
		os_memcpy(wpa_s->mlme.assocresp_ies, pos,
			  wpa_s->mlme.assocresp_ies_len);
	}

	ieee80211_set_associated(wpa_s, 1);

	rates_len = elems.supp_rates_len;
	if (rates_len > sizeof(rates))
		rates_len = sizeof(rates);
	os_memcpy(rates, elems.supp_rates, rates_len);
	if (elems.ext_supp_rates) {
		size_t _len = elems.ext_supp_rates_len;
		if (_len > sizeof(rates) - rates_len)
			_len = sizeof(rates) - rates_len;
		os_memcpy(rates + rates_len, elems.ext_supp_rates, _len);
		rates_len += _len;
	}

	if (wpa_drv_set_bssid(wpa_s, wpa_s->bssid) < 0) {
		wpa_printf(MSG_DEBUG, "MLME: failed to set BSSID for the "
			   "netstack");
	}
	if (wpa_drv_set_ssid(wpa_s, wpa_s->mlme.ssid, wpa_s->mlme.ssid_len) <
	    0) {
		wpa_printf(MSG_DEBUG, "MLME: failed to set SSID for the "
			   "netstack");
	}

	/* Remove STA entry before adding a new one just in case to avoid
	 * problems with existing configuration (e.g., keys). */
	wpa_drv_mlme_remove_sta(wpa_s, wpa_s->bssid);
	if (wpa_drv_mlme_add_sta(wpa_s, wpa_s->bssid, rates, rates_len) < 0) {
		wpa_printf(MSG_DEBUG, "MLME: failed to add STA entry to the "
			   "netstack");
	}

#if 0 /* FIX? */
	sta->assoc_ap = 1;

	if (elems.wmm_param && wpa_s->mlme.wmm_enabled) {
		sta->flags |= WLAN_STA_WME;
		ieee80211_sta_wmm_params(wpa_s, elems.wmm_param,
					 elems.wmm_param_len);
	}
#endif

	ieee80211_associated(wpa_s);
}


/* Caller must hold local->sta_bss_lock */
static void __ieee80211_bss_hash_add(struct wpa_supplicant *wpa_s,
				     struct ieee80211_sta_bss *bss)
{
	bss->hnext = wpa_s->mlme.sta_bss_hash[STA_HASH(bss->bssid)];
	wpa_s->mlme.sta_bss_hash[STA_HASH(bss->bssid)] = bss;
}


/* Caller must hold local->sta_bss_lock */
static void __ieee80211_bss_hash_del(struct wpa_supplicant *wpa_s,
				     struct ieee80211_sta_bss *bss)
{
	struct ieee80211_sta_bss *b, *prev = NULL;
	b = wpa_s->mlme.sta_bss_hash[STA_HASH(bss->bssid)];
	while (b) {
		if (b == bss) {
			if (prev == NULL) {
				wpa_s->mlme.sta_bss_hash[STA_HASH(bss->bssid)]
					= bss->hnext;
			} else {
				prev->hnext = bss->hnext;
			}
			break;
		}
		prev = b;
		b = b->hnext;
	}
}


static struct ieee80211_sta_bss *
ieee80211_bss_add(struct wpa_supplicant *wpa_s, const u8 *bssid)
{
	struct ieee80211_sta_bss *bss;

	bss = os_zalloc(sizeof(*bss));
	if (bss == NULL)
		return NULL;
	os_memcpy(bss->bssid, bssid, ETH_ALEN);

	/* TODO: order by RSSI? */
	bss->next = wpa_s->mlme.sta_bss_list;
	wpa_s->mlme.sta_bss_list = bss;
	__ieee80211_bss_hash_add(wpa_s, bss);
	return bss;
}


static struct ieee80211_sta_bss *
ieee80211_bss_get(struct wpa_supplicant *wpa_s, const u8 *bssid)
{
	struct ieee80211_sta_bss *bss;

	bss = wpa_s->mlme.sta_bss_hash[STA_HASH(bssid)];
	while (bss) {
		if (os_memcmp(bss->bssid, bssid, ETH_ALEN) == 0)
			break;
		bss = bss->hnext;
	}
	return bss;
}


static void ieee80211_bss_free(struct wpa_supplicant *wpa_s,
			       struct ieee80211_sta_bss *bss)
{
	__ieee80211_bss_hash_del(wpa_s, bss);
	os_free(bss->wpa_ie);
	os_free(bss->rsn_ie);
	os_free(bss->wmm_ie);
	os_free(bss);
}


static void ieee80211_bss_list_deinit(struct wpa_supplicant *wpa_s)
{
	struct ieee80211_sta_bss *bss, *prev;

	bss = wpa_s->mlme.sta_bss_list;
	wpa_s->mlme.sta_bss_list = NULL;
	while (bss) {
		prev = bss;
		bss = bss->next;
		ieee80211_bss_free(wpa_s, prev);
	}
}


static void ieee80211_bss_info(struct wpa_supplicant *wpa_s,
			       struct ieee80211_mgmt *mgmt,
			       size_t len,
			       struct ieee80211_rx_status *rx_status,
			       int beacon)
{
	struct ieee802_11_elems elems;
	size_t baselen;
	int channel, invalid = 0, clen;
	struct ieee80211_sta_bss *bss;
	u64 timestamp;
	u8 *pos;

	if (!beacon && os_memcmp(mgmt->da, wpa_s->own_addr, ETH_ALEN))
		return; /* ignore ProbeResp to foreign address */

#if 0
	wpa_printf(MSG_MSGDUMP, "MLME: RX %s from " MACSTR " to " MACSTR,
		   beacon ? "Beacon" : "Probe Response",
		   MAC2STR(mgmt->sa), MAC2STR(mgmt->da));
#endif

	baselen = (u8 *) mgmt->u.beacon.variable - (u8 *) mgmt;
	if (baselen > len)
		return;

	pos = mgmt->u.beacon.timestamp;
	timestamp = ((u64) pos[7] << 56) | ((u64) pos[6] << 48) |
		((u64) pos[5] << 40) | ((u64) pos[4] << 32) |
		((u64) pos[3] << 24) | ((u64) pos[2] << 16) |
		((u64) pos[1] << 8) | ((u64) pos[0]);

#if 0 /* FIX */
	if (local->conf.mode == IW_MODE_ADHOC && beacon &&
	    os_memcmp(mgmt->bssid, local->bssid, ETH_ALEN) == 0) {
#ifdef IEEE80211_IBSS_DEBUG
		static unsigned long last_tsf_debug = 0;
		u64 tsf;
		if (local->hw->get_tsf)
			tsf = local->hw->get_tsf(local->mdev);
		else
			tsf = -1LLU;
		if (time_after(jiffies, last_tsf_debug + 5 * HZ)) {
			wpa_printf(MSG_DEBUG, "RX beacon SA=" MACSTR " BSSID="
				   MACSTR " TSF=0x%llx BCN=0x%llx diff=%lld "
				   "@%ld",
				   MAC2STR(mgmt->sa), MAC2STR(mgmt->bssid),
				   tsf, timestamp, tsf - timestamp, jiffies);
			last_tsf_debug = jiffies;
		}
#endif /* IEEE80211_IBSS_DEBUG */
	}
#endif

	if (ieee802_11_parse_elems(mgmt->u.beacon.variable, len - baselen,
				   &elems) == ParseFailed)
		invalid = 1;

#if 0 /* FIX */
	if (local->conf.mode == IW_MODE_ADHOC && elems.supp_rates &&
	    os_memcmp(mgmt->bssid, local->bssid, ETH_ALEN) == 0 &&
	    (sta = sta_info_get(local, mgmt->sa))) {
		struct ieee80211_rate *rates;
		size_t num_rates;
		u32 supp_rates, prev_rates;
		int i, j, oper_mode;

		rates = local->curr_rates;
		num_rates = local->num_curr_rates;
		oper_mode = wpa_s->mlme.sta_scanning ?
			local->scan_oper_phymode : local->conf.phymode;
		for (i = 0; i < local->hw->num_modes; i++) {
			struct ieee80211_hw_modes *mode = &local->hw->modes[i];
			if (oper_mode == mode->mode) {
				rates = mode->rates;
				num_rates = mode->num_rates;
				break;
			}
		}

		supp_rates = 0;
		for (i = 0; i < elems.supp_rates_len +
			     elems.ext_supp_rates_len; i++) {
			u8 rate = 0;
			int own_rate;
			if (i < elems.supp_rates_len)
				rate = elems.supp_rates[i];
			else if (elems.ext_supp_rates)
				rate = elems.ext_supp_rates
					[i - elems.supp_rates_len];
			own_rate = 5 * (rate & 0x7f);
			if (oper_mode == MODE_ATHEROS_TURBO)
				own_rate *= 2;
			for (j = 0; j < num_rates; j++)
				if (rates[j].rate == own_rate)
					supp_rates |= BIT(j);
		}

		prev_rates = sta->supp_rates;
		sta->supp_rates &= supp_rates;
		if (sta->supp_rates == 0) {
			/* No matching rates - this should not really happen.
			 * Make sure that at least one rate is marked
			 * supported to avoid issues with TX rate ctrl. */
			sta->supp_rates = wpa_s->mlme.supp_rates_bits;
		}
		if (sta->supp_rates != prev_rates) {
			wpa_printf(MSG_DEBUG, "MLME: updated supp_rates set "
				   "for " MACSTR " based on beacon info "
				   "(0x%x & 0x%x -> 0x%x)",
				   MAC2STR(sta->addr), prev_rates,
				   supp_rates, sta->supp_rates);
		}
		sta_info_release(local, sta);
	}
#endif

	if (elems.ssid == NULL)
		return;

	if (elems.ds_params && elems.ds_params_len == 1)
		channel = elems.ds_params[0];
	else
		channel = rx_status->channel;

	bss = ieee80211_bss_get(wpa_s, mgmt->bssid);
	if (bss == NULL) {
		bss = ieee80211_bss_add(wpa_s, mgmt->bssid);
		if (bss == NULL)
			return;
	} else {
#if 0
		/* TODO: order by RSSI? */
		spin_lock_bh(&local->sta_bss_lock);
		list_move_tail(&bss->list, &local->sta_bss_list);
		spin_unlock_bh(&local->sta_bss_lock);
#endif
	}

	if (bss->probe_resp && beacon) {
		/* Do not allow beacon to override data from Probe Response. */
		return;
	}

	bss->beacon_int = le_to_host16(mgmt->u.beacon.beacon_int);
	bss->capability = le_to_host16(mgmt->u.beacon.capab_info);
	if (elems.ssid && elems.ssid_len <= MAX_SSID_LEN) {
		os_memcpy(bss->ssid, elems.ssid, elems.ssid_len);
		bss->ssid_len = elems.ssid_len;
	}

	bss->supp_rates_len = 0;
	if (elems.supp_rates) {
		clen = IEEE80211_MAX_SUPP_RATES - bss->supp_rates_len;
		if (clen > elems.supp_rates_len)
			clen = elems.supp_rates_len;
		os_memcpy(&bss->supp_rates[bss->supp_rates_len],
			  elems.supp_rates, clen);
		bss->supp_rates_len += clen;
	}
	if (elems.ext_supp_rates) {
		clen = IEEE80211_MAX_SUPP_RATES - bss->supp_rates_len;
		if (clen > elems.ext_supp_rates_len)
			clen = elems.ext_supp_rates_len;
		os_memcpy(&bss->supp_rates[bss->supp_rates_len],
			  elems.ext_supp_rates, clen);
		bss->supp_rates_len += clen;
	}

	if (elems.wpa &&
	    (bss->wpa_ie == NULL || bss->wpa_ie_len != elems.wpa_len ||
	     os_memcmp(bss->wpa_ie, elems.wpa, elems.wpa_len))) {
		os_free(bss->wpa_ie);
		bss->wpa_ie = os_malloc(elems.wpa_len + 2);
		if (bss->wpa_ie) {
			os_memcpy(bss->wpa_ie, elems.wpa - 2,
				  elems.wpa_len + 2);
			bss->wpa_ie_len = elems.wpa_len + 2;
		} else
			bss->wpa_ie_len = 0;
	} else if (!elems.wpa && bss->wpa_ie) {
		os_free(bss->wpa_ie);
		bss->wpa_ie = NULL;
		bss->wpa_ie_len = 0;
	}

	if (elems.rsn &&
	    (bss->rsn_ie == NULL || bss->rsn_ie_len != elems.rsn_len ||
	     os_memcmp(bss->rsn_ie, elems.rsn, elems.rsn_len))) {
		os_free(bss->rsn_ie);
		bss->rsn_ie = os_malloc(elems.rsn_len + 2);
		if (bss->rsn_ie) {
			os_memcpy(bss->rsn_ie, elems.rsn - 2,
				  elems.rsn_len + 2);
			bss->rsn_ie_len = elems.rsn_len + 2;
		} else
			bss->rsn_ie_len = 0;
	} else if (!elems.rsn && bss->rsn_ie) {
		os_free(bss->rsn_ie);
		bss->rsn_ie = NULL;
		bss->rsn_ie_len = 0;
	}

	if (elems.wmm_param &&
	    (bss->wmm_ie == NULL || bss->wmm_ie_len != elems.wmm_param_len ||
	     os_memcmp(bss->wmm_ie, elems.wmm_param, elems.wmm_param_len))) {
		os_free(bss->wmm_ie);
		bss->wmm_ie = os_malloc(elems.wmm_param_len + 2);
		if (bss->wmm_ie) {
			os_memcpy(bss->wmm_ie, elems.wmm_param - 2,
				  elems.wmm_param_len + 2);
			bss->wmm_ie_len = elems.wmm_param_len + 2;
		} else
			bss->wmm_ie_len = 0;
	} else if (!elems.wmm_param && bss->wmm_ie) {
		os_free(bss->wmm_ie);
		bss->wmm_ie = NULL;
		bss->wmm_ie_len = 0;
	}


	bss->hw_mode = wpa_s->mlme.phymode;
	bss->channel = channel;
	bss->freq = wpa_s->mlme.freq;
	if (channel != wpa_s->mlme.channel &&
	    (wpa_s->mlme.phymode == WPA_MODE_IEEE80211G ||
	     wpa_s->mlme.phymode == WPA_MODE_IEEE80211B) &&
	    channel >= 1 && channel <= 14) {
		static const int freq_list[] = {
			2412, 2417, 2422, 2427, 2432, 2437, 2442,
			2447, 2452, 2457, 2462, 2467, 2472, 2484
		};
		/* IEEE 802.11g/b mode can receive packets from neighboring
		 * channels, so map the channel into frequency. */
		bss->freq = freq_list[channel - 1];
	}
	bss->timestamp = timestamp;
	os_get_time(&bss->last_update);
	bss->rssi = rx_status->ssi;
	if (!beacon)
		bss->probe_resp++;
}


static void ieee80211_rx_mgmt_probe_resp(struct wpa_supplicant *wpa_s,
					 struct ieee80211_mgmt *mgmt,
					 size_t len,
					 struct ieee80211_rx_status *rx_status)
{
	ieee80211_bss_info(wpa_s, mgmt, len, rx_status, 0);
}


static void ieee80211_rx_mgmt_beacon(struct wpa_supplicant *wpa_s,
				     struct ieee80211_mgmt *mgmt,
				     size_t len,
				     struct ieee80211_rx_status *rx_status)
{
	int use_protection;
	size_t baselen;
	struct ieee802_11_elems elems;

	ieee80211_bss_info(wpa_s, mgmt, len, rx_status, 1);

	if (!wpa_s->mlme.associated ||
	    os_memcmp(wpa_s->bssid, mgmt->bssid, ETH_ALEN) != 0)
		return;

	/* Process beacon from the current BSS */
	baselen = (u8 *) mgmt->u.beacon.variable - (u8 *) mgmt;
	if (baselen > len)
		return;

	if (ieee802_11_parse_elems(mgmt->u.beacon.variable, len - baselen,
				   &elems) == ParseFailed)
		return;

	use_protection = 0;
	if (elems.erp_info && elems.erp_info_len >= 1) {
		use_protection =
			(elems.erp_info[0] & ERP_INFO_USE_PROTECTION) != 0;
	}

	if (use_protection != !!wpa_s->mlme.use_protection) {
		wpa_printf(MSG_DEBUG, "MLME: CTS protection %s (BSSID=" MACSTR
			   ")",
			   use_protection ? "enabled" : "disabled",
			   MAC2STR(wpa_s->bssid));
		wpa_s->mlme.use_protection = use_protection ? 1 : 0;
		wpa_s->mlme.cts_protect_erp_frames = use_protection;
	}

	if (elems.wmm_param && wpa_s->mlme.wmm_enabled) {
		ieee80211_sta_wmm_params(wpa_s, elems.wmm_param,
					 elems.wmm_param_len);
	}
}


static void ieee80211_rx_mgmt_probe_req(struct wpa_supplicant *wpa_s,
					struct ieee80211_mgmt *mgmt,
					size_t len,
					struct ieee80211_rx_status *rx_status)
{
	int tx_last_beacon, adhoc;
#if 0 /* FIX */
	struct ieee80211_mgmt *resp;
#endif
	u8 *pos, *end;
	struct wpa_ssid *ssid = wpa_s->current_ssid;

	adhoc = ssid && ssid->mode == 1;

	if (!adhoc || wpa_s->mlme.state != IEEE80211_IBSS_JOINED ||
	    len < 24 + 2 || wpa_s->mlme.probe_resp == NULL)
		return;

#if 0 /* FIX */
	if (local->hw->tx_last_beacon)
		tx_last_beacon = local->hw->tx_last_beacon(local->mdev);
	else
#endif
		tx_last_beacon = 1;

#ifdef IEEE80211_IBSS_DEBUG
	wpa_printf(MSG_DEBUG, "MLME: RX ProbeReq SA=" MACSTR " DA=" MACSTR
		   " BSSID=" MACSTR " (tx_last_beacon=%d)",
		   MAC2STR(mgmt->sa), MAC2STR(mgmt->da),
		   MAC2STR(mgmt->bssid), tx_last_beacon);
#endif /* IEEE80211_IBSS_DEBUG */

	if (!tx_last_beacon)
		return;

	if (os_memcmp(mgmt->bssid, wpa_s->bssid, ETH_ALEN) != 0 &&
	    os_memcmp(mgmt->bssid, "\xff\xff\xff\xff\xff\xff", ETH_ALEN) != 0)
		return;

	end = ((u8 *) mgmt) + len;
	pos = mgmt->u.probe_req.variable;
	if (pos[0] != WLAN_EID_SSID ||
	    pos + 2 + pos[1] > end) {
		wpa_printf(MSG_DEBUG, "MLME: Invalid SSID IE in ProbeReq from "
			   MACSTR, MAC2STR(mgmt->sa));
		return;
	}
	if (pos[1] != 0 &&
	    (pos[1] != wpa_s->mlme.ssid_len ||
	     os_memcmp(pos + 2, wpa_s->mlme.ssid, wpa_s->mlme.ssid_len) != 0))
	{
		/* Ignore ProbeReq for foreign SSID */
		return;
	}

#if 0 /* FIX */
	/* Reply with ProbeResp */
	skb = skb_copy(wpa_s->mlme.probe_resp, GFP_ATOMIC);
	if (skb == NULL)
		return;

	resp = (struct ieee80211_mgmt *) skb->data;
	os_memcpy(resp->da, mgmt->sa, ETH_ALEN);
#ifdef IEEE80211_IBSS_DEBUG
	wpa_printf(MSG_DEBUG, "MLME: Sending ProbeResp to " MACSTR,
		   MAC2STR(resp->da));
#endif /* IEEE80211_IBSS_DEBUG */
	ieee80211_sta_tx(wpa_s, skb, 0, 1);
#endif
}


static void ieee80211_sta_rx_mgmt(struct wpa_supplicant *wpa_s,
				  const u8 *buf, size_t len,
				  struct ieee80211_rx_status *rx_status)
{
	struct ieee80211_mgmt *mgmt;
	u16 fc;

	if (len < 24)
		return;

	mgmt = (struct ieee80211_mgmt *) buf;
	fc = le_to_host16(mgmt->frame_control);

	switch (WLAN_FC_GET_STYPE(fc)) {
	case WLAN_FC_STYPE_PROBE_REQ:
		ieee80211_rx_mgmt_probe_req(wpa_s, mgmt, len, rx_status);
		break;
	case WLAN_FC_STYPE_PROBE_RESP:
		ieee80211_rx_mgmt_probe_resp(wpa_s, mgmt, len, rx_status);
		break;
	case WLAN_FC_STYPE_BEACON:
		ieee80211_rx_mgmt_beacon(wpa_s, mgmt, len, rx_status);
		break;
	case WLAN_FC_STYPE_AUTH:
		ieee80211_rx_mgmt_auth(wpa_s, mgmt, len, rx_status);
		break;
	case WLAN_FC_STYPE_ASSOC_RESP:
		ieee80211_rx_mgmt_assoc_resp(wpa_s, mgmt, len, rx_status, 0);
		break;
	case WLAN_FC_STYPE_REASSOC_RESP:
		ieee80211_rx_mgmt_assoc_resp(wpa_s, mgmt, len, rx_status, 1);
		break;
	case WLAN_FC_STYPE_DEAUTH:
		ieee80211_rx_mgmt_deauth(wpa_s, mgmt, len, rx_status);
		break;
	case WLAN_FC_STYPE_DISASSOC:
		ieee80211_rx_mgmt_disassoc(wpa_s, mgmt, len, rx_status);
		break;
	default:
		wpa_printf(MSG_DEBUG, "MLME: received unknown management "
			   "frame - stype=%d", WLAN_FC_GET_STYPE(fc));
		break;
	}
}


static void ieee80211_sta_rx_scan(struct wpa_supplicant *wpa_s,
				  const u8 *buf, size_t len,
				  struct ieee80211_rx_status *rx_status)
{
	struct ieee80211_mgmt *mgmt;
	u16 fc;

	if (len < 24)
		return;

	mgmt = (struct ieee80211_mgmt *) buf;
	fc = le_to_host16(mgmt->frame_control);

	if (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_MGMT) {
		if (WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_PROBE_RESP) {
			ieee80211_rx_mgmt_probe_resp(wpa_s, mgmt,
						     len, rx_status);
		} else if (WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_BEACON) {
			ieee80211_rx_mgmt_beacon(wpa_s, mgmt, len, rx_status);
		}
	}
}


static int ieee80211_sta_active_ibss(struct wpa_supplicant *wpa_s)
{
	int active = 0;

#if 0 /* FIX */
	list_for_each(ptr, &local->sta_list) {
		sta = list_entry(ptr, struct sta_info, list);
		if (sta->dev == dev &&
		    time_after(sta->last_rx + IEEE80211_IBSS_MERGE_INTERVAL,
			       jiffies)) {
			active++;
			break;
		}
	}
#endif

	return active;
}


static void ieee80211_sta_expire(struct wpa_supplicant *wpa_s)
{
#if 0 /* FIX */
	list_for_each_safe(ptr, n, &local->sta_list) {
		sta = list_entry(ptr, struct sta_info, list);
		if (time_after(jiffies, sta->last_rx +
			       IEEE80211_IBSS_INACTIVITY_LIMIT)) {
			wpa_printf(MSG_DEBUG, "MLME: expiring inactive STA "
				   MACSTR, MAC2STR(sta->addr));
			sta_info_free(local, sta, 1);
		}
	}
#endif
}


static void ieee80211_sta_merge_ibss(struct wpa_supplicant *wpa_s)
{
	ieee80211_reschedule_timer(wpa_s, IEEE80211_IBSS_MERGE_INTERVAL);

	ieee80211_sta_expire(wpa_s);
	if (ieee80211_sta_active_ibss(wpa_s))
		return;

	wpa_printf(MSG_DEBUG, "MLME: No active IBSS STAs - trying to scan for "
		   "other IBSS networks with same SSID (merge)");
	ieee80211_sta_req_scan(wpa_s, wpa_s->mlme.ssid, wpa_s->mlme.ssid_len);
}


static void ieee80211_sta_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;

	switch (wpa_s->mlme.state) {
	case IEEE80211_DISABLED:
		break;
	case IEEE80211_AUTHENTICATE:
		ieee80211_authenticate(wpa_s);
		break;
	case IEEE80211_ASSOCIATE:
		ieee80211_associate(wpa_s);
		break;
	case IEEE80211_ASSOCIATED:
		ieee80211_associated(wpa_s);
		break;
	case IEEE80211_IBSS_SEARCH:
		ieee80211_sta_find_ibss(wpa_s);
		break;
	case IEEE80211_IBSS_JOINED:
		ieee80211_sta_merge_ibss(wpa_s);
		break;
	default:
		wpa_printf(MSG_DEBUG, "ieee80211_sta_timer: Unknown state %d",
			   wpa_s->mlme.state);
		break;
	}

	if (ieee80211_privacy_mismatch(wpa_s)) {
		wpa_printf(MSG_DEBUG, "MLME: privacy configuration mismatch "
			   "and mixed-cell disabled - disassociate");

		ieee80211_send_disassoc(wpa_s, WLAN_REASON_UNSPECIFIED);
		ieee80211_set_associated(wpa_s, 0);
	}
}


static void ieee80211_sta_new_auth(struct wpa_supplicant *wpa_s)
{
	struct wpa_ssid *ssid = wpa_s->current_ssid;
	if (ssid && ssid->mode != 0)
		return;

#if 0 /* FIX */
	if (local->hw->reset_tsf) {
		/* Reset own TSF to allow time synchronization work. */
		local->hw->reset_tsf(local->mdev);
	}
#endif

	wpa_s->mlme.wmm_last_param_set = -1; /* allow any WMM update */


	if (wpa_s->mlme.auth_algs & IEEE80211_AUTH_ALG_OPEN)
		wpa_s->mlme.auth_alg = WLAN_AUTH_OPEN;
	else if (wpa_s->mlme.auth_algs & IEEE80211_AUTH_ALG_SHARED_KEY)
		wpa_s->mlme.auth_alg = WLAN_AUTH_SHARED_KEY;
	else if (wpa_s->mlme.auth_algs & IEEE80211_AUTH_ALG_LEAP)
		wpa_s->mlme.auth_alg = WLAN_AUTH_LEAP;
	else
		wpa_s->mlme.auth_alg = WLAN_AUTH_OPEN;
	wpa_printf(MSG_DEBUG, "MLME: Initial auth_alg=%d",
		   wpa_s->mlme.auth_alg);
	wpa_s->mlme.auth_transaction = -1;
	wpa_s->mlme.auth_tries = wpa_s->mlme.assoc_tries = 0;
	ieee80211_authenticate(wpa_s);
}


static int ieee80211_ibss_allowed(struct wpa_supplicant *wpa_s)
{
#if 0 /* FIX */
	int m, c;

	for (m = 0; m < local->hw->num_modes; m++) {
		struct ieee80211_hw_modes *mode = &local->hw->modes[m];
		if (mode->mode != local->conf.phymode)
			continue;
		for (c = 0; c < mode->num_channels; c++) {
			struct ieee80211_channel *chan = &mode->channels[c];
			if (chan->flag & IEEE80211_CHAN_W_SCAN &&
			    chan->chan == local->conf.channel) {
				if (chan->flag & IEEE80211_CHAN_W_IBSS)
					return 1;
				break;
			}
		}
	}
#endif

	return 0;
}


static int ieee80211_sta_join_ibss(struct wpa_supplicant *wpa_s,
				   struct ieee80211_sta_bss *bss)
{
	int res = 0, rates, done = 0;
	struct ieee80211_mgmt *mgmt;
#if 0 /* FIX */
	struct ieee80211_tx_control control;
	struct ieee80211_rate *rate;
	struct rate_control_extra extra;
#endif
	u8 *pos, *buf;
	size_t len;

	/* Remove possible STA entries from other IBSS networks. */
#if 0 /* FIX */
	sta_info_flush(local, NULL);

	if (local->hw->reset_tsf) {
		/* Reset own TSF to allow time synchronization work. */
		local->hw->reset_tsf(local->mdev);
	}
#endif
	os_memcpy(wpa_s->bssid, bss->bssid, ETH_ALEN);

#if 0 /* FIX */
	local->conf.beacon_int = bss->beacon_int >= 10 ? bss->beacon_int : 10;

	sdata->drop_unencrypted = bss->capability &
		host_to_le16(WLAN_CAPABILITY_PRIVACY) ? 1 : 0;
#endif

#if 0 /* FIX */
	os_memset(&rq, 0, sizeof(rq));
	rq.m = bss->freq * 100000;
	rq.e = 1;
	res = ieee80211_ioctl_siwfreq(wpa_s, NULL, &rq, NULL);
#endif

	if (!ieee80211_ibss_allowed(wpa_s)) {
#if 0 /* FIX */
		wpa_printf(MSG_DEBUG, "MLME: IBSS not allowed on channel %d "
			   "(%d MHz)", local->conf.channel,
			   local->conf.freq);
#endif
		return -1;
	}

	/* Set beacon template based on scan results */
	buf = os_malloc(400);
	len = 0;
	do {
		if (buf == NULL)
			break;

		mgmt = (struct ieee80211_mgmt *) buf;
		len += 24 + sizeof(mgmt->u.beacon);
		os_memset(mgmt, 0, 24 + sizeof(mgmt->u.beacon));
		mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
						   WLAN_FC_STYPE_BEACON);
		os_memset(mgmt->da, 0xff, ETH_ALEN);
		os_memcpy(mgmt->sa, wpa_s->own_addr, ETH_ALEN);
		os_memcpy(mgmt->bssid, wpa_s->bssid, ETH_ALEN);
#if 0 /* FIX */
		mgmt->u.beacon.beacon_int =
			host_to_le16(local->conf.beacon_int);
#endif
		mgmt->u.beacon.capab_info = host_to_le16(bss->capability);

		pos = buf + len;
		len += 2 + wpa_s->mlme.ssid_len;
		*pos++ = WLAN_EID_SSID;
		*pos++ = wpa_s->mlme.ssid_len;
		os_memcpy(pos, wpa_s->mlme.ssid, wpa_s->mlme.ssid_len);

		rates = bss->supp_rates_len;
		if (rates > 8)
			rates = 8;
		pos = buf + len;
		len += 2 + rates;
		*pos++ = WLAN_EID_SUPP_RATES;
		*pos++ = rates;
		os_memcpy(pos, bss->supp_rates, rates);

		pos = buf + len;
		len += 2 + 1;
		*pos++ = WLAN_EID_DS_PARAMS;
		*pos++ = 1;
		*pos++ = bss->channel;

		pos = buf + len;
		len += 2 + 2;
		*pos++ = WLAN_EID_IBSS_PARAMS;
		*pos++ = 2;
		/* FIX: set ATIM window based on scan results */
		*pos++ = 0;
		*pos++ = 0;

		if (bss->supp_rates_len > 8) {
			rates = bss->supp_rates_len - 8;
			pos = buf + len;
			len += 2 + rates;
			*pos++ = WLAN_EID_EXT_SUPP_RATES;
			*pos++ = rates;
			os_memcpy(pos, &bss->supp_rates[8], rates);
		}

#if 0 /* FIX */
		os_memset(&control, 0, sizeof(control));
		control.pkt_type = PKT_PROBE_RESP;
		os_memset(&extra, 0, sizeof(extra));
		extra.endidx = local->num_curr_rates;
		rate = rate_control_get_rate(wpa_s, skb, &extra);
		if (rate == NULL) {
			wpa_printf(MSG_DEBUG, "MLME: Failed to determine TX "
				   "rate for IBSS beacon");
			break;
		}
		control.tx_rate = (wpa_s->mlme.short_preamble &&
				   (rate->flags & IEEE80211_RATE_PREAMBLE2)) ?
			rate->val2 : rate->val;
		control.antenna_sel = local->conf.antenna_sel;
		control.power_level = local->conf.power_level;
		control.no_ack = 1;
		control.retry_limit = 1;
		control.rts_cts_duration = 0;
#endif

#if 0 /* FIX */
		wpa_s->mlme.probe_resp = skb_copy(skb, GFP_ATOMIC);
		if (wpa_s->mlme.probe_resp) {
			mgmt = (struct ieee80211_mgmt *)
				wpa_s->mlme.probe_resp->data;
			mgmt->frame_control =
				IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					     WLAN_FC_STYPE_PROBE_RESP);
		} else {
			wpa_printf(MSG_DEBUG, "MLME: Could not allocate "
				   "ProbeResp template for IBSS");
		}

		if (local->hw->beacon_update &&
		    local->hw->beacon_update(wpa_s, skb, &control) == 0) {
			wpa_printf(MSG_DEBUG, "MLME: Configured IBSS beacon "
				   "template based on scan results");
			skb = NULL;
		}

		rates = 0;
		for (i = 0; i < bss->supp_rates_len; i++) {
			int rate = (bss->supp_rates[i] & 0x7f) * 5;
			if (local->conf.phymode == MODE_ATHEROS_TURBO)
				rate *= 2;
			for (j = 0; j < local->num_curr_rates; j++)
				if (local->curr_rates[j].rate == rate)
					rates |= BIT(j);
		}
		wpa_s->mlme.supp_rates_bits = rates;
#endif
		done = 1;
	} while (0);

	os_free(buf);
	if (!done) {
		wpa_printf(MSG_DEBUG, "MLME: Failed to configure IBSS beacon "
			   "template");
	}

	wpa_s->mlme.state = IEEE80211_IBSS_JOINED;
	ieee80211_reschedule_timer(wpa_s, IEEE80211_IBSS_MERGE_INTERVAL);

	return res;
}


#if 0 /* FIX */
static int ieee80211_sta_create_ibss(struct wpa_supplicant *wpa_s)
{
	struct ieee80211_sta_bss *bss;
	u8 bssid[ETH_ALEN], *pos;
	int i;

#if 0
	/* Easier testing, use fixed BSSID. */
	os_memset(bssid, 0xfe, ETH_ALEN);
#else
	/* Generate random, not broadcast, locally administered BSSID. Mix in
	 * own MAC address to make sure that devices that do not have proper
	 * random number generator get different BSSID. */
	os_get_random(bssid, ETH_ALEN);
	for (i = 0; i < ETH_ALEN; i++)
		bssid[i] ^= wpa_s->own_addr[i];
	bssid[0] &= ~0x01;
	bssid[0] |= 0x02;
#endif

	wpa_printf(MSG_DEBUG, "MLME: Creating new IBSS network, BSSID "
		   MACSTR "", MAC2STR(bssid));

	bss = ieee80211_bss_add(wpa_s, bssid);
	if (bss == NULL)
		return -ENOMEM;

#if 0 /* FIX */
	if (local->conf.beacon_int == 0)
		local->conf.beacon_int = 100;
	bss->beacon_int = local->conf.beacon_int;
	bss->hw_mode = local->conf.phymode;
	bss->channel = local->conf.channel;
	bss->freq = local->conf.freq;
#endif
	os_get_time(&bss->last_update);
	bss->capability = host_to_le16(WLAN_CAPABILITY_IBSS);
#if 0 /* FIX */
	if (sdata->default_key) {
		bss->capability |= host_to_le16(WLAN_CAPABILITY_PRIVACY);
	} else
		sdata->drop_unencrypted = 0;
	bss->supp_rates_len = local->num_curr_rates;
#endif
	pos = bss->supp_rates;
#if 0 /* FIX */
	for (i = 0; i < local->num_curr_rates; i++) {
		int rate = local->curr_rates[i].rate;
		if (local->conf.phymode == MODE_ATHEROS_TURBO)
			rate /= 2;
		*pos++ = (u8) (rate / 5);
	}
#endif

	return ieee80211_sta_join_ibss(wpa_s, bss);
}
#endif


static int ieee80211_sta_find_ibss(struct wpa_supplicant *wpa_s)
{
	struct ieee80211_sta_bss *bss;
	int found = 0;
	u8 bssid[ETH_ALEN];
	int active_ibss;
	struct os_time now;

	if (wpa_s->mlme.ssid_len == 0)
		return -EINVAL;

	active_ibss = ieee80211_sta_active_ibss(wpa_s);
#ifdef IEEE80211_IBSS_DEBUG
	wpa_printf(MSG_DEBUG, "MLME: sta_find_ibss (active_ibss=%d)",
		   active_ibss);
#endif /* IEEE80211_IBSS_DEBUG */
	for (bss = wpa_s->mlme.sta_bss_list; bss; bss = bss->next) {
		if (wpa_s->mlme.ssid_len != bss->ssid_len ||
		    os_memcmp(wpa_s->mlme.ssid, bss->ssid, bss->ssid_len) != 0
		    || !(bss->capability & WLAN_CAPABILITY_IBSS))
			continue;
#ifdef IEEE80211_IBSS_DEBUG
		wpa_printf(MSG_DEBUG, "   bssid=" MACSTR " found",
			   MAC2STR(bss->bssid));
#endif /* IEEE80211_IBSS_DEBUG */
		os_memcpy(bssid, bss->bssid, ETH_ALEN);
		found = 1;
		if (active_ibss ||
		    os_memcmp(bssid, wpa_s->bssid, ETH_ALEN) != 0)
			break;
	}

#ifdef IEEE80211_IBSS_DEBUG
	wpa_printf(MSG_DEBUG, "   sta_find_ibss: selected " MACSTR " current "
		   MACSTR, MAC2STR(bssid), MAC2STR(wpa_s->bssid));
#endif /* IEEE80211_IBSS_DEBUG */
	if (found && os_memcmp(wpa_s->bssid, bssid, ETH_ALEN) != 0 &&
	    (bss = ieee80211_bss_get(wpa_s, bssid))) {
		wpa_printf(MSG_DEBUG, "MLME: Selected IBSS BSSID " MACSTR
			   " based on configured SSID",
			   MAC2STR(bssid));
		return ieee80211_sta_join_ibss(wpa_s, bss);
	}
#ifdef IEEE80211_IBSS_DEBUG
	wpa_printf(MSG_DEBUG, "   did not try to join ibss");
#endif /* IEEE80211_IBSS_DEBUG */

	/* Selected IBSS not found in current scan results - try to scan */
	os_get_time(&now);
#if 0 /* FIX */
	if (wpa_s->mlme.state == IEEE80211_IBSS_JOINED &&
	    !ieee80211_sta_active_ibss(wpa_s)) {
		ieee80211_reschedule_timer(wpa_s,
					   IEEE80211_IBSS_MERGE_INTERVAL);
	} else if (time_after(jiffies, wpa_s->mlme.last_scan_completed +
			      IEEE80211_SCAN_INTERVAL)) {
		wpa_printf(MSG_DEBUG, "MLME: Trigger new scan to find an IBSS "
			   "to join");
		return ieee80211_sta_req_scan(wpa_s->mlme.ssid,
					      wpa_s->mlme.ssid_len);
	} else if (wpa_s->mlme.state != IEEE80211_IBSS_JOINED) {
		int interval = IEEE80211_SCAN_INTERVAL;

		if (time_after(jiffies, wpa_s->mlme.ibss_join_req +
			       IEEE80211_IBSS_JOIN_TIMEOUT)) {
			if (wpa_s->mlme.create_ibss &&
			    ieee80211_ibss_allowed(wpa_s))
				return ieee80211_sta_create_ibss(wpa_s);
			if (wpa_s->mlme.create_ibss) {
				wpa_printf(MSG_DEBUG, "MLME: IBSS not allowed "
					   "on the configured channel %d "
					   "(%d MHz)",
					   local->conf.channel,
					   local->conf.freq);
			}

			/* No IBSS found - decrease scan interval and continue
			 * scanning. */
			interval = IEEE80211_SCAN_INTERVAL_SLOW;
		}

		wpa_s->mlme.state = IEEE80211_IBSS_SEARCH;
		ieee80211_reschedule_timer(wpa_s, interval);
		return 0;
	}
#endif

	return 0;
}


int ieee80211_sta_get_ssid(struct wpa_supplicant *wpa_s, u8 *ssid,
			   size_t *len)
{
	os_memcpy(ssid, wpa_s->mlme.ssid, wpa_s->mlme.ssid_len);
	*len = wpa_s->mlme.ssid_len;
	return 0;
}


int ieee80211_sta_associate(struct wpa_supplicant *wpa_s,
			    struct wpa_driver_associate_params *params)
{
	struct ieee80211_sta_bss *bss;

	wpa_s->mlme.bssid_set = 0;
	wpa_s->mlme.freq = params->freq;
	if (params->bssid) {
		os_memcpy(wpa_s->bssid, params->bssid, ETH_ALEN);
		if (os_memcmp(params->bssid, "\x00\x00\x00\x00\x00\x00",
			      ETH_ALEN))
			wpa_s->mlme.bssid_set = 1;
		bss = ieee80211_bss_get(wpa_s, wpa_s->bssid);
		if (bss) {
			wpa_s->mlme.phymode = bss->hw_mode;
			wpa_s->mlme.channel = bss->channel;
			wpa_s->mlme.freq = bss->freq;
		}
	}

#if 0 /* FIX */
	/* TODO: This should always be done for IBSS, even if IEEE80211_QOS is
	 * not defined. */
	if (local->hw->conf_tx) {
		struct ieee80211_tx_queue_params qparam;
		int i;

		os_memset(&qparam, 0, sizeof(qparam));
		/* TODO: are these ok defaults for all hw_modes? */
		qparam.aifs = 2;
		qparam.cw_min =
			local->conf.phymode == MODE_IEEE80211B ? 31 : 15;
		qparam.cw_max = 1023;
		qparam.burst_time = 0;
		for (i = IEEE80211_TX_QUEUE_DATA0; i < NUM_TX_DATA_QUEUES; i++)
		{
			local->hw->conf_tx(wpa_s, i + IEEE80211_TX_QUEUE_DATA0,
					   &qparam);
		}
		/* IBSS uses different parameters for Beacon sending */
		qparam.cw_min++;
		qparam.cw_min *= 2;
		qparam.cw_min--;
		local->hw->conf_tx(wpa_s, IEEE80211_TX_QUEUE_BEACON, &qparam);
	}
#endif

	if (wpa_s->mlme.ssid_len != params->ssid_len ||
	    os_memcmp(wpa_s->mlme.ssid, params->ssid, params->ssid_len) != 0)
		wpa_s->mlme.prev_bssid_set = 0;
	os_memcpy(wpa_s->mlme.ssid, params->ssid, params->ssid_len);
	os_memset(wpa_s->mlme.ssid + params->ssid_len, 0,
		  MAX_SSID_LEN - params->ssid_len);
	wpa_s->mlme.ssid_len = params->ssid_len;
	wpa_s->mlme.ssid_set = 1;

	os_free(wpa_s->mlme.extra_ie);
	if (params->wpa_ie == NULL || params->wpa_ie_len == 0) {
		wpa_s->mlme.extra_ie = NULL;
		wpa_s->mlme.extra_ie_len = 0;
		return 0;
	}
	wpa_s->mlme.extra_ie = os_malloc(params->wpa_ie_len);
	if (wpa_s->mlme.extra_ie == NULL) {
		wpa_s->mlme.extra_ie_len = 0;
		return -1;
	}
	os_memcpy(wpa_s->mlme.extra_ie, params->wpa_ie, params->wpa_ie_len);
	wpa_s->mlme.extra_ie_len = params->wpa_ie_len;

	wpa_s->mlme.key_mgmt = params->key_mgmt_suite;

	ieee80211_sta_set_channel(wpa_s, wpa_s->mlme.phymode,
				  wpa_s->mlme.channel, wpa_s->mlme.freq);

	if (params->mode == 1 && !wpa_s->mlme.bssid_set) {
		os_get_time(&wpa_s->mlme.ibss_join_req);
		wpa_s->mlme.state = IEEE80211_IBSS_SEARCH;
		return ieee80211_sta_find_ibss(wpa_s);
	}

	if (wpa_s->mlme.bssid_set)
		ieee80211_sta_new_auth(wpa_s);

	return 0;
}


static void ieee80211_sta_save_oper_chan(struct wpa_supplicant *wpa_s)
{
	wpa_s->mlme.scan_oper_channel = wpa_s->mlme.channel;
	wpa_s->mlme.scan_oper_freq = wpa_s->mlme.freq;
	wpa_s->mlme.scan_oper_phymode = wpa_s->mlme.phymode;
}


static int ieee80211_sta_restore_oper_chan(struct wpa_supplicant *wpa_s)
{
	wpa_s->mlme.channel = wpa_s->mlme.scan_oper_channel;
	wpa_s->mlme.freq = wpa_s->mlme.scan_oper_freq;
	wpa_s->mlme.phymode = wpa_s->mlme.scan_oper_phymode;
	if (wpa_s->mlme.freq == 0)
		return 0;
	return ieee80211_sta_set_channel(wpa_s, wpa_s->mlme.phymode,
					 wpa_s->mlme.channel,
					 wpa_s->mlme.freq);
}


static int ieee80211_active_scan(struct wpa_supplicant *wpa_s)
{
	size_t m;
	int c;

	for (m = 0; m < wpa_s->mlme.num_modes; m++) {
		struct wpa_hw_modes *mode = &wpa_s->mlme.modes[m];
		if ((int) mode->mode != (int) wpa_s->mlme.phymode)
			continue;
		for (c = 0; c < mode->num_channels; c++) {
			struct wpa_channel_data *chan = &mode->channels[c];
			if (chan->flag & WPA_CHAN_W_SCAN &&
			    chan->chan == wpa_s->mlme.channel) {
				if (chan->flag & WPA_CHAN_W_ACTIVE_SCAN)
					return 1;
				break;
			}
		}
	}

	return 0;
}


static void ieee80211_sta_scan_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct wpa_hw_modes *mode;
	struct wpa_channel_data *chan;
	int skip = 0;
	int timeout = 0;
	struct wpa_ssid *ssid = wpa_s->current_ssid;
	int adhoc;

	if (!wpa_s->mlme.sta_scanning || wpa_s->mlme.modes == NULL)
		return;

	adhoc = ssid && ssid->mode == 1;

	switch (wpa_s->mlme.scan_state) {
	case SCAN_SET_CHANNEL:
		mode = &wpa_s->mlme.modes[wpa_s->mlme.scan_hw_mode_idx];
		if (wpa_s->mlme.scan_hw_mode_idx >=
		    (int) wpa_s->mlme.num_modes ||
		    (wpa_s->mlme.scan_hw_mode_idx + 1 ==
		     (int) wpa_s->mlme.num_modes
		     && wpa_s->mlme.scan_channel_idx >= mode->num_channels)) {
			if (ieee80211_sta_restore_oper_chan(wpa_s)) {
				wpa_printf(MSG_DEBUG, "MLME: failed to "
					   "restore operational channel after "
					   "scan");
			}
			wpa_printf(MSG_DEBUG, "MLME: scan completed");
			wpa_s->mlme.sta_scanning = 0;
			os_get_time(&wpa_s->mlme.last_scan_completed);
			wpa_supplicant_event(wpa_s, EVENT_SCAN_RESULTS, NULL);
			if (adhoc) {
				if (!wpa_s->mlme.bssid_set ||
				    (wpa_s->mlme.state ==
				     IEEE80211_IBSS_JOINED &&
				     !ieee80211_sta_active_ibss(wpa_s)))
					ieee80211_sta_find_ibss(wpa_s);
			}
			return;
		}
		skip = !(wpa_s->mlme.hw_modes & (1 << mode->mode));
		chan = &mode->channels[wpa_s->mlme.scan_channel_idx];
		if (!(chan->flag & WPA_CHAN_W_SCAN) ||
		    (adhoc && !(chan->flag & WPA_CHAN_W_IBSS)) ||
		    (wpa_s->mlme.hw_modes & (1 << WPA_MODE_IEEE80211G) &&
		     mode->mode == WPA_MODE_IEEE80211B &&
		     wpa_s->mlme.scan_skip_11b))
			skip = 1;

		if (!skip) {
			wpa_printf(MSG_MSGDUMP,
				   "MLME: scan channel %d (%d MHz)",
				   chan->chan, chan->freq);

			wpa_s->mlme.channel = chan->chan;
			wpa_s->mlme.freq = chan->freq;
			wpa_s->mlme.phymode = mode->mode;
			if (ieee80211_sta_set_channel(wpa_s, mode->mode,
						      chan->chan, chan->freq))
			{
				wpa_printf(MSG_DEBUG, "MLME: failed to set "
					   "channel %d (%d MHz) for scan",
					   chan->chan, chan->freq);
				skip = 1;
			}
		}

		wpa_s->mlme.scan_channel_idx++;
		if (wpa_s->mlme.scan_channel_idx >=
		    wpa_s->mlme.modes[wpa_s->mlme.scan_hw_mode_idx].
		    num_channels) {
			wpa_s->mlme.scan_hw_mode_idx++;
			wpa_s->mlme.scan_channel_idx = 0;
		}

		if (skip) {
			timeout = 0;
			break;
		}

		timeout = IEEE80211_PROBE_DELAY;
		wpa_s->mlme.scan_state = SCAN_SEND_PROBE;
		break;
	case SCAN_SEND_PROBE:
		if (ieee80211_active_scan(wpa_s)) {
			ieee80211_send_probe_req(wpa_s, NULL,
						 wpa_s->mlme.scan_ssid,
						 wpa_s->mlme.scan_ssid_len);
			timeout = IEEE80211_CHANNEL_TIME;
		} else {
			timeout = IEEE80211_PASSIVE_CHANNEL_TIME;
		}
		wpa_s->mlme.scan_state = SCAN_SET_CHANNEL;
		break;
	}

	eloop_register_timeout(timeout / 1000, 1000 * (timeout % 1000),
			       ieee80211_sta_scan_timer, wpa_s, NULL);
}


int ieee80211_sta_req_scan(struct wpa_supplicant *wpa_s, const u8 *ssid,
			   size_t ssid_len)
{
	if (ssid_len > MAX_SSID_LEN)
		return -1;

	/* MLME-SCAN.request (page 118)  page 144 (11.1.3.1)
	 * BSSType: INFRASTRUCTURE, INDEPENDENT, ANY_BSS
	 * BSSID: MACAddress
	 * SSID
	 * ScanType: ACTIVE, PASSIVE
	 * ProbeDelay: delay (in microseconds) to be used prior to transmitting
	 *    a Probe frame during active scanning
	 * ChannelList
	 * MinChannelTime (>= ProbeDelay), in TU
	 * MaxChannelTime: (>= MinChannelTime), in TU
	 */

	 /* MLME-SCAN.confirm
	  * BSSDescriptionSet
	  * ResultCode: SUCCESS, INVALID_PARAMETERS
	 */

	/* TODO: if assoc, move to power save mode for the duration of the
	 * scan */

	if (wpa_s->mlme.sta_scanning)
		return -1;

	wpa_printf(MSG_DEBUG, "MLME: starting scan");

	ieee80211_sta_save_oper_chan(wpa_s);

	wpa_s->mlme.sta_scanning = 1;
	/* TODO: stop TX queue? */

	if (ssid) {
		wpa_s->mlme.scan_ssid_len = ssid_len;
		os_memcpy(wpa_s->mlme.scan_ssid, ssid, ssid_len);
	} else
		wpa_s->mlme.scan_ssid_len = 0;
	wpa_s->mlme.scan_skip_11b = 1; /* FIX: clear this is 11g is not
					* supported */
	wpa_s->mlme.scan_state = SCAN_SET_CHANNEL;
	wpa_s->mlme.scan_hw_mode_idx = 0;
	wpa_s->mlme.scan_channel_idx = 0;
	eloop_register_timeout(0, 1, ieee80211_sta_scan_timer, wpa_s, NULL);

	return 0;
}


int ieee80211_sta_get_scan_results(struct wpa_supplicant *wpa_s,
				   struct wpa_scan_result *results,
				   size_t max_size)
{
	size_t ap_num = 0;
	struct wpa_scan_result *r;
	struct ieee80211_sta_bss *bss;

	os_memset(results, 0, max_size * sizeof(struct wpa_scan_result));
	for (bss = wpa_s->mlme.sta_bss_list; bss; bss = bss->next) {
		r = &results[ap_num];
		os_memcpy(r->bssid, bss->bssid, ETH_ALEN);
		os_memcpy(r->ssid, bss->ssid, bss->ssid_len);
		r->ssid_len = bss->ssid_len;
		if (bss->wpa_ie && bss->wpa_ie_len < SSID_MAX_WPA_IE_LEN) {
			os_memcpy(r->wpa_ie, bss->wpa_ie, bss->wpa_ie_len);
			r->wpa_ie_len = bss->wpa_ie_len;
		}
		if (bss->rsn_ie && bss->rsn_ie_len < SSID_MAX_WPA_IE_LEN) {
			os_memcpy(r->rsn_ie, bss->rsn_ie, bss->rsn_ie_len);
			r->rsn_ie_len = bss->rsn_ie_len;
		}
		r->freq = bss->freq;
		r->caps = bss->capability;
		r->level = bss->rssi;

		ap_num++;
		if (ap_num >= max_size)
			break;
	}

	return ap_num;
}


#if 0 /* FIX */
struct sta_info * ieee80211_ibss_add_sta(struct wpa_supplicant *wpa_s,
					 struct sk_buff *skb, u8 *bssid,
					 u8 *addr)
{
	struct ieee80211_local *local = dev->priv;
	struct list_head *ptr;
	struct sta_info *sta;
	struct wpa_supplicant *sta_dev = NULL;

	/* TODO: Could consider removing the least recently used entry and
	 * allow new one to be added. */
	if (local->num_sta >= IEEE80211_IBSS_MAX_STA_ENTRIES) {
		if (net_ratelimit()) {
			wpa_printf(MSG_DEBUG, "MLME: No room for a new IBSS "
				   "STA entry " MACSTR, MAC2STR(addr));
		}
		return NULL;
	}

	spin_lock_bh(&local->sub_if_lock);
	list_for_each(ptr, &local->sub_if_list) {
		sdata = list_entry(ptr, struct ieee80211_sub_if_data, list);
		if (sdata->type == IEEE80211_SUB_IF_TYPE_STA &&
		    os_memcmp(bssid, sdata->u.sta.bssid, ETH_ALEN) == 0) {
			sta_dev = sdata->dev;
			break;
		}
	}
	spin_unlock_bh(&local->sub_if_lock);

	if (sta_dev == NULL)
		return NULL;

	wpa_printf(MSG_DEBUG, "MLME: Adding new IBSS station " MACSTR
		   " (dev=%s)", MAC2STR(addr), sta_dev->name);

	sta = sta_info_add(wpa_s, addr);
	if (sta == NULL) {
		return NULL;
	}

	sta->dev = sta_dev;
	sta->supp_rates = wpa_s->mlme.supp_rates_bits;

	rate_control_rate_init(local, sta);

	return sta; /* caller will call sta_info_release() */
}
#endif


int ieee80211_sta_deauthenticate(struct wpa_supplicant *wpa_s, u16 reason)
{
	wpa_printf(MSG_DEBUG, "MLME: deauthenticate(reason=%d)", reason);

	ieee80211_send_deauth(wpa_s, reason);
	ieee80211_set_associated(wpa_s, 0);
	return 0;
}


int ieee80211_sta_disassociate(struct wpa_supplicant *wpa_s, u16 reason)
{
	wpa_printf(MSG_DEBUG, "MLME: disassociate(reason=%d)", reason);

	if (!wpa_s->mlme.associated)
		return -1;

	ieee80211_send_disassoc(wpa_s, reason);
	ieee80211_set_associated(wpa_s, 0);
	return 0;
}


void ieee80211_sta_rx(struct wpa_supplicant *wpa_s, const u8 *buf, size_t len,
		      struct ieee80211_rx_status *rx_status)
{
	struct ieee80211_mgmt *mgmt;
	u16 fc;
	const u8 *pos;

	/* wpa_hexdump(MSG_MSGDUMP, "MLME: Received frame", buf, len); */

	if (wpa_s->mlme.sta_scanning) {
		ieee80211_sta_rx_scan(wpa_s, buf, len, rx_status);
		return;
	}

	if (len < 24)
		return;

	mgmt = (struct ieee80211_mgmt *) buf;
	fc = le_to_host16(mgmt->frame_control);

	if (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_MGMT)
		ieee80211_sta_rx_mgmt(wpa_s, buf, len, rx_status);
	else if (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_DATA) {
		if ((fc & (WLAN_FC_TODS | WLAN_FC_FROMDS)) !=
		    WLAN_FC_FROMDS)
			return;
		/* mgmt->sa is actually BSSID for FromDS data frames */
		if (os_memcmp(mgmt->sa, wpa_s->bssid, ETH_ALEN) != 0)
			return;
		/* Skip IEEE 802.11 and LLC headers */
		pos = buf + 24 + 6;
		if (WPA_GET_BE16(pos) != ETH_P_EAPOL)
			return;
		pos += 2;
		/* mgmt->bssid is actually BSSID for SA data frames */
		wpa_supplicant_rx_eapol(wpa_s, mgmt->bssid,
					pos, buf + len - pos);
	}
}


void ieee80211_sta_free_hw_features(struct wpa_hw_modes *hw_features,
				    size_t num_hw_features)
{
	size_t i;

	if (hw_features == NULL)
		return;

	for (i = 0; i < num_hw_features; i++) {
		os_free(hw_features[i].channels);
		os_free(hw_features[i].rates);
	}

	os_free(hw_features);
}


int ieee80211_sta_init(struct wpa_supplicant *wpa_s)
{
	u16 num_modes, flags;

	wpa_s->mlme.modes = wpa_drv_get_hw_feature_data(wpa_s, &num_modes,
							&flags);
	if (wpa_s->mlme.modes == NULL) {
		wpa_printf(MSG_ERROR, "MLME: Failed to read supported "
			   "channels and rates from the driver");
		return -1;
	}

	wpa_s->mlme.num_modes = num_modes;

	wpa_s->mlme.hw_modes = 1 << WPA_MODE_IEEE80211A;
	wpa_s->mlme.hw_modes |= 1 << WPA_MODE_IEEE80211B;
	wpa_s->mlme.hw_modes |= 1 << WPA_MODE_IEEE80211G;

	return 0;
}


void ieee80211_sta_deinit(struct wpa_supplicant *wpa_s)
{
	eloop_cancel_timeout(ieee80211_sta_timer, wpa_s, NULL);
	eloop_cancel_timeout(ieee80211_sta_scan_timer, wpa_s, NULL);
	os_free(wpa_s->mlme.extra_ie);
	wpa_s->mlme.extra_ie = NULL;
	os_free(wpa_s->mlme.assocreq_ies);
	wpa_s->mlme.assocreq_ies = NULL;
	os_free(wpa_s->mlme.assocresp_ies);
	wpa_s->mlme.assocresp_ies = NULL;
	ieee80211_bss_list_deinit(wpa_s);
	ieee80211_sta_free_hw_features(wpa_s->mlme.modes,
				       wpa_s->mlme.num_modes);
}
