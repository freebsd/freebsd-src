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
#include <sys/stat.h>

#include <errno.h>
#include <grp.h>
#include <pwd.h>
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
lua_chown(lua_State *L)
{
	int n;
	const char *path;
	uid_t owner = (uid_t) -1;
	gid_t group = (gid_t) -1;

	n = lua_gettop(L);
	luaL_argcheck(L, n > 1, n,
	   "chown takes at least two arguments");
	path = luaL_checkstring(L, 1);
	if (lua_isinteger(L, 2))
		owner = (uid_t) lua_tointeger(L, 2);
	else if (lua_isstring(L, 2)) {
		struct passwd *p = getpwnam(lua_tostring(L, 2));
		if (p != NULL)
			owner = p->pw_uid;
		else
			return (luaL_argerror(L, 2,
			    lua_pushfstring(L, "unknown user %s",
			    lua_tostring(L, 2))));
	} else if (!lua_isnoneornil(L, 2)) {
		const char *type = luaL_typename(L, 2);
		return (luaL_argerror(L, 2,
		    lua_pushfstring(L, "integer or string expected, got %s",
		    type)));
	}

	if (lua_isinteger(L, 3))
		group = (gid_t) lua_tointeger(L, 3);
	else if (lua_isstring(L, 3)) {
		struct group *g = getgrnam(lua_tostring(L, 3));
		if (g != NULL)
			group = g->gr_gid;
		else
			return (luaL_argerror(L, 3,
			    lua_pushfstring(L, "unknown group %s",
			    lua_tostring(L, 3))));
	} else if (!lua_isnoneornil(L, 3)) {
		const char *type = luaL_typename(L, 3);
		return (luaL_argerror(L, 3,
		    lua_pushfstring(L, "integer or string expected, got %s",
		    type)));
	}

	if (chown(path, owner, group) == -1) {
		lua_pushnil(L);
		lua_pushstring(L, strerror(errno));
		lua_pushinteger(L, errno);
		return (3);
	}
	lua_pushinteger(L, 0);
	return (1);
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
	REG_SIMPLE(chown),
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
