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

/* set the paths we want */
#undef LUA_ROOT
#undef LUA_LDIR
#undef LUA_CDIR
#define LUA_ROOT       LUA_PATH "/" LUA_VDIR "/"
#define LUA_LDIR       LUA_ROOT "share/"
#define LUA_CDIR       LUA_ROOT "lib/"

/* Simplify this, since it's always an int */
#undef lua_numbertointeger
#define lua_numbertointeger(n,p) \
      (*(p) = (LUA_INTEGER)(n), 1)

/* Define our number type by brute force, but first undo the default defines */
#undef panic
#undef LUA_NUMBER
#undef l_floatatt
#undef LUAI_UACNUMBER
#undef LUA_NUMBER_FRMLEN
#undef LUA_NUMBER_FMT
#undef l_mathop
#undef lua_str2number
#undef lua_getlocaledecpoint

#undef LUA_FLOAT_TYPE
#define LUA_FLOAT_TYPE LUA_FLOAT_INT64

#include "lstd.h"

#include <machine/_inttypes.h>

#define panic lua_panic
/* Hack to use int64 as the LUA_NUMBER from ZFS code, kinda */

#define LUA_NUMBER	int64_t

#define l_floatatt(n)		(LUA_FLOAT_INT_HACK_##n)
#define LUA_FLOAT_INT_HACK_MANT_DIG	32
#define LUA_FLOAT_INT_HACK_MAX_10_EXP	32

#define LUAI_UACNUMBER	int64_t

#define LUA_NUMBER_FRMLEN	""
#define LUA_NUMBER_FMT		"%" PRId64

#define l_mathop(x)		(lstd_ ## x)

#define lua_str2number(s,p)	strtoll((s), (p), 0)

#define lua_getlocaledecpoint()		'.'

/* Better buffer size */
#undef LUAL_BUFFERSIZE
#define LUAL_BUFFERSIZE		128

/* Maxalign can't reference double */
#undef LUAI_MAXALIGN
#define LUAI_MAXALIGN  lua_Number n; void *s; lua_Integer i; long l

#define LUA_AVOID_FLOAT
