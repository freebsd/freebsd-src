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

#ifndef CTRL_IFACE_DBUS_NEW_HANDLERS_H
#define CTRL_IFACE_DBUS_NEW_HANDLERS_H

struct network_handler_args {
	struct wpa_supplicant *wpa_s;
	struct wpa_ssid *ssid;
};

struct bss_handler_args {
	struct wpa_supplicant *wpa_s;
	unsigned int id;
};

DBusMessage * wpas_dbus_simple_property_getter(DBusMessage *message,
					       const int type,
					       const void *val);

DBusMessage * wpas_dbus_simple_property_setter(DBusMessage *message,
					       const int type, void *val);

DBusMessage * wpas_dbus_simple_array_property_getter(DBusMessage *message,
						     const int type,
						     const void *array,
						     size_t array_len);

DBusMessage * wpas_dbus_handler_create_interface(DBusMessage *message,
						 struct wpa_global *global);

DBusMessage * wpas_dbus_handler_remove_interface(DBusMessage *message,
						 struct wpa_global *global);

DBusMessage * wpas_dbus_handler_get_interface(DBusMessage *message,
					      struct wpa_global *global);

DBusMessage * wpas_dbus_getter_debug_level(DBusMessage *message,
					   struct wpa_global *global);

DBusMessage * wpas_dbus_getter_debug_timestamp(DBusMessage *message,
					       struct wpa_global *global);

DBusMessage * wpas_dbus_getter_debug_show_keys(DBusMessage *message,
					       struct wpa_global *global);

DBusMessage * wpas_dbus_setter_debug_level(DBusMessage *message,
					   struct wpa_global *global);

DBusMessage * wpas_dbus_setter_debug_timestamp(DBusMessage *message,
					       struct wpa_global *global);

DBusMessage * wpas_dbus_setter_debug_show_keys(DBusMessage *message,
					       struct wpa_global *global);

DBusMessage * wpas_dbus_getter_interfaces(DBusMessage *message,
					  struct wpa_global *global);

DBusMessage * wpas_dbus_getter_eap_methods(DBusMessage *message,
					   void *nothing);

DBusMessage * wpas_dbus_handler_scan(DBusMessage *message,
				     struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_disconnect(DBusMessage *message,
					   struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_add_network(DBusMessage *message,
					    struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_remove_network(DBusMessage *message,
					       struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_select_network(DBusMessage *message,
					       struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_add_blob(DBusMessage *message,
					 struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_get_blob(DBusMessage *message,
					 struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_remove_blob(DBusMessage *message,
					    struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_getter_capabilities(DBusMessage *message,
					    struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_getter_state(DBusMessage *message,
				     struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_getter_scanning(DBusMessage *message,
					struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_getter_ap_scan(DBusMessage *message,
				       struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_setter_ap_scan(DBusMessage *message,
				       struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_getter_ifname(DBusMessage *message,
				      struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_getter_driver(DBusMessage *message,
				      struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_getter_bridge_ifname(DBusMessage *message,
					     struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_getter_current_bss(DBusMessage *message,
					   struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_getter_current_network(DBusMessage *message,
					       struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_getter_bsss(DBusMessage *message,
				    struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_getter_networks(DBusMessage *message,
					struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_getter_blobs(DBusMessage *message,
				     struct wpa_supplicant *bss);

DBusMessage * wpas_dbus_getter_bss_bssid(DBusMessage *message,
					 struct bss_handler_args *bss);

DBusMessage * wpas_dbus_getter_bss_ssid(DBusMessage *message,
					struct bss_handler_args *bss);

DBusMessage * wpas_dbus_getter_bss_privacy(DBusMessage *message,
					   struct bss_handler_args *bss);

DBusMessage * wpas_dbus_getter_bss_mode(DBusMessage *message,
					struct bss_handler_args *bss);

DBusMessage * wpas_dbus_getter_bss_signal(DBusMessage *message,
					  struct bss_handler_args *bss);

DBusMessage * wpas_dbus_getter_bss_frequency(DBusMessage *message,
					     struct bss_handler_args *bss);

DBusMessage * wpas_dbus_getter_bss_rates(DBusMessage *message,
					 struct bss_handler_args *bss);

DBusMessage * wpas_dbus_getter_bss_wpa(DBusMessage *message,
				       struct bss_handler_args *bss);

DBusMessage * wpas_dbus_getter_bss_rsn(DBusMessage *message,
				       struct bss_handler_args *bss);

DBusMessage * wpas_dbus_getter_bss_ies(DBusMessage *message,
				       struct bss_handler_args *bss);

DBusMessage * wpas_dbus_getter_enabled(DBusMessage *message,
				       struct network_handler_args *net);

DBusMessage * wpas_dbus_setter_enabled(DBusMessage *message,
				       struct network_handler_args *net);

DBusMessage * wpas_dbus_getter_network_properties(
	DBusMessage *message, struct network_handler_args *net);

DBusMessage * wpas_dbus_setter_network_properties(
	DBusMessage *message, struct network_handler_args *net);

DBusMessage * wpas_dbus_handler_wps_start(DBusMessage *message,
					  struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_getter_process_credentials(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_setter_process_credentials(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_getter_credentials(DBusMessage *message,
					   struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_error_invalid_args(DBusMessage *message,
					   const char *arg);
DBusMessage * wpas_dbus_error_unknown_error(DBusMessage *message,
					    const char *arg);

#endif /* CTRL_IFACE_DBUS_HANDLERS_NEW_H */
