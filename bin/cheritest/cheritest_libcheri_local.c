/*-
 * Copyright (c) 2016 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#if !__has_feature(capabilities)
#error "This code requires a CHERI-aware compiler"
#endif

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <machine/cpuregs.h>
#include <machine/sysarch.h>

#include <cheri/cheri.h>
#include <cheri/cheric.h>
#include <cheri/cheri_enter.h>
#include <cheri/cheri_fd.h>
#include <cheri/sandbox.h>

#include <cheritest-helper.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "cheritest.h"

/*
 * Tests that validate capability-flow policies configured by the kernel and
 * libcheri:
 *
 * 1. That local capabilities cannot be passed to, or returned from, CCall/
 * CReturn.
 *
 * 2. That, within a sandbox, local capabilities can only be stored in the
 * stack, not BSS / globals.
 */

/*
 * Test that if we pass in a global capability, the sandbox can successfully
 * store it to sandbox bss.
 */
void
test_sandbox_store_global_capability_in_bss(
    const struct cheri_test *ctp __unused)
{
	__capability void *carg;
	register_t v;

	carg = (__capability void *)&v;
	v = invoke_store_capability_in_bss(carg);
	if (v != 0)
		cheritest_failure_errx("unexpected return value (%ld)", v);
	cheritest_success();
}

/*
 * Test that if we pass in a global capability, the sandbox cannot store it
 * to sandbox bss.  Rely on the signal handler to unwind the stack.
 */
void
test_sandbox_store_local_capability_in_bss_catch(
    const struct cheri_test *ctp __unused)
{
	__capability void *carg;
	register_t v;

	carg = (__capability void *)&v;
	v = invoke_store_local_capability_in_bss(carg);
	if (v != -1)
		cheritest_failure_errx("unexpected return value (%ld)", v);
	cheritest_success();
}

/*
 * Test that if we pass in a global capability, and the sandbox recasts it to
 * be a local capability, it cannot store it to sandbox bss.  Disable the
 * signal handler.
 */
void
test_sandbox_store_local_capability_in_bss_nocatch(
    const struct cheri_test *ctp __unused)
{
	__capability void *carg;
	register_t v;

	signal_handler_clear(SIGPROT);
	carg = (__capability void *)&v;
	v = invoke_store_local_capability_in_bss(carg);
	if (v != -1)
		cheritest_failure_errx("unexpected return value (%ld)", v);
	cheritest_success();
}

/*
 * Test that if we pass in a global capability, the sandbox can successfully
 * store it to a sandbox's stack.
 */
void
test_sandbox_store_global_capability_in_stack(
    const struct cheri_test *ctp __unused)
{
	__capability void *carg;
	register_t v;

	carg = (__capability void *)&v;
	v = invoke_store_capability_in_stack(carg);
	if (v != 0)
		cheritest_failure_errx("unexpected return value (%ld)", v);
	cheritest_success();
}

/*
 * Test that if we pass in a global capability, and the sandbox recasts it to
 * be a local capability, it can success store it to a sandbox's stack.
 */
void
test_sandbox_store_local_capability_in_stack(
    const struct cheri_test *ctp __unused)
{
	__capability void *carg;
	register_t v;

	carg = (__capability void *)&v;
	v = invoke_store_local_capability_in_stack(carg);
	if (v != 0)
		cheritest_failure_errx("unexpected return value (%ld)", v);
	cheritest_success();
}

/*
 * Test that if we pass in a global capability, and the sandbox returns it,
 * that we get the same capability back.
 */
void
test_sandbox_return_global_capability(const struct cheri_test *ctp __unused)
{
	__capability void *carg, *cret = NULL;
	register_t v;

	carg = (__capability void *)&v;
	cret = invoke_return_capability(carg);
	if (cret != carg)
		cheritest_failure_errx("unexpected capability on return");
	cheritest_success();
}

/*
 * Test that if we pass in a global capability, and the sandbox recasts it to
 * be a local capability to return it, that it is not returned.  Rely on the
 * signal handler to unwind the stack.
 */
void
test_sandbox_return_local_capability_catch(
    const struct cheri_test *ctp __unused)
{
	__capability void *carg, *cret = NULL;
	register_t v;

	carg = (__capability void *)&v;
	cret = invoke_return_local_capability(carg);
	if (cret == carg)
		cheritest_failure_errx("local capability returned");
	cheritest_success();
}

/*
 * Test that if we pass in a global capability, and the sandbox recasts it to
 * be a local capability to return it, that it is not returned.  Disable the
 * signal handler.
 */
void
test_sandbox_return_local_capability_nocatch(
    const struct cheri_test *ctp __unused)
{
	__capability void *carg, *cret = NULL;
	register_t v;

	signal_handler_clear(SIGPROT);
	carg = (__capability void *)&v;
	cret = invoke_return_local_capability(carg);
	if (cret == carg)
		cheritest_failure_errx("local capability returned");
	cheritest_success();
}

/*
 * Test that if we pass a local capability to a sandbox, CCall rejects the
 * attempt.  Rely on the signal handler to terminate the test.  We don't have
 * a _nocatch variant as that aspect of the test suite only works for signals
 * delivered in sandboxes, and this signal is actually delivered before
 * entering the sandbox (i.e., before there are frames on the trusted stack
 * that permit unwinding).
 */
void
test_sandbox_pass_local_capability_arg(const struct cheri_test *ctp __unused)
{
	__capability void *carg;
	register_t v;

	carg = (__capability void *)&v;
	carg = cheri_local(carg);
	v = invoke_store_capability_in_stack(carg);
	cheritest_failure_errx("passing local capability argument succeeded");
}
