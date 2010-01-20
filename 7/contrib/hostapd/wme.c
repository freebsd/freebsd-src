/*
 * hostapd / WMM (Wi-Fi Multimedia)
 * Copyright 2002-2003, Instant802 Networks, Inc.
 * Copyright 2005-2006, Devicescape Software, Inc.
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

#include "hostapd.h"
#include "ieee802_11.h"
#include "wme.h"
#include "sta_info.h"
#include "driver.h"


/* TODO: maintain separate sequence and fragment numbers for each AC
 * TODO: IGMP snooping to track which multicasts to forward - and use QOS-DATA
 * if only WME stations are receiving a certain group */


static u8 wme_oui[3] = { 0x00, 0x50, 0xf2 };


/* Add WME Parameter Element to Beacon and Probe Response frames. */
u8 * hostapd_eid_wme(struct hostapd_data *hapd, u8 *eid)
{
	u8 *pos = eid;
	struct wme_parameter_element *wme =
		(struct wme_parameter_element *) (pos + 2);
	int e;

	if (!hapd->conf->wme_enabled)
		return eid;
	eid[0] = WLAN_EID_VENDOR_SPECIFIC;
	wme->oui[0] = 0x00;
	wme->oui[1] = 0x50;
	wme->oui[2] = 0xf2;
	wme->oui_type = WME_OUI_TYPE;
	wme->oui_subtype = WME_OUI_SUBTYPE_PARAMETER_ELEMENT;
	wme->version = WME_VERSION;
	wme->acInfo = hapd->parameter_set_count & 0xf;

	/* fill in a parameter set record for each AC */
	for (e = 0; e < 4; e++) {
		struct wme_ac_parameter *ac = &wme->ac[e];
		struct hostapd_wme_ac_params *acp =
			&hapd->iconf->wme_ac_params[e];

		ac->aifsn = acp->aifs;
		ac->acm = acp->admission_control_mandatory;
		ac->aci = e;
		ac->reserved = 0;
		ac->eCWmin = acp->cwmin;
		ac->eCWmax = acp->cwmax;
		ac->txopLimit = host_to_le16(acp->txopLimit);
	}

	pos = (u8 *) (wme + 1);
	eid[1] = pos - eid - 2; /* element length */

	return pos;
}


/* This function is called when a station sends an association request with
 * WME info element. The function returns zero on success or non-zero on any
 * error in WME element. eid does not include Element ID and Length octets. */
int hostapd_eid_wme_valid(struct hostapd_data *hapd, u8 *eid, size_t len)
{
	struct wme_information_element *wme;

	wpa_hexdump(MSG_MSGDUMP, "WME IE", eid, len);

	if (len < sizeof(struct wme_information_element)) {
		printf("Too short WME IE (len=%lu)\n", (unsigned long) len);
		return -1;
	}

	wme = (struct wme_information_element *) eid;
	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "Validating WME IE: OUI "
		      "%02x:%02x:%02x  OUI type %d  OUI sub-type %d  "
		      "version %d\n",
		      wme->oui[0], wme->oui[1], wme->oui[2], wme->oui_type,
		      wme->oui_subtype, wme->version);
	if (memcmp(wme->oui, wme_oui, sizeof(wme_oui)) != 0 ||
	    wme->oui_type != WME_OUI_TYPE ||
	    wme->oui_subtype != WME_OUI_SUBTYPE_INFORMATION_ELEMENT ||
	    wme->version != WME_VERSION) {
		printf("Unsupported WME IE OUI/Type/Subtype/Version\n");
		return -1;
	}

	return 0;
}


/* This function is called when a station sends an ACK frame for an AssocResp
 * frame (status=success) and the matching AssocReq contained a WME element.
 */
int hostapd_wme_sta_config(struct hostapd_data *hapd, struct sta_info *sta)
{
	/* update kernel STA data for WME related items (WLAN_STA_WPA flag) */
	if (sta->flags & WLAN_STA_WME)
		hostapd_sta_set_flags(hapd, sta->addr, WLAN_STA_WME, ~0);
	else
		hostapd_sta_set_flags(hapd, sta->addr, 0, ~WLAN_STA_WME);

	return 0;
}


static void wme_send_action(struct hostapd_data *hapd, const u8 *addr,
			    const struct wme_tspec_info_element *tspec,
			    u8 action_code, u8 dialogue_token, u8 status_code)
{
	u8 buf[256];
	struct ieee80211_mgmt *m = (struct ieee80211_mgmt *) buf;
	struct wme_tspec_info_element *t =
		(struct wme_tspec_info_element *)
		m->u.action.u.wme_action.variable;
	int len;

	hostapd_logger(hapd, addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG,
		       "action response - reason %d", status_code);
	memset(buf, 0, sizeof(buf));
	m->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					WLAN_FC_STYPE_ACTION);
	memcpy(m->da, addr, ETH_ALEN);
	memcpy(m->sa, hapd->own_addr, ETH_ALEN);
	memcpy(m->bssid, hapd->own_addr, ETH_ALEN);
	m->u.action.category = WME_ACTION_CATEGORY;
	m->u.action.u.wme_action.action_code = action_code;
	m->u.action.u.wme_action.dialog_token = dialogue_token;
	m->u.action.u.wme_action.status_code = status_code;
	memcpy(t, tspec, sizeof(struct wme_tspec_info_element));
	len = ((u8 *) (t + 1)) - buf;

	if (hostapd_send_mgmt_frame(hapd, m, len, 0) < 0)
		perror("wme_send_action: send");
}


/* given frame data payload size in bytes, and data_rate in bits per second
 * returns time to complete frame exchange */
/* FIX: should not use floating point types */
static double wme_frame_exchange_time(int bytes, int data_rate, int encryption,
				      int cts_protection)
{
	/* TODO: account for MAC/PHY headers correctly */
	/* TODO: account for encryption headers */
	/* TODO: account for WDS headers */
	/* TODO: account for CTS protection */
	/* TODO: account for SIFS + ACK at minimum PHY rate */
	return (bytes + 400) * 8.0 / data_rate;
}


static void wme_setup_request(struct hostapd_data *hapd,
			      struct ieee80211_mgmt *mgmt,
			      struct wme_tspec_info_element *tspec, size_t len)
{
	/* FIX: should not use floating point types */
	double medium_time, pps;

	/* TODO: account for airtime and answer no to tspec setup requests
	 * when none left!! */

	pps = (tspec->mean_data_rate / 8.0) / tspec->nominal_msdu_size;
	medium_time = (tspec->surplus_bandwidth_allowance / 8) * pps *
		wme_frame_exchange_time(tspec->nominal_msdu_size,
					tspec->minimum_phy_rate, 0, 0);
	tspec->medium_time = medium_time * 1000000.0 / 32.0;

	wme_send_action(hapd, mgmt->sa, tspec, WME_ACTION_CODE_SETUP_RESPONSE,
			mgmt->u.action.u.wme_action.dialog_token,
			WME_SETUP_RESPONSE_STATUS_ADMISSION_ACCEPTED);
}


void hostapd_wme_action(struct hostapd_data *hapd, struct ieee80211_mgmt *mgmt,
			size_t len)
{
	int action_code;
	int left = len - IEEE80211_HDRLEN - 4;
	u8 *pos = ((u8 *) mgmt) + IEEE80211_HDRLEN + 4;
	struct ieee802_11_elems elems;
	struct sta_info *sta = ap_get_sta(hapd, mgmt->sa);

	/* check that the request comes from a valid station */
	if (!sta ||
	    (sta->flags & (WLAN_STA_ASSOC | WLAN_STA_WME)) !=
	    (WLAN_STA_ASSOC | WLAN_STA_WME)) {
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "wme action received is not from associated wme"
			       " station");
		/* TODO: respond with action frame refused status code */
		return;
	}

	/* extract the tspec info element */
	if (ieee802_11_parse_elems(hapd, pos, left, &elems, 1) == ParseFailed)
	{
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "hostapd_wme_action - could not parse wme "
			       "action");
		/* TODO: respond with action frame invalid parameters status
		 * code */
		return;
	}

	if (!elems.wme_tspec ||
	    elems.wme_tspec_len != (sizeof(struct wme_tspec_info_element) - 2))
	{
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "hostapd_wme_action - missing or wrong length "
			       "tspec");
		/* TODO: respond with action frame invalid parameters status
		 * code */
		return;
	}

	/* TODO: check the request is for an AC with ACM set, if not, refuse
	 * request */

	action_code = mgmt->u.action.u.wme_action.action_code;
	switch (action_code) {
	case WME_ACTION_CODE_SETUP_REQUEST:
		wme_setup_request(hapd, mgmt, (struct wme_tspec_info_element *)
				  elems.wme_tspec, len);
		return;
#if 0
	/* TODO: needed for client implementation */
	case WME_ACTION_CODE_SETUP_RESPONSE:
		wme_setup_request(hapd, mgmt, len);
		return;
	/* TODO: handle station teardown requests */
	case WME_ACTION_CODE_TEARDOWN:
		wme_teardown(hapd, mgmt, len);
		return;
#endif
	}

	hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG,
		       "hostapd_wme_action - unknown action code %d",
		       action_code);
}
