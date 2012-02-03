/*
 * hostapd - Driver operations
 * Copyright (c) 2009-2010, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "drivers/driver.h"
#include "common/ieee802_11_defs.h"
#include "hostapd.h"
#include "ieee802_11.h"
#include "sta_info.h"
#include "ap_config.h"
#include "ap_drv_ops.h"


static int hostapd_sta_flags_to_drv(int flags)
{
	int res = 0;
	if (flags & WLAN_STA_AUTHORIZED)
		res |= WPA_STA_AUTHORIZED;
	if (flags & WLAN_STA_WMM)
		res |= WPA_STA_WMM;
	if (flags & WLAN_STA_SHORT_PREAMBLE)
		res |= WPA_STA_SHORT_PREAMBLE;
	if (flags & WLAN_STA_MFP)
		res |= WPA_STA_MFP;
	return res;
}


static int hostapd_set_ap_wps_ie(struct hostapd_data *hapd)
{
	struct wpabuf *beacon, *proberesp;
	int ret;

	if (hapd->driver == NULL || hapd->driver->set_ap_wps_ie == NULL)
		return 0;

	beacon = hapd->wps_beacon_ie;
	proberesp = hapd->wps_probe_resp_ie;

	ret = hapd->driver->set_ap_wps_ie(hapd->drv_priv, beacon, proberesp);

	return ret;
}


static int hostapd_send_mgmt_frame(struct hostapd_data *hapd, const void *msg,
			   size_t len)
{
	if (hapd->driver == NULL || hapd->driver->send_mlme == NULL)
		return 0;
	return hapd->driver->send_mlme(hapd->drv_priv, msg, len);
}


static int hostapd_send_eapol(struct hostapd_data *hapd, const u8 *addr,
			      const u8 *data, size_t data_len, int encrypt)
{
	if (hapd->driver == NULL || hapd->driver->hapd_send_eapol == NULL)
		return 0;
	return hapd->driver->hapd_send_eapol(hapd->drv_priv, addr, data,
					     data_len, encrypt,
					     hapd->own_addr);
}


static int hostapd_set_authorized(struct hostapd_data *hapd,
				  struct sta_info *sta, int authorized)
{
	if (authorized) {
		return hostapd_sta_set_flags(hapd, sta->addr,
					     hostapd_sta_flags_to_drv(
						     sta->flags),
					     WPA_STA_AUTHORIZED, ~0);
	}

	return hostapd_sta_set_flags(hapd, sta->addr,
				     hostapd_sta_flags_to_drv(sta->flags),
				     0, ~WPA_STA_AUTHORIZED);
}


static int hostapd_set_key(const char *ifname, struct hostapd_data *hapd,
			   enum wpa_alg alg, const u8 *addr, int key_idx,
			   int set_tx, const u8 *seq, size_t seq_len,
			   const u8 *key, size_t key_len)
{
	if (hapd->driver == NULL || hapd->driver->set_key == NULL)
		return 0;
	return hapd->driver->set_key(ifname, hapd->drv_priv, alg, addr,
				     key_idx, set_tx, seq, seq_len, key,
				     key_len);
}


static int hostapd_read_sta_data(struct hostapd_data *hapd,
				 struct hostap_sta_driver_data *data,
				 const u8 *addr)
{
	if (hapd->driver == NULL || hapd->driver->read_sta_data == NULL)
		return -1;
	return hapd->driver->read_sta_data(hapd->drv_priv, data, addr);
}


static int hostapd_sta_clear_stats(struct hostapd_data *hapd, const u8 *addr)
{
	if (hapd->driver == NULL || hapd->driver->sta_clear_stats == NULL)
		return 0;
	return hapd->driver->sta_clear_stats(hapd->drv_priv, addr);
}


static int hostapd_set_sta_flags(struct hostapd_data *hapd,
				 struct sta_info *sta)
{
	int set_flags, total_flags, flags_and, flags_or;
	total_flags = hostapd_sta_flags_to_drv(sta->flags);
	set_flags = WPA_STA_SHORT_PREAMBLE | WPA_STA_WMM | WPA_STA_MFP;
	if (((!hapd->conf->ieee802_1x && !hapd->conf->wpa) ||
	     sta->auth_alg == WLAN_AUTH_FT) &&
	    sta->flags & WLAN_STA_AUTHORIZED)
		set_flags |= WPA_STA_AUTHORIZED;
	flags_or = total_flags & set_flags;
	flags_and = total_flags | ~set_flags;
	return hostapd_sta_set_flags(hapd, sta->addr, total_flags,
				     flags_or, flags_and);
}


static int hostapd_set_drv_ieee8021x(struct hostapd_data *hapd,
				     const char *ifname, int enabled)
{
	struct wpa_bss_params params;
	os_memset(&params, 0, sizeof(params));
	params.ifname = ifname;
	params.enabled = enabled;
	if (enabled) {
		params.wpa = hapd->conf->wpa;
		params.ieee802_1x = hapd->conf->ieee802_1x;
		params.wpa_group = hapd->conf->wpa_group;
		params.wpa_pairwise = hapd->conf->wpa_pairwise;
		params.wpa_key_mgmt = hapd->conf->wpa_key_mgmt;
		params.rsn_preauth = hapd->conf->rsn_preauth;
	}
	return hostapd_set_ieee8021x(hapd, &params);
}


static int hostapd_set_radius_acl_auth(struct hostapd_data *hapd,
				       const u8 *mac, int accepted,
				       u32 session_timeout)
{
	if (hapd->driver == NULL || hapd->driver->set_radius_acl_auth == NULL)
		return 0;
	return hapd->driver->set_radius_acl_auth(hapd->drv_priv, mac, accepted,
						 session_timeout);
}


static int hostapd_set_radius_acl_expire(struct hostapd_data *hapd,
					 const u8 *mac)
{
	if (hapd->driver == NULL ||
	    hapd->driver->set_radius_acl_expire == NULL)
		return 0;
	return hapd->driver->set_radius_acl_expire(hapd->drv_priv, mac);
}


static int hostapd_set_bss_params(struct hostapd_data *hapd,
				  int use_protection)
{
	int ret = 0;
	int preamble;
#ifdef CONFIG_IEEE80211N
	u8 buf[60], *ht_capab, *ht_oper, *pos;

	pos = buf;
	ht_capab = pos;
	pos = hostapd_eid_ht_capabilities(hapd, pos);
	ht_oper = pos;
	pos = hostapd_eid_ht_operation(hapd, pos);
	if (pos > ht_oper && ht_oper > ht_capab &&
	    hostapd_set_ht_params(hapd, ht_capab + 2, ht_capab[1],
				  ht_oper + 2, ht_oper[1])) {
		wpa_printf(MSG_ERROR, "Could not set HT capabilities "
			   "for kernel driver");
		ret = -1;
	}

#endif /* CONFIG_IEEE80211N */

	if (hostapd_set_cts_protect(hapd, use_protection)) {
		wpa_printf(MSG_ERROR, "Failed to set CTS protect in kernel "
			   "driver");
		ret = -1;
	}

	if (hapd->iface->current_mode &&
	    hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G &&
	    hostapd_set_short_slot_time(hapd,
					hapd->iface->num_sta_no_short_slot_time
					> 0 ? 0 : 1)) {
		wpa_printf(MSG_ERROR, "Failed to set Short Slot Time option "
			   "in kernel driver");
		ret = -1;
	}

	if (hapd->iface->num_sta_no_short_preamble == 0 &&
	    hapd->iconf->preamble == SHORT_PREAMBLE)
		preamble = SHORT_PREAMBLE;
	else
		preamble = LONG_PREAMBLE;
	if (hostapd_set_preamble(hapd, preamble)) {
		wpa_printf(MSG_ERROR, "Could not set preamble for kernel "
			   "driver");
		ret = -1;
	}

	return ret;
}


static int hostapd_set_beacon(struct hostapd_data *hapd,
			      const u8 *head, size_t head_len,
			      const u8 *tail, size_t tail_len, int dtim_period,
			      int beacon_int)
{
	if (hapd->driver == NULL || hapd->driver->set_beacon == NULL)
		return 0;
	return hapd->driver->set_beacon(hapd->drv_priv,
					head, head_len, tail, tail_len,
					dtim_period, beacon_int);
}


static int hostapd_vlan_if_add(struct hostapd_data *hapd, const char *ifname)
{
	char force_ifname[IFNAMSIZ];
	u8 if_addr[ETH_ALEN];
	return hostapd_if_add(hapd, WPA_IF_AP_VLAN, ifname, NULL, NULL, NULL,
			      force_ifname, if_addr);
}

static int hostapd_vlan_if_remove(struct hostapd_data *hapd,
				  const char *ifname)
{
	return hostapd_if_remove(hapd, WPA_IF_AP_VLAN, ifname);
}


static int hostapd_set_wds_sta(struct hostapd_data *hapd, const u8 *addr,
			       int aid, int val)
{
	if (hapd->driver == NULL || hapd->driver->set_wds_sta == NULL)
		return 0;
	return hapd->driver->set_wds_sta(hapd->drv_priv, addr, aid, val);
}


static int hostapd_set_sta_vlan(const char *ifname, struct hostapd_data *hapd,
				const u8 *addr, int vlan_id)
{
	if (hapd->driver == NULL || hapd->driver->set_sta_vlan == NULL)
		return 0;
	return hapd->driver->set_sta_vlan(hapd->drv_priv, addr, ifname,
					  vlan_id);
}


static int hostapd_get_inact_sec(struct hostapd_data *hapd, const u8 *addr)
{
	if (hapd->driver == NULL || hapd->driver->get_inact_sec == NULL)
		return 0;
	return hapd->driver->get_inact_sec(hapd->drv_priv, addr);
}


static int hostapd_sta_deauth(struct hostapd_data *hapd, const u8 *addr,
			      int reason)
{
	if (hapd->driver == NULL || hapd->driver->sta_deauth == NULL)
		return 0;
	return hapd->driver->sta_deauth(hapd->drv_priv, hapd->own_addr, addr,
					reason);
}


static int hostapd_sta_disassoc(struct hostapd_data *hapd, const u8 *addr,
				int reason)
{
	if (hapd->driver == NULL || hapd->driver->sta_disassoc == NULL)
		return 0;
	return hapd->driver->sta_disassoc(hapd->drv_priv, hapd->own_addr, addr,
					  reason);
}


static int hostapd_sta_add(struct hostapd_data *hapd,
			   const u8 *addr, u16 aid, u16 capability,
			   const u8 *supp_rates, size_t supp_rates_len,
			   u16 listen_interval,
			   const struct ieee80211_ht_capabilities *ht_capab)
{
	struct hostapd_sta_add_params params;

	if (hapd->driver == NULL)
		return 0;
	if (hapd->driver->sta_add == NULL)
		return 0;

	os_memset(&params, 0, sizeof(params));
	params.addr = addr;
	params.aid = aid;
	params.capability = capability;
	params.supp_rates = supp_rates;
	params.supp_rates_len = supp_rates_len;
	params.listen_interval = listen_interval;
	params.ht_capabilities = ht_capab;
	return hapd->driver->sta_add(hapd->drv_priv, &params);
}


static int hostapd_sta_remove(struct hostapd_data *hapd, const u8 *addr)
{
	if (hapd->driver == NULL || hapd->driver->sta_remove == NULL)
		return 0;
	return hapd->driver->sta_remove(hapd->drv_priv, addr);
}


static int hostapd_set_countermeasures(struct hostapd_data *hapd, int enabled)
{
	if (hapd->driver == NULL ||
	    hapd->driver->hapd_set_countermeasures == NULL)
		return 0;
	return hapd->driver->hapd_set_countermeasures(hapd->drv_priv, enabled);
}


void hostapd_set_driver_ops(struct hostapd_driver_ops *ops)
{
	ops->set_ap_wps_ie = hostapd_set_ap_wps_ie;
	ops->send_mgmt_frame = hostapd_send_mgmt_frame;
	ops->send_eapol = hostapd_send_eapol;
	ops->set_authorized = hostapd_set_authorized;
	ops->set_key = hostapd_set_key;
	ops->read_sta_data = hostapd_read_sta_data;
	ops->sta_clear_stats = hostapd_sta_clear_stats;
	ops->set_sta_flags = hostapd_set_sta_flags;
	ops->set_drv_ieee8021x = hostapd_set_drv_ieee8021x;
	ops->set_radius_acl_auth = hostapd_set_radius_acl_auth;
	ops->set_radius_acl_expire = hostapd_set_radius_acl_expire;
	ops->set_bss_params = hostapd_set_bss_params;
	ops->set_beacon = hostapd_set_beacon;
	ops->vlan_if_add = hostapd_vlan_if_add;
	ops->vlan_if_remove = hostapd_vlan_if_remove;
	ops->set_wds_sta = hostapd_set_wds_sta;
	ops->set_sta_vlan = hostapd_set_sta_vlan;
	ops->get_inact_sec = hostapd_get_inact_sec;
	ops->sta_deauth = hostapd_sta_deauth;
	ops->sta_disassoc = hostapd_sta_disassoc;
	ops->sta_add = hostapd_sta_add;
	ops->sta_remove = hostapd_sta_remove;
	ops->set_countermeasures = hostapd_set_countermeasures;
}


int hostapd_set_privacy(struct hostapd_data *hapd, int enabled)
{
	if (hapd->driver == NULL || hapd->driver->set_privacy == NULL)
		return 0;
	return hapd->driver->set_privacy(hapd->drv_priv, enabled);
}


int hostapd_set_generic_elem(struct hostapd_data *hapd, const u8 *elem,
			     size_t elem_len)
{
	if (hapd->driver == NULL || hapd->driver->set_generic_elem == NULL)
		return 0;
	return hapd->driver->set_generic_elem(hapd->drv_priv, elem, elem_len);
}


int hostapd_get_ssid(struct hostapd_data *hapd, u8 *buf, size_t len)
{
	if (hapd->driver == NULL || hapd->driver->hapd_get_ssid == NULL)
		return 0;
	return hapd->driver->hapd_get_ssid(hapd->drv_priv, buf, len);
}


int hostapd_set_ssid(struct hostapd_data *hapd, const u8 *buf, size_t len)
{
	if (hapd->driver == NULL || hapd->driver->hapd_set_ssid == NULL)
		return 0;
	return hapd->driver->hapd_set_ssid(hapd->drv_priv, buf, len);
}


int hostapd_if_add(struct hostapd_data *hapd, enum wpa_driver_if_type type,
		   const char *ifname, const u8 *addr, void *bss_ctx,
		   void **drv_priv, char *force_ifname, u8 *if_addr)
{
	if (hapd->driver == NULL || hapd->driver->if_add == NULL)
		return -1;
	return hapd->driver->if_add(hapd->drv_priv, type, ifname, addr,
				    bss_ctx, drv_priv, force_ifname, if_addr);
}


int hostapd_if_remove(struct hostapd_data *hapd, enum wpa_driver_if_type type,
		      const char *ifname)
{
	if (hapd->driver == NULL || hapd->driver->if_remove == NULL)
		return -1;
	return hapd->driver->if_remove(hapd->drv_priv, type, ifname);
}


int hostapd_set_ieee8021x(struct hostapd_data *hapd,
			  struct wpa_bss_params *params)
{
	if (hapd->driver == NULL || hapd->driver->set_ieee8021x == NULL)
		return 0;
	return hapd->driver->set_ieee8021x(hapd->drv_priv, params);
}


int hostapd_get_seqnum(const char *ifname, struct hostapd_data *hapd,
		       const u8 *addr, int idx, u8 *seq)
{
	if (hapd->driver == NULL || hapd->driver->get_seqnum == NULL)
		return 0;
	return hapd->driver->get_seqnum(ifname, hapd->drv_priv, addr, idx,
					seq);
}


int hostapd_flush(struct hostapd_data *hapd)
{
	if (hapd->driver == NULL || hapd->driver->flush == NULL)
		return 0;
	return hapd->driver->flush(hapd->drv_priv);
}


int hostapd_set_freq(struct hostapd_data *hapd, int mode, int freq,
		     int channel, int ht_enabled, int sec_channel_offset)
{
	struct hostapd_freq_params data;
	if (hapd->driver == NULL)
		return 0;
	if (hapd->driver->set_freq == NULL)
		return 0;
	os_memset(&data, 0, sizeof(data));
	data.mode = mode;
	data.freq = freq;
	data.channel = channel;
	data.ht_enabled = ht_enabled;
	data.sec_channel_offset = sec_channel_offset;
	return hapd->driver->set_freq(hapd->drv_priv, &data);
}

int hostapd_set_rts(struct hostapd_data *hapd, int rts)
{
	if (hapd->driver == NULL || hapd->driver->set_rts == NULL)
		return 0;
	return hapd->driver->set_rts(hapd->drv_priv, rts);
}


int hostapd_set_frag(struct hostapd_data *hapd, int frag)
{
	if (hapd->driver == NULL || hapd->driver->set_frag == NULL)
		return 0;
	return hapd->driver->set_frag(hapd->drv_priv, frag);
}


int hostapd_sta_set_flags(struct hostapd_data *hapd, u8 *addr,
			  int total_flags, int flags_or, int flags_and)
{
	if (hapd->driver == NULL || hapd->driver->sta_set_flags == NULL)
		return 0;
	return hapd->driver->sta_set_flags(hapd->drv_priv, addr, total_flags,
					   flags_or, flags_and);
}


int hostapd_set_rate_sets(struct hostapd_data *hapd, int *supp_rates,
			  int *basic_rates, int mode)
{
	if (hapd->driver == NULL || hapd->driver->set_rate_sets == NULL)
		return 0;
	return hapd->driver->set_rate_sets(hapd->drv_priv, supp_rates,
					   basic_rates, mode);
}


int hostapd_set_country(struct hostapd_data *hapd, const char *country)
{
	if (hapd->driver == NULL ||
	    hapd->driver->set_country == NULL)
		return 0;
	return hapd->driver->set_country(hapd->drv_priv, country);
}


int hostapd_set_cts_protect(struct hostapd_data *hapd, int value)
{
	if (hapd->driver == NULL || hapd->driver->set_cts_protect == NULL)
		return 0;
	return hapd->driver->set_cts_protect(hapd->drv_priv, value);
}


int hostapd_set_preamble(struct hostapd_data *hapd, int value)
{
	if (hapd->driver == NULL || hapd->driver->set_preamble == NULL)
		return 0;
	return hapd->driver->set_preamble(hapd->drv_priv, value);
}


int hostapd_set_short_slot_time(struct hostapd_data *hapd, int value)
{
	if (hapd->driver == NULL || hapd->driver->set_short_slot_time == NULL)
		return 0;
	return hapd->driver->set_short_slot_time(hapd->drv_priv, value);
}


int hostapd_set_tx_queue_params(struct hostapd_data *hapd, int queue, int aifs,
				int cw_min, int cw_max, int burst_time)
{
	if (hapd->driver == NULL || hapd->driver->set_tx_queue_params == NULL)
		return 0;
	return hapd->driver->set_tx_queue_params(hapd->drv_priv, queue, aifs,
						 cw_min, cw_max, burst_time);
}


int hostapd_valid_bss_mask(struct hostapd_data *hapd, const u8 *addr,
			   const u8 *mask)
{
	if (hapd->driver == NULL || hapd->driver->valid_bss_mask == NULL)
		return 1;
	return hapd->driver->valid_bss_mask(hapd->drv_priv, addr, mask);
}


struct hostapd_hw_modes *
hostapd_get_hw_feature_data(struct hostapd_data *hapd, u16 *num_modes,
			    u16 *flags)
{
	if (hapd->driver == NULL ||
	    hapd->driver->get_hw_feature_data == NULL)
		return NULL;
	return hapd->driver->get_hw_feature_data(hapd->drv_priv, num_modes,
						 flags);
}


int hostapd_driver_commit(struct hostapd_data *hapd)
{
	if (hapd->driver == NULL || hapd->driver->commit == NULL)
		return 0;
	return hapd->driver->commit(hapd->drv_priv);
}


int hostapd_set_ht_params(struct hostapd_data *hapd,
			  const u8 *ht_capab, size_t ht_capab_len,
			  const u8 *ht_oper, size_t ht_oper_len)
{
	if (hapd->driver == NULL || hapd->driver->set_ht_params == NULL ||
	    ht_capab == NULL || ht_oper == NULL)
		return 0;
	return hapd->driver->set_ht_params(hapd->drv_priv,
					   ht_capab, ht_capab_len,
					   ht_oper, ht_oper_len);
}


int hostapd_drv_none(struct hostapd_data *hapd)
{
	return hapd->driver && os_strcmp(hapd->driver->name, "none") == 0;
}


int hostapd_driver_scan(struct hostapd_data *hapd,
			struct wpa_driver_scan_params *params)
{
	if (hapd->driver && hapd->driver->scan2)
		return hapd->driver->scan2(hapd->drv_priv, params);
	return -1;
}


struct wpa_scan_results * hostapd_driver_get_scan_results(
	struct hostapd_data *hapd)
{
	if (hapd->driver && hapd->driver->get_scan_results2)
		return hapd->driver->get_scan_results2(hapd->drv_priv);
	return NULL;
}
