#include <assert.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "lacpi_utils.h"

void build_acpi_obj(lua_State *L, ACPI_OBJECT *obj, UINT32 obj_type);
void push_acpi_obj(lua_State *L, ACPI_OBJECT *obj);
void free_acpi_obj(ACPI_OBJECT *obj);

/***** UTILITY *****/

ACPI_HANDLE
lua_check_handle(lua_State *L, int idx)
{
	if (lua_islightuserdata(L, idx)) {
		return (ACPI_HANDLE)lua_touserdata(L, idx);
	}

	return NULL;
}

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

UINT32
lua_int_to_uint32(lua_State *L, int index, const char *errmsg)
{
	lua_Integer temp = luaL_checkinteger(L, index);
	if (temp < 0 || temp > UINT32_MAX) {
		luaL_error(L, "%s", errmsg);
	}

	return (UINT32)temp;
}

/***** FACTORY *****/

/*** ACPI_OBJECT ***/

void
build_int(lua_State *L, ACPI_OBJECT *obj)
{
	lua_getfield(L, -1, "Integer");
	obj->Type = ACPI_TYPE_INTEGER;
	obj->Integer.Value = lua_int_to_uint32(L, -1, 
	    "ACPI object integer out of range");
	lua_pop(L, 1);
}

void
build_str(lua_State *L, ACPI_OBJECT *obj)
{
	lua_getfield(L, -1, "String");
	size_t len;
	const char *str = luaL_checklstring(L, -1, &len);
	obj->Type = ACPI_TYPE_STRING;
	obj->String.Pointer = strdup((char *)str);
	obj->String.Length = (UINT32)len;
	lua_pop(L, 1);
}

void
build_buff(lua_State *L, ACPI_OBJECT *obj)
{
	lua_getfield(L, -1, "Buffer");
	size_t len;
	const char *str = luaL_checklstring(L, -1, &len);
	obj->Type = ACPI_TYPE_BUFFER;
	obj->Buffer.Pointer = strdup((char *)str);
	obj->Buffer.Length = (UINT32)len;
	lua_pop(L, 1);
}

void
build_pkg(lua_State *L, ACPI_OBJECT *obj)
{
	lua_getfield(L, -1, "Elements");
	obj->Type = ACPI_TYPE_PACKAGE;
	obj->Package.Count = lua_rawlen(L, -1);
	obj->Package.Elements = calloc(obj->Package.Count,
	    sizeof(ACPI_OBJECT));
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

int
build_ref(lua_State *L, ACPI_OBJECT *obj)
{
	obj->Type = ACPI_TYPE_LOCAL_REFERENCE;
	
	lua_getfield(L, -1, "ActualType");
	obj->Reference.ActualType = lua_int_to_uint32(L, -1,
	    "ACPI object type out of range");
	lua_pop(L, 1);

	lua_getfield(L, -1, "Handle");
	if (lua_isuserdata(L, -1)) {
		ACPI_HANDLE handle = *(ACPI_HANDLE *)luaL_checkudata(L, -1, "lacpi_node");
		obj->Reference.Handle = handle;
		lua_pop(L, 1);
	} else {
		return luaL_error(L, "Handle not provided as first argument");
	}

	return 0;
}

int
build_proc(lua_State *L, ACPI_OBJECT *obj)
{
	obj->Type = ACPI_TYPE_PROCESSOR;

	lua_getfield(L, -1, "ProcID");
	obj->Processor.ProcId = lua_int_to_uint32(L, -1, "ProcId must be UINT32");
	lua_pop(L, 1);
	
	lua_getfield(L, -1, "PblkAddress");
	if (!lua_isinteger(L, -1)) {
		return luaL_error(L, 
		    "Unexpected value after ProcId");
	}
	obj->Processor.PblkAddress = (ACPI_IO_ADDRESS)lua_tointeger(L, -1);
	lua_pop(L, 1);
	
	lua_getfield(L, -1, "PblkLength");
	obj->Processor.PblkLength = lua_int_to_uint32(L, -1, "PblkLength must be UINT32");
	lua_pop(L, 1);

	return 0;
}

int
build_pow(lua_State *L, ACPI_OBJECT *obj)
{
	obj->Type = ACPI_TYPE_POWER;

	lua_getfield(L, -1, "SystemLevel");
	obj->PowerResource.SystemLevel = lua_int_to_uint32(L, -1,
	    "SystemLevel must be UINT32");
	lua_pop(L, 1);

	lua_getfield(L, -1, "ResourceOrder");
	obj->PowerResource.ResourceOrder = lua_int_to_uint32(L, -1,
	    "ResourceOrder must be UINT32");
	lua_pop(L, 1);

	return 0;
}

void
build_acpi_obj(lua_State *L, ACPI_OBJECT *obj, UINT32 obj_type)
{
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
		case ACPI_TYPE_LOCAL_REFERENCE:
			build_ref(L, obj);
			break;
		case ACPI_TYPE_PROCESSOR:
			build_proc(L, obj);
			break;
		case ACPI_TYPE_POWER:
			build_pow(L, obj);
			break;
		default:
			luaL_error(L, "Unable to build object: %d.", obj_type);
	}
}

void 
push_int(lua_State *L, ACPI_OBJECT *obj)
{
	lua_pushinteger(L, obj->Integer.Value);
}

void
push_str(lua_State *L, ACPI_OBJECT *obj)
{
	lua_pushlstring(L, obj->String.Pointer, obj->String.Length); 
}

void
push_buff(lua_State *L, ACPI_OBJECT *obj)
{
	lua_pushlstring(L, (const char*)obj->Buffer.Pointer,
	    obj->Buffer.Length);
}

void
push_pkg(lua_State *L, ACPI_OBJECT *obj)
{
	lua_newtable(L);
	for (UINT32 i = 0; i < obj->Package.Count; ++i) {
		push_acpi_obj(L, &obj->Package.Elements[i]);
		lua_rawseti(L, -2, i + 1);
	}
}

void
push_ref(lua_State *L, ACPI_OBJECT *obj)
{
	lua_newtable(L);

	lua_pushinteger(L, obj->Reference.ActualType);
	lua_setfield(L, -2, "ActualType");

	ACPI_HANDLE *handle = (ACPI_HANDLE *)lua_newuserdata(L, sizeof(ACPI_HANDLE));
	*handle = obj->Reference.Handle;
	luaL_setmetatable(L, "lacpi_node");
	lua_setfield(L, -2, "Handle");
}

void
push_proc(lua_State *L, ACPI_OBJECT *obj)
{
	lua_newtable(L);

	lua_pushinteger(L, obj->Processor.ProcId);
	lua_setfield(L, -2, "ProcId");

	lua_pushinteger(L, (lua_Integer)obj->Processor.PblkAddress);
	lua_setfield(L, -2, "PblkAddress");

	lua_pushinteger(L, obj->Processor.PblkLength);
	lua_setfield(L, -2, "PblkLength");
}

void
push_pow(lua_State *L, ACPI_OBJECT *obj)
{
	lua_newtable(L);

	lua_pushinteger(L, obj->PowerResource.SystemLevel);
	lua_setfield(L, -2, "SystemLevel");

	lua_pushinteger(L, obj->PowerResource.ResourceOrder);
	lua_setfield(L, -2, "ResourceOrder");
}

void
push_acpi_obj(lua_State *L, ACPI_OBJECT *obj)
{
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
		case ACPI_TYPE_LOCAL_REFERENCE:
			push_ref(L, obj);
			break;
		case ACPI_TYPE_PROCESSOR:
			push_proc(L, obj);
			break;
		case ACPI_TYPE_POWER:
			push_pow(L, obj);
			break;
		default:
			luaL_error(L, "Unable to push obj: %d.", obj->Type);
	}
}

/*
 * Keeps compiler happy during acpi_object_handler
 */
void
free_fake(ACPI_OBJECT *obj)
{
	/* No-op */
}

void
free_str(ACPI_OBJECT *obj)
{
	if (obj->String.Pointer) {
		free(obj->String.Pointer);
		obj->String.Pointer = NULL;
	}
	
	obj->String.Length = 0;
}

void
free_buff(ACPI_OBJECT *obj)
{
	if (obj->Buffer.Pointer) {
		free(obj->Buffer.Pointer);
		obj->Buffer.Pointer = NULL;
	}

	obj->Buffer.Length = 0;
}

void
free_pkg(ACPI_OBJECT *obj)
{
	for (UINT32 i = 0; i < obj->Package.Count; ++i) {
		free_acpi_obj(&obj->Package.Elements[i]);
	}
	
	free(obj->Package.Elements);
	obj->Package.Elements = NULL;
	obj->Package.Count = 0;
}

void
free_acpi_obj(ACPI_OBJECT *obj)
{
	if (obj) {
		switch (obj->Type) {
			case ACPI_TYPE_INTEGER:
			case ACPI_TYPE_LOCAL_REFERENCE:
			case ACPI_TYPE_PROCESSOR:
			case ACPI_TYPE_POWER:
				free_fake(obj);
				break;
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
				assert(obj == NULL);
				break;
		}

		free(obj);
		obj = NULL;
	}
}

void
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

/*** ACPI Namespace Node ***/

/*
 * Creates a key, value pair for the path of the current
 * namespace node.
 */
void
push_path(struct context *curr_ctx, const char *path)
{
    lua_pushstring(curr_ctx->L, "path");
    lua_pushstring(curr_ctx->L, path);
    lua_settable(curr_ctx->L, -3);
}

/*
 * Creates a key, value pair for the level of the current
 * namespace node.
 */
void
push_level(struct context *curr_ctx, UINT32 level)
{
    lua_pushstring(curr_ctx->L, "level");
    lua_pushinteger(curr_ctx->L, level);
    lua_settable(curr_ctx->L, -3);
}

/*
 * Creates a key, value pair for the hid of the current
 * namespace node.
 */
void
push_hid(struct context *curr_ctx, const char *hid)
{
    lua_pushstring(curr_ctx->L, "HID");
    lua_pushstring(curr_ctx->L, hid);
    lua_settable(curr_ctx->L, -3);
}

/*
 * Creates a key, value pair for the uid of the current
 * namespace node.
 */
void
push_uid(struct context *curr_ctx, const char *uid)
{
    lua_pushstring(curr_ctx->L, "UID");
    lua_pushstring(curr_ctx->L, uid);
    lua_settable(curr_ctx->L, -3);
}

/*
 * Generates a Lua table for the current namespace node and
 * sets it into our root table.
 */
void
push_node(struct context *curr_ctx, const char *path, UINT32 level,
    ACPI_HANDLE handle)
{
    if (path != NULL) {
    	lua_newtable(curr_ctx->L);

    	push_path(curr_ctx, path);
    	push_level(curr_ctx, level);

    	ACPI_DEVICE_INFO *info = NULL;
    	if (ACPI_SUCCESS(AcpiGetObjectInfo(handle, &info))) {
    	    if (info->Valid & ACPI_VALID_HID) {
    	        push_hid(curr_ctx, info->HardwareId.String);
    	    }
    	    
    	    if (info->Valid & ACPI_VALID_UID) {
    	        push_uid(curr_ctx, info->UniqueId.String);
    	    }
    	    
    	    ACPI_FREE(info);
    	}

    	lua_rawseti(curr_ctx->L, curr_ctx->tbl, curr_ctx->idx);
    	curr_ctx->idx++;
    }
}
