#ifndef _ASM_IA64_UNISTD_H
#define _ASM_IA64_UNISTD_H

/*
 * IA-64 Linux syscall numbers and inline-functions.
 *
 * Copyright (C) 1998-2001 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */

#include <asm/break.h>

#define __BREAK_SYSCALL			__IA64_BREAK_SYSCALL

#define __NR_ni_syscall			1024
#define __NR_exit			1025
#define __NR_read			1026
#define __NR_write			1027
#define __NR_open			1028
#define __NR_close			1029
#define __NR_creat			1030
#define __NR_link			1031
#define __NR_unlink			1032
#define __NR_execve			1033
#define __NR_chdir			1034
#define __NR_fchdir			1035
#define __NR_utimes			1036
#define __NR_mknod			1037
#define __NR_chmod			1038
#define __NR_chown			1039
#define __NR_lseek			1040
#define __NR_getpid			1041
#define __NR_getppid			1042
#define __NR_mount			1043
#define __NR_umount			1044
#define __NR_setuid			1045
#define __NR_getuid			1046
#define __NR_geteuid			1047
#define __NR_ptrace			1048
#define __NR_access			1049
#define __NR_sync			1050
#define __NR_fsync			1051
#define __NR_fdatasync			1052
#define __NR_kill			1053
#define __NR_rename			1054
#define __NR_mkdir			1055
#define __NR_rmdir			1056
#define __NR_dup			1057
#define __NR_pipe			1058
#define __NR_times			1059
#define __NR_brk			1060
#define __NR_setgid			1061
#define __NR_getgid			1062
#define __NR_getegid			1063
#define __NR_acct			1064
#define __NR_ioctl			1065
#define __NR_fcntl			1066
#define __NR_umask			1067
#define __NR_chroot			1068
#define __NR_ustat			1069
#define __NR_dup2			1070
#define __NR_setreuid			1071
#define __NR_setregid			1072
#define __NR_getresuid			1073
#define __NR_setresuid			1074
#define __NR_getresgid			1075
#define __NR_setresgid			1076
#define __NR_getgroups			1077
#define __NR_setgroups			1078
#define __NR_getpgid			1079
#define __NR_setpgid			1080
#define __NR_setsid			1081
#define __NR_getsid			1082
#define __NR_sethostname		1083
#define __NR_setrlimit			1084
#define __NR_getrlimit			1085
#define __NR_getrusage			1086
#define __NR_gettimeofday		1087
#define __NR_settimeofday		1088
#define __NR_select			1089
#define __NR_poll			1090
#define __NR_symlink			1091
#define __NR_readlink			1092
#define __NR_uselib			1093
#define __NR_swapon			1094
#define __NR_swapoff			1095
#define __NR_reboot			1096
#define __NR_truncate			1097
#define __NR_ftruncate			1098
#define __NR_fchmod			1099
#define __NR_fchown			1100
#define __NR_getpriority		1101
#define __NR_setpriority		1102
#define __NR_statfs			1103
#define __NR_fstatfs			1104
#define __NR_gettid			1105
#define __NR_semget			1106
#define __NR_semop			1107
#define __NR_semctl			1108
#define __NR_msgget			1109
#define __NR_msgsnd			1110
#define __NR_msgrcv			1111
#define __NR_msgctl			1112
#define __NR_shmget			1113
#define __NR_shmat			1114
#define __NR_shmdt			1115
#define __NR_shmctl			1116
/* also known as klogctl() in GNU libc: */
#define __NR_syslog			1117
#define __NR_setitimer			1118
#define __NR_getitimer			1119
#define __NR_old_stat			1120
#define __NR_old_lstat			1121
#define __NR_old_fstat			1122
#define __NR_vhangup			1123
#define __NR_lchown			1124
#define __NR_vm86			1125
#define __NR_wait4			1126
#define __NR_sysinfo			1127
#define __NR_clone			1128
#define __NR_setdomainname		1129
#define __NR_uname			1130
#define __NR_adjtimex			1131
#define __NR_create_module		1132
#define __NR_init_module		1133
#define __NR_delete_module		1134
#define __NR_get_kernel_syms		1135
#define __NR_query_module		1136
#define __NR_quotactl			1137
#define __NR_bdflush			1138
#define __NR_sysfs			1139
#define __NR_personality		1140
#define __NR_afs_syscall		1141
#define __NR_setfsuid			1142
#define __NR_setfsgid			1143
#define __NR_getdents			1144
#define __NR_flock			1145
#define __NR_readv			1146
#define __NR_writev			1147
#define __NR_pread			1148
#define __NR_pwrite			1149
#define __NR__sysctl			1150
#define __NR_mmap			1151
#define __NR_munmap			1152
#define __NR_mlock			1153
#define __NR_mlockall			1154
#define __NR_mprotect			1155
#define __NR_mremap			1156
#define __NR_msync			1157
#define __NR_munlock			1158
#define __NR_munlockall			1159
#define __NR_sched_getparam		1160
#define __NR_sched_setparam		1161
#define __NR_sched_getscheduler		1162
#define __NR_sched_setscheduler		1163
#define __NR_sched_yield		1164
#define __NR_sched_get_priority_max	1165
#define __NR_sched_get_priority_min	1166
#define __NR_sched_rr_get_interval	1167
#define __NR_nanosleep			1168
#define __NR_nfsservctl			1169
#define __NR_prctl			1170
/* 1171 is reserved for backwards compatibility with old __NR_getpagesize */
#define __NR_mmap2			1172
#define __NR_pciconfig_read		1173
#define __NR_pciconfig_write		1174
#define __NR_perfmonctl			1175
#define __NR_sigaltstack		1176
#define __NR_rt_sigaction		1177
#define __NR_rt_sigpending		1178
#define __NR_rt_sigprocmask		1179
#define __NR_rt_sigqueueinfo		1180
#define __NR_rt_sigreturn		1181
#define __NR_rt_sigsuspend		1182
#define __NR_rt_sigtimedwait		1183
#define __NR_getcwd			1184
#define __NR_capget			1185
#define __NR_capset			1186
#define __NR_sendfile			1187
#define __NR_getpmsg			1188
#define __NR_putpmsg			1189
#define __NR_socket			1190
#define __NR_bind			1191
#define __NR_connect			1192
#define __NR_listen			1193
#define __NR_accept			1194
#define __NR_getsockname		1195
#define __NR_getpeername		1196
#define __NR_socketpair			1197
#define __NR_send			1198
#define __NR_sendto			1199
#define __NR_recv			1200
#define __NR_recvfrom			1201
#define __NR_shutdown			1202
#define __NR_setsockopt			1203
#define __NR_getsockopt			1204
#define __NR_sendmsg			1205
#define __NR_recvmsg			1206
#define __NR_pivot_root			1207
#define __NR_mincore			1208
#define __NR_madvise			1209
#define __NR_stat			1210
#define __NR_lstat			1211
#define __NR_fstat			1212
#define __NR_clone2			1213
#define __NR_getdents64			1214
#define __NR_getunwind			1215
#define __NR_readahead			1216
#define __NR_setxattr			1217
#define __NR_lsetxattr			1218
#define __NR_fsetxattr			1219
#define __NR_getxattr			1220
#define __NR_lgetxattr			1221
#define __NR_fgetxattr			1222
#define __NR_listxattr			1223
#define __NR_llistxattr			1224
#define __NR_flistxattr			1225
#define __NR_removexattr		1226
#define __NR_lremovexattr		1227
#define __NR_fremovexattr		1228
#define __NR_tkill			1229
/* 1230-1232: reserved for futex and sched_[sg]etaffinity */
#define __NR_security			1233
/* 1234-1235: reserved for {alloc,free}_hugepages */
/* 1238-1242: reserved for io_{setup,destroy,getevents,submit,cancel} */
#define __NR_semtimedop			1247

#if !defined(__ASSEMBLY__) && !defined(ASSEMBLER)

extern long __ia64_syscall (long a0, long a1, long a2, long a3, long a4, long nr);

#define _syscall0(type,name)						\
type									\
name (void)								\
{									\
	register long dummy1 __asm__ ("out0");				\
	register long dummy2 __asm__ ("out1");				\
	register long dummy3 __asm__ ("out2");				\
	register long dummy4 __asm__ ("out3");				\
	register long dummy5 __asm__ ("out4");				\
									\
	return __ia64_syscall(dummy1, dummy2, dummy3, dummy4, dummy5,	\
			      __NR_##name);				\
}

#define _syscall1(type,name,type1,arg1)					\
type									\
name (type1 arg1)							\
{									\
	register long dummy2 __asm__ ("out1");				\
	register long dummy3 __asm__ ("out2");				\
	register long dummy4 __asm__ ("out3");				\
	register long dummy5 __asm__ ("out4");				\
									\
	return __ia64_syscall((long) arg1, dummy2, dummy3, dummy4,	\
			      dummy5, __NR_##name);			\
}

#define _syscall2(type,name,type1,arg1,type2,arg2)			\
type									\
name (type1 arg1, type2 arg2)						\
{									\
	register long dummy3 __asm__ ("out2");				\
	register long dummy4 __asm__ ("out3");				\
	register long dummy5 __asm__ ("out4");				\
									\
	return __ia64_syscall((long) arg1, (long) arg2, dummy3, dummy4,	\
			      dummy5, __NR_##name);			\
}

#define _syscall3(type,name,type1,arg1,type2,arg2,type3,arg3)		\
type									\
name (type1 arg1, type2 arg2, type3 arg3)				\
{									\
	register long dummy4 __asm__ ("out3");				\
	register long dummy5 __asm__ ("out4");				\
									\
	return __ia64_syscall((long) arg1, (long) arg2, (long) arg3,	\
			      dummy4, dummy5, __NR_##name);		\
}

#define _syscall4(type,name,type1,arg1,type2,arg2,type3,arg3,type4,arg4)	\
type										\
name (type1 arg1, type2 arg2, type3 arg3, type4 arg4)				\
{										\
	register long dummy5 __asm__ ("out4");					\
										\
	return __ia64_syscall((long) arg1, (long) arg2, (long) arg3,		\
			      (long) arg4, dummy5, __NR_##name);		\
}

#define _syscall5(type,name,type1,arg1,type2,arg2,type3,arg3,type4,arg4,type5,arg5)	\
type											\
name (type1 arg1, type2 arg2, type3 arg3, type4 arg4, type5 arg5)			\
{											\
	return __ia64_syscall((long) arg1, (long) arg2, (long) arg3,			\
			      (long) arg4, (long) arg5, __NR_##name);			\
}

#ifdef __KERNEL_SYSCALLS__

static inline _syscall0(int,sync)
static inline _syscall0(pid_t,setsid)
static inline _syscall3(int,write,int,fd,const char *,buf,off_t,count)
static inline _syscall3(int,read,int,fd,char *,buf,off_t,count)
static inline _syscall3(off_t,lseek,int,fd,off_t,offset,int,count)
static inline _syscall1(int,dup,int,fd)
static inline _syscall3(int,execve,const char *,file,char **,argv,char **,envp)
static inline _syscall3(int,open,const char *,file,int,flag,int,mode)
static inline _syscall1(int,close,int,fd)
static inline _syscall4(pid_t,wait4,pid_t,pid,int *,wait_stat,int,options,struct rusage*, rusage)
static inline _syscall1(int,delete_module,const char *,name)
static inline _syscall2(pid_t,clone,unsigned long,flags,void*,sp);

#define __NR__exit __NR_exit
static inline _syscall1(int,_exit,int,exitcode)

static inline pid_t
waitpid (int pid, int *wait_stat, int flags)
{
	return wait4(pid, wait_stat, flags, NULL);
}

static inline pid_t
wait (int * wait_stat)
{
	return wait4(-1, wait_stat, 0, 0);
}

#endif /* __KERNEL_SYSCALLS__ */
#endif /* !__ASSEMBLY__ */
#endif /* _ASM_IA64_UNISTD_H */
