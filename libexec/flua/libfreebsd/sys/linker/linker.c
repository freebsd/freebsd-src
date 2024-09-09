/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024, Baptiste Daroussin <bapt@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include <sys/param.h>
#include <sys/linker.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

int luaopen_freebsd_sys_linker(lua_State *L);

static int
lua_kldload(lua_State *L)
{
	const char *name;
	int ret;

	name = luaL_checkstring(L, 1);
	ret = kldload(name);
	if (ret == -1) {
		lua_pushnil(L);
		lua_pushstring(L, strerror(errno));
		lua_pushinteger(L, errno);
		return (3);
	}
	lua_pushinteger(L, ret);
	return (1);
}

static int
lua_kldunload(lua_State *L)
{
	const char *name;
	int ret, fileid;

	if (lua_isinteger(L, 1)) {
		fileid = lua_tointeger(L, 1);
	} else {
		name = luaL_checkstring(L, 1);
		fileid = kldfind(name);
	}
	if (fileid == -1) {
		lua_pushnil(L);
		lua_pushstring(L, strerror(errno));
		lua_pushinteger(L, errno);
		return (3);
	}
	ret = kldunload(fileid);
	lua_pushinteger(L, ret);
	if (ret == -1) {
		lua_pushnil(L);
		lua_pushstring(L, strerror(errno));
		lua_pushinteger(L, errno);
		return (3);
	}
	lua_pushinteger(L, 0);
	return (1);
}

#define REG_SIMPLE(n)	{ #n, lua_ ## n }
static const struct luaL_Reg freebsd_sys_linker[] = {
	REG_SIMPLE(kldload),
	REG_SIMPLE(kldunload),
	{ NULL, NULL },
};
#undef REG_SIMPLE

int
luaopen_freebsd_sys_linker(lua_State *L)
{
	luaL_newlib(L, freebsd_sys_linker);

	return (1);
}
