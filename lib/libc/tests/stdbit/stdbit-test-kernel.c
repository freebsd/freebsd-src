/*
 * Copyright (c) 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * test kernel for stdbit functions.
 * Requires the following macros to be defined:
 *
 * FUNCSTEM -- stem of the function name
 * SUFFIX -- type suffic
 * TYPE -- argument type
 * MKREFFUNC(ref, type) -- reference function builder
 */

#define FUNC __CONCAT(FUNCSTEM, SUFFIX)
#define REF __CONCAT(FUNCSTEM, __CONCAT(SUFFIX, _ref))

MKREFFUNC(REF, TYPE)

ATF_TC_WITHOUT_HEAD1(FUNCSTEM, SUFFIX);
ATF_TC_BODY1(FUNCSTEM, SUFFIX, tc)
{
	uintmax_t has, want;
	size_t i, j;
	TYPE value;

	/* test all single-bit patterns */
	for (i = 0; i < TYPE_WIDTH; i++) {
		value = (TYPE)1 << i;
		has = FUNC(value);
		want = REF(value);
		ATF_CHECK_EQ_MSG(has, want, "%s(%#jx) == %#jx != %#jx == %s(%#jx)",
		    __XSTRING(FUNC), (uintmax_t)value, has, want, __XSTRING(REF), (uintmax_t)value);
	}

	/* test all double-bit patterns */
	for (i = 0; i < TYPE_WIDTH; i++) {
		for (j = 0; j < i; j++) {
			value = (TYPE)1 << i | (TYPE)1 << j;
			has = FUNC(value);
			want = REF(value);
			ATF_CHECK_EQ_MSG(has, want, "%s(%#jx) == %#jx != %#jx == %s(%#jx)",
			    __XSTRING(FUNC), (uintmax_t)value, has, want, __XSTRING(REF), (uintmax_t)value);
		}
	}

	/* test all barber-pole patterns */
	value = ~(TYPE)0;
	for (i = 0; i < TYPE_WIDTH; i++) {
		has = FUNC(value);
		want = REF(value);
		ATF_CHECK_EQ_MSG(has, want, "%s(%#jx) == %#jx != %#jx == %s(%#jx)",
		    __XSTRING(FUNC), (uintmax_t)value, has, want, __XSTRING(REF), (uintmax_t)value);

		value = ~value;		
		has = FUNC(value);
		want = REF(value);
		ATF_CHECK_EQ_MSG(has, want, "%s(%#jx) == %#jx != %#jx == %s(%#jx)",
		    __XSTRING(FUNC), (uintmax_t)value, has, want, __XSTRING(REF), (uintmax_t)value);

		value = ~value << 1;
	}
}

#undef REF
#undef FUNC
