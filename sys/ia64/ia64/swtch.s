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
 * savectx: save process context, i.e. callee-saved registers
 *
 * Arguments:
 *	in0	'struct pcb *' of the process that needs its context saved
 *
 * Return:
 *	ret0	0.  (note that for child processes, it seems
 *		like savectx() returns 1, because the return address
 *		in the PCB is set to the return address from savectx().)
 */

ENTRY(savectx, 1)
	flushrs				// push out caller's dirty regs
	mov	r3=ar.unat		// caller's value for ar.unat
	;;
	mov	ar.rsc=0		// stop the RSE after the flush
	;;
	mov	r16=ar.rnat		// read RSE's NaT collection
	mov	r17=in0
	mov	r18=ar.bspstore
	mov	r19=b0
	mov	r20=b1
	mov	r21=b2
	mov	r22=b3
	mov	r23=b4
	mov	r24=b5
	;;	
	st8.spill [r17]=r4,8 ;;		// save r4..r6 
	st8.spill [r17]=r5,8 ;;		// and accumulate NaT bits
	st8.spill [r17]=r6,8 ;;
	st8.spill [r17]=r7,8 ;;
	
	stf.spill [r17]=f2,16 ;;	// save f2..f5 with NaTVals
	stf.spill [r17]=f3,16 ;;
	stf.spill [r17]=f4,16 ;;
	stf.spill [r17]=f5,16 ;;

	st8	[r17]=r19,8 ;;		// save b0..b5
	st8	[r17]=r20,8 ;;
	st8	[r17]=r21,8 ;;
	st8	[r17]=r22,8 ;;
	st8	[r17]=r23,8 ;;
	st8	[r17]=r24,8 ;;
	
	mov	r19=ar.unat		// NaT bits for r4..r6
	mov	r20=pr
	mov	ret0=r0			// return zero

	st8	[r17]=r3,8 ;;		// save caller's ar.unat
	st8	[r17]=sp,8 ;;		// stack pointer
	st8	[r17]=r2,8 ;;		// ar.pfs
	st8	[r17]=r18,8 ;;		// ar.bspstore
	st8	[r17]=r19,8 ;;		// our NaT bits
	st8	[r17]=r16,8 ;;		// ar.rnat
	st8	[r17]=r20,8 ;;		// pr

	mov	ar.rsc=3		// turn RSE back on

	br.ret.sptk.few rp
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

	add	r3=PCB_UNAT,in0		// point at NaT for r4..r7
	mov	ar.rsc=0 ;;		// switch off the RSE
	ld8	r16=[r3]		// load NaT for r4..r7
	;;
	mov	ar.unat=r16
	;;
	ld8.fill r4=[in0],8 ;;		// restore r4
	ld8.fill r5=[in0],8 ;;		// restore r5
	ld8.fill r6=[in0],8 ;;		// restore r6
	ld8.fill r7=[in0],8 ;;		// restore r7

	ldf.fill f2=[in0],16 ;;		// restore f2
	ldf.fill f3=[in0],16 ;;		// restore f3
	ldf.fill f4=[in0],16 ;;		// restore f4
	ldf.fill f5=[in0],16 ;;		// restore f5

	ld8	r16=[in0],8 ;;		// restore b0
	ld8	r17=[in0],8 ;;		// restore b1
	ld8	r18=[in0],8 ;;		// restore b2
	ld8	r19=[in0],8 ;;		// restore b3
	ld8	r20=[in0],8 ;;		// restore b4
	ld8	r21=[in0],8 ;;		// restore b5

	mov	b0=r16
	mov	b1=r17
	mov	b2=r18
	mov	b3=r19
	mov	b4=r20
	mov	b5=r21

	ld8	r16=[in0],8 ;;		// caller's ar.unat
	ld8	sp=[in0],8 ;;		// stack pointer
	ld8	r17=[in0],8 ;;		// ar.pfs
	ld8	r18=[in0],16 ;;		// ar.bspstore, skip ar.unat
	ld8	r19=[in0],8 ;;		// ar.rnat
	ld8	r20=[in0],8 ;;		// pr

	mov	ar.unat=r16
	mov	ar.pfs=r17
	mov	ar.bspstore=r18 ;;
	mov	ar.rnat=r19
	mov	pr=r20,0x1ffff
	mov	ret0=r18		// non-zero return
	;;
	loadrs
	mov	ar.rsc=3		// restart RSE
	invala
	;;
	br.ret.sptk.few rp
	END(restorectx)	

ENTRY(cpu_switch, 0)

	add	r16=PC_CURTHREAD,r13 ;;
	ld8	r17=[r16] ;; 
	add	r17=TD_PCB,r17 ;;
	ld8	r17=[r17]

	flushrs				// push out caller's dirty regs
	mov	r2=ar.pfs
	mov	r3=ar.unat		// caller's value for ar.unat
	;;
	mov	ar.rsc=0		// stop the RSE after the flush
	;;
	mov	r16=ar.rnat		// read RSE's NaT collection
	mov	r18=ar.bspstore
	mov	r19=b0
	mov	r20=b1
	mov	r21=b2
	mov	r22=b3
	mov	r23=b4
	mov	r24=b5
	;;	
	st8.spill [r17]=r4,8 ;;		// save r4..r6 
	st8.spill [r17]=r5,8 ;;		// and accumulate NaT bits
	st8.spill [r17]=r6,8 ;;
	st8.spill [r17]=r7,8 ;;
	
	stf.spill [r17]=f2,16 ;;	// save f2..f5 with NaTVals
	stf.spill [r17]=f3,16 ;;
	stf.spill [r17]=f4,16 ;;
	stf.spill [r17]=f5,16 ;;

	st8	[r17]=r19,8 ;;		// save b0..b5
	st8	[r17]=r20,8 ;;
	st8	[r17]=r21,8 ;;
	st8	[r17]=r22,8 ;;
	st8	[r17]=r23,8 ;;
	st8	[r17]=r24,8 ;;
	
	mov	r19=ar.unat		// NaT bits for r4..r6
	mov	r20=pr
	mov	ret0=r0			// return zero

	st8	[r17]=r3,8 ;;		// save caller's ar.unat
	st8	[r17]=sp,8 ;;		// stack pointer
	st8	[r17]=r2,8 ;;		// ar.pfs
	st8	[r17]=r18,8 ;;		// ar.bspstore
	st8	[r17]=r19,8 ;;		// our NaT bits
	st8	[r17]=r16,8 ;;		// ar.rnat
	st8	[r17]=r20 ;;		// pr

	mov	ar.rsc=3		// turn RSE back on

XENTRY(cpu_throw)

#ifdef SMP
	add	r17 = PC_CPUID, r13
	movl	r16 = smp_active
	;;
	ld4	r16 = [r16]
	ld4	r17 = [r17]
	;;
	cmp.ne	p1, p0 = 0, r16
(p1)	br.dptk	1f
	;;
	cmp.eq	p1, p0 = 0, r17
(p1)	br.dptk	1f
	;;
	add	r16 = PC_IDLETHREAD, r13
	;;
	ld8	ret0 = [r16]
	br.sptk	2f
1:
#endif
	br.call.sptk.few rp=choosethread

2:
	add	r14=PC_CURTHREAD,r13 ;;

#if 0
	ld8	r15 = [r14]
	;;
	cmp.ne	p6,p0=r15,ret0		// chooseproc() == curthread ?
(p6)	br.dptk.few 1f
	;;
	add	r17=TD_PCB,r15 ;;	// restore b0
	ld8	r17=[r17] ;;
	add	r17=PCB_B0,r17 ;;
	ld8	r17=[r17] ;;
	mov	b0=r17

	br.sptk.few 9f			// don't bother to restore
#endif

1:
	st8	[r14]=ret0		// set r13->pc_curthread
	mov	ar.k7=ret0
	mov	r4=ret0			// save from call
	;; 
	alloc	r15=ar.pfs,0,0,1,0	// create temporary output frame
	;;
	mov	out0=r4
	br.call.sptk.few rp=pmap_activate // install RIDs etc.

	add	r15=TD_PCB,r4
	add	r16=TD_KSTACK,r4 ;;
	ld8	r15=[r15]
	ld8	r16=[r16] ;;
	mov	ar.k5=r16

	add	r3=PCB_UNAT,r15		// point at NaT for r4..r7
	mov	ar.rsc=0 ;;		// switch off the RSE
	ld8	r16=[r3]		// load NaT for r4..r7
	;;
	mov	ar.unat=r16
	;;
	ld8.fill r4=[r15],8 ;;		// restore r4
	ld8.fill r5=[r15],8 ;;		// restore r5
	ld8.fill r6=[r15],8 ;;		// restore r6
	ld8.fill r7=[r15],8 ;;		// restore r7

	ldf.fill f2=[r15],16 ;;		// restore f2
	ldf.fill f3=[r15],16 ;;		// restore f3
	ldf.fill f4=[r15],16 ;;		// restore f4
	ldf.fill f5=[r15],16 ;;		// restore f5

	ld8	r16=[r15],8 ;;		// restore b0
	ld8	r17=[r15],8 ;;		// restore b1
	ld8	r18=[r15],8 ;;		// restore b2
	ld8	r19=[r15],8 ;;		// restore b3
	ld8	r20=[r15],8 ;;		// restore b4
	ld8	r21=[r15],8 ;;		// restore b5

	mov	b0=r16
	mov	b1=r17
	mov	b2=r18
	mov	b3=r19
	mov	b4=r20
	mov	b5=r21

	ld8	r16=[r15],8 ;;		// caller's ar.unat
	ld8	sp=[r15],8 ;;		// stack pointer
	ld8	r17=[r15],8 ;;		// ar.pfs
	ld8	r18=[r15],16 ;;		// ar.bspstore, skip ar.unat
	ld8	r19=[r15],8 ;;		// ar.rnat
	ld8	r20=[r15] ;;		// pr

	mov	ar.unat=r16
	mov	ar.pfs=r17
	mov	ar.bspstore=r18 ;;
	mov	ar.rnat=r19
	mov	pr=r20,0x1ffff
	alloc	r14=ar.pfs,0,0,0,0	// lose current frame
	;;
	loadrs
	mov	ar.rsc=3		// restart RSE
	invala
	;;
9:
	br.ret.sptk.few rp

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

	END(fork_trampoline)
