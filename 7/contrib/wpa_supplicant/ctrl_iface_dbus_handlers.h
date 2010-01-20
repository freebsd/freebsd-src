/*
 * WPA Supplicant / dbus-based control interface
 * Copyright (c) 2006, Dan Williams <dcbw@redhat.com> and Red Hat, Inc.
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

#ifndef CTRL_IFACE_DBUS_HANDLERS_H
#define CTRL_IFACE_DBUS_HANDLERS_H

#ifdef CONFIG_CTRL_IFACE_DBUS

DBusMessage * wpas_dbus_new_invalid_iface_error(DBusMessage *message);

DBusMessage * wpas_dbus_global_add_interface(DBusMessage *message,
					     struct wpa_global *global);

DBusMessage * wpas_dbus_global_remove_interface(DBusMessage *message,
						struct wpa_global *global);

DBusMessage * wpas_dbus_global_get_interface(DBusMessage *message,
					     struct wpa_global *global);

DBusMessage * wpas_dbus_iface_scan(DBusMessage *message,
				   struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_iface_scan_results(DBusMessage *message,
					   struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_bssid_properties(DBusMessage *message,
					 struct wpa_supplicant *wpa_s,
					 struct wpa_scan_result *res);

DBusMessage * wpas_dbus_iface_capabilities(DBusMessage *message,
					   struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_iface_add_network(DBusMessage *message,
					  struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_iface_remove_network(DBusMessage *message,
					     struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_iface_set_network(DBusMessage *message,
					  struct wpa_supplicant *wpa_s,
					  struct wpa_ssid *ssid);

DBusMessage * wpas_dbus_iface_enable_network(DBusMessage *message,
					     struct wpa_supplicant *wpa_s,
					     struct wpa_ssid *ssid);

DBusMessage * wpas_dbus_iface_disable_network(DBusMessage *message,
					      struct wpa_supplicant *wpa_s,
					      struct wpa_ssid *ssid);

DBusMessage * wpas_dbus_iface_select_network(DBusMessage *message,
                                             struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_iface_disconnect(DBusMessage *message,
					 struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_iface_set_ap_scan(DBusMessage *message,
                                          struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_iface_get_state(DBusMessage *message,
					struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_iface_set_blobs(DBusMessage *message,
				        struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_iface_remove_blobs(DBusMessage *message,
					   struct wpa_supplicant *wpa_s);

#endif /* CONFIG_CTRL_IFACE_DBUS */

#endif /* CTRL_IFACE_DBUS_HANDLERS_H */

