/*
 * Copyright (C) 2009  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: types.h,v 1.3 2009-09-02 23:48:02 tbox Exp $ */

#ifndef IRS_TYPES_H
#define IRS_TYPES_H 1

/* Core Types.  Alphabetized by defined type. */

/*%< per-thread IRS context */
typedef struct irs_context		irs_context_t;
/*%< resolv.conf configuration information */
typedef struct irs_resconf		irs_resconf_t;
/*%< advanced DNS-related configuration information */
typedef struct irs_dnsconf		irs_dnsconf_t;

#endif /* IRS_TYPES_H */
