/*
 *	from: vector.s, 386BSD 0.1 unknown origin
 *	$Id: apic_vector.s,v 1.27 1998/03/03 22:56:28 tegge Exp $
 */


#include <machine/apic.h>
#include <machine/smp.h>

#include "i386/isa/intr_machdep.h"


#ifdef FAST_SIMPLELOCK

#define GET_FAST_INTR_LOCK						\
	pushl	$_fast_intr_lock ;		/* address of lock */	\
	call	_s_lock ;			/* MP-safe */		\
	addl	$4,%esp

#define REL_FAST_INTR_LOCK						\
	pushl	$_fast_intr_lock ;		/* address of lock */	\
	call	_s_unlock ;			/* MP-safe */		\
	addl	$4,%esp

#else /* FAST_SIMPLELOCK */

#define GET_FAST_INTR_LOCK						\
	call	_get_isrlock

#define REL_FAST_INTR_LOCK						\
	pushl	$_mp_lock ;	/* GIANT_LOCK */			\
	call	_MPrellock ;						\
	add	$4, %esp

#endif /* FAST_SIMPLELOCK */

/* convert an absolute IRQ# into a bitmask */
#define IRQ_BIT(irq_num)	(1 << (irq_num))

/* make an index into the IO APIC from the IRQ# */
#define REDTBL_IDX(irq_num)	(0x10 + ((irq_num) * 2))


/*
 * Macros for interrupt interrupt entry, call to handler, and exit.
 */

#ifdef FAST_WITHOUTCPL

/*
 */
#define	FAST_INTR(irq_num, vec_name)					\
	.text ;								\
	SUPERALIGN_TEXT ;						\
IDTVEC(vec_name) ;							\
	pushl	%eax ;		/* save only call-used registers */	\
	pushl	%ecx ;							\
	pushl	%edx ;							\
	pushl	%ds ;							\
	MAYBE_PUSHL_ES ;						\
	movl	$KDSEL,%eax ;						\
	movl	%ax,%ds ;						\
	MAYBE_MOVW_AX_ES ;						\
	FAKE_MCOUNT((4+ACTUALLY_PUSHED)*4(%esp)) ;			\
	pushl	_intr_unit + (irq_num) * 4 ;				\
	GET_FAST_INTR_LOCK ;						\
	call	*_intr_handler + (irq_num) * 4 ; /* do the work ASAP */ \
	REL_FAST_INTR_LOCK ;						\
	addl	$4, %esp ;						\
	movl	$0, lapic_eoi ;						\
	lock ; 								\
	incl	_cnt+V_INTR ;	/* book-keeping can wait */		\
	movl	_intr_countp + (irq_num) * 4, %eax ;			\
	lock ; 								\
	incl	(%eax) ;						\
	MEXITCOUNT ;							\
	MAYBE_POPL_ES ;							\
	popl	%ds ;							\
	popl	%edx ;							\
	popl	%ecx ;							\
	popl	%eax ;							\
	iret

#else /* FAST_WITHOUTCPL */

#define	FAST_INTR(irq_num, vec_name)					\
	.text ;								\
	SUPERALIGN_TEXT ;						\
IDTVEC(vec_name) ;							\
	pushl	%eax ;		/* save only call-used registers */	\
	pushl	%ecx ;							\
	pushl	%edx ;							\
	pushl	%ds ;							\
	MAYBE_PUSHL_ES ;						\
	movl	$KDSEL, %eax ;						\
	movl	%ax, %ds ;						\
	MAYBE_MOVW_AX_ES ;						\
	FAKE_MCOUNT((4+ACTUALLY_PUSHED)*4(%esp)) ;			\
	GET_FAST_INTR_LOCK ;						\
	pushl	_intr_unit + (irq_num) * 4 ;				\
	call	*_intr_handler + (irq_num) * 4 ; /* do the work ASAP */ \
	addl	$4, %esp ;						\
	movl	$0, lapic_eoi ;						\
	lock ; 								\
	incl	_cnt+V_INTR ;	/* book-keeping can wait */		\
	movl	_intr_countp + (irq_num) * 4,%eax ;			\
	lock ; 								\
	incl	(%eax) ;						\
	movl	_cpl, %eax ;	/* unmasking pending HWIs or SWIs? */	\
	notl	%eax ;							\
	andl	_ipending, %eax ;					\
	jne	2f ; 		/* yes, maybe handle them */		\
1: ;									\
	MEXITCOUNT ;							\
	REL_FAST_INTR_LOCK ;						\
	MAYBE_POPL_ES ;							\
	popl	%ds ;							\
	popl	%edx ;							\
	popl	%ecx ;							\
	popl	%eax ;							\
	iret ;								\
;									\
	ALIGN_TEXT ;							\
2: ;									\
	cmpb	$3, _intr_nesting_level ;	/* enough stack? */	\
	jae	1b ;		/* no, return */			\
	movl	_cpl, %eax ;						\
	/* XXX next line is probably unnecessary now. */		\
	movl	$HWI_MASK|SWI_MASK, _cpl ;	/* limit nesting ... */	\
	lock ; 								\
	incb	_intr_nesting_level ;	/* ... really limit it ... */	\
	sti ;			/* to do this as early as possible */	\
	MAYBE_POPL_ES ;		/* discard most of thin frame ... */	\
	popl	%ecx ;		/* ... original %ds ... */		\
	popl	%edx ;							\
	xchgl	%eax, 4(%esp) ;	/* orig %eax; save cpl */		\
	pushal ;		/* build fat frame (grrr) ... */	\
	pushl	%ecx ;		/* ... actually %ds ... */		\
	pushl	%es ;							\
	movl	$KDSEL, %eax ;						\
	movl	%ax, %es ;						\
	movl	(2+8+0)*4(%esp), %ecx ;	/* %ecx from thin frame ... */	\
	movl	%ecx, (2+6)*4(%esp) ;	/* ... to fat frame ... */	\
	movl	(2+8+1)*4(%esp), %eax ;	/* ... cpl from thin frame */	\
	pushl	%eax ;							\
	subl	$4, %esp ;	/* junk for unit number */		\
	MEXITCOUNT ;							\
	jmp	_doreti

#endif /** FAST_WITHOUTCPL */


/*
 * 
 */
#define PUSH_FRAME							\
	pushl	$0 ;		/* dummy error code */			\
	pushl	$0 ;		/* dummy trap type */			\
	pushal ;							\
	pushl	%ds ;		/* save data and extra segments ... */	\
	pushl	%es

#define POP_FRAME							\
	popl	%es ;							\
	popl	%ds ;							\
	popal ;								\
	addl	$4+4,%esp

#define MASK_IRQ(irq_num)						\
	IMASK_LOCK ;				/* into critical reg */	\
	testl	$IRQ_BIT(irq_num), _apic_imen ;				\
	jne	7f ;			/* masked, don't mask */	\
	orl	$IRQ_BIT(irq_num), _apic_imen ;	/* set the mask bit */	\
	movl	_ioapic, %ecx ;			/* ioapic[0] addr */	\
	movl	$REDTBL_IDX(irq_num), (%ecx) ;	/* write the index */	\
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
 * Test to see if the source is currntly masked, clear if so.
 */
#define UNMASK_IRQ(irq_num)					\
	IMASK_LOCK ;				/* into critical reg */	\
	testl	$IRQ_BIT(irq_num), _apic_imen ;				\
	je	7f ;			/* bit clear, not masked */	\
	andl	$~IRQ_BIT(irq_num), _apic_imen ;/* clear mask bit */	\
	movl	_ioapic,%ecx ;			/* ioapic[0]addr */	\
	movl	$REDTBL_IDX(irq_num),(%ecx) ;	/* write the index */	\
	movl	IOAPIC_WINDOW(%ecx),%eax ;	/* current value */	\
	andl	$~IOART_INTMASK,%eax ;		/* clear the mask */	\
	movl	%eax,IOAPIC_WINDOW(%ecx) ;	/* new value */		\
7: ;									\
	IMASK_UNLOCK

#ifdef INTR_SIMPLELOCK
#define ENLOCK
#define DELOCK
#define LATELOCK call	_get_isrlock
#else
#define ENLOCK \
	ISR_TRYLOCK ;		/* XXX this is going away... */		\
	testl	%eax, %eax ;			/* did we get it? */	\
	jz	3f
#define DELOCK	ISR_RELLOCK
#define LATELOCK
#endif

#ifdef APIC_INTR_DIAGNOSTIC
#ifdef APIC_INTR_DIAGNOSTIC_IRQ
log_intr_event:
	pushf
	cli
	pushl	$CNAME(apic_itrace_debuglock)
	call	_s_lock_np
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
	call	_s_unlock_np
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
		
#ifdef CPL_AND_CML

#define	INTR(irq_num, vec_name)						\
	.text ;								\
	SUPERALIGN_TEXT ;						\
/* _XintrNN: entry point used by IDT/HWIs & splz_unpend via _vec[]. */	\
IDTVEC(vec_name) ;							\
	PUSH_FRAME ;							\
	movl	$KDSEL, %eax ;	/* reload with kernel's data segment */	\
	movl	%ax, %ds ;						\
	movl	%ax, %es ;						\
;									\
	APIC_ITRACE(apic_itrace_enter, irq_num, APIC_ITRACE_ENTER) ;	\
	lock ;					/* MP-safe */		\
	btsl	$(irq_num), iactive ;		/* lazy masking */	\
	jc	1f ;				/* already active */	\
;									\
	MASK_LEVEL_IRQ(irq_num) ;					\
	EOI_IRQ(irq_num) ;						\
0: ;									\
	APIC_ITRACE(apic_itrace_tryisrlock, irq_num, APIC_ITRACE_TRYISRLOCK) ;\
	ENLOCK ;							\
;									\
	APIC_ITRACE(apic_itrace_gotisrlock, irq_num, APIC_ITRACE_GOTISRLOCK) ;\
	AVCPL_LOCK ;				/* MP-safe */		\
	testl	$IRQ_BIT(irq_num), _cpl ;				\
	jne	2f ;				/* this INT masked */	\
	testl	$IRQ_BIT(irq_num), _cml ;				\
	jne	2f ;				/* this INT masked */	\
	orl	$IRQ_BIT(irq_num), _cil ;				\
	AVCPL_UNLOCK ;							\
;									\
	incb	_intr_nesting_level ;					\
;	 								\
  /* entry point used by doreti_unpend for HWIs. */			\
__CONCAT(Xresume,irq_num): ;						\
	FAKE_MCOUNT(12*4(%esp)) ;		/* XXX avoid dbl cnt */ \
	lock ;	incl	_cnt+V_INTR ;		/* tally interrupts */	\
	movl	_intr_countp + (irq_num) * 4, %eax ;			\
	lock ;	incl	(%eax) ;					\
;									\
	AVCPL_LOCK ;				/* MP-safe */		\
	movl	_cml, %eax ;						\
	pushl	%eax ;							\
	orl	_intr_mask + (irq_num) * 4, %eax ;			\
	movl	%eax, _cml ;						\
	AVCPL_UNLOCK ;							\
;									\
	pushl	_intr_unit + (irq_num) * 4 ;				\
	incl	_inside_intr ;						\
	APIC_ITRACE(apic_itrace_enter2, irq_num, APIC_ITRACE_ENTER2) ;	\
	sti ;								\
	call	*_intr_handler + (irq_num) * 4 ;			\
	cli ;								\
	APIC_ITRACE(apic_itrace_leave, irq_num, APIC_ITRACE_LEAVE) ;	\
	decl	_inside_intr ;						\
;									\
	lock ;	andl $~IRQ_BIT(irq_num), iactive ;			\
	lock ;	andl $~IRQ_BIT(irq_num), _cil ;				\
	UNMASK_IRQ(irq_num) ;						\
	APIC_ITRACE(apic_itrace_unmask, irq_num, APIC_ITRACE_UNMASK) ;	\
	sti ;				/* doreti repeats cli/sti */	\
	MEXITCOUNT ;							\
	LATELOCK ;							\
	jmp	_doreti ;						\
;									\
	ALIGN_TEXT ;							\
1: ;						/* active */		\
	APIC_ITRACE(apic_itrace_active, irq_num, APIC_ITRACE_ACTIVE) ;	\
	AVCPL_LOCK ;				/* MP-safe */		\
	orl	$IRQ_BIT(irq_num), _ipending ;				\
	AVCPL_UNLOCK ;							\
	MASK_IRQ(irq_num) ;						\
	EOI_IRQ(irq_num) ;						\
	btsl	$(irq_num), iactive ;		/* still active */	\
	jnc	0b ;				/* retry */		\
	POP_FRAME ;							\
	iret ;								\
;									\
	ALIGN_TEXT ;							\
2: ;						/* masked by cpl|cml */	\
	APIC_ITRACE(apic_itrace_masked, irq_num, APIC_ITRACE_MASKED) ;	\
	orl	$IRQ_BIT(irq_num), _ipending ;				\
	AVCPL_UNLOCK ;							\
	DELOCK ;		/* XXX this is going away... */		\
	POP_FRAME ;							\
	iret ;								\
	ALIGN_TEXT ;							\
3: ; 			/* other cpu has isr lock */			\
	APIC_ITRACE(apic_itrace_noisrlock, irq_num, APIC_ITRACE_NOISRLOCK) ;\
	AVCPL_LOCK ;				/* MP-safe */		\
	orl	$IRQ_BIT(irq_num), _ipending ;				\
	testl	$IRQ_BIT(irq_num), _cpl ;				\
	jne	4f ;				/* this INT masked */	\
	testl	$IRQ_BIT(irq_num), _cml ;				\
	jne	4f ;				/* this INT masked */	\
	orl	$IRQ_BIT(irq_num), _cil ;				\
	AVCPL_UNLOCK ;							\
	call	forward_irq ;	/* forward irq to lock holder */	\
	POP_FRAME ;	 			/* and return */	\
	iret ;								\
	ALIGN_TEXT ;							\
4: ;	 					/* blocked */		\
	APIC_ITRACE(apic_itrace_masked2, irq_num, APIC_ITRACE_MASKED2) ;\
	AVCPL_UNLOCK ;							\
	POP_FRAME ;	 			/* and return */	\
	iret

#else /* CPL_AND_CML */


#define	INTR(irq_num, vec_name)						\
	.text ;								\
	SUPERALIGN_TEXT ;						\
/* _XintrNN: entry point used by IDT/HWIs & splz_unpend via _vec[]. */	\
IDTVEC(vec_name) ;							\
	PUSH_FRAME ;							\
	movl	$KDSEL, %eax ;	/* reload with kernel's data segment */	\
	movl	%ax, %ds ;						\
	movl	%ax, %es ;						\
;									\
	APIC_ITRACE(apic_itrace_enter, irq_num, APIC_ITRACE_ENTER) ;	\
	lock ;					/* MP-safe */		\
	btsl	$(irq_num), iactive ;		/* lazy masking */	\
	jc	1f ;				/* already active */	\
;									\
	MASK_LEVEL_IRQ(irq_num) ;					\
	EOI_IRQ(irq_num) ;						\
0: ;									\
	APIC_ITRACE(apic_itrace_tryisrlock, irq_num, APIC_ITRACE_TRYISRLOCK) ;\
	ISR_TRYLOCK ;		/* XXX this is going away... */		\
	testl	%eax, %eax ;			/* did we get it? */	\
	jz	3f ;				/* no */		\
;									\
	APIC_ITRACE(apic_itrace_gotisrlock, irq_num, APIC_ITRACE_GOTISRLOCK) ;\
	AVCPL_LOCK ;				/* MP-safe */		\
	testl	$IRQ_BIT(irq_num), _cpl ;				\
	jne	2f ;				/* this INT masked */	\
	AVCPL_UNLOCK ;							\
;									\
	incb	_intr_nesting_level ;					\
;	 								\
  /* entry point used by doreti_unpend for HWIs. */			\
__CONCAT(Xresume,irq_num): ;						\
	FAKE_MCOUNT(12*4(%esp)) ;		/* XXX avoid dbl cnt */ \
	lock ;	incl	_cnt+V_INTR ;		/* tally interrupts */	\
	movl	_intr_countp + (irq_num) * 4, %eax ;			\
	lock ;	incl	(%eax) ;					\
;									\
	AVCPL_LOCK ;				/* MP-safe */		\
	movl	_cpl, %eax ;						\
	pushl	%eax ;							\
	orl	_intr_mask + (irq_num) * 4, %eax ;			\
	movl	%eax, _cpl ;						\
	andl	$~IRQ_BIT(irq_num), _ipending ;				\
	AVCPL_UNLOCK ;							\
;									\
	pushl	_intr_unit + (irq_num) * 4 ;				\
	APIC_ITRACE(apic_itrace_enter2, irq_num, APIC_ITRACE_ENTER2) ;	\
	sti ;								\
	call	*_intr_handler + (irq_num) * 4 ;			\
	cli ;								\
	APIC_ITRACE(apic_itrace_leave, irq_num, APIC_ITRACE_LEAVE) ;	\
;									\
	lock ;	andl	$~IRQ_BIT(irq_num), iactive ;			\
	UNMASK_IRQ(irq_num) ;						\
	APIC_ITRACE(apic_itrace_unmask, irq_num, APIC_ITRACE_UNMASK) ;	\
	sti ;				/* doreti repeats cli/sti */	\
	MEXITCOUNT ;							\
	jmp	_doreti ;						\
;									\
	ALIGN_TEXT ;							\
1: ;						/* active  */		\
	APIC_ITRACE(apic_itrace_active, irq_num, APIC_ITRACE_ACTIVE) ;	\
	AVCPL_LOCK ;				/* MP-safe */		\
	orl	$IRQ_BIT(irq_num), _ipending ;				\
	AVCPL_UNLOCK ;							\
	MASK_IRQ(irq_num) ;						\
	EOI_IRQ(irq_num) ;						\
	btsl	$(irq_num), iactive ;		/* still active */	\
	jnc	0b ;				/* retry */		\
	POP_FRAME ;							\
	iret ;		/* XXX:	 iactive bit might be 0 now */		\
	ALIGN_TEXT ;							\
2: ;				/* masked by cpl, leave iactive set */	\
	APIC_ITRACE(apic_itrace_masked, irq_num, APIC_ITRACE_MASKED) ;	\
	orl	$IRQ_BIT(irq_num), _ipending ;				\
	AVCPL_UNLOCK ;							\
	ISR_RELLOCK ;		/* XXX this is going away... */		\
	POP_FRAME ;							\
	iret ;								\
	ALIGN_TEXT ;							\
3: ; 			/* other cpu has isr lock */			\
	APIC_ITRACE(apic_itrace_noisrlock, irq_num, APIC_ITRACE_NOISRLOCK) ;\
	AVCPL_LOCK ;				/* MP-safe */		\
	orl	$IRQ_BIT(irq_num), _ipending ;				\
	testl	$IRQ_BIT(irq_num), _cpl ;				\
	jne	4f ;				/* this INT masked */	\
	AVCPL_UNLOCK ;							\
	call	forward_irq ;	 /* forward irq to lock holder */	\
	POP_FRAME ;	 			/* and return */	\
	iret ;								\
	ALIGN_TEXT ;							\
4: ;	 					/* blocked */		\
	APIC_ITRACE(apic_itrace_masked2, irq_num, APIC_ITRACE_MASKED2) ;\
	AVCPL_UNLOCK ;							\
	POP_FRAME ;	 			/* and return */	\
	iret

#endif /* CPL_AND_CML */


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
	ss
	movl	_cpuid, %eax
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
 * stack: 0 -> ds, 4 -> ebx, 8 -> eax, 12 -> eip, 16 -> cs, 20 -> eflags
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

	movl	$KDSEL, %eax
	movl	%ax, %ds		/* use KERNEL data segment */

	movl	$0, lapic_eoi		/* End Of Interrupt to APIC */

	movl	$0, %ebx		
	movl	16(%esp), %eax	
	andl	$3, %eax
	cmpl	$3, %eax
	je	1f
#ifdef VM86
	testl	$PSL_VM, 20(%esp)
	jne	1f
#endif
	incl	%ebx			/* system or interrupt */
#ifdef CPL_AND_CML	
	cmpl	$0, _inside_intr
	je	1f
	incl	%ebx			/* interrupt */
#endif
1:	
	movl	_cpuid, %eax
	movl	%ebx, _checkstate_cpustate(,%eax,4)
	movl	_curproc, %ebx
	movl	%ebx, _checkstate_curproc(,%eax,4)
	movl	12(%esp), %ebx
	movl	%ebx, _checkstate_pc(,%eax,4)

	lock				/* checkstate_probed_cpus |= (1<<id) */
	btsl	%eax, _checkstate_probed_cpus

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
	movl	%ax, %ds		/* use KERNEL data segment */
	movl	%ax, %es

	movl	_cpuid, %eax
	lock				/* checkstate_need_ast &= ~(1<<id) */
	btrl	%eax, _checkstate_need_ast
	movl	$0, lapic_eoi		/* End Of Interrupt to APIC */

	lock
	btsl	%eax, _checkstate_pending_ast
	jc	1f

	FAKE_MCOUNT(12*4(%esp))

	/* 
	 * Giant locks do not come cheap.
	 * A lot of cycles are going to be wasted here.
	 */
	call	_get_isrlock

	AVCPL_LOCK
#ifdef CPL_AND_CML
	movl	_cml, %eax
#else
	movl	_cpl, %eax
#endif
	pushl	%eax
	lock
	orl	$SWI_AST_PENDING, _ipending
	AVCPL_UNLOCK
	lock
	incb	_intr_nesting_level
	sti
	
	pushl	$0
	
	movl	_cpuid, %eax
	lock	
	btrl	%eax, _checkstate_pending_ast

	MEXITCOUNT
	jmp	_doreti
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
	movl	%ax, %ds		/* use KERNEL data segment */
	movl	%ax, %es

	movl	$0, lapic_eoi		/* End Of Interrupt to APIC */

	FAKE_MCOUNT(12*4(%esp))

	ISR_TRYLOCK
	testl	%eax,%eax		/* Did we get the lock ? */
	jz  1f				/* No */

	lock
	incl	CNAME(forward_irq_hitcnt)
	cmpb	$4, _intr_nesting_level
	jae	2f
	
	jmp	3f

	AVCPL_LOCK
#ifdef CPL_AND_CML
	movl	_cml, %eax
#else
	movl	_cpl, %eax
#endif
	pushl	%eax
	AVCPL_UNLOCK
	lock
	incb	_intr_nesting_level
	sti
	
	pushl	$0

	MEXITCOUNT
	jmp	_doreti			/* Handle forwarded interrupt */
4:	
	lock	
	decb	_intr_nesting_level
	ISR_RELLOCK
	MEXITCOUNT
	addl	$8, %esp
	POP_FRAME
	iret
1:
	lock
	incl	CNAME(forward_irq_misscnt)
	call	forward_irq	/* Oops, we've lost the isr lock */
	MEXITCOUNT
	POP_FRAME
	iret
2:
	lock
	incl	CNAME(forward_irq_toodeepcnt)
3:	
	ISR_RELLOCK
	MEXITCOUNT
	POP_FRAME
	iret

/*
 * 
 */
forward_irq:
	MCOUNT
	cmpl	$0,_invltlb_ok
	jz	4f

	cmpl	$0, CNAME(forward_irq_enabled)
	jz	4f

	movl	_mp_lock,%eax
	cmpl	$FREE_LOCK,%eax
	jne	1f
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
	pushl	%eax
	pushl	%ds			/* save current data segment */

	movl	$KDSEL, %eax
	movl	%ax, %ds		/* use KERNEL data segment */

	movl	_cpuid, %eax

	lock
	btsl	%eax, _stopped_cpus	/* stopped_cpus |= (1<<id) */
1:
	btl	%eax, _started_cpus	/* while (!(started_cpus & (1<<id))) */
	jnc	1b

	lock
	btrl	%eax, _started_cpus	/* started_cpus &= ~(1<<id) */

	movl	$0, lapic_eoi		/* End Of Interrupt to APIC */

	popl	%ds			/* restore previous data segment */
	popl	%eax
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
	INTR(0,intr0)
	INTR(1,intr1)
	INTR(2,intr2)
	INTR(3,intr3)
	INTR(4,intr4)
	INTR(5,intr5)
	INTR(6,intr6)
	INTR(7,intr7)
	INTR(8,intr8)
	INTR(9,intr9)
	INTR(10,intr10)
	INTR(11,intr11)
	INTR(12,intr12)
	INTR(13,intr13)
	INTR(14,intr14)
	INTR(15,intr15)
	INTR(16,intr16)
	INTR(17,intr17)
	INTR(18,intr18)
	INTR(19,intr19)
	INTR(20,intr20)
	INTR(21,intr21)
	INTR(22,intr22)
	INTR(23,intr23)
MCOUNT_LABEL(eintr)

	.data
/*
 * Addresses of interrupt handlers.
 *  XresumeNN: Resumption addresses for HWIs.
 */
	.globl _ihandlers
_ihandlers:
ihandlers:
/*
 * used by:
 *  ipl.s:	doreti_unpend
 */
	.long	Xresume0,  Xresume1,  Xresume2,  Xresume3 
	.long	Xresume4,  Xresume5,  Xresume6,  Xresume7
	.long	Xresume8,  Xresume9,  Xresume10, Xresume11
	.long	Xresume12, Xresume13, Xresume14, Xresume15 
	.long	Xresume16, Xresume17, Xresume18, Xresume19
	.long	Xresume20, Xresume21, Xresume22, Xresume23
/*
 * used by:
 *  ipl.s:	doreti_unpend
 *  apic_ipl.s:	splz_unpend
 */
	.long	swi_tty, swi_net
	.long	dummycamisr, dummycamisr
	.long	_swi_vm, 0
	.long	_softclock, swi_ast

imasks:				/* masks for interrupt handlers */
	.space	NHWI*4		/* padding; HWI masks are elsewhere */

	.long	SWI_TTY_MASK, SWI_NET_MASK
	.long	SWI_CAMNET_MASK, SWI_CAMBIO_MASK
	.long	SWI_VM_MASK, 0
	.long	SWI_CLOCK_MASK, SWI_AST_MASK

/* active flag for lazy masking */
iactive:
	.long	0

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
CNAME(forward_irq_misscnt):	
	.long 0
CNAME(forward_irq_hitcnt):	
	.long 0
CNAME(forward_irq_toodeepcnt):
	.long 0


	.globl	_apic_pin_trigger
_apic_pin_trigger:
	.space	(NAPIC * 4), 0


/*
 * Interrupt counters and names.  The format of these and the label names
 * must agree with what vmstat expects.  The tables are indexed by device
 * ids so that we don't have to move the names around as devices are
 * attached.
 */
#include "vector.h"
	.globl	_intrcnt, _eintrcnt
_intrcnt:
	.space	(NR_DEVICES + ICU_LEN) * 4
_eintrcnt:

	.globl	_intrnames, _eintrnames
_intrnames:
	.ascii	DEVICE_NAMES
	.asciz	"stray irq0"
	.asciz	"stray irq1"
	.asciz	"stray irq2"
	.asciz	"stray irq3"
	.asciz	"stray irq4"
	.asciz	"stray irq5"
	.asciz	"stray irq6"
	.asciz	"stray irq7"
	.asciz	"stray irq8"
	.asciz	"stray irq9"
	.asciz	"stray irq10"
	.asciz	"stray irq11"
	.asciz	"stray irq12"
	.asciz	"stray irq13"
	.asciz	"stray irq14"
	.asciz	"stray irq15"
	.asciz	"stray irq16"
	.asciz	"stray irq17"
	.asciz	"stray irq18"
	.asciz	"stray irq19"
	.asciz	"stray irq20"
	.asciz	"stray irq21"
	.asciz	"stray irq22"
	.asciz	"stray irq23"
_eintrnames:

	.text
