/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 *
 * $FreeBSD$
 */
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#define _ASM

#include <machine/asmacros.h>
#include <sys/cpuvar_defs.h>
#include <sys/dtrace.h>

#include "assym.s"

	.globl	calltrap
	.type	calltrap,@function
	ENTRY(dtrace_invop_start)

	pushl	%eax			/* push %eax -- may be return value */
	pushl	%esp			/* push stack pointer */
	addl	$48, (%esp)		/* adjust to incoming args */
	pushl	40(%esp)		/* push calling EIP */

	/*
	 * Call dtrace_invop to let it check if the exception was
	 * a fbt one. The return value in %eax will tell us what
	 * dtrace_invop wants us to do.
	 */
	call	dtrace_invop

	/*
	 * We pushed 3 times for the arguments to dtrace_invop,
	 * so we need to increment the stack pointer to get rid of
	 * those values.
	 */
	addl	$12, %esp
	ALTENTRY(dtrace_invop_callsite)
	cmpl	$DTRACE_INVOP_PUSHL_EBP, %eax
	je	invop_push
	cmpl	$DTRACE_INVOP_POPL_EBP, %eax
	je	invop_pop
	cmpl	$DTRACE_INVOP_LEAVE, %eax
	je	invop_leave
	cmpl	$DTRACE_INVOP_NOP, %eax
	je	invop_nop

	/* When all else fails handle the trap in the usual way. */
	jmpl	*dtrace_invop_calltrap_addr

invop_push:
	/*
	 * We must emulate a "pushl %ebp".  To do this, we pull the stack
	 * down 4 bytes, and then store the base pointer.
	 */
	popal
	subl	$4, %esp		/* make room for %ebp */
	pushl	%eax			/* push temp */
	movl	8(%esp), %eax		/* load calling EIP */
	incl	%eax			/* increment over LOCK prefix */
	movl	%eax, 4(%esp)		/* store calling EIP */
	movl	12(%esp), %eax		/* load calling CS */
	movl	%eax, 8(%esp)		/* store calling CS */
	movl	16(%esp), %eax		/* load calling EFLAGS */
	movl	%eax, 12(%esp)		/* store calling EFLAGS */
	movl	%ebp, 16(%esp)		/* push %ebp */
	popl	%eax			/* pop off temp */
	iret				/* Return from interrupt. */
invop_pop:
	/*
	 * We must emulate a "popl %ebp".  To do this, we do the opposite of
	 * the above:  we remove the %ebp from the stack, and squeeze up the
	 * saved state from the trap.
	 */
	popal
	pushl	%eax			/* push temp */
	movl	16(%esp), %ebp		/* pop %ebp */
	movl	12(%esp), %eax		/* load calling EFLAGS */
	movl	%eax, 16(%esp)		/* store calling EFLAGS */
	movl	8(%esp), %eax		/* load calling CS */
	movl	%eax, 12(%esp)		/* store calling CS */
	movl	4(%esp), %eax		/* load calling EIP */
	incl	%eax			/* increment over LOCK prefix */
	movl	%eax, 8(%esp)		/* store calling EIP */
	popl	%eax			/* pop off temp */
	addl	$4, %esp		/* adjust stack pointer */
	iret				/* Return from interrupt. */
invop_leave:
	/*
	 * We must emulate a "leave", which is the same as a "movl %ebp, %esp"
	 * followed by a "popl %ebp".  This looks similar to the above, but
	 * requires two temporaries:  one for the new base pointer, and one
	 * for the staging register.
	 */
	popa
	pushl	%eax			/* push temp */
	pushl	%ebx			/* push temp */
	movl	%ebp, %ebx		/* set temp to old %ebp */
	movl	(%ebx), %ebp		/* pop %ebp */
	movl	16(%esp), %eax		/* load calling EFLAGS */
	movl	%eax, (%ebx)		/* store calling EFLAGS */
	movl	12(%esp), %eax		/* load calling CS */
	movl	%eax, -4(%ebx)		/* store calling CS */
	movl	8(%esp), %eax		/* load calling EIP */
	incl	%eax			/* increment over LOCK prefix */
	movl	%eax, -8(%ebx)		/* store calling EIP */
	movl	%ebx, -4(%esp)		/* temporarily store new %esp */
	popl	%ebx			/* pop off temp */
	popl	%eax			/* pop off temp */
	movl	-12(%esp), %esp		/* set stack pointer */
	subl	$8, %esp		/* adjust for three pushes, one pop */
	iret				/* return from interrupt */
invop_nop:
	/*
	 * We must emulate a "nop".  This is obviously not hard:  we need only
	 * advance the %eip by one.
	 */
	popa
	incl	(%esp)
	iret				/* return from interrupt */

	END(dtrace_invop_start)

/*
void dtrace_invop_init(void)
*/
	ENTRY(dtrace_invop_init)
	movl	$dtrace_invop_start, dtrace_invop_jump_addr
	ret
	END(dtrace_invop_init)

/*
void dtrace_invop_uninit(void)
*/
	ENTRY(dtrace_invop_uninit)
	movl	$0, dtrace_invop_jump_addr
	ret
	END(dtrace_invop_uninit)

/*
greg_t dtrace_getfp(void)
*/

	ENTRY(dtrace_getfp)
	movl	%ebp, %eax
	ret
	END(dtrace_getfp)

/*
uint32_t dtrace_cas32(uint32_t *target, uint32_t cmp, uint32_t new)
*/

	ENTRY(dtrace_cas32)
	ALTENTRY(dtrace_casptr)
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	movl	12(%esp), %ecx
	lock
	cmpxchgl %ecx, (%edx)
	ret
	END(dtrace_casptr)
	END(dtrace_cas32)

/*
uintptr_t dtrace_caller(int aframes)
*/

	ENTRY(dtrace_caller)
	movl	$-1, %eax
	ret
	END(dtrace_caller)

/*
void dtrace_copy(uintptr_t src, uintptr_t dest, size_t size)
*/

	ENTRY(dtrace_copy)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%esi
	pushl	%edi

	movl	8(%ebp), %esi		/* Load source address */
	movl	12(%ebp), %edi		/* Load destination address */
	movl	16(%ebp), %ecx		/* Load count */
	repz				/* Repeat for count... */
	smovb				/*   move from %ds:si to %es:di */

	popl	%edi
	popl	%esi
	movl	%ebp, %esp
	popl	%ebp
	ret
	END(dtrace_copy)

/*
void dtrace_copystr(uintptr_t uaddr, uintptr_t kaddr, size_t size)
*/

	ENTRY(dtrace_copystr)

	pushl	%ebp			/* Setup stack frame */
	movl	%esp, %ebp
	pushl	%ebx			/* Save registers */

	movl	8(%ebp), %ebx		/* Load source address */
	movl	12(%ebp), %edx		/* Load destination address */
	movl	16(%ebp), %ecx		/* Load count */

0:
	movb	(%ebx), %al		/* Load from source */
	movb	%al, (%edx)		/* Store to destination */
	incl	%ebx			/* Increment source pointer */
	incl	%edx			/* Increment destination pointer */
	decl	%ecx			/* Decrement remaining count */
	cmpb	$0, %al
	je	1f
	cmpl	$0, %ecx
	jne	0b

1:
	popl	%ebx
	movl	%ebp, %esp
	popl	%ebp
	ret

	END(dtrace_copystr)

/*
uintptr_t dtrace_fulword(void *addr)
*/

	ENTRY(dtrace_fulword)
	movl	4(%esp), %ecx
	xorl	%eax, %eax
	movl	(%ecx), %eax
	ret
	END(dtrace_fulword)

/*
uint8_t dtrace_fuword8_nocheck(void *addr)
*/

	ENTRY(dtrace_fuword8_nocheck)
	movl	4(%esp), %ecx
	xorl	%eax, %eax
	movzbl	(%ecx), %eax
	ret
	END(dtrace_fuword8_nocheck)

/*
uint16_t dtrace_fuword16_nocheck(void *addr)
*/

	ENTRY(dtrace_fuword16_nocheck)
	movl	4(%esp), %ecx
	xorl	%eax, %eax
	movzwl	(%ecx), %eax
	ret
	END(dtrace_fuword16_nocheck)

/*
uint32_t dtrace_fuword32_nocheck(void *addr)
*/

	ENTRY(dtrace_fuword32_nocheck)
	movl	4(%esp), %ecx
	xorl	%eax, %eax
	movl	(%ecx), %eax
	ret
	END(dtrace_fuword32_nocheck)

/*
uint64_t dtrace_fuword64_nocheck(void *addr)
*/

	ENTRY(dtrace_fuword64_nocheck)
	movl	4(%esp), %ecx
	xorl	%eax, %eax
	xorl	%edx, %edx
	movl	(%ecx), %eax
	movl	4(%ecx), %edx
	ret
	END(dtrace_fuword64_nocheck)

/*
void dtrace_probe_error(dtrace_state_t *state, dtrace_epid_t epid, int which, int fault, int fltoffs, uintptr_t illval)
*/

	ENTRY(dtrace_probe_error)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	0x1c(%ebp)
	pushl	0x18(%ebp)
	pushl	0x14(%ebp)
	pushl	0x10(%ebp)
	pushl	0xc(%ebp)
	pushl	0x8(%ebp)
	pushl	dtrace_probeid_error
	call	dtrace_probe
	movl	%ebp, %esp
	popl	%ebp
	ret
	END(dtrace_probe_error)

/*
void dtrace_membar_producer(void)
*/

	ENTRY(dtrace_membar_producer)
	rep;	ret	/* use 2 byte return instruction when branch target */
			/* AMD Software Optimization Guide - Section 6.2 */
	END(dtrace_membar_producer)

/*
void dtrace_membar_consumer(void)
*/

	ENTRY(dtrace_membar_consumer)
	rep;	ret	/* use 2 byte return instruction when branch target */
			/* AMD Software Optimization Guide - Section 6.2 */
	END(dtrace_membar_consumer)

/*
dtrace_icookie_t dtrace_interrupt_disable(void)
*/
	ENTRY(dtrace_interrupt_disable)
	pushfl
	popl	%eax
	cli
	ret
	END(dtrace_interrupt_disable)

/*
void dtrace_interrupt_enable(dtrace_icookie_t cookie)
*/
	ENTRY(dtrace_interrupt_enable)
	movl	4(%esp), %eax
	pushl	%eax
	popfl
	ret
	END(dtrace_interrupt_enable)

/*
 * The panic() and cmn_err() functions invoke vpanic() as a common entry point
 * into the panic code implemented in panicsys().  vpanic() is responsible
 * for passing through the format string and arguments, and constructing a
 * regs structure on the stack into which it saves the current register
 * values.  If we are not dying due to a fatal trap, these registers will
 * then be preserved in panicbuf as the current processor state.  Before
 * invoking panicsys(), vpanic() activates the first panic trigger (see
 * common/os/panic.c) and switches to the panic_stack if successful.  Note that
 * DTrace takes a slightly different panic path if it must panic from probe
 * context.  Instead of calling panic, it calls into dtrace_vpanic(), which
 * sets up the initial stack as vpanic does, calls dtrace_panic_trigger(), and
 * branches back into vpanic().
 */
/*
void vpanic(const char *format, va_list alist)
*/
	ENTRY(vpanic)				/* Initial stack layout: */

	pushl	%ebp				/* | %eip | 20 */
	movl	%esp, %ebp			/* | %ebp | 16 */
	pushl	%eax				/* | %eax | 12 */
	pushl	%ebx				/* | %ebx |  8 */
	pushl	%ecx				/* | %ecx |  4 */
	pushl	%edx				/* | %edx |  0 */

	movl	%esp, %ebx			/* %ebx = current stack pointer */

	lea	panic_quiesce, %eax		/* %eax = &panic_quiesce */
	pushl	%eax				/* push &panic_quiesce */
	call	panic_trigger			/* %eax = panic_trigger() */
	addl	$4, %esp			/* reset stack pointer */

vpanic_common:
	cmpl	$0, %eax			/* if (%eax == 0) */
	je	0f				/*   goto 0f; */

	/*
	 * If panic_trigger() was successful, we are the first to initiate a
	 * panic: we now switch to the reserved panic_stack before continuing.
	 */
	lea	panic_stack, %esp		/* %esp  = panic_stack */
	addl	$PANICSTKSIZE, %esp		/* %esp += PANICSTKSIZE */

0:	subl	$REGSIZE, %esp			/* allocate struct regs */

	/*
	 * Now that we've got everything set up, store the register values as
	 * they were when we entered vpanic() to the designated location in
	 * the regs structure we allocated on the stack. 
	 */
#ifdef notyet
	mov	%gs, %edx
	mov	%edx, REGOFF_GS(%esp)
	mov	%fs, %edx
	mov	%edx, REGOFF_FS(%esp)
	mov	%es, %edx
	mov	%edx, REGOFF_ES(%esp)
	mov	%ds, %edx
	mov	%edx, REGOFF_DS(%esp)
	movl	%edi, REGOFF_EDI(%esp)
	movl	%esi, REGOFF_ESI(%esp)
	movl	16(%ebx), %ecx
	movl	%ecx, REGOFF_EBP(%esp)
	movl	%ebx, %ecx
	addl	$20, %ecx
	movl	%ecx, REGOFF_ESP(%esp)
	movl	8(%ebx), %ecx
	movl	%ecx, REGOFF_EBX(%esp)
	movl	0(%ebx), %ecx
	movl	%ecx, REGOFF_EDX(%esp)
	movl	4(%ebx), %ecx
	movl	%ecx, REGOFF_ECX(%esp)
	movl	12(%ebx), %ecx
	movl	%ecx, REGOFF_EAX(%esp)
	movl	$0, REGOFF_TRAPNO(%esp)
	movl	$0, REGOFF_ERR(%esp)
	lea	vpanic, %ecx
	movl	%ecx, REGOFF_EIP(%esp)
	mov	%cs, %edx
	movl	%edx, REGOFF_CS(%esp)
	pushfl
	popl	%ecx
	movl	%ecx, REGOFF_EFL(%esp)
	movl	$0, REGOFF_UESP(%esp)
	mov	%ss, %edx
	movl	%edx, REGOFF_SS(%esp)

	movl	%esp, %ecx			/* %ecx = &regs */
	pushl	%eax				/* push on_panic_stack */
	pushl	%ecx				/* push &regs */
	movl	12(%ebp), %ecx			/* %ecx = alist */
	pushl	%ecx				/* push alist */
	movl	8(%ebp), %ecx			/* %ecx = format */
	pushl	%ecx				/* push format */
	call	panicsys			/* panicsys(); */
	addl	$16, %esp			/* pop arguments */

	addl	$REGSIZE, %esp
#endif
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%eax
	leave
	ret
	END(vpanic)

/*
void dtrace_vpanic(const char *format, va_list alist)
*/
	ENTRY(dtrace_vpanic)			/* Initial stack layout: */

	pushl	%ebp				/* | %eip | 20 */
	movl	%esp, %ebp			/* | %ebp | 16 */
	pushl	%eax				/* | %eax | 12 */
	pushl	%ebx				/* | %ebx |  8 */
	pushl	%ecx				/* | %ecx |  4 */
	pushl	%edx				/* | %edx |  0 */

	movl	%esp, %ebx			/* %ebx = current stack pointer */

	lea	panic_quiesce, %eax		/* %eax = &panic_quiesce */
	pushl	%eax				/* push &panic_quiesce */
	call	dtrace_panic_trigger		/* %eax = dtrace_panic_trigger() */
	addl	$4, %esp			/* reset stack pointer */
	jmp	vpanic_common			/* jump back to common code */

	END(dtrace_vpanic)

/*
int
panic_trigger(int *tp)
*/
	ENTRY(panic_trigger)
	xorl	%eax, %eax
	movl	$0xdefacedd, %edx
	lock
	  xchgl	%edx, (%edi)
	cmpl	$0, %edx
	je	0f 
	movl	$0, %eax
	ret
0:	movl	$1, %eax
	ret
	END(panic_trigger)
	
/*
int
dtrace_panic_trigger(int *tp)
*/
	ENTRY(dtrace_panic_trigger)
	xorl	%eax, %eax
	movl	$0xdefacedd, %edx
	lock
	  xchgl	%edx, (%edi)
	cmpl	$0, %edx
	je	0f
	movl	$0, %eax
	ret
0:	movl	$1, %eax
	ret
	END(dtrace_panic_trigger)
