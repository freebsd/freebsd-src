/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2003 Peter Wemm.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _MACHINE_FRAME_H_
#define _MACHINE_FRAME_H_ 1

/*
 * System stack frames.
 * struct trapframe is known to and used by kernel debuggers.
 */

#ifdef __i386__
/*
 * Exception/Trap Stack Frame
 */

struct trapframe {
	int	tf_fs;
	int	tf_es;
	int	tf_ds;
	int	tf_edi;
	int	tf_esi;
	int	tf_ebp;
	int	tf_isp;
	int	tf_ebx;
	int	tf_edx;
	int	tf_ecx;
	int	tf_eax;
	int	tf_trapno;
	/* below portion defined in 386 hardware */
	int	tf_err;
	int	tf_eip;
	int	tf_cs;
	int	tf_eflags;
	/* below only when crossing rings (user to kernel) */
	int	tf_esp;
	int	tf_ss;
};

/* Superset of trap frame, for traps from virtual-8086 mode */

struct trapframe_vm86 {
	int	tf_fs;
	int	tf_es;
	int	tf_ds;
	int	tf_edi;
	int	tf_esi;
	int	tf_ebp;
	int	tf_isp;
	int	tf_ebx;
	int	tf_edx;
	int	tf_ecx;
	int	tf_eax;
	int	tf_trapno;
	/* below portion defined in 386 hardware */
	int	tf_err;
	int	tf_eip;
	int	tf_cs;
	int	tf_eflags;
	/* below only when crossing rings (user (including vm86) to kernel) */
	int	tf_esp;
	int	tf_ss;
	/* below only when crossing from vm86 mode to kernel */
	int	tf_vm86_es;
	int	tf_vm86_ds;
	int	tf_vm86_fs;
	int	tf_vm86_gs;
};

/*
 * This alias for the MI TRAPF_USERMODE() should be used when we don't
 * care about user mode itself, but need to know if a frame has stack
 * registers.  The difference is only logical, but on i386 the logic
 * for using TRAPF_USERMODE() is complicated by sometimes treating vm86
 * bioscall mode (which is a special ring 3 user mode) as kernel mode.
 */
#define	TF_HAS_STACKREGS(tf)	TRAPF_USERMODE(tf)
#endif /* __i386__ */

#ifdef __amd64__
/*
 * Exception/Trap Stack Frame
 *
 * The ordering of this is specifically so that we can take first 6
 * the syscall arguments directly from the beginning of the frame.
 */

struct trapframe {
	register_t	tf_rdi;
	register_t	tf_rsi;
	register_t	tf_rdx;
	register_t	tf_rcx;
	register_t	tf_r8;
	register_t	tf_r9;
	register_t	tf_rax;
	register_t	tf_rbx;
	register_t	tf_rbp;
	register_t	tf_r10;
	register_t	tf_r11;
	register_t	tf_r12;
	register_t	tf_r13;
	register_t	tf_r14;
	register_t	tf_r15;
	uint32_t	tf_trapno;
	uint16_t	tf_fs;
	uint16_t	tf_gs;
	register_t	tf_addr;
	uint32_t	tf_flags;
	uint16_t	tf_es;
	uint16_t	tf_ds;
	/* below portion defined in hardware */
	register_t	tf_err;
	register_t	tf_rip;
	uint16_t	tf_cs;
	uint16_t	tf_fred_evinfo3;
	uint32_t	tf_fred_zero2;
	register_t	tf_rflags;
	/* the amd64 frame always has the stack registers */
	register_t	tf_rsp;
	uint16_t	tf_ss;
	uint16_t	tf_fred_evinfo1;
	uint32_t	tf_fred_evinfo2;
};

struct trapframe_fred {
	struct trapframe tf_idt;
	/* two long words added by FRED */
	uint64_t	tf_fred_evdata;
	uint64_t	tf_fred_zero1;
};

#define	TF_FRED_EVDATA_B0	0x0000000000000001ull	/* %dr6 B0 */
#define	TF_FRED_EVDATA_B1	0x0000000000000002ull
#define	TF_FRED_EVDATA_B2	0x0000000000000004ull
#define	TF_FRED_EVDATA_B3	0x0000000000000008ull
#define	TF_FRED_EVDATA_BLD	0x0000000000000800ull	/* bus lock acq
							   detected */
#define	TF_FRED_EVDATA_BD	0x0000000000002000ull	/* dr access detected */
#define	TF_FRED_EVDATA_BS	0x0000000000004000ull	/* single step */
#define	TF_FRED_EVDATA_RTM	0x0000000000010000ull	/* #db or #bp in RTM */

#define	TF_FRED_EVINFO1_STIINT		0x0001	/* hw intr blocked by STI */
#define	TF_FRED_EVINFO1_SYSCALL		0x0002	/* SYSCALL/SYSENTER/INTn */
#define	TF_FRED_EVINFO1_NMI		0x0004	/* NMI */

#define	TF_FRED_EVINFO2_VECMASK		0x000000ff	/* event vector mask */
#define	TF_FRED_EVINFO2_TYPEMASK	0x000f0000	/* event type mask */
#define	TF_FRED_EVINFO2_TYPE_EXTINT	0x00000000
#define	TF_FRED_EVINFO2_TYPE_NMI	0x00020000
#define	TF_FRED_EVINFO2_TYPE_EXC	0x00030000
#define	TF_FRED_EVINFO2_TYPE_INTn	0x00040000
#define	TF_FRED_EVINFO2_TYPE_INT1	0x00050000
#define	TF_FRED_EVINFO2_TYPE_INT3	0x00060000
#define	TF_FRED_EVINFO2_TYPE_SYSCALL	0x00070000
#define	TF_FRED_EVINFO2_ENCL		0x01000000	/* SGX-related */
#define	TF_FRED_EVINFO2_LM		0x02000000	/* in 64bit mode */
#define	TF_FRED_EVINFO2_NEST		0x04000000	/* during ev delivery */
#define	TF_FRED_EVINFO2_INSTLENMASK	0xf0000000	/* instr length mask */
#define	TF_FRED_EVINFO2_INSTLENSHIFT	28		/* instr length shift */

#define	TF_FRED_EVINFO2_VEC_SYSCALL	1
#define	TF_FRED_EVINFO2_VEC_SYSENTER	2

#define	TF_FRED_EVINFO3_CSLMASK		0x0003	/* event CSL mask */
#define	TF_FRED_EVINFO3_WFE		0x0004	/* in WAIT_FOR_ENDBRANCH */

#define	TF_HASSEGS	0x00000001
#define	TF_HASBASES	0x00000002
#define	TF_HASFPXSTATE	0x00000004
#define	TF_RESERV0	0x00000008 /* no tlsbase in the trapframe */
#define	TF_FRED		0x00000010
#endif /* __amd64__ */

#endif /* _MACHINE_FRAME_H_ */
