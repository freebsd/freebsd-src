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
 * $Id: dwarf_loclist.c 2084 2011-10-27 04:48:12Z jkoshy $
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
 * Test case for DWARF loclist query functions.
 */
static void tp_dwarf_loclist(void);
static void tp_dwarf_loclist_sanity(void);
static struct dwarf_tp dwarf_tp_array[] = {
	{"tp_dwarf_loclist", tp_dwarf_loclist},
	{"tp_dwarf_loclist_sanity", tp_dwarf_loclist_sanity},
	{NULL, NULL},
};
static int result = TET_UNRESOLVED;
#include "driver.c"
#include "die_traverse.c"

static void
_dwarf_loclist(Dwarf_Die die)
{
	Dwarf_Attribute *attrlist, at;
	Dwarf_Signed attrcount;
	Dwarf_Half attr;
	Dwarf_Locdesc **llbuf, *llbuf0;
	Dwarf_Loc *loc;
	Dwarf_Signed listlen;
	Dwarf_Error de;
	const char *atname;
	int r, i, j, k;
	int r_loclist, r_loclist_n;

	r = dwarf_attrlist(die, &attrlist, &attrcount, &de);
	if (r == DW_DLV_ERROR) {
		tet_printf("dwarf_attrlist failed: %s\n", dwarf_errmsg(de));
		result = TET_FAIL;
		return;
	} else if (r == DW_DLV_NO_ENTRY)
		return;

	for (i = 0; i < attrcount; i++) {
		at = attrlist[i];
		if (dwarf_whatattr(at, &attr, &de) != DW_DLV_OK) {
			tet_printf("dwarf_whatattr failed: %s\n",
			    dwarf_errmsg(de));
			result = TET_FAIL;
			continue;
		}
		TS_CHECK_UINT(attr);
		switch (attr) {
		case DW_AT_location:
		case DW_AT_string_length:
		case DW_AT_return_addr:
		case DW_AT_data_member_location:
		case DW_AT_frame_base:
		case DW_AT_segment:
		case DW_AT_static_link:
		case DW_AT_use_location:
		case DW_AT_vtable_elem_location:
			break;
		default:
			continue;
		}

		atname = NULL;
		if (dwarf_get_AT_name(attr, &atname) != DW_DLV_OK) {
			tet_printf("dwarf_get_AT_name failed\n");
			result = TET_FAIL;
		}
		tet_printf("process attribute %s\n", atname);

		r_loclist_n = dwarf_loclist_n(at, &llbuf, &listlen, &de);
		TS_CHECK_INT(r_loclist_n);
		if (r_loclist_n == DW_DLV_OK){
#ifndef	TCGEN
			/*
			 * XXX SGI libdwarf do not return the End-List-Indicator
			 * to the application (when loclist is in .debug_loc .i.e,
			 * listen > 1), while our libdwarf does. Workaround this
			 * by decreasing listlen by 1 when TCGEN is not defined,
			 * so this test case can work.
			 */
			if (listlen > 1)
				listlen--;
#endif
			TS_CHECK_INT(listlen);
			for (j = 0; j < listlen; j++) {
				tet_printf("process loclist[%d]\n", j);
				TS_CHECK_UINT(llbuf[j]->ld_lopc);
				TS_CHECK_UINT(llbuf[j]->ld_hipc);
				TS_CHECK_UINT(llbuf[j]->ld_cents);
				for (k = 0; k < llbuf[j]->ld_cents; k++) {
					tet_printf("process ld_s[%d]\n", k);
					loc = &llbuf[j]->ld_s[k];
					TS_CHECK_UINT(loc->lr_atom);
					TS_CHECK_UINT(loc->lr_number);
					TS_CHECK_UINT(loc->lr_number2);
#ifndef TCGEN
					/*
					 * XXX SGI libdwarf defined that
					 * lr_offset is lr_atom's offset + 1.
					 */
					loc->lr_offset++;
#endif
					TS_CHECK_UINT(loc->lr_offset);
#ifndef TCGEN
					loc->lr_offset--;
#endif
				}
			}
		}

		r_loclist = dwarf_loclist(at, &llbuf0, &listlen, &de);
		TS_CHECK_INT(r_loclist);
		if (r_loclist == DW_DLV_OK) {
			if (listlen != 1) {
				tet_printf("listlen(%d) returned by"
				    " dwarf_loclist must be 1", listlen);
				result = TET_FAIL;
			}
			tet_printf("process the only loclist\n");
			TS_CHECK_UINT(llbuf0->ld_lopc);
			TS_CHECK_UINT(llbuf0->ld_hipc);
			TS_CHECK_UINT(llbuf0->ld_cents);
			for (k = 0; k < llbuf0->ld_cents; k++) {
				tet_printf("process ld_s[%d]\n", k);
				loc = &llbuf0->ld_s[k];
				TS_CHECK_UINT(loc->lr_atom);
				TS_CHECK_UINT(loc->lr_number);
				TS_CHECK_UINT(loc->lr_number2);
#ifndef TCGEN
					/*
					 * XXX SGI libdwarf defined that
					 * lr_offset is lr_atom's offset + 1.
					 */
					loc->lr_offset++;
#endif
				TS_CHECK_UINT(loc->lr_offset);
#ifndef TCGEN
					loc->lr_offset--;
#endif
			}
		}
	}
}

static void
tp_dwarf_loclist(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	int fd;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	TS_DWARF_DIE_TRAVERSE(dbg, _dwarf_loclist);

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}

static void
tp_dwarf_loclist_sanity(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	Dwarf_Locdesc **llbuf, *llbuf0;
	Dwarf_Signed listlen;
	int fd;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	if (dwarf_loclist_n(NULL, &llbuf, &listlen, &de) != DW_DLV_ERROR) {
		tet_infoline("dwarf_loclist_n didn't return DW_DLV_ERROR"
		    " when called with NULL arguments");
		result = TET_FAIL;
		goto done;
	}

	if (dwarf_loclist(NULL, &llbuf0, &listlen, &de) != DW_DLV_ERROR) {
		tet_infoline("dwarf_loclist didn't return DW_DLV_ERROR"
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
