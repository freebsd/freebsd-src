/*
 *  linux/arch/arm/kernel/calls.S
 *
 *  Copyright (C) 1995-2003 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  This file is included twice in entry-common.S
 */
#ifndef NR_syscalls
#define NR_syscalls 288
#else

__syscall_start:
/* 0 */		.long	SYMBOL_NAME(sys_ni_syscall)
		.long	SYMBOL_NAME(sys_exit)
		.long	SYMBOL_NAME(sys_fork_wrapper)
		.long	SYMBOL_NAME(sys_read)
		.long	SYMBOL_NAME(sys_write)
/* 5 */		.long	SYMBOL_NAME(sys_open)
		.long	SYMBOL_NAME(sys_close)
		.long	SYMBOL_NAME(sys_ni_syscall)		/* was sys_waitpid */
		.long	SYMBOL_NAME(sys_creat)
		.long	SYMBOL_NAME(sys_link)
/* 10 */	.long	SYMBOL_NAME(sys_unlink)
		.long	SYMBOL_NAME(sys_execve_wrapper)
		.long	SYMBOL_NAME(sys_chdir)
		.long	SYMBOL_NAME(sys_time)			/* used by libc4 */
		.long	SYMBOL_NAME(sys_mknod)
/* 15 */	.long	SYMBOL_NAME(sys_chmod)
		.long	SYMBOL_NAME(sys_lchown16)
		.long	SYMBOL_NAME(sys_ni_syscall)		/* was sys_break */
		.long	SYMBOL_NAME(sys_ni_syscall)		/* was sys_stat */
		.long	SYMBOL_NAME(sys_lseek)
/* 20 */	.long	SYMBOL_NAME(sys_getpid)
		.long	SYMBOL_NAME(sys_mount)
		.long	SYMBOL_NAME(sys_oldumount)		/* used by libc4 */
		.long	SYMBOL_NAME(sys_setuid16)
		.long	SYMBOL_NAME(sys_getuid16)
/* 25 */	.long	SYMBOL_NAME(sys_stime)
		.long	SYMBOL_NAME(sys_ptrace)
		.long	SYMBOL_NAME(sys_alarm)			/* used by libc4 */
		.long	SYMBOL_NAME(sys_ni_syscall)		/* was sys_fstat */
		.long	SYMBOL_NAME(sys_pause)
/* 30 */	.long	SYMBOL_NAME(sys_utime)			/* used by libc4 */
		.long	SYMBOL_NAME(sys_ni_syscall)		/* was sys_stty */
		.long	SYMBOL_NAME(sys_ni_syscall)		/* was sys_getty */
		.long	SYMBOL_NAME(sys_access)
		.long	SYMBOL_NAME(sys_nice)
/* 35 */	.long	SYMBOL_NAME(sys_ni_syscall)		/* was sys_ftime */
		.long	SYMBOL_NAME(sys_sync)
		.long	SYMBOL_NAME(sys_kill)
		.long	SYMBOL_NAME(sys_rename)
		.long	SYMBOL_NAME(sys_mkdir)
/* 40 */	.long	SYMBOL_NAME(sys_rmdir)
		.long	SYMBOL_NAME(sys_dup)
		.long	SYMBOL_NAME(sys_pipe)
		.long	SYMBOL_NAME(sys_times)
		.long	SYMBOL_NAME(sys_ni_syscall)		/* was sys_prof */
/* 45 */	.long	SYMBOL_NAME(sys_brk)
		.long	SYMBOL_NAME(sys_setgid16)
		.long	SYMBOL_NAME(sys_getgid16)
		.long	SYMBOL_NAME(sys_ni_syscall)		/* was sys_signal */
		.long	SYMBOL_NAME(sys_geteuid16)
/* 50 */	.long	SYMBOL_NAME(sys_getegid16)
		.long	SYMBOL_NAME(sys_acct)
		.long	SYMBOL_NAME(sys_umount)
		.long	SYMBOL_NAME(sys_ni_syscall)		/* was sys_lock */
		.long	SYMBOL_NAME(sys_ioctl)
/* 55 */	.long	SYMBOL_NAME(sys_fcntl)
		.long	SYMBOL_NAME(sys_ni_syscall)		/* was sys_mpx */
		.long	SYMBOL_NAME(sys_setpgid)
		.long	SYMBOL_NAME(sys_ni_syscall)		/* was sys_ulimit */
		.long	SYMBOL_NAME(sys_ni_syscall)		/* was sys_olduname */
/* 60 */	.long	SYMBOL_NAME(sys_umask)
		.long	SYMBOL_NAME(sys_chroot)
		.long	SYMBOL_NAME(sys_ustat)
		.long	SYMBOL_NAME(sys_dup2)
		.long	SYMBOL_NAME(sys_getppid)
/* 65 */	.long	SYMBOL_NAME(sys_getpgrp)
		.long	SYMBOL_NAME(sys_setsid)
		.long	SYMBOL_NAME(sys_sigaction)
		.long	SYMBOL_NAME(sys_ni_syscall)		/* was sys_sgetmask */
		.long	SYMBOL_NAME(sys_ni_syscall)		/* was sys_ssetmask */
/* 70 */	.long	SYMBOL_NAME(sys_setreuid16)
		.long	SYMBOL_NAME(sys_setregid16)
		.long	SYMBOL_NAME(sys_sigsuspend_wrapper)
		.long	SYMBOL_NAME(sys_sigpending)
		.long	SYMBOL_NAME(sys_sethostname)
/* 75 */	.long	SYMBOL_NAME(sys_setrlimit)
		.long	SYMBOL_NAME(sys_old_getrlimit)		/* used by libc4 */
		.long	SYMBOL_NAME(sys_getrusage)
		.long	SYMBOL_NAME(sys_gettimeofday)
		.long	SYMBOL_NAME(sys_settimeofday)
/* 80 */	.long	SYMBOL_NAME(sys_getgroups16)
		.long	SYMBOL_NAME(sys_setgroups16)
		.long	SYMBOL_NAME(old_select)			/* used by libc4 */
		.long	SYMBOL_NAME(sys_symlink)
		.long	SYMBOL_NAME(sys_ni_syscall)		/* was sys_lstat */
/* 85 */	.long	SYMBOL_NAME(sys_readlink)
		.long	SYMBOL_NAME(sys_uselib)
		.long	SYMBOL_NAME(sys_swapon)
		.long	SYMBOL_NAME(sys_reboot)
		.long	SYMBOL_NAME(old_readdir)		/* used by libc4 */
/* 90 */	.long	SYMBOL_NAME(old_mmap)			/* used by libc4 */
		.long	SYMBOL_NAME(sys_munmap)
		.long	SYMBOL_NAME(sys_truncate)
		.long	SYMBOL_NAME(sys_ftruncate)
		.long	SYMBOL_NAME(sys_fchmod)
/* 95 */	.long	SYMBOL_NAME(sys_fchown16)
		.long	SYMBOL_NAME(sys_getpriority)
		.long	SYMBOL_NAME(sys_setpriority)
		.long	SYMBOL_NAME(sys_ni_syscall)		/* was sys_profil */
		.long	SYMBOL_NAME(sys_statfs)
/* 100 */	.long	SYMBOL_NAME(sys_fstatfs)
		.long	SYMBOL_NAME(sys_ni_syscall)
		.long	SYMBOL_NAME(sys_socketcall)
		.long	SYMBOL_NAME(sys_syslog)
		.long	SYMBOL_NAME(sys_setitimer)
/* 105 */	.long	SYMBOL_NAME(sys_getitimer)
		.long	SYMBOL_NAME(sys_newstat)
		.long	SYMBOL_NAME(sys_newlstat)
		.long	SYMBOL_NAME(sys_newfstat)
		.long	SYMBOL_NAME(sys_ni_syscall)		/* was sys_uname */
/* 110 */	.long	SYMBOL_NAME(sys_ni_syscall)		/* was sys_iopl */
		.long	SYMBOL_NAME(sys_vhangup)
		.long	SYMBOL_NAME(sys_ni_syscall)
		.long	SYMBOL_NAME(sys_syscall)		/* call a syscall */
		.long	SYMBOL_NAME(sys_wait4)
/* 115 */	.long	SYMBOL_NAME(sys_swapoff)
		.long	SYMBOL_NAME(sys_sysinfo)
		.long	SYMBOL_NAME(sys_ipc)
		.long	SYMBOL_NAME(sys_fsync)
		.long	SYMBOL_NAME(sys_sigreturn_wrapper)
/* 120 */	.long	SYMBOL_NAME(sys_clone_wapper)
		.long	SYMBOL_NAME(sys_setdomainname)
		.long	SYMBOL_NAME(sys_newuname)
		.long	SYMBOL_NAME(sys_ni_syscall)
		.long	SYMBOL_NAME(sys_adjtimex)
/* 125 */	.long	SYMBOL_NAME(sys_mprotect)
		.long	SYMBOL_NAME(sys_sigprocmask)
		.long	SYMBOL_NAME(sys_create_module)
		.long	SYMBOL_NAME(sys_init_module)
		.long	SYMBOL_NAME(sys_delete_module)
/* 130 */	.long	SYMBOL_NAME(sys_get_kernel_syms)
		.long	SYMBOL_NAME(sys_quotactl)
		.long	SYMBOL_NAME(sys_getpgid)
		.long	SYMBOL_NAME(sys_fchdir)
		.long	SYMBOL_NAME(sys_bdflush)
/* 135 */	.long	SYMBOL_NAME(sys_sysfs)
		.long	SYMBOL_NAME(sys_personality)
		.long	SYMBOL_NAME(sys_ni_syscall)		/* .long	_sys_afs_syscall */
		.long	SYMBOL_NAME(sys_setfsuid16)
		.long	SYMBOL_NAME(sys_setfsgid16)
/* 140 */	.long	SYMBOL_NAME(sys_llseek)
		.long	SYMBOL_NAME(sys_getdents)
		.long	SYMBOL_NAME(sys_select)
		.long	SYMBOL_NAME(sys_flock)
		.long	SYMBOL_NAME(sys_msync)
/* 145 */	.long	SYMBOL_NAME(sys_readv)
		.long	SYMBOL_NAME(sys_writev)
		.long	SYMBOL_NAME(sys_getsid)
		.long	SYMBOL_NAME(sys_fdatasync)
		.long	SYMBOL_NAME(sys_sysctl)
/* 150 */	.long	SYMBOL_NAME(sys_mlock)
		.long	SYMBOL_NAME(sys_munlock)
		.long	SYMBOL_NAME(sys_mlockall)
		.long	SYMBOL_NAME(sys_munlockall)
		.long	SYMBOL_NAME(sys_sched_setparam)
/* 155 */	.long	SYMBOL_NAME(sys_sched_getparam)
		.long	SYMBOL_NAME(sys_sched_setscheduler)
		.long	SYMBOL_NAME(sys_sched_getscheduler)
		.long	SYMBOL_NAME(sys_sched_yield)
		.long	SYMBOL_NAME(sys_sched_get_priority_max)
/* 160 */	.long	SYMBOL_NAME(sys_sched_get_priority_min)
		.long	SYMBOL_NAME(sys_sched_rr_get_interval)
		.long	SYMBOL_NAME(sys_nanosleep)
		.long	SYMBOL_NAME(sys_arm_mremap)
		.long	SYMBOL_NAME(sys_setresuid16)
/* 165 */	.long	SYMBOL_NAME(sys_getresuid16)
		.long	SYMBOL_NAME(sys_ni_syscall)
		.long	SYMBOL_NAME(sys_query_module)
		.long	SYMBOL_NAME(sys_poll)
		.long	SYMBOL_NAME(sys_nfsservctl)
/* 170 */	.long	SYMBOL_NAME(sys_setresgid16)
		.long	SYMBOL_NAME(sys_getresgid16)
		.long	SYMBOL_NAME(sys_prctl)
		.long	SYMBOL_NAME(sys_rt_sigreturn_wrapper)
		.long	SYMBOL_NAME(sys_rt_sigaction)
/* 175 */	.long	SYMBOL_NAME(sys_rt_sigprocmask)
		.long	SYMBOL_NAME(sys_rt_sigpending)
		.long	SYMBOL_NAME(sys_rt_sigtimedwait)
		.long	SYMBOL_NAME(sys_rt_sigqueueinfo)
		.long	SYMBOL_NAME(sys_rt_sigsuspend_wrapper)
/* 180 */	.long	SYMBOL_NAME(sys_pread)
		.long	SYMBOL_NAME(sys_pwrite)
		.long	SYMBOL_NAME(sys_chown16)
		.long	SYMBOL_NAME(sys_getcwd)
		.long	SYMBOL_NAME(sys_capget)
/* 185 */	.long	SYMBOL_NAME(sys_capset)
		.long	SYMBOL_NAME(sys_sigaltstack_wrapper)
		.long	SYMBOL_NAME(sys_sendfile)
		.long	SYMBOL_NAME(sys_ni_syscall)
		.long	SYMBOL_NAME(sys_ni_syscall)
/* 190 */	.long	SYMBOL_NAME(sys_vfork_wrapper)
		.long	SYMBOL_NAME(sys_getrlimit)
		.long	SYMBOL_NAME(sys_mmap2)
		.long	SYMBOL_NAME(sys_truncate64)
		.long	SYMBOL_NAME(sys_ftruncate64)
/* 195 */	.long	SYMBOL_NAME(sys_stat64)
		.long	SYMBOL_NAME(sys_lstat64)
		.long	SYMBOL_NAME(sys_fstat64)
		.long	SYMBOL_NAME(sys_lchown)
		.long	SYMBOL_NAME(sys_getuid)
/* 200 */	.long	SYMBOL_NAME(sys_getgid)
		.long	SYMBOL_NAME(sys_geteuid)
		.long	SYMBOL_NAME(sys_getegid)
		.long	SYMBOL_NAME(sys_setreuid)
		.long	SYMBOL_NAME(sys_setregid)
/* 205 */	.long	SYMBOL_NAME(sys_getgroups)
		.long	SYMBOL_NAME(sys_setgroups)
		.long	SYMBOL_NAME(sys_fchown)
		.long	SYMBOL_NAME(sys_setresuid)
		.long	SYMBOL_NAME(sys_getresuid)
/* 210 */	.long	SYMBOL_NAME(sys_setresgid)
		.long	SYMBOL_NAME(sys_getresgid)
		.long	SYMBOL_NAME(sys_chown)
		.long	SYMBOL_NAME(sys_setuid)
		.long	SYMBOL_NAME(sys_setgid)
/* 215 */	.long	SYMBOL_NAME(sys_setfsuid)
		.long	SYMBOL_NAME(sys_setfsgid)
		.long	SYMBOL_NAME(sys_getdents64)
		.long	SYMBOL_NAME(sys_pivot_root)
		.long	SYMBOL_NAME(sys_mincore)
/* 220 */	.long	SYMBOL_NAME(sys_madvise)
		.long	SYMBOL_NAME(sys_fcntl64)
		.long	SYMBOL_NAME(sys_ni_syscall) /* TUX */
		.long	SYMBOL_NAME(sys_ni_syscall) /* Security */
		.long	SYMBOL_NAME(sys_gettid)
/* 225 */	.long	SYMBOL_NAME(sys_readahead)
		.long	SYMBOL_NAME(sys_ni_syscall) /* setxattr */
		.long	SYMBOL_NAME(sys_ni_syscall) /* lsetxattr */
		.long	SYMBOL_NAME(sys_ni_syscall) /* fsetxattr */
		.long	SYMBOL_NAME(sys_ni_syscall) /* getxattr */
/* 230 */	.long	SYMBOL_NAME(sys_ni_syscall) /* lgetxattr */
		.long	SYMBOL_NAME(sys_ni_syscall) /* fgetxattr */
		.long	SYMBOL_NAME(sys_ni_syscall) /* listxattr */
		.long	SYMBOL_NAME(sys_ni_syscall) /* llistxattr */
		.long	SYMBOL_NAME(sys_ni_syscall) /* flistxattr */
/* 235 */	.long	SYMBOL_NAME(sys_ni_syscall) /* removexattr */
		.long	SYMBOL_NAME(sys_ni_syscall) /* lremovexattr */
		.long	SYMBOL_NAME(sys_ni_syscall) /* fremovexattr */
		.long	SYMBOL_NAME(sys_tkill)
		.long	SYMBOL_NAME(sys_ni_syscall) /* sendfile64 */
/* 240 */	.long	SYMBOL_NAME(sys_ni_syscall) /* futex */
		.long	SYMBOL_NAME(sys_ni_syscall) /* sched_setaffinity */
		.long	SYMBOL_NAME(sys_ni_syscall) /* sched_getaffinity */
		.long	SYMBOL_NAME(sys_ni_syscall) /* io_setup */
		.long	SYMBOL_NAME(sys_ni_syscall) /* io_destroy */
/* 245 */	.long	SYMBOL_NAME(sys_ni_syscall) /* io_getevents */
		.long	SYMBOL_NAME(sys_ni_syscall) /* io_submit */
		.long	SYMBOL_NAME(sys_ni_syscall) /* io_cancel */
		.long	SYMBOL_NAME(sys_ni_syscall) /* exit_group */
		.long	SYMBOL_NAME(sys_ni_syscall) /* lookup_dcookie */
/* 250 */	.long	SYMBOL_NAME(sys_ni_syscall) /* epoll_create */
		.long	SYMBOL_NAME(sys_ni_syscall) /* epoll_ctl */
		.long	SYMBOL_NAME(sys_ni_syscall) /* epoll_wait */
		.long	SYMBOL_NAME(sys_ni_syscall) /* remap_file_pages */
		.long	SYMBOL_NAME(sys_ni_syscall) /* set_thread_area */
/* 255 */	.long	SYMBOL_NAME(sys_ni_syscall) /* get_thread_area */
		.long	SYMBOL_NAME(sys_ni_syscall) /* set_tid_address */
__syscall_end:

		.rept	NR_syscalls - (__syscall_end - __syscall_start) / 4
			.long	SYMBOL_NAME(sys_ni_syscall)
		.endr
#endif
