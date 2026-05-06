#include <lauxlib.h>
#include <lutils.h>

#include "lefi.h"
#include "lefi_guid.h"
#include "lefi_handles.h"
#include "lefi_protocols.h"

int
luaopen_efi(lua_State *L)
{
	lua_newtable(L);
	register_efi_guids(L);
	register_protocol_binding_mts(L);
	register_efi_protocols(L);
	register_efi_handles(L);

	return 1;
}

static void
efi_init_md(lua_State *L)
{
	luaL_requiref(L, "efi", luaopen_efi, 1);
	lua_pop(L, 1);
}

LUA_COMPILE_SET(efi_init_md);
