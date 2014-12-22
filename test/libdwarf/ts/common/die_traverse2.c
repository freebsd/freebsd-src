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
 * $Id: die_traverse2.c 3076 2014-06-23 23:54:01Z kaiwang27 $
 */

#include <assert.h>
#include <dwarf.h>
#include <libdwarf.h>

#include "driver.h"

static int die_cnt;

/*
 * DIE traverse function shared by test cases. (another version with
 * .debug_types support)
 */

static void
_die_traverse_recursive2(Dwarf_Debug dbg, Dwarf_Die die,
    Dwarf_Bool is_info, void (*die_callback)(Dwarf_Die die))
{
	Dwarf_Die die0;
	Dwarf_Off offset;
	Dwarf_Half tag;
	Dwarf_Error de;
	const char *tagname;
	int r;

	assert(dbg != NULL && die != NULL && die_callback != NULL);

	if (dwarf_tag(die, &tag, &de) != DW_DLV_OK) {
		tet_printf("dwarf_tag failed: %s\n", dwarf_errmsg(de));
		result = TET_FAIL;
	}
	tagname = NULL;
	if (dwarf_get_TAG_name(tag, &tagname) != DW_DLV_OK) {
		tet_infoline("dwarf_get_TAG_name failed");
		result = TET_FAIL;
	}
	offset = 0;
	if (dwarf_dieoffset(die, &offset, &de) != DW_DLV_OK) {
		tet_printf("dwarf_dieoffset failed: %s\n", dwarf_errmsg(de));
		result = TET_FAIL;
	}
	tet_printf("DIE #%d (%s) [%#x]\n", die_cnt++, tagname, offset);

	die_callback(die);

	/* Search children. */
	r = dwarf_child(die, &die0, &de);
	if (r == DW_DLV_ERROR)
		tet_printf("%s: dwarf_child failed: %s", __func__,
		    dwarf_errmsg(de));
	else if (r == DW_DLV_OK)
		_die_traverse_recursive2(dbg, die0, is_info, die_callback);

	/* Search sibling. */
	r = dwarf_siblingof_b(dbg, die, &die0, is_info, &de);
	if (r == DW_DLV_ERROR)
		tet_printf("%s: dwarf_siblingof failed: %s", __func__,
		    dwarf_errmsg(de));
	else if (r == DW_DLV_OK)
		_die_traverse_recursive2(dbg, die0, is_info, die_callback);
}

static void
_die_traverse2(Dwarf_Debug dbg, Dwarf_Bool is_info,
    void (*die_callback)(Dwarf_Die die))
{
	Dwarf_Die die;
	Dwarf_Error de;
	Dwarf_Unsigned cu_next_offset;

	assert(dbg != NULL && die_callback != NULL);

	die_cnt = 0;

	if (is_info) {
		TS_DWARF_CU_FOREACH2(dbg, 1, cu_next_offset, de) {
			if (dwarf_siblingof_b(dbg, NULL, &die, 1, &de) !=
			    DW_DLV_OK)
				break;
			_die_traverse_recursive2(dbg, die, 1, die_callback);
		}
	} else {
		do {
			TS_DWARF_CU_FOREACH2(dbg, 0, cu_next_offset, de) {
				if (dwarf_siblingof_b(dbg, NULL, &die, 0,
				    &de) != DW_DLV_OK)
				break;
				_die_traverse_recursive2(dbg, die, 0,
				    die_callback);
			}
		} while (dwarf_next_types_section(dbg, &de) == DW_DLV_OK);
	}
}
