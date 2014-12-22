/*-
 * Copyright (c) 2010,2014 Kai Wang
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
 * $Id: dwarf_next_cu_header.c 3073 2014-06-23 03:08:49Z kaiwang27 $
 */

#include <assert.h>
#include <dwarf.h>
#include <errno.h>
#include <fcntl.h>
#include <libdwarf.h>
#include <string.h>

#include "driver.h"
#include "tet_api.h"

static void tp_dwarf_next_cu_header(void);
static void tp_dwarf_next_cu_header_b(void);
static void tp_dwarf_next_cu_header_c(void);
static void tp_dwarf_next_cu_header_loop(void);
static struct dwarf_tp dwarf_tp_array[] = {
	{"tp_dwarf_next_cu_header", tp_dwarf_next_cu_header},
	{"tp_dwarf_next_cu_header_b", tp_dwarf_next_cu_header_b},
	{"tp_dwarf_next_cu_header_c", tp_dwarf_next_cu_header_c},
	{"tp_dwarf_next_cu_header_loop", tp_dwarf_next_cu_header_loop},
	{NULL, NULL},
};
#include "driver.c"

static void
tp_dwarf_next_cu_header(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	Dwarf_Unsigned cu_header_length;
	Dwarf_Half cu_version;
	Dwarf_Off cu_abbrev_offset;
	Dwarf_Half cu_pointer_size;
	Dwarf_Unsigned cu_next_offset;
	int fd, result;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	while (dwarf_next_cu_header(dbg, &cu_header_length, &cu_version,
	    &cu_abbrev_offset, &cu_pointer_size, &cu_next_offset, &de) ==
	    DW_DLV_OK) {
		TS_CHECK_UINT(cu_header_length);
		TS_CHECK_UINT(cu_version);
		TS_CHECK_INT(cu_abbrev_offset);
		TS_CHECK_UINT(cu_pointer_size);
		TS_CHECK_UINT(cu_next_offset);
	}

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}

static void
tp_dwarf_next_cu_header_b(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	Dwarf_Unsigned cu_header_length;
	Dwarf_Half cu_version;
	Dwarf_Off cu_abbrev_offset;
	Dwarf_Half cu_pointer_size;
	Dwarf_Half cu_offset_size;
	Dwarf_Half cu_extension_size;
	Dwarf_Unsigned cu_next_offset;
	int fd, result;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	while (dwarf_next_cu_header_b(dbg, &cu_header_length, &cu_version,
	    &cu_abbrev_offset, &cu_pointer_size, &cu_offset_size,
	    &cu_extension_size, &cu_next_offset, &de) == DW_DLV_OK) {
		TS_CHECK_UINT(cu_header_length);
		TS_CHECK_UINT(cu_version);
		TS_CHECK_INT(cu_abbrev_offset);
		TS_CHECK_UINT(cu_pointer_size);
		TS_CHECK_UINT(cu_offset_size);
		TS_CHECK_UINT(cu_extension_size);
		TS_CHECK_UINT(cu_next_offset);
	}

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}

static void
tp_dwarf_next_cu_header_c(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	Dwarf_Unsigned cu_header_length;
	Dwarf_Half cu_version;
	Dwarf_Off cu_abbrev_offset;
	Dwarf_Half cu_pointer_size;
	Dwarf_Half cu_offset_size;
	Dwarf_Half cu_extension_size;
	Dwarf_Sig8 cu_type_sig;
	Dwarf_Unsigned cu_type_offset;
	Dwarf_Unsigned cu_next_offset;
	int fd, result;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	while (dwarf_next_cu_header_c(dbg, 1, &cu_header_length, &cu_version,
	    &cu_abbrev_offset, &cu_pointer_size, &cu_offset_size,
	    &cu_extension_size, NULL, NULL, &cu_next_offset, &de) ==
	    DW_DLV_OK) {
		TS_CHECK_UINT(cu_header_length);
		TS_CHECK_UINT(cu_version);
		TS_CHECK_INT(cu_abbrev_offset);
		TS_CHECK_UINT(cu_pointer_size);
		TS_CHECK_UINT(cu_offset_size);
		TS_CHECK_UINT(cu_extension_size);
		TS_CHECK_UINT(cu_next_offset);
	}

	do {
		while (dwarf_next_cu_header_c(dbg, 0, &cu_header_length,
		    &cu_version, &cu_abbrev_offset, &cu_pointer_size,
		    &cu_offset_size, &cu_extension_size, &cu_type_sig,
		    &cu_type_offset, &cu_next_offset, &de) == DW_DLV_OK) {
			TS_CHECK_UINT(cu_header_length);
			TS_CHECK_UINT(cu_version);
			TS_CHECK_INT(cu_abbrev_offset);
			TS_CHECK_UINT(cu_pointer_size);
			TS_CHECK_UINT(cu_offset_size);
			TS_CHECK_UINT(cu_extension_size);
			TS_CHECK_BLOCK(cu_type_sig.signature, 8);
			TS_CHECK_UINT(cu_type_offset);
			TS_CHECK_UINT(cu_next_offset);
		}
	} while (dwarf_next_types_section(dbg, &de) == DW_DLV_OK);

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}

#define	_LOOP_COUNT	50

static void
tp_dwarf_next_cu_header_loop(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	int i, r, fd, result;
	Dwarf_Unsigned cu_next_offset;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	for (i = 0; i < _LOOP_COUNT; i++) {
		tet_printf("dwarf_next_cu_header loop(%d)\n", i);
		r = dwarf_next_cu_header(dbg, NULL, NULL, NULL, NULL,
		    &cu_next_offset, &de);
		TS_CHECK_INT(r);
	}

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}
