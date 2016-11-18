/*-
 * Copyright (c) 2015-2016 Robert N. M. Watson
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

#include <cheri/cheri.h>
#include <cheri/cheric.h>

#include <machine/cpuregs.h>
#include <machine/sysarch.h>

#include <cheri/cheri_enter.h>
#include <cheri/cheri_fd.h>
#include <cheri/sandbox.h>

#include <cheritest-helper.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "cheritest.h"

/*
 * Check for high-precision bounds for a variety of small object sizes,
 * allocated from the stack.  These should be precise regardless of capability
 * compression, as the allocator promises to align things suitably.  Test both
 * static and dynamic allocation.
 */
static void
test_bounds_precise(__capability void *c, size_t expected_len)
{
	size_t len, offset;

	/* Confirm precise lower bound: offset of zero. */
	offset = cheri_getoffset(c);
	if (offset != 0)
		cheritest_failure_errx("offset (%jd) not zero", offset);

	/* Confirm precise upper bound: length of expected size for type. */
	len = cheri_getlen(c);
	if (len != expected_len)
		cheritest_failure_errx("length (%jd) not expected %jd", len,
		    expected_len);
	cheritest_success();
}

static void
test_bounds_stack_alloca(size_t len)
{
	__capability void *c = (__capability void *)alloca(len);

	test_bounds_precise(c, len);
}

void
test_bounds_stack_static_uint8(const struct cheri_test *ctp __unused)
{
	uint8_t u8;
	__capability uint8_t *u8p = (__capability uint8_t *)&u8;

	test_bounds_precise(u8p, sizeof(*u8p));
}

void
test_bounds_stack_dynamic_uint8(const struct cheri_test *ctp __unused)
{

	test_bounds_stack_alloca(sizeof(uint8_t));
}

void
test_bounds_stack_static_uint16(const struct cheri_test *ctp __unused)
{
	uint16_t u16;
	__capability uint16_t *u16p = (__capability uint16_t *)&u16;

	test_bounds_precise(u16p, sizeof(*u16p));
}

void
test_bounds_stack_dynamic_uint16(const struct cheri_test *ctp __unused)
{

	test_bounds_stack_alloca(sizeof(uint16_t));
}

void
test_bounds_stack_static_uint32(const struct cheri_test *ctp __unused)
{
	uint32_t u32;
	__capability uint32_t *u32p = (__capability uint32_t *)&u32;

	test_bounds_precise(u32p, sizeof(*u32p));
}

void
test_bounds_stack_dynamic_uint32(const struct cheri_test *ctp __unused)
{

	test_bounds_stack_alloca(sizeof(uint32_t));
}

void
test_bounds_stack_static_uint64(const struct cheri_test *ctp __unused)
{
	uint64_t u64;
	__capability uint64_t *u64p = (__capability uint64_t *)&u64;

	test_bounds_precise(u64p, sizeof(*u64p));
}

void
test_bounds_stack_dynamic_uint64(const struct cheri_test *ctp __unused)
{

	test_bounds_stack_alloca(sizeof(uint64_t));
}

void
test_bounds_stack_static_cap(const struct cheri_test *ctp __unused)
{
	__capability void *c;
	void * __capability * __capability cp =
	    (void * __capability * __capability)&c;

	test_bounds_precise(cp, sizeof(*cp));
}

void
test_bounds_stack_dynamic_cap(const struct cheri_test *ctp __unused)
{

	/*
	 * XXXRW: Really, we should request a bit more space so that, on
	 * 256-bit CHERI, we can guarantee 32-byte alignment, not the (likely)
	 * 16-byte alignment we would naturally get back on MIPS.
	 */
	test_bounds_stack_alloca(sizeof(__capability void *));
}

void
test_bounds_stack_static_16(const struct cheri_test *ctp __unused)
{
	uint8_t array[16];
	__capability uint8_t *arrayp = (__capability uint8_t *)&array;

	test_bounds_precise(arrayp, sizeof(array));
}

void
test_bounds_stack_dynamic_16(const struct cheri_test *ctp __unused)
{

	test_bounds_stack_alloca(16);
}

void
test_bounds_stack_static_32(const struct cheri_test *ctp __unused)
{
	uint8_t array[32];
	__capability uint8_t *arrayp = (__capability uint8_t *)&array;

	test_bounds_precise(arrayp, sizeof(array));
}

void
test_bounds_stack_dynamic_32(const struct cheri_test *ctp __unused)
{

	test_bounds_stack_alloca(32);
}

void
test_bounds_stack_static_64(const struct cheri_test *ctp __unused)
{
	uint8_t array[64];
	__capability uint8_t *arrayp = (__capability uint8_t *)&array;

	test_bounds_precise(arrayp, sizeof(array));
}

void
test_bounds_stack_dynamic_64(const struct cheri_test *ctp __unused)
{

	test_bounds_stack_alloca(64);
}

void
test_bounds_stack_static_128(const struct cheri_test *ctp __unused)
{
	uint8_t array[128];
	__capability uint8_t *arrayp = (__capability uint8_t *)&array;

	test_bounds_precise(arrayp, sizeof(array));
}

void
test_bounds_stack_dynamic_128(const struct cheri_test *ctp __unused)
{

	test_bounds_stack_alloca(128);
}

void
test_bounds_stack_static_256(const struct cheri_test *ctp __unused)
{
	uint8_t array[256];
	__capability uint8_t *arrayp = (__capability uint8_t *)&array;

	test_bounds_precise(arrayp, sizeof(array));
}

void
test_bounds_stack_dynamic_256(const struct cheri_test *ctp __unused)
{

	test_bounds_stack_alloca(256);
}

void
test_bounds_stack_static_512(const struct cheri_test *ctp __unused)
{
	uint8_t array[512];
	__capability uint8_t *arrayp = (__capability uint8_t *)&array;

	test_bounds_precise(arrayp, sizeof(array));
}

void
test_bounds_stack_dynamic_512(const struct cheri_test *ctp __unused)
{

	test_bounds_stack_alloca(512);
}

void
test_bounds_stack_static_1024(const struct cheri_test *ctp __unused)
{
	uint8_t array[1024];
	__capability uint8_t *arrayp = (__capability uint8_t *)&array;

	test_bounds_precise(arrayp, sizeof(array));
}

void
test_bounds_stack_dynamic_1024(const struct cheri_test *ctp __unused)
{

	test_bounds_stack_alloca(1024);
}

void
test_bounds_stack_static_2048(const struct cheri_test *ctp __unused)
{
	uint8_t array[2048];
	__capability uint8_t *arrayp = (__capability uint8_t *)&array;

	test_bounds_precise(arrayp, sizeof(array));
}

void
test_bounds_stack_dynamic_2048(const struct cheri_test *ctp __unused)
{

	test_bounds_stack_alloca(2048);
}

void
test_bounds_stack_static_4096(const struct cheri_test *ctp __unused)
{
	uint8_t array[4096];
	__capability uint8_t *arrayp = (__capability uint8_t *)&array;

	test_bounds_precise(arrayp, sizeof(array));
}

void
test_bounds_stack_dynamic_4096(const struct cheri_test *ctp __unused)
{

	test_bounds_stack_alloca(4096);
}

void
test_bounds_stack_static_8192(const struct cheri_test *ctp __unused)
{
	uint8_t array[8192];
	__capability uint8_t *arrayp = (__capability uint8_t *)&array;

	test_bounds_precise(arrayp, sizeof(array));
}

void
test_bounds_stack_dynamic_8192(const struct cheri_test *ctp __unused)
{

	test_bounds_stack_alloca(8192);
}

void
test_bounds_stack_static_16384(const struct cheri_test *ctp __unused)
{
	uint8_t array[16384];
	__capability uint8_t *arrayp = (__capability uint8_t *)&array;

	test_bounds_precise(arrayp, sizeof(array));
}

void
test_bounds_stack_dynamic_16384(const struct cheri_test *ctp __unused)
{

	test_bounds_stack_alloca(16384);
}

void
test_bounds_stack_static_32768(const struct cheri_test *ctp __unused)
{
	uint8_t array[32768];
	__capability uint8_t *arrayp = (__capability uint8_t *)&array;

	test_bounds_precise(arrayp, sizeof(array));
}

void
test_bounds_stack_dynamic_32768(const struct cheri_test *ctp __unused)
{

	test_bounds_stack_alloca(32768);
}

void
test_bounds_stack_static_65536(const struct cheri_test *ctp __unused)
{
	uint8_t array[65536];
	__capability uint8_t *arrayp = (__capability uint8_t *)&array;

	test_bounds_precise(arrayp, sizeof(array));
}

void
test_bounds_stack_dynamic_65536(const struct cheri_test *ctp __unused)
{

	test_bounds_stack_alloca(65536);
}

void
test_bounds_stack_static_131072(const struct cheri_test *ctp __unused)
{
	uint8_t array[131072];
	__capability uint8_t *arrayp = (__capability uint8_t *)&array;

	test_bounds_precise(arrayp, sizeof(array));
}

void
test_bounds_stack_dynamic_131072(const struct cheri_test *ctp __unused)
{

	test_bounds_stack_alloca(131072);
}

void
test_bounds_stack_static_262144(const struct cheri_test *ctp __unused)
{
	uint8_t array[262144];
	__capability uint8_t *arrayp = (__capability uint8_t *)&array;

	test_bounds_precise(arrayp, sizeof(array));
}

void
test_bounds_stack_dynamic_262144(const struct cheri_test *ctp __unused)
{

	test_bounds_stack_alloca(262144);
}

void
test_bounds_stack_static_524288(const struct cheri_test *ctp __unused)
{
	uint8_t array[524288];
	__capability uint8_t *arrayp = (__capability uint8_t *)&array;

	test_bounds_precise(arrayp, sizeof(array));
}

void
test_bounds_stack_dynamic_524288(const struct cheri_test *ctp __unused)
{

	test_bounds_stack_alloca(524288);
}

void
test_bounds_stack_static_1048576(const struct cheri_test *ctp __unused)
{
	uint8_t array[1048576];
	__capability uint8_t *arrayp = (__capability uint8_t *)&array;

	test_bounds_precise(arrayp, sizeof(array));
}

void
test_bounds_stack_dynamic_1048576(const struct cheri_test *ctp __unused)
{

	test_bounds_stack_alloca(1048576);
}
