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

#ifndef _CHERITEST_H_
#define	_CHERITEST_H_

#define	CHERI_CAP_PRINT(cap) do {					\
	printf("tag %ju u %ju perms %08jx type %016jx\n",		\
	    (uintmax_t)cheri_gettag(cap),				\
	    (uintmax_t)cheri_getunsealed(cap),				\
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
#define	TESTRESULT_STR_LEN	80
struct cheritest_child_state {
	/* Fields filled in by the child signal handler. */
	int		ccs_signum;
	register_t	ccs_mips_cause;
	register_t	ccs_cp2_cause;

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
 * Useful APIs for tests.  These terminate the process returning either
 * success or failure with a test-defined, human-readable string describing
 * the error.
 *
 * XXXRW: It would be nice to also offer a cheritest_failure_err().
 */
void	cheritest_failure_errx(const char *msg, ...);
void	cheritest_success(void);

/* cheritest_ccall.c */
void	cheritest_sandbox_setup(void *sandbox_base, void *sandbox_end,
	    register_t sandbox_pc, __capability void **codecapp,
	    __capability void **datacapp);
void	test_fault_creturn(void);
void	test_nofault_ccall_creturn(void);
void	test_nofault_ccall_nop_creturn(void);
void	test_nofault_ccall_dli_creturn(void);

/* cheritest_fault.c */
void	test_fault_bounds(void);
void	test_fault_cgetcause(void);
void	test_fault_perm_load(void);
void	test_nofault_perm_load(void);
void	test_fault_perm_store(void);
void	test_nofault_perm_store(void);
void	test_fault_tag(void);
void	test_fault_ccheck_user_fail(void);
void	test_fault_read_kr1c(void);
void	test_fault_read_kr2c(void);
void	test_fault_read_kcc(void);
void	test_fault_read_kdc(void);
void	test_fault_read_epcc(void);
void	test_nofault_ccheck_user_pass(void);

/* cheritest_libcheri.c */
void	cheritest_invoke_fd_op(int op);
void	cheritest_revoke_fd(void);
void	cheritest_invoke_simple_op(int op);
void	cheritest_invoke_syscall(void);
void	cheritest_invoke_md5(void);
void	cheritest_libcheri_userfn(void);
int	cheritest_libcheri_setup(void);
void	cheritest_libcheri_destroy(void);

/* cheritest_registers.c */
void	cheritest_copyregs(void);
void	cheritest_listregs(void);

#endif /* !_CHERITEST_H_ */
