/*
 * hostapd - Driver operations
 * Copyright (c) 2009, Jouni Malinen <j@w1.fi>
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

#ifndef AP_DRV_OPS
#define AP_DRV_OPS

enum wpa_driver_if_type;
struct wpa_bss_params;
struct wpa_driver_scan_params;

void hostapd_set_driver_ops(struct hostapd_driver_ops *ops);
int hostapd_set_privacy(struct hostapd_data *hapd, int enabled);
int hostapd_set_generic_elem(struct hostapd_data *hapd, const u8 *elem,
			     size_t elem_len);
int hostapd_get_ssid(struct hostapd_data *hapd, u8 *buf, size_t len);
int hostapd_set_ssid(struct hostapd_data *hapd, const u8 *buf, size_t len);
int hostapd_if_add(struct hostapd_data *hapd, enum wpa_driver_if_type type,
		   const char *ifname, const u8 *addr, void *bss_ctx,
		   void **drv_priv, char *force_ifname, u8 *if_addr);
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
int hostapd_set_rate_sets(struct hostapd_data *hapd, int *supp_rates,
			  int *basic_rates, int mode);
int hostapd_set_country(struct hostapd_data *hapd, const char *country);
int hostapd_set_cts_protect(struct hostapd_data *hapd, int value);
int hostapd_set_preamble(struct hostapd_data *hapd, int value);
int hostapd_set_short_slot_time(struct hostapd_data *hapd, int value);
int hostapd_set_tx_queue_params(struct hostapd_data *hapd, int queue, int aifs,
				int cw_min, int cw_max, int burst_time);
int hostapd_valid_bss_mask(struct hostapd_data *hapd, const u8 *addr,
			   const u8 *mask);
struct hostapd_hw_modes *
hostapd_get_hw_feature_data(struct hostapd_data *hapd, u16 *num_modes,
			    u16 *flags);
int hostapd_driver_commit(struct hostapd_data *hapd);
int hostapd_set_ht_params(struct hostapd_data *hapd,
			  const u8 *ht_capab, size_t ht_capab_len,
			  const u8 *ht_oper, size_t ht_oper_len);
int hostapd_drv_none(struct hostapd_data *hapd);
int hostapd_driver_scan(struct hostapd_data *hapd,
			struct wpa_driver_scan_params *params);
struct wpa_scan_results * hostapd_driver_get_scan_results(
	struct hostapd_data *hapd);

#endif /* AP_DRV_OPS */
