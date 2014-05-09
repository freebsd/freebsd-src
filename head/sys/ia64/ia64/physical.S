/*-
 * Copyright (c) 2011 Marcel Moolenaar
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

#include <machine/asm.h>
#include <machine/ia64_cpu.h>

		.text

/*
 * u_long ia64_efi_physical(ia64_efi_f, u_long, u_long, u_long, u_long)
 *
 *	loc0 = ar.pfs
 *	loc1 = rp
 *	loc2 = psr
 *	loc3 = sp
 *	loc4 = bsp
 *	loc5 = gp
 */
ENTRY(ia64_efi_physical, 5)
		.prologue
		.regstk 5,6,4,0
		.save	ar.pfs,loc0
		alloc	loc0=ar.pfs,5,6,4,0
		;;
		.save   rp,loc1
		mov	loc1=rp
		;;
		.body
		mov	loc2=psr		// save psr
		movl	r16=IA64_PSR_I | IA64_PSR_IT | IA64_PSR_DT | \
			    IA64_PSR_RT | IA64_PSR_DFL | IA64_PSR_DFH
		;;
		andcm	r14=loc2,r16
		movl	r15=IA64_PSR_BN
		;;
		rsm	psr.i
		mov	r17=ar.rsc
		or	r16=r14,r15		// new psr
		;;
		mov	ar.rsc=0
		or	loc2=loc2,r15
		;;
		flushrs
		mov	loc3=sp			// save sp
		;;
		mov	loc4=ar.bsp		// save ar.bsp
		mov	r18=ar.rnat
		;;
		tpa	r19=loc4		// new bspstore
		mov	loc5=gp
		;;
		tpa	r20=loc3		// new sp
		ld8	r21=[in0],8
		;;
1:
		mov	r14=ip
		;;
		ld8	r22=[in0]
		add	r15=2f-1b,r14
		;;
		tpa	r14=r15
		;;
		rsm	psr.ic
		;;
		srlz.i
		;;
		mov	cr.iip=r14
		mov	cr.ifs=r0
		mov	cr.ipsr=r16
		;;
		rfi
2:
		mov	ar.bspstore=r19
		mov	sp=r20
		;;
		mov	ar.rnat=r18
		mov	ar.rsc=r17
		;;
		mov	b6=r21
		mov	gp=r22
		mov	out0=in1
		mov	out1=in2
		mov	out2=in3
		mov	out3=in4
		;;
		br.call.sptk.many	rp=b6
		mov	gp=loc5
		;;
		rsm	psr.i | psr.ic
		mov	r16=ar.rsc
		;;
		srlz.i
		mov	ar.rsc=0
		;;
		flushrs
		;;
		mov	r17=ar.rnat
		movl	r18=3f
		;;
		mov	cr.iip=r18
		mov	cr.ifs=r0
		mov	cr.ipsr=loc2
		;;
		rfi
3:
		mov	ar.bspstore=loc4
		mov	sp=loc3
		;;
		mov	ar.rnat=r17
		mov	ar.rsc=r16
		;;
		mov	rp=loc1
		mov	ar.pfs=loc0
		;;
		br.ret.sptk.many	rp
END(ia64_efi_physical)


/*
 * ia64_pal_ret ia64_pal_physical(ia64_fw_f, u_long, u_long, u_long, u_long)
 *
 *	loc0 = ar.pfs
 *	loc1 = rp
 *	loc2 = psr
 *	loc3 = sp
 *	loc4 = bsp
 *	loc5 = gp
 */
ENTRY(ia64_pal_physical, 5)
		.prologue
		.regstk 5,6,4,0
		.save	ar.pfs,loc0
		alloc	loc0=ar.pfs,5,6,4,0
		;;
		.save   rp,loc1
		mov	loc1=rp
		;;
		.body
		mov	loc2=psr		// save psr
		movl	r16=IA64_PSR_I | IA64_PSR_IT | IA64_PSR_DT | \
			    IA64_PSR_RT | IA64_PSR_DFL | IA64_PSR_DFH
		;;
		andcm	r14=loc2,r16
		movl	r15=IA64_PSR_BN
		;;
		rsm	psr.i
		mov	r17=ar.rsc
		or	r16=r14,r15		// new psr
		;;
		mov	ar.rsc=0
		or	loc2=loc2,r15
		;;
		flushrs
		mov	loc3=sp			// save sp
		;;
		mov	loc4=ar.bsp		// save ar.bsp
		mov	r18=ar.rnat
		;;
		mov	loc5=gp
		movl	r14=kstack
		;;
		tpa	r19=r14			// new bspstore
		movl	r15=kstack_top
		;;
		tpa	r20=r15			// new sp
		movl	r21=ia64_pal_entry
		;;
1:
		mov	r14=ip
		ld8	r22=[r21]
		;;
		tpa	r21=r22
		add	r15=2f-1b,r14
		;;
		tpa	r14=r15
		;;
		rsm	psr.ic
		;;
		srlz.i
		;;
		mov	cr.iip=r14
		mov	cr.ifs=r0
		mov	cr.ipsr=r16
		;;
		rfi
2:
		mov	ar.bspstore=r19
		add	sp=-16,r20
		;;
		mov	ar.rnat=r18
		mov	ar.rsc=r17
		;;
		mov	b6=r21
		mov	out0=in0
		mov	out1=in1
		mov	out2=in2
		mov	out3=in3
		// PAL static calls
		mov	r28=in0
		mov	r29=in1
		mov	r30=in2
		mov	r31=in3
		br.call.sptk.many	rp=b6
		mov	gp=loc5
		;;
		rsm	psr.i | psr.ic
		mov	r16=ar.rsc
		;;
		srlz.i
		mov	ar.rsc=0
		;;
		flushrs
		;;
		mov	r17=ar.rnat
		movl	r18=3f
		;;
		mov	cr.iip=r18
		mov	cr.ifs=r0
		mov	cr.ipsr=loc2
		;;
		rfi
3:
		mov	ar.bspstore=loc4
		mov	sp=loc3
		;;
		mov	ar.rnat=r17
		mov	ar.rsc=r16
		;;
		mov	rp=loc1
		mov	ar.pfs=loc0
		;;
		br.ret.sptk.many	rp
END(ia64_pal_physical)
