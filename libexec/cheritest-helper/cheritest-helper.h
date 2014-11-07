/*-
 * Copyright (c) 2013-2014 Robert N. M. Watson
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

#ifndef _LIBEXEC_CHERITEST_CHERITEST_HELPER_H_
#define	_LIBEXEC_CHERITEST_CHERITEST_HELPER_H_

#define	CHERITEST_HELPER_OP_MD5		1
#define	CHERITEST_HELPER_OP_ABORT	2
#define	CHERITEST_HELPER_OP_SPIN	3
#define	CHERITEST_HELPER_OP_CP2_BOUND	4
#define	CHERITEST_HELPER_OP_CP2_PERM_STORE	5
#define	CHERITEST_HELPER_OP_CP2_TAG	6
#define	CHERITEST_HELPER_OP_CP2_SEAL	7
#define	CHERITEST_HELPER_OP_VM_RFAULT	8
#define	CHERITEST_HELPER_OP_VM_WFAULT	9
#define	CHERITEST_HELPER_OP_VM_XFAULT	10
#define	CHERITEST_HELPER_OP_SYSCALL	11
#define	CHERITEST_HELPER_OP_DIVZERO	12
#define	CHERITEST_HELPER_OP_SYSCAP	13
#define	CHERITEST_HELPER_OP_CS_HELLOWORLD	14
#define	CHERITEST_HELPER_OP_CS_PUTS	15
#define	CHERITEST_HELPER_OP_CS_PUTCHAR	16
#define	CHERITEST_HELPER_OP_PRINTF	17
#define CHERITEST_HELPER_OP_MALLOC	18
#define	CHERITEST_HELPER_OP_FD_FSTAT_C	19
#define	CHERITEST_HELPER_OP_FD_LSEEK_C	20
#define	CHERITEST_HELPER_OP_FD_READ_C	21
#define	CHERITEST_HELPER_OP_FD_WRITE_C	22
#define	CHERITEST_HELPER_OP_CS_CLOCK_GETTIME	23
#define	CHERITEST_HELPER_LIBCHERI_USERFN	24
#define	CHERITEST_HELPER_LIBCHERI_USERFN_SETSTACK	25
#define	CHERITEST_HELPER_SAVE_CAPABILITY_IN_HEAP	26
#define	CHERITEST_HELPER_OP_CP2_PERM_LOAD	27
#define	CHERITEST_HELPER_GET_VAR_BSS	28
#define	CHERITEST_HELPER_GET_VAR_DATA	29
#define	CHERITEST_HELPER_GET_VAR_CONSTRUCTOR	30
#define	CHERITEST_HELPER_OP_INFLATE	31
#define CHERITEST_HELPER_OP_SYSTEM_CALLOC	32

/*
 * We use system-class extensions to allow cheritest-helper code to call back
 * into cheritest to exercise various cases (e.g., stack-related tests).
 * These are the corresponding method numbers.
 */
#define	CHERITEST_USERFN_RETURNARG	(CHERI_SYSTEM_USER_BASE)
#define	CHERITEST_USERFN_GETSTACK	(CHERI_SYSTEM_USER_BASE + 1)
#define	CHERITEST_USERFN_SETSTACK	(CHERI_SYSTEM_USER_BASE + 2)

/*
 * Constants used to test BSS, .data, and constructor-based variable
 * initialisation in sandboxes.
 */
#define	CHERITEST_VALUE_BSS		0x00	/* Of course. */
#define	CHERITEST_VALUE_DATA		0xaa
#define	CHERITEST_VALUE_INVALID		0xbb
#define	CHERITEST_VALUE_CONSTRUCTOR	0xcc

struct zstream_proxy {
	__capability void *next_in;
	size_t avail_in;
	__capability void *next_out;
	size_t avail_out;
	size_t total_in;
	size_t total_out;
};

#endif /* !_LIBEXEC_CHERITEST_CHERITEST_HELPER_H_ */
