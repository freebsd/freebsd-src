/*
 *  get_address.c
 *
 * Get the effective address from an FPU instruction.
 *
 *
 * Copyright (C) 1992, 1993  W. Metzenthen, 22 Parker St, Ormond,
 *                           Vic 3163, Australia.
 *                           E-mail apm233m@vaxc.cc.monash.edu.au
 * All rights reserved.
 *
 * This copyright notice covers the redistribution and use of the
 * FPU emulator developed by W. Metzenthen. It covers only its use
 * in the 386BSD operating system. Any other use is not permitted
 * under this copyright.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must include information specifying
 *    that source code for the emulator is freely available and include
 *    either:
 *      a) an offer to provide the source code for a nominal distribution
 *         fee, or
 *      b) list at least two alternative methods whereby the source
 *         can be obtained, e.g. a publically accessible bulletin board
 *         and an anonymous ftp site from which the software can be
 *         downloaded.
 * 3. All advertising materials specifically mentioning features or use of
 *    this emulator must acknowledge that it was developed by W. Metzenthen.
 * 4. The name of W. Metzenthen may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * W. METZENTHEN BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*---------------------------------------------------------------------------+
 | Note:                                                                     |
 |    The file contains code which accesses user memory.                     |
 |    Emulator static data may change when user memory is accessed, due to   |
 |    other processes using the emulator while swapping is in progress.      |
 +---------------------------------------------------------------------------*/

#include "param.h"
#include "proc.h"
#include "systm.h"
#include "machine/cpu.h"
#include "machine/pcb.h"
#include "machine/reg.h"

#include "fpu_emu.h"
#include "fpu_system.h"
#include "exception.h"

static int reg_offset[] = {
tEAX, tECX, tEDX, tEBX, tESP, tEBP, tESI, tEDI};
#define REG_(x) (*(((int*)FPU_info) + reg_offset[(x)]))

void   *FPU_data_address;


/* Decode the SIB byte. This function assumes mod != 0 */
static void *
sib(int mod)
{
	unsigned char ss, index, base;
	long    offset;

	REENTRANT_CHECK(OFF);
	base = fubyte((char *) FPU_EIP);	/* The SIB byte */
	REENTRANT_CHECK(ON);
	FPU_EIP++;
	ss = base >> 6;
	index = (base >> 3) & 7;
	base &= 7;

	if ((mod == 0) && (base == 5))
		offset = 0;	/* No base register */
	else
		offset = REG_(base);

	if (index == 4) {
		/* No index register */
		/* A non-zero ss is illegal */
		if (ss)
			EXCEPTION(EX_Invalid);
	} else {
		offset += (REG_(index)) << ss;
	}

	if (mod == 1) {
		/* 8 bit signed displacement */
		REENTRANT_CHECK(OFF);
		offset += (signed char) fubyte((char *) FPU_EIP);
		REENTRANT_CHECK(ON);
		FPU_EIP++;
	} else
		if (mod == 2 || base == 5) {	/* The second condition also
						 * has mod==0 */
			/* 32 bit displacment */
			REENTRANT_CHECK(OFF);
			offset += (signed) fuword((unsigned long *) FPU_EIP);
			REENTRANT_CHECK(ON);
			FPU_EIP += 4;
		}
	return (void *) offset;
}


/*
       MOD R/M byte:  MOD == 3 has a special use for the FPU
                      SIB byte used iff R/M = 100b

       7   6   5   4   3   2   1   0
       .....   .........   .........
        MOD    OPCODE(2)     R/M


       SIB byte

       7   6   5   4   3   2   1   0
       .....   .........   .........
        SS      INDEX        BASE

*/

void
get_address(unsigned char FPU_modrm)
{
	unsigned char mod;
	long   *cpu_reg_ptr;
	int     offset = 0;	/* Initialized just to stop compiler warnings. */

	mod = (FPU_modrm >> 6) & 3;

	if (FPU_rm == 4 && mod != 3) {
		FPU_data_address = sib(mod);
		return;
	}
	cpu_reg_ptr = (long *) &REG_(FPU_rm);
	switch (mod) {
	case 0:
		if (FPU_rm == 5) {
			/* Special case: disp32 */
			REENTRANT_CHECK(OFF);
			offset = fuword((unsigned long *) FPU_EIP);
			REENTRANT_CHECK(ON);
			FPU_EIP += 4;
			FPU_data_address = (void *) offset;
			return;
		} else {
			FPU_data_address = (void *) *cpu_reg_ptr;	/* Just return the
									 * contents of the cpu
									 * register */
			return;
		}
	case 1:
		/* 8 bit signed displacement */
		REENTRANT_CHECK(OFF);
		offset = (signed char) fubyte((char *) FPU_EIP);
		REENTRANT_CHECK(ON);
		FPU_EIP++;
		break;
	case 2:
		/* 32 bit displacement */
		REENTRANT_CHECK(OFF);
		offset = (signed) fuword((unsigned long *) FPU_EIP);
		REENTRANT_CHECK(ON);
		FPU_EIP += 4;
		break;
	case 3:
		/* Not legal for the FPU */
		EXCEPTION(EX_Invalid);
	}

	FPU_data_address = offset + (char *) *cpu_reg_ptr;
}
