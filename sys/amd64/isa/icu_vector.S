/*
 *	from: vector.s, 386BSD 0.1 unknown origin
 * $FreeBSD$
 */

#define	IRQ_BIT(irq_num)	(1 << ((irq_num) % 8))
#define	IRQ_BYTE(irq_num)	((irq_num) >> 3)

#define	ENABLE_ICU1							\
	movb	$ICU_EOI,%al ;	/* as soon as possible send EOI ... */	\
	outb	%al,$IO_ICU1	/* ... to clear in service bit */

#define	ENABLE_ICU1_AND_2						\
	movb	$ICU_EOI,%al ;	/* as above */				\
	outb	%al,$IO_ICU2 ;	/* but do second icu first ... */	\
	outb	%al,$IO_ICU1	/* ... then first icu */


/*
 * Macros for interrupt interrupt entry, call to handler, and exit.
 */

#define	FAST_INTR(irq_num, vec_name, enable_icus)			\
	.text ;								\
	SUPERALIGN_TEXT ;						\
IDTVEC(vec_name) ;							\
	subq	$TF_RIP,%rsp ;	/* skip dummy tf_err and tf_trapno */	\
	testb	$SEL_RPL_MASK,TF_CS(%rsp) ; /* come from kernel? */	\
	jz	1f ;		/* Yes, dont swapgs again */		\
	swapgs ;							\
1:	movq	%rdi,TF_RDI(%rsp) ;					\
	movq	%rsi,TF_RSI(%rsp) ;					\
	movq	%rdx,TF_RDX(%rsp) ;					\
	movq	%rcx,TF_RCX(%rsp) ;					\
	movq	%r8,TF_R8(%rsp) ;					\
	movq	%r9,TF_R9(%rsp) ;					\
	movq	%rax,TF_RAX(%rsp) ;					\
	movq	%rbx,TF_RBX(%rsp) ;					\
	movq	%rbp,TF_RBP(%rsp) ;					\
	movq	%r10,TF_R10(%rsp) ;					\
	movq	%r11,TF_R11(%rsp) ;					\
	movq	%r12,TF_R12(%rsp) ;					\
	movq	%r13,TF_R13(%rsp) ;					\
	movq	%r14,TF_R14(%rsp) ;					\
	movq	%r15,TF_R15(%rsp) ;					\
	FAKE_MCOUNT((12)*4(%rsp)) ;					\
	call	critical_enter ;					\
	movq	PCPU(CURTHREAD),%rbx ;					\
	incl	TD_INTR_NESTING_LEVEL(%rbx) ;				\
	movq	intr_unit + (irq_num) * 8, %rdi ;			\
	call	*intr_handler + (irq_num) * 8 ;	/* do the work ASAP */	\
	enable_icus ;		/* (re)enable ASAP (helps edge trigger?) */ \
	incl	cnt+V_INTR ;	/* book-keeping can wait */		\
	movq	intr_countp + (irq_num) * 8,%rax ;			\
	incq	(%rax) ;						\
	decl	TD_INTR_NESTING_LEVEL(%rbx) ;				\
	call	critical_exit ;						\
	MEXITCOUNT ;							\
	jmp	doreti

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
	subq	$TF_RIP,%rsp ;	/* skip dummy tf_err and tf_trapno */	\
	testb	$SEL_RPL_MASK,TF_CS(%rsp) ; /* come from kernel? */	\
	jz	1f ;		/* Yes, dont swapgs again */		\
	swapgs ;							\
1:	movq	%rdi,TF_RDI(%rsp) ;					\
	movq	%rsi,TF_RSI(%rsp) ;					\
	movq	%rdx,TF_RDX(%rsp) ;					\
	movq	%rcx,TF_RCX(%rsp) ;					\
	movq	%r8,TF_R8(%rsp) ;					\
	movq	%r9,TF_R9(%rsp) ;					\
	movq	%rax,TF_RAX(%rsp) ;					\
	movq	%rbx,TF_RBX(%rsp) ;					\
	movq	%rbp,TF_RBP(%rsp) ;					\
	movq	%r10,TF_R10(%rsp) ;					\
	movq	%r11,TF_R11(%rsp) ;					\
	movq	%r12,TF_R12(%rsp) ;					\
	movq	%r13,TF_R13(%rsp) ;					\
	movq	%r14,TF_R14(%rsp) ;					\
	movq	%r15,TF_R15(%rsp) ;					\
	maybe_extra_ipending ;						\
	movb	imen + IRQ_BYTE(irq_num),%al ;				\
	orb	$IRQ_BIT(irq_num),%al ;					\
	movb	%al,imen + IRQ_BYTE(irq_num) ;				\
	outb	%al,$icu+ICU_IMR_OFFSET ;				\
	enable_icus ;							\
	movq	PCPU(CURTHREAD),%rbx ;					\
	incl	TD_INTR_NESTING_LEVEL(%rbx) ;				\
	FAKE_MCOUNT(13*4(%rsp)) ;	/* XXX late to avoid double count */ \
	movq	$irq_num, %rdi;	/* pass the IRQ */			\
	call	sched_ithd ;						\
	decl	TD_INTR_NESTING_LEVEL(%rbx) ;				\
	MEXITCOUNT ;							\
	/* We could usually avoid the following jmp by inlining some of */ \
	/* doreti, but it's probably better to use less cache. */	\
	jmp	doreti

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

MCOUNT_LABEL(eintr)

