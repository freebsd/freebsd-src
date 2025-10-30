/*-
 * Copyright (c) 2024 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by the University of Cambridge Computer
 * Laboratory (Department of Computer Science and Technology) under Innovate
 * UK project 105694, "Digital Security by Design (DSbD) Technology Platform
 * Prototype".
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <vm/uma.h>

#include <machine/fpe.h>
#include <machine/reg.h>

static uma_zone_t fpu_save_area_zone;
static struct fpreg *fpu_initialstate;

void
fpe_enable(void)
{
	uint64_t reg;

	reg = SSTATUS_FS_INITIAL;

	csr_set(sstatus, reg);
}

void
fpe_disable(void)
{
	uint64_t mask;

	mask = SSTATUS_FS_MASK;

	csr_clear(sstatus, mask);
}

void
fpe_store(struct fpreg *regs)
{
	uint64_t fcsr, (*fp_x)[32][2];

	fp_x = &regs->fp_x;

	__asm __volatile(
	    "frcsr	%0		\n"
	    "fsd	f0, (16 * 0)(%2)\n"
	    "fsd	f1, (16 * 1)(%2)\n"
	    "fsd	f2, (16 * 2)(%2)\n"
	    "fsd	f3, (16 * 3)(%2)\n"
	    "fsd	f4, (16 * 4)(%2)\n"
	    "fsd	f5, (16 * 5)(%2)\n"
	    "fsd	f6, (16 * 6)(%2)\n"
	    "fsd	f7, (16 * 7)(%2)\n"
	    "fsd	f8, (16 * 8)(%2)\n"
	    "fsd	f9, (16 * 9)(%2)\n"
	    "fsd	f10, (16 * 10)(%2)\n"
	    "fsd	f11, (16 * 11)(%2)\n"
	    "fsd	f12, (16 * 12)(%2)\n"
	    "fsd	f13, (16 * 13)(%2)\n"
	    "fsd	f14, (16 * 14)(%2)\n"
	    "fsd	f15, (16 * 15)(%2)\n"
	    "fsd	f16, (16 * 16)(%2)\n"
	    "fsd	f17, (16 * 17)(%2)\n"
	    "fsd	f18, (16 * 18)(%2)\n"
	    "fsd	f19, (16 * 19)(%2)\n"
	    "fsd	f20, (16 * 20)(%2)\n"
	    "fsd	f21, (16 * 21)(%2)\n"
	    "fsd	f22, (16 * 22)(%2)\n"
	    "fsd	f23, (16 * 23)(%2)\n"
	    "fsd	f24, (16 * 24)(%2)\n"
	    "fsd	f25, (16 * 25)(%2)\n"
	    "fsd	f26, (16 * 26)(%2)\n"
	    "fsd	f27, (16 * 27)(%2)\n"
	    "fsd	f28, (16 * 28)(%2)\n"
	    "fsd	f29, (16 * 29)(%2)\n"
	    "fsd	f30, (16 * 30)(%2)\n"
	    "fsd	f31, (16 * 31)(%2)\n"
	    : "=&r"(fcsr), "=m"(*fp_x) : "r"(fp_x));

	regs->fp_fcsr = fcsr;
}

void
fpe_restore(struct fpreg *regs)
{
	uint64_t fcsr, (*fp_x)[32][2];

	fp_x = &regs->fp_x;
	fcsr = regs->fp_fcsr;

	__asm __volatile(
	    "fscsr	%0		\n"
	    "fld	f0, (16 * 0)(%1)\n"
	    "fld	f1, (16 * 1)(%1)\n"
	    "fld	f2, (16 * 2)(%1)\n"
	    "fld	f3, (16 * 3)(%1)\n"
	    "fld	f4, (16 * 4)(%1)\n"
	    "fld	f5, (16 * 5)(%1)\n"
	    "fld	f6, (16 * 6)(%1)\n"
	    "fld	f7, (16 * 7)(%1)\n"
	    "fld	f8, (16 * 8)(%1)\n"
	    "fld	f9, (16 * 9)(%1)\n"
	    "fld	f10, (16 * 10)(%1)\n"
	    "fld	f11, (16 * 11)(%1)\n"
	    "fld	f12, (16 * 12)(%1)\n"
	    "fld	f13, (16 * 13)(%1)\n"
	    "fld	f14, (16 * 14)(%1)\n"
	    "fld	f15, (16 * 15)(%1)\n"
	    "fld	f16, (16 * 16)(%1)\n"
	    "fld	f17, (16 * 17)(%1)\n"
	    "fld	f18, (16 * 18)(%1)\n"
	    "fld	f19, (16 * 19)(%1)\n"
	    "fld	f20, (16 * 20)(%1)\n"
	    "fld	f21, (16 * 21)(%1)\n"
	    "fld	f22, (16 * 22)(%1)\n"
	    "fld	f23, (16 * 23)(%1)\n"
	    "fld	f24, (16 * 24)(%1)\n"
	    "fld	f25, (16 * 25)(%1)\n"
	    "fld	f26, (16 * 26)(%1)\n"
	    "fld	f27, (16 * 27)(%1)\n"
	    "fld	f28, (16 * 28)(%1)\n"
	    "fld	f29, (16 * 29)(%1)\n"
	    "fld	f30, (16 * 30)(%1)\n"
	    "fld	f31, (16 * 31)(%1)\n"
	    :: "r"(fcsr), "r"(fp_x), "m"(*fp_x));
}

struct fpreg *
fpu_save_area_alloc(void)
{

	return (uma_zalloc(fpu_save_area_zone, M_WAITOK));
}

void
fpu_save_area_free(struct fpreg *fsa)
{

	uma_zfree(fpu_save_area_zone, fsa);
}

void
fpu_save_area_reset(struct fpreg *fsa)
{

	memcpy(fsa, fpu_initialstate, sizeof(*fsa));
}

static void
fpe_init(const void *dummy __unused)
{

	fpu_save_area_zone = uma_zcreate("FPE save area", sizeof(struct fpreg),
	    NULL, NULL, NULL, NULL, _Alignof(struct fpreg) - 1, 0);
	fpu_initialstate = uma_zalloc(fpu_save_area_zone, M_WAITOK | M_ZERO);

	fpe_enable();
	fpe_store(fpu_initialstate);
	fpe_disable();

	bzero(fpu_initialstate->fp_x, sizeof(fpu_initialstate->fp_x));
}

SYSINIT(fpe, SI_SUB_CPU, SI_ORDER_ANY, fpe_init, NULL);
