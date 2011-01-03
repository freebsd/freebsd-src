/*
 * wpa_supplicant - Event notifications
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

#ifndef NOTIFY_H
#define NOTIFY_H

struct wps_credential;
struct wps_event_m2d;
struct wps_event_fail;

int wpas_notify_supplicant_initialized(struct wpa_global *global);
void wpas_notify_supplicant_deinitialized(struct wpa_global *global);
int wpas_notify_iface_added(struct wpa_supplicant *wpa_s);
void wpas_notify_iface_removed(struct wpa_supplicant *wpa_s);
void wpas_notify_state_changed(struct wpa_supplicant *wpa_s,
			       enum wpa_states new_state,
			       enum wpa_states old_state);
void wpas_notify_network_changed(struct wpa_supplicant *wpa_s);
void wpas_notify_ap_scan_changed(struct wpa_supplicant *wpa_s);
void wpas_notify_bssid_changed(struct wpa_supplicant *wpa_s);
void wpas_notify_network_enabled_changed(struct wpa_supplicant *wpa_s,
					 struct wpa_ssid *ssid);
void wpas_notify_network_selected(struct wpa_supplicant *wpa_s,
				  struct wpa_ssid *ssid);
void wpas_notify_scanning(struct wpa_supplicant *wpa_s);
void wpas_notify_scan_done(struct wpa_supplicant *wpa_s, int success);
void wpas_notify_scan_results(struct wpa_supplicant *wpa_s);
void wpas_notify_wps_credential(struct wpa_supplicant *wpa_s,
				const struct wps_credential *cred);
void wpas_notify_wps_event_m2d(struct wpa_supplicant *wpa_s,
			       struct wps_event_m2d *m2d);
void wpas_notify_wps_event_fail(struct wpa_supplicant *wpa_s,
				struct wps_event_fail *fail);
void wpas_notify_wps_event_success(struct wpa_supplicant *wpa_s);
void wpas_notify_network_added(struct wpa_supplicant *wpa_s,
			       struct wpa_ssid *ssid);
void wpas_notify_network_removed(struct wpa_supplicant *wpa_s,
				 struct wpa_ssid *ssid);
void wpas_notify_bss_added(struct wpa_supplicant *wpa_s, u8 bssid[],
			   unsigned int id);
void wpas_notify_bss_removed(struct wpa_supplicant *wpa_s, u8 bssid[],
			     unsigned int id);
void wpas_notify_bss_freq_changed(struct wpa_supplicant *wpa_s,
				  unsigned int id);
void wpas_notify_bss_signal_changed(struct wpa_supplicant *wpa_s,
				    unsigned int id);
void wpas_notify_bss_privacy_changed(struct wpa_supplicant *wpa_s,
				     unsigned int id);
void wpas_notify_bss_mode_changed(struct wpa_supplicant *wpa_s,
				  unsigned int id);
void wpas_notify_bss_wpaie_changed(struct wpa_supplicant *wpa_s,
				   unsigned int id);
void wpas_notify_bss_rsnie_changed(struct wpa_supplicant *wpa_s,
				   unsigned int id);
void wpas_notify_bss_wps_changed(struct wpa_supplicant *wpa_s,
				 unsigned int id);
void wpas_notify_bss_ies_changed(struct wpa_supplicant *wpa_s,
				 unsigned int id);
void wpas_notify_bss_rates_changed(struct wpa_supplicant *wpa_s,
				   unsigned int id);
void wpas_notify_blob_added(struct wpa_supplicant *wpa_s, const char *name);
void wpas_notify_blob_removed(struct wpa_supplicant *wpa_s, const char *name);

void wpas_notify_debug_level_changed(struct wpa_global *global);
void wpas_notify_debug_timestamp_changed(struct wpa_global *global);
void wpas_notify_debug_show_keys_changed(struct wpa_global *global);
void wpas_notify_suspend(struct wpa_global *global);
void wpas_notify_resume(struct wpa_global *global);

#endif /* NOTIFY_H */
