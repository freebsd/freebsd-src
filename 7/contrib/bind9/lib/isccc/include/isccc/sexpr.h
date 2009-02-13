/*
 * Portions Copyright (C) 2004, 2005  Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (C) 2001  Internet Software Consortium.
 * Portions Copyright (C) 2001  Nominum, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
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

/* $Id: sexpr.h,v 1.4.18.2 2005/04/29 00:17:14 marka Exp $ */

#ifndef ISCCC_SEXPR_H
#define ISCCC_SEXPR_H 1

/*! \file */

#include <stdio.h>

#include <isc/lang.h>
#include <isccc/types.h>

ISC_LANG_BEGINDECLS

/*% dotted pair structure */
struct isccc_dottedpair {
	isccc_sexpr_t *car;
	isccc_sexpr_t *cdr;
};

/*% iscc_sexpr structure */
struct isccc_sexpr {
	unsigned int			type;
	union {
		char *			as_string;
		isccc_dottedpair_t	as_dottedpair;
		isccc_region_t		as_region;
	}				value;
};

#define ISCCC_SEXPRTYPE_NONE		0x00	/*%< Illegal. */
#define ISCCC_SEXPRTYPE_T			0x01
#define ISCCC_SEXPRTYPE_STRING		0x02
#define ISCCC_SEXPRTYPE_DOTTEDPAIR	0x03
#define ISCCC_SEXPRTYPE_BINARY		0x04

#define ISCCC_SEXPR_CAR(s)		(s)->value.as_dottedpair.car
#define ISCCC_SEXPR_CDR(s)		(s)->value.as_dottedpair.cdr

isccc_sexpr_t *
isccc_sexpr_cons(isccc_sexpr_t *car, isccc_sexpr_t *cdr);

isccc_sexpr_t *
isccc_sexpr_tconst(void);

isccc_sexpr_t *
isccc_sexpr_fromstring(const char *str);

isccc_sexpr_t *
isccc_sexpr_frombinary(const isccc_region_t *region);

void
isccc_sexpr_free(isccc_sexpr_t **sexprp);

void
isccc_sexpr_print(isccc_sexpr_t *sexpr, FILE *stream);

isccc_sexpr_t *
isccc_sexpr_car(isccc_sexpr_t *list);

isccc_sexpr_t *
isccc_sexpr_cdr(isccc_sexpr_t *list);

void
isccc_sexpr_setcar(isccc_sexpr_t *pair, isccc_sexpr_t *car);

void
isccc_sexpr_setcdr(isccc_sexpr_t *pair, isccc_sexpr_t *cdr);

isccc_sexpr_t *
isccc_sexpr_addtolist(isccc_sexpr_t **l1p, isccc_sexpr_t *l2);

isc_boolean_t
isccc_sexpr_listp(isccc_sexpr_t *sexpr);

isc_boolean_t
isccc_sexpr_emptyp(isccc_sexpr_t *sexpr);

isc_boolean_t
isccc_sexpr_stringp(isccc_sexpr_t *sexpr);

isc_boolean_t
isccc_sexpr_binaryp(isccc_sexpr_t *sexpr);

char *
isccc_sexpr_tostring(isccc_sexpr_t *sexpr);

isccc_region_t *
isccc_sexpr_tobinary(isccc_sexpr_t *sexpr);

ISC_LANG_ENDDECLS

#endif /* ISCCC_SEXPR_H */
