/*
 *	from: vector.s, 386BSD 0.1 unknown origin
 *	$Id: icu_vector.s,v 1.6 1997/09/28 19:30:01 gibbs Exp $
 */

/*
 * modified for PC98 by Kakefuda
 */

#ifdef PC98
#define ICU_IMR_OFFSET		2	/* IO_ICU{1,2} + 2 */
#else
#define ICU_IMR_OFFSET		1	/* IO_ICU{1,2} + 1 */
#endif

#define	ICU_EOI			0x20	/* XXX - define elsewhere */

#define	IRQ_BIT(irq_num)	(1 << ((irq_num) % 8))
#define	IRQ_BYTE(irq_num)	((irq_num) >> 3)

#ifdef AUTO_EOI_1
#define	ENABLE_ICU1		/* use auto-EOI to reduce i/o */
#define	OUTB_ICU1
#else
#define	ENABLE_ICU1 \
	movb	$ICU_EOI,%al ;	/* as soon as possible send EOI ... */ \
	OUTB_ICU1		/* ... to clear in service bit */
#define	OUTB_ICU1 \
	outb	%al,$IO_ICU1
#endif

#ifdef AUTO_EOI_2
/*
 * The data sheet says no auto-EOI on slave, but it sometimes works.
 */
#define	ENABLE_ICU1_AND_2	ENABLE_ICU1
#else
#define	ENABLE_ICU1_AND_2 \
	movb	$ICU_EOI,%al ;	/* as above */ \
	outb	%al,$IO_ICU2 ;	/* but do second icu first ... */ \
	OUTB_ICU1		/* ... then first icu (if !AUTO_EOI_1) */
#endif

/*
 * Macros for interrupt interrupt entry, call to handler, and exit.
 */

#define	FAST_INTR(irq_num, vec_name, enable_icus) \
	.text ; \
	SUPERALIGN_TEXT ; \
IDTVEC(vec_name) ; \
	pushl	%eax ;		/* save only call-used registers */ \
	pushl	%ecx ; \
	pushl	%edx ; \
	pushl	%ds ; \
	MAYBE_PUSHL_ES ; \
	movl	$KDSEL,%eax ; \
	movl	%ax,%ds ; \
	MAYBE_MOVW_AX_ES ; \
	FAKE_MCOUNT((4+ACTUALLY_PUSHED)*4(%esp)) ; \
	pushl	_intr_unit + (irq_num) * 4 ; \
	call	*_intr_handler + (irq_num) * 4 ; /* do the work ASAP */ \
	enable_icus ;		/* (re)enable ASAP (helps edge trigger?) */ \
	addl	$4,%esp ; \
	incl	_cnt+V_INTR ;	/* book-keeping can wait */ \
	movl	_intr_countp + (irq_num) * 4,%eax ; \
	incl	(%eax) ; \
	movl	_cpl,%eax ;	/* are we unmasking pending HWIs or SWIs? */ \
	notl	%eax ; \
	andl	_ipending,%eax ; \
	jne	2f ; 		/* yes, maybe handle them */ \
1: ; \
	MEXITCOUNT ; \
	MAYBE_POPL_ES ; \
	popl	%ds ; \
	popl	%edx ; \
	popl	%ecx ; \
	popl	%eax ; \
	iret ; \
; \
	ALIGN_TEXT ; \
2: ; \
	cmpb	$3,_intr_nesting_level ;	/* is there enough stack? */ \
	jae	1b ;		/* no, return */ \
	movl	_cpl,%eax ; \
	/* XXX next line is probably unnecessary now. */ \
	movl	$HWI_MASK|SWI_MASK,_cpl ;	/* limit nesting ... */ \
	incb	_intr_nesting_level ;	/* ... really limit it ... */ \
	sti ;			/* ... to do this as early as possible */ \
	MAYBE_POPL_ES ;		/* discard most of thin frame ... */ \
	popl	%ecx ;		/* ... original %ds ... */ \
	popl	%edx ; \
	xchgl	%eax,4(%esp) ;	/* orig %eax; save cpl */ \
	pushal ;		/* build fat frame (grrr) ... */ \
	pushl	%ecx ;		/* ... actually %ds ... */ \
	pushl	%es ; \
	movl	$KDSEL,%eax ; \
	movl	%ax,%es ; \
	movl	(2+8+0)*4(%esp),%ecx ;	/* ... %ecx from thin frame ... */ \
	movl	%ecx,(2+6)*4(%esp) ;	/* ... to fat frame ... */ \
	movl	(2+8+1)*4(%esp),%eax ;	/* ... cpl from thin frame */ \
	pushl	%eax ; \
	subl	$4,%esp ;	/* junk for unit number */ \
	MEXITCOUNT ; \
	jmp	_doreti

#define	INTR(irq_num, vec_name, icu, enable_icus, reg) \
	.text ; \
	SUPERALIGN_TEXT ; \
IDTVEC(vec_name) ; \
	pushl	$0 ;		/* dummy error code */ \
	pushl	$0 ;		/* dummy trap type */ \
	pushal ; \
	pushl	%ds ;		/* save our data and extra segments ... */ \
	pushl	%es ; \
	movl	$KDSEL,%eax ;	/* ... and reload with kernel's own ... */ \
	movl	%ax,%ds ;	/* ... early for obsolete reasons */ \
	movl	%ax,%es ; \
	movb	_imen + IRQ_BYTE(irq_num),%al ; \
	orb	$IRQ_BIT(irq_num),%al ; \
	movb	%al,_imen + IRQ_BYTE(irq_num) ; \
	outb	%al,$icu+ICU_IMR_OFFSET ; \
	enable_icus ; \
	movl	_cpl,%eax ; \
	testb	$IRQ_BIT(irq_num),%reg ; \
	jne	2f ; \
	incb	_intr_nesting_level ; \
__CONCAT(Xresume,irq_num): ; \
	FAKE_MCOUNT(12*4(%esp)) ;	/* XXX late to avoid double count */ \
	incl	_cnt+V_INTR ;	/* tally interrupts */ \
	movl	_intr_countp + (irq_num) * 4,%eax ; \
	incl	(%eax) ; \
	movl	_cpl,%eax ; \
	pushl	%eax ; \
	pushl	_intr_unit + (irq_num) * 4 ; \
	orl	_intr_mask + (irq_num) * 4,%eax ; \
	movl	%eax,_cpl ; \
	sti ; \
	call	*_intr_handler + (irq_num) * 4 ; \
	cli ;			/* must unmask _imen and icu atomically */ \
	movb	_imen + IRQ_BYTE(irq_num),%al ; \
	andb	$~IRQ_BIT(irq_num),%al ; \
	movb	%al,_imen + IRQ_BYTE(irq_num) ; \
	outb	%al,$icu+ICU_IMR_OFFSET ; \
	sti ;			/* XXX _doreti repeats the cli/sti */ \
	MEXITCOUNT ; \
	/* We could usually avoid the following jmp by inlining some of */ \
	/* _doreti, but it's probably better to use less cache. */ \
	jmp	_doreti ; \
; \
	ALIGN_TEXT ; \
2: ; \
	/* XXX skip mcounting here to avoid double count */ \
	orb	$IRQ_BIT(irq_num),_ipending + IRQ_BYTE(irq_num) ; \
	popl	%es ; \
	popl	%ds ; \
	popal ; \
	addl	$4+4,%esp ; \
	iret

MCOUNT_LABEL(bintr)
	FAST_INTR(0,fastintr0, ENABLE_ICU1)
	FAST_INTR(1,fastintr1, ENABLE_ICU1)
	FAST_INTR(2,fastintr2, ENABLE_ICU1)
	FAST_INTR(3,fastintr3, ENABLE_ICU1)
	FAST_INTR(4,fastintr4, ENABLE_ICU1)
	FAST_INTR(5,fastintr5, ENABLE_ICU1)
	FAST_INTR(6,fastintr6, ENABLE_ICU1)
	FAST_INTR(7,fastintr7, ENABLE_ICU1)
	FAST_INTR(8,fastintr8, ENABLE_ICU1_AND_2)
	FAST_INTR(9,fastintr9, ENABLE_ICU1_AND_2)
	FAST_INTR(10,fastintr10, ENABLE_ICU1_AND_2)
	FAST_INTR(11,fastintr11, ENABLE_ICU1_AND_2)
	FAST_INTR(12,fastintr12, ENABLE_ICU1_AND_2)
	FAST_INTR(13,fastintr13, ENABLE_ICU1_AND_2)
	FAST_INTR(14,fastintr14, ENABLE_ICU1_AND_2)
	FAST_INTR(15,fastintr15, ENABLE_ICU1_AND_2)
	INTR(0,intr0, IO_ICU1, ENABLE_ICU1, al)
	INTR(1,intr1, IO_ICU1, ENABLE_ICU1, al)
	INTR(2,intr2, IO_ICU1, ENABLE_ICU1, al)
	INTR(3,intr3, IO_ICU1, ENABLE_ICU1, al)
	INTR(4,intr4, IO_ICU1, ENABLE_ICU1, al)
	INTR(5,intr5, IO_ICU1, ENABLE_ICU1, al)
	INTR(6,intr6, IO_ICU1, ENABLE_ICU1, al)
	INTR(7,intr7, IO_ICU1, ENABLE_ICU1, al)
	INTR(8,intr8, IO_ICU2, ENABLE_ICU1_AND_2, ah)
	INTR(9,intr9, IO_ICU2, ENABLE_ICU1_AND_2, ah)
	INTR(10,intr10, IO_ICU2, ENABLE_ICU1_AND_2, ah)
	INTR(11,intr11, IO_ICU2, ENABLE_ICU1_AND_2, ah)
	INTR(12,intr12, IO_ICU2, ENABLE_ICU1_AND_2, ah)
	INTR(13,intr13, IO_ICU2, ENABLE_ICU1_AND_2, ah)
	INTR(14,intr14, IO_ICU2, ENABLE_ICU1_AND_2, ah)
	INTR(15,intr15, IO_ICU2, ENABLE_ICU1_AND_2, ah)
MCOUNT_LABEL(eintr)

	.data
	.globl	_ihandlers
_ihandlers:
ihandlers:			/* addresses of interrupt handlers */
				/* actually resumption addresses for HWI's */
	.long	Xresume0, Xresume1, Xresume2, Xresume3 
	.long	Xresume4, Xresume5, Xresume6, Xresume7
	.long	Xresume8, Xresume9, Xresume10, Xresume11
	.long	Xresume12, Xresume13, Xresume14, Xresume15 
	.long	swi_tty, swi_net, dummycamisr, dummycamisr
	.long	_swi_vm, 0, 0, 0
	.long	0, 0, 0, 0
	.long	0, 0, _softclock, swi_ast

imasks:				/* masks for interrupt handlers */
	.space	NHWI*4		/* padding; HWI masks are elsewhere */

	.long	SWI_TTY_MASK, SWI_NET_MASK, SWI_CAMNET_MASK, SWI_CAMBIO_MASK
	.long	SWI_VM_MASK, 0, 0, 0
	.long	0, 0, 0, 0
	.long	0, 0, SWI_CLOCK_MASK, SWI_AST_MASK

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
_eintrnames:

	.text
