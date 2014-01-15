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
 * $Id: dwarf_init.c 2084 2011-10-27 04:48:12Z jkoshy $
 */

#include <assert.h>
#include <dwarf.h>
#include <errno.h>
#include <fcntl.h>
#include <libdwarf.h>
#include <string.h>

#include "driver.h"
#include "tet_api.h"

static void tp_dwarf_init(void);
static struct dwarf_tp dwarf_tp_array[] = {
	{"tp_dwarf_init", tp_dwarf_init},
	{NULL, NULL},
};
#include "driver.c"

static void
tp_dwarf_init(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	int fd, result;

	result = TET_UNRESOLVED;

	assert(_cur_file != NULL);
	dbg = NULL;
	if ((fd = open(_cur_file, O_RDONLY)) < 0) {
		tet_printf("open %s failed: %s", _cur_file, strerror(errno));
		result = TET_FAIL;
		goto done;
	}

	if (dwarf_init(fd, DW_DLC_READ, NULL, NULL, &dbg, &de) != DW_DLV_OK) {
		tet_printf("dwarf_init failed: %s", dwarf_errmsg(de));
		result = TET_FAIL;
		goto done;
	}

	if (dwarf_init(-1, DW_DLC_READ, NULL, NULL, &dbg, &de) != DW_DLV_ERROR) {
		tet_infoline("dwarf_init didn't return DW_DLV_ERROR when"
		    " called with fd(-1)");
		result = TET_FAIL;
		goto done;
	}

	if (result == TET_UNRESOLVED)
		result = TET_PASS;

done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}
