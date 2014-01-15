/*-
 * Copyright (c) 2010 Kai Wang
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
 * $Id: dwarf_die_offset.c 2084 2011-10-27 04:48:12Z jkoshy $
 */

#include <assert.h>
#include <dwarf.h>
#include <errno.h>
#include <fcntl.h>
#include <libdwarf.h>
#include <string.h>

#include "driver.h"
#include "tet_api.h"

/*
 * Test case for DIE offset query functions: dwarf_die_CU_offset,
 * dwarf_die_CU_offset_range, dwarf_dieoffset and
 * dwarf_get_cu_die_offset_given_cu_header_offset.
 */

static void tp_dwarf_die_offset(void);
static void tp_dwarf_die_offset_given_cu(void);
static void tp_dwarf_die_offset_sanity(void);
static struct dwarf_tp dwarf_tp_array[] = {
	{"tp_dwarf_die_offset", tp_dwarf_die_offset},
	{"tp_dwarf_die_offset_given_cu", tp_dwarf_die_offset_given_cu},
	{"tp_dwarf_die_offset_sanity", tp_dwarf_die_offset_sanity},
	{NULL, NULL},
};
static int result = TET_UNRESOLVED;
#include "driver.c"
#include "die_traverse.c"

static void
_dwarf_die_offset(Dwarf_Die die)
{
	Dwarf_Off rel_off, die_off, cu_off, cu_len;
	Dwarf_Error de;

	if (dwarf_die_CU_offset(die, &rel_off, &de) != DW_DLV_OK) {
		tet_printf("dwarf_die_CU_offset failed: %s\n",
		    dwarf_errmsg(de));
		result = TET_FAIL;
	}
	TS_CHECK_INT(rel_off);

	if (dwarf_die_CU_offset_range(die, &cu_off, &cu_len, &de) !=
	    DW_DLV_OK) {
		tet_printf("dwarf_die_CU_offset_range failed: %s\n",
		    dwarf_errmsg(de));
		result = TET_FAIL;
	}
	TS_CHECK_INT(cu_off);
	TS_CHECK_INT(cu_len);

	if (dwarf_dieoffset(die, &die_off, &de) != DW_DLV_OK) {
		tet_printf("dwarf_dieoffset failed: %s\n", dwarf_errmsg(de));
		result = TET_FAIL;
	}
	TS_CHECK_INT(die_off);
}

static void
tp_dwarf_die_offset(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	int fd;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	TS_DWARF_DIE_TRAVERSE(dbg, _dwarf_die_offset);

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}

static void
tp_dwarf_die_offset_given_cu(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	Dwarf_Off cu_offset, cu_dieoff;
	Dwarf_Unsigned cu_next_offset;
	int fd;
	
	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	cu_offset = 0;
	TS_DWARF_CU_FOREACH(dbg, cu_next_offset, de) {
		if (dwarf_get_cu_die_offset_given_cu_header_offset(dbg,
		    cu_offset, &cu_dieoff, &de) != DW_DLV_OK) {
			tet_printf("dwarf_get_cu_die_offset_given_cu_header"
			    "_offset failed: %s", dwarf_errmsg(de));
			result = TET_FAIL;
			goto done;
		}
		TS_CHECK_INT(cu_dieoff);
		cu_offset = cu_next_offset;
	}

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}

static void
tp_dwarf_die_offset_sanity(void)
{
	Dwarf_Debug dbg;
	Dwarf_Die die;
	Dwarf_Error de;
	Dwarf_Off rel_off, die_off, cu_off, cu_len;
	Dwarf_Unsigned cu_next_offset;
	int fd;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	TS_DWARF_CU_FOREACH(dbg, cu_next_offset, de) {
		if (dwarf_siblingof(dbg, NULL, &die, &de) == DW_DLV_ERROR) {
			tet_printf("dwarf_siblingof failed: %s\n",
			    dwarf_errmsg(de));
			result = TET_FAIL;
			goto done;
		}
		if (dwarf_die_CU_offset(NULL, &rel_off, &de) != DW_DLV_ERROR) {
			tet_infoline("dwarf_die_CU_offset didn't return"
			    " DW_DLV_ERROR when called with NULL arguments");
			result = TET_FAIL;
			goto done;
		}
		if (dwarf_die_CU_offset_range(NULL, &cu_off, &cu_len, &de) !=
		    DW_DLV_ERROR) {
			tet_infoline("dwarf_die_CU_offset_range didn't return"
			    " DW_DLV_ERROR when called with NULL arguments");
			result = TET_FAIL;
			goto done;
		}
		if (dwarf_dieoffset(NULL, &die_off, &de) != DW_DLV_ERROR) {
			tet_infoline("dwarf_dieoffset didn't return DW_DLV_ERROR"
			    " when called with NULL arguments");
			result = TET_FAIL;
			goto done;
		}
	}

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}
