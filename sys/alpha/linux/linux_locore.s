/* $FreeBSD$ */
	
#include <machine/asm.h>
#include <alpha/linux/linux_syscall.h>


/*
 * Signal "trampoline" code. Invoked from RTE setup by sendsig().
 *
 * On entry, stack & registers look like:
 *
 *      a0	signal number
 *      a1	pointer to siginfo_t
 *      a2	pointer to signal context frame (scp)
 *      a3	address of handler
 *      sp+0	saved hardware state
 *                      .
 *                      .
 *      scp+0	beginning of signal context frame
 */

/*
 * System call glue.
 */
#define LINUX_SYSCALLNUM(name)                                   \
        ___CONCAT(LINUX_SYS_,name)

#define LINUX_CALLSYS_NOERROR(name)                              \
        ldiq    v0, LINUX_SYSCALLNUM(name);                      \
        call_pal PAL_OSF1_callsys


	
NESTED(linux_sigcode,0,0,ra,0,0)
	lda	sp, -16(sp)		/* save the sigcontext pointer */
	stq	a2, 0(sp)
	jsr	ra, (t12)		/* call the signal handler (t12==pv) */
	ldq	a0, 0(sp)		/* get the sigcontext pointer */
	lda	sp, 16(sp)
					/* and call sigreturn() with it. */
	LINUX_CALLSYS_NOERROR(osf1_sigreturn)
	mov	v0, a0			/* if that failed, get error code */
	LINUX_CALLSYS_NOERROR(exit)	/* and call exit() with it. */
XNESTED(linux_esigcode,0)
	END(linux_sigcode)

	.data
	EXPORT(linux_szsigcode)
	.quad	linux_esigcode-linux_sigcode
	.text
