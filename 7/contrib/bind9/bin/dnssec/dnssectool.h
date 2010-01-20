/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001, 2003  Internet Software Consortium.
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

/* $Id: dnssectool.h,v 1.18 2004/03/05 04:57:41 marka Exp $ */

#ifndef DNSSECTOOL_H
#define DNSSECTOOL_H 1

#include <isc/log.h>
#include <isc/stdtime.h>
#include <dns/rdatastruct.h>
#include <dst/dst.h>

typedef void (fatalcallback_t)(void);

void
fatal(const char *format, ...) ISC_FORMAT_PRINTF(1, 2);

void
setfatalcallback(fatalcallback_t *callback);

void
check_result(isc_result_t result, const char *message);

void
vbprintf(int level, const char *fmt, ...) ISC_FORMAT_PRINTF(2, 3);

void
type_format(const dns_rdatatype_t type, char *cp, unsigned int size);
#define TYPE_FORMATSIZE 10

void
alg_format(const dns_secalg_t alg, char *cp, unsigned int size);
#define ALG_FORMATSIZE 10

void
sig_format(dns_rdata_rrsig_t *sig, char *cp, unsigned int size);
#define SIG_FORMATSIZE (DNS_NAME_FORMATSIZE + ALG_FORMATSIZE + sizeof("65535"))

void
key_format(const dst_key_t *key, char *cp, unsigned int size);
#define KEY_FORMATSIZE (DNS_NAME_FORMATSIZE + ALG_FORMATSIZE + sizeof("65535"))

void
setup_logging(int verbose, isc_mem_t *mctx, isc_log_t **logp);

void
cleanup_logging(isc_log_t **logp);

void
setup_entropy(isc_mem_t *mctx, const char *randomfile, isc_entropy_t **ectx);

void
cleanup_entropy(isc_entropy_t **ectx);

isc_stdtime_t
strtotime(const char *str, isc_int64_t now, isc_int64_t base);

dns_rdataclass_t
strtoclass(const char *str);

#endif /* DNSSEC_DNSSECTOOL_H */
