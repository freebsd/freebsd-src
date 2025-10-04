/*
 * yaml.c, LibYAML binding for Lua
 * Written by Andrew Danforth, 2009
 *
 * Copyright (C) 2014-2022 Gary V. Vaughan
 * Copyright (C) 2009 Andrew Danforth
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Portions of this software were inspired by Perl's YAML::LibYAML module by
 * Ingy döt Net <ingy@cpan.org>
 *
 */

#include <string.h>
#include <stdlib.h>

#include <lualib.h>

#include "lyaml.h"

#define MYNAME		"yaml"
#define MYVERSION	MYNAME " library for " LUA_VERSION " / " VERSION

#define LYAML__STR_1(_s)	(#_s + 1)
#define LYAML_STR_1(_s)		LYAML__STR_1(_s)

static const luaL_Reg R[] =
{
#define MENTRY(_s) {LYAML_STR_1(_s), (_s)}
	MENTRY( Pemitter	),
	MENTRY( Pparser		),
	MENTRY( Pscanner	),
#undef MENTRY
	{NULL, NULL}
};

LUALIB_API int
luaopen_yaml (lua_State *L)
{
   parser_init (L);
   scanner_init (L);

   luaL_register(L, "yaml", R);

   lua_pushliteral(L, MYVERSION);
   lua_setfield(L, -2, "version");

   return 1;
}
