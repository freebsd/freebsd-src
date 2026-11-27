#include "lefi_console.h"
#include "lefi_guid.h"
#include "lefi_handles.h"
#include "lefi_protocols.h"
#include "lefi_utils.h"

static int
lua_efi_protocols(lua_State *L)
{
	lua_efi_handle *h;
	EFI_GUID **protocols = NULL;
	UINTN nproto = 0;
	EFI_STATUS status;

	h = luaL_checkudata(L, 1, "efi.handle");

	status = BS->ProtocolsPerHandle(h->handle, &protocols, &nproto);
	if (EFI_ERROR(status))
		return lefi_push_err(L, 1, "ProtocolsPerHandle failed", status);

	lua_newtable(L);

	for (UINTN i = 0; i < nproto; i++) {
		lua_efi_guid *u;

		u = lua_newuserdata(L, sizeof(*u));
		u->guid = *protocols[i];
		luaL_setmetatable(L, "efi.guid");
		lua_rawseti(L, -2, (lua_Integer)i + 1);
	}

	BS->FreePool(protocols);
	return 1;
}

void
register_efi_protocols(lua_State *L)
{
	lefi_register_mt(L, "efi.protocol", NULL);

	lua_pushcfunction(L, lua_efi_protocols);
	lua_setfield(L, -2, "protocols");
}

static struct protocol_binding_desc proto_binding_table[] = {
	{ SIMPLE_TEXT_OUTPUT_PROTOCOL, "efi.protocol.conout",
	    console_out_methods },

	{ 0 }
};

void
register_protocol_binding_mts(lua_State *L)
{
	for (int i = 0; proto_binding_table[i].mt_name != NULL; i++) {
		lefi_register_mt(L, proto_binding_table[i].mt_name,
		    proto_binding_table[i].methods);
	}
}

const struct protocol_binding_desc *
lefi_find_proto_binding(const EFI_GUID *guid)
{
	for (int i = 0; proto_binding_table[i].mt_name; i++) {
		if (lefi_guid_equal(guid, &proto_binding_table[i].guid))
			return &proto_binding_table[i];
	}
	return NULL;
}
