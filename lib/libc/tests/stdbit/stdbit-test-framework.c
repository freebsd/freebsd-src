/*     
 * Copyright (c) 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Test framework for stdbit functions.
 * Requires the following macros to be defined:
 *
 * FUNCSTEM -- name of the function without type suffix
 * MKREFFUNC(name, type) -- macro to generate a reference
 *   implementation of the function as a static function
 *   named name with give argument type.
 */

#include <sys/cdefs.h>
#include <atf-c.h>
#include <limits.h>
#include <stdbit.h>
#include <stdint.h>

#define ATF_TC_WITHOUT_HEAD1(stem, suffix) ATF_TC_WITHOUT_HEAD2(__CONCAT(stem, suffix))
#define ATF_TC_WITHOUT_HEAD2(case) ATF_TC_WITHOUT_HEAD(case)
#define ATF_TC_BODY1(stem, suffix, tc) ATF_TC_BODY2(__CONCAT(stem, suffix), tc)
#define ATF_TC_BODY2(case, tc) ATF_TC_BODY(case, tc)

#define SUFFIX _uc
#define TYPE unsigned char
#define TYPE_WIDTH UCHAR_WIDTH
#include "stdbit-test-kernel.c"
#undef TYPE_WIDTH
#undef TYPE
#undef SUFFIX

#define SUFFIX _us
#define TYPE unsigned short
#define TYPE_WIDTH USHRT_WIDTH
#include "stdbit-test-kernel.c"
#undef TYPE_WIDTH
#undef TYPE
#undef SUFFIX

#define SUFFIX _ui
#define TYPE unsigned int
#define TYPE_WIDTH UINT_WIDTH
#include "stdbit-test-kernel.c"
#undef TYPE_WIDTH
#undef TYPE
#undef SUFFIX

#define SUFFIX _ul
#define TYPE unsigned long
#define TYPE_WIDTH ULONG_WIDTH
#include "stdbit-test-kernel.c"
#undef TYPE_WIDTH
#undef TYPE
#undef SUFFIX

#define SUFFIX _ull
#define TYPE unsigned long long
#define TYPE_WIDTH ULLONG_WIDTH
#include "stdbit-test-kernel.c"
#undef TYPE_WIDTH
#undef TYPE
#undef SUFFIX

#define ADD_CASE(stem, suffix) ADD_CASE1(__CONCAT(stem, suffix))
#define ADD_CASE1(case) ATF_TP_ADD_TC(tp, case)

ATF_TP_ADD_TCS(tp)
{
	ADD_CASE(FUNCSTEM, _uc);
	ADD_CASE(FUNCSTEM, _us);
	ADD_CASE(FUNCSTEM, _ui);
	ADD_CASE(FUNCSTEM, _ul);
	ADD_CASE(FUNCSTEM, _ull);

	return (atf_no_error());
}
