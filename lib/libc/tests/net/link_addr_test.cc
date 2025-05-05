/*
 * Copyright (c) 2025 Lexi Winter
 *
 * SPDX-License-Identifier: ISC
 */

/*
 * Tests for link_addr() and link_ntoa().
 *
 * link_addr converts a string representing an (optionally null) interface name
 * followed by an Ethernet address into a sockaddr_dl.  The expected format is
 * "[ifname]:lladdr".  This means if ifname is not specified, the leading colon
 * is still required.
 *
 * link_ntoa does the inverse of link_addr, returning a string representation
 * of the address.
 *
 * Note that the output format of link_ntoa is not valid input for link_addr
 * since the leading colon may be omitted.  This is by design.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stddef.h>
#include <net/ethernet.h>
#include <net/if_dl.h>

#include <vector>
#include <span>
#include <format>
#include <utility>
#include <ranges>
#include <cstdint>

#include <atf-c++.hpp>

using namespace std::literals;

/*
 * Define operator== and operator<< for ether_addr so we can use them in
 * ATF_EXPECT_EQ expressions.
 */

bool
operator==(ether_addr a, ether_addr b)
{
	return (std::ranges::equal(a.octet, b.octet));
}

std::ostream &
operator<<(std::ostream &s, ether_addr a)
{
	for (unsigned i = 0; i < ETHER_ADDR_LEN; ++i) {
		if (i > 0)
			s << ":";

		s << std::format("{:02x}", static_cast<int>(a.octet[i]));
	}

	return (s);
}

/*
 * Create a sockaddr_dl from a string using link_addr(), and ensure the
 * returned struct looks valid.
 */
sockaddr_dl
make_linkaddr(const std::string &addr)
{
	auto sdl = sockaddr_dl{};

	sdl.sdl_len = sizeof(sdl);
	::link_addr(addr.c_str(), &sdl);

	ATF_REQUIRE_EQ(AF_LINK, static_cast<int>(sdl.sdl_family));
	ATF_REQUIRE_EQ(true, sdl.sdl_len > 0);
	ATF_REQUIRE_EQ(true, sdl.sdl_nlen >= 0);

	return (sdl);
}

/*
 * Return the data stored in a sockaddr_dl as a span.
 */
std::span<const char>
data(const sockaddr_dl &sdl)
{
	// sdl_len is the entire structure, but we only want the length of the
	// data area.
	auto dlen = sdl.sdl_len - offsetof(sockaddr_dl, sdl_data);
	return {&sdl.sdl_data[0], dlen};
}

/*
 * Return the interface name stored in a sockaddr_dl as a string.
 */
std::string_view
ifname(const sockaddr_dl &sdl)
{
	auto name = data(sdl).subspan(0, sdl.sdl_nlen);
	return {name.begin(), name.end()};
}

/*
 * Return the Ethernet address stored in a sockaddr_dl as an ether_addr.
 */
ether_addr
addr(const sockaddr_dl &sdl)
{
	ether_addr ret{};
	ATF_REQUIRE_EQ(ETHER_ADDR_LEN, sdl.sdl_alen);
	std::ranges::copy(data(sdl).subspan(sdl.sdl_nlen, ETHER_ADDR_LEN),
			  &ret.octet[0]);
	return (ret);
}

/*
 * Return the link address stored in a sockaddr_dl as a span of octets.
 */
std::span<const std::uint8_t>
lladdr(const sockaddr_dl &sdl)
{
	auto data = reinterpret_cast<const uint8_t *>(LLADDR(&sdl));
	return {data, data + sdl.sdl_alen};
}


/*
 * Some sample addresses we use for testing.  Include at least one address for
 * each format we want to support.
 */

struct test_address {
	std::string input;	/* value passed to link_addr */
	std::string ntoa;	/* expected return from link_ntoa */
	ether_addr addr{};	/* expected return from link_addr */
};

std::vector<test_address> test_addresses{
	// No delimiter
	{"001122334455"s, "0.11.22.33.44.55",
	 ether_addr{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}},

	// Colon delimiter
	{"00:11:22:33:44:55"s, "0.11.22.33.44.55",
	 ether_addr{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}},

	// Dash delimiter
	{"00-11-22-33-44-55"s, "0.11.22.33.44.55",
	 ether_addr{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}},

	// Period delimiter (link_ntoa format)
	{"00.11.22.33.44.55"s, "0.11.22.33.44.55",
	 ether_addr{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}},

	// Period delimiter (Cisco format)
	{"0011.2233.4455"s, "0.11.22.33.44.55",
	 ether_addr{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}},

	// An addresses without leading zeroes.
	{"0:1:02:30:4:55"s, "0.1.2.30.4.55",
	 ether_addr{0x00, 0x01, 0x02, 0x30, 0x04, 0x55}},

	// An address with some uppercase letters.
	{"AA:B:cC:Dd:e0:1f"s, "aa.b.cc.dd.e0.1f",
	 ether_addr{0xaa, 0x0b, 0xcc, 0xdd, 0xe0, 0x1f}},

	// Addresses composed only of letters, to make sure they're not
	// confused with an interface name.

	{"aabbccddeeff"s, "aa.bb.cc.dd.ee.ff",
	 ether_addr{0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}},

	{"aa:bb:cc:dd:ee:ff"s, "aa.bb.cc.dd.ee.ff",
	 ether_addr{0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}},
};

/*
 * Test without an interface name.
 */
ATF_TEST_CASE_WITHOUT_HEAD(basic)
ATF_TEST_CASE_BODY(basic)
{
	for (const auto &ta : test_addresses) {
		// This does basic tests on the returned value.
		auto sdl = make_linkaddr(":" + ta.input);

		// Check the ifname and address.
		ATF_REQUIRE_EQ(""s, ifname(sdl));
		ATF_REQUIRE_EQ(ETHER_ADDR_LEN, static_cast<int>(sdl.sdl_alen));
		ATF_REQUIRE_EQ(ta.addr, addr(sdl));

		// Check link_ntoa returns the expected value.
		auto ntoa = std::string(::link_ntoa(&sdl));
		ATF_REQUIRE_EQ(ta.ntoa, ntoa);
	}

}

/*
 * Test with an interface name.
 */
ATF_TEST_CASE_WITHOUT_HEAD(ifname)
ATF_TEST_CASE_BODY(ifname)
{
	for (const auto &ta : test_addresses) {
		auto sdl = make_linkaddr("ix0:" + ta.input);

		ATF_REQUIRE_EQ("ix0", ifname(sdl));
		ATF_REQUIRE_EQ(ETHER_ADDR_LEN, static_cast<int>(sdl.sdl_alen));
		ATF_REQUIRE_EQ(ta.addr, addr(sdl));

		auto ntoa = std::string(::link_ntoa(&sdl));
		ATF_REQUIRE_EQ("ix0:" + ta.ntoa, ntoa);
	}

}

/*
 * Test some non-Ethernet addresses.
 */
ATF_TEST_CASE_WITHOUT_HEAD(nonether)
ATF_TEST_CASE_BODY(nonether)
{
	sockaddr_dl sdl;

	/* A short address */
	sdl = make_linkaddr(":1:23:cc");
	ATF_REQUIRE_EQ("", ifname(sdl));
	ATF_REQUIRE_EQ("1.23.cc"s, ::link_ntoa(&sdl));
	ATF_REQUIRE_EQ(3, sdl.sdl_alen);
	ATF_REQUIRE_EQ(true,
	    std::ranges::equal(std::vector{0x01u, 0x23u, 0xccu}, lladdr(sdl)));

	/* A long address */
	sdl = make_linkaddr(":1:23:cc:a:b:c:d:e:f");
	ATF_REQUIRE_EQ("", ifname(sdl));
	ATF_REQUIRE_EQ("1.23.cc.a.b.c.d.e.f"s, ::link_ntoa(&sdl));
	ATF_REQUIRE_EQ(9, sdl.sdl_alen);
	ATF_REQUIRE_EQ(true, std::ranges::equal(
	    std::vector{0x01u, 0x23u, 0xccu, 0xau, 0xbu, 0xcu, 0xdu, 0xeu, 0xfu},
	    lladdr(sdl)));
}

/*
 * Test an extremely long address which would overflow link_ntoa's internal
 * buffer.  It should handle this by truncating the output.
 * (Test for SA-16:37.libc / CVE-2016-6559.)
 */
ATF_TEST_CASE_WITHOUT_HEAD(overlong)
ATF_TEST_CASE_BODY(overlong)
{
	auto sdl = make_linkaddr(
	    ":01.02.03.04.05.06.07.08.09.0a.0b.0c.0d.0e.0f."
	    "11.12.13.14.15.16.17.18.19.1a.1b.1c.1d.1e.1f."
	    "22.22.23.24.25.26.27.28.29.2a.2b.2c.2d.2e.2f");

	ATF_REQUIRE_EQ(
	    "1.2.3.4.5.6.7.8.9.a.b.c.d.e.f.11.12.13.14.15.16.17.18.19.1a.1b."s,
	    ::link_ntoa(&sdl));
}

ATF_INIT_TEST_CASES(tcs)
{
	ATF_ADD_TEST_CASE(tcs, basic);
	ATF_ADD_TEST_CASE(tcs, ifname);
	ATF_ADD_TEST_CASE(tcs, nonether);
	ATF_ADD_TEST_CASE(tcs, overlong);
}
