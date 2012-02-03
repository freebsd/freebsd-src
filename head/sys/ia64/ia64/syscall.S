/*-
 * Copyright (c) 2002, 2003 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *
 * $FreeBSD$
 */

#include <sys/syscall.h>
#include <machine/asm.h>
#include <assym.s>

/*
 * A process performs a syscall by performing an indirect call to the
 * address stored in ar.k5. The contents of ar.pfs and rp should be
 * saved prior to the syscall in r9 and r10 respectively. The kernel
 * will restore these values on return. The value of gp is preserved
 * across the call. This allows for small enough syscall stubs without
 * getting too weird.
 * The address in ar.k5 is the start of the EPC gateway page and also
 * the syscall entry point. The syscall code in the gateway page is
 * primarily responsible for increasing the privilege level, but will
 * also make sure we have a reliable psr.
 *
 * A process defines:
 *	r8		-	syscall number
 *	r9		-	copy of ar.pfs
 *	r10		-	copy of rp
 *	in0-in7		-	syscall arguments
 *
 * A syscall returns:
 *	r8+r9		-	syscall return value(s)
 *	r10		-	syscall error flag
 *	ar.pfs		-	restored from r9
 *	rp		-	restored from r10
 *	gp		-	preserved
 *
 * The EPC syscall code defines:
 *	r11		-	copy of psr.l
 *	r14		-	Kernel memory stack
 *	r15		-	Kernel register stack
 *
 * Also in the gateway page are the signal trampolines. As such, stacks
 * don't have to be made executable per se. Since debuggers have a need
 * to know about trampolines, we probably need to define a table of
 * vectors or something along those lines so that debuggers can get the
 * information they need and we have the freedom to move code around.
 */

	.section	.text.gateway, "ax"
	.align		PAGE_SIZE
	.global		ia64_gateway_page
ia64_gateway_page:
{	.mmb
	mov		r14=ar.k7		// Memory stack
	mov		r15=ar.k6		// Register stack
	epc
	;;
}
{	.mlx
	mov		r11=psr
	movl		r31=epc_syscall
	;;
}
{	.mib
	rum		psr.be
	mov		b7=r31
	br		b7
	;;
}
gw_ret:
{	.mmi
	mov		ar.rnat=r22
	;;
	mov		ar.rsc=r24
	mov		ar.pfs=r20
}
{	.mib
	mov		ar.fpsr=r25
	mov		b0=r18
	br.sptk		b6
	;;
}
gw_ret_ia32:
{	.mmi
	flushrs
	nop		0
	nop		0
	;;
}
{	.mib
	nop		0
	nop		0
	br.ia.sptk	b6
	;;
}


ENTRY_NOPROFILE(break_sigtramp, 0)
{	.mib
	mov		ar.rsc=0
	cmp.ne		p15,p0=0,gp
	cover
	;;
}
{	.mmi
	flushrs
(p15)	invala
	add		r16=16+UC_MCONTEXT+MC_SPECIAL,sp
	;;
}
{	.mmi
	mov		r17=ar.bsp
	mov		r18=ar.rnat
	add		r14=40,r16
	;;
}
{	.mmi
	st8		[r14]=r17,64		// bspstore
(p15)	mov		ar.bspstore=gp
	add		r15=48,r16
	;;
}
{	.mmi
	st8		[r15]=r18		// rnat
	st8		[r14]=r0		// ndirty
	nop		0
	;;
}
{	.mmi
	alloc		r14=ar.pfs, 0, 0, 3, 0
	mov		ar.rsc=15
	mov		out0=r8
	;;
}
{	.mmi
	ld8		r16=[r10],8		// function address
	;;
	ld8		gp=[r10]		// function's gp value
	mov		b7=r16
	;;
}
{	.mib
	mov		out1=r9
	add		out2=16,sp
	br.call.sptk	rp=b7
	;;
}
{	.mmi
	mov		r15=SYS_sigreturn
	add		out0=16,sp
	break		0x100000
	;;
}
{	.mmi
	mov		r15=SYS_exit
	mov		out0=ret0
	break		0x100000
	;;
}
END(break_sigtramp)

ENTRY_NOPROFILE(epc_sigtramp, 0)
{	.mmi
	ld8		r16=[r10],8		// function address
	mov		ar.rsc=0
	cmp.ne		p15,p0=0,gp
	;;
}
{	.mmi
(p15)	invala
(p15)	mov		ar.bspstore=gp
	mov		b7=r16
	;;
}
{	.mmi
	alloc		r14=ar.pfs, 0, 0, 3, 0
	mov		ar.rsc=15
	nop		0
	;;
}
{	.mii
	ld8		gp=[r10]		// function's gp value
	mov		out0=r8
	mov		out1=r9
}
{	.mib
	add		out2=16,sp
	nop		0
	br.call.sptk	rp=b7
	;;
}
	add		out0=16,sp
	CALLSYS_NOERROR(sigreturn)
	mov		out0=ret0
	CALLSYS_NOERROR(exit)
END(epc_sigtramp)

	.align		PAGE_SIZE

	.text

ENTRY_NOPROFILE(epc_syscall, 8)
	.prologue
	.unwabi		@svr4, 'E'
	.save		rp, r0
	.body
{	.mmi
	mov		r16=ar.rsc
	mov		ar.rsc=0
	mov		r17=r13
	;;
}
{	.mmi
	mov		r18=ar.bspstore
	;;
	mov		r19=ar.rnat
	dep		r15=r18,r15,0,9
	;;
}
{	.mmi
	mov		r13=ar.k4
	add		r30=-SIZEOF_TRAPFRAME,r14
	mov		r20=sp
	;;
}
{	.mii
	mov		r21=ar.unat
	dep		r30=0,r30,0,10
	;;
	add		sp=-16,r30
	;;
}
{	.mmi
	mov		ar.bspstore=r15
	;;
	mov		ar.rnat=r19
	add		r31=8,r30
	;;
}
{	.mii
	mov		r22=ar.fpsr
	sub		r29=r14,r30
	nop		0
}
{	.mmi
	mov		r23=ar.bsp
	mov		ar.rsc=3
	add		r28=FRAME_SYSCALL,r0
	;;
}
{	.mmi
	st8		[r30]=r29,16		// tf_length
	st8		[r31]=r28,16		// tf_flags
	mov		r24=rp
	;;
}
{	.mmi
	st8		[r30]=r20,16		// sp
	st8		[r31]=r21,16		// unat
	mov		r25=pr
	;;
}
{	.mmi
	st8		[r30]=r10,16		// rp (syscall caller)
	st8		[r31]=r25,16		// pr
	mov		r26=ar.pfs
	;;
}
{	.mmi
	st8		[r30]=r9,16		// pfs (syscall caller)
	st8		[r31]=r18,16		// bspstore
	sub		r27=r23,r15
	;;
}
{	.mmi
	st8		[r30]=r19,16		// rnat
	st8		[r31]=r0,16		// __spare
	nop		0
	;;
}
{	.mmi
	st8		[r30]=r17,16		// tp
	st8		[r31]=r16,16		// rsc
	dep		r11=-1,r11,32,2		// Set psr.cpl=3
	;;
}
{	.mmi
	st8		[r30]=r22,16		// fpsr
	st8		[r31]=r11,16		// psr
	nop		0
	;;
}
{	.mmi
	st8		[r30]=r1,16		// gp
	st8		[r31]=r27,16		// ndirty
	nop		0
	;;
}
{	.mmi
	st8		[r30]=r26,16		// pfs (syscall stub)
	st8		[r31]=r24,16		// rp (syscall stub)
	nop		0
	;;
}
{	.mmi
	st8		[r30]=r0,80		// ifa
	st8		[r31]=r0,80		// isr
	nop		0
	;;
}
{	.mmi
	alloc		r14=ar.pfs,0,0,8,0
	st8		[r30]=r8,16		// syscall number (=r15)
	nop		0
	;;
}
{	.mmi
	.mem.offset	0,0
	st8.spill	[r31]=r32,16		// arg0 (=r16)
	.mem.offset	8,0
	st8.spill	[r30]=r33,16		// arg1 (=r17)
	nop		0
	;;
}
{	.mmi
	.mem.offset	16,0
	st8.spill	[r31]=r34,16		// arg2 (=r18)
	.mem.offset	24,0
	st8.spill	[r30]=r35,16		// arg3 (=r19)
	nop		0
	;;
}
{	.mmi
	.mem.offset	32,0
	st8.spill	[r31]=r36,16		// arg4 (=r20)
	.mem.offset	40,0
	st8.spill	[r30]=r37,16		// arg5 (=r21)
	nop		0
	;;
}
{	.mmi
	.mem.offset	48,0
	st8.spill	[r31]=r38		// arg6 (=r22)
	.mem.offset	56,0
	st8.spill	[r30]=r39		// arg7 (=r23)
	nop		0
	;;
}
{	.mlx
	ssm		psr.dfh|psr.ac
	movl		gp=__gp
	;;
}
1:
{	.mib
	srlz.d
	add		out0=16,sp
	br.call.sptk	rp=syscall
	;;
}
	.global		epc_syscall_return
epc_syscall_return:
{	.mib
	add		out0=16,sp
	nop		0
	br.call.sptk	rp=do_ast
	;;
}
{	.mib
	cmp4.eq		p15,p0=ERESTART,r8
	add		r14=24,sp
(p15)	br.spnt		1b			// restart syscall
	;;
}
{	.mmi
	ld8		r14=[r14]		// tf_flags
	nop		0
	nop		0
	;;
}
{	.mib
	nop		0
	tbit.z		p15,p0=r14,0
(p15)	br.spnt		exception_restore
	;;
}
{	.mmi
	alloc		r31=ar.pfs,0,0,0,0
	add		r14=32,sp
	add		r15=16,sp
	;;
}
{	.mmi
	ld8		r31=[r15],24		// tf_length
	ld8		r16=[r14],16		// sp
	add		sp=16,sp
	;;
}
{	.mmi
	ld8		r17=[r15],16		// unat (before)
	ld8		r18=[r14],16		// rp (syscall caller)
	add		r31=r31,sp
	;;
}
{	.mmi
	ld8		r19=[r15],16		// pr
	ld8		r20=[r14],16		// pfs (syscall caller)
	nop		0
	;;
}
{	.mmi
	ld8		r21=[r15],24		// bspstore
	ld8		r22=[r14],24		// rnat
	mov		pr=r19,0x1fffe
	;;
}
{	.mmi
	ld8		r23=[r15],16		// tp
	ld8		r24=[r14],16		// rsc
	nop		0
	;;
}
{	.mmi
	ld8		r25=[r15],16		// fpsr
	ld8		r26=[r14],16		// psr
	nop		0
	;;
}
{	.mmi
	ld8		gp=[r15],16		// gp
	ld8		r27=[r14],16		// ndirty
	tbit.z		p14,p15=r26,34		// p14=ia64, p15=ia32
	;;
}
{	.mmi
	ld8		r28=[r15],56		// pfs (syscall stub)
	ld8		r29=[r14],56		// rp (syscall stub)
	shl		r27=r27,16
	;;
}
{	.mmi
	ld8		r8=[r15],16		// r8
	mov		ar.rsc=r27
	mov		b6=r29
	;;
}
{	.mmb
	ld8		r9=[r14],40		// r9
	ld8		r10=[r15],40		// r10
(p15)	br.spnt		epc_syscall_setup_ia32
	;;
}
{	.mmi
	loadrs
	mov		ar.k7=r31
	mov		sp=r16
	;;
}
{	.mmi
	mov		r30=ar.bspstore
	mov		r14=ar.k5
	mov		ar.pfs=r28
	;;
}
{	.mmi
	mov		ar.bspstore=r21
	add		r14=gw_ret-ia64_gateway_page,r14
	dep		r30=0,r30,0,13		// 8KB aligned.
	;;
}
{	.mii
	mov		ar.k6=r30
	mov		r13=r23
	nop		0
}
{	.mmi
	mov		psr.l=r26
	mov		ar.unat=r17
	nop		0
	;;
}
{	.mib
	srlz.d
	mov		b7=r14
	br.ret.sptk	b7
	;;
}
epc_syscall_setup_ia32:
{	.mmi
	loadrs
	mov		ar.k7=r31
	mov		sp=r16
	;;
}
{	.mmi
	mov		r30=ar.bspstore
	;;
	mov		ar.unat=r17
	dep		r30=0,r30,0,13		// 8KB aligned
	;;
}
{	.mmi
	mov		ar.k6=r30
	mov		ar.bspstore=r21
	mov		r11=r0
	;;
}
{	.mmi
	ld8		r16=[r14],64
	ld8		r17=[r15],80
	mov		r13=r0
	;;
}
 
	ld8		r24=[r14],32
	ld8		r27=[r15],16
	;;
	ld8		r28=[r14],16
	ld8		r29=[r15],16
	;;
	ld8		r30=[r14],40
	ld8		r31=[r15],40
	;;

{	.mmi
	ld8		r2=[r14]
	ld8		r3=[r15]
	mov		r14=r0
	;;
}
{	.mmi
	mov		ar.csd=r2
	mov		ar.ssd=r3
	mov		r15=r0
	;;
}

	mov		r2=ar.k5
	mov		psr.l=r26
	;;
	srlz.d
	add		r2=gw_ret_ia32-ia64_gateway_page,r2
	;;
	mov		ar.rsc=0
	mov		b7=r2
	br.ret.sptk	b7
	;;
END(epc_syscall)
