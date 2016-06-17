#ifndef _ALPHA_UNISTD_H
#define _ALPHA_UNISTD_H

#define __NR_osf_syscall	  0	/* not implemented */
#define __NR_exit		  1
#define __NR_fork		  2
#define __NR_read		  3
#define __NR_write		  4
#define __NR_osf_old_open	  5	/* not implemented */
#define __NR_close		  6
#define __NR_osf_wait4		  7
#define __NR_osf_old_creat	  8	/* not implemented */
#define __NR_link		  9
#define __NR_unlink		 10
#define __NR_osf_execve		 11	/* not implemented */
#define __NR_chdir		 12
#define __NR_fchdir		 13
#define __NR_mknod		 14
#define __NR_chmod		 15
#define __NR_chown		 16
#define __NR_brk		 17
#define __NR_osf_getfsstat	 18	/* not implemented */
#define __NR_lseek		 19
#define __NR_getxpid		 20
#define __NR_osf_mount		 21
#define __NR_umount		 22
#define __NR_setuid		 23
#define __NR_getxuid		 24
#define __NR_exec_with_loader	 25	/* not implemented */
#define __NR_ptrace		 26
#define __NR_osf_nrecvmsg	 27	/* not implemented */
#define __NR_osf_nsendmsg	 28	/* not implemented */
#define __NR_osf_nrecvfrom	 29	/* not implemented */
#define __NR_osf_naccept	 30	/* not implemented */
#define __NR_osf_ngetpeername	 31	/* not implemented */
#define __NR_osf_ngetsockname	 32	/* not implemented */
#define __NR_access		 33
#define __NR_osf_chflags	 34	/* not implemented */
#define __NR_osf_fchflags	 35	/* not implemented */
#define __NR_sync		 36
#define __NR_kill		 37
#define __NR_osf_old_stat	 38	/* not implemented */
#define __NR_setpgid		 39
#define __NR_osf_old_lstat	 40	/* not implemented */
#define __NR_dup		 41
#define __NR_pipe		 42
#define __NR_osf_set_program_attributes	43
#define __NR_osf_profil		 44	/* not implemented */
#define __NR_open		 45
#define __NR_osf_old_sigaction	 46	/* not implemented */
#define __NR_getxgid		 47
#define __NR_osf_sigprocmask	 48
#define __NR_osf_getlogin	 49	/* not implemented */
#define __NR_osf_setlogin	 50	/* not implemented */
#define __NR_acct		 51
#define __NR_sigpending		 52

#define __NR_ioctl		 54
#define __NR_osf_reboot		 55	/* not implemented */
#define __NR_osf_revoke		 56	/* not implemented */
#define __NR_symlink		 57
#define __NR_readlink		 58
#define __NR_execve		 59
#define __NR_umask		 60
#define __NR_chroot		 61
#define __NR_osf_old_fstat	 62	/* not implemented */
#define __NR_getpgrp		 63
#define __NR_getpagesize	 64
#define __NR_osf_mremap		 65	/* not implemented */
#define __NR_vfork		 66
#define __NR_stat		 67
#define __NR_lstat		 68
#define __NR_osf_sbrk		 69	/* not implemented */
#define __NR_osf_sstk		 70	/* not implemented */
#define __NR_mmap		 71	/* OSF/1 mmap is superset of Linux */
#define __NR_osf_old_vadvise	 72	/* not implemented */
#define __NR_munmap		 73
#define __NR_mprotect		 74
#define __NR_madvise		 75
#define __NR_vhangup		 76
#define __NR_osf_kmodcall	 77	/* not implemented */
#define __NR_osf_mincore	 78	/* not implemented */
#define __NR_getgroups		 79
#define __NR_setgroups		 80
#define __NR_osf_old_getpgrp	 81	/* not implemented */
#define __NR_setpgrp		 82	/* BSD alias for setpgid */
#define __NR_osf_setitimer	 83
#define __NR_osf_old_wait	 84	/* not implemented */
#define __NR_osf_table		 85	/* not implemented */
#define __NR_osf_getitimer	 86
#define __NR_gethostname	 87
#define __NR_sethostname	 88
#define __NR_getdtablesize	 89
#define __NR_dup2		 90
#define __NR_fstat		 91
#define __NR_fcntl		 92
#define __NR_osf_select		 93
#define __NR_poll		 94
#define __NR_fsync		 95
#define __NR_setpriority	 96
#define __NR_socket		 97
#define __NR_connect		 98
#define __NR_accept		 99
#define __NR_getpriority	100
#define __NR_send		101
#define __NR_recv		102
#define __NR_sigreturn		103
#define __NR_bind		104
#define __NR_setsockopt		105
#define __NR_listen		106
#define __NR_osf_plock		107	/* not implemented */
#define __NR_osf_old_sigvec	108	/* not implemented */
#define __NR_osf_old_sigblock	109	/* not implemented */
#define __NR_osf_old_sigsetmask	110	/* not implemented */
#define __NR_sigsuspend		111
#define __NR_osf_sigstack	112
#define __NR_recvmsg		113
#define __NR_sendmsg		114
#define __NR_osf_old_vtrace	115	/* not implemented */
#define __NR_osf_gettimeofday	116
#define __NR_osf_getrusage	117
#define __NR_getsockopt		118

#define __NR_readv		120
#define __NR_writev		121
#define __NR_osf_settimeofday	122
#define __NR_fchown		123
#define __NR_fchmod		124
#define __NR_recvfrom		125
#define __NR_setreuid		126
#define __NR_setregid		127
#define __NR_rename		128
#define __NR_truncate		129
#define __NR_ftruncate		130
#define __NR_flock		131
#define __NR_setgid		132
#define __NR_sendto		133
#define __NR_shutdown		134
#define __NR_socketpair		135
#define __NR_mkdir		136
#define __NR_rmdir		137
#define __NR_osf_utimes		138
#define __NR_osf_old_sigreturn	139	/* not implemented */
#define __NR_osf_adjtime	140	/* not implemented */
#define __NR_getpeername	141
#define __NR_osf_gethostid	142	/* not implemented */
#define __NR_osf_sethostid	143	/* not implemented */
#define __NR_getrlimit		144
#define __NR_setrlimit		145
#define __NR_osf_old_killpg	146	/* not implemented */
#define __NR_setsid		147
#define __NR_quotactl		148
#define __NR_osf_oldquota	149	/* not implemented */
#define __NR_getsockname	150

#define __NR_osf_pid_block	153	/* not implemented */
#define __NR_osf_pid_unblock	154	/* not implemented */

#define __NR_sigaction		156
#define __NR_osf_sigwaitprim	157	/* not implemented */
#define __NR_osf_nfssvc		158	/* not implemented */
#define __NR_osf_getdirentries	159
#define __NR_osf_statfs		160
#define __NR_osf_fstatfs	161

#define __NR_osf_asynch_daemon	163	/* not implemented */
#define __NR_osf_getfh		164	/* not implemented */	
#define __NR_osf_getdomainname	165
#define __NR_setdomainname	166

#define __NR_osf_exportfs	169	/* not implemented */

#define __NR_osf_alt_plock	181	/* not implemented */

#define __NR_osf_getmnt		184	/* not implemented */

#define __NR_osf_alt_sigpending	187	/* not implemented */
#define __NR_osf_alt_setsid	188	/* not implemented */

#define __NR_osf_swapon		199
#define __NR_msgctl		200
#define __NR_msgget		201
#define __NR_msgrcv		202
#define __NR_msgsnd		203
#define __NR_semctl		204
#define __NR_semget		205
#define __NR_semop		206
#define __NR_osf_utsname	207
#define __NR_lchown		208
#define __NR_osf_shmat		209
#define __NR_shmctl		210
#define __NR_shmdt		211
#define __NR_shmget		212
#define __NR_osf_mvalid		213	/* not implemented */
#define __NR_osf_getaddressconf	214	/* not implemented */
#define __NR_osf_msleep		215	/* not implemented */
#define __NR_osf_mwakeup	216	/* not implemented */
#define __NR_msync		217
#define __NR_osf_signal		218	/* not implemented */
#define __NR_osf_utc_gettime	219	/* not implemented */
#define __NR_osf_utc_adjtime	220	/* not implemented */

#define __NR_osf_security	222	/* not implemented */
#define __NR_osf_kloadcall	223	/* not implemented */

#define __NR_getpgid		233
#define __NR_getsid		234
#define __NR_sigaltstack	235
#define __NR_osf_waitid		236	/* not implemented */
#define __NR_osf_priocntlset	237	/* not implemented */
#define __NR_osf_sigsendset	238	/* not implemented */
#define __NR_osf_set_speculative	239	/* not implemented */
#define __NR_osf_msfs_syscall	240	/* not implemented */
#define __NR_osf_sysinfo	241
#define __NR_osf_uadmin		242	/* not implemented */
#define __NR_osf_fuser		243	/* not implemented */
#define __NR_osf_proplist_syscall    244
#define __NR_osf_ntp_adjtime	245	/* not implemented */
#define __NR_osf_ntp_gettime	246	/* not implemented */
#define __NR_osf_pathconf	247	/* not implemented */
#define __NR_osf_fpathconf	248	/* not implemented */

#define __NR_osf_uswitch	250	/* not implemented */
#define __NR_osf_usleep_thread	251
#define __NR_osf_audcntl	252	/* not implemented */
#define __NR_osf_audgen		253	/* not implemented */
#define __NR_sysfs		254
#define __NR_osf_subsys_info	255	/* not implemented */
#define __NR_osf_getsysinfo	256
#define __NR_osf_setsysinfo	257
#define __NR_osf_afs_syscall	258	/* not implemented */
#define __NR_osf_swapctl	259	/* not implemented */
#define __NR_osf_memcntl	260	/* not implemented */
#define __NR_osf_fdatasync	261	/* not implemented */


/*
 * Linux-specific system calls begin at 300
 */
#define __NR_bdflush		300
#define __NR_sethae		301
#define __NR_mount		302
#define __NR_old_adjtimex	303
#define __NR_swapoff		304
#define __NR_getdents		305
#define __NR_create_module	306
#define __NR_init_module	307
#define __NR_delete_module	308
#define __NR_get_kernel_syms	309
#define __NR_syslog		310
#define __NR_reboot		311
#define __NR_clone		312
#define __NR_uselib		313
#define __NR_mlock		314
#define __NR_munlock		315
#define __NR_mlockall		316
#define __NR_munlockall		317
#define __NR_sysinfo		318
#define __NR__sysctl		319
/* 320 was sys_idle.  */
#define __NR_oldumount		321
#define __NR_swapon		322
#define __NR_times		323
#define __NR_personality	324
#define __NR_setfsuid		325
#define __NR_setfsgid		326
#define __NR_ustat		327
#define __NR_statfs		328
#define __NR_fstatfs		329
#define __NR_sched_setparam		330
#define __NR_sched_getparam		331
#define __NR_sched_setscheduler		332
#define __NR_sched_getscheduler		333
#define __NR_sched_yield		334
#define __NR_sched_get_priority_max	335
#define __NR_sched_get_priority_min	336
#define __NR_sched_rr_get_interval	337
#define __NR_afs_syscall		338
#define __NR_uname			339
#define __NR_nanosleep			340
#define __NR_mremap			341
#define __NR_nfsservctl			342
#define __NR_setresuid			343
#define __NR_getresuid			344
#define __NR_pciconfig_read		345
#define __NR_pciconfig_write		346
#define __NR_query_module		347
#define __NR_prctl			348
#define __NR_pread			349
#define __NR_pwrite			350
#define __NR_rt_sigreturn		351
#define __NR_rt_sigaction		352
#define __NR_rt_sigprocmask		353
#define __NR_rt_sigpending		354
#define __NR_rt_sigtimedwait		355
#define __NR_rt_sigqueueinfo		356
#define __NR_rt_sigsuspend		357
#define __NR_select			358
#define __NR_gettimeofday		359
#define __NR_settimeofday		360
#define __NR_getitimer			361
#define __NR_setitimer			362
#define __NR_utimes			363
#define __NR_getrusage			364
#define __NR_wait4			365
#define __NR_adjtimex			366
#define __NR_getcwd			367
#define __NR_capget			368
#define __NR_capset			369
#define __NR_sendfile			370
#define __NR_setresgid			371
#define __NR_getresgid			372
#define __NR_dipc			373
#define __NR_pivot_root			374
#define __NR_mincore			375
#define __NR_pciconfig_iobase		376
#define __NR_getdents64			377
#define __NR_gettid			378
#define __NR_readahead			379
#define __NR_security			380 /* syscall for security modules */
#define __NR_tkill			381
#define __NR_setxattr			382
#define __NR_lsetxattr			383
#define __NR_fsetxattr			384
#define __NR_getxattr			385
#define __NR_lgetxattr			386
#define __NR_fgetxattr			387
#define __NR_listxattr			388
#define __NR_llistxattr			389
#define __NR_flistxattr			390
#define __NR_removexattr		391
#define __NR_lremovexattr		392
#define __NR_fremovexattr		393


#if defined(__GNUC__)

#define _syscall_return(type)						\
	return (_sc_err ? errno = _sc_ret, _sc_ret = -1L : 0), (type) _sc_ret

#define _syscall_clobbers						\
	"$1", "$2", "$3", "$4", "$5", "$6", "$7", "$8",			\
	"$22", "$23", "$24", "$25", "$27", "$28" 			\

#define _syscall0(type, name)						\
type name(void)								\
{									\
	long _sc_ret, _sc_err;						\
	{								\
		register long _sc_0 __asm__("$0");			\
		register long _sc_19 __asm__("$19");			\
									\
		_sc_0 = __NR_##name;					\
		__asm__("callsys # %0 %1 %2"				\
			: "=r"(_sc_0), "=r"(_sc_19)			\
			: "0"(_sc_0)					\
			: _syscall_clobbers);				\
		_sc_ret = _sc_0, _sc_err = _sc_19;			\
	}								\
	_syscall_return(type);						\
}

#define _syscall1(type,name,type1,arg1)					\
type name(type1 arg1)							\
{									\
	long _sc_ret, _sc_err;						\
	{								\
		register long _sc_0 __asm__("$0");			\
		register long _sc_16 __asm__("$16");			\
		register long _sc_19 __asm__("$19");			\
									\
		_sc_0 = __NR_##name;					\
		_sc_16 = (long) (arg1);					\
		__asm__("callsys # %0 %1 %2 %3"				\
			: "=r"(_sc_0), "=r"(_sc_19)			\
			: "0"(_sc_0), "r"(_sc_16)			\
			: _syscall_clobbers);				\
		_sc_ret = _sc_0, _sc_err = _sc_19;			\
	}								\
	_syscall_return(type);						\
}

#define _syscall2(type,name,type1,arg1,type2,arg2)			\
type name(type1 arg1,type2 arg2)					\
{									\
	long _sc_ret, _sc_err;						\
	{								\
		register long _sc_0 __asm__("$0");			\
		register long _sc_16 __asm__("$16");			\
		register long _sc_17 __asm__("$17");			\
		register long _sc_19 __asm__("$19");			\
									\
		_sc_0 = __NR_##name;					\
		_sc_16 = (long) (arg1);					\
		_sc_17 = (long) (arg2);					\
		__asm__("callsys # %0 %1 %2 %3 %4"			\
			: "=r"(_sc_0), "=r"(_sc_19)			\
			: "0"(_sc_0), "r"(_sc_16), "r"(_sc_17)		\
			: _syscall_clobbers);				\
		_sc_ret = _sc_0, _sc_err = _sc_19;			\
	}								\
	_syscall_return(type);						\
}

#define _syscall3(type,name,type1,arg1,type2,arg2,type3,arg3)		\
type name(type1 arg1,type2 arg2,type3 arg3)				\
{									\
	long _sc_ret, _sc_err;						\
	{								\
		register long _sc_0 __asm__("$0");			\
		register long _sc_16 __asm__("$16");			\
		register long _sc_17 __asm__("$17");			\
		register long _sc_18 __asm__("$18");			\
		register long _sc_19 __asm__("$19");			\
									\
		_sc_0 = __NR_##name;					\
		_sc_16 = (long) (arg1);					\
		_sc_17 = (long) (arg2);					\
		_sc_18 = (long) (arg3);					\
		__asm__("callsys # %0 %1 %2 %3 %4 %5"			\
			: "=r"(_sc_0), "=r"(_sc_19)			\
			: "0"(_sc_0), "r"(_sc_16), "r"(_sc_17),		\
			  "r"(_sc_18)					\
			: _syscall_clobbers);				\
		_sc_ret = _sc_0, _sc_err = _sc_19;			\
	}								\
	_syscall_return(type);						\
}

#define _syscall4(type,name,type1,arg1,type2,arg2,type3,arg3,type4,arg4) \
type name (type1 arg1, type2 arg2, type3 arg3, type4 arg4)		 \
{									 \
	long _sc_ret, _sc_err;						\
	{								\
		register long _sc_0 __asm__("$0");			\
		register long _sc_16 __asm__("$16");			\
		register long _sc_17 __asm__("$17");			\
		register long _sc_18 __asm__("$18");			\
		register long _sc_19 __asm__("$19");			\
									\
		_sc_0 = __NR_##name;					\
		_sc_16 = (long) (arg1);					\
		_sc_17 = (long) (arg2);					\
		_sc_18 = (long) (arg3);					\
		_sc_19 = (long) (arg4);					\
		__asm__("callsys # %0 %1 %2 %3 %4 %5 %6"		\
			: "=r"(_sc_0), "=r"(_sc_19)			\
			: "0"(_sc_0), "r"(_sc_16), "r"(_sc_17),		\
			  "r"(_sc_18), "1"(_sc_19)			\
			: _syscall_clobbers);				\
		_sc_ret = _sc_0, _sc_err = _sc_19;			\
	}								\
	_syscall_return(type);						\
} 

#define _syscall5(type,name,type1,arg1,type2,arg2,type3,arg3,type4,arg4, \
	  type5,arg5)							 \
type name (type1 arg1,type2 arg2,type3 arg3,type4 arg4,type5 arg5)	\
{									\
	long _sc_ret, _sc_err;						\
	{								\
		register long _sc_0 __asm__("$0");			\
		register long _sc_16 __asm__("$16");			\
		register long _sc_17 __asm__("$17");			\
		register long _sc_18 __asm__("$18");			\
		register long _sc_19 __asm__("$19");			\
		register long _sc_20 __asm__("$20");			\
									\
		_sc_0 = __NR_##name;					\
		_sc_16 = (long) (arg1);					\
		_sc_17 = (long) (arg2);					\
		_sc_18 = (long) (arg3);					\
		_sc_19 = (long) (arg4);					\
		_sc_20 = (long) (arg5);					\
		__asm__("callsys # %0 %1 %2 %3 %4 %5 %6 %7"		\
			: "=r"(_sc_0), "=r"(_sc_19)			\
			: "0"(_sc_0), "r"(_sc_16), "r"(_sc_17),		\
			  "r"(_sc_18), "1"(_sc_19), "r"(_sc_20)		\
			: _syscall_clobbers);				\
		_sc_ret = _sc_0, _sc_err = _sc_19;			\
	}								\
	_syscall_return(type);						\
}

#define _syscall6(type,name,type1,arg1,type2,arg2,type3,arg3,type4,arg4, \
	  type5,arg5,type6,arg6)					 \
type name (type1 arg1,type2 arg2,type3 arg3,type4 arg4,type5 arg5, type6 arg6)\
{									\
	long _sc_ret, _sc_err;						\
	{								\
		register long _sc_0 __asm__("$0");			\
		register long _sc_16 __asm__("$16");			\
		register long _sc_17 __asm__("$17");			\
		register long _sc_18 __asm__("$18");			\
		register long _sc_19 __asm__("$19");			\
		register long _sc_20 __asm__("$20");			\
		register long _sc_21 __asm__("$21");			\
									\
		_sc_0 = __NR_##name;					\
		_sc_16 = (long) (arg1);					\
		_sc_17 = (long) (arg2);					\
		_sc_18 = (long) (arg3);					\
		_sc_19 = (long) (arg4);					\
		_sc_20 = (long) (arg5);					\
		_sc_21 = (long) (arg6);					\
		__asm__("callsys # %0 %1 %2 %3 %4 %5 %6 %7 %8"		\
			: "=r"(_sc_0), "=r"(_sc_19)			\
			: "0"(_sc_0), "r"(_sc_16), "r"(_sc_17),		\
			  "r"(_sc_18), "1"(_sc_19), "r"(_sc_20), "r"(_sc_21) \
			: _syscall_clobbers);				\
		_sc_ret = _sc_0, _sc_err = _sc_19;			\
	}								\
	_syscall_return(type);						\
}

#endif /* __LIBRARY__ && __GNUC__ */

#ifdef __KERNEL_SYSCALLS__

#include <linux/string.h>
#include <linux/signal.h>

extern void sys_idle(void);
static inline void idle(void)
{
	sys_idle();
}

extern long sys_open(const char *, int, int);
static inline long open(const char * name, int mode, int flags)
{
	return sys_open(name, mode, flags);
}

extern long sys_dup(int);
static inline long dup(int fd)
{
	return sys_dup(fd);
}

static inline long close(int fd)
{
	return sys_close(fd);
}

extern off_t sys_lseek(unsigned int, off_t, unsigned int);
static inline off_t lseek(unsigned int fd, off_t off, unsigned int whense)
{
	return sys_lseek(fd, off, whense);
}

extern long sys_exit(int);
static inline long _exit(int value)
{
	return sys_exit(value);
}

#define exit(x) _exit(x)

extern long sys_write(int, const char *, int);
static inline long write(int fd, const char * buf, int nr)
{
	return sys_write(fd, buf, nr);
}

extern long sys_read(int, char *, int);
static inline long read(int fd, char * buf, int nr)
{
	return sys_read(fd, buf, nr);
}

extern int __kernel_execve(char *, char **, char **, struct pt_regs *);
static inline long execve(char * file, char ** argvp, char ** envp)
{
	struct pt_regs regs;
	memset(&regs, 0, sizeof(regs));
	return __kernel_execve(file, argvp, envp, &regs);
}

extern long sys_setsid(void);
static inline long setsid(void)
{
	return sys_setsid();
}

extern long sys_sync(void);
static inline long sync(void)
{
	return sys_sync();
}

static inline pid_t waitpid(int pid, int * wait_stat, int flags)
{
	return sys_wait4(pid, wait_stat, flags, NULL);
}

static inline pid_t wait(int * wait_stat)
{
	return waitpid(-1,wait_stat,0);
}

extern long sys_delete_module(const char *name);
static inline long delete_module(const char *name)
{
	return sys_delete_module(name);
}

#endif

#endif /* _ALPHA_UNISTD_H */
