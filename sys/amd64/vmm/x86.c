/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
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

#include <sys/types.h>

#include <machine/cpufunc.h>
#include <machine/specialreg.h>

#include "x86.h"

int
x86_emulate_cpuid(uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	unsigned int 	func, regs[4];

	func = *eax;

	cpuid_count(*eax, *ecx, regs);

	switch(func) {
		case CPUID_0000_0000:
		case CPUID_0000_0002:
		case CPUID_0000_0003:
		case CPUID_0000_0004:
		case CPUID_0000_000A:
			break;

		case CPUID_8000_0000:
		case CPUID_8000_0001:
		case CPUID_8000_0002:
		case CPUID_8000_0003:
		case CPUID_8000_0004:
		case CPUID_8000_0006:
		case CPUID_8000_0007:
		case CPUID_8000_0008:

			break;

		case CPUID_0000_0001:
			/*
			 * Override the APIC ID only in ebx
			 */
			regs[1] &= ~(CPUID_0000_0001_APICID_MASK);
			/*
			 * XXX fixme for MP case, set apicid properly for cpu. 
			 */
			regs[1] |= (0 << CPUID_0000_0001_APICID_SHIFT);

			/*
			 * Don't expose VMX capability.
			 * Advertise x2APIC capability.
			 */
			regs[2] &= ~CPUID_0000_0001_FEAT0_VMX;
			regs[2] |= CPUID2_X2APIC;

			/*
			 * Machine check handling is done in the host.
			 * Hide MTRR capability.
			 */
			regs[3] &= ~(CPUID_MCA | CPUID_MCE | CPUID_MTRR);

			break;

		case CPUID_0000_000B:
			/*
			 * XXXSMP fixme
			 * Processor topology enumeration
			 */
			regs[0] = 0;
			regs[1] = 0;
			regs[2] = *ecx & 0xff;
			regs[3] = 0;
			break;

		default:
			return (0);
	}

	*eax = regs[0];
	*ebx = regs[1];
	*ecx = regs[2];
	*edx = regs[3];
	return (1);
}

