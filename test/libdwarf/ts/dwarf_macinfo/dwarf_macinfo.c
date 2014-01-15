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
 * $Id: dwarf_macinfo.c 2084 2011-10-27 04:48:12Z jkoshy $
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
 * Test case for dwarf macro information API.
 */
static void tp_dwarf_macinfo(void);
static struct dwarf_tp dwarf_tp_array[] = {
	{"tp_dwarf_macinfo", tp_dwarf_macinfo},
	{NULL, NULL},
};
static int result = TET_UNRESOLVED;
#include "driver.c"

static void
_check_details(Dwarf_Macro_Details *details, Dwarf_Signed entry_count)
{
	char *macro_value;
	int i;

	for (i = 0; i < entry_count; i++) {
		tet_printf("check macro details entry %d:\n", i);
		TS_CHECK_INT(details[i].dmd_offset);
		TS_CHECK_UINT(details[i].dmd_type);
		TS_CHECK_INT(details[i].dmd_lineno);
		TS_CHECK_INT(details[i].dmd_fileindex);
		if (details[i].dmd_macro != NULL) {
			TS_CHECK_STRING(details[i].dmd_macro);
			macro_value = dwarf_find_macro_value_start(details[i].dmd_macro);
			if (macro_value != NULL)
				TS_CHECK_STRING(macro_value);
		}
	}
}

static void
_get_macinfo(Dwarf_Debug dbg, Dwarf_Off macro_offset, Dwarf_Unsigned max_count)
{
	Dwarf_Macro_Details *details;
	Dwarf_Signed entry_count;
	Dwarf_Error de;
	int r_details;

	r_details = dwarf_get_macro_details(dbg, macro_offset, max_count,
	    &entry_count, &details, &de);
	TS_CHECK_INT(r_details);
	if (r_details != DW_DLV_OK) {
		if (r_details == DW_DLV_ERROR) {
			tet_printf("dwarf_get_macro_details failed: %s\n",
			    dwarf_errmsg(de));
			result = TET_FAIL;
		}
		return;
	}
	TS_CHECK_UINT(entry_count);
	_check_details(details, entry_count);
}

static void
_get_all_macinfo(Dwarf_Debug dbg)
{
	Dwarf_Macro_Details *details;
	Dwarf_Signed entry_count;
	Dwarf_Off off;
	Dwarf_Error de;
	int r_details;

	off = 0;
	while ((r_details = dwarf_get_macro_details(dbg, off, 0, &entry_count,
	    &details, &de)) == DW_DLV_OK) {
		TS_CHECK_UINT(entry_count);
		_check_details(details, entry_count);
		off = details[entry_count - 1].dmd_offset + 1;
	}
	TS_CHECK_INT(r_details);
}

static void
tp_dwarf_macinfo(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	int fd;

	TS_DWARF_INIT(dbg, fd, de);

	/* Get all Dwarf_Macro_Details entries in the first CU. */
	_get_macinfo(dbg, 0, 0);

	/* Get first 100 entries. */
	_get_macinfo(dbg, 0, 100);

	/* Get all entries in all CUs. */
	_get_all_macinfo(dbg);

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}
