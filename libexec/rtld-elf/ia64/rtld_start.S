/* $FreeBSD$	*/
/*	From: NetBSD: rtld_start.S,v 1.1 1996/12/16 20:38:09 cgd Exp 	*/

/*
 * Copyright 1996 Matt Thomas <matt@3am-software.com>
 * Copyright 2000 John D. Polstra
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/asm.h>
#include <sys/syscall.h>

ENTRY(_rtld_start, 0)
	alloc	r2=ar.pfs,3,0,3,0
	;;
1:	mov	r14=ip			// calculate gp
	addl	r3=@gprel(1b),r0
	;;
	sub	gp=r14,r3
	;;
	.section .sdata
2:	data4	@ltv(1b)		// unrelocated address of 1b
	.align	8
	.previous
	add	r15=@gprel(2b),gp
	;;
	ld8	r15=[r15]
	;;
	sub	out0=r14,r15		// out0 is image base address
	br.call.sptk.many rp=_rtld_reloc // fixup image
	
	add	sp=-16,sp		// 16 bytes for us, 16 for _rtld
	;;
	mov	out0=in0
	add	out1=16,sp		// address for exit proc
	add	out2=24,sp		// address for obj_main

	br.call.sptk.many rp=_rtld	// r8=_rtld(sp, &exit_proc, &obj_main)

	add	r16=16,sp		// address for exit proc
	;; 
	ld8	r15=[r16]		// read exit proc
	add	sp=16,sp		// readjust stack
	mov	b7=r8			// address of real _start
	;;
	alloc	r2=ar.pfs,0,0,3,0	// dump register frame
	mov	out2=r15

	br.call.sptk.many rp=b7		// transfer to main program
	br.call.sptk.many rp=exit	// die
END(_rtld_start)

/*
 * _rtld_bind_start:	lookup a lazy binding and transfer to real target
 *
 * Arguments:
 *	r1	gp value for rtld
 *	r15	Index in plt
 *	r16	Obj_Entry of caller
 *	in0-in7	Arguments for target procedure
 *	rp	Return address back to caller
 */
ENTRY(_rtld_bind_start, 0)
{ .mii
	alloc	loc0=ar.pfs,8,6,3,0	// space to save r8-r11
	add	r17=16-8*16,sp		// leave 16 bytes for _rtld_bind
	add	r18=32-8*16,sp
	;;
} { .mii
	mov	loc2=r8			// structure return address
	add	sp=-8*16,sp		// space to save f8-f15
	mov	loc1=rp
	;;
} { .mii
	stf.spill [r17]=f8,32		// save float arguments
	mov	loc3=r9			// language specific
	mov	loc4=r10		// language specific
} { .mii
	stf.spill [r18]=f9,32
	mov	loc5=r11		// language specific
	shl	out1=r15,4		// 16 * index
	;;
} { .mmi
	stf.spill [r17]=f10,32
	stf.spill [r18]=f11,32
	mov	out0=r16		// Obj_Entry for caller
	;; 
} { .mmi
	stf.spill [r17]=f12,32
	stf.spill [r18]=f13,32
	shladd	out1=r15,3,out1		// rela offset = 24 * index
	;; 
} { .mmb
	stf.spill [r17]=f14,32
	stf.spill [r18]=f15,32
	br.call.sptk.many rp=_rtld_bind
} { .mii
	ld8	r14=[r8],8		// target address
	add	r17=16,sp
	add	r18=32,sp
	;;
} { .mii
	ld8	r1=[r8]			// target gp
	mov	ar.pfs=loc0		// clean up
	mov	rp=loc1
} { .mmi	
	ldf.fill f8=[r17],32		// restore float arguments
	ldf.fill f9=[r18],32
	mov	r8=loc2			// restore structure pointer
	;; 
} { .mmi
	ldf.fill f10=[r17],32
	ldf.fill f11=[r18],32
	mov	r9=loc3
	;; 
} { .mmi
	ldf.fill f12=[r17],32
	ldf.fill f13=[r18],32
	mov	r10=loc4
	;; 
} { .mmi
	ldf.fill f14=[r17],32
	ldf.fill f15=[r18],32
	mov	r11=loc5
	;;
} { .mii
	nop.m	0
	mov	b7=r14
	add	sp=8*16,sp
	;;
} { .mib
	alloc	r14=ar.pfs,0,0,8,0	// drop our register frame
	nop.i	0
	br.sptk.many b7			// jump to target
}
END(_rtld_bind_start)

/*
 * int cmp0_and_store_int(volatile int *p, int newval);
 *
 * If an int holds 0, store newval into it; else do nothing.  Returns
 * the previous value.
 */
ENTRY(cmp0_and_store_int, 2)
	mov	ar.ccv=0
	;;
	cmpxchg4.acq r8=[in0],in1,ar.ccv
	br.ret.sptk.many rp
END(cmp0_and_store_int)

ENTRY(atomic_add_int, 2)
1:	ld4	r14=[in0]
	;;
	mov	ar.ccv=r14
	add	r15=in1,r14
	;;
	cmpxchg4.acq r16=[in0],r15,ar.ccv
	;;
	cmp.ne	p6,p0=r14,r16
(p6)	br.cond.spnt.few 1b
	br.ret.sptk.many rp
END(atomic_add_int)
 
/* Atomically increment an int. */
ENTRY(atomic_incr_int, 1)
1:	ld4	r14=[in0]
	;;
	mov	ar.ccv=r14
	add	r15=1,r14
	;;
	cmpxchg4.acq r16=[in0],r15,ar.ccv
	;;
	cmp.ne	p6,p0=r14,r16
(p6)	br.cond.spnt.few 1b
	br.ret.sptk.many rp
END(atomic_incr_int)

/* Atomically decrement an int. */
ENTRY(atomic_decr_int, 1)
1:	ld4	r14=[in0]
	;;
	mov	ar.ccv=r14
	add	r15=-1,r14
	;;
	cmpxchg4.acq r16=[in0],r15,ar.ccv
	;;
	cmp.ne	p6,p0=r14,r16
(p6)	br.cond.spnt.few 1b
	br.ret.sptk.many rp
END(atomic_decr_int)

#define DT_NULL		0	/* Terminating entry. */
#define DT_RELA		7	/* Address of ElfNN_Rela relocations. */
#define DT_RELASZ	8	/* Total size of ElfNN_Rela relocations. */
#define DT_RELAENT	9	/* Size of each ElfNN_Rela relocation entry. */
	
#define R_IA64_NONE		0	/* None */
#define R_IA64_DIR64LSB		0x27	/* word64 LSB	S + A */
#define R_IA64_REL64LSB		0x6f	/* word64 LSB	BD + A */
	
/*
 * _rtld_reloc:	relocate the rtld image, apart from @fptrs.
 *
 * Assumes that rtld was linked at zero and that we only need to
 * handle REL64LSB and DIR64LSB relocations.
 *
 * Arguments:
 *	r1	gp value for rtld
 *	in0	rtld base address
 */
STATIC_ENTRY(_rtld_reloc, 1)
	alloc	loc0=ar.pfs,1,2,0,0
	mov	loc1=rp
	;; 
	movl	r15=@gprel(_DYNAMIC)	// find _DYNAMIC etc.
	;;
	add	r15=r15,gp		// relocate _DYNAMIC etc.
	;;
1:	ld8	r16=[r15],8		// read r15->d_tag
	;;
	ld8	r17=[r15],8		// and r15->d_val
	;;
	cmp.eq	p6,p0=DT_NULL,r16	// done?
(p6)	br.cond.dpnt.few 2f
	;; 
	cmp.eq	p6,p0=DT_RELA,r16
	;; 
(p6)	add	r18=r17,in0		// found rela section
	;; 
	cmp.eq	p6,p0=DT_RELASZ,r16
	;; 
(p6)	mov	r19=r17			// found rela size
	;; 
	cmp.eq	p6,p0=DT_RELAENT,r16
	;; 
(p6)	mov	r22=r17			// found rela entry size
	;;
	br.sptk.few 1b
	
2:	
	ld8	r15=[r18],8		// read r_offset
	;; 
	ld8	r16=[r18],8		// read r_info
	add	r15=r15,in0		// relocate r_offset
	;;
	ld8	r17=[r18],8		// read r_addend
	sub	r19=r19,r22		// update relasz

	extr.u	r23=r16,0,32		// ELF64_R_TYPE(r16)
	;;
	cmp.eq	p6,p0=R_IA64_NONE,r23
(p6)	br.cond.dpnt.few 3f
	;;
	cmp.eq	p6,p0=R_IA64_DIR64LSB,r23
	;;
(p6)	br.cond.dptk.few 4f
	;;
	cmp.eq	p6,p0=R_IA64_REL64LSB,r23
	;;
(p6)	br.cond.dptk.few 4f
	;;

3:	cmp.ltu	p6,p0=0,r19		// more?
(p6)	br.cond.dptk.few 2b		// loop
	
	mov	r8=0			// success return value
	;;
	br.cond.sptk.few 9f		// done

4:
	ld8	r16=[r15]		// read value
	;;
	add	r16=r16,in0		// relocate it
	;;
	st8	[r15]=r16		// and store it back
	br.cond.sptk.few 3b

9:
	mov	ar.pfs=loc0
	mov	rp=loc1
	;;
	br.ret.sptk.few rp

END(_rtld_reloc)
