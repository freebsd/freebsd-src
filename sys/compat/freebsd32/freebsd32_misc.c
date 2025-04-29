/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2002 Doug Rabson
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

#include <sys/cdefs.h>
#include "opt_ffclock.h"
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ktrace.h"

#define __ELF_WORD_SIZE 32

#ifdef COMPAT_FREEBSD11
#define	_WANT_FREEBSD11_KEVENT
#endif

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/capsicum.h>
#include <sys/clock.h>
#include <sys/exec.h>
#include <sys/fcntl.h>
#include <sys/filedesc.h>
#include <sys/imgact.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/file.h>		/* Must come after sys/malloc.h */
#include <sys/imgact.h>
#include <sys/mbuf.h>
#include <sys/mman.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/procctl.h>
#include <sys/ptrace.h>
#include <sys/reboot.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/selinfo.h>
#include <sys/eventvar.h>	/* Must come after sys/selinfo.h */
#include <sys/pipe.h>		/* Must come after sys/selinfo.h */
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/thr.h>
#include <sys/timerfd.h>
#include <sys/timex.h>
#include <sys/unistd.h>
#include <sys/ucontext.h>
#include <sys/ucred.h>
#include <sys/vnode.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/timeffc.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#ifdef INET
#include <netinet/in.h>
#endif

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>

#include <machine/cpu.h>
#include <machine/elf.h>
#ifdef __amd64__
#include <machine/md_var.h>
#endif

#include <security/audit/audit.h>
#include <security/mac/mac_syscalls.h>

#include <compat/freebsd32/freebsd32_util.h>
#include <compat/freebsd32/freebsd32.h>
#include <compat/freebsd32/freebsd32_ipc.h>
#include <compat/freebsd32/freebsd32_misc.h>
#include <compat/freebsd32/freebsd32_signal.h>
#include <compat/freebsd32/freebsd32_proto.h>

int compat_freebsd_32bit = 1;

static void
register_compat32_feature(void *arg)
{
	if (!compat_freebsd_32bit)
		return;

	FEATURE_ADD("compat_freebsd32", "Compatible with 32-bit FreeBSD");
	FEATURE_ADD("compat_freebsd_32bit",
	    "Compatible with 32-bit FreeBSD (legacy feature name)");
}
SYSINIT(freebsd32, SI_SUB_EXEC, SI_ORDER_ANY, register_compat32_feature,
    NULL);

struct ptrace_io_desc32 {
	int		piod_op;
	uint32_t	piod_offs;
	uint32_t	piod_addr;
	uint32_t	piod_len;
};

struct ptrace_vm_entry32 {
	int		pve_entry;
	int		pve_timestamp;
	uint32_t	pve_start;
	uint32_t	pve_end;
	uint32_t	pve_offset;
	u_int		pve_prot;
	u_int		pve_pathlen;
	int32_t		pve_fileid;
	u_int		pve_fsid;
	uint32_t	pve_path;
};

#ifdef __amd64__
CTASSERT(sizeof(struct timeval32) == 8);
CTASSERT(sizeof(struct timespec32) == 8);
CTASSERT(sizeof(struct itimerval32) == 16);
CTASSERT(sizeof(struct bintime32) == 12);
#else
CTASSERT(sizeof(struct timeval32) == 16);
CTASSERT(sizeof(struct timespec32) == 16);
CTASSERT(sizeof(struct itimerval32) == 32);
CTASSERT(sizeof(struct bintime32) == 16);
#endif
CTASSERT(sizeof(struct ostatfs32) == 256);
#ifdef __amd64__
CTASSERT(sizeof(struct rusage32) == 72);
#else
CTASSERT(sizeof(struct rusage32) == 88);
#endif
CTASSERT(sizeof(struct sigaltstack32) == 12);
#ifdef __amd64__
CTASSERT(sizeof(struct kevent32) == 56);
#else
CTASSERT(sizeof(struct kevent32) == 64);
#endif
CTASSERT(sizeof(struct iovec32) == 8);
CTASSERT(sizeof(struct msghdr32) == 28);
#ifdef __amd64__
CTASSERT(sizeof(struct stat32) == 208);
CTASSERT(sizeof(struct freebsd11_stat32) == 96);
#else
CTASSERT(sizeof(struct stat32) == 224);
CTASSERT(sizeof(struct freebsd11_stat32) == 120);
#endif
CTASSERT(sizeof(struct sigaction32) == 24);

static int freebsd32_kevent_copyout(void *arg, struct kevent *kevp, int count);
static int freebsd32_kevent_copyin(void *arg, struct kevent *kevp, int count);
static int freebsd32_user_clock_nanosleep(struct thread *td, clockid_t clock_id,
    int flags, const struct timespec32 *ua_rqtp, struct timespec32 *ua_rmtp);

void
freebsd32_rusage_out(const struct rusage *s, struct rusage32 *s32)
{

	TV_CP(*s, *s32, ru_utime);
	TV_CP(*s, *s32, ru_stime);
	CP(*s, *s32, ru_maxrss);
	CP(*s, *s32, ru_ixrss);
	CP(*s, *s32, ru_idrss);
	CP(*s, *s32, ru_isrss);
	CP(*s, *s32, ru_minflt);
	CP(*s, *s32, ru_majflt);
	CP(*s, *s32, ru_nswap);
	CP(*s, *s32, ru_inblock);
	CP(*s, *s32, ru_oublock);
	CP(*s, *s32, ru_msgsnd);
	CP(*s, *s32, ru_msgrcv);
	CP(*s, *s32, ru_nsignals);
	CP(*s, *s32, ru_nvcsw);
	CP(*s, *s32, ru_nivcsw);
}

int
freebsd32_wait4(struct thread *td, struct freebsd32_wait4_args *uap)
{
	int error, status;
	struct rusage32 ru32;
	struct rusage ru, *rup;

	if (uap->rusage != NULL)
		rup = &ru;
	else
		rup = NULL;
	error = kern_wait(td, uap->pid, &status, uap->options, rup);
	if (error)
		return (error);
	if (uap->status != NULL)
		error = copyout(&status, uap->status, sizeof(status));
	if (uap->rusage != NULL && error == 0) {
		freebsd32_rusage_out(&ru, &ru32);
		error = copyout(&ru32, uap->rusage, sizeof(ru32));
	}
	return (error);
}

int
freebsd32_wait6(struct thread *td, struct freebsd32_wait6_args *uap)
{
	struct __wrusage32 wru32;
	struct __wrusage wru, *wrup;
	struct __siginfo32 si32;
	struct __siginfo si, *sip;
	int error, status;

	if (uap->wrusage != NULL)
		wrup = &wru;
	else
		wrup = NULL;
	if (uap->info != NULL) {
		sip = &si;
		bzero(sip, sizeof(*sip));
	} else
		sip = NULL;
	error = kern_wait6(td, uap->idtype, PAIR32TO64(id_t, uap->id),
	    &status, uap->options, wrup, sip);
	if (error != 0)
		return (error);
	if (uap->status != NULL)
		error = copyout(&status, uap->status, sizeof(status));
	if (uap->wrusage != NULL && error == 0) {
		freebsd32_rusage_out(&wru.wru_self, &wru32.wru_self);
		freebsd32_rusage_out(&wru.wru_children, &wru32.wru_children);
		error = copyout(&wru32, uap->wrusage, sizeof(wru32));
	}
	if (uap->info != NULL && error == 0) {
		siginfo_to_siginfo32 (&si, &si32);
		error = copyout(&si32, uap->info, sizeof(si32));
	}
	return (error);
}

#ifdef COMPAT_FREEBSD4
static void
copy_statfs(struct statfs *in, struct ostatfs32 *out)
{

	statfs_scale_blocks(in, INT32_MAX);
	bzero(out, sizeof(*out));
	CP(*in, *out, f_bsize);
	out->f_iosize = MIN(in->f_iosize, INT32_MAX);
	CP(*in, *out, f_blocks);
	CP(*in, *out, f_bfree);
	CP(*in, *out, f_bavail);
	out->f_files = MIN(in->f_files, INT32_MAX);
	out->f_ffree = MIN(in->f_ffree, INT32_MAX);
	CP(*in, *out, f_fsid);
	CP(*in, *out, f_owner);
	CP(*in, *out, f_type);
	CP(*in, *out, f_flags);
	out->f_syncwrites = MIN(in->f_syncwrites, INT32_MAX);
	out->f_asyncwrites = MIN(in->f_asyncwrites, INT32_MAX);
	strlcpy(out->f_fstypename,
	      in->f_fstypename, MFSNAMELEN);
	strlcpy(out->f_mntonname,
	      in->f_mntonname, min(MNAMELEN, FREEBSD4_OMNAMELEN));
	out->f_syncreads = MIN(in->f_syncreads, INT32_MAX);
	out->f_asyncreads = MIN(in->f_asyncreads, INT32_MAX);
	strlcpy(out->f_mntfromname,
	      in->f_mntfromname, min(MNAMELEN, FREEBSD4_OMNAMELEN));
}
#endif

int
freebsd32_getfsstat(struct thread *td, struct freebsd32_getfsstat_args *uap)
{
	size_t count;
	int error;

	if (uap->bufsize < 0 || uap->bufsize > SIZE_MAX)
		return (EINVAL);
	error = kern_getfsstat(td, &uap->buf, uap->bufsize, &count,
	    UIO_USERSPACE, uap->mode);
	if (error == 0)
		td->td_retval[0] = count;
	return (error);
}

#ifdef COMPAT_FREEBSD4
int
freebsd4_freebsd32_getfsstat(struct thread *td,
    struct freebsd4_freebsd32_getfsstat_args *uap)
{
	struct statfs *buf, *sp;
	struct ostatfs32 stat32;
	size_t count, size, copycount;
	int error;

	count = uap->bufsize / sizeof(struct ostatfs32);
	size = count * sizeof(struct statfs);
	error = kern_getfsstat(td, &buf, size, &count, UIO_SYSSPACE, uap->mode);
	if (size > 0) {
		sp = buf;
		copycount = count;
		while (copycount > 0 && error == 0) {
			copy_statfs(sp, &stat32);
			error = copyout(&stat32, uap->buf, sizeof(stat32));
			sp++;
			uap->buf++;
			copycount--;
		}
		free(buf, M_STATFS);
	}
	if (error == 0)
		td->td_retval[0] = count;
	return (error);
}
#endif

#ifdef COMPAT_FREEBSD11
int
freebsd11_freebsd32_getfsstat(struct thread *td,
    struct freebsd11_freebsd32_getfsstat_args *uap)
{
	return(kern_freebsd11_getfsstat(td, uap->buf, uap->bufsize,
	    uap->mode));
}
#endif

int
freebsd32_sigaltstack(struct thread *td,
		      struct freebsd32_sigaltstack_args *uap)
{
	struct sigaltstack32 s32;
	struct sigaltstack ss, oss, *ssp;
	int error;

	if (uap->ss != NULL) {
		error = copyin(uap->ss, &s32, sizeof(s32));
		if (error)
			return (error);
		PTRIN_CP(s32, ss, ss_sp);
		CP(s32, ss, ss_size);
		CP(s32, ss, ss_flags);
		ssp = &ss;
	} else
		ssp = NULL;
	error = kern_sigaltstack(td, ssp, &oss);
	if (error == 0 && uap->oss != NULL) {
		PTROUT_CP(oss, s32, ss_sp);
		CP(oss, s32, ss_size);
		CP(oss, s32, ss_flags);
		error = copyout(&s32, uap->oss, sizeof(s32));
	}
	return (error);
}

/*
 * Custom version of exec_copyin_args() so that we can translate
 * the pointers.
 */
int
freebsd32_exec_copyin_args(struct image_args *args, const char *fname,
    uint32_t *argv, uint32_t *envv)
{
	char *argp, *envp;
	uint32_t *p32, arg;
	int error;

	bzero(args, sizeof(*args));
	if (argv == NULL)
		return (EFAULT);

	/*
	 * Allocate demand-paged memory for the file name, argument, and
	 * environment strings.
	 */
	error = exec_alloc_args(args);
	if (error != 0)
		return (error);

	/*
	 * Copy the file name.
	 */
	error = exec_args_add_fname(args, fname, UIO_USERSPACE);
	if (error != 0)
		goto err_exit;

	/*
	 * extract arguments first
	 */
	p32 = argv;
	for (;;) {
		error = copyin(p32++, &arg, sizeof(arg));
		if (error)
			goto err_exit;
		if (arg == 0)
			break;
		argp = PTRIN(arg);
		error = exec_args_add_arg(args, argp, UIO_USERSPACE);
		if (error != 0)
			goto err_exit;
	}

	/*
	 * extract environment strings
	 */
	if (envv) {
		p32 = envv;
		for (;;) {
			error = copyin(p32++, &arg, sizeof(arg));
			if (error)
				goto err_exit;
			if (arg == 0)
				break;
			envp = PTRIN(arg);
			error = exec_args_add_env(args, envp, UIO_USERSPACE);
			if (error != 0)
				goto err_exit;
		}
	}

	return (0);

err_exit:
	exec_free_args(args);
	return (error);
}

int
freebsd32_execve(struct thread *td, struct freebsd32_execve_args *uap)
{
	struct image_args eargs;
	struct vmspace *oldvmspace;
	int error;

	error = pre_execve(td, &oldvmspace);
	if (error != 0)
		return (error);
	error = freebsd32_exec_copyin_args(&eargs, uap->fname, uap->argv,
	    uap->envv);
	if (error == 0)
		error = kern_execve(td, &eargs, NULL, oldvmspace);
	post_execve(td, error, oldvmspace);
	AUDIT_SYSCALL_EXIT(error == EJUSTRETURN ? 0 : error, td);
	return (error);
}

int
freebsd32_fexecve(struct thread *td, struct freebsd32_fexecve_args *uap)
{
	struct image_args eargs;
	struct vmspace *oldvmspace;
	int error;

	error = pre_execve(td, &oldvmspace);
	if (error != 0)
		return (error);
	error = freebsd32_exec_copyin_args(&eargs, NULL, uap->argv, uap->envv);
	if (error == 0) {
		eargs.fd = uap->fd;
		error = kern_execve(td, &eargs, NULL, oldvmspace);
	}
	post_execve(td, error, oldvmspace);
	AUDIT_SYSCALL_EXIT(error == EJUSTRETURN ? 0 : error, td);
	return (error);
}

int
freebsd32_mknodat(struct thread *td, struct freebsd32_mknodat_args *uap)
{

	return (kern_mknodat(td, uap->fd, uap->path, UIO_USERSPACE,
	    uap->mode, PAIR32TO64(dev_t, uap->dev)));
}

int
freebsd32_mprotect(struct thread *td, struct freebsd32_mprotect_args *uap)
{
	int prot;

	prot = uap->prot;
#if defined(__amd64__)
	if (i386_read_exec && (prot & PROT_READ) != 0)
		prot |= PROT_EXEC;
#endif
	return (kern_mprotect(td, (uintptr_t)PTRIN(uap->addr), uap->len,
	    prot, 0));
}

int
freebsd32_mmap(struct thread *td, struct freebsd32_mmap_args *uap)
{
	int prot;

	prot = uap->prot;
#if defined(__amd64__)
	if (i386_read_exec && (prot & PROT_READ))
		prot |= PROT_EXEC;
#endif

	return (kern_mmap(td, &(struct mmap_req){
		.mr_hint = (uintptr_t)uap->addr,
		.mr_len = uap->len,
		.mr_prot = prot,
		.mr_flags = uap->flags,
		.mr_fd = uap->fd,
		.mr_pos = PAIR32TO64(off_t, uap->pos),
	    }));
}

#ifdef COMPAT_FREEBSD6
int
freebsd6_freebsd32_mmap(struct thread *td,
    struct freebsd6_freebsd32_mmap_args *uap)
{
	int prot;

	prot = uap->prot;
#if defined(__amd64__)
	if (i386_read_exec && (prot & PROT_READ))
		prot |= PROT_EXEC;
#endif

	return (kern_mmap(td, &(struct mmap_req){
		.mr_hint = (uintptr_t)uap->addr,
		.mr_len = uap->len,
		.mr_prot = prot,
		.mr_flags = uap->flags,
		.mr_fd = uap->fd,
		.mr_pos = PAIR32TO64(off_t, uap->pos),
	    }));
}
#endif

#ifdef COMPAT_43
int
ofreebsd32_mmap(struct thread *td, struct ofreebsd32_mmap_args *uap)
{
	return (kern_ommap(td, (uintptr_t)uap->addr, uap->len, uap->prot,
	    uap->flags, uap->fd, uap->pos));
}
#endif

int
freebsd32_setitimer(struct thread *td, struct freebsd32_setitimer_args *uap)
{
	struct itimerval itv, oitv, *itvp;	
	struct itimerval32 i32;
	int error;

	if (uap->itv != NULL) {
		error = copyin(uap->itv, &i32, sizeof(i32));
		if (error)
			return (error);
		TV_CP(i32, itv, it_interval);
		TV_CP(i32, itv, it_value);
		itvp = &itv;
	} else
		itvp = NULL;
	error = kern_setitimer(td, uap->which, itvp, &oitv);
	if (error || uap->oitv == NULL)
		return (error);
	TV_CP(oitv, i32, it_interval);
	TV_CP(oitv, i32, it_value);
	return (copyout(&i32, uap->oitv, sizeof(i32)));
}

int
freebsd32_getitimer(struct thread *td, struct freebsd32_getitimer_args *uap)
{
	struct itimerval itv;
	struct itimerval32 i32;
	int error;

	error = kern_getitimer(td, uap->which, &itv);
	if (error || uap->itv == NULL)
		return (error);
	TV_CP(itv, i32, it_interval);
	TV_CP(itv, i32, it_value);
	return (copyout(&i32, uap->itv, sizeof(i32)));
}

int
freebsd32_select(struct thread *td, struct freebsd32_select_args *uap)
{
	struct timeval32 tv32;
	struct timeval tv, *tvp;
	int error;

	if (uap->tv != NULL) {
		error = copyin(uap->tv, &tv32, sizeof(tv32));
		if (error)
			return (error);
		CP(tv32, tv, tv_sec);
		CP(tv32, tv, tv_usec);
		tvp = &tv;
	} else
		tvp = NULL;
	/*
	 * XXX Do pointers need PTRIN()?
	 */
	return (kern_select(td, uap->nd, uap->in, uap->ou, uap->ex, tvp,
	    sizeof(int32_t) * 8));
}

int
freebsd32_pselect(struct thread *td, struct freebsd32_pselect_args *uap)
{
	struct timespec32 ts32;
	struct timespec ts;
	struct timeval tv, *tvp;
	sigset_t set, *uset;
	int error;

	if (uap->ts != NULL) {
		error = copyin(uap->ts, &ts32, sizeof(ts32));
		if (error != 0)
			return (error);
		CP(ts32, ts, tv_sec);
		CP(ts32, ts, tv_nsec);
		TIMESPEC_TO_TIMEVAL(&tv, &ts);
		tvp = &tv;
	} else
		tvp = NULL;
	if (uap->sm != NULL) {
		error = copyin(uap->sm, &set, sizeof(set));
		if (error != 0)
			return (error);
		uset = &set;
	} else
		uset = NULL;
	/*
	 * XXX Do pointers need PTRIN()?
	 */
	error = kern_pselect(td, uap->nd, uap->in, uap->ou, uap->ex, tvp,
	    uset, sizeof(int32_t) * 8);
	return (error);
}

static void
freebsd32_kevent_to_kevent32(const struct kevent *kevp, struct kevent32 *ks32)
{
	uint64_t e;
	int j;

	CP(*kevp, *ks32, ident);
	CP(*kevp, *ks32, filter);
	CP(*kevp, *ks32, flags);
	CP(*kevp, *ks32, fflags);
#if BYTE_ORDER == LITTLE_ENDIAN
	ks32->data1 = kevp->data;
	ks32->data2 = kevp->data >> 32;
#else
	ks32->data1 = kevp->data >> 32;
	ks32->data2 = kevp->data;
#endif
	PTROUT_CP(*kevp, *ks32, udata);
	for (j = 0; j < nitems(kevp->ext); j++) {
		e = kevp->ext[j];
#if BYTE_ORDER == LITTLE_ENDIAN
		ks32->ext64[2 * j] = e;
		ks32->ext64[2 * j + 1] = e >> 32;
#else
		ks32->ext64[2 * j] = e >> 32;
		ks32->ext64[2 * j + 1] = e;
#endif
	}
}

void
freebsd32_kinfo_knote_to_32(const struct kinfo_knote *kin,
    struct kinfo_knote32 *kin32)
{
	memset(kin32, 0, sizeof(*kin32));
	CP(*kin, *kin32, knt_kq_fd);
	freebsd32_kevent_to_kevent32(&kin->knt_event, &kin32->knt_event);
	CP(*kin, *kin32, knt_status);
	CP(*kin, *kin32, knt_extdata);
	switch (kin->knt_extdata) {
	case KNOTE_EXTDATA_NONE:
		break;
	case KNOTE_EXTDATA_VNODE:
		CP(*kin, *kin32, knt_vnode.knt_vnode_type);
#if BYTE_ORDER == LITTLE_ENDIAN
		kin32->knt_vnode.knt_vnode_fsid[0] = kin->knt_vnode.
		    knt_vnode_fsid;
		kin32->knt_vnode.knt_vnode_fsid[1] = kin->knt_vnode.
		    knt_vnode_fsid >> 32;
		kin32->knt_vnode.knt_vnode_fileid[0] = kin->knt_vnode.
		    knt_vnode_fileid;
		kin32->knt_vnode.knt_vnode_fileid[1] = kin->knt_vnode.
		    knt_vnode_fileid >> 32;
#else
		kin32->knt_vnode.knt_vnode_fsid[1] = kin->knt_vnode.
		    knt_vnode_fsid;
		kin32->knt_vnode.knt_vnode_fsid[0] = kin->knt_vnode.
		    knt_vnode_fsid >> 32;
		kin32->knt_vnode.knt_vnode_fileid[1] = kin->knt_vnode.
		    knt_vnode_fileid;
		kin32->knt_vnode.knt_vnode_fileid[0] = kin->knt_vnode.
		    knt_vnode_fileid >> 32;
#endif
		memcpy(kin32->knt_vnode.knt_vnode_fullpath,
		    kin->knt_vnode.knt_vnode_fullpath, PATH_MAX);
		break;
	case KNOTE_EXTDATA_PIPE:
#if BYTE_ORDER == LITTLE_ENDIAN
		kin32->knt_pipe.knt_pipe_ino[0] = kin->knt_pipe.knt_pipe_ino;
		kin32->knt_pipe.knt_pipe_ino[1] = kin->knt_pipe.
		    knt_pipe_ino >> 32;
#else
		kin32->knt_pipe.knt_pipe_ino[1] = kin->knt_pipe.knt_pipe_ino;
		kin32->knt_pipe.knt_pipe_ino[0] = kin->knt_pipe.
		    knt_pipe_ino >> 32;
#endif
		break;
	}
}

/*
 * Copy 'count' items into the destination list pointed to by uap->eventlist.
 */
static int
freebsd32_kevent_copyout(void *arg, struct kevent *kevp, int count)
{
	struct freebsd32_kevent_args *uap;
	struct kevent32	ks32[KQ_NEVENTS];
	int i, error;

	KASSERT(count <= KQ_NEVENTS, ("count (%d) > KQ_NEVENTS", count));
	uap = (struct freebsd32_kevent_args *)arg;

	for (i = 0; i < count; i++)
		freebsd32_kevent_to_kevent32(&kevp[i], &ks32[i]);
	error = copyout(ks32, uap->eventlist, count * sizeof *ks32);
	if (error == 0)
		uap->eventlist += count;
	return (error);
}

/*
 * Copy 'count' items from the list pointed to by uap->changelist.
 */
static int
freebsd32_kevent_copyin(void *arg, struct kevent *kevp, int count)
{
	struct freebsd32_kevent_args *uap;
	struct kevent32	ks32[KQ_NEVENTS];
	uint64_t e;
	int i, j, error;

	KASSERT(count <= KQ_NEVENTS, ("count (%d) > KQ_NEVENTS", count));
	uap = (struct freebsd32_kevent_args *)arg;

	error = copyin(uap->changelist, ks32, count * sizeof *ks32);
	if (error)
		goto done;
	uap->changelist += count;

	for (i = 0; i < count; i++) {
		CP(ks32[i], kevp[i], ident);
		CP(ks32[i], kevp[i], filter);
		CP(ks32[i], kevp[i], flags);
		CP(ks32[i], kevp[i], fflags);
		kevp[i].data = PAIR32TO64(uint64_t, ks32[i].data);
		PTRIN_CP(ks32[i], kevp[i], udata);
		for (j = 0; j < nitems(kevp->ext); j++) {
#if BYTE_ORDER == LITTLE_ENDIAN
			e = ks32[i].ext64[2 * j + 1];
			e <<= 32;
			e += ks32[i].ext64[2 * j];
#else
			e = ks32[i].ext64[2 * j];
			e <<= 32;
			e += ks32[i].ext64[2 * j + 1];
#endif
			kevp[i].ext[j] = e;
		}
	}
done:
	return (error);
}

int
freebsd32_kevent(struct thread *td, struct freebsd32_kevent_args *uap)
{
	struct timespec32 ts32;
	struct timespec ts, *tsp;
	struct kevent_copyops k_ops = {
		.arg = uap,
		.k_copyout = freebsd32_kevent_copyout,
		.k_copyin = freebsd32_kevent_copyin,
	};
#ifdef KTRACE
	struct kevent32 *eventlist = uap->eventlist;
#endif
	int error;

	if (uap->timeout) {
		error = copyin(uap->timeout, &ts32, sizeof(ts32));
		if (error)
			return (error);
		CP(ts32, ts, tv_sec);
		CP(ts32, ts, tv_nsec);
		tsp = &ts;
	} else
		tsp = NULL;
#ifdef KTRACE
	if (KTRPOINT(td, KTR_STRUCT_ARRAY))
		ktrstructarray("kevent32", UIO_USERSPACE, uap->changelist,
		    uap->nchanges, sizeof(struct kevent32));
#endif
	error = kern_kevent(td, uap->fd, uap->nchanges, uap->nevents,
	    &k_ops, tsp);
#ifdef KTRACE
	if (error == 0 && KTRPOINT(td, KTR_STRUCT_ARRAY))
		ktrstructarray("kevent32", UIO_USERSPACE, eventlist,
		    td->td_retval[0], sizeof(struct kevent32));
#endif
	return (error);
}

#ifdef COMPAT_FREEBSD11
static int
freebsd32_kevent11_copyout(void *arg, struct kevent *kevp, int count)
{
	struct freebsd11_freebsd32_kevent_args *uap;
	struct freebsd11_kevent32 ks32[KQ_NEVENTS];
	int i, error;

	KASSERT(count <= KQ_NEVENTS, ("count (%d) > KQ_NEVENTS", count));
	uap = (struct freebsd11_freebsd32_kevent_args *)arg;

	for (i = 0; i < count; i++) {
		CP(kevp[i], ks32[i], ident);
		CP(kevp[i], ks32[i], filter);
		CP(kevp[i], ks32[i], flags);
		CP(kevp[i], ks32[i], fflags);
		CP(kevp[i], ks32[i], data);
		PTROUT_CP(kevp[i], ks32[i], udata);
	}
	error = copyout(ks32, uap->eventlist, count * sizeof *ks32);
	if (error == 0)
		uap->eventlist += count;
	return (error);
}

/*
 * Copy 'count' items from the list pointed to by uap->changelist.
 */
static int
freebsd32_kevent11_copyin(void *arg, struct kevent *kevp, int count)
{
	struct freebsd11_freebsd32_kevent_args *uap;
	struct freebsd11_kevent32 ks32[KQ_NEVENTS];
	int i, j, error;

	KASSERT(count <= KQ_NEVENTS, ("count (%d) > KQ_NEVENTS", count));
	uap = (struct freebsd11_freebsd32_kevent_args *)arg;

	error = copyin(uap->changelist, ks32, count * sizeof *ks32);
	if (error)
		goto done;
	uap->changelist += count;

	for (i = 0; i < count; i++) {
		CP(ks32[i], kevp[i], ident);
		CP(ks32[i], kevp[i], filter);
		CP(ks32[i], kevp[i], flags);
		CP(ks32[i], kevp[i], fflags);
		CP(ks32[i], kevp[i], data);
		PTRIN_CP(ks32[i], kevp[i], udata);
		for (j = 0; j < nitems(kevp->ext); j++)
			kevp[i].ext[j] = 0;
	}
done:
	return (error);
}

int
freebsd11_freebsd32_kevent(struct thread *td,
    struct freebsd11_freebsd32_kevent_args *uap)
{
	struct timespec32 ts32;
	struct timespec ts, *tsp;
	struct kevent_copyops k_ops = {
		.arg = uap,
		.k_copyout = freebsd32_kevent11_copyout,
		.k_copyin = freebsd32_kevent11_copyin,
	};
#ifdef KTRACE
	struct freebsd11_kevent32 *eventlist = uap->eventlist;
#endif
	int error;

	if (uap->timeout) {
		error = copyin(uap->timeout, &ts32, sizeof(ts32));
		if (error)
			return (error);
		CP(ts32, ts, tv_sec);
		CP(ts32, ts, tv_nsec);
		tsp = &ts;
	} else
		tsp = NULL;
#ifdef KTRACE
	if (KTRPOINT(td, KTR_STRUCT_ARRAY))
		ktrstructarray("freebsd11_kevent32", UIO_USERSPACE,
		    uap->changelist, uap->nchanges,
		    sizeof(struct freebsd11_kevent32));
#endif
	error = kern_kevent(td, uap->fd, uap->nchanges, uap->nevents,
	    &k_ops, tsp);
#ifdef KTRACE
	if (error == 0 && KTRPOINT(td, KTR_STRUCT_ARRAY))
		ktrstructarray("freebsd11_kevent32", UIO_USERSPACE,
		    eventlist, td->td_retval[0],
		    sizeof(struct freebsd11_kevent32));
#endif
	return (error);
}
#endif

int
freebsd32_gettimeofday(struct thread *td,
		       struct freebsd32_gettimeofday_args *uap)
{
	struct timeval atv;
	struct timeval32 atv32;
	struct timezone rtz;
	int error = 0;

	if (uap->tp) {
		microtime(&atv);
		CP(atv, atv32, tv_sec);
		CP(atv, atv32, tv_usec);
		error = copyout(&atv32, uap->tp, sizeof (atv32));
	}
	if (error == 0 && uap->tzp != NULL) {
		rtz.tz_minuteswest = 0;
		rtz.tz_dsttime = 0;
		error = copyout(&rtz, uap->tzp, sizeof (rtz));
	}
	return (error);
}

int
freebsd32_getrusage(struct thread *td, struct freebsd32_getrusage_args *uap)
{
	struct rusage32 s32;
	struct rusage s;
	int error;

	error = kern_getrusage(td, uap->who, &s);
	if (error == 0) {
		freebsd32_rusage_out(&s, &s32);
		error = copyout(&s32, uap->rusage, sizeof(s32));
	}
	return (error);
}

static void
ptrace_lwpinfo_to32(const struct ptrace_lwpinfo *pl,
    struct ptrace_lwpinfo32 *pl32)
{

	bzero(pl32, sizeof(*pl32));
	pl32->pl_lwpid = pl->pl_lwpid;
	pl32->pl_event = pl->pl_event;
	pl32->pl_flags = pl->pl_flags;
	pl32->pl_sigmask = pl->pl_sigmask;
	pl32->pl_siglist = pl->pl_siglist;
	siginfo_to_siginfo32(&pl->pl_siginfo, &pl32->pl_siginfo);
	strcpy(pl32->pl_tdname, pl->pl_tdname);
	pl32->pl_child_pid = pl->pl_child_pid;
	pl32->pl_syscall_code = pl->pl_syscall_code;
	pl32->pl_syscall_narg = pl->pl_syscall_narg;
}

static void
ptrace_sc_ret_to32(const struct ptrace_sc_ret *psr,
    struct ptrace_sc_ret32 *psr32)
{

	bzero(psr32, sizeof(*psr32));
	psr32->sr_retval[0] = psr->sr_retval[0];
	psr32->sr_retval[1] = psr->sr_retval[1];
	psr32->sr_error = psr->sr_error;
}

int
freebsd32_ptrace(struct thread *td, struct freebsd32_ptrace_args *uap)
{
	union {
		struct ptrace_io_desc piod;
		struct ptrace_lwpinfo pl;
		struct ptrace_vm_entry pve;
		struct ptrace_coredump pc;
		struct ptrace_sc_remote sr;
		struct dbreg32 dbreg;
		struct fpreg32 fpreg;
		struct reg32 reg;
		struct iovec vec;
		register_t args[nitems(td->td_sa.args)];
		struct ptrace_sc_ret psr;
		int ptevents;
	} r;
	union {
		struct ptrace_io_desc32 piod;
		struct ptrace_lwpinfo32 pl;
		struct ptrace_vm_entry32 pve;
		struct ptrace_coredump32 pc;
		struct ptrace_sc_remote32 sr;
		uint32_t args[nitems(td->td_sa.args)];
		struct ptrace_sc_ret32 psr;
		struct iovec32 vec;
	} r32;
	syscallarg_t pscr_args[nitems(td->td_sa.args)];
	u_int pscr_args32[nitems(td->td_sa.args)];
	void *addr;
	int data, error, i;

	if (!allow_ptrace)
		return (ENOSYS);
	error = 0;

	AUDIT_ARG_PID(uap->pid);
	AUDIT_ARG_CMD(uap->req);
	AUDIT_ARG_VALUE(uap->data);
	addr = &r;
	data = uap->data;
	switch (uap->req) {
	case PT_GET_EVENT_MASK:
	case PT_GET_SC_ARGS:
	case PT_GET_SC_RET:
		break;
	case PT_LWPINFO:
		if (uap->data > sizeof(r32.pl))
			return (EINVAL);

		/*
		 * Pass size of native structure in 'data'.  Truncate
		 * if necessary to avoid siginfo.
		 */
		data = sizeof(r.pl);
		if (uap->data < offsetof(struct ptrace_lwpinfo32, pl_siginfo) +
		    sizeof(struct __siginfo32))
			data = offsetof(struct ptrace_lwpinfo, pl_siginfo);
		break;
	case PT_GETREGS:
		bzero(&r.reg, sizeof(r.reg));
		break;
	case PT_GETFPREGS:
		bzero(&r.fpreg, sizeof(r.fpreg));
		break;
	case PT_GETDBREGS:
		bzero(&r.dbreg, sizeof(r.dbreg));
		break;
	case PT_SETREGS:
		error = copyin(uap->addr, &r.reg, sizeof(r.reg));
		break;
	case PT_SETFPREGS:
		error = copyin(uap->addr, &r.fpreg, sizeof(r.fpreg));
		break;
	case PT_SETDBREGS:
		error = copyin(uap->addr, &r.dbreg, sizeof(r.dbreg));
		break;
	case PT_GETREGSET:
	case PT_SETREGSET:
		error = copyin(uap->addr, &r32.vec, sizeof(r32.vec));
		if (error != 0)
			break;

		r.vec.iov_len = r32.vec.iov_len;
		r.vec.iov_base = PTRIN(r32.vec.iov_base);
		break;
	case PT_SET_EVENT_MASK:
		if (uap->data != sizeof(r.ptevents))
			error = EINVAL;
		else
			error = copyin(uap->addr, &r.ptevents, uap->data);
		break;
	case PT_IO:
		error = copyin(uap->addr, &r32.piod, sizeof(r32.piod));
		if (error)
			break;
		CP(r32.piod, r.piod, piod_op);
		PTRIN_CP(r32.piod, r.piod, piod_offs);
		PTRIN_CP(r32.piod, r.piod, piod_addr);
		CP(r32.piod, r.piod, piod_len);
		break;
	case PT_VM_ENTRY:
		error = copyin(uap->addr, &r32.pve, sizeof(r32.pve));
		if (error)
			break;

		CP(r32.pve, r.pve, pve_entry);
		CP(r32.pve, r.pve, pve_timestamp);
		CP(r32.pve, r.pve, pve_start);
		CP(r32.pve, r.pve, pve_end);
		CP(r32.pve, r.pve, pve_offset);
		CP(r32.pve, r.pve, pve_prot);
		CP(r32.pve, r.pve, pve_pathlen);
		CP(r32.pve, r.pve, pve_fileid);
		CP(r32.pve, r.pve, pve_fsid);
		PTRIN_CP(r32.pve, r.pve, pve_path);
		break;
	case PT_COREDUMP:
		if (uap->data != sizeof(r32.pc))
			error = EINVAL;
		else
			error = copyin(uap->addr, &r32.pc, uap->data);
		CP(r32.pc, r.pc, pc_fd);
		CP(r32.pc, r.pc, pc_flags);
		r.pc.pc_limit = PAIR32TO64(off_t, r32.pc.pc_limit);
		data = sizeof(r.pc);
		break;
	case PT_SC_REMOTE:
		if (uap->data != sizeof(r32.sr)) {
			error = EINVAL;
			break;
		}
		error = copyin(uap->addr, &r32.sr, uap->data);
		if (error != 0)
			break;
		CP(r32.sr, r.sr, pscr_syscall);
		CP(r32.sr, r.sr, pscr_nargs);
		if (r.sr.pscr_nargs > nitems(td->td_sa.args)) {
			error = EINVAL;
			break;
		}
		error = copyin(PTRIN(r32.sr.pscr_args), pscr_args32,
		    sizeof(u_int) * r32.sr.pscr_nargs);
		if (error != 0)
			break;
		for (i = 0; i < r32.sr.pscr_nargs; i++)
			pscr_args[i] = pscr_args32[i];
		r.sr.pscr_args = pscr_args;
		break;
	default:
		addr = uap->addr;
		break;
	}
	if (error)
		return (error);

	error = kern_ptrace(td, uap->req, uap->pid, addr, data);
	if (error)
		return (error);

	switch (uap->req) {
	case PT_VM_ENTRY:
		CP(r.pve, r32.pve, pve_entry);
		CP(r.pve, r32.pve, pve_timestamp);
		CP(r.pve, r32.pve, pve_start);
		CP(r.pve, r32.pve, pve_end);
		CP(r.pve, r32.pve, pve_offset);
		CP(r.pve, r32.pve, pve_prot);
		CP(r.pve, r32.pve, pve_pathlen);
		CP(r.pve, r32.pve, pve_fileid);
		CP(r.pve, r32.pve, pve_fsid);
		error = copyout(&r32.pve, uap->addr, sizeof(r32.pve));
		break;
	case PT_IO:
		CP(r.piod, r32.piod, piod_len);
		error = copyout(&r32.piod, uap->addr, sizeof(r32.piod));
		break;
	case PT_GETREGS:
		error = copyout(&r.reg, uap->addr, sizeof(r.reg));
		break;
	case PT_GETFPREGS:
		error = copyout(&r.fpreg, uap->addr, sizeof(r.fpreg));
		break;
	case PT_GETDBREGS:
		error = copyout(&r.dbreg, uap->addr, sizeof(r.dbreg));
		break;
	case PT_GETREGSET:
		r32.vec.iov_len = r.vec.iov_len;
		error = copyout(&r32.vec, uap->addr, sizeof(r32.vec));
		break;
	case PT_GET_EVENT_MASK:
		/* NB: The size in uap->data is validated in kern_ptrace(). */
		error = copyout(&r.ptevents, uap->addr, uap->data);
		break;
	case PT_LWPINFO:
		ptrace_lwpinfo_to32(&r.pl, &r32.pl);
		error = copyout(&r32.pl, uap->addr, uap->data);
		break;
	case PT_GET_SC_ARGS:
		for (i = 0; i < nitems(r.args); i++)
			r32.args[i] = (uint32_t)r.args[i];
		error = copyout(r32.args, uap->addr, MIN(uap->data,
		    sizeof(r32.args)));
		break;
	case PT_GET_SC_RET:
		ptrace_sc_ret_to32(&r.psr, &r32.psr);
		error = copyout(&r32.psr, uap->addr, MIN(uap->data,
		    sizeof(r32.psr)));
		break;
	case PT_SC_REMOTE:
		ptrace_sc_ret_to32(&r.sr.pscr_ret, &r32.sr.pscr_ret);
		error = copyout(&r32.sr.pscr_ret, uap->addr +
		    offsetof(struct ptrace_sc_remote32, pscr_ret),
		    sizeof(r32.psr));
		break;
	}

	return (error);
}

int
freebsd32_copyinuio(const struct iovec32 *iovp, u_int iovcnt, struct uio **uiop)
{
	struct iovec32 iov32;
	struct iovec *iov;
	struct uio *uio;
	int error, i;

	*uiop = NULL;
	if (iovcnt > UIO_MAXIOV)
		return (EINVAL);
	uio = allocuio(iovcnt);
	iov = uio->uio_iov;
	for (i = 0; i < iovcnt; i++) {
		error = copyin(&iovp[i], &iov32, sizeof(struct iovec32));
		if (error) {
			freeuio(uio);
			return (error);
		}
		iov[i].iov_base = PTRIN(iov32.iov_base);
		iov[i].iov_len = iov32.iov_len;
	}
	uio->uio_iovcnt = iovcnt;
	uio->uio_segflg = UIO_USERSPACE;
	uio->uio_offset = -1;
	uio->uio_resid = 0;
	for (i = 0; i < iovcnt; i++) {
		if (iov->iov_len > INT_MAX - uio->uio_resid) {
			freeuio(uio);
			return (EINVAL);
		}
		uio->uio_resid += iov->iov_len;
		iov++;
	}
	*uiop = uio;
	return (0);
}

int
freebsd32_readv(struct thread *td, struct freebsd32_readv_args *uap)
{
	struct uio *auio;
	int error;

	error = freebsd32_copyinuio(uap->iovp, uap->iovcnt, &auio);
	if (error)
		return (error);
	error = kern_readv(td, uap->fd, auio);
	freeuio(auio);
	return (error);
}

int
freebsd32_writev(struct thread *td, struct freebsd32_writev_args *uap)
{
	struct uio *auio;
	int error;

	error = freebsd32_copyinuio(uap->iovp, uap->iovcnt, &auio);
	if (error)
		return (error);
	error = kern_writev(td, uap->fd, auio);
	freeuio(auio);
	return (error);
}

int
freebsd32_preadv(struct thread *td, struct freebsd32_preadv_args *uap)
{
	struct uio *auio;
	int error;

	error = freebsd32_copyinuio(uap->iovp, uap->iovcnt, &auio);
	if (error)
		return (error);
	error = kern_preadv(td, uap->fd, auio, PAIR32TO64(off_t,uap->offset));
	freeuio(auio);
	return (error);
}

int
freebsd32_pwritev(struct thread *td, struct freebsd32_pwritev_args *uap)
{
	struct uio *auio;
	int error;

	error = freebsd32_copyinuio(uap->iovp, uap->iovcnt, &auio);
	if (error)
		return (error);
	error = kern_pwritev(td, uap->fd, auio, PAIR32TO64(off_t,uap->offset));
	freeuio(auio);
	return (error);
}

int
freebsd32_copyiniov(struct iovec32 *iovp32, u_int iovcnt, struct iovec **iovp,
    int error)
{
	struct iovec32 iov32;
	struct iovec *iov;
	u_int iovlen;
	int i;

	*iovp = NULL;
	if (iovcnt > UIO_MAXIOV)
		return (error);
	iovlen = iovcnt * sizeof(struct iovec);
	iov = malloc(iovlen, M_IOV, M_WAITOK);
	for (i = 0; i < iovcnt; i++) {
		error = copyin(&iovp32[i], &iov32, sizeof(struct iovec32));
		if (error) {
			free(iov, M_IOV);
			return (error);
		}
		iov[i].iov_base = PTRIN(iov32.iov_base);
		iov[i].iov_len = iov32.iov_len;
	}
	*iovp = iov;
	return (0);
}

static int
freebsd32_copyinmsghdr(const struct msghdr32 *msg32, struct msghdr *msg)
{
	struct msghdr32 m32;
	int error;

	error = copyin(msg32, &m32, sizeof(m32));
	if (error)
		return (error);
	msg->msg_name = PTRIN(m32.msg_name);
	msg->msg_namelen = m32.msg_namelen;
	msg->msg_iov = PTRIN(m32.msg_iov);
	msg->msg_iovlen = m32.msg_iovlen;
	msg->msg_control = PTRIN(m32.msg_control);
	msg->msg_controllen = m32.msg_controllen;
	msg->msg_flags = m32.msg_flags;
	return (0);
}

static int
freebsd32_copyoutmsghdr(struct msghdr *msg, struct msghdr32 *msg32)
{
	struct msghdr32 m32;
	int error;

	m32.msg_name = PTROUT(msg->msg_name);
	m32.msg_namelen = msg->msg_namelen;
	m32.msg_iov = PTROUT(msg->msg_iov);
	m32.msg_iovlen = msg->msg_iovlen;
	m32.msg_control = PTROUT(msg->msg_control);
	m32.msg_controllen = msg->msg_controllen;
	m32.msg_flags = msg->msg_flags;
	error = copyout(&m32, msg32, sizeof(m32));
	return (error);
}

#define FREEBSD32_ALIGNBYTES	(sizeof(int) - 1)
#define FREEBSD32_ALIGN(p)	\
	(((u_long)(p) + FREEBSD32_ALIGNBYTES) & ~FREEBSD32_ALIGNBYTES)
#define	FREEBSD32_CMSG_SPACE(l)	\
	(FREEBSD32_ALIGN(sizeof(struct cmsghdr)) + FREEBSD32_ALIGN(l))

#define	FREEBSD32_CMSG_DATA(cmsg)	((unsigned char *)(cmsg) + \
				 FREEBSD32_ALIGN(sizeof(struct cmsghdr)))

static size_t
freebsd32_cmsg_convert(const struct cmsghdr *cm, void *data, socklen_t datalen)
{
	size_t copylen;
	union {
		struct timespec32 ts;
		struct timeval32 tv;
		struct bintime32 bt;
	} tmp32;

	union {
		struct timespec ts;
		struct timeval tv;
		struct bintime bt;
	} *in;

	in = data;
	copylen = 0;
	switch (cm->cmsg_level) {
	case SOL_SOCKET:
		switch (cm->cmsg_type) {
		case SCM_TIMESTAMP:
			TV_CP(*in, tmp32, tv);
			copylen = sizeof(tmp32.tv);
			break;

		case SCM_BINTIME:
			BT_CP(*in, tmp32, bt);
			copylen = sizeof(tmp32.bt);
			break;

		case SCM_REALTIME:
		case SCM_MONOTONIC:
			TS_CP(*in, tmp32, ts);
			copylen = sizeof(tmp32.ts);
			break;

		default:
			break;
		}

	default:
		break;
	}

	if (copylen == 0)
		return (datalen);

	KASSERT((datalen >= copylen), ("corrupted cmsghdr"));

	bcopy(&tmp32, data, copylen);
	return (copylen);
}

static int
freebsd32_copy_msg_out(struct msghdr *msg, struct mbuf *control)
{
	struct cmsghdr *cm;
	void *data;
	socklen_t clen, datalen, datalen_out, oldclen;
	int error;
	caddr_t ctlbuf;
	int len, copylen;
	struct mbuf *m;
	error = 0;

	len    = msg->msg_controllen;
	msg->msg_controllen = 0;

	ctlbuf = msg->msg_control;
	for (m = control; m != NULL && len > 0; m = m->m_next) {
		cm = mtod(m, struct cmsghdr *);
		clen = m->m_len;
		while (cm != NULL) {
			if (sizeof(struct cmsghdr) > clen ||
			    cm->cmsg_len > clen) {
				error = EINVAL;
				break;
			}

			data   = CMSG_DATA(cm);
			datalen = (caddr_t)cm + cm->cmsg_len - (caddr_t)data;
			datalen_out = freebsd32_cmsg_convert(cm, data, datalen);

			/*
			 * Copy out the message header.  Preserve the native
			 * message size in case we need to inspect the message
			 * contents later.
			 */
			copylen = sizeof(struct cmsghdr);
			if (len < copylen) {
				msg->msg_flags |= MSG_CTRUNC;
				m_dispose_extcontrolm(m);
				goto exit;
			}
			oldclen = cm->cmsg_len;
			cm->cmsg_len = FREEBSD32_ALIGN(sizeof(struct cmsghdr)) +
			    datalen_out;
			error = copyout(cm, ctlbuf, copylen);
			cm->cmsg_len = oldclen;
			if (error != 0)
				goto exit;

			ctlbuf += FREEBSD32_ALIGN(copylen);
			len    -= FREEBSD32_ALIGN(copylen);

			copylen = datalen_out;
			if (len < copylen) {
				msg->msg_flags |= MSG_CTRUNC;
				m_dispose_extcontrolm(m);
				break;
			}

			/* Copy out the message data. */
			error = copyout(data, ctlbuf, copylen);
			if (error)
				goto exit;

			ctlbuf += FREEBSD32_ALIGN(copylen);
			len    -= FREEBSD32_ALIGN(copylen);

			if (CMSG_SPACE(datalen) < clen) {
				clen -= CMSG_SPACE(datalen);
				cm = (struct cmsghdr *)
				    ((caddr_t)cm + CMSG_SPACE(datalen));
			} else {
				clen = 0;
				cm = NULL;
			}

			msg->msg_controllen +=
			    FREEBSD32_CMSG_SPACE(datalen_out);
		}
	}
	if (len == 0 && m != NULL) {
		msg->msg_flags |= MSG_CTRUNC;
		m_dispose_extcontrolm(m);
	}

exit:
	return (error);
}

int
freebsd32_recvmsg(struct thread *td, struct freebsd32_recvmsg_args *uap)
{
	struct msghdr msg;
	struct iovec *uiov, *iov;
	struct mbuf *control = NULL;
	struct mbuf **controlp;
	int error;

	error = freebsd32_copyinmsghdr(uap->msg, &msg);
	if (error)
		return (error);
	error = freebsd32_copyiniov((void *)msg.msg_iov, msg.msg_iovlen, &iov,
	    EMSGSIZE);
	if (error)
		return (error);
	msg.msg_flags = uap->flags;
	uiov = msg.msg_iov;
	msg.msg_iov = iov;

	controlp = (msg.msg_control != NULL) ?  &control : NULL;
	error = kern_recvit(td, uap->s, &msg, UIO_USERSPACE, controlp);
	if (error == 0) {
		msg.msg_iov = uiov;

		if (control != NULL)
			error = freebsd32_copy_msg_out(&msg, control);
		else
			msg.msg_controllen = 0;

		if (error == 0)
			error = freebsd32_copyoutmsghdr(&msg, uap->msg);
	}
	free(iov, M_IOV);

	if (control != NULL) {
		if (error != 0)
			m_dispose_extcontrolm(control);
		m_freem(control);
	}

	return (error);
}

#ifdef COMPAT_43
int
ofreebsd32_recvmsg(struct thread *td, struct ofreebsd32_recvmsg_args *uap)
{
	return (ENOSYS);
}
#endif

/*
 * Copy-in the array of control messages constructed using alignment
 * and padding suitable for a 32-bit environment and construct an
 * mbuf using alignment and padding suitable for a 64-bit kernel.
 * The alignment and padding are defined indirectly by CMSG_DATA(),
 * CMSG_SPACE() and CMSG_LEN().
 */
static int
freebsd32_copyin_control(struct mbuf **mp, caddr_t buf, u_int buflen)
{
	struct cmsghdr *cm;
	struct mbuf *m;
	void *in, *in1, *md;
	u_int msglen, outlen;
	int error;

	/* Enforce the size limit of the native implementation. */
	if (buflen > MCLBYTES)
		return (EINVAL);

	in = malloc(buflen, M_TEMP, M_WAITOK);
	error = copyin(buf, in, buflen);
	if (error != 0)
		goto out;

	/*
	 * Make a pass over the input buffer to determine the amount of space
	 * required for 64 bit-aligned copies of the control messages.
	 */
	in1 = in;
	outlen = 0;
	while (buflen > 0) {
		if (buflen < sizeof(*cm)) {
			error = EINVAL;
			break;
		}
		cm = (struct cmsghdr *)in1;
		if (cm->cmsg_len < FREEBSD32_ALIGN(sizeof(*cm)) ||
		    cm->cmsg_len > buflen) {
			error = EINVAL;
			break;
		}
		msglen = FREEBSD32_ALIGN(cm->cmsg_len);
		if (msglen < cm->cmsg_len) {
			error = EINVAL;
			break;
		}
		/* The native ABI permits the final padding to be omitted. */
		if (msglen > buflen)
			msglen = buflen;
		buflen -= msglen;

		in1 = (char *)in1 + msglen;
		outlen += CMSG_ALIGN(sizeof(*cm)) +
		    CMSG_ALIGN(msglen - FREEBSD32_ALIGN(sizeof(*cm)));
	}
	if (error != 0)
		goto out;

	/*
	 * Allocate up to MJUMPAGESIZE space for the re-aligned and
	 * re-padded control messages.  This allows a full MCLBYTES of
	 * 32-bit sized and aligned messages to fit and avoids an ABI
	 * mismatch with the native implementation.
	 */
	m = m_get2(outlen, M_WAITOK, MT_CONTROL, 0);
	if (m == NULL) {
		error = EINVAL;
		goto out;
	}
	m->m_len = outlen;
	md = mtod(m, void *);

	/*
	 * Make a second pass over input messages, copying them into the output
	 * buffer.
	 */
	in1 = in;
	while (outlen > 0) {
		/* Copy the message header and align the length field. */
		cm = md;
		memcpy(cm, in1, sizeof(*cm));
		msglen = cm->cmsg_len - FREEBSD32_ALIGN(sizeof(*cm));
		cm->cmsg_len = CMSG_ALIGN(sizeof(*cm)) + msglen;

		/* Copy the message body. */
		in1 = (char *)in1 + FREEBSD32_ALIGN(sizeof(*cm));
		md = (char *)md + CMSG_ALIGN(sizeof(*cm));
		memcpy(md, in1, msglen);
		in1 = (char *)in1 + FREEBSD32_ALIGN(msglen);
		md = (char *)md + CMSG_ALIGN(msglen);
		KASSERT(outlen >= CMSG_ALIGN(sizeof(*cm)) + CMSG_ALIGN(msglen),
		    ("outlen %u underflow, msglen %u", outlen, msglen));
		outlen -= CMSG_ALIGN(sizeof(*cm)) + CMSG_ALIGN(msglen);
	}

	*mp = m;
out:
	free(in, M_TEMP);
	return (error);
}

int
freebsd32_sendmsg(struct thread *td, struct freebsd32_sendmsg_args *uap)
{
	struct msghdr msg;
	struct iovec *iov;
	struct mbuf *control = NULL;
	struct sockaddr *to = NULL;
	int error;

	error = freebsd32_copyinmsghdr(uap->msg, &msg);
	if (error)
		return (error);
	error = freebsd32_copyiniov((void *)msg.msg_iov, msg.msg_iovlen, &iov,
	    EMSGSIZE);
	if (error)
		return (error);
	msg.msg_iov = iov;
	if (msg.msg_name != NULL) {
		error = getsockaddr(&to, msg.msg_name, msg.msg_namelen);
		if (error) {
			to = NULL;
			goto out;
		}
		msg.msg_name = to;
	}

	if (msg.msg_control) {
		if (msg.msg_controllen < sizeof(struct cmsghdr)) {
			error = EINVAL;
			goto out;
		}

		error = freebsd32_copyin_control(&control, msg.msg_control,
		    msg.msg_controllen);
		if (error)
			goto out;

		msg.msg_control = NULL;
		msg.msg_controllen = 0;
	}

	error = kern_sendit(td, uap->s, &msg, uap->flags, control,
	    UIO_USERSPACE);

out:
	free(iov, M_IOV);
	if (to)
		free(to, M_SONAME);
	return (error);
}

#ifdef COMPAT_43
int
ofreebsd32_sendmsg(struct thread *td, struct ofreebsd32_sendmsg_args *uap)
{
	return (ENOSYS);
}
#endif


int
freebsd32_settimeofday(struct thread *td,
		       struct freebsd32_settimeofday_args *uap)
{
	struct timeval32 tv32;
	struct timeval tv, *tvp;
	struct timezone tz, *tzp;
	int error;

	if (uap->tv) {
		error = copyin(uap->tv, &tv32, sizeof(tv32));
		if (error)
			return (error);
		CP(tv32, tv, tv_sec);
		CP(tv32, tv, tv_usec);
		tvp = &tv;
	} else
		tvp = NULL;
	if (uap->tzp) {
		error = copyin(uap->tzp, &tz, sizeof(tz));
		if (error)
			return (error);
		tzp = &tz;
	} else
		tzp = NULL;
	return (kern_settimeofday(td, tvp, tzp));
}

int
freebsd32_utimes(struct thread *td, struct freebsd32_utimes_args *uap)
{
	struct timeval32 s32[2];
	struct timeval s[2], *sp;
	int error;

	if (uap->tptr != NULL) {
		error = copyin(uap->tptr, s32, sizeof(s32));
		if (error)
			return (error);
		CP(s32[0], s[0], tv_sec);
		CP(s32[0], s[0], tv_usec);
		CP(s32[1], s[1], tv_sec);
		CP(s32[1], s[1], tv_usec);
		sp = s;
	} else
		sp = NULL;
	return (kern_utimesat(td, AT_FDCWD, uap->path, UIO_USERSPACE,
	    sp, UIO_SYSSPACE));
}

int
freebsd32_lutimes(struct thread *td, struct freebsd32_lutimes_args *uap)
{
	struct timeval32 s32[2];
	struct timeval s[2], *sp;
	int error;

	if (uap->tptr != NULL) {
		error = copyin(uap->tptr, s32, sizeof(s32));
		if (error)
			return (error);
		CP(s32[0], s[0], tv_sec);
		CP(s32[0], s[0], tv_usec);
		CP(s32[1], s[1], tv_sec);
		CP(s32[1], s[1], tv_usec);
		sp = s;
	} else
		sp = NULL;
	return (kern_lutimes(td, uap->path, UIO_USERSPACE, sp, UIO_SYSSPACE));
}

int
freebsd32_futimes(struct thread *td, struct freebsd32_futimes_args *uap)
{
	struct timeval32 s32[2];
	struct timeval s[2], *sp;
	int error;

	if (uap->tptr != NULL) {
		error = copyin(uap->tptr, s32, sizeof(s32));
		if (error)
			return (error);
		CP(s32[0], s[0], tv_sec);
		CP(s32[0], s[0], tv_usec);
		CP(s32[1], s[1], tv_sec);
		CP(s32[1], s[1], tv_usec);
		sp = s;
	} else
		sp = NULL;
	return (kern_futimes(td, uap->fd, sp, UIO_SYSSPACE));
}

int
freebsd32_futimesat(struct thread *td, struct freebsd32_futimesat_args *uap)
{
	struct timeval32 s32[2];
	struct timeval s[2], *sp;
	int error;

	if (uap->times != NULL) {
		error = copyin(uap->times, s32, sizeof(s32));
		if (error)
			return (error);
		CP(s32[0], s[0], tv_sec);
		CP(s32[0], s[0], tv_usec);
		CP(s32[1], s[1], tv_sec);
		CP(s32[1], s[1], tv_usec);
		sp = s;
	} else
		sp = NULL;
	return (kern_utimesat(td, uap->fd, uap->path, UIO_USERSPACE,
		sp, UIO_SYSSPACE));
}

int
freebsd32_futimens(struct thread *td, struct freebsd32_futimens_args *uap)
{
	struct timespec32 ts32[2];
	struct timespec ts[2], *tsp;
	int error;

	if (uap->times != NULL) {
		error = copyin(uap->times, ts32, sizeof(ts32));
		if (error)
			return (error);
		CP(ts32[0], ts[0], tv_sec);
		CP(ts32[0], ts[0], tv_nsec);
		CP(ts32[1], ts[1], tv_sec);
		CP(ts32[1], ts[1], tv_nsec);
		tsp = ts;
	} else
		tsp = NULL;
	return (kern_futimens(td, uap->fd, tsp, UIO_SYSSPACE));
}

int
freebsd32_utimensat(struct thread *td, struct freebsd32_utimensat_args *uap)
{
	struct timespec32 ts32[2];
	struct timespec ts[2], *tsp;
	int error;

	if (uap->times != NULL) {
		error = copyin(uap->times, ts32, sizeof(ts32));
		if (error)
			return (error);
		CP(ts32[0], ts[0], tv_sec);
		CP(ts32[0], ts[0], tv_nsec);
		CP(ts32[1], ts[1], tv_sec);
		CP(ts32[1], ts[1], tv_nsec);
		tsp = ts;
	} else
		tsp = NULL;
	return (kern_utimensat(td, uap->fd, uap->path, UIO_USERSPACE,
	    tsp, UIO_SYSSPACE, uap->flag));
}

int
freebsd32_adjtime(struct thread *td, struct freebsd32_adjtime_args *uap)
{
	struct timeval32 tv32;
	struct timeval delta, olddelta, *deltap;
	int error;

	if (uap->delta) {
		error = copyin(uap->delta, &tv32, sizeof(tv32));
		if (error)
			return (error);
		CP(tv32, delta, tv_sec);
		CP(tv32, delta, tv_usec);
		deltap = &delta;
	} else
		deltap = NULL;
	error = kern_adjtime(td, deltap, &olddelta);
	if (uap->olddelta && error == 0) {
		CP(olddelta, tv32, tv_sec);
		CP(olddelta, tv32, tv_usec);
		error = copyout(&tv32, uap->olddelta, sizeof(tv32));
	}
	return (error);
}

#ifdef COMPAT_FREEBSD4
int
freebsd4_freebsd32_statfs(struct thread *td, struct freebsd4_freebsd32_statfs_args *uap)
{
	struct ostatfs32 s32;
	struct statfs *sp;
	int error;

	sp = malloc(sizeof(struct statfs), M_STATFS, M_WAITOK);
	error = kern_statfs(td, uap->path, UIO_USERSPACE, sp);
	if (error == 0) {
		copy_statfs(sp, &s32);
		error = copyout(&s32, uap->buf, sizeof(s32));
	}
	free(sp, M_STATFS);
	return (error);
}
#endif

#ifdef COMPAT_FREEBSD4
int
freebsd4_freebsd32_fstatfs(struct thread *td, struct freebsd4_freebsd32_fstatfs_args *uap)
{
	struct ostatfs32 s32;
	struct statfs *sp;
	int error;

	sp = malloc(sizeof(struct statfs), M_STATFS, M_WAITOK);
	error = kern_fstatfs(td, uap->fd, sp);
	if (error == 0) {
		copy_statfs(sp, &s32);
		error = copyout(&s32, uap->buf, sizeof(s32));
	}
	free(sp, M_STATFS);
	return (error);
}
#endif

#ifdef COMPAT_FREEBSD4
int
freebsd4_freebsd32_fhstatfs(struct thread *td, struct freebsd4_freebsd32_fhstatfs_args *uap)
{
	struct ostatfs32 s32;
	struct statfs *sp;
	fhandle_t fh;
	int error;

	if ((error = copyin(uap->u_fhp, &fh, sizeof(fhandle_t))) != 0)
		return (error);
	sp = malloc(sizeof(struct statfs), M_STATFS, M_WAITOK);
	error = kern_fhstatfs(td, fh, sp);
	if (error == 0) {
		copy_statfs(sp, &s32);
		error = copyout(&s32, uap->buf, sizeof(s32));
	}
	free(sp, M_STATFS);
	return (error);
}
#endif

int
freebsd32_pread(struct thread *td, struct freebsd32_pread_args *uap)
{

	return (kern_pread(td, uap->fd, uap->buf, uap->nbyte,
	    PAIR32TO64(off_t, uap->offset)));
}

int
freebsd32_pwrite(struct thread *td, struct freebsd32_pwrite_args *uap)
{

	return (kern_pwrite(td, uap->fd, uap->buf, uap->nbyte,
	    PAIR32TO64(off_t, uap->offset)));
}

#ifdef COMPAT_43
int
ofreebsd32_lseek(struct thread *td, struct ofreebsd32_lseek_args *uap)
{

	return (kern_lseek(td, uap->fd, uap->offset, uap->whence));
}
#endif

int
freebsd32_lseek(struct thread *td, struct freebsd32_lseek_args *uap)
{
	int error;
	off_t pos;

	error = kern_lseek(td, uap->fd, PAIR32TO64(off_t, uap->offset),
	    uap->whence);
	/* Expand the quad return into two parts for eax and edx */
	pos = td->td_uretoff.tdu_off;
	td->td_retval[RETVAL_LO] = pos & 0xffffffff;	/* %eax */
	td->td_retval[RETVAL_HI] = pos >> 32;		/* %edx */
	return error;
}

int
freebsd32_truncate(struct thread *td, struct freebsd32_truncate_args *uap)
{

	return (kern_truncate(td, uap->path, UIO_USERSPACE,
	    PAIR32TO64(off_t, uap->length)));
}

#ifdef COMPAT_43
int
ofreebsd32_truncate(struct thread *td, struct ofreebsd32_truncate_args *uap)
{
	return (kern_truncate(td, uap->path, UIO_USERSPACE, uap->length));
}
#endif

int
freebsd32_ftruncate(struct thread *td, struct freebsd32_ftruncate_args *uap)
{

	return (kern_ftruncate(td, uap->fd, PAIR32TO64(off_t, uap->length)));
}

#ifdef COMPAT_43
int
ofreebsd32_ftruncate(struct thread *td, struct ofreebsd32_ftruncate_args *uap)
{
	return (kern_ftruncate(td, uap->fd, uap->length));
}

int
ofreebsd32_getdirentries(struct thread *td,
    struct ofreebsd32_getdirentries_args *uap)
{
	struct ogetdirentries_args ap;
	int error;
	long loff;
	int32_t loff_cut;

	ap.fd = uap->fd;
	ap.buf = uap->buf;
	ap.count = uap->count;
	ap.basep = NULL;
	error = kern_ogetdirentries(td, &ap, &loff);
	if (error == 0) {
		loff_cut = loff;
		error = copyout(&loff_cut, uap->basep, sizeof(int32_t));
	}
	return (error);
}
#endif

#if defined(COMPAT_FREEBSD11)
int
freebsd11_freebsd32_getdirentries(struct thread *td,
    struct freebsd11_freebsd32_getdirentries_args *uap)
{
	long base;
	int32_t base32;
	int error;

	error = freebsd11_kern_getdirentries(td, uap->fd, uap->buf, uap->count,
	    &base, NULL);
	if (error)
		return (error);
	if (uap->basep != NULL) {
		base32 = base;
		error = copyout(&base32, uap->basep, sizeof(int32_t));
	}
	return (error);
}
#endif /* COMPAT_FREEBSD11 */

#ifdef COMPAT_FREEBSD6
/* versions with the 'int pad' argument */
int
freebsd6_freebsd32_pread(struct thread *td, struct freebsd6_freebsd32_pread_args *uap)
{

	return (kern_pread(td, uap->fd, uap->buf, uap->nbyte,
	    PAIR32TO64(off_t, uap->offset)));
}

int
freebsd6_freebsd32_pwrite(struct thread *td, struct freebsd6_freebsd32_pwrite_args *uap)
{

	return (kern_pwrite(td, uap->fd, uap->buf, uap->nbyte,
	    PAIR32TO64(off_t, uap->offset)));
}

int
freebsd6_freebsd32_lseek(struct thread *td, struct freebsd6_freebsd32_lseek_args *uap)
{
	int error;
	off_t pos;

	error = kern_lseek(td, uap->fd, PAIR32TO64(off_t, uap->offset),
	    uap->whence);
	/* Expand the quad return into two parts for eax and edx */
	pos = *(off_t *)(td->td_retval);
	td->td_retval[RETVAL_LO] = pos & 0xffffffff;	/* %eax */
	td->td_retval[RETVAL_HI] = pos >> 32;		/* %edx */
	return error;
}

int
freebsd6_freebsd32_truncate(struct thread *td, struct freebsd6_freebsd32_truncate_args *uap)
{

	return (kern_truncate(td, uap->path, UIO_USERSPACE,
	    PAIR32TO64(off_t, uap->length)));
}

int
freebsd6_freebsd32_ftruncate(struct thread *td, struct freebsd6_freebsd32_ftruncate_args *uap)
{

	return (kern_ftruncate(td, uap->fd, PAIR32TO64(off_t, uap->length)));
}
#endif /* COMPAT_FREEBSD6 */

struct sf_hdtr32 {
	uint32_t headers;
	int hdr_cnt;
	uint32_t trailers;
	int trl_cnt;
};

static int
freebsd32_do_sendfile(struct thread *td,
    struct freebsd32_sendfile_args *uap, int compat)
{
	struct sf_hdtr32 hdtr32;
	struct sf_hdtr hdtr;
	struct uio *hdr_uio, *trl_uio;
	struct file *fp;
	cap_rights_t rights;
	struct iovec32 *iov32;
	off_t offset, sbytes;
	int error;

	offset = PAIR32TO64(off_t, uap->offset);
	if (offset < 0)
		return (EINVAL);

	hdr_uio = trl_uio = NULL;

	if (uap->hdtr != NULL) {
		error = copyin(uap->hdtr, &hdtr32, sizeof(hdtr32));
		if (error)
			goto out;
		PTRIN_CP(hdtr32, hdtr, headers);
		CP(hdtr32, hdtr, hdr_cnt);
		PTRIN_CP(hdtr32, hdtr, trailers);
		CP(hdtr32, hdtr, trl_cnt);

		if (hdtr.headers != NULL) {
			iov32 = PTRIN(hdtr32.headers);
			error = freebsd32_copyinuio(iov32,
			    hdtr32.hdr_cnt, &hdr_uio);
			if (error)
				goto out;
#ifdef COMPAT_FREEBSD4
			/*
			 * In FreeBSD < 5.0 the nbytes to send also included
			 * the header.  If compat is specified subtract the
			 * header size from nbytes.
			 */
			if (compat) {
				if (uap->nbytes > hdr_uio->uio_resid)
					uap->nbytes -= hdr_uio->uio_resid;
				else
					uap->nbytes = 0;
			}
#endif
		}
		if (hdtr.trailers != NULL) {
			iov32 = PTRIN(hdtr32.trailers);
			error = freebsd32_copyinuio(iov32,
			    hdtr32.trl_cnt, &trl_uio);
			if (error)
				goto out;
		}
	}

	AUDIT_ARG_FD(uap->fd);

	if ((error = fget_read(td, uap->fd,
	    cap_rights_init_one(&rights, CAP_PREAD), &fp)) != 0)
		goto out;

	error = fo_sendfile(fp, uap->s, hdr_uio, trl_uio, offset,
	    uap->nbytes, &sbytes, uap->flags, td);
	fdrop(fp, td);

	if (uap->sbytes != NULL)
		(void)copyout(&sbytes, uap->sbytes, sizeof(off_t));

out:
	if (hdr_uio)
		freeuio(hdr_uio);
	if (trl_uio)
		freeuio(trl_uio);
	return (error);
}

#ifdef COMPAT_FREEBSD4
int
freebsd4_freebsd32_sendfile(struct thread *td,
    struct freebsd4_freebsd32_sendfile_args *uap)
{
	return (freebsd32_do_sendfile(td,
	    (struct freebsd32_sendfile_args *)uap, 1));
}
#endif

int
freebsd32_sendfile(struct thread *td, struct freebsd32_sendfile_args *uap)
{

	return (freebsd32_do_sendfile(td, uap, 0));
}

static void
copy_stat(struct stat *in, struct stat32 *out)
{

#ifndef __amd64__
	/*
	 * 32-bit architectures other than i386 have 64-bit time_t.  This
	 * results in struct timespec32 with 12 bytes for tv_sec and tv_nsec,
	 * and 4 bytes of padding.  Zero the padding holes in struct stat32.
	 */
	bzero(&out->st_atim, sizeof(out->st_atim));
	bzero(&out->st_mtim, sizeof(out->st_mtim));
	bzero(&out->st_ctim, sizeof(out->st_ctim));
	bzero(&out->st_birthtim, sizeof(out->st_birthtim));
#endif
	CP(*in, *out, st_dev);
	CP(*in, *out, st_ino);
	CP(*in, *out, st_mode);
	CP(*in, *out, st_nlink);
	CP(*in, *out, st_uid);
	CP(*in, *out, st_gid);
	CP(*in, *out, st_rdev);
	TS_CP(*in, *out, st_atim);
	TS_CP(*in, *out, st_mtim);
	TS_CP(*in, *out, st_ctim);
	CP(*in, *out, st_size);
	CP(*in, *out, st_blocks);
	CP(*in, *out, st_blksize);
	CP(*in, *out, st_flags);
	CP(*in, *out, st_gen);
	CP(*in, *out, st_filerev);
	CP(*in, *out, st_bsdflags);
	TS_CP(*in, *out, st_birthtim);
	out->st_padding1 = 0;
#ifdef __STAT32_TIME_T_EXT
	out->st_atim_ext = 0;
	out->st_mtim_ext = 0;
	out->st_ctim_ext = 0;
	out->st_btim_ext = 0;
#endif
	bzero(out->st_spare, sizeof(out->st_spare));
}

#ifdef COMPAT_43
static void
copy_ostat(struct stat *in, struct ostat32 *out)
{

	bzero(out, sizeof(*out));
	CP(*in, *out, st_dev);
	CP(*in, *out, st_ino);
	CP(*in, *out, st_mode);
	CP(*in, *out, st_nlink);
	CP(*in, *out, st_uid);
	CP(*in, *out, st_gid);
	CP(*in, *out, st_rdev);
	out->st_size = MIN(in->st_size, INT32_MAX);
	TS_CP(*in, *out, st_atim);
	TS_CP(*in, *out, st_mtim);
	TS_CP(*in, *out, st_ctim);
	CP(*in, *out, st_blksize);
	CP(*in, *out, st_blocks);
	CP(*in, *out, st_flags);
	CP(*in, *out, st_gen);
}
#endif

#ifdef COMPAT_43
int
ofreebsd32_stat(struct thread *td, struct ofreebsd32_stat_args *uap)
{
	struct stat sb;
	struct ostat32 sb32;
	int error;

	error = kern_statat(td, 0, AT_FDCWD, uap->path, UIO_USERSPACE, &sb);
	if (error)
		return (error);
	copy_ostat(&sb, &sb32);
	error = copyout(&sb32, uap->ub, sizeof (sb32));
	return (error);
}
#endif

int
freebsd32_fstat(struct thread *td, struct freebsd32_fstat_args *uap)
{
	struct stat ub;
	struct stat32 ub32;
	int error;

	error = kern_fstat(td, uap->fd, &ub);
	if (error)
		return (error);
	copy_stat(&ub, &ub32);
	error = copyout(&ub32, uap->sb, sizeof(ub32));
	return (error);
}

#ifdef COMPAT_43
int
ofreebsd32_fstat(struct thread *td, struct ofreebsd32_fstat_args *uap)
{
	struct stat ub;
	struct ostat32 ub32;
	int error;

	error = kern_fstat(td, uap->fd, &ub);
	if (error)
		return (error);
	copy_ostat(&ub, &ub32);
	error = copyout(&ub32, uap->sb, sizeof(ub32));
	return (error);
}
#endif

int
freebsd32_fstatat(struct thread *td, struct freebsd32_fstatat_args *uap)
{
	struct stat ub;
	struct stat32 ub32;
	int error;

	error = kern_statat(td, uap->flag, uap->fd, uap->path, UIO_USERSPACE,
	    &ub);
	if (error)
		return (error);
	copy_stat(&ub, &ub32);
	error = copyout(&ub32, uap->buf, sizeof(ub32));
	return (error);
}

#ifdef COMPAT_43
int
ofreebsd32_lstat(struct thread *td, struct ofreebsd32_lstat_args *uap)
{
	struct stat sb;
	struct ostat32 sb32;
	int error;

	error = kern_statat(td, AT_SYMLINK_NOFOLLOW, AT_FDCWD, uap->path,
	    UIO_USERSPACE, &sb);
	if (error)
		return (error);
	copy_ostat(&sb, &sb32);
	error = copyout(&sb32, uap->ub, sizeof (sb32));
	return (error);
}
#endif

int
freebsd32_fhstat(struct thread *td, struct freebsd32_fhstat_args *uap)
{
	struct stat sb;
	struct stat32 sb32;
	struct fhandle fh;
	int error;

	error = copyin(uap->u_fhp, &fh, sizeof(fhandle_t));
        if (error != 0)
                return (error);
	error = kern_fhstat(td, fh, &sb);
	if (error != 0)
		return (error);
	copy_stat(&sb, &sb32);
	error = copyout(&sb32, uap->sb, sizeof (sb32));
	return (error);
}

#if defined(COMPAT_FREEBSD11)
extern int ino64_trunc_error;

static int
freebsd11_cvtstat32(struct stat *in, struct freebsd11_stat32 *out)
{

#ifndef __amd64__
	/*
	 * 32-bit architectures other than i386 have 64-bit time_t.  This
	 * results in struct timespec32 with 12 bytes for tv_sec and tv_nsec,
	 * and 4 bytes of padding.  Zero the padding holes in freebsd11_stat32.
	 */
	bzero(&out->st_atim, sizeof(out->st_atim));
	bzero(&out->st_mtim, sizeof(out->st_mtim));
	bzero(&out->st_ctim, sizeof(out->st_ctim));
	bzero(&out->st_birthtim, sizeof(out->st_birthtim));
#endif

	CP(*in, *out, st_ino);
	if (in->st_ino != out->st_ino) {
		switch (ino64_trunc_error) {
		default:
		case 0:
			break;
		case 1:
			return (EOVERFLOW);
		case 2:
			out->st_ino = UINT32_MAX;
			break;
		}
	}
	CP(*in, *out, st_nlink);
	if (in->st_nlink != out->st_nlink) {
		switch (ino64_trunc_error) {
		default:
		case 0:
			break;
		case 1:
			return (EOVERFLOW);
		case 2:
			out->st_nlink = UINT16_MAX;
			break;
		}
	}
	out->st_dev = in->st_dev;
	if (out->st_dev != in->st_dev) {
		switch (ino64_trunc_error) {
		default:
			break;
		case 1:
			return (EOVERFLOW);
		}
	}
	CP(*in, *out, st_mode);
	CP(*in, *out, st_uid);
	CP(*in, *out, st_gid);
	out->st_rdev = in->st_rdev;
	if (out->st_rdev != in->st_rdev) {
		switch (ino64_trunc_error) {
		default:
			break;
		case 1:
			return (EOVERFLOW);
		}
	}
	TS_CP(*in, *out, st_atim);
	TS_CP(*in, *out, st_mtim);
	TS_CP(*in, *out, st_ctim);
	CP(*in, *out, st_size);
	CP(*in, *out, st_blocks);
	CP(*in, *out, st_blksize);
	CP(*in, *out, st_flags);
	CP(*in, *out, st_gen);
	TS_CP(*in, *out, st_birthtim);
	out->st_lspare = 0;
	bzero((char *)&out->st_birthtim + sizeof(out->st_birthtim),
	    sizeof(*out) - offsetof(struct freebsd11_stat32,
	    st_birthtim) - sizeof(out->st_birthtim));
	return (0);
}

int
freebsd11_freebsd32_stat(struct thread *td,
    struct freebsd11_freebsd32_stat_args *uap)
{
	struct stat sb;
	struct freebsd11_stat32 sb32;
	int error;

	error = kern_statat(td, 0, AT_FDCWD, uap->path, UIO_USERSPACE, &sb);
	if (error != 0)
		return (error);
	error = freebsd11_cvtstat32(&sb, &sb32);
	if (error == 0)
		error = copyout(&sb32, uap->ub, sizeof (sb32));
	return (error);
}

int
freebsd11_freebsd32_fstat(struct thread *td,
    struct freebsd11_freebsd32_fstat_args *uap)
{
	struct stat sb;
	struct freebsd11_stat32 sb32;
	int error;

	error = kern_fstat(td, uap->fd, &sb);
	if (error != 0)
		return (error);
	error = freebsd11_cvtstat32(&sb, &sb32);
	if (error == 0)
		error = copyout(&sb32, uap->sb, sizeof (sb32));
	return (error);
}

int
freebsd11_freebsd32_fstatat(struct thread *td,
    struct freebsd11_freebsd32_fstatat_args *uap)
{
	struct stat sb;
	struct freebsd11_stat32 sb32;
	int error;

	error = kern_statat(td, uap->flag, uap->fd, uap->path, UIO_USERSPACE,
	    &sb);
	if (error != 0)
		return (error);
	error = freebsd11_cvtstat32(&sb, &sb32);
	if (error == 0)
		error = copyout(&sb32, uap->buf, sizeof (sb32));
	return (error);
}

int
freebsd11_freebsd32_lstat(struct thread *td,
    struct freebsd11_freebsd32_lstat_args *uap)
{
	struct stat sb;
	struct freebsd11_stat32 sb32;
	int error;

	error = kern_statat(td, AT_SYMLINK_NOFOLLOW, AT_FDCWD, uap->path,
	    UIO_USERSPACE, &sb);
	if (error != 0)
		return (error);
	error = freebsd11_cvtstat32(&sb, &sb32);
	if (error == 0)
		error = copyout(&sb32, uap->ub, sizeof (sb32));
	return (error);
}

int
freebsd11_freebsd32_fhstat(struct thread *td,
    struct freebsd11_freebsd32_fhstat_args *uap)
{
	struct stat sb;
	struct freebsd11_stat32 sb32;
	struct fhandle fh;
	int error;

	error = copyin(uap->u_fhp, &fh, sizeof(fhandle_t));
        if (error != 0)
                return (error);
	error = kern_fhstat(td, fh, &sb);
	if (error != 0)
		return (error);
	error = freebsd11_cvtstat32(&sb, &sb32);
	if (error == 0)
		error = copyout(&sb32, uap->sb, sizeof (sb32));
	return (error);
}

static int
freebsd11_cvtnstat32(struct stat *sb, struct nstat32 *nsb32)
{
	struct nstat nsb;
	int error;

	error = freebsd11_cvtnstat(sb, &nsb);
	if (error != 0)
		return (error);

	bzero(nsb32, sizeof(*nsb32));
	CP(nsb, *nsb32, st_dev);
	CP(nsb, *nsb32, st_ino);
	CP(nsb, *nsb32, st_mode);
	CP(nsb, *nsb32, st_nlink);
	CP(nsb, *nsb32, st_uid);
	CP(nsb, *nsb32, st_gid);
	CP(nsb, *nsb32, st_rdev);
	CP(nsb, *nsb32, st_atim.tv_sec);
	CP(nsb, *nsb32, st_atim.tv_nsec);
	CP(nsb, *nsb32, st_mtim.tv_sec);
	CP(nsb, *nsb32, st_mtim.tv_nsec);
	CP(nsb, *nsb32, st_ctim.tv_sec);
	CP(nsb, *nsb32, st_ctim.tv_nsec);
	CP(nsb, *nsb32, st_size);
	CP(nsb, *nsb32, st_blocks);
	CP(nsb, *nsb32, st_blksize);
	CP(nsb, *nsb32, st_flags);
	CP(nsb, *nsb32, st_gen);
	CP(nsb, *nsb32, st_birthtim.tv_sec);
	CP(nsb, *nsb32, st_birthtim.tv_nsec);
	return (0);
}

int
freebsd11_freebsd32_nstat(struct thread *td,
    struct freebsd11_freebsd32_nstat_args *uap)
{
	struct stat sb;
	struct nstat32 nsb;
	int error;

	error = kern_statat(td, 0, AT_FDCWD, uap->path, UIO_USERSPACE, &sb);
	if (error != 0)
		return (error);
	error = freebsd11_cvtnstat32(&sb, &nsb);
	if (error != 0)
		error = copyout(&nsb, uap->ub, sizeof (nsb));
	return (error);
}

int
freebsd11_freebsd32_nlstat(struct thread *td,
    struct freebsd11_freebsd32_nlstat_args *uap)
{
	struct stat sb;
	struct nstat32 nsb;
	int error;

	error = kern_statat(td, AT_SYMLINK_NOFOLLOW, AT_FDCWD, uap->path,
	    UIO_USERSPACE, &sb);
	if (error != 0)
		return (error);
	error = freebsd11_cvtnstat32(&sb, &nsb);
	if (error == 0)
		error = copyout(&nsb, uap->ub, sizeof (nsb));
	return (error);
}

int
freebsd11_freebsd32_nfstat(struct thread *td,
    struct freebsd11_freebsd32_nfstat_args *uap)
{
	struct nstat32 nub;
	struct stat ub;
	int error;

	error = kern_fstat(td, uap->fd, &ub);
	if (error != 0)
		return (error);
	error = freebsd11_cvtnstat32(&ub, &nub);
	if (error == 0)
		error = copyout(&nub, uap->sb, sizeof(nub));
	return (error);
}
#endif

int
freebsd32___sysctl(struct thread *td, struct freebsd32___sysctl_args *uap)
{
	int error, name[CTL_MAXNAME];
	size_t j, oldlen;
	uint32_t tmp;

	if (uap->namelen > CTL_MAXNAME || uap->namelen < 2)
		return (EINVAL);
 	error = copyin(uap->name, name, uap->namelen * sizeof(int));
 	if (error)
		return (error);
	if (uap->oldlenp) {
		error = fueword32(uap->oldlenp, &tmp);
		oldlen = tmp;
	} else {
		oldlen = 0;
	}
	if (error != 0)
		return (EFAULT);
	error = userland_sysctl(td, name, uap->namelen,
		uap->old, &oldlen, 1,
		uap->new, uap->newlen, &j, SCTL_MASK32);
	if (error)
		return (error);
	if (uap->oldlenp != NULL && suword32(uap->oldlenp, j) != 0)
		error = EFAULT;
	return (error);
}

int
freebsd32___sysctlbyname(struct thread *td,
    struct freebsd32___sysctlbyname_args *uap)
{
	size_t oldlen, rv;
	int error;
	uint32_t tmp;

	if (uap->oldlenp != NULL) {
		error = fueword32(uap->oldlenp, &tmp);
		oldlen = tmp;
	} else {
		error = oldlen = 0;
	}
	if (error != 0)
		return (EFAULT);
	error = kern___sysctlbyname(td, uap->name, uap->namelen, uap->old,
	    &oldlen, uap->new, uap->newlen, &rv, SCTL_MASK32, 1);
	if (error != 0)
		return (error);
	if (uap->oldlenp != NULL && suword32(uap->oldlenp, rv) != 0)
		error = EFAULT;
	return (error);
}

int
freebsd32_jail(struct thread *td, struct freebsd32_jail_args *uap)
{
	uint32_t version;
	int error;
	struct jail j;

	error = copyin(uap->jail, &version, sizeof(uint32_t));
	if (error)
		return (error);

	switch (version) {
	case 0:
	{
		/* FreeBSD single IPv4 jails. */
		struct jail32_v0 j32_v0;

		bzero(&j, sizeof(struct jail));
		error = copyin(uap->jail, &j32_v0, sizeof(struct jail32_v0));
		if (error)
			return (error);
		CP(j32_v0, j, version);
		PTRIN_CP(j32_v0, j, path);
		PTRIN_CP(j32_v0, j, hostname);
		j.ip4s = htonl(j32_v0.ip_number);	/* jail_v0 is host order */
		break;
	}

	case 1:
		/*
		 * Version 1 was used by multi-IPv4 jail implementations
		 * that never made it into the official kernel.
		 */
		return (EINVAL);

	case 2:	/* JAIL_API_VERSION */
	{
		/* FreeBSD multi-IPv4/IPv6,noIP jails. */
		struct jail32 j32;

		error = copyin(uap->jail, &j32, sizeof(struct jail32));
		if (error)
			return (error);
		CP(j32, j, version);
		PTRIN_CP(j32, j, path);
		PTRIN_CP(j32, j, hostname);
		PTRIN_CP(j32, j, jailname);
		CP(j32, j, ip4s);
		CP(j32, j, ip6s);
		PTRIN_CP(j32, j, ip4);
		PTRIN_CP(j32, j, ip6);
		break;
	}

	default:
		/* Sci-Fi jails are not supported, sorry. */
		return (EINVAL);
	}
	return (kern_jail(td, &j));
}

int
freebsd32_jail_set(struct thread *td, struct freebsd32_jail_set_args *uap)
{
	struct uio *auio;
	int error;

	/* Check that we have an even number of iovecs. */
	if (uap->iovcnt & 1)
		return (EINVAL);

	error = freebsd32_copyinuio(uap->iovp, uap->iovcnt, &auio);
	if (error)
		return (error);
	error = kern_jail_set(td, auio, uap->flags);
	freeuio(auio);
	return (error);
}

int
freebsd32_jail_get(struct thread *td, struct freebsd32_jail_get_args *uap)
{
	struct iovec32 iov32;
	struct uio *auio;
	int error, i;

	/* Check that we have an even number of iovecs. */
	if (uap->iovcnt & 1)
		return (EINVAL);

	error = freebsd32_copyinuio(uap->iovp, uap->iovcnt, &auio);
	if (error)
		return (error);
	error = kern_jail_get(td, auio, uap->flags);
	if (error == 0)
		for (i = 0; i < uap->iovcnt; i++) {
			PTROUT_CP(auio->uio_iov[i], iov32, iov_base);
			CP(auio->uio_iov[i], iov32, iov_len);
			error = copyout(&iov32, uap->iovp + i, sizeof(iov32));
			if (error != 0)
				break;
		}
	freeuio(auio);
	return (error);
}

int
freebsd32_sigaction(struct thread *td, struct freebsd32_sigaction_args *uap)
{
	struct sigaction32 s32;
	struct sigaction sa, osa, *sap;
	int error;

	if (uap->act) {
		error = copyin(uap->act, &s32, sizeof(s32));
		if (error)
			return (error);
		sa.sa_handler = PTRIN(s32.sa_u);
		CP(s32, sa, sa_flags);
		CP(s32, sa, sa_mask);
		sap = &sa;
	} else
		sap = NULL;
	error = kern_sigaction(td, uap->sig, sap, &osa, 0);
	if (error == 0 && uap->oact != NULL) {
		s32.sa_u = PTROUT(osa.sa_handler);
		CP(osa, s32, sa_flags);
		CP(osa, s32, sa_mask);
		error = copyout(&s32, uap->oact, sizeof(s32));
	}
	return (error);
}

#ifdef COMPAT_FREEBSD4
int
freebsd4_freebsd32_sigaction(struct thread *td,
			     struct freebsd4_freebsd32_sigaction_args *uap)
{
	struct sigaction32 s32;
	struct sigaction sa, osa, *sap;
	int error;

	if (uap->act) {
		error = copyin(uap->act, &s32, sizeof(s32));
		if (error)
			return (error);
		sa.sa_handler = PTRIN(s32.sa_u);
		CP(s32, sa, sa_flags);
		CP(s32, sa, sa_mask);
		sap = &sa;
	} else
		sap = NULL;
	error = kern_sigaction(td, uap->sig, sap, &osa, KSA_FREEBSD4);
	if (error == 0 && uap->oact != NULL) {
		s32.sa_u = PTROUT(osa.sa_handler);
		CP(osa, s32, sa_flags);
		CP(osa, s32, sa_mask);
		error = copyout(&s32, uap->oact, sizeof(s32));
	}
	return (error);
}
#endif

#ifdef COMPAT_43
struct osigaction32 {
	uint32_t	sa_u;
	osigset_t	sa_mask;
	int		sa_flags;
};

#define	ONSIG	32

int
ofreebsd32_sigaction(struct thread *td,
			     struct ofreebsd32_sigaction_args *uap)
{
	struct osigaction32 s32;
	struct sigaction sa, osa, *sap;
	int error;

	if (uap->signum <= 0 || uap->signum >= ONSIG)
		return (EINVAL);

	if (uap->nsa) {
		error = copyin(uap->nsa, &s32, sizeof(s32));
		if (error)
			return (error);
		sa.sa_handler = PTRIN(s32.sa_u);
		CP(s32, sa, sa_flags);
		OSIG2SIG(s32.sa_mask, sa.sa_mask);
		sap = &sa;
	} else
		sap = NULL;
	error = kern_sigaction(td, uap->signum, sap, &osa, KSA_OSIGSET);
	if (error == 0 && uap->osa != NULL) {
		s32.sa_u = PTROUT(osa.sa_handler);
		CP(osa, s32, sa_flags);
		SIG2OSIG(osa.sa_mask, s32.sa_mask);
		error = copyout(&s32, uap->osa, sizeof(s32));
	}
	return (error);
}

struct sigvec32 {
	uint32_t	sv_handler;
	int		sv_mask;
	int		sv_flags;
};

int
ofreebsd32_sigvec(struct thread *td,
			  struct ofreebsd32_sigvec_args *uap)
{
	struct sigvec32 vec;
	struct sigaction sa, osa, *sap;
	int error;

	if (uap->signum <= 0 || uap->signum >= ONSIG)
		return (EINVAL);

	if (uap->nsv) {
		error = copyin(uap->nsv, &vec, sizeof(vec));
		if (error)
			return (error);
		sa.sa_handler = PTRIN(vec.sv_handler);
		OSIG2SIG(vec.sv_mask, sa.sa_mask);
		sa.sa_flags = vec.sv_flags;
		sa.sa_flags ^= SA_RESTART;
		sap = &sa;
	} else
		sap = NULL;
	error = kern_sigaction(td, uap->signum, sap, &osa, KSA_OSIGSET);
	if (error == 0 && uap->osv != NULL) {
		vec.sv_handler = PTROUT(osa.sa_handler);
		SIG2OSIG(osa.sa_mask, vec.sv_mask);
		vec.sv_flags = osa.sa_flags;
		vec.sv_flags &= ~SA_NOCLDWAIT;
		vec.sv_flags ^= SA_RESTART;
		error = copyout(&vec, uap->osv, sizeof(vec));
	}
	return (error);
}

struct sigstack32 {
	uint32_t	ss_sp;
	int		ss_onstack;
};

int
ofreebsd32_sigstack(struct thread *td,
			    struct ofreebsd32_sigstack_args *uap)
{
	struct sigstack32 s32;
	struct sigstack nss, oss;
	int error = 0, unss;

	if (uap->nss != NULL) {
		error = copyin(uap->nss, &s32, sizeof(s32));
		if (error)
			return (error);
		nss.ss_sp = PTRIN(s32.ss_sp);
		CP(s32, nss, ss_onstack);
		unss = 1;
	} else {
		unss = 0;
	}
	oss.ss_sp = td->td_sigstk.ss_sp;
	oss.ss_onstack = sigonstack(cpu_getstack(td));
	if (unss) {
		td->td_sigstk.ss_sp = nss.ss_sp;
		td->td_sigstk.ss_size = 0;
		td->td_sigstk.ss_flags |= (nss.ss_onstack & SS_ONSTACK);
		td->td_pflags |= TDP_ALTSTACK;
	}
	if (uap->oss != NULL) {
		s32.ss_sp = PTROUT(oss.ss_sp);
		CP(oss, s32, ss_onstack);
		error = copyout(&s32, uap->oss, sizeof(s32));
	}
	return (error);
}
#endif

int
freebsd32_nanosleep(struct thread *td, struct freebsd32_nanosleep_args *uap)
{

	return (freebsd32_user_clock_nanosleep(td, CLOCK_REALTIME,
	    TIMER_RELTIME, uap->rqtp, uap->rmtp));
}

int
freebsd32_clock_nanosleep(struct thread *td,
    struct freebsd32_clock_nanosleep_args *uap)
{
	int error;

	error = freebsd32_user_clock_nanosleep(td, uap->clock_id, uap->flags,
	    uap->rqtp, uap->rmtp);
	return (kern_posix_error(td, error));
}

static int
freebsd32_user_clock_nanosleep(struct thread *td, clockid_t clock_id,
    int flags, const struct timespec32 *ua_rqtp, struct timespec32 *ua_rmtp)
{
	struct timespec32 rmt32, rqt32;
	struct timespec rmt, rqt;
	int error, error2;

	error = copyin(ua_rqtp, &rqt32, sizeof(rqt32));
	if (error)
		return (error);

	CP(rqt32, rqt, tv_sec);
	CP(rqt32, rqt, tv_nsec);

	error = kern_clock_nanosleep(td, clock_id, flags, &rqt, &rmt);
	if (error == EINTR && ua_rmtp != NULL && (flags & TIMER_ABSTIME) == 0) {
		CP(rmt, rmt32, tv_sec);
		CP(rmt, rmt32, tv_nsec);

		error2 = copyout(&rmt32, ua_rmtp, sizeof(rmt32));
		if (error2 != 0)
			error = error2;
	}
	return (error);
}

int
freebsd32_clock_gettime(struct thread *td,
			struct freebsd32_clock_gettime_args *uap)
{
	struct timespec	ats;
	struct timespec32 ats32;
	int error;

	error = kern_clock_gettime(td, uap->clock_id, &ats);
	if (error == 0) {
		CP(ats, ats32, tv_sec);
		CP(ats, ats32, tv_nsec);
		error = copyout(&ats32, uap->tp, sizeof(ats32));
	}
	return (error);
}

int
freebsd32_clock_settime(struct thread *td,
			struct freebsd32_clock_settime_args *uap)
{
	struct timespec	ats;
	struct timespec32 ats32;
	int error;

	error = copyin(uap->tp, &ats32, sizeof(ats32));
	if (error)
		return (error);
	CP(ats32, ats, tv_sec);
	CP(ats32, ats, tv_nsec);

	return (kern_clock_settime(td, uap->clock_id, &ats));
}

int
freebsd32_clock_getres(struct thread *td,
		       struct freebsd32_clock_getres_args *uap)
{
	struct timespec	ts;
	struct timespec32 ts32;
	int error;

	if (uap->tp == NULL)
		return (0);
	error = kern_clock_getres(td, uap->clock_id, &ts);
	if (error == 0) {
		CP(ts, ts32, tv_sec);
		CP(ts, ts32, tv_nsec);
		error = copyout(&ts32, uap->tp, sizeof(ts32));
	}
	return (error);
}

int freebsd32_ktimer_create(struct thread *td,
    struct freebsd32_ktimer_create_args *uap)
{
	struct sigevent32 ev32;
	struct sigevent ev, *evp;
	int error, id;

	if (uap->evp == NULL) {
		evp = NULL;
	} else {
		evp = &ev;
		error = copyin(uap->evp, &ev32, sizeof(ev32));
		if (error != 0)
			return (error);
		error = convert_sigevent32(&ev32, &ev);
		if (error != 0)
			return (error);
	}
	error = kern_ktimer_create(td, uap->clock_id, evp, &id, -1);
	if (error == 0) {
		error = copyout(&id, uap->timerid, sizeof(int));
		if (error != 0)
			kern_ktimer_delete(td, id);
	}
	return (error);
}

int
freebsd32_ktimer_settime(struct thread *td,
    struct freebsd32_ktimer_settime_args *uap)
{
	struct itimerspec32 val32, oval32;
	struct itimerspec val, oval, *ovalp;
	int error;

	error = copyin(uap->value, &val32, sizeof(val32));
	if (error != 0)
		return (error);
	ITS_CP(val32, val);
	ovalp = uap->ovalue != NULL ? &oval : NULL;
	error = kern_ktimer_settime(td, uap->timerid, uap->flags, &val, ovalp);
	if (error == 0 && uap->ovalue != NULL) {
		ITS_CP(oval, oval32);
		error = copyout(&oval32, uap->ovalue, sizeof(oval32));
	}
	return (error);
}

int
freebsd32_ktimer_gettime(struct thread *td,
    struct freebsd32_ktimer_gettime_args *uap)
{
	struct itimerspec32 val32;
	struct itimerspec val;
	int error;

	error = kern_ktimer_gettime(td, uap->timerid, &val);
	if (error == 0) {
		ITS_CP(val, val32);
		error = copyout(&val32, uap->value, sizeof(val32));
	}
	return (error);
}

int
freebsd32_timerfd_gettime(struct thread *td,
    struct freebsd32_timerfd_gettime_args *uap)
{
	struct itimerspec curr_value;
	struct itimerspec32 curr_value32;
	int error;

	error = kern_timerfd_gettime(td, uap->fd, &curr_value);
	if (error == 0) {
		CP(curr_value, curr_value32, it_value.tv_sec);
		CP(curr_value, curr_value32, it_value.tv_nsec);
		CP(curr_value, curr_value32, it_interval.tv_sec);
		CP(curr_value, curr_value32, it_interval.tv_nsec);
		error = copyout(&curr_value32, uap->curr_value,
		    sizeof(curr_value32));
	}

	return (error);
}

int
freebsd32_timerfd_settime(struct thread *td,
    struct freebsd32_timerfd_settime_args *uap)
{
	struct itimerspec new_value, old_value;
	struct itimerspec32 new_value32, old_value32;
	int error;

	error = copyin(uap->new_value, &new_value32, sizeof(new_value32));
	if (error != 0)
		return (error);
	CP(new_value32, new_value, it_value.tv_sec);
	CP(new_value32, new_value, it_value.tv_nsec);
	CP(new_value32, new_value, it_interval.tv_sec);
	CP(new_value32, new_value, it_interval.tv_nsec);
	if (uap->old_value == NULL) {
		error = kern_timerfd_settime(td, uap->fd, uap->flags,
		    &new_value, NULL);
	} else {
		error = kern_timerfd_settime(td, uap->fd, uap->flags,
		    &new_value, &old_value);
		if (error == 0) {
			CP(old_value, old_value32, it_value.tv_sec);
			CP(old_value, old_value32, it_value.tv_nsec);
			CP(old_value, old_value32, it_interval.tv_sec);
			CP(old_value, old_value32, it_interval.tv_nsec);
			error = copyout(&old_value32, uap->old_value,
			    sizeof(old_value32));
		}
	}
	return (error);
}

int
freebsd32_clock_getcpuclockid2(struct thread *td,
    struct freebsd32_clock_getcpuclockid2_args *uap)
{
	clockid_t clk_id;
	int error;

	error = kern_clock_getcpuclockid2(td, PAIR32TO64(id_t, uap->id),
	    uap->which, &clk_id);
	if (error == 0)
		error = copyout(&clk_id, uap->clock_id, sizeof(clockid_t));
	return (error);
}

int
freebsd32_thr_new(struct thread *td,
		  struct freebsd32_thr_new_args *uap)
{
	struct thr_param32 param32;
	struct thr_param param;
	int error;

	if (uap->param_size < 0 ||
	    uap->param_size > sizeof(struct thr_param32))
		return (EINVAL);
	bzero(&param, sizeof(struct thr_param));
	bzero(&param32, sizeof(struct thr_param32));
	error = copyin(uap->param, &param32, uap->param_size);
	if (error != 0)
		return (error);
	param.start_func = PTRIN(param32.start_func);
	param.arg = PTRIN(param32.arg);
	param.stack_base = PTRIN(param32.stack_base);
	param.stack_size = param32.stack_size;
	param.tls_base = PTRIN(param32.tls_base);
	param.tls_size = param32.tls_size;
	param.child_tid = PTRIN(param32.child_tid);
	param.parent_tid = PTRIN(param32.parent_tid);
	param.flags = param32.flags;
	param.rtp = PTRIN(param32.rtp);
	param.spare[0] = PTRIN(param32.spare[0]);
	param.spare[1] = PTRIN(param32.spare[1]);
	param.spare[2] = PTRIN(param32.spare[2]);

	return (kern_thr_new(td, &param));
}

int
freebsd32_thr_suspend(struct thread *td, struct freebsd32_thr_suspend_args *uap)
{
	struct timespec32 ts32;
	struct timespec ts, *tsp;
	int error;

	error = 0;
	tsp = NULL;
	if (uap->timeout != NULL) {
		error = copyin((const void *)uap->timeout, (void *)&ts32,
		    sizeof(struct timespec32));
		if (error != 0)
			return (error);
		ts.tv_sec = ts32.tv_sec;
		ts.tv_nsec = ts32.tv_nsec;
		tsp = &ts;
	}
	return (kern_thr_suspend(td, tsp));
}

void
siginfo_to_siginfo32(const siginfo_t *src, struct __siginfo32 *dst)
{
	bzero(dst, sizeof(*dst));
	dst->si_signo = src->si_signo;
	dst->si_errno = src->si_errno;
	dst->si_code = src->si_code;
	dst->si_pid = src->si_pid;
	dst->si_uid = src->si_uid;
	dst->si_status = src->si_status;
	dst->si_addr = (uintptr_t)src->si_addr;
	dst->si_value.sival_int = src->si_value.sival_int;
	dst->si_timerid = src->si_timerid;
	dst->si_overrun = src->si_overrun;
}

#ifndef _FREEBSD32_SYSPROTO_H_
struct freebsd32_sigqueue_args {
        pid_t pid;
        int signum;
        /* union sigval32 */ int value;
};
#endif
int
freebsd32_sigqueue(struct thread *td, struct freebsd32_sigqueue_args *uap)
{
	union sigval sv;

	/*
	 * On 32-bit ABIs, sival_int and sival_ptr are the same.
	 * On 64-bit little-endian ABIs, the low bits are the same.
	 * In 64-bit big-endian ABIs, sival_int overlaps with
	 * sival_ptr's HIGH bits.  We choose to support sival_int
	 * rather than sival_ptr in this case as it seems to be
	 * more common.
	 */
	bzero(&sv, sizeof(sv));
	sv.sival_int = (uint32_t)(uint64_t)uap->value;

	return (kern_sigqueue(td, uap->pid, uap->signum, &sv));
}

int
freebsd32_sigtimedwait(struct thread *td, struct freebsd32_sigtimedwait_args *uap)
{
	struct timespec32 ts32;
	struct timespec ts;
	struct timespec *timeout;
	sigset_t set;
	ksiginfo_t ksi;
	struct __siginfo32 si32;
	int error;

	if (uap->timeout) {
		error = copyin(uap->timeout, &ts32, sizeof(ts32));
		if (error)
			return (error);
		ts.tv_sec = ts32.tv_sec;
		ts.tv_nsec = ts32.tv_nsec;
		timeout = &ts;
	} else
		timeout = NULL;

	error = copyin(uap->set, &set, sizeof(set));
	if (error)
		return (error);

	error = kern_sigtimedwait(td, set, &ksi, timeout);
	if (error)
		return (error);

	if (uap->info) {
		siginfo_to_siginfo32(&ksi.ksi_info, &si32);
		error = copyout(&si32, uap->info, sizeof(struct __siginfo32));
	}

	if (error == 0)
		td->td_retval[0] = ksi.ksi_signo;
	return (error);
}

/*
 * MPSAFE
 */
int
freebsd32_sigwaitinfo(struct thread *td, struct freebsd32_sigwaitinfo_args *uap)
{
	ksiginfo_t ksi;
	struct __siginfo32 si32;
	sigset_t set;
	int error;

	error = copyin(uap->set, &set, sizeof(set));
	if (error)
		return (error);

	error = kern_sigtimedwait(td, set, &ksi, NULL);
	if (error)
		return (error);

	if (uap->info) {
		siginfo_to_siginfo32(&ksi.ksi_info, &si32);
		error = copyout(&si32, uap->info, sizeof(struct __siginfo32));
	}	
	if (error == 0)
		td->td_retval[0] = ksi.ksi_signo;
	return (error);
}

int
freebsd32_cpuset_setid(struct thread *td,
    struct freebsd32_cpuset_setid_args *uap)
{

	return (kern_cpuset_setid(td, uap->which,
	    PAIR32TO64(id_t, uap->id), uap->setid));
}

int
freebsd32_cpuset_getid(struct thread *td,
    struct freebsd32_cpuset_getid_args *uap)
{

	return (kern_cpuset_getid(td, uap->level, uap->which,
	    PAIR32TO64(id_t, uap->id), uap->setid));
}

static int
copyin32_set(const void *u, void *k, size_t size)
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	int rv;
	struct bitset *kb = k;
	int *p;

	rv = copyin(u, k, size);
	if (rv != 0)
		return (rv);

	p = (int *)kb->__bits;
	/* Loop through swapping words.
	 * `size' is in bytes, we need bits. */
	for (int i = 0; i < __bitset_words(size * 8); i++) {
		int tmp = p[0];
		p[0] = p[1];
		p[1] = tmp;
		p += 2;
	}
	return (0);
#else
	return (copyin(u, k, size));
#endif
}

static int
copyout32_set(const void *k, void *u, size_t size)
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	const struct bitset *kb = k;
	struct bitset *ub = u;
	const int *kp = (const int *)kb->__bits;
	int *up = (int *)ub->__bits;
	int rv;

	for (int i = 0; i < __bitset_words(CPU_SETSIZE); i++) {
		/* `size' is in bytes, we need bits. */
		for (int i = 0; i < __bitset_words(size * 8); i++) {
			rv = suword32(up, kp[1]);
			if (rv == 0)
				rv = suword32(up + 1, kp[0]);
			if (rv != 0)
				return (EFAULT);
		}
	}
	return (0);
#else
	return (copyout(k, u, size));
#endif
}

static const struct cpuset_copy_cb cpuset_copy32_cb = {
	.cpuset_copyin = copyin32_set,
	.cpuset_copyout = copyout32_set
};

int
freebsd32_cpuset_getaffinity(struct thread *td,
    struct freebsd32_cpuset_getaffinity_args *uap)
{

	return (user_cpuset_getaffinity(td, uap->level, uap->which,
	    PAIR32TO64(id_t,uap->id), uap->cpusetsize, uap->mask,
	    &cpuset_copy32_cb));
}

int
freebsd32_cpuset_setaffinity(struct thread *td,
    struct freebsd32_cpuset_setaffinity_args *uap)
{

	return (user_cpuset_setaffinity(td, uap->level, uap->which,
	    PAIR32TO64(id_t,uap->id), uap->cpusetsize, uap->mask,
	    &cpuset_copy32_cb));
}

int
freebsd32_cpuset_getdomain(struct thread *td,
    struct freebsd32_cpuset_getdomain_args *uap)
{

	return (kern_cpuset_getdomain(td, uap->level, uap->which,
	    PAIR32TO64(id_t,uap->id), uap->domainsetsize, uap->mask, uap->policy,
	    &cpuset_copy32_cb));
}

int
freebsd32_cpuset_setdomain(struct thread *td,
    struct freebsd32_cpuset_setdomain_args *uap)
{

	return (kern_cpuset_setdomain(td, uap->level, uap->which,
	    PAIR32TO64(id_t,uap->id), uap->domainsetsize, uap->mask, uap->policy,
	    &cpuset_copy32_cb));
}

int
freebsd32_nmount(struct thread *td,
    struct freebsd32_nmount_args /* {
    	struct iovec *iovp;
    	unsigned int iovcnt;
    	int flags;
    } */ *uap)
{
	struct uio *auio;
	uint64_t flags;
	int error;

	/*
	 * Mount flags are now 64-bits. On 32-bit archtectures only
	 * 32-bits are passed in, but from here on everything handles
	 * 64-bit flags correctly.
	 */
	flags = uap->flags;

	AUDIT_ARG_FFLAGS(flags);

	/*
	 * Filter out MNT_ROOTFS.  We do not want clients of nmount() in
	 * userspace to set this flag, but we must filter it out if we want
	 * MNT_UPDATE on the root file system to work.
	 * MNT_ROOTFS should only be set by the kernel when mounting its
	 * root file system.
	 */
	flags &= ~MNT_ROOTFS;

	/*
	 * check that we have an even number of iovec's
	 * and that we have at least two options.
	 */
	if ((uap->iovcnt & 1) || (uap->iovcnt < 4))
		return (EINVAL);

	error = freebsd32_copyinuio(uap->iovp, uap->iovcnt, &auio);
	if (error)
		return (error);
	error = vfs_donmount(td, flags, auio);

	freeuio(auio);
	return error;
}

#if 0
int
freebsd32_xxx(struct thread *td, struct freebsd32_xxx_args *uap)
{
	struct yyy32 *p32, s32;
	struct yyy *p = NULL, s;
	struct xxx_arg ap;
	int error;

	if (uap->zzz) {
		error = copyin(uap->zzz, &s32, sizeof(s32));
		if (error)
			return (error);
		/* translate in */
		p = &s;
	}
	error = kern_xxx(td, p);
	if (error)
		return (error);
	if (uap->zzz) {
		/* translate out */
		error = copyout(&s32, p32, sizeof(s32));
	}
	return (error);
}
#endif

int
syscall32_module_handler(struct module *mod, int what, void *arg)
{

	return (kern_syscall_module_handler(freebsd32_sysent, mod, what, arg));
}

int
syscall32_helper_register(struct syscall_helper_data *sd, int flags)
{

	return (kern_syscall_helper_register(freebsd32_sysent, sd, flags));
}

int
syscall32_helper_unregister(struct syscall_helper_data *sd)
{

	return (kern_syscall_helper_unregister(freebsd32_sysent, sd));
}

int
freebsd32_copyout_strings(struct image_params *imgp, uintptr_t *stack_base)
{
	struct sysentvec *sysent;
	int argc, envc, i;
	uint32_t *vectp;
	char *stringp;
	uintptr_t destp, ustringp;
	struct freebsd32_ps_strings *arginfo;
	char canary[sizeof(long) * 8];
	int32_t pagesizes32[MAXPAGESIZES];
	size_t execpath_len;
	int error, szsigcode;

	sysent = imgp->sysent;

	arginfo = (struct freebsd32_ps_strings *)PROC_PS_STRINGS(imgp->proc);
	imgp->ps_strings = arginfo;
	destp =	(uintptr_t)arginfo;

	/*
	 * Install sigcode.
	 */
	if (!PROC_HAS_SHP(imgp->proc)) {
		szsigcode = *sysent->sv_szsigcode;
		destp -= szsigcode;
		destp = rounddown2(destp, sizeof(uint32_t));
		error = copyout(sysent->sv_sigcode, (void *)destp,
		    szsigcode);
		if (error != 0)
			return (error);
	}

	/*
	 * Copy the image path for the rtld.
	 */
	if (imgp->execpath != NULL && imgp->auxargs != NULL) {
		execpath_len = strlen(imgp->execpath) + 1;
		destp -= execpath_len;
		imgp->execpathp = (void *)destp;
		error = copyout(imgp->execpath, imgp->execpathp, execpath_len);
		if (error != 0)
			return (error);
	}

	/*
	 * Prepare the canary for SSP.
	 */
	arc4rand(canary, sizeof(canary), 0);
	destp -= sizeof(canary);
	imgp->canary = (void *)destp;
	error = copyout(canary, imgp->canary, sizeof(canary));
	if (error != 0)
		return (error);
	imgp->canarylen = sizeof(canary);

	/*
	 * Prepare the pagesizes array.
	 */
	for (i = 0; i < MAXPAGESIZES; i++)
		pagesizes32[i] = (uint32_t)pagesizes[i];
	destp -= sizeof(pagesizes32);
	destp = rounddown2(destp, sizeof(uint32_t));
	imgp->pagesizes = (void *)destp;
	error = copyout(pagesizes32, imgp->pagesizes, sizeof(pagesizes32));
	if (error != 0)
		return (error);
	imgp->pagesizeslen = sizeof(pagesizes32);

	/*
	 * Allocate room for the argument and environment strings.
	 */
	destp -= ARG_MAX - imgp->args->stringspace;
	destp = rounddown2(destp, sizeof(uint32_t));
	ustringp = destp;

	if (imgp->auxargs) {
		/*
		 * Allocate room on the stack for the ELF auxargs
		 * array.  It has up to AT_COUNT entries.
		 */
		destp -= AT_COUNT * sizeof(Elf32_Auxinfo);
		destp = rounddown2(destp, sizeof(uint32_t));
	}

	vectp = (uint32_t *)destp;

	/*
	 * Allocate room for the argv[] and env vectors including the
	 * terminating NULL pointers.
	 */
	vectp -= imgp->args->argc + 1 + imgp->args->envc + 1;

	/*
	 * vectp also becomes our initial stack base
	 */
	*stack_base = (uintptr_t)vectp;

	stringp = imgp->args->begin_argv;
	argc = imgp->args->argc;
	envc = imgp->args->envc;
	/*
	 * Copy out strings - arguments and environment.
	 */
	error = copyout(stringp, (void *)ustringp,
	    ARG_MAX - imgp->args->stringspace);
	if (error != 0)
		return (error);

	/*
	 * Fill in "ps_strings" struct for ps, w, etc.
	 */
	imgp->argv = vectp;
	if (suword32(&arginfo->ps_argvstr, (uint32_t)(intptr_t)vectp) != 0 ||
	    suword32(&arginfo->ps_nargvstr, argc) != 0)
		return (EFAULT);

	/*
	 * Fill in argument portion of vector table.
	 */
	for (; argc > 0; --argc) {
		if (suword32(vectp++, ustringp) != 0)
			return (EFAULT);
		while (*stringp++ != 0)
			ustringp++;
		ustringp++;
	}

	/* a null vector table pointer separates the argp's from the envp's */
	if (suword32(vectp++, 0) != 0)
		return (EFAULT);

	imgp->envv = vectp;
	if (suword32(&arginfo->ps_envstr, (uint32_t)(intptr_t)vectp) != 0 ||
	    suword32(&arginfo->ps_nenvstr, envc) != 0)
		return (EFAULT);

	/*
	 * Fill in environment portion of vector table.
	 */
	for (; envc > 0; --envc) {
		if (suword32(vectp++, ustringp) != 0)
			return (EFAULT);
		while (*stringp++ != 0)
			ustringp++;
		ustringp++;
	}

	/* end of vector table is a null pointer */
	if (suword32(vectp, 0) != 0)
		return (EFAULT);

	if (imgp->auxargs) {
		vectp++;
		error = imgp->sysent->sv_copyout_auxargs(imgp,
		    (uintptr_t)vectp);
		if (error != 0)
			return (error);
	}

	return (0);
}

int
freebsd32_kldstat(struct thread *td, struct freebsd32_kldstat_args *uap)
{
	struct kld_file_stat *stat;
	struct kld_file_stat32 *stat32;
	int error, version;

	if ((error = copyin(&uap->stat->version, &version, sizeof(version)))
	    != 0)
		return (error);
	if (version != sizeof(struct kld_file_stat_1_32) &&
	    version != sizeof(struct kld_file_stat32))
		return (EINVAL);

	stat = malloc(sizeof(*stat), M_TEMP, M_WAITOK | M_ZERO);
	stat32 = malloc(sizeof(*stat32), M_TEMP, M_WAITOK | M_ZERO);
	error = kern_kldstat(td, uap->fileid, stat);
	if (error == 0) {
		bcopy(&stat->name[0], &stat32->name[0], sizeof(stat->name));
		CP(*stat, *stat32, refs);
		CP(*stat, *stat32, id);
		PTROUT_CP(*stat, *stat32, address);
		CP(*stat, *stat32, size);
		bcopy(&stat->pathname[0], &stat32->pathname[0],
		    sizeof(stat->pathname));
		stat32->version  = version;
		error = copyout(stat32, uap->stat, version);
	}
	free(stat, M_TEMP);
	free(stat32, M_TEMP);
	return (error);
}

int
freebsd32_posix_fallocate(struct thread *td,
    struct freebsd32_posix_fallocate_args *uap)
{
	int error;

	error = kern_posix_fallocate(td, uap->fd,
	    PAIR32TO64(off_t, uap->offset), PAIR32TO64(off_t, uap->len));
	return (kern_posix_error(td, error));
}

int
freebsd32_posix_fadvise(struct thread *td,
    struct freebsd32_posix_fadvise_args *uap)
{
	int error;

	error = kern_posix_fadvise(td, uap->fd, PAIR32TO64(off_t, uap->offset),
	    PAIR32TO64(off_t, uap->len), uap->advice);
	return (kern_posix_error(td, error));
}

int
convert_sigevent32(struct sigevent32 *sig32, struct sigevent *sig)
{

	CP(*sig32, *sig, sigev_notify);
	switch (sig->sigev_notify) {
	case SIGEV_NONE:
		break;
	case SIGEV_THREAD_ID:
		CP(*sig32, *sig, sigev_notify_thread_id);
		/* FALLTHROUGH */
	case SIGEV_SIGNAL:
		CP(*sig32, *sig, sigev_signo);
		PTRIN_CP(*sig32, *sig, sigev_value.sival_ptr);
		break;
	case SIGEV_KEVENT:
		CP(*sig32, *sig, sigev_notify_kqueue);
		CP(*sig32, *sig, sigev_notify_kevent_flags);
		PTRIN_CP(*sig32, *sig, sigev_value.sival_ptr);
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

int
freebsd32_procctl(struct thread *td, struct freebsd32_procctl_args *uap)
{
	void *data;
	union {
		struct procctl_reaper_status rs;
		struct procctl_reaper_pids rp;
		struct procctl_reaper_kill rk;
	} x;
	union {
		struct procctl_reaper_pids32 rp;
	} x32;
	int error, error1, flags, signum;

	if (uap->com >= PROC_PROCCTL_MD_MIN)
		return (cpu_procctl(td, uap->idtype, PAIR32TO64(id_t, uap->id),
		    uap->com, PTRIN(uap->data)));

	switch (uap->com) {
	case PROC_ASLR_CTL:
	case PROC_PROTMAX_CTL:
	case PROC_SPROTECT:
	case PROC_STACKGAP_CTL:
	case PROC_TRACE_CTL:
	case PROC_TRAPCAP_CTL:
	case PROC_NO_NEW_PRIVS_CTL:
	case PROC_WXMAP_CTL:
	case PROC_LOGSIGEXIT_CTL:
		error = copyin(PTRIN(uap->data), &flags, sizeof(flags));
		if (error != 0)
			return (error);
		data = &flags;
		break;
	case PROC_REAP_ACQUIRE:
	case PROC_REAP_RELEASE:
		if (uap->data != NULL)
			return (EINVAL);
		data = NULL;
		break;
	case PROC_REAP_STATUS:
		data = &x.rs;
		break;
	case PROC_REAP_GETPIDS:
		error = copyin(uap->data, &x32.rp, sizeof(x32.rp));
		if (error != 0)
			return (error);
		CP(x32.rp, x.rp, rp_count);
		PTRIN_CP(x32.rp, x.rp, rp_pids);
		data = &x.rp;
		break;
	case PROC_REAP_KILL:
		error = copyin(uap->data, &x.rk, sizeof(x.rk));
		if (error != 0)
			return (error);
		data = &x.rk;
		break;
	case PROC_ASLR_STATUS:
	case PROC_PROTMAX_STATUS:
	case PROC_STACKGAP_STATUS:
	case PROC_TRACE_STATUS:
	case PROC_TRAPCAP_STATUS:
	case PROC_NO_NEW_PRIVS_STATUS:
	case PROC_WXMAP_STATUS:
	case PROC_LOGSIGEXIT_STATUS:
		data = &flags;
		break;
	case PROC_PDEATHSIG_CTL:
		error = copyin(uap->data, &signum, sizeof(signum));
		if (error != 0)
			return (error);
		data = &signum;
		break;
	case PROC_PDEATHSIG_STATUS:
		data = &signum;
		break;
	default:
		return (EINVAL);
	}
	error = kern_procctl(td, uap->idtype, PAIR32TO64(id_t, uap->id),
	    uap->com, data);
	switch (uap->com) {
	case PROC_REAP_STATUS:
		if (error == 0)
			error = copyout(&x.rs, uap->data, sizeof(x.rs));
		break;
	case PROC_REAP_KILL:
		error1 = copyout(&x.rk, uap->data, sizeof(x.rk));
		if (error == 0)
			error = error1;
		break;
	case PROC_ASLR_STATUS:
	case PROC_PROTMAX_STATUS:
	case PROC_STACKGAP_STATUS:
	case PROC_TRACE_STATUS:
	case PROC_TRAPCAP_STATUS:
	case PROC_NO_NEW_PRIVS_STATUS:
	case PROC_WXMAP_STATUS:
	case PROC_LOGSIGEXIT_STATUS:
		if (error == 0)
			error = copyout(&flags, uap->data, sizeof(flags));
		break;
	case PROC_PDEATHSIG_STATUS:
		if (error == 0)
			error = copyout(&signum, uap->data, sizeof(signum));
		break;
	}
	return (error);
}

int
freebsd32_fcntl(struct thread *td, struct freebsd32_fcntl_args *uap)
{
	intptr_t tmp;

	switch (uap->cmd) {
	/*
	 * Do unsigned conversion for arg when operation
	 * interprets it as flags or pointer.
	 */
	case F_SETLK_REMOTE:
	case F_SETLKW:
	case F_SETLK:
	case F_GETLK:
	case F_SETFD:
	case F_SETFL:
	case F_OGETLK:
	case F_OSETLK:
	case F_OSETLKW:
	case F_KINFO:
		tmp = (unsigned int)(uap->arg);
		break;
	default:
		tmp = uap->arg;
		break;
	}
	return (kern_fcntl_freebsd(td, uap->fd, uap->cmd, tmp));
}

int
freebsd32_ppoll(struct thread *td, struct freebsd32_ppoll_args *uap)
{
	struct timespec32 ts32;
	struct timespec ts, *tsp;
	sigset_t set, *ssp;
	int error;

	if (uap->ts != NULL) {
		error = copyin(uap->ts, &ts32, sizeof(ts32));
		if (error != 0)
			return (error);
		CP(ts32, ts, tv_sec);
		CP(ts32, ts, tv_nsec);
		tsp = &ts;
	} else
		tsp = NULL;
	if (uap->set != NULL) {
		error = copyin(uap->set, &set, sizeof(set));
		if (error != 0)
			return (error);
		ssp = &set;
	} else
		ssp = NULL;

	return (kern_poll(td, uap->fds, uap->nfds, tsp, ssp));
}

int
freebsd32_sched_rr_get_interval(struct thread *td,
    struct freebsd32_sched_rr_get_interval_args *uap)
{
	struct timespec ts;
	struct timespec32 ts32;
	int error;

	error = kern_sched_rr_get_interval(td, uap->pid, &ts);
	if (error == 0) {
		CP(ts, ts32, tv_sec);
		CP(ts, ts32, tv_nsec);
		error = copyout(&ts32, uap->interval, sizeof(ts32));
	}
	return (error);
}

static void
timex_to_32(struct timex32 *dst, struct timex *src)
{
	CP(*src, *dst, modes);
	CP(*src, *dst, offset);
	CP(*src, *dst, freq);
	CP(*src, *dst, maxerror);
	CP(*src, *dst, esterror);
	CP(*src, *dst, status);
	CP(*src, *dst, constant);
	CP(*src, *dst, precision);
	CP(*src, *dst, tolerance);
	CP(*src, *dst, ppsfreq);
	CP(*src, *dst, jitter);
	CP(*src, *dst, shift);
	CP(*src, *dst, stabil);
	CP(*src, *dst, jitcnt);
	CP(*src, *dst, calcnt);
	CP(*src, *dst, errcnt);
	CP(*src, *dst, stbcnt);
}

static void
timex_from_32(struct timex *dst, struct timex32 *src)
{
	CP(*src, *dst, modes);
	CP(*src, *dst, offset);
	CP(*src, *dst, freq);
	CP(*src, *dst, maxerror);
	CP(*src, *dst, esterror);
	CP(*src, *dst, status);
	CP(*src, *dst, constant);
	CP(*src, *dst, precision);
	CP(*src, *dst, tolerance);
	CP(*src, *dst, ppsfreq);
	CP(*src, *dst, jitter);
	CP(*src, *dst, shift);
	CP(*src, *dst, stabil);
	CP(*src, *dst, jitcnt);
	CP(*src, *dst, calcnt);
	CP(*src, *dst, errcnt);
	CP(*src, *dst, stbcnt);
}

int
freebsd32_ntp_adjtime(struct thread *td, struct freebsd32_ntp_adjtime_args *uap)
{
	struct timex tx;
	struct timex32 tx32;
	int error, retval;

	error = copyin(uap->tp, &tx32, sizeof(tx32));
	if (error == 0) {
		timex_from_32(&tx, &tx32);
		error = kern_ntp_adjtime(td, &tx, &retval);
		if (error == 0) {
			timex_to_32(&tx32, &tx);
			error = copyout(&tx32, uap->tp, sizeof(tx32));
			if (error == 0)
				td->td_retval[0] = retval;
		}
	}
	return (error);
}

#ifdef FFCLOCK
extern struct mtx ffclock_mtx;
extern struct ffclock_estimate ffclock_estimate;
extern int8_t ffclock_updated;

int
freebsd32_ffclock_setestimate(struct thread *td,
    struct freebsd32_ffclock_setestimate_args *uap)
{
	struct ffclock_estimate cest;
	struct ffclock_estimate32 cest32;
	int error;

	/* Reuse of PRIV_CLOCK_SETTIME. */
	if ((error = priv_check(td, PRIV_CLOCK_SETTIME)) != 0)
		return (error);

	if ((error = copyin(uap->cest, &cest32,
	    sizeof(struct ffclock_estimate32))) != 0)
		return (error);

	CP(cest.update_time, cest32.update_time, sec);
	memcpy(&cest.update_time.frac, &cest32.update_time.frac, sizeof(uint64_t));
	CP(cest, cest32, update_ffcount);
	CP(cest, cest32, leapsec_next);
	CP(cest, cest32, period);
	CP(cest, cest32, errb_abs);
	CP(cest, cest32, errb_rate);
	CP(cest, cest32, status);
	CP(cest, cest32, leapsec_total);
	CP(cest, cest32, leapsec);

	mtx_lock(&ffclock_mtx);
	memcpy(&ffclock_estimate, &cest, sizeof(struct ffclock_estimate));
	ffclock_updated++;
	mtx_unlock(&ffclock_mtx);
	return (error);
}

int
freebsd32_ffclock_getestimate(struct thread *td,
    struct freebsd32_ffclock_getestimate_args *uap)
{
	struct ffclock_estimate cest;
	struct ffclock_estimate32 cest32;
	int error;

	mtx_lock(&ffclock_mtx);
	memcpy(&cest, &ffclock_estimate, sizeof(struct ffclock_estimate));
	mtx_unlock(&ffclock_mtx);

	CP(cest32.update_time, cest.update_time, sec);
	memcpy(&cest32.update_time.frac, &cest.update_time.frac, sizeof(uint64_t));
	CP(cest32, cest, update_ffcount);
	CP(cest32, cest, leapsec_next);
	CP(cest32, cest, period);
	CP(cest32, cest, errb_abs);
	CP(cest32, cest, errb_rate);
	CP(cest32, cest, status);
	CP(cest32, cest, leapsec_total);
	CP(cest32, cest, leapsec);

	error = copyout(&cest32, uap->cest, sizeof(struct ffclock_estimate32));
	return (error);
}
#else /* !FFCLOCK */
int
freebsd32_ffclock_setestimate(struct thread *td,
    struct freebsd32_ffclock_setestimate_args *uap)
{
	return (ENOSYS);
}

int
freebsd32_ffclock_getestimate(struct thread *td,
    struct freebsd32_ffclock_getestimate_args *uap)
{
	return (ENOSYS);
}
#endif /* FFCLOCK */

#ifdef COMPAT_43
int
ofreebsd32_sethostid(struct thread *td, struct ofreebsd32_sethostid_args *uap)
{
	int name[] = { CTL_KERN, KERN_HOSTID };
	long hostid;

	hostid = uap->hostid;
	return (kernel_sysctl(td, name, nitems(name), NULL, NULL, &hostid,
	    sizeof(hostid), NULL, 0));
}
#endif

int
freebsd32_setcred(struct thread *td, struct freebsd32_setcred_args *uap)
{
	/* Last argument is 'is_32bit'. */
	return (user_setcred(td, uap->flags, uap->wcred, uap->size, true));
}
