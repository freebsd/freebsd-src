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
 * Base Address of this Module --- saved in __LC_ENTRY_BASE
 */
       .globl entry_base
entry_base:

#define BASED(name) name-entry_base(%r13)

/*
 * Register usage in interrupt handlers:
 *    R9  - pointer to current task structure
 *    R13 - pointer to literal pool
 *    R14 - return register for function calls
 *    R15 - kernel stack pointer
 */

        .macro  SAVE_ALL_BASE
        stm     %r13,%r15,__LC_SAVE_AREA
        basr    %r13,0                    #  temp base pointer
0:	stam    %a2,%a4,__LC_SAVE_AREA+12
        l       %r13,.Lentry_base-0b(%r13)# load &entry_base to %r13
	.endm

        .macro  SAVE_ALL psworg,sync      # system entry macro
        tm      \psworg+1,0x01            # test problem state bit
	.if	\sync
        bz      BASED(1f)                 # skip stack setup save
	.else
        bnz     BASED(0f)                 # from user -> load kernel stack
	l	%r14,__LC_ASYNC_STACK	  # are we already on the async stack ?
	slr     %r14,%r15
	sra	%r14,13
	be	BASED(1f)
        l       %r15,__LC_ASYNC_STACK     # load async. stack
	b	BASED(1f)
	.endif
0:      l       %r15,__LC_KERNEL_STACK    # problem state -> load ksp
	lam	%a2,%a4,BASED(.Lc_ac)	  # set ac.reg. 2 to primary space
					  # and ac.reg. 4 to home space
1:      s       %r15,BASED(.Lc_spsize)    # make room for registers & psw
        n       %r15,BASED(.Lc0xfffffff8) # align stack pointer to 8
        stm     %r0,%r12,SP_R0(%r15)      # store gprs 0-12 to kernel stack
        st      %r2,SP_ORIG_R2(%r15)      # store original content of gpr 2
        mvc     SP_R13(12,%r15),__LC_SAVE_AREA  # move R13-R15 to stack
        stam    %a0,%a15,SP_AREGS(%r15)   # store access registers to kst.
        mvc     SP_AREGS+8(12,%r15),__LC_SAVE_AREA+12 # store ac. regs
        mvc     SP_PSW(8,%r15),\psworg    # move user PSW to stack
        la      %r0,\psworg               # store trap indication
        st      %r0,SP_TRAP(%r15)
        xc      0(4,%r15),0(%r15)         # clear back chain
        .endm

        .macro  RESTORE_ALL sync          # system exit macro
        mvc     __LC_RETURN_PSW(8),SP_PSW(%r15)  # move user PSW to lowcore
        lam     %a0,%a15,SP_AREGS(%r15)   # load the access registers
        lm      %r0,%r15,SP_R0(%r15)      # load gprs 0-15 of user
        ni      __LC_RETURN_PSW+1,0xfd    # clear wait state bit
        lpsw    __LC_RETURN_PSW           # back to caller
        .endm

        .macro  GET_CURRENT
	l	%r9,BASED(.Lc0xffffe000)  # load pointer to task_struct to %r9
	al	%r9,__LC_KERNEL_STACK
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
        basr    %r1,0
resume_base:
	tm	__THREAD_per(%r3),0xe8		# new process is using per ?
	bz	resume_noper-resume_base(%r1)	# if not we're fine
        stctl   %c9,%c11,24(%r15)		# We are using per stuff
        clc     __THREAD_per(12,%r3),24(%r15)
        be      resume_noper-resume_base(%r1)	# we got away w/o bashing TLB's
        lctl    %c9,%c11,__THREAD_per(%r3)	# Nope we didn't
resume_noper:
        stm     %r6,%r15,24(%r15)       # store resume registers of prev task
	st	%r15,__THREAD_ksp(%r2)	# store kernel stack to prev->tss.ksp
	l	%r15,__THREAD_ksp(%r3)	# load kernel stack from next->tss.ksp
	stam    %a2,%a2,__THREAD_ar2(%r2)	# store kernel access reg. 2
	stam    %a4,%a4,__THREAD_ar4(%r2)	# store kernel access reg. 4
	lam     %a2,%a2,__THREAD_ar2(%r3)	# load kernel access reg. 2
	lam     %a4,%a4,__THREAD_ar4(%r3)	# load kernel access reg. 4
	lm	%r6,%r15,24(%r15)	# load resume registers of next task
	ahi	%r3,8192
	st	%r3,__LC_KERNEL_STACK	# __LC_KERNEL_STACK = new kernel stack
	br	%r14

/*
 * do_softirq calling function. We want to run the softirq functions on the
 * asynchronous interrupt stack.
 */
	.global do_call_softirq
do_call_softirq:
	stm	%r12,%r15,24(%r15)
	lr	%r12,%r15
        basr    %r13,0
do_call_base:
	l	%r0,__LC_ASYNC_STACK
	slr     %r0,%r15
	sra	%r0,13
	be	0f-do_call_base(%r13)
	l	%r15,__LC_ASYNC_STACK
0:	sl	%r15,.Lc_overhead-do_call_base(%r13)
        st	%r12,0(%r15)	# store backchain
	l	%r1,.Ldo_softirq-do_call_base(%r13)
	basr	%r14,%r1
	lm	%r12,%r15,24(%r12)
	br	%r14
	
/*
 * SVC interrupt handler routine. System calls are synchronous events and
 * are executed with interrupts enabled.
 */

	.globl  system_call
system_call:
	SAVE_ALL_BASE
        SAVE_ALL __LC_SVC_OLD_PSW,1
	lh	%r8,0x8a	  # get svc number from lowcore
        sll     %r8,2
        GET_CURRENT               # load pointer to task_struct to R9
        stosm   24(%r15),0x03     # reenable interrupts
        l       %r8,sys_call_table-entry_base(%r8,%r13) # get system call addr.
        tm      __TASK_ptrace+3(%r9),0x02 # PT_TRACESYS
        bnz     BASED(sysc_tracesys)
        basr    %r14,%r8          # call sys_xxxx
        st      %r2,SP_R2(%r15)   # store return value (change R2 on stack)
                                  # ATTENTION: check sys_execve_glue before
                                  # changing anything here !!

sysc_return:
        tm      SP_PSW+1(%r15),0x01 # returning to user ?
        bno     BASED(sysc_leave) # no-> skip resched & signal
#
# check, if reschedule is needed
#
        icm     %r0,15,__TASK_need_resched(%r9)
        bnz     BASED(sysc_reschedule)
        icm     %r0,15,__TASK_sigpending(%r9)
        bnz     BASED(sysc_signal_return)
sysc_leave:
	stnsm   24(%r15),0xfc     # disable I/O and ext. interrupts
        RESTORE_ALL 1

#
# call do_signal before return
#
sysc_signal_return:     
        la      %r2,SP_PTREGS(%r15) # load pt_regs
        sr      %r3,%r3           # clear *oldset
        l       %r1,BASED(.Ldo_signal)
	la      %r14,BASED(sysc_leave)
        br      %r1               # return point is sysc_leave
#
# call schedule with sysc_return as return-address
#
sysc_reschedule:        
        l       %r1,BASED(.Lschedule)
	la      %r14,BASED(sysc_return)
        br      %r1               # call scheduler, return to sysc_return
#
# call trace before and after sys_call
#
sysc_tracesys:
	la	%r11,BASED(sysc_return)

#
# call syscall_trace before and after system call
# special linkage: %r11 contains the return address for trace_svc
#
trace_svc:
        l       %r1,BASED(.Ltrace)
	l       %r7,BASED(.Lc_ENOSYS)
	st      %r7,SP_R2(%r15)   # give sysc_trace an -ENOSYS retval
        basr    %r14,%r1
	l       %r2,SP_R2(%r15)
	cr      %r2,%r7		  # compare with saved -ENOSYS
	be      BASED(trace_svc_go) # strace changed the syscall ?
	sll     %r2,24
	srl     %r2,22
        l       %r8,sys_call_table-entry_base(%r2,%r13) # get system call addr.
trace_svc_go:
	lm      %r3,%r6,SP_R3(%r15)
	l       %r2,SP_ORIG_R2(%r15)
        basr    %r14,%r8          # call sys_xxx
        st      %r2,SP_R2(%r15)   # store return value
        l       %r1,BASED(.Ltrace)
	lr	%r14,%r11	  # return point is in %r11
        br      %r1

#
# a new process exits the kernel with ret_from_fork
#
        .globl  ret_from_fork
ret_from_fork:  
        basr    %r13,0
        l       %r13,.Lentry_base-.(%r13)  # setup base pointer to &entry_base
        GET_CURRENT               # load pointer to task_struct to R9
        stosm   24(%r15),0x03     # reenable interrupts
        sr      %r0,%r0           # child returns 0
        st      %r0,SP_R2(%r15)   # store return value (change R2 on stack)
        l       %r1,BASED(.Lschedtail)
	la      %r14,BASED(sysc_return)
        br      %r1               # call schedule_tail, return to sysc_return

#
# clone, fork, vfork, exec and sigreturn need glue,
# because they all expect pt_regs as parameter,
# but are called with different parameter.
# return-address is set up above
#
sys_clone_glue: 
        la      %r2,SP_PTREGS(%r15)    # load pt_regs
        l       %r1,BASED(.Lclone)
        br      %r1                   # branch to sys_clone

sys_fork_glue:  
        la      %r2,SP_PTREGS(%r15)    # load pt_regs
        l       %r1,BASED(.Lfork)
        br      %r1                   # branch to sys_fork

sys_vfork_glue: 
        la      %r2,SP_PTREGS(%r15)    # load pt_regs
        l       %r1,BASED(.Lvfork)
        br      %r1                   # branch to sys_vfork

sys_execve_glue:        
        la      %r2,SP_PTREGS(%r15)   # load pt_regs
        l       %r1,BASED(.Lexecve)
	lr      %r12,%r14             # save return address
        basr    %r14,%r1              # call sys_execve
        ltr     %r2,%r2               # check if execve failed
        bnz     0(%r12)               # it did fail -> store result in gpr2
        b       4(%r12)               # SKIP ST 2,SP_R2(15) after BASR 14,8
                                      # in system_call/sysc_tracesys

sys_sigreturn_glue:     
        la      %r2,SP_PTREGS(%r15)   # load pt_regs as parameter
        l       %r1,BASED(.Lsigreturn)
        br      %r1                   # branch to sys_sigreturn

sys_rt_sigreturn_glue:     
        la      %r2,SP_PTREGS(%r15)   # load pt_regs as parameter
        l       %r1,BASED(.Lrt_sigreturn)
        br      %r1                   # branch to sys_sigreturn

#
# sigsuspend and rt_sigsuspend need pt_regs as an additional
# parameter and they have to skip the store of %r2 into the
# user register %r2 because the return value was set in 
# sigsuspend and rt_sigsuspend already and must not be overwritten!
#

sys_sigsuspend_glue:    
        lr      %r5,%r4               # move mask back
        lr      %r4,%r3               # move history1 parameter
        lr      %r3,%r2               # move history0 parameter
        la      %r2,SP_PTREGS(%r15)   # load pt_regs as first parameter
        l       %r1,BASED(.Lsigsuspend)
	la      %r14,4(%r14)          # skip store of return value
        br      %r1                   # branch to sys_sigsuspend

sys_rt_sigsuspend_glue: 
        lr      %r4,%r3               # move sigsetsize parameter
        lr      %r3,%r2               # move unewset parameter
        la      %r2,SP_PTREGS(%r15)   # load pt_regs as first parameter
        l       %r1,BASED(.Lrt_sigsuspend)
	la      %r14,4(%r14)          # skip store of return value
        br      %r1                   # branch to sys_rt_sigsuspend

sys_sigaltstack_glue:
        la      %r4,SP_PTREGS(%r15)   # load pt_regs as parameter
        l       %r1,BASED(.Lsigaltstack)
        br      %r1                   # branch to sys_sigreturn

	.globl  sys_call_table	
sys_call_table:
        .long  sys_ni_syscall            /* 0 */
        .long  sys_exit
        .long  sys_fork_glue
        .long  sys_read
        .long  sys_write
        .long  sys_open                  /* 5 */
        .long  sys_close
        .long  sys_ni_syscall           /* old waitpid syscall holder */
        .long  sys_creat
        .long  sys_link
        .long  sys_unlink                /* 10 */
        .long  sys_execve_glue
        .long  sys_chdir
        .long  sys_time
        .long  sys_mknod
        .long  sys_chmod                /* 15 */
        .long  sys_lchown16
        .long  sys_ni_syscall           /* old break syscall holder */
        .long  sys_ni_syscall           /* old stat syscall holder */
        .long  sys_lseek
        .long  sys_getpid               /* 20 */
        .long  sys_mount
        .long  sys_oldumount
        .long  sys_setuid16
        .long  sys_getuid16
        .long  sys_stime                /* 25 */
        .long  sys_ptrace
        .long  sys_alarm
        .long  sys_ni_syscall           /* old fstat syscall holder */
        .long  sys_pause
        .long  sys_utime                /* 30 */
        .long  sys_ni_syscall           /* old stty syscall holder */
        .long  sys_ni_syscall           /* old gtty syscall holder */
        .long  sys_access
        .long  sys_nice
        .long  sys_ni_syscall           /* 35 */  /* old ftime syscall holder */
        .long  sys_sync
        .long  sys_kill
        .long  sys_rename
        .long  sys_mkdir
        .long  sys_rmdir                /* 40 */
        .long  sys_dup
        .long  sys_pipe
        .long  sys_times
        .long  sys_ni_syscall           /* old prof syscall holder */
        .long  sys_brk                  /* 45 */
        .long  sys_setgid16
        .long  sys_getgid16
        .long  sys_signal
        .long  sys_geteuid16
        .long  sys_getegid16            /* 50 */
        .long  sys_acct
        .long  sys_umount
        .long  sys_ni_syscall           /* old lock syscall holder */
        .long  sys_ioctl
        .long  sys_fcntl                /* 55 */
        .long  sys_ni_syscall           /* old mpx syscall holder */
        .long  sys_setpgid
        .long  sys_ni_syscall           /* old ulimit syscall holder */
        .long  sys_ni_syscall           /* old uname syscall holder */
        .long  sys_umask                /* 60 */
        .long  sys_chroot
        .long  sys_ustat
        .long  sys_dup2
        .long  sys_getppid
        .long  sys_getpgrp              /* 65 */
        .long  sys_setsid
        .long  sys_sigaction
        .long  sys_ni_syscall           /* old sgetmask syscall holder */
        .long  sys_ni_syscall           /* old ssetmask syscall holder */
        .long  sys_setreuid16           /* 70 */
        .long  sys_setregid16
        .long  sys_sigsuspend_glue
        .long  sys_sigpending
        .long  sys_sethostname
        .long  sys_setrlimit            /* 75 */
        .long  sys_old_getrlimit
        .long  sys_getrusage
        .long  sys_gettimeofday
        .long  sys_settimeofday
        .long  sys_getgroups16          /* 80 */
        .long  sys_setgroups16
        .long  sys_ni_syscall           /* old select syscall holder */
        .long  sys_symlink
        .long  sys_ni_syscall           /* old lstat syscall holder */
        .long  sys_readlink             /* 85 */
        .long  sys_uselib
        .long  sys_swapon
        .long  sys_reboot
        .long  sys_ni_syscall           /* old readdir syscall holder */
        .long  old_mmap                 /* 90 */
        .long  sys_munmap
        .long  sys_truncate
        .long  sys_ftruncate
        .long  sys_fchmod
        .long  sys_fchown16              /* 95 */
        .long  sys_getpriority
        .long  sys_setpriority
        .long  sys_ni_syscall            /* old profil syscall holder */
        .long  sys_statfs
        .long  sys_fstatfs               /* 100 */
        .long  sys_ioperm
        .long  sys_socketcall
        .long  sys_syslog
        .long  sys_setitimer
        .long  sys_getitimer             /* 105 */
        .long  sys_newstat
        .long  sys_newlstat
        .long  sys_newfstat
        .long  sys_ni_syscall            /* old uname syscall holder */
        .long  sys_ni_syscall            /* 110 */ /* iopl for i386 */
        .long  sys_vhangup
        .long  sys_ni_syscall            /* old "idle" system call */
        .long  sys_ni_syscall            /* vm86old for i386 */
        .long  sys_wait4
        .long  sys_swapoff               /* 115 */
        .long  sys_sysinfo
        .long  sys_ipc
        .long  sys_fsync
        .long  sys_sigreturn_glue
        .long  sys_clone_glue            /* 120 */
        .long  sys_setdomainname
        .long  sys_newuname
        .long  sys_ni_syscall            /* modify_ldt for i386 */
        .long  sys_adjtimex
        .long  sys_mprotect              /* 125 */
        .long  sys_sigprocmask
        .long  sys_create_module
        .long  sys_init_module
        .long  sys_delete_module
        .long  sys_get_kernel_syms       /* 130 */
        .long  sys_quotactl
        .long  sys_getpgid
        .long  sys_fchdir
        .long  sys_bdflush
        .long  sys_sysfs                 /* 135 */
        .long  sys_personality
        .long  sys_ni_syscall            /* for afs_syscall */
        .long  sys_setfsuid16
        .long  sys_setfsgid16
        .long  sys_llseek                /* 140 */
        .long  sys_getdents
        .long  sys_select
        .long  sys_flock
        .long  sys_msync
        .long  sys_readv                 /* 145 */
        .long  sys_writev
        .long  sys_getsid
        .long  sys_fdatasync
        .long  sys_sysctl
        .long  sys_mlock                 /* 150 */
        .long  sys_munlock
        .long  sys_mlockall
        .long  sys_munlockall
        .long  sys_sched_setparam
        .long  sys_sched_getparam        /* 155 */
        .long  sys_sched_setscheduler
        .long  sys_sched_getscheduler
        .long  sys_sched_yield
        .long  sys_sched_get_priority_max
        .long  sys_sched_get_priority_min  /* 160 */
        .long  sys_sched_rr_get_interval
        .long  sys_nanosleep
        .long  sys_mremap
        .long  sys_setresuid16
        .long  sys_getresuid16           /* 165 */
        .long  sys_ni_syscall            /* for vm86 */
        .long  sys_query_module
        .long  sys_poll
        .long  sys_nfsservctl
        .long  sys_setresgid16           /* 170 */
        .long  sys_getresgid16
        .long  sys_prctl
        .long  sys_rt_sigreturn_glue
        .long  sys_rt_sigaction
        .long  sys_rt_sigprocmask        /* 175 */
        .long  sys_rt_sigpending
        .long  sys_rt_sigtimedwait
        .long  sys_rt_sigqueueinfo
        .long  sys_rt_sigsuspend_glue
        .long  sys_pread                 /* 180 */
        .long  sys_pwrite
        .long  sys_chown16
        .long  sys_getcwd
        .long  sys_capget
        .long  sys_capset                /* 185 */
        .long  sys_sigaltstack_glue
        .long  sys_sendfile
        .long  sys_ni_syscall            /* streams1 */
        .long  sys_ni_syscall            /* streams2 */
        .long  sys_vfork_glue            /* 190 */
        .long  sys_getrlimit
	.long  sys_mmap2
        .long  sys_truncate64
        .long  sys_ftruncate64
        .long  sys_stat64                /* 195 */
        .long  sys_lstat64
        .long  sys_fstat64
	.long  sys_lchown
	.long  sys_getuid
	.long  sys_getgid		 /* 200 */
	.long  sys_geteuid
	.long  sys_getegid
	.long  sys_setreuid
	.long  sys_setregid
	.long  sys_getgroups             /* 205 */
	.long  sys_setgroups
	.long  sys_fchown
	.long  sys_setresuid
	.long  sys_getresuid
	.long  sys_setresgid             /* 210 */
	.long  sys_getresgid
	.long  sys_chown
	.long  sys_setuid
	.long  sys_setgid
	.long  sys_setfsuid              /* 215 */
	.long  sys_setfsgid
        .long  sys_pivot_root
        .long  sys_mincore
        .long  sys_madvise
	.long  sys_getdents64		 /* 220 */
        .long  sys_fcntl64 
	.long  sys_readahead
	.long  sys_ni_syscall
	.long  sys_ni_syscall		 /* 224 - reserved for setxattr  */
	.long  sys_ni_syscall            /* 225 - reserved for lsetxattr */
	.long  sys_ni_syscall		 /* 226 - reserved for fsetxattr */
	.long  sys_ni_syscall		 /* 227 - reserved for getxattr  */
	.long  sys_ni_syscall		 /* 228 - reserved for lgetxattr */
	.long  sys_ni_syscall		 /* 229 - reserved for fgetxattr */
	.long  sys_ni_syscall		 /* 230 - reserved for listxattr */
	.long  sys_ni_syscall		 /* 231 - reserved for llistxattr */
	.long  sys_ni_syscall		 /* 232 - reserved for flistxattr */
	.long  sys_ni_syscall		 /* 233 - reserved for removexattr */
	.long  sys_ni_syscall		 /* 234 - reserved for lremovexattr */
	.long  sys_ni_syscall		 /* 235 - reserved for fremovexattr */
	.long  sys_gettid
	.long  sys_tkill
	.rept  255-237
	.long  sys_ni_syscall
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
	SAVE_ALL_BASE
        tm      __LC_PGM_INT_CODE+1,0x80 # check whether we got a per exception
        bnz     BASED(pgm_per)           # got per exception -> special case
	SAVE_ALL __LC_PGM_OLD_PSW,1
        l       %r3,__LC_PGM_ILC         # load program interruption code
	la	%r8,0x7f
        l       %r7,BASED(.Ljump_table)
	nr	%r8,%r3
        sll     %r8,2
	GET_CURRENT
        l       %r7,0(%r8,%r7)		 # load address of handler routine
        la      %r2,SP_PTREGS(%r15)	 # address of register-save area
	la      %r14,BASED(sysc_return)
	br      %r7			 # branch to interrupt-handler

#
# handle per exception
#
pgm_per:
        tm      __LC_PGM_OLD_PSW,0x40    # test if per event recording is on
        bnz     BASED(pgm_per_std)       # ok, normal per event from user space
# ok its one of the special cases, now we need to find out which one
        clc     __LC_PGM_OLD_PSW(8),__LC_SVC_NEW_PSW
        be      BASED(pgm_svcper)
# no interesting special case, ignore PER event
        lm      %r13,%r15,__LC_SAVE_AREA
	lpsw    0x28

#
# Normal per exception
#
pgm_per_std:
	SAVE_ALL __LC_PGM_OLD_PSW,1
	GET_CURRENT
	la	%r4,0x7f
	l	%r3,__LC_PGM_ILC	 # load program interruption code
        nr      %r4,%r3                  # clear per-event-bit and ilc
        be      BASED(pgm_per_only)      # only per or per+check ?
        l       %r1,BASED(.Ljump_table)
        sll     %r4,2
        l       %r1,0(%r4,%r1)		 # load address of handler routine
        la      %r2,SP_PTREGS(%r15)	 # address of register-save area
	basr    %r14,%r1		 # branch to interrupt-handler
pgm_per_only:
	la      %r2,SP_PTREGS(15)	 # address of register-save area
        l       %r1,BASED(.Lhandle_per)  # load adr. of per handler
        la      %r14,BASED(sysc_return)  # load adr. of system return
        br      %r1			 # branch to handle_per_exception

#
# it was a single stepped SVC that is causing all the trouble
#
pgm_svcper:
	SAVE_ALL __LC_SVC_OLD_PSW,1
        GET_CURRENT               # load pointer to task_struct to R9
	lh	%r8,0x8a	  # get svc number from lowcore
        sll     %r8,2
        stosm   24(%r15),0x03     # reenable interrupts
        l       %r8,sys_call_table-entry_base(%r8,%r13) # get system call addr.
        tm      __TASK_ptrace+3(%r9),0x02 # PT_TRACESYS
        bnz     BASED(pgm_tracesys)
        basr    %r14,%r8          # call sys_xxxx
        st      %r2,SP_R2(%r15)   # store return value (change R2 on stack)
                                  # ATTENTION: check sys_execve_glue before
                                  # changing anything here !!

pgm_svcret:
        icm     %r0,15,__TASK_sigpending(%r9)
        bz      BASED(pgm_svcper_nosig)
	la	%r2,SP_PTREGS(%r15) # load pt_regs
	sr	%r3,%r3		  # clear *oldset
	l	%r1,BASED(.Ldo_signal)
	basr	%r4,%r1		  # call do_signal
	
pgm_svcper_nosig:
        mvi     SP_TRAP+3(%r15),0x28     # set trap indication to pgm check
	la      %r2,SP_PTREGS(15)        # address of register-save area
        l       %r1,BASED(.Lhandle_per)  # load adr. of per handler
        la      %r14,BASED(sysc_return)  # load adr. of system return
        br      %r1                      # branch to handle_per_exception
#
# call trace before and after sys_call
#
pgm_tracesys:
	la	%r11,BASED(pgm_svcret)
	b	BASED(trace_svc)

/*
 * IO interrupt handler routine
 */

        .globl io_int_handler
io_int_handler:
	SAVE_ALL_BASE
        SAVE_ALL __LC_IO_OLD_PSW,0
        GET_CURRENT               # load pointer to task_struct to R9
        l       %r1,BASED(.Ldo_IRQ)        # load address of do_IRQ
        la      %r2,SP_PTREGS(%r15) # address of register-save area
        sr      %r3,%r3
        icm     %r3,3,__LC_SUBCHANNEL_NR   # load subchannel nr & extend to int
        l       %r4,__LC_IO_INT_PARM       # load interuption parm
	l       %r5,__LC_IO_INT_WORD       # load interuption word
        basr    %r14,%r1          # branch to standard irq handler

io_return:
#
# check, if bottom-half has to be done
#
        l       %r1,__TASK_processor(%r9)
        sll     %r1,L1_CACHE_SHIFT
        al      %r1,BASED(.Lirq_stat) # get address of irq_stat
        icm     %r0,15,0(%r1)         # test irq_stat[#cpu].__softirq_pending
        bnz     BASED(io_handle_bottom_half)
io_return_bh:

        tm      SP_PSW+1(%r15),0x01 # returning to user ?
        bno     BASED(io_leave)   # no-> skip resched & signal
        stosm   24(%r15),0x03     # reenable interrupts
#
# check, if reschedule is needed
#
        icm     %r0,15,__TASK_need_resched(%r9)
        bnz     BASED(io_reschedule)
        icm     %r0,15,__TASK_sigpending(%r9)
        bnz     BASED(io_signal_return)
io_leave:
        stnsm   24(%r15),0xfc            # disable I/O and ext. interrupts
        RESTORE_ALL 0

#
# call do_softirq
#
io_handle_bottom_half:
        l       %r1,BASED(.Ldo_softirq)
        la      %r14,BASED(io_return_bh)
        br      %r1               # call do_softirq

#
# call schedule with io_return as return-address
#
io_reschedule:        
        l       %r1,BASED(.Lschedule)
	la      %r14,BASED(io_return)
        br      %r1               # call scheduler, return to io_return

#
# call do_signal before return
#
io_signal_return:     
        la      %r2,SP_PTREGS(%r15) # load pt_regs
        sr      %r3,%r3           # clear *oldset
        l       %r1,BASED(.Ldo_signal)
	la      %r14,BASED(io_leave)
        br      %r1               # return point is io_leave

/*
 * External interrupt handler routine
 */

        .globl  ext_int_handler
ext_int_handler:
	SAVE_ALL_BASE
        SAVE_ALL __LC_EXT_OLD_PSW,0
        GET_CURRENT                    # load pointer to task_struct to R9
	lh	%r6,__LC_EXT_INT_CODE  # get interruption code
	lr	%r1,%r6		       # calculate index = code & 0xff
	n	%r1,BASED(.Lc0xff)
	sll	%r1,2
	l	%r7,BASED(.Lext_hash)
	l	%r7,0(%r1,%r7)	       # get first list entry for hash value
	ltr	%r7,%r7		       # == NULL ?
	bz	BASED(io_return)       # yes, nothing to do, exit
ext_int_loop:
	ch	%r6,8(%r7)	       # compare external interrupt code
	bne	BASED(ext_int_next)
	l	%r1,4(%r7)	       # get handler address
	la	%r2,SP_PTREGS(%r15)    # address of register-save area
	lr	%r3,%r6		       # interruption code
	basr	%r14,%r1	       # call handler
ext_int_next:
	icm	%r7,15,0(%r7)	       # next list entry
	bnz	BASED(ext_int_loop)
	b	BASED(io_return)

/*
 * Machine check handler routines
 */

        .globl mcck_int_handler
mcck_int_handler:
	SAVE_ALL_BASE
        SAVE_ALL __LC_MCK_OLD_PSW,0
	l       %r1,BASED(.Ls390_mcck)
	basr    %r14,%r1	  # call machine check handler
mcck_return:
        RESTORE_ALL 0

#ifdef CONFIG_SMP
/*
 * Restart interruption handler, kick starter for additional CPUs
 */
        .globl restart_int_handler
restart_int_handler:
        l       %r15,__LC_SAVE_AREA+60 # load ksp
        lctl    %c0,%c15,__LC_CREGS_SAVE_AREA # get new ctl regs
        lam     %a0,%a15,__LC_AREGS_SAVE_AREA
        stosm   0(%r15),0x04           # now we can turn dat on
        lm      %r6,%r15,24(%r15)      # load registers from clone
        basr    %r14,0
        l       %r14,restart_addr-.(%r14)
        br      %r14                   # branch to start_secondary
restart_addr:
        .long   start_secondary
#else
/*
 * If we do not run with SMP enabled, let the new CPU crash ...
 */
        .globl restart_int_handler
restart_int_handler:
        basr    %r1,0
restart_base:
        lpsw    restart_crash-restart_base(%r1)
        .align 8
restart_crash:
        .long  0x000a0000,0x00000000
restart_go:
#endif

/*
 * Integer constants
 */
               .align 4
.Lc0xfffffff8: .long  -8           # to align stack pointer to 8
.Lc0xffffe000: .long  -8192        # to round stack pointer to &task_struct
.Lc8191:       .long  8191
.Lc_spsize:    .long  SP_SIZE
.Lc_overhead:  .long  STACK_FRAME_OVERHEAD
.Lc_ac:        .long  0,0,1
.Lc_ENOSYS:    .long  -ENOSYS
.Lc4:          .long  4
.Lc20:         .long  20
.Lc0x1202:     .long  0x1202
.Lc0x1004:     .long  0x1004
.Lc0x2401:     .long  0x2401
.Lc0x4000:     .long  0x4000
.Lc0xff:       .long  0xff
.Lc128:        .long  128

/*
 * Symbol constants
 */
.Ls390_mcck:   .long  s390_do_machine_check
.Ldo_IRQ:      .long  do_IRQ
.Ldo_signal:   .long  do_signal
.Ldo_softirq:  .long  do_softirq
.Lentry_base:  .long  entry_base
.Lext_hash:    .long  ext_int_hash
.Lhandle_per:  .long  handle_per_exception
.Lirq_stat:    .long  irq_stat
.Ljump_table:  .long  pgm_check_table
.Lschedule:    .long  schedule
.Lclone:       .long  sys_clone
.Lexecve:      .long  sys_execve
.Lfork:        .long  sys_fork
.Lrt_sigreturn:.long  sys_rt_sigreturn
.Lrt_sigsuspend:
               .long  sys_rt_sigsuspend
.Lsigreturn:   .long  sys_sigreturn
.Lsigsuspend:  .long  sys_sigsuspend
.Lsigaltstack: .long  sys_sigaltstack
.Ltrace:       .long  syscall_trace
.Lvfork:       .long  sys_vfork

.Lschedtail:   .long  schedule_tail

