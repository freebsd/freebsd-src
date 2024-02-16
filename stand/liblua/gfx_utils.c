/*-
 * Copyright (c) 2024 Netflix, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/* Copied from a file that likely shoulve have had this at the top */
/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2020 Toomas Soome
 * Copyright 2020 RackTop Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "lua.h"
#include "lauxlib.h"
#include "lutils.h"
#include <gfx_fb.h>
#include <pnglite.h>

/*
 * put image using terminal coordinates.
 */
static int
lua_term_putimage(lua_State *L)
{
	const char *name;
	png_t png;
	uint32_t x1, y1, x2, y2, f;
	int nargs, ret = 0, error;

	nargs = lua_gettop(L);
	if (nargs != 6) {
		lua_pushboolean(L, 0);
		return 1;
	}

	name = luaL_checkstring(L, 1);
	x1 = luaL_checknumber(L, 2);
	y1 = luaL_checknumber(L, 3);
	x2 = luaL_checknumber(L, 4);
	y2 = luaL_checknumber(L, 5);
	f = luaL_checknumber(L, 6);

	x1 = gfx_state.tg_origin.tp_col + x1 * gfx_state.tg_font.vf_width;
	y1 = gfx_state.tg_origin.tp_row + y1 * gfx_state.tg_font.vf_height;
	if (x2 != 0) {
		x2 = gfx_state.tg_origin.tp_col +
		    x2 * gfx_state.tg_font.vf_width;
	}
	if (y2 != 0) {
		y2 = gfx_state.tg_origin.tp_row +
		    y2 * gfx_state.tg_font.vf_height;
	}

	if ((error = png_open(&png, name)) != PNG_NO_ERROR) {
		if (f & FL_PUTIMAGE_DEBUG)
			printf("%s\n", png_error_string(error));
	} else {
		if (gfx_fb_putimage(&png, x1, y1, x2, y2, f) == 0)
			ret = 1;
		(void) png_close(&png);
	}
	lua_pushboolean(L, ret);
	return 1;
}

static int
lua_fb_putimage(lua_State *L)
{
	const char *name;
	png_t png;
	uint32_t x1, y1, x2, y2, f;
	int nargs, ret = 0, error;

	nargs = lua_gettop(L);
	if (nargs != 6) {
		lua_pushboolean(L, 0);
		return 1;
	}

	name = luaL_checkstring(L, 1);
	x1 = luaL_checknumber(L, 2);
	y1 = luaL_checknumber(L, 3);
	x2 = luaL_checknumber(L, 4);
	y2 = luaL_checknumber(L, 5);
	f = luaL_checknumber(L, 6);

	if ((error = png_open(&png, name)) != PNG_NO_ERROR) {
		if (f & FL_PUTIMAGE_DEBUG)
			printf("%s\n", png_error_string(error));
	} else {
		if (gfx_fb_putimage(&png, x1, y1, x2, y2, f) == 0)
			ret = 1;
		(void) png_close(&png);
	}
	lua_pushboolean(L, ret);
	return 1;
}

static int
lua_fb_setpixel(lua_State *L)
{
	uint32_t x, y;
	int nargs;

	nargs = lua_gettop(L);
	if (nargs != 2) {
		lua_pushnil(L);
		return 1;
	}

	x = luaL_checknumber(L, 1);
	y = luaL_checknumber(L, 2);
        gfx_fb_setpixel(x, y);
	return 0;
}

static int
lua_fb_line(lua_State *L)
{
	uint32_t x0, y0, x1, y1, wd;
	int nargs;

	nargs = lua_gettop(L);
	if (nargs != 5) {
		lua_pushnil(L);
		return 1;
	}

	x0 = luaL_checknumber(L, 1);
	y0 = luaL_checknumber(L, 2);
	x1 = luaL_checknumber(L, 3);
	y1 = luaL_checknumber(L, 4);
	wd = luaL_checknumber(L, 5);
        gfx_fb_line(x0, y0, x1, y1, wd);
	return 0;
}

static int
lua_fb_bezier(lua_State *L)
{
	uint32_t x0, y0, x1, y1, x2, y2, width;
	int nargs;

	nargs = lua_gettop(L);
	if (nargs != 7) {
		lua_pushnil(L);
		return 1;
	}

	x0 = luaL_checknumber(L, 1);
	y0 = luaL_checknumber(L, 2);
	x1 = luaL_checknumber(L, 3);
	y1 = luaL_checknumber(L, 4);
	x2 = luaL_checknumber(L, 5);
	y2 = luaL_checknumber(L, 6);
	width = luaL_checknumber(L, 7);
        gfx_fb_bezier(x0, y0, x1, y1, x2, y2, width);
	return 0;
}

static int
lua_fb_drawrect(lua_State *L)
{
	uint32_t x0, y0, x1, y1, fill;
	int nargs;

	nargs = lua_gettop(L);
	if (nargs != 5) {
		lua_pushnil(L);
		return 1;
	}

	x0 = luaL_checknumber(L, 1);
	y0 = luaL_checknumber(L, 2);
	x1 = luaL_checknumber(L, 3);
	y1 = luaL_checknumber(L, 4);
	fill = luaL_checknumber(L, 5);
        gfx_fb_drawrect(x0, y0, x1, y1, fill);
	return 0;
}

static int
lua_term_drawrect(lua_State *L)
{
	uint32_t x0, y0, x1, y1;
	int nargs;

	nargs = lua_gettop(L);
	if (nargs != 4) {
		lua_pushnil(L);
		return 1;
	}

	x0 = luaL_checknumber(L, 1);
	y0 = luaL_checknumber(L, 2);
	x1 = luaL_checknumber(L, 3);
	y1 = luaL_checknumber(L, 4);
        gfx_term_drawrect(x0, y0, x1, y1);
	return 0;
}

#define REG_SIMPLE(n)	{ #n, lua_ ## n }
static const struct luaL_Reg gfxlib[] = {
	REG_SIMPLE(fb_bezier),
	REG_SIMPLE(fb_drawrect),
	REG_SIMPLE(fb_line),
	REG_SIMPLE(fb_putimage),
	REG_SIMPLE(fb_setpixel),
	REG_SIMPLE(term_drawrect),
	REG_SIMPLE(term_putimage),
	{ NULL, NULL },
};

int
luaopen_gfx(lua_State *L)
{
	luaL_newlib(L, gfxlib);
	return 1;
}

void
gfx_interp_md(void)
{
}
