/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 The FreeBSD Foundation
 *
 * This software was developed by Mark Johnston under sponsorship from
 * the FreeBSD Foundation.
 */

#include <sys/types.h>
#include <sys/_bitset.h>
#include <sys/bitset.h>
#include <stdio.h>
#include <stdlib.h>

#include <atf-c.h>

BITSET_DEFINE(bs256, 256);

ATF_TC_WITHOUT_HEAD(bit_foreach);
ATF_TC_BODY(bit_foreach, tc)
{
	struct bs256 bs0, bs1, bsrand;
	int setc, clrc, i;

#define	_BIT_FOREACH_COUNT(s, bs) do {					\
	int prev = -1;							\
	setc = clrc = 0;						\
	BIT_FOREACH_ISSET((s), i, (bs)) {				\
		ATF_REQUIRE_MSG(prev < i, "incorrect bit ordering");	\
		ATF_REQUIRE_MSG(BIT_ISSET((s), i, (bs)),		\
		    "bit %d is not set", i);				\
		setc++;							\
		prev = i;						\
	}								\
	prev = -1;							\
	BIT_FOREACH_ISCLR((s), i, (bs)) {				\
		ATF_REQUIRE_MSG(prev < i, "incorrect bit ordering");	\
		ATF_REQUIRE_MSG(!BIT_ISSET((s), i, (bs)),		\
		    "bit %d is set", i);				\
		clrc++;							\
		prev = i;						\
	}								\
} while (0)

	/*
	 * Create several bitsets, and for each one count the number
	 * of set and clear bits and make sure they match what we expect.
	 */

	BIT_FILL(256, &bs1);
	_BIT_FOREACH_COUNT(256, &bs1);
	ATF_REQUIRE_MSG(setc == 256, "incorrect set count %d", setc);
	ATF_REQUIRE_MSG(clrc == 0, "incorrect clear count %d", clrc);

	BIT_ZERO(256, &bs0);
	_BIT_FOREACH_COUNT(256, &bs0);
	ATF_REQUIRE_MSG(setc == 0, "incorrect set count %d", setc);
	ATF_REQUIRE_MSG(clrc == 256, "incorrect clear count %d", clrc);

	BIT_ZERO(256, &bsrand);
	for (i = 0; i < 256; i++)
		if (random() % 2 != 0)
			BIT_SET(256, i, &bsrand);
	_BIT_FOREACH_COUNT(256, &bsrand);
	ATF_REQUIRE_MSG(setc + clrc == 256, "incorrect counts %d, %d",
	    setc, clrc);

	/*
	 * Try to verify that we can safely clear bits in the set while
	 * iterating.
	 */
	BIT_FOREACH_ISSET(256, i, &bsrand) {
		ATF_REQUIRE(setc-- > 0);
		BIT_CLR(256, i, &bsrand);
	}
	_BIT_FOREACH_COUNT(256, &bsrand);
	ATF_REQUIRE_MSG(setc == 0, "incorrect set count %d", setc);
	ATF_REQUIRE_MSG(clrc == 256, "incorrect clear count %d", clrc);

#undef _BIT_FOREACH_COUNT
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, bit_foreach);
	return (atf_no_error());
}
