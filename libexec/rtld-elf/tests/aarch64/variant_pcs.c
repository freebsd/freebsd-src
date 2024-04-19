/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Arm Ltd
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <inttypes.h>
#include <string.h>
#include <atf-c.h>

extern uint32_t variant_pcs_test_ret[];
void variant_pcs_helper(void *);

ATF_TC_WITHOUT_HEAD(variant_gpr);
ATF_TC_BODY(variant_gpr, tc)
{
	uint64_t regs[31];

	memset(regs, 99, sizeof(regs));
	variant_pcs_helper(regs);

	ATF_REQUIRE(regs[0] == (uintptr_t)&regs[0]);
	ATF_REQUIRE(regs[30] == (uintptr_t)&variant_pcs_test_ret);

	for (uint64_t i = 1; i < 30; i++) {
		/*
		 * x16 and x17 are ilp0 and ilp1 respectively. They are used
		 * in the PLT code so are trashed, even with variant PCS.
		 */
		if (i == 16 || i == 17)
			continue;

		ATF_REQUIRE(regs[i] == i);
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, variant_gpr);

	return atf_no_error();
}
