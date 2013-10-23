/*
 * hostapd - Driver operations
 * Copyright (c) 2009, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef AP_DRV_OPS
#define AP_DRV_OPS

enum wpa_driver_if_type;
struct wpa_bss_params;
struct wpa_driver_scan_params;
struct ieee80211_ht_capabilities;

u32 hostapd_sta_flags_to_drv(u32 flags);
int hostapd_build_ap_extra_ies(struct hostapd_data *hapd,
			       struct wpabuf **beacon,
			       struct wpabuf **proberesp,
			       struct wpabuf **assocresp);
void hostapd_free_ap_extra_ies(struct hostapd_data *hapd, struct wpabuf *beacon,
			       struct wpabuf *proberesp,
			       struct wpabuf *assocresp);
int hostapd_set_ap_wps_ie(struct hostapd_data *hapd);
int hostapd_set_authorized(struct hostapd_data *hapd,
			   struct sta_info *sta, int authorized);
int hostapd_set_sta_flags(struct hostapd_data *hapd, struct sta_info *sta);
int hostapd_set_drv_ieee8021x(struct hostapd_data *hapd, const char *ifname,
			      int enabled);
int hostapd_vlan_if_add(struct hostapd_data *hapd, const char *ifname);
int hostapd_vlan_if_remove(struct hostapd_data *hapd, const char *ifname);
int hostapd_set_wds_sta(struct hostapd_data *hapd, const u8 *addr, int aid,
			int val);
int hostapd_sta_add(struct hostapd_data *hapd,
		    const u8 *addr, u16 aid, u16 capability,
		    const u8 *supp_rates, size_t supp_rates_len,
		    u16 listen_interval,
		    const struct ieee80211_ht_capabilities *ht_capab,
		    u32 flags, u8 qosinfo);
int hostapd_set_privacy(struct hostapd_data *hapd, int enabled);
int hostapd_set_generic_elem(struct hostapd_data *hapd, const u8 *elem,
			     size_t elem_len);
int hostapd_get_ssid(struct hostapd_data *hapd, u8 *buf, size_t len);
int hostapd_set_ssid(struct hostapd_data *hapd, const u8 *buf, size_t len);
int hostapd_if_add(struct hostapd_data *hapd, enum wpa_driver_if_type type,
		   const char *ifname, const u8 *addr, void *bss_ctx,
		   void **drv_priv, char *force_ifname, u8 *if_addr,
		   const char *bridge);
int hostapd_if_remove(struct hostapd_data *hapd, enum wpa_driver_if_type type,
		      const char *ifname);
int hostapd_set_ieee8021x(struct hostapd_data *hapd,
			  struct wpa_bss_params *params);
int hostapd_get_seqnum(const char *ifname, struct hostapd_data *hapd,
		       const u8 *addr, int idx, u8 *seq);
int hostapd_flush(struct hostapd_data *hapd);
int hostapd_set_freq(struct hostapd_data *hapd, int mode, int freq,
		     int channel, int ht_enabled, int sec_channel_offset);
int hostapd_set_rts(struct hostapd_data *hapd, int rts);
int hostapd_set_frag(struct hostapd_data *hapd, int frag);
int hostapd_sta_set_flags(struct hostapd_data *hapd, u8 *addr,
			  int total_flags, int flags_or, int flags_and);
int hostapd_set_country(struct hostapd_data *hapd, const char *country);
int hostapd_set_tx_queue_params(struct hostapd_data *hapd, int queue, int aifs,
				int cw_min, int cw_max, int burst_time);
struct hostapd_hw_modes *
hostapd_get_hw_feature_data(struct hostapd_data *hapd, u16 *num_modes,
			    u16 *flags);
int hostapd_driver_commit(struct hostapd_data *hapd);
int hostapd_drv_none(struct hostapd_data *hapd);
int hostapd_driver_scan(struct hostapd_data *hapd,
			struct wpa_driver_scan_params *params);
struct wpa_scan_results * hostapd_driver_get_scan_results(
	struct hostapd_data *hapd);
int hostapd_driver_set_noa(struct hostapd_data *hapd, u8 count, int start,
			   int duration);
int hostapd_drv_set_key(const char *ifname,
			struct hostapd_data *hapd,
			enum wpa_alg alg, const u8 *addr,
			int key_idx, int set_tx,
			const u8 *seq, size_t seq_len,
			const u8 *key, size_t key_len);
int hostapd_drv_send_mlme(struct hostapd_data *hapd,
			  const void *msg, size_t len, int noack);
int hostapd_drv_sta_deauth(struct hostapd_data *hapd,
			   const u8 *addr, int reason);
int hostapd_drv_sta_disassoc(struct hostapd_data *hapd,
			     const u8 *addr, int reason);
int hostapd_drv_send_action(struct hostapd_data *hapd, unsigned int freq,
			    unsigned int wait, const u8 *dst, const u8 *data,
			    size_t len);
int hostapd_add_sta_node(struct hostapd_data *hapd, const u8 *addr,
			 u16 auth_alg);
int hostapd_sta_auth(struct hostapd_data *hapd, const u8 *addr,
		     u16 seq, u16 status, const u8 *ie, size_t len);
int hostapd_sta_assoc(struct hostapd_data *hapd, const u8 *addr,
		      int reassoc, u16 status, const u8 *ie, size_t len);
int hostapd_add_tspec(struct hostapd_data *hapd, const u8 *addr,
		      u8 *tspec_ie, size_t tspec_ielen);


#include "drivers/driver.h"

int hostapd_drv_wnm_oper(struct hostapd_data *hapd,
			 enum wnm_oper oper, const u8 *peer,
			 u8 *buf, u16 *buf_len);

static inline int hostapd_drv_set_countermeasures(struct hostapd_data *hapd,
						  int enabled)
{
	if (hapd->driver == NULL ||
	    hapd->driver->hapd_set_countermeasures == NULL)
		return 0;
	return hapd->driver->hapd_set_countermeasures(hapd->drv_priv, enabled);
}

static inline int hostapd_drv_set_sta_vlan(const char *ifname,
					   struct hostapd_data *hapd,
					   const u8 *addr, int vlan_id)
{
	if (hapd->driver == NULL || hapd->driver->set_sta_vlan == NULL)
		return 0;
	return hapd->driver->set_sta_vlan(hapd->drv_priv, addr, ifname,
					  vlan_id);
}

static inline int hostapd_drv_get_inact_sec(struct hostapd_data *hapd,
					    const u8 *addr)
{
	if (hapd->driver == NULL || hapd->driver->get_inact_sec == NULL)
		return 0;
	return hapd->driver->get_inact_sec(hapd->drv_priv, addr);
}

static inline int hostapd_drv_sta_remove(struct hostapd_data *hapd,
					 const u8 *addr)
{
	if (hapd->driver == NULL || hapd->driver->sta_remove == NULL)
		return 0;
	return hapd->driver->sta_remove(hapd->drv_priv, addr);
}

static inline int hostapd_drv_hapd_send_eapol(struct hostapd_data *hapd,
					      const u8 *addr, const u8 *data,
					      size_t data_len, int encrypt,
					      u32 flags)
{
	if (hapd->driver == NULL || hapd->driver->hapd_send_eapol == NULL)
		return 0;
	return hapd->driver->hapd_send_eapol(hapd->drv_priv, addr, data,
					     data_len, encrypt,
					     hapd->own_addr, flags);
}

static inline int hostapd_drv_read_sta_data(
	struct hostapd_data *hapd, struct hostap_sta_driver_data *data,
	const u8 *addr)
{
	if (hapd->driver == NULL || hapd->driver->read_sta_data == NULL)
		return -1;
	return hapd->driver->read_sta_data(hapd->drv_priv, data, addr);
}

static inline int hostapd_drv_sta_clear_stats(struct hostapd_data *hapd,
					      const u8 *addr)
{
	if (hapd->driver == NULL || hapd->driver->sta_clear_stats == NULL)
		return 0;
	return hapd->driver->sta_clear_stats(hapd->drv_priv, addr);
}

static inline int hostapd_drv_set_ap(struct hostapd_data *hapd,
				     struct wpa_driver_ap_params *params)
{
	if (hapd->driver == NULL || hapd->driver->set_ap == NULL)
		return 0;
	return hapd->driver->set_ap(hapd->drv_priv, params);
}

static inline int hostapd_drv_set_radius_acl_auth(struct hostapd_data *hapd,
						  const u8 *mac, int accepted,
						  u32 session_timeout)
{
	if (hapd->driver == NULL || hapd->driver->set_radius_acl_auth == NULL)
		return 0;
	return hapd->driver->set_radius_acl_auth(hapd->drv_priv, mac, accepted,
						 session_timeout);
}

static inline int hostapd_drv_set_radius_acl_expire(struct hostapd_data *hapd,
						    const u8 *mac)
{
	if (hapd->driver == NULL ||
	    hapd->driver->set_radius_acl_expire == NULL)
		return 0;
	return hapd->driver->set_radius_acl_expire(hapd->drv_priv, mac);
}

static inline int hostapd_drv_set_authmode(struct hostapd_data *hapd,
					   int auth_algs)
{
	if (hapd->driver == NULL || hapd->driver->set_authmode == NULL)
		return 0;
	return hapd->driver->set_authmode(hapd->drv_priv, auth_algs);
}

static inline void hostapd_drv_poll_client(struct hostapd_data *hapd,
					   const u8 *own_addr, const u8 *addr,
					   int qos)
{
	if (hapd->driver == NULL || hapd->driver->poll_client == NULL)
		return;
	hapd->driver->poll_client(hapd->drv_priv, own_addr, addr, qos);
}

#endif /* AP_DRV_OPS */
