/*
 * NEED A COPYRIGHT NOPTICE HERE
 *
 *	$Id: prof_machdep.c,v 1.2 1996/04/08 16:41:06 wollman Exp $
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <machine/clock.h>
#ifdef PC98
#include <pc98/pc98/pc98.h>
#include <pc98/pc98/timerreg.h>
#else
#include <i386/isa/isa.h>
#include <i386/isa/timerreg.h>
#endif

#ifdef GUPROF
extern u_int	cputime __P((void));
#endif

#ifdef __GNUC__
asm("
GM_STATE	=	0
GMON_PROF_OFF	=	3

	.text
	.align	4,0x90
	.globl	__mcount
__mcount:
	#
	# Check that we are profiling.  Do it early for speed.
	#
	cmpl	$GMON_PROF_OFF,__gmonparam+GM_STATE
 	je	Lmcount_exit
 	#
 	# __mcount is the same as mcount except the caller hasn't changed
 	# the stack except to call here, so the caller's raddr is above
 	# our raddr.
 	#
 	movl	4(%esp),%edx
 	jmp	Lgot_frompc
 
 	.align	4,0x90
 	.globl	mcount
mcount:
	cmpl	$GMON_PROF_OFF,__gmonparam+GM_STATE
	je	Lmcount_exit
	#
	# The caller's stack frame has already been built, so %ebp is
	# the caller's frame pointer.  The caller's raddr is in the
	# caller's frame following the caller's caller's frame pointer.
	#
	movl	4(%ebp),%edx
Lgot_frompc:
	#
	# Our raddr is the caller's pc.
	#
	movl	(%esp),%eax

	pushf
	pushl	%eax
	pushl	%edx
	cli
	call	_mcount
	addl	$8,%esp
	popf
Lmcount_exit:
	ret
");
#else /* !__GNUC__ */
#error
#endif /* __GNUC__ */

#ifdef GUPROF
/*
 * mexitcount saves the return register(s), loads selfpc and calls
 * mexitcount(selfpc) to do the work.  Someday it should be in a machine
 * dependent file together with cputime(), __mcount and mcount.  cputime()
 * can't just be put in machdep.c because it has to be compiled without -pg.
 */
#ifdef __GNUC__
asm("
	.text
#
# Dummy label to be seen when gprof -u hides mexitcount.
#
	.align	4,0x90
	.globl	__mexitcount
__mexitcount:
	nop

GMON_PROF_HIRES	=	4

	.align	4,0x90
	.globl	mexitcount
mexitcount:
	cmpl	$GMON_PROF_HIRES,__gmonparam+GM_STATE
	jne	Lmexitcount_exit
	pushl	%edx
	pushl	%eax
	movl	8(%esp),%eax
	pushf
	pushl	%eax
	cli
	call	_mexitcount
	addl	$4,%esp
	popf
	popl	%eax
	popl	%edx
Lmexitcount_exit:
	ret
");
#else /* !__GNUC__ */
#error
#endif /* __GNUC__ */

/*
 * Return the time elapsed since the last call.  The units are machine-
 * dependent.
 */
u_int
cputime()
{
	u_int count;
	u_int delta;
	u_char low;
	static u_int prev_count;

	/*
	 * Read the current value of the 8254 timer counter 0.
	 */
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_LATCH);
	low = inb(TIMER_CNTR0);
	count = low | (inb(TIMER_CNTR0) << 8);

	/*
	 * The timer counts down from TIMER_CNTR0_MAX to 0 and then resets.
	 * While profiling is enabled, this routine is called at least twice
	 * per timer reset (for mcounting and mexitcounting hardclock()),
	 * so at most one reset has occurred since the last call, and one
	 * has occurred iff the current count is larger than the previous
	 * count.  This allows counter underflow to be detected faster
	 * than in microtime().
	 */
	delta = prev_count - count;
	prev_count = count;
	if ((int) delta <= 0)
		return (delta + timer0_max_count);
	return (delta);
}
#else /* not GUPROF */
#ifdef __GNUC__
asm("
	.text
	.align	4,0x90
	.globl	mexitcount
mexitcount:
	ret
");
#else /* !__GNUC__ */
#error
#endif /* __GNUC__ */
#endif /* GUPROF */
