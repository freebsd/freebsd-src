/* connection.c

   Subroutines for dealing with connections. */

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
#include <arpa/inet.h>
#include <arpa/nameser.h>


#if defined (TRACING)
static void trace_connect_input (trace_type_t *, unsigned, char *);
static void trace_connect_stop (trace_type_t *);
static void trace_disconnect_input (trace_type_t *, unsigned, char *);
static void trace_disconnect_stop (trace_type_t *);
trace_type_t *trace_connect;
trace_type_t *trace_disconnect;
extern omapi_array_t *trace_listeners;
#endif
static isc_result_t omapi_connection_connect_internal (omapi_object_t *);

OMAPI_OBJECT_ALLOC (omapi_connection,
		    omapi_connection_object_t, omapi_type_connection)

isc_result_t omapi_connect (omapi_object_t *c,
			    const char *server_name,
			    unsigned port)
{
	struct hostent *he;
	unsigned i, hix;
	omapi_addr_list_t *addrs = (omapi_addr_list_t *)0;
	struct in_addr foo;
	isc_result_t status;

#ifdef DEBUG_PROTOCOL
	log_debug ("omapi_connect(%s, port=%d)", server_name, port);
#endif

	if (!inet_aton (server_name, &foo)) {
		/* If we didn't get a numeric address, try for a domain
		   name.  It's okay for this call to block. */
		he = gethostbyname (server_name);
		if (!he)
			return ISC_R_HOSTUNKNOWN;
		for (i = 0; he -> h_addr_list [i]; i++)
			;
		if (i == 0)
			return ISC_R_HOSTUNKNOWN;
		hix = i;

		status = omapi_addr_list_new (&addrs, hix, MDL);
		if (status != ISC_R_SUCCESS)
			return status;
		for (i = 0; i < hix; i++) {
			addrs -> addresses [i].addrtype = he -> h_addrtype;
			addrs -> addresses [i].addrlen = he -> h_length;
			memcpy (addrs -> addresses [i].address,
				he -> h_addr_list [i],
				(unsigned)he -> h_length);
			addrs -> addresses [i].port = port;
		}
	} else {
		status = omapi_addr_list_new (&addrs, 1, MDL);
		if (status != ISC_R_SUCCESS)
			return status;
		addrs -> addresses [0].addrtype = AF_INET;
		addrs -> addresses [0].addrlen = sizeof foo;
		memcpy (addrs -> addresses [0].address, &foo, sizeof foo);
		addrs -> addresses [0].port = port;
		hix = 1;
	}
	status = omapi_connect_list (c, addrs, (omapi_addr_t *)0);
	omapi_addr_list_dereference (&addrs, MDL);
	return status;
}

isc_result_t omapi_connect_list (omapi_object_t *c,
				 omapi_addr_list_t *remote_addrs,
				 omapi_addr_t *local_addr)
{
	isc_result_t status;
	omapi_connection_object_t *obj;
	int flag;
	struct sockaddr_in local_sin;
#if defined (TRACING)
	trace_addr_t *addrs;
	u_int16_t naddrs;
#endif

	obj = (omapi_connection_object_t *)0;
	status = omapi_connection_allocate (&obj, MDL);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_object_reference (&c -> outer, (omapi_object_t *)obj,
					 MDL);
	if (status != ISC_R_SUCCESS) {
		omapi_connection_dereference (&obj, MDL);
		return status;
	}
	status = omapi_object_reference (&obj -> inner, c, MDL);
	if (status != ISC_R_SUCCESS) {
		omapi_connection_dereference (&obj, MDL);
		return status;
	}

	/* Store the address list on the object. */
	omapi_addr_list_reference (&obj -> connect_list, remote_addrs, MDL);
	obj -> cptr = 0;
	obj -> state = omapi_connection_unconnected;

#if defined (TRACING)
	/* If we're playing back, don't actually try to connect - just leave
	   the object available for a subsequent connect or disconnect. */
	if (!trace_playback ()) {
#endif
		/* Create a socket on which to communicate. */
		obj -> socket =
			socket (PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (obj -> socket < 0) {
			omapi_connection_dereference (&obj, MDL);
			if (errno == EMFILE || errno == ENFILE
			    || errno == ENOBUFS)
				return ISC_R_NORESOURCES;
			return ISC_R_UNEXPECTED;
		}

		/* Set up the local address, if any. */
		if (local_addr) {
			/* Only do TCPv4 so far. */
			if (local_addr -> addrtype != AF_INET) {
				omapi_connection_dereference (&obj, MDL);
				return ISC_R_INVALIDARG;
			}
			local_sin.sin_port = htons (local_addr -> port);
			memcpy (&local_sin.sin_addr,
				local_addr -> address,
				local_addr -> addrlen);
#if defined (HAVE_SA_LEN)
			local_sin.sin_len = sizeof local_addr;
#endif
			local_sin.sin_family = AF_INET;
			memset (&local_sin.sin_zero, 0,
				sizeof local_sin.sin_zero);
			
			if (bind (obj -> socket, (struct sockaddr *)&local_sin,
				  sizeof local_sin) < 0) {
				omapi_object_dereference ((omapi_object_t **)
							  &obj, MDL);
				if (errno == EADDRINUSE)
					return ISC_R_ADDRINUSE;
				if (errno == EADDRNOTAVAIL)
					return ISC_R_ADDRNOTAVAIL;
				if (errno == EACCES)
					return ISC_R_NOPERM;
				return ISC_R_UNEXPECTED;
			}
			obj -> local_addr = local_sin;
		}

#if defined (HAVE_SETFD)
		if (fcntl (obj -> socket, F_SETFD, 1) < 0) {
			close (obj -> socket);
			omapi_connection_dereference (&obj, MDL);
			return ISC_R_UNEXPECTED;
		}
#endif

		/* Set the SO_REUSEADDR flag (this should not fail). */
		flag = 1;
		if (setsockopt (obj -> socket, SOL_SOCKET, SO_REUSEADDR,
				(char *)&flag, sizeof flag) < 0) {
			omapi_connection_dereference (&obj, MDL);
			return ISC_R_UNEXPECTED;
		}
	
		/* Set the file to nonblocking mode. */
		if (fcntl (obj -> socket, F_SETFL, O_NONBLOCK) < 0) {
			omapi_connection_dereference (&obj, MDL);
			return ISC_R_UNEXPECTED;
		}

		status = (omapi_register_io_object
			  ((omapi_object_t *)obj,
			   0, omapi_connection_writefd,
			   0, omapi_connection_connect,
			   omapi_connection_reaper));
		if (status != ISC_R_SUCCESS)
			goto out;
		status = omapi_connection_connect_internal ((omapi_object_t *)
							    obj);
#if defined (TRACING)
	}
	omapi_connection_register (obj, MDL);
#endif

      out:
	omapi_connection_dereference (&obj, MDL);
	return status;
}

#if defined (TRACING)
omapi_array_t *omapi_connections;

OMAPI_ARRAY_TYPE(omapi_connection, omapi_connection_object_t)

void omapi_connection_trace_setup (void) {
	trace_connect = trace_type_register ("connect", (void *)0,
					     trace_connect_input,
					     trace_connect_stop, MDL);
	trace_disconnect = trace_type_register ("disconnect", (void *)0,
						trace_disconnect_input,
						trace_disconnect_stop, MDL);
}

void omapi_connection_register (omapi_connection_object_t *obj,
				const char *file, int line)
{
	isc_result_t status;
	trace_iov_t iov [6];
	int iov_count = 0;
	int32_t connect_index, listener_index;
	static int32_t index;

	if (!omapi_connections) {
		status = omapi_connection_array_allocate (&omapi_connections,
							  file, line);
		if (status != ISC_R_SUCCESS)
			return;
	}

	status = omapi_connection_array_extend (omapi_connections, obj,
						(int *)0, file, line);
	if (status != ISC_R_SUCCESS) {
		obj -> index = -1;
		return;
	}

#if defined (TRACING)
	if (trace_record ()) {
		/* Connection registration packet:
		   
		     int32_t index
		     int32_t listener_index [-1 means no listener]
		   u_int16_t remote_port
		   u_int16_t local_port
		   u_int32_t remote_addr
		   u_int32_t local_addr */

		connect_index = htonl (index);
		index++;
		if (obj -> listener)
			listener_index = htonl (obj -> listener -> index);
		else
			listener_index = htonl (-1);
		iov [iov_count].buf = (char *)&connect_index;
		iov [iov_count++].len = sizeof connect_index;
		iov [iov_count].buf = (char *)&listener_index;
		iov [iov_count++].len = sizeof listener_index;
		iov [iov_count].buf = (char *)&obj -> remote_addr.sin_port;
		iov [iov_count++].len = sizeof obj -> remote_addr.sin_port;
		iov [iov_count].buf = (char *)&obj -> local_addr.sin_port;
		iov [iov_count++].len = sizeof obj -> local_addr.sin_port;
		iov [iov_count].buf = (char *)&obj -> remote_addr.sin_addr;
		iov [iov_count++].len = sizeof obj -> remote_addr.sin_addr;
		iov [iov_count].buf = (char *)&obj -> local_addr.sin_addr;
		iov [iov_count++].len = sizeof obj -> local_addr.sin_addr;

		status = trace_write_packet_iov (trace_connect,
						 iov_count, iov, file, line);
	}
#endif
}

static void trace_connect_input (trace_type_t *ttype,
				 unsigned length, char *buf)
{
	struct sockaddr_in remote, local;
	int32_t connect_index, listener_index;
	char *s = buf;
	omapi_connection_object_t *obj;
	isc_result_t status;
	int i;

	if (length != ((sizeof connect_index) +
		       (sizeof remote.sin_port) +
		       (sizeof remote.sin_addr)) * 2) {
		log_error ("Trace connect: invalid length %d", length);
		return;
	}

	memset (&remote, 0, sizeof remote);
	memset (&local, 0, sizeof local);
	memcpy (&connect_index, s, sizeof connect_index);
	s += sizeof connect_index;
	memcpy (&listener_index, s, sizeof listener_index);
	s += sizeof listener_index;
	memcpy (&remote.sin_port, s, sizeof remote.sin_port);
	s += sizeof remote.sin_port;
	memcpy (&local.sin_port, s, sizeof local.sin_port);
	s += sizeof local.sin_port;
	memcpy (&remote.sin_addr, s, sizeof remote.sin_addr);
	s += sizeof remote.sin_addr;
	memcpy (&local.sin_addr, s, sizeof local.sin_addr);
	s += sizeof local.sin_addr;

	connect_index = ntohl (connect_index);
	listener_index = ntohl (listener_index);

	/* If this was a connect to a listener, then we just slap together
	   a new connection. */
	if (listener_index != -1) {
		omapi_listener_object_t *listener;
		listener = (omapi_listener_object_t *)0;
		omapi_array_foreach_begin (trace_listeners,
					   omapi_listener_object_t, lp) {
			if (lp -> address.sin_port == local.sin_port) {
				omapi_listener_reference (&listener, lp, MDL);
				omapi_listener_dereference (&lp, MDL);
				break;
			} 
		} omapi_array_foreach_end (trace_listeners,
					   omapi_listener_object_t, lp);
		if (!listener) {
			log_error ("%s%ld, addr %s, port %d",
				   "Spurious traced listener connect - index ",
				   (long int)listener_index,
				   inet_ntoa (local.sin_addr),
				   ntohs (local.sin_port));
			return;
		}
		obj = (omapi_connection_object_t *)0;
		status = omapi_listener_connect (&obj, listener, -1, &remote);
		if (status != ISC_R_SUCCESS) {
			log_error ("traced listener connect: %s",
				   isc_result_totext (status));
		}
		if (obj)
			omapi_connection_dereference (&obj, MDL);
		omapi_listener_dereference (&listener, MDL);
		return;
	}

	/* Find the matching connect object, if there is one. */
	omapi_array_foreach_begin (omapi_connections,
				   omapi_connection_object_t, lp) {
	    for (i = 0; (lp -> connect_list &&
			 i < lp -> connect_list -> count); i++) {
		    if (!memcmp (&remote.sin_addr,
				 &lp -> connect_list -> addresses [i].address,
				 sizeof remote.sin_addr) &&
			(ntohs (remote.sin_port) ==
			 lp -> connect_list -> addresses [i].port))
			lp -> state = omapi_connection_connected;
			lp -> remote_addr = remote;
			lp -> remote_addr.sin_family = AF_INET;
#if defined (HAVE_SIN_LEN)
			lp -> remote_addr.sin_len = sizeof remote;
#endif
			omapi_addr_list_dereference (&lp -> connect_list, MDL);
			lp -> index = connect_index;
			status = omapi_signal_in ((omapi_object_t *)lp,
						  "connect");
			omapi_connection_dereference (&lp, MDL);
			return;
		}
	} omapi_array_foreach_end (omapi_connections,
				   omapi_connection_object_t, lp);
						 
	log_error ("Spurious traced connect - index %ld, addr %s, port %d",
		   (long int)connect_index, inet_ntoa (remote.sin_addr),
		   ntohs (remote.sin_port));
	return;
}

static void trace_connect_stop (trace_type_t *ttype) { }

static void trace_disconnect_input (trace_type_t *ttype,
				    unsigned length, char *buf)
{
	int32_t *index;
	if (length != sizeof *index) {
		log_error ("trace disconnect: wrong length %d", length);
		return;
	}
	
	index = (int32_t *)buf;

	omapi_array_foreach_begin (omapi_connections,
				   omapi_connection_object_t, lp) {
		if (lp -> index == ntohl (*index)) {
			omapi_disconnect ((omapi_object_t *)lp, 1);
			omapi_connection_dereference (&lp, MDL);
			return;
		}
	} omapi_array_foreach_end (omapi_connections,
				   omapi_connection_object_t, lp);

	log_error ("trace disconnect: no connection matching index %ld",
		   (long int)ntohl (*index));
}

static void trace_disconnect_stop (trace_type_t *ttype) { }
#endif

/* Disconnect a connection object from the remote end.   If force is nonzero,
   close the connection immediately.   Otherwise, shut down the receiving end
   but allow any unsent data to be sent before actually closing the socket. */

isc_result_t omapi_disconnect (omapi_object_t *h,
			       int force)
{
	omapi_connection_object_t *c;
	isc_result_t status;

#ifdef DEBUG_PROTOCOL
	log_debug ("omapi_disconnect(%s)", force ? "force" : "");
#endif

	c = (omapi_connection_object_t *)h;
	if (c -> type != omapi_type_connection)
		return ISC_R_INVALIDARG;

#if defined (TRACING)
	if (trace_record ()) {
		int32_t index;

		index = htonl (c -> index);
		status = trace_write_packet (trace_disconnect,
					     sizeof index, (char *)&index,
					     MDL);
		if (status != ISC_R_SUCCESS) {
			trace_stop ();
			log_error ("trace_write_packet: %s",
				   isc_result_totext (status));
		}
	}
	if (!trace_playback ()) {
#endif
		if (!force) {
			/* If we're already disconnecting, we don't have to do
			   anything. */
			if (c -> state == omapi_connection_disconnecting)
				return ISC_R_SUCCESS;

			/* Try to shut down the socket - this sends a FIN to
			   the remote end, so that it won't send us any more
			   data.   If the shutdown succeeds, and we still
			   have bytes left to write, defer closing the socket
			   until that's done. */
			if (!shutdown (c -> socket, SHUT_RD)) {
				if (c -> out_bytes > 0) {
					c -> state =
						omapi_connection_disconnecting;
					return ISC_R_SUCCESS;
				}
			}
		}
		close (c -> socket);
#if defined (TRACING)
	}
#endif
	c -> state = omapi_connection_closed;

	/* Disconnect from I/O object, if any. */
	if (h -> outer) {
		if (h -> outer -> inner)
			omapi_object_dereference (&h -> outer -> inner, MDL);
		omapi_object_dereference (&h -> outer, MDL);
	}

	/* If whatever created us registered a signal handler, send it
	   a disconnect signal. */
	omapi_signal (h, "disconnect", h);
	return ISC_R_SUCCESS;
}

isc_result_t omapi_connection_require (omapi_object_t *h, unsigned bytes)
{
	omapi_connection_object_t *c;

	if (h -> type != omapi_type_connection)
		return ISC_R_INVALIDARG;
	c = (omapi_connection_object_t *)h;

	c -> bytes_needed = bytes;
	if (c -> bytes_needed <= c -> in_bytes) {
		return ISC_R_SUCCESS;
	}
	return ISC_R_NOTYET;
}

/* Return the socket on which the dispatcher should wait for readiness
   to read, for a connection object.   If we already have more bytes than
   we need to do the next thing, and we have at least a single full input
   buffer, then don't indicate that we're ready to read. */
int omapi_connection_readfd (omapi_object_t *h)
{
	omapi_connection_object_t *c;
	if (h -> type != omapi_type_connection)
		return -1;
	c = (omapi_connection_object_t *)h;
	if (c -> state != omapi_connection_connected)
		return -1;
	if (c -> in_bytes >= OMAPI_BUF_SIZE - 1 &&
	    c -> in_bytes > c -> bytes_needed)
		return -1;
	return c -> socket;
}

/* Return the socket on which the dispatcher should wait for readiness
   to write, for a connection object.   If there are no bytes buffered
   for writing, then don't indicate that we're ready to write. */
int omapi_connection_writefd (omapi_object_t *h)
{
	omapi_connection_object_t *c;
	if (h -> type != omapi_type_connection)
		return -1;
	c = (omapi_connection_object_t *)h;
	if (c -> state == omapi_connection_connecting)
		return c -> socket;
	if (c -> out_bytes)
		return c -> socket;
	else
		return -1;
}

isc_result_t omapi_connection_connect (omapi_object_t *h)
{
	isc_result_t status;

	status = omapi_connection_connect_internal (h);
	if (status != ISC_R_SUCCESS)
		omapi_signal (h, "status", status);
	return ISC_R_SUCCESS;
}

static isc_result_t omapi_connection_connect_internal (omapi_object_t *h)
{
	int error;
	omapi_connection_object_t *c;
	SOCKLEN_T sl;
	isc_result_t status;

	if (h -> type != omapi_type_connection)
		return ISC_R_INVALIDARG;
	c = (omapi_connection_object_t *)h;

	if (c -> state == omapi_connection_connecting) {
		sl = sizeof error;
		if (getsockopt (c -> socket, SOL_SOCKET, SO_ERROR,
				(char *)&error, &sl) < 0) {
			omapi_disconnect (h, 1);
			return ISC_R_SUCCESS;
		}
		if (!error)
			c -> state = omapi_connection_connected;
	}
	if (c -> state == omapi_connection_connecting ||
	    c -> state == omapi_connection_unconnected) {
		if (c -> cptr >= c -> connect_list -> count) {
			switch (error) {
			      case ECONNREFUSED:
				status = ISC_R_CONNREFUSED;
				break;
			      case ENETUNREACH:
				status = ISC_R_NETUNREACH;
				break;
			      default:
				status = uerr2isc (error);
				break;
			}
			omapi_disconnect (h, 1);
			return status;
		}

		if (c -> connect_list -> addresses [c -> cptr].addrtype !=
		    AF_INET) {
			omapi_disconnect (h, 1);
			return ISC_R_INVALIDARG;
		}

		memcpy (&c -> remote_addr.sin_addr,
			&c -> connect_list -> addresses [c -> cptr].address,
			sizeof c -> remote_addr.sin_addr);
		c -> remote_addr.sin_family = AF_INET;
		c -> remote_addr.sin_port =
		       htons (c -> connect_list -> addresses [c -> cptr].port);
#if defined (HAVE_SA_LEN)
		c -> remote_addr.sin_len = sizeof c -> remote_addr;
#endif
		memset (&c -> remote_addr.sin_zero, 0,
			sizeof c -> remote_addr.sin_zero);
		++c -> cptr;

		error = connect (c -> socket,
				 (struct sockaddr *)&c -> remote_addr,
				 sizeof c -> remote_addr);
		if (error < 0) {
			error = errno;
			if (error != EINPROGRESS) {
				omapi_disconnect (h, 1);
				switch (error) {
				      case ECONNREFUSED:
					status = ISC_R_CONNREFUSED;
					break;
				      case ENETUNREACH:
					status = ISC_R_NETUNREACH;
					break;
				      default:
					status = uerr2isc (error);
					break;
				}
				return status;
			}
			c -> state = omapi_connection_connecting;
			return ISC_R_INCOMPLETE;
		}
		c -> state = omapi_connection_connected;
	}
	
	/* I don't know why this would fail, so I'm tempted not to test
	   the return value. */
	sl = sizeof (c -> local_addr);
	if (getsockname (c -> socket,
			 (struct sockaddr *)&c -> local_addr, &sl) < 0) {
	}

	/* Disconnect from I/O object, if any. */
	if (h -> outer)
		omapi_unregister_io_object (h);

	status = omapi_register_io_object (h,
					   omapi_connection_readfd,
					   omapi_connection_writefd,
					   omapi_connection_reader,
					   omapi_connection_writer,
					   omapi_connection_reaper);

	if (status != ISC_R_SUCCESS) {
		omapi_disconnect (h, 1);
		return status;
	}

	omapi_signal_in (h, "connect");
	omapi_addr_list_dereference (&c -> connect_list, MDL);
	return ISC_R_SUCCESS;
}

/* Reaper function for connection - if the connection is completely closed,
   reap it.   If it's in the disconnecting state, there were bytes left
   to write when the user closed it, so if there are now no bytes left to
   write, we can close it. */
isc_result_t omapi_connection_reaper (omapi_object_t *h)
{
	omapi_connection_object_t *c;

	if (h -> type != omapi_type_connection)
		return ISC_R_INVALIDARG;

	c = (omapi_connection_object_t *)h;
	if (c -> state == omapi_connection_disconnecting &&
	    c -> out_bytes == 0) {
#ifdef DEBUG_PROTOCOL
		log_debug ("omapi_connection_reaper(): disconnect");
#endif
		omapi_disconnect (h, 1);
	}
	if (c -> state == omapi_connection_closed) {
#ifdef DEBUG_PROTOCOL
		log_debug ("omapi_connection_reaper(): closed");
#endif
		return ISC_R_NOTCONNECTED;
	}
	return ISC_R_SUCCESS;
}

static isc_result_t make_dst_key (DST_KEY **dst_key, omapi_object_t *a) {
	omapi_value_t *name      = (omapi_value_t *)0;
	omapi_value_t *algorithm = (omapi_value_t *)0;
	omapi_value_t *key       = (omapi_value_t *)0;
	int algorithm_id;
	char *name_str;
	isc_result_t status = ISC_R_SUCCESS;

	if (status == ISC_R_SUCCESS)
		status = omapi_get_value_str
			(a, (omapi_object_t *)0, "name", &name);

	if (status == ISC_R_SUCCESS)
		status = omapi_get_value_str
			(a, (omapi_object_t *)0, "algorithm", &algorithm);

	if (status == ISC_R_SUCCESS)
		status = omapi_get_value_str
			(a, (omapi_object_t *)0, "key", &key);

	if (status == ISC_R_SUCCESS) {
		if ((algorithm -> value -> type == omapi_datatype_data ||
		     algorithm -> value -> type == omapi_datatype_string) &&
		    strncasecmp ((char *)algorithm -> value -> u.buffer.value,
		                 NS_TSIG_ALG_HMAC_MD5 ".",
		                 algorithm -> value -> u.buffer.len) == 0) {
			algorithm_id = KEY_HMAC_MD5;
		} else {
			status = ISC_R_INVALIDARG;
		}
	}

	if (status == ISC_R_SUCCESS) {
		name_str = dmalloc (name -> value -> u.buffer.len + 1, MDL);
		if (!name_str)
			status = ISC_R_NOMEMORY;
	}

	if (status == ISC_R_SUCCESS) {
		memcpy (name_str,
			name -> value -> u.buffer.value,
			name -> value -> u.buffer.len);
		name_str [name -> value -> u.buffer.len] = 0;

		*dst_key = dst_buffer_to_key (name_str, algorithm_id, 0, 0,
					      key -> value -> u.buffer.value,
					      key -> value -> u.buffer.len);
		if (!*dst_key)
			status = ISC_R_NOMEMORY;
	}

	if (name_str)
		dfree (name_str, MDL);
	if (key)
		omapi_value_dereference (&key, MDL);
	if (algorithm)
		omapi_value_dereference (&algorithm, MDL);
	if (name)
		omapi_value_dereference (&name, MDL);

	return status;
}

isc_result_t omapi_connection_sign_data (int mode,
					 DST_KEY *key,
					 void **context,
					 const unsigned char *data,
					 const unsigned len,
					 omapi_typed_data_t **result)
{
	omapi_typed_data_t *td = (omapi_typed_data_t *)0;
	isc_result_t status;
	int r;

	if (mode & SIG_MODE_FINAL) {
		status = omapi_typed_data_new (MDL, &td,
					       omapi_datatype_data,
					       dst_sig_size (key));
		if (status != ISC_R_SUCCESS)
			return status;
	}

	r = dst_sign_data (mode, key, context, data, len,
			   td ? td -> u.buffer.value : (u_char *)0,
			   td ? td -> u.buffer.len   : 0);

	/* dst_sign_data() really should do this for us, shouldn't it? */
	if (mode & SIG_MODE_FINAL)
		*context = (void *)0;

	if (r < 0) {
		if (td)
			omapi_typed_data_dereference (&td, MDL);
		return ISC_R_INVALIDKEY;
	}

	if (result && td) {
		omapi_typed_data_reference (result, td, MDL);
	}

	if (td)
		omapi_typed_data_dereference (&td, MDL);

	return ISC_R_SUCCESS;
}

isc_result_t omapi_connection_output_auth_length (omapi_object_t *h,
						  unsigned *l)
{
	omapi_connection_object_t *c;

	if (h -> type != omapi_type_connection)
		return ISC_R_INVALIDARG;
	c = (omapi_connection_object_t *)h;

	if (!c -> out_key)
		return ISC_R_NOTFOUND;

	*l = dst_sig_size (c -> out_key);
	return ISC_R_SUCCESS;
}

isc_result_t omapi_connection_set_value (omapi_object_t *h,
					 omapi_object_t *id,
					 omapi_data_string_t *name,
					 omapi_typed_data_t *value)
{
	omapi_connection_object_t *c;
	isc_result_t status;

	if (h -> type != omapi_type_connection)
		return ISC_R_INVALIDARG;
	c = (omapi_connection_object_t *)h;

	if (omapi_ds_strcmp (name, "input-authenticator") == 0) {
		if (value && value -> type != omapi_datatype_object)
			return ISC_R_INVALIDARG;

		if (c -> in_context) {
			omapi_connection_sign_data (SIG_MODE_FINAL,
						    c -> in_key,
						    &c -> in_context,
						    0, 0,
						    (omapi_typed_data_t **) 0);
		}

		if (c -> in_key) {
			dst_free_key (c -> in_key);
			c -> in_key = (DST_KEY *)0;
		}

		if (value) {
			status = make_dst_key (&c -> in_key,
					       value -> u.object);
			if (status != ISC_R_SUCCESS)
				return status;
		}

		return ISC_R_SUCCESS;
	}
	else if (omapi_ds_strcmp (name, "output-authenticator") == 0) {
		if (value && value -> type != omapi_datatype_object)
			return ISC_R_INVALIDARG;

		if (c -> out_context) {
			omapi_connection_sign_data (SIG_MODE_FINAL,
						    c -> out_key,
						    &c -> out_context,
						    0, 0,
						    (omapi_typed_data_t **) 0);
		}

		if (c -> out_key) {
			dst_free_key (c -> out_key);
			c -> out_key = (DST_KEY *)0;
		}

		if (value) {
			status = make_dst_key (&c -> out_key,
					       value -> u.object);
			if (status != ISC_R_SUCCESS)
				return status;
		}

		return ISC_R_SUCCESS;
	}
	
	if (h -> inner && h -> inner -> type -> set_value)
		return (*(h -> inner -> type -> set_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t omapi_connection_get_value (omapi_object_t *h,
					 omapi_object_t *id,
					 omapi_data_string_t *name,
					 omapi_value_t **value)
{
	omapi_connection_object_t *c;
	omapi_typed_data_t *td = (omapi_typed_data_t *)0;
	isc_result_t status;

	if (h -> type != omapi_type_connection)
		return ISC_R_INVALIDARG;
	c = (omapi_connection_object_t *)h;

	if (omapi_ds_strcmp (name, "input-signature") == 0) {
		if (!c -> in_key || !c -> in_context)
			return ISC_R_NOTFOUND;

		status = omapi_connection_sign_data (SIG_MODE_FINAL,
						     c -> in_key,
						     &c -> in_context,
						     0, 0, &td);
		if (status != ISC_R_SUCCESS)
			return status;

		status = omapi_make_value (value, name, td, MDL);
		omapi_typed_data_dereference (&td, MDL);
		return status;

	} else if (omapi_ds_strcmp (name, "input-signature-size") == 0) {
		if (!c -> in_key)
			return ISC_R_NOTFOUND;

		return omapi_make_int_value (value, name,
					     dst_sig_size (c -> in_key), MDL);

	} else if (omapi_ds_strcmp (name, "output-signature") == 0) {
		if (!c -> out_key || !c -> out_context)
			return ISC_R_NOTFOUND;

		status = omapi_connection_sign_data (SIG_MODE_FINAL,
						     c -> out_key,
						     &c -> out_context,
						     0, 0, &td);
		if (status != ISC_R_SUCCESS)
			return status;

		status = omapi_make_value (value, name, td, MDL);
		omapi_typed_data_dereference (&td, MDL);
		return status;

	} else if (omapi_ds_strcmp (name, "output-signature-size") == 0) {
		if (!c -> out_key)
			return ISC_R_NOTFOUND;

		return omapi_make_int_value (value, name,
					     dst_sig_size (c -> out_key), MDL);
	}
	
	if (h -> inner && h -> inner -> type -> get_value)
		return (*(h -> inner -> type -> get_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t omapi_connection_destroy (omapi_object_t *h,
				       const char *file, int line)
{
	omapi_connection_object_t *c;

#ifdef DEBUG_PROTOCOL
	log_debug ("omapi_connection_destroy()");
#endif

	if (h -> type != omapi_type_connection)
		return ISC_R_UNEXPECTED;
	c = (omapi_connection_object_t *)(h);
	if (c -> state == omapi_connection_connected)
		omapi_disconnect (h, 1);
	if (c -> listener)
		omapi_listener_dereference (&c -> listener, file, line);
	if (c -> connect_list)
		omapi_addr_list_dereference (&c -> connect_list, file, line);
	return ISC_R_SUCCESS;
}

isc_result_t omapi_connection_signal_handler (omapi_object_t *h,
					      const char *name, va_list ap)
{
	if (h -> type != omapi_type_connection)
		return ISC_R_INVALIDARG;

#ifdef DEBUG_PROTOCOL
	log_debug ("omapi_connection_signal_handler(%s)", name);
#endif
	
	if (h -> inner && h -> inner -> type -> signal_handler)
		return (*(h -> inner -> type -> signal_handler)) (h -> inner,
								  name, ap);
	return ISC_R_NOTFOUND;
}

/* Write all the published values associated with the object through the
   specified connection. */

isc_result_t omapi_connection_stuff_values (omapi_object_t *c,
					    omapi_object_t *id,
					    omapi_object_t *m)
{
	int i;

	if (m -> type != omapi_type_connection)
		return ISC_R_INVALIDARG;

	if (m -> inner && m -> inner -> type -> stuff_values)
		return (*(m -> inner -> type -> stuff_values)) (c, id,
								m -> inner);
	return ISC_R_SUCCESS;
}
