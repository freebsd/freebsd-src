/*-
 * Copyright (c) 2011 Wojciech A. Koszek <wkoszek@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>
#include "bootstrap.h"

#define lua_c

#include "lstd.h"

#include <lua.h>
#include <ldebug.h>
#include <lauxlib.h>
#include <lualib.h>
#include <lutils.h>

struct interp_lua_softc {
	lua_State	*luap;
};

static struct interp_lua_softc lua_softc;

#ifdef LUA_DEBUG
#define	LDBG(...)	do {			\
	printf("%s(%d): ", __func__, __LINE__);	\
	printf(__VA_ARGS__);			\
	printf("\n");				\
} while (0)
#else
#define	LDBG(...)
#endif


static void *
interp_lua_realloc(void *ud __unused, void *ptr, size_t osize __unused, size_t nsize)
{

	if (nsize == 0) {
		free(ptr);
		return NULL;
	}
	return realloc(ptr, nsize);
}

/*
 * The libraries commented out below either lack the proper
 * support from libsa, or they are unlikely to be useful
 * in the bootloader, so have been commented out.
 */
static const luaL_Reg loadedlibs[] = {
  {"_G", luaopen_base},
  {LUA_LOADLIBNAME, luaopen_package},
//  {LUA_COLIBNAME, luaopen_coroutine},
//  {LUA_TABLIBNAME, luaopen_table},
  {LUA_STRLIBNAME, luaopen_string},
//  {LUA_IOLIBNAME, luaopen_io},
//  {LUA_OSLIBNAME, luaopen_os},
//  {LUA_MATHLIBNAME, luaopen_math},
//  {LUA_UTF8LIBNAME, luaopen_utf8},
//  {LUA_DBLIBNAME, luaopen_debug},
  {NULL, NULL}
};

void
interp_init(void)
{
	lua_State *luap;
	struct interp_lua_softc	*softc = &lua_softc;
	const char *filename;
	const luaL_Reg *lib;

	setenv("script.lang", "lua", 1);
	LDBG("creating context");

	luap = lua_newstate(interp_lua_realloc, NULL);
	if (luap == NULL) {
		printf("problem initializing the Lua interpreter\n");
		abort();
	}
	softc->luap = luap;
	register_utils(luap);

	/* "require" functions from 'loadedlibs' and set results to global table */
	for (lib = loadedlibs; lib->func; lib++) {
		luaL_requiref(luap, lib->name, lib->func, 1);
		lua_pop(luap, 1);  /* remove lib */
	}

	filename = "/boot/lua/loader.lua";
	if (interp_include(filename) != 0) {
                const char *errstr = lua_tostring(luap, -1);
                errstr = errstr == NULL ? "unknown" : errstr;
                printf("Startup errorr in %s:\nLUA ERROR: %s.\n", filename, errstr);
                lua_pop(luap, 1);
	}
}

int
interp_run(const char *line)
{
	int	argc;
	char	**argv;
	lua_State *luap;
	struct interp_lua_softc	*softc = &lua_softc;
	int status;

	luap = softc->luap;
	LDBG("executing line...");
	if ((status = luaL_dostring(luap, line)) != 0) {
		/*
		 * If we could not parse the line as Lua syntax,
		 * try parsing it as a loader command.
		 */
                lua_pop(luap, 1);
		if (parse(&argc, &argv, line) == 0) {
			status = interp_builtin_cmd(argc, argv);
			if (status != CMD_OK)
				printf("Command failed\n");
			free(argv);
		} else {
			printf("Failed to parse \'%s\'\n", line);
			status = -1;
		}
	}

	return (status == 0 ? CMD_OK : CMD_ERROR);
}

int
interp_include(const char *filename)
{
	struct interp_lua_softc	*softc = &lua_softc;

	LDBG("loading file %s", filename);

	return (luaL_dofile(softc->luap, filename));
}
