/*-
 * Copyright (c) 2001 Jake Burkholder.
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

#ifndef	_MACHINE_TRAP_H_
#define	_MACHINE_TRAP_H_

#ifdef _KERNEL

#define	T_RESERVED	0x0
#define	T_POWER_ON	0x1
#define	T_WATCHDOG	0x2
#define	T_RESET_EXT	0x3
#define	T_RESET_SOFT	0x4
#define	T_RED_STATE	0x5
#define	T_INSN_EXCPTN	0x6
#define	T_INSN_ERROR	0x7
#define	T_INSN_ILLEGAL	0x8
#define	T_PRIV_OPCODE	0x9
#define	T_FP_DISABLED	0xa
#define	T_FP_IEEE	0xb
#define	T_FP_OTHER	0xc
#define	T_TAG_OVFLW	0xd
#define	T_DIVIDE	0xe
#define	T_DATA_EXCPTN	0xf
#define	T_DATA_ERROR	0x10
#define	T_ALIGN		0x11
#define	T_ALIGN_LDDF	0x12
#define	T_ALIGN_STDF	0x13
#define	T_PRIV_ACTION	0x14
#define	T_INTR		0x15
#define	T_WATCH_PHYS	0x16
#define	T_WATCH_VIRT	0x17
#define	T_ECC		0x18
#define	T_IMMU_MISS	0x19
#define	T_DMMU_MISS	0x1a
#define	T_DMMU_PROT	0x1b
#define	T_SPILL		0x1c
#define	T_FILL		0x1d
#define	T_FILL_RET	0x1e
#define	T_BREAKPOINT	0x1f
#define	T_SYSCALL	0x20
#define	T_RSTRWP_PHYS	0x21
#define	T_RSTRWP_VIRT	0x22
#define	T_SOFT		0x23
#define	T_KERNEL	0x40

#ifndef LOCORE
extern const char *trap_msg[];
#endif

#endif

#define	ST_BREAKPOINT	0x1
#define	ST_SYSCALL	0x9

#endif /* !_MACHINE_TRAP_H_ */
