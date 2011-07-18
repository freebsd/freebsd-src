/*-
 * Copyright (c) 2001-2011 Marcel Moolenaar
 * Copyright (c) 1998 Doug Rabson
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

#include <machine/asm.h>
#include <machine/ia64_cpu.h>
#include <machine/intrcnt.h>
#include <machine/pte.h>
#include <assym.s>

/*
 * The Altix 350 needs more than the architected 16KB (8KB for stack and
 * 8KB for RSE backing store) when calling EFI to setup virtual mode.
 */
#define	FW_STACK_SIZE	3*PAGE_SIZE

	.section .ivt.data, "aw"
	.align	PAGE_SIZE
	.global	kstack
kstack:	.space	FW_STACK_SIZE
	.global	kstack_top
kstack_top:

	.text

/*
 * Not really a leaf but we can't return.
 * The EFI loader passes the physical address of the bootinfo block in
 * register r8.
 */
ENTRY_NOPROFILE(__start, 1)
	.prologue
	.save	rp,r0
	.body
{	.mlx
	mov	ar.rsc=0
	movl	r16=ia64_vector_table	// set up IVT early
	;;
}
{	.mlx
	mov	cr.iva=r16
	movl	r16=kstack
	;;
}
{	.mmi
	srlz.i
	;;
	ssm	IA64_PSR_DFH
	mov	r17=FW_STACK_SIZE-16
	;;
}
{	.mlx
	add	sp=r16,r17		// proc0's stack
	movl	gp=__gp			// find kernel globals
	;;
}
{	.mlx
	mov	ar.bspstore=r16		// switch backing store
	movl	r16=bootinfo
	;;
}
{	.mmi
	st8	[r16]=r8		// save the PA of the bootinfo block
	loadrs				// invalidate regs
	mov	r17=IA64_DCR_DEFAULT
	;;
}
{	.mmi
	mov	cr.dcr=r17
	mov	ar.rsc=3		// turn rse back on
	nop	0
	;;
}
{	.mmi
	srlz.d
	alloc	r16=ar.pfs,0,0,1,0
	mov	out0=r0			// we are linked at the right address 
	;;				// we just need to process fptrs
}
{	.mib
	nop	0
	nop	0
	br.call.sptk.many rp=_reloc
	;;
}
{	.mib
	nop	0
	nop	0
	br.call.sptk.many rp=ia64_init
	;;
}
	// We have the new bspstore in r8 and the new sp in r9.
	// Switch onto the new stack and call mi_startup().
{	.mmi
	mov	ar.rsc = 0
	;;
	mov	ar.bspstore = r8
	mov	sp = r9
	;;
}
{	.mmi
	loadrs
	;;
	mov	ar.rsc = 3
	nop	0
	;;
}
{	.mib
	nop	0
	nop	0
	br.call.sptk.many rp=mi_startup
	;;
}
	/* NOTREACHED */
1:	br.cond.sptk.few 1b
END(__start)

/*
 * fork_trampoline()
 *
 * Arrange for a function to be invoked neatly, after a cpu_switch().
 *
 * Invokes fork_exit() passing in three arguments: a callout function, an
 * argument to the callout, and a trapframe pointer.  For child processes
 * returning from fork(2), the argument is a pointer to the child process.
 *
 * The callout function and its argument is in the trapframe in scratch
 * registers r2 and r3.
 */
ENTRY(fork_trampoline, 0)
	.prologue
	.save	rp,r0
	.body
{	.mmi
	alloc		r14=ar.pfs,0,0,3,0
	add		r15=32+SIZEOF_SPECIAL+8,sp
	add		r16=32+SIZEOF_SPECIAL+16,sp
	;;
}
{	.mmi
	ld8		out0=[r15]
	ld8		out1=[r16]
	nop		0
}
{	.mib
	add		out2=16,sp
	nop		0
	br.call.sptk	rp=fork_exit
	;;
}
	// If we get back here, it means we're a user space process that's
	// the immediate result of fork(2).
	.global		enter_userland
	.type		enter_userland, @function
enter_userland:
{	.mib
	nop		0
	nop		0
	br.sptk		epc_syscall_return
	;;
}
END(fork_trampoline)

/*
 * Create a default interrupt name table. The first entry (vector 0) is
 * hardwaired to the clock interrupt.
 */
	.data
	.align 8
EXPORT(intrnames)
	.ascii "clock"
	.fill INTRNAME_LEN - 5 - 1, 1, ' '
	.byte 0
intr_n = 1
.rept INTRCNT_COUNT - 1
	.ascii "#"
	.byte intr_n / 100 + '0'
	.byte (intr_n % 100) / 10 + '0'
	.byte intr_n % 10 + '0'
	.fill INTRNAME_LEN - 1 - 3 - 1, 1, ' '
	.byte 0
	intr_n = intr_n + 1
.endr
EXPORT(sintrnames)
	.word INTRCNT_COUNT * INTRNAME_LEN

	.align 8
EXPORT(intrcnt)
	.fill INTRCNT_COUNT, 8, 0
EXPORT(sintrcnt)
	.word INTRCNT_COUNT

	.text
	// in0:	image base
STATIC_ENTRY(_reloc, 1)
	alloc	loc0=ar.pfs,1,2,0,0
	mov	loc1=rp
	;; 
	movl	r15=@gprel(_DYNAMIC)	// find _DYNAMIC etc.
	movl	r2=@gprel(fptr_storage)
	movl	r3=@gprel(fptr_storage_end)
	;;
	add	r15=r15,gp		// relocate _DYNAMIC etc.
	add	r2=r2,gp
	add	r3=r3,gp
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
	cmp.eq	p6,p0=DT_SYMTAB,r16
	;; 
(p6)	add	r20=r17,in0		// found symbol table
	;; 
(p6)	setf.sig f8=r20
	;; 
	cmp.eq	p6,p0=DT_SYMENT,r16
	;; 
(p6)	setf.sig f9=r17			// found symbol entry size
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
	cmp.eq	p6,p0=R_IA_64_NONE,r23
(p6)	br.cond.dpnt.few 3f
	;;
	cmp.eq	p6,p0=R_IA_64_REL64LSB,r23
(p6)	br.cond.dptk.few 4f
	;;

	extr.u	r16=r16,32,32		// ELF64_R_SYM(r16)
	;; 
	setf.sig f10=r16		// so we can multiply
	;;
	xma.lu	f10=f10,f9,f8		// f10=symtab + r_sym*syment
	;;
	getf.sig r16=f10
	;;
	add	r16=8,r16		// address of st_value
	;;
	ld8	r16=[r16]		// read symbol value
	;;
	add	r16=r16,in0		// relocate symbol value
	;;

	cmp.eq	p6,p0=R_IA_64_DIR64LSB,r23
(p6)	br.cond.dptk.few 5f
	;;
	cmp.eq	p6,p0=R_IA_64_FPTR64LSB,r23
(p6)	br.cond.dptk.few 6f
	;;

3:
	cmp.ltu	p6,p0=0,r19		// more?
(p6)	br.cond.dptk.few 2b		// loop
	mov	r8=0			// success return value
	br.cond.sptk.few 9f		// done

4:
	add	r16=in0,r17		// BD + A
	;;
	st8	[r15]=r16		// word64 (LSB)
	br.cond.sptk.few 3b

5:
	add	r16=r16,r17		// S + A
	;;
	st8	[r15]=r16		// word64 (LSB)
	br.cond.sptk.few 3b

6:
	movl	r17=@gprel(fptr_storage)
	;;
	add	r17=r17,gp		// start of fptrs
	;;
7:	cmp.geu	p6,p0=r17,r2		// end of fptrs?
(p6)	br.cond.dpnt.few 8f		// can't find existing fptr
	ld8	r20=[r17]		// read function from fptr
	;;
	cmp.eq	p6,p0=r16,r20		// same function?
	;;
(p6)	st8	[r15]=r17		// reuse fptr
(p6)	br.cond.sptk.few 3b		// done
	add	r17=16,r17		// next fptr
	br.cond.sptk.few 7b

8:					// allocate new fptr
	mov	r8=1			// failure return value
	cmp.geu	p6,p0=r2,r3		// space left?
(p6)	br.cond.dpnt.few 9f		// bail out

	st8	[r15]=r2		// install fptr
	st8	[r2]=r16,8		// write fptr address
	;;
	st8	[r2]=gp,8		// write fptr gp
	br.cond.sptk.few 3b

9:
	mov	ar.pfs=loc0
	mov	rp=loc1
	;;
	br.ret.sptk.few rp

END(_reloc)

	.data
	.align	16
	.global fptr_storage
fptr_storage:
	.space	4096*16			// XXX
fptr_storage_end:
