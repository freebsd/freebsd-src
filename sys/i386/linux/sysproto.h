/*-
 * Prototypes for linux system call functions.
 * Bruce Evans, November 1995.
 * This file is in the public domain.
 */

#ifndef _I386_LINUX_SYSPROTO_H_
#define _I386_LINUX_SYSPROTO_H_

struct linux_accept_args;
struct linux_alarm_args;
struct linux_bind_args;
struct linux_brk_args;
struct linux_connect_args;
struct linux_creat_args;
struct linux_fcntl_args;
struct linux_fstatfs_args;
struct linux_getpeername_args;
struct linux_getpgid_args;
struct linux_getsockname_args;
struct linux_getsockopt_args;
struct linux_ioctl_args;
struct linux_ipc_args;
struct linux_kill_args;
struct linux_listen_args;
struct linux_lseek_args;
struct linux_mmap_args;
struct linux_mknod_args;
struct linux_newfstat_args;
struct linux_newstat_args;
struct linux_newuname_args;
struct linux_open_args;
struct linux_pipe_args;
struct linux_readdir_args;
struct linux_recv_args;
struct linux_recvfrom_args;
struct linux_select_args;
struct linux_send_args;
struct linux_sendto_args;
struct linux_setsockopt_args;
struct linux_shutdown_args;
struct linux_sigaction_args;
struct linux_sigpending_args;
struct linux_sigprocmask_args;
struct linux_sigsetmask_args;
struct linux_sigsuspend_args;
struct linux_socket_args;
struct linux_socketcall_args;
struct linux_socketpair_args;
struct linux_statfs_args;
struct linux_time_args;
struct linux_tms_args;
struct linux_uselib_args;
struct linux_utime_args;
struct linux_wait4_args;
struct linux_waitpid_args;

/* linux_dummy.c */
int	linux_adjtimex __P((struct proc *p, void *args, int *retval));
int	linux_bdflush __P((struct proc *p, void *args, int *retval));
int	linux_break __P((struct proc *p, void *args, int *retval));
int	linux_clone __P((struct proc *p, void *args, int *retval));
int	linux_create_module __P((struct proc *p, void *args, int *retval));
int	linux_delete_module __P((struct proc *p, void *args, int *retval));
int	linux_fstat __P((struct proc *p, void *args, int *retval));
int	linux_ftime __P((struct proc *p, void *args, int *retval));
int	linux_get_kernel_syms __P((struct proc *p, void *args, int *retval));
int	linux_gtty __P((struct proc *p, void *args, int *retval));
int	linux_idle __P((struct proc *p, void *args, int *retval));
int	linux_init_module __P((struct proc *p, void *args, int *retval));
int	linux_ioperm __P((struct proc *p, void *args, int *retval));
int	linux_iopl __P((struct proc *p, void *args, int *retval));
int	linux_lock __P((struct proc *p, void *args, int *retval));
int	linux_modify_ldt __P((struct proc *p, void *args, int *retval));
int	linux_mount __P((struct proc *p, void *args, int *retval));
int	linux_mpx __P((struct proc *p, void *args, int *retval));
int	linux_nice __P((struct proc *p, void *args, int *retval));
int	linux_olduname __P((struct proc *p, void *args, int *retval));
int	linux_pause __P((struct proc *p, void *args, int *retval));
int	linux_phys __P((struct proc *p, void *args, int *retval));
int	linux_prof __P((struct proc *p, void *args, int *retval));
int	linux_ptrace __P((struct proc *p, void *args, int *retval));
int	linux_quotactl __P((struct proc *p, void *args, int *retval));
int	linux_setup __P((struct proc *p, void *args, int *retval));
int	linux_signal __P((struct proc *p, void *args, int *retval));
int	linux_sigreturn __P((struct proc *p, void *args, int *retval));
int	linux_stat __P((struct proc *p, void *args, int *retval));
int	linux_stime __P((struct proc *p, void *args, int *retval));
int	linux_stty __P((struct proc *p, void *args, int *retval));
int	linux_swapoff __P((struct proc *p, void *args, int *retval));
int	linux_sysinfo __P((struct proc *p, void *args, int *retval));
int	linux_syslog __P((struct proc *p, void *args, int *retval));
int	linux_ulimit __P((struct proc *p, void *args, int *retval));
int	linux_umount __P((struct proc *p, void *args, int *retval));
int	linux_uname __P((struct proc *p, void *args, int *retval));
int	linux_ustat __P((struct proc *p, void *args, int *retval));
int	linux_vhangup __P((struct proc *p, void *args, int *retval));
int	linux_vm86 __P((struct proc *p, void *args, int *retval));

/* linux_file.c */
int	linux_creat __P((struct proc *p, struct linux_creat_args *args,
			 int *retval));
int	linux_fcntl __P((struct proc *p, struct linux_fcntl_args *args,
			 int *retval));
int	linux_lseek __P((struct proc *p, struct linux_lseek_args *args,
			 int *retval));
int	linux_open __P((struct proc *p, struct linux_open_args *args,
			int *retval));
int	linux_readdir __P((struct proc *p, struct linux_readdir_args *args,
			   int *retval));

/* linux_ioctl.c */
int	linux_ioctl __P((struct proc *p, struct linux_ioctl_args *args,
			 int *retval));

/* linux_ipc.c */
int	linux_ipc __P((struct proc *p, struct linux_ipc_args *args,
		       int *retval));
int	linux_msgctl __P((struct proc *p, struct linux_ipc_args *args,
			  int *retval));
int	linux_msgget __P((struct proc *p, struct linux_ipc_args *args,
			  int *retval));
int	linux_msgrcv __P((struct proc *p, struct linux_ipc_args *args,
			  int *retval));
int	linux_msgsnd __P((struct proc *p, struct linux_ipc_args *args,
			  int *retval));
int	linux_semctl __P((struct proc *p, struct linux_ipc_args *args,
			  int *retval));
int	linux_semget __P((struct proc *p, struct linux_ipc_args *args,
			  int *retval));
int	linux_semop __P((struct proc *p, struct linux_ipc_args *args,
			 int *retval));
int	linux_shmat __P((struct proc *p, struct linux_ipc_args *args,
			 int *retval));
int	linux_shmctl __P((struct proc *p, struct linux_ipc_args *args,
			  int *retval));
int	linux_shmdt __P((struct proc *p, struct linux_ipc_args *args,
			 int *retval));
int	linux_shmget __P((struct proc *p, struct linux_ipc_args *args,
			  int *retval));

/* linux_misc.c */
int	linux_alarm __P((struct proc *p, struct linux_alarm_args *args,
			 int *retval));
int	linux_brk __P((struct proc *p, struct linux_brk_args *args,
		       int *retval));
int	linux_fork __P((struct proc *p, void *args, int *retval));
int	linux_getpgid __P((struct proc *p, struct linux_getpgid_args *args,
			   int *retval));
int	linux_mknod __P((struct proc *p, struct linux_mknod_args *args,
			int *retval));
int	linux_mmap __P((struct proc *p, struct linux_mmap_args *args,
			int *retval));
int	linux_newuname __P((struct proc *p, struct linux_newuname_args *args,
			    int *retval));
int	linux_pipe __P((struct proc *p, struct linux_pipe_args *args,
			int *retval));
int	linux_select __P((struct proc *p, struct linux_select_args *args,
			  int *retval));
int	linux_time __P((struct proc *p, struct linux_time_args *args,
			int *retval));
int	linux_times __P((struct proc *p, struct linux_tms_args *args,
			 int *retval));
int	linux_uselib __P((struct proc *p, struct linux_uselib_args *args,
			  int *retval));
int	linux_utime __P((struct proc *p, struct linux_utime_args *args,
			 int *retval));
int	linux_wait4 __P((struct proc *p, struct linux_wait4_args *args,
			 int *retval));
int	linux_waitpid __P((struct proc *p, struct linux_waitpid_args *args,
			   int *retval));

/* linux_signal.c */
int	linux_kill __P((struct proc *p, struct linux_kill_args *args,
			int *retval));
int	linux_sigaction __P((struct proc *p, struct linux_sigaction_args *args,
			     int *retval));
int	linux_siggetmask __P((struct proc *p, void *args, int *retval));
int	linux_sigpending __P((struct proc *p,
			      struct linux_sigpending_args *args, int *retval));
int	linux_sigprocmask __P((struct proc *p,
			       struct linux_sigprocmask_args *args,
			       int *retval));
int	linux_sigsetmask __P((struct proc *p,
			      struct linux_sigsetmask_args *args, int *retval));
int	linux_sigsuspend __P((struct proc *p,
			      struct linux_sigsuspend_args *args, int *retval));

/* linux_socket.c */
int	linux_socketcall __P((struct proc *p,
			      struct linux_socketcall_args *args, int *retval));

/* linux_stats.c */
int	linux_fstatfs __P((struct proc *p, struct linux_fstatfs_args *args,
			   int *retval));
int	linux_newfstat __P((struct proc *p, struct linux_newfstat_args *args,
			    int *retval));
int	linux_newlstat __P((struct proc *p, struct linux_newstat_args *args,
			    int *retval));
int	linux_newstat __P((struct proc *p, struct linux_newstat_args *args,
			   int *retval));
int	linux_statfs __P((struct proc *p, struct linux_statfs_args *args,
			  int *retval));

struct image_params;
int	linux_fixup __P((int **stack_base, struct image_params *iparams));

extern struct sysentvec linux_sysvec;

#endif /* !_I386_LINUX_SYSPROTO_H_ */
