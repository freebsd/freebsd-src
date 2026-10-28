#pragma once

#include <lauxlib.h>

struct context {
	lua_State *L;
	int tbl;
	int idx;
};

int luaopen_lacpi_walk(lua_State *L);
void lacpi_walk_interp_ref(void);
