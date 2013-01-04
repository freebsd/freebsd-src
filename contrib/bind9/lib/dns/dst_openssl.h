/*
 * Copyright (C) 2004, 2005, 2007-2009, 2011, 2012  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2002  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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

/* $Id$ */

#ifndef DST_OPENSSL_H
#define DST_OPENSSL_H 1

#include <isc/lang.h>
#include <isc/result.h>

#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/conf.h>
#include <openssl/crypto.h>

#if !defined(OPENSSL_NO_ENGINE) && defined(CRYPTO_LOCK_ENGINE) && \
    (OPENSSL_VERSION_NUMBER >= 0x0090707f)
#define USE_ENGINE 1
#endif

ISC_LANG_BEGINDECLS

isc_result_t
dst__openssl_toresult(isc_result_t fallback);

isc_result_t
dst__openssl_toresult2(const char *funcname, isc_result_t fallback);

#ifdef USE_ENGINE
ENGINE *
dst__openssl_getengine(const char *engine);
#else
#define dst__openssl_getengine(x) NULL
#endif

ISC_LANG_ENDDECLS

#endif /* DST_OPENSSL_H */
/*! \file */
