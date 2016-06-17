/*
 *  arch/s390/kernel/entry.S
 *    S390 low-level entry points.
 *
 *  S390 version
 *    Copyright (C) 1999,2000 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com),
 *               Hartmut Penner (hp@de.ibm.com),
 *               Denis Joseph Barrow (djbarrow@de.ibm.com,barrow_dj@yahoo.com),
 */

#include <linux/sys.h>
#include <linux/linkage.h>
#include <linux/config.h>
#include <asm/cache.h>
#include <asm/lowcore.h>
#include <asm/errno.h>
#include <asm/smp.h>
#include <asm/ptrace.h>

#include "asm-offsets.h"

/*
 * Stack layout for the system_call stack entry.
 * The first few entries are identical to the user_regs_struct.
 */
SP_PTREGS    =  STACK_FRAME_OVERHEAD 
SP_PSW       =  STACK_FRAME_OVERHEAD + PT_PSWMASK
SP_R0        =  STACK_FRAME_OVERHEAD + PT_GPR0
SP_R1        =  STACK_FRAME_OVERHEAD + PT_GPR1
SP_R2        =  STACK_FRAME_OVERHEAD + PT_GPR2
SP_R3        =  STACK_FRAME_OVERHEAD + PT_GPR3
SP_R4        =  STACK_FRAME_OVERHEAD + PT_GPR4
SP_R5        =  STACK_FRAME_OVERHEAD + PT_GPR5
SP_R6        =  STACK_FRAME_OVERHEAD + PT_GPR6
SP_R7        =  STACK_FRAME_OVERHEAD + PT_GPR7
SP_R8        =  STACK_FRAME_OVERHEAD + PT_GPR8
SP_R9        =  STACK_FRAME_OVERHEAD + PT_GPR9
SP_R10       =  STACK_FRAME_OVERHEAD + PT_GPR10
SP_R11       =  STACK_FRAME_OVERHEAD + PT_GPR11
SP_R12       =  STACK_FRAME_OVERHEAD + PT_GPR12
SP_R13       =  STACK_FRAME_OVERHEAD + PT_GPR13
SP_R14       =  STACK_FRAME_OVERHEAD + PT_GPR14
SP_R15       =  STACK_FRAME_OVERHEAD + PT_GPR15
SP_AREGS     =  STACK_FRAME_OVERHEAD + PT_ACR0
SP_ORIG_R2   =  STACK_FRAME_OVERHEAD + PT_ORIGGPR2
/* Now the additional entries */
SP_TRAP      =  (SP_ORIG_R2+GPR_SIZE)
SP_SIZE      =  (SP_TRAP+4)

/*
 * Register usage in interrupt handlers:
 *    R9  - pointer to current task structure
 *    R13 - pointer to literal pool
 *    R14 - return register for function calls
 *    R15 - kernel stack pointer
 */

        .macro  SAVE_ALL psworg,sync     # system entry macro
        stmg    %r14,%r15,__LC_SAVE_AREA
        tm      \psworg+1,0x01           # test problem state bit
	stam    %a2,%a4,__LC_SAVE_AREA+16
	.if	\sync
        jz      1f                       # skip stack setup save
	.else
	jnz	0f			 # from user -> load kernel stack
	lg	%r14,__LC_ASYNC_STACK	 # are we already on the async. stack ?
	slgr	%r14,%r15
	srag	%r14,%r14,14
	jz	1f
	lg	%r15,__LC_ASYNC_STACK	 # load async. stack
	j	1f
	.endif
0:	lg      %r15,__LC_KERNEL_STACK   # problem state -> load ksp
	larl	%r14,.Lc_ac
	lam	%a2,%a4,0(%r14)
1:      aghi    %r15,-SP_SIZE            # make room for registers & psw
        nill    %r15,0xfff8              # align stack pointer to 8
        stmg    %r0,%r14,SP_R0(%r15)     # store gprs 0-14 to kernel stack
        stg     %r2,SP_ORIG_R2(%r15)     # store original content of gpr 2
        mvc     SP_R14(16,%r15),__LC_SAVE_AREA # move R15 to stack
        stam    %a0,%a15,SP_AREGS(%r15)  # store access registers to kst.
        mvc     SP_AREGS+8(12,%r15),__LC_SAVE_AREA+16 # store ac. regs
        mvc     SP_PSW(16,%r15),\psworg  # move user PSW to stack
        lhi     %r0,\psworg              # store trap indication
        st      %r0,SP_TRAP(%r15)
        xc      0(8,%r15),0(%r15)        # clear back chain
        .endm

        .macro  RESTORE_ALL sync         # system exit macro
        mvc     __LC_RETURN_PSW(16),SP_PSW(%r15) # move user PSW to lowcore
        lam     %a0,%a15,SP_AREGS(%r15)  # load the access registers
        lmg     %r0,%r15,SP_R0(%r15)     # load gprs 0-15 of user
        ni      __LC_RETURN_PSW+1,0xfd   # clear wait state bit
        lpswe   __LC_RETURN_PSW          # back to caller
        .endm

        .macro  GET_CURRENT
	lg	%r9,__LC_KERNEL_STACK    # load pointer to task_struct to %r9
	aghi	%r9,-16384
        .endm


/*
 * Scheduler resume function, called by switch_to
 *  gpr2 = (task_struct *) prev
 *  gpr3 = (task_struct *) next
 * Returns:
 *  gpr2 = prev
 */
        .globl  resume
resume:
	tm	__THREAD_per+4(%r3),0xe8	# is the new process using per ?
	jz	resume_noper		# if not we're fine
        stctg   %c9,%c11,48(%r15)       # We are using per stuff
        clc     __THREAD_per(24,%r3),48(%r15)
        je      resume_noper            # we got away without bashing TLB's
        lctlg   %c9,%c11,__THREAD_per(%r3)	# Nope we didn't
resume_noper:
        stmg    %r6,%r15,48(%r15)       # store resume registers of prev task
	stg	%r15,__THREAD_ksp(%r2)	# store kernel stack to prev->tss.ksp
	lg	%r15,__THREAD_ksp(%r3)	# load kernel stack from next->tss.ksp
        stam    %a2,%a2,__THREAD_ar2(%r2)	# store kernel access reg. 2
        stam    %a4,%a4,__THREAD_ar4(%r2)	# store kernel access reg. 4
        lam     %a2,%a2,__THREAD_ar2(%r3)	# load kernel access reg. 2
        lam     %a4,%a4,__THREAD_ar4(%r3)	# load kernel access reg. 4
        lmg     %r6,%r15,48(%r15)       # load resume registers of next task
	aghi	%r3,16384
	stg	%r3,__LC_KERNEL_STACK	# __LC_KERNEL_STACK = new kernel stack
	br	%r14

/*
 * do_softirq calling function. We want to run the softirq functions on the
 * asynchronous interrupt stack.
 */
	.global do_call_softirq
do_call_softirq:
	stmg	%r12,%r15,48(%r15)
	lgr	%r12,%r15
	lg	%r0,__LC_ASYNC_STACK
	slgr    %r0,%r15
	srag	%r0,%r0,14
	je	0f
	lg	%r15,__LC_ASYNC_STACK
0:	aghi	%r15,-STACK_FRAME_OVERHEAD
	stg	%r12,0(%r15)		# store back chain
	brasl	%r14,do_softirq
	lmg	%r12,%r15,48(%r12)
	br	%r14
	
/*
 * SVC interrupt handler routine. System calls are synchronous events and
 * are executed with interrupts enabled.
 */

	.globl  system_call
system_call:
        SAVE_ALL __LC_SVC_OLD_PSW,1
	larl    %r7,sys_call_table
	llgh    %r8,__LC_SVC_INT_CODE # get svc number from lowcore
        sll     %r8,3
        GET_CURRENT               # load pointer to task_struct to R9
	stosm   48(%r15),0x03     # reenable interrupts
        tm      SP_PSW+3(%r15),0x01  # are we running in 31 bit mode ?
        jo      sysc_noemu
	la      %r8,4(%r8)        # use 31 bit emulation system calls
sysc_noemu:
        lgf     %r8,0(%r8,%r7)    # load address of system call routine
        tm      __TASK_ptrace+7(%r9),0x02 # PT_TRACESYS
        jnz     sysc_tracesys
        basr    %r14,%r8          # call sys_xxxx
        stg     %r2,SP_R2(%r15)   # store return value (change R2 on stack)
                                  # ATTENTION: check sys_execve_glue before
                                  # changing anything here !!

sysc_return:
        tm      SP_PSW+1(%r15),0x01 # returning to user ?
        jno     sysc_leave        # no-> skip bottom half, resched & signal
#
# check, if reschedule is needed
#
	lg      %r0,__TASK_need_resched(%r9)
	ltgr    %r0,%r0
        jnz     sysc_reschedule
        icm     %r0,15,__TASK_sigpending(%r9)
        jnz     sysc_signal_return
sysc_leave:
	stnsm   48(%r15),0xfc         # disable I/O and ext. interrupts
        RESTORE_ALL 1

#
# call do_signal before return
#
sysc_signal_return:     
        la      %r2,SP_PTREGS(%r15) # load pt_regs
        sgr     %r3,%r3             # clear *oldset
	larl    %r14,sysc_leave
	jg      do_signal           # return point is sysc_leave

#
# call schedule with sysc_return as return-address
#
sysc_reschedule:        
	larl    %r14,sysc_return
        jg      schedule            # return point is sysc_return
#
# call trace before and after sys_call
#
sysc_tracesys:
	larl	%r11,sysc_return

#
# call syscall_trace before and after system call
# special linkage: %r11 contains the return address for trace_svc
#
trace_svc:
	lghi    %r0,-ENOSYS
	stg     %r0,SP_R2(%r15)     # give sysc_trace an -ENOSYS retval
        brasl   %r14,syscall_trace
	lg      %r2,SP_R2(%r15)
	cghi    %r2,-ENOSYS
	je      trace_svc_go
	sllg    %r2,%r2,56          # strace wants to change the syscall
	srlg    %r2,%r2,53          # zap unused bits & multiply by 8
	tm      SP_PSW+3(%r15),0x01 # are we running in 31 bit mode ?
        jo      trace_svc_noemu
	la      %r2,4(%r2)          # use 31 bit emulation system calls
trace_svc_noemu:
	lgf	%r8,0(%r2,%r7)      # load address of system call routine
trace_svc_go:	
	lmg     %r3,%r6,SP_R3(%r15)
	lg      %r2,SP_ORIG_R2(%r15)
        basr    %r14,%r8            # call sys_xxx
        stg     %r2,SP_R2(%r15)     # store return value
	lgr	%r14,%r11	    # return point is in %r11
        jg      syscall_trace       # return point is sysc_return

#
# a new process exits the kernel with ret_from_fork
#
        .globl  ret_from_fork
ret_from_fork:  
        GET_CURRENT               # load pointer to task_struct to R9
        stosm   48(%r15),0x03     # reenable interrupts
	xc      SP_R2(8,%r15),SP_R2(%r15) # child returns 0
	larl    %r14,sysc_return
        jg      schedule_tail     # return to sysc_return

#
# clone, fork, vfork, exec and sigreturn need glue,
# because they all expect pt_regs as parameter,
# but are called with different parameter.
# return-address is set up above
#
sys_clone_glue: 
        la      %r2,SP_PTREGS(%r15)    # load pt_regs
        jg      sys_clone              # branch to sys_clone

sys_fork_glue:  
        la      %r2,SP_PTREGS(%r15)    # load pt_regs
        jg      sys_fork               # branch to sys_fork

sys_vfork_glue: 
        la      %r2,SP_PTREGS(%r15)    # load pt_regs
        jg      sys_vfork              # branch to sys_vfork

sys_execve_glue:        
        la      %r2,SP_PTREGS(%r15)   # load pt_regs
	lgr     %r12,%r14             # save return address
        brasl   %r14,sys_execve       # call sys_execve
        ltgr    %r2,%r2               # check if execve failed
        bnz     0(%r12)               # it did fail -> store result in gpr2
        b       6(%r12)               # SKIP STG 2,SP_R2(15) in
                                      # system_call/sysc_tracesys
#ifdef CONFIG_S390_SUPPORT
sys32_execve_glue:        
        la      %r2,SP_PTREGS(%r15)   # load pt_regs
	lgr     %r12,%r14             # save return address
        brasl   %r14,sys32_execve     # call sys32_execve
        ltgr    %r2,%r2               # check if execve failed
        bnz     0(%r12)               # it did fail -> store result in gpr2
        b       6(%r12)               # SKIP STG 2,SP_R2(15) in
                                      # system_call/sysc_tracesys
#endif

sys_sigreturn_glue:     
        la      %r2,SP_PTREGS(%r15)   # load pt_regs as parameter
        jg      sys_sigreturn         # branch to sys_sigreturn

#ifdef CONFIG_S390_SUPPORT
sys32_sigreturn_glue:     
        la      %r2,SP_PTREGS(%r15)   # load pt_regs as parameter
        jg      sys32_sigreturn       # branch to sys32_sigreturn
#endif

sys_rt_sigreturn_glue:     
        la      %r2,SP_PTREGS(%r15)   # load pt_regs as parameter
        jg      sys_rt_sigreturn      # branch to sys_sigreturn

#ifdef CONFIG_S390_SUPPORT
sys32_rt_sigreturn_glue:     
        la      %r2,SP_PTREGS(%r15)   # load pt_regs as parameter
        jg      sys32_rt_sigreturn    # branch to sys32_sigreturn
#endif

#
# sigsuspend and rt_sigsuspend need pt_regs as an additional
# parameter and they have to skip the store of %r2 into the
# user register %r2 because the return value was set in 
# sigsuspend and rt_sigsuspend already and must not be overwritten!
#

sys_sigsuspend_glue:    
        lgr     %r5,%r4               # move mask back
        lgr     %r4,%r3               # move history1 parameter
        lgr     %r3,%r2               # move history0 parameter
        la      %r2,SP_PTREGS(%r15)   # load pt_regs as first parameter
	la      %r14,6(%r14)          # skip store of return value
        jg      sys_sigsuspend        # branch to sys_sigsuspend

#ifdef CONFIG_S390_SUPPORT
sys32_sigsuspend_glue:    
	llgfr	%r4,%r4               # unsigned long			
        lgr     %r5,%r4               # move mask back
	lgfr	%r3,%r3               # int			
        lgr     %r4,%r3               # move history1 parameter
	lgfr	%r2,%r2               # int			
        lgr     %r3,%r2               # move history0 parameter
        la      %r2,SP_PTREGS(%r15)   # load pt_regs as first parameter
	la      %r14,6(%r14)          # skip store of return value
        jg      sys32_sigsuspend      # branch to sys32_sigsuspend
#endif

sys_rt_sigsuspend_glue: 
        lgr     %r4,%r3               # move sigsetsize parameter
        lgr     %r3,%r2               # move unewset parameter
        la      %r2,SP_PTREGS(%r15)   # load pt_regs as first parameter
	la      %r14,6(%r14)          # skip store of return value
        jg      sys_rt_sigsuspend     # branch to sys_rt_sigsuspend

#ifdef CONFIG_S390_SUPPORT
sys32_rt_sigsuspend_glue: 
	llgfr	%r3,%r3               # size_t			
        lgr     %r4,%r3               # move sigsetsize parameter
	llgtr	%r2,%r2               # sigset_emu31_t *
        lgr     %r3,%r2               # move unewset parameter
        la      %r2,SP_PTREGS(%r15)   # load pt_regs as first parameter
	la      %r14,6(%r14)          # skip store of return value
        jg      sys32_rt_sigsuspend   # branch to sys32_rt_sigsuspend
#endif

sys_sigaltstack_glue:
        la      %r4,SP_PTREGS(%r15)   # load pt_regs as parameter
        jg      sys_sigaltstack       # branch to sys_sigreturn

#ifdef CONFIG_S390_SUPPORT
sys32_sigaltstack_glue:
        la      %r4,SP_PTREGS(%r15)   # load pt_regs as parameter
        jg      sys32_sigaltstack_wrapper # branch to sys_sigreturn
#endif

#ifdef CONFIG_S390_SUPPORT
#define SYSCALL(esame,esa) esame,esa
#else
#define SYSCALL(esame,esa) esame,sys_ni_syscall
#endif

	.globl  sys_call_table	
sys_call_table:
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* 0 */
        .long  SYSCALL(sys_exit,sys32_exit_wrapper)
        .long  SYSCALL(sys_fork_glue,sys_fork_glue)
        .long  SYSCALL(sys_read,sys32_read_wrapper)
        .long  SYSCALL(sys_write,sys32_write_wrapper)
        .long  SYSCALL(sys_open,sys32_open_wrapper)             /* 5 */
        .long  SYSCALL(sys_close,sys32_close_wrapper)
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* old waitpid syscall */
        .long  SYSCALL(sys_creat,sys32_creat_wrapper)
        .long  SYSCALL(sys_link,sys32_link_wrapper)
        .long  SYSCALL(sys_unlink,sys32_unlink_wrapper)         /* 10 */
        .long  SYSCALL(sys_execve_glue,sys32_execve_glue)
        .long  SYSCALL(sys_chdir,sys32_chdir_wrapper)
        .long  SYSCALL(sys_ni_syscall,sys32_time_wrapper) /* old time syscall */
        .long  SYSCALL(sys_mknod,sys32_mknod_wrapper)
        .long  SYSCALL(sys_chmod,sys32_chmod_wrapper)           /* 15 */
        .long  SYSCALL(sys_ni_syscall,sys32_lchown16_wrapper)   /* old lchown16 syscall*/
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* old break syscall */
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* old stat syscall */
        .long  SYSCALL(sys_lseek,sys32_lseek_wrapper)
        .long  SYSCALL(sys_getpid,sys_getpid) /* 20 */
        .long  SYSCALL(sys_mount,sys32_mount_wrapper)
        .long  SYSCALL(sys_oldumount,sys32_oldumount_wrapper)
        .long  SYSCALL(sys_ni_syscall,sys32_setuid16_wrapper)   /* old setuid16 syscall*/
        .long  SYSCALL(sys_ni_syscall,sys32_getuid16)   /* old getuid16 syscall*/
        .long  SYSCALL(sys_ni_syscall,sys32_stime_wrapper) /* 25 old stime syscall */
        .long  SYSCALL(sys_ptrace,sys32_ptrace_wrapper)
        .long  SYSCALL(sys_alarm,sys32_alarm_wrapper)
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* old fstat syscall */
        .long  SYSCALL(sys_pause,sys32_pause)
        .long  SYSCALL(sys_utime,sys32_utime_wrapper)           /* 30 */
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* old stty syscall */
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* old gtty syscall */
        .long  SYSCALL(sys_access,sys32_access_wrapper)
        .long  SYSCALL(sys_nice,sys32_nice_wrapper)
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* old ftime syscall */
        .long  SYSCALL(sys_sync,sys_sync)
        .long  SYSCALL(sys_kill,sys32_kill_wrapper)
        .long  SYSCALL(sys_rename,sys32_rename_wrapper)
        .long  SYSCALL(sys_mkdir,sys32_mkdir_wrapper)
        .long  SYSCALL(sys_rmdir,sys32_rmdir_wrapper)           /* 40 */
        .long  SYSCALL(sys_dup,sys32_dup_wrapper)
        .long  SYSCALL(sys_pipe,sys32_pipe_wrapper)
        .long  SYSCALL(sys_times,sys32_times_wrapper)
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* old prof syscall */
        .long  SYSCALL(sys_brk,sys32_brk_wrapper)               /* 45 */
        .long  SYSCALL(sys_ni_syscall,sys32_setgid16)   /* old setgid16 syscall*/
        .long  SYSCALL(sys_ni_syscall,sys32_getgid16)   /* old getgid16 syscall*/
        .long  SYSCALL(sys_signal,sys32_signal_wrapper)
        .long  SYSCALL(sys_ni_syscall,sys32_geteuid16)  /* old geteuid16 syscall */
        .long  SYSCALL(sys_ni_syscall,sys32_getegid16)  /* old getegid16 syscall */
        .long  SYSCALL(sys_acct,sys32_acct_wrapper)
        .long  SYSCALL(sys_umount,sys32_umount_wrapper)
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* old lock syscall */
        .long  SYSCALL(sys_ioctl,sys32_ioctl_wrapper)
        .long  SYSCALL(sys_fcntl,sys32_fcntl_wrapper)   /* 55 */
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* intel mpx syscall */
        .long  SYSCALL(sys_setpgid,sys32_setpgid_wrapper)
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* old ulimit syscall */
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* old uname syscall */
        .long  SYSCALL(sys_umask,sys32_umask_wrapper)           /* 60 */
        .long  SYSCALL(sys_chroot,sys32_chroot_wrapper)
        .long  SYSCALL(sys_ustat,sys32_ustat_wrapper)
        .long  SYSCALL(sys_dup2,sys32_dup2_wrapper)
        .long  SYSCALL(sys_getppid,sys_getppid)
        .long  SYSCALL(sys_getpgrp,sys_getpgrp)       /* 65 */
        .long  SYSCALL(sys_setsid,sys_setsid)
        .long  SYSCALL(sys_sigaction,sys32_sigaction_wrapper)
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* old sgetmask syscall*/
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* old ssetmask syscall*/
        .long  SYSCALL(sys_ni_syscall,sys32_setreuid16_wrapper) /* old setreuid16 syscall */
        .long  SYSCALL(sys_ni_syscall,sys32_setregid16_wrapper) /* old setregid16 syscall */
        .long  SYSCALL(sys_sigsuspend_glue,sys32_sigsuspend_glue)
        .long  SYSCALL(sys_sigpending,sys32_sigpending_wrapper)
        .long  SYSCALL(sys_sethostname,sys32_sethostname_wrapper)
        .long  SYSCALL(sys_setrlimit,sys32_setrlimit_wrapper)   /* 75 */
        .long  SYSCALL(sys_getrlimit,sys32_old_getrlimit_wrapper) 
        .long  SYSCALL(sys_getrusage,sys32_getrusage_wrapper)
        .long  SYSCALL(sys_gettimeofday,sys32_gettimeofday_wrapper)
        .long  SYSCALL(sys_settimeofday,sys32_settimeofday_wrapper)
        .long  SYSCALL(sys_ni_syscall,sys32_getgroups16_wrapper) /* old getgroups16 syscall */
        .long  SYSCALL(sys_ni_syscall,sys32_setgroups16_wrapper) /* old setgroups16 syscall */
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* old select syscall */
        .long  SYSCALL(sys_symlink,sys32_symlink_wrapper)
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* old lstat syscall */
        .long  SYSCALL(sys_readlink,sys32_readlink_wrapper)     /* 85 */
        .long  SYSCALL(sys_uselib,sys32_uselib_wrapper)
        .long  SYSCALL(sys_swapon,sys32_swapon_wrapper)
        .long  SYSCALL(sys_reboot,sys32_reboot_wrapper)
        .long  SYSCALL(sys_ni_syscall,old32_readdir_wrapper) /* old readdir syscall */
        .long  SYSCALL(old_mmap,old32_mmap_wrapper)       /* 90 */
        .long  SYSCALL(sys_munmap,sys32_munmap_wrapper)
        .long  SYSCALL(sys_truncate,sys32_truncate_wrapper)
        .long  SYSCALL(sys_ftruncate,sys32_ftruncate_wrapper)
        .long  SYSCALL(sys_fchmod,sys32_fchmod_wrapper)
        .long  SYSCALL(sys_ni_syscall,sys32_fchown16_wrapper)   /* old fchown16 syscall*/
        .long  SYSCALL(sys_getpriority,sys32_getpriority_wrapper)
        .long  SYSCALL(sys_setpriority,sys32_setpriority_wrapper)
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* old profil syscall */
        .long  SYSCALL(sys_statfs,sys32_statfs_wrapper)
        .long  SYSCALL(sys_fstatfs,sys32_fstatfs_wrapper)   /* 100 */
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall)
        .long  SYSCALL(sys_socketcall,sys32_socketcall_wrapper)
        .long  SYSCALL(sys_syslog,sys32_syslog_wrapper)
        .long  SYSCALL(sys_setitimer,sys32_setitimer_wrapper)
        .long  SYSCALL(sys_getitimer,sys32_getitimer_wrapper)   /* 105 */
        .long  SYSCALL(sys_newstat,sys32_newstat_wrapper)
        .long  SYSCALL(sys_newlstat,sys32_newlstat_wrapper)
        .long  SYSCALL(sys_newfstat,sys32_newfstat_wrapper)
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* old uname syscall */
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* iopl for i386 */
        .long  SYSCALL(sys_vhangup,sys_vhangup)
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* old "idle" system call */
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* vm86old for i386 */
        .long  SYSCALL(sys_wait4,sys32_wait4_wrapper)
        .long  SYSCALL(sys_swapoff,sys32_swapoff_wrapper)       /* 115 */
        .long  SYSCALL(sys_sysinfo,sys32_sysinfo_wrapper)
        .long  SYSCALL(sys_ipc,sys32_ipc_wrapper)
        .long  SYSCALL(sys_fsync,sys32_fsync_wrapper)
        .long  SYSCALL(sys_sigreturn_glue,sys32_sigreturn_glue)
        .long  SYSCALL(sys_clone_glue,sys_clone_glue) /* 120 */
        .long  SYSCALL(sys_setdomainname,sys32_setdomainname_wrapper)
        .long  SYSCALL(s390x_newuname,sys32_newuname_wrapper)
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* modify_ldt for i386 */
        .long  SYSCALL(sys_adjtimex,sys32_adjtimex_wrapper)
        .long  SYSCALL(sys_mprotect,sys32_mprotect_wrapper) /* 125 */
        .long  SYSCALL(sys_sigprocmask,sys32_sigprocmask_wrapper)
        .long  SYSCALL(sys_create_module,sys32_create_module_wrapper)
        .long  SYSCALL(sys_init_module,sys32_init_module_wrapper)
        .long  SYSCALL(sys_delete_module,sys32_delete_module_wrapper)
        .long  SYSCALL(sys_get_kernel_syms,sys32_get_kernel_syms_wrapper) /* 130 */
        .long  SYSCALL(sys_quotactl,sys32_quotactl_wrapper)
        .long  SYSCALL(sys_getpgid,sys32_getpgid_wrapper)
        .long  SYSCALL(sys_fchdir,sys32_fchdir_wrapper)
        .long  SYSCALL(sys_bdflush,sys32_bdflush_wrapper)
        .long  SYSCALL(sys_sysfs,sys32_sysfs_wrapper)           /* 135 */
        .long  SYSCALL(s390x_personality,sys32_personality_wrapper)
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* for afs_syscall */
        .long  SYSCALL(sys_ni_syscall,sys32_setfsuid16_wrapper) /* old setfsuid16 syscall */
        .long  SYSCALL(sys_ni_syscall,sys32_setfsgid16_wrapper) /* old setfsgid16 syscall */
        .long  SYSCALL(sys_llseek,sys32_llseek_wrapper)         /* 140 */
        .long  SYSCALL(sys_getdents,sys32_getdents_wrapper)
        .long  SYSCALL(sys_select,sys32_select_wrapper)
        .long  SYSCALL(sys_flock,sys32_flock_wrapper)
        .long  SYSCALL(sys_msync,sys32_msync_wrapper)
        .long  SYSCALL(sys_readv,sys32_readv_wrapper)           /* 145 */
        .long  SYSCALL(sys_writev,sys32_writev_wrapper)
        .long  SYSCALL(sys_getsid,sys32_getsid_wrapper)
        .long  SYSCALL(sys_fdatasync,sys32_fdatasync_wrapper)
        .long  SYSCALL(sys_sysctl,sys32_sysctl_wrapper)
        .long  SYSCALL(sys_mlock,sys32_mlock_wrapper)           /* 150 */
        .long  SYSCALL(sys_munlock,sys32_munlock_wrapper)
        .long  SYSCALL(sys_mlockall,sys32_mlockall_wrapper)
        .long  SYSCALL(sys_munlockall,sys_munlockall)
        .long  SYSCALL(sys_sched_setparam,sys32_sched_setparam_wrapper)
        .long  SYSCALL(sys_sched_getparam,sys32_sched_getparam_wrapper) /* 155 */
        .long  SYSCALL(sys_sched_setscheduler,sys32_sched_setscheduler_wrapper)
        .long  SYSCALL(sys_sched_getscheduler,sys32_sched_getscheduler_wrapper)
        .long  SYSCALL(sys_sched_yield,sys_sched_yield)
        .long  SYSCALL(sys_sched_get_priority_max,sys32_sched_get_priority_max_wrapper)
        .long  SYSCALL(sys_sched_get_priority_min,sys32_sched_get_priority_min_wrapper)
        .long  SYSCALL(sys_sched_rr_get_interval,sys32_sched_rr_get_interval_wrapper)
        .long  SYSCALL(sys_nanosleep,sys32_nanosleep_wrapper)
        .long  SYSCALL(sys_mremap,sys32_mremap_wrapper)
        .long  SYSCALL(sys_ni_syscall,sys32_setresuid16_wrapper) /* old setresuid16 syscall */
        .long  SYSCALL(sys_ni_syscall,sys32_getresuid16_wrapper) /* old getresuid16 syscall */
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* for vm86 */
        .long  SYSCALL(sys_query_module,sys32_query_module_wrapper)
        .long  SYSCALL(sys_poll,sys32_poll_wrapper)
        .long  SYSCALL(sys_nfsservctl,sys32_nfsservctl_wrapper)
        .long  SYSCALL(sys_ni_syscall,sys32_setresgid16_wrapper) /* old setresgid16 syscall */
        .long  SYSCALL(sys_ni_syscall,sys32_getresgid16_wrapper) /* old getresgid16 syscall */
        .long  SYSCALL(sys_prctl,sys32_prctl_wrapper)
        .long  SYSCALL(sys_rt_sigreturn_glue,sys32_rt_sigreturn_glue)
        .long  SYSCALL(sys_rt_sigaction,sys32_rt_sigaction_wrapper)
        .long  SYSCALL(sys_rt_sigprocmask,sys32_rt_sigprocmask_wrapper) /* 175 */
        .long  SYSCALL(sys_rt_sigpending,sys32_rt_sigpending_wrapper)
        .long  SYSCALL(sys_rt_sigtimedwait,sys32_rt_sigtimedwait_wrapper)
        .long  SYSCALL(sys_rt_sigqueueinfo,sys32_rt_sigqueueinfo_wrapper)
        .long  SYSCALL(sys_rt_sigsuspend_glue,sys32_rt_sigsuspend_glue)
        .long  SYSCALL(sys_pread,sys32_pread_wrapper)           /* 180 */
        .long  SYSCALL(sys_pwrite,sys32_pwrite_wrapper)
        .long  SYSCALL(sys_ni_syscall,sys32_chown16_wrapper)    /* old chown16 syscall */
        .long  SYSCALL(sys_getcwd,sys32_getcwd_wrapper)
        .long  SYSCALL(sys_capget,sys32_capget_wrapper)
        .long  SYSCALL(sys_capset,sys32_capset_wrapper)         /* 185 */
        .long  SYSCALL(sys_sigaltstack_glue,sys32_sigaltstack_glue)
        .long  SYSCALL(sys_sendfile,sys32_sendfile_wrapper)
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* streams1 */
        .long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* streams2 */
        .long  SYSCALL(sys_vfork_glue,sys_vfork_glue) /* 190 */
        .long  SYSCALL(sys_getrlimit,sys32_getrlimit_wrapper)
	.long  SYSCALL(sys_mmap2,sys32_mmap2_wrapper)
	.long  SYSCALL(sys_ni_syscall,sys32_truncate64_wrapper)
        .long  SYSCALL(sys_ni_syscall,sys32_ftruncate64_wrapper)
        .long  SYSCALL(sys_ni_syscall,sys32_stat64_wrapper)     /* 195 */
        .long  SYSCALL(sys_ni_syscall,sys32_lstat64_wrapper)   
        .long  SYSCALL(sys_ni_syscall,sys32_fstat64_wrapper)    
	.long  SYSCALL(sys_lchown,sys32_lchown_wrapper)
	.long  SYSCALL(sys_getuid,sys_getuid)
	.long  SYSCALL(sys_getgid,sys_getgid)         /* 200 */
	.long  SYSCALL(sys_geteuid,sys_geteuid)
	.long  SYSCALL(sys_getegid,sys_getegid)
	.long  SYSCALL(sys_setreuid,sys32_setreuid_wrapper)
	.long  SYSCALL(sys_setregid,sys32_setregid_wrapper)
	.long  SYSCALL(sys_getgroups,sys32_getgroups_wrapper)  /* 205 */
	.long  SYSCALL(sys_setgroups,sys32_setgroups_wrapper)
	.long  SYSCALL(sys_fchown,sys32_fchown_wrapper)
	.long  SYSCALL(sys_setresuid,sys32_setresuid_wrapper)
	.long  SYSCALL(sys_getresuid,sys32_getresuid_wrapper)
	.long  SYSCALL(sys_setresgid,sys32_setresgid_wrapper)  /* 210 */
	.long  SYSCALL(sys_getresgid,sys32_getresgid_wrapper)
	.long  SYSCALL(sys_chown,sys32_chown_wrapper)
	.long  SYSCALL(sys_setuid,sys32_setuid_wrapper)
	.long  SYSCALL(sys_setgid,sys32_setgid_wrapper)
	.long  SYSCALL(sys_setfsuid,sys32_setfsuid_wrapper)   /* 215 */
	.long  SYSCALL(sys_setfsgid,sys32_setfsgid_wrapper)
        .long  SYSCALL(sys_pivot_root,sys32_pivot_root_wrapper)
        .long  SYSCALL(sys_mincore,sys32_mincore_wrapper)
        .long  SYSCALL(sys_madvise,sys32_madvise_wrapper)
	.long  SYSCALL(sys_getdents64,sys32_getdents64_wrapper)/* 220 */
	.long  SYSCALL(sys_ni_syscall,sys32_fcntl64_wrapper)
	.long  SYSCALL(sys_readahead,sys32_readahead)
	.long  SYSCALL(sys_ni_syscall,sys_ni_syscall)
	.long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* 224 - reserved for setxattr  */
	.long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* 225 - reserved for lsetxattr */
	.long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* 226 - reserved for fsetxattr */
	.long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* 227 - reserved for getxattr  */
	.long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* 228 - reserved for lgetxattr */
	.long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* 229 - reserved for fgetxattr */
	.long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* 230 - reserved for listxattr */
	.long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* 231 - reserved for llistxattr */
	.long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* 232 - reserved for flistxattr */
	.long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* 233 - reserved for removexattr */
	.long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* 234 - reserved for lremovexattr */
	.long  SYSCALL(sys_ni_syscall,sys_ni_syscall) /* 235 - reserved for fremovexattr */
	.long  SYSCALL(sys_gettid,sys_gettid)
	.long  SYSCALL(sys_tkill,sys_tkill)
	.rept  255-237
	.long  SYSCALL(sys_ni_syscall,sys_ni_syscall)
	.endr

/*
 * Program check handler routine
 */

        .globl  pgm_check_handler
pgm_check_handler:
/*
 * First we need to check for a special case:
 * Single stepping an instruction that disables the PER event mask will
 * cause a PER event AFTER the mask has been set. Example: SVC or LPSW.
 * For a single stepped SVC the program check handler gets control after
 * the SVC new PSW has been loaded. But we want to execute the SVC first and
 * then handle the PER event. Therefore we update the SVC old PSW to point
 * to the pgm_check_handler and branch to the SVC handler after we checked
 * if we have to load the kernel stack register.
 * For every other possible cause for PER event without the PER mask set
 * we just ignore the PER event (FIXME: is there anything we have to do
 * for LPSW?).
 */
        tm      __LC_PGM_INT_CODE+1,0x80 # check whether we got a per exception
        jnz     pgm_per                  # got per exception -> special case
	SAVE_ALL __LC_PGM_OLD_PSW,1
	lgf     %r3,__LC_PGM_ILC	 # load program interruption code
	lghi	%r8,0x7f
	ngr	%r8,%r3
        sll     %r8,3
	GET_CURRENT
        larl    %r1,pgm_check_table
        lg      %r1,0(%r8,%r1)		 # load address of handler routine
        la      %r2,SP_PTREGS(%r15)	 # address of register-save area
	larl	%r14,sysc_return
        br      %r1			 # branch to interrupt-handler

#
# handle per exception
#
pgm_per:
        tm      __LC_PGM_OLD_PSW,0x40    # test if per event recording is on
        jnz     pgm_per_std              # ok, normal per event from user space
# ok its one of the special cases, now we need to find out which one
        clc     __LC_PGM_OLD_PSW(16),__LC_SVC_NEW_PSW
        je      pgm_svcper
# no interesting special case, ignore PER event
	lpswe   __LC_PGM_OLD_PSW

#
# Normal per exception
#
pgm_per_std:
	SAVE_ALL __LC_PGM_OLD_PSW,1
	GET_CURRENT
	lghi    %r4,0x7f
	lgf     %r3,__LC_PGM_ILC	 # load program interruption code
        nr      %r4,%r3			 # clear per-event-bit and ilc
        je      pgm_per_only		 # only per of per+check ?
        sll     %r4,3
        larl    %r1,pgm_check_table
        lg      %r1,0(%r4,%r1)		 # load address of handler routine
        la      %r2,SP_PTREGS(%r15)	 # address of register-save area
        basr    %r14,%r1		 # branch to interrupt-handler
pgm_per_only:
        la      %r2,SP_PTREGS(15)	 # address of register-save area
        larl    %r14,sysc_return	 # load adr. of system return
        jg      handle_per_exception

#
# it was a single stepped SVC that is causing all the trouble
#
pgm_svcper:
	SAVE_ALL __LC_SVC_OLD_PSW,1
	larl    %r7,sys_call_table
	llgh    %r8,__LC_SVC_INT_CODE # get svc number from lowcore
        sll     %r8,3
        GET_CURRENT               # load pointer to task_struct to R9
	stosm   48(%r15),0x03     # reenable interrupts
        tm      SP_PSW+3(%r15),0x01  # are we running in 31 bit mode ?
        jo      pgm_svcper_noemu
	la      %r8,4(%r8)        # use 31 bit emulation system calls
pgm_svcper_noemu:
        lgf     %r8,0(%r8,%r7)    # load address of system call routine
        tm      __TASK_ptrace+7(%r9),0x02 # PT_TRACESYS
        jnz     pgm_tracesys
        basr    %r14,%r8          # call sys_xxxx
        stg     %r2,SP_R2(%r15)   # store return value (change R2 on stack)
                                  # ATTENTION: check sys_execve_glue before
                                  # changing anything here !!

pgm_svcret:
        icm     %r0,15,__TASK_sigpending(%r9)
        jz      pgm_svcper_nosig
        la      %r2,SP_PTREGS(%r15) # load pt_regs
        sgr     %r3,%r3             # clear *oldset
	brasl	%r14,do_signal
	
pgm_svcper_nosig:
	lhi     %r0,__LC_PGM_OLD_PSW     # set trap indication back to pgm_chk
	st      %r0,SP_TRAP(%r15)
        la      %r2,SP_PTREGS(15) # address of register-save area
        larl    %r14,sysc_return  # load adr. of system return
        jg      handle_per_exception
#
# call trace before and after sys_call
#
pgm_tracesys:
	larl	%r11,pgm_svcret
	j	trace_svc

/*
 * IO interrupt handler routine
 */
        .globl io_int_handler
io_int_handler:
        SAVE_ALL __LC_IO_OLD_PSW,0
        GET_CURRENT                    # load pointer to task_struct to R9
        la      %r2,SP_PTREGS(%r15)    # address of register-save area
	llgh    %r3,__LC_SUBCHANNEL_NR # load subchannel number
        llgf    %r4,__LC_IO_INT_PARM   # load interuption parm
        llgf    %r5,__LC_IO_INT_WORD   # load interuption word
	brasl   %r14,do_IRQ            # call standard irq handler

io_return:
#
# check, if bottom-half has to be done
#
	lgf     %r1,__TASK_processor(%r9)
	larl    %r2,irq_stat
	sll     %r1,L1_CACHE_SHIFT
	la      %r1,0(%r1,%r2)
	icm     %r0,15,0(%r1)         # test irq_stat[#cpu].__softirq_pending
        jnz     io_handle_bottom_half
io_return_bh:	

        tm      SP_PSW+1(%r15),0x01    # returning to user ?
        jno     io_leave               # no-> skip resched & signal
        stosm   48(%r15),0x03          # reenable interrupts
#
# check, if reschedule is needed
#
	lg      %r0,__TASK_need_resched(%r9)
	ltgr    %r0,%r0
        jnz     io_reschedule
        icm     %r0,15,__TASK_sigpending(%r9)
        jnz     io_signal_return
io_leave:
        stnsm   48(%r15),0xfc          # disable I/O and ext. interrupts
        RESTORE_ALL 0

#
# call do_softirq and return from syscall, if interrupt-level
# is zero
#
io_handle_bottom_half:        
	larl    %r14,io_return_bh
        jg      do_softirq          # return point is io_return_bh

#
# call schedule with io_return as return-address
#
io_reschedule:        
	larl    %r14,io_return
        jg      schedule            # call scheduler, return to io_return

#
# call do_signal before return
#
io_signal_return:     
        la      %r2,SP_PTREGS(%r15) # load pt_regs
        slgr    %r3,%r3             # clear *oldset
	larl    %r14,io_leave
        jg      do_signal           # return point is io_leave

/*
 * External interrupt handler routine
 */
        .globl  ext_int_handler
ext_int_handler:
        SAVE_ALL __LC_EXT_OLD_PSW,0
        GET_CURRENT                    # load pointer to task_struct to R9
	llgh	%r6,__LC_EXT_INT_CODE  # get interruption code
	lgr	%r1,%r6		       # calculate index = code & 0xff
	nill	%r1,0xff
	sll	%r1,3
	larl	%r7,ext_int_hash
	lg	%r7,0(%r1,%r7)	       # get first list entry for hash value
	ltgr	%r7,%r7		       # == NULL ?
	jz	io_return	       # yes, nothing to do, exit
ext_int_loop:
	ch	%r6,16(%r7)	       # compare external interrupt code
	jne	ext_int_next
	lg	%r1,8(%r7)	       # get handler address
	la	%r2,SP_PTREGS(%r15)    # address of register-save area
	lgr	%r3,%r6		       # interruption code
	basr	%r14,%r1	       # call handler
ext_int_next:
	lg	%r7,0(%r7)	       # next list entry
	ltgr	%r7,%r7
	jnz	ext_int_loop
	j	io_return

/*
 * Machine check handler routines
 */
        .globl mcck_int_handler
mcck_int_handler:
        SAVE_ALL __LC_MCK_OLD_PSW,0
	brasl   %r14,s390_do_machine_check
mcck_return:
        RESTORE_ALL 0

#ifdef CONFIG_SMP
/*
 * Restart interruption handler, kick starter for additional CPUs
 */
        .globl restart_int_handler
restart_int_handler:
        lg      %r15,__LC_SAVE_AREA+120 # load ksp
        lghi    %r10,__LC_CREGS_SAVE_AREA
        lctlg   %c0,%c15,0(%r10) # get new ctl regs
        lghi    %r10,__LC_AREGS_SAVE_AREA
        lam     %a0,%a15,0(%r10)
        stosm   0(%r15),0x04           # now we can turn dat on
        lmg     %r6,%r15,48(%r15)      # load registers from clone
	jg      start_secondary
#else
/*
 * If we do not run with SMP enabled, let the new CPU crash ...
 */
        .globl restart_int_handler
restart_int_handler:
        basr    %r1,0
restart_base:
        lpswe   restart_crash-restart_base(%r1)
        .align 8
restart_crash:
        .long  0x000a0000,0x00000000,0x00000000,0x00000000
restart_go:
#endif

/*
 * Integer constants
 */
               .align 4
.Lc_ac:        .long  0,0,1
