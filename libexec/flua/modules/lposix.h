/*-
 *
 * This file is in the public domain.
 */

#pragma once

#include <lua.h>

int luaopen_posix_libgen(lua_State *L);
int luaopen_posix_stdlib(lua_State *L);
int luaopen_posix_sys_stat(lua_State *L);
int luaopen_posix_sys_utsname(lua_State *L);
int luaopen_posix_sys_wait(lua_State *L);
int luaopen_posix_unistd(lua_State *L);
