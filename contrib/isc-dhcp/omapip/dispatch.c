/* dispatch.c

   I/O dispatcher. */

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

static omapi_io_object_t omapi_io_states;
u_int32_t cur_time;

OMAPI_OBJECT_ALLOC (omapi_io,
		    omapi_io_object_t, omapi_type_io_object)
OMAPI_OBJECT_ALLOC (omapi_waiter,
		    omapi_waiter_object_t, omapi_type_waiter)

/* Register an I/O handle so that we can do asynchronous I/O on it. */

isc_result_t omapi_register_io_object (omapi_object_t *h,
				       int (*readfd) (omapi_object_t *),
				       int (*writefd) (omapi_object_t *),
				       isc_result_t (*reader)
						(omapi_object_t *),
				       isc_result_t (*writer)
						(omapi_object_t *),
				       isc_result_t (*reaper)
						(omapi_object_t *))
{
	isc_result_t status;
	omapi_io_object_t *obj, *p;

	/* omapi_io_states is a static object.   If its reference count
	   is zero, this is the first I/O handle to be registered, so
	   we need to initialize it.   Because there is no inner or outer
	   pointer on this object, and we're setting its refcnt to 1, it
	   will never be freed. */
	if (!omapi_io_states.refcnt) {
		omapi_io_states.refcnt = 1;
		omapi_io_states.type = omapi_type_io_object;
	}
		
	obj = (omapi_io_object_t *)0;
	status = omapi_io_allocate (&obj, MDL);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_object_reference (&obj -> inner, h, MDL);
	if (status != ISC_R_SUCCESS) {
		omapi_io_dereference (&obj, MDL);
		return status;
	}

	status = omapi_object_reference (&h -> outer,
					 (omapi_object_t *)obj, MDL);
	if (status != ISC_R_SUCCESS) {
		omapi_io_dereference (&obj, MDL);
		return status;
	}

	/* Find the last I/O state, if there are any. */
	for (p = omapi_io_states.next;
	     p && p -> next; p = p -> next)
		;
	if (p)
		omapi_io_reference (&p -> next, obj, MDL);
	else
		omapi_io_reference (&omapi_io_states.next, obj, MDL);

	obj -> readfd = readfd;
	obj -> writefd = writefd;
	obj -> reader = reader;
	obj -> writer = writer;
	obj -> reaper = reaper;
	return ISC_R_SUCCESS;
}

isc_result_t omapi_unregister_io_object (omapi_object_t *h)
{
	omapi_io_object_t *p, *obj, *last, *ph;

	if (!h -> outer || h -> outer -> type != omapi_type_io_object)
		return ISC_R_INVALIDARG;
	obj = (omapi_io_object_t *)h -> outer;
	ph = (omapi_io_object_t *)0;
	omapi_io_reference (&ph, obj, MDL);

	/* remove from the list of I/O states */
        last = &omapi_io_states;
	for (p = omapi_io_states.next; p; p = p -> next) {
		if (p == obj) {
			omapi_io_dereference (&last -> next, MDL);
			omapi_io_reference (&last -> next, p -> next, MDL);
			break;
		}
		last = p;
	}
	if (obj -> next)
		omapi_io_dereference (&obj -> next, MDL);

	if (obj -> outer) {
		if (obj -> outer -> inner == (omapi_object_t *)obj)
			omapi_object_dereference (&obj -> outer -> inner,
						  MDL);
		omapi_object_dereference (&obj -> outer, MDL);
	}
	omapi_object_dereference (&obj -> inner, MDL);
	omapi_object_dereference (&h -> outer, MDL);
	omapi_io_dereference (&ph, MDL);
	return ISC_R_SUCCESS;
}

isc_result_t omapi_dispatch (struct timeval *t)
{
	return omapi_wait_for_completion ((omapi_object_t *)&omapi_io_states,
					  t);
}

isc_result_t omapi_wait_for_completion (omapi_object_t *object,
					struct timeval *t)
{
	isc_result_t status;
	omapi_waiter_object_t *waiter;
	omapi_object_t *inner;

	if (object) {
		waiter = (omapi_waiter_object_t *)0;
		status = omapi_waiter_allocate (&waiter, MDL);
		if (status != ISC_R_SUCCESS)
			return status;

		/* Paste the waiter object onto the inner object we're
		   waiting on. */
		for (inner = object; inner -> inner; inner = inner -> inner)
			;

		status = omapi_object_reference (&waiter -> outer, inner, MDL);
		if (status != ISC_R_SUCCESS) {
			omapi_waiter_dereference (&waiter, MDL);
			return status;
		}
		
		status = omapi_object_reference (&inner -> inner,
						 (omapi_object_t *)waiter,
						 MDL);
		if (status != ISC_R_SUCCESS) {
			omapi_waiter_dereference (&waiter, MDL);
			return status;
		}
	} else
		waiter = (omapi_waiter_object_t *)0;

	do {
		status = omapi_one_dispatch ((omapi_object_t *)waiter, t);
		if (status != ISC_R_SUCCESS)
			return status;
	} while (!waiter || !waiter -> ready);

	if (waiter -> outer) {
		if (waiter -> outer -> inner) {
			omapi_object_dereference (&waiter -> outer -> inner,
						  MDL);
			if (waiter -> inner)
				omapi_object_reference
					(&waiter -> outer -> inner,
					 waiter -> inner, MDL);
		}
		omapi_object_dereference (&waiter -> outer, MDL);
	}
	if (waiter -> inner)
		omapi_object_dereference (&waiter -> inner, MDL);
	
	status = waiter -> waitstatus;
	omapi_waiter_dereference (&waiter, MDL);
	return status;
}

isc_result_t omapi_one_dispatch (omapi_object_t *wo,
				 struct timeval *t)
{
	fd_set r, w, x;
	int max = 0;
	int count;
	int desc;
	struct timeval now, to;
	omapi_io_object_t *io, *prev;
	isc_result_t status;
	omapi_waiter_object_t *waiter;
	omapi_object_t *tmp = (omapi_object_t *)0;

	if (!wo || wo -> type != omapi_type_waiter)
		waiter = (omapi_waiter_object_t *)0;
	else
		waiter = (omapi_waiter_object_t *)wo;

	FD_ZERO (&x);

	/* First, see if the timeout has expired, and if so return. */
	if (t) {
		gettimeofday (&now, (struct timezone *)0);
		cur_time = now.tv_sec;
		if (now.tv_sec > t -> tv_sec ||
		    (now.tv_sec == t -> tv_sec && now.tv_usec >= t -> tv_usec))
			return ISC_R_TIMEDOUT;
			
		/* We didn't time out, so figure out how long until
		   we do. */
		to.tv_sec = t -> tv_sec - now.tv_sec;
		to.tv_usec = t -> tv_usec - now.tv_usec;
		if (to.tv_usec < 0) {
			to.tv_usec += 1000000;
			to.tv_sec--;
		}

		/* It is possible for the timeout to get set larger than
		   the largest time select() is willing to accept.
		   Restricting the timeout to a maximum of one day should
		   work around this.  -DPN.  (Ref: Bug #416) */
		if (to.tv_sec > (60 * 60 * 24))
			to.tv_sec = 60 * 60 * 24;
	}
	
	/* If the object we're waiting on has reached completion,
	   return now. */
	if (waiter && waiter -> ready)
		return ISC_R_SUCCESS;
	
      again:
	/* If we have no I/O state, we can't proceed. */
	if (!(io = omapi_io_states.next))
		return ISC_R_NOMORE;

	/* Set up the read and write masks. */
	FD_ZERO (&r);
	FD_ZERO (&w);

	for (; io; io = io -> next) {
		/* Check for a read socket.   If we shouldn't be
		   trying to read for this I/O object, either there
		   won't be a readfd function, or it'll return -1. */
		if (io -> readfd && io -> inner &&
		    (desc = (*(io -> readfd)) (io -> inner)) >= 0) {
			FD_SET (desc, &r);
			if (desc > max)
				max = desc;
		}
		
		/* Same deal for write fdets. */
		if (io -> writefd && io -> inner &&
		    (desc = (*(io -> writefd)) (io -> inner)) >= 0) {
			FD_SET (desc, &w);
			if (desc > max)
				max = desc;
		}
	}

	/* Wait for a packet or a timeout... XXX */
#if 0
#if defined (__linux__)
#define fds_bits __fds_bits
#endif
	log_error ("dispatch: %d %lx %lx", max,
		   (unsigned long)r.fds_bits [0],
		   (unsigned long)w.fds_bits [0]);
#endif
	count = select (max + 1, &r, &w, &x, t ? &to : (struct timeval *)0);

	/* Get the current time... */
	gettimeofday (&now, (struct timezone *)0);
	cur_time = now.tv_sec;

	/* We probably have a bad file descriptor.   Figure out which one.
	   When we find it, call the reaper function on it, which will
	   maybe make it go away, and then try again. */
	if (count < 0) {
		struct timeval t0;
		omapi_io_object_t *prev = (omapi_io_object_t *)0;
		io = (omapi_io_object_t *)0;
		if (omapi_io_states.next)
			omapi_io_reference (&io, omapi_io_states.next, MDL);

		while (io) {
			omapi_object_t *obj;
			FD_ZERO (&r);
			FD_ZERO (&w);
			t0.tv_sec = t0.tv_usec = 0;

			if (io -> readfd && io -> inner &&
			    (desc = (*(io -> readfd)) (io -> inner)) >= 0) {
			    FD_SET (desc, &r);
#if 0
			    log_error ("read check: %d %lx %lx", max,
				       (unsigned long)r.fds_bits [0],
				       (unsigned long)w.fds_bits [0]);
#endif
			    count = select (desc + 1, &r, &w, &x, &t0);
			   bogon:
			    if (count < 0) {
				log_error ("Bad descriptor %d.", desc);
				for (obj = (omapi_object_t *)io;
				     obj -> outer;
				     obj = obj -> outer)
					;
				for (; obj; obj = obj -> inner) {
				    omapi_value_t *ov;
				    int len;
				    const char *s;
				    ov = (omapi_value_t *)0;
				    omapi_get_value_str (obj,
							 (omapi_object_t *)0,
							 "name", &ov);
				    if (ov && ov -> value &&
					(ov -> value -> type ==
					 omapi_datatype_string)) {
					s = (char *)
						ov -> value -> u.buffer.value;
					len = ov -> value -> u.buffer.len;
				    } else {
					s = "";
					len = 0;
				    }
				    log_error ("Object %lx %s%s%.*s",
					       (unsigned long)obj,
					       obj -> type -> name,
					       len ? " " : "",
					       len, s);
				    if (len)
					omapi_value_dereference (&ov, MDL);
				}
				status = (*(io -> reaper)) (io -> inner);
				if (prev) {
				    omapi_io_dereference (&prev -> next, MDL);
				    if (io -> next)
					omapi_io_reference (&prev -> next,
							    io -> next, MDL);
				} else {
				    omapi_io_dereference
					    (&omapi_io_states.next, MDL);
				    if (io -> next)
					omapi_io_reference
						(&omapi_io_states.next,
						 io -> next, MDL);
				}
				omapi_io_dereference (&io, MDL);
				goto again;
			    }
			}
			
			FD_ZERO (&r);
			FD_ZERO (&w);
			t0.tv_sec = t0.tv_usec = 0;

			/* Same deal for write fdets. */
			if (io -> writefd && io -> inner &&
			    (desc = (*(io -> writefd)) (io -> inner)) >= 0) {
				FD_SET (desc, &w);
				count = select (desc + 1, &r, &w, &x, &t0);
				if (count < 0)
					goto bogon;
			}
			if (prev)
				omapi_io_dereference (&prev, MDL);
			omapi_io_reference (&prev, io, MDL);
			omapi_io_dereference (&io, MDL);
			if (prev -> next)
			    omapi_io_reference (&io, prev -> next, MDL);
		}
		if (prev)
			omapi_io_dereference (&prev, MDL);
		
	}

	for (io = omapi_io_states.next; io; io = io -> next) {
		if (!io -> inner)
			continue;
		omapi_object_reference (&tmp, io -> inner, MDL);
		/* Check for a read descriptor, and if there is one,
		   see if we got input on that socket. */
		if (io -> readfd &&
		    (desc = (*(io -> readfd)) (tmp)) >= 0) {
			if (FD_ISSET (desc, &r))
				status = ((*(io -> reader)) (tmp));
				/* XXX what to do with status? */
		}
		
		/* Same deal for write descriptors. */
		if (io -> writefd &&
		    (desc = (*(io -> writefd)) (tmp)) >= 0)
		{
			if (FD_ISSET (desc, &w))
				status = ((*(io -> writer)) (tmp));
				/* XXX what to do with status? */
		}
		omapi_object_dereference (&tmp, MDL);
	}

	/* Now check for I/O handles that are no longer valid,
	   and remove them from the list. */
	prev = (omapi_io_object_t *)0;
	for (io = omapi_io_states.next; io; io = io -> next) {
		if (io -> reaper) {
			if (io -> inner)
				status = (*(io -> reaper)) (io -> inner);
			if (!io -> inner || status != ISC_R_SUCCESS) {
				omapi_io_object_t *tmp =
					(omapi_io_object_t *)0;
				/* Save a reference to the next
				   pointer, if there is one. */
				if (io -> next)
					omapi_io_reference (&tmp,
							    io -> next, MDL);
				if (prev) {
					omapi_io_dereference (&prev -> next,
							      MDL);
					if (tmp)
						omapi_io_reference
							(&prev -> next,
							 tmp, MDL);
				} else {
					omapi_io_dereference
						(&omapi_io_states.next, MDL);
					if (tmp)
						omapi_io_reference
						    (&omapi_io_states.next,
						     tmp, MDL);
					else
						omapi_signal_in
							((omapi_object_t *)
							 &omapi_io_states,
							 "ready");
				}
				if (tmp)
					omapi_io_dereference (&tmp, MDL);
			}
		}
		prev = io;
	}

	return ISC_R_SUCCESS;
}

isc_result_t omapi_io_set_value (omapi_object_t *h,
				 omapi_object_t *id,
				 omapi_data_string_t *name,
				 omapi_typed_data_t *value)
{
	if (h -> type != omapi_type_io_object)
		return ISC_R_INVALIDARG;
	
	if (h -> inner && h -> inner -> type -> set_value)
		return (*(h -> inner -> type -> set_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t omapi_io_get_value (omapi_object_t *h,
				 omapi_object_t *id,
				 omapi_data_string_t *name,
				 omapi_value_t **value)
{
	if (h -> type != omapi_type_io_object)
		return ISC_R_INVALIDARG;
	
	if (h -> inner && h -> inner -> type -> get_value)
		return (*(h -> inner -> type -> get_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t omapi_io_destroy (omapi_object_t *h, const char *file, int line)
{
	omapi_io_object_t *obj, *p, *last;

	if (h -> type != omapi_type_io_object)
		return ISC_R_INVALIDARG;
	
	obj = (omapi_io_object_t *)h;

	/* remove from the list of I/O states */
	for (p = omapi_io_states.next; p; p = p -> next) {
		if (p == obj) {
			omapi_io_dereference (&last -> next, MDL);
			omapi_io_reference (&last -> next, p -> next, MDL);
			omapi_io_dereference (&p, MDL);
			break;
		}
		last = p;
	}
		
	return ISC_R_SUCCESS;
}

isc_result_t omapi_io_signal_handler (omapi_object_t *h,
				      const char *name, va_list ap)
{
	if (h -> type != omapi_type_io_object)
		return ISC_R_INVALIDARG;
	
	if (h -> inner && h -> inner -> type -> signal_handler)
		return (*(h -> inner -> type -> signal_handler)) (h -> inner,
								  name, ap);
	return ISC_R_NOTFOUND;
}

isc_result_t omapi_io_stuff_values (omapi_object_t *c,
				    omapi_object_t *id,
				    omapi_object_t *i)
{
	if (i -> type != omapi_type_io_object)
		return ISC_R_INVALIDARG;

	if (i -> inner && i -> inner -> type -> stuff_values)
		return (*(i -> inner -> type -> stuff_values)) (c, id,
								i -> inner);
	return ISC_R_SUCCESS;
}

isc_result_t omapi_waiter_signal_handler (omapi_object_t *h,
					  const char *name, va_list ap)
{
	omapi_waiter_object_t *waiter;

	if (h -> type != omapi_type_waiter)
		return ISC_R_INVALIDARG;
	
	if (!strcmp (name, "ready")) {
		waiter = (omapi_waiter_object_t *)h;
		waiter -> ready = 1;
		waiter -> waitstatus = ISC_R_SUCCESS;
		return ISC_R_SUCCESS;
	}

	if (!strcmp (name, "status")) {
		waiter = (omapi_waiter_object_t *)h;
		waiter -> ready = 1;
		waiter -> waitstatus = va_arg (ap, isc_result_t);
		return ISC_R_SUCCESS;
	}

	if (!strcmp (name, "disconnect")) {
		waiter = (omapi_waiter_object_t *)h;
		waiter -> ready = 1;
		waiter -> waitstatus = ISC_R_CONNRESET;
		return ISC_R_SUCCESS;
	}

	if (h -> inner && h -> inner -> type -> signal_handler)
		return (*(h -> inner -> type -> signal_handler)) (h -> inner,
								  name, ap);
	return ISC_R_NOTFOUND;
}

isc_result_t omapi_io_state_foreach (isc_result_t (*func) (omapi_object_t *,
							   void *),
				     void *p)
{
	omapi_io_object_t *io;
	isc_result_t status;

	for (io = omapi_io_states.next; io; io = io -> next) {
		if (io -> inner) {
			status = (*func) (io -> inner, p);
			if (status != ISC_R_SUCCESS)
				return status;
		}
	}
	return ISC_R_SUCCESS;
}
