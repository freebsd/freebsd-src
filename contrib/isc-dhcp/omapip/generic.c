/* generic.c

   Subroutines that support the generic object. */

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

OMAPI_OBJECT_ALLOC (omapi_generic,
		    omapi_generic_object_t, omapi_type_generic)

isc_result_t omapi_generic_new (omapi_object_t **gen,
				const char *file, int line)
{
	/* Backwards compatibility. */
	return omapi_generic_allocate ((omapi_generic_object_t **)gen,
				       file, line);
}

isc_result_t omapi_generic_set_value (omapi_object_t *h,
				      omapi_object_t *id,
				      omapi_data_string_t *name,
				      omapi_typed_data_t *value)
{
	omapi_generic_object_t *g;
	omapi_value_t *new;
	omapi_value_t **va;
	u_int8_t *ca;
	int vm_new;
	int i, vfree = -1;
	isc_result_t status;

	if (h -> type != omapi_type_generic)
		return ISC_R_INVALIDARG;
	g = (omapi_generic_object_t *)h;

	/* See if there's already a value with this name attached to
	   the generic object, and if so, replace the current value
	   with the new one. */
	for (i = 0; i < g -> nvalues; i++) {
		if (!omapi_data_string_cmp (name, g -> values [i] -> name)) {
			/* There's an inconsistency here: the standard
			   behaviour of a set_values method when
			   passed a matching name and a null value is
			   to delete the value associated with that
			   name (where possible).  In the generic
			   object, we remember the name/null pair,
			   because generic objects are generally used
			   to pass messages around, and this is the
			   way that remote entities delete values from
			   local objects.  If the get_value method of
			   a generic object is called for a name that
			   maps to a name/null pair, ISC_R_NOTFOUND is
			   returned. */
			new = (omapi_value_t *)0;
			status = (omapi_value_new (&new, MDL));
			if (status != ISC_R_SUCCESS)
				return status;
			omapi_data_string_reference (&new -> name, name, MDL);
			if (value)
				omapi_typed_data_reference (&new -> value,
							    value, MDL);

			omapi_value_dereference (&(g -> values [i]), MDL);
			status = (omapi_value_reference
				  (&(g -> values [i]), new, MDL));
			omapi_value_dereference (&new, MDL);
			g -> changed [i] = 1;
			return status;
		}
		/* Notice a free slot if we pass one. */
		else if (vfree == -1 && !g -> values [i])
			vfree = i;
	}			

	/* If the name isn't already attached to this object, see if an
	   inner object has it. */
	if (h -> inner && h -> inner -> type -> set_value) {
		status = ((*(h -> inner -> type -> set_value))
			  (h -> inner, id, name, value));
		if (status != ISC_R_NOTFOUND)
			return status;
	}

	/* Okay, so it's a value that no inner object knows about, and
	   (implicitly, since the outer object set_value method would
	   have called this object's set_value method) it's an object that
	   no outer object knows about, it's this object's responsibility
	   to remember it - that's what generic objects do. */

	/* Arrange for there to be space for the pointer to the new
           name/value pair if necessary: */
	if (vfree == -1) {
		vfree = g -> nvalues;
		if (vfree == g -> va_max) {
			if (g -> va_max)
				vm_new = 2 * g -> va_max;
			else
				vm_new = 10;
			va = dmalloc (vm_new * sizeof *va, MDL);
			if (!va)
				return ISC_R_NOMEMORY;
			ca = dmalloc (vm_new * sizeof *ca, MDL);
			if (!ca) {
				dfree (va, MDL);
				return ISC_R_NOMEMORY;
			}
			if (g -> va_max) {
				memcpy (va, g -> values,
					g -> va_max * sizeof *va);
				memcpy (ca, g -> changed,
					g -> va_max * sizeof *ca);
			}
			memset (va + g -> va_max, 0,
				(vm_new - g -> va_max) * sizeof *va);
			memset (ca + g -> va_max, 0,
				(vm_new - g -> va_max) * sizeof *ca);
			if (g -> values)
				dfree (g -> values, MDL);
			if (g -> changed)
				dfree (g -> changed, MDL);
			g -> values = va;
			g -> changed = ca;
			g -> va_max = vm_new;
		}
	}
	status = omapi_value_new (&g -> values [vfree], MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	omapi_data_string_reference (&g -> values [vfree] -> name,
				     name, MDL);
	if (value)
		omapi_typed_data_reference
			(&g -> values [vfree] -> value, value, MDL);
	g -> changed [vfree] = 1;
	if (vfree == g -> nvalues)
		g -> nvalues++;
	return ISC_R_SUCCESS;
}

isc_result_t omapi_generic_get_value (omapi_object_t *h,
				      omapi_object_t *id,
				      omapi_data_string_t *name,
				      omapi_value_t **value)
{
	int i;
	omapi_generic_object_t *g;

	if (h -> type != omapi_type_generic)
		return ISC_R_INVALIDARG;
	g = (omapi_generic_object_t *)h;
	
	/* Look up the specified name in our list of objects. */
	for (i = 0; i < g -> nvalues; i++) {
		if (!omapi_data_string_cmp (name, g -> values [i] -> name)) {
			/* If this is a name/null value pair, this is the
			   same as if there were no value that matched
			   the specified name, so return ISC_R_NOTFOUND. */
			if (!g -> values [i] -> value)
				return ISC_R_NOTFOUND;
			/* Otherwise, return the name/value pair. */
			return omapi_value_reference (value,
						      g -> values [i], MDL);
		}
	}			

	if (h -> inner && h -> inner -> type -> get_value)
		return (*(h -> inner -> type -> get_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t omapi_generic_destroy (omapi_object_t *h,
				    const char *file, int line)
{
	omapi_generic_object_t *g;
	int i;

	if (h -> type != omapi_type_generic)
		return ISC_R_UNEXPECTED;
	g = (omapi_generic_object_t *)h;
	
	if (g -> values) {
		for (i = 0; i < g -> nvalues; i++) {
			if (g -> values [i])
				omapi_value_dereference (&g -> values [i],
							 file, line);
		}
		dfree (g -> values, file, line);
		dfree (g -> changed, file, line);
		g -> values = (omapi_value_t **)0;
		g -> changed = (u_int8_t *)0;
		g -> va_max = 0;
	}

	return ISC_R_SUCCESS;
}

isc_result_t omapi_generic_signal_handler (omapi_object_t *h,
					   const char *name, va_list ap)
{
	if (h -> type != omapi_type_generic)
		return ISC_R_INVALIDARG;
	
	if (h -> inner && h -> inner -> type -> signal_handler)
		return (*(h -> inner -> type -> signal_handler)) (h -> inner,
								  name, ap);
	return ISC_R_NOTFOUND;
}

/* Write all the published values associated with the object through the
   specified connection. */

isc_result_t omapi_generic_stuff_values (omapi_object_t *c,
					 omapi_object_t *id,
					 omapi_object_t *g)
{
	omapi_generic_object_t *src;
	int i;
	isc_result_t status;

	if (g -> type != omapi_type_generic)
		return ISC_R_INVALIDARG;
	src = (omapi_generic_object_t *)g;
	
	for (i = 0; i < src -> nvalues; i++) {
		if (src -> values [i] && src -> values [i] -> name -> len &&
		    src -> changed [i]) {
			status = (omapi_connection_put_uint16
				  (c, src -> values [i] -> name -> len));
			if (status != ISC_R_SUCCESS)
				return status;
			status = (omapi_connection_copyin
				  (c, src -> values [i] -> name -> value,
				   src -> values [i] -> name -> len));
			if (status != ISC_R_SUCCESS)
				return status;

			status = (omapi_connection_write_typed_data
				  (c, src -> values [i] -> value));
			if (status != ISC_R_SUCCESS)
				return status;
		}
	}			

	if (g -> inner && g -> inner -> type -> stuff_values)
		return (*(g -> inner -> type -> stuff_values)) (c, id,
								g -> inner);
	return ISC_R_SUCCESS;
}

/* Clear the changed flags on the object.   This has the effect that if
   generic_stuff is called, any attributes that still have a cleared changed
   flag aren't sent to the peer.   This also deletes any values that are
   null, presuming that these have now been properly handled. */

isc_result_t omapi_generic_clear_flags (omapi_object_t *o)
{
	int i;
	isc_result_t status;
	omapi_generic_object_t *g;

	if (o -> type != omapi_type_generic)
		return ISC_R_INVALIDARG;
	g = (omapi_generic_object_t *)o;

	for (i = 0; i < g -> nvalues; i++) {
		g -> changed [i] = 0;
		if (g -> values [i] &&
		    !g -> values [i] -> value)
			omapi_value_dereference (&g -> values [i], MDL);
	}
	return ISC_R_SUCCESS;
}
