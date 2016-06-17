/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995, 96, 97, 98, 99, 2000, 2001, 2002 by Ralf Baechle
 */

/*
 * This file is being included twice - once to build a list of all
 * syscalls and once to build a table of how many arguments each syscall
 * accepts.  Syscalls that receive a pointer to the saved registers are
 * marked as having zero arguments.
 *
 * The binary compatibility calls are in a separate list.
 */
SYS(sys_syscall, 0)				/* 4000 */
SYS(sys_exit, 1)
SYS(sys_fork, 0)
SYS(sys_read, 3)
SYS(sys_write, 3)
SYS(sys_open, 3)				/* 4005 */
SYS(sys_close, 1)
SYS(sys_waitpid, 3)
SYS(sys_creat, 2)
SYS(sys_link, 2)
SYS(sys_unlink, 1)				/* 4010 */
SYS(sys_execve, 0)
SYS(sys_chdir, 1)
SYS(sys_time, 1)
SYS(sys_mknod, 3)
SYS(sys_chmod, 2)				/* 4015 */
SYS(sys_lchown, 3)
SYS(sys_ni_syscall, 0)
SYS(sys_ni_syscall, 0)				/* was sys_stat */
SYS(sys_lseek, 3)
SYS(sys_getpid, 0)				/* 4020 */
SYS(sys_mount, 5)
SYS(sys_oldumount, 1)
SYS(sys_setuid, 1)
SYS(sys_getuid, 0)
SYS(sys_stime, 1)				/* 4025 */
SYS(sys_ptrace, 4)
SYS(sys_alarm, 1)
SYS(sys_ni_syscall, 0)				/* was sys_fstat */
SYS(sys_pause, 0)
SYS(sys_utime, 2)				/* 4030 */
SYS(sys_ni_syscall, 0)
SYS(sys_ni_syscall, 0)
SYS(sys_access, 2)
SYS(sys_nice, 1)
SYS(sys_ni_syscall, 0)				/* 4035 */
SYS(sys_sync, 0)
SYS(sys_kill, 2)
SYS(sys_rename, 2)
SYS(sys_mkdir, 2)
SYS(sys_rmdir, 1)				/* 4040 */
SYS(sys_dup, 1)
SYS(sys_pipe, 0)
SYS(sys_times, 1)
SYS(sys_ni_syscall, 0)
SYS(sys_brk, 1)					/* 4045 */
SYS(sys_setgid, 1)
SYS(sys_getgid, 0)
SYS(sys_ni_syscall, 0)	/* was signal(2) */
SYS(sys_geteuid, 0)
SYS(sys_getegid, 0)				/* 4050 */
SYS(sys_acct, 0)
SYS(sys_umount, 2)
SYS(sys_ni_syscall, 0)
SYS(sys_ioctl, 3)
SYS(sys_fcntl, 3)				/* 4055 */
SYS(sys_ni_syscall, 2)
SYS(sys_setpgid, 2)
SYS(sys_ni_syscall, 0)
SYS(sys_olduname, 1)
SYS(sys_umask, 1)				/* 4060 */
SYS(sys_chroot, 1)
SYS(sys_ustat, 2)
SYS(sys_dup2, 2)
SYS(sys_getppid, 0)
SYS(sys_getpgrp, 0)				/* 4065 */
SYS(sys_setsid, 0)
SYS(sys_sigaction, 3)
SYS(sys_sgetmask, 0)
SYS(sys_ssetmask, 1)
SYS(sys_setreuid, 2)				/* 4070 */
SYS(sys_setregid, 2)
SYS(sys_sigsuspend, 0)
SYS(sys_sigpending, 1)
SYS(sys_sethostname, 2)
SYS(sys_setrlimit, 2)				/* 4075 */
SYS(sys_getrlimit, 2)
SYS(sys_getrusage, 2)
SYS(sys_gettimeofday, 2)
SYS(sys_settimeofday, 2)
SYS(sys_getgroups, 2)				/* 4080 */
SYS(sys_setgroups, 2)
SYS(sys_ni_syscall, 0)				/* old_select */
SYS(sys_symlink, 2)
SYS(sys_ni_syscall, 0)				/* was sys_lstat */
SYS(sys_readlink, 3)				/* 4085 */
SYS(sys_uselib, 1)
SYS(sys_swapon, 2)
SYS(sys_reboot, 3)
SYS(old_readdir, 3)
SYS(old_mmap, 6)				/* 4090 */
SYS(sys_munmap, 2)
SYS(sys_truncate, 2)
SYS(sys_ftruncate, 2)
SYS(sys_fchmod, 2)
SYS(sys_fchown, 3)				/* 4095 */
SYS(sys_getpriority, 2)
SYS(sys_setpriority, 3)
SYS(sys_ni_syscall, 0)
SYS(sys_statfs, 2)
SYS(sys_fstatfs, 2)				/* 4100 */
SYS(sys_ni_syscall, 3)				/* was ioperm(2) */
SYS(sys_socketcall, 2)
SYS(sys_syslog, 3)
SYS(sys_setitimer, 3)
SYS(sys_getitimer, 2)				/* 4105 */
SYS(sys_newstat, 2)
SYS(sys_newlstat, 2)
SYS(sys_newfstat, 2)
SYS(sys_uname, 1)
SYS(sys_ni_syscall, 0)				/* 4110 was iopl(2) */
SYS(sys_vhangup, 0)
SYS(sys_ni_syscall, 0)				/* was sys_idle() */
SYS(sys_ni_syscall, 0)				/* was vm86(2) */
SYS(sys_wait4, 4)
SYS(sys_swapoff, 1)				/* 4115 */
SYS(sys_sysinfo, 1)
SYS(sys_ipc, 6)
SYS(sys_fsync, 1)
SYS(sys_sigreturn, 0)
SYS(sys_clone, 0)				/* 4120 */
SYS(sys_setdomainname, 2)
SYS(sys_newuname, 1)
SYS(sys_ni_syscall, 0) /* sys_modify_ldt */
SYS(sys_adjtimex, 1)
SYS(sys_mprotect, 3)				/* 4125 */
SYS(sys_sigprocmask, 3)
SYS(sys_create_module, 2)
SYS(sys_init_module, 5)
SYS(sys_delete_module, 1)
SYS(sys_get_kernel_syms, 1)			/* 4130 */
SYS(sys_quotactl, 0)
SYS(sys_getpgid, 1)
SYS(sys_fchdir, 1)
SYS(sys_bdflush, 2)
SYS(sys_sysfs, 3)				/* 4135 */
SYS(sys_personality, 1)
SYS(sys_ni_syscall, 0) /* for afs_syscall */
SYS(sys_setfsuid, 1)
SYS(sys_setfsgid, 1)
SYS(sys_llseek, 5)				/* 4140 */
SYS(sys_getdents, 3)
SYS(sys_select, 5)
SYS(sys_flock, 2)
SYS(sys_msync, 3)
SYS(sys_readv, 3)				/* 4145 */
SYS(sys_writev, 3)
SYS(sys_cacheflush, 3)
SYS(sys_cachectl, 3)
SYS(sys_sysmips, 4)
SYS(sys_ni_syscall, 0)				/* 4150 */
SYS(sys_getsid, 1)
SYS(sys_fdatasync, 0)
SYS(sys_sysctl, 1)
SYS(sys_mlock, 2)
SYS(sys_munlock, 2)				/* 4155 */
SYS(sys_mlockall, 1)
SYS(sys_munlockall, 0)
SYS(sys_sched_setparam,2)
SYS(sys_sched_getparam,2)
SYS(sys_sched_setscheduler,3)			/* 4160 */
SYS(sys_sched_getscheduler,1)
SYS(sys_sched_yield,0)
SYS(sys_sched_get_priority_max,1)
SYS(sys_sched_get_priority_min,1)
SYS(sys_sched_rr_get_interval,2)		/* 4165 */
SYS(sys_nanosleep,2)
SYS(sys_mremap,4)
SYS(sys_accept, 3)
SYS(sys_bind, 3)
SYS(sys_connect, 3)				/* 4170 */
SYS(sys_getpeername, 3)
SYS(sys_getsockname, 3)
SYS(sys_getsockopt, 5)
SYS(sys_listen, 2)
SYS(sys_recv, 4)				/* 4175 */
SYS(sys_recvfrom, 6)
SYS(sys_recvmsg, 3)
SYS(sys_send, 4)
SYS(sys_sendmsg, 3)
SYS(sys_sendto, 6)				/* 4180 */
SYS(sys_setsockopt, 5)
SYS(sys_shutdown, 2)
SYS(sys_socket, 3)
SYS(sys_socketpair, 4)
SYS(sys_setresuid, 3)				/* 4185 */
SYS(sys_getresuid, 3)
SYS(sys_query_module, 5)
SYS(sys_poll, 3)
SYS(sys_nfsservctl, 3)
SYS(sys_setresgid, 3)				/* 4190 */
SYS(sys_getresgid, 3)
SYS(sys_prctl, 5)
SYS(sys_rt_sigreturn, 0)
SYS(sys_rt_sigaction, 4)
SYS(sys_rt_sigprocmask, 4)			/* 4195 */
SYS(sys_rt_sigpending, 2)
SYS(sys_rt_sigtimedwait, 4)
SYS(sys_rt_sigqueueinfo, 3)
SYS(sys_rt_sigsuspend, 0)
SYS(sys_pread, 6)				/* 4200 */
SYS(sys_pwrite, 6)
SYS(sys_chown, 3)
SYS(sys_getcwd, 2)
SYS(sys_capget, 2)
SYS(sys_capset, 2)				/* 4205 */
SYS(sys_sigaltstack, 0)
SYS(sys_sendfile, 4)
SYS(sys_ni_syscall, 0)
SYS(sys_ni_syscall, 0)
SYS(sys_mmap2, 6)				/* 4210 */
SYS(sys_truncate64, 4)
SYS(sys_ftruncate64, 4)
SYS(sys_stat64, 2)
SYS(sys_lstat64, 2)
SYS(sys_fstat64, 2)				/* 4215 */
SYS(sys_pivot_root, 2)
SYS(sys_mincore, 3)
SYS(sys_madvise, 3)
SYS(sys_getdents64, 3)
SYS(sys_fcntl64, 3)				/* 4220 */
SYS(sys_ni_syscall, 0)
SYS(sys_gettid, 0)
SYS(sys_readahead, 5)
SYS(sys_setxattr, 5)
SYS(sys_lsetxattr, 5)				/* 4225 */
SYS(sys_fsetxattr, 5)
SYS(sys_getxattr, 4)
SYS(sys_lgetxattr, 4)
SYS(sys_fgetxattr, 4)
SYS(sys_listxattr, 3)				/* 4230 */
SYS(sys_llistxattr, 3)
SYS(sys_flistxattr, 3)
SYS(sys_removexattr, 2)
SYS(sys_lremovexattr, 2)
SYS(sys_fremovexattr, 2)			/* 4235 */
SYS(sys_tkill, 2)
SYS(sys_sendfile64, 5)
SYS(sys_ni_syscall, 0)				/* res. for futex */
SYS(sys_ni_syscall, 0)				/* res. for sched_setaffinity */
SYS(sys_ni_syscall, 0)				/* 4240 res. for sched_getaffinity */
