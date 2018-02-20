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

#include <sys/param.h>

#include "lua.h"
#include "lauxlib.h"
#include "lstd.h"
#include "lutils.h"
#include "bootstrap.h"

/*
 * Like loader.perform, except args are passed already parsed
 * on the stack.
 */
static int
lua_command(lua_State *L)
{
	int	i;
	int	res = 1;
	int 	argc = lua_gettop(L);
	char	**argv;

	argv = malloc(sizeof(char *) * (argc + 1));
	if (argv == NULL)
		return 0;
	for (i = 0; i < argc; i++)
		argv[i] = (char *)(intptr_t)luaL_checkstring(L, i + 1);
	argv[argc] = NULL;
	res = interp_builtin_cmd(argc, argv);
	free(argv);
	lua_pushinteger(L, res);

	return 1;
}

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

#define REG_SIMPLE(n)	{ #n, lua_ ## n }
static const struct luaL_Reg loaderlib[] = {
	REG_SIMPLE(delay),
	REG_SIMPLE(command),
	REG_SIMPLE(getenv),
	REG_SIMPLE(perform),
	REG_SIMPLE(printc),
	REG_SIMPLE(setenv),
	REG_SIMPLE(time),
	REG_SIMPLE(unsetenv),
	{ NULL, NULL },
};

static const struct luaL_Reg iolib[] = {
	{ "close", lua_closefile },
	REG_SIMPLE(getchar),
	REG_SIMPLE(gets),
	REG_SIMPLE(ischar),
	{ "open", lua_openfile },
	{ "read", lua_readfile },
	{ NULL, NULL },
};
#undef REG_SIMPLE

int
luaopen_loader(lua_State *L)
{
	luaL_newlib(L, loaderlib);
	/* Add loader.machine and loader.machine_arch properties */
	lua_pushstring(L, MACHINE);
	lua_setfield(L, -2, "machine");
	lua_pushstring(L, MACHINE_ARCH);
	lua_setfield(L, -2, "machine_arch");
	return 1;
}

int
luaopen_io(lua_State *L)
{
	luaL_newlib(L, iolib);
	return 1;
}
