/* $Id: dhcpctl.h,v 1.13.2.4 2004/06/10 17:59:24 dhankins Exp $

   Subroutines providing general support for objects. */

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

#ifndef _DHCPCTL_H_
#define _DHCPCTL_H_

#include <omapip/omapip.h>

typedef isc_result_t dhcpctl_status;
typedef omapi_object_t *dhcpctl_handle;
typedef omapi_data_string_t *dhcpctl_data_string;

#define dhcpctl_null_handle ((dhcpctl_handle) 0)

#define DHCPCTL_CREATE		OMAPI_CREATE
#define DHCPCTL_UPDATE		OMAPI_UPDATE
#define DHCPCTL_EXCL		OMAPI_EXCL

typedef struct {
	OMAPI_OBJECT_PREAMBLE;
	omapi_object_t *object;
	void *data;
	void (*callback) (dhcpctl_handle, dhcpctl_status, void *);
} dhcpctl_callback_object_t;

typedef struct {
	OMAPI_OBJECT_PREAMBLE;
	omapi_typed_data_t *rtype;
	isc_result_t waitstatus;
	omapi_typed_data_t *message;
	omapi_handle_t remote_handle;
} dhcpctl_remote_object_t;

extern omapi_object_type_t *dhcpctl_callback_type;
extern omapi_object_type_t *dhcpctl_remote_type;

dhcpctl_status dhcpctl_initialize (void);
dhcpctl_status dhcpctl_connect (dhcpctl_handle *,
				const char *, int, dhcpctl_handle);
dhcpctl_status dhcpctl_wait_for_completion (dhcpctl_handle, dhcpctl_status *);
dhcpctl_status dhcpctl_get_value (dhcpctl_data_string *,
				  dhcpctl_handle, const char *);
dhcpctl_status dhcpctl_get_boolean (int *, dhcpctl_handle, const char *);
dhcpctl_status dhcpctl_set_value (dhcpctl_handle,
				  dhcpctl_data_string, const char *);
dhcpctl_status dhcpctl_set_string_value (dhcpctl_handle, const char *,
					 const char *);
dhcpctl_status dhcpctl_set_data_value (dhcpctl_handle,
				       const char *, unsigned, const char *);
dhcpctl_status dhcpctl_set_null_value (dhcpctl_handle, const char *);
dhcpctl_status dhcpctl_set_boolean_value (dhcpctl_handle, int, const char *);
dhcpctl_status dhcpctl_set_int_value (dhcpctl_handle, int, const char *);
dhcpctl_status dhcpctl_object_update (dhcpctl_handle, dhcpctl_handle);
dhcpctl_status dhcpctl_object_refresh (dhcpctl_handle, dhcpctl_handle);
dhcpctl_status dhcpctl_object_remove (dhcpctl_handle, dhcpctl_handle);

dhcpctl_status dhcpctl_set_callback (dhcpctl_handle, void *,
				     void (*) (dhcpctl_handle,
					       dhcpctl_status, void *));
isc_result_t dhcpctl_callback_set_value  (omapi_object_t *, omapi_object_t *,
					  omapi_data_string_t *,
					  omapi_typed_data_t *);
isc_result_t dhcpctl_callback_get_value (omapi_object_t *, omapi_object_t *,
					 omapi_data_string_t *,
					 omapi_value_t **); 
isc_result_t dhcpctl_callback_destroy (omapi_object_t *, const char *, int);
isc_result_t dhcpctl_callback_signal_handler (omapi_object_t *,
					      const char *, va_list);
isc_result_t dhcpctl_callback_stuff_values (omapi_object_t *,
					    omapi_object_t *,
					    omapi_object_t *);

dhcpctl_status dhcpctl_new_authenticator (dhcpctl_handle *,
					  const char *, const char *,
					  const unsigned char *, unsigned);

dhcpctl_status dhcpctl_open_object (dhcpctl_handle, dhcpctl_handle, int);
dhcpctl_status dhcpctl_new_object (dhcpctl_handle *,
				   dhcpctl_handle, const char *);
isc_result_t dhcpctl_remote_set_value  (omapi_object_t *, omapi_object_t *,
					omapi_data_string_t *,
					omapi_typed_data_t *);
isc_result_t dhcpctl_remote_get_value (omapi_object_t *, omapi_object_t *,
				       omapi_data_string_t *,
				       omapi_value_t **); 
isc_result_t dhcpctl_remote_destroy (omapi_object_t *, const char *, int);
isc_result_t dhcpctl_remote_signal_handler (omapi_object_t *,
					    const char *, va_list);
isc_result_t dhcpctl_remote_stuff_values (omapi_object_t *,
					  omapi_object_t *,
					  omapi_object_t *);
isc_result_t dhcpctl_data_string_dereference (dhcpctl_data_string *,
					      const char *, int);
#endif /* _DHCPCTL_H_ */
