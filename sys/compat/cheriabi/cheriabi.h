/*-
 * Copyright (c) 2015 SRI International
 * Copyright (c) 2001 Doug Rabson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 *
 * $FreeBSD$
 */

#ifndef _COMPAT_CHERIABI_CHERIABI_H_
#define _COMPAT_CHERIABI_CHERIABI_H_

#include "opt_ktrace.h"
#include <sys/ktrace.h>
#include <sys/proc.h>

#include <cheri/cheri.h>
#include <cheri/cheric.h>

#define CP(src,dst,fld) do { (dst).fld = (src).fld; } while (0)

/*
 * Take a capability and check that it has the expected pointer
 * properties.  If it does, output a pointer via ptrp and return(0).
 * Otherwise, return an error.
 *
 * Design note: I considered adding a may_be_integer flag to handle
 * cases where the value may be a pointer or an integer, but doing so
 * risks allowing the caller to operate on arbitrary virtual addresses.
 * If we don't know what type of object we're trying to fill, we should
 * not create a spurious pointer. -- BD
 */
static inline int
cheriabi_cap_to_ptr(caddr_t *ptrp, void * __capability cap, size_t reqlen,
    register_t reqperms, int may_be_null)
{
	size_t length, offset;
#ifdef KTRACE
	struct thread *td = curthread;
#endif

	if (!cheri_gettag(cap)) {
		if (!may_be_null)
			return (EFAULT);
		*ptrp = (caddr_t)cap;
		if (*ptrp != NULL)
			return (EFAULT);
	} else {
		if (cheri_getsealed(cap)) {
			SYSERRCAUSE("cap %p is sealed", (void *)cap);
			return (EPROT);
		}

		if ((cheri_getperm(cap) & reqperms) != reqperms) {
			SYSERRCAUSE("cap %p has insufficent perms, missing %lx",
			    (void *)cap, reqperms & ~cheri_getperm(cap));
			return (EPROT);
		}

		length = cheri_getlen(cap);
		offset = cheri_getoffset(cap);
		if (offset >= length) {
			SYSERRCAUSE(
			    "cap %p out of bounds, offset %zu > length %zu",
			    (void *)cap, offset, length);
			return (EPROT);
		}
		length -= offset;
		if (length < reqlen) {
			SYSERRCAUSE("cap %p too short "
			    "(length - offset) %zu < reqlen %zu",
			    (void *)cap, length, reqlen);
			return (EPROT);
		}

		*ptrp = (caddr_t)cap;
	}
	return (0);
}

static inline int
cheriabi_strcap_to_ptr(char **strp, void * __capability cap, int may_be_null)
{

	/*
	 * XXX-BD: place holder implementation checks that the capability
	 * could hold a NUL terminated string empty string.  We can't
	 * check that it does hold one because the caller could change
	 * that out from under us.  Completely safe string handling
	 * requires pushing the length down to the copyinstr().
	 */
	return (cheriabi_cap_to_ptr(strp, cap, 1,
	    CHERI_PERM_LOAD, may_be_null));
}

struct kevent_c {
	__intcap_t		ident;	/* identifier for this event */
	short			filter;	/* filter for event */
	u_short			flags;
	u_int			fflags;
	int64_t			data;
	void * __capability	udata;	/* opaque user data identifier */
};

struct iovec_c {
	void * __capability	iov_base;
	size_t			iov_len;
};

struct msghdr_c {
	void * __capability		msg_name;
	socklen_t			msg_namelen;
	struct iovec_c * __capability	msg_iov;
	int				msg_iovlen;
	void * __capability		msg_control;
	socklen_t			msg_controllen;
	int				msg_flags;
};

struct jail_c {
	uint32_t			version;
	char * __capability		path;
	char * __capability		hostname;
	char * __capability		jailname;
	uint32_t			ip4s;
	uint32_t			ip6s;
	struct in_addr * __capability	ip4;
	struct in6_addr * __capability	ip6;
};

struct sigaction_c {
	void * __capability	sa_u;
	int		sa_flags;
	sigset_t	sa_mask;
};

struct thr_param_c {
	void * __capability		start_func;
	void * __capability		arg;
	char * __capability		stack_base;
	size_t				stack_size;
	char * __capability		tls_base;
	size_t				tls_size;
	long * __capability		child_tid;
	long * __capability		parent_tid;
	int				flags;
	struct rtprio * __capability	rtp;
	void * __capability		ddc;
	void * __capability		spare[2];
};

struct kinfo_proc_c {
	int	ki_structsize;
	int	ki_layout;
	void * __capability	ki_args;		/* struct pargs */
	void * __capability	ki_paddr;		/* struct proc */
	void * __capability	ki_addr;		/* struct user */
	void * __capability	ki_tracep;		/* struct vnode */
	void * __capability	ki_textvp;		/* struct vnode */
	void * __capability	ki_fd;			/* struct filedesc */
	void * __capability	ki_vmspace;		/* struct vmspace */
	void * __capability	ki_wchan;		/* void */
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
	gid_t	ki_groups[KI_NGROUPS];
	vm_size_t ki_size;
	segsz_t ki_rssize;
	segsz_t ki_swrss;
	segsz_t ki_tsize;
	segsz_t ki_dsize;
	segsz_t ki_ssize;
	u_short	ki_xstat;
	u_short	ki_acflag;
	fixpt_t	ki_pctcpu;
	u_int	ki_estcpu;
	u_int	ki_slptime;
	u_int	ki_swtime;
	u_int	ki_cow;
	u_int64_t ki_runtime;
	struct	timeval ki_start;
	struct	timeval ki_childtime;
	long	ki_flag;
	long	ki_kiflag;
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
	/*
	 * When adding new variables, take space for char-strings from the
	 * front of ki_sparestrings, and ints from the end of ki_spareints.
	 * That way the spare room from both arrays will remain contiguous.
	 */
	char	ki_sparestrings[50];
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
	struct	rusage ki_rusage;
	/* XXX - most fields in ki_rusage_ch are not (yet) filled in */
	struct	rusage ki_rusage_ch;
	void * __capability	ki_pcb;			/* struct pcb */
	void * __capability	ki_kstack;		/* void	*/
	void * __capability	ki_udata;		/* void	*/
	void * __capability	ki_tdaddr;		/* struct thread  */
	void * __capability	ki_spareptrs[KI_NSPARE_PTR];	/* void */
	long	ki_sparelongs[KI_NSPARE_LONG];
	long	ki_sflag;
	long	ki_tdflags;
};

struct mac_c {
	size_t		m_buflen;
	char * __capability	m_string;
};

struct kld_sym_lookup_c {
	int		version; /* set to sizeof(struct kld_sym_lookup_c) */
	char * __capability symname; /* Symbol name we are looking up */
	u_long		symvalue;
	size_t		symsize;
};

struct sf_hdtr_c {
	struct iovec_c * __capability	headers;
	int				hdr_cnt;
	struct iovec_c * __capability	trailers;
	int				trl_cnt;
};

struct procctl_reaper_pids_c {
	u_int						rp_count;
	u_int						rp_pad0[15];
	struct procctl_reaper_pidinfo * __capability	rp_pids;
};

union semun_c {
	int val;
	/* struct semid_ds *buf; */
	/* unsigned short  *array; */
	void * __capability ptr;
};

#include <sys/ipc.h>
#include <sys/msg.h>

struct msqid_ds_c {
	struct ipc_perm			msg_perm;
	struct msg_c * __capability	msg_first;
	struct msg_c * __capability	msg_last;
	msglen_t			msg_cbytes;
	msgqnum_t			msg_qnum;
	msglen_t			msg_qbytes;
	pid_t				msg_lspid;
	pid_t  				msg_lrpid;
	time_t				msg_stime;
	time_t 				msg_rtime;
	time_t				msg_ctime;
};

#endif /* !_COMPAT_CHERIABI_CHERIABI_H_ */
