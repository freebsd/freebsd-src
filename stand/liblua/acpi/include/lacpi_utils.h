#pragma once

#include <lauxlib.h>
#include <lua.h>
#include <lacpi_walk.h>
#include <contrib/dev/acpica/include/acpi.h>

/***** UTILITY *****/

/* verifies lua passed over a handle on the stack */
ACPI_HANDLE lacpi_check_handle(lua_State *L, int idx);

/* destructor dispatcher */
typedef void (*acpi_destructor_t)(ACPI_OBJECT *);

/* safety check -- lua stores integers as 64bit */
ACPI_STATUS lacpi_int_to_uint32(lua_State *L, int index, UINT32 *num);

/* to convert ACPI_OBJECT_TYPE from table value */
ACPI_STATUS lacpi_infer_type(lua_State *L, int idx, UINT32 *type);

/* build error code and push it onto stack */
int lacpi_push_err(lua_State *L, const int push_nil, const char *errmsg,
    const ACPI_STATUS status);

/* extract error message relating to ACPI_STATUS */
char *lacpi_extract_status(const ACPI_STATUS status);

/***** FACTORY *****/

/* create metatable gc */
int lacpi_create_mt_gc(lua_State *L, const char *mt, lua_CFunction gc_func);

/*** ACPI_OBJECT ***/
/* build ACPI_OBJECTs */
ACPI_STATUS build_int(lua_State *L, ACPI_OBJECT *obj, int idx);
ACPI_STATUS build_str(lua_State *L, ACPI_OBJECT *obj, int idx);
ACPI_STATUS build_buff(lua_State *L, ACPI_OBJECT *obj, int idx);
ACPI_STATUS build_pkg(lua_State *L, ACPI_OBJECT *obj, int idx);
ACPI_STATUS build_acpi_obj(lua_State *L, ACPI_OBJECT *obj, UINT32 obj_type, int idx);
ACPI_STATUS build_ref(lua_State *L, ACPI_OBJECT *obj, int idx);
ACPI_STATUS build_proc(lua_State *L, ACPI_OBJECT *obj, int idx);
ACPI_STATUS build_pow(lua_State *L, ACPI_OBJECT *obj, int idx);

/* push ACPI_OBJECTs onto lua stack */
void push_int(lua_State *L, ACPI_OBJECT *obj);
void push_str(lua_State *L, ACPI_OBJECT *obj);
void push_buff(lua_State *L, ACPI_OBJECT *obj);
void push_pkg(lua_State *L, ACPI_OBJECT *obj);
void push_acpi_obj(lua_State *L, ACPI_OBJECT *obj);
void push_ref(lua_State *L, ACPI_OBJECT *obj);
void push_proc(lua_State *L, ACPI_OBJECT *obj);
void push_pow(lua_State *L, ACPI_OBJECT *obj);

/* free ACPI_OBJECTs */
void free_fake(ACPI_OBJECT *obj);
void free_str(ACPI_OBJECT *obj);
void free_buff(ACPI_OBJECT *obj);
void free_pkg(ACPI_OBJECT *obj);
void free_acpi_obj(ACPI_OBJECT *obj);
void free_acpi_objs(ACPI_OBJECT **objs, UINT32 init_count);

/*** ACPI Namespace Node ***/
void push_path(struct context *curr_ctx, const char *path);
void push_lvl(struct context *curr_ctx, UINT32 level);
void push_hid(struct context *curr_ctx, const char *hid);
void push_uid(struct context *curr_ctx, const char *uid);
void push_node(struct context *curr_ctx, const char *path, UINT32 level,
    ACPI_HANDLE handle);
