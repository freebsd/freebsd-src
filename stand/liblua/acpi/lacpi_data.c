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
			obj = NULL;
		}
	}
}

/*
 * Create an ACPI_OBJECT to attach to a namespace node.
 * Uses ACPI_OBJECT as an interface for building the data
 * object.
 *
 * Lua stack expectations:
 * 1 = ACPI_HANDLE - lightuserdata (opaque) handle of the node to attach the
 * data onto
 * 2..n = Argument table containing each field of an ACPI_OBJECT
 * Example: lacpi.data.attach(handle, { Integer = 42 })
 * (see actypes.h for reference)
 */
static int
lAcpiAttachData(lua_State *L)
{
	ACPI_STATUS status;	
	ACPI_HANDLE handle;
	UINT32 type;
	ACPI_OBJECT *obj;
	int obj_idx = 2; // start of ACPI_OBJECT parameters

	handle = lacpi_check_handle(L, 1);
	if (handle == NULL) {
		return lacpi_push_err(L, 0, "lAcpiAttachData: Handle is NULL. "
		    "Status: ",
		    AE_NULL_OBJECT);
	}

	if (!lua_istable(L, 2)) {
		return lacpi_push_err(L, 0, "lAcpiAttachData: Expected table. Status: ",
		    AE_TYPE);
	}

	if (ACPI_FAILURE(status = lacpi_infer_type(L, 2, &type))) {
		return lacpi_push_err(L, 0,
		    "Cannot infer ACPI_OBJECT_TYPE from table. Status: ", status);
	}

	obj = calloc(1, sizeof(ACPI_OBJECT));
	if (obj == NULL) {
		return lacpi_push_err(L, 0,
		    "lAcpiAttachData: Failed to malloc ACPI_OBJECT. Status: ",
		    AE_NULL_OBJECT);
	}
	
	if (ACPI_FAILURE(status = build_acpi_obj(L, obj, type, obj_idx))) {
		free_acpi_obj(obj);
		free(obj);
		return lacpi_push_err(L, 0, 
		    "lAcpiAttachData: Failed to build ACPI_OBJECT. Status: ",
		    status);
	}

	if (ACPI_FAILURE(status = AcpiAttachData(handle, acpi_object_handler,
	    (void *)obj))) {
		free_acpi_obj(obj);
		free(obj);
		return lacpi_push_err(L, 0,
		    "lAcpiAttachData: AcpiAttachData failed with status: ",
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
		return lacpi_push_err(L, 1, "lAcpiDetachData: NULL Argument(s)",
		    AE_NULL_OBJECT);
	}

	if (ACPI_FAILURE(status = AcpiDetachData(handle,
	    acpi_object_handler))) {
		return lacpi_push_err(L, 1,
		    "lAcpiDetachData: AcpiDetachData failed with status: ",
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
		return lacpi_push_err(L, 1, "lAcpiGetData: NULL Argument(s). "
		    "Status: ",
		    AE_NULL_OBJECT);
	}
	
	if (ACPI_FAILURE(status = AcpiGetData(handle, acpi_object_handler,
	    &data))) {
		return lacpi_push_err(L, 1,
		    "lAcpiGetData: AcpiGetData failed with status: ",
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
