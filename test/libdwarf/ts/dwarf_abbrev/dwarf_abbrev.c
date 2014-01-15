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
 * $Id: dwarf_abbrev.c 2084 2011-10-27 04:48:12Z jkoshy $
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
 * Test case for dwarf address range API.
 */
static void tp_dwarf_abbrev(void);
static struct dwarf_tp dwarf_tp_array[] = {
	{"tp_dwarf_abbrev", tp_dwarf_abbrev},
	{NULL, NULL},
};
static int result = TET_UNRESOLVED;
#include "driver.c"

static void
tp_dwarf_abbrev(void)
{
	Dwarf_Debug dbg;
	Dwarf_Abbrev ab;
	Dwarf_Unsigned off, length, attr_count, code;
	Dwarf_Signed children_flag, form;
	Dwarf_Half tag, attr_num;
	Dwarf_Off attr_off;
	Dwarf_Error de;
	int fd, r_abbrev, r_abbrev_entry, i;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	off = 0;
	for (;;) {
		tet_printf("check abbrev at offset(%ju):\n", (uintmax_t) off);
		r_abbrev = dwarf_get_abbrev(dbg, off, &ab, &length,
		    &attr_count, &de);
		off += length;
		TS_CHECK_INT(r_abbrev);
		if (r_abbrev != DW_DLV_OK)
			break;
		TS_CHECK_UINT(length);
		TS_CHECK_UINT(attr_count);
		if (dwarf_get_abbrev_tag(ab, &tag, &de) != DW_DLV_OK) {
			tet_printf("dwarf_get_abbrev_tag failed: %s\n",
			    dwarf_errmsg(de));
			result = TET_FAIL;
		}
		TS_CHECK_UINT(tag);
		if (dwarf_get_abbrev_code(ab, &code, &de) != DW_DLV_OK) {
			tet_printf("dwarf_get_abbrev_code failed: %s\n",
			    dwarf_errmsg(de));
			result = TET_FAIL;
		}
		TS_CHECK_UINT(code);
		if (dwarf_get_abbrev_children_flag(ab, &children_flag, &de) !=
		    DW_DLV_OK) {
			tet_printf("dwarf_get_abbrev_children_flag failed: "
			    "%s\n", dwarf_errmsg(de));
			result = TET_FAIL;
		}
		TS_CHECK_INT(children_flag);
		for (i = 0; i < attr_count; i++) {
			tet_printf("check attr %d:\n", i);
			if (dwarf_get_abbrev_entry(ab, i, &attr_num, &form,
			    &attr_off, &de) != DW_DLV_OK) {
				tet_printf("dwarf_get_abbrev_entry failed: "
				    "%s\n", dwarf_errmsg(de));
				result = TET_FAIL;
				continue;
			}
			TS_CHECK_UINT(attr_num);
			TS_CHECK_INT(form);
			TS_CHECK_INT(attr_off);
		}
		/* Invalid index. */
		r_abbrev_entry = dwarf_get_abbrev_entry(ab, i + 10, &attr_num,
		    &form, &attr_off, &de);
		TS_CHECK_INT(r_abbrev_entry);
	}
	if (r_abbrev == DW_DLV_ERROR) {
		tet_printf("dwarf_get_abbrev failed: %s\n", dwarf_errmsg(de));
		result = TET_FAIL;
	}

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}
