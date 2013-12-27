/*
 * wpa_supplicant - Internal driver interface wrappers
 * Copyright (c) 2003-2009, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef DRIVER_I_H
#define DRIVER_I_H

#include "drivers/driver.h"

/* driver_ops */
static inline void * wpa_drv_init(struct wpa_supplicant *wpa_s,
				  const char *ifname)
{
	if (wpa_s->driver->init2)
		return wpa_s->driver->init2(wpa_s, ifname,
					    wpa_s->global_drv_priv);
	if (wpa_s->driver->init) {
		return wpa_s->driver->init(wpa_s, ifname);
	}
	return NULL;
}

static inline void wpa_drv_deinit(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->driver->deinit)
		wpa_s->driver->deinit(wpa_s->drv_priv);
}

static inline int wpa_drv_set_param(struct wpa_supplicant *wpa_s,
				    const char *param)
{
	if (wpa_s->driver->set_param)
		return wpa_s->driver->set_param(wpa_s->drv_priv, param);
	return 0;
}

static inline int wpa_drv_set_countermeasures(struct wpa_supplicant *wpa_s,
					      int enabled)
{
	if (wpa_s->driver->set_countermeasures) {
		return wpa_s->driver->set_countermeasures(wpa_s->drv_priv,
							  enabled);
	}
	return -1;
}

static inline int wpa_drv_authenticate(struct wpa_supplicant *wpa_s,
				       struct wpa_driver_auth_params *params)
{
	if (wpa_s->driver->authenticate)
		return wpa_s->driver->authenticate(wpa_s->drv_priv, params);
	return -1;
}

static inline int wpa_drv_associate(struct wpa_supplicant *wpa_s,
				    struct wpa_driver_associate_params *params)
{
	if (wpa_s->driver->associate) {
		return wpa_s->driver->associate(wpa_s->drv_priv, params);
	}
	return -1;
}

static inline int wpa_drv_scan(struct wpa_supplicant *wpa_s,
			       struct wpa_driver_scan_params *params)
{
	if (wpa_s->driver->scan2)
		return wpa_s->driver->scan2(wpa_s->drv_priv, params);
	return -1;
}

static inline int wpa_drv_sched_scan(struct wpa_supplicant *wpa_s,
				     struct wpa_driver_scan_params *params,
				     u32 interval)
{
	if (wpa_s->driver->sched_scan)
		return wpa_s->driver->sched_scan(wpa_s->drv_priv,
						 params, interval);
	return -1;
}

static inline int wpa_drv_stop_sched_scan(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->driver->stop_sched_scan)
		return wpa_s->driver->stop_sched_scan(wpa_s->drv_priv);
	return -1;
}

static inline struct wpa_scan_results * wpa_drv_get_scan_results2(
	struct wpa_supplicant *wpa_s)
{
	if (wpa_s->driver->get_scan_results2)
		return wpa_s->driver->get_scan_results2(wpa_s->drv_priv);
	return NULL;
}

static inline int wpa_drv_get_bssid(struct wpa_supplicant *wpa_s, u8 *bssid)
{
	if (wpa_s->driver->get_bssid) {
		return wpa_s->driver->get_bssid(wpa_s->drv_priv, bssid);
	}
	return -1;
}

static inline int wpa_drv_get_ssid(struct wpa_supplicant *wpa_s, u8 *ssid)
{
	if (wpa_s->driver->get_ssid) {
		return wpa_s->driver->get_ssid(wpa_s->drv_priv, ssid);
	}
	return -1;
}

static inline int wpa_drv_set_key(struct wpa_supplicant *wpa_s,
				  enum wpa_alg alg, const u8 *addr,
				  int key_idx, int set_tx,
				   const u8 *seq, size_t seq_len,
				   const u8 *key, size_t key_len)
{
	if (wpa_s->driver->set_key) {
		wpa_s->keys_cleared = 0;
		return wpa_s->driver->set_key(wpa_s->ifname, wpa_s->drv_priv,
					      alg, addr, key_idx, set_tx,
					      seq, seq_len, key, key_len);
	}
	return -1;
}

static inline int wpa_drv_deauthenticate(struct wpa_supplicant *wpa_s,
					 const u8 *addr, int reason_code)
{
	if (wpa_s->driver->deauthenticate) {
		return wpa_s->driver->deauthenticate(wpa_s->drv_priv, addr,
						     reason_code);
	}
	return -1;
}

static inline int wpa_drv_add_pmkid(struct wpa_supplicant *wpa_s,
				    const u8 *bssid, const u8 *pmkid)
{
	if (wpa_s->driver->add_pmkid) {
		return wpa_s->driver->add_pmkid(wpa_s->drv_priv, bssid, pmkid);
	}
	return -1;
}

static inline int wpa_drv_remove_pmkid(struct wpa_supplicant *wpa_s,
				       const u8 *bssid, const u8 *pmkid)
{
	if (wpa_s->driver->remove_pmkid) {
		return wpa_s->driver->remove_pmkid(wpa_s->drv_priv, bssid,
						   pmkid);
	}
	return -1;
}

static inline int wpa_drv_flush_pmkid(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->driver->flush_pmkid) {
		return wpa_s->driver->flush_pmkid(wpa_s->drv_priv);
	}
	return -1;
}

static inline int wpa_drv_get_capa(struct wpa_supplicant *wpa_s,
				   struct wpa_driver_capa *capa)
{
	if (wpa_s->driver->get_capa) {
		return wpa_s->driver->get_capa(wpa_s->drv_priv, capa);
	}
	return -1;
}

static inline void wpa_drv_poll(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->driver->poll) {
		wpa_s->driver->poll(wpa_s->drv_priv);
	}
}

static inline const char * wpa_drv_get_ifname(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->driver->get_ifname) {
		return wpa_s->driver->get_ifname(wpa_s->drv_priv);
	}
	return NULL;
}

static inline const u8 * wpa_drv_get_mac_addr(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->driver->get_mac_addr) {
		return wpa_s->driver->get_mac_addr(wpa_s->drv_priv);
	}
	return NULL;
}

static inline int wpa_drv_send_eapol(struct wpa_supplicant *wpa_s,
				     const u8 *dst, u16 proto,
				     const u8 *data, size_t data_len)
{
	if (wpa_s->driver->send_eapol)
		return wpa_s->driver->send_eapol(wpa_s->drv_priv, dst, proto,
						 data, data_len);
	return -1;
}

static inline int wpa_drv_set_operstate(struct wpa_supplicant *wpa_s,
					int state)
{
	if (wpa_s->driver->set_operstate)
		return wpa_s->driver->set_operstate(wpa_s->drv_priv, state);
	return 0;
}

static inline int wpa_drv_mlme_setprotection(struct wpa_supplicant *wpa_s,
					     const u8 *addr, int protect_type,
					     int key_type)
{
	if (wpa_s->driver->mlme_setprotection)
		return wpa_s->driver->mlme_setprotection(wpa_s->drv_priv, addr,
							 protect_type,
							 key_type);
	return 0;
}

static inline struct hostapd_hw_modes *
wpa_drv_get_hw_feature_data(struct wpa_supplicant *wpa_s, u16 *num_modes,
			    u16 *flags)
{
	if (wpa_s->driver->get_hw_feature_data)
		return wpa_s->driver->get_hw_feature_data(wpa_s->drv_priv,
							  num_modes, flags);
	return NULL;
}

static inline int wpa_drv_set_country(struct wpa_supplicant *wpa_s,
				      const char *alpha2)
{
	if (wpa_s->driver->set_country)
		return wpa_s->driver->set_country(wpa_s->drv_priv, alpha2);
	return 0;
}

static inline int wpa_drv_send_mlme(struct wpa_supplicant *wpa_s,
				    const u8 *data, size_t data_len, int noack)
{
	if (wpa_s->driver->send_mlme)
		return wpa_s->driver->send_mlme(wpa_s->drv_priv,
						data, data_len, noack);
	return -1;
}

static inline int wpa_drv_update_ft_ies(struct wpa_supplicant *wpa_s,
					const u8 *md,
					const u8 *ies, size_t ies_len)
{
	if (wpa_s->driver->update_ft_ies)
		return wpa_s->driver->update_ft_ies(wpa_s->drv_priv, md,
						    ies, ies_len);
	return -1;
}

static inline int wpa_drv_send_ft_action(struct wpa_supplicant *wpa_s,
					 u8 action, const u8 *target_ap,
					 const u8 *ies, size_t ies_len)
{
	if (wpa_s->driver->send_ft_action)
		return wpa_s->driver->send_ft_action(wpa_s->drv_priv, action,
						     target_ap, ies, ies_len);
	return -1;
}

static inline int wpa_drv_set_ap(struct wpa_supplicant *wpa_s,
				 struct wpa_driver_ap_params *params)
{
	if (wpa_s->driver->set_ap)
		return wpa_s->driver->set_ap(wpa_s->drv_priv, params);
	return -1;
}

static inline int wpa_drv_sta_add(struct wpa_supplicant *wpa_s,
				  struct hostapd_sta_add_params *params)
{
	if (wpa_s->driver->sta_add)
		return wpa_s->driver->sta_add(wpa_s->drv_priv, params);
	return -1;
}

static inline int wpa_drv_sta_remove(struct wpa_supplicant *wpa_s,
				     const u8 *addr)
{
	if (wpa_s->driver->sta_remove)
		return wpa_s->driver->sta_remove(wpa_s->drv_priv, addr);
	return -1;
}

static inline int wpa_drv_hapd_send_eapol(struct wpa_supplicant *wpa_s,
					  const u8 *addr, const u8 *data,
					  size_t data_len, int encrypt,
					  const u8 *own_addr, u32 flags)
{
	if (wpa_s->driver->hapd_send_eapol)
		return wpa_s->driver->hapd_send_eapol(wpa_s->drv_priv, addr,
						      data, data_len, encrypt,
						      own_addr, flags);
	return -1;
}

static inline int wpa_drv_sta_set_flags(struct wpa_supplicant *wpa_s,
					const u8 *addr, int total_flags,
					int flags_or, int flags_and)
{
	if (wpa_s->driver->sta_set_flags)
		return wpa_s->driver->sta_set_flags(wpa_s->drv_priv, addr,
						    total_flags, flags_or,
						    flags_and);
	return -1;
}

static inline int wpa_drv_set_supp_port(struct wpa_supplicant *wpa_s,
					int authorized)
{
	if (wpa_s->driver->set_supp_port) {
		return wpa_s->driver->set_supp_port(wpa_s->drv_priv,
						    authorized);
	}
	return 0;
}

static inline int wpa_drv_send_action(struct wpa_supplicant *wpa_s,
				      unsigned int freq,
				      unsigned int wait,
				      const u8 *dst, const u8 *src,
				      const u8 *bssid,
				      const u8 *data, size_t data_len,
				      int no_cck)
{
	if (wpa_s->driver->send_action)
		return wpa_s->driver->send_action(wpa_s->drv_priv, freq,
						  wait, dst, src, bssid,
						  data, data_len, no_cck);
	return -1;
}

static inline void wpa_drv_send_action_cancel_wait(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->driver->send_action_cancel_wait)
		wpa_s->driver->send_action_cancel_wait(wpa_s->drv_priv);
}

static inline int wpa_drv_set_freq(struct wpa_supplicant *wpa_s,
				   struct hostapd_freq_params *freq)
{
	if (wpa_s->driver->set_freq)
		return wpa_s->driver->set_freq(wpa_s->drv_priv, freq);
	return -1;
}

static inline int wpa_drv_if_add(struct wpa_supplicant *wpa_s,
				 enum wpa_driver_if_type type,
				 const char *ifname, const u8 *addr,
				 void *bss_ctx, char *force_ifname,
				 u8 *if_addr, const char *bridge)
{
	if (wpa_s->driver->if_add)
		return wpa_s->driver->if_add(wpa_s->drv_priv, type, ifname,
					     addr, bss_ctx, NULL, force_ifname,
					     if_addr, bridge);
	return -1;
}

static inline int wpa_drv_if_remove(struct wpa_supplicant *wpa_s,
				    enum wpa_driver_if_type type,
				    const char *ifname)
{
	if (wpa_s->driver->if_remove)
		return wpa_s->driver->if_remove(wpa_s->drv_priv, type, ifname);
	return -1;
}

static inline int wpa_drv_remain_on_channel(struct wpa_supplicant *wpa_s,
					    unsigned int freq,
					    unsigned int duration)
{
	if (wpa_s->driver->remain_on_channel)
		return wpa_s->driver->remain_on_channel(wpa_s->drv_priv, freq,
							duration);
	return -1;
}

static inline int wpa_drv_cancel_remain_on_channel(
	struct wpa_supplicant *wpa_s)
{
	if (wpa_s->driver->cancel_remain_on_channel)
		return wpa_s->driver->cancel_remain_on_channel(
			wpa_s->drv_priv);
	return -1;
}

static inline int wpa_drv_probe_req_report(struct wpa_supplicant *wpa_s,
					   int report)
{
	if (wpa_s->driver->probe_req_report)
		return wpa_s->driver->probe_req_report(wpa_s->drv_priv,
						       report);
	return -1;
}

static inline int wpa_drv_deinit_ap(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->driver->deinit_ap)
		return wpa_s->driver->deinit_ap(wpa_s->drv_priv);
	return 0;
}

static inline int wpa_drv_deinit_p2p_cli(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->driver->deinit_p2p_cli)
		return wpa_s->driver->deinit_p2p_cli(wpa_s->drv_priv);
	return 0;
}

static inline void wpa_drv_suspend(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->driver->suspend)
		wpa_s->driver->suspend(wpa_s->drv_priv);
}

static inline void wpa_drv_resume(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->driver->resume)
		wpa_s->driver->resume(wpa_s->drv_priv);
}

static inline int wpa_drv_signal_monitor(struct wpa_supplicant *wpa_s,
					 int threshold, int hysteresis)
{
	if (wpa_s->driver->signal_monitor)
		return wpa_s->driver->signal_monitor(wpa_s->drv_priv,
						     threshold, hysteresis);
	return -1;
}

static inline int wpa_drv_signal_poll(struct wpa_supplicant *wpa_s,
				      struct wpa_signal_info *si)
{
	if (wpa_s->driver->signal_poll)
		return wpa_s->driver->signal_poll(wpa_s->drv_priv, si);
	return -1;
}

static inline int wpa_drv_pktcnt_poll(struct wpa_supplicant *wpa_s,
				      struct hostap_sta_driver_data *sta)
{
	if (wpa_s->driver->read_sta_data)
		return wpa_s->driver->read_sta_data(wpa_s->drv_priv, sta,
						    wpa_s->bssid);
	return -1;
}

static inline int wpa_drv_set_ap_wps_ie(struct wpa_supplicant *wpa_s,
					const struct wpabuf *beacon,
					const struct wpabuf *proberesp,
					const struct wpabuf *assocresp)
{
	if (!wpa_s->driver->set_ap_wps_ie)
		return -1;
	return wpa_s->driver->set_ap_wps_ie(wpa_s->drv_priv, beacon,
					    proberesp, assocresp);
}

static inline int wpa_drv_shared_freq(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s->driver->shared_freq)
		return -1;
	return wpa_s->driver->shared_freq(wpa_s->drv_priv);
}

static inline int wpa_drv_get_noa(struct wpa_supplicant *wpa_s,
				  u8 *buf, size_t buf_len)
{
	if (!wpa_s->driver->get_noa)
		return -1;
	return wpa_s->driver->get_noa(wpa_s->drv_priv, buf, buf_len);
}

static inline int wpa_drv_set_p2p_powersave(struct wpa_supplicant *wpa_s,
					    int legacy_ps, int opp_ps,
					    int ctwindow)
{
	if (!wpa_s->driver->set_p2p_powersave)
		return -1;
	return wpa_s->driver->set_p2p_powersave(wpa_s->drv_priv, legacy_ps,
						opp_ps, ctwindow);
}

static inline int wpa_drv_ampdu(struct wpa_supplicant *wpa_s, int ampdu)
{
	if (!wpa_s->driver->ampdu)
		return -1;
	return wpa_s->driver->ampdu(wpa_s->drv_priv, ampdu);
}

static inline int wpa_drv_p2p_find(struct wpa_supplicant *wpa_s,
				   unsigned int timeout, int type)
{
	if (!wpa_s->driver->p2p_find)
		return -1;
	return wpa_s->driver->p2p_find(wpa_s->drv_priv, timeout, type);
}

static inline int wpa_drv_p2p_stop_find(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s->driver->p2p_stop_find)
		return -1;
	return wpa_s->driver->p2p_stop_find(wpa_s->drv_priv);
}

static inline int wpa_drv_p2p_listen(struct wpa_supplicant *wpa_s,
				     unsigned int timeout)
{
	if (!wpa_s->driver->p2p_listen)
		return -1;
	return wpa_s->driver->p2p_listen(wpa_s->drv_priv, timeout);
}

static inline int wpa_drv_p2p_connect(struct wpa_supplicant *wpa_s,
				      const u8 *peer_addr, int wps_method,
				      int go_intent,
				      const u8 *own_interface_addr,
				      unsigned int force_freq,
				      int persistent_group)
{
	if (!wpa_s->driver->p2p_connect)
		return -1;
	return wpa_s->driver->p2p_connect(wpa_s->drv_priv, peer_addr,
					  wps_method, go_intent,
					  own_interface_addr, force_freq,
					  persistent_group);
}

static inline int wpa_drv_wps_success_cb(struct wpa_supplicant *wpa_s,
					 const u8 *peer_addr)
{
	if (!wpa_s->driver->wps_success_cb)
		return -1;
	return wpa_s->driver->wps_success_cb(wpa_s->drv_priv, peer_addr);
}

static inline int
wpa_drv_p2p_group_formation_failed(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s->driver->p2p_group_formation_failed)
		return -1;
	return wpa_s->driver->p2p_group_formation_failed(wpa_s->drv_priv);
}

static inline int wpa_drv_p2p_set_params(struct wpa_supplicant *wpa_s,
					 const struct p2p_params *params)
{
	if (!wpa_s->driver->p2p_set_params)
		return -1;
	return wpa_s->driver->p2p_set_params(wpa_s->drv_priv, params);
}

static inline int wpa_drv_p2p_prov_disc_req(struct wpa_supplicant *wpa_s,
					    const u8 *peer_addr,
					    u16 config_methods, int join)
{
	if (!wpa_s->driver->p2p_prov_disc_req)
		return -1;
	return wpa_s->driver->p2p_prov_disc_req(wpa_s->drv_priv, peer_addr,
						config_methods, join);
}

static inline u64 wpa_drv_p2p_sd_request(struct wpa_supplicant *wpa_s,
					 const u8 *dst,
					 const struct wpabuf *tlvs)
{
	if (!wpa_s->driver->p2p_sd_request)
		return 0;
	return wpa_s->driver->p2p_sd_request(wpa_s->drv_priv, dst, tlvs);
}

static inline int wpa_drv_p2p_sd_cancel_request(struct wpa_supplicant *wpa_s,
						u64 req)
{
	if (!wpa_s->driver->p2p_sd_cancel_request)
		return -1;
	return wpa_s->driver->p2p_sd_cancel_request(wpa_s->drv_priv, req);
}

static inline int wpa_drv_p2p_sd_response(struct wpa_supplicant *wpa_s,
					  int freq, const u8 *dst,
					  u8 dialog_token,
					  const struct wpabuf *resp_tlvs)
{
	if (!wpa_s->driver->p2p_sd_response)
		return -1;
	return wpa_s->driver->p2p_sd_response(wpa_s->drv_priv, freq, dst,
					      dialog_token, resp_tlvs);
}

static inline int wpa_drv_p2p_service_update(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s->driver->p2p_service_update)
		return -1;
	return wpa_s->driver->p2p_service_update(wpa_s->drv_priv);
}

static inline int wpa_drv_p2p_reject(struct wpa_supplicant *wpa_s,
				     const u8 *addr)
{
	if (!wpa_s->driver->p2p_reject)
		return -1;
	return wpa_s->driver->p2p_reject(wpa_s->drv_priv, addr);
}

static inline int wpa_drv_p2p_invite(struct wpa_supplicant *wpa_s,
				     const u8 *peer, int role, const u8 *bssid,
				     const u8 *ssid, size_t ssid_len,
				     const u8 *go_dev_addr,
				     int persistent_group)
{
	if (!wpa_s->driver->p2p_invite)
		return -1;
	return wpa_s->driver->p2p_invite(wpa_s->drv_priv, peer, role, bssid,
					 ssid, ssid_len, go_dev_addr,
					 persistent_group);
}

static inline int wpa_drv_send_tdls_mgmt(struct wpa_supplicant *wpa_s,
					 const u8 *dst, u8 action_code,
					 u8 dialog_token, u16 status_code,
					 const u8 *buf, size_t len)
{
	if (wpa_s->driver->send_tdls_mgmt) {
		return wpa_s->driver->send_tdls_mgmt(wpa_s->drv_priv, dst,
						     action_code, dialog_token,
						     status_code, buf, len);
	}
	return -1;
}

static inline int wpa_drv_tdls_oper(struct wpa_supplicant *wpa_s,
				    enum tdls_oper oper, const u8 *peer)
{
	if (!wpa_s->driver->tdls_oper)
		return -1;
	return wpa_s->driver->tdls_oper(wpa_s->drv_priv, oper, peer);
}

static inline void wpa_drv_set_rekey_info(struct wpa_supplicant *wpa_s,
					  const u8 *kek, const u8 *kck,
					  const u8 *replay_ctr)
{
	if (!wpa_s->driver->set_rekey_info)
		return;
	wpa_s->driver->set_rekey_info(wpa_s->drv_priv, kek, kck, replay_ctr);
}

static inline int wpa_drv_radio_disable(struct wpa_supplicant *wpa_s,
					int disabled)
{
	if (!wpa_s->driver->radio_disable)
		return -1;
	return wpa_s->driver->radio_disable(wpa_s->drv_priv, disabled);
}

static inline int wpa_drv_switch_channel(struct wpa_supplicant *wpa_s,
					 unsigned int freq)
{
	if (!wpa_s->driver->switch_channel)
		return -1;
	return wpa_s->driver->switch_channel(wpa_s->drv_priv, freq);
}

static inline int wpa_drv_wnm_oper(struct wpa_supplicant *wpa_s,
				   enum wnm_oper oper, const u8 *peer,
				   u8 *buf, u16 *buf_len)
{
	if (!wpa_s->driver->wnm_oper)
		return -1;
	return wpa_s->driver->wnm_oper(wpa_s->drv_priv, oper, peer, buf,
				       buf_len);
}

#endif /* DRIVER_I_H */
