/*-
 * Copyright (c) 2014 Pedro Souza <pedrosouza@freebsd.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "lua.h"
#include "lauxlib.h"
#include "lstd.h"
#include "lutils.h"
#include "bootstrap.h"

static int
lua_perform(lua_State *L)
{
	int	argc;
	char	**argv;
	int	res = 1;

	if (parse(&argc, &argv, luaL_checkstring(L, 1)) == 0) {
		res = interp_builtin_cmd(argc, argv);
		free(argv);
	}
	lua_pushinteger(L, res);

	return 1;
}

static int
lua_getchar(lua_State *L)
{

	lua_pushinteger(L, getchar());
	return 1;
}

static int
lua_ischar(lua_State *L)
{

	lua_pushboolean(L, ischar());
	return 1;
}

static int
lua_gets(lua_State *L)
{
	char	buf[129];

	ngets(buf, 128);
	lua_pushstring(L, buf);
	return 1;
}

static int
lua_time(lua_State *L)
{

	lua_pushinteger(L, time(NULL));
	return 1;
}

static int
lua_delay(lua_State *L)
{

	delay((int)luaL_checknumber(L, 1));
	return 0;
}

static int
lua_getenv(lua_State *L)
{
	lua_pushstring(L, getenv(luaL_checkstring(L, 1)));

	return 1;
}

static int
lua_setenv(lua_State *L)
{
	const char *key, *val;

	key = luaL_checkstring(L, 1);
	val = luaL_checkstring(L, 2);
	lua_pushinteger(L, setenv(key, val, 1));

	return 1;
}

static int
lua_unsetenv(lua_State *L)
{
	const char	*ev;

	ev = luaL_checkstring(L, 1);
	lua_pushinteger(L, unsetenv(ev));

	return 1;
}

static int
lua_printc(lua_State *L)
{
	int status;
	ssize_t l;
	const char *s = luaL_checklstring(L, 1, &l);

	status = (printf("%s", s) == l);

	return status;
}

static int
lua_openfile(lua_State *L)
{
	const char	*str;

	if (lua_gettop(L) != 1) {
		lua_pushnil(L);
		return 1;
	}
	str = lua_tostring(L, 1);

	FILE * f = fopen(str, "r");
	if (f != NULL) {
		FILE ** ptr = (FILE**)lua_newuserdata(L, sizeof(FILE**));
		*ptr = f;
	} else
		lua_pushnil(L);
	return 1;
}

static int
lua_closefile(lua_State *L)
{
	FILE ** f;
	if (lua_gettop(L) != 1) {
		lua_pushboolean(L, 0);
		return 1;
	}

	f = (FILE**)lua_touserdata(L, 1);
	if (f != NULL && *f != NULL) {
		lua_pushboolean(L, fclose(*f) == 0 ? 1 : 0);
		*f = NULL;
	} else
		lua_pushboolean(L, 0);

	return 1;
}

static int
lua_readfile(lua_State *L)
{
	FILE	**f;
	size_t	size, r;
	char * buf;

	if (lua_gettop(L) < 1 || lua_gettop(L) > 2) {
		lua_pushnil(L);
		lua_pushinteger(L, 0);
		return 2;
	}

	f = (FILE**)lua_touserdata(L, 1);

	if (f == NULL || *f == NULL) {
		lua_pushnil(L);
		lua_pushinteger(L, 0);
		return 2;
	}

	if (lua_gettop(L) == 2)
		size = (size_t)lua_tonumber(L, 2);
	else
		size = (*f)->size;


	buf = (char*)malloc(size);
	r = fread(buf, 1, size, *f);
	lua_pushlstring(L, buf, r);
	free(buf);
	lua_pushinteger(L, r);

	return 2;
}

void
lregister(lua_State *L, const char *tableName, const char *funcName, int (*funcPointer)(lua_State *))
{
	lua_getglobal(L, tableName);
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_setglobal(L, tableName);
		lua_getglobal(L, tableName);
	}

	lua_pushcfunction(L, funcPointer);
	lua_setfield(L, -2, funcName);
	lua_pop(L, 1);
}


typedef struct utils_func
{
	int (*func)(lua_State *);
	const char *table;
	const char *name;
} utils_func;

static utils_func reg_funcs[] = {
			{lua_delay, "loader", "delay"},
			{lua_getenv, "loader", "getenv"},
			{lua_perform, "loader", "perform"},
			{lua_printc, "loader", "printc"},
			{lua_setenv, "loader", "setenv"},
			{lua_time, "loader", "time"},
			{lua_unsetenv, "loader", "unsetenv"},

			{lua_closefile, "io", "close"},
			{lua_getchar, "io", "getchar"},
			{lua_gets, "io", "gets"},
			{lua_ischar, "io", "ischar"},
			{lua_openfile, "io", "open"},
			{lua_readfile, "io", "read"},

			{NULL, NULL, NULL},
			};

void
register_utils(lua_State *L)
{
	utils_func	*f = reg_funcs;

	while (f->func != NULL && f->name != NULL) {
		if (f->table != NULL)
			lregister(L, f->table, f->name, f->func);
		else
			lua_register(L, f->name, f->func);
		++f;
	}
}
