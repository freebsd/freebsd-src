/* auth.c

   Subroutines having to do with authentication. */

/*
 * Copyright (c) 1998-2001 Internet Software Consortium.
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

#ifndef lint
static char ocopyright[] =
"$Id: auth.c,v 1.3.2.3 2001/10/17 03:27:56 mellon Exp $ Copyright 1998-2000 The Internet Software Consortium.";
#endif

#include <omapip/omapip_p.h>

OMAPI_OBJECT_ALLOC (omapi_auth_key, omapi_auth_key_t, omapi_type_auth_key)
typedef struct hash omapi_auth_hash_t;
HASH_FUNCTIONS_DECL (omapi_auth_key, const char *,
		     omapi_auth_key_t, omapi_auth_hash_t)
omapi_auth_hash_t *auth_key_hash;
HASH_FUNCTIONS (omapi_auth_key, const char *, omapi_auth_key_t,
		omapi_auth_hash_t,
		omapi_auth_key_reference, omapi_auth_key_dereference)

isc_result_t omapi_auth_key_new (omapi_auth_key_t **o, const char *file,
				 int line)
{
	return omapi_auth_key_allocate (o, file, line);
}

isc_result_t omapi_auth_key_destroy (omapi_object_t *h,
				     const char *file, int line)
{
	omapi_auth_key_t *a;

	if (h -> type != omapi_type_auth_key)
		return ISC_R_INVALIDARG;
	a = (omapi_auth_key_t *)h;

	if (auth_key_hash)
		omapi_auth_key_hash_delete (auth_key_hash, a -> name, 0, MDL);

	if (a -> name)
		dfree (a -> name, MDL);
	if (a -> algorithm)
		dfree (a -> algorithm, MDL);
	if (a -> key)
		omapi_data_string_dereference (&a -> key, MDL);
	
	return ISC_R_SUCCESS;
}

isc_result_t omapi_auth_key_enter (omapi_auth_key_t *a)
{
	omapi_auth_key_t *tk;

	if (a -> type != omapi_type_auth_key)
		return ISC_R_INVALIDARG;

	tk = (omapi_auth_key_t *)0;
	if (auth_key_hash) {
		omapi_auth_key_hash_lookup (&tk, auth_key_hash,
					    a -> name, 0, MDL);
		if (tk == a) {
			omapi_auth_key_dereference (&tk, MDL);
			return ISC_R_SUCCESS;
		}
		if (tk) {
			omapi_auth_key_hash_delete (auth_key_hash,
						    tk -> name, 0, MDL);
			omapi_auth_key_dereference (&tk, MDL);
		}
	} else {
		if (!omapi_auth_key_new_hash (&auth_key_hash, 1, MDL))
			return ISC_R_NOMEMORY;
	}
	omapi_auth_key_hash_add (auth_key_hash, a -> name, 0, a, MDL);
	return ISC_R_SUCCESS;
	
}

isc_result_t omapi_auth_key_lookup_name (omapi_auth_key_t **a,
					 const char *name)
{
	if (!auth_key_hash)
		return ISC_R_NOTFOUND;
	if (!omapi_auth_key_hash_lookup (a, auth_key_hash, name, 0, MDL))
		return ISC_R_NOTFOUND;
	return ISC_R_SUCCESS;
}

isc_result_t omapi_auth_key_lookup (omapi_object_t **h,
				    omapi_object_t *id,
				    omapi_object_t *ref)
{
	isc_result_t status;
	omapi_value_t *name = (omapi_value_t *)0;
	omapi_value_t *algorithm = (omapi_value_t *)0;

	if (!auth_key_hash)
		return ISC_R_NOTFOUND;

	if (!ref)
		return ISC_R_NOKEYS;

	status = omapi_get_value_str (ref, id, "name", &name);
	if (status != ISC_R_SUCCESS)
		return status;

	if ((name -> value -> type != omapi_datatype_string) &&
	    (name -> value -> type != omapi_datatype_data)) {
		omapi_value_dereference (&name, MDL);
		return ISC_R_NOTFOUND;
	}

	status = omapi_get_value_str (ref, id, "algorithm", &algorithm);
	if (status != ISC_R_SUCCESS) {
		omapi_value_dereference (&name, MDL);
		return status;
	}

	if ((algorithm -> value -> type != omapi_datatype_string) &&
	    (algorithm -> value -> type != omapi_datatype_data)) {
		omapi_value_dereference (&name, MDL);
		omapi_value_dereference (&algorithm, MDL);
		return ISC_R_NOTFOUND;
	}


	if (!omapi_auth_key_hash_lookup ((omapi_auth_key_t **)h, auth_key_hash,
					 (const char *)
					 name -> value -> u.buffer.value,
					 name -> value -> u.buffer.len, MDL)) {
		omapi_value_dereference (&name, MDL);
		omapi_value_dereference (&algorithm, MDL);
		return ISC_R_NOTFOUND;
	}

	if (omapi_td_strcasecmp (algorithm -> value,
				 ((omapi_auth_key_t *)*h) -> algorithm) != 0) {
		omapi_value_dereference (&name, MDL);
		omapi_value_dereference (&algorithm, MDL);
		omapi_object_dereference (h, MDL);
		return ISC_R_NOTFOUND;
	}

	omapi_value_dereference (&name, MDL);
	omapi_value_dereference (&algorithm, MDL);

	return ISC_R_SUCCESS;
}

isc_result_t omapi_auth_key_stuff_values (omapi_object_t *c,
					  omapi_object_t *id,
					  omapi_object_t *h)
{
	omapi_auth_key_t *a;
	isc_result_t status;

	if (h -> type != omapi_type_auth_key)
		return ISC_R_INVALIDARG;
	a = (omapi_auth_key_t *)h;

	/* Write only the name and algorithm -- not the secret! */
	if (a -> name) {
		status = omapi_connection_put_name (c, "name");
		if (status != ISC_R_SUCCESS)
			return status;
		status = omapi_connection_put_string (c, a -> name);
		if (status != ISC_R_SUCCESS)
			return status;
	}
	if (a -> algorithm) {
		status = omapi_connection_put_name (c, "algorithm");
		if (status != ISC_R_SUCCESS)
			return status;
		status = omapi_connection_put_string (c, a -> algorithm);
		if (status != ISC_R_SUCCESS)
			return status;
	}

	return ISC_R_SUCCESS;
}

isc_result_t omapi_auth_key_get_value (omapi_object_t *h,
				       omapi_object_t *id,
				       omapi_data_string_t *name,
				       omapi_value_t **value)
{
	omapi_auth_key_t *a;
	isc_result_t status;

	if (h -> type != omapi_type_auth_key)
		return ISC_R_UNEXPECTED;
	a = (omapi_auth_key_t *)h;

	if (omapi_ds_strcmp (name, "name") == 0) {
		if (a -> name)
			return omapi_make_string_value
				(value, name, a -> name, MDL);
		else
			return ISC_R_NOTFOUND;
	} else if (omapi_ds_strcmp (name, "key") == 0) {
		if (a -> key) {
			status = omapi_value_new (value, MDL);
			if (status != ISC_R_SUCCESS)
				return status;

			status = omapi_data_string_reference
				(&(*value) -> name, name, MDL);
			if (status != ISC_R_SUCCESS) {
				omapi_value_dereference (value, MDL);
				return status;
			}

			status = omapi_typed_data_new (MDL, &(*value) -> value,
						       omapi_datatype_data,
						       a -> key -> len);
			if (status != ISC_R_SUCCESS) {
				omapi_value_dereference (value, MDL);
				return status;
			}

			memcpy ((*value) -> value -> u.buffer.value,
				a -> key -> value, a -> key -> len);
			return ISC_R_SUCCESS;
		} else
			return ISC_R_NOTFOUND;
	} else if (omapi_ds_strcmp (name, "algorithm") == 0) {
		if (a -> algorithm)
			return omapi_make_string_value
				(value, name, a -> algorithm, MDL);
		else
			return ISC_R_NOTFOUND;
	}

	return ISC_R_SUCCESS;
}
