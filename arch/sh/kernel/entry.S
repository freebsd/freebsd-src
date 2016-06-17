/* $Id: entry.S,v 1.1.1.1.2.19 2003/11/26 21:28:28 kkojima Exp $
 *
 *  linux/arch/sh/entry.S
 *
 *  Copyright (C) 1999, 2000  Niibe Yutaka
 *
 *  Modified to support SH7300. Takashi Kusuda (Jun 2003).
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 */

#include <linux/sys.h>
#include <linux/linkage.h>
#include <linux/config.h>


! NOTE:
! GNU as (as of 2.9.1) changes bf/s into bt/s and bra, when the address
! to be jumped is too far, but it causes illegal slot exception.

/*	
 * entry.S contains the system-call and fault low-level handling routines.
 * This also contains the timer-interrupt handler, as well as all interrupts
 * and faults that can result in a task-switch.
 *
 * NOTE: This code handles signal-recognition, which happens every time
 * after a timer-interrupt and after each system call.
 *
 * NOTE: This code uses a convention that instructions in the delay slot
 * of a transfer-control instruction are indented by an extra space, thus:
 *
 *    jmp	@k0	    ! control-transfer instruction
 *     ldc	k1, ssr     ! delay slot
 */

/*
 * ABI at ret_from_syscall:
 *	r8: from_syscall (0 or 1) indicating it's from syscall or not
 *	r9: original r0 (to restart system call)
 *
 * Stack layout in 'ret_from_syscall':
 * 	ptrace needs to have all regs on the stack.
 *	if the order here is changed, it needs to be
 *	updated in ptrace.c and ptrace.h
 *
 *	r0
 *      ...
 *	r15 = stack pointer
 *	spc
 *	pr
 *	ssr
 *	gbr
 *	mach
 *	macl
 *	dummy
 */

/*
 * These are offsets into the task-struct.
 */
flags		=  4
sigpending	=  8
need_resched	= 20
tsk_ptrace	= 24

PT_TRACESYS  = 0x00000002

ENOSYS = 38
EINVAL = 22

#if defined(__sh3__)
TRA     = 0xffffffd0
EXPEVT  = 0xffffffd4
#if defined(CONFIG_CPU_SUBTYPE_SH7300) || defined(CONFIG_CPU_SUBTYPE_SH7707) || \ 
    defined(CONFIG_CPU_SUBTYPE_SH7709)
INTEVT  = 0xa4000000		! INTEVTE2(0xa4000000)
#else
INTEVT  = 0xffffffd8
#endif
MMU_TEA = 0xfffffffc		! TLB Exception Address Register
#elif defined(__SH4__)
TRA     = 0xff000020
EXPEVT  = 0xff000024
INTEVT  = 0xff000028
MMU_TEA = 0xff00000c		! TLB Exception Address Register
#endif

#if defined(CONFIG_KGDB_NMI)
NMI_VEC = 0x1c0			! Must catch early for debounce
#endif

/* Offsets to the stack */
OFF_R0         =  0		/* Return value. New ABI also arg4 */
OFF_R1         =  4     	/* New ABI: arg5 */
OFF_R2         =  8     	/* New ABI: arg6 */
OFF_R3         =  12     	/* New ABI: syscall_nr */
OFF_R4         =  16     	/* New ABI: arg0 */
OFF_R5         =  20     	/* New ABI: arg1 */
OFF_R6         =  24     	/* New ABI: arg2 */
OFF_R7         =  28     	/* New ABI: arg3 */
OFF_SP	   =  (15*4)
OFF_PC	   =  (16*4)
OFF_SR	   =  (16*4+8)

#define k0	r0
#define k1	r1
#define k2	r2
#define k3	r3
#define k4	r4

#define current		r7	/* r7_bank1 */
#define g_imask		r6	/* r6_bank1 */
#define k_current	r7_bank	/* r7_bank1 */
#define k_g_imask	r6_bank	/* r6_bank1 */
#define k_ex_code	r2_bank	/* r2_bank1 */

/*
 * Kernel mode register usage:
 *	k0	scratch
 *	k1	scratch
 *	k2	scratch (Exception code)
 *	k3	scratch (Return address)
 *	k4	scratch
 *	k5	reserved
 *	k6	Global Interrupt Mask (0--15 << 4)
 *	k7	CURRENT (pointer to current task)
 */

!
! TLB Miss / Initial Page write exception handling
!			_and_
! TLB hits, but the access violate the protection.
! It can be valid access, such as stack grow and/or C-O-W.
!
!
! Find the pmd/pte entry and loadtlb
! If it's not found, cause address error (SEGV)
!
! Although this could be written in assembly language (and it'd be faster),
! this first version depends *much* on C implementation.
!

#define STI()				\
	mov.l	__INV_IMASK, r11;	\
	stc	sr, r10;		\
	and	r11, r10;		\
	stc	k_g_imask, r11;	\
	or	r11, r10;		\
	ldc	r10, sr

	.align	2
tlb_miss_load:
	bra	call_dpf
	 mov	#0, r5

	.align	2
tlb_miss_store:
	bra	call_dpf
	 mov	#1, r5

	.align	2
initial_page_write:
	bra	call_dpf
	 mov	#1, r5

	.align	2
tlb_protection_violation_load:
	bra	call_dpf
	 mov	#0, r5

	.align	2
tlb_protection_violation_store:
	bra	call_dpf
	 mov	#1, r5

call_dpf:
	mov.l	1f, r0
	mov	r5, r8
	mov.l	@r0, r6
	mov	r6, r9
	mov.l	2f, r0
	sts	pr, r10
	jsr	@r0
	 mov	r15, r4
	!
	tst	r0, r0
	bf/s	0f
	 lds	r10, pr
	rts
	 nop
0:	STI()
	mov.l	3f, r0
	mov	r9, r6
	mov	r8, r5
	jmp	@r0
	 mov	r15, r4

	.align 2
1:	.long	MMU_TEA
2:	.long	SYMBOL_NAME(__do_page_fault)
3:	.long	SYMBOL_NAME(do_page_fault)

	.align	2
address_error_load:
	bra	call_dae
	 mov	#0,r5		! writeaccess = 0

	.align	2
address_error_store:
	bra	call_dae
	 mov	#1,r5		! writeaccess = 1

call_dae:
	mov.l	1f, r0
	mov.l	@r0, r6		! address
	mov.l	2f, r0
	jmp	@r0
	 mov	r15, r4		! regs

	.align 2
1:	.long	MMU_TEA
2:	.long   SYMBOL_NAME(do_address_error)


#if defined(CONFIG_SH_STANDARD_BIOS) || defined(CONFIG_SH_KGDB)
! Handle kernel debug if either kgdb (SW) or gdb-stub (FW) is present.
! If both are configured, handle the debug traps (breakpoints) in SW,
! but still allow BIOS traps to FW.

	.align	2
debug_kernel:
#if defined(CONFIG_SH_STANDARD_BIOS) && defined(CONFIG_SH_KGDB)
	/* Force BIOS call to FW (debug_trap put TRA in r8) */
	mov	r8,r0
	shlr2	r0
	cmp/eq	#0x3f,r0
	bt	debug_kernel_fw
#endif /* CONFIG_SH_STANDARD_BIOS && CONFIG_SH_KGDB */

debug_enter:		
#if defined(CONFIG_SH_KGDB)
	/* Jump to kgdb, pass stacked regs as arg */
debug_kernel_sw:
	mov.l	3f, r0
	jmp	@r0
	 mov	r15, r4
	.align	2
3:	.long	SYMBOL_NAME(kgdb_handle_exception)
#endif /* CONFIG_SH_KGDB */

#if defined(CONFIG_SH_STANDARD_BIOS)
	/* Unwind the stack and jmp to the debug entry */
debug_kernel_fw:
	mov.l	@r15+, r0
	mov.l	@r15+, r1
	mov.l	@r15+, r2
	mov.l	@r15+, r3
	mov.l	@r15+, r4
	mov.l	@r15+, r5
	mov.l	@r15+, r6
	mov.l	@r15+, r7
	stc	sr, r8
	mov.l	1f, r9			! BL =1, RB=1, IMASK=0x0F
	or	r9, r8
	ldc	r8, sr			! here, change the register bank
	mov.l	@r15+, r8
	mov.l	@r15+, r9
	mov.l	@r15+, r10
	mov.l	@r15+, r11
	mov.l	@r15+, r12
	mov.l	@r15+, r13
	mov.l	@r15+, r14
	mov.l	@r15+, k0
	ldc.l	@r15+, spc
	lds.l	@r15+, pr
	mov.l	@r15+, k1
	ldc.l	@r15+, gbr
	lds.l	@r15+, mach
	lds.l	@r15+, macl
	mov	k0, r15
	!
	mov.l	2f, k0
	mov.l	@k0, k0
	jmp	@k0
	 ldc	k1, ssr
	.align	2
1:	.long	0x300000f0
2:	.long	SYMBOL_NAME(gdb_vbr_vector)
#endif /* CONFIG_SH_STANDARD_BIOS */

#endif /* CONFIG_SH_STANDARD_BIOS || CONFIG_SH_KGDB */


	.align	2
debug_trap:	
#if defined(CONFIG_SH_STANDARD_BIOS) || defined(CONFIG_SH_KGDB)
	mov	#OFF_SR, r0
	mov.l	@(r0,r15), r0		! get status register
	shll	r0
	shll	r0			! kernel space?
	bt/s	debug_kernel
#endif
	 mov.l	@r15, r0
	mov.l	1f, r8
	jmp	@r8
	 nop

	.align	2
1:	.long	SYMBOL_NAME(break_point_trap_software)

	.align	2
error:	
	!
	STI()
	mov.l	1f, r0
	jmp	@r0
	 nop
	.align	2
1:	.long	SYMBOL_NAME(do_exception_error)


!
!
!
ENTRY(ret_from_fork)
	mov.l	@r15+,r0
	! Call schedule_tail
	mov.l	1f, r1
	jsr @r1
	 mov	r0, r4
	! If we're being traced, return via syscall_ret_trace, otherwise
	! return directly to ret_from_syscall
	stc	k_current, r0
	mov.l	@(tsk_ptrace,r0), r0	! Is current PTRACE_SYSCALL'd?
	mov	#PT_TRACESYS, r1
	tst	r1, r0
	bt/s	syscall_ret
	 mov	#0, r0
	bra	syscall_ret_trace
	 nop	 

	.align	2
1:	.long	SYMBOL_NAME(schedule_tail)

/*
 * Old syscall interface:
 *
 *	Syscall #: R0
 *	Arguments #0 to #3: R4--R7
 *	more arguments: On the stack
 *	TRA: (number of arguments on the stack) x 4
 *
 * New syscall interface:
 *
 *	Syscall #: R3
 *	Arguments #0 to #3: R4--R7
 *	Arguments #4 to #6: R0, R1, R2
 *	TRA: (number of arguments + 0x10) x 4
 *
 * This code also handles delegating other traps to the BIOS/gdb stub
 * according to:
 *
 * Trap number
 * (TRA>>2) 	    Purpose
 * -------- 	    -------
 * 0x0-0xf  	    old syscall ABI
 * 0x10-0x1f  	    new syscall ABI
 * 0x20-0xff  	    delegated through debug_trap to BIOS/gdb stub.
 *
 * Note: When we're first called, the TRA value must be shifted
 * right 2 bits in order to get the value that was used as the "trapa"
 * argument.
 */

system_call:
	mov.l	__TRA, r9
	mov.l	@r9, r8
	!
	! Is the trap argument >= 0x20? (TRA will be >= 0x80)
	mov	#0x7f, r9
	cmp/hi	r9, r8
	bt	debug_trap
	!
	! New Syscall ABI
	STI()
	!
	stc	k_current, r11
	mov.l	@(tsk_ptrace,r11), r10	! Is current PTRACE_SYSCALL'd?
	mov	#PT_TRACESYS, r11
	tst	r11, r10
	bt	5f
	!                     	Yes it is traced.
	mov.l	__syscall_trace, r11	! Call syscall_trace() which notifies
	jsr	@r11	    	    	! superior (will chomp R[0-7])
	 nop
	!			Reload R0-R4 from kernel stack, where the
	!   	    	    	parent may have modified them using
	!   	    	    	ptrace(POKEUSR).  (Note that R0-R2 are
	!   	    	    	used by the system call handler directly
	!   	    	    	from the kernel stack anyway, so don't need
	!   	    	    	to be reloaded here.)  This allows the parent
	!   	    	    	to rewrite system calls and args on the fly.
	mov.l	@(OFF_R4,r15), r4   ! arg0
	mov.l	@(OFF_R5,r15), r5
	mov.l	@(OFF_R6,r15), r6
	mov.l	@(OFF_R7,r15), r7   ! arg3
	mov.l	@(OFF_R3,r15), r3   ! syscall_nr
	!   	    	    Arrange for syscall_trace() to be called
	!   	    	    again as the system call returns.
	mov.l	__syscall_ret_trace, r10
	bra	6f
	 lds	r10, pr
	!   	    	    No it isn't traced.
	!   	    	    Arrange for normal system call return.
5:	mov.l	__syscall_ret, r10
	lds	r10, pr
	!   	    	    Call the system call handler through the table.
	!   	    	    (both normal and ptrace'd)
	!   	    	    First check for bad syscall number
6:	mov	r3, r9
	mov.l	__n_sys, r10
	cmp/hs	r10, r9
	bf	2f
	!   	    	    Bad syscall number
	rts			! go to syscall_ret or syscall_ret_trace
    	 mov	#-ENOSYS, r0
	!   	    	    Good syscall number
2:	shll2	r9		! x4
	mov.l	__sct, r11
	add	r11, r9
	mov.l	@r9, r11
	!		    Prepare args for do_signal on syscall return
	mov	#1, r8			! indicate it's syscall return
	mov.l	@(OFF_R0,r15), r9	! save original r0 (syscall arg4)
	!
	jmp	@r11	    	! jump to specific syscall handler
	 nop

	! In case of trace
syscall_ret_trace:
	mov.l	r0, @(OFF_R0,r15)		! save the return value
	mov.l	__syscall_trace, r1
	mova	SYMBOL_NAME(ret_from_syscall), r0
	jmp	@r1    	! Call syscall_trace() which notifies superior
	 lds	r0, pr    	! Then return to ret_from_syscall()

	.align	2
__TRA:	.long	TRA
__syscall_trace:
    	.long	SYMBOL_NAME(syscall_trace)
__n_sys:.long	NR_syscalls
__sct:	.long	SYMBOL_NAME(sys_call_table)
__syscall_ret_trace:
	.long	syscall_ret_trace
__syscall_ret:
	.long	syscall_ret
__INV_IMASK:
	.long	0xffffff0f	! ~(IMASK)


	.align	2
reschedule:
	! gUSA handling
	mov.l	@(OFF_SP,r15), r0	! get user space stack pointer
	mov	r0, r1
	shll	r0
	bf/s	1f
	 shll	r0
	bf/s	1f
	 mov	#OFF_PC, r0
	! 				  SP >= 0xc0000000 : gUSA mark
	mov.l	@(r0,r15), r2		! get user space PC (program counter)
	mov.l	@(OFF_R0,r15), r3	! end point
	cmp/hs	r3, r2			! r2 >= r3? 
	bt	1f
	add	r3, r1			! rewind point #2
	mov.l	r1, @(r0,r15)		! reset PC to rewind point #2
	!
1:	mov.l	2f, r1
	mova	SYMBOL_NAME(ret_from_syscall), r0
	jmp	@r1
	 lds	r0, pr
	.align	2
2:	.long	SYMBOL_NAME(schedule)

ret_from_irq:
ret_from_exception:
	mov	#OFF_SR, r0
	mov.l	@(r0,r15), r0	! get status register
	shll	r0
	shll	r0		! kernel space?
	bt	restore_all	! Yes, it's from kernel, go back soon
	!
	bra	ret_from_syscall
	 mov	#0, r8		! indicate it's *not* syscall return

	.align 2
syscall_ret:
	mov.l	r0, @(OFF_R0,r15)	! save the return value
	/* fall through */

ENTRY(ret_from_syscall)
	/* CLI */
	stc	sr, r0
	or	#0xf0, r0
	ldc	r0, sr
	!
	stc	k_current, r1
	mov.l	@(need_resched,r1), r0
	tst	r0, r0
	bf	reschedule
	mov.l	@(sigpending,r1), r0
	tst	r0, r0
	bt	restore_all
signal_return:
	mov.l	__do_signal, r1
	mov	r15, r4
	mov	#0, r5
	mov	r8, r6
	mov	r9, r7
	mova	restore_all, r0
	jmp	@r1
	 lds	r0, pr
	.align	2
__do_signal:
	.long	SYMBOL_NAME(do_signal)
__irq_stat:
	.long	SYMBOL_NAME(irq_stat)

	.align 2
restore_all:
	mov.l	@r15+, r0
	mov.l	@r15+, r1
	mov.l	@r15+, r2
	mov.l	@r15+, r3
	mov.l	@r15+, r4
	mov.l	@r15+, r5
	mov.l	@r15+, r6
	mov.l	@r15+, r7
	!
	stc	sr, r8
	mov.l	__blrb_flags, r9	! BL =1, RB=1
	or	r9, r8
	ldc	r8, sr			! here, change the register bank
	!
	mov.l	@r15+, r8
	mov.l	@r15+, r9
	mov.l	@r15+, r10
	mov.l	@r15+, r11
	mov.l	@r15+, r12
	mov.l	@r15+, r13
	mov.l	@r15+, r14
	mov.l	@r15+, k4		! original stack pointer
	ldc.l	@r15+, spc
	lds.l	@r15+, pr
	mov.l	@r15+, k3		! original SR
	ldc.l	@r15+, gbr
	lds.l	@r15+, mach
	lds.l	@r15+, macl
 
#ifdef CONFIG_SH_DSP
	mov.l	@r15+, k0		! DSP mode marker
	mov.l	5f, k1
	cmp/eq	k0, k1			! Do we have a DSP stack frame?
	bf	skip_restore

	stc	sr, k0			! Enable CPU DSP mode
	or	k1, k0			! (within kernel it may be disabled)
	ldc	k0, sr
	mov	r2, k0			! Backup r2

	! Restore DSP registers from stack
	mov	r15, r2
	movs.l	@r2+, a1
	movs.l	@r2+, a0g
	movs.l	@r2+, a1g
	movs.l	@r2+, m0
	movs.l	@r2+, m1
	mov	r2, r15

	lds.l	@r15+, a0
	lds.l	@r15+, x0
	lds.l	@r15+, x1
	lds.l	@r15+, y0
	lds.l	@r15+, y1
	lds.l	@r15+, dsr
	ldc.l	@r15+, rs
	ldc.l	@r15+, re
	ldc.l	@r15+, mod

	mov	k0, r2			! Restore r2
skip_restore:
#endif

	!
	!
	! Calculate new SR value
	mov	k3, k2			! original SR value
	mov.l	1f, k1
	stc	sr, k0
	and	k1, k0			! Get current FD-bit
	mov.l	2f, k1
	and	k1, k2			! Mask orignal SR value
	or	k0, k2			! Inherit current FD-bit
	!
	mov	k3, k0			! Calculate IMASK-bits
	shlr2	k0
	and	#0x3c, k0
	cmp/eq	#0x3c, k0
	bt/s	7f
	 shll2	k0
	mov	g_imask, k0
	!
7:	or	k0, k2			! Set the IMASK-bits
	ldc	k2, ssr
	!
#if defined(CONFIG_KGDB_NMI)
	! Clear in_nmi
	mov.l	4f, k0
	mov	#0, k1
	mov.b	k1, @k0
#endif
	mov.l	@r15+, k2		! restore EXPEVT
	mov	k4, r15
	rte
	 nop

	.align	2
__blrb_flags:	.long	0x30000000
1:	.long	0x00008000	! FD
2:	.long	0xffff7f0f	! ~(IMASK+FD)
3:	.long	0x00080000	! SZ=0, PR=1
#if defined(CONFIG_KGDB_NMI)
4:	.long	SYMBOL_NAME(in_nmi)
#endif
5:	.long	0x00001000	! DSP


! Exception Vector Base
!
!	Should be aligned page boundary.
!
	.balign 	4096,0,4096
ENTRY(vbr_base)
	.long	0
!
	.balign 	256,0,256
general_exception:
	mov.l	1f, k2
	mov.l	2f, k3
	bra	handle_exception
	 mov.l	@k2, k2
	.align	2
2:	.long	ret_from_exception
1:	.long	EXPEVT
!
!
	.balign 	1024,0,1024
tlb_miss:
	mov.l	1f, k2
	mov.l	4f, k3
	bra	handle_exception
	 mov.l	@k2, k2
!
	.balign 	512,0,512
interrupt:
	mov.l	2f, k2
	mov.l	3f, k3
#if defined(CONFIG_KGDB_NMI)
	! Debounce (filter nested NMI)
	mov.l	@k2, k0
	mov.l	5f, k1
	cmp/eq	k1, k0
	bf	0f
	mov.l	6f, k1
	tas.b	@k1
	bt	0f
	rte
	 nop
	.align	2
5:	.long	NMI_VEC
6:	.long	SYMBOL_NAME(in_nmi)
0:
#endif /* defined(CONFIG_KGDB_NMI) */
	bra	handle_exception
	 mov.l	@k2, k2

	.align	2
1:	.long	EXPEVT
2:	.long	INTEVT
3:	.long	ret_from_irq
4:	.long	ret_from_exception

!
!
handle_exception:
	! Using k0, k1 for scratch registers (r0_bank1, r1_bank),
	! save all registers onto stack.
	!
	stc	ssr, k0		! Is it from kernel space?
	shll	k0		! Check MD bit (bit30) by shifting it into...
	shll	k0		!       ...the T bit
	bt/s	9f		! It's a kernel to kernel transition.
	 mov	r15, k0		! save original stack to k0
	/* User space to kernel */
	mov	#0x20, k1
	shll8	k1		! k1 <= 8192 == THREAD_SIZE
	add	current, k1
	mov	k1, r15		! change to kernel stack
	!
9:	mov.l	3f, k1
	!
#ifdef CONFIG_SH_DSP
	mov.l	r2, @-r15		! Save r2, we need another reg
	stc	sr, k4
	mov.l	4f, r2
	tst	r2, k4			! Check if in DSP mode
	mov.l	@r15+, r2		! Restore r2 now
	bt/s	skip_save
	 mov	#0, k4			! Set marker for no stack frame

	mov	r2, k4			! Backup r2 (in k4) for later

	! Save DSP registers on stack
	stc.l	mod, @-r15
	stc.l	re, @-r15
	stc.l	rs, @-r15
	sts.l	dsr, @-r15
	sts.l	y1, @-r15
	sts.l	y0, @-r15
	sts.l	x1, @-r15
	sts.l	x0, @-r15
	sts.l	a0, @-r15
	mov     r15, r2
	movs.l	a1, @-r2
	movs.l	a0g, @-r2
	movs.l	a1g, @-r2
	movs.l	m0, @-r2
	movs.l	m1, @-r2
	mov	r2, r15

	mov	k4, r2			! Restore r2
	mov.l	4f, k4			! Force DSP stack frame
skip_save:
	mov.l	k4, @-r15		! Push DSP mode marker onto stack
#endif

	! Save the user registers on the stack.
	mov.l	k2, @-r15	! EXPEVT
	!
	sts.l	macl, @-r15
	sts.l	mach, @-r15
	stc.l	gbr, @-r15
	stc.l	ssr, @-r15
	sts.l	pr, @-r15
	stc.l	spc, @-r15
	!
	lds	k3, pr		! Set the return address to pr
	!
	mov.l	k0, @-r15	! save orignal stack
	mov.l	r14, @-r15
	mov.l	r13, @-r15
	mov.l	r12, @-r15
	mov.l	r11, @-r15
	mov.l	r10, @-r15
	mov.l	r9, @-r15
	mov.l	r8, @-r15
	!
	stc	sr, r8		! Back to normal register bank, and
	or	k1, r8		! Block all interrupts
	mov.l	5f, k1
	and	k1, r8		! ...
	ldc	r8, sr		! ...changed here.
	!
	mov.l	r7, @-r15
	mov.l	r6, @-r15
	mov.l	r5, @-r15
	mov.l	r4, @-r15
	mov.l	r3, @-r15
	mov.l	r2, @-r15
	mov.l	r1, @-r15
	mov.l	r0, @-r15
	! Then, dispatch to the handler, according to the exception code.
	stc	k_ex_code, r8
	shlr2	r8
	shlr	r8
	mov.l	1f, r9
	add	r8, r9
	mov.l	@r9, r9
	jmp	@r9
	 nop
	.align	2
1:	.long	SYMBOL_NAME(exception_handling_table)
3:	.long	0x000000f0	! FD=0, IMASK=15
4:	.long	0x00001000	! DSP bit set
5:	.long	0xcfffffff	! RB=0, BL=0
6:	.long	0x00080000	! SZ=0, PR=1

none:
	rts
	 nop

.data
ENTRY(exception_handling_table)
	.long	error
	.long	error
	.long	tlb_miss_load
	.long	tlb_miss_store
	.long	initial_page_write
	.long	tlb_protection_violation_load
	.long	tlb_protection_violation_store
	.long	address_error_load
	.long	address_error_store
#if defined(__SH4__)
	.long	SYMBOL_NAME(do_fpu_error)
#else
	.long	error	! fpu_exception
#endif
	.long	error
	.long	system_call			! Unconditional Trap
	.long	error	! reserved_instruction      (filled by trap_init)
	.long	error	! illegal_slot_instruction  (filled by trap_init)
ENTRY(nmi_slot)
#if defined (CONFIG_KGDB_NMI)
	.long	debug_enter			! Allow trap to debugger
#else
	.long	none				! Not implemented yet
#endif
ENTRY(user_break_point_trap)
	.long	break_point_trap
ENTRY(interrupt_table)
	! external hardware
	.long	SYMBOL_NAME(do_IRQ)	! 0000
	.long	SYMBOL_NAME(do_IRQ)	! 0001
	.long	SYMBOL_NAME(do_IRQ)	! 0010
	.long	SYMBOL_NAME(do_IRQ)	! 0011
	.long	SYMBOL_NAME(do_IRQ)	! 0100
	.long	SYMBOL_NAME(do_IRQ)	! 0101
	.long	SYMBOL_NAME(do_IRQ)	! 0110
	.long	SYMBOL_NAME(do_IRQ)	! 0111
	.long	SYMBOL_NAME(do_IRQ)	! 1000
	.long	SYMBOL_NAME(do_IRQ)	! 1001
	.long	SYMBOL_NAME(do_IRQ)	! 1010
	.long	SYMBOL_NAME(do_IRQ)	! 1011
	.long	SYMBOL_NAME(do_IRQ)	! 1100
	.long	SYMBOL_NAME(do_IRQ)	! 1101
	.long	SYMBOL_NAME(do_IRQ)	! 1110
	.long	error
	! Internal hardware
	.long	SYMBOL_NAME(do_IRQ)	! TMU0 tuni0
	.long	SYMBOL_NAME(do_IRQ)	! TMU1 tuni1
	.long	SYMBOL_NAME(do_IRQ)	! TMU2 tuni2
	.long	SYMBOL_NAME(do_IRQ)	!      ticpi2
	.long	SYMBOL_NAME(do_IRQ)	! RTC  ati
	.long	SYMBOL_NAME(do_IRQ)	!      pri
	.long	SYMBOL_NAME(do_IRQ)	!      cui
	.long	SYMBOL_NAME(do_IRQ)	! SCI  eri
	.long	SYMBOL_NAME(do_IRQ)	!      rxi
	.long	SYMBOL_NAME(do_IRQ)	!      txi
	.long	SYMBOL_NAME(do_IRQ)	!      tei
	.long	SYMBOL_NAME(do_IRQ)	! WDT  iti
	.long	SYMBOL_NAME(do_IRQ)	! REF  rcmi
	.long	SYMBOL_NAME(do_IRQ)	!      rovi
	.long	SYMBOL_NAME(do_IRQ)
	.long	SYMBOL_NAME(do_IRQ)
#if defined(CONFIG_CPU_SUBTYPE_SH7300) || defined(CONFIG_CPU_SUBTYPE_SH7707) || \
    defined(CONFIG_CPU_SUBTYPE_SH7709)
	.long	SYMBOL_NAME(do_IRQ)	! 32 IRQ  irq0
	.long	SYMBOL_NAME(do_IRQ)	! 33      irq1
	.long	SYMBOL_NAME(do_IRQ)	! 34      irq2
	.long	SYMBOL_NAME(do_IRQ)	! 35      irq3
	.long	SYMBOL_NAME(do_IRQ)	! 36      irq4
	.long	SYMBOL_NAME(do_IRQ)	! 37      irq5
	.long	SYMBOL_NAME(do_IRQ)	! 38
	.long	SYMBOL_NAME(do_IRQ)	! 39
	.long	SYMBOL_NAME(do_IRQ)	! 40 PINT pint0-7
	.long	SYMBOL_NAME(do_IRQ)	! 41      pint8-15
	.long	SYMBOL_NAME(do_IRQ)	! 42
	.long	SYMBOL_NAME(do_IRQ)	! 43
	.long	SYMBOL_NAME(do_IRQ)	! 44
	.long	SYMBOL_NAME(do_IRQ)	! 45
	.long	SYMBOL_NAME(do_IRQ)	! 46
	.long	SYMBOL_NAME(do_IRQ)	! 47
	.long	SYMBOL_NAME(do_IRQ)	! 48 DMAC dei0
	.long	SYMBOL_NAME(do_IRQ)	! 49      dei1
	.long	SYMBOL_NAME(do_IRQ)	! 50      dei2
	.long	SYMBOL_NAME(do_IRQ)	! 51      dei3
	.long	SYMBOL_NAME(do_IRQ)	! 52 IrDA eri1
	.long	SYMBOL_NAME(do_IRQ)	! 53      rxi1
	.long	SYMBOL_NAME(do_IRQ)	! 54      bri1
	.long	SYMBOL_NAME(do_IRQ)	! 55      txi1
	.long	SYMBOL_NAME(do_IRQ)	! 56 SCIF eri2
	.long	SYMBOL_NAME(do_IRQ)	! 57      rxi2
	.long	SYMBOL_NAME(do_IRQ)	! 58      bri2
	.long	SYMBOL_NAME(do_IRQ)	! 59      txi2
	.long	SYMBOL_NAME(do_IRQ)	! 60 ADC  adi
#if defined(CONFIG_CPU_SUBTYPE_SH7300) || defined(CONFIG_CPU_SUBTYPE_SH7707)
	.long   SYMBOL_NAME(do_IRQ)	! 61 LCDC lcdi
	.long   SYMBOL_NAME(do_IRQ)	! 62 PCC  pcc0i
	.long   SYMBOL_NAME(do_IRQ)	! 63      pcc1i
#if defined(CONFIG_CPU_SUBTYPE_SH7300)
	.long   SYMBOL_NAME(do_IRQ)	! 64
	.long   SYMBOL_NAME(do_IRQ)	! 65
	.long   SYMBOL_NAME(do_IRQ)	! 66
	.long   SYMBOL_NAME(do_IRQ)	! 67
	.long   SYMBOL_NAME(do_IRQ)	! 68
	.long   SYMBOL_NAME(do_IRQ)	! 69
	.long   SYMBOL_NAME(do_IRQ)	! 70
	.long   SYMBOL_NAME(do_IRQ)	! 71
	.long   SYMBOL_NAME(do_IRQ)	! 72
	.long   SYMBOL_NAME(do_IRQ)	! 73
	.long   SYMBOL_NAME(do_IRQ)	! 74
	.long   SYMBOL_NAME(do_IRQ)	! 75
	.long   SYMBOL_NAME(do_IRQ)	! 76
	.long   SYMBOL_NAME(do_IRQ)	! 77
	.long   SYMBOL_NAME(do_IRQ)	! 78
	.long   SYMBOL_NAME(do_IRQ)	! 79
	.long   SYMBOL_NAME(do_IRQ)	! 80 SCIF0(SH7300)
	.long   SYMBOL_NAME(do_IRQ)	! 81
	.long   SYMBOL_NAME(do_IRQ)	! 82
	.long   SYMBOL_NAME(do_IRQ)	! 83
	.long   SYMBOL_NAME(do_IRQ)	! 84
	.long   SYMBOL_NAME(do_IRQ)	! 85
	.long   SYMBOL_NAME(do_IRQ)	! 86
	.long   SYMBOL_NAME(do_IRQ)	! 87
	.long   SYMBOL_NAME(do_IRQ)	! 88
	.long   SYMBOL_NAME(do_IRQ)	! 89
	.long   SYMBOL_NAME(do_IRQ)	! 90
	.long   SYMBOL_NAME(do_IRQ)	! 91
	.long   SYMBOL_NAME(do_IRQ)	! 92
	.long   SYMBOL_NAME(do_IRQ)	! 93
	.long   SYMBOL_NAME(do_IRQ)	! 94
	.long   SYMBOL_NAME(do_IRQ)	! 95
	.long   SYMBOL_NAME(do_IRQ)	! 96
	.long   SYMBOL_NAME(do_IRQ)	! 97
	.long   SYMBOL_NAME(do_IRQ)	! 98
	.long   SYMBOL_NAME(do_IRQ)	! 99
	.long   SYMBOL_NAME(do_IRQ)	! 100
	.long   SYMBOL_NAME(do_IRQ)	! 101
	.long   SYMBOL_NAME(do_IRQ)	! 102
	.long   SYMBOL_NAME(do_IRQ)	! 103
	.long   SYMBOL_NAME(do_IRQ)	! 104
	.long   SYMBOL_NAME(do_IRQ)	! 105
	.long   SYMBOL_NAME(do_IRQ)	! 106
	.long   SYMBOL_NAME(do_IRQ)	! 107
	.long   SYMBOL_NAME(do_IRQ)	! 108
#endif
#endif
#elif defined(__SH4__)
	.long	SYMBOL_NAME(do_IRQ)	! 32 Hitachi UDI
	.long	SYMBOL_NAME(do_IRQ)	! 33 GPIO
	.long	SYMBOL_NAME(do_IRQ)	! 34 DMAC dmte0
	.long	SYMBOL_NAME(do_IRQ)	! 35      dmte1
	.long	SYMBOL_NAME(do_IRQ)	! 36      dmte2
	.long	SYMBOL_NAME(do_IRQ)	! 37      dmte3
	.long	SYMBOL_NAME(do_IRQ)	! 38      dmae
	.long	error			! 39
	.long	SYMBOL_NAME(do_IRQ)	! 40 SCIF eri
	.long	SYMBOL_NAME(do_IRQ)	! 41      rxi
	.long	SYMBOL_NAME(do_IRQ)	! 42      bri
	.long	SYMBOL_NAME(do_IRQ)	! 43      txi
	.long	error			! 44
	.long	error			! 45
	.long	error			! 46
	.long	error			! 47
	.long	SYMBOL_NAME(do_fpu_state_restore)	! 48
	.long	SYMBOL_NAME(do_fpu_state_restore)	! 49
#endif
#if defined(CONFIG_CPU_SUBTYPE_SH7751)
	.long	error
	.long	error
	.long	error
	.long	error
	.long	error
	.long	error
	.long	error
	.long	error
	.long	error
	.long	error
	.long	error
	.long	error
	.long	error
	.long	error
	.long	SYMBOL_NAME(do_IRQ)	! PCI serr
	.long	SYMBOL_NAME(do_IRQ)	!     dma3
	.long	SYMBOL_NAME(do_IRQ)	!     dma2
	.long	SYMBOL_NAME(do_IRQ)	!     dma1
	.long	SYMBOL_NAME(do_IRQ)	!     dma0
	.long	SYMBOL_NAME(do_IRQ)	!     pwon
	.long	SYMBOL_NAME(do_IRQ)	!     pwdwn
	.long	SYMBOL_NAME(do_IRQ)	!     err
#elif defined(CONFIG_CPU_SUBTYPE_ST40)
	.long	error			!  50 0x840
	.long	error			!  51 0x860
	.long	error			!  52 0x880
	.long	error			!  53 0x8a0
	.long	error			!  54 0x8c0
	.long	error			!  55 0x8e0
	.long	error			!  56 0x900
	.long	error			!  57 0x920
	.long	error			!  58 0x940
	.long	error			!  59 0x960
	.long	error			!  60 0x980
	.long	error			!  61 0x9a0
	.long	error			!  62 0x9c0
	.long	error			!  63 0x9e0
	.long	SYMBOL_NAME(do_IRQ)	!  64 0xa00 PCI serr
	.long	SYMBOL_NAME(do_IRQ)	!  65 0xa20     err
	.long	SYMBOL_NAME(do_IRQ)	!  66 0xa40     ad
	.long	SYMBOL_NAME(do_IRQ)	!  67 0xa60     pwr_dwn
	.long	error			!  68 0xa80
	.long	error			!  69 0xaa0
	.long	error			!  70 0xac0
	.long	error			!  71 0xae0
	.long	SYMBOL_NAME(do_IRQ)	!  72 0xb00 DMA INT0
	.long	SYMBOL_NAME(do_IRQ)	!  73 0xb20     INT1
	.long	SYMBOL_NAME(do_IRQ)	!  74 0xb40     INT2
	.long	SYMBOL_NAME(do_IRQ)	!  75 0xb60     INT3
	.long	SYMBOL_NAME(do_IRQ)	!  76 0xb80     INT4
	.long	error			!  77 0xba0
	.long	SYMBOL_NAME(do_IRQ)	!  78 0xbc0 DMA ERR
	.long	error			!  79 0xbe0
	.long	SYMBOL_NAME(do_IRQ)	!  80 0xc00 PIO0
	.long	error			!  81 0xc20
	.long	error			!  82 0xc40
	.long	error			!  83 0xc60
	.long	SYMBOL_NAME(do_IRQ)	!  84 0xc80 PIO1
	.long	error			!  85 0xca0
	.long	error			!  86 0xcc0
	.long	error			!  87 0xce0
	.long	SYMBOL_NAME(do_IRQ)	!  88 0xd00 PIO2
	.long	error			!  89 0xd20
	.long	error			!  90 0xd40
	.long	error			!  91 0xd60
	.long	error			!  92 0xd80
	.long	error			!  93 0xda0
	.long	error			!  94 0xdc0
	.long	error			!  95 0xde0
	.long	error			!  96 0xe00
	.long	error			!  97 0xe20
	.long	error			!  98 0xe40
	.long	error			!  99 0xe60
	.long	error			! 100 0xe80
	.long	error			! 101 0xea0
	.long	error			! 102 0xec0
	.long	error			! 103 0xee0
	.long	error			! 104 0xf00
	.long	error			! 105 0xf20
	.long	error			! 106 0xf40
	.long	error			! 107 0xf60
	.long	error			! 108 0xf80
	.long	error			! 109 0xfa0
	.long	error			! 110 0xfc0
	.long	error			! 111 0xfe0
# if defined(CONFIG_CPU_SUBTYPE_ST40STB1)
	.long	SYMBOL_NAME(do_IRQ)	! 112 0x1000 Mailbox
	.long	error			! 113 0x1020
	.long	error			! 114 0x1040
	.long	error			! 115 0x1060
	.long	error			! 116 0x1080
	.long	error			! 117 0x10a0
	.long	error			! 118 0x10c0
	.long	error			! 119 0x10e0
	.long	error			! 120 0x1100
	.long	error			! 121 0x1120
	.long	error			! 122 0x1140
	.long	error			! 123 0x1160
	.long	error			! 124 0x1180
	.long	error			! 125 0x11a0
	.long	error			! 126 0x11c0
	.long	error			! 127 0x11e0
	.long	error			! 128 0x1200
	.long	error			! 129 0x1220
	.long	error			! 130 0x1240
	.long	error			! 131 0x1260
	.long	error			! 132 0x1280
	.long	error			! 133 0x12a0
	.long	error			! 134 0x12c0
	.long	error			! 135 0x12e0
	.long	error			! 136 0x1300
	.long	error			! 137 0x1320
	.long	error			! 138 0x1340
	.long	error			! 139 0x1360
	.long	SYMBOL_NAME(do_IRQ)	! 140 0x1380 EMPI INV_ADDR
	.long	error			! 141 0x13a0
	.long	error			! 142 0x13c0
	.long	error			! 143 0x13e0
# elif defined(CONFIG_CPU_SUBTYPE_ST40GX1)
	.long	SYMBOL_NAME(do_IRQ)	! 112 0x1000 Mailbox
	.long	error			! 113 0x1020
	.long	error			! 114 0x1040
	.long	error			! 115 0x1060
	.long	SYMBOL_NAME(do_IRQ)	! 116 0x1080 SSC0
	.long	error			! 117 0x10a0
	.long	error			! 118 0x10c0
	.long	error			! 119 0x10e0
	.long	SYMBOL_NAME(do_IRQ)	! 120 0x1100 IRBlaster IRB
	.long	error			! 121 0x1120
	.long	error			! 122 0x1140
	.long	error			! 123 0x1160
	.long	SYMBOL_NAME(do_IRQ)	! 124 0x1180 USB host
	.long	error			! 125 0x11a0
	.long	error			! 126 0x11c0
	.long	error			! 127 0x11e0
	.long	SYMBOL_NAME(do_IRQ)	! 128 0x1200 Video Processor BLITER
	.long	error			! 129 0x1220
	.long	error			! 130 0x1240
	.long	error			! 131 0x1260
	.long	SYMBOL_NAME(do_IRQ)	! 132 0x1280 UART0 UART0
	.long	error			! 133 0x12a0
	.long	SYMBOL_NAME(do_IRQ)	! 134 0x12c0       UART2
	.long	error			! 135 0x12e0
	.long	SYMBOL_NAME(do_IRQ)	! 136 0x1300 Additional PIO IO_PIO0
	.long	error			! 137 0x1320
	.long	error			! 138 0x1340
	.long	error			! 139 0x1360
	.long	SYMBOL_NAME(do_IRQ)	! 140 0x1380 EMPI INV_ADDR
	.long	error			! 141 0x13a0
	.long	error			! 142 0x13c0
	.long	error			! 143 0x13e0
	.long	SYMBOL_NAME(do_IRQ)	! 144 0x1400 MAFE
	.long	error			! 145 0x1420
	.long	error			! 146 0x1440
	.long	error			! 147 0x1460
	.long	SYMBOL_NAME(do_IRQ)	! 148 0x1480 PWM
	.long	error			! 149 0x14a0
	.long	error			! 150 0x14c0
	.long	error			! 151 0x14e0
	.long	SYMBOL_NAME(do_IRQ)	! 152 0x1500 SSC1
	.long	error			! 153 0x1520
	.long	error			! 154 0x1540
	.long	error			! 155 0x1560
	.long	SYMBOL_NAME(do_IRQ)	! 156 0x1580 Additional PIO IO_PIO1
	.long	error			! 157 0x15a0
	.long	error			! 158 0x15c0
	.long	error			! 159 0x15e0
	.long	SYMBOL_NAME(do_IRQ)	! 160 0x1600 USB target
	.long	error			! 161 0x1620
	.long	error			! 162 0x1640
	.long	error			! 163 0x1660
	.long	SYMBOL_NAME(do_IRQ)	! 164 0x1680 UART1 UART1
	.long	error			! 165 0x16a0
	.long	error			! 166 0x16c0
	.long	error			! 167 0x16e0
	.long	SYMBOL_NAME(do_IRQ)	! 168 0x1700 Teletext TTXT
	.long	error			! 169 0x1720
	.long	error			! 170 0x1740
	.long	error			! 171 0x1760
	.long	SYMBOL_NAME(do_IRQ)	! 172 0x1780 Video Sync VTG
	.long	SYMBOL_NAME(do_IRQ)	! 173 0x17a0            DVP0
	.long	SYMBOL_NAME(do_IRQ)	! 174 0x17c0            DVP1
	.long	error			! 175 0x17e0
# else
#  error Unknown ST40 type
# endif
#endif

ENTRY(sys_call_table)
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
	.long SYMBOL_NAME(sys_ni_syscall)	/* 35 */		/* old ftime syscall holder */
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
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_olduname */
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
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_oldselect */
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
	.long SYMBOL_NAME(sys_ni_syscall)	/* ioperm */
	.long SYMBOL_NAME(sys_socketcall)
	.long SYMBOL_NAME(sys_syslog)
	.long SYMBOL_NAME(sys_setitimer)
	.long SYMBOL_NAME(sys_getitimer)	/* 105 */
	.long SYMBOL_NAME(sys_newstat)
	.long SYMBOL_NAME(sys_newlstat)
	.long SYMBOL_NAME(sys_newfstat)
	.long SYMBOL_NAME(sys_ni_syscall)	/* old uname */
	.long SYMBOL_NAME(sys_ni_syscall)	/* 110 */ /* iopl */
	.long SYMBOL_NAME(sys_vhangup)
	.long SYMBOL_NAME(sys_ni_syscall)	/* idle */
	.long SYMBOL_NAME(sys_ni_syscall)	/* vm86old */
	.long SYMBOL_NAME(sys_wait4)
	.long SYMBOL_NAME(sys_swapoff)		/* 115 */
	.long SYMBOL_NAME(sys_sysinfo)
	.long SYMBOL_NAME(sys_ipc)
	.long SYMBOL_NAME(sys_fsync)
	.long SYMBOL_NAME(sys_sigreturn)
	.long SYMBOL_NAME(sys_clone)		/* 120 */
	.long SYMBOL_NAME(sys_setdomainname)
	.long SYMBOL_NAME(sys_newuname)
	.long SYMBOL_NAME(sys_cacheflush)	/* sys_modify_ldt for i386 */
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
	.long SYMBOL_NAME(sys_ni_syscall)	/* vm86 */
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
	.long SYMBOL_NAME(sys_pread_wrapper)	/* 180 */
	.long SYMBOL_NAME(sys_pwrite_wrapper)
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
	.long SYMBOL_NAME(sys_getdents64)	/* 220 */
	.long SYMBOL_NAME(sys_fcntl64)
	.long SYMBOL_NAME(sys_gettid)
 	.long SYMBOL_NAME(sys_tkill)
	.long SYMBOL_NAME(sys_ni_syscall)
	.long SYMBOL_NAME(sys_readahead)	/* 225 */
	/*
	 * NOTE!! This doesn't have to be exact - we just have
	 * to make sure we have _enough_ of the "sys_ni_syscall"
	 * entries. Don't panic if you notice that this hasn't
	 * been shrunk every time we add a new system call.
	 */
	.rept NR_syscalls-(.-sys_call_table)/4
		.long SYMBOL_NAME(sys_ni_syscall)
	.endr

/* End of entry.S */
