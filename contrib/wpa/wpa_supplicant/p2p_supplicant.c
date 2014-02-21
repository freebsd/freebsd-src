/*
 * wpa_supplicant - P2P
 * Copyright (c) 2009-2010, Atheros Communications
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "eloop.h"
#include "common/ieee802_11_common.h"
#include "common/ieee802_11_defs.h"
#include "common/wpa_ctrl.h"
#include "wps/wps_i.h"
#include "p2p/p2p.h"
#include "ap/hostapd.h"
#include "ap/ap_config.h"
#include "ap/p2p_hostapd.h"
#include "eapol_supp/eapol_supp_sm.h"
#include "rsn_supp/wpa.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "ap.h"
#include "config_ssid.h"
#include "config.h"
#include "notify.h"
#include "scan.h"
#include "bss.h"
#include "offchannel.h"
#include "wps_supplicant.h"
#include "p2p_supplicant.h"


/*
 * How many times to try to scan to find the GO before giving up on join
 * request.
 */
#define P2P_MAX_JOIN_SCAN_ATTEMPTS 10

#define P2P_AUTO_PD_SCAN_ATTEMPTS 5

#ifndef P2P_MAX_CLIENT_IDLE
/*
 * How many seconds to try to reconnect to the GO when connection in P2P client
 * role has been lost.
 */
#define P2P_MAX_CLIENT_IDLE 10
#endif /* P2P_MAX_CLIENT_IDLE */

#ifndef P2P_MAX_INITIAL_CONN_WAIT
/*
 * How many seconds to wait for initial 4-way handshake to get completed after
 * WPS provisioning step.
 */
#define P2P_MAX_INITIAL_CONN_WAIT 10
#endif /* P2P_MAX_INITIAL_CONN_WAIT */

#ifndef P2P_CONCURRENT_SEARCH_DELAY
#define P2P_CONCURRENT_SEARCH_DELAY 500
#endif /* P2P_CONCURRENT_SEARCH_DELAY */

enum p2p_group_removal_reason {
	P2P_GROUP_REMOVAL_UNKNOWN,
	P2P_GROUP_REMOVAL_SILENT,
	P2P_GROUP_REMOVAL_FORMATION_FAILED,
	P2P_GROUP_REMOVAL_REQUESTED,
	P2P_GROUP_REMOVAL_IDLE_TIMEOUT,
	P2P_GROUP_REMOVAL_UNAVAILABLE,
	P2P_GROUP_REMOVAL_GO_ENDING_SESSION
};


static void wpas_p2p_long_listen_timeout(void *eloop_ctx, void *timeout_ctx);
static struct wpa_supplicant *
wpas_p2p_get_group_iface(struct wpa_supplicant *wpa_s, int addr_allocated,
			 int go);
static int wpas_p2p_join_start(struct wpa_supplicant *wpa_s);
static void wpas_p2p_join_scan_req(struct wpa_supplicant *wpa_s, int freq);
static void wpas_p2p_join_scan(void *eloop_ctx, void *timeout_ctx);
static int wpas_p2p_join(struct wpa_supplicant *wpa_s, const u8 *iface_addr,
			 const u8 *dev_addr, enum p2p_wps_method wps_method,
			 int auto_join);
static int wpas_p2p_create_iface(struct wpa_supplicant *wpa_s);
static void wpas_p2p_cross_connect_setup(struct wpa_supplicant *wpa_s);
static void wpas_p2p_group_idle_timeout(void *eloop_ctx, void *timeout_ctx);
static void wpas_p2p_set_group_idle_timeout(struct wpa_supplicant *wpa_s);
static void wpas_p2p_fallback_to_go_neg(struct wpa_supplicant *wpa_s,
					int group_added);
static int wpas_p2p_stop_find_oper(struct wpa_supplicant *wpa_s);


static void wpas_p2p_scan_res_handler(struct wpa_supplicant *wpa_s,
				      struct wpa_scan_results *scan_res)
{
	size_t i;

	if (wpa_s->global->p2p_disabled || wpa_s->global->p2p == NULL)
		return;

	wpa_printf(MSG_DEBUG, "P2P: Scan results received (%d BSS)",
		   (int) scan_res->num);

	for (i = 0; i < scan_res->num; i++) {
		struct wpa_scan_res *bss = scan_res->res[i];
		if (p2p_scan_res_handler(wpa_s->global->p2p, bss->bssid,
					 bss->freq, bss->age, bss->level,
					 (const u8 *) (bss + 1),
					 bss->ie_len) > 0)
			break;
	}

	p2p_scan_res_handled(wpa_s->global->p2p);
}


static int wpas_p2p_scan(void *ctx, enum p2p_scan_type type, int freq,
			 unsigned int num_req_dev_types,
			 const u8 *req_dev_types, const u8 *dev_id, u16 pw_id)
{
	struct wpa_supplicant *wpa_s = ctx;
	struct wpa_supplicant *ifs;
	struct wpa_driver_scan_params params;
	int ret;
	struct wpabuf *wps_ie, *ies;
	int social_channels[] = { 2412, 2437, 2462, 0, 0 };
	size_t ielen;

	if (wpa_s->global->p2p_disabled || wpa_s->global->p2p == NULL)
		return -1;

	for (ifs = wpa_s->global->ifaces; ifs; ifs = ifs->next) {
		if (ifs->sta_scan_pending &&
		    wpas_p2p_in_progress(wpa_s) == 2) {
			wpa_printf(MSG_DEBUG, "Delaying P2P scan to allow "
				   "pending station mode scan to be "
				   "completed on interface %s", ifs->ifname);
			wpa_s->global->p2p_cb_on_scan_complete = 1;
			wpa_supplicant_req_scan(ifs, 0, 0);
			return 1;
		}
	}

	os_memset(&params, 0, sizeof(params));

	/* P2P Wildcard SSID */
	params.num_ssids = 1;
	params.ssids[0].ssid = (u8 *) P2P_WILDCARD_SSID;
	params.ssids[0].ssid_len = P2P_WILDCARD_SSID_LEN;

	wpa_s->wps->dev.p2p = 1;
	wps_ie = wps_build_probe_req_ie(pw_id, &wpa_s->wps->dev,
					wpa_s->wps->uuid, WPS_REQ_ENROLLEE,
					num_req_dev_types, req_dev_types);
	if (wps_ie == NULL)
		return -1;

	ielen = p2p_scan_ie_buf_len(wpa_s->global->p2p);
	ies = wpabuf_alloc(wpabuf_len(wps_ie) + ielen);
	if (ies == NULL) {
		wpabuf_free(wps_ie);
		return -1;
	}
	wpabuf_put_buf(ies, wps_ie);
	wpabuf_free(wps_ie);

	p2p_scan_ie(wpa_s->global->p2p, ies, dev_id);

	params.p2p_probe = 1;
	params.extra_ies = wpabuf_head(ies);
	params.extra_ies_len = wpabuf_len(ies);

	switch (type) {
	case P2P_SCAN_SOCIAL:
		params.freqs = social_channels;
		break;
	case P2P_SCAN_FULL:
		break;
	case P2P_SCAN_SOCIAL_PLUS_ONE:
		social_channels[3] = freq;
		params.freqs = social_channels;
		break;
	}

	ret = wpa_drv_scan(wpa_s, &params);

	wpabuf_free(ies);

	if (ret) {
		for (ifs = wpa_s->global->ifaces; ifs; ifs = ifs->next) {
			if (ifs->scanning ||
			    ifs->scan_res_handler == wpas_p2p_scan_res_handler) {
				wpa_s->global->p2p_cb_on_scan_complete = 1;
				ret = 1;
				break;
			}
		}
	} else
		wpa_s->scan_res_handler = wpas_p2p_scan_res_handler;

	return ret;
}


static enum wpa_driver_if_type wpas_p2p_if_type(int p2p_group_interface)
{
	switch (p2p_group_interface) {
	case P2P_GROUP_INTERFACE_PENDING:
		return WPA_IF_P2P_GROUP;
	case P2P_GROUP_INTERFACE_GO:
		return WPA_IF_P2P_GO;
	case P2P_GROUP_INTERFACE_CLIENT:
		return WPA_IF_P2P_CLIENT;
	}

	return WPA_IF_P2P_GROUP;
}


static struct wpa_supplicant * wpas_get_p2p_group(struct wpa_supplicant *wpa_s,
						  const u8 *ssid,
						  size_t ssid_len, int *go)
{
	struct wpa_ssid *s;

	for (wpa_s = wpa_s->global->ifaces; wpa_s; wpa_s = wpa_s->next) {
		for (s = wpa_s->conf->ssid; s; s = s->next) {
			if (s->disabled != 0 || !s->p2p_group ||
			    s->ssid_len != ssid_len ||
			    os_memcmp(ssid, s->ssid, ssid_len) != 0)
				continue;
			if (s->mode == WPAS_MODE_P2P_GO &&
			    s != wpa_s->current_ssid)
				continue;
			if (go)
				*go = s->mode == WPAS_MODE_P2P_GO;
			return wpa_s;
		}
	}

	return NULL;
}


static int wpas_p2p_group_delete(struct wpa_supplicant *wpa_s,
				 enum p2p_group_removal_reason removal_reason)
{
	struct wpa_ssid *ssid;
	char *gtype;
	const char *reason;

	ssid = wpa_s->current_ssid;
	if (ssid == NULL) {
		/*
		 * The current SSID was not known, but there may still be a
		 * pending P2P group interface waiting for provisioning or a
		 * P2P group that is trying to reconnect.
		 */
		ssid = wpa_s->conf->ssid;
		while (ssid) {
			if (ssid->p2p_group && ssid->disabled != 2)
				break;
			ssid = ssid->next;
		}
		if (ssid == NULL &&
			wpa_s->p2p_group_interface == NOT_P2P_GROUP_INTERFACE)
		{
			wpa_printf(MSG_ERROR, "P2P: P2P group interface "
				   "not found");
			return -1;
		}
	}
	if (wpa_s->p2p_group_interface == P2P_GROUP_INTERFACE_GO)
		gtype = "GO";
	else if (wpa_s->p2p_group_interface == P2P_GROUP_INTERFACE_CLIENT ||
		 (ssid && ssid->mode == WPAS_MODE_INFRA)) {
		wpa_s->reassociate = 0;
		wpa_s->disconnected = 1;
		wpa_supplicant_deauthenticate(wpa_s,
					      WLAN_REASON_DEAUTH_LEAVING);
		gtype = "client";
	} else
		gtype = "GO";
	if (wpa_s->cross_connect_in_use) {
		wpa_s->cross_connect_in_use = 0;
		wpa_msg(wpa_s->parent, MSG_INFO,
			P2P_EVENT_CROSS_CONNECT_DISABLE "%s %s",
			wpa_s->ifname, wpa_s->cross_connect_uplink);
	}
	switch (removal_reason) {
	case P2P_GROUP_REMOVAL_REQUESTED:
		reason = " reason=REQUESTED";
		break;
	case P2P_GROUP_REMOVAL_FORMATION_FAILED:
		reason = " reason=FORMATION_FAILED";
		break;
	case P2P_GROUP_REMOVAL_IDLE_TIMEOUT:
		reason = " reason=IDLE";
		break;
	case P2P_GROUP_REMOVAL_UNAVAILABLE:
		reason = " reason=UNAVAILABLE";
		break;
	case P2P_GROUP_REMOVAL_GO_ENDING_SESSION:
		reason = " reason=GO_ENDING_SESSION";
		break;
	default:
		reason = "";
		break;
	}
	if (removal_reason != P2P_GROUP_REMOVAL_SILENT) {
		wpa_msg(wpa_s->parent, MSG_INFO,
			P2P_EVENT_GROUP_REMOVED "%s %s%s",
			wpa_s->ifname, gtype, reason);
	}

	if (eloop_cancel_timeout(wpas_p2p_group_idle_timeout, wpa_s, NULL) > 0)
		wpa_printf(MSG_DEBUG, "P2P: Cancelled P2P group idle timeout");

	if (removal_reason != P2P_GROUP_REMOVAL_SILENT && ssid)
		wpas_notify_p2p_group_removed(wpa_s, ssid, gtype);

	if (wpa_s->p2p_group_interface != NOT_P2P_GROUP_INTERFACE) {
		struct wpa_global *global;
		char *ifname;
		enum wpa_driver_if_type type;
		wpa_printf(MSG_DEBUG, "P2P: Remove group interface %s",
			wpa_s->ifname);
		global = wpa_s->global;
		ifname = os_strdup(wpa_s->ifname);
		type = wpas_p2p_if_type(wpa_s->p2p_group_interface);
		wpa_supplicant_remove_iface(wpa_s->global, wpa_s, 0);
		wpa_s = global->ifaces;
		if (wpa_s && ifname)
			wpa_drv_if_remove(wpa_s, type, ifname);
		os_free(ifname);
		return 1;
	}

	wpa_printf(MSG_DEBUG, "P2P: Remove temporary group network");
	if (ssid && (ssid->p2p_group ||
		     ssid->mode == WPAS_MODE_P2P_GROUP_FORMATION ||
		     (ssid->key_mgmt & WPA_KEY_MGMT_WPS))) {
		int id = ssid->id;
		if (ssid == wpa_s->current_ssid) {
			wpa_sm_set_config(wpa_s->wpa, NULL);
			eapol_sm_notify_config(wpa_s->eapol, NULL, NULL);
			wpa_s->current_ssid = NULL;
		}
		/*
		 * Networks objects created during any P2P activities are not
		 * exposed out as they might/will confuse certain non-P2P aware
		 * applications since these network objects won't behave like
		 * regular ones.
		 *
		 * Likewise, we don't send out network removed signals for such
		 * network objects.
		 */
		wpa_config_remove_network(wpa_s->conf, id);
		wpa_supplicant_clear_status(wpa_s);
		wpa_supplicant_cancel_sched_scan(wpa_s);
		wpa_s->sta_scan_pending = 0;
	} else {
		wpa_printf(MSG_DEBUG, "P2P: Temporary group network not "
			   "found");
	}
	if (wpa_s->ap_iface)
		wpa_supplicant_ap_deinit(wpa_s);
	else
		wpa_drv_deinit_p2p_cli(wpa_s);

	return 0;
}


static int wpas_p2p_persistent_group(struct wpa_supplicant *wpa_s,
				     u8 *go_dev_addr,
				     const u8 *ssid, size_t ssid_len)
{
	struct wpa_bss *bss;
	const u8 *bssid;
	struct wpabuf *p2p;
	u8 group_capab;
	const u8 *addr;

	if (wpa_s->go_params)
		bssid = wpa_s->go_params->peer_interface_addr;
	else
		bssid = wpa_s->bssid;

	bss = wpa_bss_get(wpa_s, bssid, ssid, ssid_len);
	if (bss == NULL) {
		u8 iface_addr[ETH_ALEN];
		if (p2p_get_interface_addr(wpa_s->global->p2p, bssid,
					   iface_addr) == 0)
			bss = wpa_bss_get(wpa_s, iface_addr, ssid, ssid_len);
	}
	if (bss == NULL) {
		wpa_printf(MSG_DEBUG, "P2P: Could not figure out whether "
			   "group is persistent - BSS " MACSTR " not found",
			   MAC2STR(bssid));
		return 0;
	}

	p2p = wpa_bss_get_vendor_ie_multi(bss, P2P_IE_VENDOR_TYPE);
	if (p2p == NULL) {
		wpa_printf(MSG_DEBUG, "P2P: Could not figure out whether "
			   "group is persistent - BSS " MACSTR
			   " did not include P2P IE", MAC2STR(bssid));
		wpa_hexdump(MSG_DEBUG, "P2P: Probe Response IEs",
			    (u8 *) (bss + 1), bss->ie_len);
		wpa_hexdump(MSG_DEBUG, "P2P: Beacon IEs",
			    ((u8 *) bss + 1) + bss->ie_len,
			    bss->beacon_ie_len);
		return 0;
	}

	group_capab = p2p_get_group_capab(p2p);
	addr = p2p_get_go_dev_addr(p2p);
	wpa_printf(MSG_DEBUG, "P2P: Checking whether group is persistent: "
		   "group_capab=0x%x", group_capab);
	if (addr) {
		os_memcpy(go_dev_addr, addr, ETH_ALEN);
		wpa_printf(MSG_DEBUG, "P2P: GO Device Address " MACSTR,
			   MAC2STR(addr));
	} else
		os_memset(go_dev_addr, 0, ETH_ALEN);
	wpabuf_free(p2p);

	wpa_printf(MSG_DEBUG, "P2P: BSS " MACSTR " group_capab=0x%x "
		   "go_dev_addr=" MACSTR,
		   MAC2STR(bssid), group_capab, MAC2STR(go_dev_addr));

	return group_capab & P2P_GROUP_CAPAB_PERSISTENT_GROUP;
}


static int wpas_p2p_store_persistent_group(struct wpa_supplicant *wpa_s,
					   struct wpa_ssid *ssid,
					   const u8 *go_dev_addr)
{
	struct wpa_ssid *s;
	int changed = 0;

	wpa_printf(MSG_DEBUG, "P2P: Storing credentials for a persistent "
		   "group (GO Dev Addr " MACSTR ")", MAC2STR(go_dev_addr));
	for (s = wpa_s->conf->ssid; s; s = s->next) {
		if (s->disabled == 2 &&
		    os_memcmp(go_dev_addr, s->bssid, ETH_ALEN) == 0 &&
		    s->ssid_len == ssid->ssid_len &&
		    os_memcmp(ssid->ssid, s->ssid, ssid->ssid_len) == 0)
			break;
	}

	if (s) {
		wpa_printf(MSG_DEBUG, "P2P: Update existing persistent group "
			   "entry");
		if (ssid->passphrase && !s->passphrase)
			changed = 1;
		else if (ssid->passphrase && s->passphrase &&
			 os_strcmp(ssid->passphrase, s->passphrase) != 0)
			changed = 1;
	} else {
		wpa_printf(MSG_DEBUG, "P2P: Create a new persistent group "
			   "entry");
		changed = 1;
		s = wpa_config_add_network(wpa_s->conf);
		if (s == NULL)
			return -1;

		/*
		 * Instead of network_added we emit persistent_group_added
		 * notification. Also to keep the defense checks in
		 * persistent_group obj registration method, we set the
		 * relevant flags in s to designate it as a persistent group.
		 */
		s->p2p_group = 1;
		s->p2p_persistent_group = 1;
		wpas_notify_persistent_group_added(wpa_s, s);
		wpa_config_set_network_defaults(s);
	}

	s->p2p_group = 1;
	s->p2p_persistent_group = 1;
	s->disabled = 2;
	s->bssid_set = 1;
	os_memcpy(s->bssid, go_dev_addr, ETH_ALEN);
	s->mode = ssid->mode;
	s->auth_alg = WPA_AUTH_ALG_OPEN;
	s->key_mgmt = WPA_KEY_MGMT_PSK;
	s->proto = WPA_PROTO_RSN;
	s->pairwise_cipher = WPA_CIPHER_CCMP;
	s->export_keys = 1;
	if (ssid->passphrase) {
		os_free(s->passphrase);
		s->passphrase = os_strdup(ssid->passphrase);
	}
	if (ssid->psk_set) {
		s->psk_set = 1;
		os_memcpy(s->psk, ssid->psk, 32);
	}
	if (s->passphrase && !s->psk_set)
		wpa_config_update_psk(s);
	if (s->ssid == NULL || s->ssid_len < ssid->ssid_len) {
		os_free(s->ssid);
		s->ssid = os_malloc(ssid->ssid_len);
	}
	if (s->ssid) {
		s->ssid_len = ssid->ssid_len;
		os_memcpy(s->ssid, ssid->ssid, s->ssid_len);
	}

#ifndef CONFIG_NO_CONFIG_WRITE
	if (changed && wpa_s->conf->update_config &&
	    wpa_config_write(wpa_s->confname, wpa_s->conf)) {
		wpa_printf(MSG_DEBUG, "P2P: Failed to update configuration");
	}
#endif /* CONFIG_NO_CONFIG_WRITE */

	return s->id;
}


static void wpas_p2p_add_persistent_group_client(struct wpa_supplicant *wpa_s,
						 const u8 *addr)
{
	struct wpa_ssid *ssid, *s;
	u8 *n;
	size_t i;
	int found = 0;

	ssid = wpa_s->current_ssid;
	if (ssid == NULL || ssid->mode != WPAS_MODE_P2P_GO ||
	    !ssid->p2p_persistent_group)
		return;

	for (s = wpa_s->parent->conf->ssid; s; s = s->next) {
		if (s->disabled != 2 || s->mode != WPAS_MODE_P2P_GO)
			continue;

		if (s->ssid_len == ssid->ssid_len &&
		    os_memcmp(s->ssid, ssid->ssid, s->ssid_len) == 0)
			break;
	}

	if (s == NULL)
		return;

	for (i = 0; s->p2p_client_list && i < s->num_p2p_clients; i++) {
		if (os_memcmp(s->p2p_client_list + i * ETH_ALEN, addr,
			      ETH_ALEN) != 0)
			continue;

		if (i == s->num_p2p_clients - 1)
			return; /* already the most recent entry */

		/* move the entry to mark it most recent */
		os_memmove(s->p2p_client_list + i * ETH_ALEN,
			   s->p2p_client_list + (i + 1) * ETH_ALEN,
			   (s->num_p2p_clients - i - 1) * ETH_ALEN);
		os_memcpy(s->p2p_client_list +
			  (s->num_p2p_clients - 1) * ETH_ALEN, addr, ETH_ALEN);
		found = 1;
		break;
	}

	if (!found && s->num_p2p_clients < P2P_MAX_STORED_CLIENTS) {
		n = os_realloc_array(s->p2p_client_list,
				     s->num_p2p_clients + 1, ETH_ALEN);
		if (n == NULL)
			return;
		os_memcpy(n + s->num_p2p_clients * ETH_ALEN, addr, ETH_ALEN);
		s->p2p_client_list = n;
		s->num_p2p_clients++;
	} else if (!found) {
		/* Not enough room for an additional entry - drop the oldest
		 * entry */
		os_memmove(s->p2p_client_list,
			   s->p2p_client_list + ETH_ALEN,
			   (s->num_p2p_clients - 1) * ETH_ALEN);
		os_memcpy(s->p2p_client_list +
			  (s->num_p2p_clients - 1) * ETH_ALEN,
			  addr, ETH_ALEN);
	}

#ifndef CONFIG_NO_CONFIG_WRITE
	if (wpa_s->parent->conf->update_config &&
	    wpa_config_write(wpa_s->parent->confname, wpa_s->parent->conf))
		wpa_printf(MSG_DEBUG, "P2P: Failed to update configuration");
#endif /* CONFIG_NO_CONFIG_WRITE */
}


static void wpas_group_formation_completed(struct wpa_supplicant *wpa_s,
					   int success)
{
	struct wpa_ssid *ssid;
	const char *ssid_txt;
	int client;
	int persistent;
	u8 go_dev_addr[ETH_ALEN];
	int network_id = -1;

	/*
	 * This callback is likely called for the main interface. Update wpa_s
	 * to use the group interface if a new interface was created for the
	 * group.
	 */
	if (wpa_s->global->p2p_group_formation)
		wpa_s = wpa_s->global->p2p_group_formation;
	wpa_s->global->p2p_group_formation = NULL;
	wpa_s->p2p_in_provisioning = 0;

	if (!success) {
		wpa_msg(wpa_s->parent, MSG_INFO,
			P2P_EVENT_GROUP_FORMATION_FAILURE);
		wpas_p2p_group_delete(wpa_s,
				      P2P_GROUP_REMOVAL_FORMATION_FAILED);
		return;
	}

	wpa_msg(wpa_s->parent, MSG_INFO, P2P_EVENT_GROUP_FORMATION_SUCCESS);

	ssid = wpa_s->current_ssid;
	if (ssid && ssid->mode == WPAS_MODE_P2P_GROUP_FORMATION) {
		ssid->mode = WPAS_MODE_P2P_GO;
		p2p_group_notif_formation_done(wpa_s->p2p_group);
		wpa_supplicant_ap_mac_addr_filter(wpa_s, NULL);
	}

	persistent = 0;
	if (ssid) {
		ssid_txt = wpa_ssid_txt(ssid->ssid, ssid->ssid_len);
		client = ssid->mode == WPAS_MODE_INFRA;
		if (ssid->mode == WPAS_MODE_P2P_GO) {
			persistent = ssid->p2p_persistent_group;
			os_memcpy(go_dev_addr, wpa_s->global->p2p_dev_addr,
				  ETH_ALEN);
		} else
			persistent = wpas_p2p_persistent_group(wpa_s,
							       go_dev_addr,
							       ssid->ssid,
							       ssid->ssid_len);
	} else {
		ssid_txt = "";
		client = wpa_s->p2p_group_interface ==
			P2P_GROUP_INTERFACE_CLIENT;
		os_memset(go_dev_addr, 0, ETH_ALEN);
	}

	wpa_s->show_group_started = 0;
	if (client) {
		/*
		 * Indicate event only after successfully completed 4-way
		 * handshake, i.e., when the interface is ready for data
		 * packets.
		 */
		wpa_s->show_group_started = 1;
	} else if (ssid && ssid->passphrase == NULL && ssid->psk_set) {
		char psk[65];
		wpa_snprintf_hex(psk, sizeof(psk), ssid->psk, 32);
		wpa_msg(wpa_s->parent, MSG_INFO, P2P_EVENT_GROUP_STARTED
			"%s GO ssid=\"%s\" freq=%d psk=%s go_dev_addr=" MACSTR
			"%s",
			wpa_s->ifname, ssid_txt, ssid->frequency, psk,
			MAC2STR(go_dev_addr),
			persistent ? " [PERSISTENT]" : "");
		wpas_p2p_cross_connect_setup(wpa_s);
		wpas_p2p_set_group_idle_timeout(wpa_s);
	} else {
		wpa_msg(wpa_s->parent, MSG_INFO, P2P_EVENT_GROUP_STARTED
			"%s GO ssid=\"%s\" freq=%d passphrase=\"%s\" "
			"go_dev_addr=" MACSTR "%s",
			wpa_s->ifname, ssid_txt, ssid ? ssid->frequency : 0,
			ssid && ssid->passphrase ? ssid->passphrase : "",
			MAC2STR(go_dev_addr),
			persistent ? " [PERSISTENT]" : "");
		wpas_p2p_cross_connect_setup(wpa_s);
		wpas_p2p_set_group_idle_timeout(wpa_s);
	}

	if (persistent)
		network_id = wpas_p2p_store_persistent_group(wpa_s->parent,
							     ssid, go_dev_addr);
	if (network_id < 0 && ssid)
		network_id = ssid->id;
	if (!client)
		wpas_notify_p2p_group_started(wpa_s, ssid, network_id, 0);
}


static void wpas_p2p_send_action_tx_status(struct wpa_supplicant *wpa_s,
					   unsigned int freq,
					   const u8 *dst, const u8 *src,
					   const u8 *bssid,
					   const u8 *data, size_t data_len,
					   enum offchannel_send_action_result
					   result)
{
	enum p2p_send_action_result res = P2P_SEND_ACTION_SUCCESS;

	if (wpa_s->global->p2p == NULL || wpa_s->global->p2p_disabled)
		return;
	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT)
		return;

	switch (result) {
	case OFFCHANNEL_SEND_ACTION_SUCCESS:
		res = P2P_SEND_ACTION_SUCCESS;
		break;
	case OFFCHANNEL_SEND_ACTION_NO_ACK:
		res = P2P_SEND_ACTION_NO_ACK;
		break;
	case OFFCHANNEL_SEND_ACTION_FAILED:
		res = P2P_SEND_ACTION_FAILED;
		break;
	}

	p2p_send_action_cb(wpa_s->global->p2p, freq, dst, src, bssid, res);

	if (result != OFFCHANNEL_SEND_ACTION_SUCCESS &&
	    wpa_s->pending_pd_before_join &&
	    (os_memcmp(dst, wpa_s->pending_join_dev_addr, ETH_ALEN) == 0 ||
	     os_memcmp(dst, wpa_s->pending_join_iface_addr, ETH_ALEN) == 0) &&
	    wpa_s->p2p_fallback_to_go_neg) {
		wpa_s->pending_pd_before_join = 0;
		wpa_dbg(wpa_s, MSG_DEBUG, "P2P: No ACK for PD Req "
			"during p2p_connect-auto");
		wpas_p2p_fallback_to_go_neg(wpa_s, 0);
		return;
	}
}


static int wpas_send_action(void *ctx, unsigned int freq, const u8 *dst,
			    const u8 *src, const u8 *bssid, const u8 *buf,
			    size_t len, unsigned int wait_time)
{
	struct wpa_supplicant *wpa_s = ctx;
	return offchannel_send_action(wpa_s, freq, dst, src, bssid, buf, len,
				      wait_time,
				      wpas_p2p_send_action_tx_status, 1);
}


static void wpas_send_action_done(void *ctx)
{
	struct wpa_supplicant *wpa_s = ctx;
	offchannel_send_action_done(wpa_s);
}


static int wpas_copy_go_neg_results(struct wpa_supplicant *wpa_s,
				    struct p2p_go_neg_results *params)
{
	if (wpa_s->go_params == NULL) {
		wpa_s->go_params = os_malloc(sizeof(*params));
		if (wpa_s->go_params == NULL)
			return -1;
	}
	os_memcpy(wpa_s->go_params, params, sizeof(*params));
	return 0;
}


static void wpas_start_wps_enrollee(struct wpa_supplicant *wpa_s,
				    struct p2p_go_neg_results *res)
{
	wpa_printf(MSG_DEBUG, "P2P: Start WPS Enrollee for peer " MACSTR,
		   MAC2STR(res->peer_interface_addr));
	wpa_hexdump_ascii(MSG_DEBUG, "P2P: Start WPS Enrollee for SSID",
			  res->ssid, res->ssid_len);
	wpa_supplicant_ap_deinit(wpa_s);
	wpas_copy_go_neg_results(wpa_s, res);
	if (res->wps_method == WPS_PBC)
		wpas_wps_start_pbc(wpa_s, res->peer_interface_addr, 1);
	else {
		u16 dev_pw_id = DEV_PW_DEFAULT;
		if (wpa_s->p2p_wps_method == WPS_PIN_KEYPAD)
			dev_pw_id = DEV_PW_REGISTRAR_SPECIFIED;
		wpas_wps_start_pin(wpa_s, res->peer_interface_addr,
				   wpa_s->p2p_pin, 1, dev_pw_id);
	}
}


static void p2p_go_configured(void *ctx, void *data)
{
	struct wpa_supplicant *wpa_s = ctx;
	struct p2p_go_neg_results *params = data;
	struct wpa_ssid *ssid;
	int network_id = -1;

	ssid = wpa_s->current_ssid;
	if (ssid && ssid->mode == WPAS_MODE_P2P_GO) {
		wpa_printf(MSG_DEBUG, "P2P: Group setup without provisioning");
		if (wpa_s->global->p2p_group_formation == wpa_s)
			wpa_s->global->p2p_group_formation = NULL;
		if (os_strlen(params->passphrase) > 0) {
			wpa_msg(wpa_s->parent, MSG_INFO, P2P_EVENT_GROUP_STARTED
				"%s GO ssid=\"%s\" freq=%d passphrase=\"%s\" "
				"go_dev_addr=" MACSTR "%s", wpa_s->ifname,
				wpa_ssid_txt(ssid->ssid, ssid->ssid_len),
				ssid->frequency, params->passphrase,
				MAC2STR(wpa_s->global->p2p_dev_addr),
				params->persistent_group ? " [PERSISTENT]" :
				"");
		} else {
			char psk[65];
			wpa_snprintf_hex(psk, sizeof(psk), params->psk,
					 sizeof(params->psk));
			wpa_msg(wpa_s->parent, MSG_INFO, P2P_EVENT_GROUP_STARTED
				"%s GO ssid=\"%s\" freq=%d psk=%s "
				"go_dev_addr=" MACSTR "%s", wpa_s->ifname,
				wpa_ssid_txt(ssid->ssid, ssid->ssid_len),
				ssid->frequency, psk,
				MAC2STR(wpa_s->global->p2p_dev_addr),
				params->persistent_group ? " [PERSISTENT]" :
				"");
		}

		if (params->persistent_group)
			network_id = wpas_p2p_store_persistent_group(
				wpa_s->parent, ssid,
				wpa_s->global->p2p_dev_addr);
		if (network_id < 0)
			network_id = ssid->id;
		wpas_notify_p2p_group_started(wpa_s, ssid, network_id, 0);
		wpas_p2p_cross_connect_setup(wpa_s);
		wpas_p2p_set_group_idle_timeout(wpa_s);
		return;
	}

	wpa_printf(MSG_DEBUG, "P2P: Setting up WPS for GO provisioning");
	if (wpa_supplicant_ap_mac_addr_filter(wpa_s,
					      params->peer_interface_addr)) {
		wpa_printf(MSG_DEBUG, "P2P: Failed to setup MAC address "
			   "filtering");
		return;
	}
	if (params->wps_method == WPS_PBC)
		wpa_supplicant_ap_wps_pbc(wpa_s, params->peer_interface_addr,
					  params->peer_device_addr);
	else if (wpa_s->p2p_pin[0])
		wpa_supplicant_ap_wps_pin(wpa_s, params->peer_interface_addr,
					  wpa_s->p2p_pin, NULL, 0, 0);
	os_free(wpa_s->go_params);
	wpa_s->go_params = NULL;
}


static void wpas_start_wps_go(struct wpa_supplicant *wpa_s,
			      struct p2p_go_neg_results *params,
			      int group_formation)
{
	struct wpa_ssid *ssid;

	wpa_dbg(wpa_s, MSG_DEBUG, "P2P: Starting GO");
	if (wpas_copy_go_neg_results(wpa_s, params) < 0) {
		wpa_dbg(wpa_s, MSG_DEBUG, "P2P: Could not copy GO Negotiation "
			"results");
		return;
	}

	ssid = wpa_config_add_network(wpa_s->conf);
	if (ssid == NULL) {
		wpa_dbg(wpa_s, MSG_DEBUG, "P2P: Could not add network for GO");
		return;
	}

	wpa_s->show_group_started = 0;

	wpa_config_set_network_defaults(ssid);
	ssid->temporary = 1;
	ssid->p2p_group = 1;
	ssid->p2p_persistent_group = params->persistent_group;
	ssid->mode = group_formation ? WPAS_MODE_P2P_GROUP_FORMATION :
		WPAS_MODE_P2P_GO;
	ssid->frequency = params->freq;
	ssid->ht40 = params->ht40;
	ssid->ssid = os_zalloc(params->ssid_len + 1);
	if (ssid->ssid) {
		os_memcpy(ssid->ssid, params->ssid, params->ssid_len);
		ssid->ssid_len = params->ssid_len;
	}
	ssid->auth_alg = WPA_AUTH_ALG_OPEN;
	ssid->key_mgmt = WPA_KEY_MGMT_PSK;
	ssid->proto = WPA_PROTO_RSN;
	ssid->pairwise_cipher = WPA_CIPHER_CCMP;
	if (os_strlen(params->passphrase) > 0) {
		ssid->passphrase = os_strdup(params->passphrase);
		if (ssid->passphrase == NULL) {
			wpa_msg(wpa_s, MSG_ERROR, "P2P: Failed to copy "
				"passphrase for GO");
			wpa_config_remove_network(wpa_s->conf, ssid->id);
			return;
		}
	} else
		ssid->passphrase = NULL;
	ssid->psk_set = params->psk_set;
	if (ssid->psk_set)
		os_memcpy(ssid->psk, params->psk, sizeof(ssid->psk));
	else if (ssid->passphrase)
		wpa_config_update_psk(ssid);
	ssid->ap_max_inactivity = wpa_s->parent->conf->p2p_go_max_inactivity;

	wpa_s->ap_configured_cb = p2p_go_configured;
	wpa_s->ap_configured_cb_ctx = wpa_s;
	wpa_s->ap_configured_cb_data = wpa_s->go_params;
	wpa_s->connect_without_scan = ssid;
	wpa_s->reassociate = 1;
	wpa_s->disconnected = 0;
	wpa_dbg(wpa_s, MSG_DEBUG, "P2P: Request scan (that will be skipped) to "
		"start GO)");
	wpa_supplicant_req_scan(wpa_s, 0, 0);
}


static void wpas_p2p_clone_config(struct wpa_supplicant *dst,
				  const struct wpa_supplicant *src)
{
	struct wpa_config *d;
	const struct wpa_config *s;

	d = dst->conf;
	s = src->conf;

#define C(n) if (s->n) d->n = os_strdup(s->n)
	C(device_name);
	C(manufacturer);
	C(model_name);
	C(model_number);
	C(serial_number);
	C(config_methods);
#undef C

	os_memcpy(d->device_type, s->device_type, WPS_DEV_TYPE_LEN);
	os_memcpy(d->sec_device_type, s->sec_device_type,
		  sizeof(d->sec_device_type));
	d->num_sec_device_types = s->num_sec_device_types;

	d->p2p_group_idle = s->p2p_group_idle;
	d->p2p_intra_bss = s->p2p_intra_bss;
	d->persistent_reconnect = s->persistent_reconnect;
	d->max_num_sta = s->max_num_sta;
	d->pbc_in_m1 = s->pbc_in_m1;
}


static int wpas_p2p_add_group_interface(struct wpa_supplicant *wpa_s,
					enum wpa_driver_if_type type)
{
	char ifname[120], force_ifname[120];

	if (wpa_s->pending_interface_name[0]) {
		wpa_printf(MSG_DEBUG, "P2P: Pending virtual interface exists "
			   "- skip creation of a new one");
		if (is_zero_ether_addr(wpa_s->pending_interface_addr)) {
			wpa_printf(MSG_DEBUG, "P2P: Pending virtual address "
				   "unknown?! ifname='%s'",
				   wpa_s->pending_interface_name);
			return -1;
		}
		return 0;
	}

	os_snprintf(ifname, sizeof(ifname), "p2p-%s-%d", wpa_s->ifname,
		    wpa_s->p2p_group_idx);
	if (os_strlen(ifname) >= IFNAMSIZ &&
	    os_strlen(wpa_s->ifname) < IFNAMSIZ) {
		/* Try to avoid going over the IFNAMSIZ length limit */
		os_snprintf(ifname, sizeof(ifname), "p2p-%d",
			    wpa_s->p2p_group_idx);
	}
	force_ifname[0] = '\0';

	wpa_printf(MSG_DEBUG, "P2P: Create a new interface %s for the group",
		   ifname);
	wpa_s->p2p_group_idx++;

	wpa_s->pending_interface_type = type;
	if (wpa_drv_if_add(wpa_s, type, ifname, NULL, NULL, force_ifname,
			   wpa_s->pending_interface_addr, NULL) < 0) {
		wpa_printf(MSG_ERROR, "P2P: Failed to create new group "
			   "interface");
		return -1;
	}

	if (force_ifname[0]) {
		wpa_printf(MSG_DEBUG, "P2P: Driver forced interface name %s",
			   force_ifname);
		os_strlcpy(wpa_s->pending_interface_name, force_ifname,
			   sizeof(wpa_s->pending_interface_name));
	} else
		os_strlcpy(wpa_s->pending_interface_name, ifname,
			   sizeof(wpa_s->pending_interface_name));
	wpa_printf(MSG_DEBUG, "P2P: Created pending virtual interface %s addr "
		   MACSTR, wpa_s->pending_interface_name,
		   MAC2STR(wpa_s->pending_interface_addr));

	return 0;
}


static void wpas_p2p_remove_pending_group_interface(
	struct wpa_supplicant *wpa_s)
{
	if (!wpa_s->pending_interface_name[0] ||
	    is_zero_ether_addr(wpa_s->pending_interface_addr))
		return; /* No pending virtual interface */

	wpa_printf(MSG_DEBUG, "P2P: Removing pending group interface %s",
		   wpa_s->pending_interface_name);
	wpa_drv_if_remove(wpa_s, wpa_s->pending_interface_type,
			  wpa_s->pending_interface_name);
	os_memset(wpa_s->pending_interface_addr, 0, ETH_ALEN);
	wpa_s->pending_interface_name[0] = '\0';
}


static struct wpa_supplicant *
wpas_p2p_init_group_interface(struct wpa_supplicant *wpa_s, int go)
{
	struct wpa_interface iface;
	struct wpa_supplicant *group_wpa_s;

	if (!wpa_s->pending_interface_name[0]) {
		wpa_printf(MSG_ERROR, "P2P: No pending group interface");
		if (!wpas_p2p_create_iface(wpa_s))
			return NULL;
		/*
		 * Something has forced us to remove the pending interface; try
		 * to create a new one and hope for the best that we will get
		 * the same local address.
		 */
		if (wpas_p2p_add_group_interface(wpa_s, go ? WPA_IF_P2P_GO :
						 WPA_IF_P2P_CLIENT) < 0)
			return NULL;
	}

	os_memset(&iface, 0, sizeof(iface));
	iface.ifname = wpa_s->pending_interface_name;
	iface.driver = wpa_s->driver->name;
	iface.ctrl_interface = wpa_s->conf->ctrl_interface;
	iface.driver_param = wpa_s->conf->driver_param;
	group_wpa_s = wpa_supplicant_add_iface(wpa_s->global, &iface);
	if (group_wpa_s == NULL) {
		wpa_printf(MSG_ERROR, "P2P: Failed to create new "
			   "wpa_supplicant interface");
		return NULL;
	}
	wpa_s->pending_interface_name[0] = '\0';
	group_wpa_s->parent = wpa_s;
	group_wpa_s->p2p_group_interface = go ? P2P_GROUP_INTERFACE_GO :
		P2P_GROUP_INTERFACE_CLIENT;
	wpa_s->global->p2p_group_formation = group_wpa_s;

	wpas_p2p_clone_config(group_wpa_s, wpa_s);

	return group_wpa_s;
}


static void wpas_p2p_group_formation_timeout(void *eloop_ctx,
					     void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	wpa_printf(MSG_DEBUG, "P2P: Group Formation timed out");
	if (wpa_s->global->p2p)
		p2p_group_formation_failed(wpa_s->global->p2p);
	else if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT)
		wpa_drv_p2p_group_formation_failed(wpa_s);
	wpas_group_formation_completed(wpa_s, 0);
}


void wpas_go_neg_completed(void *ctx, struct p2p_go_neg_results *res)
{
	struct wpa_supplicant *wpa_s = ctx;

	if (wpa_s->off_channel_freq || wpa_s->roc_waiting_drv_freq) {
		wpa_drv_cancel_remain_on_channel(wpa_s);
		wpa_s->off_channel_freq = 0;
		wpa_s->roc_waiting_drv_freq = 0;
	}

	if (res->status) {
		wpa_msg(wpa_s, MSG_INFO, P2P_EVENT_GO_NEG_FAILURE "status=%d",
			res->status);
		wpas_notify_p2p_go_neg_completed(wpa_s, res);
		wpas_p2p_remove_pending_group_interface(wpa_s);
		return;
	}

	if (wpa_s->p2p_go_ht40)
		res->ht40 = 1;

	wpa_msg(wpa_s, MSG_INFO, P2P_EVENT_GO_NEG_SUCCESS);
	wpas_notify_p2p_go_neg_completed(wpa_s, res);

	if (res->role_go && wpa_s->p2p_persistent_id >= 0) {
		struct wpa_ssid *ssid;
		ssid = wpa_config_get_network(wpa_s->conf,
					      wpa_s->p2p_persistent_id);
		if (ssid && ssid->disabled == 2 &&
		    ssid->mode == WPAS_MODE_P2P_GO && ssid->passphrase) {
			size_t len = os_strlen(ssid->passphrase);
			wpa_printf(MSG_DEBUG, "P2P: Override passphrase based "
				   "on requested persistent group");
			os_memcpy(res->passphrase, ssid->passphrase, len);
			res->passphrase[len] = '\0';
		}
	}

	if (wpa_s->create_p2p_iface) {
		struct wpa_supplicant *group_wpa_s =
			wpas_p2p_init_group_interface(wpa_s, res->role_go);
		if (group_wpa_s == NULL) {
			wpas_p2p_remove_pending_group_interface(wpa_s);
			return;
		}
		if (group_wpa_s != wpa_s) {
			os_memcpy(group_wpa_s->p2p_pin, wpa_s->p2p_pin,
				  sizeof(group_wpa_s->p2p_pin));
			group_wpa_s->p2p_wps_method = wpa_s->p2p_wps_method;
		}
		os_memset(wpa_s->pending_interface_addr, 0, ETH_ALEN);
		wpa_s->pending_interface_name[0] = '\0';
		group_wpa_s->p2p_in_provisioning = 1;

		if (res->role_go)
			wpas_start_wps_go(group_wpa_s, res, 1);
		else
			wpas_start_wps_enrollee(group_wpa_s, res);
	} else {
		wpa_s->p2p_in_provisioning = 1;
		wpa_s->global->p2p_group_formation = wpa_s;

		if (res->role_go)
			wpas_start_wps_go(wpa_s, res, 1);
		else
			wpas_start_wps_enrollee(ctx, res);
	}

	wpa_s->p2p_long_listen = 0;
	eloop_cancel_timeout(wpas_p2p_long_listen_timeout, wpa_s, NULL);

	eloop_cancel_timeout(wpas_p2p_group_formation_timeout, wpa_s, NULL);
	eloop_register_timeout(15 + res->peer_config_timeout / 100,
			       (res->peer_config_timeout % 100) * 10000,
			       wpas_p2p_group_formation_timeout, wpa_s, NULL);
}


void wpas_go_neg_req_rx(void *ctx, const u8 *src, u16 dev_passwd_id)
{
	struct wpa_supplicant *wpa_s = ctx;
	wpa_msg(wpa_s, MSG_INFO, P2P_EVENT_GO_NEG_REQUEST MACSTR
		" dev_passwd_id=%u", MAC2STR(src), dev_passwd_id);

	wpas_notify_p2p_go_neg_req(wpa_s, src, dev_passwd_id);
}


void wpas_dev_found(void *ctx, const u8 *addr,
		    const struct p2p_peer_info *info,
		    int new_device)
{
#ifndef CONFIG_NO_STDOUT_DEBUG
	struct wpa_supplicant *wpa_s = ctx;
	char devtype[WPS_DEV_TYPE_BUFSIZE];

	wpa_msg(wpa_s, MSG_INFO, P2P_EVENT_DEVICE_FOUND MACSTR
		" p2p_dev_addr=" MACSTR
		" pri_dev_type=%s name='%s' config_methods=0x%x "
		"dev_capab=0x%x group_capab=0x%x",
		MAC2STR(addr), MAC2STR(info->p2p_device_addr),
		wps_dev_type_bin2str(info->pri_dev_type, devtype,
				     sizeof(devtype)),
		info->device_name, info->config_methods,
		info->dev_capab, info->group_capab);
#endif /* CONFIG_NO_STDOUT_DEBUG */

	wpas_notify_p2p_device_found(ctx, info->p2p_device_addr, new_device);
}


static void wpas_dev_lost(void *ctx, const u8 *dev_addr)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpa_msg(wpa_s, MSG_INFO, P2P_EVENT_DEVICE_LOST
		"p2p_dev_addr=" MACSTR, MAC2STR(dev_addr));

	wpas_notify_p2p_device_lost(wpa_s, dev_addr);
}


static int wpas_start_listen(void *ctx, unsigned int freq,
			     unsigned int duration,
			     const struct wpabuf *probe_resp_ie)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpa_drv_set_ap_wps_ie(wpa_s, NULL, probe_resp_ie, NULL);

	if (wpa_drv_probe_req_report(wpa_s, 1) < 0) {
		wpa_printf(MSG_DEBUG, "P2P: Failed to request the driver to "
			   "report received Probe Request frames");
		return -1;
	}

	wpa_s->pending_listen_freq = freq;
	wpa_s->pending_listen_duration = duration;

	if (wpa_drv_remain_on_channel(wpa_s, freq, duration) < 0) {
		wpa_printf(MSG_DEBUG, "P2P: Failed to request the driver "
			   "to remain on channel (%u MHz) for Listen "
			   "state", freq);
		wpa_s->pending_listen_freq = 0;
		return -1;
	}
	wpa_s->off_channel_freq = 0;
	wpa_s->roc_waiting_drv_freq = freq;

	return 0;
}


static void wpas_stop_listen(void *ctx)
{
	struct wpa_supplicant *wpa_s = ctx;
	if (wpa_s->off_channel_freq || wpa_s->roc_waiting_drv_freq) {
		wpa_drv_cancel_remain_on_channel(wpa_s);
		wpa_s->off_channel_freq = 0;
		wpa_s->roc_waiting_drv_freq = 0;
	}
	wpa_drv_set_ap_wps_ie(wpa_s, NULL, NULL, NULL);
	wpa_drv_probe_req_report(wpa_s, 0);
}


static int wpas_send_probe_resp(void *ctx, const struct wpabuf *buf)
{
	struct wpa_supplicant *wpa_s = ctx;
	return wpa_drv_send_mlme(wpa_s, wpabuf_head(buf), wpabuf_len(buf), 1);
}


/*
 * DNS Header section is used only to calculate compression pointers, so the
 * contents of this data does not matter, but the length needs to be reserved
 * in the virtual packet.
 */
#define DNS_HEADER_LEN 12

/*
 * 27-octet in-memory packet from P2P specification containing two implied
 * queries for _tcp.lcoal. PTR IN and _udp.local. PTR IN
 */
#define P2P_SD_IN_MEMORY_LEN 27

static int p2p_sd_dns_uncompress_label(char **upos, char *uend, u8 *start,
				       u8 **spos, const u8 *end)
{
	while (*spos < end) {
		u8 val = ((*spos)[0] & 0xc0) >> 6;
		int len;

		if (val == 1 || val == 2) {
			/* These are reserved values in RFC 1035 */
			wpa_printf(MSG_DEBUG, "P2P: Invalid domain name "
				   "sequence starting with 0x%x", val);
			return -1;
		}

		if (val == 3) {
			u16 offset;
			u8 *spos_tmp;

			/* Offset */
			if (*spos + 2 > end) {
				wpa_printf(MSG_DEBUG, "P2P: No room for full "
					   "DNS offset field");
				return -1;
			}

			offset = (((*spos)[0] & 0x3f) << 8) | (*spos)[1];
			if (offset >= *spos - start) {
				wpa_printf(MSG_DEBUG, "P2P: Invalid DNS "
					   "pointer offset %u", offset);
				return -1;
			}

			(*spos) += 2;
			spos_tmp = start + offset;
			return p2p_sd_dns_uncompress_label(upos, uend, start,
							   &spos_tmp,
							   *spos - 2);
		}

		/* Label */
		len = (*spos)[0] & 0x3f;
		if (len == 0)
			return 0;

		(*spos)++;
		if (*spos + len > end) {
			wpa_printf(MSG_DEBUG, "P2P: Invalid domain name "
				   "sequence - no room for label with length "
				   "%u", len);
			return -1;
		}

		if (*upos + len + 2 > uend)
			return -2;

		os_memcpy(*upos, *spos, len);
		*spos += len;
		*upos += len;
		(*upos)[0] = '.';
		(*upos)++;
		(*upos)[0] = '\0';
	}

	return 0;
}


/* Uncompress domain names per RFC 1035 using the P2P SD in-memory packet.
 * Returns -1 on parsing error (invalid input sequence), -2 if output buffer is
 * not large enough */
static int p2p_sd_dns_uncompress(char *buf, size_t buf_len, const u8 *msg,
				 size_t msg_len, size_t offset)
{
	/* 27-octet in-memory packet from P2P specification */
	const char *prefix = "\x04_tcp\x05local\x00\x00\x0C\x00\x01"
		"\x04_udp\xC0\x11\x00\x0C\x00\x01";
	u8 *tmp, *end, *spos;
	char *upos, *uend;
	int ret = 0;

	if (buf_len < 2)
		return -1;
	if (offset > msg_len)
		return -1;

	tmp = os_malloc(DNS_HEADER_LEN + P2P_SD_IN_MEMORY_LEN + msg_len);
	if (tmp == NULL)
		return -1;
	spos = tmp + DNS_HEADER_LEN + P2P_SD_IN_MEMORY_LEN;
	end = spos + msg_len;
	spos += offset;

	os_memset(tmp, 0, DNS_HEADER_LEN);
	os_memcpy(tmp + DNS_HEADER_LEN, prefix, P2P_SD_IN_MEMORY_LEN);
	os_memcpy(tmp + DNS_HEADER_LEN + P2P_SD_IN_MEMORY_LEN, msg, msg_len);

	upos = buf;
	uend = buf + buf_len;

	ret = p2p_sd_dns_uncompress_label(&upos, uend, tmp, &spos, end);
	if (ret) {
		os_free(tmp);
		return ret;
	}

	if (upos == buf) {
		upos[0] = '.';
		upos[1] = '\0';
	} else if (upos[-1] == '.')
		upos[-1] = '\0';

	os_free(tmp);
	return 0;
}


static struct p2p_srv_bonjour *
wpas_p2p_service_get_bonjour(struct wpa_supplicant *wpa_s,
			     const struct wpabuf *query)
{
	struct p2p_srv_bonjour *bsrv;
	size_t len;

	len = wpabuf_len(query);
	dl_list_for_each(bsrv, &wpa_s->global->p2p_srv_bonjour,
			 struct p2p_srv_bonjour, list) {
		if (len == wpabuf_len(bsrv->query) &&
		    os_memcmp(wpabuf_head(query), wpabuf_head(bsrv->query),
			      len) == 0)
			return bsrv;
	}
	return NULL;
}


static struct p2p_srv_upnp *
wpas_p2p_service_get_upnp(struct wpa_supplicant *wpa_s, u8 version,
			  const char *service)
{
	struct p2p_srv_upnp *usrv;

	dl_list_for_each(usrv, &wpa_s->global->p2p_srv_upnp,
			 struct p2p_srv_upnp, list) {
		if (version == usrv->version &&
		    os_strcmp(service, usrv->service) == 0)
			return usrv;
	}
	return NULL;
}


static void wpas_sd_add_proto_not_avail(struct wpabuf *resp, u8 srv_proto,
					u8 srv_trans_id)
{
	u8 *len_pos;

	if (wpabuf_tailroom(resp) < 5)
		return;

	/* Length (to be filled) */
	len_pos = wpabuf_put(resp, 2);
	wpabuf_put_u8(resp, srv_proto);
	wpabuf_put_u8(resp, srv_trans_id);
	/* Status Code */
	wpabuf_put_u8(resp, P2P_SD_PROTO_NOT_AVAILABLE);
	/* Response Data: empty */
	WPA_PUT_LE16(len_pos, (u8 *) wpabuf_put(resp, 0) - len_pos - 2);
}


static void wpas_sd_all_bonjour(struct wpa_supplicant *wpa_s,
				struct wpabuf *resp, u8 srv_trans_id)
{
	struct p2p_srv_bonjour *bsrv;
	u8 *len_pos;

	wpa_printf(MSG_DEBUG, "P2P: SD Request for all Bonjour services");

	if (dl_list_empty(&wpa_s->global->p2p_srv_bonjour)) {
		wpa_printf(MSG_DEBUG, "P2P: Bonjour protocol not available");
		return;
	}

	dl_list_for_each(bsrv, &wpa_s->global->p2p_srv_bonjour,
			 struct p2p_srv_bonjour, list) {
		if (wpabuf_tailroom(resp) <
		    5 + wpabuf_len(bsrv->query) + wpabuf_len(bsrv->resp))
			return;
		/* Length (to be filled) */
		len_pos = wpabuf_put(resp, 2);
		wpabuf_put_u8(resp, P2P_SERV_BONJOUR);
		wpabuf_put_u8(resp, srv_trans_id);
		/* Status Code */
		wpabuf_put_u8(resp, P2P_SD_SUCCESS);
		wpa_hexdump_ascii(MSG_DEBUG, "P2P: Matching Bonjour service",
				  wpabuf_head(bsrv->resp),
				  wpabuf_len(bsrv->resp));
		/* Response Data */
		wpabuf_put_buf(resp, bsrv->query); /* Key */
		wpabuf_put_buf(resp, bsrv->resp); /* Value */
		WPA_PUT_LE16(len_pos, (u8 *) wpabuf_put(resp, 0) - len_pos -
			     2);
	}
}


static int match_bonjour_query(struct p2p_srv_bonjour *bsrv, const u8 *query,
			       size_t query_len)
{
	char str_rx[256], str_srv[256];

	if (query_len < 3 || wpabuf_len(bsrv->query) < 3)
		return 0; /* Too short to include DNS Type and Version */
	if (os_memcmp(query + query_len - 3,
		      wpabuf_head_u8(bsrv->query) + wpabuf_len(bsrv->query) - 3,
		      3) != 0)
		return 0; /* Mismatch in DNS Type or Version */
	if (query_len == wpabuf_len(bsrv->query) &&
	    os_memcmp(query, wpabuf_head(bsrv->query), query_len - 3) == 0)
		return 1; /* Binary match */

	if (p2p_sd_dns_uncompress(str_rx, sizeof(str_rx), query, query_len - 3,
				  0))
		return 0; /* Failed to uncompress query */
	if (p2p_sd_dns_uncompress(str_srv, sizeof(str_srv),
				  wpabuf_head(bsrv->query),
				  wpabuf_len(bsrv->query) - 3, 0))
		return 0; /* Failed to uncompress service */

	return os_strcmp(str_rx, str_srv) == 0;
}


static void wpas_sd_req_bonjour(struct wpa_supplicant *wpa_s,
				struct wpabuf *resp, u8 srv_trans_id,
				const u8 *query, size_t query_len)
{
	struct p2p_srv_bonjour *bsrv;
	u8 *len_pos;
	int matches = 0;

	wpa_hexdump_ascii(MSG_DEBUG, "P2P: SD Request for Bonjour",
			  query, query_len);
	if (dl_list_empty(&wpa_s->global->p2p_srv_bonjour)) {
		wpa_printf(MSG_DEBUG, "P2P: Bonjour protocol not available");
		wpas_sd_add_proto_not_avail(resp, P2P_SERV_BONJOUR,
					    srv_trans_id);
		return;
	}

	if (query_len == 0) {
		wpas_sd_all_bonjour(wpa_s, resp, srv_trans_id);
		return;
	}

	dl_list_for_each(bsrv, &wpa_s->global->p2p_srv_bonjour,
			 struct p2p_srv_bonjour, list) {
		if (!match_bonjour_query(bsrv, query, query_len))
			continue;

		if (wpabuf_tailroom(resp) <
		    5 + query_len + wpabuf_len(bsrv->resp))
			return;

		matches++;

		/* Length (to be filled) */
		len_pos = wpabuf_put(resp, 2);
		wpabuf_put_u8(resp, P2P_SERV_BONJOUR);
		wpabuf_put_u8(resp, srv_trans_id);

		/* Status Code */
		wpabuf_put_u8(resp, P2P_SD_SUCCESS);
		wpa_hexdump_ascii(MSG_DEBUG, "P2P: Matching Bonjour service",
				  wpabuf_head(bsrv->resp),
				  wpabuf_len(bsrv->resp));

		/* Response Data */
		wpabuf_put_data(resp, query, query_len); /* Key */
		wpabuf_put_buf(resp, bsrv->resp); /* Value */

		WPA_PUT_LE16(len_pos, (u8 *) wpabuf_put(resp, 0) - len_pos - 2);
	}

	if (matches == 0) {
		wpa_printf(MSG_DEBUG, "P2P: Requested Bonjour service not "
			   "available");
		if (wpabuf_tailroom(resp) < 5)
			return;

		/* Length (to be filled) */
		len_pos = wpabuf_put(resp, 2);
		wpabuf_put_u8(resp, P2P_SERV_BONJOUR);
		wpabuf_put_u8(resp, srv_trans_id);

		/* Status Code */
		wpabuf_put_u8(resp, P2P_SD_REQUESTED_INFO_NOT_AVAILABLE);
		/* Response Data: empty */
		WPA_PUT_LE16(len_pos, (u8 *) wpabuf_put(resp, 0) - len_pos -
			     2);
	}
}


static void wpas_sd_all_upnp(struct wpa_supplicant *wpa_s,
			     struct wpabuf *resp, u8 srv_trans_id)
{
	struct p2p_srv_upnp *usrv;
	u8 *len_pos;

	wpa_printf(MSG_DEBUG, "P2P: SD Request for all UPnP services");

	if (dl_list_empty(&wpa_s->global->p2p_srv_upnp)) {
		wpa_printf(MSG_DEBUG, "P2P: UPnP protocol not available");
		return;
	}

	dl_list_for_each(usrv, &wpa_s->global->p2p_srv_upnp,
			 struct p2p_srv_upnp, list) {
		if (wpabuf_tailroom(resp) < 5 + 1 + os_strlen(usrv->service))
			return;

		/* Length (to be filled) */
		len_pos = wpabuf_put(resp, 2);
		wpabuf_put_u8(resp, P2P_SERV_UPNP);
		wpabuf_put_u8(resp, srv_trans_id);

		/* Status Code */
		wpabuf_put_u8(resp, P2P_SD_SUCCESS);
		/* Response Data */
		wpabuf_put_u8(resp, usrv->version);
		wpa_printf(MSG_DEBUG, "P2P: Matching UPnP Service: %s",
			   usrv->service);
		wpabuf_put_str(resp, usrv->service);
		WPA_PUT_LE16(len_pos, (u8 *) wpabuf_put(resp, 0) - len_pos -
			     2);
	}
}


static void wpas_sd_req_upnp(struct wpa_supplicant *wpa_s,
			     struct wpabuf *resp, u8 srv_trans_id,
			     const u8 *query, size_t query_len)
{
	struct p2p_srv_upnp *usrv;
	u8 *len_pos;
	u8 version;
	char *str;
	int count = 0;

	wpa_hexdump_ascii(MSG_DEBUG, "P2P: SD Request for UPnP",
			  query, query_len);

	if (dl_list_empty(&wpa_s->global->p2p_srv_upnp)) {
		wpa_printf(MSG_DEBUG, "P2P: UPnP protocol not available");
		wpas_sd_add_proto_not_avail(resp, P2P_SERV_UPNP,
					    srv_trans_id);
		return;
	}

	if (query_len == 0) {
		wpas_sd_all_upnp(wpa_s, resp, srv_trans_id);
		return;
	}

	if (wpabuf_tailroom(resp) < 5)
		return;

	/* Length (to be filled) */
	len_pos = wpabuf_put(resp, 2);
	wpabuf_put_u8(resp, P2P_SERV_UPNP);
	wpabuf_put_u8(resp, srv_trans_id);

	version = query[0];
	str = os_malloc(query_len);
	if (str == NULL)
		return;
	os_memcpy(str, query + 1, query_len - 1);
	str[query_len - 1] = '\0';

	dl_list_for_each(usrv, &wpa_s->global->p2p_srv_upnp,
			 struct p2p_srv_upnp, list) {
		if (version != usrv->version)
			continue;

		if (os_strcmp(str, "ssdp:all") != 0 &&
		    os_strstr(usrv->service, str) == NULL)
			continue;

		if (wpabuf_tailroom(resp) < 2)
			break;
		if (count == 0) {
			/* Status Code */
			wpabuf_put_u8(resp, P2P_SD_SUCCESS);
			/* Response Data */
			wpabuf_put_u8(resp, version);
		} else
			wpabuf_put_u8(resp, ',');

		count++;

		wpa_printf(MSG_DEBUG, "P2P: Matching UPnP Service: %s",
			   usrv->service);
		if (wpabuf_tailroom(resp) < os_strlen(usrv->service))
			break;
		wpabuf_put_str(resp, usrv->service);
	}
	os_free(str);

	if (count == 0) {
		wpa_printf(MSG_DEBUG, "P2P: Requested UPnP service not "
			   "available");
		/* Status Code */
		wpabuf_put_u8(resp, P2P_SD_REQUESTED_INFO_NOT_AVAILABLE);
		/* Response Data: empty */
	}

	WPA_PUT_LE16(len_pos, (u8 *) wpabuf_put(resp, 0) - len_pos - 2);
}


#ifdef CONFIG_WIFI_DISPLAY
static void wpas_sd_req_wfd(struct wpa_supplicant *wpa_s,
			    struct wpabuf *resp, u8 srv_trans_id,
			    const u8 *query, size_t query_len)
{
	const u8 *pos;
	u8 role;
	u8 *len_pos;

	wpa_hexdump(MSG_DEBUG, "P2P: SD Request for WFD", query, query_len);

	if (!wpa_s->global->wifi_display) {
		wpa_printf(MSG_DEBUG, "P2P: WFD protocol not available");
		wpas_sd_add_proto_not_avail(resp, P2P_SERV_WIFI_DISPLAY,
					    srv_trans_id);
		return;
	}

	if (query_len < 1) {
		wpa_printf(MSG_DEBUG, "P2P: Missing WFD Requested Device "
			   "Role");
		return;
	}

	if (wpabuf_tailroom(resp) < 5)
		return;

	pos = query;
	role = *pos++;
	wpa_printf(MSG_DEBUG, "P2P: WSD for device role 0x%x", role);

	/* TODO: role specific handling */

	/* Length (to be filled) */
	len_pos = wpabuf_put(resp, 2);
	wpabuf_put_u8(resp, P2P_SERV_WIFI_DISPLAY);
	wpabuf_put_u8(resp, srv_trans_id);
	wpabuf_put_u8(resp, P2P_SD_SUCCESS); /* Status Code */

	while (pos < query + query_len) {
		if (*pos < MAX_WFD_SUBELEMS &&
		    wpa_s->global->wfd_subelem[*pos] &&
		    wpabuf_tailroom(resp) >=
		    wpabuf_len(wpa_s->global->wfd_subelem[*pos])) {
			wpa_printf(MSG_DEBUG, "P2P: Add WSD response "
				   "subelement %u", *pos);
			wpabuf_put_buf(resp, wpa_s->global->wfd_subelem[*pos]);
		}
		pos++;
	}

	WPA_PUT_LE16(len_pos, (u8 *) wpabuf_put(resp, 0) - len_pos - 2);
}
#endif /* CONFIG_WIFI_DISPLAY */


void wpas_sd_request(void *ctx, int freq, const u8 *sa, u8 dialog_token,
		     u16 update_indic, const u8 *tlvs, size_t tlvs_len)
{
	struct wpa_supplicant *wpa_s = ctx;
	const u8 *pos = tlvs;
	const u8 *end = tlvs + tlvs_len;
	const u8 *tlv_end;
	u16 slen;
	struct wpabuf *resp;
	u8 srv_proto, srv_trans_id;
	size_t buf_len;
	char *buf;

	wpa_hexdump(MSG_MSGDUMP, "P2P: Service Discovery Request TLVs",
		    tlvs, tlvs_len);
	buf_len = 2 * tlvs_len + 1;
	buf = os_malloc(buf_len);
	if (buf) {
		wpa_snprintf_hex(buf, buf_len, tlvs, tlvs_len);
		wpa_msg_ctrl(wpa_s, MSG_INFO, P2P_EVENT_SERV_DISC_REQ "%d "
			     MACSTR " %u %u %s",
			     freq, MAC2STR(sa), dialog_token, update_indic,
			     buf);
		os_free(buf);
	}

	if (wpa_s->p2p_sd_over_ctrl_iface) {
		wpas_notify_p2p_sd_request(wpa_s, freq, sa, dialog_token,
					   update_indic, tlvs, tlvs_len);
		return; /* to be processed by an external program */
	}

	resp = wpabuf_alloc(10000);
	if (resp == NULL)
		return;

	while (pos + 1 < end) {
		wpa_printf(MSG_DEBUG, "P2P: Service Request TLV");
		slen = WPA_GET_LE16(pos);
		pos += 2;
		if (pos + slen > end || slen < 2) {
			wpa_printf(MSG_DEBUG, "P2P: Unexpected Query Data "
				   "length");
			wpabuf_free(resp);
			return;
		}
		tlv_end = pos + slen;

		srv_proto = *pos++;
		wpa_printf(MSG_DEBUG, "P2P: Service Protocol Type %u",
			   srv_proto);
		srv_trans_id = *pos++;
		wpa_printf(MSG_DEBUG, "P2P: Service Transaction ID %u",
			   srv_trans_id);

		wpa_hexdump(MSG_MSGDUMP, "P2P: Query Data",
			    pos, tlv_end - pos);


		if (wpa_s->force_long_sd) {
			wpa_printf(MSG_DEBUG, "P2P: SD test - force long "
				   "response");
			wpas_sd_all_bonjour(wpa_s, resp, srv_trans_id);
			wpas_sd_all_upnp(wpa_s, resp, srv_trans_id);
			goto done;
		}

		switch (srv_proto) {
		case P2P_SERV_ALL_SERVICES:
			wpa_printf(MSG_DEBUG, "P2P: Service Discovery Request "
				   "for all services");
			if (dl_list_empty(&wpa_s->global->p2p_srv_upnp) &&
			    dl_list_empty(&wpa_s->global->p2p_srv_bonjour)) {
				wpa_printf(MSG_DEBUG, "P2P: No service "
					   "discovery protocols available");
				wpas_sd_add_proto_not_avail(
					resp, P2P_SERV_ALL_SERVICES,
					srv_trans_id);
				break;
			}
			wpas_sd_all_bonjour(wpa_s, resp, srv_trans_id);
			wpas_sd_all_upnp(wpa_s, resp, srv_trans_id);
			break;
		case P2P_SERV_BONJOUR:
			wpas_sd_req_bonjour(wpa_s, resp, srv_trans_id,
					    pos, tlv_end - pos);
			break;
		case P2P_SERV_UPNP:
			wpas_sd_req_upnp(wpa_s, resp, srv_trans_id,
					 pos, tlv_end - pos);
			break;
#ifdef CONFIG_WIFI_DISPLAY
		case P2P_SERV_WIFI_DISPLAY:
			wpas_sd_req_wfd(wpa_s, resp, srv_trans_id,
					pos, tlv_end - pos);
			break;
#endif /* CONFIG_WIFI_DISPLAY */
		default:
			wpa_printf(MSG_DEBUG, "P2P: Unavailable service "
				   "protocol %u", srv_proto);
			wpas_sd_add_proto_not_avail(resp, srv_proto,
						    srv_trans_id);
			break;
		}

		pos = tlv_end;
	}

done:
	wpas_notify_p2p_sd_request(wpa_s, freq, sa, dialog_token,
				   update_indic, tlvs, tlvs_len);

	wpas_p2p_sd_response(wpa_s, freq, sa, dialog_token, resp);

	wpabuf_free(resp);
}


void wpas_sd_response(void *ctx, const u8 *sa, u16 update_indic,
		      const u8 *tlvs, size_t tlvs_len)
{
	struct wpa_supplicant *wpa_s = ctx;
	const u8 *pos = tlvs;
	const u8 *end = tlvs + tlvs_len;
	const u8 *tlv_end;
	u16 slen;
	size_t buf_len;
	char *buf;

	wpa_hexdump(MSG_MSGDUMP, "P2P: Service Discovery Response TLVs",
		    tlvs, tlvs_len);
	if (tlvs_len > 1500) {
		/* TODO: better way for handling this */
		wpa_msg_ctrl(wpa_s, MSG_INFO,
			     P2P_EVENT_SERV_DISC_RESP MACSTR
			     " %u <long response: %u bytes>",
			     MAC2STR(sa), update_indic,
			     (unsigned int) tlvs_len);
	} else {
		buf_len = 2 * tlvs_len + 1;
		buf = os_malloc(buf_len);
		if (buf) {
			wpa_snprintf_hex(buf, buf_len, tlvs, tlvs_len);
			wpa_msg_ctrl(wpa_s, MSG_INFO,
				     P2P_EVENT_SERV_DISC_RESP MACSTR " %u %s",
				     MAC2STR(sa), update_indic, buf);
			os_free(buf);
		}
	}

	while (pos < end) {
		u8 srv_proto, srv_trans_id, status;

		wpa_printf(MSG_DEBUG, "P2P: Service Response TLV");
		slen = WPA_GET_LE16(pos);
		pos += 2;
		if (pos + slen > end || slen < 3) {
			wpa_printf(MSG_DEBUG, "P2P: Unexpected Response Data "
				   "length");
			return;
		}
		tlv_end = pos + slen;

		srv_proto = *pos++;
		wpa_printf(MSG_DEBUG, "P2P: Service Protocol Type %u",
			   srv_proto);
		srv_trans_id = *pos++;
		wpa_printf(MSG_DEBUG, "P2P: Service Transaction ID %u",
			   srv_trans_id);
		status = *pos++;
		wpa_printf(MSG_DEBUG, "P2P: Status Code ID %u",
			   status);

		wpa_hexdump(MSG_MSGDUMP, "P2P: Response Data",
			    pos, tlv_end - pos);

		pos = tlv_end;
	}

	wpas_notify_p2p_sd_response(wpa_s, sa, update_indic, tlvs, tlvs_len);
}


u64 wpas_p2p_sd_request(struct wpa_supplicant *wpa_s, const u8 *dst,
			const struct wpabuf *tlvs)
{
	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT)
		return wpa_drv_p2p_sd_request(wpa_s, dst, tlvs);
	if (wpa_s->global->p2p_disabled || wpa_s->global->p2p == NULL)
		return 0;
	return (uintptr_t) p2p_sd_request(wpa_s->global->p2p, dst, tlvs);
}


u64 wpas_p2p_sd_request_upnp(struct wpa_supplicant *wpa_s, const u8 *dst,
			     u8 version, const char *query)
{
	struct wpabuf *tlvs;
	u64 ret;

	tlvs = wpabuf_alloc(2 + 1 + 1 + 1 + os_strlen(query));
	if (tlvs == NULL)
		return 0;
	wpabuf_put_le16(tlvs, 1 + 1 + 1 + os_strlen(query));
	wpabuf_put_u8(tlvs, P2P_SERV_UPNP); /* Service Protocol Type */
	wpabuf_put_u8(tlvs, 1); /* Service Transaction ID */
	wpabuf_put_u8(tlvs, version);
	wpabuf_put_str(tlvs, query);
	ret = wpas_p2p_sd_request(wpa_s, dst, tlvs);
	wpabuf_free(tlvs);
	return ret;
}


#ifdef CONFIG_WIFI_DISPLAY

static u64 wpas_p2p_sd_request_wfd(struct wpa_supplicant *wpa_s, const u8 *dst,
				   const struct wpabuf *tlvs)
{
	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT)
		return 0;
	if (wpa_s->global->p2p_disabled || wpa_s->global->p2p == NULL)
		return 0;
	return (uintptr_t) p2p_sd_request_wfd(wpa_s->global->p2p, dst, tlvs);
}


#define MAX_WFD_SD_SUBELEMS 20

static void wfd_add_sd_req_role(struct wpabuf *tlvs, u8 id, u8 role,
				const char *subelems)
{
	u8 *len;
	const char *pos;
	int val;
	int count = 0;

	len = wpabuf_put(tlvs, 2);
	wpabuf_put_u8(tlvs, P2P_SERV_WIFI_DISPLAY); /* Service Protocol Type */
	wpabuf_put_u8(tlvs, id); /* Service Transaction ID */

	wpabuf_put_u8(tlvs, role);

	pos = subelems;
	while (*pos) {
		val = atoi(pos);
		if (val >= 0 && val < 256) {
			wpabuf_put_u8(tlvs, val);
			count++;
			if (count == MAX_WFD_SD_SUBELEMS)
				break;
		}
		pos = os_strchr(pos + 1, ',');
		if (pos == NULL)
			break;
		pos++;
	}

	WPA_PUT_LE16(len, (u8 *) wpabuf_put(tlvs, 0) - len - 2);
}


u64 wpas_p2p_sd_request_wifi_display(struct wpa_supplicant *wpa_s,
				     const u8 *dst, const char *role)
{
	struct wpabuf *tlvs;
	u64 ret;
	const char *subelems;
	u8 id = 1;

	subelems = os_strchr(role, ' ');
	if (subelems == NULL)
		return 0;
	subelems++;

	tlvs = wpabuf_alloc(4 * (2 + 1 + 1 + 1 + MAX_WFD_SD_SUBELEMS));
	if (tlvs == NULL)
		return 0;

	if (os_strstr(role, "[source]"))
		wfd_add_sd_req_role(tlvs, id++, 0x00, subelems);
	if (os_strstr(role, "[pri-sink]"))
		wfd_add_sd_req_role(tlvs, id++, 0x01, subelems);
	if (os_strstr(role, "[sec-sink]"))
		wfd_add_sd_req_role(tlvs, id++, 0x02, subelems);
	if (os_strstr(role, "[source+sink]"))
		wfd_add_sd_req_role(tlvs, id++, 0x03, subelems);

	ret = wpas_p2p_sd_request_wfd(wpa_s, dst, tlvs);
	wpabuf_free(tlvs);
	return ret;
}

#endif /* CONFIG_WIFI_DISPLAY */


int wpas_p2p_sd_cancel_request(struct wpa_supplicant *wpa_s, u64 req)
{
	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT)
		return wpa_drv_p2p_sd_cancel_request(wpa_s, req);
	if (wpa_s->global->p2p_disabled || wpa_s->global->p2p == NULL)
		return -1;
	return p2p_sd_cancel_request(wpa_s->global->p2p,
				     (void *) (uintptr_t) req);
}


void wpas_p2p_sd_response(struct wpa_supplicant *wpa_s, int freq,
			  const u8 *dst, u8 dialog_token,
			  const struct wpabuf *resp_tlvs)
{
	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT) {
		wpa_drv_p2p_sd_response(wpa_s, freq, dst, dialog_token,
					resp_tlvs);
		return;
	}
	if (wpa_s->global->p2p_disabled || wpa_s->global->p2p == NULL)
		return;
	p2p_sd_response(wpa_s->global->p2p, freq, dst, dialog_token,
			resp_tlvs);
}


void wpas_p2p_sd_service_update(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT) {
		wpa_drv_p2p_service_update(wpa_s);
		return;
	}
	if (wpa_s->global->p2p)
		p2p_sd_service_update(wpa_s->global->p2p);
}


static void wpas_p2p_srv_bonjour_free(struct p2p_srv_bonjour *bsrv)
{
	dl_list_del(&bsrv->list);
	wpabuf_free(bsrv->query);
	wpabuf_free(bsrv->resp);
	os_free(bsrv);
}


static void wpas_p2p_srv_upnp_free(struct p2p_srv_upnp *usrv)
{
	dl_list_del(&usrv->list);
	os_free(usrv->service);
	os_free(usrv);
}


void wpas_p2p_service_flush(struct wpa_supplicant *wpa_s)
{
	struct p2p_srv_bonjour *bsrv, *bn;
	struct p2p_srv_upnp *usrv, *un;

	dl_list_for_each_safe(bsrv, bn, &wpa_s->global->p2p_srv_bonjour,
			      struct p2p_srv_bonjour, list)
		wpas_p2p_srv_bonjour_free(bsrv);

	dl_list_for_each_safe(usrv, un, &wpa_s->global->p2p_srv_upnp,
			      struct p2p_srv_upnp, list)
		wpas_p2p_srv_upnp_free(usrv);

	wpas_p2p_sd_service_update(wpa_s);
}


int wpas_p2p_service_add_bonjour(struct wpa_supplicant *wpa_s,
				 struct wpabuf *query, struct wpabuf *resp)
{
	struct p2p_srv_bonjour *bsrv;

	bsrv = os_zalloc(sizeof(*bsrv));
	if (bsrv == NULL)
		return -1;
	bsrv->query = query;
	bsrv->resp = resp;
	dl_list_add(&wpa_s->global->p2p_srv_bonjour, &bsrv->list);

	wpas_p2p_sd_service_update(wpa_s);
	return 0;
}


int wpas_p2p_service_del_bonjour(struct wpa_supplicant *wpa_s,
				 const struct wpabuf *query)
{
	struct p2p_srv_bonjour *bsrv;

	bsrv = wpas_p2p_service_get_bonjour(wpa_s, query);
	if (bsrv == NULL)
		return -1;
	wpas_p2p_srv_bonjour_free(bsrv);
	wpas_p2p_sd_service_update(wpa_s);
	return 0;
}


int wpas_p2p_service_add_upnp(struct wpa_supplicant *wpa_s, u8 version,
			      const char *service)
{
	struct p2p_srv_upnp *usrv;

	if (wpas_p2p_service_get_upnp(wpa_s, version, service))
		return 0; /* Already listed */
	usrv = os_zalloc(sizeof(*usrv));
	if (usrv == NULL)
		return -1;
	usrv->version = version;
	usrv->service = os_strdup(service);
	if (usrv->service == NULL) {
		os_free(usrv);
		return -1;
	}
	dl_list_add(&wpa_s->global->p2p_srv_upnp, &usrv->list);

	wpas_p2p_sd_service_update(wpa_s);
	return 0;
}


int wpas_p2p_service_del_upnp(struct wpa_supplicant *wpa_s, u8 version,
			      const char *service)
{
	struct p2p_srv_upnp *usrv;

	usrv = wpas_p2p_service_get_upnp(wpa_s, version, service);
	if (usrv == NULL)
		return -1;
	wpas_p2p_srv_upnp_free(usrv);
	wpas_p2p_sd_service_update(wpa_s);
	return 0;
}


static void wpas_prov_disc_local_display(struct wpa_supplicant *wpa_s,
					 const u8 *peer, const char *params,
					 unsigned int generated_pin)
{
	wpa_msg(wpa_s, MSG_INFO, P2P_EVENT_PROV_DISC_SHOW_PIN MACSTR " %08d%s",
		MAC2STR(peer), generated_pin, params);
}


static void wpas_prov_disc_local_keypad(struct wpa_supplicant *wpa_s,
					const u8 *peer, const char *params)
{
	wpa_msg(wpa_s, MSG_INFO, P2P_EVENT_PROV_DISC_ENTER_PIN MACSTR "%s",
		MAC2STR(peer), params);
}


void wpas_prov_disc_req(void *ctx, const u8 *peer, u16 config_methods,
			const u8 *dev_addr, const u8 *pri_dev_type,
			const char *dev_name, u16 supp_config_methods,
			u8 dev_capab, u8 group_capab, const u8 *group_id,
			size_t group_id_len)
{
	struct wpa_supplicant *wpa_s = ctx;
	char devtype[WPS_DEV_TYPE_BUFSIZE];
	char params[300];
	u8 empty_dev_type[8];
	unsigned int generated_pin = 0;
	struct wpa_supplicant *group = NULL;

	if (group_id) {
		for (group = wpa_s->global->ifaces; group; group = group->next)
		{
			struct wpa_ssid *s = group->current_ssid;
			if (s != NULL &&
			    s->mode == WPAS_MODE_P2P_GO &&
			    group_id_len - ETH_ALEN == s->ssid_len &&
			    os_memcmp(group_id + ETH_ALEN, s->ssid,
				      s->ssid_len) == 0)
				break;
		}
	}

	if (pri_dev_type == NULL) {
		os_memset(empty_dev_type, 0, sizeof(empty_dev_type));
		pri_dev_type = empty_dev_type;
	}
	os_snprintf(params, sizeof(params), " p2p_dev_addr=" MACSTR
		    " pri_dev_type=%s name='%s' config_methods=0x%x "
		    "dev_capab=0x%x group_capab=0x%x%s%s",
		    MAC2STR(dev_addr),
		    wps_dev_type_bin2str(pri_dev_type, devtype,
					 sizeof(devtype)),
		    dev_name, supp_config_methods, dev_capab, group_capab,
		    group ? " group=" : "",
		    group ? group->ifname : "");
	params[sizeof(params) - 1] = '\0';

	if (config_methods & WPS_CONFIG_DISPLAY) {
		generated_pin = wps_generate_pin();
		wpas_prov_disc_local_display(wpa_s, peer, params,
					     generated_pin);
	} else if (config_methods & WPS_CONFIG_KEYPAD)
		wpas_prov_disc_local_keypad(wpa_s, peer, params);
	else if (config_methods & WPS_CONFIG_PUSHBUTTON)
		wpa_msg(wpa_s, MSG_INFO, P2P_EVENT_PROV_DISC_PBC_REQ MACSTR
			"%s", MAC2STR(peer), params);

	wpas_notify_p2p_provision_discovery(wpa_s, peer, 1 /* request */,
					    P2P_PROV_DISC_SUCCESS,
					    config_methods, generated_pin);
}


void wpas_prov_disc_resp(void *ctx, const u8 *peer, u16 config_methods)
{
	struct wpa_supplicant *wpa_s = ctx;
	unsigned int generated_pin = 0;
	char params[20];

	if (wpa_s->pending_pd_before_join &&
	    (os_memcmp(peer, wpa_s->pending_join_dev_addr, ETH_ALEN) == 0 ||
	     os_memcmp(peer, wpa_s->pending_join_iface_addr, ETH_ALEN) == 0)) {
		wpa_s->pending_pd_before_join = 0;
		wpa_printf(MSG_DEBUG, "P2P: Starting pending "
			   "join-existing-group operation");
		wpas_p2p_join_start(wpa_s);
		return;
	}

	if (wpa_s->pending_pd_use == AUTO_PD_JOIN ||
	    wpa_s->pending_pd_use == AUTO_PD_GO_NEG)
		os_snprintf(params, sizeof(params), " peer_go=%d",
			    wpa_s->pending_pd_use == AUTO_PD_JOIN);
	else
		params[0] = '\0';

	if (config_methods & WPS_CONFIG_DISPLAY)
		wpas_prov_disc_local_keypad(wpa_s, peer, params);
	else if (config_methods & WPS_CONFIG_KEYPAD) {
		generated_pin = wps_generate_pin();
		wpas_prov_disc_local_display(wpa_s, peer, params,
					     generated_pin);
	} else if (config_methods & WPS_CONFIG_PUSHBUTTON)
		wpa_msg(wpa_s, MSG_INFO, P2P_EVENT_PROV_DISC_PBC_RESP MACSTR
			"%s", MAC2STR(peer), params);

	wpas_notify_p2p_provision_discovery(wpa_s, peer, 0 /* response */,
					    P2P_PROV_DISC_SUCCESS,
					    config_methods, generated_pin);
}


static void wpas_prov_disc_fail(void *ctx, const u8 *peer,
				enum p2p_prov_disc_status status)
{
	struct wpa_supplicant *wpa_s = ctx;

	if (wpa_s->p2p_fallback_to_go_neg) {
		wpa_dbg(wpa_s, MSG_DEBUG, "P2P: PD for p2p_connect-auto "
			"failed - fall back to GO Negotiation");
		wpas_p2p_fallback_to_go_neg(wpa_s, 0);
		return;
	}

	if (status == P2P_PROV_DISC_TIMEOUT_JOIN) {
		wpa_s->pending_pd_before_join = 0;
		wpa_printf(MSG_DEBUG, "P2P: Starting pending "
			   "join-existing-group operation (no ACK for PD "
			   "Req attempts)");
		wpas_p2p_join_start(wpa_s);
		return;
	}

	wpa_msg(wpa_s, MSG_INFO, P2P_EVENT_PROV_DISC_FAILURE
		" p2p_dev_addr=" MACSTR " status=%d",
		MAC2STR(peer), status);

	wpas_notify_p2p_provision_discovery(wpa_s, peer, 0 /* response */,
					    status, 0, 0);
}


static u8 wpas_invitation_process(void *ctx, const u8 *sa, const u8 *bssid,
				  const u8 *go_dev_addr, const u8 *ssid,
				  size_t ssid_len, int *go, u8 *group_bssid,
				  int *force_freq, int persistent_group)
{
	struct wpa_supplicant *wpa_s = ctx;
	struct wpa_ssid *s;
	u8 cur_bssid[ETH_ALEN];
	int res;
	struct wpa_supplicant *grp;

	if (!persistent_group) {
		wpa_printf(MSG_DEBUG, "P2P: Invitation from " MACSTR
			   " to join an active group", MAC2STR(sa));
		if (!is_zero_ether_addr(wpa_s->p2p_auth_invite) &&
		    (os_memcmp(go_dev_addr, wpa_s->p2p_auth_invite, ETH_ALEN)
		     == 0 ||
		     os_memcmp(sa, wpa_s->p2p_auth_invite, ETH_ALEN) == 0)) {
			wpa_printf(MSG_DEBUG, "P2P: Accept previously "
				   "authorized invitation");
			goto accept_inv;
		}
		/*
		 * Do not accept the invitation automatically; notify user and
		 * request approval.
		 */
		return P2P_SC_FAIL_INFO_CURRENTLY_UNAVAILABLE;
	}

	grp = wpas_get_p2p_group(wpa_s, ssid, ssid_len, go);
	if (grp) {
		wpa_printf(MSG_DEBUG, "P2P: Accept invitation to already "
			   "running persistent group");
		if (*go)
			os_memcpy(group_bssid, grp->own_addr, ETH_ALEN);
		goto accept_inv;
	}

	if (!wpa_s->conf->persistent_reconnect)
		return P2P_SC_FAIL_INFO_CURRENTLY_UNAVAILABLE;

	for (s = wpa_s->conf->ssid; s; s = s->next) {
		if (s->disabled == 2 &&
		    os_memcmp(s->bssid, go_dev_addr, ETH_ALEN) == 0 &&
		    s->ssid_len == ssid_len &&
		    os_memcmp(ssid, s->ssid, ssid_len) == 0)
			break;
	}

	if (!s) {
		wpa_printf(MSG_DEBUG, "P2P: Invitation from " MACSTR
			   " requested reinvocation of an unknown group",
			   MAC2STR(sa));
		return P2P_SC_FAIL_UNKNOWN_GROUP;
	}

	if (s->mode == WPAS_MODE_P2P_GO && !wpas_p2p_create_iface(wpa_s)) {
		*go = 1;
		if (wpa_s->wpa_state >= WPA_AUTHENTICATING) {
			wpa_printf(MSG_DEBUG, "P2P: The only available "
				   "interface is already in use - reject "
				   "invitation");
			return P2P_SC_FAIL_UNABLE_TO_ACCOMMODATE;
		}
		os_memcpy(group_bssid, wpa_s->own_addr, ETH_ALEN);
	} else if (s->mode == WPAS_MODE_P2P_GO) {
		*go = 1;
		if (wpas_p2p_add_group_interface(wpa_s, WPA_IF_P2P_GO) < 0)
		{
			wpa_printf(MSG_ERROR, "P2P: Failed to allocate a new "
				   "interface address for the group");
			return P2P_SC_FAIL_UNABLE_TO_ACCOMMODATE;
		}
		os_memcpy(group_bssid, wpa_s->pending_interface_addr,
			  ETH_ALEN);
	}

accept_inv:
	if (wpa_s->current_ssid && wpa_drv_get_bssid(wpa_s, cur_bssid) == 0 &&
	    wpa_s->assoc_freq) {
		wpa_printf(MSG_DEBUG, "P2P: Trying to force channel to match "
			   "the channel we are already using");
		*force_freq = wpa_s->assoc_freq;
	}

	res = wpa_drv_shared_freq(wpa_s);
	if (res > 0) {
		wpa_printf(MSG_DEBUG, "P2P: Trying to force channel to match "
			   "with the channel we are already using on a "
			   "shared interface");
		*force_freq = res;
	}

	return P2P_SC_SUCCESS;
}


static void wpas_invitation_received(void *ctx, const u8 *sa, const u8 *bssid,
				     const u8 *ssid, size_t ssid_len,
				     const u8 *go_dev_addr, u8 status,
				     int op_freq)
{
	struct wpa_supplicant *wpa_s = ctx;
	struct wpa_ssid *s;

	for (s = wpa_s->conf->ssid; s; s = s->next) {
		if (s->disabled == 2 &&
		    s->ssid_len == ssid_len &&
		    os_memcmp(ssid, s->ssid, ssid_len) == 0)
			break;
	}

	if (status == P2P_SC_SUCCESS) {
		wpa_printf(MSG_DEBUG, "P2P: Invitation from peer " MACSTR
			   " was accepted; op_freq=%d MHz",
			   MAC2STR(sa), op_freq);
		if (s) {
			int go = s->mode == WPAS_MODE_P2P_GO;
			wpas_p2p_group_add_persistent(
				wpa_s, s, go, go ? op_freq : 0, 0);
		} else if (bssid) {
			wpa_s->user_initiated_pd = 0;
			wpas_p2p_join(wpa_s, bssid, go_dev_addr,
				      wpa_s->p2p_wps_method, 0);
		}
		return;
	}

	if (status != P2P_SC_FAIL_INFO_CURRENTLY_UNAVAILABLE) {
		wpa_printf(MSG_DEBUG, "P2P: Invitation from peer " MACSTR
			   " was rejected (status %u)", MAC2STR(sa), status);
		return;
	}

	if (!s) {
		if (bssid) {
			wpa_msg(wpa_s, MSG_INFO, P2P_EVENT_INVITATION_RECEIVED
				"sa=" MACSTR " go_dev_addr=" MACSTR
				" bssid=" MACSTR " unknown-network",
				MAC2STR(sa), MAC2STR(go_dev_addr),
				MAC2STR(bssid));
		} else {
			wpa_msg(wpa_s, MSG_INFO, P2P_EVENT_INVITATION_RECEIVED
				"sa=" MACSTR " go_dev_addr=" MACSTR
				" unknown-network",
				MAC2STR(sa), MAC2STR(go_dev_addr));
		}
		return;
	}

	wpa_msg(wpa_s, MSG_INFO, P2P_EVENT_INVITATION_RECEIVED "sa=" MACSTR
		" persistent=%d", MAC2STR(sa), s->id);
}


static void wpas_invitation_result(void *ctx, int status, const u8 *bssid)
{
	struct wpa_supplicant *wpa_s = ctx;
	struct wpa_ssid *ssid;

	if (bssid) {
		wpa_msg(wpa_s, MSG_INFO, P2P_EVENT_INVITATION_RESULT
			"status=%d " MACSTR,
			status, MAC2STR(bssid));
	} else {
		wpa_msg(wpa_s, MSG_INFO, P2P_EVENT_INVITATION_RESULT
			"status=%d ", status);
	}
	wpas_notify_p2p_invitation_result(wpa_s, status, bssid);

	if (wpa_s->pending_invite_ssid_id == -1)
		return; /* Invitation to active group */

	if (status != P2P_SC_SUCCESS) {
		wpas_p2p_remove_pending_group_interface(wpa_s);
		return;
	}

	ssid = wpa_config_get_network(wpa_s->conf,
				      wpa_s->pending_invite_ssid_id);
	if (ssid == NULL) {
		wpa_printf(MSG_ERROR, "P2P: Could not find persistent group "
			   "data matching with invitation");
		return;
	}

	/*
	 * The peer could have missed our ctrl::ack frame for Invitation
	 * Response and continue retransmitting the frame. To reduce the
	 * likelihood of the peer not getting successful TX status for the
	 * Invitation Response frame, wait a short time here before starting
	 * the persistent group so that we will remain on the current channel to
	 * acknowledge any possible retransmission from the peer.
	 */
	wpa_dbg(wpa_s, MSG_DEBUG, "P2P: 50 ms wait on current channel before "
		"starting persistent group");
	os_sleep(0, 50000);

	wpas_p2p_group_add_persistent(wpa_s, ssid,
				      ssid->mode == WPAS_MODE_P2P_GO,
				      wpa_s->p2p_persistent_go_freq,
				      wpa_s->p2p_go_ht40);
}


static int wpas_p2p_disallowed_freq(struct wpa_global *global,
				    unsigned int freq)
{
	unsigned int i;

	if (global->p2p_disallow_freq == NULL)
		return 0;

	for (i = 0; i < global->num_p2p_disallow_freq; i++) {
		if (freq >= global->p2p_disallow_freq[i].min &&
		    freq <= global->p2p_disallow_freq[i].max)
			return 1;
	}

	return 0;
}


static void wpas_p2p_add_chan(struct p2p_reg_class *reg, u8 chan)
{
	reg->channel[reg->channels] = chan;
	reg->channels++;
}


static int wpas_p2p_default_channels(struct wpa_supplicant *wpa_s,
				     struct p2p_channels *chan)
{
	int i, cla = 0;

	wpa_printf(MSG_DEBUG, "P2P: Enable operating classes for 2.4 GHz "
		   "band");

	/* Operating class 81 - 2.4 GHz band channels 1..13 */
	chan->reg_class[cla].reg_class = 81;
	chan->reg_class[cla].channels = 0;
	for (i = 0; i < 11; i++) {
		if (!wpas_p2p_disallowed_freq(wpa_s->global, 2412 + i * 5))
			wpas_p2p_add_chan(&chan->reg_class[cla], i + 1);
	}
	if (chan->reg_class[cla].channels)
		cla++;

	wpa_printf(MSG_DEBUG, "P2P: Enable operating classes for lower 5 GHz "
		   "band");

	/* Operating class 115 - 5 GHz, channels 36-48 */
	chan->reg_class[cla].reg_class = 115;
	chan->reg_class[cla].channels = 0;
	if (!wpas_p2p_disallowed_freq(wpa_s->global, 5000 + 36 * 5))
		wpas_p2p_add_chan(&chan->reg_class[cla], 36);
	if (!wpas_p2p_disallowed_freq(wpa_s->global, 5000 + 40 * 5))
		wpas_p2p_add_chan(&chan->reg_class[cla], 40);
	if (!wpas_p2p_disallowed_freq(wpa_s->global, 5000 + 44 * 5))
		wpas_p2p_add_chan(&chan->reg_class[cla], 44);
	if (!wpas_p2p_disallowed_freq(wpa_s->global, 5000 + 48 * 5))
		wpas_p2p_add_chan(&chan->reg_class[cla], 48);
	if (chan->reg_class[cla].channels)
		cla++;

	wpa_printf(MSG_DEBUG, "P2P: Enable operating classes for higher 5 GHz "
		   "band");

	/* Operating class 124 - 5 GHz, channels 149,153,157,161 */
	chan->reg_class[cla].reg_class = 124;
	chan->reg_class[cla].channels = 0;
	if (!wpas_p2p_disallowed_freq(wpa_s->global, 5000 + 149 * 5))
		wpas_p2p_add_chan(&chan->reg_class[cla], 149);
	if (!wpas_p2p_disallowed_freq(wpa_s->global, 5000 + 153 * 5))
		wpas_p2p_add_chan(&chan->reg_class[cla], 153);
	if (!wpas_p2p_disallowed_freq(wpa_s->global, 5000 + 156 * 5))
		wpas_p2p_add_chan(&chan->reg_class[cla], 157);
	if (!wpas_p2p_disallowed_freq(wpa_s->global, 5000 + 161 * 5))
		wpas_p2p_add_chan(&chan->reg_class[cla], 161);
	if (chan->reg_class[cla].channels)
		cla++;

	chan->reg_classes = cla;
	return 0;
}


static struct hostapd_hw_modes * get_mode(struct hostapd_hw_modes *modes,
					  u16 num_modes,
					  enum hostapd_hw_mode mode)
{
	u16 i;

	for (i = 0; i < num_modes; i++) {
		if (modes[i].mode == mode)
			return &modes[i];
	}

	return NULL;
}


static int has_channel(struct wpa_global *global,
		       struct hostapd_hw_modes *mode, u8 chan, int *flags)
{
	int i;
	unsigned int freq;

	freq = (mode->mode == HOSTAPD_MODE_IEEE80211A ? 5000 : 2407) +
		chan * 5;
	if (wpas_p2p_disallowed_freq(global, freq))
		return 0;

	for (i = 0; i < mode->num_channels; i++) {
		if (mode->channels[i].chan == chan) {
			if (flags)
				*flags = mode->channels[i].flag;
			return !(mode->channels[i].flag &
				 (HOSTAPD_CHAN_DISABLED |
				  HOSTAPD_CHAN_PASSIVE_SCAN |
				  HOSTAPD_CHAN_NO_IBSS |
				  HOSTAPD_CHAN_RADAR));
		}
	}

	return 0;
}


struct p2p_oper_class_map {
	enum hostapd_hw_mode mode;
	u8 op_class;
	u8 min_chan;
	u8 max_chan;
	u8 inc;
	enum { BW20, BW40PLUS, BW40MINUS } bw;
};

static struct p2p_oper_class_map op_class[] = {
	{ HOSTAPD_MODE_IEEE80211G, 81, 1, 13, 1, BW20 },
#if 0 /* Do not enable HT40 on 2 GHz for now */
	{ HOSTAPD_MODE_IEEE80211G, 83, 1, 9, 1, BW40PLUS },
	{ HOSTAPD_MODE_IEEE80211G, 84, 5, 13, 1, BW40MINUS },
#endif
	{ HOSTAPD_MODE_IEEE80211A, 115, 36, 48, 4, BW20 },
	{ HOSTAPD_MODE_IEEE80211A, 124, 149, 161, 4, BW20 },
	{ HOSTAPD_MODE_IEEE80211A, 116, 36, 44, 8, BW40PLUS },
	{ HOSTAPD_MODE_IEEE80211A, 117, 40, 48, 8, BW40MINUS },
	{ HOSTAPD_MODE_IEEE80211A, 126, 149, 157, 8, BW40PLUS },
	{ HOSTAPD_MODE_IEEE80211A, 127, 153, 161, 8, BW40MINUS },
	{ -1, 0, 0, 0, 0, BW20 }
};


static int wpas_p2p_verify_channel(struct wpa_supplicant *wpa_s,
				   struct hostapd_hw_modes *mode,
				   u8 channel, u8 bw)
{
	int flag;

	if (!has_channel(wpa_s->global, mode, channel, &flag))
		return -1;
	if (bw == BW40MINUS &&
	    (!(flag & HOSTAPD_CHAN_HT40MINUS) ||
	     !has_channel(wpa_s->global, mode, channel - 4, NULL)))
		return 0;
	if (bw == BW40PLUS &&
	    (!(flag & HOSTAPD_CHAN_HT40PLUS) ||
	     !has_channel(wpa_s->global, mode, channel + 4, NULL)))
		return 0;
	return 1;
}


static int wpas_p2p_setup_channels(struct wpa_supplicant *wpa_s,
				   struct p2p_channels *chan)
{
	struct hostapd_hw_modes *mode;
	int cla, op;

	if (wpa_s->hw.modes == NULL) {
		wpa_printf(MSG_DEBUG, "P2P: Driver did not support fetching "
			   "of all supported channels; assume dualband "
			   "support");
		return wpas_p2p_default_channels(wpa_s, chan);
	}

	cla = 0;

	for (op = 0; op_class[op].op_class; op++) {
		struct p2p_oper_class_map *o = &op_class[op];
		u8 ch;
		struct p2p_reg_class *reg = NULL;

		mode = get_mode(wpa_s->hw.modes, wpa_s->hw.num_modes, o->mode);
		if (mode == NULL)
			continue;
		for (ch = o->min_chan; ch <= o->max_chan; ch += o->inc) {
			if (wpas_p2p_verify_channel(wpa_s, mode, ch, o->bw) < 1)
				continue;
			if (reg == NULL) {
				wpa_printf(MSG_DEBUG, "P2P: Add operating "
					   "class %u", o->op_class);
				reg = &chan->reg_class[cla];
				cla++;
				reg->reg_class = o->op_class;
			}
			reg->channel[reg->channels] = ch;
			reg->channels++;
		}
		if (reg) {
			wpa_hexdump(MSG_DEBUG, "P2P: Channels",
				    reg->channel, reg->channels);
		}
	}

	chan->reg_classes = cla;

	return 0;
}


int wpas_p2p_get_ht40_mode(struct wpa_supplicant *wpa_s,
			   struct hostapd_hw_modes *mode, u8 channel)
{
	int op, ret;

	for (op = 0; op_class[op].op_class; op++) {
		struct p2p_oper_class_map *o = &op_class[op];
		u8 ch;

		for (ch = o->min_chan; ch <= o->max_chan; ch += o->inc) {
			if (o->mode != HOSTAPD_MODE_IEEE80211A ||
			    o->bw == BW20 || ch != channel)
				continue;
			ret = wpas_p2p_verify_channel(wpa_s, mode, ch, o->bw);
			if (ret < 0)
				continue;
			else if (ret > 0)
				return (o->bw == BW40MINUS) ? -1 : 1;
			else
				return 0;
		}
	}
	return 0;
}


static int wpas_get_noa(void *ctx, const u8 *interface_addr, u8 *buf,
			size_t buf_len)
{
	struct wpa_supplicant *wpa_s = ctx;

	for (wpa_s = wpa_s->global->ifaces; wpa_s; wpa_s = wpa_s->next) {
		if (os_memcmp(wpa_s->own_addr, interface_addr, ETH_ALEN) == 0)
			break;
	}
	if (wpa_s == NULL)
		return -1;

	return wpa_drv_get_noa(wpa_s, buf, buf_len);
}


static int wpas_go_connected(void *ctx, const u8 *dev_addr)
{
	struct wpa_supplicant *wpa_s = ctx;

	for (wpa_s = wpa_s->global->ifaces; wpa_s; wpa_s = wpa_s->next) {
		struct wpa_ssid *ssid = wpa_s->current_ssid;
		if (ssid == NULL)
			continue;
		if (ssid->mode != WPAS_MODE_INFRA)
			continue;
		if (wpa_s->wpa_state != WPA_COMPLETED &&
		    wpa_s->wpa_state != WPA_GROUP_HANDSHAKE)
			continue;
		if (os_memcmp(wpa_s->go_dev_addr, dev_addr, ETH_ALEN) == 0)
			return 1;
	}

	return 0;
}


/**
 * wpas_p2p_init - Initialize P2P module for %wpa_supplicant
 * @global: Pointer to global data from wpa_supplicant_init()
 * @wpa_s: Pointer to wpa_supplicant data from wpa_supplicant_add_iface()
 * Returns: 0 on success, -1 on failure
 */
int wpas_p2p_init(struct wpa_global *global, struct wpa_supplicant *wpa_s)
{
	struct p2p_config p2p;
	unsigned int r;
	int i;

	if (!(wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_CAPABLE))
		return 0;

	if (global->p2p)
		return 0;

	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT) {
		struct p2p_params params;

		wpa_printf(MSG_DEBUG, "P2P: Use driver-based P2P management");
		os_memset(&params, 0, sizeof(params));
		params.dev_name = wpa_s->conf->device_name;
		os_memcpy(params.pri_dev_type, wpa_s->conf->device_type,
			  WPS_DEV_TYPE_LEN);
		params.num_sec_dev_types = wpa_s->conf->num_sec_device_types;
		os_memcpy(params.sec_dev_type,
			  wpa_s->conf->sec_device_type,
			  params.num_sec_dev_types * WPS_DEV_TYPE_LEN);

		if (wpa_drv_p2p_set_params(wpa_s, &params) < 0)
			return -1;

		return 0;
	}

	os_memset(&p2p, 0, sizeof(p2p));
	p2p.msg_ctx = wpa_s;
	p2p.cb_ctx = wpa_s;
	p2p.p2p_scan = wpas_p2p_scan;
	p2p.send_action = wpas_send_action;
	p2p.send_action_done = wpas_send_action_done;
	p2p.go_neg_completed = wpas_go_neg_completed;
	p2p.go_neg_req_rx = wpas_go_neg_req_rx;
	p2p.dev_found = wpas_dev_found;
	p2p.dev_lost = wpas_dev_lost;
	p2p.start_listen = wpas_start_listen;
	p2p.stop_listen = wpas_stop_listen;
	p2p.send_probe_resp = wpas_send_probe_resp;
	p2p.sd_request = wpas_sd_request;
	p2p.sd_response = wpas_sd_response;
	p2p.prov_disc_req = wpas_prov_disc_req;
	p2p.prov_disc_resp = wpas_prov_disc_resp;
	p2p.prov_disc_fail = wpas_prov_disc_fail;
	p2p.invitation_process = wpas_invitation_process;
	p2p.invitation_received = wpas_invitation_received;
	p2p.invitation_result = wpas_invitation_result;
	p2p.get_noa = wpas_get_noa;
	p2p.go_connected = wpas_go_connected;

	os_memcpy(wpa_s->global->p2p_dev_addr, wpa_s->own_addr, ETH_ALEN);
	os_memcpy(p2p.dev_addr, wpa_s->global->p2p_dev_addr, ETH_ALEN);
	p2p.dev_name = wpa_s->conf->device_name;
	p2p.manufacturer = wpa_s->conf->manufacturer;
	p2p.model_name = wpa_s->conf->model_name;
	p2p.model_number = wpa_s->conf->model_number;
	p2p.serial_number = wpa_s->conf->serial_number;
	if (wpa_s->wps) {
		os_memcpy(p2p.uuid, wpa_s->wps->uuid, 16);
		p2p.config_methods = wpa_s->wps->config_methods;
	}

	if (wpa_s->conf->p2p_listen_reg_class &&
	    wpa_s->conf->p2p_listen_channel) {
		p2p.reg_class = wpa_s->conf->p2p_listen_reg_class;
		p2p.channel = wpa_s->conf->p2p_listen_channel;
	} else {
		p2p.reg_class = 81;
		/*
		 * Pick one of the social channels randomly as the listen
		 * channel.
		 */
		os_get_random((u8 *) &r, sizeof(r));
		p2p.channel = 1 + (r % 3) * 5;
	}
	wpa_printf(MSG_DEBUG, "P2P: Own listen channel: %d", p2p.channel);

	if (wpa_s->conf->p2p_oper_reg_class &&
	    wpa_s->conf->p2p_oper_channel) {
		p2p.op_reg_class = wpa_s->conf->p2p_oper_reg_class;
		p2p.op_channel = wpa_s->conf->p2p_oper_channel;
		p2p.cfg_op_channel = 1;
		wpa_printf(MSG_DEBUG, "P2P: Configured operating channel: "
			   "%d:%d", p2p.op_reg_class, p2p.op_channel);

	} else {
		p2p.op_reg_class = 81;
		/*
		 * Use random operation channel from (1, 6, 11) if no other
		 * preference is indicated.
		 */
		os_get_random((u8 *) &r, sizeof(r));
		p2p.op_channel = 1 + (r % 3) * 5;
		p2p.cfg_op_channel = 0;
		wpa_printf(MSG_DEBUG, "P2P: Random operating channel: "
			   "%d:%d", p2p.op_reg_class, p2p.op_channel);
	}
	if (wpa_s->conf->country[0] && wpa_s->conf->country[1]) {
		os_memcpy(p2p.country, wpa_s->conf->country, 2);
		p2p.country[2] = 0x04;
	} else
		os_memcpy(p2p.country, "XX\x04", 3);

	if (wpas_p2p_setup_channels(wpa_s, &p2p.channels)) {
		wpa_printf(MSG_ERROR, "P2P: Failed to configure supported "
			   "channel list");
		return -1;
	}

	os_memcpy(p2p.pri_dev_type, wpa_s->conf->device_type,
		  WPS_DEV_TYPE_LEN);

	p2p.num_sec_dev_types = wpa_s->conf->num_sec_device_types;
	os_memcpy(p2p.sec_dev_type, wpa_s->conf->sec_device_type,
		  p2p.num_sec_dev_types * WPS_DEV_TYPE_LEN);

	p2p.concurrent_operations = !!(wpa_s->drv_flags &
				       WPA_DRIVER_FLAGS_P2P_CONCURRENT);

	p2p.max_peers = 100;

	if (wpa_s->conf->p2p_ssid_postfix) {
		p2p.ssid_postfix_len =
			os_strlen(wpa_s->conf->p2p_ssid_postfix);
		if (p2p.ssid_postfix_len > sizeof(p2p.ssid_postfix))
			p2p.ssid_postfix_len = sizeof(p2p.ssid_postfix);
		os_memcpy(p2p.ssid_postfix, wpa_s->conf->p2p_ssid_postfix,
			  p2p.ssid_postfix_len);
	}

	p2p.p2p_intra_bss = wpa_s->conf->p2p_intra_bss;

	p2p.max_listen = wpa_s->max_remain_on_chan;

	global->p2p = p2p_init(&p2p);
	if (global->p2p == NULL)
		return -1;
	global->p2p_init_wpa_s = wpa_s;

	for (i = 0; i < MAX_WPS_VENDOR_EXT; i++) {
		if (wpa_s->conf->wps_vendor_ext[i] == NULL)
			continue;
		p2p_add_wps_vendor_extension(
			global->p2p, wpa_s->conf->wps_vendor_ext[i]);
	}

	return 0;
}


/**
 * wpas_p2p_deinit - Deinitialize per-interface P2P data
 * @wpa_s: Pointer to wpa_supplicant data from wpa_supplicant_add_iface()
 *
 * This function deinitialize per-interface P2P data.
 */
void wpas_p2p_deinit(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->driver && wpa_s->drv_priv)
		wpa_drv_probe_req_report(wpa_s, 0);

	if (wpa_s->go_params) {
		/* Clear any stored provisioning info */
		p2p_clear_provisioning_info(
			wpa_s->global->p2p,
			wpa_s->go_params->peer_device_addr);
	}

	os_free(wpa_s->go_params);
	wpa_s->go_params = NULL;
	eloop_cancel_timeout(wpas_p2p_group_formation_timeout, wpa_s, NULL);
	eloop_cancel_timeout(wpas_p2p_join_scan, wpa_s, NULL);
	wpa_s->p2p_long_listen = 0;
	eloop_cancel_timeout(wpas_p2p_long_listen_timeout, wpa_s, NULL);
	eloop_cancel_timeout(wpas_p2p_group_idle_timeout, wpa_s, NULL);
	wpas_p2p_remove_pending_group_interface(wpa_s);

	/* TODO: remove group interface from the driver if this wpa_s instance
	 * is on top of a P2P group interface */
}


/**
 * wpas_p2p_deinit_global - Deinitialize global P2P module
 * @global: Pointer to global data from wpa_supplicant_init()
 *
 * This function deinitializes the global (per device) P2P module.
 */
void wpas_p2p_deinit_global(struct wpa_global *global)
{
	struct wpa_supplicant *wpa_s, *tmp;

	wpa_s = global->ifaces;
	if (wpa_s)
		wpas_p2p_service_flush(wpa_s);

	if (global->p2p == NULL)
		return;

	/* Remove remaining P2P group interfaces */
	while (wpa_s && wpa_s->p2p_group_interface != NOT_P2P_GROUP_INTERFACE)
		wpa_s = wpa_s->next;
	while (wpa_s) {
		tmp = global->ifaces;
		while (tmp &&
		       (tmp == wpa_s ||
			tmp->p2p_group_interface == NOT_P2P_GROUP_INTERFACE)) {
			tmp = tmp->next;
		}
		if (tmp == NULL)
			break;
		/* Disconnect from the P2P group and deinit the interface */
		wpas_p2p_disconnect(tmp);
	}

	/*
	 * Deinit GO data on any possibly remaining interface (if main
	 * interface is used as GO).
	 */
	for (wpa_s = global->ifaces; wpa_s; wpa_s = wpa_s->next) {
		if (wpa_s->ap_iface)
			wpas_p2p_group_deinit(wpa_s);
	}

	p2p_deinit(global->p2p);
	global->p2p = NULL;
	global->p2p_init_wpa_s = NULL;
}


static int wpas_p2p_create_iface(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->conf->p2p_no_group_iface)
		return 0; /* separate interface disabled per configuration */
	if (wpa_s->drv_flags &
	    (WPA_DRIVER_FLAGS_P2P_DEDICATED_INTERFACE |
	     WPA_DRIVER_FLAGS_P2P_MGMT_AND_NON_P2P))
		return 1; /* P2P group requires a new interface in every case
			   */
	if (!(wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_CONCURRENT))
		return 0; /* driver does not support concurrent operations */
	if (wpa_s->global->ifaces->next)
		return 1; /* more that one interface already in use */
	if (wpa_s->wpa_state >= WPA_AUTHENTICATING)
		return 1; /* this interface is already in use */
	return 0;
}


static int wpas_p2p_start_go_neg(struct wpa_supplicant *wpa_s,
				 const u8 *peer_addr,
				 enum p2p_wps_method wps_method,
				 int go_intent, const u8 *own_interface_addr,
				 unsigned int force_freq, int persistent_group,
				 struct wpa_ssid *ssid, unsigned int pref_freq)
{
	if (persistent_group && wpa_s->conf->persistent_reconnect)
		persistent_group = 2;

	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT) {
		return wpa_drv_p2p_connect(wpa_s, peer_addr, wps_method,
					   go_intent, own_interface_addr,
					   force_freq, persistent_group);
	}

	/*
	 * Increase GO config timeout if HT40 is used since it takes some time
	 * to scan channels for coex purposes before the BSS can be started.
	 */
	p2p_set_config_timeout(wpa_s->global->p2p,
			       wpa_s->p2p_go_ht40 ? 255 : 100, 20);

	return p2p_connect(wpa_s->global->p2p, peer_addr, wps_method,
			   go_intent, own_interface_addr, force_freq,
			   persistent_group, ssid ? ssid->ssid : NULL,
			   ssid ? ssid->ssid_len : 0,
			   wpa_s->p2p_pd_before_go_neg, pref_freq);
}


static int wpas_p2p_auth_go_neg(struct wpa_supplicant *wpa_s,
				const u8 *peer_addr,
				enum p2p_wps_method wps_method,
				int go_intent, const u8 *own_interface_addr,
				unsigned int force_freq, int persistent_group,
				struct wpa_ssid *ssid, unsigned int pref_freq)
{
	if (persistent_group && wpa_s->conf->persistent_reconnect)
		persistent_group = 2;

	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT)
		return -1;

	return p2p_authorize(wpa_s->global->p2p, peer_addr, wps_method,
			     go_intent, own_interface_addr, force_freq,
			     persistent_group, ssid ? ssid->ssid : NULL,
			     ssid ? ssid->ssid_len : 0, pref_freq);
}


static void wpas_p2p_check_join_scan_limit(struct wpa_supplicant *wpa_s)
{
	wpa_s->p2p_join_scan_count++;
	wpa_printf(MSG_DEBUG, "P2P: Join scan attempt %d",
		   wpa_s->p2p_join_scan_count);
	if (wpa_s->p2p_join_scan_count > P2P_MAX_JOIN_SCAN_ATTEMPTS) {
		wpa_printf(MSG_DEBUG, "P2P: Failed to find GO " MACSTR
			   " for join operationg - stop join attempt",
			   MAC2STR(wpa_s->pending_join_iface_addr));
		eloop_cancel_timeout(wpas_p2p_join_scan, wpa_s, NULL);
		if (wpa_s->p2p_auto_pd) {
			wpa_s->p2p_auto_pd = 0;
			wpa_msg(wpa_s, MSG_INFO, P2P_EVENT_PROV_DISC_FAILURE
				" p2p_dev_addr=" MACSTR " status=N/A",
				MAC2STR(wpa_s->pending_join_dev_addr));
			return;
		}
		wpa_msg(wpa_s->parent, MSG_INFO,
			P2P_EVENT_GROUP_FORMATION_FAILURE);
	}
}


static int wpas_check_freq_conflict(struct wpa_supplicant *wpa_s, int freq)
{
	struct wpa_supplicant *iface;
	int shared_freq;
	u8 bssid[ETH_ALEN];

	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_MULTI_CHANNEL_CONCURRENT)
		return 0;

	for (iface = wpa_s->global->ifaces; iface; iface = iface->next) {
		if (!wpas_p2p_create_iface(wpa_s) && iface == wpa_s)
			continue;
		if (iface->current_ssid == NULL || iface->assoc_freq == 0)
			continue;
		if (iface->current_ssid->mode == WPAS_MODE_AP ||
		    iface->current_ssid->mode == WPAS_MODE_P2P_GO)
			shared_freq = iface->current_ssid->frequency;
		else if (wpa_drv_get_bssid(iface, bssid) == 0)
			shared_freq = iface->assoc_freq;
		else
			shared_freq = 0;

		if (shared_freq && freq != shared_freq) {
			wpa_printf(MSG_DEBUG, "P2P: Frequency conflict - %s "
				   "connected on %d MHz - new connection on "
				   "%d MHz", iface->ifname, shared_freq, freq);
			return 1;
		}
	}

	shared_freq = wpa_drv_shared_freq(wpa_s);
	if (shared_freq > 0 && shared_freq != freq) {
		wpa_printf(MSG_DEBUG, "P2P: Frequency conflict - shared "
			   "virtual interface connected on %d MHz - new "
			   "connection on %d MHz", shared_freq, freq);
		return 1;
	}

	return 0;
}


static int wpas_p2p_peer_go(struct wpa_supplicant *wpa_s,
			    const u8 *peer_dev_addr)
{
	struct wpa_bss *bss;
	int updated;

	bss = wpa_bss_get_p2p_dev_addr(wpa_s, peer_dev_addr);
	if (bss == NULL)
		return -1;
	if (bss->last_update_idx < wpa_s->bss_update_idx) {
		wpa_printf(MSG_DEBUG, "P2P: Peer BSS entry not updated in the "
			   "last scan");
		return 0;
	}

	updated = os_time_before(&wpa_s->p2p_auto_started, &bss->last_update);
	wpa_printf(MSG_DEBUG, "P2P: Current BSS entry for peer updated at "
		   "%ld.%06ld (%supdated in last scan)",
		   bss->last_update.sec, bss->last_update.usec,
		   updated ? "": "not ");

	return updated;
}


static void wpas_p2p_scan_res_join(struct wpa_supplicant *wpa_s,
				   struct wpa_scan_results *scan_res)
{
	struct wpa_bss *bss;
	int freq;
	u8 iface_addr[ETH_ALEN];

	eloop_cancel_timeout(wpas_p2p_join_scan, wpa_s, NULL);

	if (wpa_s->global->p2p_disabled)
		return;

	wpa_printf(MSG_DEBUG, "P2P: Scan results received (%d BSS) for %sjoin",
		   scan_res ? (int) scan_res->num : -1,
		   wpa_s->p2p_auto_join ? "auto_" : "");

	if (scan_res)
		wpas_p2p_scan_res_handler(wpa_s, scan_res);

	if (wpa_s->p2p_auto_pd) {
		int join = wpas_p2p_peer_go(wpa_s,
					    wpa_s->pending_join_dev_addr);
		if (join == 0 &&
		    wpa_s->auto_pd_scan_retry < P2P_AUTO_PD_SCAN_ATTEMPTS) {
			wpa_s->auto_pd_scan_retry++;
			bss = wpa_bss_get_bssid(wpa_s,
						wpa_s->pending_join_dev_addr);
			if (bss) {
				freq = bss->freq;
				wpa_printf(MSG_DEBUG, "P2P: Scan retry %d for "
					   "the peer " MACSTR " at %d MHz",
					   wpa_s->auto_pd_scan_retry,
					   MAC2STR(wpa_s->
						   pending_join_dev_addr),
					   freq);
				wpas_p2p_join_scan_req(wpa_s, freq);
				return;
			}
		}

		if (join < 0)
			join = 0;

		wpa_s->p2p_auto_pd = 0;
		wpa_s->pending_pd_use = join ? AUTO_PD_JOIN : AUTO_PD_GO_NEG;
		wpa_printf(MSG_DEBUG, "P2P: Auto PD with " MACSTR " join=%d",
			   MAC2STR(wpa_s->pending_join_dev_addr), join);
		if (p2p_prov_disc_req(wpa_s->global->p2p,
				      wpa_s->pending_join_dev_addr,
				      wpa_s->pending_pd_config_methods, join,
				      0, wpa_s->user_initiated_pd) < 0) {
			wpa_s->p2p_auto_pd = 0;
			wpa_msg(wpa_s, MSG_INFO, P2P_EVENT_PROV_DISC_FAILURE
				" p2p_dev_addr=" MACSTR " status=N/A",
				MAC2STR(wpa_s->pending_join_dev_addr));
		}
		return;
	}

	if (wpa_s->p2p_auto_join) {
		int join = wpas_p2p_peer_go(wpa_s,
					    wpa_s->pending_join_dev_addr);
		if (join < 0) {
			wpa_printf(MSG_DEBUG, "P2P: Peer was not found to be "
				   "running a GO -> use GO Negotiation");
			wpas_p2p_connect(wpa_s, wpa_s->pending_join_dev_addr,
					 wpa_s->p2p_pin, wpa_s->p2p_wps_method,
					 wpa_s->p2p_persistent_group, 0, 0, 0,
					 wpa_s->p2p_go_intent,
					 wpa_s->p2p_connect_freq,
					 wpa_s->p2p_persistent_id,
					 wpa_s->p2p_pd_before_go_neg,
					 wpa_s->p2p_go_ht40);
			return;
		}

		wpa_printf(MSG_DEBUG, "P2P: Peer was found running GO%s -> "
			   "try to join the group", join ? "" :
			   " in older scan");
		if (!join)
			wpa_s->p2p_fallback_to_go_neg = 1;
	}

	freq = p2p_get_oper_freq(wpa_s->global->p2p,
				 wpa_s->pending_join_iface_addr);
	if (freq < 0 &&
	    p2p_get_interface_addr(wpa_s->global->p2p,
				   wpa_s->pending_join_dev_addr,
				   iface_addr) == 0 &&
	    os_memcmp(iface_addr, wpa_s->pending_join_dev_addr, ETH_ALEN) != 0)
	{
		wpa_printf(MSG_DEBUG, "P2P: Overwrite pending interface "
			   "address for join from " MACSTR " to " MACSTR
			   " based on newly discovered P2P peer entry",
			   MAC2STR(wpa_s->pending_join_iface_addr),
			   MAC2STR(iface_addr));
		os_memcpy(wpa_s->pending_join_iface_addr, iface_addr,
			  ETH_ALEN);

		freq = p2p_get_oper_freq(wpa_s->global->p2p,
					 wpa_s->pending_join_iface_addr);
	}
	if (freq >= 0) {
		wpa_printf(MSG_DEBUG, "P2P: Target GO operating frequency "
			   "from P2P peer table: %d MHz", freq);
	}
	bss = wpa_bss_get_bssid(wpa_s, wpa_s->pending_join_iface_addr);
	if (bss) {
		freq = bss->freq;
		wpa_printf(MSG_DEBUG, "P2P: Target GO operating frequency "
			   "from BSS table: %d MHz", freq);
	}
	if (freq > 0) {
		u16 method;

		if (wpas_check_freq_conflict(wpa_s, freq) > 0) {
			wpa_msg(wpa_s->parent, MSG_INFO,
				P2P_EVENT_GROUP_FORMATION_FAILURE
				"reason=FREQ_CONFLICT");
			return;
		}

		wpa_printf(MSG_DEBUG, "P2P: Send Provision Discovery Request "
			   "prior to joining an existing group (GO " MACSTR
			   " freq=%u MHz)",
			   MAC2STR(wpa_s->pending_join_dev_addr), freq);
		wpa_s->pending_pd_before_join = 1;

		switch (wpa_s->pending_join_wps_method) {
		case WPS_PIN_DISPLAY:
			method = WPS_CONFIG_KEYPAD;
			break;
		case WPS_PIN_KEYPAD:
			method = WPS_CONFIG_DISPLAY;
			break;
		case WPS_PBC:
			method = WPS_CONFIG_PUSHBUTTON;
			break;
		default:
			method = 0;
			break;
		}

		if ((p2p_get_provisioning_info(wpa_s->global->p2p,
					       wpa_s->pending_join_dev_addr) ==
		     method)) {
			/*
			 * We have already performed provision discovery for
			 * joining the group. Proceed directly to join
			 * operation without duplicated provision discovery. */
			wpa_printf(MSG_DEBUG, "P2P: Provision discovery "
				   "with " MACSTR " already done - proceed to "
				   "join",
				   MAC2STR(wpa_s->pending_join_dev_addr));
			wpa_s->pending_pd_before_join = 0;
			goto start;
		}

		if (p2p_prov_disc_req(wpa_s->global->p2p,
				      wpa_s->pending_join_dev_addr, method, 1,
				      freq, wpa_s->user_initiated_pd) < 0) {
			wpa_printf(MSG_DEBUG, "P2P: Failed to send Provision "
				   "Discovery Request before joining an "
				   "existing group");
			wpa_s->pending_pd_before_join = 0;
			goto start;
		}
		return;
	}

	wpa_printf(MSG_DEBUG, "P2P: Failed to find BSS/GO - try again later");
	eloop_cancel_timeout(wpas_p2p_join_scan, wpa_s, NULL);
	eloop_register_timeout(1, 0, wpas_p2p_join_scan, wpa_s, NULL);
	wpas_p2p_check_join_scan_limit(wpa_s);
	return;

start:
	/* Start join operation immediately */
	wpas_p2p_join_start(wpa_s);
}


static void wpas_p2p_join_scan_req(struct wpa_supplicant *wpa_s, int freq)
{
	int ret;
	struct wpa_driver_scan_params params;
	struct wpabuf *wps_ie, *ies;
	size_t ielen;
	int freqs[2] = { 0, 0 };

	os_memset(&params, 0, sizeof(params));

	/* P2P Wildcard SSID */
	params.num_ssids = 1;
	params.ssids[0].ssid = (u8 *) P2P_WILDCARD_SSID;
	params.ssids[0].ssid_len = P2P_WILDCARD_SSID_LEN;

	wpa_s->wps->dev.p2p = 1;
	wps_ie = wps_build_probe_req_ie(DEV_PW_DEFAULT, &wpa_s->wps->dev,
					wpa_s->wps->uuid, WPS_REQ_ENROLLEE, 0,
					NULL);
	if (wps_ie == NULL) {
		wpas_p2p_scan_res_join(wpa_s, NULL);
		return;
	}

	ielen = p2p_scan_ie_buf_len(wpa_s->global->p2p);
	ies = wpabuf_alloc(wpabuf_len(wps_ie) + ielen);
	if (ies == NULL) {
		wpabuf_free(wps_ie);
		wpas_p2p_scan_res_join(wpa_s, NULL);
		return;
	}
	wpabuf_put_buf(ies, wps_ie);
	wpabuf_free(wps_ie);

	p2p_scan_ie(wpa_s->global->p2p, ies, NULL);

	params.p2p_probe = 1;
	params.extra_ies = wpabuf_head(ies);
	params.extra_ies_len = wpabuf_len(ies);
	if (freq > 0) {
		freqs[0] = freq;
		params.freqs = freqs;
	}

	/*
	 * Run a scan to update BSS table and start Provision Discovery once
	 * the new scan results become available.
	 */
	ret = wpa_drv_scan(wpa_s, &params);
	if (!ret)
		wpa_s->scan_res_handler = wpas_p2p_scan_res_join;

	wpabuf_free(ies);

	if (ret) {
		wpa_printf(MSG_DEBUG, "P2P: Failed to start scan for join - "
			   "try again later");
		eloop_cancel_timeout(wpas_p2p_join_scan, wpa_s, NULL);
		eloop_register_timeout(1, 0, wpas_p2p_join_scan, wpa_s, NULL);
		wpas_p2p_check_join_scan_limit(wpa_s);
	}
}


static void wpas_p2p_join_scan(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	wpas_p2p_join_scan_req(wpa_s, 0);
}


static int wpas_p2p_join(struct wpa_supplicant *wpa_s, const u8 *iface_addr,
			 const u8 *dev_addr, enum p2p_wps_method wps_method,
			 int auto_join)
{
	wpa_printf(MSG_DEBUG, "P2P: Request to join existing group (iface "
		   MACSTR " dev " MACSTR ")%s",
		   MAC2STR(iface_addr), MAC2STR(dev_addr),
		   auto_join ? " (auto_join)" : "");

	wpa_s->p2p_auto_pd = 0;
	wpa_s->p2p_auto_join = !!auto_join;
	os_memcpy(wpa_s->pending_join_iface_addr, iface_addr, ETH_ALEN);
	os_memcpy(wpa_s->pending_join_dev_addr, dev_addr, ETH_ALEN);
	wpa_s->pending_join_wps_method = wps_method;

	/* Make sure we are not running find during connection establishment */
	wpas_p2p_stop_find(wpa_s);

	wpa_s->p2p_join_scan_count = 0;
	wpas_p2p_join_scan(wpa_s, NULL);
	return 0;
}


static int wpas_p2p_join_start(struct wpa_supplicant *wpa_s)
{
	struct wpa_supplicant *group;
	struct p2p_go_neg_results res;
	struct wpa_bss *bss;

	group = wpas_p2p_get_group_iface(wpa_s, 0, 0);
	if (group == NULL)
		return -1;
	if (group != wpa_s) {
		os_memcpy(group->p2p_pin, wpa_s->p2p_pin,
			  sizeof(group->p2p_pin));
		group->p2p_wps_method = wpa_s->p2p_wps_method;
	}

	group->p2p_in_provisioning = 1;
	wpa_s->global->p2p_group_formation = wpa_s;
	group->p2p_fallback_to_go_neg = wpa_s->p2p_fallback_to_go_neg;

	os_memset(&res, 0, sizeof(res));
	os_memcpy(res.peer_interface_addr, wpa_s->pending_join_iface_addr,
		  ETH_ALEN);
	res.wps_method = wpa_s->pending_join_wps_method;
	bss = wpa_bss_get_bssid(wpa_s, wpa_s->pending_join_iface_addr);
	if (bss) {
		res.freq = bss->freq;
		res.ssid_len = bss->ssid_len;
		os_memcpy(res.ssid, bss->ssid, bss->ssid_len);
	}

	if (wpa_s->off_channel_freq || wpa_s->roc_waiting_drv_freq) {
		wpa_printf(MSG_DEBUG, "P2P: Cancel remain-on-channel prior to "
			   "starting client");
		wpa_drv_cancel_remain_on_channel(wpa_s);
		wpa_s->off_channel_freq = 0;
		wpa_s->roc_waiting_drv_freq = 0;
	}
	wpas_start_wps_enrollee(group, &res);

	/*
	 * Allow a longer timeout for join-a-running-group than normal 15
	 * second group formation timeout since the GO may not have authorized
	 * our connection yet.
	 */
	eloop_cancel_timeout(wpas_p2p_group_formation_timeout, wpa_s, NULL);
	eloop_register_timeout(60, 0, wpas_p2p_group_formation_timeout,
			       wpa_s, NULL);

	return 0;
}


/**
 * wpas_p2p_connect - Request P2P Group Formation to be started
 * @wpa_s: Pointer to wpa_supplicant data from wpa_supplicant_add_iface()
 * @peer_addr: Address of the peer P2P Device
 * @pin: PIN to use during provisioning or %NULL to indicate PBC mode
 * @persistent_group: Whether to create a persistent group
 * @auto_join: Whether to select join vs. GO Negotiation automatically
 * @join: Whether to join an existing group (as a client) instead of starting
 *	Group Owner negotiation; @peer_addr is BSSID in that case
 * @auth: Whether to only authorize the connection instead of doing that and
 *	initiating Group Owner negotiation
 * @go_intent: GO Intent or -1 to use default
 * @freq: Frequency for the group or 0 for auto-selection
 * @persistent_id: Persistent group credentials to use for forcing GO
 *	parameters or -1 to generate new values (SSID/passphrase)
 * @pd: Whether to send Provision Discovery prior to GO Negotiation as an
 *	interoperability workaround when initiating group formation
 * @ht40: Start GO with 40 MHz channel width
 * Returns: 0 or new PIN (if pin was %NULL) on success, -1 on unspecified
 *	failure, -2 on failure due to channel not currently available,
 *	-3 if forced channel is not supported
 */
int wpas_p2p_connect(struct wpa_supplicant *wpa_s, const u8 *peer_addr,
		     const char *pin, enum p2p_wps_method wps_method,
		     int persistent_group, int auto_join, int join, int auth,
		     int go_intent, int freq, int persistent_id, int pd,
		     int ht40)
{
	int force_freq = 0, pref_freq = 0, oper_freq = 0;
	u8 bssid[ETH_ALEN];
	int ret = 0;
	enum wpa_driver_if_type iftype;
	const u8 *if_addr;
	struct wpa_ssid *ssid = NULL;

	if (wpa_s->global->p2p_disabled || wpa_s->global->p2p == NULL)
		return -1;

	if (persistent_id >= 0) {
		ssid = wpa_config_get_network(wpa_s->conf, persistent_id);
		if (ssid == NULL || ssid->disabled != 2 ||
		    ssid->mode != WPAS_MODE_P2P_GO)
			return -1;
	}

	if (go_intent < 0)
		go_intent = wpa_s->conf->p2p_go_intent;

	if (!auth)
		wpa_s->p2p_long_listen = 0;

	wpa_s->p2p_wps_method = wps_method;
	wpa_s->p2p_persistent_group = !!persistent_group;
	wpa_s->p2p_persistent_id = persistent_id;
	wpa_s->p2p_go_intent = go_intent;
	wpa_s->p2p_connect_freq = freq;
	wpa_s->p2p_fallback_to_go_neg = 0;
	wpa_s->p2p_pd_before_go_neg = !!pd;
	wpa_s->p2p_go_ht40 = !!ht40;

	if (pin)
		os_strlcpy(wpa_s->p2p_pin, pin, sizeof(wpa_s->p2p_pin));
	else if (wps_method == WPS_PIN_DISPLAY) {
		ret = wps_generate_pin();
		os_snprintf(wpa_s->p2p_pin, sizeof(wpa_s->p2p_pin), "%08d",
			    ret);
		wpa_printf(MSG_DEBUG, "P2P: Randomly generated PIN: %s",
			   wpa_s->p2p_pin);
	} else
		wpa_s->p2p_pin[0] = '\0';

	if (join || auto_join) {
		u8 iface_addr[ETH_ALEN], dev_addr[ETH_ALEN];
		if (auth) {
			wpa_printf(MSG_DEBUG, "P2P: Authorize invitation to "
				   "connect a running group from " MACSTR,
				   MAC2STR(peer_addr));
			os_memcpy(wpa_s->p2p_auth_invite, peer_addr, ETH_ALEN);
			return ret;
		}
		os_memcpy(dev_addr, peer_addr, ETH_ALEN);
		if (p2p_get_interface_addr(wpa_s->global->p2p, peer_addr,
					   iface_addr) < 0) {
			os_memcpy(iface_addr, peer_addr, ETH_ALEN);
			p2p_get_dev_addr(wpa_s->global->p2p, peer_addr,
					 dev_addr);
		}
		if (auto_join) {
			os_get_time(&wpa_s->p2p_auto_started);
			wpa_printf(MSG_DEBUG, "P2P: Auto join started at "
				   "%ld.%06ld",
				   wpa_s->p2p_auto_started.sec,
				   wpa_s->p2p_auto_started.usec);
		}
		wpa_s->user_initiated_pd = 1;
		if (wpas_p2p_join(wpa_s, iface_addr, dev_addr, wps_method,
				  auto_join) < 0)
			return -1;
		return ret;
	}

	if (wpa_s->current_ssid && wpa_drv_get_bssid(wpa_s, bssid) == 0 &&
	    wpa_s->assoc_freq)
		oper_freq = wpa_s->assoc_freq;
	else {
		oper_freq = wpa_drv_shared_freq(wpa_s);
		if (oper_freq < 0)
			oper_freq = 0;
	}

	if (freq > 0) {
		if (!p2p_supported_freq(wpa_s->global->p2p, freq)) {
			wpa_printf(MSG_DEBUG, "P2P: The forced channel "
				   "(%u MHz) is not supported for P2P uses",
				   freq);
			return -3;
		}

		if (oper_freq > 0 && freq != oper_freq &&
		    !(wpa_s->drv_flags &
		      WPA_DRIVER_FLAGS_MULTI_CHANNEL_CONCURRENT)) {
			wpa_printf(MSG_DEBUG, "P2P: Cannot start P2P group "
				   "on %u MHz while connected on another "
				   "channel (%u MHz)", freq, oper_freq);
			return -2;
		}
		wpa_printf(MSG_DEBUG, "P2P: Trying to force us to use the "
			   "requested channel (%u MHz)", freq);
		force_freq = freq;
	} else if (oper_freq > 0 &&
		   !p2p_supported_freq(wpa_s->global->p2p, oper_freq)) {
		if (!(wpa_s->drv_flags &
		      WPA_DRIVER_FLAGS_MULTI_CHANNEL_CONCURRENT)) {
			wpa_printf(MSG_DEBUG, "P2P: Cannot start P2P group "
				   "while connected on non-P2P supported "
				   "channel (%u MHz)", oper_freq);
			return -2;
		}
		wpa_printf(MSG_DEBUG, "P2P: Current operating channel "
			   "(%u MHz) not available for P2P - try to use "
			   "another channel", oper_freq);
		force_freq = 0;
	} else if (oper_freq > 0 &&
		   (wpa_s->drv_flags &
		    WPA_DRIVER_FLAGS_MULTI_CHANNEL_CONCURRENT)) {
		wpa_printf(MSG_DEBUG, "P2P: Trying to prefer the channel we "
			   "are already using (%u MHz) on another interface",
			   oper_freq);
		pref_freq = oper_freq;
	} else if (oper_freq > 0) {
		wpa_printf(MSG_DEBUG, "P2P: Trying to force us to use the "
			   "channel we are already using (%u MHz) on another "
			   "interface", oper_freq);
		force_freq = oper_freq;
	}

	wpa_s->create_p2p_iface = wpas_p2p_create_iface(wpa_s);

	if (wpa_s->create_p2p_iface) {
		/* Prepare to add a new interface for the group */
		iftype = WPA_IF_P2P_GROUP;
		if (go_intent == 15)
			iftype = WPA_IF_P2P_GO;
		if (wpas_p2p_add_group_interface(wpa_s, iftype) < 0) {
			wpa_printf(MSG_ERROR, "P2P: Failed to allocate a new "
				   "interface for the group");
			return -1;
		}

		if_addr = wpa_s->pending_interface_addr;
	} else
		if_addr = wpa_s->own_addr;

	if (auth) {
		if (wpas_p2p_auth_go_neg(wpa_s, peer_addr, wps_method,
					 go_intent, if_addr,
					 force_freq, persistent_group, ssid,
					 pref_freq) < 0)
			return -1;
		return ret;
	}

	if (wpas_p2p_start_go_neg(wpa_s, peer_addr, wps_method,
				  go_intent, if_addr, force_freq,
				  persistent_group, ssid, pref_freq) < 0) {
		if (wpa_s->create_p2p_iface)
			wpas_p2p_remove_pending_group_interface(wpa_s);
		return -1;
	}
	return ret;
}


/**
 * wpas_p2p_remain_on_channel_cb - Indication of remain-on-channel start
 * @wpa_s: Pointer to wpa_supplicant data from wpa_supplicant_add_iface()
 * @freq: Frequency of the channel in MHz
 * @duration: Duration of the stay on the channel in milliseconds
 *
 * This callback is called when the driver indicates that it has started the
 * requested remain-on-channel duration.
 */
void wpas_p2p_remain_on_channel_cb(struct wpa_supplicant *wpa_s,
				   unsigned int freq, unsigned int duration)
{
	if (wpa_s->global->p2p_disabled || wpa_s->global->p2p == NULL)
		return;
	if (wpa_s->off_channel_freq == wpa_s->pending_listen_freq) {
		p2p_listen_cb(wpa_s->global->p2p, wpa_s->pending_listen_freq,
			      wpa_s->pending_listen_duration);
		wpa_s->pending_listen_freq = 0;
	}
}


static int wpas_p2p_listen_start(struct wpa_supplicant *wpa_s,
				 unsigned int timeout)
{
	/* Limit maximum Listen state time based on driver limitation. */
	if (timeout > wpa_s->max_remain_on_chan)
		timeout = wpa_s->max_remain_on_chan;

	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT)
		return wpa_drv_p2p_listen(wpa_s, timeout);

	return p2p_listen(wpa_s->global->p2p, timeout);
}


/**
 * wpas_p2p_cancel_remain_on_channel_cb - Remain-on-channel timeout
 * @wpa_s: Pointer to wpa_supplicant data from wpa_supplicant_add_iface()
 * @freq: Frequency of the channel in MHz
 *
 * This callback is called when the driver indicates that a remain-on-channel
 * operation has been completed, i.e., the duration on the requested channel
 * has timed out.
 */
void wpas_p2p_cancel_remain_on_channel_cb(struct wpa_supplicant *wpa_s,
					  unsigned int freq)
{
	wpa_printf(MSG_DEBUG, "P2P: Cancel remain-on-channel callback "
		   "(p2p_long_listen=%d ms pending_action_tx=%p)",
		   wpa_s->p2p_long_listen, offchannel_pending_action_tx(wpa_s));
	if (wpa_s->global->p2p_disabled || wpa_s->global->p2p == NULL)
		return;
	if (p2p_listen_end(wpa_s->global->p2p, freq) > 0)
		return; /* P2P module started a new operation */
	if (offchannel_pending_action_tx(wpa_s))
		return;
	if (wpa_s->p2p_long_listen > 0)
		wpa_s->p2p_long_listen -= wpa_s->max_remain_on_chan;
	if (wpa_s->p2p_long_listen > 0) {
		wpa_printf(MSG_DEBUG, "P2P: Continuing long Listen state");
		wpas_p2p_listen_start(wpa_s, wpa_s->p2p_long_listen);
	}
}


/**
 * wpas_p2p_group_remove - Remove a P2P group
 * @wpa_s: Pointer to wpa_supplicant data from wpa_supplicant_add_iface()
 * @ifname: Network interface name of the group interface or "*" to remove all
 *	groups
 * Returns: 0 on success, -1 on failure
 *
 * This function is used to remove a P2P group. This can be used to disconnect
 * from a group in which the local end is a P2P Client or to end a P2P Group in
 * case the local end is the Group Owner. If a virtual network interface was
 * created for this group, that interface will be removed. Otherwise, only the
 * configured P2P group network will be removed from the interface.
 */
int wpas_p2p_group_remove(struct wpa_supplicant *wpa_s, const char *ifname)
{
	struct wpa_global *global = wpa_s->global;

	if (os_strcmp(ifname, "*") == 0) {
		struct wpa_supplicant *prev;
		wpa_s = global->ifaces;
		while (wpa_s) {
			prev = wpa_s;
			wpa_s = wpa_s->next;
			wpas_p2p_disconnect(prev);
		}
		return 0;
	}

	for (wpa_s = global->ifaces; wpa_s; wpa_s = wpa_s->next) {
		if (os_strcmp(wpa_s->ifname, ifname) == 0)
			break;
	}

	return wpas_p2p_disconnect(wpa_s);
}


static int wpas_p2p_init_go_params(struct wpa_supplicant *wpa_s,
				   struct p2p_go_neg_results *params,
				   int freq, int ht40)
{
	u8 bssid[ETH_ALEN];
	int res;

	os_memset(params, 0, sizeof(*params));
	params->role_go = 1;
	params->ht40 = ht40;
	if (freq) {
		wpa_printf(MSG_DEBUG, "P2P: Set GO freq based on forced "
			   "frequency %d MHz", freq);
		params->freq = freq;
	} else if (wpa_s->conf->p2p_oper_reg_class == 81 &&
		   wpa_s->conf->p2p_oper_channel >= 1 &&
		   wpa_s->conf->p2p_oper_channel <= 11) {
		params->freq = 2407 + 5 * wpa_s->conf->p2p_oper_channel;
		wpa_printf(MSG_DEBUG, "P2P: Set GO freq based on configured "
			   "frequency %d MHz", params->freq);
	} else if (wpa_s->conf->p2p_oper_reg_class == 115 ||
		   wpa_s->conf->p2p_oper_reg_class == 124) {
		params->freq = 5000 + 5 * wpa_s->conf->p2p_oper_channel;
		wpa_printf(MSG_DEBUG, "P2P: Set GO freq based on configured "
			   "frequency %d MHz", params->freq);
	} else if (wpa_s->conf->p2p_oper_channel == 0 &&
		   wpa_s->best_overall_freq > 0 &&
		   p2p_supported_freq(wpa_s->global->p2p,
				      wpa_s->best_overall_freq)) {
		params->freq = wpa_s->best_overall_freq;
		wpa_printf(MSG_DEBUG, "P2P: Set GO freq based on best overall "
			   "channel %d MHz", params->freq);
	} else if (wpa_s->conf->p2p_oper_channel == 0 &&
		   wpa_s->best_24_freq > 0 &&
		   p2p_supported_freq(wpa_s->global->p2p,
				      wpa_s->best_24_freq)) {
		params->freq = wpa_s->best_24_freq;
		wpa_printf(MSG_DEBUG, "P2P: Set GO freq based on best 2.4 GHz "
			   "channel %d MHz", params->freq);
	} else if (wpa_s->conf->p2p_oper_channel == 0 &&
		   wpa_s->best_5_freq > 0 &&
		   p2p_supported_freq(wpa_s->global->p2p,
				      wpa_s->best_5_freq)) {
		params->freq = wpa_s->best_5_freq;
		wpa_printf(MSG_DEBUG, "P2P: Set GO freq based on best 5 GHz "
			   "channel %d MHz", params->freq);
	} else {
		int chan;
		for (chan = 0; chan < 11; chan++) {
			params->freq = 2412 + chan * 5;
			if (!wpas_p2p_disallowed_freq(wpa_s->global,
						      params->freq))
				break;
		}
		if (chan == 11) {
			wpa_printf(MSG_DEBUG, "P2P: No 2.4 GHz channel "
				   "allowed");
			return -1;
		}
		wpa_printf(MSG_DEBUG, "P2P: Set GO freq %d MHz (no preference "
			   "known)", params->freq);
	}

	if (wpa_s->current_ssid && wpa_drv_get_bssid(wpa_s, bssid) == 0 &&
	    wpa_s->assoc_freq && !freq) {
		wpa_printf(MSG_DEBUG, "P2P: Force GO on the channel we are "
			   "already using");
		params->freq = wpa_s->assoc_freq;
	}

	res = wpa_drv_shared_freq(wpa_s);
	if (res > 0 && !freq) {
		wpa_printf(MSG_DEBUG, "P2P: Force GO on the channel we are "
			   "already using on a shared interface");
		params->freq = res;
	} else if (res > 0 && freq != res &&
		   !(wpa_s->drv_flags &
		     WPA_DRIVER_FLAGS_MULTI_CHANNEL_CONCURRENT)) {
		wpa_printf(MSG_DEBUG, "P2P: Cannot start P2P group on %u MHz "
			   "while connected on another channel (%u MHz)",
			   freq, res);
		return -1;
	}

	return 0;
}


static struct wpa_supplicant *
wpas_p2p_get_group_iface(struct wpa_supplicant *wpa_s, int addr_allocated,
			 int go)
{
	struct wpa_supplicant *group_wpa_s;

	if (!wpas_p2p_create_iface(wpa_s)) {
		wpa_dbg(wpa_s, MSG_DEBUG, "P2P: Use same interface for group "
			"operations");
		return wpa_s;
	}

	if (wpas_p2p_add_group_interface(wpa_s, go ? WPA_IF_P2P_GO :
					 WPA_IF_P2P_CLIENT) < 0) {
		wpa_msg(wpa_s, MSG_ERROR, "P2P: Failed to add group interface");
		return NULL;
	}
	group_wpa_s = wpas_p2p_init_group_interface(wpa_s, go);
	if (group_wpa_s == NULL) {
		wpa_msg(wpa_s, MSG_ERROR, "P2P: Failed to initialize group "
			"interface");
		wpas_p2p_remove_pending_group_interface(wpa_s);
		return NULL;
	}

	wpa_dbg(wpa_s, MSG_DEBUG, "P2P: Use separate group interface %s",
		group_wpa_s->ifname);
	return group_wpa_s;
}


/**
 * wpas_p2p_group_add - Add a new P2P group with local end as Group Owner
 * @wpa_s: Pointer to wpa_supplicant data from wpa_supplicant_add_iface()
 * @persistent_group: Whether to create a persistent group
 * @freq: Frequency for the group or 0 to indicate no hardcoding
 * Returns: 0 on success, -1 on failure
 *
 * This function creates a new P2P group with the local end as the Group Owner,
 * i.e., without using Group Owner Negotiation.
 */
int wpas_p2p_group_add(struct wpa_supplicant *wpa_s, int persistent_group,
		       int freq, int ht40)
{
	struct p2p_go_neg_results params;
	unsigned int r;

	if (wpa_s->global->p2p_disabled || wpa_s->global->p2p == NULL)
		return -1;

	/* Make sure we are not running find during connection establishment */
	wpa_printf(MSG_DEBUG, "P2P: Stop any on-going P2P FIND");
	wpas_p2p_stop_find_oper(wpa_s);

	if (freq == 2) {
		wpa_printf(MSG_DEBUG, "P2P: Request to start GO on 2.4 GHz "
			   "band");
		if (wpa_s->best_24_freq > 0 &&
		    p2p_supported_freq(wpa_s->global->p2p,
				       wpa_s->best_24_freq)) {
			freq = wpa_s->best_24_freq;
			wpa_printf(MSG_DEBUG, "P2P: Use best 2.4 GHz band "
				   "channel: %d MHz", freq);
		} else {
			os_get_random((u8 *) &r, sizeof(r));
			freq = 2412 + (r % 3) * 25;
			wpa_printf(MSG_DEBUG, "P2P: Use random 2.4 GHz band "
				   "channel: %d MHz", freq);
		}
	}

	if (freq == 5) {
		wpa_printf(MSG_DEBUG, "P2P: Request to start GO on 5 GHz "
			   "band");
		if (wpa_s->best_5_freq > 0 &&
		    p2p_supported_freq(wpa_s->global->p2p,
				       wpa_s->best_5_freq)) {
			freq = wpa_s->best_5_freq;
			wpa_printf(MSG_DEBUG, "P2P: Use best 5 GHz band "
				   "channel: %d MHz", freq);
		} else {
			os_get_random((u8 *) &r, sizeof(r));
			freq = 5180 + (r % 4) * 20;
			if (!p2p_supported_freq(wpa_s->global->p2p, freq)) {
				wpa_printf(MSG_DEBUG, "P2P: Could not select "
					   "5 GHz channel for P2P group");
				return -1;
			}
			wpa_printf(MSG_DEBUG, "P2P: Use random 5 GHz band "
				   "channel: %d MHz", freq);
		}
	}

	if (freq > 0 && !p2p_supported_freq(wpa_s->global->p2p, freq)) {
		wpa_printf(MSG_DEBUG, "P2P: The forced channel for GO "
			   "(%u MHz) is not supported for P2P uses",
			   freq);
		return -1;
	}

	if (wpas_p2p_init_go_params(wpa_s, &params, freq, ht40))
		return -1;
	if (params.freq &&
	    !p2p_supported_freq(wpa_s->global->p2p, params.freq)) {
		wpa_printf(MSG_DEBUG, "P2P: The selected channel for GO "
			   "(%u MHz) is not supported for P2P uses",
			   params.freq);
		return -1;
	}
	p2p_go_params(wpa_s->global->p2p, &params);
	params.persistent_group = persistent_group;

	wpa_s = wpas_p2p_get_group_iface(wpa_s, 0, 1);
	if (wpa_s == NULL)
		return -1;
	wpas_start_wps_go(wpa_s, &params, 0);

	return 0;
}


static int wpas_start_p2p_client(struct wpa_supplicant *wpa_s,
				 struct wpa_ssid *params, int addr_allocated)
{
	struct wpa_ssid *ssid;

	wpa_s = wpas_p2p_get_group_iface(wpa_s, addr_allocated, 0);
	if (wpa_s == NULL)
		return -1;

	wpa_supplicant_ap_deinit(wpa_s);

	ssid = wpa_config_add_network(wpa_s->conf);
	if (ssid == NULL)
		return -1;
	wpa_config_set_network_defaults(ssid);
	ssid->temporary = 1;
	ssid->proto = WPA_PROTO_RSN;
	ssid->pairwise_cipher = WPA_CIPHER_CCMP;
	ssid->group_cipher = WPA_CIPHER_CCMP;
	ssid->key_mgmt = WPA_KEY_MGMT_PSK;
	ssid->ssid = os_malloc(params->ssid_len);
	if (ssid->ssid == NULL) {
		wpa_config_remove_network(wpa_s->conf, ssid->id);
		return -1;
	}
	os_memcpy(ssid->ssid, params->ssid, params->ssid_len);
	ssid->ssid_len = params->ssid_len;
	ssid->p2p_group = 1;
	ssid->export_keys = 1;
	if (params->psk_set) {
		os_memcpy(ssid->psk, params->psk, 32);
		ssid->psk_set = 1;
	}
	if (params->passphrase)
		ssid->passphrase = os_strdup(params->passphrase);

	wpa_supplicant_select_network(wpa_s, ssid);

	wpa_s->show_group_started = 1;

	return 0;
}


int wpas_p2p_group_add_persistent(struct wpa_supplicant *wpa_s,
				  struct wpa_ssid *ssid, int addr_allocated,
				  int freq, int ht40)
{
	struct p2p_go_neg_results params;
	int go = 0;

	if (ssid->disabled != 2 || ssid->ssid == NULL)
		return -1;

	if (wpas_get_p2p_group(wpa_s, ssid->ssid, ssid->ssid_len, &go) &&
	    go == (ssid->mode == WPAS_MODE_P2P_GO)) {
		wpa_printf(MSG_DEBUG, "P2P: Requested persistent group is "
			   "already running");
		return 0;
	}

	/* Make sure we are not running find during connection establishment */
	wpas_p2p_stop_find_oper(wpa_s);

	wpa_s->p2p_fallback_to_go_neg = 0;

	if (ssid->mode == WPAS_MODE_INFRA)
		return wpas_start_p2p_client(wpa_s, ssid, addr_allocated);

	if (ssid->mode != WPAS_MODE_P2P_GO)
		return -1;

	if (wpas_p2p_init_go_params(wpa_s, &params, freq, ht40))
		return -1;

	params.role_go = 1;
	params.psk_set = ssid->psk_set;
	if (params.psk_set)
		os_memcpy(params.psk, ssid->psk, sizeof(params.psk));
	if (ssid->passphrase) {
		if (os_strlen(ssid->passphrase) >= sizeof(params.passphrase)) {
			wpa_printf(MSG_ERROR, "P2P: Invalid passphrase in "
				   "persistent group");
			return -1;
		}
		os_strlcpy(params.passphrase, ssid->passphrase,
			   sizeof(params.passphrase));
	}
	os_memcpy(params.ssid, ssid->ssid, ssid->ssid_len);
	params.ssid_len = ssid->ssid_len;
	params.persistent_group = 1;

	wpa_s = wpas_p2p_get_group_iface(wpa_s, addr_allocated, 1);
	if (wpa_s == NULL)
		return -1;

	wpas_start_wps_go(wpa_s, &params, 0);

	return 0;
}


static void wpas_p2p_ie_update(void *ctx, struct wpabuf *beacon_ies,
			       struct wpabuf *proberesp_ies)
{
	struct wpa_supplicant *wpa_s = ctx;
	if (wpa_s->ap_iface) {
		struct hostapd_data *hapd = wpa_s->ap_iface->bss[0];
		if (!(hapd->conf->p2p & P2P_GROUP_OWNER)) {
			wpabuf_free(beacon_ies);
			wpabuf_free(proberesp_ies);
			return;
		}
		if (beacon_ies) {
			wpabuf_free(hapd->p2p_beacon_ie);
			hapd->p2p_beacon_ie = beacon_ies;
		}
		wpabuf_free(hapd->p2p_probe_resp_ie);
		hapd->p2p_probe_resp_ie = proberesp_ies;
	} else {
		wpabuf_free(beacon_ies);
		wpabuf_free(proberesp_ies);
	}
	wpa_supplicant_ap_update_beacon(wpa_s);
}


static void wpas_p2p_idle_update(void *ctx, int idle)
{
	struct wpa_supplicant *wpa_s = ctx;
	if (!wpa_s->ap_iface)
		return;
	wpa_printf(MSG_DEBUG, "P2P: GO - group %sidle", idle ? "" : "not ");
	if (idle)
		wpas_p2p_set_group_idle_timeout(wpa_s);
	else
		eloop_cancel_timeout(wpas_p2p_group_idle_timeout, wpa_s, NULL);
}


struct p2p_group * wpas_p2p_group_init(struct wpa_supplicant *wpa_s,
				       struct wpa_ssid *ssid)
{
	struct p2p_group *group;
	struct p2p_group_config *cfg;

	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT)
		return NULL;
	if (wpa_s->global->p2p_disabled || wpa_s->global->p2p == NULL)
		return NULL;

	cfg = os_zalloc(sizeof(*cfg));
	if (cfg == NULL)
		return NULL;

	if (ssid->p2p_persistent_group && wpa_s->conf->persistent_reconnect)
		cfg->persistent_group = 2;
	else if (ssid->p2p_persistent_group)
		cfg->persistent_group = 1;
	os_memcpy(cfg->interface_addr, wpa_s->own_addr, ETH_ALEN);
	if (wpa_s->max_stations &&
	    wpa_s->max_stations < wpa_s->conf->max_num_sta)
		cfg->max_clients = wpa_s->max_stations;
	else
		cfg->max_clients = wpa_s->conf->max_num_sta;
	os_memcpy(cfg->ssid, ssid->ssid, ssid->ssid_len);
	cfg->ssid_len = ssid->ssid_len;
	cfg->cb_ctx = wpa_s;
	cfg->ie_update = wpas_p2p_ie_update;
	cfg->idle_update = wpas_p2p_idle_update;

	group = p2p_group_init(wpa_s->global->p2p, cfg);
	if (group == NULL)
		os_free(cfg);
	if (ssid->mode != WPAS_MODE_P2P_GROUP_FORMATION)
		p2p_group_notif_formation_done(group);
	wpa_s->p2p_group = group;
	return group;
}


void wpas_p2p_wps_success(struct wpa_supplicant *wpa_s, const u8 *peer_addr,
			  int registrar)
{
	struct wpa_ssid *ssid = wpa_s->current_ssid;

	if (!wpa_s->p2p_in_provisioning) {
		wpa_printf(MSG_DEBUG, "P2P: Ignore WPS success event - P2P "
			   "provisioning not in progress");
		return;
	}

	if (ssid && ssid->mode == WPAS_MODE_INFRA) {
		u8 go_dev_addr[ETH_ALEN];
		os_memcpy(go_dev_addr, wpa_s->bssid, ETH_ALEN);
		wpas_p2p_persistent_group(wpa_s, go_dev_addr, ssid->ssid,
					  ssid->ssid_len);
		/* Clear any stored provisioning info */
		p2p_clear_provisioning_info(wpa_s->global->p2p, go_dev_addr);
	}

	eloop_cancel_timeout(wpas_p2p_group_formation_timeout, wpa_s->parent,
			     NULL);
	if (ssid && ssid->mode == WPAS_MODE_INFRA) {
		/*
		 * Use a separate timeout for initial data connection to
		 * complete to allow the group to be removed automatically if
		 * something goes wrong in this step before the P2P group idle
		 * timeout mechanism is taken into use.
		 */
		eloop_register_timeout(P2P_MAX_INITIAL_CONN_WAIT, 0,
				       wpas_p2p_group_formation_timeout,
				       wpa_s->parent, NULL);
	}
	if (wpa_s->global->p2p)
		p2p_wps_success_cb(wpa_s->global->p2p, peer_addr);
	else if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT)
		wpa_drv_wps_success_cb(wpa_s, peer_addr);
	wpas_group_formation_completed(wpa_s, 1);
}


void wpas_p2p_wps_failed(struct wpa_supplicant *wpa_s,
			 struct wps_event_fail *fail)
{
	if (!wpa_s->p2p_in_provisioning) {
		wpa_printf(MSG_DEBUG, "P2P: Ignore WPS fail event - P2P "
			   "provisioning not in progress");
		return;
	}

	if (wpa_s->go_params) {
		p2p_clear_provisioning_info(
			wpa_s->global->p2p,
			wpa_s->go_params->peer_device_addr);
	}

	wpas_notify_p2p_wps_failed(wpa_s, fail);
}


int wpas_p2p_prov_disc(struct wpa_supplicant *wpa_s, const u8 *peer_addr,
		       const char *config_method,
		       enum wpas_p2p_prov_disc_use use)
{
	u16 config_methods;

	wpa_s->p2p_fallback_to_go_neg = 0;
	wpa_s->pending_pd_use = NORMAL_PD;
	if (os_strncmp(config_method, "display", 7) == 0)
		config_methods = WPS_CONFIG_DISPLAY;
	else if (os_strncmp(config_method, "keypad", 6) == 0)
		config_methods = WPS_CONFIG_KEYPAD;
	else if (os_strncmp(config_method, "pbc", 3) == 0 ||
		 os_strncmp(config_method, "pushbutton", 10) == 0)
		config_methods = WPS_CONFIG_PUSHBUTTON;
	else {
		wpa_printf(MSG_DEBUG, "P2P: Unknown config method");
		return -1;
	}

	if (use == WPAS_P2P_PD_AUTO) {
		os_memcpy(wpa_s->pending_join_dev_addr, peer_addr, ETH_ALEN);
		wpa_s->pending_pd_config_methods = config_methods;
		wpa_s->p2p_auto_pd = 1;
		wpa_s->p2p_auto_join = 0;
		wpa_s->pending_pd_before_join = 0;
		wpa_s->auto_pd_scan_retry = 0;
		wpas_p2p_stop_find(wpa_s);
		wpa_s->p2p_join_scan_count = 0;
		os_get_time(&wpa_s->p2p_auto_started);
		wpa_printf(MSG_DEBUG, "P2P: Auto PD started at %ld.%06ld",
			   wpa_s->p2p_auto_started.sec,
			   wpa_s->p2p_auto_started.usec);
		wpas_p2p_join_scan(wpa_s, NULL);
		return 0;
	}

	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT) {
		return wpa_drv_p2p_prov_disc_req(wpa_s, peer_addr,
						 config_methods,
						 use == WPAS_P2P_PD_FOR_JOIN);
	}

	if (wpa_s->global->p2p == NULL || wpa_s->global->p2p_disabled)
		return -1;

	return p2p_prov_disc_req(wpa_s->global->p2p, peer_addr,
				 config_methods, use == WPAS_P2P_PD_FOR_JOIN,
				 0, 1);
}


int wpas_p2p_scan_result_text(const u8 *ies, size_t ies_len, char *buf,
			      char *end)
{
	return p2p_scan_result_text(ies, ies_len, buf, end);
}


static void wpas_p2p_clear_pending_action_tx(struct wpa_supplicant *wpa_s)
{
	if (!offchannel_pending_action_tx(wpa_s))
		return;

	wpa_printf(MSG_DEBUG, "P2P: Drop pending Action TX due to new "
		   "operation request");
	offchannel_clear_pending_action_tx(wpa_s);
}


int wpas_p2p_find(struct wpa_supplicant *wpa_s, unsigned int timeout,
		  enum p2p_discovery_type type,
		  unsigned int num_req_dev_types, const u8 *req_dev_types,
		  const u8 *dev_id, unsigned int search_delay)
{
	wpas_p2p_clear_pending_action_tx(wpa_s);
	wpa_s->p2p_long_listen = 0;

	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT)
		return wpa_drv_p2p_find(wpa_s, timeout, type);

	if (wpa_s->global->p2p_disabled || wpa_s->global->p2p == NULL ||
	    wpa_s->p2p_in_provisioning)
		return -1;

	wpa_supplicant_cancel_sched_scan(wpa_s);

	return p2p_find(wpa_s->global->p2p, timeout, type,
			num_req_dev_types, req_dev_types, dev_id,
			search_delay);
}


static int wpas_p2p_stop_find_oper(struct wpa_supplicant *wpa_s)
{
	wpas_p2p_clear_pending_action_tx(wpa_s);
	wpa_s->p2p_long_listen = 0;
	eloop_cancel_timeout(wpas_p2p_long_listen_timeout, wpa_s, NULL);
	eloop_cancel_timeout(wpas_p2p_join_scan, wpa_s, NULL);
	wpa_s->global->p2p_cb_on_scan_complete = 0;

	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT) {
		wpa_drv_p2p_stop_find(wpa_s);
		return 1;
	}

	if (wpa_s->global->p2p)
		p2p_stop_find(wpa_s->global->p2p);

	return 0;
}


void wpas_p2p_stop_find(struct wpa_supplicant *wpa_s)
{
	if (wpas_p2p_stop_find_oper(wpa_s) > 0)
		return;
	wpas_p2p_remove_pending_group_interface(wpa_s);
}


static void wpas_p2p_long_listen_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	wpa_s->p2p_long_listen = 0;
}


int wpas_p2p_listen(struct wpa_supplicant *wpa_s, unsigned int timeout)
{
	int res;

	if (wpa_s->global->p2p_disabled || wpa_s->global->p2p == NULL)
		return -1;

	wpa_supplicant_cancel_sched_scan(wpa_s);
	wpas_p2p_clear_pending_action_tx(wpa_s);

	if (timeout == 0) {
		/*
		 * This is a request for unlimited Listen state. However, at
		 * least for now, this is mapped to a Listen state for one
		 * hour.
		 */
		timeout = 3600;
	}
	eloop_cancel_timeout(wpas_p2p_long_listen_timeout, wpa_s, NULL);
	wpa_s->p2p_long_listen = 0;

	/*
	 * Stop previous find/listen operation to avoid trying to request a new
	 * remain-on-channel operation while the driver is still running the
	 * previous one.
	 */
	if (wpa_s->global->p2p)
		p2p_stop_find(wpa_s->global->p2p);

	res = wpas_p2p_listen_start(wpa_s, timeout * 1000);
	if (res == 0 && timeout * 1000 > wpa_s->max_remain_on_chan) {
		wpa_s->p2p_long_listen = timeout * 1000;
		eloop_register_timeout(timeout, 0,
				       wpas_p2p_long_listen_timeout,
				       wpa_s, NULL);
	}

	return res;
}


int wpas_p2p_assoc_req_ie(struct wpa_supplicant *wpa_s, struct wpa_bss *bss,
			  u8 *buf, size_t len, int p2p_group)
{
	struct wpabuf *p2p_ie;
	int ret;

	if (wpa_s->global->p2p_disabled)
		return -1;
	if (wpa_s->global->p2p == NULL)
		return -1;
	if (bss == NULL)
		return -1;

	p2p_ie = wpa_bss_get_vendor_ie_multi(bss, P2P_IE_VENDOR_TYPE);
	ret = p2p_assoc_req_ie(wpa_s->global->p2p, bss->bssid, buf, len,
			       p2p_group, p2p_ie);
	wpabuf_free(p2p_ie);

	return ret;
}


int wpas_p2p_probe_req_rx(struct wpa_supplicant *wpa_s, const u8 *addr,
			  const u8 *dst, const u8 *bssid,
			  const u8 *ie, size_t ie_len, int ssi_signal)
{
	if (wpa_s->global->p2p_disabled)
		return 0;
	if (wpa_s->global->p2p == NULL)
		return 0;

	switch (p2p_probe_req_rx(wpa_s->global->p2p, addr, dst, bssid,
				 ie, ie_len)) {
	case P2P_PREQ_NOT_P2P:
		wpas_notify_preq(wpa_s, addr, dst, bssid, ie, ie_len,
				 ssi_signal);
		/* fall through */
	case P2P_PREQ_MALFORMED:
	case P2P_PREQ_NOT_LISTEN:
	case P2P_PREQ_NOT_PROCESSED:
	default: /* make gcc happy */
		return 0;
	case P2P_PREQ_PROCESSED:
		return 1;
	}
}


void wpas_p2p_rx_action(struct wpa_supplicant *wpa_s, const u8 *da,
			const u8 *sa, const u8 *bssid,
			u8 category, const u8 *data, size_t len, int freq)
{
	if (wpa_s->global->p2p_disabled)
		return;
	if (wpa_s->global->p2p == NULL)
		return;

	p2p_rx_action(wpa_s->global->p2p, da, sa, bssid, category, data, len,
		      freq);
}


void wpas_p2p_scan_ie(struct wpa_supplicant *wpa_s, struct wpabuf *ies)
{
	if (wpa_s->global->p2p_disabled)
		return;
	if (wpa_s->global->p2p == NULL)
		return;

	p2p_scan_ie(wpa_s->global->p2p, ies, NULL);
}


void wpas_p2p_group_deinit(struct wpa_supplicant *wpa_s)
{
	p2p_group_deinit(wpa_s->p2p_group);
	wpa_s->p2p_group = NULL;

	wpa_s->ap_configured_cb = NULL;
	wpa_s->ap_configured_cb_ctx = NULL;
	wpa_s->ap_configured_cb_data = NULL;
	wpa_s->connect_without_scan = NULL;
}


int wpas_p2p_reject(struct wpa_supplicant *wpa_s, const u8 *addr)
{
	wpa_s->p2p_long_listen = 0;

	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT)
		return wpa_drv_p2p_reject(wpa_s, addr);

	if (wpa_s->global->p2p_disabled || wpa_s->global->p2p == NULL)
		return -1;

	return p2p_reject(wpa_s->global->p2p, addr);
}


/* Invite to reinvoke a persistent group */
int wpas_p2p_invite(struct wpa_supplicant *wpa_s, const u8 *peer_addr,
		    struct wpa_ssid *ssid, const u8 *go_dev_addr, int freq,
		    int ht40)
{
	enum p2p_invite_role role;
	u8 *bssid = NULL;

	wpa_s->p2p_persistent_go_freq = freq;
	wpa_s->p2p_go_ht40 = !!ht40;
	if (ssid->mode == WPAS_MODE_P2P_GO) {
		role = P2P_INVITE_ROLE_GO;
		if (peer_addr == NULL) {
			wpa_printf(MSG_DEBUG, "P2P: Missing peer "
				   "address in invitation command");
			return -1;
		}
		if (wpas_p2p_create_iface(wpa_s)) {
			if (wpas_p2p_add_group_interface(wpa_s,
							 WPA_IF_P2P_GO) < 0) {
				wpa_printf(MSG_ERROR, "P2P: Failed to "
					   "allocate a new interface for the "
					   "group");
				return -1;
			}
			bssid = wpa_s->pending_interface_addr;
		} else
			bssid = wpa_s->own_addr;
	} else {
		role = P2P_INVITE_ROLE_CLIENT;
		peer_addr = ssid->bssid;
	}
	wpa_s->pending_invite_ssid_id = ssid->id;

	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT)
		return wpa_drv_p2p_invite(wpa_s, peer_addr, role, bssid,
					  ssid->ssid, ssid->ssid_len,
					  go_dev_addr, 1);

	if (wpa_s->global->p2p_disabled || wpa_s->global->p2p == NULL)
		return -1;

	return p2p_invite(wpa_s->global->p2p, peer_addr, role, bssid,
			  ssid->ssid, ssid->ssid_len, freq, go_dev_addr, 1);
}


/* Invite to join an active group */
int wpas_p2p_invite_group(struct wpa_supplicant *wpa_s, const char *ifname,
			  const u8 *peer_addr, const u8 *go_dev_addr)
{
	struct wpa_global *global = wpa_s->global;
	enum p2p_invite_role role;
	u8 *bssid = NULL;
	struct wpa_ssid *ssid;
	int persistent;

	wpa_s->p2p_persistent_go_freq = 0;
	wpa_s->p2p_go_ht40 = 0;

	for (wpa_s = global->ifaces; wpa_s; wpa_s = wpa_s->next) {
		if (os_strcmp(wpa_s->ifname, ifname) == 0)
			break;
	}
	if (wpa_s == NULL) {
		wpa_printf(MSG_DEBUG, "P2P: Interface '%s' not found", ifname);
		return -1;
	}

	ssid = wpa_s->current_ssid;
	if (ssid == NULL) {
		wpa_printf(MSG_DEBUG, "P2P: No current SSID to use for "
			   "invitation");
		return -1;
	}

	persistent = ssid->p2p_persistent_group &&
		wpas_p2p_get_persistent(wpa_s->parent, peer_addr,
					ssid->ssid, ssid->ssid_len);

	if (ssid->mode == WPAS_MODE_P2P_GO) {
		role = P2P_INVITE_ROLE_ACTIVE_GO;
		bssid = wpa_s->own_addr;
		if (go_dev_addr == NULL)
			go_dev_addr = wpa_s->global->p2p_dev_addr;
	} else {
		role = P2P_INVITE_ROLE_CLIENT;
		if (wpa_s->wpa_state < WPA_ASSOCIATED) {
			wpa_printf(MSG_DEBUG, "P2P: Not associated - cannot "
				   "invite to current group");
			return -1;
		}
		bssid = wpa_s->bssid;
		if (go_dev_addr == NULL &&
		    !is_zero_ether_addr(wpa_s->go_dev_addr))
			go_dev_addr = wpa_s->go_dev_addr;
	}
	wpa_s->parent->pending_invite_ssid_id = -1;

	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT)
		return wpa_drv_p2p_invite(wpa_s, peer_addr, role, bssid,
					  ssid->ssid, ssid->ssid_len,
					  go_dev_addr, persistent);

	if (wpa_s->global->p2p_disabled || wpa_s->global->p2p == NULL)
		return -1;

	return p2p_invite(wpa_s->global->p2p, peer_addr, role, bssid,
			  ssid->ssid, ssid->ssid_len, wpa_s->assoc_freq,
			  go_dev_addr, persistent);
}


void wpas_p2p_completed(struct wpa_supplicant *wpa_s)
{
	struct wpa_ssid *ssid = wpa_s->current_ssid;
	const char *ssid_txt;
	u8 go_dev_addr[ETH_ALEN];
	int network_id = -1;
	int persistent;
	int freq;

	if (ssid == NULL || ssid->mode != WPAS_MODE_P2P_GROUP_FORMATION) {
		eloop_cancel_timeout(wpas_p2p_group_formation_timeout,
				     wpa_s->parent, NULL);
	}

	if (!wpa_s->show_group_started || !ssid)
		goto done;

	wpa_s->show_group_started = 0;

	ssid_txt = wpa_ssid_txt(ssid->ssid, ssid->ssid_len);
	os_memset(go_dev_addr, 0, ETH_ALEN);
	if (ssid->bssid_set)
		os_memcpy(go_dev_addr, ssid->bssid, ETH_ALEN);
	persistent = wpas_p2p_persistent_group(wpa_s, go_dev_addr, ssid->ssid,
					       ssid->ssid_len);
	os_memcpy(wpa_s->go_dev_addr, go_dev_addr, ETH_ALEN);

	if (wpa_s->global->p2p_group_formation == wpa_s)
		wpa_s->global->p2p_group_formation = NULL;

	freq = wpa_s->current_bss ? wpa_s->current_bss->freq :
		(int) wpa_s->assoc_freq;
	if (ssid->passphrase == NULL && ssid->psk_set) {
		char psk[65];
		wpa_snprintf_hex(psk, sizeof(psk), ssid->psk, 32);
		wpa_msg(wpa_s->parent, MSG_INFO, P2P_EVENT_GROUP_STARTED
			"%s client ssid=\"%s\" freq=%d psk=%s go_dev_addr="
			MACSTR "%s",
			wpa_s->ifname, ssid_txt, freq, psk,
			MAC2STR(go_dev_addr),
			persistent ? " [PERSISTENT]" : "");
	} else {
		wpa_msg(wpa_s->parent, MSG_INFO, P2P_EVENT_GROUP_STARTED
			"%s client ssid=\"%s\" freq=%d passphrase=\"%s\" "
			"go_dev_addr=" MACSTR "%s",
			wpa_s->ifname, ssid_txt, freq,
			ssid->passphrase ? ssid->passphrase : "",
			MAC2STR(go_dev_addr),
			persistent ? " [PERSISTENT]" : "");
	}

	if (persistent)
		network_id = wpas_p2p_store_persistent_group(wpa_s->parent,
							     ssid, go_dev_addr);
	if (network_id < 0)
		network_id = ssid->id;
	wpas_notify_p2p_group_started(wpa_s, ssid, network_id, 1);

done:
	if (wpa_s->global->p2p_cb_on_scan_complete && !wpa_s->global->p2p_disabled &&
	    wpa_s->global->p2p != NULL) {
		wpa_s->global->p2p_cb_on_scan_complete = 0;
		if (p2p_other_scan_completed(wpa_s->global->p2p) == 1) {
			wpa_dbg(wpa_s, MSG_DEBUG, "P2P: Pending P2P operation "
				"continued after successful connection");
			p2p_increase_search_delay(
				wpa_s->global->p2p,
				wpas_p2p_search_delay(wpa_s));
		}
	}
}


int wpas_p2p_presence_req(struct wpa_supplicant *wpa_s, u32 duration1,
			  u32 interval1, u32 duration2, u32 interval2)
{
	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT)
		return -1;
	if (wpa_s->global->p2p_disabled || wpa_s->global->p2p == NULL)
		return -1;

	if (wpa_s->wpa_state < WPA_ASSOCIATED ||
	    wpa_s->current_ssid == NULL ||
	    wpa_s->current_ssid->mode != WPAS_MODE_INFRA)
		return -1;

	return p2p_presence_req(wpa_s->global->p2p, wpa_s->bssid,
				wpa_s->own_addr, wpa_s->assoc_freq,
				duration1, interval1, duration2, interval2);
}


int wpas_p2p_ext_listen(struct wpa_supplicant *wpa_s, unsigned int period,
			unsigned int interval)
{
	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT)
		return -1;

	if (wpa_s->global->p2p_disabled || wpa_s->global->p2p == NULL)
		return -1;

	return p2p_ext_listen(wpa_s->global->p2p, period, interval);
}


static int wpas_p2p_is_client(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->current_ssid == NULL) {
		/*
		 * current_ssid can be cleared when P2P client interface gets
		 * disconnected, so assume this interface was used as P2P
		 * client.
		 */
		return 1;
	}
	return wpa_s->current_ssid->p2p_group &&
		wpa_s->current_ssid->mode == WPAS_MODE_INFRA;
}


static void wpas_p2p_group_idle_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;

	if (wpa_s->conf->p2p_group_idle == 0 && !wpas_p2p_is_client(wpa_s)) {
		wpa_printf(MSG_DEBUG, "P2P: Ignore group idle timeout - "
			   "disabled");
		return;
	}

	wpa_printf(MSG_DEBUG, "P2P: Group idle timeout reached - terminate "
		   "group");
	wpas_p2p_group_delete(wpa_s, P2P_GROUP_REMOVAL_IDLE_TIMEOUT);
}


static void wpas_p2p_set_group_idle_timeout(struct wpa_supplicant *wpa_s)
{
	int timeout;

	if (eloop_cancel_timeout(wpas_p2p_group_idle_timeout, wpa_s, NULL) > 0)
		wpa_printf(MSG_DEBUG, "P2P: Cancelled P2P group idle timeout");

	if (wpa_s->current_ssid == NULL || !wpa_s->current_ssid->p2p_group)
		return;

	timeout = wpa_s->conf->p2p_group_idle;
	if (wpa_s->current_ssid->mode == WPAS_MODE_INFRA &&
	    (timeout == 0 || timeout > P2P_MAX_CLIENT_IDLE))
	    timeout = P2P_MAX_CLIENT_IDLE;

	if (timeout == 0)
		return;

	if (timeout < 0) {
		if (wpa_s->current_ssid->mode == WPAS_MODE_INFRA)
			timeout = 0; /* special client mode no-timeout */
		else
			return;
	}

	if (wpa_s->p2p_in_provisioning) {
		/*
		 * Use the normal group formation timeout during the
		 * provisioning phase to avoid terminating this process too
		 * early due to group idle timeout.
		 */
		wpa_printf(MSG_DEBUG, "P2P: Do not use P2P group idle timeout "
			   "during provisioning");
		return;
	}

	if (wpa_s->show_group_started) {
		/*
		 * Use the normal group formation timeout between the end of
		 * the provisioning phase and completion of 4-way handshake to
		 * avoid terminating this process too early due to group idle
		 * timeout.
		 */
		wpa_printf(MSG_DEBUG, "P2P: Do not use P2P group idle timeout "
			   "while waiting for initial 4-way handshake to "
			   "complete");
		return;
	}

	wpa_printf(MSG_DEBUG, "P2P: Set P2P group idle timeout to %u seconds",
		   timeout);
	eloop_register_timeout(timeout, 0, wpas_p2p_group_idle_timeout,
			       wpa_s, NULL);
}


/* Returns 1 if the interface was removed */
int wpas_p2p_deauth_notif(struct wpa_supplicant *wpa_s, const u8 *bssid,
			  u16 reason_code, const u8 *ie, size_t ie_len,
			  int locally_generated)
{
	if (wpa_s->global->p2p_disabled || wpa_s->global->p2p == NULL)
		return 0;
	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT)
		return 0;

	if (!locally_generated)
		p2p_deauth_notif(wpa_s->global->p2p, bssid, reason_code, ie,
				 ie_len);

	if (reason_code == WLAN_REASON_DEAUTH_LEAVING && !locally_generated &&
	    wpa_s->current_ssid &&
	    wpa_s->current_ssid->p2p_group &&
	    wpa_s->current_ssid->mode == WPAS_MODE_INFRA) {
		wpa_printf(MSG_DEBUG, "P2P: GO indicated that the P2P Group "
			   "session is ending");
		if (wpas_p2p_group_delete(wpa_s,
					  P2P_GROUP_REMOVAL_GO_ENDING_SESSION)
		    > 0)
			return 1;
	}

	return 0;
}


void wpas_p2p_disassoc_notif(struct wpa_supplicant *wpa_s, const u8 *bssid,
			     u16 reason_code, const u8 *ie, size_t ie_len,
			     int locally_generated)
{
	if (wpa_s->global->p2p_disabled || wpa_s->global->p2p == NULL)
		return;
	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT)
		return;

	if (!locally_generated)
		p2p_disassoc_notif(wpa_s->global->p2p, bssid, reason_code, ie,
				   ie_len);
}


void wpas_p2p_update_config(struct wpa_supplicant *wpa_s)
{
	struct p2p_data *p2p = wpa_s->global->p2p;

	if (p2p == NULL)
		return;

	if (!(wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_CAPABLE))
		return;

	if (wpa_s->conf->changed_parameters & CFG_CHANGED_DEVICE_NAME)
		p2p_set_dev_name(p2p, wpa_s->conf->device_name);

	if (wpa_s->conf->changed_parameters & CFG_CHANGED_DEVICE_TYPE)
		p2p_set_pri_dev_type(p2p, wpa_s->conf->device_type);

	if (wpa_s->wps &&
	    (wpa_s->conf->changed_parameters & CFG_CHANGED_CONFIG_METHODS))
		p2p_set_config_methods(p2p, wpa_s->wps->config_methods);

	if (wpa_s->wps && (wpa_s->conf->changed_parameters & CFG_CHANGED_UUID))
		p2p_set_uuid(p2p, wpa_s->wps->uuid);

	if (wpa_s->conf->changed_parameters & CFG_CHANGED_WPS_STRING) {
		p2p_set_manufacturer(p2p, wpa_s->conf->manufacturer);
		p2p_set_model_name(p2p, wpa_s->conf->model_name);
		p2p_set_model_number(p2p, wpa_s->conf->model_number);
		p2p_set_serial_number(p2p, wpa_s->conf->serial_number);
	}

	if (wpa_s->conf->changed_parameters & CFG_CHANGED_SEC_DEVICE_TYPE)
		p2p_set_sec_dev_types(p2p,
				      (void *) wpa_s->conf->sec_device_type,
				      wpa_s->conf->num_sec_device_types);

	if (wpa_s->conf->changed_parameters & CFG_CHANGED_VENDOR_EXTENSION) {
		int i;
		p2p_remove_wps_vendor_extensions(p2p);
		for (i = 0; i < MAX_WPS_VENDOR_EXT; i++) {
			if (wpa_s->conf->wps_vendor_ext[i] == NULL)
				continue;
			p2p_add_wps_vendor_extension(
				p2p, wpa_s->conf->wps_vendor_ext[i]);
		}
	}

	if ((wpa_s->conf->changed_parameters & CFG_CHANGED_COUNTRY) &&
	    wpa_s->conf->country[0] && wpa_s->conf->country[1]) {
		char country[3];
		country[0] = wpa_s->conf->country[0];
		country[1] = wpa_s->conf->country[1];
		country[2] = 0x04;
		p2p_set_country(p2p, country);
	}

	if (wpa_s->conf->changed_parameters & CFG_CHANGED_P2P_SSID_POSTFIX) {
		p2p_set_ssid_postfix(p2p, (u8 *) wpa_s->conf->p2p_ssid_postfix,
				     wpa_s->conf->p2p_ssid_postfix ?
				     os_strlen(wpa_s->conf->p2p_ssid_postfix) :
				     0);
	}

	if (wpa_s->conf->changed_parameters & CFG_CHANGED_P2P_INTRA_BSS)
		p2p_set_intra_bss_dist(p2p, wpa_s->conf->p2p_intra_bss);

	if (wpa_s->conf->changed_parameters & CFG_CHANGED_P2P_LISTEN_CHANNEL) {
		u8 reg_class, channel;
		int ret;
		unsigned int r;
		if (wpa_s->conf->p2p_listen_reg_class &&
		    wpa_s->conf->p2p_listen_channel) {
			reg_class = wpa_s->conf->p2p_listen_reg_class;
			channel = wpa_s->conf->p2p_listen_channel;
		} else {
			reg_class = 81;
			/*
			 * Pick one of the social channels randomly as the
			 * listen channel.
			 */
			os_get_random((u8 *) &r, sizeof(r));
			channel = 1 + (r % 3) * 5;
		}
		ret = p2p_set_listen_channel(p2p, reg_class, channel);
		if (ret)
			wpa_printf(MSG_ERROR, "P2P: Own listen channel update "
				   "failed: %d", ret);
	}
	if (wpa_s->conf->changed_parameters & CFG_CHANGED_P2P_OPER_CHANNEL) {
		u8 op_reg_class, op_channel, cfg_op_channel;
		int ret = 0;
		unsigned int r;
		if (wpa_s->conf->p2p_oper_reg_class &&
		    wpa_s->conf->p2p_oper_channel) {
			op_reg_class = wpa_s->conf->p2p_oper_reg_class;
			op_channel = wpa_s->conf->p2p_oper_channel;
			cfg_op_channel = 1;
		} else {
			op_reg_class = 81;
			/*
			 * Use random operation channel from (1, 6, 11)
			 *if no other preference is indicated.
			 */
			os_get_random((u8 *) &r, sizeof(r));
			op_channel = 1 + (r % 3) * 5;
			cfg_op_channel = 0;
		}
		ret = p2p_set_oper_channel(p2p, op_reg_class, op_channel,
					   cfg_op_channel);
		if (ret)
			wpa_printf(MSG_ERROR, "P2P: Own oper channel update "
				   "failed: %d", ret);
	}

	if (wpa_s->conf->changed_parameters & CFG_CHANGED_P2P_PREF_CHAN) {
		if (p2p_set_pref_chan(p2p, wpa_s->conf->num_p2p_pref_chan,
				      wpa_s->conf->p2p_pref_chan) < 0) {
			wpa_printf(MSG_ERROR, "P2P: Preferred channel list "
				   "update failed");
		}
	}
}


int wpas_p2p_set_noa(struct wpa_supplicant *wpa_s, u8 count, int start,
		     int duration)
{
	if (!wpa_s->ap_iface)
		return -1;
	return hostapd_p2p_set_noa(wpa_s->ap_iface->bss[0], count, start,
				   duration);
}


int wpas_p2p_set_cross_connect(struct wpa_supplicant *wpa_s, int enabled)
{
	if (wpa_s->global->p2p_disabled || wpa_s->global->p2p == NULL)
		return -1;
	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT)
		return -1;

	wpa_s->global->cross_connection = enabled;
	p2p_set_cross_connect(wpa_s->global->p2p, enabled);

	if (!enabled) {
		struct wpa_supplicant *iface;

		for (iface = wpa_s->global->ifaces; iface; iface = iface->next)
		{
			if (iface->cross_connect_enabled == 0)
				continue;

			iface->cross_connect_enabled = 0;
			iface->cross_connect_in_use = 0;
			wpa_msg(iface->parent, MSG_INFO,
				P2P_EVENT_CROSS_CONNECT_DISABLE "%s %s",
				iface->ifname, iface->cross_connect_uplink);
		}
	}

	return 0;
}


static void wpas_p2p_enable_cross_connect(struct wpa_supplicant *uplink)
{
	struct wpa_supplicant *iface;

	if (!uplink->global->cross_connection)
		return;

	for (iface = uplink->global->ifaces; iface; iface = iface->next) {
		if (!iface->cross_connect_enabled)
			continue;
		if (os_strcmp(uplink->ifname, iface->cross_connect_uplink) !=
		    0)
			continue;
		if (iface->ap_iface == NULL)
			continue;
		if (iface->cross_connect_in_use)
			continue;

		iface->cross_connect_in_use = 1;
		wpa_msg(iface->parent, MSG_INFO,
			P2P_EVENT_CROSS_CONNECT_ENABLE "%s %s",
			iface->ifname, iface->cross_connect_uplink);
	}
}


static void wpas_p2p_disable_cross_connect(struct wpa_supplicant *uplink)
{
	struct wpa_supplicant *iface;

	for (iface = uplink->global->ifaces; iface; iface = iface->next) {
		if (!iface->cross_connect_enabled)
			continue;
		if (os_strcmp(uplink->ifname, iface->cross_connect_uplink) !=
		    0)
			continue;
		if (!iface->cross_connect_in_use)
			continue;

		wpa_msg(iface->parent, MSG_INFO,
			P2P_EVENT_CROSS_CONNECT_DISABLE "%s %s",
			iface->ifname, iface->cross_connect_uplink);
		iface->cross_connect_in_use = 0;
	}
}


void wpas_p2p_notif_connected(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->ap_iface || wpa_s->current_ssid == NULL ||
	    wpa_s->current_ssid->mode != WPAS_MODE_INFRA ||
	    wpa_s->cross_connect_disallowed)
		wpas_p2p_disable_cross_connect(wpa_s);
	else
		wpas_p2p_enable_cross_connect(wpa_s);
	if (!wpa_s->ap_iface &&
	    eloop_cancel_timeout(wpas_p2p_group_idle_timeout, wpa_s, NULL) > 0)
		wpa_printf(MSG_DEBUG, "P2P: Cancelled P2P group idle timeout");
}


void wpas_p2p_notif_disconnected(struct wpa_supplicant *wpa_s)
{
	wpas_p2p_disable_cross_connect(wpa_s);
	if (!wpa_s->ap_iface &&
	    !eloop_is_timeout_registered(wpas_p2p_group_idle_timeout,
					 wpa_s, NULL))
		wpas_p2p_set_group_idle_timeout(wpa_s);
}


static void wpas_p2p_cross_connect_setup(struct wpa_supplicant *wpa_s)
{
	struct wpa_supplicant *iface;

	if (!wpa_s->global->cross_connection)
		return;

	for (iface = wpa_s->global->ifaces; iface; iface = iface->next) {
		if (iface == wpa_s)
			continue;
		if (iface->drv_flags &
		    WPA_DRIVER_FLAGS_P2P_DEDICATED_INTERFACE)
			continue;
		if (iface->drv_flags & WPA_DRIVER_FLAGS_P2P_CAPABLE)
			continue;

		wpa_s->cross_connect_enabled = 1;
		os_strlcpy(wpa_s->cross_connect_uplink, iface->ifname,
			   sizeof(wpa_s->cross_connect_uplink));
		wpa_printf(MSG_DEBUG, "P2P: Enable cross connection from "
			   "%s to %s whenever uplink is available",
			   wpa_s->ifname, wpa_s->cross_connect_uplink);

		if (iface->ap_iface || iface->current_ssid == NULL ||
		    iface->current_ssid->mode != WPAS_MODE_INFRA ||
		    iface->cross_connect_disallowed ||
		    iface->wpa_state != WPA_COMPLETED)
			break;

		wpa_s->cross_connect_in_use = 1;
		wpa_msg(wpa_s->parent, MSG_INFO,
			P2P_EVENT_CROSS_CONNECT_ENABLE "%s %s",
			wpa_s->ifname, wpa_s->cross_connect_uplink);
		break;
	}
}


int wpas_p2p_notif_pbc_overlap(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->p2p_group_interface != P2P_GROUP_INTERFACE_CLIENT &&
	    !wpa_s->p2p_in_provisioning)
		return 0; /* not P2P client operation */

	wpa_printf(MSG_DEBUG, "P2P: Terminate connection due to WPS PBC "
		   "session overlap");
	if (wpa_s != wpa_s->parent)
		wpa_msg_ctrl(wpa_s->parent, MSG_INFO, WPS_EVENT_OVERLAP);

	if (wpa_s->global->p2p)
		p2p_group_formation_failed(wpa_s->global->p2p);

	eloop_cancel_timeout(wpas_p2p_group_formation_timeout,
			     wpa_s->parent, NULL);

	wpas_group_formation_completed(wpa_s, 0);
	return 1;
}


void wpas_p2p_update_channel_list(struct wpa_supplicant *wpa_s)
{
	struct p2p_channels chan;

	if (wpa_s->global == NULL || wpa_s->global->p2p == NULL)
		return;

	os_memset(&chan, 0, sizeof(chan));
	if (wpas_p2p_setup_channels(wpa_s, &chan)) {
		wpa_printf(MSG_ERROR, "P2P: Failed to update supported "
			   "channel list");
		return;
	}

	p2p_update_channel_list(wpa_s->global->p2p, &chan);
}


static void wpas_p2p_scan_res_ignore(struct wpa_supplicant *wpa_s,
				     struct wpa_scan_results *scan_res)
{
	wpa_printf(MSG_DEBUG, "P2P: Ignore scan results");
}


int wpas_p2p_cancel(struct wpa_supplicant *wpa_s)
{
	struct wpa_global *global = wpa_s->global;
	int found = 0;
	const u8 *peer;

	if (global->p2p == NULL)
		return -1;

	wpa_printf(MSG_DEBUG, "P2P: Request to cancel group formation");

	if (wpa_s->pending_interface_name[0] &&
	    !is_zero_ether_addr(wpa_s->pending_interface_addr))
		found = 1;

	peer = p2p_get_go_neg_peer(global->p2p);
	if (peer) {
		wpa_printf(MSG_DEBUG, "P2P: Unauthorize pending GO Neg peer "
			   MACSTR, MAC2STR(peer));
		p2p_unauthorize(global->p2p, peer);
		found = 1;
	}

	if (wpa_s->scan_res_handler == wpas_p2p_scan_res_join) {
		wpa_printf(MSG_DEBUG, "P2P: Stop pending scan for join");
		wpa_s->scan_res_handler = wpas_p2p_scan_res_ignore;
		found = 1;
	}

	if (wpa_s->pending_pd_before_join) {
		wpa_printf(MSG_DEBUG, "P2P: Stop pending PD before join");
		wpa_s->pending_pd_before_join = 0;
		found = 1;
	}

	wpas_p2p_stop_find(wpa_s);

	for (wpa_s = global->ifaces; wpa_s; wpa_s = wpa_s->next) {
		if (wpa_s == global->p2p_group_formation &&
		    (wpa_s->p2p_in_provisioning ||
		     wpa_s->parent->pending_interface_type ==
		     WPA_IF_P2P_CLIENT)) {
			wpa_printf(MSG_DEBUG, "P2P: Interface %s in group "
				   "formation found - cancelling",
				   wpa_s->ifname);
			found = 1;
			eloop_cancel_timeout(wpas_p2p_group_formation_timeout,
					     wpa_s->parent, NULL);
			if (wpa_s->p2p_in_provisioning) {
				wpas_group_formation_completed(wpa_s, 0);
				break;
			}
			wpas_p2p_group_delete(wpa_s,
					      P2P_GROUP_REMOVAL_REQUESTED);
			break;
		}
	}

	if (!found) {
		wpa_printf(MSG_DEBUG, "P2P: No ongoing group formation found");
		return -1;
	}

	return 0;
}


void wpas_p2p_interface_unavailable(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->current_ssid == NULL || !wpa_s->current_ssid->p2p_group)
		return;

	wpa_printf(MSG_DEBUG, "P2P: Remove group due to driver resource not "
		   "being available anymore");
	wpas_p2p_group_delete(wpa_s, P2P_GROUP_REMOVAL_UNAVAILABLE);
}


void wpas_p2p_update_best_channels(struct wpa_supplicant *wpa_s,
				   int freq_24, int freq_5, int freq_overall)
{
	struct p2p_data *p2p = wpa_s->global->p2p;
	if (p2p == NULL || (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT))
		return;
	p2p_set_best_channels(p2p, freq_24, freq_5, freq_overall);
}


int wpas_p2p_unauthorize(struct wpa_supplicant *wpa_s, const char *addr)
{
	u8 peer[ETH_ALEN];
	struct p2p_data *p2p = wpa_s->global->p2p;

	if (p2p == NULL || (wpa_s->drv_flags & WPA_DRIVER_FLAGS_P2P_MGMT))
		return -1;

	if (hwaddr_aton(addr, peer))
		return -1;

	return p2p_unauthorize(p2p, peer);
}


/**
 * wpas_p2p_disconnect - Disconnect from a P2P Group
 * @wpa_s: Pointer to wpa_supplicant data
 * Returns: 0 on success, -1 on failure
 *
 * This can be used to disconnect from a group in which the local end is a P2P
 * Client or to end a P2P Group in case the local end is the Group Owner. If a
 * virtual network interface was created for this group, that interface will be
 * removed. Otherwise, only the configured P2P group network will be removed
 * from the interface.
 */
int wpas_p2p_disconnect(struct wpa_supplicant *wpa_s)
{

	if (wpa_s == NULL)
		return -1;

	return wpas_p2p_group_delete(wpa_s, P2P_GROUP_REMOVAL_REQUESTED) < 0 ?
		-1 : 0;
}


int wpas_p2p_in_progress(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->global->p2p_disabled || wpa_s->global->p2p == NULL)
		return 0;

	return p2p_in_progress(wpa_s->global->p2p);
}


void wpas_p2p_network_removed(struct wpa_supplicant *wpa_s,
			      struct wpa_ssid *ssid)
{
	if (wpa_s->p2p_in_provisioning && ssid->p2p_group &&
	    eloop_cancel_timeout(wpas_p2p_group_formation_timeout,
				 wpa_s->parent, NULL) > 0) {
		/**
		 * Remove the network by scheduling the group formation
		 * timeout to happen immediately. The teardown code
		 * needs to be scheduled to run asynch later so that we
		 * don't delete data from under ourselves unexpectedly.
		 * Calling wpas_p2p_group_formation_timeout directly
		 * causes a series of crashes in WPS failure scenarios.
		 */
		wpa_printf(MSG_DEBUG, "P2P: Canceled group formation due to "
			   "P2P group network getting removed");
		eloop_register_timeout(0, 0, wpas_p2p_group_formation_timeout,
				       wpa_s->parent, NULL);
	}
}


struct wpa_ssid * wpas_p2p_get_persistent(struct wpa_supplicant *wpa_s,
					  const u8 *addr, const u8 *ssid,
					  size_t ssid_len)
{
	struct wpa_ssid *s;
	size_t i;

	for (s = wpa_s->conf->ssid; s; s = s->next) {
		if (s->disabled != 2)
			continue;
		if (ssid &&
		    (ssid_len != s->ssid_len ||
		     os_memcmp(ssid, s->ssid, ssid_len) != 0))
			continue;
		if (os_memcmp(s->bssid, addr, ETH_ALEN) == 0)
			return s; /* peer is GO in the persistent group */
		if (s->mode != WPAS_MODE_P2P_GO || s->p2p_client_list == NULL)
			continue;
		for (i = 0; i < s->num_p2p_clients; i++) {
			if (os_memcmp(s->p2p_client_list + i * ETH_ALEN,
				      addr, ETH_ALEN) == 0)
				return s; /* peer is P2P client in persistent
					   * group */
		}
	}

	return NULL;
}


void wpas_p2p_notify_ap_sta_authorized(struct wpa_supplicant *wpa_s,
				       const u8 *addr)
{
	if (addr == NULL)
		return;
	wpas_p2p_add_persistent_group_client(wpa_s, addr);
}


static void wpas_p2p_fallback_to_go_neg(struct wpa_supplicant *wpa_s,
					int group_added)
{
	struct wpa_supplicant *group = wpa_s;
	if (wpa_s->global->p2p_group_formation)
		group = wpa_s->global->p2p_group_formation;
	wpa_s = wpa_s->parent;
	offchannel_send_action_done(wpa_s);
	if (group_added)
		wpas_p2p_group_delete(group, P2P_GROUP_REMOVAL_SILENT);
	wpa_dbg(wpa_s, MSG_DEBUG, "P2P: Fall back to GO Negotiation");
	wpas_p2p_connect(wpa_s, wpa_s->pending_join_dev_addr, wpa_s->p2p_pin,
			 wpa_s->p2p_wps_method, wpa_s->p2p_persistent_group, 0,
			 0, 0, wpa_s->p2p_go_intent, wpa_s->p2p_connect_freq,
			 wpa_s->p2p_persistent_id,
			 wpa_s->p2p_pd_before_go_neg,
			 wpa_s->p2p_go_ht40);
}


int wpas_p2p_scan_no_go_seen(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s->p2p_fallback_to_go_neg ||
	    wpa_s->p2p_in_provisioning <= 5)
		return 0;

	if (wpas_p2p_peer_go(wpa_s, wpa_s->pending_join_dev_addr) > 0)
		return 0; /* peer operating as a GO */

	wpa_dbg(wpa_s, MSG_DEBUG, "P2P: GO not found for p2p_connect-auto - "
		"fallback to GO Negotiation");
	wpas_p2p_fallback_to_go_neg(wpa_s, 1);

	return 1;
}


unsigned int wpas_p2p_search_delay(struct wpa_supplicant *wpa_s)
{
	const char *rn, *rn2;
	struct wpa_supplicant *ifs;

	if (wpa_s->wpa_state > WPA_SCANNING) {
		wpa_dbg(wpa_s, MSG_DEBUG, "P2P: Use %u ms search delay due to "
			"concurrent operation",
			P2P_CONCURRENT_SEARCH_DELAY);
		return P2P_CONCURRENT_SEARCH_DELAY;
	}

	if (!wpa_s->driver->get_radio_name)
		return 0;
	rn = wpa_s->driver->get_radio_name(wpa_s->drv_priv);
	if (rn == NULL || rn[0] == '\0')
		return 0;

	for (ifs = wpa_s->global->ifaces; ifs; ifs = ifs->next) {
		if (ifs == wpa_s || !ifs->driver->get_radio_name)
			continue;

		rn2 = ifs->driver->get_radio_name(ifs->drv_priv);
		if (!rn2 || os_strcmp(rn, rn2) != 0)
			continue;
		if (ifs->wpa_state > WPA_SCANNING) {
			wpa_dbg(wpa_s, MSG_DEBUG, "P2P: Use %u ms search "
				"delay due to concurrent operation on "
				"interface %s",
				P2P_CONCURRENT_SEARCH_DELAY, ifs->ifname);
			return P2P_CONCURRENT_SEARCH_DELAY;
		}
	}

	return 0;
}
