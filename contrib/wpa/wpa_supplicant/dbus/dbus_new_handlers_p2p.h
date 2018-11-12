/*
 * WPA Supplicant / dbus-based control interface for p2p
 * Copyright (c) 2011-2012, Intel Corporation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef DBUS_NEW_HANDLERS_P2P_H
#define DBUS_NEW_HANDLERS_P2P_H

struct peer_handler_args {
	struct wpa_supplicant *wpa_s;
	u8 p2p_device_addr[ETH_ALEN];
};

/*
 * P2P Device methods
 */

DBusMessage *wpas_dbus_handler_p2p_find(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

DBusMessage *wpas_dbus_handler_p2p_stop_find(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

DBusMessage *wpas_dbus_handler_p2p_rejectpeer(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

DBusMessage *wpas_dbus_handler_p2p_listen(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

DBusMessage *wpas_dbus_handler_p2p_extendedlisten(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

DBusMessage *wpas_dbus_handler_p2p_presence_request(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

DBusMessage *wpas_dbus_handler_p2p_prov_disc_req(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

DBusMessage *wpas_dbus_handler_p2p_group_add(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

DBusMessage *wpas_dbus_handler_p2p_connect(
		DBusMessage *message,
		struct wpa_supplicant *wpa_s);

DBusMessage *wpas_dbus_handler_p2p_invite(
		DBusMessage *message,
		struct wpa_supplicant *wpa_s);

DBusMessage *wpas_dbus_handler_p2p_disconnect(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

DBusMessage *wpas_dbus_handler_p2p_flush(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

DBusMessage *wpas_dbus_handler_p2p_add_service(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

DBusMessage *wpas_dbus_handler_p2p_delete_service(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

DBusMessage *wpas_dbus_handler_p2p_flush_service(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

DBusMessage *wpas_dbus_handler_p2p_service_sd_req(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

DBusMessage *wpas_dbus_handler_p2p_service_sd_res(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

DBusMessage *wpas_dbus_handler_p2p_service_sd_cancel_req(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

DBusMessage *wpas_dbus_handler_p2p_service_update(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

DBusMessage *wpas_dbus_handler_p2p_serv_disc_external(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

/*
 * P2P Device property accessor methods.
 */
dbus_bool_t wpas_dbus_setter_p2p_device_config(DBusMessageIter *iter,
					       DBusError *error,
					       void *user_data);

dbus_bool_t wpas_dbus_getter_p2p_device_config(DBusMessageIter *iter,
					       DBusError *error,
					       void *user_data);

dbus_bool_t wpas_dbus_getter_p2p_peers(DBusMessageIter *iter, DBusError *error,
				       void *user_data);

dbus_bool_t wpas_dbus_getter_p2p_role(DBusMessageIter *iter, DBusError *error,
				      void *user_data);

dbus_bool_t wpas_dbus_getter_p2p_group(DBusMessageIter *iter, DBusError *error,
				       void *user_data);

dbus_bool_t wpas_dbus_getter_p2p_peergo(DBusMessageIter *iter,
					DBusError *error,
					void *user_data);

/*
 * P2P Peer properties.
 */

dbus_bool_t wpas_dbus_getter_p2p_peer_device_name(DBusMessageIter *iter,
						  DBusError *error,
						  void *user_data);

dbus_bool_t wpas_dbus_getter_p2p_peer_primary_device_type(
	DBusMessageIter *iter, DBusError *error, void *user_data);

dbus_bool_t wpas_dbus_getter_p2p_peer_config_method(DBusMessageIter *iter,
						    DBusError *error,
						    void *user_data);

dbus_bool_t wpas_dbus_getter_p2p_peer_level(DBusMessageIter *iter,
					    DBusError *error,
					    void *user_data);

dbus_bool_t wpas_dbus_getter_p2p_peer_device_capability(DBusMessageIter *iter,
							DBusError *error,
							void *user_data);

dbus_bool_t wpas_dbus_getter_p2p_peer_group_capability(DBusMessageIter *iter,
						       DBusError *error,
						       void *user_data);

dbus_bool_t wpas_dbus_getter_p2p_peer_secondary_device_types(
	DBusMessageIter *iter, DBusError *error, void *user_data);

dbus_bool_t wpas_dbus_getter_p2p_peer_vendor_extension(DBusMessageIter *iter,
						       DBusError *error,
						       void *user_data);

dbus_bool_t wpas_dbus_getter_p2p_peer_ies(DBusMessageIter *iter,
					  DBusError *error,
					  void *user_data);

dbus_bool_t wpas_dbus_getter_p2p_peer_device_address(DBusMessageIter *iter,
						     DBusError *error,
						     void *user_data);

dbus_bool_t wpas_dbus_getter_p2p_peer_groups(DBusMessageIter *iter,
					     DBusError *error,
					     void *user_data);

/*
 * P2P Group properties
 */

dbus_bool_t wpas_dbus_getter_p2p_group_members(DBusMessageIter *iter,
					       DBusError *error,
					       void *user_data);

dbus_bool_t wpas_dbus_getter_p2p_group_ssid(DBusMessageIter *iter,
					    DBusError *error,
					    void *user_data);

dbus_bool_t wpas_dbus_getter_p2p_group_bssid(DBusMessageIter *iter,
					     DBusError *error,
					     void *user_data);

dbus_bool_t wpas_dbus_getter_p2p_group_frequency(DBusMessageIter *iter,
						 DBusError *error,
						 void *user_data);

dbus_bool_t wpas_dbus_getter_p2p_group_passphrase(DBusMessageIter *iter,
						  DBusError *error,
						  void *user_data);

dbus_bool_t wpas_dbus_getter_p2p_group_psk(DBusMessageIter *iter,
					   DBusError *error,
					   void *user_data);

dbus_bool_t wpas_dbus_getter_p2p_group_vendor_ext(DBusMessageIter *iter,
						  DBusError *error,
						  void *user_data);

dbus_bool_t wpas_dbus_setter_p2p_group_vendor_ext(DBusMessageIter *iter,
						  DBusError *error,
						  void *user_data);

/*
 * P2P Persistent Groups and properties
 */

dbus_bool_t wpas_dbus_getter_persistent_groups(DBusMessageIter *iter,
					       DBusError *error,
					       void *user_data);

dbus_bool_t wpas_dbus_getter_persistent_group_properties(DBusMessageIter *iter,
	DBusError *error, void *user_data);

dbus_bool_t wpas_dbus_setter_persistent_group_properties(DBusMessageIter *iter,
							 DBusError *error,
							 void *user_data);

DBusMessage * wpas_dbus_handler_add_persistent_group(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_remove_persistent_group(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

DBusMessage * wpas_dbus_handler_remove_all_persistent_groups(
	DBusMessage *message, struct wpa_supplicant *wpa_s);

#ifdef CONFIG_WIFI_DISPLAY

dbus_bool_t wpas_dbus_getter_global_wfd_ies(DBusMessageIter *iter,
					    DBusError *error,
					    void *user_data);

dbus_bool_t wpas_dbus_setter_global_wfd_ies(DBusMessageIter *iter,
					    DBusError *error,
					    void *user_data);

#endif /* CONFIG_WIFI_DISPLAY */

#endif /* DBUS_NEW_HANDLERS_P2P_H */
