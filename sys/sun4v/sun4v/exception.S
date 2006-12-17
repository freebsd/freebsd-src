/*
 * Copyright (c) 2006 Kip Macy <kmacy@FreeBSD.org>
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
 */

	
#include <machine/asm.h>
__FBSDID("$FreeBSD$")			

#include "opt_compat.h"
#include "opt_ddb.h"
#include "opt_simulator.h"
#include "opt_trap_trace.h"
	
#include <machine/asi.h>
#include <machine/asmacros.h>
#include <machine/ktr.h>
#include <machine/pstate.h>
#include <machine/trap.h>
#include <machine/tstate.h>
#include <machine/wstate.h>
#include <machine/hypervisorvar.h>

#include "assym.s"
#include <machine/mmu.h>

#define PMAP_DEBUG

#define SPILL_FILL_MAGIC_TRAP_ON	nop
#define SPILL_FILL_MAGIC_TRAP_OFF	nop

#define REGSIZE         8

	.register %g2,#ignore
	.register %g3,#ignore
	.register %g6,#ignore
	.register %g7,#ignore

	.globl trap_conversion

#define PCB_REG %g6

#define PUTCHAR(x) \
0:				; \
	mov	x, %o0		; \
	mov	CONS_PUTCHAR, %o5	; \
	ta	FAST_TRAP	; \
	brnz	%o0, 0b		; \
	  nop

/*	
 * Atomically set the reference bit in a tte.
 */
#define	TTE_SET_BIT(r1, r2, r3, bit) \
	add	r1, TTE_DATA, r1 ; \
	ldx	[r1], r2 ; \
9:	or	r2, bit, r3 ; \
	casxa	[r1] ASI_N, r2, r3 ; \
	cmp	r2, r3 ; \
	bne,pn	%xcc, 9b ; \
	 mov	r3, r2

#define	TTE_SET_REF(r1, r2, r3)		TTE_SET_BIT(r1, r2, r3, TD_REF)
#define	TTE_SET_W(r1, r2, r3)		TTE_SET_BIT(r1, r2, r3, TD_W)

		
/*
 * Macros for spilling and filling live windows.
 * Here we use the more complicated [regaddr] format which requires
 * us to interleave setting the globals in order to be able to use
 * imm_asi - we don't ever implicitly assume kernel context as in
 * Solaris' spill/fill handlers so that we have the option of using
 * block initializing stores - twin doubleword loads could also be
 * advantageous but will require an additional macro
 *
 */


#define	SPILL(storer, base, size) \
	storer	%l0, [base + (0 * size)] ; \
	storer	%l1, [base + (1 * size)] ; \
	storer	%l2, [base + (2 * size)] ; \
	storer	%l3, [base + (3 * size)] ; \
	storer	%l4, [base + (4 * size)] ; \
	storer	%l5, [base + (5 * size)] ; \
	storer	%l6, [base + (6 * size)] ; \
	storer	%l7, [base + (7 * size)] ; \
	storer	%i0, [base + (8 * size)] ; \
	storer	%i1, [base + (9 * size)] ; \
	storer	%i2, [base + (10 * size)] ; \
	storer	%i3, [base + (11 * size)] ; \
	storer	%i4, [base + (12 * size)] ; \
	storer	%i5, [base + (13 * size)] ; \
	storer	%i6, [base + (14 * size)] ; \
	storer	%i7, [base + (15 * size)]

#define	SPILL_ASI(storer, bias, size, asi) \
	mov	0 + bias, %g1	                ;\
	storer	%l0, [%sp + %g1]asi             ;\
	mov	size + bias, %g2	        ;\
	storer	%l1, [%sp + %g2]asi             ;\
	mov	(2 * size) + bias, %g3	        ;\
	storer	%l2, [%sp + %g3]asi             ;\
	mov	(3 * size) + bias, %g4	        ;\
	storer	%l3, [%sp + %g4]asi             ;\
	add	%sp,  (4 * size), %g5           ;\
	storer	%l4, [%g5 + %g1]asi             ;\
	storer	%l5, [%g5 + %g2]asi             ;\
	storer	%l6, [%g5 + %g3]asi             ;\
	storer	%l7, [%g5 + %g4]asi             ;\
	add	%g5,  (4 * size), %g5           ;\
	storer	%i0, [%g5 + %g1]asi             ;\
	storer	%i1, [%g5 + %g2]asi             ;\
	storer	%i2, [%g5 + %g3]asi             ;\
	storer	%i3, [%g5 + %g4]asi             ;\
	add	%g5,  (4 * size), %g5           ;\
	storer	%i4, [%g5 + %g1]asi             ;\
	storer	%i5, [%g5 + %g2]asi             ;\
	storer	%i6, [%g5 + %g3]asi             ;\
	storer	%i7, [%g5 + %g4]asi  

/* 16 instructions */
#define	SPILL_ASI_64 \
	stxa	%l0, [%g1 + 0x0]%asi             ;\
	stxa	%i0, [%g1 + 0x40]%asi             ;\
	stxa	%l1, [%g1 + 0x8]%asi              ;\
	stxa	%l2, [%g1 + 0x10]%asi             ;\
	stxa	%l3, [%g1 + 0x18]%asi             ;\
	stxa	%l4, [%g1 + 0x20]%asi             ;\
	stxa	%l5, [%g1 + 0x28]%asi             ;\
	stxa	%l6, [%g1 + 0x30]%asi             ;\
	stxa	%l7, [%g1 + 0x38]%asi             ;\
	stxa	%i1, [%g1 + 0x48]%asi             ;\
	stxa	%i2, [%g1 + 0x50]%asi             ;\
	stxa	%i3, [%g1 + 0x58]%asi             ;\
	stxa	%i4, [%g1 + 0x60]%asi             ;\
	stxa	%i5, [%g1 + 0x68]%asi             ;\
	stxa	%i6, [%g1 + 0x70]%asi             ;\
	stxa	%i7, [%g1 + 0x78]%asi  

/* 23 instructions */
#define	FILL(loader, bias, size, asi) \
	mov	0 + bias, %g1			;\
	loader	[%sp + %g1]asi, %l0		;\
	mov	size + bias, %g2		;\
	loader	[%sp + %g2]asi, %l1		;\
	mov	(2 * size) + bias, %g3		;\
	loader	[%sp + %g3]asi, %l2		;\
	mov	(3 * size) + bias, %g4		;\
	loader	[%sp + %g4]asi, %l3		;\
	add	%sp, (4 * size), %g5		;\
	loader	[%g5 + %g1]asi, %l4		;\
	loader	[%g5 + %g2]asi, %l5		;\
	loader	[%g5 + %g3]asi, %l6		;\
	loader	[%g5 + %g4]asi, %l7		;\
	add	%g5, (4 * size), %g5		;\
	loader	[%g5 + %g1]asi, %i0		;\
	loader	[%g5 + %g2]asi, %i1		;\
	loader	[%g5 + %g3]asi, %i2		;\
	loader	[%g5 + %g4]asi, %i3		;\
	add	%g5, (4 * size), %g5		;\
	loader	[%g5 + %g1]asi, %i4		;\
	loader	[%g5 + %g2]asi, %i5		;\
	loader	[%g5 + %g3]asi, %i6		;\
	loader	[%g5 + %g4]asi, %i7

#define	SPILL_ASI_SET(storer, size) \
	storer	%l0, [%g1 + (0 * size)]%asi     ;\
	storer	%l1, [%g1 + (1 * size)]%asi     ;\
	storer	%l2, [%g1 + (2 * size)]%asi     ;\
	storer	%l3, [%g1 + (3 * size)]%asi     ;\
	storer	%l4, [%g1 + (4 * size)]%asi     ;\
	storer	%l5, [%g1 + (5 * size)]%asi     ;\
	storer	%l6, [%g1 + (6 * size)]%asi     ;\
	storer	%l7, [%g1 + (7 * size)]%asi     ;\
	storer	%i0, [%g1 + (8 * size)]%asi     ;\
	storer	%i1, [%g1 + (9 * size)]%asi     ;\
	storer	%i2, [%g1 + (10 * size)]%asi     ;\
	storer	%i3, [%g1 + (11 * size)]%asi     ;\
	storer	%i4, [%g1 + (12 * size)]%asi     ;\
	storer	%i5, [%g1 + (13 * size)]%asi     ;\
	storer	%i6, [%g1 + (14 * size)]%asi     ;\
	storer	%i7, [%g1 + (15 * size)]%asi 

/* 16 instructions */
#define	FILL_ASI_SET(loader, size) \
	loader	[%g1 + 0x0]%asi, %l0		;\
	loader	[%g1 + (size * 1)]%asi, %l1	;\
	loader	[%g1 + (size * 2)]%asi, %l2	;\
	loader	[%g1 + (size * 3)]%asi, %l3	;\
	loader	[%g1 + (size * 4)]%asi, %l4	;\
	loader	[%g1 + (size * 5)]%asi, %l5	;\
	loader	[%g1 + (size * 6)]%asi, %l6	;\
	loader	[%g1 + (size * 7)]%asi, %l7	;\
	loader	[%g1 + (size * 8)]%asi, %i0	;\
	loader	[%g1 + (size * 9)]%asi, %i1	;\
	loader	[%g1 + (size * 10)]%asi, %i2	;\
	loader	[%g1 + (size * 11)]%asi, %i3	;\
	loader	[%g1 + (size * 12)]%asi, %i4	;\
	loader	[%g1 + (size * 13)]%asi, %i5	;\
	loader	[%g1 + (size * 14)]%asi, %i6	;\
	loader	[%g1 + (size * 15)]%asi, %i7	
	
/* 9 instructions */
#define	FILL_DW \
	prefetch [%g1 + 0x40], #one_read        ;\
	ldda	[%g1 + 0]%asi, %l0		;\
	ldda	[%g1 + 0x10]%asi, %l2		;\
	ldda	[%g1 + 0x20]%asi, %l4		;\
	ldda	[%g1 + 0x30]%asi, %l6		;\
	ldda	[%g1 + 0x40]%asi, %i0		;\
	ldda	[%g1 + 0x50]%asi, %i2		;\
	ldda	[%g1 + 0x60]%asi, %i4		;\
	ldda	[%g1 + 0x70]%asi, %i6		

#include <sun4v/sun4v/wbuf.S>	
	/*
	 * Clean window traps occur when %cleanwin is zero to ensure that data
	 * is not leaked between address spaces in registers.
	 */
	.macro	clean_window
	clr	%o0
	clr	%o1
	clr	%o2
	clr	%o3
	clr	%o4
	clr	%o5
	clr	%o6
	clr	%o7
	clr	%l0
	clr	%l1
	clr	%l2
	clr	%l3
	clr	%l4
	clr	%l5
	clr	%l6
	rdpr	%cleanwin, %l7
	inc	%l7
	wrpr	%l7, 0, %cleanwin
	clr	%l7
	retry
	.align	128
	.endm

	.macro	tl0_split
	.endm

	.macro	tl0_setup	type
	rdpr	%tt, %g3
	sub	%g0, 1, %g4
	set	trap, %g1
	ba	%xcc, tl0_trap
	  mov	\type, %g2
	.endm

	/*
	 * Generic trap type.  Call trap() with the specified type.
	 */
	.macro	tl0_gen		type
	tl0_setup \type
	.align	32
	.endm

	/*
	 * This is used to suck up the massive swaths of reserved trap types.
	 * Generates count "reserved" trap vectors.
	 */

	.macro	tl0_reserved	count
	.rept	\count
	tl0_gen	T_RESERVED
	.align	32
	.endr
	.endm

	.macro	tl1_setup	type
	rdpr	%tt, %g3
	sub	%g0, 1, %g4
	set	trap, %g1
	ba	%xcc, tl1_trap
	  mov	\type, %g2
	.endm

	.macro	tl1_gen		type
	tl1_setup \type
	.align	32
	.endm

	.macro	tl1_reserved	count
	.rept	\count
	tl1_gen	T_RESERVED
	.endr
	.endm

	.macro	insn_excptn
	GET_MMFSA_SCRATCH(%g1)			
	mov	MMFSA_D_ADDR, %g2
	ldxa	[%g1 + %g2]ASI_REAL, %g3
	sub	%g0, 1, %g4
	set	trap, %g1
	ba,pt	%xcc, tl0_trap 
	  mov	T_INSTRUCTION_EXCEPTION, %g2

	.align	32
	.endm

	.macro	insn_miss
	GET_MMFSA_SCRATCH(%g1)	
	mov	MMFSA_I_TYPE, %g2 
	mov	MMFSA_I_ADDR, %g3
	mov	MMFSA_I_CTX, %g7 
	ldxa	[%g1 + %g2]ASI_REAL, %g4
	ba,pt	%xcc, tsb_miss_handler
	  ldxa	[%g1 + %g3]ASI_REAL, %g5
	.align	32
	.endm
	
	.macro	data_excptn
	GET_MMFSA_SCRATCH(%g1)			
	mov	MMFSA_D_ADDR, %g2
	ba,pt	%xcc, data_excptn_fault
	ldxa	[%g1 + %g2]ASI_REAL, %g3
	.align	32
	.endm

ENTRY(data_excptn_fault)
	mov	MMFSA_D_CTX, %g7 
	ldxa	[%g1 + %g7]ASI_REAL, %g4
	sllx	%g4, TRAP_CTX_SHIFT, %g4
	or	%g4, T_DATA_EXCEPTION, %g2
	set	trap, %g1
	ba,pt	%xcc, tl0_trap 
	  sub	%g0, 1, %g4
END(data_excptn_fault)

	.macro	data_miss
	GET_MMFSA_SCRATCH(%g1)			
	mov	MMFSA_D_TYPE, %g2 
	mov	MMFSA_D_ADDR, %g3
	mov	MMFSA_D_CTX, %g7 
	ldxa	[%g1 + %g2]ASI_REAL, %g4
	ba,pt	%xcc, tsb_miss_handler
	  ldxa	[%g1 + %g3]ASI_REAL, %g5
	.align	32
	.endm

	.macro	data_prot
	GET_MMFSA_SCRATCH(%g1)		
	mov	MMFSA_D_ADDR, %g3
	mov	MMFSA_D_CTX,  %g7
	ba,pt	%xcc, tsb_miss_handler
	  ldxa	[%g1 + %g3]ASI_REAL, %g5
	.align	32
	.endm

	.macro	tl0_align
	GET_MMFSA_SCRATCH(%g1)	
	mov	MMFSA_D_ADDR, %g3
	mov	MMFSA_D_CTX,  %g7
	ba,pt	%xcc, align_fault
	  ldxa	[%g1 + %g3]ASI_REAL, %g3
	.align	32
	.endm

ENTRY(align_fault)
	ldxa	[%g1 + %g7]ASI_REAL, %g4
	sllx	%g4, TRAP_CTX_SHIFT, %g4
	or	%g4, T_MEM_ADDRESS_NOT_ALIGNED, %g2
	set	trap, %g1
	ba,pt	%xcc, tl0_trap
	  sub	%g0, 1, %g4
END(align_fault)
		
	.macro	cpu_mondo
	ba,a,pt %xcc, cpu_mondo
	.align	32
	.endm

	.macro	dev_mondo
	ba,a,pt %xcc, dev_mondo
	.align	32
	.endm

	.macro	resumable_error
	!MAGIC_TRAP_ON
	!MAGIC_EXIT
	clr	%g3
	sub	%g0, 1, %g4
	set	trap, %g1
	ba,pt	%xcc, tl0_trap
	  mov	T_RESUMABLE_ERROR, %g2
	.align	32
	.endm

	.macro	nonresumable_error
	clr	%g3
	sub	%g0, 1, %g4
	set	trap, %g1
	ba,pt	%xcc, tl0_trap
	  mov	T_NONRESUMABLE_ERROR, %g2
	.align	32
	.endm


#define ALIGN_128	.align  128
#define SYNC		#Sync
#define LOOKASIDE	#Lookaside

#ifdef USE_FAST_SPILLFILL
#define spill_64bit_asi(asival, asival_unaligned, target)	\
	wr	%g0, asival, %asi  ;            \
	add	%sp, SPOFF, %g1	   ;            \
	SPILL_ASI_64               ;	        \
	membar	LOOKASIDE          ;            \
	saved			   ;		\
	retry			   ;		\
	.skip (31-21)*4		   ;		\
	ba,a,pt %xcc, fault_64bit_##target ; \
	ALIGN_128	

#define	spill_64clean(asival, asival_unaligned, target)		\
	wr	%g0, asival, %asi  ;            \
	add	%sp, SPOFF, %g1	   ;            \
	SPILL_ASI_64               ; 	        \
	membar	LOOKASIDE          ;            \
	b	spill_clean	   ;		\
	  mov	WSTATE_USER64, %g7 ; 		\
	.skip (31-21)*4		   ;		\
	ba,a,pt %xcc, fault_64bit_##target ; 	\
	ALIGN_128	

#define fill_64bit_asi(asival, asival_unaligned, target)	\
	add	%sp, SPOFF, %g1	   ;            \
	wr	%g0, asival, %asi  ;            \
	FILL_DW                    ; 		\
	restored		   ;		\
	retry			   ;		\
	.skip (31-13)*4		   ;		\
	ba,a,pt %xcc, fault_64bit_##target ; \
	ALIGN_128	
#else
#define spill_64bit_asi(asival, asival_unaligned, target)	\
	wr	%g0, asival_unaligned, %asi ;   \
	add	%sp, SPOFF, %g1	  ;             \
	SPILL_ASI_SET(stxa, 8)     ;	        \
	saved			   ;		\
	retry			   ;		\
	.skip (31-20)*4		   ;		\
	ba,a,pt %xcc, fault_64bit_##target ; \
	ALIGN_128	

#define	spill_64clean(asival, asival_unaligned, target)		\
	wr	%g0, asival_unaligned, %asi  ;            \
	add	%sp, SPOFF, %g1	   ;            \
	SPILL_ASI_SET(stxa, 8)     ; 	        \
	b	spill_clean	   ;		\
	  mov	WSTATE_USER64, %g7 ; 		\
	.skip (31-20)*4		   ;		\
	ba,a,pt %xcc, fault_64bit_##target ; 	\
	ALIGN_128	

#define fill_64bit_asi(asival, asival_unaligned, target)	\
	wr	%g0, asival_unaligned, %asi  ;  \
	add	%sp, SPOFF, %g1	  ;             \
	FILL_ASI_SET(ldxa, 8)      ; 		\
	restored		   ;		\
	retry			   ;		\
	.skip (31-20)*4		   ;		\
	ba,a,pt %xcc, fault_64bit_##target ; \
	ALIGN_128	
#endif

#define spill_32bit_asi(asi, target)		\
	srl	%sp, 0, %sp	;		\
	SPILL_FILL_MAGIC_TRAP_ON;               \
	SPILL_ASI(sta, 0, 4, asi)	;	\
	saved			; 		\
	SPILL_FILL_MAGIC_TRAP_OFF;		\
	retry			; 		\
	.skip (31-28)*4		; 		\
	ba,a,pt %xcc, fault_32bit_##target ; \
	ALIGN_128

#define	spill_32clean(asi, target)		\
	srl	%sp, 0, %sp	; 		\
	SPILL_FILL_MAGIC_TRAP_ON;               \
	SPILL_ASI(sta, 0, 4, asi)	; 	\
	b	spill_clean	; 		\
	  mov	WSTATE_USER32, %g7 ; 		\
	.skip (31-27)*4		; 		\
	ba,a,pt    %xcc, fault_32bit_##target ; \
	ALIGN_128	
	
#define fill_32bit_asi(asi, target)		\
	srl	%sp, 0, %sp	;		\
	SPILL_FILL_MAGIC_TRAP_ON;               \
	FILL(lda, 0, 4, asi)	; 		\
	restored		; 		\
	retry			; 		\
	.skip (31-27)*4		; 		\
	ba,a,pt %xcc, fault_32bit_##target ; \
	ALIGN_128	

.align 128
ENTRY(fill_64bit_slow_fn0)                      
fill_slow_start:		
	FILL_ASI_SET(ldxa, 8);                  
	restored		;               
	retry 			;               
	.skip (31-18)*4		   ;		
	ba,a,pt %xcc, fault_64bit_fn0 ;
	.align 128	
END(fill_64bit_slow_fn0)
ENTRY(fill_64bit_slow_not)                      
	FILL_ASI_SET(ldxa, 8);                  
	restored		;               
	retry 			;               
	.skip (31-18)*4		   ;		
	ba,a,pt %xcc, fault_64bit_not ; 
	.align 128	
END(fill_64bit_slow_not)
fill_slow_end:	
		
	.macro	spill_32bit_primary_sn0
	spill_32bit_asi(ASI_AIUP, sn0)
	.endm

	.macro	spill_64bit_primary_sn0
	spill_64bit_asi(ASI_LDSTBI_AIUP, ASI_AIUP, sn0)
	.endm

	.macro spill_32clean_primary_sn0
	spill_32clean(ASI_AIUP, sn0)
	.endm
			
	.macro spill_64clean_primary_sn0
	spill_64clean(ASI_LDSTBI_AIUP, ASI_AIUP, sn0)
	.endm

	.macro spill_32bit_nucleus_not
	spill_32bit_asi(ASI_N, not)
	.endm

	.macro spill_64bit_nucleus_not
	spill_64bit_asi(ASI_LDSTBI_N, ASI_N, not)
	.endm

	.macro	spill_32bit_secondary_so0
	spill_32bit_asi(ASI_AIUS, so0)
	.endm

	.macro	spill_64bit_secondary_so0
	spill_64bit_asi(ASI_LDSTBI_AIUS, ASI_AIUS, so0)
	.endm
	
	.macro	fill_32bit_primary_fn0
	fill_32bit_asi(ASI_AIUP, fn0)
	.endm

	.macro	fill_64bit_primary_fn0
	fill_64bit_asi(ASI_LDSTBI_AIUP, ASI_AIUP, fn0)
	.endm

	.macro fill_32bit_nucleus_not
	fill_32bit_asi(ASI_N, not)
	.endm

	.macro fill_64bit_nucleus_not
	fill_64bit_asi(ASI_LDSTBI_N, ASI_N, not)
	.endm

	.macro	spill_32bit_tt1_primary_sn1
	ba,a,pt %xcc, fault_32bit_sn1
	  nop
	.align 128
	.endm

	.macro	spill_64bit_tt1_primary_sn1 
	ba,a,pt %xcc, fault_64bit_sn1
	  nop
	.align 128
	.endm

	.macro	spill_64bit_ktt1_sk
	ba,a,pt %xcc, fault_64bit_sk
	  nop
	.align 128
	.endm

	.macro	spill_mixed_ktt1_sk
	btst	1, %sp
	bz,a,pt	%xcc, fault_32bit_sk
	  srl	%sp, 0, %sp
	ba,a,pt	%xcc, fault_64bit_sk
	  nop
	.align 128
	.endm

	.macro	spill_32bit_tt1_secondary_so1
	ba,a,pt %xcc, fault_32bit_so1
	  nop
	.align 128
	.endm

	.macro	spill_64bit_tt1_secondary_so1
	ba,a,pt %xcc, fault_64bit_so1
	  nop
	.align 128
	.endm

	.macro	spill_mixed
!	MAGIC_EXIT
	nop
	.align	128
	.endm

	.macro	fill_mixed
!	MAGIC_EXIT
	nop
	.align	128
	.endm

	.macro	tl1_align
	ba,a,pt	%xcc, tl1_trap
	.align	32
	.endm		
		
	.macro	tl0_pil_entry level, mask
	wrpr	%g0, 1, %gl
	set	\mask, %g1
	clr	%g2
	clr	%g3
	wr	%g1, 0, %clear_softint
	ba	%xcc, tl0_intr
	  mov	\level, %g4
	.align	32
	.endm

#define	INTR(level, traplvl)						\
	tl ## traplvl ## _pil_entry	level, 1 << level

#define	TICK(traplvl) \
	tl ## traplvl ## _pil_entry	PIL_TICK, 1

#define	INTR_LEVEL(tl)							\
	INTR(1, tl) ;							\
	INTR(2, tl) ;							\
	INTR(3, tl) ;							\
	INTR(4, tl) ;							\
	INTR(5, tl) ;							\
	INTR(6, tl) ;							\
	INTR(7, tl) ;							\
	INTR(8, tl) ;							\
	INTR(9, tl) ;							\
	INTR(10, tl) ;							\
	INTR(11, tl) ;							\
	INTR(12, tl) ;							\
	INTR(13, tl) ;							\
tick_ ## tl ## _entry:							\
	TICK(tl) ;							\
	INTR(15, tl) ;

	.macro	tl0_pil
	INTR_LEVEL(0)
	.endm


	.macro	tl0_syscall
	clr	%g3
	sub	%g0, 1, %g4
        set	syscall, %g1
	ba	%xcc, tl0_trap
	  mov	T_SYSCALL, %g2
	.align	32
	.endm

#ifdef KDTRACE
	.macro	dtrace_fasttrap
	sethi	%hi(dtrace_fasttrap_probe_ptr), %g4
	ldx	[%g4 + %lo(dtrace_fasttrap_probe_ptr)], %g4
	set	dtrace_fasttrap_probe, %g1
	brnz,pn	%g4, tl0_utrap
	  sub	%g0, 1, %g4
	.align	32
	.endm

	.macro	dtrace_pid
	set	dtrace_pid_probe, %g1
	ba,pt	%xcc, tl0_utrap
	  sub	%g0, 1, %g4
	.align	32
	.endm

	.macro	dtrace_return
	set	dtrace_return_probe, %g1
	ba,pt	%xcc, tl0_utrap
	  sub	%g0, 1, %g4
	.align 32
	.endm
#else
	.macro	dtrace_fasttrap
	nop
	.align 32
	.endm
		
	.macro	dtrace_pid
	nop
	.align 32
	.endm

	.macro	dtrace_return
	nop
	.align 32
	.endm
#endif	

	! fetch FP context into local registers
	.macro	tl0_fpemu_context
	GET_PCB(PCB_REG)			! 3 instructions
	ldx	[PCB_REG + PCB_TSTATE], %l5	! %tstate
	ldx	[PCB_REG + PCB_TPC], %l6	! %tpc
	ldx	[PCB_REG + PCB_TNPC], %l7	! %tnpc
	ldx	[PCB_REG + PCB_TT], %g2		! %tt
	ba,a,pt	%xcc, tl0_fpemu_context
	.align	32
	.endm

ENTRY(tl0_fpemu_context)
	mov	%g2, %o0
	clr	%o1
	ldx	[PCB_REG + PCB_SFAR], %o4
	
	rd	%fprs, %l1
	or	%l1, FPRS_FEF, %l2
	wr	%l2, 0, %fprs
	stx	%fsr, [PCB_REG + PCB_PAD]
	ldx	[PCB_REG + PCB_PAD], %l4
	wr	%l1, 0, %fprs
		
	sub	%fp, CCFSZ, %sp 
	done	
END(tl0_fpemu_context)
	
	.macro	tl0_fp_restore
	GET_PCB(PCB_REG)			! 3 instructions
	ldx	[%g6 + PCB_FLAGS], %g1
	ba,pt	%xcc, tl0_fp_restore
	  wr	%g0, FPRS_FEF, %fprs
	.align	32
	.endm

	.macro tl0_fp_enable
	GET_PCB(PCB_REG)			! 3 instructions
	ldx	[PCB_REG + PCB_FLAGS], %g1
	andcc	%g1, PCB_FEF, %g0
	bnz,pt	%xcc, tl0_fp_restore		
	  wr	%g0, FPRS_FEF, %fprs
	retry
	.align	32
	.endm
	
ENTRY(tl0_fp_restore)
	andn	%g1, PCB_FEF, %g1
	stx	%g1, [%g6 + PCB_FLAGS]
	
	ldd	[PCB_REG + PCB_UFP + (0 * 64)], %f0
	ldd	[PCB_REG + PCB_UFP + (1 * 64)], %f16
	ldd	[PCB_REG + PCB_UFP + (2 * 64)], %f32
	ldd	[PCB_REG + PCB_UFP + (3 * 64)], %f48
	retry
END(tl0_fp_restore)

	.macro	tl1_insn_excptn
	 nop
	.align	32
	.endm


	.macro	tl1_soft	count
	.rept	\count
	tl1_gen	T_SOFT | T_KERNEL
	.endr
	.endm

	.sect	.trap
	.align	0x8000
	.globl	tl0_base
tl0_base:
 	tl0_reserved	8				! 0x0-0x7
tl0_insn_excptn:
 	insn_excptn					! 0x8
tl0_insn_miss:
	insn_miss					! 0x9
	tl0_reserved	6				! 0xa-0xf
tl0_insn_illegal:
 	tl0_gen		T_ILLEGAL_INSTRUCTION		! 0x10
tl0_priv_opcode:
 	tl0_gen		T_PRIVILEGED_OPCODE		! 0x11
 	tl0_reserved	14				! 0x12-0x1f
tl0_fp_disabled:
	tl0_fp_enable					! 0x20
tl0_fp_ieee:
 	tl0_gen		T_FP_EXCEPTION_IEEE_754		! 0x21
tl0_fp_other:
 	tl0_gen		T_FP_EXCEPTION_OTHER		! 0x22
tl0_tag_ovflw:
 	tl0_gen		T_TAG_OVERFLOW			! 0x23
tl0_clean_window:
 	clean_window					! 0x24
tl0_divide:
 	tl0_gen		T_DIVISION_BY_ZERO		! 0x28
 	tl0_reserved	7				! 0x29-0x2f
tl0_data_excptn:
 	data_excptn					! 0x30
tl0_data_miss:
 	data_miss					! 0x31
 	tl0_reserved	2				! 0x32-0x33
tl0_align:
 	tl0_align					! 0x34
tl0_align_lddf:
 	tl0_gen		T_RESERVED			! 0x35
tl0_align_stdf:
	tl0_gen		T_RESERVED			! 0x36
tl0_priv_action:
	tl0_gen		T_PRIVILEGED_ACTION		! 0x37
	tl0_reserved	9				! 0x38-0x40
tl0_intr_level_41:
	tl0_pil						! 0x41-0x4f
	tl0_reserved	18				! 0x50-0x61
tl0_watch_virt_62:
	tl0_gen		T_VA_WATCHPOINT			! 0x62
	tl0_reserved	9				! 0x63-0x6b
tl0_data_prot_6c:	
	data_prot					! 0x6c
	tl0_reserved	9				! 0x6d-0x75
tl0_breakpoint_76:
	tl0_gen		T_BREAKPOINT			! 0x76
	tl0_reserved	5				! 0x77-0x7b
tl0_cpu_mondo_7c:
	cpu_mondo					! 0x7c
tl0_dev_mondo_7d:	
	dev_mondo					! 0x7d	
tl0_resumable_error_7e:
	resumable_error					! 0x7e
tl0_nonresumable_error_7f:	
	nonresumable_error				! 0x7f	
tl0_spill_n_normal_80:
tl0_spill_0_normal:	
	tl0_reserved		4			! 0x80
tl0_spill_1_normal:	
	spill_32bit_primary_sn0				! 0x84
tl0_spill_2_normal:	
	spill_64bit_primary_sn0				! 0x88
tl0_spill_3_normal:	
	spill_32clean_primary_sn0			! 0x8c
tl0_spill_4_normal:	
	spill_64clean_primary_sn0			! 0x90
tl0_spill_5_normal:	
	spill_32bit_nucleus_not				! 0x94
tl0_spill_6_normal:	
	spill_64bit_nucleus_not				! 0x98
tl0_spill_7_normal:	
	spill_mixed					! 0x9c
tl0_spill_0_other:
	tl0_reserved		4			! 0xa0
tl0_spill_1_other:
	spill_32bit_secondary_so0			! 0xa4
tl0_spill_2_other:
	spill_64bit_secondary_so0			! 0xa8
tl0_spill_3_other:
	spill_32bit_secondary_so0			! 0xac
tl0_spill_4_other:
	spill_64bit_secondary_so0			! 0xb0
tl0_spill_5_other:
	tl0_reserved		4			! 0xb4
tl0_spill_6_other:
	tl0_reserved		4			! 0xb8
tl0_spill_7_other:
	tl0_reserved		4			! 0xbc
tl0_fill_n_normal:
	tl0_reserved		4			! 0xc0
tl0_fill_1_normal:	
	fill_32bit_primary_fn0				! 0xc4 
tl0_fill_2_normal:	
	fill_64bit_primary_fn0				! 0xc8
tl0_fill_3_normal:	
	fill_32bit_primary_fn0				! 0xcc
tl0_fill_4_normal:	
	fill_64bit_primary_fn0				! 0xd0
tl0_fill_5_normal:	
	fill_32bit_nucleus_not				! 0xd4
tl0_fill_6_normal:	
	fill_64bit_nucleus_not				! 0xd8
tl0_fill_7_normal:	
	fill_mixed					! 0xdc
tl0_fill_n_other_e0:
	tl0_reserved		32			! 0xe0-0xff
tl0_soft_100:
	tl0_gen		T_SYSCALL			! 0x100
	tl0_gen		T_BREAKPOINT			! 0x101
	tl0_gen		T_DIVISION_BY_ZERO		! 0x102
	tl0_reserved	1				! 0x103
	tl0_gen		T_CLEAN_WINDOW			! 0x104
	tl0_gen		T_RANGE_CHECK			! 0x105
	tl0_gen		T_FIX_ALIGNMENT			! 0x106
	tl0_gen		T_INTEGER_OVERFLOW		! 0x107
	tl0_gen		T_SYSCALL			! 0x108
	tl0_gen		T_SYSCALL			! 0x109
	tl0_fp_restore					! 0x10a
	tl0_fpemu_context				! 0x10b
	tl0_reserved	4				! 0x10c-0x10f
	tl0_gen		T_TRAP_INSTRUCTION_16		! 0x110
	tl0_gen		T_TRAP_INSTRUCTION_17		! 0x111
	tl0_gen		T_TRAP_INSTRUCTION_18		! 0x112
	tl0_gen		T_TRAP_INSTRUCTION_19		! 0x113
	tl0_gen		T_TRAP_INSTRUCTION_20		! 0x114
	tl0_gen		T_TRAP_INSTRUCTION_21		! 0x115
	tl0_gen		T_TRAP_INSTRUCTION_22		! 0x116
	tl0_gen		T_TRAP_INSTRUCTION_23		! 0x117
	tl0_gen		T_TRAP_INSTRUCTION_24		! 0x118
	tl0_gen		T_TRAP_INSTRUCTION_25		! 0x119
	tl0_gen		T_TRAP_INSTRUCTION_26		! 0x11a
	tl0_gen		T_TRAP_INSTRUCTION_27		! 0x11b
	tl0_gen		T_TRAP_INSTRUCTION_28		! 0x11c
	tl0_gen		T_TRAP_INSTRUCTION_29		! 0x11d
	tl0_gen		T_TRAP_INSTRUCTION_30		! 0x11e
	tl0_gen		T_TRAP_INSTRUCTION_31		! 0x11f
	tl0_reserved	24				! 0x120-0x137
tl0_dtrace_pid:						
	dtrace_pid					! 0x138
tl0_dtrace_fasttrap:					
	dtrace_fasttrap					! 0x139
tl0_dtrace_return:					
	dtrace_return					! 0x13a
	tl0_reserved	5				! 0x13b - 0x13f
	tl0_gen		T_SYSCALL			! 0x140       LP64 system call
	tl0_syscall					! 0x141
	tl0_gen		T_SYSCALL			! 0x142
	tl0_gen		T_SYSCALL			! 0x143
	tl0_reserved	188				! 0x144-0x1ff
tl1_base:
 	tl1_reserved	9				! 0x200-0x208
tl1_insn_miss_209:
	insn_miss					! 0x209
	tl1_reserved	26				! 0x20a-0x223
tl1_clean_window_224:
 	clean_window					! 0x224
tl1_divide_228:
 	tl1_reserved	8				! 0x228-0x22f
tl1_data_excptn_230:
 	data_excptn					! 0x230
tl1_data_miss_231:
	data_miss					! 0x231
 	tl1_reserved	2				! 0x232-0x233
tl1_align:
 	tl1_align					! 0x234
	tl1_reserved	55				! 0x235-0x26b
tl1_data_prot:
	data_prot					! 0x26c
	tl1_reserved	18				! 0x26c-0x27e
tl1_nonresumable_error:	
	nonresumable_error				! 0x27f	
tl1_spill_n_normal:
tl1_spill_0_normal:	
	tl1_reserved		4			! 0x280
tl1_spill_1_normal:
	spill_32bit_tt1_primary_sn1			! 0x284
tl1_spill_2_normal:
	spill_64bit_tt1_primary_sn1			! 0x288
tl1_spill_3_normal:
	spill_32bit_tt1_primary_sn1			! 0x28c
tl1_spill_4_normal:
	spill_64bit_tt1_primary_sn1			! 0x290
tl1_spill_5_normal:
	tl1_reserved		4			! 0x294
tl1_spill_6_normal:
	spill_64bit_ktt1_sk				! 0x298
tl1_spill_7_normal:
	spill_mixed_ktt1_sk				! 0x29c
tl1_spill_n_other:
tl1_spill_0_other:	
	tl1_reserved		4			! 0x2a0
tl1_spill_1_other:	
	spill_32bit_tt1_secondary_so1			! 0x2a4
tl1_spill_2_other:	
	spill_64bit_tt1_secondary_so1			! 0x2a8
tl1_spill_3_other:	
	spill_32bit_tt1_secondary_so1			! 0x2ac
tl1_spill_4_other:	
	spill_64bit_tt1_secondary_so1			! 0x2b0
tl1_spill_5_other:	
	tl1_reserved		4			! 0x2b4
tl1_spill_6_other:	
	tl1_reserved		4			! 0x2b8
tl1_spill_7_other:	
	tl1_reserved		4			! 0x2bc
tl1_fill_n_normal:
	tl1_reserved		32			! 0x2c0-0x2df
tl1_fill_n_other:
	tl1_reserved		32			! 0x2e0-0x2ff
tl1_soft_traps:
	tl1_reserved		256
.globl tl1_end
tl1_end:					

spill_clean:
	sethi	%hi(nwin_minus_one), %g5
	ld	[%g5 + %lo(nwin_minus_one)], %g5
	rdpr	%cwp, %g6
	deccc	%g6
	movneg	%xcc, %g5, %g6				! if (--%cwp < 0) %g6 = nwin-1
	wrpr	%g6, %cwp
	clr	%l0
	clr	%l1
	clr	%l2
	clr	%l3
	clr	%l4
	clr	%l5
	clr	%l6
	clr	%l7
	wrpr	%g0, %g7, %wstate
	saved
	retry

	
	
#define	KWBUF64_TO_STACK(SBP,SPP,TMP) \
	ldx	[SBP + (0*8)], TMP; \
	stx	TMP, [SPP + SPOFF + 0]; \
	ldx	[SBP + (1*8)], TMP; \
	stx	TMP, [SPP + SPOFF + 8]; \
	ldx	[SBP + (2*8)], TMP; \
	stx	TMP, [SPP + SPOFF + 16]; \
	ldx	[SBP + (3*8)], TMP; \
	stx	TMP, [SPP + SPOFF + 24]; \
	ldx	[SBP + (4*8)], TMP; \
	stx	TMP, [SPP + SPOFF + 32]; \
	ldx	[SBP + (5*8)], TMP; \
	stx	TMP, [SPP + SPOFF + 40]; \
	ldx	[SBP + (6*8)], TMP; \
	stx	TMP, [SPP + SPOFF + 48]; \
	ldx	[SBP + (7*8)], TMP; \
	stx	TMP, [SPP + SPOFF + 56]; \
	ldx	[SBP + (8*8)], TMP; \
	stx	TMP, [SPP + SPOFF + 64]; \
	ldx	[SBP + (9*8)], TMP; \
	stx	TMP, [SPP + SPOFF + 72]; \
	ldx	[SBP + (10*8)], TMP; \
	stx	TMP, [SPP + SPOFF + 80]; \
	ldx	[SBP + (11*8)], TMP; \
	stx	TMP, [SPP + SPOFF + 88]; \
	ldx	[SBP + (12*8)], TMP; \
	stx	TMP, [SPP + SPOFF + 96]; \
	ldx	[SBP + (13*8)], TMP; \
	stx	TMP, [SPP + SPOFF + 104]; \
	ldx	[SBP + (14*8)], TMP; \
	stx	TMP, [SPP + SPOFF + 112]; \
	ldx	[SBP + (15*8)], TMP; \
	stx	TMP, [SPP + SPOFF + 120];


#define	fill_64bit_rtt(asi_num)				\
	wr	%g0, asi_num, %asi				;\
	rdpr	%cwp, %g1					;\
	sub	%g1, 1, %g1					;\
	wrpr	%g1, %cwp					;\
	ldxa	[%sp + SPOFF + 0]%asi, %l0			;\
	ldxa	[%sp + SPOFF + 8]%asi, %l1			;\
	ldxa	[%sp + SPOFF + 16]%asi, %l2			;\
	ldxa	[%sp + SPOFF + 24]%asi, %l3			;\
	ldxa	[%sp + SPOFF + 32]%asi, %l4			;\
	ldxa	[%sp + SPOFF + 40]%asi, %l5			;\
	ldxa	[%sp + SPOFF + 48]%asi, %l6			;\
	ldxa	[%sp + SPOFF + 56]%asi, %l7			;\
	ldxa	[%sp + SPOFF + 64]%asi, %i0			;\
	ldxa	[%sp + SPOFF + 72]%asi, %i1			;\
	ldxa	[%sp + SPOFF + 80]%asi, %i2			;\
	ldxa	[%sp + SPOFF + 88]%asi, %i3			;\
	ldxa	[%sp + SPOFF + 96]%asi, %i4			;\
	ldxa	[%sp + SPOFF + 104]%asi, %i5			;\
	ldxa	[%sp + SPOFF + 112]%asi, %i6			;\
	ldxa	[%sp + SPOFF + 120]%asi, %i7			;\
	restored						;\
	add	%g1, 1, %g1					;\
	wrpr	%g1, %cwp


	
	
ENTRY(utl0)
	SAVE_GLOBALS(%l7)
	rd	%asi, %g1
	SAVE_OUTS(%l7)
	stx	%g1, [%l7 + TF_ASI]
	GET_PCPU_SCRATCH_SLOW(%g6)
	wrpr	%g0, PSTATE_KERNEL, %pstate	! enable ints

	brnz	%o1, common_utrap
	  nop		
	call	spinlock_enter
	  nop
common_uintr:	
	jmpl	%l3, %o7			! call interrupt handler
	  mov	%l7, %o0
	call	spinlock_exit
	  nop
	ba,pt	%xcc, user_rtt
	  nop
common_utrap:
	jmpl	%l3, %o7			! call trap handler / syscall
	  mov	%l7, %o0

	ldx	[PCPU_REG + PC_CURPCB], %g6
	ldx	[%g6 + PCB_KSTACK], %g6
	sub	%g6, TF_SIZEOF, %sp
	add	%sp, REGOFF + SPOFF, %l7
ENTRY(user_rtt)
	! pil handling needs to be re-visited
	wrpr	%g0, PIL_TICK, %pil
	ldx	[PCPU(CURTHREAD)], %l4
	lduw	[%l4 + TD_FLAGS], %l1
	ldx	[%l4 + TD_MD + MD_SAVED_PIL], %l0
	set	TDF_ASTPENDING | TDF_NEEDRESCHED, %l2
	and	%l1, %l2, %l1
	brz,a,pt %l1, 1f
	 nop

	! handle AST and retry return
	wrpr	%g0, %l0, %pil
	call	ast
	  mov   %l7, %o0
	ba,pt	%xcc, user_rtt
	 nop

1:	
	ldx	[PCPU_REG + PC_CURPCB], %g6
	ldx	[%g6 + PCB_NSAVED], %l1
	brz,a,pt %l1, 2f
	  nop
	wrpr	%g0, %l0, %pil
	mov	T_SPILL, %o1
	call	trap
	  mov	%l7, %o0
	ba,pt	%xcc, user_rtt
	 nop
2:

	ld	[%l7 + TF_WSTATE], %l3	
	!
	! restore user globals and outs
	!
	rdpr	%pstate, %l1
	ldx	[%l7 + TF_ASI], %g1
	wrpr	%l1, PSTATE_IE, %pstate
	wr	%g1, 0, %asi
	RESTORE_GLOBALS_USER(%l7)
	wrpr	%g0, 1, %gl
	RESTORE_OUTS(%l7)

	wrpr	%g0, 0, %pil	! drop pil to 0
	wrpr	%g0, 1, %tl	! raise tl -> 1 before setting pcontext
	
	mov	MMU_CID_S, %g1
	GET_MMU_CONTEXT(%g1, %g2)
	mov	MMU_CID_P, %g1
	sethi	%hi(FLUSH_ADDR), %g3
	SET_MMU_CONTEXT(%g1, %g2)
	flush	%g3				! flush required by immu
						! hangover from US I
	!
	! setup trap regs
	!

	ldx	[%l7 + TF_TPC], %g1
	ldx	[%l7 + TF_TNPC], %g2
	ldx	[%l7 + TF_TSTATE], %l0
	ldx	[%l7 + TF_FPRS], %l1

	wrpr	%g1, %tpc
	wrpr	%g2, %tnpc
	andn	%l0, TSTATE_CWP_MASK, %g6

	wr	%g0, FPRS_FEF, %fprs
	ldx	[%l7 + TF_FSR], %fsr
	wr	%l1, 0, %fprs	


		!
	! switch "other" windows back to "normal" windows and
	! restore to window we originally trapped in
	!
	rdpr	%otherwin, %g1
	wrpr	%g0, 0, %otherwin
	add	%l3, WSTATE_CLEAN_OFFSET, %l3	! convert to "clean" wstate
	wrpr	%g0, %l3, %wstate
	wrpr	%g0, %g1, %canrestore
	
	rdpr	%canrestore, %g1
	brnz	%g1, 3f
	  nop				! no trap, use restore directly
	rdpr	%cwp, %g1
	wrpr	%g1, %g6, %tstate	! needed by wbuf recovery code
	! hand craft the restore to avoid getting to TL > 2
	rdpr	%wstate, %g1
	btst	1, %g1
	beq	4f
	  nop
	.global	rtt_fill_start
rtt_fill_start:
#if 0
	fill_32bit_rtt(ASI_AIUP)
	ba,a	3f
#endif
4:
	membar	#Lookaside
	fill_64bit_rtt(ASI_AIUP)
	.global	rtt_fill_end
rtt_fill_end:
3:
	restore				! should not trap
2:
	
	!
	! set %cleanwin to %canrestore
	! set %tstate to the correct %cwp
	! retry resumes user execution
	!
	rdpr	%canrestore, %g1
	wrpr	%g0, %g1, %cleanwin
	rdpr	%cwp, %g1
	wrpr	%g1, %g6, %tstate
	retry
END(user_rtt)
END(utl0)		

ENTRY(ktl0)
	nop
	SAVE_GLOBALS(%l7)
	rd	%asi, %g1
	SAVE_OUTS(%l7)
	stx	%g1, [%l7 + TF_ASI]
	GET_PCPU_SCRATCH_SLOW(%g6)		! we really shouldn't need this ...	
	wrpr	%g0, PSTATE_KERNEL, %pstate	! enable interrupts

	brnz	%o1, common_ktrap
	  nop	
	call	spinlock_enter
	  nop
common_kintr:	
	jmpl	%l3, %o7			! call trap handler
	  mov	%l7, %o0
	call	spinlock_exit
	  nop
	b	common_rtt
	  nop
common_ktrap:
	jmpl	%l3, %o7			! call trap handler
	  mov	%l7, %o0
	
ENTRY(krtt)
common_rtt:	
	!
	! restore globals and outs
	!
	rdpr	%pstate, %l1
	ldx	[%l7 + TF_ASI], %g1
	wrpr	%l1, PSTATE_IE, %pstate
	wr	%g1, 0, %asi	

	RESTORE_GLOBALS_KERNEL(%l7)
	
	! switch to global set 1
	wrpr	%g0, 1, %gl
	RESTORE_OUTS(%l7)
#ifdef notyet
	!
	! set %pil from max(old pil, cur_thread_spl)
	!
	ldn	[%l0 + T_CPU], %l0
	ld	[%l0 + CPU_BASE_SPL], %l0
	cmp	%l6, %l0
	movg	%xcc, %l6, %l0
	wrpr	%g0, %l0, %pil
#endif
	GET_PCPU_SCRATCH
	ldx	[PCPU(CURTHREAD)], %l0
	ldx	[%l0 + TD_MD + MD_SAVED_PIL], %l0
	wrpr	%g0, %l0, %pil
	!
	! raise tl
	! setup trap regs
	! restore to window we originally trapped in
	!
	wrpr	%g0, 1, %tl
	
	ldx	[%l7 + TF_TSTATE], %l0
	ldx	[%l7 + TF_TPC], %g1
	ldx	[%l7 + TF_TNPC], %g2
	ldx	[%l7 + TF_FPRS], %l1
			
	andn	%l0, TSTATE_CWP_MASK, %g6
	wrpr	%g1, %tpc
	wrpr	%g2, %tnpc


	wr	%g0, FPRS_FEF, %fprs
	ldx	[%l7 + TF_FSR], %fsr
	wr	%l1, 0, %fprs	
	
	rdpr	%canrestore, %g1
	brnz	%g1, 3f
	  nop				! can use restore directly
	rdpr	%cwp, %g1
	wrpr	%g1, %g6, %tstate	! needed by wbuf recovery code

	! avoid going above TL2
	fill_64bit_rtt(ASI_N)

3:
	restore
	!
	! set %tstate to the correct %cwp
	!
	rdpr	%cwp, %g1
	wrpr	%g1, %g6, %tstate
	retry
END(krtt)	
END(ktl0)



ENTRY(tl0_ktrap)
	GET_PCPU_SCRATCH
        set     ktl0, %g6
	
	save	%sp, -(CCFSZ + TF_SIZEOF), %sp
		
	brz	%g2, 2f
	  nop
	or	%g2, T_KERNEL, %g2	
2:			
	! if the kwbuf is full we need to save to the stack now
	ld	[PCPU_REG + PC_KWBUF_FULL], %o0   
	brz,pt	%o0, 1f
	nop
	st	%g0, [PCPU_REG + PC_KWBUF_FULL]
	ldx	[PCPU_REG + PC_KWBUF_SP], %o1
	add	PCPU_REG, PC_KWBUF, %o0
	KWBUF64_TO_STACK(%o0, %o1, %o2)
1:			
	ba,a,pt %xcc, win_saved
END(tl0_ktrap)	
	

	! register convention:	
	! %g2=level %g1=mask
	
ENTRY(tl0_intr)
	SET(intr_handlers, %g7, %g6)
	sllx	%g4, IH_SHIFT, %g7
	ldx	[%g6 + %g7], %g1	! pointer to interrupt handler
	rdpr	%pil, %g5
	mov	%g5, %g4
				
	! %g1		pc of trap handler
	! %g2, %g3	args of trap handler
	! %g2           software trap type
	! %g3           additional argument to trap
	! %g4		desired pil
	! %g5, %g6	temps
	! %g7		saved

	! %l0, %l1	temps
	! %l3		saved %g1
	! %l4		flags
	! %l5		memory fault info
	! %l6		%pil for priv traps
	! %l7		trapframe

ENTRY(tl0_trap)
	/* if we're at tl2 we have some extra work to do */
	rdpr	%tl, %g5
	cmp	%g5, 2
	be,pn	%xcc, tl1_trap
	  nop
	
	rdpr	%tstate, %g5
	btst	TSTATE_PRIV, %g5
	and	%g5, TSTATE_CWP_MASK, %g6
	wrpr	%g0, %g6, %cwp
	bnz,pn	%xcc, tl0_ktrap
	  nop
ENTRY(tl0_utrap)
	GET_PCPU_SCRATCH

	and	%g2, TRAP_MASK, %g4
	cmp	%g4, UT_MAX
	bge,a,pt %xcc, tl0_skip_utrap
	  nop
	
	ldx	[PCPU(CURTHREAD)], %g5
	ldx	[%g5 + TD_PROC], %g5
	ldx	[%g5 + P_MD + MD_UTRAP], %g5
	brz,pn	%g5, tl0_skip_utrap
	  sllx	%g4, PTR_SHIFT, %g6
	ldx	[%g5 + %g6], %g5
	brz,pn	%g5, tl0_skip_utrap	
	  nop

	mov	%g4, %g2
	mov	%g5, %g4
	
	! 0) save trap state to memory	
	ldx	[PCPU_REG + PC_CURPCB], %g6	
	rdpr	%tstate, %g5
	stx	%g5, [%g6 + PCB_TSTATE]
	rdpr	%tpc, %g5
	stx	%g5, [%g6 + PCB_TPC]
	rdpr	%tnpc, %g5
	stx	%g5, [%g6 + PCB_TNPC]
	stx	%g2, [%g6 + PCB_TT]
	stx	%g3, [%g6 + PCB_SFAR]
	
	wrpr	%g4, %tnpc
	done
tl0_skip_utrap:	

#ifdef	notyet
	/* we need to determine from the hardware the number of register windows */
	sethi	%hi(nwin_minus_one), %g5
	ld	[%g5 + %lo(nwin_minus_one)], %g5
#else
	mov	nwin_minus_one, %g5
#endif
	ldx	[PCPU_REG + PC_CURPCB], %g6	
	wrpr	%g0, %g5, %cleanwin
	ldx	[%g6 + PCB_KSTACK], %g6
	brnz,pt	%g6, 5f
	  nop
	set	PCPU_PAGES*PAGE_SIZE - PC_SIZEOF, %g6
	add	%g7, %g6, %g6
	sub	%g6, SPOFF + CCFSZ, %g6
	mov	T_KSTACK_FAULT, %g2
	set	trap, %g3
5:	
	sub	%g6, TF_SIZEOF, %g6

	save	%g6, 0, %sp
	rdpr	%canrestore, %l0
	rdpr	%wstate, %l1
	wrpr	%g0, 0, %canrestore
	sllx	%l1, WSTATE_SHIFT, %l1
	wrpr	%l1, WSTATE_K64, %wstate
	wrpr	%g0, %l0, %otherwin
	!
	! set pcontext to run kernel
	!
	mov	KCONTEXT, %l0
	mov	MMU_CID_P, %l1
	sethi	%hi(FLUSH_ADDR), %l2
	SET_MMU_CONTEXT(%l1, %l0)
	flush	%l2			! flush/membar required by immu for 
					! consistency guarantee
	set	utl0, %g6
win_saved:
	mov	%g1, %l3		! set trap/interrupt for tl0
#ifdef TRAP_TRACING
	GET_PCPU_SCRATCH
	rdpr	%tl, %g1
	dec	%g1	
	sll	%g1, RW_SHIFT, %g1
	add	%g1, PC_TSBWBUF, %g1
	add	PCPU_REG, %g1, %g1
	wr	%g0, ASI_N, %asi
	TTRACE_ADD_SAFE(%g1, 0, 0, 0, 0, 0)
	mov	%l3, %g1
#endif	
	mov	%g2, %o1		! trap type
	mov	%g3, %o2		! fault info if set
	mov	%g5, %l6		! %pil if priv trap
	!
	! save state in trapframe
	!
	add	%sp, REGOFF + SPOFF, %l7
	rdpr	%tpc, %l0
	rdpr	%tnpc, %l1
	rdpr	%tstate, %l2
	stx	%l0, [%l7 + TF_TPC]
	rd	%fprs, %l0
	stx	%l1, [%l7 + TF_TNPC]
	stx	%l2, [%l7 + TF_TSTATE]
	stx	%l0, [%l7 + TF_FPRS]

	/*
	 * According to the sparc64 port fp must me enabled
	 * before reading %fsr
	 */
	wr	%g0, FPRS_FEF, %fprs
	stx	%fsr, [%l7 + TF_FSR]
	wr	%g0, 0, %fprs
	!
	!  setup pil
	!
	brlz,pt	%g4, 1f
	  nop
#if 0
#ifdef PMAP_DEBUG
	rdpr	%pil, %l0
	cmp	%g4, %l0
	bge,pt	%xcc, 10f
	  nop
	call	panic
10:	
#endif
#endif
	wrpr	%g0, %g4, %pil
1:		
	wrpr	%g0, %g6, %tnpc

	! save g7 before it can be overwritten by PCPU when returning from an interrupt 
	wrpr	%g0, 0, %gl
	stx	%g7, [%l7 + TF_G7]		
	wrpr	%g0, 1, %gl
	
	rdpr	%cwp, %l0
	set	TSTATE_KERNEL, %l1
	wrpr	%l1, %l0, %tstate
	done	
END(tl0_utrap)
END(tl0_trap)
END(tl0_intr)


/* 
 * workaround for CPP brokenness
 */
#define LOADLOAD #LoadLoad
#define LOADSTORE #LoadStore 
#define STORESTORE #StoreStore


#define WORKING
#ifdef WORKING
#define ENTER LOADLOAD
#define EXIT LOADSTORE|STORESTORE
#else
#define ENTER #Sync
#define EXIT  #Sync
#endif

#define THE_LOCK_ENTER(addr, lock_bit, oldval, newval, label1) \
	mov	1, lock_bit	; \
	add	addr, 8, addr	; \
	sllx	lock_bit, 56, lock_bit ; \
label1:				; \
	ldxa	[addr]%asi, oldval;	\
	or	oldval, lock_bit, newval; \
	andn	oldval, lock_bit, oldval; \
	casxa	[addr]%asi, oldval, newval; \
	cmp	newval, oldval	; \
	bne,pn	%xcc, label1 ## b	; \
 	  membar ENTER	; \
	sub	addr, 8, addr	;

#define THE_LOCK_EXIT(addr, lock_bit, tmp)\
	membar	EXIT ; \
	ldxa	[addr + 8]%asi, tmp ; \
	andn	tmp, lock_bit, tmp ; \
	stxa	tmp, [addr + 8]%asi ; 

#define HASH_LOOKUP(addr, tag, searchtag, faillabel, matchlabel) \
	ldda	[addr]%asi, tag	; \
	cmp	tag, %g0	; \
	be,pn	%xcc, faillabel	; \
	  nop			; \
	cmp	tag, searchtag	; \
	be,pn	%xcc, matchlabel ;\
	  nop

#define RESTORE_TRAPWIN(pcpu, cansave, label1, label2) \
	brz	cansave, label1 ## f; \
	  nop			; \
	restore			; \
	ba,a,pt	%xcc, label2 ## f ; \
label1:				; \
	rdpr	%tl, cansave	; \
	dec	cansave		; \
	sll	cansave, RW_SHIFT, cansave ; \
	add	cansave, PC_TSBWBUF, cansave ; \
	add	pcpu, cansave, cansave ; \
	RESTORE_LOCALS_ASI(cansave) ; \
label2:	

ENTRY(hash_bucket_lock)
	wr	%g0, ASI_N, %asi 
	rdpr	%pstate, %o1
	and	%o1, PSTATE_INTR_DISABLE, %o2
	wrpr	%o2, %pstate
	THE_LOCK_ENTER(%o0, %o3, %o4, %o5, 1)
	mov	%o1, %o0
	retl
	  nop
END(hash_bucket_lock)


ENTRY(hash_bucket_unlock)
	mov	1, %g2
	wr	%g0, ASI_N, %asi
	sllx	%g2, 56, %g2
	THE_LOCK_EXIT(%o0, %g2, %g3)
	wrpr	%o1, %pstate
	retl	  
	  nop
END(hash_bucket_unlock)

			
! %g3==trap type
! %g4==fault type (if data miss)
! %g5==fault addr
! internal usage:
! %g1==absolute index
! %g2==hash base, pointer to hash entry
! %g3==flag bits, TSB (RA)
! %g4==fault type,entry tag
! %g5==tag
! %g6==context
! %g7 temp
ENTRY(tsb_miss_handler)
	ldxa	[%g1 + %g7]ASI_REAL, %g6	! load in the context

	GET_HASH_SCRATCH_USER(%g2)
	GET_TSB_SCRATCH_USER(%g4)

	brnz,pn	%g6, 2f
	  nop	
	GET_HASH_SCRATCH_KERNEL(%g2)
	GET_TSB_SCRATCH_KERNEL(%g4)
2:

	rdpr	%tl, %g1			! need to use real addresses?
	mov	ASI_LDTD_N, %g3
	wr	%g0, ASI_N, %asi
	dec	%g1
	GET_PCPU_SCRATCH

	brz,pt	%g1, 3f			! for tl == 1
	  nop
	sethi	%uhi(VM_MIN_DIRECT_ADDRESS), %g1
	wr	%g0, ASI_REAL, %asi
	sllx	%g1, 32, %g1
	mov	ASI_LDTD_REAL, %g3
	andn	%g2, %g1, %g2
	andn	%g4, %g1, %g4
	andn	%g7, %g1, %g7
3:	
#ifdef notyet
	rdpr	%cansave, %g1
	/* XXX use save operation if %g1 > 0 and tl == 1 */
#endif
	rdpr	%tl, %g1
	dec	%g1	
	sll	%g1, RW_SHIFT, %g1
	add	%g1, PC_TSBWBUF, %g1
	add	PCPU_REG, %g1, %g1
#ifdef TRAP_TRACING	
	TTRACE_ADD_SAFE(%g1, 0, 0, 0, 0, 0)
#endif	
	SAVE_LOCALS_ASI(%g1)
	mov	0, %g1			! cansave is 0
	! %g1 == %cansave
	! %g2 == hash scratch value
	! %g3 == TWDW ASI
	! %g4 == TSB RA
	! %g5 == fault addr
	! %g6 == context

	srlx	%g5, TTARGET_VA_SHIFT, %l0
	sllx	%g6, TTARGET_CTX_SHIFT, %l1
	or	%l0, %l1, %l2			! %l2 == search tag

tsb_miss_compute_hash_addr:
	sethi	%hi(PAGE_SIZE), %l0
	sub	%l0, 1, %l1			! %l1==PAGE_MASK

	and	%g2, %l1, %l3			! size stored in lower 13 bits
	andn	%g2, %l1, %g2			! actual VA/RA of hash

	! XXX only handle 8k page miss
	! calculate hash index
	srlx	%g5, PAGE_SHIFT, %l4		! absolute hash index
	sllx	%l3, (PAGE_SHIFT - THE_SHIFT), %l0 ! size of hash in THEs
	sub	%l0, 1, %l5			! THE_MASK
	and	%l4, %l5, %l5			! masked hash index
	sllx	%l5, THE_SHIFT, %l5		! masked hash offset
	! fetch hash entries - exit when we find what were looking for 

	! %g2==entry base
	add	%g2, %l5, %g2			! base + offset == entry base
	

	THE_LOCK_ENTER(%g2, %l0, %l7, %l6, 6)
	
	! %g1 == cansave
	! %g2 == THE
	! %g3 == TWDW ASI
	! %g4 == TSB RA
	! %g5 == fault addr
	! %g6 == context
	! %g7 == PCPU_REG
	! %l0 == VTD_LOCK
	! %l1 == PAGE_MASK
	! %l2 == search tag
	! %l4 == absolute index

	! %l3 == ASI
	! %l5 == saved head of bucket
	! %l6 == tag
	! %l7 == data

	rd	%asi, %l3
	wr	%g0, %g3, %asi
	mov	%g2, %l5			! save head of bucket
	rdpr	%tt, %g3			! reload trap type

tsb_miss_lookup_0:
	HASH_LOOKUP(%g2, %l6, %l2, tsb_miss_not_found, tsb_miss_found)
tsb_miss_lookup_1:
	add	%g2, 16, %g2
	HASH_LOOKUP(%g2, %l6, %l2, tsb_miss_not_found, tsb_miss_found)
tsb_miss_lookup_2:
	add	%g2, 16, %g2
	HASH_LOOKUP(%g2, %l6, %l2, tsb_miss_not_found, tsb_miss_found)
#if HASH_ENTRY_SHIFT > 2 
tsb_miss_lookup_3:
	add	%g2, 16, %g2
	HASH_LOOKUP(%g2, %l6, %l2, tsb_miss_not_found, tsb_miss_found)
tsb_miss_lookup_4:
	add	%g2, 16, %g2
	HASH_LOOKUP(%g2, %l6, %l2, tsb_miss_not_found, tsb_miss_found)
tsb_miss_lookup_5:
	add	%g2, 16, %g2
	HASH_LOOKUP(%g2, %l6, %l2, tsb_miss_not_found, tsb_miss_found)
tsb_miss_lookup_6:
	add	%g2, 16, %g2
	HASH_LOOKUP(%g2, %l6, %l2, tsb_miss_not_found, tsb_miss_found)
#endif			
tsb_miss_collision:	
	add	%g2, 16, %g2
	ldda	[%g2]%asi, %l6
	
	sethi	%uhi(VM_MIN_DIRECT_ADDRESS), %g3
	cmp	%l3, ASI_N
	sllx	%g3, 32, %g3
	beq,pt	%xcc, 7f
	  nop
	andn	%l7, %g3, %l7			! generate real address
7:
	srl	%l6, 0, %l6
	sethi	%hi(0xcafebabe), %g3
	mov	%l7, %g2
	or	%g3, %lo(0xcafebabe), %g3
	cmp	%g3, %l6
	rdpr	%tt, %g3	
	beq,pt %xcc, tsb_miss_lookup_0
	  nop

tsb_miss_not_found:	
	! we need to jump to tl0_trap to drop us back down to tl0
	! and take us to trap(...) to service the fault
	wr	%g0, %l3, %asi
	THE_LOCK_EXIT(%l5, %l0, %g2)

	andn	%g5, %l1, %g5			! fault page PA

	RESTORE_TRAPWIN(PCPU_REG, %g1, 14, 15)

	! convert hardware trap type to kernel trap type
	set	trap_conversion, %g2
	sllx	%g3, INT_SHIFT, %g3
	ld	[%g2 + %g3], %g2
	
        sethi	%hi(trap), %g1
	or	%g6, %g5, %g3			! trap data
	sub	%g0, 1, %g4			! pil info
	ba	%xcc, tl0_trap
	  or	%g1, %lo(trap), %g1

tsb_miss_found:

	wr	%g0, %l3, %asi
	cmp	%g3, TT_DATA_MISS		! TSB data miss
	be,pt	%xcc, 9f
	  or	%l7, VTD_REF, %l7		! set referenced unconditionally
	cmp	%g3, TT_INSTRUCTION_MISS	! TSB instruction miss
	be,pt	%xcc, 9f
	  nop
	cmp	%g3, TT_DATA_PROTECTION		! protection fault
	bne,pn	%xcc, unsupported_fault_trap	! we don't handle any other fault types currently
	  nop
	andcc	%l7, VTD_SW_W, %g0		! write enabled?
	bz,a,pn	%xcc, prot_fault_trap		! write to read only page
	  nop
	or	%l7, VTD_W, %l7			! add modifed bit 
9:	

	andn	%l7, %l0, %l7			! remove lock bit
	
        and	%g4, %l1, %g3			! shift of TSB in pages
	andn	%g4, %l1, %l3			! TSB real address

	mov	1, %l2	
	add	%g3, (PAGE_SHIFT - TTE_SHIFT), %g3 ! add shift value for number of ttes / page
	sllx	%l2, %g3, %g3			! nttes

	subx	%g3, 1, %g3			! TSB_MASK
	and	%g3, %l4, %g3			! masked index
	sllx	%g3, TTE_SHIFT, %g3		! masked byte offset
	add	%g3, %l3, %g3			! TTE RA

	rdpr	%tl, %l2			! don't track misses for kernel context
	brz,pn	%g6, 13f
	  dec	%l2
	brnz,pn	%l2, 13f			! don't track misses at tl > 1
	  nop
 	ldx	[PCPU(PMAP)], %l1
	ld	[%l1 + PM_TSB_MISS_COUNT], %l3
	add	1, %l3, %l3
	st	%l3, [%l1 + PM_TSB_MISS_COUNT]

	!	if the data value is zero then this is not a capacity miss
	ldda	[%g3]ASI_LDTD_N, %l2
	brz,pt	%l3, 13f
	  nop
	!	compare the tag bits - if they're the same we're merely 
	!	changing the mapping
	brz,pt	%l3, 13f
	  nop
	cmp	%l6, %l2
	beq,pt	%xcc, 13f
	  nop
			
	ld	[%l1 + PM_TSB_CAP_MISS_COUNT], %l3
	add	1, %l3, %l3
	st	%l3, [%l1 + PM_TSB_CAP_MISS_COUNT]

13:	stxa	%g0, [%g3 + 8]%asi		! invalidate data
	membar	#StoreStore

	stxa	%l6, [%g3]%asi			! store tag
	stxa	%l7, [%g3 + 8]%asi		! store data
	stxa	%l7, [%g2 + 8]%asi		! update TTE with ref bit  
	membar	#StoreLoad
	
	THE_LOCK_EXIT(%l5, %l0, %l7)
	RESTORE_TRAPWIN(PCPU_REG, %g1, 13, 16)	
upgrade_demap:
	rdpr	%tt, %g3
	cmp	%g3, TT_DATA_PROTECTION
	beq,pn	%xcc, demap_begin
	  sethi	%hi(PAGE_SIZE), %g1  
	retry
demap_begin:
	dec	%g1
	mov	%o0, %g1
	mov	%o1, %g2
	mov	%o2, %g3
	mov	MAP_DTLB, %o2
	mov	%g5, %o0
	mov	%g6, %o1							
	ta	MMU_UNMAP_ADDR
	mov	%g1, %o0
	mov	%g2, %o1
	mov	%g3, %o2
	retry
END(tsb_miss_handler)


/*
 * Write to read-only page
 */
! %g1 == cansave
! %g4 == tag
! %g5 == fault addr
! %g6 == context
! %l0 == VTD_LOCK
! %l5 == head of bucket
	
ENTRY(prot_fault_trap)
	THE_LOCK_EXIT(%l5, %l0, %g2)
	RESTORE_TRAPWIN(PCPU_REG, %g1, 14, 15)
        sethi	%hi(trap), %g1
	mov	T_DATA_PROTECTION, %g2			
	or 	%g5, %g6, %g3
	sub	%g0, 1, %g4
	ba	%xcc, tl0_trap
	  or	%g1, %lo(trap), %g1
END(prot_fault_trap)
/*
 * Programming error
 */
ENTRY(unsupported_fault_trap)
	add	0x20, %g3, %g1
	PUTCHAR(0x5b)
	PUTCHAR(%g1)
	PUTCHAR(0x5d)
END(unsupported_fault_trap)


/*
 * Freshly forked processes come here when switched to for the first time.
 * The arguments to fork_exit() have been setup in the locals, we must move
 * them to the outs.
 */
ENTRY(fork_trampoline)
	mov	%l0, %o0
	mov	%l1, %o1
	call	fork_exit
	  mov	%l2, %o2
	ba,pt	%xcc, user_rtt
	  add     %sp, CCFSZ + SPOFF, %l7
END(fork_trampoline)



	.comm	intrnames, IV_NAMLEN
	.comm	eintrnames, 0

	.comm	intrcnt, IV_MAX * 8
	.comm	eintrcnt, 0

#define	TRAP_ENTRY_SHIFT	5
#define	TRAP_ENTRY_MASK		0x1ff
ENTRY(tl1_trap)
	! assume no tl1 handler   
	rdpr	%tpc, %g7

	set	rtt_fill_start, %g6
	cmp	%g7, %g6
	blu,pn	%xcc, 1f
	 .empty
	set	rtt_fill_end, %g6
	cmp	%g7, %g6
	bgeu,pn %xcc, 1f
	 nop
	set	fault_rtt_fn1, %g7
	ba,a,pt	%xcc, 4f
1:
#ifdef USING_OPTIMIZED_SPILL_FILL
	set	fill_slow_start, %g6
	cmp	%g7, %g6
	bleu,a,pn %xcc, 2f
	  nop
	set	fill_slow_end, %g6
	cmp	%g7, %g6
	blu,a,pn %xcc, 3f
	  nop
#endif
2:		
	set	tl1_end, %g6
	cmp	%g7, %g6
	bgeu,a,pn %xcc, ptl1_panic
	  mov	PTL1_BAD_TRAP, %g1
	! tpc is in the trap table
	! convert to trap index
	srl	%g7, TRAP_ENTRY_SHIFT, %g6
	and	%g6, TRAP_ENTRY_MASK, %g6
	! check for window trap type
	and	%g6, WTRAP_TTMASK, %g6
	cmp	%g6, WTRAP_TYPE
	bne,a,pn %xcc, ptl1_panic
	  mov	PTL1_BAD_NOT_WTRAP, %g1
3:		
	andn	%g7, WTRAP_ALIGN, %g7
	add	%g7, WTRAP_FAULTOFF, %g7
4:	
	wrpr	%g0, %g7, %tnpc
	wrpr	%g0, 1, %gl
	rdpr	%tt, %g5
	GET_MMFSA_SCRATCH(%g7)
	wr	%g0, ASI_REAL, %asi
	ldxa	[%g7 + MMFSA_D_ADDR]%asi, %g6
	ldxa	[%g7 + MMFSA_D_CTX]%asi, %g7
	cmp	%g5, T_ALIGNMENT
	be,pn	%xcc, 5f
	  nop
	srlx	%g6, PAGE_SHIFT, %g6 
	sllx	%g6, PAGE_SHIFT, %g6	! mask off bottom
	or	%g6, %g7, %g6
	done
5:
	sllx	%g7, TRAP_CTX_SHIFT, %g7
	or	%g7, %g5, %g5
	done
END(tl1_trap)
