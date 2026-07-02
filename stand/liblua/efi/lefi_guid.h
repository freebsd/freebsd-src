#pragma once
#include <efi.h>
#include <lua.h>

typedef struct lua_efi_guid {
	EFI_GUID guid;
} lua_efi_guid;

struct efi_uuid_mapping {
	const char *efi_guid_name;
	EFI_GUID efi_guid;
};

int lefi_guid_equal(const EFI_GUID *, const EFI_GUID *);
void register_efi_guids(lua_State *L);
