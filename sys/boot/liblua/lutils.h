/*-
 * Copyright (c) 2014 Pedro Souza <pedrosouza@freebsd.org>
 * All rights reserved.
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
 *
 * $FreeBSD$
 */

#include <src/lua.h>

#define lua_create() lua_newstate(lua_realloc, NULL)

int	 lua_perform(lua_State *);
int	 lua_print(lua_State *);
int	 lua_getchar(lua_State *);
int	 lua_ischar(lua_State *);
int	 lua_gets(lua_State *);
int	 lua_time(lua_State *);
int	 lua_delay(lua_State *);
int	 lua_getenv(lua_State *);
void 	*lua_realloc(void *, void *, size_t, size_t);
int	 ldo_string(lua_State *, const char *, size_t);
int	 ldo_file(lua_State *, const char *);
int	 lua_include(lua_State *);
int	 lua_openfile(lua_State *);
int	 lua_closefile(lua_State *L);
int	 lua_readfile(lua_State *L);
void	 lregister(lua_State *, const char *, const char *, int (*fptr)(lua_State *));
void	 register_utils(lua_State *);
