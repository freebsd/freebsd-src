/*
 *	from: vector.s, 386BSD 0.1 unknown origin
 *	$Id: vector.s,v 1.25 1997/02/22 09:37:23 peter Exp $
 */

/*
 * modified for PC98 by Kakefuda
 */

#include "opt_auto_eoi.h"
#include "opt_smp.h"

#if defined(SMP)
#include <machine/smpasm.h>	/* this includes <machine/apic.h> */
#include <machine/smptests.h>	/** TEST_CPUHITS */
#endif /* SMP */

#include <i386/isa/icu.h>
#ifdef PC98
#include <pc98/pc98/pc98.h>
#else
#include <i386/isa/isa.h>
#endif

#ifdef PC98
#define ICU_IMR_OFFSET		2	/* IO_ICU{1,2} + 2 */
#else
#define ICU_IMR_OFFSET		1	/* IO_ICU{1,2} + 1 */
#endif


#if defined(SMP)

#define GET_MPLOCK	call _get_mplock
#define REL_MPLOCK	call _rel_mplock

#else

#define GET_MPLOCK	/* NOP get Kernel Mutex */
#define REL_MPLOCK	/* NOP release mutex */

#endif /* SMP */


#if defined(APIC_IO)

#if defined(SMP) && defined(TEST_CPUHITS)

#undef GET_MPLOCK
#define GET_MPLOCK		\
	call	_get_mplock ; 	\
	GETCPUID(%eax) ; 	\
	incl	_cpuhits(,%eax,4)

#endif /* SMP && TEST_CPUHITS */

#define REDTBL_IDX(irq_num)	(0x10 + ((irq_num) * 2))
#define IRQ_BIT(irq_num)	(1 << (irq_num))

#define ENABLE_APIC			\
	movl	_apic_base, %eax ;	\
	movl	$0, APIC_EOI(%eax)

#define ENABLE_ICU1		ENABLE_APIC
#define ENABLE_ICU1_AND_2	ENABLE_APIC

#define MASK_IRQ(irq_num,icu)						\
	orl	$IRQ_BIT(irq_num),_imen ;	/* set the mask bit */	\
	movl	_io_apic_base,%ecx ;		/* io apic addr */	\
	movl	$REDTBL_IDX(irq_num),(%ecx) ;	/* write the index */	\
	movl	IOAPIC_WINDOW(%ecx),%eax ;	/* current value */	\
	orl	$IOART_INTMASK,%eax ;		/* set the mask */	\
	movl	%eax,IOAPIC_WINDOW(%ecx) ;	/* new value */

#define UNMASK_IRQ(irq_num,icu)						\
	andl	$~IRQ_BIT(irq_num),_imen ;	/* clear mask bit */	\
	movl	_io_apic_base,%ecx ;		/* io apic addr */	\
	movl	$REDTBL_IDX(irq_num),(%ecx) ;	/* write the index */	\
	movl	IOAPIC_WINDOW(%ecx),%eax ;	/* current value */	\
	andl	$~IOART_INTMASK,%eax ;		/* clear the mask */	\
	movl	%eax,IOAPIC_WINDOW(%ecx) ;	/* new value */

#define TEST_IRQ(irq_num,reg)						\
	testl	$IRQ_BIT(irq_num),%eax

#define SET_IPENDING(irq_num)						\
	orl	$IRQ_BIT(irq_num),_ipending

/*
 * 'lazy masking' code submitted by: Bruce Evans <bde@zeta.org.au>
 */
#define MAYBE_MASK_IRQ(irq_num,icu)					\
	testl	$IRQ_BIT(irq_num),iactive ;	/* lazy masking */	\
	je	1f ;			/* NOT currently active */	\
  	MASK_IRQ(irq_num,icu) ;						\
  	ENABLE_APIC ;							\
	SET_IPENDING(irq_num) ;						\
	REL_MPLOCK ;			/* SMP release global lock */	\
	popl	%es ;							\
	popl	%ds ;							\
	popal ;								\
	addl	$4+4,%esp ;						\
	iret ;								\
;									\
	ALIGN_TEXT ;							\
1: ;									\
	orl	$IRQ_BIT(irq_num),iactive

#define MAYBE_UNMASK_IRQ(irq_num,icu)					\
	andl	$~IRQ_BIT(irq_num),iactive ;				\
	testl	$IRQ_BIT(irq_num),_imen ;				\
	je	3f ;							\
	UNMASK_IRQ(irq_num,icu) ;					\
3:

#else /* APIC_IO */

#define MASK_IRQ(irq_num,icu)						\
	movb	_imen + IRQ_BYTE(irq_num),%al ;				\
	orb	$IRQ_BIT(irq_num),%al ;					\
	movb	%al,_imen + IRQ_BYTE(irq_num) ;				\
	outb	%al,$icu+ICU_IMR_OFFSET

#define UNMASK_IRQ(irq_num,icu)						\
	movb	_imen + IRQ_BYTE(irq_num),%al ;				\
	andb	$~IRQ_BIT(irq_num),%al ;				\
	movb	%al,_imen + IRQ_BYTE(irq_num) ;				\
	outb	%al,$icu+ICU_IMR_OFFSET

#define TEST_IRQ(irq_num,reg)						\
	testb	$IRQ_BIT(irq_num),%reg
	
#define SET_IPENDING(irq_num)						\
	orb	$IRQ_BIT(irq_num),_ipending + IRQ_BYTE(irq_num)

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

#define MAYBE_MASK_IRQ(irq_num,icu)					\
  	MASK_IRQ(irq_num,icu)

#define MAYBE_UNMASK_IRQ(irq_num,icu)					\
	UNMASK_IRQ(irq_num,icu)

#endif /* APIC_IO */


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
	GET_MPLOCK ;		/* SMP Spin lock */ \
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
	REL_MPLOCK ;		/* SMP release global lock */ \
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
	GET_MPLOCK ;		/* SMP Spin lock */ \
	MAYBE_MASK_IRQ(irq_num,icu) ; \
	enable_icus ; \
	movl	_cpl,%eax ; \
	TEST_IRQ(irq_num,reg) ; \
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
	MAYBE_UNMASK_IRQ(irq_num,icu) ; \
	sti ;			/* XXX _doreti repeats the cli/sti */ \
	MEXITCOUNT ; \
	/* We could usually avoid the following jmp by inlining some of */ \
	/* _doreti, but it's probably better to use less cache. */ \
	jmp	_doreti ; \
; \
	ALIGN_TEXT ; \
2: ; \
	/* XXX skip mcounting here to avoid double count */ \
	SET_IPENDING(irq_num) ; \
	REL_MPLOCK ;		/* SMP release global lock */  \
	popl	%es ; \
	popl	%ds ; \
	popal ; \
	addl	$4+4,%esp ; \
	iret


#if defined(APIC_IO) && defined(IPI_INTS)
/*
 * A simple IPI_INTR() macro based on a heavily cut down FAST_INTR().
 *  call it's handler, EOI and return.
 */
#define	IPI_INTR(irq_num, vec_name) \
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
	pushl	_intr_unit + (irq_num) * 4 ; \
	call	*_intr_handler + (irq_num) * 4 ; \
	ENABLE_APIC ; \
	addl	$4,%esp ; \
	incl	_cnt+V_INTR ;	/* book-keeping can wait */ \
	movl	_intr_countp + (irq_num) * 4,%eax ; \
	incl	(%eax) ; \
	MAYBE_POPL_ES ; \
	popl	%ds ; \
	popl	%edx ; \
	popl	%ecx ; \
	popl	%eax ; \
	iret
#endif /* APIC_IO && IPI_INTS */

#if defined(XFAST_IPI32)
	.text
	SUPERALIGN_TEXT
	.globl	_Xfastipi32
_Xfastipi32:
	pushl	%eax
	movl	%cr3, %eax
	movl	%eax, %cr3
	pushl	%ds
	movl	$KDSEL,%eax
	movl	%ax,%ds
	incl	_ipihits
	movl	_apic_base, %eax
	movl	$0, APIC_EOI(%eax)
	popl	%ds
	popl	%eax
	iret
#endif /* XFAST_IPI32 */

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
#if defined(APIC_IO)
	FAST_INTR(16,fastintr16, ENABLE_ICU1_AND_2)
	FAST_INTR(17,fastintr17, ENABLE_ICU1_AND_2)
	FAST_INTR(18,fastintr18, ENABLE_ICU1_AND_2)
	FAST_INTR(19,fastintr19, ENABLE_ICU1_AND_2)
	FAST_INTR(20,fastintr20, ENABLE_ICU1_AND_2)
	FAST_INTR(21,fastintr21, ENABLE_ICU1_AND_2)
	FAST_INTR(22,fastintr22, ENABLE_ICU1_AND_2)
	FAST_INTR(23,fastintr23, ENABLE_ICU1_AND_2)
#endif /* APIC_IO */
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
#if defined(APIC_IO)
	INTR(16,intr16, IO_ICU2, ENABLE_ICU1_AND_2, ah)
	INTR(17,intr17, IO_ICU2, ENABLE_ICU1_AND_2, ah)
	INTR(18,intr18, IO_ICU2, ENABLE_ICU1_AND_2, ah)
	INTR(19,intr19, IO_ICU2, ENABLE_ICU1_AND_2, ah)
	INTR(20,intr20, IO_ICU2, ENABLE_ICU1_AND_2, ah)
	INTR(21,intr21, IO_ICU2, ENABLE_ICU1_AND_2, ah)
	INTR(22,intr22, IO_ICU2, ENABLE_ICU1_AND_2, ah)
	INTR(23,intr23, IO_ICU2, ENABLE_ICU1_AND_2, ah)
#if defined(IPI_INTS)
	IPI_INTR(24, ipi24)
	IPI_INTR(25, ipi25)
	IPI_INTR(26, ipi26)
	IPI_INTR(27, ipi27)
#endif /* IPI_INTS */
#endif /* APIC_IO */
MCOUNT_LABEL(eintr)

	.data
ihandlers:			/* addresses of interrupt handlers */
				/* actually resumption addresses for HWI's */
	.long	Xresume0, Xresume1, Xresume2, Xresume3 
	.long	Xresume4, Xresume5, Xresume6, Xresume7
	.long	Xresume8, Xresume9, Xresume10, Xresume11
	.long	Xresume12, Xresume13, Xresume14, Xresume15 
#if defined(APIC_IO)
	.long	Xresume16, Xresume17, Xresume18, Xresume19
	.long	Xresume20, Xresume21, Xresume22, Xresume23
	.long	0, 0, 0, 0, swi_tty, swi_net, _softclock, swi_ast
#else
	.long	swi_tty, swi_net, 0, 0, 0, 0, 0, 0
	.long	0, 0, 0, 0, 0, 0, _softclock, swi_ast
#endif /* APIC_IO */

imasks:				/* masks for interrupt handlers */
	.space	NHWI*4		/* padding; HWI masks are elsewhere */

#if defined(APIC_IO)
#if defined(IPI_INTS)
	/* these 4 IPI slots are counted as HARDWARE INTs, ie NHWI, above */
#else
	.long	0, 0, 0, 0	/* padding */
#endif /* IPI_INTS */
	.long	SWI_TTY_MASK, SWI_NET_MASK, SWI_CLOCK_MASK, SWI_AST_MASK
#else
	.long	SWI_TTY_MASK, SWI_NET_MASK, 0, 0, 0, 0, 0, 0
	.long	0, 0, 0, 0, 0, 0, SWI_CLOCK_MASK, SWI_AST_MASK
#endif /* APIC_IO */

	.globl	_intr_nesting_level
_intr_nesting_level:
	.byte	0
	.space	3

#if defined(APIC_IO)

	.globl _ivectors
_ivectors:
	.long	_Xintr0,  _Xintr1,  _Xintr2,  _Xintr3 
	.long	_Xintr4,  _Xintr5,  _Xintr6,  _Xintr7
	.long	_Xintr8,  _Xintr9,  _Xintr10, _Xintr11
	.long	_Xintr12, _Xintr13, _Xintr14, _Xintr15 
	.long	_Xintr16, _Xintr17, _Xintr18, _Xintr19
	.long	_Xintr20, _Xintr21, _Xintr22, _Xintr23
#if defined(IPI_INTS)
	.long	_Xipi24, _Xipi25, _Xipi26, _Xipi27
#endif /* IPI_INTS */

/* active flag for lazy masking */
iactive:
	.long	0

#if defined(XFAST_IPI32)
	.globl _ipihits
_ipihits:
	.long	0
#endif /* XFAST_IPI32 */

#if defined(TEST_CPUHITS)
	.globl _cpuhits
_cpuhits:
#if !defined(NCPU)
/**
 * FIXME: need a way to pass NCPU to .s files.
 *        NCPU currently defined in smp.h IF NOT defined in opt_smp.h.
 */
#define NCPU	4
#endif /* NCPU */
	.space	NCPU*4
#endif /* TEST_CPUHITS */

#endif /* APIC_IO */

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
#if defined(APIC_IO)
	.asciz	"stray irq16"
	.asciz	"stray irq17"
	.asciz	"stray irq18"
	.asciz	"stray irq19"
	.asciz	"stray irq20"
	.asciz	"stray irq21"
	.asciz	"stray irq22"
	.asciz	"stray irq23"
#if defined(IPI_INTS)
	.asciz	"stray irq24"
	.asciz	"stray irq25"
	.asciz	"stray irq26"
	.asciz	"stray irq27"
#endif /* IPI_INTS */
#endif /* APIC_IO */
_eintrnames:

	.text
