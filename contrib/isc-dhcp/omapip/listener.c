/* listener.c

   Subroutines that support the generic listener object. */

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

#if defined (TRACING)
omapi_array_t *trace_listeners;
static void trace_listener_accept_input (trace_type_t *, unsigned, char *);
static void trace_listener_remember (omapi_listener_object_t *,
				     const char *, int);
static void trace_listener_accept_stop (trace_type_t *);
trace_type_t *trace_listener_accept;
#endif

OMAPI_OBJECT_ALLOC (omapi_listener,
		    omapi_listener_object_t, omapi_type_listener)

isc_result_t omapi_listen (omapi_object_t *h,
			   unsigned port,
			   int max)
{
	omapi_addr_t addr;

#ifdef DEBUG_PROTOCOL
	log_debug ("omapi_listen(port=%d, max=%d)", port, max);
#endif

	addr.addrtype = AF_INET;
	addr.addrlen = sizeof (struct in_addr);
	memset (addr.address, 0, sizeof addr.address); /* INADDR_ANY */
	addr.port = port;

	return omapi_listen_addr (h, &addr, max);
}

isc_result_t omapi_listen_addr (omapi_object_t *h,
				omapi_addr_t *addr,
				int max)
{
	struct hostent *he;
	int hix;
	isc_result_t status;
	omapi_listener_object_t *obj;
	int i;
	struct in_addr ia;

	/* Get the handle. */
	obj = (omapi_listener_object_t *)0;
	status = omapi_listener_allocate (&obj, MDL);
	if (status != ISC_R_SUCCESS)
		return status;

	/* Connect this object to the inner object. */
	status = omapi_object_reference (&h -> outer,
					 (omapi_object_t *)obj, MDL);
	if (status != ISC_R_SUCCESS) {
		omapi_listener_dereference (&obj, MDL);
		return status;
	}
	status = omapi_object_reference (&obj -> inner, h, MDL);
	if (status != ISC_R_SUCCESS) {
		omapi_listener_dereference (&obj, MDL);
		return status;
	}

	/* Currently only support TCPv4 addresses. */
	if (addr -> addrtype != AF_INET)
		return ISC_R_INVALIDARG;

	/* Set up the address on which we will listen... */
	obj -> address.sin_port = htons (addr -> port);
	memcpy (&obj -> address.sin_addr,
		addr -> address, sizeof obj -> address.sin_addr);
#if defined (HAVE_SA_LEN)
	obj -> address.sin_len =
		sizeof (struct sockaddr_in);
#endif
	obj -> address.sin_family = AF_INET;
	memset (&(obj -> address.sin_zero), 0,
		sizeof obj -> address.sin_zero);

#if defined (TRACING)
	/* If we're playing back a trace file, we remember the object
	   on the trace listener queue. */
	if (trace_playback ()) {
		trace_listener_remember (obj, MDL);
	}  else {
#endif
		/* Create a socket on which to listen. */
		obj -> socket = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (!obj -> socket) {
			omapi_listener_dereference (&obj, MDL);
			if (errno == EMFILE
			    || errno == ENFILE || errno == ENOBUFS)
				return ISC_R_NORESOURCES;
			return ISC_R_UNEXPECTED;
		}
	
#if defined (HAVE_SETFD)
		if (fcntl (obj -> socket, F_SETFD, 1) < 0) {
			close (obj -> socket);
			omapi_listener_dereference (&obj, MDL);
			return ISC_R_UNEXPECTED;
		}
#endif

		/* Set the REUSEADDR option so that we don't fail to start if
		   we're being restarted. */
		i = 1;
		if (setsockopt (obj -> socket, SOL_SOCKET, SO_REUSEADDR,
				(char *)&i, sizeof i) < 0) {
			close (obj -> socket);
			omapi_listener_dereference (&obj, MDL);
			return ISC_R_UNEXPECTED;
		}
		
		/* Try to bind to the wildcard address using the port number
		   we were given. */
		i = bind (obj -> socket,
			  (struct sockaddr *)&obj -> address,
			  sizeof obj -> address);
		if (i < 0) {
			omapi_listener_dereference (&obj, MDL);
			if (errno == EADDRINUSE)
				return ISC_R_ADDRNOTAVAIL;
			if (errno == EPERM)
				return ISC_R_NOPERM;
			return ISC_R_UNEXPECTED;
		}

		/* Now tell the kernel to listen for connections. */
		if (listen (obj -> socket, max)) {
			omapi_listener_dereference (&obj, MDL);
			return ISC_R_UNEXPECTED;
		}

		if (fcntl (obj -> socket, F_SETFL, O_NONBLOCK) < 0) {
			omapi_listener_dereference (&obj, MDL);
			return ISC_R_UNEXPECTED;
		}

		status = omapi_register_io_object ((omapi_object_t *)obj,
						   omapi_listener_readfd, 0,
						   omapi_accept, 0, 0);
#if defined (TRACING)
	}
#endif
	omapi_listener_dereference (&obj, MDL);
	return status;
}

/* Return the socket on which the dispatcher should wait for readiness
   to read, for a listener object. */
int omapi_listener_readfd (omapi_object_t *h)
{
	omapi_listener_object_t *l;

	if (h -> type != omapi_type_listener)
		return -1;
	l = (omapi_listener_object_t *)h;
	
	return l -> socket;
}

/* Reader callback for a listener object.   Accept an incoming connection. */
isc_result_t omapi_accept (omapi_object_t *h)
{
	isc_result_t status;
	SOCKLEN_T len;
	omapi_connection_object_t *obj;
	omapi_listener_object_t *listener;
	omapi_addr_t remote_addr;
	int i;
	struct sockaddr_in addr;
	int socket;

	if (h -> type != omapi_type_listener)
		return ISC_R_INVALIDARG;
	listener = (omapi_listener_object_t *)h;

	/* Accept the connection. */
	len = sizeof addr;
	socket = accept (listener -> socket,
			 ((struct sockaddr *)&(addr)), &len);
	if (socket < 0) {
		if (errno == EMFILE || errno == ENFILE || errno == ENOBUFS)
			return ISC_R_NORESOURCES;
		return ISC_R_UNEXPECTED;
	}
	
#if defined (TRACING)
	/* If we're recording a trace, remember the connection. */
	if (trace_record ()) {
		trace_iov_t iov [3];
		u_int32_t lsock;
		iov [0].buf = (char *)&addr.sin_port;
		iov [0].len = sizeof addr.sin_port;
		iov [1].buf = (char *)&addr.sin_addr;
		iov [1].len = sizeof addr.sin_addr;
		iov [2].buf = (char *)&listener -> address.sin_port;
		iov [2].len = sizeof listener -> address.sin_port;
		trace_write_packet_iov (trace_listener_accept,
					3, iov, MDL);
	}
#endif

	obj = (omapi_connection_object_t *)0;
	status = omapi_listener_connect (&obj, listener, socket, &addr);
	if (status != ISC_R_SUCCESS) {
		close (socket);
		return status;
	}

	status = omapi_register_io_object ((omapi_object_t *)obj,
					   omapi_connection_readfd,
					   omapi_connection_writefd,
					   omapi_connection_reader,
					   omapi_connection_writer,
					   omapi_connection_reaper);

	/* Lose our reference to the connection, so it'll be gc'd when it's
	   reaped. */
	omapi_connection_dereference (&obj, MDL);
	if (status != ISC_R_SUCCESS)
		omapi_disconnect ((omapi_object_t *)(obj), 1);
	return status;
}

isc_result_t omapi_listener_connect (omapi_connection_object_t **obj,
				     omapi_listener_object_t *listener,
				     int socket,
				     struct sockaddr_in *remote_addr)
{
	isc_result_t status;
	omapi_object_t *h = (omapi_object_t *)listener;
	omapi_addr_t addr;

#ifdef DEBUG_PROTOCOL
	log_debug ("omapi_accept()");
#endif
	
	/* Get the handle. */
	status = omapi_connection_allocate (obj, MDL);
	if (status != ISC_R_SUCCESS)
		return status;

	(*obj) -> state = omapi_connection_connected;
	(*obj) -> remote_addr = *remote_addr;
	(*obj) -> socket = socket;

	/* Verify that this host is allowed to connect. */
	if (listener -> verify_addr) {
		addr.addrtype = AF_INET;
		addr.addrlen = sizeof (remote_addr -> sin_addr);
		memcpy (addr.address, &remote_addr -> sin_addr,
			sizeof (remote_addr -> sin_addr));
		addr.port = ntohs(remote_addr -> sin_port);

		status = (listener -> verify_addr) (h, &addr);
		if (status != ISC_R_SUCCESS) {
			omapi_disconnect ((omapi_object_t *)(*obj), 1);
			omapi_connection_dereference (obj, MDL);
			return status;
		}
	}

	omapi_listener_reference (&(*obj) -> listener, listener, MDL);
#if defined (TRACING)
	omapi_connection_register (*obj, MDL);
#endif
	status = omapi_signal (h, "connect", (*obj));
	return status;
}

#if defined (TRACING)
OMAPI_ARRAY_TYPE(omapi_listener, omapi_listener_object_t)

void omapi_listener_trace_setup (void) {
	trace_listener_accept =
		trace_type_register ("listener-accept", (void *)0,
				     trace_listener_accept_input,
				     trace_listener_accept_stop, MDL);
}

static void trace_listener_remember (omapi_listener_object_t *obj,
				     const char *file, int line)
{
	isc_result_t status;
	if (!trace_listeners) {
		status = omapi_listener_array_allocate (&trace_listeners,
							file, line);
		if (status != ISC_R_SUCCESS) {
		      foo:
			log_error ("trace_listener_remember: %s",
				   isc_result_totext (status));
			return;
		}
	}
	status = omapi_listener_array_extend (trace_listeners, obj,
					      &obj -> index, MDL);
	if (status != ISC_R_SUCCESS)
		goto foo;
}

static void trace_listener_accept_input (trace_type_t *ttype,
					 unsigned length, char *buf)
{
	struct in_addr *addr;
	u_int16_t *remote_port;
	u_int16_t *local_port;
	omapi_connection_object_t *obj;
	isc_result_t status;
	struct sockaddr_in remote_addr;

	addr = (struct in_addr *)buf;
	remote_port = (u_int16_t *)(addr + 1);
	local_port = remote_port + 1;

	memset (&remote_addr, 0, sizeof remote_addr);
	remote_addr.sin_addr = *addr;
	remote_addr.sin_port = *remote_port;

	omapi_array_foreach_begin (trace_listeners,
				   omapi_listener_object_t, lp) {
		if (lp -> address.sin_port == *local_port) {
			obj = (omapi_connection_object_t *)0;
			status = omapi_listener_connect (&obj,
							 lp, 0, &remote_addr);
			omapi_listener_dereference (&lp, MDL);
			return;
		}
	} omapi_array_foreach_end (trace_listeners,
				   omapi_listener_object_t, lp);
	log_error ("trace_listener_accept: %s from %s/%d to port %d",
		   "unexpected connect",
		   inet_ntoa (*addr), *remote_port, *local_port);
}

static void trace_listener_accept_stop (trace_type_t *ttype) { }


#endif

isc_result_t omapi_listener_configure_security (omapi_object_t *h,
						isc_result_t (*verify_addr)
						 (omapi_object_t *,
						  omapi_addr_t *))
{
	omapi_listener_object_t *l;

	if (h -> type != omapi_type_listener)
		return ISC_R_INVALIDARG;
	l = (omapi_listener_object_t *)h;

	l -> verify_addr = verify_addr;

	return ISC_R_SUCCESS;
}

isc_result_t omapi_listener_set_value (omapi_object_t *h,
				      omapi_object_t *id,
				      omapi_data_string_t *name,
				      omapi_typed_data_t *value)
{
	if (h -> type != omapi_type_listener)
		return ISC_R_INVALIDARG;
	
	if (h -> inner && h -> inner -> type -> set_value)
		return (*(h -> inner -> type -> set_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t omapi_listener_get_value (omapi_object_t *h,
				       omapi_object_t *id,
				       omapi_data_string_t *name,
				       omapi_value_t **value)
{
	if (h -> type != omapi_type_listener)
		return ISC_R_INVALIDARG;
	
	if (h -> inner && h -> inner -> type -> get_value)
		return (*(h -> inner -> type -> get_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t omapi_listener_destroy (omapi_object_t *h,
				     const char *file, int line)
{
	omapi_listener_object_t *l;

	if (h -> type != omapi_type_listener)
		return ISC_R_INVALIDARG;
	l = (omapi_listener_object_t *)h;

#ifdef DEBUG_PROTOCOL
	log_debug ("omapi_listener_destroy()");
#endif
	
	if (l -> socket != -1) {
		close (l -> socket);
		l -> socket = -1;
	}
	return ISC_R_SUCCESS;
}

isc_result_t omapi_listener_signal_handler (omapi_object_t *h,
					    const char *name, va_list ap)
{
	if (h -> type != omapi_type_listener)
		return ISC_R_INVALIDARG;
	
	if (h -> inner && h -> inner -> type -> signal_handler)
		return (*(h -> inner -> type -> signal_handler)) (h -> inner,
								  name, ap);
	return ISC_R_NOTFOUND;
}

/* Write all the published values associated with the object through the
   specified connection. */

isc_result_t omapi_listener_stuff_values (omapi_object_t *c,
					  omapi_object_t *id,
					  omapi_object_t *l)
{
	int i;

	if (l -> type != omapi_type_listener)
		return ISC_R_INVALIDARG;

	if (l -> inner && l -> inner -> type -> stuff_values)
		return (*(l -> inner -> type -> stuff_values)) (c, id,
								l -> inner);
	return ISC_R_SUCCESS;
}

