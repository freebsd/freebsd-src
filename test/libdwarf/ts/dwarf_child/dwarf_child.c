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
 * $Id: dwarf_child.c 2084 2011-10-27 04:48:12Z jkoshy $
 */

#include <assert.h>
#include <dwarf.h>
#include <errno.h>
#include <fcntl.h>
#include <libdwarf.h>
#include <string.h>

#include "driver.h"
#include "tet_api.h"

static void tp_dwarf_child_first(void);
static void tp_dwarf_child_sanity(void);
static struct dwarf_tp dwarf_tp_array[] = {
	{"tp_dwarf_child_first", tp_dwarf_child_first},
	{"tp_dwarf_child_sanity", tp_dwarf_child_sanity},
	{NULL, NULL},
};
#include "driver.c"

static void
tp_dwarf_child_first(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	Dwarf_Die die, die0;
	Dwarf_Unsigned cu_next_offset;
	int r, fd, result, die_cnt;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	tet_infoline("count the number of children of compilation unit DIE");

	die_cnt = 0;
	TS_DWARF_CU_FOREACH(dbg, cu_next_offset, de) {
		r = dwarf_siblingof(dbg, NULL, &die, &de);
		if (r == DW_DLV_OK) {
			r = dwarf_child(die, &die0, &de);
			while (r == DW_DLV_OK) {
				if (die0 == NULL) {
					tet_infoline("dwarf_child or "
					    "dwarf_siblingof return "
					    "DW_DLV_OK while argument die0 "
					    "is not filled in");
					result = TET_FAIL;
					goto done;
				}
				die_cnt++;
				die = die0;
				r = dwarf_siblingof(dbg, die, &die0, &de);
			}
		}
		if (r == DW_DLV_ERROR) {
			tet_printf("dwarf_siblingof or dwarf_child failed:"
			    " %s\n", dwarf_errmsg(de));
			result = TET_FAIL;
			goto done;
		}
	}

	TS_CHECK_INT(die_cnt);

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}

static void
tp_dwarf_child_sanity(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	Dwarf_Die die;
	Dwarf_Unsigned cu_next_offset;
	int fd, result;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	if (dwarf_child(NULL, &die, &de) != DW_DLV_ERROR ||
	    dwarf_child(NULL, NULL, &de) != DW_DLV_ERROR) {
		tet_infoline("dwarf_child didn't return DW_DLV_ERROR when"
		    " called with NULL arguments");
		result = TET_FAIL;
		goto done;

	}

	TS_DWARF_CU_FOREACH(dbg, cu_next_offset, de) {
		if (dwarf_child(NULL, &die, &de) != DW_DLV_ERROR ||
		    dwarf_child(NULL, NULL, &de) != DW_DLV_ERROR) {
			tet_infoline("dwarf_child didn't return DW_DLV_ERROR"
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
