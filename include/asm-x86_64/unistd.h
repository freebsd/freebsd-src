#ifndef _ASM_X86_64_UNISTD_H_
#define _ASM_X86_64_UNISTD_H_

#ifndef __SYSCALL
#define __SYSCALL(a,b) 
#endif

/*
 * This file contains the system call numbers.
 * 
 * Note: holes are not allowed.
 */

/* at least 8 syscall per cacheline */
#define __NR_read                                0
__SYSCALL(__NR_read, sys_read)
#define __NR_write                               1
__SYSCALL(__NR_write, sys_write)
#define __NR_open                                2
__SYSCALL(__NR_open, sys_open)
#define __NR_close                               3
__SYSCALL(__NR_close, sys_close)
#define __NR_stat                                4
__SYSCALL(__NR_stat, sys_newstat)
#define __NR_fstat                               5
__SYSCALL(__NR_fstat, sys_newfstat)
#define __NR_lstat                               6
__SYSCALL(__NR_lstat, sys_newlstat)
#define __NR_poll                                7
__SYSCALL(__NR_poll, sys_poll)

#define __NR_lseek                               8
__SYSCALL(__NR_lseek, sys_lseek)
#define __NR_mmap                                9
__SYSCALL(__NR_mmap, sys_mmap)
#define __NR_mprotect                           10
__SYSCALL(__NR_mprotect, sys_mprotect)
#define __NR_munmap                             11
__SYSCALL(__NR_munmap, sys_munmap)
#define __NR_brk                                12
__SYSCALL(__NR_brk, sys_brk)
#define __NR_rt_sigaction                       13
__SYSCALL(__NR_rt_sigaction, sys_rt_sigaction)
#define __NR_rt_sigprocmask                     14
__SYSCALL(__NR_rt_sigprocmask, sys_rt_sigprocmask)
#define __NR_rt_sigreturn                       15
__SYSCALL(__NR_rt_sigreturn, stub_rt_sigreturn)

#define __NR_ioctl                              16
__SYSCALL(__NR_ioctl, sys_ioctl)
#define __NR_pread                              17
__SYSCALL(__NR_pread, sys_pread)
#define __NR_pwrite                             18
__SYSCALL(__NR_pwrite, sys_pwrite)
#define __NR_readv                              19
__SYSCALL(__NR_readv, sys_readv)
#define __NR_writev                             20
__SYSCALL(__NR_writev, sys_writev)
#define __NR_access                             21
__SYSCALL(__NR_access, sys_access)
#define __NR_pipe                               22
__SYSCALL(__NR_pipe, sys_pipe)
#define __NR_select                             23
__SYSCALL(__NR_select, sys_select)

#define __NR_sched_yield                        24
__SYSCALL(__NR_sched_yield, sys_sched_yield)
#define __NR_mremap                             25
__SYSCALL(__NR_mremap, sys_mremap)
#define __NR_msync                              26
__SYSCALL(__NR_msync, sys_msync)
#define __NR_mincore                            27
__SYSCALL(__NR_mincore, sys_mincore)
#define __NR_madvise                            28
__SYSCALL(__NR_madvise, sys_madvise)
#define __NR_shmget                             29
__SYSCALL(__NR_shmget, sys_shmget)
#define __NR_shmat                              30
__SYSCALL(__NR_shmat, wrap_sys_shmat)
#define __NR_shmctl                             31
__SYSCALL(__NR_shmctl, sys_shmctl)

#define __NR_dup                                32
__SYSCALL(__NR_dup, sys_dup)
#define __NR_dup2                               33
__SYSCALL(__NR_dup2, sys_dup2)
#define __NR_pause                              34
__SYSCALL(__NR_pause, sys_pause)
#define __NR_nanosleep                          35
__SYSCALL(__NR_nanosleep, sys_nanosleep)
#define __NR_getitimer                          36
__SYSCALL(__NR_getitimer, sys_getitimer)
#define __NR_alarm                              37
__SYSCALL(__NR_alarm, sys_alarm)
#define __NR_setitimer                          38
__SYSCALL(__NR_setitimer, sys_setitimer)
#define __NR_getpid                             39
__SYSCALL(__NR_getpid, sys_getpid)

#define __NR_sendfile                           40
__SYSCALL(__NR_sendfile, sys_sendfile)
#define __NR_socket                             41
__SYSCALL(__NR_socket, sys_socket)
#define __NR_connect                            42
__SYSCALL(__NR_connect, sys_connect)
#define __NR_accept                             43
__SYSCALL(__NR_accept, sys_accept)
#define __NR_sendto                             44
__SYSCALL(__NR_sendto, sys_sendto)
#define __NR_recvfrom                           45
__SYSCALL(__NR_recvfrom, sys_recvfrom)
#define __NR_sendmsg                            46
__SYSCALL(__NR_sendmsg, sys_sendmsg)
#define __NR_recvmsg                            47
__SYSCALL(__NR_recvmsg, sys_recvmsg)

#define __NR_shutdown                           48
__SYSCALL(__NR_shutdown, sys_shutdown)
#define __NR_bind                               49
__SYSCALL(__NR_bind, sys_bind)
#define __NR_listen                             50
__SYSCALL(__NR_listen, sys_listen)
#define __NR_getsockname                        51
__SYSCALL(__NR_getsockname, sys_getsockname)
#define __NR_getpeername                        52
__SYSCALL(__NR_getpeername, sys_getpeername)
#define __NR_socketpair                         53
__SYSCALL(__NR_socketpair, sys_socketpair)
#define __NR_setsockopt                         54
__SYSCALL(__NR_setsockopt, sys_setsockopt)
#define __NR_getsockopt                         55
__SYSCALL(__NR_getsockopt, sys_getsockopt)

#define __NR_clone                              56
__SYSCALL(__NR_clone, stub_clone)
#define __NR_fork                               57
__SYSCALL(__NR_fork, stub_fork) 
#define __NR_vfork                              58
__SYSCALL(__NR_vfork, stub_vfork)
#define __NR_execve                             59
__SYSCALL(__NR_execve, stub_execve)
#define __NR_exit                               60
__SYSCALL(__NR_exit, sys_exit)
#define __NR_wait4                              61
__SYSCALL(__NR_wait4, sys_wait4)
#define __NR_kill                               62
__SYSCALL(__NR_kill, sys_kill)
#define __NR_uname                              63
__SYSCALL(__NR_uname, sys_uname)

#define __NR_semget                             64
__SYSCALL(__NR_semget, sys_semget)
#define __NR_semop                              65
__SYSCALL(__NR_semop, sys_semop)
#define __NR_semctl                             66
__SYSCALL(__NR_semctl, sys_semctl)
#define __NR_shmdt                              67
__SYSCALL(__NR_shmdt, sys_shmdt)
#define __NR_msgget                             68
__SYSCALL(__NR_msgget, sys_msgget)
#define __NR_msgsnd                             69
__SYSCALL(__NR_msgsnd, sys_msgsnd)
#define __NR_msgrcv                             70
__SYSCALL(__NR_msgrcv, sys_msgrcv)
#define __NR_msgctl                             71
__SYSCALL(__NR_msgctl, sys_msgctl)

#define __NR_fcntl                              72
__SYSCALL(__NR_fcntl, sys_fcntl)
#define __NR_flock                              73
__SYSCALL(__NR_flock, sys_flock)
#define __NR_fsync                              74
__SYSCALL(__NR_fsync, sys_fsync)
#define __NR_fdatasync                          75
__SYSCALL(__NR_fdatasync, sys_fdatasync)
#define __NR_truncate                           76
__SYSCALL(__NR_truncate, sys_truncate)
#define __NR_ftruncate                          77
__SYSCALL(__NR_ftruncate, sys_ftruncate)
#define __NR_getdents                           78
__SYSCALL(__NR_getdents, sys_getdents)
#define __NR_getcwd                             79
__SYSCALL(__NR_getcwd, sys_getcwd)

#define __NR_chdir                              80
__SYSCALL(__NR_chdir, sys_chdir)
#define __NR_fchdir                             81
__SYSCALL(__NR_fchdir, sys_fchdir)
#define __NR_rename                             82
__SYSCALL(__NR_rename, sys_rename)
#define __NR_mkdir                              83
__SYSCALL(__NR_mkdir, sys_mkdir)
#define __NR_rmdir                              84
__SYSCALL(__NR_rmdir, sys_rmdir)
#define __NR_creat                              85
__SYSCALL(__NR_creat, sys_creat)
#define __NR_link                               86
__SYSCALL(__NR_link, sys_link)
#define __NR_unlink                             87
__SYSCALL(__NR_unlink, sys_unlink)

#define __NR_symlink                            88
__SYSCALL(__NR_symlink, sys_symlink)
#define __NR_readlink                           89
__SYSCALL(__NR_readlink, sys_readlink)
#define __NR_chmod                              90
__SYSCALL(__NR_chmod, sys_chmod)
#define __NR_fchmod                             91
__SYSCALL(__NR_fchmod, sys_fchmod)
#define __NR_chown                              92
__SYSCALL(__NR_chown, sys_chown)
#define __NR_fchown                             93
__SYSCALL(__NR_fchown, sys_fchown)
#define __NR_lchown                             94
__SYSCALL(__NR_lchown, sys_lchown)
#define __NR_umask                              95
__SYSCALL(__NR_umask, sys_umask)

#define __NR_gettimeofday                       96
__SYSCALL(__NR_gettimeofday, sys_gettimeofday)
#define __NR_getrlimit                          97
__SYSCALL(__NR_getrlimit, sys_getrlimit)
#define __NR_getrusage                          98
__SYSCALL(__NR_getrusage, sys_getrusage)
#define __NR_sysinfo                            99
__SYSCALL(__NR_sysinfo, sys_sysinfo)
#define __NR_times                             100
__SYSCALL(__NR_times, sys_times)
#define __NR_ptrace                            101
__SYSCALL(__NR_ptrace, sys_ptrace)
#define __NR_getuid                            102
__SYSCALL(__NR_getuid, sys_getuid)
#define __NR_syslog                            103
__SYSCALL(__NR_syslog, sys_syslog)

/* at the very end the stuff that never runs during the benchmarks */
#define __NR_getgid                            104
__SYSCALL(__NR_getgid, sys_getgid)
#define __NR_setuid                            105
__SYSCALL(__NR_setuid, sys_setuid)
#define __NR_setgid                            106
__SYSCALL(__NR_setgid, sys_setgid)
#define __NR_geteuid                           107
__SYSCALL(__NR_geteuid, sys_geteuid)
#define __NR_getegid                           108
__SYSCALL(__NR_getegid, sys_getegid)
#define __NR_setpgid                           109
__SYSCALL(__NR_setpgid, sys_setpgid)
#define __NR_getppid                           110
__SYSCALL(__NR_getppid, sys_getppid)
#define __NR_getpgrp                           111
__SYSCALL(__NR_getpgrp, sys_getpgrp)

#define __NR_setsid                            112
__SYSCALL(__NR_setsid, sys_setsid)
#define __NR_setreuid                          113
__SYSCALL(__NR_setreuid, sys_setreuid)
#define __NR_setregid                          114
__SYSCALL(__NR_setregid, sys_setregid)
#define __NR_getgroups                         115
__SYSCALL(__NR_getgroups, sys_getgroups)
#define __NR_setgroups                         116
__SYSCALL(__NR_setgroups, sys_setgroups)
#define __NR_setresuid                         117
__SYSCALL(__NR_setresuid, sys_setresuid)
#define __NR_getresuid                         118
__SYSCALL(__NR_getresuid, sys_getresuid)
#define __NR_setresgid                         119
__SYSCALL(__NR_setresgid, sys_setresgid)

#define __NR_getresgid                         120
__SYSCALL(__NR_getresgid, sys_getresgid)
#define __NR_getpgid                           121
__SYSCALL(__NR_getpgid, sys_getpgid)
#define __NR_setfsuid                          122
__SYSCALL(__NR_setfsuid, sys_setfsuid)
#define __NR_setfsgid                          123
__SYSCALL(__NR_setfsgid, sys_setfsgid)
#define __NR_getsid                            124
__SYSCALL(__NR_getsid, sys_getsid)
#define __NR_capget                            125
__SYSCALL(__NR_capget, sys_capget)
#define __NR_capset                            126
__SYSCALL(__NR_capset, sys_capset)

#define __NR_rt_sigpending                     127
__SYSCALL(__NR_rt_sigpending, sys_rt_sigpending)
#define __NR_rt_sigtimedwait                   128
__SYSCALL(__NR_rt_sigtimedwait, sys_rt_sigtimedwait)
#define __NR_rt_sigqueueinfo                   129
__SYSCALL(__NR_rt_sigqueueinfo, sys_rt_sigqueueinfo)
#define __NR_rt_sigsuspend                     130
__SYSCALL(__NR_rt_sigsuspend, stub_rt_sigsuspend)
#define __NR_sigaltstack                       131
__SYSCALL(__NR_sigaltstack, stub_sigaltstack)
#define __NR_utime                             132
__SYSCALL(__NR_utime, sys_utime)
#define __NR_mknod                             133
__SYSCALL(__NR_mknod, sys_mknod)

#define __NR_uselib                            134
__SYSCALL(__NR_uselib, sys_uselib)
#define __NR_personality                       135
__SYSCALL(__NR_personality, sys_personality)

#define __NR_ustat                             136
__SYSCALL(__NR_ustat, sys_ustat)
#define __NR_statfs                            137
__SYSCALL(__NR_statfs, sys_statfs)
#define __NR_fstatfs                           138
__SYSCALL(__NR_fstatfs, sys_fstatfs)
#define __NR_sysfs                             139
__SYSCALL(__NR_sysfs, sys_sysfs)

#define __NR_getpriority                       140
__SYSCALL(__NR_getpriority, sys_getpriority)
#define __NR_setpriority                       141
__SYSCALL(__NR_setpriority, sys_setpriority)
#define __NR_sched_setparam                    142
__SYSCALL(__NR_sched_setparam, sys_sched_setparam)
#define __NR_sched_getparam                    143
__SYSCALL(__NR_sched_getparam, sys_sched_getparam)
#define __NR_sched_setscheduler                144
__SYSCALL(__NR_sched_setscheduler, sys_sched_setscheduler)
#define __NR_sched_getscheduler                145
__SYSCALL(__NR_sched_getscheduler, sys_sched_getscheduler)
#define __NR_sched_get_priority_max            146
__SYSCALL(__NR_sched_get_priority_max, sys_sched_get_priority_max)
#define __NR_sched_get_priority_min            147
__SYSCALL(__NR_sched_get_priority_min, sys_sched_get_priority_min)
#define __NR_sched_rr_get_interval             148
__SYSCALL(__NR_sched_rr_get_interval, sys_sched_rr_get_interval)

#define __NR_mlock                             149
__SYSCALL(__NR_mlock, sys_mlock)
#define __NR_munlock                           150
__SYSCALL(__NR_munlock, sys_munlock)
#define __NR_mlockall                          151
__SYSCALL(__NR_mlockall, sys_mlockall)
#define __NR_munlockall                        152
__SYSCALL(__NR_munlockall, sys_munlockall)

#define __NR_vhangup                           153
__SYSCALL(__NR_vhangup, sys_vhangup)

#define __NR_modify_ldt                        154
__SYSCALL(__NR_modify_ldt, sys_modify_ldt)

#define __NR_pivot_root                        155
__SYSCALL(__NR_pivot_root, sys_pivot_root)

#define __NR__sysctl                           156
__SYSCALL(__NR__sysctl, sys_sysctl)

#define __NR_prctl                             157
__SYSCALL(__NR_prctl, sys_prctl)
#define __NR_arch_prctl                        158
__SYSCALL(__NR_arch_prctl,	sys_arch_prctl) 

#define __NR_adjtimex                          159
__SYSCALL(__NR_adjtimex, sys_adjtimex)

#define __NR_setrlimit                         160
__SYSCALL(__NR_setrlimit, sys_setrlimit)

#define __NR_chroot                            161
__SYSCALL(__NR_chroot, sys_chroot)

#define __NR_sync                              162
__SYSCALL(__NR_sync, sys_sync)

#define __NR_acct                              163
__SYSCALL(__NR_acct, sys_acct)

#define __NR_settimeofday                      164
__SYSCALL(__NR_settimeofday, sys_settimeofday)

#define __NR_mount                             165
__SYSCALL(__NR_mount, sys_mount)
#define __NR_umount2                           166
__SYSCALL(__NR_umount2, sys_umount)

#define __NR_swapon                            167
__SYSCALL(__NR_swapon, sys_swapon)
#define __NR_swapoff                           168
__SYSCALL(__NR_swapoff, sys_swapoff)

#define __NR_reboot                            169
__SYSCALL(__NR_reboot, sys_reboot)

#define __NR_sethostname                       170
__SYSCALL(__NR_sethostname, sys_sethostname)
#define __NR_setdomainname                     171
__SYSCALL(__NR_setdomainname, sys_setdomainname)

#define __NR_iopl                              172
__SYSCALL(__NR_iopl, stub_iopl)
#define __NR_ioperm                            173
__SYSCALL(__NR_ioperm, sys_ioperm)

#define __NR_create_module                     174
__SYSCALL(__NR_create_module, sys_create_module)
#define __NR_init_module                       175
__SYSCALL(__NR_init_module, sys_init_module)
#define __NR_delete_module                     176
__SYSCALL(__NR_delete_module, sys_delete_module)
#define __NR_get_kernel_syms                   177
__SYSCALL(__NR_get_kernel_syms, sys_get_kernel_syms)
#define __NR_query_module                      178
__SYSCALL(__NR_query_module, sys_query_module)

#define __NR_quotactl                          179
__SYSCALL(__NR_quotactl, sys_quotactl)

#define __NR_nfsservctl                        180
__SYSCALL(__NR_nfsservctl, sys_nfsservctl)

#define __NR_getpmsg                           181
__SYSCALL(__NR_getpmsg, sys_ni_syscall)
#define __NR_putpmsg                           182
__SYSCALL(__NR_putpmsg, sys_ni_syscall)

#define __NR_afs_syscall                       183
__SYSCALL(__NR_afs_syscall, sys_ni_syscall)

#define __NR_tuxcall      		184 /* reserved for tux */
__SYSCALL(__NR_tuxcall, sys_ni_syscall)

#define __NR_security			185 /* reserved for security */
__SYSCALL(__NR_security, sys_ni_syscall)

#define __NR_gettid		186
__SYSCALL(__NR_gettid, sys_gettid)

#define __NR_readahead		187
__SYSCALL(__NR_readahead, sys_readahead)

#define __NR_setxattr		188
__SYSCALL(__NR_setxattr, sys_ni_syscall)
#define __NR_lsetxattr		189
__SYSCALL(__NR_lsetxattr, sys_ni_syscall)
#define __NR_fsetxattr		190
__SYSCALL(__NR_fsetxattr, sys_ni_syscall)
#define __NR_getxattr		191
__SYSCALL(__NR_getxattr, sys_ni_syscall)
#define __NR_lgetxattr		192
__SYSCALL(__NR_lgetxattr, sys_ni_syscall)
#define __NR_fgetxattr		193
__SYSCALL(__NR_fgetxattr, sys_ni_syscall) 
#define __NR_listxattr		194
__SYSCALL(__NR_listxattr, sys_ni_syscall) 
#define __NR_llistxattr		195
__SYSCALL(__NR_llistxattr, sys_ni_syscall) 
#define __NR_flistxattr		196
__SYSCALL(__NR_flistxattr, sys_ni_syscall) 
#define __NR_removexattr	197
__SYSCALL(__NR_removexattr, sys_ni_syscall) 
#define __NR_lremovexattr	198
__SYSCALL(__NR_lremovexattr, sys_ni_syscall) 
#define __NR_fremovexattr	199
__SYSCALL(__NR_fremovexattr, sys_ni_syscall) 
#define __NR_tkill	200	/* 2.5 only */
__SYSCALL(__NR_tkill, sys_ni_syscall) 
#define __NR_time		201
__SYSCALL(__NR_time, sys_time64)
#define __NR_futex		202 /* 2.5 only */
__SYSCALL(__NR_futex, sys_ni_syscall)
#define __NR_sched_setaffinity    203
__SYSCALL(__NR_sched_setaffinity, sys_ni_syscall)
#define __NR_sched_getaffinity     204
__SYSCALL(__NR_sched_getaffinity, sys_ni_syscall)
#define __NR_set_thread_area	205
__SYSCALL(__NR_set_thread_area, sys_ni_syscall)
#define __NR_io_setup	206
__SYSCALL(__NR_io_setup, sys_ni_syscall)
#define __NR_io_destroy	207
__SYSCALL(__NR_io_destroy, sys_ni_syscall)
#define __NR_io_getevents	208
__SYSCALL(__NR_io_getevents, sys_ni_syscall)
#define __NR_io_submit	209
__SYSCALL(__NR_io_submit, sys_ni_syscall)
#define __NR_io_cancel	210
__SYSCALL(__NR_io_cancel, sys_ni_syscall)
#define __NR_get_thread_area	211
__SYSCALL(__NR_get_thread_area, sys_ni_syscall)
#define __NR_lookup_dcookie     212
__SYSCALL(__NR_lookup_dcookie, sys_ni_syscall)
#define __NR_epoll_create       213
__SYSCALL(__NR_epoll_create, sys_ni_syscall)
#define __NR_epoll_ctl  214
__SYSCALL(__NR_epoll_ctl, sys_ni_syscall)
#define __NR_epoll_wait 215
__SYSCALL(__NR_epoll_wait, sys_ni_syscall)
#define __NR_remap_file_pages   216
__SYSCALL(__NR_remap_file_pages, sys_ni_syscall)
#define __NR_getdents64 217
__SYSCALL(__NR_getdents64, sys_getdents64)
#define __NR_restart_syscall	219
__SYSCALL(__NR_restart_syscall, sys_ni_syscall)
#define __NR_semtimedop		220
__SYSCALL(__NR_semtimedop, sys_semtimedop)

#define __NR_syscall_max __NR_semtimedop

#ifndef __NO_STUBS

#define __syscall_clobber "r11","rcx","memory" 

#define __syscall_return(type, res) \
do { \
	if ((unsigned long)(res) >= (unsigned long)(-127)) { \
		errno = -(res); \
		res = -1; \
	} \
	return (type) (res); \
} while (0)

#ifndef __KERNEL_SYSCALLS__

#define __syscall "syscall"


#define _syscall0(type,name) \
type name(void) \
{ \
long __res; \
__asm__ volatile (__syscall \
	: "=a" (__res) \
	: "0" (__NR_##name) : __syscall_clobber ); \
__syscall_return(type,__res); \
}

#define _syscall1(type,name,type1,arg1) \
type name(type1 arg1) \
{ \
long __res; \
__asm__ volatile (__syscall \
	: "=a" (__res) \
	: "0" (__NR_##name),"D" ((long)(arg1)) : __syscall_clobber ); \
__syscall_return(type,__res); \
}

#define _syscall2(type,name,type1,arg1,type2,arg2) \
type name(type1 arg1,type2 arg2) \
{ \
long __res; \
__asm__ volatile (__syscall \
	: "=a" (__res) \
	: "0" (__NR_##name),"D" ((long)(arg1)),"S" ((long)(arg2)) : __syscall_clobber ); \
__syscall_return(type,__res); \
}

#define _syscall3(type,name,type1,arg1,type2,arg2,type3,arg3) \
type name(type1 arg1,type2 arg2,type3 arg3) \
{ \
long __res; \
__asm__ volatile (__syscall \
	: "=a" (__res) \
	: "0" (__NR_##name),"D" ((long)(arg1)),"S" ((long)(arg2)), \
		  "d" ((long)(arg3)) : __syscall_clobber); \
__syscall_return(type,__res); \
}

#define _syscall4(type,name,type1,arg1,type2,arg2,type3,arg3,type4,arg4) \
type name (type1 arg1, type2 arg2, type3 arg3, type4 arg4) \
{ \
long __res; \
__asm__ volatile ("movq %[a3],%%r10 ;" __syscall \
	: "=a" (__res) \
	: "0" (__NR_##name),"D" ((long)(arg1)),"S" ((long)(arg2)), \
	  "d" ((long)(arg3)), [a3] "g" ((long)(arg4)) : __syscall_clobber,"r10" ); \
__syscall_return(type,__res); \
} 

#define _syscall5(type,name,type1,arg1,type2,arg2,type3,arg3,type4,arg4, \
	  type5,arg5) \
type name (type1 arg1,type2 arg2,type3 arg3,type4 arg4,type5 arg5) \
{ \
long __res; \
__asm__ volatile ("movq %[a3],%%r10 ; movq %[a4],%%r8 ; " __syscall \
	: "=a" (__res) \
	: "0" (__NR_##name),"D" ((long)(arg1)),"S" ((long)(arg2)), \
	  "d" ((long)(arg3)), [a3] "g" ((long)(arg4)), [a4] "g" ((long)(arg5)) : \
	__syscall_clobber,"r8","r10" ); \
__syscall_return(type,__res); \
}

#define _syscall6(type,name,type1,arg1,type2,arg2,type3,arg3,type4,arg4, \
	  type5,arg5,type6,arg6) \
type name (type1 arg1,type2 arg2,type3 arg3,type4 arg4,type5 arg5,type6 arg6) \
{ \
long __res; \
__asm__ volatile ("movq %[a3],%%r10 ; movq %[a4],%%r8 ; movq %[a5],%%r9 ; " __syscall \
	: "=a" (__res) \
	: "0" (__NR_##name),"D" ((long)(arg1)),"S" ((long)(arg2)), \
	  "d" ((long)(arg3)), [a3] "g" ((long)(arg4)), [a4] "g" ((long)(arg5)), \
	  [a5] "g" ((long)(arg6)) : \
	__syscall_clobber,"r8","r10","r9" ); \
__syscall_return(type,__res); \
}

#else /* __KERNEL_SYSCALLS__ */

/*
 * we need this inline - forking from kernel space will result
 * in NO COPY ON WRITE (!!!), until an execve is executed. This
 * is no problem, but for the stack. This is handled by not letting
 * main() use the stack at all after fork(). Thus, no function
 * calls - which means inline code for fork too, as otherwise we
 * would use the stack upon exit from 'fork()'.
 *
 * Actually only pause and fork are needed inline, so that there
 * won't be any messing with the stack from main(), but we define
 * some others too.
 */
#define __NR__exit __NR_exit

extern long sys_pause(void);
static inline long pause(void)
{
	return sys_pause();
}

extern long sys_sync(void);
static inline long sync(void)
{
	return sys_sync();
}

extern pid_t sys_setsid(void);
static inline pid_t setsid(void)
{
	return sys_setsid();
}

extern ssize_t sys_write(unsigned int, char *, size_t);
static inline ssize_t write(unsigned int fd, char * buf, size_t count)
{
	return sys_write(fd, buf, count);
}

extern ssize_t sys_read(unsigned int, char *, size_t);
static inline ssize_t read(unsigned int fd, char * buf, size_t count)
{
	return sys_read(fd, buf, count);
}

extern off_t sys_lseek(unsigned int, off_t, unsigned int);
static inline off_t lseek(unsigned int fd, off_t offset, unsigned int origin)
{
	return sys_lseek(fd, offset, origin);
}

extern long sys_dup(unsigned int);
static inline long dup(unsigned int fd)
{
	return sys_dup(fd);
}

/* implemented in asm in arch/x86_64/kernel/entry.S */
extern long execve(char *, char **, char **);

extern long sys_open(const char *, int, int);
static inline long open(const char * filename, int flags, int mode)
{
	return sys_open(filename, flags, mode);
}

extern long sys_close(unsigned int);
static inline long close(unsigned int fd)
{
	return sys_close(fd);
}

extern long sys_exit(int) __attribute__((noreturn));
extern inline long exit(int error_code)
{
	sys_exit(error_code);
}

extern long sys_delete_module(const char *);
static inline long delete_module(const char *name_user)
{
	return sys_delete_module(name_user);
}

extern long sys_wait4(pid_t, unsigned int *, int, struct rusage *);
static inline pid_t waitpid(int pid, int * wait_stat, int flags)
{
	return sys_wait4(pid, wait_stat, flags, NULL);
}

static inline pid_t wait(int * wait_stat)
{
	return waitpid(-1,wait_stat,0);
}

#endif /* __KERNEL_SYSCALLS__ */

#endif /* __NO_STUBS */

#endif
