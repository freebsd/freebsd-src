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
	movl	PCPU(CURPROC),%ebx ;					\
	incl	P_INTR_NESTING_LEVEL(%ebx) ;				\
	pushl	intr_unit + (irq_num) * 4 ;				\
	call	*intr_handler + (irq_num) * 4 ; /* do the work ASAP */ \
	addl	$4, %esp ;						\
	movl	$0, lapic+LA_EOI ;					\
	lock ; 								\
	incl	cnt+V_INTR ;	/* book-keeping can wait */		\
	movl	intr_countp + (irq_num) * 4, %eax ;			\
	lock ; 								\
	incl	(%eax) ;						\
	decl	P_INTR_NESTING_LEVEL(%ebx) ;				\
	MEXITCOUNT ;							\
	jmp	doreti

#define IOAPICADDR(irq_num) CNAME(int_to_apicintpin) + 16 * (irq_num) + 8
#define REDIRIDX(irq_num) CNAME(int_to_apicintpin) + 16 * (irq_num) + 12
	
#define MASK_IRQ(irq_num)						\
	IMASK_LOCK ;				/* into critical reg */	\
	testl	$IRQ_BIT(irq_num), apic_imen ;				\
	jne	7f ;			/* masked, don't mask */	\
	orl	$IRQ_BIT(irq_num), apic_imen ;	/* set the mask bit */	\
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
	testl	$IRQ_BIT(irq_num), apic_pin_trigger ;			\
	jz	9f ;				/* edge, don't mask */	\
	MASK_IRQ(irq_num) ;						\
9:


#ifdef APIC_INTR_REORDER
#define EOI_IRQ(irq_num)						\
	movl	apic_isrbit_location + 8 * (irq_num), %eax ;		\
	movl	(%eax), %eax ;						\
	testl	apic_isrbit_location + 4 + 8 * (irq_num), %eax ;	\
	jz	9f ;				/* not active */	\
	movl	$0, lapic+LA_EOI ;					\
9:

#else
#define EOI_IRQ(irq_num)						\
	testl	$IRQ_BIT(irq_num), lapic+LA_ISR1;			\
	jz	9f	;			/* not active */	\
	movl	$0, lapic+LA_EOI;					\
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
	movl	IOAPICADDR(irq_num), %ecx ;	/* ioapic addr */	\
	movl	REDIRIDX(irq_num), %eax ;	/* get the index */	\
	movl	%eax, (%ecx) ;			/* write the index */	\
	movl	IOAPIC_WINDOW(%ecx), %eax ;	/* current value */	\
	andl	$~IOART_INTMASK, %eax ;		/* clear the mask */	\
	movl	%eax, IOAPIC_WINDOW(%ecx) ;	/* new value */		\
7: ;						/* already unmasked */	\
	IMASK_UNLOCK

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
/* _XintrNN: entry point used by IDT/HWIs via _vec[]. */		\
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
	MASK_LEVEL_IRQ(irq_num) ;					\
	EOI_IRQ(irq_num) ;						\
0: ;									\
	movl	PCPU(CURPROC),%ebx ;					\
	incl	P_INTR_NESTING_LEVEL(%ebx) ;				\
;	 								\
  /* entry point used by doreti_unpend for HWIs. */			\
__CONCAT(Xresume,irq_num): ;						\
	FAKE_MCOUNT(13*4(%esp)) ;		/* XXX avoid dbl cnt */ \
	pushl	$irq_num;			/* pass the IRQ */	\
	call	sched_ithd ;						\
	addl	$4, %esp ;		/* discard the parameter */	\
;									\
	decl	P_INTR_NESTING_LEVEL(%ebx) ;				\
	MEXITCOUNT ;							\
	jmp	doreti

/*
 * Handle "spurious INTerrupts".
 * Notes:
 *  This is different than the "spurious INTerrupt" generated by an
 *   8259 PIC for missing INTs.  See the APIC documentation for details.
 *  This routine should NOT do an 'EOI' cycle.
 */
	.text
	SUPERALIGN_TEXT
	.globl Xspuriousint
Xspuriousint:

	/* No EOI cycle used here */

	iret

/*
 * Handle TLB shootdowns.
 */
	.text
	SUPERALIGN_TEXT
	.globl	Xinvltlb
Xinvltlb:
	pushl	%eax

#ifdef COUNT_XINVLTLB_HITS
	pushl	%fs
	movl	$KPSEL, %eax
	mov	%ax, %fs
	movl	PCPU(CPUID), %eax
	popl	%fs
	ss
	incl	_xhits(,%eax,4)
#endif /* COUNT_XINVLTLB_HITS */

	movl	%cr3, %eax		/* invalidate the TLB */
	movl	%eax, %cr3

	ss				/* stack segment, avoid %ds load */
	movl	$0, lapic+LA_EOI	/* End Of Interrupt to APIC */

	popl	%eax
	iret

/*
 * Forward hardclock to another CPU.  Pushes a trapframe and calls
 * forwarded_hardclock().
 */
	.text
	SUPERALIGN_TEXT
	.globl Xhardclock
Xhardclock:
	PUSH_FRAME
	movl	$KDSEL, %eax	/* reload with kernel's data segment */
	mov	%ax, %ds
	mov	%ax, %es
	movl	$KPSEL, %eax
	mov	%ax, %fs

	movl	$0, lapic+LA_EOI	/* End Of Interrupt to APIC */

	movl	PCPU(CURPROC),%ebx
	incl	P_INTR_NESTING_LEVEL(%ebx)
	call	forwarded_hardclock
	decl	P_INTR_NESTING_LEVEL(%ebx)
	MEXITCOUNT
	jmp	doreti

/*
 * Forward statclock to another CPU.  Pushes a trapframe and calls
 * forwarded_statclock().
 */
	.text
	SUPERALIGN_TEXT
	.globl Xstatclock
Xstatclock:
	PUSH_FRAME
	movl	$KDSEL, %eax	/* reload with kernel's data segment */
	mov	%ax, %ds
	mov	%ax, %es
	movl	$KPSEL, %eax
	mov	%ax, %fs

	movl	$0, lapic+LA_EOI	/* End Of Interrupt to APIC */

	FAKE_MCOUNT(13*4(%esp))
	movl	PCPU(CURPROC),%ebx
	incl	P_INTR_NESTING_LEVEL(%ebx)
	call	forwarded_statclock
	decl	P_INTR_NESTING_LEVEL(%ebx)
	MEXITCOUNT
	jmp	doreti

/*
 * Executed by a CPU when it receives an Xcpuast IPI from another CPU,
 *
 * The other CPU has already executed aston() or need_resched() on our
 * current process, so we simply need to ack the interrupt and return
 * via doreti to run ast().
 */

	.text
	SUPERALIGN_TEXT
	.globl Xcpuast
Xcpuast:
	PUSH_FRAME
	movl	$KDSEL, %eax
	mov	%ax, %ds		/* use KERNEL data segment */
	mov	%ax, %es
	movl	$KPSEL, %eax
	mov	%ax, %fs

	movl	$0, lapic+LA_EOI	/* End Of Interrupt to APIC */

	FAKE_MCOUNT(13*4(%esp))

	MEXITCOUNT
	jmp	doreti

/*
 * Executed by a CPU when it receives an Xcpustop IPI from another CPU,
 *
 *  - Signals its receipt.
 *  - Waits for permission to restart.
 *  - Signals its restart.
 */
	.text
	SUPERALIGN_TEXT
	.globl Xcpustop
Xcpustop:
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

	movl	$0, lapic+LA_EOI	/* End Of Interrupt to APIC */

	movl	PCPU(CPUID), %eax
	imull	$PCB_SIZE, %eax
	leal	CNAME(stoppcbs)(%eax), %eax
	pushl	%eax
	call	CNAME(savectx)		/* Save process context */
	addl	$4, %esp
		
	movl	PCPU(CPUID), %eax

	lock
	btsl	%eax, CNAME(stopped_cpus) /* stopped_cpus |= (1<<id) */
1:
	btl	%eax, CNAME(started_cpus) /* while (!(started_cpus & (1<<id))) */
	jnc	1b

	lock
	btrl	%eax, CNAME(started_cpus) /* started_cpus &= ~(1<<id) */
	lock
	btrl	%eax, CNAME(stopped_cpus) /* stopped_cpus &= ~(1<<id) */

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
	FAST_INTR(24,fastintr24)
	FAST_INTR(25,fastintr25)
	FAST_INTR(26,fastintr26)
	FAST_INTR(27,fastintr27)
	FAST_INTR(28,fastintr28)
	FAST_INTR(29,fastintr29)
	FAST_INTR(30,fastintr30)
	FAST_INTR(31,fastintr31)
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
	INTR(24,intr24,)
	INTR(25,intr25,)
	INTR(26,intr26,)
	INTR(27,intr27,)
	INTR(28,intr28,)
	INTR(29,intr29,)
	INTR(30,intr30,)
	INTR(31,intr31,)
MCOUNT_LABEL(eintr)

/*
 * Executed by a CPU when it receives a RENDEZVOUS IPI from another CPU.
 *
 * - Calls the generic rendezvous action function.
 */
	.text
	SUPERALIGN_TEXT
	.globl	Xrendezvous
Xrendezvous:
	PUSH_FRAME
	movl	$KDSEL, %eax
	mov	%ax, %ds		/* use KERNEL data segment */
	mov	%ax, %es
	movl	$KPSEL, %eax
	mov	%ax, %fs

	call	smp_rendezvous_action

	movl	$0, lapic+LA_EOI	/* End Of Interrupt to APIC */
	POP_FRAME
	iret
	
	
	.data

#ifdef COUNT_XINVLTLB_HITS
	.globl	_xhits
_xhits:
	.space	(NCPU * 4), 0
#endif /* COUNT_XINVLTLB_HITS */

	.globl	apic_pin_trigger
apic_pin_trigger:
	.long	0

	.text
