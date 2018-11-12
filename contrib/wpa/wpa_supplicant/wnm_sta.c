/*
 * wpa_supplicant - WNM
 * Copyright (c) 2011-2012, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "rsn_supp/wpa.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "scan.h"

#define MAX_TFS_IE_LEN  1024


/* get the TFS IE from driver */
static int ieee80211_11_get_tfs_ie(struct wpa_supplicant *wpa_s, u8 *buf,
				   u16 *buf_len, enum wnm_oper oper)
{
	wpa_printf(MSG_DEBUG, "%s: TFS get operation %d", __func__, oper);

	return wpa_drv_wnm_oper(wpa_s, oper, wpa_s->bssid, buf, buf_len);
}


/* set the TFS IE to driver */
static int ieee80211_11_set_tfs_ie(struct wpa_supplicant *wpa_s,
				   const u8 *addr, u8 *buf, u16 *buf_len,
				   enum wnm_oper oper)
{
	wpa_printf(MSG_DEBUG, "%s: TFS set operation %d", __func__, oper);

	return wpa_drv_wnm_oper(wpa_s, oper, addr, buf, buf_len);
}


/* MLME-SLEEPMODE.request */
int ieee802_11_send_wnmsleep_req(struct wpa_supplicant *wpa_s,
				 u8 action, u16 intval, struct wpabuf *tfs_req)
{
	struct ieee80211_mgmt *mgmt;
	int res;
	size_t len;
	struct wnm_sleep_element *wnmsleep_ie;
	u8 *wnmtfs_ie;
	u8 wnmsleep_ie_len;
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
		wnmtfs_ie = os_malloc(wnmtfs_ie_len);
		if (wnmtfs_ie == NULL) {
			os_free(wnmsleep_ie);
			return -1;
		}
		os_memcpy(wnmtfs_ie, wpabuf_head(tfs_req), wnmtfs_ie_len);
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

	mgmt = os_zalloc(sizeof(*mgmt) + wnmsleep_ie_len + wnmtfs_ie_len);
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

	len = 1 + sizeof(mgmt->u.action.u.wnm_sleep_req) + wnmsleep_ie_len +
		wnmtfs_ie_len;

	res = wpa_drv_send_action(wpa_s, wpa_s->assoc_freq, 0, wpa_s->bssid,
				  wpa_s->own_addr, wpa_s->bssid,
				  &mgmt->u.action.category, len, 0);
	if (res < 0)
		wpa_printf(MSG_DEBUG, "Failed to send WNM-Sleep Request "
			   "(action=%d, intval=%d)", action, intval);

	os_free(wnmsleep_ie);
	os_free(wnmtfs_ie);
	os_free(mgmt);

	return res;
}


static void wnm_sleep_mode_enter_success(struct wpa_supplicant *wpa_s,
					 u8 *tfsresp_ie_start,
					 u8 *tfsresp_ie_end)
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
					    &tfsresp_ie_len,
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
	ptr = (u8 *) frm + 1 + 1 + 2;
	end = ptr + key_len_total;
	wpa_hexdump_key(MSG_DEBUG, "WNM: Key Data", ptr, key_len_total);

	while (ptr + 1 < end) {
		if (ptr + 2 + ptr[1] > end) {
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
#ifdef CONFIG_IEEE80211W
		} else if (*ptr == WNM_SLEEP_SUBELEM_IGTK) {
			if (ptr[1] < 2 + 6 + WPA_IGTK_LEN) {
				wpa_printf(MSG_DEBUG, "WNM: Too short IGTK "
					   "subelem");
				break;
			}
			wpa_wnmsleep_install_key(wpa_s->wpa,
						 WNM_SLEEP_SUBELEM_IGTK, ptr);
			ptr += 10 + WPA_IGTK_LEN;
#endif /* CONFIG_IEEE80211W */
		} else
			break; /* skip the loop */
	}
}


static void ieee802_11_rx_wnmsleep_resp(struct wpa_supplicant *wpa_s,
					const u8 *frm, int len)
{
	/*
	 * Action [1] | Diaglog Token [1] | Key Data Len [2] | Key Data |
	 * WNM-Sleep Mode IE | TFS Response IE
	 */
	u8 *pos = (u8 *) frm; /* point to action field */
	u16 key_len_total = le_to_host16(*((u16 *)(frm+2)));
	struct wnm_sleep_element *wnmsleep_ie = NULL;
	/* multiple TFS Resp IE (assuming consecutive) */
	u8 *tfsresp_ie_start = NULL;
	u8 *tfsresp_ie_end = NULL;

	wpa_printf(MSG_DEBUG, "action=%d token = %d key_len_total = %d",
		   frm[0], frm[1], key_len_total);
	pos += 4 + key_len_total;
	if (pos > frm + len) {
		wpa_printf(MSG_INFO, "WNM: Too short frame for Key Data field");
		return;
	}
	while (pos - frm < len) {
		u8 ie_len = *(pos + 1);
		if (pos + 2 + ie_len > frm + len) {
			wpa_printf(MSG_INFO, "WNM: Invalid IE len %u", ie_len);
			break;
		}
		wpa_hexdump(MSG_DEBUG, "WNM: Element", pos, 2 + ie_len);
		if (*pos == WLAN_EID_WNMSLEEP)
			wnmsleep_ie = (struct wnm_sleep_element *) pos;
		else if (*pos == WLAN_EID_TFS_RESP) {
			if (!tfsresp_ie_start)
				tfsresp_ie_start = pos;
			tfsresp_ie_end = pos;
		} else
			wpa_printf(MSG_DEBUG, "EID %d not recognized", *pos);
		pos += ie_len + 2;
	}

	if (!wnmsleep_ie) {
		wpa_printf(MSG_DEBUG, "No WNM-Sleep IE found");
		return;
	}

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


static void wnm_send_bss_transition_mgmt_resp(struct wpa_supplicant *wpa_s,
					      u8 dialog_token, u8 status,
					      u8 delay, const u8 *target_bssid)
{
	u8 buf[1000], *pos;
	struct ieee80211_mgmt *mgmt;
	size_t len;

	wpa_printf(MSG_DEBUG, "WNM: Send BSS Transition Management Response "
		   "to " MACSTR " dialog_token=%u status=%u delay=%d",
		   MAC2STR(wpa_s->bssid), dialog_token, status, delay);

	mgmt = (struct ieee80211_mgmt *) buf;
	os_memset(&buf, 0, sizeof(buf));
	os_memcpy(mgmt->da, wpa_s->bssid, ETH_ALEN);
	os_memcpy(mgmt->sa, wpa_s->own_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, wpa_s->bssid, ETH_ALEN);
	mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_ACTION);
	mgmt->u.action.category = WLAN_ACTION_WNM;
	mgmt->u.action.u.bss_tm_resp.action = WNM_BSS_TRANS_MGMT_RESP;
	mgmt->u.action.u.bss_tm_resp.dialog_token = dialog_token;
	mgmt->u.action.u.bss_tm_resp.status_code = status;
	mgmt->u.action.u.bss_tm_resp.bss_termination_delay = delay;
	pos = mgmt->u.action.u.bss_tm_resp.variable;
	if (target_bssid) {
		os_memcpy(pos, target_bssid, ETH_ALEN);
		pos += ETH_ALEN;
	}

	len = pos - (u8 *) &mgmt->u.action.category;

	wpa_drv_send_action(wpa_s, wpa_s->assoc_freq, 0, wpa_s->bssid,
			    wpa_s->own_addr, wpa_s->bssid,
			    &mgmt->u.action.category, len, 0);
}


static void ieee802_11_rx_bss_trans_mgmt_req(struct wpa_supplicant *wpa_s,
					     const u8 *pos, const u8 *end,
					     int reply)
{
	u8 dialog_token;
	u8 mode;
	u16 disassoc_timer;

	if (pos + 5 > end)
		return;

	dialog_token = pos[0];
	mode = pos[1];
	disassoc_timer = WPA_GET_LE16(pos + 2);

	wpa_printf(MSG_DEBUG, "WNM: BSS Transition Management Request: "
		   "dialog_token=%u request_mode=0x%x "
		   "disassoc_timer=%u validity_interval=%u",
		   dialog_token, mode, disassoc_timer, pos[4]);
	pos += 5;
	if (mode & 0x08)
		pos += 12; /* BSS Termination Duration */
	if (mode & 0x10) {
		char url[256];
		if (pos + 1 > end || pos + 1 + pos[0] > end) {
			wpa_printf(MSG_DEBUG, "WNM: Invalid BSS Transition "
				   "Management Request (URL)");
			return;
		}
		os_memcpy(url, pos + 1, pos[0]);
		url[pos[0]] = '\0';
		wpa_msg(wpa_s, MSG_INFO, "WNM: ESS Disassociation Imminent - "
			"session_info_url=%s", url);
	}

	if (mode & 0x04) {
		wpa_msg(wpa_s, MSG_INFO, "WNM: Disassociation Imminent - "
			"Disassociation Timer %u", disassoc_timer);
		if (disassoc_timer && !wpa_s->scanning) {
			/* TODO: mark current BSS less preferred for
			 * selection */
			wpa_printf(MSG_DEBUG, "Trying to find another BSS");
			wpa_supplicant_req_scan(wpa_s, 0, 0);
		}
	}

	if (reply) {
		/* TODO: add support for reporting Accept */
		wnm_send_bss_transition_mgmt_resp(wpa_s, dialog_token,
						  1 /* Reject - unspecified */,
						  0, NULL);
	}
}


void ieee802_11_rx_wnm_action(struct wpa_supplicant *wpa_s,
			      struct rx_action *action)
{
	const u8 *pos, *end;
	u8 act;

	if (action->data == NULL || action->len == 0)
		return;

	pos = action->data;
	end = pos + action->len;
	act = *pos++;

	wpa_printf(MSG_DEBUG, "WNM: RX action %u from " MACSTR,
		   act, MAC2STR(action->sa));
	if (wpa_s->wpa_state < WPA_ASSOCIATED ||
	    os_memcmp(action->sa, wpa_s->bssid, ETH_ALEN) != 0) {
		wpa_printf(MSG_DEBUG, "WNM: Ignore unexpected WNM Action "
			   "frame");
		return;
	}

	switch (act) {
	case WNM_BSS_TRANS_MGMT_REQ:
		ieee802_11_rx_bss_trans_mgmt_req(wpa_s, pos, end,
						 !(action->da[0] & 0x01));
		break;
	case WNM_SLEEP_MODE_RESP:
		ieee802_11_rx_wnmsleep_resp(wpa_s, action->data, action->len);
		break;
	default:
		break;
	}
}
