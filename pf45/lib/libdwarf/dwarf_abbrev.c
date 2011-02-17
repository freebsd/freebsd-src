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
dwarf_abbrev_add(Dwarf_CU cu, uint64_t entry, uint64_t tag, uint8_t children, Dwarf_Abbrev *ap, Dwarf_Error *error)
{
	Dwarf_Abbrev a;
	int ret = DWARF_E_NONE;

	if ((a = malloc(sizeof(struct _Dwarf_Abbrev))) == NULL) {
		DWARF_SET_ERROR(error, DWARF_E_MEMORY);
		return DWARF_E_MEMORY;
	}

	/* Initialise the abbrev structure. */
	a->a_entry	= entry;
	a->a_tag	= tag;
	a->a_children	= children;

	/* Initialise the list of attributes. */
	STAILQ_INIT(&a->a_attrib);

	/* Add the abbrev to the list in the compilation unit. */
	STAILQ_INSERT_TAIL(&cu->cu_abbrev, a, a_next);

	if (ap != NULL)
		*ap = a;

	return ret;
}

Dwarf_Abbrev
dwarf_abbrev_find(Dwarf_CU cu, uint64_t entry)
{
	Dwarf_Abbrev a = NULL;

	STAILQ_FOREACH(a, &cu->cu_abbrev, a_next) {
		if (a->a_entry == entry)
			break;
	}

	return a;
}
