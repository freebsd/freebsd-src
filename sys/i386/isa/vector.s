/*
 *	from: vector.s, 386BSD 0.1 unknown origin
 *	$Id: vector.s,v 1.13 1995/10/28 16:58:05 markm Exp $
 */

#include <i386/isa/icu.h>
#include <i386/isa/isa.h>

#define	ICU_EOI			0x20	/* XXX - define elsewhere */

#define	IRQ_BIT(irq_num)	(1 << ((irq_num) % 8))
#define	IRQ_BYTE(irq_num)	((irq_num) / 8)

#ifdef AUTO_EOI_1
#define	ENABLE_ICU1		/* use auto-EOI to reduce i/o */
#define	OUTB_ICU1
#else
#define	ENABLE_ICU1 \
	movb	$ICU_EOI,%al ;	/* as soon as possible send EOI ... */ \
	OUTB_ICU1		/* ... to clear in service bit */
#define	OUTB_ICU1 \
	FASTER_NOP ; \
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
	FASTER_NOP ; \
	outb	%al,$IO_ICU2 ;	/* but do second icu first ... */ \
	OUTB_ICU1		/* ... then first icu (if !AUTO_EOI_1) */
#endif

#ifdef FAST_INTR_HANDLER_USES_ES
#define	ACTUALLY_PUSHED		1
#define	MAYBE_MOVW_AX_ES	movl	%ax,%es
#define	MAYBE_POPL_ES		popl	%es
#define	MAYBE_PUSHL_ES		pushl	%es
#else
/*
 * We can usually skip loading %es for fastintr handlers.  %es should
 * only be used for string instructions, and fastintr handlers shouldn't
 * do anything slow enough to justify using a string instruction.
 */
#define	ACTUALLY_PUSHED		0
#define	MAYBE_MOVW_AX_ES
#define	MAYBE_POPL_ES
#define	MAYBE_PUSHL_ES
#endif

#define	ADDENTROPY(irq_num) \
	/* Add this interrupt to the pool of entropy */ \
	pushl	$irq_num ; \
	call	_add_interrupt_randomness ; \
	addl	$4,%esp

/*
 * Macros for interrupt interrupt entry, call to handler, and exit.
 *
 * XXX - the interrupt frame is set up to look like a trap frame.  This is
 * usually a waste of time.  The only interrupt handlers that want a frame
 * are the clock handler (it wants a clock frame), the npx handler (it's
 * easier to do right all in assembler).  The interrupt return routine
 * needs a trap frame for rare AST's (it could easily convert the frame).
 * The direct costs of setting up a trap frame are two pushl's (error
 * code and trap number), an addl to get rid of these, and pushing and
 * popping the call-saved regs %esi, %edi and %ebp twice,  The indirect
 * costs are making the driver interface nonuniform so unpending of
 * interrupts is more complicated and slower (call_driver(unit) would
 * be easier than ensuring an interrupt frame for all handlers.  Finally,
 * there are some struct copies in the npx handler and maybe in the clock
 * handler that could be avoided by working more with pointers to frames
 * instead of frames.
 *
 * XXX - should we do a cld on every system entry to avoid the requirement
 * for scattered cld's?
 *
 * Coding notes for *.s:
 *
 * If possible, avoid operations that involve an operand size override.
 * Word-sized operations might be smaller, but the operand size override
 * makes them slower on on 486's and no faster on 386's unless perhaps
 * the instruction pipeline is depleted.  E.g.,
 *
 *	Use movl to seg regs instead of the equivalent but more descriptive
 *	movw - gas generates an irelevant (slower) operand size override.
 *
 *	Use movl to ordinary regs in preference to movw and especially
 *	in preference to movz[bw]l.  Use unsigned (long) variables with the
 *	top bits clear instead of unsigned short variables to provide more
 *	opportunities for movl.
 *
 * If possible, use byte-sized operations.  They are smaller and no slower.
 *
 * Use (%reg) instead of 0(%reg) - gas generates larger code for the latter.
 *
 * If the interrupt frame is made more flexible,  INTR can push %eax first
 * and decide the ipending case with less overhead, e.g., by avoiding
 * loading segregs.
 */

#define	FAST_INTR(irq_num, enable_icus) \
	.text ; \
	SUPERALIGN_TEXT ; \
IDTVEC(fastintr/**/irq_num) ; \
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
	jne	1f ;		/* yes, handle them */ \
	MEXITCOUNT ; \
	MAYBE_POPL_ES ; \
	popl	%ds ; \
	popl	%edx ; \
	popl	%ecx ; \
	popl	%eax ; \
	iret ; \
; \
	ALIGN_TEXT ; \
1: ; \
	movl	_cpl,%eax ; \
	movl	$HWI_MASK|SWI_MASK,_cpl ;	/* limit nesting ... */ \
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
	incb	_intr_nesting_level ; \
	MEXITCOUNT ; \
	jmp	_doreti

#define	INTR(irq_num, icu, enable_icus, reg) \
	.text ; \
	SUPERALIGN_TEXT ; \
IDTVEC(intr/**/irq_num) ; \
	pushl	$0 ;		/* dumby error code */ \
	pushl	$0 ;		/* dumby trap type */ \
	pushal ; \
	pushl	%ds ;		/* save our data and extra segments ... */ \
	pushl	%es ; \
	movl	$KDSEL,%eax ;	/* ... and reload with kernel's own ... */ \
	movl	%ax,%ds ;	/* ... early for obsolete reasons */ \
	movl	%ax,%es ; \
	movb	_imen + IRQ_BYTE(irq_num),%al ; \
	orb	$IRQ_BIT(irq_num),%al ; \
	movb	%al,_imen + IRQ_BYTE(irq_num) ; \
	FASTER_NOP ; \
	outb	%al,$icu+1 ; \
	enable_icus ; \
	incl	_cnt+V_INTR ;	/* tally interrupts */ \
	movl	_cpl,%eax ; \
	testb	$IRQ_BIT(irq_num),%reg ; \
	jne	2f ; \
	incb	_intr_nesting_level ; \
Xresume/**/irq_num: ; \
	FAKE_MCOUNT(12*4(%esp)) ;	/* XXX late to avoid double count */ \
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
	FASTER_NOP ; \
	outb	%al,$icu+1 ; \
	sti ;			/* XXX _doreti repeats the cli/sti */ \
	/* Add this interrupt to the pool of entropy */ \
	ADDENTROPY(irq_num) ; \
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
	FAST_INTR(0, ENABLE_ICU1)
	FAST_INTR(1, ENABLE_ICU1)
	FAST_INTR(2, ENABLE_ICU1)
	FAST_INTR(3, ENABLE_ICU1)
	FAST_INTR(4, ENABLE_ICU1)
	FAST_INTR(5, ENABLE_ICU1)
	FAST_INTR(6, ENABLE_ICU1)
	FAST_INTR(7, ENABLE_ICU1)
	FAST_INTR(8, ENABLE_ICU1_AND_2)
	FAST_INTR(9, ENABLE_ICU1_AND_2)
	FAST_INTR(10, ENABLE_ICU1_AND_2)
	FAST_INTR(11, ENABLE_ICU1_AND_2)
	FAST_INTR(12, ENABLE_ICU1_AND_2)
	FAST_INTR(13, ENABLE_ICU1_AND_2)
	FAST_INTR(14, ENABLE_ICU1_AND_2)
	FAST_INTR(15, ENABLE_ICU1_AND_2)
	INTR(0, IO_ICU1, ENABLE_ICU1, al)
	INTR(1, IO_ICU1, ENABLE_ICU1, al)
	INTR(2, IO_ICU1, ENABLE_ICU1, al)
	INTR(3, IO_ICU1, ENABLE_ICU1, al)
	INTR(4, IO_ICU1, ENABLE_ICU1, al)
	INTR(5, IO_ICU1, ENABLE_ICU1, al)
	INTR(6, IO_ICU1, ENABLE_ICU1, al)
	INTR(7, IO_ICU1, ENABLE_ICU1, al)
	INTR(8, IO_ICU2, ENABLE_ICU1_AND_2, ah)
	INTR(9, IO_ICU2, ENABLE_ICU1_AND_2, ah)
	INTR(10, IO_ICU2, ENABLE_ICU1_AND_2, ah)
	INTR(11, IO_ICU2, ENABLE_ICU1_AND_2, ah)
	INTR(12, IO_ICU2, ENABLE_ICU1_AND_2, ah)
	INTR(13, IO_ICU2, ENABLE_ICU1_AND_2, ah)
	INTR(14, IO_ICU2, ENABLE_ICU1_AND_2, ah)
	INTR(15, IO_ICU2, ENABLE_ICU1_AND_2, ah)
MCOUNT_LABEL(eintr)

	.data
ihandlers:			/* addresses of interrupt handlers */
				/* actually resumption addresses for HWI's */
	.long	Xresume0, Xresume1, Xresume2, Xresume3 
	.long	Xresume4, Xresume5, Xresume6, Xresume7
	.long	Xresume8, Xresume9, Xresume10, Xresume11
	.long	Xresume12, Xresume13, Xresume14, Xresume15 
	.long	swi_tty, swi_net, 0, 0, 0, 0, 0, 0
	.long	0, 0, 0, 0, 0, 0, _softclock, swi_ast
imasks:				/* masks for interrupt handlers */
	.space	NHWI*4		/* padding; HWI masks are elsewhere */
	.long	SWI_TTY_MASK, SWI_NET_MASK, 0, 0, 0, 0, 0, 0
	.long	0, 0, 0, 0, 0, 0, SWI_CLOCK_MASK, SWI_AST_MASK
	.globl	_intr_nesting_level
_intr_nesting_level:
	.byte	0
	.space	3

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
