/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995, 96, 97, 98, 99, 2000, 01 by Ralf Baechle
 * Copyright (C) 1999, 2000 Silicon Graphics, Inc.
 * Copyright (C) 2001 MIPS Technologies, Inc.
 */
#include <linux/config.h>
#include <asm/asm.h>
#include <linux/errno.h>
#include <asm/current.h>
#include <asm/mipsregs.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>
#include <asm/unistd.h>
#include <asm/offset.h>

#ifndef CONFIG_BINFMT_ELF32
/* Neither O32 nor N32, so define handle_sys here */
#define handle_sys64 handle_sys
#endif

	.align  5
NESTED(handle_sys64, PT_SIZE, sp)
/* When 32-bit compatibility is configured scall_o32.S already did this.  */
#if !defined(CONFIG_MIPS32_O32) && !defined(CONFIG_MIPS32_N32)
	.set	noat
	SAVE_SOME
	STI
	.set	at
#endif
	ld	t1, PT_EPC(sp)		# skip syscall on return

	subu	t0, v0, __NR_64_Linux	# check syscall number
	sltiu	t0, t0,	__NR_64_Linux_syscalls + 1
	daddiu	t1, 4			# skip to next instruction
	beqz	t0, illegal_syscall
	sd	t1, PT_EPC(sp)

	dsll	t0, v0, 3		# offset into table
	ld	t2, (sys_call_table - (__NR_64_Linux * 8))(t0) # syscall routine

	sd	a3, PT_R26(sp)		# save a3 for syscall restarting

	ld	t0, TASK_PTRACE($28)	# syscall tracing enabled?
	andi	t0, _PT_TRACESYS
	bnez	t0, trace_a_syscall

	jalr	t2			# Do The Real Thing (TM)

	li	t0, -EMAXERRNO - 1	# error?
	sltu	t0, t0, v0
	sd	t0, PT_R7(sp)		# set error flag
	beqz	t0, 1f

	negu	v0			# error
	sd	v0, PT_R0(sp)		# set flag for syscall restarting
1:	sd	v0, PT_R2(sp)		# result

ret_from_sys_call:
	mfc0	t0, CP0_STATUS
	ori	t0, t0, 1
	xori	t0, t0, 1
	mtc0	t0, CP0_STATUS
	SSNOP; SSNOP; SSNOP

	ld	t2, TASK_NEED_RESCHED($28)
	bnez	t2, _64_reschedule
	lw	v0, TASK_SIGPENDING($28)
	bnez	v0, signal_return

restore_all:
	RESTORE_SOME
	RESTORE_SP
	eret

/* Put this behind restore_all for the sake of the branch prediction.  */
signal_return:
	.type	signal_return, @function

	mfc0	t0, CP0_STATUS
	ori	t0, t0, 1
	mtc0	t0, CP0_STATUS

	SAVE_STATIC
	move	a0, zero
	move	a1, sp
	jal	do_signal
	RESTORE_STATIC
	b	restore_all

_64_reschedule:
	SAVE_STATIC
	jal	schedule
	b	ret_from_sys_call

/* ------------------------------------------------------------------------ */

trace_a_syscall:
	SAVE_STATIC
	sd	t2,PT_R1(sp)
	jal	syscall_trace
	ld	t2,PT_R1(sp)

	ld	a0, PT_R4(sp)		# Restore argument registers
	ld	a1, PT_R5(sp)
	ld	a2, PT_R6(sp)
	ld	a3, PT_R7(sp)
	jalr	t2

	li	t0, -EMAXERRNO - 1	# error?
	sltu	t0, t0, v0
	sd	t0, PT_R7(sp)		# set error flag
	beqz	t0, 1f

	negu	v0			# error
	sd	v0, PT_R0(sp)		# set flag for syscall restarting
1:	sd	v0, PT_R2(sp)		# result

	jal	syscall_trace
	j	ret_from_sys_call

illegal_syscall:
	/* This also isn't a 64-bit syscall, throw an error.  */
	li	v0, ENOSYS			# error
	sd	v0, PT_R2(sp)
	li	t0, 1				# set error flag
	sd	t0, PT_R7(sp)
	j	ret_from_sys_call
	END(handle_sys64)

	.align	3
sys_call_table:
	PTR	sys_read			/* 5000 */
	PTR	sys_write
	PTR	sys_open
	PTR	sys_close
	PTR	sys_newstat
	PTR	sys_newfstat			/* 5005 */
	PTR	sys_newlstat
	PTR	sys_poll
	PTR	sys_lseek
	PTR	sys_mmap
	PTR	sys_mprotect			/* 5010 */
	PTR	sys_munmap
	PTR	sys_brk
	PTR	sys_rt_sigaction
	PTR	sys_rt_sigprocmask
	PTR	sys_ioctl			/* 5015 */
	PTR	sys_pread
	PTR	sys_pwrite
	PTR	sys_readv
	PTR	sys_writev
	PTR	sys_access			/* 5020 */
	PTR	sys_pipe
	PTR	sys_select
	PTR	sys_sched_yield
	PTR	sys_mremap
	PTR	sys_msync			/* 5025 */
	PTR	sys_mincore
	PTR	sys_madvise
	PTR	sys_shmget
	PTR	sys_shmat
	PTR	sys_shmctl			/* 5030 */
	PTR	sys_dup
	PTR	sys_dup2
	PTR	sys_pause
	PTR	sys_nanosleep
	PTR	sys_getitimer			/* 5035 */
	PTR	sys_setitimer
	PTR	sys_alarm
	PTR	sys_getpid
	PTR	sys_sendfile
	PTR	sys_socket			/* 5040 */
	PTR	sys_connect
	PTR	sys_accept
	PTR	sys_sendto
	PTR	sys_recvfrom
	PTR	sys_sendmsg			/* 5045 */
	PTR	sys_recvmsg
	PTR	sys_shutdown
	PTR	sys_bind
	PTR	sys_listen
	PTR	sys_getsockname			/* 5050 */
	PTR	sys_getpeername
	PTR	sys_socketpair
	PTR	sys_setsockopt
	PTR	sys_getsockopt
	PTR	sys_clone			/* 5055 */
	PTR	sys_fork
	PTR	sys_execve
	PTR	sys_exit
	PTR	sys_wait4
	PTR	sys_kill			/* 5060 */
	PTR	sys_newuname
	PTR	sys_semget
	PTR	sys_semop
	PTR	sys_semctl
	PTR	sys_shmdt			/* 5065 */
	PTR	sys_msgget
	PTR	sys_msgsnd
	PTR	sys_msgrcv
	PTR	sys_msgctl
	PTR	sys_fcntl			/* 5070 */
	PTR	sys_flock
	PTR	sys_fsync
	PTR	sys_fdatasync
	PTR	sys_truncate
	PTR	sys_ftruncate			/* 5075 */
	PTR	sys_getdents
	PTR	sys_getcwd
	PTR	sys_chdir
	PTR	sys_fchdir
	PTR	sys_rename			/* 5080 */
	PTR	sys_mkdir
	PTR	sys_rmdir
	PTR	sys_creat
	PTR	sys_link
	PTR	sys_unlink			/* 5085 */
	PTR	sys_symlink
	PTR	sys_readlink
	PTR	sys_chmod
	PTR	sys_fchmod
	PTR	sys_chown			/* 5090 */
	PTR	sys_fchown
	PTR	sys_lchown
	PTR	sys_umask
	PTR	sys_gettimeofday
	PTR	sys_getrlimit			/* 5095 */
	PTR	sys_getrusage
	PTR	sys_sysinfo
	PTR	sys_times
	PTR	sys_ptrace
	PTR	sys_getuid			/* 5100 */
	PTR	sys_syslog
	PTR	sys_getgid
	PTR	sys_setuid
	PTR	sys_setgid
	PTR	sys_geteuid			/* 5105 */
	PTR	sys_getegid
	PTR	sys_setpgid
	PTR	sys_getppid
	PTR	sys_getpgrp
	PTR	sys_setsid			/* 5110 */
	PTR	sys_setreuid
	PTR	sys_setregid
	PTR	sys_getgroups
	PTR	sys_setgroups
	PTR	sys_setresuid			/* 5115 */
	PTR	sys_getresuid
	PTR	sys_setresgid
	PTR	sys_getresgid
	PTR	sys_getpgid
	PTR	sys_setfsuid			/* 5120 */
	PTR	sys_setfsgid
	PTR	sys_getsid
	PTR	sys_capget
	PTR	sys_capset
	PTR	sys_rt_sigpending		/* 5125 */
	PTR	sys_rt_sigtimedwait
	PTR	sys_rt_sigqueueinfo
	PTR	sys_rt_sigsuspend
	PTR	sys_sigaltstack
	PTR	sys_utime			/* 5130 */
	PTR	sys_mknod
	PTR	sys_personality
	PTR	sys_ustat
	PTR	sys_statfs
	PTR	sys_fstatfs			/* 5135 */
	PTR	sys_sysfs
	PTR	sys_getpriority
	PTR	sys_setpriority
	PTR	sys_sched_setparam
	PTR	sys_sched_getparam		/* 5140 */
	PTR	sys_sched_setscheduler
	PTR	sys_sched_getscheduler
	PTR	sys_sched_get_priority_max
	PTR	sys_sched_get_priority_min
	PTR	sys_sched_rr_get_interval	/* 5145 */
	PTR	sys_mlock
	PTR	sys_munlock
	PTR	sys_mlockall
	PTR	sys_munlockall
	PTR	sys_vhangup			/* 5150 */
	PTR	sys_pivot_root
	PTR	sys_sysctl
	PTR	sys_prctl
	PTR	sys_adjtimex
	PTR	sys_setrlimit			/* 5155 */
	PTR	sys_chroot
	PTR	sys_sync
	PTR	sys_acct
	PTR	sys_settimeofday
	PTR	sys_mount			/* 5160 */
	PTR	sys_umount
	PTR	sys_swapon
	PTR	sys_swapoff
	PTR	sys_reboot
	PTR	sys_sethostname			/* 5165 */
	PTR	sys_setdomainname
	PTR	sys_create_module
	PTR	sys_init_module
	PTR	sys_delete_module
	PTR	sys_get_kernel_syms		/* 5170 */
	PTR	sys_query_module
	PTR	sys_quotactl
	PTR	sys_nfsservctl
	PTR	sys_ni_syscall			/* res. for getpmsg */
	PTR	sys_ni_syscall			/* 5175  for putpmsg */
	PTR	sys_ni_syscall			/* res. for afs_syscall */
	PTR	sys_ni_syscall			/* res. for security */
	PTR	sys_gettid
	PTR	sys_readahead
	PTR	sys_setxattr			/* 5180 */
	PTR	sys_lsetxattr
	PTR	sys_fsetxattr
	PTR	sys_getxattr
	PTR	sys_lgetxattr
	PTR	sys_fgetxattr			/* 5185 */
	PTR	sys_listxattr
	PTR	sys_llistxattr
	PTR	sys_flistxattr
	PTR	sys_removexattr
	PTR	sys_lremovexattr		/* 5190 */
	PTR	sys_fremovexattr
	PTR	sys_tkill
	PTR	sys_ni_syscall
	PTR	sys_ni_syscall			/* res. for futex */
	PTR	sys_ni_syscall			/* 5195 rs. sched_setaffinity */
	PTR	sys_ni_syscall			/* res. f. sched_getaffinity */
	PTR	sys_cacheflush
	PTR	sys_cachectl
	PTR	sys_sysmips
	PTR	sys_ni_syscall			/* 5200 */
	PTR	sys_ni_syscall
	PTR	sys_ni_syscall
	PTR	sys_ni_syscall
	PTR	sys_ni_syscall
	PTR	sys_ni_syscall			/* 5205 */
	PTR	sys_ni_syscall
	PTR	sys_ni_syscall
	PTR	sys_ni_syscall
	PTR	sys_ni_syscall
	PTR	sys_ni_syscall			/* 5210 */
	PTR	sys_rt_sigreturn
	PTR	sys_ni_syscall
	PTR	sys_ni_syscall
	PTR	sys_semtimedop
