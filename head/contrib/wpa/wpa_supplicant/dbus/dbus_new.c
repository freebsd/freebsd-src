/*
 * WPA Supplicant / dbus-based control interface
 * Copyright (c) 2006, Dan Williams <dcbw@redhat.com> and Red Hat, Inc.
 * Copyright (c) 2009-2010, Witold Sowa <witold.sowa@gmail.com>
 * Copyright (c) 2009, Jouni Malinen <j@w1.fi>
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

#include "includes.h"

#include "common.h"
#include "wps/wps.h"
#include "../config.h"
#include "../wpa_supplicant_i.h"
#include "../bss.h"
#include "dbus_new_helpers.h"
#include "dbus_dict_helpers.h"
#include "dbus_new.h"
#include "dbus_new_handlers.h"
#include "dbus_common.h"
#include "dbus_common_i.h"


/**
 * wpas_dbus_signal_interface - Send a interface related event signal
 * @wpa_s: %wpa_supplicant network interface data
 * @sig_name: signal name - InterfaceAdded or InterfaceRemoved
 * @properties: Whether to add second argument with object properties
 *
 * Notify listeners about event related with interface
 */
static void wpas_dbus_signal_interface(struct wpa_supplicant *wpa_s,
				       const char *sig_name, int properties)
{
	struct wpas_dbus_priv *iface;
	DBusMessage *msg;
	DBusMessageIter iter, iter_dict;

	iface = wpa_s->global->dbus;

	/* Do nothing if the control interface is not turned on */
	if (iface == NULL)
		return;

	msg = dbus_message_new_signal(WPAS_DBUS_NEW_PATH,
				      WPAS_DBUS_NEW_INTERFACE, sig_name);
	if (msg == NULL)
		return;

	dbus_message_iter_init_append(msg, &iter);
	if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH,
					    &wpa_s->dbus_new_path))
		goto err;

	if (properties) {
		if (!wpa_dbus_dict_open_write(&iter, &iter_dict))
			goto err;

		wpa_dbus_get_object_properties(iface, wpa_s->dbus_new_path,
					       WPAS_DBUS_NEW_IFACE_INTERFACE,
					       &iter_dict);

		if (!wpa_dbus_dict_close_write(&iter, &iter_dict))
			goto err;
	}

	dbus_connection_send(iface->con, msg, NULL);
	dbus_message_unref(msg);
	return;

err:
	wpa_printf(MSG_ERROR, "dbus: Failed to construct signal");
	dbus_message_unref(msg);
}


/**
 * wpas_dbus_signal_interface_added - Send a interface created signal
 * @wpa_s: %wpa_supplicant network interface data
 *
 * Notify listeners about creating new interface
 */
static void wpas_dbus_signal_interface_added(struct wpa_supplicant *wpa_s)
{
	wpas_dbus_signal_interface(wpa_s, "InterfaceAdded", TRUE);
}


/**
 * wpas_dbus_signal_interface_removed - Send a interface removed signal
 * @wpa_s: %wpa_supplicant network interface data
 *
 * Notify listeners about removing interface
 */
static void wpas_dbus_signal_interface_removed(struct wpa_supplicant *wpa_s)
{
	wpas_dbus_signal_interface(wpa_s, "InterfaceRemoved", FALSE);

}


/**
 * wpas_dbus_signal_scan_done - send scan done signal
 * @wpa_s: %wpa_supplicant network interface data
 * @success: indicates if scanning succeed or failed
 *
 * Notify listeners about finishing a scan
 */
void wpas_dbus_signal_scan_done(struct wpa_supplicant *wpa_s, int success)
{
	struct wpas_dbus_priv *iface;
	DBusMessage *msg;
	dbus_bool_t succ;

	iface = wpa_s->global->dbus;

	/* Do nothing if the control interface is not turned on */
	if (iface == NULL)
		return;

	msg = dbus_message_new_signal(wpa_s->dbus_new_path,
				      WPAS_DBUS_NEW_IFACE_INTERFACE,
				      "ScanDone");
	if (msg == NULL)
		return;

	succ = success ? TRUE : FALSE;
	if (dbus_message_append_args(msg, DBUS_TYPE_BOOLEAN, &succ,
				     DBUS_TYPE_INVALID))
		dbus_connection_send(iface->con, msg, NULL);
	else
		wpa_printf(MSG_ERROR, "dbus: Failed to construct signal");
	dbus_message_unref(msg);
}


/**
 * wpas_dbus_signal_blob - Send a BSS related event signal
 * @wpa_s: %wpa_supplicant network interface data
 * @bss_obj_path: BSS object path
 * @sig_name: signal name - BSSAdded or BSSRemoved
 * @properties: Whether to add second argument with object properties
 *
 * Notify listeners about event related with BSS
 */
static void wpas_dbus_signal_bss(struct wpa_supplicant *wpa_s,
				 const char *bss_obj_path,
				 const char *sig_name, int properties)
{
	struct wpas_dbus_priv *iface;
	DBusMessage *msg;
	DBusMessageIter iter, iter_dict;

	iface = wpa_s->global->dbus;

	/* Do nothing if the control interface is not turned on */
	if (iface == NULL)
		return;

	msg = dbus_message_new_signal(wpa_s->dbus_new_path,
				      WPAS_DBUS_NEW_IFACE_INTERFACE,
				      sig_name);
	if (msg == NULL)
		return;

	dbus_message_iter_init_append(msg, &iter);
	if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH,
					    &bss_obj_path))
		goto err;

	if (properties) {
		if (!wpa_dbus_dict_open_write(&iter, &iter_dict))
			goto err;

		wpa_dbus_get_object_properties(iface, bss_obj_path,
					       WPAS_DBUS_NEW_IFACE_BSS,
					       &iter_dict);

		if (!wpa_dbus_dict_close_write(&iter, &iter_dict))
			goto err;
	}

	dbus_connection_send(iface->con, msg, NULL);
	dbus_message_unref(msg);
	return;

err:
	wpa_printf(MSG_ERROR, "dbus: Failed to construct signal");
	dbus_message_unref(msg);
}


/**
 * wpas_dbus_signal_bss_added - Send a BSS added signal
 * @wpa_s: %wpa_supplicant network interface data
 * @bss_obj_path: new BSS object path
 *
 * Notify listeners about adding new BSS
 */
static void wpas_dbus_signal_bss_added(struct wpa_supplicant *wpa_s,
				       const char *bss_obj_path)
{
	wpas_dbus_signal_bss(wpa_s, bss_obj_path, "BSSAdded", TRUE);
}


/**
 * wpas_dbus_signal_bss_removed - Send a BSS removed signal
 * @wpa_s: %wpa_supplicant network interface data
 * @bss_obj_path: BSS object path
 *
 * Notify listeners about removing BSS
 */
static void wpas_dbus_signal_bss_removed(struct wpa_supplicant *wpa_s,
					 const char *bss_obj_path)
{
	wpas_dbus_signal_bss(wpa_s, bss_obj_path, "BSSRemoved", FALSE);
}


/**
 * wpas_dbus_signal_blob - Send a blob related event signal
 * @wpa_s: %wpa_supplicant network interface data
 * @name: blob name
 * @sig_name: signal name - BlobAdded or BlobRemoved
 *
 * Notify listeners about event related with blob
 */
static void wpas_dbus_signal_blob(struct wpa_supplicant *wpa_s,
				  const char *name, const char *sig_name)
{
	struct wpas_dbus_priv *iface;
	DBusMessage *msg;

	iface = wpa_s->global->dbus;

	/* Do nothing if the control interface is not turned on */
	if (iface == NULL)
		return;

	msg = dbus_message_new_signal(wpa_s->dbus_new_path,
				      WPAS_DBUS_NEW_IFACE_INTERFACE,
				      sig_name);
	if (msg == NULL)
		return;

	if (dbus_message_append_args(msg, DBUS_TYPE_STRING, &name,
				     DBUS_TYPE_INVALID))
		dbus_connection_send(iface->con, msg, NULL);
	else
		wpa_printf(MSG_ERROR, "dbus: Failed to construct signal");
	dbus_message_unref(msg);
}


/**
 * wpas_dbus_signal_blob_added - Send a blob added signal
 * @wpa_s: %wpa_supplicant network interface data
 * @name: blob name
 *
 * Notify listeners about adding a new blob
 */
void wpas_dbus_signal_blob_added(struct wpa_supplicant *wpa_s,
				 const char *name)
{
	wpas_dbus_signal_blob(wpa_s, name, "BlobAdded");
}


/**
 * wpas_dbus_signal_blob_removed - Send a blob removed signal
 * @wpa_s: %wpa_supplicant network interface data
 * @name: blob name
 *
 * Notify listeners about removing blob
 */
void wpas_dbus_signal_blob_removed(struct wpa_supplicant *wpa_s,
				   const char *name)
{
	wpas_dbus_signal_blob(wpa_s, name, "BlobRemoved");
}


/**
 * wpas_dbus_signal_network - Send a network related event signal
 * @wpa_s: %wpa_supplicant network interface data
 * @id: new network id
 * @sig_name: signal name - NetworkAdded, NetworkRemoved or NetworkSelected
 * @properties: determines if add second argument with object properties
 *
 * Notify listeners about event related with configured network
 */
static void wpas_dbus_signal_network(struct wpa_supplicant *wpa_s,
				     int id, const char *sig_name,
				     int properties)
{
	struct wpas_dbus_priv *iface;
	DBusMessage *msg;
	DBusMessageIter iter, iter_dict;
	char net_obj_path[WPAS_DBUS_OBJECT_PATH_MAX], *path;

	iface = wpa_s->global->dbus;

	/* Do nothing if the control interface is not turned on */
	if (iface == NULL)
		return;

	os_snprintf(net_obj_path, WPAS_DBUS_OBJECT_PATH_MAX,
		    "%s/" WPAS_DBUS_NEW_NETWORKS_PART "/%u",
		    wpa_s->dbus_new_path, id);

	msg = dbus_message_new_signal(wpa_s->dbus_new_path,
				      WPAS_DBUS_NEW_IFACE_INTERFACE,
				      sig_name);
	if (msg == NULL)
		return;

	dbus_message_iter_init_append(msg, &iter);
	path = net_obj_path;
	if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH,
					    &path))
		goto err;

	if (properties) {
		if (!wpa_dbus_dict_open_write(&iter, &iter_dict))
			goto err;

		wpa_dbus_get_object_properties(iface, net_obj_path,
					       WPAS_DBUS_NEW_IFACE_NETWORK,
					       &iter_dict);

		if (!wpa_dbus_dict_close_write(&iter, &iter_dict))
			goto err;
	}

	dbus_connection_send(iface->con, msg, NULL);

	dbus_message_unref(msg);
	return;

err:
	wpa_printf(MSG_ERROR, "dbus: Failed to construct signal");
	dbus_message_unref(msg);
}


/**
 * wpas_dbus_signal_network_added - Send a network added signal
 * @wpa_s: %wpa_supplicant network interface data
 * @id: new network id
 *
 * Notify listeners about adding new network
 */
static void wpas_dbus_signal_network_added(struct wpa_supplicant *wpa_s,
					   int id)
{
	wpas_dbus_signal_network(wpa_s, id, "NetworkAdded", TRUE);
}


/**
 * wpas_dbus_signal_network_removed - Send a network removed signal
 * @wpa_s: %wpa_supplicant network interface data
 * @id: network id
 *
 * Notify listeners about removing a network
 */
static void wpas_dbus_signal_network_removed(struct wpa_supplicant *wpa_s,
					     int id)
{
	wpas_dbus_signal_network(wpa_s, id, "NetworkRemoved", FALSE);
}


/**
 * wpas_dbus_signal_network_selected - Send a network selected signal
 * @wpa_s: %wpa_supplicant network interface data
 * @id: network id
 *
 * Notify listeners about selecting a network
 */
void wpas_dbus_signal_network_selected(struct wpa_supplicant *wpa_s, int id)
{
	wpas_dbus_signal_network(wpa_s, id, "NetworkSelected", FALSE);
}


/**
 * wpas_dbus_signal_network_enabled_changed - Signals Enabled property changes
 * @wpa_s: %wpa_supplicant network interface data
 * @ssid: configured network which Enabled property has changed
 *
 * Sends PropertyChanged signals containing new value of Enabled property
 * for specified network
 */
void wpas_dbus_signal_network_enabled_changed(struct wpa_supplicant *wpa_s,
					      struct wpa_ssid *ssid)
{

	char path[WPAS_DBUS_OBJECT_PATH_MAX];
	os_snprintf(path, WPAS_DBUS_OBJECT_PATH_MAX,
		    "%s/" WPAS_DBUS_NEW_NETWORKS_PART "/%d",
		    wpa_s->dbus_new_path, ssid->id);

	wpa_dbus_mark_property_changed(wpa_s->global->dbus, path,
				       WPAS_DBUS_NEW_IFACE_NETWORK, "Enabled");
}


#ifdef CONFIG_WPS

/**
 * wpas_dbus_signal_wps_event_success - Signals Success WPS event
 * @wpa_s: %wpa_supplicant network interface data
 *
 * Sends Event dbus signal with name "success" and empty dict as arguments
 */
void wpas_dbus_signal_wps_event_success(struct wpa_supplicant *wpa_s)
{

	DBusMessage *msg;
	DBusMessageIter iter, dict_iter;
	struct wpas_dbus_priv *iface;
	char *key = "success";

	iface = wpa_s->global->dbus;

	/* Do nothing if the control interface is not turned on */
	if (iface == NULL)
		return;

	msg = dbus_message_new_signal(wpa_s->dbus_new_path,
				      WPAS_DBUS_NEW_IFACE_WPS, "Event");
	if (msg == NULL)
		return;

	dbus_message_iter_init_append(msg, &iter);

	if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &key) ||
	    !wpa_dbus_dict_open_write(&iter, &dict_iter) ||
	    !wpa_dbus_dict_close_write(&iter, &dict_iter))
		wpa_printf(MSG_ERROR, "dbus: Failed to construct signal");
	else
		dbus_connection_send(iface->con, msg, NULL);

	dbus_message_unref(msg);
}


/**
 * wpas_dbus_signal_wps_event_fail - Signals Fail WPS event
 * @wpa_s: %wpa_supplicant network interface data
 *
 * Sends Event dbus signal with name "fail" and dictionary containing
 * "msg field with fail message number (int32) as arguments
 */
void wpas_dbus_signal_wps_event_fail(struct wpa_supplicant *wpa_s,
				     struct wps_event_fail *fail)
{

	DBusMessage *msg;
	DBusMessageIter iter, dict_iter;
	struct wpas_dbus_priv *iface;
	char *key = "fail";

	iface = wpa_s->global->dbus;

	/* Do nothing if the control interface is not turned on */
	if (iface == NULL)
		return;

	msg = dbus_message_new_signal(wpa_s->dbus_new_path,
				      WPAS_DBUS_NEW_IFACE_WPS, "Event");
	if (msg == NULL)
		return;

	dbus_message_iter_init_append(msg, &iter);

	if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &key) ||
	    !wpa_dbus_dict_open_write(&iter, &dict_iter) ||
	    !wpa_dbus_dict_append_int32(&dict_iter, "msg", fail->msg) ||
	    !wpa_dbus_dict_close_write(&iter, &dict_iter))
		wpa_printf(MSG_ERROR, "dbus: Failed to construct signal");
	else
		dbus_connection_send(iface->con, msg, NULL);

	dbus_message_unref(msg);
}


/**
 * wpas_dbus_signal_wps_event_m2d - Signals M2D WPS event
 * @wpa_s: %wpa_supplicant network interface data
 *
 * Sends Event dbus signal with name "m2d" and dictionary containing
 * fields of wps_event_m2d structure.
 */
void wpas_dbus_signal_wps_event_m2d(struct wpa_supplicant *wpa_s,
				    struct wps_event_m2d *m2d)
{

	DBusMessage *msg;
	DBusMessageIter iter, dict_iter;
	struct wpas_dbus_priv *iface;
	char *key = "m2d";

	iface = wpa_s->global->dbus;

	/* Do nothing if the control interface is not turned on */
	if (iface == NULL)
		return;

	msg = dbus_message_new_signal(wpa_s->dbus_new_path,
				      WPAS_DBUS_NEW_IFACE_WPS, "Event");
	if (msg == NULL)
		return;

	dbus_message_iter_init_append(msg, &iter);

	if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &key) ||
	    !wpa_dbus_dict_open_write(&iter, &dict_iter) ||
	    !wpa_dbus_dict_append_uint16(&dict_iter, "config_methods",
					 m2d->config_methods) ||
	    !wpa_dbus_dict_append_byte_array(&dict_iter, "manufacturer",
					     (const char *) m2d->manufacturer,
					     m2d->manufacturer_len) ||
	    !wpa_dbus_dict_append_byte_array(&dict_iter, "model_name",
					     (const char *) m2d->model_name,
					     m2d->model_name_len) ||
	    !wpa_dbus_dict_append_byte_array(&dict_iter, "model_number",
					     (const char *) m2d->model_number,
					     m2d->model_number_len) ||
	    !wpa_dbus_dict_append_byte_array(&dict_iter, "serial_number",
					     (const char *)
					     m2d->serial_number,
					     m2d->serial_number_len) ||
	    !wpa_dbus_dict_append_byte_array(&dict_iter, "dev_name",
					     (const char *) m2d->dev_name,
					     m2d->dev_name_len) ||
	    !wpa_dbus_dict_append_byte_array(&dict_iter, "primary_dev_type",
					     (const char *)
					     m2d->primary_dev_type, 8) ||
	    !wpa_dbus_dict_append_uint16(&dict_iter, "config_error",
					 m2d->config_error) ||
	    !wpa_dbus_dict_append_uint16(&dict_iter, "dev_password_id",
					 m2d->dev_password_id) ||
	    !wpa_dbus_dict_close_write(&iter, &dict_iter))
		wpa_printf(MSG_ERROR, "dbus: Failed to construct signal");
	else
		dbus_connection_send(iface->con, msg, NULL);

	dbus_message_unref(msg);
}


/**
 * wpas_dbus_signal_wps_cred - Signals new credentials
 * @wpa_s: %wpa_supplicant network interface data
 *
 * Sends signal with credentials in directory argument
 */
void wpas_dbus_signal_wps_cred(struct wpa_supplicant *wpa_s,
			       const struct wps_credential *cred)
{
	DBusMessage *msg;
	DBusMessageIter iter, dict_iter;
	struct wpas_dbus_priv *iface;
	char *auth_type[6]; /* we have six possible authorization types */
	int at_num = 0;
	char *encr_type[4]; /* we have four possible encryption types */
	int et_num = 0;

	iface = wpa_s->global->dbus;

	/* Do nothing if the control interface is not turned on */
	if (iface == NULL)
		return;

	msg = dbus_message_new_signal(wpa_s->dbus_new_path,
				      WPAS_DBUS_NEW_IFACE_WPS,
				      "Credentials");
	if (msg == NULL)
		return;

	dbus_message_iter_init_append(msg, &iter);
	if (!wpa_dbus_dict_open_write(&iter, &dict_iter))
		goto nomem;

	if (cred->auth_type & WPS_AUTH_OPEN)
		auth_type[at_num++] = "open";
	if (cred->auth_type & WPS_AUTH_WPAPSK)
		auth_type[at_num++] = "wpa-psk";
	if (cred->auth_type & WPS_AUTH_SHARED)
		auth_type[at_num++] = "shared";
	if (cred->auth_type & WPS_AUTH_WPA)
		auth_type[at_num++] = "wpa-eap";
	if (cred->auth_type & WPS_AUTH_WPA2)
		auth_type[at_num++] = "wpa2-eap";
	if (cred->auth_type & WPS_AUTH_WPA2PSK)
		auth_type[at_num++] =
		"wpa2-psk";

	if (cred->encr_type & WPS_ENCR_NONE)
		encr_type[et_num++] = "none";
	if (cred->encr_type & WPS_ENCR_WEP)
		encr_type[et_num++] = "wep";
	if (cred->encr_type & WPS_ENCR_TKIP)
		encr_type[et_num++] = "tkip";
	if (cred->encr_type & WPS_ENCR_AES)
		encr_type[et_num++] = "aes";

	if (wpa_s->current_ssid) {
		if (!wpa_dbus_dict_append_byte_array(
			    &dict_iter, "BSSID",
			    (const char *) wpa_s->current_ssid->bssid,
			    ETH_ALEN))
			goto nomem;
	}

	if (!wpa_dbus_dict_append_byte_array(&dict_iter, "SSID",
					     (const char *) cred->ssid,
					     cred->ssid_len) ||
	    !wpa_dbus_dict_append_string_array(&dict_iter, "AuthType",
					       (const char **) auth_type,
					       at_num) ||
	    !wpa_dbus_dict_append_string_array(&dict_iter, "EncrType",
					       (const char **) encr_type,
					       et_num) ||
	    !wpa_dbus_dict_append_byte_array(&dict_iter, "Key",
					     (const char *) cred->key,
					     cred->key_len) ||
	    !wpa_dbus_dict_append_uint32(&dict_iter, "KeyIndex",
					 cred->key_idx) ||
	    !wpa_dbus_dict_close_write(&iter, &dict_iter))
		goto nomem;

	dbus_connection_send(iface->con, msg, NULL);

nomem:
	dbus_message_unref(msg);
}

#endif /* CONFIG_WPS */


/**
 * wpas_dbus_signal_prop_changed - Signals change of property
 * @wpa_s: %wpa_supplicant network interface data
 * @property: indicates which property has changed
 *
 * Sends ProertyChanged signals with path, interface and arguments
 * depending on which property has changed.
 */
void wpas_dbus_signal_prop_changed(struct wpa_supplicant *wpa_s,
				   enum wpas_dbus_prop property)
{
	WPADBusPropertyAccessor getter;
	char *prop;

	if (wpa_s->dbus_new_path == NULL)
		return; /* Skip signal since D-Bus setup is not yet ready */

	switch (property) {
	case WPAS_DBUS_PROP_AP_SCAN:
		getter = (WPADBusPropertyAccessor) wpas_dbus_getter_ap_scan;
		prop = "ApScan";
		break;
	case WPAS_DBUS_PROP_SCANNING:
		getter = (WPADBusPropertyAccessor) wpas_dbus_getter_scanning;
		prop = "Scanning";
		break;
	case WPAS_DBUS_PROP_STATE:
		getter = (WPADBusPropertyAccessor) wpas_dbus_getter_state;
		prop = "State";
		break;
	case WPAS_DBUS_PROP_CURRENT_BSS:
		getter = (WPADBusPropertyAccessor)
			wpas_dbus_getter_current_bss;
		prop = "CurrentBSS";
		break;
	case WPAS_DBUS_PROP_CURRENT_NETWORK:
		getter = (WPADBusPropertyAccessor)
			wpas_dbus_getter_current_network;
		prop = "CurrentNetwork";
		break;
	default:
		wpa_printf(MSG_ERROR, "dbus: %s: Unknown Property value %d",
			   __func__, property);
		return;
	}

	wpa_dbus_mark_property_changed(wpa_s->global->dbus,
				       wpa_s->dbus_new_path,
				       WPAS_DBUS_NEW_IFACE_INTERFACE, prop);
}


/**
 * wpas_dbus_bss_signal_prop_changed - Signals change of BSS property
 * @wpa_s: %wpa_supplicant network interface data
 * @property: indicates which property has changed
 * @id: unique BSS identifier
 *
 * Sends PropertyChanged signals with path, interface, and arguments depending
 * on which property has changed.
 */
void wpas_dbus_bss_signal_prop_changed(struct wpa_supplicant *wpa_s,
				       enum wpas_dbus_bss_prop property,
				       unsigned int id)
{
	char path[WPAS_DBUS_OBJECT_PATH_MAX];
	char *prop;

	switch (property) {
	case WPAS_DBUS_BSS_PROP_SIGNAL:
		prop = "Signal";
		break;
	case WPAS_DBUS_BSS_PROP_FREQ:
		prop = "Frequency";
		break;
	case WPAS_DBUS_BSS_PROP_MODE:
		prop = "Mode";
		break;
	case WPAS_DBUS_BSS_PROP_PRIVACY:
		prop = "Privacy";
		break;
	case WPAS_DBUS_BSS_PROP_RATES:
		prop = "Rates";
		break;
	case WPAS_DBUS_BSS_PROP_WPA:
		prop = "WPA";
		break;
	case WPAS_DBUS_BSS_PROP_RSN:
		prop = "RSN";
		break;
	case WPAS_DBUS_BSS_PROP_IES:
		prop = "IEs";
		break;
	default:
		wpa_printf(MSG_ERROR, "dbus: %s: Unknown Property value %d",
			   __func__, property);
		return;
	}

	os_snprintf(path, WPAS_DBUS_OBJECT_PATH_MAX,
		    "%s/" WPAS_DBUS_NEW_BSSIDS_PART "/%u",
		    wpa_s->dbus_new_path, id);

	wpa_dbus_mark_property_changed(wpa_s->global->dbus, path,
				       WPAS_DBUS_NEW_IFACE_BSS, prop);
}


/**
 * wpas_dbus_signal_debug_level_changed - Signals change of debug param
 * @global: wpa_global structure
 *
 * Sends ProertyChanged signals informing that debug level has changed.
 */
void wpas_dbus_signal_debug_level_changed(struct wpa_global *global)
{
	wpa_dbus_mark_property_changed(global->dbus, WPAS_DBUS_NEW_PATH,
				       WPAS_DBUS_NEW_INTERFACE,
				       "DebugLevel");
}


/**
 * wpas_dbus_signal_debug_timestamp_changed - Signals change of debug param
 * @global: wpa_global structure
 *
 * Sends ProertyChanged signals informing that debug timestamp has changed.
 */
void wpas_dbus_signal_debug_timestamp_changed(struct wpa_global *global)
{
	wpa_dbus_mark_property_changed(global->dbus, WPAS_DBUS_NEW_PATH,
				       WPAS_DBUS_NEW_INTERFACE,
				       "DebugTimestamp");
}


/**
 * wpas_dbus_signal_debug_show_keys_changed - Signals change of debug param
 * @global: wpa_global structure
 *
 * Sends ProertyChanged signals informing that debug show_keys has changed.
 */
void wpas_dbus_signal_debug_show_keys_changed(struct wpa_global *global)
{
	wpa_dbus_mark_property_changed(global->dbus, WPAS_DBUS_NEW_PATH,
				       WPAS_DBUS_NEW_INTERFACE,
				       "DebugShowKeys");
}


static void wpas_dbus_register(struct wpa_dbus_object_desc *obj_desc,
			       void *priv,
			       WPADBusArgumentFreeFunction priv_free,
			       const struct wpa_dbus_method_desc *methods,
			       const struct wpa_dbus_property_desc *properties,
			       const struct wpa_dbus_signal_desc *signals)
{
	int n;

	obj_desc->user_data = priv;
	obj_desc->user_data_free_func = priv_free;
	obj_desc->methods = methods;
	obj_desc->properties = properties;
	obj_desc->signals = signals;

	for (n = 0; properties && properties->dbus_property; properties++)
		n++;

	obj_desc->prop_changed_flags = os_zalloc(n);
	if (!obj_desc->prop_changed_flags)
		wpa_printf(MSG_DEBUG, "dbus: %s: can't register handlers",
			   __func__);
}


static const struct wpa_dbus_method_desc wpas_dbus_global_methods[] = {
	{ "CreateInterface", WPAS_DBUS_NEW_INTERFACE,
	  (WPADBusMethodHandler) &wpas_dbus_handler_create_interface,
	  {
		  { "args", "a{sv}", ARG_IN },
		  { "path", "o", ARG_OUT },
		  END_ARGS
	  }
	},
	{ "RemoveInterface", WPAS_DBUS_NEW_INTERFACE,
	  (WPADBusMethodHandler) &wpas_dbus_handler_remove_interface,
	  {
		  { "path", "o", ARG_IN },
		  END_ARGS
	  }
	},
	{ "GetInterface", WPAS_DBUS_NEW_INTERFACE,
	  (WPADBusMethodHandler) &wpas_dbus_handler_get_interface,
	  {
		  { "ifname", "s", ARG_IN },
		  { "path", "o", ARG_OUT },
		  END_ARGS
	  }
	},
	{ NULL, NULL, NULL, { END_ARGS } }
};

static const struct wpa_dbus_property_desc wpas_dbus_global_properties[] = {
	{ "DebugLevel", WPAS_DBUS_NEW_INTERFACE, "s",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_debug_level,
	  (WPADBusPropertyAccessor) wpas_dbus_setter_debug_level,
	  RW
	},
	{ "DebugTimestamp", WPAS_DBUS_NEW_INTERFACE, "b",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_debug_timestamp,
	  (WPADBusPropertyAccessor) wpas_dbus_setter_debug_timestamp,
	  RW
	},
	{ "DebugShowKeys", WPAS_DBUS_NEW_INTERFACE, "b",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_debug_show_keys,
	  (WPADBusPropertyAccessor) wpas_dbus_setter_debug_show_keys,
	  RW
	},
	{ "Interfaces", WPAS_DBUS_NEW_INTERFACE, "ao",
	  (WPADBusPropertyAccessor) &wpas_dbus_getter_interfaces,
	  NULL,
	  R
	},
	{ "EapMethods", WPAS_DBUS_NEW_INTERFACE, "as",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_eap_methods,
	  NULL,
	  R
	},
	{ NULL, NULL, NULL, NULL, NULL, 0 }
};

static const struct wpa_dbus_signal_desc wpas_dbus_global_signals[] = {
	{ "InterfaceAdded", WPAS_DBUS_NEW_INTERFACE,
	  {
		  { "path", "o", ARG_OUT },
		  { "properties", "a{sv}", ARG_OUT },
		  END_ARGS
	  }
	},
	{ "InterfaceRemoved", WPAS_DBUS_NEW_INTERFACE,
	  {
		  { "path", "o", ARG_OUT },
		  END_ARGS
	  }
	},
	{ "PropertiesChanged", WPAS_DBUS_NEW_INTERFACE,
	  {
		  { "properties", "a{sv}", ARG_OUT },
		  END_ARGS
	  }
	},
	{ NULL, NULL, { END_ARGS } }
};


/**
 * wpas_dbus_ctrl_iface_init - Initialize dbus control interface
 * @global: Pointer to global data from wpa_supplicant_init()
 * Returns: 0 on success or -1 on failure
 *
 * Initialize the dbus control interface for wpa_supplicantand and start
 * receiving commands from external programs over the bus.
 */
int wpas_dbus_ctrl_iface_init(struct wpas_dbus_priv *priv)
{
	struct wpa_dbus_object_desc *obj_desc;
	int ret;

	obj_desc = os_zalloc(sizeof(struct wpa_dbus_object_desc));
	if (!obj_desc) {
		wpa_printf(MSG_ERROR, "Not enough memory "
			   "to create object description");
		return -1;
	}

	wpas_dbus_register(obj_desc, priv->global, NULL,
			   wpas_dbus_global_methods,
			   wpas_dbus_global_properties,
			   wpas_dbus_global_signals);

	wpa_printf(MSG_DEBUG, "dbus: Register D-Bus object '%s'",
		   WPAS_DBUS_NEW_PATH);
	ret = wpa_dbus_ctrl_iface_init(priv, WPAS_DBUS_NEW_PATH,
				       WPAS_DBUS_NEW_SERVICE,
				       obj_desc);
	if (ret < 0)
		free_dbus_object_desc(obj_desc);
	else
		priv->dbus_new_initialized = 1;

	return ret;
}


/**
 * wpas_dbus_ctrl_iface_deinit - Deinitialize dbus ctrl interface for
 * wpa_supplicant
 * @iface: Pointer to dbus private data from wpas_dbus_init()
 *
 * Deinitialize the dbus control interface that was initialized with
 * wpas_dbus_ctrl_iface_init().
 */
void wpas_dbus_ctrl_iface_deinit(struct wpas_dbus_priv *iface)
{
	if (!iface->dbus_new_initialized)
		return;
	wpa_printf(MSG_DEBUG, "dbus: Unregister D-Bus object '%s'",
		   WPAS_DBUS_NEW_PATH);
	dbus_connection_unregister_object_path(iface->con,
					       WPAS_DBUS_NEW_PATH);
}


static void wpa_dbus_free(void *ptr)
{
	os_free(ptr);
}


static const struct wpa_dbus_property_desc wpas_dbus_network_properties[] = {
	{ "Properties", WPAS_DBUS_NEW_IFACE_NETWORK, "a{sv}",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_network_properties,
	  (WPADBusPropertyAccessor) wpas_dbus_setter_network_properties,
	  RW
	},
	{ "Enabled", WPAS_DBUS_NEW_IFACE_NETWORK, "b",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_enabled,
	  (WPADBusPropertyAccessor) wpas_dbus_setter_enabled,
	  RW
	},
	{ NULL, NULL, NULL, NULL, NULL, 0 }
};


static const struct wpa_dbus_signal_desc wpas_dbus_network_signals[] = {
	{ "PropertiesChanged", WPAS_DBUS_NEW_IFACE_NETWORK,
	  {
		  { "properties", "a{sv}", ARG_OUT },
		  END_ARGS
	  }
	},
	{ NULL, NULL, { END_ARGS } }
};


/**
 * wpas_dbus_register_network - Register a configured network with dbus
 * @wpa_s: wpa_supplicant interface structure
 * @ssid: network configuration data
 * Returns: 0 on success, -1 on failure
 *
 * Registers network representing object with dbus
 */
int wpas_dbus_register_network(struct wpa_supplicant *wpa_s,
			       struct wpa_ssid *ssid)
{
	struct wpas_dbus_priv *ctrl_iface;
	struct wpa_dbus_object_desc *obj_desc;
	struct network_handler_args *arg;
	char net_obj_path[WPAS_DBUS_OBJECT_PATH_MAX];

	/* Do nothing if the control interface is not turned on */
	if (wpa_s == NULL || wpa_s->global == NULL)
		return 0;
	ctrl_iface = wpa_s->global->dbus;
	if (ctrl_iface == NULL)
		return 0;

	os_snprintf(net_obj_path, WPAS_DBUS_OBJECT_PATH_MAX,
		    "%s/" WPAS_DBUS_NEW_NETWORKS_PART "/%u",
		    wpa_s->dbus_new_path, ssid->id);

	wpa_printf(MSG_DEBUG, "dbus: Register network object '%s'",
		   net_obj_path);
	obj_desc = os_zalloc(sizeof(struct wpa_dbus_object_desc));
	if (!obj_desc) {
		wpa_printf(MSG_ERROR, "Not enough memory "
			   "to create object description");
		goto err;
	}

	/* allocate memory for handlers arguments */
	arg = os_zalloc(sizeof(struct network_handler_args));
	if (!arg) {
		wpa_printf(MSG_ERROR, "Not enough memory "
			   "to create arguments for method");
		goto err;
	}

	arg->wpa_s = wpa_s;
	arg->ssid = ssid;

	wpas_dbus_register(obj_desc, arg, wpa_dbus_free, NULL,
			   wpas_dbus_network_properties,
			   wpas_dbus_network_signals);

	if (wpa_dbus_register_object_per_iface(ctrl_iface, net_obj_path,
					       wpa_s->ifname, obj_desc))
		goto err;

	wpas_dbus_signal_network_added(wpa_s, ssid->id);

	return 0;

err:
	free_dbus_object_desc(obj_desc);
	return -1;
}


/**
 * wpas_dbus_unregister_network - Unregister a configured network from dbus
 * @wpa_s: wpa_supplicant interface structure
 * @nid: network id
 * Returns: 0 on success, -1 on failure
 *
 * Unregisters network representing object from dbus
 */
int wpas_dbus_unregister_network(struct wpa_supplicant *wpa_s, int nid)
{
	struct wpas_dbus_priv *ctrl_iface;
	char net_obj_path[WPAS_DBUS_OBJECT_PATH_MAX];
	int ret;

	/* Do nothing if the control interface is not turned on */
	if (wpa_s == NULL || wpa_s->global == NULL ||
	    wpa_s->dbus_new_path == NULL)
		return 0;
	ctrl_iface = wpa_s->global->dbus;
	if (ctrl_iface == NULL)
		return 0;

	os_snprintf(net_obj_path, WPAS_DBUS_OBJECT_PATH_MAX,
		    "%s/" WPAS_DBUS_NEW_NETWORKS_PART "/%u",
		    wpa_s->dbus_new_path, nid);

	wpa_printf(MSG_DEBUG, "dbus: Unregister network object '%s'",
		   net_obj_path);
	ret = wpa_dbus_unregister_object_per_iface(ctrl_iface, net_obj_path);

	if (!ret)
		wpas_dbus_signal_network_removed(wpa_s, nid);

	return ret;
}


static const struct wpa_dbus_property_desc wpas_dbus_bss_properties[] = {
	{ "SSID", WPAS_DBUS_NEW_IFACE_BSS, "ay",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_bss_ssid,
	  NULL,
	  R
	},
	{ "BSSID", WPAS_DBUS_NEW_IFACE_BSS, "ay",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_bss_bssid,
	  NULL,
	  R
	},
	{ "Privacy", WPAS_DBUS_NEW_IFACE_BSS, "b",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_bss_privacy,
	  NULL,
	  R
	},
	{ "Mode", WPAS_DBUS_NEW_IFACE_BSS, "s",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_bss_mode,
	  NULL,
	  R
	},
	{ "Signal", WPAS_DBUS_NEW_IFACE_BSS, "n",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_bss_signal,
	  NULL,
	  R
	},
	{ "Frequency", WPAS_DBUS_NEW_IFACE_BSS, "q",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_bss_frequency,
	  NULL,
	  R
	},
	{ "Rates", WPAS_DBUS_NEW_IFACE_BSS, "au",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_bss_rates,
	  NULL,
	  R
	},
	{ "WPA", WPAS_DBUS_NEW_IFACE_BSS, "a{sv}",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_bss_wpa,
	  NULL,
	  R
	},
	{ "RSN", WPAS_DBUS_NEW_IFACE_BSS, "a{sv}",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_bss_rsn,
	  NULL,
	  R
	},
	{ "IEs", WPAS_DBUS_NEW_IFACE_BSS, "ay",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_bss_ies,
	  NULL,
	  R
	},
	{ NULL, NULL, NULL, NULL, NULL, 0 }
};


static const struct wpa_dbus_signal_desc wpas_dbus_bss_signals[] = {
	{ "PropertiesChanged", WPAS_DBUS_NEW_IFACE_BSS,
	  {
		  { "properties", "a{sv}", ARG_OUT },
		  END_ARGS
	  }
	},
	{ NULL, NULL, { END_ARGS } }
};


/**
 * wpas_dbus_unregister_bss - Unregister a scanned BSS from dbus
 * @wpa_s: wpa_supplicant interface structure
 * @bssid: scanned network bssid
 * @id: unique BSS identifier
 * Returns: 0 on success, -1 on failure
 *
 * Unregisters BSS representing object from dbus
 */
int wpas_dbus_unregister_bss(struct wpa_supplicant *wpa_s,
			     u8 bssid[ETH_ALEN], unsigned int id)
{
	struct wpas_dbus_priv *ctrl_iface;
	char bss_obj_path[WPAS_DBUS_OBJECT_PATH_MAX];

	/* Do nothing if the control interface is not turned on */
	if (wpa_s == NULL || wpa_s->global == NULL)
		return 0;
	ctrl_iface = wpa_s->global->dbus;
	if (ctrl_iface == NULL)
		return 0;

	os_snprintf(bss_obj_path, WPAS_DBUS_OBJECT_PATH_MAX,
		    "%s/" WPAS_DBUS_NEW_BSSIDS_PART "/%u",
		    wpa_s->dbus_new_path, id);

	wpa_printf(MSG_DEBUG, "dbus: Unregister BSS object '%s'",
		   bss_obj_path);
	if (wpa_dbus_unregister_object_per_iface(ctrl_iface, bss_obj_path)) {
		wpa_printf(MSG_ERROR, "dbus: Cannot unregister BSS object %s",
			   bss_obj_path);
		return -1;
	}

	wpas_dbus_signal_bss_removed(wpa_s, bss_obj_path);

	return 0;
}


/**
 * wpas_dbus_register_bss - Register a scanned BSS with dbus
 * @wpa_s: wpa_supplicant interface structure
 * @bssid: scanned network bssid
 * @id: unique BSS identifier
 * Returns: 0 on success, -1 on failure
 *
 * Registers BSS representing object with dbus
 */
int wpas_dbus_register_bss(struct wpa_supplicant *wpa_s,
			   u8 bssid[ETH_ALEN], unsigned int id)
{
	struct wpas_dbus_priv *ctrl_iface;
	struct wpa_dbus_object_desc *obj_desc;
	char bss_obj_path[WPAS_DBUS_OBJECT_PATH_MAX];
	struct bss_handler_args *arg;

	/* Do nothing if the control interface is not turned on */
	if (wpa_s == NULL || wpa_s->global == NULL)
		return 0;
	ctrl_iface = wpa_s->global->dbus;
	if (ctrl_iface == NULL)
		return 0;

	os_snprintf(bss_obj_path, WPAS_DBUS_OBJECT_PATH_MAX,
		    "%s/" WPAS_DBUS_NEW_BSSIDS_PART "/%u",
		    wpa_s->dbus_new_path, id);

	obj_desc = os_zalloc(sizeof(struct wpa_dbus_object_desc));
	if (!obj_desc) {
		wpa_printf(MSG_ERROR, "Not enough memory "
			   "to create object description");
		goto err;
	}

	arg = os_zalloc(sizeof(struct bss_handler_args));
	if (!arg) {
		wpa_printf(MSG_ERROR, "Not enough memory "
			   "to create arguments for handler");
		goto err;
	}
	arg->wpa_s = wpa_s;
	arg->id = id;

	wpas_dbus_register(obj_desc, arg, wpa_dbus_free, NULL,
			   wpas_dbus_bss_properties,
			   wpas_dbus_bss_signals);

	wpa_printf(MSG_DEBUG, "dbus: Register BSS object '%s'",
		   bss_obj_path);
	if (wpa_dbus_register_object_per_iface(ctrl_iface, bss_obj_path,
					       wpa_s->ifname, obj_desc)) {
		wpa_printf(MSG_ERROR,
			   "Cannot register BSSID dbus object %s.",
			   bss_obj_path);
		goto err;
	}

	wpas_dbus_signal_bss_added(wpa_s, bss_obj_path);

	return 0;

err:
	free_dbus_object_desc(obj_desc);
	return -1;
}


static const struct wpa_dbus_method_desc wpas_dbus_interface_methods[] = {
	{ "Scan", WPAS_DBUS_NEW_IFACE_INTERFACE,
	  (WPADBusMethodHandler) &wpas_dbus_handler_scan,
	  {
		  { "args", "a{sv}", ARG_IN },
		  END_ARGS
	  }
	},
	{ "Disconnect", WPAS_DBUS_NEW_IFACE_INTERFACE,
	  (WPADBusMethodHandler) &wpas_dbus_handler_disconnect,
	  {
		  END_ARGS
	  }
	},
	{ "AddNetwork", WPAS_DBUS_NEW_IFACE_INTERFACE,
	  (WPADBusMethodHandler) &wpas_dbus_handler_add_network,
	  {
		  { "args", "a{sv}", ARG_IN },
		  { "path", "o", ARG_OUT },
		  END_ARGS
	  }
	},
	{ "RemoveNetwork", WPAS_DBUS_NEW_IFACE_INTERFACE,
	  (WPADBusMethodHandler) &wpas_dbus_handler_remove_network,
	  {
		  { "path", "o", ARG_IN },
		  END_ARGS
	  }
	},
	{ "SelectNetwork", WPAS_DBUS_NEW_IFACE_INTERFACE,
	  (WPADBusMethodHandler) &wpas_dbus_handler_select_network,
	  {
		  { "path", "o", ARG_IN },
		  END_ARGS
	  }
	},
	{ "AddBlob", WPAS_DBUS_NEW_IFACE_INTERFACE,
	  (WPADBusMethodHandler) &wpas_dbus_handler_add_blob,
	  {
		  { "name", "s", ARG_IN },
		  { "data", "ay", ARG_IN },
		  END_ARGS
	  }
	},
	{ "GetBlob", WPAS_DBUS_NEW_IFACE_INTERFACE,
	  (WPADBusMethodHandler) &wpas_dbus_handler_get_blob,
	  {
		  { "name", "s", ARG_IN },
		  { "data", "ay", ARG_OUT },
		  END_ARGS
	  }
	},
	{ "RemoveBlob", WPAS_DBUS_NEW_IFACE_INTERFACE,
	  (WPADBusMethodHandler) &wpas_dbus_handler_remove_blob,
	  {
		  { "name", "s", ARG_IN },
		  END_ARGS
	  }
	},
#ifdef CONFIG_WPS
	{ "Start", WPAS_DBUS_NEW_IFACE_WPS,
	  (WPADBusMethodHandler) &wpas_dbus_handler_wps_start,
	  {
		  { "args", "a{sv}", ARG_IN },
		  { "output", "a{sv}", ARG_OUT },
		  END_ARGS
	  }
	},
#endif /* CONFIG_WPS */
	{ NULL, NULL, NULL, { END_ARGS } }
};

static const struct wpa_dbus_property_desc wpas_dbus_interface_properties[] = {
	{ "Capabilities", WPAS_DBUS_NEW_IFACE_INTERFACE, "a{sv}",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_capabilities,
	  NULL, R
	},
	{ "State", WPAS_DBUS_NEW_IFACE_INTERFACE, "s",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_state,
	  NULL, R
	},
	{ "Scanning", WPAS_DBUS_NEW_IFACE_INTERFACE, "b",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_scanning,
	  NULL, R
	},
	{ "ApScan", WPAS_DBUS_NEW_IFACE_INTERFACE, "u",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_ap_scan,
	  (WPADBusPropertyAccessor) wpas_dbus_setter_ap_scan,
	  RW
	},
	{ "Ifname", WPAS_DBUS_NEW_IFACE_INTERFACE, "s",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_ifname,
	  NULL, R
	},
	{ "Driver", WPAS_DBUS_NEW_IFACE_INTERFACE, "s",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_driver,
	  NULL, R
	},
	{ "BridgeIfname", WPAS_DBUS_NEW_IFACE_INTERFACE, "s",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_bridge_ifname,
	  NULL, R
	},
	{ "CurrentBSS", WPAS_DBUS_NEW_IFACE_INTERFACE, "o",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_current_bss,
	  NULL, R
	},
	{ "CurrentNetwork", WPAS_DBUS_NEW_IFACE_INTERFACE, "o",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_current_network,
	  NULL, R
	},
	{ "Blobs", WPAS_DBUS_NEW_IFACE_INTERFACE, "a{say}",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_blobs,
	  NULL, R
	},
	{ "BSSs", WPAS_DBUS_NEW_IFACE_INTERFACE, "ao",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_bsss,
	  NULL, R
	},
	{ "Networks", WPAS_DBUS_NEW_IFACE_INTERFACE, "ao",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_networks,
	  NULL, R
	},
#ifdef CONFIG_WPS
	{ "ProcessCredentials", WPAS_DBUS_NEW_IFACE_WPS, "b",
	  (WPADBusPropertyAccessor) wpas_dbus_getter_process_credentials,
	  (WPADBusPropertyAccessor) wpas_dbus_setter_process_credentials,
	  RW
	},
#endif /* CONFIG_WPS */
	{ NULL, NULL, NULL, NULL, NULL, 0 }
};

static const struct wpa_dbus_signal_desc wpas_dbus_interface_signals[] = {
	{ "ScanDone", WPAS_DBUS_NEW_IFACE_INTERFACE,
	  {
		  { "success", "b", ARG_OUT },
		  END_ARGS
	  }
	},
	{ "BSSAdded", WPAS_DBUS_NEW_IFACE_INTERFACE,
	  {
		  { "path", "o", ARG_OUT },
		  { "properties", "a{sv}", ARG_OUT },
		  END_ARGS
	  }
	},
	{ "BSSRemoved", WPAS_DBUS_NEW_IFACE_INTERFACE,
	  {
		  { "path", "o", ARG_OUT },
		  END_ARGS
	  }
	},
	{ "BlobAdded", WPAS_DBUS_NEW_IFACE_INTERFACE,
	  {
		  { "name", "s", ARG_OUT },
		  END_ARGS
	  }
	},
	{ "BlobRemoved", WPAS_DBUS_NEW_IFACE_INTERFACE,
	  {
		  { "name", "s", ARG_OUT },
		  END_ARGS
	  }
	},
	{ "NetworkAdded", WPAS_DBUS_NEW_IFACE_INTERFACE,
	  {
		  { "path", "o", ARG_OUT },
		  { "properties", "a{sv}", ARG_OUT },
		  END_ARGS
	  }
	},
	{ "NetworkRemoved", WPAS_DBUS_NEW_IFACE_INTERFACE,
	  {
		  { "path", "o", ARG_OUT },
		  END_ARGS
	  }
	},
	{ "NetworkSelected", WPAS_DBUS_NEW_IFACE_INTERFACE,
	  {
		  { "path", "o", ARG_OUT },
		  END_ARGS
	  }
	},
	{ "PropertiesChanged", WPAS_DBUS_NEW_IFACE_INTERFACE,
	  {
		  { "properties", "a{sv}", ARG_OUT },
		  END_ARGS
	  }
	},
#ifdef CONFIG_WPS
	{ "Event", WPAS_DBUS_NEW_IFACE_WPS,
	  {
		  { "name", "s", ARG_OUT },
		  { "args", "a{sv}", ARG_OUT },
		  END_ARGS
	  }
	},
	{ "Credentials", WPAS_DBUS_NEW_IFACE_WPS,
	  {
		  { "credentials", "a{sv}", ARG_OUT },
		  END_ARGS
	  }
	},
	{ "PropertiesChanged", WPAS_DBUS_NEW_IFACE_WPS,
	  {
		  { "properties", "a{sv}", ARG_OUT },
		  END_ARGS
	  }
	},
#endif /* CONFIG_WPS */
	{ NULL, NULL, { END_ARGS } }
};


int wpas_dbus_register_interface(struct wpa_supplicant *wpa_s)
{

	struct wpa_dbus_object_desc *obj_desc = NULL;
	struct wpas_dbus_priv *ctrl_iface = wpa_s->global->dbus;
	int next;

	/* Do nothing if the control interface is not turned on */
	if (ctrl_iface == NULL)
		return 0;

	/* Create and set the interface's object path */
	wpa_s->dbus_new_path = os_zalloc(WPAS_DBUS_OBJECT_PATH_MAX);
	if (wpa_s->dbus_new_path == NULL)
		return -1;
	next = ctrl_iface->next_objid++;
	os_snprintf(wpa_s->dbus_new_path, WPAS_DBUS_OBJECT_PATH_MAX,
		    WPAS_DBUS_NEW_PATH_INTERFACES "/%u",
		    next);

	obj_desc = os_zalloc(sizeof(struct wpa_dbus_object_desc));
	if (!obj_desc) {
		wpa_printf(MSG_ERROR, "Not enough memory "
			   "to create object description");
		goto err;
	}

	wpas_dbus_register(obj_desc, wpa_s, NULL, wpas_dbus_interface_methods,
			   wpas_dbus_interface_properties,
			   wpas_dbus_interface_signals);

	wpa_printf(MSG_DEBUG, "dbus: Register interface object '%s'",
		   wpa_s->dbus_new_path);
	if (wpa_dbus_register_object_per_iface(ctrl_iface,
					       wpa_s->dbus_new_path,
					       wpa_s->ifname, obj_desc))
		goto err;

	wpas_dbus_signal_interface_added(wpa_s);

	return 0;

err:
	os_free(wpa_s->dbus_new_path);
	wpa_s->dbus_new_path = NULL;
	free_dbus_object_desc(obj_desc);
	return -1;
}


int wpas_dbus_unregister_interface(struct wpa_supplicant *wpa_s)
{
	struct wpas_dbus_priv *ctrl_iface;

	/* Do nothing if the control interface is not turned on */
	if (wpa_s == NULL || wpa_s->global == NULL)
		return 0;
	ctrl_iface = wpa_s->global->dbus;
	if (ctrl_iface == NULL)
		return 0;

	wpa_printf(MSG_DEBUG, "dbus: Unregister interface object '%s'",
		   wpa_s->dbus_new_path);
	if (wpa_dbus_unregister_object_per_iface(ctrl_iface,
						 wpa_s->dbus_new_path))
		return -1;

	wpas_dbus_signal_interface_removed(wpa_s);

	os_free(wpa_s->dbus_new_path);
	wpa_s->dbus_new_path = NULL;

	return 0;
}
