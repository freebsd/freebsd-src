/*-
 * Copyright (c) 2003 Marcel Moolenaar
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
 * ar.k7 = kernel memory stack
 * ar.k6 = kernel register stack
 * ar.k5 = EPC gateway page
 * ar.k4 = PCPU data
 */

	.text

/*
 * exception_save: save interrupted state
 *
 * Arguments:
 *	r16	address of bundle that contains the branch. The
 *		return address will be the next bundle.
 *	r17	the value to save as ifa in the trapframe. This
 *		normally is cr.ifa, but some interruptions set
 *		set cr.iim and not cr.ifa.
 *
 * Returns:
 *	p15	interrupted from user stack
 *	p14	interrupted from kernel stack
 *	p13	interrupted from user backing store
 *	p12	interrupted from kernel backing store
 *	p11	interrupts were enabled
 *	p10	interrupts were disabled
 */
ENTRY(exception_save, 0)
{	.mii
	mov		r20=ar.unat
	extr.u		r31=sp,61,3
	mov		r18=pr
	;;
}
{	.mmi
	cmp.le		p14,p15=5,r31
	;;
(p15)	mov		r23=ar.k7		// kernel memory stack
(p14)	mov		r23=sp
	;;
}
{	.mii
	mov		r21=ar.rsc
	add		r30=-SIZEOF_TRAPFRAME,r23
	;;
	dep		r30=0,r30,0,10
	;;
}
{	.mmi
	mov		ar.rsc=0
	sub		r19=r23,r30
	add		r31=8,r30
	;;
}
{	.mlx
	mov		r22=cr.iip
	movl		r26=exception_save_restart
	;;
}

	/*
	 * We have a 1KB aligned trapframe, pointed to by sp. If we write
	 * to the trapframe, we may trigger a data nested TLB fault. By
	 * aligning the trapframe on a 1KB boundary, we guarantee that if
	 * we get a data nested TLB fault, it will be on the very first
	 * write. Since the data nested TLB fault does not preserve any
	 * state, we have to be careful what we clobber. Consequently, we
	 * have to be careful what we use here. Below a list of registers
	 * that are currently alive:
	 *	r16,r17=arguments
	 *	r18=pr, r19=length, r20=unat, r21=rsc, r22=iip, r23=TOS
	 *	r26=restart point
	 *	r30,r31=trapframe pointers
	 *	p14,p15=memory stack switch
	 */
exception_save_restart:
{	.mmi
	st8		[r30]=r19,16		// length
	st8		[r31]=r0,16		// flags
	add		r19=16,r19
	;;
}
{	.mmi
	st8.spill	[r30]=sp,16		// sp
	st8		[r31]=r20,16		// unat
	sub		sp=r23,r19
	;;
}
{	.mmi
	mov		r19=ar.rnat
	mov		r20=ar.bspstore
	mov		r23=rp
	;;
}
	// r18=pr, r19=rnat, r20=bspstore, r21=rsc, r22=iip, r23=rp
{	.mmi
	st8		[r30]=r23,16		// rp
	st8		[r31]=r18,16		// pr
	mov		r24=ar.pfs
	;;
}
{	.mmb
	st8		[r30]=r24,16		// pfs
	st8		[r31]=r20,16		// bspstore
	cover
	;;
}
{	.mmi
	mov		r18=ar.fpsr
	mov		r23=cr.ipsr
	extr.u		r24=r20,61,3
	;;
}
	// r18=fpsr, r19=rnat, r20=bspstore, r21=rsc, r22=iip, r23=ipsr
{	.mmi
	st8		[r30]=r19,16		// rnat
	st8		[r31]=r0,16		// __spare
	cmp.le		p12,p13=5,r24
	;;
}
{	.mmi
	st8.spill	[r30]=r13,16		// tp
	st8		[r31]=r21,16		// rsc
	tbit.nz		p11,p10=r23,14		// p11=interrupts enabled
	;;
}
{	.mmi
	st8		[r30]=r18,16		// fpsr
(p13)	mov		r20=ar.k6		// kernel register stack
	nop		0
	;;
}
	// r20=bspstore, r22=iip, r23=ipsr
{	.mmi
	st8		[r31]=r23,16		// psr
	mov		ar.bspstore=r20
	nop		0
	;;
}
{	.mmi
	mov		r18=ar.bsp
	;;
	mov		r19=cr.ifs
	sub		r18=r18,r20
	;;
}
{	.mmi
	st8.spill	[r30]=gp,16		// gp
	st8		[r31]=r18,16		// ndirty
	nop		0
	;;
}
	// r19=ifs, r22=iip
{	.mmi
	st8		[r30]=r19,16		// cfm
	st8		[r31]=r22,16		// iip
	nop		0
	;;
}
{	.mmi
	st8		[r30]=r17		// ifa
	mov		r18=cr.isr
	add		r29=16,r30
	;;
}
{	.mmi
	st8		[r31]=r18		// isr
	add		r30=8,r29
	add		r31=16,r29
	;;
}
{	.mmi
	.mem.offset	0,0
	st8.spill	[r30]=r2,16		// r2
	.mem.offset	8,0
	st8.spill	[r31]=r3,16		// r3
	add		r2=9*8,r29
	;;
}
{	.mmi
	.mem.offset	0,0
	st8.spill	[r30]=r8,16		// r8
	.mem.offset	8,0
	st8.spill	[r31]=r9,16		// r9
	add		r3=8,r2
	;;
}
{	.mmi
	.mem.offset	0,0
	st8.spill	[r30]=r10,16		// r10
	.mem.offset	8,0
	st8.spill	[r31]=r11,16		// r11
	add		r8=16,r16
	;;
}
{	.mmi
	.mem.offset	0,0
	st8.spill	[r30]=r14		// r14
	.mem.offset	8,0
	st8.spill	[r31]=r15		// r15
	mov		r9=r29
}
{	.mmb
	mov		r10=ar.csd
	mov		r11=ar.ssd
	bsw.1
	;;
}
{	.mmi
	.mem.offset	0,0
	st8.spill	[r2]=r16,16		// r16
	.mem.offset	8,0
	st8.spill	[r3]=r17,16		// r17
	mov		r14=b6
	;;
}
{	.mmi
	.mem.offset	0,0
	st8.spill	[r2]=r18,16		// r18
	.mem.offset	8,0
	st8.spill	[r3]=r19,16		// r19
	mov		r15=b7
	;;
}
{	.mmi
	.mem.offset	0,0
	st8.spill	[r2]=r20,16		// r20
	.mem.offset	8,0
	st8.spill	[r3]=r21,16		// r21
	mov		b7=r8
	;;
}
{	.mmi
	.mem.offset	0,0
	st8.spill	[r2]=r22,16		// r22
	.mem.offset	8,0
	st8.spill	[r3]=r23,16		// r23
	;;
}

	.mem.offset	0,0
	st8.spill	[r2]=r24,16		// r24
	.mem.offset	8,0
	st8.spill	[r3]=r25,16		// r25
	;;
	.mem.offset	0,0
	st8.spill	[r2]=r26,16		// r26
	.mem.offset	8,0
	st8.spill	[r3]=r27,16		// r27
	;;
	.mem.offset	0,0
	st8.spill	[r2]=r28,16		// r28
	.mem.offset	8,0
	st8.spill	[r3]=r29,16		// r29
	;;
	.mem.offset	0,0
	st8.spill	[r2]=r30,16		// r30
	.mem.offset	8,0
	st8.spill	[r3]=r31,16		// r31
	;;

{	.mmi
	st8		[r2]=r14,16		// b6
	mov		r17=ar.unat
	nop		0
	;;
}
{	.mmi
	st8		[r3]=r15,16		// b7
	mov		r16=ar.ccv
	nop		0
	;;
}
{	.mmi
	st8		[r2]=r16,16		// ccv
	st8		[r3]=r10,16		// csd
	nop		0
	;;
}
{	.mmi
	st8		[r2]=r11,24		// ssd
	st8		[r9]=r17
	nop		0
	;;
}

	stf.spill	[r3]=f6,32		// f6
	stf.spill	[r2]=f7,32		// f7
	;;
	stf.spill	[r3]=f8,32		// f8
	stf.spill	[r2]=f9,32		// f9
	;;
	stf.spill	[r3]=f10,32		// f10
	stf.spill	[r2]=f11,32		// f11
	;;
	stf.spill	[r3]=f12,32		// f12
	stf.spill	[r2]=f13,32		// f13
	;;
	stf.spill	[r3]=f14		// f14
	stf.spill	[r2]=f15		// f15
	;;
{	.mmi
	mov		ar.rsc=3
	mov		r13=ar.k4
	nop		0
	;;
}
{	.mlx
	ssm		psr.ic|psr.dfh
	movl		gp=__gp
	;;
}
{	.mfb
	srlz.d
	nop		0
	br.sptk		b7
	;;
}
END(exception_save)

/*
 * exception_restore:	restore interrupted state
 *
 * Arguments:
 *	sp+16	trapframe pointer
 */
ENTRY(exception_restore, 0)
{	.mmi
	rsm		psr.i
	add		sp=16,sp
	nop		0
	;;
}
{	.mmi
	add		r3=SIZEOF_TRAPFRAME-32,sp
	add		r2=SIZEOF_TRAPFRAME-16,sp
	add		r8=SIZEOF_SPECIAL+16,sp
	;;
}
	// The next load can trap. Let it be...
	ldf.fill	f15=[r2],-32		// f15
	ldf.fill	f14=[r3],-32		// f14
	;;
	ldf.fill	f13=[r2],-32		// f13
	ldf.fill	f12=[r3],-32		// f12
	;;
	ldf.fill	f11=[r2],-32		// f11
	ldf.fill	f10=[r3],-32		// f10
	;;
	ldf.fill	f9=[r2],-32		// f9
	ldf.fill	f8=[r3],-32		// f8
	;;
	ldf.fill	f7=[r2],-24		// f7
	ldf.fill	f6=[r3],-16		// f6
	;;

{	.mmi
	ld8		r8=[r8]			// unat (after)
	;;
	mov		ar.unat=r8
	nop		0
	;;
}

	ld8		r10=[r2],-16		// ssd
	ld8		r11=[r3],-16		// csd
	;;
	mov		ar.ssd=r10
	mov		ar.csd=r11

	ld8		r14=[r2],-16		// ccv
	ld8		r15=[r3],-16		// b7
	;;

{	.mmi
	mov		ar.ccv=r14
	ld8		r8=[r2],-16		// b6
	mov		b7=r15
	;;
}
{	.mmi
	ld8.fill	r31=[r3],-16		// r31
	ld8.fill	r30=[r2],-16		// r30
	mov		b6=r8
	;;
}

	ld8.fill	r29=[r3],-16		// r29
	ld8.fill	r28=[r2],-16		// r28
	;;
	ld8.fill	r27=[r3],-16		// r27
	ld8.fill	r26=[r2],-16		// r26
	;;
	ld8.fill	r25=[r3],-16		// r25
	ld8.fill	r24=[r2],-16		// r24
	;;
	ld8.fill	r23=[r3],-16		// r23
	ld8.fill	r22=[r2],-16		// r22
	;;
	ld8.fill	r21=[r3],-16		// r21
	ld8.fill	r20=[r2],-16		// r20
	;;
	ld8.fill	r19=[r3],-16		// r19
	ld8.fill	r18=[r2],-16		// r18
	;;

{	.mmb
	ld8.fill	r17=[r3],-16		// r17
	ld8.fill	r16=[r2],-16		// r16
	bsw.0
	;;
}
{	.mmi
	ld8.fill	r15=[r3],-16		// r15
	ld8.fill	r14=[r2],-16		// r14
	add		r31=16,sp
	;;
}
{	.mmi
	ld8		r16=[sp]		// tf_length
	ld8.fill	r11=[r3],-16		// r11
	add		r30=24,sp
	;;
}
{	.mmi
	ld8.fill	r10=[r2],-16		// r10
	ld8.fill	r9=[r3],-16		// r9
	add		r16=r16,sp		// ar.k7
	;;
}
{	.mmi
	ld8.fill	r8=[r2],-16		// r8
	ld8.fill	r3=[r3]			// r3
	;;
}
	// We want nested TLB faults from here on...
	rsm		psr.ic|psr.i
	ld8.fill	r2=[r2]			// r2
	nop		0
	;;
	srlz.d
	ld8.fill	sp=[r31],16		// sp
	nop		0
	;;

	ld8		r17=[r30],16		// unat
	ld8		r29=[r31],16		// rp
	;;
	ld8		r18=[r30],16		// pr
	ld8		r28=[r31],16		// pfs
	mov		rp=r29
	;;
	ld8		r20=[r30],24		// bspstore
	ld8		r21=[r31],24		// rnat
	mov		ar.pfs=r28
	;;
	ld8.fill	r29=[r30],16		// tp
	ld8		r22=[r31],16		// rsc
	;;
{	.mmi
	ld8		r23=[r30],16		// fpsr
	ld8		r24=[r31],16		// psr
	extr.u		r28=r20,61,3
	;;
}
{	.mmi
	ld8.fill	r1=[r30],16		// gp
	ld8		r25=[r31],16		// ndirty
	cmp.le		p14,p15=5,r28
	;;
}
{	.mmb
	ld8		r26=[r30]		// cfm
	ld8		r19=[r31]		// ip
(p14)	br.cond.sptk	1f
	;;
}
{	.mib
	// Switch register stack
	alloc		r30=ar.pfs,0,0,0,0	// discard current frame
	shl		r31=r25,16		// value for ar.rsc
	nop		0
	;;
}
	// The loadrs can fault if the backing store is not currently
	// mapped. We assured forward progress by getting everything we
	// need from the trapframe so that we don't care if the CPU
	// purges that translation when it needs to insert a new one for
	// the backing store.
{	.mmi
	mov		ar.rsc=r31		// setup for loadrs
	mov		ar.k7=r16
	mov		r13=r29
	;;
}
exception_restore_restart:
{	.mmi
	mov		r30=ar.bspstore
	;;
	loadrs					// load user regs
	nop		0
	;;
}
{	.mmi
	mov		r31=ar.bspstore
	;;
	mov		ar.bspstore=r20
	dep		r31=0,r31,0,9
	;;
}
{	.mmb
	mov		ar.k6=r31
	mov		ar.rnat=r21
	nop		0
	;;
}
1:
{	.mmb
	mov		ar.unat=r17
	mov		cr.iip=r19
	nop		0
}
{	.mmi
	mov		cr.ipsr=r24
	mov		cr.ifs=r26
	mov		pr=r18,0x1fffe
	;;
}
{	.mmb
	mov		ar.rsc=r22
	mov		ar.fpsr=r23
	rfi
	;;
}
END(exception_restore)

/*
 * Call exception_save_regs to preserve the interrupted state in a
 * trapframe. Note that we don't use a call instruction because we
 * must be careful not to lose track of the RSE state. We then call
 * trap() with the value of _n_ as an argument to handle the
 * exception. We arrange for trap() to return to exception_restore
 * which will restore the interrupted state before executing an rfi to
 * resume it.
 */
#define TRAP(_n_, _ifa_)			\
{	.mib ;					\
	mov		r17=_ifa_ ;		\
	mov		r16=ip ;		\
	br.sptk		exception_save ;	\
} ;						\
{	.mmi ;					\
(p11)	ssm		psr.i ;;		\
	alloc		r15=ar.pfs,0,0,2,0 ;	\
	mov		out0=_n_ ;;		\
} ;						\
{	.mfb ;					\
	add		out1=16,sp ;		\
	nop		0 ;			\
	br.call.sptk	rp=trap ;		\
} ;						\
{	.mfb ;					\
	nop		0 ;			\
	nop		0 ;			\
	br.sptk		exception_restore ;	\
}

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
	TRAP(0, cr.ifa)
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
	cmp.ne	p15,p0=r21,r19
(p15)	br.dpnt.few 1f
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
2:	cmp.eq	p15,p0=r0,r20		// done?
(p15)	br.cond.spnt.few 9f		// bail if done
	;;
	add	r21=16,r20		// tag location
	;;
	ld8	r21=[r21]		// read tag
	;;
	cmp.ne	p15,p0=r21,r19		// compare tags
(p15)	br.cond.sptk.few 3f		// if not, read next in chain
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
	TRAP(20, cr.ifa)		// Page Not Present trap
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
	cmp.ne	p15,p0=r21,r19
(p15)	br.dpnt.few 1f
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
2:	cmp.eq	p15,p0=r0,r20		// done?
(p15)	br.cond.spnt.few 9f		// bail if done
	;;
	add	r21=16,r20		// tag location
	;;
	ld8	r21=[r21]		// read tag
	;;
	cmp.ne	p15,p0=r21,r19		// compare tags
(p15)	br.cond.sptk.few 3f		// if not, read next in chain
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
	TRAP(20, cr.ifa)		// Page Not Present trap
IVT_END(Data_TLB)

IVT_ENTRY(Alternate_Instruction_TLB, 0x0c00)
	mov	r16=cr.ifa		// where did it happen
	mov	r18=pr			// save predicates
	;;
	extr.u	r17=r16,61,3		// get region number
	;;
	cmp.ge	p13,p0=5,r17		// RR0-RR5?
	cmp.eq	p15,p14=7,r17		// RR7->p15, RR6->p14
(p13)	br.spnt	9f
	;;
(p15)	movl	r17=PTE_P+PTE_MA_WB+PTE_A+PTE_D+PTE_PL_KERN+PTE_AR_RX
(p14)	movl	r17=PTE_P+PTE_MA_UC+PTE_A+PTE_D+PTE_PL_KERN+PTE_AR_RX
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
	TRAP(3, cr.ifa)
IVT_END(Alternate_Instruction_TLB)

IVT_ENTRY(Alternate_Data_TLB, 0x1000)
	mov	r16=cr.ifa		// where did it happen
	mov	r18=pr			// save predicates
	;;
	extr.u	r17=r16,61,3		// get region number
	;;
	cmp.ge	p13,p0=5,r17		// RR0-RR5?
	cmp.eq	p15,p14=7,r17		// RR7->p15, RR6->p14
(p13)	br.spnt	9f
	;;
(p15)	movl	r17=PTE_P+PTE_MA_WB+PTE_A+PTE_D+PTE_PL_KERN+PTE_AR_RW
(p14)	movl	r17=PTE_P+PTE_MA_UC+PTE_A+PTE_D+PTE_PL_KERN+PTE_AR_RW
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
	TRAP(4, cr.ifa)
IVT_END(Alternate_Data_TLB)

IVT_ENTRY(Data_Nested_TLB, 0x1400)
	// See exception_save_restart and exception_restore_restart for the
	// contexts that may cause a data nested TLB. We can only use the
	// banked general registers and predicates, but don't use:
	//	p14 & p15	-	Set in exception save
	//	r16 & r17	-	Arguments to exception save
	//	r30		-	Faulting address (modulo page size)
	// We assume r30 has the virtual addresses that relate to the data
	// nested TLB fault. The address does not have to be exact, as long
	// as it's in the same page. We use physical addressing to avoid
	// double nested faults. Since all virtual addresses we encounter
	// here are direct mapped region 7 addresses, we have no problem
	// constructing physical addresses.
{	.mlx
	rsm		psr.dt
	movl		r27=ia64_kptdir
	;;
}
{	.mii
	srlz.d
	dep		r27=0,r27,61,3
	extr.u		r28=r30,PAGE_SHIFT,61-PAGE_SHIFT
	;;
}
{	.mii
	ld8		r27=[r27]
	shr.u		r29=r28,PAGE_SHIFT-5	// dir index
	extr.u		r28=r28,0,PAGE_SHIFT-5	// pte index
	;;
}
{	.mmi
	shladd		r27=r29,3,r27
	;;
	mov		cr.ifa=r30
	dep		r27=0,r27,61,3
	;;
}
{	.mmi
	ld8		r27=[r27]
	mov		r29=rr[r30]
	shl		r28=r28,5
	;;
}
{	.mii
	add		r27=r27,r28		// address of pte
	dep		r29=0,r29,0,2
	;;
	dep		r27=0,r27,61,3
	;;
}
{	.mmi
	ld8		r28=[r27]
	;;
	mov		cr.itir=r29
	or		r28=PTE_D|PTE_A,r28
	;;
}
{	.mlx
	st8		[r27]=r28
	movl		r29=exception_save_restart
	;;
}
{	.mmi
	itc.d		r28
	;;
	ssm		psr.dt
	cmp.eq		p12,p13=r26,r29
	;;
}
{	.mbb
	srlz.d
(p12)	br.sptk		exception_save_restart
(p13)	br.sptk		exception_restore_restart
	;;
}
IVT_END(Data_Nested_TLB)

IVT_ENTRY(Instruction_Key_Miss, 0x1800)
	TRAP(6, cr.ifa)
IVT_END(Instruction_Key_Miss)

IVT_ENTRY(Data_Key_Miss, 0x1c00)
	TRAP(7, cr.ifa)
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
1:	cmp.eq	p15,p0=r0,r20		// done?
(p15)	br.cond.spnt.few 9f		// bail if done
	;;
	add	r21=16,r20		// tag location
	;;
	ld8	r21=[r21]		// read tag
	;;
	cmp.ne	p15,p0=r21,r19		// compare tags
(p15)	br.cond.sptk.few 2f		// if not, read next in chain
	;;
	ld8	r21=[r20]		// read pte
	mov	r22=PTE_D|PTE_A
	;;
	or	r21=r22,r21		// set dirty & access bit
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
	TRAP(8, cr.ifa)			// die horribly
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
1:	cmp.eq	p15,p0=r0,r20		// done?
(p15)	br.cond.spnt.few 9f		// bail if done
	;;
	add	r21=16,r20		// tag location
	;;
	ld8	r21=[r21]		// read tag
	;;
	cmp.ne	p15,p0=r21,r19		// compare tags
(p15)	br.cond.sptk.few 2f		// if not, read next in chain
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
	TRAP(9, cr.ifa)
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
1:	cmp.eq	p15,p0=r0,r20		// done?
(p15)	br.cond.spnt.few 9f		// bail if done
	;;
	add	r21=16,r20		// tag location
	;;
	ld8	r21=[r21]		// read tag
	;;
	cmp.ne	p15,p0=r21,r19		// compare tags
(p15)	br.cond.sptk.few 2f		// if not, read next in chain
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
	TRAP(10, cr.ifa)
IVT_END(Data_Access_Bit)

IVT_ENTRY(Break_Instruction, 0x2c00)
{	.mib
	mov		r17=cr.iim
	mov		r16=ip
	br.sptk		exception_save
	;;
}
{	.mmi
	alloc		r15=ar.pfs,0,0,2,0
	flushrs
	mov		out0=11
	;;
}
{	.mib
(p11)	ssm		psr.i
	add		out1=16,sp
	br.call.sptk	rp=trap
	;;
}
{	.mfb
	nop		0
	nop		0
	br.sptk		exception_restore
	;;
}
IVT_END(Break_Instruction)

IVT_ENTRY(External_Interrupt, 0x3000)
{	.mib
	mov		r17=cr.lid	// cr.iim and cr.ifa are undefined.
	mov		r16=ip
	br.sptk		exception_save
	;;
}
{	.mmb
2:	alloc	r14=ar.pfs,0,0,2,0	// make a frame for calling with
	add	out1=16,sp
	nop	0
}

3:	mov	out0=cr.ivr		// find interrupt vector
	;;
	cmp.eq	p15,p0=15,out0		// check for spurious vector number
(p15)	br.dpnt.few exception_restore	// if spurious, we are done
	;;
	ssm	psr.i			// re-enable interrupts
	br.call.sptk.many rp=interrupt	// call high-level handler
	;;
	rsm	psr.i			// disable interrupts
	;;
	srlz.d
	mov	cr.eoi=r0		// and ack the interrupt
	;;
	srlz.d
	br.sptk.few 3b			// loop for more
	;;
IVT_END(External_Interrupt)

IVT_ENTRY(Reserved_3400, 0x3400)
	TRAP(13, cr.ifa)
IVT_END(Reserved_3400)

IVT_ENTRY(Reserved_3800, 0x3800)
	TRAP(14, cr.ifa)
IVT_END(Reserved_3800)

IVT_ENTRY(Reserved_3c00, 0x3c00)
	TRAP(15, cr.ifa)
IVT_END(Reserved_3c00)

IVT_ENTRY(Reserved_4000, 0x4000)
	TRAP(16, cr.ifa)
IVT_END(Reserved_4000)

IVT_ENTRY(Reserved_4400, 0x4400)
	TRAP(17, cr.ifa)
IVT_END(Reserved_4400)

IVT_ENTRY(Reserved_4800, 0x4800)
	TRAP(18, cr.ifa)
IVT_END(Reserved_4800)

IVT_ENTRY(Reserved_4c00, 0x4c00)
	TRAP(19, cr.ifa)
IVT_END(Reserved_4c00)

IVT_ENTRY(Page_Not_Present, 0x5000)
	TRAP(20, cr.ifa)
IVT_END(Page_Not_Present)

IVT_ENTRY(Key_Permission, 0x5100)
	TRAP(21, cr.ifa)
IVT_END(Key_Permission)

IVT_ENTRY(Instruction_Access_Rights, 0x5200)
	TRAP(22, cr.ifa)
IVT_END(Instruction_Access_Rights)

IVT_ENTRY(Data_Access_Rights, 0x5300)
	TRAP(23, cr.ifa)
IVT_END(Data_Access_Rights)

IVT_ENTRY(General_Exception, 0x5400)
	TRAP(24, cr.ifa)
IVT_END(General_Exception)

IVT_ENTRY(Disabled_FP_Register, 0x5500)
	TRAP(25, cr.ifa)
IVT_END(Disabled_FP_Register)

IVT_ENTRY(NaT_Consumption, 0x5600)
	TRAP(26, cr.ifa)
IVT_END(NaT_Consumption)

IVT_ENTRY(Speculation, 0x5700)
	TRAP(27, cr.iim)
IVT_END(Speculation)

IVT_ENTRY(Reserved_5800, 0x5800)
	TRAP(28, cr.ifa)
IVT_END(Reserved_5800)

IVT_ENTRY(Debug, 0x5900)
	TRAP(29, cr.ifa)
IVT_END(Debug)

IVT_ENTRY(Unaligned_Reference, 0x5a00)
	TRAP(30, cr.ifa)
IVT_END(Unaligned_Reference)

IVT_ENTRY(Unsupported_Data_Reference, 0x5b00)
	TRAP(31, cr.ifa)
IVT_END(Unsupported_Data_Reference)

IVT_ENTRY(Floating_Point_Fault, 0x5c00)
	TRAP(32, cr.ifa)
IVT_END(Floating_Point_Fault)

IVT_ENTRY(Floating_Point_Trap, 0x5d00)
	TRAP(33, cr.ifa)
IVT_END(Floating_Point_Trap)

IVT_ENTRY(Lower_Privilege_Transfer_Trap, 0x5e00)
	TRAP(34, cr.ifa)
IVT_END(Lower_Privilege_Transfer_Trap)

IVT_ENTRY(Taken_Branch_Trap, 0x5f00)
	TRAP(35, cr.ifa)
IVT_END(Taken_Branch_Trap)

IVT_ENTRY(Single_Step_Trap, 0x6000)
	TRAP(36, cr.ifa)
IVT_END(Single_Step_Trap)

IVT_ENTRY(Reserved_6100, 0x6100)
	TRAP(37, cr.ifa)
IVT_END(Reserved_6100)

IVT_ENTRY(Reserved_6200, 0x6200)
	TRAP(38, cr.ifa)
IVT_END(Reserved_6200)

IVT_ENTRY(Reserved_6300, 0x6300)
	TRAP(39, cr.ifa)
IVT_END(Reserved_6300)

IVT_ENTRY(Reserved_6400, 0x6400)
	TRAP(40, cr.ifa)
IVT_END(Reserved_6400)

IVT_ENTRY(Reserved_6500, 0x6500)
	TRAP(41, cr.ifa)
IVT_END(Reserved_6500)

IVT_ENTRY(Reserved_6600, 0x6600)
	TRAP(42, cr.ifa)
IVT_END(Reserved_6600)

IVT_ENTRY(Reserved_6700, 0x6700)
	TRAP(43, cr.ifa)
IVT_END(Reserved_6700)

IVT_ENTRY(Reserved_6800, 0x6800)
	TRAP(44, cr.ifa)
IVT_END(Reserved_6800)

IVT_ENTRY(IA_32_Exception, 0x6900)
	TRAP(45, cr.ifa)
IVT_END(IA_32_Exception)

IVT_ENTRY(IA_32_Intercept, 0x6a00)
	TRAP(46, cr.iim)
IVT_END(IA_32_Intercept)

IVT_ENTRY(IA_32_Interrupt, 0x6b00)
	TRAP(47, cr.ifa)
IVT_END(IA_32_Interrupt)

IVT_ENTRY(Reserved_6c00, 0x6c00)
	TRAP(48, cr.ifa)
IVT_END(Reserved_6c00)

IVT_ENTRY(Reserved_6d00, 0x6d00)
	TRAP(49, cr.ifa)
IVT_END(Reserved_6d00)

IVT_ENTRY(Reserved_6e00, 0x6e00)
	TRAP(50, cr.ifa)
IVT_END(Reserved_6e00)

IVT_ENTRY(Reserved_6f00, 0x6f00)
	TRAP(51, cr.ifa)
IVT_END(Reserved_6f00)

IVT_ENTRY(Reserved_7000, 0x7000)
	TRAP(52, cr.ifa)
IVT_END(Reserved_7000)

IVT_ENTRY(Reserved_7100, 0x7100)
	TRAP(53, cr.ifa)
IVT_END(Reserved_7100)

IVT_ENTRY(Reserved_7200, 0x7200)
	TRAP(54, cr.ifa)
IVT_END(Reserved_7200)

IVT_ENTRY(Reserved_7300, 0x7300)
	TRAP(55, cr.ifa)
IVT_END(Reserved_7300)

IVT_ENTRY(Reserved_7400, 0x7400)
	TRAP(56, cr.ifa)
IVT_END(Reserved_7400)

IVT_ENTRY(Reserved_7500, 0x7500)
	TRAP(57, cr.ifa)
IVT_END(Reserved_7500)

IVT_ENTRY(Reserved_7600, 0x7600)
	TRAP(58, cr.ifa)
IVT_END(Reserved_7600)

IVT_ENTRY(Reserved_7700, 0x7700)
	TRAP(59, cr.ifa)
IVT_END(Reserved_7700)

IVT_ENTRY(Reserved_7800, 0x7800)
	TRAP(60, cr.ifa)
IVT_END(Reserved_7800)

IVT_ENTRY(Reserved_7900, 0x7900)
	TRAP(61, cr.ifa)
IVT_END(Reserved_7900)

IVT_ENTRY(Reserved_7a00, 0x7a00)
	TRAP(62, cr.ifa)
IVT_END(Reserved_7a00)

IVT_ENTRY(Reserved_7b00, 0x7b00)
	TRAP(63, cr.ifa)
IVT_END(Reserved_7b00)

IVT_ENTRY(Reserved_7c00, 0x7c00)
	TRAP(64, cr.ifa)
IVT_END(Reserved_7c00)

IVT_ENTRY(Reserved_7d00, 0x7d00)
	TRAP(65, cr.ifa)
IVT_END(Reserved_7d00)

IVT_ENTRY(Reserved_7e00, 0x7e00)
	TRAP(66, cr.ifa)
IVT_END(Reserved_7e00)

IVT_ENTRY(Reserved_7f00, 0x7f00)
	TRAP(67, cr.ifa)
IVT_END(Reserved_7f00)
