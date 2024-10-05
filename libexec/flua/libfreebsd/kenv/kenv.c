/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024, Baptiste Daroussin <bapt@FreeBSD.org>
 */

#include <errno.h>
#include <kenv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

int luaopen_freebsd_kenv(lua_State *L);

static int
lua_kenv_get(lua_State *L)
{
	const char *env;
	int ret, n;
	char value[1024];

	n = lua_gettop(L);
	if (n == 0) {
		char *buf, *bp, *cp;
		int size;

		size = kenv(KENV_DUMP, NULL, NULL, 0);
		if (size < 0) {
			lua_pushnil(L);
			lua_pushstring(L, strerror(errno));
			lua_pushinteger(L, errno);
			return (3);
		}
		size += 1;
		buf = malloc(size);
		if (buf == NULL) {
			lua_pushnil(L);
			lua_pushstring(L, strerror(errno));
			lua_pushinteger(L, errno);
			return (3);
		}
		if (kenv(KENV_DUMP, NULL, buf, size) < 0) {
			free(buf);
			lua_pushnil(L);
			lua_pushstring(L, strerror(errno));
			lua_pushinteger(L, errno);
			return (3);
		}

		lua_newtable(L);
		for (bp = buf; *bp != '\0'; bp += strlen(bp) + 1) {
			cp = strchr(bp, '=');
			if (cp == NULL)
				continue;
			*cp++ = '\0';
			lua_pushstring(L, cp);
			lua_setfield(L, -2, bp);
			bp = cp;
		}
		free(buf);
		return (1);
	}
	env = luaL_checkstring(L, 1);
	ret = kenv(KENV_GET, env, value, sizeof(value));
	if (ret == -1) {
		if (errno == ENOENT) {
			lua_pushnil(L);
			return (1);
		}
		lua_pushnil(L);
		lua_pushstring(L, strerror(errno));
		lua_pushinteger(L, errno);
		return (3);
	}
	lua_pushstring(L, value);
	return (1);
}

#define REG_SIMPLE(n)	{ #n, lua_kenv_ ## n }
static const struct luaL_Reg freebsd_kenv[] = {
	REG_SIMPLE(get),
	{ NULL, NULL },
};
#undef REG_SIMPLE

int
luaopen_freebsd_kenv(lua_State *L)
{
	luaL_newlib(L, freebsd_kenv);

	return (1);
}
