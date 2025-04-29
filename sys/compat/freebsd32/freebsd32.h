/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2001 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _COMPAT_FREEBSD32_FREEBSD32_H_
#define _COMPAT_FREEBSD32_FREEBSD32_H_

#include <sys/abi_compat.h>
#include <sys/procfs.h>
#include <sys/socket.h>
#include <sys/user.h>
#include <sys/_ffcounter.h>

/*
 * i386 is the only arch with a 32-bit time_t
 */
#ifdef __amd64__
typedef	int32_t	time32_t;
#else
typedef	int64_t	time32_t;
#endif

struct timeval32 {
	time32_t tv_sec;
	int32_t tv_usec;
};

struct timespec32 {
	time32_t tv_sec;
	int32_t tv_nsec;
};

struct itimerspec32 {
	struct timespec32  it_interval;
	struct timespec32  it_value;
};

struct bintime32 {
	time32_t sec;
	uint32_t frac[2];
};

struct ffclock_estimate32 {
	struct bintime32 update_time;
	ffcounter update_ffcount;
	ffcounter leapsec_next;
	uint64_t period;
	uint32_t errb_abs;
	uint32_t errb_rate;
	uint32_t status;
	int16_t leapsec_total;
	int8_t leapsec;
	int8_t _pad;
}
#if defined(__amd64__)
__attribute__((packed))
#endif
;
#if defined(__amd64__)
_Static_assert(sizeof(struct ffclock_estimate32) == 52, "ffclock_estimate32 size");
#else
_Static_assert(sizeof(struct ffclock_estimate32) == 56, "ffclock_estimate32 size");
#endif

struct rusage32 {
	struct timeval32 ru_utime;
	struct timeval32 ru_stime;
	int32_t	ru_maxrss;
	int32_t	ru_ixrss;
	int32_t	ru_idrss;
	int32_t	ru_isrss;
	int32_t	ru_minflt;
	int32_t	ru_majflt;
	int32_t	ru_nswap;
	int32_t	ru_inblock;
	int32_t	ru_oublock;
	int32_t	ru_msgsnd;
	int32_t	ru_msgrcv;
	int32_t	ru_nsignals;
	int32_t	ru_nvcsw;
	int32_t	ru_nivcsw;
};

struct __wrusage32 {
	struct rusage32	wru_self;
	struct rusage32 wru_children;
};

struct itimerval32 {
	struct timeval32 it_interval;
	struct timeval32 it_value;
};

struct umtx_time32 {
	struct	timespec32	_timeout;
	uint32_t		_flags;
	uint32_t		_clockid;
};

struct umtx_robust_lists_params_compat32 {
	uint32_t	robust_list_offset;
	uint32_t	robust_priv_list_offset;
	uint32_t	robust_inact_offset;
};

struct umutex32 {
	volatile __lwpid_t	m_owner;	/* Owner of the mutex */
	__uint32_t		m_flags;	/* Flags of the mutex */
	__uint32_t		m_ceilings[2];	/* Priority protect ceiling */
	__uint32_t		m_rb_lnk;	/* Robust linkage */
	__uint32_t		m_pad;
	__uint32_t		m_spare[2];
};

#define FREEBSD4_OMFSNAMELEN	16
#define FREEBSD4_OMNAMELEN	(88 - 2 * sizeof(int32_t))

/* 4.x version */
struct ostatfs32 {
	int32_t	f_spare2;
	int32_t	f_bsize;
	int32_t	f_iosize;
	int32_t	f_blocks;
	int32_t	f_bfree;
	int32_t	f_bavail;
	int32_t	f_files;
	int32_t	f_ffree;
	fsid_t	f_fsid;
	uid_t	f_owner;
	int32_t	f_type;
	int32_t	f_flags;
	int32_t	f_syncwrites;
	int32_t	f_asyncwrites;
	char	f_fstypename[FREEBSD4_OMFSNAMELEN];
	char	f_mntonname[FREEBSD4_OMNAMELEN];
	int32_t	f_syncreads;
	int32_t	f_asyncreads;
	int16_t	f_spares1;
	char	f_mntfromname[FREEBSD4_OMNAMELEN];
	int16_t	f_spares2 __packed;
	int32_t f_spare[2];
};

struct nstat32 {
	__uint32_t st_dev;		/* inode's device */
	__uint32_t st_ino;		/* inode's number */
	__uint32_t st_mode;		/* inode protection mode */
	__uint32_t st_nlink;		/* number of hard links */
	uid_t	  st_uid;		/* user ID of the file's owner */
	gid_t	  st_gid;		/* group ID of the file's group */
	__uint32_t st_rdev;		/* device type */
	struct	timespec32 st_atim;	/* time of last access */
	struct	timespec32 st_mtim;	/* time of last data modification */
	struct	timespec32 st_ctim;	/* time of last file status change */
	off_t	  st_size;		/* file size, in bytes */
	blkcnt_t st_blocks;		/* blocks allocated for file */
	blksize_t st_blksize;		/* optimal blocksize for I/O */
	fflags_t  st_flags;		/* user defined flags for file */
	__uint32_t st_gen;		/* file generation number */
	struct timespec32 st_birthtim;	/* time of file creation */
	/*
	 * See comment in the definition of struct freebsd11_stat
	 * in sys/stat.h about the following padding.
	 */
	unsigned int :(8 / 2) * (16 - (int)sizeof(struct timespec32));
	unsigned int :(8 / 2) * (16 - (int)sizeof(struct timespec32));
};

struct iovec32 {
	uint32_t iov_base;
	int	iov_len;
};

struct msghdr32 {
	uint32_t	 msg_name;
	socklen_t	 msg_namelen;
	uint32_t	 msg_iov;
	int		 msg_iovlen;
	uint32_t	 msg_control;
	socklen_t	 msg_controllen;
	int		 msg_flags;
};

#if defined(__amd64__)
#define	__STAT32_TIME_T_EXT	1
#endif

struct stat32 {
	dev_t st_dev;
	ino_t st_ino;
	nlink_t st_nlink;
	mode_t	st_mode;
	uint16_t st_bsdflags;
	uid_t	st_uid;
	gid_t	st_gid;
	uint32_t st_padding1;
	dev_t st_rdev;
#ifdef	__STAT32_TIME_T_EXT
	__int32_t st_atim_ext;
#endif
	struct timespec32 st_atim;
#ifdef	__STAT32_TIME_T_EXT
	__int32_t st_mtim_ext;
#endif
	struct timespec32 st_mtim;
#ifdef	__STAT32_TIME_T_EXT
	__int32_t st_ctim_ext;
#endif
	struct timespec32 st_ctim;
#ifdef	__STAT32_TIME_T_EXT
	__int32_t st_btim_ext;
#endif
	struct timespec32 st_birthtim;
	off_t	st_size;
	int64_t	st_blocks;
	uint32_t st_blksize;
	uint32_t st_flags;
	uint64_t st_gen;
	uint64_t st_filerev;
	uint64_t st_spare[9];
};
struct freebsd11_stat32 {
	uint32_t st_dev;
	uint32_t st_ino;
	mode_t	st_mode;
	uint16_t st_nlink;
	uid_t	st_uid;
	gid_t	st_gid;
	uint32_t st_rdev;
	struct timespec32 st_atim;
	struct timespec32 st_mtim;
	struct timespec32 st_ctim;
	off_t	st_size;
	int64_t	st_blocks;
	uint32_t st_blksize;
	uint32_t st_flags;
	uint32_t st_gen;
	int32_t	st_lspare;
	struct timespec32 st_birthtim;
	unsigned int :(8 / 2) * (16 - (int)sizeof(struct timespec32));
	unsigned int :(8 / 2) * (16 - (int)sizeof(struct timespec32));
};

struct ostat32 {
	__uint16_t st_dev;
	__uint32_t st_ino;
	mode_t	st_mode;
	__uint16_t st_nlink;
	__uint16_t st_uid;
	__uint16_t st_gid;
	__uint16_t st_rdev;
	__int32_t st_size;
	struct timespec32 st_atim;
	struct timespec32 st_mtim;
	struct timespec32 st_ctim;
	__int32_t st_blksize;
	__int32_t st_blocks;
	uint32_t st_flags;
	__uint32_t st_gen;
};

struct jail32_v0 {
	uint32_t	version;
	uint32_t	path;
	uint32_t	hostname;
	uint32_t	ip_number;
};

struct jail32 {
	uint32_t	version;
	uint32_t	path;
	uint32_t	hostname;
	uint32_t	jailname;
	uint32_t	ip4s;
	uint32_t	ip6s;
	uint32_t	ip4;
	uint32_t	ip6;
};

struct sigaction32 {
	uint32_t	sa_u;
	int		sa_flags;
	sigset_t	sa_mask;
};

struct thr_param32 {
	uint32_t start_func;
	uint32_t arg;
	uint32_t stack_base;
	uint32_t stack_size;
	uint32_t tls_base;
	uint32_t tls_size;
	uint32_t child_tid;
	uint32_t parent_tid;
	int32_t	 flags;
	uint32_t rtp;
	uint32_t spare[3];
};

struct i386_ldt_args32 {
	uint32_t start;
	uint32_t descs;
	uint32_t num;
};

struct mq_attr32 {
	int	mq_flags;
	int	mq_maxmsg;
	int	mq_msgsize;
	int	mq_curmsgs;
	int	__reserved[4];
};

struct kinfo_proc32 {
	int	ki_structsize;
	int	ki_layout;
	uint32_t ki_args;
	uint32_t ki_paddr;
	uint32_t ki_addr;
	uint32_t ki_tracep;
	uint32_t ki_textvp;
	uint32_t ki_fd;
	uint32_t ki_vmspace;
	uint32_t ki_wchan;
	pid_t	ki_pid;
	pid_t	ki_ppid;
	pid_t	ki_pgid;
	pid_t	ki_tpgid;
	pid_t	ki_sid;
	pid_t	ki_tsid;
	short	ki_jobc;
	short	ki_spare_short1;
	uint32_t ki_tdev_freebsd11;
	sigset_t ki_siglist;
	sigset_t ki_sigmask;
	sigset_t ki_sigignore;
	sigset_t ki_sigcatch;
	uid_t	ki_uid;
	uid_t	ki_ruid;
	uid_t	ki_svuid;
	gid_t	ki_rgid;
	gid_t	ki_svgid;
	short	ki_ngroups;
	short	ki_spare_short2;
	gid_t 	ki_groups[KI_NGROUPS];
	uint32_t ki_size;
	int32_t ki_rssize;
	int32_t ki_swrss;
	int32_t ki_tsize;
	int32_t ki_dsize;
	int32_t ki_ssize;
	u_short	ki_xstat;
	u_short	ki_acflag;
	fixpt_t	ki_pctcpu;
	u_int	ki_estcpu;
	u_int	ki_slptime;
	u_int	ki_swtime;
	u_int	ki_cow;
	uint64_t ki_runtime;
	struct	timeval32 ki_start;
	struct	timeval32 ki_childtime;
	int	ki_flag;
	int	ki_kiflag;
	int	ki_traceflag;
	char	ki_stat;
	signed char ki_nice;
	char	ki_lock;
	char	ki_rqindex;
	u_char	ki_oncpu_old;
	u_char	ki_lastcpu_old;
	char	ki_tdname[TDNAMLEN+1];
	char	ki_wmesg[WMESGLEN+1];
	char	ki_login[LOGNAMELEN+1];
	char	ki_lockname[LOCKNAMELEN+1];
	char	ki_comm[COMMLEN+1];
	char	ki_emul[KI_EMULNAMELEN+1];
	char	ki_loginclass[LOGINCLASSLEN+1];
	char	ki_moretdname[MAXCOMLEN-TDNAMLEN+1];
	char	ki_sparestrings[46];
	int	ki_spareints[KI_NSPARE_INT];
	uint64_t ki_tdev;
	int	ki_oncpu;
	int	ki_lastcpu;
	int	ki_tracer;
	int	ki_flag2;
	int	ki_fibnum;
	u_int	ki_cr_flags;
	int	ki_jid;
	int	ki_numthreads;
	lwpid_t	ki_tid;
	struct	priority ki_pri;
	struct	rusage32 ki_rusage;
	struct	rusage32 ki_rusage_ch;
	uint32_t ki_pcb;
	uint32_t ki_kstack;
	uint32_t ki_udata;
	uint32_t ki_tdaddr;
	uint32_t ki_spareptrs[KI_NSPARE_PTR];	/* spare room for growth */
	int	ki_sparelongs[KI_NSPARE_LONG];
	int	ki_sflag;
	int	ki_tdflags;
};

struct kinfo_sigtramp32 {
	uint32_t ksigtramp_start;
	uint32_t ksigtramp_end;
	uint32_t ksigtramp_spare[4];
};

struct kinfo_vm_layout32 {
	uint32_t	kvm_min_user_addr;
	uint32_t	kvm_max_user_addr;
	uint32_t	kvm_text_addr;
	uint32_t	kvm_text_size;
	uint32_t	kvm_data_addr;
	uint32_t	kvm_data_size;
	uint32_t	kvm_stack_addr;
	uint32_t	kvm_stack_size;
	int		kvm_map_flags;
	uint32_t	kvm_shp_addr;
	uint32_t	kvm_shp_size;
	uint32_t	kvm_spare[12];
};

#if defined(_WANT_KEVENT32) || defined(_KERNEL)
struct kinfo_knote32 {
	int		knt_kq_fd;
	struct kevent32	knt_event;
	int		knt_status;
	int		knt_extdata;
	uint32_t	knt_spare0[8];
	union {
		struct {
			int		knt_vnode_type;
			uint32_t	knt_vnode_fsid[2];
			uint32_t	knt_vnode_fileid[2];
			char		knt_vnode_fullpath[PATH_MAX];
		} knt_vnode;
		struct {
			uint32_t	knt_pipe_ino[2];
		} knt_pipe;
	};
};
#endif

struct kld_file_stat_1_32 {
	int	version;	/* set to sizeof(struct kld_file_stat_1) */
	char	name[MAXPATHLEN];
	int	refs;
	int	id;
	uint32_t address;	/* load address */
	uint32_t size;		/* size in bytes */
};

struct kld_file_stat32 {
	int	version;	/* set to sizeof(struct kld_file_stat) */
	char	name[MAXPATHLEN];
	int	refs;
	int	id;
	uint32_t address;	/* load address */
	uint32_t size;		/* size in bytes */
	char	pathname[MAXPATHLEN];
};

struct procctl_reaper_pids32 {
	u_int	rp_count;
	u_int	rp_pad0[15];
	uint32_t rp_pids;
};

struct timex32 {
	unsigned int modes;
	int32_t	offset;
	int32_t	freq;
	int32_t	maxerror;
	int32_t	esterror;
	int	status;
	int32_t	constant;
	int32_t	precision;
	int32_t	tolerance;
	int32_t	ppsfreq;
	int32_t	jitter;
	int	shift;
	int32_t	stabil;
	int32_t	jitcnt;
	int32_t	calcnt;
	int32_t	errcnt;
	int32_t	stbcnt;
};

struct ptrace_sc_ret32 {
	uint32_t	sr_retval[2];
	int		sr_error;
};

struct ptrace_coredump32 {
	int		pc_fd;
	uint32_t	pc_flags;
	uint32_t	pc_limit1, pc_limit2;
};

struct ptrace_sc_remote32 {
	struct ptrace_sc_ret32 pscr_ret;
	u_int		pscr_syscall;
	u_int		pscr_nargs;
	uint32_t	pscr_args;
};

#endif /* !_COMPAT_FREEBSD32_FREEBSD32_H_ */
