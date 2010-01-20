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

/* $Id: symtab.h,v 1.3.18.2 2005/04/29 00:17:14 marka Exp $ */

#ifndef ISCCC_SYMTAB_H
#define ISCCC_SYMTAB_H 1

/*****
 ***** Module Info
 *****/

/*! \file 
 * \brief
 * Provides a simple memory-based symbol table.
 *
 * Keys are C strings.  A type may be specified when looking up,
 * defining, or undefining.  A type value of 0 means "match any type";
 * any other value will only match the given type.
 *
 * It's possible that a client will attempt to define a <key, type,
 * value> tuple when a tuple with the given key and type already
 * exists in the table.  What to do in this case is specified by the
 * client.  Possible policies are:
 *
 *\li	isccc_symexists_reject	Disallow the define, returning #ISC_R_EXISTS
 *\li	isccc_symexists_replace	Replace the old value with the new.  The
 *				undefine action (if provided) will be called
 *				with the old <key, type, value> tuple.
 *\li	isccc_symexists_add	Add the new tuple, leaving the old tuple in
 *				the table.  Subsequent lookups will retrieve
 *				the most-recently-defined tuple.
 *
 * A lookup of a key using type 0 will return the most-recently
 * defined symbol with that key.  An undefine of a key using type 0
 * will undefine the most-recently defined symbol with that key.
 * Trying to define a key with type 0 is illegal.
 *
 * The symbol table library does not make a copy the key field, so the
 * caller must ensure that any key it passes to isccc_symtab_define()
 * will not change until it calls isccc_symtab_undefine() or
 * isccc_symtab_destroy().
 *
 * A user-specified action will be called (if provided) when a symbol
 * is undefined.  It can be used to free memory associated with keys
 * and/or values.
 */

/***
 *** Imports.
 ***/

#include <isc/lang.h>
#include <isccc/types.h>

/***
 *** Symbol Tables.
 ***/

typedef union isccc_symvalue {
	void *				as_pointer;
	int				as_integer;
	unsigned int			as_uinteger;
} isccc_symvalue_t;

typedef void (*isccc_symtabundefaction_t)(char *key, unsigned int type,
					isccc_symvalue_t value, void *userarg);

typedef isc_boolean_t (*isccc_symtabforeachaction_t)(char *key,
						   unsigned int type,
						   isccc_symvalue_t value,
						   void *userarg);

typedef enum {
	isccc_symexists_reject = 0,
	isccc_symexists_replace = 1,
	isccc_symexists_add = 2
} isccc_symexists_t;

ISC_LANG_BEGINDECLS

isc_result_t
isccc_symtab_create(unsigned int size,
		  isccc_symtabundefaction_t undefine_action, void *undefine_arg,
		  isc_boolean_t case_sensitive, isccc_symtab_t **symtabp);

void
isccc_symtab_destroy(isccc_symtab_t **symtabp);

isc_result_t
isccc_symtab_lookup(isccc_symtab_t *symtab, const char *key, unsigned int type,
		  isccc_symvalue_t *value);

isc_result_t
isccc_symtab_define(isccc_symtab_t *symtab, char *key, unsigned int type,
		  isccc_symvalue_t value, isccc_symexists_t exists_policy);

isc_result_t
isccc_symtab_undefine(isccc_symtab_t *symtab, const char *key, unsigned int type);

void
isccc_symtab_foreach(isccc_symtab_t *symtab, isccc_symtabforeachaction_t action,
		   void *arg);

ISC_LANG_ENDDECLS

#endif /* ISCCC_SYMTAB_H */
