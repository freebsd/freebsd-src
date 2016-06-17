/*
 * Compatibility mode system call entry point for x86-64. 
 * 		
 * Copyright 2000,2001 Andi Kleen, SuSE Labs.
 * 
 * $Id: ia32entry.S,v 1.42 2003/11/27 00:55:43 ak Exp $ 		
 */		 

#include <asm/calling.h>
#include <asm/offset.h>
#include <asm/current.h>
#include <linux/linkage.h>
#include <asm/errno.h>
#include <asm/ia32_unistd.h>	

	.macro IA32_ARG_FIXUP
	movl	%edi,%r8d
	movl	%ebp,%r9d
	xchg	%ecx,%esi
	movl	%ebx,%edi
	movl	%edx,%edx	/* zero extension */
	.endm 

/*
 * 32bit SYSCALL instruction entry. 	
 */ 	
ENTRY(ia32_cstar_target)
	movq $-ENOSYS,%rax
	sysretl	
	
/* 
 * Emulated IA32 system calls via int 0x80. 
 *
 * Arguments:	 
 * %eax	System call number.
 * %ebx Arg1
 * %ecx Arg2
 * %edx Arg3
 * %esi Arg4
 * %edi Arg5
 * %ebp Arg6    [note: not saved in the stack frame, should not be touched]
 *
 * Notes:
 * Uses the same stack frame as the x86-64 version.	
 * All registers except %eax must be saved (but ptrace may violate that)
 * Arguments are zero extended. For system calls that want sign extension and
 * take long arguments a wrapper is needed. Most calls can just be called
 * directly.
 * Assumes it is only called from user space and entered with interrups off.		
 */ 				

ENTRY(ia32_syscall)
	swapgs	
	sti
	pushq %rax
	cld
	SAVE_ARGS
	GET_CURRENT(%r10)
	testl $PT_TRACESYS,tsk_ptrace(%r10) 
	jne  ia32_tracesys
	cmpl $(IA32_NR_syscalls),%eax
	jae  ia32_badsys
	IA32_ARG_FIXUP
	call *ia32_sys_call_table(,%rax,8) # xxx: rip relative
	movq %rax,RAX-ARGOFFSET(%rsp)
	jmp int_ret_from_sys_call 

ia32_tracesys:			 
	SAVE_REST
	movq $-ENOSYS,RAX(%rsp)
	movq %rsp,%rdi        /* &pt_regs -> arg1 */
	call syscall_trace
	LOAD_ARGS ARGOFFSET  /* reload args from stack in case ptrace changed it */
	addq $ARGOFFSET,%rsp	
	cmpl $(IA32_NR_syscalls),%eax
	jae  1f
	IA32_ARG_FIXUP
	call *ia32_sys_call_table(,%rax,8)
ia32_tracesys_end:	
	movq %rax,RAX-ARGOFFSET(%rsp)
1:	SAVE_REST
	movq %rsp,%rdi		/* &pt_regs -> arg1 */ 
	call syscall_trace
	addq $ARGOFFSET,%rsp
	jmp int_ret_from_sys_call
		
ia32_badsys:
	movq $-ENOSYS,RAX-ARGOFFSET(%rsp)
	movq $0,ORIG_RAX-ARGOFFSET(%rsp)
	jmp int_ret_from_sys_call

ni_syscall:
	movq %rax,%rdi
	jmp  sys32_ni_syscall			

quiet_ni_syscall:
	movl $-ENOSYS,%eax
	ret	
	
	.macro PTREGSCALL label, func
	.globl \label
\label:
	leaq \func(%rip),%rax
	jmp  ia32_ptregs_common	
	.endm

	PTREGSCALL stub32_rt_sigreturn, sys32_rt_sigreturn
	PTREGSCALL stub32_sigreturn, sys32_sigreturn
	PTREGSCALL stub32_sigaltstack, sys32_sigaltstack
	PTREGSCALL stub32_sigsuspend, sys32_sigsuspend
	PTREGSCALL stub32_execve, sys32_execve
	PTREGSCALL stub32_fork, sys32_fork
	PTREGSCALL stub32_clone, sys32_clone
	PTREGSCALL stub32_vfork, sys32_vfork
	PTREGSCALL stub32_iopl, sys_iopl
	PTREGSCALL stub32_rt_sigsuspend, sys_rt_sigsuspend

ENTRY(ia32_ptregs_common)
	popq %r11
	SAVE_REST
	movq %r11, %r15
	call *%rax
	movq %r15, %r11
	RESTORE_REST
	pushq %r11
	ret

	.data
	.align 8
	.globl ia32_sys_call_table
ia32_sys_call_table:
	.quad ni_syscall	/* 0  -  old "setup" system call*/
	.quad sys_exit
	.quad stub32_fork
	.quad sys_read
	.quad sys_write
	.quad sys32_open		/* 5 */
	.quad sys_close
	.quad sys32_waitpid
	.quad sys_creat
	.quad sys_link
	.quad sys_unlink		/* 10 */
	.quad stub32_execve
	.quad sys_chdir
	.quad sys32_time
	.quad sys_mknod
	.quad sys_chmod		/* 15 */
	.quad sys_lchown16
	.quad ni_syscall			/* old break syscall holder */
	.quad ni_syscall	/* (old)stat */ 
	.quad sys32_lseek
	.quad sys_getpid		/* 20 */
	.quad sys_mount	/* mount  */
	.quad sys_oldumount	/* old_umount  */
	.quad sys_setuid16
	.quad sys_getuid16
	.quad sys_stime		/* stime */		/* 25 */
	.quad sys32_ptrace	/* ptrace */
	.quad sys_alarm		/* XXX sign extension??? */ 
	.quad ni_syscall	/* (old)fstat */
	.quad sys_pause
	.quad sys32_utime	/* 30 */
	.quad ni_syscall	/* old stty syscall holder */
	.quad ni_syscall	/* old gtty syscall holder */
	.quad sys_access
	.quad sys_nice	
	.quad ni_syscall	/* 35 */	/* old ftime syscall holder */
	.quad sys_sync
	.quad sys32_kill
	.quad sys_rename
	.quad sys_mkdir
	.quad sys_rmdir		/* 40 */
	.quad sys_dup
	.quad sys32_pipe
	.quad sys32_times
	.quad ni_syscall			/* old prof syscall holder */
	.quad sys_brk		/* 45 */
	.quad sys_setgid16
	.quad sys_getgid16
	.quad sys_signal
	.quad sys_geteuid16
	.quad sys_getegid16	/* 50 */
	.quad sys_acct
	.quad sys_umount			/* new_umount */
	.quad ni_syscall			/* old lock syscall holder */
	.quad sys32_ioctl
	.quad sys32_fcntl		/* 55 */
	.quad ni_syscall			/* old mpx syscall holder */
	.quad sys_setpgid
	.quad ni_syscall			/* old ulimit syscall holder */
	.quad sys32_olduname
	.quad sys_umask		/* 60 */
	.quad sys_chroot
	.quad sys32_ustat
	.quad sys_dup2
	.quad sys_getppid
	.quad sys_getpgrp		/* 65 */
	.quad sys_setsid
	.quad sys32_sigaction
	.quad sys_sgetmask
	.quad sys_ssetmask
	.quad sys_setreuid16	/* 70 */
	.quad sys_setregid16
	.quad stub32_sigsuspend
	.quad sys32_sigpending
	.quad sys_sethostname
	.quad sys32_setrlimit	/* 75 */
	.quad sys32_old_getrlimit	/* old_getrlimit */
	.quad sys32_getrusage
	.quad sys32_gettimeofday
	.quad sys32_settimeofday
	.quad sys_getgroups16	/* 80 */
	.quad sys_setgroups16
	.quad sys32_old_select
	.quad sys_symlink
	.quad ni_syscall	/* (old)lstat */
	.quad sys_readlink		/* 85 */
	.quad sys_uselib
	.quad sys_swapon
	.quad sys_reboot
	.quad sys32_oldreaddir
	.quad sys32_mmap		/* 90 */
	.quad sys_munmap
	.quad sys_truncate
	.quad sys_ftruncate
	.quad sys_fchmod
	.quad sys_fchown16		/* 95 */
	.quad sys_getpriority
	.quad sys_setpriority
	.quad ni_syscall			/* old profil syscall holder */
	.quad sys32_statfs
	.quad sys32_fstatfs		/* 100 */
	.quad sys_ioperm
	.quad sys32_socketcall
	.quad sys_syslog
	.quad sys32_setitimer
	.quad sys32_getitimer	/* 105 */
	.quad sys32_newstat
	.quad sys32_newlstat
	.quad sys32_newfstat
	.quad sys32_uname
	.quad stub32_iopl		/* 110 */
	.quad sys_vhangup
	.quad ni_syscall	/* old "idle" system call */
	.quad sys32_vm86_warning	/* vm86old */ 
	.quad sys32_wait4
	.quad sys_swapoff		/* 115 */
	.quad sys32_sysinfo
	.quad sys32_ipc
	.quad sys_fsync
	.quad stub32_sigreturn
	.quad stub32_clone		/* 120 */
	.quad sys_setdomainname
	.quad sys_uname
	.quad sys32_modify_ldt
	.quad sys32_adjtimex
	.quad sys32_mprotect		/* 125 */
	.quad sys32_sigprocmask
	.quad sys32_create_module
	.quad sys32_init_module
	.quad sys32_delete_module
	.quad sys32_get_kernel_syms 	/* 130 */
	.quad ni_syscall	/* quotactl */ 
	.quad sys_getpgid
	.quad sys_fchdir
	.quad ni_syscall	/* bdflush */
	.quad sys_sysfs		/* 135 */
	.quad sys_personality
	.quad ni_syscall	/* for afs_syscall */
	.quad sys_setfsuid16
	.quad sys_setfsgid16
	.quad sys_llseek		/* 140 */
	.quad sys32_getdents
	.quad sys32_select
	.quad sys_flock
	.quad sys_msync
	.quad sys32_readv		/* 145 */
	.quad sys32_writev
	.quad sys_getsid
	.quad sys_fdatasync
	.quad sys32_sysctl	/* sysctl */
	.quad sys_mlock		/* 150 */
	.quad sys_munlock
	.quad sys_mlockall
	.quad sys_munlockall
	.quad sys_sched_setparam
	.quad sys_sched_getparam   /* 155 */
	.quad sys_sched_setscheduler
	.quad sys_sched_getscheduler
	.quad sys_sched_yield
	.quad sys_sched_get_priority_max
	.quad sys_sched_get_priority_min  /* 160 */
	.quad sys_sched_rr_get_interval
	.quad sys32_nanosleep
	.quad sys_mremap
	.quad sys_setresuid16
	.quad sys_getresuid16	/* 165 */
	.quad sys32_vm86_warning	/* vm86 */ 
	.quad sys32_query_module
	.quad sys_poll
	.quad sys32_nfsservctl
	.quad sys_setresgid16	/* 170 */
	.quad sys_getresgid16
	.quad sys_prctl
	.quad stub32_rt_sigreturn
	.quad sys32_rt_sigaction
	.quad sys32_rt_sigprocmask	/* 175 */
	.quad sys32_rt_sigpending
	.quad sys32_rt_sigtimedwait
	.quad sys32_rt_sigqueueinfo
	.quad stub32_rt_sigsuspend
	.quad sys32_pread		/* 180 */
	.quad sys32_pwrite
	.quad sys_chown16
	.quad sys_getcwd
	.quad sys_capget
	.quad sys_capset
	.quad stub32_sigaltstack
	.quad sys32_sendfile
	.quad ni_syscall		/* streams1 */
	.quad ni_syscall		/* streams2 */
	.quad stub32_vfork            /* 190 */
	.quad sys32_getrlimit
	.quad sys32_mmap2
	.quad sys32_truncate64
	.quad sys32_ftruncate64
	.quad sys32_stat64		/* 195 */
	.quad sys32_lstat64
	.quad sys32_fstat64
	.quad sys_lchown
	.quad sys_getuid
	.quad sys_getgid		/* 200 */
	.quad sys_geteuid
	.quad sys_getegid
	.quad sys_setreuid
	.quad sys_setregid
	.quad sys_getgroups	/* 205 */
	.quad sys_setgroups
	.quad sys_fchown
	.quad sys_setresuid
	.quad sys_getresuid
	.quad sys_setresgid	/* 210 */
	.quad sys_getresgid
	.quad sys_chown
	.quad sys_setuid
	.quad sys_setgid
	.quad sys_setfsuid		/* 215 */
	.quad sys_setfsgid
	.quad sys_pivot_root
	.quad sys_mincore
	.quad sys_madvise
	.quad sys_getdents64	/* 220 getdents64 */ 
	.quad sys32_fcntl64	
	.quad sys_ni_syscall	/* tux */
	.quad sys_ni_syscall    /* security */
	.quad sys_gettid	
	.quad sys_readahead	/* 225 */ 
	.quad quiet_ni_syscall  /* xattr syscalls 226-237 */
	.quad quiet_ni_syscall
	.quad quiet_ni_syscall
	.quad quiet_ni_syscall
	.quad quiet_ni_syscall  /* 230 */
	.quad quiet_ni_syscall  
	.quad quiet_ni_syscall  
	.quad quiet_ni_syscall  
	.quad quiet_ni_syscall  
	.quad quiet_ni_syscall  /* 235 */
	.quad quiet_ni_syscall  
	.quad quiet_ni_syscall  /* fremovexattr - 237 */
	.quad sys_tkill
	.quad sys_sendfile64
	.quad quiet_ni_syscall  /* futex */
	.quad quiet_ni_syscall  /* sched_setaffinity */
	.quad quiet_ni_syscall  /* sched_getaffinity */	
ia32_syscall_end:		
	.rept IA32_NR_syscalls-(ia32_syscall_end-ia32_sys_call_table)/8
		.quad ni_syscall
	.endr

	
