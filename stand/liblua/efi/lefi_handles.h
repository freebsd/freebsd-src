#pragma once
#include <efi.h>
#include <lua.h>

typedef struct {
	EFI_HANDLE handle;
} lua_efi_handle;

void register_efi_handles(lua_State *L);
