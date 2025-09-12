#include <init_acpi.h>
#include <lutils.h>
#include <lauxlib.h>
#include "lacpi.h"
#include "lacpi_object.h"
#include "lacpi_walk.h"
#include "lacpi_data.h"

/*
 * Reference set for all lacpi modules.
 */
void
lacpi_interp_ref(void)
{
	lacpi_object_interp_ref();
	lacpi_walk_interp_ref();
	lacpi_data_interp_ref();
}

int
luaopen_lacpi(lua_State *L)
{
	lua_newtable(L);

	luaopen_lacpi_object(L);
	lua_setfield(L, -2, "object");

	luaopen_lacpi_walk(L);
	lua_setfield(L, -2, "walk");
	
	luaopen_lacpi_data(L);
	lua_setfield(L, -2, "data");

	return 1;
}

/*
 * Unpacks all lacpi modules.
 */
static void
lua_acpi_bindings(lua_State *L)
{
	struct lua_acpi_module **mod;

	SET_FOREACH(mod, lua_acpi_modules) {
		(*mod)->init(L);
		lua_setglobal(L, (*mod)->mod_name);
	}

	luaopen_lacpi(L);
	lua_setglobal(L, "lacpi");
}

/*
 * Function hook for lacpi modules.
 */
void
lua_acpi_register_hook(void)
{
	if (acpi_is_initialized()) {
		lua_acpi_register = lua_acpi_bindings;
	}
}

LUA_ACPI_COMPILE_SET(lacpi, luaopen_lacpi);
