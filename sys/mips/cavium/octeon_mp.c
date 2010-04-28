/*-
 * Copyright (c) 2004-2010 Juli Mallett <jmallett@FreeBSD.org>
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
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <machine/hwfunc.h>
#include <machine/smp.h>

#include <mips/cavium/octeon_pcmap_regs.h>

unsigned octeon_ap_boot = ~0;

void
platform_ipi_send(int cpuid)
{
	oct_write64(OCTEON_CIU_MBOX_SETX(cpuid), 1);
	mips_wbflush();
}

void
platform_ipi_clear(void)
{
	uint64_t action;

	action = oct_read64(OCTEON_CIU_MBOX_CLRX(PCPU_GET(cpuid)));
	KASSERT(action == 1, ("unexpected IPIs: %#jx", (uintmax_t)action));
	oct_write64(OCTEON_CIU_MBOX_CLRX(PCPU_GET(cpuid)), action);
}

int
platform_ipi_intrnum(void)
{
	return (1);
}

void
platform_init_ap(int cpuid)
{
	/*
	 * Set the exception base.
	 */
	mips_wr_ebase(0x80000000 | cpuid);

	/*
	 * Set up interrupts, clear IPIs and unmask the IPI interrupt.
	 */
	octeon_ciu_reset();

	oct_write64(OCTEON_CIU_MBOX_CLRX(cpuid), 0xffffffff);
	ciu_enable_interrupts(cpuid, CIU_INT_1, CIU_EN_0, OCTEON_CIU_ENABLE_MBOX_INTR, CIU_MIPS_IP3);

	mips_wbflush();
}

int
platform_num_processors(void)
{
	return (fls(octeon_core_mask));
}

int
platform_start_ap(int cpuid)
{
	if (atomic_cmpset_32(&octeon_ap_boot, ~0, cpuid) == 0)
		return (-1);
	for (;;) {
		DELAY(1000);
		if (atomic_cmpset_32(&octeon_ap_boot, 0, ~0) != 0)
			return (0);
		printf("Waiting for cpu%d to start\n", cpuid);
	}
}
