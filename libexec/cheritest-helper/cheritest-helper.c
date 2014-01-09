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

#include <sys/types.h>

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <cheri/cheri_invoke.h>
#include <cheri/cheri_memcpy.h>
#include <cheri/cheri_system.h>

#include <md5.h>
#include <stdio.h>
#include <stdlib.h>

#include "cheritest-helper.h"

int	invoke(register_t op, size_t len, __capability void *system_codecap,
	    __capability void *system_datacap, __capability char *data_input,
	    __capability char *data_output);

static int
invoke_md5(size_t len, __capability char *data_input,
  __capability char *data_output)
{
	MD5_CTX md5context;
	char buf[33], ch;
	u_int count;

	MD5Init(&md5context);
	for (count = 0; count < len; count++) {
		/* XXXRW: Want a CMD5Update() to avoid copying byte by byte. */
		ch = data_input[count];
		MD5Update(&md5context, &ch, sizeof(ch));
	}
	MD5End(&md5context, buf);
	for (count = 0; count < sizeof(buf); count++)
		data_output[count] = buf[count];

	return (123456);
}

#define	N	10
static int
invoke_cap_fault(register_t op)
{
	char buffer[N], ch;
	__capability char *cap;

	cap = cheri_ptrperm(buffer, sizeof(buffer), CHERI_PERM_LOAD);
	switch (op) {
	case CHERITEST_HELPER_OP_CP2_BOUND:
		ch = cap[N];
		return (ch);

	case CHERITEST_HELPER_OP_CP2_PERM:
		cap[0] = 0;
		break;

	case CHERITEST_HELPER_OP_CP2_TAG:
		cap = cheri_zerocap();
		ch = cap[0];
		return (ch);

	case CHERITEST_HELPER_OP_CP2_SEAL:
		cap = cheri_sealcode(cap);
		ch = cap[0];
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
invoke_syscap(__capability void *system_codecap,
    __capability void *system_datacap)
{

	return (cheri_invoke(system_codecap, system_datacap, 0, 0, 0, 0, 0, 0,
	    0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
}

static int
invoke_malloc(void)
{
	size_t i;
	void *tmp;
	const size_t sizes[] = {1, 2, 4, 8, 16, 32, 64, 128, 1024, 4096, 10000};

	for (i = 0; i < sizeof(sizes) / sizeof(*sizes); i++) {
		tmp = malloc(sizes[i]);
		if (tmp == NULL) {
			printf("Failed to allocate %zd bytes\n", sizes[i]);
			return (-1);
		}
		printf("allocated %zd bytes at %p\n", sizes[i], tmp);
		free(tmp);
	}

	return (0);
}

/*
 * Sample sandboxed code.  Calculate an MD5 checksum of the data arriving via
 * c3, and place the checksum in c4.  a0 will hold input data length.  a1
 * indicates whether we should try a system call (abort()).  c4 must be (at
 * least) 33 bytes.
 */
int
invoke(register_t op, size_t len, __capability void *system_codecap,
    __capability void *system_datacap, __capability char *data_input,
    __capability char *data_output)
{

	cheri_system_setup(system_codecap, system_datacap);

	switch (op) {
	case CHERITEST_HELPER_OP_MD5:
		return (invoke_md5(len, data_input, data_output));

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
		return (invoke_syscap(system_codecap, system_datacap));

	case CHERITEST_HELPER_OP_CS_HELLOWORLD:
		return (cheri_system_helloworld());

	case CHERITEST_HELPER_OP_CS_PUTS:
		return (cheri_system_puts(
		    (__capability char *)"sandbox cs_puts\n"));

	case CHERITEST_HELPER_OP_CS_PUTCHAR:
		return (cheri_system_putchar('C'));	/* Is for cookie. */

	case CHERITEST_HELPER_OP_PRINTF:
		return (printf("%s: printf in sandbox test\n", __func__));

	case CHERITEST_HELPER_OP_MALLOC:
		return (invoke_malloc());
	}
	return (-1);
}
