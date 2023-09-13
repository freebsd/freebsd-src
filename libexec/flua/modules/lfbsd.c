/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2023 Baptiste Daroussin <bapt@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions~
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <lua.h>
#include "lauxlib.h"
#include "lfbsd.h"

extern char **environ;

static const char**
luaL_checkarraystrings(lua_State *L, int arg)
{
	const char **ret;
	lua_Integer n, i;
	int t;
	int abs_arg = lua_absindex(L, arg);
	luaL_checktype(L, abs_arg, LUA_TTABLE);
	n = lua_rawlen(L, abs_arg);
	ret = lua_newuserdata(L, (n+1)*sizeof(char*));
	for (i=0; i<n; i++) {
		t = lua_rawgeti(L, abs_arg, i+1);
		if (t == LUA_TNIL)
			break;
		luaL_argcheck(L, t == LUA_TSTRING, arg, "expected array of strings");
		ret[i] = lua_tostring(L, -1);
		lua_pop(L, 1);
	}
	ret[i] = NULL;
	return ret;
}

static int
lua_exec(lua_State *L)
{
	int r, pstat;
	posix_spawn_file_actions_t action;
	int stdin_pipe[2] = {-1, -1};
	pid_t pid;
	const char **argv;
	int n = lua_gettop(L);
	luaL_argcheck(L, n == 1, n > 1 ? 2 : n,
	    "fbsd.exec takes exactly one argument");

	if (pipe(stdin_pipe) < 0) {
		lua_pushnil(L);
		lua_pushstring(L, strerror(errno));
		lua_pushinteger(L, errno);
		return (3);
	}

	posix_spawn_file_actions_init(&action);
	posix_spawn_file_actions_adddup2(&action, stdin_pipe[0], STDIN_FILENO);
	posix_spawn_file_actions_addclose(&action, stdin_pipe[1]);

	argv = luaL_checkarraystrings(L, 1);
	if (0 != (r = posix_spawnp(&pid, argv[0], &action, NULL,
		(char*const*)argv, environ))) {
		lua_pushnil(L);
		lua_pushstring(L, strerror(r));
		lua_pushinteger(L, r);
		return (3);
	}
	while (waitpid(pid, &pstat, 0) == -1) {
		if (errno != EINTR) {
			lua_pushnil(L);
			lua_pushstring(L, strerror(r));
			lua_pushinteger(L, r);
			return (3);
		}
	}

	if (WEXITSTATUS(pstat) != 0) {
		lua_pushnil(L);
		lua_pushstring(L, "Abnormal termination");
		lua_pushinteger(L, r);
		return (3);
	}

	posix_spawn_file_actions_destroy(&action);

	if (stdin_pipe[0] != -1)
		close(stdin_pipe[0]);
	if (stdin_pipe[1] != -1)
		close(stdin_pipe[1]);
	lua_pushinteger(L, pid);
	return 1;
}

#define REG_SIMPLE(n)	{ #n, lua_ ## n }
static const struct luaL_Reg fbsd_lib[] = {
	REG_SIMPLE(exec),
	{ NULL, NULL },
};
#undef REG_SIMPLE

int
luaopen_fbsd(lua_State *L)
{
	luaL_newlib(L, fbsd_lib);
	return (1);
}
