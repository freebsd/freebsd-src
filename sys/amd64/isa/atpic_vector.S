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
#define	IRQ_LBIT(irq_num)	(1 << (irq_num))
#define	IRQ_BYTE(irq_num)	((irq_num) >> 3)

#ifdef AUTO_EOI_1

#define	ENABLE_ICU1		/* use auto-EOI to reduce i/o */
#define	OUTB_ICU1

#else

#define	ENABLE_ICU1							\
	movb	$ICU_EOI,%al ;	/* as soon as possible send EOI ... */	\
	OUTB_ICU1		/* ... to clear in service bit */

#define	OUTB_ICU1							\
	outb	%al,$IO_ICU1

#endif

#ifdef AUTO_EOI_2
/*
 * The data sheet says no auto-EOI on slave, but it sometimes works.
 */
#define	ENABLE_ICU1_AND_2	ENABLE_ICU1

#else

#define	ENABLE_ICU1_AND_2						\
	movb	$ICU_EOI,%al ;	/* as above */				\
	outb	%al,$IO_ICU2 ;	/* but do second icu first ... */	\
	OUTB_ICU1		/* ... then first icu (if !AUTO_EOI_1) */

#endif

#define PUSH_FRAME							\
	pushl	$0 ;		/* dummy error code */			\
	pushl	$0 ;		/* dummy trap type */			\
	pushal ;		/* 8 ints */				\
	pushl	%ds ;		/* save data and extra segments ... */	\
	pushl	%es ;							\
	pushl	%fs

#define PUSH_DUMMY							\
	pushfl ;		/* eflags */				\
	pushl	%cs ;		/* cs */				\
	pushl	12(%esp) ;	/* original caller eip */		\
	pushl	$0 ;		/* dummy error code */			\
	pushl	$0 ;		/* dummy trap type */			\
	subl	$11*4,%esp

#define POP_FRAME							\
	popl	%fs ;							\
	popl	%es ;							\
	popl	%ds ;							\
	popal ;								\
	addl	$4+4,%esp

#define POP_DUMMY							\
	addl	$16*4,%esp

#define MASK_IRQ(icu, irq_num)						\
	movb	imen + IRQ_BYTE(irq_num),%al ;				\
	orb	$IRQ_BIT(irq_num),%al ;					\
	movb	%al,imen + IRQ_BYTE(irq_num) ;				\
	outb	%al,$icu+ICU_IMR_OFFSET

#define UNMASK_IRQ(icu, irq_num)					\
	movb	imen + IRQ_BYTE(irq_num),%al ;				\
	andb	$~IRQ_BIT(irq_num),%al ;				\
	movb	%al,imen + IRQ_BYTE(irq_num) ;				\
	outb	%al,$icu+ICU_IMR_OFFSET
/*
 * Macros for interrupt interrupt entry, call to handler, and exit.
 */

#define	FAST_INTR(irq_num, vec_name, icu, enable_icus)			\
	.text ;								\
	SUPERALIGN_TEXT ;						\
IDTVEC(vec_name) ;							\
	PUSH_FRAME ;							\
	mov	$KDSEL,%ax ;						\
	mov	%ax,%ds ;						\
	mov	%ax,%es ;						\
	mov	$KPSEL,%ax ;						\
	mov	%ax,%fs ;						\
	FAKE_MCOUNT((12+ACTUALLY_PUSHED)*4(%esp)) ;			\
	movl	PCPU(CURTHREAD),%ebx ;					\
	cmpl	$0,TD_CRITNEST(%ebx) ;					\
	je	1f ;							\
;									\
	movl	$1,PCPU(INT_PENDING) ;					\
	orl	$IRQ_LBIT(irq_num),PCPU(FPENDING) ;			\
	MASK_IRQ(icu, irq_num) ;					\
	enable_icus ;							\
	jmp	10f ;							\
1: ;									\
	incl	TD_CRITNEST(%ebx) ;					\
	incl	TD_INTR_NESTING_LEVEL(%ebx) ;				\
	pushl	intr_unit + (irq_num) * 4 ;				\
	call	*intr_handler + (irq_num) * 4 ;				\
	addl	$4,%esp ;						\
	enable_icus ;							\
	incl	cnt+V_INTR ;	/* book-keeping can wait */		\
	movl	intr_countp + (irq_num) * 4,%eax ;			\
	incl	(%eax) ;						\
	decl	TD_CRITNEST(%ebx) ;					\
	cmpl	$0,PCPU(INT_PENDING) ;					\
	je	2f ;							\
;									\
	call	i386_unpend ;						\
2: ;									\
	decl	TD_INTR_NESTING_LEVEL(%ebx) ;				\
10: ;									\
	MEXITCOUNT ;							\
	jmp	doreti

/*
 * Restart a fast interrupt that was held up by a critical section.
 * This routine is called from unpend().  unpend() ensures we are
 * in a critical section and deals with the interrupt nesting level
 * for us.  If we previously masked the irq, we have to unmask it.
 *
 * We have a choice.  We can regenerate the irq using the 'int'
 * instruction or we can create a dummy frame and call the interrupt
 * handler directly.  I've chosen to use the dummy-frame method.
 */
#define	FAST_UNPEND(irq_num, vec_name, icu)				\
	.text ;								\
	SUPERALIGN_TEXT ;						\
IDTVEC(vec_name) ;							\
;									\
	pushl %ebp ;							\
	movl %esp, %ebp ;						\
	PUSH_DUMMY ;							\
	pushl	intr_unit + (irq_num) * 4 ;				\
	call	*intr_handler + (irq_num) * 4 ;	/* do the work ASAP */	\
	addl	$4, %esp ;						\
	incl	cnt+V_INTR ;	/* book-keeping can wait */		\
	movl	intr_countp + (irq_num) * 4,%eax ;			\
	incl	(%eax) ;						\
	UNMASK_IRQ(icu, irq_num) ;					\
	POP_DUMMY ;							\
	popl %ebp ;							\
	ret

/* 
 * Slow, threaded interrupts.
 *
 * XXX Most of the parameters here are obsolete.  Fix this when we're
 * done.
 * XXX we really shouldn't return via doreti if we just schedule the
 * interrupt handler and don't run anything.  We could just do an
 * iret.  FIXME.
 */
#define	INTR(irq_num, vec_name, icu, enable_icus, maybe_extra_ipending) \
	.text ;								\
	SUPERALIGN_TEXT ;						\
IDTVEC(vec_name) ;							\
	PUSH_FRAME ;							\
	mov	$KDSEL,%ax ;	/* load kernel ds, es and fs */		\
	mov	%ax,%ds ;						\
	mov	%ax,%es ;						\
	mov	$KPSEL,%ax ;						\
	mov	%ax,%fs ;						\
;									\
	maybe_extra_ipending ;						\
	MASK_IRQ(icu, irq_num) ;					\
	enable_icus ;							\
;									\
	movl	PCPU(CURTHREAD),%ebx ;					\
        cmpl	$0,TD_CRITNEST(%ebx) ;					\
	je      1f ;							\
	movl	$1,PCPU(INT_PENDING);					\
	orl     $IRQ_LBIT(irq_num),PCPU(IPENDING) ;			\
	jmp     10f ;							\
1: ;									\
	incl	TD_INTR_NESTING_LEVEL(%ebx) ;				\
;									\
	FAKE_MCOUNT(13*4(%esp)) ;	/* XXX late to avoid double count */ \
	cmpl	$0,PCPU(INT_PENDING) ;					\
	je      9f ;							\
	call    i386_unpend ;						\
9: ;									\
	pushl	$irq_num; 	/* pass the IRQ */			\
	call	sched_ithd ;						\
	addl	$4, %esp ;	/* discard the parameter */		\
;									\
	decl	TD_INTR_NESTING_LEVEL(%ebx) ;				\
10: ;									\
	MEXITCOUNT ;							\
	jmp	doreti

MCOUNT_LABEL(bintr)
	FAST_INTR(0,fastintr0, IO_ICU1, ENABLE_ICU1)
	FAST_INTR(1,fastintr1, IO_ICU1, ENABLE_ICU1)
	FAST_INTR(2,fastintr2, IO_ICU1, ENABLE_ICU1)
	FAST_INTR(3,fastintr3, IO_ICU1, ENABLE_ICU1)
	FAST_INTR(4,fastintr4, IO_ICU1, ENABLE_ICU1)
	FAST_INTR(5,fastintr5, IO_ICU1, ENABLE_ICU1)
	FAST_INTR(6,fastintr6, IO_ICU1, ENABLE_ICU1)
	FAST_INTR(7,fastintr7, IO_ICU1, ENABLE_ICU1)
	FAST_INTR(8,fastintr8, IO_ICU2, ENABLE_ICU1_AND_2)
	FAST_INTR(9,fastintr9, IO_ICU2, ENABLE_ICU1_AND_2)
	FAST_INTR(10,fastintr10, IO_ICU2, ENABLE_ICU1_AND_2)
	FAST_INTR(11,fastintr11, IO_ICU2, ENABLE_ICU1_AND_2)
	FAST_INTR(12,fastintr12, IO_ICU2, ENABLE_ICU1_AND_2)
	FAST_INTR(13,fastintr13, IO_ICU2, ENABLE_ICU1_AND_2)
	FAST_INTR(14,fastintr14, IO_ICU2, ENABLE_ICU1_AND_2)
	FAST_INTR(15,fastintr15, IO_ICU2, ENABLE_ICU1_AND_2)

#define	CLKINTR_PENDING	movl $1,CNAME(clkintr_pending)
/* Threaded interrupts */
	INTR(0,intr0, IO_ICU1, ENABLE_ICU1, CLKINTR_PENDING)
	INTR(1,intr1, IO_ICU1, ENABLE_ICU1,)
	INTR(2,intr2, IO_ICU1, ENABLE_ICU1,)
	INTR(3,intr3, IO_ICU1, ENABLE_ICU1,)
	INTR(4,intr4, IO_ICU1, ENABLE_ICU1,)
	INTR(5,intr5, IO_ICU1, ENABLE_ICU1,)
	INTR(6,intr6, IO_ICU1, ENABLE_ICU1,)
	INTR(7,intr7, IO_ICU1, ENABLE_ICU1,)
	INTR(8,intr8, IO_ICU2, ENABLE_ICU1_AND_2,)
	INTR(9,intr9, IO_ICU2, ENABLE_ICU1_AND_2,)
	INTR(10,intr10, IO_ICU2, ENABLE_ICU1_AND_2,)
	INTR(11,intr11, IO_ICU2, ENABLE_ICU1_AND_2,)
	INTR(12,intr12, IO_ICU2, ENABLE_ICU1_AND_2,)
	INTR(13,intr13, IO_ICU2, ENABLE_ICU1_AND_2,)
	INTR(14,intr14, IO_ICU2, ENABLE_ICU1_AND_2,)
	INTR(15,intr15, IO_ICU2, ENABLE_ICU1_AND_2,)

	FAST_UNPEND(0,fastunpend0, IO_ICU1)
	FAST_UNPEND(1,fastunpend1, IO_ICU1)
	FAST_UNPEND(2,fastunpend2, IO_ICU1)
	FAST_UNPEND(3,fastunpend3, IO_ICU1)
	FAST_UNPEND(4,fastunpend4, IO_ICU1)
	FAST_UNPEND(5,fastunpend5, IO_ICU1)
	FAST_UNPEND(6,fastunpend6, IO_ICU1)
	FAST_UNPEND(7,fastunpend7, IO_ICU1)
	FAST_UNPEND(8,fastunpend8, IO_ICU2)
	FAST_UNPEND(9,fastunpend9, IO_ICU2)
	FAST_UNPEND(10,fastunpend10, IO_ICU2)
	FAST_UNPEND(11,fastunpend11, IO_ICU2)
	FAST_UNPEND(12,fastunpend12, IO_ICU2)
	FAST_UNPEND(13,fastunpend13, IO_ICU2)
	FAST_UNPEND(14,fastunpend14, IO_ICU2)
	FAST_UNPEND(15,fastunpend15, IO_ICU2)
MCOUNT_LABEL(eintr)

