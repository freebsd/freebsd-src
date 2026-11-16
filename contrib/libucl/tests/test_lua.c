/* Copyright (c) 2026, Vsevolod Stakhov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *       * Redistributions of source code must retain the above copyright
 *         notice, this list of conditions and the following disclaimer.
 *       * Redistributions in binary form must reproduce the above copyright
 *         notice, this list of conditions and the following disclaimer in the
 *         documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Objects pushed to Lua by the binding, built only with ENABLE_LUA.
 * The values of a repeated key are a table of those values, whatever their
 * types, as the emitters write them: an array among them is one value, not
 * elements to merge with the others.
 */

#include "ucl.h"
#include "lua_ucl.h"

#include <lauxlib.h>
#include <lualib.h>

/* The checks below call the code under test, so they must run in release builds */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Lua tables of values written as nested braces, keys sorted */
static const char helpers_lua[] =
	"local ucl = require 'ucl'\n"
	"function parse_a(s)\n"
	"  local p = ucl.parser()\n"
	"  assert(p:parse_string(s))\n"
	"  return p:get_object().a\n"
	"end\n"
	"function dump(v)\n"
	"  if type(v) ~= 'table' then return tostring(v) end\n"
	"  local parts = {}\n"
	"  if #v > 0 then\n"
	"    for _, x in ipairs(v) do parts[#parts + 1] = dump(x) end\n"
	"  else\n"
	"    local keys = {}\n"
	"    for k in pairs(v) do keys[#keys + 1] = k end\n"
	"    table.sort(keys)\n"
	"    for _, k in ipairs(keys) do parts[#parts + 1] = k .. '=' .. dump(v[k]) end\n"
	"  end\n"
	"  return '{' .. table.concat(parts, ',') .. '}'\n"
	"end\n";

static lua_State *
make_state(void)
{
	lua_State *L = luaL_newstate();

	assert(L != NULL);
	luaL_openlibs(L);
	lua_getglobal(L, "package");
	lua_getfield(L, -1, "preload");
	lua_pushcfunction(L, luaopen_ucl);
	lua_setfield(L, -2, "ucl");
	lua_pop(L, 2);

	assert(luaL_dostring(L, helpers_lua) == 0);

	return L;
}

static void
check_dump(lua_State *L, const char *what, const char *expect)
{
	const char *got;

	lua_getglobal(L, "dump");
	lua_insert(L, -2);
	assert(lua_pcall(L, 1, 1, 0) == 0);
	got = lua_tostring(L, -1);
	if (got == NULL || strcmp(got, expect) != 0) {
		fprintf(stderr, "%s: got %s, expected %s\n", what,
				got != NULL ? got : "(null)", expect);
		assert(0);
	}
	lua_pop(L, 1);
}

/* A repeated key whose first value is an array was flattened into it */
static void
test_repeated_keys(lua_State *L)
{
	static const struct {
		const char *input;
		const char *expect;
	} cases[] = {
		{"a [1, 2]; a = 5;", "{{1,2},5}"},
		{"a [1, 2]; a [3];", "{{1,2},{3}}"},
		{"a = 5; a [1, 2]; a = 6;", "{5,{1,2},6}"},
		{"a { b [1]; b = 2 };", "{b={{1},2}}"},
		{"a = 5; a = 6;", "{5,6}"},
		{"a { b = 1 }; a { b = 2 };", "{{b=1},{b=2}}"},
		{"a [1, 2];", "{1,2}"},
		{"a [[1], 2];", "{{1},2}"},
	};
	unsigned int i;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		lua_getglobal(L, "parse_a");
		lua_pushstring(L, cases[i].input);
		assert(lua_pcall(L, 1, 1, 0) == 0);
		check_dump(L, cases[i].input, cases[i].expect);
	}
}

/*
 * ucl_object_push_lua_filter_nil() pushed each value of a repeated key as
 * the values from it to the last one, and left their nulls as they were
 */
static void
test_filter_nil(lua_State *L)
{
	struct ucl_parser *parser = ucl_parser_new(0);
	ucl_object_t *top;

	assert(parser != NULL);
	assert(ucl_parser_add_string(parser, "a = 5; a = null; a [null];", 0));
	top = ucl_parser_get_object(parser);
	assert(top != NULL);
	ucl_parser_free(parser);

	ucl_object_push_lua_filter_nil(L, ucl_object_lookup(top, "a"), true);
	check_dump(L, "filter_nil", "{5,false,{false}}");
	ucl_object_unref(top);
}

int main(int argc, char **argv)
{
	lua_State *L;

	(void) argc;
	(void) argv;

	L = make_state();
	test_repeated_keys(L);
	test_filter_nil(L);
	lua_close(L);

	printf("All lua tests passed\n");

	return 0;
}
