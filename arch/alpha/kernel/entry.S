/*
 * alpha/entry.S
 *
 * kernel entry-points
 */

#include <linux/config.h>
#include <asm/system.h>
#include <asm/cache.h>

#define SIGCHLD 20

#define NR_SYSCALLS 382

/*
 * These offsets must match with alpha_mv in <asm/machvec.h>.
 */
#define HAE_CACHE	0
#define HAE_REG		8

/*
 * stack offsets
 */
#define SP_OFF		184

#define SWITCH_STACK_SIZE 320

/*
 * task structure offsets
 */
#define TASK_STATE		0
#define TASK_FLAGS		8
#define TASK_SIGPENDING		16
#define TASK_ADDR_LIMIT		24
#define TASK_EXEC_DOMAIN	32
#define TASK_NEED_RESCHED	40
#define TASK_PTRACE		48
#define TASK_PROCESSOR		100

/*
 * task flags (must match include/linux/sched.h):
 */
#define	PT_PTRACED	0x00000001

#define CLONE_VM        0x00000100 

/*
 * This defines the normal kernel pt-regs layout.
 *
 * regs 9-15 preserved by C code
 * regs 16-18 saved by PAL-code
 * regs 29-30 saved and set up by PAL-code
 * JRP - Save regs 16-18 in a special area of the stack, so that
 * the palcode-provided values are available to the signal handler.
 */

#define SAVE_ALL			\
	subq	$30,184,$30;		\
	stq	$0,0($30);		\
	stq	$1,8($30);		\
	stq	$2,16($30);		\
	stq	$3,24($30);		\
	stq	$4,32($30);		\
	stq	$28,144($30);		\
	lda	$2,alpha_mv;		\
	stq	$5,40($30);		\
	stq	$6,48($30);		\
	stq	$7,56($30);		\
	stq	$8,64($30);		\
	stq	$19,72($30);		\
	stq	$20,80($30);		\
	stq	$21,88($30);		\
	ldq	$2,HAE_CACHE($2);	\
	stq	$22,96($30);		\
	stq	$23,104($30);		\
	stq	$24,112($30);		\
	stq	$25,120($30);		\
	stq	$26,128($30);		\
	stq	$27,136($30);		\
	stq	$2,152($30);		\
	stq	$16,160($30);		\
	stq	$17,168($30);		\
	stq	$18,176($30)

#define RESTORE_ALL			\
	lda	$19,alpha_mv;		\
	ldq	$0,0($30);		\
	ldq	$1,8($30);		\
	ldq	$2,16($30);		\
	ldq	$3,24($30);		\
	ldq	$21,152($30);		\
	ldq	$20,HAE_CACHE($19);	\
	ldq	$4,32($30);		\
	ldq	$5,40($30);		\
	ldq	$6,48($30);		\
	ldq	$7,56($30);		\
	subq	$20,$21,$20;		\
	ldq	$8,64($30);		\
	beq	$20,99f;		\
	ldq	$20,HAE_REG($19);	\
	addq	$31,7,$16;		\
	call_pal PAL_swpipl;		\
	stq	$21,HAE_CACHE($19);	\
	stq	$21,0($20);		\
	mov	$0,$16;			\
	call_pal PAL_swpipl;		\
	ldq	$0,0($30);		\
	ldq	$1,8($30);		\
99:;					\
	ldq	$19,72($30);		\
	ldq	$20,80($30);		\
	ldq	$21,88($30);		\
	ldq	$22,96($30);		\
	ldq	$23,104($30);		\
	ldq	$24,112($30);		\
	ldq	$25,120($30);		\
	ldq	$26,128($30);		\
	ldq	$27,136($30);		\
	ldq	$28,144($30);		\
	addq	$30,184,$30

.text
.set noat
#if defined(__linux__) && !defined(__ELF__)
  .set singlegp
#endif

.align 3
.globl	entInt
.ent	entInt
entInt:
	SAVE_ALL
	lda	$8,0x3fff
	lda	$26,ret_from_sys_call
	bic	$30,$8,$8
	jsr	$31,do_entInt
.end entInt

.align 3
.globl	entMM
.ent	entMM
entMM:
	SAVE_ALL
/* save $9 - $15 so the inline exception code can manipulate them.  */
	subq	$30,56,$30
	stq	$9,0($30)
	stq	$10,8($30)
	stq	$11,16($30)
	stq	$12,24($30)
	stq	$13,32($30)
	stq	$14,40($30)
	stq	$15,48($30)
	addq	$30,56,$19
/* handle the fault */
	lda	$8,0x3fff
	bic	$30,$8,$8
	jsr	$26,do_page_fault
/* reload the registers after the exception code played.  */
	ldq	$9,0($30)
	ldq	$10,8($30)
	ldq	$11,16($30)
	ldq	$12,24($30)
	ldq	$13,32($30)
	ldq	$14,40($30)
	ldq	$15,48($30)
	addq	$30,56,$30
/* finish up the syscall as normal.  */
	br	ret_from_sys_call
.end entMM

.align 3
.globl	entArith
.ent	entArith
entArith:
	SAVE_ALL
	lda	$8,0x3fff
	lda	$26,ret_from_sys_call
	bic	$30,$8,$8
	jsr	$31,do_entArith
.end entArith

.align 3
.globl	entIF
.ent	entIF
entIF:
	SAVE_ALL
	lda	$8,0x3fff
	lda	$26,ret_from_sys_call
	bic	$30,$8,$8
	jsr	$31,do_entIF
.end entIF

.align 3
.globl	entDbg
.ent	entDbg
entDbg:
	SAVE_ALL
	lda	$8,0x3fff
	lda	$26,ret_from_sys_call
	bic	$30,$8,$8
	jsr	$31,do_entDbg
.end entDbg


/*
 * Fork() is one of the special system calls: it needs to
 * save the callee-saved regs so that the regs can be found
 * for the new process.. We save them in the "context switch"
 * stack format (see arch/alpha/kernel/process.c).
 *
 * Also, for the kernel fork, we need to fake the system call
 * stack buildup, as we can't do system calls from kernel space.
 */
.align 3
.ent	kernel_clone
kernel_clone:
	.frame $30, 0, $26
	.prologue 0
	subq	$30,6*8,$30
	stq	$31,0($30)
	stq	$26,8($30)
	stq	$29,16($30)
	stq	$16,24($30)
	stq	$17,32($30)
	stq	$18,40($30)
	bis	$31,2,$0	/* Register v0: syscall nr for fork() */
	SAVE_ALL
	bsr	$26,sys_clone
	stq	$0,0($30)
	br	ret_from_sys_call
.end	kernel_clone

/*
 * arch_kernel_thread(fn, arg, clone_flags)
 */
.align 3
.globl	arch_kernel_thread
.ent	arch_kernel_thread
arch_kernel_thread:
	ldgp	$29,0($27)	/* we can be called from a module */
	.frame $30, 4*8, $26
	subq	$30,4*8,$30
	stq	$10,16($30)
	stq	$9,8($30)
	lda	$0,CLONE_VM
	stq	$26,0($30)
	.prologue 1
	mov	$16,$9		/* save fn */		
	mov	$17,$10		/* save arg */
	or	$18,$0,$16	/* shuffle flags to front; add CLONE_VM.  */
	bsr	$26,kernel_clone
	bne	$20,1f		/* $20 is non-zero in child */
	ldq	$26,0($30)
	ldq	$9,8($30)
	ldq	$10,16($30)
	addq	$30,4*8,$30
	ret	$31,($26),1
/* this is in child: look out as we don't have any stack here.. */
1:	mov	$9,$27		/* get fn */
	lda	$8,0x3fff
	mov	$10,$16		/* get arg */
	bic	$30,$8,$8	/* get current */
	jsr	$26,($27)
	ldgp	$29,0($26)
	mov	$0,$16
	mov	$31,$26
	jsr	$31,sys_exit
.end	arch_kernel_thread

/*
 * __kernel_execve(path, argv, envp, regs)
 */
.align 3
.globl	__kernel_execve
.ent	__kernel_execve
__kernel_execve:
	ldgp	$29,0($27)	/* we can be called from modules.  */
	subq	$30,16,$30
	.frame	$30,16,$26,0
	stq	$26,0($30)
	stq	$19,8($30)
	.prologue 1
	jsr	$26,do_execve
	bne	$0,1f		/* error! */
	ldq	$30,8($30)
	br	$31,ret_from_sys_call
1:	ldq	$26,0($30)
	addq	$30,16,$30
	ret
.end	__kernel_execve

.align 3
.ent	do_switch_stack
do_switch_stack:
	lda	$30,-SWITCH_STACK_SIZE($30)
	stq	$9,0($30)
	stq	$10,8($30)
	stq	$11,16($30)
	stq	$12,24($30)
	stq	$13,32($30)
	stq	$14,40($30)
	stq	$15,48($30)
	stq	$26,56($30)
	stt	$f0,64($30)
	stt	$f1,72($30)
	stt	$f2,80($30)
	stt	$f3,88($30)
	stt	$f4,96($30)
	stt	$f5,104($30)
	stt	$f6,112($30)
	stt	$f7,120($30)
	stt	$f8,128($30)
	stt	$f9,136($30)
	stt	$f10,144($30)
	stt	$f11,152($30)
	stt	$f12,160($30)
	stt	$f13,168($30)
	stt	$f14,176($30)
	stt	$f15,184($30)
	stt	$f16,192($30)
	stt	$f17,200($30)
	stt	$f18,208($30)
	stt	$f19,216($30)
	stt	$f20,224($30)
	stt	$f21,232($30)
	stt	$f22,240($30)
	stt	$f23,248($30)
	stt	$f24,256($30)
	stt	$f25,264($30)
	stt	$f26,272($30)
	stt	$f27,280($30)
	mf_fpcr	$f0		# get fpcr
	stt	$f28,288($30)
	stt	$f29,296($30)
	stt	$f30,304($30)
	stt	$f0,312($30)	# save fpcr in slot of $f31
	ldt	$f0,64($30)	# dont let "do_switch_stack" change fp state.
	ret	$31,($1),1
.end do_switch_stack

.align 3
.ent	undo_switch_stack
undo_switch_stack:
	ldq	$9,0($30)
	ldq	$10,8($30)
	ldq	$11,16($30)
	ldq	$12,24($30)
	ldq	$13,32($30)
	ldq	$14,40($30)
	ldq	$15,48($30)
	ldq	$26,56($30)
	ldt	$f30,312($30)	# get saved fpcr
	ldt	$f0,64($30)
	ldt	$f1,72($30)
	ldt	$f2,80($30)
	ldt	$f3,88($30)
	mt_fpcr	$f30		# install saved fpcr
	ldt	$f4,96($30)
	ldt	$f5,104($30)
	ldt	$f6,112($30)
	ldt	$f7,120($30)
	ldt	$f8,128($30)
	ldt	$f9,136($30)
	ldt	$f10,144($30)
	ldt	$f11,152($30)
	ldt	$f12,160($30)
	ldt	$f13,168($30)
	ldt	$f14,176($30)
	ldt	$f15,184($30)
	ldt	$f16,192($30)
	ldt	$f17,200($30)
	ldt	$f18,208($30)
	ldt	$f19,216($30)
	ldt	$f20,224($30)
	ldt	$f21,232($30)
	ldt	$f22,240($30)
	ldt	$f23,248($30)
	ldt	$f24,256($30)
	ldt	$f25,264($30)
	ldt	$f26,272($30)
	ldt	$f27,280($30)
	ldt	$f28,288($30)
	ldt	$f29,296($30)
	ldt	$f30,304($30)
	lda	$30,SWITCH_STACK_SIZE($30)
	ret	$31,($1),1
.end undo_switch_stack

.align 3
.globl	entUna
.ent	entUna
entUna:
	lda	$30,-256($30)
	stq	$0,0($30)
	ldq	$0,256($30)	/* get PS */
	stq	$1,8($30)
	stq	$2,16($30)
	stq	$3,24($30)
	and	$0,8,$0		/* user mode? */
	stq	$4,32($30)
	bne	$0,entUnaUser	/* yup -> do user-level unaligned fault */
	stq	$5,40($30)
	stq	$6,48($30)
	stq	$7,56($30)
	stq	$8,64($30)
	stq	$9,72($30)
	stq	$10,80($30)
	stq	$11,88($30)
	stq	$12,96($30)
	stq	$13,104($30)
	stq	$14,112($30)
	stq	$15,120($30)
	/* 16-18 PAL-saved */
	stq	$19,152($30)
	stq	$20,160($30)
	stq	$21,168($30)
	stq	$22,176($30)
	stq	$23,184($30)
	stq	$24,192($30)
	stq	$25,200($30)
	stq	$26,208($30)
	stq	$27,216($30)
	stq	$28,224($30)
	stq	$29,232($30)
	lda	$8,0x3fff
	stq	$31,248($30)
	bic	$30,$8,$8
	jsr	$26,do_entUna
	ldq	$0,0($30)
	ldq	$1,8($30)
	ldq	$2,16($30)
	ldq	$3,24($30)
	ldq	$4,32($30)
	ldq	$5,40($30)
	ldq	$6,48($30)
	ldq	$7,56($30)
	ldq	$8,64($30)
	ldq	$9,72($30)
	ldq	$10,80($30)
	ldq	$11,88($30)
	ldq	$12,96($30)
	ldq	$13,104($30)
	ldq	$14,112($30)
	ldq	$15,120($30)
	/* 16-18 PAL-saved */
	ldq	$19,152($30)
	ldq	$20,160($30)
	ldq	$21,168($30)
	ldq	$22,176($30)
	ldq	$23,184($30)
	ldq	$24,192($30)
	ldq	$25,200($30)
	ldq	$26,208($30)
	ldq	$27,216($30)
	ldq	$28,224($30)
	ldq	$29,232($30)
	lda	$30,256($30)
	call_pal PAL_rti
.end entUna

.align 3
.ent	entUnaUser
entUnaUser:
	ldq	$0,0($30)	/* restore original $0 */
	lda	$30,256($30)	/* pop entUna's stack frame */
	SAVE_ALL		/* setup normal kernel stack */
	lda	$30,-56($30)
	stq	$9,0($30)
	stq	$10,8($30)
	stq	$11,16($30)
	stq	$12,24($30)
	stq	$13,32($30)
	stq	$14,40($30)
	stq	$15,48($30)
	lda	$8,0x3fff
	addq	$30,56,$19
	bic	$30,$8,$8
	jsr	$26,do_entUnaUser
	ldq	$9,0($30)
	ldq	$10,8($30)
	ldq	$11,16($30)
	ldq	$12,24($30)
	ldq	$13,32($30)
	ldq	$14,40($30)
	ldq	$15,48($30)
	lda	$30,56($30)
	br	ret_from_sys_call
.end	entUnaUser

/*
 * A fork is the same as clone(SIGCHLD, 0);
 */
.align 3
.globl	sys_fork
.ent	sys_fork
sys_fork:
	bsr	$1,do_switch_stack
	bis	$31,SIGCHLD,$16
	mov	$31,$17
	mov	$30,$18
	jsr	$26,alpha_clone
	bsr	$1,undo_switch_stack
	ret	$31,($26),1
.end	sys_fork

.align 3
.globl	sys_clone
.ent	sys_clone
sys_clone:
	bsr	$1,do_switch_stack
	/* arg1 and arg2 come from the user */
	mov	$30,$18
	jsr	$26,alpha_clone
	bsr	$1,undo_switch_stack
	ret	$31,($26),1
.end	sys_clone

.align 3
.globl	sys_vfork
.ent	sys_vfork
sys_vfork:
	bsr	$1,do_switch_stack
	mov	$30,$16
	jsr	$26,alpha_vfork
	bsr	$1,undo_switch_stack
	ret	$31,($26),1
.end	sys_vfork

.align 3
.globl  alpha_switch_to
.ent    alpha_switch_to
alpha_switch_to:
	.prologue 0
	bsr	$1,do_switch_stack
	call_pal PAL_swpctx
	unop
	bsr	$1,undo_switch_stack
	mov	$17,$0
	ret	$31,($26),1
.end alpha_switch_to

/*
 * Oh, well.. Disassembling OSF/1 binaries to find out how the
 * system calls work isn't much fun.
 *
 * entSys is special in that the PAL-code doesn't save a0-a2, so
 * we start off by doing that by hand.
 */
.align 3
.globl	entSys
.globl	ret_from_sys_call
.ent	entSys
entSys:
	SAVE_ALL
	lda	$8,0x3fff
	bic	$30,$8,$8
	lda	$4,NR_SYSCALLS($31)
	stq	$16,SP_OFF+24($30)
	lda	$5,sys_call_table
	lda	$27,sys_ni_syscall
	cmpult	$0,$4,$4
	ldq	$3,TASK_PTRACE($8)
	stq	$17,SP_OFF+32($30)
	s8addq	$0,$5,$5
	and     $3,PT_PTRACED,$3
	stq	$18,SP_OFF+40($30)
	bne     $3,strace
	beq	$4,1f
	ldq	$27,0($5)
1:	jsr	$26,($27),alpha_ni_syscall
	ldgp	$29,0($26)
	blt	$0,syscall_error	/* the call failed */
	stq	$0,0($30)
	stq	$31,72($30)		/* a3=0 => no error */

.align 3
ret_from_sys_call:
	cmovne	$26,0,$19		/* $19 = 0 => non-restartable */
	ldq	$0,SP_OFF($30)
	and	$0,8,$0
	beq	$0,restore_all
ret_from_reschedule:
	ldq	$2,TASK_NEED_RESCHED($8)
	lda	$4,init_task_union
	bne	$2,reschedule
	xor	$4,$8,$4
	ldl	$5,TASK_SIGPENDING($8)
	beq	$4,restore_all
	bne	$5,signal_return
restore_all:
	RESTORE_ALL
	call_pal PAL_rti


/* PTRACE syscall handler */
.align 3
strace:
	/* set up signal stack, call syscall_trace */
	bsr	$1,do_switch_stack
	jsr     $26,syscall_trace
	bsr	$1,undo_switch_stack

	/* get the system call number and the arguments back.. */
	ldq     $0,0($30)
	ldq     $16,SP_OFF+24($30)
	ldq     $17,SP_OFF+32($30)
	ldq     $18,SP_OFF+40($30)
	ldq     $19,72($30)
	ldq     $20,80($30)
	ldq     $21,88($30)

	/* get the system call pointer.. */
	lda	$1,NR_SYSCALLS($31)
	lda	$2,sys_call_table
	lda	$27,alpha_ni_syscall
	cmpult	$0,$1,$1
	s8addq	$0,$2,$2
	beq	$1,1f
	ldq	$27,0($2)
1:	jsr	$26,($27),sys_gettimeofday
	ldgp	$29,0($26)

	/* check return.. */
	blt	$0,strace_error	/* the call failed */
	stq	$31,72($30)		/* a3=0 => no error */
strace_success:
	stq	$0,0($30)		/* save return value */

	bsr	$1,do_switch_stack
	jsr     $26,syscall_trace
	bsr	$1,undo_switch_stack
	br	$31,ret_from_sys_call

	.align  3
strace_error:
	ldq	$19,0($30)	/* old syscall nr (zero if success) */
	beq	$19,strace_success
	ldq	$20,72($30)	/* .. and this a3 */

	subq	$31,$0,$0	/* with error in v0 */
	addq	$31,1,$1	/* set a3 for errno return */
	stq	$0,0($30)
	stq	$1,72($30)	/* a3 for return */

	bsr	$1,do_switch_stack
	mov	$19,$9		/* save old syscall number */
	mov	$20,$10		/* save old a3 */
	jsr     $26,syscall_trace
	mov	$9,$19
	mov	$10,$20
	bsr	$1,undo_switch_stack

	mov	$31,$26		/* tell "ret_from_sys_call" we can restart */
	br	ret_from_sys_call

	.align 3
syscall_error:
	/*
	 * Some system calls (e.g., ptrace) can return arbitrary
	 * values which might normally be mistaken as error numbers.
	 * Those functions must zero $0 (v0) directly in the stack
	 * frame to indicate that a negative return value wasn't an
	 * error number..
	 */
	ldq	$19,0($30)	/* old syscall nr (zero if success) */
	beq	$19,ret_success

	ldq	$20,72($30)	/* .. and this a3 */
	subq	$31,$0,$0	/* with error in v0 */
	addq	$31,1,$1	/* set a3 for errno return */
	stq	$0,0($30)
	mov	$31,$26		/* tell "ret_from_sys_call" we can restart */
	stq	$1,72($30)	/* a3 for return */
	br	ret_from_sys_call

ret_success:
	stq	$0,0($30)
	stq	$31,72($30)	/* a3=0 => no error */
	br	ret_from_sys_call

.align 3
signal_return:
	mov	$30,$17
	br	$1,do_switch_stack
	mov	$30,$18
	mov	$31,$16
	jsr	$26,do_signal
	bsr	$1,undo_switch_stack
	br	restore_all
.end entSys

        .globl  ret_from_fork
.align 3
.ent ret_from_fork
ret_from_fork:
	lda	$26,ret_from_sys_call
	mov	$17,$16
	jsr	$31,schedule_tail
.end ret_from_fork

.align 3
.ent reschedule
reschedule:
	subq	$30,16,$30
	stq	$19,0($30)	/* save syscall nr */
	stq	$20,8($30)	/* and error indication (a3) */
	jsr	$26,schedule
	ldq	$19,0($30)
	ldq	$20,8($30)
	addq	$30,16,$30
	br	ret_from_reschedule
.end reschedule

.align 3
.ent sys_sigreturn
sys_sigreturn:
	mov	$30,$17
	lda	$18,-SWITCH_STACK_SIZE($30)
	lda	$30,-SWITCH_STACK_SIZE($30)
	jsr	$26,do_sigreturn
	br	$1,undo_switch_stack
	br	ret_from_sys_call
.end sys_sigreturn

.align 3
.ent sys_rt_sigreturn
sys_rt_sigreturn:
	mov	$30,$17
	lda	$18,-SWITCH_STACK_SIZE($30)
	lda	$30,-SWITCH_STACK_SIZE($30)
	jsr	$26,do_rt_sigreturn
	br	$1,undo_switch_stack
	br	ret_from_sys_call
.end sys_rt_sigreturn

.align 3
.ent sys_sigsuspend
sys_sigsuspend:
	mov	$30,$17
	br	$1,do_switch_stack
	mov	$30,$18
	subq	$30,16,$30
	stq	$26,0($30)
	jsr	$26,do_sigsuspend
	ldq	$26,0($30)
	lda	$30,SWITCH_STACK_SIZE+16($30)
	ret	$31,($26),1
.end sys_sigsuspend

.align 3
.ent sys_rt_sigsuspend
sys_rt_sigsuspend:
	mov	$30,$18
	br	$1,do_switch_stack
	mov	$30,$19
	subq	$30,16,$30
	stq	$26,0($30)
	jsr	$26,do_rt_sigsuspend
	ldq	$26,0($30)
	lda	$30,SWITCH_STACK_SIZE+16($30)
	ret	$31,($26),1
.end sys_rt_sigsuspend

	.data
	.align 3
	.globl sys_call_table
sys_call_table:
	.quad alpha_ni_syscall			/* 0 */
	.quad sys_exit
	.quad sys_fork
	.quad sys_read
	.quad sys_write
	.quad alpha_ni_syscall			/* 5 */
	.quad sys_close
	.quad osf_wait4
	.quad alpha_ni_syscall
	.quad sys_link
	.quad sys_unlink			/* 10 */
	.quad alpha_ni_syscall
	.quad sys_chdir
	.quad sys_fchdir
	.quad sys_mknod
	.quad sys_chmod				/* 15 */
	.quad sys_chown
	.quad osf_brk
	.quad alpha_ni_syscall
	.quad sys_lseek
	.quad sys_getxpid			/* 20 */
	.quad osf_mount
	.quad sys_umount
	.quad sys_setuid
	.quad sys_getxuid
	.quad alpha_ni_syscall			/* 25 */
	.quad sys_ptrace
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 30 */
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad sys_access
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 35 */
	.quad sys_sync
	.quad sys_kill
	.quad alpha_ni_syscall
	.quad sys_setpgid
	.quad alpha_ni_syscall			/* 40 */
	.quad sys_dup
	.quad sys_pipe
	.quad osf_set_program_attributes
	.quad alpha_ni_syscall
	.quad sys_open				/* 45 */
	.quad alpha_ni_syscall
	.quad sys_getxgid
	.quad osf_sigprocmask
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 50 */
	.quad sys_acct
	.quad sys_sigpending
	.quad alpha_ni_syscall
	.quad sys_ioctl
	.quad alpha_ni_syscall			/* 55 */
	.quad alpha_ni_syscall
	.quad sys_symlink
	.quad sys_readlink
	.quad sys_execve
	.quad sys_umask				/* 60 */
	.quad sys_chroot
	.quad alpha_ni_syscall
	.quad sys_getpgrp
	.quad sys_getpagesize
	.quad alpha_ni_syscall			/* 65 */
	.quad sys_vfork
	.quad sys_newstat
	.quad sys_newlstat
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 70 */
	.quad osf_mmap
	.quad alpha_ni_syscall
	.quad sys_munmap
	.quad sys_mprotect
	.quad sys_madvise			/* 75 */
	.quad sys_vhangup
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad sys_getgroups
	/* map BSD's setpgrp to sys_setpgid for binary compatibility: */
	.quad sys_setgroups			/* 80 */
	.quad alpha_ni_syscall
	.quad sys_setpgid
	.quad osf_setitimer
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 85 */
	.quad osf_getitimer
	.quad sys_gethostname
	.quad sys_sethostname
	.quad sys_getdtablesize
	.quad sys_dup2				/* 90 */
	.quad sys_newfstat
	.quad sys_fcntl
	.quad osf_select
	.quad sys_poll
	.quad sys_fsync				/* 95 */
	.quad sys_setpriority
	.quad sys_socket
	.quad sys_connect
	.quad sys_accept
	.quad osf_getpriority			/* 100 */
	.quad sys_send
	.quad sys_recv
	.quad sys_sigreturn
	.quad sys_bind
	.quad sys_setsockopt			/* 105 */
	.quad sys_listen
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 110 */
	.quad sys_sigsuspend
	.quad osf_sigstack
	.quad sys_recvmsg
	.quad sys_sendmsg
	.quad alpha_ni_syscall			/* 115 */
	.quad osf_gettimeofday
	.quad osf_getrusage
	.quad sys_getsockopt
	.quad alpha_ni_syscall
#ifdef CONFIG_OSF4_COMPAT
	.quad osf_readv				/* 120 */
	.quad osf_writev
#else
	.quad sys_readv				/* 120 */
	.quad sys_writev
#endif
	.quad osf_settimeofday
	.quad sys_fchown
	.quad sys_fchmod
	.quad sys_recvfrom			/* 125 */
	.quad sys_setreuid
	.quad sys_setregid
	.quad sys_rename
	.quad sys_truncate
	.quad sys_ftruncate			/* 130 */
	.quad sys_flock
	.quad sys_setgid
	.quad sys_sendto
	.quad sys_shutdown
	.quad sys_socketpair			/* 135 */
	.quad sys_mkdir
	.quad sys_rmdir
	.quad osf_utimes
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 140 */
	.quad sys_getpeername
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad sys_getrlimit
	.quad sys_setrlimit			/* 145 */
	.quad alpha_ni_syscall
	.quad sys_setsid
	.quad sys_quotactl
	.quad alpha_ni_syscall
	.quad sys_getsockname			/* 150 */
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 155 */
	.quad osf_sigaction
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad osf_getdirentries
	.quad osf_statfs			/* 160 */
	.quad osf_fstatfs
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad osf_getdomainname			/* 165 */
	.quad sys_setdomainname
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 170 */
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 175 */
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 180 */
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 185 */
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 190 */
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 195 */
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad osf_swapon
	.quad sys_msgctl			/* 200 */
	.quad sys_msgget
	.quad sys_msgrcv
	.quad sys_msgsnd
	.quad sys_semctl
	.quad sys_semget			/* 205 */
	.quad sys_semop
	.quad osf_utsname
	.quad sys_lchown
	.quad osf_shmat
	.quad sys_shmctl			/* 210 */
	.quad sys_shmdt
	.quad sys_shmget
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 215 */
	.quad alpha_ni_syscall
	.quad sys_msync
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 220 */
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 225 */
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 230 */
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad sys_getpgid
	.quad sys_getsid
	.quad sys_sigaltstack			/* 235 */
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 240 */
	.quad osf_sysinfo
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad osf_proplist_syscall
	.quad alpha_ni_syscall			/* 245 */
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 250 */
	.quad osf_usleep_thread
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad sys_sysfs
	.quad alpha_ni_syscall			/* 255 */
	.quad osf_getsysinfo
	.quad osf_setsysinfo
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 260 */
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 265 */
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 270 */
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 275 */
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 280 */
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 285 */
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 290 */
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall			/* 295 */
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
	.quad alpha_ni_syscall
/* linux-specific system calls start at 300 */
	.quad sys_bdflush			/* 300 */
	.quad sys_sethae
	.quad sys_mount
	.quad sys_old_adjtimex
	.quad sys_swapoff
	.quad sys_getdents			/* 305 */
	.quad alpha_create_module
	.quad sys_init_module
	.quad sys_delete_module
	.quad sys_get_kernel_syms
	.quad sys_syslog			/* 310 */
	.quad sys_reboot
	.quad sys_clone
	.quad sys_uselib
	.quad sys_mlock
	.quad sys_munlock			/* 315 */
	.quad sys_mlockall
	.quad sys_munlockall
	.quad sys_sysinfo
	.quad sys_sysctl
	.quad sys_ni_syscall			/* 320 */
	.quad sys_oldumount
	.quad sys_swapon
	.quad sys_times
	.quad sys_personality
	.quad sys_setfsuid			/* 325 */
	.quad sys_setfsgid
	.quad sys_ustat
	.quad sys_statfs
	.quad sys_fstatfs
	.quad sys_sched_setparam		/* 330 */
	.quad sys_sched_getparam
	.quad sys_sched_setscheduler
	.quad sys_sched_getscheduler
	.quad sys_sched_yield
	.quad sys_sched_get_priority_max	/* 335 */
	.quad sys_sched_get_priority_min
	.quad sys_sched_rr_get_interval
	.quad sys_ni_syscall			/* sys_afs_syscall */
	.quad sys_newuname
	.quad sys_nanosleep			/* 340 */
	.quad sys_mremap
	.quad sys_nfsservctl
	.quad sys_setresuid
	.quad sys_getresuid
	.quad sys_pciconfig_read		/* 345 */
	.quad sys_pciconfig_write
	.quad sys_query_module
	.quad sys_prctl
	.quad sys_pread
	.quad sys_pwrite			/* 350 */
	.quad sys_rt_sigreturn
	.quad sys_rt_sigaction
	.quad sys_rt_sigprocmask
	.quad sys_rt_sigpending
	.quad sys_rt_sigtimedwait		/* 355 */
	.quad sys_rt_sigqueueinfo
	.quad sys_rt_sigsuspend
	.quad sys_select
	.quad sys_gettimeofday
	.quad sys_settimeofday			/* 360 */
	.quad sys_getitimer
	.quad sys_setitimer
	.quad sys_utimes
	.quad sys_getrusage
	.quad sys_wait4				/* 365 */
	.quad sys_adjtimex
	.quad sys_getcwd
	.quad sys_capget
	.quad sys_capset
	.quad sys_sendfile			/* 370 */
	.quad sys_setresgid
	.quad sys_getresgid
	.quad sys_ni_syscall			/* sys_dipc */
	.quad sys_pivot_root
	.quad sys_mincore			/* 375 */
	.quad sys_pciconfig_iobase
	.quad sys_getdents64
	.quad sys_gettid
	.quad sys_readahead
	.quad sys_ni_syscall			/* 380, sys_security */
	.quad sys_tkill

/* Remember to update everything, kids.  */
.ifne (. - sys_call_table) - (NR_SYSCALLS * 8)
.err
.endif
