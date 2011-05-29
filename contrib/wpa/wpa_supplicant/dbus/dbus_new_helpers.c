/*
 * WPA Supplicant / dbus-based control interface
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

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "dbus_common.h"
#include "dbus_common_i.h"
#include "dbus_new.h"
#include "dbus_new_helpers.h"


/**
 * recursive_iter_copy - Reads arguments from one iterator and
 * writes to another recursively
 * @from: iterator to read from
 * @to: iterator to write to
 *
 * Copies one iterator's elements to another. If any element in
 * iterator is of container type, its content is copied recursively
 */
static void recursive_iter_copy(DBusMessageIter *from, DBusMessageIter *to)
{

	char *subtype = NULL;
	int type;

	/* iterate over iterator to copy */
	while ((type = dbus_message_iter_get_arg_type(from)) !=
	       DBUS_TYPE_INVALID) {

		/* simply copy basic type entries */
		if (dbus_type_is_basic(type)) {
			if (dbus_type_is_fixed(type)) {
				/*
				 * According to DBus documentation all
				 * fixed-length types are guaranteed to fit
				 * 8 bytes
				 */
				dbus_uint64_t v;
				dbus_message_iter_get_basic(from, &v);
				dbus_message_iter_append_basic(to, type, &v);
			} else {
				char *v;
				dbus_message_iter_get_basic(from, &v);
				dbus_message_iter_append_basic(to, type, &v);
			}
		} else {
			/* recursively copy container type entries */
			DBusMessageIter write_subiter, read_subiter;

			dbus_message_iter_recurse(from, &read_subiter);

			if (type == DBUS_TYPE_VARIANT ||
			    type == DBUS_TYPE_ARRAY) {
				subtype = dbus_message_iter_get_signature(
					&read_subiter);
			}

			dbus_message_iter_open_container(to, type, subtype,
							 &write_subiter);

			recursive_iter_copy(&read_subiter, &write_subiter);

			dbus_message_iter_close_container(to, &write_subiter);
			if (subtype)
				dbus_free(subtype);
		}

		dbus_message_iter_next(from);
	}
}


static unsigned int fill_dict_with_properties(
	DBusMessageIter *dict_iter, const struct wpa_dbus_property_desc *props,
	const char *interface, const void *user_data)
{
	DBusMessage *reply;
	DBusMessageIter entry_iter, ret_iter;
	unsigned int counter = 0;
	const struct wpa_dbus_property_desc *dsc;

	for (dsc = props; dsc && dsc->dbus_property; dsc++) {
		if (!os_strncmp(dsc->dbus_interface, interface,
				WPAS_DBUS_INTERFACE_MAX) &&
		    dsc->access != W && dsc->getter) {
			reply = dsc->getter(NULL, user_data);
			if (!reply)
				continue;

			if (dbus_message_get_type(reply) ==
			    DBUS_MESSAGE_TYPE_ERROR) {
				dbus_message_unref(reply);
				continue;
			}

			dbus_message_iter_init(reply, &ret_iter);

			dbus_message_iter_open_container(dict_iter,
							 DBUS_TYPE_DICT_ENTRY,
							 NULL, &entry_iter);
			dbus_message_iter_append_basic(
				&entry_iter, DBUS_TYPE_STRING,
				&dsc->dbus_property);

			recursive_iter_copy(&ret_iter, &entry_iter);

			dbus_message_iter_close_container(dict_iter,
							  &entry_iter);
			dbus_message_unref(reply);
			counter++;
		}
	}

	return counter;
}


/**
 * get_all_properties - Responds for GetAll properties calls on object
 * @message: Message with GetAll call
 * @interface: interface name which properties will be returned
 * @property_dsc: list of object's properties
 * Returns: Message with dict of variants as argument with properties values
 *
 * Iterates over all properties registered with object and execute getters
 * of those, which are readable and which interface matches interface
 * specified as argument. Returned message contains one dict argument
 * with properties names as keys and theirs values as values.
 */
static DBusMessage * get_all_properties(
	DBusMessage *message, char *interface,
	struct wpa_dbus_object_desc *obj_dsc)
{
	/* Create and initialize the return message */
	DBusMessage *reply = dbus_message_new_method_return(message);
	DBusMessageIter iter, dict_iter;
	int props_num;

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					 DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
					 DBUS_TYPE_STRING_AS_STRING
					 DBUS_TYPE_VARIANT_AS_STRING
					 DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
					 &dict_iter);

	props_num = fill_dict_with_properties(&dict_iter, obj_dsc->properties,
					      interface, obj_dsc->user_data);

	dbus_message_iter_close_container(&iter, &dict_iter);

	if (props_num == 0) {
		dbus_message_unref(reply);
		reply = dbus_message_new_error(message,
					       DBUS_ERROR_INVALID_ARGS,
					       "No readable properties in "
					       "this interface");
	}

	return reply;
}


static int is_signature_correct(DBusMessage *message,
				const struct wpa_dbus_method_desc *method_dsc)
{
	/* According to DBus documentation max length of signature is 255 */
#define MAX_SIG_LEN 256
	char registered_sig[MAX_SIG_LEN], *pos;
	const char *sig = dbus_message_get_signature(message);
	int ret;
	const struct wpa_dbus_argument *arg;

	pos = registered_sig;
	*pos = '\0';

	for (arg = method_dsc->args; arg && arg->name; arg++) {
		if (arg->dir == ARG_IN) {
			size_t blen = registered_sig + MAX_SIG_LEN - pos;
			ret = os_snprintf(pos, blen, "%s", arg->type);
			if (ret < 0 || (size_t) ret >= blen)
				return 0;
			pos += ret;
		}
	}

	return !os_strncmp(registered_sig, sig, MAX_SIG_LEN);
}


static DBusMessage * properties_get_all(DBusMessage *message, char *interface,
					struct wpa_dbus_object_desc *obj_dsc)
{
	if (os_strcmp(dbus_message_get_signature(message), "s") != 0)
		return dbus_message_new_error(message, DBUS_ERROR_INVALID_ARGS,
					      NULL);

	return get_all_properties(message, interface, obj_dsc);
}


static DBusMessage * properties_get(DBusMessage *message,
				    const struct wpa_dbus_property_desc *dsc,
				    void *user_data)
{
	if (os_strcmp(dbus_message_get_signature(message), "ss"))
		return dbus_message_new_error(message, DBUS_ERROR_INVALID_ARGS,
					      NULL);

	if (dsc->access != W && dsc->getter)
		return dsc->getter(message, user_data);

	return dbus_message_new_error(message, DBUS_ERROR_INVALID_ARGS,
				      "Property is write-only");
}


static DBusMessage * properties_set(DBusMessage *message,
				    const struct wpa_dbus_property_desc *dsc,
				    void *user_data)
{
	if (os_strcmp(dbus_message_get_signature(message), "ssv"))
		return dbus_message_new_error(message, DBUS_ERROR_INVALID_ARGS,
					      NULL);

	if (dsc->access != R && dsc->setter)
		return dsc->setter(message, user_data);

	return dbus_message_new_error(message, DBUS_ERROR_INVALID_ARGS,
				      "Property is read-only");
}


static DBusMessage *
properties_get_or_set(DBusMessage *message, DBusMessageIter *iter,
		      char *interface,
		      struct wpa_dbus_object_desc *obj_dsc)
{
	const struct wpa_dbus_property_desc *property_dsc;
	char *property;
	const char *method;

	method = dbus_message_get_member(message);
	property_dsc = obj_dsc->properties;

	/* Second argument: property name (DBUS_TYPE_STRING) */
	if (!dbus_message_iter_next(iter) ||
	    dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_STRING) {
		return dbus_message_new_error(message, DBUS_ERROR_INVALID_ARGS,
					      NULL);
	}
	dbus_message_iter_get_basic(iter, &property);

	while (property_dsc && property_dsc->dbus_property) {
		/* compare property names and
		 * interfaces */
		if (!os_strncmp(property_dsc->dbus_property, property,
				WPAS_DBUS_METHOD_SIGNAL_PROP_MAX) &&
		    !os_strncmp(property_dsc->dbus_interface, interface,
				WPAS_DBUS_INTERFACE_MAX))
			break;

		property_dsc++;
	}
	if (property_dsc == NULL || property_dsc->dbus_property == NULL) {
		wpa_printf(MSG_DEBUG, "no property handler for %s.%s on %s",
			   interface, property,
			   dbus_message_get_path(message));
		return dbus_message_new_error(message, DBUS_ERROR_INVALID_ARGS,
					      "No such property");
	}

	if (os_strncmp(WPA_DBUS_PROPERTIES_GET, method,
		       WPAS_DBUS_METHOD_SIGNAL_PROP_MAX) == 0)
		return properties_get(message, property_dsc,
				      obj_dsc->user_data);

	return properties_set(message, property_dsc, obj_dsc->user_data);
}


static DBusMessage * properties_handler(DBusMessage *message,
					struct wpa_dbus_object_desc *obj_dsc)
{
	DBusMessageIter iter;
	char *interface;
	const char *method;

	method = dbus_message_get_member(message);
	dbus_message_iter_init(message, &iter);

	if (!os_strncmp(WPA_DBUS_PROPERTIES_GET, method,
			WPAS_DBUS_METHOD_SIGNAL_PROP_MAX) ||
	    !os_strncmp(WPA_DBUS_PROPERTIES_SET, method,
			WPAS_DBUS_METHOD_SIGNAL_PROP_MAX) ||
	    !os_strncmp(WPA_DBUS_PROPERTIES_GETALL, method,
			WPAS_DBUS_METHOD_SIGNAL_PROP_MAX)) {
		/* First argument: interface name (DBUS_TYPE_STRING) */
		if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
		{
			return dbus_message_new_error(message,
						      DBUS_ERROR_INVALID_ARGS,
						      NULL);
		}

		dbus_message_iter_get_basic(&iter, &interface);

		if (!os_strncmp(WPA_DBUS_PROPERTIES_GETALL, method,
				WPAS_DBUS_METHOD_SIGNAL_PROP_MAX)) {
			/* GetAll */
			return properties_get_all(message, interface, obj_dsc);
		}
		/* Get or Set */
		return properties_get_or_set(message, &iter, interface,
					     obj_dsc);
	}
	return dbus_message_new_error(message, DBUS_ERROR_UNKNOWN_METHOD,
				      NULL);
}


static DBusMessage * msg_method_handler(DBusMessage *message,
					struct wpa_dbus_object_desc *obj_dsc)
{
	const struct wpa_dbus_method_desc *method_dsc = obj_dsc->methods;
	const char *method;
	const char *msg_interface;

	method = dbus_message_get_member(message);
	msg_interface = dbus_message_get_interface(message);

	/* try match call to any registered method */
	while (method_dsc && method_dsc->dbus_method) {
		/* compare method names and interfaces */
		if (!os_strncmp(method_dsc->dbus_method, method,
				WPAS_DBUS_METHOD_SIGNAL_PROP_MAX) &&
		    !os_strncmp(method_dsc->dbus_interface, msg_interface,
				WPAS_DBUS_INTERFACE_MAX))
			break;

		method_dsc++;
	}
	if (method_dsc == NULL || method_dsc->dbus_method == NULL) {
		wpa_printf(MSG_DEBUG, "no method handler for %s.%s on %s",
			   msg_interface, method,
			   dbus_message_get_path(message));
		return dbus_message_new_error(message,
					      DBUS_ERROR_UNKNOWN_METHOD, NULL);
	}

	if (!is_signature_correct(message, method_dsc)) {
		return dbus_message_new_error(message, DBUS_ERROR_INVALID_ARGS,
					      NULL);
	}

	return method_dsc->method_handler(message,
					  obj_dsc->user_data);
}


/**
 * message_handler - Handles incoming DBus messages
 * @connection: DBus connection on which message was received
 * @message: Received message
 * @user_data: pointer to description of object to which message was sent
 * Returns: Returns information whether message was handled or not
 *
 * Reads message interface and method name, then checks if they matches one
 * of the special cases i.e. introspection call or properties get/getall/set
 * methods and handles it. Else it iterates over registered methods list
 * and tries to match method's name and interface to those read from message
 * If appropriate method was found its handler function is called and
 * response is sent. Otherwise, the DBUS_ERROR_UNKNOWN_METHOD error message
 * will be sent.
 */
static DBusHandlerResult message_handler(DBusConnection *connection,
					 DBusMessage *message, void *user_data)
{
	struct wpa_dbus_object_desc *obj_dsc = user_data;
	const char *method;
	const char *path;
	const char *msg_interface;
	DBusMessage *reply;

	/* get method, interface and path the message is addressed to */
	method = dbus_message_get_member(message);
	path = dbus_message_get_path(message);
	msg_interface = dbus_message_get_interface(message);
	if (!method || !path || !msg_interface)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	wpa_printf(MSG_MSGDUMP, "dbus: %s.%s (%s)",
		   msg_interface, method, path);

	/* if message is introspection method call */
	if (!os_strncmp(WPA_DBUS_INTROSPECTION_METHOD, method,
			WPAS_DBUS_METHOD_SIGNAL_PROP_MAX) &&
	    !os_strncmp(WPA_DBUS_INTROSPECTION_INTERFACE, msg_interface,
			WPAS_DBUS_INTERFACE_MAX)) {
#ifdef CONFIG_CTRL_IFACE_DBUS_INTRO
		reply = wpa_dbus_introspect(message, obj_dsc);
#else /* CONFIG_CTRL_IFACE_DBUS_INTRO */
		reply = dbus_message_new_error(
			message, DBUS_ERROR_UNKNOWN_METHOD,
			"wpa_supplicant was compiled without "
			"introspection support.");
#endif /* CONFIG_CTRL_IFACE_DBUS_INTRO */
	} else if (!os_strncmp(WPA_DBUS_PROPERTIES_INTERFACE, msg_interface,
			     WPAS_DBUS_INTERFACE_MAX)) {
		/* if message is properties method call */
		reply = properties_handler(message, obj_dsc);
	} else {
		reply = msg_method_handler(message, obj_dsc);
	}

	/* If handler succeed returning NULL, reply empty message */
	if (!reply)
		reply = dbus_message_new_method_return(message);
	if (reply) {
		if (!dbus_message_get_no_reply(message))
			dbus_connection_send(connection, reply, NULL);
		dbus_message_unref(reply);
	}

	wpa_dbus_flush_all_changed_properties(connection);

	return DBUS_HANDLER_RESULT_HANDLED;
}


/**
 * free_dbus_object_desc - Frees object description data structure
 * @connection: DBus connection
 * @obj_dsc: Object description to free
 *
 * Frees each of properties, methods and signals description lists and
 * the object description structure itself.
 */
void free_dbus_object_desc(struct wpa_dbus_object_desc *obj_dsc)
{
	if (!obj_dsc)
		return;

	/* free handler's argument */
	if (obj_dsc->user_data_free_func)
		obj_dsc->user_data_free_func(obj_dsc->user_data);

	os_free(obj_dsc->path);
	os_free(obj_dsc->prop_changed_flags);
	os_free(obj_dsc);
}


static void free_dbus_object_desc_cb(DBusConnection *connection, void *obj_dsc)
{
	free_dbus_object_desc(obj_dsc);
}

/**
 * wpa_dbus_ctrl_iface_init - Initialize dbus control interface
 * @application_data: Pointer to application specific data structure
 * @dbus_path: DBus path to interface object
 * @dbus_service: DBus service name to register with
 * @messageHandler: a pointer to function which will handle dbus messages
 * coming on interface
 * Returns: 0 on success, -1 on failure
 *
 * Initialize the dbus control interface and start receiving commands from
 * external programs over the bus.
 */
int wpa_dbus_ctrl_iface_init(struct wpas_dbus_priv *iface,
			     char *dbus_path, char *dbus_service,
			     struct wpa_dbus_object_desc *obj_desc)
{
	DBusError error;
	int ret = -1;
	DBusObjectPathVTable wpa_vtable = {
		&free_dbus_object_desc_cb, &message_handler,
		NULL, NULL, NULL, NULL
	};

	obj_desc->connection = iface->con;
	obj_desc->path = os_strdup(dbus_path);

	/* Register the message handler for the global dbus interface */
	if (!dbus_connection_register_object_path(iface->con,
						  dbus_path, &wpa_vtable,
						  obj_desc)) {
		wpa_printf(MSG_ERROR, "dbus: Could not set up message "
			   "handler");
		return -1;
	}

	/* Register our service with the message bus */
	dbus_error_init(&error);
	switch (dbus_bus_request_name(iface->con, dbus_service,
				      0, &error)) {
	case DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER:
		ret = 0;
		break;
	case DBUS_REQUEST_NAME_REPLY_EXISTS:
	case DBUS_REQUEST_NAME_REPLY_IN_QUEUE:
	case DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER:
		wpa_printf(MSG_ERROR, "dbus: Could not request service name: "
			   "already registered");
		break;
	default:
		wpa_printf(MSG_ERROR, "dbus: Could not request service name: "
			   "%s %s", error.name, error.message);
		break;
	}
	dbus_error_free(&error);

	if (ret != 0)
		return -1;

	wpa_printf(MSG_DEBUG, "Providing DBus service '%s'.", dbus_service);

	return 0;
}


/**
 * wpa_dbus_register_object_per_iface - Register a new object with dbus
 * @ctrl_iface: pointer to dbus private data
 * @path: DBus path to object
 * @ifname: interface name
 * @obj_desc: description of object's methods, signals and properties
 * Returns: 0 on success, -1 on error
 *
 * Registers a new interface with dbus and assigns it a dbus object path.
 */
int wpa_dbus_register_object_per_iface(
	struct wpas_dbus_priv *ctrl_iface,
	const char *path, const char *ifname,
	struct wpa_dbus_object_desc *obj_desc)
{
	DBusConnection *con;

	DBusObjectPathVTable vtable = {
		&free_dbus_object_desc_cb, &message_handler,
		NULL, NULL, NULL, NULL
	};

	/* Do nothing if the control interface is not turned on */
	if (ctrl_iface == NULL)
		return 0;

	con = ctrl_iface->con;
	obj_desc->connection = con;
	obj_desc->path = os_strdup(path);

	/* Register the message handler for the interface functions */
	if (!dbus_connection_register_object_path(con, path, &vtable,
						  obj_desc)) {
		wpa_printf(MSG_ERROR, "dbus: Could not set up message "
			   "handler for interface %s object %s", ifname, path);
		return -1;
	}

	return 0;
}


static void flush_object_timeout_handler(void *eloop_ctx, void *timeout_ctx);


/**
 * wpa_dbus_unregister_object_per_iface - Unregisters DBus object
 * @ctrl_iface: Pointer to dbus private data
 * @path: DBus path to object which will be unregistered
 * Returns: Zero on success and -1 on failure
 *
 * Unregisters DBus object given by its path
 */
int wpa_dbus_unregister_object_per_iface(
	struct wpas_dbus_priv *ctrl_iface, const char *path)
{
	DBusConnection *con = ctrl_iface->con;
	struct wpa_dbus_object_desc *obj_desc = NULL;

	dbus_connection_get_object_path_data(con, path, (void **) &obj_desc);
	if (!obj_desc) {
		wpa_printf(MSG_ERROR, "dbus: %s: Could not obtain object's "
			   "private data: %s", __func__, path);
	} else {
		eloop_cancel_timeout(flush_object_timeout_handler, con,
				     obj_desc);
	}

	if (!dbus_connection_unregister_object_path(con, path))
		return -1;

	return 0;
}


static void put_changed_properties(const struct wpa_dbus_object_desc *obj_dsc,
				   const char *interface,
				   DBusMessageIter *dict_iter)
{
	DBusMessage *getter_reply;
	DBusMessageIter prop_iter, entry_iter;
	const struct wpa_dbus_property_desc *dsc;
	int i;

	for (dsc = obj_dsc->properties, i = 0; dsc && dsc->dbus_property;
	     dsc++, i++) {
		if (obj_dsc->prop_changed_flags == NULL ||
		    !obj_dsc->prop_changed_flags[i])
			continue;
		if (os_strcmp(dsc->dbus_interface, interface) != 0)
			continue;
		obj_dsc->prop_changed_flags[i] = 0;

		getter_reply = dsc->getter(NULL, obj_dsc->user_data);
		if (!getter_reply ||
		    dbus_message_get_type(getter_reply) ==
		    DBUS_MESSAGE_TYPE_ERROR) {
			wpa_printf(MSG_ERROR, "dbus: %s: Cannot get new value "
				   "of property %s", __func__,
				   dsc->dbus_property);
			continue;
		}

		if (!dbus_message_iter_init(getter_reply, &prop_iter) ||
		    !dbus_message_iter_open_container(dict_iter,
						      DBUS_TYPE_DICT_ENTRY,
						      NULL, &entry_iter) ||
		    !dbus_message_iter_append_basic(&entry_iter,
						    DBUS_TYPE_STRING,
						    &dsc->dbus_property))
			goto err;

		recursive_iter_copy(&prop_iter, &entry_iter);

		if (!dbus_message_iter_close_container(dict_iter, &entry_iter))
			goto err;

		dbus_message_unref(getter_reply);
	}

	return;

err:
	wpa_printf(MSG_ERROR, "dbus: %s: Cannot construct signal", __func__);
}


static void send_prop_changed_signal(
	DBusConnection *con, const char *path, const char *interface,
	const struct wpa_dbus_object_desc *obj_dsc)
{
	DBusMessage *msg;
	DBusMessageIter signal_iter, dict_iter;

	msg = dbus_message_new_signal(path, interface, "PropertiesChanged");
	if (msg == NULL)
		return;

	dbus_message_iter_init_append(msg, &signal_iter);

	if (!dbus_message_iter_open_container(&signal_iter, DBUS_TYPE_ARRAY,
					      "{sv}", &dict_iter))
		goto err;

	put_changed_properties(obj_dsc, interface, &dict_iter);

	if (!dbus_message_iter_close_container(&signal_iter, &dict_iter))
		goto err;

	dbus_connection_send(con, msg, NULL);

out:
	dbus_message_unref(msg);
	return;

err:
	wpa_printf(MSG_DEBUG, "dbus: %s: Failed to construct signal",
		   __func__);
	goto out;
}


static void flush_object_timeout_handler(void *eloop_ctx, void *timeout_ctx)
{
	DBusConnection *con = eloop_ctx;
	struct wpa_dbus_object_desc *obj_desc = timeout_ctx;

	wpa_printf(MSG_DEBUG, "dbus: %s: Timeout - sending changed properties "
		   "of object %s", __func__, obj_desc->path);
	wpa_dbus_flush_object_changed_properties(con, obj_desc->path);
}


static void recursive_flush_changed_properties(DBusConnection *con,
					       const char *path)
{
	char **objects = NULL;
	char subobj_path[WPAS_DBUS_OBJECT_PATH_MAX];
	int i;

	wpa_dbus_flush_object_changed_properties(con, path);

	if (!dbus_connection_list_registered(con, path, &objects))
		goto out;

	for (i = 0; objects[i]; i++) {
		os_snprintf(subobj_path, WPAS_DBUS_OBJECT_PATH_MAX,
			    "%s/%s", path, objects[i]);
		recursive_flush_changed_properties(con, subobj_path);
	}

out:
	dbus_free_string_array(objects);
}


/**
 * wpa_dbus_flush_all_changed_properties - Send all PropertiesChanged signals
 * @con: DBus connection
 *
 * Traverses through all registered objects and sends PropertiesChanged for
 * each properties.
 */
void wpa_dbus_flush_all_changed_properties(DBusConnection *con)
{
	recursive_flush_changed_properties(con, WPAS_DBUS_NEW_PATH);
}


/**
 * wpa_dbus_flush_object_changed_properties - Send PropertiesChanged for object
 * @con: DBus connection
 * @path: path to a DBus object for which PropertiesChanged will be sent.
 *
 * Iterates over all properties registered with object and for each interface
 * containing properties marked as changed, sends a PropertiesChanged signal
 * containing names and new values of properties that have changed.
 *
 * You need to call this function after wpa_dbus_mark_property_changed()
 * if you want to send PropertiesChanged signal immediately (i.e., without
 * waiting timeout to expire). PropertiesChanged signal for an object is sent
 * automatically short time after first marking property as changed. All
 * PropertiesChanged signals are sent automatically after responding on DBus
 * message, so if you marked a property changed as a result of DBus call
 * (e.g., param setter), you usually do not need to call this function.
 */
void wpa_dbus_flush_object_changed_properties(DBusConnection *con,
					      const char *path)
{
	struct wpa_dbus_object_desc *obj_desc = NULL;
	const struct wpa_dbus_property_desc *dsc;
	int i;

	dbus_connection_get_object_path_data(con, path, (void **) &obj_desc);
	if (!obj_desc)
		return;
	eloop_cancel_timeout(flush_object_timeout_handler, con, obj_desc);

	dsc = obj_desc->properties;
	for (dsc = obj_desc->properties, i = 0; dsc && dsc->dbus_property;
	     dsc++, i++) {
		if (obj_desc->prop_changed_flags == NULL ||
		    !obj_desc->prop_changed_flags[i])
			continue;
		send_prop_changed_signal(con, path, dsc->dbus_interface,
					 obj_desc);
	}
}


#define WPA_DBUS_SEND_PROP_CHANGED_TIMEOUT 5000


/**
 * wpa_dbus_mark_property_changed - Mark a property as changed and
 * @iface: dbus priv struct
 * @path: path to DBus object which property has changed
 * @interface: interface containing changed property
 * @property: property name which has changed
 *
 * Iterates over all properties registered with an object and marks the one
 * given in parameters as changed. All parameters registered for an object
 * within a single interface will be aggregated together and sent in one
 * PropertiesChanged signal when function
 * wpa_dbus_flush_object_changed_properties() is called.
 */
void wpa_dbus_mark_property_changed(struct wpas_dbus_priv *iface,
				    const char *path, const char *interface,
				    const char *property)
{
	struct wpa_dbus_object_desc *obj_desc = NULL;
	const struct wpa_dbus_property_desc *dsc;
	int i = 0;

	if (iface == NULL)
		return;

	dbus_connection_get_object_path_data(iface->con, path,
					     (void **) &obj_desc);
	if (!obj_desc) {
		wpa_printf(MSG_ERROR, "dbus: wpa_dbus_property_changed: "
			   "could not obtain object's private data: %s", path);
		return;
	}

	for (dsc = obj_desc->properties; dsc && dsc->dbus_property; dsc++, i++)
		if (os_strcmp(property, dsc->dbus_property) == 0 &&
		    os_strcmp(interface, dsc->dbus_interface) == 0) {
			if (obj_desc->prop_changed_flags)
				obj_desc->prop_changed_flags[i] = 1;
			break;
		}

	if (!dsc || !dsc->dbus_property) {
		wpa_printf(MSG_ERROR, "dbus: wpa_dbus_property_changed: "
			   "no property %s in object %s", property, path);
		return;
	}

	if (!eloop_is_timeout_registered(flush_object_timeout_handler,
					 iface->con, obj_desc->path)) {
		eloop_register_timeout(0, WPA_DBUS_SEND_PROP_CHANGED_TIMEOUT,
				       flush_object_timeout_handler,
				       iface->con, obj_desc);
	}
}


/**
 * wpa_dbus_get_object_properties - Put object's properties into dictionary
 * @iface: dbus priv struct
 * @path: path to DBus object which properties will be obtained
 * @interface: interface name which properties will be obtained
 * @dict_iter: correct, open DBus dictionary iterator.
 *
 * Iterates over all properties registered with object and execute getters
 * of those, which are readable and which interface matches interface
 * specified as argument. Obtained properties values are stored in
 * dict_iter dictionary.
 */
void wpa_dbus_get_object_properties(struct wpas_dbus_priv *iface,
				    const char *path, const char *interface,
				    DBusMessageIter *dict_iter)
{
	struct wpa_dbus_object_desc *obj_desc = NULL;

	dbus_connection_get_object_path_data(iface->con, path,
					     (void **) &obj_desc);
	if (!obj_desc) {
		wpa_printf(MSG_ERROR, "dbus: wpa_dbus_get_object_properties: "
			   "could not obtain object's private data: %s", path);
		return;
	}

	fill_dict_with_properties(dict_iter, obj_desc->properties,
				  interface, obj_desc->user_data);
}
