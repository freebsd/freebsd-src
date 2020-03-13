/*-
 * Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/stat.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <lua.h>
#include "lauxlib.h"
#include "lposix.h"

/*
 * Minimal implementation of luaposix needed for internal FreeBSD bits.
 */

static int
lua_chmod(lua_State *L)
{
	int n;
	const char *path;
	mode_t mode;

	n = lua_gettop(L);
	luaL_argcheck(L, n == 2, n > 2 ? 3 : n,
	    "chmod takes exactly two arguments");
	path = luaL_checkstring(L, 1);
	mode = (mode_t)luaL_checkinteger(L, 2);
	if (chmod(path, mode) == -1) {
		lua_pushnil(L);
		lua_pushstring(L, strerror(errno));
		lua_pushinteger(L, errno);
		return 3;
	}
	lua_pushinteger(L, 0);
	return 1;
}

static int
lua_getpid(lua_State *L)
{
	int n;

	n = lua_gettop(L);
	luaL_argcheck(L, n == 0, 1, "too many arguments");
	lua_pushinteger(L, getpid());
	return 1;
}

#define REG_SIMPLE(n)	{ #n, lua_ ## n }
static const struct luaL_Reg sys_statlib[] = {
	REG_SIMPLE(chmod),
	{ NULL, NULL },
};

static const struct luaL_Reg unistdlib[] = {
	REG_SIMPLE(getpid),
	{ NULL, NULL },
};
#undef REG_SIMPLE

int
luaopen_posix_sys_stat(lua_State *L)
{
	luaL_newlib(L, sys_statlib);
	return 1;
}

int
luaopen_posix_unistd(lua_State *L)
{
	luaL_newlib(L, unistdlib);
	return 1;
}
