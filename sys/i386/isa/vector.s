/*
 *	from: vector.s, 386BSD 0.1 unknown origin
 *	$Id: vector.s,v 1.7 1994/04/02 07:00:50 davidg Exp $
 */

#include "i386/isa/icu.h"
#include "i386/isa/isa.h"
#include "vector.h"

#define	ICU_EOI			0x20	/* XXX - define elsewhere */

#define	IRQ_BIT(irq_num)	(1 << ((irq_num) % 8))
#define	IRQ_BYTE(irq_num)	((irq_num) / 8)

#ifdef AUTO_EOI_1
#define	ENABLE_ICU1		/* use auto-EOI to reduce i/o */
#else
#define	ENABLE_ICU1 \
	movb	$ICU_EOI,%al ;	/* as soon as possible send EOI ... */ \
	FASTER_NOP ;		/* ... ASAP ... */ \
	outb	%al,$IO_ICU1	/* ... to clear in service bit */
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
	outb	%al,$IO_ICU2 ;	/* but do second icu first */ \
	FASTER_NOP ; \
	outb	%al,$IO_ICU1	/* then first icu */
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

#define	FAST_INTR(unit, irq_num, id_num, handler, enable_icus) \
	pushl	%eax ;		/* save only call-used registers */ \
	pushl	%ecx ; \
	pushl	%edx ; \
	pushl	%ds ; \
	MAYBE_PUSHL_ES ; \
	movl	$KDSEL,%eax ; \
	movl	%ax,%ds ; \
	MAYBE_MOVW_AX_ES ; \
	FAKE_MCOUNT((4+ACTUALLY_PUSHED)*4(%esp)) ; \
	pushl	$unit ; \
	call	handler ;	/* do the work ASAP */ \
	enable_icus ;		/* (re)enable ASAP (helps edge trigger?) */ \
	addl	$4,%esp ; \
	incl	_cnt+V_INTR ;	/* book-keeping can wait */ \
	incl	_intrcnt_actv + (id_num) * 4 ; \
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
	xchgl	%eax,(1+ACTUALLY_PUSHED)*4(%esp) ; /* orig %eax; save cpl */ \
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

#define	INTR(unit, irq_num, id_num, mask, handler, icu, enable_icus, reg, stray) \
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
1: ; \
	FAKE_MCOUNT(12*4(%esp)) ;	/* XXX late to avoid double count */ \
	incl	_intrcnt_actv + (id_num) * 4 ; \
	movl	_cpl,%eax ; \
	pushl	%eax ; \
	pushl	$unit ; \
	orl	mask,%eax ; \
	movl	%eax,_cpl ; \
	sti ; \
	call	handler ; \
	movb	_imen + IRQ_BYTE(irq_num),%al ; \
	andb	$~IRQ_BIT(irq_num),%al ; \
	movb	%al,_imen + IRQ_BYTE(irq_num) ; \
	FASTER_NOP ; \
	outb	%al,$icu+1 ; \
	MEXITCOUNT ; \
	/* We could usually avoid the following jmp by inlining some of */ \
	/* _doreti, but it's probably better to use less cache. */ \
	jmp	_doreti ; \
; \
	ALIGN_TEXT ; \
2: ; \
	/* XXX skip mcounting here to avoid double count */ \
	movl	$1b,%eax ;	/* register resume address */ \
				/* XXX - someday do it at attach time */ \
	movl	%eax,ihandlers + (irq_num) * 4 ;	\
	orb	$IRQ_BIT(irq_num),_ipending + IRQ_BYTE(irq_num) ; \
	popl	%es ; \
	popl	%ds ; \
	popal ; \
	addl	$4+4,%esp ; \
	iret

/*
 * vector.h has defined a macro 'BUILD_VECTORS' containing a big list of info
 * about vectors, including a submacro 'BUILD_VECTOR' that operates on the
 * info about each vector.  We redefine 'BUILD_VECTOR' to expand the info
 * in different ways.  Here we expand it to a list of interrupt handlers.
 * This order is of course unimportant.  Elsewhere we expand it to inline
 * linear search code for which the order is a little more important and
 * concatenating the code with no holes is very important.
 *
 * XXX - now there is BUILD_FAST_VECTOR as well as BUILD_VECTOR.
 *
 * The info consists of the following items for each vector:
 *
 *	name (identifier):	name of the vector; used to build labels
 *	unit (expression):	unit number to call the device driver with
 *	irq_num (number):	number of the IRQ to handled (0-15)
 *	id_num (number):	uniq numeric id for handler (assigned by config)
 *	mask (blank-ident):	priority mask used
 *	handler (blank-ident):	interrupt handler to call
 *	icu_num (number):	(1 + irq_num / 8) converted for label building
 *	icu_enables (number):	1 for icu_num == 1, 1_AND_2 for icu_num == 2
 *	reg (blank-ident):	al for icu_num == 1, ah for icu_num == 2
 *
 * 'irq_num' is converted in several ways at config time to get around
 * limitations in cpp.  The macros have blanks after commas iff they would
 * not mess up identifiers and numbers.
 */

#undef BUILD_FAST_VECTOR
#define	BUILD_FAST_VECTOR(name, unit, irq_num, id_num, mask, handler, \
			  icu_num, icu_enables, reg) \
	.globl	handler ; \
	.text ; \
	.globl	_V/**/name ; \
	SUPERALIGN_TEXT ; \
_V/**/name: ; \
	FAST_INTR(unit, irq_num,id_num, handler, ENABLE_ICU/**/icu_enables)

#undef BUILD_VECTOR
#define	BUILD_VECTOR(name, unit, irq_num, id_num, mask, handler, \
		     icu_num, icu_enables, reg) \
	.globl	handler ; \
	.text ; \
	.globl	_V/**/name ; \
	SUPERALIGN_TEXT ; \
_V/**/name: ; \
	INTR(unit,irq_num, id_num, mask, handler, IO_ICU/**/icu_num, \
	     ENABLE_ICU/**/icu_enables, reg,)

MCOUNT_LABEL(bintr)
	BUILD_VECTORS

	/* hardware interrupt catcher (IDT 32 - 47) */
	.globl	_isa_strayintr

#define	STRAYINTR(irq_num, icu_num, icu_enables, reg) \
IDTVEC(intr/**/irq_num) ; \
	INTR(irq_num,irq_num,irq_num, _high_imask,  _isa_strayintr, \
		  IO_ICU/**/icu_num, ENABLE_ICU/**/icu_enables, reg,stray)

/*
 * XXX - the mask (1 << 2) == IRQ_SLAVE will be generated for IRQ 2, instead
 * of the mask IRQ2 (defined as IRQ9 == (1 << 9)).  But IRQ 2 "can't happen".
 * In fact, all stray interrupts "can't happen" except for bugs.  The
 * "stray" IRQ 7 is documented behaviour of the 8259.  It happens when there
 * is a glitch on any of its interrupt inputs.  Does it really interrupt when
 * IRQ 7 is masked?
 *
 * XXX - unpend doesn't work for these, it sends them to the real handler.
 *
 * XXX - the race bug during initialization may be because I changed the
 * order of switching from the stray to the real interrupt handler to before
 * enabling interrupts.  The old order looked unsafe but maybe it is OK with
 * the stray interrupt handler installed.  But these handlers only reduce
 * the window of vulnerability - it is still open at the end of
 * isa_configure().
 *
 * XXX - many comments are stale.
 */

	STRAYINTR(0,1,1, al)
	STRAYINTR(1,1,1, al)
	STRAYINTR(2,1,1, al)
	STRAYINTR(3,1,1, al)
	STRAYINTR(4,1,1, al)
	STRAYINTR(5,1,1, al)
	STRAYINTR(6,1,1, al)
	STRAYINTR(7,1,1, al)
	STRAYINTR(8,2,1_AND_2, ah)
	STRAYINTR(9,2,1_AND_2, ah)
	STRAYINTR(10,2,1_AND_2, ah)
	STRAYINTR(11,2,1_AND_2, ah)
	STRAYINTR(12,2,1_AND_2, ah)
	STRAYINTR(13,2,1_AND_2, ah)
	STRAYINTR(14,2,1_AND_2, ah)
	STRAYINTR(15,2,1_AND_2, ah)
#if 0
	INTRSTRAY(255, _highmask, 255) ; call	_isa_strayintr ; INTREXIT2
#endif
MCOUNT_LABEL(eintr)

/*
 * These are the interrupt counters, I moved them here from icu.s so that
 * they are with the name table.  rgrimes
 *
 * There are now lots of counters, this has been redone to work with
 * Bruce Evans intr-0.1 code, which I modified some more to make it all
 * work with vmstat.
 */
	.data
ihandlers:			/* addresses of interrupt handlers */
	.space	NHWI*4		/* actually resumption addresses for HWI's */
	.long	swi_tty, swi_net, 0, 0, 0, 0, 0, 0
	.long	0, 0, 0, 0, 0, 0, swi_clock, swi_ast
imasks:				/* masks for interrupt handlers */
	.space	NHWI*4		/* padding; HWI masks are elsewhere */
	.long	SWI_TTY_MASK, SWI_NET_MASK, 0, 0, 0, 0, 0, 0
	.long	0, 0, 0, 0, 0, 0, SWI_CLOCK_MASK, SWI_AST_MASK

	.globl	_intrcnt
_intrcnt:			/* used by vmstat to calc size of table */
	.globl	_intrcnt_bad7
_intrcnt_bad7:	.space	4	/* glitches on irq 7 */
	.globl	_intrcnt_bad15
_intrcnt_bad15:	.space	4	/* glitches on irq 15 */
	.globl	_intrcnt_stray
_intrcnt_stray:	.space	4	/* total count of stray interrupts */
	.globl	_intrcnt_actv
_intrcnt_actv:	.space	NR_REAL_INT_HANDLERS * 4	/* active interrupts */
	.globl	_eintrcnt
_eintrcnt:			/* used by vmstat to calc size of table */

/*
 * Build the interrupt name table for vmstat
 */

#undef BUILD_FAST_VECTOR
#define BUILD_FAST_VECTOR	BUILD_VECTOR

#undef BUILD_VECTOR
#define	BUILD_VECTOR(name, unit, irq_num, id_num, mask, handler, \
		     icu_num, icu_enables, reg) \
	.ascii	"name irq" ; \
	.asciz	"irq_num"
/*
 * XXX - use the __STRING and __CONCAT macros from <sys/cdefs.h> to stringize
 * and concatenate names above and elsewhere.  Note that __CONCAT doesn't
 * work when nested.
 */

	.text
	.globl	_intrnames, _eintrnames
_intrnames:
	BUILD_VECTOR(bad,,7,,,,,,)
	BUILD_VECTOR(bad,,15,,,,,,)
	BUILD_VECTOR(stray,,,,,,,,)
	BUILD_VECTORS

_eintrnames:
