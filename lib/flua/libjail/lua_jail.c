/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020, Ryan Moeller <freqlabs@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/jail.h>
#include <errno.h>
#include <jail.h>
#include <stdlib.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

int luaopen_jail(lua_State *);

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

static int
l_getparams(lua_State *L)
{
	const char *name;
	struct jailparam *params;
	size_t params_count, skipped;
	int flags, jid, type;

	type = lua_type(L, 1);
	luaL_argcheck(L, type == LUA_TSTRING || type == LUA_TNUMBER, 1,
	    "expected a jail name (string) or id (integer)");
	luaL_checktype(L, 2, LUA_TTABLE);
	params_count = 1 + lua_rawlen(L, 2);
	luaL_argcheck(L, params_count > 1, 2, "expected #params > 0");
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

	skipped = 0;
	for (size_t i = 1; i < params_count; ++i) {
		const char *param_name;

		lua_rawgeti(L, -1, i);
		param_name = lua_tostring(L, -1);
		if (param_name == NULL) {
			jailparam_free(params, i - skipped);
			free(params);
			return (luaL_argerror(L, 2,
			    "param names must be strings"));
		}
		lua_pop(L, 1);
		/* Skip name or jid, whichever was given. */
		if (type == LUA_TSTRING) {
			if (strcmp(param_name, "name") == 0) {
				++skipped;
				continue;
			}
		} else /* type == LUA_TNUMBER */ {
			if (strcmp(param_name, "jid") == 0) {
				++skipped;
				continue;
			}
		}
		if (jailparam_init(&params[i - skipped], param_name) == -1) {
			jailparam_free(params, i - skipped);
			free(params);
			return (luaL_error(L, "jailparam_init: %s",
			    jail_errmsg));
		}
	}
	params_count -= skipped;

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

		value = jailparam_export(&params[i]);
		lua_pushstring(L, value);
		free(value);
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
	luaL_argcheck(L, params_count > 1, 2, "expected #params > 0");

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
		if (value == NULL) {
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

	return (1);
}
