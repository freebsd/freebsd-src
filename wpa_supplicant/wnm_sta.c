/*
 * wpa_supplicant - WNM
 * Copyright (c) 2011-2013, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "common/wpa_ctrl.h"
#include "common/ocv.h"
#include "rsn_supp/wpa.h"
#include "config.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "scan.h"
#include "ctrl_iface.h"
#include "bss.h"
#include "wnm_sta.h"
#include "notify.h"
#include "hs20_supplicant.h"

#define MAX_TFS_IE_LEN  1024
#define WNM_MAX_NEIGHBOR_REPORT 10


/* get the TFS IE from driver */
static int ieee80211_11_get_tfs_ie(struct wpa_supplicant *wpa_s, u8 *buf,
				   u16 *buf_len, enum wnm_oper oper)
{
	wpa_printf(MSG_DEBUG, "%s: TFS get operation %d", __func__, oper);

	return wpa_drv_wnm_oper(wpa_s, oper, wpa_s->bssid, buf, buf_len);
}


/* set the TFS IE to driver */
static int ieee80211_11_set_tfs_ie(struct wpa_supplicant *wpa_s,
				   const u8 *addr, const u8 *buf, u16 buf_len,
				   enum wnm_oper oper)
{
	u16 len = buf_len;

	wpa_printf(MSG_DEBUG, "%s: TFS set operation %d", __func__, oper);

	return wpa_drv_wnm_oper(wpa_s, oper, addr, (u8 *) buf, &len);
}


/* MLME-SLEEPMODE.request */
int ieee802_11_send_wnmsleep_req(struct wpa_supplicant *wpa_s,
				 u8 action, u16 intval, struct wpabuf *tfs_req)
{
	struct ieee80211_mgmt *mgmt;
	int res;
	size_t len;
	struct wnm_sleep_element *wnmsleep_ie;
	u8 *wnmtfs_ie, *oci_ie;
	u8 wnmsleep_ie_len, oci_ie_len;
	u16 wnmtfs_ie_len;  /* possibly multiple IE(s) */
	enum wnm_oper tfs_oper = action == 0 ? WNM_SLEEP_TFS_REQ_IE_ADD :
		WNM_SLEEP_TFS_REQ_IE_NONE;

	wpa_printf(MSG_DEBUG, "WNM: Request to send WNM-Sleep Mode Request "
		   "action=%s to " MACSTR,
		   action == 0 ? "enter" : "exit",
		   MAC2STR(wpa_s->bssid));

	/* WNM-Sleep Mode IE */
	wnmsleep_ie_len = sizeof(struct wnm_sleep_element);
	wnmsleep_ie = os_zalloc(sizeof(struct wnm_sleep_element));
	if (wnmsleep_ie == NULL)
		return -1;
	wnmsleep_ie->eid = WLAN_EID_WNMSLEEP;
	wnmsleep_ie->len = wnmsleep_ie_len - 2;
	wnmsleep_ie->action_type = action;
	wnmsleep_ie->status = WNM_STATUS_SLEEP_ACCEPT;
	wnmsleep_ie->intval = host_to_le16(intval);
	wpa_hexdump(MSG_DEBUG, "WNM: WNM-Sleep Mode element",
		    (u8 *) wnmsleep_ie, wnmsleep_ie_len);

	/* TFS IE(s) */
	if (tfs_req) {
		wnmtfs_ie_len = wpabuf_len(tfs_req);
		wnmtfs_ie = os_memdup(wpabuf_head(tfs_req), wnmtfs_ie_len);
		if (wnmtfs_ie == NULL) {
			os_free(wnmsleep_ie);
			return -1;
		}
	} else {
		wnmtfs_ie = os_zalloc(MAX_TFS_IE_LEN);
		if (wnmtfs_ie == NULL) {
			os_free(wnmsleep_ie);
			return -1;
		}
		if (ieee80211_11_get_tfs_ie(wpa_s, wnmtfs_ie, &wnmtfs_ie_len,
					    tfs_oper)) {
			wnmtfs_ie_len = 0;
			os_free(wnmtfs_ie);
			wnmtfs_ie = NULL;
		}
	}
	wpa_hexdump(MSG_DEBUG, "WNM: TFS Request element",
		    (u8 *) wnmtfs_ie, wnmtfs_ie_len);

	oci_ie = NULL;
	oci_ie_len = 0;
#ifdef CONFIG_OCV
	if (action == WNM_SLEEP_MODE_EXIT && wpa_sm_ocv_enabled(wpa_s->wpa)) {
		struct wpa_channel_info ci;

		if (wpa_drv_channel_info(wpa_s, &ci) != 0) {
			wpa_printf(MSG_WARNING,
				   "Failed to get channel info for OCI element in WNM-Sleep Mode frame");
			os_free(wnmsleep_ie);
			os_free(wnmtfs_ie);
			return -1;
		}
#ifdef CONFIG_TESTING_OPTIONS
		if (wpa_s->oci_freq_override_wnm_sleep) {
			wpa_printf(MSG_INFO,
				   "TEST: Override OCI KDE frequency %d -> %d MHz",
				   ci.frequency,
				   wpa_s->oci_freq_override_wnm_sleep);
			ci.frequency = wpa_s->oci_freq_override_wnm_sleep;
		}
#endif /* CONFIG_TESTING_OPTIONS */

		oci_ie_len = OCV_OCI_EXTENDED_LEN;
		oci_ie = os_zalloc(oci_ie_len);
		if (!oci_ie) {
			wpa_printf(MSG_WARNING,
				   "Failed to allocate buffer for for OCI element in WNM-Sleep Mode frame");
			os_free(wnmsleep_ie);
			os_free(wnmtfs_ie);
			return -1;
		}

		if (ocv_insert_extended_oci(&ci, oci_ie) < 0) {
			os_free(wnmsleep_ie);
			os_free(wnmtfs_ie);
			os_free(oci_ie);
			return -1;
		}
	}
#endif /* CONFIG_OCV */

	mgmt = os_zalloc(sizeof(*mgmt) + wnmsleep_ie_len + wnmtfs_ie_len +
			 oci_ie_len);
	if (mgmt == NULL) {
		wpa_printf(MSG_DEBUG, "MLME: Failed to allocate buffer for "
			   "WNM-Sleep Request action frame");
		os_free(wnmsleep_ie);
		os_free(wnmtfs_ie);
		return -1;
	}

	os_memcpy(mgmt->da, wpa_s->bssid, ETH_ALEN);
	os_memcpy(mgmt->sa, wpa_s->own_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, wpa_s->bssid, ETH_ALEN);
	mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_ACTION);
	mgmt->u.action.category = WLAN_ACTION_WNM;
	mgmt->u.action.u.wnm_sleep_req.action = WNM_SLEEP_MODE_REQ;
	mgmt->u.action.u.wnm_sleep_req.dialogtoken = 1;
	os_memcpy(mgmt->u.action.u.wnm_sleep_req.variable, wnmsleep_ie,
		  wnmsleep_ie_len);
	/* copy TFS IE here */
	if (wnmtfs_ie_len > 0) {
		os_memcpy(mgmt->u.action.u.wnm_sleep_req.variable +
			  wnmsleep_ie_len, wnmtfs_ie, wnmtfs_ie_len);
	}

#ifdef CONFIG_OCV
	/* copy OCV OCI here */
	if (oci_ie_len > 0) {
		os_memcpy(mgmt->u.action.u.wnm_sleep_req.variable +
			  wnmsleep_ie_len + wnmtfs_ie_len, oci_ie, oci_ie_len);
	}
#endif /* CONFIG_OCV */

	len = 1 + sizeof(mgmt->u.action.u.wnm_sleep_req) + wnmsleep_ie_len +
		wnmtfs_ie_len + oci_ie_len;

	res = wpa_drv_send_action(wpa_s, wpa_s->assoc_freq, 0, wpa_s->bssid,
				  wpa_s->own_addr, wpa_s->bssid,
				  &mgmt->u.action.category, len, 0);
	if (res < 0)
		wpa_printf(MSG_DEBUG, "Failed to send WNM-Sleep Request "
			   "(action=%d, intval=%d)", action, intval);
	else
		wpa_s->wnmsleep_used = 1;

	os_free(wnmsleep_ie);
	os_free(wnmtfs_ie);
	os_free(oci_ie);
	os_free(mgmt);

	return res;
}


static void wnm_sleep_mode_enter_success(struct wpa_supplicant *wpa_s,
					 const u8 *tfsresp_ie_start,
					 const u8 *tfsresp_ie_end)
{
	wpa_drv_wnm_oper(wpa_s, WNM_SLEEP_ENTER_CONFIRM,
			 wpa_s->bssid, NULL, NULL);
	/* remove GTK/IGTK ?? */

	/* set the TFS Resp IE(s) */
	if (tfsresp_ie_start && tfsresp_ie_end &&
	    tfsresp_ie_end - tfsresp_ie_start >= 0) {
		u16 tfsresp_ie_len;
		tfsresp_ie_len = (tfsresp_ie_end + tfsresp_ie_end[1] + 2) -
			tfsresp_ie_start;
		wpa_printf(MSG_DEBUG, "TFS Resp IE(s) found");
		/* pass the TFS Resp IE(s) to driver for processing */
		if (ieee80211_11_set_tfs_ie(wpa_s, wpa_s->bssid,
					    tfsresp_ie_start,
					    tfsresp_ie_len,
					    WNM_SLEEP_TFS_RESP_IE_SET))
			wpa_printf(MSG_DEBUG, "WNM: Fail to set TFS Resp IE");
	}
}


static void wnm_sleep_mode_exit_success(struct wpa_supplicant *wpa_s,
					const u8 *frm, u16 key_len_total)
{
	u8 *ptr, *end;
	u8 gtk_len;

	wpa_drv_wnm_oper(wpa_s, WNM_SLEEP_EXIT_CONFIRM,  wpa_s->bssid,
			 NULL, NULL);

	/* Install GTK/IGTK */

	/* point to key data field */
	ptr = (u8 *) frm + 1 + 2;
	end = ptr + key_len_total;
	wpa_hexdump_key(MSG_DEBUG, "WNM: Key Data", ptr, key_len_total);

	if (key_len_total && !wpa_sm_pmf_enabled(wpa_s->wpa)) {
		wpa_msg(wpa_s, MSG_INFO,
			"WNM: Ignore Key Data in WNM-Sleep Mode Response - PMF not enabled");
		return;
	}

	while (end - ptr > 1) {
		if (2 + ptr[1] > end - ptr) {
			wpa_printf(MSG_DEBUG, "WNM: Invalid Key Data element "
				   "length");
			if (end > ptr) {
				wpa_hexdump(MSG_DEBUG, "WNM: Remaining data",
					    ptr, end - ptr);
			}
			break;
		}
		if (*ptr == WNM_SLEEP_SUBELEM_GTK) {
			if (ptr[1] < 11 + 5) {
				wpa_printf(MSG_DEBUG, "WNM: Too short GTK "
					   "subelem");
				break;
			}
			gtk_len = *(ptr + 4);
			if (ptr[1] < 11 + gtk_len ||
			    gtk_len < 5 || gtk_len > 32) {
				wpa_printf(MSG_DEBUG, "WNM: Invalid GTK "
					   "subelem");
				break;
			}
			wpa_wnmsleep_install_key(
				wpa_s->wpa,
				WNM_SLEEP_SUBELEM_GTK,
				ptr);
			ptr += 13 + gtk_len;
		} else if (*ptr == WNM_SLEEP_SUBELEM_IGTK) {
			if (ptr[1] < 2 + 6 + WPA_IGTK_LEN) {
				wpa_printf(MSG_DEBUG, "WNM: Too short IGTK "
					   "subelem");
				break;
			}
			wpa_wnmsleep_install_key(wpa_s->wpa,
						 WNM_SLEEP_SUBELEM_IGTK, ptr);
			ptr += 10 + WPA_IGTK_LEN;
		} else if (*ptr == WNM_SLEEP_SUBELEM_BIGTK) {
			if (ptr[1] < 2 + 6 + WPA_BIGTK_LEN) {
				wpa_printf(MSG_DEBUG,
					   "WNM: Too short BIGTK subelem");
				break;
			}
			wpa_wnmsleep_install_key(wpa_s->wpa,
						 WNM_SLEEP_SUBELEM_BIGTK, ptr);
			ptr += 10 + WPA_BIGTK_LEN;
		} else
			break; /* skip the loop */
	}
}


static void ieee802_11_rx_wnmsleep_resp(struct wpa_supplicant *wpa_s,
					const u8 *da, const u8 *sa,
					const u8 *frm, int len)
{
	/*
	 * Action [1] | Dialog Token [1] | Key Data Len [2] | Key Data |
	 * WNM-Sleep Mode IE | TFS Response IE
	 */
	const u8 *pos = frm; /* point to payload after the action field */
	u16 key_len_total;
	struct wnm_sleep_element *wnmsleep_ie = NULL;
	/* multiple TFS Resp IE (assuming consecutive) */
	const u8 *tfsresp_ie_start = NULL;
	const u8 *tfsresp_ie_end = NULL;
#ifdef CONFIG_OCV
	const u8 *oci_ie = NULL;
	u8 oci_ie_len = 0;
#endif /* CONFIG_OCV */
	size_t left;

	if (!wpa_s->wnmsleep_used) {
		wpa_printf(MSG_DEBUG,
			   "WNM: Ignore WNM-Sleep Mode Response frame since WNM-Sleep Mode operation has not been requested");
		return;
	}

	if (is_multicast_ether_addr(da)) {
		wpa_printf(MSG_DEBUG,
			   "WNM: Ignore group-addressed WNM-Sleep Mode Response frame (A1="
			   MACSTR " A2=" MACSTR ")",
			   MAC2STR(da), MAC2STR(sa));
		return;
	}

	if (len < 3)
		return;
	key_len_total = WPA_GET_LE16(frm + 1);

	wpa_printf(MSG_DEBUG, "WNM-Sleep Mode Response token=%u key_len_total=%d",
		   frm[0], key_len_total);
	left = len - 3;
	if (key_len_total > left) {
		wpa_printf(MSG_INFO, "WNM: Too short frame for Key Data field");
		return;
	}
	pos += 3 + key_len_total;
	while (pos - frm + 1 < len) {
		u8 ie_len = *(pos + 1);
		if (2 + ie_len > frm + len - pos) {
			wpa_printf(MSG_INFO, "WNM: Invalid IE len %u", ie_len);
			break;
		}
		wpa_hexdump(MSG_DEBUG, "WNM: Element", pos, 2 + ie_len);
		if (*pos == WLAN_EID_WNMSLEEP && ie_len >= 4)
			wnmsleep_ie = (struct wnm_sleep_element *) pos;
		else if (*pos == WLAN_EID_TFS_RESP) {
			if (!tfsresp_ie_start)
				tfsresp_ie_start = pos;
			tfsresp_ie_end = pos;
#ifdef CONFIG_OCV
		} else if (*pos == WLAN_EID_EXTENSION && ie_len >= 1 &&
			   pos[2] == WLAN_EID_EXT_OCV_OCI) {
			oci_ie = pos + 3;
			oci_ie_len = ie_len - 1;
#endif /* CONFIG_OCV */
		} else
			wpa_printf(MSG_DEBUG, "EID %d not recognized", *pos);
		pos += ie_len + 2;
	}

	if (!wnmsleep_ie) {
		wpa_printf(MSG_DEBUG, "No WNM-Sleep IE found");
		return;
	}

#ifdef CONFIG_OCV
	if (wnmsleep_ie->action_type == WNM_SLEEP_MODE_EXIT &&
	    wpa_sm_ocv_enabled(wpa_s->wpa)) {
		struct wpa_channel_info ci;

		if (wpa_drv_channel_info(wpa_s, &ci) != 0) {
			wpa_msg(wpa_s, MSG_WARNING,
				"Failed to get channel info to validate received OCI in WNM-Sleep Mode frame");
			return;
		}

		if (ocv_verify_tx_params(oci_ie, oci_ie_len, &ci,
					 channel_width_to_int(ci.chanwidth),
					 ci.seg1_idx) != OCI_SUCCESS) {
			wpa_msg(wpa_s, MSG_WARNING, "WNM: OCV failed: %s",
				ocv_errorstr);
			return;
		}
	}
#endif /* CONFIG_OCV */

	wpa_s->wnmsleep_used = 0;

	if (wnmsleep_ie->status == WNM_STATUS_SLEEP_ACCEPT ||
	    wnmsleep_ie->status == WNM_STATUS_SLEEP_EXIT_ACCEPT_GTK_UPDATE) {
		wpa_printf(MSG_DEBUG, "Successfully recv WNM-Sleep Response "
			   "frame (action=%d, intval=%d)",
			   wnmsleep_ie->action_type, wnmsleep_ie->intval);
		if (wnmsleep_ie->action_type == WNM_SLEEP_MODE_ENTER) {
			wnm_sleep_mode_enter_success(wpa_s, tfsresp_ie_start,
						     tfsresp_ie_end);
		} else if (wnmsleep_ie->action_type == WNM_SLEEP_MODE_EXIT) {
			wnm_sleep_mode_exit_success(wpa_s, frm, key_len_total);
		}
	} else {
		wpa_printf(MSG_DEBUG, "Reject recv WNM-Sleep Response frame "
			   "(action=%d, intval=%d)",
			   wnmsleep_ie->action_type, wnmsleep_ie->intval);
		if (wnmsleep_ie->action_type == WNM_SLEEP_MODE_ENTER)
			wpa_drv_wnm_oper(wpa_s, WNM_SLEEP_ENTER_FAIL,
					 wpa_s->bssid, NULL, NULL);
		else if (wnmsleep_ie->action_type == WNM_SLEEP_MODE_EXIT)
			wpa_drv_wnm_oper(wpa_s, WNM_SLEEP_EXIT_FAIL,
					 wpa_s->bssid, NULL, NULL);
	}
}


void wnm_btm_reset(struct wpa_supplicant *wpa_s)
{
	int i;

	for (i = 0; i < wpa_s->wnm_num_neighbor_report; i++) {
		os_free(wpa_s->wnm_neighbor_report_elements[i].meas_pilot);
		os_free(wpa_s->wnm_neighbor_report_elements[i].mul_bssid);
	}

	wpa_s->wnm_num_neighbor_report = 0;
	os_free(wpa_s->wnm_neighbor_report_elements);
	wpa_s->wnm_neighbor_report_elements = NULL;

	wpa_s->wnm_target_bss = NULL;

	wpa_s->wnm_cand_valid_until.sec = 0;
	wpa_s->wnm_cand_valid_until.usec = 0;

	wpa_s->wnm_mode = 0;
	wpa_s->wnm_dialog_token = 0;
	wpa_s->wnm_reply = 0;

#ifdef CONFIG_MBO
	wpa_s->wnm_mbo_trans_reason_present = 0;
	wpa_s->wnm_mbo_transition_reason = 0;
#endif /* CONFIG_MBO */
}


static void wnm_parse_neighbor_report_multi_link(struct neighbor_report *rep,
						 u8 id, u8 elen, const u8 *pos)
{
	const struct ieee80211_eht_ml *ml =
		(const struct ieee80211_eht_ml *) pos;
	bool has_link_id;
	u8 common_info_len;

	/* The Basic Multi-Link subelement has the same body as the Basic MLE.
	 * It includes at least the 2 octet Multi-Link Control field, 1 octet
	 * Common Info Length, and the 6 oxtet MLD MAC Address fields. */
	if (elen < sizeof(*ml) + 1 + ETH_ALEN) {
		wpa_printf(MSG_DEBUG, "WNM: Too short ML element");
		return;
	}

	/* The ML control should be all zeroes except for the Link ID Info
	 * Present field. */
	if ((le_to_host16(ml->ml_control) &
	     ~BASIC_MULTI_LINK_CTRL_PRES_LINK_ID))
		wpa_printf(MSG_DEBUG,
			   "WNM: Ignore unsupported ML Control field bits: 0x%04x",
			   le_to_host16(ml->ml_control) &
			   ~BASIC_MULTI_LINK_CTRL_PRES_LINK_ID);

	has_link_id = !!(le_to_host16(ml->ml_control) &
			 BASIC_MULTI_LINK_CTRL_PRES_LINK_ID);

	/* Followed by the Common Info Length and the MLD MAC Address fields */
	common_info_len = pos[2];
	if (common_info_len < 1 + ETH_ALEN) {
		wpa_printf(MSG_DEBUG, "WNM: Too short ML Common Info: %u < 7",
			   common_info_len);
		return;
	}

	/* MLD MAC Address */
	os_memcpy(rep->mld_addr, &pos[3], ETH_ALEN);

	if (!has_link_id)
		return;

	if (common_info_len < 1 + ETH_ALEN + 1 || common_info_len + 2 > elen) {
		wpa_printf(MSG_DEBUG,
			   "WNM: ML Common Info too short or does not fit: %u (elen: %u)",
			   common_info_len, elen);
		return;
	}

	/* Link ID Info */
	if ((pos[9] & EHT_ML_LINK_ID_MSK) >= MAX_NUM_MLD_LINKS) {
		wpa_printf(MSG_DEBUG,
			   "WNM: ML common info contains invalid link ID");
		return;
	}

	rep->mld_links = BIT(pos[9] & EHT_ML_LINK_ID_MSK);

	elen -= common_info_len + 2;
	pos += common_info_len + 2;

	/* Parse out per-STA information */
	while (elen >= 2) {
		u8 sub_elem_len = pos[1];

		if (2 + sub_elem_len > elen) {
			wpa_printf(MSG_DEBUG,
				   "WNM: Invalid sub-element length: %u %u",
				   2 + sub_elem_len, elen);
			rep->mld_links = 0;
			break;
		}

		if  (*pos == MULTI_LINK_SUB_ELEM_ID_PER_STA_PROFILE) {
			const struct ieee80211_eht_per_sta_profile *sta_prof =
				(const struct ieee80211_eht_per_sta_profile *)
				(pos + 2);
			u16 control;
			u8 link_id;

			if (sub_elem_len < sizeof(*sta_prof)) {
				wpa_printf(MSG_DEBUG,
					   "WNM: Invalid STA-profile length: %u",
					   sub_elem_len);
				rep->mld_links = 0;
				break;
			}

			control = le_to_host16(sta_prof->sta_control);

			link_id = control & EHT_PER_STA_RECONF_CTRL_LINK_ID_MSK;
			if (link_id < MAX_NUM_MLD_LINKS)
				rep->mld_links |= BIT(link_id);
		}

		pos += 2 + sub_elem_len;
		elen -= 2 + sub_elem_len;
	}

	if (elen != 0) {
		wpa_printf(MSG_DEBUG,
			   "WNM: Data left at end of multi-link element: %u",
			   elen);
		rep->mld_links = 0;
	}
}


static void wnm_parse_neighbor_report_elem(struct neighbor_report *rep,
					   u8 id, u8 elen, const u8 *pos)
{
	switch (id) {
	case WNM_NEIGHBOR_TSF:
		if (elen < 2 + 2) {
			wpa_printf(MSG_DEBUG, "WNM: Too short TSF");
			break;
		}
		rep->tsf_offset = WPA_GET_LE16(pos);
		rep->beacon_int = WPA_GET_LE16(pos + 2);
		rep->tsf_present = 1;
		break;
	case WNM_NEIGHBOR_CONDENSED_COUNTRY_STRING:
		if (elen < 2) {
			wpa_printf(MSG_DEBUG, "WNM: Too short condensed "
				   "country string");
			break;
		}
		os_memcpy(rep->country, pos, 2);
		rep->country_present = 1;
		break;
	case WNM_NEIGHBOR_BSS_TRANSITION_CANDIDATE:
		if (elen < 1) {
			wpa_printf(MSG_DEBUG, "WNM: Too short BSS transition "
				   "candidate");
			break;
		}
		rep->preference = pos[0];
		rep->preference_present = 1;
		break;
	case WNM_NEIGHBOR_BSS_TERMINATION_DURATION:
		if (elen < 10) {
			wpa_printf(MSG_DEBUG,
				   "WNM: Too short BSS termination duration");
			break;
		}
		rep->bss_term_tsf = WPA_GET_LE64(pos);
		rep->bss_term_dur = WPA_GET_LE16(pos + 8);
		rep->bss_term_present = 1;
		break;
	case WNM_NEIGHBOR_BEARING:
		if (elen < 8) {
			wpa_printf(MSG_DEBUG, "WNM: Too short neighbor "
				   "bearing");
			break;
		}
		rep->bearing = WPA_GET_LE16(pos);
		rep->distance = WPA_GET_LE32(pos + 2);
		rep->rel_height = WPA_GET_LE16(pos + 2 + 4);
		rep->bearing_present = 1;
		break;
	case WNM_NEIGHBOR_MEASUREMENT_PILOT:
		if (elen < 1) {
			wpa_printf(MSG_DEBUG, "WNM: Too short measurement "
				   "pilot");
			break;
		}
		os_free(rep->meas_pilot);
		rep->meas_pilot = os_zalloc(sizeof(struct measurement_pilot));
		if (rep->meas_pilot == NULL)
			break;
		rep->meas_pilot->measurement_pilot = pos[0];
		rep->meas_pilot->subelem_len = elen - 1;
		os_memcpy(rep->meas_pilot->subelems, pos + 1, elen - 1);
		break;
	case WNM_NEIGHBOR_RRM_ENABLED_CAPABILITIES:
		if (elen < 5) {
			wpa_printf(MSG_DEBUG, "WNM: Too short RRM enabled "
				   "capabilities");
			break;
		}
		os_memcpy(rep->rm_capab, pos, 5);
		rep->rm_capab_present = 1;
		break;
	case WNM_NEIGHBOR_MULTIPLE_BSSID:
		if (elen < 1) {
			wpa_printf(MSG_DEBUG, "WNM: Too short multiple BSSID");
			break;
		}
		os_free(rep->mul_bssid);
		rep->mul_bssid = os_zalloc(sizeof(struct multiple_bssid));
		if (rep->mul_bssid == NULL)
			break;
		rep->mul_bssid->max_bssid_indicator = pos[0];
		rep->mul_bssid->subelem_len = elen - 1;
		os_memcpy(rep->mul_bssid->subelems, pos + 1, elen - 1);
		break;
	case WNM_NEIGHBOR_MULTI_LINK:
		wnm_parse_neighbor_report_multi_link(rep, id, elen, pos);
		break;
	default:
		wpa_printf(MSG_DEBUG,
			   "WNM: Unsupported neighbor report subelement id %u",
			   id);
		break;
	}
}


static int wnm_nei_get_chan(struct wpa_supplicant *wpa_s, u8 op_class, u8 chan)
{
	struct wpa_bss *bss = wpa_s->current_bss;
	const char *country = NULL;
	int freq;

	if (bss) {
		const u8 *elem = wpa_bss_get_ie(bss, WLAN_EID_COUNTRY);

		if (elem && elem[1] >= 2)
			country = (const char *) (elem + 2);
	}

	freq = ieee80211_chan_to_freq(country, op_class, chan);
	if (freq <= 0 && (op_class == 0 || op_class == 255)) {
		/*
		 * Some APs do not advertise correct operating class
		 * information. Try to determine the most likely operating
		 * frequency based on the channel number.
		 */
		if (chan >= 1 && chan <= 13)
			freq = 2407 + chan * 5;
		else if (chan == 14)
			freq = 2484;
		else if (chan >= 36 && chan <= 177)
			freq = 5000 + chan * 5;
	}
	return freq;
}


static void wnm_parse_neighbor_report(struct wpa_supplicant *wpa_s,
				      const u8 *pos, u8 len,
				      struct neighbor_report *rep)
{
	u8 left = len;

	if (left < 13) {
		wpa_printf(MSG_DEBUG, "WNM: Too short neighbor report");
		return;
	}

	os_memcpy(rep->bssid, pos, ETH_ALEN);
	rep->bssid_info = WPA_GET_LE32(pos + ETH_ALEN);
	rep->regulatory_class = *(pos + 10);
	rep->channel_number = *(pos + 11);
	rep->phy_type = *(pos + 12);

	pos += 13;
	left -= 13;

	while (left >= 2) {
		u8 id, elen;

		id = *pos++;
		elen = *pos++;
		wpa_printf(MSG_DEBUG, "WNM: Subelement id=%u len=%u", id, elen);
		left -= 2;
		if (elen > left) {
			wpa_printf(MSG_DEBUG,
				   "WNM: Truncated neighbor report subelement");
			break;
		}
		wnm_parse_neighbor_report_elem(rep, id, elen, pos);
		left -= elen;
		pos += elen;
	}

	rep->freq = wnm_nei_get_chan(wpa_s, rep->regulatory_class,
				     rep->channel_number);
}


static void
fetch_drv_mbo_candidate_info(struct wpa_supplicant *wpa_s,
			     enum mbo_transition_reject_reason *reason)
{
#ifdef CONFIG_MBO
	struct wpa_bss_trans_info params;
	struct wpa_bss_candidate_info *info = NULL;
	struct neighbor_report *nei;
	u8 *pos;
	unsigned int i;

	if (!wpa_s->wnm_mbo_trans_reason_present)
		return;

	params.mbo_transition_reason = wpa_s->wnm_mbo_transition_reason;
	params.n_candidates = 0;
	params.bssid = os_calloc(wpa_s->wnm_num_neighbor_report, ETH_ALEN);
	if (!params.bssid)
		return;

	pos = params.bssid;
	for (i = 0; i < wpa_s->wnm_num_neighbor_report; i++) {
		nei = &wpa_s->wnm_neighbor_report_elements[i];

		nei->drv_mbo_reject = 0;

		if (nei->preference_present && nei->preference == 0)
			continue;

		/* Should we query BSSIDs that we reject for other reasons? */

		os_memcpy(pos, nei->bssid, ETH_ALEN);
		pos += ETH_ALEN;
		params.n_candidates++;
	}

	if (!params.n_candidates)
		goto end;

	info = wpa_drv_get_bss_trans_status(wpa_s, &params);
	if (!info)
		goto end;

	for (i = 0; i < info->num; i++) {
		int j;

		for (j = 0; j < wpa_s->wnm_num_neighbor_report; j++) {
			nei = &wpa_s->wnm_neighbor_report_elements[j];

			if (!ether_addr_equal(info->candidates[i].bssid,
					      nei->bssid))
				continue;

			nei->drv_mbo_reject = !info->candidates[i].is_accept;

			/* Use the reject reason from the first candidate */
			if (i == 0 && nei->drv_mbo_reject)
				*reason = info->candidates[i].reject_reason;

			break;
		}
	}

end:
	os_free(params.bssid);
	if (info) {
		os_free(info->candidates);
		os_free(info);
	}
#endif /* CONFIG_MBO */
}


static int wpa_bss_ies_eq(struct wpa_bss *a, struct wpa_bss *b, u8 eid)
{
	const u8 *ie_a, *ie_b;

	if (!a || !b)
		return 0;

	ie_a = wpa_bss_get_ie(a, eid);
	ie_b = wpa_bss_get_ie(b, eid);

	if (!ie_a || !ie_b || ie_a[1] != ie_b[1])
		return 0;

	return os_memcmp(ie_a, ie_b, ie_a[1]) == 0;
}


static u32 wnm_get_bss_info(struct wpa_supplicant *wpa_s, struct wpa_bss *bss)
{
	u32 info = 0;

	info |= NEI_REP_BSSID_INFO_AP_UNKNOWN_REACH;

	/*
	 * Leave the security and key scope bits unset to indicate that the
	 * security information is not available.
	 */

	if (bss->caps & WLAN_CAPABILITY_SPECTRUM_MGMT)
		info |= NEI_REP_BSSID_INFO_SPECTRUM_MGMT;
	if (bss->caps & WLAN_CAPABILITY_QOS)
		info |= NEI_REP_BSSID_INFO_QOS;
	if (bss->caps & WLAN_CAPABILITY_APSD)
		info |= NEI_REP_BSSID_INFO_APSD;
	if (bss->caps & WLAN_CAPABILITY_RADIO_MEASUREMENT)
		info |= NEI_REP_BSSID_INFO_RM;
	if (wpa_bss_ies_eq(bss, wpa_s->current_bss, WLAN_EID_MOBILITY_DOMAIN))
		info |= NEI_REP_BSSID_INFO_MOBILITY_DOMAIN;
	if (wpa_bss_ies_eq(bss, wpa_s->current_bss, WLAN_EID_HT_CAP))
		info |= NEI_REP_BSSID_INFO_HT;

	return info;
}


static int wnm_add_nei_rep(struct wpabuf **buf, const u8 *bssid,
			   u32 bss_info, u8 op_class, u8 chan, u8 phy_type,
			   u8 pref)
{
	if (wpabuf_len(*buf) + 18 >
	    IEEE80211_MAX_MMPDU_SIZE - IEEE80211_HDRLEN) {
		wpa_printf(MSG_DEBUG,
			   "WNM: No room in frame for Neighbor Report element");
		return -1;
	}

	if (wpabuf_resize(buf, 18) < 0) {
		wpa_printf(MSG_DEBUG,
			   "WNM: Failed to allocate memory for Neighbor Report element");
		return -1;
	}

	wpabuf_put_u8(*buf, WLAN_EID_NEIGHBOR_REPORT);
	/* length: 13 for basic neighbor report + 3 for preference subelement */
	wpabuf_put_u8(*buf, 16);
	wpabuf_put_data(*buf, bssid, ETH_ALEN);
	wpabuf_put_le32(*buf, bss_info);
	wpabuf_put_u8(*buf, op_class);
	wpabuf_put_u8(*buf, chan);
	wpabuf_put_u8(*buf, phy_type);
	wpabuf_put_u8(*buf, WNM_NEIGHBOR_BSS_TRANSITION_CANDIDATE);
	wpabuf_put_u8(*buf, 1);
	wpabuf_put_u8(*buf, pref);
	return 0;
}


static int wnm_nei_rep_add_bss(struct wpa_supplicant *wpa_s,
			       struct wpa_bss *bss, struct wpabuf **buf,
			       u8 pref)
{
	u8 op_class, chan;
	int sec_chan = 0, chanwidth = 0;
	struct ieee802_11_elems elems;
	struct ieee80211_ht_operation *ht_oper;
	enum phy_type phy_type;
	u32 info;

	if (ieee802_11_parse_elems(wpa_bss_ie_ptr(bss), bss->ie_len, &elems,
				   1) == ParseFailed)
		return -2;

	chanwidth = get_operation_channel_width(&elems);
	if (chanwidth == CHAN_WIDTH_UNKNOWN) {
		wpa_printf(MSG_DEBUG, "Cannot determine channel width");
		return -2;
	}
	ht_oper = (struct ieee80211_ht_operation *) elems.ht_operation;
	if (ht_oper) {
		u8 sec_chan_offset = ht_oper->ht_param &
			HT_INFO_HT_PARAM_SECONDARY_CHNL_OFF_MASK;

		if (sec_chan_offset == HT_INFO_HT_PARAM_SECONDARY_CHNL_ABOVE)
			sec_chan = 1;
		else if (sec_chan_offset ==
			 HT_INFO_HT_PARAM_SECONDARY_CHNL_BELOW)
			sec_chan = -1;
	}

	if (ieee80211_freq_to_channel_ext(bss->freq, sec_chan, chanwidth,
					  &op_class, &chan) ==
	    NUM_HOSTAPD_MODES) {
		wpa_printf(MSG_DEBUG,
			   "WNM: Cannot determine operating class and channel");
		return -2;
	}

	phy_type = ieee80211_get_phy_type(bss->freq, elems.ht_operation != NULL,
					  elems.vht_operation != NULL,
					  elems.he_operation != NULL);
	if (phy_type == PHY_TYPE_UNSPECIFIED) {
		wpa_printf(MSG_DEBUG,
			   "WNM: Cannot determine BSS phy type for Neighbor Report");
		return -2;
	}

	info = wnm_get_bss_info(wpa_s, bss);

	return wnm_add_nei_rep(buf, bss->bssid, info, op_class, chan, phy_type,
			       pref);
}


static void wnm_add_cand_list(struct wpa_supplicant *wpa_s, struct wpabuf **buf)
{
	unsigned int i, pref = 255;
	struct os_reltime now;
	struct wpa_ssid *ssid = wpa_s->current_ssid;

	if (!ssid)
		return;

	/*
	 * TODO: Define when scan results are no longer valid for the candidate
	 * list.
	 */
	os_get_reltime(&now);
	if (os_reltime_expired(&now, &wpa_s->last_scan, 10))
		return;

	wpa_printf(MSG_DEBUG,
		   "WNM: Add candidate list to BSS Transition Management Response frame");
	for (i = 0; i < wpa_s->last_scan_res_used && pref; i++) {
		struct wpa_bss *bss = wpa_s->last_scan_res[i];
		int res;

		if (wpa_scan_res_match(wpa_s, i, bss, ssid, 1, 0, false)) {
			res = wnm_nei_rep_add_bss(wpa_s, bss, buf, pref--);
			if (res == -2)
				continue; /* could not build entry for BSS */
			if (res < 0)
				break; /* no more room for candidates */
			if (pref == 1)
				break;
		}
	}

	wpa_hexdump_buf(MSG_DEBUG,
			"WNM: BSS Transition Management Response candidate list",
			*buf);
}


#define BTM_RESP_MIN_SIZE	5 + ETH_ALEN

static int wnm_send_bss_transition_mgmt_resp(
	struct wpa_supplicant *wpa_s,
	enum bss_trans_mgmt_status_code status,
	enum mbo_transition_reject_reason reason,
	u8 delay, const u8 *target_bssid)
{
	struct wpabuf *buf;
	int res;

	wpa_s->wnm_reply = 0;

	wpa_printf(MSG_DEBUG,
		   "WNM: Send BSS Transition Management Response to " MACSTR
		   " dialog_token=%u status=%u reason=%u delay=%d",
		   MAC2STR(wpa_s->bssid), wpa_s->wnm_dialog_token, status,
		   reason, delay);
	if (!wpa_s->current_bss) {
		wpa_printf(MSG_DEBUG,
			   "WNM: Current BSS not known - drop response");
		return -1;
	}

	buf = wpabuf_alloc(BTM_RESP_MIN_SIZE);
	if (!buf) {
		wpa_printf(MSG_DEBUG,
			   "WNM: Failed to allocate memory for BTM response");
		return -1;
	}

	wpa_s->bss_tm_status = status;
	wpas_notify_bss_tm_status(wpa_s);

	wpabuf_put_u8(buf, WLAN_ACTION_WNM);
	wpabuf_put_u8(buf, WNM_BSS_TRANS_MGMT_RESP);
	wpabuf_put_u8(buf, wpa_s->wnm_dialog_token);
	wpabuf_put_u8(buf, status);
	wpabuf_put_u8(buf, delay);
	if (target_bssid) {
		wpabuf_put_data(buf, target_bssid, ETH_ALEN);
	} else if (status == WNM_BSS_TM_ACCEPT) {
		/*
		 * IEEE Std 802.11-2024, 9.6.13.10 (BSS Transition Management
		 * Response frame format) clarifies that the Target BSSID field
		 * is always present when status code is zero, so use a fake
		 * value here if no BSSID is yet known.
		 */
		wpabuf_put_data(buf, "\0\0\0\0\0\0", ETH_ALEN);
	}

	if (status == WNM_BSS_TM_ACCEPT && target_bssid)
		wnm_add_cand_list(wpa_s, &buf);

#ifdef CONFIG_MBO
	if (status != WNM_BSS_TM_ACCEPT &&
	    wpa_bss_get_vendor_ie(wpa_s->current_bss, MBO_IE_VENDOR_TYPE)) {
		u8 mbo[10];
		size_t ret;

		ret = wpas_mbo_ie_bss_trans_reject(wpa_s, mbo, sizeof(mbo),
						   reason);
		if (ret) {
			if (wpabuf_resize(&buf, ret) < 0) {
				wpabuf_free(buf);
				wpa_printf(MSG_DEBUG,
					   "WNM: Failed to allocate memory for MBO IE");
				return -1;
			}

			wpabuf_put_data(buf, mbo, ret);
		}
	}
#endif /* CONFIG_MBO */

	res = wpa_drv_send_action(wpa_s, wpa_s->assoc_freq, 0, wpa_s->bssid,
				  wpa_s->own_addr, wpa_s->bssid,
				  wpabuf_head_u8(buf), wpabuf_len(buf), 0);
	if (res < 0) {
		wpa_printf(MSG_DEBUG,
			   "WNM: Failed to send BSS Transition Management Response");
	}

	wpabuf_free(buf);

	return res;
}


static void wnm_bss_tm_connect(struct wpa_supplicant *wpa_s,
			       struct wpa_bss *bss, struct wpa_ssid *ssid,
			       int after_new_scan)
{
	struct wpa_radio_work *already_connecting;

	wpa_dbg(wpa_s, MSG_DEBUG,
		"WNM: Transition to BSS " MACSTR
		" based on BSS Transition Management Request (old BSSID "
		MACSTR " after_new_scan=%d)",
		MAC2STR(bss->bssid), MAC2STR(wpa_s->bssid), after_new_scan);

	/* Send the BSS Management Response - Accept */
	if (wpa_s->wnm_reply) {
		wpa_s->wnm_target_bss = bss;
		wpa_printf(MSG_DEBUG,
			   "WNM: Sending successful BSS Transition Management Response");

		/* This function will be called again from the TX handler to
		 * start the actual reassociation after this response has been
		 * delivered to the current AP. */
		if (wnm_send_bss_transition_mgmt_resp(
			    wpa_s, WNM_BSS_TM_ACCEPT,
			    MBO_TRANSITION_REJECT_REASON_UNSPECIFIED, 0,
			    bss->bssid) >= 0)
			return;
	}

	if (bss == wpa_s->current_bss) {
		wpa_printf(MSG_DEBUG,
			   "WNM: Already associated with the preferred candidate");
		wnm_btm_reset(wpa_s);
		return;
	}

	already_connecting = radio_work_pending(wpa_s, "sme-connect");
	wpa_s->reassociate = 1;
	wpa_printf(MSG_DEBUG, "WNM: Issuing connect");
	wpa_supplicant_connect(wpa_s, bss, ssid);

	/*
	 * Indicate that a BSS transition is in progress so scan results that
	 * come in before the 'sme-connect' radio work gets executed do not
	 * override the original connection attempt.
	 */
	if (!already_connecting && radio_work_pending(wpa_s, "sme-connect"))
		wpa_s->bss_trans_mgmt_in_progress = true;
}


int wnm_scan_process(struct wpa_supplicant *wpa_s, bool pre_scan_check)
{
	struct wpa_bss *bss, *current_bss = wpa_s->current_bss;
	struct wpa_ssid *ssid = wpa_s->current_ssid;
	enum bss_trans_mgmt_status_code status = WNM_BSS_TM_REJECT_UNSPECIFIED;
	enum mbo_transition_reject_reason reason =
		MBO_TRANSITION_REJECT_REASON_UNSPECIFIED;
	struct wpa_ssid *selected_ssid = NULL;

	if (!ssid || !wpa_s->wnm_dialog_token)
		return 0;

	wpa_dbg(wpa_s, MSG_DEBUG,
		"WNM: Process scan results for BSS Transition Management");
	if (!pre_scan_check &&
	    os_reltime_initialized(&wpa_s->wnm_cand_valid_until) &&
	    os_reltime_before(&wpa_s->wnm_cand_valid_until,
			      &wpa_s->scan_trigger_time)) {
		wpa_printf(MSG_DEBUG, "WNM: Previously stored BSS transition candidate list is not valid anymore - drop it");
		goto send_bss_resp_fail;
	}

	if (!pre_scan_check && !wpa_s->wnm_transition_scan)
		return 0;

	wpa_s->wnm_transition_scan = false;

	/* Fetch MBO transition candidate rejection information from driver */
	fetch_drv_mbo_candidate_info(wpa_s, &reason);

	/* Compare the Neighbor Report and scan results */
	bss = wpa_supplicant_select_bss(wpa_s, ssid, &selected_ssid, 1);
#ifdef CONFIG_MBO
	if (!bss && wpa_s->wnm_mbo_trans_reason_present &&
	    (wpa_s->wnm_mode & WNM_BSS_TM_REQ_DISASSOC_IMMINENT)) {
		int i;
		bool changed = false;

		/*
		 * We didn't find any candidate, the driver had a say about
		 * which targets to reject and disassociation is immiment.
		 *
		 * We should still try to roam, so retry after ignoring the
		 * driver reject for any BSS that has an RSSI better than
		 * disassoc_imminent_rssi_threshold.
		 */
		for (i = 0; i < wpa_s->wnm_num_neighbor_report; i++) {
			struct neighbor_report *nei;

			nei = &wpa_s->wnm_neighbor_report_elements[i];
			bss = wpa_bss_get_bssid(wpa_s, nei->bssid);
			if (bss && bss->level >
			    wpa_s->conf->disassoc_imminent_rssi_threshold) {
				nei->drv_mbo_reject = 0;
				changed = true;
			}
		}

		if (changed) {
			wpa_printf(MSG_DEBUG,
				   "WNM: Ignore driver rejection due to imminent disassociation and acceptable RSSI");
			bss = wpa_supplicant_select_bss(wpa_s, ssid,
							&selected_ssid, 1);
		}
	}
#endif /* CONFIG_MBO */

	/*
	 * If this is a pre-scan check, returning 0 will trigger a scan and
	 * another call. In that case, reject "bad" candidates in the hope of
	 * finding a better candidate after scanning.
	 *
	 * Use a simple heuristic to check whether the selection is reasonable
	 * or a scan is a good idea. For that, we need to have found a
	 * candidate BSS (which might be the current one), it is up-to-date,
	 * and we don't want to immediately roam back again.
	 */
	if (pre_scan_check) {
		struct os_reltime age;

		if (!bss)
			return 0;

		os_reltime_age(&bss->last_update, &age);
		if (age.sec >= 10)
			return 0;

#ifndef CONFIG_NO_ROAMING
		if (current_bss && bss != current_bss &&
		    wpa_supplicant_need_to_roam_within_ess(wpa_s, bss,
							   current_bss, false))
			return 0;
#endif /* CONFIG_NO_ROAMING */
	}

#ifndef CONFIG_NO_ROAMING
	/* Apply normal roaming rules if we can stay with the current BSS */
	if (current_bss && bss != current_bss &&
	    wpa_scan_res_match(wpa_s, 0, current_bss, wpa_s->current_ssid,
			       1, 0, false) &&
	    !wpa_supplicant_need_to_roam_within_ess(wpa_s, current_bss, bss,
						    true))
		bss = current_bss;
#endif /* CONFIG_NO_ROAMING */

	if (!bss) {
		wpa_printf(MSG_DEBUG, "WNM: No BSS transition candidate match found");
		status = WNM_BSS_TM_REJECT_NO_SUITABLE_CANDIDATES;
		goto send_bss_resp_fail;
	}

	wpa_printf(MSG_DEBUG,
		   "WNM: Found an acceptable preferred transition candidate BSS "
		   MACSTR " (RSSI %d, tput: %d  bss-tput: %d)",
		   MAC2STR(bss->bssid), bss->level, bss->est_throughput,
		   current_bss ? (int) current_bss->est_throughput : -1);

	/* Associate to the network */
	wnm_bss_tm_connect(wpa_s, bss, ssid, 1);
	return 1;

send_bss_resp_fail:
	if (wpa_s->wnm_reply) {
		/* If disassoc imminent is set, we must not reject */
		if (wpa_s->wnm_mode &
		    (WNM_BSS_TM_REQ_DISASSOC_IMMINENT |
		     WNM_BSS_TM_REQ_ESS_DISASSOC_IMMINENT)) {
			wpa_printf(MSG_DEBUG,
				   "WNM: Accept BTM request because disassociation imminent bit is set");
			status = WNM_BSS_TM_ACCEPT;
		}

		wnm_send_bss_transition_mgmt_resp(wpa_s, status, reason,
						  0, NULL);
	}

	wnm_btm_reset(wpa_s);

	return 1;
}


static int cand_pref_compar(const void *a, const void *b)
{
	const struct neighbor_report *aa = a;
	const struct neighbor_report *bb = b;

	if (aa->disassoc_imminent && !bb->disassoc_imminent)
		return 1;
	if (bb->disassoc_imminent && !aa->disassoc_imminent)
		return -1;

	if (!aa->preference_present && !bb->preference_present)
		return 0;
	if (!aa->preference_present)
		return 1;
	if (!bb->preference_present)
		return -1;
	if (bb->preference > aa->preference)
		return 1;
	if (bb->preference < aa->preference)
		return -1;
	return 0;
}


static void wnm_sort_cand_list(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s->wnm_neighbor_report_elements)
		return;
	qsort(wpa_s->wnm_neighbor_report_elements,
	      wpa_s->wnm_num_neighbor_report, sizeof(struct neighbor_report),
	      cand_pref_compar);
}


static void wnm_dump_cand_list(struct wpa_supplicant *wpa_s)
{
	unsigned int i;

	wpa_printf(MSG_DEBUG, "WNM: BSS Transition Candidate List");
	if (!wpa_s->wnm_neighbor_report_elements)
		return;
	for (i = 0; i < wpa_s->wnm_num_neighbor_report; i++) {
		struct neighbor_report *nei;
		char mld_info[42] = "";

		nei = &wpa_s->wnm_neighbor_report_elements[i];

		if (!is_zero_ether_addr(nei->mld_addr))
			os_snprintf(mld_info, sizeof(mld_info) - 1,
				    " mld_addr=" MACSTR " links=0x%02x",
				    MAC2STR(nei->mld_addr), nei->mld_links);

		wpa_printf(MSG_DEBUG, "%u: " MACSTR
			   " info=0x%x op_class=%u chan=%u phy=%u pref=%d freq=%d%s",
			   i, MAC2STR(nei->bssid), nei->bssid_info,
			   nei->regulatory_class,
			   nei->channel_number, nei->phy_type,
			   nei->preference_present ? nei->preference : -1,
			   nei->freq, mld_info);
	}
}


static int chan_supported(struct wpa_supplicant *wpa_s, int freq)
{
	unsigned int i;

	for (i = 0; i < wpa_s->hw.num_modes; i++) {
		struct hostapd_hw_modes *mode = &wpa_s->hw.modes[i];
		int j;

		for (j = 0; j < mode->num_channels; j++) {
			struct hostapd_channel_data *chan;

			chan = &mode->channels[j];
			if (chan->freq == freq &&
			    !(chan->flag & HOSTAPD_CHAN_DISABLED))
				return 1;
		}
	}

	return 0;
}


static void wnm_set_scan_freqs(struct wpa_supplicant *wpa_s)
{
	unsigned int i;
	bool has_6ghz = false;

	if (!wpa_s->wnm_neighbor_report_elements)
		return;

	if (wpa_s->hw.modes == NULL)
		return;

	os_free(wpa_s->next_scan_freqs);
	wpa_s->next_scan_freqs = NULL;

	for (i = 0; i < wpa_s->wnm_num_neighbor_report; i++) {
		struct neighbor_report *nei;

		nei = &wpa_s->wnm_neighbor_report_elements[i];

		if (nei->preference_present && nei->preference == 0)
			continue;

		if (nei->freq <= 0) {
			wpa_printf(MSG_DEBUG,
				   "WNM: Unknown neighbor operating frequency for "
				   MACSTR " - scan all channels",
				   MAC2STR(nei->bssid));
			os_free(wpa_s->next_scan_freqs);
			wpa_s->next_scan_freqs = NULL;
			return;
		}
		if (chan_supported(wpa_s, nei->freq))
			int_array_add_unique(&wpa_s->next_scan_freqs,
					     nei->freq);
		has_6ghz |= is_6ghz_freq(nei->freq);
	}

	if (!wpa_s->next_scan_freqs)
		return;

	wpa_printf(MSG_DEBUG,
		   "WNM: Scan %zu frequencies based on transition candidate list",
		   int_array_len(wpa_s->next_scan_freqs));

	/*
	 * Candidates on 6 GHz channels might be collocated ones, and thus, in
	 * order to discover them need to include the frequencies on the 2.4
	 * and 5 GHz bands. Since the scan time can be long, optimize the case
	 * of a single channel by forcing passive scan instead of doing a
	 * collocated scan.
	 */
	if (has_6ghz) {
		struct hostapd_channel_data *chan;
		struct os_reltime now;

		/* In case the candidate validity time is too short, force it
		 * to be long enough to account for the longer scan time.
		 */
		os_get_reltime(&now);
		now.sec += 5;

		if (os_reltime_initialized(&wpa_s->wnm_cand_valid_until) &&
		    !os_reltime_before(&wpa_s->wnm_cand_valid_until, &now)) {
			wpa_printf(MSG_DEBUG,
				   "WNM: Scan: 6 GHz: update validity time");

			os_memcpy(&wpa_s->wnm_cand_valid_until, &now,
				  sizeof(now));
		}

		if (int_array_len(wpa_s->next_scan_freqs) == 1) {
			wpa_printf(MSG_DEBUG,
				   "WNM: Scan: Single 6 GHz channel: passive");

			wpa_s->scan_req = MANUAL_SCAN_REQ;
			wpa_s->manual_scan_passive = 1;
			return;
		}

		wpa_printf(MSG_DEBUG,
			   "WNM: Scan: Add 2.4/5 GHz channels as well for 6 GHz discovery");

		for (i = 0; i < wpa_s->hw.num_modes; i++) {
			struct hostapd_hw_modes *mode = &wpa_s->hw.modes[i];
			int j;

			/* skip all the irrelevant modes */
			if ((mode->mode != HOSTAPD_MODE_IEEE80211B &&
			     mode->mode != HOSTAPD_MODE_IEEE80211G &&
			     mode->mode != HOSTAPD_MODE_IEEE80211A) ||
			    mode->is_6ghz)
				continue;

			for (j = 0; j < mode->num_channels; j++) {
				chan = &mode->channels[j];
				if (chan->flag & HOSTAPD_CHAN_DISABLED)
					continue;

				int_array_add_unique(&wpa_s->next_scan_freqs,
						     chan->freq);
			}
		}

		wpa_printf(MSG_DEBUG,
			   "WNM: Scan %zu frequencies (including collocated)",
			   int_array_len(wpa_s->next_scan_freqs));
	}
}


static int wnm_parse_candidate_list(struct wpa_supplicant *wpa_s,
				    const u8 *pos, const u8 *end,
				    int *num_valid_candidates)
{
	*num_valid_candidates = 0;

	while (end - pos >= 2 &&
	       wpa_s->wnm_num_neighbor_report < WNM_MAX_NEIGHBOR_REPORT) {
		u8 tag = *pos++;
		u8 len = *pos++;

		wpa_printf(MSG_DEBUG, "WNM: Neighbor report tag %u", tag);
		if (len > end - pos) {
			wpa_printf(MSG_DEBUG, "WNM: Truncated request");
			return -1;
		}
		if (tag == WLAN_EID_NEIGHBOR_REPORT) {
			struct neighbor_report *rep;

			if (!wpa_s->wnm_num_neighbor_report) {
				wpa_s->wnm_neighbor_report_elements = os_calloc(
					WNM_MAX_NEIGHBOR_REPORT,
					sizeof(struct neighbor_report));
				if (!wpa_s->wnm_neighbor_report_elements)
					return -1;
			}

			rep = &wpa_s->wnm_neighbor_report_elements[
				wpa_s->wnm_num_neighbor_report];
			wnm_parse_neighbor_report(wpa_s, pos, len, rep);
			if ((wpa_s->wnm_mode &
			     WNM_BSS_TM_REQ_DISASSOC_IMMINENT) &&
			    ether_addr_equal(rep->bssid, wpa_s->bssid))
				rep->disassoc_imminent = 1;

			if (rep->preference_present && rep->preference)
				*num_valid_candidates += 1;

			wpa_s->wnm_num_neighbor_report++;
		}

		pos += len;
	}

	return 0;
}


static void ieee802_11_rx_bss_trans_mgmt_req(struct wpa_supplicant *wpa_s,
					     const u8 *pos, const u8 *end,
					     int reply)
{
	unsigned int beacon_int;
	u8 valid_int;
#ifdef CONFIG_MBO
	const u8 *vendor;
#endif /* CONFIG_MBO */
	bool disassoc_imminent;
	int num_valid_candidates;

	if (wpa_s->disable_mbo_oce || wpa_s->conf->disable_btm)
		return;

	if (end - pos < 5)
		return;

	if (wpa_s->current_bss)
		beacon_int = wpa_s->current_bss->beacon_int;
	else
		beacon_int = 100; /* best guess */

	wnm_btm_reset(wpa_s);

	wpa_s->wnm_dialog_token = pos[0];
	wpa_s->wnm_mode = pos[1];
	wpa_s->wnm_disassoc_timer = WPA_GET_LE16(pos + 2);
	wpa_s->wnm_link_removal = false;
	valid_int = pos[4];
	wpa_s->wnm_reply = reply;

	wpa_printf(MSG_DEBUG, "WNM: BSS Transition Management Request: "
		   "dialog_token=%u request_mode=0x%x "
		   "disassoc_timer=%u validity_interval=%u",
		   wpa_s->wnm_dialog_token, wpa_s->wnm_mode,
		   wpa_s->wnm_disassoc_timer, valid_int);

	if (!wpa_s->wnm_dialog_token) {
		wpa_printf(MSG_DEBUG, "WNM: Invalid dialog token");
		goto reset;
	}

#if defined(CONFIG_MBO) && defined(CONFIG_TESTING_OPTIONS)
	if (wpa_s->reject_btm_req_reason) {
		wpa_printf(MSG_INFO,
			   "WNM: Testing - reject BSS Transition Management Request: reject_btm_req_reason=%d",
			   wpa_s->reject_btm_req_reason);
		wnm_send_bss_transition_mgmt_resp(
			wpa_s, wpa_s->reject_btm_req_reason,
			MBO_TRANSITION_REJECT_REASON_UNSPECIFIED, 0, NULL);
		goto reset;
	}
#endif /* CONFIG_MBO && CONFIG_TESTING_OPTIONS */

	pos += 5;

	if (wpa_s->wnm_mode & WNM_BSS_TM_REQ_BSS_TERMINATION_INCLUDED) {
		if (end - pos < 12) {
			wpa_printf(MSG_DEBUG, "WNM: Too short BSS TM Request");
			goto reset;
		}
		os_memcpy(wpa_s->wnm_bss_termination_duration, pos, 12);
		pos += 12; /* BSS Termination Duration */
	}

	if (wpa_s->wnm_mode & WNM_BSS_TM_REQ_ESS_DISASSOC_IMMINENT) {
		char url[256];
		u8 url_len;

		if (end - pos < 1) {
			wpa_printf(MSG_DEBUG, "WNM: Invalid BSS Transition "
				   "Management Request (URL)");
			goto reset;
		}
		url_len = *pos++;
		if (url_len > end - pos) {
			wpa_printf(MSG_DEBUG,
				   "WNM: Invalid BSS Transition Management Request (URL truncated)");
			goto reset;
		}
		os_memcpy(url, pos, url_len);
		url[url_len] = '\0';
		pos += url_len;

		wpa_msg(wpa_s, MSG_INFO, ESS_DISASSOC_IMMINENT "%d %u %s",
			wpa_sm_pmf_enabled(wpa_s->wpa),
			wpa_s->wnm_disassoc_timer * beacon_int * 128 / 125,
			url);
	}

	disassoc_imminent = wpa_s->wnm_mode & WNM_BSS_TM_REQ_DISASSOC_IMMINENT;

	/*
	 * Based on IEEE Std 802.11be-2024, Table 9-538a (BSS Termination
	 * Included and Link Removal Imminent fields encoding), when a station
	 * is a non-AP MLD with more than one affiliated link, the Link Removal
	 * Imminent field is set to 1, and the BSS Termination Included field
	 * is set to 1, only one of the links is removed and the other links
	 * remain associated. Ignore the Disassociation Imminent field in such
	 * a case.
	 *
	 * TODO: We should check if the AP has more than one link.
	 * TODO: We should pass the RX link and use that
	 */
	if (disassoc_imminent && wpa_s->valid_links &&
	    (wpa_s->wnm_mode & WNM_BSS_TM_REQ_LINK_REMOVAL_IMMINENT) &&
	    (wpa_s->wnm_mode & WNM_BSS_TM_REQ_BSS_TERMINATION_INCLUDED)) {
		/* If we still have a link, then just accept the request */
		if (wpa_s->valid_links & (wpa_s->valid_links - 1)) {
			wpa_printf(MSG_INFO,
				   "WNM: BTM request for a single MLO link - ignore disassociation imminent since other links remain associated");
			disassoc_imminent = false;

			wnm_send_bss_transition_mgmt_resp(
				wpa_s, WNM_BSS_TM_ACCEPT, 0, 0, NULL);

			goto reset;
		}

		/* The last link is being removed (which must be the assoc link)
		 */
		wpa_s->wnm_link_removal = true;
		wpa_s->wnm_disassoc_mld = false;
		os_memcpy(wpa_s->wnm_disassoc_addr,
			  wpa_s->links[wpa_s->mlo_assoc_link_id].bssid,
			  ETH_ALEN);
	} else if (wpa_s->valid_links) {
		wpa_s->wnm_disassoc_mld = true;
		os_memcpy(wpa_s->wnm_disassoc_addr, wpa_s->ap_mld_addr,
			  ETH_ALEN);
	} else {
		wpa_s->wnm_disassoc_mld = false;
		os_memcpy(wpa_s->wnm_disassoc_addr, wpa_s->bssid, ETH_ALEN);
	}

	if (disassoc_imminent)
		wpa_msg(wpa_s, MSG_INFO, "WNM: Disassociation Imminent - "
			"Disassociation Timer %u", wpa_s->wnm_disassoc_timer);

#ifdef CONFIG_MBO
	vendor = get_ie(pos, end - pos, WLAN_EID_VENDOR_SPECIFIC);
	if (vendor)
		wpas_mbo_ie_trans_req(wpa_s, vendor + 2, vendor[1]);
#endif /* CONFIG_MBO */

	if (wnm_parse_candidate_list(wpa_s, pos, end,
				     &num_valid_candidates) < 0)
		goto reset;

	if (wpa_s->wnm_mode & WNM_BSS_TM_REQ_PREF_CAND_LIST_INCLUDED) {
		if (!wpa_s->wnm_num_neighbor_report) {
			wpa_printf(MSG_DEBUG,
				   "WNM: Candidate list included bit is set, but no candidates found");
			wnm_send_bss_transition_mgmt_resp(
				wpa_s, WNM_BSS_TM_REJECT_NO_SUITABLE_CANDIDATES,
				MBO_TRANSITION_REJECT_REASON_UNSPECIFIED, 0,
				NULL);
			goto reset;
		}
		wpa_msg(wpa_s, MSG_INFO, "WNM: Preferred List Available");
	}

	if (wpa_s->wnm_num_neighbor_report) {
		unsigned int valid_ms;

		wnm_sort_cand_list(wpa_s);
		wnm_dump_cand_list(wpa_s);
		valid_ms = valid_int * beacon_int * 128 / 125;
		wpa_printf(MSG_DEBUG, "WNM: Candidate list valid for %u ms",
			   valid_ms);
		os_get_reltime(&wpa_s->wnm_cand_valid_until);
		os_reltime_add_ms(&wpa_s->wnm_cand_valid_until, valid_ms);
	} else if (!disassoc_imminent) {
		enum bss_trans_mgmt_status_code status;

		/* No candidate list and disassociation is not imminent */

		if ((wpa_s->wnm_mode & WNM_BSS_TM_REQ_ESS_DISASSOC_IMMINENT) ||
		    wpa_s->wnm_link_removal)
			status = WNM_BSS_TM_ACCEPT;
		else {
			wpa_msg(wpa_s, MSG_INFO, "WNM: BSS Transition Management Request did not include candidates");
			status = WNM_BSS_TM_REJECT_UNSPECIFIED;
		}

		if (reply)
			wnm_send_bss_transition_mgmt_resp(
				wpa_s, status,
				MBO_TRANSITION_REJECT_REASON_UNSPECIFIED, 0,
				NULL);

		goto reset;
	}

	/*
	 * Try fetching the latest scan results from the kernel.
	 * This can help in finding more up-to-date information should
	 * the driver have done some internal scanning operations after
	 * the last scan result update in wpa_supplicant.
	 *
	 * It is not a new scan, this does not update the last_scan
	 * timestamp nor will it expire old BSSs.
	 */
	wpa_supplicant_update_scan_results(wpa_s, NULL);
	if (wnm_scan_process(wpa_s, true) > 0)
		return;
	wpa_printf(MSG_DEBUG,
		   "WNM: No valid match in previous scan results - try a new scan");

	/*
	 * If we have a fixed BSSID configured, just reject at this point.
	 * NOTE: We could actually check if we are allowed to stay (and we do
	 * above if we have scan results available).
	 */
	if (wpa_s->current_ssid && wpa_s->current_ssid->bssid_set) {
		wpa_printf(MSG_DEBUG, "WNM: Fixed BSSID, rejecting request");

		if (reply)
			wnm_send_bss_transition_mgmt_resp(
				wpa_s, WNM_BSS_TM_REJECT_NO_SUITABLE_CANDIDATES,
				MBO_TRANSITION_REJECT_REASON_UNSPECIFIED, 0,
				NULL);

		goto reset;
	}

	wnm_set_scan_freqs(wpa_s);
	if (num_valid_candidates == 1) {
		/* Any invalid candidate was sorted to the end */
		os_memcpy(wpa_s->next_scan_bssid,
			  wpa_s->wnm_neighbor_report_elements[0].bssid,
			  ETH_ALEN);
		wpa_printf(MSG_DEBUG,
			  "WNM: Scan only for a specific BSSID since there is only a single candidate "
			  MACSTR, MAC2STR(wpa_s->next_scan_bssid));
	}
	wpa_s->wnm_transition_scan = true;
	wpa_supplicant_req_scan(wpa_s, 0, 0);

	/* Continue from scan handler */
	return;

reset:
	wnm_btm_reset(wpa_s);
}


int wnm_btm_resp_tx_status(struct wpa_supplicant *wpa_s, const u8 *data,
			   size_t data_len)
{
	const struct ieee80211_mgmt *frame =
		(const struct ieee80211_mgmt *) data;

	if (data_len <
	    IEEE80211_HDRLEN + sizeof(frame->u.action.u.bss_tm_resp) ||
	    frame->u.action.category != WLAN_ACTION_WNM ||
	    frame->u.action.u.bss_tm_resp.action != WNM_BSS_TRANS_MGMT_RESP ||
	    frame->u.action.u.bss_tm_resp.status_code != WNM_BSS_TM_ACCEPT)
		return -1;

	/*
	 * If disassoc imminent bit was set in the request, the response may
	 * indicate accept even if no candidate was found, so bail out here.
	 */
	if (!wpa_s->wnm_target_bss) {
		wpa_printf(MSG_DEBUG, "WNM: Target BSS is not set");
		return 0;
	}

	if (!wpa_s->current_ssid)
		return 0;

	wnm_bss_tm_connect(wpa_s, wpa_s->wnm_target_bss, wpa_s->current_ssid,
			   0);

	wpa_s->wnm_target_bss = NULL;
	return 0;
}


#define BTM_QUERY_MIN_SIZE	4

int wnm_send_bss_transition_mgmt_query(struct wpa_supplicant *wpa_s,
				       u8 query_reason,
				       const char *btm_candidates,
				       int cand_list)
{
	struct wpabuf *buf;
	int ret;

	wpa_printf(MSG_DEBUG, "WNM: Send BSS Transition Management Query to "
		   MACSTR " query_reason=%u%s",
		   MAC2STR(wpa_s->bssid), query_reason,
		   cand_list ? " candidate list" : "");

	buf = wpabuf_alloc(BTM_QUERY_MIN_SIZE);
	if (!buf)
		return -1;

	wpabuf_put_u8(buf, WLAN_ACTION_WNM);
	wpabuf_put_u8(buf, WNM_BSS_TRANS_MGMT_QUERY);
	wpabuf_put_u8(buf, 1);
	wpabuf_put_u8(buf, query_reason);

	if (cand_list)
		wnm_add_cand_list(wpa_s, &buf);

	if (btm_candidates) {
		const size_t max_len = 1000;

		ret = wpabuf_resize(&buf, max_len);
		if (ret < 0) {
			wpabuf_free(buf);
			return ret;
		}

		ret = ieee802_11_parse_candidate_list(btm_candidates,
						      wpabuf_put(buf, 0),
						      max_len);
		if (ret < 0) {
			wpabuf_free(buf);
			return ret;
		}

		wpabuf_put(buf, ret);
	}

	ret = wpa_drv_send_action(wpa_s, wpa_s->assoc_freq, 0, wpa_s->bssid,
				  wpa_s->own_addr, wpa_s->bssid,
				  wpabuf_head_u8(buf), wpabuf_len(buf), 0);

	wpabuf_free(buf);
	return ret;
}


static void ieee802_11_rx_wnm_notif_req_wfa(struct wpa_supplicant *wpa_s,
					    const u8 *sa, const u8 *data,
					    int len)
{
	const u8 *pos, *end, *next;
	u8 ie, ie_len;

	pos = data;
	end = data + len;

	while (end - pos > 1) {
		ie = *pos++;
		ie_len = *pos++;
		wpa_printf(MSG_DEBUG, "WNM: WFA subelement %u len %u",
			   ie, ie_len);
		if (ie_len > end - pos) {
			wpa_printf(MSG_DEBUG, "WNM: Not enough room for "
				   "subelement");
			break;
		}
		next = pos + ie_len;
		if (ie_len < 4) {
			pos = next;
			continue;
		}
		wpa_printf(MSG_DEBUG, "WNM: Subelement OUI %06x type %u",
			   WPA_GET_BE24(pos), pos[3]);

#ifdef CONFIG_HS20
		if (ie == WLAN_EID_VENDOR_SPECIFIC && ie_len >= 8 &&
		    WPA_GET_BE24(pos) == OUI_WFA &&
		    pos[3] == HS20_WNM_DEAUTH_IMMINENT_NOTICE) {
			const u8 *ie_end;
			u8 url_len;
			char *url;
			u8 code;
			u16 reauth_delay;

			ie_end = pos + ie_len;
			pos += 4;
			code = *pos++;
			reauth_delay = WPA_GET_LE16(pos);
			pos += 2;
			url_len = *pos++;
			wpa_printf(MSG_DEBUG, "WNM: HS 2.0 Deauthentication "
				   "Imminent - Reason Code %u   "
				   "Re-Auth Delay %u  URL Length %u",
				   code, reauth_delay, url_len);
			if (url_len > ie_end - pos)
				break;
			url = os_malloc(url_len + 1);
			if (url == NULL)
				break;
			os_memcpy(url, pos, url_len);
			url[url_len] = '\0';
			hs20_rx_deauth_imminent_notice(wpa_s, code,
						       reauth_delay, url);
			os_free(url);
			pos = next;
			continue;
		}

		if (ie == WLAN_EID_VENDOR_SPECIFIC && ie_len >= 5 &&
		    WPA_GET_BE24(pos) == OUI_WFA &&
		    pos[3] == HS20_WNM_T_C_ACCEPTANCE) {
			const u8 *ie_end;
			u8 url_len;
			char *url;

			ie_end = pos + ie_len;
			pos += 4;
			url_len = *pos++;
			wpa_printf(MSG_DEBUG,
				   "WNM: HS 2.0 Terms and Conditions Acceptance (URL Length %u)",
				   url_len);
			if (url_len > ie_end - pos)
				break;
			url = os_malloc(url_len + 1);
			if (!url)
				break;
			os_memcpy(url, pos, url_len);
			url[url_len] = '\0';
			hs20_rx_t_c_acceptance(wpa_s, url);
			os_free(url);
			pos = next;
			continue;
		}
#endif /* CONFIG_HS20 */

		pos = next;
	}
}


static void ieee802_11_rx_wnm_notif_req(struct wpa_supplicant *wpa_s,
					const u8 *da, const u8 *sa,
					const u8 *frm, int len)
{
	const u8 *pos, *end;
	u8 dialog_token, type;

	if (is_multicast_ether_addr(da)) {
		wpa_printf(MSG_DEBUG,
			   "WNM: Ignore group-addressed WNM Notification Request frame (A1="
			   MACSTR " A2=" MACSTR ")",
			   MAC2STR(da), MAC2STR(sa));
		return;
	}

	/* Dialog Token [1] | Type [1] | Subelements */

	if (len < 2 || sa == NULL)
		return;
	end = frm + len;
	pos = frm;
	dialog_token = *pos++;
	type = *pos++;

	wpa_dbg(wpa_s, MSG_DEBUG, "WNM: Received WNM-Notification Request "
		"(dialog_token %u type %u sa " MACSTR ")",
		dialog_token, type, MAC2STR(sa));
	wpa_hexdump(MSG_DEBUG, "WNM-Notification Request subelements",
		    pos, end - pos);

	if (wpa_s->wpa_state != WPA_COMPLETED ||
	    (!ether_addr_equal(sa, wpa_s->bssid) &&
	     (!wpa_s->valid_links ||
	      !ether_addr_equal(sa, wpa_s->ap_mld_addr)))) {
		wpa_dbg(wpa_s, MSG_DEBUG, "WNM: WNM-Notification frame not "
			"from our AP - ignore it");
		return;
	}

	switch (type) {
	case 1:
		ieee802_11_rx_wnm_notif_req_wfa(wpa_s, sa, pos, end - pos);
		break;
	default:
		wpa_dbg(wpa_s, MSG_DEBUG, "WNM: Ignore unknown "
			"WNM-Notification type %u", type);
		break;
	}
}


static void ieee802_11_rx_wnm_coloc_intf_req(struct wpa_supplicant *wpa_s,
					     const u8 *sa, const u8 *frm,
					     int len)
{
	u8 dialog_token, req_info, auto_report, timeout;

	if (!wpa_s->conf->coloc_intf_reporting)
		return;

	/* Dialog Token [1] | Request Info [1] */

	if (len < 2)
		return;
	dialog_token = frm[0];
	req_info = frm[1];
	auto_report = req_info & 0x03;
	timeout = req_info >> 2;

	wpa_dbg(wpa_s, MSG_DEBUG,
		"WNM: Received Collocated Interference Request (dialog_token %u auto_report %u timeout %u sa " MACSTR ")",
		dialog_token, auto_report, timeout, MAC2STR(sa));

	if (dialog_token == 0)
		return; /* only nonzero values are used for request */

	if (wpa_s->wpa_state != WPA_COMPLETED ||
	    (!ether_addr_equal(sa, wpa_s->bssid) &&
	     (!wpa_s->valid_links ||
	      !ether_addr_equal(sa, wpa_s->ap_mld_addr)))) {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"WNM: Collocated Interference Request frame not from current AP - ignore it");
		return;
	}

	wpa_msg(wpa_s, MSG_INFO, COLOC_INTF_REQ "%u %u %u",
		dialog_token, auto_report, timeout);
	wpa_s->coloc_intf_dialog_token = dialog_token;
	wpa_s->coloc_intf_auto_report = auto_report;
	wpa_s->coloc_intf_timeout = timeout;
}


void ieee802_11_rx_wnm_action(struct wpa_supplicant *wpa_s,
			      const struct ieee80211_mgmt *mgmt, size_t len)
{
	const u8 *pos, *end;
	u8 act;

	if (len < IEEE80211_HDRLEN + 2)
		return;

	pos = ((const u8 *) mgmt) + IEEE80211_HDRLEN + 1;
	act = *pos++;
	end = ((const u8 *) mgmt) + len;

	wpa_printf(MSG_DEBUG, "WNM: RX action %u from " MACSTR,
		   act, MAC2STR(mgmt->sa));
	if (wpa_s->wpa_state < WPA_ASSOCIATED ||
	    (!ether_addr_equal(mgmt->sa, wpa_s->bssid) &&
	     (!wpa_s->valid_links ||
	      !ether_addr_equal(mgmt->sa, wpa_s->ap_mld_addr)))) {
		wpa_printf(MSG_DEBUG, "WNM: Ignore unexpected WNM Action "
			   "frame");
		return;
	}

	switch (act) {
	case WNM_BSS_TRANS_MGMT_REQ:
		ieee802_11_rx_bss_trans_mgmt_req(wpa_s, pos, end,
						 !(mgmt->da[0] & 0x01));
		break;
	case WNM_SLEEP_MODE_RESP:
		ieee802_11_rx_wnmsleep_resp(wpa_s, mgmt->da, mgmt->sa,
					    pos, end - pos);
		break;
	case WNM_NOTIFICATION_REQ:
		ieee802_11_rx_wnm_notif_req(wpa_s, mgmt->da, mgmt->sa,
					    pos, end - pos);
		break;
	case WNM_COLLOCATED_INTERFERENCE_REQ:
		ieee802_11_rx_wnm_coloc_intf_req(wpa_s, mgmt->sa, pos,
						 end - pos);
		break;
	default:
		wpa_printf(MSG_ERROR, "WNM: Unknown request");
		break;
	}
}


int wnm_send_coloc_intf_report(struct wpa_supplicant *wpa_s, u8 dialog_token,
			       const struct wpabuf *elems)
{
	struct wpabuf *buf;
	int ret;

	if (wpa_s->wpa_state < WPA_ASSOCIATED || !elems)
		return -1;

	wpa_printf(MSG_DEBUG, "WNM: Send Collocated Interference Report to "
		   MACSTR " (dialog token %u)",
		   MAC2STR(wpa_s->bssid), dialog_token);

	buf = wpabuf_alloc(3 + wpabuf_len(elems));
	if (!buf)
		return -1;

	wpabuf_put_u8(buf, WLAN_ACTION_WNM);
	wpabuf_put_u8(buf, WNM_COLLOCATED_INTERFERENCE_REPORT);
	wpabuf_put_u8(buf, dialog_token);
	wpabuf_put_buf(buf, elems);

	ret = wpa_drv_send_action(wpa_s, wpa_s->assoc_freq, 0, wpa_s->bssid,
				  wpa_s->own_addr, wpa_s->bssid,
				  wpabuf_head_u8(buf), wpabuf_len(buf), 0);
	wpabuf_free(buf);
	return ret;
}


void wnm_set_coloc_intf_elems(struct wpa_supplicant *wpa_s,
			      struct wpabuf *elems)
{
	if (elems && wpabuf_len(elems) == 0) {
		wpabuf_free(elems);
		elems = NULL;
	}

	/* NOTE: The elements are not stored as they are only send out once */

	if (wpa_s->conf->coloc_intf_reporting && elems &&
	    wpa_s->coloc_intf_dialog_token &&
	    (wpa_s->coloc_intf_auto_report == 1 ||
	     wpa_s->coloc_intf_auto_report == 3)) {
		/* TODO: Check that there has not been less than
		 * wpa_s->coloc_intf_timeout * 200 TU from the last report.
		 */
		wnm_send_coloc_intf_report(wpa_s,
					   wpa_s->coloc_intf_dialog_token,
					   elems);
	}

	wpabuf_free(elems);
}


void wnm_clear_coloc_intf_reporting(struct wpa_supplicant *wpa_s)
{
	wpa_s->coloc_intf_dialog_token = 0;
	wpa_s->coloc_intf_auto_report = 0;
}


bool wnm_is_bss_excluded(struct wpa_supplicant *wpa_s, struct wpa_bss *bss)
{
	int i;

	/*
	 * In case disassociation imminent is set, do no try to use a BSS to
	 * which we are connected.
	 */
	if (wpa_s->wnm_mode & WNM_BSS_TM_REQ_DISASSOC_IMMINENT) {
		if (!wpa_s->wnm_disassoc_mld) {
			if (ether_addr_equal(bss->bssid,
					     wpa_s->wnm_disassoc_addr))
				return true;
		} else {
			if (ether_addr_equal(bss->mld_addr,
					     wpa_s->wnm_disassoc_addr))
				return true;
		}
	}

	for (i = 0; i < wpa_s->wnm_num_neighbor_report; i++) {
		struct neighbor_report *nei;

		nei = &wpa_s->wnm_neighbor_report_elements[i];
		if (!ether_addr_equal(nei->bssid, bss->bssid) &&
		    (is_zero_ether_addr(bss->mld_addr) ||
		     !ether_addr_equal(nei->mld_addr, bss->mld_addr)))
			continue;

		if (nei->preference_present && nei->preference == 0)
			return true;

#ifdef CONFIG_MBO
		if (nei->drv_mbo_reject)
			return true;
#endif /* CONFIG_MBO */

		/*
		 * NOTE: We should select one entry and stick with it, but to
		 * do that we need to refactor the BSS selection to be MLD
		 * aware from the beginning. Instead we just check whether the
		 * link is permitted in any possible configuration. We are not
		 * supposed to do that, however the AP is able to reject a
		 * subset of the requested links.
		 */
		if (nei->mld_links && !(nei->mld_links & BIT(bss->mld_link_id)))
			continue;

		break;
	}

	/* If the abridged bit is set, the BSS must be a known neighbor. */
	if ((wpa_s->wnm_mode & WNM_BSS_TM_REQ_ABRIDGED) &&
	    wpa_s->wnm_num_neighbor_report == i)
		return true;

	return false;
}
