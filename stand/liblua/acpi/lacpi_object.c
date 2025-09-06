#include <lua.h>
#include <lauxlib.h>
#include <lacpi.h>
#include "lacpi_object.h"
#include "lacpi_factory.h"
#include "lacpi.h"

static void build_acpi_obj(lua_State *L, ACPI_OBJECT *obj, UINT32 obj_type);
static void push_acpi_obj(lua_State *L, ACPI_OBJECT *obj);
static void free_acpi_obj(ACPI_OBJECT *obj);

static UINT32
lua_int_to_uint32(lua_State *L, int index, const char *errmsg)
{
	lua_Integer temp = luaL_checkinteger(L, index);
	if (temp < 0 || temp > UINT32_MAX) {
		luaL_error(L, "%s", errmsg);
	}

	return (UINT32)temp;
}

/*
 * Retrieves opaque pointer (ACPI_HANDLE).
 *
 * A string containing a valid ACPI pathname must be on 
 * top of the Lua stack.
 * Passes back a lacpi_node object. 
 */
static int 
LAcpiGetHandle(lua_State *L)
{
	const char *pathname = luaL_checkstring(L, 1);
	ACPI_HANDLE handle;
	ACPI_STATUS status;

	if (ACPI_FAILURE(status = AcpiGetHandle(NULL, pathname, &handle))) {
		return luaL_error(L, "AcpiGetHandle failed with status 0x%x", status);
	}
	

	struct lacpi_node *node = (struct lacpi_node *)lua_newuserdata(L, sizeof(struct lacpi_node));
	
	node->pathname = strdup(pathname);
	if (node->pathname == NULL) {
		return luaL_error(L, "Failed to strdup pathname.");
	}

	node->handle = handle;
	
	luaL_getmetatable(L, "lacpi_node");
	lua_setmetatable(L, -2);
	
	return 1;
}

static void
build_int(lua_State *L, ACPI_OBJECT *obj)
{
	lua_getfield(L, -1, "value");
	obj->Type = ACPI_TYPE_INTEGER;
	obj->Integer.Value = lua_int_to_uint32(L, -1, 
	    "ACPI object integer out of range");
	lua_pop(L, 1);
}

static void
build_str(lua_State *L, ACPI_OBJECT *obj)
{
	lua_getfield(L, -1, "value");
	size_t len;
	const char *str = luaL_checklstring(L, -1, &len);
	obj->Type = ACPI_TYPE_STRING;
	obj->String.Pointer = strdup((char *)str);
	obj->String.Length = (UINT32)len;
	lua_pop(L, 1);
}

static void
build_buff(lua_State *L, ACPI_OBJECT *obj)
{
	lua_getfield(L, -1, "value");
	size_t len;
	const char *str = luaL_checklstring(L, -1, &len);
	obj->Type = ACPI_TYPE_BUFFER;
	obj->Buffer.Pointer = strdup((char *)str);
	obj->Buffer.Length = (UINT32)len;
	lua_pop(L, 1);
}

static void
build_pkg(lua_State *L, ACPI_OBJECT *obj)
{
	lua_getfield(L, -1, "value");
	obj->Type = ACPI_TYPE_PACKAGE;
	obj->Package.Count = lua_rawlen(L, -1);
	obj->Package.Elements = calloc(obj->Package.Count, sizeof(ACPI_OBJECT));
	if (obj->Package.Elements == NULL) {
		luaL_error(L, "Failed to allocate ACPI package elements");
	}

	for (size_t i = 0; i < obj->Package.Count; ++i) {
		lua_rawgeti(L, -1, i + 1);
		lua_getfield(L, -1, "obj_type");
		UINT32 type = lua_int_to_uint32(L, -1,
		    "ACPI object type out of range");
		lua_pop(L, 1);
		build_acpi_obj(L, &obj->Package.Elements[i], type);
		lua_pop(L, 1);
	}

	lua_pop(L, 1);
}

static void
build_acpi_obj(lua_State *L, ACPI_OBJECT *obj, UINT32 obj_type)
{
	// XXX add all ACPI_TYPES to switch
	switch(obj_type) {
		case ACPI_TYPE_INTEGER:
			build_int(L, obj);
			break;
		case ACPI_TYPE_STRING:
			build_str(L, obj);
			break;
		case ACPI_TYPE_BUFFER:
			build_buff(L, obj);
			break;
		case ACPI_TYPE_PACKAGE:
			build_pkg(L, obj);
			break;
		default:
			luaL_error(L, "Unable to build object: %d.", obj_type);
	}
}

static void 
push_int(lua_State *L, ACPI_OBJECT *obj)
{
	lua_pushinteger(L, obj->Integer.Value);
}

static void
push_str(lua_State *L, ACPI_OBJECT *obj)
{
	lua_pushlstring(L, obj->String.Pointer, obj->String.Length); 
}

static void
push_buff(lua_State *L, ACPI_OBJECT *obj)
{
	lua_pushlstring(L, (const char*)obj->Buffer.Pointer,
	    obj->Buffer.Length);
}

static void
push_pkg(lua_State *L, ACPI_OBJECT *obj)
{
	lua_newtable(L);
	for (UINT32 i = 0; i < obj->Package.Count; ++i) {
		push_acpi_obj(L, &obj->Package.Elements[i]);
		lua_rawseti(L, -2, i + 1);
	}
}

static void
push_acpi_obj(lua_State *L, ACPI_OBJECT *obj)
{
	// XXX add all ACPI_TYPES to switch
	switch (obj->Type) {
		case ACPI_TYPE_INTEGER:
			push_int(L, obj);
			break;
		case ACPI_TYPE_STRING:
			push_str(L, obj);
			break;
		case ACPI_TYPE_BUFFER:
			push_buff(L, obj);
			break;
		case ACPI_TYPE_PACKAGE:
			push_pkg(L, obj);
			break;
		default:
			luaL_error(L, "Unable to push obj: %d.", obj->Type);
	}
}

static void
free_str(ACPI_OBJECT *obj)
{
	if (obj->String.Pointer) {
		free(obj->String.Pointer);
		obj->String.Pointer = NULL;
	}
	
	obj->String.Length = 0;
}

static void
free_buff(ACPI_OBJECT *obj)
{
	if (obj->Buffer.Pointer) {
		free(obj->Buffer.Pointer);
		obj->Buffer.Pointer = NULL;
	}

	obj->Buffer.Length = 0;
}

static void
free_pkg(ACPI_OBJECT *obj)
{
	for (UINT32 i = 0; i < obj->Package.Count; ++i) {
		free_acpi_obj(&obj->Package.Elements[i]);
	}
	
	free(obj->Package.Elements);
	obj->Package.Elements = NULL;
	obj->Package.Count = 0;
}

static void
free_acpi_obj(ACPI_OBJECT *obj)
{
	if (obj) {
		switch (obj->Type) {
			case ACPI_TYPE_STRING:
				free_str(obj);
				break;
			case ACPI_TYPE_BUFFER:
				free_buff(obj);
				break;
			case ACPI_TYPE_PACKAGE:
				free_pkg(obj);
				break;
			default:
				break;
		}
	}
}

static void
free_acpi_objs(ACPI_OBJECT *objs, UINT32 obj_count)
{
	if (objs) {
		for (UINT32 i = 0; i < obj_count; ++i) {
			free_acpi_obj(&objs[i]);
		}

		free(objs);
		objs = NULL;
	}
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
 * Only if 1 is provided, 2 may also be provided, but not required.
 * Any other arguments not being passed must be nil.
 *
 * Returns: ACPI object or table of ACPI objects.
 */
static int
LAcpiEvaluateObject(lua_State *L)
{
	ACPI_STATUS status;
	ACPI_OBJECT_LIST obj_list;
	ACPI_HANDLE handle = NULL;
	ACPI_OBJECT *objs = NULL;
	UINT32 obj_type = -1;
	UINT32 obj_count = 0;
	ACPI_BUFFER return_buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	char *pathname = NULL;

	if (lua_isuserdata(L, 1)) {
		handle = *(ACPI_HANDLE *)luaL_checkudata(L, 1, "lacpi_node");
	}
	
	const char *rel_path = lua_isstring(L, 2) ? lua_tostring(L, 2) : NULL;
	const char *abs_path = lua_isstring(L, 3) ? lua_tostring(L, 3) : NULL;

	pathname = rel_path ? strdup(rel_path) : strdup(abs_path);

	if (handle == NULL && pathname == NULL) {
		return luaL_error(L, 
		    "At least one of handle or pathname must be provided");
	}

	if (lua_istable(L, 4)) {
		obj_count = lua_rawlen(L, 4);
		objs = malloc(sizeof(ACPI_OBJECT) * obj_count);
		if (objs == NULL) {
			return luaL_error(L, "Failed to malloc objs.");
		}

		for (int i = 0; i < obj_count; ++i) {
			lua_rawgeti(L, 4, i + 1);
		
			lua_getfield(L, -1, "obj_type");
			obj_type = lua_int_to_uint32(L, -1, 
			    "Invalid ACPI Object type");
			lua_pop(L, 1);
			build_acpi_obj(L, &objs[i], obj_type);
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

		return luaL_error(L, 
		    "AcpiEvaluateObject failed with status 0x%x", status);
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
			push_acpi_obj(L, (ACPI_OBJECT *)return_buffer.Pointer);
		}

		AcpiOsFree(return_buffer.Pointer);
		
		free_acpi_objs(objs, obj_count);
	}

	return 1;
}

static int
lacpi_node_gc(lua_State *L)
{
	struct lacpi_node *node = (struct lacpi_node *)luaL_checkudata(L, 1, 
	    "lacpi_node");
	if (node->pathname) {
		free((void *)node->pathname);
		node->pathname = NULL;
	}

	return 0;
}

static const 
luaL_Reg lacpi_funcs[] = {
	{ "get_handle", LAcpiGetHandle },
	{ "evaluate", LAcpiEvaluateObject },
	{ NULL, NULL }
};

int
luaopen_lacpi(lua_State *L)
{
	lacpi_create_mt_gc(L, "lacpi_node", lacpi_node_gc);

	luaL_newlib(L, lacpi_funcs);
	return 1;
}

void
lacpi_object_interp_ref(void)
{
}

LUA_ACPI_COMPILE_SET(acpi_object, luaopen_lacpi);
