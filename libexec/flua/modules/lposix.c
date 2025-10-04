/*-
 * Copyright (c) 2019, 2023 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#include <errno.h>
#include <fnmatch.h>
#include <grp.h>
#include <libgen.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <lua.h>
#include "lauxlib.h"
#include "lposix.h"

static void
enforce_max_args(lua_State *L, int max)
{
	int narg;

	narg = lua_gettop(L);
	luaL_argcheck(L, narg <= max, max + 1, "too many arguments");
}

/*
 * Minimal implementation of luaposix needed for internal FreeBSD bits.
 */
static int
lua__exit(lua_State *L)
{
	int code;

	enforce_max_args(L, 1);
	code = luaL_checkinteger(L, 1);

	_exit(code);
}

static int
lua_basename(lua_State *L)
{
	char *inpath, *outpath;

	enforce_max_args(L, 1);
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
	const char *path;
	mode_t mode;

	enforce_max_args(L, 2);
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
	const char *path;
	uid_t owner = (uid_t)-1;
	gid_t group = (gid_t)-1;
	int error;

	enforce_max_args(L, 3);

	path = luaL_checkstring(L, 1);
	if (lua_isinteger(L, 2))
		owner = (uid_t)lua_tointeger(L, 2);
	else if (lua_isstring(L, 2)) {
		char buf[4096];
		struct passwd passwd, *pwd;

		error = getpwnam_r(lua_tostring(L, 2), &passwd,
		    buf, sizeof(buf), &pwd);
		if (error == 0)
			owner = pwd->pw_uid;
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
		group = (gid_t)lua_tointeger(L, 3);
	else if (lua_isstring(L, 3)) {
		char buf[4096];
		struct group gr, *grp;

		error = getgrnam_r(lua_tostring(L, 3), &gr, buf, sizeof(buf),
		    &grp);
		if (error == 0)
			group = grp->gr_gid;
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
	int error, fd;

	enforce_max_args(L, 1);

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
lua_dup2(lua_State *L)
{
	int error, oldd, newd;

	enforce_max_args(L, 2);

	oldd = luaL_checkinteger(L, 1);
	if (oldd < 0) {
		error = EBADF;
		goto err;
	}

	newd = luaL_checkinteger(L, 2);
	if (newd < 0) {
		error = EBADF;
		goto err;
	}

	error = dup2(oldd, newd);
	if (error >= 0) {
		lua_pushinteger(L, error);
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
lua_execp(lua_State *L)
{
	int argc, error;
	const char *file;
	const char **argv;

	enforce_max_args(L, 2);

	file = luaL_checkstring(L, 1);
	luaL_checktype(L, 2, LUA_TTABLE);

	lua_len(L, 2);
	argc = lua_tointeger(L, -1);

	/*
	 * Use lua_newuserdatauv() to allocate a scratch buffer that is tracked
	 * and freed by lua's GC. This avoid any chance of a leak if a lua error
	 * is raised later in this function (e.g. by luaL_argerror()).
	 * The (argc + 2) size gives enough space in the buffer for argv[0] and
	 * the terminating NULL.
	 */
	argv = lua_newuserdatauv(L, (argc + 2) * sizeof(char *), 0);

	/*
	 * Sequential tables in lua start at index 1 by convention.
	 * If there happens to be a string at index 0, use that to
	 * override the default argv[0]. This matches the lposix API.
	 */
	lua_pushinteger(L, 0);
	lua_gettable(L, 2);
	argv[0] = lua_tostring(L, -1);
	if (argv[0] == NULL) {
		argv[0] = file;
	}

	for (int i = 1; i <= argc; i++) {
		lua_pushinteger(L, i);
		lua_gettable(L, 2);
		argv[i] = lua_tostring(L, -1);
		if (argv[i] == NULL) {
			luaL_argerror(L, 2,
			    "argv table must contain only strings");
		}
	}
	argv[argc + 1] = NULL;

	execvp(file, (char **)argv);
	error = errno;

	lua_pushnil(L);
	lua_pushstring(L, strerror(error));
	lua_pushinteger(L, error);
	return (3);
}

static int
lua_fnmatch(lua_State *L)
{
	const char *pattern, *string;
	int flags;

	enforce_max_args(L, 3);
	pattern = luaL_checkstring(L, 1);
	string = luaL_checkstring(L, 2);
	flags = luaL_optinteger(L, 3, 0);

	lua_pushinteger(L, fnmatch(pattern, string, flags));

	return (1);
}

static int
lua_uname(lua_State *L)
{
	struct utsname name;
	int error;

	enforce_max_args(L, 0);

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

	enforce_max_args(L, 1);

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

	enforce_max_args(L, 0);

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
	enforce_max_args(L, 0);

	lua_pushinteger(L, getpid());
	return (1);
}

static int
lua_pipe(lua_State *L)
{
	int error, fd[2];

	enforce_max_args(L, 0);

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
	int error, fd;

	enforce_max_args(L, 2);
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

	enforce_max_args(L, 1);
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

	enforce_max_args(L, 2);
	pid = luaL_optinteger(L, 1, -1);
	options = luaL_optinteger(L, 2, 0);

	status = 0;
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
	int error, fd;

	enforce_max_args(L, 4);

	fd = luaL_checkinteger(L, 1);
	if (fd < 0) {
		error = EBADF;
		goto err;
	}

	buf = luaL_checkstring(L, 2);

	bufsz = lua_rawlen(L, 2);
	sz = luaL_optinteger(L, 3, bufsz);

	offset = luaL_optinteger(L, 4, 0);


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

static const struct luaL_Reg fnmatchlib[] = {
	REG_SIMPLE(fnmatch),
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
	REG_SIMPLE(dup2),
	REG_SIMPLE(execp),
	REG_SIMPLE(fork),
	REG_SIMPLE(getpid),
	REG_SIMPLE(pipe),
	REG_SIMPLE(read),
	REG_SIMPLE(write),
	{ NULL, NULL },
};

#undef REG_SIMPLE
#undef REG_DEF

static int
luaopen_posix_libgen(lua_State *L)
{
	luaL_newlib(L, libgenlib);
	return (1);
}

static int
luaopen_posix_stdlib(lua_State *L)
{
	luaL_newlib(L, stdliblib);
	return (1);
}

static int
luaopen_posix_fnmatch(lua_State *L)
{
	luaL_newlib(L, fnmatchlib);

#define	setkv(f) do {			\
	lua_pushinteger(L, f);		\
	lua_setfield(L, -2, #f);	\
} while (0)
	setkv(FNM_PATHNAME);
	setkv(FNM_NOESCAPE);
	setkv(FNM_NOMATCH);
	setkv(FNM_PERIOD);
#undef setkv

	return 1;
}

static int
luaopen_posix_sys_stat(lua_State *L)
{
	luaL_newlib(L, sys_statlib);
	return (1);
}

static int
luaopen_posix_sys_utsname(lua_State *L)
{
	luaL_newlib(L, sys_utsnamelib);
	return 1;
}

static int
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

static int
luaopen_posix_unistd(lua_State *L)
{
	luaL_newlib(L, unistdlib);
	return (1);
}

int
luaopen_posix(lua_State *L)
{
	lua_newtable(L); /* posix */

	luaL_requiref(L, "posix.fnmatch", luaopen_posix_fnmatch, 0);
	lua_setfield(L, -2, "fnmatch");

	luaL_requiref(L, "posix.libgen", luaopen_posix_libgen, 0);
	lua_setfield(L, -2, "libgen");

	luaL_requiref(L, "posix.stdlib", luaopen_posix_stdlib, 0);
	lua_setfield(L, -2, "stdlib");

	lua_newtable(L); /* posix.sys */
	luaL_requiref(L, "posix.sys.stat", luaopen_posix_sys_stat, 0);
	lua_setfield(L, -2, "stat");
	luaL_requiref(L, "posix.sys.utsname", luaopen_posix_sys_utsname, 0);
	lua_setfield(L, -2, "utsname");
	luaL_requiref(L, "posix.sys.wait", luaopen_posix_sys_wait, 0);
	lua_setfield(L, -2, "wait");
	lua_setfield(L, -2, "sys");

	luaL_requiref(L, "posix.unistd", luaopen_posix_unistd, 0);
	lua_setfield(L, -2, "unistd");

	return (1);
}
