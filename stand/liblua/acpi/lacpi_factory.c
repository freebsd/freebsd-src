#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

int
lacpi_create_mt_gc(lua_State *L, const char *mt, lua_CFunction gc_func)
{
	if (luaL_newmetatable(L, mt)) {
		lua_pushcfunction(L, gc_func);
		lua_setfield(L, -2, "__gc");

		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}

	return 1;
}
