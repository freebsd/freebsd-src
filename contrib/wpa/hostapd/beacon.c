/*
 * hostapd / IEEE 802.11 Management: Beacon and Probe Request/Response
 * Copyright (c) 2002-2004, Instant802 Networks, Inc.
 * Copyright (c) 2005-2006, Devicescape Software, Inc.
 * Copyright (c) 2008, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2007-2008, Intel Corporation
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

#ifndef CONFIG_NATIVE_WINDOWS

#include "hostapd.h"
#include "ieee802_11.h"
#include "wpa.h"
#include "wme.h"
#include "beacon.h"
#include "hw_features.h"
#include "driver.h"
#include "sta_info.h"
#include "wps_hostapd.h"


static u8 ieee802_11_erp_info(struct hostapd_data *hapd)
{
	u8 erp = 0;

	if (hapd->iface->current_mode == NULL ||
	    hapd->iface->current_mode->mode != HOSTAPD_MODE_IEEE80211G)
		return 0;

	switch (hapd->iconf->cts_protection_type) {
	case CTS_PROTECTION_FORCE_ENABLED:
		erp |= ERP_INFO_NON_ERP_PRESENT | ERP_INFO_USE_PROTECTION;
		break;
	case CTS_PROTECTION_FORCE_DISABLED:
		erp = 0;
		break;
	case CTS_PROTECTION_AUTOMATIC:
		if (hapd->iface->olbc)
			erp |= ERP_INFO_USE_PROTECTION;
		/* continue */
	case CTS_PROTECTION_AUTOMATIC_NO_OLBC:
		if (hapd->iface->num_sta_non_erp > 0) {
			erp |= ERP_INFO_NON_ERP_PRESENT |
				ERP_INFO_USE_PROTECTION;
		}
		break;
	}
	if (hapd->iface->num_sta_no_short_preamble > 0)
		erp |= ERP_INFO_BARKER_PREAMBLE_MODE;

	return erp;
}


static u8 * hostapd_eid_ds_params(struct hostapd_data *hapd, u8 *eid)
{
	*eid++ = WLAN_EID_DS_PARAMS;
	*eid++ = 1;
	*eid++ = hapd->iconf->channel;
	return eid;
}


static u8 * hostapd_eid_erp_info(struct hostapd_data *hapd, u8 *eid)
{
	if (hapd->iface->current_mode == NULL ||
	    hapd->iface->current_mode->mode != HOSTAPD_MODE_IEEE80211G)
		return eid;

	/* Set NonERP_present and use_protection bits if there
	 * are any associated NonERP stations. */
	/* TODO: use_protection bit can be set to zero even if
	 * there are NonERP stations present. This optimization
	 * might be useful if NonERP stations are "quiet".
	 * See 802.11g/D6 E-1 for recommended practice.
	 * In addition, Non ERP present might be set, if AP detects Non ERP
	 * operation on other APs. */

	/* Add ERP Information element */
	*eid++ = WLAN_EID_ERP_INFO;
	*eid++ = 1;
	*eid++ = ieee802_11_erp_info(hapd);

	return eid;
}


static u8 * hostapd_eid_country_add(u8 *pos, u8 *end, int chan_spacing,
				    struct hostapd_channel_data *start,
				    struct hostapd_channel_data *prev)
{
	if (end - pos < 3)
		return pos;

	/* first channel number */
	*pos++ = start->chan;
	/* number of channels */
	*pos++ = (prev->chan - start->chan) / chan_spacing + 1;
	/* maximum transmit power level */
	*pos++ = start->max_tx_power;

	return pos;
}


static u8 * hostapd_eid_country(struct hostapd_data *hapd, u8 *eid,
				int max_len)
{
	u8 *pos = eid;
	u8 *end = eid + max_len;
	int i;
	struct hostapd_hw_modes *mode;
	struct hostapd_channel_data *start, *prev;
	int chan_spacing = 1;

	if (!hapd->iconf->ieee80211d || max_len < 6 ||
	    hapd->iface->current_mode == NULL)
		return eid;

	*pos++ = WLAN_EID_COUNTRY;
	pos++; /* length will be set later */
	os_memcpy(pos, hapd->iconf->country, 3); /* e.g., 'US ' */
	pos += 3;

	mode = hapd->iface->current_mode;
	if (mode->mode == HOSTAPD_MODE_IEEE80211A)
		chan_spacing = 4;

	start = prev = NULL;
	for (i = 0; i < mode->num_channels; i++) {
		struct hostapd_channel_data *chan = &mode->channels[i];
		if (chan->flag & HOSTAPD_CHAN_DISABLED)
			continue;
		if (start && prev &&
		    prev->chan + chan_spacing == chan->chan &&
		    start->max_tx_power == chan->max_tx_power) {
			prev = chan;
			continue; /* can use same entry */
		}

		if (start) {
			pos = hostapd_eid_country_add(pos, end, chan_spacing,
						      start, prev);
			start = NULL;
		}

		/* Start new group */
		start = prev = chan;
	}

	if (start) {
		pos = hostapd_eid_country_add(pos, end, chan_spacing,
					      start, prev);
	}

	if ((pos - eid) & 1) {
		if (end - pos < 1)
			return eid;
		*pos++ = 0; /* pad for 16-bit alignment */
	}

	eid[1] = (pos - eid) - 2;

	return pos;
}


static u8 * hostapd_eid_wpa(struct hostapd_data *hapd, u8 *eid, size_t len,
			    struct sta_info *sta)
{
	const u8 *ie;
	size_t ielen;

	ie = wpa_auth_get_wpa_ie(hapd->wpa_auth, &ielen);
	if (ie == NULL || ielen > len)
		return eid;

	os_memcpy(eid, ie, ielen);
	return eid + ielen;
}


void handle_probe_req(struct hostapd_data *hapd, struct ieee80211_mgmt *mgmt,
		      size_t len)
{
	struct ieee80211_mgmt *resp;
	struct ieee802_11_elems elems;
	char *ssid;
	u8 *pos, *epos, *ie;
	size_t ssid_len, ie_len;
	struct sta_info *sta = NULL;

	ie = mgmt->u.probe_req.variable;
	ie_len = len - (IEEE80211_HDRLEN + sizeof(mgmt->u.probe_req));

	hostapd_wps_probe_req_rx(hapd, mgmt->sa, ie, ie_len);

	if (!hapd->iconf->send_probe_response)
		return;

	if (ieee802_11_parse_elems(ie, ie_len, &elems, 0) == ParseFailed) {
		wpa_printf(MSG_DEBUG, "Could not parse ProbeReq from " MACSTR,
			   MAC2STR(mgmt->sa));
		return;
	}

	ssid = NULL;
	ssid_len = 0;

	if ((!elems.ssid || !elems.supp_rates)) {
		wpa_printf(MSG_DEBUG, "STA " MACSTR " sent probe request "
			   "without SSID or supported rates element",
			   MAC2STR(mgmt->sa));
		return;
	}

	if (hapd->conf->ignore_broadcast_ssid && elems.ssid_len == 0) {
		wpa_printf(MSG_MSGDUMP, "Probe Request from " MACSTR " for "
			   "broadcast SSID ignored", MAC2STR(mgmt->sa));
		return;
	}

	sta = ap_get_sta(hapd, mgmt->sa);

	if (elems.ssid_len == 0 ||
	    (elems.ssid_len == hapd->conf->ssid.ssid_len &&
	     os_memcmp(elems.ssid, hapd->conf->ssid.ssid, elems.ssid_len) ==
	     0)) {
		ssid = hapd->conf->ssid.ssid;
		ssid_len = hapd->conf->ssid.ssid_len;
		if (sta)
			sta->ssid_probe = &hapd->conf->ssid;
	}

	if (!ssid) {
		if (!(mgmt->da[0] & 0x01)) {
			char ssid_txt[33];
			ieee802_11_print_ssid(ssid_txt, elems.ssid,
					      elems.ssid_len);
			wpa_printf(MSG_MSGDUMP, "Probe Request from " MACSTR
				   " for foreign SSID '%s'",
				   MAC2STR(mgmt->sa), ssid_txt);
		}
		return;
	}

	/* TODO: verify that supp_rates contains at least one matching rate
	 * with AP configuration */
#define MAX_PROBERESP_LEN 768
	resp = os_zalloc(MAX_PROBERESP_LEN);
	if (resp == NULL)
		return;
	epos = ((u8 *) resp) + MAX_PROBERESP_LEN;

	resp->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_PROBE_RESP);
	os_memcpy(resp->da, mgmt->sa, ETH_ALEN);
	os_memcpy(resp->sa, hapd->own_addr, ETH_ALEN);

	os_memcpy(resp->bssid, hapd->own_addr, ETH_ALEN);
	resp->u.probe_resp.beacon_int =
		host_to_le16(hapd->iconf->beacon_int);

	/* hardware or low-level driver will setup seq_ctrl and timestamp */
	resp->u.probe_resp.capab_info =
		host_to_le16(hostapd_own_capab_info(hapd, sta, 1));

	pos = resp->u.probe_resp.variable;
	*pos++ = WLAN_EID_SSID;
	*pos++ = ssid_len;
	os_memcpy(pos, ssid, ssid_len);
	pos += ssid_len;

	/* Supported rates */
	pos = hostapd_eid_supp_rates(hapd, pos);

	/* DS Params */
	pos = hostapd_eid_ds_params(hapd, pos);

	pos = hostapd_eid_country(hapd, pos, epos - pos);

	/* ERP Information element */
	pos = hostapd_eid_erp_info(hapd, pos);

	/* Extended supported rates */
	pos = hostapd_eid_ext_supp_rates(hapd, pos);

	pos = hostapd_eid_wpa(hapd, pos, epos - pos, sta);

	/* Wi-Fi Alliance WMM */
	pos = hostapd_eid_wmm(hapd, pos);

	pos = hostapd_eid_ht_capabilities_info(hapd, pos);
	pos = hostapd_eid_ht_operation(hapd, pos);

#ifdef CONFIG_WPS
	if (hapd->conf->wps_state && hapd->wps_probe_resp_ie) {
		os_memcpy(pos, hapd->wps_probe_resp_ie,
			  hapd->wps_probe_resp_ie_len);
		pos += hapd->wps_probe_resp_ie_len;
	}
#endif /* CONFIG_WPS */

	if (hostapd_send_mgmt_frame(hapd, resp, pos - (u8 *) resp, 0) < 0)
		perror("handle_probe_req: send");

	os_free(resp);

	wpa_printf(MSG_MSGDUMP, "STA " MACSTR " sent probe request for %s "
		   "SSID", MAC2STR(mgmt->sa),
		   elems.ssid_len == 0 ? "broadcast" : "our");
}


void ieee802_11_set_beacon(struct hostapd_data *hapd)
{
	struct ieee80211_mgmt *head;
	u8 *pos, *tail, *tailpos;
	int preamble;
	u16 capab_info;
	size_t head_len, tail_len;
	int cts_protection = ((ieee802_11_erp_info(hapd) &
			      ERP_INFO_USE_PROTECTION) ? 1 : 0);

#define BEACON_HEAD_BUF_SIZE 256
#define BEACON_TAIL_BUF_SIZE 512
	head = os_zalloc(BEACON_HEAD_BUF_SIZE);
	tailpos = tail = os_malloc(BEACON_TAIL_BUF_SIZE);
	if (head == NULL || tail == NULL) {
		wpa_printf(MSG_ERROR, "Failed to set beacon data");
		os_free(head);
		os_free(tail);
		return;
	}

	head->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_BEACON);
	head->duration = host_to_le16(0);
	os_memset(head->da, 0xff, ETH_ALEN);

	os_memcpy(head->sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(head->bssid, hapd->own_addr, ETH_ALEN);
	head->u.beacon.beacon_int =
		host_to_le16(hapd->iconf->beacon_int);

	/* hardware or low-level driver will setup seq_ctrl and timestamp */
	capab_info = hostapd_own_capab_info(hapd, NULL, 0);
	head->u.beacon.capab_info = host_to_le16(capab_info);
	pos = &head->u.beacon.variable[0];

	/* SSID */
	*pos++ = WLAN_EID_SSID;
	if (hapd->conf->ignore_broadcast_ssid == 2) {
		/* clear the data, but keep the correct length of the SSID */
		*pos++ = hapd->conf->ssid.ssid_len;
		os_memset(pos, 0, hapd->conf->ssid.ssid_len);
		pos += hapd->conf->ssid.ssid_len;
	} else if (hapd->conf->ignore_broadcast_ssid) {
		*pos++ = 0; /* empty SSID */
	} else {
		*pos++ = hapd->conf->ssid.ssid_len;
		os_memcpy(pos, hapd->conf->ssid.ssid,
			  hapd->conf->ssid.ssid_len);
		pos += hapd->conf->ssid.ssid_len;
	}

	/* Supported rates */
	pos = hostapd_eid_supp_rates(hapd, pos);

	/* DS Params */
	pos = hostapd_eid_ds_params(hapd, pos);

	head_len = pos - (u8 *) head;

	tailpos = hostapd_eid_country(hapd, tailpos,
				      tail + BEACON_TAIL_BUF_SIZE - tailpos);

	/* ERP Information element */
	tailpos = hostapd_eid_erp_info(hapd, tailpos);

	/* Extended supported rates */
	tailpos = hostapd_eid_ext_supp_rates(hapd, tailpos);

	tailpos = hostapd_eid_wpa(hapd, tailpos, tail + BEACON_TAIL_BUF_SIZE -
				  tailpos, NULL);

	/* Wi-Fi Alliance WMM */
	tailpos = hostapd_eid_wmm(hapd, tailpos);

#ifdef CONFIG_IEEE80211N
	if (hapd->iconf->ieee80211n) {
		u8 *ht_capab, *ht_oper;
		ht_capab = tailpos;
		tailpos = hostapd_eid_ht_capabilities_info(hapd, tailpos);

		ht_oper = tailpos;
		tailpos = hostapd_eid_ht_operation(hapd, tailpos);

		if (tailpos > ht_oper && ht_oper > ht_capab &&
		    hostapd_set_ht_params(hapd->conf->iface, hapd,
					  ht_capab + 2, ht_capab[1],
					  ht_oper + 2, ht_oper[1])) {
			wpa_printf(MSG_ERROR, "Could not set HT capabilities "
				   "for kernel driver");
		}
	}
#endif /* CONFIG_IEEE80211N */

#ifdef CONFIG_WPS
	if (hapd->conf->wps_state && hapd->wps_beacon_ie) {
		os_memcpy(tailpos, hapd->wps_beacon_ie,
			  hapd->wps_beacon_ie_len);
		tailpos += hapd->wps_beacon_ie_len;
	}
#endif /* CONFIG_WPS */

	tail_len = tailpos > tail ? tailpos - tail : 0;

	if (hostapd_set_beacon(hapd->conf->iface, hapd, (u8 *) head, head_len,
			       tail, tail_len))
		wpa_printf(MSG_ERROR, "Failed to set beacon head/tail");

	os_free(tail);
	os_free(head);

	if (hostapd_set_cts_protect(hapd, cts_protection))
		wpa_printf(MSG_ERROR, "Failed to set CTS protect in kernel "
			   "driver");

	if (hapd->iface->current_mode &&
	    hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G &&
	    hostapd_set_short_slot_time(hapd,
					hapd->iface->num_sta_no_short_slot_time
					> 0 ? 0 : 1))
		wpa_printf(MSG_ERROR, "Failed to set Short Slot Time option "
			   "in kernel driver");

	if (hapd->iface->num_sta_no_short_preamble == 0 &&
	    hapd->iconf->preamble == SHORT_PREAMBLE)
		preamble = SHORT_PREAMBLE;
	else
		preamble = LONG_PREAMBLE;
	if (hostapd_set_preamble(hapd, preamble))
		wpa_printf(MSG_ERROR, "Could not set preamble for kernel "
			   "driver");
}


void ieee802_11_set_beacons(struct hostapd_iface *iface)
{
	size_t i;
	for (i = 0; i < iface->num_bss; i++)
		ieee802_11_set_beacon(iface->bss[i]);
}

#endif /* CONFIG_NATIVE_WINDOWS */
