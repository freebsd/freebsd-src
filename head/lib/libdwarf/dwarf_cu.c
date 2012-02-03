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

#include "_libdwarf.h"

int
dwarf_next_cu_header(Dwarf_Debug dbg, Dwarf_Unsigned *cu_header_length,
    Dwarf_Half *cu_version, Dwarf_Unsigned *cu_abbrev_offset,
    Dwarf_Half *cu_pointer_size, Dwarf_Unsigned *cu_next_offset, Dwarf_Error *error)
{
	Dwarf_CU next;

	if (error == NULL)
		return DWARF_E_ERROR;

	if (dbg == NULL || cu_header_length == NULL || cu_version == NULL ||
	    cu_abbrev_offset == NULL || cu_pointer_size == NULL ||
	    cu_next_offset == NULL) {
		DWARF_SET_ERROR(error, DWARF_E_ARGUMENT);
		return DWARF_E_ERROR;
	}

	if (dbg->dbg_cu_current == NULL)
		dbg->dbg_cu_current = STAILQ_FIRST(&dbg->dbg_cu);
	else if ((next = STAILQ_NEXT(dbg->dbg_cu_current, cu_next)) == NULL) {
		DWARF_SET_ERROR(error, DWARF_E_NO_ENTRY);
		return DWARF_E_NO_ENTRY;
	} else
		dbg->dbg_cu_current = next;

	if (dbg->dbg_cu_current == NULL) {
		DWARF_SET_ERROR(error, DWARF_E_NO_ENTRY);
		return DWARF_E_NO_ENTRY;
	}

	*cu_header_length	= dbg->dbg_cu_current->cu_header_length;
	*cu_version 		= dbg->dbg_cu_current->cu_version;
	*cu_abbrev_offset	= dbg->dbg_cu_current->cu_abbrev_offset;
	*cu_pointer_size	= dbg->dbg_cu_current->cu_pointer_size;
	*cu_next_offset		= dbg->dbg_cu_current->cu_next_offset;

	return DWARF_E_NONE;
}
