/*-
 * Copyright (c) 2012-2014 Robert N. M. Watson
 * Copyright (c) 2014 SRI International
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

int zero_fd = -1;
struct cheri_object stdin_fd_object, stdout_fd_object, zero_fd_object;

void
test_sandbox_fd_op(const struct cheri_test *ctp __unused, int op)
{
	register_t v;

	v = sandbox_object_cinvoke(cheritest_objectp, op, 0, 0, 0, 0, 0, 0, 0,
	    sandbox_object_getsystemobject(cheritest_objectp).co_codecap,
	    sandbox_object_getsystemobject(cheritest_objectp).co_datacap,
	    cheri_zerocap(), cheri_zerocap(),
	    zero_fd_object.co_codecap, zero_fd_object.co_datacap,
	    cheri_zerocap(), cheri_zerocap());

	/*
	 * XXXRW: Pretty soon we'll want to break this one function out into
	 * test-specific functions that have more rich definitions of
	 * 'success'.
	 */
	cheritest_success();
}

static char read_string[128];

void
test_sandbox_fd_read(const struct cheri_test *ctp)
{
	__capability char *stringc;
	register_t v;

	stringc = cheri_ptrperm(read_string, sizeof(read_string),
	    CHERI_PERM_STORE);
	v = sandbox_object_cinvoke(cheritest_objectp,
	    CHERITEST_HELPER_OP_FD_READ_C, 0, 0, 0, 0, 0, 0, 0,
	    sandbox_object_getsystemobject(cheritest_objectp).co_codecap,
	    sandbox_object_getsystemobject(cheritest_objectp).co_datacap,
	    /* data_input */ cheri_zerocap(), /* data_output */ stringc,
	    stdin_fd_object.co_codecap, stdin_fd_object.co_datacap,
	    cheri_zerocap(), cheri_zerocap());
	if (v != (ssize_t)strlen(ctp->ct_stdin_string))
		cheritest_failure_errx("invoke returned %lu; expected %d\n",
		    v, strlen(read_string));
	cheritest_success();
}

void
test_sandbox_fd_read_revoke(const struct cheri_test *ctp __unused)
{
	__capability char *stringc;
	register_t v;

	/*
	 * Essentially the same test as test_sandbox_fd_read() except that we
	 * expect not to receive input.
	 */
	cheri_fd_revoke(stdin_fd_object);
	stringc = cheri_ptrperm(read_string, sizeof(read_string),
	    CHERI_PERM_STORE);
	v = sandbox_object_cinvoke(cheritest_objectp,
	    CHERITEST_HELPER_OP_FD_READ_C, 0, 0, 0, 0, 0, 0, 0,
	    sandbox_object_getsystemobject(cheritest_objectp).co_codecap,
	    sandbox_object_getsystemobject(cheritest_objectp).co_datacap,
	    /* data_input */ cheri_zerocap(), /* data_output */ stringc,
	    stdin_fd_object.co_codecap, stdin_fd_object.co_datacap,
	    cheri_zerocap(), cheri_zerocap());
	if (v != -1)
		cheritest_failure_errx("invoke returned %lu; expected %d\n",
		    v, -1);
	cheritest_success();
}

void
test_sandbox_fd_write(const struct cheri_test *ctp __unused)
{
	__capability char *stringc;
	register_t v;

	stringc = cheri_ptrperm(ctp->ct_stdout_string,
	    strlen(ctp->ct_stdout_string), CHERI_PERM_LOAD);
	v = sandbox_object_cinvoke(cheritest_objectp,
	    CHERITEST_HELPER_OP_FD_WRITE_C, 0, 0, 0, 0, 0, 0, 0,
	    sandbox_object_getsystemobject(cheritest_objectp).co_codecap,
	    sandbox_object_getsystemobject(cheritest_objectp).co_datacap,
	    /* data_input */ stringc, /* data_output */ cheri_zerocap(),
	    stdout_fd_object.co_codecap, stdout_fd_object.co_datacap,
	    cheri_zerocap(), cheri_zerocap());
	if (v != (ssize_t)strlen(ctp->ct_stdout_string))
		cheritest_failure_errx("invoke returned %lu; expected %d\n",
		    v, strlen(ctp->ct_stdout_string));
	cheritest_success();
}

void
test_sandbox_fd_write_revoke(const struct cheri_test *ctp __unused)
{
	__capability char *stringc;
	register_t v;

	/*
	 * Essentially the same test as test_sandbox_fd_write() except that we
	 * expect to see no output.
	 */
	cheri_fd_revoke(stdout_fd_object);
	stringc = cheri_ptrperm(ctp->ct_stdout_string,
	    strlen(ctp->ct_stdout_string), CHERI_PERM_LOAD);
	v = sandbox_object_cinvoke(cheritest_objectp,
	    CHERITEST_HELPER_OP_FD_WRITE_C, 0, 0, 0, 0, 0, 0, 0,
	    sandbox_object_getsystemobject(cheritest_objectp).co_codecap,
	    sandbox_object_getsystemobject(cheritest_objectp).co_datacap,
	    /* data_input */ stringc, /* data_output */ cheri_zerocap(),
	    stdout_fd_object.co_codecap, stdout_fd_object.co_datacap,
	    cheri_zerocap(), cheri_zerocap());
	if (v != -1)
		cheritest_failure_errx("invoke returned %lu; expected %d\n",
		    v, -1);
	cheritest_success();
}
