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
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/ucontext.h>
#include <sys/wait.h>

#include <machine/cheri.h>
#include <machine/cheric.h>
#include <machine/cpuregs.h>
#include <machine/frame.h>
#include <machine/trap.h>

#include <cheri/cheri_fd.h>
#include <cheri/sandbox.h>

#include <cheritest-helper.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "cheritest.h"

#define	max(x, y)	((x) > (y) ? (x) : (y))

static const struct cheri_test cheri_tests[] = {
	/*
	 * Exercise CHERI functions without an expectation of a signal.
	 */
	{ .ct_name = "test_copyregs",
	  .ct_desc = "Exercise CP2 register assignments",
	  .ct_func = test_copyregs },

	{ .ct_name = "test_listregs",
	  .ct_desc = "Print out a list of CP2 registers and values",
	  .ct_func = test_listregs,
	  .ct_flags = CT_FLAG_STDOUT_IGNORE },

	/*
	 * Capability manipulation and use tests that sometimes generate
	 * signals.
	 */
	/* XXXRW: Check this CP2 exception code number. */
	{ .ct_name = "test_fault_cgetcause",
	  .ct_desc = "Ensure CGetCause is unavailable in userspace",
	  .ct_func = test_fault_cgetcause,
	  .ct_flags = CT_FLAG_SIGNAL | CT_FLAG_MIPS_EXCCODE |
		    CT_FLAG_CP2_EXCCODE,
	  .ct_signum = SIGPROT,
	  .ct_mips_exccode = T_C2E,
	  .ct_cp2_exccode = CHERI_EXCCODE_ACCESS_EPCC },

	{ .ct_name = "test_fault_bounds",
	  .ct_desc = "Exercise capability bounds check failure",
	  .ct_func = test_fault_bounds,
	  .ct_flags = CT_FLAG_SIGNAL | CT_FLAG_MIPS_EXCCODE |
		    CT_FLAG_CP2_EXCCODE,
	  .ct_signum = SIGPROT,
	  .ct_mips_exccode = T_C2E,
	  .ct_cp2_exccode = CHERI_EXCCODE_LENGTH },

	{ .ct_name = "test_fault_perm_load",
	  .ct_desc = "Exercise capability load permission failure",
	  .ct_func = test_fault_perm_load,
	  .ct_flags = CT_FLAG_SIGNAL | CT_FLAG_MIPS_EXCCODE |
		    CT_FLAG_CP2_EXCCODE,
	  .ct_signum = SIGPROT,
	  .ct_mips_exccode = T_C2E,
	  .ct_cp2_exccode = CHERI_EXCCODE_PERM_LOAD },

	{ .ct_name = "test_nofault_perm_load",
	  .ct_desc = "Exercise capability load permission success",
	  .ct_func = test_nofault_perm_load },

	{ .ct_name = "test_fault_perm_store",
	  .ct_desc = "Exercise capability store permission failure",
	  .ct_func = test_fault_perm_store,
	  .ct_flags = CT_FLAG_SIGNAL | CT_FLAG_MIPS_EXCCODE |
		    CT_FLAG_CP2_EXCCODE,
	  .ct_signum = SIGPROT,
	  .ct_mips_exccode = T_C2E,
	  .ct_cp2_exccode = CHERI_EXCCODE_PERM_STORE },

	{ .ct_name = "test_nofault_perm_store",
	  .ct_desc = "Exercise capability store permission success",
	  .ct_func = test_nofault_perm_store },

	{ .ct_name = "test_fault_tag",
	  .ct_desc = "Store via untagged capability",
	  .ct_func = test_fault_tag,
	  .ct_flags = CT_FLAG_SIGNAL | CT_FLAG_MIPS_EXCCODE |
		    CT_FLAG_CP2_EXCCODE,
	  .ct_signum = SIGPROT,
	  .ct_mips_exccode = T_C2E,
	  .ct_cp2_exccode = CHERI_EXCCODE_TAG },

	{ .ct_name = "test_fault_ccheck_user_fail",
	  .ct_desc = "Exercise CCheckPerm failure",
	  .ct_func = test_fault_ccheck_user_fail,
	  .ct_flags = CT_FLAG_SIGNAL | CT_FLAG_MIPS_EXCCODE |
		    CT_FLAG_CP2_EXCCODE,
	  .ct_signum = SIGPROT,
	  .ct_mips_exccode = T_C2E,
	  .ct_cp2_exccode = CHERI_EXCCODE_USER_PERM },

	{ .ct_name = "test_nofault_ccheck_user_pass",
	  .ct_desc = "Exercise CCheckPerm success",
	  .ct_func = test_nofault_ccheck_user_pass },

	{ .ct_name = "test_fault_read_kr1c",
	  .ct_desc = "Ensure KR1C is unavailable in userspace",
	  .ct_func = test_fault_read_kr1c,
	  .ct_flags = CT_FLAG_SIGNAL | CT_FLAG_MIPS_EXCCODE |
		    CT_FLAG_CP2_EXCCODE,
	  .ct_signum = SIGPROT,
	  .ct_mips_exccode = T_C2E,
	  .ct_cp2_exccode = CHERI_EXCCODE_ACCESS_KR1C },

	{ .ct_name = "test_fault_read_kr2c",
	  .ct_desc = "Ensure KR2C is unavailable in userspace",
	  .ct_func = test_fault_read_kr2c,
	  .ct_flags = CT_FLAG_SIGNAL | CT_FLAG_MIPS_EXCCODE |
		    CT_FLAG_CP2_EXCCODE,
	  .ct_signum = SIGPROT,
	  .ct_mips_exccode = T_C2E,
	  .ct_cp2_exccode = CHERI_EXCCODE_ACCESS_KR2C },

	{ .ct_name = "test_fault_read_kcc",
	  .ct_desc = "Ensure KCC is unavailable in userspace",
	  .ct_func = test_fault_read_kcc,
	  .ct_flags = CT_FLAG_SIGNAL | CT_FLAG_MIPS_EXCCODE |
		    CT_FLAG_CP2_EXCCODE,
	  .ct_signum = SIGPROT,
	  .ct_mips_exccode = T_C2E,
	  .ct_cp2_exccode = CHERI_EXCCODE_ACCESS_KCC },

	{ .ct_name = "test_fault_read_kdc",
	  .ct_desc = "Ensure KDC is unavailable in userspace",
	  .ct_func = test_fault_read_kdc,
	  .ct_flags = CT_FLAG_SIGNAL | CT_FLAG_MIPS_EXCCODE |
		    CT_FLAG_CP2_EXCCODE,
	  .ct_signum = SIGPROT,
	  .ct_mips_exccode = T_C2E,
	  .ct_cp2_exccode = CHERI_EXCCODE_ACCESS_KDC },

	{ .ct_name = "test_fault_read_epcc",
	  .ct_desc = "Ensure EPCC is unavailable in userspace",
	  .ct_func = test_fault_read_epcc,
	  .ct_flags = CT_FLAG_SIGNAL | CT_FLAG_MIPS_EXCCODE |
		    CT_FLAG_CP2_EXCCODE,
	  .ct_signum = SIGPROT,
	  .ct_mips_exccode = T_C2E,
	  .ct_cp2_exccode = CHERI_EXCCODE_ACCESS_EPCC },

	/*
	 * Simple CCall/CReturn tests that sometimes generate signals.
	 */
	{ .ct_name = "test_fault_creturn",
	  .ct_desc = "Exercise trusted stack underflow",
	  .ct_func = test_fault_creturn,
	  .ct_flags = CT_FLAG_SIGNAL | CT_FLAG_MIPS_EXCCODE |
		    CT_FLAG_CP2_EXCCODE,
	  .ct_signum = SIGPROT,
	  .ct_mips_exccode = T_C2E,
	  .ct_cp2_exccode = CHERI_EXCCODE_RETURN },

	{ .ct_name = "test_nofault_ccall_creturn",
	  .ct_desc = "Exercise CCall/CReturn",
	  .ct_func = test_nofault_ccall_creturn },

	{ .ct_name = "test_nofault_ccall_nop_creturn",
	  .ct_desc = "Exercise CCall/NOP/NOP/NOP/CReturn",
	  .ct_func = test_nofault_ccall_nop_creturn },

	{ .ct_name = "test_nofault_ccall_dli_creturn",
	  .ct_desc = "Exercise CCall/DLI/CReturn",
	  .ct_func = test_nofault_ccall_dli_creturn },

	/*
	 * Test libcheri sandboxing -- and kernel sandbox unwind.
	 */
	{ .ct_name = "test_sandbox_abort",
	  .ct_desc = "Exercise system call in a libcheri sandbox",
	  .ct_func_arg = test_sandbox_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_ABORT },

	{ .ct_name = "test_sandbox_clock_gettime",
	  .ct_desc = "Exercise clock_gettime() in a libcheri sandbox",
	  .ct_func_arg = test_sandbox_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_CS_CLOCK_GETTIME,
	  .ct_flags = CT_FLAG_STDOUT_IGNORE },

	{ .ct_name = "test_sandbox_cp2_bound_catch",
	  .ct_desc = "Exercise sandboxed CP2 bounds-check failure; caught",
	  .ct_func = test_sandbox_cp2_bound_catch,
	  .ct_flags = CT_FLAG_SIGNAL | CT_FLAG_MIPS_EXCCODE |
		    CT_FLAG_CP2_EXCCODE,
	  .ct_signum = SIGPROT,
	  .ct_mips_exccode = T_C2E,
	  .ct_cp2_exccode = CHERI_EXCCODE_LENGTH },

	{ .ct_name = "test_sandbox_cp2_bound_nocatch",
	  .ct_desc = "Exercise sandboxed CP2 bounds-check failure; uncaught",
	  .ct_func = test_sandbox_cp2_bound_nocatch },

	{ .ct_name = "test_sandbox_cp2_perm_load_catch",
	  .ct_desc = "Exercise sandboxed CP2 load-perm-check failure; caught",
	  .ct_func = test_sandbox_cp2_perm_load_catch,
	  .ct_flags = CT_FLAG_SIGNAL | CT_FLAG_MIPS_EXCCODE |
		    CT_FLAG_CP2_EXCCODE,
	  .ct_signum = SIGPROT,
	  .ct_mips_exccode = T_C2E,
	  .ct_cp2_exccode = CHERI_EXCCODE_PERM_LOAD },

	{ .ct_name = "test_sandbox_cp2_perm_load_nocatch",
	  .ct_desc = "Exercise sandboxed CP2 load-perm-check failure; uncaught",
	  .ct_func = test_sandbox_cp2_perm_load_nocatch, },

	{ .ct_name = "test_sandbox_cp2_perm_store_catch",
	  .ct_desc = "Exercise sandboxed CP2 store-perm-check failure; caught",
	  .ct_func = test_sandbox_cp2_perm_store_catch,
	  .ct_flags = CT_FLAG_SIGNAL | CT_FLAG_MIPS_EXCCODE |
		    CT_FLAG_CP2_EXCCODE,
	  .ct_signum = SIGPROT,
	  .ct_mips_exccode = T_C2E,
	  .ct_cp2_exccode = CHERI_EXCCODE_PERM_STORE },

	{ .ct_name = "test_sandbox_cp2_perm_store_nocatch",
	  .ct_desc = "Exercise sandboxed CP2 store-perm-check failure; uncaught",
	  .ct_func = test_sandbox_cp2_perm_store_nocatch, },

	{ .ct_name = "test_sandbox_cp2_tag_catch",
	  .ct_desc = "Exercise sandboxed CP2 tag-check failure; caught",
	  .ct_func = test_sandbox_cp2_tag_catch,
	  .ct_flags = CT_FLAG_SIGNAL | CT_FLAG_MIPS_EXCCODE |
		    CT_FLAG_CP2_EXCCODE,
	  .ct_signum = SIGPROT,
	  .ct_mips_exccode = T_C2E,
	  .ct_cp2_exccode = CHERI_EXCCODE_TAG },

	{ .ct_name = "test_sandbox_cp2_tag_nocatch",
	  .ct_desc = "Exercise sandboxed CP2 tag-check failure; uncaught",
	  .ct_func = test_sandbox_cp2_tag_nocatch, },

	{ .ct_name = "test_sandbox_cp2_seal_catch",
	  .ct_desc = "Exercise sandboxed CP2 seal failure; caught",
	  .ct_func = test_sandbox_cp2_seal_catch,
	  .ct_flags = CT_FLAG_SIGNAL | CT_FLAG_MIPS_EXCCODE |
		    CT_FLAG_CP2_EXCCODE,
	  .ct_signum = SIGPROT,
	  .ct_mips_exccode = T_C2E,
	  .ct_cp2_exccode = CHERI_EXCCODE_PERM_SEAL },

	{ .ct_name = "test_sandbox_cp2_seal_nocatch",
	  .ct_desc = "Exercise sandboxed CP2 seal failure; uncaught",
	  .ct_func = test_sandbox_cp2_seal_nocatch, },

	{ .ct_name = "test_sandbox_divzero_catch",
	  .ct_desc = "Exercise sandboxed divide-by-zero exception; caught",
	  .ct_func = test_sandbox_divzero_catch,
	  .ct_flags = CT_FLAG_SIGNAL | CT_FLAG_MIPS_EXCCODE,
	  .ct_signum = SIGEMT,
	  .ct_mips_exccode = T_TRAP },

	{ .ct_name = "test_sandbox_divzero_nocatch",
	  .ct_desc = "Exercise sandboxed divide-by-zero exception; uncaught",
	  .ct_func = test_sandbox_divzero_nocatch, },

	{ .ct_name = "test_sandbox_vm_rfault_catch",
	  .ct_desc = "Exercise sandboxed VM read fault; caught",
	  .ct_func = test_sandbox_vm_rfault_catch,
	  .ct_flags = CT_FLAG_SIGNAL | CT_FLAG_MIPS_EXCCODE,
	  .ct_signum = SIGBUS,
	  .ct_mips_exccode = T_TLB_LD_MISS },

	{ .ct_name = "test_sandbox_vm_rfault_nocatch",
	  .ct_desc = "Exercise sandboxed VM read fault; uncaught",
	  .ct_func = test_sandbox_vm_rfault_nocatch, },

	{ .ct_name = "test_sandbox_vm_wfault_catch",
	  .ct_desc = "Exercise sandboxed VM write fault; caught",
	  .ct_func = test_sandbox_vm_wfault_catch,
	  .ct_flags = CT_FLAG_SIGNAL | CT_FLAG_MIPS_EXCCODE,
	  .ct_signum = SIGBUS,
	  .ct_mips_exccode = T_TLB_ST_MISS },

	{ .ct_name = "test_sandbox_vm_wfault_nocatch",
	  .ct_desc = "Exercise sandboxed VM write fault; uncaught",
	  .ct_func = test_sandbox_vm_wfault_nocatch, },

	{ .ct_name = "test_sandbox_vm_xfault_catch",
	  .ct_desc = "Exercise sandboxed VM exec fault; caught",
	  .ct_func = test_sandbox_vm_xfault_catch,
	  .ct_flags = CT_FLAG_SIGNAL | CT_FLAG_MIPS_EXCCODE,
	  .ct_signum = SIGBUS,
	  .ct_mips_exccode = T_TLB_LD_MISS },

	{ .ct_name = "test_sandbox_vm_xfault_nocatch",
	  .ct_desc = "Exercise sandboxed VM exec fault; uncaught",
	  .ct_func = test_sandbox_vm_xfault_nocatch },

	{ .ct_name = "test_sandbox_helloworld",
	  .ct_desc = "Print 'hello world' in a libcheri sandbox",
	  .ct_func_arg = test_sandbox_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_CS_HELLOWORLD,
	  .ct_flags = CT_FLAG_STDOUT_STRING,
	  .ct_stdout_string = "hello world\n" },

	{ .ct_name = "test_sandbox_md5",
	  .ct_desc = "Generate an MD5 checksum in a libcheri sandbox",
	  .ct_func = test_sandbox_md5 },

	{ .ct_name = "test_sandbox_malloc",
	  .ct_desc = "Malloc memory in a libcheri sandbox",
	  .ct_func_arg = test_sandbox_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_MALLOC },

	{ .ct_name = "test_sandbox_printf",
	  .ct_desc = "printf() in a libcheri sandbox",
	  .ct_func_arg = test_sandbox_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_PRINTF ,
	  .ct_flags = CT_FLAG_STDOUT_STRING,
	  .ct_stdout_string = "invoke: printf in sandbox test\n" },

	{ .ct_name = "test_sandbox_cs_putchar",
	  .ct_desc = "putchar() in a libcheri sandbox",
	  .ct_func_arg = test_sandbox_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_CS_PUTCHAR,
	  .ct_flags = CT_FLAG_STDOUT_STRING,
	  .ct_stdout_string = "C" },

	{ .ct_name = "test_sandbox_cs_puts",
	  .ct_desc = "puts() in a libcheri sandbox",
	  .ct_func_arg = test_sandbox_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_CS_PUTS,
	  .ct_flags = CT_FLAG_STDOUT_STRING,
	  .ct_stdout_string = "sandbox cs_puts\n" },

	{ .ct_name = "test_sandbox_spin",
	  .ct_desc = "spin in a libcheri sandbox",
	  .ct_func_arg = test_sandbox_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_SPIN,
	  .ct_flags = CT_FLAG_SIGNAL,
	  .ct_signum = SIGALRM },

	{ .ct_name = "test_sandbox_syscall",
	  .ct_desc = "Invoke a system call in a libcheri sandbox",
	  .ct_func_arg = test_sandbox_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_SYSCALL },

	{ .ct_name = "test_sandbox_syscap",
	  .ct_desc = "Invoke the system capability in a libcheri sandbox",
	  .ct_func_arg = test_sandbox_simple_op,
	  .ct_arg = CHERITEST_HELPER_OP_SYSCAP },

	/*
	 * libcheri + cheri_fd tests.
	 */
	{ .ct_name = "test_sandbox_fd_fstat",
	  .ct_desc = "Exercise fstat() on a cheri_fd in a libcheri sandbox",
	  .ct_func_arg = test_sandbox_fd_op,
	  .ct_arg = CHERITEST_HELPER_OP_FD_FSTAT_C },

	{ .ct_name = "test_sandbox_fd_lseek",
	  .ct_desc = "Exercise lseek() on a cheri_fd in a libcheri sandbox",
	  .ct_func_arg = test_sandbox_fd_op,
	  .ct_arg = CHERITEST_HELPER_OP_FD_LSEEK_C },

	{ .ct_name = "test_sandbox_fd_read",
	  .ct_desc = "Exercise read() on a cheri_fd in a libcheri sandbox",
	  .ct_func = test_sandbox_fd_read,
	  .ct_flags = CT_FLAG_STDIN_STRING,
	  .ct_stdin_string = "read123" },

	{ .ct_name = "test_sandbox_fd_read_revoke",
	  .ct_desc = "Exercise revoke() before read() on a cheri_fd",
	  .ct_func = test_sandbox_fd_read_revoke,
	  .ct_flags = CT_FLAG_STDIN_STRING,
	  .ct_stdin_string = "read123" },

	{ .ct_name = "test_sandbox_fd_write",
	  .ct_desc = "Exercise write() on a cheri_fd in a libcheri sandbox",
	  .ct_func = test_sandbox_fd_write,
	  .ct_flags = CT_FLAG_STDOUT_STRING,
	  .ct_stdout_string = "write123" },

	{ .ct_name = "test_sandbox_fd_write_revoke",
	  .ct_desc = "Exercise revoke() before write() on a cheri_fd",
	  .ct_func = test_sandbox_fd_write_revoke,
	  /* NB: String defined but flag not set: shouldn't print. */
	  .ct_stdout_string = "write123" },

	{ .ct_name = "test_sandbox_userfn",
	  .ct_desc = "Exercise user-defined system-class method",
	  .ct_func = test_sandbox_userfn },

	{ .ct_name = "test_sandbox_getstack",
	  .ct_desc = "Exercise CHERI_GET_STACK sysarch()",
	  .ct_func = test_sandbox_getstack },

	{ .ct_name = "test_sandbox_setstack_nop",
	  .ct_desc = "Exercise CHERI_SET_STACK sysarch() for nop rewrite",
	  .ct_func = test_sandbox_setstack_nop },

	{ .ct_name = "test_sandbox_setstack",
	  .ct_desc = "Exercise CHERI_SET_STACK sysarch() to change stack",
	  .ct_func = test_sandbox_setstack },

	{ .ct_name = "test_sandbox_save_global",
	  .ct_desc = "Try to save global argument to sandbox heap",
	  .ct_func = test_sandbox_save_global },

	{ .ct_name = "test_sandbox_save_local",
	  .ct_desc = "Try to save local argument to sandbox heap",
	  .ct_func = test_sandbox_save_local,
	  .ct_flags = CT_FLAG_SIGNAL | CT_FLAG_MIPS_EXCCODE |
		    CT_FLAG_CP2_EXCCODE,
	  .ct_signum = SIGPROT,
	  .ct_mips_exccode = T_C2E,
	  .ct_cp2_exccode = CHERI_EXCCODE_STORE_LOCALCAP },

	/*
	 * libcheri + inflate/deflate tests.
	 */
	{ .ct_name = "test_sandbox_inflate_zeros",
	  .ct_desc = "Inflate a compressed buffer of zeros",
	  .ct_func = test_sandbox_inflate_zeros },

	{ .ct_name = "test_sandbox_var_bss",
	  .ct_desc = "Check initial value of .bss variable",
	  .ct_func = test_sandbox_var_bss },

	{ .ct_name = "test_sandbox_var_data",
	  .ct_desc = "Check initial value of .data variable",
	  .ct_func = test_sandbox_var_data },

	{ .ct_name = "test_sandbox_var_constructor",
	  .ct_desc = "Check initial value of constructor-initalised variable",
	  .ct_func = test_sandbox_var_constructor },
};
static const u_int cheri_tests_len = sizeof(cheri_tests) /
	    sizeof(cheri_tests[0]);

/* Shared memory page with child process. */
struct cheritest_child_state *ccsp;

int tests_failed, tests_passed;

static void
usage(void)
{
	u_int i;

	fprintf(stderr, "cheritest all\n");
	for (i = 0; i < cheri_tests_len; i++)
		fprintf(stderr, "cheritest %s\n", cheri_tests[i].ct_name);
	exit(EX_USAGE);
}

static void
signal_handler(int signum, siginfo_t *info __unused, ucontext_t *uap)
{
	struct cheri_frame *cfp;

	cfp = (struct cheri_frame *)uap->uc_mcontext.mc_cp2state;
	if (cfp == NULL || uap->uc_mcontext.mc_cp2state_len != sizeof(*cfp)) {
		ccsp->ccs_signum = -1;
		_exit(EX_SOFTWARE);
	}
	ccsp->ccs_signum = signum;
	ccsp->ccs_mips_cause = uap->uc_mcontext.cause;
	ccsp->ccs_cp2_cause = cfp->cf_capcause;
	_exit(EX_SOFTWARE);
}

/* Maximum size of stdout data we will check if called for by a test. */
#define	TEST_BUFFER_LEN	1024

static void
cheritest_run_test(const struct cheri_test *ctp)
{
	struct sigaction sa;
	pid_t childpid;
	int status, pipefd_stdin[2], pipefd_stdout[2];
	char reason[TESTRESULT_STR_LEN];
	char buffer[TEST_BUFFER_LEN];
	register_t cp2_exccode, mips_exccode;
	ssize_t len;

	bzero(ccsp, sizeof(*ccsp));
	printf("TEST: %s: %s\n", ctp->ct_name, ctp->ct_desc);

	if (pipe(pipefd_stdin) < 0)
		err(EX_OSERR, "pipe");
	if (pipe(pipefd_stdout) < 0)
		err(EX_OSERR, "pipe");

	/* If stdin is to be filled, fill it. */
	if (ctp->ct_flags & CT_FLAG_STDIN_STRING) {
		len = write(pipefd_stdin[1], ctp->ct_stdin_string,
		    strlen(ctp->ct_stdin_string));
		if (len < 0) {
			snprintf(reason, sizeof(reason),
			    "write() on test stdin failed with -1 (%d)",
			    errno);
			goto fail;
		}
		if (len != (ssize_t)strlen(ctp->ct_stdin_string)) {
			snprintf(reason, sizeof(reason),
			    "write() on test stdin expected %lu but got %ld",
			    strlen(ctp->ct_stdin_string), len);
			goto fail;
		}
	}

	childpid = fork();
	if (childpid < 0)
		err(EX_OSERR, "fork");
	if (childpid == 0) {
		/* Install signal handlers. */
		sa.sa_sigaction = signal_handler;
		sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
		sigemptyset(&sa.sa_mask);
		if (sigaction(SIGALRM, &sa, NULL) < 0)
			err(EX_OSERR, "sigaction(SIGALRM)");
		if (sigaction(SIGPROT, &sa, NULL) < 0)
			err(EX_OSERR, "sigaction(SIGPROT)");
		if (sigaction(SIGSEGV, &sa, NULL) < 0)
			err(EX_OSERR, "sigaction(SIGSEGV)");
		if (sigaction(SIGBUS, &sa, NULL) < 0)
			err(EX_OSERR, "sigaction(SIGBUS");
		if (sigaction(SIGEMT, &sa, NULL) < 0)
			err(EX_OSERR, "sigaction(SIGEMT)");
		if (sigaction(SIGTRAP, &sa, NULL) < 0)
			err(EX_OSERR, "sigaction(SIGEMT)");

		/*
		 * Set up synthetic stdin and stdout.
		 */
		if (dup2(pipefd_stdin[0], STDIN_FILENO) < 0)
			err(EX_OSERR, "dup2(STDIN_FILENO)");
		if (dup2(pipefd_stdout[1], STDOUT_FILENO) < 0)
			err(EX_OSERR, "dup2(STDOUT_FILENO)");
		close(pipefd_stdin[0]);
		close(pipefd_stdin[1]);
		close(pipefd_stdout[0]);
		close(pipefd_stdout[1]);

		/* Run the actual test. */
		if (ctp->ct_arg != 0)
			ctp->ct_func_arg(ctp, ctp->ct_arg);
		else
			ctp->ct_func(ctp);
		exit(0);
	}
	close(pipefd_stdin[0]);
	close(pipefd_stdout[1]);
	if (fcntl(pipefd_stdout[0], F_SETFL, O_NONBLOCK) < 0)
		err(EX_OSERR, "fcntl(F_SETFL, O_NONBLOCK) on test stdout");
	(void)waitpid(childpid, &status, 0);

	/*
	 * First, check for errors from the test framework: successful process
	 * termination, signal disposition/exception codes/etc.
	 *
	 * Analyse child's signal state returned via shared memory.
	 */
	if (!WIFEXITED(status)) {
		snprintf(reason, sizeof(reason), "Child exited abnormally");
		goto fail;
	}
	if (WEXITSTATUS(status) != 0 && WEXITSTATUS(status) != EX_SOFTWARE) {
		snprintf(reason, sizeof(reason), "Child status %d",
		    WEXITSTATUS(status));
		goto fail;
	}
	if (ccsp->ccs_signum < 0) {
		snprintf(reason, sizeof(reason),
		    "Child returned negative signal %d", ccsp->ccs_signum);
		goto fail;
	}
	if (ctp->ct_flags & CT_FLAG_SIGNAL &&
	    ccsp->ccs_signum != ctp->ct_signum) {
		snprintf(reason, sizeof(reason), "Expected signal %d, got %d",
		    ctp->ct_signum, ccsp->ccs_signum);
		goto fail;
	}
	if (ctp->ct_flags & CT_FLAG_MIPS_EXCCODE) {
		mips_exccode = (ccsp->ccs_mips_cause & MIPS_CR_EXC_CODE) >>
		    MIPS_CR_EXC_CODE_SHIFT;
		if (mips_exccode != ctp->ct_mips_exccode) {
			snprintf(reason, sizeof(reason),
			    "Expected MIPS exccode %ju, got %ju",
			    ctp->ct_mips_exccode, mips_exccode);
			goto fail;
		}
	}
	if (ctp->ct_flags & CT_FLAG_CP2_EXCCODE) {
		cp2_exccode = (ccsp->ccs_cp2_cause &
		    CHERI_CAPCAUSE_EXCCODE_MASK) >>
		    CHERI_CAPCAUSE_EXCCODE_SHIFT;
		if (cp2_exccode != ctp->ct_cp2_exccode) {
			snprintf(reason, sizeof(reason),
			    "Expected CP2 exccode %ju, got %ju",
			    ctp->ct_cp2_exccode, cp2_exccode);
			goto fail;
		}
	}

	/*
	 * Next, see whether any expected output was present.
	 */
	if (ctp->ct_flags & CT_FLAG_STDOUT_STRING) {
		len = read(pipefd_stdout[0], buffer, sizeof(buffer) - 1);
		if (len < 0) {
			snprintf(reason, sizeof(reason),
			    "read() on test stdout failed with -1 (%d)",
			    errno);
			goto fail;
		}
		buffer[len] = '\0';
		if (strcmp(buffer, ctp->ct_stdout_string) != 0) {
			snprintf(reason, sizeof(reason),
			    "read() on test stdout expected '%s' but got "
			    "'%s'", ctp->ct_stdout_string, buffer);
			goto fail;
		}
	} else if (!(ctp->ct_flags & CT_FLAG_STDOUT_IGNORE)) {
		len = read(pipefd_stdout[0], buffer, sizeof(buffer) - 1);
		if (len > 0) {
			snprintf(reason, sizeof(reason),
			    "read() on test stdout produced unexpected "
			    "output '%s'", buffer);
			goto fail;
		}
	}

	/*
	 * Next, we are concerned with whether the test itself reports a
	 * success.  This is based not on whether the test experiences a
	 * fault, but whether its semantics are correct -- e.g., did code in a
	 * sandbox run as expected.  Tests that have successfully experienced
	 * an expected/desired fault don't undergo these checks.
	 */
	if (!(ctp->ct_flags & CT_FLAG_SIGNAL)) {
		if (ccsp->ccs_testresult == TESTRESULT_UNKNOWN) {
			snprintf(reason, sizeof(reason),
			    "Test failed to set a success/failure status");
			goto fail;
		}
		if (ccsp->ccs_testresult == TESTRESULT_FAILURE) {
			/*
			 * Ensure string is nul-terminated, as we will print
			 * it in due course, and a failed test might have left
			 * a corrupted string.
			 */
			ccsp->ccs_testresult_str[
			    sizeof(ccsp->ccs_testresult_str) - 1] = '\0';
			memcpy(reason, ccsp->ccs_testresult_str,
			    sizeof(reason));
			goto fail;
		}
		if (ccsp->ccs_testresult != TESTRESULT_SUCCESS) {
			snprintf(reason, sizeof(reason),
			    "Test returned unexpected result (%d)",
			    ccsp->ccs_testresult);
			goto fail;
		}
	}

	fprintf(stderr, "PASS: %s\n", ctp->ct_name);
	tests_passed++;
	close(pipefd_stdin[1]);
	close(pipefd_stdout[0]);
	return;

fail:
	fprintf(stderr, "FAIL: %s: %s\n", ctp->ct_name, reason);
	tests_failed++;
	close(pipefd_stdin[1]);
	close(pipefd_stdout[0]);
}

static void
cheritest_run_test_name(const char *name)
{
	u_int i;

	for (i = 0; i < cheri_tests_len; i++) {
		if (strcmp(name, cheri_tests[i].ct_name) == 0)
			break;
	}
	if (i == cheri_tests_len)
		usage();
	cheritest_run_test(&cheri_tests[i]);
}

int
main(__unused int argc, __unused char *argv[])
{
	stack_t stack;
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

	/*
	 * Allocate an alternative stack, required to safely process signals in
	 * sandboxes.
	 *
	 * XXXRW: It is unclear if this should be done by libcheri rather than
	 * the main program?
	 */
	stack.ss_size = max(getpagesize(), SIGSTKSZ);
	stack.ss_sp = mmap(NULL, stack.ss_size, PROT_READ | PROT_WRITE,
	    MAP_ANON, -1, 0);
	if (stack.ss_sp == MAP_FAILED)
		err(EX_OSERR, "mmap");
	stack.ss_flags = 0;
	if (sigaltstack(&stack, NULL) < 0)
		err(EX_OSERR, "sigaltstack");

	/*
	 * Allocate a page shared with children processes to return success/
	 * failure status.
	 */
	assert(sizeof(*ccsp) <= (size_t)getpagesize());
	ccsp = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE, MAP_ANON, -1,
	    0);
	if (ccsp == MAP_FAILED)
		err(EX_OSERR, "mmap");
	if (minherit(ccsp, getpagesize(), INHERIT_SHARE) < 0)
		err(EX_OSERR, "minherit");

	/* Run the actual tests. */
	cheritest_libcheri_setup();
	if (argc == 1 && strcmp(argv[0], "all") == 0) {
		for (t = 0; t < cheri_tests_len; t++)
			cheritest_run_test(&cheri_tests[t]);
	} else {
		for (i = 0; i < argc; i++)
			cheritest_run_test_name(argv[i]);
	}
	if (tests_passed + tests_failed > 1)
		fprintf(stderr, "SUMMARY: passed %d failed %d\n",
		    tests_passed, tests_failed);

	cheritest_libcheri_destroy();
	if (tests_failed > 0)
		exit(-1);
	exit(EX_OK);
}
