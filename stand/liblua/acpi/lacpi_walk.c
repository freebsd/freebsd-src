#include <lutils.h>
#include <contrib/dev/acpica/include/acpi.h>
#include "lacpi_utils.h"
#include "lacpi_object.h"
#include "lacpi_walk.h"

static ACPI_STATUS
lua_dump_acpi_namespace(ACPI_HANDLE handle, UINT32 level, void *ctx,
    void **retval)
{
	struct context *curr_ctx = (struct context *)ctx;
	ACPI_BUFFER path = { ACPI_ALLOCATE_BUFFER, NULL };

	if (ACPI_FAILURE(AcpiGetName(handle, ACPI_FULL_PATHNAME, &path))) {
		// XXX Continue anyway?
		return AE_OK;
	}

	push_node(curr_ctx, (char *)path.Pointer, level, handle);

	ACPI_FREE(path.Pointer);
	return AE_OK;
}

/*
 * WIP: Currently hard coded to just walk the entire tree.
 * Need to reiterate on this with strategy pattern once
 * I have tested this works.
 *
 * Needed strategies:
 * - User ability to define ACPI_OBJECT_TYPE
 * - DescendingCallback function hooks
 * - AscendingCallback function hooks
 */
static int
lAcpiWalkNamespace(lua_State *L)
{
	ACPI_STATUS status;
	ACPI_HANDLE handle = ACPI_ROOT_OBJECT;

	/* If user does not want to provide a handle, we start at root. */
	if (lua_gettop(L) < 1 || lua_isnil(L, 1)) {
		handle = ACPI_ROOT_OBJECT;
	} else {
		handle = lacpi_check_handle(L, 1);
		if (handle == NULL) {
			return lacpi_push_err(L, 1, "Invalid ACPI handle", 0);
		}
	}

	lua_newtable(L);
	/* ctx->tbl == -2 because in push_node we add a table */
	struct context ctx = { L, -2, 1 };

	if (ACPI_FAILURE(status = AcpiWalkNamespace(ACPI_TYPE_ANY, handle,
		ACPI_UINT32_MAX, lua_dump_acpi_namespace, NULL, &ctx, NULL))) {
			return lacpi_push_err(L, 1, "Failed to walk ACPI namespace", status);
	}

	return 1;
}

static const 
luaL_Reg lacpi_walk_funcs[] = {
	{ "namespace", lAcpiWalkNamespace },
	{ NULL, NULL }
};

int
luaopen_lacpi_walk(lua_State *L)
{
	luaL_newlib(L, lacpi_walk_funcs);
	return 1;
}

void
lacpi_walk_interp_ref(void)
{
}
