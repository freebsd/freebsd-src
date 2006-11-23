/*-
 * Copyright (c) 2002 Jake Burkholder.
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
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

#include "opt_simulator.h"
#include "opt_trap_trace.h"
	
#include <machine/hypervisorvar.h>
#include <machine/asi.h>
#include <machine/asmacros.h>
#include <machine/ktr.h>
#include <machine/pstate.h>

#include "assym.s"


#define PUTCHAR(x) \
0:	mov	x, %o0		; \
	mov	CONS_PUTCHAR, %o5	; \
	ta	FAST_TRAP	; \
	brnz	%o0, 0b		; \
	  nop
		
ENTRY(intr_fast)
	save	%sp, -CCFSZ, %sp
1:	ldx	[PCPU(IRHEAD)], %l0
	brnz,a,pt %l0, 2f
	 nop

	ret
	 restore

2:	wrpr	%g0, PSTATE_NORMAL, %pstate

	ldx	[%l0 + IR_NEXT], %l1
	brnz,pt	%l1, 3f
	 stx	%l1, [PCPU(IRHEAD)]
	PCPU_ADDR(IRHEAD, %l1)
	stx	%l1, [PCPU(IRTAIL)]

3:	ldx	[%l0 + IR_FUNC], %o0
	ldx	[%l0 + IR_ARG], %o1
	lduw	[%l0 + IR_VEC], %o2

	/* intrcnt[intr_countp[%o2]]++ */
	SET(intrcnt, %l7, %l2)		/* %l2 = intrcnt */
	prefetcha [%l2] ASI_N, 1
	SET(intr_countp, %l7, %l3)	/* %l3 = intr_countp */
	sllx	%o2, 1, %l4		/* %l4 = vec << 1 */
	lduh	[%l4 + %l3], %l5	/* %l5 = intr_countp[%o2] */
	sllx	%l5, 3, %l6		/* %l6 = intr_countp[%o2] << 3 */
	add	%l6, %l2, %l7		/* %l7 = intrcnt[intr_countp[%o2]] */
	ldx	[%l7], %l2
	inc	%l2
	stx	%l2, [%l7]

	ldx	[PCPU(IRFREE)], %l1
	stx	%l1, [%l0 + IR_NEXT]
	stx	%l0, [PCPU(IRFREE)]

	wrpr	%g0, PSTATE_KERNEL, %pstate

	call	%o0
	 mov	%o1, %o0

	ba,a	%xcc, 1b
	 nop
END(intr_fast)

/*
 * Running tally of invalid CPU mondo interrupts
 */
#if defined(lint)
uint64_t cpu_mondo_invalid;
#else /* lint */
        .data
        .globl cpu_mondo_invalid
        .align  8
cpu_mondo_invalid:
        .skip   8

        .text
#endif  /* lint */

#if defined(lint)
void
cpu_mondo(void)
{}
#else   /* lint */

/*
 * (TT 0x7c, TL>0) CPU Mondo Queue Handler
 *      Globals are the Interrupt Globals.
 *
 * Interrupts in sun4v are delivered to privileged code in the form 
 * of interrupt reports.  Each interrupt report is 64-bytes long, 
 * consisting of 8 64-bit words.  Each interrupt report is appended 
 * onto the appropriate interrupt queue of which there are currently two,
 * one for CPU mondos (formerly referred to as cross trap function
 * requests) and one for device mondos.  Each queue has its own 
 * trap vector.
 *
 * New reports are appended onto the tail of the queue, and privileged
 * code reads the report from the queue's head.  The head pointer is stored
 * in a register and is equal to the current offset from the base address 
 * of its associated queue.  
 *
 * The structure of the cpu mondo report follows sun4u conventions:
 *
 * word 0:	address of cross-trap handler
 * word 1:	first argument to handler
 * word 2:	second argument to handler
 * word 3-7:	unused
 *
 * Privileged code is responsible for incrementing the head pointer 
 * to remove each entry.  The pointers are updated using modulo
 * arithmetic such that the queue is empty when head == tail and is full
 * when the addition of an entry would make head == tail. 
 * 
 * This trap handler is called when the cpu mondo queue becomes non-empty
 * and will continue to be called while interrupts enabled until the 
 * queue becomes empty.
 *
 */
ENTRY(cpu_mondo)
#ifdef TRAP_TRACING	
	GET_PCPU_PHYS_SCRATCH(%g1)
	rdpr	%tl, %g1
	dec	%g1	
	sll	%g1, RW_SHIFT, %g1
	add	%g1, PC_TSBWBUF, %g1
	add	PCPU_REG, %g1, %g1
	wr	%g0, ASI_REAL, %asi
	TTRACE_ADD_SAFE(%g1, 0, 0, 0, 0, 0)
#endif	
        !
        !       Register Usage:-
        !       %g5     PC for fasttrap TL>0 handler
        !       %g1     arg 1   
        !       %g2     arg 2
        !       %g3     queue base RA, ackmask
        !       %g4     queue size mask
        !       %g6     head ptr
        mov     CPU_MONDO_QUEUE_HEAD, %g1     
        ldxa    [%g1]ASI_QUEUE, %g6     ! %g6 = head ptr 
        mov     CPU_MONDO_QUEUE_TAIL, %g2     
        ldxa    [%g2]ASI_QUEUE, %g4     ! %g4 = tail ptr 
        cmp     %g6, %g4
        be,pn   %xcc, 0f                ! head == tail
         nop

	/*
	 * Get the address of the current CPU and index into
	 * the pcpu structure for the Q values.
	 */
	GET_PCPU_SCRATCH
	ldx	[PCPU(CPU_Q_RA)], %g3		! %g3 = queue base PA
	ldx	[PCPU(CPU_Q_SIZE)], %g4		! %g4 = queue size
        sub     %g4, 1, %g4             	! %g4 = queue size mask 

        ! Load interrupt receive data registers 1 and 2 to fetch
        ! the arguments for the fast trap handler.
        !
        ! Since the data words in the interrupt report are not defined yet 
        ! we assume that the consective words contain valid data and preserve
        ! sun4u's xcall mondo arguments. 
        ! Register usage:
        !       %g5     PC for fasttrap TL>0 handler
        !       %g1     arg 1   
        !       %g2     arg 2   

        ldxa    [%g3 + %g6]ASI_REAL, %g5 	! get PC from q base + head
        add     %g6, 0x8, %g6           	! inc head 8 bytes
        ldxa    [%g3 + %g6]ASI_REAL, %g1 	! read data word 1
        add     %g6, 0x8, %g6           	! inc head 8 bytes
        ldxa    [%g3 + %g6]ASI_REAL, %g2 	! read data word 2
        add     %g6, 0x8, %g6           	! inc head 8 bytes
        ldxa    [%g3 + %g6]ASI_REAL, %g7 	! read data word 3
        add     %g6, (INTR_REPORT_SIZE - 24) , %g6 ! inc head to next record

        and     %g6, %g4, %g6           ! and size mask for wrap around 
        mov     CPU_MONDO_QUEUE_HEAD, %g3     
        stxa    %g6, [%g3]ASI_QUEUE     ! store head pointer 
        membar  #Sync
	
	mov	%g7, %g3		! ackmask
        /*
         * For now catch invalid PC being passed via cpu_mondo queue
         */
        set     KERNBASE, %g4
        cmp     %g5, %g4
        bl,a,pn %xcc, 1f                ! branch if bad %pc
          nop

        jmp     %g5                     ! jump to traphandler
          nop
1:
        ! invalid trap handler, discard it for now
        set     cpu_mondo_invalid, %g4
        ldx     [%g4], %g5
        inc     %g5
        stx     %g5, [%g4]
0:
        retry
        /* NOTREACHED */
END(cpu_mondo)

#endif /* lint */


#if defined(lint)
void
dev_mondo(void)
{}
#else   /* lint */

/*
 * (TT 0x7d, TL>0) Dev Mondo Queue Handler
 *      Globals are the Interrupt Globals.
 *
 * Device interrupt reports take the following form:
 * Bits:   	63		11 10		6 5		0
 * word 0: 	|   zero	 | |  bus port  | | device ID 	|
 * word 1-7: 			unused
 *
 */
ENTRY(dev_mondo)
#ifdef TRAP_TRACING	
	GET_PCPU_PHYS_SCRATCH(%g1)
	rdpr	%tl, %g1
	dec	%g1	
	sll	%g1, RW_SHIFT, %g1
	add	%g1, PC_TSBWBUF, %g1
	add	PCPU_REG, %g1, %g1
	wr	%g0, ASI_REAL, %asi
	TTRACE_ADD_SAFE(%g1, 0, 0, 0, 0, 0)
#endif		
        /*
         *       Register Usage:-
         *       %g5     inum
         *       %g1     ptr to intrerrupt vector entry for inum
         *       %g3     queue base PA 
         *       %g4     queue size mask
         *       %g6     head ptr
	 */
!	mov %o0, %g1
!	mov %o5, %g6
!	PUTCHAR(0x2b)	
!	PUTCHAR(0x2b)	
!	mov %g1, %o0
!	mov %g6, %o5	
		
        mov     DEV_MONDO_QUEUE_HEAD, %g1     
        ldxa    [%g1]ASI_QUEUE, %g6     	! %g6 = head ptr 
        mov     DEV_MONDO_QUEUE_TAIL, %g2     
        ldxa    [%g2]ASI_QUEUE, %g4     	! %g4 = tail ptr 
        cmp     %g6, %g4
        be,pn   %xcc, 0f                	! head == tail
        nop

	/*
	 * Get CPU address and index to Q base
	 */
	GET_PCPU_SCRATCH
        ldx     [PCPU(DEV_Q_RA)], %g3  		! %g3 = queue base PA

        /*
	 *  Register usage:
         *       %g5 - inum
         */
        ldxa    [%g3 + %g6]ASI_REAL, %g5 	! %g5 = inum from q base + head

        /*
         * We verify that inum is valid ( < IV_MAX). If it is greater
         * than IV_MAX, we let the software interrupt handler take care
	 * of it.
	 */
        set     IV_MAX, %g4
        cmp     %g5, %g4
        bgeu,a,pn       %xcc, 1f
          ldx     [PCPU(DEV_Q_SIZE)], %g4		! queue size (delay slot)

        /*
	 * Find the function, argument and desired priority from the
	 * intr_vectors table
	 */
        set     intr_vectors, %g1
        sll     %g5, IV_SHIFT, %g2
        add     %g1, %g2, %g1                   ! %g1 = &intr_vectors[inum]

        /*
         * Get an intr_request from the free list.  There should always be one
         * unless we are getting an interrupt storm from stray interrupts, in
         * which case the we will dereference a NULL pointer and panic.
         */
	ldx	[PCPU(IRFREE)], %g2		! %g2 = ptr to tail element

	/*
	 * Sun4v-fixme: It might be better to have the software interrupt
	 * handler deal with this scenario rather than just panic.
	 */
	!brz,a,pt        %g2, 1f
	!ldx     [PCPU(DEV_Q_SIZE)], %g4	! queue size (delay slot)

	ldx	[%g2 + IR_NEXT], %g3		! %g3 = next element in list
	stx	%g3, [PCPU(IRFREE)]		! %g3 is now the tail

	/*
	 * Store the vector number, function, argument, and priority.
	 */
	stw	%g5, [%g2 + IR_VEC]
	ldx     [%g1 + IV_FUNC], %g3		! %g3 = function ptr
	stx	%g3, [%g2 + IR_FUNC]
	ldx	[%g1 + IV_ARG], %g3		! %g3 = argument ptr
	stx	%g3, [%g2 + IR_ARG]
	lduw	[%g1 + IV_PRI], %g1		! %g1 = priority
	stw	%g1, [%g2 + IR_PRI]

	/*
	 * Link it onto the end of the active list
	 */
	stx	%g0, [%g2 + IR_NEXT]
	ldx	[PCPU(IRTAIL)], %g3		! %g3 = ptr to tail
	stx	%g2, [%g3]
	add	%g2, IR_NEXT, %g2
	stx	%g2, [PCPU(IRTAIL)]
	
 	/* 
	 * Unlike Solaris, FreeBSD throws away the rest of the 
	 * 64-byte packet.  We may want to revisit this philosophy
	 * at some point since information in the packet is defined by 
	 * the device and MAY be meaningful.
	 *
	 * For now, we retain FreeBSD's sun4u semantics.
	 */
        ldx     [PCPU(DEV_Q_SIZE)], %g4		! queue size

1:      sub     %g4, 1, %g4             	! %g4 = queue size mask 
        add     %g6, INTR_REPORT_SIZE, %g6 	! inc head to next record   
        and     %g6, %g4, %g6           	! and mask for wrap around      
        mov     DEV_MONDO_QUEUE_HEAD, %g3     
        stxa    %g6, [%g3]ASI_QUEUE     	! increment head offset 
        membar  #Sync

	/*
	 * Trigger a softint at the level indicated by the priority
	 */
	mov	1, %g6
	sllx	%g6, %g1, %g6
	wr	%g6, 0, %set_softint
#if 0
	mov %o0, %g1
	mov %o5, %g6
	PUTCHAR(0x2b)	
	PUTCHAR(0x2b)	
	mov %g1, %o0
	mov %g6, %o5	
#endif
	/*
	 * Done, retry the instruction	
	 */
0:      retry 

	
        /* NOTREACHED */
END(dev_mondo)
#endif /* lint */

ENTRY(tl_invlctx)
	mov	%o0, %g2
	mov	%o1, %g4
	mov	%o2, %g5
	mov	%o3, %g6
	mov	%o5, %g7
	mov	%g0, %o0
	mov	%g0, %o1
	mov	%g1, %o2
	mov	MAP_ITLB|MAP_DTLB, %o3
	mov	MMU_DEMAP_CTX, %o5
	ta	FAST_TRAP
	brz,pt	%o0, 1f
	  nop
	ba	interrupt_panic_bad_hcall
	  mov	MMU_DEMAP_CTX, %o1
1:	
	mov	%g2, %o0
	mov	%g4, %o1
	mov	%g5, %o2
	mov	%g6, %o3
	mov	%g7, %o5
	ba,a,pt	%xcc, set_ackmask
	  nop
END(tl_invlctx)

ENTRY(tl_invltlb)
	mov	%o0, %g1
	mov	%o1, %g2
	mov	%o2, %g4
	mov	%o5, %g5
	
	mov	%g0, %o0
	mov	%g0, %o1
	mov	MAP_ITLB | MAP_DTLB, %o2
	mov	MMU_DEMAP_ALL, %o5
	ta	FAST_TRAP
	brz,pt	%o0, 1f
	  nop
	ba	interrupt_panic_bad_hcall
	   mov	MMU_DEMAP_ALL, %o1
1:
	mov	%g1, %o0
	mov	%g2, %o1
	mov	%g4, %o2
	mov	%g5, %o5
	
	ba,a,pt	%xcc, set_ackmask
	  nop
END(tl_invltlb)

ENTRY(tl_invlpg)
	mov	%o0, %g5
	mov	%o1, %g6
	mov	%o2, %g7
	mov	MAP_ITLB|MAP_DTLB, %o2
	mov	%g1, %o0
	mov	%g2, %o1							
	ta	MMU_UNMAP_ADDR
	brz,pt	%o0, 1f
	  nop
	ba	interrupt_panic_bad_hcall
	   mov	MMU_UNMAP_ADDR, %o1
1:
	mov	%g5, %o0
	mov	%g6, %o1
	mov	%g7, %o2
	ba,a,pt	%xcc, set_ackmask
	  nop
END(tl_invlpg)

ENTRY(tl_tsbupdate)
/* compare current context with one to be updated */
	mov	MMU_CID_S, %g4
	GET_MMU_CONTEXT(%g4, %g4)
	
	cmp	%g1, %g4
	bne,a,pt %xcc, set_ackmask			
	  nop
/*  update scratch pointer to tsbscratch */
	wr	%g0, ASI_REAL, %asi	
	ldxa	[%g2]%asi, %g4		! tsbscratch
	ldxa	[%g2 + 8]%asi, %g2	! tsb_ra
	mov	SCRATCH_REG_TSB_USER, %g5
	SET_SCRATCH(%g5, %g4)
/* reset tsb context with hypervisor */

	
	mov	%o0, %g1
	mov	%o1, %g2
	mov	%o5, %g4
	mov	1, %o0
	mov	%g2, %o1
	mov	MMU_TSB_CTXNON0, %o5
	ta	FAST_TRAP
	/* XXX test %o0 */
	mov	%g4, %o5
	mov	%g2, %o1
	mov	%g1, %o0	
	ba,a,pt	%xcc, set_ackmask
	  nop
END(tl_tsbupdate)

ENTRY(tl_ttehashupdate)
/* compare current context with one to be updated */
	mov	MMU_CID_S, %g4
	GET_MMU_CONTEXT(%g4, %g4)
	cmp	%g1, %g4
	bne,a,pt %xcc, set_ackmask
	  nop
/*  update scratch pointer to hashscratch */
	mov	SCRATCH_REG_HASH_USER, %g4
	SET_SCRATCH(%g4, %g2)
END(tl_ttehashupdate)
		
ENTRY(set_ackmask)
	membar	#Sync
	GET_PCPU_PHYS_SCRATCH(%g6)
	wr	%g0, ASI_REAL, %asi

#ifdef TRAP_TRACING
	/* pcpu->pad[0] = %pc */
	rd	%pc, %g4
	stxa	%g4, [PCPU(PAD)]%asi

	/* pcpu->pad[1] = %tick */
	rdpr	%tick, %g4
	stxa    %g4, [PCPU(PAD) + 8]%asi

	/*
	 * Increment a counter which might help us notice if we're
	 * stuck in a loop. pcpu->pad[2] = count
	 */
	ldxa	[PCPU(PAD) + 16]%asi, %g4
	add	%g4, 1, %g4
	stxa    %g4, [PCPU(PAD) + 16]%asi
#endif

	lda	[PCPU(CPUMASK)]%asi, %g4
	lda	[%g3]%asi, %g1
1:	or	%g1, %g4, %g2
	casa	[%g3]%asi, %g1, %g2
	cmp	%g1, %g2
	bne,a,pn %icc, 1b
	  lda	[%g3]%asi, %g1
	retry
END(set_ackmask)

	/*
	 * interrupt_panic_bad_hcall is called when a hcall returns
	 * unexpected error
	 * %o0 error number
	 * %o1 hcall number
	 */

	.text
interrupt_bad_hcall_error:
	.asciz	"hypervisor call 0x%x returned an unexpected error %d"

	
ENTRY(interrupt_panic_bad_hcall)
	PUTCHAR(0x5b)
	PUTCHAR(0x5b)
	PUTCHAR(0x5b)
	PUTCHAR(0x5b)
	PUTCHAR(0x5b)
	mov	%o0, %o2
	sethi	%hi(interrupt_bad_hcall_error), %o0
	or	%o0, %lo(interrupt_bad_hcall_error), %o0
	mov	%o7, %o3
	call	panic
	  mov	%o3, %o7
END(interrupt_panic_bad_hcall)
