#pragma once
#include <efi.h>
#include <lauxlib.h>
#include <lua.h>

struct protocol_binding_desc {
	const EFI_GUID guid;
	const char *mt_name;
	const luaL_Reg *methods;
};

typedef struct {
	void *iface;
} lua_efi_protocol;

void register_efi_protocols(lua_State *L);

void register_protocol_binding_mts(lua_State *L);

const struct protocol_binding_desc *lefi_find_proto_binding(
    const EFI_GUID *guid);
