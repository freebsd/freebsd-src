/*
 * System call prototypes.
 *
 * DO NOT EDIT-- this file is automatically @generated.
 */

#ifndef _FREEBSD32_SYSPROTO_H_
#define	_FREEBSD32_SYSPROTO_H_

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/cpuset.h>
#include <sys/domainset.h>
#include <sys/_ffcounter.h>
#include <sys/_semaphore.h>
#include <sys/ucontext.h>
#include <sys/wait.h>

#include <bsm/audit_kevents.h>

struct proc;

struct thread;

#define	PAD_(t)	(sizeof(syscallarg_t) <= sizeof(t) ? \
		0 : sizeof(syscallarg_t) - sizeof(t))

#if BYTE_ORDER == LITTLE_ENDIAN
#define	PADL_(t)	0
#define	PADR_(t)	PAD_(t)
#else
#define	PADL_(t)	PAD_(t)
#define	PADR_(t)	0
#endif

#if !defined(PAD64_REQUIRED) && !defined(__amd64__)
#define PAD64_REQUIRED
#endif
struct freebsd32_wait4_args {
	char pid_l_[PADL_(int)]; int pid; char pid_r_[PADR_(int)];
	char status_l_[PADL_(int *)]; int * status; char status_r_[PADR_(int *)];
	char options_l_[PADL_(int)]; int options; char options_r_[PADR_(int)];
	char rusage_l_[PADL_(struct rusage32 *)]; struct rusage32 * rusage; char rusage_r_[PADR_(struct rusage32 *)];
};
struct freebsd32_ptrace_args {
	char req_l_[PADL_(int)]; int req; char req_r_[PADR_(int)];
	char pid_l_[PADL_(pid_t)]; pid_t pid; char pid_r_[PADR_(pid_t)];
	char addr_l_[PADL_(caddr_t)]; caddr_t addr; char addr_r_[PADR_(caddr_t)];
	char data_l_[PADL_(int)]; int data; char data_r_[PADR_(int)];
};
struct freebsd32_recvmsg_args {
	char s_l_[PADL_(int)]; int s; char s_r_[PADR_(int)];
	char msg_l_[PADL_(struct msghdr32 *)]; struct msghdr32 * msg; char msg_r_[PADR_(struct msghdr32 *)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
};
struct freebsd32_sendmsg_args {
	char s_l_[PADL_(int)]; int s; char s_r_[PADR_(int)];
	char msg_l_[PADL_(const struct msghdr32 *)]; const struct msghdr32 * msg; char msg_r_[PADR_(const struct msghdr32 *)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
};
struct freebsd32_sigaltstack_args {
	char ss_l_[PADL_(const struct sigaltstack32 *)]; const struct sigaltstack32 * ss; char ss_r_[PADR_(const struct sigaltstack32 *)];
	char oss_l_[PADL_(struct sigaltstack32 *)]; struct sigaltstack32 * oss; char oss_r_[PADR_(struct sigaltstack32 *)];
};
struct freebsd32_ioctl_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char com_l_[PADL_(u_long)]; u_long com; char com_r_[PADR_(u_long)];
	char data_l_[PADL_(char *)]; char * data; char data_r_[PADR_(char *)];
};
struct freebsd32_execve_args {
	char fname_l_[PADL_(const char *)]; const char * fname; char fname_r_[PADR_(const char *)];
	char argv_l_[PADL_(uint32_t *)]; uint32_t * argv; char argv_r_[PADR_(uint32_t *)];
	char envv_l_[PADL_(uint32_t *)]; uint32_t * envv; char envv_r_[PADR_(uint32_t *)];
};
struct freebsd32_mprotect_args {
	char addr_l_[PADL_(void *)]; void * addr; char addr_r_[PADR_(void *)];
	char len_l_[PADL_(size_t)]; size_t len; char len_r_[PADR_(size_t)];
	char prot_l_[PADL_(int)]; int prot; char prot_r_[PADR_(int)];
};
struct freebsd32_setitimer_args {
	char which_l_[PADL_(int)]; int which; char which_r_[PADR_(int)];
	char itv_l_[PADL_(const struct itimerval32 *)]; const struct itimerval32 * itv; char itv_r_[PADR_(const struct itimerval32 *)];
	char oitv_l_[PADL_(struct itimerval32 *)]; struct itimerval32 * oitv; char oitv_r_[PADR_(struct itimerval32 *)];
};
struct freebsd32_getitimer_args {
	char which_l_[PADL_(int)]; int which; char which_r_[PADR_(int)];
	char itv_l_[PADL_(struct itimerval32 *)]; struct itimerval32 * itv; char itv_r_[PADR_(struct itimerval32 *)];
};
struct freebsd32_fcntl_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char cmd_l_[PADL_(int)]; int cmd; char cmd_r_[PADR_(int)];
	char arg_l_[PADL_(intptr_t)]; intptr_t arg; char arg_r_[PADR_(intptr_t)];
};
struct freebsd32_select_args {
	char nd_l_[PADL_(int)]; int nd; char nd_r_[PADR_(int)];
	char in_l_[PADL_(fd_set *)]; fd_set * in; char in_r_[PADR_(fd_set *)];
	char ou_l_[PADL_(fd_set *)]; fd_set * ou; char ou_r_[PADR_(fd_set *)];
	char ex_l_[PADL_(fd_set *)]; fd_set * ex; char ex_r_[PADR_(fd_set *)];
	char tv_l_[PADL_(struct timeval32 *)]; struct timeval32 * tv; char tv_r_[PADR_(struct timeval32 *)];
};
struct freebsd32_gettimeofday_args {
	char tp_l_[PADL_(struct timeval32 *)]; struct timeval32 * tp; char tp_r_[PADR_(struct timeval32 *)];
	char tzp_l_[PADL_(struct timezone *)]; struct timezone * tzp; char tzp_r_[PADR_(struct timezone *)];
};
struct freebsd32_getrusage_args {
	char who_l_[PADL_(int)]; int who; char who_r_[PADR_(int)];
	char rusage_l_[PADL_(struct rusage32 *)]; struct rusage32 * rusage; char rusage_r_[PADR_(struct rusage32 *)];
};
struct freebsd32_readv_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char iovp_l_[PADL_(const struct iovec32 *)]; const struct iovec32 * iovp; char iovp_r_[PADR_(const struct iovec32 *)];
	char iovcnt_l_[PADL_(u_int)]; u_int iovcnt; char iovcnt_r_[PADR_(u_int)];
};
struct freebsd32_writev_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char iovp_l_[PADL_(const struct iovec32 *)]; const struct iovec32 * iovp; char iovp_r_[PADR_(const struct iovec32 *)];
	char iovcnt_l_[PADL_(u_int)]; u_int iovcnt; char iovcnt_r_[PADR_(u_int)];
};
struct freebsd32_settimeofday_args {
	char tv_l_[PADL_(const struct timeval32 *)]; const struct timeval32 * tv; char tv_r_[PADR_(const struct timeval32 *)];
	char tzp_l_[PADL_(const struct timezone *)]; const struct timezone * tzp; char tzp_r_[PADR_(const struct timezone *)];
};
struct freebsd32_utimes_args {
	char path_l_[PADL_(const char *)]; const char * path; char path_r_[PADR_(const char *)];
	char tptr_l_[PADL_(const struct timeval32 *)]; const struct timeval32 * tptr; char tptr_r_[PADR_(const struct timeval32 *)];
};
struct freebsd32_adjtime_args {
	char delta_l_[PADL_(const struct timeval32 *)]; const struct timeval32 * delta; char delta_r_[PADR_(const struct timeval32 *)];
	char olddelta_l_[PADL_(struct timeval32 *)]; struct timeval32 * olddelta; char olddelta_r_[PADR_(struct timeval32 *)];
};
struct freebsd32_sysarch_args {
	char op_l_[PADL_(int)]; int op; char op_r_[PADR_(int)];
	char parms_l_[PADL_(char *)]; char * parms; char parms_r_[PADR_(char *)];
};
struct freebsd32_semsys_args {
	char which_l_[PADL_(int)]; int which; char which_r_[PADR_(int)];
	char a2_l_[PADL_(int)]; int a2; char a2_r_[PADR_(int)];
	char a3_l_[PADL_(int)]; int a3; char a3_r_[PADR_(int)];
	char a4_l_[PADL_(int)]; int a4; char a4_r_[PADR_(int)];
	char a5_l_[PADL_(int)]; int a5; char a5_r_[PADR_(int)];
};
struct freebsd32_msgsys_args {
	char which_l_[PADL_(int)]; int which; char which_r_[PADR_(int)];
	char a2_l_[PADL_(int)]; int a2; char a2_r_[PADR_(int)];
	char a3_l_[PADL_(int)]; int a3; char a3_r_[PADR_(int)];
	char a4_l_[PADL_(int)]; int a4; char a4_r_[PADR_(int)];
	char a5_l_[PADL_(int)]; int a5; char a5_r_[PADR_(int)];
	char a6_l_[PADL_(int)]; int a6; char a6_r_[PADR_(int)];
};
struct freebsd32_shmsys_args {
	char which_l_[PADL_(int)]; int which; char which_r_[PADR_(int)];
	char a2_l_[PADL_(int)]; int a2; char a2_r_[PADR_(int)];
	char a3_l_[PADL_(int)]; int a3; char a3_r_[PADR_(int)];
	char a4_l_[PADL_(int)]; int a4; char a4_r_[PADR_(int)];
};
struct freebsd32_ntp_adjtime_args {
	char tp_l_[PADL_(struct timex32 *)]; struct timex32 * tp; char tp_r_[PADR_(struct timex32 *)];
};
struct freebsd32___sysctl_args {
	char name_l_[PADL_(int *)]; int * name; char name_r_[PADR_(int *)];
	char namelen_l_[PADL_(u_int)]; u_int namelen; char namelen_r_[PADR_(u_int)];
	char old_l_[PADL_(void *)]; void * old; char old_r_[PADR_(void *)];
	char oldlenp_l_[PADL_(uint32_t *)]; uint32_t * oldlenp; char oldlenp_r_[PADR_(uint32_t *)];
	char new_l_[PADL_(const void *)]; const void * new; char new_r_[PADR_(const void *)];
	char newlen_l_[PADL_(size_t)]; size_t newlen; char newlen_r_[PADR_(size_t)];
};
struct freebsd32_futimes_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char tptr_l_[PADL_(const struct timeval32 *)]; const struct timeval32 * tptr; char tptr_r_[PADR_(const struct timeval32 *)];
};
struct freebsd32_msgsnd_args {
	char msqid_l_[PADL_(int)]; int msqid; char msqid_r_[PADR_(int)];
	char msgp_l_[PADL_(const void *)]; const void * msgp; char msgp_r_[PADR_(const void *)];
	char msgsz_l_[PADL_(size_t)]; size_t msgsz; char msgsz_r_[PADR_(size_t)];
	char msgflg_l_[PADL_(int)]; int msgflg; char msgflg_r_[PADR_(int)];
};
struct freebsd32_msgrcv_args {
	char msqid_l_[PADL_(int)]; int msqid; char msqid_r_[PADR_(int)];
	char msgp_l_[PADL_(void *)]; void * msgp; char msgp_r_[PADR_(void *)];
	char msgsz_l_[PADL_(size_t)]; size_t msgsz; char msgsz_r_[PADR_(size_t)];
	char msgtyp_l_[PADL_(int32_t)]; int32_t msgtyp; char msgtyp_r_[PADR_(int32_t)];
	char msgflg_l_[PADL_(int)]; int msgflg; char msgflg_r_[PADR_(int)];
};
struct freebsd32_clock_gettime_args {
	char clock_id_l_[PADL_(clockid_t)]; clockid_t clock_id; char clock_id_r_[PADR_(clockid_t)];
	char tp_l_[PADL_(struct timespec32 *)]; struct timespec32 * tp; char tp_r_[PADR_(struct timespec32 *)];
};
struct freebsd32_clock_settime_args {
	char clock_id_l_[PADL_(clockid_t)]; clockid_t clock_id; char clock_id_r_[PADR_(clockid_t)];
	char tp_l_[PADL_(const struct timespec32 *)]; const struct timespec32 * tp; char tp_r_[PADR_(const struct timespec32 *)];
};
struct freebsd32_clock_getres_args {
	char clock_id_l_[PADL_(clockid_t)]; clockid_t clock_id; char clock_id_r_[PADR_(clockid_t)];
	char tp_l_[PADL_(struct timespec32 *)]; struct timespec32 * tp; char tp_r_[PADR_(struct timespec32 *)];
};
struct freebsd32_ktimer_create_args {
	char clock_id_l_[PADL_(clockid_t)]; clockid_t clock_id; char clock_id_r_[PADR_(clockid_t)];
	char evp_l_[PADL_(struct sigevent32 *)]; struct sigevent32 * evp; char evp_r_[PADR_(struct sigevent32 *)];
	char timerid_l_[PADL_(int *)]; int * timerid; char timerid_r_[PADR_(int *)];
};
struct freebsd32_ktimer_settime_args {
	char timerid_l_[PADL_(int)]; int timerid; char timerid_r_[PADR_(int)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
	char value_l_[PADL_(const struct itimerspec32 *)]; const struct itimerspec32 * value; char value_r_[PADR_(const struct itimerspec32 *)];
	char ovalue_l_[PADL_(struct itimerspec32 *)]; struct itimerspec32 * ovalue; char ovalue_r_[PADR_(struct itimerspec32 *)];
};
struct freebsd32_ktimer_gettime_args {
	char timerid_l_[PADL_(int)]; int timerid; char timerid_r_[PADR_(int)];
	char value_l_[PADL_(struct itimerspec32 *)]; struct itimerspec32 * value; char value_r_[PADR_(struct itimerspec32 *)];
};
struct freebsd32_nanosleep_args {
	char rqtp_l_[PADL_(const struct timespec32 *)]; const struct timespec32 * rqtp; char rqtp_r_[PADR_(const struct timespec32 *)];
	char rmtp_l_[PADL_(struct timespec32 *)]; struct timespec32 * rmtp; char rmtp_r_[PADR_(struct timespec32 *)];
};
struct freebsd32_ffclock_setestimate_args {
	char cest_l_[PADL_(struct ffclock_estimate32 *)]; struct ffclock_estimate32 * cest; char cest_r_[PADR_(struct ffclock_estimate32 *)];
};
struct freebsd32_ffclock_getestimate_args {
	char cest_l_[PADL_(struct ffclock_estimate32 *)]; struct ffclock_estimate32 * cest; char cest_r_[PADR_(struct ffclock_estimate32 *)];
};
struct freebsd32_clock_nanosleep_args {
	char clock_id_l_[PADL_(clockid_t)]; clockid_t clock_id; char clock_id_r_[PADR_(clockid_t)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
	char rqtp_l_[PADL_(const struct timespec32 *)]; const struct timespec32 * rqtp; char rqtp_r_[PADR_(const struct timespec32 *)];
	char rmtp_l_[PADL_(struct timespec32 *)]; struct timespec32 * rmtp; char rmtp_r_[PADR_(struct timespec32 *)];
};
struct freebsd32_clock_getcpuclockid2_args {
	char id1_l_[PADL_(uint32_t)]; uint32_t id1; char id1_r_[PADR_(uint32_t)];
	char id2_l_[PADL_(uint32_t)]; uint32_t id2; char id2_r_[PADR_(uint32_t)];
	char which_l_[PADL_(int)]; int which; char which_r_[PADR_(int)];
	char clock_id_l_[PADL_(clockid_t *)]; clockid_t * clock_id; char clock_id_r_[PADR_(clockid_t *)];
};
struct freebsd32_aio_read_args {
	char aiocbp_l_[PADL_(struct aiocb32 *)]; struct aiocb32 * aiocbp; char aiocbp_r_[PADR_(struct aiocb32 *)];
};
struct freebsd32_aio_write_args {
	char aiocbp_l_[PADL_(struct aiocb32 *)]; struct aiocb32 * aiocbp; char aiocbp_r_[PADR_(struct aiocb32 *)];
};
struct freebsd32_lio_listio_args {
	char mode_l_[PADL_(int)]; int mode; char mode_r_[PADR_(int)];
	char acb_list_l_[PADL_(uint32_t *)]; uint32_t * acb_list; char acb_list_r_[PADR_(uint32_t *)];
	char nent_l_[PADL_(int)]; int nent; char nent_r_[PADR_(int)];
	char sig_l_[PADL_(struct sigevent32 *)]; struct sigevent32 * sig; char sig_r_[PADR_(struct sigevent32 *)];
};
struct freebsd32_lutimes_args {
	char path_l_[PADL_(const char *)]; const char * path; char path_r_[PADR_(const char *)];
	char tptr_l_[PADL_(const struct timeval32 *)]; const struct timeval32 * tptr; char tptr_r_[PADR_(const struct timeval32 *)];
};
struct freebsd32_preadv_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char iovp_l_[PADL_(struct iovec32 *)]; struct iovec32 * iovp; char iovp_r_[PADR_(struct iovec32 *)];
	char iovcnt_l_[PADL_(u_int)]; u_int iovcnt; char iovcnt_r_[PADR_(u_int)];
#ifdef PAD64_REQUIRED
	char _pad_l_[PADL_(int)]; int _pad; char _pad_r_[PADR_(int)];
#endif
	char offset1_l_[PADL_(uint32_t)]; uint32_t offset1; char offset1_r_[PADR_(uint32_t)];
	char offset2_l_[PADL_(uint32_t)]; uint32_t offset2; char offset2_r_[PADR_(uint32_t)];
};
struct freebsd32_pwritev_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char iovp_l_[PADL_(struct iovec32 *)]; struct iovec32 * iovp; char iovp_r_[PADR_(struct iovec32 *)];
	char iovcnt_l_[PADL_(u_int)]; u_int iovcnt; char iovcnt_r_[PADR_(u_int)];
#ifdef PAD64_REQUIRED
	char _pad_l_[PADL_(int)]; int _pad; char _pad_r_[PADR_(int)];
#endif
	char offset1_l_[PADL_(uint32_t)]; uint32_t offset1; char offset1_r_[PADR_(uint32_t)];
	char offset2_l_[PADL_(uint32_t)]; uint32_t offset2; char offset2_r_[PADR_(uint32_t)];
};
struct freebsd32_modstat_args {
	char modid_l_[PADL_(int)]; int modid; char modid_r_[PADR_(int)];
	char stat_l_[PADL_(struct module_stat32 *)]; struct module_stat32 * stat; char stat_r_[PADR_(struct module_stat32 *)];
};
struct freebsd32_kldstat_args {
	char fileid_l_[PADL_(int)]; int fileid; char fileid_r_[PADR_(int)];
	char stat_l_[PADL_(struct kld_file_stat32 *)]; struct kld_file_stat32 * stat; char stat_r_[PADR_(struct kld_file_stat32 *)];
};
struct freebsd32_aio_return_args {
	char aiocbp_l_[PADL_(struct aiocb32 *)]; struct aiocb32 * aiocbp; char aiocbp_r_[PADR_(struct aiocb32 *)];
};
struct freebsd32_aio_suspend_args {
	char aiocbp_l_[PADL_(uint32_t *)]; uint32_t * aiocbp; char aiocbp_r_[PADR_(uint32_t *)];
	char nent_l_[PADL_(int)]; int nent; char nent_r_[PADR_(int)];
	char timeout_l_[PADL_(const struct timespec32 *)]; const struct timespec32 * timeout; char timeout_r_[PADR_(const struct timespec32 *)];
};
struct freebsd32_aio_error_args {
	char aiocbp_l_[PADL_(struct aiocb32 *)]; struct aiocb32 * aiocbp; char aiocbp_r_[PADR_(struct aiocb32 *)];
};
struct freebsd32_sched_rr_get_interval_args {
	char pid_l_[PADL_(pid_t)]; pid_t pid; char pid_r_[PADR_(pid_t)];
	char interval_l_[PADL_(struct timespec32 *)]; struct timespec32 * interval; char interval_r_[PADR_(struct timespec32 *)];
};
struct freebsd32_jail_args {
	char jail_l_[PADL_(struct jail32 *)]; struct jail32 * jail; char jail_r_[PADR_(struct jail32 *)];
};
struct freebsd32_sigtimedwait_args {
	char set_l_[PADL_(const sigset_t *)]; const sigset_t * set; char set_r_[PADR_(const sigset_t *)];
	char info_l_[PADL_(struct __siginfo32 *)]; struct __siginfo32 * info; char info_r_[PADR_(struct __siginfo32 *)];
	char timeout_l_[PADL_(const struct timespec32 *)]; const struct timespec32 * timeout; char timeout_r_[PADR_(const struct timespec32 *)];
};
struct freebsd32_sigwaitinfo_args {
	char set_l_[PADL_(const sigset_t *)]; const sigset_t * set; char set_r_[PADR_(const sigset_t *)];
	char info_l_[PADL_(struct __siginfo32 *)]; struct __siginfo32 * info; char info_r_[PADR_(struct __siginfo32 *)];
};
struct freebsd32_aio_waitcomplete_args {
	char aiocbp_l_[PADL_(uint32_t *)]; uint32_t * aiocbp; char aiocbp_r_[PADR_(uint32_t *)];
	char timeout_l_[PADL_(struct timespec32 *)]; struct timespec32 * timeout; char timeout_r_[PADR_(struct timespec32 *)];
};
struct freebsd32_nmount_args {
	char iovp_l_[PADL_(struct iovec32 *)]; struct iovec32 * iovp; char iovp_r_[PADR_(struct iovec32 *)];
	char iovcnt_l_[PADL_(unsigned int)]; unsigned int iovcnt; char iovcnt_r_[PADR_(unsigned int)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
};
struct freebsd32_sendfile_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char s_l_[PADL_(int)]; int s; char s_r_[PADR_(int)];
	char offset1_l_[PADL_(uint32_t)]; uint32_t offset1; char offset1_r_[PADR_(uint32_t)];
	char offset2_l_[PADL_(uint32_t)]; uint32_t offset2; char offset2_r_[PADR_(uint32_t)];
	char nbytes_l_[PADL_(size_t)]; size_t nbytes; char nbytes_r_[PADR_(size_t)];
	char hdtr_l_[PADL_(struct sf_hdtr32 *)]; struct sf_hdtr32 * hdtr; char hdtr_r_[PADR_(struct sf_hdtr32 *)];
	char sbytes_l_[PADL_(off_t *)]; off_t * sbytes; char sbytes_r_[PADR_(off_t *)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
};
struct freebsd32_ksem_init_args {
	char idp_l_[PADL_(int32_t *)]; int32_t * idp; char idp_r_[PADR_(int32_t *)];
	char value_l_[PADL_(unsigned int)]; unsigned int value; char value_r_[PADR_(unsigned int)];
};
struct freebsd32_ksem_open_args {
	char idp_l_[PADL_(int32_t *)]; int32_t * idp; char idp_r_[PADR_(int32_t *)];
	char name_l_[PADL_(const char *)]; const char * name; char name_r_[PADR_(const char *)];
	char oflag_l_[PADL_(int)]; int oflag; char oflag_r_[PADR_(int)];
	char mode_l_[PADL_(mode_t)]; mode_t mode; char mode_r_[PADR_(mode_t)];
	char value_l_[PADL_(unsigned int)]; unsigned int value; char value_r_[PADR_(unsigned int)];
};
struct freebsd32_sigaction_args {
	char sig_l_[PADL_(int)]; int sig; char sig_r_[PADR_(int)];
	char act_l_[PADL_(const struct sigaction32 *)]; const struct sigaction32 * act; char act_r_[PADR_(const struct sigaction32 *)];
	char oact_l_[PADL_(struct sigaction32 *)]; struct sigaction32 * oact; char oact_r_[PADR_(struct sigaction32 *)];
};
struct freebsd32_sigreturn_args {
	char sigcntxp_l_[PADL_(const struct __ucontext32 *)]; const struct __ucontext32 * sigcntxp; char sigcntxp_r_[PADR_(const struct __ucontext32 *)];
};
struct freebsd32_getcontext_args {
	char ucp_l_[PADL_(struct __ucontext32 *)]; struct __ucontext32 * ucp; char ucp_r_[PADR_(struct __ucontext32 *)];
};
struct freebsd32_setcontext_args {
	char ucp_l_[PADL_(const struct __ucontext32 *)]; const struct __ucontext32 * ucp; char ucp_r_[PADR_(const struct __ucontext32 *)];
};
struct freebsd32_swapcontext_args {
	char oucp_l_[PADL_(struct __ucontext32 *)]; struct __ucontext32 * oucp; char oucp_r_[PADR_(struct __ucontext32 *)];
	char ucp_l_[PADL_(const struct __ucontext32 *)]; const struct __ucontext32 * ucp; char ucp_r_[PADR_(const struct __ucontext32 *)];
};
struct freebsd32_ksem_timedwait_args {
	char id_l_[PADL_(int32_t)]; int32_t id; char id_r_[PADR_(int32_t)];
	char abstime_l_[PADL_(const struct timespec32 *)]; const struct timespec32 * abstime; char abstime_r_[PADR_(const struct timespec32 *)];
};
struct freebsd32_thr_suspend_args {
	char timeout_l_[PADL_(const struct timespec32 *)]; const struct timespec32 * timeout; char timeout_r_[PADR_(const struct timespec32 *)];
};
struct freebsd32__umtx_op_args {
	char obj_l_[PADL_(void *)]; void * obj; char obj_r_[PADR_(void *)];
	char op_l_[PADL_(int)]; int op; char op_r_[PADR_(int)];
	char val_l_[PADL_(u_long)]; u_long val; char val_r_[PADR_(u_long)];
	char uaddr1_l_[PADL_(void *)]; void * uaddr1; char uaddr1_r_[PADR_(void *)];
	char uaddr2_l_[PADL_(void *)]; void * uaddr2; char uaddr2_r_[PADR_(void *)];
};
struct freebsd32_thr_new_args {
	char param_l_[PADL_(struct thr_param32 *)]; struct thr_param32 * param; char param_r_[PADR_(struct thr_param32 *)];
	char param_size_l_[PADL_(int)]; int param_size; char param_size_r_[PADR_(int)];
};
struct freebsd32_sigqueue_args {
	char pid_l_[PADL_(pid_t)]; pid_t pid; char pid_r_[PADR_(pid_t)];
	char signum_l_[PADL_(int)]; int signum; char signum_r_[PADR_(int)];
	char value_l_[PADL_(void *)]; void * value; char value_r_[PADR_(void *)];
};
struct freebsd32_kmq_open_args {
	char path_l_[PADL_(const char *)]; const char * path; char path_r_[PADR_(const char *)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
	char mode_l_[PADL_(mode_t)]; mode_t mode; char mode_r_[PADR_(mode_t)];
	char attr_l_[PADL_(const struct mq_attr32 *)]; const struct mq_attr32 * attr; char attr_r_[PADR_(const struct mq_attr32 *)];
};
struct freebsd32_kmq_setattr_args {
	char mqd_l_[PADL_(int)]; int mqd; char mqd_r_[PADR_(int)];
	char attr_l_[PADL_(const struct mq_attr32 *)]; const struct mq_attr32 * attr; char attr_r_[PADR_(const struct mq_attr32 *)];
	char oattr_l_[PADL_(struct mq_attr32 *)]; struct mq_attr32 * oattr; char oattr_r_[PADR_(struct mq_attr32 *)];
};
struct freebsd32_kmq_timedreceive_args {
	char mqd_l_[PADL_(int)]; int mqd; char mqd_r_[PADR_(int)];
	char msg_ptr_l_[PADL_(char *)]; char * msg_ptr; char msg_ptr_r_[PADR_(char *)];
	char msg_len_l_[PADL_(size_t)]; size_t msg_len; char msg_len_r_[PADR_(size_t)];
	char msg_prio_l_[PADL_(unsigned *)]; unsigned * msg_prio; char msg_prio_r_[PADR_(unsigned *)];
	char abs_timeout_l_[PADL_(const struct timespec32 *)]; const struct timespec32 * abs_timeout; char abs_timeout_r_[PADR_(const struct timespec32 *)];
};
struct freebsd32_kmq_timedsend_args {
	char mqd_l_[PADL_(int)]; int mqd; char mqd_r_[PADR_(int)];
	char msg_ptr_l_[PADL_(const char *)]; const char * msg_ptr; char msg_ptr_r_[PADR_(const char *)];
	char msg_len_l_[PADL_(size_t)]; size_t msg_len; char msg_len_r_[PADR_(size_t)];
	char msg_prio_l_[PADL_(unsigned)]; unsigned msg_prio; char msg_prio_r_[PADR_(unsigned)];
	char abs_timeout_l_[PADL_(const struct timespec32 *)]; const struct timespec32 * abs_timeout; char abs_timeout_r_[PADR_(const struct timespec32 *)];
};
struct freebsd32_kmq_notify_args {
	char mqd_l_[PADL_(int)]; int mqd; char mqd_r_[PADR_(int)];
	char sigev_l_[PADL_(const struct sigevent32 *)]; const struct sigevent32 * sigev; char sigev_r_[PADR_(const struct sigevent32 *)];
};
struct freebsd32_abort2_args {
	char why_l_[PADL_(const char *)]; const char * why; char why_r_[PADR_(const char *)];
	char nargs_l_[PADL_(int)]; int nargs; char nargs_r_[PADR_(int)];
	char args_l_[PADL_(uint32_t *)]; uint32_t * args; char args_r_[PADR_(uint32_t *)];
};
struct freebsd32_aio_fsync_args {
	char op_l_[PADL_(int)]; int op; char op_r_[PADR_(int)];
	char aiocbp_l_[PADL_(struct aiocb32 *)]; struct aiocb32 * aiocbp; char aiocbp_r_[PADR_(struct aiocb32 *)];
};
struct freebsd32_pread_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char buf_l_[PADL_(void *)]; void * buf; char buf_r_[PADR_(void *)];
	char nbyte_l_[PADL_(size_t)]; size_t nbyte; char nbyte_r_[PADR_(size_t)];
#ifdef PAD64_REQUIRED
	char _pad_l_[PADL_(int)]; int _pad; char _pad_r_[PADR_(int)];
#endif
	char offset1_l_[PADL_(uint32_t)]; uint32_t offset1; char offset1_r_[PADR_(uint32_t)];
	char offset2_l_[PADL_(uint32_t)]; uint32_t offset2; char offset2_r_[PADR_(uint32_t)];
};
struct freebsd32_pwrite_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char buf_l_[PADL_(const void *)]; const void * buf; char buf_r_[PADR_(const void *)];
	char nbyte_l_[PADL_(size_t)]; size_t nbyte; char nbyte_r_[PADR_(size_t)];
#ifdef PAD64_REQUIRED
	char _pad_l_[PADL_(int)]; int _pad; char _pad_r_[PADR_(int)];
#endif
	char offset1_l_[PADL_(uint32_t)]; uint32_t offset1; char offset1_r_[PADR_(uint32_t)];
	char offset2_l_[PADL_(uint32_t)]; uint32_t offset2; char offset2_r_[PADR_(uint32_t)];
};
struct freebsd32_mmap_args {
	char addr_l_[PADL_(void *)]; void * addr; char addr_r_[PADR_(void *)];
	char len_l_[PADL_(size_t)]; size_t len; char len_r_[PADR_(size_t)];
	char prot_l_[PADL_(int)]; int prot; char prot_r_[PADR_(int)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
#ifdef PAD64_REQUIRED
	char _pad_l_[PADL_(int)]; int _pad; char _pad_r_[PADR_(int)];
#endif
	char pos1_l_[PADL_(uint32_t)]; uint32_t pos1; char pos1_r_[PADR_(uint32_t)];
	char pos2_l_[PADL_(uint32_t)]; uint32_t pos2; char pos2_r_[PADR_(uint32_t)];
};
struct freebsd32_lseek_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
#ifdef PAD64_REQUIRED
	char _pad_l_[PADL_(int)]; int _pad; char _pad_r_[PADR_(int)];
#endif
	char offset1_l_[PADL_(uint32_t)]; uint32_t offset1; char offset1_r_[PADR_(uint32_t)];
	char offset2_l_[PADL_(uint32_t)]; uint32_t offset2; char offset2_r_[PADR_(uint32_t)];
	char whence_l_[PADL_(int)]; int whence; char whence_r_[PADR_(int)];
};
struct freebsd32_truncate_args {
	char path_l_[PADL_(const char *)]; const char * path; char path_r_[PADR_(const char *)];
#ifdef PAD64_REQUIRED
	char _pad_l_[PADL_(int)]; int _pad; char _pad_r_[PADR_(int)];
#endif
	char length1_l_[PADL_(uint32_t)]; uint32_t length1; char length1_r_[PADR_(uint32_t)];
	char length2_l_[PADL_(uint32_t)]; uint32_t length2; char length2_r_[PADR_(uint32_t)];
};
struct freebsd32_ftruncate_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
#ifdef PAD64_REQUIRED
	char _pad_l_[PADL_(int)]; int _pad; char _pad_r_[PADR_(int)];
#endif
	char length1_l_[PADL_(uint32_t)]; uint32_t length1; char length1_r_[PADR_(uint32_t)];
	char length2_l_[PADL_(uint32_t)]; uint32_t length2; char length2_r_[PADR_(uint32_t)];
};
struct freebsd32_cpuset_setid_args {
	char which_l_[PADL_(cpuwhich_t)]; cpuwhich_t which; char which_r_[PADR_(cpuwhich_t)];
#ifdef PAD64_REQUIRED
	char _pad_l_[PADL_(int)]; int _pad; char _pad_r_[PADR_(int)];
#endif
	char id1_l_[PADL_(uint32_t)]; uint32_t id1; char id1_r_[PADR_(uint32_t)];
	char id2_l_[PADL_(uint32_t)]; uint32_t id2; char id2_r_[PADR_(uint32_t)];
	char setid_l_[PADL_(cpusetid_t)]; cpusetid_t setid; char setid_r_[PADR_(cpusetid_t)];
};
struct freebsd32_cpuset_getid_args {
	char level_l_[PADL_(cpulevel_t)]; cpulevel_t level; char level_r_[PADR_(cpulevel_t)];
	char which_l_[PADL_(cpuwhich_t)]; cpuwhich_t which; char which_r_[PADR_(cpuwhich_t)];
	char id1_l_[PADL_(uint32_t)]; uint32_t id1; char id1_r_[PADR_(uint32_t)];
	char id2_l_[PADL_(uint32_t)]; uint32_t id2; char id2_r_[PADR_(uint32_t)];
	char setid_l_[PADL_(cpusetid_t *)]; cpusetid_t * setid; char setid_r_[PADR_(cpusetid_t *)];
};
struct freebsd32_cpuset_getaffinity_args {
	char level_l_[PADL_(cpulevel_t)]; cpulevel_t level; char level_r_[PADR_(cpulevel_t)];
	char which_l_[PADL_(cpuwhich_t)]; cpuwhich_t which; char which_r_[PADR_(cpuwhich_t)];
	char id1_l_[PADL_(uint32_t)]; uint32_t id1; char id1_r_[PADR_(uint32_t)];
	char id2_l_[PADL_(uint32_t)]; uint32_t id2; char id2_r_[PADR_(uint32_t)];
	char cpusetsize_l_[PADL_(size_t)]; size_t cpusetsize; char cpusetsize_r_[PADR_(size_t)];
	char mask_l_[PADL_(cpuset_t *)]; cpuset_t * mask; char mask_r_[PADR_(cpuset_t *)];
};
struct freebsd32_cpuset_setaffinity_args {
	char level_l_[PADL_(cpulevel_t)]; cpulevel_t level; char level_r_[PADR_(cpulevel_t)];
	char which_l_[PADL_(cpuwhich_t)]; cpuwhich_t which; char which_r_[PADR_(cpuwhich_t)];
	char id1_l_[PADL_(uint32_t)]; uint32_t id1; char id1_r_[PADR_(uint32_t)];
	char id2_l_[PADL_(uint32_t)]; uint32_t id2; char id2_r_[PADR_(uint32_t)];
	char cpusetsize_l_[PADL_(size_t)]; size_t cpusetsize; char cpusetsize_r_[PADR_(size_t)];
	char mask_l_[PADL_(const cpuset_t *)]; const cpuset_t * mask; char mask_r_[PADR_(const cpuset_t *)];
};
struct freebsd32_fexecve_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char argv_l_[PADL_(uint32_t *)]; uint32_t * argv; char argv_r_[PADR_(uint32_t *)];
	char envv_l_[PADL_(uint32_t *)]; uint32_t * envv; char envv_r_[PADR_(uint32_t *)];
};
struct freebsd32_futimesat_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char path_l_[PADL_(const char *)]; const char * path; char path_r_[PADR_(const char *)];
	char times_l_[PADL_(const struct timeval32 *)]; const struct timeval32 * times; char times_r_[PADR_(const struct timeval32 *)];
};
struct freebsd32_jail_get_args {
	char iovp_l_[PADL_(struct iovec32 *)]; struct iovec32 * iovp; char iovp_r_[PADR_(struct iovec32 *)];
	char iovcnt_l_[PADL_(unsigned int)]; unsigned int iovcnt; char iovcnt_r_[PADR_(unsigned int)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
};
struct freebsd32_jail_set_args {
	char iovp_l_[PADL_(struct iovec32 *)]; struct iovec32 * iovp; char iovp_r_[PADR_(struct iovec32 *)];
	char iovcnt_l_[PADL_(unsigned int)]; unsigned int iovcnt; char iovcnt_r_[PADR_(unsigned int)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
};
struct freebsd32___semctl_args {
	char semid_l_[PADL_(int)]; int semid; char semid_r_[PADR_(int)];
	char semnum_l_[PADL_(int)]; int semnum; char semnum_r_[PADR_(int)];
	char cmd_l_[PADL_(int)]; int cmd; char cmd_r_[PADR_(int)];
	char arg_l_[PADL_(union semun32 *)]; union semun32 * arg; char arg_r_[PADR_(union semun32 *)];
};
struct freebsd32_msgctl_args {
	char msqid_l_[PADL_(int)]; int msqid; char msqid_r_[PADR_(int)];
	char cmd_l_[PADL_(int)]; int cmd; char cmd_r_[PADR_(int)];
	char buf_l_[PADL_(struct msqid_ds32 *)]; struct msqid_ds32 * buf; char buf_r_[PADR_(struct msqid_ds32 *)];
};
struct freebsd32_shmctl_args {
	char shmid_l_[PADL_(int)]; int shmid; char shmid_r_[PADR_(int)];
	char cmd_l_[PADL_(int)]; int cmd; char cmd_r_[PADR_(int)];
	char buf_l_[PADL_(struct shmid_ds32 *)]; struct shmid_ds32 * buf; char buf_r_[PADR_(struct shmid_ds32 *)];
};
struct freebsd32_pselect_args {
	char nd_l_[PADL_(int)]; int nd; char nd_r_[PADR_(int)];
	char in_l_[PADL_(fd_set *)]; fd_set * in; char in_r_[PADR_(fd_set *)];
	char ou_l_[PADL_(fd_set *)]; fd_set * ou; char ou_r_[PADR_(fd_set *)];
	char ex_l_[PADL_(fd_set *)]; fd_set * ex; char ex_r_[PADR_(fd_set *)];
	char ts_l_[PADL_(const struct timespec32 *)]; const struct timespec32 * ts; char ts_r_[PADR_(const struct timespec32 *)];
	char sm_l_[PADL_(const sigset_t *)]; const sigset_t * sm; char sm_r_[PADR_(const sigset_t *)];
};
struct freebsd32_posix_fallocate_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
#ifdef PAD64_REQUIRED
	char _pad_l_[PADL_(int)]; int _pad; char _pad_r_[PADR_(int)];
#endif
	char offset1_l_[PADL_(uint32_t)]; uint32_t offset1; char offset1_r_[PADR_(uint32_t)];
	char offset2_l_[PADL_(uint32_t)]; uint32_t offset2; char offset2_r_[PADR_(uint32_t)];
	char len1_l_[PADL_(uint32_t)]; uint32_t len1; char len1_r_[PADR_(uint32_t)];
	char len2_l_[PADL_(uint32_t)]; uint32_t len2; char len2_r_[PADR_(uint32_t)];
};
struct freebsd32_posix_fadvise_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
#ifdef PAD64_REQUIRED
	char _pad_l_[PADL_(int)]; int _pad; char _pad_r_[PADR_(int)];
#endif
	char offset1_l_[PADL_(uint32_t)]; uint32_t offset1; char offset1_r_[PADR_(uint32_t)];
	char offset2_l_[PADL_(uint32_t)]; uint32_t offset2; char offset2_r_[PADR_(uint32_t)];
	char len1_l_[PADL_(uint32_t)]; uint32_t len1; char len1_r_[PADR_(uint32_t)];
	char len2_l_[PADL_(uint32_t)]; uint32_t len2; char len2_r_[PADR_(uint32_t)];
	char advice_l_[PADL_(int)]; int advice; char advice_r_[PADR_(int)];
};
struct freebsd32_wait6_args {
	char idtype_l_[PADL_(idtype_t)]; idtype_t idtype; char idtype_r_[PADR_(idtype_t)];
#ifdef PAD64_REQUIRED
	char _pad_l_[PADL_(int)]; int _pad; char _pad_r_[PADR_(int)];
#endif
	char id1_l_[PADL_(uint32_t)]; uint32_t id1; char id1_r_[PADR_(uint32_t)];
	char id2_l_[PADL_(uint32_t)]; uint32_t id2; char id2_r_[PADR_(uint32_t)];
	char status_l_[PADL_(int *)]; int * status; char status_r_[PADR_(int *)];
	char options_l_[PADL_(int)]; int options; char options_r_[PADR_(int)];
	char wrusage_l_[PADL_(struct __wrusage32 *)]; struct __wrusage32 * wrusage; char wrusage_r_[PADR_(struct __wrusage32 *)];
	char info_l_[PADL_(struct __siginfo32 *)]; struct __siginfo32 * info; char info_r_[PADR_(struct __siginfo32 *)];
};
struct freebsd32_cap_ioctls_limit_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char cmds_l_[PADL_(const uint32_t *)]; const uint32_t * cmds; char cmds_r_[PADR_(const uint32_t *)];
	char ncmds_l_[PADL_(size_t)]; size_t ncmds; char ncmds_r_[PADR_(size_t)];
};
struct freebsd32_cap_ioctls_get_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char cmds_l_[PADL_(uint32_t *)]; uint32_t * cmds; char cmds_r_[PADR_(uint32_t *)];
	char maxcmds_l_[PADL_(size_t)]; size_t maxcmds; char maxcmds_r_[PADR_(size_t)];
};
struct freebsd32_aio_mlock_args {
	char aiocbp_l_[PADL_(struct aiocb32 *)]; struct aiocb32 * aiocbp; char aiocbp_r_[PADR_(struct aiocb32 *)];
};
struct freebsd32_procctl_args {
	char idtype_l_[PADL_(idtype_t)]; idtype_t idtype; char idtype_r_[PADR_(idtype_t)];
#ifdef PAD64_REQUIRED
	char _pad_l_[PADL_(int)]; int _pad; char _pad_r_[PADR_(int)];
#endif
	char id1_l_[PADL_(uint32_t)]; uint32_t id1; char id1_r_[PADR_(uint32_t)];
	char id2_l_[PADL_(uint32_t)]; uint32_t id2; char id2_r_[PADR_(uint32_t)];
	char com_l_[PADL_(int)]; int com; char com_r_[PADR_(int)];
	char data_l_[PADL_(void *)]; void * data; char data_r_[PADR_(void *)];
};
struct freebsd32_ppoll_args {
	char fds_l_[PADL_(struct pollfd *)]; struct pollfd * fds; char fds_r_[PADR_(struct pollfd *)];
	char nfds_l_[PADL_(u_int)]; u_int nfds; char nfds_r_[PADR_(u_int)];
	char ts_l_[PADL_(const struct timespec32 *)]; const struct timespec32 * ts; char ts_r_[PADR_(const struct timespec32 *)];
	char set_l_[PADL_(const sigset_t *)]; const sigset_t * set; char set_r_[PADR_(const sigset_t *)];
};
struct freebsd32_futimens_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char times_l_[PADL_(const struct timespec32 *)]; const struct timespec32 * times; char times_r_[PADR_(const struct timespec32 *)];
};
struct freebsd32_utimensat_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char path_l_[PADL_(const char *)]; const char * path; char path_r_[PADR_(const char *)];
	char times_l_[PADL_(const struct timespec32 *)]; const struct timespec32 * times; char times_r_[PADR_(const struct timespec32 *)];
	char flag_l_[PADL_(int)]; int flag; char flag_r_[PADR_(int)];
};
struct freebsd32_fstat_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char sb_l_[PADL_(struct stat32 *)]; struct stat32 * sb; char sb_r_[PADR_(struct stat32 *)];
};
struct freebsd32_fstatat_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char path_l_[PADL_(const char *)]; const char * path; char path_r_[PADR_(const char *)];
	char buf_l_[PADL_(struct stat32 *)]; struct stat32 * buf; char buf_r_[PADR_(struct stat32 *)];
	char flag_l_[PADL_(int)]; int flag; char flag_r_[PADR_(int)];
};
struct freebsd32_fhstat_args {
	char u_fhp_l_[PADL_(const struct fhandle *)]; const struct fhandle * u_fhp; char u_fhp_r_[PADR_(const struct fhandle *)];
	char sb_l_[PADL_(struct stat32 *)]; struct stat32 * sb; char sb_r_[PADR_(struct stat32 *)];
};
struct freebsd32_getfsstat_args {
	char buf_l_[PADL_(struct statfs *)]; struct statfs * buf; char buf_r_[PADR_(struct statfs *)];
	char bufsize_l_[PADL_(int32_t)]; int32_t bufsize; char bufsize_r_[PADR_(int32_t)];
	char mode_l_[PADL_(int)]; int mode; char mode_r_[PADR_(int)];
};
struct freebsd32_mknodat_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char path_l_[PADL_(const char *)]; const char * path; char path_r_[PADR_(const char *)];
	char mode_l_[PADL_(mode_t)]; mode_t mode; char mode_r_[PADR_(mode_t)];
#ifdef PAD64_REQUIRED
	char _pad_l_[PADL_(int)]; int _pad; char _pad_r_[PADR_(int)];
#endif
	char dev1_l_[PADL_(uint32_t)]; uint32_t dev1; char dev1_r_[PADR_(uint32_t)];
	char dev2_l_[PADL_(uint32_t)]; uint32_t dev2; char dev2_r_[PADR_(uint32_t)];
};
struct freebsd32_kevent_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char changelist_l_[PADL_(const struct kevent32 *)]; const struct kevent32 * changelist; char changelist_r_[PADR_(const struct kevent32 *)];
	char nchanges_l_[PADL_(int)]; int nchanges; char nchanges_r_[PADR_(int)];
	char eventlist_l_[PADL_(struct kevent32 *)]; struct kevent32 * eventlist; char eventlist_r_[PADR_(struct kevent32 *)];
	char nevents_l_[PADL_(int)]; int nevents; char nevents_r_[PADR_(int)];
	char timeout_l_[PADL_(const struct timespec32 *)]; const struct timespec32 * timeout; char timeout_r_[PADR_(const struct timespec32 *)];
};
struct freebsd32_cpuset_getdomain_args {
	char level_l_[PADL_(cpulevel_t)]; cpulevel_t level; char level_r_[PADR_(cpulevel_t)];
	char which_l_[PADL_(cpuwhich_t)]; cpuwhich_t which; char which_r_[PADR_(cpuwhich_t)];
	char id1_l_[PADL_(uint32_t)]; uint32_t id1; char id1_r_[PADR_(uint32_t)];
	char id2_l_[PADL_(uint32_t)]; uint32_t id2; char id2_r_[PADR_(uint32_t)];
	char domainsetsize_l_[PADL_(size_t)]; size_t domainsetsize; char domainsetsize_r_[PADR_(size_t)];
	char mask_l_[PADL_(domainset_t *)]; domainset_t * mask; char mask_r_[PADR_(domainset_t *)];
	char policy_l_[PADL_(int *)]; int * policy; char policy_r_[PADR_(int *)];
};
struct freebsd32_cpuset_setdomain_args {
	char level_l_[PADL_(cpulevel_t)]; cpulevel_t level; char level_r_[PADR_(cpulevel_t)];
	char which_l_[PADL_(cpuwhich_t)]; cpuwhich_t which; char which_r_[PADR_(cpuwhich_t)];
	char id1_l_[PADL_(uint32_t)]; uint32_t id1; char id1_r_[PADR_(uint32_t)];
	char id2_l_[PADL_(uint32_t)]; uint32_t id2; char id2_r_[PADR_(uint32_t)];
	char domainsetsize_l_[PADL_(size_t)]; size_t domainsetsize; char domainsetsize_r_[PADR_(size_t)];
	char mask_l_[PADL_(domainset_t *)]; domainset_t * mask; char mask_r_[PADR_(domainset_t *)];
	char policy_l_[PADL_(int)]; int policy; char policy_r_[PADR_(int)];
};
struct freebsd32___sysctlbyname_args {
	char name_l_[PADL_(const char *)]; const char * name; char name_r_[PADR_(const char *)];
	char namelen_l_[PADL_(size_t)]; size_t namelen; char namelen_r_[PADR_(size_t)];
	char old_l_[PADL_(void *)]; void * old; char old_r_[PADR_(void *)];
	char oldlenp_l_[PADL_(uint32_t *)]; uint32_t * oldlenp; char oldlenp_r_[PADR_(uint32_t *)];
	char new_l_[PADL_(void *)]; void * new; char new_r_[PADR_(void *)];
	char newlen_l_[PADL_(size_t)]; size_t newlen; char newlen_r_[PADR_(size_t)];
};
struct freebsd32_aio_writev_args {
	char aiocbp_l_[PADL_(struct aiocb32 *)]; struct aiocb32 * aiocbp; char aiocbp_r_[PADR_(struct aiocb32 *)];
};
struct freebsd32_aio_readv_args {
	char aiocbp_l_[PADL_(struct aiocb32 *)]; struct aiocb32 * aiocbp; char aiocbp_r_[PADR_(struct aiocb32 *)];
};
struct freebsd32_timerfd_gettime_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char curr_value_l_[PADL_(struct itimerspec32 *)]; struct itimerspec32 * curr_value; char curr_value_r_[PADR_(struct itimerspec32 *)];
};
struct freebsd32_timerfd_settime_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
	char new_value_l_[PADL_(const struct itimerspec32 *)]; const struct itimerspec32 * new_value; char new_value_r_[PADR_(const struct itimerspec32 *)];
	char old_value_l_[PADL_(struct itimerspec32 *)]; struct itimerspec32 * old_value; char old_value_r_[PADR_(struct itimerspec32 *)];
};
struct freebsd32_setcred_args {
	char flags_l_[PADL_(u_int)]; u_int flags; char flags_r_[PADR_(u_int)];
	char wcred_l_[PADL_(const struct setcred32 *)]; const struct setcred32 * wcred; char wcred_r_[PADR_(const struct setcred32 *)];
	char size_l_[PADL_(size_t)]; size_t size; char size_r_[PADR_(size_t)];
};
int	freebsd32_wait4(struct thread *, struct freebsd32_wait4_args *);
int	freebsd32_ptrace(struct thread *, struct freebsd32_ptrace_args *);
int	freebsd32_recvmsg(struct thread *, struct freebsd32_recvmsg_args *);
int	freebsd32_sendmsg(struct thread *, struct freebsd32_sendmsg_args *);
int	freebsd32_sigaltstack(struct thread *, struct freebsd32_sigaltstack_args *);
int	freebsd32_ioctl(struct thread *, struct freebsd32_ioctl_args *);
int	freebsd32_execve(struct thread *, struct freebsd32_execve_args *);
int	freebsd32_mprotect(struct thread *, struct freebsd32_mprotect_args *);
int	freebsd32_setitimer(struct thread *, struct freebsd32_setitimer_args *);
int	freebsd32_getitimer(struct thread *, struct freebsd32_getitimer_args *);
int	freebsd32_fcntl(struct thread *, struct freebsd32_fcntl_args *);
int	freebsd32_select(struct thread *, struct freebsd32_select_args *);
int	freebsd32_gettimeofday(struct thread *, struct freebsd32_gettimeofday_args *);
int	freebsd32_getrusage(struct thread *, struct freebsd32_getrusage_args *);
int	freebsd32_readv(struct thread *, struct freebsd32_readv_args *);
int	freebsd32_writev(struct thread *, struct freebsd32_writev_args *);
int	freebsd32_settimeofday(struct thread *, struct freebsd32_settimeofday_args *);
int	freebsd32_utimes(struct thread *, struct freebsd32_utimes_args *);
int	freebsd32_adjtime(struct thread *, struct freebsd32_adjtime_args *);
int	freebsd32_sysarch(struct thread *, struct freebsd32_sysarch_args *);
int	freebsd32_semsys(struct thread *, struct freebsd32_semsys_args *);
int	freebsd32_msgsys(struct thread *, struct freebsd32_msgsys_args *);
int	freebsd32_shmsys(struct thread *, struct freebsd32_shmsys_args *);
int	freebsd32_ntp_adjtime(struct thread *, struct freebsd32_ntp_adjtime_args *);
int	freebsd32___sysctl(struct thread *, struct freebsd32___sysctl_args *);
int	freebsd32_futimes(struct thread *, struct freebsd32_futimes_args *);
int	freebsd32_msgsnd(struct thread *, struct freebsd32_msgsnd_args *);
int	freebsd32_msgrcv(struct thread *, struct freebsd32_msgrcv_args *);
int	freebsd32_clock_gettime(struct thread *, struct freebsd32_clock_gettime_args *);
int	freebsd32_clock_settime(struct thread *, struct freebsd32_clock_settime_args *);
int	freebsd32_clock_getres(struct thread *, struct freebsd32_clock_getres_args *);
int	freebsd32_ktimer_create(struct thread *, struct freebsd32_ktimer_create_args *);
int	freebsd32_ktimer_settime(struct thread *, struct freebsd32_ktimer_settime_args *);
int	freebsd32_ktimer_gettime(struct thread *, struct freebsd32_ktimer_gettime_args *);
int	freebsd32_nanosleep(struct thread *, struct freebsd32_nanosleep_args *);
int	freebsd32_ffclock_setestimate(struct thread *, struct freebsd32_ffclock_setestimate_args *);
int	freebsd32_ffclock_getestimate(struct thread *, struct freebsd32_ffclock_getestimate_args *);
int	freebsd32_clock_nanosleep(struct thread *, struct freebsd32_clock_nanosleep_args *);
int	freebsd32_clock_getcpuclockid2(struct thread *, struct freebsd32_clock_getcpuclockid2_args *);
int	freebsd32_aio_read(struct thread *, struct freebsd32_aio_read_args *);
int	freebsd32_aio_write(struct thread *, struct freebsd32_aio_write_args *);
int	freebsd32_lio_listio(struct thread *, struct freebsd32_lio_listio_args *);
int	freebsd32_lutimes(struct thread *, struct freebsd32_lutimes_args *);
int	freebsd32_preadv(struct thread *, struct freebsd32_preadv_args *);
int	freebsd32_pwritev(struct thread *, struct freebsd32_pwritev_args *);
int	freebsd32_modstat(struct thread *, struct freebsd32_modstat_args *);
int	freebsd32_kldstat(struct thread *, struct freebsd32_kldstat_args *);
int	freebsd32_aio_return(struct thread *, struct freebsd32_aio_return_args *);
int	freebsd32_aio_suspend(struct thread *, struct freebsd32_aio_suspend_args *);
int	freebsd32_aio_error(struct thread *, struct freebsd32_aio_error_args *);
int	freebsd32_sched_rr_get_interval(struct thread *, struct freebsd32_sched_rr_get_interval_args *);
int	freebsd32_jail(struct thread *, struct freebsd32_jail_args *);
int	freebsd32_sigtimedwait(struct thread *, struct freebsd32_sigtimedwait_args *);
int	freebsd32_sigwaitinfo(struct thread *, struct freebsd32_sigwaitinfo_args *);
int	freebsd32_aio_waitcomplete(struct thread *, struct freebsd32_aio_waitcomplete_args *);
int	freebsd32_nmount(struct thread *, struct freebsd32_nmount_args *);
int	freebsd32_sendfile(struct thread *, struct freebsd32_sendfile_args *);
int	freebsd32_ksem_init(struct thread *, struct freebsd32_ksem_init_args *);
int	freebsd32_ksem_open(struct thread *, struct freebsd32_ksem_open_args *);
int	freebsd32_sigaction(struct thread *, struct freebsd32_sigaction_args *);
int	freebsd32_sigreturn(struct thread *, struct freebsd32_sigreturn_args *);
int	freebsd32_getcontext(struct thread *, struct freebsd32_getcontext_args *);
int	freebsd32_setcontext(struct thread *, struct freebsd32_setcontext_args *);
int	freebsd32_swapcontext(struct thread *, struct freebsd32_swapcontext_args *);
int	freebsd32_ksem_timedwait(struct thread *, struct freebsd32_ksem_timedwait_args *);
int	freebsd32_thr_suspend(struct thread *, struct freebsd32_thr_suspend_args *);
int	freebsd32__umtx_op(struct thread *, struct freebsd32__umtx_op_args *);
int	freebsd32_thr_new(struct thread *, struct freebsd32_thr_new_args *);
int	freebsd32_sigqueue(struct thread *, struct freebsd32_sigqueue_args *);
int	freebsd32_kmq_open(struct thread *, struct freebsd32_kmq_open_args *);
int	freebsd32_kmq_setattr(struct thread *, struct freebsd32_kmq_setattr_args *);
int	freebsd32_kmq_timedreceive(struct thread *, struct freebsd32_kmq_timedreceive_args *);
int	freebsd32_kmq_timedsend(struct thread *, struct freebsd32_kmq_timedsend_args *);
int	freebsd32_kmq_notify(struct thread *, struct freebsd32_kmq_notify_args *);
int	freebsd32_abort2(struct thread *, struct freebsd32_abort2_args *);
int	freebsd32_aio_fsync(struct thread *, struct freebsd32_aio_fsync_args *);
int	freebsd32_pread(struct thread *, struct freebsd32_pread_args *);
int	freebsd32_pwrite(struct thread *, struct freebsd32_pwrite_args *);
int	freebsd32_mmap(struct thread *, struct freebsd32_mmap_args *);
int	freebsd32_lseek(struct thread *, struct freebsd32_lseek_args *);
int	freebsd32_truncate(struct thread *, struct freebsd32_truncate_args *);
int	freebsd32_ftruncate(struct thread *, struct freebsd32_ftruncate_args *);
int	freebsd32_cpuset_setid(struct thread *, struct freebsd32_cpuset_setid_args *);
int	freebsd32_cpuset_getid(struct thread *, struct freebsd32_cpuset_getid_args *);
int	freebsd32_cpuset_getaffinity(struct thread *, struct freebsd32_cpuset_getaffinity_args *);
int	freebsd32_cpuset_setaffinity(struct thread *, struct freebsd32_cpuset_setaffinity_args *);
int	freebsd32_fexecve(struct thread *, struct freebsd32_fexecve_args *);
int	freebsd32_futimesat(struct thread *, struct freebsd32_futimesat_args *);
int	freebsd32_jail_get(struct thread *, struct freebsd32_jail_get_args *);
int	freebsd32_jail_set(struct thread *, struct freebsd32_jail_set_args *);
int	freebsd32___semctl(struct thread *, struct freebsd32___semctl_args *);
int	freebsd32_msgctl(struct thread *, struct freebsd32_msgctl_args *);
int	freebsd32_shmctl(struct thread *, struct freebsd32_shmctl_args *);
int	freebsd32_pselect(struct thread *, struct freebsd32_pselect_args *);
int	freebsd32_posix_fallocate(struct thread *, struct freebsd32_posix_fallocate_args *);
int	freebsd32_posix_fadvise(struct thread *, struct freebsd32_posix_fadvise_args *);
int	freebsd32_wait6(struct thread *, struct freebsd32_wait6_args *);
int	freebsd32_cap_ioctls_limit(struct thread *, struct freebsd32_cap_ioctls_limit_args *);
int	freebsd32_cap_ioctls_get(struct thread *, struct freebsd32_cap_ioctls_get_args *);
int	freebsd32_aio_mlock(struct thread *, struct freebsd32_aio_mlock_args *);
int	freebsd32_procctl(struct thread *, struct freebsd32_procctl_args *);
int	freebsd32_ppoll(struct thread *, struct freebsd32_ppoll_args *);
int	freebsd32_futimens(struct thread *, struct freebsd32_futimens_args *);
int	freebsd32_utimensat(struct thread *, struct freebsd32_utimensat_args *);
int	freebsd32_fstat(struct thread *, struct freebsd32_fstat_args *);
int	freebsd32_fstatat(struct thread *, struct freebsd32_fstatat_args *);
int	freebsd32_fhstat(struct thread *, struct freebsd32_fhstat_args *);
int	freebsd32_getfsstat(struct thread *, struct freebsd32_getfsstat_args *);
int	freebsd32_mknodat(struct thread *, struct freebsd32_mknodat_args *);
int	freebsd32_kevent(struct thread *, struct freebsd32_kevent_args *);
int	freebsd32_cpuset_getdomain(struct thread *, struct freebsd32_cpuset_getdomain_args *);
int	freebsd32_cpuset_setdomain(struct thread *, struct freebsd32_cpuset_setdomain_args *);
int	freebsd32___sysctlbyname(struct thread *, struct freebsd32___sysctlbyname_args *);
int	freebsd32_aio_writev(struct thread *, struct freebsd32_aio_writev_args *);
int	freebsd32_aio_readv(struct thread *, struct freebsd32_aio_readv_args *);
int	freebsd32_timerfd_gettime(struct thread *, struct freebsd32_timerfd_gettime_args *);
int	freebsd32_timerfd_settime(struct thread *, struct freebsd32_timerfd_settime_args *);
int	freebsd32_setcred(struct thread *, struct freebsd32_setcred_args *);

#ifdef COMPAT_43

struct ofreebsd32_lseek_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char offset_l_[PADL_(int32_t)]; int32_t offset; char offset_r_[PADR_(int32_t)];
	char whence_l_[PADL_(int)]; int whence; char whence_r_[PADR_(int)];
};
struct ofreebsd32_stat_args {
	char path_l_[PADL_(const char *)]; const char * path; char path_r_[PADR_(const char *)];
	char ub_l_[PADL_(struct ostat32 *)]; struct ostat32 * ub; char ub_r_[PADR_(struct ostat32 *)];
};
struct ofreebsd32_lstat_args {
	char path_l_[PADL_(const char *)]; const char * path; char path_r_[PADR_(const char *)];
	char ub_l_[PADL_(struct ostat32 *)]; struct ostat32 * ub; char ub_r_[PADR_(struct ostat32 *)];
};
struct ofreebsd32_sigaction_args {
	char signum_l_[PADL_(int)]; int signum; char signum_r_[PADR_(int)];
	char nsa_l_[PADL_(struct osigaction32 *)]; struct osigaction32 * nsa; char nsa_r_[PADR_(struct osigaction32 *)];
	char osa_l_[PADL_(struct osigaction32 *)]; struct osigaction32 * osa; char osa_r_[PADR_(struct osigaction32 *)];
};
struct ofreebsd32_fstat_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char sb_l_[PADL_(struct ostat32 *)]; struct ostat32 * sb; char sb_r_[PADR_(struct ostat32 *)];
};
struct ofreebsd32_mmap_args {
	char addr_l_[PADL_(void *)]; void * addr; char addr_r_[PADR_(void *)];
	char len_l_[PADL_(int)]; int len; char len_r_[PADR_(int)];
	char prot_l_[PADL_(int)]; int prot; char prot_r_[PADR_(int)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char pos_l_[PADL_(int32_t)]; int32_t pos; char pos_r_[PADR_(int32_t)];
};
struct ofreebsd32_sigreturn_args {
	char sigcntxp_l_[PADL_(struct osigcontext *)]; struct osigcontext * sigcntxp; char sigcntxp_r_[PADR_(struct osigcontext *)];
};
struct ofreebsd32_sigvec_args {
	char signum_l_[PADL_(int)]; int signum; char signum_r_[PADR_(int)];
	char nsv_l_[PADL_(struct sigvec32 *)]; struct sigvec32 * nsv; char nsv_r_[PADR_(struct sigvec32 *)];
	char osv_l_[PADL_(struct sigvec32 *)]; struct sigvec32 * osv; char osv_r_[PADR_(struct sigvec32 *)];
};
struct ofreebsd32_sigstack_args {
	char nss_l_[PADL_(struct sigstack32 *)]; struct sigstack32 * nss; char nss_r_[PADR_(struct sigstack32 *)];
	char oss_l_[PADL_(struct sigstack32 *)]; struct sigstack32 * oss; char oss_r_[PADR_(struct sigstack32 *)];
};
struct ofreebsd32_recvmsg_args {
	char s_l_[PADL_(int)]; int s; char s_r_[PADR_(int)];
	char msg_l_[PADL_(struct omsghdr32 *)]; struct omsghdr32 * msg; char msg_r_[PADR_(struct omsghdr32 *)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
};
struct ofreebsd32_sendmsg_args {
	char s_l_[PADL_(int)]; int s; char s_r_[PADR_(int)];
	char msg_l_[PADL_(const struct omsghdr32 *)]; const struct omsghdr32 * msg; char msg_r_[PADR_(const struct omsghdr32 *)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
};
struct ofreebsd32_truncate_args {
	char path_l_[PADL_(const char *)]; const char * path; char path_r_[PADR_(const char *)];
	char length_l_[PADL_(int32_t)]; int32_t length; char length_r_[PADR_(int32_t)];
};
struct ofreebsd32_ftruncate_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char length_l_[PADL_(int32_t)]; int32_t length; char length_r_[PADR_(int32_t)];
};
struct ofreebsd32_sethostid_args {
	char hostid_l_[PADL_(int32_t)]; int32_t hostid; char hostid_r_[PADR_(int32_t)];
};
struct ofreebsd32_getdirentries_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char buf_l_[PADL_(char *)]; char * buf; char buf_r_[PADR_(char *)];
	char count_l_[PADL_(u_int)]; u_int count; char count_r_[PADR_(u_int)];
	char basep_l_[PADL_(int32_t *)]; int32_t * basep; char basep_r_[PADR_(int32_t *)];
};
int	ofreebsd32_lseek(struct thread *, struct ofreebsd32_lseek_args *);
int	ofreebsd32_stat(struct thread *, struct ofreebsd32_stat_args *);
int	ofreebsd32_lstat(struct thread *, struct ofreebsd32_lstat_args *);
int	ofreebsd32_sigaction(struct thread *, struct ofreebsd32_sigaction_args *);
int	ofreebsd32_fstat(struct thread *, struct ofreebsd32_fstat_args *);
int	ofreebsd32_mmap(struct thread *, struct ofreebsd32_mmap_args *);
int	ofreebsd32_sigreturn(struct thread *, struct ofreebsd32_sigreturn_args *);
int	ofreebsd32_sigvec(struct thread *, struct ofreebsd32_sigvec_args *);
int	ofreebsd32_sigstack(struct thread *, struct ofreebsd32_sigstack_args *);
int	ofreebsd32_recvmsg(struct thread *, struct ofreebsd32_recvmsg_args *);
int	ofreebsd32_sendmsg(struct thread *, struct ofreebsd32_sendmsg_args *);
int	ofreebsd32_truncate(struct thread *, struct ofreebsd32_truncate_args *);
int	ofreebsd32_ftruncate(struct thread *, struct ofreebsd32_ftruncate_args *);
int	ofreebsd32_sethostid(struct thread *, struct ofreebsd32_sethostid_args *);
int	ofreebsd32_getdirentries(struct thread *, struct ofreebsd32_getdirentries_args *);

#endif /* COMPAT_43 */


#ifdef COMPAT_FREEBSD4

struct freebsd4_freebsd32_getfsstat_args {
	char buf_l_[PADL_(struct ostatfs32 *)]; struct ostatfs32 * buf; char buf_r_[PADR_(struct ostatfs32 *)];
	char bufsize_l_[PADL_(int32_t)]; int32_t bufsize; char bufsize_r_[PADR_(int32_t)];
	char mode_l_[PADL_(int)]; int mode; char mode_r_[PADR_(int)];
};
struct freebsd4_freebsd32_statfs_args {
	char path_l_[PADL_(const char *)]; const char * path; char path_r_[PADR_(const char *)];
	char buf_l_[PADL_(struct ostatfs32 *)]; struct ostatfs32 * buf; char buf_r_[PADR_(struct ostatfs32 *)];
};
struct freebsd4_freebsd32_fstatfs_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char buf_l_[PADL_(struct ostatfs32 *)]; struct ostatfs32 * buf; char buf_r_[PADR_(struct ostatfs32 *)];
};
struct freebsd4_freebsd32_fhstatfs_args {
	char u_fhp_l_[PADL_(const struct fhandle *)]; const struct fhandle * u_fhp; char u_fhp_r_[PADR_(const struct fhandle *)];
	char buf_l_[PADL_(struct ostatfs32 *)]; struct ostatfs32 * buf; char buf_r_[PADR_(struct ostatfs32 *)];
};
struct freebsd4_freebsd32_sendfile_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char s_l_[PADL_(int)]; int s; char s_r_[PADR_(int)];
	char offset1_l_[PADL_(uint32_t)]; uint32_t offset1; char offset1_r_[PADR_(uint32_t)];
	char offset2_l_[PADL_(uint32_t)]; uint32_t offset2; char offset2_r_[PADR_(uint32_t)];
	char nbytes_l_[PADL_(size_t)]; size_t nbytes; char nbytes_r_[PADR_(size_t)];
	char hdtr_l_[PADL_(struct sf_hdtr32 *)]; struct sf_hdtr32 * hdtr; char hdtr_r_[PADR_(struct sf_hdtr32 *)];
	char sbytes_l_[PADL_(off_t *)]; off_t * sbytes; char sbytes_r_[PADR_(off_t *)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
};
struct freebsd4_freebsd32_sigaction_args {
	char sig_l_[PADL_(int)]; int sig; char sig_r_[PADR_(int)];
	char act_l_[PADL_(const struct sigaction32 *)]; const struct sigaction32 * act; char act_r_[PADR_(const struct sigaction32 *)];
	char oact_l_[PADL_(struct sigaction32 *)]; struct sigaction32 * oact; char oact_r_[PADR_(struct sigaction32 *)];
};
struct freebsd4_freebsd32_sigreturn_args {
	char sigcntxp_l_[PADL_(const struct freebsd4_ucontext32 *)]; const struct freebsd4_ucontext32 * sigcntxp; char sigcntxp_r_[PADR_(const struct freebsd4_ucontext32 *)];
};
int	freebsd4_freebsd32_getfsstat(struct thread *, struct freebsd4_freebsd32_getfsstat_args *);
int	freebsd4_freebsd32_statfs(struct thread *, struct freebsd4_freebsd32_statfs_args *);
int	freebsd4_freebsd32_fstatfs(struct thread *, struct freebsd4_freebsd32_fstatfs_args *);
int	freebsd4_freebsd32_fhstatfs(struct thread *, struct freebsd4_freebsd32_fhstatfs_args *);
int	freebsd4_freebsd32_sendfile(struct thread *, struct freebsd4_freebsd32_sendfile_args *);
int	freebsd4_freebsd32_sigaction(struct thread *, struct freebsd4_freebsd32_sigaction_args *);
int	freebsd4_freebsd32_sigreturn(struct thread *, struct freebsd4_freebsd32_sigreturn_args *);

#endif /* COMPAT_FREEBSD4 */


#ifdef COMPAT_FREEBSD6

struct freebsd6_freebsd32_pread_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char buf_l_[PADL_(void *)]; void * buf; char buf_r_[PADR_(void *)];
	char nbyte_l_[PADL_(size_t)]; size_t nbyte; char nbyte_r_[PADR_(size_t)];
	char pad_l_[PADL_(int)]; int pad; char pad_r_[PADR_(int)];
	char offset1_l_[PADL_(uint32_t)]; uint32_t offset1; char offset1_r_[PADR_(uint32_t)];
	char offset2_l_[PADL_(uint32_t)]; uint32_t offset2; char offset2_r_[PADR_(uint32_t)];
};
struct freebsd6_freebsd32_pwrite_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char buf_l_[PADL_(const void *)]; const void * buf; char buf_r_[PADR_(const void *)];
	char nbyte_l_[PADL_(size_t)]; size_t nbyte; char nbyte_r_[PADR_(size_t)];
	char pad_l_[PADL_(int)]; int pad; char pad_r_[PADR_(int)];
	char offset1_l_[PADL_(uint32_t)]; uint32_t offset1; char offset1_r_[PADR_(uint32_t)];
	char offset2_l_[PADL_(uint32_t)]; uint32_t offset2; char offset2_r_[PADR_(uint32_t)];
};
struct freebsd6_freebsd32_mmap_args {
	char addr_l_[PADL_(void *)]; void * addr; char addr_r_[PADR_(void *)];
	char len_l_[PADL_(size_t)]; size_t len; char len_r_[PADR_(size_t)];
	char prot_l_[PADL_(int)]; int prot; char prot_r_[PADR_(int)];
	char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char pad_l_[PADL_(int)]; int pad; char pad_r_[PADR_(int)];
	char pos1_l_[PADL_(uint32_t)]; uint32_t pos1; char pos1_r_[PADR_(uint32_t)];
	char pos2_l_[PADL_(uint32_t)]; uint32_t pos2; char pos2_r_[PADR_(uint32_t)];
};
struct freebsd6_freebsd32_lseek_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char pad_l_[PADL_(int)]; int pad; char pad_r_[PADR_(int)];
	char offset1_l_[PADL_(uint32_t)]; uint32_t offset1; char offset1_r_[PADR_(uint32_t)];
	char offset2_l_[PADL_(uint32_t)]; uint32_t offset2; char offset2_r_[PADR_(uint32_t)];
	char whence_l_[PADL_(int)]; int whence; char whence_r_[PADR_(int)];
};
struct freebsd6_freebsd32_truncate_args {
	char path_l_[PADL_(const char *)]; const char * path; char path_r_[PADR_(const char *)];
	char pad_l_[PADL_(int)]; int pad; char pad_r_[PADR_(int)];
	char length1_l_[PADL_(uint32_t)]; uint32_t length1; char length1_r_[PADR_(uint32_t)];
	char length2_l_[PADL_(uint32_t)]; uint32_t length2; char length2_r_[PADR_(uint32_t)];
};
struct freebsd6_freebsd32_ftruncate_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char pad_l_[PADL_(int)]; int pad; char pad_r_[PADR_(int)];
	char length1_l_[PADL_(uint32_t)]; uint32_t length1; char length1_r_[PADR_(uint32_t)];
	char length2_l_[PADL_(uint32_t)]; uint32_t length2; char length2_r_[PADR_(uint32_t)];
};
struct freebsd6_freebsd32_aio_read_args {
	char aiocbp_l_[PADL_(struct oaiocb32 *)]; struct oaiocb32 * aiocbp; char aiocbp_r_[PADR_(struct oaiocb32 *)];
};
struct freebsd6_freebsd32_aio_write_args {
	char aiocbp_l_[PADL_(struct oaiocb32 *)]; struct oaiocb32 * aiocbp; char aiocbp_r_[PADR_(struct oaiocb32 *)];
};
struct freebsd6_freebsd32_lio_listio_args {
	char mode_l_[PADL_(int)]; int mode; char mode_r_[PADR_(int)];
	char acb_list_l_[PADL_(uint32_t *)]; uint32_t * acb_list; char acb_list_r_[PADR_(uint32_t *)];
	char nent_l_[PADL_(int)]; int nent; char nent_r_[PADR_(int)];
	char sig_l_[PADL_(struct osigevent32 *)]; struct osigevent32 * sig; char sig_r_[PADR_(struct osigevent32 *)];
};
int	freebsd6_freebsd32_pread(struct thread *, struct freebsd6_freebsd32_pread_args *);
int	freebsd6_freebsd32_pwrite(struct thread *, struct freebsd6_freebsd32_pwrite_args *);
int	freebsd6_freebsd32_mmap(struct thread *, struct freebsd6_freebsd32_mmap_args *);
int	freebsd6_freebsd32_lseek(struct thread *, struct freebsd6_freebsd32_lseek_args *);
int	freebsd6_freebsd32_truncate(struct thread *, struct freebsd6_freebsd32_truncate_args *);
int	freebsd6_freebsd32_ftruncate(struct thread *, struct freebsd6_freebsd32_ftruncate_args *);
int	freebsd6_freebsd32_aio_read(struct thread *, struct freebsd6_freebsd32_aio_read_args *);
int	freebsd6_freebsd32_aio_write(struct thread *, struct freebsd6_freebsd32_aio_write_args *);
int	freebsd6_freebsd32_lio_listio(struct thread *, struct freebsd6_freebsd32_lio_listio_args *);

#endif /* COMPAT_FREEBSD6 */


#ifdef COMPAT_FREEBSD7

struct freebsd7_freebsd32___semctl_args {
	char semid_l_[PADL_(int)]; int semid; char semid_r_[PADR_(int)];
	char semnum_l_[PADL_(int)]; int semnum; char semnum_r_[PADR_(int)];
	char cmd_l_[PADL_(int)]; int cmd; char cmd_r_[PADR_(int)];
	char arg_l_[PADL_(union semun_old32 *)]; union semun_old32 * arg; char arg_r_[PADR_(union semun_old32 *)];
};
struct freebsd7_freebsd32_msgctl_args {
	char msqid_l_[PADL_(int)]; int msqid; char msqid_r_[PADR_(int)];
	char cmd_l_[PADL_(int)]; int cmd; char cmd_r_[PADR_(int)];
	char buf_l_[PADL_(struct msqid_ds_old32 *)]; struct msqid_ds_old32 * buf; char buf_r_[PADR_(struct msqid_ds_old32 *)];
};
struct freebsd7_freebsd32_shmctl_args {
	char shmid_l_[PADL_(int)]; int shmid; char shmid_r_[PADR_(int)];
	char cmd_l_[PADL_(int)]; int cmd; char cmd_r_[PADR_(int)];
	char buf_l_[PADL_(struct shmid_ds_old32 *)]; struct shmid_ds_old32 * buf; char buf_r_[PADR_(struct shmid_ds_old32 *)];
};
int	freebsd7_freebsd32___semctl(struct thread *, struct freebsd7_freebsd32___semctl_args *);
int	freebsd7_freebsd32_msgctl(struct thread *, struct freebsd7_freebsd32_msgctl_args *);
int	freebsd7_freebsd32_shmctl(struct thread *, struct freebsd7_freebsd32_shmctl_args *);

#endif /* COMPAT_FREEBSD7 */


#ifdef COMPAT_FREEBSD10

struct freebsd10_freebsd32__umtx_lock_args {
	char umtx_l_[PADL_(struct umtx *)]; struct umtx * umtx; char umtx_r_[PADR_(struct umtx *)];
};
struct freebsd10_freebsd32__umtx_unlock_args {
	char umtx_l_[PADL_(struct umtx *)]; struct umtx * umtx; char umtx_r_[PADR_(struct umtx *)];
};
int	freebsd10_freebsd32__umtx_lock(struct thread *, struct freebsd10_freebsd32__umtx_lock_args *);
int	freebsd10_freebsd32__umtx_unlock(struct thread *, struct freebsd10_freebsd32__umtx_unlock_args *);

#endif /* COMPAT_FREEBSD10 */


#ifdef COMPAT_FREEBSD11

struct freebsd11_freebsd32_stat_args {
	char path_l_[PADL_(const char *)]; const char * path; char path_r_[PADR_(const char *)];
	char ub_l_[PADL_(struct freebsd11_stat32 *)]; struct freebsd11_stat32 * ub; char ub_r_[PADR_(struct freebsd11_stat32 *)];
};
struct freebsd11_freebsd32_fstat_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char sb_l_[PADL_(struct freebsd11_stat32 *)]; struct freebsd11_stat32 * sb; char sb_r_[PADR_(struct freebsd11_stat32 *)];
};
struct freebsd11_freebsd32_lstat_args {
	char path_l_[PADL_(const char *)]; const char * path; char path_r_[PADR_(const char *)];
	char ub_l_[PADL_(struct freebsd11_stat32 *)]; struct freebsd11_stat32 * ub; char ub_r_[PADR_(struct freebsd11_stat32 *)];
};
struct freebsd11_freebsd32_getdirentries_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char buf_l_[PADL_(char *)]; char * buf; char buf_r_[PADR_(char *)];
	char count_l_[PADL_(u_int)]; u_int count; char count_r_[PADR_(u_int)];
	char basep_l_[PADL_(int32_t *)]; int32_t * basep; char basep_r_[PADR_(int32_t *)];
};
struct freebsd11_freebsd32_nstat_args {
	char path_l_[PADL_(const char *)]; const char * path; char path_r_[PADR_(const char *)];
	char ub_l_[PADL_(struct nstat32 *)]; struct nstat32 * ub; char ub_r_[PADR_(struct nstat32 *)];
};
struct freebsd11_freebsd32_nfstat_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char sb_l_[PADL_(struct nstat32 *)]; struct nstat32 * sb; char sb_r_[PADR_(struct nstat32 *)];
};
struct freebsd11_freebsd32_nlstat_args {
	char path_l_[PADL_(const char *)]; const char * path; char path_r_[PADR_(const char *)];
	char ub_l_[PADL_(struct nstat32 *)]; struct nstat32 * ub; char ub_r_[PADR_(struct nstat32 *)];
};
struct freebsd11_freebsd32_fhstat_args {
	char u_fhp_l_[PADL_(const struct fhandle *)]; const struct fhandle * u_fhp; char u_fhp_r_[PADR_(const struct fhandle *)];
	char sb_l_[PADL_(struct freebsd11_stat32 *)]; struct freebsd11_stat32 * sb; char sb_r_[PADR_(struct freebsd11_stat32 *)];
};
struct freebsd11_freebsd32_kevent_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char changelist_l_[PADL_(const struct freebsd11_kevent32 *)]; const struct freebsd11_kevent32 * changelist; char changelist_r_[PADR_(const struct freebsd11_kevent32 *)];
	char nchanges_l_[PADL_(int)]; int nchanges; char nchanges_r_[PADR_(int)];
	char eventlist_l_[PADL_(struct freebsd11_kevent32 *)]; struct freebsd11_kevent32 * eventlist; char eventlist_r_[PADR_(struct freebsd11_kevent32 *)];
	char nevents_l_[PADL_(int)]; int nevents; char nevents_r_[PADR_(int)];
	char timeout_l_[PADL_(const struct timespec32 *)]; const struct timespec32 * timeout; char timeout_r_[PADR_(const struct timespec32 *)];
};
struct freebsd11_freebsd32_getfsstat_args {
	char buf_l_[PADL_(struct freebsd11_statfs *)]; struct freebsd11_statfs * buf; char buf_r_[PADR_(struct freebsd11_statfs *)];
	char bufsize_l_[PADL_(int32_t)]; int32_t bufsize; char bufsize_r_[PADR_(int32_t)];
	char mode_l_[PADL_(int)]; int mode; char mode_r_[PADR_(int)];
};
struct freebsd11_freebsd32_fstatat_args {
	char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
	char path_l_[PADL_(const char *)]; const char * path; char path_r_[PADR_(const char *)];
	char buf_l_[PADL_(struct freebsd11_stat32 *)]; struct freebsd11_stat32 * buf; char buf_r_[PADR_(struct freebsd11_stat32 *)];
	char flag_l_[PADL_(int)]; int flag; char flag_r_[PADR_(int)];
};
int	freebsd11_freebsd32_stat(struct thread *, struct freebsd11_freebsd32_stat_args *);
int	freebsd11_freebsd32_fstat(struct thread *, struct freebsd11_freebsd32_fstat_args *);
int	freebsd11_freebsd32_lstat(struct thread *, struct freebsd11_freebsd32_lstat_args *);
int	freebsd11_freebsd32_getdirentries(struct thread *, struct freebsd11_freebsd32_getdirentries_args *);
int	freebsd11_freebsd32_nstat(struct thread *, struct freebsd11_freebsd32_nstat_args *);
int	freebsd11_freebsd32_nfstat(struct thread *, struct freebsd11_freebsd32_nfstat_args *);
int	freebsd11_freebsd32_nlstat(struct thread *, struct freebsd11_freebsd32_nlstat_args *);
int	freebsd11_freebsd32_fhstat(struct thread *, struct freebsd11_freebsd32_fhstat_args *);
int	freebsd11_freebsd32_kevent(struct thread *, struct freebsd11_freebsd32_kevent_args *);
int	freebsd11_freebsd32_getfsstat(struct thread *, struct freebsd11_freebsd32_getfsstat_args *);
int	freebsd11_freebsd32_fstatat(struct thread *, struct freebsd11_freebsd32_fstatat_args *);

#endif /* COMPAT_FREEBSD11 */


#ifdef COMPAT_FREEBSD12


#endif /* COMPAT_FREEBSD12 */


#ifdef COMPAT_FREEBSD13


#endif /* COMPAT_FREEBSD13 */


#ifdef COMPAT_FREEBSD14


#endif /* COMPAT_FREEBSD14 */

#define	FREEBSD32_SYS_AUE_freebsd32_wait4	AUE_WAIT4
#define	FREEBSD32_SYS_AUE_freebsd4_freebsd32_getfsstat	AUE_GETFSSTAT
#define	FREEBSD32_SYS_AUE_ofreebsd32_lseek	AUE_LSEEK
#define	FREEBSD32_SYS_AUE_freebsd32_ptrace	AUE_PTRACE
#define	FREEBSD32_SYS_AUE_freebsd32_recvmsg	AUE_RECVMSG
#define	FREEBSD32_SYS_AUE_freebsd32_sendmsg	AUE_SENDMSG
#define	FREEBSD32_SYS_AUE_ofreebsd32_stat	AUE_STAT
#define	FREEBSD32_SYS_AUE_ofreebsd32_lstat	AUE_LSTAT
#define	FREEBSD32_SYS_AUE_ofreebsd32_sigaction	AUE_SIGACTION
#define	FREEBSD32_SYS_AUE_freebsd32_sigaltstack	AUE_SIGALTSTACK
#define	FREEBSD32_SYS_AUE_freebsd32_ioctl	AUE_IOCTL
#define	FREEBSD32_SYS_AUE_freebsd32_execve	AUE_EXECVE
#define	FREEBSD32_SYS_AUE_ofreebsd32_fstat	AUE_FSTAT
#define	FREEBSD32_SYS_AUE_ofreebsd32_mmap	AUE_MMAP
#define	FREEBSD32_SYS_AUE_freebsd32_mprotect	AUE_MPROTECT
#define	FREEBSD32_SYS_AUE_freebsd32_setitimer	AUE_SETITIMER
#define	FREEBSD32_SYS_AUE_freebsd32_getitimer	AUE_GETITIMER
#define	FREEBSD32_SYS_AUE_freebsd32_fcntl	AUE_FCNTL
#define	FREEBSD32_SYS_AUE_freebsd32_select	AUE_SELECT
#define	FREEBSD32_SYS_AUE_ofreebsd32_sigreturn	AUE_SIGRETURN
#define	FREEBSD32_SYS_AUE_ofreebsd32_sigvec	AUE_NULL
#define	FREEBSD32_SYS_AUE_ofreebsd32_sigstack	AUE_NULL
#define	FREEBSD32_SYS_AUE_ofreebsd32_recvmsg	AUE_RECVMSG
#define	FREEBSD32_SYS_AUE_ofreebsd32_sendmsg	AUE_SENDMSG
#define	FREEBSD32_SYS_AUE_freebsd32_gettimeofday	AUE_GETTIMEOFDAY
#define	FREEBSD32_SYS_AUE_freebsd32_getrusage	AUE_GETRUSAGE
#define	FREEBSD32_SYS_AUE_freebsd32_readv	AUE_READV
#define	FREEBSD32_SYS_AUE_freebsd32_writev	AUE_WRITEV
#define	FREEBSD32_SYS_AUE_freebsd32_settimeofday	AUE_SETTIMEOFDAY
#define	FREEBSD32_SYS_AUE_ofreebsd32_truncate	AUE_TRUNCATE
#define	FREEBSD32_SYS_AUE_ofreebsd32_ftruncate	AUE_FTRUNCATE
#define	FREEBSD32_SYS_AUE_freebsd32_utimes	AUE_UTIMES
#define	FREEBSD32_SYS_AUE_freebsd32_adjtime	AUE_ADJTIME
#define	FREEBSD32_SYS_AUE_ofreebsd32_sethostid	AUE_SYSCTL
#define	FREEBSD32_SYS_AUE_ofreebsd32_getdirentries	AUE_GETDIRENTRIES
#define	FREEBSD32_SYS_AUE_freebsd4_freebsd32_statfs	AUE_STATFS
#define	FREEBSD32_SYS_AUE_freebsd4_freebsd32_fstatfs	AUE_FSTATFS
#define	FREEBSD32_SYS_AUE_freebsd32_sysarch	AUE_SYSARCH
#define	FREEBSD32_SYS_AUE_freebsd32_semsys	AUE_SEMSYS
#define	FREEBSD32_SYS_AUE_freebsd32_msgsys	AUE_MSGSYS
#define	FREEBSD32_SYS_AUE_freebsd32_shmsys	AUE_SHMSYS
#define	FREEBSD32_SYS_AUE_freebsd6_freebsd32_pread	AUE_PREAD
#define	FREEBSD32_SYS_AUE_freebsd6_freebsd32_pwrite	AUE_PWRITE
#define	FREEBSD32_SYS_AUE_freebsd32_ntp_adjtime	AUE_NTP_ADJTIME
#define	FREEBSD32_SYS_AUE_freebsd11_freebsd32_stat	AUE_STAT
#define	FREEBSD32_SYS_AUE_freebsd11_freebsd32_fstat	AUE_FSTAT
#define	FREEBSD32_SYS_AUE_freebsd11_freebsd32_lstat	AUE_LSTAT
#define	FREEBSD32_SYS_AUE_freebsd11_freebsd32_getdirentries	AUE_GETDIRENTRIES
#define	FREEBSD32_SYS_AUE_freebsd6_freebsd32_mmap	AUE_MMAP
#define	FREEBSD32_SYS_AUE_freebsd6_freebsd32_lseek	AUE_LSEEK
#define	FREEBSD32_SYS_AUE_freebsd6_freebsd32_truncate	AUE_TRUNCATE
#define	FREEBSD32_SYS_AUE_freebsd6_freebsd32_ftruncate	AUE_FTRUNCATE
#define	FREEBSD32_SYS_AUE_freebsd32___sysctl	AUE_SYSCTL
#define	FREEBSD32_SYS_AUE_freebsd32_futimes	AUE_FUTIMES
#define	FREEBSD32_SYS_AUE_freebsd7_freebsd32___semctl	AUE_SEMCTL
#define	FREEBSD32_SYS_AUE_freebsd7_freebsd32_msgctl	AUE_MSGCTL
#define	FREEBSD32_SYS_AUE_freebsd32_msgsnd	AUE_MSGSND
#define	FREEBSD32_SYS_AUE_freebsd32_msgrcv	AUE_MSGRCV
#define	FREEBSD32_SYS_AUE_freebsd7_freebsd32_shmctl	AUE_SHMCTL
#define	FREEBSD32_SYS_AUE_freebsd32_clock_gettime	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_clock_settime	AUE_CLOCK_SETTIME
#define	FREEBSD32_SYS_AUE_freebsd32_clock_getres	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_ktimer_create	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_ktimer_settime	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_ktimer_gettime	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_nanosleep	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_ffclock_setestimate	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_ffclock_getestimate	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_clock_nanosleep	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_clock_getcpuclockid2	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_aio_read	AUE_AIO_READ
#define	FREEBSD32_SYS_AUE_freebsd32_aio_write	AUE_AIO_WRITE
#define	FREEBSD32_SYS_AUE_freebsd32_lio_listio	AUE_LIO_LISTIO
#define	FREEBSD32_SYS_AUE_freebsd32_lutimes	AUE_LUTIMES
#define	FREEBSD32_SYS_AUE_freebsd11_freebsd32_nstat	AUE_STAT
#define	FREEBSD32_SYS_AUE_freebsd11_freebsd32_nfstat	AUE_FSTAT
#define	FREEBSD32_SYS_AUE_freebsd11_freebsd32_nlstat	AUE_LSTAT
#define	FREEBSD32_SYS_AUE_freebsd32_preadv	AUE_PREADV
#define	FREEBSD32_SYS_AUE_freebsd32_pwritev	AUE_PWRITEV
#define	FREEBSD32_SYS_AUE_freebsd4_freebsd32_fhstatfs	AUE_FHSTATFS
#define	FREEBSD32_SYS_AUE_freebsd11_freebsd32_fhstat	AUE_FHSTAT
#define	FREEBSD32_SYS_AUE_freebsd32_modstat	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_kldstat	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_aio_return	AUE_AIO_RETURN
#define	FREEBSD32_SYS_AUE_freebsd32_aio_suspend	AUE_AIO_SUSPEND
#define	FREEBSD32_SYS_AUE_freebsd32_aio_error	AUE_AIO_ERROR
#define	FREEBSD32_SYS_AUE_freebsd6_freebsd32_aio_read	AUE_AIO_READ
#define	FREEBSD32_SYS_AUE_freebsd6_freebsd32_aio_write	AUE_AIO_WRITE
#define	FREEBSD32_SYS_AUE_freebsd6_freebsd32_lio_listio	AUE_LIO_LISTIO
#define	FREEBSD32_SYS_AUE_freebsd32_sched_rr_get_interval	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd4_freebsd32_sendfile	AUE_SENDFILE
#define	FREEBSD32_SYS_AUE_freebsd32_jail	AUE_JAIL
#define	FREEBSD32_SYS_AUE_freebsd4_freebsd32_sigaction	AUE_SIGACTION
#define	FREEBSD32_SYS_AUE_freebsd4_freebsd32_sigreturn	AUE_SIGRETURN
#define	FREEBSD32_SYS_AUE_freebsd32_sigtimedwait	AUE_SIGWAIT
#define	FREEBSD32_SYS_AUE_freebsd32_sigwaitinfo	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_aio_waitcomplete	AUE_AIO_WAITCOMPLETE
#define	FREEBSD32_SYS_AUE_freebsd11_freebsd32_kevent	AUE_KEVENT
#define	FREEBSD32_SYS_AUE_freebsd32_nmount	AUE_NMOUNT
#define	FREEBSD32_SYS_AUE_freebsd32_sendfile	AUE_SENDFILE
#define	FREEBSD32_SYS_AUE_freebsd11_freebsd32_getfsstat	AUE_GETFSSTAT
#define	FREEBSD32_SYS_AUE_freebsd32_ksem_init	AUE_SEMINIT
#define	FREEBSD32_SYS_AUE_freebsd32_ksem_open	AUE_SEMOPEN
#define	FREEBSD32_SYS_AUE_freebsd32_sigaction	AUE_SIGACTION
#define	FREEBSD32_SYS_AUE_freebsd32_sigreturn	AUE_SIGRETURN
#define	FREEBSD32_SYS_AUE_freebsd32_getcontext	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_setcontext	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_swapcontext	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd10_freebsd32__umtx_lock	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd10_freebsd32__umtx_unlock	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_ksem_timedwait	AUE_SEMWAIT
#define	FREEBSD32_SYS_AUE_freebsd32_thr_suspend	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32__umtx_op	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_thr_new	AUE_THR_NEW
#define	FREEBSD32_SYS_AUE_freebsd32_sigqueue	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_kmq_open	AUE_MQ_OPEN
#define	FREEBSD32_SYS_AUE_freebsd32_kmq_setattr	AUE_MQ_SETATTR
#define	FREEBSD32_SYS_AUE_freebsd32_kmq_timedreceive	AUE_MQ_TIMEDRECEIVE
#define	FREEBSD32_SYS_AUE_freebsd32_kmq_timedsend	AUE_MQ_TIMEDSEND
#define	FREEBSD32_SYS_AUE_freebsd32_kmq_notify	AUE_MQ_NOTIFY
#define	FREEBSD32_SYS_AUE_freebsd32_abort2	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_aio_fsync	AUE_AIO_FSYNC
#define	FREEBSD32_SYS_AUE_freebsd32_pread	AUE_PREAD
#define	FREEBSD32_SYS_AUE_freebsd32_pwrite	AUE_PWRITE
#define	FREEBSD32_SYS_AUE_freebsd32_mmap	AUE_MMAP
#define	FREEBSD32_SYS_AUE_freebsd32_lseek	AUE_LSEEK
#define	FREEBSD32_SYS_AUE_freebsd32_truncate	AUE_TRUNCATE
#define	FREEBSD32_SYS_AUE_freebsd32_ftruncate	AUE_FTRUNCATE
#define	FREEBSD32_SYS_AUE_freebsd32_cpuset_setid	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_cpuset_getid	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_cpuset_getaffinity	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_cpuset_setaffinity	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_fexecve	AUE_FEXECVE
#define	FREEBSD32_SYS_AUE_freebsd11_freebsd32_fstatat	AUE_FSTATAT
#define	FREEBSD32_SYS_AUE_freebsd32_futimesat	AUE_FUTIMESAT
#define	FREEBSD32_SYS_AUE_freebsd32_jail_get	AUE_JAIL_GET
#define	FREEBSD32_SYS_AUE_freebsd32_jail_set	AUE_JAIL_SET
#define	FREEBSD32_SYS_AUE_freebsd32___semctl	AUE_SEMCTL
#define	FREEBSD32_SYS_AUE_freebsd32_msgctl	AUE_MSGCTL
#define	FREEBSD32_SYS_AUE_freebsd32_shmctl	AUE_SHMCTL
#define	FREEBSD32_SYS_AUE_freebsd32_pselect	AUE_SELECT
#define	FREEBSD32_SYS_AUE_freebsd32_posix_fallocate	AUE_POSIX_FALLOCATE
#define	FREEBSD32_SYS_AUE_freebsd32_posix_fadvise	AUE_POSIX_FADVISE
#define	FREEBSD32_SYS_AUE_freebsd32_wait6	AUE_WAIT6
#define	FREEBSD32_SYS_AUE_freebsd32_cap_ioctls_limit	AUE_CAP_IOCTLS_LIMIT
#define	FREEBSD32_SYS_AUE_freebsd32_cap_ioctls_get	AUE_CAP_IOCTLS_GET
#define	FREEBSD32_SYS_AUE_freebsd32_aio_mlock	AUE_AIO_MLOCK
#define	FREEBSD32_SYS_AUE_freebsd32_procctl	AUE_PROCCTL
#define	FREEBSD32_SYS_AUE_freebsd32_ppoll	AUE_POLL
#define	FREEBSD32_SYS_AUE_freebsd32_futimens	AUE_FUTIMES
#define	FREEBSD32_SYS_AUE_freebsd32_utimensat	AUE_FUTIMESAT
#define	FREEBSD32_SYS_AUE_freebsd32_fstat	AUE_FSTAT
#define	FREEBSD32_SYS_AUE_freebsd32_fstatat	AUE_FSTATAT
#define	FREEBSD32_SYS_AUE_freebsd32_fhstat	AUE_FHSTAT
#define	FREEBSD32_SYS_AUE_freebsd32_getfsstat	AUE_GETFSSTAT
#define	FREEBSD32_SYS_AUE_freebsd32_mknodat	AUE_MKNODAT
#define	FREEBSD32_SYS_AUE_freebsd32_kevent	AUE_KEVENT
#define	FREEBSD32_SYS_AUE_freebsd32_cpuset_getdomain	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32_cpuset_setdomain	AUE_NULL
#define	FREEBSD32_SYS_AUE_freebsd32___sysctlbyname	AUE_SYSCTL
#define	FREEBSD32_SYS_AUE_freebsd32_aio_writev	AUE_AIO_WRITEV
#define	FREEBSD32_SYS_AUE_freebsd32_aio_readv	AUE_AIO_READV
#define	FREEBSD32_SYS_AUE_freebsd32_timerfd_gettime	AUE_TIMERFD
#define	FREEBSD32_SYS_AUE_freebsd32_timerfd_settime	AUE_TIMERFD
#define	FREEBSD32_SYS_AUE_freebsd32_setcred	AUE_SETCRED

#undef PAD_
#undef PADL_
#undef PADR_

#endif /* !_FREEBSD32_SYSPROTO_H_ */
