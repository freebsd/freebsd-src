/* callback.c

   The dhcpctl callback object. */

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

#ifndef lint
static char copyright[] =
"$Id: callback.c,v 1.5.2.2 2004/06/10 17:59:23 dhankins Exp $ Copyright (c) 2004 Internet Systems Consortium.  All rights reserved.\n";
#endif /* not lint */

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

