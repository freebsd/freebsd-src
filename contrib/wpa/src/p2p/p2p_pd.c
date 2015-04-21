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
#include "common/wpa_ctrl.h"
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


static void p2ps_add_new_group_info(struct p2p_data *p2p, struct wpabuf *buf)
{
	int found;
	u8 intended_addr[ETH_ALEN];
	u8 ssid[32];
	size_t ssid_len;
	int group_iface;

	if (!p2p->cfg->get_go_info)
		return;

	found = p2p->cfg->get_go_info(
		p2p->cfg->cb_ctx, intended_addr, ssid,
		&ssid_len, &group_iface);
	if (found) {
		p2p_buf_add_group_id(buf, p2p->cfg->dev_addr,
				     ssid, ssid_len);
		p2p_buf_add_intended_addr(buf, intended_addr);
	} else {
		if (!p2p->ssid_set) {
			p2p_build_ssid(p2p, p2p->ssid, &p2p->ssid_len);
			p2p->ssid_set = 1;
		}

		/* Add pre-composed P2P Group ID */
		p2p_buf_add_group_id(buf, p2p->cfg->dev_addr,
				     p2p->ssid, p2p->ssid_len);

		if (group_iface)
			p2p_buf_add_intended_addr(
				buf, p2p->intended_addr);
		else
			p2p_buf_add_intended_addr(
				buf, p2p->cfg->dev_addr);
	}
}


static void p2ps_add_pd_req_attrs(struct p2p_data *p2p, struct p2p_device *dev,
				  struct wpabuf *buf, u16 config_methods)
{
	struct p2ps_provision *prov = p2p->p2ps_prov;
	u8 feat_cap_mask[] = { 1, 0 };
	int shared_group = 0;
	u8 ssid[32];
	size_t ssid_len;
	u8 go_dev_addr[ETH_ALEN];

	/* If we might be explicite group owner, add GO details */
	if (prov->conncap & (P2PS_SETUP_GROUP_OWNER |
			     P2PS_SETUP_NEW))
		p2ps_add_new_group_info(p2p, buf);

	if (prov->status >= 0)
		p2p_buf_add_status(buf, (u8) prov->status);
	else
		prov->method = config_methods;

	if (p2p->cfg->get_persistent_group) {
		shared_group = p2p->cfg->get_persistent_group(
			p2p->cfg->cb_ctx, dev->info.p2p_device_addr, NULL, 0,
			go_dev_addr, ssid, &ssid_len);
	}

	/* Add Operating Channel if conncap includes GO */
	if (shared_group ||
	    (prov->conncap & (P2PS_SETUP_GROUP_OWNER |
			      P2PS_SETUP_NEW))) {
		u8 tmp;

		p2p_go_select_channel(p2p, dev, &tmp);

		if (p2p->op_reg_class && p2p->op_channel)
			p2p_buf_add_operating_channel(buf, p2p->cfg->country,
						      p2p->op_reg_class,
						      p2p->op_channel);
		else
			p2p_buf_add_operating_channel(buf, p2p->cfg->country,
						      p2p->cfg->op_reg_class,
						      p2p->cfg->op_channel);
	}

	p2p_buf_add_channel_list(buf, p2p->cfg->country, &p2p->cfg->channels);

	if (prov->info[0])
		p2p_buf_add_session_info(buf, prov->info);

	p2p_buf_add_connection_capability(buf, prov->conncap);

	p2p_buf_add_advertisement_id(buf, prov->adv_id, prov->adv_mac);

	if (shared_group || prov->conncap == P2PS_SETUP_NEW ||
	    prov->conncap ==
	    (P2PS_SETUP_GROUP_OWNER | P2PS_SETUP_NEW) ||
	    prov->conncap ==
	    (P2PS_SETUP_GROUP_OWNER | P2PS_SETUP_CLIENT)) {
		/* Add Config Timeout */
		p2p_buf_add_config_timeout(buf, p2p->go_timeout,
					   p2p->client_timeout);
	}

	p2p_buf_add_listen_channel(buf, p2p->cfg->country, p2p->cfg->reg_class,
				   p2p->cfg->channel);

	p2p_buf_add_session_id(buf, prov->session_id, prov->session_mac);

	p2p_buf_add_feature_capability(buf, sizeof(feat_cap_mask),
				       feat_cap_mask);

	if (shared_group)
		p2p_buf_add_persistent_group_info(buf, go_dev_addr,
						  ssid, ssid_len);
}


static struct wpabuf * p2p_build_prov_disc_req(struct p2p_data *p2p,
					       struct p2p_device *dev,
					       int join)
{
	struct wpabuf *buf;
	u8 *len;
	size_t extra = 0;
	u8 dialog_token = dev->dialog_token;
	u16 config_methods = dev->req_config_methods;
	struct p2p_device *go = join ? dev : NULL;
	u8 group_capab;

#ifdef CONFIG_WIFI_DISPLAY
	if (p2p->wfd_ie_prov_disc_req)
		extra = wpabuf_len(p2p->wfd_ie_prov_disc_req);
#endif /* CONFIG_WIFI_DISPLAY */

	if (p2p->vendor_elem && p2p->vendor_elem[VENDOR_ELEM_P2P_PD_REQ])
		extra += wpabuf_len(p2p->vendor_elem[VENDOR_ELEM_P2P_PD_REQ]);

	if (p2p->p2ps_prov)
		extra += os_strlen(p2p->p2ps_prov->info) + 1 +
			sizeof(struct p2ps_provision);

	buf = wpabuf_alloc(1000 + extra);
	if (buf == NULL)
		return NULL;

	p2p_buf_add_public_action_hdr(buf, P2P_PROV_DISC_REQ, dialog_token);

	len = p2p_buf_add_ie_hdr(buf);

	group_capab = 0;
	if (p2p->p2ps_prov) {
		group_capab |= P2P_GROUP_CAPAB_PERSISTENT_GROUP;
		group_capab |= P2P_GROUP_CAPAB_PERSISTENT_RECONN;
		if (p2p->cross_connect)
			group_capab |= P2P_GROUP_CAPAB_CROSS_CONN;
		if (p2p->cfg->p2p_intra_bss)
			group_capab |= P2P_GROUP_CAPAB_INTRA_BSS_DIST;
	}
	p2p_buf_add_capability(buf, p2p->dev_capab &
			       ~P2P_DEV_CAPAB_CLIENT_DISCOVERABILITY,
			       group_capab);
	p2p_buf_add_device_info(buf, p2p, NULL);
	if (p2p->p2ps_prov) {
		p2ps_add_pd_req_attrs(p2p, dev, buf, config_methods);
	} else if (go) {
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

	if (p2p->vendor_elem && p2p->vendor_elem[VENDOR_ELEM_P2P_PD_REQ])
		wpabuf_put_buf(buf, p2p->vendor_elem[VENDOR_ELEM_P2P_PD_REQ]);

	return buf;
}


static struct wpabuf * p2p_build_prov_disc_resp(struct p2p_data *p2p,
						struct p2p_device *dev,
						u8 dialog_token,
						enum p2p_status_code status,
						u16 config_methods,
						u32 adv_id,
						const u8 *group_id,
						size_t group_id_len,
						const u8 *persist_ssid,
						size_t persist_ssid_len)
{
	struct wpabuf *buf;
	size_t extra = 0;
	int persist = 0;

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

	if (p2p->vendor_elem && p2p->vendor_elem[VENDOR_ELEM_P2P_PD_RESP])
		extra += wpabuf_len(p2p->vendor_elem[VENDOR_ELEM_P2P_PD_RESP]);

	buf = wpabuf_alloc(1000 + extra);
	if (buf == NULL)
		return NULL;

	p2p_buf_add_public_action_hdr(buf, P2P_PROV_DISC_RESP, dialog_token);

	/* Add P2P IE for P2PS */
	if (p2p->p2ps_prov && p2p->p2ps_prov->adv_id == adv_id) {
		u8 feat_cap_mask[] = { 1, 0 };
		u8 *len = p2p_buf_add_ie_hdr(buf);
		struct p2ps_provision *prov = p2p->p2ps_prov;
		u8 group_capab;

		if (!status && prov->status != -1)
			status = prov->status;

		p2p_buf_add_status(buf, status);
		group_capab = P2P_GROUP_CAPAB_PERSISTENT_GROUP |
			P2P_GROUP_CAPAB_PERSISTENT_RECONN;
		if (p2p->cross_connect)
			group_capab |= P2P_GROUP_CAPAB_CROSS_CONN;
		if (p2p->cfg->p2p_intra_bss)
			group_capab |= P2P_GROUP_CAPAB_INTRA_BSS_DIST;
		p2p_buf_add_capability(buf, p2p->dev_capab &
				       ~P2P_DEV_CAPAB_CLIENT_DISCOVERABILITY,
				       group_capab);
		p2p_buf_add_device_info(buf, p2p, NULL);

		if (persist_ssid && p2p->cfg->get_persistent_group &&
		    (status == P2P_SC_SUCCESS ||
		     status == P2P_SC_SUCCESS_DEFERRED)) {
			u8 ssid[32];
			size_t ssid_len;
			u8 go_dev_addr[ETH_ALEN];

			persist = p2p->cfg->get_persistent_group(
				p2p->cfg->cb_ctx,
				dev->info.p2p_device_addr,
				persist_ssid, persist_ssid_len, go_dev_addr,
				ssid, &ssid_len);
			if (persist)
				p2p_buf_add_persistent_group_info(
					buf, go_dev_addr, ssid, ssid_len);
		}

		if (!persist && (prov->conncap & P2PS_SETUP_GROUP_OWNER))
			p2ps_add_new_group_info(p2p, buf);

		/* Add Operating Channel if conncap indicates GO */
		if (persist || (prov->conncap & P2PS_SETUP_GROUP_OWNER)) {
			u8 tmp;

			if (dev)
				p2p_go_select_channel(p2p, dev, &tmp);

			if (p2p->op_reg_class && p2p->op_channel)
				p2p_buf_add_operating_channel(
					buf, p2p->cfg->country,
					p2p->op_reg_class,
					p2p->op_channel);
			else
				p2p_buf_add_operating_channel(
					buf, p2p->cfg->country,
					p2p->cfg->op_reg_class,
					p2p->cfg->op_channel);
		}

		p2p_buf_add_channel_list(buf, p2p->cfg->country,
					 &p2p->cfg->channels);

		if (!persist && (status == P2P_SC_SUCCESS ||
				 status == P2P_SC_SUCCESS_DEFERRED))
			p2p_buf_add_connection_capability(buf, prov->conncap);

		p2p_buf_add_advertisement_id(buf, adv_id, prov->adv_mac);

		p2p_buf_add_config_timeout(buf, p2p->go_timeout,
					   p2p->client_timeout);

		p2p_buf_add_session_id(buf, prov->session_id,
				       prov->session_mac);

		p2p_buf_add_feature_capability(buf, sizeof(feat_cap_mask),
					       feat_cap_mask);
		p2p_buf_update_ie_hdr(buf, len);
	} else if (status != P2P_SC_SUCCESS || adv_id) {
		u8 *len = p2p_buf_add_ie_hdr(buf);

		p2p_buf_add_status(buf, status);

		if (p2p->p2ps_prov)
			p2p_buf_add_advertisement_id(buf, adv_id,
						     p2p->p2ps_prov->adv_mac);

		p2p_buf_update_ie_hdr(buf, len);
	}

	/* WPS IE with Config Methods attribute */
	p2p_build_wps_ie_config_methods(buf, config_methods);

#ifdef CONFIG_WIFI_DISPLAY
	if (wfd_ie)
		wpabuf_put_buf(buf, wfd_ie);
#endif /* CONFIG_WIFI_DISPLAY */

	if (p2p->vendor_elem && p2p->vendor_elem[VENDOR_ELEM_P2P_PD_RESP])
		wpabuf_put_buf(buf, p2p->vendor_elem[VENDOR_ELEM_P2P_PD_RESP]);

	return buf;
}


static int p2ps_setup_p2ps_prov(struct p2p_data *p2p, u32 adv_id,
				u32 session_id, u16 method,
				const u8 *session_mac, const u8 *adv_mac)
{
	struct p2ps_provision *tmp;

	if (!p2p->p2ps_prov) {
		p2p->p2ps_prov = os_zalloc(sizeof(struct p2ps_provision) + 1);
		if (!p2p->p2ps_prov)
			return -1;
	} else {
		os_memset(p2p->p2ps_prov, 0, sizeof(struct p2ps_provision) + 1);
	}

	tmp = p2p->p2ps_prov;
	tmp->adv_id = adv_id;
	tmp->session_id = session_id;
	tmp->method = method;
	os_memcpy(tmp->session_mac, session_mac, ETH_ALEN);
	os_memcpy(tmp->adv_mac, adv_mac, ETH_ALEN);
	tmp->info[0] = '\0';

	return 0;
}


void p2p_process_prov_disc_req(struct p2p_data *p2p, const u8 *sa,
			       const u8 *data, size_t len, int rx_freq)
{
	struct p2p_message msg;
	struct p2p_device *dev;
	int freq;
	enum p2p_status_code reject = P2P_SC_FAIL_INCOMPATIBLE_PARAMS;
	struct wpabuf *resp;
	u32 adv_id = 0;
	struct p2ps_advertisement *p2ps_adv = NULL;
	u8 conncap = P2PS_SETUP_NEW;
	u8 auto_accept = 0;
	u32 session_id = 0;
	u8 session_mac[ETH_ALEN];
	u8 adv_mac[ETH_ALEN];
	u8 group_mac[ETH_ALEN];
	int passwd_id = DEV_PW_DEFAULT;
	u16 config_methods;

	if (p2p_parse(data, len, &msg))
		return;

	p2p_dbg(p2p, "Received Provision Discovery Request from " MACSTR
		" with config methods 0x%x (freq=%d)",
		MAC2STR(sa), msg.wps_config_methods, rx_freq);

	dev = p2p_get_device(p2p, sa);
	if (dev == NULL || (dev->flags & P2P_DEV_PROBE_REQ_ONLY)) {
		p2p_dbg(p2p, "Provision Discovery Request from unknown peer "
			MACSTR, MAC2STR(sa));

		if (p2p_add_device(p2p, sa, rx_freq, NULL, 0, data + 1, len - 1,
				   0)) {
			p2p_dbg(p2p, "Provision Discovery Request add device failed "
				MACSTR, MAC2STR(sa));
		}
	} else if (msg.wfd_subelems) {
		wpabuf_free(dev->info.wfd_subelems);
		dev->info.wfd_subelems = wpabuf_dup(msg.wfd_subelems);
	}

	if (!(msg.wps_config_methods &
	      (WPS_CONFIG_DISPLAY | WPS_CONFIG_KEYPAD |
	       WPS_CONFIG_PUSHBUTTON | WPS_CONFIG_P2PS))) {
		p2p_dbg(p2p, "Unsupported Config Methods in Provision Discovery Request");
		goto out;
	}

	/* Legacy (non-P2PS) - Unknown groups allowed for P2PS */
	if (!msg.adv_id && msg.group_id) {
		size_t i;
		for (i = 0; i < p2p->num_groups; i++) {
			if (p2p_group_is_group_id_match(p2p->groups[i],
							msg.group_id,
							msg.group_id_len))
				break;
		}
		if (i == p2p->num_groups) {
			p2p_dbg(p2p, "PD request for unknown P2P Group ID - reject");
			goto out;
		}
	}

	if (dev) {
		dev->flags &= ~(P2P_DEV_PD_PEER_DISPLAY |
				P2P_DEV_PD_PEER_KEYPAD |
				P2P_DEV_PD_PEER_P2PS);

		/* Remove stale persistent groups */
		if (p2p->cfg->remove_stale_groups) {
			p2p->cfg->remove_stale_groups(
				p2p->cfg->cb_ctx, dev->info.p2p_device_addr,
				msg.persistent_dev,
				msg.persistent_ssid, msg.persistent_ssid_len);
		}
	}
	if (msg.wps_config_methods & WPS_CONFIG_DISPLAY) {
		p2p_dbg(p2p, "Peer " MACSTR
			" requested us to show a PIN on display", MAC2STR(sa));
		if (dev)
			dev->flags |= P2P_DEV_PD_PEER_KEYPAD;
		passwd_id = DEV_PW_USER_SPECIFIED;
	} else if (msg.wps_config_methods & WPS_CONFIG_KEYPAD) {
		p2p_dbg(p2p, "Peer " MACSTR
			" requested us to write its PIN using keypad",
			MAC2STR(sa));
		if (dev)
			dev->flags |= P2P_DEV_PD_PEER_DISPLAY;
		passwd_id = DEV_PW_REGISTRAR_SPECIFIED;
	} else if (msg.wps_config_methods & WPS_CONFIG_P2PS) {
		p2p_dbg(p2p, "Peer " MACSTR " requesting P2PS PIN",
			MAC2STR(sa));
		if (dev)
			dev->flags |= P2P_DEV_PD_PEER_P2PS;
		passwd_id = DEV_PW_P2PS_DEFAULT;
	}

	reject = P2P_SC_SUCCESS;

	os_memset(session_mac, 0, ETH_ALEN);
	os_memset(adv_mac, 0, ETH_ALEN);
	os_memset(group_mac, 0, ETH_ALEN);

	if (msg.adv_id && msg.session_id && msg.session_mac && msg.adv_mac &&
	    (msg.status || msg.conn_cap)) {
		u8 remote_conncap;

		if (msg.intended_addr)
			os_memcpy(group_mac, msg.intended_addr, ETH_ALEN);

		os_memcpy(session_mac, msg.session_mac, ETH_ALEN);
		os_memcpy(adv_mac, msg.adv_mac, ETH_ALEN);

		session_id = WPA_GET_LE32(msg.session_id);
		adv_id = WPA_GET_LE32(msg.adv_id);

		if (!msg.status)
			p2ps_adv = p2p_service_p2ps_id(p2p, adv_id);

		p2p_dbg(p2p, "adv_id: %x - p2ps_adv - %p", adv_id, p2ps_adv);

		if (msg.conn_cap)
			conncap = *msg.conn_cap;
		remote_conncap = conncap;

		if (p2ps_adv) {
			auto_accept = p2ps_adv->auto_accept;
			conncap = p2p->cfg->p2ps_group_capability(
				p2p->cfg->cb_ctx, conncap, auto_accept);

			p2p_dbg(p2p, "Conncap: local:%d remote:%d result:%d",
				auto_accept, remote_conncap, conncap);

			if (p2ps_adv->config_methods &&
			    !(msg.wps_config_methods &
			      p2ps_adv->config_methods)) {
				p2p_dbg(p2p,
					"Unsupported config methods in Provision Discovery Request (own=0x%x peer=0x%x)",
					p2ps_adv->config_methods,
					msg.wps_config_methods);
				reject = P2P_SC_FAIL_INCOMPATIBLE_PARAMS;
			} else if (!p2ps_adv->state) {
				p2p_dbg(p2p, "P2PS state unavailable");
				reject = P2P_SC_FAIL_UNABLE_TO_ACCOMMODATE;
			} else if (!conncap) {
				p2p_dbg(p2p, "Conncap resolution failed");
				reject = P2P_SC_FAIL_INCOMPATIBLE_PARAMS;
			}

			if (msg.wps_config_methods & WPS_CONFIG_KEYPAD) {
				p2p_dbg(p2p, "Keypad - always defer");
				auto_accept = 0;
			}

			if (auto_accept || reject != P2P_SC_SUCCESS) {
				struct p2ps_provision *tmp;

				if (reject == P2P_SC_SUCCESS && !conncap) {
					reject =
						P2P_SC_FAIL_INCOMPATIBLE_PARAMS;
				}

				if (p2ps_setup_p2ps_prov(
					    p2p, adv_id, session_id,
					    msg.wps_config_methods,
					    session_mac, adv_mac) < 0) {
					reject = P2P_SC_FAIL_UNABLE_TO_ACCOMMODATE;
					goto out;
				}

				tmp = p2p->p2ps_prov;
				if (conncap) {
					tmp->conncap = conncap;
					tmp->status = P2P_SC_SUCCESS;
				} else {
					tmp->conncap = auto_accept;
					tmp->status = P2P_SC_FAIL_INCOMPATIBLE_PARAMS;
				}

				if (reject != P2P_SC_SUCCESS)
					goto out;
			}
		} else if (!msg.status) {
			reject = P2P_SC_FAIL_INCOMPATIBLE_PARAMS;
			goto out;
		}

		if (!msg.status && !auto_accept &&
		    (!p2p->p2ps_prov || p2p->p2ps_prov->adv_id != adv_id)) {
			struct p2ps_provision *tmp;

			if (!conncap) {
				reject = P2P_SC_FAIL_INCOMPATIBLE_PARAMS;
				goto out;
			}

			if (p2ps_setup_p2ps_prov(p2p, adv_id, session_id,
						 msg.wps_config_methods,
						 session_mac, adv_mac) < 0) {
				reject = P2P_SC_FAIL_UNABLE_TO_ACCOMMODATE;
				goto out;
			}
			tmp = p2p->p2ps_prov;
			reject = P2P_SC_FAIL_INFO_CURRENTLY_UNAVAILABLE;
			tmp->status = reject;
		}

		if (msg.status) {
			if (*msg.status &&
			    *msg.status != P2P_SC_SUCCESS_DEFERRED) {
				reject = *msg.status;
			} else if (*msg.status == P2P_SC_SUCCESS_DEFERRED &&
				   p2p->p2ps_prov) {
				u16 method = p2p->p2ps_prov->method;

				conncap = p2p->cfg->p2ps_group_capability(
					p2p->cfg->cb_ctx, remote_conncap,
					p2p->p2ps_prov->conncap);

				p2p_dbg(p2p,
					"Conncap: local:%d remote:%d result:%d",
					p2p->p2ps_prov->conncap,
					remote_conncap, conncap);

				/*
				 * Ensure that if we asked for PIN originally,
				 * our method is consistent with original
				 * request.
				 */
				if (method & WPS_CONFIG_DISPLAY)
					method = WPS_CONFIG_KEYPAD;
				else if (method & WPS_CONFIG_KEYPAD)
					method = WPS_CONFIG_DISPLAY;

				/* Reject this "Deferred Accept* if incompatible
				 * conncap or method */
				if (!conncap ||
				    !(msg.wps_config_methods & method))
					reject =
						P2P_SC_FAIL_INCOMPATIBLE_PARAMS;
				else
					reject = P2P_SC_SUCCESS;

				p2p->p2ps_prov->status = reject;
				p2p->p2ps_prov->conncap = conncap;
			}
		}
	}

out:
	if (reject == P2P_SC_SUCCESS ||
	    reject == P2P_SC_FAIL_INFO_CURRENTLY_UNAVAILABLE)
		config_methods = msg.wps_config_methods;
	else
		config_methods = 0;
	resp = p2p_build_prov_disc_resp(p2p, dev, msg.dialog_token, reject,
					config_methods, adv_id,
					msg.group_id, msg.group_id_len,
					msg.persistent_ssid,
					msg.persistent_ssid_len);
	if (resp == NULL) {
		p2p_parse_free(&msg);
		return;
	}
	p2p_dbg(p2p, "Sending Provision Discovery Response");
	if (rx_freq > 0)
		freq = rx_freq;
	else
		freq = p2p_channel_to_freq(p2p->cfg->reg_class,
					   p2p->cfg->channel);
	if (freq < 0) {
		p2p_dbg(p2p, "Unknown regulatory class/channel");
		wpabuf_free(resp);
		p2p_parse_free(&msg);
		return;
	}
	p2p->pending_action_state = P2P_PENDING_PD_RESPONSE;
	if (p2p_send_action(p2p, freq, sa, p2p->cfg->dev_addr,
			    p2p->cfg->dev_addr,
			    wpabuf_head(resp), wpabuf_len(resp), 200) < 0) {
		p2p_dbg(p2p, "Failed to send Action frame");
	} else
		p2p->send_action_in_progress = 1;

	wpabuf_free(resp);

	if (!p2p->cfg->p2ps_prov_complete) {
		/* Don't emit anything */
	} else if (msg.status && *msg.status != P2P_SC_SUCCESS &&
		   *msg.status != P2P_SC_SUCCESS_DEFERRED) {
		reject = *msg.status;
		p2p->cfg->p2ps_prov_complete(p2p->cfg->cb_ctx, reject,
					     sa, adv_mac, session_mac,
					     NULL, adv_id, session_id,
					     0, 0, msg.persistent_ssid,
					     msg.persistent_ssid_len,
					     0, 0, NULL);
	} else if (msg.status && *msg.status == P2P_SC_SUCCESS_DEFERRED &&
		   p2p->p2ps_prov) {
		p2p->p2ps_prov->status = reject;
		p2p->p2ps_prov->conncap = conncap;

		if (reject != P2P_SC_SUCCESS)
			p2p->cfg->p2ps_prov_complete(p2p->cfg->cb_ctx, reject,
						     sa, adv_mac, session_mac,
						     NULL, adv_id,
						     session_id, conncap, 0,
						     msg.persistent_ssid,
						     msg.persistent_ssid_len, 0,
						     0, NULL);
		else
			p2p->cfg->p2ps_prov_complete(p2p->cfg->cb_ctx,
						     *msg.status,
						     sa, adv_mac, session_mac,
						     group_mac, adv_id,
						     session_id, conncap,
						     passwd_id,
						     msg.persistent_ssid,
						     msg.persistent_ssid_len, 0,
						     0, NULL);
	} else if (msg.status && p2p->p2ps_prov) {
		p2p->p2ps_prov->status = P2P_SC_SUCCESS;
		p2p->cfg->p2ps_prov_complete(p2p->cfg->cb_ctx, *msg.status, sa,
					     adv_mac, session_mac, group_mac,
					     adv_id, session_id, conncap,
					     passwd_id,
					     msg.persistent_ssid,
					     msg.persistent_ssid_len,
					     0, 0, NULL);
	} else if (msg.status) {
	} else if (auto_accept && reject == P2P_SC_SUCCESS) {
		p2p->cfg->p2ps_prov_complete(p2p->cfg->cb_ctx, P2P_SC_SUCCESS,
					     sa, adv_mac, session_mac,
					     group_mac, adv_id, session_id,
					     conncap, passwd_id,
					     msg.persistent_ssid,
					     msg.persistent_ssid_len,
					     0, 0, NULL);
	} else if (reject == P2P_SC_FAIL_INFO_CURRENTLY_UNAVAILABLE &&
		   (!msg.session_info || !msg.session_info_len)) {
		p2p->p2ps_prov->method = msg.wps_config_methods;

		p2p->cfg->p2ps_prov_complete(p2p->cfg->cb_ctx, P2P_SC_SUCCESS,
					     sa, adv_mac, session_mac,
					     group_mac, adv_id, session_id,
					     conncap, passwd_id,
					     msg.persistent_ssid,
					     msg.persistent_ssid_len,
					     0, 1, NULL);
	} else if (reject == P2P_SC_FAIL_INFO_CURRENTLY_UNAVAILABLE) {
		size_t buf_len = msg.session_info_len;
		char *buf = os_malloc(2 * buf_len + 1);

		if (buf) {
			p2p->p2ps_prov->method = msg.wps_config_methods;

			utf8_escape((char *) msg.session_info, buf_len,
				    buf, 2 * buf_len + 1);

			p2p->cfg->p2ps_prov_complete(
				p2p->cfg->cb_ctx, P2P_SC_SUCCESS, sa,
				adv_mac, session_mac, group_mac, adv_id,
				session_id, conncap, passwd_id,
				msg.persistent_ssid, msg.persistent_ssid_len,
				0, 1, buf);

			os_free(buf);
		}
	}

	if (reject == P2P_SC_SUCCESS && p2p->cfg->prov_disc_req) {
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
	u16 report_config_methods = 0, req_config_methods;
	u8 status = P2P_SC_SUCCESS;
	int success = 0;
	u32 adv_id = 0;
	u8 conncap = P2PS_SETUP_NEW;
	u8 adv_mac[ETH_ALEN];
	u8 group_mac[ETH_ALEN];
	int passwd_id = DEV_PW_DEFAULT;

	if (p2p_parse(data, len, &msg))
		return;

	/* Parse the P2PS members present */
	if (msg.status)
		status = *msg.status;

	if (msg.intended_addr)
		os_memcpy(group_mac, msg.intended_addr, ETH_ALEN);
	else
		os_memset(group_mac, 0, ETH_ALEN);

	if (msg.adv_mac)
		os_memcpy(adv_mac, msg.adv_mac, ETH_ALEN);
	else
		os_memset(adv_mac, 0, ETH_ALEN);

	if (msg.adv_id)
		adv_id = WPA_GET_LE32(msg.adv_id);

	if (msg.conn_cap) {
		conncap = *msg.conn_cap;

		/* Switch bits to local relative */
		switch (conncap) {
		case P2PS_SETUP_GROUP_OWNER:
			conncap = P2PS_SETUP_CLIENT;
			break;
		case P2PS_SETUP_CLIENT:
			conncap = P2PS_SETUP_GROUP_OWNER;
			break;
		}
	}

	p2p_dbg(p2p, "Received Provision Discovery Response from " MACSTR
		" with config methods 0x%x",
		MAC2STR(sa), msg.wps_config_methods);

	dev = p2p_get_device(p2p, sa);
	if (dev == NULL || !dev->req_config_methods) {
		p2p_dbg(p2p, "Ignore Provision Discovery Response from " MACSTR
			" with no pending request", MAC2STR(sa));
		p2p_parse_free(&msg);
		return;
	}

	if (dev->dialog_token != msg.dialog_token) {
		p2p_dbg(p2p, "Ignore Provision Discovery Response with unexpected Dialog Token %u (expected %u)",
			msg.dialog_token, dev->dialog_token);
		p2p_parse_free(&msg);
		return;
	}

	if (p2p->pending_action_state == P2P_PENDING_PD) {
		os_memset(p2p->pending_pd_devaddr, 0, ETH_ALEN);
		p2p->pending_action_state = P2P_NO_PENDING_ACTION;
	}

	/*
	 * Use a local copy of the requested config methods since
	 * p2p_reset_pending_pd() can clear this in the peer entry.
	 */
	req_config_methods = dev->req_config_methods;

	/*
	 * If the response is from the peer to whom a user initiated request
	 * was sent earlier, we reset that state info here.
	 */
	if (p2p->user_initiated_pd &&
	    os_memcmp(p2p->pending_pd_devaddr, sa, ETH_ALEN) == 0)
		p2p_reset_pending_pd(p2p);

	if (msg.wps_config_methods != req_config_methods) {
		p2p_dbg(p2p, "Peer rejected our Provision Discovery Request (received config_methods 0x%x expected 0x%x",
			msg.wps_config_methods, req_config_methods);
		if (p2p->cfg->prov_disc_fail)
			p2p->cfg->prov_disc_fail(p2p->cfg->cb_ctx, sa,
						 P2P_PROV_DISC_REJECTED,
						 adv_id, adv_mac, NULL);
		p2p_parse_free(&msg);
		os_free(p2p->p2ps_prov);
		p2p->p2ps_prov = NULL;
		goto out;
	}

	report_config_methods = req_config_methods;
	dev->flags &= ~(P2P_DEV_PD_PEER_DISPLAY |
			P2P_DEV_PD_PEER_KEYPAD |
			P2P_DEV_PD_PEER_P2PS);
	if (req_config_methods & WPS_CONFIG_DISPLAY) {
		p2p_dbg(p2p, "Peer " MACSTR
			" accepted to show a PIN on display", MAC2STR(sa));
		dev->flags |= P2P_DEV_PD_PEER_DISPLAY;
		passwd_id = DEV_PW_REGISTRAR_SPECIFIED;
	} else if (msg.wps_config_methods & WPS_CONFIG_KEYPAD) {
		p2p_dbg(p2p, "Peer " MACSTR
			" accepted to write our PIN using keypad",
			MAC2STR(sa));
		dev->flags |= P2P_DEV_PD_PEER_KEYPAD;
		passwd_id = DEV_PW_USER_SPECIFIED;
	} else if (msg.wps_config_methods & WPS_CONFIG_P2PS) {
		p2p_dbg(p2p, "Peer " MACSTR " accepted P2PS PIN",
			MAC2STR(sa));
		dev->flags |= P2P_DEV_PD_PEER_P2PS;
		passwd_id = DEV_PW_P2PS_DEFAULT;
	}

	if ((msg.conn_cap || msg.persistent_dev) &&
	    msg.adv_id &&
	    (status == P2P_SC_SUCCESS || status == P2P_SC_SUCCESS_DEFERRED) &&
	    p2p->p2ps_prov) {
		if (p2p->cfg->p2ps_prov_complete) {
			p2p->cfg->p2ps_prov_complete(
				p2p->cfg->cb_ctx, status, sa, adv_mac,
				p2p->p2ps_prov->session_mac,
				group_mac, adv_id, p2p->p2ps_prov->session_id,
				conncap, passwd_id, msg.persistent_ssid,
				msg.persistent_ssid_len, 1, 0, NULL);
		}
		os_free(p2p->p2ps_prov);
		p2p->p2ps_prov = NULL;
	}

	if (status != P2P_SC_SUCCESS &&
	    status != P2P_SC_FAIL_INFO_CURRENTLY_UNAVAILABLE &&
	    status != P2P_SC_SUCCESS_DEFERRED && p2p->p2ps_prov) {
		if (p2p->cfg->p2ps_prov_complete)
			p2p->cfg->p2ps_prov_complete(
				p2p->cfg->cb_ctx, status, sa, adv_mac,
				p2p->p2ps_prov->session_mac,
				group_mac, adv_id, p2p->p2ps_prov->session_id,
				0, 0, NULL, 0, 1, 0, NULL);
		os_free(p2p->p2ps_prov);
		p2p->p2ps_prov = NULL;
	}

	if (status == P2P_SC_FAIL_INFO_CURRENTLY_UNAVAILABLE) {
		if (p2p->cfg->remove_stale_groups) {
			p2p->cfg->remove_stale_groups(p2p->cfg->cb_ctx,
						      dev->info.p2p_device_addr,
						      NULL, NULL, 0);
		}

		if (msg.session_info && msg.session_info_len) {
			size_t info_len = msg.session_info_len;
			char *deferred_sess_resp = os_malloc(2 * info_len + 1);

			if (!deferred_sess_resp) {
				p2p_parse_free(&msg);
				os_free(p2p->p2ps_prov);
				p2p->p2ps_prov = NULL;
				goto out;
			}
			utf8_escape((char *) msg.session_info, info_len,
				    deferred_sess_resp, 2 * info_len + 1);

			if (p2p->cfg->prov_disc_fail)
				p2p->cfg->prov_disc_fail(
					p2p->cfg->cb_ctx, sa,
					P2P_PROV_DISC_INFO_UNAVAILABLE,
					adv_id, adv_mac,
					deferred_sess_resp);
			os_free(deferred_sess_resp);
		} else
			if (p2p->cfg->prov_disc_fail)
				p2p->cfg->prov_disc_fail(
					p2p->cfg->cb_ctx, sa,
					P2P_PROV_DISC_INFO_UNAVAILABLE,
					adv_id, adv_mac, NULL);
	} else if (msg.wps_config_methods != dev->req_config_methods ||
		   status != P2P_SC_SUCCESS) {
		p2p_dbg(p2p, "Peer rejected our Provision Discovery Request");
		if (p2p->cfg->prov_disc_fail)
			p2p->cfg->prov_disc_fail(p2p->cfg->cb_ctx, sa,
						 P2P_PROV_DISC_REJECTED, 0,
						 NULL, NULL);
		p2p_parse_free(&msg);
		os_free(p2p->p2ps_prov);
		p2p->p2ps_prov = NULL;
		goto out;
	}

	/* Store the provisioning info */
	dev->wps_prov_info = msg.wps_config_methods;

	p2p_parse_free(&msg);
	success = 1;

out:
	dev->req_config_methods = 0;
	p2p->cfg->send_action_done(p2p->cfg->cb_ctx);
	if (dev->flags & P2P_DEV_PD_BEFORE_GO_NEG) {
		p2p_dbg(p2p, "Start GO Neg after the PD-before-GO-Neg workaround with "
			MACSTR, MAC2STR(dev->info.p2p_device_addr));
		dev->flags &= ~P2P_DEV_PD_BEFORE_GO_NEG;
		p2p_connect_send(p2p, dev);
		return;
	}
	if (success && p2p->cfg->prov_disc_resp)
		p2p->cfg->prov_disc_resp(p2p->cfg->cb_ctx, sa,
					 report_config_methods);

	if (p2p->state == P2P_PD_DURING_FIND) {
		p2p_clear_timeout(p2p);
		p2p_continue_find(p2p);
	}
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
		p2p_dbg(p2p, "No Listen/Operating frequency known for the peer "
			MACSTR " to send Provision Discovery Request",
			MAC2STR(dev->info.p2p_device_addr));
		return -1;
	}

	if (dev->flags & P2P_DEV_GROUP_CLIENT_ONLY) {
		if (!(dev->info.dev_capab &
		      P2P_DEV_CAPAB_CLIENT_DISCOVERABILITY)) {
			p2p_dbg(p2p, "Cannot use PD with P2P Device " MACSTR
				" that is in a group and is not discoverable",
				MAC2STR(dev->info.p2p_device_addr));
			return -1;
		}
		/* TODO: use device discoverability request through GO */
	}

	if (p2p->p2ps_prov) {
		if (p2p->p2ps_prov->status == P2P_SC_SUCCESS_DEFERRED) {
			if (p2p->p2ps_prov->method == WPS_CONFIG_DISPLAY)
				dev->req_config_methods = WPS_CONFIG_KEYPAD;
			else if (p2p->p2ps_prov->method == WPS_CONFIG_KEYPAD)
				dev->req_config_methods = WPS_CONFIG_DISPLAY;
			else
				dev->req_config_methods = WPS_CONFIG_P2PS;
		} else {
			/* Order of preference, based on peer's capabilities */
			if (p2p->p2ps_prov->method)
				dev->req_config_methods =
					p2p->p2ps_prov->method;
			else if (dev->info.config_methods & WPS_CONFIG_P2PS)
				dev->req_config_methods = WPS_CONFIG_P2PS;
			else if (dev->info.config_methods & WPS_CONFIG_DISPLAY)
				dev->req_config_methods = WPS_CONFIG_DISPLAY;
			else
				dev->req_config_methods = WPS_CONFIG_KEYPAD;
		}
		p2p_dbg(p2p,
			"Building PD Request based on P2PS config method 0x%x status %d --> req_config_methods 0x%x",
			p2p->p2ps_prov->method, p2p->p2ps_prov->status,
			dev->req_config_methods);
	}

	req = p2p_build_prov_disc_req(p2p, dev, join);
	if (req == NULL)
		return -1;

	if (p2p->state != P2P_IDLE)
		p2p_stop_listen_for_freq(p2p, freq);
	p2p->pending_action_state = P2P_PENDING_PD;
	if (p2p_send_action(p2p, freq, dev->info.p2p_device_addr,
			    p2p->cfg->dev_addr, dev->info.p2p_device_addr,
			    wpabuf_head(req), wpabuf_len(req), 200) < 0) {
		p2p_dbg(p2p, "Failed to send Action frame");
		wpabuf_free(req);
		return -1;
	}

	os_memcpy(p2p->pending_pd_devaddr, dev->info.p2p_device_addr, ETH_ALEN);

	wpabuf_free(req);
	return 0;
}


int p2p_prov_disc_req(struct p2p_data *p2p, const u8 *peer_addr,
		      struct p2ps_provision *p2ps_prov,
		      u16 config_methods, int join, int force_freq,
		      int user_initiated_pd)
{
	struct p2p_device *dev;

	dev = p2p_get_device(p2p, peer_addr);
	if (dev == NULL)
		dev = p2p_get_device_interface(p2p, peer_addr);
	if (dev == NULL || (dev->flags & P2P_DEV_PROBE_REQ_ONLY)) {
		p2p_dbg(p2p, "Provision Discovery Request destination " MACSTR
			" not yet known", MAC2STR(peer_addr));
		os_free(p2ps_prov);
		return -1;
	}

	p2p_dbg(p2p, "Provision Discovery Request with " MACSTR
		" (config methods 0x%x)",
		MAC2STR(peer_addr), config_methods);
	if (config_methods == 0 && !p2ps_prov) {
		os_free(p2ps_prov);
		return -1;
	}

	if (p2ps_prov && p2ps_prov->status == P2P_SC_SUCCESS_DEFERRED &&
	    p2p->p2ps_prov) {
		/* Use cached method from deferred provisioning */
		p2ps_prov->method = p2p->p2ps_prov->method;
	}

	/* Reset provisioning info */
	dev->wps_prov_info = 0;
	os_free(p2p->p2ps_prov);
	p2p->p2ps_prov = p2ps_prov;

	dev->req_config_methods = config_methods;
	if (join)
		dev->flags |= P2P_DEV_PD_FOR_JOIN;
	else
		dev->flags &= ~P2P_DEV_PD_FOR_JOIN;

	if (p2p->state != P2P_IDLE && p2p->state != P2P_SEARCH &&
	    p2p->state != P2P_LISTEN_ONLY) {
		p2p_dbg(p2p, "Busy with other operations; postpone Provision Discovery Request with "
			MACSTR " (config methods 0x%x)",
			MAC2STR(peer_addr), config_methods);
		return 0;
	}

	p2p->user_initiated_pd = user_initiated_pd;
	p2p->pd_force_freq = force_freq;

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
	p2p->pd_force_freq = 0;
}
