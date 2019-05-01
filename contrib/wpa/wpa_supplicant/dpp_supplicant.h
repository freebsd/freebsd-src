/*
 * wpa_supplicant - DPP
 * Copyright (c) 2017, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef DPP_SUPPLICANT_H
#define DPP_SUPPLICANT_H

int wpas_dpp_qr_code(struct wpa_supplicant *wpa_s, const char *cmd);
int wpas_dpp_auth_init(struct wpa_supplicant *wpa_s, const char *cmd);
int wpas_dpp_listen(struct wpa_supplicant *wpa_s, const char *cmd);
void wpas_dpp_listen_stop(struct wpa_supplicant *wpa_s);
void wpas_dpp_cancel_remain_on_channel_cb(struct wpa_supplicant *wpa_s,
					  unsigned int freq);
void wpas_dpp_rx_action(struct wpa_supplicant *wpa_s, const u8 *src,
			const u8 *buf, size_t len, unsigned int freq);
int wpas_dpp_configurator_sign(struct wpa_supplicant *wpa_s, const char *cmd);
int wpas_dpp_pkex_add(struct wpa_supplicant *wpa_s, const char *cmd);
int wpas_dpp_pkex_remove(struct wpa_supplicant *wpa_s, const char *id);
void wpas_dpp_stop(struct wpa_supplicant *wpa_s);
int wpas_dpp_init(struct wpa_supplicant *wpa_s);
void wpas_dpp_deinit(struct wpa_supplicant *wpa_s);
int wpas_dpp_check_connect(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid,
			   struct wpa_bss *bss);

#endif /* DPP_SUPPLICANT_H */
