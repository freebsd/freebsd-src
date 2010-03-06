/*	$OpenBSD: cpu.h,v 1.4 1998/09/15 10:50:12 pefo Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	Copyright (C) 1989 Digital Equipment Corporation.
 *	Permission to use, copy, modify, and distribute this software and
 *	its documentation for any purpose and without fee is hereby granted,
 *	provided that the above copyright notice appears in all copies.
 *	Digital Equipment Corporation makes no representations about the
 *	suitability of this software for any purpose.  It is provided "as is"
 *	without express or implied warranty.
 *
 *	from: @(#)cpu.h	8.4 (Berkeley) 1/4/94
 *	JNPR: cpu.h,v 1.9.2.2 2007/09/10 08:23:46 girish
 * $FreeBSD$
 */

#ifndef _MACHINE_CPU_H_
#define	_MACHINE_CPU_H_

#include <machine/psl.h>
#include <machine/endian.h>

#define MIPS_KSEG0_LARGEST_PHYS         0x20000000
#define	MIPS_PHYS_MASK			(0x1fffffff)

#define	MIPS_PHYS_TO_KSEG0(x)		((uintptr_t)(x) | MIPS_KSEG0_START)
#define	MIPS_PHYS_TO_KSEG1(x)		((uintptr_t)(x) | MIPS_KSEG1_START)
#define	MIPS_KSEG0_TO_PHYS(x)		((uintptr_t)(x) & MIPS_PHYS_MASK)
#define	MIPS_KSEG1_TO_PHYS(x)		((uintptr_t)(x) & MIPS_PHYS_MASK)

#define	MIPS_IS_KSEG0_ADDR(x)					\
	(((vm_offset_t)(x) >= MIPS_KSEG0_START) &&		\
	    ((vm_offset_t)(x) <= MIPS_KSEG0_END))
#define	MIPS_IS_KSEG1_ADDR(x)					\
	(((vm_offset_t)(x) >= MIPS_KSEG1_START) &&		\
	    ((vm_offset_t)(x) <= MIPS_KSEG1_END))
#define	MIPS_IS_VALID_PTR(x)		(MIPS_IS_KSEG0_ADDR(x) || \
						MIPS_IS_KSEG1_ADDR(x))

/*
 *  Status register.
 */
#define	SR_COP_USABILITY	0xf0000000
#define	SR_COP_0_BIT		0x10000000
#define	SR_COP_1_BIT		0x20000000
#define	SR_COP_2_BIT		0x40000000
#define	SR_RP			0x08000000
#define	SR_FR_32		0x04000000
#define	SR_RE			0x02000000
#define	SR_PX			0x00800000
#define	SR_BOOT_EXC_VEC		0x00400000
#define	SR_TLB_SHUTDOWN		0x00200000
#define	SR_SOFT_RESET		0x00100000
#define	SR_DIAG_CH		0x00040000
#define	SR_DIAG_CE		0x00020000
#define	SR_DIAG_DE		0x00010000
#define	SR_KX			0x00000080
#define	SR_SX			0x00000040
#define	SR_UX			0x00000020
#define	SR_KSU_MASK		0x00000018
#define	SR_KSU_USER		0x00000010
#define	SR_KSU_SUPER		0x00000008
#define	SR_KSU_KERNEL		0x00000000
#define	SR_ERL			0x00000004
#define	SR_EXL			0x00000002
#define	SR_INT_ENAB		0x00000001

#define	SR_INT_MASK		0x0000ff00
#define	SOFT_INT_MASK_0		0x00000100
#define	SOFT_INT_MASK_1		0x00000200
#define	SR_INT_MASK_0		0x00000400
#define	SR_INT_MASK_1		0x00000800
#define	SR_INT_MASK_2		0x00001000
#define	SR_INT_MASK_3		0x00002000
#define	SR_INT_MASK_4		0x00004000
#define	SR_INT_MASK_5		0x00008000
#define	ALL_INT_MASK		SR_INT_MASK
#define	SOFT_INT_MASK		(SOFT_INT_MASK_0 | SOFT_INT_MASK_1)
#define	HW_INT_MASK		(ALL_INT_MASK & ~SOFT_INT_MASK)

#define	soft_int_mask(softintr)	(1 << ((softintr) + 8))
#define	hard_int_mask(hardintr)	(1 << ((hardintr) + 10))

/*
 * The bits in the cause register.
 *
 *	CR_BR_DELAY	Exception happened in branch delay slot.
 *	CR_COP_ERR	Coprocessor error.
 *	CR_IP		Interrupt pending bits defined below.
 *	CR_EXC_CODE	The exception type (see exception codes below).
 */
#define	CR_BR_DELAY		0x80000000
#define	CR_COP_ERR		0x30000000
#define	CR_EXC_CODE		0x0000007c
#define	CR_EXC_CODE_SHIFT	2
#define	CR_IPEND		0x0000ff00

/*
 * Cause Register Format:
 *
 *   31  30  29 28 27  26  25  24 23                   8  7 6       2  1  0
 *  ----------------------------------------------------------------------
 * | BD | 0| CE   | 0| W2| W1| IV|	IP15 - IP0	| 0| Exc Code | 0|
 * |______________________________________________________________________
 */

#define	CR_INT_SOFT0		0x00000100
#define	CR_INT_SOFT1		0x00000200
#define	CR_INT_0		0x00000400
#define	CR_INT_1		0x00000800
#define	CR_INT_2		0x00001000
#define	CR_INT_3		0x00002000
#define	CR_INT_4		0x00004000
#define	CR_INT_5		0x00008000

#define	CR_INT_UART	CR_INT_1
#define	CR_INT_IPI	CR_INT_2
#define	CR_INT_CLOCK	CR_INT_5

/*
 * The bits in the CONFIG register
 */
#define CFG_K0_UNCACHED	2
#define	CFG_K0_CACHED	3
#define	CFG_K0_MASK	0x7

/*
 * The bits in the context register.
 */
#define	CNTXT_PTE_BASE		0xff800000
#define	CNTXT_BAD_VPN2		0x007ffff0

/*
 * Location of exception vectors.
 */
#define	RESET_EXC_VEC		0xbfc00000
#define	TLB_MISS_EXC_VEC	0x80000000
#define	XTLB_MISS_EXC_VEC	0x80000080
#define	CACHE_ERR_EXC_VEC	0x80000100
#define	GEN_EXC_VEC		0x80000180

/*
 * Coprocessor 0 registers:
 */
#define	COP_0_TLB_INDEX		$0
#define	COP_0_TLB_RANDOM	$1
#define	COP_0_TLB_LO0		$2
#define	COP_0_TLB_LO1		$3
#define	COP_0_TLB_CONTEXT	$4
#define	COP_0_TLB_PG_MASK	$5
#define	COP_0_TLB_WIRED		$6
#define	COP_0_INFO		$7
#define	COP_0_BAD_VADDR		$8
#define	COP_0_COUNT		$9
#define	COP_0_TLB_HI		$10
#define	COP_0_COMPARE		$11
#define	COP_0_STATUS_REG	$12
#define	COP_0_CAUSE_REG		$13
#define	COP_0_EXC_PC		$14
#define	COP_0_PRID		$15
#define	COP_0_CONFIG		$16
#define	COP_0_LLADDR		$17
#define	COP_0_WATCH_LO		$18
#define	COP_0_WATCH_HI		$19
#define	COP_0_TLB_XCONTEXT	$20
#define	COP_0_ECC		$26
#define	COP_0_CACHE_ERR		$27
#define	COP_0_TAG_LO		$28
#define	COP_0_TAG_HI		$29
#define	COP_0_ERROR_PC		$30

/*
 *  Coprocessor 0 Set 1
 */
#define	C0P_1_IPLLO	$18
#define	C0P_1_IPLHI	$19
#define	C0P_1_INTCTL	$20
#define	C0P_1_DERRADDR0	$26
#define	C0P_1_DERRADDR1	$27

/*
 * Values for the code field in a break instruction.
 */
#define	BREAK_INSTR		0x0000000d
#define	BREAK_VAL_MASK		0x03ffffc0
#define	BREAK_VAL_SHIFT		16
#define	BREAK_KDB_VAL		512
#define	BREAK_SSTEP_VAL		513
#define	BREAK_BRKPT_VAL		514
#define	BREAK_SOVER_VAL		515
#define	BREAK_DDB_VAL		516
#define	BREAK_KDB	(BREAK_INSTR | (BREAK_KDB_VAL << BREAK_VAL_SHIFT))
#define	BREAK_SSTEP	(BREAK_INSTR | (BREAK_SSTEP_VAL << BREAK_VAL_SHIFT))
#define	BREAK_BRKPT	(BREAK_INSTR | (BREAK_BRKPT_VAL << BREAK_VAL_SHIFT))
#define	BREAK_SOVER	(BREAK_INSTR | (BREAK_SOVER_VAL << BREAK_VAL_SHIFT))
#define	BREAK_DDB	(BREAK_INSTR | (BREAK_DDB_VAL << BREAK_VAL_SHIFT))

/*
 * Mininum and maximum cache sizes.
 */
#define	MIN_CACHE_SIZE		(16 * 1024)
#define	MAX_CACHE_SIZE		(256 * 1024)

/*
 * The floating point version and status registers.
 */
#define	FPC_ID			$0
#define	FPC_CSR			$31

/*
 * The floating point coprocessor status register bits.
 */
#define	FPC_ROUNDING_BITS		0x00000003
#define	FPC_ROUND_RN			0x00000000
#define	FPC_ROUND_RZ			0x00000001
#define	FPC_ROUND_RP			0x00000002
#define	FPC_ROUND_RM			0x00000003
#define	FPC_STICKY_BITS			0x0000007c
#define	FPC_STICKY_INEXACT		0x00000004
#define	FPC_STICKY_UNDERFLOW		0x00000008
#define	FPC_STICKY_OVERFLOW		0x00000010
#define	FPC_STICKY_DIV0			0x00000020
#define	FPC_STICKY_INVALID		0x00000040
#define	FPC_ENABLE_BITS			0x00000f80
#define	FPC_ENABLE_INEXACT		0x00000080
#define	FPC_ENABLE_UNDERFLOW		0x00000100
#define	FPC_ENABLE_OVERFLOW		0x00000200
#define	FPC_ENABLE_DIV0			0x00000400
#define	FPC_ENABLE_INVALID		0x00000800
#define	FPC_EXCEPTION_BITS		0x0003f000
#define	FPC_EXCEPTION_INEXACT		0x00001000
#define	FPC_EXCEPTION_UNDERFLOW		0x00002000
#define	FPC_EXCEPTION_OVERFLOW		0x00004000
#define	FPC_EXCEPTION_DIV0		0x00008000
#define	FPC_EXCEPTION_INVALID		0x00010000
#define	FPC_EXCEPTION_UNIMPL		0x00020000
#define	FPC_COND_BIT			0x00800000
#define	FPC_FLUSH_BIT			0x01000000
#define	FPC_MBZ_BITS			0xfe7c0000

/*
 * Constants to determine if have a floating point instruction.
 */
#define	OPCODE_SHIFT		26
#define	OPCODE_C1		0x11

/*
 * The low part of the TLB entry.
 */
#define	VMTLB_PF_NUM		0x3fffffc0
#define	VMTLB_ATTR_MASK		0x00000038
#define	VMTLB_MOD_BIT		0x00000004
#define	VMTLB_VALID_BIT		0x00000002
#define	VMTLB_GLOBAL_BIT	0x00000001

#define	VMTLB_PHYS_PAGE_SHIFT	6

/*
 * The high part of the TLB entry.
 */
#define	VMTLB_VIRT_PAGE_NUM		0xffffe000
#define	VMTLB_PID			0x000000ff
#define	VMTLB_PID_R9K			0x00000fff
#define	VMTLB_PID_SHIFT			0
#define	VMTLB_VIRT_PAGE_SHIFT		12
#define	VMTLB_VIRT_PAGE_SHIFT_R9K	13

/*
 * The first TLB entry that write random hits.
 * TLB entry 0 maps the kernel stack of the currently running thread
 * TLB entry 1 maps the pcpu area of processor (only for SMP builds)
 */
#define	KSTACK_TLB_ENTRY	0
#ifdef SMP
#define	PCPU_TLB_ENTRY		1
#define	VMWIRED_ENTRIES		2
#else
#define	VMWIRED_ENTRIES		1
#endif	/* SMP */

/*
 * The number of process id entries.
 */
#define	VMNUM_PIDS		256

/*
 * TLB probe return codes.
 */
#define	VMTLB_NOT_FOUND		0
#define	VMTLB_FOUND		1
#define	VMTLB_FOUND_WITH_PATCH	2
#define	VMTLB_PROBE_ERROR	3

/*
 * Exported definitions unique to mips cpu support.
 */

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#define	COPY_SIGCODE		/* copy sigcode above user stack in exec */

#define	cpu_swapout(p)		panic("cpu_swapout: can't get here");

#ifndef _LOCORE
#include <machine/frame.h>
/*
 * Arguments to hardclock and gatherstats encapsulate the previous
 * machine state in an opaque clockframe.
 */
#define	clockframe trapframe	/* Use normal trap frame */

#define	CLKF_USERMODE(framep)	((framep)->sr & SR_KSU_USER)
#define	CLKF_BASEPRI(framep)	((framep)->cpl == 0)
#define	CLKF_PC(framep)		((framep)->pc)
#define	CLKF_INTR(framep)	(0)
#define	MIPS_CLKF_INTR()	(intr_nesting_level >= 1)
#define	TRAPF_USERMODE(framep)  (((framep)->sr & SR_KSU_USER) != 0)
#define	TRAPF_PC(framep)	((framep)->pc)
#define	cpu_getstack(td)	((td)->td_frame->sp)

/*
 * CPU identification, from PRID register.
 */
union cpuprid {
	int cpuprid;
	struct {
#if BYTE_ORDER == BIG_ENDIAN
		u_int pad1:8;	/* reserved */
		u_int cp_vendor:8;	/* company identifier */
		u_int cp_imp:8;	/* implementation identifier */
		u_int cp_majrev:4;	/* major revision identifier */
		u_int cp_minrev:4;	/* minor revision identifier */
#else
		u_int cp_minrev:4;	/* minor revision identifier */
		u_int cp_majrev:4;	/* major revision identifier */
		u_int cp_imp:8;	/* implementation identifier */
		u_int cp_vendor:8;	/* company identifier */
		u_int pad1:8;	/* reserved */
#endif
	}      cpu;
};

#endif				/* !_LOCORE */

/*
 * CTL_MACHDEP definitions.
 */
#define	CPU_CONSDEV		1	/* dev_t: console terminal device */
#define	CPU_ADJKERNTZ		2	/* int: timezone offset (seconds) */
#define	CPU_DISRTCSET		3	/* int: disable resettodr() call */
#define	CPU_BOOTINFO		4	/* struct: bootinfo */
#define	CPU_WALLCLOCK		5	/* int: indicates wall CMOS clock */
#define	CPU_MAXID		6	/* number of valid machdep ids */

#define	CTL_MACHDEP_NAMES {			\
	{ 0, 0 },				\
	{ "console_device", CTLTYPE_STRUCT },	\
	{ "adjkerntz", CTLTYPE_INT },		\
	{ "disable_rtc_set", CTLTYPE_INT },	\
	{ "bootinfo", CTLTYPE_STRUCT },		\
	{ "wall_cmos_clock", CTLTYPE_INT },	\
}

/*
 * MIPS CPU types (cp_imp).
 */
#define	MIPS_R2000	0x01	/* MIPS R2000 CPU		ISA I	 */
#define	MIPS_R3000	0x02	/* MIPS R3000 CPU		ISA I	 */
#define	MIPS_R6000	0x03	/* MIPS R6000 CPU		ISA II	 */
#define	MIPS_R4000	0x04	/* MIPS R4000/4400 CPU		ISA III	 */
#define	MIPS_R3LSI	0x05	/* LSI Logic R3000 derivate	ISA I	 */
#define	MIPS_R6000A	0x06	/* MIPS R6000A CPU		ISA II	 */
#define	MIPS_R3IDT	0x07	/* IDT R3000 derivate		ISA I	 */
#define	MIPS_R10000	0x09	/* MIPS R10000/T5 CPU		ISA IV	 */
#define	MIPS_R4200	0x0a	/* MIPS R4200 CPU (ICE)		ISA III	 */
#define	MIPS_R4300	0x0b	/* NEC VR4300 CPU		ISA III	 */
#define	MIPS_R4100	0x0c	/* NEC VR41xx CPU MIPS-16	ISA III	 */
#define	MIPS_R8000	0x10	/* MIPS R8000 Blackbird/TFP	ISA IV	 */
#define	MIPS_R4600	0x20	/* QED R4600 Orion		ISA III	 */
#define	MIPS_R4700	0x21	/* QED R4700 Orion		ISA III	 */
#define	MIPS_R3TOSH	0x22	/* Toshiba R3000 based CPU	ISA I	 */
#define	MIPS_R5000	0x23	/* MIPS R5000 CPU		ISA IV	 */
#define	MIPS_RM7000	0x27	/* QED RM7000 CPU		ISA IV	 */
#define	MIPS_RM52X0	0x28	/* QED RM52X0 CPU		ISA IV	 */
#define	MIPS_VR5400	0x54	/* NEC Vr5400 CPU		ISA IV+	 */
#define	MIPS_RM9000	0x34	/* E9000 CPU				 */

/*
 * MIPS FPU types
 */
#define	MIPS_SOFT	0x00	/* Software emulation		ISA I	 */
#define	MIPS_R2360	0x01	/* MIPS R2360 FPC		ISA I	 */
#define	MIPS_R2010	0x02	/* MIPS R2010 FPC		ISA I	 */
#define	MIPS_R3010	0x03	/* MIPS R3010 FPC		ISA I	 */
#define	MIPS_R6010	0x04	/* MIPS R6010 FPC		ISA II	 */
#define	MIPS_R4010	0x05	/* MIPS R4000/R4400 FPC		ISA II	 */
#define	MIPS_R31LSI	0x06	/* LSI Logic derivate		ISA I	 */
#define	MIPS_R10010	0x09	/* MIPS R10000/T5 FPU		ISA IV	 */
#define	MIPS_R4210	0x0a	/* MIPS R4200 FPC (ICE)		ISA III	 */
#define	MIPS_UNKF1	0x0b	/* unnanounced product cpu	ISA III	 */
#define	MIPS_R8000	0x10	/* MIPS R8000 Blackbird/TFP	ISA IV	 */
#define	MIPS_R4600	0x20	/* QED R4600 Orion		ISA III	 */
#define	MIPS_R3SONY	0x21	/* Sony R3000 based FPU		ISA I	 */
#define	MIPS_R3TOSH	0x22	/* Toshiba R3000 based FPU	ISA I	 */
#define	MIPS_R5010	0x23	/* MIPS R5000 based FPU		ISA IV	 */
#define	MIPS_RM7000	0x27	/* QED RM7000 FPU		ISA IV	 */
#define	MIPS_RM5230	0x28	/* QED RM52X0 based FPU		ISA IV	 */
#define	MIPS_RM52XX	0x28	/* QED RM52X0 based FPU		ISA IV	 */
#define	MIPS_VR5400	0x54	/* NEC Vr5400 FPU		ISA IV+	 */

#ifndef _LOCORE
extern union cpuprid cpu_id;

#define	mips_proc_type()      ((cpu_id.cpu.cp_vendor << 8) | cpu_id.cpu.cp_imp)
#define	mips_set_proc_type(type)	(cpu_id.cpu.cp_vendor = (type)  >> 8, \
					 cpu_id.cpu.cp_imp = ((type) & 0x00ff))
#endif				/* !_LOCORE */

#if defined(_KERNEL) && !defined(_LOCORE)
extern union cpuprid fpu_id;

struct tlb;
struct user;

u_int32_t mips_cp0_config1_read(void);
int Mips_ConfigCache(void);
void Mips_SetWIRED(int);
void Mips_SetPID(int);
u_int Mips_GetCOUNT(void);
void Mips_SetCOMPARE(u_int);
u_int Mips_GetCOMPARE(void);

void Mips_SyncCache(void);
void Mips_SyncDCache(vm_offset_t, int);
void Mips_HitSyncDCache(vm_offset_t, int);
void Mips_HitSyncSCache(vm_offset_t, int);
void Mips_IOSyncDCache(vm_offset_t, int, int);
void Mips_HitInvalidateDCache(vm_offset_t, int);
void Mips_SyncICache(vm_offset_t, int);
void Mips_InvalidateICache(vm_offset_t, int);

void Mips_TLBFlush(int);
void Mips_TLBFlushAddr(vm_offset_t);
void Mips_TLBWriteIndexed(int, struct tlb *);
void Mips_TLBUpdate(vm_offset_t, unsigned);
void Mips_TLBRead(int, struct tlb *);
void mips_TBIAP(int);
void wbflush(void);

extern u_int32_t cpu_counter_interval;	/* Number of counter ticks/tick */
extern u_int32_t cpu_counter_last;	/* Last compare value loaded    */
extern int num_tlbentries;
extern char btext[];
extern char etext[];
extern int intr_nesting_level;

#define	func_0args_asmmacro(func, in)					\
	__asm __volatile ( "jalr %0"					\
			: "=r" (in)	/* outputs */			\
			: "r" (func)	/* inputs */			\
			: "$31", "$4");

#define	func_1args_asmmacro(func, arg0)					\
	__asm __volatile ("move $4, %1;"				\
			"jalr %0"					\
			:				/* outputs */	\
			: "r" (func), "r" (arg0)	/* inputs */	\
			: "$31", "$4");

#define	func_2args_asmmacro(func, arg0, arg1)				\
	__asm __volatile ("move $4, %1;"				\
			"move $5, %2;"					\
			"jalr %0"					\
			:				/* outputs */   \
			: "r" (func), "r" (arg0), "r" (arg1) /* inputs */ \
			: "$31", "$4", "$5");

#define	func_3args_asmmacro(func, arg0, arg1, arg2)			\
	__asm __volatile ( "move $4, %1;"				\
			"move $5, %2;"					\
			"move $6, %3;"					\
			"jalr %0"					\
			:				/* outputs */	\
			: "r" (func), "r" (arg0), "r" (arg1), "r" (arg2)  /* inputs */ \
			: "$31", "$4", "$5", "$6");

#define	MachSetPID			Mips_SetPID
#define	MachTLBUpdate   		Mips_TLBUpdate
#define	mips_TBIS			Mips_TLBFlushAddr
#define	MIPS_TBIAP()			mips_TBIAP(num_tlbentries)
#define	MachSetWIRED(index)		Mips_SetWIRED(index)
#define	MachTLBFlush(count)		Mips_TLBFlush(count)
#define	MachTLBGetPID(pid)		(pid = Mips_TLBGetPID())
#define	MachTLBRead(tlbno, tlbp)	Mips_TLBRead(tlbno, tlbp)
#define	MachFPTrap(sr, cause, pc)	MipsFPTrap(sr, cause, pc)

/*
 * Enable realtime clock (always enabled).
 */
#define	enablertclock()

/*
 * Are we in an interrupt handler? required by JunOS
 */
#define	IN_INT_HANDLER()				\
	(curthread->td_intr_nesting_level != 0 ||	\
	(curthread->td_pflags & TDP_ITHREAD))

/*
 *  Low level access routines to CPU registers
 */

void setsoftintr0(void);
void clearsoftintr0(void);
void setsoftintr1(void);
void clearsoftintr1(void);


u_int32_t mips_cp0_status_read(void);
void mips_cp0_status_write(u_int32_t);

int disableintr(void);
void restoreintr(int);
int enableintr(void);
int Mips_TLBGetPID(void);

void swi_vm(void *);
void cpu_halt(void);
void cpu_reset(void);

u_int32_t set_intr_mask(u_int32_t);
u_int32_t get_intr_mask(void);
u_int32_t get_cyclecount(void);

#define	cpu_spinwait()		/* nothing */

#endif				/* _KERNEL */
#endif				/* !_MACHINE_CPU_H_ */
