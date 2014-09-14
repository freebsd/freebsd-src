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

struct sandbox_class	*cheritest_classp;
struct sandbox_object	*cheritest_objectp;

void
test_sandbox_simple_op(const struct cheri_test *ctp __unused, int op)
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

	/*
	 * XXXRW: Pretty soon we'll want to break this one function out into
	 * test-specific functions that have more rich definitions of
	 * 'success'.
	 */
	cheritest_success();
}

static char string_to_md5[] = "hello world";
static char string_md5[] = "5eb63bbbe01eeed093cb22bb8f5acdc3";

void
test_sandbox_md5(const struct cheri_test *ctp __unused)
{
	__capability void *md5cap, *bufcap, *cclear;
	char buf[33];
	register_t v;

	cclear = cheri_zerocap();
	md5cap = cheri_ptrperm(string_to_md5, sizeof(string_to_md5),
	    CHERI_PERM_LOAD);
	bufcap = cheri_ptrperm(buf, sizeof(buf), CHERI_PERM_STORE);

	v = sandbox_object_cinvoke(cheritest_objectp, CHERITEST_HELPER_OP_MD5,
	    0, strlen(string_to_md5), 0, 0, 0, 0, 0,
	    sandbox_object_getsystemobject(cheritest_objectp).co_codecap,
	    sandbox_object_getsystemobject(cheritest_objectp).co_datacap,
	    md5cap, bufcap, cclear, cclear, cclear, cclear);

	buf[32] = '\0';
	if (strcmp(buf, string_md5) != 0)
		cheritest_failure_errx(
		    "Incorrect MD5 checksum returned from sandbox ('%s')",
		    buf);
	cheritest_success();
}

static register_t cheritest_libcheri_userfn_handler(register_t, register_t,
    register_t, register_t, register_t, register_t, register_t, register_t,
    struct cheri_object, __capability void *, __capability void *,
    __capability void *, __capability void *, __capability void *)
    __attribute__((cheri_ccall));

static register_t
cheritest_libcheri_userfn_handler(register_t methodnum, register_t arg,
    register_t a2 __unused, register_t a3 __unused,
    register_t a4 __unused, register_t a5 __unused, register_t a6 __unused,
    register_t a7 __unused, struct cheri_object system_object __unused,
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
	__capability void *cclear;
	register_t i, v;

	cclear = cheri_zerocap();
	for (i = 0; i < 10; i++) {
		v = sandbox_object_cinvoke(cheritest_objectp,
		    CHERITEST_HELPER_LIBCHERI_USERFN,
		    CHERITEST_USERFN_RETURNARG, i, 0, 0, 0, 0, 0,
		   sandbox_object_getsystemobject(cheritest_objectp).co_codecap,
		   sandbox_object_getsystemobject(cheritest_objectp).co_datacap,
		    cclear, cclear, cclear, cclear, cclear, cclear);
		if (v != i)
			cheritest_failure_errx("Incorrect return value "
			    "0x%lx (expected 0x%lx)\n", v, i);
	}
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
	    CHERITEST_HELPER_OP_CP2_PERM_LOAD, "cp2_perm_load");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_CP2_PERM_STORE, "cp2_perm_store");
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
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_LIBCHERI_USERFN, "libcheri_fn");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_LIBCHERI_USERFN_SETSTACK,
	    "libcheri_userfn_setstack");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_SAVE_CAPABILITY_IN_HEAP,
	    "save_capability_in_heap");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_CP2_PERM_LOAD, "cp2_perm_load");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_GET_VAR_BSS, "get_var_bss");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_GET_VAR_DATA, "get_var_data");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_GET_VAR_CONSTRUCTOR, "get_var_constructor");

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
