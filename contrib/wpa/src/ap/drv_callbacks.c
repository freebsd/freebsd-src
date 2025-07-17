/*
 * hostapd / Callback functions for driver wrappers
 * Copyright (c) 2002-2013, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "radius/radius.h"
#include "drivers/driver.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "common/wpa_ctrl.h"
#include "common/dpp.h"
#include "common/sae.h"
#include "common/hw_features_common.h"
#include "crypto/random.h"
#include "p2p/p2p.h"
#include "wps/wps.h"
#include "fst/fst.h"
#include "wnm_ap.h"
#include "hostapd.h"
#include "ieee802_11.h"
#include "ieee802_11_auth.h"
#include "sta_info.h"
#include "accounting.h"
#include "tkip_countermeasures.h"
#include "ieee802_1x.h"
#include "wpa_auth.h"
#include "wps_hostapd.h"
#include "ap_drv_ops.h"
#include "ap_config.h"
#include "ap_mlme.h"
#include "hw_features.h"
#include "dfs.h"
#include "beacon.h"
#include "mbo_ap.h"
#include "dpp_hostapd.h"
#include "fils_hlp.h"
#include "neighbor_db.h"
#include "nan_usd_ap.h"


#ifdef CONFIG_FILS
void hostapd_notify_assoc_fils_finish(struct hostapd_data *hapd,
				      struct sta_info *sta)
{
	u16 reply_res = WLAN_STATUS_SUCCESS;
	struct ieee802_11_elems elems;
	u8 buf[IEEE80211_MAX_MMPDU_SIZE], *p = buf;
	int new_assoc;
	bool updated;

	wpa_printf(MSG_DEBUG, "%s FILS: Finish association with " MACSTR,
		   __func__, MAC2STR(sta->addr));
	eloop_cancel_timeout(fils_hlp_timeout, hapd, sta);
	if (!sta->fils_pending_assoc_req)
		return;

	if (ieee802_11_parse_elems(sta->fils_pending_assoc_req,
				   sta->fils_pending_assoc_req_len, &elems,
				   0) == ParseFailed ||
	    !elems.fils_session) {
		wpa_printf(MSG_DEBUG, "%s failed to find FILS Session element",
			   __func__);
		return;
	}

	p = hostapd_eid_assoc_fils_session(sta->wpa_sm, p,
					   elems.fils_session,
					   sta->fils_hlp_resp);

	reply_res = hostapd_sta_assoc(hapd, sta->addr,
				      sta->fils_pending_assoc_is_reassoc,
				      WLAN_STATUS_SUCCESS,
				      buf, p - buf);
	updated = ap_sta_set_authorized_flag(hapd, sta, 1);
	new_assoc = (sta->flags & WLAN_STA_ASSOC) == 0;
	sta->flags |= WLAN_STA_AUTH | WLAN_STA_ASSOC;
	sta->flags &= ~WLAN_STA_WNM_SLEEP_MODE;
	hostapd_set_sta_flags(hapd, sta);
	if (updated)
		ap_sta_set_authorized_event(hapd, sta, 1);
	wpa_auth_sm_event(sta->wpa_sm, WPA_ASSOC_FILS);
	ieee802_1x_notify_port_enabled(sta->eapol_sm, 1);
	hostapd_new_assoc_sta(hapd, sta, !new_assoc);
	os_free(sta->fils_pending_assoc_req);
	sta->fils_pending_assoc_req = NULL;
	sta->fils_pending_assoc_req_len = 0;
	wpabuf_free(sta->fils_hlp_resp);
	sta->fils_hlp_resp = NULL;
	wpabuf_free(sta->hlp_dhcp_discover);
	sta->hlp_dhcp_discover = NULL;
	fils_hlp_deinit(hapd);

	/*
	 * Remove the station in case transmission of a success response fails
	 * (the STA was added associated to the driver) or if the station was
	 * previously added unassociated.
	 */
	if (reply_res != WLAN_STATUS_SUCCESS || sta->added_unassoc) {
		hostapd_drv_sta_remove(hapd, sta->addr);
		sta->added_unassoc = 0;
	}
}
#endif /* CONFIG_FILS */


static bool check_sa_query_need(struct hostapd_data *hapd, struct sta_info *sta)
{
	if ((sta->flags &
	     (WLAN_STA_ASSOC | WLAN_STA_MFP | WLAN_STA_AUTHORIZED)) !=
	    (WLAN_STA_ASSOC | WLAN_STA_MFP | WLAN_STA_AUTHORIZED))
		return false;

	if (!sta->sa_query_timed_out && sta->sa_query_count > 0)
		ap_check_sa_query_timeout(hapd, sta);

	if (!sta->sa_query_timed_out && (sta->auth_alg != WLAN_AUTH_FT)) {
		/*
		 * STA has already been associated with MFP and SA Query timeout
		 * has not been reached. Reject the association attempt
		 * temporarily and start SA Query, if one is not pending.
		 */
		if (sta->sa_query_count == 0)
			ap_sta_start_sa_query(hapd, sta);

		return true;
	}

	return false;
}


#ifdef CONFIG_IEEE80211BE
static int hostapd_update_sta_links_status(struct hostapd_data *hapd,
					   struct sta_info *sta,
					   const u8 *resp_ies,
					   size_t resp_ies_len)
{
	struct mld_info *info = &sta->mld_info;
	struct wpabuf *mlebuf;
	const u8 *mle, *pos;
	struct ieee802_11_elems elems;
	size_t mle_len, rem_len;
	int ret = 0;

	if (!resp_ies) {
		wpa_printf(MSG_DEBUG,
			   "MLO: (Re)Association Response frame elements not available");
		return -1;
	}

	if (ieee802_11_parse_elems(resp_ies, resp_ies_len, &elems, 0) ==
	    ParseFailed) {
		wpa_printf(MSG_DEBUG,
			   "MLO: Failed to parse (Re)Association Response frame elements");
		return -1;
	}

	mlebuf = ieee802_11_defrag(elems.basic_mle, elems.basic_mle_len, true);
	if (!mlebuf) {
		wpa_printf(MSG_ERROR,
			   "MLO: Basic Multi-Link element not found in (Re)Association Response frame");
		return -1;
	}

	mle = wpabuf_head(mlebuf);
	mle_len = wpabuf_len(mlebuf);
	if (mle_len < MULTI_LINK_CONTROL_LEN + 1 ||
	    mle_len - MULTI_LINK_CONTROL_LEN < mle[MULTI_LINK_CONTROL_LEN]) {
		wpa_printf(MSG_ERROR,
			   "MLO: Invalid Multi-Link element in (Re)Association Response frame");
		ret = -1;
		goto out;
	}

	/* Skip Common Info */
	pos = mle + MULTI_LINK_CONTROL_LEN + mle[MULTI_LINK_CONTROL_LEN];
	rem_len = mle_len -
		(MULTI_LINK_CONTROL_LEN + mle[MULTI_LINK_CONTROL_LEN]);

	/* Parse Subelements */
	while (rem_len > 2) {
		size_t ie_len = 2 + pos[1];

		if (rem_len < ie_len)
			break;

		if (pos[0] == MULTI_LINK_SUB_ELEM_ID_PER_STA_PROFILE) {
			u8 link_id;
			const u8 *sta_profile;
			size_t sta_profile_len;
			u16 sta_ctrl;

			if (pos[1] < BASIC_MLE_STA_CTRL_LEN + 1) {
				wpa_printf(MSG_DEBUG,
					   "MLO: Invalid per-STA profile IE");
				goto next_subelem;
			}

			sta_profile_len = pos[1];
			sta_profile = &pos[2];
			sta_ctrl = WPA_GET_LE16(sta_profile);
			link_id = sta_ctrl & BASIC_MLE_STA_CTRL_LINK_ID_MASK;
			if (link_id >= MAX_NUM_MLD_LINKS) {
				wpa_printf(MSG_DEBUG,
					   "MLO: Invalid link ID in per-STA profile IE");
				goto next_subelem;
			}

			/* Skip STA Control and STA Info */
			if (sta_profile_len - BASIC_MLE_STA_CTRL_LEN <
			    sta_profile[BASIC_MLE_STA_CTRL_LEN]) {
				wpa_printf(MSG_DEBUG,
					   "MLO: Invalid STA info in per-STA profile IE");
				goto next_subelem;
			}

			sta_profile_len = sta_profile_len -
				(BASIC_MLE_STA_CTRL_LEN +
				 sta_profile[BASIC_MLE_STA_CTRL_LEN]);
			sta_profile = sta_profile + BASIC_MLE_STA_CTRL_LEN +
				sta_profile[BASIC_MLE_STA_CTRL_LEN];

			/* Skip Capabilities Information field */
			if (sta_profile_len < 2)
				goto next_subelem;
			sta_profile_len -= 2;
			sta_profile += 2;

			/* Get status of the link */
			info->links[link_id].status = WPA_GET_LE16(sta_profile);
		}
next_subelem:
		pos += ie_len;
		rem_len -= ie_len;
	}

out:
	wpabuf_free(mlebuf);
	return ret;
}
#endif /* CONFIG_IEEE80211BE */


int hostapd_notif_assoc(struct hostapd_data *hapd, const u8 *addr,
			const u8 *req_ies, size_t req_ies_len,
			const u8 *resp_ies, size_t resp_ies_len,
			const u8 *link_addr, int reassoc)
{
	struct sta_info *sta;
	int new_assoc;
	enum wpa_validate_result res;
	struct ieee802_11_elems elems;
	const u8 *ie;
	size_t ielen;
	u8 buf[sizeof(struct ieee80211_mgmt) + 1024];
	u8 *p = buf;
	u16 reason = WLAN_REASON_UNSPECIFIED;
	int status = WLAN_STATUS_SUCCESS;
	const u8 *p2p_dev_addr = NULL;
#ifdef CONFIG_OWE
	struct hostapd_iface *iface = hapd->iface;
#endif /* CONFIG_OWE */
	bool updated = false;

	if (addr == NULL) {
		/*
		 * This could potentially happen with unexpected event from the
		 * driver wrapper. This was seen at least in one case where the
		 * driver ended up being set to station mode while hostapd was
		 * running, so better make sure we stop processing such an
		 * event here.
		 */
		wpa_printf(MSG_DEBUG,
			   "hostapd_notif_assoc: Skip event with no address");
		return -1;
	}

	if (is_multicast_ether_addr(addr) ||
	    is_zero_ether_addr(addr) ||
	    ether_addr_equal(addr, hapd->own_addr)) {
		/* Do not process any frames with unexpected/invalid SA so that
		 * we do not add any state for unexpected STA addresses or end
		 * up sending out frames to unexpected destination. */
		wpa_printf(MSG_DEBUG, "%s: Invalid SA=" MACSTR
			   " in received indication - ignore this indication silently",
			   __func__, MAC2STR(addr));
		return 0;
	}

	random_add_randomness(addr, ETH_ALEN);

	hostapd_logger(hapd, addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO, "associated");

	if (ieee802_11_parse_elems(req_ies, req_ies_len, &elems, 0) ==
	    ParseFailed) {
		wpa_printf(MSG_DEBUG, "%s: Could not parse elements", __func__);
		return -1;
	}

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
#ifdef CONFIG_HS20
	} else if (elems.osen) {
		ie = elems.osen - 2;
		ielen = elems.osen_len + 2;
		wpa_printf(MSG_DEBUG, "STA included OSEN IE in (Re)AssocReq");
#endif /* CONFIG_HS20 */
	} else {
		ie = NULL;
		ielen = 0;
		wpa_printf(MSG_DEBUG,
			   "STA did not include WPS/RSN/WPA IE in (Re)AssocReq");
	}

	sta = ap_get_sta(hapd, addr);
	if (sta) {
		ap_sta_no_session_timeout(hapd, sta);
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

	if (hapd->conf->wpa && check_sa_query_need(hapd, sta)) {
		status = WLAN_STATUS_ASSOC_REJECTED_TEMPORARILY;
		p = hostapd_eid_assoc_comeback_time(hapd, sta, p);
		hostapd_sta_assoc(hapd, addr, reassoc, status, buf, p - buf);

		return 0;
	}

#ifdef CONFIG_IEEE80211BE
	if (link_addr) {
		struct mld_info *info = &sta->mld_info;
		int i, num_valid_links = 0;
		u8 link_id = hapd->mld_link_id;

		ap_sta_set_mld(sta, true);
		sta->mld_assoc_link_id = link_id;
		os_memcpy(info->common_info.mld_addr, addr, ETH_ALEN);
		info->links[link_id].valid = true;
		os_memcpy(info->links[link_id].peer_addr, link_addr, ETH_ALEN);
		os_memcpy(info->links[link_id].local_addr, hapd->own_addr,
			  ETH_ALEN);

		if (!elems.basic_mle ||
		    hostapd_process_ml_assoc_req(hapd, &elems, sta) !=
		    WLAN_STATUS_SUCCESS) {
			reason = WLAN_REASON_UNSPECIFIED;
			wpa_printf(MSG_DEBUG,
				   "Failed to get STA non-assoc links info");
			goto fail;
		}

		for (i = 0 ; i < MAX_NUM_MLD_LINKS; i++) {
			if (info->links[i].valid)
				num_valid_links++;
		}
		if (num_valid_links > 1 &&
		    hostapd_update_sta_links_status(hapd, sta, resp_ies,
						    resp_ies_len)) {
			wpa_printf(MSG_DEBUG,
				   "Failed to get STA non-assoc links status info");
			reason = WLAN_REASON_UNSPECIFIED;
			goto fail;
		}
	}
#endif /* CONFIG_IEEE80211BE */

	sta->flags &= ~(WLAN_STA_WPS | WLAN_STA_MAYBE_WPS | WLAN_STA_WPS2);

	/*
	 * ACL configurations to the drivers (implementing AP SME and ACL
	 * offload) without hostapd's knowledge, can result in a disconnection
	 * though the driver accepts the connection. Skip the hostapd check for
	 * ACL if the driver supports ACL offload to avoid potentially
	 * conflicting ACL rules.
	 */
	if (hapd->iface->drv_max_acl_mac_addrs == 0 &&
	    hostapd_check_acl(hapd, addr, NULL) != HOSTAPD_ACL_ACCEPT) {
		wpa_printf(MSG_INFO, "STA " MACSTR " not allowed to connect",
			   MAC2STR(addr));
		reason = WLAN_REASON_UNSPECIFIED;
		goto fail;
	}

#ifdef CONFIG_P2P
	if (elems.p2p) {
		wpabuf_free(sta->p2p_ie);
		sta->p2p_ie = ieee802_11_vendor_ie_concat(req_ies, req_ies_len,
							  P2P_IE_VENDOR_TYPE);
		if (sta->p2p_ie)
			p2p_dev_addr = p2p_get_go_dev_addr(sta->p2p_ie);
	}
#endif /* CONFIG_P2P */

#ifdef NEED_AP_MLME
	if (elems.ht_capabilities &&
	    (hapd->iface->conf->ht_capab &
	     HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET)) {
		struct ieee80211_ht_capabilities *ht_cap =
			(struct ieee80211_ht_capabilities *)
			elems.ht_capabilities;

		if (le_to_host16(ht_cap->ht_capabilities_info) &
		    HT_CAP_INFO_40MHZ_INTOLERANT)
			ht40_intolerant_add(hapd->iface, sta);
	}
#endif /* NEED_AP_MLME */

	check_ext_capab(hapd, sta, elems.ext_capab, elems.ext_capab_len);

#ifdef CONFIG_HS20
	wpabuf_free(sta->hs20_ie);
	if (elems.hs20 && elems.hs20_len > 4) {
		sta->hs20_ie = wpabuf_alloc_copy(elems.hs20 + 4,
						 elems.hs20_len - 4);
	} else
		sta->hs20_ie = NULL;

	wpabuf_free(sta->roaming_consortium);
	if (elems.roaming_cons_sel)
		sta->roaming_consortium = wpabuf_alloc_copy(
			elems.roaming_cons_sel + 4,
			elems.roaming_cons_sel_len - 4);
	else
		sta->roaming_consortium = NULL;
#endif /* CONFIG_HS20 */

#ifdef CONFIG_FST
	wpabuf_free(sta->mb_ies);
	if (hapd->iface->fst)
		sta->mb_ies = mb_ies_by_info(&elems.mb_ies);
	else
		sta->mb_ies = NULL;
#endif /* CONFIG_FST */

	mbo_ap_check_sta_assoc(hapd, sta, &elems);

	ap_copy_sta_supp_op_classes(sta, elems.supp_op_classes,
				    elems.supp_op_classes_len);

	if (hapd->conf->wpa) {
		if (ie == NULL || ielen == 0) {
#ifdef CONFIG_WPS
			if (hapd->conf->wps_state) {
				wpa_printf(MSG_DEBUG,
					   "STA did not include WPA/RSN IE in (Re)Association Request - possible WPS use");
				sta->flags |= WLAN_STA_MAYBE_WPS;
				goto skip_wpa_check;
			}
#endif /* CONFIG_WPS */

			wpa_printf(MSG_DEBUG, "No WPA/RSN IE from STA");
			reason = WLAN_REASON_INVALID_IE;
			status = WLAN_STATUS_INVALID_IE;
			goto fail;
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
					wpa_printf(MSG_DEBUG,
						   "WPS: STA supports WPS 2.0");
					sta->flags |= WLAN_STA_WPS2;
				}
				wpabuf_free(wps);
			}
			goto skip_wpa_check;
		}
#endif /* CONFIG_WPS */

		if (sta->wpa_sm == NULL)
			sta->wpa_sm = wpa_auth_sta_init(hapd->wpa_auth,
							sta->addr,
							p2p_dev_addr);
		if (sta->wpa_sm == NULL) {
			wpa_printf(MSG_ERROR,
				   "Failed to initialize WPA state machine");
			return -1;
		}
#ifdef CONFIG_IEEE80211BE
		if (ap_sta_is_mld(hapd, sta)) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Set ML info in RSN Authenticator");
			wpa_auth_set_ml_info(sta->wpa_sm,
					     sta->mld_assoc_link_id,
					     &sta->mld_info);
		}
#endif /* CONFIG_IEEE80211BE */
		res = wpa_validate_wpa_ie(hapd->wpa_auth, sta->wpa_sm,
					  hapd->iface->freq,
					  ie, ielen,
					  elems.rsnxe ? elems.rsnxe - 2 : NULL,
					  elems.rsnxe ? elems.rsnxe_len + 2 : 0,
					  elems.mdie, elems.mdie_len,
					  elems.owe_dh, elems.owe_dh_len, NULL);
		reason = WLAN_REASON_INVALID_IE;
		status = WLAN_STATUS_INVALID_IE;
		switch (res) {
		case WPA_IE_OK:
			reason = WLAN_REASON_UNSPECIFIED;
			status = WLAN_STATUS_SUCCESS;
			break;
		case WPA_INVALID_IE:
			reason = WLAN_REASON_INVALID_IE;
			status = WLAN_STATUS_INVALID_IE;
			break;
		case WPA_INVALID_GROUP:
			reason = WLAN_REASON_GROUP_CIPHER_NOT_VALID;
			status = WLAN_STATUS_GROUP_CIPHER_NOT_VALID;
			break;
		case WPA_INVALID_PAIRWISE:
			reason = WLAN_REASON_PAIRWISE_CIPHER_NOT_VALID;
			status = WLAN_STATUS_PAIRWISE_CIPHER_NOT_VALID;
			break;
		case WPA_INVALID_AKMP:
			reason = WLAN_REASON_AKMP_NOT_VALID;
			status = WLAN_STATUS_AKMP_NOT_VALID;
			break;
		case WPA_NOT_ENABLED:
			reason = WLAN_REASON_INVALID_IE;
			status = WLAN_STATUS_INVALID_IE;
			break;
		case WPA_ALLOC_FAIL:
			reason = WLAN_REASON_UNSPECIFIED;
			status = WLAN_STATUS_UNSPECIFIED_FAILURE;
			break;
		case WPA_MGMT_FRAME_PROTECTION_VIOLATION:
			reason = WLAN_REASON_INVALID_IE;
			status = WLAN_STATUS_INVALID_IE;
			break;
		case WPA_INVALID_MGMT_GROUP_CIPHER:
			reason = WLAN_REASON_CIPHER_SUITE_REJECTED;
			status = WLAN_STATUS_CIPHER_REJECTED_PER_POLICY;
			break;
		case WPA_INVALID_MDIE:
			reason = WLAN_REASON_INVALID_MDE;
			status = WLAN_STATUS_INVALID_MDIE;
			break;
		case WPA_INVALID_PROTO:
			reason = WLAN_REASON_INVALID_IE;
			status = WLAN_STATUS_INVALID_IE;
			break;
		case WPA_INVALID_PMKID:
			reason = WLAN_REASON_INVALID_PMKID;
			status = WLAN_STATUS_INVALID_PMKID;
			break;
		case WPA_DENIED_OTHER_REASON:
			reason = WLAN_REASON_UNSPECIFIED;
			status = WLAN_STATUS_ASSOC_DENIED_UNSPEC;
			break;
		}
		if (status != WLAN_STATUS_SUCCESS) {
			wpa_printf(MSG_DEBUG,
				   "WPA/RSN information element rejected? (res %u)",
				   res);
			wpa_hexdump(MSG_DEBUG, "IE", ie, ielen);
			goto fail;
		}

		if (wpa_auth_uses_mfp(sta->wpa_sm))
			sta->flags |= WLAN_STA_MFP;
		else
			sta->flags &= ~WLAN_STA_MFP;

#ifdef CONFIG_IEEE80211R_AP
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
#endif /* CONFIG_IEEE80211R_AP */
#ifdef CONFIG_SAE
		if (hapd->conf->sae_pwe == SAE_PWE_BOTH &&
		    sta->auth_alg == WLAN_AUTH_SAE &&
		    sta->sae && !sta->sae->h2e &&
		    ieee802_11_rsnx_capab_len(elems.rsnxe, elems.rsnxe_len,
					      WLAN_RSNX_CAPAB_SAE_H2E)) {
			wpa_printf(MSG_INFO, "SAE: " MACSTR
				   " indicates support for SAE H2E, but did not use it",
				   MAC2STR(sta->addr));
			status = WLAN_STATUS_UNSPECIFIED_FAILURE;
			reason = WLAN_REASON_UNSPECIFIED;
			goto fail;
		}
#endif /* CONFIG_SAE */
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
				wpa_printf(MSG_DEBUG,
					   "WPS: STA supports WPS 2.0");
				sta->flags |= WLAN_STA_WPS2;
			}
		} else
			sta->flags |= WLAN_STA_MAYBE_WPS;
		wpabuf_free(wps);
#endif /* CONFIG_WPS */
#ifdef CONFIG_HS20
	} else if (hapd->conf->osen) {
		if (elems.osen == NULL) {
			hostapd_logger(
				hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
				HOSTAPD_LEVEL_INFO,
				"No HS 2.0 OSEN element in association request");
			return WLAN_STATUS_INVALID_IE;
		}

		wpa_printf(MSG_DEBUG, "HS 2.0: OSEN association");
		if (sta->wpa_sm == NULL)
			sta->wpa_sm = wpa_auth_sta_init(hapd->wpa_auth,
							sta->addr, NULL);
		if (sta->wpa_sm == NULL) {
			wpa_printf(MSG_WARNING,
				   "Failed to initialize WPA state machine");
			return WLAN_STATUS_UNSPECIFIED_FAILURE;
		}
		if (wpa_validate_osen(hapd->wpa_auth, sta->wpa_sm,
				      elems.osen - 2, elems.osen_len + 2) < 0)
			return WLAN_STATUS_INVALID_IE;
#endif /* CONFIG_HS20 */
	}
#ifdef CONFIG_WPS
skip_wpa_check:
#endif /* CONFIG_WPS */

#ifdef CONFIG_MBO
	if (hapd->conf->mbo_enabled && (hapd->conf->wpa & 2) &&
	    elems.mbo && sta->cell_capa && !(sta->flags & WLAN_STA_MFP) &&
	    hapd->conf->ieee80211w != NO_MGMT_FRAME_PROTECTION) {
		wpa_printf(MSG_INFO,
			   "MBO: Reject WPA2 association without PMF");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}
#endif /* CONFIG_MBO */

#ifdef CONFIG_IEEE80211R_AP
	p = wpa_sm_write_assoc_resp_ies(sta->wpa_sm, buf, sizeof(buf),
					sta->auth_alg, req_ies, req_ies_len,
					!elems.rsnxe);
	if (!p) {
		wpa_printf(MSG_DEBUG, "FT: Failed to write AssocResp IEs");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}
#endif /* CONFIG_IEEE80211R_AP */

#ifdef CONFIG_FILS
	if (sta->auth_alg == WLAN_AUTH_FILS_SK ||
	    sta->auth_alg == WLAN_AUTH_FILS_SK_PFS ||
	    sta->auth_alg == WLAN_AUTH_FILS_PK) {
		int delay_assoc = 0;

		if (!req_ies)
			return WLAN_STATUS_UNSPECIFIED_FAILURE;

		if (!wpa_fils_validate_fils_session(sta->wpa_sm, req_ies,
						    req_ies_len,
						    sta->fils_session)) {
			wpa_printf(MSG_DEBUG,
				   "FILS: Session validation failed");
			return WLAN_STATUS_UNSPECIFIED_FAILURE;
		}

		res = wpa_fils_validate_key_confirm(sta->wpa_sm, req_ies,
						    req_ies_len);
		if (res < 0) {
			wpa_printf(MSG_DEBUG,
				   "FILS: Key Confirm validation failed");
			return WLAN_STATUS_UNSPECIFIED_FAILURE;
		}

		if (fils_process_hlp(hapd, sta, req_ies, req_ies_len) > 0) {
			wpa_printf(MSG_DEBUG,
				   "FILS: Delaying Assoc Response (HLP)");
			delay_assoc = 1;
		} else {
			wpa_printf(MSG_DEBUG,
				   "FILS: Going ahead with Assoc Response (no HLP)");
		}

		if (sta) {
			wpa_printf(MSG_DEBUG, "FILS: HLP callback cleanup");
			eloop_cancel_timeout(fils_hlp_timeout, hapd, sta);
			os_free(sta->fils_pending_assoc_req);
			sta->fils_pending_assoc_req = NULL;
			sta->fils_pending_assoc_req_len = 0;
			wpabuf_free(sta->fils_hlp_resp);
			sta->fils_hlp_resp = NULL;
			sta->fils_drv_assoc_finish = 0;
		}

		if (sta && delay_assoc && status == WLAN_STATUS_SUCCESS) {
			u8 *req_tmp;

			req_tmp = os_malloc(req_ies_len);
			if (!req_tmp) {
				wpa_printf(MSG_DEBUG,
					   "FILS: buffer allocation failed for assoc req");
				goto fail;
			}
			os_memcpy(req_tmp, req_ies, req_ies_len);
			sta->fils_pending_assoc_req = req_tmp;
			sta->fils_pending_assoc_req_len = req_ies_len;
			sta->fils_pending_assoc_is_reassoc = reassoc;
			sta->fils_drv_assoc_finish = 1;
			wpa_printf(MSG_DEBUG,
				   "FILS: Waiting for HLP processing before sending (Re)Association Response frame to "
				   MACSTR, MAC2STR(sta->addr));
			eloop_register_timeout(
				0, hapd->conf->fils_hlp_wait_time * 1024,
				fils_hlp_timeout, hapd, sta);
			return 0;
		}
		p = hostapd_eid_assoc_fils_session(sta->wpa_sm, p,
						   elems.fils_session,
						   sta->fils_hlp_resp);
		wpa_hexdump(MSG_DEBUG, "FILS Assoc Resp BUF (IEs)",
			    buf, p - buf);
	}
#endif /* CONFIG_FILS */

#ifdef CONFIG_OWE
	if ((hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_OWE) &&
	    !(iface->drv_flags2 & WPA_DRIVER_FLAGS2_OWE_OFFLOAD_AP) &&
	    wpa_auth_sta_key_mgmt(sta->wpa_sm) == WPA_KEY_MGMT_OWE &&
	    elems.owe_dh) {
		u8 *npos;
		u16 ret_status;

		npos = owe_assoc_req_process(hapd, sta,
					     elems.owe_dh, elems.owe_dh_len,
					     p, sizeof(buf) - (p - buf),
					     &ret_status);
		status = ret_status;
		if (npos)
			p = npos;

		if (!npos &&
		    status == WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED) {
			hostapd_sta_assoc(hapd, addr, reassoc, ret_status, buf,
					  p - buf);
			return 0;
		}

		if (!npos || status != WLAN_STATUS_SUCCESS)
			goto fail;
	}
#endif /* CONFIG_OWE */

#ifdef CONFIG_DPP2
		dpp_pfs_free(sta->dpp_pfs);
		sta->dpp_pfs = NULL;

		if ((hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_DPP) &&
		    hapd->conf->dpp_netaccesskey && sta->wpa_sm &&
		    wpa_auth_sta_key_mgmt(sta->wpa_sm) == WPA_KEY_MGMT_DPP &&
		    elems.owe_dh) {
			sta->dpp_pfs = dpp_pfs_init(
				wpabuf_head(hapd->conf->dpp_netaccesskey),
				wpabuf_len(hapd->conf->dpp_netaccesskey));
			if (!sta->dpp_pfs) {
				wpa_printf(MSG_DEBUG,
					   "DPP: Could not initialize PFS");
				/* Try to continue without PFS */
				goto pfs_fail;
			}

			if (dpp_pfs_process(sta->dpp_pfs, elems.owe_dh,
					    elems.owe_dh_len) < 0) {
				dpp_pfs_free(sta->dpp_pfs);
				sta->dpp_pfs = NULL;
				reason = WLAN_REASON_UNSPECIFIED;
				goto fail;
			}
		}

		wpa_auth_set_dpp_z(sta->wpa_sm, sta->dpp_pfs ?
				   sta->dpp_pfs->secret : NULL);
	pfs_fail:
#endif /* CONFIG_DPP2 */

	if (elems.rrm_enabled &&
	    elems.rrm_enabled_len >= sizeof(sta->rrm_enabled_capa))
	    os_memcpy(sta->rrm_enabled_capa, elems.rrm_enabled,
		      sizeof(sta->rrm_enabled_capa));

#if defined(CONFIG_IEEE80211R_AP) || defined(CONFIG_FILS) || defined(CONFIG_OWE)
	hostapd_sta_assoc(hapd, addr, reassoc, status, buf, p - buf);

	if (sta->auth_alg == WLAN_AUTH_FT ||
	    sta->auth_alg == WLAN_AUTH_FILS_SK ||
	    sta->auth_alg == WLAN_AUTH_FILS_SK_PFS ||
	    sta->auth_alg == WLAN_AUTH_FILS_PK)
		updated = ap_sta_set_authorized_flag(hapd, sta, 1);
#else /* CONFIG_IEEE80211R_AP || CONFIG_FILS */
	/* Keep compiler silent about unused variables */
	if (status) {
	}
#endif /* CONFIG_IEEE80211R_AP || CONFIG_FILS */

#ifdef CONFIG_IEEE80211BE
	if (hostapd_process_assoc_ml_info(hapd, sta, req_ies, req_ies_len,
					  !!reassoc, WLAN_STATUS_SUCCESS,
					  true)) {
		status = WLAN_STATUS_UNSPECIFIED_FAILURE;
		reason = WLAN_REASON_UNSPECIFIED;
		goto fail;
	}
#endif /* CONFIG_IEEE80211BE */

	new_assoc = (sta->flags & WLAN_STA_ASSOC) == 0;
	sta->flags |= WLAN_STA_AUTH | WLAN_STA_ASSOC;
	sta->flags &= ~WLAN_STA_WNM_SLEEP_MODE;

	hostapd_set_sta_flags(hapd, sta);
	if (updated)
		ap_sta_set_authorized_event(hapd, sta, 1);

	if (reassoc && (sta->auth_alg == WLAN_AUTH_FT))
		wpa_auth_sm_event(sta->wpa_sm, WPA_ASSOC_FT);
#ifdef CONFIG_FILS
	else if (sta->auth_alg == WLAN_AUTH_FILS_SK ||
		 sta->auth_alg == WLAN_AUTH_FILS_SK_PFS ||
		 sta->auth_alg == WLAN_AUTH_FILS_PK)
		wpa_auth_sm_event(sta->wpa_sm, WPA_ASSOC_FILS);
#endif /* CONFIG_FILS */
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
#ifdef CONFIG_IEEE80211R_AP
	if (status >= 0)
		hostapd_sta_assoc(hapd, addr, reassoc, status, buf, p - buf);
#endif /* CONFIG_IEEE80211R_AP */
	hostapd_drv_sta_disassoc(hapd, sta->addr, reason);
	ap_free_sta(hapd, sta);
	return -1;
}


static void hostapd_remove_sta(struct hostapd_data *hapd, struct sta_info *sta)
{
	ap_sta_set_authorized(hapd, sta, 0);
	sta->flags &= ~(WLAN_STA_AUTH | WLAN_STA_ASSOC);
	hostapd_set_sta_flags(hapd, sta);
	wpa_auth_sm_event(sta->wpa_sm, WPA_DISASSOC);
	sta->acct_terminate_cause = RADIUS_ACCT_TERMINATE_CAUSE_USER_REQUEST;
	ieee802_1x_notify_port_enabled(sta->eapol_sm, 0);
	ap_free_sta(hapd, sta);
}


#ifdef CONFIG_IEEE80211BE
static void hostapd_notif_disassoc_mld(struct hostapd_data *assoc_hapd,
				       struct sta_info *sta,
				       const u8 *addr)
{
	unsigned int link_id, i;
	struct hostapd_data *tmp_hapd;
	struct hapd_interfaces *interfaces = assoc_hapd->iface->interfaces;

	/* Remove STA entry in non-assoc links */
	for (link_id = 0; link_id < MAX_NUM_MLD_LINKS; link_id++) {
		if (!sta->mld_info.links[link_id].valid)
			continue;

		for (i = 0; i < interfaces->count; i++) {
			struct sta_info *tmp_sta;

			tmp_hapd = interfaces->iface[i]->bss[0];

			if (!tmp_hapd->conf->mld_ap ||
			    assoc_hapd == tmp_hapd ||
			    assoc_hapd->conf->mld_id != tmp_hapd->conf->mld_id)
				continue;

			tmp_sta = ap_get_sta(tmp_hapd, addr);
			if (tmp_sta)
				ap_free_sta(tmp_hapd, tmp_sta);
		}
	}

	/* Remove STA in assoc link */
	hostapd_remove_sta(assoc_hapd, sta);
}
#endif /* CONFIG_IEEE80211BE */


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
		wpa_printf(MSG_DEBUG,
			   "hostapd_notif_disassoc: Skip event with no address");
		return;
	}

	hostapd_logger(hapd, addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO, "disassociated");

	sta = ap_get_sta(hapd, addr);
#ifdef CONFIG_IEEE80211BE
	if (hostapd_is_mld_ap(hapd)) {
		struct hostapd_data *assoc_hapd;
		unsigned int i;

		if (!sta) {
			/* Find non-MLO cases from any of the affiliated AP
			 * links. */
			for (i = 0; i < hapd->iface->interfaces->count; ++i) {
				struct hostapd_iface *h =
					hapd->iface->interfaces->iface[i];
				struct hostapd_data *h_hapd = h->bss[0];
				struct hostapd_bss_config *hconf = h_hapd->conf;

				if (!hconf->mld_ap ||
				    hconf->mld_id != hapd->conf->mld_id)
					continue;

				sta = ap_get_sta(h_hapd, addr);
				if (sta) {
					if (!sta->mld_info.mld_sta) {
						hapd = h_hapd;
						goto legacy;
					}
					break;
				}
			}
		} else if (!sta->mld_info.mld_sta) {
			goto legacy;
		}
		if (!sta) {
			wpa_printf(MSG_DEBUG,
			   "Disassociation notification for unknown STA "
			   MACSTR, MAC2STR(addr));
			return;
		}
		sta = hostapd_ml_get_assoc_sta(hapd, sta, &assoc_hapd);
		if (sta)
			hostapd_notif_disassoc_mld(assoc_hapd, sta, addr);
		return;
	}

legacy:
#endif /* CONFIG_IEEE80211BE */
	if (sta == NULL) {
		wpa_printf(MSG_DEBUG,
			   "Disassociation notification for unknown STA "
			   MACSTR, MAC2STR(addr));
		return;
	}

	hostapd_remove_sta(hapd, sta);
}


void hostapd_event_sta_low_ack(struct hostapd_data *hapd, const u8 *addr)
{
	struct sta_info *sta = ap_get_sta(hapd, addr);

	if (!sta || !hapd->conf->disassoc_low_ack || sta->agreed_to_steer)
		return;

	hostapd_logger(hapd, addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO,
		       "disconnected due to excessive missing ACKs");
	hostapd_drv_sta_disassoc(hapd, addr, WLAN_REASON_DISASSOC_LOW_ACK);
	ap_sta_disassociate(hapd, sta, WLAN_REASON_DISASSOC_LOW_ACK);
}


void hostapd_event_sta_opmode_changed(struct hostapd_data *hapd, const u8 *addr,
				      enum smps_mode smps_mode,
				      enum chan_width chan_width, u8 rx_nss)
{
	struct sta_info *sta = ap_get_sta(hapd, addr);
	const char *txt;

	if (!sta)
		return;

	switch (smps_mode) {
	case SMPS_AUTOMATIC:
		txt = "automatic";
		break;
	case SMPS_OFF:
		txt = "off";
		break;
	case SMPS_DYNAMIC:
		txt = "dynamic";
		break;
	case SMPS_STATIC:
		txt = "static";
		break;
	default:
		txt = NULL;
		break;
	}
	if (txt) {
		wpa_msg(hapd->msg_ctx, MSG_INFO, STA_OPMODE_SMPS_MODE_CHANGED
			MACSTR " %s", MAC2STR(addr), txt);
	}

	switch (chan_width) {
	case CHAN_WIDTH_20_NOHT:
		txt = "20(no-HT)";
		break;
	case CHAN_WIDTH_20:
		txt = "20";
		break;
	case CHAN_WIDTH_40:
		txt = "40";
		break;
	case CHAN_WIDTH_80:
		txt = "80";
		break;
	case CHAN_WIDTH_80P80:
		txt = "80+80";
		break;
	case CHAN_WIDTH_160:
		txt = "160";
		break;
	case CHAN_WIDTH_320:
		txt = "320";
		break;
	default:
		txt = NULL;
		break;
	}
	if (txt) {
		wpa_msg(hapd->msg_ctx, MSG_INFO, STA_OPMODE_MAX_BW_CHANGED
			MACSTR " %s", MAC2STR(addr), txt);
	}

	if (rx_nss != 0xff) {
		wpa_msg(hapd->msg_ctx, MSG_INFO, STA_OPMODE_N_SS_CHANGED
			MACSTR " %d", MAC2STR(addr), rx_nss);
	}
}


void hostapd_event_ch_switch(struct hostapd_data *hapd, int freq, int ht,
			     int offset, int width, int cf1, int cf2,
			     u16 punct_bitmap, int finished)
{
#ifdef NEED_AP_MLME
	int channel, chwidth, is_dfs0, is_dfs;
	u8 seg0_idx = 0, seg1_idx = 0, op_class, chan_no;
	size_t i;

	hostapd_logger(hapd, NULL, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO,
		       "driver %s channel switch: iface->freq=%d, freq=%d, ht=%d, vht_ch=0x%x, he_ch=0x%x, eht_ch=0x%x, offset=%d, width=%d (%s), cf1=%d, cf2=%d, puncturing_bitmap=0x%x",
		       finished ? "had" : "starting",
		       hapd->iface->freq,
		       freq, ht, hapd->iconf->ch_switch_vht_config,
		       hapd->iconf->ch_switch_he_config,
		       hapd->iconf->ch_switch_eht_config, offset,
		       width, channel_width_to_string(width), cf1, cf2,
		       punct_bitmap);

	if (!hapd->iface->current_mode) {
		hostapd_logger(hapd, NULL, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_WARNING,
			       "ignore channel switch since the interface is not yet ready");
		return;
	}

	/* Check if any of configured channels require DFS */
	is_dfs0 = hostapd_is_dfs_required(hapd->iface);
	hapd->iface->freq = freq;

	channel = hostapd_hw_get_channel(hapd, freq);
	if (!channel) {
		hostapd_logger(hapd, NULL, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_WARNING,
			       "driver switched to bad channel!");
		return;
	}

	switch (width) {
	case CHAN_WIDTH_80:
		chwidth = CONF_OPER_CHWIDTH_80MHZ;
		break;
	case CHAN_WIDTH_80P80:
		chwidth = CONF_OPER_CHWIDTH_80P80MHZ;
		break;
	case CHAN_WIDTH_160:
		chwidth = CONF_OPER_CHWIDTH_160MHZ;
		break;
	case CHAN_WIDTH_320:
		chwidth = CONF_OPER_CHWIDTH_320MHZ;
		break;
	case CHAN_WIDTH_20_NOHT:
	case CHAN_WIDTH_20:
	case CHAN_WIDTH_40:
	default:
		chwidth = CONF_OPER_CHWIDTH_USE_HT;
		break;
	}

	/* The operating channel changed when CSA finished, so need to update
	 * hw_mode for all following operations to cover the cases where the
	 * driver changed the operating band. */
	if (finished && hostapd_csa_update_hwmode(hapd->iface))
		return;

	switch (hapd->iface->current_mode->mode) {
	case HOSTAPD_MODE_IEEE80211A:
		if (cf1 == 5935)
			seg0_idx = (cf1 - 5925) / 5;
		else if (cf1 > 5950)
			seg0_idx = (cf1 - 5950) / 5;
		else if (cf1 > 5000)
			seg0_idx = (cf1 - 5000) / 5;

		if (cf2 == 5935)
			seg1_idx = (cf2 - 5925) / 5;
		else if (cf2 > 5950)
			seg1_idx = (cf2 - 5950) / 5;
		else if (cf2 > 5000)
			seg1_idx = (cf2 - 5000) / 5;
		break;
	default:
		ieee80211_freq_to_chan(cf1, &seg0_idx);
		ieee80211_freq_to_chan(cf2, &seg1_idx);
		break;
	}

	hapd->iconf->channel = channel;
	hapd->iconf->ieee80211n = ht;
	if (!ht)
		hapd->iconf->ieee80211ac = 0;
	if (hapd->iconf->ch_switch_vht_config) {
		/* CHAN_SWITCH VHT config */
		if (hapd->iconf->ch_switch_vht_config &
		    CH_SWITCH_VHT_ENABLED)
			hapd->iconf->ieee80211ac = 1;
		else if (hapd->iconf->ch_switch_vht_config &
			 CH_SWITCH_VHT_DISABLED)
			hapd->iconf->ieee80211ac = 0;
	}
	if (hapd->iconf->ch_switch_he_config) {
		/* CHAN_SWITCH HE config */
		if (hapd->iconf->ch_switch_he_config &
		    CH_SWITCH_HE_ENABLED) {
			hapd->iconf->ieee80211ax = 1;
			if (hapd->iface->freq > 4000 &&
			    hapd->iface->freq < 5895)
				hapd->iconf->ieee80211ac = 1;
		}
		else if (hapd->iconf->ch_switch_he_config &
			 CH_SWITCH_HE_DISABLED)
			hapd->iconf->ieee80211ax = 0;
	}
#ifdef CONFIG_IEEE80211BE
	if (hapd->iconf->ch_switch_eht_config) {
		/* CHAN_SWITCH EHT config */
		if (hapd->iconf->ch_switch_eht_config &
		    CH_SWITCH_EHT_ENABLED) {
			hapd->iconf->ieee80211be = 1;
			hapd->iconf->ieee80211ax = 1;
			if (!is_6ghz_freq(hapd->iface->freq) &&
			    hapd->iface->freq > 4000)
				hapd->iconf->ieee80211ac = 1;
		} else if (hapd->iconf->ch_switch_eht_config &
			   CH_SWITCH_EHT_DISABLED)
			hapd->iconf->ieee80211be = 0;
	}
#endif /* CONFIG_IEEE80211BE */
	hapd->iconf->ch_switch_vht_config = 0;
	hapd->iconf->ch_switch_he_config = 0;
	hapd->iconf->ch_switch_eht_config = 0;

	if (width == CHAN_WIDTH_40 || width == CHAN_WIDTH_80 ||
	    width == CHAN_WIDTH_80P80 || width == CHAN_WIDTH_160 ||
	    width == CHAN_WIDTH_320)
		hapd->iconf->ht_capab |= HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET;
	else if (width == CHAN_WIDTH_20 || width == CHAN_WIDTH_20_NOHT)
		hapd->iconf->ht_capab &= ~HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET;

	hapd->iconf->secondary_channel = offset;
	if (ieee80211_freq_to_channel_ext(freq, offset, chwidth,
					  &op_class, &chan_no) !=
	    NUM_HOSTAPD_MODES)
		hapd->iconf->op_class = op_class;
	hostapd_set_oper_chwidth(hapd->iconf, chwidth);
	hostapd_set_oper_centr_freq_seg0_idx(hapd->iconf, seg0_idx);
	hostapd_set_oper_centr_freq_seg1_idx(hapd->iconf, seg1_idx);
	/* Auto-detect new bw320_offset */
	hostapd_set_and_check_bw320_offset(hapd->iconf, 0);
#ifdef CONFIG_IEEE80211BE
	hapd->iconf->punct_bitmap = punct_bitmap;
#endif /* CONFIG_IEEE80211BE */
	if (hapd->iconf->ieee80211ac) {
		hapd->iconf->vht_capab &= ~VHT_CAP_SUPP_CHAN_WIDTH_MASK;
		if (chwidth == CONF_OPER_CHWIDTH_160MHZ)
			hapd->iconf->vht_capab |=
				VHT_CAP_SUPP_CHAN_WIDTH_160MHZ;
		else if (chwidth == CONF_OPER_CHWIDTH_80P80MHZ)
			hapd->iconf->vht_capab |=
				VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ;
	}

	is_dfs = ieee80211_is_dfs(freq, hapd->iface->hw_features,
				  hapd->iface->num_hw_features);

	wpa_msg(hapd->msg_ctx, MSG_INFO,
		"%sfreq=%d ht_enabled=%d ch_offset=%d ch_width=%s cf1=%d cf2=%d is_dfs0=%d dfs=%d puncturing_bitmap=0x%04x",
		finished ? WPA_EVENT_CHANNEL_SWITCH :
		WPA_EVENT_CHANNEL_SWITCH_STARTED,
		freq, ht, offset, channel_width_to_string(width),
		cf1, cf2, is_dfs0, is_dfs, punct_bitmap);
	if (!finished)
		return;

	if (hapd->csa_in_progress &&
	    freq == hapd->cs_freq_params.freq) {
		hostapd_cleanup_cs_params(hapd);
		ieee802_11_set_beacon(hapd);

		wpa_msg(hapd->msg_ctx, MSG_INFO, AP_CSA_FINISHED
			"freq=%d dfs=%d", freq, is_dfs);
	} else if (hapd->iface->drv_flags & WPA_DRIVER_FLAGS_DFS_OFFLOAD) {
		/* Complete AP configuration for the first bring up. */
		if (is_dfs0 > 0 &&
		    hostapd_is_dfs_required(hapd->iface) <= 0 &&
		    hapd->iface->state != HAPD_IFACE_ENABLED) {
			/* Fake a CAC start bit to skip setting channel */
			hapd->iface->cac_started = 1;
			hostapd_setup_interface_complete(hapd->iface, 0);
		}
		wpa_msg(hapd->msg_ctx, MSG_INFO, AP_CSA_FINISHED
			"freq=%d dfs=%d", freq, is_dfs);
	} else if (is_dfs &&
		   hostapd_is_dfs_required(hapd->iface) &&
		   !hostapd_is_dfs_chan_available(hapd->iface) &&
		   !hapd->iface->cac_started) {
		hostapd_disable_iface(hapd->iface);
		hostapd_enable_iface(hapd->iface);
	}

	for (i = 0; i < hapd->iface->num_bss; i++)
		hostapd_neighbor_set_own_report(hapd->iface->bss[i]);

#ifdef CONFIG_OCV
	if (hapd->conf->ocv &&
	    !(hapd->iface->drv_flags2 &
	      WPA_DRIVER_FLAGS2_SA_QUERY_OFFLOAD_AP)) {
		struct sta_info *sta;
		bool check_sa_query = false;

		for (sta = hapd->sta_list; sta; sta = sta->next) {
			if (wpa_auth_uses_ocv(sta->wpa_sm) &&
			    !(sta->flags & WLAN_STA_WNM_SLEEP_MODE)) {
				sta->post_csa_sa_query = 1;
				check_sa_query = true;
			}
		}

		if (check_sa_query) {
			wpa_printf(MSG_DEBUG,
				   "OCV: Check post-CSA SA Query initiation in 15 seconds");
			eloop_register_timeout(15, 0,
					       hostapd_ocv_check_csa_sa_query,
					       hapd, NULL);
		}
	}
#endif /* CONFIG_OCV */
#endif /* NEED_AP_MLME */
}


void hostapd_event_connect_failed_reason(struct hostapd_data *hapd,
					 const u8 *addr, int reason_code)
{
	switch (reason_code) {
	case MAX_CLIENT_REACHED:
		wpa_msg(hapd->msg_ctx, MSG_INFO, AP_REJECTED_MAX_STA MACSTR,
			MAC2STR(addr));
		break;
	case BLOCKED_CLIENT:
		wpa_msg(hapd->msg_ctx, MSG_INFO, AP_REJECTED_BLOCKED_STA MACSTR,
			MAC2STR(addr));
		break;
	}
}


#ifdef CONFIG_ACS
void hostapd_acs_channel_selected(struct hostapd_data *hapd,
				  struct acs_selected_channels *acs_res)
{
	int ret, i;
	int err = 0;
	struct hostapd_channel_data *pri_chan;

#ifdef CONFIG_IEEE80211BE
	if (acs_res->link_id != -1) {
		hapd = hostapd_mld_get_link_bss(hapd, acs_res->link_id);
		if (!hapd) {
			wpa_printf(MSG_ERROR,
				   "MLD: Failed to get link BSS for EVENT_ACS_CHANNEL_SELECTED link_id=%d",
				   acs_res->link_id);
			return;
		}
	}
#endif /* CONFIG_IEEE80211BE */

	if (hapd->iconf->channel) {
		wpa_printf(MSG_INFO, "ACS: Channel was already set to %d",
			   hapd->iconf->channel);
		return;
	}

	hapd->iface->freq = acs_res->pri_freq;

	if (!hapd->iface->current_mode) {
		for (i = 0; i < hapd->iface->num_hw_features; i++) {
			struct hostapd_hw_modes *mode =
				&hapd->iface->hw_features[i];

			if (mode->mode == acs_res->hw_mode) {
				if (hapd->iface->freq > 0 &&
				    !hw_get_chan(mode->mode,
						 hapd->iface->freq,
						 hapd->iface->hw_features,
						 hapd->iface->num_hw_features))
					continue;
				hapd->iface->current_mode = mode;
				break;
			}
		}
		if (!hapd->iface->current_mode) {
			hostapd_logger(hapd, NULL, HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_WARNING,
				       "driver selected to bad hw_mode");
			err = 1;
			goto out;
		}
	}

	if (!acs_res->pri_freq) {
		hostapd_logger(hapd, NULL, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_WARNING,
			       "driver switched to bad channel");
		err = 1;
		goto out;
	}
	pri_chan = hw_get_channel_freq(hapd->iface->current_mode->mode,
				       acs_res->pri_freq, NULL,
				       hapd->iface->hw_features,
				       hapd->iface->num_hw_features);
	if (!pri_chan) {
		wpa_printf(MSG_ERROR,
			   "ACS: Could not determine primary channel number from pri_freq %u",
			   acs_res->pri_freq);
		err = 1;
		goto out;
	}

	hapd->iconf->channel = pri_chan->chan;
	hapd->iconf->acs = 1;

	if (acs_res->sec_freq == 0)
		hapd->iconf->secondary_channel = 0;
	else if (acs_res->sec_freq < acs_res->pri_freq)
		hapd->iconf->secondary_channel = -1;
	else if (acs_res->sec_freq > acs_res->pri_freq)
		hapd->iconf->secondary_channel = 1;
	else {
		wpa_printf(MSG_ERROR, "Invalid secondary channel!");
		err = 1;
		goto out;
	}

	hapd->iconf->edmg_channel = acs_res->edmg_channel;

	if (hapd->iface->conf->ieee80211ac || hapd->iface->conf->ieee80211ax) {
		/* set defaults for backwards compatibility */
		hostapd_set_oper_centr_freq_seg1_idx(hapd->iconf, 0);
		hostapd_set_oper_centr_freq_seg0_idx(hapd->iconf, 0);
		hostapd_set_oper_chwidth(hapd->iconf, CONF_OPER_CHWIDTH_USE_HT);
		if (acs_res->ch_width == 40) {
			if (is_6ghz_freq(acs_res->pri_freq))
				hostapd_set_oper_centr_freq_seg0_idx(
					hapd->iconf,
					acs_res->vht_seg0_center_ch);
		} else if (acs_res->ch_width == 80) {
			hostapd_set_oper_centr_freq_seg0_idx(
				hapd->iconf, acs_res->vht_seg0_center_ch);
			if (acs_res->vht_seg1_center_ch == 0) {
				hostapd_set_oper_chwidth(
					hapd->iconf, CONF_OPER_CHWIDTH_80MHZ);
			} else {
				hostapd_set_oper_chwidth(
					hapd->iconf,
					CONF_OPER_CHWIDTH_80P80MHZ);
				hostapd_set_oper_centr_freq_seg1_idx(
					hapd->iconf,
					acs_res->vht_seg1_center_ch);
			}
		} else if (acs_res->ch_width == 160) {
			hostapd_set_oper_chwidth(hapd->iconf,
						 CONF_OPER_CHWIDTH_160MHZ);
			hostapd_set_oper_centr_freq_seg0_idx(
				hapd->iconf, acs_res->vht_seg1_center_ch);
		}
	}

#ifdef CONFIG_IEEE80211BE
	if (hapd->iface->conf->ieee80211be && acs_res->ch_width == 320) {
		hostapd_set_oper_chwidth(hapd->iconf, CONF_OPER_CHWIDTH_320MHZ);
		hostapd_set_oper_centr_freq_seg0_idx(
			hapd->iconf, acs_res->vht_seg1_center_ch);
		hostapd_set_oper_centr_freq_seg1_idx(hapd->iconf, 0);
	}

	if (hapd->iface->conf->ieee80211be && acs_res->puncture_bitmap)
		hapd->iconf->punct_bitmap = acs_res->puncture_bitmap;
#endif /* CONFIG_IEEE80211BE */

out:
	ret = hostapd_acs_completed(hapd->iface, err);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "ACS: Possibly channel configuration is invalid");
	}
}
#endif /* CONFIG_ACS */


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

#ifdef CONFIG_IEEE80211R_AP
static void hostapd_notify_auth_ft_finish(void *ctx, const u8 *dst,
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
#endif /* CONFIG_IEEE80211R_AP */


#ifdef CONFIG_FILS
static void hostapd_notify_auth_fils_finish(struct hostapd_data *hapd,
					    struct sta_info *sta, u16 resp,
					    struct wpabuf *data, int pub)
{
	if (resp == WLAN_STATUS_SUCCESS) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG, "authentication OK (FILS)");
		sta->flags |= WLAN_STA_AUTH;
		wpa_auth_sm_event(sta->wpa_sm, WPA_AUTH);
		sta->auth_alg = WLAN_AUTH_FILS_SK;
		mlme_authenticate_indication(hapd, sta);
	} else {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "authentication failed (FILS)");
	}

	hostapd_sta_auth(hapd, sta->addr, 2, resp,
			 data ? wpabuf_head(data) : NULL,
			 data ? wpabuf_len(data) : 0);
	wpabuf_free(data);
}
#endif /* CONFIG_FILS */


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
			status = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
			goto fail;
		}
	}
	sta->flags &= ~WLAN_STA_PREAUTH;
	ieee802_1x_notify_pre_auth(sta->eapol_sm, 0);
#ifdef CONFIG_IEEE80211R_AP
	if (rx_auth->auth_type == WLAN_AUTH_FT && hapd->wpa_auth) {
		sta->auth_alg = WLAN_AUTH_FT;
		if (sta->wpa_sm == NULL)
			sta->wpa_sm = wpa_auth_sta_init(hapd->wpa_auth,
							sta->addr, NULL);
		if (sta->wpa_sm == NULL) {
			wpa_printf(MSG_DEBUG,
				   "FT: Failed to initialize WPA state machine");
			status = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
		wpa_ft_process_auth(sta->wpa_sm,
				    rx_auth->auth_transaction, rx_auth->ies,
				    rx_auth->ies_len,
				    hostapd_notify_auth_ft_finish, hapd);
		return;
	}
#endif /* CONFIG_IEEE80211R_AP */

#ifdef CONFIG_FILS
	if (rx_auth->auth_type == WLAN_AUTH_FILS_SK) {
		sta->auth_alg = WLAN_AUTH_FILS_SK;
		handle_auth_fils(hapd, sta, rx_auth->ies, rx_auth->ies_len,
				 rx_auth->auth_type, rx_auth->auth_transaction,
				 rx_auth->status_code,
				 hostapd_notify_auth_fils_finish);
		return;
	}
#endif /* CONFIG_FILS */

fail:
	hostapd_sta_auth(hapd, rx_auth->peer, rx_auth->auth_transaction + 1,
			 status, resp_ies, resp_ies_len);
}


#ifndef NEED_AP_MLME
static void hostapd_action_rx(struct hostapd_data *hapd,
			      struct rx_mgmt *drv_mgmt)
{
	struct ieee80211_mgmt *mgmt;
	struct sta_info *sta;
	size_t plen __maybe_unused;
	u16 fc;
	u8 *action __maybe_unused;

	if (drv_mgmt->frame_len < IEEE80211_HDRLEN + 2 + 1)
		return;

	plen = drv_mgmt->frame_len - IEEE80211_HDRLEN;

	mgmt = (struct ieee80211_mgmt *) drv_mgmt->frame;
	fc = le_to_host16(mgmt->frame_control);
	if (WLAN_FC_GET_STYPE(fc) != WLAN_FC_STYPE_ACTION)
		return; /* handled by the driver */

	action = (u8 *) &mgmt->u.action.u;
	wpa_printf(MSG_DEBUG, "RX_ACTION category %u action %u sa " MACSTR
		   " da " MACSTR " plen %d",
		   mgmt->u.action.category, *action,
		   MAC2STR(mgmt->sa), MAC2STR(mgmt->da), (int) plen);

	sta = ap_get_sta(hapd, mgmt->sa);
	if (sta == NULL) {
		wpa_printf(MSG_DEBUG, "%s: station not found", __func__);
		return;
	}
#ifdef CONFIG_IEEE80211R_AP
	if (mgmt->u.action.category == WLAN_ACTION_FT) {
		wpa_ft_action_rx(sta->wpa_sm, (u8 *) &mgmt->u.action, plen);
		return;
	}
#endif /* CONFIG_IEEE80211R_AP */
	if (mgmt->u.action.category == WLAN_ACTION_SA_QUERY) {
		ieee802_11_sa_query_action(hapd, mgmt, drv_mgmt->frame_len);
		return;
	}
#ifdef CONFIG_WNM_AP
	if (mgmt->u.action.category == WLAN_ACTION_WNM) {
		ieee802_11_rx_wnm_action_ap(hapd, mgmt, drv_mgmt->frame_len);
		return;
	}
#endif /* CONFIG_WNM_AP */
#ifdef CONFIG_FST
	if (mgmt->u.action.category == WLAN_ACTION_FST && hapd->iface->fst) {
		fst_rx_action(hapd->iface->fst, mgmt, drv_mgmt->frame_len);
		return;
	}
#endif /* CONFIG_FST */
#ifdef CONFIG_DPP
	if (plen >= 2 + 4 &&
	    mgmt->u.action.category == WLAN_ACTION_PUBLIC &&
	    mgmt->u.action.u.vs_public_action.action ==
	    WLAN_PA_VENDOR_SPECIFIC &&
	    WPA_GET_BE24(mgmt->u.action.u.vs_public_action.oui) ==
	    OUI_WFA &&
	    mgmt->u.action.u.vs_public_action.variable[0] ==
	    DPP_OUI_TYPE) {
		const u8 *pos, *end;

		pos = mgmt->u.action.u.vs_public_action.oui;
		end = drv_mgmt->frame + drv_mgmt->frame_len;
		hostapd_dpp_rx_action(hapd, mgmt->sa, pos, end - pos,
				      drv_mgmt->freq);
		return;
	}
#endif /* CONFIG_DPP */
#ifdef CONFIG_NAN_USD
	if (mgmt->u.action.category == WLAN_ACTION_PUBLIC && plen >= 5 &&
	    mgmt->u.action.u.vs_public_action.action ==
	    WLAN_PA_VENDOR_SPECIFIC &&
	    WPA_GET_BE24(mgmt->u.action.u.vs_public_action.oui) ==
	    OUI_WFA &&
	    mgmt->u.action.u.vs_public_action.variable[0] == NAN_OUI_TYPE) {
		const u8 *pos, *end;

		pos = mgmt->u.action.u.vs_public_action.variable;
		end = drv_mgmt->frame + drv_mgmt->frame_len;
		pos++;
		hostapd_nan_usd_rx_sdf(hapd, mgmt->sa, drv_mgmt->freq,
				       pos, end - pos);
		return;
	}
#endif /* CONFIG_NAN_USD */
}
#endif /* NEED_AP_MLME */


#ifdef NEED_AP_MLME

static struct hostapd_data *
switch_link_hapd(struct hostapd_data *hapd, int link_id)
{
#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap && link_id >= 0) {
		struct hostapd_data *link_bss;

		link_bss = hostapd_mld_get_link_bss(hapd, link_id);
		if (link_bss)
			return link_bss;
	}
#endif /* CONFIG_IEEE80211BE */

	return hapd;
}


static struct hostapd_data *
switch_link_scan(struct hostapd_data *hapd, u64 scan_cookie)
{
#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap && scan_cookie != 0) {
		unsigned int i;

		for (i = 0; i < hapd->iface->interfaces->count; i++) {
			struct hostapd_iface *h;
			struct hostapd_data *h_hapd;

			h = hapd->iface->interfaces->iface[i];
			h_hapd = h->bss[0];
			if (!hostapd_is_ml_partner(hapd, h_hapd))
				continue;

			if (h_hapd->scan_cookie == scan_cookie) {
				h_hapd->scan_cookie = 0;
				return h_hapd;
			}
		}
	}
#endif /* CONFIG_IEEE80211BE */

	return hapd;
}


#define HAPD_BROADCAST ((struct hostapd_data *) -1)

static struct hostapd_data * get_hapd_bssid(struct hostapd_iface *iface,
					    const u8 *bssid, int link_id)
{
	size_t i;

	if (bssid == NULL)
		return NULL;
	if (bssid[0] == 0xff && bssid[1] == 0xff && bssid[2] == 0xff &&
	    bssid[3] == 0xff && bssid[4] == 0xff && bssid[5] == 0xff)
		return HAPD_BROADCAST;

	for (i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *hapd;
#ifdef CONFIG_IEEE80211BE
		struct hostapd_data *p_hapd;
#endif /* CONFIG_IEEE80211BE */

		hapd = iface->bss[i];
		if (ether_addr_equal(bssid, hapd->own_addr))
			return hapd;

#ifdef CONFIG_IEEE80211BE
		if (ether_addr_equal(bssid, hapd->own_addr) ||
		    (hapd->conf->mld_ap &&
		     ether_addr_equal(bssid, hapd->mld->mld_addr) &&
		     link_id == hapd->mld_link_id))
			return hapd;

		if (!hapd->conf->mld_ap)
			continue;

		for_each_mld_link(p_hapd, hapd) {
			if (p_hapd == hapd)
				continue;

			if (ether_addr_equal(bssid, p_hapd->own_addr) ||
			    (ether_addr_equal(bssid, p_hapd->mld->mld_addr) &&
			     link_id == p_hapd->mld_link_id))
				return p_hapd;
		}
#endif /* CONFIG_IEEE80211BE */
	}

	return NULL;
}


static void hostapd_rx_from_unknown_sta(struct hostapd_data *hapd,
					const u8 *bssid, const u8 *addr,
					int wds)
{
	hapd = get_hapd_bssid(hapd->iface, bssid, -1);
	if (hapd == NULL || hapd == HAPD_BROADCAST)
		return;

	ieee802_11_rx_from_unknown(hapd, addr, wds);
}


static int hostapd_mgmt_rx(struct hostapd_data *hapd, struct rx_mgmt *rx_mgmt)
{
	struct hostapd_iface *iface;
	const struct ieee80211_hdr *hdr;
	const u8 *bssid;
	struct hostapd_frame_info fi;
	int ret;

	if (rx_mgmt->ctx)
		hapd = rx_mgmt->ctx;
	hapd = switch_link_hapd(hapd, rx_mgmt->link_id);
	iface = hapd->iface;

#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->ext_mgmt_frame_handling) {
		size_t hex_len = 2 * rx_mgmt->frame_len + 1;
		char *hex = os_malloc(hex_len);

		if (hex) {
			wpa_snprintf_hex(hex, hex_len, rx_mgmt->frame,
					 rx_mgmt->frame_len);
			wpa_msg(hapd->msg_ctx, MSG_INFO, "MGMT-RX %s", hex);
			os_free(hex);
		}
		return 1;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	hdr = (const struct ieee80211_hdr *) rx_mgmt->frame;
	bssid = get_hdr_bssid(hdr, rx_mgmt->frame_len);
	if (bssid == NULL)
		return 0;

	hapd = get_hapd_bssid(iface, bssid, rx_mgmt->link_id);

	if (!hapd) {
		u16 fc = le_to_host16(hdr->frame_control);

		/*
		 * Drop frames to unknown BSSIDs except for Beacon frames which
		 * could be used to update neighbor information.
		 */
		if (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_MGMT &&
		    WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_BEACON)
			hapd = iface->bss[0];
		else
			return 0;
	}

	os_memset(&fi, 0, sizeof(fi));
	fi.freq = rx_mgmt->freq;
	fi.datarate = rx_mgmt->datarate;
	fi.ssi_signal = rx_mgmt->ssi_signal;

	if (hapd == HAPD_BROADCAST) {
		size_t i;

		ret = 0;
		for (i = 0; i < iface->num_bss; i++) {
			/* if bss is set, driver will call this function for
			 * each bss individually. */
			if (rx_mgmt->drv_priv &&
			    (iface->bss[i]->drv_priv != rx_mgmt->drv_priv))
				continue;

			if (ieee802_11_mgmt(iface->bss[i], rx_mgmt->frame,
					    rx_mgmt->frame_len, &fi) > 0)
				ret = 1;
		}
	} else
		ret = ieee802_11_mgmt(hapd, rx_mgmt->frame, rx_mgmt->frame_len,
				      &fi);

	random_add_randomness(&fi, sizeof(fi));

	return ret;
}


static void hostapd_mgmt_tx_cb(struct hostapd_data *hapd, const u8 *buf,
			       size_t len, u16 stype, int ok, int link_id)
{
	struct ieee80211_hdr *hdr;
	struct hostapd_data *orig_hapd, *tmp_hapd;

	orig_hapd = hapd;

	hdr = (struct ieee80211_hdr *) buf;
	hapd = switch_link_hapd(hapd, link_id);
	tmp_hapd = get_hapd_bssid(hapd->iface, get_hdr_bssid(hdr, len), link_id);
	if (tmp_hapd) {
		hapd = tmp_hapd;
#ifdef CONFIG_IEEE80211BE
	} else if (hapd->conf->mld_ap &&
		   ether_addr_equal(hapd->mld->mld_addr,
				    get_hdr_bssid(hdr, len))) {
		/* AP MLD address match - use hapd pointer as-is */
#endif /* CONFIG_IEEE80211BE */
	} else {
		return;
	}

	if (hapd == HAPD_BROADCAST) {
		if (stype != WLAN_FC_STYPE_ACTION || len <= 25 ||
		    buf[24] != WLAN_ACTION_PUBLIC)
			return;
		hapd = get_hapd_bssid(orig_hapd->iface, hdr->addr2, link_id);
		if (!hapd || hapd == HAPD_BROADCAST)
			return;
		/*
		 * Allow processing of TX status for a Public Action frame that
		 * used wildcard BBSID.
		 */
	}
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


static struct hostapd_data * hostapd_find_by_sta(struct hostapd_iface *iface,
						 const u8 *src, bool rsn,
						 struct sta_info **sta_ret)
{
	struct hostapd_data *hapd;
	struct sta_info *sta;
	unsigned int j;

	if (sta_ret)
		*sta_ret = NULL;

	for (j = 0; j < iface->num_bss; j++) {
		hapd = iface->bss[j];
		sta = ap_get_sta(hapd, src);
		if (sta && (sta->flags & WLAN_STA_ASSOC) &&
		    (!rsn || sta->wpa_sm)) {
			if (sta_ret)
				*sta_ret = sta;
			return hapd;
		}
#ifdef CONFIG_IEEE80211BE
		if (hapd->conf->mld_ap) {
			struct hostapd_data *p_hapd;

			for_each_mld_link(p_hapd, hapd) {
				if (p_hapd == hapd)
					continue;

				sta = ap_get_sta(p_hapd, src);
				if (sta && (sta->flags & WLAN_STA_ASSOC) &&
				    (!rsn || sta->wpa_sm)) {
					if (sta_ret)
						*sta_ret = sta;
					return p_hapd;
				}
			}
		}
#endif /* CONFIG_IEEE80211BE */
	}

	return NULL;
}


static void hostapd_event_eapol_rx(struct hostapd_data *hapd, const u8 *src,
				   const u8 *data, size_t data_len,
				   enum frame_encryption encrypted,
				   int link_id)
{
	struct hostapd_data *orig_hapd = hapd;

#ifdef CONFIG_IEEE80211BE
	hapd = switch_link_hapd(hapd, link_id);
	hapd = hostapd_find_by_sta(hapd->iface, src, true, NULL);
#else /* CONFIG_IEEE80211BE */
	hapd = hostapd_find_by_sta(hapd->iface, src, false, NULL);
#endif /* CONFIG_IEEE80211BE */

	if (!hapd) {
		/* WLAN cases need to have an existing association, but non-WLAN
		 * cases (mainly, wired IEEE 802.1X) need to be able to process
		 * EAPOL frames from new devices that do not yet have a STA
		 * entry and as such, do not get a match in
		 * hostapd_find_by_sta(). */
		wpa_printf(MSG_DEBUG,
			   "No STA-specific hostapd instance for EAPOL RX found - fall back to initial context");
		hapd = orig_hapd;
	}

	ieee802_1x_receive(hapd, src, data, data_len, encrypted);
}

#endif /* HOSTAPD */


static struct hostapd_channel_data *
hostapd_get_mode_chan(struct hostapd_hw_modes *mode, unsigned int freq)
{
	int i;
	struct hostapd_channel_data *chan;

	for (i = 0; i < mode->num_channels; i++) {
		chan = &mode->channels[i];
		if ((unsigned int) chan->freq == freq)
			return chan;
	}

	return NULL;
}


static struct hostapd_channel_data * hostapd_get_mode_channel(
	struct hostapd_iface *iface, unsigned int freq)
{
	int i;
	struct hostapd_channel_data *chan;

	for (i = 0; i < iface->num_hw_features; i++) {
		if (hostapd_hw_skip_mode(iface, &iface->hw_features[i]))
			continue;
		chan = hostapd_get_mode_chan(&iface->hw_features[i], freq);
		if (chan)
			return chan;
	}

	return NULL;
}


static void hostapd_update_nf(struct hostapd_iface *iface,
			      struct hostapd_channel_data *chan,
			      struct freq_survey *survey)
{
	if (!iface->chans_surveyed) {
		chan->min_nf = survey->nf;
		iface->lowest_nf = survey->nf;
	} else {
		if (dl_list_empty(&chan->survey_list))
			chan->min_nf = survey->nf;
		else if (survey->nf < chan->min_nf)
			chan->min_nf = survey->nf;
		if (survey->nf < iface->lowest_nf)
			iface->lowest_nf = survey->nf;
	}
}


static void hostapd_single_channel_get_survey(struct hostapd_iface *iface,
					      struct survey_results *survey_res)
{
	struct hostapd_channel_data *chan;
	struct freq_survey *survey;
	u64 divisor, dividend;

	survey = dl_list_first(&survey_res->survey_list, struct freq_survey,
			       list);
	if (!survey || !survey->freq)
		return;

	chan = hostapd_get_mode_channel(iface, survey->freq);
	if (!chan || chan->flag & HOSTAPD_CHAN_DISABLED)
		return;

	wpa_printf(MSG_DEBUG,
		   "Single Channel Survey: (freq=%d channel_time=%ld channel_time_busy=%ld)",
		   survey->freq,
		   (unsigned long int) survey->channel_time,
		   (unsigned long int) survey->channel_time_busy);

	if (survey->channel_time > iface->last_channel_time &&
	    survey->channel_time > survey->channel_time_busy) {
		dividend = survey->channel_time_busy -
			iface->last_channel_time_busy;
		divisor = survey->channel_time - iface->last_channel_time;

		iface->channel_utilization = dividend * 255 / divisor;
		wpa_printf(MSG_DEBUG, "Channel Utilization: %d",
			   iface->channel_utilization);
	}
	iface->last_channel_time = survey->channel_time;
	iface->last_channel_time_busy = survey->channel_time_busy;
}


void hostapd_event_get_survey(struct hostapd_iface *iface,
			      struct survey_results *survey_results)
{
	struct freq_survey *survey, *tmp;
	struct hostapd_channel_data *chan;

	if (dl_list_empty(&survey_results->survey_list)) {
		wpa_printf(MSG_DEBUG, "No survey data received");
		return;
	}

	if (survey_results->freq_filter) {
		hostapd_single_channel_get_survey(iface, survey_results);
		return;
	}

	dl_list_for_each_safe(survey, tmp, &survey_results->survey_list,
			      struct freq_survey, list) {
		chan = hostapd_get_mode_channel(iface, survey->freq);
		if (!chan)
			continue;
		if (chan->flag & HOSTAPD_CHAN_DISABLED)
			continue;

		dl_list_del(&survey->list);
		dl_list_add_tail(&chan->survey_list, &survey->list);

		hostapd_update_nf(iface, chan, survey);

		iface->chans_surveyed++;
	}
}


#ifdef HOSTAPD
#ifdef NEED_AP_MLME

static void hostapd_event_iface_unavailable(struct hostapd_data *hapd)
{
	wpa_printf(MSG_DEBUG, "Interface %s is unavailable -- stopped",
		   hapd->conf->iface);

	if (hapd->csa_in_progress) {
		wpa_printf(MSG_INFO, "CSA failed (%s was stopped)",
			   hapd->conf->iface);
		hostapd_switch_channel_fallback(hapd->iface,
						&hapd->cs_freq_params);
	}
}


static void hostapd_event_dfs_radar_detected(struct hostapd_data *hapd,
					     struct dfs_event *radar)
{
	wpa_printf(MSG_DEBUG, "DFS radar detected on %d MHz", radar->freq);
	hostapd_dfs_radar_detected(hapd->iface, radar->freq, radar->ht_enabled,
				   radar->chan_offset, radar->chan_width,
				   radar->cf1, radar->cf2);
}


static void hostapd_event_dfs_pre_cac_expired(struct hostapd_data *hapd,
					      struct dfs_event *radar)
{
	wpa_printf(MSG_DEBUG, "DFS Pre-CAC expired on %d MHz", radar->freq);
	hostapd_dfs_pre_cac_expired(hapd->iface, radar->freq, radar->ht_enabled,
				    radar->chan_offset, radar->chan_width,
				    radar->cf1, radar->cf2);
}


static void hostapd_event_dfs_cac_finished(struct hostapd_data *hapd,
					   struct dfs_event *radar)
{
	wpa_printf(MSG_DEBUG, "DFS CAC finished on %d MHz", radar->freq);
	hostapd_dfs_complete_cac(hapd->iface, 1, radar->freq, radar->ht_enabled,
				 radar->chan_offset, radar->chan_width,
				 radar->cf1, radar->cf2);
}


static void hostapd_event_dfs_cac_aborted(struct hostapd_data *hapd,
					  struct dfs_event *radar)
{
	wpa_printf(MSG_DEBUG, "DFS CAC aborted on %d MHz", radar->freq);
	hostapd_dfs_complete_cac(hapd->iface, 0, radar->freq, radar->ht_enabled,
				 radar->chan_offset, radar->chan_width,
				 radar->cf1, radar->cf2);
}


static void hostapd_event_dfs_nop_finished(struct hostapd_data *hapd,
					   struct dfs_event *radar)
{
	wpa_printf(MSG_DEBUG, "DFS NOP finished on %d MHz", radar->freq);
	hostapd_dfs_nop_finished(hapd->iface, radar->freq, radar->ht_enabled,
				 radar->chan_offset, radar->chan_width,
				 radar->cf1, radar->cf2);
}


static void hostapd_event_dfs_cac_started(struct hostapd_data *hapd,
					  struct dfs_event *radar)
{
	wpa_printf(MSG_DEBUG, "DFS offload CAC started on %d MHz", radar->freq);
	hostapd_dfs_start_cac(hapd->iface, radar->freq, radar->ht_enabled,
			      radar->chan_offset, radar->chan_width,
			      radar->cf1, radar->cf2);
}

#endif /* NEED_AP_MLME */


static void hostapd_event_wds_sta_interface_status(struct hostapd_data *hapd,
						   int istatus,
						   const char *ifname,
						   const u8 *addr)
{
	struct sta_info *sta = ap_get_sta(hapd, addr);

	if (sta) {
		os_free(sta->ifname_wds);
		if (istatus == INTERFACE_ADDED)
			sta->ifname_wds = os_strdup(ifname);
		else
			sta->ifname_wds = NULL;
	}

	wpa_msg(hapd->msg_ctx, MSG_INFO, "%sifname=%s sta_addr=" MACSTR,
		istatus == INTERFACE_ADDED ?
		WDS_STA_INTERFACE_ADDED : WDS_STA_INTERFACE_REMOVED,
		ifname, MAC2STR(addr));
}


#ifdef CONFIG_OWE
static int hostapd_notif_update_dh_ie(struct hostapd_data *hapd,
				      const u8 *peer, const u8 *ie,
				      size_t ie_len, const u8 *link_addr)
{
	u16 status;
	struct sta_info *sta;
	struct ieee802_11_elems elems;

	if (!hapd || !hapd->wpa_auth) {
		wpa_printf(MSG_DEBUG, "OWE: Invalid hapd context");
		return -1;
	}
	if (!peer) {
		wpa_printf(MSG_DEBUG, "OWE: Peer unknown");
		return -1;
	}
	if (!(hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_OWE)) {
		wpa_printf(MSG_DEBUG, "OWE: No OWE AKM configured");
		status = WLAN_STATUS_AKMP_NOT_VALID;
		goto err;
	}
	if (ieee802_11_parse_elems(ie, ie_len, &elems, 1) == ParseFailed) {
		wpa_printf(MSG_DEBUG, "OWE: Failed to parse OWE IE for "
			   MACSTR, MAC2STR(peer));
		status = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto err;
	}
	status = owe_validate_request(hapd, peer, elems.rsn_ie,
				      elems.rsn_ie_len,
				      elems.owe_dh, elems.owe_dh_len);
	if (status != WLAN_STATUS_SUCCESS)
		goto err;

	sta = ap_get_sta(hapd, peer);
	if (sta) {
		ap_sta_no_session_timeout(hapd, sta);
		accounting_sta_stop(hapd, sta);

		/*
		 * Make sure that the previously registered inactivity timer
		 * will not remove the STA immediately.
		 */
		sta->timeout_next = STA_NULLFUNC;
	} else {
		sta = ap_sta_add(hapd, peer);
		if (!sta) {
			status = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto err;
		}
	}
	sta->flags &= ~(WLAN_STA_WPS | WLAN_STA_MAYBE_WPS | WLAN_STA_WPS2);

#ifdef CONFIG_IEEE80211BE
	if (link_addr) {
		struct mld_info *info = &sta->mld_info;
		u8 link_id = hapd->mld_link_id;

		ap_sta_set_mld(sta, true);
		sta->mld_assoc_link_id = link_id;
		os_memcpy(info->common_info.mld_addr, peer, ETH_ALEN);
		info->links[link_id].valid = true;
		os_memcpy(info->links[link_id].local_addr, hapd->own_addr,
			  ETH_ALEN);
		os_memcpy(info->links[link_id].peer_addr, link_addr, ETH_ALEN);
	}
#endif /* CONFIG_IEEE80211BE */

	status = owe_process_rsn_ie(hapd, sta, elems.rsn_ie,
				    elems.rsn_ie_len, elems.owe_dh,
				    elems.owe_dh_len, link_addr);
	if (status != WLAN_STATUS_SUCCESS)
		ap_free_sta(hapd, sta);

	return 0;
err:
	hostapd_drv_update_dh_ie(hapd, link_addr ? link_addr : peer, status,
				 NULL, 0);
	return 0;
}
#endif /* CONFIG_OWE */


#ifdef NEED_AP_MLME
static void hostapd_eapol_tx_status(struct hostapd_data *hapd, const u8 *dst,
				    const u8 *data, size_t len, int ack,
				    int link_id)
{
	struct sta_info *sta;

	hapd = switch_link_hapd(hapd, link_id);
	hapd = hostapd_find_by_sta(hapd->iface, dst, false, &sta);

	if (!sta) {
		wpa_printf(MSG_DEBUG, "Ignore TX status for Data frame to STA "
			   MACSTR " that is not currently associated",
			   MAC2STR(dst));
		return;
	}

	ieee802_1x_eapol_tx_status(hapd, sta, data, len, ack);
}
#endif /* NEED_AP_MLME */


#ifdef CONFIG_IEEE80211AX
static void hostapd_event_color_change(struct hostapd_data *hapd, bool success)
{
	struct hostapd_data *bss;
	size_t i;

	for (i = 0; i < hapd->iface->num_bss; i++) {
		bss = hapd->iface->bss[i];
		if (bss->cca_color == 0)
			continue;

		if (success)
			hapd->iface->conf->he_op.he_bss_color = bss->cca_color;

		bss->cca_in_progress = 0;
		if (ieee802_11_set_beacon(bss)) {
			wpa_printf(MSG_ERROR, "Failed to remove BCCA element");
			bss->cca_in_progress = 1;
		} else {
			hostapd_cleanup_cca_params(bss);
		}
	}
}
#endif  /* CONFIG_IEEE80211AX */


void wpa_supplicant_event(void *ctx, enum wpa_event_type event,
			  union wpa_event_data *data)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;
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
		if (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_MGMT &&
		    WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_PROBE_REQ)
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
#ifdef NEED_AP_MLME
		if (data)
			hapd = switch_link_scan(hapd,
						data->scan_info.scan_cookie);
#endif /* NEED_AP_MLME */
		if (hapd->iface->scan_cb)
			hapd->iface->scan_cb(hapd->iface);
#ifdef CONFIG_IEEE80211BE
		if (!hapd->iface->scan_cb && hapd->conf->mld_ap) {
			/* Other links may be waiting for HT scan result */
			unsigned int i;

			for (i = 0; i < hapd->iface->interfaces->count; i++) {
				struct hostapd_iface *h =
					hapd->iface->interfaces->iface[i];
				struct hostapd_data *h_hapd = h->bss[0];

				if (hostapd_is_ml_partner(hapd, h_hapd) &&
				    h_hapd->iface->scan_cb)
					h_hapd->iface->scan_cb(h_hapd->iface);
			}
		}
#endif /* CONFIG_IEEE80211BE */
		break;
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
					   data->tx_status.ack,
					   data->tx_status.link_id);
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
					data->eapol_tx_status.ack,
					data->eapol_tx_status.link_id);
		break;
	case EVENT_DRIVER_CLIENT_POLL_OK:
		hostapd_client_poll_ok(hapd, data->client_poll.addr);
		break;
	case EVENT_RX_FROM_UNKNOWN:
		hostapd_rx_from_unknown_sta(hapd, data->rx_from_unknown.bssid,
					    data->rx_from_unknown.addr,
					    data->rx_from_unknown.wds);
		break;
#endif /* NEED_AP_MLME */
	case EVENT_RX_MGMT:
		if (!data->rx_mgmt.frame)
			break;
#ifdef NEED_AP_MLME
		hostapd_mgmt_rx(hapd, &data->rx_mgmt);
#else /* NEED_AP_MLME */
		hostapd_action_rx(hapd, &data->rx_mgmt);
#endif /* NEED_AP_MLME */
		break;
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
				       data->eapol_rx.data_len,
				       data->eapol_rx.encrypted,
				       data->eapol_rx.link_id);
		break;
	case EVENT_ASSOC:
		if (!data)
			return;
#ifdef CONFIG_IEEE80211BE
		if (data->assoc_info.assoc_link_id != -1) {
			hapd = hostapd_mld_get_link_bss(
				hapd, data->assoc_info.assoc_link_id);
			if (!hapd) {
				wpa_printf(MSG_ERROR,
					   "MLD: Failed to get link BSS for EVENT_ASSOC");
				return;
			}
		}
#endif /* CONFIG_IEEE80211BE */
		hostapd_notif_assoc(hapd, data->assoc_info.addr,
				    data->assoc_info.req_ies,
				    data->assoc_info.req_ies_len,
				    data->assoc_info.resp_ies,
				    data->assoc_info.resp_ies_len,
				    data->assoc_info.link_addr,
				    data->assoc_info.reassoc);
		break;
	case EVENT_PORT_AUTHORIZED:
		/* Port authorized event for an associated STA */
		sta = ap_get_sta(hapd, data->port_authorized.sta_addr);
		if (sta)
			ap_sta_set_authorized(hapd, sta, 1);
		else
			wpa_printf(MSG_DEBUG,
				   "No STA info matching port authorized event found");
		break;
#ifdef CONFIG_OWE
	case EVENT_UPDATE_DH:
		if (!data)
			return;
#ifdef CONFIG_IEEE80211BE
		if (data->update_dh.assoc_link_id != -1) {
			hapd = hostapd_mld_get_link_bss(
				hapd, data->update_dh.assoc_link_id);
			if (!hapd) {
				wpa_printf(MSG_ERROR,
					   "MLD: Failed to get link BSS for EVENT_UPDATE_DH assoc_link_id=%d",
					   data->update_dh.assoc_link_id);
				return;
			}
		}
#endif /* CONFIG_IEEE80211BE */
		hostapd_notif_update_dh_ie(hapd, data->update_dh.peer,
					   data->update_dh.ie,
					   data->update_dh.ie_len,
					   data->update_dh.link_addr);
		break;
#endif /* CONFIG_OWE */
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
	case EVENT_AUTH:
		hostapd_notif_auth(hapd, &data->auth);
		break;
	case EVENT_CH_SWITCH_STARTED:
	case EVENT_CH_SWITCH:
		if (!data)
			break;
#ifdef CONFIG_IEEE80211BE
		if (data->ch_switch.link_id != -1) {
			hapd = hostapd_mld_get_link_bss(
				hapd, data->ch_switch.link_id);
			if (!hapd) {
				wpa_printf(MSG_ERROR,
					   "MLD: Failed to get link (ID %d) BSS for EVENT_CH_SWITCH/EVENT_CH_SWITCH_STARTED",
					   data->ch_switch.link_id);
				break;
			}
		}
#endif /* CONFIG_IEEE80211BE */
		hostapd_event_ch_switch(hapd, data->ch_switch.freq,
					data->ch_switch.ht_enabled,
					data->ch_switch.ch_offset,
					data->ch_switch.ch_width,
					data->ch_switch.cf1,
					data->ch_switch.cf2,
					data->ch_switch.punct_bitmap,
					event == EVENT_CH_SWITCH);
		break;
	case EVENT_CONNECT_FAILED_REASON:
		if (!data)
			break;
		hostapd_event_connect_failed_reason(
			hapd, data->connect_failed_reason.addr,
			data->connect_failed_reason.code);
		break;
	case EVENT_SURVEY:
		hostapd_event_get_survey(hapd->iface, &data->survey_results);
		break;
#ifdef NEED_AP_MLME
	case EVENT_INTERFACE_UNAVAILABLE:
		hostapd_event_iface_unavailable(hapd);
		break;
	case EVENT_DFS_RADAR_DETECTED:
		if (!data)
			break;
		hapd = switch_link_hapd(hapd, data->dfs_event.link_id);
		hostapd_event_dfs_radar_detected(hapd, &data->dfs_event);
		break;
	case EVENT_DFS_PRE_CAC_EXPIRED:
		if (!data)
			break;
		hapd = switch_link_hapd(hapd, data->dfs_event.link_id);
		hostapd_event_dfs_pre_cac_expired(hapd, &data->dfs_event);
		break;
	case EVENT_DFS_CAC_FINISHED:
		if (!data)
			break;
		hapd = switch_link_hapd(hapd, data->dfs_event.link_id);
		hostapd_event_dfs_cac_finished(hapd, &data->dfs_event);
		break;
	case EVENT_DFS_CAC_ABORTED:
		if (!data)
			break;
		hapd = switch_link_hapd(hapd, data->dfs_event.link_id);
		hostapd_event_dfs_cac_aborted(hapd, &data->dfs_event);
		break;
	case EVENT_DFS_NOP_FINISHED:
		if (!data)
			break;
		hapd = switch_link_hapd(hapd, data->dfs_event.link_id);
		hostapd_event_dfs_nop_finished(hapd, &data->dfs_event);
		break;
	case EVENT_CHANNEL_LIST_CHANGED:
		/* channel list changed (regulatory?), update channel list */
		/* TODO: check this. hostapd_get_hw_features() initializes
		 * too much stuff. */
		/* hostapd_get_hw_features(hapd->iface); */
		hostapd_channel_list_updated(
			hapd->iface, data->channel_list_changed.initiator);
		break;
	case EVENT_DFS_CAC_STARTED:
		if (!data)
			break;
		hapd = switch_link_hapd(hapd, data->dfs_event.link_id);
		hostapd_event_dfs_cac_started(hapd, &data->dfs_event);
		break;
#endif /* NEED_AP_MLME */
	case EVENT_INTERFACE_ENABLED:
		wpa_msg(hapd->msg_ctx, MSG_INFO, INTERFACE_ENABLED);
		if (hapd->disabled && hapd->started) {
			hapd->disabled = 0;
			/*
			 * Try to re-enable interface if the driver stopped it
			 * when the interface got disabled.
			 */
			if (hapd->wpa_auth)
				wpa_auth_reconfig_group_keys(hapd->wpa_auth);
			else
				hostapd_reconfig_encryption(hapd);
			hapd->reenable_beacon = 1;
			ieee802_11_set_beacon(hapd);
#ifdef NEED_AP_MLME
		} else if (hapd->disabled && hapd->iface->cac_started) {
			wpa_printf(MSG_DEBUG, "DFS: restarting pending CAC");
			hostapd_handle_dfs(hapd->iface);
#endif /* NEED_AP_MLME */
		}
		break;
	case EVENT_INTERFACE_DISABLED:
		hostapd_free_stas(hapd);
		wpa_msg(hapd->msg_ctx, MSG_INFO, INTERFACE_DISABLED);
		hapd->disabled = 1;
		break;
#ifdef CONFIG_ACS
	case EVENT_ACS_CHANNEL_SELECTED:
		hostapd_acs_channel_selected(hapd,
					     &data->acs_selected_channels);
		break;
#endif /* CONFIG_ACS */
	case EVENT_STATION_OPMODE_CHANGED:
		hostapd_event_sta_opmode_changed(hapd, data->sta_opmode.addr,
						 data->sta_opmode.smps_mode,
						 data->sta_opmode.chan_width,
						 data->sta_opmode.rx_nss);
		break;
	case EVENT_WDS_STA_INTERFACE_STATUS:
		hostapd_event_wds_sta_interface_status(
			hapd, data->wds_sta_interface.istatus,
			data->wds_sta_interface.ifname,
			data->wds_sta_interface.sta_addr);
		break;
#ifdef CONFIG_IEEE80211AX
	case EVENT_BSS_COLOR_COLLISION:
		/* The BSS color is shared amongst all BBSs on a specific phy.
		 * Therefore we always start the color change on the primary
		 * BSS. */
		hapd = switch_link_hapd(hapd,
					data->bss_color_collision.link_id);
		wpa_printf(MSG_DEBUG, "BSS color collision on %s",
			   hapd->conf->iface);
		hostapd_switch_color(hapd->iface->bss[0],
				     data->bss_color_collision.bitmap);
		break;
	case EVENT_CCA_STARTED_NOTIFY:
		hapd = switch_link_hapd(hapd,
					data->bss_color_collision.link_id);
		wpa_printf(MSG_DEBUG, "CCA started on %s",
			   hapd->conf->iface);
		break;
	case EVENT_CCA_ABORTED_NOTIFY:
		hapd = switch_link_hapd(hapd,
					data->bss_color_collision.link_id);
		wpa_printf(MSG_DEBUG, "CCA aborted on %s",
			   hapd->conf->iface);
		hostapd_event_color_change(hapd, false);
		break;
	case EVENT_CCA_NOTIFY:
		hapd = switch_link_hapd(hapd,
					data->bss_color_collision.link_id);
		wpa_printf(MSG_DEBUG, "CCA finished on %s",
			   hapd->conf->iface);
		hostapd_event_color_change(hapd, true);
		break;
#endif /* CONFIG_IEEE80211AX */
	default:
		wpa_printf(MSG_DEBUG, "Unknown event %d", event);
		break;
	}
}


void wpa_supplicant_event_global(void *ctx, enum wpa_event_type event,
				 union wpa_event_data *data)
{
	struct hapd_interfaces *interfaces = ctx;
	struct hostapd_data *hapd;

	if (event != EVENT_INTERFACE_STATUS)
		return;

	hapd = hostapd_get_iface(interfaces, data->interface_status.ifname);
	if (hapd && hapd->driver && hapd->driver->get_ifindex &&
	    hapd->drv_priv) {
		unsigned int ifindex;

		ifindex = hapd->driver->get_ifindex(hapd->drv_priv);
		if (ifindex != data->interface_status.ifindex) {
			wpa_dbg(hapd->msg_ctx, MSG_DEBUG,
				"interface status ifindex %d mismatch (%d)",
				ifindex, data->interface_status.ifindex);
			return;
		}
	}
	if (hapd)
		wpa_supplicant_event(hapd, event, data);
}

#endif /* HOSTAPD */
