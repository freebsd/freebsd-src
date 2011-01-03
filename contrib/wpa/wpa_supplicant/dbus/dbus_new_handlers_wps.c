/*
 * WPA Supplicant / dbus-based control interface (WPS)
 * Copyright (c) 2006, Dan Williams <dcbw@redhat.com> and Red Hat, Inc.
 * Copyright (c) 2009, Witold Sowa <witold.sowa@gmail.com>
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
#include "../config.h"
#include "../wpa_supplicant_i.h"
#include "../wps_supplicant.h"
#include "dbus_new_helpers.h"
#include "dbus_new.h"
#include "dbus_new_handlers.h"
#include "dbus_dict_helpers.h"


struct wps_start_params {
	int role; /* 0 - not set, 1 - enrollee, 2 - registrar */
	int type; /* 0 - not set, 1 - pin,      2 - pbc       */
	u8 *bssid;
	char *pin;
};


static int wpas_dbus_handler_wps_role(DBusMessage *message,
				      DBusMessageIter *entry_iter,
				      struct wps_start_params *params,
				      DBusMessage **reply)
{
	DBusMessageIter variant_iter;
	char *val;

	dbus_message_iter_recurse(entry_iter, &variant_iter);
	if (dbus_message_iter_get_arg_type(&variant_iter) !=
	    DBUS_TYPE_STRING) {
		wpa_printf(MSG_DEBUG, "dbus: WPS.Start - Wrong Role type, "
			   "string required");
		*reply = wpas_dbus_error_invalid_args(message,
						      "Role must be a string");
		return -1;
	}
	dbus_message_iter_get_basic(&variant_iter, &val);
	if (os_strcmp(val, "enrollee") == 0)
		params->role = 1;
	else if (os_strcmp(val, "registrar") == 0)
		params->role = 2;
	else {
		wpa_printf(MSG_DEBUG, "dbus: WPS.Start - Uknown role %s", val);
		*reply = wpas_dbus_error_invalid_args(message, val);
		return -1;
	}
	return 0;
}


static int wpas_dbus_handler_wps_type(DBusMessage *message,
				      DBusMessageIter *entry_iter,
				      struct wps_start_params *params,
				      DBusMessage **reply)
{
	DBusMessageIter variant_iter;
	char *val;

	dbus_message_iter_recurse(entry_iter, &variant_iter);
	if (dbus_message_iter_get_arg_type(&variant_iter) !=
	    DBUS_TYPE_STRING) {
		wpa_printf(MSG_DEBUG, "dbus: WPS.Start - Wrong Type type, "
			   "string required");
		*reply = wpas_dbus_error_invalid_args(message,
						      "Type must be a string");
		return -1;
	}
	dbus_message_iter_get_basic(&variant_iter, &val);
	if (os_strcmp(val, "pin") == 0)
		params->type = 1;
	else if (os_strcmp(val, "pbc") == 0)
		params->type = 2;
	else {
		wpa_printf(MSG_DEBUG, "dbus: WPS.Start - Unknown type %s",
			   val);
		*reply = wpas_dbus_error_invalid_args(message, val);
		return -1;
	}
	return 0;
}


static int wpas_dbus_handler_wps_bssid(DBusMessage *message,
				       DBusMessageIter *entry_iter,
				       struct wps_start_params *params,
				       DBusMessage **reply)
{
	DBusMessageIter variant_iter, array_iter;
	int len;

	dbus_message_iter_recurse(entry_iter, &variant_iter);
	if (dbus_message_iter_get_arg_type(&variant_iter) != DBUS_TYPE_ARRAY ||
	    dbus_message_iter_get_element_type(&variant_iter) !=
	    DBUS_TYPE_ARRAY) {
		wpa_printf(MSG_DEBUG, "dbus: WPS.Start - Wrong Bssid type, "
			   "byte array required");
		*reply = wpas_dbus_error_invalid_args(
			message, "Bssid must be a byte array");
		return -1;
	}
	dbus_message_iter_recurse(&variant_iter, &array_iter);
	dbus_message_iter_get_fixed_array(&array_iter, &params->bssid, &len);
	if (len != ETH_ALEN) {
		wpa_printf(MSG_DEBUG, "dbus: WPS.Stsrt - Wrong Bssid length "
			   "%d", len);
		*reply = wpas_dbus_error_invalid_args(message,
						      "Bssid is wrong length");
		return -1;
	}
	return 0;
}


static int wpas_dbus_handler_wps_pin(DBusMessage *message,
				     DBusMessageIter *entry_iter,
				     struct wps_start_params *params,
				     DBusMessage **reply)
{
	DBusMessageIter variant_iter;

	dbus_message_iter_recurse(entry_iter, &variant_iter);
	if (dbus_message_iter_get_arg_type(&variant_iter) !=
	    DBUS_TYPE_STRING) {
		wpa_printf(MSG_DEBUG, "dbus: WPS.Start - Wrong Pin type, "
			   "string required");
		*reply = wpas_dbus_error_invalid_args(message,
						      "Pin must be a string");
		return -1;
	}
	dbus_message_iter_get_basic(&variant_iter, &params->pin);
	return 0;
}


static int wpas_dbus_handler_wps_start_entry(DBusMessage *message, char *key,
					     DBusMessageIter *entry_iter,
					     struct wps_start_params *params,
					     DBusMessage **reply)
{
	if (os_strcmp(key, "Role") == 0)
		return wpas_dbus_handler_wps_role(message, entry_iter,
						  params, reply);
	else if (os_strcmp(key, "Type") == 0)
		return wpas_dbus_handler_wps_type(message, entry_iter,
						  params, reply);
	else if (os_strcmp(key, "Bssid") == 0)
		return wpas_dbus_handler_wps_bssid(message, entry_iter,
						   params, reply);
	else if (os_strcmp(key, "Pin") == 0)
		return wpas_dbus_handler_wps_pin(message, entry_iter,
						 params, reply);

	wpa_printf(MSG_DEBUG, "dbus: WPS.Start - unknown key %s", key);
	*reply = wpas_dbus_error_invalid_args(message, key);
	return -1;
}


/**
 * wpas_dbus_handler_wps_start - Start WPS configuration
 * @message: Pointer to incoming dbus message
 * @wpa_s: %wpa_supplicant data structure
 * Returns: DBus message dictionary on success or DBus error on failure
 *
 * Handler for "Start" method call. DBus dictionary argument contains
 * information about role (enrollee or registrar), authorization method
 * (pin or push button) and optionally pin and bssid. Returned message
 * has a dictionary argument which may contain newly generated pin (optional).
 */
DBusMessage * wpas_dbus_handler_wps_start(DBusMessage *message,
					  struct wpa_supplicant *wpa_s)
{
	DBusMessage *reply = NULL;
	DBusMessageIter iter, dict_iter, entry_iter;
	struct wps_start_params params;
	char *key;
	char npin[9] = { '\0' };
	int ret;

	os_memset(&params, 0, sizeof(params));
	dbus_message_iter_init(message, &iter);

	dbus_message_iter_recurse(&iter, &dict_iter);
	while (dbus_message_iter_get_arg_type(&dict_iter) ==
	       DBUS_TYPE_DICT_ENTRY) {
		dbus_message_iter_recurse(&dict_iter, &entry_iter);

		dbus_message_iter_get_basic(&entry_iter, &key);
		dbus_message_iter_next(&entry_iter);

		if (wpas_dbus_handler_wps_start_entry(message, key,
						      &entry_iter,
						      &params, &reply))
			return reply;

		dbus_message_iter_next(&dict_iter);
	}

	if (params.role == 0) {
		wpa_printf(MSG_DEBUG, "dbus: WPS.Start - Role not specified");
		return wpas_dbus_error_invalid_args(message,
						    "Role not specified");
	} else if (params.role == 1 && params.type == 0) {
		wpa_printf(MSG_DEBUG, "dbus: WPS.Start - Type not specified");
		return wpas_dbus_error_invalid_args(message,
						    "Type not specified");
	} else if (params.role == 2 && params.pin == NULL) {
		wpa_printf(MSG_DEBUG, "dbus: WPS.Start - Pin required for "
			   "registrar role");
		return wpas_dbus_error_invalid_args(
			message, "Pin required for registrar role.");
	}

	if (params.role == 2)
		ret = wpas_wps_start_reg(wpa_s, params.bssid, params.pin,
					 NULL);
	else if (params.type == 1) {
		ret = wpas_wps_start_pin(wpa_s, params.bssid, params.pin);
		if (ret > 0)
			os_snprintf(npin, sizeof(npin), "%08d", ret);
	} else
		ret = wpas_wps_start_pbc(wpa_s, params.bssid);

	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "dbus: WPS.Start wpas_wps_failed in "
			   "role %s and key %s",
			   (params.role == 1 ? "enrollee" : "registrar"),
			   (params.type == 0 ? "" :
			    (params.type == 1 ? "pin" : "pbc")));
		return wpas_dbus_error_unknown_error(message,
						     "WPS start failed");
	}

	reply = dbus_message_new_method_return(message);
	if (!reply) {
		return dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					      NULL);
	}

	dbus_message_iter_init_append(reply, &iter);
	if (!wpa_dbus_dict_open_write(&iter, &dict_iter)) {
		dbus_message_unref(reply);
		return dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					      NULL);
	}

	if (os_strlen(npin) > 0) {
		if (!wpa_dbus_dict_append_string(&dict_iter, "Pin", npin)) {
			dbus_message_unref(reply);
			return dbus_message_new_error(message,
						      DBUS_ERROR_NO_MEMORY,
						      NULL);
		}
	}

	if (!wpa_dbus_dict_close_write(&iter, &dict_iter)) {
		dbus_message_unref(reply);
		return dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					      NULL);
	}

	return reply;
}


/**
 * wpas_dbus_getter_process_credentials - Check if credentials are processed
 * @message: Pointer to incoming dbus message
 * @wpa_s: %wpa_supplicant data structure
 * Returns: DBus message with a boolean on success or DBus error on failure
 *
 * Getter for "ProcessCredentials" property. Returns returned boolean will be
 * true if wps_cred_processing configuration field is not equal to 1 or false
 * if otherwise.
 */
DBusMessage * wpas_dbus_getter_process_credentials(
	DBusMessage *message, struct wpa_supplicant *wpa_s)
{
	dbus_bool_t process = (wpa_s->conf->wps_cred_processing != 1);
	return wpas_dbus_simple_property_getter(message, DBUS_TYPE_BOOLEAN,
						&process);
}


/**
 * wpas_dbus_setter_process_credentials - Set credentials_processed conf param
 * @message: Pointer to incoming dbus message
 * @wpa_s: %wpa_supplicant data structure
 * Returns: NULL on success or DBus error on failure
 *
 * Setter for "ProcessCredentials" property. Sets credentials_processed on 2
 * if boolean argument is true or on 1 if otherwise.
 */
DBusMessage * wpas_dbus_setter_process_credentials(
	DBusMessage *message, struct wpa_supplicant *wpa_s)
{
	DBusMessage *reply = NULL;
	dbus_bool_t process_credentials, old_pc;

	reply = wpas_dbus_simple_property_setter(message, DBUS_TYPE_BOOLEAN,
						 &process_credentials);
	if (reply)
		return reply;

	old_pc = (wpa_s->conf->wps_cred_processing != 1);
	wpa_s->conf->wps_cred_processing = (process_credentials ? 2 : 1);

	if ((wpa_s->conf->wps_cred_processing != 1) != old_pc)
		wpa_dbus_mark_property_changed(wpa_s->global->dbus,
					       wpa_s->dbus_new_path,
					       WPAS_DBUS_NEW_IFACE_WPS,
					       "ProcessCredentials");

	return NULL;
}
