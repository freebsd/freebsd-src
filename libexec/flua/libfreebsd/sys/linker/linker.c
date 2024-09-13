/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024, Baptiste Daroussin <bapt@FreeBSD.org>
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
