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

#ifndef _CHERITEST_H_
#define	_CHERITEST_H_

#define	CHERI_CAP_PRINT(cap) do {					\
	printf("tag %ju s %ju perms %08jx type %016jx\n",		\
	    (uintmax_t)cheri_gettag(cap),				\
	    (uintmax_t)cheri_getsealed(cap),				\
	    (uintmax_t)cheri_getperm(cap),				\
	    (uintmax_t)cheri_gettype(cap));				\
	printf("\tbase %016jx length %016jx\n",				\
	    (uintmax_t)cheri_getbase(cap),				\
	    (uintmax_t)cheri_getlen(cap));				\
} while (0)

#define	CHERI_CAPREG_PRINT(crn) do {					\
	__capability void *cap;						\
	if (crn == 0)							\
		cap = cheri_getdefault();				\
	else								\
		cap = cheri_getreg(crn);				\
	printf("C%u ", crn);						\
	CHERI_CAP_PRINT(cap);						\
} while (0)

#define	CHERI_PCC_PRINT() do {						\
	__capability void *cap;						\
	cap = cheri_getpcc();						\
	printf("PCC ");							\
	CHERI_CAP_PRINT(cap);						\
} while (0)

/*
 * Shared memory interface between tests and the test controller process.
 */
#define	TESTRESULT_STR_LEN	1024
struct cheritest_child_state {
	/* Fields filled in by the child signal handler. */
	int		ccs_signum;
	register_t	ccs_mips_cause;
	register_t	ccs_cp2_cause;
	int		ccs_unwound;  /* If any trusted-stack frames unwound. */

	/* Fields filled in by the test itself. */
	int		ccs_testresult;
	char		ccs_testresult_str[TESTRESULT_STR_LEN];
};
extern struct cheritest_child_state *ccsp;

/*
 * If the test runs to completion, it must set ccs_testresult to SUCCESS or
 * FAILURE.  If the latter, it should also fill ccs_testresult_str with a
 * suitable message to display to the user.
 */
#define	TESTRESULT_UNKNOWN	0	/* Default initialisation. */
#define	TESTRESULT_SUCCESS	1	/* Test declares success. */
#define	TESTRESULT_FAILURE	2	/* Test declares failure. */

/*
 * Description structure for each test -- passed to the test in case it needs
 * access to configuration state, such as strings passed to/from stdio.
 */
#define	CT_FLAG_SIGNAL		0x00000001  /* Should fault; checks signum. */
#define	CT_FLAG_MIPS_EXCCODE	0x00000002  /* Check MIPS exception code. */
#define	CT_FLAG_CP2_EXCCODE	0x00000004  /* Check CP2 exception code. */
#define	CT_FLAG_STDOUT_STRING	0x00000008  /* Check stdout for a string. */
#define	CT_FLAG_STDIN_STRING	0x00000010  /* Provide string on stdin. */
#define	CT_FLAG_STDOUT_IGNORE	0x00000020  /* Standard output produced,
					       but not checkable */
#define CT_FLAG_SLOW		0x00000040  /* Test is expected to take a 
					       long time to run */
#define	CT_FLAG_SIGNAL_UNWIND	0x00000080  /* Should fault and unwind
					       trusted stack; checks signum
					       and result. */

#define	CHERITEST_SANDBOX_UNWOUND	0x123456789ULL

struct cheri_test {
	const char	*ct_name;
	const char	*ct_desc;
	int		 ct_arg;	/* 0: ct_func; otherwise ct_func_arg. */
	void		(*ct_func)(const struct cheri_test *);
	void		(*ct_func_arg)(const struct cheri_test *, int);
	const char *	(*ct_check_xfail)(const char *);
	u_int		 ct_flags;
	int		 ct_signum;
	register_t	 ct_mips_exccode;
	register_t	 ct_cp2_exccode;
	const char	*ct_stdin_string;
	const char	*ct_stdout_string;
	const char	*ct_xfail_reason;
};

/*
 * Useful APIs for tests.  These terminate the process returning either
 * success or failure with a test-defined, human-readable string describing
 * the error.
 */
void	cheritest_failure_err(const char *msg, ...) __dead2  __printflike(1, 2);
void	cheritest_failure_errx(const char *msg, ...) __dead2  __printflike(1, 2);
void	cheritest_success(void) __dead2;
void	signal_handler_clear(int sig);

/* cheritest_bounds_heap.c */
void	test_bounds_calloc(const struct cheri_test *ctp);

/* cheritest_bounds_stack.c */
void	test_bounds_stack_static_uint8(const struct cheri_test *ctp);
void	test_bounds_stack_static_uint16(const struct cheri_test *ctp);
void	test_bounds_stack_static_uint32(const struct cheri_test *ctp);
void	test_bounds_stack_static_uint64(const struct cheri_test *ctp);
void	test_bounds_stack_static_cap(const struct cheri_test *ctp);
void	test_bounds_stack_static_16(const struct cheri_test *ctp);
void	test_bounds_stack_static_32(const struct cheri_test *ctp);
void	test_bounds_stack_static_64(const struct cheri_test *ctp);
void	test_bounds_stack_static_128(const struct cheri_test *ctp);
void	test_bounds_stack_static_256(const struct cheri_test *ctp);
void	test_bounds_stack_static_512(const struct cheri_test *ctp);
void	test_bounds_stack_static_1024(const struct cheri_test *ctp);
void	test_bounds_stack_static_2048(const struct cheri_test *ctp);
void	test_bounds_stack_static_4096(const struct cheri_test *ctp);
void	test_bounds_stack_static_8192(const struct cheri_test *ctp);
void	test_bounds_stack_static_16384(const struct cheri_test *ctp);
void	test_bounds_stack_static_32768(const struct cheri_test *ctp);
void	test_bounds_stack_static_65536(const struct cheri_test *ctp);
void	test_bounds_stack_static_131072(const struct cheri_test *ctp);
void	test_bounds_stack_static_262144(const struct cheri_test *ctp);
void	test_bounds_stack_static_524288(const struct cheri_test *ctp);
void	test_bounds_stack_static_1048576(const struct cheri_test *ctp);
void	test_bounds_stack_dynamic_uint8(const struct cheri_test *ctp);
void	test_bounds_stack_dynamic_uint16(const struct cheri_test *ctp);
void	test_bounds_stack_dynamic_uint32(const struct cheri_test *ctp);
void	test_bounds_stack_dynamic_uint64(const struct cheri_test *ctp);
void	test_bounds_stack_dynamic_cap(const struct cheri_test *ctp);
void	test_bounds_stack_dynamic_16(const struct cheri_test *ctp);
void	test_bounds_stack_dynamic_32(const struct cheri_test *ctp);
void	test_bounds_stack_dynamic_64(const struct cheri_test *ctp);
void	test_bounds_stack_dynamic_128(const struct cheri_test *ctp);
void	test_bounds_stack_dynamic_256(const struct cheri_test *ctp);
void	test_bounds_stack_dynamic_512(const struct cheri_test *ctp);
void	test_bounds_stack_dynamic_1024(const struct cheri_test *ctp);
void	test_bounds_stack_dynamic_2048(const struct cheri_test *ctp);
void	test_bounds_stack_dynamic_4096(const struct cheri_test *ctp);
void	test_bounds_stack_dynamic_8192(const struct cheri_test *ctp);
void	test_bounds_stack_dynamic_16384(const struct cheri_test *ctp);
void	test_bounds_stack_dynamic_32768(const struct cheri_test *ctp);
void	test_bounds_stack_dynamic_65536(const struct cheri_test *ctp);
void	test_bounds_stack_dynamic_131072(const struct cheri_test *ctp);
void	test_bounds_stack_dynamic_262144(const struct cheri_test *ctp);
void	test_bounds_stack_dynamic_524288(const struct cheri_test *ctp);
void	test_bounds_stack_dynamic_1048576(const struct cheri_test *ctp);

/* cheritest_ccall.c */
void	cheritest_ccall_setup(void);
void	test_nofault_ccall_creturn(const struct cheri_test *ctp);
void	test_nofault_ccall_nop_creturn(const struct cheri_test *ctp);
void	test_nofault_ccall_dli_creturn(const struct cheri_test *ctp);
void	test_fault_creturn(const struct cheri_test *ctp);
void	test_fault_ccall_code_untagged(const struct cheri_test *ctp);
void	test_fault_ccall_data_untagged(const struct cheri_test *ctp);
void	test_fault_ccall_code_unsealed(const struct cheri_test *ctp);
void	test_fault_ccall_data_unsealed(const struct cheri_test *ctp);
void	test_fault_ccall_typemismatch(const struct cheri_test *ctp);
void	test_fault_ccall_code_noexecute(const struct cheri_test *ctp);
void	test_fault_ccall_data_execute(const struct cheri_test *ctp);

/* cheritest_cheriabi.c */
void	test_cheriabi_mmap_nospace(const struct cheri_test *ctp);
void	test_cheriabi_mmap_perms(const struct cheri_test *ctp);

/* cheritest_fault.c */
void	test_fault_bounds(const struct cheri_test *ctp);
void	test_fault_cgetcause(const struct cheri_test *ctp);
void	test_nofault_cfromptr(const struct cheri_test *ctp);
void	test_fault_perm_load(const struct cheri_test *ctp);
void	test_nofault_perm_load(const struct cheri_test *ctp);
void	test_fault_perm_store(const struct cheri_test *ctp);
void	test_nofault_perm_store(const struct cheri_test *ctp);
void	test_fault_tag(const struct cheri_test *ctp);
void	test_fault_ccheck_user_fail(const struct cheri_test *ctp);
void	test_fault_read_kr1c(const struct cheri_test *ctp);
void	test_fault_read_kr2c(const struct cheri_test *ctp);
void	test_fault_read_kcc(const struct cheri_test *ctp);
void	test_fault_read_kdc(const struct cheri_test *ctp);
void	test_fault_read_epcc(const struct cheri_test *ctp);
void	test_nofault_ccheck_user_pass(const struct cheri_test *ctp);

void	test_sandbox_cp2_bound_catch(const struct cheri_test *ctp);
void	test_sandbox_cp2_bound_nocatch(const struct cheri_test *ctp);
void	test_sandbox_cp2_perm_load_catch(const struct cheri_test *ctp);
void	test_sandbox_cp2_perm_load_nocatch(const struct cheri_test *ctp);
void	test_sandbox_cp2_perm_store_catch(const struct cheri_test *ctp);
void	test_sandbox_cp2_perm_store_nocatch(const struct cheri_test *ctp);
void	test_sandbox_cp2_tag_catch(const struct cheri_test *ctp);
void	test_sandbox_cp2_tag_nocatch(const struct cheri_test *ctp);
void	test_sandbox_cp2_seal_catch(const struct cheri_test *ctp);
void	test_sandbox_cp2_seal_nocatch(const struct cheri_test *ctp);
void	test_sandbox_divzero_catch(const struct cheri_test *ctp);
void	test_sandbox_divzero_nocatch(const struct cheri_test *ctp);
void	test_sandbox_vm_rfault_catch(const struct cheri_test *ctp);
void	test_sandbox_vm_rfault_nocatch(const struct cheri_test *ctp);
void	test_sandbox_vm_wfault_catch(const struct cheri_test *ctp);
void	test_sandbox_vm_wfault_nocatch(const struct cheri_test *ctp);
void	test_sandbox_vm_xfault_catch(const struct cheri_test *ctp);
void	test_sandbox_vm_xfault_nocatch(const struct cheri_test *ctp);

/* cheritest_fd.c */
#define	CHERITEST_FD_READ_STR	"read123"
#define	CHERITEST_FD_WRITE_STR	"write123"

extern int			 zero_fd;
extern struct cheri_object	 stdin_fd_object;
extern struct cheri_object	 stdout_fd_object;
extern struct cheri_object	 zero_fd_object;

void	test_sandbox_fd_fstat(const struct cheri_test *ctp);
void	test_sandbox_fd_lseek(const struct cheri_test *ctp);
void	test_sandbox_fd_read(const struct cheri_test *ctp);
void	test_sandbox_fd_read_revoke(const struct cheri_test *ctp);
void	test_sandbox_fd_write(const struct cheri_test *ctp);
void	test_sandbox_fd_write_revoke(const struct cheri_test *ctp);

/* cheritest_libcheri.c */
extern struct sandbox_class	*cheritest_classp;
extern struct sandbox_object	*cheritest_objectp;

void	test_sandbox_abort(const struct cheri_test *ctp);
void	test_sandbox_cs_calloc(const struct cheri_test *ctp);
void	test_sandbox_cs_clock_gettime(const struct cheri_test *ctp);
void	test_sandbox_cs_clock_gettime_default(const struct cheri_test *ctp);
void	test_sandbox_cs_clock_gettime_deny(const struct cheri_test *ctp);
void	test_sandbox_cs_helloworld(const struct cheri_test *ctp);
void	test_sandbox_cs_putchar(const struct cheri_test *ctp);
void	test_sandbox_cs_puts(const struct cheri_test *ctp);
void	test_sandbox_malloc(const struct cheri_test *ctp);
void	test_sandbox_md5_ccall(const struct cheri_test *ctp, int class2);
void	test_sandbox_printf(const struct cheri_test *ctp);
void	test_sandbox_ptrdiff(const struct cheri_test *ctp);
void	test_sandbox_varargs(const struct cheri_test *ctp);
void	test_sandbox_va_copy(const struct cheri_test *ctp);
void	test_sandbox_spin(const struct cheri_test *ctp);
void	test_sandbox_userfn(const struct cheri_test *ctp);
void	test_2sandbox_newdestroy(const struct cheri_test *ctp);
int	cheritest_libcheri_setup(void);
void	cheritest_libcheri_destroy(void);

/* cheritest_local.c */
void	test_sandbox_store_global_capability_in_bss(
	    const struct cheri_test *ctp);
void	test_sandbox_store_local_capability_in_bss_catch(
	    const struct cheri_test *ctp);
void	test_sandbox_store_local_capability_in_bss_nocatch(
	    const struct cheri_test *ctp);
void	test_sandbox_store_global_capability_in_stack(
	    const struct cheri_test *ctp);
void	test_sandbox_store_local_capability_in_stack(
	    const struct cheri_test *ctp);
void	test_sandbox_return_global_capability(const struct cheri_test *ctp);
void	test_sandbox_return_local_capability_catch(
	    const struct cheri_test *ctp);
void	test_sandbox_return_local_capability_nocatch(
	    const struct cheri_test *ctp);
void	test_sandbox_pass_local_capability_arg(const struct cheri_test *ctp);

/* cheritest_stack.c */
register_t	cheritest_libcheri_userfn_getstack(void);
register_t	cheritest_libcheri_userfn_setstack(register_t arg);
void	test_sandbox_getstack(const struct cheri_test *ctp);
void	test_sandbox_setstack(const struct cheri_test *ctp);
void	test_sandbox_setstack_nop(const struct cheri_test *ctp);

/* cheritest_string.c */
void	test_string_memcpy(const struct cheri_test *ctp);
void	test_string_memcpy_c(const struct cheri_test *ctp);
void	test_string_memmove(const struct cheri_test *ctp);
void	test_string_memmove_c(const struct cheri_test *ctp);

/* cheritest_syscall.c */
void	test_sandbox_syscall(const struct cheri_test *ctp);

/* cheritest_registers.c */
void	test_initregs_default(const struct cheri_test *ctp);
void	test_initregs_stack(const struct cheri_test *ctp);
void	test_initregs_idc(const struct cheri_test *ctp);
void	test_initregs_pcc(const struct cheri_test *ctp);
void	test_copyregs(const struct cheri_test *ctp);
void	test_listregs(const struct cheri_test *ctp);

/* cheritest_var.c */
void	test_sandbox_var_bss(const struct cheri_test *ctp);
void	test_sandbox_var_data(const struct cheri_test *ctp);
void	test_sandbox_var_data_getset(const struct cheri_test *ctp);
void	test_2sandbox_var_data_getset(const struct cheri_test *ctp);
void	test_sandbox_var_constructor(const struct cheri_test *ctp);

/* cheritest_vm.c */
void	cheritest_vm_tag_mmap_anon(const struct cheri_test *ctp __unused);
void	cheritest_vm_tag_shm_open_anon_shared(const struct cheri_test *ctp
	    __unused);
void	cheritest_vm_tag_shm_open_anon_private(const struct cheri_test *ctp
	    __unused);
void	cheritest_vm_tag_dev_zero_shared(const struct cheri_test *ctp
	    __unused);
void	cheritest_vm_tag_dev_zero_private(const struct cheri_test *ctp
	    __unused);
void	cheritest_vm_notag_tmpfile_shared(const struct cheri_test *ctp __unused);
void	cheritest_vm_tag_tmpfile_private(const struct cheri_test *ctp __unused);
void	cheritest_vm_tag_tmpfile_private_prefault(const struct cheri_test *ctp
	    __unused);
void	cheritest_vm_cow_read(const struct cheri_test *ctp);
void	cheritest_vm_cow_write(const struct cheri_test *ctp);
const char	*xfail_need_writable_tmp(const char *name);
const char	*xfail_need_writable_non_tmpfs_tmp(const char *name);

/* cheritest_vm_swap.c */
void	cheritest_vm_swap(const struct cheri_test *ctp __unused);
const char	*xfail_swap_required(const char *name);

/* cheritest_zlib.c */
void	test_deflate_zeroes(const struct cheri_test *ctp);
void	test_inflate_zeroes(const struct cheri_test *ctp);
void 	test_sandbox_inflate_zeroes(const struct cheri_test *ctp);

#ifdef CHERI_C_TESTS
#define DECLARE_TEST(name, desc) \
    void	cheri_c_test_ ## name(const struct cheri_test *ctp __unused);
#define	DECLARE_TEST_FAULT(name, desc)	/* Not supported */
#include <cheri_c_testdecls.h>
#undef DECLARE_TEST
#endif

#endif /* !_CHERITEST_H_ */
