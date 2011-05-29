/*
 * WPA Supplicant / dbus-based control interface
 * Copyright (c) 2006, Dan Williams <dcbw@redhat.com> and Red Hat, Inc.
 * Copyright (c) 2009-2010, Witold Sowa <witold.sowa@gmail.com>
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

#ifndef CTRL_IFACE_DBUS_NEW_H
#define CTRL_IFACE_DBUS_NEW_H

struct wpa_global;
struct wpa_supplicant;
struct wpa_ssid;
struct wps_event_m2d;
struct wps_event_fail;
struct wps_credential;
enum wpa_states;

enum wpas_dbus_prop {
	WPAS_DBUS_PROP_AP_SCAN,
	WPAS_DBUS_PROP_SCANNING,
	WPAS_DBUS_PROP_STATE,
	WPAS_DBUS_PROP_CURRENT_BSS,
	WPAS_DBUS_PROP_CURRENT_NETWORK,
};

enum wpas_dbus_bss_prop {
	WPAS_DBUS_BSS_PROP_SIGNAL,
	WPAS_DBUS_BSS_PROP_FREQ,
	WPAS_DBUS_BSS_PROP_MODE,
	WPAS_DBUS_BSS_PROP_PRIVACY,
	WPAS_DBUS_BSS_PROP_RATES,
	WPAS_DBUS_BSS_PROP_WPA,
	WPAS_DBUS_BSS_PROP_RSN,
	WPAS_DBUS_BSS_PROP_IES,
};

#define WPAS_DBUS_OBJECT_PATH_MAX 150

#define WPAS_DBUS_NEW_SERVICE		"fi.w1.wpa_supplicant1"
#define WPAS_DBUS_NEW_PATH		"/fi/w1/wpa_supplicant1"
#define WPAS_DBUS_NEW_INTERFACE		"fi.w1.wpa_supplicant1"

#define WPAS_DBUS_NEW_PATH_INTERFACES	WPAS_DBUS_NEW_PATH "/Interfaces"
#define WPAS_DBUS_NEW_IFACE_INTERFACE	WPAS_DBUS_NEW_INTERFACE ".Interface"
#define WPAS_DBUS_NEW_IFACE_WPS WPAS_DBUS_NEW_IFACE_INTERFACE ".WPS"

#define WPAS_DBUS_NEW_NETWORKS_PART "Networks"
#define WPAS_DBUS_NEW_IFACE_NETWORK WPAS_DBUS_NEW_INTERFACE ".Network"

#define WPAS_DBUS_NEW_BSSIDS_PART "BSSs"
#define WPAS_DBUS_NEW_IFACE_BSS	WPAS_DBUS_NEW_INTERFACE ".BSS"


/* Errors */
#define WPAS_DBUS_ERROR_UNKNOWN_ERROR \
	WPAS_DBUS_NEW_INTERFACE ".UnknownError"
#define WPAS_DBUS_ERROR_INVALID_ARGS \
	WPAS_DBUS_NEW_INTERFACE ".InvalidArgs"

#define WPAS_DBUS_ERROR_IFACE_EXISTS \
	WPAS_DBUS_NEW_INTERFACE ".InterfaceExists"
#define WPAS_DBUS_ERROR_IFACE_UNKNOWN \
	WPAS_DBUS_NEW_INTERFACE ".InterfaceUnknown"

#define WPAS_DBUS_ERROR_NOT_CONNECTED \
	WPAS_DBUS_NEW_INTERFACE ".NotConnected"
#define WPAS_DBUS_ERROR_NETWORK_UNKNOWN \
	WPAS_DBUS_NEW_INTERFACE ".NetworkUnknown"

#define WPAS_DBUS_ERROR_BLOB_EXISTS \
	WPAS_DBUS_NEW_INTERFACE ".BlobExists"
#define WPAS_DBUS_ERROR_BLOB_UNKNOWN \
	WPAS_DBUS_NEW_INTERFACE ".BlobUnknown"


#ifdef CONFIG_CTRL_IFACE_DBUS_NEW

int wpas_dbus_ctrl_iface_init(struct wpas_dbus_priv *priv);
void wpas_dbus_ctrl_iface_deinit(struct wpas_dbus_priv *iface);

int wpas_dbus_register_interface(struct wpa_supplicant *wpa_s);
int wpas_dbus_unregister_interface(struct wpa_supplicant *wpa_s);
void wpas_dbus_signal_prop_changed(struct wpa_supplicant *wpa_s,
				   enum wpas_dbus_prop property);
void wpas_dbus_bss_signal_prop_changed(struct wpa_supplicant *wpa_s,
				       enum wpas_dbus_bss_prop property,
				       unsigned int id);
void wpas_dbus_signal_network_enabled_changed(struct wpa_supplicant *wpa_s,
					      struct wpa_ssid *ssid);
void wpas_dbus_signal_network_selected(struct wpa_supplicant *wpa_s, int id);
void wpas_dbus_signal_scan_done(struct wpa_supplicant *wpa_s, int success);
void wpas_dbus_signal_wps_cred(struct wpa_supplicant *wpa_s,
			       const struct wps_credential *cred);
void wpas_dbus_signal_wps_event_m2d(struct wpa_supplicant *wpa_s,
				    struct wps_event_m2d *m2d);
void wpas_dbus_signal_wps_event_fail(struct wpa_supplicant *wpa_s,
				     struct wps_event_fail *fail);
void wpas_dbus_signal_wps_event_success(struct wpa_supplicant *wpa_s);
int wpas_dbus_register_network(struct wpa_supplicant *wpa_s,
			       struct wpa_ssid *ssid);
int wpas_dbus_unregister_network(struct wpa_supplicant *wpa_s, int nid);
int wpas_dbus_unregister_bss(struct wpa_supplicant *wpa_s,
			     u8 bssid[ETH_ALEN], unsigned int id);
int wpas_dbus_register_bss(struct wpa_supplicant *wpa_s,
			   u8 bssid[ETH_ALEN], unsigned int id);
void wpas_dbus_signal_blob_added(struct wpa_supplicant *wpa_s,
				 const char *name);
void wpas_dbus_signal_blob_removed(struct wpa_supplicant *wpa_s,
				   const char *name);
void wpas_dbus_signal_debug_level_changed(struct wpa_global *global);
void wpas_dbus_signal_debug_timestamp_changed(struct wpa_global *global);
void wpas_dbus_signal_debug_show_keys_changed(struct wpa_global *global);

#else /* CONFIG_CTRL_IFACE_DBUS_NEW */

static inline int wpas_dbus_register_interface(struct wpa_supplicant *wpa_s)
{
	return 0;
}

static inline int wpas_dbus_unregister_interface(struct wpa_supplicant *wpa_s)
{
	return 0;
}

#define wpas_dbus_signal_state_changed(w, n, o) do { } while (0)

static inline void wpas_dbus_signal_prop_changed(struct wpa_supplicant *wpa_s,
						 enum wpas_dbus_prop property)
{
}

static inline void wpas_dbus_bss_signal_prop_changed(
	struct wpa_supplicant *wpa_s, enum wpas_dbus_bss_prop property,
	unsigned int id)
{
}

static inline void wpas_dbus_signal_network_enabled_changed(
	struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid)
{
}

static inline void wpas_dbus_signal_network_selected(
	struct wpa_supplicant *wpa_s, int id)
{
}

static inline void wpas_dbus_signal_scan_done(struct wpa_supplicant *wpa_s,
					      int success)
{
}

static inline void wpas_dbus_signal_wps_cred(struct wpa_supplicant *wpa_s,
					     const struct wps_credential *cred)
{
}

static inline void wpas_dbus_signal_wps_event_m2d(struct wpa_supplicant *wpa_s,
						  struct wps_event_m2d *m2d)
{
}

static inline void wpas_dbus_signal_wps_event_fail(
	struct wpa_supplicant *wpa_s, struct wps_event_fail *fail)
{
}

static inline void wpas_dbus_signal_wps_event_success(
	struct wpa_supplicant *wpa_s)
{
}

static inline int wpas_dbus_register_network(struct wpa_supplicant *wpa_s,
					     struct wpa_ssid *ssid)
{
	return 0;
}

static inline int wpas_dbus_unregister_network(struct wpa_supplicant *wpa_s,
					       int nid)
{
	return 0;
}

static inline int wpas_dbus_unregister_bss(struct wpa_supplicant *wpa_s,
					   u8 bssid[ETH_ALEN], unsigned int id)
{
	return 0;
}

static inline int wpas_dbus_register_bss(struct wpa_supplicant *wpa_s,
					 u8 bssid[ETH_ALEN], unsigned int id)
{
	return 0;
}

static inline void wpas_dbus_signal_blob_added(struct wpa_supplicant *wpa_s,
					       const char *name)
{
}

static inline void wpas_dbus_signal_blob_removed(struct wpa_supplicant *wpa_s,
						 const char *name)
{
}

static inline void wpas_dbus_signal_debug_level_changed(
	struct wpa_global *global)
{
}

static inline void wpas_dbus_signal_debug_timestamp_changed(
	struct wpa_global *global)
{
}

static inline void wpas_dbus_signal_debug_show_keys_changed(
	struct wpa_global *global)
{
}

#endif /* CONFIG_CTRL_IFACE_DBUS_NEW */

#endif /* CTRL_IFACE_DBUS_H_NEW */
