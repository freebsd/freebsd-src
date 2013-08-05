/*
 * hostapd - WNM
 * Copyright (c) 2011-2012, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "ap/hostapd.h"
#include "ap/sta_info.h"
#include "ap/ap_config.h"
#include "ap/ap_drv_ops.h"
#include "ap/wpa_auth.h"
#include "wnm_ap.h"

#define MAX_TFS_IE_LEN  1024


/* get the TFS IE from driver */
static int ieee80211_11_get_tfs_ie(struct hostapd_data *hapd, const u8 *addr,
				   u8 *buf, u16 *buf_len, enum wnm_oper oper)
{
	wpa_printf(MSG_DEBUG, "%s: TFS get operation %d", __func__, oper);

	return hostapd_drv_wnm_oper(hapd, oper, addr, buf, buf_len);
}


/* set the TFS IE to driver */
static int ieee80211_11_set_tfs_ie(struct hostapd_data *hapd, const u8 *addr,
				   u8 *buf, u16 *buf_len, enum wnm_oper oper)
{
	wpa_printf(MSG_DEBUG, "%s: TFS set operation %d", __func__, oper);

	return hostapd_drv_wnm_oper(hapd, oper, addr, buf, buf_len);
}


/* MLME-SLEEPMODE.response */
static int ieee802_11_send_wnmsleep_resp(struct hostapd_data *hapd,
					 const u8 *addr, u8 dialog_token,
					 u8 action_type, u16 intval)
{
	struct ieee80211_mgmt *mgmt;
	int res;
	size_t len;
	size_t gtk_elem_len = 0;
	size_t igtk_elem_len = 0;
	struct wnm_sleep_element wnmsleep_ie;
	u8 *wnmtfs_ie;
	u8 wnmsleep_ie_len;
	u16 wnmtfs_ie_len;
	u8 *pos;
	struct sta_info *sta;
	enum wnm_oper tfs_oper = action_type == WNM_SLEEP_MODE_ENTER ?
		WNM_SLEEP_TFS_RESP_IE_ADD : WNM_SLEEP_TFS_RESP_IE_NONE;

	sta = ap_get_sta(hapd, addr);
	if (sta == NULL) {
		wpa_printf(MSG_DEBUG, "%s: station not found", __func__);
		return -EINVAL;
	}

	/* WNM-Sleep Mode IE */
	os_memset(&wnmsleep_ie, 0, sizeof(struct wnm_sleep_element));
	wnmsleep_ie_len = sizeof(struct wnm_sleep_element);
	wnmsleep_ie.eid = WLAN_EID_WNMSLEEP;
	wnmsleep_ie.len = wnmsleep_ie_len - 2;
	wnmsleep_ie.action_type = action_type;
	wnmsleep_ie.status = WNM_STATUS_SLEEP_ACCEPT;
	wnmsleep_ie.intval = intval;

	/* TFS IE(s) */
	wnmtfs_ie = os_zalloc(MAX_TFS_IE_LEN);
	if (wnmtfs_ie == NULL)
		return -1;
	if (ieee80211_11_get_tfs_ie(hapd, addr, wnmtfs_ie, &wnmtfs_ie_len,
				    tfs_oper)) {
		wnmtfs_ie_len = 0;
		os_free(wnmtfs_ie);
		wnmtfs_ie = NULL;
	}

#define MAX_GTK_SUBELEM_LEN 45
#define MAX_IGTK_SUBELEM_LEN 26
	mgmt = os_zalloc(sizeof(*mgmt) + wnmsleep_ie_len +
			 MAX_GTK_SUBELEM_LEN + MAX_IGTK_SUBELEM_LEN);
	if (mgmt == NULL) {
		wpa_printf(MSG_DEBUG, "MLME: Failed to allocate buffer for "
			   "WNM-Sleep Response action frame");
		return -1;
	}
	os_memcpy(mgmt->da, addr, ETH_ALEN);
	os_memcpy(mgmt->sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, hapd->own_addr, ETH_ALEN);
	mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_ACTION);
	mgmt->u.action.category = WLAN_ACTION_WNM;
	mgmt->u.action.u.wnm_sleep_resp.action = WNM_SLEEP_MODE_RESP;
	mgmt->u.action.u.wnm_sleep_resp.dialogtoken = dialog_token;
	pos = (u8 *)mgmt->u.action.u.wnm_sleep_resp.variable;
	/* add key data if MFP is enabled */
	if (!wpa_auth_uses_mfp(sta->wpa_sm) ||
	    action_type != WNM_SLEEP_MODE_EXIT) {
		mgmt->u.action.u.wnm_sleep_resp.keydata_len = 0;
	} else {
		gtk_elem_len = wpa_wnmsleep_gtk_subelem(sta->wpa_sm, pos);
		pos += gtk_elem_len;
		wpa_printf(MSG_DEBUG, "Pass 4, gtk_len = %d",
			   (int) gtk_elem_len);
#ifdef CONFIG_IEEE80211W
		res = wpa_wnmsleep_igtk_subelem(sta->wpa_sm, pos);
		if (res < 0) {
			os_free(wnmtfs_ie);
			os_free(mgmt);
			return -1;
		}
		igtk_elem_len = res;
		pos += igtk_elem_len;
		wpa_printf(MSG_DEBUG, "Pass 4 igtk_len = %d",
			   (int) igtk_elem_len);
#endif /* CONFIG_IEEE80211W */

		WPA_PUT_LE16((u8 *)
			     &mgmt->u.action.u.wnm_sleep_resp.keydata_len,
			     gtk_elem_len + igtk_elem_len);
	}
	os_memcpy(pos, &wnmsleep_ie, wnmsleep_ie_len);
	/* copy TFS IE here */
	pos += wnmsleep_ie_len;
	if (wnmtfs_ie)
		os_memcpy(pos, wnmtfs_ie, wnmtfs_ie_len);

	len = 1 + sizeof(mgmt->u.action.u.wnm_sleep_resp) + gtk_elem_len +
		igtk_elem_len + wnmsleep_ie_len + wnmtfs_ie_len;

	/* In driver, response frame should be forced to sent when STA is in
	 * PS mode */
	res = hostapd_drv_send_action(hapd, hapd->iface->freq, 0,
				      mgmt->da, &mgmt->u.action.category, len);

	if (!res) {
		wpa_printf(MSG_DEBUG, "Successfully send WNM-Sleep Response "
			   "frame");

		/* when entering wnmsleep
		 * 1. pause the node in driver
		 * 2. mark the node so that AP won't update GTK/IGTK during
		 * WNM Sleep
		 */
		if (wnmsleep_ie.status == WNM_STATUS_SLEEP_ACCEPT &&
		    wnmsleep_ie.action_type == WNM_SLEEP_MODE_ENTER) {
			hostapd_drv_wnm_oper(hapd, WNM_SLEEP_ENTER_CONFIRM,
					     addr, NULL, NULL);
			wpa_set_wnmsleep(sta->wpa_sm, 1);
		}
		/* when exiting wnmsleep
		 * 1. unmark the node
		 * 2. start GTK/IGTK update if MFP is not used
		 * 3. unpause the node in driver
		 */
		if ((wnmsleep_ie.status == WNM_STATUS_SLEEP_ACCEPT ||
		     wnmsleep_ie.status ==
		     WNM_STATUS_SLEEP_EXIT_ACCEPT_GTK_UPDATE) &&
		    wnmsleep_ie.action_type == WNM_SLEEP_MODE_EXIT) {
			wpa_set_wnmsleep(sta->wpa_sm, 0);
			hostapd_drv_wnm_oper(hapd, WNM_SLEEP_EXIT_CONFIRM,
					     addr, NULL, NULL);
			if (!wpa_auth_uses_mfp(sta->wpa_sm))
				wpa_wnmsleep_rekey_gtk(sta->wpa_sm);
		}
	} else
		wpa_printf(MSG_DEBUG, "Fail to send WNM-Sleep Response frame");

#undef MAX_GTK_SUBELEM_LEN
#undef MAX_IGTK_SUBELEM_LEN
	os_free(wnmtfs_ie);
	os_free(mgmt);
	return res;
}


static void ieee802_11_rx_wnmsleep_req(struct hostapd_data *hapd,
				       const u8 *addr, const u8 *frm, int len)
{
	/* Dialog Token [1] | WNM-Sleep Mode IE | TFS Response IE */
	const u8 *pos = frm;
	u8 dialog_token;
	struct wnm_sleep_element *wnmsleep_ie = NULL;
	/* multiple TFS Req IE (assuming consecutive) */
	u8 *tfsreq_ie_start = NULL;
	u8 *tfsreq_ie_end = NULL;
	u16 tfsreq_ie_len = 0;

	dialog_token = *pos++;
	while (pos + 1 < frm + len) {
		u8 ie_len = pos[1];
		if (pos + 2 + ie_len > frm + len)
			break;
		if (*pos == WLAN_EID_WNMSLEEP)
			wnmsleep_ie = (struct wnm_sleep_element *) pos;
		else if (*pos == WLAN_EID_TFS_REQ) {
			if (!tfsreq_ie_start)
				tfsreq_ie_start = (u8 *) pos;
			tfsreq_ie_end = (u8 *) pos;
		} else
			wpa_printf(MSG_DEBUG, "WNM: EID %d not recognized",
				   *pos);
		pos += ie_len + 2;
	}

	if (!wnmsleep_ie) {
		wpa_printf(MSG_DEBUG, "No WNM-Sleep IE found");
		return;
	}

	if (wnmsleep_ie->action_type == WNM_SLEEP_MODE_ENTER &&
	    tfsreq_ie_start && tfsreq_ie_end &&
	    tfsreq_ie_end - tfsreq_ie_start >= 0) {
		tfsreq_ie_len = (tfsreq_ie_end + tfsreq_ie_end[1] + 2) -
			tfsreq_ie_start;
		wpa_printf(MSG_DEBUG, "TFS Req IE(s) found");
		/* pass the TFS Req IE(s) to driver for processing */
		if (ieee80211_11_set_tfs_ie(hapd, addr, tfsreq_ie_start,
					    &tfsreq_ie_len,
					    WNM_SLEEP_TFS_REQ_IE_SET))
			wpa_printf(MSG_DEBUG, "Fail to set TFS Req IE");
	}

	ieee802_11_send_wnmsleep_resp(hapd, addr, dialog_token,
				      wnmsleep_ie->action_type,
				      wnmsleep_ie->intval);

	if (wnmsleep_ie->action_type == WNM_SLEEP_MODE_EXIT) {
		/* clear the tfs after sending the resp frame */
		ieee80211_11_set_tfs_ie(hapd, addr, tfsreq_ie_start,
					&tfsreq_ie_len, WNM_SLEEP_TFS_IE_DEL);
	}
}


int ieee802_11_rx_wnm_action_ap(struct hostapd_data *hapd,
				struct rx_action *action)
{
	if (action->len < 1 || action->data == NULL)
		return -1;

	switch (action->data[0]) {
	case WNM_BSS_TRANS_MGMT_QUERY:
		wpa_printf(MSG_DEBUG, "WNM: BSS Transition Management Query");
		/* TODO */
		return -1;
	case WNM_BSS_TRANS_MGMT_RESP:
		wpa_printf(MSG_DEBUG, "WNM: BSS Transition Management "
			   "Response");
		/* TODO */
		return -1;
	case WNM_SLEEP_MODE_REQ:
		ieee802_11_rx_wnmsleep_req(hapd, action->sa, action->data + 1,
					   action->len - 1);
		return 0;
	}

	wpa_printf(MSG_DEBUG, "WNM: Unsupported WNM Action %u from " MACSTR,
		   action->data[0], MAC2STR(action->sa));
	return -1;
}
