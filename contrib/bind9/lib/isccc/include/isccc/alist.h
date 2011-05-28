/*
 * Portions Copyright (C) 2004-2007  Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (C) 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NOMINUM DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Portions Copyright (C) 2001  Nominum, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NOMINUM DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: alist.h,v 1.10 2007-08-28 07:20:43 tbox Exp $ */

#ifndef ISCCC_ALIST_H
#define ISCCC_ALIST_H 1

/*! \file isccc/alist.h */

#include <stdio.h>

#include <isc/lang.h>
#include <isccc/types.h>

ISC_LANG_BEGINDECLS

isccc_sexpr_t *
isccc_alist_create(void);

isc_boolean_t
isccc_alist_alistp(isccc_sexpr_t *alist);

isc_boolean_t
isccc_alist_emptyp(isccc_sexpr_t *alist);

isccc_sexpr_t *
isccc_alist_first(isccc_sexpr_t *alist);

isccc_sexpr_t *
isccc_alist_assq(isccc_sexpr_t *alist, const char *key);

void
isccc_alist_delete(isccc_sexpr_t *alist, const char *key);

isccc_sexpr_t *
isccc_alist_define(isccc_sexpr_t *alist, const char *key, isccc_sexpr_t *value);

isccc_sexpr_t *
isccc_alist_definestring(isccc_sexpr_t *alist, const char *key, const char *str);

isccc_sexpr_t *
isccc_alist_definebinary(isccc_sexpr_t *alist, const char *key, isccc_region_t *r);

isccc_sexpr_t *
isccc_alist_lookup(isccc_sexpr_t *alist, const char *key);

isc_result_t
isccc_alist_lookupstring(isccc_sexpr_t *alist, const char *key, char **strp);

isc_result_t
isccc_alist_lookupbinary(isccc_sexpr_t *alist, const char *key, isccc_region_t **r);

void
isccc_alist_prettyprint(isccc_sexpr_t *sexpr, unsigned int indent, FILE *stream);

ISC_LANG_ENDDECLS

#endif /* ISCCC_ALIST_H */
