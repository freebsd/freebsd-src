/*-
 * Copyright (c) 2018 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Maya Rashish
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Test that C++ "cabsl" is usable. PR lib/50646
 */

#include <atf-c++.hpp>
#include <complex>

ATF_TEST_CASE(cabsl);
ATF_TEST_CASE_HEAD(cabsl)
{
	set_md_var("descr", "Check that cabsl is usable from C++");
}
ATF_TEST_CASE_BODY(cabsl)
{
	int sum = 0;

#ifdef __HAVE_LONG_DOUBLE
	std::complex<long double> cld(3.0,4.0);
	sum += std::abs(cld);
#endif
	std::complex<double> cd(3.0,4.0);
	sum += std::abs(cd);

	std::complex<float> cf(3.0,4.0);
	sum += std::abs(cf);

#ifdef __HAVE_LONG_DOUBLE
	ATF_REQUIRE_EQ(sum, 3*5);
#else
	ATF_REQUIRE_EQ(sum, 2*5);
#endif
}

ATF_INIT_TEST_CASES(tcs)
{
	ATF_ADD_TEST_CASE(tcs, cabsl);
}
