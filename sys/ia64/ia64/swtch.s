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
#include <machine/mutex.h>
#include "assym.s"

/*
 * pcb_save - save process context, i.e. callee-saved registers. This
 * function is called by savectx() and cpu_switch().
 *
 * The callee-saved (ie preserved) registers are:
 *	r4-r7
 *	f2-f5, f16-f31
 *	p1-p5, p16-p63
 *	b1-b5
 *	ar.unat, ar.lc
 *
 * Notes:
 * o  We don't need to save and restore gp because its value is constant.
 * o  We don't need to save and restore ar.rsc because its value is known.
 * o  We don't need to save and restore r13 because it always points to
 *    the pcpu structure.
 *
 * Arguments:
 *	r8	'struct pcb *' of the process that needs its context saved.
 *	b6	return address.
 */
ENTRY(pcb_save,0)
{	.mii
	mov	r16=ar.unat
	add	r9=8,r8
	mov	r17=b0
	;;
}
{	.mmi
	st8	[r8]=r12,16		// sp
	st8	[r9]=r16,16		// ar.unat
	mov	r18=pr
	;;
}
{	.mmi
	st8	[r8]=r17,16		// rp
	st8	[r9]=r18,24		// pr
	add	r31=PC_CURRENT_PMAP,r13
	;;
}
	stf.spill [r8]=f2,32
	stf.spill [r9]=f3,32
	;;
	stf.spill [r8]=f4,32
	stf.spill [r9]=f5,32
	;;
	stf.spill [r8]=f16,32
	stf.spill [r9]=f17,32
	;;
	stf.spill [r8]=f18,32
	stf.spill [r9]=f19,32
	;;
	stf.spill [r8]=f20,32
	stf.spill [r9]=f21,32
	;;
	stf.spill [r8]=f22,32
	stf.spill [r9]=f23,32
	;;
	stf.spill [r8]=f24,32
	stf.spill [r9]=f25,32
	;;
	stf.spill [r8]=f26,32
	stf.spill [r9]=f27,32
	;;
	stf.spill [r8]=f28,32
	stf.spill [r9]=f29,32
	;;
	stf.spill [r8]=f30,32
	stf.spill [r9]=f31,24		// BEWARE
	;;
{	.mmi
	.mem.offset 0,0
	st8.spill [r8]=r4,16
	.mem.offset 8,0
	st8.spill [r9]=r5,16
	mov	r17=b1
	;;
}
{	.mmi
	.mem.offset 16,0
	st8.spill [r8]=r6,16
	.mem.offset 24,0
	st8.spill [r9]=r7,16
	mov	r18=b2
	;;
}
{	.mmi
	flushrs
	mov	r16=ar.unat
	mov	r19=b3
	;;
}
{	.mmi
	st8	[r8]=r16,16		// ar.unat
	st8	[r9]=r17,16		// b1
	mov	r20=b4
	;;
}
{	.mmi
	st8	[r8]=r18,16		// b2
	st8	[r9]=r19,16		// b3
	mov	r21=b5
	;;
}
	st8	[r8]=r20,16		// b4
	st8	[r9]=r21,16		// b5
	;;
{	.mmi
	mov	ar.rsc=0
	mov	r16=ar.bsp
	mov	r17=ar.pfs
	;;
}
{	.mmi
	st8	[r8]=r16,16		// ar.bsp
	st8	[r9]=r17,16		// ar.pfs
	mov	r18=ar.lc
	;;
}
	mov	r17=ar.rnat
	ld8	r19=[r31]
	;;
	st8	[r8]=r17,16		// ar.rnat
	st8	[r9]=r18,16		// ar.lc
	;;
{	.mmb
	st8	[r8]=r19,16		// pc_current_pmap
#ifdef IA32
	mov	r16=ar.fcr
	;;
}
	st8	[r9]=r16,16		// ar.fcr
	mov	r17=ar.eflag
	;;
	st8	[r8]=r17,16		// ar.eflag
	mov	r16=ar.csd
	;;
	st8	[r9]=r16,16		// ar.csd
	mov	r17=ar.ssd
	;;
	st8	[r8]=r17,16		// ar.ssd
	mov	r16=ar.fsr
	;;
	st8	[r9]=r16,16		// ar.fsr
	mov	r17=ar.fir
	;;
	st8	[r8]=r17,16		// ar.fir
	mov	r16=ar.fdr
	;;
{	.mmb
	st8	[r9]=r16,16		// ar.fdr
#endif
	mov	ar.rsc=3
	br.sptk.many b6
	;;
}
END(pcb_save)

/*
 * pcb_restore - restore a process context. This does not return.
 *
 * Arguments:
 *	r8	'struct pcb *' of the process that needs its context saved.
 */
ENTRY(pcb_restore,0)
	mov	ar.rsc=0
	add	r9=8,r8
	add	r31=PCB_UNAT47,r8
	;;
	ld8	r12=[r8],16		// sp
	ld8	r30=[r9],16		// ar.unat
	;;
	ld8	r16=[r8],16		// rp
	ld8	r17=[r9],24		// pr
	;;
	ldf.fill f2=[r8],32
	ldf.fill f3=[r9],32
	mov	b0=r16
	;;
	ldf.fill f4=[r8],32
	ldf.fill f5=[r9],32
	mov	pr=r17,0x1fffe
	;;
	ldf.fill f16=[r8],32
	ldf.fill f17=[r9],32
	;;
	ldf.fill f18=[r8],32
	ldf.fill f19=[r9],32
	;;
	ldf.fill f20=[r8],32
	ldf.fill f21=[r9],32
	;;
	ldf.fill f22=[r8],32
	ldf.fill f23=[r9],32
	;;
	ldf.fill f24=[r8],32
	ldf.fill f25=[r9],32
	;;
	ldf.fill f26=[r8],32
	ldf.fill f27=[r9],32
	;;
	ldf.fill f28=[r8],32
	ldf.fill f29=[r9],32
	;;
	ldf.fill f30=[r8],32
	ldf.fill f31=[r9],24		// BEWARE
	;;
	ld8	r16=[r31]
	;;
	mov	ar.unat=r16
	;;
	.mem.offset 0,0
	ld8.fill r4=[r8],16
	.mem.offset 8,0
	ld8.fill r5=[r9],16
	;;
	.mem.offset 16,0
	ld8.fill r6=[r8],24
	.mem.offset 24,0
	ld8.fill r7=[r9],24
	;;
	loadrs
	mov	ar.unat=r30
	;;
	ld8	r16=[r8],16		// b1
	ld8	r17=[r9],16		// b2
	;;
	ld8	r18=[r8],16		// b3
	ld8	r19=[r9],16		// b4
	mov	b1=r16
	;;
	ld8	r20=[r8],16		// b5
	ld8	r21=[r9],16		// ar.bsp
	mov	b2=r17
	;;
	ld8	r22=[r8],16		// ar.pfs
	ld8	r23=[r9],24		// ar.rnat
	mov	b3=r18
	;;
	ld8	r24=[r8],24		// ar.lc
	mov	b4=r19
	mov	b5=r20
	;;
{	.mmi
	mov	ar.bspstore=r21
	;;
	mov	ar.rnat=r23
	mov	ar.pfs=r22
	;;
}
{	.mmi
	invala
	;;
#ifdef IA32
	ld8	r16=[r9],16		// ar.fcr
#endif
	mov	r18=ar.lc
	;;
}
#ifdef IA32
	ld8	r17=[r8],16		// ar.eflag
	mov	ar.fcr=r16
	;;
	ld8	r16=[r9],16		// ar.csd
	mov	ar.eflag=r17
	;;
	ld8	r17=[r8],16		// ar.ssd
	mov	ar.csd=r16
	;;
	ld8	r16=[r9],16		// ar.fsr
	mov	ar.ssd=r17
	;;
	ld8	r17=[r8],16		// ar.fir
	mov	ar.fsr=r16
	;;
	ld8	r16=[r9],16		// ar.fdr
	mov	ar.fir=r17
	;;
#endif
{	.mmb
#ifdef IA32
	mov	ar.fdr=r16
#endif
	mov	ar.rsc=3
	br.ret.sptk b0
	;;
}
END(pcb_restore)

/*
 * savectx - Save current context.
 */
ENTRY(savectx,1)
{	.mii
1:	nop	1
	mov	r17=ip
	;;
	add	r16=2f-1b,r17
	;;
}
{	.mib
	mov	r8=r32
	mov	b6=r16
	br.sptk.many pcb_save
}
{	.mfb
2:	nop	2
	nop	3
	br.ret.sptk rp
}
END(savectx)

/*
 * restorectx: restore process context, i.e. callee-saved registers
 *
 * Arguments:
 *	in0	'struct pcb *' of the process being restored
 *
 * Return:
 *	Does not return. We arrange things so that savectx appears to
 *	return a second time with a non-zero return value.
 */
ENTRY(restorectx, 1)
	mov	r8=in0
	br.cond.sptk.many pcb_restore
	;;
END(restorectx)	

ENTRY(cpu_throw, 0)
	br.sptk.many 2f
	;;
END(cpu_throw)

ENTRY(cpu_switch, 0)
{	.mmi
1:	alloc	r16=ar.pfs,0,0,2,0
	mov	r17=ip
	;;
}
	add	r16=1f-1b,r17
	add	r17=PC_CURTHREAD,r13
	;;
	ld8	r17=[r17]
	;;
	add	r17=TD_PCB,r17
	mov	b6=r16
	;;
	ld8	r8=[r17]
	br.sptk.many pcb_save
	;;
1:
#ifdef SMP
	add	r17 = PC_CPUID, r13
	movl	r16 = smp_active
	;;
	ld4	r16 = [r16]
	ld4	r17 = [r17]
	;;
	cmp.ne	p1, p0 = 0, r16
(p1)	br.dptk	2f
	;;
	cmp.eq	p1, p0 = 0, r17
(p1)	br.dptk	2f
	;;
	add	r16 = PC_IDLETHREAD, r13
	;;
	ld8	r4 = [r16]
	br.sptk	3f
#endif

2:	br.call.sptk.few rp=choosethread
	;;
	mov	r4=ret0			// save from call

3:
#ifdef SMP
	add	r14=PC_CURTHREAD,r13
	;;
	ld8	out0=[r14]
	mov	out1=1
	br.call.sptk.few rp=ia64_fpstate_save // clear fpcurthread
	;;
#endif

	add	r14=PC_CURTHREAD,r13
	;;
	st8	[r14]=r4		// set r13->pc_curthread
	mov	ar.k7=r4
	;;
	add	r15=TD_PCB,r4
	;;
	ld8	r15=[r15]
	;;
	add	r15=PCB_CURRENT_PMAP,r15 // &pcb_pmap
	;;
	ld8	out0=[r15]
	br.call.sptk.few rp=pmap_install // install RIDs etc.
	;;
	add	r15=TD_PCB,r4
	add	r16=TD_KSTACK,r4
	;;
	ld8	r8=[r15]
	ld8	r16=[r16]
	;;
	mov	ar.k5=r16
	br.cond.sptk pcb_restore
	;;
END(cpu_switch)

/*
 * savehighfp: Save f32-f127
 *
 * Arguments:
 *	in0	array of struct ia64_fpreg
 */
ENTRY(savehighfp, 1)

	add	r14=16,in0
	;;
	stf.spill [in0]=f32,32
	stf.spill [r14]=f33,32
	;; 
	stf.spill [in0]=f34,32
	stf.spill [r14]=f35,32
	;; 
	stf.spill [in0]=f36,32
	stf.spill [r14]=f37,32
	;;
	stf.spill [in0]=f38,32
	stf.spill [r14]=f39,32
	;;
	stf.spill [in0]=f40,32
	stf.spill [r14]=f41,32
	;;
	stf.spill [in0]=f42,32
	stf.spill [r14]=f43,32
	;; 
	stf.spill [in0]=f44,32
	stf.spill [r14]=f45,32
	;; 
	stf.spill [in0]=f46,32
	stf.spill [r14]=f47,32
	;; 
	stf.spill [in0]=f48,32
	stf.spill [r14]=f49,32
	;; 
	stf.spill [in0]=f50,32
	stf.spill [r14]=f51,32
	;; 
	stf.spill [in0]=f52,32
	stf.spill [r14]=f53,32
	;; 
	stf.spill [in0]=f54,32
	stf.spill [r14]=f55,32
	;; 
	stf.spill [in0]=f56,32
	stf.spill [r14]=f57,32
	;; 
	stf.spill [in0]=f58,32
	stf.spill [r14]=f59,32
	;; 
	stf.spill [in0]=f60,32
	stf.spill [r14]=f61,32
	;; 
	stf.spill [in0]=f62,32
	stf.spill [r14]=f63,32
	;; 
	stf.spill [in0]=f64,32
	stf.spill [r14]=f65,32
	;; 
	stf.spill [in0]=f66,32
	stf.spill [r14]=f67,32
	;; 
	stf.spill [in0]=f68,32
	stf.spill [r14]=f69,32
	;; 
	stf.spill [in0]=f70,32
	stf.spill [r14]=f71,32
	;; 
	stf.spill [in0]=f72,32
	stf.spill [r14]=f73,32
	;; 
	stf.spill [in0]=f74,32
	stf.spill [r14]=f75,32
	;; 
	stf.spill [in0]=f76,32
	stf.spill [r14]=f77,32
	;; 
	stf.spill [in0]=f78,32
	stf.spill [r14]=f79,32
	;; 
	stf.spill [in0]=f80,32
	stf.spill [r14]=f81,32
	;; 
	stf.spill [in0]=f82,32
	stf.spill [r14]=f83,32
	;; 
	stf.spill [in0]=f84,32
	stf.spill [r14]=f85,32
	;; 
	stf.spill [in0]=f86,32
	stf.spill [r14]=f87,32
	;; 
	stf.spill [in0]=f88,32
	stf.spill [r14]=f89,32
	;; 
	stf.spill [in0]=f90,32
	stf.spill [r14]=f91,32
	;; 
	stf.spill [in0]=f92,32
	stf.spill [r14]=f93,32
	;; 
	stf.spill [in0]=f94,32
	stf.spill [r14]=f95,32
	;; 
	stf.spill [in0]=f96,32
	stf.spill [r14]=f97,32
	;; 
	stf.spill [in0]=f98,32
	stf.spill [r14]=f99,32
	;; 
	stf.spill [in0]=f100,32
	stf.spill [r14]=f101,32
	;; 
	stf.spill [in0]=f102,32
	stf.spill [r14]=f103,32
	;; 
	stf.spill [in0]=f104,32
	stf.spill [r14]=f105,32
	;; 
	stf.spill [in0]=f106,32
	stf.spill [r14]=f107,32
	;; 
	stf.spill [in0]=f108,32
	stf.spill [r14]=f109,32
	;; 
	stf.spill [in0]=f110,32
	stf.spill [r14]=f111,32
	;; 
	stf.spill [in0]=f112,32
	stf.spill [r14]=f113,32
	;; 
	stf.spill [in0]=f114,32
	stf.spill [r14]=f115,32
	;; 
	stf.spill [in0]=f116,32
	stf.spill [r14]=f117,32
	;; 
	stf.spill [in0]=f118,32
	stf.spill [r14]=f119,32
	;; 
	stf.spill [in0]=f120,32
	stf.spill [r14]=f121,32
	;; 
	stf.spill [in0]=f122,32
	stf.spill [r14]=f123,32
	;; 
	stf.spill [in0]=f124,32
	stf.spill [r14]=f125,32
	;; 
	stf.spill [in0]=f126
	stf.spill [r14]=f127
	;; 
	br.ret.sptk.few rp

END(savehighfp)

/*
 * restorehighfp: Restore f32-f127
 *
 * Arguments:
 *	in0	array of struct ia64_fpreg
 */
ENTRY(restorehighfp, 1)
	
	add	r14=16,in0
	;;
	ldf.fill f32=[in0],32
	ldf.fill f33=[r14],32
	;; 
	ldf.fill f34=[in0],32
	ldf.fill f35=[r14],32
	;; 
	ldf.fill f36=[in0],32
	ldf.fill f37=[r14],32
	;; 
	ldf.fill f38=[in0],32
	ldf.fill f39=[r14],32
	;; 
	ldf.fill f40=[in0],32
	ldf.fill f41=[r14],32
	;; 
	ldf.fill f42=[in0],32
	ldf.fill f43=[r14],32
	;; 
	ldf.fill f44=[in0],32
	ldf.fill f45=[r14],32
	;; 
	ldf.fill f46=[in0],32
	ldf.fill f47=[r14],32
	;; 
	ldf.fill f48=[in0],32
	ldf.fill f49=[r14],32
	;; 
	ldf.fill f50=[in0],32
	ldf.fill f51=[r14],32
	;; 
	ldf.fill f52=[in0],32
	ldf.fill f53=[r14],32
	;; 
	ldf.fill f54=[in0],32
	ldf.fill f55=[r14],32
	;; 
	ldf.fill f56=[in0],32
	ldf.fill f57=[r14],32
	;; 
	ldf.fill f58=[in0],32
	ldf.fill f59=[r14],32
	;; 
	ldf.fill f60=[in0],32
	ldf.fill f61=[r14],32
	;; 
	ldf.fill f62=[in0],32
	ldf.fill f63=[r14],32
	;; 
	ldf.fill f64=[in0],32
	ldf.fill f65=[r14],32
	;; 
	ldf.fill f66=[in0],32
	ldf.fill f67=[r14],32
	;; 
	ldf.fill f68=[in0],32
	ldf.fill f69=[r14],32
	;; 
	ldf.fill f70=[in0],32
	ldf.fill f71=[r14],32
	;; 
	ldf.fill f72=[in0],32
	ldf.fill f73=[r14],32
	;; 
	ldf.fill f74=[in0],32
	ldf.fill f75=[r14],32
	;; 
	ldf.fill f76=[in0],32
	ldf.fill f77=[r14],32
	;; 
	ldf.fill f78=[in0],32
	ldf.fill f79=[r14],32
	;; 
	ldf.fill f80=[in0],32
	ldf.fill f81=[r14],32
	;; 
	ldf.fill f82=[in0],32
	ldf.fill f83=[r14],32
	;; 
	ldf.fill f84=[in0],32
	ldf.fill f85=[r14],32
	;; 
	ldf.fill f86=[in0],32
	ldf.fill f87=[r14],32
	;; 
	ldf.fill f88=[in0],32
	ldf.fill f89=[r14],32
	;; 
	ldf.fill f90=[in0],32
	ldf.fill f91=[r14],32
	;; 
	ldf.fill f92=[in0],32
	ldf.fill f93=[r14],32
	;; 
	ldf.fill f94=[in0],32
	ldf.fill f95=[r14],32
	;; 
	ldf.fill f96=[in0],32
	ldf.fill f97=[r14],32
	;; 
	ldf.fill f98=[in0],32
	ldf.fill f99=[r14],32
	;; 
	ldf.fill f100=[in0],32
	ldf.fill f101=[r14],32
	;; 
	ldf.fill f102=[in0],32
	ldf.fill f103=[r14],32
	;; 
	ldf.fill f104=[in0],32
	ldf.fill f105=[r14],32
	;; 
	ldf.fill f106=[in0],32
	ldf.fill f107=[r14],32
	;; 
	ldf.fill f108=[in0],32
	ldf.fill f109=[r14],32
	;; 
	ldf.fill f110=[in0],32
	ldf.fill f111=[r14],32
	;; 
	ldf.fill f112=[in0],32
	ldf.fill f113=[r14],32
	;; 
	ldf.fill f114=[in0],32
	ldf.fill f115=[r14],32
	;; 
	ldf.fill f116=[in0],32
	ldf.fill f117=[r14],32
	;; 
	ldf.fill f118=[in0],32
	ldf.fill f119=[r14],32
	;; 
	ldf.fill f120=[in0],32
	ldf.fill f121=[r14],32
	;; 
	ldf.fill f122=[in0],32
	ldf.fill f123=[r14],32
	;; 
	ldf.fill f124=[in0],32
	ldf.fill f125=[r14],32
	;; 
	ldf.fill f126=[in0]
	ldf.fill f127=[r14]
	;; 
	br.ret.sptk.few rp

END(restorehighfp)

/*
 * fork_trampoline()
 *
 * Arrange for a function to be invoked neatly, after a cpu_switch().
 *
 * Invokes fork_exit() passing in three arguments: a callout function, an
 * argument to the callout, and a trapframe pointer.  For child processes
 * returning from fork(2), the argument is a pointer to the child process.
 *
 * The callout function is in r4, the address to return to after executing
 * fork_exit() is in r5, and the argument is in r6.
 */
ENTRY(fork_trampoline, 0)
	.prologue
	.save	rp,r0
	.body
	alloc	r14=ar.pfs,0,0,3,0
	;;
	mov	b0=r5
	mov	out0=r4
	mov	out1=r6
	add	out2=16,sp
	;;
	br.call.sptk.few rp=fork_exit
	;; 
	br.cond.sptk.many exception_restore
	;;
END(fork_trampoline)
