/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2023 Baptiste Daroussin <bapt@FreeBSD.org>
 * Copyright (C) 2025 Kyle Evans <kevans@FreeBSD.org>
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
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <lua.h>
#include "lauxlib.h"
#include "lfbsd.h"

#define	FBSD_PROCESSHANDLE	"fbsd_process_t*"

struct fbsd_process {
	int	pid;
	int	stdin_fileno;
	int	stdout_fileno;
};

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

static void
close_pipes(int pipes[2])
{

	if (pipes[0] != -1)
		close(pipes[0]);
	if (pipes[1] != -1)
		close(pipes[1]);
}

static int
lua_exec(lua_State *L)
{
	struct fbsd_process *proc;
	int r;
	posix_spawn_file_actions_t action;
	int stdin_pipe[2] = {-1, -1};
	int stdout_pipe[2] = {-1, -1};
	pid_t pid;
	const char **argv;
	int n = lua_gettop(L);
	bool capture_stdout;
	luaL_argcheck(L, n > 0 && n <= 2, n >= 2 ? 2 : n,
	    "fbsd.exec takes exactly one or two arguments");

	capture_stdout = lua_toboolean(L, 2);
	if (pipe(stdin_pipe) < 0) {
		lua_pushnil(L);
		lua_pushstring(L, strerror(errno));
		lua_pushinteger(L, errno);
		return (3);
	}
	if (capture_stdout && pipe(stdout_pipe) < 0) {
		close_pipes(stdin_pipe);
		lua_pushnil(L);
		lua_pushstring(L, strerror(errno));
		lua_pushinteger(L, errno);
		return (3);
	}

	proc = lua_newuserdata(L, sizeof(*proc));
	proc->stdin_fileno = stdin_pipe[1];
	proc->stdout_fileno = stdout_pipe[1];
	posix_spawn_file_actions_init(&action);
	posix_spawn_file_actions_adddup2(&action, stdin_pipe[0], STDIN_FILENO);
	posix_spawn_file_actions_addclose(&action, stdin_pipe[1]);
	if (stdin_pipe[0] != STDIN_FILENO)
		posix_spawn_file_actions_addclose(&action, stdin_pipe[0]);

	/*
	 * Setup stdout to be captured if requested.  Otherwise, we just let it
	 * go to our own stdout.
	 */
	if (stdout_pipe[0] != -1) {
		posix_spawn_file_actions_adddup2(&action, stdout_pipe[0],
		    STDOUT_FILENO);
		posix_spawn_file_actions_addclose(&action, stdout_pipe[1]);
		if (stdout_pipe[0] != STDOUT_FILENO) {
			posix_spawn_file_actions_addclose(&action,
			    stdout_pipe[0]);
		}
	}

	argv = luaL_checkarraystrings(L, 1);
	if (0 != (r = posix_spawnp(&pid, argv[0], &action, NULL,
		(char*const*)argv, environ))) {
		close_pipes(stdin_pipe);
		close_pipes(stdout_pipe);
		posix_spawn_file_actions_destroy(&action);
		lua_pop(L, 2);	/* Pop off the process handle and args. */

		lua_pushnil(L);
		lua_pushstring(L, strerror(r));
		lua_pushinteger(L, r);
		return (3);
	}

	lua_pop(L, 1);

	close(stdin_pipe[0]);
	if (stdout_pipe[0] != -1)
		close(stdout_pipe[0]);
	posix_spawn_file_actions_destroy(&action);

	proc->pid = pid;
	luaL_setmetatable(L, FBSD_PROCESSHANDLE);

	return (1);
}

static int
lua_process_close(lua_State *L)
{
	struct fbsd_process *proc;
	int pstat, r;

	proc = luaL_checkudata(L, 1, FBSD_PROCESSHANDLE);
	while (waitpid(proc->pid, &pstat, 0) == -1) {
		if ((r = errno) != EINTR) {
			lua_pushnil(L);
			lua_pushstring(L, strerror(r));
			lua_pushinteger(L, r);
			return (3);
		}
	}

	if (!WIFEXITED(pstat) || WEXITSTATUS(pstat) != 0) {
		lua_pushnil(L);
		lua_pushstring(L, "Abnormal termination");
		return (2);
	}

	if (proc->stdin_fileno >= 0) {
		close(proc->stdin_fileno);
		proc->stdin_fileno = -1;
	}

	if (proc->stdout_fileno >= 0) {
		close(proc->stdout_fileno);
		proc->stdout_fileno = -1;
	}

	lua_pushboolean(L, 1);
	return (1);
}

static int
lua_process_makestdio(lua_State *L, int fd, const char *mode)
{
	luaL_Stream *p;
	FILE *fp;
	int r;

	if (fd == -1) {
		lua_pushnil(L);
		lua_pushstring(L, "Stream not captured");
		return (2);
	}

	fp = fdopen(fd, mode);
	if (fp == NULL) {
		r = errno;

		lua_pushnil(L);
		lua_pushstring(L, strerror(r));
		lua_pushinteger(L, r);
		return (3);
	}

	p = lua_newuserdata(L, sizeof(*p));
	p->closef = &lua_process_close;
	p->f = fp;
	luaL_setmetatable(L, LUA_FILEHANDLE);
	return (1);
}

static int
lua_process_stdin(lua_State *L)
{
	struct fbsd_process *proc;

	proc = luaL_checkudata(L, 1, FBSD_PROCESSHANDLE);
	return (lua_process_makestdio(L, proc->stdin_fileno, "w"));
}

static int
lua_process_stdout(lua_State *L)
{
	struct fbsd_process *proc;

	proc = luaL_checkudata(L, 1, FBSD_PROCESSHANDLE);
	return (lua_process_makestdio(L, proc->stdout_fileno, "r"));
}

#define PROCESS_SIMPLE(n)	{ #n, lua_process_ ## n }
static const struct luaL_Reg fbsd_process[] = {
	PROCESS_SIMPLE(close),
	PROCESS_SIMPLE(stdin),
	PROCESS_SIMPLE(stdout),
	{ NULL, NULL },
};

static const struct luaL_Reg fbsd_process_meta[] = {
	{ "__index", NULL },
	{ "__gc", lua_process_close },
	{ "__close", lua_process_close },
	{ NULL, NULL },
};

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

	luaL_newmetatable(L, FBSD_PROCESSHANDLE);
	luaL_setfuncs(L, fbsd_process_meta, 0);

	luaL_newlibtable(L, fbsd_process);
	luaL_setfuncs(L, fbsd_process, 0);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);

	return (1);
}
