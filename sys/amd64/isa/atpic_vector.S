/*
 *	from: vector.s, 386BSD 0.1 unknown origin
 * $FreeBSD$
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
	pushl	$0 ;		/* dummy error code */ \
	pushl	$0 ;		/* dummy trap type */ \
	pushal ; \
	pushl	%ds ; \
	pushl	%es ; \
	pushl	%fs ; \
	mov	$KDSEL,%ax ; \
	mov	%ax,%ds ; \
	mov	%ax,%es ; \
	mov	%ax,%fs ; \
	FAKE_MCOUNT((12+ACTUALLY_PUSHED)*4(%esp)) ; \
	incb	_intr_nesting_level ; \
	pushl	_intr_unit + (irq_num) * 4 ; \
	call	*_intr_handler + (irq_num) * 4 ; /* do the work ASAP */ \
	enable_icus ;		/* (re)enable ASAP (helps edge trigger?) */ \
	addl	$4,%esp ; \
	incl	_cnt+V_INTR ;	/* book-keeping can wait */ \
	movl	_intr_countp + (irq_num) * 4,%eax ; \
	incl	(%eax) ; \
	MEXITCOUNT ; \
	jmp	doreti_next

#if 0
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
	popl	%fs ; \
	popl	%ecx ;		/* ... original %ds ... */ \
	popl	%edx ; \
	xchgl	%eax,4(%esp) ;	/* orig %eax; save cpl */ \
	pushal ;		/* build fat frame (grrr) ... */ \
	pushl	%ecx ;		/* ... actually %ds ... */ \
	pushl	%es ; \
	pushl	%fs ; \
	mov	$KDSEL,%ax ; \
	mov	%ax,%es ; \
	mov	%ax,%fs ; \
	movl	(3+8+0)*4(%esp),%ecx ;	/* ... %ecx from thin frame ... */ \
	movl	%ecx,(3+6)*4(%esp) ;	/* ... to fat frame ... */ \
	movl	(3+8+1)*4(%esp),%eax ;	/* ... cpl from thin frame */ \
	subl	$4,%esp ;	/* junk for unit number */ \
	MEXITCOUNT ; \
	jmp	_doreti
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
#define	INTR(irq_num, vec_name, icu, enable_icus, reg, maybe_extra_ipending) \
	.text ; \
	SUPERALIGN_TEXT ; \
IDTVEC(vec_name) ; \
	pushl	$0 ;		/* dummy error code */ \
	pushl	$0 ;		/* dummy trap type */ \
	pushal ; \
	pushl	%ds ;		/* save our data and extra segments ... */ \
	pushl	%es ; \
	pushl	%fs ; \
	mov	$KDSEL,%ax ;	/* load kernel ds, es and fs */ \
	mov	%ax,%ds ; \
	mov	%ax,%es ; \
	mov	%ax,%fs ; \
	maybe_extra_ipending ; \
	movb	_imen + IRQ_BYTE(irq_num),%al ; \
	orb	$IRQ_BIT(irq_num),%al ; \
	movb	%al,_imen + IRQ_BYTE(irq_num) ; \
	outb	%al,$icu+ICU_IMR_OFFSET ; \
	enable_icus ; \
	incb	_intr_nesting_level ; \
__CONCAT(Xresume,irq_num): ; \
	FAKE_MCOUNT(13*4(%esp)) ;	/* XXX late to avoid double count */ \
	pushl	$irq_num; 	/* pass the IRQ */ \
	sti ; \
	call	_sched_ithd ; \
	addl	$4, %esp ;	/* discard the parameter */ \
	MEXITCOUNT ; \
	/* We could usually avoid the following jmp by inlining some of */ \
	/* _doreti, but it's probably better to use less cache. */ \
	jmp	doreti_next	/* and catch up inside doreti */

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

#define	CLKINTR_PENDING	movl $1,CNAME(clkintr_pending)
/* Threaded interrupts */
	INTR(0,intr0, IO_ICU1, ENABLE_ICU1, al, CLKINTR_PENDING)
	INTR(1,intr1, IO_ICU1, ENABLE_ICU1, al,)
	INTR(2,intr2, IO_ICU1, ENABLE_ICU1, al,)
	INTR(3,intr3, IO_ICU1, ENABLE_ICU1, al,)
	INTR(4,intr4, IO_ICU1, ENABLE_ICU1, al,)
	INTR(5,intr5, IO_ICU1, ENABLE_ICU1, al,)
	INTR(6,intr6, IO_ICU1, ENABLE_ICU1, al,)
	INTR(7,intr7, IO_ICU1, ENABLE_ICU1, al,)
	INTR(8,intr8, IO_ICU2, ENABLE_ICU1_AND_2, ah,)
	INTR(9,intr9, IO_ICU2, ENABLE_ICU1_AND_2, ah,)
	INTR(10,intr10, IO_ICU2, ENABLE_ICU1_AND_2, ah,)
	INTR(11,intr11, IO_ICU2, ENABLE_ICU1_AND_2, ah,)
	INTR(12,intr12, IO_ICU2, ENABLE_ICU1_AND_2, ah,)
	INTR(13,intr13, IO_ICU2, ENABLE_ICU1_AND_2, ah,)
	INTR(14,intr14, IO_ICU2, ENABLE_ICU1_AND_2, ah,)
	INTR(15,intr15, IO_ICU2, ENABLE_ICU1_AND_2, ah,)

MCOUNT_LABEL(eintr)
