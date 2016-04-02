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

#ifndef _CHERITEST_LIST_ONLY_H_
#define	_CHERITEST_LIST_ONLY_H_

/*
 * A minimal set of definitions to allow the cheri_tests array to be
 * complied and linked for use by listtests().
 */

#ifndef SIGPROT
#define	SIGPROT				0
#endif
#define T_C2E				0
#define T_TRAP				0
#define T_TLB_LD_MISS			0
#define T_TLB_ST_MISS			0
#define CHERI_EXCCODE_ACCESS_EPCC	0
#define	CHERI_EXCCODE_LENGTH		0
#define	CHERI_EXCCODE_PERM_LOAD		0
#define	CHERI_EXCCODE_PERM_STORE	0
#define CHERI_EXCCODE_TAG		0
#define	CHERI_EXCCODE_USER_PERM		0
#define	CHERI_EXCCODE_ACCESS_KR1C	0
#define	CHERI_EXCCODE_ACCESS_KR2C	0
#define	CHERI_EXCCODE_ACCESS_KCC	0
#define	CHERI_EXCCODE_ACCESS_KDC	0
#define	CHERI_EXCCODE_TLBSTORE		0
#define	CHERI_EXCCODE_RETURN		0
#define	CHERI_EXCCODE_SEAL		0
#define	CHERI_EXCCODE_TYPE		0
#define	CHERI_EXCCODE_PERM_EXECUTE	0
#define	CHERI_EXCCODE_PERM_SEAL		0
#define	CHERI_EXCCODE_STORE_LOCALCAP	0
#define	CHERI_EXCCODE_SW_LOCALARG	0
#define	CHERI_EXCCODE_SW_LOCALRET	0

#define	test_initregs_default				NULL
#define	test_initregs_stack				NULL
#define	test_initregs_idc				NULL
#define	test_initregs_pcc				NULL
#define	test_copyregs					NULL
#define	test_listregs					NULL
#define	test_fault_cgetcause				NULL
#define	test_nofault_cfromptr				NULL
#define	test_fault_bounds				NULL
#define	test_fault_perm_load				NULL
#define	test_nofault_perm_load				NULL
#define	test_fault_perm_store				NULL
#define	test_nofault_perm_store				NULL
#define	test_fault_tag					NULL
#define	test_fault_ccheck_user_fail			NULL
#define	test_nofault_ccheck_user_pass			NULL
#define	test_fault_read_kr1c				NULL
#define	test_fault_read_kr2c				NULL
#define	test_fault_read_kcc				NULL
#define	test_fault_read_kdc				NULL
#define	test_fault_read_epcc				NULL
#define	test_bounds_stack_static_uint8			NULL
#define	test_bounds_stack_static_uint16			NULL
#define	test_bounds_stack_static_uint32			NULL
#define	test_bounds_stack_static_uint64			NULL
#define	test_bounds_stack_static_cap			NULL
#define	test_bounds_stack_static_16			NULL
#define	test_bounds_stack_static_32			NULL
#define	test_bounds_stack_static_64			NULL
#define	test_bounds_stack_static_128			NULL
#define	test_bounds_stack_static_256			NULL
#define	test_bounds_stack_static_512			NULL
#define	test_bounds_stack_static_1024			NULL
#define	test_bounds_stack_static_2048			NULL
#define	test_bounds_stack_static_4096			NULL
#define	test_bounds_stack_static_8192			NULL
#define	test_bounds_stack_static_16384			NULL
#define	test_bounds_stack_static_32768			NULL
#define	test_bounds_stack_static_65536			NULL
#define	test_bounds_stack_static_131072			NULL
#define	test_bounds_stack_static_262144			NULL
#define	test_bounds_stack_static_524288			NULL
#define	test_bounds_stack_static_1048576		NULL
#define	test_bounds_stack_dynamic_uint8			NULL
#define	test_bounds_stack_dynamic_uint16		NULL
#define	test_bounds_stack_dynamic_uint32		NULL
#define	test_bounds_stack_dynamic_uint64		NULL
#define	test_bounds_stack_dynamic_cap			NULL
#define	test_bounds_stack_dynamic_16			NULL
#define	test_bounds_stack_dynamic_32			NULL
#define	test_bounds_stack_dynamic_64			NULL
#define	test_bounds_stack_dynamic_128			NULL
#define	test_bounds_stack_dynamic_256			NULL
#define	test_bounds_stack_dynamic_512			NULL
#define	test_bounds_stack_dynamic_1024			NULL
#define	test_bounds_stack_dynamic_2048			NULL
#define	test_bounds_stack_dynamic_4096			NULL
#define	test_bounds_stack_dynamic_8192			NULL
#define	test_bounds_stack_dynamic_16384			NULL
#define	test_bounds_stack_dynamic_32768			NULL
#define	test_bounds_stack_dynamic_65536			NULL
#define	test_bounds_stack_dynamic_131072		NULL
#define	test_bounds_stack_dynamic_262144		NULL
#define	test_bounds_stack_dynamic_524288		NULL
#define	test_bounds_stack_dynamic_1048576		NULL
#define	cheritest_vm_tag_mmap_anon			NULL
#define	cheritest_vm_tag_shm_open_anon_shared		NULL
#define	cheritest_vm_tag_shm_open_anon_private		NULL
#define	cheritest_vm_tag_dev_zero_shared		NULL
#define	cheritest_vm_tag_dev_zero_private		NULL
#define	cheritest_vm_notag_tmpfile_shared		NULL
#define	cheritest_vm_tag_tmpfile_private		NULL
#define	cheritest_vm_tag_tmpfile_private_prefault	NULL
#define	cheritest_vm_cow_read				NULL
#define	cheritest_vm_cow_write				NULL
#define	cheritest_vm_swap				NULL
#define	test_fault_creturn				NULL
#define	test_nofault_ccall_creturn			NULL
#define	test_nofault_ccall_nop_creturn			NULL
#define	test_nofault_ccall_dli_creturn			NULL
#define	test_fault_ccall_code_untagged			NULL
#define	test_fault_ccall_data_untagged			NULL
#define	test_fault_ccall_code_unsealed			NULL
#define	test_fault_ccall_data_unsealed			NULL
#define	test_fault_ccall_typemismatch			NULL
#define	test_fault_ccall_code_noexecute			NULL
#define	test_fault_ccall_data_execute			NULL
#define	test_cheriabi_mmap_perms			NULL
#define	test_deflate_zeroes				NULL
#define	test_inflate_zeroes				NULL
#define	test_sandbox_inflate_zeroes			NULL
#define	test_sandbox_abort				NULL
#define	test_sandbox_cs_calloc				NULL
#define	test_sandbox_cs_clock_gettime			NULL
#define	test_sandbox_cs_helloworld			NULL
#define	test_sandbox_cs_putchar				NULL
#define	test_sandbox_cs_puts				NULL
#define	test_sandbox_malloc				NULL
#define	test_sandbox_printf				NULL
#define	test_sandbox_spin				NULL
#define	test_sandbox_syscall				NULL
#define	test_sandbox_cp2_bound_catch			NULL
#define	test_sandbox_cp2_bound_nocatch			NULL
#define	test_sandbox_cp2_perm_load_catch		NULL
#define	test_sandbox_cp2_perm_load_nocatch		NULL
#define	test_sandbox_cp2_perm_store_catch		NULL
#define	test_sandbox_cp2_perm_store_nocatch		NULL
#define	test_sandbox_cp2_tag_catch			NULL
#define	test_sandbox_cp2_tag_nocatch			NULL
#define	test_sandbox_cp2_seal_catch			NULL
#define	test_sandbox_cp2_seal_nocatch			NULL
#define	test_sandbox_divzero_catch			NULL
#define	test_sandbox_divzero_nocatch			NULL
#define	test_sandbox_vm_rfault_catch			NULL
#define	test_sandbox_vm_rfault_nocatch			NULL
#define	test_sandbox_vm_wfault_catch			NULL
#define	test_sandbox_vm_wfault_nocatch			NULL
#define	test_sandbox_vm_xfault_catch			NULL
#define	test_sandbox_vm_xfault_nocatch			NULL
#define	test_sandbox_md5_ccall				NULL
#define	test_sandbox_ptrdiff				NULL
#define	test_sandbox_varargs				NULL
#define	test_sandbox_va_copy				NULL
#define	test_sandbox_fd_fstat				NULL
#define	test_sandbox_fd_lseek				NULL
#define	test_sandbox_fd_read				NULL
#define	test_sandbox_fd_read_revoke			NULL
#define	test_sandbox_fd_write				NULL
#define	test_sandbox_fd_write_revoke			NULL
#define	test_sandbox_userfn				NULL
#define	test_2sandbox_newdestroy			NULL
#define	test_sandbox_store_global_capability_in_bss	NULL
#define	test_sandbox_store_local_capability_in_bss_catch	NULL
#define	test_sandbox_store_local_capability_in_bss_nocatch	NULL
#define	test_sandbox_store_global_capability_in_stack	NULL
#define	test_sandbox_store_local_capability_in_stack	NULL
#define	test_sandbox_return_global_capability		NULL
#define	test_sandbox_return_local_capability_catch	NULL
#define	test_sandbox_return_local_capability_nocatch	NULL
#define	test_sandbox_pass_local_capability_arg		NULL
#define	test_sandbox_getstack				NULL
#define	test_sandbox_setstack_nop			NULL
#define	test_sandbox_setstack				NULL
#define	test_sandbox_var_bss				NULL
#define	test_sandbox_var_data				NULL
#define	test_sandbox_var_data_getset			NULL
#define	test_2sandbox_var_data_getset			NULL
#define	test_sandbox_var_constructor			NULL
#define test_string_memcpy				NULL
#define test_string_memcpy_c				NULL
#define test_string_memmove				NULL
#define test_string_memmove_c				NULL

#define	xfail_swap_required				NULL
#define	xfail_need_writable_tmp				NULL

#ifdef CHERI_C_TESTS
#define DECLARE_TEST(name, desc)				\
    inline void cheri_c_test_ ## name( 			\
	const struct cheri_test *ctp __unused) {}
#include <cheri_c_testdecls.h>
#undef DECLARE_TEST
#endif


#endif /* !_CHERITEST_LIST_ONLY_H_ */
