/* support.c

   Subroutines providing general support for objects. */

/*
 * Copyright (c) 1999-2001 Internet Software Consortium.
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

omapi_object_type_t *omapi_type_connection;
omapi_object_type_t *omapi_type_listener;
omapi_object_type_t *omapi_type_io_object;
omapi_object_type_t *omapi_type_datagram;
omapi_object_type_t *omapi_type_generic;
omapi_object_type_t *omapi_type_protocol;
omapi_object_type_t *omapi_type_protocol_listener;
omapi_object_type_t *omapi_type_waiter;
omapi_object_type_t *omapi_type_remote;
omapi_object_type_t *omapi_type_message;
omapi_object_type_t *omapi_type_auth_key;

omapi_object_type_t *omapi_object_types;
int omapi_object_type_count;
static int ot_max;

#if defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
void omapi_type_relinquish ()
{
	omapi_object_type_t *t, *n;

	for (t = omapi_object_types; t; t = n) {
		n = t -> next;
		dfree (t, MDL);
	}
	omapi_object_types = (omapi_object_type_t *)0;
}
#endif

isc_result_t omapi_init (void)
{
	isc_result_t status;

	dst_init();

	/* Register all the standard object types... */
	status = omapi_object_type_register (&omapi_type_connection,
					     "connection",
					     omapi_connection_set_value,
					     omapi_connection_get_value,
					     omapi_connection_destroy,
					     omapi_connection_signal_handler,
					     omapi_connection_stuff_values,
					     0, 0, 0, 0, 0, 0,
					     sizeof
					     (omapi_connection_object_t), 0,
					     RC_MISC);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_object_type_register (&omapi_type_listener,
					     "listener",
					     omapi_listener_set_value,
					     omapi_listener_get_value,
					     omapi_listener_destroy,
					     omapi_listener_signal_handler,
					     omapi_listener_stuff_values,
					     0, 0, 0, 0, 0, 0,
					     sizeof (omapi_listener_object_t),
					     0, RC_MISC);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_object_type_register (&omapi_type_io_object,
					     "io",
					     omapi_io_set_value,
					     omapi_io_get_value,
					     omapi_io_destroy,
					     omapi_io_signal_handler,
					     omapi_io_stuff_values,
					     0, 0, 0, 0, 0, 0,
					     sizeof (omapi_io_object_t),
					     0, RC_MISC);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_object_type_register (&omapi_type_generic,
					     "generic",
					     omapi_generic_set_value,
					     omapi_generic_get_value,
					     omapi_generic_destroy,
					     omapi_generic_signal_handler,
					     omapi_generic_stuff_values,
					     0, 0, 0, 0, 0, 0,
					     sizeof (omapi_generic_object_t),
					     0, RC_MISC);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_object_type_register (&omapi_type_protocol,
					     "protocol",
					     omapi_protocol_set_value,
					     omapi_protocol_get_value,
					     omapi_protocol_destroy,
					     omapi_protocol_signal_handler,
					     omapi_protocol_stuff_values,
					     0, 0, 0, 0, 0, 0,
					     sizeof (omapi_protocol_object_t),
					     0, RC_MISC);
	if (status != ISC_R_SUCCESS)
		return status;

	status = (omapi_object_type_register
		  (&omapi_type_protocol_listener, "protocol-listener",
		   omapi_protocol_listener_set_value,
		   omapi_protocol_listener_get_value,
		   omapi_protocol_listener_destroy,
		   omapi_protocol_listener_signal,
		   omapi_protocol_listener_stuff,
		   0, 0, 0, 0, 0, 0,
		   sizeof (omapi_protocol_listener_object_t), 0, RC_MISC));
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_object_type_register (&omapi_type_message,
					     "message",
					     omapi_message_set_value,
					     omapi_message_get_value,
					     omapi_message_destroy,
					     omapi_message_signal_handler,
					     omapi_message_stuff_values,
					     0, 0, 0, 0, 0, 0,
					     sizeof (omapi_message_object_t),
					     0, RC_MISC);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_object_type_register (&omapi_type_waiter,
					     "waiter",
					     0,
					     0,
					     0,
					     omapi_waiter_signal_handler, 0,
					     0, 0, 0, 0, 0, 0,
					     sizeof (omapi_waiter_object_t),
					     0, RC_MISC);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_object_type_register (&omapi_type_auth_key,
					     "authenticator",
					     0,
					     omapi_auth_key_get_value,
					     omapi_auth_key_destroy,
					     0,
					     omapi_auth_key_stuff_values,
					     omapi_auth_key_lookup,
					     0, 0, 0, 0, 0,
					     sizeof (omapi_auth_key_t), 0,
					     RC_MISC);
	if (status != ISC_R_SUCCESS)
		return status;

#if defined (TRACING)
	omapi_listener_trace_setup ();
	omapi_connection_trace_setup ();
	omapi_buffer_trace_setup ();
	trace_mr_init ();
#endif

	/* This seems silly, but leave it. */
	return ISC_R_SUCCESS;
}

isc_result_t omapi_object_type_register (omapi_object_type_t **type,
					 const char *name,
					 isc_result_t (*set_value)
						 (omapi_object_t *,
						  omapi_object_t *,
						  omapi_data_string_t *,
						  omapi_typed_data_t *),
					 isc_result_t (*get_value)
						(omapi_object_t *,
						 omapi_object_t *,
						 omapi_data_string_t *,
						 omapi_value_t **),
					 isc_result_t (*destroy)
						(omapi_object_t *,
						 const char *, int),
					 isc_result_t (*signal_handler)
						 (omapi_object_t *,
						  const char *, va_list),
					 isc_result_t (*stuff_values)
						(omapi_object_t *,
						 omapi_object_t *,
						 omapi_object_t *),
					 isc_result_t (*lookup)
						(omapi_object_t **,
						 omapi_object_t *,
						 omapi_object_t *),
					 isc_result_t (*create)
						(omapi_object_t **,
						 omapi_object_t *),
					 isc_result_t (*remove)
						(omapi_object_t *,
						 omapi_object_t *),
					 isc_result_t (*freer)
						(omapi_object_t *,
						 const char *, int),
					 isc_result_t (*allocator)
						(omapi_object_t **,
						 const char *, int),
					 isc_result_t (*sizer) (size_t),
					 size_t size,
					 isc_result_t (*initialize)
						(omapi_object_t *,
						 const char *, int),
					 int rc_flag)
{
	omapi_object_type_t *t;

	t = dmalloc (sizeof *t, MDL);
	if (!t)
		return ISC_R_NOMEMORY;
	memset (t, 0, sizeof *t);

	t -> name = name;
	t -> set_value = set_value;
	t -> get_value = get_value;
	t -> destroy = destroy;
	t -> signal_handler = signal_handler;
	t -> stuff_values = stuff_values;
	t -> lookup = lookup;
	t -> create = create;
	t -> remove = remove;
	t -> next = omapi_object_types;
	t -> sizer = sizer;
	t -> size = size;
	t -> freer = freer;
	t -> allocator = allocator;
	t -> initialize = initialize;
	t -> rc_flag = rc_flag;
	omapi_object_types = t;
	if (type)
		*type = t;
	return ISC_R_SUCCESS;
}

isc_result_t omapi_signal (omapi_object_t *handle, const char *name, ...)
{
	va_list ap;
	omapi_object_t *outer;
	isc_result_t status;

	va_start (ap, name);
	for (outer = handle; outer -> outer; outer = outer -> outer)
		;
	if (outer -> type -> signal_handler)
		status = (*(outer -> type -> signal_handler)) (outer,
							       name, ap);
	else
		status = ISC_R_NOTFOUND;
	va_end (ap);
	return status;
}

isc_result_t omapi_signal_in (omapi_object_t *handle, const char *name, ...)
{
	va_list ap;
	omapi_object_t *outer;
	isc_result_t status;

	if (!handle)
		return ISC_R_NOTFOUND;
	va_start (ap, name);

	if (handle -> type -> signal_handler)
		status = (*(handle -> type -> signal_handler)) (handle,
								name, ap);
	else
		status = ISC_R_NOTFOUND;
	va_end (ap);
	return status;
}

isc_result_t omapi_set_value (omapi_object_t *h,
			      omapi_object_t *id,
			      omapi_data_string_t *name,
			      omapi_typed_data_t *value)
{
	omapi_object_t *outer;
	isc_result_t status;

#if defined (DEBUG)
	if (!value) {
		log_info ("omapi_set_value (%.*s, NULL)",
			  (int)name -> len, name -> value);
	} else if (value -> type == omapi_datatype_int) {
		log_info ("omapi_set_value (%.*s, %ld)",
			  (int)name -> len, name -> value,
			  (long)value -> u.integer);
	} else if (value -> type == omapi_datatype_string) {
		log_info ("omapi_set_value (%.*s, %.*s)",
			  (int)name -> len, name -> value,
			  (int)value -> u.buffer.len, value -> u.buffer.value);
	} else if (value -> type == omapi_datatype_data) {
		log_info ("omapi_set_value (%.*s, %ld %lx)",
			  (int)name -> len, name -> value,
			  (long)value -> u.buffer.len,
			  (unsigned long)value -> u.buffer.value);
	} else if (value -> type == omapi_datatype_object) {
		log_info ("omapi_set_value (%.*s, %s)",
			  (int)name -> len, name -> value,
			  value -> u.object
			  ? (value -> u.object -> type
			     ? value -> u.object -> type -> name
			     : "(unknown object)")
			  : "(unknown object)");
	}
#endif

	for (outer = h; outer -> outer; outer = outer -> outer)
		;
	if (outer -> type -> set_value)
		status = (*(outer -> type -> set_value)) (outer,
							  id, name, value);
	else
		status = ISC_R_NOTFOUND;
#if defined (DEBUG)
	log_info (" ==> %s", isc_result_totext (status));
#endif
	return status;
}

isc_result_t omapi_set_value_str (omapi_object_t *h,
				  omapi_object_t *id,
				  const char *name,
				  omapi_typed_data_t *value)
{
	omapi_object_t *outer;
	omapi_data_string_t *nds;
	isc_result_t status;

	nds = (omapi_data_string_t *)0;
	status = omapi_data_string_new (&nds, strlen (name), MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	memcpy (nds -> value, name, strlen (name));

	status = omapi_set_value (h, id, nds, value);
	omapi_data_string_dereference (&nds, MDL);
	return status;
}

isc_result_t omapi_set_boolean_value (omapi_object_t *h, omapi_object_t *id,
				      const char *name, int value)
{
	isc_result_t status;
	omapi_typed_data_t *tv = (omapi_typed_data_t *)0;
	omapi_data_string_t *n = (omapi_data_string_t *)0;
	int len;
	int ip;

	status = omapi_data_string_new (&n, strlen (name), MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	memcpy (n -> value, name, strlen (name));

	status = omapi_typed_data_new (MDL, &tv, omapi_datatype_int, value);
	if (status != ISC_R_SUCCESS) {
		omapi_data_string_dereference (&n, MDL);
		return status;
	}

	status = omapi_set_value (h, id, n, tv);
	omapi_data_string_dereference (&n, MDL);
	omapi_typed_data_dereference (&tv, MDL);
	return status;
}

isc_result_t omapi_set_int_value (omapi_object_t *h, omapi_object_t *id,
				  const char *name, int value)
{
	isc_result_t status;
	omapi_typed_data_t *tv = (omapi_typed_data_t *)0;
	omapi_data_string_t *n = (omapi_data_string_t *)0;
	int len;
	int ip;

	status = omapi_data_string_new (&n, strlen (name), MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	memcpy (n -> value, name, strlen (name));

	status = omapi_typed_data_new (MDL, &tv, omapi_datatype_int, value);
	if (status != ISC_R_SUCCESS) {
		omapi_data_string_dereference (&n, MDL);
		return status;
	}

	status = omapi_set_value (h, id, n, tv);
	omapi_data_string_dereference (&n, MDL);
	omapi_typed_data_dereference (&tv, MDL);
	return status;
}

isc_result_t omapi_set_object_value (omapi_object_t *h, omapi_object_t *id,
				     const char *name, omapi_object_t *value)
{
	isc_result_t status;
	omapi_typed_data_t *tv = (omapi_typed_data_t *)0;
	omapi_data_string_t *n = (omapi_data_string_t *)0;
	int len;
	int ip;

	status = omapi_data_string_new (&n, strlen (name), MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	memcpy (n -> value, name, strlen (name));

	status = omapi_typed_data_new (MDL, &tv, omapi_datatype_object, value);
	if (status != ISC_R_SUCCESS) {
		omapi_data_string_dereference (&n, MDL);
		return status;
	}

	status = omapi_set_value (h, id, n, tv);
	omapi_data_string_dereference (&n, MDL);
	omapi_typed_data_dereference (&tv, MDL);
	return status;
}

isc_result_t omapi_set_string_value (omapi_object_t *h, omapi_object_t *id,
				     const char *name, const char *value)
{
	isc_result_t status;
	omapi_typed_data_t *tv = (omapi_typed_data_t *)0;
	omapi_data_string_t *n = (omapi_data_string_t *)0;
	int len;
	int ip;

	status = omapi_data_string_new (&n, strlen (name), MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	memcpy (n -> value, name, strlen (name));

	status = omapi_typed_data_new (MDL, &tv, omapi_datatype_string, value);
	if (status != ISC_R_SUCCESS) {
		omapi_data_string_dereference (&n, MDL);
		return status;
	}

	status = omapi_set_value (h, id, n, tv);
	omapi_data_string_dereference (&n, MDL);
	omapi_typed_data_dereference (&tv, MDL);
	return status;
}

isc_result_t omapi_get_value (omapi_object_t *h,
			      omapi_object_t *id,
			      omapi_data_string_t *name,
			      omapi_value_t **value)
{
	omapi_object_t *outer;

	for (outer = h; outer -> outer; outer = outer -> outer)
		;
	if (outer -> type -> get_value)
		return (*(outer -> type -> get_value)) (outer,
							id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t omapi_get_value_str (omapi_object_t *h,
				  omapi_object_t *id,
				  const char *name,
				  omapi_value_t **value)
{
	omapi_object_t *outer;
	omapi_data_string_t *nds;
	isc_result_t status;

	nds = (omapi_data_string_t *)0;
	status = omapi_data_string_new (&nds, strlen (name), MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	memcpy (nds -> value, name, strlen (name));

	for (outer = h; outer -> outer; outer = outer -> outer)
		;
	if (outer -> type -> get_value)
		status = (*(outer -> type -> get_value)) (outer,
							  id, nds, value);
	else
		status = ISC_R_NOTFOUND;
	omapi_data_string_dereference (&nds, MDL);
	return status;
}

isc_result_t omapi_stuff_values (omapi_object_t *c,
				 omapi_object_t *id,
				 omapi_object_t *o)
{
	omapi_object_t *outer;

	for (outer = o; outer -> outer; outer = outer -> outer)
		;
	if (outer -> type -> stuff_values)
		return (*(outer -> type -> stuff_values)) (c, id, outer);
	return ISC_R_NOTFOUND;
}

isc_result_t omapi_object_create (omapi_object_t **obj, omapi_object_t *id,
				  omapi_object_type_t *type)
{
	if (!type -> create)
		return ISC_R_NOTIMPLEMENTED;
	return (*(type -> create)) (obj, id);
}

isc_result_t omapi_object_update (omapi_object_t *obj, omapi_object_t *id,
				  omapi_object_t *src, omapi_handle_t handle)
{
	omapi_generic_object_t *gsrc;
	isc_result_t status;
	int i;

	if (!src)
		return ISC_R_INVALIDARG;
	if (src -> type != omapi_type_generic)
		return ISC_R_NOTIMPLEMENTED;
	gsrc = (omapi_generic_object_t *)src;
	for (i = 0; i < gsrc -> nvalues; i++) {
		status = omapi_set_value (obj, id,
					  gsrc -> values [i] -> name,
					  gsrc -> values [i] -> value);
		if (status != ISC_R_SUCCESS && status != ISC_R_UNCHANGED)
			return status;
	}
	if (handle)
		omapi_set_int_value (obj, id, "remote-handle", (int)handle);
	status = omapi_signal (obj, "updated");
	if (status != ISC_R_NOTFOUND)
		return status;
	return ISC_R_SUCCESS;
}

int omapi_data_string_cmp (omapi_data_string_t *s1, omapi_data_string_t *s2)
{
	unsigned len;
	int rv;

	if (s1 -> len > s2 -> len)
		len = s2 -> len;
	else
		len = s1 -> len;
	rv = memcmp (s1 -> value, s2 -> value, len);
	if (rv)
		return rv;
	if (s1 -> len > s2 -> len)
		return 1;
	else if (s1 -> len < s2 -> len)
		return -1;
	return 0;
}

int omapi_ds_strcmp (omapi_data_string_t *s1, const char *s2)
{
	unsigned len, slen;
	int rv;

	slen = strlen (s2);
	if (slen > s1 -> len)
		len = s1 -> len;
	else
		len = slen;
	rv = memcmp (s1 -> value, s2, len);
	if (rv)
		return rv;
	if (s1 -> len > slen)
		return 1;
	else if (s1 -> len < slen)
		return -1;
	return 0;
}

int omapi_td_strcmp (omapi_typed_data_t *s1, const char *s2)
{
	unsigned len, slen;
	int rv;

	/* If the data type is not compatible, never equal. */
	if (s1 -> type != omapi_datatype_data &&
	    s1 -> type != omapi_datatype_string)
		return -1;

	slen = strlen (s2);
	if (slen > s1 -> u.buffer.len)
		len = s1 -> u.buffer.len;
	else
		len = slen;
	rv = memcmp (s1 -> u.buffer.value, s2, len);
	if (rv)
		return rv;
	if (s1 -> u.buffer.len > slen)
		return 1;
	else if (s1 -> u.buffer.len < slen)
		return -1;
	return 0;
}

int omapi_td_strcasecmp (omapi_typed_data_t *s1, const char *s2)
{
	unsigned len, slen;
	int rv;

	/* If the data type is not compatible, never equal. */
	if (s1 -> type != omapi_datatype_data &&
	    s1 -> type != omapi_datatype_string)
		return -1;

	slen = strlen (s2);
	if (slen > s1 -> u.buffer.len)
		len = s1 -> u.buffer.len;
	else
		len = slen;
	rv = casecmp (s1 -> u.buffer.value, s2, len);
	if (rv)
		return rv;
	if (s1 -> u.buffer.len > slen)
		return 1;
	else if (s1 -> u.buffer.len < slen)
		return -1;
	return 0;
}

isc_result_t omapi_make_value (omapi_value_t **vp,
			       omapi_data_string_t *name,
			       omapi_typed_data_t *value,
			       const char *file, int line)
{
	isc_result_t status;

	status = omapi_value_new (vp, file, line);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_data_string_reference (&(*vp) -> name,
					      name, file, line);
	if (status != ISC_R_SUCCESS) {
		omapi_value_dereference (vp, file, line);
		return status;
	}
	if (value) {
		status = omapi_typed_data_reference (&(*vp) -> value,
						     value, file, line);
		if (status != ISC_R_SUCCESS) {
			omapi_value_dereference (vp, file, line);
			return status;
		}
	}
	return ISC_R_SUCCESS;
}

isc_result_t omapi_make_const_value (omapi_value_t **vp,
				     omapi_data_string_t *name,
				     const unsigned char *value,
				     unsigned len,
				     const char *file, int line)
{
	isc_result_t status;

	status = omapi_value_new (vp, file, line);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_data_string_reference (&(*vp) -> name,
					      name, file, line);
	if (status != ISC_R_SUCCESS) {
		omapi_value_dereference (vp, file, line);
		return status;
	}
	if (value) {
		status = omapi_typed_data_new (file, line, &(*vp) -> value,
					       omapi_datatype_data, len);
		if (status != ISC_R_SUCCESS) {
			omapi_value_dereference (vp, file, line);
			return status;
		}
		memcpy ((*vp) -> value -> u.buffer.value, value, len);
	}
	return ISC_R_SUCCESS;
}

isc_result_t omapi_make_int_value (omapi_value_t **vp,
				   omapi_data_string_t *name,
				   int value, const char *file, int line)
{
	isc_result_t status;

	status = omapi_value_new (vp, file, line);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_data_string_reference (&(*vp) -> name,
					      name, file, line);
	if (status != ISC_R_SUCCESS) {
		omapi_value_dereference (vp, file, line);
		return status;
	}
	status = omapi_typed_data_new (file, line, &(*vp) -> value,
				       omapi_datatype_int, value);
	if (status != ISC_R_SUCCESS) {
		omapi_value_dereference (vp, file, line);
		return status;
	}
	return ISC_R_SUCCESS;
}

isc_result_t omapi_make_uint_value (omapi_value_t **vp,
				    omapi_data_string_t *name,
				    unsigned int value,
				    const char *file, int line)
{
	return omapi_make_int_value (vp, name, (int)value, file, line);
}

isc_result_t omapi_make_object_value (omapi_value_t **vp,
				      omapi_data_string_t *name,
				      omapi_object_t *value,
				      const char *file, int line)
{
	isc_result_t status;
	
	status = omapi_value_new (vp, file, line);
	if (status != ISC_R_SUCCESS)
		return status;
	
	status = omapi_data_string_reference (&(*vp) -> name,
                                              name, file, line);
	if (status != ISC_R_SUCCESS) {
		omapi_value_dereference (vp, file, line);
		return status;
	}
	
	if (value) {
		status = omapi_typed_data_new (file, line, &(*vp) -> value,
					       omapi_datatype_object, value);
		if (status != ISC_R_SUCCESS) {
			omapi_value_dereference (vp, file, line);
			return status;
		}
	}
	
	return ISC_R_SUCCESS;
}

isc_result_t omapi_make_handle_value (omapi_value_t **vp,
				      omapi_data_string_t *name,
				      omapi_object_t *value,
				      const char *file, int line)
{
	isc_result_t status;

	status = omapi_value_new (vp, file, line);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_data_string_reference (&(*vp) -> name,
					      name, file, line);
	if (status != ISC_R_SUCCESS) {
		omapi_value_dereference (vp, file, line);
		return status;
	}
	if (value) {
		status = omapi_typed_data_new (file, line, &(*vp) -> value,
					       omapi_datatype_int);
		if (status != ISC_R_SUCCESS) {
			omapi_value_dereference (vp, file, line);
			return status;
		}
		status = (omapi_object_handle
			  ((omapi_handle_t *)&(*vp) -> value -> u.integer,
			   value));
		if (status != ISC_R_SUCCESS) {
			omapi_value_dereference (vp, file, line);
			return status;
		}
	}
	return ISC_R_SUCCESS;
}

isc_result_t omapi_make_string_value (omapi_value_t **vp,
				      omapi_data_string_t *name,
				      const char *value,
				      const char *file, int line)
{
	isc_result_t status;

	status = omapi_value_new (vp, file, line);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_data_string_reference (&(*vp) -> name,
					      name, file, line);
	if (status != ISC_R_SUCCESS) {
		omapi_value_dereference (vp, file, line);
		return status;
	}
	if (value) {
		status = omapi_typed_data_new (file, line, &(*vp) -> value,
					       omapi_datatype_string, value);
		if (status != ISC_R_SUCCESS) {
			omapi_value_dereference (vp, file, line);
			return status;
		}
	}
	return ISC_R_SUCCESS;
}

isc_result_t omapi_get_int_value (unsigned long *v, omapi_typed_data_t *t)
{
	u_int32_t rv;

	if (t -> type == omapi_datatype_int) {
		*v = t -> u.integer;
		return ISC_R_SUCCESS;
	} else if (t -> type == omapi_datatype_string ||
		   t -> type == omapi_datatype_data) {
		if (t -> u.buffer.len != sizeof (rv))
			return ISC_R_INVALIDARG;
		memcpy (&rv, t -> u.buffer.value, sizeof rv);
		*v = ntohl (rv);
		return ISC_R_SUCCESS;
	}
	return ISC_R_INVALIDARG;
}
