/* dhcpctl.c

   Subroutines providing general support for objects. */

/*
 * Copyright (c) 1999-2002 Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about the Internet Software Consortium, see
 * ``http://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

#include <omapip/omapip_p.h>
#include "dhcpctl.h"

omapi_object_type_t *dhcpctl_callback_type;
omapi_object_type_t *dhcpctl_remote_type;

/* dhcpctl_initialize ()

   Must be called before any other dhcpctl function. */

dhcpctl_status dhcpctl_initialize ()
{
	isc_result_t status;

	status = omapi_init();
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_object_type_register (&dhcpctl_callback_type,
					     "dhcpctl-callback",
					     dhcpctl_callback_set_value,
					     dhcpctl_callback_get_value,
					     dhcpctl_callback_destroy,
					     dhcpctl_callback_signal_handler,
					     dhcpctl_callback_stuff_values,
					     0, 0, 0, 0, 0, 0,
					     sizeof
					     (dhcpctl_callback_object_t), 0,
					     RC_MISC);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_object_type_register (&dhcpctl_remote_type,
					     "dhcpctl-remote",
					     dhcpctl_remote_set_value,
					     dhcpctl_remote_get_value,
					     dhcpctl_remote_destroy,
					     dhcpctl_remote_signal_handler,
					     dhcpctl_remote_stuff_values,
					     0, 0, 0, 0, 0, 0,
					     sizeof (dhcpctl_remote_object_t),
					     0, RC_MISC);
	if (status != ISC_R_SUCCESS)
		return status;

	return ISC_R_SUCCESS;
}

/* dhcpctl_connect

   synchronous
   returns nonzero status code if it didn't connect, zero otherwise
   stores connection handle through connection, which can be used
   for subsequent access to the specified server. 
   server_name is the name of the server, and port is the TCP
   port on which it is listening.
   authinfo is the handle to an object containing authentication
   information. */

dhcpctl_status dhcpctl_connect (dhcpctl_handle *connection,
				const char *server_name, int port,
				dhcpctl_handle authinfo)
{
	isc_result_t status;
	dhcpctl_status waitstatus;

	status = omapi_generic_new (connection, MDL);
	if (status != ISC_R_SUCCESS) {
		return status;
	}

	status = omapi_protocol_connect (*connection, server_name,
					 (unsigned)port, authinfo);
	if (status == ISC_R_SUCCESS)
		return status;
	if (status != ISC_R_INCOMPLETE) {
		omapi_object_dereference (connection, MDL);
		return status;
	}

	status = omapi_wait_for_completion (*connection, 0);
	if (status != ISC_R_SUCCESS) {
		omapi_object_dereference (connection, MDL);
		return status;
	}

	return status;
}

/* dhcpctl_wait_for_completion

   synchronous
   returns zero if the callback completes, a nonzero status if
   there was some problem relating to the wait operation.   The
   status of the queued request will be stored through s, and
   will also be either zero for success or nonzero for some kind
   of failure.    Never returns until completion or until the
   connection to the server is lost.   This performs the same
   function as dhcpctl_set_callback and the subsequent callback,
   for programs that want to do inline execution instead of using
   callbacks. */

dhcpctl_status dhcpctl_wait_for_completion (dhcpctl_handle h,
					    dhcpctl_status *s)
{
	isc_result_t status;
	status = omapi_wait_for_completion (h, 0);
	if (status != ISC_R_SUCCESS)
		return status;
	if (h -> type == dhcpctl_remote_type)
		*s = ((dhcpctl_remote_object_t *)h) -> waitstatus;
	return ISC_R_SUCCESS;
}

/* dhcpctl_get_value

   synchronous
   returns zero if the call succeeded, a nonzero status code if
   it didn't. 
   result is the address of an empty data string (initialized
   with bzero or cleared with data_string_forget).   On
   successful completion, the addressed data string will contain
   the value that was fetched.
   dhcpctl_handle refers to some dhcpctl item
   value_name refers to some value related to that item - e.g.,
   for a handle associated with a completed host lookup, value
   could be one of "hardware-address", "dhcp-client-identifier",
   "known" or "client-hostname". */

dhcpctl_status dhcpctl_get_value (dhcpctl_data_string *result,
				  dhcpctl_handle h, const char *value_name)
{
	isc_result_t status;
	omapi_value_t *tv = (omapi_value_t *)0;
	omapi_data_string_t *value = (omapi_data_string_t *)0;
	unsigned len;
	int ip;

	status = omapi_get_value_str (h, (omapi_object_t *)0, value_name, &tv);
	if (status != ISC_R_SUCCESS)
		return status;

	switch (tv -> value -> type) {
	      case omapi_datatype_int:
		len = sizeof (int);
		break;

	      case omapi_datatype_string:
	      case omapi_datatype_data:
		len = tv -> value -> u.buffer.len;
		break;

	      case omapi_datatype_object:
		len = sizeof (omapi_handle_t);
		break;

	      default:
		omapi_typed_data_dereference (&tv -> value, MDL);
		return ISC_R_UNEXPECTED;
	}

	status = omapi_data_string_new (result, len, MDL);
	if (status != ISC_R_SUCCESS) {
		omapi_typed_data_dereference (&tv -> value, MDL);
		return status;
	}

	switch (tv -> value -> type) {
	      case omapi_datatype_int:
		ip = htonl (tv -> value -> u.integer);
		memcpy ((*result) -> value, &ip, sizeof ip);
		break;

	      case omapi_datatype_string:
	      case omapi_datatype_data:
		memcpy ((*result) -> value,
			tv -> value -> u.buffer.value,
			tv -> value -> u.buffer.len);
		break;

	      case omapi_datatype_object:
		ip = htonl (tv -> value -> u.object -> handle);
		memcpy ((*result) -> value, &ip, sizeof ip);
		break;
	}

	omapi_value_dereference (&tv, MDL);
	return ISC_R_SUCCESS;
}

/* dhcpctl_get_boolean

   like dhcpctl_get_value, but more convenient for boolean
   values, since no data_string needs to be dealt with. */

dhcpctl_status dhcpctl_get_boolean (int *result,
				    dhcpctl_handle h, const char *value_name)
{
	isc_result_t status;
	dhcpctl_data_string data = (dhcpctl_data_string)0;
	int rv;
	
	status = dhcpctl_get_value (&data, h, value_name);
	if (status != ISC_R_SUCCESS)
		return status;
	if (data -> len != sizeof rv) {
		omapi_data_string_dereference (&data, MDL);
		return ISC_R_UNEXPECTED;
	}
	memcpy (&rv, data -> value, sizeof rv);
	*result = ntohl (rv);
	return ISC_R_SUCCESS;
}

/* dhcpctl_set_value

   Sets a value on an object referred to by a dhcpctl_handle.
   The opposite of dhcpctl_get_value.   Does not update the
   server - just sets the value on the handle. */

dhcpctl_status dhcpctl_set_value (dhcpctl_handle h, dhcpctl_data_string value,
				  const char *value_name)
{
	isc_result_t status;
	omapi_typed_data_t *tv = (omapi_typed_data_t *)0;
	omapi_data_string_t *name = (omapi_data_string_t *)0;
	int len;

	status = omapi_data_string_new (&name, strlen (value_name), MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	memcpy (name -> value, value_name, strlen (value_name));

	status = omapi_typed_data_new (MDL, &tv, omapi_datatype_data,
				       value -> len);
	if (status != ISC_R_SUCCESS) {
		omapi_data_string_dereference (&name, MDL);
		return status;
	}
	memcpy (tv -> u.buffer.value, value -> value, value -> len);

	status = omapi_set_value (h, (omapi_object_t *)0, name, tv);
	omapi_data_string_dereference (&name, MDL);
	omapi_typed_data_dereference (&tv, MDL);
	return status;
}

/* dhcpctl_set_string_value

   Sets a NUL-terminated ASCII value on an object referred to by
   a dhcpctl_handle.   like dhcpctl_set_value, but saves the
   trouble of creating a data_string for a NUL-terminated string.
   Does not update the server - just sets the value on the handle. */

dhcpctl_status dhcpctl_set_string_value (dhcpctl_handle h, const char *value,
					 const char *value_name)
{
	isc_result_t status;
	omapi_typed_data_t *tv = (omapi_typed_data_t *)0;
	omapi_data_string_t *name = (omapi_data_string_t *)0;
	int len;

	status = omapi_data_string_new (&name, strlen (value_name), MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	memcpy (name -> value, value_name, strlen (value_name));

	status = omapi_typed_data_new (MDL, &tv, omapi_datatype_string, value);
	if (status != ISC_R_SUCCESS) {
		omapi_data_string_dereference (&name, MDL);
		return status;
	}

	status = omapi_set_value (h, (omapi_object_t *)0, name, tv);
	omapi_data_string_dereference (&name, MDL);
	omapi_typed_data_dereference (&tv, MDL);
	return status;
}

/* dhcpctl_set_buffer_value

   Sets a value on an object referred to by a dhcpctl_handle.  like
   dhcpctl_set_value, but saves the trouble of creating a data_string
   for string for which we have a buffer and length.  Does not update
   the server - just sets the value on the handle. */

dhcpctl_status dhcpctl_set_data_value (dhcpctl_handle h,
				       const char *value, unsigned len,
				       const char *value_name)
{
	isc_result_t status;
	omapi_typed_data_t *tv = (omapi_typed_data_t *)0;
	omapi_data_string_t *name = (omapi_data_string_t *)0;
	unsigned ll;

	ll = strlen (value_name);
	status = omapi_data_string_new (&name, ll, MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	memcpy (name -> value, value_name, ll);

	status = omapi_typed_data_new (MDL, &tv,
				       omapi_datatype_data, len, value);
	if (status != ISC_R_SUCCESS) {
		omapi_data_string_dereference (&name, MDL);
		return status;
	}
	memcpy (tv -> u.buffer.value, value, len);

	status = omapi_set_value (h, (omapi_object_t *)0, name, tv);
	omapi_data_string_dereference (&name, MDL);
	omapi_typed_data_dereference (&tv, MDL);
	return status;
}

/* dhcpctl_set_null_value

   Sets a null value on an object referred to by a dhcpctl_handle. */

dhcpctl_status dhcpctl_set_null_value (dhcpctl_handle h,
				       const char *value_name)
{
	isc_result_t status;
	omapi_data_string_t *name = (omapi_data_string_t *)0;
	unsigned ll;

	ll = strlen (value_name);
	status = omapi_data_string_new (&name, ll, MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	memcpy (name -> value, value_name, ll);

	status = omapi_set_value (h, (omapi_object_t *)0, name,
				  (omapi_typed_data_t *)0);
	omapi_data_string_dereference (&name, MDL);
	return status;
}

/* dhcpctl_set_boolean_value

   Sets a boolean value on an object - like dhcpctl_set_value,
   only more convenient for booleans. */

dhcpctl_status dhcpctl_set_boolean_value (dhcpctl_handle h, int value,
					  const char *value_name)
{
	isc_result_t status;
	omapi_typed_data_t *tv = (omapi_typed_data_t *)0;
	omapi_data_string_t *name = (omapi_data_string_t *)0;
	int len;

	status = omapi_data_string_new (&name, strlen (value_name), MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	memcpy (name -> value, value_name, strlen (value_name));

	status = omapi_typed_data_new (MDL, &tv, omapi_datatype_int, value);
	if (status != ISC_R_SUCCESS) {
		omapi_data_string_dereference (&name, MDL);
		return status;
	}

	status = omapi_set_value (h, (omapi_object_t *)0, name, tv);
	omapi_data_string_dereference (&name, MDL);
	omapi_typed_data_dereference (&tv, MDL);
	return status;
}

/* dhcpctl_set_int_value

   Sets a boolean value on an object - like dhcpctl_set_value,
   only more convenient for booleans. */

dhcpctl_status dhcpctl_set_int_value (dhcpctl_handle h, int value,
				      const char *value_name)
{
	isc_result_t status;
	omapi_typed_data_t *tv = (omapi_typed_data_t *)0;
	omapi_data_string_t *name = (omapi_data_string_t *)0;
	int len;

	status = omapi_data_string_new (&name, strlen (value_name), MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	memcpy (name -> value, value_name, strlen (value_name));

	status = omapi_typed_data_new (MDL, &tv, omapi_datatype_int, value);
	if (status != ISC_R_SUCCESS) {
		omapi_data_string_dereference (&name, MDL);
		return status;
	}

	status = omapi_set_value (h, (omapi_object_t *)0, name, tv);
	omapi_data_string_dereference (&name, MDL);
	omapi_typed_data_dereference (&tv, MDL);
	return status;
}

/* dhcpctl_object_update

   Queues an update on the object referenced by the handle (there
   can't be any other work in progress on the handle).   An
   update means local parameters will be sent to the server. */

dhcpctl_status dhcpctl_object_update (dhcpctl_handle connection,
				      dhcpctl_handle h)
{
	isc_result_t status;
	omapi_object_t *message = (omapi_object_t *)0;
	dhcpctl_remote_object_t *ro;

	if (h -> type != dhcpctl_remote_type)
		return ISC_R_INVALIDARG;
	ro = (dhcpctl_remote_object_t *)h;

	status = omapi_message_new (&message, MDL);
	if (status != ISC_R_SUCCESS) {
		omapi_object_dereference (&message, MDL);
		return status;
	}
	status = omapi_set_int_value (message, (omapi_object_t *)0,
				      "op", OMAPI_OP_UPDATE);
	if (status != ISC_R_SUCCESS) {
		omapi_object_dereference (&message, MDL);
		return status;
	}

	status = omapi_set_object_value (message, (omapi_object_t *)0,
					 "object", h);
	if (status != ISC_R_SUCCESS) {
		omapi_object_dereference (&message, MDL);
		return status;
	}

	status = omapi_set_int_value (message, (omapi_object_t *)0, "handle",
				      (int)(ro -> remote_handle));
	if (status != ISC_R_SUCCESS) {
		omapi_object_dereference (&message, MDL);
		return status;
	}

	omapi_message_register (message);
	status = omapi_protocol_send_message (connection -> outer,
					      (omapi_object_t *)0,
					      message, (omapi_object_t *)0);
	omapi_object_dereference (&message, MDL);
	return status;
}

/* Requests a refresh on the object referenced by the handle (there
   can't be any other work in progress on the handle).   A
   refresh means local parameters are updated from the server. */

dhcpctl_status dhcpctl_object_refresh (dhcpctl_handle connection,
				       dhcpctl_handle h)
{
	isc_result_t status;
	omapi_object_t *message = (omapi_object_t *)0;
	dhcpctl_remote_object_t *ro;

	if (h -> type != dhcpctl_remote_type)
		return ISC_R_INVALIDARG;
	ro = (dhcpctl_remote_object_t *)h;

	status = omapi_message_new (&message, MDL);
	if (status != ISC_R_SUCCESS) {
		omapi_object_dereference (&message, MDL);
		return status;
	}
	status = omapi_set_int_value (message, (omapi_object_t *)0,
				      "op", OMAPI_OP_REFRESH);
	if (status != ISC_R_SUCCESS) {
		omapi_object_dereference (&message, MDL);
		return status;
	}
	status = omapi_set_int_value (message, (omapi_object_t *)0,
				      "handle", (int)(ro -> remote_handle));
	if (status != ISC_R_SUCCESS) {
		omapi_object_dereference (&message, MDL);
		return status;
	}

	omapi_message_register (message);
	status = omapi_protocol_send_message (connection -> outer,
					      (omapi_object_t *)0,
					      message, (omapi_object_t *)0);

	/* We don't want to send the contents of the object down the
	   wire, but we do need to reference it so that we know what
	   to do with the update. */
	status = omapi_set_object_value (message, (omapi_object_t *)0,
					 "object", h);
	if (status != ISC_R_SUCCESS) {
		omapi_object_dereference (&message, MDL);
		return status;
	}

	omapi_object_dereference (&message, MDL);
	return status;
}

/* Requests the removal of the object referenced by the handle (there
   can't be any other work in progress on the handle).   A
   removal means that all searchable references to the object on the
   server are deleted. */

dhcpctl_status dhcpctl_object_remove (dhcpctl_handle connection,
				      dhcpctl_handle h)
{
	isc_result_t status;
	omapi_object_t *message = (omapi_object_t *)0;
	dhcpctl_remote_object_t *ro;

	if (h -> type != dhcpctl_remote_type)
		return ISC_R_INVALIDARG;
	ro = (dhcpctl_remote_object_t *)h;

	status = omapi_message_new (&message, MDL);
	if (status != ISC_R_SUCCESS) {
		omapi_object_dereference (&message, MDL);
		return status;
	}
	status = omapi_set_int_value (message, (omapi_object_t *)0,
				      "op", OMAPI_OP_DELETE);
	if (status != ISC_R_SUCCESS) {
		omapi_object_dereference (&message, MDL);
		return status;
	}

	status = omapi_set_int_value (message, (omapi_object_t *)0, "handle",
				      (int)(ro -> remote_handle));
	if (status != ISC_R_SUCCESS) {
		omapi_object_dereference (&message, MDL);
		return status;
	}

	status = omapi_set_object_value (message, (omapi_object_t *)0,
					 "notify-object", h);
	if (status != ISC_R_SUCCESS) {
		omapi_object_dereference (&message, MDL);
		return status;
	}

	omapi_message_register (message);
	status = omapi_protocol_send_message (connection -> outer,
					      (omapi_object_t *)0,
					      message, (omapi_object_t *)0);
	omapi_object_dereference (&message, MDL);
	return status;
}

isc_result_t dhcpctl_data_string_dereference (dhcpctl_data_string *vp,
					      const char *file, int line)
{
	return omapi_data_string_dereference (vp, file, line);
}
