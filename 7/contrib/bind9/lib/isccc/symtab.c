/*
 * Portions Copyright (C) 2004, 2005, 2007  Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (C) 2001  Internet Software Consortium.
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

/* $Id: symtab.c,v 1.5.18.4 2007/09/13 23:46:26 tbox Exp $ */

/*! \file */

#include <config.h>

#include <ctype.h>
#include <stdlib.h>

#include <isc/assertions.h>
#include <isc/magic.h>
#include <isc/string.h>

#include <isccc/result.h>
#include <isccc/symtab.h>
#include <isccc/util.h>

typedef struct elt {
	char *				key;
	unsigned int			type;
	isccc_symvalue_t			value;
	ISC_LINK(struct elt)		link;
} elt_t;

typedef ISC_LIST(elt_t)			eltlist_t;

#define SYMTAB_MAGIC			ISC_MAGIC('S', 'y', 'm', 'T')
#define VALID_SYMTAB(st)		ISC_MAGIC_VALID(st, SYMTAB_MAGIC)

struct isccc_symtab {
	unsigned int			magic;
	unsigned int			size;
	eltlist_t *			table;
	isccc_symtabundefaction_t		undefine_action;
	void *				undefine_arg;
	isc_boolean_t			case_sensitive;
};

isc_result_t
isccc_symtab_create(unsigned int size,
		  isccc_symtabundefaction_t undefine_action,
		  void *undefine_arg,
		  isc_boolean_t case_sensitive,
		  isccc_symtab_t **symtabp)
{
	isccc_symtab_t *symtab;
	unsigned int i;

	REQUIRE(symtabp != NULL && *symtabp == NULL);
	REQUIRE(size > 0);	/* Should be prime. */

	symtab = malloc(sizeof(*symtab));
	if (symtab == NULL)
		return (ISC_R_NOMEMORY);
	symtab->table = malloc(size * sizeof(eltlist_t));
	if (symtab->table == NULL) {
		free(symtab);
		return (ISC_R_NOMEMORY);
	}
	for (i = 0; i < size; i++)
		ISC_LIST_INIT(symtab->table[i]);
	symtab->size = size;
	symtab->undefine_action = undefine_action;
	symtab->undefine_arg = undefine_arg;
	symtab->case_sensitive = case_sensitive;
	symtab->magic = SYMTAB_MAGIC;

	*symtabp = symtab;

	return (ISC_R_SUCCESS);
}

static inline void
free_elt(isccc_symtab_t *symtab, unsigned int bucket, elt_t *elt) {
	ISC_LIST_UNLINK(symtab->table[bucket], elt, link);
	if (symtab->undefine_action != NULL)
		(symtab->undefine_action)(elt->key, elt->type, elt->value,
					  symtab->undefine_arg);
	free(elt);
}

void
isccc_symtab_destroy(isccc_symtab_t **symtabp) {
	isccc_symtab_t *symtab;
	unsigned int i;
	elt_t *elt, *nelt;

	REQUIRE(symtabp != NULL);
	symtab = *symtabp;
	REQUIRE(VALID_SYMTAB(symtab));

	for (i = 0; i < symtab->size; i++) {
		for (elt = ISC_LIST_HEAD(symtab->table[i]);
		     elt != NULL;
		     elt = nelt) {
			nelt = ISC_LIST_NEXT(elt, link);
			free_elt(symtab, i, elt);
		}
	}
	free(symtab->table);
	symtab->magic = 0;
	free(symtab);

	*symtabp = NULL;
}

static inline unsigned int
hash(const char *key, isc_boolean_t case_sensitive) {
	const char *s;
	unsigned int h = 0;
	unsigned int g;
	int c;

	/*
	 * P. J. Weinberger's hash function, adapted from p. 436 of
	 * _Compilers: Principles, Techniques, and Tools_, Aho, Sethi
	 * and Ullman, Addison-Wesley, 1986, ISBN 0-201-10088-6.
	 */

	if (case_sensitive) {
		for (s = key; *s != '\0'; s++) {
			h = ( h << 4 ) + *s;
			if ((g = ( h & 0xf0000000 )) != 0) {
				h = h ^ (g >> 24);
				h = h ^ g;
			}
		}
	} else {
		for (s = key; *s != '\0'; s++) {
			c = *s;
			c = tolower((unsigned char)c);
			h = ( h << 4 ) + c;
			if ((g = ( h & 0xf0000000 )) != 0) {
				h = h ^ (g >> 24);
				h = h ^ g;
			}
		}
	}

	return (h);
}

#define FIND(s, k, t, b, e) \
	b = hash((k), (s)->case_sensitive) % (s)->size; \
	if ((s)->case_sensitive) { \
		for (e = ISC_LIST_HEAD((s)->table[b]); \
		     e != NULL; \
		     e = ISC_LIST_NEXT(e, link)) { \
			if (((t) == 0 || e->type == (t)) && \
			    strcmp(e->key, (k)) == 0) \
				break; \
		} \
	} else { \
		for (e = ISC_LIST_HEAD((s)->table[b]); \
		     e != NULL; \
		     e = ISC_LIST_NEXT(e, link)) { \
			if (((t) == 0 || e->type == (t)) && \
			    strcasecmp(e->key, (k)) == 0) \
				break; \
		} \
	}

isc_result_t
isccc_symtab_lookup(isccc_symtab_t *symtab, const char *key, unsigned int type,
		  isccc_symvalue_t *value)
{
	unsigned int bucket;
	elt_t *elt;

	REQUIRE(VALID_SYMTAB(symtab));
	REQUIRE(key != NULL);

	FIND(symtab, key, type, bucket, elt);

	if (elt == NULL)
		return (ISC_R_NOTFOUND);

	if (value != NULL)
		*value = elt->value;

	return (ISC_R_SUCCESS);
}

isc_result_t
isccc_symtab_define(isccc_symtab_t *symtab, char *key, unsigned int type,
		  isccc_symvalue_t value, isccc_symexists_t exists_policy)
{
	unsigned int bucket;
	elt_t *elt;

	REQUIRE(VALID_SYMTAB(symtab));
	REQUIRE(key != NULL);
	REQUIRE(type != 0);

	FIND(symtab, key, type, bucket, elt);

	if (exists_policy != isccc_symexists_add && elt != NULL) {
		if (exists_policy == isccc_symexists_reject)
			return (ISC_R_EXISTS);
		INSIST(exists_policy == isccc_symexists_replace);
		ISC_LIST_UNLINK(symtab->table[bucket], elt, link);
		if (symtab->undefine_action != NULL)
			(symtab->undefine_action)(elt->key, elt->type,
						  elt->value,
						  symtab->undefine_arg);
	} else {
		elt = malloc(sizeof(*elt));
		if (elt == NULL)
			return (ISC_R_NOMEMORY);
		ISC_LINK_INIT(elt, link);
	}

	elt->key = key;
	elt->type = type;
	elt->value = value;

	/*
	 * We prepend so that the most recent definition will be found.
	 */
	ISC_LIST_PREPEND(symtab->table[bucket], elt, link);

	return (ISC_R_SUCCESS);
}

isc_result_t
isccc_symtab_undefine(isccc_symtab_t *symtab, const char *key, unsigned int type) {
	unsigned int bucket;
	elt_t *elt;

	REQUIRE(VALID_SYMTAB(symtab));
	REQUIRE(key != NULL);

	FIND(symtab, key, type, bucket, elt);

	if (elt == NULL)
		return (ISC_R_NOTFOUND);

	free_elt(symtab, bucket, elt);

	return (ISC_R_SUCCESS);
}

void
isccc_symtab_foreach(isccc_symtab_t *symtab, isccc_symtabforeachaction_t action,
		   void *arg)
{
	unsigned int i;
	elt_t *elt, *nelt;

	REQUIRE(VALID_SYMTAB(symtab));
	REQUIRE(action != NULL);

	for (i = 0; i < symtab->size; i++) {
		for (elt = ISC_LIST_HEAD(symtab->table[i]);
		     elt != NULL;
		     elt = nelt) {
			nelt = ISC_LIST_NEXT(elt, link);
			if ((action)(elt->key, elt->type, elt->value, arg))
				free_elt(symtab, i, elt);
		}
	}
}
