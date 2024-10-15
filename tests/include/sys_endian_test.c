/*-
 * Copyright (c) 2021 M. Warner Losh <imp@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/endian.h>

#include <atf-c.h>

ATF_TC(sys_endian);
ATF_TC_HEAD(sys_endian, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test swapping macros in <byteswap.h>");
}

ATF_TC_BODY(sys_endian, tc)
{
	/* FreeBSD sys/endian.h doesn't define the {__,}bswap_{16,32,64} */
#ifdef __bswap_16
	atf_tc_fail_nonfatal("__bswap_16 defined");
#endif
#ifdef bswap_16
	atf_tc_fail_nonfatal("bswap_16 defined");
#endif
#ifdef __bswap_32
	atf_tc_fail_nonfatal("__bswap_32 defined");
#endif
#ifdef bswap_32
	atf_tc_fail_nonfatal("bswap_32 defined");
#endif
#ifdef __bswap_64
	atf_tc_fail_nonfatal("__bswap_64 defined");
#endif
#ifdef bswap_64
	atf_tc_fail_nonfatal("bswap_64 defined");
#endif

	/* FreeBSD sys/endian.h does define bswap{16,32,64} */
#ifndef bswap16
	atf_tc_fail_nonfatal("bswap16 not defined");
#endif
#ifndef bswap32
	atf_tc_fail_nonfatal("bswap32 not defined");
#endif
#ifndef bswap64
	atf_tc_fail_nonfatal("bswap64 not defined");
#endif

	/*
	 * FreeBSD defines with one underscore
	 * We don't test for two since that doesn't interfere
	 */
#ifndef _BIG_ENDIAN
	atf_tc_fail_nonfatal("_BIG_ENDIAN not defined");
#endif
#ifndef _LITTLE_ENDIAN
	atf_tc_fail_nonfatal("_LITTLE_ENDIAN not defined");
#endif
#ifndef _PDP_ENDIAN
	atf_tc_fail_nonfatal("_PDP_ENDIAN not defined");
#endif
#ifndef _BYTE_ORDER
	atf_tc_fail_nonfatal("_BYTE_ORDER not defined");
#endif

	/* order to host */
#ifdef _BYTE_ORDER
#if _BYTE_ORDER == _BIG_ENDIAN
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
#ifdef _BYTE_ORDER
#if _BYTE_ORDER == _BIG_ENDIAN
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

	ATF_TP_ADD_TC(tp, sys_endian);

	return atf_no_error();
}
