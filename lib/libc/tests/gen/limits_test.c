/*
 * Copyright (c) 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: FreeBSD-2-Clause
 */

#include <assert.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <wchar.h>

#include <atf-c.h>

/* if this file builds, the unit tests have passed */
#define CHECK_STYPE(type, TYPE)						\
	static_assert(sizeof(type) * CHAR_BIT == TYPE ## _WIDTH,	\
		__XSTRING(TYPE) "_WIDTH wrongly defined");		\
	static_assert((1ULL << (TYPE ## _WIDTH - 1)) - 1 == TYPE ## _MAX, \
		__XSTRING(TYPE) "_MAX wrongly defined");		\
	static_assert(TYPE ## _MIN == -TYPE ## _MAX - 1, \
		__XSTRING(TYPE) "_MIN wrongly defined")
#define CHECK_UTYPE(type, TYPE)						\
	static_assert(sizeof(type) * CHAR_BIT == TYPE ## _WIDTH,	\
		__XSTRING(TYPE) "_WIDTH wrongly defined");		\
	static_assert((type)~0ULL == TYPE ## _MAX,			\
		__XSTRING(TYPE) "_MAX wrongly defined");

/* primitive types */
#ifdef __CHAR_UNSIGNED__
CHECK_UTYPE(char, CHAR);
#else
CHECK_STYPE(char, CHAR);
#endif

CHECK_STYPE(signed char, SCHAR);
CHECK_STYPE(short, SHRT);
CHECK_STYPE(int, INT);
CHECK_STYPE(long, LONG);
CHECK_STYPE(long long, LLONG);

CHECK_UTYPE(unsigned char, UCHAR);
CHECK_UTYPE(unsigned short, USHRT);
CHECK_UTYPE(unsigned int, UINT);
CHECK_UTYPE(unsigned long, ULONG);
CHECK_UTYPE(unsigned long long, ULLONG);

/* fixed-width types */
CHECK_STYPE(int8_t, INT8);
CHECK_STYPE(int16_t, INT16);
CHECK_STYPE(int32_t, INT32);
CHECK_STYPE(int64_t, INT64);

CHECK_UTYPE(uint8_t, UINT8);
CHECK_UTYPE(uint16_t, UINT16);
CHECK_UTYPE(uint32_t, UINT32);
CHECK_UTYPE(uint64_t, UINT64);

CHECK_STYPE(int_least8_t, INT_LEAST8);
CHECK_STYPE(int_least16_t, INT_LEAST16);
CHECK_STYPE(int_least32_t, INT_LEAST32);
CHECK_STYPE(int_least64_t, INT_LEAST64);

CHECK_UTYPE(uint_least8_t, UINT_LEAST8);
CHECK_UTYPE(uint_least16_t, UINT_LEAST16);
CHECK_UTYPE(uint_least32_t, UINT_LEAST32);
CHECK_UTYPE(uint_least64_t, UINT_LEAST64);

CHECK_STYPE(int_fast8_t, INT_FAST8);
CHECK_STYPE(int_fast16_t, INT_FAST16);
CHECK_STYPE(int_fast32_t, INT_FAST32);
CHECK_STYPE(int_fast64_t, INT_FAST64);

CHECK_UTYPE(uint_fast8_t, UINT_FAST8);
CHECK_UTYPE(uint_fast16_t, UINT_FAST16);
CHECK_UTYPE(uint_fast32_t, UINT_FAST32);
CHECK_UTYPE(uint_fast64_t, UINT_FAST64);

/* other types */
#if WCHAR_MIN == 0
CHECK_UTYPE(wchar_t, WCHAR);
#else
CHECK_STYPE(wchar_t, WCHAR);
#endif
CHECK_STYPE(intmax_t, INTMAX);
CHECK_STYPE(intptr_t, INTPTR);
CHECK_STYPE(ptrdiff_t, PTRDIFF);
CHECK_STYPE(wint_t, WINT);
CHECK_STYPE(sig_atomic_t, SIG_ATOMIC);

CHECK_UTYPE(uintmax_t, UINTMAX);
CHECK_UTYPE(uintptr_t, UINTPTR);
CHECK_UTYPE(size_t, SIZE);

/* dummy */
ATF_TP_ADD_TCS(tp)
{
	(void)tp;

	return (atf_no_error());
}
