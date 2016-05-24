/*-
 * Copyright (c) 2015-2016 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
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

#ifndef _MACHINE_RISCVREG_H_
#define	_MACHINE_RISCVREG_H_

/* Machine mode requests */
#define	ECALL_MTIMECMP		0x01
#define	ECALL_CLEAR_PENDING	0x02
#define	ECALL_HTIF_CMD		0x03
#define	ECALL_HTIF_GET_ENTRY	0x04
#define	ECALL_MCPUID_GET	0x05
#define	ECALL_MIMPID_GET	0x06
#define	ECALL_SEND_IPI		0x07
#define	ECALL_CLEAR_IPI		0x08
#define	ECALL_HTIF_LOWPUTC	0x09
#define	ECALL_MIE_SET		0x0a
#define	ECALL_IO_IRQ_MASK	0x0b

#define	EXCP_SHIFT			0
#define	EXCP_MASK			(0xf << EXCP_SHIFT)
#define	EXCP_INSTR_ADDR_MISALIGNED	0
#define	EXCP_INSTR_ACCESS_FAULT		1
#define	EXCP_INSTR_ILLEGAL		2
#define	EXCP_INSTR_BREAKPOINT		3
#define	EXCP_LOAD_ADDR_MISALIGNED	4
#define	EXCP_LOAD_ACCESS_FAULT		5
#define	EXCP_STORE_ADDR_MISALIGNED	6
#define	EXCP_STORE_ACCESS_FAULT		7
#define	EXCP_UMODE_ENV_CALL		8
#define	EXCP_SMODE_ENV_CALL		9
#define	EXCP_HMODE_ENV_CALL		10
#define	EXCP_MMODE_ENV_CALL		11
#define	EXCP_INTR			(1 << 31)
#define	EXCP_INTR_SOFTWARE		0
#define	EXCP_INTR_TIMER			1
#define	EXCP_INTR_HTIF			2

#define	SSTATUS_IE			(1 << 0)
#define	SSTATUS_PIE			(1 << 3)
#define	SSTATUS_PS			(1 << 4)

#define	MSTATUS_MPRV		(1 << 16)
#define	MSTATUS_PRV_SHIFT	1
#define	MSTATUS_PRV1_SHIFT	4
#define	MSTATUS_PRV2_SHIFT	7
#define	MSTATUS_PRV_MASK	(0x3 << MSTATUS_PRV_SHIFT)
#define	MSTATUS_PRV_U		0	/* user */
#define	MSTATUS_PRV_S		1	/* supervisor */
#define	MSTATUS_PRV_H		2	/* hypervisor */
#define	MSTATUS_PRV_M		3	/* machine */

#define	MSTATUS_VM_SHIFT	17
#define	MSTATUS_VM_MASK		0x1f
#define	MSTATUS_VM_MBARE	0
#define	MSTATUS_VM_MBB		1
#define	MSTATUS_VM_MBBID	2
#define	MSTATUS_VM_SV32		8
#define	MSTATUS_VM_SV39		9
#define	MSTATUS_VM_SV48		10

#define	MIE_SSIE	(1 << 1)
#define	MIE_HSIE	(1 << 2)
#define	MIE_MSIE	(1 << 3)
#define	MIE_STIE	(1 << 5)
#define	MIE_HTIE	(1 << 6)
#define	MIE_MTIE	(1 << 7)

#define	MIP_SSIP	(1 << 1)
#define	MIP_HSIP	(1 << 2)
#define	MIP_MSIP	(1 << 3)
#define	MIP_STIP	(1 << 5)
#define	MIP_HTIP	(1 << 6)
#define	MIP_MTIP	(1 << 7)

#define	SR_IE		(1 << 0)
#define	SR_IE1		(1 << 3)
#define	SR_IE2		(1 << 6)
#define	SR_IE3		(1 << 9)

#define	SIE_SSIE	(1 << 1)
#define	SIE_STIE	(1 << 5)

/* Note: sip register has no SIP_STIP bit in Spike simulator */
#define	SIP_SSIP	(1 << 1)
#define	SIP_STIP	(1 << 5)

#define	NCSRS		4096
#define	CSR_IPI		0x783
#define	CSR_IO_IRQ	0x7c0	/* lowRISC only? */
#define	XLEN		8
#define	INSN_SIZE	4

#define	INSN_SIZE		4
#define	RISCV_INSN_NOP		0x00000013
#define	RISCV_INSN_BREAK	0x00100073
#define	RISCV_INSN_RET		0x00008067

#define	CSR_ZIMM(val)							\
	(__builtin_constant_p(val) && ((u_long)(val) < 32))

#define	csr_swap(csr, val)						\
({	if (CSR_ZIMM(val))  						\
		__asm __volatile("csrrwi %0, " #csr ", %1"		\
				: "=r" (val) : "i" (val));		\
	else 								\
		__asm __volatile("csrrw %0, " #csr ", %1"		\
				: "=r" (val) : "r" (val));		\
	val;								\
})

#define	csr_write(csr, val)						\
({	if (CSR_ZIMM(val)) 						\
		__asm __volatile("csrwi " #csr ", %0" :: "i" (val));	\
	else 								\
		__asm __volatile("csrw " #csr ", %0" ::  "r" (val));	\
})

#define	csr_set(csr, val)						\
({	if (CSR_ZIMM(val)) 						\
		__asm __volatile("csrsi " #csr ", %0" :: "i" (val));	\
	else								\
		__asm __volatile("csrs " #csr ", %0" :: "r" (val));	\
})

#define	csr_clear(csr, val)						\
({	if (CSR_ZIMM(val))						\
		__asm __volatile("csrci " #csr ", %0" :: "i" (val));	\
	else								\
		__asm __volatile("csrc " #csr ", %0" :: "r" (val));	\
})

#define	csr_read(csr)							\
({	u_long val;							\
	__asm __volatile("csrr %0, " #csr : "=r" (val));		\
	val;								\
})

#endif /* !_MACHINE_RISCVREG_H_ */
