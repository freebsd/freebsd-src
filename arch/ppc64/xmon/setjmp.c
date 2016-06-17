/*
 * Copyright (C) 1996 Paul Mackerras.
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 * NB this file must be compiled with -O2.
 */

int
xmon_setjmp(long *buf)  /* NOTE: assert(sizeof(buf) > 184) */
{
	/* XXX should save fp regs as well */
	asm volatile (
	"mflr 0; std 0,0(%0)\n\
	 std	1,8(%0)\n\
	 std	2,16(%0)\n\
	 mfcr 0; std 0,24(%0)\n\
	 std	13,32(%0)\n\
	 std	14,40(%0)\n\
	 std	15,48(%0)\n\
	 std	16,56(%0)\n\
	 std	17,64(%0)\n\
	 std	18,72(%0)\n\
	 std	19,80(%0)\n\
	 std	20,88(%0)\n\
	 std	21,96(%0)\n\
	 std	22,104(%0)\n\
	 std	23,112(%0)\n\
	 std	24,120(%0)\n\
	 std	25,128(%0)\n\
	 std	26,136(%0)\n\
	 std	27,144(%0)\n\
	 std	28,152(%0)\n\
	 std	29,160(%0)\n\
	 std	30,168(%0)\n\
	 std	31,176(%0)\n\
	 " : : "r" (buf));
    return 0;
}

void
xmon_longjmp(long *buf, int val)
{
	if (val == 0)
		val = 1;
	asm volatile (
	"ld	13,32(%0)\n\
	 ld	14,40(%0)\n\
	 ld	15,48(%0)\n\
	 ld	16,56(%0)\n\
	 ld	17,64(%0)\n\
	 ld	18,72(%0)\n\
	 ld	19,80(%0)\n\
	 ld	20,88(%0)\n\
	 ld	21,96(%0)\n\
	 ld	22,104(%0)\n\
	 ld	23,112(%0)\n\
	 ld	24,120(%0)\n\
	 ld	25,128(%0)\n\
	 ld	26,136(%0)\n\
	 ld	27,144(%0)\n\
	 ld	28,152(%0)\n\
	 ld	29,160(%0)\n\
	 ld	30,168(%0)\n\
	 ld	31,176(%0)\n\
	 ld	0,24(%0)\n\
	 mtcrf	0x38,0\n\
	 ld	0,0(%0)\n\
	 ld	1,8(%0)\n\
	 ld	2,16(%0)\n\
	 mtlr	0\n\
	 mr	3,%1\n\
	 " : : "r" (buf), "r" (val));
}
