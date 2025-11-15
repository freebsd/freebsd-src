/*
 * SPDX-License-Identifier: ISC
 *
 * Copyright (c) 2025 Lexi Winter <ivy@FreeBSD.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Tests for inet_net_pton() and inet_net_ntop().
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ranges>
#include <string>
#include <vector>

#include <atf-c++.hpp>

using namespace std::literals;

/*
 * inet_net_ntop() and inet_net_pton() for IPv4.
 */
ATF_TEST_CASE_WITHOUT_HEAD(inet_net_inet4)
ATF_TEST_CASE_BODY(inet_net_inet4)
{
	/*
	 * Define a list of addresses we want to check.  Each address is passed
	 * to inet_net_pton() to convert it to an in_addr, then we convert the
	 * in_addr back to a string and compare it with the expected value.  We
	 * want to test over-long prefixes here (such as 10.0.0.1/8), so we also
	 * specify what the result is expected to be.
	 */

	struct test_addr {
		std::string input;
		int bits;
		std::string output;
	};

	auto test_addrs = std::vector<test_addr>{
		// Simple prefixes that fall on octet boundaries.
		{ "10.0.0.0/8",		8,	"10/8" },
		{ "10.1.0.0/16",	16,	"10.1/16" },
		{ "10.1.2.0/24",	24,	"10.1.2/24" },
		{ "10.1.2.3/32",	32,	"10.1.2.3/32" },

		// Simple prefixes with the short-form address.
		{ "10/8",		8,	"10/8" },
		{ "10.1/16",		16,	"10.1/16" },
		{ "10.1.2/24",		24,	"10.1.2/24" },

		// A prefix that doesn't fall on an octet boundary.
		{ "10.1.64/18",		18,	"10.1.64/18" },

		// An overlong prefix with bits that aren't part of the prefix.
		{ "10.0.0.1/8",		8,	"10/8" },
	};

	for (auto const &addr: test_addrs) {
		/*
		 * Convert the input string to an in_addr + bits, and make
		 * sure the result produces the number of bits we expected.
		 */

		auto in = in_addr{};
		auto bits = inet_net_pton(AF_INET, addr.input.c_str(),
		    &in, sizeof(in));
		ATF_REQUIRE(bits != -1);
		ATF_REQUIRE_EQ(bits, addr.bits);

		/*
		 * Convert the in_addr back to a string
		 */

		/*
		 * XXX: Should there be a constant for the size of the result
		 * buffer?  For now, use ADDRSTRLEN + 3 ("/32") + 1 (NUL).
		 *
		 * Fill the buffer with 'Z', so we can check the result was
		 * properly terminated.
		 */
		auto strbuf = std::vector<char>(INET_ADDRSTRLEN + 3 + 1, 'Z');
		auto ret = inet_net_ntop(AF_INET, &in, bits,
		    strbuf.data(), strbuf.size());
		ATF_REQUIRE(ret != NULL);
		ATF_REQUIRE_EQ(ret, strbuf.data());

		/* Make sure the result was NUL-terminated and find the NUL */
		ATF_REQUIRE(strbuf.size() >= 1);
		auto end = std::ranges::find(strbuf, '\0');
		ATF_REQUIRE(end != strbuf.end());

		/*
		 * Check the result matches what we expect.  Use a temporary
		 * string here instead of std::ranges::equal because this
		 * means ATF can print the mismatch.
		 */
		auto str = std::string(std::ranges::begin(strbuf), end);
		ATF_REQUIRE_EQ(str, addr.output);
	}
}

/*
 * inet_net_ntop() and inet_net_pton() for IPv6.
 */
ATF_TEST_CASE_WITHOUT_HEAD(inet_net_inet6)
ATF_TEST_CASE_BODY(inet_net_inet6)
{
	/*
	 * Define a list of addresses we want to check.  Each address is
	 * passed to inet_net_pton() to convert it to an in6_addr, then we
	 * convert the in6_addr back to a string and compare it with the
	 * expected value.  We want to test over-long prefixes here (such
	 * as 2001:db8::1/32), so we also specify what the result is
	 * expected to be.
	 */

	struct test_addr {
		std::string input;
		int bits;
		std::string output;
	};

	auto test_addrs = std::vector<test_addr>{
		// A prefix with a trailing ::
		{ "2001:db8::/32",	32,	"2001:db8::/32" },

		// A prefix with a leading ::.  Note that the output is
		// different from the input because inet_ntop() renders
		// this prefix with an IPv4 suffix for legacy reasons.
		{ "::ffff:0:0/96",	96,	"::ffff:0.0.0.0/96" },

		// The same prefix but with the IPv4 legacy form as input.
		{ "::ffff:0.0.0.0/96",	96,	"::ffff:0.0.0.0/96" },

		// A prefix with an infix ::.
		{ "2001:db8::1/128",	128,	"2001:db8::1/128" },

		// A prefix with bits set which are outside the prefix;
		// these should be silently ignored.
		{ "2001:db8:1:1:1:1:1:1/32", 32, "2001:db8::/32" },

		// As above but with infix ::.
		{ "2001:db8::1/32",	32,	"2001:db8::/32" },

		// A prefix with only ::, commonly used to represent the
		// entire address space.
		{ "::/0",		0,	"::/0" },

		// A single address with no ::.
		{ "2001:db8:1:1:1:1:1:1/128", 128, "2001:db8:1:1:1:1:1:1/128" },

		// A prefix with no ::.
		{ "2001:db8:1:1:0:0:0:0/64", 64, "2001:db8:1:1::/64" },

		// A prefix which isn't on a 16-bit boundary.
		{ "2001:db8:c000::/56",	56,	"2001:db8:c000::/56" },

		// A prefix which isn't on a nibble boundary.
		{ "2001:db8:c100::/57",	57,	"2001:db8:c100::/57" },

		// An address without a prefix length, which should be treated
		// as a /128.
		{ "2001:db8::",		128,	"2001:db8::/128" },
		{ "2001:db8::1",	128,	"2001:db8::1/128" },

		// Test vectors provided in PR bin/289198.
		{ "fe80::1/64",		64,	"fe80::/64" },
		{ "fe80::f000:74ff:fe54:bed2/64",
					64,	"fe80::/64" },
		{ "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/64",
					64,	"ffff:ffff:ffff:ffff::/64" },
		{ "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", 128,
		    "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/128" },
	};

	for (auto const &addr: test_addrs) {
		/*
		 * Convert the input string to an in6_addr + bits, and make
		 * sure the result produces the number of bits we expected.
		 */

		auto in6 = in6_addr{};
		errno = 0;
		auto bits = inet_net_pton(AF_INET6, addr.input.c_str(),
		    &in6, sizeof(in6));
		ATF_REQUIRE(bits != -1);
		ATF_REQUIRE_EQ(bits, addr.bits);

		/*
		 * Convert the in6_addr back to a string
		 */

		/*
		 * XXX: Should there be a constant for the size of the result
		 * buffer?  For now, use ADDRSTRLEN + 4 ("/128") + 1 (NUL).
		 *
		 * Fill the buffer with 'Z', so we can check the result was
		 * properly terminated.
		 */
		auto strbuf = std::vector<char>(INET6_ADDRSTRLEN + 4 + 1, 'Z');
		auto ret = inet_net_ntop(AF_INET6, &in6, bits,
		    strbuf.data(), strbuf.size());
		ATF_REQUIRE(ret != NULL);
		ATF_REQUIRE_EQ(ret, strbuf.data());

		/* Make sure the result was NUL-terminated and find the NUL */
		ATF_REQUIRE(strbuf.size() >= 1);
		auto end = std::ranges::find(strbuf, '\0');
		ATF_REQUIRE(end != strbuf.end());

		/*
		 * Check the result matches what we expect.  Use a temporary
		 * string here instead of std::ranges::equal because this
		 * means ATF can print the mismatch.
		 */
		auto str = std::string(std::ranges::begin(strbuf), end);
		ATF_REQUIRE_EQ(str, addr.output);
	}
}

ATF_TEST_CASE_WITHOUT_HEAD(inet_net_pton_invalid)
ATF_TEST_CASE_BODY(inet_net_pton_invalid)
{
	auto ret = int{};
	auto addr4 = in_addr{};
	auto str4 = "10.0.0.0"s;
	auto addr6 = in6_addr{};
	auto str6 = "2001:db8::"s;

	/* Passing an address which is too short should be an error */
	ret = inet_net_pton(AF_INET6, str6.c_str(), &addr6, sizeof(addr6) - 1);
	ATF_REQUIRE_EQ(ret, -1);

	ret = inet_net_pton(AF_INET, str4.c_str(), &addr4, sizeof(addr4) - 1);
	ATF_REQUIRE_EQ(ret, -1);

	/* Test some generally invalid addresses. */
	auto invalid4 = std::vector<std::string>{
		// Prefix length too big
		"10.0.0.0/33",
		// Prefix length is negative
		"10.0.0.0/-1",
		// Prefix length is not a number
		"10.0.0.0/foo",
		// Input is not a network prefix
		"this is not an IP address",
	};

	for (auto const &addr: invalid4) {
		auto ret = inet_net_pton(AF_INET, addr.c_str(), &addr4,
		    sizeof(addr4));
		ATF_REQUIRE_EQ(ret, -1);
	}

	auto invalid6 = std::vector<std::string>{
		// Prefix length too big
		"2001:db8::/129",
		// Prefix length is negative
		"2001:db8::/-1",
		// Prefix length is not a number
		"2001:db8::/foo",
		// Input is not a network prefix
		"this is not an IP address",
	};

	for (auto const &addr: invalid6) {
		auto ret = inet_net_pton(AF_INET6, addr.c_str(), &addr6,
		    sizeof(addr6));
		ATF_REQUIRE_EQ(ret, -1);
	}
}

ATF_TEST_CASE_WITHOUT_HEAD(inet_net_ntop_invalid)
ATF_TEST_CASE_BODY(inet_net_ntop_invalid)
{
	auto addr4 = in_addr{};
	auto addr6 = in6_addr{};
	auto strbuf = std::vector<char>(INET6_ADDRSTRLEN + 4 + 1);

	/*
	 * Passing a buffer which is too small should not overrun the buffer.
	 * Test this by initialising the buffer to 'Z', and only providing
	 * part of it to the function.
	 */

	std::ranges::fill(strbuf, 'Z');
	auto ret = inet_net_ntop(AF_INET6, &addr6, 128, strbuf.data(), 1);
	ATF_REQUIRE_EQ(ret, nullptr);
	ATF_REQUIRE_EQ(strbuf[1], 'Z');

	std::ranges::fill(strbuf, 'Z');
	ret = inet_net_ntop(AF_INET, &addr4, 32, strbuf.data(), 1);
	ATF_REQUIRE_EQ(ret, nullptr);
	ATF_REQUIRE_EQ(strbuf[1], 'Z');

	/* Check that invalid prefix lengths return an error */

	ret = inet_net_ntop(AF_INET6, &addr6, 129, strbuf.data(), strbuf.size());
	ATF_REQUIRE_EQ(ret, nullptr);
	ret = inet_net_ntop(AF_INET6, &addr6, -1, strbuf.data(), strbuf.size());
	ATF_REQUIRE_EQ(ret, nullptr);

	ret = inet_net_ntop(AF_INET, &addr4, 33, strbuf.data(), strbuf.size());
	ATF_REQUIRE_EQ(ret, nullptr);
	ret = inet_net_ntop(AF_INET, &addr4, -1, strbuf.data(), strbuf.size());
	ATF_REQUIRE_EQ(ret, nullptr);
}

ATF_INIT_TEST_CASES(tcs)
{
	ATF_ADD_TEST_CASE(tcs, inet_net_inet4);
	ATF_ADD_TEST_CASE(tcs, inet_net_inet6);
	ATF_ADD_TEST_CASE(tcs, inet_net_pton_invalid);
	ATF_ADD_TEST_CASE(tcs, inet_net_ntop_invalid);
}
