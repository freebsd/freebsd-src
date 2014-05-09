/*
 * wpa_supplicant / WPS integration
 * Copyright (c) 2008-2012, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef WPS_SUPPLICANT_H
#define WPS_SUPPLICANT_H

struct wpa_scan_results;

#ifdef CONFIG_WPS

#include "wps/wps.h"
#include "wps/wps_defs.h"

struct wpa_bss;

struct wps_new_ap_settings {
	const char *ssid_hex;
	const char *auth;
	const char *encr;
	const char *key_hex;
};

int wpas_wps_init(struct wpa_supplicant *wpa_s);
void wpas_wps_deinit(struct wpa_supplicant *wpa_s);
int wpas_wps_eapol_cb(struct wpa_supplicant *wpa_s);
enum wps_request_type wpas_wps_get_req_type(struct wpa_ssid *ssid);
int wpas_wps_start_pbc(struct wpa_supplicant *wpa_s, const u8 *bssid,
		       int p2p_group);
int wpas_wps_start_pin(struct wpa_supplicant *wpa_s, const u8 *bssid,
		       const char *pin, int p2p_group, u16 dev_pw_id);
int wpas_wps_cancel(struct wpa_supplicant *wpa_s);
int wpas_wps_start_reg(struct wpa_supplicant *wpa_s, const u8 *bssid,
		       const char *pin, struct wps_new_ap_settings *settings);
int wpas_wps_ssid_bss_match(struct wpa_supplicant *wpa_s,
			    struct wpa_ssid *ssid, struct wpa_bss *bss);
int wpas_wps_ssid_wildcard_ok(struct wpa_supplicant *wpa_s,
			      struct wpa_ssid *ssid, struct wpa_bss *bss);
int wpas_wps_scan_pbc_overlap(struct wpa_supplicant *wpa_s,
			      struct wpa_bss *selected, struct wpa_ssid *ssid);
void wpas_wps_notify_scan_results(struct wpa_supplicant *wpa_s);
int wpas_wps_searching(struct wpa_supplicant *wpa_s);
int wpas_wps_scan_result_text(const u8 *ies, size_t ies_len, char *pos,
			      char *end);
int wpas_wps_er_start(struct wpa_supplicant *wpa_s, const char *filter);
int wpas_wps_er_stop(struct wpa_supplicant *wpa_s);
int wpas_wps_er_add_pin(struct wpa_supplicant *wpa_s, const u8 *addr,
			const char *uuid, const char *pin);
int wpas_wps_er_pbc(struct wpa_supplicant *wpa_s, const char *uuid);
int wpas_wps_er_learn(struct wpa_supplicant *wpa_s, const char *uuid,
		      const char *pin);
int wpas_wps_er_set_config(struct wpa_supplicant *wpa_s, const char *uuid,
			   int id);
int wpas_wps_er_config(struct wpa_supplicant *wpa_s, const char *uuid,
		       const char *pin, struct wps_new_ap_settings *settings);
struct wpabuf * wpas_wps_er_nfc_config_token(struct wpa_supplicant *wpa_s,
					     int ndef, const char *uuid);
int wpas_wps_terminate_pending(struct wpa_supplicant *wpa_s);
int wpas_wps_in_progress(struct wpa_supplicant *wpa_s);
void wpas_wps_update_config(struct wpa_supplicant *wpa_s);
struct wpabuf * wpas_wps_nfc_token(struct wpa_supplicant *wpa_s, int ndef);
int wpas_wps_start_nfc(struct wpa_supplicant *wpa_s, const u8 *bssid);
int wpas_wps_nfc_tag_read(struct wpa_supplicant *wpa_s,
			  const struct wpabuf *data);
struct wpabuf * wpas_wps_nfc_handover_req(struct wpa_supplicant *wpa_s);
struct wpabuf * wpas_wps_nfc_handover_sel(struct wpa_supplicant *wpa_s);
int wpas_wps_nfc_rx_handover_req(struct wpa_supplicant *wpa_s,
				 const struct wpabuf *data);
int wpas_wps_nfc_rx_handover_sel(struct wpa_supplicant *wpa_s,
				 const struct wpabuf *data);
void wpas_wps_update_ap_info(struct wpa_supplicant *wpa_s,
			     struct wpa_scan_results *scan_res);
void wpas_wps_notify_assoc(struct wpa_supplicant *wpa_s, const u8 *bssid);

#else /* CONFIG_WPS */

static inline int wpas_wps_init(struct wpa_supplicant *wpa_s)
{
	return 0;
}

static inline void wpas_wps_deinit(struct wpa_supplicant *wpa_s)
{
}

static inline int wpas_wps_eapol_cb(struct wpa_supplicant *wpa_s)
{
	return 0;
}

static inline u8 wpas_wps_get_req_type(struct wpa_ssid *ssid)
{
	return 0;
}

static inline int wpas_wps_ssid_bss_match(struct wpa_supplicant *wpa_s,
					  struct wpa_ssid *ssid,
					  struct wpa_bss *bss)
{
	return -1;
}

static inline int wpas_wps_ssid_wildcard_ok(struct wpa_supplicant *wpa_s,
					    struct wpa_ssid *ssid,
					    struct wpa_bss *bss)
{
	return 0;
}

static inline int wpas_wps_scan_pbc_overlap(struct wpa_supplicant *wpa_s,
					    struct wpa_bss *selected,
					    struct wpa_ssid *ssid)
{
	return 0;
}

static inline void wpas_wps_notify_scan_results(struct wpa_supplicant *wpa_s)
{
}

static inline int wpas_wps_searching(struct wpa_supplicant *wpa_s)
{
	return 0;
}

static inline void wpas_wps_update_ap_info(struct wpa_supplicant *wpa_s,
					   struct wpa_scan_results *scan_res)
{
}

static inline void wpas_wps_notify_assoc(struct wpa_supplicant *wpa_s,
					 const u8 *bssid)
{
}

#endif /* CONFIG_WPS */

#endif /* WPS_SUPPLICANT_H */
