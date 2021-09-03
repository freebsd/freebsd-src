/*
 * wpa_supplicant - Robust AV procedures
 * Copyright (c) 2020, The Linux Foundation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "common/wpa_ctrl.h"
#include "common/ieee802_11_common.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "bss.h"


void wpas_populate_mscs_descriptor_ie(struct robust_av_data *robust_av,
				      struct wpabuf *buf)
{
	u8 *len, *len1;

	/* MSCS descriptor element */
	wpabuf_put_u8(buf, WLAN_EID_EXTENSION);
	len = wpabuf_put(buf, 1);
	wpabuf_put_u8(buf, WLAN_EID_EXT_MSCS_DESCRIPTOR);
	wpabuf_put_u8(buf, robust_av->request_type);
	wpabuf_put_u8(buf, robust_av->up_bitmap);
	wpabuf_put_u8(buf, robust_av->up_limit);
	wpabuf_put_le32(buf, robust_av->stream_timeout);

	if (robust_av->request_type != SCS_REQ_REMOVE) {
		/* TCLAS mask element */
		wpabuf_put_u8(buf, WLAN_EID_EXTENSION);
		len1 = wpabuf_put(buf, 1);
		wpabuf_put_u8(buf, WLAN_EID_EXT_TCLAS_MASK);

		/* Frame classifier */
		wpabuf_put_data(buf, robust_av->frame_classifier,
				robust_av->frame_classifier_len);
		*len1 = (u8 *) wpabuf_put(buf, 0) - len1 - 1;
	}

	*len = (u8 *) wpabuf_put(buf, 0) - len - 1;
}


int wpas_send_mscs_req(struct wpa_supplicant *wpa_s)
{
	struct wpabuf *buf;
	size_t buf_len;
	int ret;

	if (wpa_s->wpa_state != WPA_COMPLETED || !wpa_s->current_ssid)
		return 0;

	if (!wpa_bss_ext_capab(wpa_s->current_bss, WLAN_EXT_CAPAB_MSCS)) {
		wpa_dbg(wpa_s, MSG_INFO,
			"AP does not support MSCS - could not send MSCS Req");
		return -1;
	}

	if (!wpa_s->mscs_setup_done &&
	    wpa_s->robust_av.request_type != SCS_REQ_ADD) {
		wpa_msg(wpa_s, MSG_INFO,
			"MSCS: Failed to send MSCS Request: request type invalid");
		return -1;
	}

	buf_len = 3 +	/* Action frame header */
		  3 +	/* MSCS descriptor IE header */
		  1 +	/* Request type */
		  2 +	/* User priority control */
		  4 +	/* Stream timeout */
		  3 +	/* TCLAS Mask IE header */
		  wpa_s->robust_av.frame_classifier_len;

	buf = wpabuf_alloc(buf_len);
	if (!buf) {
		wpa_printf(MSG_ERROR, "Failed to allocate MSCS req");
		return -1;
	}

	wpabuf_put_u8(buf, WLAN_ACTION_ROBUST_AV_STREAMING);
	wpabuf_put_u8(buf, ROBUST_AV_MSCS_REQ);
	wpa_s->robust_av.dialog_token++;
	wpabuf_put_u8(buf, wpa_s->robust_av.dialog_token);

	/* MSCS descriptor element */
	wpas_populate_mscs_descriptor_ie(&wpa_s->robust_av, buf);

	wpa_hexdump_buf(MSG_MSGDUMP, "MSCS Request", buf);
	ret = wpa_drv_send_action(wpa_s, wpa_s->assoc_freq, 0, wpa_s->bssid,
				  wpa_s->own_addr, wpa_s->bssid,
				  wpabuf_head(buf), wpabuf_len(buf), 0);
	if (ret < 0)
		wpa_dbg(wpa_s, MSG_INFO, "MSCS: Failed to send MSCS Request");

	wpabuf_free(buf);
	return ret;
}


void wpas_handle_robust_av_recv_action(struct wpa_supplicant *wpa_s,
				       const u8 *src, const u8 *buf, size_t len)
{
	u8 dialog_token;
	u16 status_code;

	if (len < 3)
		return;

	dialog_token = *buf++;
	if (dialog_token != wpa_s->robust_av.dialog_token) {
		wpa_printf(MSG_INFO,
			   "MSCS: Drop received frame due to dialog token mismatch: received:%u expected:%u",
			   dialog_token, wpa_s->robust_av.dialog_token);
		return;
	}

	status_code = WPA_GET_LE16(buf);
	wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_MSCS_RESULT "bssid=" MACSTR
		" status_code=%u", MAC2STR(src), status_code);
	wpa_s->mscs_setup_done = status_code == WLAN_STATUS_SUCCESS;
}


void wpas_handle_assoc_resp_mscs(struct wpa_supplicant *wpa_s, const u8 *bssid,
				 const u8 *ies, size_t ies_len)
{
	const u8 *mscs_desc_ie, *mscs_status;
	u16 status;

	/* Process optional MSCS Status subelement when MSCS IE is in
	 * (Re)Association Response frame */
	if (!ies || ies_len == 0 || !wpa_s->robust_av.valid_config)
		return;

	mscs_desc_ie = get_ie_ext(ies, ies_len, WLAN_EID_EXT_MSCS_DESCRIPTOR);
	if (!mscs_desc_ie || mscs_desc_ie[1] <= 8)
		return;

	/* Subelements start after (ie_id(1) + ie_len(1) + ext_id(1) +
	 * request type(1) + upc(2) + stream timeout(4) =) 10.
	 */
	mscs_status = get_ie(&mscs_desc_ie[10], mscs_desc_ie[1] - 8,
			     MCSC_SUBELEM_STATUS);
	if (!mscs_status || mscs_status[1] < 2)
		return;

	status = WPA_GET_LE16(mscs_status + 2);
	wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_MSCS_RESULT "bssid=" MACSTR
		" status_code=%u", MAC2STR(bssid), status);
	wpa_s->mscs_setup_done = status == WLAN_STATUS_SUCCESS;
}
