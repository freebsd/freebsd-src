/*
 *	from: vector.s, 386BSD 0.1 unknown origin
 *	$Id: vector.s,v 1.2 1997/05/24 17:05:26 smp Exp smp $
 */

/*
 * modified for PC98 by Kakefuda
 */

#include "opt_auto_eoi.h"

#include <i386/isa/icu.h>
#ifdef PC98
#include <pc98/pc98/pc98.h>
#else
#include <i386/isa/isa.h>
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

	.data
	ALIGN_DATA

	.globl	_intr_nesting_level
_intr_nesting_level:
	.byte	0
	.space	3

	.text

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

#ifdef APIC_IO
#include "i386/isa/apic_vector.s"
#else
#include "i386/isa/icu_vector.s"
#endif  /* APIC_IO */
