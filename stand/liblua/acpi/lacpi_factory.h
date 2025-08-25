#pragma once

int lacpi_create_mt_gc(lua_State *L, const char *mt, lua_CFunction gc_func);
