#pragma once

#include <lua.h>
#include <lauxlib.h>

void lacpi_data_interp_ref(void);
int luaopen_lacpi_data(lua_State *L);
