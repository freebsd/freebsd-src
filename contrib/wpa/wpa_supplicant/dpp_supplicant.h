/*
 * wpa_supplicant - DPP
 * Copyright (c) 2017, Qualcomm Atheros, Inc.
 * Copyright (c) 2018-2020, The Linux Foundation
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef DPP_SUPPLICANT_H
#define DPP_SUPPLICANT_H

enum dpp_status_error;

int wpas_dpp_qr_code(struct wpa_supplicant *wpa_s, const char *cmd);
int wpas_dpp_nfc_uri(struct wpa_supplicant *wpa_s, const char *cmd);
int wpas_dpp_nfc_handover_req(struct wpa_supplicant *wpa_s, const char *cmd);
int wpas_dpp_nfc_handover_sel(struct wpa_supplicant *wpa_s, const char *cmd);
int wpas_dpp_auth_init(struct wpa_supplicant *wpa_s, const char *cmd);
int wpas_dpp_listen(struct wpa_supplicant *wpa_s, const char *cmd);
void wpas_dpp_listen_stop(struct wpa_supplicant *wpa_s);
void wpas_dpp_remain_on_channel_cb(struct wpa_supplicant *wpa_s,
				   unsigned int freq, unsigned int duration);
void wpas_dpp_cancel_remain_on_channel_cb(struct wpa_supplicant *wpa_s,
					  unsigned int freq);
void wpas_dpp_tx_wait_expire(struct wpa_supplicant *wpa_s);
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
int wpas_dpp_controller_start(struct wpa_supplicant *wpa_s, const char *cmd);
void wpas_dpp_connected(struct wpa_supplicant *wpa_s);
void wpas_dpp_send_conn_status_result(struct wpa_supplicant *wpa_s,
				      enum dpp_status_error result);
int wpas_dpp_chirp(struct wpa_supplicant *wpa_s, const char *cmd);
void wpas_dpp_chirp_stop(struct wpa_supplicant *wpa_s);
int wpas_dpp_reconfig(struct wpa_supplicant *wpa_s, const char *cmd);
int wpas_dpp_ca_set(struct wpa_supplicant *wpa_s, const char *cmd);
int wpas_dpp_conf_set(struct wpa_supplicant *wpa_s, const char *cmd);
int wpas_dpp_push_button(struct wpa_supplicant *wpa_s, const char *cmd);
void wpas_dpp_push_button_stop(struct wpa_supplicant *wpa_s);

#endif /* DPP_SUPPLICANT_H */
