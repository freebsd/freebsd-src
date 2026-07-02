#include <efi.h>
#include <efilib.h>
#include <lauxlib.h>

#include "lefi_guid.h"
#include "lefi_handles.h"
#include "lefi_protocols.h"
#include "lefi_utils.h"

static int
lua_efi_to_char16(lua_State *L)
{
	const char *s = luaL_checkstring(L, 1);
	size_t len = strlen(s);

	CHAR16 *buf = lua_newuserdata(L, (len + 1) * sizeof(CHAR16));

	for (size_t i = 0; i < len; i++)
		buf[i] = (CHAR16)(unsigned char)s[i];

	buf[len] = L'\0';
	return 1;
}

static int
lua_efi_handles(lua_State *L)
{
	EFI_HANDLE *buffer = NULL;
	UINTN bufsz = 0, i;
	EFI_STATUS status;

	status = BS->LocateHandle(AllHandles, NULL, NULL, &bufsz, buffer);
	if (status != EFI_BUFFER_TOO_SMALL) {
		return lefi_push_err(L, 1, "LocateHandle failed", status);
	}
	if ((buffer = malloc(bufsz)) == NULL) {
		return lefi_push_err(L, 1, "out of memory",
		    EFI_OUT_OF_RESOURCES);
	}

	status = BS->LocateHandle(AllHandles, NULL, NULL, &bufsz, buffer);
	if (EFI_ERROR(status)) {
		free(buffer);
		return lefi_push_err(L, 1, "LocateHandle failed", status);
	}

	lua_newtable(L);

	for (i = 0; i < (bufsz / sizeof(EFI_HANDLE)); i++) {
		lua_efi_handle *h;

		h = lua_newuserdata(L, sizeof(*h));
		h->handle = buffer[i];

		luaL_setmetatable(L, "efi.handle");
		lua_rawseti(L, -2, (lua_Integer)i + 1);
	}
	free(buffer);
	return 1;
}

static int
lua_efi_bind(lua_State *L)
{
	lua_efi_handle *h;
	lua_efi_guid *id;
	lua_efi_protocol *p;
	void *iface;
	EFI_STATUS status;
	const struct protocol_binding_desc *pd;

	h = luaL_checkudata(L, 1, "efi.handle");

	id = luaL_checkudata(L, 2, "efi.guid");

	pd = lefi_find_proto_binding(&id->guid);
	if (pd == NULL)
		return lefi_push_err(L, 1, "unsupported protocol",
		    EFI_UNSUPPORTED);

	status = OpenProtocolByHandle(h->handle, &id->guid, &iface);
	if (EFI_ERROR(status))
		return lefi_push_err(L, 1, "OpenProtocol failed", status);

	if (iface == NULL)
		return lefi_push_err(L, 1,
		    "OpenProtocol returned NULL interface", EFI_DEVICE_ERROR);

	p = lua_newuserdata(L, sizeof(*p));
	p->iface = iface;

	luaL_getmetatable(L, pd->mt_name);
	lua_setmetatable(L, -2);

	return 1;
}

void
register_efi_handles(lua_State *L)
{
	lefi_register_mt(L, "efi.handle", NULL);

	lua_pushcfunction(L, lua_efi_handles);
	lua_setfield(L, -2, "handles");

	lua_pushcfunction(L, lua_efi_bind);
	lua_setfield(L, -2, "bind");

	lua_pushcfunction(L, lua_efi_to_char16);
	lua_setfield(L, -2, "to_char16");
}
