#include <efi.h>
#include <efilib.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <contrib/dev/acpica/include/acpi.h>
#include "lacpi.h"
#include "lacpi_data.h"
#include "lacpi_utils.h"

/*
 * Dynamic dispatcher for destructor based on ACPI_OBJECT_TYPE.
 * Do nothing in the case of integer-based ACPI_OBJECT_TYPES.
 */
acpi_destructor_t
get_object_destructor(UINT32 type)
{
	switch (type) {
		case ACPI_TYPE_INTEGER:
		case ACPI_TYPE_LOCAL_REFERENCE:
		case ACPI_TYPE_PROCESSOR:
		case ACPI_TYPE_POWER:
			return free_fake;
		case ACPI_TYPE_STRING:
			return free_str;
		case ACPI_TYPE_BUFFER:
			return free_buff;
		case ACPI_TYPE_PACKAGE:
			return free_pkg;
		default:
			return NULL;
	}
}

/*
 * Attaching data requires providing a handler method.
 */
void
acpi_object_handler(ACPI_HANDLE handle, void *data)
{
	if (data != NULL) {
		ACPI_OBJECT *obj = (ACPI_OBJECT *)data;

		acpi_destructor_t dtor = get_object_destructor(obj->Type);
		if (dtor) {
			dtor(obj);
		} else {
			free(obj);
			obj == NULL;
		}
	}
}

/*
 * Create an ACPI_OBJECT to attach to a namespace node.
 * Uses ACPI_OBJECT as an interface for building the data
 * object so we can re-use it rather than re-invent the wheel.
 *
 * Lua stack expectations:
 * 1 = ACPI_HANDLE - the handle of the node to attach data onto
 * 2 = ACPI_OBJECT_TYPE - the type of the object
 * 
 * The rest of the stack should match 1:1 to the ACPI_OBJECTs.
 * Example: if attaching ACPI_TYPE_PACKAGE, Count should be next
 * on the stack, and then *Elements.
 */
static int
lAcpiAttachData(lua_State *L)
{
	ACPI_STATUS status;	
	ACPI_HANDLE handle = (ACPI_HANDLE) lua_touserdata(L, 1);
	UINT32 type = lua_int_to_uint32(L, 2, "Object type must be 32 bit");
	ACPI_OBJECT *obj;

	if (handle == NULL) {
		return luaL_error(L, "lAcpiAttachData: Handle is NULL");
	}

	obj = malloc(sizeof(ACPI_OBJECT));
	if (obj == NULL) {
		return luaL_error(L, 
		    "lAcpiAttachData: Failed to malloc ACPI_OBJECT");
	}
	
	build_acpi_obj(L, obj, type);

	if (ACPI_FAILURE(status = AcpiAttachData(handle, acpi_object_handler, (void *)obj))) {
		free_acpi_obj(obj);
		return luaL_error(L,
		    "lAcpiAttachData: AcpiAttachData failed with status: 0x%x",
		    status);
	}

	lua_pushinteger(L, (lua_Integer)status);
	return 1;
}

/*
 * Removes data from an ACPI node.
 */
static int
lAcpiDetachData(lua_State *L)
{
	ACPI_STATUS status;
	ACPI_HANDLE handle = (ACPI_HANDLE) lua_touserdata(L, 1);
	
	if (handle == NULL) {
		return luaL_error(L, "lAcpiDetachData: NULL Argument(s)");
	}

	if (ACPI_FAILURE(status = AcpiDetachData(handle, acpi_object_handler))) {
		return luaL_error(L,
		    "lAcpiDetachData: AcpiDetachData failed with status: 0x%x",
		    status);
	}

	lua_pushinteger(L, (lua_Integer)status);
	return 1;
}

/*
 * Retrieves data from an ACPI node.
 */
static int
lAcpiGetData(lua_State *L)
{
	ACPI_STATUS status;
	ACPI_HANDLE handle = (ACPI_HANDLE) lua_touserdata(L, 1);
	void *data = NULL;

	if (handle == NULL) {
		return luaL_error(L, "lAcpiGetData: NULL Argument(s)");
	}
	
	if (ACPI_FAILURE(status = AcpiGetData(handle, acpi_object_handler, &data))) {
		return luaL_error(L,
		    "lAcpiGetData: AcpiGetData failed with status: 0x%x",
		    status);
	}
	
	push_acpi_obj(L, (ACPI_OBJECT *)data);
	return 1; 
}

static const
luaL_Reg lacpi_data_funcs[] = {
        { "attach", lAcpiAttachData },
        { "detach", lAcpiDetachData },
        { "get", lAcpiGetData },
        { NULL, NULL }
};

int
luaopen_lacpi_data(lua_State *L)
{
	luaL_newlib(L, lacpi_data_funcs);
	return 1;
}

void
lacpi_data_interp_ref(void)
{
}
