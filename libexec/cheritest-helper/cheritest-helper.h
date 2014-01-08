/*-
 * Copyright (c) 2013-2014 Robert N. M. Watson
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
#define	CHERITEST_HELPER_OP_CP2_PERM	5
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

#endif /* !_LIBEXEC_CHERITEST_CHERITEST_HELPER_H_ */
