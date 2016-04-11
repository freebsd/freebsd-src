/*-
 * Copyright (c) 2012-2015 Robert N. M. Watson
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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "cheritest.h"

/*
 * First batch of tests exercises CHERI faults outside of sandboxes.
 */
#define	ARRAY_LEN	2
static char array[ARRAY_LEN];
static char sink;

void
test_fault_bounds(const struct cheri_test *ctp __unused)
{
	__capability char *arrayp = cheri_ptr(array, sizeof(array));
	int i;

	for (i = 0; i < ARRAY_LEN; i++)
		arrayp[i] = 0;
	arrayp[i] = 0;
}

void
test_fault_perm_load(const struct cheri_test *ctp __unused)
{
	__capability char *arrayp = cheri_ptrperm(array, sizeof(array), 0);

	sink = arrayp[0];
}

void
test_nofault_perm_load(const struct cheri_test *ctp __unused)
{
	__capability char *arrayp = cheri_ptrperm(array, sizeof(array),
	    CHERI_PERM_LOAD);

	sink = arrayp[0];
	cheritest_success();
}

void
test_fault_perm_store(const struct cheri_test *ctp __unused)
{
	__capability char *arrayp = cheri_ptrperm(array, sizeof(array), 0);

	arrayp[0] = sink;
}

void
test_nofault_perm_store(const struct cheri_test *ctp __unused)
{
	__capability char *arrayp = cheri_ptrperm(array, sizeof(array),
	    CHERI_PERM_STORE);

	arrayp[0] = sink;
	cheritest_success();
}

void
test_fault_tag(const struct cheri_test *ctp __unused)
{
	char ch;
	__capability char *chp = cheri_ptr(&ch, sizeof(ch));

	chp = cheri_cleartag(chp);
	*chp = '\0';
}

void
test_fault_ccheck_user_fail(const struct cheri_test *ctp __unused)
{
	__capability void *cp;
	char ch;

	cp = cheri_ptrperm(&ch, sizeof(ch), 0);
	cheri_ccheckperm(cp, CHERI_PERM_USER0);
}

void
test_nofault_ccheck_user_pass(const struct cheri_test *ctp __unused)
{
	__capability void *cp;
	char ch;

	cp = cheri_ptrperm(&ch, sizeof(ch), CHERI_PERM_USER0);
	cheri_ccheckperm(cp, CHERI_PERM_USER0);
	cheritest_success();
}

void
test_fault_cgetcause(const struct cheri_test *ctp __unused)
{
	register_t cause;

	cause = cheri_getcause();
	printf("CP2 cause register: %ju\n", (uintmax_t)cause);
}

void
test_nofault_cfromptr(const struct cheri_test *ctp __unused)
{
	char buf[256];
	__capability void * cd; /* stored into here */
	__capability void * cb; /* derived from here */
	int rt;

	/*
	 * XXXRW: Could we be using CHERI_CFROMPTR() here to avoid explicit
	 * inline assembly?
	 */
	cb = cheri_ptr(buf, 256);
	rt = 10;
	__asm__ __volatile__ ("cfromptr %0, %1, %2" : "=r"(cd) : "r"(cb),
	    "r"(rt) : "memory");
	cheritest_success();
}

void
test_fault_read_kr1c(const struct cheri_test *ctp __unused)
{

	CHERI_CAPREG_PRINT(27);
}

void
test_fault_read_kr2c(const struct cheri_test *ctp __unused)
{

	CHERI_CAPREG_PRINT(28);
}

void
test_fault_read_kcc(const struct cheri_test *ctp __unused)
{

	CHERI_CAPREG_PRINT(29);
}

void
test_fault_read_kdc(const struct cheri_test *ctp __unused)
{

	CHERI_CAPREG_PRINT(30);
}

void
test_fault_read_epcc(const struct cheri_test *ctp __unused)
{

	CHERI_CAPREG_PRINT(31);
}

/*
 * Second batch of tests exercises various faults inside of sandboxes,
 * including VM faults, arithmetic faults, etc.
 */
void
test_sandbox_cp2_bound_catch(const struct cheri_test *ctp __unused)
{

	invoke_cap_fault(CHERITEST_HELPER_CAP_FAULT_CP2_BOUND);
	cheritest_failure_errx("invoke returned");
}

void
test_sandbox_cp2_bound_nocatch(const struct cheri_test *ctp __unused)
{
	register_t v;

	signal_handler_clear(SIGPROT);
	v = invoke_cap_fault(CHERITEST_HELPER_CAP_FAULT_CP2_BOUND);
	if (v != -1)
		cheritest_failure_errx("invoke returned %ld (expected %d)", v,
		    -1);
	cheritest_success();
}

void
test_sandbox_cp2_perm_load_catch(const struct cheri_test *ctp __unused)
{

	invoke_cap_fault(CHERITEST_HELPER_CAP_FAULT_CP2_PERM_LOAD);
	cheritest_failure_errx("invoke returned");
}

void
test_sandbox_cp2_perm_load_nocatch(const struct cheri_test *ctp __unused)
{
	register_t v;

	signal_handler_clear(SIGPROT);
	v = invoke_cap_fault(CHERITEST_HELPER_CAP_FAULT_CP2_PERM_LOAD);
	if (v != -1)
		cheritest_failure_errx("invoke returned %ld (expected %d)", v,
		    -1);
	cheritest_success();
}

void
test_sandbox_cp2_perm_store_catch(const struct cheri_test *ctp __unused)
{

	invoke_cap_fault(CHERITEST_HELPER_CAP_FAULT_CP2_PERM_STORE);
	cheritest_failure_errx("invoke returned");
}

void
test_sandbox_cp2_perm_store_nocatch(const struct cheri_test *ctp __unused)
{
	register_t v;

	signal_handler_clear(SIGPROT);
	v = invoke_cap_fault(CHERITEST_HELPER_CAP_FAULT_CP2_PERM_STORE);
	if (v != -1)
		cheritest_failure_errx("invoke returned %ld (expected %d)", v,
		    -1);
	cheritest_success();
}

void
test_sandbox_cp2_tag_catch(const struct cheri_test *ctp __unused)
{

	invoke_cap_fault(CHERITEST_HELPER_CAP_FAULT_CP2_TAG);
	cheritest_failure_errx("invoke returned");
}

void
test_sandbox_cp2_tag_nocatch(const struct cheri_test *ctp __unused)
{
	register_t v;

	signal_handler_clear(SIGPROT);
	v = invoke_cap_fault(CHERITEST_HELPER_CAP_FAULT_CP2_TAG);
	if (v != -1)
		cheritest_failure_errx("invoke returned %ld (expected %d)", v,
		    -1);
	cheritest_success();
}

void
test_sandbox_cp2_seal_catch(const struct cheri_test *ctp __unused)
{

	invoke_cap_fault(CHERITEST_HELPER_CAP_FAULT_CP2_SEAL);
	cheritest_failure_errx("invoke returned");
}

void
test_sandbox_cp2_seal_nocatch(const struct cheri_test *ctp __unused)
{
	register_t v;

	signal_handler_clear(SIGPROT);
	v = invoke_cap_fault(CHERITEST_HELPER_CAP_FAULT_CP2_SEAL);
	if (v != -1)
		cheritest_failure_errx("invoke returned %ld (expected %d)", v,
		    -1);
	cheritest_success();
}

void
test_sandbox_divzero_catch(const struct cheri_test *ctp __unused)
{

	invoke_divzero();
	cheritest_failure_errx("invoke returned");
}

void
test_sandbox_divzero_nocatch(const struct cheri_test *ctp __unused)
{
	register_t v;

	signal_handler_clear(SIGEMT);
	v = invoke_divzero();
	if (v != -1)
		cheritest_failure_errx("invoke returned %ld (expected %d)", v,
		    -1);
	cheritest_success();
}

void
test_sandbox_vm_rfault_catch(const struct cheri_test *ctp __unused)
{

	invoke_vm_fault(CHERITEST_HELPER_VM_FAULT_RFAULT);
	cheritest_failure_errx("invoke returned");
}

void
test_sandbox_vm_rfault_nocatch(const struct cheri_test *ctp __unused)
{
	register_t v;

	signal_handler_clear(SIGBUS);
	v = invoke_vm_fault(CHERITEST_HELPER_VM_FAULT_RFAULT);
	if (v != -1)
		cheritest_failure_errx("invoke returned %ld (expected %d)", v,
		    -1);
	cheritest_success();
}

void
test_sandbox_vm_wfault_catch(const struct cheri_test *ctp __unused)
{

	invoke_vm_fault(CHERITEST_HELPER_VM_FAULT_WFAULT);
	cheritest_failure_errx("invoke returned");
}

void
test_sandbox_vm_wfault_nocatch(const struct cheri_test *ctp __unused)
{
	register_t v;

	signal_handler_clear(SIGBUS);
	v = invoke_vm_fault(CHERITEST_HELPER_VM_FAULT_WFAULT);
	if (v != -1)
		cheritest_failure_errx("invoke returned %ld (expected %d)", v,
		    -1);
	cheritest_success();
}

void
test_sandbox_vm_xfault_catch(const struct cheri_test *ctp __unused)
{

	invoke_vm_fault(CHERITEST_HELPER_VM_FAULT_XFAULT);
	cheritest_failure_errx("invoke returned");
}

void
test_sandbox_vm_xfault_nocatch(const struct cheri_test *ctp __unused)
{
	register_t v;

	signal_handler_clear(SIGBUS);
	v = invoke_vm_fault(CHERITEST_HELPER_VM_FAULT_XFAULT);
	if (v != -1)
		cheritest_failure_errx("invoke returned %ld (expected %d)", v,
		    -1);
	cheritest_success();
}
