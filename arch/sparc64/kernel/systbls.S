/* $Id: systbls.S,v 1.79 2001/10/18 08:27:05 davem Exp $
 * systbls.S: System call entry point tables for OS compatibility.
 *            The native Linux system call table lives here also.
 *
 * Copyright (C) 1995, 1996 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1997 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 *
 * Based upon preliminary work which is:
 *
 * Copyright (C) 1995 Adrian M. Rodriguez (adrian@remus.rutgers.edu)
 */

#include <linux/config.h>

	.text
	.align	1024

	/* First, the 32-bit Linux native syscall table. */

	.globl sys_call_table32
sys_call_table32:
/*0*/	.word sys_nis_syscall, sparc_exit, sys_fork, sys_read, sys_write
/*5*/	.word sparc32_open, sys_close, sys32_wait4, sys_creat, sys_link
/*10*/  .word sys_unlink, sunos_execv, sys_chdir, sys32_chown16, sys32_mknod
/*15*/	.word sys32_chmod, sys32_lchown16, sparc_brk, sys_perfctr, sys32_lseek
/*20*/	.word sys_getpid, sys_capget, sys_capset, sys32_setuid16, sys32_getuid16
/*25*/	.word sys_time, sys_ptrace, sys_alarm, sys32_sigaltstack, sys32_pause
/*30*/	.word sys32_utime, sys_lchown, sys_fchown, sys_access, sys_nice
	.word sys_chown, sys_sync, sys_kill, sys32_newstat, sys32_sendfile
/*40*/	.word sys32_newlstat, sys_dup, sys_pipe, sys32_times, sys_getuid
	.word sys_umount, sys32_setgid16, sys32_getgid16, sys_signal, sys32_geteuid16
/*50*/	.word sys32_getegid16, sys_acct, sys_nis_syscall, sys_getgid, sys32_ioctl
	.word sys_reboot, sys32_mmap2, sys_symlink, sys_readlink, sys32_execve
/*60*/	.word sys_umask, sys_chroot, sys32_newfstat, sys_fstat64, sys_getpagesize
	.word sys_msync, sys_vfork, sys32_pread, sys32_pwrite, sys_geteuid
/*70*/	.word sys_getegid, sys32_mmap, sys_setreuid, sys_munmap, sys_mprotect
	.word sys_madvise, sys_vhangup, sys32_truncate64, sys_mincore, sys32_getgroups16
/*80*/	.word sys32_setgroups16, sys_getpgrp, sys_setgroups, sys32_setitimer, sys32_ftruncate64
	.word sys_swapon, sys32_getitimer, sys_setuid, sys_sethostname, sys_setgid
/*90*/	.word sys_dup2, sys_setfsuid, sys32_fcntl, sys32_select, sys_setfsgid
	.word sys_fsync, sys_setpriority32, sys_nis_syscall, sys_nis_syscall, sys_nis_syscall
/*100*/ .word sys_getpriority, sys32_rt_sigreturn, sys32_rt_sigaction, sys32_rt_sigprocmask, sys32_rt_sigpending
	.word sys32_rt_sigtimedwait, sys32_rt_sigqueueinfo, sys32_rt_sigsuspend, sys_setresuid, sys_getresuid
/*110*/	.word sys_setresgid, sys_getresgid, sys_setregid, sys_nis_syscall, sys_nis_syscall
	.word sys_getgroups, sys32_gettimeofday, sys32_getrusage, sys_nis_syscall, sys_getcwd
/*120*/	.word sys32_readv, sys32_writev, sys32_settimeofday, sys32_fchown16, sys_fchmod
	.word sys_nis_syscall, sys32_setreuid16, sys32_setregid16, sys_rename, sys_truncate
/*130*/	.word sys_ftruncate, sys_flock, sys_lstat64, sys_nis_syscall, sys_nis_syscall
	.word sys_nis_syscall, sys_mkdir, sys_rmdir, sys32_utimes, sys_stat64
/*140*/	.word sys_nis_syscall, sys_nis_syscall, sys_nis_syscall, sys_gettid, sys32_getrlimit
	.word sys32_setrlimit, sys_pivot_root, sys32_prctl, sys32_pciconfig_read, sys32_pciconfig_write
/*150*/	.word sys_nis_syscall, sys_nis_syscall, sys_nis_syscall, sys_poll, sys_getdents64
	.word sys32_fcntl64, sys_nis_syscall, sys32_statfs, sys32_fstatfs, sys_oldumount
/*160*/	.word sys_nis_syscall, sys_nis_syscall, sys_getdomainname, sys_setdomainname, sys_nis_syscall
	.word sys32_quotactl, sys_nis_syscall, sys32_mount, sys_ustat, sys_setxattr
/*170*/	.word sys_lsetxattr, sys_fsetxattr, sys_getxattr, sys_lgetxattr, sys32_getdents
	.word sys_setsid, sys_fchdir, sys_fgetxattr, sys_listxattr, sys_llistxattr
/*180*/	.word sys_flistxattr, sys_removexattr, sys_lremovexattr, sys32_sigpending, sys32_query_module
	.word sys_setpgid, sys_fremovexattr, sys_tkill, sys_nis_syscall, sparc64_newuname
/*190*/	.word sys32_init_module, sparc64_personality, sys_nis_syscall, sys_nis_syscall, sys_nis_syscall
	.word sys_nis_syscall, sys_nis_syscall, sys_getppid, sys32_sigaction, sys_sgetmask
/*200*/	.word sys_ssetmask, sys_sigsuspend, sys32_newlstat, sys_uselib, old32_readdir
	.word sys32_readahead, sys32_socketcall, sys_syslog, sys_nis_syscall, sys_nis_syscall
/*210*/	.word sys_nis_syscall, sys_nis_syscall, sys_waitpid, sys_swapoff, sys32_sysinfo
	.word sys32_ipc, sys32_sigreturn, sys_clone, sys_nis_syscall, sys32_adjtimex
/*220*/	.word sys32_sigprocmask, sys32_create_module, sys32_delete_module, sys32_get_kernel_syms, sys_getpgid
	.word sys32_bdflush, sys32_sysfs, sys_nis_syscall, sys32_setfsuid16, sys32_setfsgid16
/*230*/	.word sys32_select, sys_time, sys_nis_syscall, sys_stime, sys_nis_syscall
	.word sys_nis_syscall, sys_llseek, sys_mlock, sys_munlock, sys_mlockall
/*240*/	.word sys_munlockall, sys_sched_setparam, sys_sched_getparam, sys_sched_setscheduler, sys_sched_getscheduler
	.word sys_sched_yield, sys_sched_get_priority_max, sys_sched_get_priority_min, sys32_sched_rr_get_interval, sys32_nanosleep
/*250*/	.word sys32_mremap, sys32_sysctl, sys_getsid, sys_fdatasync, sys32_nfsservctl
	.word sys_aplib

	/* Now the 64-bit native Linux syscall table. */

	.align	1024
	.globl sys_call_table64, sys_call_table
sys_call_table64:
sys_call_table:
/*0*/	.word sys_nis_syscall, sparc_exit, sys_fork, sys_read, sys_write
/*5*/	.word sys_open, sys_close, sys_wait4, sys_creat, sys_link
/*10*/  .word sys_unlink, sunos_execv, sys_chdir, sys_chown, sys_mknod
/*15*/	.word sys_chmod, sys_lchown, sparc_brk, sys_perfctr, sys_lseek
/*20*/	.word sys_getpid, sys_capget, sys_capset, sys_setuid, sys_getuid
/*25*/	.word sys_nis_syscall, sys_ptrace, sys_alarm, sys_sigaltstack, sys_nis_syscall
/*30*/	.word sys_utime, sys_nis_syscall, sys_nis_syscall, sys_access, sys_nice
	.word sys_nis_syscall, sys_sync, sys_kill, sys_newstat, sys_sendfile
/*40*/	.word sys_newlstat, sys_dup, sys_pipe, sys_times, sys_nis_syscall
	.word sys_umount, sys_setgid, sys_getgid, sys_signal, sys_geteuid
/*50*/	.word sys_getegid, sys_acct, sys_memory_ordering, sys_nis_syscall, sys_ioctl
	.word sys_reboot, sys_nis_syscall, sys_symlink, sys_readlink, sys_execve
/*60*/	.word sys_umask, sys_chroot, sys_newfstat, sys_nis_syscall, sys_getpagesize
	.word sys_msync, sys_vfork, sys_pread, sys_pwrite, sys_nis_syscall
/*70*/	.word sys_nis_syscall, sys_mmap, sys_nis_syscall, sys64_munmap, sys_mprotect
	.word sys_madvise, sys_vhangup, sys_nis_syscall, sys_mincore, sys_getgroups
/*80*/	.word sys_setgroups, sys_getpgrp, sys_nis_syscall, sys_setitimer, sys_nis_syscall
	.word sys_swapon, sys_getitimer, sys_nis_syscall, sys_sethostname, sys_nis_syscall
/*90*/	.word sys_dup2, sys_nis_syscall, sys_fcntl, sys_select, sys_nis_syscall
	.word sys_fsync, sys_setpriority, sys_socket, sys_connect, sys_accept
/*100*/	.word sys_getpriority, sys_rt_sigreturn, sys_rt_sigaction, sys_rt_sigprocmask, sys_rt_sigpending
	.word sys_rt_sigtimedwait, sys_rt_sigqueueinfo, sys_rt_sigsuspend, sys_setresuid, sys_getresuid
/*110*/	.word sys_setresgid, sys_getresgid, sys_nis_syscall, sys_recvmsg, sys_sendmsg
	.word sys_nis_syscall, sys_gettimeofday, sys_getrusage, sys_getsockopt, sys_getcwd
/*120*/	.word sys_readv, sys_writev, sys_settimeofday, sys_fchown, sys_fchmod
	.word sys_recvfrom, sys_setreuid, sys_setregid, sys_rename, sys_truncate
/*130*/	.word sys_ftruncate, sys_flock, sys_nis_syscall, sys_sendto, sys_shutdown
	.word sys_socketpair, sys_mkdir, sys_rmdir, sys_utimes, sys_nis_syscall
/*140*/	.word sys_nis_syscall, sys_getpeername, sys_nis_syscall, sys_gettid, sys_getrlimit
	.word sys_setrlimit, sys_pivot_root, sys_prctl, sys_pciconfig_read, sys_pciconfig_write
/*150*/	.word sys_getsockname, sys_nis_syscall, sys_nis_syscall, sys_poll, sys_getdents64
	.word sys_nis_syscall, sys_nis_syscall, sys_statfs, sys_fstatfs, sys_oldumount
/*160*/	.word sys_nis_syscall, sys_nis_syscall, sys_getdomainname, sys_setdomainname, sys_utrap_install
	.word sys_quotactl, sys_nis_syscall, sys_mount, sys_ustat, sys_setxattr
/*170*/	.word sys_lsetxattr, sys_fsetxattr, sys_getxattr, sys_lgetxattr, sys_getdents
	.word sys_setsid, sys_fchdir, sys_fgetxattr, sys_listxattr, sys_llistxattr
/*180*/	.word sys_flistxattr, sys_removexattr, sys_lremovexattr, sys_nis_syscall, sys_query_module
	.word sys_setpgid, sys_fremovexattr, sys_tkill, sys_nis_syscall, sparc64_newuname
/*190*/	.word sys_init_module, sparc64_personality, sys_nis_syscall, sys_nis_syscall, sys_nis_syscall
	.word sys_nis_syscall, sys_nis_syscall, sys_getppid, sys_nis_syscall, sys_sgetmask
/*200*/	.word sys_ssetmask, sys_nis_syscall, sys_newlstat, sys_uselib, sys_nis_syscall
	.word sys_readahead, sys_socketcall, sys_syslog, sys_nis_syscall, sys_nis_syscall
/*210*/	.word sys_nis_syscall, sys_nis_syscall, sys_waitpid, sys_swapoff, sys_sysinfo
	.word sys_ipc, sys_nis_syscall, sys_clone, sys_nis_syscall, sys_adjtimex
/*220*/	.word sys_nis_syscall, sys_create_module, sys_delete_module, sys_get_kernel_syms, sys_getpgid
	.word sys_bdflush, sys_sysfs, sys_nis_syscall, sys_setfsuid, sys_setfsgid
/*230*/	.word sys_select, sys_nis_syscall, sys_nis_syscall, sys_stime, sys_nis_syscall
	.word sys_nis_syscall, sys_llseek, sys_mlock, sys_munlock, sys_mlockall
/*240*/	.word sys_munlockall, sys_sched_setparam, sys_sched_getparam, sys_sched_setscheduler, sys_sched_getscheduler
	.word sys_sched_yield, sys_sched_get_priority_max, sys_sched_get_priority_min, sys_sched_rr_get_interval, sys_nanosleep
/*250*/	.word sys64_mremap, sys_sysctl, sys_getsid, sys_fdatasync, sys_nfsservctl
	.word sys_aplib

#if defined(CONFIG_SUNOS_EMUL) || defined(CONFIG_SOLARIS_EMUL) || \
    defined(CONFIG_SOLARIS_EMUL_MODULE)
	/* Now the 32-bit SunOS syscall table. */

	.align 1024
	.globl sunos_sys_table
sunos_sys_table:
/*0*/	.word sunos_indir, sparc_exit, sys_fork
	.word sunos_read, sunos_write, sunos_open
	.word sys_close, sunos_wait4, sys_creat
	.word sys_link, sys_unlink, sunos_execv
	.word sys_chdir, sunos_nosys, sys32_mknod
	.word sys32_chmod, sys32_lchown16, sunos_brk
	.word sunos_nosys, sys32_lseek, sunos_getpid
	.word sunos_nosys, sunos_nosys, sunos_nosys
	.word sunos_getuid, sunos_nosys, sys_ptrace
	.word sunos_nosys, sunos_nosys, sunos_nosys
	.word sunos_nosys, sunos_nosys, sunos_nosys
	.word sys_access, sunos_nosys, sunos_nosys
	.word sys_sync, sys_kill, sys32_newstat
	.word sunos_nosys, sys32_newlstat, sys_dup
	.word sys_pipe, sunos_nosys, sunos_nosys
	.word sunos_nosys, sunos_nosys, sunos_getgid
	.word sunos_nosys, sunos_nosys
/*50*/	.word sunos_nosys, sys_acct, sunos_nosys
	.word sunos_mctl, sunos_ioctl, sys_reboot
	.word sunos_nosys, sys_symlink, sys_readlink
	.word sys32_execve, sys_umask, sys_chroot
	.word sys32_newfstat, sunos_nosys, sys_getpagesize
	.word sys_msync, sys_vfork, sunos_nosys
	.word sunos_nosys, sunos_sbrk, sunos_sstk
	.word sunos_mmap, sunos_vadvise, sys_munmap
	.word sys_mprotect, sys_madvise, sys_vhangup
	.word sunos_nosys, sys_mincore, sys32_getgroups16
	.word sys32_setgroups16, sys_getpgrp, sunos_setpgrp
	.word sys32_setitimer, sunos_nosys, sys_swapon
	.word sys32_getitimer, sys_gethostname, sys_sethostname
	.word sunos_getdtablesize, sys_dup2, sunos_nop
	.word sys32_fcntl, sunos_select, sunos_nop
	.word sys_fsync, sys_setpriority32, sunos_socket
	.word sys_connect, sunos_accept
/*100*/	.word sys_getpriority, sunos_send, sunos_recv
	.word sunos_nosys, sys_bind, sunos_setsockopt
	.word sys_listen, sunos_nosys, sunos_sigaction
	.word sunos_sigblock, sunos_sigsetmask, sys_sigpause
	.word sys32_sigstack, sys32_recvmsg, sys32_sendmsg
	.word sunos_nosys, sys32_gettimeofday, sys32_getrusage
	.word sunos_getsockopt, sunos_nosys, sunos_readv
	.word sunos_writev, sys32_settimeofday, sys32_fchown16
	.word sys_fchmod, sys32_recvfrom, sys32_setreuid16
	.word sys32_setregid16, sys_rename, sys_truncate
	.word sys_ftruncate, sys_flock, sunos_nosys
	.word sys32_sendto, sys_shutdown, sys_socketpair
	.word sys_mkdir, sys_rmdir, sys32_utimes
	.word sys32_sigreturn, sunos_nosys, sys_getpeername
	.word sunos_gethostid, sunos_nosys, sys32_getrlimit
	.word sys32_setrlimit, sunos_killpg, sunos_nosys
	.word sunos_nosys, sunos_nosys
/*150*/	.word sys_getsockname, sunos_nosys, sunos_nosys
	.word sys_poll, sunos_nosys, sunos_nosys
	.word sunos_getdirentries, sys32_statfs, sys32_fstatfs
	.word sys_oldumount, sunos_nosys, sunos_nosys
	.word sys_getdomainname, sys_setdomainname
	.word sunos_nosys, sys32_quotactl, sunos_nosys
	.word sunos_mount, sys_ustat, sunos_semsys
	.word sunos_nosys, sunos_shmsys, sunos_audit
	.word sunos_nosys, sunos_getdents, sys_setsid
	.word sys_fchdir, sunos_nosys, sunos_nosys
	.word sunos_nosys, sunos_nosys, sunos_nosys
	.word sunos_nosys, sys32_sigpending, sunos_nosys
	.word sys_setpgid, sunos_pathconf, sunos_fpathconf
	.word sunos_sysconf, sunos_uname, sunos_nosys
	.word sunos_nosys, sunos_nosys, sunos_nosys
	.word sunos_nosys, sunos_nosys, sunos_nosys
	.word sunos_nosys, sunos_nosys, sunos_nosys
/*200*/	.word sunos_nosys, sunos_nosys, sunos_nosys
	.word sunos_nosys, sunos_nosys, sunos_nosys
	.word sunos_nosys, sunos_nosys, sunos_nosys
	.word sunos_nosys, sunos_nosys, sunos_nosys
	.word sunos_nosys, sunos_nosys, sunos_nosys
	.word sunos_nosys, sunos_nosys, sunos_nosys
	.word sunos_nosys, sunos_nosys, sunos_nosys
	.word sunos_nosys, sunos_nosys, sunos_nosys
	.word sunos_nosys, sunos_nosys, sunos_nosys
	.word sunos_nosys, sunos_nosys, sunos_nosys
	.word sunos_nosys, sunos_nosys, sunos_nosys
	.word sunos_nosys, sunos_nosys, sunos_nosys
	.word sunos_nosys, sunos_nosys, sunos_nosys
	.word sunos_nosys, sunos_nosys, sunos_nosys
	.word sunos_nosys, sunos_nosys, sunos_nosys
	.word sunos_nosys, sunos_nosys, sunos_nosys
	.word sunos_nosys, sunos_nosys
/*250*/	.word sunos_nosys, sunos_nosys, sunos_nosys
	.word sunos_nosys, sunos_nosys, sys_aplib

#endif
