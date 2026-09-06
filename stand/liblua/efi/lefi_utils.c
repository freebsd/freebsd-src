#include "lefi_utils.h"

int
lefi_push_err(lua_State *L, int push_nil, const char *errmsg, EFI_STATUS status)
{
	int n = 0;

	if (push_nil) {
		lua_pushnil(L);
		n++;
	}

	if (errmsg != NULL) {
		lua_pushstring(L, errmsg);
		n++;

		lua_pushinteger(L, (lua_Integer)status);
		n++;
	}

	return n;
}

void
lefi_register_mt(lua_State *L, const char *mt_name, const luaL_Reg *methods)
{
	luaL_newmetatable(L, mt_name);

	if (methods != NULL) {
		luaL_setfuncs(L, methods, 0);

		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}

	lua_pop(L, 1);
}
