/*
 * WPA Supplicant / dbus-based control interface
 * Copyright (c) 2006, Dan Williams <dcbw@redhat.com> and Red Hat, Inc.
 * Copyright (c) 2009-2010, Witold Sowa <witold.sowa@gmail.com>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
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

dbus_bool_t wpas_dbus_simple_property_getter(DBusMessageIter *iter,
					     const int type,
					     const void *val,
					     DBusError *error);

dbus_bool_t wpas_dbus_simple_property_setter(DBusMessageIter *iter,
					     DBusError *error,
					     const int type, void *val);

dbus_bool_t wpas_dbus_simple_array_property_getter(DBusMessageIter *iter,
						   const int type,
						   const void *array,
						   size_t array_len,
						   DBusError *error);

dbus_bool_t wpas_dbus_simple_array_array_property_getter(DBusMessageIter *iter,
							 const int type,
							 struct wpabuf **array,
							 size_t array_len,
							 DBusError *error);

DBusMessage * wpas_dbus_handler_create_interface(DBusMessage *message,
						 struct wpa_global *global);

DBusMessage * wpas_dbus_handler_remove_interface(DBusMessage *message,
						 struct wpa_global *global);

DBusMessage * wpas_dbus_handler_get_interface(DBusMessage *message,
					      struct wpa_global *global);

dbus_bool_t wpas_dbus_getter_debug_level(DBusMessageIter *iter,
					 DBusError *error,
					 void *user_data);

dbus_bool_t wpas_dbus_getter_debug_timestamp(DBusMessageIter *iter,
					     DBusError *error,
					     void *user_data);

dbus_bool_t wpas_dbus_getter_debug_show_keys(DBusMessageIter *iter,
					     DBusError *error,
					     void *user_data);

dbus_bool_t wpas_dbus_setter_debug_level(DBusMessageIter *iter,
					 DBusError *error, void *user_data);

dbus_bool_t wpas_dbus_setter_debug_timestamp(DBusMessageIter *iter,
					     DBusError *error,
					     void *user_data);

dbus_bool_t wpas_dbus_setter_debug_show_keys(DBusMessageIter *iter,
					     DBusError *error,
					     void *user_data);

dbus_bool_t wpas_dbus_getter_interfaces(DBusMessageIter *iter,
					DBusError *error,
					void *user_data);

dbus_bool_t wpas_dbus_getter_eap_methods(DBusMessageIter *iter,
					 DBusError *error, void *user_data);

dbus_bool_t wpas_dbus_getter_global_capabilities(DBusMessageIter *iter,
						 DBusError *error,
						 void *user_data);

DBusMessage * wpas_dbus_handler_scan(DBusMessage *message,
				     struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_signal_poll(DBusMessage *message,
					    struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_disconnect(DBusMessage *message,
					   struct wpa_supplicant *wpa_s);

dbus_bool_t set_network_properties(struct wpa_supplicant *wpa_s,
				   struct wpa_ssid *ssid,
				   DBusMessageIter *iter,
				   DBusError *error);

DBusMessage * wpas_dbus_handler_add_network(DBusMessage *message,
					    struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_reassociate(DBusMessage *message,
					    struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_reattach(DBusMessage *message,
					 struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_reconnect(DBusMessage *message,
					  struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_remove_network(DBusMessage *message,
					       struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_remove_all_networks(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_select_network(DBusMessage *message,
					       struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_network_reply(DBusMessage *message,
					      struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_add_blob(DBusMessage *message,
					 struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_get_blob(DBusMessage *message,
					 struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_remove_blob(DBusMessage *message,
					    struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_set_pkcs11_engine_and_module_path(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_flush_bss(DBusMessage *message,
					  struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_autoscan(DBusMessage *message,
					 struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_eap_logoff(DBusMessage *message,
					   struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_eap_logon(DBusMessage *message,
					  struct wpa_supplicant *wpa_s);

dbus_bool_t wpas_dbus_getter_capabilities(DBusMessageIter *iter,
					  DBusError *error, void *user_data);

dbus_bool_t wpas_dbus_getter_state(DBusMessageIter *iter, DBusError *error,
				   void *user_data);

dbus_bool_t wpas_dbus_getter_scanning(DBusMessageIter *iter, DBusError *error,
				      void *user_data);

dbus_bool_t wpas_dbus_getter_ap_scan(DBusMessageIter *iter, DBusError *error,
				     void *user_data);

dbus_bool_t wpas_dbus_setter_ap_scan(DBusMessageIter *iter, DBusError *error,
				     void *user_data);

dbus_bool_t wpas_dbus_getter_fast_reauth(DBusMessageIter *iter,
					 DBusError *error,
					 void *user_data);

dbus_bool_t wpas_dbus_setter_fast_reauth(DBusMessageIter *iter,
					 DBusError *error,
					 void *user_data);

dbus_bool_t wpas_dbus_getter_disconnect_reason(DBusMessageIter *iter,
					       DBusError *error,
					       void *user_data);

dbus_bool_t wpas_dbus_getter_bss_expire_age(DBusMessageIter *iter,
					    DBusError *error, void *user_data);

dbus_bool_t wpas_dbus_setter_bss_expire_age(DBusMessageIter *iter,
					    DBusError *error,
					    void *user_data);

dbus_bool_t wpas_dbus_getter_bss_expire_count(DBusMessageIter *iter,
					      DBusError *error,
					      void *user_data);

dbus_bool_t wpas_dbus_setter_bss_expire_count(DBusMessageIter *iter,
					      DBusError *error,
					      void *user_data);

dbus_bool_t wpas_dbus_getter_country(DBusMessageIter *iter, DBusError *error,
				     void *user_data);

dbus_bool_t wpas_dbus_setter_country(DBusMessageIter *iter, DBusError *error,
				     void *user_data);

dbus_bool_t wpas_dbus_getter_scan_interval(DBusMessageIter *iter,
					   DBusError *error,
					   void *user_data);

dbus_bool_t wpas_dbus_setter_scan_interval(DBusMessageIter *iter,
					   DBusError *error,
					   void *user_data);

dbus_bool_t wpas_dbus_getter_ifname(DBusMessageIter *iter, DBusError *error,
				    void *user_data);

dbus_bool_t wpas_dbus_getter_driver(DBusMessageIter *iter, DBusError *error,
				    void *user_data);

dbus_bool_t wpas_dbus_getter_bridge_ifname(DBusMessageIter *iter,
					   DBusError *error,
					   void *user_data);

dbus_bool_t wpas_dbus_getter_current_bss(DBusMessageIter *iter,
					 DBusError *error,
					 void *user_data);

dbus_bool_t wpas_dbus_getter_current_network(DBusMessageIter *iter,
					     DBusError *error,
					     void *user_data);

dbus_bool_t wpas_dbus_getter_current_auth_mode(DBusMessageIter *iter,
					       DBusError *error,
					       void *user_data);

dbus_bool_t wpas_dbus_getter_bsss(DBusMessageIter *iter, DBusError *error,
				  void *user_data);

dbus_bool_t wpas_dbus_getter_networks(DBusMessageIter *iter, DBusError *error,
				      void *user_data);

dbus_bool_t wpas_dbus_getter_pkcs11_engine_path(DBusMessageIter *iter,
						DBusError *error,
						void *user_data);

dbus_bool_t wpas_dbus_getter_pkcs11_module_path(DBusMessageIter *iter,
						DBusError *error,
						void *user_data);

dbus_bool_t wpas_dbus_getter_blobs(DBusMessageIter *iter, DBusError *error,
				   void *user_data);

dbus_bool_t wpas_dbus_getter_bss_bssid(DBusMessageIter *iter, DBusError *error,
				       void *user_data);

dbus_bool_t wpas_dbus_getter_bss_ssid(DBusMessageIter *iter, DBusError *error,
				      void *user_data);

dbus_bool_t wpas_dbus_getter_bss_privacy(DBusMessageIter *iter,
					 DBusError *error, void *user_data);

dbus_bool_t wpas_dbus_getter_bss_mode(DBusMessageIter *iter, DBusError *error,
				      void *user_data);

dbus_bool_t wpas_dbus_getter_bss_signal(DBusMessageIter *iter,
					DBusError *error, void *user_data);

dbus_bool_t wpas_dbus_getter_bss_frequency(DBusMessageIter *iter,
					   DBusError *error, void *user_data);

dbus_bool_t wpas_dbus_getter_bss_rates(DBusMessageIter *iter,
				       DBusError *error, void *user_data);

dbus_bool_t wpas_dbus_getter_bss_wpa(DBusMessageIter *iter, DBusError *error,
				     void *user_data);

dbus_bool_t wpas_dbus_getter_bss_rsn(DBusMessageIter *iter, DBusError *error,
				     void *user_data);

dbus_bool_t wpas_dbus_getter_bss_wps(DBusMessageIter *iter, DBusError *error,
				     void *user_data);

dbus_bool_t wpas_dbus_getter_bss_ies(DBusMessageIter *iter, DBusError *error,
				     void *user_data);

dbus_bool_t wpas_dbus_getter_bss_age(DBusMessageIter *iter, DBusError *error,
				     void *user_data);

dbus_bool_t wpas_dbus_getter_enabled(DBusMessageIter *iter, DBusError *error,
				     void *user_data);

dbus_bool_t wpas_dbus_setter_enabled(DBusMessageIter *iter, DBusError *error,
				     void *user_data);

dbus_bool_t wpas_dbus_getter_network_properties(DBusMessageIter *iter,
						DBusError *error,
						void *user_data);

dbus_bool_t wpas_dbus_setter_network_properties(DBusMessageIter *iter,
						DBusError *error,
						void *user_data);

DBusMessage * wpas_dbus_handler_wps_start(DBusMessage *message,
					  struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_wps_cancel(DBusMessage *message,
					   struct wpa_supplicant *wpa_s);

dbus_bool_t wpas_dbus_getter_process_credentials(DBusMessageIter *iter,
	DBusError *error, void *user_data);

dbus_bool_t wpas_dbus_setter_process_credentials(DBusMessageIter *iter,
						 DBusError *error,
						 void *user_data);

dbus_bool_t wpas_dbus_getter_config_methods(DBusMessageIter *iter,
					    DBusError *error,
					    void *user_data);

dbus_bool_t wpas_dbus_setter_config_methods(DBusMessageIter *iter,
					    DBusError *error,
					    void *user_data);

DBusMessage * wpas_dbus_handler_tdls_discover(DBusMessage *message,
					      struct wpa_supplicant *wpa_s);
DBusMessage * wpas_dbus_handler_tdls_setup(DBusMessage *message,
					   struct wpa_supplicant *wpa_s);
DBusMessage * wpas_dbus_handler_tdls_status(DBusMessage *message,
					    struct wpa_supplicant *wpa_s);
DBusMessage * wpas_dbus_handler_tdls_teardown(DBusMessage *message,
					      struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_error_invalid_args(DBusMessage *message,
					   const char *arg);
DBusMessage * wpas_dbus_error_unknown_error(DBusMessage *message,
					    const char *arg);
DBusMessage * wpas_dbus_error_no_memory(DBusMessage *message);

DBusMessage * wpas_dbus_handler_subscribe_preq(
	DBusMessage *message, struct wpa_supplicant *wpa_s);
DBusMessage * wpas_dbus_handler_unsubscribe_preq(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

#endif /* CTRL_IFACE_DBUS_HANDLERS_NEW_H */
