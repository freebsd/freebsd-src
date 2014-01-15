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
 * $Id: dwarf_get_address_size.c 2084 2011-10-27 04:48:12Z jkoshy $
 */

#include <assert.h>
#include <dwarf.h>
#include <errno.h>
#include <fcntl.h>
#include <libdwarf.h>
#include <string.h>

#include "driver.h"
#include "tet_api.h"

static void tp_dwarf_get_address_size(void);
static struct dwarf_tp dwarf_tp_array[] = {
	{"tp_dwarf_get_address_size", tp_dwarf_get_address_size},
	{NULL, NULL},
};
#include "driver.c"

static void
tp_dwarf_get_address_size(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	Dwarf_Half addr_size;
	int fd, result;

	dbg = NULL;
	result = TET_UNRESOLVED;

	if (dwarf_get_address_size(NULL, &addr_size, &de) != DW_DLV_ERROR) {
		tet_infoline("dwarf_get_adderss_size NULL 'dbg' test failed");
		result = TET_FAIL;
		goto done;
	}

	TS_DWARF_INIT(dbg, fd, de);

	if (dwarf_get_address_size(dbg, NULL, &de) != DW_DLV_ERROR) {
		tet_infoline("dwarf_get_adderss_size NULL 'addr_size' test "
		    "failed");
		result = TET_FAIL;
		goto done;
	}

	if (dwarf_get_address_size(dbg, &addr_size, &de) != DW_DLV_OK) {
		tet_printf("dwarf_get_address_size failed: %s",
		    dwarf_errmsg(de));
		result = TET_FAIL;
		goto done;
	}

	TS_CHECK_UINT(addr_size);

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}
