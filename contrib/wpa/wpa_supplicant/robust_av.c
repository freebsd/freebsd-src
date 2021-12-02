/*
 * wpa_supplicant - Robust AV procedures
 * Copyright (c) 2020, The Linux Foundation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "utils/eloop.h"
#include "common/wpa_ctrl.h"
#include "common/ieee802_11_common.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "bss.h"


#define SCS_RESP_TIMEOUT 1
#define DSCP_REQ_TIMEOUT 5


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


static int wpas_populate_type4_classifier(struct type4_params *type4_param,
					  struct wpabuf *buf)
{
	/* classifier parameters */
	wpabuf_put_u8(buf, type4_param->classifier_mask);
	if (type4_param->ip_version == IPV4) {
		wpabuf_put_u8(buf, IPV4); /* IP version */
		wpabuf_put_data(buf, &type4_param->ip_params.v4.src_ip.s_addr,
				4);
		wpabuf_put_data(buf, &type4_param->ip_params.v4.dst_ip.s_addr,
				4);
		wpabuf_put_be16(buf, type4_param->ip_params.v4.src_port);
		wpabuf_put_be16(buf, type4_param->ip_params.v4.dst_port);
		wpabuf_put_u8(buf, type4_param->ip_params.v4.dscp);
		wpabuf_put_u8(buf, type4_param->ip_params.v4.protocol);
		wpabuf_put_u8(buf, 0); /* Reserved octet */
	} else {
		wpabuf_put_u8(buf, IPV6);
		wpabuf_put_data(buf, &type4_param->ip_params.v6.src_ip.s6_addr,
				16);
		wpabuf_put_data(buf, &type4_param->ip_params.v6.dst_ip.s6_addr,
				16);
		wpabuf_put_be16(buf, type4_param->ip_params.v6.src_port);
		wpabuf_put_be16(buf, type4_param->ip_params.v6.dst_port);
		wpabuf_put_u8(buf, type4_param->ip_params.v6.dscp);
		wpabuf_put_u8(buf, type4_param->ip_params.v6.next_header);
		wpabuf_put_data(buf, type4_param->ip_params.v6.flow_label, 3);
	}

	return 0;
}


static int wpas_populate_type10_classifier(struct type10_params *type10_param,
					   struct wpabuf *buf)
{
	/* classifier parameters */
	wpabuf_put_u8(buf, type10_param->prot_instance);
	wpabuf_put_u8(buf, type10_param->prot_number);
	wpabuf_put_data(buf, type10_param->filter_value,
			type10_param->filter_len);
	wpabuf_put_data(buf, type10_param->filter_mask,
			type10_param->filter_len);
	return 0;
}


static int wpas_populate_scs_descriptor_ie(struct scs_desc_elem *desc_elem,
					   struct wpabuf *buf)
{
	u8 *len, *len1;
	struct tclas_element *tclas_elem;
	unsigned int i;

	/* SCS Descriptor element */
	wpabuf_put_u8(buf, WLAN_EID_SCS_DESCRIPTOR);
	len = wpabuf_put(buf, 1);
	wpabuf_put_u8(buf, desc_elem->scs_id);
	wpabuf_put_u8(buf, desc_elem->request_type);
	if (desc_elem->request_type == SCS_REQ_REMOVE)
		goto end;

	if (desc_elem->intra_access_priority || desc_elem->scs_up_avail) {
		wpabuf_put_u8(buf, WLAN_EID_INTRA_ACCESS_CATEGORY_PRIORITY);
		wpabuf_put_u8(buf, 1);
		wpabuf_put_u8(buf, desc_elem->intra_access_priority);
	}

	tclas_elem = desc_elem->tclas_elems;

	if (!tclas_elem)
		return -1;

	for (i = 0; i < desc_elem->num_tclas_elem; i++, tclas_elem++) {
		int ret;

		/* TCLAS element */
		wpabuf_put_u8(buf, WLAN_EID_TCLAS);
		len1 = wpabuf_put(buf, 1);
		wpabuf_put_u8(buf, 255); /* User Priority: not compared */
		/* Frame Classifier */
		wpabuf_put_u8(buf, tclas_elem->classifier_type);
		/* Frame classifier parameters */
		switch (tclas_elem->classifier_type) {
		case 4:
			ret = wpas_populate_type4_classifier(
				&tclas_elem->frame_classifier.type4_param,
				buf);
			break;
		case 10:
			ret = wpas_populate_type10_classifier(
				&tclas_elem->frame_classifier.type10_param,
				buf);
			break;
		default:
			return -1;
		}

		if (ret == -1) {
			wpa_printf(MSG_ERROR,
				   "Failed to populate frame classifier");
			return -1;
		}

		*len1 = (u8 *) wpabuf_put(buf, 0) - len1 - 1;
	}

	if (desc_elem->num_tclas_elem > 1) {
		/* TCLAS Processing element */
		wpabuf_put_u8(buf, WLAN_EID_TCLAS_PROCESSING);
		wpabuf_put_u8(buf, 1);
		wpabuf_put_u8(buf, desc_elem->tclas_processing);
	}

end:
	*len = (u8 *) wpabuf_put(buf, 0) - len - 1;
	return 0;
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


static size_t tclas_elem_len(const struct tclas_element *elem)
{
	size_t buf_len = 0;

	buf_len += 2 +	/* TCLAS element header */
		1 +	/* User Priority */
		1 ;	/* Classifier Type */

	if (elem->classifier_type == 4) {
		enum ip_version ip_ver;

		buf_len += 1 +	/* Classifier mask */
			1 +	/* IP version */
			1 +	/* user priority */
			2 +	/* src_port */
			2 +	/* dst_port */
			1 ;	/* dscp */
		ip_ver = elem->frame_classifier.type4_param.ip_version;
		if (ip_ver == IPV4) {
			buf_len += 4 +  /* src_ip */
				4 +	/* dst_ip */
				1 +	/* protocol */
				1 ;  /* Reserved */
		} else if (ip_ver == IPV6) {
			buf_len += 16 +  /* src_ip */
				16 +  /* dst_ip */
				1  +  /* next_header */
				3  ;  /* flow_label */
		} else {
			wpa_printf(MSG_ERROR, "%s: Incorrect IP version %d",
				   __func__, ip_ver);
			return 0;
		}
	} else if (elem->classifier_type == 10) {
		buf_len += 1 +	/* protocol instance */
			1 +	/* protocol number */
			2 * elem->frame_classifier.type10_param.filter_len;
	} else {
		wpa_printf(MSG_ERROR, "%s: Incorrect classifier type %u",
			   __func__, elem->classifier_type);
		return 0;
	}

	return buf_len;
}


static struct wpabuf * allocate_scs_buf(struct scs_desc_elem *desc_elem,
					unsigned int num_scs_desc)
{
	struct wpabuf *buf;
	size_t buf_len = 0;
	unsigned int i, j;

	buf_len = 3; /* Action frame header */

	for (i = 0; i < num_scs_desc; i++, desc_elem++) {
		struct tclas_element *tclas_elem;

		buf_len += 2 +	/* SCS descriptor IE header */
			   1 +	/* SCSID */
			   1 ;	/* Request type */

		if (desc_elem->request_type == SCS_REQ_REMOVE)
			continue;

		if (desc_elem->intra_access_priority || desc_elem->scs_up_avail)
			buf_len += 3;

		tclas_elem = desc_elem->tclas_elems;
		if (!tclas_elem) {
			wpa_printf(MSG_ERROR, "%s: TCLAS element null",
				   __func__);
			return NULL;
		}

		for (j = 0; j < desc_elem->num_tclas_elem; j++, tclas_elem++) {
			size_t elen;

			elen = tclas_elem_len(tclas_elem);
			if (elen == 0)
				return NULL;
			buf_len += elen;
		}

		if (desc_elem->num_tclas_elem > 1) {
			buf_len += 1 +	/* TCLAS Processing eid */
				   1 +	/* length */
				   1 ;	/* processing */
		}
	}

	buf = wpabuf_alloc(buf_len);
	if (!buf) {
		wpa_printf(MSG_ERROR, "Failed to allocate SCS req");
		return NULL;
	}

	return buf;
}


static void scs_request_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct active_scs_elem *scs_desc, *prev;

	if (wpa_s->wpa_state != WPA_COMPLETED || !wpa_s->current_ssid)
		return;

	/* Once timeout is over, remove all SCS descriptors with no response */
	dl_list_for_each_safe(scs_desc, prev, &wpa_s->active_scs_ids,
			      struct active_scs_elem, list) {
		u8 bssid[ETH_ALEN] = { 0 };
		const u8 *src;

		if (scs_desc->status == SCS_DESC_SUCCESS)
			continue;

		if (wpa_s->current_bss)
			src = wpa_s->current_bss->bssid;
		else
			src = bssid;

		wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_SCS_RESULT "bssid=" MACSTR
			" SCSID=%u status_code=timedout", MAC2STR(src),
			scs_desc->scs_id);

		dl_list_del(&scs_desc->list);
		wpa_printf(MSG_INFO, "%s: SCSID %d removed after timeout",
			   __func__, scs_desc->scs_id);
		os_free(scs_desc);
	}

	eloop_cancel_timeout(scs_request_timer, wpa_s, NULL);
	wpa_s->ongoing_scs_req = false;
}


int wpas_send_scs_req(struct wpa_supplicant *wpa_s)
{
	struct wpabuf *buf = NULL;
	struct scs_desc_elem *desc_elem = NULL;
	int ret = -1;
	unsigned int i;

	if (wpa_s->wpa_state != WPA_COMPLETED || !wpa_s->current_ssid)
		return -1;

	if (!wpa_bss_ext_capab(wpa_s->current_bss, WLAN_EXT_CAPAB_SCS)) {
		wpa_dbg(wpa_s, MSG_INFO,
			"AP does not support SCS - could not send SCS Request");
		return -1;
	}

	desc_elem = wpa_s->scs_robust_av_req.scs_desc_elems;
	if (!desc_elem)
		return -1;

	buf = allocate_scs_buf(desc_elem,
			       wpa_s->scs_robust_av_req.num_scs_desc);
	if (!buf)
		return -1;

	wpabuf_put_u8(buf, WLAN_ACTION_ROBUST_AV_STREAMING);
	wpabuf_put_u8(buf, ROBUST_AV_SCS_REQ);
	wpa_s->scs_dialog_token++;
	if (wpa_s->scs_dialog_token == 0)
		wpa_s->scs_dialog_token++;
	wpabuf_put_u8(buf, wpa_s->scs_dialog_token);

	for (i = 0; i < wpa_s->scs_robust_av_req.num_scs_desc;
	     i++, desc_elem++) {
		/* SCS Descriptor element */
		if (wpas_populate_scs_descriptor_ie(desc_elem, buf) < 0)
			goto end;
	}

	wpa_hexdump_buf(MSG_DEBUG, "SCS Request", buf);
	ret = wpa_drv_send_action(wpa_s, wpa_s->assoc_freq, 0, wpa_s->bssid,
				  wpa_s->own_addr, wpa_s->bssid,
				  wpabuf_head(buf), wpabuf_len(buf), 0);
	if (ret < 0) {
		wpa_dbg(wpa_s, MSG_ERROR, "SCS: Failed to send SCS Request");
		wpa_s->scs_dialog_token--;
		goto end;
	}

	desc_elem = wpa_s->scs_robust_av_req.scs_desc_elems;
	for (i = 0; i < wpa_s->scs_robust_av_req.num_scs_desc;
	     i++, desc_elem++) {
		struct active_scs_elem *active_scs_elem;

		if (desc_elem->request_type != SCS_REQ_ADD)
			continue;

		active_scs_elem = os_malloc(sizeof(struct active_scs_elem));
		if (!active_scs_elem)
			break;
		active_scs_elem->scs_id = desc_elem->scs_id;
		active_scs_elem->status = SCS_DESC_SENT;
		dl_list_add(&wpa_s->active_scs_ids, &active_scs_elem->list);
	}

	/*
	 * Register a timeout after which this request will be removed from
	 * the cache.
	 */
	eloop_register_timeout(SCS_RESP_TIMEOUT, 0, scs_request_timer, wpa_s,
			       NULL);
	wpa_s->ongoing_scs_req = true;

end:
	wpabuf_free(buf);
	free_up_scs_desc(&wpa_s->scs_robust_av_req);

	return ret;
}


void free_up_tclas_elem(struct scs_desc_elem *elem)
{
	struct tclas_element *tclas_elems = elem->tclas_elems;
	unsigned int num_tclas_elem = elem->num_tclas_elem;
	struct tclas_element *tclas_data;
	unsigned int j;

	elem->tclas_elems = NULL;
	elem->num_tclas_elem = 0;

	if (!tclas_elems)
		return;

	tclas_data = tclas_elems;
	for (j = 0; j < num_tclas_elem; j++, tclas_data++) {
		if (tclas_data->classifier_type != 10)
			continue;

		os_free(tclas_data->frame_classifier.type10_param.filter_value);
		os_free(tclas_data->frame_classifier.type10_param.filter_mask);
	}

	os_free(tclas_elems);
}


void free_up_scs_desc(struct scs_robust_av_data *data)
{
	struct scs_desc_elem *desc_elems = data->scs_desc_elems;
	unsigned int num_scs_desc = data->num_scs_desc;
	struct scs_desc_elem *desc_data;
	unsigned int i;

	data->scs_desc_elems = NULL;
	data->num_scs_desc = 0;

	if (!desc_elems)
		return;

	desc_data = desc_elems;
	for (i = 0; i < num_scs_desc; i++, desc_data++) {
		if (desc_data->request_type == SCS_REQ_REMOVE ||
		    !desc_data->tclas_elems)
			continue;

		free_up_tclas_elem(desc_data);
	}
	os_free(desc_elems);
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


static void wpas_wait_for_dscp_req_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;

	/* Once timeout is over, reset wait flag and allow sending DSCP query */
	wpa_printf(MSG_DEBUG,
		   "QM: Wait time over for sending DSCP request - allow DSCP query");
	wpa_s->wait_for_dscp_req = 0;
	wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_DSCP_POLICY "request_wait end");
}


void wpas_handle_assoc_resp_qos_mgmt(struct wpa_supplicant *wpa_s,
				     const u8 *ies, size_t ies_len)
{
	const u8 *wfa_capa;

	wpa_s->connection_dscp = 0;
	if (wpa_s->wait_for_dscp_req)
		eloop_cancel_timeout(wpas_wait_for_dscp_req_timer, wpa_s, NULL);

	if (!ies || ies_len == 0 || !wpa_s->enable_dscp_policy_capa)
		return;

	wfa_capa = get_vendor_ie(ies, ies_len, WFA_CAPA_IE_VENDOR_TYPE);
	if (!wfa_capa || wfa_capa[1] < 6 || wfa_capa[6] < 1 ||
	    !(wfa_capa[7] & WFA_CAPA_QM_DSCP_POLICY))
		return; /* AP does not enable QM DSCP Policy */

	wpa_s->connection_dscp = 1;
	wpa_s->wait_for_dscp_req = !!(wfa_capa[7] &
				      WFA_CAPA_QM_UNSOLIC_DSCP);
	if (!wpa_s->wait_for_dscp_req)
		return;

	/* Register a timeout after which dscp query can be sent to AP. */
	wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_DSCP_POLICY "request_wait start");
	eloop_register_timeout(DSCP_REQ_TIMEOUT, 0,
			       wpas_wait_for_dscp_req_timer, wpa_s, NULL);
}


void wpas_handle_robust_av_scs_recv_action(struct wpa_supplicant *wpa_s,
					   const u8 *src, const u8 *buf,
					   size_t len)
{
	u8 dialog_token;
	unsigned int i, count;
	struct active_scs_elem *scs_desc, *prev;

	if (len < 2)
		return;
	if (!wpa_s->ongoing_scs_req) {
		wpa_printf(MSG_INFO,
			   "SCS: Drop received response due to no ongoing request");
		return;
	}

	dialog_token = *buf++;
	len--;
	if (dialog_token != wpa_s->scs_dialog_token) {
		wpa_printf(MSG_INFO,
			   "SCS: Drop received frame due to dialog token mismatch: received:%u expected:%u",
			   dialog_token, wpa_s->scs_dialog_token);
		return;
	}

	/* This Count field does not exist in the IEEE Std 802.11-2020
	 * definition of the SCS Response frame. However, it was accepted to
	 * be added into REVme per REVme/D0.0 CC35 CID 49 (edits in document
	 * 11-21-0688-07). */
	count = *buf++;
	len--;
	if (count == 0 || count * 3 > len) {
		wpa_printf(MSG_INFO,
			   "SCS: Drop received frame due to invalid count: %u (remaining %zu octets)",
			   count, len);
		return;
	}

	for (i = 0; i < count; i++) {
		u8 id;
		u16 status;
		bool scs_desc_found = false;

		id = *buf++;
		status = WPA_GET_LE16(buf);
		buf += 2;
		len -= 3;

		dl_list_for_each(scs_desc, &wpa_s->active_scs_ids,
				 struct active_scs_elem, list) {
			if (id == scs_desc->scs_id) {
				scs_desc_found = true;
				break;
			}
		}

		if (!scs_desc_found) {
			wpa_printf(MSG_INFO, "SCS: SCS ID invalid %u", id);
			continue;
		}

		if (status != WLAN_STATUS_SUCCESS) {
			dl_list_del(&scs_desc->list);
			os_free(scs_desc);
		} else if (status == WLAN_STATUS_SUCCESS) {
			scs_desc->status = SCS_DESC_SUCCESS;
		}

		wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_SCS_RESULT "bssid=" MACSTR
			" SCSID=%u status_code=%u", MAC2STR(src), id, status);
	}

	eloop_cancel_timeout(scs_request_timer, wpa_s, NULL);
	wpa_s->ongoing_scs_req = false;

	dl_list_for_each_safe(scs_desc, prev, &wpa_s->active_scs_ids,
			      struct active_scs_elem, list) {
		if (scs_desc->status != SCS_DESC_SUCCESS) {
			wpa_msg(wpa_s, MSG_INFO,
				WPA_EVENT_SCS_RESULT "bssid=" MACSTR
				" SCSID=%u status_code=response_not_received",
				MAC2STR(src), scs_desc->scs_id);
			dl_list_del(&scs_desc->list);
			os_free(scs_desc);
		}
	}
}


static void wpas_clear_active_scs_ids(struct wpa_supplicant *wpa_s)
{
	struct active_scs_elem *scs_elem;

	while ((scs_elem = dl_list_first(&wpa_s->active_scs_ids,
					 struct active_scs_elem, list))) {
		dl_list_del(&scs_elem->list);
		os_free(scs_elem);
	}
}


void wpas_scs_deinit(struct wpa_supplicant *wpa_s)
{
	free_up_scs_desc(&wpa_s->scs_robust_av_req);
	wpa_s->scs_dialog_token = 0;
	wpas_clear_active_scs_ids(wpa_s);
	eloop_cancel_timeout(scs_request_timer, wpa_s, NULL);
	wpa_s->ongoing_scs_req = false;
}


static int write_ipv4_info(char *pos, int total_len,
			   const struct ipv4_params *v4)
{
	int res, rem_len;
	char addr[INET_ADDRSTRLEN];

	rem_len = total_len;

	if (v4->param_mask & BIT(1)) {
		if (!inet_ntop(AF_INET, &v4->src_ip, addr, INET_ADDRSTRLEN)) {
			wpa_printf(MSG_ERROR,
				   "QM: Failed to set IPv4 source address");
			return -1;
		}

		res = os_snprintf(pos, rem_len, " src_ip=%s", addr);
		if (os_snprintf_error(rem_len, res))
			return -1;

		pos += res;
		rem_len -= res;
	}

	if (v4->param_mask & BIT(2)) {
		if (!inet_ntop(AF_INET, &v4->dst_ip, addr, INET_ADDRSTRLEN)) {
			wpa_printf(MSG_ERROR,
				   "QM: Failed to set IPv4 destination address");
			return -1;
		}

		res = os_snprintf(pos, rem_len, " dst_ip=%s", addr);
		if (os_snprintf_error(rem_len, res))
			return -1;

		pos += res;
		rem_len -= res;
	}

	if (v4->param_mask & BIT(3)) {
		res = os_snprintf(pos, rem_len, " src_port=%d", v4->src_port);
		if (os_snprintf_error(rem_len, res))
			return -1;

		pos += res;
		rem_len -= res;
	}

	if (v4->param_mask & BIT(4)) {
		res = os_snprintf(pos, rem_len, " dst_port=%d", v4->dst_port);
		if (os_snprintf_error(rem_len, res))
			return -1;

		pos += res;
		rem_len -= res;
	}

	if (v4->param_mask & BIT(6)) {
		res = os_snprintf(pos, rem_len, " protocol=%d", v4->protocol);
		if (os_snprintf_error(rem_len, res))
			return -1;

		pos += res;
		rem_len -= res;
	}

	return total_len - rem_len;
}


static int write_ipv6_info(char *pos, int total_len,
			   const struct ipv6_params *v6)
{
	int res, rem_len;
	char addr[INET6_ADDRSTRLEN];

	rem_len = total_len;

	if (v6->param_mask & BIT(1)) {
		if (!inet_ntop(AF_INET6, &v6->src_ip, addr, INET6_ADDRSTRLEN)) {
			wpa_printf(MSG_ERROR,
				   "QM: Failed to set IPv6 source addr");
			return -1;
		}

		res = os_snprintf(pos, rem_len, " src_ip=%s", addr);
		if (os_snprintf_error(rem_len, res))
			return -1;

		pos += res;
		rem_len -= res;
	}

	if (v6->param_mask & BIT(2)) {
		if (!inet_ntop(AF_INET6, &v6->dst_ip, addr, INET6_ADDRSTRLEN)) {
			wpa_printf(MSG_ERROR,
				   "QM: Failed to set IPv6 destination addr");
			return -1;
		}

		res = os_snprintf(pos, rem_len, " dst_ip=%s", addr);
		if (os_snprintf_error(rem_len, res))
			return -1;

		pos += res;
		rem_len -= res;
	}

	if (v6->param_mask & BIT(3)) {
		res = os_snprintf(pos, rem_len, " src_port=%d", v6->src_port);
		if (os_snprintf_error(rem_len, res))
			return -1;

		pos += res;
		rem_len -= res;
	}

	if (v6->param_mask & BIT(4)) {
		res = os_snprintf(pos, rem_len, " dst_port=%d", v6->dst_port);
		if (os_snprintf_error(rem_len, res))
			return -1;

		pos += res;
		rem_len -= res;
	}

	if (v6->param_mask & BIT(6)) {
		res = os_snprintf(pos, rem_len, " protocol=%d",
				  v6->next_header);
		if (os_snprintf_error(rem_len, res))
			return -1;

		pos += res;
		rem_len -= res;
	}

	return total_len - rem_len;
}


struct dscp_policy_data {
	u8 policy_id;
	u8 req_type;
	u8 dscp;
	bool dscp_info;
	const u8 *frame_classifier;
	u8 frame_classifier_len;
	struct type4_params type4_param;
	const u8 *domain_name;
	u8 domain_name_len;
	u16 start_port;
	u16 end_port;
	bool port_range_info;
};


static int set_frame_classifier_type4_ipv4(struct dscp_policy_data *policy)
{
	u8 classifier_mask;
	const u8 *frame_classifier = policy->frame_classifier;
	struct type4_params *type4_param = &policy->type4_param;

	if (policy->frame_classifier_len < 18) {
		wpa_printf(MSG_ERROR,
			   "QM: Received IPv4 frame classifier with insufficient length %d",
			   policy->frame_classifier_len);
		return -1;
	}

	classifier_mask = frame_classifier[1];

	/* Classifier Mask - bit 1 = Source IP Address */
	if (classifier_mask & BIT(1)) {
		type4_param->ip_params.v4.param_mask |= BIT(1);
		os_memcpy(&type4_param->ip_params.v4.src_ip,
			  &frame_classifier[3], 4);
	}

	/* Classifier Mask - bit 2 = Destination IP Address */
	if (classifier_mask & BIT(2)) {
		if (policy->domain_name) {
			wpa_printf(MSG_ERROR,
				   "QM: IPv4: Both domain name and destination IP address not expected");
			return -1;
		}

		type4_param->ip_params.v4.param_mask |= BIT(2);
		os_memcpy(&type4_param->ip_params.v4.dst_ip,
			  &frame_classifier[7], 4);
	}

	/* Classifier Mask - bit 3 = Source Port */
	if (classifier_mask & BIT(3)) {
		type4_param->ip_params.v4.param_mask |= BIT(3);
		type4_param->ip_params.v4.src_port =
			WPA_GET_BE16(&frame_classifier[11]);
	}

	/* Classifier Mask - bit 4 = Destination Port */
	if (classifier_mask & BIT(4)) {
		if (policy->port_range_info) {
			wpa_printf(MSG_ERROR,
				   "QM: IPv4: Both port range and destination port not expected");
			return -1;
		}

		type4_param->ip_params.v4.param_mask |= BIT(4);
		type4_param->ip_params.v4.dst_port =
			WPA_GET_BE16(&frame_classifier[13]);
	}

	/* Classifier Mask - bit 5 = DSCP (ignored) */

	/* Classifier Mask - bit 6 = Protocol */
	if (classifier_mask & BIT(6)) {
		type4_param->ip_params.v4.param_mask |= BIT(6);
		type4_param->ip_params.v4.protocol = frame_classifier[16];
	}

	return 0;
}


static int set_frame_classifier_type4_ipv6(struct dscp_policy_data *policy)
{
	u8 classifier_mask;
	const u8 *frame_classifier = policy->frame_classifier;
	struct type4_params *type4_param = &policy->type4_param;

	if (policy->frame_classifier_len < 44) {
		wpa_printf(MSG_ERROR,
			   "QM: Received IPv6 frame classifier with insufficient length %d",
			   policy->frame_classifier_len);
		return -1;
	}

	classifier_mask = frame_classifier[1];

	/* Classifier Mask - bit 1 = Source IP Address */
	if (classifier_mask & BIT(1)) {
		type4_param->ip_params.v6.param_mask |= BIT(1);
		os_memcpy(&type4_param->ip_params.v6.src_ip,
			  &frame_classifier[3], 16);
	}

	/* Classifier Mask - bit 2 = Destination IP Address */
	if (classifier_mask & BIT(2)) {
		if (policy->domain_name) {
			wpa_printf(MSG_ERROR,
				   "QM: IPv6: Both domain name and destination IP address not expected");
			return -1;
		}
		type4_param->ip_params.v6.param_mask |= BIT(2);
		os_memcpy(&type4_param->ip_params.v6.dst_ip,
			  &frame_classifier[19], 16);
	}

	/* Classifier Mask - bit 3 = Source Port */
	if (classifier_mask & BIT(3)) {
		type4_param->ip_params.v6.param_mask |= BIT(3);
		type4_param->ip_params.v6.src_port =
				WPA_GET_BE16(&frame_classifier[35]);
	}

	/* Classifier Mask - bit 4 = Destination Port */
	if (classifier_mask & BIT(4)) {
		if (policy->port_range_info) {
			wpa_printf(MSG_ERROR,
				   "IPv6: Both port range and destination port not expected");
			return -1;
		}

		type4_param->ip_params.v6.param_mask |= BIT(4);
		type4_param->ip_params.v6.dst_port =
				WPA_GET_BE16(&frame_classifier[37]);
	}

	/* Classifier Mask - bit 5 = DSCP (ignored) */

	/* Classifier Mask - bit 6 = Next Header */
	if (classifier_mask & BIT(6)) {
		type4_param->ip_params.v6.param_mask |= BIT(6);
		type4_param->ip_params.v6.next_header = frame_classifier[40];
	}

	return 0;
}


static int wpas_set_frame_classifier_params(struct dscp_policy_data *policy)
{
	const u8 *frame_classifier = policy->frame_classifier;
	u8 frame_classifier_len = policy->frame_classifier_len;

	if (frame_classifier_len < 3) {
		wpa_printf(MSG_ERROR,
			   "QM: Received frame classifier with insufficient length %d",
			   frame_classifier_len);
		return -1;
	}

	/* Only allowed Classifier Type: IP and higher layer parameters (4) */
	if (frame_classifier[0] != 4) {
		wpa_printf(MSG_ERROR,
			   "QM: Received frame classifier with invalid classifier type %d",
			   frame_classifier[0]);
		return -1;
	}

	/* Classifier Mask - bit 0 = Version */
	if (!(frame_classifier[1] & BIT(0))) {
		wpa_printf(MSG_ERROR,
			   "QM: Received frame classifier without IP version");
		return -1;
	}

	/* Version (4 or 6) */
	if (frame_classifier[2] == 4) {
		if (set_frame_classifier_type4_ipv4(policy)) {
			wpa_printf(MSG_ERROR,
				   "QM: Failed to set IPv4 parameters");
			return -1;
		}

		policy->type4_param.ip_version = IPV4;
	} else if (frame_classifier[2] == 6) {
		if (set_frame_classifier_type4_ipv6(policy)) {
			wpa_printf(MSG_ERROR,
				   "QM: Failed to set IPv6 parameters");
			return -1;
		}

		policy->type4_param.ip_version = IPV6;
	} else {
		wpa_printf(MSG_ERROR,
			   "QM: Received unknown IP version %d",
			   frame_classifier[2]);
		return -1;
	}

	return 0;
}


static bool dscp_valid_domain_name(const char *str)
{
	if (!str[0])
		return false;

	while (*str) {
		if (is_ctrl_char(*str) || *str == ' ' || *str == '=')
			return false;
		str++;
	}

	return true;
}


static void wpas_add_dscp_policy(struct wpa_supplicant *wpa_s,
				 struct dscp_policy_data *policy)
{
	int ip_ver = 0, res;
	char policy_str[1000], *pos;
	int len;

	if (!policy->frame_classifier && !policy->domain_name &&
	    !policy->port_range_info) {
		wpa_printf(MSG_ERROR,
			   "QM: Invalid DSCP policy - no attributes present");
		goto fail;
	}

	policy_str[0] = '\0';
	pos = policy_str;
	len = sizeof(policy_str);

	if (policy->frame_classifier) {
		struct type4_params *type4 = &policy->type4_param;

		if (wpas_set_frame_classifier_params(policy)) {
			wpa_printf(MSG_ERROR,
				   "QM: Failed to set frame classifier parameters");
			goto fail;
		}

		if (type4->ip_version == IPV4)
			res = write_ipv4_info(pos, len, &type4->ip_params.v4);
		else
			res = write_ipv6_info(pos, len, &type4->ip_params.v6);

		if (res <= 0) {
			wpa_printf(MSG_ERROR,
				   "QM: Failed to write IP parameters");
			goto fail;
		}

		ip_ver = type4->ip_version;

		pos += res;
		len -= res;
	}

	if (policy->port_range_info) {
		res = os_snprintf(pos, len, " start_port=%u end_port=%u",
				  policy->start_port, policy->end_port);
		if (os_snprintf_error(len, res)) {
			wpa_printf(MSG_ERROR,
				   "QM: Failed to write port range attributes for policy id = %d",
				   policy->policy_id);
			goto fail;
		}

		pos += res;
		len -= res;
	}

	if (policy->domain_name) {
		char domain_name_str[250];

		if (policy->domain_name_len >= sizeof(domain_name_str)) {
			wpa_printf(MSG_ERROR,
				   "QM: Domain name length higher than max expected");
			goto fail;
		}
		os_memcpy(domain_name_str, policy->domain_name,
			  policy->domain_name_len);
		domain_name_str[policy->domain_name_len] = '\0';
		if (!dscp_valid_domain_name(domain_name_str)) {
			wpa_printf(MSG_ERROR, "QM: Invalid domain name string");
			goto fail;
		}
		res = os_snprintf(pos, len, " domain_name=%s", domain_name_str);
		if (os_snprintf_error(len, res)) {
			wpa_printf(MSG_ERROR,
				   "QM: Failed to write domain name attribute for policy id = %d",
				   policy->policy_id);
			goto fail;
		}
	}

	wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_DSCP_POLICY
		"add policy_id=%u dscp=%u ip_version=%d%s",
		policy->policy_id, policy->dscp, ip_ver, policy_str);
	return;
fail:
	wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_DSCP_POLICY "reject policy_id=%u",
		policy->policy_id);
}


void wpas_dscp_deinit(struct wpa_supplicant *wpa_s)
{
	wpa_printf(MSG_DEBUG, "QM: Clear all active DSCP policies");
	wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_DSCP_POLICY "clear_all");
	wpa_s->dscp_req_dialog_token = 0;
	wpa_s->dscp_query_dialog_token = 0;
	wpa_s->connection_dscp = 0;
	if (wpa_s->wait_for_dscp_req) {
		wpa_s->wait_for_dscp_req = 0;
		eloop_cancel_timeout(wpas_wait_for_dscp_req_timer, wpa_s, NULL);
	}
}


static void wpas_fill_dscp_policy(struct dscp_policy_data *policy, u8 attr_id,
				  u8 attr_len, const u8 *attr_data)
{
	switch (attr_id) {
	case QM_ATTR_PORT_RANGE:
		if (attr_len < 4) {
			wpa_printf(MSG_ERROR,
				   "QM: Received Port Range attribute with insufficient length %d",
				    attr_len);
			break;
		}
		policy->start_port = WPA_GET_BE16(attr_data);
		policy->end_port = WPA_GET_BE16(attr_data + 2);
		policy->port_range_info = true;
		break;
	case QM_ATTR_DSCP_POLICY:
		if (attr_len < 3) {
			wpa_printf(MSG_ERROR,
				   "QM: Received DSCP Policy attribute with insufficient length %d",
				   attr_len);
			return;
		}
		policy->policy_id = attr_data[0];
		policy->req_type = attr_data[1];
		policy->dscp = attr_data[2];
		policy->dscp_info = true;
		break;
	case QM_ATTR_TCLAS:
		if (attr_len < 1) {
			wpa_printf(MSG_ERROR,
				   "QM: Received TCLAS attribute with insufficient length %d",
				   attr_len);
			return;
		}
		policy->frame_classifier = attr_data;
		policy->frame_classifier_len = attr_len;
		break;
	case QM_ATTR_DOMAIN_NAME:
		if (attr_len < 1) {
			wpa_printf(MSG_ERROR,
				   "QM: Received domain name attribute with insufficient length %d",
				   attr_len);
			return;
		}
		policy->domain_name = attr_data;
		policy->domain_name_len = attr_len;
		break;
	default:
		wpa_printf(MSG_ERROR, "QM: Received invalid QoS attribute %d",
			   attr_id);
		break;
	}
}


void wpas_handle_qos_mgmt_recv_action(struct wpa_supplicant *wpa_s,
				      const u8 *src,
				      const u8 *buf, size_t len)
{
	int rem_len;
	const u8 *qos_ie, *attr;
	int more, reset;

	if (!wpa_s->enable_dscp_policy_capa) {
		wpa_printf(MSG_ERROR,
			   "QM: Ignore DSCP Policy frame since the capability is not enabled");
		return;
	}

	if (!pmf_in_use(wpa_s, src)) {
		wpa_printf(MSG_ERROR,
			   "QM: Ignore DSCP Policy frame since PMF is not in use");
		return;
	}

	if (!wpa_s->connection_dscp) {
		 wpa_printf(MSG_DEBUG,
			    "QM: DSCP Policy capability not enabled for the current association - ignore QoS Management Action frames");
		return;
	}

	if (len < 1)
		return;

	/* Handle only DSCP Policy Request frame */
	if (buf[0] != QM_DSCP_POLICY_REQ) {
		wpa_printf(MSG_ERROR, "QM: Received unexpected QoS action frame %d",
			   buf[0]);
		return;
	}

	if (len < 3) {
		wpa_printf(MSG_ERROR,
			   "Received QoS Management DSCP Policy Request frame with invalid length %zu",
			   len);
		return;
	}

	/* Clear wait_for_dscp_req on receiving first DSCP request from AP */
	if (wpa_s->wait_for_dscp_req) {
		wpa_s->wait_for_dscp_req = 0;
		eloop_cancel_timeout(wpas_wait_for_dscp_req_timer, wpa_s, NULL);
	}

	wpa_s->dscp_req_dialog_token = buf[1];
	more = buf[2] & DSCP_POLICY_CTRL_MORE;
	reset = buf[2] & DSCP_POLICY_CTRL_RESET;

	wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_DSCP_POLICY "request_start%s%s",
		reset ? " clear_all" : "", more ? " more" : "");

	qos_ie = buf + 3;
	rem_len = len - 3;
	while (rem_len > 2) {
		struct dscp_policy_data policy;
		int rem_attrs_len, ie_len;

		ie_len = 2 + qos_ie[1];
		if (rem_len < ie_len)
			break;

		if (rem_len < 6 || qos_ie[0] != WLAN_EID_VENDOR_SPECIFIC ||
		    qos_ie[1] < 4 ||
		    WPA_GET_BE32(&qos_ie[2]) != QM_IE_VENDOR_TYPE) {
			rem_len -= ie_len;
			qos_ie += ie_len;
			continue;
		}

		os_memset(&policy, 0, sizeof(struct dscp_policy_data));
		attr = qos_ie + 6;
		rem_attrs_len = qos_ie[1] - 4;

		while (rem_attrs_len > 2 && rem_attrs_len >= 2 + attr[1]) {
			wpas_fill_dscp_policy(&policy, attr[0], attr[1],
					      &attr[2]);
			rem_attrs_len -= 2 + attr[1];
			attr += 2 + attr[1];
		}

		rem_len -= ie_len;
		qos_ie += ie_len;

		if (!policy.dscp_info) {
			wpa_printf(MSG_ERROR,
				   "QM: Received QoS IE without DSCP Policy attribute");
			continue;
		}

		if (policy.req_type == DSCP_POLICY_REQ_ADD)
			wpas_add_dscp_policy(wpa_s, &policy);
		else if (policy.req_type == DSCP_POLICY_REQ_REMOVE)
			wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_DSCP_POLICY
				"remove policy_id=%u", policy.policy_id);
		else
			wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_DSCP_POLICY
				"reject policy_id=%u", policy.policy_id);
	}

	wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_DSCP_POLICY "request_end");
}


int wpas_send_dscp_response(struct wpa_supplicant *wpa_s,
			    struct dscp_resp_data *resp_data)
{
	struct wpabuf *buf = NULL;
	size_t buf_len;
	int ret = -1, i;
	u8 resp_control = 0;

	if (wpa_s->wpa_state != WPA_COMPLETED || !wpa_s->current_ssid) {
		wpa_printf(MSG_ERROR,
			   "QM: Failed to send DSCP response - not connected to AP");
		return -1;
	}

	if (resp_data->solicited && !wpa_s->dscp_req_dialog_token) {
		wpa_printf(MSG_ERROR, "QM: No ongoing DSCP request");
		return -1;
	}

	if (!wpa_s->connection_dscp) {
		wpa_printf(MSG_ERROR,
			   "QM: Failed to send DSCP response - DSCP capability not enabled for the current association");
		return -1;

	}

	buf_len = 1 +	/* Category */
		  3 +	/* OUI */
		  1 +	/* OUI Type */
		  1 +	/* OUI Subtype */
		  1 +	/* Dialog Token */
		  1 +	/* Response Control */
		  1 +	/* Count */
		  2 * resp_data->num_policies;  /* Status list */
	buf = wpabuf_alloc(buf_len);
	if (!buf) {
		wpa_printf(MSG_ERROR,
			   "QM: Failed to allocate DSCP policy response");
		return -1;
	}

	wpabuf_put_u8(buf, WLAN_ACTION_VENDOR_SPECIFIC_PROTECTED);
	wpabuf_put_be24(buf, OUI_WFA);
	wpabuf_put_u8(buf, QM_ACTION_OUI_TYPE);
	wpabuf_put_u8(buf, QM_DSCP_POLICY_RESP);

	wpabuf_put_u8(buf, resp_data->solicited ?
		      wpa_s->dscp_req_dialog_token : 0);

	if (resp_data->more)
		resp_control |= DSCP_POLICY_CTRL_MORE;
	if (resp_data->reset)
		resp_control |= DSCP_POLICY_CTRL_RESET;
	wpabuf_put_u8(buf, resp_control);

	wpabuf_put_u8(buf, resp_data->num_policies);
	for (i = 0; i < resp_data->num_policies; i++) {
		wpabuf_put_u8(buf, resp_data->policy[i].id);
		wpabuf_put_u8(buf, resp_data->policy[i].status);
	}

	wpa_hexdump_buf(MSG_MSGDUMP, "DSCP response frame: ", buf);
	ret = wpa_drv_send_action(wpa_s, wpa_s->assoc_freq, 0, wpa_s->bssid,
				  wpa_s->own_addr, wpa_s->bssid,
				  wpabuf_head(buf), wpabuf_len(buf), 0);
	if (ret < 0) {
		wpa_msg(wpa_s, MSG_INFO, "QM: Failed to send DSCP response");
		goto fail;
	}

	/*
	 * Mark DSCP request complete whether response sent is solicited or
	 * unsolicited
	 */
	wpa_s->dscp_req_dialog_token = 0;

fail:
	wpabuf_free(buf);
	return ret;
}


int wpas_send_dscp_query(struct wpa_supplicant *wpa_s, const char *domain_name,
			 size_t domain_name_length)
{
	struct wpabuf *buf = NULL;
	int ret, dscp_query_size;

	if (wpa_s->wpa_state != WPA_COMPLETED || !wpa_s->current_ssid)
		return -1;

	if (!wpa_s->connection_dscp) {
		wpa_printf(MSG_ERROR,
			   "QM: Failed to send DSCP query - DSCP capability not enabled for the current association");
		return -1;
	}

	if (wpa_s->wait_for_dscp_req) {
		wpa_printf(MSG_INFO, "QM: Wait until AP sends a DSCP request");
		return -1;
	}

#define DOMAIN_NAME_OFFSET (4 /* OUI */ + 1 /* Attr Id */ + 1 /* Attr len */)

	if (domain_name_length > 255 - DOMAIN_NAME_OFFSET) {
		wpa_printf(MSG_ERROR, "QM: Too long domain name");
		return -1;
	}

	dscp_query_size = 1 + /* Category */
			  4 + /* OUI Type */
			  1 + /* OUI subtype */
			  1; /* Dialog Token */
	if (domain_name && domain_name_length)
		dscp_query_size += 1 + /* Element ID */
			1 + /* IE Length */
			DOMAIN_NAME_OFFSET + domain_name_length;

	buf = wpabuf_alloc(dscp_query_size);
	if (!buf) {
		wpa_printf(MSG_ERROR, "QM: Failed to allocate DSCP query");
		return -1;
	}

	wpabuf_put_u8(buf, WLAN_ACTION_VENDOR_SPECIFIC_PROTECTED);
	wpabuf_put_be32(buf, QM_ACTION_VENDOR_TYPE);
	wpabuf_put_u8(buf, QM_DSCP_POLICY_QUERY);
	wpa_s->dscp_query_dialog_token++;
	if (wpa_s->dscp_query_dialog_token == 0)
		wpa_s->dscp_query_dialog_token++;
	wpabuf_put_u8(buf, wpa_s->dscp_query_dialog_token);

	if (domain_name && domain_name_length) {
		/* Domain Name attribute */
		wpabuf_put_u8(buf, WLAN_EID_VENDOR_SPECIFIC);
		wpabuf_put_u8(buf, DOMAIN_NAME_OFFSET + domain_name_length);
		wpabuf_put_be32(buf, QM_IE_VENDOR_TYPE);
		wpabuf_put_u8(buf, QM_ATTR_DOMAIN_NAME);
		wpabuf_put_u8(buf, domain_name_length);
		wpabuf_put_data(buf, domain_name, domain_name_length);
	}
#undef DOMAIN_NAME_OFFSET

	ret = wpa_drv_send_action(wpa_s, wpa_s->assoc_freq, 0, wpa_s->bssid,
				  wpa_s->own_addr, wpa_s->bssid,
				  wpabuf_head(buf), wpabuf_len(buf), 0);
	if (ret < 0) {
		wpa_dbg(wpa_s, MSG_ERROR, "QM: Failed to send DSCP query");
		wpa_s->dscp_query_dialog_token--;
	}

	wpabuf_free(buf);
	return ret;
}
