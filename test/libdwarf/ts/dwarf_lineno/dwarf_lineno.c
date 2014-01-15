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
 * $Id: dwarf_lineno.c 2084 2011-10-27 04:48:12Z jkoshy $
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
 * Test case for dwarf line informatio API.
 */
static void tp_dwarf_lineno(void);
static void tp_dwarf_srcfiles(void);
static void tp_dwarf_lineno_sanity(void);
static struct dwarf_tp dwarf_tp_array[] = {
	{"tp_dwarf_lineno", tp_dwarf_lineno},
	{"tp_dwarf_srcfiles", tp_dwarf_srcfiles},
	{"tp_dwarf_lineno_sanity", tp_dwarf_lineno_sanity},
	{NULL, NULL},
};
static int result = TET_UNRESOLVED;
#include "driver.c"
#include "die_traverse.c"

static void
_dwarf_lineno(Dwarf_Die die)
{
	Dwarf_Line *linebuf, ln;
	Dwarf_Signed linecount, lineoff;
	Dwarf_Unsigned lineno, srcfileno;
	Dwarf_Addr lineaddr;
	Dwarf_Half tag;
	Dwarf_Error de;
	Dwarf_Bool linebeginstatement, lineendsequence, lineblock;
	char *linesrc;
	int r_srclines;
	int i;

	if (dwarf_tag(die, &tag, &de) != DW_DLV_OK) {
		tet_printf("dwarf_tag failed: %s\n", dwarf_errmsg(de));
		result = TET_FAIL;
		return;
	}

	r_srclines = dwarf_srclines(die, &linebuf, &linecount, &de);
	TS_CHECK_INT(r_srclines);

	if (r_srclines == DW_DLV_ERROR) {
		tet_printf("dwarf_srclines should not fail but still failed:",
		    " %s", dwarf_errmsg(de));
		return;
	}

	if (r_srclines != DW_DLV_OK)
		return;

	for (i = 0; i < linecount; i++) {

		ln = linebuf[i];

		if (dwarf_linebeginstatement(ln, &linebeginstatement,
		     &de) != DW_DLV_OK) {
			tet_printf("dwarf_linebeginstatement failed: %s",
			    dwarf_errmsg(de));
			result = TET_FAIL;
		}
		TS_CHECK_INT(linebeginstatement);

		if (dwarf_linebeginstatement(ln, &linebeginstatement,
		     &de) != DW_DLV_OK) {
			tet_printf("dwarf_linebeginstatement failed: %s",
			    dwarf_errmsg(de));
			result = TET_FAIL;
		}
		TS_CHECK_INT(linebeginstatement);

		if (dwarf_lineendsequence(ln, &lineendsequence,
		     &de) != DW_DLV_OK) {
			tet_printf("dwarf_lineendsequence failed: %s",
			    dwarf_errmsg(de));
			result = TET_FAIL;
		}
		TS_CHECK_INT(lineendsequence);

		if (dwarf_lineno(ln, &lineno, &de) != DW_DLV_OK) {
			tet_printf("dwarf_lineno failed: %s",
			    dwarf_errmsg(de));
			result = TET_FAIL;
		}
		TS_CHECK_UINT(lineno);

		if (dwarf_line_srcfileno(ln, &srcfileno, &de) != DW_DLV_OK) {
			tet_printf("dwarf_line_srcfileno failed: %s",
			    dwarf_errmsg(de));
			result = TET_FAIL;
		}
		TS_CHECK_UINT(srcfileno);

		if (dwarf_lineaddr(ln, &lineaddr, &de) != DW_DLV_OK) {
			tet_printf("dwarf_lineaddr failed: %s",
			    dwarf_errmsg(de));
			result = TET_FAIL;
		}
		TS_CHECK_UINT(lineaddr);

		if (dwarf_lineoff(ln, &lineoff, &de) != DW_DLV_OK) {
			tet_printf("dwarf_lineoff failed: %s",
			    dwarf_errmsg(de));
			result = TET_FAIL;
		}
		TS_CHECK_INT(lineoff);

		if (dwarf_linesrc(ln, &linesrc, &de) != DW_DLV_OK) {
			tet_printf("dwarf_linesrc failed: %s",
			    dwarf_errmsg(de));
			result = TET_FAIL;
		}
		TS_CHECK_STRING(linesrc);

		if (dwarf_lineblock(ln, &lineblock, &de) != DW_DLV_OK) {
			tet_printf("dwarf_lineblock failed: %s",
			    dwarf_errmsg(de));
			result = TET_FAIL;
		}
		TS_CHECK_INT(lineblock);
	}
}

static void
tp_dwarf_lineno(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	int fd;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	TS_DWARF_DIE_TRAVERSE(dbg, _dwarf_lineno);

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}

static void
_dwarf_srcfiles(Dwarf_Die die)
{
	Dwarf_Half tag;
	Dwarf_Error de;
	Dwarf_Signed srccount;
	char **srcfiles;
	int r_srcfiles, i;

	if (dwarf_tag(die, &tag, &de) != DW_DLV_OK) {
		tet_printf("dwarf_tag failed: %s\n", dwarf_errmsg(de));
		result = TET_FAIL;
		return;
	}

	r_srcfiles = dwarf_srcfiles(die, &srcfiles, &srccount, &de);
	TS_CHECK_INT(r_srcfiles);

	if (r_srcfiles == DW_DLV_ERROR) {
		tet_printf("dwarf_srcfiles should not fail but still failed:",
		    " %s", dwarf_errmsg(de));
		return;
	}

	if (r_srcfiles != DW_DLV_OK)
		return;

	if (dwarf_srcfiles(die, &srcfiles, &srccount, &de) != DW_DLV_OK) {
		tet_printf("dwarf_srcfiles failed: %s\n", dwarf_errmsg(de));
		result = TET_FAIL;
		return;
	}

	TS_CHECK_INT(srccount);
	for (i = 0; i < srccount; i++) {
		if (srcfiles[i] == NULL) {
			tet_printf("dwarf_srcfiles returned NULL pointer"
			    " srcfiles[%d]\n", i);
			result = TET_FAIL;
		} else
			TS_CHECK_STRING(srcfiles[i]);
	}
}

static void
tp_dwarf_srcfiles(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	int fd;
	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	TS_DWARF_DIE_TRAVERSE(dbg, _dwarf_srcfiles);

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}

static void
tp_dwarf_lineno_sanity(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	Dwarf_Line *linebuf;
	Dwarf_Signed linecount;
	Dwarf_Signed srccount;
	char **srcfiles;
	int fd;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	if (dwarf_srclines(NULL, &linebuf, &linecount, &de) !=
	    DW_DLV_ERROR) {
		tet_infoline("dwarf_srclines didn't return DW_DLV_ERROR"
		    " when called with NULL arguments");
		result = TET_FAIL;
	}

	if (dwarf_srcfiles(NULL, &srcfiles, &srccount, &de) !=
	    DW_DLV_ERROR) {
		tet_infoline("dwarf_srcfiles didn't return DW_DLV_ERROR"
		    " when called with NULL arguments");
		result = TET_FAIL;
	}

	if (result == TET_UNRESOLVED)
		result = TET_PASS;
done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}
