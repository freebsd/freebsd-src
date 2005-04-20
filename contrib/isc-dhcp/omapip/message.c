/* message.c

   Subroutines for dealing with message objects. */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1999-2003 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   http://www.isc.org/
 *
 * This software has been written for Internet Systems Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about Internet Systems Consortium, see
 * ``http://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

#include <omapip/omapip_p.h>

OMAPI_OBJECT_ALLOC (omapi_message,
		    omapi_message_object_t, omapi_type_message)

omapi_message_object_t *omapi_registered_messages;

isc_result_t omapi_message_new (omapi_object_t **o, const char *file, int line)
{
	omapi_message_object_t *m;
	omapi_object_t *g;
	isc_result_t status;

	m = (omapi_message_object_t *)0;
	status = omapi_message_allocate (&m, file, line);
	if (status != ISC_R_SUCCESS)
		return status;

	g = (omapi_object_t *)0;
	status = omapi_generic_new (&g, file, line);
	if (status != ISC_R_SUCCESS) {
		dfree (m, file, line);
		return status;
	}
	status = omapi_object_reference (&m -> inner, g, file, line);
	if (status != ISC_R_SUCCESS) {
		omapi_object_dereference ((omapi_object_t **)&m, file, line);
		omapi_object_dereference (&g, file, line);
		return status;
	}
	status = omapi_object_reference (&g -> outer,
					 (omapi_object_t *)m, file, line);

	if (status != ISC_R_SUCCESS) {
		omapi_object_dereference ((omapi_object_t **)&m, file, line);
		omapi_object_dereference (&g, file, line);
		return status;
	}

	status = omapi_object_reference (o, (omapi_object_t *)m, file, line);
	omapi_message_dereference (&m, file, line);
	omapi_object_dereference (&g, file, line);
	if (status != ISC_R_SUCCESS)
		return status;

	return status;
}

isc_result_t omapi_message_set_value (omapi_object_t *h,
				      omapi_object_t *id,
				      omapi_data_string_t *name,
				      omapi_typed_data_t *value)
{
	omapi_message_object_t *m;
	isc_result_t status;

	if (h -> type != omapi_type_message)
		return ISC_R_INVALIDARG;
	m = (omapi_message_object_t *)h;

	/* Can't set authlen. */

	/* Can set authenticator, but the value must be typed data. */
	if (!omapi_ds_strcmp (name, "authenticator")) {
		if (m -> authenticator)
			omapi_typed_data_dereference (&m -> authenticator,
						      MDL);
		omapi_typed_data_reference (&m -> authenticator, value, MDL);
		return ISC_R_SUCCESS;

	} else if (!omapi_ds_strcmp (name, "object")) {
		if (value -> type != omapi_datatype_object)
			return ISC_R_INVALIDARG;
		if (m -> object)
			omapi_object_dereference (&m -> object, MDL);
		omapi_object_reference (&m -> object, value -> u.object, MDL);
		return ISC_R_SUCCESS;

	} else if (!omapi_ds_strcmp (name, "notify-object")) {
		if (value -> type != omapi_datatype_object)
			return ISC_R_INVALIDARG;
		if (m -> notify_object)
			omapi_object_dereference (&m -> notify_object, MDL);
		omapi_object_reference (&m -> notify_object,
					value -> u.object, MDL);
		return ISC_R_SUCCESS;

	/* Can set authid, but it has to be an integer. */
	} else if (!omapi_ds_strcmp (name, "authid")) {
		if (value -> type != omapi_datatype_int)
			return ISC_R_INVALIDARG;
		m -> authid = value -> u.integer;
		return ISC_R_SUCCESS;

	/* Can set op, but it has to be an integer. */
	} else if (!omapi_ds_strcmp (name, "op")) {
		if (value -> type != omapi_datatype_int)
			return ISC_R_INVALIDARG;
		m -> op = value -> u.integer;
		return ISC_R_SUCCESS;

	/* Handle also has to be an integer. */
	} else if (!omapi_ds_strcmp (name, "handle")) {
		if (value -> type != omapi_datatype_int)
			return ISC_R_INVALIDARG;
		m -> h = value -> u.integer;
		return ISC_R_SUCCESS;

	/* Transaction ID has to be an integer. */
	} else if (!omapi_ds_strcmp (name, "id")) {
		if (value -> type != omapi_datatype_int)
			return ISC_R_INVALIDARG;
		m -> id = value -> u.integer;
		return ISC_R_SUCCESS;

	/* Remote transaction ID has to be an integer. */
	} else if (!omapi_ds_strcmp (name, "rid")) {
		if (value -> type != omapi_datatype_int)
			return ISC_R_INVALIDARG;
		m -> rid = value -> u.integer;
		return ISC_R_SUCCESS;
	}

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> set_value) {
		status = ((*(h -> inner -> type -> set_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS)
			return status;
	}
			  
	return ISC_R_NOTFOUND;
}

isc_result_t omapi_message_get_value (omapi_object_t *h,
				      omapi_object_t *id,
				      omapi_data_string_t *name,
				      omapi_value_t **value)
{
	omapi_message_object_t *m;
	if (h -> type != omapi_type_message)
		return ISC_R_INVALIDARG;
	m = (omapi_message_object_t *)h;

	/* Look for values that are in the message data structure. */
	if (!omapi_ds_strcmp (name, "authlen"))
		return omapi_make_int_value (value, name, (int)m -> authlen,
					     MDL);
	else if (!omapi_ds_strcmp (name, "authenticator")) {
		if (m -> authenticator)
			return omapi_make_value (value, name,
						 m -> authenticator, MDL);
		else
			return ISC_R_NOTFOUND;
	} else if (!omapi_ds_strcmp (name, "authid")) {
		return omapi_make_int_value (value,
					     name, (int)m -> authid, MDL);
	} else if (!omapi_ds_strcmp (name, "op")) {
		return omapi_make_int_value (value, name, (int)m -> op, MDL);
	} else if (!omapi_ds_strcmp (name, "handle")) {
		return omapi_make_int_value (value, name, (int)m -> h, MDL);
	} else if (!omapi_ds_strcmp (name, "id")) {
		return omapi_make_int_value (value, name, (int)m -> id, MDL);
	} else if (!omapi_ds_strcmp (name, "rid")) {
		return omapi_make_int_value (value, name, (int)m -> rid, MDL);
	}

	/* See if there's an inner object that has the value. */
	if (h -> inner && h -> inner -> type -> get_value)
		return (*(h -> inner -> type -> get_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t omapi_message_destroy (omapi_object_t *h,
				    const char *file, int line)
{
	int i;

	omapi_message_object_t *m;
	if (h -> type != omapi_type_message)
		return ISC_R_INVALIDARG;
	m = (omapi_message_object_t *)h;
	if (m -> authenticator) {
		omapi_typed_data_dereference (&m -> authenticator, file, line);
	}
	if (!m -> prev && omapi_registered_messages != m)
		omapi_message_unregister (h);
	if (m -> id_object)
		omapi_object_dereference (&m -> id_object, file, line);
	if (m -> object)
		omapi_object_dereference (&m -> object, file, line);
	if (m -> notify_object)
		omapi_object_dereference (&m -> notify_object, file, line);
	if (m -> protocol_object)
		omapi_protocol_dereference (&m -> protocol_object, file, line);
	return ISC_R_SUCCESS;
}

isc_result_t omapi_message_signal_handler (omapi_object_t *h,
					   const char *name, va_list ap)
{
	omapi_message_object_t *m;
	if (h -> type != omapi_type_message)
		return ISC_R_INVALIDARG;
	m = (omapi_message_object_t *)h;
	
	if (!strcmp (name, "status")) {
		if (m -> notify_object &&
		    m -> notify_object -> type -> signal_handler)
			return ((m -> notify_object -> type -> signal_handler))
				(m -> notify_object, name, ap);
		else if (m -> object && m -> object -> type -> signal_handler)
			return ((m -> object -> type -> signal_handler))
				(m -> object, name, ap);
	}
	if (h -> inner && h -> inner -> type -> signal_handler)
		return (*(h -> inner -> type -> signal_handler)) (h -> inner,
								  name, ap);
	return ISC_R_NOTFOUND;
}

/* Write all the published values associated with the object through the
   specified connection. */

isc_result_t omapi_message_stuff_values (omapi_object_t *c,
					 omapi_object_t *id,
					 omapi_object_t *m)
{
	int i;

	if (m -> type != omapi_type_message)
		return ISC_R_INVALIDARG;

	if (m -> inner && m -> inner -> type -> stuff_values)
		return (*(m -> inner -> type -> stuff_values)) (c, id,
								m -> inner);
	return ISC_R_SUCCESS;
}

isc_result_t omapi_message_register (omapi_object_t *mo)
{
	omapi_message_object_t *m;

	if (mo -> type != omapi_type_message)
		return ISC_R_INVALIDARG;
	m = (omapi_message_object_t *)mo;
	
	/* Already registered? */
	if (m -> prev || m -> next || omapi_registered_messages == m)
		return ISC_R_INVALIDARG;

	if (omapi_registered_messages) {
		omapi_object_reference
			((omapi_object_t **)&m -> next,
			 (omapi_object_t *)omapi_registered_messages, MDL);
		omapi_object_reference
			((omapi_object_t **)&omapi_registered_messages -> prev,
			 (omapi_object_t *)m, MDL);
		omapi_object_dereference
			((omapi_object_t **)&omapi_registered_messages, MDL);
	}
	omapi_object_reference
		((omapi_object_t **)&omapi_registered_messages,
		 (omapi_object_t *)m, MDL);
	return ISC_R_SUCCESS;;
}

isc_result_t omapi_message_unregister (omapi_object_t *mo)
{
	omapi_message_object_t *m;
	omapi_message_object_t *n;

	if (mo -> type != omapi_type_message)
		return ISC_R_INVALIDARG;
	m = (omapi_message_object_t *)mo;
	
	/* Not registered? */
	if (!m -> prev && omapi_registered_messages != m)
		return ISC_R_INVALIDARG;

	n = (omapi_message_object_t *)0;
	if (m -> next) {
		omapi_object_reference ((omapi_object_t **)&n,
					(omapi_object_t *)m -> next, MDL);
		omapi_object_dereference ((omapi_object_t **)&m -> next, MDL);
		omapi_object_dereference ((omapi_object_t **)&n -> prev, MDL);
	}
	if (m -> prev) {
		omapi_message_object_t *tmp = (omapi_message_object_t *)0;
		omapi_object_reference ((omapi_object_t **)&tmp,
					(omapi_object_t *)m -> prev, MDL);
		omapi_object_dereference ((omapi_object_t **)&m -> prev, MDL);
		if (tmp -> next)
			omapi_object_dereference
				((omapi_object_t **)&tmp -> next, MDL);
		if (n)
			omapi_object_reference
				((omapi_object_t **)&tmp -> next,
				 (omapi_object_t *)n, MDL);
		omapi_object_dereference ((omapi_object_t **)&tmp, MDL);
	} else {
		omapi_object_dereference
			((omapi_object_t **)&omapi_registered_messages, MDL);
		if (n)
			omapi_object_reference
				((omapi_object_t **)&omapi_registered_messages,
				 (omapi_object_t *)n, MDL);
	}
	if (n)
		omapi_object_dereference ((omapi_object_t **)&n, MDL);
	return ISC_R_SUCCESS;
}

#ifdef DEBUG_PROTOCOL
static const char *omapi_message_op_name(int op) {
	switch (op) {
	case OMAPI_OP_OPEN:    return "OMAPI_OP_OPEN";
	case OMAPI_OP_REFRESH: return "OMAPI_OP_REFRESH";
	case OMAPI_OP_UPDATE:  return "OMAPI_OP_UPDATE";
	case OMAPI_OP_STATUS:  return "OMAPI_OP_STATUS";
	case OMAPI_OP_DELETE:  return "OMAPI_OP_DELETE";
	case OMAPI_OP_NOTIFY:  return "OMAPI_OP_NOTIFY";
	default:               return "(unknown op)";
	}
}
#endif

static isc_result_t
omapi_message_process_internal (omapi_object_t *, omapi_object_t *);

isc_result_t omapi_message_process (omapi_object_t *mo, omapi_object_t *po)
{
	isc_result_t status;
#if defined (DEBUG_MEMORY_LEAKAGE)
	unsigned long previous_outstanding = dmalloc_outstanding;
#endif

	status = omapi_message_process_internal (mo, po);

#if defined (DEBUG_MEMORY_LEAKAGE) && 0
	log_info ("generation %ld: %ld new, %ld outstanding, %ld long-term",
		  dmalloc_generation,
		  dmalloc_outstanding - previous_outstanding,
		  dmalloc_outstanding, dmalloc_longterm);
#endif
#if defined (DEBUG_MEMORY_LEAKAGE) && 0
	dmalloc_dump_outstanding ();
#endif
#if defined (DEBUG_RC_HISTORY_EXHAUSTIVELY) && 0
	dump_rc_history ();
#endif

	return status;
}

static isc_result_t
omapi_message_process_internal (omapi_object_t *mo, omapi_object_t *po)
{
	omapi_message_object_t *message, *m;
	omapi_object_t *object = (omapi_object_t *)0;
	omapi_value_t *tv = (omapi_value_t *)0;
	unsigned long create, update, exclusive;
	unsigned long wsi;
	isc_result_t status, waitstatus;
	omapi_object_type_t *type;

	if (mo -> type != omapi_type_message)
		return ISC_R_INVALIDARG;
	message = (omapi_message_object_t *)mo;

#ifdef DEBUG_PROTOCOL
	log_debug ("omapi_message_process(): "
		   "op=%s  handle=%#x  id=%#x  rid=%#x",
		   omapi_message_op_name (message -> op),
		   message -> h, message -> id, message -> rid);
#endif

	if (message -> rid) {
		for (m = omapi_registered_messages; m; m = m -> next)
			if (m -> id == message -> rid)
				break;
		/* If we don't have a real message corresponding to
		   the message ID to which this message claims it is a
		   response, something's fishy. */
		if (!m)
			return ISC_R_NOTFOUND;
		/* The authenticator on responses must match the initial
		   message. */
		if (message -> authid != m -> authid)
			return ISC_R_NOTFOUND;
	} else {
		m = (omapi_message_object_t *)0;

		/* All messages must have an authenticator, with the exception
		   of messages that are opening a new authenticator. */
		if (omapi_protocol_authenticated (po) &&
		    !message -> id_object &&
		    message -> op != OMAPI_OP_OPEN) {
			return omapi_protocol_send_status
				(po, message -> id_object, ISC_R_NOKEYS,
				 message -> id, "No authenticator on message");
		}
	}

	switch (message -> op) {
	      case OMAPI_OP_OPEN:
		if (m) {
			return omapi_protocol_send_status
				(po, message -> id_object, ISC_R_INVALIDARG,
				 message -> id, "OPEN can't be a response");
		}

		/* Get the type of the requested object, if one was
		   specified. */
		status = omapi_get_value_str (mo, message -> id_object,
					      "type", &tv);
		if (status == ISC_R_SUCCESS &&
		    (tv -> value -> type == omapi_datatype_data ||
		     tv -> value -> type == omapi_datatype_string)) {
			for (type = omapi_object_types;
			     type; type = type -> next)
				if (!omapi_td_strcmp (tv -> value,
						      type -> name))
					break;
		} else
			type = (omapi_object_type_t *)0;
		if (tv)
			omapi_value_dereference (&tv, MDL);

		/* If this object had no authenticator, the requested object
		   must be an authenticator object. */
		if (omapi_protocol_authenticated (po) &&
		    !message -> id_object &&
		    type != omapi_type_auth_key) {
			return omapi_protocol_send_status
				(po, message -> id_object, ISC_R_NOKEYS,
				 message -> id, "No authenticator on message");
		}

		/* Get the create flag. */
		status = omapi_get_value_str (mo, message -> id_object,
					      "create", &tv);
		if (status == ISC_R_SUCCESS) {
			status = omapi_get_int_value (&create, tv -> value);
			omapi_value_dereference (&tv, MDL);
			if (status != ISC_R_SUCCESS) {
				return omapi_protocol_send_status
					(po, message -> id_object,
					 status, message -> id,
					 "invalid create flag value");
			}
		} else
			create = 0;

		/* Get the update flag. */
		status = omapi_get_value_str (mo, message -> id_object,
					      "update", &tv);
		if (status == ISC_R_SUCCESS) {
			status = omapi_get_int_value (&update, tv -> value);
			omapi_value_dereference (&tv, MDL);
			if (status != ISC_R_SUCCESS) {
				return omapi_protocol_send_status
					(po, message -> id_object,
					 status, message -> id,
					 "invalid update flag value");
			}
		} else
			update = 0;

		/* Get the exclusive flag. */
		status = omapi_get_value_str (mo, message -> id_object,
					      "exclusive", &tv);
		if (status == ISC_R_SUCCESS) {
			status = omapi_get_int_value (&exclusive, tv -> value);
			omapi_value_dereference (&tv, MDL);
			if (status != ISC_R_SUCCESS) {
				return omapi_protocol_send_status
					(po, message -> id_object,
					 status, message -> id,
					 "invalid exclusive flag value");
			}
		} else
			exclusive = 0;

		/* If we weren't given a type, look the object up with
                   the handle. */
		if (!type) {
			if (create) {
				return omapi_protocol_send_status
					(po, message -> id_object,
					 ISC_R_INVALIDARG,
					 message -> id,
					 "type required on create");
			}
			goto refresh;
		}

		/* If the type doesn't provide a lookup method, we can't
		   look up the object. */
		if (!type -> lookup) {
			return omapi_protocol_send_status
				(po, message -> id_object,
				 ISC_R_NOTIMPLEMENTED, message -> id,
				 "unsearchable object type");
		}

		status = (*(type -> lookup)) (&object, message -> id_object,
					      message -> object);

		if (status != ISC_R_SUCCESS &&
		    status != ISC_R_NOTFOUND &&
		    status != ISC_R_NOKEYS) {
			return omapi_protocol_send_status
				(po, message -> id_object,
				 status, message -> id,
				 "object lookup failed");
		}

		/* If we didn't find the object and we aren't supposed to
		   create it, return an error. */
		if (status == ISC_R_NOTFOUND && !create) {
			return omapi_protocol_send_status
				(po, message -> id_object,
				 ISC_R_NOTFOUND, message -> id,
				 "no object matches specification");
		}			

		/* If we found an object, we're supposed to be creating an
		   object, and we're not supposed to have found an object,
		   return an error. */
		if (status == ISC_R_SUCCESS && create && exclusive) {
			omapi_object_dereference (&object, MDL);
			return omapi_protocol_send_status
				(po, message -> id_object,
				 ISC_R_EXISTS, message -> id,
				 "specified object already exists");
		}

		/* If we're creating the object, do it now. */
		if (!object) {
			status = omapi_object_create (&object,
						      message -> id_object,
						      type);
			if (status != ISC_R_SUCCESS) {
				return omapi_protocol_send_status
					(po, message -> id_object,
					 status, message -> id,
					 "can't create new object");
			}
		}

		/* If we're updating it, do so now. */
		if (create || update) {
			/* This check does not belong here. */
			if (object -> type == omapi_type_auth_key) {
				omapi_object_dereference (&object, MDL);
				return omapi_protocol_send_status
					(po, message -> id_object,
					 status, message -> id,
					 "can't update object");
			}

			status = omapi_object_update (object,
						      message -> id_object,
						      message -> object,
						      message -> h);
			if (status != ISC_R_SUCCESS) {
				omapi_object_dereference (&object, MDL);
				return omapi_protocol_send_status
					(po, message -> id_object,
					 status, message -> id,
					 "can't update object");
			}
		}

		/* If this is an authenticator object, add it to the active
		   set for the connection. */
		if (object -> type == omapi_type_auth_key) {
			omapi_handle_t handle;
			status = omapi_object_handle (&handle, object);
			if (status != ISC_R_SUCCESS) {
				omapi_object_dereference (&object, MDL);
				return omapi_protocol_send_status
					(po, message -> id_object,
					 status, message -> id,
					 "can't select authenticator");
			}

			status = omapi_protocol_add_auth (po, object, handle);
			if (status != ISC_R_SUCCESS) {
				omapi_object_dereference (&object, MDL);
				return omapi_protocol_send_status
					(po, message -> id_object,
					 status, message -> id,
					 "can't select authenticator");
			}
		}
		
		/* Now send the new contents of the object back in
		   response. */
		goto send;

	      case OMAPI_OP_REFRESH:
	      refresh:
		status = omapi_handle_lookup (&object, message -> h);
		if (status != ISC_R_SUCCESS) {
			return omapi_protocol_send_status
				(po, message -> id_object,
				 status, message -> id,
				 "no matching handle");
		}
	      send:		
		status = omapi_protocol_send_update (po, message -> id_object,
						     message -> id, object);
		omapi_object_dereference (&object, MDL);
		return status;

	      case OMAPI_OP_UPDATE:
		if (m && m -> object) {
			omapi_object_reference (&object, m -> object, MDL);
		} else {
			status = omapi_handle_lookup (&object, message -> h);
			if (status != ISC_R_SUCCESS) {
				return omapi_protocol_send_status
					(po, message -> id_object,
					 status, message -> id,
					 "no matching handle");
			}
		}

		if (object -> type == omapi_type_auth_key ||
		    (object -> inner &&
		     object -> inner -> type == omapi_type_auth_key)) {
			if (!m) {
				omapi_object_dereference (&object, MDL);
				return omapi_protocol_send_status
					(po, message -> id_object,
					 status, message -> id,
					 "cannot update authenticator");
			}
			
			status = omapi_protocol_add_auth (po, object,
							  message -> h);
		} else {
			status = omapi_object_update (object,
						      message -> id_object,
						      message -> object,
						      message -> h);
		}
		if (status != ISC_R_SUCCESS) {
			omapi_object_dereference (&object, MDL);
			if (!message -> rid)
				return omapi_protocol_send_status
					(po, message -> id_object,
					 status, message -> id,
					 "can't update object");
			if (m)
				omapi_signal ((omapi_object_t *)m,
					      "status", status,
					      (omapi_typed_data_t *)0);
			return ISC_R_SUCCESS;
		}
		if (!message -> rid)
			status = omapi_protocol_send_status
				(po, message -> id_object, ISC_R_SUCCESS,
				 message -> id, (char *)0);
		if (m) {
			omapi_signal ((omapi_object_t *)m,
				      "status", ISC_R_SUCCESS,
				      (omapi_typed_data_t *)0);
			omapi_message_unregister ((omapi_object_t *)m);
		}

		omapi_object_dereference (&object, MDL);

		return status;

	      case OMAPI_OP_NOTIFY:
		return omapi_protocol_send_status
			(po, message -> id_object, ISC_R_NOTIMPLEMENTED,
			 message -> id, "notify not implemented yet");

	      case OMAPI_OP_STATUS:
		/* The return status of a request. */
		if (!m)
			return ISC_R_UNEXPECTED;

		/* Get the wait status. */
		status = omapi_get_value_str (mo, message -> id_object,
					      "result", &tv);
		if (status == ISC_R_SUCCESS) {
			status = omapi_get_int_value (&wsi, tv -> value);
			waitstatus = wsi;
			omapi_value_dereference (&tv, MDL);
			if (status != ISC_R_SUCCESS)
				waitstatus = ISC_R_UNEXPECTED;
		} else
			waitstatus = ISC_R_UNEXPECTED;

		status = omapi_get_value_str (mo, message -> id_object,
					      "message", &tv);
		omapi_signal ((omapi_object_t *)m, "status", waitstatus, tv);
		if (status == ISC_R_SUCCESS)
			omapi_value_dereference (&tv, MDL);

		omapi_message_unregister((omapi_object_t *)m);

		return ISC_R_SUCCESS;

	      case OMAPI_OP_DELETE:
		status = omapi_handle_lookup (&object, message -> h);
		if (status != ISC_R_SUCCESS) {
			return omapi_protocol_send_status
				(po, message -> id_object,
				 status, message -> id,
				 "no matching handle");
		}

		if (!object -> type -> remove)
			return omapi_protocol_send_status
				(po, message -> id_object,
				 ISC_R_NOTIMPLEMENTED, message -> id,
				 "no remove method for object");

		status = (*(object -> type -> remove)) (object,
							message -> id_object);
		omapi_object_dereference (&object, MDL);

		return omapi_protocol_send_status (po, message -> id_object,
						   status, message -> id,
						   (char *)0);
	}
	return ISC_R_NOTIMPLEMENTED;
}
