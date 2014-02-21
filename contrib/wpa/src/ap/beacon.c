/*
 * hostapd / IEEE 802.11 Management: Beacon and Probe Request/Response
 * Copyright (c) 2002-2004, Instant802 Networks, Inc.
 * Copyright (c) 2005-2006, Devicescape Software, Inc.
 * Copyright (c) 2008-2012, Jouni Malinen <j@w1.fi>
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

#include "utils/includes.h"

#ifndef CONFIG_NATIVE_WINDOWS

#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "drivers/driver.h"
#include "wps/wps_defs.h"
#include "p2p/p2p.h"
#include "hostapd.h"
#include "ieee802_11.h"
#include "wpa_auth.h"
#include "wmm.h"
#include "ap_config.h"
#include "sta_info.h"
#include "p2p_hostapd.h"
#include "ap_drv_ops.h"
#include "beacon.h"
#include "hs20.h"


#ifdef NEED_AP_MLME

static u8 ieee802_11_erp_info(struct hostapd_data *hapd)
{
	u8 erp = 0;

	if (hapd->iface->current_mode == NULL ||
	    hapd->iface->current_mode->mode != HOSTAPD_MODE_IEEE80211G)
		return 0;

	if (hapd->iface->olbc)
		erp |= ERP_INFO_USE_PROTECTION;
	if (hapd->iface->num_sta_non_erp > 0) {
		erp |= ERP_INFO_NON_ERP_PRESENT |
			ERP_INFO_USE_PROTECTION;
	}
	if (hapd->iface->num_sta_no_short_preamble > 0 ||
	    hapd->iconf->preamble == LONG_PREAMBLE)
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


static u8 * hostapd_eid_wpa(struct hostapd_data *hapd, u8 *eid, size_t len)
{
	const u8 *ie;
	size_t ielen;

	ie = wpa_auth_get_wpa_ie(hapd->wpa_auth, &ielen);
	if (ie == NULL || ielen > len)
		return eid;

	os_memcpy(eid, ie, ielen);
	return eid + ielen;
}


static u8 * hostapd_gen_probe_resp(struct hostapd_data *hapd,
				   struct sta_info *sta,
				   const struct ieee80211_mgmt *req,
				   int is_p2p, size_t *resp_len)
{
	struct ieee80211_mgmt *resp;
	u8 *pos, *epos;
	size_t buflen;

#define MAX_PROBERESP_LEN 768
	buflen = MAX_PROBERESP_LEN;
#ifdef CONFIG_WPS
	if (hapd->wps_probe_resp_ie)
		buflen += wpabuf_len(hapd->wps_probe_resp_ie);
#endif /* CONFIG_WPS */
#ifdef CONFIG_P2P
	if (hapd->p2p_probe_resp_ie)
		buflen += wpabuf_len(hapd->p2p_probe_resp_ie);
#endif /* CONFIG_P2P */
	if (hapd->conf->vendor_elements)
		buflen += wpabuf_len(hapd->conf->vendor_elements);
	resp = os_zalloc(buflen);
	if (resp == NULL)
		return NULL;

	epos = ((u8 *) resp) + MAX_PROBERESP_LEN;

	resp->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_PROBE_RESP);
	if (req)
		os_memcpy(resp->da, req->sa, ETH_ALEN);
	os_memcpy(resp->sa, hapd->own_addr, ETH_ALEN);

	os_memcpy(resp->bssid, hapd->own_addr, ETH_ALEN);
	resp->u.probe_resp.beacon_int =
		host_to_le16(hapd->iconf->beacon_int);

	/* hardware or low-level driver will setup seq_ctrl and timestamp */
	resp->u.probe_resp.capab_info =
		host_to_le16(hostapd_own_capab_info(hapd, sta, 1));

	pos = resp->u.probe_resp.variable;
	*pos++ = WLAN_EID_SSID;
	*pos++ = hapd->conf->ssid.ssid_len;
	os_memcpy(pos, hapd->conf->ssid.ssid, hapd->conf->ssid.ssid_len);
	pos += hapd->conf->ssid.ssid_len;

	/* Supported rates */
	pos = hostapd_eid_supp_rates(hapd, pos);

	/* DS Params */
	pos = hostapd_eid_ds_params(hapd, pos);

	pos = hostapd_eid_country(hapd, pos, epos - pos);

	/* ERP Information element */
	pos = hostapd_eid_erp_info(hapd, pos);

	/* Extended supported rates */
	pos = hostapd_eid_ext_supp_rates(hapd, pos);

	/* RSN, MDIE, WPA */
	pos = hostapd_eid_wpa(hapd, pos, epos - pos);

#ifdef CONFIG_IEEE80211N
	pos = hostapd_eid_ht_capabilities(hapd, pos);
	pos = hostapd_eid_ht_operation(hapd, pos);
#endif /* CONFIG_IEEE80211N */

	pos = hostapd_eid_ext_capab(hapd, pos);

	pos = hostapd_eid_time_adv(hapd, pos);
	pos = hostapd_eid_time_zone(hapd, pos);

	pos = hostapd_eid_interworking(hapd, pos);
	pos = hostapd_eid_adv_proto(hapd, pos);
	pos = hostapd_eid_roaming_consortium(hapd, pos);

#ifdef CONFIG_IEEE80211AC
	pos = hostapd_eid_vht_capabilities(hapd, pos);
	pos = hostapd_eid_vht_operation(hapd, pos);
#endif /* CONFIG_IEEE80211AC */

	/* Wi-Fi Alliance WMM */
	pos = hostapd_eid_wmm(hapd, pos);

#ifdef CONFIG_WPS
	if (hapd->conf->wps_state && hapd->wps_probe_resp_ie) {
		os_memcpy(pos, wpabuf_head(hapd->wps_probe_resp_ie),
			  wpabuf_len(hapd->wps_probe_resp_ie));
		pos += wpabuf_len(hapd->wps_probe_resp_ie);
	}
#endif /* CONFIG_WPS */

#ifdef CONFIG_P2P
	if ((hapd->conf->p2p & P2P_ENABLED) && is_p2p &&
	    hapd->p2p_probe_resp_ie) {
		os_memcpy(pos, wpabuf_head(hapd->p2p_probe_resp_ie),
			  wpabuf_len(hapd->p2p_probe_resp_ie));
		pos += wpabuf_len(hapd->p2p_probe_resp_ie);
	}
#endif /* CONFIG_P2P */
#ifdef CONFIG_P2P_MANAGER
	if ((hapd->conf->p2p & (P2P_MANAGE | P2P_ENABLED | P2P_GROUP_OWNER)) ==
	    P2P_MANAGE)
		pos = hostapd_eid_p2p_manage(hapd, pos);
#endif /* CONFIG_P2P_MANAGER */

#ifdef CONFIG_HS20
	pos = hostapd_eid_hs20_indication(hapd, pos);
#endif /* CONFIG_HS20 */

	if (hapd->conf->vendor_elements) {
		os_memcpy(pos, wpabuf_head(hapd->conf->vendor_elements),
			  wpabuf_len(hapd->conf->vendor_elements));
		pos += wpabuf_len(hapd->conf->vendor_elements);
	}

	*resp_len = pos - (u8 *) resp;
	return (u8 *) resp;
}


enum ssid_match_result {
	NO_SSID_MATCH,
	EXACT_SSID_MATCH,
	WILDCARD_SSID_MATCH
};

static enum ssid_match_result ssid_match(struct hostapd_data *hapd,
					 const u8 *ssid, size_t ssid_len,
					 const u8 *ssid_list,
					 size_t ssid_list_len)
{
	const u8 *pos, *end;
	int wildcard = 0;

	if (ssid_len == 0)
		wildcard = 1;
	if (ssid_len == hapd->conf->ssid.ssid_len &&
	    os_memcmp(ssid, hapd->conf->ssid.ssid, ssid_len) == 0)
		return EXACT_SSID_MATCH;

	if (ssid_list == NULL)
		return wildcard ? WILDCARD_SSID_MATCH : NO_SSID_MATCH;

	pos = ssid_list;
	end = ssid_list + ssid_list_len;
	while (pos + 1 <= end) {
		if (pos + 2 + pos[1] > end)
			break;
		if (pos[1] == 0)
			wildcard = 1;
		if (pos[1] == hapd->conf->ssid.ssid_len &&
		    os_memcmp(pos + 2, hapd->conf->ssid.ssid, pos[1]) == 0)
			return EXACT_SSID_MATCH;
		pos += 2 + pos[1];
	}

	return wildcard ? WILDCARD_SSID_MATCH : NO_SSID_MATCH;
}


void handle_probe_req(struct hostapd_data *hapd,
		      const struct ieee80211_mgmt *mgmt, size_t len,
		      int ssi_signal)
{
	u8 *resp;
	struct ieee802_11_elems elems;
	const u8 *ie;
	size_t ie_len;
	struct sta_info *sta = NULL;
	size_t i, resp_len;
	int noack;
	enum ssid_match_result res;

	ie = mgmt->u.probe_req.variable;
	if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.probe_req))
		return;
	ie_len = len - (IEEE80211_HDRLEN + sizeof(mgmt->u.probe_req));

	for (i = 0; hapd->probereq_cb && i < hapd->num_probereq_cb; i++)
		if (hapd->probereq_cb[i].cb(hapd->probereq_cb[i].ctx,
					    mgmt->sa, mgmt->da, mgmt->bssid,
					    ie, ie_len, ssi_signal) > 0)
			return;

	if (!hapd->iconf->send_probe_response)
		return;

	if (ieee802_11_parse_elems(ie, ie_len, &elems, 0) == ParseFailed) {
		wpa_printf(MSG_DEBUG, "Could not parse ProbeReq from " MACSTR,
			   MAC2STR(mgmt->sa));
		return;
	}

	if ((!elems.ssid || !elems.supp_rates)) {
		wpa_printf(MSG_DEBUG, "STA " MACSTR " sent probe request "
			   "without SSID or supported rates element",
			   MAC2STR(mgmt->sa));
		return;
	}

#ifdef CONFIG_P2P
	if (hapd->p2p && elems.wps_ie) {
		struct wpabuf *wps;
		wps = ieee802_11_vendor_ie_concat(ie, ie_len, WPS_DEV_OUI_WFA);
		if (wps && !p2p_group_match_dev_type(hapd->p2p_group, wps)) {
			wpa_printf(MSG_MSGDUMP, "P2P: Ignore Probe Request "
				   "due to mismatch with Requested Device "
				   "Type");
			wpabuf_free(wps);
			return;
		}
		wpabuf_free(wps);
	}

	if (hapd->p2p && elems.p2p) {
		struct wpabuf *p2p;
		p2p = ieee802_11_vendor_ie_concat(ie, ie_len, P2P_IE_VENDOR_TYPE);
		if (p2p && !p2p_group_match_dev_id(hapd->p2p_group, p2p)) {
			wpa_printf(MSG_MSGDUMP, "P2P: Ignore Probe Request "
				   "due to mismatch with Device ID");
			wpabuf_free(p2p);
			return;
		}
		wpabuf_free(p2p);
	}
#endif /* CONFIG_P2P */

	if (hapd->conf->ignore_broadcast_ssid && elems.ssid_len == 0 &&
	    elems.ssid_list_len == 0) {
		wpa_printf(MSG_MSGDUMP, "Probe Request from " MACSTR " for "
			   "broadcast SSID ignored", MAC2STR(mgmt->sa));
		return;
	}

	sta = ap_get_sta(hapd, mgmt->sa);

#ifdef CONFIG_P2P
	if ((hapd->conf->p2p & P2P_GROUP_OWNER) &&
	    elems.ssid_len == P2P_WILDCARD_SSID_LEN &&
	    os_memcmp(elems.ssid, P2P_WILDCARD_SSID,
		      P2P_WILDCARD_SSID_LEN) == 0) {
		/* Process P2P Wildcard SSID like Wildcard SSID */
		elems.ssid_len = 0;
	}
#endif /* CONFIG_P2P */

	res = ssid_match(hapd, elems.ssid, elems.ssid_len,
			 elems.ssid_list, elems.ssid_list_len);
	if (res != NO_SSID_MATCH) {
		if (sta)
			sta->ssid_probe = &hapd->conf->ssid;
	} else {
		if (!(mgmt->da[0] & 0x01)) {
			char ssid_txt[33];
			ieee802_11_print_ssid(ssid_txt, elems.ssid,
					      elems.ssid_len);
			wpa_printf(MSG_MSGDUMP, "Probe Request from " MACSTR
				   " for foreign SSID '%s' (DA " MACSTR ")%s",
				   MAC2STR(mgmt->sa), ssid_txt,
				   MAC2STR(mgmt->da),
				   elems.ssid_list ? " (SSID list)" : "");
		}
		return;
	}

#ifdef CONFIG_INTERWORKING
	if (elems.interworking && elems.interworking_len >= 1) {
		u8 ant = elems.interworking[0] & 0x0f;
		if (ant != INTERWORKING_ANT_WILDCARD &&
		    ant != hapd->conf->access_network_type) {
			wpa_printf(MSG_MSGDUMP, "Probe Request from " MACSTR
				   " for mismatching ANT %u ignored",
				   MAC2STR(mgmt->sa), ant);
			return;
		}
	}

	if (elems.interworking &&
	    (elems.interworking_len == 7 || elems.interworking_len == 9)) {
		const u8 *hessid;
		if (elems.interworking_len == 7)
			hessid = elems.interworking + 1;
		else
			hessid = elems.interworking + 1 + 2;
		if (!is_broadcast_ether_addr(hessid) &&
		    os_memcmp(hessid, hapd->conf->hessid, ETH_ALEN) != 0) {
			wpa_printf(MSG_MSGDUMP, "Probe Request from " MACSTR
				   " for mismatching HESSID " MACSTR
				   " ignored",
				   MAC2STR(mgmt->sa), MAC2STR(hessid));
			return;
		}
	}
#endif /* CONFIG_INTERWORKING */

	/* TODO: verify that supp_rates contains at least one matching rate
	 * with AP configuration */

	resp = hostapd_gen_probe_resp(hapd, sta, mgmt, elems.p2p != NULL,
				      &resp_len);
	if (resp == NULL)
		return;

	/*
	 * If this is a broadcast probe request, apply no ack policy to avoid
	 * excessive retries.
	 */
	noack = !!(res == WILDCARD_SSID_MATCH &&
		   is_broadcast_ether_addr(mgmt->da));

	if (hostapd_drv_send_mlme(hapd, resp, resp_len, noack) < 0)
		perror("handle_probe_req: send");

	os_free(resp);

	wpa_printf(MSG_EXCESSIVE, "STA " MACSTR " sent probe request for %s "
		   "SSID", MAC2STR(mgmt->sa),
		   elems.ssid_len == 0 ? "broadcast" : "our");
}


static u8 * hostapd_probe_resp_offloads(struct hostapd_data *hapd,
					size_t *resp_len)
{
	/* check probe response offloading caps and print warnings */
	if (!(hapd->iface->drv_flags & WPA_DRIVER_FLAGS_PROBE_RESP_OFFLOAD))
		return NULL;

#ifdef CONFIG_WPS
	if (hapd->conf->wps_state && hapd->wps_probe_resp_ie &&
	    (!(hapd->iface->probe_resp_offloads &
	       (WPA_DRIVER_PROBE_RESP_OFFLOAD_WPS |
		WPA_DRIVER_PROBE_RESP_OFFLOAD_WPS2))))
		wpa_printf(MSG_WARNING, "Device is trying to offload WPS "
			   "Probe Response while not supporting this");
#endif /* CONFIG_WPS */

#ifdef CONFIG_P2P
	if ((hapd->conf->p2p & P2P_ENABLED) && hapd->p2p_probe_resp_ie &&
	    !(hapd->iface->probe_resp_offloads &
	      WPA_DRIVER_PROBE_RESP_OFFLOAD_P2P))
		wpa_printf(MSG_WARNING, "Device is trying to offload P2P "
			   "Probe Response while not supporting this");
#endif  /* CONFIG_P2P */

	if (hapd->conf->interworking &&
	    !(hapd->iface->probe_resp_offloads &
	      WPA_DRIVER_PROBE_RESP_OFFLOAD_INTERWORKING))
		wpa_printf(MSG_WARNING, "Device is trying to offload "
			   "Interworking Probe Response while not supporting "
			   "this");

	/* Generate a Probe Response template for the non-P2P case */
	return hostapd_gen_probe_resp(hapd, NULL, NULL, 0, resp_len);
}

#endif /* NEED_AP_MLME */


void ieee802_11_set_beacon(struct hostapd_data *hapd)
{
	struct ieee80211_mgmt *head = NULL;
	u8 *tail = NULL;
	size_t head_len = 0, tail_len = 0;
	u8 *resp = NULL;
	size_t resp_len = 0;
	struct wpa_driver_ap_params params;
	struct wpabuf *beacon, *proberesp, *assocresp;
#ifdef NEED_AP_MLME
	u16 capab_info;
	u8 *pos, *tailpos;
#endif /* NEED_AP_MLME */

	hapd->beacon_set_done = 1;

#ifdef NEED_AP_MLME

#define BEACON_HEAD_BUF_SIZE 256
#define BEACON_TAIL_BUF_SIZE 512
	head = os_zalloc(BEACON_HEAD_BUF_SIZE);
	tail_len = BEACON_TAIL_BUF_SIZE;
#ifdef CONFIG_WPS
	if (hapd->conf->wps_state && hapd->wps_beacon_ie)
		tail_len += wpabuf_len(hapd->wps_beacon_ie);
#endif /* CONFIG_WPS */
#ifdef CONFIG_P2P
	if (hapd->p2p_beacon_ie)
		tail_len += wpabuf_len(hapd->p2p_beacon_ie);
#endif /* CONFIG_P2P */
	if (hapd->conf->vendor_elements)
		tail_len += wpabuf_len(hapd->conf->vendor_elements);
	tailpos = tail = os_malloc(tail_len);
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

	/* RSN, MDIE, WPA */
	tailpos = hostapd_eid_wpa(hapd, tailpos, tail + BEACON_TAIL_BUF_SIZE -
				  tailpos);

#ifdef CONFIG_IEEE80211N
	tailpos = hostapd_eid_ht_capabilities(hapd, tailpos);
	tailpos = hostapd_eid_ht_operation(hapd, tailpos);
#endif /* CONFIG_IEEE80211N */

	tailpos = hostapd_eid_ext_capab(hapd, tailpos);

	/*
	 * TODO: Time Advertisement element should only be included in some
	 * DTIM Beacon frames.
	 */
	tailpos = hostapd_eid_time_adv(hapd, tailpos);

	tailpos = hostapd_eid_interworking(hapd, tailpos);
	tailpos = hostapd_eid_adv_proto(hapd, tailpos);
	tailpos = hostapd_eid_roaming_consortium(hapd, tailpos);

#ifdef CONFIG_IEEE80211AC
	tailpos = hostapd_eid_vht_capabilities(hapd, tailpos);
	tailpos = hostapd_eid_vht_operation(hapd, tailpos);
#endif /* CONFIG_IEEE80211AC */

	/* Wi-Fi Alliance WMM */
	tailpos = hostapd_eid_wmm(hapd, tailpos);

#ifdef CONFIG_WPS
	if (hapd->conf->wps_state && hapd->wps_beacon_ie) {
		os_memcpy(tailpos, wpabuf_head(hapd->wps_beacon_ie),
			  wpabuf_len(hapd->wps_beacon_ie));
		tailpos += wpabuf_len(hapd->wps_beacon_ie);
	}
#endif /* CONFIG_WPS */

#ifdef CONFIG_P2P
	if ((hapd->conf->p2p & P2P_ENABLED) && hapd->p2p_beacon_ie) {
		os_memcpy(tailpos, wpabuf_head(hapd->p2p_beacon_ie),
			  wpabuf_len(hapd->p2p_beacon_ie));
		tailpos += wpabuf_len(hapd->p2p_beacon_ie);
	}
#endif /* CONFIG_P2P */
#ifdef CONFIG_P2P_MANAGER
	if ((hapd->conf->p2p & (P2P_MANAGE | P2P_ENABLED | P2P_GROUP_OWNER)) ==
	    P2P_MANAGE)
		tailpos = hostapd_eid_p2p_manage(hapd, tailpos);
#endif /* CONFIG_P2P_MANAGER */

#ifdef CONFIG_HS20
	tailpos = hostapd_eid_hs20_indication(hapd, tailpos);
#endif /* CONFIG_HS20 */

	if (hapd->conf->vendor_elements) {
		os_memcpy(tailpos, wpabuf_head(hapd->conf->vendor_elements),
			  wpabuf_len(hapd->conf->vendor_elements));
		tailpos += wpabuf_len(hapd->conf->vendor_elements);
	}

	tail_len = tailpos > tail ? tailpos - tail : 0;

	resp = hostapd_probe_resp_offloads(hapd, &resp_len);
#endif /* NEED_AP_MLME */

	os_memset(&params, 0, sizeof(params));
	params.head = (u8 *) head;
	params.head_len = head_len;
	params.tail = tail;
	params.tail_len = tail_len;
	params.proberesp = resp;
	params.proberesp_len = resp_len;
	params.dtim_period = hapd->conf->dtim_period;
	params.beacon_int = hapd->iconf->beacon_int;
	params.basic_rates = hapd->iface->basic_rates;
	params.ssid = hapd->conf->ssid.ssid;
	params.ssid_len = hapd->conf->ssid.ssid_len;
	params.pairwise_ciphers = hapd->conf->rsn_pairwise ?
		hapd->conf->rsn_pairwise : hapd->conf->wpa_pairwise;
	params.group_cipher = hapd->conf->wpa_group;
	params.key_mgmt_suites = hapd->conf->wpa_key_mgmt;
	params.auth_algs = hapd->conf->auth_algs;
	params.wpa_version = hapd->conf->wpa;
	params.privacy = hapd->conf->ssid.wep.keys_set || hapd->conf->wpa ||
		(hapd->conf->ieee802_1x &&
		 (hapd->conf->default_wep_key_len ||
		  hapd->conf->individual_wep_key_len));
	switch (hapd->conf->ignore_broadcast_ssid) {
	case 0:
		params.hide_ssid = NO_SSID_HIDING;
		break;
	case 1:
		params.hide_ssid = HIDDEN_SSID_ZERO_LEN;
		break;
	case 2:
		params.hide_ssid = HIDDEN_SSID_ZERO_CONTENTS;
		break;
	}
	hostapd_build_ap_extra_ies(hapd, &beacon, &proberesp, &assocresp);
	params.beacon_ies = beacon;
	params.proberesp_ies = proberesp;
	params.assocresp_ies = assocresp;
	params.isolate = hapd->conf->isolate;
#ifdef NEED_AP_MLME
	params.cts_protect = !!(ieee802_11_erp_info(hapd) &
				ERP_INFO_USE_PROTECTION);
	params.preamble = hapd->iface->num_sta_no_short_preamble == 0 &&
		hapd->iconf->preamble == SHORT_PREAMBLE;
	if (hapd->iface->current_mode &&
	    hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G)
		params.short_slot_time =
			hapd->iface->num_sta_no_short_slot_time > 0 ? 0 : 1;
	else
		params.short_slot_time = -1;
	if (!hapd->iconf->ieee80211n || hapd->conf->disable_11n)
		params.ht_opmode = -1;
	else
		params.ht_opmode = hapd->iface->ht_op_mode;
#endif /* NEED_AP_MLME */
	params.interworking = hapd->conf->interworking;
	if (hapd->conf->interworking &&
	    !is_zero_ether_addr(hapd->conf->hessid))
		params.hessid = hapd->conf->hessid;
	params.access_network_type = hapd->conf->access_network_type;
	params.ap_max_inactivity = hapd->conf->ap_max_inactivity;
#ifdef CONFIG_HS20
	params.disable_dgaf = hapd->conf->disable_dgaf;
#endif /* CONFIG_HS20 */
	if (hostapd_drv_set_ap(hapd, &params))
		wpa_printf(MSG_ERROR, "Failed to set beacon parameters");
	hostapd_free_ap_extra_ies(hapd, beacon, proberesp, assocresp);

	os_free(tail);
	os_free(head);
	os_free(resp);
}


void ieee802_11_set_beacons(struct hostapd_iface *iface)
{
	size_t i;
	for (i = 0; i < iface->num_bss; i++)
		ieee802_11_set_beacon(iface->bss[i]);
}


/* only update beacons if started */
void ieee802_11_update_beacons(struct hostapd_iface *iface)
{
	size_t i;
	for (i = 0; i < iface->num_bss; i++)
		if (iface->bss[i]->beacon_set_done)
			ieee802_11_set_beacon(iface->bss[i]);
}

#endif /* CONFIG_NATIVE_WINDOWS */
