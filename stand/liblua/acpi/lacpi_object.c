#include "lacpi_utils.h"
#include "lacpi.h"
#include "lacpi_utils.h"
#include "lacpi.h"

/*
 * Retrieves opaque pointer (ACPI_HANDLE).
 *
 * A string containing a valid ACPI pathname must be on 
 * top of the Lua stack.
 * Passes back an opaque pointer to lacpi_node object.
 */
static int 
lAcpiGetHandle(lua_State *L)
{
	const char *pathname = luaL_checkstring(L, 1);
	ACPI_HANDLE handle;
	ACPI_STATUS status;

	if (ACPI_FAILURE(status = AcpiGetHandle(NULL, pathname, &handle))) {
		return lacpi_push_err(L, 1,
		    "AcpiGetHandleFailed with status: ", status);
	}
	
	lua_pushlightuserdata(L, handle);
	return 1;
}

/*
 * Lua stack expectations:
 *
 * 1 = userdata ACPI handle ("lacpi_node") or nil
 * 2 = relative pathname string from handle or nil
 * 3 = absolute pathname string or nil
 * 4 = optional table of ACPI objects (ACPI_OBJECT_LIST) to pass as arguments
 *
 * Either a handle (arg 1) or a pathname (arg 3) must be provided.
 * Only if arg 1 is provided, arg 2 may also be provided, but not required.
 * Any other arguments not being passed must be nil.
 *
 * Returns: ACPI object or table of ACPI objects.
 */
static int
lAcpiEvaluateObject(lua_State *L)
{
	ACPI_STATUS status;
	char errmsg[128];
	ACPI_OBJECT_LIST obj_list;
	ACPI_HANDLE handle = NULL;
	ACPI_OBJECT *objs = NULL;
	UINT32 obj_type = 0;
	UINT32 obj_count = 0;
	ACPI_BUFFER return_buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	char *pathname = NULL;

	if (lua_isuserdata(L, 1)) {
		handle = (ACPI_HANDLE)lua_touserdata(L, 1);
	}
	
	const char *rel_path = lua_isstring(L, 2) ? lua_tostring(L, 2) : NULL;
	const char *abs_path = lua_isstring(L, 3) ? lua_tostring(L, 3) : NULL;

	/*
	 * We must have either a handle and optionally a relative path,
	 * or just the absolute path.
	 */
	if (handle != NULL) {
		// Optional
		if (rel_path != NULL) {
			pathname = strdup(rel_path);
		}
	} else if (abs_path != NULL) {
		pathname = strdup(abs_path);
	} else {
		return lacpi_push_err(L, 1, "Incorrect arguments", 0);
	}

	/*
	 * 4 is expected to be a table of ACPI_OBJECTs.
	 * These are passed when the method you are evaluating needs them.
	 * Each element of this table is converted into an ACPI_OBJECT.
	 *
	 * Each element of this table must be a table with:
	 * 1: obj_type		-- UINT32 specifying the current element's 
	 *     ACPI_OBJECT_TYPE
	 * 2..n: fields		-- used to build the corresponding
	 *     ACPI_OBJECT, where n depends on obj_type. (see actypes.h)
	 *
	 */
	if (lua_istable(L, 4)) {
		obj_count = lua_rawlen(L, 4);
		objs = malloc(sizeof(ACPI_OBJECT) * obj_count);
		if (objs == NULL) {
			lua_pushnil(L);
			lua_pushstring(L, "Failed to malloc objs.");
			return 2;
		}

		for (int i = 0; i < obj_count; ++i) {
			lua_rawgeti(L, 4, i + 1);
			lua_getfield(L, -1, "obj_type");
			
			if (ACPI_FAILURE(status = lacpi_int_to_uint32(L, -1,
			    &obj_type))) {
				free(objs);
				return lacpi_push_err(L, 1, 
				    "ACPI_OBJECT_TYPE must be UINT32", 0);
			}
			lua_pop(L, 1);
			
			if (ACPI_FAILURE(status = 
			    build_acpi_obj(L, &objs[i], obj_type))) {
				snprintf(errmsg, sizeof(errmsg), 
				    "Failed to build objs[%d]", i);
				return lacpi_push_err(L, 1, errmsg, status);
			}
			lua_pop(L, 1);
		}

		obj_list.Count = obj_count;
		obj_list.Pointer = objs;
	} else {
		obj_list.Count = 0;
		obj_list.Pointer = NULL;
	}

	if (ACPI_FAILURE(status = AcpiEvaluateObject(handle, pathname, 
	    (obj_list.Count > 0) ? &obj_list : NULL, &return_buffer))) {
		if (return_buffer.Pointer) {
			AcpiOsFree(return_buffer.Pointer);
		}

		free_acpi_objs(objs, obj_count);

		char errbuf[64];
		return lacpi_push_err(L, 1, 
		    "AcpiEvaluateObject failed with status", status);
	}

	if (return_buffer.Pointer != NULL) {
		ACPI_OBJECT *ret_obj = (ACPI_OBJECT *)return_buffer.Pointer;
		
		if (ret_obj->Type == ACPI_TYPE_PACKAGE) {
			lua_newtable(L);
			for (UINT32 i = 0; i < ret_obj->Package.Count; ++i) {
				push_acpi_obj(L, &ret_obj->Package.Elements[i]);
				lua_rawseti(L, -2, i + 1);
			}
		} else {
			push_acpi_obj(L, ret_obj);
		}

		AcpiOsFree(return_buffer.Pointer);	
	}

	if (objs != NULL) {
		free_acpi_objs(objs, obj_count);
	}

	if (pathname != NULL) {
		free(pathname);
		pathname = NULL;
	}

	return 0;
}

static const 
luaL_Reg lacpi_object_funcs[] = {
	{ "get_handle", lAcpiGetHandle },
	{ "evaluate", lAcpiEvaluateObject },
	{ NULL, NULL }
};

int
luaopen_lacpi_object(lua_State *L)
{
	luaL_newlib(L, lacpi_object_funcs);
	return 1;
}

void
lacpi_object_interp_ref(void)
{
}
