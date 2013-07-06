/*-
 * Copyright (c) 2007 John Birrell (jb@freebsd.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <stdlib.h>
#include "_libdwarf.h"

int
dwarf_die_add(Dwarf_CU cu, int level, uint64_t offset, uint64_t abnum, Dwarf_Abbrev a, Dwarf_Die *diep, Dwarf_Error *err)
{
	Dwarf_Die die;
	uint64_t key;
	int ret = DWARF_E_NONE;

	if (err == NULL)
		return DWARF_E_ERROR;

	if (cu == NULL || a == NULL) {
		DWARF_SET_ERROR(err, DWARF_E_ARGUMENT);
		return DWARF_E_ARGUMENT;
	}

	if ((die = malloc(sizeof(struct _Dwarf_Die))) == NULL) {
		DWARF_SET_ERROR(err, DWARF_E_MEMORY);
		return DWARF_E_MEMORY;
	}

	/* Initialise the abbrev structure. */
	die->die_level	= level;
	die->die_offset	= offset;
	die->die_abnum	= abnum;
	die->die_a	= a;
	die->die_cu	= cu;
	die->die_name	= "";

	/* Initialise the list of attribute values. */
	STAILQ_INIT(&die->die_attrval);

	/* Add the die to the list in the compilation unit. */
	STAILQ_INSERT_TAIL(&cu->cu_die, die, die_next);

	/* Add the die to the hash table in the compilation unit. */
	key = offset % DWARF_DIE_HASH_SIZE;
	STAILQ_INSERT_TAIL(&cu->cu_die_hash[key], die, die_hash);

	if (diep != NULL)
		*diep = die;

	return ret;
}

int
dwarf_dieoffset(Dwarf_Die die, Dwarf_Off *ret_offset, Dwarf_Error *err __unused)
{
	*ret_offset = die->die_offset;

	return DWARF_E_NONE;
}

int
dwarf_child(Dwarf_Die die, Dwarf_Die *ret_die, Dwarf_Error *err)
{
	Dwarf_Die next;
	int ret = DWARF_E_NONE;

	if (err == NULL)
		return DWARF_E_ERROR;

	if (die == NULL || ret_die == NULL) {
		DWARF_SET_ERROR(err, DWARF_E_ARGUMENT);
		return DWARF_E_ARGUMENT;
	}

	if ((next = STAILQ_NEXT(die, die_next)) == NULL ||
	    next->die_level != die->die_level + 1) {
		*ret_die = NULL;
		DWARF_SET_ERROR(err, DWARF_E_NO_ENTRY);
		ret = DWARF_E_NO_ENTRY;
	} else
		*ret_die = next;

	return ret;
}

int
dwarf_tag(Dwarf_Die die, Dwarf_Half *tag, Dwarf_Error *err)
{
	Dwarf_Abbrev a;

	if (err == NULL)
		return DWARF_E_ERROR;

	if (die == NULL || tag == NULL || (a = die->die_a) == NULL) {
		DWARF_SET_ERROR(err, DWARF_E_ARGUMENT);
		return DWARF_E_ARGUMENT;
	}

	*tag = a->a_tag;

	return DWARF_E_NONE;
}

int
dwarf_siblingof(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Die *caller_ret_die, Dwarf_Error *err)
{
	Dwarf_Die next;
	Dwarf_CU cu;
	int ret = DWARF_E_NONE;

	if (err == NULL)
		return DWARF_E_ERROR;

	if (dbg == NULL || caller_ret_die == NULL) {
		DWARF_SET_ERROR(err, DWARF_E_ARGUMENT);
		return DWARF_E_ARGUMENT;
	}

	if ((cu = dbg->dbg_cu_current) == NULL) {
		DWARF_SET_ERROR(err, DWARF_E_CU_CURRENT);
		return DWARF_E_CU_CURRENT;
	}

	if (die == NULL) {
		*caller_ret_die = STAILQ_FIRST(&cu->cu_die);

		if (*caller_ret_die == NULL) {
			DWARF_SET_ERROR(err, DWARF_E_NO_ENTRY);
			ret = DWARF_E_NO_ENTRY;
		}
	} else {
		next = die;
		while ((next = STAILQ_NEXT(next, die_next)) != NULL) {
			if (next->die_level < die->die_level) {
				next = NULL;
				break;
			}
			if (next->die_level == die->die_level) {
				*caller_ret_die = next;
				break;
			}
		}

		if (next == NULL) {
			*caller_ret_die = NULL;
			DWARF_SET_ERROR(err, DWARF_E_NO_ENTRY);
			ret = DWARF_E_NO_ENTRY;
		}
	}

	return ret;
}

Dwarf_Die
dwarf_die_find(Dwarf_Die die, Dwarf_Unsigned off)
{
	Dwarf_CU cu = die->die_cu;
	Dwarf_Die die1;

	STAILQ_FOREACH(die1, &cu->cu_die, die_next) {
		if (die1->die_offset == off)
			return (die1);
	}

	return (NULL);
}
