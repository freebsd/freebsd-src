/*
 * hostapd / IEEE 802.11 Management: Beacon and Probe Request/Response
 * Copyright (c) 2002-2004, Instant802 Networks, Inc.
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

#ifndef CONFIG_NATIVE_WINDOWS

#include "hostapd.h"
#include "ieee802_11.h"
#include "wpa.h"
#include "wme.h"
#include "beacon.h"
#include "hw_features.h"
#include "driver.h"
#include "sta_info.h"
#include "ieee802_11h.h"


static u8 ieee802_11_erp_info(struct hostapd_data *hapd)
{
	u8 erp = 0;

	if (hapd->iface == NULL || hapd->iface->current_mode == NULL ||
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
	if (hapd->iface == NULL || hapd->iface->current_mode == NULL ||
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


static u8 * hostapd_eid_country(struct hostapd_data *hapd, u8 *eid,
				int max_len)
{
	int left;
	u8 *pos = eid;

	if ((!hapd->iconf->ieee80211d && !hapd->iface->dfs_enable) ||
	    max_len < 6)
		return eid;

	*pos++ = WLAN_EID_COUNTRY;
	pos++; /* length will be set later */
	memcpy(pos, hapd->iconf->country, 3); /* e.g., 'US ' */
	pos += 3;
	left = max_len - 3;

	if ((pos - eid) & 1) {
		if (left < 1)
			return eid;
		*pos++ = 0; /* pad for 16-bit alignment */
		left--;
	}

	eid[1] = (pos - eid) - 2;

	return pos;
}


static u8 * hostapd_eid_power_constraint(struct hostapd_data *hapd, u8 *eid)

{
	if (!hapd->iface->dfs_enable)
		return eid;
	*eid++ = WLAN_EID_PWR_CONSTRAINT;
	*eid++ = 1;
	*eid++ = hapd->iface->pwr_const;
	return eid;
}


static u8 * hostapd_eid_tpc_report(struct hostapd_data *hapd, u8 *eid)

{
	if (!hapd->iface->dfs_enable)
		return eid;
	*eid++ = WLAN_EID_TPC_REPORT;
	*eid++ = 2;
	*eid++ = hapd->iface->tx_power; /* TX POWER */
	*eid++ = 0; /* Link Margin */
	return eid;
}

static u8 * hostapd_eid_channel_switch(struct hostapd_data *hapd, u8 *eid)

{
	if (!hapd->iface->dfs_enable || !hapd->iface->channel_switch)
		return eid;
	*eid++ = WLAN_EID_CHANNEL_SWITCH;
	*eid++ = 3;
	*eid++ = CHAN_SWITCH_MODE_QUIET;
	*eid++ = hapd->iface->channel_switch; /* New channel */
	/* 0 - very soon; 1 - before next TBTT; num - after num beacons */
	*eid++ = 0;
	return eid;
}


static u8 * hostapd_eid_wpa(struct hostapd_data *hapd, u8 *eid, size_t len,
			    struct sta_info *sta)
{
	const u8 *ie;
	size_t ielen;

	ie = wpa_auth_get_wpa_ie(hapd->wpa_auth, &ielen);
	if (ie == NULL || ielen > len)
		return eid;

	memcpy(eid, ie, ielen);
	return eid + ielen;
}


void handle_probe_req(struct hostapd_data *hapd, struct ieee80211_mgmt *mgmt,
		      size_t len)
{
	struct ieee80211_mgmt *resp;
	struct ieee802_11_elems elems;
	char *ssid;
	u8 *pos, *epos;
	size_t ssid_len;
	struct sta_info *sta = NULL;

	if (!hapd->iconf->send_probe_response)
		return;

	if (ieee802_11_parse_elems(hapd, mgmt->u.probe_req.variable,
				   len - (IEEE80211_HDRLEN +
					  sizeof(mgmt->u.probe_req)), &elems,
				   0)
	    == ParseFailed) {
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
			      "Could not parse ProbeReq from " MACSTR "\n",
			      MAC2STR(mgmt->sa));
		return;
	}

	ssid = NULL;
	ssid_len = 0;

	if ((!elems.ssid || !elems.supp_rates)) {
		printf("STA " MACSTR " sent probe request without SSID or "
		       "supported rates element\n", MAC2STR(mgmt->sa));
		return;
	}

	if (hapd->conf->ignore_broadcast_ssid && elems.ssid_len == 0) {
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MSGDUMPS,
			      "Probe Request from " MACSTR " for broadcast "
			      "SSID ignored\n", MAC2STR(mgmt->sa));
		return;
	}

	sta = ap_get_sta(hapd, mgmt->sa);

	if (elems.ssid_len == 0 ||
	    (elems.ssid_len == hapd->conf->ssid.ssid_len &&
	     memcmp(elems.ssid, hapd->conf->ssid.ssid, elems.ssid_len) == 0)) {
		ssid = hapd->conf->ssid.ssid;
		ssid_len = hapd->conf->ssid.ssid_len;
		if (sta)
			sta->ssid_probe = &hapd->conf->ssid;
	}

	if (!ssid) {
		if (HOSTAPD_DEBUG_COND(HOSTAPD_DEBUG_MSGDUMPS)) {
			printf("Probe Request from " MACSTR " for foreign "
			       "SSID '", MAC2STR(mgmt->sa));
			ieee802_11_print_ssid(elems.ssid, elems.ssid_len);
			printf("'\n");
		}
		return;
	}

	/* TODO: verify that supp_rates contains at least one matching rate
	 * with AP configuration */
#define MAX_PROBERESP_LEN 512
	resp = wpa_zalloc(MAX_PROBERESP_LEN);
	if (resp == NULL)
		return;
	epos = ((u8 *) resp) + MAX_PROBERESP_LEN;

	resp->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_PROBE_RESP);
	memcpy(resp->da, mgmt->sa, ETH_ALEN);
	memcpy(resp->sa, hapd->own_addr, ETH_ALEN);

	memcpy(resp->bssid, hapd->own_addr, ETH_ALEN);
	resp->u.probe_resp.beacon_int =
		host_to_le16(hapd->iconf->beacon_int);

	/* hardware or low-level driver will setup seq_ctrl and timestamp */
	resp->u.probe_resp.capab_info =
		host_to_le16(hostapd_own_capab_info(hapd, sta, 1));

	pos = resp->u.probe_resp.variable;
	*pos++ = WLAN_EID_SSID;
	*pos++ = ssid_len;
	memcpy(pos, ssid, ssid_len);
	pos += ssid_len;

	/* Supported rates */
	pos = hostapd_eid_supp_rates(hapd, pos);

	/* DS Params */
	pos = hostapd_eid_ds_params(hapd, pos);

	pos = hostapd_eid_country(hapd, pos, epos - pos);

	pos = hostapd_eid_power_constraint(hapd, pos);
	pos = hostapd_eid_tpc_report(hapd, pos);

	/* ERP Information element */
	pos = hostapd_eid_erp_info(hapd, pos);

	/* Extended supported rates */
	pos = hostapd_eid_ext_supp_rates(hapd, pos);

	pos = hostapd_eid_wpa(hapd, pos, epos - pos, sta);

	/* Wi-Fi Wireless Multimedia Extensions */
	if (hapd->conf->wme_enabled)
		pos = hostapd_eid_wme(hapd, pos);

	if (hostapd_send_mgmt_frame(hapd, resp, pos - (u8 *) resp, 0) < 0)
		perror("handle_probe_req: send");

	free(resp);

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MSGDUMPS, "STA " MACSTR
		      " sent probe request for %s SSID\n",
		      MAC2STR(mgmt->sa), elems.ssid_len == 0 ? "broadcast" :
		      "our");
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
#define BEACON_TAIL_BUF_SIZE 256
	head = wpa_zalloc(BEACON_HEAD_BUF_SIZE);
	tailpos = tail = malloc(BEACON_TAIL_BUF_SIZE);
	if (head == NULL || tail == NULL) {
		printf("Failed to set beacon data\n");
		free(head);
		free(tail);
		return;
	}

	head->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_BEACON);
	head->duration = host_to_le16(0);
	memset(head->da, 0xff, ETH_ALEN);

	memcpy(head->sa, hapd->own_addr, ETH_ALEN);
	memcpy(head->bssid, hapd->own_addr, ETH_ALEN);
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
		memset(pos, 0, hapd->conf->ssid.ssid_len);
		pos += hapd->conf->ssid.ssid_len;
	} else if (hapd->conf->ignore_broadcast_ssid) {
		*pos++ = 0; /* empty SSID */
	} else {
		*pos++ = hapd->conf->ssid.ssid_len;
		memcpy(pos, hapd->conf->ssid.ssid, hapd->conf->ssid.ssid_len);
		pos += hapd->conf->ssid.ssid_len;
	}

	/* Supported rates */
	pos = hostapd_eid_supp_rates(hapd, pos);

	/* DS Params */
	pos = hostapd_eid_ds_params(hapd, pos);

	head_len = pos - (u8 *) head;

	tailpos = hostapd_eid_country(hapd, tailpos,
				      tail + BEACON_TAIL_BUF_SIZE - tailpos);

	tailpos = hostapd_eid_power_constraint(hapd, tailpos);
	tailpos = hostapd_eid_channel_switch(hapd, tailpos);
	tailpos = hostapd_eid_tpc_report(hapd, tailpos);

	/* ERP Information element */
	tailpos = hostapd_eid_erp_info(hapd, tailpos);

	/* Extended supported rates */
	tailpos = hostapd_eid_ext_supp_rates(hapd, tailpos);

	tailpos = hostapd_eid_wpa(hapd, tailpos, tail + BEACON_TAIL_BUF_SIZE -
				  tailpos, NULL);

	/* Wi-Fi Wireless Multimedia Extensions */
	if (hapd->conf->wme_enabled)
		tailpos = hostapd_eid_wme(hapd, tailpos);

	tail_len = tailpos > tail ? tailpos - tail : 0;

	if (hostapd_set_beacon(hapd->conf->iface, hapd, (u8 *) head, head_len,
			       tail, tail_len))
		printf("Failed to set beacon head/tail\n");

	free(tail);
	free(head);

	if (hostapd_set_cts_protect(hapd, cts_protection))
		printf("Failed to set CTS protect in kernel driver\n");

	if (hapd->iface && hapd->iface->current_mode &&
	    hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G &&
	    hostapd_set_short_slot_time(hapd,
					hapd->iface->num_sta_no_short_slot_time
					> 0 ? 0 : 1))
		printf("Failed to set Short Slot Time option in kernel "
		       "driver\n");

	if (hapd->iface && hapd->iface->num_sta_no_short_preamble == 0 &&
	    hapd->iconf->preamble == SHORT_PREAMBLE)
		preamble = SHORT_PREAMBLE;
	else
		preamble = LONG_PREAMBLE;
	if (hostapd_set_preamble(hapd, preamble))
		printf("Could not set preamble for kernel driver\n");
}


void ieee802_11_set_beacons(struct hostapd_iface *iface)
{
	size_t i;
	for (i = 0; i < iface->num_bss; i++)
		ieee802_11_set_beacon(iface->bss[i]);
}

#endif /* CONFIG_NATIVE_WINDOWS */
