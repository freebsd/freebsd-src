/*
 *	from: vector.s, 386BSD 0.1 unknown origin
 * $FreeBSD$
 */


#include <machine/apic.h>
#include <machine/smp.h>

#include "i386/isa/intr_machdep.h"

/* convert an absolute IRQ# into a bitmask */
#define IRQ_BIT(irq_num)	(1 << (irq_num))

/* make an index into the IO APIC from the IRQ# */
#define REDTBL_IDX(irq_num)	(0x10 + ((irq_num) * 2))

/*
 * 
 */
#define PUSH_FRAME							\
	pushl	$0 ;		/* dummy error code */			\
	pushl	$0 ;		/* dummy trap type */			\
	pushal ;							\
	pushl	%ds ;		/* save data and extra segments ... */	\
	pushl	%es ;							\
	pushl	%fs

#define POP_FRAME							\
	popl	%fs ;							\
	popl	%es ;							\
	popl	%ds ;							\
	popal ;								\
	addl	$4+4,%esp

/*
 * Macros for interrupt entry, call to handler, and exit.
 */

#define	FAST_INTR(irq_num, vec_name)					\
	.text ;								\
	SUPERALIGN_TEXT ;						\
IDTVEC(vec_name) ;							\
	PUSH_FRAME ;							\
	movl	$KDSEL,%eax ;						\
	mov	%ax,%ds ;						\
	mov	%ax,%es ;						\
	movl	$KPSEL,%eax ;						\
	mov	%ax,%fs ;						\
	FAKE_MCOUNT(13*4(%esp)) ;					\
	incb	_intr_nesting_level ;					\
	pushl	_intr_unit + (irq_num) * 4 ;				\
	call	*_intr_handler + (irq_num) * 4 ; /* do the work ASAP */ \
	addl	$4, %esp ;						\
	movl	$0, lapic_eoi ;						\
	lock ; 								\
	incl	_cnt+V_INTR ;	/* book-keeping can wait */		\
	movl	_intr_countp + (irq_num) * 4, %eax ;			\
	lock ; 								\
	incl	(%eax) ;						\
	MEXITCOUNT ;							\
	jmp	doreti_next

#define IOAPICADDR(irq_num) CNAME(int_to_apicintpin) + 16 * (irq_num) + 8
#define REDIRIDX(irq_num) CNAME(int_to_apicintpin) + 16 * (irq_num) + 12
	
#define MASK_IRQ(irq_num)						\
	IMASK_LOCK ;				/* into critical reg */	\
	testl	$IRQ_BIT(irq_num), _apic_imen ;				\
	jne	7f ;			/* masked, don't mask */	\
	orl	$IRQ_BIT(irq_num), _apic_imen ;	/* set the mask bit */	\
	movl	IOAPICADDR(irq_num), %ecx ;	/* ioapic addr */	\
	movl	REDIRIDX(irq_num), %eax ;	/* get the index */	\
	movl	%eax, (%ecx) ;			/* write the index */	\
	movl	IOAPIC_WINDOW(%ecx), %eax ;	/* current value */	\
	orl	$IOART_INTMASK, %eax ;		/* set the mask */	\
	movl	%eax, IOAPIC_WINDOW(%ecx) ;	/* new value */		\
7: ;						/* already masked */	\
	IMASK_UNLOCK
/*
 * Test to see whether we are handling an edge or level triggered INT.
 *  Level-triggered INTs must still be masked as we don't clear the source,
 *  and the EOI cycle would cause redundant INTs to occur.
 */
#define MASK_LEVEL_IRQ(irq_num)						\
	testl	$IRQ_BIT(irq_num), _apic_pin_trigger ;			\
	jz	9f ;				/* edge, don't mask */	\
	MASK_IRQ(irq_num) ;						\
9:


#ifdef APIC_INTR_REORDER
#define EOI_IRQ(irq_num)						\
	movl	_apic_isrbit_location + 8 * (irq_num), %eax ;		\
	movl	(%eax), %eax ;						\
	testl	_apic_isrbit_location + 4 + 8 * (irq_num), %eax ;	\
	jz	9f ;				/* not active */	\
	movl	$0, lapic_eoi ;						\
	APIC_ITRACE(apic_itrace_eoi, irq_num, APIC_ITRACE_EOI) ;	\
9:

#else
#define EOI_IRQ(irq_num)						\
	testl	$IRQ_BIT(irq_num), lapic_isr1;				\
	jz	9f	;			/* not active */	\
	movl	$0, lapic_eoi;						\
	APIC_ITRACE(apic_itrace_eoi, irq_num, APIC_ITRACE_EOI) ;	\
9:
#endif
	
	
/*
 * Test to see if the source is currently masked, clear if so.
 */
#define UNMASK_IRQ(irq_num)					\
	IMASK_LOCK ;				/* into critical reg */	\
	testl	$IRQ_BIT(irq_num), _apic_imen ;				\
	je	7f ;			/* bit clear, not masked */	\
	andl	$~IRQ_BIT(irq_num), _apic_imen ;/* clear mask bit */	\
	movl	IOAPICADDR(irq_num),%ecx ;	/* ioapic addr */	\
	movl	REDIRIDX(irq_num), %eax ;	/* get the index */	\
	movl	%eax,(%ecx) ;			/* write the index */	\
	movl	IOAPIC_WINDOW(%ecx),%eax ;	/* current value */	\
	andl	$~IOART_INTMASK,%eax ;		/* clear the mask */	\
	movl	%eax,IOAPIC_WINDOW(%ecx) ;	/* new value */		\
7: ;									\
	IMASK_UNLOCK

#ifdef APIC_INTR_DIAGNOSTIC
#ifdef APIC_INTR_DIAGNOSTIC_IRQ
log_intr_event:
	pushf
	cli
	pushl	$CNAME(apic_itrace_debuglock)
	call	CNAME(s_lock_np)
	addl	$4, %esp
	movl	CNAME(apic_itrace_debugbuffer_idx), %ecx
	andl	$32767, %ecx
	movl	_cpuid, %eax
	shll	$8,	%eax
	orl	8(%esp), %eax
	movw	%ax,	CNAME(apic_itrace_debugbuffer)(,%ecx,2)
	incl	%ecx
	andl	$32767, %ecx
	movl	%ecx,	CNAME(apic_itrace_debugbuffer_idx)
	pushl	$CNAME(apic_itrace_debuglock)
	call	CNAME(s_unlock_np)
	addl	$4, %esp
	popf
	ret
	

#define APIC_ITRACE(name, irq_num, id)					\
	lock ;					/* MP-safe */		\
	incl	CNAME(name) + (irq_num) * 4 ;				\
	pushl	%eax ;							\
	pushl	%ecx ;							\
	pushl	%edx ;							\
	movl	$(irq_num), %eax ;					\
	cmpl	$APIC_INTR_DIAGNOSTIC_IRQ, %eax ;			\
	jne	7f ;							\
	pushl	$id ;							\
	call	log_intr_event ;					\
	addl	$4, %esp ;						\
7: ;									\
	popl	%edx ;							\
	popl	%ecx ;							\
	popl	%eax
#else
#define APIC_ITRACE(name, irq_num, id)					\
	lock ;					/* MP-safe */		\
	incl	CNAME(name) + (irq_num) * 4
#endif

#define APIC_ITRACE_ENTER 1
#define APIC_ITRACE_EOI 2
#define APIC_ITRACE_TRYISRLOCK 3
#define APIC_ITRACE_GOTISRLOCK 4
#define APIC_ITRACE_ENTER2 5
#define APIC_ITRACE_LEAVE 6
#define APIC_ITRACE_UNMASK 7
#define APIC_ITRACE_ACTIVE 8
#define APIC_ITRACE_MASKED 9
#define APIC_ITRACE_NOISRLOCK 10
#define APIC_ITRACE_MASKED2 11
#define APIC_ITRACE_SPLZ 12
#define APIC_ITRACE_DORETI 13	
	
#else	
#define APIC_ITRACE(name, irq_num, id)
#endif

/* 
 * Slow, threaded interrupts.
 *
 * XXX Most of the parameters here are obsolete.  Fix this when we're
 * done.
 * XXX we really shouldn't return via doreti if we just schedule the
 * interrupt handler and don't run anything.  We could just do an
 * iret.  FIXME.
 */
#define	INTR(irq_num, vec_name, maybe_extra_ipending)			\
	.text ;								\
	SUPERALIGN_TEXT ;						\
/* _XintrNN: entry point used by IDT/HWIs & splz_unpend via _vec[]. */	\
IDTVEC(vec_name) ;							\
	PUSH_FRAME ;							\
	movl	$KDSEL, %eax ;	/* reload with kernel's data segment */	\
	mov	%ax, %ds ;						\
	mov	%ax, %es ;						\
	movl	$KPSEL, %eax ;						\
	mov	%ax, %fs ;						\
;									\
	maybe_extra_ipending ;						\
;									\
	APIC_ITRACE(apic_itrace_enter, irq_num, APIC_ITRACE_ENTER) ;	\
;									\
	MASK_LEVEL_IRQ(irq_num) ;					\
	EOI_IRQ(irq_num) ;						\
0: ;									\
	incb	_intr_nesting_level ;					\
;	 								\
  /* entry point used by doreti_unpend for HWIs. */			\
__CONCAT(Xresume,irq_num): ;						\
	FAKE_MCOUNT(13*4(%esp)) ;		/* XXX avoid dbl cnt */ \
	pushl	$irq_num;			/* pass the IRQ */	\
	APIC_ITRACE(apic_itrace_enter2, irq_num, APIC_ITRACE_ENTER2) ;	\
	sti ;								\
	call	_sched_ithd ;						\
	addl	$4, %esp ;		/* discard the parameter */	\
	APIC_ITRACE(apic_itrace_leave, irq_num, APIC_ITRACE_LEAVE) ;	\
;									\
	MEXITCOUNT ;							\
	jmp	doreti_next

/*
 * Handle "spurious INTerrupts".
 * Notes:
 *  This is different than the "spurious INTerrupt" generated by an
 *   8259 PIC for missing INTs.  See the APIC documentation for details.
 *  This routine should NOT do an 'EOI' cycle.
 */
	.text
	SUPERALIGN_TEXT
	.globl _Xspuriousint
_Xspuriousint:

	/* No EOI cycle used here */

	iret


/*
 * Handle TLB shootdowns.
 */
	.text
	SUPERALIGN_TEXT
	.globl	_Xinvltlb
_Xinvltlb:
	pushl	%eax

#ifdef COUNT_XINVLTLB_HITS
	pushl	%fs
	movl	$KPSEL, %eax
	mov	%ax, %fs
	movl	_cpuid, %eax
	popl	%fs
	ss
	incl	_xhits(,%eax,4)
#endif /* COUNT_XINVLTLB_HITS */

	movl	%cr3, %eax		/* invalidate the TLB */
	movl	%eax, %cr3

	ss				/* stack segment, avoid %ds load */
	movl	$0, lapic_eoi		/* End Of Interrupt to APIC */

	popl	%eax
	iret


#ifdef BETTER_CLOCK

/*
 * Executed by a CPU when it receives an Xcpucheckstate IPI from another CPU,
 *
 *  - Stores current cpu state in checkstate_cpustate[cpuid]
 *      0 == user, 1 == sys, 2 == intr
 *  - Stores current process in checkstate_curproc[cpuid]
 *
 *  - Signals its receipt by setting bit cpuid in checkstate_probed_cpus.
 *
 * stack: 0->ds, 4->fs, 8->ebx, 12->eax, 16->eip, 20->cs, 24->eflags
 */

	.text
	SUPERALIGN_TEXT
	.globl _Xcpucheckstate
	.globl _checkstate_cpustate
	.globl _checkstate_curproc
	.globl _checkstate_pc
_Xcpucheckstate:
	pushl	%eax
	pushl	%ebx		
	pushl	%ds			/* save current data segment */
	pushl	%fs

	movl	$KDSEL, %eax
	mov	%ax, %ds		/* use KERNEL data segment */
	movl	$KPSEL, %eax
	mov	%ax, %fs

	movl	$0, lapic_eoi		/* End Of Interrupt to APIC */

	movl	$0, %ebx		
	movl	20(%esp), %eax	
	andl	$3, %eax
	cmpl	$3, %eax
	je	1f
	testl	$PSL_VM, 24(%esp)
	jne	1f
	incl	%ebx			/* system or interrupt */
1:	
	movl	_cpuid, %eax
	movl	%ebx, _checkstate_cpustate(,%eax,4)
	movl	_curproc, %ebx
	movl	%ebx, _checkstate_curproc(,%eax,4)
	movl	16(%esp), %ebx
	movl	%ebx, _checkstate_pc(,%eax,4)

	lock				/* checkstate_probed_cpus |= (1<<id) */
	btsl	%eax, _checkstate_probed_cpus

	popl	%fs
	popl	%ds			/* restore previous data segment */
	popl	%ebx
	popl	%eax
	iret

#endif /* BETTER_CLOCK */

/*
 * Executed by a CPU when it receives an Xcpuast IPI from another CPU,
 *
 *  - Signals its receipt by clearing bit cpuid in checkstate_need_ast.
 *
 *  - We need a better method of triggering asts on other cpus.
 */

	.text
	SUPERALIGN_TEXT
	.globl _Xcpuast
_Xcpuast:
	PUSH_FRAME
	movl	$KDSEL, %eax
	mov	%ax, %ds		/* use KERNEL data segment */
	mov	%ax, %es
	movl	$KPSEL, %eax
	mov	%ax, %fs

	movl	_cpuid, %eax
	lock				/* checkstate_need_ast &= ~(1<<id) */
	btrl	%eax, _checkstate_need_ast
	movl	$0, lapic_eoi		/* End Of Interrupt to APIC */

	lock
	btsl	%eax, _checkstate_pending_ast
	jc	1f

	FAKE_MCOUNT(13*4(%esp))

	orl	$AST_PENDING, _astpending	/* XXX */
	incb	_intr_nesting_level
	sti
	
	movl	_cpuid, %eax
	lock	
	btrl	%eax, _checkstate_pending_ast
	lock	
	btrl	%eax, CNAME(resched_cpus)
	jnc	2f
	orl	$AST_PENDING+AST_RESCHED,_astpending
	lock
	incl	CNAME(want_resched_cnt)
2:		
	lock
	incl	CNAME(cpuast_cnt)
	MEXITCOUNT
	jmp	doreti_next
1:
	/* We are already in the process of delivering an ast for this CPU */
	POP_FRAME
	iret			


/*
 *	 Executed by a CPU when it receives an XFORWARD_IRQ IPI.
 */

	.text
	SUPERALIGN_TEXT
	.globl _Xforward_irq
_Xforward_irq:
	PUSH_FRAME
	movl	$KDSEL, %eax
	mov	%ax, %ds		/* use KERNEL data segment */
	mov	%ax, %es
	movl	$KPSEL, %eax
	mov	%ax, %fs

	movl	$0, lapic_eoi		/* End Of Interrupt to APIC */

	FAKE_MCOUNT(13*4(%esp))

	lock
	incl	CNAME(forward_irq_hitcnt)
	cmpb	$4, _intr_nesting_level
	jae	1f
	
	incb	_intr_nesting_level
	sti
	
	MEXITCOUNT
	jmp	doreti_next		/* Handle forwarded interrupt */
1:
	lock
	incl	CNAME(forward_irq_toodeepcnt)
	MEXITCOUNT
	POP_FRAME
	iret

#if 0
/*
 * 
 */
forward_irq:
	MCOUNT
	cmpl	$0,_invltlb_ok
	jz	4f

	cmpl	$0, CNAME(forward_irq_enabled)
	jz	4f

/* XXX - this is broken now, because mp_lock doesn't exist
	movl	_mp_lock,%eax
	cmpl	$FREE_LOCK,%eax
	jne	1f
 */
	movl	$0, %eax		/* Pick CPU #0 if noone has lock */
1:
	shrl	$24,%eax
	movl	_cpu_num_to_apic_id(,%eax,4),%ecx
	shll	$24,%ecx
	movl	lapic_icr_hi, %eax
	andl	$~APIC_ID_MASK, %eax
	orl	%ecx, %eax
	movl	%eax, lapic_icr_hi

2:
	movl	lapic_icr_lo, %eax
	andl	$APIC_DELSTAT_MASK,%eax
	jnz	2b
	movl	lapic_icr_lo, %eax
	andl	$APIC_RESV2_MASK, %eax
	orl	$(APIC_DEST_DESTFLD|APIC_DELMODE_FIXED|XFORWARD_IRQ_OFFSET), %eax
	movl	%eax, lapic_icr_lo
3:
	movl	lapic_icr_lo, %eax
	andl	$APIC_DELSTAT_MASK,%eax
	jnz	3b
4:		
	ret
#endif
	
/*
 * Executed by a CPU when it receives an Xcpustop IPI from another CPU,
 *
 *  - Signals its receipt.
 *  - Waits for permission to restart.
 *  - Signals its restart.
 */

	.text
	SUPERALIGN_TEXT
	.globl _Xcpustop
_Xcpustop:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%eax
	pushl	%ecx
	pushl	%edx
	pushl	%ds			/* save current data segment */
	pushl	%fs

	movl	$KDSEL, %eax
	mov	%ax, %ds		/* use KERNEL data segment */
	movl	$KPSEL, %eax
	mov	%ax, %fs

	movl	$0, lapic_eoi		/* End Of Interrupt to APIC */

	movl	_cpuid, %eax
	imull	$PCB_SIZE, %eax
	leal	CNAME(stoppcbs)(%eax), %eax
	pushl	%eax
	call	CNAME(savectx)		/* Save process context */
	addl	$4, %esp
	
		
	movl	_cpuid, %eax

	lock
	btsl	%eax, _stopped_cpus	/* stopped_cpus |= (1<<id) */
1:
	btl	%eax, _started_cpus	/* while (!(started_cpus & (1<<id))) */
	jnc	1b

	lock
	btrl	%eax, _started_cpus	/* started_cpus &= ~(1<<id) */
	lock
	btrl	%eax, _stopped_cpus	/* stopped_cpus &= ~(1<<id) */

	test	%eax, %eax
	jnz	2f

	movl	CNAME(cpustop_restartfunc), %eax
	test	%eax, %eax
	jz	2f
	movl	$0, CNAME(cpustop_restartfunc)	/* One-shot */

	call	*%eax
2:
	popl	%fs
	popl	%ds			/* restore previous data segment */
	popl	%edx
	popl	%ecx
	popl	%eax
	movl	%ebp, %esp
	popl	%ebp
	iret


MCOUNT_LABEL(bintr)
	FAST_INTR(0,fastintr0)
	FAST_INTR(1,fastintr1)
	FAST_INTR(2,fastintr2)
	FAST_INTR(3,fastintr3)
	FAST_INTR(4,fastintr4)
	FAST_INTR(5,fastintr5)
	FAST_INTR(6,fastintr6)
	FAST_INTR(7,fastintr7)
	FAST_INTR(8,fastintr8)
	FAST_INTR(9,fastintr9)
	FAST_INTR(10,fastintr10)
	FAST_INTR(11,fastintr11)
	FAST_INTR(12,fastintr12)
	FAST_INTR(13,fastintr13)
	FAST_INTR(14,fastintr14)
	FAST_INTR(15,fastintr15)
	FAST_INTR(16,fastintr16)
	FAST_INTR(17,fastintr17)
	FAST_INTR(18,fastintr18)
	FAST_INTR(19,fastintr19)
	FAST_INTR(20,fastintr20)
	FAST_INTR(21,fastintr21)
	FAST_INTR(22,fastintr22)
	FAST_INTR(23,fastintr23)
#define	CLKINTR_PENDING	movl $1,CNAME(clkintr_pending)
/* Threaded interrupts */
	INTR(0,intr0, CLKINTR_PENDING)
	INTR(1,intr1,)
	INTR(2,intr2,)
	INTR(3,intr3,)
	INTR(4,intr4,)
	INTR(5,intr5,)
	INTR(6,intr6,)
	INTR(7,intr7,)
	INTR(8,intr8,)
	INTR(9,intr9,)
	INTR(10,intr10,)
	INTR(11,intr11,)
	INTR(12,intr12,)
	INTR(13,intr13,)
	INTR(14,intr14,)
	INTR(15,intr15,)
	INTR(16,intr16,)
	INTR(17,intr17,)
	INTR(18,intr18,)
	INTR(19,intr19,)
	INTR(20,intr20,)
	INTR(21,intr21,)
	INTR(22,intr22,)
	INTR(23,intr23,)
MCOUNT_LABEL(eintr)

/*
 * Executed by a CPU when it receives a RENDEZVOUS IPI from another CPU.
 *
 * - Calls the generic rendezvous action function.
 */
	.text
	SUPERALIGN_TEXT
	.globl	_Xrendezvous
_Xrendezvous:
	PUSH_FRAME
	movl	$KDSEL, %eax
	mov	%ax, %ds		/* use KERNEL data segment */
	mov	%ax, %es
	movl	$KPSEL, %eax
	mov	%ax, %fs

	call	_smp_rendezvous_action

	movl	$0, lapic_eoi		/* End Of Interrupt to APIC */
	POP_FRAME
	iret
	
	
	.data
#if 0
/* active flag for lazy masking */
iactive:
	.long	0
#endif

#ifdef COUNT_XINVLTLB_HITS
	.globl	_xhits
_xhits:
	.space	(NCPU * 4), 0
#endif /* COUNT_XINVLTLB_HITS */

/* variables used by stop_cpus()/restart_cpus()/Xcpustop */
	.globl _stopped_cpus, _started_cpus
_stopped_cpus:
	.long	0
_started_cpus:
	.long	0

#ifdef BETTER_CLOCK
	.globl _checkstate_probed_cpus
_checkstate_probed_cpus:
	.long	0	
#endif /* BETTER_CLOCK */
	.globl _checkstate_need_ast
_checkstate_need_ast:
	.long	0
_checkstate_pending_ast:
	.long	0
	.globl CNAME(forward_irq_misscnt)
	.globl CNAME(forward_irq_toodeepcnt)
	.globl CNAME(forward_irq_hitcnt)
	.globl CNAME(resched_cpus)
	.globl CNAME(want_resched_cnt)
	.globl CNAME(cpuast_cnt)
	.globl CNAME(cpustop_restartfunc)
CNAME(forward_irq_misscnt):	
	.long 0
CNAME(forward_irq_hitcnt):	
	.long 0
CNAME(forward_irq_toodeepcnt):
	.long 0
CNAME(resched_cpus):
	.long 0
CNAME(want_resched_cnt):
	.long 0
CNAME(cpuast_cnt):
	.long 0
CNAME(cpustop_restartfunc):
	.long 0
		


	.globl	_apic_pin_trigger
_apic_pin_trigger:
	.long	0

	.text
