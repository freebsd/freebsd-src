#pragma once

#include <lua.h>
#include <lauxlib.h>

void lacpi_object_interp_ref(void);
void lacpi_node_register_mt(lua_State *L);
int luaopen_lacpi_object(lua_State *L);
