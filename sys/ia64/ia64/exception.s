/*-
 * Copyright (c) 2000 Doug Rabson
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
 *	$FreeBSD$
 */

#include <machine/asm.h>
#include <machine/pte.h>
#include <assym.s>

/*
 * ar.k7 = curthread
 * ar.k6 = ksp
 * ar.k5 = kbsp
 * ar.k4 = pcpup
 */

/*
 * Call exception_save_regs to preserve the interrupted state in a
 * trapframe. Note that we don't use a call instruction because we
 * must be careful not to lose track of the RSE state. We then call
 * trap() with the value of _n_ as an argument to handle the
 * exception. We arrange for trap() to return to exception_restore
 * which will restore the interrupted state before executing an rfi to
 * resume it.
 */
#define TRAP(_n_)				\
	mov	r16=b0;				\
1:	mov	r17=ip;;			\
	add	r17=2f-1b,r17;;			\
	mov	b0=r17;				\
	br.sptk.few exception_save;		\
2: (p3)	ssm	psr.i;				\
	alloc	r15=ar.pfs,0,0,3,0;		\
	mov	out0=_n_;			\
	mov	out1=r14;			\
	mov	out2=sp;;			\
	add	sp=-16,sp;;			\
	.prologue;				\
	.save	rp,r0;				\
	.body;					\
	br.call.sptk.few rp=trap;		\
3:	br.sptk.many exception_restore
	
/*
 * The IA64 Interrupt Vector Table (IVT) contains 20 slots with 64
 * bundles per vector and 48 slots with 16 bundles per vector.
 */

	.section .text.ivt,"ax"

	.align	32768
	.global ia64_vector_table
ia64_vector_table:

	.org	ia64_vector_table + 0x0000	// VHPT Translation vector
interruption_VHPT_Translation:
	TRAP(0)

	.org	ia64_vector_table + 0x0400	// Instruction TLB vector
interruption_Instruction_TLB:
	mov	r16=cr.ifa
	mov	r17=pr
	;;
	thash	r18=r16
	ttag	r19=r16
	;;
	add	r21=16,r18		// tag
	add	r20=24,r18		// collision chain
	;; 
	ld8	r21=[r21]		// check VHPT tag
	;;
	cmp.eq	p1,p2=r21,r19
(p2)	br.dpnt.few 1f
	;;
	ld8	r21=[r18]		// read pte
	;;
	itc.i	r21			// insert pte
	rfi				// done
	;;
1:	ld8	r20=[r20]		// first entry
	;; 
	rsm	psr.dt			// turn off data translations
	;;
	srlz.d				// serialize
	;;
2:	cmp.eq	p1,p2=r0,r20		// done?
(p1)	br.cond.spnt.few 9f		// bail if done
	;;
	add	r21=16,r20		// tag location
	;;
	ld8	r21=[r21]		// read tag
	;;
	cmp.eq	p1,p2=r21,r19		// compare tags
(p2)	br.cond.sptk.few 3f		// if not, read next in chain
	;;
	ld8	r21=[r20],8		// read pte
	;; 
	ld8	r22=[r20]		// read rest of pte
	;;
	dep	r18=0,r18,61,3		// convert vhpt ptr to physical
	;;
	add	r20=16,r18		// address of tag
	;;
	ld8.acq	r23=[r20]		// read old tag
	;;
	dep	r23=-1,r23,63,1		// set ti bit
	;;
	st8.rel	[r20]=r23		// store old tag + ti
	;;
	mf				// make sure everyone sees
	;;
	st8	[r18]=r21,8		// store pte
	;;
	st8	[r18]=r22,8
	;;
	st8.rel	[r18]=r19		// store new tag
	;; 
	mov	pr=r17,0x1ffff		// restore predicates
	;;
	itc.i	r21			// and place in TLB
	rfi
	
3:	add	r20=24,r20		// next in chain
	;;
	ld8	r20=[r20]		// read chain
	br.cond.sptk.few 2b		// loop

9:	mov	pr=r17,0x1ffff		// restore predicates
	ssm	psr.dt
	;;
	srlz.d
	;; 
	TRAP(20)			// Page Not Present trap

	.org	ia64_vector_table + 0x0800	// Data TLB vector
interruption_Data_TLB:
	mov	r16=cr.ifa
	mov	r17=pr
	;;
	thash	r18=r16
	ttag	r19=r16
	;;
	add	r21=16,r18		// tag
	add	r20=24,r18		// collision chain
	;; 
	ld8	r21=[r21]		// check VHPT tag
	;;
	cmp.eq	p1,p2=r21,r19
	br.dpnt.few 1f
	;;
	ld8	r21=[r18]		// read pte
	;;
	itc.d	r21			// insert pte
	rfi				// done
	;;
1:	ld8	r20=[r20]		// first entry
	;; 
	rsm	psr.dt			// turn off data translations
	;;
	srlz.d				// serialize
	;;
2:	cmp.eq	p1,p2=r0,r20		// done?
(p1)	br.cond.spnt.few 9f		// bail if done
	;;
	add	r21=16,r20		// tag location
	;;
	ld8	r21=[r21]		// read tag
	;;
	cmp.eq	p1,p2=r21,r19		// compare tags
(p2)	br.cond.sptk.few 3f		// if not, read next in chain
	;;
	ld8	r21=[r20],8		// read pte
	;; 
	ld8	r22=[r20]		// read rest of pte
	;;
	dep	r18=0,r18,61,3		// convert vhpt ptr to physical
	;;
	add	r20=16,r18		// address of tag
	;;
	ld8.acq	r23=[r20]		// read old tag
	;;
	dep	r23=-1,r23,63,1		// set ti bit
	;;
	st8.rel	[r20]=r23		// store old tag + ti
	;;
	mf				// make sure everyone sees
	;;
	st8	[r18]=r21,8		// store pte
	;;
	st8	[r18]=r22,8
	;;
	st8.rel	[r18]=r19		// store new tag
	;; 
	mov	pr=r17,0x1ffff		// restore predicates
	;;
	itc.d	r21			// and place in TLB
	rfi
	
3:	add	r20=24,r20		// next in chain
	;;
	ld8	r20=[r20]		// read chain
	br.cond.sptk.few 2b		// loop

9:	mov	pr=r17,0x1ffff		// restore predicates
	ssm	psr.dt
	;;
	srlz.d
	;; 
	TRAP(20)			// Page Not Present trap

	.org	ia64_vector_table + 0x0c00	// Alternate ITLB vector
interruption_Alternate_Instruction_TLB:
	mov	r16=cr.ifa		// where did it happen
	mov	r18=pr			// save predicates
	;;
	extr.u	r17=r16,61,3		// get region number
	;;
	cmp.ge	p3,p0=5,r17		// RR0-RR5?
	cmp.eq	p1,p2=7,r17		// RR7->p1, RR6->p2
(p3)	br.spnt	9f
	;;
(p1)	movl	r17=PTE_P+PTE_MA_WB+PTE_A+PTE_D+PTE_PL_KERN+PTE_AR_RX
(p2)	movl	r17=PTE_P+PTE_MA_UC+PTE_A+PTE_D+PTE_PL_KERN+PTE_AR_RX
	;;
	dep	r16=0,r16,50,14		// clear bits above PPN
	;;
	dep	r16=r17,r16,0,12	// put pte bits in 0..11
	;;
	itc.i	r16
	mov	pr=r18,0x1ffff		// restore predicates
	;;
	rfi
9:	mov	pr=r18,0x1ffff		// restore predicates
	TRAP(3)

	.org	ia64_vector_table + 0x1000	// Alternate DTLB vector
interruption_Alternate_Data_TLB:
	mov	r16=cr.ifa		// where did it happen
	mov	r18=pr			// save predicates
	;;
	extr.u	r17=r16,61,3		// get region number
	;;
	cmp.ge	p3,p0=5,r17		// RR0-RR5?
	cmp.eq	p1,p2=7,r17		// RR7->p1, RR6->p2
(p3)	br.spnt	9f
	;;
(p1)	movl	r17=PTE_P+PTE_MA_WB+PTE_A+PTE_D+PTE_PL_KERN+PTE_AR_RW
(p2)	movl	r17=PTE_P+PTE_MA_UC+PTE_A+PTE_D+PTE_PL_KERN+PTE_AR_RW
	;;
	dep	r16=0,r16,50,14		// clear bits above PPN
	;;
	dep	r16=r17,r16,0,12	// put pte bits in 0..11
	;;
	itc.d	r16
	mov	pr=r18,0x1ffff		// restore predicates
	;;
	rfi
9:	mov	pr=r18,0x1ffff		// restore predicates
	TRAP(4)

	.org	ia64_vector_table + 0x1400	// Data Nested TLB vector
interruption_Data_Nested_TLB:
	TRAP(5)

	.org	ia64_vector_table + 0x1800	// Instr. Key Miss vector
interruption_Instruction_Key_Miss:
	TRAP(6)
	
	.org	ia64_vector_table + 0x1c00	// Data Key Miss vector
interruption_Data_Key_Miss:
	TRAP(7)

	.org	ia64_vector_table + 0x2000	// Dirty-Bit vector
interruption_Dirty_Bit:
	mov	r16=cr.ifa
	mov	r17=pr
	mov	r20=PAGE_SHIFT<<2	// XXX get page size from VHPT
	;;
	ptc.l	r16,r20			// purge TLB
	thash	r18=r16
	ttag	r19=r16
	;;
	srlz.d
	add	r20=24,r18		// collision chain
	;; 
	ld8	r20=[r20]		// first entry
	;; 
	rsm	psr.dt			// turn off data translations
	;;
	srlz.d				// serialize
	;;
1:	cmp.eq	p1,p2=r0,r20		// done?
(p1)	br.cond.spnt.few 9f		// bail if done
	;;
	add	r21=16,r20		// tag location
	;;
	ld8	r21=[r21]		// read tag
	;;
	cmp.eq	p1,p2=r21,r19		// compare tags
(p2)	br.cond.sptk.few 2f		// if not, read next in chain
	;;
	ld8	r21=[r20]		// read pte
	mov	r22=PTE_D
	;;
	or	r21=r22,r21		// set dirty bit
	;;
	st8	[r20]=r21		// store back
	;; 
	ld8	r22=[r20]		// read rest of pte
	;;
	dep	r18=0,r18,61,3		// convert vhpt ptr to physical
	;;
	add	r20=16,r18		// address of tag
	;;
	ld8.acq	r23=[r20]		// read old tag
	;;
	dep	r23=-1,r23,63,1		// set ti bit
	;;
	st8.rel	[r20]=r23		// store old tag + ti
	;;
	mf				// make sure everyone sees
	;;
	st8	[r18]=r21,8		// store pte
	;;
	st8	[r18]=r22,8
	;;
	st8.rel	[r18]=r19		// store new tag
	;; 
	mov	pr=r17,0x1ffff		// restore predicates
	;;
	itc.d	r21			// and place in TLB
	rfi
	
2:	add	r20=24,r20		// next in chain
	;;
	ld8	r20=[r20]		// read chain
	br.cond.sptk.few 1b		// loop

9:	mov	pr=r17,0x1ffff		// restore predicates
	TRAP(8)				// die horribly

	.org	ia64_vector_table + 0x2400	// Instr. Access-Bit vector
interruption_Instruction_Access_Bit:
	mov	r16=cr.ifa
	mov	r17=pr
	mov	r20=PAGE_SHIFT<<2	// XXX get page size from VHPT
	;;
	ptc.l	r16,r20			// purge TLB
	thash	r18=r16
	ttag	r19=r16
	;;
	srlz.d
	add	r20=24,r18		// collision chain
	;; 
	ld8	r20=[r20]		// first entry
	;; 
	rsm	psr.dt			// turn off data translations
	;;
	srlz.d				// serialize
	;;
1:	cmp.eq	p1,p2=r0,r20		// done?
(p1)	br.cond.spnt.few 9f		// bail if done
	;;
	add	r21=16,r20		// tag location
	;;
	ld8	r21=[r21]		// read tag
	;;
	cmp.eq	p1,p2=r21,r19		// compare tags
(p2)	br.cond.sptk.few 2f		// if not, read next in chain
	;;
	ld8	r21=[r20]		// read pte
	mov	r22=PTE_A
	;;
	or	r21=r22,r21		// set accessed bit
	;;
	st8	[r20]=r21		// store back
	;; 
	ld8	r22=[r20]		// read rest of pte
	;;
	dep	r18=0,r18,61,3		// convert vhpt ptr to physical
	;;
	add	r20=16,r18		// address of tag
	;;
	ld8.acq	r23=[r20]		// read old tag
	;;
	dep	r23=-1,r23,63,1		// set ti bit
	;;
	st8.rel	[r20]=r23		// store old tag + ti
	;;
	mf				// make sure everyone sees
	;;
	st8	[r18]=r21,8		// store pte
	;;
	st8	[r18]=r22,8
	;;
	st8.rel	[r18]=r19		// store new tag
	;; 
	mov	pr=r17,0x1ffff		// restore predicates
	;;
	itc.i	r21			// and place in TLB
	rfi				// walker will retry the access
	
2:	add	r20=24,r20		// next in chain
	;;
	ld8	r20=[r20]		// read chain
	br.cond.sptk.few 1b		// loop

9:	mov	pr=r17,0x1ffff		// restore predicates
	TRAP(9)

	.org	ia64_vector_table + 0x2800	// Data Access-Bit vector
interruption_Data_Access_Bit:
	mov	r16=cr.ifa
	mov	r17=pr
	mov	r20=PAGE_SHIFT<<2	// XXX get page size from VHPT
	;;
	ptc.l	r16,r20			// purge TLB
	thash	r18=r16
	ttag	r19=r16
	;;
	srlz.d
	add	r20=24,r18		// collision chain
	;; 
	ld8	r20=[r20]		// first entry
	;; 
	rsm	psr.dt			// turn off data translations
	;;
	srlz.d				// serialize
	;;
1:	cmp.eq	p1,p2=r0,r20		// done?
(p1)	br.cond.spnt.few 9f		// bail if done
	;;
	add	r21=16,r20		// tag location
	;;
	ld8	r21=[r21]		// read tag
	;;
	cmp.eq	p1,p2=r21,r19		// compare tags
(p2)	br.cond.sptk.few 2f		// if not, read next in chain
	;;
	ld8	r21=[r20]		// read pte
	mov	r22=PTE_A
	;;
	or	r21=r22,r21		// set accessed bit
	;;
	st8	[r20]=r21		// store back
	;; 
	ld8	r22=[r20]		// read rest of pte
	;;
	dep	r18=0,r18,61,3		// convert vhpt ptr to physical
	;;
	add	r20=16,r18		// address of tag
	;;
	ld8.acq	r23=[r20]		// read old tag
	;;
	dep	r23=-1,r23,63,1		// set ti bit
	;;
	st8.rel	[r20]=r23		// store old tag + ti
	;;
	mf				// make sure everyone sees
	;;
	st8	[r18]=r21,8		// store pte
	;;
	st8	[r18]=r22,8
	;;
	st8.rel	[r18]=r19		// store new tag
	;; 
	mov	pr=r17,0x1ffff		// restore predicates
	;;
	itc.d	r21			// and place in TLB
	rfi				// walker will retry the access
	
2:	add	r20=24,r20		// next in chain
	;;
	ld8	r20=[r20]		// read chain
	br.cond.sptk.few 1b		// loop

9:	mov	pr=r17,0x1ffff		// restore predicates
	TRAP(10)

	.org	ia64_vector_table + 0x2c00	// Break Instruction vector
interruption_Break:
	mov	r16=pr			// save pr for a moment
	mov	r17=cr.iim;;		// read break value
	mov	r18=0x100000;;		// syscall number
	cmp.eq	p6,p7=r18,r17;;		// check for syscall
(p7)	br.dpnt.few 9f

	mov	r17=cr.ipsr;;		// check for user mode
	extr.u	r17=r17,32,2;;
	cmp.eq	p6,p7=r0,r17
(p6)	br.dpnt.few 9f			// trap if kernel mode

	// Note: p6 and p7 are temporaries so we don't need to restore
	// the value of pr here since the user-mode program assumes
	// that syscalls only preserve the function-preserved state.

	br.sptk.many	do_syscall
	;;
9:	mov	pr=r16,0x1ffff		// restore pr
	TRAP(11)

	.org	ia64_vector_table + 0x3000	// External Interrupt vector
interruption_External_Interrupt:
	mov	r16=b0			// save user's b0
1:	mov	r17=ip;;		// construct return address
	add	r17=2f-1b,r17;;		// for exception_save
	mov	b0=r17
	br.sptk.few exception_save	// 'call' exception_save

2:	alloc	r14=ar.pfs,0,0,2,0	// make a frame for calling with

	mov	out1=sp;;
	add	sp=-16,sp;;

3:	mov	out0=cr.ivr		// find interrupt vector
	;;
	cmp.eq	p6,p0=15,out0		// check for spurious vector number
(p6)	br.dpnt.few exception_restore	// if spurious, we are done
	;;
	ssm	psr.i			// re-enable interrupts
	;;				// now that we are in-progress
	srlz.d
	;;
	br.call.sptk.many rp=interrupt	// call high-level handler

	rsm	psr.i			// disable interrupts
	;;
	srlz.d
	;;
	mov	cr.eoi=r0		// and ack the interrupt
	;;
	srlz.d
	br.sptk.few 3b			// loop for more

	.org	ia64_vector_table + 0x3400	// Reserved
interruption_3400:
	TRAP(13)

	.org	ia64_vector_table + 0x3800	// Reserved
interruption_3800:
	TRAP(14)

	.org	ia64_vector_table + 0x3c00	// Reserved
interruption_3c00:
	TRAP(15)

	.org	ia64_vector_table + 0x4000	// Reserved
interruption_4000:
	TRAP(16)

	.org	ia64_vector_table + 0x4400	// Reserved
interruption_4400:
	TRAP(17)

	.org	ia64_vector_table + 0x4800	// Reserved
interruption_4800:
	TRAP(18)

	.org	ia64_vector_table + 0x4c00	// Reserved
interruption_4c00:
	TRAP(19)

	.org	ia64_vector_table + 0x5000	// Page Not Present vector
interruption_Page_Not_Present:
	TRAP(20)

	.org	ia64_vector_table + 0x5100	// Key Permission vector
interruption_Key_Permission:
	TRAP(21)

	.org	ia64_vector_table + 0x5200	// Instr. Access Rights vector
interruption_Instruction_Access_Rights:
	TRAP(22)

	.org	ia64_vector_table + 0x5300	// Data Access Rights vector
interruption_Data_Access_Rights:
	TRAP(23)

	.org	ia64_vector_table + 0x5400	// General Exception vector
interruption_General_Exception:
	TRAP(24)

	.org	ia64_vector_table + 0x5500	// Disabled FP-Register vector
interruption_Disabled_FP_Register:
	TRAP(25)

	.org	ia64_vector_table + 0x5600	// NaT Consumption vector
interruption_NaT_Consumption:
	TRAP(26)

	.org	ia64_vector_table + 0x5700	// Speculation vector
interruption_Speculation:
	TRAP(27)

	.org	ia64_vector_table + 0x5800	// Reserved
interruption_5800:
	TRAP(28)

	.org	ia64_vector_table + 0x5900	// Debug vector
interruption_Debug:
	TRAP(29)

	.org	ia64_vector_table + 0x5a00	// Unaligned Reference vector
interruption_Unaligned_Reference:
	TRAP(30)

	.org	ia64_vector_table + 0x5b00	// Unsupported Data Ref. vec.
interruption_Unsupported_Data_Reference:
	TRAP(31)

	.org	ia64_vector_table + 0x5c00	// Floating-point Fault vector
interruption_Floating_Point_Fault:
	TRAP(32)

	.org	ia64_vector_table + 0x5d00	// Floating-point Trap vector
interruption_Floating_Point_Trap:
	TRAP(33)

	.org	ia64_vector_table + 0x5e00	// Lower-Priv. Transfer Trap
interruption_Lower_Privilege_Transfer_Trap:
	TRAP(34)

	.org	ia64_vector_table + 0x5f00	// Taken Branch Trap vector
interruption_Taken_Branch_Trap:
	TRAP(35)

	.org	ia64_vector_table + 0x6000	// Single Step Trap vector
interruption_Single_Step_Trap:
	TRAP(36)

	.org	ia64_vector_table + 0x6100	// Reserved
interruption_6100:
	TRAP(37)

	.org	ia64_vector_table + 0x6200	// Reserved
interruption_6200:
	TRAP(38)

	.org	ia64_vector_table + 0x6300	// Reserved
interruption_6300:
	TRAP(39)

	.org	ia64_vector_table + 0x6400	// Reserved
interruption_6400:
	TRAP(40)

	.org	ia64_vector_table + 0x6500	// Reserved
interruption_6500:
	TRAP(41)

	.org	ia64_vector_table + 0x6600	// Reserved
interruption_6600:
	TRAP(42)

	.org	ia64_vector_table + 0x6700	// Reserved
interruption_6700:
	TRAP(43)

	.org	ia64_vector_table + 0x6800	// Reserved
interruption_6800:
	TRAP(44)

	.org	ia64_vector_table + 0x6900	// IA-32 Exception vector
interruption_IA_32_Exception:
	TRAP(45)

	.org	ia64_vector_table + 0x6a00	// IA-32 Intercept vector
interruption_IA_32_Intercept:
	TRAP(46)

	.org	ia64_vector_table + 0x6b00	// IA-32 Interrupt vector
interruption_IA_32_Interrupt:
	TRAP(47)

	.org	ia64_vector_table + 0x6c00	// Reserved
interruption_6c00:
	TRAP(48)

	.org	ia64_vector_table + 0x6d00	// Reserved
interruption_6d00:
	TRAP(49)

	.org	ia64_vector_table + 0x6e00	// Reserved
interruption_6e00:
	TRAP(50)

	.org	ia64_vector_table + 0x6f00	// Reserved
interruption_6f00:
	TRAP(51)

	.org	ia64_vector_table + 0x7000	// Reserved
interruption_7000:
	TRAP(52)

	.org	ia64_vector_table + 0x7100	// Reserved
interruption_7100:
	TRAP(53)

	.org	ia64_vector_table + 0x7200	// Reserved
interruption_7200:
	TRAP(54)

	.org	ia64_vector_table + 0x7300	// Reserved
interruption_7300:
	TRAP(55)

	.org	ia64_vector_table + 0x7400	// Reserved
interruption_7400:
	TRAP(56)

	.org	ia64_vector_table + 0x7500	// Reserved
interruption_7500:
	TRAP(57)

	.org	ia64_vector_table + 0x7600	// Reserved
interruption_7600:
	TRAP(58)

	.org	ia64_vector_table + 0x7700	// Reserved
interruption_7700:
	TRAP(59)

	.org	ia64_vector_table + 0x7800	// Reserved
interruption_7800:
	TRAP(60)

	.org	ia64_vector_table + 0x7900	// Reserved
interruption_7900:
	TRAP(61)

	.org	ia64_vector_table + 0x7a00	// Reserved
interruption_7a00:
	TRAP(62)

	.org	ia64_vector_table + 0x7b00	// Reserved
interruption_7b00:
	TRAP(63)

	.org	ia64_vector_table + 0x7c00	// Reserved
interruption_7c00:
	TRAP(64)

	.org	ia64_vector_table + 0x7d00	// Reserved
interruption_7d00:
	TRAP(65)

	.org	ia64_vector_table + 0x7e00	// Reserved
interruption_7e00:
	TRAP(66)

	.org	ia64_vector_table + 0x7f00	// Reserved
interruption_7f00:
	TRAP(67)

	// Make the IVT 32KB in size
	.org	ia64_vector_table + 0x8000

	.section .data.vhpt,"aw"

	.global ia64_vhpt

	.align	32768
ia64_vhpt:	.quad 0
	.align	32768

	.text

#define rIIP	r31
#define rIPSR	r30
#define rISR	r29
#define rIFA	r28
#define rPR	r27
#define rSP	r26
#define rIFS	r25
#define rR1	r24
#define rR2	r23
#define rBSPSTORE r22
#define rRNAT	r21
#define rNDIRTY	r27		/* overlay rPR */
#define rRSC	r20
#define rPFS	r19
#define rB0	r31		/* overlay rIIP */

/*
 * exception_restore:	restore interrupted state
 *
 * Arguments:
 *	sp+16	trapframe pointer
 *	r4	ar.pfs before the alloc in TRAP()
 *
 */
ENTRY(exception_restore, 0)

	alloc	r14=ar.pfs,0,0,1,0	// in case we call ast()
	add	r3=TF_CR_IPSR+16,sp
	;;
	ld8	rIPSR=[r3]
	;; 
	extr.u	r16=rIPSR,32,2		// extract ipsr.cpl
	;;
	cmp.eq	p1,p2=r0,r16		// test for return to kernel mode
	;;
(p2)	add	out0=16,sp		// trapframe argument to ast()
(p2)	br.call.dptk.many rp=ast	// note: p1, p2 preserved
	
	rsm	psr.ic|psr.dt|psr.i	// disable interrupt collection and vm
	add	r3=16,sp;
	;;
	srlz.i
	dep	r3=0,r3,61,3		// physical address
	;; 
(p2)	add	r16=SIZEOF_TRAPFRAME+16,sp  // restore ar.k6 (kernel sp)
	;; 
(p2)	mov	ar.k6=r16
	add	r1=SIZEOF_TRAPFRAME-16,r3 // r1=&tf_f[FRAME_F15]
	add	r2=SIZEOF_TRAPFRAME-32,r3 // r2=&tf_f[FRAME_F14]
	;;
	ldf.fill f15=[r1],-32		// r1=&tf_f[FRAME_F13]
	ldf.fill f14=[r2],-32		// r2=&tf_f[FRAME_F12]
	;;
	ldf.fill f13=[r1],-32		// r1=&tf_f[FRAME_F11]
	ldf.fill f12=[r2],-32		// r2=&tf_f[FRAME_F10]
	;;
	ldf.fill f11=[r1],-32		// r1=&tf_f[FRAME_F9]
	ldf.fill f10=[r2],-32		// r2=&tf_f[FRAME_F8]
	;;
	ldf.fill f9=[r1],-32		// r1=&tf_f[FRAME_F7]
	ldf.fill f8=[r2],-32		// r2=&tf_f[FRAME_F6]
	;; 
	ldf.fill f7=[r1],-24		// r1=&tf_r[FRAME_R31]
	ldf.fill f6=[r2],-16		// r2=&tf_r[FRAME_R30]
	;; 
	ld8.fill r31=[r1],-16		// r1=&tf_r[FRAME_R29]
	ld8.fill r30=[r2],-16		// r2=&tf_r[FRAME_R28]
	;; 
	ld8.fill r29=[r1],-16		// r1=&tf_r[FRAME_R27]
	ld8.fill r28=[r2],-16		// r2=&tf_r[FRAME_R26]
	;; 
	ld8.fill r27=[r1],-16		// r1=&tf_r[FRAME_R25]
	ld8.fill r26=[r2],-16		// r2=&tf_r[FRAME_R24]
	;; 
	ld8.fill r25=[r1],-16		// r1=&tf_r[FRAME_R23]
	ld8.fill r24=[r2],-16		// r2=&tf_r[FRAME_R22]
	;; 
	ld8.fill r23=[r1],-16		// r1=&tf_r[FRAME_R21]
	ld8.fill r22=[r2],-16		// r2=&tf_r[FRAME_R20]
	;; 
	ld8.fill r21=[r1],-16		// r1=&tf_r[FRAME_R19]
	ld8.fill r20=[r2],-16		// r2=&tf_r[FRAME_R18]
	;; 
	ld8.fill r19=[r1],-16		// r1=&tf_r[FRAME_R17]
	ld8.fill r18=[r2],-16		// r2=&tf_r[FRAME_R16]
	;; 
	ld8.fill r17=[r1],-16		// r1=&tf_r[FRAME_R15]
	ld8.fill r16=[r2],-16		// r2=&tf_r[FRAME_R14]
	;;
	bsw.0				// switch to bank 0
	;;
	ld8.fill r15=[r1],-16		// r1=&tf_r[FRAME_R13]
	ld8.fill r14=[r2],-16		// r2=&tf_r[FRAME_R12]
	;;
	ld8.fill r13=[r1],-16		// r1=&tf_r[FRAME_R11]
	ld8.fill r12=[r2],-16		// r2=&tf_r[FRAME_R10]
	;;
	ld8.fill r11=[r1],-16		// r1=&tf_r[FRAME_R9]
	ld8.fill r10=[r2],-16		// r2=&tf_r[FRAME_R8]
	;;
	ld8.fill r9=[r1],-16		// r1=&tf_r[FRAME_R7]
	ld8.fill r8=[r2],-16		// r2=&tf_r[FRAME_R6]
	;;
	ld8.fill r7=[r1],-16		// r1=&tf_r[FRAME_R5]
	ld8.fill r6=[r2],-16		// r2=&tf_r[FRAME_R4]
	;;
	ld8.fill r5=[r1],-16		// r1=&tf_r[FRAME_R3]
	ld8.fill r4=[r2],-16		// r2=&tf_r[FRAME_R2]
	;;
	ld8.fill r3=[r1],-16		// r1=&tf_r[FRAME_R1]
	ld8.fill rR2=[r2],-16		// r2=&tf_b[7]
	;;
	ld8.fill rR1=[r1],-16		// r1=&tf_b[6]
	ld8	r16=[r2],-16		// r2=&tf_b[5]
	;;
	mov	b7=r16
	ld8	r18=[r1],-16		// r1=&tf_b[4]
	ld8	r19=[r2],-16		// r2=&tf_b[3]
	;;
	mov	b6=r18
	mov	b5=r19
	ld8	r16=[r1],-16		// r1=&tf_b[2]
	ld8	r17=[r2],-16		// r2=&tf_b[1]
	;;
	mov	b4=r16
	mov	b3=r17
	ld8	r18=[r1],-16		// r1=&tf_b[0]
	ld8	r19=[r2],-16		// r2=&tf_ar_fpsr
	;;
	mov	b2=r18
	mov	b1=r19
	ld8	r16=[r1],-16		// r1=&tf_ar_ccv
	ld8	r17=[r2],-16		// r2=&tf_ar_unat
	;;
	mov	b0=r16
	mov	ar.fpsr=r17
	ld8	r18=[r1],-16		// r1=&tf_ndirty
	ld8	r19=[r2],-16		// r2=&tf_ar_rnat
	;;
	mov	ar.ccv=r18
	mov	ar.unat=r19
	ld8	rNDIRTY=[r1],-16	// r1=&tf_ar_bspstore
	ld8	rRNAT=[r2],-16		// r2=&tf_cr_ifs
	;;
	ld8	rBSPSTORE=[r1],-16	// r1=&tf_cr_pfs
	ld8	rIFS=[r2],-16		// r2=&tf_ar_rsc
	;;
(p1)	br.cond.dpnt.few 1f		// don't switch bs if kernel
	;;
	alloc	r16=ar.pfs,0,0,0,0	// discard current frame
	;;
	shl	r16=rNDIRTY,16		// value for ar.rsc
	;;
	mov	ar.rsc=r16		// setup for loadrs
	;;
	loadrs				// load user regs from kernel bs
	;;
	mov	ar.bspstore=rBSPSTORE
	;;
	mov	ar.rnat=rRNAT
	;;
1:	ld8	rPFS=[r1],-16		// r1=&tf_pr
	ld8	rRSC=[r2],-16		// r2=&tf_cr_ifa
	;;
	ld8	rPR=[r1],-16		// r1=&tf_cr_isr
	ld8	rIFA=[r2],-16		// r2=&tf_cr_ipsr
	;;
	ld8	rISR=[r1],-16		// r1=&tf_cr_iip
	ld8	rIPSR=[r2]
	;;
	ld8	rIIP=[r1]
	;; 
	mov	r1=rR1
	mov	r2=rR2
	mov	ar.pfs=rPFS
	mov	cr.ifs=rIFS
	mov	ar.rsc=rRSC
	mov	pr=rPR,0x1ffff
	mov	cr.ifa=rIFA
	mov	cr.iip=rIIP
	mov	cr.ipsr=rIPSR
	;;
	rfi

END(exception_restore)
	

/*
 * exception_save: save interrupted state
 *
 * Arguments:
 *	b0	return address
 *	r16	saved b0
 *
 * Return:
 *	r14	cr.iim value for break traps
 *	sp	kernel stack pointer
 *	p1	true if user mode
 *	p2	true if kernel mode
 *	p3	true if interrupts were enabled
 */
ENTRY(exception_save, 0)
	rsm	psr.dt			// turn off data translations
	;;
	srlz.d				// serialize
	;;
	mov	rIIP=cr.iip
	mov	rIPSR=cr.ipsr
	mov	rISR=cr.isr
	mov	rIFA=cr.ifa
	mov	rPR=pr
	;; 
	tbit.nz	p3,p0=rIPSR,14		// check for interrupt enable state
	extr.u	r17=rIPSR,32,2		// extract ipsr.cpl
	;;
	cmp.eq	p1,p2=r0,r17		// test for kernel mode
	;;
	mov	rSP=sp			// save sp
	;; 
(p2)	mov	sp=ar.k6		// and switch to kernel stack
	;;
	add	sp=-SIZEOF_TRAPFRAME,sp	// reserve trapframe
	mov	rR1=r1
	mov	rR2=r2
	;; 
	dep	r1=0,sp,61,3		// r1=&tf_flags
	;;
	add	r2=16,r1		// r2=&tf_cr_ipsr
	st8	[r1]=r0,8		// zero flags, r1=&tf_cr_iip
	;; 
	st8	[r1]=rIIP,16		// r1=&tf_cr_isr
	st8	[r2]=rIPSR,16		// r2=&tf_cr_ifa
	;; 
	st8	[r1]=rISR,16		// r1=&tf_pr
	st8	[r2]=rIFA,16		// r2=&tf_ar_rsc
	;; 
	st8	[r1]=rPR,16		// r1=&tf_cr_pfs

	mov	rB0=r16
	mov	rRSC=ar.rsc
	mov	rPFS=ar.pfs
	cover
	mov	rIFS=cr.ifs
	;; 
	mov	ar.rsc=0
	;; 
	mov	rBSPSTORE=ar.bspstore
	;; 
(p2)	mov	r16=ar.k5		// kernel backing store
	mov	rRNAT=ar.rnat
	;; 
(p1)	mov	r16=rBSPSTORE		// so we can figure out ndirty
(p2)	mov	ar.bspstore=r16		// switch bspstore
	st8	[r2]=rRSC,16		// r2=&tf_cr_ifs
	;; 
	st8	[r1]=rPFS,16		// r1=&tf_ar_bspstore
	st8	[r2]=rIFS,16		// r2=&tf_ar_rnat
	mov	r17=ar.bsp
	;;
	sub	r17=r17,r16		// ndirty (in bytes)
	;; 
	st8	[r1]=rBSPSTORE,16	// r1=&tf_ndirty
	st8	[r2]=rRNAT,16		// r2=&tf_ar_unat
	;; 
	st8	[r1]=r17,16		// r1=&tf_ar_ccv
	mov	ar.rsc=3		// switch RSE back on
	mov	r16=ar.unat
	;; 
	mov	r17=ar.ccv
	st8	[r2]=r16,16		// r2=&tf_ar_fpsr
	mov	r18=ar.fpsr
	;; 
	st8	[r1]=r17,16		// r1=&tf_b[0]
	st8	[r2]=r18,16		// r2=&tf_b[1]
	mov	r17=b1
	;; 
	st8	[r1]=rB0,16		// r1=&tf_b[2]
	mov	r18=b2
	st8	[r2]=r17,16		// r2=&tf_b[3]
	;; 
	mov	r17=b3
	st8	[r1]=r18,16		// r1=&tf_b[4]
	;; 
	mov	r18=b4
	st8	[r2]=r17,16		// r2=&tf_b[5]
	;; 
	mov	r17=b5
	st8	[r1]=r18,16		// r1=&tf_b[6]
	;; 
	mov	r18=b6
	st8	[r2]=r17,16		// r2=&tf_b[7]
	;; 
	mov	r17=b7
	st8	[r1]=r18,16		// r1=&tf_r[FRAME_R1]
	;; 
	st8	[r2]=r17,16		// r2=&tf_r[FRAME_R2]
	;; 
	.mem.offset 0,0
	st8.spill [r1]=rR1,16		// r1=&tf_r[FRAME_R3]
	.mem.offset 8,0
	st8.spill [r2]=rR2,16		// r2=&tf_r[FRAME_R4]
	;; 
	.mem.offset 0,0
	st8.spill [r1]=r3,16		// r1=&tf_r[FRAME_R5]
	.mem.offset 8,0
	st8.spill [r2]=r4,16		// r2=&tf_r[FRAME_R6]
	;; 
	.mem.offset 0,0
	st8.spill [r1]=r5,16		// r1=&tf_r[FRAME_R7]
	.mem.offset 8,0
	st8.spill [r2]=r6,16		// r2=&tf_r[FRAME_R8]
	;; 
	.mem.offset 0,0
	st8.spill [r1]=r7,16		// r1=&tf_r[FRAME_R9]
	.mem.offset 8,0
	st8.spill [r2]=r8,16		// r2=&tf_r[FRAME_R10]
	;; 
	.mem.offset 0,0
	st8.spill [r1]=r9,16		// r1=&tf_r[FRAME_R11]
	.mem.offset 8,0
	st8.spill [r2]=r10,16		// r2=&tf_r[FRAME_SP]
	;; 
	.mem.offset 0,0
	st8.spill [r1]=r11,16		// r1=&tf_r[FRAME_R13]
	.mem.offset 8,0
	st8.spill [r2]=rSP,16		// r2=&tf_r[FRAME_R14]
	;; 
	.mem.offset 0,0
	st8.spill [r1]=r13,16		// r1=&tf_r[FRAME_R15]
	.mem.offset 8,0
	st8.spill [r2]=r14,16		// r2=&tf_r[FRAME_R16]
	;; 
	.mem.offset 0,0
	st8.spill [r1]=r15,16		// r1=&tf_r[FRAME_R17]
	;; 
	bsw.1				// switch to bank 1
	;; 
	.mem.offset 8,0
	st8.spill [r2]=r16,16		// r2=&tf_r[FRAME_R18]
	.mem.offset 0,0
	st8.spill [r1]=r17,16		// r1=&tf_r[FRAME_R19]
	;; 
	.mem.offset 8,0
	st8.spill [r2]=r18,16		// r2=&tf_r[FRAME_R20]
	.mem.offset 0,0
	st8.spill [r1]=r19,16		// r1=&tf_r[FRAME_R21]
	;; 
	.mem.offset 8,0
	st8.spill [r2]=r20,16		// r2=&tf_r[FRAME_R22]
	.mem.offset 0,0
	st8.spill [r1]=r21,16		// r1=&tf_r[FRAME_R23]
	;; 
	.mem.offset 8,0
	st8.spill [r2]=r22,16		// r2=&tf_r[FRAME_R24]
	.mem.offset 0,0
	st8.spill [r1]=r23,16		// r1=&tf_r[FRAME_R25]
	;; 
	.mem.offset 8,0
	st8.spill [r2]=r24,16		// r2=&tf_r[FRAME_R26]
	.mem.offset 0,0
	st8.spill [r1]=r25,16		// r1=&tf_r[FRAME_R27]
	;; 
	.mem.offset 8,0
	st8.spill [r2]=r26,16		// r2=&tf_r[FRAME_R28]
	.mem.offset 0,0
	st8.spill [r1]=r27,16		// r1=&tf_r[FRAME_R29]
	;; 
	.mem.offset 8,0
	st8.spill [r2]=r28,16		// r2=&tf_r[FRAME_R30]
	.mem.offset 0,0
	st8.spill [r1]=r29,16		// r1=&tf_r[FRAME_R31]
	;; 
	.mem.offset 8,0
	st8.spill [r2]=r30,16		// r2=&tf_f[FRAME_F6]
	.mem.offset 0,0
	st8.spill [r1]=r31,24		// r1=&tf_f[FRAME_F7]
	;; 
	stf.spill [r2]=f6,32		// r2=&tf_f[FRAME_F8]
	stf.spill [r1]=f7,32		// r1=&tf_f[FRAME_F9]
	;; 
	stf.spill [r2]=f8,32		// r2=&tf_f[FRAME_F10]
	stf.spill [r1]=f9,32		// r1=&tf_f[FRAME_F11]
	;; 
	stf.spill [r2]=f10,32		// r2=&tf_f[FRAME_F12]
	stf.spill [r1]=f11,32		// r1=&tf_f[FRAME_F13]
	;; 
	stf.spill [r2]=f12,32		// r2=&tf_f[FRAME_F14]
	stf.spill [r1]=f13,32		// r1=&tf_f[FRAME_F15]
	;; 
	stf.spill [r2]=f14		// 
	stf.spill [r1]=f15		// 
	;; 
	movl	r1=__gp			// kernel globals
	mov	r13=ar.k4		// processor globals
	mov	r14=cr.iim		// break immediate
	ssm	psr.ic|psr.dt		// enable interrupts & translation
	;;
	srlz.i				// serialize
	;;
	br.sptk.few b0			// not br.ret - we were not br.call'ed

END(exception_save)
	
/*
 * System call entry point (via Break Instruction vector).
 *
 * Arguments:
 *	r15		System call number
 *	out0-out7	System call arguments
 */
ENTRY(do_syscall, 0)
	.prologue
	.save	rp,r0
	.body
	// Save minimal state for syscall.
	// We need to save enough state so that sendsig doesn't
	// trash things if we take a signal during the system call.
	// Essentially we need to save all the function-preserved
	// state. Note that if we don't take a signal, we don't need
	// to restore much of that state on the way out. Note also
	// that when we save r4-r7 we spill their NaT bits into
	// ar.unat. This register is preserved by the call to
	// syscall() and if a full restore is needed,
	// exception_restore will recover the NaT bits from ar.unat.
	// The function-preserved state (including syscall number) is:
	//
	//	r1,r4-r7,sp,r15
	//	f16-f31
	//	p1-p5,p16-p63
	//	b0-b5
	//	various ar's
	//
{ .mmi					// start reading high latency regs
	mov	r16=cr.ipsr		// (13)
	mov.m	r17=ar.rsc		// (13)
	mov	r18=sp			// save user sp
	;;
} { .mmi
	mov	sp=ar.k6		// (13) kernel sp
	mov	r19=cr.isr		// (13)
	nop.i	0
} { .mmi
	mov.m	ar.rsc=0
	;; 
	mov.m	r20=ar.bspstore		// (13)
	nop.i	0
} { .mmi
	mov.m	r21=ar.k5		// (13)
	mov.m	r22=ar.rnat		// (6)
	nop.i	0
} { .mmi
	mov.m	r23=ar.unat		// (6)
	rsm	psr.dt			// (5) physical addressing
} { .mii
	mov	r24=cr.iip		// (2)
	mov.i	r25=ar.pfs		// (2)
	add	sp=-SIZEOF_TRAPFRAME,sp // reserve trapframe
	;; 
} { .mii
	addl	r27=FRAME_SYSCALL,r0	// (1)
	mov	r26=pr			// (2)
	dep	r30=0,sp,61,3		// physical address
} { .mmi
	srlz.d				// serialize psr.dt
	;; 
	add	r31=8,r30		// secondary pointer
	;; 
} { .mmi
	st8	[r30]=r27,16		// tf_flags
	st8	[r31]=r24,16		// save cr.iip
	mov	r28=b0
	;;
} { .mmi
	st8	[r30]=r16,24		// save cr.ipsr, skip to pr
	st8	[r31]=r19,24		// save cr.isr, skip to ar.rsc
	mov	r24=b1
	;;
} { .mmi
	st8	[r30]=r26,16		// save pr, skip to ar.pfs
	st8	[r31]=r17,24		// save ar.rsc, skip to ar.bspstore
	mov	r27=b2
	;;
} { .mii
	st8	[r30]=r25,24		// save ar.pfs, skip to ar.rnat
	mov	r16=b3
	mov	r17=b4
	;;
} { .mmi
	st8	[r31]=r20,24		// save ar.bspstore, skip to ar.unat
	mov.m	ar.bspstore=r21		// switch to kernel backing store
	mov	r29=b5
	;;
} { .mmi
	mov.m	r20=ar.ccv
	mov.m	r21=ar.fpsr
	nop.i	0
	;; 
} { .mmi
	st8	[r30]=r22,24		// save ar.rnat, skip to ar.ccv
	st8	[r31]=r23,16		// save ar.unat, skip to ar.fpsr
	nop.i	0
	;;
} { .mmi
	st8	[r30]=r20,16		// save ar.ccv, skip to b0
	st8	[r31]=r21,16		// save ar.fpsr, skip to b1
	nop.i	0
	;;
} { .mmi
	st8	[r30]=r28,16		// save b0, skip to b2
	st8	[r31]=r24,16		// save b1, skip to b3
	nop.i	0
	;; 
} { .mmi
	st8	[r30]=r27,16		// save b2, skip to b4
	st8	[r31]=r16,16		// save b3, skip to b5
	nop.i	0
	;;
} { .mmi
	st8	[r30]=r17,TF_R_R1-(TF_B+4*8) // save b4, skip to r1
	st8	[r31]=r29,TF_R_R4-(TF_B+5*8) // save b5, skip to r4
	nop.i	0
	;;
} { .mmi
	st8	[r30]=r1,TF_R_R5-TF_R_R1 // save r1, skip to r5
	.mem.offset 8,0
	st8.spill [r31]=r4,16		// save r4, skip to r6
	nop.i	0
	;;
} { .mmi
	.mem.offset 0,0
	st8.spill [r30]=r5,16		// save r5, skip to r7
	.mem.offset 8,0
	st8.spill [r31]=r6,TF_R_SP-TF_R_R6 // save r6, skip to sp
	nop.i	0
	;;
} { .mmi
	.mem.offset 0,0
	st8.spill [r30]=r7,TF_R_R15-TF_R_R7 // save r7, skip to r15
	st8	[r31]=r18		// save sp
	nop.i	0
	;; 
} { .mmb
	st8	[r30]=r15		// save r15 (syscall number)
	add	sp=-(8*8),sp		// reserve stack for arguments
	br.call.sptk.few b0=Lsaveargs	// dump args
} { .mmb
	mov.m	r13=ar.k4		// processor globals
	nop.m	0
	bsw.1				// switch back to bank 1
	;;
} { .mmb
	mov	r16=sp			// point at args
	mov.m	r17=ar.k5		// for calculating ndirty
	cover				// preserve user register frame
	;;
} { .mmi
	mov	r18=cr.ifs		// record user's CFM
	mov.m	r19=ar.bsp		// ndirty = ar.bsp - kbsp
	add	sp=-16,sp		// reserve scratch space
	;;
} { .mmi
	add	r20=TF_CR_IFS+(8*8),r16	// point at cr.ifs
	ssm	psr.ic|psr.dt		// reenable traps and translation
	sub	r19=r19,r17		// calculate ndirty
	;;
} { .mmi
	srlz.i				// serialize psr.ic and psr.dt
	;;
	ssm	psr.i			// safe to take interrupts again
	add	r21=TF_NDIRTY+(8*8),r16	// point at ndirty
	;;
} { .mmi
	st8	[r20]=r18		// save cr.ifs
	st8	[r21]=r19		// save ndirty
	;;
} { .mmi
	alloc	r14=ar.pfs,0,1,3,0
	srlz.d				// serialize psr.i
	add	loc0=(8*8),r16		// remember where trapframe is
	;;
} { .mlx
	mov	out0=r15		// syscall number (from user)
	movl	gp=__gp			// kernel globals
} { .mmb
	mov	out1=r16		// arguments
	add	out2=(8*8),r16		// trapframe pointer
	br.call.sptk.many rp=syscall	// do the work
} { .mii
	ld8	r14=[loc0]		// check tf_flags
	dep	r15=0,loc0,61,3		// physical address of trapframe
	;;
	tbit.z p6,p0=r14,0		// check FRAME_SYSCALL bit
	;;
} { .mib
(p6)	add	sp=-16,loc0		// do a full restore if clear
	add	r16=SIZEOF_TRAPFRAME,loc0 // new kernel sp
(p6)	br.dpnt.many exception_restore
} { .mmi
	rsm 	psr.dt|psr.ic|psr.i	// get ready to restore
	;;
	srlz.i				// serialise psr.dt and psr.ic
	add	r30=TF_R_R15,r15	// point at r15
	;; 
} { .mmi
	alloc	r14=ar.pfs,0,0,0,0	// discard register frame
	mov	ar.k6=r16		// restore kernel sp
	add	r31=TF_R_SP,r15		// point at sp
	;;
} { .mmi
	ld8	r15=[r30],TF_R_R10-TF_R_R15 // restore r15, skip to r10
	ld8	sp=[r31],TF_R_R9-TF_R_SP // restore user sp, skip to r9
	nop.i	0
	;;
} { .mmi
	ld8	r10=[r30],-16		// restore r10, skip to r8
	ld8	r9=[r31],TF_R_R1-TF_R_R9 // restore r9, skip to r1
	nop.i	0
	;;
} { .mmi
	ld8	r8=[r30],TF_B-TF_R_R8	// restore r8, skip to b0
	ld8	r1=[r31],TF_AR_FPSR-TF_R_R1 // restore r1, skip to ar.fpsr
	nop.i	0
	;;
} { .mmi
	ld8	r16=[r30],-16		// restore b0, skip to ar.ccv
	ld8	r17=[r31],-16		// restore ar.fpsr, skip to ar.unat
	nop.i	0
	;;
} { .mmi
	ld8	r18=[r30],-16		// restore ar.ccv, skip to ndirty
	ld8	r19=[r31],-16		// restore ar.unat, skip to ar.rnat
	mov	b0=r16
	;;
} { .mmi
	ld8	r20=[r30],-16		// restore ndirty, skip to ar.bspstore
	ld8	r21=[r31],-16		// restore ar.rnat, skip to cr.ifs
	nop.i	0
	;; 
} { .mmi
	ld8	r16=[r30],-16		// restore ar.bspstore, skip to ar.pfs
	mov	ar.fpsr=r17
	shl	r20=r20,16		// value for ar.rsc
	;; 
} { .mmi
	ld8	r22=[r31],-16		// restore cr.ifs, skip to ar.rsc
	mov.m	ar.ccv=r18
	nop.i	0
	;;
} { .mmi
	ld8	r17=[r30],-16		// restore ar.pfs, skip to pr
	mov.m	ar.unat=r19
	nop.i	0
	;;
} { .mmi
	ld8	r18=[r31],-32		// restore ar.rsc, skip to cr.ipsr
	mov.m	ar.rsc=r20		// setup for loadrs
	nop.i	0
	;;
} { .mmi
	loadrs				// restore user stacked registers
	;; 
	mov.m	ar.bspstore=r16		// back to user backing store
	mov.i	ar.pfs=r17	
	;;
} { .mmi
	mov.m	ar.rnat=r21
	mov.m	ar.rsc=r18
	nop.i	0
	;;
} { .mmi
	ld8	r16=[r30],-32		// restore pr, skip to cr.iip
	ld8	r17=[r31]		// restore cr.ipsr
	nop.i	0
	;;
} { .mmi
	ld8	r18=[r30]		// restore cr.iip
	mov	cr.ifs=r22
	nop.i	0
	;;
} { .mmi
	mov	cr.iip=r18
	mov	cr.ipsr=r17
	mov	pr=r16,0x1ffff
	;;
} { .bbb
	rfi
}	

	// This is done as a function call to make sure that we only
	// have output registers in the register frame. It also gives
	// us a chance to use alloc to round up to 8 arguments for
	// simplicity.
	//
	// We are still running in physical mode with psr.ic==0 because
	// we haven't yet covered the user's register frame to get a
	// value for cr.ifs
Lsaveargs:
{ .mii
	alloc	r14=ar.pfs,0,0,8,0	// round up to 8 outputs
	extr.u	r31=sp,0,61		// physical address
	;;
	add	r30=8,r31
	;;
} { .mmi
	st8	[r31]=r32,16
	st8	[r30]=r33,16
	;;
} { .mmi
	st8	[r31]=r34,16
	st8	[r30]=r35,16
	;; 
} { .mmi
	st8	[r31]=r36,16
	st8	[r30]=r37,16
	;; 
} { .mmb
	st8	[r31]=r38
	st8	[r30]=r39
	br.ret.sptk.many b0
}
	.global do_syscall_end
do_syscall_end:

END(do_syscall)
