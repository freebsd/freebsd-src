/* remote.c

   The dhcpctl remote object. */

/*
 * Copyright (c) 1999-2003 Internet Software Consortium.
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

/* dhcpctl_new_authenticator

   synchronous - creates an authenticator object.
   returns nonzero status code if the object couldn't be created
   stores handle to authenticator through h if successful, and returns zero.
   name is the authenticator name (NUL-terminated string).
   algorithm is the NUL-terminated string name of the algorithm to use
   (currently, only "hmac-md5" is supported).
   secret and secret_len is the key secret. */

dhcpctl_status dhcpctl_new_authenticator (dhcpctl_handle *h,
					  const char *name,
					  const char *algorithm,
					  const unsigned char *secret,
					  unsigned secret_len)
{
	struct auth_key *key = (struct auth_key *)0;
	isc_result_t status;

	status = omapi_auth_key_new (&key, MDL);
	if (status != ISC_R_SUCCESS)
		return status;

	key -> name = dmalloc (strlen (name) + 1, MDL);
	if (!key -> name) {
		omapi_auth_key_dereference (&key, MDL);
		return ISC_R_NOMEMORY;
	}
	strcpy (key -> name, name);

	/* If the algorithm name isn't an FQDN, tack on the
	   .SIG-ALG.REG.NET. domain. */
	if (strchr (algorithm, '.') == 0) {
		static char add[] = ".SIG-ALG.REG.INT.";
		key -> algorithm = dmalloc (strlen (algorithm) +
		                            sizeof (add), MDL);
		if (!key -> algorithm) {
			omapi_auth_key_dereference (&key, MDL);
			return ISC_R_NOMEMORY;
		}
		strcpy (key -> algorithm, algorithm);
		strcat (key -> algorithm, add);
	} else {
		key -> algorithm = dmalloc (strlen (algorithm) + 1, MDL);
		if (!key -> algorithm) {
			omapi_auth_key_dereference (&key, MDL);
			return ISC_R_NOMEMORY;
		}
		strcpy (key -> algorithm, algorithm);
	}

	status = omapi_data_string_new (&key -> key, secret_len, MDL);
	if (status != ISC_R_SUCCESS) {
		omapi_auth_key_dereference (&key, MDL);
		return status;
	}
	memcpy (key -> key -> value, secret, secret_len);
	key -> key -> len = secret_len;

	*h = (dhcpctl_handle) key;
	return ISC_R_SUCCESS;
}


/* dhcpctl_new_object

   synchronous - creates a local handle for a host entry.
   returns nonzero status code if the local host entry couldn't
   be created
   stores handle to host through h if successful, and returns zero.
   object_type is a pointer to a NUL-terminated string containing
   the ascii name of the type of object being accessed - e.g., "host" */

dhcpctl_status dhcpctl_new_object (dhcpctl_handle *h,
				   dhcpctl_handle connection,
				   const char *object_type)
{
	dhcpctl_remote_object_t *m;
	omapi_object_t *g;
	isc_result_t status;

	m = (dhcpctl_remote_object_t *)0;
	status = omapi_object_allocate ((omapi_object_t **)&m,
					dhcpctl_remote_type, 0, MDL);
	if (status != ISC_R_SUCCESS)
		return status;

	g = (omapi_object_t *)0;
	status = omapi_generic_new (&g, MDL);
	if (status != ISC_R_SUCCESS) {
		dfree (m, MDL);
		return status;
	}
	status = omapi_object_reference (&m -> inner, g, MDL);
	if (status != ISC_R_SUCCESS) {
		omapi_object_dereference ((omapi_object_t **)&m, MDL);
		omapi_object_dereference (&g, MDL);
		return status;
	}
	status = omapi_object_reference (&g -> outer,
					 (omapi_object_t *)m, MDL);

	if (status != ISC_R_SUCCESS) {
		omapi_object_dereference ((omapi_object_t **)&m, MDL);
		omapi_object_dereference (&g, MDL);
		return status;
	}

	status = omapi_typed_data_new (MDL, &m -> rtype,
				       omapi_datatype_string,
				       object_type);
	if (status != ISC_R_SUCCESS) {
		omapi_object_dereference ((omapi_object_t **)&m, MDL);
		omapi_object_dereference (&g, MDL);
		return status;
	}

	status = omapi_object_reference (h, (omapi_object_t *)m, MDL);
	omapi_object_dereference ((omapi_object_t **)&m, MDL);
	omapi_object_dereference (&g, MDL);
	if (status != ISC_R_SUCCESS)
		return status;

	return status;
}

/* asynchronous - just queues the request
   returns nonzero status code if open couldn't be queued
   returns zero if open was queued
   h is a handle to an object created by dhcpctl_new_object
   connection is a connection to a DHCP server
   flags include:
     DHCPCTL_CREATE - if the object doesn't exist, create it
     DHCPCTL_UPDATE - update the object on the server using the
     		      attached parameters 
     DHCPCTL_EXCL - error if the object exists and DHCPCTL_CREATE
     		      was also specified */

dhcpctl_status dhcpctl_open_object (dhcpctl_handle h,
				    dhcpctl_handle connection,
				    int flags)
{
	isc_result_t status;
	omapi_object_t *message = (omapi_object_t *)0;
	dhcpctl_remote_object_t *remote;

	if (h -> type != dhcpctl_remote_type)
		return ISC_R_INVALIDARG;
	remote = (dhcpctl_remote_object_t *)h;

	status = omapi_message_new (&message, MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_set_int_value (message, (omapi_object_t *)0,
				      "op", OMAPI_OP_OPEN);
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
	if (flags & DHCPCTL_CREATE) {
		status = omapi_set_boolean_value (message, (omapi_object_t *)0,
						  "create", 1);
		if (status != ISC_R_SUCCESS) {
			omapi_object_dereference (&message, MDL);
			return status;
		}
	}
	if (flags & DHCPCTL_UPDATE) {
		status = omapi_set_boolean_value (message, (omapi_object_t *)0,
						  "update", 1);
		if (status != ISC_R_SUCCESS) {
			omapi_object_dereference (&message, MDL);
			return status;
		}
	}
	if (flags & DHCPCTL_EXCL) {
		status = omapi_set_boolean_value (message, (omapi_object_t *)0,
						  "exclusive", 1);
		if (status != ISC_R_SUCCESS) {
			omapi_object_dereference (&message, MDL);
			return status;
		}
	}

	if (remote -> rtype) {
		status = omapi_set_value_str (message, (omapi_object_t *)0,
					      "type", remote -> rtype);
		if (status != ISC_R_SUCCESS) {
			omapi_object_dereference (&message, MDL);
			return status;
		}
	}

	status = omapi_message_register (message);
	if (status != ISC_R_SUCCESS) {
		omapi_object_dereference (&message, MDL);
		return status;
	}

	status = omapi_protocol_send_message (connection -> outer,
					    (omapi_object_t *)0,
					    message, (omapi_object_t *)0);

	if (status != ISC_R_SUCCESS)
		omapi_message_unregister (message);

	omapi_object_dereference (&message, MDL);
	return status;
}

/* Callback methods (not meant to be called directly) */

isc_result_t dhcpctl_remote_set_value (omapi_object_t *h,
				       omapi_object_t *id,
				       omapi_data_string_t *name,
				       omapi_typed_data_t *value)
{
	dhcpctl_remote_object_t *ro;
	unsigned long rh;
	isc_result_t status;

	if (h -> type != dhcpctl_remote_type)
		return ISC_R_INVALIDARG;
	ro = (dhcpctl_remote_object_t *)h;

	if (!omapi_ds_strcmp (name, "remote-handle")) {
		status = omapi_get_int_value (&rh, value);
		if (status == ISC_R_SUCCESS)
			ro -> remote_handle = rh;
		return status;
	}

	if (h -> inner && h -> inner -> type -> set_value)
		return (*(h -> inner -> type -> set_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t dhcpctl_remote_get_value (omapi_object_t *h,
				       omapi_object_t *id,
				       omapi_data_string_t *name,
				       omapi_value_t **value)
{
	if (h -> type != dhcpctl_remote_type)
		return ISC_R_INVALIDARG;
	
	if (h -> inner && h -> inner -> type -> get_value)
		return (*(h -> inner -> type -> get_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t dhcpctl_remote_signal_handler (omapi_object_t *o,
					    const char *name, va_list ap)
{
	dhcpctl_remote_object_t *p;
	omapi_typed_data_t *tv;

	if (o -> type != dhcpctl_remote_type)
		return ISC_R_INVALIDARG;
	p = (dhcpctl_remote_object_t *)o;

	if (!strcmp (name, "updated")) {
		p -> waitstatus = ISC_R_SUCCESS;
		if (o -> inner -> type == omapi_type_generic)
			omapi_generic_clear_flags (o -> inner);
		return omapi_signal_in (o -> inner, "ready");
	}
	if (!strcmp (name, "status")) {
		p -> waitstatus = va_arg (ap, isc_result_t);
		if (p -> message)
			omapi_typed_data_dereference (&p -> message, MDL);
		tv = va_arg (ap, omapi_typed_data_t *);
		if (tv)
			omapi_typed_data_reference (&p -> message, tv, MDL);
		return omapi_signal_in (o -> inner, "ready");
	}

	if (p -> inner && p -> inner -> type -> signal_handler)
		return (*(p -> inner -> type -> signal_handler))
			(p -> inner, name, ap);

	return ISC_R_SUCCESS;
}

isc_result_t dhcpctl_remote_destroy (omapi_object_t *h,
				     const char *file, int line)
{
	dhcpctl_remote_object_t *p;
	if (h -> type != dhcpctl_remote_type)
		return ISC_R_INVALIDARG;
	p = (dhcpctl_remote_object_t *)h;
	if (p -> handle)
		omapi_object_dereference ((omapi_object_t **)&p -> handle,
					  file, line);
	if (p -> rtype)
		omapi_typed_data_dereference ((omapi_typed_data_t **)&p->rtype,
					      file, line);
	return ISC_R_SUCCESS;
}

/* Write all the published values associated with the object through the
   specified connection. */

isc_result_t dhcpctl_remote_stuff_values (omapi_object_t *c,
					  omapi_object_t *id,
					  omapi_object_t *p)
{
	int i;

	if (p -> type != dhcpctl_remote_type)
		return ISC_R_INVALIDARG;

	if (p -> inner && p -> inner -> type -> stuff_values)
		return (*(p -> inner -> type -> stuff_values)) (c, id,
								p -> inner);
	return ISC_R_SUCCESS;
}

