/*
 *	from: vector.s, 386BSD 0.1 unknown origin
 *	$Id: apic_vector.s,v 1.13 1997/07/31 05:42:05 fsmp Exp $
 */


#include <machine/smp.h>
#include <machine/smptests.h>		/** PEND_INTS, various counters */
#include "i386/isa/intr_machdep.h"

/* convert an absolute IRQ# into a bitmask */
#define IRQ_BIT(irq_num)	(1 << (irq_num))

/* make an index into the IO APIC from the IRQ# */
#define REDTBL_IDX(irq_num)	(0x10 + ((irq_num) * 2))

/*
 * 'lazy masking' code suggested by Bruce Evans <bde@zeta.org.au>
 */

#ifdef PEND_INTS

/*
 * the 1st version fails because masked edge-triggered INTs are lost
 * by the IO APIC.  This version tests to see whether we are handling
 * an edge or level triggered INT.  Level-triggered INTs must still be
 * masked as we don't clear the source, and the EOI cycle would allow
 * recursive INTs to occur.
 */
#define MAYBE_MASK_IRQ(irq_num)						\
	lock ;					/* MP-safe */		\
	btsl	$(irq_num),iactive ;		/* lazy masking */	\
	jc	6f ;				/* already active */	\
	TRY_ISRLOCK(irq_num) ;			/* try to get lock */	\
	testl	%eax, %eax ;			/* did we get it? */	\
	jnz	8f ;				/* yes, enter kernel */	\
6: ;						/* active or locked */	\
	IMASK_LOCK ;				/* into critical reg */	\
	testl	$IRQ_BIT(irq_num),_apic_pin_trigger ;			\
	jz	7f ;				/* edge, don't mask */	\
	orl	$IRQ_BIT(irq_num),_apic_imen ;	/* set the mask bit */	\
	movl	_ioapic,%ecx ;			/* ioapic[0] addr */	\
	movl	$REDTBL_IDX(irq_num),(%ecx) ;	/* write the index */	\
	movl	IOAPIC_WINDOW(%ecx),%eax ;	/* current value */	\
	orl	$IOART_INTMASK,%eax ;		/* set the mask */	\
	movl	%eax,IOAPIC_WINDOW(%ecx) ;	/* new value */		\
7: ;									\
	lock ;					/* MP-safe */		\
	orl	$IRQ_BIT(irq_num), _ipending ;	/* set _ipending bit */	\
	IMASK_UNLOCK ;				/* exit critical reg */	\
	movl	$0, lapic_eoi ;			/* do the EOI */	\
	popl	%es ;							\
	popl	%ds ;							\
	popal ;								\
	addl	$4+4,%esp ;						\
	iret ;								\
;									\
	ALIGN_TEXT ;							\
8:

#else /* PEND_INTS */

#define MAYBE_MASK_IRQ(irq_num)						\
	lock ;					/* MP-safe */		\
	btsl	$(irq_num),iactive ;		/* lazy masking */	\
	jnc	1f ;				/* NOT active */	\
	IMASK_LOCK ;				/* enter critical reg */\
	orl	$IRQ_BIT(irq_num),_apic_imen ;	/* set the mask bit */	\
	movl	_ioapic,%ecx ;			/* ioapic[0]addr */	\
	movl	$REDTBL_IDX(irq_num),(%ecx) ;	/* write the index */	\
	movl	IOAPIC_WINDOW(%ecx),%eax ;	/* current value */	\
	orl	$IOART_INTMASK,%eax ;		/* set the mask */	\
	movl	%eax,IOAPIC_WINDOW(%ecx) ;	/* new value */		\
	lock ;					/* MP-safe */		\
	orl	$IRQ_BIT(irq_num), _ipending ;	/* set _ipending bit */	\
	movl	$0, lapic_eoi ;			/* do the EOI */	\
	IMASK_UNLOCK ;				/* exit critical reg */	\
	popl	%es ;							\
	popl	%ds ;							\
	popal ;								\
	addl	$4+4,%esp ;						\
	iret ;								\
;									\
	ALIGN_TEXT ;							\
1: ;									\
	GET_MPLOCK				/* SMP Spin lock */

#endif /* PEND_INTS */


#define MAYBE_UNMASK_IRQ(irq_num)					\
	cli ;	/* must unmask _apic_imen and IO APIC atomically */	\
	lock ;					/* MP-safe */		\
	andl	$~IRQ_BIT(irq_num),iactive ;				\
	IMASK_LOCK ;				/* enter critical reg */\
	testl	$IRQ_BIT(irq_num),_apic_imen ;				\
	je	9f ;							\
	andl	$~IRQ_BIT(irq_num),_apic_imen ;	/* clear mask bit */	\
	movl	_ioapic,%ecx ;			/* ioapic[0]addr */	\
	movl	$REDTBL_IDX(irq_num),(%ecx) ;	/* write the index */	\
	movl	IOAPIC_WINDOW(%ecx),%eax ;	/* current value */	\
	andl	$~IOART_INTMASK,%eax ;		/* clear the mask */	\
	movl	%eax,IOAPIC_WINDOW(%ecx) ;	/* new value */		\
9: ;									\
	IMASK_UNLOCK ;				/* exit critical reg */	\
	sti	/* XXX _doreti repeats the cli/sti */


/*
 * Macros for interrupt interrupt entry, call to handler, and exit.
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
	GET_ISRLOCK(irq_num) ;						\
	pushl	_intr_unit + (irq_num) * 4 ;				\
	call	*_intr_handler + (irq_num) * 4 ; /* do the work ASAP */ \
	movl	$0, lapic_eoi ;						\
	addl	$4,%esp ;						\
	incl	_cnt+V_INTR ;	/* book-keeping can wait */		\
	movl	_intr_countp + (irq_num) * 4,%eax ;			\
	incl	(%eax) ;						\
	movl	_cpl,%eax ;	/* unmasking pending HWIs or SWIs? */	\
	notl	%eax ;							\
	andl	_ipending,%eax ;					\
	jne	2f ; 		/* yes, maybe handle them */		\
1: ;									\
	MEXITCOUNT ;							\
	REL_ISRLOCK(irq_num) ;						\
	MAYBE_POPL_ES ;							\
	popl	%ds ;							\
	popl	%edx ;							\
	popl	%ecx ;							\
	popl	%eax ;							\
	iret ;								\
;									\
	ALIGN_TEXT ;							\
2: ;									\
	cmpb	$3,_intr_nesting_level ;	/* enough stack? */	\
	jae	1b ;		/* no, return */			\
	movl	_cpl,%eax ;						\
	/* XXX next line is probably unnecessary now. */		\
	movl	$HWI_MASK|SWI_MASK,_cpl ;	/* limit nesting ... */	\
	incb	_intr_nesting_level ;	/* ... really limit it ... */	\
	sti ;			/* to do this as early as possible */	\
	MAYBE_POPL_ES ;		/* discard most of thin frame ... */	\
	popl	%ecx ;		/* ... original %ds ... */		\
	popl	%edx ;							\
	xchgl	%eax,4(%esp) ;	/* orig %eax; save cpl */		\
	pushal ;		/* build fat frame (grrr) ... */	\
	pushl	%ecx ;		/* ... actually %ds ... */		\
	pushl	%es ;							\
	movl	$KDSEL,%eax ;						\
	movl	%ax,%es ;						\
	movl	(2+8+0)*4(%esp),%ecx ;	/* %ecx from thin frame ... */	\
	movl	%ecx,(2+6)*4(%esp) ;	/* ... to fat frame ... */	\
	movl	(2+8+1)*4(%esp),%eax ;	/* ... cpl from thin frame */	\
	pushl	%eax ;							\
	subl	$4,%esp ;	/* junk for unit number */		\
	MEXITCOUNT ;							\
	jmp	_doreti

#define	INTR(irq_num, vec_name)						\
	.text ;								\
	SUPERALIGN_TEXT ;						\
IDTVEC(vec_name) ;							\
	pushl	$0 ;		/* dummy error code */			\
	pushl	$0 ;		/* dummy trap type */			\
	pushal ;							\
	pushl	%ds ;		/* save data and extra segments ... */	\
	pushl	%es ;							\
	movl	$KDSEL,%eax ;	/* ... and reload with kernel's ... */	\
	movl	%ax,%ds ;	/* ... early for obsolete reasons */	\
	movl	%ax,%es ;						\
	MAYBE_MASK_IRQ(irq_num) ;					\
	movl	$0, lapic_eoi ;						\
	movl	_cpl,%eax ;						\
	testl	$IRQ_BIT(irq_num), %eax ;				\
	jne	3f ;							\
	incb	_intr_nesting_level ;					\
__CONCAT(Xresume,irq_num): ;						\
	FAKE_MCOUNT(12*4(%esp)) ;	/* XXX late to avoid dbl cnt */ \
	incl	_cnt+V_INTR ;	/* tally interrupts */			\
	movl	_intr_countp + (irq_num) * 4,%eax ;			\
	incl	(%eax) ;						\
	movl	_cpl,%eax ;						\
	pushl	%eax ;							\
	pushl	_intr_unit + (irq_num) * 4 ;				\
	orl	_intr_mask + (irq_num) * 4,%eax ;			\
	movl	%eax,_cpl ;						\
	sti ;								\
	call	*_intr_handler + (irq_num) * 4 ;			\
	MAYBE_UNMASK_IRQ(irq_num) ;					\
	MEXITCOUNT ;							\
	jmp	_doreti ;						\
;									\
	ALIGN_TEXT ;							\
3: ;									\
	/* XXX skip mcounting here to avoid double count */		\
	lock ;					/* MP-safe */		\
	orl	$IRQ_BIT(irq_num), _ipending ;				\
	REL_ISRLOCK(irq_num) ;						\
	popl	%es ;							\
	popl	%ds ;							\
	popal ;								\
	addl	$4+4,%esp ;						\
	iret


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
#ifdef COUNT_SPURIOUS_INTS
	ss
	incl	_sihits
#endif

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

#ifdef COUNT_CSHITS
	incl	_cshits(,%eax,4)
#endif /* COUNT_CSHITS */

	ASMPOSTCODE_HI(0x1)

	lock
	btsl	%eax, _stopped_cpus	/* stopped_cpus |= (1<<id) */

	ASMPOSTCODE_HI(0x2);
1:
	btl	%eax, _started_cpus	/* while (!(started_cpus & (1<<id))) */
	jnc	1b

	ASMPOSTCODE_HI(0x3)

	lock
	btrl	%eax, _started_cpus	/* started_cpus &= ~(1<<id) */

	ASMPOSTCODE_HI(0x4)

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
ihandlers:			/* addresses of interrupt handlers */
				/* actually resumption addresses for HWI's */
	.long	Xresume0,  Xresume1,  Xresume2,  Xresume3 
	.long	Xresume4,  Xresume5,  Xresume6,  Xresume7
	.long	Xresume8,  Xresume9,  Xresume10, Xresume11
	.long	Xresume12, Xresume13, Xresume14, Xresume15 
	.long	Xresume16, Xresume17, Xresume18, Xresume19
	.long	Xresume20, Xresume21, Xresume22, Xresume23
	.long	swi_tty,   swi_net
	.long	0, 0, 0, 0
	.long	_softclock, swi_ast

imasks:				/* masks for interrupt handlers */
	.space	NHWI*4		/* padding; HWI masks are elsewhere */

	.long	SWI_TTY_MASK, SWI_NET_MASK
	.long	0, 0, 0, 0
	.long	SWI_CLOCK_MASK, SWI_AST_MASK

	.globl _ivectors
_ivectors:
	.long	_Xintr0,  _Xintr1,  _Xintr2,  _Xintr3 
	.long	_Xintr4,  _Xintr5,  _Xintr6,  _Xintr7
	.long	_Xintr8,  _Xintr9,  _Xintr10, _Xintr11
	.long	_Xintr12, _Xintr13, _Xintr14, _Xintr15 
	.long	_Xintr16, _Xintr17, _Xintr18, _Xintr19
	.long	_Xintr20, _Xintr21, _Xintr22, _Xintr23

/* active flag for lazy masking */
iactive:
	.long	0

#ifdef COUNT_SPURIOUS_INTS
	.globl	_sihits
_sihits:
	.long	0
#endif /* COUNT_SPURIOUS_INTS */

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

#ifdef COUNT_CSHITS
	.globl	_cshits
_cshits:
	.space	(NCPU * 4), 0
#endif /* COUNT_CSHITS */

#ifdef PEND_INTS
	.globl	_apic_pin_trigger
_apic_pin_trigger:
	.space	(NAPIC * 4), 0
#endif /* PEND_INTS */


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
