/*
 * wpa_supplicant - P2P
 * Copyright (c) 2009-2010, Atheros Communications
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef P2P_SUPPLICANT_H
#define P2P_SUPPLICANT_H

enum p2p_wps_method;
struct p2p_go_neg_results;
enum p2p_send_action_result;
struct p2p_peer_info;

int wpas_p2p_init(struct wpa_global *global, struct wpa_supplicant *wpa_s);
void wpas_p2p_deinit(struct wpa_supplicant *wpa_s);
void wpas_p2p_deinit_global(struct wpa_global *global);
int wpas_p2p_connect(struct wpa_supplicant *wpa_s, const u8 *peer_addr,
		     const char *pin, enum p2p_wps_method wps_method,
		     int persistent_group, int auto_join, int join,
		     int auth, int go_intent, int freq, int persistent_id,
		     int pd, int ht40);
void wpas_p2p_remain_on_channel_cb(struct wpa_supplicant *wpa_s,
				   unsigned int freq, unsigned int duration);
void wpas_p2p_cancel_remain_on_channel_cb(struct wpa_supplicant *wpa_s,
					  unsigned int freq);
int wpas_p2p_group_remove(struct wpa_supplicant *wpa_s, const char *ifname);
int wpas_p2p_group_add(struct wpa_supplicant *wpa_s, int persistent_group,
		       int freq, int ht40);
int wpas_p2p_group_add_persistent(struct wpa_supplicant *wpa_s,
				  struct wpa_ssid *ssid, int addr_allocated,
				  int freq, int ht40);
struct p2p_group * wpas_p2p_group_init(struct wpa_supplicant *wpa_s,
				       struct wpa_ssid *ssid);
void wpas_p2p_wps_success(struct wpa_supplicant *wpa_s, const u8 *peer_addr,
			  int registrar);
enum wpas_p2p_prov_disc_use {
	WPAS_P2P_PD_FOR_GO_NEG,
	WPAS_P2P_PD_FOR_JOIN,
	WPAS_P2P_PD_AUTO
};
int wpas_p2p_prov_disc(struct wpa_supplicant *wpa_s, const u8 *peer_addr,
		       const char *config_method,
		       enum wpas_p2p_prov_disc_use use);
void wpas_send_action_tx_status(struct wpa_supplicant *wpa_s, const u8 *dst,
				const u8 *data, size_t data_len,
				enum p2p_send_action_result result);
int wpas_p2p_scan_result_text(const u8 *ies, size_t ies_len, char *buf,
			      char *end);
enum p2p_discovery_type;
int wpas_p2p_find(struct wpa_supplicant *wpa_s, unsigned int timeout,
		  enum p2p_discovery_type type,
		  unsigned int num_req_dev_types, const u8 *req_dev_types,
		  const u8 *dev_id, unsigned int search_delay);
void wpas_p2p_stop_find(struct wpa_supplicant *wpa_s);
int wpas_p2p_listen(struct wpa_supplicant *wpa_s, unsigned int timeout);
int wpas_p2p_assoc_req_ie(struct wpa_supplicant *wpa_s, struct wpa_bss *bss,
			  u8 *buf, size_t len, int p2p_group);
int wpas_p2p_probe_req_rx(struct wpa_supplicant *wpa_s, const u8 *addr,
			  const u8 *dst, const u8 *bssid,
			  const u8 *ie, size_t ie_len,
			  int ssi_signal);
void wpas_p2p_rx_action(struct wpa_supplicant *wpa_s, const u8 *da,
			const u8 *sa, const u8 *bssid,
			u8 category, const u8 *data, size_t len, int freq);
void wpas_p2p_scan_ie(struct wpa_supplicant *wpa_s, struct wpabuf *ies);
void wpas_p2p_group_deinit(struct wpa_supplicant *wpa_s);
void wpas_dev_found(void *ctx, const u8 *addr,
		    const struct p2p_peer_info *info,
		    int new_device);
void wpas_go_neg_completed(void *ctx, struct p2p_go_neg_results *res);
void wpas_go_neg_req_rx(void *ctx, const u8 *src, u16 dev_passwd_id);
void wpas_prov_disc_req(void *ctx, const u8 *peer, u16 config_methods,
			const u8 *dev_addr, const u8 *pri_dev_type,
			const char *dev_name, u16 supp_config_methods,
			u8 dev_capab, u8 group_capab, const u8 *group_id,
			size_t group_id_len);
void wpas_prov_disc_resp(void *ctx, const u8 *peer, u16 config_methods);
void wpas_sd_request(void *ctx, int freq, const u8 *sa, u8 dialog_token,
		     u16 update_indic, const u8 *tlvs, size_t tlvs_len);
void wpas_sd_response(void *ctx, const u8 *sa, u16 update_indic,
		      const u8 *tlvs, size_t tlvs_len);
u64 wpas_p2p_sd_request(struct wpa_supplicant *wpa_s, const u8 *dst,
			const struct wpabuf *tlvs);
u64 wpas_p2p_sd_request_upnp(struct wpa_supplicant *wpa_s, const u8 *dst,
			     u8 version, const char *query);
u64 wpas_p2p_sd_request_wifi_display(struct wpa_supplicant *wpa_s,
				     const u8 *dst, const char *role);
int wpas_p2p_sd_cancel_request(struct wpa_supplicant *wpa_s, u64 req);
void wpas_p2p_sd_response(struct wpa_supplicant *wpa_s, int freq,
			  const u8 *dst, u8 dialog_token,
			  const struct wpabuf *resp_tlvs);
void wpas_p2p_sd_service_update(struct wpa_supplicant *wpa_s);
void wpas_p2p_service_flush(struct wpa_supplicant *wpa_s);
int wpas_p2p_service_add_bonjour(struct wpa_supplicant *wpa_s,
				 struct wpabuf *query, struct wpabuf *resp);
int wpas_p2p_service_del_bonjour(struct wpa_supplicant *wpa_s,
				 const struct wpabuf *query);
int wpas_p2p_service_add_upnp(struct wpa_supplicant *wpa_s, u8 version,
			      const char *service);
int wpas_p2p_service_del_upnp(struct wpa_supplicant *wpa_s, u8 version,
			      const char *service);
int wpas_p2p_reject(struct wpa_supplicant *wpa_s, const u8 *addr);
int wpas_p2p_invite(struct wpa_supplicant *wpa_s, const u8 *peer_addr,
		    struct wpa_ssid *ssid, const u8 *go_dev_addr, int freq,
		    int ht40);
int wpas_p2p_invite_group(struct wpa_supplicant *wpa_s, const char *ifname,
			  const u8 *peer_addr, const u8 *go_dev_addr);
void wpas_p2p_completed(struct wpa_supplicant *wpa_s);
int wpas_p2p_presence_req(struct wpa_supplicant *wpa_s, u32 duration1,
			  u32 interval1, u32 duration2, u32 interval2);
int wpas_p2p_ext_listen(struct wpa_supplicant *wpa_s, unsigned int period,
			unsigned int interval);
int wpas_p2p_deauth_notif(struct wpa_supplicant *wpa_s, const u8 *bssid,
			  u16 reason_code, const u8 *ie, size_t ie_len,
			  int locally_generated);
void wpas_p2p_disassoc_notif(struct wpa_supplicant *wpa_s, const u8 *bssid,
			     u16 reason_code, const u8 *ie, size_t ie_len,
			     int locally_generated);
void wpas_p2p_update_config(struct wpa_supplicant *wpa_s);
int wpas_p2p_set_noa(struct wpa_supplicant *wpa_s, u8 count, int start,
		     int duration);
int wpas_p2p_set_cross_connect(struct wpa_supplicant *wpa_s, int enabled);
void wpas_p2p_notif_connected(struct wpa_supplicant *wpa_s);
void wpas_p2p_notif_disconnected(struct wpa_supplicant *wpa_s);
int wpas_p2p_notif_pbc_overlap(struct wpa_supplicant *wpa_s);
void wpas_p2p_update_channel_list(struct wpa_supplicant *wpa_s);
int wpas_p2p_cancel(struct wpa_supplicant *wpa_s);
void wpas_p2p_interface_unavailable(struct wpa_supplicant *wpa_s);
void wpas_p2p_update_best_channels(struct wpa_supplicant *wpa_s,
				   int freq_24, int freq_5, int freq_overall);
int wpas_p2p_unauthorize(struct wpa_supplicant *wpa_s, const char *addr);
int wpas_p2p_disconnect(struct wpa_supplicant *wpa_s);
void wpas_p2p_wps_failed(struct wpa_supplicant *wpa_s,
			 struct wps_event_fail *fail);
int wpas_p2p_in_progress(struct wpa_supplicant *wpa_s);
void wpas_p2p_network_removed(struct wpa_supplicant *wpa_s,
			      struct wpa_ssid *ssid);
struct wpa_ssid * wpas_p2p_get_persistent(struct wpa_supplicant *wpa_s,
					  const u8 *addr, const u8 *ssid,
					  size_t ssid_len);
void wpas_p2p_notify_ap_sta_authorized(struct wpa_supplicant *wpa_s,
				       const u8 *addr);
int wpas_p2p_scan_no_go_seen(struct wpa_supplicant *wpa_s);
int wpas_p2p_get_ht40_mode(struct wpa_supplicant *wpa_s,
			   struct hostapd_hw_modes *mode, u8 channel);
unsigned int wpas_p2p_search_delay(struct wpa_supplicant *wpa_s);

#endif /* P2P_SUPPLICANT_H */
