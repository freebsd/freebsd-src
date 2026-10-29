/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kevin Bowling
 */

#include <sys/param.h>
#include <sys/mbuf.h>

#include <stdbool.h>
#include <stdint.h>

#include <atf-c.h>

ATF_TC_WITHOUT_HEAD(hashtype_ishash_tcp);
ATF_TC_BODY(hashtype_ishash_tcp, tc)
{
	static const bool expected[UINT8_MAX + 1] = {
		[M_HASHTYPE_RSS_TCP_IPV4] = true,
		[M_HASHTYPE_RSS_TCP_IPV6] = true,
		[M_HASHTYPE_RSS_TCP_IPV6_EX] = true,
		[M_HASHTYPE_RSS_TCP_IPV4 | M_HASHTYPE_INNER] = true,
		[M_HASHTYPE_RSS_TCP_IPV6 | M_HASHTYPE_INNER] = true,
		[M_HASHTYPE_RSS_TCP_IPV6_EX | M_HASHTYPE_INNER] = true,
	};
	struct mbuf m = { 0 };

	/* Only TCP four-tuple hashes qualify, including inner-header hashes. */
	for (u_int type = 0; type < nitems(expected); type++) {
		M_HASHTYPE_SET(&m, type);
		ATF_CHECK_MSG(!!M_HASHTYPE_ISHASH_TCP(&m) == expected[type],
		    "hash type %#x: expected %d", type, expected[type]);
	}
}

ATF_TC_WITHOUT_HEAD(hashtype_ishash_tcp_evaluate_once);
ATF_TC_BODY(hashtype_ishash_tcp_evaluate_once, tc)
{
	struct mbuf mbufs[3] = { 0 };
	const struct mbuf *m;

	m = mbufs;
	ATF_CHECK(!M_HASHTYPE_ISHASH_TCP(m++));
	ATF_CHECK(m == &mbufs[1]);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, hashtype_ishash_tcp);
	ATF_TP_ADD_TC(tp, hashtype_ishash_tcp_evaluate_once);

	return (atf_no_error());
}
