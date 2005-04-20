/*-
 * Copyright (c) 2001 Doug Rabson
 * All rights reserved.
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <stdlib.h>
#include <stdio.h>

static u_int64_t ld2(char** pp, int add)
{
	u_int64_t p = (u_int64_t) *pp;
	u_int64_t v;
	__asm __volatile("ld2 %0=[%1]" : "=r" (v) : "r" (p));
	return v;
}

static u_int64_t ld4(char** pp, int add)
{
	u_int64_t p = (u_int64_t) *pp;
	u_int64_t v;
	__asm __volatile("ld4 %0=[%1]" : "=r" (v) : "r" (p));
	return v;
}

static u_int64_t ld8(char** pp, int add)
{
	u_int64_t p = (u_int64_t) *pp;
	u_int64_t v;
	__asm __volatile("ld8 %0=[%1]" : "=r" (v) : "r" (p));
	return v;
}

static u_int64_t ld8_reg(char** pp, int add)
{
	u_int64_t p = (u_int64_t) *pp;
	u_int64_t v;
	__asm __volatile("ld8 %0=[%1],%3"
			 : "=r" (v), "=r" (p)
			 : "1" (p), "r" (add));
	*pp = (char*) p;
	return v;
}

static u_int64_t ld8_8(char** pp, int add)
{
	u_int64_t p = (u_int64_t) *pp;
	u_int64_t v;
	__asm __volatile("ld8 %0=[%1],8"
			 : "=r" (v), "=r" (p)
			 : "1" (p));
	*pp = (char*) p;
	return v;
}

struct load_test {
	u_int64_t	(*ldfunc)(char**, int);
	int		off, add;
	u_int64_t	value;
};

char testbuf[16] = {
	0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
	0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff
};

struct load_test tests[] = {
	{ld8,		1,	0,	0x8877665544332211L},
	{ld8,		2,	0,	0x9988776655443322L},
	{ld4,		1,	0,	0x44332211L},
	{ld4,		2,	0,	0x55443322L},
	{ld2,		1,	0,	0x2211L},
	{ld8_reg,	1,	4,	0x8877665544332211L},
	{ld8_8,		1,	8,	0x8877665544332211L},
};

int
main(int argc, char** argv)
{
	int verbose = 0;
	int passed = 1;
	int i;

	if (argc == 2 && !strcmp(argv[1], "-v"))
		verbose = 1;

	for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
		struct load_test *tp = &tests[i];
		char* p;
		u_int64_t value;
		p = &testbuf[tp->off];
		value = tp->ldfunc(&p, tp->add);
		if (verbose)
			printf("read 0x%lx, expected 0x%lx\n",
			       value, tp->value);
		if (value != tp->value)
			passed = 0;
		if (p - &testbuf[tp->off] != tp->add) {
			printf("postincrement was %d, %d expected\n",
			       p - &testbuf[tp->off], tp->add);
			passed = 0;
		}
	}
	if (passed)
		printf("passed\n");
	else
		printf("failed\n");
}
