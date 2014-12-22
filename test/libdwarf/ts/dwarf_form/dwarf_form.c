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
 * $Id: dwarf_form.c 3084 2014-09-02 22:08:13Z kaiwang27 $
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
 * Test case for DWARF attribute query functions.
 */
static void tp_dwarf_form(void);
static void tp_dwarf_form_sanity(void);
static struct dwarf_tp dwarf_tp_array[] = {
	{"tp_dwarf_form", tp_dwarf_form},
	{"tp_dwarf_form_sanity", tp_dwarf_form_sanity},
	{NULL, NULL},
};
static int result = TET_UNRESOLVED;
#include "driver.c"
#include "die_traverse2.c"

static void
_dwarf_form(Dwarf_Die die)
{
	Dwarf_Attribute *attrlist, at;
	Dwarf_Signed attrcount;
	Dwarf_Half form, direct_form;
	Dwarf_Off offset;
	Dwarf_Addr addr;
	Dwarf_Bool flag, hasform;
	Dwarf_Unsigned uvalue;
	Dwarf_Signed svalue;
	Dwarf_Block *block;
	Dwarf_Sig8 sig8;
	Dwarf_Ptr ptr;
	Dwarf_Error de;
	char *str;
	int i, r;
	int r_formref, r_global_formref, r_formaddr, r_formflag;
	int r_formudata, r_formsdata, r_formblock, r_formstring;
	int r_formsig8, r_formexprloc;

	r = dwarf_attrlist(die, &attrlist, &attrcount, &de);
	if (r == DW_DLV_ERROR) {
		tet_printf("dwarf_attrlist failed: %s\n", dwarf_errmsg(de));
		result = TET_FAIL;
		return;
	} else if (r == DW_DLV_NO_ENTRY)
		return;

	for (i = 0; i < attrcount; i++) {
		at = attrlist[i];
		if (dwarf_whatform(at, &form, &de) != DW_DLV_OK) {
			tet_printf("dwarf_whatform failed: %s\n",
			    dwarf_errmsg(de));
			result = TET_FAIL;
			continue;
		}
		TS_CHECK_UINT(form);
		if (dwarf_hasform(at, form, &hasform, &de) != DW_DLV_OK) {
			tet_printf("dwarf_hasform failed: %s\n",
			    dwarf_errmsg(de));
			result = TET_FAIL;
		}
		if (!hasform) {
			tet_infoline("dwarf_hasform contradicts with"
			    " dwarf_whatform");
			result = TET_FAIL;
		}
		if (dwarf_whatform_direct(at, &direct_form, &de) != DW_DLV_OK) {
			tet_printf("dwarf_whatform_direct failed: %s\n",
			    dwarf_errmsg(de));
			result = TET_FAIL;
		}
		TS_CHECK_UINT(direct_form);

		r_formref = dwarf_formref(at, &offset, &de);
		TS_CHECK_INT(r_formref);
		if (r_formref == DW_DLV_OK)
			TS_CHECK_INT(offset);

		r_global_formref = dwarf_global_formref(at, &offset, &de);
		TS_CHECK_INT(r_global_formref);
		if (r_global_formref == DW_DLV_OK)
			TS_CHECK_INT(offset);

		r_formaddr = dwarf_formaddr(at, &addr, &de);
		TS_CHECK_INT(r_formaddr);
		if (r_formaddr == DW_DLV_OK)
			TS_CHECK_UINT(addr);

		r_formflag = dwarf_formflag(at, &flag, &de);
		TS_CHECK_INT(r_formflag);
		if (r_formflag == DW_DLV_OK)
			TS_CHECK_INT(flag);

		r_formudata = dwarf_formudata(at, &uvalue, &de);
		TS_CHECK_INT(r_formudata);
		if (r_formudata == DW_DLV_OK)
			TS_CHECK_UINT(uvalue);

		r_formsdata = dwarf_formsdata(at, &svalue, &de);
		TS_CHECK_INT(r_formsdata);
		if (r_formsdata == DW_DLV_OK)
			TS_CHECK_INT(svalue);

		r_formblock = dwarf_formblock(at, &block, &de);
		TS_CHECK_INT(r_formblock);
		if (r_formblock == DW_DLV_OK)
			TS_CHECK_BLOCK(block->bl_data, block->bl_len);

		r_formstring = dwarf_formstring(at, &str, &de);
		TS_CHECK_INT(r_formstring);
		if (r_formstring == DW_DLV_OK)
			TS_CHECK_STRING(str);

		r_formsig8 = dwarf_formsig8(at, &sig8, &de);
		TS_CHECK_INT(r_formsig8);
		if (r_formsig8 == DW_DLV_OK)
			TS_CHECK_BLOCK(sig8.signature, 8);

		r_formexprloc = dwarf_formexprloc(at, &uvalue, &ptr, &de);
		TS_CHECK_INT(r_formexprloc);
		if (r_formexprloc == DW_DLV_OK)
			TS_CHECK_BLOCK(ptr, uvalue);
	}
}

static void
tp_dwarf_form(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	int fd;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	TS_DWARF_DIE_TRAVERSE2(dbg, 1, _dwarf_form);
	TS_DWARF_DIE_TRAVERSE2(dbg, 0, _dwarf_form);

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}

static void
tp_dwarf_form_sanity(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	Dwarf_Half form, direct_form;
	Dwarf_Off offset;
	Dwarf_Addr addr;
	Dwarf_Bool flag, hasform;
	Dwarf_Unsigned uvalue;
	Dwarf_Signed svalue;
	Dwarf_Block *block;
	char *str;
	int fd;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	if (dwarf_whatform(NULL, &form, &de) != DW_DLV_ERROR) {
		tet_infoline("dwarf_whatform didn't return DW_DLV_ERROR"
		    " when called with NULL arguments");
		result = TET_FAIL;
		goto done;
	}

	if (dwarf_whatform_direct(NULL, &direct_form, &de) != DW_DLV_ERROR) {
		tet_infoline("dwarf_whatform_direct didn't return DW_DLV_ERROR"
		    " when called with NULL arguments");
		result = TET_FAIL;
		goto done;
	}

	if (dwarf_hasform(NULL, DW_FORM_indirect, &hasform, &de) !=
	    DW_DLV_ERROR) {
		tet_infoline("dwarf_hasform didn't return DW_DLV_ERROR"
		    " when called with NULL arguments");
		result = TET_FAIL;
		goto done;
	}

	if (dwarf_formref(NULL, &offset, &de) != DW_DLV_ERROR) {
		tet_infoline("dwarf_formref didn't return DW_DLV_ERROR"
		    " when called with NULL arguments");
		result = TET_FAIL;
		goto done;
	}

	if (dwarf_global_formref(NULL, &offset, &de) != DW_DLV_ERROR) {
		tet_infoline("dwarf_global_formref didn't return DW_DLV_ERROR"
		    " when called with NULL arguments");
		result = TET_FAIL;
		goto done;
	}

	if (dwarf_formaddr(NULL, &addr, &de) != DW_DLV_ERROR) {
		tet_infoline("dwarf_formaddr didn't return DW_DLV_ERROR"
		    " when called with NULL arguments");
		result = TET_FAIL;
		goto done;
	}

	if (dwarf_formflag(NULL, &flag, &de) != DW_DLV_ERROR) {
		tet_infoline("dwarf_formflag didn't return DW_DLV_ERROR"
		    " when called with NULL arguments");
		result = TET_FAIL;
		goto done;
	}
	
	if (dwarf_formudata(NULL, &uvalue, &de) != DW_DLV_ERROR) {
		tet_infoline("dwarf_formudata didn't return DW_DLV_ERROR"
		    " when called with NULL arguments");
		result = TET_FAIL;
		goto done;
	}

	if (dwarf_formsdata(NULL, &svalue, &de) != DW_DLV_ERROR) {
		tet_infoline("dwarf_formsdata didn't return DW_DLV_ERROR"
		    " when called with NULL arguments");
		result = TET_FAIL;
		goto done;
	}

	if (dwarf_formblock(NULL, &block, &de) != DW_DLV_ERROR) {
		tet_infoline("dwarf_formblock didn't return DW_DLV_ERROR"
		    " when called with NULL arguments");
		result = TET_FAIL;
		goto done;
	}

	if (dwarf_formstring(NULL, &str, &de) != DW_DLV_ERROR) {
		tet_infoline("dwarf_formstring didn't return DW_DLV_ERROR"
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
