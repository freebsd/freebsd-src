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
 * $Id: dwarf_ranges.c 2084 2011-10-27 04:48:12Z jkoshy $
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
static void tp_dwarf_ranges(void);
static struct dwarf_tp dwarf_tp_array[] = {
	{"tp_dwarf_ranges", tp_dwarf_ranges},
	{NULL, NULL},
};
static int result = TET_UNRESOLVED;
#include "driver.c"

static void
tp_dwarf_ranges(void)
{
	Dwarf_Debug dbg;
	Dwarf_Ranges *ranges;
	Dwarf_Signed range_cnt;
	Dwarf_Unsigned byte_cnt;
	Dwarf_Off off;
	Dwarf_Error de;
	int fd, r_ranges, i;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	off = 0;
	for (;;) {
		tet_printf("check ranges at offset(%ju):\n", (uintmax_t) off);
		r_ranges = dwarf_get_ranges_a(dbg, off, NULL, &ranges,
		    &range_cnt, &byte_cnt, &de);
		if (r_ranges != DW_DLV_OK)
			break;
		TS_CHECK_INT(range_cnt);
		TS_CHECK_UINT(byte_cnt);
		off += byte_cnt;
		for (i = 0; i < range_cnt; i++) {
			tet_printf("check range %d:\n", i);
			TS_CHECK_INT(ranges[i].dwr_type);
			TS_CHECK_UINT(ranges[i].dwr_addr1);
			TS_CHECK_UINT(ranges[i].dwr_addr2);
		}
	}

	/*
	 * SGI libdwarf return DW_DLV_ERROR when provided offset is out of
	 * range, instead of DW_DLV_NO_ENTRY as stated in the SGI libdwarf
	 * documentation. elftoolchain libdwarf follows the SGI libdwarf
	 * documentation.
	 */
#if 0
	if (r_ranges == DW_DLV_ERROR) {
		tet_printf("dwarf_get_ranges failed: %s\n", dwarf_errmsg(de));
		result = TET_FAIL;
	}
#endif

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}
