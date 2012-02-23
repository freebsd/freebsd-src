/*
 * wpa_supplicant / WPS integration
 * Copyright (c) 2008-2009, Jouni Malinen <j@w1.fi>
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

#ifndef WPS_SUPPLICANT_H
#define WPS_SUPPLICANT_H

struct wpa_scan_res;

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
int wpas_wps_start_pbc(struct wpa_supplicant *wpa_s, const u8 *bssid);
int wpas_wps_start_pin(struct wpa_supplicant *wpa_s, const u8 *bssid,
		       const char *pin);
int wpas_wps_start_oob(struct wpa_supplicant *wpa_s, char *device_type,
		       char *path, char *method, char *name);
int wpas_wps_start_reg(struct wpa_supplicant *wpa_s, const u8 *bssid,
		       const char *pin, struct wps_new_ap_settings *settings);
int wpas_wps_ssid_bss_match(struct wpa_supplicant *wpa_s,
			    struct wpa_ssid *ssid, struct wpa_scan_res *bss);
int wpas_wps_ssid_wildcard_ok(struct wpa_supplicant *wpa_s,
			      struct wpa_ssid *ssid, struct wpa_scan_res *bss);
int wpas_wps_scan_pbc_overlap(struct wpa_supplicant *wpa_s,
			      struct wpa_bss *selected, struct wpa_ssid *ssid);
void wpas_wps_notify_scan_results(struct wpa_supplicant *wpa_s);
int wpas_wps_searching(struct wpa_supplicant *wpa_s);
int wpas_wps_scan_result_text(const u8 *ies, size_t ies_len, char *pos,
			      char *end);
int wpas_wps_er_start(struct wpa_supplicant *wpa_s);
int wpas_wps_er_stop(struct wpa_supplicant *wpa_s);
int wpas_wps_er_add_pin(struct wpa_supplicant *wpa_s, const char *uuid,
			const char *pin);
int wpas_wps_er_pbc(struct wpa_supplicant *wpa_s, const char *uuid);
int wpas_wps_er_learn(struct wpa_supplicant *wpa_s, const char *uuid,
		      const char *pin);
int wpas_wps_terminate_pending(struct wpa_supplicant *wpa_s);

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
					  struct wpa_scan_res *bss)
{
	return -1;
}

static inline int wpas_wps_ssid_wildcard_ok(struct wpa_supplicant *wpa_s,
					    struct wpa_ssid *ssid,
					    struct wpa_scan_res *bss)
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

#endif /* CONFIG_WPS */

#endif /* WPS_SUPPLICANT_H */
