/*-
 * Copyright (c) 2019, 2023 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#include <errno.h>
#include <grp.h>
#include <libgen.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <lua.h>
#include "lauxlib.h"
#include "lposix.h"

/*
 * Minimal implementation of luaposix needed for internal FreeBSD bits.
 */
static int
lua__exit(lua_State *L)
{
	int code, narg;

	narg = lua_gettop(L);
	luaL_argcheck(L, narg == 1, 1, "_exit takes exactly one argument");

	code = luaL_checkinteger(L, 1);
	_exit(code);
}

static int
lua_basename(lua_State *L)
{
	char *inpath, *outpath;
	int narg;

	narg = lua_gettop(L);
	luaL_argcheck(L, narg > 0, 1, "at least one argument required");
	inpath = strdup(luaL_checkstring(L, 1));
	if (inpath == NULL) {
		lua_pushnil(L);
		lua_pushstring(L, strerror(ENOMEM));
		lua_pushinteger(L, ENOMEM);
		return (3);
	}

	outpath = basename(inpath);
	lua_pushstring(L, outpath);
	free(inpath);
	return (1);
}

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
		return (3);
	}
	lua_pushinteger(L, 0);
	return (1);
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
lua_pclose(lua_State *L)
{
	int error, fd, n;

	n = lua_gettop(L);
	luaL_argcheck(L, n == 1, 1,
	    "close takes exactly one argument (fd)");

	fd = luaL_checkinteger(L, 1);
	if (fd < 0) {
		error = EBADF;
		goto err;
	}

	if (close(fd) == 0) {
		lua_pushinteger(L, 0);
		return (1);
	}

	error = errno;
err:
	lua_pushnil(L);
	lua_pushstring(L, strerror(error));
	lua_pushinteger(L, error);
	return (3);

}

static int
lua_uname(lua_State *L)
{
	struct utsname name;
	int error, n;

	n = lua_gettop(L);
	luaL_argcheck(L, n == 0, 1, "too many arguments");

	error = uname(&name);
	if (error != 0) {
		error = errno;
		lua_pushnil(L);
		lua_pushstring(L, strerror(error));
		lua_pushinteger(L, error);
		return (3);
	}

	lua_newtable(L);
#define	setkv(f) do {			\
	lua_pushstring(L, name.f);	\
	lua_setfield(L, -2, #f);	\
} while (0)
	setkv(sysname);
	setkv(nodename);
	setkv(release);
	setkv(version);
	setkv(machine);
#undef setkv

	return (1);
}

static int
lua_dirname(lua_State *L)
{
	char *inpath, *outpath;
	int narg;

	narg = lua_gettop(L);
	luaL_argcheck(L, narg > 0, 1,
	    "dirname takes at least one argument (path)");
	inpath = strdup(luaL_checkstring(L, 1));
	if (inpath == NULL) {
		lua_pushnil(L);
		lua_pushstring(L, strerror(ENOMEM));
		lua_pushinteger(L, ENOMEM);
		return (3);
	}

	outpath = dirname(inpath);
	lua_pushstring(L, outpath);
	free(inpath);
	return (1);
}

static int
lua_fork(lua_State *L)
{
	pid_t pid;
	int narg;

	narg = lua_gettop(L);
	luaL_argcheck(L, narg == 0, 1, "too many arguments");

	pid = fork();
	if (pid < 0) {
		lua_pushnil(L);
		lua_pushstring(L, strerror(errno));
		lua_pushinteger(L, errno);
		return (3);
	}

	lua_pushinteger(L, pid);
	return (1);
}

static int
lua_getpid(lua_State *L)
{
	int narg;

	narg = lua_gettop(L);
	luaL_argcheck(L, narg == 0, 1, "too many arguments");
	lua_pushinteger(L, getpid());
	return (1);
}

static int
lua_pipe(lua_State *L)
{
	int error, fd[2], narg;

	narg = lua_gettop(L);
	luaL_argcheck(L, narg == 0, 1, "too many arguments");

	error = pipe(fd);
	if (error != 0) {
		lua_pushnil(L);
		lua_pushstring(L, strerror(errno));
		lua_pushinteger(L, errno);
		return (1);
	}

	lua_pushinteger(L, fd[0]);
	lua_pushinteger(L, fd[1]);
	return (2);
}

static int
lua_read(lua_State *L)
{
	char *buf;
	ssize_t ret;
	size_t sz;
	int error, fd, narg;

	narg = lua_gettop(L);
	luaL_argcheck(L, narg == 2, 1,
	    "read takes exactly two arguments (fd, size)");

	fd = luaL_checkinteger(L, 1);
	sz = luaL_checkinteger(L, 2);

	if (fd < 0) {
		error = EBADF;
		goto err;
	}

	buf = malloc(sz);
	if (buf == NULL)
		goto err;

	/*
	 * For 0-byte reads, we'll still push the empty string and let the
	 * caller deal with EOF to match lposix semantics.
	 */
	ret = read(fd, buf, sz);
	if (ret >= 0)
		lua_pushlstring(L, buf, ret);
	else if (ret < 0)
		error = errno; /* Save to avoid clobber by free() */

	free(buf);
	if (error != 0)
		goto err;

	/* Just the string pushed. */
	return (1);
err:
	lua_pushnil(L);
	lua_pushstring(L, strerror(error));
	lua_pushinteger(L, error);
	return (3);
}

static int
lua_realpath(lua_State *L)
{
	const char *inpath;
	char *outpath;
	int narg;

	narg = lua_gettop(L);
	luaL_argcheck(L, narg > 0, 1, "at least one argument required");
	inpath = luaL_checkstring(L, 1);

	outpath = realpath(inpath, NULL);
	if (outpath == NULL) {
		lua_pushnil(L);
		lua_pushstring(L, strerror(errno));
		lua_pushinteger(L, errno);
		return (3);
	}

	lua_pushstring(L, outpath);
	free(outpath);
	return (1);
}

static int
lua_wait(lua_State *L)
{
	pid_t pid;
	int options, status;
	int narg;

	narg = lua_gettop(L);

	pid = -1;
	status = options = 0;
	if (narg >= 1 && !lua_isnil(L, 1))
		pid = luaL_checkinteger(L, 1);
	if (narg >= 2 && !lua_isnil(L, 2))
		options = luaL_checkinteger(L, 2);

	pid = waitpid(pid, &status, options);
	if (pid < 0) {
		lua_pushnil(L);
		lua_pushstring(L, strerror(errno));
		lua_pushinteger(L, errno);
		return (3);
	}

	lua_pushinteger(L, pid);
	if (pid == 0) {
		lua_pushliteral(L, "running");
		return (2);
	}

	if (WIFCONTINUED(status)) {
		lua_pushliteral(L, "continued");
		return (2);
	} else if(WIFSTOPPED(status)) {
		lua_pushliteral(L, "stopped");
		lua_pushinteger(L, WSTOPSIG(status));
		return (3);
	} else if (WIFEXITED(status)) {
		lua_pushliteral(L, "exited");
		lua_pushinteger(L, WEXITSTATUS(status));
		return (3);
	} else if (WIFSIGNALED(status)) {
		lua_pushliteral(L, "killed");
		lua_pushinteger(L, WTERMSIG(status));
		return (3);
	}

	return (1);
}

static int
lua_write(lua_State *L)
{
	const char *buf;
	size_t bufsz, sz;
	ssize_t ret;
	off_t offset;
	int error, fd, narg;

	narg = lua_gettop(L);
	luaL_argcheck(L, narg >= 2, 1,
	    "write takes at least two arguments (fd, buf, sz, off)");
	luaL_argcheck(L, narg <= 4, 5,
	    "write takes no more than four arguments (fd, buf, sz, off)");

	fd = luaL_checkinteger(L, 1);
	if (fd < 0) {
		error = EBADF;
		goto err;
	}

	buf = luaL_checkstring(L, 2);

	bufsz = sz = lua_rawlen(L, 2);
	if (narg >= 3 && !lua_isnil(L, 3))
		sz = luaL_checkinteger(L, 3);

	offset = 0;
	if (narg >= 4 && !lua_isnil(L, 4))
		offset = luaL_checkinteger(L, 4);

	if ((size_t)offset > bufsz || offset + sz > bufsz) {
		lua_pushnil(L);
		lua_pushfstring(L,
		    "write: invalid access offset %zu, size %zu in a buffer size %zu",
		    offset, sz, bufsz);
		lua_pushinteger(L, EINVAL);
		return (3);
	}

	ret = write(fd, buf + offset, sz);
	if (ret < 0) {
		error = errno;
		goto err;
	}

	lua_pushinteger(L, ret);
	return (1);
err:
	lua_pushnil(L);
	lua_pushstring(L, strerror(error));
	lua_pushinteger(L, error);
	return (3);
}

#define	REG_DEF(n, func)	{ #n, func }
#define REG_SIMPLE(n)		REG_DEF(n, lua_ ## n)
static const struct luaL_Reg libgenlib[] = {
	REG_SIMPLE(basename),
	REG_SIMPLE(dirname),
	{ NULL, NULL },
};

static const struct luaL_Reg stdliblib[] = {
	REG_SIMPLE(realpath),
	{ NULL, NULL },
};

static const struct luaL_Reg sys_statlib[] = {
	REG_SIMPLE(chmod),
	{ NULL, NULL },
};

static const struct luaL_Reg sys_utsnamelib[] = {
	REG_SIMPLE(uname),
	{ NULL, NULL },
};

static const struct luaL_Reg sys_waitlib[] = {
	REG_SIMPLE(wait),
	{NULL, NULL},
};

static const struct luaL_Reg unistdlib[] = {
	REG_SIMPLE(_exit),
	REG_SIMPLE(chown),
	REG_DEF(close, lua_pclose),
	REG_SIMPLE(fork),
	REG_SIMPLE(getpid),
	REG_SIMPLE(pipe),
	REG_SIMPLE(read),
	REG_SIMPLE(write),
	{ NULL, NULL },
};

#undef REG_SIMPLE
#undef REG_DEF

int
luaopen_posix_libgen(lua_State *L)
{
	luaL_newlib(L, libgenlib);
	return (1);
}

int
luaopen_posix_stdlib(lua_State *L)
{
	luaL_newlib(L, stdliblib);
	return (1);
}

int
luaopen_posix_sys_stat(lua_State *L)
{
	luaL_newlib(L, sys_statlib);
	return (1);
}

int
luaopen_posix_sys_wait(lua_State *L)
{
	luaL_newlib(L, sys_waitlib);

#define	lua_pushflag(L, flag) do {	\
	lua_pushinteger(L, flag);	\
	lua_setfield(L, -2, #flag);	\
} while(0)

	/* Only these two exported by lposix */
	lua_pushflag(L, WNOHANG);
	lua_pushflag(L, WUNTRACED);

	lua_pushflag(L, WCONTINUED);
	lua_pushflag(L, WSTOPPED);
#ifdef WTRAPPED
	lua_pushflag(L, WTRAPPED);
#endif
	lua_pushflag(L, WEXITED);
	lua_pushflag(L, WNOWAIT);
#undef lua_pushflag

	return (1);
}

int
luaopen_posix_sys_utsname(lua_State *L)
{
	luaL_newlib(L, sys_utsnamelib);
	return 1;
}

int
luaopen_posix_unistd(lua_State *L)
{
	luaL_newlib(L, unistdlib);
	return (1);
}
