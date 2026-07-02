#pragma once
#include <efi.h>
#include <efilib.h>
#include <lauxlib.h>
#include <lua.h>

int lefi_push_err(lua_State *L, int push_nil, const char *errmsg,
    EFI_STATUS status);

void lefi_register_mt(lua_State *L, const char *mt_name,
    const luaL_Reg *methods);
