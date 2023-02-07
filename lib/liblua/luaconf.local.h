/*
 * Copyright (c) 2023, Netflix, Inc
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

/*
 * We need to always define this. For the boot loader, we use it. For flua
 * we don't, but it needs to be defined to keep some ifdefs happy.
 */
#define LUA_FLOAT_INT64		4

#define LUA_USE_POSIX
#ifndef BOOTSTRAPPING
#define LUA_USE_DLOPEN
#endif

#undef LUA_ROOT
#undef LUA_LDIR
#undef LUA_CDIR
#define LUA_ROOT	"/usr/"
#define LUA_LDIR	LUA_ROOT "share/flua/"
#define LUA_CDIR	LUA_ROOT "lib/flua/"
