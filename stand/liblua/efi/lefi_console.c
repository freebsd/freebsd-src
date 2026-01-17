#include <efi.h>
#include <efilib.h>
#include <lauxlib.h>
#include <lua.h>

#include "lefi_console.h"
#include "lefi_handles.h"
#include "lefi_protocols.h"
#include "lefi_utils.h"

static lua_efi_protocol *
lefi_check_conout(lua_State *L)
{
	lua_efi_protocol *p = luaL_checkudata(L, 1, "efi.protocol.conout");

	if (p->iface == NULL)
		luaL_error(L, "protocol interface is NULL");

	return p;
}

static int
lua_conout_reset(lua_State *L)
{
	lua_efi_protocol *p = lefi_check_conout(L);
	int extended = lua_toboolean(L, 2);
	EFI_STATUS status;

	SIMPLE_TEXT_OUTPUT_INTERFACE *conout = (SIMPLE_TEXT_OUTPUT_INTERFACE *)
						   p->iface;

	status = conout->Reset(conout, extended ? TRUE : FALSE);
	if (EFI_ERROR(status))
		return lefi_push_err(L, 1, "Reset failed", status);
	return 0;
}

static int
lua_conout_output_string(lua_State *L)
{
	lua_efi_protocol *p = lefi_check_conout(L);
	CHAR16 *str;
	EFI_STATUS status;

	str = (CHAR16 *)lua_touserdata(L, 2);
	if (str == NULL)
		return luaL_error(L, "expected CHAR16*");

	SIMPLE_TEXT_OUTPUT_INTERFACE *conout = (SIMPLE_TEXT_OUTPUT_INTERFACE *)
						   p->iface;

	status = conout->OutputString(conout, str);
	if (EFI_ERROR(status))
		return lefi_push_err(L, 1, "OutputString failed", status);

	return 0;
}

static int
lua_conout_clear_screen(lua_State *L)
{
	lua_efi_protocol *p = lefi_check_conout(L);

	SIMPLE_TEXT_OUTPUT_INTERFACE *conout = (SIMPLE_TEXT_OUTPUT_INTERFACE *)
						   p->iface;

	conout->ClearScreen(conout);
	return 0;
}

static int
lua_conout_set_cursor(lua_State *L)
{
	lua_efi_protocol *p = lefi_check_conout(L);
	UINTN col = (UINTN)luaL_checkinteger(L, 2);
	UINTN row = (UINTN)luaL_checkinteger(L, 3);
	EFI_STATUS status;

	SIMPLE_TEXT_OUTPUT_INTERFACE *conout = (SIMPLE_TEXT_OUTPUT_INTERFACE *)
						   p->iface;

	status = conout->SetCursorPosition(conout, col, row);
	if (EFI_ERROR(status))
		return lefi_push_err(L, 1, "SetCursorPosition failed", status);

	return 0;
}

static int
lua_conout_enable_cursor(lua_State *L)
{
	lua_efi_protocol *p = lefi_check_conout(L);
	int visible = lua_toboolean(L, 2);
	EFI_STATUS status;

	SIMPLE_TEXT_OUTPUT_INTERFACE *conout = (SIMPLE_TEXT_OUTPUT_INTERFACE *)
						   p->iface;

	status = conout->EnableCursor(conout, visible ? TRUE : FALSE);
	if (EFI_ERROR(status))
		return lefi_push_err(L, 1, "EnableCursor failed", status);

	return 0;
}

static int
lua_conout_query_mode(lua_State *L)
{
	lua_efi_protocol *p = lefi_check_conout(L);
	UINTN mode = (UINTN)luaL_checkinteger(L, 2);
	UINTN cols, rows;
	EFI_STATUS status;

	SIMPLE_TEXT_OUTPUT_INTERFACE *conout = (SIMPLE_TEXT_OUTPUT_INTERFACE *)
						   p->iface;

	status = conout->QueryMode(conout, mode, &cols, &rows);
	if (EFI_ERROR(status))
		return lefi_push_err(L, 1, "QueryMode failed", status);

	lua_pushinteger(L, (lua_Integer)cols);
	lua_pushinteger(L, (lua_Integer)rows);
	return 2;
}

static int
lua_conout_set_mode(lua_State *L)
{
	lua_efi_protocol *p = lefi_check_conout(L);
	UINTN mode = (UINTN)luaL_checkinteger(L, 2);
	EFI_STATUS status;

	SIMPLE_TEXT_OUTPUT_INTERFACE *conout = (SIMPLE_TEXT_OUTPUT_INTERFACE *)
						   p->iface;

	status = conout->SetMode(conout, mode);
	if (EFI_ERROR(status))
		return lefi_push_err(L, 1, "SetMode failed", status);

	return 0;
}

static int
lua_conout_set_attribute(lua_State *L)
{
	lua_efi_protocol *p = lefi_check_conout(L);
	UINTN attr = (UINTN)luaL_checkinteger(L, 2);
	EFI_STATUS status;

	SIMPLE_TEXT_OUTPUT_INTERFACE *conout = (SIMPLE_TEXT_OUTPUT_INTERFACE *)
						   p->iface;

	status = conout->SetAttribute(conout, attr);
	if (EFI_ERROR(status))
		return lefi_push_err(L, 1, "SetAttribute failed", status);

	return 0;
}

static int
lua_conout_mode(lua_State *L)
{
	lua_efi_protocol *p = lefi_check_conout(L);
	SIMPLE_TEXT_OUTPUT_INTERFACE *conout = (SIMPLE_TEXT_OUTPUT_INTERFACE *)
						   p->iface;

	SIMPLE_TEXT_OUTPUT_MODE *m = conout->Mode;
	if (m == NULL)
		return lefi_push_err(L, 1, "console mode unavailable",
		    EFI_UNSUPPORTED);

	lua_newtable(L);

	lua_pushinteger(L, m->MaxMode);
	lua_setfield(L, -2, "max_mode");

	lua_pushinteger(L, m->Mode);
	lua_setfield(L, -2, "mode");

	lua_pushinteger(L, m->Attribute);
	lua_setfield(L, -2, "attribute");

	lua_pushinteger(L, m->CursorColumn);
	lua_setfield(L, -2, "cursor_col");

	lua_pushinteger(L, m->CursorRow);
	lua_setfield(L, -2, "cursor_row");

	lua_pushboolean(L, m->CursorVisible);
	lua_setfield(L, -2, "cursor_visible");

	return 1;
}

const luaL_Reg console_out_methods[] = { { "output_string",
					     lua_conout_output_string },
	{ "reset", lua_conout_reset },
	{ "clear_screen", lua_conout_clear_screen },
	{ "set_cursor", lua_conout_set_cursor },
	{ "enable_cursor", lua_conout_enable_cursor },
	{ "query_mode", lua_conout_query_mode },
	{ "set_mode", lua_conout_set_mode },
	{ "set_attribute", lua_conout_set_attribute },
	{ "mode", lua_conout_mode }, { NULL, NULL } };
