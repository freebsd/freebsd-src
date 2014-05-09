/*
 * wpa_supplicant - Event notifications
 * Copyright (c) 2009-2010, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/wpa_ctrl.h"
#include "config.h"
#include "wpa_supplicant_i.h"
#include "wps_supplicant.h"
#include "dbus/dbus_common.h"
#include "dbus/dbus_old.h"
#include "dbus/dbus_new.h"
#include "rsn_supp/wpa.h"
#include "driver_i.h"
#include "scan.h"
#include "p2p_supplicant.h"
#include "sme.h"
#include "notify.h"

int wpas_notify_supplicant_initialized(struct wpa_global *global)
{
#ifdef CONFIG_DBUS
	if (global->params.dbus_ctrl_interface) {
		global->dbus = wpas_dbus_init(global);
		if (global->dbus == NULL)
			return -1;
	}
#endif /* CONFIG_DBUS */

	return 0;
}


void wpas_notify_supplicant_deinitialized(struct wpa_global *global)
{
#ifdef CONFIG_DBUS
	if (global->dbus)
		wpas_dbus_deinit(global->dbus);
#endif /* CONFIG_DBUS */
}


int wpas_notify_iface_added(struct wpa_supplicant *wpa_s)
{
	if (wpas_dbus_register_iface(wpa_s))
		return -1;

	if (wpas_dbus_register_interface(wpa_s))
		return -1;

	return 0;
}


void wpas_notify_iface_removed(struct wpa_supplicant *wpa_s)
{
	/* unregister interface in old DBus ctrl iface */
	wpas_dbus_unregister_iface(wpa_s);

	/* unregister interface in new DBus ctrl iface */
	wpas_dbus_unregister_interface(wpa_s);
}


void wpas_notify_state_changed(struct wpa_supplicant *wpa_s,
			       enum wpa_states new_state,
			       enum wpa_states old_state)
{
	/* notify the old DBus API */
	wpa_supplicant_dbus_notify_state_change(wpa_s, new_state,
						old_state);

	/* notify the new DBus API */
	wpas_dbus_signal_prop_changed(wpa_s, WPAS_DBUS_PROP_STATE);

#ifdef CONFIG_P2P
	if (new_state == WPA_COMPLETED)
		wpas_p2p_notif_connected(wpa_s);
	else if (old_state >= WPA_ASSOCIATED && new_state < WPA_ASSOCIATED)
		wpas_p2p_notif_disconnected(wpa_s);
#endif /* CONFIG_P2P */

	sme_state_changed(wpa_s);

#ifdef ANDROID
	wpa_msg_ctrl(wpa_s, MSG_INFO, WPA_EVENT_STATE_CHANGE
		     "id=%d state=%d BSSID=" MACSTR,
		     wpa_s->current_ssid ? wpa_s->current_ssid->id : -1,
		     new_state, MAC2STR(wpa_s->pending_bssid));
#endif /* ANDROID */
}


void wpas_notify_disconnect_reason(struct wpa_supplicant *wpa_s)
{
	wpas_dbus_signal_prop_changed(wpa_s, WPAS_DBUS_PROP_DISCONNECT_REASON);
}


void wpas_notify_network_changed(struct wpa_supplicant *wpa_s)
{
	wpas_dbus_signal_prop_changed(wpa_s, WPAS_DBUS_PROP_CURRENT_NETWORK);
}


void wpas_notify_ap_scan_changed(struct wpa_supplicant *wpa_s)
{
	wpas_dbus_signal_prop_changed(wpa_s, WPAS_DBUS_PROP_AP_SCAN);
}


void wpas_notify_bssid_changed(struct wpa_supplicant *wpa_s)
{
	wpas_dbus_signal_prop_changed(wpa_s, WPAS_DBUS_PROP_CURRENT_BSS);
}


void wpas_notify_auth_changed(struct wpa_supplicant *wpa_s)
{
	wpas_dbus_signal_prop_changed(wpa_s, WPAS_DBUS_PROP_CURRENT_AUTH_MODE);
}


void wpas_notify_network_enabled_changed(struct wpa_supplicant *wpa_s,
					 struct wpa_ssid *ssid)
{
	wpas_dbus_signal_network_enabled_changed(wpa_s, ssid);
}


void wpas_notify_network_selected(struct wpa_supplicant *wpa_s,
				  struct wpa_ssid *ssid)
{
	wpas_dbus_signal_network_selected(wpa_s, ssid->id);
}


void wpas_notify_network_request(struct wpa_supplicant *wpa_s,
				 struct wpa_ssid *ssid,
				 enum wpa_ctrl_req_type rtype,
				 const char *default_txt)
{
	wpas_dbus_signal_network_request(wpa_s, ssid, rtype, default_txt);
}


void wpas_notify_scanning(struct wpa_supplicant *wpa_s)
{
	/* notify the old DBus API */
	wpa_supplicant_dbus_notify_scanning(wpa_s);

	/* notify the new DBus API */
	wpas_dbus_signal_prop_changed(wpa_s, WPAS_DBUS_PROP_SCANNING);
}


void wpas_notify_scan_done(struct wpa_supplicant *wpa_s, int success)
{
	wpas_dbus_signal_scan_done(wpa_s, success);
}


void wpas_notify_scan_results(struct wpa_supplicant *wpa_s)
{
	/* notify the old DBus API */
	wpa_supplicant_dbus_notify_scan_results(wpa_s);

	wpas_wps_notify_scan_results(wpa_s);
}


void wpas_notify_wps_credential(struct wpa_supplicant *wpa_s,
				const struct wps_credential *cred)
{
#ifdef CONFIG_WPS
	/* notify the old DBus API */
	wpa_supplicant_dbus_notify_wps_cred(wpa_s, cred);
	/* notify the new DBus API */
	wpas_dbus_signal_wps_cred(wpa_s, cred);
#endif /* CONFIG_WPS */
}


void wpas_notify_wps_event_m2d(struct wpa_supplicant *wpa_s,
			       struct wps_event_m2d *m2d)
{
#ifdef CONFIG_WPS
	wpas_dbus_signal_wps_event_m2d(wpa_s, m2d);
#endif /* CONFIG_WPS */
}


void wpas_notify_wps_event_fail(struct wpa_supplicant *wpa_s,
				struct wps_event_fail *fail)
{
#ifdef CONFIG_WPS
	wpas_dbus_signal_wps_event_fail(wpa_s, fail);
#endif /* CONFIG_WPS */
}


void wpas_notify_wps_event_success(struct wpa_supplicant *wpa_s)
{
#ifdef CONFIG_WPS
	wpas_dbus_signal_wps_event_success(wpa_s);
#endif /* CONFIG_WPS */
}


void wpas_notify_network_added(struct wpa_supplicant *wpa_s,
			       struct wpa_ssid *ssid)
{
	/*
	 * Networks objects created during any P2P activities should not be
	 * exposed out. They might/will confuse certain non-P2P aware
	 * applications since these network objects won't behave like
	 * regular ones.
	 */
	if (wpa_s->global->p2p_group_formation != wpa_s)
		wpas_dbus_register_network(wpa_s, ssid);
}


void wpas_notify_persistent_group_added(struct wpa_supplicant *wpa_s,
					struct wpa_ssid *ssid)
{
#ifdef CONFIG_P2P
	wpas_dbus_register_persistent_group(wpa_s, ssid);
#endif /* CONFIG_P2P */
}


void wpas_notify_persistent_group_removed(struct wpa_supplicant *wpa_s,
					  struct wpa_ssid *ssid)
{
#ifdef CONFIG_P2P
	wpas_dbus_unregister_persistent_group(wpa_s, ssid->id);
#endif /* CONFIG_P2P */
}


void wpas_notify_network_removed(struct wpa_supplicant *wpa_s,
				 struct wpa_ssid *ssid)
{
	if (wpa_s->wpa)
		wpa_sm_pmksa_cache_flush(wpa_s->wpa, ssid);
	if (wpa_s->global->p2p_group_formation != wpa_s)
		wpas_dbus_unregister_network(wpa_s, ssid->id);
#ifdef CONFIG_P2P
	wpas_p2p_network_removed(wpa_s, ssid);
#endif /* CONFIG_P2P */
}


void wpas_notify_bss_added(struct wpa_supplicant *wpa_s,
			   u8 bssid[], unsigned int id)
{
	wpas_dbus_register_bss(wpa_s, bssid, id);
	wpa_msg_ctrl(wpa_s, MSG_INFO, WPA_EVENT_BSS_ADDED "%u " MACSTR,
		     id, MAC2STR(bssid));
}


void wpas_notify_bss_removed(struct wpa_supplicant *wpa_s,
			     u8 bssid[], unsigned int id)
{
	wpas_dbus_unregister_bss(wpa_s, bssid, id);
	wpa_msg_ctrl(wpa_s, MSG_INFO, WPA_EVENT_BSS_REMOVED "%u " MACSTR,
		     id, MAC2STR(bssid));
}


void wpas_notify_bss_freq_changed(struct wpa_supplicant *wpa_s,
				  unsigned int id)
{
	wpas_dbus_bss_signal_prop_changed(wpa_s, WPAS_DBUS_BSS_PROP_FREQ, id);
}


void wpas_notify_bss_signal_changed(struct wpa_supplicant *wpa_s,
				    unsigned int id)
{
	wpas_dbus_bss_signal_prop_changed(wpa_s, WPAS_DBUS_BSS_PROP_SIGNAL,
					  id);
}


void wpas_notify_bss_privacy_changed(struct wpa_supplicant *wpa_s,
				     unsigned int id)
{
	wpas_dbus_bss_signal_prop_changed(wpa_s, WPAS_DBUS_BSS_PROP_PRIVACY,
					  id);
}


void wpas_notify_bss_mode_changed(struct wpa_supplicant *wpa_s,
				  unsigned int id)
{
	wpas_dbus_bss_signal_prop_changed(wpa_s, WPAS_DBUS_BSS_PROP_MODE, id);
}


void wpas_notify_bss_wpaie_changed(struct wpa_supplicant *wpa_s,
				   unsigned int id)
{
	wpas_dbus_bss_signal_prop_changed(wpa_s, WPAS_DBUS_BSS_PROP_WPA, id);
}


void wpas_notify_bss_rsnie_changed(struct wpa_supplicant *wpa_s,
				   unsigned int id)
{
	wpas_dbus_bss_signal_prop_changed(wpa_s, WPAS_DBUS_BSS_PROP_RSN, id);
}


void wpas_notify_bss_wps_changed(struct wpa_supplicant *wpa_s,
				 unsigned int id)
{
#ifdef CONFIG_WPS
	wpas_dbus_bss_signal_prop_changed(wpa_s, WPAS_DBUS_BSS_PROP_WPS, id);
#endif /* CONFIG_WPS */
}


void wpas_notify_bss_ies_changed(struct wpa_supplicant *wpa_s,
				   unsigned int id)
{
	wpas_dbus_bss_signal_prop_changed(wpa_s, WPAS_DBUS_BSS_PROP_IES, id);
}


void wpas_notify_bss_rates_changed(struct wpa_supplicant *wpa_s,
				   unsigned int id)
{
	wpas_dbus_bss_signal_prop_changed(wpa_s, WPAS_DBUS_BSS_PROP_RATES, id);
}


void wpas_notify_blob_added(struct wpa_supplicant *wpa_s, const char *name)
{
	wpas_dbus_signal_blob_added(wpa_s, name);
}


void wpas_notify_blob_removed(struct wpa_supplicant *wpa_s, const char *name)
{
	wpas_dbus_signal_blob_removed(wpa_s, name);
}


void wpas_notify_debug_level_changed(struct wpa_global *global)
{
	wpas_dbus_signal_debug_level_changed(global);
}


void wpas_notify_debug_timestamp_changed(struct wpa_global *global)
{
	wpas_dbus_signal_debug_timestamp_changed(global);
}


void wpas_notify_debug_show_keys_changed(struct wpa_global *global)
{
	wpas_dbus_signal_debug_show_keys_changed(global);
}


void wpas_notify_suspend(struct wpa_global *global)
{
	struct wpa_supplicant *wpa_s;

	os_get_time(&global->suspend_time);
	wpa_printf(MSG_DEBUG, "System suspend notification");
	for (wpa_s = global->ifaces; wpa_s; wpa_s = wpa_s->next)
		wpa_drv_suspend(wpa_s);
}


void wpas_notify_resume(struct wpa_global *global)
{
	struct os_time now;
	int slept;
	struct wpa_supplicant *wpa_s;

	if (global->suspend_time.sec == 0)
		slept = -1;
	else {
		os_get_time(&now);
		slept = now.sec - global->suspend_time.sec;
	}
	wpa_printf(MSG_DEBUG, "System resume notification (slept %d seconds)",
		   slept);

	for (wpa_s = global->ifaces; wpa_s; wpa_s = wpa_s->next) {
		wpa_drv_resume(wpa_s);
		if (wpa_s->wpa_state == WPA_DISCONNECTED)
			wpa_supplicant_req_scan(wpa_s, 0, 100000);
	}
}


#ifdef CONFIG_P2P

void wpas_notify_p2p_device_found(struct wpa_supplicant *wpa_s,
				  const u8 *dev_addr, int new_device)
{
	if (new_device) {
		/* Create the new peer object */
		wpas_dbus_register_peer(wpa_s, dev_addr);
	}

	/* Notify a new peer has been detected*/
	wpas_dbus_signal_peer_device_found(wpa_s, dev_addr);
}


void wpas_notify_p2p_device_lost(struct wpa_supplicant *wpa_s,
				 const u8 *dev_addr)
{
	wpas_dbus_unregister_peer(wpa_s, dev_addr);

	/* Create signal on interface object*/
	wpas_dbus_signal_peer_device_lost(wpa_s, dev_addr);
}


void wpas_notify_p2p_group_removed(struct wpa_supplicant *wpa_s,
				   const struct wpa_ssid *ssid,
				   const char *role)
{
	wpas_dbus_unregister_p2p_group(wpa_s, ssid);

	wpas_dbus_signal_p2p_group_removed(wpa_s, role);
}


void wpas_notify_p2p_go_neg_req(struct wpa_supplicant *wpa_s,
				const u8 *src, u16 dev_passwd_id)
{
	wpas_dbus_signal_p2p_go_neg_req(wpa_s, src, dev_passwd_id);
}


void wpas_notify_p2p_go_neg_completed(struct wpa_supplicant *wpa_s,
				      struct p2p_go_neg_results *res)
{
	wpas_dbus_signal_p2p_go_neg_resp(wpa_s, res);
}


void wpas_notify_p2p_invitation_result(struct wpa_supplicant *wpa_s,
				       int status, const u8 *bssid)
{
	wpas_dbus_signal_p2p_invitation_result(wpa_s, status, bssid);
}


void wpas_notify_p2p_sd_request(struct wpa_supplicant *wpa_s,
				int freq, const u8 *sa, u8 dialog_token,
				u16 update_indic, const u8 *tlvs,
				size_t tlvs_len)
{
	wpas_dbus_signal_p2p_sd_request(wpa_s, freq, sa, dialog_token,
					update_indic, tlvs, tlvs_len);
}


void wpas_notify_p2p_sd_response(struct wpa_supplicant *wpa_s,
				 const u8 *sa, u16 update_indic,
				 const u8 *tlvs, size_t tlvs_len)
{
	wpas_dbus_signal_p2p_sd_response(wpa_s, sa, update_indic,
					 tlvs, tlvs_len);
}


/**
 * wpas_notify_p2p_provision_discovery - Notification of provision discovery
 * @dev_addr: Who sent the request or responded to our request.
 * @request: Will be 1 if request, 0 for response.
 * @status: Valid only in case of response (0 in case of success)
 * @config_methods: WPS config methods
 * @generated_pin: PIN to be displayed in case of WPS_CONFIG_DISPLAY method
 *
 * This can be used to notify:
 * - Requests or responses
 * - Various config methods
 * - Failure condition in case of response
 */
void wpas_notify_p2p_provision_discovery(struct wpa_supplicant *wpa_s,
					 const u8 *dev_addr, int request,
					 enum p2p_prov_disc_status status,
					 u16 config_methods,
					 unsigned int generated_pin)
{
	wpas_dbus_signal_p2p_provision_discovery(wpa_s, dev_addr, request,
						 status, config_methods,
						 generated_pin);
}


void wpas_notify_p2p_group_started(struct wpa_supplicant *wpa_s,
				   struct wpa_ssid *ssid, int network_id,
				   int client)
{
	/* Notify a group has been started */
	wpas_dbus_register_p2p_group(wpa_s, ssid);

	wpas_dbus_signal_p2p_group_started(wpa_s, ssid, client, network_id);
}


void wpas_notify_p2p_wps_failed(struct wpa_supplicant *wpa_s,
				struct wps_event_fail *fail)
{
	wpas_dbus_signal_p2p_wps_failed(wpa_s, fail);
}

#endif /* CONFIG_P2P */


static void wpas_notify_ap_sta_authorized(struct wpa_supplicant *wpa_s,
					  const u8 *sta,
					  const u8 *p2p_dev_addr)
{
#ifdef CONFIG_P2P
	wpas_p2p_notify_ap_sta_authorized(wpa_s, p2p_dev_addr);

	/*
	 * Register a group member object corresponding to this peer and
	 * emit a PeerJoined signal. This will check if it really is a
	 * P2P group.
	 */
	wpas_dbus_register_p2p_groupmember(wpa_s, sta);

	/*
	 * Create 'peer-joined' signal on group object -- will also
	 * check P2P itself.
	 */
	wpas_dbus_signal_p2p_peer_joined(wpa_s, sta);
#endif /* CONFIG_P2P */
}


static void wpas_notify_ap_sta_deauthorized(struct wpa_supplicant *wpa_s,
					    const u8 *sta)
{
#ifdef CONFIG_P2P
	/*
	 * Unregister a group member object corresponding to this peer
	 * if this is a P2P group.
	 */
	wpas_dbus_unregister_p2p_groupmember(wpa_s, sta);

	/*
	 * Create 'peer-disconnected' signal on group object if this
	 * is a P2P group.
	 */
	wpas_dbus_signal_p2p_peer_disconnected(wpa_s, sta);
#endif /* CONFIG_P2P */
}


void wpas_notify_sta_authorized(struct wpa_supplicant *wpa_s,
				const u8 *mac_addr, int authorized,
				const u8 *p2p_dev_addr)
{
	if (authorized)
		wpas_notify_ap_sta_authorized(wpa_s, mac_addr, p2p_dev_addr);
	else
		wpas_notify_ap_sta_deauthorized(wpa_s, mac_addr);
}


void wpas_notify_certification(struct wpa_supplicant *wpa_s, int depth,
			       const char *subject, const char *cert_hash,
			       const struct wpabuf *cert)
{
	wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_EAP_PEER_CERT
		"depth=%d subject='%s'%s%s",
		depth, subject,
		cert_hash ? " hash=" : "",
		cert_hash ? cert_hash : "");

	if (cert) {
		char *cert_hex;
		size_t len = wpabuf_len(cert) * 2 + 1;
		cert_hex = os_malloc(len);
		if (cert_hex) {
			wpa_snprintf_hex(cert_hex, len, wpabuf_head(cert),
					 wpabuf_len(cert));
			wpa_msg_ctrl(wpa_s, MSG_INFO,
				     WPA_EVENT_EAP_PEER_CERT
				     "depth=%d subject='%s' cert=%s",
				     depth, subject, cert_hex);
			os_free(cert_hex);
		}
	}

	/* notify the old DBus API */
	wpa_supplicant_dbus_notify_certification(wpa_s, depth, subject,
						 cert_hash, cert);
	/* notify the new DBus API */
	wpas_dbus_signal_certification(wpa_s, depth, subject, cert_hash, cert);
}


void wpas_notify_preq(struct wpa_supplicant *wpa_s,
		      const u8 *addr, const u8 *dst, const u8 *bssid,
		      const u8 *ie, size_t ie_len, u32 ssi_signal)
{
#ifdef CONFIG_AP
	wpas_dbus_signal_preq(wpa_s, addr, dst, bssid, ie, ie_len, ssi_signal);
#endif /* CONFIG_AP */
}


void wpas_notify_eap_status(struct wpa_supplicant *wpa_s, const char *status,
			    const char *parameter)
{
	wpas_dbus_signal_eap_status(wpa_s, status, parameter);
}
