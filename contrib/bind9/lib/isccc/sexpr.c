/*
 * Portions Copyright (C) 2004, 2005, 2007  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: sexpr.c,v 1.9 2007/08/28 07:20:43 tbox Exp $ */

/*! \file */

#include <config.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <isc/assertions.h>
#include <isccc/sexpr.h>
#include <isccc/util.h>

static isccc_sexpr_t sexpr_t = { ISCCC_SEXPRTYPE_T, { NULL } };

#define CAR(s)			(s)->value.as_dottedpair.car
#define CDR(s)			(s)->value.as_dottedpair.cdr

isccc_sexpr_t *
isccc_sexpr_cons(isccc_sexpr_t *car, isccc_sexpr_t *cdr)
{
	isccc_sexpr_t *sexpr;

	sexpr = malloc(sizeof(*sexpr));
	if (sexpr == NULL)
		return (NULL);
	sexpr->type = ISCCC_SEXPRTYPE_DOTTEDPAIR;
	CAR(sexpr) = car;
	CDR(sexpr) = cdr;

	return (sexpr);
}

isccc_sexpr_t *
isccc_sexpr_tconst(void)
{
	return (&sexpr_t);
}

isccc_sexpr_t *
isccc_sexpr_fromstring(const char *str)
{
	isccc_sexpr_t *sexpr;

	sexpr = malloc(sizeof(*sexpr));
	if (sexpr == NULL)
		return (NULL);
	sexpr->type = ISCCC_SEXPRTYPE_STRING;
	sexpr->value.as_string = strdup(str);
	if (sexpr->value.as_string == NULL) {
		free(sexpr);
		return (NULL);
	}

	return (sexpr);
}

isccc_sexpr_t *
isccc_sexpr_frombinary(const isccc_region_t *region)
{
	isccc_sexpr_t *sexpr;
	unsigned int region_size;

	sexpr = malloc(sizeof(*sexpr));
	if (sexpr == NULL)
		return (NULL);
	sexpr->type = ISCCC_SEXPRTYPE_BINARY;
	region_size = REGION_SIZE(*region);
	/*
	 * We add an extra byte when we malloc so we can NUL terminate
	 * the binary data.  This allows the caller to use it as a C
	 * string.  It's up to the caller to ensure this is safe.  We don't
	 * add 1 to the length of the binary region, because the NUL is
	 * not part of the binary data.
	 */
	sexpr->value.as_region.rstart = malloc(region_size + 1);
	if (sexpr->value.as_region.rstart == NULL) {
		free(sexpr);
		return (NULL);
	}
	sexpr->value.as_region.rend = sexpr->value.as_region.rstart +
		region_size;
	memcpy(sexpr->value.as_region.rstart, region->rstart, region_size);
	/*
	 * NUL terminate.
	 */
	sexpr->value.as_region.rstart[region_size] = '\0';

	return (sexpr);
}

void
isccc_sexpr_free(isccc_sexpr_t **sexprp)
{
	isccc_sexpr_t *sexpr;
	isccc_sexpr_t *item;

	sexpr = *sexprp;
	if (sexpr == NULL)
		return;
	switch (sexpr->type) {
	case ISCCC_SEXPRTYPE_STRING:
		free(sexpr->value.as_string);
		break;
	case ISCCC_SEXPRTYPE_DOTTEDPAIR:
		item = CAR(sexpr);
		if (item != NULL)
			isccc_sexpr_free(&item);
		item = CDR(sexpr);
		if (item != NULL)
			isccc_sexpr_free(&item);
		break;
	case ISCCC_SEXPRTYPE_BINARY:
		free(sexpr->value.as_region.rstart);
		break;
	}
	free(sexpr);

	*sexprp = NULL;
}

static isc_boolean_t
printable(isccc_region_t *r)
{
	unsigned char *curr;

	curr = r->rstart;
	while (curr != r->rend) {
		if (!isprint(*curr))
			return (ISC_FALSE);
		curr++;
	}

	return (ISC_TRUE);
}

void
isccc_sexpr_print(isccc_sexpr_t *sexpr, FILE *stream)
{
	isccc_sexpr_t *cdr;
	unsigned int size, i;
	unsigned char *curr;

	if (sexpr == NULL) {
		fprintf(stream, "nil");
		return;
	}

	switch (sexpr->type) {
	case ISCCC_SEXPRTYPE_T:
		fprintf(stream, "t");
		break;
	case ISCCC_SEXPRTYPE_STRING:
		fprintf(stream, "\"%s\"", sexpr->value.as_string);
		break;
	case ISCCC_SEXPRTYPE_DOTTEDPAIR:
		fprintf(stream, "(");
		do {
			isccc_sexpr_print(CAR(sexpr), stream);
			cdr = CDR(sexpr);
			if (cdr != NULL) {
				fprintf(stream, " ");
				if (cdr->type != ISCCC_SEXPRTYPE_DOTTEDPAIR) {
					fprintf(stream, ". ");
					isccc_sexpr_print(cdr, stream);
					cdr = NULL;
				}
			}
			sexpr = cdr;
		} while (sexpr != NULL);
		fprintf(stream, ")");
		break;
	case ISCCC_SEXPRTYPE_BINARY:
		size = REGION_SIZE(sexpr->value.as_region);
		curr = sexpr->value.as_region.rstart;
		if (printable(&sexpr->value.as_region)) {
			fprintf(stream, "'%.*s'", (int)size, curr);
		} else {
			fprintf(stream, "0x");
			for (i = 0; i < size; i++)
				fprintf(stream, "%02x", *curr++);
		}
		break;
	default:
		INSIST(0);
	}
}

isccc_sexpr_t *
isccc_sexpr_car(isccc_sexpr_t *list)
{
	REQUIRE(list->type == ISCCC_SEXPRTYPE_DOTTEDPAIR);

	return (CAR(list));
}

isccc_sexpr_t *
isccc_sexpr_cdr(isccc_sexpr_t *list)
{
	REQUIRE(list->type == ISCCC_SEXPRTYPE_DOTTEDPAIR);

	return (CDR(list));
}

void
isccc_sexpr_setcar(isccc_sexpr_t *pair, isccc_sexpr_t *car)
{
	REQUIRE(pair->type == ISCCC_SEXPRTYPE_DOTTEDPAIR);

	CAR(pair) = car;
}

void
isccc_sexpr_setcdr(isccc_sexpr_t *pair, isccc_sexpr_t *cdr)
{
	REQUIRE(pair->type == ISCCC_SEXPRTYPE_DOTTEDPAIR);

	CDR(pair) = cdr;
}

isccc_sexpr_t *
isccc_sexpr_addtolist(isccc_sexpr_t **l1p, isccc_sexpr_t *l2)
{
	isccc_sexpr_t *last, *elt, *l1;

	REQUIRE(l1p != NULL);
	l1 = *l1p;
	REQUIRE(l1 == NULL || l1->type == ISCCC_SEXPRTYPE_DOTTEDPAIR);

	elt = isccc_sexpr_cons(l2, NULL);
	if (elt == NULL)
		return (NULL);
	if (l1 == NULL) {
		*l1p = elt;
		return (elt);
	}
	for (last = l1; CDR(last) != NULL; last = CDR(last))
		/* Nothing */;
	CDR(last) = elt;

	return (elt);
}

isc_boolean_t
isccc_sexpr_listp(isccc_sexpr_t *sexpr)
{
	if (sexpr == NULL || sexpr->type == ISCCC_SEXPRTYPE_DOTTEDPAIR)
		return (ISC_TRUE);
	return (ISC_FALSE);
}

isc_boolean_t
isccc_sexpr_emptyp(isccc_sexpr_t *sexpr)
{
	if (sexpr == NULL)
		return (ISC_TRUE);
	return (ISC_FALSE);
}

isc_boolean_t
isccc_sexpr_stringp(isccc_sexpr_t *sexpr)
{
	if (sexpr != NULL && sexpr->type == ISCCC_SEXPRTYPE_STRING)
		return (ISC_TRUE);
	return (ISC_FALSE);
}

isc_boolean_t
isccc_sexpr_binaryp(isccc_sexpr_t *sexpr)
{
	if (sexpr != NULL && sexpr->type == ISCCC_SEXPRTYPE_BINARY)
		return (ISC_TRUE);
	return (ISC_FALSE);
}

char *
isccc_sexpr_tostring(isccc_sexpr_t *sexpr)
{
	REQUIRE(sexpr != NULL &&
		(sexpr->type == ISCCC_SEXPRTYPE_STRING ||
		 sexpr->type == ISCCC_SEXPRTYPE_BINARY));
	
	if (sexpr->type == ISCCC_SEXPRTYPE_BINARY)
		return ((char *)sexpr->value.as_region.rstart);
	return (sexpr->value.as_string);
}

isccc_region_t *
isccc_sexpr_tobinary(isccc_sexpr_t *sexpr)
{
	REQUIRE(sexpr != NULL && sexpr->type == ISCCC_SEXPRTYPE_BINARY);
	return (&sexpr->value.as_region);
}
