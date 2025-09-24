#pragma once

#include <lua.h>
#include <sys/linker_set.h>
#include <contrib/dev/acpica/include/acpi.h>

typedef int (*lua_module_init_fn)(lua_State *L);
extern void lacpi_object_interp_ref(void);
extern void lacpi_data_interp_ref(void);

struct lua_acpi_module {
	const char *mod_name;
	lua_module_init_fn init;
};

SET_DECLARE(lua_acpi_modules, struct lua_acpi_module);

#define LUA_ACPI_COMPILE_SET(name, initfn)			\
	static struct lua_acpi_module lua_##name =		\
	{							\
		.mod_name = #name,				\
		.init = initfn					\
	};							\
	DATA_SET(lua_acpi_modules, lua_##name)

struct lacpi_node {
	const char*	pathname;
	ACPI_HANDLE 	handle;
};

void lacpi_interp_ref(void);
void lua_acpi_register_hook(void);
