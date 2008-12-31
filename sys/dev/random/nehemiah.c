/*-
 * Copyright (c) 2004 Mark R V Murray
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/random/nehemiah.c,v 1.4.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/time.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/selinfo.h>
#include <sys/systm.h>

#include <dev/random/randomdev.h>

#define RANDOM_BLOCK_SIZE	256
#define CIPHER_BLOCK_SIZE	16

static void random_nehemiah_init(void);
static void random_nehemiah_deinit(void);
static int random_nehemiah_read(void *, int);

struct random_systat random_nehemiah = {
	.ident = "Hardware, VIA Nehemiah",
	.init = random_nehemiah_init,
	.deinit = random_nehemiah_deinit,
	.read = random_nehemiah_read,
	.write = (random_write_func_t *)random_null_func,
	.reseed = (random_reseed_func_t *)random_null_func,
	.seeded = 1,
};

union VIA_ACE_CW {
	uint64_t raw;
	struct {
		u_int round_count : 4;
		u_int algorithm_type : 3;
		u_int key_generation_type : 1;
		u_int intermediate : 1;
		u_int decrypt : 1;
		u_int key_size : 2;
		u_int filler0 : 20;
		u_int filler1 : 32;
		u_int filler2 : 32;
		u_int filler3 : 32;
	} field;
};

/* The extra 7 is to allow an 8-byte write on the last byte of the
 * arrays.  The ACE wants the AES data 16-byte/128-bit aligned, and
 * it _always_ writes n*64 bits. The RNG does not care about alignment,
 * and it always writes n*32 bits or n*64 bits.
 */
static uint8_t key[CIPHER_BLOCK_SIZE+7]	__aligned(16);
static uint8_t iv[CIPHER_BLOCK_SIZE+7]	__aligned(16);
static uint8_t in[RANDOM_BLOCK_SIZE+7]	__aligned(16);
static uint8_t out[RANDOM_BLOCK_SIZE+7]	__aligned(16);

static union VIA_ACE_CW acw		__aligned(16);

static struct mtx random_nehemiah_mtx;

/* ARGSUSED */
static __inline size_t
VIA_RNG_store(void *buf)
{
#ifdef __GNUCLIKE_ASM
	uint32_t retval = 0;
	uint32_t rate = 0;

	/* The .byte line is really VIA C3 "xstore" instruction */
	__asm __volatile(
		"movl	$0,%%edx		\n\t"
		".byte	0x0f, 0xa7, 0xc0"
			: "=a" (retval), "+d" (rate), "+D" (buf)
			:
			: "memory"
	);
	if (rate == 0)
		return (retval&0x1f);
#endif
	return (0);
}

/* ARGSUSED */
static __inline void
VIA_ACE_cbc(void *in, void *out, size_t count, void *key, union VIA_ACE_CW *cw, void *iv)
{
#ifdef __GNUCLIKE_ASM
	/* The .byte line is really VIA C3 "xcrypt-cbc" instruction */
	__asm __volatile(
		"pushf				\n\t"
		"popf				\n\t"
		"rep				\n\t"
		".byte	0x0f, 0xa7, 0xc8"
			: "+a" (iv), "+c" (count), "+D" (out), "+S" (in)
			: "b" (key), "d" (cw)
			: "cc", "memory"
		);
#endif
}

static void
random_nehemiah_init(void)
{
	acw.raw = 0ULL;
	acw.field.round_count = 12;
	
	mtx_init(&random_nehemiah_mtx, "random nehemiah", NULL, MTX_DEF);
}

void
random_nehemiah_deinit(void)
{
	mtx_destroy(&random_nehemiah_mtx);
}

static int
random_nehemiah_read(void *buf, int c)
{
	int i;
	size_t count, ret;
	uint8_t *p;

	mtx_lock(&random_nehemiah_mtx);

	/* Get a random AES key */
	count = 0;
	p = key;
	do {
		ret = VIA_RNG_store(p);
		p += ret;
		count += ret;
	} while (count < CIPHER_BLOCK_SIZE);

	/* Get a random AES IV */
	count = 0;
	p = iv;
	do {
		ret = VIA_RNG_store(p);
		p += ret;
		count += ret;
	} while (count < CIPHER_BLOCK_SIZE);

	/* Get a block of random bytes */
	count = 0;
	p = in;
	do {
		ret = VIA_RNG_store(p);
		p += ret;
		count += ret;
	} while (count < RANDOM_BLOCK_SIZE);

	/* This is a Davies-Meyer hash of the most paranoid variety; the
	 * key, IV and the data are all read directly from the hardware RNG.
	 * All of these are used precisely once.
	 */
	VIA_ACE_cbc(in, out, RANDOM_BLOCK_SIZE/CIPHER_BLOCK_SIZE,
	    key, &acw, iv);
	for (i = 0; i < RANDOM_BLOCK_SIZE; i++)
		out[i] ^= in[i];

	c = MIN(RANDOM_BLOCK_SIZE, c);
	memcpy(buf, out, (size_t)c);

	mtx_unlock(&random_nehemiah_mtx);
	return (c);
}
