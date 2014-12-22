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
 * $Id: dwarf_attrlist.c 3083 2014-09-02 22:08:01Z kaiwang27 $
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
 * Test case for dwarf_attrlist and dwarf_whatattr etc.
 */
static void tp_dwarf_attrlist(void);
static void tp_dwarf_attrlist_sanity(void);
static struct dwarf_tp dwarf_tp_array[] = {
	{"tp_dwarf_attrlist", tp_dwarf_attrlist},
	{"tp_dwarf_attrlist_sanity", tp_dwarf_attrlist_sanity},
	{NULL, NULL},
};
static int result = TET_UNRESOLVED;
#include "driver.c"
#include "die_traverse2.c"

static void
_dwarf_attrlist(Dwarf_Die die)
{
	Dwarf_Attribute *attrlist;
	Dwarf_Signed attrcount;
	Dwarf_Half attr;
	Dwarf_Error de;
	int i, r;

	r = dwarf_attrlist(die, &attrlist, &attrcount, &de);
	TS_CHECK_INT(r);
	if (r == DW_DLV_ERROR) {
		tet_printf("dwarf_attrlist failed: %s\n", dwarf_errmsg(de));
		result = TET_FAIL;
		return;
	} else if (r == DW_DLV_NO_ENTRY)
		return;

	TS_CHECK_INT(attrcount);
	for (i = 0; i < attrcount; i++) {
		if (dwarf_whatattr(attrlist[i], &attr, &de) != DW_DLV_OK) {
			tet_printf("dwarf_whatattr failed: %s\n",
			    dwarf_errmsg(de));
			result = TET_FAIL;
		}
		TS_CHECK_UINT(attr);
	}
}

static void
tp_dwarf_attrlist(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	int fd;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	TS_DWARF_DIE_TRAVERSE2(dbg, 1, _dwarf_attrlist);
	TS_DWARF_DIE_TRAVERSE2(dbg, 0, _dwarf_attrlist);

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}

static void
tp_dwarf_attrlist_sanity(void)
{
	Dwarf_Debug dbg;
	Dwarf_Attribute *attrlist;
	Dwarf_Signed attrcount;
	Dwarf_Error de;
	int fd;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	if (dwarf_attrlist(NULL, &attrlist, &attrcount, &de) != DW_DLV_ERROR) {
		tet_infoline("dwarf_attrlist didn't return DW_DLV_ERROR"
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
