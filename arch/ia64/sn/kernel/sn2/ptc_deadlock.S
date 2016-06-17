/* 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */

#include <asm/sn/sn2/shub_mmr.h>

#define ZEROVAL		0x3f		// "zero" value for outstanding PIO requests
#define DEADLOCKBIT	SH_PIO_WRITE_STATUS_0_WRITE_DEADLOCK_SHFT
#define WRITECOUNT	SH_PIO_WRITE_STATUS_0_PENDING_WRITE_COUNT_SHFT
#define ALIAS_OFFSET	(SH_PIO_WRITE_STATUS_0_ALIAS-SH_PIO_WRITE_STATUS_0)


	.global	sn2_ptc_deadlock_recovery_core
	.proc  	sn2_ptc_deadlock_recovery_core

sn2_ptc_deadlock_recovery_core:
	.regstk 5,0,0,0

	ptc0  	 = in0
	data0 	 = in1
	ptc1  	 = in2
	data1 	 = in3
	piowc 	 = in4
	piowcphy = r30
	psrsave  = r2
	zeroval  = r3
	scr1	 = r16
	scr2	 = r17


	extr.u	piowcphy=piowc,0,61;;	// Convert piowc to uncached physical address
	dep	piowcphy=-1,piowcphy,63,1

	mov	zeroval=ZEROVAL		// "zero" value for PIO write count

1:
	add	scr2=ALIAS_OFFSET,piowc	// Address of WRITE_STATUS alias register 
	mov	scr1=7;;		// Clear DEADLOCK, WRITE_ERROR, MULTI_WRITE_ERROR
	st8.rel	[scr2]=scr1;;

5:	ld8.acq	scr1=[piowc];;		// Wait for PIOs to complete.
	extr.u	scr2=scr1,WRITECOUNT,7;;// PIO count
	cmp.ne	p6,p0=zeroval,scr2
(p6)	br.cond.sptk 5b
	


	////////////// BEGIN PHYSICAL MODE ////////////////////
	mov psrsave=psr			// Disable IC (no PMIs)
	rsm psr.i | psr.dt | psr.ic;;
	srlz.i;;

	st8.rel [ptc0]=data0		// Write PTC0 & wait for completion.

5:	ld8.acq	scr1=[piowcphy];;	// Wait for PIOs to complete.
	extr.u	scr2=scr1,WRITECOUNT,7;;// PIO count
	cmp.ne	p6,p0=zeroval,scr2
(p6)	br.cond.sptk 5b;;

	tbit.nz	p8,p7=scr1,DEADLOCKBIT;;// Test for DEADLOCK
	
(p7)	st8.rel [ptc1]=data1;;		// Now write PTC1.

5:	ld8.acq	scr1=[piowcphy];;	// Wait for PIOs to complete.
	extr.u	scr2=scr1,WRITECOUNT,7;;// PIO count
	cmp.ne	p6,p0=zeroval,scr2
(p6)	br.cond.sptk 5b
	
	tbit.nz	p8,p0=scr1,DEADLOCKBIT;;// Test for DEADLOCK

	mov psr.l=psrsave;;		// Reenable IC
	srlz.i;;
	////////////// END   PHYSICAL MODE ////////////////////

(p8)	br.cond.spnt 1b;;		// Repeat if DEADLOCK occurred.

	br.ret.sptk	rp
	.endp sn2_ptc_deadlock_recovery_core
