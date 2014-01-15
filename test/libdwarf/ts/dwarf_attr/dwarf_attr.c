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
 * $Id: dwarf_attr.c 2084 2011-10-27 04:48:12Z jkoshy $
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
 * Test case for dwarf_attr, dwarf_hasattr and dwarf_whatattr etc.
 */
static void tp_dwarf_attr(void);
static void tp_dwarf_attr_sanity(void);
static struct dwarf_tp dwarf_tp_array[] = {
	{"tp_dwarf_attr", tp_dwarf_attr},
	{"tp_dwarf_attr_sanity", tp_dwarf_attr_sanity},
	{NULL, NULL},
};
static int result = TET_UNRESOLVED;
#include "driver.c"
#include "die_traverse.c"

static Dwarf_Half attr_array[] = { DW_AT_ordering,
				   DW_AT_bit_offset,
				   DW_AT_bit_size,
				   DW_AT_byte_size,
				   DW_AT_high_pc,
				   DW_AT_low_pc,
				   DW_AT_language,
				   DW_AT_name,
				   DW_AT_data_member_location,
				   DW_AT_producer,
				   DW_AT_comp_dir,
				   DW_AT_location,
				   DW_AT_decl_file,
				   DW_AT_decl_line };
static int attr_array_size = sizeof(attr_array) / sizeof(Dwarf_Half);

static void
_dwarf_attr(Dwarf_Die die)
{
	Dwarf_Attribute at;
	Dwarf_Bool has_attr;
	Dwarf_Half attr;
	Dwarf_Error de;
	const char *attr_name;
	int i, r;

	for (i = 0; i < attr_array_size; i++) {
		if (dwarf_hasattr(die, attr_array[i], &has_attr, &de) !=
		    DW_DLV_OK) {
			tet_printf("dwarf_hasattr failed: %s\n",
			    dwarf_errmsg(de));
			result = TET_FAIL;
		}
		TS_CHECK_INT(has_attr);

		if (has_attr) {
			if (dwarf_get_AT_name(attr_array[i], &attr_name) !=
			    DW_DLV_OK) {
				tet_printf("dwarf_get_AT_name failed: %s\n",
				    dwarf_errmsg(de));
				result = TET_FAIL;
				continue;
			}
			if (attr_name == NULL) {
				tet_infoline("dwarf_get_AT_name returned "
				    "DW_DLV_OK but didn't return string");
				result = TET_FAIL;
				continue;
			}
			TS_CHECK_STRING(attr_name);

			tet_printf("DIE #%d has attribute '%s'\n", die_cnt,
			    attr_name);

			r = dwarf_attr(die, attr_array[i], &at, &de);
			if (r == DW_DLV_ERROR) {
				tet_printf("dwarf_attr failed: %s",
				    dwarf_errmsg(de));
				result = TET_FAIL;
				continue;
			} else if (r == DW_DLV_NO_ENTRY) {
				tet_infoline("dwarf_hasattr returned true for "
				    "attribute '%s', while dwarf_attr returned"
				    " DW_DLV_NO_ENTRY for the same attr");
				result = TET_FAIL;
				continue;
			}
			if (dwarf_whatattr(at, &attr, &de) != DW_DLV_OK) {
				tet_printf("dwarf_whatattr failed: %s",
				    dwarf_errmsg(de));
				result = TET_FAIL;
				continue;
			}
			if (attr != attr_array[i]) {
				tet_infoline("attr returned by dwarf_whatattr"
				    " != attr_array[i]");
				result = TET_FAIL;
				continue;
			}
		}
	}
}

static void
tp_dwarf_attr(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	int fd;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	TS_DWARF_DIE_TRAVERSE(dbg, _dwarf_attr);

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}

static void
tp_dwarf_attr_sanity(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	Dwarf_Bool has_attr;
	Dwarf_Half attr;
	Dwarf_Attribute at;
	int fd;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	if (dwarf_hasattr(NULL, DW_AT_name, &has_attr, &de) != DW_DLV_ERROR) {
		tet_infoline("dwarf_hasattr didn't return DW_DLV_ERROR"
		    " when called with NULL arguments");
		result = TET_FAIL;
		goto done;
	}

	if (dwarf_attr(NULL, DW_AT_name, &at, &de) != DW_DLV_ERROR) {
		tet_infoline("dwarf_attr didn't return DW_DLV_ERROR"
		    " when called with NULL arguments");
		result = TET_FAIL;
		goto done;
	}

	if (dwarf_whatattr(NULL, &attr, &de) != DW_DLV_ERROR) {
		tet_infoline("dwarf_whatattr didn't return DW_DLV_ERROR"
		    " when called with NULL arguments");
		result = TET_FAIL;
		goto done;
	}

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}
