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
#define USE_C_CAPS
#endif

#include <sys/types.h>
#include <sys/time.h>

#include <machine/cheri.h>
#include <machine/cheric.h>
#include <machine/cpuregs.h>

#include <cheri/sandbox.h>

#include <cheritest-helper.h>
#include <err.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "cheritest_sandbox.h"

#ifdef USE_C_CAPS
#define	CHERI_CAPREG_PRINT(crn) do {					\
	__capability void *cap = cheri_getreg(crn);			\
	printf("C%u tag %ju u %ju perms %04jx type %016jx\n", crn,	\
	    (uintmax_t)cheri_gettag(cap),				\
	    (uintmax_t)cheri_getunsealed(cap),				\
	    (uintmax_t)cheri_getperm(cap),				\
	    (uintmax_t)cheri_gettype(cap));				\
	printf("\tbase %016jx length %016jx\n",				\
	    (uintmax_t)cheri_getbase(cap),				\
	    (uintmax_t)cheri_getlen(cap));				\
} while (0)
#else
#define	CHERI_CAPREG_PRINT(crn) do {					\
	register_t c_tag;						\
	register_t c_unsealed, c_perms, c_otype, c_base, c_length;	\
									\
	CHERI_CGETTAG(c_tag, (crn));					\
	CHERI_CGETUNSEALED(c_unsealed, (crn));				\
	CHERI_CGETPERM(c_perms, (crn));					\
	CHERI_CGETBASE(c_base, (crn));					\
	CHERI_CGETTYPE(c_otype, (crn));					\
	CHERI_CGETLEN(c_length, (crn));					\
									\
	printf("C%u tag %u u %u perms %04jx type %016jx\n", crn,	\
	    (u_int)c_tag, (u_int)c_unsealed,				\
	    (uintmax_t)c_perms, (uintmax_t)c_otype);			\
	printf("\tbase %016jx length %016jx\n", (uintmax_t)c_base,	\
	    (uintmax_t)c_length);					\
} while (0)
#endif

static struct sandbox_class	*cheritest_classp;
static struct sandbox_object	*cheritest_objectp;

static void
usage(void)
{

	fprintf(stderr, "cheritest creturn\n");
	fprintf(stderr, "cheritest ccall_creturn\n");
	fprintf(stderr, "cheritest ccall_nop_creturn\n");
	fprintf(stderr, "cheritest copyregs\n");
	fprintf(stderr, "cheritest invoke_abort\n");
	fprintf(stderr, "cheritest invoke_cp2_bound\n");
	fprintf(stderr, "cheritest invoke_cp2_perm\n");
	fprintf(stderr, "cheritest invoke_cp2_seal\n");
	fprintf(stderr, "cheritest invoke_cp2_tag\n");
	fprintf(stderr, "cheritest invoke_divzero\n");
	fprintf(stderr, "cheritest invoke_helloworld\n");
	fprintf(stderr, "cheritest invoke_md5\n");
	fprintf(stderr, "cheritest invoke_malloc\n");
	fprintf(stderr, "cheritest invoke_printf\n");
	fprintf(stderr, "cheritest invoke_putchar\n");
	fprintf(stderr, "cheritest invoke_puts\n");
	fprintf(stderr, "cheritest invoke_spin\n");
	fprintf(stderr, "cheritest invoke_syscall\n");
	fprintf(stderr, "cheritest invoke_syscap\n");
	fprintf(stderr, "cheritest invoke_vm_rfault\n");
	fprintf(stderr, "cheritest invoke_vm_wfault\n");
	fprintf(stderr, "cheritest invoke_vm_xfault\n");
	fprintf(stderr, "cheritest listcausereg\n");
	fprintf(stderr, "cheritest listprivregs\n");
	fprintf(stderr, "cheritest listregs\n");
	fprintf(stderr, "cheritest overrun\n");
	fprintf(stderr, "cheritest sleep\n");
	exit(EX_USAGE);
}

static void
cheritest_overrun(void)
{
#ifdef USE_C_CAPS
#define	ARRAY_LEN	2
	char array[ARRAY_LEN];
	__capability char *arrayp = (__capability char *)array;
	int i;

	for (i = 0; i < ARRAY_LEN; i++)
		arrayp[i] = 0;
	arrayp[i] = 0;
#else
	/* Move a copy of $c0 into $c1. */
	CHERI_CMOVE(1, 0);

	/* Restrict segment length to 0. */
	CHERI_CSETLEN(1, 1, 0);

	/* Perform a byte store operation via $c1. */
	CHERI_CSB(0, 0, 0, 1);
#endif
}

/*
 * Configure $c1 and $c2 to enter a simple sandbox.  Not suitable for more
 * complex tests as it has no notion of configuring heaps/stacks/etc.  For
 * that, we use libcheri.
 */
static void
#ifdef USE_C_CAPS
cheritest_sandbox_setup(void *sandbox_base, void *sandbox_end,
    register_t sandbox_pc, __capability void **codecapp,
    __capability void **datacapp)
{
	__capability void *codecap, *datacap, *basecap;

	basecap = cheri_ptrtype(sandbox_base, (uintptr_t)sandbox_end -
	    (uintptr_t)sandbox_base, sandbox_pc);

	codecap = cheri_andperm(basecap, CHERI_PERM_EXECUTE |
	    CHERI_PERM_SEAL | CHERI_PERM_STORE_EPHEM_CAP);
	codecap = cheri_sealcode(codecap);

	datacap = cheri_andperm(basecap, CHERI_PERM_LOAD | CHERI_PERM_STORE |
	    CHERI_PERM_LOAD_CAP | CHERI_PERM_STORE_CAP);
	datacap = cheri_sealdata(datacap, basecap);

	*codecapp = codecap;
	*datacapp = datacap;
}
#else
cheritest_sandbox_setup(void *sandbox_base, void *sandbox_end,
    register_t sandbox_pc)
{
	/*-
	 * Construct a generic capability in $c3 that describes the combined
	 * code/data segment that we will seal.
	 *
	 * Derive from $c3 a code capability in $c1, and data capability in
	 * $c2, suitable for use with CCall.
	 */
	CHERI_CINCBASE(3, 0, sandbox_base);
	CHERI_CSETTYPE(3, 3, sandbox_pc);
	CHERI_CSETLEN(3, 3, (uintptr_t)sandbox_end - (uintptr_t)sandbox_base);

	/*
	 * Construct a code capability in $c1, derived from $c3, suitable for
	 * use with CCall.
	 */
	CHERI_CANDPERM(1, 3, CHERI_PERM_EXECUTE | CHERI_PERM_SEAL);
	CHERI_CSEALCODE(1, 1);

	/*
	 * Construct a data capability in $c2, derived from $c1 and $c3,
	 * suitable for use with CCall.
	 */
	CHERI_CANDPERM(2, 3, CHERI_PERM_LOAD | CHERI_PERM_STORE |
	    CHERI_PERM_LOAD_CAP | CHERI_PERM_STORE_CAP |
	    CHERI_PERM_STORE_EPHEM_CAP);
	CHERI_CSEALDATA(2, 2, 3);

	/*
	 * Clear $c3 which we no longer require.
	 */
	CHERI_CCLEARTAG(3);
}
#endif

static void
cheritest_creturn(void)
{

	CHERI_CRETURN();
}

static void
cheritest_ccall_creturn(void)
{
#ifdef USE_C_CAPS
	__capability void *codecap, *datacap;

	cheritest_sandbox_setup(&sandbox_creturn, &sandbox_creturn_end, 0,
	    &codecap, &datacap);
	cheritest_ccall(codecap, datacap);
#else
	cheritest_sandbox_setup(&sandbox_creturn, &sandbox_creturn_end, 0);
	cheritest_ccall();
#endif
}

static void
cheritest_ccall_nop_creturn(void)
{
#ifdef USE_C_CAPS
	__capability void *codecap, *datacap;

	cheritest_sandbox_setup(&sandbox_nop_creturn,
	    &sandbox_nop_creturn_end, 0, &codecap, &datacap);
	cheritest_ccall(codecap, datacap);
#else
	cheritest_sandbox_setup(&sandbox_nop_creturn,
	    &sandbox_nop_creturn_end, 0);
	cheritest_ccall();
#endif
}

static void
cheritest_copyregs(void)
{

	CHERI_CMOVE(2, 0);
	CHERI_CMOVE(3, 0);
	CHERI_CMOVE(4, 0);
	CHERI_CMOVE(5, 0);
	CHERI_CMOVE(6, 0);
	CHERI_CMOVE(7, 0);
}

static void
cheritest_listcausereg(void)
{
	register_t cause;

	printf("CP2 cause register:\n");
#ifdef USE_C_CAPS
	cause = cheri_getcause();
#else
	CHERI_CGETCAUSE(cause);
#endif
	printf("Cause: %ju\n", (uintmax_t)cause);
}

static void
cheritest_listprivregs(void)
{

	/*
	 * Because of the assembly generated by CP2_CR_GET(), can't use a loop
	 * -- register numbers must be available at compile-time.
	 */
	printf("CP2 privileged registers:\n");
	CHERI_CAPREG_PRINT(27);
	CHERI_CAPREG_PRINT(28);
	CHERI_CAPREG_PRINT(29);
	CHERI_CAPREG_PRINT(30);
	CHERI_CAPREG_PRINT(31);
}

static void
cheritest_listregs(void)
{

	/*
	 * Because of the assembly generated by CP2_CR_GET(), can't use a loop
	 * -- register numbers must be available at compile-time.
	 */
	printf("CP2 registers:\n");
	CHERI_CAPREG_PRINT(0);
	CHERI_CAPREG_PRINT(1);
	CHERI_CAPREG_PRINT(2);
	CHERI_CAPREG_PRINT(3);
	CHERI_CAPREG_PRINT(4);
	CHERI_CAPREG_PRINT(5);
	CHERI_CAPREG_PRINT(6);
	CHERI_CAPREG_PRINT(7);
	CHERI_CAPREG_PRINT(8);
	CHERI_CAPREG_PRINT(9);
	CHERI_CAPREG_PRINT(10);
	CHERI_CAPREG_PRINT(11);
	CHERI_CAPREG_PRINT(12);
	CHERI_CAPREG_PRINT(13);
	CHERI_CAPREG_PRINT(14);
	CHERI_CAPREG_PRINT(15);
	CHERI_CAPREG_PRINT(16);
	CHERI_CAPREG_PRINT(17);
	CHERI_CAPREG_PRINT(18);
	CHERI_CAPREG_PRINT(19);
	CHERI_CAPREG_PRINT(20);
	CHERI_CAPREG_PRINT(21);
	CHERI_CAPREG_PRINT(22);
	CHERI_CAPREG_PRINT(23);
	CHERI_CAPREG_PRINT(24);
	CHERI_CAPREG_PRINT(25);
	CHERI_CAPREG_PRINT(26);
}

static void
cheritest_invoke_simple_op(int op)
{
	register_t v;

#ifdef USE_C_CAPS
	v = sandbox_object_cinvoke(cheritest_objectp, op, 0, 0, 0, 0, 0, 0, 0,
	    sandbox_object_getsystemobject(cheritest_objectp).co_codecap,
	    sandbox_object_getsystemobject(cheritest_objectp).co_datacap,
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(),
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap());
#else
	struct chericap cheri_system_object;

	cheri_system_object =
	    sandbox_object_getsystemobject(cheritest_objectp);
	v = sandbox_object_invoke(cheritest_objectp, op, 0, 0, 0, 0, 0, 0, 0,
	    &cheri_system_object.co_codecap, &cheri_system_object.co_datacap,
	    NULL, NULL, NULL, NULL, NULL, NULL);
#endif
	printf("%s: sandbox returned %jd\n", __func__, (intmax_t)v);
}

/*
 * XXXRW: c1 and c2 were not getting properly aligned when placed in the
 * stack.  Odd.
 */
static char md5string[] = "hello world";
#ifndef USE_C_CAPS
static struct chericap c5, c6;
#endif

static void
cheritest_invoke_md5(void)
{
#ifdef USE_C_CAPS
	__capability void *md5cap, *bufcap, *cclear;
#else
	struct chericap cheri_system_object;
#endif
	char buf[33];
	register_t v;

#ifdef USE_C_CAPS
	cclear = cheri_zerocap();
	md5cap = cheri_ptrperm(md5string, sizeof(md5string), CHERI_PERM_LOAD);
	bufcap = cheri_ptrperm(buf, sizeof(buf), CHERI_PERM_STORE);

	v = sandbox_object_cinvoke(cheritest_objectp, CHERITEST_HELPER_OP_MD5,
	    strlen(md5string), 0, 0, 0, 0, 0, 0,
	    sandbox_object_getsystemobject(cheritest_objectp).co_codecap,
	    sandbox_object_getsystemobject(cheritest_objectp).co_datacap,
	    md5cap, bufcap, cclear, cclear, cclear, cclear);
#else
	CHERI_CINCBASE(10, 0, &md5string);
	CHERI_CSETLEN(10, 10, strlen(md5string));
	CHERI_CANDPERM(10, 10, CHERI_PERM_LOAD);
	CHERI_CSC(10, 0, &c5, 0);

	CHERI_CINCBASE(10, 0, &buf);
	CHERI_CSETLEN(10, 10, sizeof(buf));
	CHERI_CANDPERM(10, 10, CHERI_PERM_STORE);
	CHERI_CSC(10, 0, &c6, 0);

	cheri_system_object =
	    sandbox_object_getsystemobject(cheritest_objectp);
	v = sandbox_object_invoke(cheritest_objectp, CHERITEST_HELPER_OP_MD5,
	    strlen(md5string), 0, 0, 0, 0, 0, 0,
	    &cheri_system_object.co_codecap, &cheri_system_object.co_datacap,
	    &c5, &c6, NULL, NULL, NULL, NULL);
#endif

	printf("%s: sandbox returned %ju\n", __func__, (uintmax_t)v);
	buf[32] = '\0';
	printf("MD5 checksum of '%s' is %s\n", md5string, buf);
}

static int
cheritest_libcheri_setup(void)
{

	if (sandbox_class_new("/usr/libexec/cheritest-helper.bin",
	    4*1024*1024, &cheritest_classp) < 0)
		return (-1);
	if (sandbox_object_new(cheritest_classp, &cheritest_objectp) < 0)
		return (-1);
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_MD5, "md5");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_ABORT, "abort");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_SPIN, "spin");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_CP2_BOUND, "cp2_bound");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_CP2_PERM, "cp2_perm");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_CP2_TAG, "cp2_tag");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_CP2_SEAL, "cp2_seal");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_CS_HELLOWORLD, "helloworld");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_CS_PUTS, "puts");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_PRINTF, "printf");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_VM_RFAULT, "vm_rfault");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_VM_WFAULT, "vm_wfault");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_VM_XFAULT, "vm_xfault");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_SYSCALL, "syscall");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_DIVZERO, "divzero");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_SYSCAP, "syscap");
	(void)sandbox_class_method_declare(cheritest_classp,
	    CHERITEST_HELPER_OP_MALLOC, "malloc");
	return (0);
}

static void
cheritest_libcheri_destroy(void)
{

	sandbox_object_destroy(cheritest_objectp);
	sandbox_class_destroy(cheritest_classp);
}

int
main(__unused int argc, __unused char *argv[])
{
	int i, opt;

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

	cheritest_libcheri_setup();
	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "listcausereg") == 0)
			cheritest_listcausereg();
		else if (strcmp(argv[i], "listprivregs") == 0)
			cheritest_listprivregs();
		else if (strcmp(argv[i], "listregs") == 0)
			cheritest_listregs();
		else if (strcmp(argv[i], "ccall_creturn") == 0)
			cheritest_ccall_creturn();
		else if (strcmp(argv[i], "ccall_nop_creturn") == 0)
			cheritest_ccall_nop_creturn();
		else if (strcmp(argv[i], "creturn") == 0)
			cheritest_creturn();
		else if (strcmp(argv[i], "copyregs") == 0)
			cheritest_copyregs();
		else if (strcmp(argv[i], "invoke_abort") == 0)
			cheritest_invoke_simple_op(CHERITEST_HELPER_OP_ABORT);
		else if (strcmp(argv[i], "invoke_cp2_bound") == 0)
			cheritest_invoke_simple_op(
			    CHERITEST_HELPER_OP_CP2_BOUND);
		else if (strcmp(argv[i], "invoke_cp2_perm") == 0)
			cheritest_invoke_simple_op(
			    CHERITEST_HELPER_OP_CP2_PERM);
		else if (strcmp(argv[i], "invoke_cp2_tag") == 0)
			cheritest_invoke_simple_op(
			    CHERITEST_HELPER_OP_CP2_TAG);
		else if (strcmp(argv[i], "invoke_cp2_seal") == 0)
			cheritest_invoke_simple_op(
			    CHERITEST_HELPER_OP_CP2_SEAL);
		else if (strcmp(argv[i], "invoke_divzero") == 0)
			cheritest_invoke_simple_op(
			    CHERITEST_HELPER_OP_DIVZERO);
		else if (strcmp(argv[i], "invoke_helloworld") == 0)
			cheritest_invoke_simple_op(
			    CHERITEST_HELPER_OP_CS_HELLOWORLD);
		else if (strcmp(argv[i], "invoke_md5") == 0)
			cheritest_invoke_md5();
		else if (strcmp(argv[i], "invoke_malloc") == 0)
			cheritest_invoke_simple_op(
			    CHERITEST_HELPER_OP_MALLOC);
		else if (strcmp(argv[i], "invoke_printf") == 0)
			cheritest_invoke_simple_op(
			    CHERITEST_HELPER_OP_PRINTF);
		else if (strcmp(argv[i], "invoke_putchar") == 0)
			cheritest_invoke_simple_op(
			    CHERITEST_HELPER_OP_CS_PUTCHAR);
		else if (strcmp(argv[i], "invoke_puts") == 0)
			cheritest_invoke_simple_op(
			    CHERITEST_HELPER_OP_CS_PUTS);
		else if (strcmp(argv[i], "invoke_spin") == 0)
			cheritest_invoke_simple_op(
			    CHERITEST_HELPER_OP_SPIN);
		else if (strcmp(argv[i], "invoke_syscall") == 0)
			cheritest_invoke_simple_op(
			    CHERITEST_HELPER_OP_SYSCALL);
		else if (strcmp(argv[i], "invoke_syscap") == 0)
			cheritest_invoke_simple_op(
			    CHERITEST_HELPER_OP_SYSCAP);
		else if (strcmp(argv[i], "invoke_vm_rfault") == 0)
			cheritest_invoke_simple_op(
			    CHERITEST_HELPER_OP_VM_RFAULT);
		else if (strcmp(argv[i], "invoke_vm_wfault") == 0)
			cheritest_invoke_simple_op(
			    CHERITEST_HELPER_OP_VM_WFAULT);
		else if (strcmp(argv[i], "invoke_vm_xfault") == 0)
			cheritest_invoke_simple_op(
			    CHERITEST_HELPER_OP_VM_XFAULT);
		else if (strcmp(argv[i], "overrun") == 0)
			cheritest_overrun();
		else if (strcmp(argv[i], "sleep") == 0)
			sleep(10);
		else
			usage();
	}
	cheritest_libcheri_destroy();
	exit(EX_OK);
}
