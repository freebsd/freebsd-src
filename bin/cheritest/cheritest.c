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

static const struct cheri_test {
	const char	*ct_name;
	int		 ct_arg;	/* 0: ct_func; otherwise ct_func_arg. */
	void		(*ct_func)(void);
	void		(*ct_func_arg)(int);
} cheri_tests[] = {
	{ .ct_name = "test_fault_creturn",
	  .ct_func = test_fault_creturn },
	{ .ct_name = "test_nofault_ccall_creturn",
	  .ct_func = test_nofault_ccall_creturn },
	{ .ct_name = "test_nofault_ccall_nop_creturn",
	  .ct_func = test_nofault_ccall_nop_creturn },
	{ .ct_name = "copyregs",
	  .ct_func = cheritest_copyregs },
	{ .ct_name = "listregs",
	  .ct_func = cheritest_listregs },
	{ .ct_name = "invoke_abort",
	  .ct_func_arg = cheritest_invoke_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_ABORT },
	{ .ct_name = "invoke_clock_gettime",
	  .ct_func_arg = cheritest_invoke_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_CS_CLOCK_GETTIME },
	{ .ct_name = "invoke_cp2_bound",
	  .ct_func_arg = cheritest_invoke_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_CP2_BOUND },
	{ .ct_name = "invoke_cp2_perm",
	  .ct_func_arg = cheritest_invoke_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_CP2_PERM },
	{ .ct_name = "invoke_cp2_tag",
	  .ct_func_arg = cheritest_invoke_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_CP2_TAG },
	{ .ct_name = "invoke_cp2_seal",
	  .ct_func_arg = cheritest_invoke_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_CP2_SEAL },
	{ .ct_name = "invoke_divzero",
	  .ct_func_arg = cheritest_invoke_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_DIVZERO },
	{ .ct_name = "invoke_fd_fstat_c",
	  .ct_func_arg = cheritest_invoke_fd_op,
	  .ct_arg = CHERITEST_HELPER_OP_FD_FSTAT_C },
	{ .ct_name = "invoke_fd_lseek_c",
	  .ct_func_arg = cheritest_invoke_fd_op,
	  .ct_arg = CHERITEST_HELPER_OP_FD_LSEEK_C },
	{ .ct_name = "invoke_fd_read_c",
	  .ct_func_arg = cheritest_invoke_fd_op,
	  .ct_arg = CHERITEST_HELPER_OP_FD_READ_C },
	{ .ct_name = "invoke_fd_write_c",
	  .ct_func_arg = cheritest_invoke_fd_op,
	  .ct_arg = CHERITEST_HELPER_OP_FD_WRITE_C },
	{ .ct_name = "invoke_helloworld",
	  .ct_func_arg = cheritest_invoke_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_CS_HELLOWORLD },
	{ .ct_name = "invoke_md5",
	  .ct_func = cheritest_invoke_md5 },
	{ .ct_name = "invoke_malloc",
	  .ct_func_arg = cheritest_invoke_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_MALLOC },
	{ .ct_name = "invoke_printf",
	  .ct_func_arg = cheritest_invoke_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_PRINTF },
	{ .ct_name = "invoke_cs_putchar",
	  .ct_func_arg = cheritest_invoke_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_CS_PUTCHAR },
	{ .ct_name = "invoke_cs_puts",
	  .ct_func_arg = cheritest_invoke_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_CS_PUTS },
	{ .ct_name = "invoke_spin",
	  .ct_func_arg = cheritest_invoke_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_SPIN },
	{ .ct_name = "invoke_syscall",
	  .ct_func_arg = cheritest_invoke_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_SYSCALL },
	{ .ct_name = "invoke_syscap",
	  .ct_func_arg = cheritest_invoke_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_SYSCAP },
	{ .ct_name = "invoke_vm_rfault",
	  .ct_func_arg = cheritest_invoke_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_VM_RFAULT },
	{ .ct_name = "invoke_vm_wfault",
	  .ct_func_arg = cheritest_invoke_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_VM_WFAULT },
	{ .ct_name = "invoke_vm_xfault",
	  .ct_func_arg = cheritest_invoke_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_VM_XFAULT },
	{ .ct_name = "revoke_fd",
	  .ct_func = cheritest_revoke_fd },
	{ .ct_name = "sleep",
	  .ct_arg = 10,
	  .ct_func = sleep },
	{ .ct_name = "test_fault_cgetcause",
	  .ct_func = test_fault_cgetcause },
	{ .ct_name = "test_fault_overrun",
	  .ct_func = test_fault_overrun },
	{ .ct_name = "test_fault_ccheck_user_fail",
	  .ct_func = test_fault_ccheck_user_fail },
	{ .ct_name = "test_nofault_ccheck_user_pass",
	  .ct_func = test_nofault_ccheck_user_pass },
	{ .ct_name = "test_fault_read_kr1c",
	  .ct_func = test_fault_read_kr1c },
	{ .ct_name = "test_fault_read_kr2c",
	  .ct_func = test_fault_read_kr2c },
	{ .ct_name = "test_fault_read_kcc",
	  .ct_func = test_fault_read_kcc },
	{ .ct_name = "test_fault_read_kdc",
	  .ct_func = test_fault_read_kdc },
	{ .ct_name = "test_fault_read_epcc",
	  .ct_func = test_fault_read_epcc },
};
static const u_int cheri_tests_len = sizeof(cheri_tests) /
	    sizeof(cheri_tests[0]);

static void
usage(void)
{
	u_int i;

	for (i = 0; i < cheri_tests_len; i++)
		fprintf(stderr, "cheritest %s\n", cheri_tests[i].ct_name);
	exit(EX_USAGE);
}

int
main(__unused int argc, __unused char *argv[])
{
	int i, opt;
	u_int t;

	while ((opt = getopt(argc, argv, "")) != -1) {
		switch (opt) {
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc == 0)
		usage();

	cheritest_libcheri_setup();
	for (i = 0; i < argc; i++) {
		for (t = 0; t < cheri_tests_len; t++) {
			if (strcmp(argv[i], cheri_tests[t].ct_name) == 0)
				break;
		}
		if (t == cheri_tests_len)
			usage();
		if (cheri_tests[t].ct_arg != 0)
			cheri_tests[t].ct_func_arg(cheri_tests[t].ct_arg);
		else
			cheri_tests[t].ct_func();
	}
	cheritest_libcheri_destroy();
	exit(EX_OK);
}
