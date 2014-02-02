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
 */

#include "_libdwarf.h"

ELFTC_VCSID("$Id: dwarf_cu.c 2072 2011-10-27 03:26:49Z jkoshy $");

int
dwarf_next_cu_header_b(Dwarf_Debug dbg, Dwarf_Unsigned *cu_length,
    Dwarf_Half *cu_version, Dwarf_Off *cu_abbrev_offset,
    Dwarf_Half *cu_pointer_size, Dwarf_Half *cu_offset_size,
    Dwarf_Half *cu_extension_size, Dwarf_Unsigned *cu_next_offset,
    Dwarf_Error *error)
{
	Dwarf_CU cu;
	int ret;

	if (dbg == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}

	if (dbg->dbg_cu_current == NULL)
		ret = _dwarf_info_first_cu(dbg, error);
	else
		ret = _dwarf_info_next_cu(dbg, error);

	if (ret == DW_DLE_NO_ENTRY) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_NO_ENTRY);
		return (DW_DLV_NO_ENTRY);
	} else if (ret != DW_DLE_NONE)
		return (DW_DLV_ERROR);

	if (dbg->dbg_cu_current == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_NO_ENTRY);
		return (DW_DLV_NO_ENTRY);
	}
	cu = dbg->dbg_cu_current;

	if (cu_length)
		*cu_length = cu->cu_length;
	if (cu_version)
		*cu_version = cu->cu_version;
	if (cu_abbrev_offset)
		*cu_abbrev_offset = (Dwarf_Off) cu->cu_abbrev_offset;
	if (cu_pointer_size)
		*cu_pointer_size = cu->cu_pointer_size;
	if (cu_offset_size) {
		if (cu->cu_length_size == 4)
			*cu_offset_size = 4;
		else
			*cu_offset_size = 8;
	}
	if (cu_extension_size) {
		if (cu->cu_length_size == 4)
			*cu_extension_size = 0;
		else
			*cu_extension_size = 4;
	}
	if (cu_next_offset)
		*cu_next_offset	= dbg->dbg_cu_current->cu_next_offset;

	return (DW_DLV_OK);
}

int
dwarf_next_cu_header(Dwarf_Debug dbg, Dwarf_Unsigned *cu_length,
    Dwarf_Half *cu_version, Dwarf_Off *cu_abbrev_offset,
    Dwarf_Half *cu_pointer_size, Dwarf_Unsigned *cu_next_offset,
    Dwarf_Error *error)
{

	return (dwarf_next_cu_header_b(dbg, cu_length, cu_version,
	    cu_abbrev_offset, cu_pointer_size, NULL, NULL, cu_next_offset,
	    error));
}
