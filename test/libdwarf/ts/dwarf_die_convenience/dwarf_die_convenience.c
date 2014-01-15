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
 * $Id: dwarf_die_convenience.c 2084 2011-10-27 04:48:12Z jkoshy $
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
 * Test case for a few convenience functions used to retrieve certain
 * attribute values from DIE.
 */

static void tp_dwarf_die_convenience(void);
static void tp_dwarf_die_convenience_sanity(void);
static struct dwarf_tp dwarf_tp_array[] = {
	{"tp_dwarf_die_convenience", tp_dwarf_die_convenience},
	{"tp_dwarf_die_convenience_sanity", tp_dwarf_die_convenience_sanity},
	{NULL, NULL},
};
static int result = TET_UNRESOLVED;
#include "driver.c"
#include "die_traverse.c"

static void
_dwarf_die_convenience(Dwarf_Die die)
{
	Dwarf_Error de;
	Dwarf_Addr highpc, lowpc;
	Dwarf_Unsigned arrayorder, bitoffset, bitsize, bytesize;
	Dwarf_Unsigned srclang;
	int r_arrayorder, r_bitoffset, r_bitsize, r_bytesize;
	int r_highpc, r_lowpc, r_srclang;

	r_arrayorder = dwarf_arrayorder(die, &arrayorder, &de);
	TS_CHECK_INT(r_arrayorder);
	if (r_arrayorder == DW_DLV_ERROR) {
		tet_printf("dwarf_arrayorder failed: %s\n", dwarf_errmsg(de));
		result = TET_FAIL;
	} else if (r_arrayorder == DW_DLV_OK)
		TS_CHECK_UINT(arrayorder);

	r_bitoffset = dwarf_bitoffset(die, &bitoffset, &de);
	TS_CHECK_INT(r_bitoffset);
	if (r_bitoffset == DW_DLV_ERROR) {
		tet_printf("dwarf_bitoffset failed: %s\n", dwarf_errmsg(de));
		result = TET_FAIL;
	} else if (r_bitoffset == DW_DLV_OK)
		TS_CHECK_UINT(bitoffset);

	r_bitsize = dwarf_bitsize(die, &bitsize, &de);
	TS_CHECK_INT(r_bitsize);
	if (r_bitsize == DW_DLV_ERROR) {
		tet_printf("dwarf_bitsize failed: %s\n", dwarf_errmsg(de));
		result = TET_FAIL;
	} else if (r_bitsize == DW_DLV_OK)
		TS_CHECK_UINT(bitsize);

	r_bytesize = dwarf_bytesize(die, &bytesize, &de);
	TS_CHECK_INT(r_bytesize);
	if (r_bytesize == DW_DLV_ERROR) {
		tet_printf("dwarf_bytesize failed: %s\n", dwarf_errmsg(de));
		result = TET_FAIL;
	} else if (r_bytesize == DW_DLV_OK)
		TS_CHECK_UINT(bytesize);

	r_highpc = dwarf_highpc(die, &highpc, &de);
	TS_CHECK_INT(r_highpc);
	if (r_highpc == DW_DLV_ERROR) {
		tet_printf("dwarf_highpc failed: %s\n", dwarf_errmsg(de));
		result = TET_FAIL;
	} else if (r_highpc == DW_DLV_OK)
		TS_CHECK_UINT(highpc);

	r_lowpc = dwarf_lowpc(die, &lowpc, &de);
	TS_CHECK_INT(r_lowpc);
	if (r_lowpc == DW_DLV_ERROR) {
		tet_printf("dwarf_lowpc failed: %s\n", dwarf_errmsg(de));
		result = TET_FAIL;
	} else if (r_lowpc == DW_DLV_OK)
		TS_CHECK_UINT(lowpc);

	r_srclang = dwarf_srclang(die, &srclang, &de);
	TS_CHECK_INT(r_srclang);
	if (r_srclang == DW_DLV_ERROR) {
		tet_printf("dwarf_srclang failed: %s\n", dwarf_errmsg(de));
		result = TET_FAIL;
	} else if (r_srclang == DW_DLV_OK)
		TS_CHECK_UINT(srclang);
}

static void
tp_dwarf_die_convenience(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	int fd;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	TS_DWARF_DIE_TRAVERSE(dbg, _dwarf_die_convenience);

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}

static void
tp_dwarf_die_convenience_sanity(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	Dwarf_Die die;
	Dwarf_Addr highpc, lowpc;
	Dwarf_Unsigned arrayorder, bitoffset, bitsize, bytesize;
	Dwarf_Unsigned srclang, cu_next_offset;
	int r, fd;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	TS_DWARF_CU_FOREACH(dbg, cu_next_offset, de) {
		if (dwarf_siblingof(dbg, NULL, &die, &de) == DW_DLV_ERROR) {
			tet_printf("dwarf_siblingof failed: %s\n",
			    dwarf_errmsg(de));
			result = TET_FAIL;
			goto done;
		}
		if (dwarf_arrayorder(NULL, &arrayorder, &de) != DW_DLV_ERROR) {
			tet_infoline("dwarf_arrayorder didn't return"
			    " DW_DLV_ERROR when called with NULL arguments");
			result = TET_FAIL;
			goto done;
		}
		if (dwarf_bitoffset(NULL, &bitoffset, &de) != DW_DLV_ERROR) {
			tet_infoline("dwarf_bitoffset didn't return"
			    " DW_DLV_ERROR when called with NULL arguments");
			result = TET_FAIL;
			goto done;
		}
		if (dwarf_bitsize(NULL, &bitsize, &de) != DW_DLV_ERROR) {
			tet_infoline("dwarf_bitsize didn't return"
			    " DW_DLV_ERROR when called with NULL arguments");
			result = TET_FAIL;
			goto done;
		}
		if (dwarf_bytesize(NULL, &bytesize, &de) != DW_DLV_ERROR) {
			tet_infoline("dwarf_bytesize didn't return"
			    " DW_DLV_ERROR when called with NULL arguments");
			result = TET_FAIL;
			goto done;
		}

		if (dwarf_highpc(NULL, &highpc, &de) != DW_DLV_ERROR) {
			tet_infoline("dwarf_highpc didn't return"
			    " DW_DLV_ERROR when called with NULL arguments");
			result = TET_FAIL;
			goto done;
		}
		r = dwarf_highpc(die, &highpc, &de);
		if (r == DW_DLV_ERROR) {
			tet_printf("dwarf_highpc failed: %s\n",
			    dwarf_errmsg(de));
			result = TET_FAIL;
			goto done;
		} else if (r == DW_DLV_OK)
			TS_CHECK_UINT(highpc);

		if (dwarf_lowpc(NULL, &lowpc, &de) != DW_DLV_ERROR) {
			tet_infoline("dwarf_lowpc didn't return"
			    " DW_DLV_ERROR when called with NULL arguments");
			result = TET_FAIL;
			goto done;
		}
		r = dwarf_lowpc(die, &lowpc, &de);
		if (r == DW_DLV_ERROR) {
			tet_printf("dwarf_lowpc failed: %s\n",
			    dwarf_errmsg(de));
			result = TET_FAIL;
			goto done;
		} else if (r == DW_DLV_OK)
			TS_CHECK_UINT(lowpc);

		if (dwarf_srclang(NULL, &srclang, &de) != DW_DLV_ERROR) {
			tet_infoline("dwarf_srclang didn't return"
			    " DW_DLV_ERROR when called with NULL arguments");
			result = TET_FAIL;
			goto done;
		}
		r = dwarf_srclang(die, &srclang, &de);
		if (r == DW_DLV_ERROR) {
			tet_printf("dwarf_srclang failed: %s\n",
			    dwarf_errmsg(de));
			result = TET_FAIL;
			goto done;
		} else if (r == DW_DLV_OK)
			TS_CHECK_UINT(srclang);
	}

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}
