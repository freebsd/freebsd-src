/*
 * hostapd / Callback functions for driver wrappers
 * Copyright (c) 2002-2009, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "radius/radius.h"
#include "drivers/driver.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "crypto/random.h"
#include "p2p/p2p.h"
#include "wps/wps.h"
#include "wnm_ap.h"
#include "hostapd.h"
#include "ieee802_11.h"
#include "sta_info.h"
#include "accounting.h"
#include "tkip_countermeasures.h"
#include "ieee802_1x.h"
#include "wpa_auth.h"
#include "wps_hostapd.h"
#include "ap_drv_ops.h"
#include "ap_config.h"
#include "hw_features.h"


int hostapd_notif_assoc(struct hostapd_data *hapd, const u8 *addr,
			const u8 *req_ies, size_t req_ies_len, int reassoc)
{
	struct sta_info *sta;
	int new_assoc, res;
	struct ieee802_11_elems elems;
	const u8 *ie;
	size_t ielen;
#ifdef CONFIG_IEEE80211R
	u8 buf[sizeof(struct ieee80211_mgmt) + 1024];
	u8 *p = buf;
#endif /* CONFIG_IEEE80211R */
	u16 reason = WLAN_REASON_UNSPECIFIED;
	u16 status = WLAN_STATUS_SUCCESS;

	if (addr == NULL) {
		/*
		 * This could potentially happen with unexpected event from the
		 * driver wrapper. This was seen at least in one case where the
		 * driver ended up being set to station mode while hostapd was
		 * running, so better make sure we stop processing such an
		 * event here.
		 */
		wpa_printf(MSG_DEBUG, "hostapd_notif_assoc: Skip event with "
			   "no address");
		return -1;
	}
	random_add_randomness(addr, ETH_ALEN);

	hostapd_logger(hapd, addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO, "associated");

	ieee802_11_parse_elems(req_ies, req_ies_len, &elems, 0);
	if (elems.wps_ie) {
		ie = elems.wps_ie - 2;
		ielen = elems.wps_ie_len + 2;
		wpa_printf(MSG_DEBUG, "STA included WPS IE in (Re)AssocReq");
	} else if (elems.rsn_ie) {
		ie = elems.rsn_ie - 2;
		ielen = elems.rsn_ie_len + 2;
		wpa_printf(MSG_DEBUG, "STA included RSN IE in (Re)AssocReq");
	} else if (elems.wpa_ie) {
		ie = elems.wpa_ie - 2;
		ielen = elems.wpa_ie_len + 2;
		wpa_printf(MSG_DEBUG, "STA included WPA IE in (Re)AssocReq");
	} else {
		ie = NULL;
		ielen = 0;
		wpa_printf(MSG_DEBUG, "STA did not include WPS/RSN/WPA IE in "
			   "(Re)AssocReq");
	}

	sta = ap_get_sta(hapd, addr);
	if (sta) {
		accounting_sta_stop(hapd, sta);

		/*
		 * Make sure that the previously registered inactivity timer
		 * will not remove the STA immediately.
		 */
		sta->timeout_next = STA_NULLFUNC;
	} else {
		sta = ap_sta_add(hapd, addr);
		if (sta == NULL) {
			hostapd_drv_sta_disassoc(hapd, addr,
						 WLAN_REASON_DISASSOC_AP_BUSY);
			return -1;
		}
	}
	sta->flags &= ~(WLAN_STA_WPS | WLAN_STA_MAYBE_WPS | WLAN_STA_WPS2);

#ifdef CONFIG_P2P
	if (elems.p2p) {
		wpabuf_free(sta->p2p_ie);
		sta->p2p_ie = ieee802_11_vendor_ie_concat(req_ies, req_ies_len,
							  P2P_IE_VENDOR_TYPE);
	}
#endif /* CONFIG_P2P */

#ifdef CONFIG_HS20
	wpabuf_free(sta->hs20_ie);
	if (elems.hs20 && elems.hs20_len > 4) {
		sta->hs20_ie = wpabuf_alloc_copy(elems.hs20 + 4,
						 elems.hs20_len - 4);
	} else
		sta->hs20_ie = NULL;
#endif /* CONFIG_HS20 */

	if (hapd->conf->wpa) {
		if (ie == NULL || ielen == 0) {
#ifdef CONFIG_WPS
			if (hapd->conf->wps_state) {
				wpa_printf(MSG_DEBUG, "STA did not include "
					   "WPA/RSN IE in (Re)Association "
					   "Request - possible WPS use");
				sta->flags |= WLAN_STA_MAYBE_WPS;
				goto skip_wpa_check;
			}
#endif /* CONFIG_WPS */

			wpa_printf(MSG_DEBUG, "No WPA/RSN IE from STA");
			return -1;
		}
#ifdef CONFIG_WPS
		if (hapd->conf->wps_state && ie[0] == 0xdd && ie[1] >= 4 &&
		    os_memcmp(ie + 2, "\x00\x50\xf2\x04", 4) == 0) {
			struct wpabuf *wps;
			sta->flags |= WLAN_STA_WPS;
			wps = ieee802_11_vendor_ie_concat(ie, ielen,
							  WPS_IE_VENDOR_TYPE);
			if (wps) {
				if (wps_is_20(wps)) {
					wpa_printf(MSG_DEBUG, "WPS: STA "
						   "supports WPS 2.0");
					sta->flags |= WLAN_STA_WPS2;
				}
				wpabuf_free(wps);
			}
			goto skip_wpa_check;
		}
#endif /* CONFIG_WPS */

		if (sta->wpa_sm == NULL)
			sta->wpa_sm = wpa_auth_sta_init(hapd->wpa_auth,
							sta->addr);
		if (sta->wpa_sm == NULL) {
			wpa_printf(MSG_ERROR, "Failed to initialize WPA state "
				   "machine");
			return -1;
		}
		res = wpa_validate_wpa_ie(hapd->wpa_auth, sta->wpa_sm,
					  ie, ielen,
					  elems.mdie, elems.mdie_len);
		if (res != WPA_IE_OK) {
			wpa_printf(MSG_DEBUG, "WPA/RSN information element "
				   "rejected? (res %u)", res);
			wpa_hexdump(MSG_DEBUG, "IE", ie, ielen);
			if (res == WPA_INVALID_GROUP) {
				reason = WLAN_REASON_GROUP_CIPHER_NOT_VALID;
				status = WLAN_STATUS_GROUP_CIPHER_NOT_VALID;
			} else if (res == WPA_INVALID_PAIRWISE) {
				reason = WLAN_REASON_PAIRWISE_CIPHER_NOT_VALID;
				status = WLAN_STATUS_PAIRWISE_CIPHER_NOT_VALID;
			} else if (res == WPA_INVALID_AKMP) {
				reason = WLAN_REASON_AKMP_NOT_VALID;
				status = WLAN_STATUS_AKMP_NOT_VALID;
			}
#ifdef CONFIG_IEEE80211W
			else if (res == WPA_MGMT_FRAME_PROTECTION_VIOLATION) {
				reason = WLAN_REASON_INVALID_IE;
				status = WLAN_STATUS_INVALID_IE;
			} else if (res == WPA_INVALID_MGMT_GROUP_CIPHER) {
				reason = WLAN_REASON_GROUP_CIPHER_NOT_VALID;
				status = WLAN_STATUS_GROUP_CIPHER_NOT_VALID;
			}
#endif /* CONFIG_IEEE80211W */
			else {
				reason = WLAN_REASON_INVALID_IE;
				status = WLAN_STATUS_INVALID_IE;
			}
			goto fail;
		}
#ifdef CONFIG_IEEE80211W
		if ((sta->flags & WLAN_STA_MFP) && !sta->sa_query_timed_out &&
		    sta->sa_query_count > 0)
			ap_check_sa_query_timeout(hapd, sta);
		if ((sta->flags & WLAN_STA_MFP) && !sta->sa_query_timed_out &&
		    (sta->auth_alg != WLAN_AUTH_FT)) {
			/*
			 * STA has already been associated with MFP and SA
			 * Query timeout has not been reached. Reject the
			 * association attempt temporarily and start SA Query,
			 * if one is not pending.
			 */

			if (sta->sa_query_count == 0)
				ap_sta_start_sa_query(hapd, sta);

#ifdef CONFIG_IEEE80211R
			status = WLAN_STATUS_ASSOC_REJECTED_TEMPORARILY;

			p = hostapd_eid_assoc_comeback_time(hapd, sta, p);

			hostapd_sta_assoc(hapd, addr, reassoc, status, buf,
					  p - buf);
#endif /* CONFIG_IEEE80211R */
			return 0;
		}

		if (wpa_auth_uses_mfp(sta->wpa_sm))
			sta->flags |= WLAN_STA_MFP;
		else
			sta->flags &= ~WLAN_STA_MFP;
#endif /* CONFIG_IEEE80211W */

#ifdef CONFIG_IEEE80211R
		if (sta->auth_alg == WLAN_AUTH_FT) {
			status = wpa_ft_validate_reassoc(sta->wpa_sm, req_ies,
							 req_ies_len);
			if (status != WLAN_STATUS_SUCCESS) {
				if (status == WLAN_STATUS_INVALID_PMKID)
					reason = WLAN_REASON_INVALID_IE;
				if (status == WLAN_STATUS_INVALID_MDIE)
					reason = WLAN_REASON_INVALID_IE;
				if (status == WLAN_STATUS_INVALID_FTIE)
					reason = WLAN_REASON_INVALID_IE;
				goto fail;
			}
		}
#endif /* CONFIG_IEEE80211R */
	} else if (hapd->conf->wps_state) {
#ifdef CONFIG_WPS
		struct wpabuf *wps;
		if (req_ies)
			wps = ieee802_11_vendor_ie_concat(req_ies, req_ies_len,
							  WPS_IE_VENDOR_TYPE);
		else
			wps = NULL;
#ifdef CONFIG_WPS_STRICT
		if (wps && wps_validate_assoc_req(wps) < 0) {
			reason = WLAN_REASON_INVALID_IE;
			status = WLAN_STATUS_INVALID_IE;
			wpabuf_free(wps);
			goto fail;
		}
#endif /* CONFIG_WPS_STRICT */
		if (wps) {
			sta->flags |= WLAN_STA_WPS;
			if (wps_is_20(wps)) {
				wpa_printf(MSG_DEBUG, "WPS: STA supports "
					   "WPS 2.0");
				sta->flags |= WLAN_STA_WPS2;
			}
		} else
			sta->flags |= WLAN_STA_MAYBE_WPS;
		wpabuf_free(wps);
#endif /* CONFIG_WPS */
	}
#ifdef CONFIG_WPS
skip_wpa_check:
#endif /* CONFIG_WPS */

#ifdef CONFIG_IEEE80211R
	p = wpa_sm_write_assoc_resp_ies(sta->wpa_sm, buf, sizeof(buf),
					sta->auth_alg, req_ies, req_ies_len);

	hostapd_sta_assoc(hapd, addr, reassoc, status, buf, p - buf);
#else /* CONFIG_IEEE80211R */
	/* Keep compiler silent about unused variables */
	if (status) {
	}
#endif /* CONFIG_IEEE80211R */

	new_assoc = (sta->flags & WLAN_STA_ASSOC) == 0;
	sta->flags |= WLAN_STA_AUTH | WLAN_STA_ASSOC;

	if (reassoc && (sta->auth_alg == WLAN_AUTH_FT))
		wpa_auth_sm_event(sta->wpa_sm, WPA_ASSOC_FT);
	else
		wpa_auth_sm_event(sta->wpa_sm, WPA_ASSOC);

	hostapd_new_assoc_sta(hapd, sta, !new_assoc);

	ieee802_1x_notify_port_enabled(sta->eapol_sm, 1);

#ifdef CONFIG_P2P
	if (req_ies) {
		p2p_group_notif_assoc(hapd->p2p_group, sta->addr,
				      req_ies, req_ies_len);
	}
#endif /* CONFIG_P2P */

	return 0;

fail:
#ifdef CONFIG_IEEE80211R
	hostapd_sta_assoc(hapd, addr, reassoc, status, buf, p - buf);
#endif /* CONFIG_IEEE80211R */
	hostapd_drv_sta_disassoc(hapd, sta->addr, reason);
	ap_free_sta(hapd, sta);
	return -1;
}


void hostapd_notif_disassoc(struct hostapd_data *hapd, const u8 *addr)
{
	struct sta_info *sta;

	if (addr == NULL) {
		/*
		 * This could potentially happen with unexpected event from the
		 * driver wrapper. This was seen at least in one case where the
		 * driver ended up reporting a station mode event while hostapd
		 * was running, so better make sure we stop processing such an
		 * event here.
		 */
		wpa_printf(MSG_DEBUG, "hostapd_notif_disassoc: Skip event "
			   "with no address");
		return;
	}

	hostapd_logger(hapd, addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO, "disassociated");

	sta = ap_get_sta(hapd, addr);
	if (sta == NULL) {
		wpa_printf(MSG_DEBUG, "Disassociation notification for "
			   "unknown STA " MACSTR, MAC2STR(addr));
		return;
	}

	ap_sta_set_authorized(hapd, sta, 0);
	sta->flags &= ~(WLAN_STA_AUTH | WLAN_STA_ASSOC);
	wpa_auth_sm_event(sta->wpa_sm, WPA_DISASSOC);
	sta->acct_terminate_cause = RADIUS_ACCT_TERMINATE_CAUSE_USER_REQUEST;
	ieee802_1x_notify_port_enabled(sta->eapol_sm, 0);
	ap_free_sta(hapd, sta);
}


void hostapd_event_sta_low_ack(struct hostapd_data *hapd, const u8 *addr)
{
	struct sta_info *sta = ap_get_sta(hapd, addr);

	if (!sta || !hapd->conf->disassoc_low_ack)
		return;

	hostapd_logger(hapd, addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO, "disconnected due to excessive "
		       "missing ACKs");
	hostapd_drv_sta_disassoc(hapd, addr, WLAN_REASON_DISASSOC_LOW_ACK);
	if (sta)
		ap_sta_disassociate(hapd, sta, WLAN_REASON_DISASSOC_LOW_ACK);
}


void hostapd_event_ch_switch(struct hostapd_data *hapd, int freq, int ht,
			     int offset)
{
#ifdef NEED_AP_MLME
	int channel;

	hostapd_logger(hapd, NULL, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO, "driver had channel switch: "
		       "freq=%d, ht=%d, offset=%d", freq, ht, offset);

	hapd->iface->freq = freq;

	channel = hostapd_hw_get_channel(hapd, freq);
	if (!channel) {
		hostapd_logger(hapd, NULL, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_WARNING, "driver switched to "
			       "bad channel!");
		return;
	}

	hapd->iconf->channel = channel;
	hapd->iconf->ieee80211n = ht;
	hapd->iconf->secondary_channel = offset;
#endif /* NEED_AP_MLME */
}


int hostapd_probe_req_rx(struct hostapd_data *hapd, const u8 *sa, const u8 *da,
			 const u8 *bssid, const u8 *ie, size_t ie_len,
			 int ssi_signal)
{
	size_t i;
	int ret = 0;

	if (sa == NULL || ie == NULL)
		return -1;

	random_add_randomness(sa, ETH_ALEN);
	for (i = 0; hapd->probereq_cb && i < hapd->num_probereq_cb; i++) {
		if (hapd->probereq_cb[i].cb(hapd->probereq_cb[i].ctx,
					    sa, da, bssid, ie, ie_len,
					    ssi_signal) > 0) {
			ret = 1;
			break;
		}
	}
	return ret;
}


#ifdef HOSTAPD

#ifdef CONFIG_IEEE80211R
static void hostapd_notify_auth_ft_finish(void *ctx, const u8 *dst,
					  const u8 *bssid,
					  u16 auth_transaction, u16 status,
					  const u8 *ies, size_t ies_len)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;

	sta = ap_get_sta(hapd, dst);
	if (sta == NULL)
		return;

	hostapd_logger(hapd, dst, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG, "authentication OK (FT)");
	sta->flags |= WLAN_STA_AUTH;

	hostapd_sta_auth(hapd, dst, auth_transaction, status, ies, ies_len);
}
#endif /* CONFIG_IEEE80211R */


static void hostapd_notif_auth(struct hostapd_data *hapd,
			       struct auth_info *rx_auth)
{
	struct sta_info *sta;
	u16 status = WLAN_STATUS_SUCCESS;
	u8 resp_ies[2 + WLAN_AUTH_CHALLENGE_LEN];
	size_t resp_ies_len = 0;

	sta = ap_get_sta(hapd, rx_auth->peer);
	if (!sta) {
		sta = ap_sta_add(hapd, rx_auth->peer);
		if (sta == NULL) {
			status = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
	}
	sta->flags &= ~WLAN_STA_PREAUTH;
	ieee802_1x_notify_pre_auth(sta->eapol_sm, 0);
#ifdef CONFIG_IEEE80211R
	if (rx_auth->auth_type == WLAN_AUTH_FT && hapd->wpa_auth) {
		sta->auth_alg = WLAN_AUTH_FT;
		if (sta->wpa_sm == NULL)
			sta->wpa_sm = wpa_auth_sta_init(hapd->wpa_auth,
							sta->addr);
		if (sta->wpa_sm == NULL) {
			wpa_printf(MSG_DEBUG, "FT: Failed to initialize WPA "
				   "state machine");
			status = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
		wpa_ft_process_auth(sta->wpa_sm, rx_auth->bssid,
				    rx_auth->auth_transaction, rx_auth->ies,
				    rx_auth->ies_len,
				    hostapd_notify_auth_ft_finish, hapd);
		return;
	}
#endif /* CONFIG_IEEE80211R */
fail:
	hostapd_sta_auth(hapd, rx_auth->peer, rx_auth->auth_transaction + 1,
			 status, resp_ies, resp_ies_len);
}


static void hostapd_action_rx(struct hostapd_data *hapd,
			      struct rx_action *action)
{
	struct sta_info *sta;

        wpa_printf(MSG_DEBUG, "RX_ACTION cat %d action plen %d",
		   action->category, (int) action->len);

	sta = ap_get_sta(hapd, action->sa);
	if (sta == NULL) {
		wpa_printf(MSG_DEBUG, "%s: station not found", __func__);
		return;
	}
#ifdef CONFIG_IEEE80211R
	if (action->category == WLAN_ACTION_FT) {
		wpa_printf(MSG_DEBUG, "%s: FT_ACTION length %d",
			   __func__, (int) action->len);
		wpa_ft_action_rx(sta->wpa_sm, action->data, action->len);
	}
#endif /* CONFIG_IEEE80211R */
#ifdef CONFIG_IEEE80211W
	if (action->category == WLAN_ACTION_SA_QUERY && action->len >= 4) {
		wpa_printf(MSG_DEBUG, "%s: SA_QUERY_ACTION length %d",
			   __func__, (int) action->len);
		ieee802_11_sa_query_action(hapd, action->sa,
					   *(action->data + 1),
					   action->data + 2);
	}
#endif /* CONFIG_IEEE80211W */
#ifdef CONFIG_WNM
	if (action->category == WLAN_ACTION_WNM) {
		wpa_printf(MSG_DEBUG, "%s: WNM_ACTION length %d",
			   __func__, (int) action->len);
		ieee802_11_rx_wnm_action_ap(hapd, action);
	}
#endif /* CONFIG_WNM */
}


#ifdef NEED_AP_MLME

#define HAPD_BROADCAST ((struct hostapd_data *) -1)

static struct hostapd_data * get_hapd_bssid(struct hostapd_iface *iface,
					    const u8 *bssid)
{
	size_t i;

	if (bssid == NULL)
		return NULL;
	if (bssid[0] == 0xff && bssid[1] == 0xff && bssid[2] == 0xff &&
	    bssid[3] == 0xff && bssid[4] == 0xff && bssid[5] == 0xff)
		return HAPD_BROADCAST;

	for (i = 0; i < iface->num_bss; i++) {
		if (os_memcmp(bssid, iface->bss[i]->own_addr, ETH_ALEN) == 0)
			return iface->bss[i];
	}

	return NULL;
}


static void hostapd_rx_from_unknown_sta(struct hostapd_data *hapd,
					const u8 *bssid, const u8 *addr,
					int wds)
{
	hapd = get_hapd_bssid(hapd->iface, bssid);
	if (hapd == NULL || hapd == HAPD_BROADCAST)
		return;

	ieee802_11_rx_from_unknown(hapd, addr, wds);
}


static void hostapd_mgmt_rx(struct hostapd_data *hapd, struct rx_mgmt *rx_mgmt)
{
	struct hostapd_iface *iface = hapd->iface;
	const struct ieee80211_hdr *hdr;
	const u8 *bssid;
	struct hostapd_frame_info fi;

	hdr = (const struct ieee80211_hdr *) rx_mgmt->frame;
	bssid = get_hdr_bssid(hdr, rx_mgmt->frame_len);
	if (bssid == NULL)
		return;

	hapd = get_hapd_bssid(iface, bssid);
	if (hapd == NULL) {
		u16 fc;
		fc = le_to_host16(hdr->frame_control);

		/*
		 * Drop frames to unknown BSSIDs except for Beacon frames which
		 * could be used to update neighbor information.
		 */
		if (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_MGMT &&
		    WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_BEACON)
			hapd = iface->bss[0];
		else
			return;
	}

	os_memset(&fi, 0, sizeof(fi));
	fi.datarate = rx_mgmt->datarate;
	fi.ssi_signal = rx_mgmt->ssi_signal;

	if (hapd == HAPD_BROADCAST) {
		size_t i;
		for (i = 0; i < iface->num_bss; i++)
			ieee802_11_mgmt(iface->bss[i], rx_mgmt->frame,
					rx_mgmt->frame_len, &fi);
	} else
		ieee802_11_mgmt(hapd, rx_mgmt->frame, rx_mgmt->frame_len, &fi);

	random_add_randomness(&fi, sizeof(fi));
}


static void hostapd_rx_action(struct hostapd_data *hapd,
			      struct rx_action *rx_action)
{
	struct rx_mgmt rx_mgmt;
	u8 *buf;
	struct ieee80211_hdr *hdr;

	wpa_printf(MSG_DEBUG, "EVENT_RX_ACTION DA=" MACSTR " SA=" MACSTR
		   " BSSID=" MACSTR " category=%u",
		   MAC2STR(rx_action->da), MAC2STR(rx_action->sa),
		   MAC2STR(rx_action->bssid), rx_action->category);
	wpa_hexdump(MSG_MSGDUMP, "Received action frame contents",
		    rx_action->data, rx_action->len);

	buf = os_zalloc(24 + 1 + rx_action->len);
	if (buf == NULL)
		return;
	hdr = (struct ieee80211_hdr *) buf;
	hdr->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					  WLAN_FC_STYPE_ACTION);
	if (rx_action->category == WLAN_ACTION_SA_QUERY) {
		/*
		 * Assume frame was protected; it would have been dropped if
		 * not.
		 */
		hdr->frame_control |= host_to_le16(WLAN_FC_ISWEP);
	}
	os_memcpy(hdr->addr1, rx_action->da, ETH_ALEN);
	os_memcpy(hdr->addr2, rx_action->sa, ETH_ALEN);
	os_memcpy(hdr->addr3, rx_action->bssid, ETH_ALEN);
	buf[24] = rx_action->category;
	os_memcpy(buf + 24 + 1, rx_action->data, rx_action->len);
	os_memset(&rx_mgmt, 0, sizeof(rx_mgmt));
	rx_mgmt.frame = buf;
	rx_mgmt.frame_len = 24 + 1 + rx_action->len;
	hostapd_mgmt_rx(hapd, &rx_mgmt);
	os_free(buf);
}


static void hostapd_mgmt_tx_cb(struct hostapd_data *hapd, const u8 *buf,
			       size_t len, u16 stype, int ok)
{
	struct ieee80211_hdr *hdr;
	hdr = (struct ieee80211_hdr *) buf;
	hapd = get_hapd_bssid(hapd->iface, get_hdr_bssid(hdr, len));
	if (hapd == NULL || hapd == HAPD_BROADCAST)
		return;
	ieee802_11_mgmt_cb(hapd, buf, len, stype, ok);
}

#endif /* NEED_AP_MLME */


static int hostapd_event_new_sta(struct hostapd_data *hapd, const u8 *addr)
{
	struct sta_info *sta = ap_get_sta(hapd, addr);
	if (sta)
		return 0;

	wpa_printf(MSG_DEBUG, "Data frame from unknown STA " MACSTR
		   " - adding a new STA", MAC2STR(addr));
	sta = ap_sta_add(hapd, addr);
	if (sta) {
		hostapd_new_assoc_sta(hapd, sta, 0);
	} else {
		wpa_printf(MSG_DEBUG, "Failed to add STA entry for " MACSTR,
			   MAC2STR(addr));
		return -1;
	}

	return 0;
}


static void hostapd_event_eapol_rx(struct hostapd_data *hapd, const u8 *src,
				   const u8 *data, size_t data_len)
{
	struct hostapd_iface *iface = hapd->iface;
	struct sta_info *sta;
	size_t j;

	for (j = 0; j < iface->num_bss; j++) {
		if ((sta = ap_get_sta(iface->bss[j], src))) {
			if (sta->flags & WLAN_STA_ASSOC) {
				hapd = iface->bss[j];
				break;
			}
		}
	}

	ieee802_1x_receive(hapd, src, data, data_len);
}


void wpa_supplicant_event(void *ctx, enum wpa_event_type event,
			  union wpa_event_data *data)
{
	struct hostapd_data *hapd = ctx;
#ifndef CONFIG_NO_STDOUT_DEBUG
	int level = MSG_DEBUG;

	if (event == EVENT_RX_MGMT && data->rx_mgmt.frame &&
	    data->rx_mgmt.frame_len >= 24) {
		const struct ieee80211_hdr *hdr;
		u16 fc;
		hdr = (const struct ieee80211_hdr *) data->rx_mgmt.frame;
		fc = le_to_host16(hdr->frame_control);
		if (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_MGMT &&
		    WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_BEACON)
			level = MSG_EXCESSIVE;
	}

	wpa_dbg(hapd->msg_ctx, level, "Event %s (%d) received",
		event_to_string(event), event);
#endif /* CONFIG_NO_STDOUT_DEBUG */

	switch (event) {
	case EVENT_MICHAEL_MIC_FAILURE:
		michael_mic_failure(hapd, data->michael_mic_failure.src, 1);
		break;
	case EVENT_SCAN_RESULTS:
		if (hapd->iface->scan_cb)
			hapd->iface->scan_cb(hapd->iface);
		break;
#ifdef CONFIG_IEEE80211R
	case EVENT_FT_RRB_RX:
		wpa_ft_rrb_rx(hapd->wpa_auth, data->ft_rrb_rx.src,
			      data->ft_rrb_rx.data, data->ft_rrb_rx.data_len);
		break;
#endif /* CONFIG_IEEE80211R */
	case EVENT_WPS_BUTTON_PUSHED:
		hostapd_wps_button_pushed(hapd, NULL);
		break;
#ifdef NEED_AP_MLME
	case EVENT_TX_STATUS:
		switch (data->tx_status.type) {
		case WLAN_FC_TYPE_MGMT:
			hostapd_mgmt_tx_cb(hapd, data->tx_status.data,
					   data->tx_status.data_len,
					   data->tx_status.stype,
					   data->tx_status.ack);
			break;
		case WLAN_FC_TYPE_DATA:
			hostapd_tx_status(hapd, data->tx_status.dst,
					  data->tx_status.data,
					  data->tx_status.data_len,
					  data->tx_status.ack);
			break;
		}
		break;
	case EVENT_EAPOL_TX_STATUS:
		hostapd_eapol_tx_status(hapd, data->eapol_tx_status.dst,
					data->eapol_tx_status.data,
					data->eapol_tx_status.data_len,
					data->eapol_tx_status.ack);
		break;
	case EVENT_DRIVER_CLIENT_POLL_OK:
		hostapd_client_poll_ok(hapd, data->client_poll.addr);
		break;
	case EVENT_RX_FROM_UNKNOWN:
		hostapd_rx_from_unknown_sta(hapd, data->rx_from_unknown.bssid,
					    data->rx_from_unknown.addr,
					    data->rx_from_unknown.wds);
		break;
	case EVENT_RX_MGMT:
		hostapd_mgmt_rx(hapd, &data->rx_mgmt);
		break;
#endif /* NEED_AP_MLME */
	case EVENT_RX_PROBE_REQ:
		if (data->rx_probe_req.sa == NULL ||
		    data->rx_probe_req.ie == NULL)
			break;
		hostapd_probe_req_rx(hapd, data->rx_probe_req.sa,
				     data->rx_probe_req.da,
				     data->rx_probe_req.bssid,
				     data->rx_probe_req.ie,
				     data->rx_probe_req.ie_len,
				     data->rx_probe_req.ssi_signal);
		break;
	case EVENT_NEW_STA:
		hostapd_event_new_sta(hapd, data->new_sta.addr);
		break;
	case EVENT_EAPOL_RX:
		hostapd_event_eapol_rx(hapd, data->eapol_rx.src,
				       data->eapol_rx.data,
				       data->eapol_rx.data_len);
		break;
	case EVENT_ASSOC:
		hostapd_notif_assoc(hapd, data->assoc_info.addr,
				    data->assoc_info.req_ies,
				    data->assoc_info.req_ies_len,
				    data->assoc_info.reassoc);
		break;
	case EVENT_DISASSOC:
		if (data)
			hostapd_notif_disassoc(hapd, data->disassoc_info.addr);
		break;
	case EVENT_DEAUTH:
		if (data)
			hostapd_notif_disassoc(hapd, data->deauth_info.addr);
		break;
	case EVENT_STATION_LOW_ACK:
		if (!data)
			break;
		hostapd_event_sta_low_ack(hapd, data->low_ack.addr);
		break;
	case EVENT_RX_ACTION:
		if (data->rx_action.da == NULL || data->rx_action.sa == NULL ||
		    data->rx_action.bssid == NULL)
			break;
#ifdef NEED_AP_MLME
		hostapd_rx_action(hapd, &data->rx_action);
#endif /* NEED_AP_MLME */
		hostapd_action_rx(hapd, &data->rx_action);
		break;
	case EVENT_AUTH:
		hostapd_notif_auth(hapd, &data->auth);
		break;
	case EVENT_CH_SWITCH:
		if (!data)
			break;
		hostapd_event_ch_switch(hapd, data->ch_switch.freq,
					data->ch_switch.ht_enabled,
					data->ch_switch.ch_offset);
		break;
	default:
		wpa_printf(MSG_DEBUG, "Unknown event %d", event);
		break;
	}
}

#endif /* HOSTAPD */
