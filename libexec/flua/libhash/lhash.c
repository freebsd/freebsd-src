/*-
 * Copyright (c) 2024 Netflix, Inc
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <lua.h>
#include "lauxlib.h"
#include "lhash.h"

#include <sha256.h>
#include <string.h>

#define SHA256_META "SHA256 meta table"
#define SHA256_DIGEST_LEN 32

/*
 * Note C++ comments indicate the before -- after state of the stack, in with a
 * similar convention to forth's ( ) comments. Lua indexes are from 1 and can be
 * read left to right (leftmost is 1). Negative are relative to the end (-1 is
 * rightmost). A '.' indicates a return value left on the stack (all values to
 * its right). Trivial functions don't do this.
 */

/*
 * Updates the digest with the new data passed in. Takes 1 argument, which
 * is converted to a string.
 */
static int
lua_sha256_update(lua_State *L)
{
	size_t len;
	const unsigned char *data;
	SHA256_CTX *ctx;

	ctx = luaL_checkudata(L, 1, SHA256_META);
	data = luaL_checklstring(L, 2, &len);
	SHA256_Update(ctx, data, len);

	lua_settop(L, 1);

	return (1);
}

/*
 * Finalizes the digest value and returns it as a 32-byte binary string. The ctx
 * is zeroed.
 */
static int
lua_sha256_digest(lua_State *L)
{
	SHA256_CTX *ctx;
	unsigned char digest[SHA256_DIGEST_LEN];

	ctx = luaL_checkudata(L, 1, SHA256_META);
	SHA256_Final(digest, ctx);
	lua_pushlstring(L, digest, sizeof(digest));

	return (1);
}

/*
 * Finalizes the digest value and returns it as a 64-byte ascii string of hex
 * numbers. The ctx is zeroed.
 */
static int
lua_sha256_hexdigest(lua_State *L)
{
	SHA256_CTX *ctx;
	char buf[SHA256_DIGEST_LEN * 2 + 1];
	unsigned char digest[SHA256_DIGEST_LEN];
	static const char hex[]="0123456789abcdef";
	int i;

	ctx = luaL_checkudata(L, 1, SHA256_META);
	SHA256_Final(digest, ctx);
	for (i = 0; i < SHA256_DIGEST_LEN; i++) {
		buf[i+i] = hex[digest[i] >> 4];
		buf[i+i+1] = hex[digest[i] & 0x0f];
	}
	buf[i+i] = '\0';

	lua_pushstring(L, buf);

	return (1);
}

/*
 * Zeros out the ctx before garbage collection. Normally this is done in
 * obj:digest or obj:hexdigest, but if not, it will be wiped here. Lua
 * manages freeing the ctx memory.
 */
static int
lua_sha256_done(lua_State *L)
{
	SHA256_CTX *ctx;

	ctx = luaL_checkudata(L, 1, SHA256_META);
	memset(ctx, 0, sizeof(*ctx));

	return (0);
}

/*
 * Create object obj which accumulates the state of the sha256 digest
 * for its contents and any subsequent obj:update call. It takes zero
 * or 1 arguments.
 */
static int
lua_sha256(lua_State *L)
{
	SHA256_CTX *ctx;
	int top;

	/* We take 0 or 1 args */
	top = lua_gettop(L);				// data -- data
	if (top > 1) {
		lua_pushnil(L);
		return (1);
	}

	ctx = lua_newuserdata(L, sizeof(*ctx));		// data -- data ctx
	SHA256_Init(ctx);
	if (top == 1) {
		size_t len;
		const unsigned char *data;

		data = luaL_checklstring(L, 1, &len);
		SHA256_Update(ctx, data, len);
	}
	luaL_setmetatable(L, SHA256_META);		// data ctx -- data ctx

	return (1);					// data . ctx
}

/*
 * Setup the metatable to manage our userdata that we create in lua_sha256. We
 * request a finalization call with __gc so we can zero out the ctx buffer so
 * that we don't leak secrets if obj:digest or obj:hexdigest aren't called.
 */
static void
register_metatable_sha256(lua_State *L)
{
	luaL_newmetatable(L, SHA256_META);		// -- meta

	lua_newtable(L);				// meta -- meta tbl
	lua_pushcfunction(L, lua_sha256_update);	// meta tbl -- meta tbl fn
	lua_setfield(L, -2, "update");			// meta tbl fn -- meta tbl
	lua_pushcfunction(L, lua_sha256_digest);	// meta tbl -- meta tbl fn
	lua_setfield(L, -2, "digest");			// meta tbl fn -- meta tbl
	lua_pushcfunction(L, lua_sha256_hexdigest);	// meta tbl -- meta tbl fn
	lua_setfield(L, -2, "hexdigest");		// meta tbl fn -- meta tbl

	/* Associate tbl with metatable */
	lua_setfield(L, -2, "__index");			// meta tbl -- meta
	lua_pushcfunction(L, lua_sha256_done);		// meta -- meta fn
	lua_setfield(L, -2, "__gc");			// meta fn -- meta

	lua_pop(L, 1);					// meta --
}

#define REG_SIMPLE(n)	{ #n, lua_ ## n }
static const struct luaL_Reg hashlib[] = {
	REG_SIMPLE(sha256),
	{ NULL, NULL },
};
#undef REG_SIMPLE

int
luaopen_hash(lua_State *L)
{
	register_metatable_sha256(L);

	luaL_newlib(L, hashlib);

	return 1;
}
