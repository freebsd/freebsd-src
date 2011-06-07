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
#include "common/ieee802_11_defs.h"
#include "eap_peer/eap_methods.h"
#include "eapol_supp/eapol_supp_sm.h"
#include "rsn_supp/wpa.h"
#include "../config.h"
#include "../wpa_supplicant_i.h"
#include "../driver_i.h"
#include "../notify.h"
#include "../wpas_glue.h"
#include "../bss.h"
#include "../scan.h"
#include "dbus_new_helpers.h"
#include "dbus_new.h"
#include "dbus_new_handlers.h"
#include "dbus_dict_helpers.h"

extern int wpa_debug_level;
extern int wpa_debug_show_keys;
extern int wpa_debug_timestamp;

static const char *debug_strings[] = {
	"msgdump", "debug", "info", "warning", "error", NULL
};


/**
 * wpas_dbus_new_decompose_object_path - Decompose an interface object path into parts
 * @path: The dbus object path
 * @network: (out) the configured network this object path refers to, if any
 * @bssid: (out) the scanned bssid this object path refers to, if any
 * Returns: The object path of the network interface this path refers to
 *
 * For a given object path, decomposes the object path into object id, network,
 * and BSSID parts, if those parts exist.
 */
static char * wpas_dbus_new_decompose_object_path(const char *path,
						  char **network,
						  char **bssid)
{
	const unsigned int dev_path_prefix_len =
		strlen(WPAS_DBUS_NEW_PATH_INTERFACES "/");
	char *obj_path_only;
	char *next_sep;

	/* Be a bit paranoid about path */
	if (!path || os_strncmp(path, WPAS_DBUS_NEW_PATH_INTERFACES "/",
				dev_path_prefix_len))
		return NULL;

	/* Ensure there's something at the end of the path */
	if ((path + dev_path_prefix_len)[0] == '\0')
		return NULL;

	obj_path_only = os_strdup(path);
	if (obj_path_only == NULL)
		return NULL;

	next_sep = os_strchr(obj_path_only + dev_path_prefix_len, '/');
	if (next_sep != NULL) {
		const char *net_part = os_strstr(
			next_sep, WPAS_DBUS_NEW_NETWORKS_PART "/");
		const char *bssid_part = os_strstr(
			next_sep, WPAS_DBUS_NEW_BSSIDS_PART "/");

		if (network && net_part) {
			/* Deal with a request for a configured network */
			const char *net_name = net_part +
				os_strlen(WPAS_DBUS_NEW_NETWORKS_PART "/");
			*network = NULL;
			if (os_strlen(net_name))
				*network = os_strdup(net_name);
		} else if (bssid && bssid_part) {
			/* Deal with a request for a scanned BSSID */
			const char *bssid_name = bssid_part +
				os_strlen(WPAS_DBUS_NEW_BSSIDS_PART "/");
			if (strlen(bssid_name))
				*bssid = os_strdup(bssid_name);
			else
				*bssid = NULL;
		}

		/* Cut off interface object path before "/" */
		*next_sep = '\0';
	}

	return obj_path_only;
}


/**
 * wpas_dbus_error_unknown_error - Return a new InvalidArgs error message
 * @message: Pointer to incoming dbus message this error refers to
 * @arg: Optional string appended to error message
 * Returns: a dbus error message
 *
 * Convenience function to create and return an UnknownError
 */
DBusMessage * wpas_dbus_error_unknown_error(DBusMessage *message,
					    const char *arg)
{
	return dbus_message_new_error(message, WPAS_DBUS_ERROR_UNKNOWN_ERROR,
				      arg);
}


/**
 * wpas_dbus_error_iface_unknown - Return a new invalid interface error message
 * @message: Pointer to incoming dbus message this error refers to
 * Returns: A dbus error message
 *
 * Convenience function to create and return an invalid interface error
 */
static DBusMessage * wpas_dbus_error_iface_unknown(DBusMessage *message)
{
	return dbus_message_new_error(message, WPAS_DBUS_ERROR_IFACE_UNKNOWN,
				      "wpa_supplicant knows nothing about "
				      "this interface.");
}


/**
 * wpas_dbus_error_network_unknown - Return a new NetworkUnknown error message
 * @message: Pointer to incoming dbus message this error refers to
 * Returns: a dbus error message
 *
 * Convenience function to create and return an invalid network error
 */
static DBusMessage * wpas_dbus_error_network_unknown(DBusMessage *message)
{
	return dbus_message_new_error(message, WPAS_DBUS_ERROR_NETWORK_UNKNOWN,
				      "There is no such a network in this "
				      "interface.");
}


/**
 * wpas_dbus_error_invalid_args - Return a new InvalidArgs error message
 * @message: Pointer to incoming dbus message this error refers to
 * Returns: a dbus error message
 *
 * Convenience function to create and return an invalid options error
 */
DBusMessage * wpas_dbus_error_invalid_args(DBusMessage *message,
					  const char *arg)
{
	DBusMessage *reply;

	reply = dbus_message_new_error(message, WPAS_DBUS_ERROR_INVALID_ARGS,
				       "Did not receive correct message "
				       "arguments.");
	if (arg != NULL)
		dbus_message_append_args(reply, DBUS_TYPE_STRING, &arg,
					 DBUS_TYPE_INVALID);

	return reply;
}


static const char *dont_quote[] = {
	"key_mgmt", "proto", "pairwise", "auth_alg", "group", "eap",
	"opensc_engine_path", "pkcs11_engine_path", "pkcs11_module_path",
	"bssid", NULL
};

static dbus_bool_t should_quote_opt(const char *key)
{
	int i = 0;
	while (dont_quote[i] != NULL) {
		if (os_strcmp(key, dont_quote[i]) == 0)
			return FALSE;
		i++;
	}
	return TRUE;
}

/**
 * get_iface_by_dbus_path - Get a new network interface
 * @global: Pointer to global data from wpa_supplicant_init()
 * @path: Pointer to a dbus object path representing an interface
 * Returns: Pointer to the interface or %NULL if not found
 */
static struct wpa_supplicant * get_iface_by_dbus_path(
	struct wpa_global *global, const char *path)
{
	struct wpa_supplicant *wpa_s;

	for (wpa_s = global->ifaces; wpa_s; wpa_s = wpa_s->next) {
		if (os_strcmp(wpa_s->dbus_new_path, path) == 0)
			return wpa_s;
	}
	return NULL;
}


/**
 * set_network_properties - Set properties of a configured network
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * @ssid: wpa_ssid structure for a configured network
 * @iter: DBus message iterator containing dictionary of network
 * properties to set.
 * Returns: NULL when succeed or DBus error on failure
 *
 * Sets network configuration with parameters given id DBus dictionary
 */
static DBusMessage * set_network_properties(DBusMessage *message,
					    struct wpa_supplicant *wpa_s,
					    struct wpa_ssid *ssid,
					    DBusMessageIter *iter)
{

	struct wpa_dbus_dict_entry entry = { .type = DBUS_TYPE_STRING };
	DBusMessage *reply = NULL;
	DBusMessageIter	iter_dict;

	if (!wpa_dbus_dict_open_read(iter, &iter_dict))
		return wpas_dbus_error_invalid_args(message, NULL);

	while (wpa_dbus_dict_has_dict_entry(&iter_dict)) {
		char *value = NULL;
		size_t size = 50;
		int ret;
		if (!wpa_dbus_dict_get_entry(&iter_dict, &entry)) {
			reply = wpas_dbus_error_invalid_args(message, NULL);
			break;
		}
		if (entry.type == DBUS_TYPE_ARRAY &&
		    entry.array_type == DBUS_TYPE_BYTE) {
			if (entry.array_len <= 0)
				goto error;

			size = entry.array_len * 2 + 1;
			value = os_zalloc(size);
			if (value == NULL)
				goto error;

			ret = wpa_snprintf_hex(value, size,
					       (u8 *) entry.bytearray_value,
					       entry.array_len);
			if (ret <= 0)
				goto error;
		} else if (entry.type == DBUS_TYPE_STRING) {
			if (should_quote_opt(entry.key)) {
				size = os_strlen(entry.str_value);
				if (size <= 0)
					goto error;

				size += 3;
				value = os_zalloc(size);
				if (value == NULL)
					goto error;

				ret = os_snprintf(value, size, "\"%s\"",
						  entry.str_value);
				if (ret < 0 || (size_t) ret != (size - 1))
					goto error;
			} else {
				value = os_strdup(entry.str_value);
				if (value == NULL)
					goto error;
			}
		} else if (entry.type == DBUS_TYPE_UINT32) {
			value = os_zalloc(size);
			if (value == NULL)
				goto error;

			ret = os_snprintf(value, size, "%u",
					  entry.uint32_value);
			if (ret <= 0)
				goto error;
		} else if (entry.type == DBUS_TYPE_INT32) {
			value = os_zalloc(size);
			if (value == NULL)
				goto error;

			ret = os_snprintf(value, size, "%d",
					  entry.int32_value);
			if (ret <= 0)
				goto error;
		} else
			goto error;

		if (wpa_config_set(ssid, entry.key, value, 0) < 0)
			goto error;

		if ((os_strcmp(entry.key, "psk") == 0 &&
		     value[0] == '"' && ssid->ssid_len) ||
		    (strcmp(entry.key, "ssid") == 0 && ssid->passphrase))
			wpa_config_update_psk(ssid);
		else if (os_strcmp(entry.key, "priority") == 0)
			wpa_config_update_prio_list(wpa_s->conf);

		os_free(value);
		wpa_dbus_dict_entry_clear(&entry);
		continue;

	error:
		os_free(value);
		reply = wpas_dbus_error_invalid_args(message, entry.key);
		wpa_dbus_dict_entry_clear(&entry);
		break;
	}

	return reply;
}


/**
 * wpas_dbus_simple_property_getter - Get basic type property
 * @message: Pointer to incoming dbus message
 * @type: DBus type of property (must be basic type)
 * @val: pointer to place holding property value
 * Returns: The DBus message containing response for Properties.Get call
 * or DBus error message if error occurred.
 *
 * Generic getter for basic type properties. Type is required to be basic.
 */
DBusMessage * wpas_dbus_simple_property_getter(DBusMessage *message,
					       const int type, const void *val)
{
	DBusMessage *reply = NULL;
	DBusMessageIter iter, variant_iter;

	if (!dbus_type_is_basic(type)) {
		wpa_printf(MSG_ERROR, "dbus: wpas_dbus_simple_property_getter:"
			   " given type is not basic");
		return wpas_dbus_error_unknown_error(message, NULL);
	}

	if (message == NULL)
		reply = dbus_message_new(DBUS_MESSAGE_TYPE_SIGNAL);
	else
		reply = dbus_message_new_method_return(message);

	if (reply != NULL) {
		dbus_message_iter_init_append(reply, &iter);
		if (!dbus_message_iter_open_container(
			    &iter, DBUS_TYPE_VARIANT,
			    wpa_dbus_type_as_string(type), &variant_iter) ||
		    !dbus_message_iter_append_basic(&variant_iter, type,
						    val) ||
		    !dbus_message_iter_close_container(&iter, &variant_iter)) {
			wpa_printf(MSG_ERROR, "dbus: "
				   "wpas_dbus_simple_property_getter: out of "
				   "memory to put property value into "
				   "message");
			dbus_message_unref(reply);
			reply = dbus_message_new_error(message,
						       DBUS_ERROR_NO_MEMORY,
						       NULL);
		}
	} else {
		wpa_printf(MSG_ERROR, "dbus: wpas_dbus_simple_property_getter:"
			   " out of memory to return property value");
		reply = dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					       NULL);
	}

	return reply;
}


/**
 * wpas_dbus_simple_property_setter - Set basic type property
 * @message: Pointer to incoming dbus message
 * @type: DBus type of property (must be basic type)
 * @val: pointer to place where value being set will be stored
 * Returns: NULL or DBus error message if error occurred.
 *
 * Generic setter for basic type properties. Type is required to be basic.
 */
DBusMessage * wpas_dbus_simple_property_setter(DBusMessage *message,
					       const int type, void *val)
{
	DBusMessageIter iter, variant_iter;

	if (!dbus_type_is_basic(type)) {
		wpa_printf(MSG_ERROR, "dbus: wpas_dbus_simple_property_setter:"
			   " given type is not basic");
		return wpas_dbus_error_unknown_error(message, NULL);
	}

	if (!dbus_message_iter_init(message, &iter)) {
		wpa_printf(MSG_ERROR, "dbus: wpas_dbus_simple_property_setter:"
			   " out of memory to return scanning state");
		return dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					      NULL);
	}

	/* omit first and second argument and get value from third */
	dbus_message_iter_next(&iter);
	dbus_message_iter_next(&iter);
	dbus_message_iter_recurse(&iter, &variant_iter);

	if (dbus_message_iter_get_arg_type(&variant_iter) != type) {
		wpa_printf(MSG_DEBUG, "dbus: wpas_dbus_simple_property_setter:"
			   " wrong property type");
		return wpas_dbus_error_invalid_args(message,
						    "wrong property type");
	}
	dbus_message_iter_get_basic(&variant_iter, val);

	return NULL;
}


/**
 * wpas_dbus_simple_array_property_getter - Get array type property
 * @message: Pointer to incoming dbus message
 * @type: DBus type of property array elements (must be basic type)
 * @array: pointer to array of elements to put into response message
 * @array_len: length of above array
 * Returns: The DBus message containing response for Properties.Get call
 * or DBus error message if error occurred.
 *
 * Generic getter for array type properties. Array elements type is
 * required to be basic.
 */
DBusMessage * wpas_dbus_simple_array_property_getter(DBusMessage *message,
						     const int type,
						     const void *array,
						     size_t array_len)
{
	DBusMessage *reply = NULL;
	DBusMessageIter iter, variant_iter, array_iter;
	char type_str[] = "a?"; /* ? will be replaced with subtype letter; */
	const char *sub_type_str;
	size_t element_size, i;

	if (!dbus_type_is_basic(type)) {
		wpa_printf(MSG_ERROR, "dbus: "
			   "wpas_dbus_simple_array_property_getter: given "
			   "type is not basic");
		return wpas_dbus_error_unknown_error(message, NULL);
	}

	sub_type_str = wpa_dbus_type_as_string(type);
	type_str[1] = sub_type_str[0];

	if (message == NULL)
		reply = dbus_message_new(DBUS_MESSAGE_TYPE_SIGNAL);
	else
		reply = dbus_message_new_method_return(message);
	if (reply == NULL) {
		wpa_printf(MSG_ERROR, "dbus: "
			   "wpas_dbus_simple_array_property_getter: out of "
			   "memory to create return message");
		return dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					      NULL);
	}

	dbus_message_iter_init_append(reply, &iter);

	if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT,
					      type_str, &variant_iter) ||
	    !dbus_message_iter_open_container(&variant_iter, DBUS_TYPE_ARRAY,
					      sub_type_str, &array_iter)) {
		wpa_printf(MSG_ERROR, "dbus: "
			   "wpas_dbus_simple_array_property_getter: out of "
			   "memory to open container");
		dbus_message_unref(reply);
		return dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					      NULL);
	}

	switch(type) {
	case DBUS_TYPE_BYTE:
	case DBUS_TYPE_BOOLEAN:
		element_size = 1;
		break;
	case DBUS_TYPE_INT16:
	case DBUS_TYPE_UINT16:
		element_size = sizeof(uint16_t);
		break;
	case DBUS_TYPE_INT32:
	case DBUS_TYPE_UINT32:
		element_size = sizeof(uint32_t);
		break;
	case DBUS_TYPE_INT64:
	case DBUS_TYPE_UINT64:
		element_size = sizeof(uint64_t);
		break;
	case DBUS_TYPE_DOUBLE:
		element_size = sizeof(double);
		break;
	case DBUS_TYPE_STRING:
	case DBUS_TYPE_OBJECT_PATH:
		element_size = sizeof(char *);
		break;
	default:
		wpa_printf(MSG_ERROR, "dbus: "
			   "wpas_dbus_simple_array_property_getter: "
			   "fatal: unknown element type");
		element_size = 1;
		break;
	}

	for (i = 0; i < array_len; i++) {
		dbus_message_iter_append_basic(&array_iter, type,
					       array + i * element_size);
	}

	if (!dbus_message_iter_close_container(&variant_iter, &array_iter) ||
	    !dbus_message_iter_close_container(&iter, &variant_iter)) {
		wpa_printf(MSG_ERROR, "dbus: "
			   "wpas_dbus_simple_array_property_getter: out of "
			   "memory to close container");
		dbus_message_unref(reply);
		return dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					      NULL);
	}

	return reply;
}


/**
 * wpas_dbus_handler_create_interface - Request registration of a network iface
 * @message: Pointer to incoming dbus message
 * @global: %wpa_supplicant global data structure
 * Returns: The object path of the new interface object,
 *          or a dbus error message with more information
 *
 * Handler function for "CreateInterface" method call. Handles requests
 * by dbus clients to register a network interface that wpa_supplicant
 * will manage.
 */
DBusMessage * wpas_dbus_handler_create_interface(DBusMessage *message,
						 struct wpa_global *global)
{
	DBusMessageIter iter_dict;
	DBusMessage *reply = NULL;
	DBusMessageIter iter;
	struct wpa_dbus_dict_entry entry;
	char *driver = NULL;
	char *ifname = NULL;
	char *bridge_ifname = NULL;

	dbus_message_iter_init(message, &iter);

	if (!wpa_dbus_dict_open_read(&iter, &iter_dict))
		goto error;
	while (wpa_dbus_dict_has_dict_entry(&iter_dict)) {
		if (!wpa_dbus_dict_get_entry(&iter_dict, &entry))
			goto error;
		if (!strcmp(entry.key, "Driver") &&
		    (entry.type == DBUS_TYPE_STRING)) {
			driver = os_strdup(entry.str_value);
			wpa_dbus_dict_entry_clear(&entry);
			if (driver == NULL)
				goto error;
		} else if (!strcmp(entry.key, "Ifname") &&
			   (entry.type == DBUS_TYPE_STRING)) {
			ifname = os_strdup(entry.str_value);
			wpa_dbus_dict_entry_clear(&entry);
			if (ifname == NULL)
				goto error;
		} else if (!strcmp(entry.key, "BridgeIfname") &&
			   (entry.type == DBUS_TYPE_STRING)) {
			bridge_ifname = os_strdup(entry.str_value);
			wpa_dbus_dict_entry_clear(&entry);
			if (bridge_ifname == NULL)
				goto error;
		} else {
			wpa_dbus_dict_entry_clear(&entry);
			goto error;
		}
	}

	if (ifname == NULL)
		goto error; /* Required Ifname argument missing */

	/*
	 * Try to get the wpa_supplicant record for this iface, return
	 * an error if we already control it.
	 */
	if (wpa_supplicant_get_iface(global, ifname) != NULL) {
		reply = dbus_message_new_error(message,
					       WPAS_DBUS_ERROR_IFACE_EXISTS,
					       "wpa_supplicant already "
					       "controls this interface.");
	} else {
		struct wpa_supplicant *wpa_s;
		struct wpa_interface iface;
		os_memset(&iface, 0, sizeof(iface));
		iface.driver = driver;
		iface.ifname = ifname;
		iface.bridge_ifname = bridge_ifname;
		/* Otherwise, have wpa_supplicant attach to it. */
		if ((wpa_s = wpa_supplicant_add_iface(global, &iface))) {
			const char *path = wpa_s->dbus_new_path;
			reply = dbus_message_new_method_return(message);
			dbus_message_append_args(reply, DBUS_TYPE_OBJECT_PATH,
			                         &path, DBUS_TYPE_INVALID);
		} else {
			reply = wpas_dbus_error_unknown_error(
				message, "wpa_supplicant couldn't grab this "
				"interface.");
		}
	}

out:
	os_free(driver);
	os_free(ifname);
	os_free(bridge_ifname);
	return reply;

error:
	reply = wpas_dbus_error_invalid_args(message, NULL);
	goto out;
}


/**
 * wpas_dbus_handler_remove_interface - Request deregistration of an interface
 * @message: Pointer to incoming dbus message
 * @global: wpa_supplicant global data structure
 * Returns: a dbus message containing a UINT32 indicating success (1) or
 *          failure (0), or returns a dbus error message with more information
 *
 * Handler function for "removeInterface" method call.  Handles requests
 * by dbus clients to deregister a network interface that wpa_supplicant
 * currently manages.
 */
DBusMessage * wpas_dbus_handler_remove_interface(DBusMessage *message,
						 struct wpa_global *global)
{
	struct wpa_supplicant *wpa_s;
	char *path;
	DBusMessage *reply = NULL;

	dbus_message_get_args(message, NULL, DBUS_TYPE_OBJECT_PATH, &path,
			      DBUS_TYPE_INVALID);

	wpa_s = get_iface_by_dbus_path(global, path);
	if (wpa_s == NULL)
		reply = wpas_dbus_error_iface_unknown(message);
	else if (wpa_supplicant_remove_iface(global, wpa_s)) {
		reply = wpas_dbus_error_unknown_error(
			message, "wpa_supplicant couldn't remove this "
			"interface.");
	}

	return reply;
}


/**
 * wpas_dbus_handler_get_interface - Get the object path for an interface name
 * @message: Pointer to incoming dbus message
 * @global: %wpa_supplicant global data structure
 * Returns: The object path of the interface object,
 *          or a dbus error message with more information
 *
 * Handler function for "getInterface" method call.
 */
DBusMessage * wpas_dbus_handler_get_interface(DBusMessage *message,
					      struct wpa_global *global)
{
	DBusMessage *reply = NULL;
	const char *ifname;
	const char *path;
	struct wpa_supplicant *wpa_s;

	dbus_message_get_args(message, NULL, DBUS_TYPE_STRING, &ifname,
			      DBUS_TYPE_INVALID);

	wpa_s = wpa_supplicant_get_iface(global, ifname);
	if (wpa_s == NULL)
		return wpas_dbus_error_iface_unknown(message);

	path = wpa_s->dbus_new_path;
	reply = dbus_message_new_method_return(message);
	if (reply == NULL)
		return dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					      NULL);
	if (!dbus_message_append_args(reply, DBUS_TYPE_OBJECT_PATH, &path,
				      DBUS_TYPE_INVALID)) {
		dbus_message_unref(reply);
		return dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					      NULL);
	}

	return reply;
}


/**
 * wpas_dbus_getter_debug_level - Get debug level
 * @message: Pointer to incoming dbus message
 * @global: %wpa_supplicant global data structure
 * Returns: DBus message with value of debug level
 *
 * Getter for "DebugLevel" property.
 */
DBusMessage * wpas_dbus_getter_debug_level(DBusMessage *message,
					   struct wpa_global *global)
{
	const char *str;
	int idx = wpa_debug_level;
	if (idx < 0)
		idx = 0;
	if (idx > 4)
		idx = 4;
	str = debug_strings[idx];
	return wpas_dbus_simple_property_getter(message, DBUS_TYPE_STRING,
						&str);
}


/**
 * wpas_dbus_getter_debug_timestamp - Get debug timestamp
 * @message: Pointer to incoming dbus message
 * @global: %wpa_supplicant global data structure
 * Returns: DBus message with value of debug timestamp
 *
 * Getter for "DebugTimestamp" property.
 */
DBusMessage * wpas_dbus_getter_debug_timestamp(DBusMessage *message,
					       struct wpa_global *global)
{
	return wpas_dbus_simple_property_getter(message, DBUS_TYPE_BOOLEAN,
						&wpa_debug_timestamp);

}


/**
 * wpas_dbus_getter_debug_show_keys - Get debug show keys
 * @message: Pointer to incoming dbus message
 * @global: %wpa_supplicant global data structure
 * Returns: DBus message with value of debug show_keys
 *
 * Getter for "DebugShowKeys" property.
 */
DBusMessage * wpas_dbus_getter_debug_show_keys(DBusMessage *message,
					       struct wpa_global *global)
{
	return wpas_dbus_simple_property_getter(message, DBUS_TYPE_BOOLEAN,
						&wpa_debug_show_keys);

}

/**
 * wpas_dbus_setter_debug_level - Set debug level
 * @message: Pointer to incoming dbus message
 * @global: %wpa_supplicant global data structure
 * Returns: %NULL or DBus error message
 *
 * Setter for "DebugLevel" property.
 */
DBusMessage * wpas_dbus_setter_debug_level(DBusMessage *message,
					   struct wpa_global *global)
{
	DBusMessage *reply;
	const char *str = NULL;
	int i, val = -1;

	reply = wpas_dbus_simple_property_setter(message, DBUS_TYPE_STRING,
						 &str);
	if (reply)
		return reply;

	for (i = 0; debug_strings[i]; i++)
		if (os_strcmp(debug_strings[i], str) == 0) {
			val = i;
			break;
		}

	if (val < 0 ||
	    wpa_supplicant_set_debug_params(global, val, wpa_debug_timestamp,
					    wpa_debug_show_keys)) {
		dbus_message_unref(reply);
		return wpas_dbus_error_invalid_args(
			message, "Wrong debug level value");
	}

	return NULL;
}


/**
 * wpas_dbus_setter_debug_timestamp - Set debug timestamp
 * @message: Pointer to incoming dbus message
 * @global: %wpa_supplicant global data structure
 * Returns: %NULL or DBus error message
 *
 * Setter for "DebugTimestamp" property.
 */
DBusMessage * wpas_dbus_setter_debug_timestamp(DBusMessage *message,
					       struct wpa_global *global)
{
	DBusMessage *reply;
	dbus_bool_t val;

	reply = wpas_dbus_simple_property_setter(message, DBUS_TYPE_BOOLEAN,
						 &val);
	if (reply)
		return reply;

	wpa_supplicant_set_debug_params(global, wpa_debug_level, val ? 1 : 0,
					wpa_debug_show_keys);

	return NULL;
}


/**
 * wpas_dbus_setter_debug_show_keys - Set debug show keys
 * @message: Pointer to incoming dbus message
 * @global: %wpa_supplicant global data structure
 * Returns: %NULL or DBus error message
 *
 * Setter for "DebugShowKeys" property.
 */
DBusMessage * wpas_dbus_setter_debug_show_keys(DBusMessage *message,
					       struct wpa_global *global)
{
	DBusMessage *reply;
	dbus_bool_t val;

	reply = wpas_dbus_simple_property_setter(message, DBUS_TYPE_BOOLEAN,
						 &val);
	if (reply)
		return reply;

	wpa_supplicant_set_debug_params(global, wpa_debug_level,
					wpa_debug_timestamp,
					val ? 1 : 0);

	return NULL;
}


/**
 * wpas_dbus_getter_interfaces - Request registered interfaces list
 * @message: Pointer to incoming dbus message
 * @global: %wpa_supplicant global data structure
 * Returns: The object paths array containing registered interfaces
 * objects paths or DBus error on failure
 *
 * Getter for "Interfaces" property. Handles requests
 * by dbus clients to return list of registered interfaces objects
 * paths
 */
DBusMessage * wpas_dbus_getter_interfaces(DBusMessage *message,
					  struct wpa_global *global)
{
	DBusMessage *reply = NULL;
	struct wpa_supplicant *wpa_s;
	const char **paths;
	unsigned int i = 0, num = 0;

	for (wpa_s = global->ifaces; wpa_s; wpa_s = wpa_s->next)
		num++;

	paths = os_zalloc(num * sizeof(char*));
	if (!paths) {
		return dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					      NULL);
	}

	for (wpa_s = global->ifaces; wpa_s; wpa_s = wpa_s->next)
		paths[i] = wpa_s->dbus_new_path;

	reply = wpas_dbus_simple_array_property_getter(message,
						       DBUS_TYPE_OBJECT_PATH,
						       paths, num);

	os_free(paths);
	return reply;
}


/**
 * wpas_dbus_getter_eap_methods - Request supported EAP methods list
 * @message: Pointer to incoming dbus message
 * @nothing: not used argument. may be NULL or anything else
 * Returns: The object paths array containing supported EAP methods
 * represented by strings or DBus error on failure
 *
 * Getter for "EapMethods" property. Handles requests
 * by dbus clients to return list of strings with supported EAP methods
 */
DBusMessage * wpas_dbus_getter_eap_methods(DBusMessage *message, void *nothing)
{
	DBusMessage *reply = NULL;
	char **eap_methods;
	size_t num_items = 0;

	eap_methods = eap_get_names_as_string_array(&num_items);
	if (!eap_methods) {
		return dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					      NULL);
	}

	reply = wpas_dbus_simple_array_property_getter(message,
						       DBUS_TYPE_STRING,
						       eap_methods, num_items);

	while (num_items)
		os_free(eap_methods[--num_items]);
	os_free(eap_methods);
	return reply;
}


static int wpas_dbus_get_scan_type(DBusMessage *message, DBusMessageIter *var,
				   char **type, DBusMessage **reply)
{
	if (dbus_message_iter_get_arg_type(var) != DBUS_TYPE_STRING) {
		wpa_printf(MSG_DEBUG, "wpas_dbus_handler_scan[dbus]: "
			   "Type must be a string");
		*reply = wpas_dbus_error_invalid_args(
			message, "Wrong Type value type. String required");
		return -1;
	}
	dbus_message_iter_get_basic(var, type);
	return 0;
}


static int wpas_dbus_get_scan_ssids(DBusMessage *message, DBusMessageIter *var,
				    struct wpa_driver_scan_params *params,
				    DBusMessage **reply)
{
	struct wpa_driver_scan_ssid *ssids = params->ssids;
	size_t ssids_num = 0;
	u8 *ssid;
	DBusMessageIter array_iter, sub_array_iter;
	char *val;
	int len;

	if (dbus_message_iter_get_arg_type(var) != DBUS_TYPE_ARRAY) {
		wpa_printf(MSG_DEBUG, "wpas_dbus_handler_scan[dbus]: ssids "
			   "must be an array of arrays of bytes");
		*reply = wpas_dbus_error_invalid_args(
			message, "Wrong SSIDs value type. Array of arrays of "
			"bytes required");
		return -1;
	}

	dbus_message_iter_recurse(var, &array_iter);

	if (dbus_message_iter_get_arg_type(&array_iter) != DBUS_TYPE_ARRAY ||
	    dbus_message_iter_get_element_type(&array_iter) != DBUS_TYPE_BYTE)
	{
		wpa_printf(MSG_DEBUG, "wpas_dbus_handler_scan[dbus]: ssids "
			   "must be an array of arrays of bytes");
		*reply = wpas_dbus_error_invalid_args(
			message, "Wrong SSIDs value type. Array of arrays of "
			"bytes required");
		return -1;
	}

	while (dbus_message_iter_get_arg_type(&array_iter) == DBUS_TYPE_ARRAY)
	{
		if (ssids_num >= WPAS_MAX_SCAN_SSIDS) {
			wpa_printf(MSG_DEBUG, "wpas_dbus_handler_scan[dbus]: "
				   "Too many ssids specified on scan dbus "
				   "call");
			*reply = wpas_dbus_error_invalid_args(
				message, "Too many ssids specified. Specify "
				"at most four");
			return -1;
		}

		dbus_message_iter_recurse(&array_iter, &sub_array_iter);

		dbus_message_iter_get_fixed_array(&sub_array_iter, &val, &len);
		if (len == 0) {
			dbus_message_iter_next(&array_iter);
			continue;
		}

		ssid = os_malloc(len);
		if (ssid == NULL) {
			wpa_printf(MSG_DEBUG, "wpas_dbus_handler_scan[dbus]: "
				   "out of memory. Cannot allocate memory for "
				   "SSID");
			*reply = dbus_message_new_error(
				message, DBUS_ERROR_NO_MEMORY, NULL);
			return -1;
		}
		os_memcpy(ssid, val, len);
		ssids[ssids_num].ssid = ssid;
		ssids[ssids_num].ssid_len = len;

		dbus_message_iter_next(&array_iter);
		ssids_num++;
	}

	params->num_ssids = ssids_num;
	return 0;
}


static int wpas_dbus_get_scan_ies(DBusMessage *message, DBusMessageIter *var,
				  struct wpa_driver_scan_params *params,
				  DBusMessage **reply)
{
	u8 *ies = NULL, *nies;
	int ies_len = 0;
	DBusMessageIter array_iter, sub_array_iter;
	char *val;
	int len;

	if (dbus_message_iter_get_arg_type(var) != DBUS_TYPE_ARRAY) {
		wpa_printf(MSG_DEBUG, "wpas_dbus_handler_scan[dbus]: ies must "
			   "be an array of arrays of bytes");
		*reply = wpas_dbus_error_invalid_args(
			message, "Wrong IEs value type. Array of arrays of "
			"bytes required");
		return -1;
	}

	dbus_message_iter_recurse(var, &array_iter);

	if (dbus_message_iter_get_arg_type(&array_iter) != DBUS_TYPE_ARRAY ||
	    dbus_message_iter_get_element_type(&array_iter) != DBUS_TYPE_BYTE)
	{
		wpa_printf(MSG_DEBUG, "wpas_dbus_handler_scan[dbus]: ies must "
			   "be an array of arrays of bytes");
		*reply = wpas_dbus_error_invalid_args(
			message, "Wrong IEs value type. Array required");
		return -1;
	}

	while (dbus_message_iter_get_arg_type(&array_iter) == DBUS_TYPE_ARRAY)
	{
		dbus_message_iter_recurse(&array_iter, &sub_array_iter);

		dbus_message_iter_get_fixed_array(&sub_array_iter, &val, &len);
		if (len == 0) {
			dbus_message_iter_next(&array_iter);
			continue;
		}

		nies = os_realloc(ies, ies_len + len);
		if (nies == NULL) {
			wpa_printf(MSG_DEBUG, "wpas_dbus_handler_scan[dbus]: "
				   "out of memory. Cannot allocate memory for "
				   "IE");
			os_free(ies);
			*reply = dbus_message_new_error(
				message, DBUS_ERROR_NO_MEMORY, NULL);
			return -1;
		}
		ies = nies;
		os_memcpy(ies + ies_len, val, len);
		ies_len += len;

		dbus_message_iter_next(&array_iter);
	}

	params->extra_ies = ies;
	params->extra_ies_len = ies_len;
	return 0;
}


static int wpas_dbus_get_scan_channels(DBusMessage *message,
				       DBusMessageIter *var,
				       struct wpa_driver_scan_params *params,
				       DBusMessage **reply)
{
	DBusMessageIter array_iter, sub_array_iter;
	int *freqs = NULL, *nfreqs;
	int freqs_num = 0;

	if (dbus_message_iter_get_arg_type(var) != DBUS_TYPE_ARRAY) {
		wpa_printf(MSG_DEBUG, "wpas_dbus_handler_scan[dbus]: "
			   "Channels must be an array of structs");
		*reply = wpas_dbus_error_invalid_args(
			message, "Wrong Channels value type. Array of structs "
			"required");
		return -1;
	}

	dbus_message_iter_recurse(var, &array_iter);

	if (dbus_message_iter_get_arg_type(&array_iter) != DBUS_TYPE_STRUCT) {
		wpa_printf(MSG_DEBUG,
			   "wpas_dbus_handler_scan[dbus]: Channels must be an "
			   "array of structs");
		*reply = wpas_dbus_error_invalid_args(
			message, "Wrong Channels value type. Array of structs "
			"required");
		return -1;
	}

	while (dbus_message_iter_get_arg_type(&array_iter) == DBUS_TYPE_STRUCT)
	{
		int freq, width;

		dbus_message_iter_recurse(&array_iter, &sub_array_iter);

		if (dbus_message_iter_get_arg_type(&sub_array_iter) !=
		    DBUS_TYPE_UINT32) {
			wpa_printf(MSG_DEBUG, "wpas_dbus_handler_scan[dbus]: "
				   "Channel must by specified by struct of "
				   "two UINT32s %c",
				   dbus_message_iter_get_arg_type(
					   &sub_array_iter));
			*reply = wpas_dbus_error_invalid_args(
				message, "Wrong Channel struct. Two UINT32s "
				"required");
			os_free(freqs);
			return -1;
		}
		dbus_message_iter_get_basic(&sub_array_iter, &freq);

		if (!dbus_message_iter_next(&sub_array_iter) ||
		    dbus_message_iter_get_arg_type(&sub_array_iter) !=
		    DBUS_TYPE_UINT32) {
			wpa_printf(MSG_DEBUG, "wpas_dbus_handler_scan[dbus]: "
				   "Channel must by specified by struct of "
				   "two UINT32s");
			*reply = wpas_dbus_error_invalid_args(
				message,
				"Wrong Channel struct. Two UINT32s required");
			os_free(freqs);
			return -1;
		}

		dbus_message_iter_get_basic(&sub_array_iter, &width);

#define FREQS_ALLOC_CHUNK 32
		if (freqs_num % FREQS_ALLOC_CHUNK == 0) {
			nfreqs = os_realloc(freqs, sizeof(int) *
					    (freqs_num + FREQS_ALLOC_CHUNK));
			if (nfreqs == NULL)
				os_free(freqs);
			freqs = nfreqs;
		}
		if (freqs == NULL) {
			wpa_printf(MSG_DEBUG, "wpas_dbus_handler_scan[dbus]: "
				   "out of memory. can't allocate memory for "
				   "freqs");
			*reply = dbus_message_new_error(
				message, DBUS_ERROR_NO_MEMORY, NULL);
			return -1;
		}

		freqs[freqs_num] = freq;

		freqs_num++;
		dbus_message_iter_next(&array_iter);
	}

	nfreqs = os_realloc(freqs,
			    sizeof(int) * (freqs_num + 1));
	if (nfreqs == NULL)
		os_free(freqs);
	freqs = nfreqs;
	if (freqs == NULL) {
		wpa_printf(MSG_DEBUG, "wpas_dbus_handler_scan[dbus]: "
			   "out of memory. Can't allocate memory for freqs");
		*reply = dbus_message_new_error(
			message, DBUS_ERROR_NO_MEMORY, NULL);
		return -1;
	}
	freqs[freqs_num] = 0;

	params->freqs = freqs;
	return 0;
}


/**
 * wpas_dbus_handler_scan - Request a wireless scan on an interface
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: NULL indicating success or DBus error message on failure
 *
 * Handler function for "Scan" method call of a network device. Requests
 * that wpa_supplicant perform a wireless scan as soon as possible
 * on a particular wireless interface.
 */
DBusMessage * wpas_dbus_handler_scan(DBusMessage *message,
				     struct wpa_supplicant *wpa_s)
{
	DBusMessage *reply = NULL;
	DBusMessageIter iter, dict_iter, entry_iter, variant_iter;
	char *key = NULL, *type = NULL;
	struct wpa_driver_scan_params params;
	size_t i;

	os_memset(&params, 0, sizeof(params));

	dbus_message_iter_init(message, &iter);

	dbus_message_iter_recurse(&iter, &dict_iter);

	while (dbus_message_iter_get_arg_type(&dict_iter) ==
			DBUS_TYPE_DICT_ENTRY) {
		dbus_message_iter_recurse(&dict_iter, &entry_iter);
		dbus_message_iter_get_basic(&entry_iter, &key);
		dbus_message_iter_next(&entry_iter);
		dbus_message_iter_recurse(&entry_iter, &variant_iter);

		if (os_strcmp(key, "Type") == 0) {
			if (wpas_dbus_get_scan_type(message, &variant_iter,
						    &type, &reply) < 0)
				goto out;
		} else if (os_strcmp(key, "SSIDs") == 0) {
			if (wpas_dbus_get_scan_ssids(message, &variant_iter,
						     &params, &reply) < 0)
				goto out;
		} else if (os_strcmp(key, "IEs") == 0) {
			if (wpas_dbus_get_scan_ies(message, &variant_iter,
						   &params, &reply) < 0)
				goto out;
		} else if (os_strcmp(key, "Channels") == 0) {
			if (wpas_dbus_get_scan_channels(message, &variant_iter,
							&params, &reply) < 0)
				goto out;
		} else {
			wpa_printf(MSG_DEBUG, "wpas_dbus_handler_scan[dbus]: "
				   "Unknown argument %s", key);
			reply = wpas_dbus_error_invalid_args(message, key);
			goto out;
		}

		dbus_message_iter_next(&dict_iter);
	}

	if (!type) {
		wpa_printf(MSG_DEBUG, "wpas_dbus_handler_scan[dbus]: "
			   "Scan type not specified");
		reply = wpas_dbus_error_invalid_args(message, key);
		goto out;
	}

	if (!os_strcmp(type, "passive")) {
		if (params.num_ssids || params.extra_ies_len) {
			wpa_printf(MSG_DEBUG, "wpas_dbus_handler_scan[dbus]: "
				   "SSIDs or IEs specified for passive scan.");
			reply = wpas_dbus_error_invalid_args(
				message, "You can specify only Channels in "
				"passive scan");
			goto out;
		} else if (params.freqs && params.freqs[0]) {
			/* wildcard ssid */
			params.num_ssids++;
			wpa_supplicant_trigger_scan(wpa_s, &params);
		} else {
			wpa_s->scan_req = 2;
			wpa_supplicant_req_scan(wpa_s, 0, 0);
		}
	} else if (!os_strcmp(type, "active")) {
		wpa_supplicant_trigger_scan(wpa_s, &params);
	} else {
		wpa_printf(MSG_DEBUG, "wpas_dbus_handler_scan[dbus]: "
			   "Unknown scan type: %s", type);
		reply = wpas_dbus_error_invalid_args(message,
						     "Wrong scan type");
		goto out;
	}

out:
	for (i = 0; i < WPAS_MAX_SCAN_SSIDS; i++)
		os_free((u8 *) params.ssids[i].ssid);
	os_free((u8 *) params.extra_ies);
	os_free(params.freqs);
	return reply;
}


/*
 * wpas_dbus_handler_disconnect - Terminate the current connection
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: NotConnected DBus error message if already not connected
 * or NULL otherwise.
 *
 * Handler function for "Disconnect" method call of network interface.
 */
DBusMessage * wpas_dbus_handler_disconnect(DBusMessage *message,
					   struct wpa_supplicant *wpa_s)
{
	if (wpa_s->current_ssid != NULL) {
		wpa_s->disconnected = 1;
		wpa_supplicant_deauthenticate(wpa_s,
					      WLAN_REASON_DEAUTH_LEAVING);

		return NULL;
	}

	return dbus_message_new_error(message, WPAS_DBUS_ERROR_NOT_CONNECTED,
				      "This interface is not connected");
}


/**
 * wpas_dbus_new_iface_add_network - Add a new configured network
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: A dbus message containing the object path of the new network
 *
 * Handler function for "AddNetwork" method call of a network interface.
 */
DBusMessage * wpas_dbus_handler_add_network(DBusMessage *message,
					    struct wpa_supplicant *wpa_s)
{
	DBusMessage *reply = NULL;
	DBusMessageIter	iter;
	struct wpa_ssid *ssid = NULL;
	char path_buf[WPAS_DBUS_OBJECT_PATH_MAX], *path = path_buf;

	dbus_message_iter_init(message, &iter);

	ssid = wpa_config_add_network(wpa_s->conf);
	if (ssid == NULL) {
		wpa_printf(MSG_ERROR, "wpas_dbus_handler_add_network[dbus]: "
			   "can't add new interface.");
		reply = wpas_dbus_error_unknown_error(
			message,
			"wpa_supplicant could not add "
			"a network on this interface.");
		goto err;
	}
	wpas_notify_network_added(wpa_s, ssid);
	ssid->disabled = 1;
	wpa_config_set_network_defaults(ssid);

	reply = set_network_properties(message, wpa_s, ssid, &iter);
	if (reply) {
		wpa_printf(MSG_DEBUG, "wpas_dbus_handler_add_network[dbus]:"
			   "control interface couldn't set network "
			   "properties");
		goto err;
	}

	/* Construct the object path for this network. */
	os_snprintf(path, WPAS_DBUS_OBJECT_PATH_MAX,
		    "%s/" WPAS_DBUS_NEW_NETWORKS_PART "/%d",
		    wpa_s->dbus_new_path, ssid->id);

	reply = dbus_message_new_method_return(message);
	if (reply == NULL) {
		reply = dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					       NULL);
		goto err;
	}
	if (!dbus_message_append_args(reply, DBUS_TYPE_OBJECT_PATH, &path,
				      DBUS_TYPE_INVALID)) {
		dbus_message_unref(reply);
		reply = dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					       NULL);
		goto err;
	}

	return reply;

err:
	if (ssid) {
		wpas_notify_network_removed(wpa_s, ssid);
		wpa_config_remove_network(wpa_s->conf, ssid->id);
	}
	return reply;
}


/**
 * wpas_dbus_handler_remove_network - Remove a configured network
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: NULL on success or dbus error on failure
 *
 * Handler function for "RemoveNetwork" method call of a network interface.
 */
DBusMessage * wpas_dbus_handler_remove_network(DBusMessage *message,
					       struct wpa_supplicant *wpa_s)
{
	DBusMessage *reply = NULL;
	const char *op;
	char *iface = NULL, *net_id = NULL;
	int id;
	struct wpa_ssid *ssid;

	dbus_message_get_args(message, NULL, DBUS_TYPE_OBJECT_PATH, &op,
			      DBUS_TYPE_INVALID);

	/* Extract the network ID and ensure the network */
	/* is actually a child of this interface */
	iface = wpas_dbus_new_decompose_object_path(op, &net_id, NULL);
	if (iface == NULL || os_strcmp(iface, wpa_s->dbus_new_path) != 0) {
		reply = wpas_dbus_error_invalid_args(message, op);
		goto out;
	}

	id = strtoul(net_id, NULL, 10);
	if (errno == EINVAL) {
		reply = wpas_dbus_error_invalid_args(message, op);
		goto out;
	}

	ssid = wpa_config_get_network(wpa_s->conf, id);
	if (ssid == NULL) {
		reply = wpas_dbus_error_network_unknown(message);
		goto out;
	}

	wpas_notify_network_removed(wpa_s, ssid);

	if (wpa_config_remove_network(wpa_s->conf, id) < 0) {
		wpa_printf(MSG_ERROR,
			   "wpas_dbus_handler_remove_network[dbus]: "
			   "error occurred when removing network %d", id);
		reply = wpas_dbus_error_unknown_error(
			message, "error removing the specified network on "
			"this interface.");
		goto out;
	}

	if (ssid == wpa_s->current_ssid)
		wpa_supplicant_deauthenticate(wpa_s,
					      WLAN_REASON_DEAUTH_LEAVING);

out:
	os_free(iface);
	os_free(net_id);
	return reply;
}


/**
 * wpas_dbus_handler_select_network - Attempt association with a network
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: NULL on success or dbus error on failure
 *
 * Handler function for "SelectNetwork" method call of network interface.
 */
DBusMessage * wpas_dbus_handler_select_network(DBusMessage *message,
					       struct wpa_supplicant *wpa_s)
{
	DBusMessage *reply = NULL;
	const char *op;
	char *iface = NULL, *net_id = NULL;
	int id;
	struct wpa_ssid *ssid;

	dbus_message_get_args(message, NULL, DBUS_TYPE_OBJECT_PATH, &op,
			      DBUS_TYPE_INVALID);

	/* Extract the network ID and ensure the network */
	/* is actually a child of this interface */
	iface = wpas_dbus_new_decompose_object_path(op, &net_id, NULL);
	if (iface == NULL || os_strcmp(iface, wpa_s->dbus_new_path) != 0) {
		reply = wpas_dbus_error_invalid_args(message, op);
		goto out;
	}

	id = strtoul(net_id, NULL, 10);
	if (errno == EINVAL) {
		reply = wpas_dbus_error_invalid_args(message, op);
		goto out;
	}

	ssid = wpa_config_get_network(wpa_s->conf, id);
	if (ssid == NULL) {
		reply = wpas_dbus_error_network_unknown(message);
		goto out;
	}

	/* Finally, associate with the network */
	wpa_supplicant_select_network(wpa_s, ssid);

out:
	os_free(iface);
	os_free(net_id);
	return reply;
}


/**
 * wpas_dbus_handler_add_blob - Store named binary blob (ie, for certificates)
 * @message: Pointer to incoming dbus message
 * @wpa_s: %wpa_supplicant data structure
 * Returns: A dbus message containing an error on failure or NULL on success
 *
 * Asks wpa_supplicant to internally store a binary blobs.
 */
DBusMessage * wpas_dbus_handler_add_blob(DBusMessage *message,
					 struct wpa_supplicant *wpa_s)
{
	DBusMessage *reply = NULL;
	DBusMessageIter	iter, array_iter;

	char *blob_name;
	u8 *blob_data;
	int blob_len;
	struct wpa_config_blob *blob = NULL;

	dbus_message_iter_init(message, &iter);
	dbus_message_iter_get_basic(&iter, &blob_name);

	if (wpa_config_get_blob(wpa_s->conf, blob_name)) {
		return dbus_message_new_error(message,
					      WPAS_DBUS_ERROR_BLOB_EXISTS,
					      NULL);
	}

	dbus_message_iter_next(&iter);
	dbus_message_iter_recurse(&iter, &array_iter);

	dbus_message_iter_get_fixed_array(&array_iter, &blob_data, &blob_len);

	blob = os_zalloc(sizeof(*blob));
	if (!blob) {
		reply = dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					       NULL);
		goto err;
	}

	blob->data = os_malloc(blob_len);
	if (!blob->data) {
		reply = dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					       NULL);
		goto err;
	}
	os_memcpy(blob->data, blob_data, blob_len);

	blob->len = blob_len;
	blob->name = os_strdup(blob_name);
	if (!blob->name) {
		reply = dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					       NULL);
		goto err;
	}

	wpa_config_set_blob(wpa_s->conf, blob);
	wpas_notify_blob_added(wpa_s, blob->name);

	return reply;

err:
	if (blob) {
		os_free(blob->name);
		os_free(blob->data);
		os_free(blob);
	}
	return reply;
}


/**
 * wpas_dbus_handler_get_blob - Get named binary blob (ie, for certificates)
 * @message: Pointer to incoming dbus message
 * @wpa_s: %wpa_supplicant data structure
 * Returns: A dbus message containing array of bytes (blob)
 *
 * Gets one wpa_supplicant's binary blobs.
 */
DBusMessage * wpas_dbus_handler_get_blob(DBusMessage *message,
					 struct wpa_supplicant *wpa_s)
{
	DBusMessage *reply = NULL;
	DBusMessageIter	iter, array_iter;

	char *blob_name;
	const struct wpa_config_blob *blob;

	dbus_message_get_args(message, NULL, DBUS_TYPE_STRING, &blob_name,
			      DBUS_TYPE_INVALID);

	blob = wpa_config_get_blob(wpa_s->conf, blob_name);
	if (!blob) {
		return dbus_message_new_error(message,
					      WPAS_DBUS_ERROR_BLOB_UNKNOWN,
					      "Blob id not set");
	}

	reply = dbus_message_new_method_return(message);
	if (!reply) {
		reply = dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					       NULL);
		goto out;
	}

	dbus_message_iter_init_append(reply, &iter);

	if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					      DBUS_TYPE_BYTE_AS_STRING,
					      &array_iter)) {
		dbus_message_unref(reply);
		reply = dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					       NULL);
		goto out;
	}

	if (!dbus_message_iter_append_fixed_array(&array_iter, DBUS_TYPE_BYTE,
						  &(blob->data), blob->len)) {
		dbus_message_unref(reply);
		reply = dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					       NULL);
		goto out;
	}

	if (!dbus_message_iter_close_container(&iter, &array_iter)) {
		dbus_message_unref(reply);
		reply = dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					       NULL);
		goto out;
	}

out:
	return reply;
}


/**
 * wpas_remove_handler_remove_blob - Remove named binary blob
 * @message: Pointer to incoming dbus message
 * @wpa_s: %wpa_supplicant data structure
 * Returns: NULL on success or dbus error
 *
 * Asks wpa_supplicant to internally remove a binary blobs.
 */
DBusMessage * wpas_dbus_handler_remove_blob(DBusMessage *message,
					    struct wpa_supplicant *wpa_s)
{
	DBusMessage *reply = NULL;
	char *blob_name;

	dbus_message_get_args(message, NULL, DBUS_TYPE_STRING, &blob_name,
			      DBUS_TYPE_INVALID);

	if (wpa_config_remove_blob(wpa_s->conf, blob_name)) {
		return dbus_message_new_error(message,
					      WPAS_DBUS_ERROR_BLOB_UNKNOWN,
					      "Blob id not set");
	}
	wpas_notify_blob_removed(wpa_s, blob_name);

	return reply;

}


/**
 * wpas_dbus_getter_capabilities - Return interface capabilities
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: A dbus message containing a dict of strings
 *
 * Getter for "Capabilities" property of an interface.
 */
DBusMessage * wpas_dbus_getter_capabilities(DBusMessage *message,
					    struct wpa_supplicant *wpa_s)
{
	DBusMessage *reply = NULL;
	struct wpa_driver_capa capa;
	int res;
	DBusMessageIter iter, iter_dict;
	DBusMessageIter iter_dict_entry, iter_dict_val, iter_array,
		variant_iter;
	const char *scans[] = { "active", "passive", "ssid" };
	const char *modes[] = { "infrastructure", "ad-hoc", "ap" };
	int n = sizeof(modes) / sizeof(char *);

	if (message == NULL)
		reply = dbus_message_new(DBUS_MESSAGE_TYPE_SIGNAL);
	else
		reply = dbus_message_new_method_return(message);
	if (!reply)
		goto nomem;

	dbus_message_iter_init_append(reply, &iter);
	if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT,
					      "a{sv}", &variant_iter))
		goto nomem;

	if (!wpa_dbus_dict_open_write(&variant_iter, &iter_dict))
		goto nomem;

	res = wpa_drv_get_capa(wpa_s, &capa);

	/***** pairwise cipher */
	if (res < 0) {
		const char *args[] = {"ccmp", "tkip", "none"};
		if (!wpa_dbus_dict_append_string_array(
			    &iter_dict, "Pairwise", args,
			    sizeof(args) / sizeof(char*)))
			goto nomem;
	} else {
		if (!wpa_dbus_dict_begin_string_array(&iter_dict, "Pairwise",
						      &iter_dict_entry,
						      &iter_dict_val,
						      &iter_array))
			goto nomem;

		if (capa.enc & WPA_DRIVER_CAPA_ENC_CCMP) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "ccmp"))
				goto nomem;
		}

		if (capa.enc & WPA_DRIVER_CAPA_ENC_TKIP) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "tkip"))
				goto nomem;
		}

		if (capa.key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_WPA_NONE) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "none"))
				goto nomem;
		}

		if (!wpa_dbus_dict_end_string_array(&iter_dict,
						    &iter_dict_entry,
						    &iter_dict_val,
						    &iter_array))
			goto nomem;
	}

	/***** group cipher */
	if (res < 0) {
		const char *args[] = {
			"ccmp", "tkip", "wep104", "wep40"
		};
		if (!wpa_dbus_dict_append_string_array(
			    &iter_dict, "Group", args,
			    sizeof(args) / sizeof(char*)))
			goto nomem;
	} else {
		if (!wpa_dbus_dict_begin_string_array(&iter_dict, "Group",
						      &iter_dict_entry,
						      &iter_dict_val,
						      &iter_array))
			goto nomem;

		if (capa.enc & WPA_DRIVER_CAPA_ENC_CCMP) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "ccmp"))
				goto nomem;
		}

		if (capa.enc & WPA_DRIVER_CAPA_ENC_TKIP) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "tkip"))
				goto nomem;
		}

		if (capa.enc & WPA_DRIVER_CAPA_ENC_WEP104) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "wep104"))
				goto nomem;
		}

		if (capa.enc & WPA_DRIVER_CAPA_ENC_WEP40) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "wep40"))
				goto nomem;
		}

		if (!wpa_dbus_dict_end_string_array(&iter_dict,
						    &iter_dict_entry,
						    &iter_dict_val,
						    &iter_array))
			goto nomem;
	}

	/***** key management */
	if (res < 0) {
		const char *args[] = {
			"wpa-psk", "wpa-eap", "ieee8021x", "wpa-none",
#ifdef CONFIG_WPS
			"wps",
#endif /* CONFIG_WPS */
			"none"
		};
		if (!wpa_dbus_dict_append_string_array(
			    &iter_dict, "KeyMgmt", args,
			    sizeof(args) / sizeof(char*)))
			goto nomem;
	} else {
		if (!wpa_dbus_dict_begin_string_array(&iter_dict, "KeyMgmt",
						      &iter_dict_entry,
						      &iter_dict_val,
						      &iter_array))
			goto nomem;

		if (!wpa_dbus_dict_string_array_add_element(&iter_array,
							    "none"))
			goto nomem;

		if (!wpa_dbus_dict_string_array_add_element(&iter_array,
							    "ieee8021x"))
			goto nomem;

		if (capa.key_mgmt & (WPA_DRIVER_CAPA_KEY_MGMT_WPA |
				     WPA_DRIVER_CAPA_KEY_MGMT_WPA2)) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "wpa-eap"))
				goto nomem;

			if (capa.key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_FT)
				if (!wpa_dbus_dict_string_array_add_element(
					    &iter_array, "wpa-ft-eap"))
					goto nomem;

/* TODO: Ensure that driver actually supports sha256 encryption. */
#ifdef CONFIG_IEEE80211W
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "wpa-eap-sha256"))
				goto nomem;
#endif /* CONFIG_IEEE80211W */
		}

		if (capa.key_mgmt & (WPA_DRIVER_CAPA_KEY_MGMT_WPA_PSK |
				     WPA_DRIVER_CAPA_KEY_MGMT_WPA2_PSK)) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "wpa-psk"))
				goto nomem;

			if (capa.key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_FT_PSK)
				if (!wpa_dbus_dict_string_array_add_element(
					    &iter_array, "wpa-ft-psk"))
					goto nomem;

/* TODO: Ensure that driver actually supports sha256 encryption. */
#ifdef CONFIG_IEEE80211W
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "wpa-psk-sha256"))
				goto nomem;
#endif /* CONFIG_IEEE80211W */
		}

		if (capa.key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_WPA_NONE) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "wpa-none"))
				goto nomem;
		}


#ifdef CONFIG_WPS
		if (!wpa_dbus_dict_string_array_add_element(&iter_array,
							    "wps"))
			goto nomem;
#endif /* CONFIG_WPS */

		if (!wpa_dbus_dict_end_string_array(&iter_dict,
						    &iter_dict_entry,
						    &iter_dict_val,
						    &iter_array))
			goto nomem;
	}

	/***** WPA protocol */
	if (res < 0) {
		const char *args[] = { "rsn", "wpa" };
		if (!wpa_dbus_dict_append_string_array(
			    &iter_dict, "Protocol", args,
			    sizeof(args) / sizeof(char*)))
			goto nomem;
	} else {
		if (!wpa_dbus_dict_begin_string_array(&iter_dict, "Protocol",
						      &iter_dict_entry,
						      &iter_dict_val,
						      &iter_array))
			goto nomem;

		if (capa.key_mgmt & (WPA_DRIVER_CAPA_KEY_MGMT_WPA2 |
				     WPA_DRIVER_CAPA_KEY_MGMT_WPA2_PSK)) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "rsn"))
				goto nomem;
		}

		if (capa.key_mgmt & (WPA_DRIVER_CAPA_KEY_MGMT_WPA |
				     WPA_DRIVER_CAPA_KEY_MGMT_WPA_PSK)) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "wpa"))
				goto nomem;
		}

		if (!wpa_dbus_dict_end_string_array(&iter_dict,
						    &iter_dict_entry,
						    &iter_dict_val,
						    &iter_array))
			goto nomem;
	}

	/***** auth alg */
	if (res < 0) {
		const char *args[] = { "open", "shared", "leap" };
		if (!wpa_dbus_dict_append_string_array(
			    &iter_dict, "AuthAlg", args,
			    sizeof(args) / sizeof(char*)))
			goto nomem;
	} else {
		if (!wpa_dbus_dict_begin_string_array(&iter_dict, "AuthAlg",
						      &iter_dict_entry,
						      &iter_dict_val,
						      &iter_array))
			goto nomem;

		if (capa.auth & (WPA_DRIVER_AUTH_OPEN)) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "open"))
				goto nomem;
		}

		if (capa.auth & (WPA_DRIVER_AUTH_SHARED)) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "shared"))
				goto nomem;
		}

		if (capa.auth & (WPA_DRIVER_AUTH_LEAP)) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "leap"))
				goto nomem;
		}

		if (!wpa_dbus_dict_end_string_array(&iter_dict,
						    &iter_dict_entry,
						    &iter_dict_val,
						    &iter_array))
			goto nomem;
	}

	/***** Scan */
	if (!wpa_dbus_dict_append_string_array(&iter_dict, "Scan", scans,
					       sizeof(scans) / sizeof(char *)))
		goto nomem;

	/***** Modes */
	if (res < 0 || !(capa.flags & WPA_DRIVER_FLAGS_AP))
		n--; /* exclude ap mode if it is not supported by the driver */
	if (!wpa_dbus_dict_append_string_array(&iter_dict, "Modes", modes, n))
		goto nomem;

	if (!wpa_dbus_dict_close_write(&variant_iter, &iter_dict))
		goto nomem;
	if (!dbus_message_iter_close_container(&iter, &variant_iter))
		goto nomem;

	return reply;

nomem:
	if (reply)
		dbus_message_unref(reply);

	return dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY, NULL);
}


/**
 * wpas_dbus_getter_state - Get interface state
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: A dbus message containing a STRING representing the current
 *          interface state
 *
 * Getter for "State" property.
 */
DBusMessage * wpas_dbus_getter_state(DBusMessage *message,
				     struct wpa_supplicant *wpa_s)
{
	DBusMessage *reply = NULL;
	const char *str_state;
	char *state_ls, *tmp;

	str_state = wpa_supplicant_state_txt(wpa_s->wpa_state);

	/* make state string lowercase to fit new DBus API convention
	 */
	state_ls = tmp = os_strdup(str_state);
	if (!tmp) {
		return dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					      NULL);
	}
	while (*tmp) {
		*tmp = tolower(*tmp);
		tmp++;
	}

	reply = wpas_dbus_simple_property_getter(message, DBUS_TYPE_STRING,
						 &state_ls);

	os_free(state_ls);

	return reply;
}


/**
 * wpas_dbus_new_iface_get_scanning - Get interface scanning state
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: A dbus message containing whether the interface is scanning
 *
 * Getter for "scanning" property.
 */
DBusMessage * wpas_dbus_getter_scanning(DBusMessage *message,
					struct wpa_supplicant *wpa_s)
{
	dbus_bool_t scanning = wpa_s->scanning ? TRUE : FALSE;
	return wpas_dbus_simple_property_getter(message, DBUS_TYPE_BOOLEAN,
						&scanning);
}


/**
 * wpas_dbus_getter_ap_scan - Control roaming mode
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: A message containong value of ap_scan variable
 *
 * Getter function for "ApScan" property.
 */
DBusMessage * wpas_dbus_getter_ap_scan(DBusMessage *message,
				       struct wpa_supplicant *wpa_s)
{
	dbus_uint32_t ap_scan = wpa_s->conf->ap_scan;
	return wpas_dbus_simple_property_getter(message, DBUS_TYPE_UINT32,
						&ap_scan);
}


/**
 * wpas_dbus_setter_ap_scan - Control roaming mode
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: NULL
 *
 * Setter function for "ApScan" property.
 */
DBusMessage * wpas_dbus_setter_ap_scan(DBusMessage *message,
				       struct wpa_supplicant *wpa_s)
{
	DBusMessage *reply = NULL;
	dbus_uint32_t ap_scan;

	reply = wpas_dbus_simple_property_setter(message, DBUS_TYPE_UINT32,
						 &ap_scan);
	if (reply)
		return reply;

	if (wpa_supplicant_set_ap_scan(wpa_s, ap_scan)) {
		return wpas_dbus_error_invalid_args(
			message, "ap_scan must equal 0, 1 or 2");
	}
	return NULL;
}


/**
 * wpas_dbus_getter_ifname - Get interface name
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: A dbus message containing a name of network interface
 * associated with with wpa_s
 *
 * Getter for "Ifname" property.
 */
DBusMessage * wpas_dbus_getter_ifname(DBusMessage *message,
				      struct wpa_supplicant *wpa_s)
{
	const char *ifname = wpa_s->ifname;
	return wpas_dbus_simple_property_getter(message, DBUS_TYPE_STRING,
						&ifname);
}


/**
 * wpas_dbus_getter_driver - Get interface name
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: A dbus message containing a name of network interface
 * driver associated with with wpa_s
 *
 * Getter for "Driver" property.
 */
DBusMessage * wpas_dbus_getter_driver(DBusMessage *message,
				      struct wpa_supplicant *wpa_s)
{
	const char *driver;

	if (wpa_s->driver == NULL || wpa_s->driver->name == NULL) {
		wpa_printf(MSG_DEBUG, "wpas_dbus_getter_driver[dbus]: "
			   "wpa_s has no driver set");
		return wpas_dbus_error_unknown_error(message, NULL);
	}

	driver = wpa_s->driver->name;
	return wpas_dbus_simple_property_getter(message, DBUS_TYPE_STRING,
						&driver);
}


/**
 * wpas_dbus_getter_current_bss - Get current bss object path
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: A dbus message containing a DBus object path to
 * current BSS
 *
 * Getter for "CurrentBSS" property.
 */
DBusMessage * wpas_dbus_getter_current_bss(DBusMessage *message,
					   struct wpa_supplicant *wpa_s)
{
	DBusMessage *reply;
	char path_buf[WPAS_DBUS_OBJECT_PATH_MAX], *bss_obj_path = path_buf;

	if (wpa_s->current_bss)
		os_snprintf(bss_obj_path, WPAS_DBUS_OBJECT_PATH_MAX,
			    "%s/" WPAS_DBUS_NEW_BSSIDS_PART "/%u",
			    wpa_s->dbus_new_path, wpa_s->current_bss->id);
	else
		os_snprintf(bss_obj_path, WPAS_DBUS_OBJECT_PATH_MAX, "/");

	reply = wpas_dbus_simple_property_getter(message,
						 DBUS_TYPE_OBJECT_PATH,
						 &bss_obj_path);

	return reply;
}


/**
 * wpas_dbus_getter_current_network - Get current network object path
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: A dbus message containing a DBus object path to
 * current network
 *
 * Getter for "CurrentNetwork" property.
 */
DBusMessage * wpas_dbus_getter_current_network(DBusMessage *message,
					       struct wpa_supplicant *wpa_s)
{
	DBusMessage *reply;
	char path_buf[WPAS_DBUS_OBJECT_PATH_MAX], *net_obj_path = path_buf;

	if (wpa_s->current_ssid)
		os_snprintf(net_obj_path, WPAS_DBUS_OBJECT_PATH_MAX,
			    "%s/" WPAS_DBUS_NEW_NETWORKS_PART "/%u",
			    wpa_s->dbus_new_path, wpa_s->current_ssid->id);
	else
		os_snprintf(net_obj_path, WPAS_DBUS_OBJECT_PATH_MAX, "/");

	reply = wpas_dbus_simple_property_getter(message,
						 DBUS_TYPE_OBJECT_PATH,
						 &net_obj_path);

	return reply;
}


/**
 * wpas_dbus_getter_bridge_ifname - Get interface name
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: A dbus message containing a name of bridge network
 * interface associated with with wpa_s
 *
 * Getter for "BridgeIfname" property.
 */
DBusMessage * wpas_dbus_getter_bridge_ifname(DBusMessage *message,
					     struct wpa_supplicant *wpa_s)
{
	const char *bridge_ifname = NULL;

	bridge_ifname = wpa_s->bridge_ifname;
	if (bridge_ifname == NULL) {
		wpa_printf(MSG_ERROR, "wpas_dbus_getter_bridge_ifname[dbus]: "
			   "wpa_s has no bridge interface name set");
		return wpas_dbus_error_unknown_error(message, NULL);
	}

	return wpas_dbus_simple_property_getter(message, DBUS_TYPE_STRING,
						&bridge_ifname);
}


/**
 * wpas_dbus_getter_bsss - Get array of BSSs objects
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: a dbus message containing an array of all known BSS objects
 * dbus paths
 *
 * Getter for "BSSs" property.
 */
DBusMessage * wpas_dbus_getter_bsss(DBusMessage *message,
				    struct wpa_supplicant *wpa_s)
{
	DBusMessage *reply = NULL;
	struct wpa_bss *bss;
	char **paths;
	unsigned int i = 0;

	paths = os_zalloc(wpa_s->num_bss * sizeof(char *));
	if (!paths) {
		return dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					      NULL);
	}

	/* Loop through scan results and append each result's object path */
	dl_list_for_each(bss, &wpa_s->bss_id, struct wpa_bss, list_id) {
		paths[i] = os_zalloc(WPAS_DBUS_OBJECT_PATH_MAX);
		if (paths[i] == NULL) {
			reply = dbus_message_new_error(message,
						       DBUS_ERROR_NO_MEMORY,
						       NULL);
			goto out;
		}
		/* Construct the object path for this BSS. */
		os_snprintf(paths[i++], WPAS_DBUS_OBJECT_PATH_MAX,
			    "%s/" WPAS_DBUS_NEW_BSSIDS_PART "/%u",
			    wpa_s->dbus_new_path, bss->id);
	}

	reply = wpas_dbus_simple_array_property_getter(message,
						       DBUS_TYPE_OBJECT_PATH,
						       paths, wpa_s->num_bss);

out:
	while (i)
		os_free(paths[--i]);
	os_free(paths);
	return reply;
}


/**
 * wpas_dbus_getter_networks - Get array of networks objects
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: a dbus message containing an array of all configured
 * networks dbus object paths.
 *
 * Getter for "Networks" property.
 */
DBusMessage * wpas_dbus_getter_networks(DBusMessage *message,
					struct wpa_supplicant *wpa_s)
{
	DBusMessage *reply = NULL;
	struct wpa_ssid *ssid;
	char **paths;
	unsigned int i = 0, num = 0;

	if (wpa_s->conf == NULL) {
		wpa_printf(MSG_ERROR, "wpas_dbus_getter_networks[dbus]: "
			   "An error occurred getting networks list.");
		return wpas_dbus_error_unknown_error(message, NULL);
	}

	for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next)
		num++;

	paths = os_zalloc(num * sizeof(char *));
	if (!paths) {
		return dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					      NULL);
	}

	/* Loop through configured networks and append object path of each */
	for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next) {
		paths[i] = os_zalloc(WPAS_DBUS_OBJECT_PATH_MAX);
		if (paths[i] == NULL) {
			reply = dbus_message_new_error(message,
						       DBUS_ERROR_NO_MEMORY,
						       NULL);
			goto out;
		}

		/* Construct the object path for this network. */
		os_snprintf(paths[i++], WPAS_DBUS_OBJECT_PATH_MAX,
			    "%s/" WPAS_DBUS_NEW_NETWORKS_PART "/%d",
			    wpa_s->dbus_new_path, ssid->id);
	}

	reply = wpas_dbus_simple_array_property_getter(message,
						       DBUS_TYPE_OBJECT_PATH,
						       paths, num);

out:
	while (i)
		os_free(paths[--i]);
	os_free(paths);
	return reply;
}


/**
 * wpas_dbus_getter_blobs - Get all blobs defined for this interface
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: a dbus message containing a dictionary of pairs (blob_name, blob)
 *
 * Getter for "Blobs" property.
 */
DBusMessage * wpas_dbus_getter_blobs(DBusMessage *message,
				     struct wpa_supplicant *wpa_s)
{
	DBusMessage *reply = NULL;
	DBusMessageIter iter, variant_iter, dict_iter, entry_iter, array_iter;
	struct wpa_config_blob *blob;

	if (message == NULL)
		reply = dbus_message_new(DBUS_MESSAGE_TYPE_SIGNAL);
	else
		reply = dbus_message_new_method_return(message);
	if (!reply)
		return dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					      NULL);

	dbus_message_iter_init_append(reply, &iter);

	if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT,
					      "a{say}", &variant_iter) ||
	    !dbus_message_iter_open_container(&variant_iter, DBUS_TYPE_ARRAY,
					      "{say}", &dict_iter)) {
		dbus_message_unref(reply);
		return dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					      NULL);
	}

	blob = wpa_s->conf->blobs;
	while (blob) {
		if (!dbus_message_iter_open_container(&dict_iter,
						      DBUS_TYPE_DICT_ENTRY,
						      NULL, &entry_iter) ||
		    !dbus_message_iter_append_basic(&entry_iter,
						    DBUS_TYPE_STRING,
						    &(blob->name)) ||
		    !dbus_message_iter_open_container(&entry_iter,
						      DBUS_TYPE_ARRAY,
						      DBUS_TYPE_BYTE_AS_STRING,
						      &array_iter) ||
		    !dbus_message_iter_append_fixed_array(&array_iter,
							  DBUS_TYPE_BYTE,
							  &(blob->data),
							  blob->len) ||
		    !dbus_message_iter_close_container(&entry_iter,
						       &array_iter) ||
		    !dbus_message_iter_close_container(&dict_iter,
						       &entry_iter)) {
			dbus_message_unref(reply);
			return dbus_message_new_error(message,
						      DBUS_ERROR_NO_MEMORY,
						      NULL);
		}

		blob = blob->next;
	}

	if (!dbus_message_iter_close_container(&variant_iter, &dict_iter) ||
	    !dbus_message_iter_close_container(&iter, &variant_iter)) {
		dbus_message_unref(reply);
		return dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					      NULL);
	}

	return reply;
}


/**
 * wpas_dbus_getter_bss_bssid - Return the BSSID of a BSS
 * @message: Pointer to incoming dbus message
 * @bss: a pair of interface describing structure and bss's id
 * Returns: a dbus message containing the bssid for the requested bss
 *
 * Getter for "BSSID" property.
 */
DBusMessage * wpas_dbus_getter_bss_bssid(DBusMessage *message,
					 struct bss_handler_args *bss)
{
	struct wpa_bss *res = wpa_bss_get_id(bss->wpa_s, bss->id);

	if (!res) {
		wpa_printf(MSG_ERROR, "wpas_dbus_getter_bss_bssid[dbus]: no "
			   "bss with id %d found", bss->id);
		return NULL;
	}

	return wpas_dbus_simple_array_property_getter(message, DBUS_TYPE_BYTE,
						      res->bssid, ETH_ALEN);
}


/**
 * wpas_dbus_getter_bss_ssid - Return the SSID of a BSS
 * @message: Pointer to incoming dbus message
 * @bss: a pair of interface describing structure and bss's id
 * Returns: a dbus message containing the ssid for the requested bss
 *
 * Getter for "SSID" property.
 */
DBusMessage * wpas_dbus_getter_bss_ssid(DBusMessage *message,
					      struct bss_handler_args *bss)
{
	struct wpa_bss *res = wpa_bss_get_id(bss->wpa_s, bss->id);

	if (!res) {
		wpa_printf(MSG_ERROR, "wpas_dbus_getter_bss_ssid[dbus]: no "
			   "bss with id %d found", bss->id);
		return NULL;
	}

	return wpas_dbus_simple_array_property_getter(message, DBUS_TYPE_BYTE,
						      res->ssid,
						      res->ssid_len);
}


/**
 * wpas_dbus_getter_bss_privacy - Return the privacy flag of a BSS
 * @message: Pointer to incoming dbus message
 * @bss: a pair of interface describing structure and bss's id
 * Returns: a dbus message containing the privacy flag value of requested bss
 *
 * Getter for "Privacy" property.
 */
DBusMessage * wpas_dbus_getter_bss_privacy(DBusMessage *message,
					   struct bss_handler_args *bss)
{
	struct wpa_bss *res = wpa_bss_get_id(bss->wpa_s, bss->id);
	dbus_bool_t privacy;

	if (!res) {
		wpa_printf(MSG_ERROR, "wpas_dbus_getter_bss_privacy[dbus]: no "
			   "bss with id %d found", bss->id);
		return NULL;
	}

	privacy = (res->caps & IEEE80211_CAP_PRIVACY) ? TRUE : FALSE;
	return wpas_dbus_simple_property_getter(message, DBUS_TYPE_BOOLEAN,
						&privacy);
}


/**
 * wpas_dbus_getter_bss_mode - Return the mode of a BSS
 * @message: Pointer to incoming dbus message
 * @bss: a pair of interface describing structure and bss's id
 * Returns: a dbus message containing the mode of requested bss
 *
 * Getter for "Mode" property.
 */
DBusMessage * wpas_dbus_getter_bss_mode(DBusMessage *message,
					struct bss_handler_args *bss)
{
	struct wpa_bss *res = wpa_bss_get_id(bss->wpa_s, bss->id);
	const char *mode;

	if (!res) {
		wpa_printf(MSG_ERROR, "wpas_dbus_getter_bss_mode[dbus]: no "
			   "bss with id %d found", bss->id);
		return NULL;
	}

	if (res->caps & IEEE80211_CAP_IBSS)
		mode = "ad-hoc";
	else
		mode = "infrastructure";

	return wpas_dbus_simple_property_getter(message, DBUS_TYPE_STRING,
						&mode);
}


/**
 * wpas_dbus_getter_bss_level - Return the signal strength of a BSS
 * @message: Pointer to incoming dbus message
 * @bss: a pair of interface describing structure and bss's id
 * Returns: a dbus message containing the signal strength of requested bss
 *
 * Getter for "Level" property.
 */
DBusMessage * wpas_dbus_getter_bss_signal(DBusMessage *message,
					  struct bss_handler_args *bss)
{
	struct wpa_bss *res = wpa_bss_get_id(bss->wpa_s, bss->id);

	if (!res) {
		wpa_printf(MSG_ERROR, "wpas_dbus_getter_bss_signal[dbus]: no "
			   "bss with id %d found", bss->id);
		return NULL;
	}

	return wpas_dbus_simple_property_getter(message, DBUS_TYPE_INT16,
						&res->level);
}


/**
 * wpas_dbus_getter_bss_frequency - Return the frequency of a BSS
 * @message: Pointer to incoming dbus message
 * @bss: a pair of interface describing structure and bss's id
 * Returns: a dbus message containing the frequency of requested bss
 *
 * Getter for "Frequency" property.
 */
DBusMessage * wpas_dbus_getter_bss_frequency(DBusMessage *message,
					     struct bss_handler_args *bss)
{
	struct wpa_bss *res = wpa_bss_get_id(bss->wpa_s, bss->id);

	if (!res) {
		wpa_printf(MSG_ERROR, "wpas_dbus_getter_bss_frequency[dbus]: "
			   "no bss with id %d found", bss->id);
		return NULL;
	}

	return wpas_dbus_simple_property_getter(message, DBUS_TYPE_UINT16,
						&res->freq);
}


static int cmp_u8s_desc(const void *a, const void *b)
{
	return (*(u8 *) b - *(u8 *) a);
}


/**
 * wpas_dbus_getter_bss_rates - Return available bit rates of a BSS
 * @message: Pointer to incoming dbus message
 * @bss: a pair of interface describing structure and bss's id
 * Returns: a dbus message containing sorted array of bit rates
 *
 * Getter for "Rates" property.
 */
DBusMessage * wpas_dbus_getter_bss_rates(DBusMessage *message,
					    struct bss_handler_args *bss)
{
	DBusMessage *reply;
	struct wpa_bss *res = wpa_bss_get_id(bss->wpa_s, bss->id);
	u8 *ie_rates = NULL;
	u32 *real_rates;
	int rates_num, i;

	if (!res) {
		wpa_printf(MSG_ERROR, "wpas_dbus_getter_bss_rates[dbus]: "
			   "no bss with id %d found", bss->id);
		return NULL;
	}

	rates_num = wpa_bss_get_bit_rates(res, &ie_rates);
	if (rates_num < 0)
		return NULL;

	qsort(ie_rates, rates_num, 1, cmp_u8s_desc);

	real_rates = os_malloc(sizeof(u32) * rates_num);
	if (!real_rates) {
		os_free(ie_rates);
		return dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					      NULL);
	}

	for (i = 0; i < rates_num; i++)
		real_rates[i] = ie_rates[i] * 500000;

	reply = wpas_dbus_simple_array_property_getter(message,
						       DBUS_TYPE_UINT32,
						       real_rates, rates_num);

	os_free(ie_rates);
	os_free(real_rates);
	return reply;
}


static DBusMessage * wpas_dbus_get_bss_security_prop(
	DBusMessage *message, struct wpa_ie_data *ie_data)
{
	DBusMessage *reply;
	DBusMessageIter iter, iter_dict, variant_iter;
	const char *group;
	const char *pairwise[2]; /* max 2 pairwise ciphers is supported */
	const char *key_mgmt[7]; /* max 7 key managements may be supported */
	int n;

	if (message == NULL)
		reply = dbus_message_new(DBUS_MESSAGE_TYPE_SIGNAL);
	else
		reply = dbus_message_new_method_return(message);
	if (!reply)
		goto nomem;

	dbus_message_iter_init_append(reply, &iter);
	if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT,
					      "a{sv}", &variant_iter))
		goto nomem;

	if (!wpa_dbus_dict_open_write(&variant_iter, &iter_dict))
		goto nomem;

	/* KeyMgmt */
	n = 0;
	if (ie_data->key_mgmt & WPA_KEY_MGMT_PSK)
		key_mgmt[n++] = "wpa-psk";
	if (ie_data->key_mgmt & WPA_KEY_MGMT_FT_PSK)
		key_mgmt[n++] = "wpa-ft-psk";
	if (ie_data->key_mgmt & WPA_KEY_MGMT_PSK_SHA256)
		key_mgmt[n++] = "wpa-psk-sha256";
	if (ie_data->key_mgmt & WPA_KEY_MGMT_IEEE8021X)
		key_mgmt[n++] = "wpa-eap";
	if (ie_data->key_mgmt & WPA_KEY_MGMT_FT_IEEE8021X)
		key_mgmt[n++] = "wpa-ft-eap";
	if (ie_data->key_mgmt & WPA_KEY_MGMT_IEEE8021X_SHA256)
		key_mgmt[n++] = "wpa-eap-sha256";
	if (ie_data->key_mgmt & WPA_KEY_MGMT_NONE)
		key_mgmt[n++] = "wpa-none";

	if (!wpa_dbus_dict_append_string_array(&iter_dict, "KeyMgmt",
					       key_mgmt, n))
		goto nomem;

	/* Group */
	switch (ie_data->group_cipher) {
	case WPA_CIPHER_WEP40:
		group = "wep40";
		break;
	case WPA_CIPHER_TKIP:
		group = "tkip";
		break;
	case WPA_CIPHER_CCMP:
		group = "ccmp";
		break;
	case WPA_CIPHER_WEP104:
		group = "wep104";
		break;
	default:
		group = "";
		break;
	}

	if (!wpa_dbus_dict_append_string(&iter_dict, "Group", group))
		goto nomem;

	/* Pairwise */
	n = 0;
	if (ie_data->pairwise_cipher & WPA_CIPHER_TKIP)
		pairwise[n++] = "tkip";
	if (ie_data->pairwise_cipher & WPA_CIPHER_CCMP)
		pairwise[n++] = "ccmp";

	if (!wpa_dbus_dict_append_string_array(&iter_dict, "Pairwise",
					       pairwise, n))
		goto nomem;

	/* Management group (RSN only) */
	if (ie_data->proto == WPA_PROTO_RSN) {
		switch (ie_data->mgmt_group_cipher) {
#ifdef CONFIG_IEEE80211W
		case WPA_CIPHER_AES_128_CMAC:
			group = "aes128cmac";
			break;
#endif /* CONFIG_IEEE80211W */
		default:
			group = "";
			break;
		}

		if (!wpa_dbus_dict_append_string(&iter_dict, "MgmtGroup",
						 group))
			goto nomem;
	}

	if (!wpa_dbus_dict_close_write(&variant_iter, &iter_dict))
		goto nomem;
	if (!dbus_message_iter_close_container(&iter, &variant_iter))
		goto nomem;

	return reply;

nomem:
	if (reply)
		dbus_message_unref(reply);

	return dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY, NULL);
}


/**
 * wpas_dbus_getter_bss_wpa - Return the WPA options of a BSS
 * @message: Pointer to incoming dbus message
 * @bss: a pair of interface describing structure and bss's id
 * Returns: a dbus message containing the WPA options of requested bss
 *
 * Getter for "WPA" property.
 */
DBusMessage * wpas_dbus_getter_bss_wpa(DBusMessage *message,
				       struct bss_handler_args *bss)
{
	struct wpa_bss *res = wpa_bss_get_id(bss->wpa_s, bss->id);
	struct wpa_ie_data wpa_data;
	const u8 *ie;

	if (!res) {
		wpa_printf(MSG_ERROR, "wpas_dbus_getter_bss_wpa[dbus]: no "
			   "bss with id %d found", bss->id);
		return NULL;
	}

	os_memset(&wpa_data, 0, sizeof(wpa_data));
	ie = wpa_bss_get_vendor_ie(res, WPA_IE_VENDOR_TYPE);
	if (ie) {
		if (wpa_parse_wpa_ie(ie, 2 + ie[1], &wpa_data) < 0)
			return wpas_dbus_error_unknown_error(message,
							     "invalid WPA IE");
	}

	return wpas_dbus_get_bss_security_prop(message, &wpa_data);
}


/**
 * wpas_dbus_getter_bss_rsn - Return the RSN options of a BSS
 * @message: Pointer to incoming dbus message
 * @bss: a pair of interface describing structure and bss's id
 * Returns: a dbus message containing the RSN options of requested bss
 *
 * Getter for "RSN" property.
 */
DBusMessage * wpas_dbus_getter_bss_rsn(DBusMessage *message,
				       struct bss_handler_args *bss)
{
	struct wpa_bss *res = wpa_bss_get_id(bss->wpa_s, bss->id);
	struct wpa_ie_data wpa_data;
	const u8 *ie;

	if (!res) {
		wpa_printf(MSG_ERROR, "wpas_dbus_getter_bss_rsn[dbus]: no "
			   "bss with id %d found", bss->id);
		return NULL;
	}

	os_memset(&wpa_data, 0, sizeof(wpa_data));
	ie = wpa_bss_get_ie(res, WLAN_EID_RSN);
	if (ie) {
		if (wpa_parse_wpa_ie(ie, 2 + ie[1], &wpa_data) < 0)
			return wpas_dbus_error_unknown_error(message,
							     "invalid RSN IE");
	}

	return wpas_dbus_get_bss_security_prop(message, &wpa_data);
}


/**
 * wpas_dbus_getter_bss_ies - Return all IEs of a BSS
 * @message: Pointer to incoming dbus message
 * @bss: a pair of interface describing structure and bss's id
 * Returns: a dbus message containing IEs byte array
 *
 * Getter for "IEs" property.
 */
DBusMessage * wpas_dbus_getter_bss_ies(DBusMessage *message,
				       struct bss_handler_args *bss)
{
	struct wpa_bss *res = wpa_bss_get_id(bss->wpa_s, bss->id);

	if (!res) {
		wpa_printf(MSG_ERROR, "wpas_dbus_getter_bss_ies[dbus]: no "
			   "bss with id %d found", bss->id);
		return NULL;
	}

	return wpas_dbus_simple_array_property_getter(message, DBUS_TYPE_BYTE,
						      res + 1, res->ie_len);
}


/**
 * wpas_dbus_getter_enabled - Check whether network is enabled or disabled
 * @message: Pointer to incoming dbus message
 * @wpas_dbus_setter_enabled: wpa_supplicant structure for a network interface
 * and wpa_ssid structure for a configured network
 * Returns: DBus message with boolean indicating state of configured network
 * or DBus error on failure
 *
 * Getter for "enabled" property of a configured network.
 */
DBusMessage * wpas_dbus_getter_enabled(DBusMessage *message,
				       struct network_handler_args *net)
{
	dbus_bool_t enabled = net->ssid->disabled ? FALSE : TRUE;
	return wpas_dbus_simple_property_getter(message, DBUS_TYPE_BOOLEAN,
						&enabled);
}


/**
 * wpas_dbus_setter_enabled - Mark a configured network as enabled or disabled
 * @message: Pointer to incoming dbus message
 * @wpas_dbus_setter_enabled: wpa_supplicant structure for a network interface
 * and wpa_ssid structure for a configured network
 * Returns: NULL indicating success or DBus error on failure
 *
 * Setter for "Enabled" property of a configured network.
 */
DBusMessage * wpas_dbus_setter_enabled(DBusMessage *message,
				       struct network_handler_args *net)
{
	DBusMessage *reply = NULL;

	struct wpa_supplicant *wpa_s;
	struct wpa_ssid *ssid;

	dbus_bool_t enable;

	reply = wpas_dbus_simple_property_setter(message, DBUS_TYPE_BOOLEAN,
						 &enable);

	if (reply)
		return reply;

	wpa_s = net->wpa_s;
	ssid = net->ssid;

	if (enable)
		wpa_supplicant_enable_network(wpa_s, ssid);
	else
		wpa_supplicant_disable_network(wpa_s, ssid);

	return NULL;
}


/**
 * wpas_dbus_getter_network_properties - Get options for a configured network
 * @message: Pointer to incoming dbus message
 * @net: wpa_supplicant structure for a network interface and
 * wpa_ssid structure for a configured network
 * Returns: DBus message with network properties or DBus error on failure
 *
 * Getter for "Properties" property of a configured network.
 */
DBusMessage * wpas_dbus_getter_network_properties(
	DBusMessage *message, struct network_handler_args *net)
{
	DBusMessage *reply = NULL;
	DBusMessageIter	iter, variant_iter, dict_iter;
	char **iterator;
	char **props = wpa_config_get_all(net->ssid, 0);
	if (!props)
		return dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					      NULL);

	if (message == NULL)
		reply = dbus_message_new(DBUS_MESSAGE_TYPE_SIGNAL);
	else
		reply = dbus_message_new_method_return(message);
	if (!reply) {
		reply = dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					       NULL);
		goto out;
	}

	dbus_message_iter_init_append(reply, &iter);

	if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT,
			"a{sv}", &variant_iter) ||
	    !wpa_dbus_dict_open_write(&variant_iter, &dict_iter)) {
		dbus_message_unref(reply);
		reply = dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					       NULL);
		goto out;
	}

	iterator = props;
	while (*iterator) {
		if (!wpa_dbus_dict_append_string(&dict_iter, *iterator,
						 *(iterator + 1))) {
			dbus_message_unref(reply);
			reply = dbus_message_new_error(message,
						       DBUS_ERROR_NO_MEMORY,
						       NULL);
			goto out;
		}
		iterator += 2;
	}


	if (!wpa_dbus_dict_close_write(&variant_iter, &dict_iter) ||
	    !dbus_message_iter_close_container(&iter, &variant_iter)) {
		dbus_message_unref(reply);
		reply = dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY,
					       NULL);
		goto out;
	}

out:
	iterator = props;
	while (*iterator) {
		os_free(*iterator);
		iterator++;
	}
	os_free(props);
	return reply;
}


/**
 * wpas_dbus_setter_network_properties - Set options for a configured network
 * @message: Pointer to incoming dbus message
 * @net: wpa_supplicant structure for a network interface and
 * wpa_ssid structure for a configured network
 * Returns: NULL indicating success or DBus error on failure
 *
 * Setter for "Properties" property of a configured network.
 */
DBusMessage * wpas_dbus_setter_network_properties(
	DBusMessage *message, struct network_handler_args *net)
{
	struct wpa_ssid *ssid = net->ssid;

	DBusMessage *reply = NULL;
	DBusMessageIter	iter, variant_iter;

	dbus_message_iter_init(message, &iter);

	dbus_message_iter_next(&iter);
	dbus_message_iter_next(&iter);

	dbus_message_iter_recurse(&iter, &variant_iter);

	reply = set_network_properties(message, net->wpa_s, ssid,
				       &variant_iter);
	if (reply)
		wpa_printf(MSG_DEBUG, "dbus control interface couldn't set "
			   "network properties");

	return reply;
}
