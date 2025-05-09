/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 The FreeBSD Foundation
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#define _WANT_KERNEL_ERRNO 1
#include <sys/errno.h>
#include <sys/tree.h>
#include <machine/armreg.h>
#include <machine/vmm.h>
#include <machine/vmm_instruction_emul.h>
#include <vmmapi.h>

#include "mem.h"

int
mmio_handle_non_backed_mem(struct vcpu *vcpu, uint64_t paddr,
    struct mem_range **mr_paramp __unused)
{
	int err;
	uint64_t spsr, esr;

	if (vm_get_register(vcpu, VM_REG_GUEST_CPSR, &spsr) == -1)
		return (errno);
	if ((spsr & PSR_M_MASK) == PSR_M_EL0t)
		esr = EXCP_DATA_ABORT_L << ESR_ELx_EC_SHIFT;
	else
		esr = EXCP_DATA_ABORT << ESR_ELx_EC_SHIFT;
	esr |= ESR_ELx_IL | ISS_DATA_DFSC_EXT;
	err = vm_inject_exception(vcpu, esr, paddr);
	return (err != 0 ? err : EJUSTRETURN);
}
