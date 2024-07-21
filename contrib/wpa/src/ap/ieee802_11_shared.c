/*
 * hostapd / IEEE 802.11 Management
 * Copyright (c) 2002-2024, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "common/ocv.h"
#include "common/wpa_ctrl.h"
#include "hostapd.h"
#include "sta_info.h"
#include "ap_config.h"
#include "ap_drv_ops.h"
#include "wpa_auth.h"
#include "dpp_hostapd.h"
#include "ieee802_11.h"


static u8 * hostapd_eid_timeout_interval(u8 *pos, u8 type, u32 value)
{
	*pos++ = WLAN_EID_TIMEOUT_INTERVAL;
	*pos++ = 5;
	*pos++ = type;
	WPA_PUT_LE32(pos, value);
	pos += 4;

	return pos;
}


u8 * hostapd_eid_assoc_comeback_time(struct hostapd_data *hapd,
				     struct sta_info *sta, u8 *eid)
{
	u32 timeout, tu;
	struct os_reltime now, passed;
	u8 type = WLAN_TIMEOUT_ASSOC_COMEBACK;

	os_get_reltime(&now);
	os_reltime_sub(&now, &sta->sa_query_start, &passed);
	tu = (passed.sec * 1000000 + passed.usec) / 1024;
	if (hapd->conf->assoc_sa_query_max_timeout > tu)
		timeout = hapd->conf->assoc_sa_query_max_timeout - tu;
	else
		timeout = 0;
	if (timeout < hapd->conf->assoc_sa_query_max_timeout)
		timeout++; /* add some extra time for local timers */

#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->conf->test_assoc_comeback_type != -1)
		type = hapd->conf->test_assoc_comeback_type;
#endif /* CONFIG_TESTING_OPTIONS */
	return hostapd_eid_timeout_interval(eid, type, timeout);
}


/* MLME-SAQuery.request */
void ieee802_11_send_sa_query_req(struct hostapd_data *hapd,
				  const u8 *addr, const u8 *trans_id)
{
#if defined(CONFIG_OCV) || defined(CONFIG_IEEE80211BE)
	struct sta_info *sta = ap_get_sta(hapd, addr);
#endif /* CONFIG_OCV || CONFIG_IEEE80211BE */
	struct ieee80211_mgmt *mgmt;
	u8 *oci_ie = NULL;
	u8 oci_ie_len = 0;
	u8 *end;
	const u8 *own_addr = hapd->own_addr;

	wpa_printf(MSG_DEBUG, "IEEE 802.11: Sending SA Query Request to "
		   MACSTR, MAC2STR(addr));
	wpa_hexdump(MSG_DEBUG, "IEEE 802.11: SA Query Transaction ID",
		    trans_id, WLAN_SA_QUERY_TR_ID_LEN);

#ifdef CONFIG_OCV
	if (sta && wpa_auth_uses_ocv(sta->wpa_sm)) {
		struct wpa_channel_info ci;

		if (hostapd_drv_channel_info(hapd, &ci) != 0) {
			wpa_printf(MSG_WARNING,
				   "Failed to get channel info for OCI element in SA Query Request");
			return;
		}
#ifdef CONFIG_TESTING_OPTIONS
		if (hapd->conf->oci_freq_override_saquery_req) {
			wpa_printf(MSG_INFO,
				   "TEST: Override OCI frequency %d -> %u MHz",
				   ci.frequency,
				   hapd->conf->oci_freq_override_saquery_req);
			ci.frequency =
				hapd->conf->oci_freq_override_saquery_req;
		}
#endif /* CONFIG_TESTING_OPTIONS */

		oci_ie_len = OCV_OCI_EXTENDED_LEN;
		oci_ie = os_zalloc(oci_ie_len);
		if (!oci_ie) {
			wpa_printf(MSG_WARNING,
				   "Failed to allocate buffer for OCI element in SA Query Request");
			return;
		}

		if (ocv_insert_extended_oci(&ci, oci_ie) < 0) {
			os_free(oci_ie);
			return;
		}
	}
#endif /* CONFIG_OCV */

	mgmt = os_zalloc(sizeof(*mgmt) + oci_ie_len);
	if (!mgmt) {
		wpa_printf(MSG_DEBUG,
			   "Failed to allocate buffer for SA Query Response frame");
		os_free(oci_ie);
		return;
	}

#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(hapd, sta))
		own_addr = hapd->mld->mld_addr;
#endif /* CONFIG_IEEE80211BE */

	mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_ACTION);
	os_memcpy(mgmt->da, addr, ETH_ALEN);
	os_memcpy(mgmt->sa, own_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, own_addr, ETH_ALEN);
	mgmt->u.action.category = WLAN_ACTION_SA_QUERY;
	mgmt->u.action.u.sa_query_req.action = WLAN_SA_QUERY_REQUEST;
	os_memcpy(mgmt->u.action.u.sa_query_req.trans_id, trans_id,
		  WLAN_SA_QUERY_TR_ID_LEN);
	end = mgmt->u.action.u.sa_query_req.variable;
#ifdef CONFIG_OCV
	if (oci_ie_len > 0) {
		os_memcpy(end, oci_ie, oci_ie_len);
		end += oci_ie_len;
	}
#endif /* CONFIG_OCV */
	if (hostapd_drv_send_mlme(hapd, mgmt, end - (u8 *) mgmt, 0, NULL, 0, 0)
	    < 0)
		wpa_printf(MSG_INFO, "ieee802_11_send_sa_query_req: send failed");

	os_free(mgmt);
	os_free(oci_ie);
}


static void ieee802_11_send_sa_query_resp(struct hostapd_data *hapd,
					  const u8 *sa, const u8 *trans_id)
{
	struct sta_info *sta;
	struct ieee80211_mgmt *resp;
	u8 *oci_ie = NULL;
	u8 oci_ie_len = 0;
	u8 *end;
	const u8 *own_addr = hapd->own_addr;

	wpa_printf(MSG_DEBUG, "IEEE 802.11: Received SA Query Request from "
		   MACSTR, MAC2STR(sa));
	wpa_hexdump(MSG_DEBUG, "IEEE 802.11: SA Query Transaction ID",
		    trans_id, WLAN_SA_QUERY_TR_ID_LEN);

	sta = ap_get_sta(hapd, sa);
	if (sta == NULL || !(sta->flags & WLAN_STA_ASSOC)) {
		wpa_printf(MSG_DEBUG, "IEEE 802.11: Ignore SA Query Request "
			   "from unassociated STA " MACSTR, MAC2STR(sa));
		return;
	}

#ifdef CONFIG_OCV
	if (wpa_auth_uses_ocv(sta->wpa_sm)) {
		struct wpa_channel_info ci;

		if (hostapd_drv_channel_info(hapd, &ci) != 0) {
			wpa_printf(MSG_WARNING,
				   "Failed to get channel info for OCI element in SA Query Response");
			return;
		}
#ifdef CONFIG_TESTING_OPTIONS
		if (hapd->conf->oci_freq_override_saquery_resp) {
			wpa_printf(MSG_INFO,
				   "TEST: Override OCI frequency %d -> %u MHz",
				   ci.frequency,
				   hapd->conf->oci_freq_override_saquery_resp);
			ci.frequency =
				hapd->conf->oci_freq_override_saquery_resp;
		}
#endif /* CONFIG_TESTING_OPTIONS */

		oci_ie_len = OCV_OCI_EXTENDED_LEN;
		oci_ie = os_zalloc(oci_ie_len);
		if (!oci_ie) {
			wpa_printf(MSG_WARNING,
				   "Failed to allocate buffer for for OCI element in SA Query Response");
			return;
		}

		if (ocv_insert_extended_oci(&ci, oci_ie) < 0) {
			os_free(oci_ie);
			return;
		}
	}
#endif /* CONFIG_OCV */

	resp = os_zalloc(sizeof(*resp) + oci_ie_len);
	if (!resp) {
		wpa_printf(MSG_DEBUG,
			   "Failed to allocate buffer for SA Query Response frame");
		os_free(oci_ie);
		return;
	}

	wpa_printf(MSG_DEBUG, "IEEE 802.11: Sending SA Query Response to "
		   MACSTR, MAC2STR(sa));

#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(hapd, sta))
		own_addr = hapd->mld->mld_addr;
#endif /* CONFIG_IEEE80211BE */

	resp->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_ACTION);
	os_memcpy(resp->da, sa, ETH_ALEN);
	os_memcpy(resp->sa, own_addr, ETH_ALEN);
	os_memcpy(resp->bssid, own_addr, ETH_ALEN);
	resp->u.action.category = WLAN_ACTION_SA_QUERY;
	resp->u.action.u.sa_query_req.action = WLAN_SA_QUERY_RESPONSE;
	os_memcpy(resp->u.action.u.sa_query_req.trans_id, trans_id,
		  WLAN_SA_QUERY_TR_ID_LEN);
	end = resp->u.action.u.sa_query_req.variable;
#ifdef CONFIG_OCV
	if (oci_ie_len > 0) {
		os_memcpy(end, oci_ie, oci_ie_len);
		end += oci_ie_len;
	}
#endif /* CONFIG_OCV */
	if (hostapd_drv_send_mlme(hapd, resp, end - (u8 *) resp, 0, NULL, 0, 0)
	    < 0)
		wpa_printf(MSG_INFO, "ieee80211_mgmt_sa_query_request: send failed");

	os_free(resp);
	os_free(oci_ie);
}


void ieee802_11_sa_query_action(struct hostapd_data *hapd,
				const struct ieee80211_mgmt *mgmt,
				size_t len)
{
	struct sta_info *sta;
	int i;
	const u8 *sa = mgmt->sa;
	const u8 action_type = mgmt->u.action.u.sa_query_resp.action;
	const u8 *trans_id = mgmt->u.action.u.sa_query_resp.trans_id;

	if (((const u8 *) mgmt) + len <
	    mgmt->u.action.u.sa_query_resp.variable) {
		wpa_printf(MSG_DEBUG,
			   "IEEE 802.11: Too short SA Query Action frame (len=%lu)",
			   (unsigned long) len);
		return;
	}
	if (is_multicast_ether_addr(mgmt->da)) {
		wpa_printf(MSG_DEBUG,
			   "IEEE 802.11: Ignore group-addressed SA Query frame (A1=" MACSTR " A2=" MACSTR ")",
			   MAC2STR(mgmt->da), MAC2STR(mgmt->sa));
		return;
	}

	sta = ap_get_sta(hapd, sa);

#ifdef CONFIG_OCV
	if (sta && wpa_auth_uses_ocv(sta->wpa_sm)) {
		struct ieee802_11_elems elems;
		struct wpa_channel_info ci;
		int tx_chanwidth;
		int tx_seg1_idx;
		size_t ies_len;
		const u8 *ies;

		ies = mgmt->u.action.u.sa_query_resp.variable;
		ies_len = len - (ies - (u8 *) mgmt);
		if (ieee802_11_parse_elems(ies, ies_len, &elems, 1) ==
		    ParseFailed) {
			wpa_printf(MSG_DEBUG,
				   "SA Query: Failed to parse elements");
			return;
		}

		if (hostapd_drv_channel_info(hapd, &ci) != 0) {
			wpa_printf(MSG_WARNING,
				   "Failed to get channel info to validate received OCI in SA Query Action frame");
			return;
		}

		if (get_sta_tx_parameters(sta->wpa_sm,
					  channel_width_to_int(ci.chanwidth),
					  ci.seg1_idx, &tx_chanwidth,
					  &tx_seg1_idx) < 0)
			return;

		if (ocv_verify_tx_params(elems.oci, elems.oci_len, &ci,
					 tx_chanwidth, tx_seg1_idx) !=
		    OCI_SUCCESS) {
			wpa_msg(hapd->msg_ctx, MSG_INFO, OCV_FAILURE "addr="
				MACSTR " frame=saquery%s error=%s",
				MAC2STR(sa),
				action_type == WLAN_SA_QUERY_REQUEST ?
				"req" : "resp", ocv_errorstr);
			return;
		}
	}
#endif /* CONFIG_OCV */

	if (action_type == WLAN_SA_QUERY_REQUEST) {
		if (sta)
			sta->post_csa_sa_query = 0;
		ieee802_11_send_sa_query_resp(hapd, sa, trans_id);
		return;
	}

	if (action_type != WLAN_SA_QUERY_RESPONSE) {
		wpa_printf(MSG_DEBUG, "IEEE 802.11: Unexpected SA Query "
			   "Action %d", action_type);
		return;
	}

	wpa_printf(MSG_DEBUG, "IEEE 802.11: Received SA Query Response from "
		   MACSTR, MAC2STR(sa));
	wpa_hexdump(MSG_DEBUG, "IEEE 802.11: SA Query Transaction ID",
		    trans_id, WLAN_SA_QUERY_TR_ID_LEN);

	/* MLME-SAQuery.confirm */

	if (sta == NULL || sta->sa_query_trans_id == NULL) {
		wpa_printf(MSG_DEBUG, "IEEE 802.11: No matching STA with "
			   "pending SA Query request found");
		return;
	}

	for (i = 0; i < sta->sa_query_count; i++) {
		if (os_memcmp(sta->sa_query_trans_id +
			      i * WLAN_SA_QUERY_TR_ID_LEN,
			      trans_id, WLAN_SA_QUERY_TR_ID_LEN) == 0)
			break;
	}

	if (i >= sta->sa_query_count) {
		wpa_printf(MSG_DEBUG, "IEEE 802.11: No matching SA Query "
			   "transaction identifier found");
		return;
	}

	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG,
		       "Reply to pending SA Query received");
	ap_sta_stop_sa_query(hapd, sta);
}


static void hostapd_ext_capab_byte(struct hostapd_data *hapd, u8 *pos, int idx,
				   bool mbssid_complete)
{
	*pos = 0x00;

	switch (idx) {
	case 0: /* Bits 0-7 */
		if (hapd->iconf->obss_interval)
			*pos |= 0x01; /* Bit 0 - Coexistence management */
		if (hapd->iface->drv_flags & WPA_DRIVER_FLAGS_AP_CSA)
			*pos |= 0x04; /* Bit 2 - Extended Channel Switching */
		break;
	case 1: /* Bits 8-15 */
		if (hapd->conf->proxy_arp)
			*pos |= 0x10; /* Bit 12 - Proxy ARP */
		if (hapd->conf->coloc_intf_reporting) {
			/* Bit 13 - Collocated Interference Reporting */
			*pos |= 0x20;
		}
		break;
	case 2: /* Bits 16-23 */
		if (hapd->conf->wnm_sleep_mode)
			*pos |= 0x02; /* Bit 17 - WNM-Sleep Mode */
		if (hapd->conf->bss_transition)
			*pos |= 0x08; /* Bit 19 - BSS Transition */
		if (hapd->iconf->mbssid)
			*pos |= 0x40; /* Bit 22 - Multiple BSSID */
		break;
	case 3: /* Bits 24-31 */
#ifdef CONFIG_WNM_AP
		*pos |= 0x02; /* Bit 25 - SSID List */
#endif /* CONFIG_WNM_AP */
		if (hapd->conf->time_advertisement == 2)
			*pos |= 0x08; /* Bit 27 - UTC TSF Offset */
		if (hapd->conf->interworking)
			*pos |= 0x80; /* Bit 31 - Interworking */
		break;
	case 4: /* Bits 32-39 */
		if (hapd->conf->qos_map_set_len)
			*pos |= 0x01; /* Bit 32 - QoS Map */
		if (hapd->conf->tdls & TDLS_PROHIBIT)
			*pos |= 0x40; /* Bit 38 - TDLS Prohibited */
		if (hapd->conf->tdls & TDLS_PROHIBIT_CHAN_SWITCH) {
			/* Bit 39 - TDLS Channel Switching Prohibited */
			*pos |= 0x80;
		}
		break;
	case 5: /* Bits 40-47 */
#ifdef CONFIG_HS20
		if (hapd->conf->hs20)
			*pos |= 0x40; /* Bit 46 - WNM-Notification */
#endif /* CONFIG_HS20 */
#ifdef CONFIG_MBO
		if (hapd->conf->mbo_enabled)
			*pos |= 0x40; /* Bit 46 - WNM-Notification */
#endif /* CONFIG_MBO */
		break;
	case 6: /* Bits 48-55 */
		if (hapd->conf->ssid.utf8_ssid)
			*pos |= 0x01; /* Bit 48 - UTF-8 SSID */
		break;
	case 7: /* Bits 56-63 */
		break;
	case 8: /* Bits 64-71 */
		if (hapd->conf->ftm_responder)
			*pos |= 0x40; /* Bit 70 - FTM responder */
		if (hapd->conf->ftm_initiator)
			*pos |= 0x80; /* Bit 71 - FTM initiator */
		break;
	case 9: /* Bits 72-79 */
#ifdef CONFIG_FILS
		if ((hapd->conf->wpa & WPA_PROTO_RSN) &&
		    wpa_key_mgmt_fils(hapd->conf->wpa_key_mgmt))
			*pos |= 0x01;
#endif /* CONFIG_FILS */
#ifdef CONFIG_IEEE80211AX
		if (hostapd_get_he_twt_responder(hapd, IEEE80211_MODE_AP))
			*pos |= 0x40; /* Bit 78 - TWT responder */
#endif /* CONFIG_IEEE80211AX */
		if (hostapd_get_ht_vht_twt_responder(hapd))
			*pos |= 0x40; /* Bit 78 - TWT responder */
		break;
	case 10: /* Bits 80-87 */
#ifdef CONFIG_SAE
		if (hapd->conf->wpa &&
		    wpa_key_mgmt_sae(hapd->conf->wpa_key_mgmt)) {
			int in_use = hostapd_sae_pw_id_in_use(hapd->conf);

			if (in_use)
				*pos |= 0x02; /* Bit 81 - SAE Password
					       * Identifiers In Use */
			if (in_use == 2)
				*pos |= 0x04; /* Bit 82 - SAE Password
					       * Identifiers Used Exclusively */
		}
#endif /* CONFIG_SAE */
		if (hapd->conf->beacon_prot &&
		    (hapd->iface->drv_flags &
		     WPA_DRIVER_FLAGS_BEACON_PROTECTION))
			*pos |= 0x10; /* Bit 84 - Beacon Protection Enabled */
		if (hapd->iconf->mbssid == ENHANCED_MBSSID_ENABLED)
			*pos |= 0x08; /* Bit 83 - Enhanced multiple BSSID */
		if (mbssid_complete)
			*pos |= 0x01; /* Bit 80 - Complete List of NonTxBSSID
				       * Profiles */
		break;
	case 11: /* Bits 88-95 */
#ifdef CONFIG_SAE_PK
		if (hapd->conf->wpa &&
		    wpa_key_mgmt_sae(hapd->conf->wpa_key_mgmt) &&
		    hostapd_sae_pk_exclusively(hapd->conf))
			*pos |= 0x01; /* Bit 88 - SAE PK Exclusively */
#endif /* CONFIG_SAE_PK */
		break;
	}
}


u8 * hostapd_eid_ext_capab(struct hostapd_data *hapd, u8 *eid,
			   bool mbssid_complete)
{
	u8 *pos = eid;
	u8 len = EXT_CAPA_MAX_LEN, i;

	if (len < hapd->iface->extended_capa_len)
		len = hapd->iface->extended_capa_len;

	*pos++ = WLAN_EID_EXT_CAPAB;
	*pos++ = len;
	for (i = 0; i < len; i++, pos++) {
		hostapd_ext_capab_byte(hapd, pos, i, mbssid_complete);

		if (i < hapd->iface->extended_capa_len) {
			*pos &= ~hapd->iface->extended_capa_mask[i];
			*pos |= hapd->iface->extended_capa[i];
		}

		if (i < EXT_CAPA_MAX_LEN) {
			*pos &= ~hapd->conf->ext_capa_mask[i];
			*pos |= hapd->conf->ext_capa[i];
		}

		/* Clear bits 83 and 22 if EMA and MBSSID are not enabled
		 * otherwise association fails with some clients */
		if (i == 10 && hapd->iconf->mbssid < ENHANCED_MBSSID_ENABLED)
			*pos &= ~0x08;
		if (i == 2 && !hapd->iconf->mbssid)
			*pos &= ~0x40;
	}

	while (len > 0 && eid[1 + len] == 0) {
		len--;
		eid[1] = len;
	}
	if (len == 0)
		return eid;

	return eid + 2 + len;
}


u8 * hostapd_eid_qos_map_set(struct hostapd_data *hapd, u8 *eid)
{
	u8 *pos = eid;
	u8 len = hapd->conf->qos_map_set_len;

	if (!len)
		return eid;

	*pos++ = WLAN_EID_QOS_MAP_SET;
	*pos++ = len;
	os_memcpy(pos, hapd->conf->qos_map_set, len);
	pos += len;

	return pos;
}


u8 * hostapd_eid_interworking(struct hostapd_data *hapd, u8 *eid)
{
	u8 *pos = eid;
#ifdef CONFIG_INTERWORKING
	u8 *len;

	if (!hapd->conf->interworking)
		return eid;

	*pos++ = WLAN_EID_INTERWORKING;
	len = pos++;

	*pos = hapd->conf->access_network_type;
	if (hapd->conf->internet)
		*pos |= INTERWORKING_ANO_INTERNET;
	if (hapd->conf->asra)
		*pos |= INTERWORKING_ANO_ASRA;
	if (hapd->conf->esr)
		*pos |= INTERWORKING_ANO_ESR;
	if (hapd->conf->uesa)
		*pos |= INTERWORKING_ANO_UESA;
	pos++;

	if (hapd->conf->venue_info_set) {
		*pos++ = hapd->conf->venue_group;
		*pos++ = hapd->conf->venue_type;
	}

	if (!is_zero_ether_addr(hapd->conf->hessid)) {
		os_memcpy(pos, hapd->conf->hessid, ETH_ALEN);
		pos += ETH_ALEN;
	}

	*len = pos - len - 1;
#endif /* CONFIG_INTERWORKING */

	return pos;
}


u8 * hostapd_eid_adv_proto(struct hostapd_data *hapd, u8 *eid)
{
	u8 *pos = eid;
#ifdef CONFIG_INTERWORKING

	/* TODO: Separate configuration for ANQP? */
	if (!hapd->conf->interworking)
		return eid;

	*pos++ = WLAN_EID_ADV_PROTO;
	*pos++ = 2;
	*pos++ = 0x7F; /* Query Response Length Limit | PAME-BI */
	*pos++ = ACCESS_NETWORK_QUERY_PROTOCOL;
#endif /* CONFIG_INTERWORKING */

	return pos;
}


u8 * hostapd_eid_roaming_consortium(struct hostapd_data *hapd, u8 *eid)
{
	u8 *pos = eid;
#ifdef CONFIG_INTERWORKING
	u8 *len;
	unsigned int i, count;

	if (!hapd->conf->interworking ||
	    hapd->conf->roaming_consortium == NULL ||
	    hapd->conf->roaming_consortium_count == 0)
		return eid;

	*pos++ = WLAN_EID_ROAMING_CONSORTIUM;
	len = pos++;

	/* Number of ANQP OIs (in addition to the max 3 listed here) */
	if (hapd->conf->roaming_consortium_count > 3 + 255)
		*pos++ = 255;
	else if (hapd->conf->roaming_consortium_count > 3)
		*pos++ = hapd->conf->roaming_consortium_count - 3;
	else
		*pos++ = 0;

	/* OU #1 and #2 Lengths */
	*pos = hapd->conf->roaming_consortium[0].len;
	if (hapd->conf->roaming_consortium_count > 1)
		*pos |= hapd->conf->roaming_consortium[1].len << 4;
	pos++;

	if (hapd->conf->roaming_consortium_count > 3)
		count = 3;
	else
		count = hapd->conf->roaming_consortium_count;

	for (i = 0; i < count; i++) {
		os_memcpy(pos, hapd->conf->roaming_consortium[i].oi,
			  hapd->conf->roaming_consortium[i].len);
		pos += hapd->conf->roaming_consortium[i].len;
	}

	*len = pos - len - 1;
#endif /* CONFIG_INTERWORKING */

	return pos;
}


u8 * hostapd_eid_time_adv(struct hostapd_data *hapd, u8 *eid)
{
	if (hapd->conf->time_advertisement != 2)
		return eid;

	if (hapd->time_adv == NULL &&
	    hostapd_update_time_adv(hapd) < 0)
		return eid;

	if (hapd->time_adv == NULL)
		return eid;

	os_memcpy(eid, wpabuf_head(hapd->time_adv),
		  wpabuf_len(hapd->time_adv));
	eid += wpabuf_len(hapd->time_adv);

	return eid;
}


u8 * hostapd_eid_time_zone(struct hostapd_data *hapd, u8 *eid)
{
	size_t len;

	if (hapd->conf->time_advertisement != 2 || !hapd->conf->time_zone)
		return eid;

	len = os_strlen(hapd->conf->time_zone);

	*eid++ = WLAN_EID_TIME_ZONE;
	*eid++ = len;
	os_memcpy(eid, hapd->conf->time_zone, len);
	eid += len;

	return eid;
}


int hostapd_update_time_adv(struct hostapd_data *hapd)
{
	const int elen = 2 + 1 + 10 + 5 + 1;
	struct os_time t;
	struct os_tm tm;
	u8 *pos;

	if (hapd->conf->time_advertisement != 2)
		return 0;

	if (os_get_time(&t) < 0 || os_gmtime(t.sec, &tm) < 0)
		return -1;

	if (!hapd->time_adv) {
		hapd->time_adv = wpabuf_alloc(elen);
		if (hapd->time_adv == NULL)
			return -1;
		pos = wpabuf_put(hapd->time_adv, elen);
	} else
		pos = wpabuf_mhead_u8(hapd->time_adv);

	*pos++ = WLAN_EID_TIME_ADVERTISEMENT;
	*pos++ = 1 + 10 + 5 + 1;

	*pos++ = 2; /* UTC time at which the TSF timer is 0 */

	/* Time Value at TSF 0 */
	/* FIX: need to calculate this based on the current TSF value */
	WPA_PUT_LE16(pos, tm.year); /* Year */
	pos += 2;
	*pos++ = tm.month; /* Month */
	*pos++ = tm.day; /* Day of month */
	*pos++ = tm.hour; /* Hours */
	*pos++ = tm.min; /* Minutes */
	*pos++ = tm.sec; /* Seconds */
	WPA_PUT_LE16(pos, 0); /* Milliseconds (not used) */
	pos += 2;
	*pos++ = 0; /* Reserved */

	/* Time Error */
	/* TODO: fill in an estimate on the error */
	*pos++ = 0;
	*pos++ = 0;
	*pos++ = 0;
	*pos++ = 0;
	*pos++ = 0;

	*pos++ = hapd->time_update_counter++;

	return 0;
}


u8 * hostapd_eid_bss_max_idle_period(struct hostapd_data *hapd, u8 *eid,
				     u16 value)
{
	u8 *pos = eid;

#ifdef CONFIG_WNM_AP
	if (hapd->conf->ap_max_inactivity > 0 &&
	    hapd->conf->bss_max_idle) {
		unsigned int val;
		*pos++ = WLAN_EID_BSS_MAX_IDLE_PERIOD;
		*pos++ = 3;
		val = hapd->conf->ap_max_inactivity;
		if (val > 68000)
			val = 68000;
		val *= 1000;
		val /= 1024;
		if (val == 0)
			val = 1;
		if (val > 65535)
			val = 65535;
		if (value)
			val = value;
		WPA_PUT_LE16(pos, val);
		pos += 2;
		/* Set the Protected Keep-Alive Required bit based on
		 * configuration */
		*pos++ = hapd->conf->bss_max_idle == 2 ? BIT(0) : 0x00;
	}
#endif /* CONFIG_WNM_AP */

	return pos;
}


#ifdef CONFIG_MBO

u8 * hostapd_eid_mbo_rssi_assoc_rej(struct hostapd_data *hapd, u8 *eid,
				    size_t len, int delta)
{
	u8 mbo[4];

	mbo[0] = OCE_ATTR_ID_RSSI_BASED_ASSOC_REJECT;
	mbo[1] = 2;
	/* Delta RSSI */
	mbo[2] = delta;
	/* Retry delay */
	mbo[3] = hapd->iconf->rssi_reject_assoc_timeout;

	return eid + mbo_add_ie(eid, len, mbo, 4);
}


u8 * hostapd_eid_mbo(struct hostapd_data *hapd, u8 *eid, size_t len)
{
	u8 mbo[9], *mbo_pos = mbo;
	u8 *pos = eid;

	if (!hapd->conf->mbo_enabled &&
	    !OCE_STA_CFON_ENABLED(hapd) && !OCE_AP_ENABLED(hapd))
		return eid;

	if (hapd->conf->mbo_enabled) {
		*mbo_pos++ = MBO_ATTR_ID_AP_CAPA_IND;
		*mbo_pos++ = 1;
		/* Not Cellular aware */
		*mbo_pos++ = 0;
	}

	if (hapd->conf->mbo_enabled && hapd->mbo_assoc_disallow) {
		*mbo_pos++ = MBO_ATTR_ID_ASSOC_DISALLOW;
		*mbo_pos++ = 1;
		*mbo_pos++ = hapd->mbo_assoc_disallow;
	}

	if (OCE_STA_CFON_ENABLED(hapd) || OCE_AP_ENABLED(hapd)) {
		u8 ctrl;

		ctrl = OCE_RELEASE;
		if (OCE_STA_CFON_ENABLED(hapd) && !OCE_AP_ENABLED(hapd))
			ctrl |= OCE_IS_STA_CFON;

		*mbo_pos++ = OCE_ATTR_ID_CAPA_IND;
		*mbo_pos++ = 1;
		*mbo_pos++ = ctrl;
	}

	pos += mbo_add_ie(pos, len, mbo, mbo_pos - mbo);

	return pos;
}


u8 hostapd_mbo_ie_len(struct hostapd_data *hapd)
{
	u8 len;

	if (!hapd->conf->mbo_enabled &&
	    !OCE_STA_CFON_ENABLED(hapd) && !OCE_AP_ENABLED(hapd))
		return 0;

	/*
	 * MBO IE header (6) + Capability Indication attribute (3) +
	 * Association Disallowed attribute (3) = 12
	 */
	len = 6;
	if (hapd->conf->mbo_enabled)
		len += 3 + (hapd->mbo_assoc_disallow ? 3 : 0);

	/* OCE capability indication attribute (3) */
	if (OCE_STA_CFON_ENABLED(hapd) || OCE_AP_ENABLED(hapd))
		len += 3;

	return len;
}

#endif /* CONFIG_MBO */


#ifdef CONFIG_OWE
static int hostapd_eid_owe_trans_enabled(struct hostapd_data *hapd)
{
	return hapd->conf->owe_transition_ssid_len > 0 &&
		!is_zero_ether_addr(hapd->conf->owe_transition_bssid);
}
#endif /* CONFIG_OWE */


size_t hostapd_eid_owe_trans_len(struct hostapd_data *hapd)
{
#ifdef CONFIG_OWE
	if (!hostapd_eid_owe_trans_enabled(hapd))
		return 0;
	return 6 + ETH_ALEN + 1 + hapd->conf->owe_transition_ssid_len;
#else /* CONFIG_OWE */
	return 0;
#endif /* CONFIG_OWE */
}


u8 * hostapd_eid_owe_trans(struct hostapd_data *hapd, u8 *eid,
				  size_t len)
{
#ifdef CONFIG_OWE
	u8 *pos = eid;
	size_t elen;

	if (hapd->conf->owe_transition_ifname[0] &&
	    !hostapd_eid_owe_trans_enabled(hapd))
		hostapd_owe_trans_get_info(hapd);

	if (!hostapd_eid_owe_trans_enabled(hapd))
		return pos;

	elen = hostapd_eid_owe_trans_len(hapd);
	if (len < elen) {
		wpa_printf(MSG_DEBUG,
			   "OWE: Not enough room in the buffer for OWE IE");
		return pos;
	}

	*pos++ = WLAN_EID_VENDOR_SPECIFIC;
	*pos++ = elen - 2;
	WPA_PUT_BE24(pos, OUI_WFA);
	pos += 3;
	*pos++ = OWE_OUI_TYPE;
	os_memcpy(pos, hapd->conf->owe_transition_bssid, ETH_ALEN);
	pos += ETH_ALEN;
	*pos++ = hapd->conf->owe_transition_ssid_len;
	os_memcpy(pos, hapd->conf->owe_transition_ssid,
		  hapd->conf->owe_transition_ssid_len);
	pos += hapd->conf->owe_transition_ssid_len;

	return pos;
#else /* CONFIG_OWE */
	return eid;
#endif /* CONFIG_OWE */
}


size_t hostapd_eid_dpp_cc_len(struct hostapd_data *hapd)
{
#ifdef CONFIG_DPP2
	if (hostapd_dpp_configurator_connectivity(hapd))
		return 6;
#endif /* CONFIG_DPP2 */
	return 0;
}


u8 * hostapd_eid_dpp_cc(struct hostapd_data *hapd, u8 *eid, size_t len)
{
	u8 *pos = eid;

#ifdef CONFIG_DPP2
	if (!hostapd_dpp_configurator_connectivity(hapd) || len < 6)
		return pos;

	*pos++ = WLAN_EID_VENDOR_SPECIFIC;
	*pos++ = 4;
	WPA_PUT_BE24(pos, OUI_WFA);
	pos += 3;
	*pos++ = DPP_CC_OUI_TYPE;
#endif /* CONFIG_DPP2 */

	return pos;
}


void ap_copy_sta_supp_op_classes(struct sta_info *sta,
				 const u8 *supp_op_classes,
				 size_t supp_op_classes_len)
{
	if (!supp_op_classes)
		return;
	os_free(sta->supp_op_classes);
	sta->supp_op_classes = os_malloc(1 + supp_op_classes_len);
	if (!sta->supp_op_classes)
		return;

	sta->supp_op_classes[0] = supp_op_classes_len;
	os_memcpy(sta->supp_op_classes + 1, supp_op_classes,
		  supp_op_classes_len);
}


u8 * hostapd_eid_fils_indic(struct hostapd_data *hapd, u8 *eid, int hessid)
{
	u8 *pos = eid;
#ifdef CONFIG_FILS
	u8 *len;
	u16 fils_info = 0;
	size_t realms;
	struct fils_realm *realm;

	if (!(hapd->conf->wpa & WPA_PROTO_RSN) ||
	    !wpa_key_mgmt_fils(hapd->conf->wpa_key_mgmt))
		return pos;

	realms = dl_list_len(&hapd->conf->fils_realms);
	if (realms > 7)
		realms = 7; /* 3 bit count field limits this to max 7 */

	*pos++ = WLAN_EID_FILS_INDICATION;
	len = pos++;
	/* TODO: B0..B2: Number of Public Key Identifiers */
	if (hapd->conf->erp_domain) {
		/* B3..B5: Number of Realm Identifiers */
		fils_info |= realms << 3;
	}
	/* TODO: B6: FILS IP Address Configuration */
	if (hapd->conf->fils_cache_id_set)
		fils_info |= BIT(7);
	if (hessid && !is_zero_ether_addr(hapd->conf->hessid))
		fils_info |= BIT(8); /* HESSID Included */
	/* FILS Shared Key Authentication without PFS Supported */
	fils_info |= BIT(9);
	if (hapd->conf->fils_dh_group) {
		/* FILS Shared Key Authentication with PFS Supported */
		fils_info |= BIT(10);
	}
	/* TODO: B11: FILS Public Key Authentication Supported */
	/* B12..B15: Reserved */
	WPA_PUT_LE16(pos, fils_info);
	pos += 2;
	if (hapd->conf->fils_cache_id_set) {
		os_memcpy(pos, hapd->conf->fils_cache_id, FILS_CACHE_ID_LEN);
		pos += FILS_CACHE_ID_LEN;
	}
	if (hessid && !is_zero_ether_addr(hapd->conf->hessid)) {
		os_memcpy(pos, hapd->conf->hessid, ETH_ALEN);
		pos += ETH_ALEN;
	}

	dl_list_for_each(realm, &hapd->conf->fils_realms, struct fils_realm,
			 list) {
		if (realms == 0)
			break;
		realms--;
		os_memcpy(pos, realm->hash, 2);
		pos += 2;
	}
	*len = pos - len - 1;
#endif /* CONFIG_FILS */

	return pos;
}


#ifdef CONFIG_OCV
int get_tx_parameters(struct sta_info *sta, int ap_max_chanwidth,
		      int ap_seg1_idx, int *bandwidth, int *seg1_idx)
{
	int ht_40mhz = 0;
	int vht_80p80 = 0;
	int requested_bw;

	if (sta->ht_capabilities)
		ht_40mhz = !!(sta->ht_capabilities->ht_capabilities_info &
			      HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET);

	if (sta->vht_operation) {
		struct ieee80211_vht_operation *oper = sta->vht_operation;

		/*
		 * If a VHT Operation element was present, use it to determine
		 * the supported channel bandwidth.
		 */
		if (oper->vht_op_info_chwidth == CHANWIDTH_USE_HT) {
			requested_bw = ht_40mhz ? 40 : 20;
		} else if (oper->vht_op_info_chan_center_freq_seg1_idx == 0) {
			requested_bw = 80;
		} else {
			int diff;

			requested_bw = 160;
			diff = abs((int)
				   oper->vht_op_info_chan_center_freq_seg0_idx -
				   (int)
				   oper->vht_op_info_chan_center_freq_seg1_idx);
			vht_80p80 = oper->vht_op_info_chan_center_freq_seg1_idx
				!= 0 &&	diff > 16;
		}
	} else if (sta->vht_capabilities) {
		struct ieee80211_vht_capabilities *capab;
		int vht_chanwidth;

		capab = sta->vht_capabilities;

		/*
		 * If only the VHT Capabilities element is present (e.g., for
		 * normal clients), use it to determine the supported channel
		 * bandwidth.
		 */
		vht_chanwidth = capab->vht_capabilities_info &
			VHT_CAP_SUPP_CHAN_WIDTH_MASK;
		vht_80p80 = capab->vht_capabilities_info &
			VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ;

		/* TODO: Also take into account Extended NSS BW Support field */
		requested_bw = vht_chanwidth ? 160 : 80;
	} else {
		requested_bw = ht_40mhz ? 40 : 20;
	}

	*bandwidth = requested_bw < ap_max_chanwidth ?
		requested_bw : ap_max_chanwidth;

	*seg1_idx = 0;
	if (ap_seg1_idx && vht_80p80)
		*seg1_idx = ap_seg1_idx;

	return 0;
}
#endif /* CONFIG_OCV */


u8 * hostapd_eid_rsnxe(struct hostapd_data *hapd, u8 *eid, size_t len)
{
	u8 *pos = eid;
	bool sae_pk = false;
	u32 capab = 0, tmp;
	size_t flen;

	if (!(hapd->conf->wpa & WPA_PROTO_RSN))
		return eid;

#ifdef CONFIG_SAE_PK
	sae_pk = hostapd_sae_pk_in_use(hapd->conf);
#endif /* CONFIG_SAE_PK */

	if (wpa_key_mgmt_sae(hapd->conf->wpa_key_mgmt) &&
	    (hapd->conf->sae_pwe == SAE_PWE_HASH_TO_ELEMENT ||
	     hapd->conf->sae_pwe == SAE_PWE_BOTH ||
	     hostapd_sae_pw_id_in_use(hapd->conf) || sae_pk ||
	     wpa_key_mgmt_sae_ext_key(hapd->conf->wpa_key_mgmt)) &&
	    hapd->conf->sae_pwe != SAE_PWE_FORCE_HUNT_AND_PECK) {
		capab |= BIT(WLAN_RSNX_CAPAB_SAE_H2E);
#ifdef CONFIG_SAE_PK
		if (sae_pk)
			capab |= BIT(WLAN_RSNX_CAPAB_SAE_PK);
#endif /* CONFIG_SAE_PK */
	}

	if (hapd->iface->drv_flags2 & WPA_DRIVER_FLAGS2_SEC_LTF_AP)
		capab |= BIT(WLAN_RSNX_CAPAB_SECURE_LTF);
	if (hapd->iface->drv_flags2 & WPA_DRIVER_FLAGS2_SEC_RTT_AP)
		capab |= BIT(WLAN_RSNX_CAPAB_SECURE_RTT);
	if (hapd->iface->drv_flags2 & WPA_DRIVER_FLAGS2_PROT_RANGE_NEG_AP)
		capab |= BIT(WLAN_RSNX_CAPAB_URNM_MFPR);
	if (hapd->conf->ssid_protection)
		capab |= BIT(WLAN_RSNX_CAPAB_SSID_PROTECTION);

	if (!capab)
		return eid; /* no supported extended RSN capabilities */
	tmp = capab;
	flen = 0;
	while (tmp) {
		flen++;
		tmp >>= 8;
	}

	if (len < 2 + flen)
		return eid; /* no supported extended RSN capabilities */
	capab |= flen - 1; /* bit 0-3 = Field length (n - 1) */

	*pos++ = WLAN_EID_RSNX;
	*pos++ = flen;
	while (capab) {
		*pos++ = capab & 0xff;
		capab >>= 8;
	}

	return pos;
}


u16 check_ext_capab(struct hostapd_data *hapd, struct sta_info *sta,
		    const u8 *ext_capab_ie, size_t ext_capab_ie_len)
{
	/* check for QoS Map support */
	if (ext_capab_ie_len >= 5) {
		if (ext_capab_ie[4] & 0x01)
			sta->qos_map_enabled = 1;
	}

	if (ext_capab_ie_len > 0) {
		sta->ecsa_supported = !!(ext_capab_ie[0] & BIT(2));
		os_free(sta->ext_capability);
		sta->ext_capability = os_malloc(1 + ext_capab_ie_len);
		if (sta->ext_capability) {
			sta->ext_capability[0] = ext_capab_ie_len;
			os_memcpy(sta->ext_capability + 1, ext_capab_ie,
				  ext_capab_ie_len);
		}
	}

	return WLAN_STATUS_SUCCESS;
}


struct sta_info * hostapd_ml_get_assoc_sta(struct hostapd_data *hapd,
					   struct sta_info *sta,
					   struct hostapd_data **assoc_hapd)
{
#ifdef CONFIG_IEEE80211BE
	struct hostapd_data *other_hapd = NULL;
	struct sta_info *tmp_sta;

	if (!ap_sta_is_mld(hapd, sta))
		return NULL;

	*assoc_hapd = hapd;

	/* The station is the one on which the association was performed */
	if (sta->mld_assoc_link_id == hapd->mld_link_id)
		return sta;

	other_hapd = hostapd_mld_get_link_bss(hapd, sta->mld_assoc_link_id);
	if (!other_hapd) {
		wpa_printf(MSG_DEBUG, "MLD: No link match for link_id=%u",
			   sta->mld_assoc_link_id);
		return sta;
	}

	/*
	 * Iterate over the stations and find the one with the matching link ID
	 * and association ID.
	 */
	for (tmp_sta = other_hapd->sta_list; tmp_sta; tmp_sta = tmp_sta->next) {
		if (tmp_sta->mld_assoc_link_id == sta->mld_assoc_link_id &&
		    tmp_sta->aid == sta->aid) {
			*assoc_hapd = other_hapd;
			return tmp_sta;
		}
	}
#endif /* CONFIG_IEEE80211BE */

	return sta;
}


bool hostapd_get_ht_vht_twt_responder(struct hostapd_data *hapd)
{
	return hapd->iconf->ht_vht_twt_responder &&
		((hapd->iconf->ieee80211n && !hapd->conf->disable_11n) ||
		 (hapd->iconf->ieee80211ac && !hapd->conf->disable_11ac)) &&
		(hapd->iface->drv_flags2 &
		 WPA_DRIVER_FLAGS2_HT_VHT_TWT_RESPONDER);
}
