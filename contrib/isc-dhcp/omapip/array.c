/* listener.c

   Subroutines that support the omapi extensible array type. */

/*
 * Copyright (c) 2001 Internet Software Consortium.
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

/* Allocate a new extensible array. */

isc_result_t omapi_array_allocate (omapi_array_t **array,
				   omapi_array_ref_t ref,
				   omapi_array_deref_t deref,
				   const char *file, int line)
{
	isc_result_t status;
	omapi_array_t *aptr;

	if (!array || *array)
		return ISC_R_INVALIDARG;
	aptr = dmalloc (sizeof (omapi_array_t),file, line);
	if (!aptr)
		return ISC_R_NOMEMORY;
	*array = aptr;
	aptr -> ref = ref;
	aptr -> deref = deref;
	return ISC_R_SUCCESS;
}

isc_result_t omapi_array_free (omapi_array_t **array,
			       const char *file, int line)
{
	isc_result_t status;
	omapi_array_t *aptr;
	int i;

	if (!array || !*array)
		return ISC_R_INVALIDARG;
	aptr = *array;
	for (i = 0; i < aptr -> count; i++)
		if (aptr -> data [i] && aptr -> deref)
			(*aptr -> deref) (&aptr -> data [i], file, line);
	dfree (aptr -> data, MDL);
	dfree (aptr, MDL);
	*array = (omapi_array_t *)0;
	return ISC_R_SUCCESS;
}

/* Extend the size of the array by one entry (we may allocate more than that)
   and store the specified value in the new array element. */

isc_result_t omapi_array_extend (omapi_array_t *array, char *ptr,
				 int *index, const char *file, int line)
{
	isc_result_t status;
	int new = array -> count;
	status = omapi_array_set (array, ptr, new, file, line);
	if (index && status == ISC_R_SUCCESS)
		*index = new;
	return status;
}

/* Set a value in the specified array, extending it if necessary. */

isc_result_t omapi_array_set (omapi_array_t *array, void *ptr, int index,
			      const char *file, int line)
{
	char **newbuf;
	int delta;
	isc_result_t status;

	if (!array)
		return ISC_R_INVALIDARG;
	if (!ptr)
		return ISC_R_INVALIDARG;
	if (index < 0)
		return ISC_R_INVALIDARG;

	/* If the proposed index is larger than the current available
	   space in the array, make more space in the array. */
	if (array -> max <= index) {
		delta = index - array -> max + 10;
		newbuf = dmalloc ((array -> max + delta) * sizeof (char *),
				  file, line);
		if (!newbuf)
			return ISC_R_NOMEMORY;
		/* Zero the new elements. */
		memset (&newbuf [array -> max], 0, (sizeof (char *)) * delta);
		array -> max += delta;
		/* Copy the old array data into the new buffer. */
		if (array -> data) {
		    memcpy (newbuf,
			    array -> data, array -> count * sizeof (char *));
		    dfree (array -> data, file, line);
		}
		array -> data = newbuf;
	} else {
		/* If there's already data there, and this is an array
		   of references, dereference what's there. */
		if (array -> data [index]) {
			status = ((*array -> deref) (&array -> data [index],
						     file, line));
		
			if (status != ISC_R_SUCCESS)
				return status;
		}
	}

	/* Store the pointer using the referencer function.  We have
	   either just memset this to zero or dereferenced what was
	   there previously, so there is no need to do anything if the
	   pointer we have been asked to store is null. */
	if (ptr) {
		status = (*array -> ref) (&array -> data [index], ptr,
					  file, line);
		if (status != ISC_R_SUCCESS)
			return status;
	}
	if (index >= array -> count)
		array -> count = index + 1;
	return ISC_R_SUCCESS;
}

isc_result_t omapi_array_lookup (char **ptr, omapi_array_t *array, int index,
				 const char *file, int line)
{
	if (!array || !ptr || *ptr || index < 0 || index >= array -> count)
		return ISC_R_INVALIDARG;
	if (array -> data [index])
		return (*array -> ref) (ptr,
					array -> data [index], file, line);
	return ISC_R_NOTFOUND;
}

OMAPI_ARRAY_TYPE_DECL(omapi_object, omapi_object_t);
