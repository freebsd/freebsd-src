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
 * $Id: dwarf_arange.c 2084 2011-10-27 04:48:12Z jkoshy $
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
static void tp_dwarf_arange(void);
static struct dwarf_tp dwarf_tp_array[] = {
	{"tp_dwarf_arange", tp_dwarf_arange},
	{NULL, NULL},
};
static int result = TET_UNRESOLVED;
#include "driver.c"

static void
tp_dwarf_arange(void)
{
	Dwarf_Debug dbg;
	Dwarf_Arange *aranges;
	Dwarf_Arange arange;
	Dwarf_Signed arange_cnt;
	Dwarf_Off cu_die_offset, cu_die_offset2, cu_header_offset;
	Dwarf_Addr start;
	Dwarf_Unsigned length;
	Dwarf_Error de;
	int fd, i, r_aranges, r_arange;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	r_aranges = dwarf_get_aranges(dbg, &aranges, &arange_cnt, &de);
	TS_CHECK_INT(r_aranges);
	if (r_aranges == DW_DLV_ERROR) {
		tet_printf("dwarf_get_aranges failed: %s\n", dwarf_errmsg(de));
		result = TET_FAIL;
		goto done;
	}
	if (r_aranges == DW_DLV_OK) {
		for (i = 0; i < arange_cnt; i++) {
			if (dwarf_get_cu_die_offset(aranges[i], &cu_die_offset,
			    &de) != DW_DLV_OK) {
				tet_printf("dwarf_get_cu_die_offset failed:"
				    " %s\n", dwarf_errmsg(de));
				result = TET_FAIL;
				continue;
			}
			TS_CHECK_INT(cu_die_offset);
			if (dwarf_get_arange_cu_header_offset(aranges[i],
			    &cu_header_offset, &de) != DW_DLV_OK) {
				tet_printf("dwarf_get_arange_cu_header_offset"
				    "failed: %s\n", dwarf_errmsg(de));
				result = TET_FAIL;
				continue;
			}
			TS_CHECK_INT(cu_header_offset);
			if (dwarf_get_arange_info(aranges[i], &start, &length,
			    &cu_die_offset2, &de) != DW_DLV_OK) {
				tet_printf("dwarf_get_arange_info failed:%s\n",
				    dwarf_errmsg(de));
				result = TET_FAIL;
				continue;
			}
			TS_CHECK_UINT(start);
			TS_CHECK_UINT(length);
			TS_CHECK_UINT(cu_die_offset2);
			r_arange = dwarf_get_arange(aranges, arange_cnt, start,
			    &arange, &de);
			TS_CHECK_INT(r_arange);
			r_arange = dwarf_get_arange(aranges, arange_cnt,
			    start + 1, &arange, &de);
			TS_CHECK_INT(r_arange);
			r_arange = dwarf_get_arange(aranges, arange_cnt,
			    start + length, &arange, &de);
			TS_CHECK_INT(r_arange);
			r_arange = dwarf_get_arange(aranges, arange_cnt,
			    start + length + 1, &arange, &de);
			TS_CHECK_INT(r_arange);
			r_arange = dwarf_get_arange(aranges, arange_cnt,
			    start + length - 1, &arange, &de);
			TS_CHECK_INT(r_arange);
		}
	}


	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}
