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
