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
1:	mov	r17=ip;;			\
	add	r17=2f-1b,r17;			\
	mov	r16=b6;;			\
	mov	b6=r17;				\
	br.sptk.few exception_save;		\
2: (p3)	ssm	psr.i;				\
	alloc	r15=ar.pfs,0,0,3,0;		\
	mov	out0=_n_;			\
	mov	out1=r14;			\
	mov	out2=sp;;			\
	add	sp=-16,sp;;			\
	br.call.sptk.few rp=trap;		\
3:	br.sptk.many exception_restore

#define	IVT_ENTRY(name, offset)			\
	.org	ia64_vector_table + offset;	\
	.global	ivt_##name;			\
	.proc	ivt_##name;			\
	.prologue;				\
	.unwabi	@svr4, 'I';			\
	.save	rp, r0;				\
	.body;					\
ivt_##name:

#define	IVT_END(name)				\
	.endp	ivt_##name;			\
	.align	0x100

/*
 * The IA64 Interrupt Vector Table (IVT) contains 20 slots with 64
 * bundles per vector and 48 slots with 16 bundles per vector.
 */

	.section .text.ivt,"ax"

	.align	32768
	.global ia64_vector_table
	.size	ia64_vector_table, 32768
ia64_vector_table:

IVT_ENTRY(VHPT_Translation, 0x0000)
	TRAP(0)
IVT_END(VHPT_Translation)

IVT_ENTRY(Instruction_TLB, 0x0400)
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
	cmp.ne	p1,p0=r21,r19
(p1)	br.dpnt.few 1f
	;;
	ld8	r21=[r18]		// read pte
	mov	pr=r17,0x1ffff
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
2:	cmp.eq	p1,p0=r0,r20		// done?
(p1)	br.cond.spnt.few 9f		// bail if done
	;;
	add	r21=16,r20		// tag location
	;;
	ld8	r21=[r21]		// read tag
	;;
	cmp.ne	p1,p0=r21,r19		// compare tags
(p1)	br.cond.sptk.few 3f		// if not, read next in chain
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
IVT_END(Instruction_TLB)

IVT_ENTRY(Data_TLB, 0x0800)
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
	cmp.ne	p1,p0=r21,r19
(p1)	br.dpnt.few 1f
	;;
	ld8	r21=[r18]		// read pte
	mov	pr=r17,0x1ffff
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
2:	cmp.eq	p1,p0=r0,r20		// done?
(p1)	br.cond.spnt.few 9f		// bail if done
	;;
	add	r21=16,r20		// tag location
	;;
	ld8	r21=[r21]		// read tag
	;;
	cmp.ne	p1,p0=r21,r19		// compare tags
(p1)	br.cond.sptk.few 3f		// if not, read next in chain
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
IVT_END(Data_TLB)

IVT_ENTRY(Alternate_Instruction_TLB, 0x0c00)
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
IVT_END(Alternate_Instruction_TLB)

IVT_ENTRY(Alternate_Data_TLB, 0x1000)
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
IVT_END(Alternate_Data_TLB)

IVT_ENTRY(Data_Nested_TLB, 0x1400)
	TRAP(5)
IVT_END(Data_Nested_TLB)

IVT_ENTRY(Instruction_Key_Miss, 0x1800)
	TRAP(6)
IVT_END(Instruction_Key_Miss)

IVT_ENTRY(Data_Key_Miss, 0x1c00)
	TRAP(7)
IVT_END(Data_Key_Miss)

IVT_ENTRY(Dirty_Bit, 0x2000)
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
1:	cmp.eq	p1,p0=r0,r20		// done?
(p1)	br.cond.spnt.few 9f		// bail if done
	;;
	add	r21=16,r20		// tag location
	;;
	ld8	r21=[r21]		// read tag
	;;
	cmp.ne	p1,p0=r21,r19		// compare tags
(p1)	br.cond.sptk.few 2f		// if not, read next in chain
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
IVT_END(Dirty_Bit)

IVT_ENTRY(Instruction_Access_Bit, 0x2400)
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
1:	cmp.eq	p1,p0=r0,r20		// done?
(p1)	br.cond.spnt.few 9f		// bail if done
	;;
	add	r21=16,r20		// tag location
	;;
	ld8	r21=[r21]		// read tag
	;;
	cmp.ne	p1,p0=r21,r19		// compare tags
(p1)	br.cond.sptk.few 2f		// if not, read next in chain
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
IVT_END(Instruction_Access_Bit)

IVT_ENTRY(Data_Access_Bit, 0x2800)
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
1:	cmp.eq	p1,p0=r0,r20		// done?
(p1)	br.cond.spnt.few 9f		// bail if done
	;;
	add	r21=16,r20		// tag location
	;;
	ld8	r21=[r21]		// read tag
	;;
	cmp.ne	p1,p0=r21,r19		// compare tags
(p1)	br.cond.sptk.few 2f		// if not, read next in chain
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
IVT_END(Data_Access_Bit)

IVT_ENTRY(Break_Instruction, 0x2c00)
	mov	r16=pr			// save pr for a moment
	mov	r17=cr.iim;;		// read break value
	mov	r18=0x100000;;		// syscall number
	cmp.ne	p6,p0=r18,r17;;		// check for syscall
(p6)	br.dpnt.few 9f

	mov	r17=cr.ipsr;;		// check for user mode
	extr.u	r17=r17,32,2;;
	cmp.eq	p6,p0=r0,r17
(p6)	br.dpnt.few 9f			// trap if kernel mode

	// Note: p6 and p7 are temporaries so we don't need to restore
	// the value of pr here since the user-mode program assumes
	// that syscalls only preserve the function-preserved state.

	br.sptk.many	do_syscall
	;;
9:	mov	pr=r16,0x1ffff		// restore pr
	TRAP(11)
IVT_END(Break_Instruction)

IVT_ENTRY(External_Interrupt, 0x3000)
	mov	r16=b6			// save user's b6
1:	mov	r17=ip;;		// construct return address
	add	r17=2f-1b,r17;;		// for exception_save
	mov	b6=r17
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
IVT_END(External_Interrupt)

IVT_ENTRY(Reserved_3400, 0x3400)
	TRAP(13)
IVT_END(Reserved_3400)

IVT_ENTRY(Reserved_3800, 0x3800)
	TRAP(14)
IVT_END(Reserved_3800)

IVT_ENTRY(Reserved_3c00, 0x3c00)
	TRAP(15)
IVT_END(Reserved_3c00)

IVT_ENTRY(Reserved_4000, 0x4000)
	TRAP(16)
IVT_END(Reserved_4000)

IVT_ENTRY(Reserved_4400, 0x4400)
	TRAP(17)
IVT_END(Reserved_4400)

IVT_ENTRY(Reserved_4800, 0x4800)
	TRAP(18)
IVT_END(Reserved_4800)

IVT_ENTRY(Reserved_4c00, 0x4c00)
	TRAP(19)
IVT_END(Reserved_4c00)

IVT_ENTRY(Page_Not_Present, 0x5000)
	TRAP(20)
IVT_END(Page_Not_Present)

IVT_ENTRY(Key_Permission, 0x5100)
	TRAP(21)
IVT_END(Key_Permission)

IVT_ENTRY(Instruction_Access_Rights, 0x5200)
	TRAP(22)
IVT_END(Instruction_Access_Rights)

IVT_ENTRY(Data_Access_Rights, 0x5300)
	TRAP(23)
IVT_END(Data_Access_Rights)

IVT_ENTRY(General_Exception, 0x5400)
	TRAP(24)
IVT_END(General_Exception)

IVT_ENTRY(Disabled_FP_Register, 0x5500)
	TRAP(25)
IVT_END(Disabled_FP_Register)

IVT_ENTRY(NaT_Consumption, 0x5600)
	TRAP(26)
IVT_END(NaT_Consumption)

IVT_ENTRY(Speculation, 0x5700)
	TRAP(27)
IVT_END(Speculation)

IVT_ENTRY(Reserved_5800, 0x5800)
	TRAP(28)
IVT_END(Reserved_5800)

IVT_ENTRY(Debug, 0x5900)
	TRAP(29)
IVT_END(Debug)

IVT_ENTRY(Unaligned_Reference, 0x5a00)
	TRAP(30)
IVT_END(Unaligned_Reference)

IVT_ENTRY(Unsupported_Data_Reference, 0x5b00)
	TRAP(31)
IVT_END(Unsupported_Data_Reference)

IVT_ENTRY(Floating_Point_Fault, 0x5c00)
	TRAP(32)
IVT_END(Floating_Point_Fault)

IVT_ENTRY(Floating_Point_Trap, 0x5d00)
	TRAP(33)
IVT_END(Floating_Point_Trap)

IVT_ENTRY(Lower_Privilege_Transfer_Trap, 0x5e00)
	TRAP(34)
IVT_END(Lower_Privilege_Transfer_Trap)

IVT_ENTRY(Taken_Branch_Trap, 0x5f00)
	TRAP(35)
IVT_END(Taken_Branch_Trap)

IVT_ENTRY(Single_Step_Trap, 0x6000)
	TRAP(36)
IVT_END(Single_Step_Trap)

IVT_ENTRY(Reserved_6100, 0x6100)
	TRAP(37)
IVT_END(Reserved_6100)

IVT_ENTRY(Reserved_6200, 0x6200)
	TRAP(38)
IVT_END(Reserved_6200)

IVT_ENTRY(Reserved_6300, 0x6300)
	TRAP(39)
IVT_END(Reserved_6300)

IVT_ENTRY(Reserved_6400, 0x6400)
	TRAP(40)
IVT_END(Reserved_6400)

IVT_ENTRY(Reserved_6500, 0x6500)
	TRAP(41)
IVT_END(Reserved_6500)

IVT_ENTRY(Reserved_6600, 0x6600)
	TRAP(42)
IVT_END(Reserved_6600)

IVT_ENTRY(Reserved_6700, 0x6700)
	TRAP(43)
IVT_END(Reserved_6700)

IVT_ENTRY(Reserved_6800, 0x6800)
	TRAP(44)
IVT_END(Reserved_6800)

IVT_ENTRY(IA_32_Exception, 0x6900)
	TRAP(45)
IVT_END(IA_32_Exception)

IVT_ENTRY(IA_32_Intercept, 0x6a00)
	TRAP(46)
IVT_END(IA_32_Intercept)

IVT_ENTRY(IA_32_Interrupt, 0x6b00)
	TRAP(47)
IVT_END(IA_32_Interrupt)

IVT_ENTRY(Reserved_6c00, 0x6c00)
	TRAP(48)
IVT_END(Reserved_6c00)

IVT_ENTRY(Reserved_6d00, 0x6d00)
	TRAP(49)
IVT_END(Reserved_6d00)

IVT_ENTRY(Reserved_6e00, 0x6e00)
	TRAP(50)
IVT_END(Reserved_6e00)

IVT_ENTRY(Reserved_6f00, 0x6f00)
	TRAP(51)
IVT_END(Reserved_6f00)

IVT_ENTRY(Reserved_7000, 0x7000)
	TRAP(52)
IVT_END(Reserved_7000)

IVT_ENTRY(Reserved_7100, 0x7100)
	TRAP(53)
IVT_END(Reserved_7100)

IVT_ENTRY(Reserved_7200, 0x7200)
	TRAP(54)
IVT_END(Reserved_7200)

IVT_ENTRY(Reserved_7300, 0x7300)
	TRAP(55)
IVT_END(Reserved_7300)

IVT_ENTRY(Reserved_7400, 0x7400)
	TRAP(56)
IVT_END(Reserved_7400)

IVT_ENTRY(Reserved_7500, 0x7500)
	TRAP(57)
IVT_END(Reserved_7500)

IVT_ENTRY(Reserved_7600, 0x7600)
	TRAP(58)
IVT_END(Reserved_7600)

IVT_ENTRY(Reserved_7700, 0x7700)
	TRAP(59)
IVT_END(Reserved_7700)

IVT_ENTRY(Reserved_7800, 0x7800)
	TRAP(60)
IVT_END(Reserved_7800)

IVT_ENTRY(Reserved_7900, 0x7900)
	TRAP(61)
IVT_END(Reserved_7900)

IVT_ENTRY(Reserved_7a00, 0x7a00)
	TRAP(62)
IVT_END(Reserved_7a00)

IVT_ENTRY(Reserved_7b00, 0x7b00)
	TRAP(63)
IVT_END(Reserved_7b00)

IVT_ENTRY(Reserved_7c00, 0x7c00)
	TRAP(64)
IVT_END(Reserved_7c00)

IVT_ENTRY(Reserved_7d00, 0x7d00)
	TRAP(65)
IVT_END(Reserved_7d00)

IVT_ENTRY(Reserved_7e00, 0x7e00)
	TRAP(66)
IVT_END(Reserved_7e00)

IVT_ENTRY(Reserved_7f00, 0x7f00)
	TRAP(67)
IVT_END(Reserved_7f00)

	.section .data.vhpt,"aw"

	.align	32768
	.global ia64_vhpt
	.size	ia64_vhpt, 32768
ia64_vhpt:
	.skip	32768

	.text

/*
 * exception_restore:	restore interrupted state
 *
 * Arguments:
 *	sp+16	trapframe pointer
 *	r4	ar.pfs before the alloc in TRAP()
 *
 */
ENTRY(exception_restore, 0)
{	.mfi
	alloc	r14=ar.pfs,0,0,1,0	// in case we call ast()
	nop	1
	add	r3=TF_CR_IPSR+16,sp
	;;
}
{	.mmi
	ld8	r30=[r3]		// ipsr
	;;
	nop	2
	extr.u	r16=r30,32,2		// extract ipsr.cpl
	;;
}
{	.mfb
	cmp.eq	p1,p2=r0,r16		// test for return to kernel mode
	nop	3
(p1)	br.cond.dpnt 2f			// no ast check for returns to kernel
}
3:
{	.mmi
	add	r3=PC_CURTHREAD,r13	// &curthread
	;;
	ld8	r3=[r3]			// curthread
	add	r2=(KEF_ASTPENDING|KEF_NEEDRESCHED),r0
	;;
}
{	.mmb
	add	r3=TD_KSE,r3		// &curthread->td_kse
	mov	r15=psr			// save interrupt enable status
	nop	4
	;;
}
{	.mmi
	ld8	r3=[r3]			// curkse
	;;
	rsm	psr.i			// disable interrupts
	add	r3=KE_FLAGS,r3		// &curkse->ke_flags
	;;
}
{	.mmi
	ld4	r14=[r3]		// fetch curkse->ke_flags
	;;
	and	r14=r2,r14	    // flags & (KEF_ASTPENDING|KEF_NEEDRESCHED)
	nop	5
	;;
}
{	.mfb
	cmp4.eq	p6,p7=r0,r14		//  == 0 ?
	nop	6
(p6)	br.cond.dptk	2f
	;;
}
{	.mmi
	mov	psr.l=r15		// restore interrups
	;;
	srlz.d
	add	out0=16,sp		// trapframe argument to ast()
}
{	.bbb
	br.call.sptk.many rp=ast	// note: p1, p2 preserved
	br.sptk	3b
	nop	7
}
2:
{	.mmi
	rsm	psr.ic|psr.dt|psr.i	// disable interrupt collection and vm
	;;
	srlz.i
	add	r3=16,sp
	;;
}
{	.mmi
(p2)	add	r16=SIZEOF_TRAPFRAME+16,sp  // restore ar.k6 (kernel sp)
	;;
(p2)	mov	ar.k6=r16
	dep	r3=0,r3,61,3		// physical address
	;; 
}
{	.mmi
	add	r1=SIZEOF_TRAPFRAME-16,r3 // r1=&tf_f[FRAME_F15]
	;;
	ldf.fill f15=[r1],-32		// r1=&tf_f[FRAME_F13]
	add	r2=SIZEOF_TRAPFRAME-32,r3 // r2=&tf_f[FRAME_F14]
	;;
}
{	.mmb
	ldf.fill f14=[r2],-32		// r2=&tf_f[FRAME_F12]
	ldf.fill f13=[r1],-32		// r1=&tf_f[FRAME_F11]
	nop	8
	;;
}
{	.mmi
	ldf.fill f12=[r2],-32		// r2=&tf_f[FRAME_F10]
	ldf.fill f11=[r1],-32		// r1=&tf_f[FRAME_F9]
	nop	9
	;;
}
{	.mmb
	ldf.fill f10=[r2],-32		// r2=&tf_f[FRAME_F8]
	ldf.fill f9=[r1],-32		// r1=&tf_f[FRAME_F7]
	nop	10
	;;
}
{	.mmi
	ldf.fill f8=[r2],-32		// r2=&tf_f[FRAME_F6]
	ldf.fill f7=[r1],-24		// r1=&tf_r[FRAME_R31]
	nop	11
	;;
}
{	.mmb
	ldf.fill f6=[r2],-16		// r2=&tf_r[FRAME_R30]
	ld8.fill r31=[r1],-16		// r1=&tf_r[FRAME_R29]
	nop	12
	;;
}
{	.mmi
	ld8.fill r30=[r2],-16		// r2=&tf_r[FRAME_R28]
	ld8.fill r29=[r1],-16		// r1=&tf_r[FRAME_R27]
	nop	13
	;;
}
{	.mmb
	ld8.fill r28=[r2],-16		// r2=&tf_r[FRAME_R26]
	ld8.fill r27=[r1],-16		// r1=&tf_r[FRAME_R25]
	nop	14
	;;
}
{	.mmi
	ld8.fill r26=[r2],-16		// r2=&tf_r[FRAME_R24]
	ld8.fill r25=[r1],-16		// r1=&tf_r[FRAME_R23]
	nop	15
	;;
}
{	.mmb
	ld8.fill r24=[r2],-16		// r2=&tf_r[FRAME_R22]
	ld8.fill r23=[r1],-16		// r1=&tf_r[FRAME_R21]
	nop	16
	;;
}
{	.mmi
	ld8.fill r22=[r2],-16		// r2=&tf_r[FRAME_R20]
	ld8.fill r21=[r1],-16		// r1=&tf_r[FRAME_R19]
	nop	17
	;;
}
{	.mmb
	ld8.fill r20=[r2],-16		// r2=&tf_r[FRAME_R18]
	ld8.fill r19=[r1],-16		// r1=&tf_r[FRAME_R17]
	nop	18
	;;
}
{	.mmi
	ld8.fill r18=[r2],-16		// r2=&tf_r[FRAME_R16]
	ld8.fill r17=[r1],-16		// r1=&tf_r[FRAME_R15]
	nop	19
	;;
}
{	.mfb
	ld8.fill r16=[r2],-16		// r2=&tf_r[FRAME_R14]
	nop	20
	bsw.0				// switch to bank 0
	;;
}
{	.mmi
	ld8.fill r15=[r1],-16		// r1=&tf_r[FRAME_R13]
	ld8.fill r14=[r2],-16		// r2=&tf_r[FRAME_R12]
	nop	21
	;;
}
	// Don't restore r13 if returning to kernel
{	.mmi
	.pred.rel.mutex p1,p2
(p2)	ld8.fill r13=[r1],-16		// r1=&tf_r[FRAME_R11]
	ld8.fill r12=[r2],-16		// r2=&tf_r[FRAME_R10]
(p1)	add	r1=-16,r1		// r1=&tf_r[FRAME_R11]
	;;
}
{	.mmb
	ld8.fill r11=[r1],-16		// r1=&tf_r[FRAME_R9]
	ld8.fill r10=[r2],-16		// r2=&tf_r[FRAME_R8]
	nop	22
	;;
}
{	.mmi
	ld8.fill r9=[r1],-16		// r1=&tf_r[FRAME_R7]
	ld8.fill r8=[r2],-16		// r2=&tf_r[FRAME_R6]
	nop	23
	;;
}
{	.mmb
	ld8.fill r7=[r1],-16		// r1=&tf_r[FRAME_R5]
	ld8.fill r6=[r2],-16		// r2=&tf_r[FRAME_R4]
	nop	24
	;;
}
{	.mmi
	ld8.fill r5=[r1],-16		// r1=&tf_r[FRAME_R3]
	ld8.fill r4=[r2],-16		// r2=&tf_r[FRAME_R2]
	nop	25
	;;
}
{	.mmb
	ld8.fill r3=[r1],-16		// r1=&tf_r[FRAME_R1]
	ld8.fill r23=[r2],-16		// r2=&tf_b[7]
	nop	26
	;;
}
{	.mmi
	ld8.fill r24=[r1],-16		// r1=&tf_b[6]
	ld8	r16=[r2],-16		// r16=b7, r2=&tf_b[5]
	nop	27
	;;
}
{	.mmi
	ld8	r17=[r1],-16		// r17=b6, r1=&tf_b[4]
	ld8	r18=[r2],-16		// r18=b5, r2=&tf_b[3]
	mov	b7=r16
	;;
}
{	.mmi
	ld8	r16=[r1],-16		// r16=b4, r1=&tf_b[2]
	ld8	r19=[r2],-16		// r19=b3, r2=&tf_b[1]
	mov	b6=r17
	;;
}
{	.mii
	ld8	r17=[r1],-16		// r17=b2, r1=&tf_b[0]
	mov	b5=r18
	mov	b4=r16
	;;
}
{	.mii
	ld8	r16=[r2],-16		// r16=b1, r2=&tf_ar_ec
	mov	b3=r19
	mov	b2=r17
	;;
}
{	.mmi
	ld8	r17=[r1],-16		// r17=b0, r1=&tf_ar_lc
	ld8	r18=[r2],-16		// r18=ar.ec, r2=&tf_ar_fptr
	mov	b1=r16
	;;
}
{	.mmi
	ld8	r16=[r1],-16		// r16=ar.lc, r1=&tf_ar_ccv
	ld8	r19=[r2],-16		// r19=ar.fpsr, r1=&tf_ar_unat
	mov	b0=r17
	;;
}
{	.mmi
	ld8	r17=[r1],-16		// r17=ar.ccv, r1=&tf_ndirty
	mov	ar.fpsr=r19
	mov	ar.ec=r18
	;;
}
{	.mmi
	ld8	r18=[r2],-16		// r18=ar.unat, r2=&tf_ar_rnat
	mov	ar.ccv=r17
	mov	ar.lc=r16
	;;
}
{	.mmb
	ld8	r27=[r1],-16	// r1=&tf_ar_bspstore
	ld8	r21=[r2],-16		// r2=&tf_cr_ifs
	nop	28
	;;
}
{	.mmi
	mov	ar.unat=r18
	ld8	r22=[r1],-16	// r1=&tf_ar_pfs
	nop	29
}
{	.mfb
	ld8	r25=[r2],-16		// r2=&tf_ar_rsc
	nop	30
(p1)	br.cond.dpnt.few 1f		// don't switch bs if kernel
	;;
}
{	.mmi
	alloc	r16=ar.pfs,0,0,0,0	// discard current frame
	;;
	nop	31
	shl	r16=r27,16		// value for ar.rsc
	;;
}
{	.mmi
	mov	ar.rsc=r16		// setup for loadrs
	;;
	loadrs				// load user regs from kernel bs
	nop	32
	;;
}
{	.mmi
	mov	ar.bspstore=r22
	;;
	mov	ar.rnat=r21
	nop	33
	;;
}
1:
{	.mmb
	ld8	r19=[r1],-16		// r1=&tf_pr
	ld8	r20=[r2],-16		// r2=&tf_cr_ifa
	nop	34
	;;
}
{	.mmi
	ld8	r27=[r1],-16		// r1=&tf_cr_isr
	ld8	r28=[r2],-16		// r2=&tf_cr_ipsr
	mov	ar.pfs=r19
	;;
}
{	.mmi
	ld8	r29=[r1],-16		// r1=&tf_cr_iip
	ld8	r30=[r2]
	mov	pr=r27,0x1ffff
	;;
}
{	.mmi
	ld8	r31=[r1]
	mov	cr.ifs=r25
	mov	r2=r23
	;;
}
{	.mmi
	mov	cr.ifa=r28
	mov	cr.iip=r31
	mov	r1=r24
	;;
}
{	.mmi
	mov	cr.ipsr=r30
	mov	ar.rsc=r20
	nop	35
	;;
}
{	.bbb
	nop	36
	nop	37
	rfi
	;;
}
END(exception_restore)
	

/*
 * exception_save: save interrupted state
 *
 * Arguments:
 *	b6	return address
 *	r16	saved b6
 *
 * Return:
 *	r14	cr.iim value for break traps
 *	sp	kernel stack pointer
 *	p1	true if user mode
 *	p2	true if kernel mode
 *	p3	true if interrupts were enabled
 */
ENTRY(exception_save, 0)
{	.mmi
	rsm	psr.dt			// turn off data translations
	;;
	srlz.d				// serialize
	mov	r27=pr
}
{	.mmi
	mov	r30=cr.ipsr
	;;
	mov	r31=cr.iip
	tbit.nz	p3,p0=r30,14		// check for interrupt enable state
}
{	.mmi
	mov	r29=cr.isr
	;; 
	mov	r26=sp			// save sp
	extr.u	r17=r30,32,2		// extract ipsr.cpl
	;;
}
{	.mmi
	cmp.eq	p1,p2=r0,r17		// test for kernel mode
	;;
(p2)	mov	sp=ar.k6		// and switch to kernel stack
	mov	r24=r1
	;;
}
{	.mii
	mov	r28=cr.ifa
	add	sp=-SIZEOF_TRAPFRAME,sp	// reserve trapframe
	;;
	dep	r1=0,sp,61,3		// r1=&tf_flags
	;;
}
{	.mmi
	st8	[r1]=r0,8		// zero flags, r1=&tf_cr_iip
	;;
	mov	r23=r2
	add	r2=8,r1			// r2=&tf_cr_ipsr
	;;
}
{	.mmb
	st8	[r1]=r31,16		// r1=&tf_cr_isr
	st8	[r2]=r30,16		// r2=&tf_cr_ifa
	nop	1
	;;
}
{	.mmb
	st8	[r1]=r29,16		// r1=&tf_pr
	st8	[r2]=r28,16		// r2=&tf_ar_rsc
	nop	2
	;;
}
{	.mmi
	st8	[r1]=r27,16		// r1=&tf_cr_pfs
	mov	r20=ar.rsc
	mov	r19=ar.pfs
	;;
}
{	.mmb
	st8	[r2]=r20,16		// r2=&tf_cr_ifs
	st8	[r1]=r19,16		// r1=&tf_ar_bspstore
	cover
	;;
}
{	.mmi
	mov	ar.rsc=0
	;;
	mov	r22=ar.bspstore
	;;
}
{	.mmi
	mov	r25=cr.ifs
	mov	r21=ar.rnat
(p1)	mov	r31=r22		// so we can figure out ndirty
	;;
}
{	.mmb
(p2)	mov	r31=ar.k5		// kernel backing store
	st8	[r2]=r25,16		// r2=&tf_ar_rnat
	nop	3
	;;
}
{	.mmi
	st8	[r1]=r22,16	// r1=&tf_ndirty
(p2)	mov	ar.bspstore=r31		// switch bspstore
	nop	4
	;;
}
{	.mmi
	mov	r17=ar.bsp
	;;
	st8	[r2]=r21,16		// r2=&tf_ar_unat
	sub	r17=r17,r31		// ndirty (in bytes)
	;;
}
{	.mmi
	st8	[r1]=r17,16		// r1=&tf_ar_ccv
	mov	ar.rsc=3		// switch RSE back on
	mov	r18=ar.lc
}
{	.mmi
	mov	r31=ar.unat
	;;
	st8	[r2]=r31,16		// r2=&tf_ar_fpsr
	mov	r19=ar.ec
}
{	.mmi
	mov	r17=ar.ccv
	;;
	st8	[r1]=r17,16		// r1=&tf_ar_lc
	nop	5
	;;
}
{	.mmi
	mov	r31=ar.fpsr
	;;
	st8	[r2]=r31,16		// r2=&tf_ar_ec
	mov	r30=b0
	;;
}
{	.mmi
	st8	[r1]=r18,16		// r1=&tf_b[0]
	;;
	st8	[r2]=r19,16		// r2=&tf_b[1]
	mov	r31=b1
}
{	.mmi
	st8	[r1]=r30,16		// r1=&tf_b[2]
	;;
	st8	[r2]=r31,16		// r2=&tf_b[3]
	mov	r31=b2
	;;
}
{	.mii
	st8	[r1]=r31,16		// r1=&tf_b[4]
	mov	r17=b3
	;;
	mov	r18=b4
}
{	.mmi
	st8	[r2]=r17,16		// r2=&tf_b[5]
	;; 
	st8	[r1]=r18,16		// r1=&tf_b[6]
	mov	r31=b5
	;;
}
{	.mii
	st8	[r2]=r31,16		// r2=&tf_b[7]
	mov	r18=b7
}
{	.mmi
	st8	[r1]=r16,16		// r1=&tf_r[FRAME_R1]
	;; 
	st8	[r2]=r18,16		// r2=&tf_r[FRAME_R2]
	nop	7
	;;
}
{	.mmb
	.mem.offset 0,0
	st8.spill [r1]=r24,16		// r1=&tf_r[FRAME_R3]
	.mem.offset 8,0
	st8.spill [r2]=r23,16		// r2=&tf_r[FRAME_R4]
	nop	8
	;;
}
{	.mmi
	.mem.offset 16,0
	st8.spill [r1]=r3,16		// r1=&tf_r[FRAME_R5]
	.mem.offset 32,0
	st8.spill [r2]=r4,16		// r2=&tf_r[FRAME_R6]
	nop	9
	;;
}
{	.mmb
	.mem.offset 48,0
	st8.spill [r1]=r5,16		// r1=&tf_r[FRAME_R7]
	.mem.offset 64,0
	st8.spill [r2]=r6,16		// r2=&tf_r[FRAME_R8]
	nop	10
	;;
}
{	.mmi
	.mem.offset 80,0
	st8.spill [r1]=r7,16		// r1=&tf_r[FRAME_R9]
	.mem.offset 96,0
	st8.spill [r2]=r8,16		// r2=&tf_r[FRAME_R10]
	nop	11
	;;
}
{	.mmb
	.mem.offset 112,0
	st8.spill [r1]=r9,16		// r1=&tf_r[FRAME_R11]
	.mem.offset 128,0
	st8.spill [r2]=r10,16		// r2=&tf_r[FRAME_SP]
	nop	12
	;;
}
{	.mmi
	.mem.offset 144,0
	st8.spill [r1]=r11,16		// r1=&tf_r[FRAME_R13]
	.mem.offset 160,0
	st8.spill [r2]=r26,16		// r2=&tf_r[FRAME_R14]
	nop	13
	;;
}
{	.mmb
	.mem.offset 176,0
	st8.spill [r1]=r13,16		// r1=&tf_r[FRAME_R15]
	.mem.offset 192,0
	st8.spill [r2]=r14,16		// r2=&tf_r[FRAME_R16]
	nop	14
	;;
}
{	.mfb
	.mem.offset 208,0
	st8.spill [r1]=r15,16		// r1=&tf_r[FRAME_R17]
	nop	15
	bsw.1				// switch to bank 1
	;;
}
{	.mmi
	.mem.offset 224,0
	st8.spill [r2]=r16,16		// r2=&tf_r[FRAME_R18]
	.mem.offset 240,0
	st8.spill [r1]=r17,16		// r1=&tf_r[FRAME_R19]
	nop	16
	;;
}
{	.mmb
	.mem.offset 256,0
	st8.spill [r2]=r18,16		// r2=&tf_r[FRAME_R20]
	.mem.offset 272,0
	st8.spill [r1]=r19,16		// r1=&tf_r[FRAME_R21]
	nop	17
	;;
}
{	.mmi
	.mem.offset 288,0
	st8.spill [r2]=r20,16		// r2=&tf_r[FRAME_R22]
	.mem.offset 304,0
	st8.spill [r1]=r21,16		// r1=&tf_r[FRAME_R23]
	nop	18
	;;
}
{	.mmb
	.mem.offset 320,0
	st8.spill [r2]=r22,16		// r2=&tf_r[FRAME_R24]
	.mem.offset 336,0
	st8.spill [r1]=r23,16		// r1=&tf_r[FRAME_R25]
	nop	19
	;;
}
{	.mmi
	.mem.offset 352,0
	st8.spill [r2]=r24,16		// r2=&tf_r[FRAME_R26]
	.mem.offset 368,0
	st8.spill [r1]=r25,16		// r1=&tf_r[FRAME_R27]
	nop	20
	;;
}
{	.mmb
	.mem.offset 384,0
	st8.spill [r2]=r26,16		// r2=&tf_r[FRAME_R28]
	.mem.offset 400,0
	st8.spill [r1]=r27,16		// r1=&tf_r[FRAME_R29]
	nop	21
	;;
}
{	.mmi
	.mem.offset 416,0
	st8.spill [r2]=r28,16		// r2=&tf_r[FRAME_R30]
	.mem.offset 432,0
	st8.spill [r1]=r29,16		// r1=&tf_r[FRAME_R31]
	nop	22
	;;
}
{	.mmb
	.mem.offset 448,0
	st8.spill [r2]=r30,16		// r2=&tf_f[FRAME_F6]
	.mem.offset 464,0
	st8.spill [r1]=r31,24		// r1=&tf_f[FRAME_F7]
	nop	23
	;;
}
{	.mmi
	stf.spill [r2]=f6,32		// r2=&tf_f[FRAME_F8]
	stf.spill [r1]=f7,32		// r1=&tf_f[FRAME_F9]
	nop	24
	;;
}
{	.mmb
	stf.spill [r2]=f8,32		// r2=&tf_f[FRAME_F10]
	stf.spill [r1]=f9,32		// r1=&tf_f[FRAME_F11]
	nop	25
	;;
}
{	.mmi
	stf.spill [r2]=f10,32		// r2=&tf_f[FRAME_F12]
	stf.spill [r1]=f11,32		// r1=&tf_f[FRAME_F13]
	nop	26
	;;
}
{	.mmb
	stf.spill [r2]=f12,32		// r2=&tf_f[FRAME_F14]
	stf.spill [r1]=f13,32		// r1=&tf_f[FRAME_F15]
	nop	27
	;;
}
{	.mmi
	stf.spill [r2]=f14		//
	stf.spill [r1]=f15		//
	nop	28
	;;
}
{	.mlx
	mov	r14=cr.iim		// break immediate
	movl	r1=__gp			// kernel globals
}
{	.mmi
	ssm	psr.ic|psr.dt		// enable interrupts & translation
	;;
	srlz.i				// serialize
	nop	29
	;;
}
{	.mfb
	mov	r13=ar.k4		// processor globals
	nop	30
	br.sptk.few b6			// not br.ret - we were not br.call'ed
	;;
}
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
	.unwabi	@svr4, 'I'
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
	st8	[r30]=r20,32		// save ar.ccv, skip to b0
	st8	[r31]=r21,32		// save ar.fpsr, skip to b1
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
} { .mmi
3:	rsm	psr.i			// we know that psr.i == 1
	add	r14=PC_CURTHREAD,r13	// &curthread
	nop.i	0
	;;
} { .mmi
	ld8	r14=[r14]		// curthread
	;;
	add	r14=TD_KSE,r14		// &curthread->td_kse
	nop.i	0
	;;
} { .mmi
	ld8	r14=[r14]		// curkse
	;;
	add	r14=KE_FLAGS,r14	// &curkse->ke_flags
	nop.i	0
	;;
} { .mmi
	ld4	r14=[r14]		// curkse->ke_flags
	;;
	nop.m	0
	tbit.nz	p6,p7=r14,10		// KEF_ASTPENDING
	;;
} { .mib
	nop.m	0
(p7)	tbit.nz.or.andcm p6,p7=r14,11	// KEF_NEEDRESCHED
(p7)	br.cond.dptk 2f
	;;
} { .mmi
	ssm	psr.i			// restore interrupts
	;;
	srlz.d
	mov	out0=loc0		// trapframe argument to ast()
} { .mib
	nop.m	0
	nop.i	0
	br.call.sptk.many rp=ast
} { .mib
	nop.m	0
	nop.i	0
	br	3b
} { .mii
2:	ld8	r14=[loc0]		// check tf_flags
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
	ld8	r16=[r30],-32		// restore b0, skip to ar.ccv
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
