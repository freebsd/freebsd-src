/*-
 * Copyright (c) 2014-2015 Robert N. M. Watson
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

#include <machine/cheri.h>
#include <machine/cheric.h>
#include <machine/cpuregs.h>
#include <machine/sysarch.h>

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

void
test_sandbox_var_bss(const struct cheri_test *ctp __unused)
{
	register_t v;

	v = invoke_get_var_bss();
	if (v != CHERITEST_VALUE_BSS)
		cheritest_failure_errx(".bss returned 0x%lx (expected 0x%x)",
		    v, CHERITEST_VALUE_BSS);
	cheritest_success();
}

void
test_sandbox_var_data(const struct cheri_test *ctp __unused)
{
	register_t v;

	v = invoke_get_var_data();
	if (v != CHERITEST_VALUE_DATA)
		cheritest_failure_errx(".data returned 0x%lx (expected 0x%x)",
		    v, CHERITEST_VALUE_DATA);
	cheritest_success();

}


void
test_sandbox_var_data_getset(const struct cheri_test *ctp __unused)
{
	register_t v;


	/* Set data to zero. */
	invoke_set_var_data(0);

	/* Read back data and ensure zero. */
	v = invoke_get_var_data();
	if (v != 0)
		cheritest_failure_errx(".data set to 0 but returned %lu\n", v);

	/* Set data to one. */
	invoke_set_var_data(1);

	/* Read back data and ensure one. */
	v = invoke_get_var_data();
	if (v != 1)
		cheritest_failure_errx(".data set to 1 but returned %lu\n", v);
	cheritest_success();
}

void
test_2sandbox_var_data_getset(const struct cheri_test *ctp __unused)
{
	struct sandbox_object *sbop;
	register_t v;

	if (sandbox_object_new(cheritest_classp, 2*1024*1024, &sbop) < 0)
		cheritest_failure_errx("sandbox_object_new() failed");

	/*
	 * Set global data in the default object to '1', and in the additional
	 * object to '2'.  Read both back and ensure there has been no
	 * confusion on this topic.
	 */
	invoke_set_var_data(1);
	invoke_set_var_data_cap(sandbox_object_getobject(sbop), 2);
	v = invoke_get_var_data();
	if (v != 1)
		cheritest_failure_errx(
		    "default sandbox object: set 1, got back %u", 1);
	v = invoke_get_var_data_cap(sandbox_object_getobject(sbop));
	if (v != 2)
		cheritest_failure_errx(
		    "additional sandbox object: set 2, got back %u", 2);
	sandbox_object_destroy(sbop);
	cheritest_success();
}

void
test_sandbox_var_constructor(const struct cheri_test *ctp __unused)
{
	register_t v;

	v = invoke_get_var_constructor();
	if (v != CHERITEST_VALUE_CONSTRUCTOR)
		cheritest_failure_errx(
		    "Constructor returned 0x%lx (expected 0x%x)",
		    v, CHERITEST_VALUE_CONSTRUCTOR);
	cheritest_success();
}
