/*
*  arch/s390/kernel/sys_wrapper31.S
*    wrapper for 31 bit compatible system calls.
*
*  S390 version
*    Copyright (C) 2000 IBM Deutschland Entwicklung GmbH, IBM Corporation
*    Author(s): Gerhard Tonn (ton@de.ibm.com),
*/ 

	.globl  sys32_exit_wrapper 
sys32_exit_wrapper:
	lgfr	%r2,%r2			# int
	jg	sys_exit		# branch to sys_exit
    
	.globl  sys32_read_wrapper 
sys32_read_wrapper:
	llgfr	%r2,%r2			# unsigned int
	llgtr	%r3,%r3			# char *
	llgfr	%r4,%r4			# size_t
	jg	sys32_read		# branch to sys_read

	.globl  sys32_write_wrapper 
sys32_write_wrapper:
	llgfr	%r2,%r2			# unsigned int
	llgtr	%r3,%r3			# const char *
	llgfr	%r4,%r4			# size_t
	jg	sys32_write		# branch to system call

	.globl  sys32_open_wrapper 
sys32_open_wrapper:
	llgtr	%r2,%r2			# const char *
	lgfr	%r3,%r3			# int
	lgfr	%r4,%r4			# int
	jg	sys_open		# branch to system call

	.globl  sys32_close_wrapper 
sys32_close_wrapper:
	llgfr	%r2,%r2			# unsigned int
	jg	sys_close		# branch to system call

	.globl  sys32_creat_wrapper 
sys32_creat_wrapper:
	llgtr	%r2,%r2			# const char *
	lgfr	%r3,%r3			# int
	jg	sys_creat		# branch to system call

	.globl  sys32_link_wrapper 
sys32_link_wrapper:
	llgtr	%r2,%r2			# const char *
	llgtr	%r3,%r3			# const char *
	jg	sys_link		# branch to system call

	.globl  sys32_unlink_wrapper 
sys32_unlink_wrapper:
	llgtr	%r2,%r2			# const char *
	jg	sys_unlink		# branch to system call

	.globl  sys32_chdir_wrapper 
sys32_chdir_wrapper:
	llgtr	%r2,%r2			# const char *
	jg	sys_chdir		# branch to system call

	.globl  sys32_time_wrapper 
sys32_time_wrapper:
	llgtr	%r2,%r2			# int *
	jg	sys_time		# branch to system call

	.globl  sys32_mknod_wrapper 
sys32_mknod_wrapper:
	llgtr	%r2,%r2			# const char *
	lgfr	%r3,%r3			# int 
	llgfr	%r4,%r4			# dev
	jg	sys_mknod		# branch to system call

	.globl  sys32_chmod_wrapper 
sys32_chmod_wrapper:
	llgtr	%r2,%r2			# const char *
	llgfr	%r3,%r3			# mode_t
	jg	sys_chmod		# branch to system call

	.globl  sys32_lchown16_wrapper 
sys32_lchown16_wrapper:
	llgtr	%r2,%r2			# const char *
	llgfr	%r3,%r3			# __kernel_old_uid_emu31_t 
	llgfr	%r4,%r4			# __kernel_old_uid_emu31_t 
	jg	sys32_lchown16		# branch to system call

	.globl  sys32_lseek_wrapper 
sys32_lseek_wrapper:
	llgfr	%r2,%r2			# unsigned int
	lgfr	%r3,%r3			# off_t
	llgfr	%r4,%r4			# unsigned int
	jg	sys_lseek		# branch to system call

#sys32_getpid_wrapper				# void 

	.globl  sys32_mount_wrapper 
sys32_mount_wrapper:
	llgtr	%r2,%r2			# char *
	llgtr	%r3,%r3			# char *
	llgtr	%r4,%r4			# char *
	llgfr	%r5,%r5			# unsigned long
	llgtr	%r6,%r6			# void *
	jg	sys32_mount		# branch to system call

	.globl  sys32_oldumount_wrapper 
sys32_oldumount_wrapper:
	llgtr	%r2,%r2			# char *
	jg	sys_oldumount		# branch to system call

	.globl  sys32_setuid16_wrapper 
sys32_setuid16_wrapper:
	llgfr	%r2,%r2			# __kernel_old_uid_emu31_t 
	jg	sys32_setuid16		# branch to system call

#sys32_getuid16_wrapper			# void 

	.globl  sys32_ptrace_wrapper 
sys32_ptrace_wrapper:
	lgfr	%r2,%r2			# long
	lgfr	%r3,%r3			# long
	llgtr	%r4,%r4			# long
	llgfr	%r5,%r5			# long
	jg	sys_ptrace		# branch to system call

	.globl  sys32_alarm_wrapper 
sys32_alarm_wrapper:
	llgtr	%r2,%r2			# unsigned int
	jg	sys_alarm		# branch to system call

#sys32_pause_wrapper			# void 

	.globl  sys32_utime_wrapper 
sys32_utime_wrapper:
	llgtr	%r2,%r2			# char *
	llgtr	%r3,%r3			# struct utimbuf_emu31 *
	jg	sys32_utime		# branch to system call

	.globl  sys32_access_wrapper 
sys32_access_wrapper:
	llgtr	%r2,%r2			# const char *
	lgfr	%r3,%r3			# int
	jg	sys_access		# branch to system call

	.globl  sys32_nice_wrapper 
sys32_nice_wrapper:
	lgfr	%r2,%r2			# int
	jg	sys_nice		# branch to system call

#sys32_sync_wrapper			# void 

	.globl  sys32_kill_wrapper 
sys32_kill_wrapper:
	lgfr	%r2,%r2			# int
	lgfr	%r3,%r3			# int
	jg	sys_kill		# branch to system call

	.globl  sys32_rename_wrapper 
sys32_rename_wrapper:
	llgtr	%r2,%r2			# const char *
	llgtr	%r3,%r3			# const char *
	jg	sys_rename		# branch to system call

	.globl  sys32_mkdir_wrapper 
sys32_mkdir_wrapper:
	llgtr	%r2,%r2			# const char *
	lgfr	%r3,%r3			# int
	jg	sys_mkdir		# branch to system call

	.globl  sys32_rmdir_wrapper 
sys32_rmdir_wrapper:
	llgtr	%r2,%r2			# const char *
	jg	sys_rmdir		# branch to system call

	.globl  sys32_dup_wrapper 
sys32_dup_wrapper:
	llgfr	%r2,%r2			# unsigned int
	jg	sys_dup			# branch to system call

	.globl  sys32_pipe_wrapper 
sys32_pipe_wrapper:
	llgtr	%r2,%r2			# u32 *
	jg	sys_pipe		# branch to system call

	.globl  sys32_times_wrapper 
sys32_times_wrapper:
	llgtr	%r2,%r2			# struct tms_emu31 *
	jg	sys32_times		# branch to system call

	.globl  sys32_brk_wrapper 
sys32_brk_wrapper:
	llgtr	%r2,%r2			# unsigned long
	jg	sys_brk			# branch to system call

	.globl  sys32_setgid16_wrapper 
sys32_setgid16_wrapper:
	llgfr	%r2,%r2			# __kernel_old_gid_emu31_t 
	jg	sys32_setgid16		# branch to system call

#sys32_getgid16_wrapper			# void 

	.globl sys32_signal_wrapper
sys32_signal_wrapper:
	lgfr	%r2,%r2			# int 
	llgfr	%r3,%r3			# __sighandler_t 
	jg	sys_signal

#sys32_geteuid16_wrapper		# void 

#sys32_getegid16_wrapper		# void 

	.globl  sys32_acct_wrapper 
sys32_acct_wrapper:
	llgtr	%r2,%r2			# char *
	jg	sys_acct		# branch to system call

	.globl  sys32_umount_wrapper 
sys32_umount_wrapper:
	llgtr	%r2,%r2			# char *
	lgfr	%r3,%r3			# int
	jg	sys_umount		# branch to system call

	.globl  sys32_ioctl_wrapper 
sys32_ioctl_wrapper:
	llgfr	%r2,%r2			# unsigned int
	llgfr	%r3,%r3			# unsigned int
	llgfr	%r4,%r4			# unsigned int
	jg	sys32_ioctl		# branch to system call

	.globl  sys32_fcntl_wrapper 
sys32_fcntl_wrapper:
	llgfr	%r2,%r2			# unsigned int
	llgfr	%r3,%r3			# unsigned int 
	llgfr	%r4,%r4			# unsigned long
	jg	sys32_fcntl		# branch to system call

	.globl  sys32_setpgid_wrapper 
sys32_setpgid_wrapper:
	lgfr	%r2,%r2			# pid_t
	lgfr	%r3,%r3			# pid_t
	jg	sys_setpgid		# branch to system call

	.globl  sys32_umask_wrapper 
sys32_umask_wrapper:
	lgfr	%r3,%r3			# int
	jg	sys_umask		# branch to system call

	.globl  sys32_chroot_wrapper 
sys32_chroot_wrapper:
	llgtr	%r2,%r2			# char *
	jg	sys_chroot		# branch to system call

	.globl sys32_ustat_wrapper
sys32_ustat_wrapper:
	llgfr	%r2,%r2			# dev_t 
	llgtr	%r3,%r3			# struct ustat *
	jg	sys_ustat

	.globl  sys32_dup2_wrapper 
sys32_dup2_wrapper:
	llgfr	%r2,%r2			# unsigned int
	llgfr	%r3,%r3			# unsigned int
	jg	sys_dup2		# branch to system call

#sys32_getppid_wrapper			# void 

#sys32_getpgrp_wrapper			# void 

#sys32_setsid_wrapper			# void 

	.globl  sys32_sigaction_wrapper
sys32_sigaction_wrapper:
	lgfr	%r2,%r2			# int 
	llgtr	%r3,%r3			# const struct old_sigaction *
	jg	sys32_sigaction		# branch to system call

	.globl  sys32_setreuid16_wrapper 
sys32_setreuid16_wrapper:
	llgfr	%r2,%r2			# __kernel_old_uid_emu31_t 
	llgfr	%r3,%r3			# __kernel_old_uid_emu31_t 
	jg	sys32_setreuid16	# branch to system call

	.globl  sys32_setregid16_wrapper 
sys32_setregid16_wrapper:
	llgfr	%r2,%r2			# __kernel_old_gid_emu31_t 
	llgfr	%r3,%r3			# __kernel_old_gid_emu31_t 
	jg	sys32_setregid16	# branch to system call

#sys32_sigsuspend_wrapper		# done in sigsuspend_glue 

	.globl  sys32_sigpending_wrapper 
sys32_sigpending_wrapper:
	llgtr	%r2,%r2			# old_sigset_emu31_t *
	jg	sys32_sigpending	# branch to system call

	.globl  sys32_sethostname_wrapper 
sys32_sethostname_wrapper:
	llgtr	%r2,%r2			# char *
	lgfr	%r3,%r3			# int
	jg	sys_sethostname		# branch to system call

	.globl  sys32_setrlimit_wrapper 
sys32_setrlimit_wrapper:
	llgfr	%r2,%r2			# unsigned int
	llgtr	%r3,%r3			# struct rlimit_emu31 *
	jg	sys32_setrlimit		# branch to system call

	.globl  sys32_old_getrlimit_wrapper 
sys32_old_getrlimit_wrapper:
	llgfr	%r2,%r2			# unsigned int
	llgtr	%r3,%r3			# struct rlimit_emu31 *
	jg	sys32_old_getrlimit	# branch to system call

	.globl  sys32_getrlimit_wrapper 
sys32_getrlimit_wrapper:
	llgfr	%r2,%r2			# unsigned int
	llgtr	%r3,%r3			# struct rlimit_emu31 *
	jg	sys32_getrlimit		# branch to system call

	.globl  sys32_mmap2_wrapper 
sys32_mmap2_wrapper:
	llgtr	%r2,%r2			# struct mmap_arg_struct_emu31 *
	jg	sys32_mmap2			# branch to system call

	.globl  sys32_getrusage_wrapper 
sys32_getrusage_wrapper:
	lgfr	%r2,%r2			# int
	llgtr	%r3,%r3			# struct rusage_emu31 *
	jg	sys32_getrusage		# branch to system call

	.globl  sys32_gettimeofday_wrapper 
sys32_gettimeofday_wrapper:
	llgtr	%r2,%r2			# struct timeval_emu31 *
	llgtr	%r3,%r3			# struct timezone *
	jg	sys32_gettimeofday	# branch to system call

	.globl  sys32_settimeofday_wrapper 
sys32_settimeofday_wrapper:
	llgtr	%r2,%r2			# struct timeval_emu31 *
	llgtr	%r3,%r3			# struct timezone *
	jg	sys32_settimeofday	# branch to system call

	.globl  sys32_getgroups16_wrapper 
sys32_getgroups16_wrapper:
	lgfr	%r2,%r2			# int
	llgtr	%r3,%r3			# __kernel_old_gid_emu31_t *
	jg	sys32_getgroups16	# branch to system call

	.globl  sys32_setgroups16_wrapper 
sys32_setgroups16_wrapper:
	lgfr	%r2,%r2			# int
	llgtr	%r3,%r3			# __kernel_old_gid_emu31_t *
	jg	sys32_setgroups16	# branch to system call

	.globl  sys32_symlink_wrapper 
sys32_symlink_wrapper:
	llgtr	%r2,%r2			# const char *
	llgtr	%r3,%r3			# const char *
	jg	sys_symlink		# branch to system call

	.globl  sys32_readlink_wrapper 
sys32_readlink_wrapper:
	llgtr	%r2,%r2			# const char *
	llgtr	%r3,%r3			# char *
	lgfr	%r4,%r4			# int
	jg	sys_readlink		# branch to system call

	.globl  sys32_uselib_wrapper 
sys32_uselib_wrapper:
	llgtr	%r2,%r2			# const char *
	jg	sys_uselib		# branch to system call

	.globl  sys32_swapon_wrapper 
sys32_swapon_wrapper:
	llgtr	%r2,%r2			# const char *
	lgfr	%r3,%r3			# int
	jg	sys_swapon		# branch to system call

	.globl  sys32_reboot_wrapper 
sys32_reboot_wrapper:
	lgfr	%r2,%r2			# int
	lgfr	%r3,%r3			# int
	llgfr	%r4,%r4			# unsigned int
	llgtr	%r5,%r5			# void *
	jg	sys_reboot		# branch to system call

	.globl  old32_readdir_wrapper 
old32_readdir_wrapper:
	llgfr	%r2,%r2			# unsigned int
	llgtr	%r3,%r3			# void *
	llgfr	%r4,%r4			# unsigned int
	jg	old32_readdir		# branch to system call

	.globl  old32_mmap_wrapper 
old32_mmap_wrapper:
	llgtr	%r2,%r2			# struct mmap_arg_struct_emu31 *
	jg	old32_mmap		# branch to system call

	.globl  sys32_munmap_wrapper 
sys32_munmap_wrapper:
	llgfr	%r2,%r2			# unsigned long
	llgfr	%r3,%r3			# size_t 
	jg	sys_munmap		# branch to system call

	.globl  sys32_truncate_wrapper 
sys32_truncate_wrapper:
	llgtr	%r2,%r2			# const char *
	llgfr	%r3,%r3			# unsigned long
	jg	sys_truncate		# branch to system call

	.globl  sys32_ftruncate_wrapper 
sys32_ftruncate_wrapper:
	llgfr	%r2,%r2			# unsigned int
	llgfr	%r3,%r3			# unsigned long
	jg	sys_ftruncate		# branch to system call

	.globl  sys32_fchmod_wrapper 
sys32_fchmod_wrapper:
	llgfr	%r2,%r2			# unsigned int
	llgfr	%r3,%r3			# mode_t
	jg	sys_fchmod		# branch to system call

	.globl  sys32_fchown16_wrapper 
sys32_fchown16_wrapper:
	llgfr	%r2,%r2			# unsigned int
	llgtr	%r3,%r3			# __kernel_old_uid_emu31_t *
	llgtr	%r4,%r4			# __kernel_old_gid_emu31_t *
	jg	sys32_fchown16		# branch to system call

	.globl  sys32_getpriority_wrapper 
sys32_getpriority_wrapper:
	lgfr	%r2,%r2			# int
	lgfr	%r3,%r3			# int
	jg	sys_getpriority		# branch to system call

	.globl  sys32_setpriority_wrapper 
sys32_setpriority_wrapper:
	lgfr	%r2,%r2			# int
	lgfr	%r3,%r3			# int
	lgfr	%r4,%r4			# int
	jg	sys_setpriority		# branch to system call

	.globl  sys32_statfs_wrapper 
sys32_statfs_wrapper:
	llgtr	%r2,%r2			# char *
	llgtr	%r3,%r3			# struct statfs_emu31 *
	jg	sys32_statfs		# branch to system call

	.globl  sys32_fstatfs_wrapper 
sys32_fstatfs_wrapper:
	llgfr	%r2,%r2			# unsigned int
	llgtr	%r3,%r3			# struct statfs_emu31 *
	jg	sys32_fstatfs		# branch to system call

	.globl  sys32_socketcall_wrapper 
sys32_socketcall_wrapper:
	lgfr	%r2,%r2			# int
	llgtr	%r3,%r3			# u32 *
	jg	sys32_socketcall	# branch to system call

	.globl  sys32_syslog_wrapper 
sys32_syslog_wrapper:
	lgfr	%r2,%r2			# int
	llgtr	%r3,%r3			# char *
	lgfr	%r4,%r4			# int
	jg	sys_syslog		# branch to system call

	.globl  sys32_setitimer_wrapper 
sys32_setitimer_wrapper:
	lgfr	%r2,%r2			# int
	llgtr	%r3,%r3			# struct itimerval_emu31 *
	llgtr	%r4,%r4			# struct itimerval_emu31 *
	jg	sys32_setitimer		# branch to system call

	.globl  sys32_getitimer_wrapper 
sys32_getitimer_wrapper:
	lgfr	%r2,%r2			# int
	llgtr	%r3,%r3			# struct itimerval_emu31 *
	jg	sys32_getitimer		# branch to system call

	.globl  sys32_newstat_wrapper 
sys32_newstat_wrapper:
	llgtr	%r2,%r2			# char *
	llgtr	%r3,%r3			# struct stat_emu31 *
	jg	sys32_newstat		# branch to system call

	.globl  sys32_newlstat_wrapper 
sys32_newlstat_wrapper:
	llgtr	%r2,%r2			# char *
	llgtr	%r3,%r3			# struct stat_emu31 *
	jg	sys32_newlstat		# branch to system call

	.globl  sys32_newfstat_wrapper 
sys32_newfstat_wrapper:
	llgfr	%r2,%r2			# unsigned int
	llgtr	%r3,%r3			# struct stat_emu31 *
	jg	sys32_newfstat		# branch to system call

#sys32_vhangup_wrapper			# void 

	.globl  sys32_wait4_wrapper 
sys32_wait4_wrapper:
	lgfr	%r2,%r2			# pid_t
	llgtr	%r3,%r3			# unsigned int *
	lgfr	%r4,%r4			# int
	llgtr	%r5,%r5			# struct rusage *
	jg	sys32_wait4		# branch to system call

	.globl  sys32_swapoff_wrapper 
sys32_swapoff_wrapper:
	llgtr	%r2,%r2			# const char *
	jg	sys_swapoff		# branch to system call

	.globl  sys32_sysinfo_wrapper 
sys32_sysinfo_wrapper:
	llgtr	%r2,%r2			# struct sysinfo_emu31 *
	jg	sys32_sysinfo		# branch to system call

	.globl  sys32_ipc_wrapper 
sys32_ipc_wrapper:
	llgfr	%r2,%r2			# uint
	lgfr	%r3,%r3			# int
	lgfr	%r4,%r4			# int
	lgfr	%r5,%r5			# int
	llgtr	%r6,%r6			# void *
	jg	sys32_ipc		# branch to system call

	.globl  sys32_fsync_wrapper 
sys32_fsync_wrapper:
	llgfr	%r2,%r2			# unsigned int
	jg	sys_fsync		# branch to system call

#sys32_sigreturn_wrapper		# done in sigreturn_glue 

#sys32_clone_wrapper			# done in clone_glue 

	.globl  sys32_setdomainname_wrapper 
sys32_setdomainname_wrapper:
	llgtr	%r2,%r2			# char *
	lgfr	%r3,%r3			# int
	jg	sys_setdomainname	# branch to system call

	.globl  sys32_newuname_wrapper 
sys32_newuname_wrapper:
	llgtr	%r2,%r2			# struct new_utsname *
	jg	s390x_newuname		# branch to system call

	.globl  sys32_adjtimex_wrapper 
sys32_adjtimex_wrapper:
	llgtr	%r2,%r2			# struct timex_emu31 *
	jg	sys32_adjtimex		# branch to system call

	.globl  sys32_mprotect_wrapper 
sys32_mprotect_wrapper:
	llgtr	%r2,%r2			# unsigned long (actually pointer
	llgfr	%r3,%r3			# size_t
	llgfr	%r4,%r4			# unsigned long
	jg	sys_mprotect		# branch to system call

	.globl  sys32_sigprocmask_wrapper 
sys32_sigprocmask_wrapper:
	lgfr	%r2,%r2			# int
	llgtr	%r3,%r3			# old_sigset_emu31 *
	llgtr	%r4,%r4			# old_sigset_emu31 *
	jg	sys32_sigprocmask		# branch to system call

	.globl  sys32_create_module_wrapper 
sys32_create_module_wrapper:
	llgtr	%r2,%r2			# const char *
	llgfr	%r3,%r3			# size_t
	jg	sys32_create_module	# branch to system call

	.globl  sys32_init_module_wrapper 
sys32_init_module_wrapper:
	llgtr	%r2,%r2			# const char *
	llgtr	%r3,%r3			# struct module *
	jg	sys32_init_module	# branch to system call

	.globl  sys32_delete_module_wrapper 
sys32_delete_module_wrapper:
	llgtr	%r2,%r2			# const char *
	jg	sys32_delete_module	# branch to system call

	.globl  sys32_get_kernel_syms_wrapper 
sys32_get_kernel_syms_wrapper:
	llgtr	%r2,%r2			# struct kernel_sym_emu31 *
	jg	sys32_get_kernel_syms	# branch to system call

	.globl  sys32_quotactl_wrapper 
sys32_quotactl_wrapper:
	lgfr	%r2,%r2			# int
	llgtr	%r3,%r3			# const char *
	lgfr	%r4,%r4			# int
	llgtr	%r5,%r5			# caddr_t
	jg	sys32_quotactl		# branch to system call

	.globl  sys32_getpgid_wrapper 
sys32_getpgid_wrapper:
	lgfr	%r2,%r2			# pid_t
	jg	sys_getpgid		# branch to system call

	.globl  sys32_fchdir_wrapper 
sys32_fchdir_wrapper:
	llgfr	%r2,%r2			# unsigned int
	jg	sys_fchdir		# branch to system call

	.globl  sys32_bdflush_wrapper 
sys32_bdflush_wrapper:
	lgfr	%r2,%r2			# int
	lgfr	%r3,%r3			# long
	jg	sys_bdflush		# branch to system call

	.globl  sys32_sysfs_wrapper 
sys32_sysfs_wrapper:
	lgfr	%r2,%r2			# int
	llgfr	%r3,%r3			# unsigned long
	llgfr	%r4,%r4			# unsigned long
	jg	sys_sysfs		# branch to system call

	.globl  sys32_personality_wrapper 
sys32_personality_wrapper:
	llgfr	%r2,%r2			# unsigned long
	jg	s390x_personality	# branch to system call

	.globl  sys32_setfsuid16_wrapper 
sys32_setfsuid16_wrapper:
	llgfr	%r2,%r2			# __kernel_old_uid_emu31_t 
	jg	sys32_setfsuid16	# branch to system call

	.globl  sys32_setfsgid16_wrapper 
sys32_setfsgid16_wrapper:
	llgfr	%r2,%r2			# __kernel_old_gid_emu31_t 
	jg	sys32_setfsgid16	# branch to system call

	.globl  sys32_llseek_wrapper 
sys32_llseek_wrapper:
	llgfr	%r2,%r2			# unsigned int
	llgfr	%r3,%r3			# unsigned long
	llgfr	%r4,%r4			# unsigned long
	llgtr	%r5,%r5			# loff_t *
	llgfr	%r6,%r6			# unsigned int
	jg	sys_llseek		# branch to system call

	.globl  sys32_getdents_wrapper 
sys32_getdents_wrapper:
	llgfr	%r2,%r2			# unsigned int
	llgtr	%r3,%r3			# void *
	llgfr	%r4,%r4			# unsigned int
	jg	sys32_getdents		# branch to system call

	.globl  sys32_select_wrapper 
sys32_select_wrapper:
	lgfr	%r2,%r2			# int
	llgtr	%r3,%r3			# fd_set *
	llgtr	%r4,%r4			# fd_set *
	llgtr	%r5,%r5			# fd_set *
	llgtr	%r6,%r6			# struct timeval_emu31 *
	jg	sys32_select		# branch to system call

	.globl  sys32_flock_wrapper 
sys32_flock_wrapper:
	llgfr	%r2,%r2			# unsigned int
	llgfr	%r3,%r3			# unsigned int
	jg	sys_flock		# branch to system call

	.globl  sys32_msync_wrapper 
sys32_msync_wrapper:
	llgfr	%r2,%r2			# unsigned long
	llgfr	%r3,%r3			# size_t
	lgfr	%r4,%r4			# int
	jg	sys_msync		# branch to system call

	.globl  sys32_readv_wrapper 
sys32_readv_wrapper:
	llgfr	%r2,%r2			# unsigned long
	llgtr	%r3,%r3			# const struct iovec_emu31 *
	llgfr	%r4,%r4			# unsigned long
	jg	sys32_readv		# branch to system call

	.globl  sys32_writev_wrapper 
sys32_writev_wrapper:
	llgfr	%r2,%r2			# unsigned long
	llgtr	%r3,%r3			# const struct iovec_emu31 *
	llgfr	%r4,%r4			# unsigned long
	jg	sys32_writev		# branch to system call

	.globl  sys32_getsid_wrapper 
sys32_getsid_wrapper:
	lgfr	%r2,%r2			# pid_t
	jg	sys_getsid		# branch to system call

	.globl  sys32_fdatasync_wrapper 
sys32_fdatasync_wrapper:
	llgfr	%r2,%r2			# unsigned int
	jg	sys_fdatasync		# branch to system call

#sys32_sysctl_wrapper			# tbd 

	.globl  sys32_mlock_wrapper 
sys32_mlock_wrapper:
	llgfr	%r2,%r2			# unsigned long
	llgfr	%r3,%r3			# size_t
	jg	sys_mlock		# branch to system call

	.globl  sys32_munlock_wrapper 
sys32_munlock_wrapper:
	llgfr	%r2,%r2			# unsigned long
	llgfr	%r3,%r3			# size_t
	jg	sys_munlock		# branch to system call

	.globl  sys32_mlockall_wrapper 
sys32_mlockall_wrapper:
	lgfr	%r2,%r2			# int
	jg	sys_mlockall		# branch to system call

#sys32_munlockall_wrapper		# void 

	.globl  sys32_sched_setparam_wrapper 
sys32_sched_setparam_wrapper:
	lgfr	%r2,%r2			# pid_t
	llgtr	%r3,%r3			# struct sched_param *
	jg	sys_sched_setparam	# branch to system call

	.globl  sys32_sched_getparam_wrapper 
sys32_sched_getparam_wrapper:
	lgfr	%r2,%r2			# pid_t
	llgtr	%r3,%r3			# struct sched_param *
	jg	sys_sched_getparam	# branch to system call

	.globl  sys32_sched_setscheduler_wrapper 
sys32_sched_setscheduler_wrapper:
	lgfr	%r2,%r2			# pid_t
	lgfr	%r3,%r3			# int
	llgtr	%r4,%r4			# struct sched_param *
	jg	sys_sched_setscheduler	# branch to system call

	.globl  sys32_sched_getscheduler_wrapper 
sys32_sched_getscheduler_wrapper:
	lgfr	%r2,%r2			# pid_t
	jg	sys_sched_getscheduler	# branch to system call

#sys32_sched_yield_wrapper		# void 

	.globl  sys32_sched_get_priority_max_wrapper 
sys32_sched_get_priority_max_wrapper:
	lgfr	%r2,%r2			# int
	jg	sys_sched_get_priority_max	# branch to system call

	.globl  sys32_sched_get_priority_min_wrapper 
sys32_sched_get_priority_min_wrapper:
	lgfr	%r2,%r2			# int
	jg	sys_sched_get_priority_min	# branch to system call

	.globl  sys32_sched_rr_get_interval_wrapper 
sys32_sched_rr_get_interval_wrapper:
	lgfr	%r2,%r2			# pid_t
	llgtr	%r3,%r3			# struct timespec_emu31 *
	jg	sys32_sched_rr_get_interval	# branch to system call

	.globl  sys32_nanosleep_wrapper 
sys32_nanosleep_wrapper:
	llgtr	%r2,%r2			# struct timespec_emu31 *
	llgtr	%r3,%r3			# struct timespec_emu31 *
	jg	sys32_nanosleep		# branch to system call

	.globl  sys32_mremap_wrapper 
sys32_mremap_wrapper:
	llgfr	%r2,%r2			# unsigned long
	llgfr	%r3,%r3			# unsigned long
	llgfr	%r4,%r4			# unsigned long
	llgfr	%r5,%r5			# unsigned long
	llgfr	%r6,%r6			# unsigned long
	jg	sys_mremap		# branch to system call

	.globl  sys32_setresuid16_wrapper 
sys32_setresuid16_wrapper:
	llgfr	%r2,%r2			# __kernel_old_uid_emu31_t 
	llgfr	%r3,%r3			# __kernel_old_uid_emu31_t 
	llgfr	%r4,%r4			# __kernel_old_uid_emu31_t 
	jg	sys32_setresuid16	# branch to system call

	.globl  sys32_getresuid16_wrapper 
sys32_getresuid16_wrapper:
	llgtr	%r2,%r2			# __kernel_old_uid_emu31_t *
	llgtr	%r3,%r3			# __kernel_old_uid_emu31_t *
	llgtr	%r4,%r4			# __kernel_old_uid_emu31_t *
	jg	sys32_getresuid16	# branch to system call

	.globl  sys32_query_module_wrapper 
sys32_query_module_wrapper:
	llgtr	%r2,%r2			# const char * 
	lgfr	%r3,%r3			# int 
	llgtr	%r4,%r4			# char * 
	llgfr	%r5,%r5			# size_t 
	llgtr	%r6,%r6			# size_t * 
	jg	sys32_query_module	# branch to system call

	.globl  sys32_poll_wrapper 
sys32_poll_wrapper:
	llgtr	%r2,%r2			# struct pollfd * 
	llgfr	%r3,%r3			# unsigned int 
	lgfr	%r4,%r4			# long 
	jg	sys_poll		# branch to system call

	.globl  sys32_nfsservctl_wrapper 
sys32_nfsservctl_wrapper:
	lgfr	%r2,%r2			# int 
	llgtr	%r3,%r3			# struct nfsctl_arg_emu31 * 
	llgtr	%r4,%r4			# union nfsctl_res_emu31 * 
	jg	sys32_nfsservctl	# branch to system call

	.globl  sys32_setresgid16_wrapper 
sys32_setresgid16_wrapper:
	llgfr	%r2,%r2			# __kernel_old_gid_emu31_t 
	llgfr	%r3,%r3			# __kernel_old_gid_emu31_t 
	llgfr	%r4,%r4			# __kernel_old_gid_emu31_t 
	jg	sys32_setresgid16	# branch to system call

	.globl  sys32_getresgid16_wrapper 
sys32_getresgid16_wrapper:
	llgtr	%r2,%r2			# __kernel_old_gid_emu31_t *
	llgtr	%r3,%r3			# __kernel_old_gid_emu31_t *
	llgtr	%r4,%r4			# __kernel_old_gid_emu31_t *
	jg	sys32_getresgid16	# branch to system call

	.globl  sys32_prctl_wrapper 
sys32_prctl_wrapper:
	lgfr	%r2,%r2			# int
	llgfr	%r3,%r3			# unsigned long
	llgfr	%r4,%r4			# unsigned long
	llgfr	%r5,%r5			# unsigned long
	llgfr	%r6,%r6			# unsigned long
	jg	sys_prctl		# branch to system call

#sys32_rt_sigreturn_wrapper		# done in rt_sigreturn_glue 

	.globl  sys32_rt_sigaction_wrapper 
sys32_rt_sigaction_wrapper:
	lgfr	%r2,%r2			# int
	llgtr	%r3,%r3			# const struct sigaction_emu31 *
	llgtr	%r4,%r4			# const struct sigaction_emu31 *
	llgfr	%r5,%r5			# size_t
	jg	sys32_rt_sigaction	# branch to system call

	.globl  sys32_rt_sigprocmask_wrapper 
sys32_rt_sigprocmask_wrapper:
	lgfr	%r2,%r2			# int
	llgtr	%r3,%r3			# old_sigset_emu31 *
	llgtr	%r4,%r4			# old_sigset_emu31 *
	jg	sys32_rt_sigprocmask	# branch to system call

	.globl  sys32_rt_sigpending_wrapper 
sys32_rt_sigpending_wrapper:
	llgtr	%r2,%r2			# sigset_emu31 *
	llgfr	%r3,%r3			# size_t
	jg	sys32_rt_sigpending	# branch to system call

	.globl  sys32_rt_sigtimedwait_wrapper 
sys32_rt_sigtimedwait_wrapper:
	llgtr	%r2,%r2			# const sigset_emu31_t *
	llgtr	%r3,%r3			# siginfo_emu31_t *
	llgtr	%r4,%r4			# const struct timespec_emu31 *
	llgfr	%r5,%r5			# size_t
	jg	sys32_rt_sigtimedwait	# branch to system call

	.globl  sys32_rt_sigqueueinfo_wrapper 
sys32_rt_sigqueueinfo_wrapper:
	lgfr	%r2,%r2			# int
	lgfr	%r3,%r3			# int
	llgtr	%r4,%r4			# siginfo_emu31_t *
	jg	sys32_rt_sigqueueinfo	# branch to system call

#sys32_rt_sigsuspend_wrapper		# done in rt_sigsuspend_glue 

	.globl  sys32_pread_wrapper 
sys32_pread_wrapper:
	llgfr	%r2,%r2			# unsigned int
	llgtr	%r3,%r3			# char *
	llgfr	%r4,%r4			# size_t
	llgfr	%r5,%r5			# u32
	llgfr	%r6,%r6			# u32
	jg	sys32_pread		# branch to system call

	.globl  sys32_pwrite_wrapper 
sys32_pwrite_wrapper:
	llgfr	%r2,%r2			# unsigned int
	llgtr	%r3,%r3			# const char *
	llgfr	%r4,%r4			# size_t
	llgfr	%r5,%r5			# u32
	llgfr	%r6,%r6			# u32
	jg	sys32_pwrite		# branch to system call

	.globl  sys32_chown16_wrapper 
sys32_chown16_wrapper:
	llgtr	%r2,%r2			# const char *
	llgfr	%r3,%r3			# __kernel_old_uid_emu31_t 
	llgfr	%r4,%r4			# __kernel_old_gid_emu31_t 
	jg	sys32_chown16		# branch to system call

	.globl  sys32_getcwd_wrapper 
sys32_getcwd_wrapper:
	llgtr	%r2,%r2			# char *
	llgfr	%r3,%r3			# unsigned long
	jg	sys_getcwd		# branch to system call

	.globl  sys32_capget_wrapper 
sys32_capget_wrapper:
	llgtr	%r2,%r2			# cap_user_header_t
	llgtr	%r3,%r3			# cap_user_data_t
	jg	sys_capget		# branch to system call

	.globl  sys32_capset_wrapper 
sys32_capset_wrapper:
	llgtr	%r2,%r2			# cap_user_header_t
	llgtr	%r3,%r3			# const cap_user_data_t
	jg	sys_capset		# branch to system call

	.globl sys32_sigaltstack_wrapper
sys32_sigaltstack_wrapper:
	llgtr	%r2,%r2			# const stack_emu31_t * 
	llgtr	%r3,%r3			# stack_emu31_t * 
	jg	sys32_sigaltstack

	.globl  sys32_sendfile_wrapper 
sys32_sendfile_wrapper:
	lgfr	%r2,%r2			# int
	lgfr	%r3,%r3			# int
	llgtr	%r4,%r4			# __kernel_off_emu31_t *
	llgfr	%r5,%r5			# size_t
	jg	sys32_sendfile		# branch to system call

#sys32_vfork_wrapper			# done in vfork_glue 

	.globl  sys32_truncate64_wrapper 
sys32_truncate64_wrapper:
	llgtr	%r2,%r2			# const char *
	lgfr	%r3,%r3			# s32 
	llgfr	%r4,%r4			# u32 
	jg	sys32_truncate64	# branch to system call

	.globl  sys32_ftruncate64_wrapper 
sys32_ftruncate64_wrapper:
	llgfr	%r2,%r2			# unsigned int
	lgfr	%r3,%r3			# s32 
	llgfr	%r4,%r4			# u32 
	jg	sys32_ftruncate64	# branch to system call

	.globl sys32_lchown_wrapper	
sys32_lchown_wrapper:
	llgtr	%r2,%r2			# const char *
	llgfr	%r3,%r3			# uid_t
	llgfr	%r4,%r4			# gid_t
	jg	sys_lchown		# branch to system call

#sys32_getuid_wrapper			# void			 
#sys32_getgid_wrapper			# void 
#sys32_geteuid_wrapper			# void 
#sys32_getegid_wrapper			# void 

	.globl sys32_setreuid_wrapper
sys32_setreuid_wrapper:
	llgfr	%r2,%r2			# uid_t
	llgfr	%r3,%r3			# uid_t
	jg	sys_setreuid		# branch to system call

	.globl sys32_setregid_wrapper
sys32_setregid_wrapper:
	llgfr	%r2,%r2			# gid_t
	llgfr	%r3,%r3			# gid_t
	jg	sys_setregid		# branch to system call

	.globl  sys32_getgroups_wrapper 
sys32_getgroups_wrapper:
	lgfr	%r2,%r2			# int
	llgtr	%r3,%r3			# gid_t *
	jg	sys_getgroups		# branch to system call

	.globl  sys32_setgroups_wrapper 
sys32_setgroups_wrapper:
	lgfr	%r2,%r2			# int
	llgtr	%r3,%r3			# gid_t *
	jg	sys_setgroups		# branch to system call

	.globl sys32_fchown_wrapper	
sys32_fchown_wrapper:
	llgfr	%r2,%r2			# unsigned int
	llgfr	%r3,%r3			# uid_t
	llgfr	%r4,%r4			# gid_t
	jg	sys_fchown		# branch to system call

	.globl sys32_setresuid_wrapper	
sys32_setresuid_wrapper:
	llgfr	%r2,%r2			# uid_t
	llgfr	%r3,%r3			# uid_t
	llgfr	%r4,%r4			# uid_t
	jg	sys_setresuid		# branch to system call

	.globl sys32_getresuid_wrapper	
sys32_getresuid_wrapper:
	llgtr	%r2,%r2			# uid_t *
	llgtr	%r3,%r3			# uid_t *
	llgtr	%r4,%r4			# uid_t *
	jg	sys_getresuid		# branch to system call

	.globl sys32_setresgid_wrapper	
sys32_setresgid_wrapper:
	llgfr	%r2,%r2			# gid_t
	llgfr	%r3,%r3			# gid_t
	llgfr	%r4,%r4			# gid_t
	jg	sys_setresgid		# branch to system call

	.globl sys32_getresgid_wrapper	
sys32_getresgid_wrapper:
	llgtr	%r2,%r2			# gid_t *
	llgtr	%r3,%r3			# gid_t *
	llgtr	%r4,%r4			# gid_t *
	jg	sys_getresgid		# branch to system call

	.globl sys32_chown_wrapper	
sys32_chown_wrapper:
	llgtr	%r2,%r2			# const char *
	llgfr	%r3,%r3			# uid_t
	llgfr	%r4,%r4			# gid_t
	jg	sys_chown		# branch to system call

	.globl sys32_setuid_wrapper	
sys32_setuid_wrapper:
	llgfr	%r2,%r2			# uid_t
	jg	sys_setuid		# branch to system call

	.globl sys32_setgid_wrapper	
sys32_setgid_wrapper:
	llgfr	%r2,%r2			# gid_t
	jg	sys_setgid		# branch to system call

	.globl sys32_setfsuid_wrapper	
sys32_setfsuid_wrapper:
	llgfr	%r2,%r2			# uid_t
	jg	sys_setfsuid		# branch to system call

	.globl sys32_setfsgid_wrapper	
sys32_setfsgid_wrapper:
	llgfr	%r2,%r2			# gid_t
	jg	sys_setfsgid		# branch to system call

	.globl  sys32_pivot_root_wrapper 
sys32_pivot_root_wrapper:
	llgtr	%r2,%r2			# const char *
	llgtr	%r3,%r3			# const char *
	jg	sys_pivot_root		# branch to system call

	.globl  sys32_mincore_wrapper 
sys32_mincore_wrapper:
	llgfr	%r2,%r2			# unsigned long
	llgfr	%r3,%r3			# size_t
	llgtr	%r4,%r4			# unsigned char *
	jg	sys_mincore		# branch to system call

	.globl  sys32_madvise_wrapper 
sys32_madvise_wrapper:
	llgfr	%r2,%r2			# unsigned long
	llgfr	%r3,%r3			# size_t
	lgfr	%r4,%r4			# int
	jg	sys_madvise		# branch to system call

	.globl  sys32_getdents64_wrapper 
sys32_getdents64_wrapper:
	llgfr	%r2,%r2			# unsigned int
	llgtr	%r3,%r3			# void *
	llgfr	%r4,%r4			# unsigned int
	jg	sys_getdents64		# branch to system call

	.globl  sys32_fcntl64_wrapper 
sys32_fcntl64_wrapper:
	llgfr	%r2,%r2			# unsigned int
	llgfr	%r3,%r3			# unsigned int 
	llgfr	%r4,%r4			# unsigned long
	jg	sys32_fcntl64		# branch to system call

	.globl	sys32_stat64_wrapper
sys32_stat64_wrapper:
	llgtr	%r2,%r2			# char *
	llgtr	%r3,%r3			# struct stat64 *
	llgfr	%r4,%r4			# long
	jg	sys32_stat64		# branch to system call

	.globl	sys32_lstat64_wrapper
sys32_lstat64_wrapper:
	llgtr	%r2,%r2			# char *
	llgtr	%r3,%r3			# struct stat64 *
	llgfr	%r4,%r4			# long
	jg	sys32_lstat64		# branch to system call

	.globl	sys32_fstat64_wrapper
sys32_fstat64_wrapper:
	llgfr	%r2,%r2			# unsigned long
	llgtr	%r3,%r3			# struct stat64 *
	llgfr	%r4,%r4			# long
	jg	sys32_fstat64		# branch to system call

	.globl	sys32_stime_wrapper
sys32_stime_wrapper:
	llgtr	%r2,%r2			# int *
	jg	sys_stime		# branch to system call

	.globl  sys32_sysctl_wrapper
sys32_sysctl_wrapper:
	llgtr   %r2,%r2                 # struct __sysctl_args32 *
	jg      sys32_sysctl
