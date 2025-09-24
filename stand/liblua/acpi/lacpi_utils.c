#include <assert.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "lacpi_utils.h"

ACPI_STATUS build_acpi_obj(lua_State *L, ACPI_OBJECT *obj, UINT32 obj_type);
void push_acpi_obj(lua_State *L, ACPI_OBJECT *obj);
void free_acpi_obj(ACPI_OBJECT *obj);

/***** UTILITY *****/

ACPI_HANDLE
lacpi_check_handle(lua_State *L, int idx)
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

ACPI_STATUS
lacpi_int_to_uint32(lua_State *L, int index, UINT32 *num)
{
	lua_Integer temp = luaL_checkinteger(L, index);

	if (temp < 0 || temp > UINT32_MAX) {
		return AE_NUMERIC_OVERFLOW;
	}

	*num = (UINT32)temp;
	return AE_OK;
}

int
lacpi_push_err(lua_State *L, const int push_nil, const char *errmsg,
    const ACPI_STATUS status) 
{
	int stack = 0;
	
	if (push_nil) {
		lua_pushnil(L);
		++stack;
	}

	if (errmsg != NULL) {
		lua_pushstring(L, errmsg);
		++stack;

		if (status > 0) {
			char *status_msg = lacpi_extract_status(status);
			lua_pushstring(L, status_msg);
			++stack;
		}
	}
	
	return stack;
}

char *
lacpi_extract_status(const ACPI_STATUS status)
{
	switch (status) {
		case AE_TYPE:
			return "Unexpected type found";
		case AE_NULL_OBJECT:
			return "Failed to allocate memory";
		case AE_NUMERIC_OVERFLOW:
			return "Field on top of Lua stack was not UINT32";
		case AE_BAD_PARAMETER:
			return "Failed string assignment off of lua stack";
		case AE_ERROR:
		default:
			return "Unexpected error";
	}
}

/***** FACTORY *****/

/*** ACPI_OBJECT ***/

ACPI_STATUS
build_int(lua_State *L, ACPI_OBJECT *obj)
{
	obj->Type = ACPI_TYPE_INTEGER;
	
	lua_getfield(L, -1, "Integer");
	if (!lua_isinteger(L, -1)) {
		return AE_TYPE;
	}
	obj->Integer.Value = lua_tointeger(L, -1);
	lua_pop(L, 1);

	return AE_OK;
}

ACPI_STATUS
build_str(lua_State *L, ACPI_OBJECT *obj)
{
	obj->Type = ACPI_TYPE_STRING;
	
	lua_getfield(L, -1, "String");
	if (!lua_isstring(L, -1)) {
		return AE_TYPE;
	}
	size_t len;
	const char *str = lua_tolstring(L, -1, &len);
	if (str == NULL) {
		return AE_BAD_PARAMETER;
	}
	obj->String.Pointer = strdup((char *)str);
	obj->String.Length = (UINT32)len;
	lua_pop(L, 1);

	return AE_OK;
}

ACPI_STATUS
build_buff(lua_State *L, ACPI_OBJECT *obj)
{
	obj->Type = ACPI_TYPE_BUFFER;
	
	lua_getfield(L, -1, "Buffer");
	if (!lua_isstring(L, -1)) {
		return AE_TYPE;
	}
	size_t len;
	const char *str = lua_tolstring(L, -1, &len);
	if (str == NULL) {
		return AE_BAD_PARAMETER;
	}
	obj->Buffer.Pointer = strdup((char *)str);
	if (obj->Buffer.Pointer == NULL) {
		return AE_NULL_OBJECT;
	}
	obj->Buffer.Length = (UINT32)len;
	lua_pop(L, 1);

	return AE_OK;
}

ACPI_STATUS
build_pkg(lua_State *L, ACPI_OBJECT *obj)
{
	ACPI_STATUS status;
	obj->Type = ACPI_TYPE_PACKAGE;
	
	lua_getfield(L, -1, "Elements");
	obj->Package.Count = lua_rawlen(L, -1);
	obj->Package.Elements = calloc(obj->Package.Count,
	    sizeof(ACPI_OBJECT));
	if (obj->Package.Elements == NULL) {
		return AE_NULL_OBJECT;
	}
	for (size_t i = 0; i < obj->Package.Count; ++i) {
		lua_rawgeti(L, -1, i + 1);
		lua_getfield(L, -1, "obj_type");
		
		UINT32 obj_type;
		if (ACPI_FAILURE(lacpi_int_to_uint32(L, -1, &obj_type))) {
			free(obj->Package.Elements);
			return AE_NUMERIC_OVERFLOW;
		}
		lua_pop(L, 1);
		
		if (ACPI_FAILURE(status = build_acpi_obj(L,
		    &obj->Package.Elements[i], obj_type))) {
			free(obj->Package.Elements);
			return status;
		}
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	
	return AE_OK;
}

ACPI_STATUS
build_ref(lua_State *L, ACPI_OBJECT *obj)
{
	obj->Type = ACPI_TYPE_LOCAL_REFERENCE;
	
	lua_getfield(L, -1, "ActualType");
	if (ACPI_FAILURE(lacpi_int_to_uint32(L, -1,
	    &obj->Reference.ActualType))) {
		return AE_NUMERIC_OVERFLOW;
	}
	lua_pop(L, 1);

	lua_getfield(L, -1, "Handle");
	if (!lua_isuserdata(L, -1)) {
		return AE_TYPE;
	}
	ACPI_HANDLE handle = *(ACPI_HANDLE *)luaL_checkudata(L, -1, "lacpi_node");
	obj->Reference.Handle = handle;
	lua_pop(L, 1);

	return AE_OK;
}

ACPI_STATUS
build_proc(lua_State *L, ACPI_OBJECT *obj)
{
	obj->Type = ACPI_TYPE_PROCESSOR;

	lua_getfield(L, -1, "ProcID");
	if (ACPI_FAILURE(lacpi_int_to_uint32(L, -1, &obj->Processor.ProcId))) {
		return AE_NUMERIC_OVERFLOW;
	}
	lua_pop(L, 1);
	
	lua_getfield(L, -1, "PblkAddress");
	if (!lua_isinteger(L, -1)) {
		return AE_TYPE;
	}
	obj->Processor.PblkAddress = (ACPI_IO_ADDRESS)lua_tointeger(L, -1);
	lua_pop(L, 1);
	
	lua_getfield(L, -1, "PblkLength");
	if (ACPI_FAILURE(lacpi_int_to_uint32(L, -1,
	    &obj->Processor.PblkLength))) {
		return AE_TYPE;
	}
	lua_pop(L, 1);

	return AE_OK;
}

ACPI_STATUS
build_pow(lua_State *L, ACPI_OBJECT *obj)
{
	obj->Type = ACPI_TYPE_POWER;

	lua_getfield(L, -1, "SystemLevel");
	if (ACPI_FAILURE(lacpi_int_to_uint32(L, -1,
	    &obj->PowerResource.SystemLevel))) {
		return AE_TYPE;
	}
	lua_pop(L, 1);

	lua_getfield(L, -1, "ResourceOrder");
	if (ACPI_FAILURE(lacpi_int_to_uint32(L, -1,
	    &obj->PowerResource.ResourceOrder))) {
		return AE_TYPE;
	}
	lua_pop(L, 1);

	return AE_OK;
}

ACPI_STATUS
build_acpi_obj(lua_State *L, ACPI_OBJECT *obj, UINT32 obj_type)
{
	switch(obj_type) {
		case ACPI_TYPE_INTEGER:
			return build_int(L, obj);
		case ACPI_TYPE_STRING:
			return build_str(L, obj);
		case ACPI_TYPE_BUFFER:
			return build_buff(L, obj);
		case ACPI_TYPE_PACKAGE:
			return build_pkg(L, obj);
		case ACPI_TYPE_LOCAL_REFERENCE:
			return build_ref(L, obj);
		case ACPI_TYPE_PROCESSOR:
			return build_proc(L, obj);
		case ACPI_TYPE_POWER:
			return build_pow(L, obj);
		default:
			return AE_ERROR;
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
