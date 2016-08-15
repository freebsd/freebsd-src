/*-
 * Copyright (c) 2012-2016 Robert N. M. Watson
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
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <machine/cpuregs.h>
#include <machine/sysarch.h>

#include <cheri/cheri.h>
#include <cheri/cheric.h>
#include <cheri/cheri_enter.h>
#include <cheri/cheri_system.h>
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

struct sandbox_class	*cheritest_classp;
struct sandbox_object	*cheritest_objectp;

struct cheri_object cheritest, cheritest2;

void
test_sandbox_abort(const struct cheri_test *ctp __unused)
{
	register_t v;

	v = invoke_abort();
	if (v == -2)
		cheritest_success();
	else
		cheritest_failure_errx("Sandbox did not abort()");
}

void
test_sandbox_cs_calloc(const struct cheri_test *ctp __unused)
{
	register_t v;

	v = invoke_system_calloc();
	if (v < 0)
		cheritest_failure_errx("Sandbox returned %jd", (intmax_t)v);
	else
		cheritest_success();
}

static int
allow_syscall(int *retp __unused, __capability int *errno __unused)
{

	return (0);
}

static int
deny_syscall(int *retp, __capability int *stub_errno)
{

	*retp = -1;
	*stub_errno = ENOSYS;

	return (-1);
}

void
test_sandbox_cs_clock_gettime(const struct cheri_test *ctp __unused)
{
	register_t v;

	syscall_checks[SYS_clock_gettime] = (syscall_check_t)allow_syscall;

	v = invoke_clock_gettime();
	if (v < 0)
		cheritest_failure_errx("Sandbox returned %jd", (intmax_t)v);
	else
		cheritest_success();
}

void
test_sandbox_cs_clock_gettime_default(const struct cheri_test *ctp __unused)
{
	register_t v;

#ifdef CHERIERRNO_LINKS
	cherierrno = 0;
#endif
	v = invoke_clock_gettime();
	if (v != -1)
		cheritest_failure_errx("Sandbox returned %jd", (intmax_t)v);
#ifdef CHERIERRNO_LINKS
	else if (cherierrno != 0)
		cheritest_failure_errx(
		    "Sandbox returned -1, but set cherierrno to %d",
		    cherierrno);
#endif
	else
		cheritest_success();
}

void
test_sandbox_cs_clock_gettime_deny(const struct cheri_test *ctp __unused)
{
	register_t v;

	syscall_checks[SYS_clock_gettime] = (syscall_check_t)deny_syscall;

#ifdef CHERIERRNO_LINKS
	cherierrno = 0;
#endif
	v = invoke_clock_gettime();
	if (v != -1)
		cheritest_failure_errx("Sandbox returned %jd", (intmax_t)v);
#ifdef CHERIERRNO_LINKS
	else if (cherierrno != 0)
		cheritest_failure_errx(
		    "Sandbox returned -1, but set cherierrno to %d",
		    cherierrno);
#endif
	else
		cheritest_success();
}

void
test_sandbox_cs_helloworld(const struct cheri_test *ctp __unused)
{
	register_t v;

	v = invoke_cheri_system_helloworld();
	if (v < 0)
		cheritest_failure_errx("Sandbox returned %jd", (intmax_t)v);
	else
		cheritest_success();
}

void
test_sandbox_cs_putchar(const struct cheri_test *ctp __unused)
{
	register_t v;

	v = invoke_cheri_system_putchar();
	if (v < 0)
		cheritest_failure_errx("Sandbox returned %jd", (intmax_t)v);
	else
		cheritest_success();
}

void
test_sandbox_cs_puts(const struct cheri_test *ctp __unused)
{
	register_t v;

	v = invoke_cheri_system_puts();
	if (v < 0)
		cheritest_failure_errx("Sandbox returned %jd", (intmax_t)v);
	else
		cheritest_success();
}

void
test_sandbox_printf(const struct cheri_test *ctp __unused)
{
	register_t v;

	v = invoke_cheri_system_printf();
	if (v < 0)
		cheritest_failure_errx("Sandbox returned %jd", (intmax_t)v);
	else
		cheritest_success();
}

void
test_sandbox_malloc(const struct cheri_test *ctp __unused)
{
	register_t v;

	v = invoke_malloc();
	if (v < 0)
		cheritest_failure_errx("Sandbox returned %jd", (intmax_t)v);
	else
		cheritest_success();
}

static char string_to_md5[] = "hello world";
static char string_md5[] = "5eb63bbbe01eeed093cb22bb8f5acdc3";

void
test_sandbox_md5_ccall(const struct cheri_test *ctp __unused, int class)
{
	__capability void *md5cap, *bufcap;
	char buf[33];

	md5cap = cheri_ptrperm(string_to_md5, sizeof(string_to_md5),
	    CHERI_PERM_LOAD);
	bufcap = cheri_ptrperm(buf, sizeof(buf), CHERI_PERM_STORE);

	switch (class) {
	case 1:
		invoke_md5(strlen(string_to_md5), md5cap, bufcap);
		break;
	case 2:
		call_invoke_md5(strlen(string_to_md5), md5cap, bufcap);
		break;
	default:
		cheritest_failure_errx("invalid class %d", class);
		break;
	}

	buf[32] = '\0';
	if (strcmp(buf, string_md5) != 0)
		cheritest_failure_errx(
		    "Incorrect MD5 checksum returned from sandbox ('%s')",
		    buf);
	cheritest_success();
}

static register_t cheritest_libcheri_userfn_handler(
    struct cheri_object system_object,
    register_t methodnum,
    register_t a0, register_t a1, register_t a2, register_t a3,
    register_t a4, register_t a5, register_t a6, register_t a7,
    __capability void *c3, __capability void *c4, __capability void *c5,
    __capability void *c6, __capability void *c7)
    __attribute__((cheri_ccall)); /* XXXRW: Will be ccheri_ccallee. */

void
test_sandbox_spin(const struct cheri_test *ctp __unused)
{
	register_t v;

	/*
	 * Test will never terminate on it's own.  We set an alarm to
	 * trigger a signal.
	 */
	alarm(10);

	v = invoke_spin();

	alarm(0);

	if (v != CHERITEST_SANDBOX_UNWOUND)
		cheritest_failure_errx(
		    "Sandbox not unwound (returned 0x%jx instead of 0x%jx)",
		    (uintmax_t)v, (uintmax_t)CHERITEST_SANDBOX_UNWOUND);
	else
		cheritest_success();
}

static register_t
cheritest_libcheri_userfn_handler(struct cheri_object system_object __unused,
    register_t methodnum,
    register_t arg,
    register_t a1 __unused, register_t a2 __unused, register_t a3 __unused,
    register_t a4 __unused, register_t a5 __unused, register_t a6 __unused,
    register_t a7 __unused,
    __capability void *c3 __unused, __capability void *c4 __unused,
    __capability void *c5 __unused, __capability void *c6 __unused,
    __capability void *c7 __unused)
{

	switch (methodnum) {
	case CHERITEST_USERFN_RETURNARG:
		return (arg);

	case CHERITEST_USERFN_GETSTACK:
		return (cheritest_libcheri_userfn_getstack());

	case CHERITEST_USERFN_SETSTACK:
		return (cheritest_libcheri_userfn_setstack(arg));

	default:
		cheritest_failure_errx("%s: unexpected method %ld", __func__,
		    methodnum);
	}
}

void
test_sandbox_userfn(const struct cheri_test *ctp __unused)
{
	register_t i, v;

	for (i = 0; i < 10; i++) {
		v = invoke_libcheri_userfn(CHERITEST_USERFN_RETURNARG, i);
		if (v != i)
			cheritest_failure_errx("Incorrect return value "
			    "0x%lx (expected 0x%lx)\n", v, i);
	}
	cheritest_success();
}

/*
 * Most tests run within a single object instantiated by
 * cheritest_libcheri_setup().  These tests perform variations on the them of
 * "create a second object and optionally do stuff with it".
 */
void
test_2sandbox_newdestroy(const struct cheri_test *ctp __unused)
{

	struct sandbox_object *sbop;

	if (sandbox_object_new(cheritest_classp, 2*1024*1024, &sbop) < 0)
		cheritest_failure_errx("sandbox_object_new() failed");
	sandbox_object_destroy(sbop);
	cheritest_success();
}

void
test_sandbox_ptrdiff(const struct cheri_test *ctp __unused)
{
	intmax_t ret;

	if ((ret = sandbox_test_ptrdiff()) != 0)
		cheritest_failure_errx("sandbox_test_ptrdiff returned %jd\n",
		    ret);
	else
		cheritest_success();
}

void
test_sandbox_varargs(const struct cheri_test *ctp __unused)
{
	intmax_t ret;

	if ((ret = sandbox_test_varargs()) != 0)
		cheritest_failure_errx("sandbox_test_varargs returned %jd\n",
		    ret);
	else
		cheritest_success();
}

void
test_sandbox_va_copy(const struct cheri_test *ctp __unused)
{
	intmax_t ret;

	if ((ret = sandbox_test_va_copy()) != 0)
		cheritest_failure_errx("sandbox_test_va_copy returned %jd\n",
		    ret);
	else
		cheritest_success();
}

int
cheritest_libcheri_setup(void)
{

	/*
	 * Prepare CHERI objects representing stdin, stdout, and /dev/zero.
	 */
	if (cheri_fd_new(STDIN_FILENO, &stdin_fd_object) < 0)
		err(EX_OSFILE, "cheri_fd_new: stdin");
	if (cheri_fd_new(STDOUT_FILENO, &stdout_fd_object) < 0)
		err(EX_OSFILE, "cheri_fd_new: stdout");
	zero_fd = open("/dev/zero", O_RDWR);
	if (zero_fd < 0)
		err(EX_OSFILE, "open: /dev/zero");
	if (cheri_fd_new(zero_fd, &zero_fd_object) < 0)
		err(EX_OSFILE, "cheri_fd_new: /dev/zero");

	if (sandbox_class_new("/usr/libexec/cheritest-helper",
	    4*1024*1024, &cheritest_classp) < 0)
		return (-1);
	if (sandbox_object_new(cheritest_classp, 2*1024*1024, &cheritest_objectp) < 0)
		return (-1);
	cheritest = sandbox_object_getobject(cheritest_objectp);
	cheritest2 = sandbox_object_getobject(cheritest_objectp);

	cheri_system_user_register_fn(&cheritest_libcheri_userfn_handler);

	return (0);
}

void
cheritest_libcheri_destroy(void)
{

	sandbox_object_destroy(cheritest_objectp);
	sandbox_class_destroy(cheritest_classp);
	cheri_fd_destroy(zero_fd_object);
	close(zero_fd);
}
