/*
 * Copyright (C) 1997-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * $Id: assertions.h,v 1.17 2001/07/12 05:58:21 mayer Exp $
 */

#ifndef ISC_ASSERTIONS_H
#define ISC_ASSERTIONS_H 1

#include <isc/lang.h>
#include <isc/platform.h>

ISC_LANG_BEGINDECLS

typedef enum {
	isc_assertiontype_require,
	isc_assertiontype_ensure,
	isc_assertiontype_insist,
	isc_assertiontype_invariant
} isc_assertiontype_t;

typedef void (*isc_assertioncallback_t)(const char *, int, isc_assertiontype_t,
					const char *);

LIBISC_EXTERNAL_DATA extern isc_assertioncallback_t isc_assertion_failed;

void
isc_assertion_setcallback(isc_assertioncallback_t);

const char *
isc_assertion_typetotext(isc_assertiontype_t type);

#ifdef ISC_CHECK_ALL
#define ISC_CHECK_REQUIRE		1
#define ISC_CHECK_ENSURE		1
#define ISC_CHECK_INSIST		1
#define ISC_CHECK_INVARIANT		1
#endif

#ifdef ISC_CHECK_NONE
#define ISC_CHECK_REQUIRE		0
#define ISC_CHECK_ENSURE		0
#define ISC_CHECK_INSIST		0
#define ISC_CHECK_INVARIANT		0
#endif

#ifndef ISC_CHECK_REQUIRE
#define ISC_CHECK_REQUIRE		1
#endif

#ifndef ISC_CHECK_ENSURE
#define ISC_CHECK_ENSURE		1
#endif

#ifndef ISC_CHECK_INSIST
#define ISC_CHECK_INSIST		1
#endif

#ifndef ISC_CHECK_INVARIANT
#define ISC_CHECK_INVARIANT		1
#endif

#if ISC_CHECK_REQUIRE != 0
#define ISC_REQUIRE(cond) \
	((void) ((cond) || \
		 ((isc_assertion_failed)(__FILE__, __LINE__, \
					 isc_assertiontype_require, \
					 #cond), 0)))
#else
#define ISC_REQUIRE(cond)	((void) 0)
#endif /* ISC_CHECK_REQUIRE */

#if ISC_CHECK_ENSURE != 0
#define ISC_ENSURE(cond) \
	((void) ((cond) || \
		 ((isc_assertion_failed)(__FILE__, __LINE__, \
					 isc_assertiontype_ensure, \
					 #cond), 0)))
#else
#define ISC_ENSURE(cond)	((void) 0)
#endif /* ISC_CHECK_ENSURE */

#if ISC_CHECK_INSIST != 0
#define ISC_INSIST(cond) \
	((void) ((cond) || \
		 ((isc_assertion_failed)(__FILE__, __LINE__, \
					 isc_assertiontype_insist, \
					 #cond), 0)))
#else
#define ISC_INSIST(cond)	((void) 0)
#endif /* ISC_CHECK_INSIST */

#if ISC_CHECK_INVARIANT != 0
#define ISC_INVARIANT(cond) \
	((void) ((cond) || \
		 ((isc_assertion_failed)(__FILE__, __LINE__, \
					 isc_assertiontype_invariant, \
					 #cond), 0)))
#else
#define ISC_INVARIANT(cond)	((void) 0)
#endif /* ISC_CHECK_INVARIANT */

ISC_LANG_ENDDECLS

#endif /* ISC_ASSERTIONS_H */
