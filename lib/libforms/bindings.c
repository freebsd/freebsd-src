/*
 * Copyright (c) 1995
 *	Paul Richards.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Paul Richards.
 * 4. The name Paul Richards may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL RICHARDS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PAUL RICHARDS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <err.h>
#include <forms.h>
#include <strhash.h>
#include <stdlib.h>

#include "internal.h"

int
bind_tuple(hash_table *htable, char *name,
                TupleType type, void(*fn)(void*))
{
	struct Tuple *tuple;

	tuple = malloc(sizeof (struct Tuple));
	if (!tuple) {
		warn("Couldn't allocate memory for new tuple");
		return (ST_ERROR);
	}

	tuple->name = name;
	tuple->type = type;
	tuple->addr = fn;

	if (hash_search(htable, tuple->name, tuple, NULL)) {
		warn("Duplicate tuple name, %s, skipping", name);
		return (ST_ERROR);
	}

#ifdef DEBUG
	debug_dump_table(htable);
#endif

	return (0);
}

int
tuple_match_any(char *key, void *data, void *arg)
{
	TUPLE *tuple = (TUPLE *)data;
	TupleType *type = (TupleType *)arg;

	if (tuple->type != *type) {
		arg = 0;
		return (1);
	} else {
		arg = data;
		return (0);
	}
}

/*
 * Search a single hash table for a tuple.
 */

TUPLE *
get_tuple(hash_table *table, char *key, TupleType type)
{
	void *arg = &type;

	/*
	 * If a key is specified then search for that key,
	 * otherwise, search the whole table for the first
	 * tuple of the required type.
	 */

	if (key)
		return(hash_search(table, key, NULL, NULL));
	else {
		hash_traverse(table, &tuple_match_any, arg);
		return (arg);
	}
}

/*
 * Search all tables currently in scope.
 */

TUPLE *
tuple_search(OBJECT *obj, char *key, TupleType type)
{
	TUPLE *tuple;

	while (obj) {

		tuple = get_tuple(obj->bind, key, type);

		if (tuple)
			return (tuple);
		else
			obj = obj->parent;
	}
	return (get_tuple(root_table, key, type));
}
