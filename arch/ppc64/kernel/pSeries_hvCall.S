/*
 * arch/ppc64/kernel/pSeries_hvCall.S
 *
 *
 * This file contains the generic code to perform a call to the
 * pSeries LPAR hypervisor.
 * NOTE: this file will go away when we move to inline this work.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <linux/config.h>
#include <linux/sys.h>
#include <asm/unistd.h>
#include <asm/errno.h>
#include <asm/processor.h>
#include <asm/page.h>
#include <asm/cache.h>
#include "ppc_asm.h"
	
/*
 * hcall interface to pSeries LPAR
 */
#define HVSC	.long 0x44000022

/* long plpar_hcall(unsigned long opcode,	 R3 
		 unsigned long arg1,		 R4 
		 unsigned long arg2,		 R5 
		 unsigned long arg3,		 R6 
		 unsigned long arg4,		 R7 
		 unsigned long *out1,		 R8 
		 unsigned long *out2,		 R9
		 unsigned long *out3);		 R10
 */

	.text
_GLOBAL(plpar_hcall)
	mfcr	r0
	std	r0,-8(r1)
	stdu	r1,-32(r1)

        std     r8,-8(r1)       /* Save out ptrs. */
        std     r9,-16(r1)
        std     r10,-24(r1)
	
	HVSC                    /* invoke the hypervisor */

        ld      r10,-8(r1)      /* Fetch r4-r7 ret args. */
        std     r4,0(r10)
        ld      r10,-16(r1)
        std     r5,0(r10)
        ld      r10,-24(r1)
        std     r6,0(r10)

	ld	r1,0(r1)
	ld	r0,-8(r1)
	mtcrf	0xff,r0
	blr                     /* return r3 = status */


/* long plpar_hcall_4out(unsigned long opcode,	 R3
		 unsigned long arg1,		 R4
		 unsigned long arg2,		 R5
		 unsigned long arg3,		 R6
		 unsigned long arg4,		 R7
		 unsigned long *out1,	(r4)	 R8
		 unsigned long *out2,	(r5)	 R9
		 unsigned long *out3,   (r6)     R10
		 unsigned long *out4);	(r7)	 112(R1). From Parameter save area. 
 */
_GLOBAL(plpar_hcall_4out)
	mfcr	r0
	std	r0,-8(r1)
	ld      r14,112(r1)
	stdu	r1,-48(r1)

	std     r8,32(r1)       /* Save out ptrs. */
	std     r9,24(r1)
	std     r10,16(r1)
	std     r14,8(r1)

	HVSC                    /* invoke the hypervisor */

	ld      r14,32(r1)      /* Fetch r4-r7 ret args. */
	std     r4,0(r14)
	ld      r14,24(r1)
	std     r5,0(r14)
	ld      r14,16(r1)
	std     r6,0(r14)
	ld      r14,8(r1)
	std     r7,0(r14)

	ld	r1,0(r1) 
	ld	r0,-8(r1)
	mtcrf	0xff,r0
	blr                     /* return r3 = status */


/* Simple interface with no output values (other than status) */
_GLOBAL(plpar_hcall_norets)
	mfcr	r0
	std	r0,-8(r1)
	HVSC                    /* invoke the hypervisor */
	ld	r0,-8(r1)
	mtcrf	0xff,r0
	blr                     /* return r3 = status */


/* long plpar_hcall_8arg_2ret(unsigned long opcode,		 R3
				unsigned long arg1,		 R4
				unsigned long arg2,		 R5
				unsigned long arg3,		 R6
				unsigned long arg4,		 R7
				unsigned long arg5,		 R8
				unsigned long arg6,		 R9
				unsigned long arg7,		 R10
				unsigned long arg8,		 112(R1)
				unsigned long *out1);		 120(R1)

 */

	.text
_GLOBAL(plpar_hcall_8arg_2ret)
	mfcr	r0

	ld	r11, 112(r1) /* put arg8 and out1 in R11 and R12 */
	ld	r12, 120(r1)

	std	r0,-8(r1)
	stdu	r1,-32(r1)

	std	r12,-8(r1)      /* Save out ptr */

	HVSC                     /* invoke the hypervisor */

	ld	r10,-8(r1)      /* Fetch r4 ret arg */
	std	r4,0(r10)

	ld	r1,0(r1)
	ld	r0,-8(r1)
	mtcrf	0xff,r0
	blr                     /* return r3 = status */
