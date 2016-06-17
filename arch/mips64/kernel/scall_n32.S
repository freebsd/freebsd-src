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

/* This duplicates the definition from <linux/sched.h> */
#define PT_TRACESYS	0x00000002	/* tracing system calls */

/* This duplicates the definition from <asm/signal.h> */
#define SIGILL		4		/* Illegal instruction (ANSI).  */

#ifndef CONFIG_MIPS32_O32
/* No O32, so define handle_sys here */
#define handle_sysn32 handle_sys
#endif

	.align  5
NESTED(handle_sysn32, PT_SIZE, sp)
#ifndef CONFIG_MIPS32_O32
	.set	noat
	SAVE_SOME
	STI
	.set	at
#endif
	ld	t1, PT_EPC(sp)		# skip syscall on return

	subu	t0, v0, __NR_N32_Linux	# check syscall number
	sltiu	t0, t0,	__NR_N32_Linux_syscalls + 1
	daddiu	t1, 4			# skip to next instruction
	beqz	t0, not_n32_scall
	sd	t1, PT_EPC(sp)

	dsll	t0, v0, 3		# offset into table
	ld	t2, (sysn32_call_table - (__NR_N32_Linux * 8))(t0)

	sd	a3, PT_R26(sp)		# save a3 for syscall restarting

	ld	t0, TASK_PTRACE($28)	# syscall tracing enabled?
	andi	t0, PT_TRACESYS
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
	xori	t0, t0, 1
	ori	t0, t0, 1
	mtc0	t0, CP0_STATUS

	ld	t2, TASK_NEED_RESCHED($28)
	bnez	t2, n32_reschedule
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

n32_reschedule:
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

not_n32_scall:
	/* This is not an n32 compatibility syscall, pass it on to
	   the n64 syscall handlers.  */
	j	handle_sys64

	END(handle_sysn32)

EXPORT(sysn32_call_table)
	PTR	sys_read			/* 6000 */
	PTR	sys_write
	PTR	sys_open
	PTR	sys_close
	PTR	sys_newstat
	PTR	sys_newfstat			/* 6005 */
	PTR	sys_newlstat
	PTR	sys_poll
	PTR	sys_lseek
	PTR	sys_mmap
	PTR	sys_mprotect			/* 6010 */
	PTR	sys_munmap
	PTR	sys_brk
	PTR	sys32_rt_sigaction
	PTR	sys32_rt_sigprocmask
	PTR	sys32_ioctl			/* 6015 */
	PTR	sys_pread
	PTR	sys_pwrite
	PTR	sys32_readv
	PTR	sys32_writev
	PTR	sys_access			/* 6020 */
	PTR	sys_pipe
	PTR	sys32_select
	PTR	sys_sched_yield
	PTR	sys_mremap
	PTR	sys_msync			/* 6025 */
	PTR	sys_mincore
	PTR	sys_madvise
	PTR	sys_shmget
	PTR	sys_shmat
	PTR	sys_shmctl			/* 6030 */
	PTR	sys_dup
	PTR	sys_dup2
	PTR	sys_pause
	PTR	sys32_nanosleep
	PTR	sys32_getitimer			/* 6035 */
	PTR	sys32_setitimer
	PTR	sys_alarm
	PTR	sys_getpid
	PTR	sys32_sendfile
	PTR	sys_socket			/* 6040 */
	PTR	sys_connect
	PTR	sys_accept
	PTR	sys_sendto
	PTR	sys_recvfrom
	PTR	sys32_sendmsg			/* 6045 */
	PTR	sys32_recvmsg
	PTR	sys_shutdown
	PTR	sys_bind
	PTR	sys_listen
	PTR	sys_getsockname			/* 6050 */
	PTR	sys_getpeername
	PTR	sys_socketpair
	PTR	sys32_setsockopt
	PTR	sys_getsockopt
	PTR	sys_clone			/* 6055 */
	PTR	sys_fork
	PTR	sys32_execve
	PTR	sys_exit
	PTR	sys32_wait4
	PTR	sys_kill			/* 6060 */
	PTR	sys32_newuname
	PTR	sys_semget
	PTR	sys_semop
	PTR	sys_semctl
	PTR	sys_shmdt			/* 6065 */
	PTR	sys_msgget
	PTR	sys_msgsnd
	PTR	sys_msgrcv
	PTR	sys_msgctl
	PTR	sys32_fcntl			/* 6070 */
	PTR	sys_flock
	PTR	sys_fsync
	PTR	sys_fdatasync
	PTR	sys_truncate
	PTR	sys_ftruncate			/* 6075 */
	PTR	sys32_getdents
	PTR	sys_getcwd
	PTR	sys_chdir
	PTR	sys_fchdir
	PTR	sys_rename			/* 6080 */
	PTR	sys_mkdir
	PTR	sys_rmdir
	PTR	sys_creat
	PTR	sys_link
	PTR	sys_unlink			/* 6085 */
	PTR	sys_symlink
	PTR	sys_readlink
	PTR	sys_chmod
	PTR	sys_fchmod
	PTR	sys_chown			/* 6090 */
	PTR	sys_fchown
	PTR	sys_lchown
	PTR	sys_umask
	PTR	sys32_gettimeofday
	PTR	sys32_getrlimit			/* 6095 */
	PTR	sys32_getrusage
	PTR	sys32_sysinfo
	PTR	sys32_times
	PTR	sys_ptrace
	PTR	sys_getuid			/* 6100 */
	PTR	sys_syslog
	PTR	sys_getgid
	PTR	sys_setuid
	PTR	sys_setgid
	PTR	sys_geteuid			/* 6105 */
	PTR	sys_getegid
	PTR	sys_setpgid
	PTR	sys_getppid
	PTR	sys_getpgrp
	PTR	sys_setsid			/* 6110 */
	PTR	sys_setreuid
	PTR	sys_setregid
	PTR	sys_getgroups
	PTR	sys_setgroups
	PTR	sys_setresuid			/* 6115 */
	PTR	sys_getresuid
	PTR	sys_setresgid
	PTR	sys_getresgid
	PTR	sys_getpgid
	PTR	sys_setfsuid			/* 6120 */
	PTR	sys_setfsgid
	PTR	sys_getsid
	PTR	sys_capget
	PTR	sys_capset
	PTR	sys32_rt_sigpending		/* 6125 */
	PTR	sys32_rt_sigtimedwait
	PTR	sys32_rt_sigqueueinfo
	PTR	sys32_rt_sigsuspend
	PTR	sys32_sigaltstack
	PTR	sys32_utime			/* 6130 */
	PTR	sys_mknod
	PTR	sys32_personality
	PTR	sys_ustat
	PTR	sys32_statfs
	PTR	sys32_fstatfs			/* 6135 */
	PTR	sys_sysfs
	PTR	sys_getpriority
	PTR	sys_setpriority
	PTR	sys_sched_setparam
	PTR	sys_sched_getparam		/* 6140 */
	PTR	sys_sched_setscheduler
	PTR	sys_sched_getscheduler
	PTR	sys_sched_get_priority_max
	PTR	sys_sched_get_priority_min
	PTR	sys32_sched_rr_get_interval	/* 6145 */
	PTR	sys_mlock
	PTR	sys_munlock
	PTR	sys_mlockall
	PTR	sys_munlockall
	PTR	sys_vhangup			/* 6150 */
	PTR	sys_pivot_root
	PTR	sys32_sysctl
	PTR	sys_prctl
	PTR	sys32_adjtimex
	PTR	sys32_setrlimit			/* 6155 */
	PTR	sys_chroot
	PTR	sys_sync
	PTR	sys_acct
	PTR	sys32_settimeofday
	PTR	sys_mount			/* 6160 */
	PTR	sys_umount
	PTR	sys_swapon
	PTR	sys_swapoff
	PTR	sys_reboot
	PTR	sys_sethostname			/* 6165 */
	PTR	sys_setdomainname
	PTR	sys_create_module
	PTR	sys_init_module
	PTR	sys_delete_module
	PTR	sys_get_kernel_syms		/* 6170 */
	PTR	sys_query_module
	PTR	sys_quotactl
	PTR	sys_nfsservctl
	PTR	sys_ni_syscall			/* res. for getpmsg */
	PTR	sys_ni_syscall			/* 6175  for putpmsg */
	PTR	sys_ni_syscall			/* res. for afs_syscall */
	PTR	sys_ni_syscall			/* res. for security */
	PTR	sys_gettid
	PTR	sys32_readahead
	PTR	sys_setxattr			/* 6180 */
	PTR	sys_lsetxattr
	PTR	sys_fsetxattr
	PTR	sys_getxattr
	PTR	sys_lgetxattr
	PTR	sys_fgetxattr			/* 6185 */
	PTR	sys_listxattr
	PTR	sys_llistxattr
	PTR	sys_flistxattr
	PTR	sys_removexattr
	PTR	sys_lremovexattr		/* 6190 */
	PTR	sys_fremovexattr
	PTR	sys_tkill
	PTR	sys_ni_syscall
	PTR	sys_ni_syscall			/* res. for futex */
	PTR	sys_ni_syscall			/* 6195 rs. sched_setaffinity */
	PTR	sys_ni_syscall			/* res. f. sched_getaffinity */
	PTR	sys_cacheflush
	PTR	sys_cachectl
	PTR	sys_sysmips
	PTR	sys_ni_syscall			/* 6200 */
	PTR	sys_ni_syscall
	PTR	sys_ni_syscall
	PTR	sys_ni_syscall
	PTR	sys_ni_syscall
	PTR	sys_ni_syscall			/* 6205 */
	PTR	sys_ni_syscall
	PTR	sys_ni_syscall
	PTR	sys_ni_syscall
	PTR	sys_ni_syscall
	PTR	sys_ni_syscall			/* 6210 */
	PTR	sysn32_rt_sigreturn
	PTR	sys_fcntl
	PTR	sys_ni_syscall
	PTR	sys_ni_syscall
	PTR	sys_semtimedop			/* 6215 */
	PTR	sys_ni_syscall
	PTR	sys_ni_syscall
	PTR	sys_ni_syscall
	PTR	sys_sendfile64
