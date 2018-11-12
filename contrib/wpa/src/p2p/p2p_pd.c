/*
 * Wi-Fi Direct - P2P provision discovery
 * Copyright (c) 2009-2010, Atheros Communications
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "common/ieee802_11_defs.h"
#include "wps/wps_defs.h"
#include "p2p_i.h"
#include "p2p.h"


/*
 * Number of retries to attempt for provision discovery requests
 * in case the peer is not listening.
 */
#define MAX_PROV_DISC_REQ_RETRIES 120


static void p2p_build_wps_ie_config_methods(struct wpabuf *buf,
					    u16 config_methods)
{
	u8 *len;
	wpabuf_put_u8(buf, WLAN_EID_VENDOR_SPECIFIC);
	len = wpabuf_put(buf, 1);
	wpabuf_put_be32(buf, WPS_DEV_OUI_WFA);

	/* Config Methods */
	wpabuf_put_be16(buf, ATTR_CONFIG_METHODS);
	wpabuf_put_be16(buf, 2);
	wpabuf_put_be16(buf, config_methods);

	p2p_buf_update_ie_hdr(buf, len);
}


static struct wpabuf * p2p_build_prov_disc_req(struct p2p_data *p2p,
					       u8 dialog_token,
					       u16 config_methods,
					       struct p2p_device *go)
{
	struct wpabuf *buf;
	u8 *len;
	size_t extra = 0;

#ifdef CONFIG_WIFI_DISPLAY
	if (p2p->wfd_ie_prov_disc_req)
		extra = wpabuf_len(p2p->wfd_ie_prov_disc_req);
#endif /* CONFIG_WIFI_DISPLAY */

	buf = wpabuf_alloc(1000 + extra);
	if (buf == NULL)
		return NULL;

	p2p_buf_add_public_action_hdr(buf, P2P_PROV_DISC_REQ, dialog_token);

	len = p2p_buf_add_ie_hdr(buf);
	p2p_buf_add_capability(buf, p2p->dev_capab &
			       ~P2P_DEV_CAPAB_CLIENT_DISCOVERABILITY, 0);
	p2p_buf_add_device_info(buf, p2p, NULL);
	if (go) {
		p2p_buf_add_group_id(buf, go->info.p2p_device_addr,
				     go->oper_ssid, go->oper_ssid_len);
	}
	p2p_buf_update_ie_hdr(buf, len);

	/* WPS IE with Config Methods attribute */
	p2p_build_wps_ie_config_methods(buf, config_methods);

#ifdef CONFIG_WIFI_DISPLAY
	if (p2p->wfd_ie_prov_disc_req)
		wpabuf_put_buf(buf, p2p->wfd_ie_prov_disc_req);
#endif /* CONFIG_WIFI_DISPLAY */

	return buf;
}


static struct wpabuf * p2p_build_prov_disc_resp(struct p2p_data *p2p,
						u8 dialog_token,
						u16 config_methods,
						const u8 *group_id,
						size_t group_id_len)
{
	struct wpabuf *buf;
	size_t extra = 0;

#ifdef CONFIG_WIFI_DISPLAY
	struct wpabuf *wfd_ie = p2p->wfd_ie_prov_disc_resp;
	if (wfd_ie && group_id) {
		size_t i;
		for (i = 0; i < p2p->num_groups; i++) {
			struct p2p_group *g = p2p->groups[i];
			struct wpabuf *ie;
			if (!p2p_group_is_group_id_match(g, group_id,
							 group_id_len))
				continue;
			ie = p2p_group_get_wfd_ie(g);
			if (ie) {
				wfd_ie = ie;
				break;
			}
		}
	}
	if (wfd_ie)
		extra = wpabuf_len(wfd_ie);
#endif /* CONFIG_WIFI_DISPLAY */

	buf = wpabuf_alloc(100 + extra);
	if (buf == NULL)
		return NULL;

	p2p_buf_add_public_action_hdr(buf, P2P_PROV_DISC_RESP, dialog_token);

	/* WPS IE with Config Methods attribute */
	p2p_build_wps_ie_config_methods(buf, config_methods);

#ifdef CONFIG_WIFI_DISPLAY
	if (wfd_ie)
		wpabuf_put_buf(buf, wfd_ie);
#endif /* CONFIG_WIFI_DISPLAY */

	return buf;
}


void p2p_process_prov_disc_req(struct p2p_data *p2p, const u8 *sa,
			       const u8 *data, size_t len, int rx_freq)
{
	struct p2p_message msg;
	struct p2p_device *dev;
	int freq;
	int reject = 1;
	struct wpabuf *resp;

	if (p2p_parse(data, len, &msg))
		return;

	wpa_msg(p2p->cfg->msg_ctx, MSG_DEBUG,
		"P2P: Received Provision Discovery Request from " MACSTR
		" with config methods 0x%x (freq=%d)",
		MAC2STR(sa), msg.wps_config_methods, rx_freq);

	dev = p2p_get_device(p2p, sa);
	if (dev == NULL || (dev->flags & P2P_DEV_PROBE_REQ_ONLY)) {
		wpa_msg(p2p->cfg->msg_ctx, MSG_DEBUG,
			"P2P: Provision Discovery Request from "
			"unknown peer " MACSTR, MAC2STR(sa));

		if (p2p_add_device(p2p, sa, rx_freq, 0, 0, data + 1, len - 1,
				   0)) {
			wpa_msg(p2p->cfg->msg_ctx, MSG_DEBUG,
			        "P2P: Provision Discovery Request add device "
				"failed " MACSTR, MAC2STR(sa));
		}
	} else if (msg.wfd_subelems) {
		wpabuf_free(dev->info.wfd_subelems);
		dev->info.wfd_subelems = wpabuf_dup(msg.wfd_subelems);
	}

	if (!(msg.wps_config_methods &
	      (WPS_CONFIG_DISPLAY | WPS_CONFIG_KEYPAD |
	       WPS_CONFIG_PUSHBUTTON))) {
		wpa_msg(p2p->cfg->msg_ctx, MSG_DEBUG, "P2P: Unsupported "
			"Config Methods in Provision Discovery Request");
		goto out;
	}

	if (msg.group_id) {
		size_t i;
		for (i = 0; i < p2p->num_groups; i++) {
			if (p2p_group_is_group_id_match(p2p->groups[i],
							msg.group_id,
							msg.group_id_len))
				break;
		}
		if (i == p2p->num_groups) {
			wpa_msg(p2p->cfg->msg_ctx, MSG_DEBUG, "P2P: PD "
				"request for unknown P2P Group ID - reject");
			goto out;
		}
	}

	if (dev)
		dev->flags &= ~(P2P_DEV_PD_PEER_DISPLAY |
				P2P_DEV_PD_PEER_KEYPAD);
	if (msg.wps_config_methods & WPS_CONFIG_DISPLAY) {
		wpa_msg(p2p->cfg->msg_ctx, MSG_DEBUG, "P2P: Peer " MACSTR
			" requested us to show a PIN on display", MAC2STR(sa));
		if (dev)
			dev->flags |= P2P_DEV_PD_PEER_KEYPAD;
	} else if (msg.wps_config_methods & WPS_CONFIG_KEYPAD) {
		wpa_msg(p2p->cfg->msg_ctx, MSG_DEBUG, "P2P: Peer " MACSTR
			" requested us to write its PIN using keypad",
			MAC2STR(sa));
		if (dev)
			dev->flags |= P2P_DEV_PD_PEER_DISPLAY;
	}

	reject = 0;

out:
	resp = p2p_build_prov_disc_resp(p2p, msg.dialog_token,
					reject ? 0 : msg.wps_config_methods,
					msg.group_id, msg.group_id_len);
	if (resp == NULL) {
		p2p_parse_free(&msg);
		return;
	}
	wpa_msg(p2p->cfg->msg_ctx, MSG_DEBUG,
		"P2P: Sending Provision Discovery Response");
	if (rx_freq > 0)
		freq = rx_freq;
	else
		freq = p2p_channel_to_freq(p2p->cfg->country,
					   p2p->cfg->reg_class,
					   p2p->cfg->channel);
	if (freq < 0) {
		wpa_msg(p2p->cfg->msg_ctx, MSG_DEBUG,
			"P2P: Unknown regulatory class/channel");
		wpabuf_free(resp);
		p2p_parse_free(&msg);
		return;
	}
	p2p->pending_action_state = P2P_NO_PENDING_ACTION;
	if (p2p_send_action(p2p, freq, sa, p2p->cfg->dev_addr,
			    p2p->cfg->dev_addr,
			    wpabuf_head(resp), wpabuf_len(resp), 200) < 0) {
		wpa_msg(p2p->cfg->msg_ctx, MSG_DEBUG,
			"P2P: Failed to send Action frame");
	}

	wpabuf_free(resp);

	if (!reject && p2p->cfg->prov_disc_req) {
		const u8 *dev_addr = sa;
		if (msg.p2p_device_addr)
			dev_addr = msg.p2p_device_addr;
		p2p->cfg->prov_disc_req(p2p->cfg->cb_ctx, sa,
					msg.wps_config_methods,
					dev_addr, msg.pri_dev_type,
					msg.device_name, msg.config_methods,
					msg.capability ? msg.capability[0] : 0,
					msg.capability ? msg.capability[1] :
					0,
					msg.group_id, msg.group_id_len);
	}
	p2p_parse_free(&msg);
}


void p2p_process_prov_disc_resp(struct p2p_data *p2p, const u8 *sa,
				const u8 *data, size_t len)
{
	struct p2p_message msg;
	struct p2p_device *dev;
	u16 report_config_methods = 0;
	int success = 0;

	if (p2p_parse(data, len, &msg))
		return;

	wpa_msg(p2p->cfg->msg_ctx, MSG_DEBUG,
		"P2P: Received Provision Discovery Response from " MACSTR
		" with config methods 0x%x",
		MAC2STR(sa), msg.wps_config_methods);

	dev = p2p_get_device(p2p, sa);
	if (dev == NULL || !dev->req_config_methods) {
		wpa_msg(p2p->cfg->msg_ctx, MSG_DEBUG,
			"P2P: Ignore Provision Discovery Response from "
			MACSTR " with no pending request", MAC2STR(sa));
		p2p_parse_free(&msg);
		return;
	}

	if (dev->dialog_token != msg.dialog_token) {
		wpa_msg(p2p->cfg->msg_ctx, MSG_DEBUG,
			"P2P: Ignore Provision Discovery Response with "
			"unexpected Dialog Token %u (expected %u)",
			msg.dialog_token, dev->dialog_token);
		p2p_parse_free(&msg);
		return;
	}

	if (p2p->pending_action_state == P2P_PENDING_PD) {
		os_memset(p2p->pending_pd_devaddr, 0, ETH_ALEN);
		p2p->pending_action_state = P2P_NO_PENDING_ACTION;
	}

	/*
	 * If the response is from the peer to whom a user initiated request
	 * was sent earlier, we reset that state info here.
	 */
	if (p2p->user_initiated_pd &&
	    os_memcmp(p2p->pending_pd_devaddr, sa, ETH_ALEN) == 0)
		p2p_reset_pending_pd(p2p);

	if (msg.wps_config_methods != dev->req_config_methods) {
		wpa_msg(p2p->cfg->msg_ctx, MSG_DEBUG, "P2P: Peer rejected "
			"our Provision Discovery Request");
		if (p2p->cfg->prov_disc_fail)
			p2p->cfg->prov_disc_fail(p2p->cfg->cb_ctx, sa,
						 P2P_PROV_DISC_REJECTED);
		p2p_parse_free(&msg);
		goto out;
	}

	report_config_methods = dev->req_config_methods;
	dev->flags &= ~(P2P_DEV_PD_PEER_DISPLAY |
			P2P_DEV_PD_PEER_KEYPAD);
	if (dev->req_config_methods & WPS_CONFIG_DISPLAY) {
		wpa_msg(p2p->cfg->msg_ctx, MSG_DEBUG, "P2P: Peer " MACSTR
			" accepted to show a PIN on display", MAC2STR(sa));
		dev->flags |= P2P_DEV_PD_PEER_DISPLAY;
	} else if (msg.wps_config_methods & WPS_CONFIG_KEYPAD) {
		wpa_msg(p2p->cfg->msg_ctx, MSG_DEBUG, "P2P: Peer " MACSTR
			" accepted to write our PIN using keypad",
			MAC2STR(sa));
		dev->flags |= P2P_DEV_PD_PEER_KEYPAD;
	}

	/* Store the provisioning info */
	dev->wps_prov_info = msg.wps_config_methods;

	p2p_parse_free(&msg);
	success = 1;

out:
	dev->req_config_methods = 0;
	p2p->cfg->send_action_done(p2p->cfg->cb_ctx);
	if (dev->flags & P2P_DEV_PD_BEFORE_GO_NEG) {
		wpa_msg(p2p->cfg->msg_ctx, MSG_DEBUG,
			"P2P: Start GO Neg after the PD-before-GO-Neg "
			"workaround with " MACSTR,
			MAC2STR(dev->info.p2p_device_addr));
		dev->flags &= ~P2P_DEV_PD_BEFORE_GO_NEG;
		p2p_connect_send(p2p, dev);
		return;
	}
	if (success && p2p->cfg->prov_disc_resp)
		p2p->cfg->prov_disc_resp(p2p->cfg->cb_ctx, sa,
					 report_config_methods);
}


int p2p_send_prov_disc_req(struct p2p_data *p2p, struct p2p_device *dev,
			   int join, int force_freq)
{
	struct wpabuf *req;
	int freq;

	if (force_freq > 0)
		freq = force_freq;
	else
		freq = dev->listen_freq > 0 ? dev->listen_freq :
			dev->oper_freq;
	if (freq <= 0) {
		wpa_msg(p2p->cfg->msg_ctx, MSG_DEBUG,
			"P2P: No Listen/Operating frequency known for the "
			"peer " MACSTR " to send Provision Discovery Request",
			MAC2STR(dev->info.p2p_device_addr));
		return -1;
	}

	if (dev->flags & P2P_DEV_GROUP_CLIENT_ONLY) {
		if (!(dev->info.dev_capab &
		      P2P_DEV_CAPAB_CLIENT_DISCOVERABILITY)) {
			wpa_msg(p2p->cfg->msg_ctx, MSG_DEBUG,
				"P2P: Cannot use PD with P2P Device " MACSTR
				" that is in a group and is not discoverable",
				MAC2STR(dev->info.p2p_device_addr));
			return -1;
		}
		/* TODO: use device discoverability request through GO */
	}

	req = p2p_build_prov_disc_req(p2p, dev->dialog_token,
				      dev->req_config_methods,
				      join ? dev : NULL);
	if (req == NULL)
		return -1;

	if (p2p->state != P2P_IDLE)
		p2p_stop_listen_for_freq(p2p, freq);
	p2p->pending_action_state = P2P_PENDING_PD;
	if (p2p_send_action(p2p, freq, dev->info.p2p_device_addr,
			    p2p->cfg->dev_addr, dev->info.p2p_device_addr,
			    wpabuf_head(req), wpabuf_len(req), 200) < 0) {
		wpa_msg(p2p->cfg->msg_ctx, MSG_DEBUG,
			"P2P: Failed to send Action frame");
		wpabuf_free(req);
		return -1;
	}

	os_memcpy(p2p->pending_pd_devaddr, dev->info.p2p_device_addr, ETH_ALEN);

	wpabuf_free(req);
	return 0;
}


int p2p_prov_disc_req(struct p2p_data *p2p, const u8 *peer_addr,
		      u16 config_methods, int join, int force_freq,
		      int user_initiated_pd)
{
	struct p2p_device *dev;

	dev = p2p_get_device(p2p, peer_addr);
	if (dev == NULL)
		dev = p2p_get_device_interface(p2p, peer_addr);
	if (dev == NULL || (dev->flags & P2P_DEV_PROBE_REQ_ONLY)) {
		wpa_msg(p2p->cfg->msg_ctx, MSG_DEBUG, "P2P: Provision "
			"Discovery Request destination " MACSTR
			" not yet known", MAC2STR(peer_addr));
		return -1;
	}

	wpa_msg(p2p->cfg->msg_ctx, MSG_DEBUG, "P2P: Provision Discovery "
		"Request with " MACSTR " (config methods 0x%x)",
		MAC2STR(peer_addr), config_methods);
	if (config_methods == 0)
		return -1;

	/* Reset provisioning info */
	dev->wps_prov_info = 0;

	dev->req_config_methods = config_methods;
	if (join)
		dev->flags |= P2P_DEV_PD_FOR_JOIN;
	else
		dev->flags &= ~P2P_DEV_PD_FOR_JOIN;

	if (p2p->state != P2P_IDLE && p2p->state != P2P_SEARCH &&
	    p2p->state != P2P_LISTEN_ONLY) {
		wpa_msg(p2p->cfg->msg_ctx, MSG_DEBUG, "P2P: Busy with other "
			"operations; postpone Provision Discovery Request "
			"with " MACSTR " (config methods 0x%x)",
			MAC2STR(peer_addr), config_methods);
		return 0;
	}

	p2p->user_initiated_pd = user_initiated_pd;

	if (p2p->user_initiated_pd)
		p2p->pd_retries = MAX_PROV_DISC_REQ_RETRIES;

	/*
	 * Assign dialog token here to use the same value in each retry within
	 * the same PD exchange.
	 */
	dev->dialog_token++;
	if (dev->dialog_token == 0)
		dev->dialog_token = 1;

	return p2p_send_prov_disc_req(p2p, dev, join, force_freq);
}


void p2p_reset_pending_pd(struct p2p_data *p2p)
{
	struct p2p_device *dev;

	dl_list_for_each(dev, &p2p->devices, struct p2p_device, list) {
		if (os_memcmp(p2p->pending_pd_devaddr,
			      dev->info.p2p_device_addr, ETH_ALEN))
			continue;
		if (!dev->req_config_methods)
			continue;
		if (dev->flags & P2P_DEV_PD_FOR_JOIN)
			continue;
		/* Reset the config methods of the device */
		dev->req_config_methods = 0;
	}

	p2p->user_initiated_pd = 0;
	os_memset(p2p->pending_pd_devaddr, 0, ETH_ALEN);
	p2p->pd_retries = 0;
}
