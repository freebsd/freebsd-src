/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020, Ryan Moeller <freqlabs@FreeBSD.org>
 * Copyright (c) 2020, Kyle Evans <kevans@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/jail.h>
#include <errno.h>
#include <jail.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define	JAIL_METATABLE "jail iterator metatable"

/*
 * Taken from RhodiumToad's lspawn implementation, let static analyzers make
 * better decisions about the behavior after we raise an error.
 */
#if defined(LUA_VERSION_NUM) && defined(LUA_API)
LUA_API int   (lua_error) (lua_State *L) __dead2;
#endif
#if defined(LUA_ERRFILE) && defined(LUALIB_API)
LUALIB_API int (luaL_argerror) (lua_State *L, int arg, const char *extramsg) __dead2;
LUALIB_API int (luaL_typeerror) (lua_State *L, int arg, const char *tname) __dead2;
LUALIB_API int (luaL_error) (lua_State *L, const char *fmt, ...) __dead2;
#endif

int luaopen_jail(lua_State *);

typedef bool (*getparam_filter)(const char *, void *);

static void getparam_table(lua_State *L, int paramindex,
    struct jailparam *params, size_t paramoff, size_t *params_countp,
    getparam_filter keyfilt, void *udata);

struct l_jail_iter {
	struct jailparam	*params;
	size_t			params_count;
	int			jid;
};

static bool
l_jail_filter(const char *param_name, void *data __unused)
{

	/*
	 * Allowing lastjid will mess up our iteration over all jails on the
	 * system, as this is a special parameter that indicates where the search
	 * starts from.  We'll always add jid and name, so just silently remove
	 * these.
	 */
	return (strcmp(param_name, "lastjid") != 0 &&
	    strcmp(param_name, "jid") != 0 &&
	    strcmp(param_name, "name") != 0);
}

static int
l_jail_iter_next(lua_State *L)
{
	struct l_jail_iter *iter, **iterp;
	struct jailparam *jp;
	int serrno;

	iterp = (struct l_jail_iter **)luaL_checkudata(L, 1, JAIL_METATABLE);
	iter = *iterp;
	luaL_argcheck(L, iter != NULL, 1, "closed jail iterator");

	jp = iter->params;
	/* Populate lastjid; we must keep it in params[0] for our sake. */
	if (jailparam_import_raw(&jp[0], &iter->jid, sizeof(iter->jid))) {
		jailparam_free(jp, iter->params_count);
		free(jp);
		free(iter);
		*iterp = NULL;
		return (luaL_error(L, "jailparam_import_raw: %s", jail_errmsg));
	}

	/* The list of requested params was populated back in l_list(). */
	iter->jid = jailparam_get(jp, iter->params_count, 0);
	if (iter->jid == -1) {
		/*
		 * We probably got an ENOENT to signify the end of the jail
		 * listing, but just in case we didn't; stash it off and start
		 * cleaning up.  We'll handle non-ENOENT errors later.
		 */
		serrno = errno;
		jailparam_free(jp, iter->params_count);
		free(iter->params);
		free(iter);
		*iterp = NULL;
		if (serrno != ENOENT)
			return (luaL_error(L, "jailparam_get: %s",
			    strerror(serrno)));
		return (0);
	}

	/*
	 * Finally, we'll fill in the return table with whatever parameters the
	 * user requested, in addition to the ones we forced with exception to
	 * lastjid.
	 */
	lua_newtable(L);
	for (size_t i = 0; i < iter->params_count; ++i) {
		char *value;

		jp = &iter->params[i];
		if (strcmp(jp->jp_name, "lastjid") == 0)
			continue;
		value = jailparam_export(jp);
		lua_pushstring(L, value);
		lua_setfield(L, -2, jp->jp_name);
		free(value);
	}

	return (1);
}

static int
l_jail_iter_close(lua_State *L)
{
	struct l_jail_iter *iter, **iterp;

	/*
	 * Since we're using this as the __gc method as well, there's a good
	 * chance that it's already been cleaned up by iterating to the end of
	 * the list.
	 */
	iterp = (struct l_jail_iter **)lua_touserdata(L, 1);
	iter = *iterp;
	if (iter == NULL)
		return (0);

	jailparam_free(iter->params, iter->params_count);
	free(iter->params);
	free(iter);
	*iterp = NULL;
	return (0);
}

static int
l_list(lua_State *L)
{
	struct l_jail_iter *iter;
	int nargs;

	nargs = lua_gettop(L);
	if (nargs >= 1)
		luaL_checktype(L, 1, LUA_TTABLE);

	iter = malloc(sizeof(*iter));
	if (iter == NULL)
		return (luaL_error(L, "malloc: %s", strerror(errno)));

	/*
	 * lastjid, jid, name + length of the table.  This may be too much if
	 * we have duplicated one of those fixed parameters.
	 */
	iter->params_count = 3 + (nargs != 0 ? lua_rawlen(L, 1) : 0);
	iter->params = malloc(iter->params_count * sizeof(*iter->params));
	if (iter->params == NULL) {
		free(iter);
		return (luaL_error(L, "malloc params: %s", strerror(errno)));
	}

	/* The :next() method will populate lastjid before jail_getparam(). */
	if (jailparam_init(&iter->params[0], "lastjid") == -1) {
		free(iter->params);
		free(iter);
		return (luaL_error(L, "jailparam_init: %s", jail_errmsg));
	}
	/* These two will get populated by jail_getparam(). */
	if (jailparam_init(&iter->params[1], "jid") == -1) {
		jailparam_free(iter->params, 1);
		free(iter->params);
		free(iter);
		return (luaL_error(L, "jailparam_init: %s",
		    jail_errmsg));
	}
	if (jailparam_init(&iter->params[2], "name") == -1) {
		jailparam_free(iter->params, 2);
		free(iter->params);
		free(iter);
		return (luaL_error(L, "jailparam_init: %s",
		    jail_errmsg));
	}

	/*
	 * We only need to process additional arguments if we were given any.
	 * That is, we don't descend into getparam_table if we're passed nothing
	 * or an empty table.
	 */
	iter->jid = 0;
	if (iter->params_count != 3)
		getparam_table(L, 1, iter->params, 2, &iter->params_count,
		    l_jail_filter, NULL);

	/*
	 * Part of the iterator magic.  We give it an iterator function with a
	 * metatable defining next() and close() that can be used for manual
	 * iteration.  iter->jid is how we track which jail we last iterated, to
	 * be supplied as "lastjid".
	 */
	lua_pushcfunction(L, l_jail_iter_next);
	*(struct l_jail_iter **)lua_newuserdata(L,
	    sizeof(struct l_jail_iter **)) = iter;
	luaL_getmetatable(L, JAIL_METATABLE);
	lua_setmetatable(L, -2);
	return (2);
}

static void
register_jail_metatable(lua_State *L)
{
	luaL_newmetatable(L, JAIL_METATABLE);
	lua_newtable(L);
	lua_pushcfunction(L, l_jail_iter_next);
	lua_setfield(L, -2, "next");
	lua_pushcfunction(L, l_jail_iter_close);
	lua_setfield(L, -2, "close");

	lua_setfield(L, -2, "__index");

	lua_pushcfunction(L, l_jail_iter_close);
	lua_setfield(L, -2, "__gc");

	lua_pop(L, 1);
}

static int
l_getid(lua_State *L)
{
	const char *name;
	int jid;

	name = luaL_checkstring(L, 1);
	jid = jail_getid(name);
	if (jid == -1) {
		lua_pushnil(L);
		lua_pushstring(L, jail_errmsg);
		return (2);
	}
	lua_pushinteger(L, jid);
	return (1);
}

static int
l_getname(lua_State *L)
{
	char *name;
	int jid;

	jid = luaL_checkinteger(L, 1);
	name = jail_getname(jid);
	if (name == NULL) {
		lua_pushnil(L);
		lua_pushstring(L, jail_errmsg);
		return (2);
	}
	lua_pushstring(L, name);
	free(name);
	return (1);
}

static int
l_allparams(lua_State *L)
{
	struct jailparam *params;
	int params_count;

	params_count = jailparam_all(&params);
	if (params_count == -1) {
		lua_pushnil(L);
		lua_pushstring(L, jail_errmsg);
		return (2);
	}
	lua_newtable(L);
	for (int i = 0; i < params_count; ++i) {
		lua_pushstring(L, params[i].jp_name);
		lua_rawseti(L, -2, i + 1);
	}
	jailparam_free(params, params_count);
	free(params);
	return (1);
}

static void
getparam_table(lua_State *L, int paramindex, struct jailparam *params,
    size_t params_off, size_t *params_countp, getparam_filter keyfilt,
    void *udata)
{
	size_t params_count;
	int skipped;

	params_count = *params_countp;
	skipped = 0;
	for (size_t i = 1 + params_off; i < params_count; ++i) {
		const char *param_name;

		lua_rawgeti(L, -1, i - params_off);
		param_name = lua_tostring(L, -1);
		if (param_name == NULL) {
			jailparam_free(params, i - skipped);
			free(params);
			luaL_argerror(L, paramindex,
			    "param names must be strings");
		}
		lua_pop(L, 1);
		if (keyfilt != NULL && !keyfilt(param_name, udata)) {
			++skipped;
			continue;
		}
		if (jailparam_init(&params[i - skipped], param_name) == -1) {
			jailparam_free(params, i - skipped);
			free(params);
			luaL_error(L, "jailparam_init: %s", jail_errmsg);
		}
	}
	*params_countp -= skipped;
}

struct getparams_filter_args {
	int	filter_type;
};

static bool
l_getparams_filter(const char *param_name, void *udata)
{
	struct getparams_filter_args *gpa;

	gpa = udata;

	/* Skip name or jid, whichever was given. */
	if (gpa->filter_type == LUA_TSTRING) {
		if (strcmp(param_name, "name") == 0)
			return (false);
	} else /* type == LUA_TNUMBER */ {
		if (strcmp(param_name, "jid") == 0)
			return (false);
	}

	return (true);
}

static int
l_getparams(lua_State *L)
{
	const char *name;
	struct jailparam *params;
	size_t params_count;
	struct getparams_filter_args gpa;
	int flags, jid, type;

	type = lua_type(L, 1);
	luaL_argcheck(L, type == LUA_TSTRING || type == LUA_TNUMBER, 1,
	    "expected a jail name (string) or id (integer)");
	luaL_checktype(L, 2, LUA_TTABLE);
	params_count = 1 + lua_rawlen(L, 2);
	flags = luaL_optinteger(L, 3, 0);

	params = malloc(params_count * sizeof(struct jailparam));
	if (params == NULL)
		return (luaL_error(L, "malloc: %s", strerror(errno)));

	/*
	 * Set the jail name or id param as determined by the first arg.
	 */

	if (type == LUA_TSTRING) {
		if (jailparam_init(&params[0], "name") == -1) {
			free(params);
			return (luaL_error(L, "jailparam_init: %s",
			    jail_errmsg));
		}
		name = lua_tostring(L, 1);
		if (jailparam_import(&params[0], name) == -1) {
			jailparam_free(params, 1);
			free(params);
			return (luaL_error(L, "jailparam_import: %s",
			    jail_errmsg));
		}
	} else /* type == LUA_TNUMBER */ {
		if (jailparam_init(&params[0], "jid") == -1) {
			free(params);
			return (luaL_error(L, "jailparam_init: %s",
			    jail_errmsg));
		}
		jid = lua_tointeger(L, 1);
		if (jailparam_import_raw(&params[0], &jid, sizeof(jid)) == -1) {
			jailparam_free(params, 1);
			free(params);
			return (luaL_error(L, "jailparam_import_raw: %s",
			    jail_errmsg));
		}
	}

	/*
	 * Set the remaining param names being requested.
	 */
	gpa.filter_type = type;
	getparam_table(L, 2, params, 0, &params_count, l_getparams_filter, &gpa);

	/*
	 * Get the values and convert to a table.
	 */

	jid = jailparam_get(params, params_count, flags);
	if (jid == -1) {
		jailparam_free(params, params_count);
		free(params);
		lua_pushnil(L);
		lua_pushstring(L, jail_errmsg);
		return (2);
	}
	lua_pushinteger(L, jid);

	lua_newtable(L);
	for (size_t i = 0; i < params_count; ++i) {
		char *value;

		if (params[i].jp_flags & JP_KEYVALUE &&
		    params[i].jp_valuelen == 0) {
			/* Communicate back a missing key. */
			lua_pushnil(L);
		} else {
			value = jailparam_export(&params[i]);
			lua_pushstring(L, value);
			free(value);
		}

		lua_setfield(L, -2, params[i].jp_name);
	}

	jailparam_free(params, params_count);
	free(params);

	return (2);
}

static int
l_setparams(lua_State *L)
{
	const char *name;
	struct jailparam *params;
	size_t params_count;
	int flags, jid, type;

	type = lua_type(L, 1);
	luaL_argcheck(L, type == LUA_TSTRING || type == LUA_TNUMBER, 1,
	    "expected a jail name (string) or id (integer)");
	luaL_checktype(L, 2, LUA_TTABLE);

	lua_pushnil(L);
	for (params_count = 1; lua_next(L, 2) != 0; ++params_count)
		lua_pop(L, 1);

	flags = luaL_optinteger(L, 3, 0);

	params = malloc(params_count * sizeof(struct jailparam));
	if (params == NULL)
		return (luaL_error(L, "malloc: %s", strerror(errno)));

	/*
	 * Set the jail name or id param as determined by the first arg.
	 */

	if (type == LUA_TSTRING) {
		if (jailparam_init(&params[0], "name") == -1) {
			free(params);
			return (luaL_error(L, "jailparam_init: %s",
			    jail_errmsg));
		}
		name = lua_tostring(L, 1);
		if (jailparam_import(&params[0], name) == -1) {
			jailparam_free(params, 1);
			free(params);
			return (luaL_error(L, "jailparam_import: %s",
			    jail_errmsg));
		}
	} else /* type == LUA_TNUMBER */ {
		if (jailparam_init(&params[0], "jid") == -1) {
			free(params);
			return (luaL_error(L, "jailparam_init: %s",
			    jail_errmsg));
		}
		jid = lua_tointeger(L, 1);
		if (jailparam_import_raw(&params[0], &jid, sizeof(jid)) == -1) {
			jailparam_free(params, 1);
			free(params);
			return (luaL_error(L, "jailparam_import_raw: %s",
			    jail_errmsg));
		}
	}

	/*
	 * Set the rest of the provided params.
	 */

	lua_pushnil(L);
	for (size_t i = 1; i < params_count && lua_next(L, 2) != 0; ++i) {
		const char *value;

		name = lua_tostring(L, -2);
		if (name == NULL) {
			jailparam_free(params, i);
			free(params);
			return (luaL_argerror(L, 2,
			    "param names must be strings"));
		}
		if (jailparam_init(&params[i], name) == -1) {
			jailparam_free(params, i);
			free(params);
			return (luaL_error(L, "jailparam_init: %s",
			    jail_errmsg));
		}

		value = lua_tostring(L, -1);
		/* Allow passing NULL for key removal. */
		if (value == NULL && !(params[i].jp_flags & JP_KEYVALUE)) {
			jailparam_free(params, i + 1);
			free(params);
			return (luaL_argerror(L, 2,
			    "param values must be strings"));
		}
		if (jailparam_import(&params[i], value) == -1) {
			jailparam_free(params, i + 1);
			free(params);
			return (luaL_error(L, "jailparam_import: %s",
			    jail_errmsg));
		}

		lua_pop(L, 1);
	}

	/*
	 * Attempt to set the params.
	 */

	jid = jailparam_set(params, params_count, flags);
	if (jid == -1) {
		jailparam_free(params, params_count);
		free(params);
		lua_pushnil(L);
		lua_pushstring(L, jail_errmsg);
		return (2);
	}
	lua_pushinteger(L, jid);

	jailparam_free(params, params_count);
	free(params);
	return (1);
}

static int
l_attach(lua_State *L)
{
	int jid, type;

	type = lua_type(L, 1);
	luaL_argcheck(L, type == LUA_TSTRING || type == LUA_TNUMBER, 1,
	    "expected a jail name (string) or id (integer)");

	if (lua_isstring(L, 1)) {
		/* Resolve it to a jid. */
		jid = jail_getid(lua_tostring(L, 1));
		if (jid == -1) {
			lua_pushnil(L);
			lua_pushstring(L, jail_errmsg);
			return (2);
		}
	} else {
		jid = lua_tointeger(L, 1);
	}

	if (jail_attach(jid) == -1) {
		lua_pushnil(L);
		lua_pushstring(L, strerror(errno));
		return (2);
	}

	lua_pushboolean(L, 1);
	return (1);
}

static int
l_remove(lua_State *L)
{
	int jid, type;

	type = lua_type(L, 1);
	luaL_argcheck(L, type == LUA_TSTRING || type == LUA_TNUMBER, 1,
	    "expected a jail name (string) or id (integer)");

	if (lua_isstring(L, 1)) {
		/* Resolve it to a jid. */
		jid = jail_getid(lua_tostring(L, 1));
		if (jid == -1) {
			lua_pushnil(L);
			lua_pushstring(L, jail_errmsg);
			return (2);
		}
	} else {
		jid = lua_tointeger(L, 1);
	}

	if (jail_remove(jid) == -1) {
		lua_pushnil(L);
		lua_pushstring(L, strerror(errno));
		return (2);
	}

	lua_pushboolean(L, 1);
	return (1);
}

static const struct luaL_Reg l_jail[] = {
	/** Get id of a jail by name.
	 * @param name	jail name (string)
	 * @return	jail id (integer)
	 *		or nil, error (string) on error
	 */
	{"getid", l_getid},
	/** Get name of a jail by id.
	 * @param jid	jail id (integer)
	 * @return	jail name (string)
	 *		or nil, error (string) on error
	 */
	{"getname", l_getname},
	/** Get a list of all known jail parameters.
	 * @return	list of jail parameter names (table of strings)
	 *		or nil, error (string) on error
	 */
	{"allparams", l_allparams},
	/** Get the listed params for a given jail.
	 * @param jail	jail name (string) or id (integer)
	 * @param params	list of parameter names (table of strings)
	 * @param flags	optional flags (integer)
	 * @return	jid (integer), params (table of [string] = string)
	 *		or nil, error (string) on error
	 */
	{"getparams", l_getparams},
	/** Set params for a given jail.
	 * @param jail	jail name (string) or id (integer)
	 * @param params	params and values (table of [string] = string)
	 * @param flags	optional flags (integer)
	 * @return	jid (integer)
	 *		or nil, error (string) on error
	 */
	{"setparams", l_setparams},
	/** Get a list of jail parameters for running jails on the system.
	 * @param params	optional list of parameter names (table of
	 *			strings)
	 * @return	iterator (function), jail_obj (object) with next and
	 *		close methods
	 */
	{"list", l_list},
	/** Attach to a running jail.
	 * @param jail	jail name (string) or id (integer)
	 * @return	true (boolean)
	 *		or nil, error (string) on error
	 */
	{"attach", l_attach},
	/** Remove a running jail.
	 * @param jail	jail name (string) or id (integer)
	 * @return	true (boolean)
	 *		or nil, error (string) on error
	 */
	{"remove", l_remove},
	{NULL, NULL}
};

int
luaopen_jail(lua_State *L)
{
	lua_newtable(L);

	luaL_setfuncs(L, l_jail, 0);

	lua_pushinteger(L, JAIL_CREATE);
	lua_setfield(L, -2, "CREATE");
	lua_pushinteger(L, JAIL_UPDATE);
	lua_setfield(L, -2, "UPDATE");
	lua_pushinteger(L, JAIL_ATTACH);
	lua_setfield(L, -2, "ATTACH");
	lua_pushinteger(L, JAIL_DYING);
	lua_setfield(L, -2, "DYING");

	register_jail_metatable(L);

	return (1);
}
