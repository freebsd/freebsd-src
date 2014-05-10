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
#include <sys/sysctl.h>
#include <sys/time.h>

#include <machine/cheri.h>
#include <machine/cheric.h>
#include <machine/cpuregs.h>

#include <cheri/cheri_fd.h>
#include <cheri/sandbox.h>

#include <cheritest-helper.h>
#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "cheritest.h"

static struct sandbox_class	*cheritest_classp;
static struct sandbox_object	*cheritest_objectp;

static int zero_fd = -1;
static struct cheri_object zero_fd_object;

void
cheritest_invoke_fd_op(int op)
{
	register_t v;

	v = sandbox_object_cinvoke(cheritest_objectp, op, 0, 0, 0, 0, 0, 0, 0,
	    sandbox_object_getsystemobject(cheritest_objectp).co_codecap,
	    sandbox_object_getsystemobject(cheritest_objectp).co_datacap,
	    cheri_zerocap(), cheri_zerocap(),
	    zero_fd_object.co_codecap, zero_fd_object.co_datacap,
	    cheri_zerocap(), cheri_zerocap());
	printf("%s: sandbox returned %jd\n", __func__, (intmax_t)v);
}

void
cheritest_revoke_fd(void)
{

	cheri_fd_revoke(zero_fd_object);
	printf("/dev/zero fd_object revoked\n");
}

void
cheritest_invoke_simple_op(int op)
{
	register_t v;

	/*
	 * Test must be done in 10 seconds or less: not the ideal way to do
	 * this, as we'd rather time it out in the parent, I think, but works
	 * fine in practice.
	 */
	alarm(10);

	v = sandbox_object_cinvoke(cheritest_objectp, op, 0, 0, 0, 0, 0, 0, 0,
	    sandbox_object_getsystemobject(cheritest_objectp).co_codecap,
	    sandbox_object_getsystemobject(cheritest_objectp).co_datacap,
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(),
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap());
	printf("%s: sandbox returned %jd\n", __func__, (intmax_t)v);
}

void
cheritest_invoke_syscall(void)
{
	size_t len;
	int old, new;

	/*
	 * Track whether or not the number of system-call violations increases
	 * as a result of triggering a system call in a sandbox.  Note that
	 * this isn't really authoritative (nor in the strictest sense
	 * correct), as we can race with other threads that trigger
	 * violations, but it's still a useful test case.
	 */
	len = sizeof(old);
	if (sysctlbyname("security.cheri.syscall_violations", &old, &len,
	    NULL, 0) < 0)
		err(EX_OSERR, "security.cheri.syscall_violations");

	cheritest_invoke_simple_op(CHERITEST_HELPER_OP_SYSCALL);

	len = sizeof(new);
	if (sysctlbyname("security.cheri.syscall_violations", &new, &len,
	    NULL, 0) < 0)
		err(EX_OSERR, "security.cheri.syscall_violations");
	if (old == new)
		warnx("security.cheri.syscall_violations unchanged: bug?");
	else
		printf("security.cheri.syscall_violations increased as "
		    "expected\n");
}

/*
 * XXXRW: c1 and c2 were not getting properly aligned when placed in the
 * stack.  Odd.
 */
static char md5string[] = "hello world";

void
cheritest_invoke_md5(void)
{
	__capability void *md5cap, *bufcap, *cclear;
	char buf[33];
	register_t v;

	cclear = cheri_zerocap();
	md5cap = cheri_ptrperm(md5string, sizeof(md5string), CHERI_PERM_LOAD);
	bufcap = cheri_ptrperm(buf, sizeof(buf), CHERI_PERM_STORE);

	v = sandbox_object_cinvoke(cheritest_objectp, CHERITEST_HELPER_OP_MD5,
	    strlen(md5string), 0, 0, 0, 0, 0, 0,
	    sandbox_object_getsystemobject(cheritest_objectp).co_codecap,
	    sandbox_object_getsystemobject(cheritest_objectp).co_datacap,
	    md5cap, bufcap, cclear, cclear, cclear, cclear);

	printf("%s: sandbox returned %ju\n", __func__, (uintmax_t)v);
	buf[32] = '\0';
	printf("MD5 checksum of '%s' is %s\n", md5string, buf);
}

int
cheritest_libcheri_setup(void)
{

	/*
	 * Prepare a CHERI object representing /dev/zero for fd-related tests.
	 */
	zero_fd = open("/dev/zero", O_RDWR);
	if (zero_fd < 0)
		err(EX_OSFILE, "open: /dev/zero");
	if (cheri_fd_new(zero_fd, &zero_fd_object) < 0)
		err(EX_OSFILE, "cheri_fd_new: /dev/zero");

	if (sandbox_class_new("/usr/libexec/cheritest-helper.bin",
	    4*1024*1024, &cheritest_classp) < 0)
		return (-1);
	if (sandbox_object_new(cheritest_classp, &cheritest_objectp) < 0)
		return (-1);
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_MD5, "md5");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_ABORT, "abort");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_SPIN, "spin");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_CP2_BOUND, "cp2_bound");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_CP2_PERM, "cp2_perm");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_CP2_TAG, "cp2_tag");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_CP2_SEAL, "cp2_seal");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_CS_HELLOWORLD, "helloworld");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_CS_PUTS, "puts");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_PRINTF, "printf");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_VM_RFAULT, "vm_rfault");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_VM_WFAULT, "vm_wfault");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_VM_XFAULT, "vm_xfault");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_SYSCALL, "syscall");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_DIVZERO, "divzero");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_SYSCAP, "syscap");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_MALLOC, "malloc");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_CS_CLOCK_GETTIME, "clock_gettime");
	return (0);
}

void
cheritest_libcheri_destroy(void)
{

	sandbox_object_destroy(cheritest_objectp);
	sandbox_class_destroy(cheritest_classp);
}
