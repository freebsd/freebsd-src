/* dispatch.c

   Network input dispatcher... */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1995-2003 by Internet Software Consortium
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
"$Id: dispatch.c,v 1.63.2.4 2004/06/10 17:59:16 dhankins Exp $ Copyright (c) 2004 Internet Systems Consortium.  All rights reserved.\n"
"$FreeBSD$\n";
#endif /* not lint */

#include "dhcpd.h"

struct timeout *timeouts;
static struct timeout *free_timeouts;

#ifdef ENABLE_POLLING_MODE
extern int polling_interval;
#endif

void set_time (TIME t)
{
	/* Do any outstanding timeouts. */
	if (cur_time != t) {
		cur_time = t;
		process_outstanding_timeouts ((struct timeval *)0);
	}
}

struct timeval *process_outstanding_timeouts (struct timeval *tvp)
{
	/* Call any expired timeouts, and then if there's
	   still a timeout registered, time out the select
	   call then. */
      another:
	if (timeouts) {
		struct timeout *t;
		if (timeouts -> when <= cur_time) {
			t = timeouts;
			timeouts = timeouts -> next;
			(*(t -> func)) (t -> what);
			if (t -> unref)
				(*t -> unref) (&t -> what, MDL);
			t -> next = free_timeouts;
			free_timeouts = t;
			goto another;
		}
		if (tvp) {
			tvp -> tv_sec = timeouts -> when;
			tvp -> tv_usec = 0;
		}
		return tvp;
	} else
		return (struct timeval *)0;
}

/* Wait for packets to come in using select().   When one does, call
   receive_packet to receive the packet and possibly strip hardware
   addressing information from it, and then call through the
   bootp_packet_handler hook to try to do something with it. */

void dispatch ()
{
	struct timeval tv, *tvp;
#ifdef ENABLE_POLLING_MODE
	struct timeval *tvp_new;
#endif
	isc_result_t status;

	tvp = NULL;
#ifdef ENABLE_POLLING_MODE
	tvp_new = NULL;
#endif
	/* Wait for a packet or a timeout... XXX */
	do {
		tvp = process_outstanding_timeouts (&tv);
#ifdef ENABLE_POLLING_MODE
		GET_TIME (&cur_time);
		add_timeout(cur_time + polling_interval, state_polling, 0, 0, 0);
		tvp_new = process_outstanding_timeouts(&tv);
		if (tvp != NULL && (tvp -> tv_sec > tvp_new -> tv_sec))
			tvp = tvp_new;
#endif /* ENABLE_POLLING_MODE */
		status = omapi_one_dispatch (0, tvp);
	} while (status == ISC_R_TIMEDOUT || status == ISC_R_SUCCESS);
	log_fatal ("omapi_one_dispatch failed: %s -- exiting.",
		   isc_result_totext (status));
}

void add_timeout (when, where, what, ref, unref)
	TIME when;
	void (*where) PROTO ((void *));
	void *what;
	tvref_t ref;
	tvunref_t unref;
{
	struct timeout *t, *q;

	/* See if this timeout supersedes an existing timeout. */
	t = (struct timeout *)0;
	for (q = timeouts; q; q = q -> next) {
		if ((where == NULL || q -> func == where) &&
		    q -> what == what) {
			if (t)
				t -> next = q -> next;
			else
				timeouts = q -> next;
			break;
		}
		t = q;
	}

	/* If we didn't supersede a timeout, allocate a timeout
	   structure now. */
	if (!q) {
		if (free_timeouts) {
			q = free_timeouts;
			free_timeouts = q -> next;
		} else {
			q = ((struct timeout *)
			     dmalloc (sizeof (struct timeout), MDL));
			if (!q)
				log_fatal ("add_timeout: no memory!");
		}
		memset (q, 0, sizeof *q);
		q -> func = where;
		q -> ref = ref;
		q -> unref = unref;
		if (q -> ref)
			(*q -> ref)(&q -> what, what, MDL);
		else
			q -> what = what;
	}

	q -> when = when;

	/* Now sort this timeout into the timeout list. */

	/* Beginning of list? */
	if (!timeouts || timeouts -> when > q -> when) {
		q -> next = timeouts;
		timeouts = q;
		return;
	}

	/* Middle of list? */
	for (t = timeouts; t -> next; t = t -> next) {
		if (t -> next -> when > q -> when) {
			q -> next = t -> next;
			t -> next = q;
			return;
		}
	}

	/* End of list. */
	t -> next = q;
	q -> next = (struct timeout *)0;
}

void cancel_timeout (where, what)
	void (*where) PROTO ((void *));
	void *what;
{
	struct timeout *t, *q;

	/* Look for this timeout on the list, and unlink it if we find it. */
	t = (struct timeout *)0;
	for (q = timeouts; q; q = q -> next) {
		if (q -> func == where && q -> what == what) {
			if (t)
				t -> next = q -> next;
			else
				timeouts = q -> next;
			break;
		}
		t = q;
	}

	/* If we found the timeout, put it on the free list. */
	if (q) {
		if (q -> unref)
			(*q -> unref) (&q -> what, MDL);
		q -> next = free_timeouts;
		free_timeouts = q;
	}
}

#if defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
void cancel_all_timeouts ()
{
	struct timeout *t, *n;
	for (t = timeouts; t; t = n) {
		n = t -> next;
		if (t -> unref && t -> what)
			(*t -> unref) (&t -> what, MDL);
		t -> next = free_timeouts;
		free_timeouts = t;
	}
}

void relinquish_timeouts ()
{
	struct timeout *t, *n;
	for (t = free_timeouts; t; t = n) {
		n = t -> next;
		dfree (t, MDL);
	}
}
#endif
