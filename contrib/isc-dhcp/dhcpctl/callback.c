/* callback.c

   The dhcpctl callback object. */

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

/* dhcpctl_set_callback

   synchronous, with asynchronous aftereffect
   handle is some object upon which some kind of process has been
   started - e.g., an open, an update or a refresh.
   data is an anonymous pointer containing some information that
   the callback will use to figure out what event completed.
   return value of 0 means callback was successfully set, a nonzero
   status code is returned otherwise.
   Upon completion of whatever task is in process, the callback
   will be passed the handle to the object, a status code
   indicating what happened, and the anonymous pointer passed to  */

dhcpctl_status dhcpctl_set_callback (dhcpctl_handle h, void *data,
				     void (*func) (dhcpctl_handle,
						   dhcpctl_status, void *))
{
	dhcpctl_callback_object_t *callback;
	omapi_object_t *inner;
	isc_result_t status;

	callback = dmalloc (sizeof *callback, MDL);
	if (!callback)
		return ISC_R_NOMEMORY;

	/* Tie the callback object to the innermost object in the chain. */
	for (inner = h; inner -> inner; inner = inner -> inner)
		;
	omapi_object_reference (&inner -> inner,
				(omapi_object_t *)callback, MDL);
	omapi_object_reference ((omapi_object_t **)&callback -> outer,
				inner, MDL);

	/* Save the actual handle pointer we were passed for the callback. */
	omapi_object_reference (&callback -> object, h, MDL);
	callback -> data = data;
	callback -> callback = func;
	
	return ISC_R_SUCCESS;
}

/* Callback methods (not meant to be called directly) */

isc_result_t dhcpctl_callback_set_value (omapi_object_t *h,
					 omapi_object_t *id,
					 omapi_data_string_t *name,
					 omapi_typed_data_t *value)
{
	if (h -> type != dhcpctl_callback_type)
		return ISC_R_INVALIDARG;

	if (h -> inner && h -> inner -> type -> set_value)
		return (*(h -> inner -> type -> set_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t dhcpctl_callback_get_value (omapi_object_t *h,
					 omapi_object_t *id,
					 omapi_data_string_t *name,
					 omapi_value_t **value)
{
	if (h -> type != dhcpctl_callback_type)
		return ISC_R_INVALIDARG;
	
	if (h -> inner && h -> inner -> type -> get_value)
		return (*(h -> inner -> type -> get_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t dhcpctl_callback_signal_handler (omapi_object_t *o,
					      const char *name, va_list ap)
{
	dhcpctl_callback_object_t *p;
	isc_result_t waitstatus;

	if (o -> type != dhcpctl_callback_type)
		return ISC_R_INVALIDARG;
	p = (dhcpctl_callback_object_t *)o;

	/* Not a signal we recognize? */
	if (strcmp (name, "ready")) {
		if (p -> inner && p -> inner -> type -> signal_handler)
			return (*(p -> inner -> type -> signal_handler))
				(p -> inner, name, ap);
		return ISC_R_NOTFOUND;
	}

	if (p -> object -> type == dhcpctl_remote_type) {
		waitstatus = (((dhcpctl_remote_object_t *)
			       (p -> object)) -> waitstatus);
	} else
		waitstatus = ISC_R_SUCCESS;

	/* Do the callback. */
	if (p -> callback)
		(*(p -> callback)) (p -> object, waitstatus, p -> data);

	return ISC_R_SUCCESS;
}

isc_result_t dhcpctl_callback_destroy (omapi_object_t *h,
				       const char *file, int line)
{
	dhcpctl_callback_object_t *p;
	if (h -> type != dhcpctl_callback_type)
		return ISC_R_INVALIDARG;
	p = (dhcpctl_callback_object_t *)h;
	if (p -> handle)
		omapi_object_dereference ((omapi_object_t **)&p -> handle,
					  file, line);
	return ISC_R_SUCCESS;
}

/* Write all the published values associated with the object through the
   specified connection. */

isc_result_t dhcpctl_callback_stuff_values (omapi_object_t *c,
					    omapi_object_t *id,
					    omapi_object_t *p)
{
	int i;

	if (p -> type != dhcpctl_callback_type)
		return ISC_R_INVALIDARG;

	if (p -> inner && p -> inner -> type -> stuff_values)
		return (*(p -> inner -> type -> stuff_values)) (c, id,
								p -> inner);
	return ISC_R_SUCCESS;
}

