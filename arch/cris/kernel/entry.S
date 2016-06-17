/* $Id: entry.S,v 1.42 2003/07/07 09:00:50 johana Exp $
 *
 *  linux/arch/cris/entry.S
 *
 *  Copyright (C) 2000, 2001, 2002 Axis Communications AB
 *
 *  Authors:	Bjorn Wesen (bjornw@axis.com)
 *
 *  $Log: entry.S,v $
 *  Revision 1.42  2003/07/07 09:00:50  johana
 *  Added CONFIG_ETRAX_DEBUG_INTERRUPT support.
 *
 *  Revision 1.41  2003/06/17 13:57:50  starvik
 *  Merge of Linux 2.4.21
 *
 *  Revision 1.40  2002/12/02 08:11:41  starvik
 *  Merge of Linux 2.4.20
 *
 *  Revision 1.39  2002/04/24 00:46:45  bjornw
 *  Added sys_tkill
 *
 *  Revision 1.38  2002/01/16 15:15:30  bjornw
 *  Use a C-code compatible watchdog reset when NICE_DOGGY is enabled
 *
 *  Revision 1.37  2001/12/07 17:03:55  bjornw
 *  Call a c-hook called watchdog_bite_hook instead of show_registers directly
 *
 *  Revision 1.36  2001/11/22 13:36:36  bjornw
 *  * In ret_from_intr, check regs->dccr for usermode reentrance instead of
 *    DCCR explicitely (because the latter might not reflect current reality)
 *  * In mmu_bus_fault, set $r9 _after_ calling the C-code instead of before
 *    since $r9 is call-clobbered and is potentially needed afterwards
 *
 *  Revision 1.35  2001/10/30 17:10:15  bjornw
 *  Add some syscalls
 *
 *  Revision 1.34  2001/10/01 14:45:03  bjornw
 *  Removed underscores and added register prefixes
 *
 *  Revision 1.33  2001/08/21 13:48:01  jonashg
 *  Added fix by HP to avoid oops when doing a hard_reset_now.
 *
 *  Revision 1.32  2001/08/14 04:32:02  hp
 *  In _resume, add comment why R9 is saved; don't sound like it's call-saved.
 *
 *  Revision 1.31  2001/07/25 16:07:42  bjornw
 *  softirq_active/mask -> softirq_pending only
 *
 *  Revision 1.30  2001/07/05 01:03:32  hp
 *  - include asm/errno.h to get ENOSYS.
 *  - Use ENOSYS, not local constant LENOSYS; tweak comments.
 *  - Explain why .include, not #include is used.
 *  - Make oops-register-dump if watchdog bits and it's not expected.
 *  - Don't jsr, use jump _hard_reset_now, and skip spurious nop.
 *  - Use correct section attribute for section .rodata.
 *  - Adjust sys_ni_syscall fill number.
 *
 *  Revision 1.29  2001/06/25 14:07:00  hp
 *  	Fix review comment.
 *  	* head.S: Use IO_STATE, IO_FIELD and IO_MASK constructs instead of
 *  	magic numbers.  Add comment that -traditional must not be used.
 *  	* entry.S (SYMBOL_NAME): Change redefinition to use ## concatenation.
 *  	Correct and update comment.
 *  	* Makefile (.S.o): Don't use -traditional.  Add comment why the
 *  	toplevel rule can't be used (now that there's a reason).
 *
 *  Revision 1.28  2001/06/21 02:00:40  hp
 *  	* entry.S: Include asm/unistd.h.
 *  	(_sys_call_table): Use section .rodata, not .data.
 *  	(_kernel_thread): Move from...
 *  	* process.c: ... here.
 *  	* entryoffsets.c (VAL): Break out from...
 *  	(OF): Use VAL.
 *  	(LCLONE_VM): New asmified value from CLONE_VM.
 *
 *  Revision 1.27  2001/05/29 11:25:27  markusl
 *  In case of "spurious_interrupt", do hard_reset instead of hanging system in a loop...
 *
 *  Revision 1.26  2001/05/15 15:46:03  bjornw
 *  Include config.h now that we use some CONFIG_ options
 *
 *  Revision 1.25  2001/05/15 05:38:47  hp
 *  Tweaked code in _ret_from_sys_call
 *
 *  Revision 1.24  2001/05/15 05:27:49  hp
 *  Save r9 in r1 over function call rather than on stack.
 *
 *  Revision 1.23  2001/05/15 05:10:00  hp
 *  Generate entry.S structure offsets from C
 *
 *  Revision 1.22  2001/04/17 13:58:39  orjanf
 *  * Renamed CONFIG_KGDB to CONFIG_ETRAX_KGDB.
 *
 *  Revision 1.21  2001/04/17 11:33:29  orjanf
 *  Updated according to review:
 *  * Included asm/sv_addr_ag.h to get macro for internal register.
 *  * Corrected comment regarding system call argument passing.
 *  * Removed comment about instruction being in a delay slot.
 *  * Added comment about SYMBOL_NAME macro.
 *
 *  Revision 1.20  2001/04/12 08:51:07  hp
 *  - Add entry for sys_fcntl64.  In fact copy last piece from i386 including ...
 *  - .rept to fill table to safe state with sys_ni_syscall.
 *
 *  Revision 1.19  2001/04/04 09:43:32  orjanf
 *  * Moved do_sigtrap from traps.c to entry.S.
 *  * LTASK_PID need not be global anymore.
 *
 *  Revision 1.18  2001/03/26 09:25:02  markusl
 *  Updated after review, should now handle USB interrupts correctly.
 *
 *  Revision 1.17  2001/03/21 16:12:55  bjornw
 *  * Always make room for the cpu status record in the frame, in order to
 *    use the same framelength and layout for both mmu busfaults and normal
 *    irqs. No need to check for the explicit CRIS_FRAME_FIXUP type anymore.
 *  * Fixed bug with using addq for popping the stack in the epilogue - it
 *    destroyed the flag register. Use instructions that don't affect the
 *    flag register instead.
 *  * Removed write to R_PORT_PA_DATA during spurious_interrupt
 *
 *  Revision 1.16  2001/03/20 19:43:02  bjornw
 *  * Get rid of esp0 setting
 *  * Give a 7th argument to a systemcall - the stackframe
 *
 *  Revision 1.15  2001/03/05 13:14:30  bjornw
 *  Spelling fix
 *
 *  Revision 1.14  2001/02/23 08:36:36  perf
 *  New ABI; syscallnr=r9, arg5=mof, arg6=srp.
 *  Corrected tracesys call check.
 *
 *  Revision 1.13  2001/02/15 08:40:55  perf
 *  H-P by way of perf;
 *  - (_system_call): Don't read system call function address into r1.
 *  - (RBFExit): There is no such thing as a null pop.  Adjust sp by addq.
 *  - (_system_call): Don't use r10 and don't save and restore it.
 *  - (THREAD_ESP0): New constant.
 *  - (_system_call): Inline set_esp0.
 *
 *  Revision 1.12  2001/01/31 17:56:25  orjanf
 *  Added definition of LTASK_PID and made it global.
 *
 *  Revision 1.11  2001/01/10 21:13:29  bjornw
 *  SYMBOL_NAME is defined incorrectly for the compiler options we currently use
 *
 *  Revision 1.10  2000/12/18 23:47:56  bjornw
 *  * Added syscall trace support (ptrace), completely untested of course
 *  * Removed redundant check for NULL entries in syscall_table
 *
 *  Revision 1.9  2000/11/21 16:40:51  bjornw
 *  * New frame type used when an SBFS frame needs to be popped without
 *    actually restarting the instruction
 *  * Enable interrupts in signal_return (they did so in x86, I hope it's a good
 *    idea)
 *
 *  Revision 1.8  2000/11/17 16:53:35  bjornw
 *  Added detection of frame-type in Rexit, so that mmu_bus_fault can
 *  use ret_from_intr in the return-path to check for signals (like SEGV)
 *  and other foul things that might have occurred during the fault.
 *
 *  Revision 1.7  2000/10/06 15:04:28  bjornw
 *  Include mof in register savings
 *
 *  Revision 1.6  2000/09/12 16:02:44  bjornw
 *  Linux-2.4.0-test7 derived updates
 *
 *  Revision 1.5  2000/08/17 15:35:15  bjornw
 *  2.4.0-test6 changed local_irq_count and friends API
 *
 *  Revision 1.4  2000/08/02 13:59:30  bjornw
 *  Removed olduname and uname from the syscall list
 *
 *  Revision 1.3  2000/07/31 13:32:58  bjornw
 *  * Export ret_from_intr
 *  * _resume updated (prev/last tjohejsan)
 *  * timer_interrupt obsolete
 *  * SIGSEGV detection in mmu_bus_fault temporarily disabled
 *
 *
 */

/*
 * entry.S contains the system-call and fault low-level handling routines.
 *
 * NOTE: This code handles signal-recognition, which happens every time
 * after a timer-interrupt and after each system call.
 *
 * Stack layout in 'ret_from_system_call':
 * 	ptrace needs to have all regs on the stack.
 *	if the order here is changed, it needs to be 
 *	updated in fork.c:copy_process, signal.c:do_signal,
 *	ptrace.c and ptrace.h
 *
 */

#include <linux/config.h>
#include <linux/linkage.h>
#include <linux/sys.h>
#include <asm/unistd.h>
#include <asm/sv_addr_ag.h>
#include <asm/errno.h>
	
	;; functions exported from this file
	
	.globl system_call
	.globl ret_from_intr
	.globl ret_from_sys_call
	.globl resume
	.globl multiple_interrupt
	.globl hwbreakpoint
	.globl IRQ1_interrupt
	.globl timer_interrupt
	.globl timer_shortcut
	.globl spurious_interrupt
	.globl hw_bp_trigs
	.globl mmu_bus_fault
	.globl do_sigtrap
	.globl gdb_handle_breakpoint
			
	.globl sys_call_table
	
	;; Get values and offsets into various structs.  The file isn't
	;; suitable for consumption by the preprocessor, so don't use
	;; #include.
	.include "entryoffsets.s"

	;; process bits for ptrace.  FIXME: Should be in a header file.

PT_TRACESYS_BIT	  = 1

	;; below are various parts of system_call which are not in the fast-path
	
	;; handle software irqs
				
_handle_softirq:
	move.d	$r9, $r1
	jsr	do_softirq	; call the C routine for softirq handling
	move.d	$r1, $r9

	;; fall-through
	
ret_from_intr:
	;; check for resched only if we're going back to user-mode
	;; this test matches the user_regs(regs) macro
	;; we cannot simply test $dccr, because that does not necessarily
	;; reflect what mode we'll return into.
	
	move.d	[$sp + LDCCR], $r0; regs->dccr
	btstq	8, $r0		; U-flag
	bpl	_Rexit		; go back directly
	nop
	ba	_ret_with_reschedule  ; go back but check schedule and signals first 
	nop

_reschedule:
	;; keep r9 intact
	move.d	$r9, $r1
	jsr	schedule
	ba	ret_from_sys_call
	move.d	$r1, $r9

	;; return but call do_signal first
_signal_return:
	ei			; we can get here from an interrupt
	move.d	$r9, $r10	; do_signals syscall/irq param
	moveq	0, $r11		; oldset param - 0 in this case
	move.d	$sp, $r12	; another argument to do_signal (the regs param)
	jsr	do_signal	; arch/cris/kernel/signal.c
	ba	_Rexit
	nop
	
	;; The system_call is called by a BREAK instruction, which works like
	;; an interrupt call but it stores the return PC in BRP instead of IRP.
	;; Since we dont really want to have two epilogues (one for system calls
	;; and one for interrupts) we push the contents of BRP instead of IRP in the
	;; system call prologue, to make it look like an ordinary interrupt on the
	;; stackframe.
	;;
	;; Since we can't have system calls inside interrupts, it should not matter
	;; that we don't stack IRP.
	;; 
	;; In r9 we have the wanted syscall number. Arguments come in r10,r11,r12,r13,mof,srp
	;;
	;; This function looks on the _surface_ like spaghetti programming, but it's
	;; really designed so that the fast-path does not force cache-loading of non-used
	;; instructions. Only the non-common cases cause the outlined code to run..

system_call:
	;; stack-frame similar to the irq heads, which is reversed in ret_from_sys_call
	move	$brp,[$sp=$sp-16]; instruction pointer and room for a fake SBFS frame
	push	$srp
	push	$dccr
	push	$mof
	subq	14*4, $sp		; make room for r0-r13
	movem	$r13, [$sp]	; push r0-r13
	push	$r10		; push orig_r10
	clear.d [$sp=$sp-4]	; frametype == 0, normal stackframe
	
	movs.w	-ENOSYS, $r0
	move.d	$r0, [$sp+LR10]	; put the default return value in r10 in the frame

	;; check if this process is syscall-traced

	movs.w	-8192, $r0	; THREAD_SIZE == 8192
	and.d	$sp, $r0
	
	move.d	[$r0+LTASK_PTRACE], $r0
	btstq	PT_TRACESYS_BIT, $r0
	bmi	_tracesys
	nop	

	;; check for sanity in the requested syscall number
	
	cmpu.w	NR_syscalls, $r9	
	bcc	ret_from_sys_call
	lslq	2, $r9		;  multiply by 4, in the delay slot

	;; as a bonus 7th parameter, we give the location on the stack
	;; of the register structure itself. some syscalls need this.

	push	$sp
	
	;; the parameter carrying registers r10, r11, r12 and 13 are intact.
	;; the fifth and sixth parameters (if any) was in mof and srp 
	;; respectively, and we need to put them on the stack.

	push	$srp
	push	$mof
	
	jsr	[$r9+sys_call_table]	; actually do the system call
	addq	3*4, $sp		; pop the mof, srp and regs parameters
	move.d	$r10, [$sp+LR10]	; save the return value

	moveq	1, $r9		; "parameter" to ret_from_sys_call to show it was a sys call
	
	;; fall through into ret_from_sys_call to return
	
ret_from_sys_call:
	;; r9 is a parameter - if 1, we came from a syscall, if 0, from an irq
		
	;; check if any bottom halves need service
	
	test.d	[irq_stat]	       ; softirq_pending
	bne	_handle_softirq
	nop

_ret_with_reschedule:
	;; first get the current task-struct pointer (see top for defs)

	move.d	$sp, $r0
	and.d	-8192, $r0	; THREAD_SIZE == 8192
	
	;; see if we want to reschedule into another process
	
	test.d	[$r0+LTASK_NEEDRESCHED]
	bne	_reschedule
	nop

	;; see if we need to run signal checks (important that r9 is intact here)

	test.d	[$r0+LTASK_SIGPENDING]
	bne	_signal_return
	nop
	
_Rexit:
	;; this epilogue MUST match the prologues in multiple_interrupt, irq.h and ptregs.h
#ifdef CONFIG_ETRAX_DEBUG_INTERRUPT
	move.d $pc,$r10
	move $ccr,$r11
	move.d	[$sp + LDCCR], $r12; regs->dccr	
	jsr log_int
#endif
	pop	$r10		; frametype
	bne	_RBFexit	; was not CRIS_FRAME_NORMAL, handle otherwise
	addq	4, $sp		; skip orig_r10, in delayslot
	movem	[$sp+], $r13	; registers r0-r13
	pop	$mof		; multiply overflow register 
	pop	$dccr		; condition codes
	pop	$srp		; subroutine return pointer
	;; now we have a 4-word SBFS frame which we do not want to restore
	;; using RBF since it was not stacked with SBFS. instead we would like to
	;; just get the PC value to restart it with, and skip the rest of
	;; the frame.
	;; Also notice that it's important to use instructions here that
	;; keep the interrupts disabled (since we've already popped DCCR)
	move	[$sp=$sp+16], $p8; pop the SBFS frame from the sp
	jmpu	[$sp-16]	; return through the irp field in the sbfs frame

_RBFexit:
	movem	[$sp+], $r13	; registers r0-r13, in delay slot
	pop	$mof		; multiply overflow register 
	pop	$dccr		; condition codes
	pop	$srp		; subroutine return pointer
	rbf	[$sp+]		; return by popping the CPU status

_tracesys:
	;; this first invocation of syscall_trace _requires_ that
	;; LR10 in the frame contains -ENOSYS (as is set in the beginning
	;; of system_call).

	jsr	syscall_trace

	;; now we should more or less do the same things as in the system_call
	;; but since our argument regs got clobbered during syscall_trace and
	;; because syscall_trace might want to alter them, we need to reload them
	;; from the stack-frame as we use them.

	;; check for sanity in the requested syscall number

	move.d	[$sp+LR9], $r9
	movs.w	-ENOSYS, $r10
	cmpu.w	NR_syscalls, $r9	
	bcc	1f
	lslq	2, $r9		;  multiply by 4, in the delay slot

	;; read the system call vector entry into r9
	
	move.d	[$r9+sys_call_table], $r9

	;; restore r10, r11, r12, r13, mof and srp into the needed registers

	move.d	[$sp+LORIG_R10], $r10  ; LR10 is already filled with -ENOSYS.
	move.d	[$sp+LR11],      $r11
	move.d	[$sp+LR12],      $r12
	move.d	[$sp+LR13],      $r13
	move	[$sp+LMOF],      $mof
	move	[$sp+LSRP],      $srp

	;; as a bonus 7th parameter, we give the location on the stack
	;; of the register structure itself. some syscalls need this.

	push	$sp
	
	;; the fifth and sixth parameters needs to be put on the stack for
	;; the system call to find them

	push	$srp
	push	$mof

	jsr	$r9		; actually call the system-call
	addq	3*4, $sp	; pop the srp, mof and regs parameters

1:	move.d	$r10, [$sp+LR10]; save the return value

	;; second call of syscall_trace, to let it grab the results
		
	jsr	syscall_trace

	moveq	1, $r9		; "parameter" to ret_from_sys_call to show it was a sys call
	ba	ret_from_sys_call
	nop
		
	;; resume performs the actual task-switching, by switching stack pointers
	;; input arguments: r10 = prev, r11 = next, r12 = thread offset in task struct
	;; returns old current in r10
	;;
	;; TODO:  see the i386 version. The switch_to which calls resume in our version
	;;        could really be an inline asm of this.

resume:	
	push	$srp		         ; we keep the old/new PC on the stack 
	add.d	$r12, $r10		 ; r10 = current tasks tss
	move	$dccr, [$r10+LTHREAD_DCCR] ; save irq enable state
#ifdef CONFIG_ETRAX_DEBUG_INTERRUPT
	push $r9
	push $r10
	push $r11
	push $r12
	push $r13
	move.d $pc,$r10
	move $ccr,$r11
	clear.d $r12
	jsr log_int
	pop $r13
	pop $r12
	pop $r11
	pop $r10
	pop $r9
#endif
	di

	move	$usp, [$r10+LTHREAD_USP] ; save user-mode stackpointer
	
	;; See copy_thread for the reason why register R9 is saved.
	subq	10*4, $sp
	movem	$r9, [$sp]		 ; save non-scratch registers and R9.
	
	move.d	$sp, [$r10+LTHREAD_KSP]	 ; save the kernel stack pointer for the old task
	move.d	$sp, $r10		 ; return last running task in r10
	and.d   -8192, $r10	         ; get task ptr from stackpointer
	add.d	$r12, $r11		 ; find the new tasks tss
	move.d	[$r11+LTHREAD_KSP], $sp	 ; switch into the new stackframe by restoring kernel sp

	movem	[$sp+], $r9		 ; restore non-scratch registers and R9.

	move	[$r11+LTHREAD_USP], $usp ; restore user-mode stackpointer
	
	move	[$r11+LTHREAD_DCCR], $dccr ; restore irq enable status
	jump	[$sp+]		         ; restore PC

	;; This is the MMU bus fault handler.
	;; It needs to stack the CPU status and overall is different
	;; from the other interrupt handlers.

mmu_bus_fault:	
	sbfs	[$sp=$sp-16]	; push the internal CPU status
	;; the first longword in the sbfs frame was the interrupted PC
	;; which fits nicely with the "IRP" slot in pt_regs normally used to
	;; contain the return address. used by Oops to print kernel errors..
	push	$srp		; make a stackframe similar to pt_regs
	push	$dccr
	push	$mof
	di
	subq	14*4, $sp
	movem	$r13, [$sp]
	push	$r10		; dummy orig_r10
	moveq	1, $r10
	push	$r10		; frametype == 1, BUSFAULT frame type

	move.d	$sp, $r10	; pt_regs argument to handle_mmu_bus_fault
		
	jsr	handle_mmu_bus_fault  ; in arch/cris/mm/fault.c

	;; now we need to return through the normal path, we cannot just
	;; do the RBFexit since we might have killed off the running
	;; process due to a SEGV, scheduled due to a page blocking or
	;; whatever.

	moveq	0, $r9		; busfault is equivalent to an irq
		
	ba	ret_from_intr
	nop
		
	;; special handlers for breakpoint and NMI
#if 0			
hwbreakpoint:
	push	$dccr
	di
	push	$r10
	push	$r11
	push	$r12
	push	$r13
	clearf	b
	move	$brp,$r11
	move.d	[hw_bp_msg],$r10
	jsr	printk
	setf	b
	pop	$r13
	pop	$r12
	pop	$r11
	pop	$r10
	pop	$dccr
	retb
	nop
#else
hwbreakpoint:
	push	$dccr
	di
#if 1
	push	$r10
	push	$r11
	move.d	[hw_bp_trig_ptr],$r10
	move.d	[$r10],$r11
	cmp.d	42,$r11
	beq	1f
	nop
	move	$brp,$r11
	move.d	$r11,[$r10+]
	move.d	$r10,[hw_bp_trig_ptr]
1:	pop	$r11
	pop	$r10
#endif
	pop	$dccr
	retb
	nop
#endif
	
IRQ1_interrupt:

#if defined(CONFIG_ETRAX_WATCHDOG) && !defined(CONFIG_SVINTO_SIM)
;; If we receive a watchdog interrupt while it is not expected, then set
;; up a canonical frame and dump register contents before dying.

	;; this prologue MUST match the one in irq.h and the struct in ptregs.h!!!
	move	$brp,[$sp=$sp-16]; instruction pointer and room for a fake SBFS frame
	push	$srp
	push	$dccr
	push	$mof
	di
	subq	14*4, $sp
	movem	$r13, [$sp]
	push	$r10		; push orig_r10
	clear.d [$sp=$sp-4]	; frametype == 0, normal frame

;; We don't check that we actually were bit by the watchdog as opposed to
;; an external NMI, since there is currently no handler for external NMI.

;; Check if we're waiting for reset to happen, as signalled by
;; hard_reset_now setting cause_of_death to a magic value.  If so, just
;; get stuck until reset happens.
	.comm	cause_of_death, 4	;; Don't declare this anywhere.
	move.d	[cause_of_death], $r10
	cmp.d	0xbedead, $r10
_killed_by_death:
	beq	_killed_by_death
	nop

;; We'll see this in ksymoops dumps.
Watchdog_bite:

#ifdef CONFIG_ETRAX_WATCHDOG_NICE_DOGGY
	;; We just restart the watchdog here to be sure we dont get
	;; hit while printing the watchdogmsg below
	;; This restart is compatible with the rest of the C-code, so
	;; the C-code can keep restarting the watchdog after this point.
	;; The non-NICE_DOGGY code below though, disables the possibility
	;; to restart since it changes the watchdog key, to avoid any
	;; buggy loops etc. keeping the watchdog alive after this.
	jsr	reset_watchdog
#else
;; We need to extend the 3.3ms after the NMI at watchdog bite, so we have
;; time for an oops-dump over a 115k2 serial wire.  Another 100ms should do.

;; Change the watchdog key to an arbitrary 3-bit value and restart the
;; watchdog.
#define WD_INIT 2
	moveq	  IO_FIELD (R_WATCHDOG, key, WD_INIT), $r10
	move.d	R_WATCHDOG, $r11

	move.d	$r10, [$r11]
	moveq	  IO_FIELD (R_WATCHDOG, key,				\
			    IO_EXTRACT (R_WATCHDOG, key,		\
					IO_MASK (R_WATCHDOG, key))	\
			    ^ WD_INIT)					\
		| IO_STATE (R_WATCHDOG, enable, start), $r10
	move.d	$r10, [$r11]
#endif
	
;; Note that we don't do "setf m" here (or after two necessary NOPs),
;; since *not* doing that saves us from re-entrancy checks.  We don't want
;; to get here again due to possible subsequent NMIs; we want the watchdog
;; to reset us.

	move.d	_watchdogmsg,$r10
	jsr	printk

	move.d	$sp, $r10
	jsr	watchdog_bite_hook

;; This nop is here so we see the "Watchdog_bite" label in ksymoops dumps
;; rather than "spurious_interrupt".
	nop
;; At this point we drop down into spurious_interrupt, which will do a
;; hard reset.

	.section .rodata,"a"
_watchdogmsg:
	.ascii	"Oops: bitten by watchdog\n\0"
	.previous

#endif /* CONFIG_ETRAX_WATCHDOG and not CONFIG_SVINTO_SIM */

spurious_interrupt:	
	di
	jump hard_reset_now

	;; this handles the case when multiple interrupts arrive at the same time
	;; we jump to the first set interrupt bit in a priority fashion
	;; the hardware will call the unserved interrupts after the handler finishes
	
multiple_interrupt:
	;; this prologue MUST match the one in irq.h and the struct in ptregs.h!!!
	move	$irp,[$sp=$sp-16]; instruction pointer and room for a fake SBFS frame
	push	$srp
	push	$dccr
	push	$mof
	di
	subq	14*4, $sp
	movem	$r13, [$sp]
	push	$r10		; push orig_r10
	clear.d [$sp=$sp-4]	; frametype == 0, normal frame
	
	move.d	irq_shortcuts + 8, $r1
	moveq	2, $r2		; first bit we care about is the timer0 irq
	move.d	[R_VECT_MASK_RD], $r0; read the irq bits that triggered the multiple irq
1:	
	btst	$r2, $r0	; check for the irq given by bit r2
	bmi	_do_shortcut	; actually do the shortcut
	nop
	addq	1, $r2		; next vector bit
	addq	4, $r1		; next vector
	cmp.b	32, $r2
	bne	1b	; process all irq's up to and including number 31
	nop
	
	;; strange, we didn't get any set vector bits.. oh well, just return
	
	ba	_Rexit
	nop

_do_shortcut:
	test.d	[$r1]
	beq	_Rexit
	nop
	jump	[$r1]		; jump to the irq handlers shortcut

do_sigtrap:
	;; 
	;; SIGTRAP the process that executed the break instruction.
	;; Make a frame that Rexit in entry.S expects.
	;;
	move	$brp, [$sp=$sp-16]	; Push BRP while faking a cpu status record.
	push	$srp			; Push subroutine return pointer.
	push	$dccr			; Push condition codes.
	push	$mof			; Push multiply overflow reg.
	di				; Need to disable irq's at this point.
	subq	14*4, $sp		; Make room for r0-r13.
	movem	$r13, [$sp]		; Push the r0-r13 registers.
	push	$r10			; Push orig_r10.
	clear.d	[$sp=$sp-4]		; Frametype - this is a normal stackframe.

	movs.w	-8192,$r9		; THREAD_SIZE == 8192
	and.d	$sp, $r9
	move.d	[$r9+LTASK_PID], $r10	; current->pid as arg1.
	moveq	5, $r11			; SIGTRAP as arg2.
	jsr	sys_kill       
	jump	ret_from_intr		; Use the return routine for interrupts.

gdb_handle_breakpoint:	
	push	$dccr
	push	$r0
#ifdef CONFIG_ETRAX_KGDB
	move	$dccr, $r0		; U-flag not affected by previous insns. 
	btstq	8, $r0			; Test the U-flag.
	bmi	_ugdb_handle_breakpoint	; Go to user mode debugging. 
	nop				; Empty delay slot (cannot pop r0 here). 
	pop	$r0			; Restore r0.
	ba	kgdb_handle_breakpoint	; Go to kernel debugging. 
	pop	$dccr			; Restore dccr in delay slot.
#endif
	
_ugdb_handle_breakpoint:	
	move	$brp, $r0		; Use r0 temporarily for calculation.
	subq	2, $r0			; Set to address of previous instruction.
	move	$r0, $brp
	pop	$r0			; Restore r0. 
	ba	do_sigtrap		; SIGTRAP the offending process. 
	pop	$dccr			; Restore dccr in delay slot.
	
	.data

hw_bp_trigs:
	.space 64*4
hw_bp_trig_ptr:
	.dword hw_bp_trigs

/*
 * This is the mechanism for creating a new kernel thread.
 *
 * NOTE! Only a kernel-only process (i.e. the swapper or direct descendants
 * who haven't done an "execve()") should use this: it will work within
 * a system call from a "real" process, but the process memory space will
 * not be free'd until both the parent and the child have exited.
 *
 * This *can* be done in C with an single-asm-wrapped-in-a-function, but you
 * get more or less gross code.  The safer you make the asm-constraints,
 * the grosser the code, at least with the gcc version in cris-dist-1.13.
 */

/* int arch_kernel_thread(int (*fn)(void *), void * arg, unsigned long flags) */
/*                   r10                r11         r12  */

	.text
	.global arch_kernel_thread
arch_kernel_thread:

	/* Save ARG for later.  */
	move.d $r11, $r13

	/* r11 is argument 2 to clone, the flags */
	move.d  $r12, $r11
	or.w	LCLONE_VM, $r11

	/* Save FN for later.  */
	move.d	$r10, $r12

	/* r9 contains syscall number, to sys_clone */
	movu.w	__NR_clone, $r9

	/* r10 is argument 1 to clone */
	clear.d	$r10
	
	/* call sys_clone, this will fork */
	break	13

	/* parent or child? child returns 0 here. */
	test.d	$r10

	/* jump if parent */
	bne	1f
	nop              /* delay slot */

	/* set argument to function to call */
	move.d	$r13, $r10

	/* call specified function */
	jsr	$r12
	/* If we ever return from the function, something bad has happened.  */

	/* r9 is sys_exit syscall number */
	movu.w	__NR_exit, $r9

	/* Give a really bad exit-value */
	moveq	-1, $r10

	/* call sys_exit, killing the child */
	break	13
1:
	ret
	nop              /* delay slot */

	.section .rodata,"a"
sys_call_table:	
	.long SYMBOL_NAME(sys_ni_syscall)	/* 0  -  old "setup()" system call*/
	.long SYMBOL_NAME(sys_exit)
	.long SYMBOL_NAME(sys_fork)
	.long SYMBOL_NAME(sys_read)
	.long SYMBOL_NAME(sys_write)
	.long SYMBOL_NAME(sys_open)		/* 5 */
	.long SYMBOL_NAME(sys_close)
	.long SYMBOL_NAME(sys_waitpid)
	.long SYMBOL_NAME(sys_creat)
	.long SYMBOL_NAME(sys_link)
	.long SYMBOL_NAME(sys_unlink)		/* 10 */
	.long SYMBOL_NAME(sys_execve)
	.long SYMBOL_NAME(sys_chdir)
	.long SYMBOL_NAME(sys_time)
	.long SYMBOL_NAME(sys_mknod)
	.long SYMBOL_NAME(sys_chmod)		/* 15 */
	.long SYMBOL_NAME(sys_lchown16)
	.long SYMBOL_NAME(sys_ni_syscall)	/* old break syscall holder */
	.long SYMBOL_NAME(sys_stat)
	.long SYMBOL_NAME(sys_lseek)
	.long SYMBOL_NAME(sys_getpid)		/* 20 */
	.long SYMBOL_NAME(sys_mount)
	.long SYMBOL_NAME(sys_oldumount)
	.long SYMBOL_NAME(sys_setuid16)
	.long SYMBOL_NAME(sys_getuid16)
	.long SYMBOL_NAME(sys_stime)		/* 25 */
	.long SYMBOL_NAME(sys_ptrace)
	.long SYMBOL_NAME(sys_alarm)
	.long SYMBOL_NAME(sys_fstat)
	.long SYMBOL_NAME(sys_pause)
	.long SYMBOL_NAME(sys_utime)		/* 30 */
	.long SYMBOL_NAME(sys_ni_syscall)	/* old stty syscall holder */
	.long SYMBOL_NAME(sys_ni_syscall)	/* old gtty syscall holder */
	.long SYMBOL_NAME(sys_access)
	.long SYMBOL_NAME(sys_nice)
	.long SYMBOL_NAME(sys_ni_syscall)	/* 35  old ftime syscall holder */
	.long SYMBOL_NAME(sys_sync)
	.long SYMBOL_NAME(sys_kill)
	.long SYMBOL_NAME(sys_rename)
	.long SYMBOL_NAME(sys_mkdir)
	.long SYMBOL_NAME(sys_rmdir)		/* 40 */
	.long SYMBOL_NAME(sys_dup)
	.long SYMBOL_NAME(sys_pipe)
	.long SYMBOL_NAME(sys_times)
	.long SYMBOL_NAME(sys_ni_syscall)	/* old prof syscall holder */
	.long SYMBOL_NAME(sys_brk)		/* 45 */
	.long SYMBOL_NAME(sys_setgid16)
	.long SYMBOL_NAME(sys_getgid16)
	.long SYMBOL_NAME(sys_signal)
	.long SYMBOL_NAME(sys_geteuid16)
	.long SYMBOL_NAME(sys_getegid16)	/* 50 */
	.long SYMBOL_NAME(sys_acct)
	.long SYMBOL_NAME(sys_umount)		/* recycled never used phys() */
	.long SYMBOL_NAME(sys_ni_syscall)	/* old lock syscall holder */
	.long SYMBOL_NAME(sys_ioctl)
	.long SYMBOL_NAME(sys_fcntl)		/* 55 */
	.long SYMBOL_NAME(sys_ni_syscall)	/* old mpx syscall holder */
	.long SYMBOL_NAME(sys_setpgid)
	.long SYMBOL_NAME(sys_ni_syscall)	/* old ulimit syscall holder */
	.long SYMBOL_NAME(sys_ni_syscall)       /* old sys_olduname holder */
	.long SYMBOL_NAME(sys_umask)		/* 60 */
	.long SYMBOL_NAME(sys_chroot)
	.long SYMBOL_NAME(sys_ustat)
	.long SYMBOL_NAME(sys_dup2)
	.long SYMBOL_NAME(sys_getppid)
	.long SYMBOL_NAME(sys_getpgrp)		/* 65 */
	.long SYMBOL_NAME(sys_setsid)
	.long SYMBOL_NAME(sys_sigaction)
	.long SYMBOL_NAME(sys_sgetmask)
	.long SYMBOL_NAME(sys_ssetmask)
	.long SYMBOL_NAME(sys_setreuid16)	/* 70 */
	.long SYMBOL_NAME(sys_setregid16)
	.long SYMBOL_NAME(sys_sigsuspend)
	.long SYMBOL_NAME(sys_sigpending)
	.long SYMBOL_NAME(sys_sethostname)
	.long SYMBOL_NAME(sys_setrlimit)	/* 75 */
	.long SYMBOL_NAME(sys_old_getrlimit)
	.long SYMBOL_NAME(sys_getrusage)
	.long SYMBOL_NAME(sys_gettimeofday)
	.long SYMBOL_NAME(sys_settimeofday)
	.long SYMBOL_NAME(sys_getgroups16)	/* 80 */
	.long SYMBOL_NAME(sys_setgroups16)
	.long SYMBOL_NAME(sys_select)           /* was old_select in Linux/E100 */
	.long SYMBOL_NAME(sys_symlink)
	.long SYMBOL_NAME(sys_lstat)
	.long SYMBOL_NAME(sys_readlink)		/* 85 */
	.long SYMBOL_NAME(sys_uselib)
	.long SYMBOL_NAME(sys_swapon)
	.long SYMBOL_NAME(sys_reboot)
	.long SYMBOL_NAME(old_readdir)
	.long SYMBOL_NAME(old_mmap)		/* 90 */
	.long SYMBOL_NAME(sys_munmap)
	.long SYMBOL_NAME(sys_truncate)
	.long SYMBOL_NAME(sys_ftruncate)
	.long SYMBOL_NAME(sys_fchmod)
	.long SYMBOL_NAME(sys_fchown16)		/* 95 */
	.long SYMBOL_NAME(sys_getpriority)
	.long SYMBOL_NAME(sys_setpriority)
	.long SYMBOL_NAME(sys_ni_syscall)	/* old profil syscall holder */
	.long SYMBOL_NAME(sys_statfs)
	.long SYMBOL_NAME(sys_fstatfs)		/* 100 */
	.long SYMBOL_NAME(sys_ni_syscall)       /* sys_ioperm in i386 */
	.long SYMBOL_NAME(sys_socketcall)
	.long SYMBOL_NAME(sys_syslog)
	.long SYMBOL_NAME(sys_setitimer)
	.long SYMBOL_NAME(sys_getitimer)	/* 105 */
	.long SYMBOL_NAME(sys_newstat)
	.long SYMBOL_NAME(sys_newlstat)
	.long SYMBOL_NAME(sys_newfstat)
	.long SYMBOL_NAME(sys_ni_syscall)	/* old sys_uname holder */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_iopl in i386 */
	.long SYMBOL_NAME(sys_vhangup)
	.long SYMBOL_NAME(sys_ni_syscall)	/* old "idle" system call */
	.long SYMBOL_NAME(sys_ni_syscall)	/* vm86old in i386 */
	.long SYMBOL_NAME(sys_wait4)
	.long SYMBOL_NAME(sys_swapoff)		/* 115 */
	.long SYMBOL_NAME(sys_sysinfo)
	.long SYMBOL_NAME(sys_ipc)
	.long SYMBOL_NAME(sys_fsync)
	.long SYMBOL_NAME(sys_sigreturn)
	.long SYMBOL_NAME(sys_clone)		/* 120 */
	.long SYMBOL_NAME(sys_setdomainname)
	.long SYMBOL_NAME(sys_newuname)
	.long SYMBOL_NAME(sys_ni_syscall)	/* TODO sys_modify_ldt - do something ?*/
	.long SYMBOL_NAME(sys_adjtimex)
	.long SYMBOL_NAME(sys_mprotect)		/* 125 */
	.long SYMBOL_NAME(sys_sigprocmask)
	.long SYMBOL_NAME(sys_create_module)
	.long SYMBOL_NAME(sys_init_module)
	.long SYMBOL_NAME(sys_delete_module)
	.long SYMBOL_NAME(sys_get_kernel_syms)	/* 130 */
	.long SYMBOL_NAME(sys_quotactl)
	.long SYMBOL_NAME(sys_getpgid)
	.long SYMBOL_NAME(sys_fchdir)
	.long SYMBOL_NAME(sys_bdflush)
	.long SYMBOL_NAME(sys_sysfs)		/* 135 */
	.long SYMBOL_NAME(sys_personality)
	.long SYMBOL_NAME(sys_ni_syscall)	/* for afs_syscall */
	.long SYMBOL_NAME(sys_setfsuid16)
	.long SYMBOL_NAME(sys_setfsgid16)
	.long SYMBOL_NAME(sys_llseek)		/* 140 */
	.long SYMBOL_NAME(sys_getdents)
	.long SYMBOL_NAME(sys_select)
	.long SYMBOL_NAME(sys_flock)
	.long SYMBOL_NAME(sys_msync)
	.long SYMBOL_NAME(sys_readv)		/* 145 */
	.long SYMBOL_NAME(sys_writev)
	.long SYMBOL_NAME(sys_getsid)
	.long SYMBOL_NAME(sys_fdatasync)
	.long SYMBOL_NAME(sys_sysctl)
	.long SYMBOL_NAME(sys_mlock)		/* 150 */
	.long SYMBOL_NAME(sys_munlock)
	.long SYMBOL_NAME(sys_mlockall)
	.long SYMBOL_NAME(sys_munlockall)
	.long SYMBOL_NAME(sys_sched_setparam)
	.long SYMBOL_NAME(sys_sched_getparam)   /* 155 */
	.long SYMBOL_NAME(sys_sched_setscheduler)
	.long SYMBOL_NAME(sys_sched_getscheduler)
	.long SYMBOL_NAME(sys_sched_yield)
	.long SYMBOL_NAME(sys_sched_get_priority_max)
	.long SYMBOL_NAME(sys_sched_get_priority_min)  /* 160 */
	.long SYMBOL_NAME(sys_sched_rr_get_interval)
	.long SYMBOL_NAME(sys_nanosleep)
	.long SYMBOL_NAME(sys_mremap)
	.long SYMBOL_NAME(sys_setresuid16)
	.long SYMBOL_NAME(sys_getresuid16)	/* 165 */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_vm86 */
	.long SYMBOL_NAME(sys_query_module)
	.long SYMBOL_NAME(sys_poll)
	.long SYMBOL_NAME(sys_nfsservctl)
	.long SYMBOL_NAME(sys_setresgid16)	/* 170 */
	.long SYMBOL_NAME(sys_getresgid16)
	.long SYMBOL_NAME(sys_prctl)
	.long SYMBOL_NAME(sys_rt_sigreturn)
	.long SYMBOL_NAME(sys_rt_sigaction)
	.long SYMBOL_NAME(sys_rt_sigprocmask)	/* 175 */
	.long SYMBOL_NAME(sys_rt_sigpending)
	.long SYMBOL_NAME(sys_rt_sigtimedwait)
	.long SYMBOL_NAME(sys_rt_sigqueueinfo)
	.long SYMBOL_NAME(sys_rt_sigsuspend)
	.long SYMBOL_NAME(sys_pread)		/* 180 */
	.long SYMBOL_NAME(sys_pwrite)
	.long SYMBOL_NAME(sys_chown16)
	.long SYMBOL_NAME(sys_getcwd)
	.long SYMBOL_NAME(sys_capget)
	.long SYMBOL_NAME(sys_capset)           /* 185 */
	.long SYMBOL_NAME(sys_sigaltstack)
	.long SYMBOL_NAME(sys_sendfile)
	.long SYMBOL_NAME(sys_ni_syscall)	/* streams1 */
	.long SYMBOL_NAME(sys_ni_syscall)	/* streams2 */
	.long SYMBOL_NAME(sys_vfork)            /* 190 */
	.long SYMBOL_NAME(sys_getrlimit)
	.long SYMBOL_NAME(sys_mmap2)
	.long SYMBOL_NAME(sys_truncate64)
	.long SYMBOL_NAME(sys_ftruncate64)
	.long SYMBOL_NAME(sys_stat64)		/* 195 */
	.long SYMBOL_NAME(sys_lstat64)
	.long SYMBOL_NAME(sys_fstat64)
	.long SYMBOL_NAME(sys_lchown)
	.long SYMBOL_NAME(sys_getuid)
	.long SYMBOL_NAME(sys_getgid)		/* 200 */
	.long SYMBOL_NAME(sys_geteuid)
	.long SYMBOL_NAME(sys_getegid)
	.long SYMBOL_NAME(sys_setreuid)
	.long SYMBOL_NAME(sys_setregid)
	.long SYMBOL_NAME(sys_getgroups)	/* 205 */
	.long SYMBOL_NAME(sys_setgroups)
	.long SYMBOL_NAME(sys_fchown)
	.long SYMBOL_NAME(sys_setresuid)
	.long SYMBOL_NAME(sys_getresuid)
	.long SYMBOL_NAME(sys_setresgid)	/* 210 */
	.long SYMBOL_NAME(sys_getresgid)
	.long SYMBOL_NAME(sys_chown)
	.long SYMBOL_NAME(sys_setuid)
	.long SYMBOL_NAME(sys_setgid)
	.long SYMBOL_NAME(sys_setfsuid)		/* 215 */
	.long SYMBOL_NAME(sys_setfsgid)
	.long SYMBOL_NAME(sys_pivot_root)
	.long SYMBOL_NAME(sys_mincore)
	.long SYMBOL_NAME(sys_madvise)
	.long SYMBOL_NAME(sys_getdents64)       /* 220 */
	.long SYMBOL_NAME(sys_fcntl64)
	.long SYMBOL_NAME(sys_ni_syscall)	/* reserved for TUX */
        .long SYMBOL_NAME(sys_ni_syscall)       /* Reserved for Security */
        .long SYMBOL_NAME(sys_gettid)
        .long SYMBOL_NAME(sys_readahead)        /* 225 */
	.long SYMBOL_NAME(sys_setxattr)
	.long SYMBOL_NAME(sys_lsetxattr)
	.long SYMBOL_NAME(sys_fsetxattr)
	.long SYMBOL_NAME(sys_getxattr)
	.long SYMBOL_NAME(sys_lgetxattr)	/* 230 */
	.long SYMBOL_NAME(sys_fgetxattr)
	.long SYMBOL_NAME(sys_listxattr)
	.long SYMBOL_NAME(sys_llistxattr)
	.long SYMBOL_NAME(sys_flistxattr)
	.long SYMBOL_NAME(sys_removexattr)	/* 235 */
	.long SYMBOL_NAME(sys_lremovexattr)
	.long SYMBOL_NAME(sys_fremovexattr)
 	.long SYMBOL_NAME(sys_tkill)
	.long SYMBOL_NAME(sys_ni_syscall)	/* reserved for sendfile64 */
	.long SYMBOL_NAME(sys_ni_syscall)	/* 240 reserved for futex */
	.long SYMBOL_NAME(sys_ni_syscall)	/* reserved for sched_setaffinity */
	.long SYMBOL_NAME(sys_ni_syscall)	/* reserved for sched_getaffinity */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_set_thread_area */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_get_thread_area */
	.long SYMBOL_NAME(sys_ni_syscall)	/* 245 sys_io_setup */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_io_destroy */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_io_getevents */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_io_submit */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_io_cancel */
	.long SYMBOL_NAME(sys_ni_syscall)	/* 250 sys_alloc_hugepages */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_free_hugepages */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_exit_group */
	
        /*
         * NOTE!! This doesn't have to be exact - we just have
         * to make sure we have _enough_ of the "sys_ni_syscall"
         * entries. Don't panic if you notice that this hasn't
         * been shrunk every time we add a new system call.
         */

	.rept NR_syscalls-(.-sys_call_table)/4
		.long SYMBOL_NAME(sys_ni_syscall)
	.endr
	
