/* protocol.c

   Functions supporting the object management protocol... */

/*
 * Copyright (c) 1999-2000 Internet Software Consortium.
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

OMAPI_OBJECT_ALLOC (omapi_protocol, omapi_protocol_object_t,
		    omapi_type_protocol)
OMAPI_OBJECT_ALLOC (omapi_protocol_listener, omapi_protocol_listener_object_t,
		    omapi_type_protocol_listener)

isc_result_t omapi_protocol_connect (omapi_object_t *h,
				     const char *server_name,
				     unsigned port,
				     omapi_object_t *a)
{
	isc_result_t rstatus, status;
	omapi_protocol_object_t *obj;

#ifdef DEBUG_PROTOCOL
	log_debug ("omapi_protocol_connect(%s port=%d)", server_name, port);
#endif

	obj = (omapi_protocol_object_t *)0;
	status = omapi_protocol_allocate (&obj, MDL);
	if (status != ISC_R_SUCCESS)
		return status;

	rstatus = omapi_connect ((omapi_object_t *)obj, server_name, port);
	if (rstatus != ISC_R_SUCCESS && rstatus != ISC_R_INCOMPLETE) {
		omapi_protocol_dereference (&obj, MDL);
		return rstatus;
	}
	status = omapi_object_reference (&h -> outer,
					 (omapi_object_t *)obj, MDL);
	if (status != ISC_R_SUCCESS) {
		omapi_protocol_dereference (&obj, MDL);
		return status;
	}
	status = omapi_object_reference (&obj -> inner, h, MDL);
	if (status != ISC_R_SUCCESS) {
		omapi_protocol_dereference (&obj, MDL);
		return status;
	}

	/* If we were passed a default authenticator, store it now.  We'll
	   open it once we're connected. */
	if (a) {
		obj -> default_auth =
			dmalloc (sizeof(omapi_remote_auth_t), MDL);
		if (!obj -> default_auth) {
			omapi_protocol_dereference (&obj, MDL);
			return ISC_R_NOMEMORY;
		}

		obj -> default_auth -> next = (omapi_remote_auth_t *)0;
		status = omapi_object_reference (&obj -> default_auth -> a,
						 a, MDL);
		if (status != ISC_R_SUCCESS) {
			dfree (obj -> default_auth, MDL);
			omapi_protocol_dereference (&obj, MDL);
			return status;
		}

		obj -> insecure = 0;
		rstatus = ISC_R_INCOMPLETE;
	} else {
		obj -> insecure = 1;
#if 0
		status = ISC_R_SUCCESS;
#endif
	}

	omapi_protocol_dereference (&obj, MDL);
	return rstatus;
}

/* Send the protocol introduction message. */
isc_result_t omapi_protocol_send_intro (omapi_object_t *h,
					unsigned ver,
					unsigned hsize)
{
	isc_result_t status;
	omapi_protocol_object_t *p;

#ifdef DEBUG_PROTOCOL
	log_debug ("omapi_protocol_send_intro()");
#endif

	if (h -> type != omapi_type_protocol)
		return ISC_R_INVALIDARG;
	p = (omapi_protocol_object_t *)h;

	if (!h -> outer || h -> outer -> type != omapi_type_connection)
		return ISC_R_NOTCONNECTED;

	status = omapi_connection_put_uint32 (h -> outer, ver);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_connection_put_uint32 (h -> outer, hsize);

	if (status != ISC_R_SUCCESS)
		return status;

	/* Require the other end to send an intro - this kicks off the
	   protocol input state machine. */
	p -> state = omapi_protocol_intro_wait;
	status = omapi_connection_require (h -> outer, 8);
	if (status != ISC_R_SUCCESS && status != ISC_R_NOTYET)
		return status;

	/* Make up an initial transaction ID for this connection. */
	p -> next_xid = random ();
	return ISC_R_SUCCESS;
}

isc_result_t omapi_protocol_send_message (omapi_object_t *po,
					  omapi_object_t *id,
					  omapi_object_t *mo,
					  omapi_object_t *omo)
{
	omapi_protocol_object_t *p;
	omapi_object_t *c;
	omapi_message_object_t *m, *om;
	omapi_remote_auth_t *ra;
	omapi_value_t *signature;
	isc_result_t status;
	u_int32_t foo;
	unsigned auth_len;

	if (po -> type != omapi_type_protocol ||
	    !po -> outer || po -> outer -> type != omapi_type_connection ||
	    mo -> type != omapi_type_message)
		return ISC_R_INVALIDARG;
	if (omo && omo -> type != omapi_type_message)
		return ISC_R_INVALIDARG;
	p = (omapi_protocol_object_t *)po;
	c = (omapi_object_t *)(po -> outer);
	m = (omapi_message_object_t *)mo;
	om = (omapi_message_object_t *)omo;

#ifdef DEBUG_PROTOCOL
	log_debug ("omapi_protocol_send_message()"
		   "op=%ld  handle=%#lx  id=%#lx  rid=%#lx",
		   (long)m -> op,
		   (long)(m -> object ? m -> object -> handle : m -> handle),
		   (long)p -> next_xid, (long)m -> rid);
#endif

	/* Find the authid to use for this message. */
	if (id) {
		for (ra = p -> remote_auth_list; ra; ra = ra -> next) {
			if (ra -> a == id) {
				break;
			}
		}

		if (!ra)
			return ISC_R_KEY_UNKNOWN;
	} else if (p -> remote_auth_list) {
		ra = p -> default_auth;
	} else {
		ra = (omapi_remote_auth_t *)0;
	}

	if (ra) {
		m -> authid = ra -> remote_handle;
		status = omapi_object_reference (&m -> id_object,
						 ra -> a, MDL);
		if (status != ISC_R_SUCCESS)
			return status;
	}

	/* Write the ID of the authentication key we're using. */
	status = omapi_connection_put_uint32 (c, ra ? ra -> remote_handle : 0);
	if (status != ISC_R_SUCCESS) {
		omapi_disconnect (c, 1);
		return status;
	}

	/* Activate the authentication key on the connection. */
	auth_len = 0;
	if (ra) {
		status = omapi_set_object_value (c, (omapi_object_t *)0,
						 "output-authenticator",
						 ra -> a);
		if (status != ISC_R_SUCCESS) {
			omapi_disconnect (c, 1);
			return status;
		}

		status = omapi_connection_output_auth_length (c, &auth_len);
		if (status != ISC_R_SUCCESS) {
			omapi_disconnect (c, 1);
			return status;
		}
	}

	/* Write the authenticator length */
	status = omapi_connection_put_uint32 (c, auth_len);
	if (status != ISC_R_SUCCESS) {
		omapi_disconnect (c, 1);
		return status;
	}

	/* Write the opcode. */
	status = omapi_connection_put_uint32 (c, m -> op);
	if (status != ISC_R_SUCCESS) {
		omapi_disconnect (c, 1);
		return status;
	}

	/* Write the handle.  If we've been given an explicit handle, use
	   that.   Otherwise, use the handle of the object we're sending.
	   The caller is responsible for arranging for one of these handles
	   to be set (or not). */
	status = omapi_connection_put_uint32 (c, (m -> h
						  ? m -> h
						  : (m -> object
						     ? m -> object -> handle
						     : 0)));
	if (status != ISC_R_SUCCESS) {
		omapi_disconnect (c, 1);
		return status;
	}

	/* Set and write the transaction ID. */
	m -> id = p -> next_xid++;
	status = omapi_connection_put_uint32 (c, m -> id);
	if (status != ISC_R_SUCCESS) {
		omapi_disconnect (c, 1);
		return status;
	}

	/* Write the transaction ID of the message to which this is a
	   response, if there is such a message. */
	status = omapi_connection_put_uint32 (c, om ? om -> id : m -> rid);
	if (status != ISC_R_SUCCESS) {
		omapi_disconnect (c, 1);
		return status;
	}

	/* Stuff out the name/value pairs specific to this message. */
	status = omapi_stuff_values (c, id, (omapi_object_t *)m);
	if (status != ISC_R_SUCCESS) {
		omapi_disconnect (c, 1);
		return status;
	}

	/* Write the zero-length name that terminates the list of name/value
	   pairs specific to the message. */
	status = omapi_connection_put_uint16 (c, 0);
	if (status != ISC_R_SUCCESS) {
		omapi_disconnect (c, 1);
		return status;
	}

	/* Stuff out all the published name/value pairs in the object that's
	   being sent in the message, if there is one. */
	if (m -> object) {
		status = omapi_stuff_values (c, id, m -> object);
		if (status != ISC_R_SUCCESS) {
			omapi_disconnect (c, 1);
			return status;
		}
	}

	/* Write the zero-length name that terminates the list of name/value
	   pairs for the associated object. */
	status = omapi_connection_put_uint16 (c, 0);
	if (status != ISC_R_SUCCESS) {
		omapi_disconnect (c, 1);
		return status;
	}

	if (ra) {
		/* Calculate the message signature. */
		signature = (omapi_value_t *)0;
		status = omapi_get_value_str (c, (omapi_object_t *)0,
					      "output-signature", &signature);
		if (status != ISC_R_SUCCESS) {
			omapi_disconnect (c, 1);
			return status;
		}

		/* Write the authenticator... */
		status = (omapi_connection_copyin
			  (c, signature -> value -> u.buffer.value,
			   signature -> value -> u.buffer.len));
		omapi_value_dereference (&signature, MDL);
		if (status != ISC_R_SUCCESS) {
			omapi_disconnect (c, 1);
			return status;
		}

		/* Dectivate the authentication key on the connection. */
		status = omapi_set_value_str (c, (omapi_object_t *)0,
						 "output-authenticator",
						 (omapi_typed_data_t *)0);
		if (status != ISC_R_SUCCESS) {
			omapi_disconnect (c, 1);
			return status;
		}
	}

	if (!omo) {
		omapi_protocol_reference (&m -> protocol_object, p, MDL);
	}
	return ISC_R_SUCCESS;
}
					  

isc_result_t omapi_protocol_signal_handler (omapi_object_t *h,
					    const char *name, va_list ap)
{
	isc_result_t status;
	omapi_protocol_object_t *p;
	omapi_object_t *c;
	omapi_message_object_t *m;
	omapi_value_t *signature;
	u_int16_t nlen;
	u_int32_t vlen;
	u_int32_t th;
#if defined (DEBUG_MEMORY_LEAKAGE)
	unsigned long previous_outstanding = 0xDEADBEEF;
	unsigned long connect_outstanding = 0xDEADBEEF;
#endif

	if (h -> type != omapi_type_protocol) {
		/* XXX shouldn't happen.   Put an assert here? */
		return ISC_R_UNEXPECTED;
	}
	p = (omapi_protocol_object_t *)h;

	if (!strcmp (name, "connect")) {
#if defined (DEBUG_MEMORY_LEAKAGE)
		connect_outstanding = dmalloc_outstanding;
#endif
		/* Send the introductory message. */
		status = omapi_protocol_send_intro
			(h, OMAPI_PROTOCOL_VERSION,
			 sizeof (omapi_protocol_header_t));
		if (status != ISC_R_SUCCESS) {
			omapi_disconnect (p -> outer, 1);
			return status;
		}
		return ISC_R_SUCCESS;
	}

	/* Should only receive these when opening the initial authenticator. */
	if (!strcmp (name, "status")) {
		status = va_arg (ap, isc_result_t);
		if (status != ISC_R_SUCCESS) {
			omapi_signal_in (h -> inner, "status", status,
					 (omapi_object_t *)0);
			omapi_disconnect (p -> outer, 1);
			return status;
		} else {
			return omapi_signal_in (h -> inner, "ready");
		}
	}

	/* If we get a disconnect, dump memory usage. */
	if (!strcmp (name, "disconnect")) {
#if defined (DEBUG_MEMORY_LEAKAGE)
	    if (connect_outstanding != 0xDEADBEEF) {
		log_info ("generation %ld: %ld new, %ld outstanding, %ld%s",
			  dmalloc_generation,
			  dmalloc_outstanding - previous_outstanding,
			  dmalloc_outstanding, dmalloc_longterm, " long-term");
	    }
#endif
#if defined (DEBUG_MEMORY_LEAKAGE)
	    dmalloc_dump_outstanding ();
#endif
#if defined (DEBUG_RC_HISTORY_EXHAUSTIVELY)
	    dump_rc_history ();
#endif
	    for (m = omapi_registered_messages; m; m = m -> next) {
		if (m -> protocol_object == p) {
		    if (m -> object)
			omapi_signal (m -> object, "disconnect");
		}
	    }
	}

	/* Not a signal we recognize? */
	if (strcmp (name, "ready")) {
		if (p -> inner && p -> inner -> type -> signal_handler)
			return (*(p -> inner -> type -> signal_handler)) (h,
									  name,
									  ap);
		return ISC_R_NOTFOUND;
	}

	if (!p -> outer || p -> outer -> type != omapi_type_connection)
		return ISC_R_INVALIDARG;
	c = p -> outer;

	/* We get here because we requested that we be woken up after
           some number of bytes were read, and that number of bytes
           has in fact been read. */
	switch (p -> state) {
	      case omapi_protocol_intro_wait:
		/* Get protocol version and header size in network
		   byte order. */
		omapi_connection_get_uint32 (c, &p -> protocol_version);
		omapi_connection_get_uint32 (c, &p -> header_size);
	
		/* We currently only support the current protocol version. */
		if (p -> protocol_version != OMAPI_PROTOCOL_VERSION) {
			omapi_disconnect (c, 1);
			return ISC_R_VERSIONMISMATCH;
		}

		if (p -> header_size < sizeof (omapi_protocol_header_t)) {
			omapi_disconnect (c, 1);
			return ISC_R_PROTOCOLERROR;
		}

		if (p -> default_auth) {
			status = omapi_protocol_send_open
				(h, (omapi_object_t *)0, "authenticator",
				 p -> default_auth -> a,
				 OMAPI_NOTIFY_PROTOCOL);
			if (status != ISC_R_SUCCESS) {
				omapi_disconnect (c, 1);
				return status;
			}
		} else {
			status = omapi_signal_in (h -> inner, "ready");
		}

	      to_header_wait:
		/* The next thing we're expecting is a message header. */
		p -> state = omapi_protocol_header_wait;

		/* Register a need for the number of bytes in a
		   header, and if we already have that many, process
		   them immediately. */
		if ((omapi_connection_require (c, p -> header_size)) !=
		    ISC_R_SUCCESS)
			break;
		/* If we already have the data, fall through. */

	      case omapi_protocol_header_wait:
#if defined (DEBUG_MEMORY_LEAKAGE)
		if (previous_outstanding != 0xDEADBEEF) {
			log_info ("%s %ld: %ld new, %ld outstanding, %ld%s",
				  "generation", dmalloc_generation,
				  dmalloc_outstanding - previous_outstanding,
				  dmalloc_outstanding, dmalloc_longterm,
				  " long-term");
#endif
#if (defined (DEBUG_MEMORY_LEAKAGE) || defined (DEBUG_MALLOC_POOL))
			dmalloc_dump_outstanding ();
#endif
#if defined (DEBUG_RC_HISTORY_EXHAUSTIVELY)
			dump_rc_history ();
#endif
#if defined (DEBUG_MEMORY_LEAKAGE)
		}
		previous_outstanding = dmalloc_outstanding;
#endif
		status = omapi_message_new ((omapi_object_t **)&p -> message,
					    MDL);
		if (status != ISC_R_SUCCESS) {
			omapi_disconnect (c, 1);
			return status;
		}

		p -> verify_result = ISC_R_SUCCESS;

		/* Swap in the header... */
		omapi_connection_get_uint32 (c, &p -> message -> authid);

		/* Bind the authenticator to the message object. */
		if (p -> message -> authid) {
			status = (omapi_protocol_lookup_auth
				  (&p -> message -> id_object, h,
				   p -> message -> authid));
			if (status != ISC_R_SUCCESS)
				p -> verify_result = status;

			/* Activate the authentication key. */
			status = omapi_set_object_value
				(c, (omapi_object_t *)0, "input-authenticator",
				 p -> message -> id_object);
			if (status != ISC_R_SUCCESS) {
				omapi_disconnect (c, 1);
				return status;
			}
		}

		omapi_connection_get_uint32 (c, &p -> message -> authlen);
		omapi_connection_get_uint32 (c, &p -> message -> op);
		omapi_connection_get_uint32 (c, &th);
		p -> message -> h = th;
		omapi_connection_get_uint32 (c, &p -> message -> id);
		omapi_connection_get_uint32 (c, &p -> message -> rid);

		/* If there was any extra header data, skip over it. */
		if (p -> header_size > sizeof (omapi_protocol_header_t)) {
			omapi_connection_copyout
				(0, c, (p -> header_size -
					sizeof (omapi_protocol_header_t)));
		}
						     
		/* XXX must compute partial signature across the
                   XXX preceding bytes.    Also, if authenticator
		   specifies encryption as well as signing, we may
		   have to decrypt the data on the way in. */

		/* First we read in message-specific values, then object
		   values. */
		p -> reading_message_values = 1;

	      need_name_length:
		/* The next thing we're expecting is length of the
		   first name. */
		p -> state = omapi_protocol_name_length_wait;

		/* Wait for a 16-bit length. */
		if ((omapi_connection_require (c, 2)) != ISC_R_SUCCESS)
			break;
		/* If it's already here, fall through. */

	      case omapi_protocol_name_length_wait:
		omapi_connection_get_uint16 (c, &nlen);
		/* A zero-length name means that we're done reading name+value
		   pairs. */
		if (nlen == 0) {
			/* If we've already read in the object, we are
			   done reading the message, but if we've just
			   finished reading in the values associated
			   with the message, we need to read the
			   object. */
			if (p -> reading_message_values) {
				p -> reading_message_values = 0;
				goto need_name_length;
			}

			/* If the authenticator length is zero, there's no
			   signature to read in, so go straight to processing
			   the message. */
			if (p -> message -> authlen == 0)
				goto message_done;

			/* The next thing we're expecting is the
                           message signature. */
			p -> state = omapi_protocol_signature_wait;

			/* Wait for the number of bytes specified for
			   the authenticator.  If we already have it,
			   go read it in. */
			if (omapi_connection_require
			    (c, p -> message -> authlen) == ISC_R_SUCCESS)
				goto signature_wait;
			break;
		}

		/* Allocate a buffer for the name. */
		status = (omapi_data_string_new (&p -> name, nlen, MDL));
		if (status != ISC_R_SUCCESS) {
			omapi_disconnect (c, 1);
			return ISC_R_NOMEMORY;
		}
		p -> state = omapi_protocol_name_wait;
		if (omapi_connection_require (c, nlen) != ISC_R_SUCCESS)
			break;
		/* If it's already here, fall through. */
					     
	      case omapi_protocol_name_wait:
		omapi_connection_copyout (p -> name -> value, c,
					  p -> name -> len);
		/* Wait for a 32-bit length. */
		p -> state = omapi_protocol_value_length_wait;
		if ((omapi_connection_require (c, 4)) != ISC_R_SUCCESS)
			break;
		/* If it's already here, fall through. */

	      case omapi_protocol_value_length_wait:
		omapi_connection_get_uint32 (c, &vlen);

		/* Zero-length values are allowed - if we get one, we
		   don't have to read any data for the value - just
		   get the next one, if there is a next one. */
		if (!vlen)
			goto insert_new_value;

		status = omapi_typed_data_new (MDL, &p -> value,
					       omapi_datatype_data,
					       vlen);
		if (status != ISC_R_SUCCESS) {
			omapi_disconnect (c, 1);
			return ISC_R_NOMEMORY;
		}

		p -> state = omapi_protocol_value_wait;
		if (omapi_connection_require (c, vlen) != ISC_R_SUCCESS)
			break;
		/* If it's already here, fall through. */
					     
	      case omapi_protocol_value_wait:
		omapi_connection_copyout (p -> value -> u.buffer.value, c,
					  p -> value -> u.buffer.len);

	      insert_new_value:
		if (p -> reading_message_values) {
			status = (omapi_set_value
				  ((omapi_object_t *)p -> message,
				   p -> message -> id_object,
				   p -> name, p -> value));
		} else {
			if (!p -> message -> object) {
				/* We need a generic object to hang off of the
				   incoming message. */
				status = (omapi_generic_new
					  (&p -> message -> object, MDL));
				if (status != ISC_R_SUCCESS) {
					omapi_disconnect (c, 1);
					return status;
				}
			}
			status = (omapi_set_value
				  ((omapi_object_t *)p -> message -> object,
				   p -> message -> id_object,
				   p -> name, p -> value));
		}
		if (status != ISC_R_SUCCESS) {
			omapi_disconnect (c, 1);
			return status;
		}
		omapi_data_string_dereference (&p -> name, MDL);
		if (p -> value)
			omapi_typed_data_dereference (&p -> value, MDL);
		goto need_name_length;

	      signature_wait:
	      case omapi_protocol_signature_wait:
		if (p -> message -> id_object) {
			/* Compute the signature of the message. */
			signature = (omapi_value_t *)0;
			status = omapi_get_value_str (c, (omapi_object_t *)0,
						      "input-signature",
						      &signature);
			if (status != ISC_R_SUCCESS) {
				omapi_disconnect (c, 1);
				return status;
			}

			/* Disable the authentication key on the connection. */
			status = omapi_set_value_str (c, (omapi_object_t *)0,
						      "input-authenticator",
						      (omapi_typed_data_t *)0);
			if (status != ISC_R_SUCCESS) {
				omapi_value_dereference (&signature, MDL);
				omapi_disconnect (c, 1);
				return status;
			}
		}

		/* Read the authenticator. */
		status = omapi_typed_data_new (MDL,
					       &p -> message -> authenticator,
					       omapi_datatype_data,
					       p -> message -> authlen);
			
		if (status != ISC_R_SUCCESS) {
			omapi_value_dereference (&signature, MDL);
			omapi_disconnect (c, 1);
			return ISC_R_NOMEMORY;
		}
		omapi_connection_copyout
			(p -> message -> authenticator -> u.buffer.value, c,
			 p -> message -> authlen);

		/* Verify the signature. */
		if (p -> message -> id_object &&
		    ((signature -> value -> u.buffer.len !=
		      p -> message -> authlen) ||
		     (memcmp (signature -> value -> u.buffer.value,
			      p -> message -> authenticator -> u.buffer.value,
			      p -> message -> authlen) != 0))) {
			/* Invalid signature. */
			p -> verify_result = ISC_R_INVALIDKEY;
		}

		omapi_value_dereference (&signature, MDL);

		/* Process the message. */
	      message_done:
		if (p -> verify_result != ISC_R_SUCCESS) {
			status = omapi_protocol_send_status
				(h, (omapi_object_t *)0, p -> verify_result,
				 p -> message -> id, (char *)0);
		} else {
			status = omapi_message_process
				((omapi_object_t *)p -> message, h);
		}
		if (status != ISC_R_SUCCESS) {
			omapi_disconnect (c, 1);
			return ISC_R_NOMEMORY;
		}

		omapi_message_dereference (&p -> message, MDL);
#if defined (DEBUG_MEMORY_LEAKAGE)
		log_info ("generation %ld: %ld new, %ld outstanding, %ld%s",
			  dmalloc_generation,
			  dmalloc_outstanding - previous_outstanding,
			  dmalloc_outstanding, dmalloc_longterm, " long-term");
#endif
#if (defined (DEBUG_MEMORY_LEAKAGE) || defined (DEBUG_MALLOC_POOL))
		dmalloc_dump_outstanding ();
#endif
#if defined (DEBUG_RC_HISTORY_EXHAUSTIVELY)
		dump_rc_history ();
#endif
#if defined (DEBUG_MEMORY_LEAKAGE)
		previous_outstanding = 0xDEADBEEF;
#endif
		/* Now wait for the next message. */
		goto to_header_wait;		

	      default:
		/* XXX should never get here.   Assertion? */
		break;
	}
	return ISC_R_SUCCESS;
}

isc_result_t omapi_protocol_add_auth (omapi_object_t *po,
				      omapi_object_t *ao,
				      omapi_handle_t handle)
{
	omapi_protocol_object_t *p;
	omapi_remote_auth_t *r;
	isc_result_t status;

	if (ao -> type != omapi_type_auth_key &&
	    (!ao -> inner || ao -> inner -> type != omapi_type_auth_key))
		return ISC_R_INVALIDARG;

	if (po -> type != omapi_type_protocol)
		return ISC_R_INVALIDARG;
	p = (omapi_protocol_object_t *)po;

#ifdef DEBUG_PROTOCOL
	log_debug ("omapi_protocol_add_auth(name=%s)",
		   ((omapi_auth_key_t *)ao) -> name);
#endif

	if (p -> verify_auth) {
		status = (p -> verify_auth) (po, (omapi_auth_key_t *)ao);
		if (status != ISC_R_SUCCESS)
			return status;
	}

	/* If omapi_protocol_connect() was called with a default
	   authenticator, p -> default_auth will already be set,
	   but p -> remote_auth_list will not yet be initialized. */
	if (p -> default_auth && !p -> remote_auth_list) {
		if (p -> default_auth -> a != ao) {
			/* Something just went horribly wrong. */
			omapi_disconnect (p -> outer, 1);
			return ISC_R_UNEXPECTED;
		}

		p -> remote_auth_list = p -> default_auth;
		p -> default_auth -> remote_handle = handle;

		return omapi_signal_in (p -> inner, "ready");
	}

	r = dmalloc (sizeof(*r), MDL);
	if (!r)
		return ISC_R_NOMEMORY;

	status = omapi_object_reference (&r -> a, ao, MDL);
	if (status != ISC_R_SUCCESS) {
		dfree (r, MDL);
		return status;
	}

	r -> remote_handle = handle;
	r -> next = p -> remote_auth_list;
	p -> remote_auth_list = r;

	return ISC_R_SUCCESS;
}

isc_result_t omapi_protocol_lookup_auth (omapi_object_t **a,
					 omapi_object_t *po,
					 omapi_handle_t handle)
{
	omapi_protocol_object_t *p;
	omapi_remote_auth_t *r;

	if (po -> type != omapi_type_protocol)
		return ISC_R_INVALIDARG;
	p = (omapi_protocol_object_t *)po;

	for (r = p -> remote_auth_list; r; r = r -> next)
		if (r -> remote_handle == handle)
			return omapi_object_reference (a, r -> a, MDL);

	return ISC_R_KEY_UNKNOWN;
}

isc_result_t omapi_protocol_set_value (omapi_object_t *h,
				       omapi_object_t *id,
				       omapi_data_string_t *name,
				       omapi_typed_data_t *value)
{
	omapi_protocol_object_t *p;
	omapi_remote_auth_t *r;

	if (h -> type != omapi_type_protocol)
		return ISC_R_INVALIDARG;
	p = (omapi_protocol_object_t *)h;

	if (omapi_ds_strcmp (name, "default-authenticator") == 0) {
		if (value -> type != omapi_datatype_object)
			return ISC_R_INVALIDARG;

		if (!value || !value -> u.object) {
			p -> default_auth = (omapi_remote_auth_t *)0;
		} else {
			for (r = p -> remote_auth_list; r; r = r -> next)
				if (r -> a == value -> u.object)
					break;

			if (!r)
				return ISC_R_KEY_UNKNOWN;

			p -> default_auth = r;
		}

		return ISC_R_SUCCESS;
	}

	if (h -> inner && h -> inner -> type -> set_value)
		return (*(h -> inner -> type -> set_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t omapi_protocol_get_value (omapi_object_t *h,
				       omapi_object_t *id,
				       omapi_data_string_t *name,
				       omapi_value_t **value)
{
	omapi_protocol_object_t *p;

	if (h -> type != omapi_type_protocol)
		return ISC_R_INVALIDARG;
	p = (omapi_protocol_object_t *)h;

	if (omapi_ds_strcmp (name, "default-authenticator") == 0) {
		if (!p -> default_auth)
			return ISC_R_NOTFOUND;

		return omapi_make_object_value (value, name,
						p -> default_auth -> a, MDL);
	}
	
	if (h -> inner && h -> inner -> type -> get_value)
		return (*(h -> inner -> type -> get_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t omapi_protocol_destroy (omapi_object_t *h,
				     const char *file, int line)
{
	omapi_protocol_object_t *p;
	if (h -> type != omapi_type_protocol)
		return ISC_R_INVALIDARG;
	p = (omapi_protocol_object_t *)h;
	if (p -> message)
		omapi_message_dereference (&p -> message, file, line);

	/* This will happen if: 1) A default authenticator is supplied to
	   omapi_protocol_connect(), and 2) something goes wrong before
	   the authenticator can be opened. */
	if (p -> default_auth && !p -> remote_auth_list)
		dfree (p -> default_auth, file, line);

	while (p -> remote_auth_list) {
		omapi_remote_auth_t *r = p -> remote_auth_list -> next;
		p -> remote_auth_list = r;
		if (r) {
			omapi_object_dereference (&r -> a, file, line);
			dfree (r, file, line);
		}
	}
	return ISC_R_SUCCESS;
}

/* Write all the published values associated with the object through the
   specified connection. */

isc_result_t omapi_protocol_stuff_values (omapi_object_t *c,
					  omapi_object_t *id,
					  omapi_object_t *p)
{
	int i;

	if (p -> type != omapi_type_protocol)
		return ISC_R_INVALIDARG;

	if (p -> inner && p -> inner -> type -> stuff_values)
		return (*(p -> inner -> type -> stuff_values)) (c, id,
								p -> inner);
	return ISC_R_SUCCESS;
}

/* Returns a boolean indicating whether this protocol requires that
   messages be authenticated or not. */

isc_boolean_t omapi_protocol_authenticated (omapi_object_t *h)
{
	if (h -> type != omapi_type_protocol)
		return isc_boolean_false;
	if (((omapi_protocol_object_t *)h) -> insecure)
		return isc_boolean_false;
	else
		return isc_boolean_true;
}

/* Sets the address and authenticator verification callbacks.  The handle
   is to a listener object, not a protocol object. */

isc_result_t omapi_protocol_configure_security (omapi_object_t *h,
						isc_result_t (*verify_addr)
						 (omapi_object_t *,
						  omapi_addr_t *),
						isc_result_t (*verify_auth)
						 (omapi_object_t *,
						  omapi_auth_key_t *))
{
	omapi_protocol_listener_object_t *l;

	if (h -> outer && h -> outer -> type == omapi_type_protocol_listener)
		h = h -> outer;

	if (h -> type != omapi_type_protocol_listener)
		return ISC_R_INVALIDARG;
	l = (omapi_protocol_listener_object_t *)h;

	l -> verify_auth = verify_auth;
	l -> insecure = 0;

	return omapi_listener_configure_security (h -> outer, verify_addr);
}
					      

/* Set up a listener for the omapi protocol.    The handle stored points to
   a listener object, not a protocol object. */

isc_result_t omapi_protocol_listen (omapi_object_t *h,
				    unsigned port,
				    int max)
{
	isc_result_t status;
	omapi_protocol_listener_object_t *obj;

	obj = (omapi_protocol_listener_object_t *)0;
	status = omapi_protocol_listener_allocate (&obj, MDL);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_object_reference (&h -> outer,
					 (omapi_object_t *)obj, MDL);
	if (status != ISC_R_SUCCESS) {
		omapi_protocol_listener_dereference (&obj, MDL);
		return status;
	}
	status = omapi_object_reference (&obj -> inner, h, MDL);
	if (status != ISC_R_SUCCESS) {
		omapi_protocol_listener_dereference (&obj, MDL);
		return status;
	}

	/* What a terrible default. */
	obj -> insecure = 1;

	status = omapi_listen ((omapi_object_t *)obj, port, max);
	omapi_protocol_listener_dereference (&obj, MDL);
	return status;
}

/* Signal handler for protocol listener - if we get a connect signal,
   create a new protocol connection, otherwise pass the signal down. */

isc_result_t omapi_protocol_listener_signal (omapi_object_t *o,
					     const char *name, va_list ap)
{
	isc_result_t status;
	omapi_object_t *c;
	omapi_protocol_object_t *obj;
	omapi_protocol_listener_object_t *p;

	if (!o || o -> type != omapi_type_protocol_listener)
		return ISC_R_INVALIDARG;
	p = (omapi_protocol_listener_object_t *)o;

	/* Not a signal we recognize? */
	if (strcmp (name, "connect")) {
		if (p -> inner && p -> inner -> type -> signal_handler)
			return (*(p -> inner -> type -> signal_handler))
				(p -> inner, name, ap);
		return ISC_R_NOTFOUND;
	}

	c = va_arg (ap, omapi_object_t *);
	if (!c || c -> type != omapi_type_connection)
		return ISC_R_INVALIDARG;

	obj = (omapi_protocol_object_t *)0;
	status = omapi_protocol_allocate (&obj, MDL);
	if (status != ISC_R_SUCCESS)
		return status;

	obj -> verify_auth = p -> verify_auth;
	obj -> insecure = p -> insecure;

	status = omapi_object_reference (&obj -> outer, c, MDL);
	if (status != ISC_R_SUCCESS) {
	      lose:
		omapi_protocol_dereference (&obj, MDL);
		omapi_disconnect (c, 1);
		return status;
	}

	status = omapi_object_reference (&c -> inner,
					 (omapi_object_t *)obj, MDL);
	if (status != ISC_R_SUCCESS)
		goto lose;

	/* Send the introductory message. */
	status = omapi_protocol_send_intro ((omapi_object_t *)obj,
					    OMAPI_PROTOCOL_VERSION,
					    sizeof (omapi_protocol_header_t));
	if (status != ISC_R_SUCCESS)
		goto lose;

	omapi_protocol_dereference (&obj, MDL);
	return status;
}

isc_result_t omapi_protocol_listener_set_value (omapi_object_t *h,
						omapi_object_t *id,
						omapi_data_string_t *name,
						omapi_typed_data_t *value)
{
	if (h -> type != omapi_type_protocol_listener)
		return ISC_R_INVALIDARG;
	
	if (h -> inner && h -> inner -> type -> set_value)
		return (*(h -> inner -> type -> set_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t omapi_protocol_listener_get_value (omapi_object_t *h,
						omapi_object_t *id,
						omapi_data_string_t *name,
						omapi_value_t **value)
{
	if (h -> type != omapi_type_protocol_listener)
		return ISC_R_INVALIDARG;
	
	if (h -> inner && h -> inner -> type -> get_value)
		return (*(h -> inner -> type -> get_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t omapi_protocol_listener_destroy (omapi_object_t *h,
					      const char *file, int line)
{
	if (h -> type != omapi_type_protocol_listener)
		return ISC_R_INVALIDARG;
	return ISC_R_SUCCESS;
}

/* Write all the published values associated with the object through the
   specified connection. */

isc_result_t omapi_protocol_listener_stuff (omapi_object_t *c,
					    omapi_object_t *id,
					    omapi_object_t *p)
{
	int i;

	if (p -> type != omapi_type_protocol_listener)
		return ISC_R_INVALIDARG;

	if (p -> inner && p -> inner -> type -> stuff_values)
		return (*(p -> inner -> type -> stuff_values)) (c, id,
								p -> inner);
	return ISC_R_SUCCESS;
}

isc_result_t omapi_protocol_send_status (omapi_object_t *po,
					 omapi_object_t *id,
					 isc_result_t waitstatus,
					 unsigned rid, const char *msg)
{
	isc_result_t status;
	omapi_message_object_t *message = (omapi_message_object_t *)0;
	omapi_object_t *mo;

	if (po -> type != omapi_type_protocol)
		return ISC_R_INVALIDARG;

	status = omapi_message_new ((omapi_object_t **)&message, MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	mo = (omapi_object_t *)message;

	status = omapi_set_int_value (mo, (omapi_object_t *)0,
				      "op", OMAPI_OP_STATUS);
	if (status != ISC_R_SUCCESS) {
		omapi_message_dereference (&message, MDL);
		return status;
	}

	status = omapi_set_int_value (mo, (omapi_object_t *)0,
				      "rid", (int)rid);
	if (status != ISC_R_SUCCESS) {
		omapi_message_dereference (&message, MDL);
		return status;
	}

	status = omapi_set_int_value (mo, (omapi_object_t *)0,
				      "result", (int)waitstatus);
	if (status != ISC_R_SUCCESS) {
		omapi_message_dereference (&message, MDL);
		return status;
	}

	/* If a message has been provided, send it. */
	if (msg) {
		status = omapi_set_string_value (mo, (omapi_object_t *)0,
						 "message", msg);
		if (status != ISC_R_SUCCESS) {
			omapi_message_dereference (&message, MDL);
			return status;
		}
	}

	status = omapi_protocol_send_message (po, id, mo, (omapi_object_t *)0);
	omapi_message_dereference (&message, MDL);
	return status;
}

/* The OMAPI_NOTIFY_PROTOCOL flag will cause the notify-object for the
   message to be set to the protocol object.  This is used when opening
   the default authenticator. */

isc_result_t omapi_protocol_send_open (omapi_object_t *po,
				       omapi_object_t *id,
				       const char *type,
				       omapi_object_t *object,
				       unsigned flags)
{
	isc_result_t status;
	omapi_message_object_t *message = (omapi_message_object_t *)0;
	omapi_object_t *mo;

	if (po -> type != omapi_type_protocol)
		return ISC_R_INVALIDARG;

	status = omapi_message_new ((omapi_object_t **)&message, MDL);
	mo = (omapi_object_t *)message;

	if (status == ISC_R_SUCCESS)
		status = omapi_set_int_value (mo, (omapi_object_t *)0,
					      "op", OMAPI_OP_OPEN);

	if (status == ISC_R_SUCCESS)
		status = omapi_set_object_value (mo, (omapi_object_t *)0,
						 "object", object);

	if ((flags & OMAPI_CREATE) && (status == ISC_R_SUCCESS))
		status = omapi_set_boolean_value (mo, (omapi_object_t *)0,
						  "create", 1);

	if ((flags & OMAPI_UPDATE) && (status == ISC_R_SUCCESS))
		status = omapi_set_boolean_value (mo, (omapi_object_t *)0,
						  "update", 1);

	if ((flags & OMAPI_EXCL) && (status == ISC_R_SUCCESS))
		status = omapi_set_boolean_value (mo, (omapi_object_t *)0,
						  "exclusive", 1);

	if ((flags & OMAPI_NOTIFY_PROTOCOL) && (status == ISC_R_SUCCESS))
		status = omapi_set_object_value (mo, (omapi_object_t *)0,
						 "notify-object", po);

	if (type && (status == ISC_R_SUCCESS))
		status = omapi_set_string_value (mo, (omapi_object_t *)0,
						 "type", type);

	if (status == ISC_R_SUCCESS)
		status = omapi_message_register (mo);

	if (status == ISC_R_SUCCESS) {
		status = omapi_protocol_send_message (po, id, mo,
						      (omapi_object_t *)0);
		if (status != ISC_R_SUCCESS)
			omapi_message_unregister (mo);
	}

	if (message)
		omapi_message_dereference (&message, MDL);

	return status;
}

isc_result_t omapi_protocol_send_update (omapi_object_t *po,
					 omapi_object_t *id,
					 unsigned rid,
					 omapi_object_t *object)
{
	isc_result_t status;
	omapi_message_object_t *message = (omapi_message_object_t *)0;
	omapi_object_t *mo;

	if (po -> type != omapi_type_protocol)
		return ISC_R_INVALIDARG;

	status = omapi_message_new ((omapi_object_t **)&message, MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	mo = (omapi_object_t *)message;

	status = omapi_set_int_value (mo, (omapi_object_t *)0,
				      "op", OMAPI_OP_UPDATE);
	if (status != ISC_R_SUCCESS) {
		omapi_message_dereference (&message, MDL);
		return status;
	}

	if (rid) {
		omapi_handle_t handle;
		status = omapi_set_int_value (mo, (omapi_object_t *)0,
					      "rid", (int)rid);
		if (status != ISC_R_SUCCESS) {
			omapi_message_dereference (&message, MDL);
			return status;
		}

		status = omapi_object_handle (&handle, object);
		if (status != ISC_R_SUCCESS) {
			omapi_message_dereference (&message, MDL);
			return status;
		}
		status = omapi_set_int_value (mo, (omapi_object_t *)0,
					      "handle", (int)handle);
		if (status != ISC_R_SUCCESS) {
			omapi_message_dereference (&message, MDL);
			return status;
		}
	}		
		
	status = omapi_set_object_value (mo, (omapi_object_t *)0,
					 "object", object);
	if (status != ISC_R_SUCCESS) {
		omapi_message_dereference (&message, MDL);
		return status;
	}

	status = omapi_protocol_send_message (po, id, mo, (omapi_object_t *)0);
	omapi_message_dereference (&message, MDL);
	return status;
}
