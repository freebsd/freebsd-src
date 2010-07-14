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

#include <machine/endian.h>

/* BEGIN: these are going away */

#define	SR_KSU_MASK		0x00000018
#define	SR_KSU_USER		0x00000010
#define	SR_KSU_SUPER		0x00000008
#define	SR_KSU_KERNEL		0x00000000

#define	soft_int_mask(softintr)	(1 << ((softintr) + 8))
#define	hard_int_mask(hardintr)	(1 << ((hardintr) + 10))

/* END: These are going away */

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
 * Exported definitions unique to mips cpu support.
 */

#define	cpu_swapout(p)		panic("cpu_swapout: can't get here");

#ifndef _LOCORE
#include <machine/cpufunc.h>
#include <machine/frame.h>

/*
 * Arguments to hardclock and gatherstats encapsulate the previous
 * machine state in an opaque clockframe.
 */
#define	clockframe trapframe	/* Use normal trap frame */

#define	CLKF_USERMODE(framep)	((framep)->sr & SR_KSU_USER)
#define	CLKF_PC(framep)		((framep)->pc)
#define	CLKF_INTR(framep)	(0)
#define	MIPS_CLKF_INTR()	(intr_nesting_level >= 1)
#define	TRAPF_USERMODE(framep)  (((framep)->sr & SR_KSU_USER) != 0)
#define	TRAPF_PC(framep)	((framep)->pc)
#define	cpu_getstack(td)	((td)->td_frame->sp)

/*
 * A machine-independent interface to the CPU's counter.
 */
#define	get_cyclecount()	mips_rd_count()
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

#if defined(_KERNEL) && !defined(_LOCORE)
struct user;

int Mips_ConfigCache(void);

void Mips_SyncCache(void);
void Mips_SyncDCache(vm_offset_t, int);
void Mips_HitSyncDCache(vm_offset_t, int);
void Mips_HitSyncSCache(vm_offset_t, int);
void Mips_IOSyncDCache(vm_offset_t, int, int);
void Mips_HitInvalidateDCache(vm_offset_t, int);
void Mips_SyncICache(vm_offset_t, int);
void Mips_InvalidateICache(vm_offset_t, int);

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

void swi_vm(void *);
void cpu_halt(void);
void cpu_reset(void);

u_int32_t set_intr_mask(u_int32_t);
u_int32_t get_intr_mask(void);

#define	cpu_spinwait()		/* nothing */

#endif				/* _KERNEL */
#endif				/* !_MACHINE_CPU_H_ */
