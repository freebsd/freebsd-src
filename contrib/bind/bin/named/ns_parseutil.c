/*
 * Copyright (c) 1996-2000 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/* Global C stuff goes here. */


#include "port_before.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/nameser.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include <isc/eventlib.h>
#include <isc/logging.h>
#include <isc/memcluster.h>

#include "port_after.h"

#include "named.h"
#include "ns_parseutil.h"


/*
 * Symbol Table
 */

symbol_table
new_symbol_table(int size_guess, free_function free_value) {
	symbol_table st;

	st = (symbol_table)memget(sizeof (struct symbol_table));
	if (st == NULL)
		panic("memget failed in new_symbol_table()", NULL);
	st->table = (symbol_entry *)memget(size_guess * sizeof *st->table);
	if (st->table == NULL)
		panic("memget failed in new_symbol_table()", NULL);
	memset(st->table, 0, size_guess * sizeof (symbol_entry));
	st->size = size_guess;   /* size_guess should be prime */
	st->free_value = free_value;
	return (st);
}

void
free_symbol(symbol_table st, symbol_entry ste) {
	if (ste->flags & SYMBOL_FREE_KEY)
		freestr(ste->key);
	if (ste->flags & SYMBOL_FREE_VALUE)
		(st->free_value)(ste->type, ste->value.pointer);
}

void
free_symbol_table(symbol_table st) {
	int i;
	symbol_entry ste, ste_next;

	for (i = 0; i < st->size; i++) {
		for (ste = st->table[i]; ste != NULL; ste = ste_next) {
			ste_next = ste->next;
			free_symbol(st, ste);
			memput(ste, sizeof *ste);
		}
	}
	memput(st->table, st->size * sizeof (symbol_entry));
	memput(st, sizeof *st);
}

void
dprint_symbol_table(int level, symbol_table st) {
	int i;
	symbol_entry ste;

	for (i = 0; i < st->size; i++) {
		for (ste = st->table[i]; ste != NULL; ste = ste->next)
			ns_debug(ns_log_parser, level,
				 "%7d: (%s: %d %p/%d %04x) ",
				 i, ste->key, ste->type, ste->value.pointer,
				 ste->value.integer, ste->flags);
	}
}

/*
 * P. J. Weinberger's hash function, adapted from p. 436 of
 * _Compilers: Principles, Techniques, and Tools_, Aho, Sethi
 * and Ullman, Addison-Wesley, 1986, ISBN 0-201-10088-6.
 */
static int
symbol_hash(const char *key, int prime) {
	const char *s;
	unsigned int h = 0;
	unsigned int g;
	int c;

	for (s = key; *s != '\0'; s++) {
		c = *s;
		if (isascii(c) && isupper(c))
			c = tolower(c);
		h = ( h << 4 ) + c;
		if ((g = ( h & 0xf0000000 )) != 0) {
			h = h ^ (g >> 24);
			h = h ^ g;
		}
	}
	return (h % prime);
}

int
lookup_symbol(symbol_table st, const char *key, int type,
	      symbol_value *value) {
	int hash;
	symbol_entry ste;

	hash = symbol_hash(key, st->size);
	for (ste = st->table[hash]; ste != NULL; ste = ste->next)
		if ((type == 0 || ste->type == type) &&
		    strcasecmp(ste->key, key) == 0)
			break;
	if (ste != NULL) {
		if (value != NULL)
			*value = ste->value;
		return (1);
	}
	return (0);
}

void
define_symbol(symbol_table st, char *key, int type, symbol_value value,
	      unsigned int flags) {
	int hash;
	symbol_entry ste;

	hash = symbol_hash(key, st->size);
	for (ste = st->table[hash]; ste != NULL; ste = ste->next)
		if ((type == 0 || ste->type == type) &&
		    strcasecmp(ste->key, key) == 0)
			break;
	if (ste == NULL) {
		ste = (symbol_entry)memget(sizeof *ste);
		if (ste == NULL)
			panic("memget failed in define_symbol()", NULL);
		ste->key = key;
		ste->type = type;
		ste->value = value;
		ste->flags = flags;
		ste->next = st->table[hash];
		st->table[hash] = ste;
	} else {
		ns_debug(ns_log_parser, 7, "redefined symbol %s type %d",
			 key, type);
		free_symbol(st, ste);
		ste->key = key;
		ste->value = value;
		ste->flags = flags;
	}
}

void
undefine_symbol(symbol_table st, char *key, int type) {
	int hash;
	symbol_entry prev_ste, ste;

	hash = symbol_hash(key, st->size);
	for (prev_ste = NULL, ste = st->table[hash];
	     ste != NULL;
	     prev_ste = ste, ste = ste->next)
		if ((type == 0 || ste->type == type) &&
		    strcasecmp(ste->key, key) == 0)
			break;
	if (ste != NULL) {
		free_symbol(st, ste);
		if (prev_ste != NULL)
			prev_ste->next = ste->next;
		else
			st->table[hash] = ste->next;
		memput(ste, sizeof *ste);
	}
}

/*
 * Conversion Routines
 */

int
unit_to_ulong(char *in, u_long *out) {	
	int c, units_done = 0;
	u_long result = 0L;

	INSIST(in != NULL);

	for (; (c = *in) != '\0'; in++) {
		if (units_done)
			return (0);
		if (isdigit(c)) {
			result *= 10;
			result += (c - '0');
		} else {
			switch (c) {
			case 'k':
			case 'K':
				result *= 1024;
				units_done = 1;
				break;
			case 'm':
			case 'M':
				result *= (1024*1024);
				units_done = 1;
				break;
			case 'g':
			case 'G':
				result *= (1024*1024*1024);
				units_done = 1;
				break;
			default:
				return (0);
			}
		}
	}

	*out = result;
	return (1);
}
