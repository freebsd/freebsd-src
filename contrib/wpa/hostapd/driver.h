/*
 * hostapd - driver interface definition
 * Copyright (c) 2002-2007, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2007-2008, Intel Corporation
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

#ifndef DRIVER_H
#define DRIVER_H

struct hostapd_sta_add_params {
	const u8 *addr;
	u16 aid;
	u16 capability;
	const u8 *supp_rates;
	size_t supp_rates_len;
	int flags;
	u16 listen_interval;
	const struct ht_cap_ie *ht_capabilities;
};

struct hostapd_freq_params {
	int mode;
	int freq;
	int ht_enabled;
	int sec_channel_offset; /* 0 = HT40 disabled, -1 = HT40 enabled,
				 * secondary channel below primary, 1 = HT40
				 * enabled, secondary channel above primary */
};

enum hostapd_driver_if_type {
	HOSTAPD_IF_VLAN, HOSTAPD_IF_WDS
};

struct wpa_driver_ops {
	const char *name;		/* as appears in the config file */

	void * (*init)(struct hostapd_data *hapd);
	void * (*init_bssid)(struct hostapd_data *hapd, const u8 *bssid);
	void (*deinit)(void *priv);

	int (*wireless_event_init)(void *priv);
	void (*wireless_event_deinit)(void *priv);

	/**
	 * set_8021x - enable/disable IEEE 802.1X support
	 * @ifname: Interface name (for multi-SSID/VLAN support)
	 * @priv: driver private data
	 * @enabled: 1 = enable, 0 = disable
	 *
	 * Returns: 0 on success, -1 on failure
	 *
	 * Configure the kernel driver to enable/disable 802.1X support.
	 * This may be an empty function if 802.1X support is always enabled.
	 */
	int (*set_ieee8021x)(const char *ifname, void *priv, int enabled);

	/**
	 * set_privacy - enable/disable privacy
	 * @priv: driver private data
	 * @enabled: 1 = privacy enabled, 0 = disabled
	 *
	 * Return: 0 on success, -1 on failure
	 *
	 * Configure privacy.
	 */
	int (*set_privacy)(const char *ifname, void *priv, int enabled);

	int (*set_encryption)(const char *ifname, void *priv, const char *alg,
			      const u8 *addr, int idx,
			      const u8 *key, size_t key_len, int txkey);
	int (*get_seqnum)(const char *ifname, void *priv, const u8 *addr,
			  int idx, u8 *seq);
	int (*get_seqnum_igtk)(const char *ifname, void *priv, const u8 *addr,
			       int idx, u8 *seq);
	int (*flush)(void *priv);
	int (*set_generic_elem)(const char *ifname, void *priv, const u8 *elem,
				size_t elem_len);

	int (*read_sta_data)(void *priv, struct hostap_sta_driver_data *data,
			     const u8 *addr);
	int (*send_eapol)(void *priv, const u8 *addr, const u8 *data,
			  size_t data_len, int encrypt, const u8 *own_addr);
	int (*sta_deauth)(void *priv, const u8 *addr, int reason);
	int (*sta_disassoc)(void *priv, const u8 *addr, int reason);
	int (*sta_remove)(void *priv, const u8 *addr);
	int (*get_ssid)(const char *ifname, void *priv, u8 *buf, int len);
	int (*set_ssid)(const char *ifname, void *priv, const u8 *buf,
			int len);
	int (*set_countermeasures)(void *priv, int enabled);
	int (*send_mgmt_frame)(void *priv, const void *msg, size_t len,
			       int flags);
	int (*set_assoc_ap)(void *priv, const u8 *addr);
	/* note: sta_add() is deprecated; use sta_add2() instead */
	int (*sta_add)(const char *ifname, void *priv, const u8 *addr, u16 aid,
		       u16 capability, u8 *supp_rates, size_t supp_rates_len,
		       int flags, u16 listen_interval);
	int (*sta_add2)(const char *ifname, void *priv,
			struct hostapd_sta_add_params *params);
	int (*get_inact_sec)(void *priv, const u8 *addr);
	int (*sta_clear_stats)(void *priv, const u8 *addr);

	/* note: set_freq() is deprecated; use set_freq2() instead */
	int (*set_freq)(void *priv, int mode, int freq);
	int (*set_freq2)(void *priv, struct hostapd_freq_params *freq);
	int (*set_rts)(void *priv, int rts);
	int (*get_rts)(void *priv, int *rts);
	int (*set_frag)(void *priv, int frag);
	int (*get_frag)(void *priv, int *frag);
	int (*set_retry)(void *priv, int short_retry, int long_retry);
	int (*get_retry)(void *priv, int *short_retry, int *long_retry);

	int (*sta_set_flags)(void *priv, const u8 *addr,
			     int total_flags, int flags_or, int flags_and);
	int (*set_rate_sets)(void *priv, int *supp_rates, int *basic_rates,
			     int mode);
	int (*set_regulatory_domain)(void *priv, unsigned int rd);
	int (*set_country)(void *priv, const char *country);
	int (*set_ieee80211d)(void *priv, int enabled);
	int (*set_beacon)(const char *ifname, void *priv,
			  u8 *head, size_t head_len,
			  u8 *tail, size_t tail_len);

	/* Configure internal bridge:
	 * 0 = disabled, i.e., client separation is enabled (no bridging of
	 *     packets between associated STAs
	 * 1 = enabled, i.e., bridge packets between associated STAs (default)
	 */
	int (*set_internal_bridge)(void *priv, int value);
	int (*set_beacon_int)(void *priv, int value);
	int (*set_dtim_period)(const char *ifname, void *priv, int value);
	/* Configure broadcast SSID mode:
	 * 0 = include SSID in Beacon frames and reply to Probe Request frames
	 *     that use broadcast SSID
	 * 1 = hide SSID from Beacon frames and ignore Probe Request frames for
	 *     broadcast SSID
	 */
	int (*set_broadcast_ssid)(void *priv, int value);
	int (*set_cts_protect)(void *priv, int value);
	int (*set_key_tx_rx_threshold)(void *priv, int value);
	int (*set_preamble)(void *priv, int value);
	int (*set_short_slot_time)(void *priv, int value);
	int (*set_tx_queue_params)(void *priv, int queue, int aifs, int cw_min,
				   int cw_max, int burst_time);
	int (*bss_add)(void *priv, const char *ifname, const u8 *bssid);
	int (*bss_remove)(void *priv, const char *ifname);
	int (*valid_bss_mask)(void *priv, const u8 *addr, const u8 *mask);
	int (*passive_scan)(void *priv, int now, int our_mode_only,
			    int interval, int _listen, int *channel,
			    int *last_rx);
	struct hostapd_hw_modes * (*get_hw_feature_data)(void *priv,
							 u16 *num_modes,
							 u16 *flags);
	int (*if_add)(const char *iface, void *priv,
		      enum hostapd_driver_if_type type, char *ifname,
		      const u8 *addr);
	int (*if_update)(void *priv, enum hostapd_driver_if_type type,
			 char *ifname, const u8 *addr);
	int (*if_remove)(void *priv, enum hostapd_driver_if_type type,
			 const char *ifname, const u8 *addr);
	int (*set_sta_vlan)(void *priv, const u8 *addr, const char *ifname,
			    int vlan_id);
	/**
	 * commit - Optional commit changes handler
	 * @priv: driver private data
	 * Returns: 0 on success, -1 on failure
	 *
	 * This optional handler function can be registered if the driver
	 * interface implementation needs to commit changes (e.g., by setting
	 * network interface up) at the end of initial configuration. If set,
	 * this handler will be called after initial setup has been completed.
	 */
	int (*commit)(void *priv);

	int (*send_ether)(void *priv, const u8 *dst, const u8 *src, u16 proto,
			  const u8 *data, size_t data_len);

	int (*set_radius_acl_auth)(void *priv, const u8 *mac, int accepted, 
				   u32 session_timeout);
	int (*set_radius_acl_expire)(void *priv, const u8 *mac);

	int (*set_ht_params)(const char *ifname, void *priv,
			     const u8 *ht_capab, size_t ht_capab_len,
			     const u8 *ht_oper, size_t ht_oper_len);

	int (*set_wps_beacon_ie)(const char *ifname, void *priv,
				 const u8 *ie, size_t len);
	int (*set_wps_probe_resp_ie)(const char *ifname, void *priv,
				     const u8 *ie, size_t len);
};

static inline void *
hostapd_driver_init(struct hostapd_data *hapd)
{
	if (hapd->driver == NULL || hapd->driver->init == NULL)
		return NULL;
	return hapd->driver->init(hapd);
}

static inline void *
hostapd_driver_init_bssid(struct hostapd_data *hapd, const u8 *bssid)
{
	if (hapd->driver == NULL || hapd->driver->init_bssid == NULL)
		return NULL;
	return hapd->driver->init_bssid(hapd, bssid);
}

static inline void
hostapd_driver_deinit(struct hostapd_data *hapd)
{
	if (hapd->driver == NULL || hapd->driver->deinit == NULL)
		return;
	hapd->driver->deinit(hapd->drv_priv);
}

static inline int
hostapd_wireless_event_init(struct hostapd_data *hapd)
{
	if (hapd->driver == NULL ||
	    hapd->driver->wireless_event_init == NULL)
		return 0;
	return hapd->driver->wireless_event_init(hapd->drv_priv);
}

static inline void
hostapd_wireless_event_deinit(struct hostapd_data *hapd)
{
	if (hapd->driver == NULL ||
	    hapd->driver->wireless_event_deinit == NULL)
		return;
	hapd->driver->wireless_event_deinit(hapd->drv_priv);
}

static inline int
hostapd_set_ieee8021x(const char *ifname, struct hostapd_data *hapd,
		      int enabled)
{
	if (hapd->driver == NULL || hapd->driver->set_ieee8021x == NULL)
		return 0;
	return hapd->driver->set_ieee8021x(ifname, hapd->drv_priv, enabled);
}

static inline int
hostapd_set_privacy(struct hostapd_data *hapd, int enabled)
{
	if (hapd->driver == NULL || hapd->driver->set_privacy == NULL)
		return 0;
	return hapd->driver->set_privacy(hapd->conf->iface, hapd->drv_priv,
					 enabled);
}

static inline int
hostapd_set_encryption(const char *ifname, struct hostapd_data *hapd,
		       const char *alg, const u8 *addr, int idx,
		       u8 *key, size_t key_len, int txkey)
{
	if (hapd->driver == NULL || hapd->driver->set_encryption == NULL)
		return 0;
	return hapd->driver->set_encryption(ifname, hapd->drv_priv, alg, addr,
					    idx, key, key_len, txkey);
}

static inline int
hostapd_get_seqnum(const char *ifname, struct hostapd_data *hapd,
		   const u8 *addr, int idx, u8 *seq)
{
	if (hapd->driver == NULL || hapd->driver->get_seqnum == NULL)
		return 0;
	return hapd->driver->get_seqnum(ifname, hapd->drv_priv, addr, idx,
					seq);
}

static inline int
hostapd_get_seqnum_igtk(const char *ifname, struct hostapd_data *hapd,
			const u8 *addr, int idx, u8 *seq)
{
	if (hapd->driver == NULL || hapd->driver->get_seqnum_igtk == NULL)
		return -1;
	return hapd->driver->get_seqnum_igtk(ifname, hapd->drv_priv, addr, idx,
					     seq);
}

static inline int
hostapd_flush(struct hostapd_data *hapd)
{
	if (hapd->driver == NULL || hapd->driver->flush == NULL)
		return 0;
	return hapd->driver->flush(hapd->drv_priv);
}

static inline int
hostapd_set_generic_elem(struct hostapd_data *hapd, const u8 *elem,
			 size_t elem_len)
{
	if (hapd->driver == NULL || hapd->driver->set_generic_elem == NULL)
		return 0;
	return hapd->driver->set_generic_elem(hapd->conf->iface,
					      hapd->drv_priv, elem, elem_len);
}

static inline int
hostapd_read_sta_data(struct hostapd_data *hapd,
		      struct hostap_sta_driver_data *data, const u8 *addr)
{
	if (hapd->driver == NULL || hapd->driver->read_sta_data == NULL)
		return -1;
	return hapd->driver->read_sta_data(hapd->drv_priv, data, addr);
}

static inline int
hostapd_send_eapol(struct hostapd_data *hapd, const u8 *addr, const u8 *data,
		   size_t data_len, int encrypt)
{
	if (hapd->driver == NULL || hapd->driver->send_eapol == NULL)
		return 0;
	return hapd->driver->send_eapol(hapd->drv_priv, addr, data, data_len,
					encrypt, hapd->own_addr);
}

static inline int
hostapd_sta_deauth(struct hostapd_data *hapd, const u8 *addr, int reason)
{
	if (hapd->driver == NULL || hapd->driver->sta_deauth == NULL)
		return 0;
	return hapd->driver->sta_deauth(hapd->drv_priv, addr, reason);
}

static inline int
hostapd_sta_disassoc(struct hostapd_data *hapd, const u8 *addr, int reason)
{
	if (hapd->driver == NULL || hapd->driver->sta_disassoc == NULL)
		return 0;
	return hapd->driver->sta_disassoc(hapd->drv_priv, addr, reason);
}

static inline int
hostapd_sta_remove(struct hostapd_data *hapd, const u8 *addr)
{
	if (hapd->driver == NULL || hapd->driver->sta_remove == NULL)
		return 0;
	return hapd->driver->sta_remove(hapd->drv_priv, addr);
}

static inline int
hostapd_get_ssid(struct hostapd_data *hapd, u8 *buf, size_t len)
{
	if (hapd->driver == NULL || hapd->driver->get_ssid == NULL)
		return 0;
	return hapd->driver->get_ssid(hapd->conf->iface, hapd->drv_priv, buf,
				      len);
}

static inline int
hostapd_set_ssid(struct hostapd_data *hapd, const u8 *buf, size_t len)
{
	if (hapd->driver == NULL || hapd->driver->set_ssid == NULL)
		return 0;
	return hapd->driver->set_ssid(hapd->conf->iface, hapd->drv_priv, buf,
				      len);
}

static inline int
hostapd_send_mgmt_frame(struct hostapd_data *hapd, const void *msg, size_t len,
			int flags)
{
	if (hapd->driver == NULL || hapd->driver->send_mgmt_frame == NULL)
		return 0;
	return hapd->driver->send_mgmt_frame(hapd->drv_priv, msg, len, flags);
}

static inline int
hostapd_set_assoc_ap(struct hostapd_data *hapd, const u8 *addr)
{
	if (hapd->driver == NULL || hapd->driver->set_assoc_ap == NULL)
		return 0;
	return hapd->driver->set_assoc_ap(hapd->drv_priv, addr);
}

static inline int
hostapd_set_countermeasures(struct hostapd_data *hapd, int enabled)
{
	if (hapd->driver == NULL || hapd->driver->set_countermeasures == NULL)
		return 0;
	return hapd->driver->set_countermeasures(hapd->drv_priv, enabled);
}

static inline int
hostapd_sta_add(const char *ifname, struct hostapd_data *hapd, const u8 *addr,
		u16 aid, u16 capability, const u8 *supp_rates,
		size_t supp_rates_len, int flags, u16 listen_interval,
		const struct ht_cap_ie *ht_capabilities)
{
	if (hapd->driver == NULL)
		return 0;

	if (hapd->driver->sta_add2) {
		struct hostapd_sta_add_params params;
		os_memset(&params, 0, sizeof(params));
		params.addr = addr;
		params.aid = aid;
		params.capability = capability;
		params.supp_rates = supp_rates;
		params.supp_rates_len = supp_rates_len;
		params.flags = flags;
		params.listen_interval = listen_interval;
		params.ht_capabilities = ht_capabilities;
		return hapd->driver->sta_add2(ifname, hapd->drv_priv, &params);
	}

	if (hapd->driver->sta_add == NULL)
		return 0;
	return hapd->driver->sta_add(ifname, hapd->drv_priv, addr, aid,
				     capability, (u8 *) supp_rates,
				     supp_rates_len,
				     flags, listen_interval);
}

static inline int
hostapd_get_inact_sec(struct hostapd_data *hapd, const u8 *addr)
{
	if (hapd->driver == NULL || hapd->driver->get_inact_sec == NULL)
		return 0;
	return hapd->driver->get_inact_sec(hapd->drv_priv, addr);
}

static inline int
hostapd_set_freq(struct hostapd_data *hapd, int mode, int freq, int ht_enabled,
		 int sec_channel_offset)
{
	if (hapd->driver == NULL)
		return 0;
	if (hapd->driver->set_freq2) {
		struct hostapd_freq_params data;
		os_memset(&data, 0, sizeof(data));
		data.mode = mode;
		data.freq = freq;
		data.ht_enabled = ht_enabled;
		data.sec_channel_offset = sec_channel_offset;
		return hapd->driver->set_freq2(hapd->drv_priv, &data);
	}

	if (hapd->driver->set_freq == NULL)
		return 0;
	return hapd->driver->set_freq(hapd->drv_priv, mode, freq);
}

static inline int
hostapd_set_rts(struct hostapd_data *hapd, int rts)
{
	if (hapd->driver == NULL || hapd->driver->set_rts == NULL)
		return 0;
	return hapd->driver->set_rts(hapd->drv_priv, rts);
}

static inline int
hostapd_get_rts(struct hostapd_data *hapd, int *rts)
{
	if (hapd->driver == NULL || hapd->driver->get_rts == NULL)
		return 0;
	return hapd->driver->get_rts(hapd->drv_priv, rts);
}

static inline int
hostapd_set_frag(struct hostapd_data *hapd, int frag)
{
	if (hapd->driver == NULL || hapd->driver->set_frag == NULL)
		return 0;
	return hapd->driver->set_frag(hapd->drv_priv, frag);
}

static inline int
hostapd_get_frag(struct hostapd_data *hapd, int *frag)
{
	if (hapd->driver == NULL || hapd->driver->get_frag == NULL)
		return 0;
	return hapd->driver->get_frag(hapd->drv_priv, frag);
}

static inline int
hostapd_set_retry(struct hostapd_data *hapd, int short_retry, int long_retry)
{
	if (hapd->driver == NULL || hapd->driver->set_retry == NULL)
		return 0;
	return hapd->driver->set_retry(hapd->drv_priv, short_retry,
				       long_retry);
}

static inline int
hostapd_get_retry(struct hostapd_data *hapd, int *short_retry, int *long_retry)
{
	if (hapd->driver == NULL || hapd->driver->get_retry == NULL)
		return 0;
	return hapd->driver->get_retry(hapd->drv_priv, short_retry,
				       long_retry);
}

static inline int
hostapd_sta_set_flags(struct hostapd_data *hapd, u8 *addr,
		      int total_flags, int flags_or, int flags_and)
{
	if (hapd->driver == NULL || hapd->driver->sta_set_flags == NULL)
		return 0;
	return hapd->driver->sta_set_flags(hapd->drv_priv, addr, total_flags,
					   flags_or, flags_and);
}

static inline int
hostapd_set_rate_sets(struct hostapd_data *hapd, int *supp_rates,
		      int *basic_rates, int mode)
{
	if (hapd->driver == NULL || hapd->driver->set_rate_sets == NULL)
		return 0;
	return hapd->driver->set_rate_sets(hapd->drv_priv, supp_rates,
					   basic_rates, mode);
}

static inline int
hostapd_set_regulatory_domain(struct hostapd_data *hapd, unsigned int rd)
{
	if (hapd->driver == NULL ||
	    hapd->driver->set_regulatory_domain == NULL)
		return 0;
	return hapd->driver->set_regulatory_domain(hapd->drv_priv, rd);
}

static inline int
hostapd_set_country(struct hostapd_data *hapd, const char *country)
{
	if (hapd->driver == NULL ||
	    hapd->driver->set_country == NULL)
		return 0;
	return hapd->driver->set_country(hapd->drv_priv, country);
}

static inline int
hostapd_set_ieee80211d(struct hostapd_data *hapd, int enabled)
{
	if (hapd->driver == NULL ||
	    hapd->driver->set_ieee80211d == NULL)
		return 0;
	return hapd->driver->set_ieee80211d(hapd->drv_priv, enabled);
}

static inline int
hostapd_sta_clear_stats(struct hostapd_data *hapd, const u8 *addr)
{
	if (hapd->driver == NULL || hapd->driver->sta_clear_stats == NULL)
		return 0;
	return hapd->driver->sta_clear_stats(hapd->drv_priv, addr);
}

static inline int
hostapd_set_beacon(const char *ifname, struct hostapd_data *hapd,
		   u8 *head, size_t head_len,
		   u8 *tail, size_t tail_len)
{
	if (hapd->driver == NULL || hapd->driver->set_beacon == NULL)
		return 0;
	return hapd->driver->set_beacon(ifname, hapd->drv_priv, head, head_len,
					tail, tail_len);
}

static inline int
hostapd_set_internal_bridge(struct hostapd_data *hapd, int value)
{
	if (hapd->driver == NULL || hapd->driver->set_internal_bridge == NULL)
		return 0;
	return hapd->driver->set_internal_bridge(hapd->drv_priv, value);
}

static inline int
hostapd_set_beacon_int(struct hostapd_data *hapd, int value)
{
	if (hapd->driver == NULL || hapd->driver->set_beacon_int == NULL)
		return 0;
	return hapd->driver->set_beacon_int(hapd->drv_priv, value);
}

static inline int
hostapd_set_dtim_period(struct hostapd_data *hapd, int value)
{
	if (hapd->driver == NULL || hapd->driver->set_dtim_period == NULL)
		return 0;
	return hapd->driver->set_dtim_period(hapd->conf->iface, hapd->drv_priv,
					     value);
}

static inline int
hostapd_set_broadcast_ssid(struct hostapd_data *hapd, int value)
{
	if (hapd->driver == NULL || hapd->driver->set_broadcast_ssid == NULL)
		return 0;
	return hapd->driver->set_broadcast_ssid(hapd->drv_priv, value);
}

static inline int
hostapd_set_cts_protect(struct hostapd_data *hapd, int value)
{
	if (hapd->driver == NULL || hapd->driver->set_cts_protect == NULL)
		return 0;
	return hapd->driver->set_cts_protect(hapd->drv_priv, value);
}

static inline int
hostapd_set_key_tx_rx_threshold(struct hostapd_data *hapd, int value)
{
	if (hapd->driver == NULL ||
	    hapd->driver->set_key_tx_rx_threshold == NULL)
		return 0;
	return hapd->driver->set_key_tx_rx_threshold(hapd->drv_priv, value);
}

static inline int
hostapd_set_preamble(struct hostapd_data *hapd, int value)
{
	if (hapd->driver == NULL || hapd->driver->set_preamble == NULL)
		return 0;
	return hapd->driver->set_preamble(hapd->drv_priv, value);
}

static inline int
hostapd_set_short_slot_time(struct hostapd_data *hapd, int value)
{
	if (hapd->driver == NULL || hapd->driver->set_short_slot_time == NULL)
		return 0;
	return hapd->driver->set_short_slot_time(hapd->drv_priv, value);
}

static inline int
hostapd_set_tx_queue_params(struct hostapd_data *hapd, int queue, int aifs,
			    int cw_min, int cw_max, int burst_time)
{
	if (hapd->driver == NULL || hapd->driver->set_tx_queue_params == NULL)
		return 0;
	return hapd->driver->set_tx_queue_params(hapd->drv_priv, queue, aifs,
						 cw_min, cw_max, burst_time);
}

static inline int
hostapd_bss_add(struct hostapd_data *hapd, const char *ifname, const u8 *bssid)
{
	if (hapd->driver == NULL || hapd->driver->bss_add == NULL)
		return 0;
	return hapd->driver->bss_add(hapd->drv_priv, ifname, bssid);
}

static inline int
hostapd_bss_remove(struct hostapd_data *hapd, const char *ifname)
{
	if (hapd->driver == NULL || hapd->driver->bss_remove == NULL)
		return 0;
	return hapd->driver->bss_remove(hapd->drv_priv, ifname);
}

static inline int
hostapd_valid_bss_mask(struct hostapd_data *hapd, const u8 *addr,
		       const u8 *mask)
{
	if (hapd->driver == NULL || hapd->driver->valid_bss_mask == NULL)
		return 1;
	return hapd->driver->valid_bss_mask(hapd->drv_priv, addr, mask);
}

static inline int
hostapd_if_add(struct hostapd_data *hapd, enum hostapd_driver_if_type type,
	       char *ifname, const u8 *addr)
{
	if (hapd->driver == NULL || hapd->driver->if_add == NULL)
		return -1;
	return hapd->driver->if_add(hapd->conf->iface, hapd->drv_priv, type,
				    ifname, addr);
}

static inline int
hostapd_if_update(struct hostapd_data *hapd, enum hostapd_driver_if_type type,
		  char *ifname, const u8 *addr)
{
	if (hapd->driver == NULL || hapd->driver->if_update == NULL)
		return -1;
	return hapd->driver->if_update(hapd->drv_priv, type, ifname, addr);
}

static inline int
hostapd_if_remove(struct hostapd_data *hapd, enum hostapd_driver_if_type type,
		  char *ifname, const u8 *addr)
{
	if (hapd->driver == NULL || hapd->driver->if_remove == NULL)
		return -1;
	return hapd->driver->if_remove(hapd->drv_priv, type, ifname, addr);
}

static inline int
hostapd_passive_scan(struct hostapd_data *hapd, int now, int our_mode_only,
		     int interval, int _listen, int *channel,
		     int *last_rx)
{
	if (hapd->driver == NULL || hapd->driver->passive_scan == NULL)
		return -1;
	return hapd->driver->passive_scan(hapd->drv_priv, now, our_mode_only,
					  interval, _listen, channel, last_rx);
}

static inline struct hostapd_hw_modes *
hostapd_get_hw_feature_data(struct hostapd_data *hapd, u16 *num_modes,
			    u16 *flags)
{
	if (hapd->driver == NULL || hapd->driver->get_hw_feature_data == NULL)
		return NULL;
	return hapd->driver->get_hw_feature_data(hapd->drv_priv, num_modes,
						 flags);
}

static inline int
hostapd_set_sta_vlan(const char *ifname, struct hostapd_data *hapd,
		     const u8 *addr, int vlan_id)
{
	if (hapd->driver == NULL || hapd->driver->set_sta_vlan == NULL)
		return 0;
	return hapd->driver->set_sta_vlan(hapd->drv_priv, addr, ifname, vlan_id);
}

static inline int
hostapd_driver_commit(struct hostapd_data *hapd)
{
	if (hapd->driver == NULL || hapd->driver->commit == NULL)
		return 0;
	return hapd->driver->commit(hapd->drv_priv);
}

static inline int
hostapd_set_radius_acl_auth(struct hostapd_data *hapd, const u8 *mac,
			    int accepted, u32 session_timeout)
{
	if (hapd->driver == NULL || hapd->driver->set_radius_acl_auth == NULL)
		return 0;
	return hapd->driver->set_radius_acl_auth(hapd->drv_priv, mac, accepted,
						 session_timeout);
}

static inline int
hostapd_set_radius_acl_expire(struct hostapd_data *hapd, const u8 *mac)
{
	if (hapd->driver == NULL ||
	    hapd->driver->set_radius_acl_expire == NULL)
		return 0;
	return hapd->driver->set_radius_acl_expire(hapd->drv_priv, mac);
}

#ifdef CONFIG_IEEE80211N
static inline int
hostapd_set_ht_params(const char *ifname, struct hostapd_data *hapd,
		      const u8 *ht_capab, size_t ht_capab_len,
		      const u8 *ht_oper, size_t ht_oper_len)
{
	if (hapd->driver == NULL || hapd->driver->set_ht_params == NULL ||
	    ht_capab == NULL || ht_oper == NULL)
		return 0;
	return hapd->driver->set_ht_params(
		ifname, hapd->drv_priv, ht_capab, ht_capab_len,
		ht_oper, ht_oper_len);
}
#endif /* CONFIG_IEEE80211N */

static inline int
hostapd_drv_none(struct hostapd_data *hapd)
{
	return hapd->driver && os_strcmp(hapd->driver->name, "none") == 0;
}

static inline int
hostapd_set_wps_beacon_ie(struct hostapd_data *hapd, const u8 *ie, size_t len)
{
	if (hapd->driver == NULL || hapd->driver->set_wps_beacon_ie == NULL)
		return 0;
	return hapd->driver->set_wps_beacon_ie(hapd->conf->iface,
					       hapd->drv_priv, ie, len);
}

static inline int
hostapd_set_wps_probe_resp_ie(struct hostapd_data *hapd, const u8 *ie,
			      size_t len)
{
	if (hapd->driver == NULL ||
	    hapd->driver->set_wps_probe_resp_ie == NULL)
		return 0;
	return hapd->driver->set_wps_probe_resp_ie(hapd->conf->iface,
						   hapd->drv_priv, ie, len);
}

#endif /* DRIVER_H */
