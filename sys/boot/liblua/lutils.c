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

#include <src/lua.h>
#include <lstd.h>
#include <lutils.h>

#include <bootstrap.h>
#include <interp.h>

int
lua_perform(lua_State *L)
{
	int	argc;
	char	**argv;
	int	res = -1;
	int	n = lua_gettop(L);

	if (n >= 1)
	{
		parse(&argc, &argv, lua_tostring(L, 1));
		res = perform(argc, argv);
	}
	lua_pushnumber(L, res);

	return 1;
}

int
lua_getchar(lua_State *L)
{
	lua_pushnumber(L, getchar());
	return 1;
}

int
lua_ischar(lua_State *L)
{
	lua_pushboolean(L, ischar());
	return 1;
}

int
lua_gets(lua_State *L)
{
	char	buf[129];
	ngets(buf, 128);
	lua_pushstring(L, buf);
	return 1;
}

int
lua_time(lua_State *L)
{
	lua_pushnumber(L, time(NULL));
	return 1;
}

int
lua_delay(lua_State *L)
{
	int	n = lua_gettop(L);

	if (n == 1)
	{
		delay((int)lua_tonumber(L, 1));
	}
	return 0;
}

int
lua_getenv(lua_State *L)
{
	char	*ev;
	int	n = lua_gettop(L);

	if (n == 1)
	{
		ev = getenv(lua_tostring(L, 1));
		if (ev != NULL)
			lua_pushstring(L, ev);
		else
			lua_pushnil(L);
	} else
		lua_pushnil(L);
	return 1;
}

void *
lua_realloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
	(void)ud; (void)osize;  /* not used */
	if (nsize == 0)
	{
		free(ptr);
		return NULL;
	}
	else
		return realloc(ptr, nsize);
}

typedef struct data_chunk
{
	void * data;
	size_t size;
} data_chunk;

static const char *
read_chunk(lua_State *L, void *chunk, size_t *sz)
{
	data_chunk * ds = (data_chunk *)chunk;
	if (ds->size == 0) return NULL;
	*sz = ds->size;
	ds->size = 0;
	return (const char*)ds->data;
}


int
ldo_string(lua_State *L, const char *str, size_t size)
{
	int		res;
	data_chunk	ds;

	ds.data = (void*)str;
	ds.size = size;
	res = lua_load(L, read_chunk, &ds, "do_string", 0);
	res = lua_pcall(L, 0, LUA_MULTRET, 0);
	return res;
}

int
ldo_file(lua_State *L, const char *filename)
{
	struct stat		st;
	int			fd, r;
	char			*buf;
	const char		*errstr;

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		printf("Failed to open file %s\n", filename);
		return 1;
	}

	r = fstat(fd, &st);

	if (r != 0) {
		printf("Failed to retrieve file stat!\n");
		close(fd);
		return 1;
	}

	buf = malloc(st.st_size);
	if (buf == NULL) {
		printf("Failed to alloc buf!\n");
		close(fd);
		return 1;
	}

	r = read(fd, buf, st.st_size);
	if (r != st.st_size) {
		printf("Failed to read file (%d/%d)!\n", r, (unsigned int)st.st_size);
		free(buf);
		close(fd);
		return 1;
	}

	if (ldo_string(L, buf, st.st_size) != 0) {
		errstr = lua_tostring(L, -1);
		errstr = errstr == NULL ? "unknown" : errstr;
		printf("Failed to run %s file with error: %s.\n", filename, errstr);
		lua_pop(L, 1);
	}

	free(buf);
	close(fd);

	return 0;
}

int
lua_include(lua_State *L)
{
	const char	*str;

	if (lua_gettop(L) != 1)
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	str = lua_tostring(L, 1);
	lua_pushboolean(L, (ldo_file(L, str) == 0));
	return 1;
}

int
lua_openfile(lua_State *L)
{
	const char	*str;

	if (lua_gettop(L) != 1)
	{
		lua_pushnil(L);
		return 1;
	}
	str = lua_tostring(L, 1);

	FILE * f = fopen(str, "r");
	if (f != NULL)
	{
		FILE ** ptr = (FILE**)lua_newuserdata(L, sizeof(FILE**));
		*ptr = f;
	} else
		lua_pushnil(L);
	return 1;
}

int
lua_closefile(lua_State *L)
{
	FILE ** f;
	if (lua_gettop(L) != 1)
	{
		lua_pushboolean(L, 0);
		return 1;
	}

	f = (FILE**)lua_touserdata(L, 1);
	if (f != NULL && *f != NULL)
	{
		lua_pushboolean(L, fclose(*f) == 0 ? 1 : 0);
		*f = NULL;
	} else
		lua_pushboolean(L, 0);

	return 1;
}

int
lua_readfile(lua_State *L)
{
	FILE	**f;
	size_t	size, r;
	char * buf;

	if (lua_gettop(L) < 1 || lua_gettop(L) > 2)
	{
		lua_pushnil(L);
		lua_pushnumber(L, 0);
		return 2;
	}

	f = (FILE**)lua_touserdata(L, 1);

	if (f == NULL || *f == NULL)
	{
		lua_pushnil(L);
		lua_pushnumber(L, 0);
		return 2;
	}

	if (lua_gettop(L) == 2)
	{
		size = (size_t)lua_tonumber(L, 2);
	} else
		size = (*f)->size;


	buf = (char*)malloc(size);
	r = fread(buf, 1, size, *f);
	lua_pushlstring(L, buf, r);
	free(buf);
	lua_pushnumber(L, r);

	return 2;
}

void
lregister(lua_State *L, const char *tableName, const char *funcName, int (*funcPointer)(lua_State *))
{
	lua_getglobal(L, tableName);
	if (!lua_istable(L, -1))
	{
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
			{lua_perform, "loader", "perform"},
			{lua_delay, "loader", "delay"},
			{lua_time, "loader", "time"},
			{lua_include, "loader", "include"},
			{lua_getenv, "loader", "getenv"},
			{lua_getchar, "io", "getchar"},
			{lua_ischar, "io", "ischar"},
			{lua_gets, "io", "gets"},
			{lua_openfile, "io", "open"},
			{lua_closefile, "io", "close"},
			{lua_readfile, "io", "read"},
			{NULL, NULL, NULL},
			};

void
register_utils(lua_State *L)
{
	utils_func	*f = reg_funcs;

	while (f->func != NULL && f->name != NULL)
	{
		if (f->table != NULL)
		{
			lregister(L, f->table, f->name, f->func);
		}
		else
		{
			lua_register(L, f->name, f->func);
		}
		++f;
	}
}
