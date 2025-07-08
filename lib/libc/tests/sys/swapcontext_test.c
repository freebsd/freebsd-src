/*-
 * Copyright (c) 2025 Raptor Computing Systems, LLC
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <errno.h>

#include <atf-c.h>

#define STACK_SIZE	(64ull << 10)

static volatile int callback_reached = 0;

static ucontext_t uctx_save, uctx_switch;

static void swapcontext_callback()
{
	// Increment callback reached variable
	// If this is called multiple times, we will fail the test
	// If this is not called at all, we will fail the test
	callback_reached++;
}

ATF_TC(swapcontext_basic);
ATF_TC_HEAD(swapcontext_basic, tc)
{
	atf_tc_set_md_var(tc, "descr",
		"Verify basic functionality of swapcontext");
}

ATF_TC_BODY(swapcontext_basic, tc)
{
	char *stack;
        int res;

	stack = malloc(STACK_SIZE);
	ATF_REQUIRE_MSG(stack != NULL, "malloc failed: %s", strerror(errno));
	res = getcontext(&uctx_switch);
	ATF_REQUIRE_MSG(res == 0, "getcontext failed: %s", strerror(errno));

	uctx_switch.uc_stack.ss_sp = stack;
	uctx_switch.uc_stack.ss_size = STACK_SIZE;
	uctx_switch.uc_link = &uctx_save;
	makecontext(&uctx_switch, swapcontext_callback, 0);

	res = swapcontext(&uctx_save, &uctx_switch);

        ATF_REQUIRE_MSG(res == 0, "swapcontext failed: %s", strerror(errno));
        ATF_REQUIRE_MSG(callback_reached == 1,
		"callback failed, reached %d times", callback_reached);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, swapcontext_basic);

	return (atf_no_error());
}

