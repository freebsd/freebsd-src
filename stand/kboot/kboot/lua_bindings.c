/*
 * Copyright (c) 2024 Netflix, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "lua.h"
#include "lauxlib.h"
#include "lutils.h"
#include "lhash.h"

static void
lua_hash_bindings(lua_State *L)
{
	luaL_requiref(L, "hash", luaopen_hash, 1);
	lua_pop(L, 1);	/* Remove lib */
}

LUA_COMPILE_SET(lua_hash_bindings);
