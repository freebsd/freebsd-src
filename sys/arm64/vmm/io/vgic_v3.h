/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2015 Mihai Carabas <mihai.carabas@gmail.com>
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _VMM_VGIC_V3_H_
#define	_VMM_VGIC_V3_H_

#define VGIC_ICH_LR_NUM_MAX	16
#define	VGIC_ICH_APR_NUM_MAX	4

/* Registers accessed by EL2 */
struct vgic_v3_regs {
	uint32_t	ich_eisr_el2;	/* End of Interrupt Status Register */
	uint32_t	ich_elrsr_el2;	/* Empty List register Status Register (ICH_ELRSR_EL2) */
	uint32_t	ich_hcr_el2;	/* Hyp Control Register */
	uint32_t	ich_misr_el2;	/* Maintenance Interrupt State Register */
	uint32_t	ich_vmcr_el2;	/* Virtual Machine Control Register */

	/*
	 * The List Registers are part of the VM context and are modified on a
	 * world switch. They need to be allocated statically so they are
	 * mapped in the EL2 translation tables when struct hypctx is mapped.
	 */
	uint64_t	ich_lr_el2[VGIC_ICH_LR_NUM_MAX];
	uint16_t	ich_lr_num;

	/* Active Priorities Registers for Group 0 and 1 interrupts */
	uint16_t	ich_apr_num;
	uint32_t	ich_ap0r_el2[VGIC_ICH_APR_NUM_MAX];
	uint32_t	ich_ap1r_el2[VGIC_ICH_APR_NUM_MAX];
};

#endif /* !_VMM_VGIC_V3_H_ */
