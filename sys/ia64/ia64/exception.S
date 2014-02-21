/*-
 * Copyright (c) 2003,2004 Marcel Moolenaar
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
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

#include "opt_xtrace.h"

#include <machine/pte.h>
#include <assym.s>

/*
 * Nested TLB restart tokens. These are used by the
 * nested TLB handler for jumping back to the code
 * where the nested TLB was caused.
 */
#define	NTLBRT_SAVE	0x12c12c
#define	NTLBRT_RESTORE  0x12c12d

/*
 * ar.k7 = kernel memory stack
 * ar.k6 = kernel register stack
 * ar.k5 = EPC gateway page
 * ar.k4 = PCPU data
 */

	.section .ivt.data, "aw"

	.global ia64_kptdir
ia64_kptdir:	data8	0

#ifdef EXCEPTION_TRACING

	.global xtrace, xhead
xtrace:	.space	1024*5*8
xhead:	data8	xtrace

#define	XTRACE(offset)				\
{	.mmi ;					\
	mov	r24=ar.itc ;			\
	mov	r25=cr.iip ;			\
	mov	r27=offset ;			\
} ;						\
{	.mlx ;					\
	mov	r28=cr.ifa ;			\
	movl	r29=xhead ;;			\
} ;						\
{	.mmi ;					\
	ld8	r29=[r29] ;;			\
	st8	[r29]=r24,8 ;			\
	nop	0 ;;				\
} ;						\
{	.mmi ;					\
	st8	[r29]=r27,8 ;;			\
	mov	r24=cr.isr ;			\
	add	r27=8,r29 ;;			\
} ;						\
{	.mmi ;					\
	st8	[r29]=r25,16 ;;			\
	st8	[r27]=r28,16 ;			\
	mov	r25=pr ;;			\
} ;						\
{	.mlx ;					\
	st8	[r29]=r24 ;			\
	movl	r28=xhead ;;			\
} ;						\
{	.mii ;					\
	cmp.eq	p15,p0=r27,r28 ;		\
	addl	r29=1024*5*8,r0 ;;		\
(p15)	sub	r27=r28,r29 ;;			\
} ;						\
{	.mmi ;					\
	st8	[r28]=r27 ;			\
	nop	0 ;				\
	mov	pr=r25,0x1ffff ;;		\
}

#else

#define	XTRACE(offset)

#endif

	.section .ivt.text, "ax"

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
ENTRY_NOPROFILE(exception_save, 0)
{	.mii
	mov		r20=ar.unat
	extr.u		r31=sp,61,3
	mov		r18=pr
	;;
}
{	.mmi
	cmp.le		p14,p15=IA64_VM_MINKERN_REGION,r31
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
	mov		r22=cr.iip
	addl		r29=NTLBRT_SAVE,r0	// 22-bit restart token.
	;;
}

	/*
	 * We have a 1KB aligned trapframe, pointed to by r30. We can't
	 * reliably write to the trapframe using virtual addressing, due
	 * to the fact that TC entries we depend on can be removed by:
	 * 1.  ptc.g instructions issued by other threads/cores/CPUs, or
	 * 2.  TC modifications in another thread on the same core.
	 * When our TC entry gets removed, we get nested TLB faults and
	 * since no state is saved, we can only deal with those when
	 * explicitly coded and expected.
	 * As such, we switch to physical addressing and account for the
	 * fact that the tpa instruction can cause a nested TLB fault.
	 * Since the data nested TLB fault does not preserve any state,
	 * we have to be careful what we clobber. Consequently, we have
	 * to be careful what we use here. Below a list of registers that
	 * are considered alive:
	 *	r16,r17=arguments
	 *	r18=pr, r19=length, r20=unat, r21=rsc, r22=iip, r23=TOS
	 *	r29=restart token
	 *	r30=trapframe pointers
	 *	p14,p15=memory stack switch
	 */
exception_save_restart:
	tpa		r24=r30			// Nested TLB fault possible
	sub		r19=r23,r30
	nop		0
	;;

	rsm		psr.dt
	add		r29=16,r19		// Clobber restart token
	mov		r30=r24
	;;
	srlz.d
	add		r31=8,r24
	;;

	// r18=pr, r19=length, r20=unat, r21=rsc, r22=iip, r23=TOS
	// r29=delta
{	.mmi
	st8		[r30]=r19,16		// length
	st8		[r31]=r0,16		// flags
	;;
}
{	.mmi
	st8.spill	[r30]=sp,16		// sp
	st8		[r31]=r20,16		// unat
	sub		sp=r23,r29
	;;
}
{	.mmi
	mov		r19=ar.rnat
	mov		r20=ar.bspstore
	mov		r23=rp
	;;
}
	// r18=pr, r19=rnat, r20=bspstore, r21=rsc, r22=iip, r23=rp
	// r24=pfs
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
	cmp.le		p12,p13=IA64_VM_MINKERN_REGION,r24
	;;
}
{	.mmi
	st8.spill	[r30]=r13,16		// tp
	st8		[r31]=r21,16		// rsc
	tbit.nz		p11,p10=r23,14		// p11=interrupts enabled
	;;
}
{	.mmi
(p13)	mov		r21=ar.k6		// kernel register stack
	;;
	st8		[r30]=r18,16		// fpsr
(p13)	dep		r20=r20,r21,0,9		// align dirty registers
	;;
}
	// r19=rnat, r20=bspstore, r22=iip, r23=ipsr
{	.mmi
	st8		[r31]=r23,16		// psr
(p13)	mov		ar.bspstore=r20
	nop		0
	;;
}
{	.mmi
(p13)	mov		ar.rnat=r19
	mov		r18=ar.bsp
	nop		0
	;;
}
{	.mmi
	mov		r19=cr.ifs
	st8.spill	[r30]=gp,16		// gp
	sub		r18=r18,r20
	;;
}
	// r18=ndirty, r19=ifs, r22=iip
{	.mmi
	st8		[r31]=r18,16		// ndirty
	st8		[r30]=r19,16		// cfm
	nop		0
	;;
}
{	.mmi
	mov		r18=cr.isr
	st8		[r31]=r22,16		// iip
	add		r29=16,r30
	;;
}
{	.mmi
	st8		[r30]=r17,24		// ifa
	st8		[r31]=r18,24		// isr
	nop		0
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
	ssm		psr.dt|psr.ic|psr.dfh
	movl		gp=__gp
	;;
}
{	.mib
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
ENTRY_NOPROFILE(exception_restore, 0)
{	.mmi
	rsm		psr.i
	add		sp=16,sp
	nop		0
	;;
}

	// The next instruction can fault. Let it be...
	tpa		r9=sp
	;;
	rsm		psr.dt|psr.ic
	add		r8=SIZEOF_SPECIAL+16,r9
	;;
	srlz.d
	add		r2=SIZEOF_TRAPFRAME-16,r9
	add		r3=SIZEOF_TRAPFRAME-32,r9
	;;

{	.mmi
	ldf.fill	f15=[r2],-32		// f15
	ldf.fill	f14=[r3],-32		// f14
	nop		0
	;;
}
{	.mmi
	ldf.fill	f13=[r2],-32		// f13
	ldf.fill	f12=[r3],-32		// f12
	nop		0
	;;
}
{	.mmi
	ldf.fill	f11=[r2],-32		// f11
	ldf.fill	f10=[r3],-32		// f10
	nop		0
	;;
}
{	.mmi
	ldf.fill	f9=[r2],-32		// f9
	ldf.fill	f8=[r3],-32		// f8
	nop		0
	;;
}
{	.mmi
	ldf.fill	f7=[r2],-24		// f7
	ldf.fill	f6=[r3],-16		// f6
	nop		0
	;;
}
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
{	.mii
	ld8		r16=[r9]		// tf_length
	add		r31=16,r9
	add		r30=24,r9
}
{	.mmi
	ld8.fill	r15=[r3],-16		// r15
	ld8.fill	r14=[r2],-16		// r14
	nop		0
	;;
}
{	.mmi
	ld8.fill	r11=[r3],-16		// r11
	ld8.fill	r10=[r2],-16		// r10
	add		r16=r16,sp		// ar.k7
	;;
}
{	.mmi
	ld8.fill	r9=[r3],-16		// r9
	ld8.fill	r8=[r2],-16		// r8
	nop		0
	;;
}
{	.mmi
	ld8.fill	r3=[r3]			// r3
	ld8.fill	r2=[r2]			// r2
	nop		0
	;;
}

	ld8.fill	sp=[r31],16		// sp
	ld8		r17=[r30],16		// unat
	;;
	ld8		r29=[r31],16		// rp
	ld8		r18=[r30],16		// pr
	;;
	ld8		r28=[r31],16		// pfs
	ld8		r20=[r30],24		// bspstore
	mov		rp=r29
	;;
	ld8		r21=[r31],24		// rnat
	mov		ar.pfs=r28
	;;
	ld8.fill	r26=[r30],16		// tp
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
	ld8		r27=[r31],16		// ndirty
	cmp.le		p14,p15=IA64_VM_MINKERN_REGION,r28
	;;
}
{	.mmi
	ld8		r25=[r30]		// cfm
	ld8		r19=[r31]		// ip
	nop		0
	;;
}
{	.mii
	// Switch register stack
	alloc		r30=ar.pfs,0,0,0,0	// discard current frame
	shl		r31=r27,16		// value for ar.rsc
(p15)	mov		r13=r26
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
	addl		r29=NTLBRT_RESTORE,r0	// 22-bit restart token 
	;;
}

	ssm		psr.dt
	;;
	srlz.d

exception_restore_restart:
{	.mmi
	mov		r30=ar.bspstore
	;;
	loadrs					// load user regs
	mov		r29=0			// Clobber restart token
	;;
}
{	.mmi
	mov		r31=ar.bspstore
	;;
	mov		ar.bspstore=r20
	dep		r31=0,r31,0,13		// 8KB aligned
	;;
}
{	.mmi
	mov		ar.k6=r31
	mov		ar.rnat=r21
	nop		0
	;;
}
{	.mmi
	mov		ar.unat=r17
	mov		cr.iip=r19
	nop		0
}
{	.mmi
	mov		cr.ipsr=r24
	mov		cr.ifs=r25
	mov		pr=r18,0x1ffff
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
#define CALL(_func_, _n_, _ifa_)		\
{	.mib ;					\
	mov		r17=_ifa_ ;		\
	mov		r16=ip ;		\
	br.sptk		exception_save ;;	\
} ;						\
{	.mmi ;					\
	alloc		r15=ar.pfs,0,0,2,0 ;;	\
(p11)	ssm		psr.i ;			\
	mov		out0=_n_ ;;		\
} ;						\
{	.mib ;					\
(p11)	srlz.d ;				\
	add		out1=16,sp ;		\
	br.call.sptk	rp=_func_ ;;		\
} ;						\
{	.mib ;					\
	nop		0 ;			\
	nop		0 ;			\
	br.sptk		exception_restore ;;	\
}

#define	IVT_ENTRY(name, offset)			\
	.org	ia64_vector_table + offset;	\
	.global	ivt_##name;			\
	.proc	ivt_##name;			\
	.prologue;				\
	.unwabi	@svr4, 'I';			\
	.save	rp, r0;				\
	.body;					\
ivt_##name:					\
	XTRACE(offset)

#define	IVT_END(name)				\
	.endp	ivt_##name

#ifdef COMPAT_FREEBSD32
#define	IA32_TRAP	ia32_trap
#else
#define	IA32_TRAP	trap
#endif

/*
 * The IA64 Interrupt Vector Table (IVT) contains 20 slots with 64
 * bundles per vector and 48 slots with 16 bundles per vector.
 */

	.section .ivt, "ax"

	.align	32768
	.global ia64_vector_table
	.size	ia64_vector_table, 32768
ia64_vector_table:

IVT_ENTRY(VHPT_Translation, 0x0000)
	CALL(trap, 0, cr.ifa)
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
	ld8	r20=[r20]		// bucket head
	;;
	cmp.ne	p15,p0=r21,r19
(p15)	br.dpnt.few 1f
	;;
	ld8	r21=[r18]		// read pte
	;;
	itc.i	r21			// insert pte
	mov	pr=r17,0x1ffff
	;;
	rfi				// done
	;;
1:	rsm	psr.dt			// turn off data translations
	dep	r20=0,r20,61,3		// convert vhpt ptr to physical
	;;
	srlz.d				// serialize
	ld8	r20=[r20]		// first entry
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
	ld8	r21=[r20]		// read pte
	mov	r22=PTE_ACCESSED
	;;
	or	r21=r21,r22
	;;
	st8	[r20]=r21,8
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
	itc.i	r21			// and place in TLB
	ssm	psr.dt
	;; 
	srlz.d
	mov	pr=r17,0x1ffff		// restore predicates
	rfi
	;;
3:	add	r20=24,r20		// next in chain
	;;
	ld8	r20=[r20]		// read chain
	br.sptk	2b			// loop
	;;
9:	ssm	psr.dt
	mov	pr=r17,0x1ffff		// restore predicates
	;;
	srlz.d
	;; 
	CALL(trap, 20, cr.ifa)		// Page Not Present trap
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
	ld8	r20=[r20]		// bucket head
	;;
	cmp.ne	p15,p0=r21,r19
(p15)	br.dpnt.few 1f
	;;
	ld8	r21=[r18]		// read pte
	;;
	itc.d	r21			// insert pte
	mov	pr=r17,0x1ffff
	;;
	rfi				// done
	;;
1:	rsm	psr.dt			// turn off data translations
	dep	r20=0,r20,61,3		// convert vhpt ptr to physical
	;; 
	srlz.d				// serialize
	ld8	r20=[r20]		// first entry
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
	ld8	r21=[r20]		// read pte
	mov	r22=PTE_ACCESSED
	;;
	or	r21=r21,r22
	;;
	st8	[r20]=r21,8
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
	itc.d	r21			// and place in TLB
	ssm	psr.dt
	;; 
	srlz.d
	mov	pr=r17,0x1ffff		// restore predicates
	rfi
	;;
3:	add	r20=24,r20		// next in chain
	;;
	ld8	r20=[r20]		// read chain
	br.sptk	2b			// loop
	;;
9:	ssm	psr.dt
	mov	pr=r17,0x1ffff		// restore predicates
	;;
	srlz.d
	;; 
	CALL(trap, 20, cr.ifa)		// Page Not Present trap
IVT_END(Data_TLB)

IVT_ENTRY(Alternate_Instruction_TLB, 0x0c00)
	mov	r16=cr.ifa		// where did it happen
	mov	r18=pr			// save predicates
	;;
	extr.u	r17=r16,61,3		// get region number
	mov	r19=PTE_PRESENT+PTE_ACCESSED+PTE_DIRTY+PTE_PL_KERN+PTE_AR_RWX
	;;
	cmp.eq	p13,p0=IA64_PBVM_RR,r17		// RR4?
(p13)	br.cond.sptk.few	4f
	;;
	cmp.ge	p13,p0=5,r17		// RR0-RR5?
	cmp.eq	p14,p15=7,r17		// RR7?
(p13)	br.cond.spnt.few	9f
	;;
(p14)	add	r19=PTE_MA_WB,r19
(p15)	add	r19=PTE_MA_UC,r19
	dep	r17=0,r16,50,14		// clear bits above PPN
	;;
1:	dep	r16=r19,r17,0,12	// put pte bits in 0..11
	;;
	itc.i	r16
	mov	pr=r18,0x1ffff		// restore predicates
	;;
	rfi
	;;
4:
	add	r19=PTE_MA_WB,r19
	movl	r17=IA64_PBVM_BASE
	;;
	sub	r17=r16,r17
	movl	r16=IA64_PBVM_PGTBL
	;;
	extr.u	r17=r17,IA64_PBVM_PAGE_SHIFT,61-IA64_PBVM_PAGE_SHIFT
	;;
	shladd	r16=r17,3,r16
	;;
	ld8	r17=[r16]
	br.sptk	1b
	;;
9:	mov	pr=r18,0x1ffff		// restore predicates
	CALL(trap, 3, cr.ifa)
IVT_END(Alternate_Instruction_TLB)

IVT_ENTRY(Alternate_Data_TLB, 0x1000)
	mov	r16=cr.ifa		// where did it happen
	mov	r18=pr			// save predicates
	;;
	extr.u	r17=r16,61,3		// get region number
	mov	r19=PTE_PRESENT+PTE_ACCESSED+PTE_DIRTY+PTE_PL_KERN+PTE_AR_RWX
	;;
	cmp.eq	p13,p0=IA64_PBVM_RR,r17		// RR4?
(p13)	br.cond.sptk.few	4f
	;;
	cmp.ge	p13,p0=5,r17		// RR0-RR5?
	cmp.eq	p14,p15=7,r17		// RR7?
(p13)	br.cond.spnt.few	9f
	;;
(p14)	add	r19=PTE_MA_WB,r19
(p15)	add	r19=PTE_MA_UC,r19
	dep	r17=0,r16,50,14		// clear bits above PPN
	;;
1:	dep	r16=r19,r17,0,12	// put pte bits in 0..11
	;;
	itc.d	r16
	mov	pr=r18,0x1ffff		// restore predicates
	;;
	rfi
	;;
4:
	add	r19=PTE_MA_WB,r19
	movl	r17=IA64_PBVM_BASE
	;;
	sub	r17=r16,r17
	movl	r16=IA64_PBVM_PGTBL
	;;
	extr.u	r17=r17,IA64_PBVM_PAGE_SHIFT,61-IA64_PBVM_PAGE_SHIFT
	;;
	shladd	r16=r17,3,r16
	;;
	ld8	r17=[r16]
	br.sptk	1b
	;;
9:	mov	pr=r18,0x1ffff		// restore predicates
	CALL(trap, 4, cr.ifa)
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

{	.mmi
	mov		cr.ifa=r30
	mov		r26=rr[r30]
	extr.u		r27=r30,61,3
	;;
}
{	.mii
	nop		0
	dep		r26=0,r26,0,2
	cmp.eq		p12,p13=7,r27
	;;
}
{	.mii
	mov		cr.itir=r26
(p12)	dep		r28=0,r30,0,12
(p13)	extr.u		r28=r30,3*PAGE_SHIFT-8, PAGE_SHIFT-3	// dir L0 index
	;;
}
{	.mlx
(p12)	add		r28=PTE_PRESENT+PTE_ACCESSED+PTE_DIRTY+PTE_PL_KERN+PTE_AR_RWX+PTE_MA_WB,r28
(p13)	movl		r27=ia64_kptdir
	;;
}
{	.mib
(p13)	ld8		r27=[r27]
(p13)	extr.u		r26=r30,2*PAGE_SHIFT-5, PAGE_SHIFT-3	// dir L1 index
(p12)	br.cond.spnt.few 1f
	;;
}
{	.mmi
	rsm		psr.dt
	;;
	srlz.d
	dep		r27=0,r27,61,3
	;;
}
{	.mmi
	shladd		r27=r28,3,r27
	;;
	ld8		r27=[r27]				// dir L1 page
	extr.u		r28=r30,PAGE_SHIFT,PAGE_SHIFT-5		// pte index
	;;
}
{	.mii
	shladd		r27=r26,3,r27
	shl		r28=r28,5
	;;
	dep		r27=0,r27,61,3
	;;
}
	ld8		r27=[r27]				// pte page
	;;
	add		r27=r28,r27
	;;
	dep		r27=0,r27,61,3
	;;
	ld8		r28=[r27]				// pte
	;;
	or		r28=PTE_DIRTY+PTE_ACCESSED,r28
	;;
	st8		[r27]=r28
	;;
	ssm		psr.dt
	;;
1:
{	.mmi
	itc.d		r28
	;;
	addl		r26=NTLBRT_SAVE,r0
	addl		r27=NTLBRT_RESTORE,r0
	;;
}
{	.mmi
	srlz.d
	cmp.eq		p12,p0=r29,r26
	cmp.eq		p13,p0=r29,r27
	;;
}
{	.mbb
	nop		0
(p12)	br.cond.sptk.few	exception_save_restart
(p13)	br.cond.sptk.few	exception_restore_restart
	;;
}

{	.mlx
	mov		r26=ar.bsp
	movl		r29=kstack
	;;
}
{	.mlx
	mov		r28=sp
	movl		r27=kstack_top
	;;
}
{	.mmi
	add		sp=-16,r27
	;;
	mov		r27=ar.bspstore
	nop		0
	;;
}
	mov		ar.rsc=0
	dep		r29=r27,r29,0,9
	;;
	mov		ar.bspstore=r29
	;;
	CALL(trap, 5, r30)
IVT_END(Data_Nested_TLB)

IVT_ENTRY(Instruction_Key_Miss, 0x1800)
	CALL(trap, 6, cr.ifa)
IVT_END(Instruction_Key_Miss)

IVT_ENTRY(Data_Key_Miss, 0x1c00)
	CALL(trap, 7, cr.ifa)
IVT_END(Data_Key_Miss)

IVT_ENTRY(Dirty_Bit, 0x2000)
	mov	r16=cr.ifa
	mov	r17=pr
	;;
	thash	r18=r16
	;;
	ttag	r19=r16
	add	r20=24,r18		// collision chain
	;; 
	ld8	r20=[r20]		// bucket head
	;;
	rsm	psr.dt			// turn off data translations
	dep	r20=0,r20,61,3		// convert vhpt ptr to physical
	;;
	srlz.d				// serialize
	ld8	r20=[r20]		// first entry
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
	mov	r22=PTE_DIRTY+PTE_ACCESSED
	;;
	or	r21=r22,r21		// set dirty & access bit
	;;
	st8	[r20]=r21,8		// store back
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
	itc.d	r21			// and place in TLB
	ssm	psr.dt
	;; 
	srlz.d
	mov	pr=r17,0x1ffff		// restore predicates
	rfi
	;;
2:	add	r20=24,r20		// next in chain
	;;
	ld8	r20=[r20]		// read chain
	br.sptk	1b			// loop
	;;
9:	ssm	psr.dt
	mov	pr=r17,0x1ffff		// restore predicates
	;;
	srlz.d
	;;
	CALL(trap, 8, cr.ifa)			// die horribly
IVT_END(Dirty_Bit)

IVT_ENTRY(Instruction_Access_Bit, 0x2400)
	mov	r16=cr.ifa
	mov	r17=pr
	;;
	thash	r18=r16
	;;
	ttag	r19=r16
	add	r20=24,r18		// collision chain
	;; 
	ld8	r20=[r20]		// bucket head
	;;
	rsm	psr.dt			// turn off data translations
	dep	r20=0,r20,61,3		// convert vhpt ptr to physical
	;;
	srlz.d				// serialize
	ld8	r20=[r20]		// first entry
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
	mov	r22=PTE_ACCESSED
	;;
	or	r21=r22,r21		// set accessed bit
	;;
	st8	[r20]=r21,8		// store back
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
	itc.i	r21			// and place in TLB
	ssm	psr.dt
	;; 
	srlz.d
	mov	pr=r17,0x1ffff		// restore predicates
	rfi				// walker will retry the access
	;;
2:	add	r20=24,r20		// next in chain
	;;
	ld8	r20=[r20]		// read chain
	br.sptk	1b			// loop
	;;
9:	ssm	psr.dt
	mov	pr=r17,0x1ffff		// restore predicates
	;;
	srlz.d
	;;
	CALL(trap, 9, cr.ifa)
IVT_END(Instruction_Access_Bit)

IVT_ENTRY(Data_Access_Bit, 0x2800)
	mov	r16=cr.ifa
	mov	r17=pr
	;;
	thash	r18=r16
	;;
	ttag	r19=r16
	add	r20=24,r18		// collision chain
	;;
	ld8	r20=[r20]		// bucket head
	;;
	rsm	psr.dt			// turn off data translations
	dep	r20=0,r20,61,3		// convert vhpt ptr to physical
	;;
	srlz.d				// serialize
	ld8	r20=[r20]		// first entry
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
	mov	r22=PTE_ACCESSED
	;;
	or	r21=r22,r21		// set accessed bit
	;;
	st8	[r20]=r21,8		// store back
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
	itc.d	r21			// and place in TLB
	ssm	psr.dt
	;; 
	srlz.d
	mov	pr=r17,0x1ffff		// restore predicates
	rfi				// walker will retry the access
	;;
2:	add	r20=24,r20		// next in chain
	;;
	ld8	r20=[r20]		// read chain
	br.sptk	1b			// loop
	;;
9:	ssm	psr.dt
	mov	pr=r17,0x1ffff		// restore predicates
	;;
	srlz.d
	;;
	CALL(trap, 10, cr.ifa)
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
	;;
(p11)	ssm		psr.i
	mov		out0=11
	;;
}
{	.mmi
	flushrs
	;;
(p11)	srlz.d
	add		out1=16,sp
}
{	.mib
	nop		0
	nop		0
	br.call.sptk	rp=trap
	;;
}
{	.mib
	nop		0
	nop		0
	br.sptk		exception_restore
	;;
}
IVT_END(Break_Instruction)

IVT_ENTRY(External_Interrupt, 0x3000)
{	.mib
	mov		r17=ar.itc	// Put the ITC in the trapframe.
	mov		r16=ip
	br.sptk		exception_save
	;;
}
{	.mmi
	alloc		r15=ar.pfs,0,0,1,0
	nop		0
	nop		0
	;;
}
{	.mib
	add		out0=16,sp
	nop		0
	br.call.sptk	rp=ia64_handle_intr
	;;
}
{	.mib
	nop		0
	nop		0
	br.sptk		exception_restore
	;;
}
IVT_END(External_Interrupt)

IVT_ENTRY(Reserved_3400, 0x3400)
	CALL(trap, 13, cr.ifa)
IVT_END(Reserved_3400)

IVT_ENTRY(Reserved_3800, 0x3800)
	CALL(trap, 14, cr.ifa)
IVT_END(Reserved_3800)

IVT_ENTRY(Reserved_3c00, 0x3c00)
	CALL(trap, 15, cr.ifa)
IVT_END(Reserved_3c00)

IVT_ENTRY(Reserved_4000, 0x4000)
	CALL(trap, 16, cr.ifa)
IVT_END(Reserved_4000)

IVT_ENTRY(Reserved_4400, 0x4400)
	CALL(trap, 17, cr.ifa)
IVT_END(Reserved_4400)

IVT_ENTRY(Reserved_4800, 0x4800)
	CALL(trap, 18, cr.ifa)
IVT_END(Reserved_4800)

IVT_ENTRY(Reserved_4c00, 0x4c00)
	CALL(trap, 19, cr.ifa)
IVT_END(Reserved_4c00)

IVT_ENTRY(Page_Not_Present, 0x5000)
	CALL(trap, 20, cr.ifa)
IVT_END(Page_Not_Present)

IVT_ENTRY(Key_Permission, 0x5100)
	CALL(trap, 21, cr.ifa)
IVT_END(Key_Permission)

IVT_ENTRY(Instruction_Access_Rights, 0x5200)
	CALL(trap, 22, cr.ifa)
IVT_END(Instruction_Access_Rights)

IVT_ENTRY(Data_Access_Rights, 0x5300)
	CALL(trap, 23, cr.ifa)
IVT_END(Data_Access_Rights)

IVT_ENTRY(General_Exception, 0x5400)
	CALL(trap, 24, cr.ifa)
IVT_END(General_Exception)

IVT_ENTRY(Disabled_FP_Register, 0x5500)
	CALL(trap, 25, cr.ifa)
IVT_END(Disabled_FP_Register)

IVT_ENTRY(NaT_Consumption, 0x5600)
	CALL(trap, 26, cr.ifa)
IVT_END(NaT_Consumption)

IVT_ENTRY(Speculation, 0x5700)
	CALL(trap, 27, cr.iim)
IVT_END(Speculation)

IVT_ENTRY(Reserved_5800, 0x5800)
	CALL(trap, 28, cr.ifa)
IVT_END(Reserved_5800)

IVT_ENTRY(Debug, 0x5900)
	CALL(trap, 29, cr.ifa)
IVT_END(Debug)

IVT_ENTRY(Unaligned_Reference, 0x5a00)
	CALL(trap, 30, cr.ifa)
IVT_END(Unaligned_Reference)

IVT_ENTRY(Unsupported_Data_Reference, 0x5b00)
	CALL(trap, 31, cr.ifa)
IVT_END(Unsupported_Data_Reference)

IVT_ENTRY(Floating_Point_Fault, 0x5c00)
	CALL(trap, 32, cr.ifa)
IVT_END(Floating_Point_Fault)

IVT_ENTRY(Floating_Point_Trap, 0x5d00)
	CALL(trap, 33, cr.ifa)
IVT_END(Floating_Point_Trap)

IVT_ENTRY(Lower_Privilege_Transfer_Trap, 0x5e00)
	CALL(trap, 34, cr.ifa)
IVT_END(Lower_Privilege_Transfer_Trap)

IVT_ENTRY(Taken_Branch_Trap, 0x5f00)
	CALL(trap, 35, cr.ifa)
IVT_END(Taken_Branch_Trap)

IVT_ENTRY(Single_Step_Trap, 0x6000)
	CALL(trap, 36, cr.ifa)
IVT_END(Single_Step_Trap)

IVT_ENTRY(Reserved_6100, 0x6100)
	CALL(trap, 37, cr.ifa)
IVT_END(Reserved_6100)

IVT_ENTRY(Reserved_6200, 0x6200)
	CALL(trap, 38, cr.ifa)
IVT_END(Reserved_6200)

IVT_ENTRY(Reserved_6300, 0x6300)
	CALL(trap, 39, cr.ifa)
IVT_END(Reserved_6300)

IVT_ENTRY(Reserved_6400, 0x6400)
	CALL(trap, 40, cr.ifa)
IVT_END(Reserved_6400)

IVT_ENTRY(Reserved_6500, 0x6500)
	CALL(trap, 41, cr.ifa)
IVT_END(Reserved_6500)

IVT_ENTRY(Reserved_6600, 0x6600)
	CALL(trap, 42, cr.ifa)
IVT_END(Reserved_6600)

IVT_ENTRY(Reserved_6700, 0x6700)
	CALL(trap, 43, cr.ifa)
IVT_END(Reserved_6700)

IVT_ENTRY(Reserved_6800, 0x6800)
	CALL(trap, 44, cr.ifa)
IVT_END(Reserved_6800)

IVT_ENTRY(IA_32_Exception, 0x6900)
	CALL(IA32_TRAP, 45, cr.ifa)
IVT_END(IA_32_Exception)

IVT_ENTRY(IA_32_Intercept, 0x6a00)
	CALL(IA32_TRAP, 46, cr.iim)
IVT_END(IA_32_Intercept)

IVT_ENTRY(IA_32_Interrupt, 0x6b00)
	CALL(IA32_TRAP, 47, cr.ifa)
IVT_END(IA_32_Interrupt)

IVT_ENTRY(Reserved_6c00, 0x6c00)
	CALL(trap, 48, cr.ifa)
IVT_END(Reserved_6c00)

IVT_ENTRY(Reserved_6d00, 0x6d00)
	CALL(trap, 49, cr.ifa)
IVT_END(Reserved_6d00)

IVT_ENTRY(Reserved_6e00, 0x6e00)
	CALL(trap, 50, cr.ifa)
IVT_END(Reserved_6e00)

IVT_ENTRY(Reserved_6f00, 0x6f00)
	CALL(trap, 51, cr.ifa)
IVT_END(Reserved_6f00)

IVT_ENTRY(Reserved_7000, 0x7000)
	CALL(trap, 52, cr.ifa)
IVT_END(Reserved_7000)

IVT_ENTRY(Reserved_7100, 0x7100)
	CALL(trap, 53, cr.ifa)
IVT_END(Reserved_7100)

IVT_ENTRY(Reserved_7200, 0x7200)
	CALL(trap, 54, cr.ifa)
IVT_END(Reserved_7200)

IVT_ENTRY(Reserved_7300, 0x7300)
	CALL(trap, 55, cr.ifa)
IVT_END(Reserved_7300)

IVT_ENTRY(Reserved_7400, 0x7400)
	CALL(trap, 56, cr.ifa)
IVT_END(Reserved_7400)

IVT_ENTRY(Reserved_7500, 0x7500)
	CALL(trap, 57, cr.ifa)
IVT_END(Reserved_7500)

IVT_ENTRY(Reserved_7600, 0x7600)
	CALL(trap, 58, cr.ifa)
IVT_END(Reserved_7600)

IVT_ENTRY(Reserved_7700, 0x7700)
	CALL(trap, 59, cr.ifa)
IVT_END(Reserved_7700)

IVT_ENTRY(Reserved_7800, 0x7800)
	CALL(trap, 60, cr.ifa)
IVT_END(Reserved_7800)

IVT_ENTRY(Reserved_7900, 0x7900)
	CALL(trap, 61, cr.ifa)
IVT_END(Reserved_7900)

IVT_ENTRY(Reserved_7a00, 0x7a00)
	CALL(trap, 62, cr.ifa)
IVT_END(Reserved_7a00)

IVT_ENTRY(Reserved_7b00, 0x7b00)
	CALL(trap, 63, cr.ifa)
IVT_END(Reserved_7b00)

IVT_ENTRY(Reserved_7c00, 0x7c00)
	CALL(trap, 64, cr.ifa)
IVT_END(Reserved_7c00)

IVT_ENTRY(Reserved_7d00, 0x7d00)
	CALL(trap, 65, cr.ifa)
IVT_END(Reserved_7d00)

IVT_ENTRY(Reserved_7e00, 0x7e00)
	CALL(trap, 66, cr.ifa)
IVT_END(Reserved_7e00)

IVT_ENTRY(Reserved_7f00, 0x7f00)
	CALL(trap, 67, cr.ifa)
IVT_END(Reserved_7f00)
