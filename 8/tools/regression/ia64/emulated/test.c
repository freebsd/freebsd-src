/*
 * Copyright (c) 2006 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
 * $FreeBSD$
 */

#include <sys/mman.h>
#include <stdio.h>
#include <string.h>

/* Supported long branch types */
#define	Call		1
#define	Cond		2

/* Supported predicates */
#define	False		1
#define	True		2

/* Supported variations */
#define	Backward	1
#define	Forward		2

#if TYPE == 0 || PRED == 0 || VAR == 0
#error Define TYPE, PRED and/or VAR
#endif

union bundle {
	unsigned char bytes[16];
	long double _align;
};

/*
 * Machine code of a bundle containing a long branch. The predicate of the
 * long branch is the result of the compare in the first slot.
 * The assembly of the bundle is:
 *	{	.mlx
 *		cmp.eq		p0,p15= <PREDICATE>,r0
 *	  (p15)	brl.few		<TARGET> ;;
 *	}
 * the predicate is written to bit 18:1
 * The branch target is written to bits 100:20, 48:39 and 123:1
 */
unsigned char mc_brl_cond[16] = {
	0x05, 0x00, 0x00, 0x00, 0x0f, 0x39,
	0x00, 0x00, 0x00, 0x00, 0x80, 0x07,
	0x00, 0x00, 0x00, 0xc0 
};

/*
 * Machine code of the epilogue of a typical function returning an integer.
 * The assembly of the epilogue is:
 *	{	.mib
 *		nop.m		0
 *		addl		r8 = <RETVAL>, r0
 *		br.ret.sptk.few b0 ;;
 *	}
 * The return value is written to bits 59:7, 73:9, 68:5, and 82:1.
 */
unsigned char mc_epilogue[16] = {
	0x11, 0x00, 0x00, 0x00, 0x01, 0x00,
	0x80, 0x00, 0x00, 0x00, 0x48, 0x80,
	0x00, 0x00, 0x84, 0x00
};

void
mc_patch(union bundle *b, unsigned long val, int start, int len)
{
	unsigned long mask;
	int bit, byte, run;

	byte = start >> 3;
	bit = start & 7;
	while (len) {
		run = ((len > (8 - bit)) ? (8 - bit) : len);
		mask = (1UL << run) - 1UL;
		b->bytes[byte] |= (val & mask) << bit;
		val >>= run;
		len -= run;
		byte++;
		bit = 0;
	}
}

void
assemble_brl_cond(union bundle *b, int pred, unsigned long tgt)
{
	unsigned long iprel;

	iprel = tgt - (unsigned long)b;
	memcpy(b->bytes, mc_brl_cond, sizeof(mc_brl_cond));
	mc_patch(b, pred ? 1 : 0, 18, 1);
	mc_patch(b, iprel >> 4, 100, 20);
	mc_patch(b, iprel >> 24, 48, 39);
	mc_patch(b, iprel >> 63, 123, 1);
}

void
assemble_epilogue(union bundle *b, int retval)
{
	memcpy(b->bytes, mc_epilogue, sizeof(mc_epilogue));
	mc_patch(b, retval, 59, 7);
	mc_patch(b, retval >> 7, 73, 9);
	mc_patch(b, retval >> 16, 68, 5);
	mc_patch(b, retval >> 21, 82, 1);
}

int
doit(void *addr)
{
	asm("mov b6 = %0; br.sptk b6;;" :: "r"(addr));
	return 1;
}

int
test_cond(int pred, union bundle *src, union bundle *dst)
{
	assemble_epilogue(dst, pred ? 0 : 2);
	assemble_brl_cond(src, pred ? 1 : 0, (unsigned long)dst);
	assemble_epilogue(src + 1, !pred ? 0 : 2);
	return doit(src);
}

int
main()
{
	static union bundle blob_low[2];
	union bundle *blob_high;
	void *addr;

	addr = (void *)0x7FFFFFFF00000000L;
	blob_high = mmap(addr, 32, PROT_EXEC | PROT_READ | PROT_WRITE,
	    MAP_ANON, -1, 0L);
	if (blob_high != addr)
		printf("NOTICE: blob_high is at %p, not at %p\n", blob_high,
		    addr);

#if TYPE == Call
	return (test_call(blob_high, blob_low));
#elif TYPE == Cond
  #if VAR == Forward
	return (test_cond(PRED - 1, blob_low, blob_high));
  #elif VAR == Backward
	return (test_cond(PRED - 1, blob_high, blob_low));
  #else
	return (1);
  #endif
#else
	return (1);
#endif
}
