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
 * $Id: dwarf_pubnames.c 2084 2011-10-27 04:48:12Z jkoshy $
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
static void tp_dwarf_pubnames(void);
static struct dwarf_tp dwarf_tp_array[] = {
	{"tp_dwarf_pubnames", tp_dwarf_pubnames},
	{NULL, NULL},
};
static int result = TET_UNRESOLVED;
#include "driver.c"

static void
tp_dwarf_pubnames(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	Dwarf_Global *globals;
	Dwarf_Signed global_cnt;
	Dwarf_Off die_off, cu_off;
	char *glob_name;
	int fd, r_globals, i;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	r_globals = dwarf_get_globals(dbg, &globals, &global_cnt, &de);
	TS_CHECK_INT(r_globals);
	if (r_globals == DW_DLV_ERROR) {
		tet_printf("dwarf_get_globals failed: %s\n", dwarf_errmsg(de));
		result = TET_FAIL;
		goto done;
	}
	TS_CHECK_INT(global_cnt);
	if (r_globals == DW_DLV_OK) {
		for (i = 0; i < global_cnt; i++) {
			if (dwarf_globname(globals[i], &glob_name, &de) !=
			    DW_DLV_OK) {
				tet_printf("dwarf_globname failed: %s\n",
				    dwarf_errmsg(de));
				result = TET_FAIL;
			}
			TS_CHECK_STRING(glob_name);
			if (dwarf_global_die_offset(globals[i], &die_off,
			    &de) != DW_DLV_OK) {
				tet_printf("dwarf_global_die_offset failed: "
				    "%s\n", dwarf_errmsg(de));
				result = TET_FAIL;
			}
			TS_CHECK_INT(die_off);
			if (dwarf_global_cu_offset(globals[i], &cu_off,
			    &de) != DW_DLV_OK) {
				tet_printf("dwarf_global_cu_offset failed: "
				    "%s\n", dwarf_errmsg(de));
				result = TET_FAIL;
			}
			TS_CHECK_INT(cu_off);
			if (dwarf_global_name_offsets(globals[i], &glob_name,
			    &die_off, &cu_off, &de) != DW_DLV_OK) {
				tet_printf("dwarf_global_name_offsets failed: ",
				    "%s\n", dwarf_errmsg(de));
				result = TET_FAIL;
			}
			TS_CHECK_STRING(glob_name);
			TS_CHECK_INT(die_off);
			TS_CHECK_INT(cu_off);
		}
	}

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}
