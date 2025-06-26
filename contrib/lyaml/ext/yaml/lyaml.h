/*
 * lyaml.h, libyaml parser binding for Lua
 * Written by Gary V. Vaughan, 2013
 *
 * Copyright (C) 2013-2022 Gary V. Vaughan
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef LYAML_H
#define LYAML_H 1

#include <yaml.h>

#include <lua.h>
#include <lauxlib.h>

#include "lyaml.h"

#if LUA_VERSION_NUM == 502 || LUA_VERSION_NUM == 503 || LUA_VERSION_NUM == 504
#  define lua_objlen lua_rawlen
#  define lua_strlen lua_rawlen
#  define luaL_openlib(L,n,l,nup) luaL_setfuncs((L),(l),(nup))
#  define luaL_register(L,n,l) (luaL_newlib(L,l))
#endif

#ifndef STREQ
#define STREQ !strcmp
#endif
#ifndef STRNEQ
#define STRNEQ strcmp
#endif

/* NOTE: Make sure L is in scope before using these macros.
         lua_pushyamlstr casts away the impedance mismatch between Lua's
	 signed char APIs and libYAML's unsigned char APIs. */

#define lua_pushyamlstr(_s) lua_pushstring (L, (char *)(_s))

#define RAWSET_BOOLEAN(_k, _v)				\
        lua_pushyamlstr (_k);				\
        lua_pushboolean (L, (_v) != 0);			\
        lua_rawset      (L, -3)

#define RAWSET_INTEGER(_k, _v)				\
        lua_pushyamlstr (_k);				\
        lua_pushinteger (L, (_v));			\
        lua_rawset      (L, -3)

#define RAWSET_STRING(_k, _v)				\
        lua_pushyamlstr (_k);				\
        lua_pushyamlstr (_v);				\
        lua_rawset      (L, -3)

#define RAWSET_EVENTF(_k)				\
        lua_pushstring  (L, #_k);			\
        lua_pushyamlstr (EVENTF(_k));			\
        lua_rawset      (L, -3)


/* NOTE: Make sure L is in scope before using these macros.
         The table value at _k is not popped from the stack for strings
	 or tables, so that we can check for an empty table entry with
	 lua_isnil (L, -1), or get the length of a string with
	 lua_objlen (L, -1) before popping. */

#define RAWGET_BOOLEAN(_k)				\
	lua_pushstring (L, #_k);			\
	lua_rawget     (L, -2);				\
	if (!lua_isnil (L, -1)) {			\
	   _k = lua_toboolean (L, -1);			\
	}						\
	lua_pop (L, 1)

#define RAWGET_INTEGER(_k)				\
	lua_pushstring (L, #_k);			\
	lua_rawget     (L, -2);				\
	if (!lua_isnil (L, -1)) {			\
	   _k = lua_tointeger (L, -1);			\
	}						\
	lua_pop (L, 1)

#define RAWGET_STRING(_k)				\
	lua_pushstring (L, #_k);			\
	lua_rawget     (L, -2);				\
	if (!lua_isnil (L, -1)) {			\
	  _k = lua_tostring (L, -1);			\
	} else { _k = NULL; }

#define RAWGET_STRDUP(_k)				\
	lua_pushstring (L, #_k);			\
	lua_rawget     (L, -2);				\
	if (!lua_isnil (L, -1)) {			\
	  _k = strdup (lua_tostring (L, -1));		\
	} else { _k = NULL; }

#define RAWGET_YAML_CHARP(_k)				\
	lua_pushstring (L, #_k);			\
	lua_rawget     (L, -2);				\
	if (!lua_isnil (L, -1)) {			\
	  _k = (yaml_char_t *) lua_tostring (L, -1);	\
	} else { _k = NULL; }

#define RAWGETS_INTEGER(_v, _s)				\
	lua_pushstring (L, _s);				\
	lua_rawget     (L, -2);				\
	if (!lua_isnil (L, -1)) {			\
	   _v = lua_tointeger (L, -1);			\
	}						\
	lua_pop (L, 1)

#define RAWGETS_STRDUP( _v, _s)				\
	lua_pushstring (L, _s);				\
	lua_rawget     (L, -2);				\
	if (!lua_isnil (L, -1)) {			\
	   _v = (yaml_char_t *) strdup (lua_tostring (L, -1)); \
	} else { _v = NULL; }

#define RAWGET_PUSHTABLE(_k)						\
	lua_pushstring (L, _k);						\
	lua_rawget     (L, -2);						\
	if ((lua_type (L, -1) != LUA_TTABLE) && !lua_isnil (L, -1)) {	\
	  lua_pop (L, 1);						\
	  return luaL_error (L, "%s must be a table", _k);		\
	}

#define ERROR_IFNIL(_err)				\
	if (lua_isnil (L, -1)) {			\
	  emitter->error++;				\
	  luaL_addstring (&emitter->errbuff, _err);	\
	}


/* from emitter.c */
extern int	Pemitter	(lua_State *L);

/* from parser.c */
extern void	parser_init	(lua_State *L);
extern int	Pparser		(lua_State *L);

/* from scanner.c */
extern void	scanner_init	(lua_State *L);
extern int	Pscanner	(lua_State *L);

#endif
