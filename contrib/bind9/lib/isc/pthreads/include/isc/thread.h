/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1998-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: thread.h,v 1.19.206.1 2004/03/06 08:14:57 marka Exp $ */

#ifndef ISC_THREAD_H
#define ISC_THREAD_H 1

#include <pthread.h>

#include <isc/lang.h>
#include <isc/result.h>

ISC_LANG_BEGINDECLS

typedef pthread_t isc_thread_t;
typedef void * isc_threadresult_t;
typedef void * isc_threadarg_t;
typedef isc_threadresult_t (*isc_threadfunc_t)(isc_threadarg_t);

isc_result_t
isc_thread_create(isc_threadfunc_t, isc_threadarg_t, isc_thread_t *);

void
isc_thread_setconcurrency(unsigned int level);

/* XXX We could do fancier error handling... */

#define isc_thread_join(t, rp) \
	((pthread_join((t), (rp)) == 0) ? \
	 ISC_R_SUCCESS : ISC_R_UNEXPECTED)

#define isc_thread_self \
	(unsigned long)pthread_self

ISC_LANG_ENDDECLS

#endif /* ISC_THREAD_H */
