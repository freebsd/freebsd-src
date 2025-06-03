/*-
 * Copyright (c) 2021 M. Warner Losh <imp@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <endian.h>

#include <atf-c.h>

ATF_TC(endian);
ATF_TC_HEAD(endian, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test swapping macros in <byteswap.h>");
}

ATF_TC_BODY(endian, tc)
{
	/* glibc doesn't define the {__,}bswap_{16,32,64} */
#ifdef __bswap_16
	atf_tc_fail_nonfatal("__bswap_16 improperly defined");
#endif
#ifdef bswap_16
	atf_tc_fail_nonfatal("bswap_16 improperly defined");
#endif
#ifdef __bswap_32
	atf_tc_fail_nonfatal("__bswap_32 improperly defined");
#endif
#ifdef bswap_32
	atf_tc_fail_nonfatal("bswap_32 improperly defined");
#endif
#ifdef __bswap_64
	atf_tc_fail_nonfatal("__bswap_64 improperly defined");
#endif
#ifdef bswap_64
	atf_tc_fail_nonfatal("bswap_64 improperly defined");
#endif

	/* glibc doesn't define bswap{16,32,64} */
#ifdef bswap16
	atf_tc_fail_nonfatal("bswap16 improperly defined");
#endif
#ifdef bswap32
	atf_tc_fail_nonfatal("bswap32 improperly defined");
#endif
#ifdef bswap64
	atf_tc_fail_nonfatal("bswap64 improperly defined");
#endif

	/*
	 * glibc defines with two underscores. We don't test for only one since
	 * that doesn't interfere.
	 */
#ifndef __BIG_ENDIAN
	atf_tc_fail_nonfatal("__BIG_ENDIAN not defined");
#endif
#ifndef __LITTLE_ENDIAN
	atf_tc_fail_nonfatal("__LITTLE_ENDIAN not defined");
#endif
#ifndef __PDP_ENDIAN
	atf_tc_fail_nonfatal("__PDP_ENDIAN not defined");
#endif
#ifndef __FLOAT_WORD_ORDER
	atf_tc_fail_nonfatal("__FLOAT_WORD_ORDER not defined");
#endif
#ifndef __BYTE_ORDER
	atf_tc_fail_nonfatal("__BYTE_ORDER not defined");
#endif

	/* order to host */
#ifdef __BYTE_ORDER
#if __BYTE_ORDER == __BIG_ENDIAN
#define H16(x) be16toh(x)
#define H32(x) be32toh(x)
#define H64(x) be64toh(x)
#define O16(x) le16toh(x)
#define O32(x) le32toh(x)
#define O64(x) le64toh(x)
#else
#define H16(x) le16toh(x)
#define H32(x) le32toh(x)
#define H64(x) le64toh(x)
#define O16(x) be16toh(x)
#define O32(x) be32toh(x)
#define O64(x) be64toh(x)
#endif
#endif
	ATF_REQUIRE(H16(0x1234) == 0x1234);
	ATF_REQUIRE(H32(0x12345678ul) == 0x12345678ul);
	ATF_REQUIRE(H64(0x123456789abcdef0ull) == 0x123456789abcdef0ull);
	ATF_REQUIRE(O16(0x1234) == __bswap16(0x1234));
	ATF_REQUIRE(O32(0x12345678ul) == __bswap32(0x12345678ul));
	ATF_REQUIRE(O64(0x123456789abcdef0ull) == __bswap64(0x123456789abcdef0ull));
#undef H16
#undef H32
#undef H64
#undef O16
#undef O32
#undef O64

	/* host to order */
#ifdef __BYTE_ORDER
#if __BYTE_ORDER == __BIG_ENDIAN
#define H16(x) htobe16(x)
#define H32(x) htobe32(x)
#define H64(x) htobe64(x)
#define O16(x) htole16(x)
#define O32(x) htole32(x)
#define O64(x) htole64(x)
#else
#define H16(x) htole16(x)
#define H32(x) htole32(x)
#define H64(x) htole64(x)
#define O16(x) htobe16(x)
#define O32(x) htobe32(x)
#define O64(x) htobe64(x)
#endif
#endif
	ATF_REQUIRE(H16(0x1234) == 0x1234);
	ATF_REQUIRE(H32(0x12345678ul) == 0x12345678ul);
	ATF_REQUIRE(H64(0x123456789abcdef0ull) == 0x123456789abcdef0ull);
	ATF_REQUIRE(O16(0x1234) == __bswap16(0x1234));
	ATF_REQUIRE(O32(0x12345678ul) == __bswap32(0x12345678ul));
	ATF_REQUIRE(O64(0x123456789abcdef0ull) == __bswap64(0x123456789abcdef0ull));
#undef H16
#undef H32
#undef H64
#undef O16
#undef O32
#undef O64
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, endian);

	return atf_no_error();
}
