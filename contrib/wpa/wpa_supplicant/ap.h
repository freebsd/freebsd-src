/*
 * WPA Supplicant - Basic AP mode support routines
 * Copyright (c) 2003-2009, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2009, Atheros Communications
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

#ifndef AP_H
#define AP_H

int wpa_supplicant_create_ap(struct wpa_supplicant *wpa_s,
			     struct wpa_ssid *ssid);
void wpa_supplicant_ap_deinit(struct wpa_supplicant *wpa_s);
void wpa_supplicant_ap_rx_eapol(struct wpa_supplicant *wpa_s,
				const u8 *src_addr, const u8 *buf, size_t len);
int wpa_supplicant_ap_wps_pbc(struct wpa_supplicant *wpa_s, const u8 *bssid);
int wpa_supplicant_ap_wps_pin(struct wpa_supplicant *wpa_s, const u8 *bssid,
			      const char *pin, char *buf, size_t buflen);
int ap_ctrl_iface_sta_first(struct wpa_supplicant *wpa_s,
			    char *buf, size_t buflen);
int ap_ctrl_iface_sta(struct wpa_supplicant *wpa_s, const char *txtaddr,
		      char *buf, size_t buflen);
int ap_ctrl_iface_sta_next(struct wpa_supplicant *wpa_s, const char *txtaddr,
			   char *buf, size_t buflen);
int ap_ctrl_iface_wpa_get_status(struct wpa_supplicant *wpa_s, char *buf,
				 size_t buflen, int verbose);
void ap_tx_status(void *ctx, const u8 *addr,
		  const u8 *buf, size_t len, int ack);
void ap_rx_from_unknown_sta(void *ctx, const u8 *frame, size_t len);
void ap_mgmt_rx(void *ctx, struct rx_mgmt *rx_mgmt);
void ap_mgmt_tx_cb(void *ctx, const u8 *buf, size_t len, u16 stype, int ok);
int wpa_supplicant_ap_mac_addr_filter(struct wpa_supplicant *wpa_s,
				      const u8 *addr);

#endif /* AP_H */
