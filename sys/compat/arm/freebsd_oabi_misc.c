/*-
 * Copyright (c) 2002 Doug Rabson
 * Copyright (C) 2010 Andrew Turner
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/exec.h>
#include <sys/fcntl.h>
#include <sys/filedesc.h>
#include <sys/imgact.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/file.h>		/* Must come after sys/malloc.h */
#include <sys/imgact.h>
#include <sys/mbuf.h>
#include <sys/mman.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/mqueue.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
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
#include <sys/unistd.h>
#include <sys/ucontext.h>
#include <sys/vnode.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>

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

#include <security/audit/audit.h>

#include <compat/arm/freebsd_oabi.h>
#include <compat/arm/freebsd_oabi_proto.h>

#ifndef __ARM_EABI__
#error COMPAT_OABI requires an ARM EABI kernel
#endif

CTASSERT(sizeof(struct aiocb_oabi) == 96);
CTASSERT(sizeof(fd_set) == 128);
CTASSERT(sizeof(struct fhandle) == 28);
CTASSERT(sizeof(struct iovec) == 8);
CTASSERT(sizeof(struct itimerval_oabi) == 24);
CTASSERT(sizeof(struct jail) == 32);
CTASSERT(sizeof(struct kevent) == 20);
CTASSERT(sizeof(struct mq_attr) == 32);
CTASSERT(sizeof(struct msqid_ds_oabi) == 76);
CTASSERT(sizeof(struct msghdr) == 28);
CTASSERT(sizeof(struct sf_hdtr) == 16);
CTASSERT(sizeof(sigset_t) == 16);
CTASSERT(sizeof(struct sigaltstack) == 12);
CTASSERT(sizeof(struct sigvec) == 12);
CTASSERT(sizeof(struct sockaddr) == 16);
CTASSERT(sizeof(struct stat_oabi) == 108);
CTASSERT(sizeof(struct statfs) == 472);
CTASSERT(sizeof(struct thr_param) == 52);
CTASSERT(sizeof(struct rusage_oabi) == 80);
CTASSERT(sizeof(struct sigaction) == 24);
CTASSERT(sizeof(struct timespec_oabi) == 12);
CTASSERT(sizeof(struct timeval_oabi) == 12);
CTASSERT(sizeof(struct timezone) == 8);

static void copy_rusage(const struct rusage *, struct rusage_oabi *);
static void copy_stat( struct stat *in, struct stat_oabi *out);

static void
copy_rusage(const struct rusage *s, struct rusage_oabi *s_oabi)
{

	TV_CP(*s, *s_oabi, ru_utime);
	TV_CP(*s, *s_oabi, ru_stime);
	CP(*s, *s_oabi, ru_maxrss);
	CP(*s, *s_oabi, ru_ixrss);
	CP(*s, *s_oabi, ru_idrss);
	CP(*s, *s_oabi, ru_isrss);
	CP(*s, *s_oabi, ru_minflt);
	CP(*s, *s_oabi, ru_majflt);
	CP(*s, *s_oabi, ru_nswap);
	CP(*s, *s_oabi, ru_inblock);
	CP(*s, *s_oabi, ru_oublock);
	CP(*s, *s_oabi, ru_msgsnd);
	CP(*s, *s_oabi, ru_msgrcv);
	CP(*s, *s_oabi, ru_nsignals);
	CP(*s, *s_oabi, ru_nvcsw);
	CP(*s, *s_oabi, ru_nivcsw);
}
int
freebsd_oabi_wait4(struct thread *td, struct freebsd_oabi_wait4_args *uap)
{
	struct rusage_oabi ru_oabi;
	struct rusage ru, *rup;
	int error, status;

	if (uap->rusage != NULL)
		rup = &ru;
	else
		rup = NULL;
	error = kern_wait(td, uap->pid, &status, uap->options, rup);
	if (uap->status != NULL && error == 0)
		error = copyout(&status, uap->status, sizeof(status));
	if (uap->rusage != NULL && error == 0) {
		copy_rusage(&ru, &ru_oabi);
		error = copyout(&ru_oabi, uap->rusage, sizeof(ru_oabi));
	}
	return (error);
}

int
freebsd_oabi_getitimer(struct thread *td, struct freebsd_oabi_getitimer_args *uap)
{
	struct itimerval aitv;
	struct itimerval_oabi aitv_oabi;
	int error;

	error = kern_getitimer(td, uap->which, &aitv);
	if (error != 0)
		return (error);
	TV_CP(aitv, aitv_oabi, it_interval);
	TV_CP(aitv, aitv_oabi, it_value);
	return (copyout(&aitv_oabi, uap->itv, sizeof (struct itimerval_oabi)));
}

int
freebsd_oabi_setitimer(struct thread *td, struct freebsd_oabi_setitimer_args *uap)
{
	struct itimerval aitv, oitv;
	struct itimerval_oabi aitv_oabi, oitv_oabi;
	int error;

	if (uap->itv == NULL) {
		uap->itv = uap->oitv;
		return (freebsd_oabi_getitimer(td,
		    (struct freebsd_oabi_getitimer_args *)uap));
	}

	if ((error = copyin(uap->itv, &aitv_oabi,
	    sizeof(struct itimerval_oabi))))
		return (error);
	TV_CP(aitv_oabi, aitv, it_interval);
	TV_CP(aitv_oabi, aitv, it_value);

	error = kern_setitimer(td, uap->which, &aitv, &oitv);
	if (error != 0 || uap->oitv == NULL)
		return (error);

	TV_CP(oitv, oitv_oabi, it_interval);
	TV_CP(oitv, oitv_oabi, it_value);
	return (copyout(&oitv_oabi, uap->oitv, sizeof(struct itimerval_oabi)));
}

int
freebsd_oabi_select(struct thread *td, struct freebsd_oabi_select_args *uap)
{
	struct timeval tv, *tvp;
	struct timeval_oabi tv_oabi;
	int error;

	if (uap->tv != NULL) {
		error = copyin(uap->tv, &tv_oabi, sizeof(tv_oabi));
		if (error)
			return (error);
		CP(tv_oabi, tv, tv_sec);
		CP(tv_oabi, tv, tv_usec);
		tvp = &tv;
	} else
		tvp = NULL;

	return (kern_select(td, uap->nd, uap->in, uap->ou, uap->ex, tvp,
	    NFDBITS));
}

int
freebsd_oabi_gettimeofday(struct thread *td,
    struct freebsd_oabi_gettimeofday_args *uap)
{
	struct timeval atv;
	struct timeval_oabi atv_oabi;
	struct timezone rtz;
	int error = 0;

	if (uap->tp) {
		microtime(&atv);
		CP(atv, atv_oabi, tv_sec);
		CP(atv, atv_oabi, tv_usec);
		error = copyout(&atv_oabi, uap->tp, sizeof (atv_oabi));
	}
	if (error == 0 && uap->tzp != NULL) {
		rtz.tz_minuteswest = tz_minuteswest;
		rtz.tz_dsttime = tz_dsttime;
		error = copyout(&rtz, uap->tzp, sizeof (rtz));
	}
	return (error);
}

int
freebsd_oabi_settimeofday(struct thread *td,
		       struct freebsd_oabi_settimeofday_args *uap)
{
	struct timeval atv, *tvp;
	struct timeval_oabi atv_oabi;
	struct timezone atz, *tzp;
	int error;

	if (uap->tv) {
		error = copyin(uap->tv, &atv_oabi, sizeof(atv_oabi));
		if (error)
			return (error);
		CP(atv_oabi, atv, tv_sec);
		CP(atv_oabi, atv, tv_usec);
		tvp = &atv;
	} else
		tvp = NULL;
	if (uap->tzp) {
		error = copyin(uap->tzp, &atz, sizeof(atz));
		if (error)
			return (error);
		tzp = &atz;
	} else
		tzp = NULL;
	return (kern_settimeofday(td, tvp, tzp));
}

int
freebsd_oabi_getrusage(struct thread *td, struct freebsd_oabi_getrusage_args *uap)
{
	struct rusage_oabi ru_oabi;
	struct rusage ru;
	int error;

	error = kern_getrusage(td, uap->who, &ru);
	if (error == 0) {
		copy_rusage(&ru, &ru_oabi);
		error = copyout(&ru_oabi, uap->rusage,
		    sizeof(struct rusage_oabi));
	}
	return (error);
}

int
freebsd_oabi_utimes(struct thread *td, struct freebsd_oabi_utimes_args *uap)
{
	panic("freebsd_oabi_utimes");
}

int
freebsd_oabi_lutimes(struct thread *td, struct freebsd_oabi_lutimes_args *uap)
{
	panic("freebsd_oabi_lutimes");
}

int
freebsd_oabi_futimes(struct thread *td, struct freebsd_oabi_futimes_args *uap)
{
	panic("freebsd_oabi_futimes");
}

int
freebsd_oabi_futimesat(struct thread *td, struct freebsd_oabi_futimesat_args *uap)
{
	panic("freebsd_oabi_futimesat");
}

int
freebsd_oabi_adjtime(struct thread *td, struct freebsd_oabi_adjtime_args *uap)
{
	panic("freebsd_oabi_adjtime");
}

int
freebsd_oabi_preadv(struct thread *td, struct freebsd_oabi_preadv_args *uap)
{
	struct uio *auio;
	off_t offset;
	int error;

	error = copyinuio(uap->iovp, uap->iovcnt, &auio);
	if (error)
		return (error);
	offset = PAIR32TO64(off_t, uap->offset);
	error = kern_preadv(td, uap->fd, auio, offset);
	free(auio, M_IOV);
	return (error);
}

int
freebsd_oabi_pwritev(struct thread *td, struct freebsd_oabi_pwritev_args *uap)
{
	struct uio *auio;
	off_t offset;
	int error;

	error = copyinuio(uap->iovp, uap->iovcnt, &auio);
	if (error)
		return (error);
	offset = PAIR32TO64(off_t, uap->offset);
	error = kern_pwritev(td, uap->fd, auio, offset);
	free(auio, M_IOV);
	return(error);
}

int
freebsd_oabi_pread(struct thread *td, struct freebsd_oabi_pread_args *uap)
{
	struct uio auio;
	struct iovec aiov;
	off_t offset;
	int error;

	if (uap->nbyte > INT_MAX)
		return (EINVAL);
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->nbyte;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = uap->nbyte;
	auio.uio_segflg = UIO_USERSPACE;
	offset = PAIR32TO64(off_t, uap->offset);
	error = kern_preadv(td, uap->fd, &auio, offset);
	return(error);
}

int
freebsd_oabi_pwrite(struct thread *td, struct freebsd_oabi_pwrite_args *uap)
{
	struct uio auio;
	struct iovec aiov;
	off_t offset;
	int error;

	if (uap->nbyte > INT_MAX)
		return (EINVAL);
	aiov.iov_base = (void *)(uintptr_t)uap->buf;
	aiov.iov_len = uap->nbyte;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = uap->nbyte;
	auio.uio_segflg = UIO_USERSPACE;
	offset = PAIR32TO64(off_t, uap->offset);
	error = kern_pwritev(td, uap->fd, &auio, offset);
	return(error);
}

int
freebsd_oabi_mmap(struct thread *td, struct freebsd_oabi_mmap_args *uap)
{
	struct mmap_args ap;

	CP(*uap, ap, addr);
	CP(*uap, ap, len);
	CP(*uap, ap, prot);
	CP(*uap, ap, flags);
	CP(*uap, ap, fd);

	ap.pos = PAIR32TO64(off_t, uap->pos);
	return mmap(td, &ap);
}

int
freebsd_oabi_lseek(struct thread *td, struct freebsd_oabi_lseek_args *uap)
{
	struct lseek_args ap;

	CP(*uap, ap, fd);
	CP64(*uap, ap, offset);
	CP(*uap, ap, whence);
	return lseek(td, &ap);
}

int
freebsd_oabi_truncate(struct thread *td, struct freebsd_oabi_truncate_args *uap)
{
	off_t length;

	length = PAIR32TO64(off_t, uap->length);
	return (kern_truncate(td, uap->path, UIO_USERSPACE, length));
}

int
freebsd_oabi_ftruncate(struct thread *td,
    struct freebsd_oabi_ftruncate_args *uap)
{
	off_t length;

	length = PAIR32TO64(off_t, uap->length);
	return (kern_ftruncate(td, uap->fd, length));
}

static void
copy_stat(struct stat *in, struct stat_oabi *out)
{
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
}

int
freebsd_oabi_stat(struct thread *td, struct freebsd_oabi_stat_args *uap)
{
	struct stat sb;
	struct stat_oabi sb_oabi;
	int error;

	error = kern_stat(td, uap->path, UIO_USERSPACE, &sb);
	if (error)
		return (error);
	copy_stat(&sb, &sb_oabi);
	error = copyout(&sb_oabi, uap->ub, sizeof (sb_oabi));
	return (error);
}

int
freebsd_oabi_fstat(struct thread *td, struct freebsd_oabi_fstat_args *uap)
{
	struct stat ub;
	struct stat_oabi ub_oabi;
	int error;

	error = kern_fstat(td, uap->fd, &ub);
	if (error)
		return (error);
	copy_stat(&ub, &ub_oabi);
	error = copyout(&ub_oabi, uap->ub, sizeof(ub_oabi));
	return (error);
}

int
freebsd_oabi_fstatat(struct thread *td, struct freebsd_oabi_fstatat_args *uap)
{
	struct stat ub;
	struct stat_oabi ub_oabi;
	int error;

	error = kern_statat(td, uap->flag, uap->fd, uap->path, UIO_USERSPACE,
	    &ub);
	if (error)
		return (error);
	copy_stat(&ub, &ub_oabi);
	error = copyout(&ub_oabi, uap->buf, sizeof(ub_oabi));
	return (error);
}

int
freebsd_oabi_lstat(struct thread *td, struct freebsd_oabi_lstat_args *uap)
{
	struct stat sb;
	struct stat_oabi sb_oabi;
	int error;

	error = kern_lstat(td, uap->path, UIO_USERSPACE, &sb);
	if (error)
		return (error);
	copy_stat(&sb, &sb_oabi);
	error = copyout(&sb_oabi, uap->ub, sizeof (sb_oabi));
	return (error);
}

int
freebsd_oabi_cpuset_setid(struct thread *td,
    struct freebsd_oabi_cpuset_setid_args *uap)
{
	panic("freebsd_oabi_cpuset_setid");
}

int
freebsd_oabi_clock_gettime(struct thread *td,
    struct freebsd_oabi_clock_gettime_args *uap)
{
	panic("freebsd_oabi_clock_gettime");
}

int
freebsd_oabi_clock_settime(struct thread *td,
    struct freebsd_oabi_clock_settime_args *uap)
{
	panic("freebsd_oabi_clock_settime");
}

int
freebsd_oabi_clock_getres(struct thread *td,
    struct freebsd_oabi_clock_getres_args *uap)
{
	panic("freebsd_oabi_clock_getres");
}

int
freebsd_oabi_nanosleep(struct thread *td,
    struct freebsd_oabi_nanosleep_args *uap)
{
	panic("freebsd_oabi_nanosleep");
}

int
freebsd_oabi_sched_rr_get_interval(struct thread *td,
    struct freebsd_oabi_sched_rr_get_interval_args *uap)
{
	panic("freebsd_oabi_sched_rr_get_interval");
}

int
freebsd_oabi_sigtimedwait(struct thread *td,
    struct freebsd_oabi_sigtimedwait_args *uap)
{
	panic("freebsd_oabi_sigtimedwait");
}

int
freebsd_oabi_kevent(struct thread *td,
    struct freebsd_oabi_kevent_args *uap)
{
	panic("freebsd_oabi_kevent");
}

int
freebsd_oabi_thr_suspend(struct thread *td,
    struct freebsd_oabi_thr_suspend_args *uap)
{
	panic("freebsd_oabi_thr_suspend");
}

int
freebsd_oabi_pselect(struct thread *td, struct freebsd_oabi_pselect_args *uap)
{
	panic("freebsd_oabi_pselect");
}

