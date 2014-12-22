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
 * $Id: dwarf_die_query.c 3075 2014-06-23 03:08:57Z kaiwang27 $
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
 * Test case for DIE query functions: dwarf_tag, dwarf_die_abbrev_code,
 * dwarf_diename and dwarf_dieoffset.
 */

static void tp_dwarf_die_query(void);
static void tp_dwarf_die_query_types(void);
static void tp_dwarf_die_query_sanity(void);
static struct dwarf_tp dwarf_tp_array[] = {
	{"tp_dwarf_die_query", tp_dwarf_die_query},
	{"tp_dwarf_die_query_types", tp_dwarf_die_query_types},
	{"tp_dwarf_die_query_sanity", tp_dwarf_die_query_sanity},
	{NULL, NULL},
};
static int result = TET_UNRESOLVED;
#include "driver.c"
#include "die_traverse.c"
#include "die_traverse2.c"

static void
_dwarf_die_query(Dwarf_Die die)
{
	Dwarf_Half tag;
	Dwarf_Error de;
	char *die_name;
	int abbrev_code, dwarf_diename_ret;

	/* Check DIE abbreviation code. */
	abbrev_code = dwarf_die_abbrev_code(die);
	TS_CHECK_INT(abbrev_code);

	/* Check DIE tag. */
	if (dwarf_tag(die, &tag, &de) != DW_DLV_OK) {
		tet_printf("dwarf_tag failed: %s\n", dwarf_errmsg(de));
		result = TET_FAIL;
	}
	TS_CHECK_UINT(tag);

	/* Check DIE name. */
	dwarf_diename_ret = dwarf_diename(die, &die_name, &de);
	TS_CHECK_INT(dwarf_diename_ret);
	if (dwarf_diename_ret == DW_DLV_ERROR) {
		tet_printf("dwarf_diename failed: %s\n", dwarf_errmsg(de));
		result = TET_FAIL;
	} else if (dwarf_diename_ret == DW_DLV_OK)
		TS_CHECK_STRING(die_name);
}

static void
tp_dwarf_die_query(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	int fd;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	TS_DWARF_DIE_TRAVERSE(dbg, _dwarf_die_query);

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}

static void
tp_dwarf_die_query_types(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	int fd;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	TS_DWARF_DIE_TRAVERSE2(dbg, 0, _dwarf_die_query);

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}

static void
tp_dwarf_die_query_sanity(void)
{
	Dwarf_Debug dbg;
	Dwarf_Die die;
	Dwarf_Error de;
	Dwarf_Half tag;
	Dwarf_Unsigned cu_next_offset;
	char *die_name;
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
		if (dwarf_tag(NULL, &tag, &de) != DW_DLV_ERROR) {
			tet_infoline("dwarf_tag didn't return DW_DLV_ERROR"
			    " when called with NULL arguments");
			result = TET_FAIL;
			goto done;
		}
		if (dwarf_tag(die, &tag, &de) != DW_DLV_OK) {
			tet_printf("dwarf_tag failed: %s", dwarf_errmsg(de));
			result = TET_FAIL;
			goto done;
		}
		TS_CHECK_UINT(tag);
		
		if (dwarf_diename(NULL, &die_name, &de) != DW_DLV_ERROR) {
			tet_infoline("dwarf_diename didn't return DW_DLV_ERROR"
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
