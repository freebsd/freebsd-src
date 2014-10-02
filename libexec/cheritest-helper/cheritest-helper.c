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

#include <sys/types.h>
#include <sys/stat.h>

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <cheri/cheri_enter.h>
#include <cheri/cheri_fd.h>
#include <cheri/cheri_invoke.h>
#include <cheri/cheri_memcpy.h>
#include <cheri/cheri_system.h>

#include <inttypes.h>
#include <md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "cheritest-helper.h"

int	invoke(register_t op, register_t arg, size_t len,
	    struct cheri_object system_object, __capability char *data_input,
	    __capability char *data_output, struct cheri_object fd_object,
	    struct zstream_proxy *zspp);

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

	switch (op) {
	case CHERITEST_HELPER_OP_CP2_BOUND:
		cap = cheri_ptrperm(buffer, sizeof(buffer), CHERI_PERM_LOAD);
		ch = cap[N];
		return (ch);

	case CHERITEST_HELPER_OP_CP2_PERM_LOAD:
		cap = cheri_ptrperm(buffer, sizeof(buffer), CHERI_PERM_STORE);
		ch = cap[0];
		return (ch);

	case CHERITEST_HELPER_OP_CP2_PERM_STORE:
		cap = cheri_ptrperm(buffer, sizeof(buffer), CHERI_PERM_LOAD);
		cap[0] = 0;
		return (0);

	case CHERITEST_HELPER_OP_CP2_TAG:
		cap = cheri_ptrperm(buffer, sizeof(buffer), CHERI_PERM_LOAD);
		cap = cheri_ccleartag(cap);
		ch = cap[0];
		return (ch);

	case CHERITEST_HELPER_OP_CP2_SEAL:
		cap = cheri_ptrperm(buffer, sizeof(buffer), CHERI_PERM_LOAD);
		cap = cheri_seal(cap, cheri_maketype(&buffer,
		    CHERI_PERM_GLOBAL | CHERI_PERM_SEAL));
		ch = cap[0];
		return (ch);
	}
	return (0);
}

/*
 * NB: Can't use NULL to generate VM faults when compiling with pointers as
 * capabilities, as NULL generates an untagged capability.  Instead, use
 * near-NULL values.
 */
static int
invoke_vm_fault(register_t op)
{
	volatile char *chp;
	void (*fn)(void);
	char ch;

	chp = (void *)4;
	fn = (void *)4;
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
invoke_syscap(struct cheri_object system_object)
{

	return (cheri_invoke(system_object, 0, 0, 0, 0, 0, 0, 0, 0, NULL,
	    NULL, NULL, NULL, NULL, NULL, NULL, NULL));
}

static int
invoke_fd_fstat_c(struct cheri_object fd_object)
{
	struct cheri_fd_ret ret;
	struct stat sb;
	__capability void *sb_c;

	sb_c = cheri_ptr(&sb, sizeof(sb));
	ret = cheri_fd_fstat_c(fd_object, sb_c);
	return (ret.cfr_retval0);
}

static int
invoke_fd_lseek_c(struct cheri_object fd_object)
{
	struct cheri_fd_ret ret;

	ret = cheri_fd_lseek_c(fd_object, 0, SEEK_SET);
	return (ret.cfr_retval0);
}

static int
invoke_fd_read_c(struct cheri_object fd_object)
{
	struct cheri_fd_ret ret;
	char buf[10];
	__capability void *buf_c;

	buf_c = cheri_ptr(buf, sizeof(buf));
	ret = cheri_fd_read_c(fd_object, buf_c);
	return (ret.cfr_retval0);
}

static int
invoke_fd_write_c(struct cheri_object fd_object, __capability char *arg)
{

	return (cheri_fd_write_c(fd_object, arg).cfr_retval0);
}

static int
invoke_malloc(void)
{
	size_t i;
	void *tmp;
	const size_t sizes[] = {1, 2, 4, 8, 16, 32, 64, 128, 1024, 4096, 10000};

	for (i = 0; i < sizeof(sizes) / sizeof(*sizes); i++) {
		tmp = malloc(sizes[i]);
		if (tmp == NULL)
			return (-1);
		free(tmp);
	}
	return (0);
}

static int
invoke_clock_gettime(void)
{
	struct timespec t;

	if (clock_gettime(CLOCK_REALTIME, &t) == -1)
		return (-1);
	return (0);
}

static int
invoke_libcheri_userfn(register_t arg __unused, size_t len __unused,
    struct cheri_object system_object __unused)
{

	/*
	 * Argument passed to the cheritest-helper method turns into the
	 * method number for the underlying system class invocation.
	 */
	return (cheri_invoke(system_object, arg, len, 0, 0, 0, 0, 0, 0,
	    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
}

static int
invoke_libcheri_userfn_setstack(register_t arg,
    struct cheri_object system_object)
{
	int v;

	/*
	 * In the setstack test, ensure that execution of the return path via
	 * the sandbox has a visible effect that can be tested for.
	 */
	v = (cheri_invoke(system_object, CHERITEST_USERFN_SETSTACK, arg, 0, 0,
	    0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
	v += 10;
	return (v);
}

static __capability void *saved_capability;
static int
invoke_libcheri_save_capability_in_heap(__capability void *data_input)
{

	saved_capability = data_input;
	return (0);
}

/*
 * A series of variables whose values will be set via BSS, preinitialised data
 * in the binary, and a constructor, along with methods to query them.  Access
 * them through volatile pointers to ensure that constant memory accesses are
 * not optimised to instruction immediates in code generation, and that
 * constant initialisation is not converted from .data + a constructor to
 * static .data.
 */
static register_t cheritest_var_bss;
static register_t cheritest_var_data = CHERITEST_VALUE_DATA;
static register_t cheritest_var_constructor = CHERITEST_VALUE_INVALID;

static __attribute__ ((constructor)) void
cheritest_helper_var_constructor_init(void)
{
	volatile register_t *cheritest_var_constructorp =
	    &cheritest_var_constructor;

	*cheritest_var_constructorp = CHERITEST_VALUE_CONSTRUCTOR;
}

static register_t
invoke_get_var_bss(void)
{
	volatile register_t *cheritest_var_bssp = &cheritest_var_bss;

	return (*cheritest_var_bssp);
}

static register_t
invoke_get_var_data(void)
{
	volatile register_t *cheritest_var_datap = &cheritest_var_data;

	return (*cheritest_var_datap);
}

static register_t
invoke_get_var_constructor(void)
{
	volatile register_t *cheritest_var_constructorp =
	    &cheritest_var_constructor;

	return (*cheritest_var_constructorp);
}

static register_t
invoke_inflate(struct zstream_proxy *zspp)
{
	z_stream zs;

	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	zs.next_in = zspp->next_in;
	zs.avail_in = zspp->avail_in;
	zs.next_out = zspp->next_out;
	zs.avail_out = zspp->avail_out;
	if (inflateInit(&zs) != Z_OK) {
		printf("inflateInit");
		abort();
	}
	if (inflate(&zs, Z_FINISH) != Z_STREAM_END) {
		printf("inflate");
		abort();
	}
	if (inflateEnd(&zs) != Z_OK) {
		printf("inflateEnd");
		abort();
	}
	zspp->next_in = zs.next_in;
	zspp->avail_in = zs.avail_in;
	zspp->next_out = zs.next_out;
	zspp->avail_out = zs.avail_out;
	zspp->total_in = zs.total_in;
	zspp->total_out = zs.total_out;
	return (0);
}

/*
 * Demux of various cheritest test cases to run within a sandbox.
 */
static volatile int zero = 0;
int
invoke(register_t op, register_t arg, size_t len,
    struct cheri_object system_object, __capability char *data_input,
    __capability char *data_output, struct cheri_object fd_object,
    struct zstream_proxy* zspp)
{

	cheri_system_setup(system_object);

	switch (op) {
	case CHERITEST_HELPER_OP_MD5:
		return (invoke_md5(len, data_input, data_output));

	case CHERITEST_HELPER_OP_ABORT:
		abort();

	case CHERITEST_HELPER_OP_SPIN:
		while (1);

	case CHERITEST_HELPER_OP_CP2_BOUND:
	case CHERITEST_HELPER_OP_CP2_PERM_LOAD:
	case CHERITEST_HELPER_OP_CP2_PERM_STORE:
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
		return (1/(zero));

	case CHERITEST_HELPER_OP_SYSCAP:
		return (invoke_syscap(system_object));

	case CHERITEST_HELPER_OP_CS_HELLOWORLD:
		return (cheri_system_helloworld());

	case CHERITEST_HELPER_OP_CS_PUTS:
		return (cheri_system_puts(
		    (__capability char *)"sandbox cs_puts"));

	case CHERITEST_HELPER_OP_CS_PUTCHAR:
		return (cheri_system_putchar('C'));	/* Is for cookie. */

	case CHERITEST_HELPER_OP_PRINTF:
		return (printf("%s: printf in sandbox test\n", __func__));

	case CHERITEST_HELPER_OP_MALLOC:
		return (invoke_malloc());

	case CHERITEST_HELPER_OP_FD_FSTAT_C:
		return (invoke_fd_fstat_c(fd_object));

	case CHERITEST_HELPER_OP_FD_LSEEK_C:
		return (invoke_fd_lseek_c(fd_object));

	case CHERITEST_HELPER_OP_FD_READ_C:
		return (invoke_fd_read_c(fd_object));

	case CHERITEST_HELPER_OP_FD_WRITE_C:
		return (invoke_fd_write_c(fd_object, data_input));

	case CHERITEST_HELPER_OP_CS_CLOCK_GETTIME:
		return (invoke_clock_gettime());

	case CHERITEST_HELPER_LIBCHERI_USERFN:
		return (invoke_libcheri_userfn(arg, len, system_object));

	case CHERITEST_HELPER_LIBCHERI_USERFN_SETSTACK:
		return (invoke_libcheri_userfn_setstack(arg, system_object));

	case CHERITEST_HELPER_SAVE_CAPABILITY_IN_HEAP:
		return (invoke_libcheri_save_capability_in_heap(data_input));

	case CHERITEST_HELPER_GET_VAR_BSS:
		return (invoke_get_var_bss());

	case CHERITEST_HELPER_GET_VAR_DATA:
		return (invoke_get_var_data());

	case CHERITEST_HELPER_GET_VAR_CONSTRUCTOR:
		return (invoke_get_var_constructor());

	case CHERITEST_HELPER_OP_INFLATE:
		return (invoke_inflate(zspp));
	}
	return (-1);
}
