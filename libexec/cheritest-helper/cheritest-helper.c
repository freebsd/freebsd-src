/*-
 * Copyright (c) 2012-2014 Robert N. M. Watson
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

#if __has_feature(capabilities)
#define	USE_C_CAPS
#endif

#include <sys/types.h>

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <cheri_invoke.h>
#include <md5.h>
#include <stdlib.h>

#include "cmemcpy.h"
#include "cheritest-helper.h"

#ifdef USE_C_CAPS
int	invoke(register_t op, size_t len, __capability void *system_codecap,
	    __capability void *system_datacap, __capability char *data_input,
	    __capability char *data_output);
#else
int	invoke(register_t op, size_t len);
#endif

static int
#ifdef USE_C_CAPS
invoke_md5(size_t len, __capability char *data_input,
  __capability char *data_output)
#else
invoke_md5(size_t len)
#endif
{
	MD5_CTX md5context;
	char buf[33], ch;
	u_int count;

	MD5Init(&md5context);
	for (count = 0; count < len; count++) {
#ifdef USE_C_CAPS
		/* XXXRW: Want a CMD5Update() to avoid copying byte by byte. */
		ch = data_input[count];
#else
		memcpy_fromcap(&ch, 5, count, sizeof(ch));
#endif
		MD5Update(&md5context, &ch, sizeof(ch));
	}
	MD5End(&md5context, buf);
#ifdef USE_C_CAPS
	for (count = 0; count < sizeof(buf); count++)
		data_output[count] = buf[count];
#else
	memcpy_tocap(6, buf, 0, sizeof(buf));
#endif

	return (123456);
}

#define	N	10
static int
invoke_cap_fault(register_t op)
{
	char buffer[N], ch;
#ifdef USE_C_CAPS
	__capability char *cap;

	cap = cheri_ptrperm(buffer, sizeof(buffer), CHERI_PERM_LOAD);
#else
	CHERI_CINCBASE(11, 0, (uintptr_t)buffer);
	CHERI_CSETLEN(11, 11, sizeof(buffer));
	CHERI_CANDPERM(11, 11, CHERI_PERM_LOAD);
#endif

	switch (op) {
	case CHERITEST_HELPER_OP_CP2_BOUND:
#ifdef USE_C_CAPS
		ch = cap[N];
#else
		CHERI_CLB(ch, N, 0, 11);
#endif
		return (ch);

	case CHERITEST_HELPER_OP_CP2_PERM:
#ifdef USE_C_CAPS
		cap[0] = 0;
#else
		CHERI_CSB(0, 0, 0, 11);
#endif
		break;

	case CHERITEST_HELPER_OP_CP2_TAG:
#ifdef USE_C_CAPS
		cap = cheri_zerocap();
		ch = cap[0];
#else
		CHERI_CCLEARTAG(11);
		CHERI_CLB(ch, 0, 0, 11);
#endif
		return (ch);

	case CHERITEST_HELPER_OP_CP2_SEAL:
#ifdef USE_C_CAPS
		cap = cheri_sealcode(cap);
		ch = cap[0];
#else
		CHERI_CSEALCODE(11, 11);
		CHERI_CLB(ch, 0, 0, 11);
#endif
		return (ch);
	}
	return (0);
}

static int
invoke_vm_fault(register_t op)
{
	volatile char *chp;
	void (*fn)(void) = NULL;
	char ch;

	chp = NULL;
	switch (op) {
	case CHERITEST_HELPER_OP_VM_RFAULT:
		ch = chp[0];
		break;

	case CHERITEST_HELPER_OP_VM_WFAULT:
		chp[0] = 0;
		break;

	case CHERITEST_HELPER_OP_VM_XFAULT:
		fn();
		break;
	}
	return (0);
}

static int
invoke_syscall(void)
{

	/*
	 * Invoke getpid() to trigger kernel protection features.  Should
	 * mostly be a nop.
	 */
	__asm__ __volatile__ ("syscall 20");

	return (123456);
}

static int
#ifdef USE_C_CAPS
invoke_syscap(__capability void *system_codecap,
    __capability void *system_datacap)
{

	return (cheri_invoke(system_codecap, system_datacap, 0, 0, 0, 0, 0, 0,
	    0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
}
#else
invoke_syscap(void)
{

	CHERI_CMOVE(1, 3);
	CHERI_CMOVE(2, 4);
	return (cheri_invoke(0, 0, 0, 0, 0, 0, 0, 0));
}
#endif

/*
 * Sample sandboxed code.  Calculate an MD5 checksum of the data arriving via
 * c3, and place the checksum in c4.  a0 will hold input data length.  a1
 * indicates whether we should try a system call (abort()).  c4 must be (at
 * least) 33 bytes.
 */
int
#ifdef USE_C_CAPS
invoke(register_t op, size_t len, __capability void *system_codecap,
    __capability void *system_datacap, __capability char *data_input,
    __capability char *data_output)
#else
invoke(register_t op, size_t len)
#endif
{
	int ret;

	switch (op) {
	case CHERITEST_HELPER_OP_MD5:
#ifdef USE_C_CAPS
		return (invoke_md5(len, data_input, data_output));
#else
		return (invoke_md5(len));
#endif

	case CHERITEST_HELPER_OP_ABORT:
		abort();

	case CHERITEST_HELPER_OP_SPIN:
		while (1);

	case CHERITEST_HELPER_OP_CP2_BOUND:
	case CHERITEST_HELPER_OP_CP2_PERM:
	case CHERITEST_HELPER_OP_CP2_TAG:
	case CHERITEST_HELPER_OP_CP2_SEAL:
		return (invoke_cap_fault(op));

	case CHERITEST_HELPER_OP_VM_RFAULT:
	case CHERITEST_HELPER_OP_VM_WFAULT:
	case CHERITEST_HELPER_OP_VM_XFAULT:
		return (invoke_vm_fault(op));

	case CHERITEST_HELPER_OP_SYSCALL:
		return (invoke_syscall());

	case CHERITEST_HELPER_OP_DIVZERO:
		return (1/0);

	case CHERITEST_HELPER_OP_SYSCAP:
#ifdef USE_C_CAPS
		return (invoke_syscap(system_codecap, system_datacap));
#else
		return (invoke_syscap());
#endif
	}
	return (ret);
}
