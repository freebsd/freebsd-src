/*-
 * Copyright (c) 2021 M. Warner Losh <imp@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <byteswap.h>

#include <atf-c.h>

ATF_TC(byteswap);
ATF_TC_HEAD(byteswap, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test swapping macros in <byteswap.h>");
}

ATF_TC_BODY(byteswap, tc)
{
	uint16_t ui16;
	uint32_t ui32;
	uint64_t ui64;

	/* glibc defines the {__,}bswap_{16,32,64} */
#ifndef __bswap_16
	atf_tc_fail_nonfatal("__bswap_16 not defined");
#endif
#ifndef bswap_16
	atf_tc_fail_nonfatal("bswap_16 not defined");
#endif
#ifndef __bswap_32
	atf_tc_fail_nonfatal("__bswap_32 not defined");
#endif
#ifndef bswap_32
	atf_tc_fail_nonfatal("bswap_32 not defined");
#endif
#ifndef __bswap_64
	atf_tc_fail_nonfatal("__bswap_64 not defined");
#endif
#ifndef bswap_64
	atf_tc_fail_nonfatal("bswap_64 not defined");
#endif

	/* glibc does not define bswap{16,32,64} */
#ifdef bswap16
	atf_tc_fail_nonfatal("bswap16 improperly defined");
#endif
#ifdef bswap32
	atf_tc_fail_nonfatal("bswap32 improperly defined");
#endif
#ifdef bswap64
	atf_tc_fail_nonfatal("bswap64 improperly defined");
#endif

	ui16 = 0x1234;
	ATF_REQUIRE_MSG(0x3412 == bswap_16(ui16),
	    "bswap16(%#x) != 0x3412 instead %#x\n", ui16, bswap_16(ui16));

	ui32 = 0x12345678ul;
	ATF_REQUIRE_MSG(0x78563412ul == bswap_32(ui32),
	    "bswap32(%#lx) != 0x78563412 instead %#lx\n",
	    (unsigned long)ui32, (unsigned long)bswap_32(ui32));

	ui64 = 0x123456789abcdef0ull;
	ATF_REQUIRE_MSG(0xf0debc9a78563412ull == bswap_64(ui64),
	    "bswap64(%#llx) != 0x3412 instead %#llx\n",
	    (unsigned long long)ui64, (unsigned long long)bswap_64(ui64));

}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, byteswap);

	return atf_no_error();
}
