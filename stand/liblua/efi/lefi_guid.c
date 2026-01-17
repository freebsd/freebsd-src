#include <efi.h>
#include <efilib.h>
#include <lauxlib.h>

#include "lefi_guid.h"
#include "lefi_utils.h"

#define EFI_GUID_LIST                  \
	X(SIMPLE_TEXT_OUTPUT_PROTOCOL) \
	X(SIMPLE_TEXT_INPUT_PROTOCOL)

static struct efi_uuid_mapping efi_uuid_mapping[] = {
#define X(name) { #name, name },
	EFI_GUID_LIST
#undef X
};

#undef EFI_GUID_LIST

static int
lua_efi_guid_name(lua_State *L)
{
	lua_efi_guid *g = luaL_checkudata(L, 1, "efi.guid");
	char *name = NULL;

	if (efi_guid_to_name(&g->guid, &name)) {
		lua_pushstring(L, name);
		free(name);
		return 1;
	}

	lua_pushnil(L);
	return 1;
}

int
lefi_guid_equal(const EFI_GUID *a, const EFI_GUID *b)
{
	return memcmp(a, b, sizeof(EFI_GUID)) == 0;
}

static int
lua_efi_guid_eq(lua_State *L)
{
	struct lua_efi_guid *a = luaL_checkudata(L, 1, "efi.guid");
	struct lua_efi_guid *b = luaL_checkudata(L, 2, "efi.guid");

	lua_pushboolean(L, lefi_guid_equal(&a->guid, &b->guid));
	return 1;
}

static const luaL_Reg efi_guid_methods[] = { { "__eq", lua_efi_guid_eq },
	{ "to_name", lua_efi_guid_name }, { NULL, NULL } };

void
register_efi_guid_mt(lua_State *L)
{
	lefi_register_mt(L, "efi.guid", efi_guid_methods);
}

static void
lefi_push_efi_guid(lua_State *L, const char *name, const EFI_GUID *guid)
{
	struct lua_efi_guid *u;

	u = lua_newuserdata(L, sizeof(*u));
	memcpy(&u->guid, guid, sizeof(EFI_GUID));

	luaL_setmetatable(L, "efi.guid");
	lua_setfield(L, -2, name);
}

void
lefi_populate_efi_guid_table(lua_State *L)
{
	lua_newtable(L);

	for (size_t i = 0;
	    i < sizeof(efi_uuid_mapping) / sizeof(efi_uuid_mapping[0]); i++) {
		lefi_push_efi_guid(L, efi_uuid_mapping[i].efi_guid_name,
		    &efi_uuid_mapping[i].efi_guid);
	}

	lua_setfield(L, -2, "guid");
}

void
register_efi_guids(lua_State *L)
{
	register_efi_guid_mt(L);
	lefi_populate_efi_guid_table(L);
}
