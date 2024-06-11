/*-
 * Copyright (c) 2020 Kyle Evans <kevans@FreeBSD.org>
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
 */

#include <lua.h>
#include "lauxlib.h"

/* Open the pager.  No arguments, no return value. */
static int
lpager_open(lua_State *L)
{

	pager_open();
	return (0);
}

/*
 * Output to the pager.  All arguments are interpreted as strings and passed to
 * pager_output().  No return value.
 */
static int
lpager_output(lua_State *L)
{
	const char *outstr;
	int i;

	for (i = 1; i <= lua_gettop(L); i++) {
		outstr = luaL_tolstring(L,  i, NULL);
		pager_output(outstr);
		lua_pop(L, -1);
	}

	return (0);
}

/* Output to the pager from a file.  Takes a filename, no return value. */
static int
lpager_file(lua_State *L)
{

	return (pager_file(luaL_checkstring(L, 1)));
}

static int
lpager_close(lua_State *L)
{

	pager_close();
	return (0);
}

static const struct luaL_Reg pagerlib[] = {
	{ "open", lpager_open },
	{ "output", lpager_output },
	{ "file", lpager_file },
	{ "close", lpager_close },
	{ NULL, NULL },
};

int
luaopen_pager(lua_State *L)
{
	luaL_newlib(L, pagerlib);
	return 1;
}
