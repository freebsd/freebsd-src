/*-
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
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "opt_inet.h"
#include "opt_inet6.h"

#define __ELF_WORD_SIZE 32

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/capability.h>
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
#include <sys/proc.h>
#include <sys/procctl.h>
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
#include <sys/condvar.h>
#include <sys/sf_buf.h>
#include <sys/sf_sync.h>
#include <sys/sf_base.h>

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

#include <compat/freebsd32/freebsd32_util.h>
#include <compat/freebsd32/freebsd32.h>
#include <compat/freebsd32/freebsd32_ipc.h>
#include <compat/freebsd32/freebsd32_misc.h>
#include <compat/freebsd32/freebsd32_signal.h>
#include <compat/freebsd32/freebsd32_proto.h>

FEATURE(compat_freebsd_32bit, "Compatible with 32-bit FreeBSD");

#ifndef __mips__
CTASSERT(sizeof(struct timeval32) == 8);
CTASSERT(sizeof(struct timespec32) == 8);
CTASSERT(sizeof(struct itimerval32) == 16);
#endif
CTASSERT(sizeof(struct statfs32) == 256);
#ifndef __mips__
CTASSERT(sizeof(struct rusage32) == 72);
#endif
CTASSERT(sizeof(struct sigaltstack32) == 12);
CTASSERT(sizeof(struct kevent32) == 20);
CTASSERT(sizeof(struct iovec32) == 8);
CTASSERT(sizeof(struct msghdr32) == 28);
#ifndef __mips__
CTASSERT(sizeof(struct stat32) == 96);
#endif
CTASSERT(sizeof(struct sigaction32) == 24);

static int freebsd32_kevent_copyout(void *arg, struct kevent *kevp, int count);
static int freebsd32_kevent_copyin(void *arg, struct kevent *kevp, int count);

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
	struct wrusage32 wru32;
	struct __wrusage wru, *wrup;
	struct siginfo32 si32;
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
copy_statfs(struct statfs *in, struct statfs32 *out)
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
	      in->f_mntonname, min(MNAMELEN, FREEBSD4_MNAMELEN));
	out->f_syncreads = MIN(in->f_syncreads, INT32_MAX);
	out->f_asyncreads = MIN(in->f_asyncreads, INT32_MAX);
	strlcpy(out->f_mntfromname,
	      in->f_mntfromname, min(MNAMELEN, FREEBSD4_MNAMELEN));
}
#endif

#ifdef COMPAT_FREEBSD4
int
freebsd4_freebsd32_getfsstat(struct thread *td, struct freebsd4_freebsd32_getfsstat_args *uap)
{
	struct statfs *buf, *sp;
	struct statfs32 stat32;
	size_t count, size;
	int error;

	count = uap->bufsize / sizeof(struct statfs32);
	size = count * sizeof(struct statfs);
	error = kern_getfsstat(td, &buf, size, UIO_SYSSPACE, uap->flags);
	if (size > 0) {
		count = td->td_retval[0];
		sp = buf;
		while (count > 0 && error == 0) {
			copy_statfs(sp, &stat32);
			error = copyout(&stat32, uap->buf, sizeof(stat32));
			sp++;
			uap->buf++;
			count--;
		}
		free(buf, M_TEMP);
	}
	return (error);
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
freebsd32_exec_copyin_args(struct image_args *args, char *fname,
    enum uio_seg segflg, u_int32_t *argv, u_int32_t *envv)
{
	char *argp, *envp;
	u_int32_t *p32, arg;
	size_t length;
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
	if (fname != NULL) {
		args->fname = args->buf;
		error = (segflg == UIO_SYSSPACE) ?
		    copystr(fname, args->fname, PATH_MAX, &length) :
		    copyinstr(fname, args->fname, PATH_MAX, &length);
		if (error != 0)
			goto err_exit;
	} else
		length = 0;

	args->begin_argv = args->buf + length;
	args->endp = args->begin_argv;
	args->stringspace = ARG_MAX;

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
		error = copyinstr(argp, args->endp, args->stringspace, &length);
		if (error) {
			if (error == ENAMETOOLONG)
				error = E2BIG;
			goto err_exit;
		}
		args->stringspace -= length;
		args->endp += length;
		args->argc++;
	}
			
	args->begin_envv = args->endp;

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
			error = copyinstr(envp, args->endp, args->stringspace,
			    &length);
			if (error) {
				if (error == ENAMETOOLONG)
					error = E2BIG;
				goto err_exit;
			}
			args->stringspace -= length;
			args->endp += length;
			args->envc++;
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
	int error;

	error = freebsd32_exec_copyin_args(&eargs, uap->fname, UIO_USERSPACE,
	    uap->argv, uap->envv);
	if (error == 0)
		error = kern_execve(td, &eargs, NULL);
	return (error);
}

int
freebsd32_fexecve(struct thread *td, struct freebsd32_fexecve_args *uap)
{
	struct image_args eargs;
	int error;

	error = freebsd32_exec_copyin_args(&eargs, NULL, UIO_SYSSPACE,
	    uap->argv, uap->envv);
	if (error == 0) {
		eargs.fd = uap->fd;
		error = kern_execve(td, &eargs, NULL);
	}
	return (error);
}

#ifdef __ia64__
static int
freebsd32_mmap_partial(struct thread *td, vm_offset_t start, vm_offset_t end,
		       int prot, int fd, off_t pos)
{
	vm_map_t map;
	vm_map_entry_t entry;
	int rv;

	map = &td->td_proc->p_vmspace->vm_map;
	if (fd != -1)
		prot |= VM_PROT_WRITE;

	if (vm_map_lookup_entry(map, start, &entry)) {
		if ((entry->protection & prot) != prot) {
			rv = vm_map_protect(map,
					    trunc_page(start),
					    round_page(end),
					    entry->protection | prot,
					    FALSE);
			if (rv != KERN_SUCCESS)
				return (EINVAL);
		}
	} else {
		vm_offset_t addr = trunc_page(start);
		rv = vm_map_find(map, NULL, 0, &addr, PAGE_SIZE, 0,
		    VMFS_NO_SPACE, prot, VM_PROT_ALL, 0);
		if (rv != KERN_SUCCESS)
			return (EINVAL);
	}

	if (fd != -1) {
		struct pread_args r;
		r.fd = fd;
		r.buf = (void *) start;
		r.nbyte = end - start;
		r.offset = pos;
		return (sys_pread(td, &r));
	} else {
		while (start < end) {
			subyte((void *) start, 0);
			start++;
		}
		return (0);
	}
}
#endif

int
freebsd32_mprotect(struct thread *td, struct freebsd32_mprotect_args *uap)
{
	struct mprotect_args ap;

	ap.addr = PTRIN(uap->addr);
	ap.len = uap->len;
	ap.prot = uap->prot;
#if defined(__amd64__) || defined(__ia64__)
	if (i386_read_exec && (ap.prot & PROT_READ) != 0)
		ap.prot |= PROT_EXEC;
#endif
	return (sys_mprotect(td, &ap));
}

int
freebsd32_mmap(struct thread *td, struct freebsd32_mmap_args *uap)
{
	struct mmap_args ap;
	vm_offset_t addr = (vm_offset_t) uap->addr;
	vm_size_t len	 = uap->len;
	int prot	 = uap->prot;
	int flags	 = uap->flags;
	int fd		 = uap->fd;
	off_t pos	 = PAIR32TO64(off_t,uap->pos);
#ifdef __ia64__
	vm_size_t pageoff;
	int error;

	/*
	 * Attempt to handle page size hassles.
	 */
	pageoff = (pos & PAGE_MASK);
	if (flags & MAP_FIXED) {
		vm_offset_t start, end;
		start = addr;
		end = addr + len;

		if (start != trunc_page(start)) {
			error = freebsd32_mmap_partial(td, start,
						       round_page(start), prot,
						       fd, pos);
			if (fd != -1)
				pos += round_page(start) - start;
			start = round_page(start);
		}
		if (end != round_page(end)) {
			vm_offset_t t = trunc_page(end);
			error = freebsd32_mmap_partial(td, t, end,
						  prot, fd,
						  pos + t - start);
			end = trunc_page(end);
		}
		if (end > start && fd != -1 && (pos & PAGE_MASK)) {
			/*
			 * We can't map this region at all. The specified
			 * address doesn't have the same alignment as the file
			 * position. Fake the mapping by simply reading the
			 * entire region into memory. First we need to make
			 * sure the region exists.
			 */
			vm_map_t map;
			struct pread_args r;
			int rv;

			prot |= VM_PROT_WRITE;
			map = &td->td_proc->p_vmspace->vm_map;
			rv = vm_map_remove(map, start, end);
			if (rv != KERN_SUCCESS)
				return (EINVAL);
			rv = vm_map_find(map, NULL, 0, &start, end - start,
			    0, VMFS_NO_SPACE, prot, VM_PROT_ALL, 0);
			if (rv != KERN_SUCCESS)
				return (EINVAL);
			r.fd = fd;
			r.buf = (void *) start;
			r.nbyte = end - start;
			r.offset = pos;
			error = sys_pread(td, &r);
			if (error)
				return (error);

			td->td_retval[0] = addr;
			return (0);
		}
		if (end == start) {
			/*
			 * After dealing with the ragged ends, there
			 * might be none left.
			 */
			td->td_retval[0] = addr;
			return (0);
		}
		addr = start;
		len = end - start;
	}
#endif

#if defined(__amd64__) || defined(__ia64__)
	if (i386_read_exec && (prot & PROT_READ))
		prot |= PROT_EXEC;
#endif

	ap.addr = (void *) addr;
	ap.len = len;
	ap.prot = prot;
	ap.flags = flags;
	ap.fd = fd;
	ap.pos = pos;

	return (sys_mmap(td, &ap));
}

#ifdef COMPAT_FREEBSD6
int
freebsd6_freebsd32_mmap(struct thread *td, struct freebsd6_freebsd32_mmap_args *uap)
{
	struct freebsd32_mmap_args ap;

	ap.addr = uap->addr;
	ap.len = uap->len;
	ap.prot = uap->prot;
	ap.flags = uap->flags;
	ap.fd = uap->fd;
	ap.pos1 = uap->pos1;
	ap.pos2 = uap->pos2;

	return (freebsd32_mmap(td, &ap));
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

/*
 * Copy 'count' items into the destination list pointed to by uap->eventlist.
 */
static int
freebsd32_kevent_copyout(void *arg, struct kevent *kevp, int count)
{
	struct freebsd32_kevent_args *uap;
	struct kevent32	ks32[KQ_NEVENTS];
	int i, error = 0;

	KASSERT(count <= KQ_NEVENTS, ("count (%d) > KQ_NEVENTS", count));
	uap = (struct freebsd32_kevent_args *)arg;

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
freebsd32_kevent_copyin(void *arg, struct kevent *kevp, int count)
{
	struct freebsd32_kevent_args *uap;
	struct kevent32	ks32[KQ_NEVENTS];
	int i, error = 0;

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
		CP(ks32[i], kevp[i], data);
		PTRIN_CP(ks32[i], kevp[i], udata);
	}
done:
	return (error);
}

int
freebsd32_kevent(struct thread *td, struct freebsd32_kevent_args *uap)
{
	struct timespec32 ts32;
	struct timespec ts, *tsp;
	struct kevent_copyops k_ops = { uap,
					freebsd32_kevent_copyout,
					freebsd32_kevent_copyin};
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
	error = kern_kevent(td, uap->fd, uap->nchanges, uap->nevents,
	    &k_ops, tsp);
	return (error);
}

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
		rtz.tz_minuteswest = tz_minuteswest;
		rtz.tz_dsttime = tz_dsttime;
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
	if (error)
		return (error);
	if (uap->rusage != NULL) {
		freebsd32_rusage_out(&s, &s32);
		error = copyout(&s32, uap->rusage, sizeof(s32));
	}
	return (error);
}

static int
freebsd32_copyinuio(struct iovec32 *iovp, u_int iovcnt, struct uio **uiop)
{
	struct iovec32 iov32;
	struct iovec *iov;
	struct uio *uio;
	u_int iovlen;
	int error, i;

	*uiop = NULL;
	if (iovcnt > UIO_MAXIOV)
		return (EINVAL);
	iovlen = iovcnt * sizeof(struct iovec);
	uio = malloc(iovlen + sizeof *uio, M_IOV, M_WAITOK);
	iov = (struct iovec *)(uio + 1);
	for (i = 0; i < iovcnt; i++) {
		error = copyin(&iovp[i], &iov32, sizeof(struct iovec32));
		if (error) {
			free(uio, M_IOV);
			return (error);
		}
		iov[i].iov_base = PTRIN(iov32.iov_base);
		iov[i].iov_len = iov32.iov_len;
	}
	uio->uio_iov = iov;
	uio->uio_iovcnt = iovcnt;
	uio->uio_segflg = UIO_USERSPACE;
	uio->uio_offset = -1;
	uio->uio_resid = 0;
	for (i = 0; i < iovcnt; i++) {
		if (iov->iov_len > INT_MAX - uio->uio_resid) {
			free(uio, M_IOV);
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
	free(auio, M_IOV);
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
	free(auio, M_IOV);
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
	free(auio, M_IOV);
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
	free(auio, M_IOV);
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
freebsd32_copyinmsghdr(struct msghdr32 *msg32, struct msghdr *msg)
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

#ifndef __mips__
#define FREEBSD32_ALIGNBYTES	(sizeof(int) - 1)
#else
#define FREEBSD32_ALIGNBYTES	(sizeof(long) - 1)
#endif
#define FREEBSD32_ALIGN(p)	\
	(((u_long)(p) + FREEBSD32_ALIGNBYTES) & ~FREEBSD32_ALIGNBYTES)
#define	FREEBSD32_CMSG_SPACE(l)	\
	(FREEBSD32_ALIGN(sizeof(struct cmsghdr)) + FREEBSD32_ALIGN(l))

#define	FREEBSD32_CMSG_DATA(cmsg)	((unsigned char *)(cmsg) + \
				 FREEBSD32_ALIGN(sizeof(struct cmsghdr)))
static int
freebsd32_copy_msg_out(struct msghdr *msg, struct mbuf *control)
{
	struct cmsghdr *cm;
	void *data;
	socklen_t clen, datalen;
	int error;
	caddr_t ctlbuf;
	int len, maxlen, copylen;
	struct mbuf *m;
	error = 0;

	len    = msg->msg_controllen;
	maxlen = msg->msg_controllen;
	msg->msg_controllen = 0;

	m = control;
	ctlbuf = msg->msg_control;
      
	while (m && len > 0) {
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

			/* Adjust message length */
			cm->cmsg_len = FREEBSD32_ALIGN(sizeof(struct cmsghdr)) +
			    datalen;


			/* Copy cmsghdr */
			copylen = sizeof(struct cmsghdr);
			if (len < copylen) {
				msg->msg_flags |= MSG_CTRUNC;
				copylen = len;
			}

			error = copyout(cm,ctlbuf,copylen);
			if (error)
				goto exit;

			ctlbuf += FREEBSD32_ALIGN(copylen);
			len    -= FREEBSD32_ALIGN(copylen);

			if (len <= 0)
				break;

			/* Copy data */
			copylen = datalen;
			if (len < copylen) {
				msg->msg_flags |= MSG_CTRUNC;
				copylen = len;
			}

			error = copyout(data,ctlbuf,copylen);
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
		}	
		m = m->m_next;
	}

	msg->msg_controllen = (len <= 0) ? maxlen :  ctlbuf - (caddr_t)msg->msg_control;
	
exit:
	return (error);

}

int
freebsd32_recvmsg(td, uap)
	struct thread *td;
	struct freebsd32_recvmsg_args /* {
		int	s;
		struct	msghdr32 *msg;
		int	flags;
	} */ *uap;
{
	struct msghdr msg;
	struct msghdr32 m32;
	struct iovec *uiov, *iov;
	struct mbuf *control = NULL;
	struct mbuf **controlp;

	int error;
	error = copyin(uap->msg, &m32, sizeof(m32));
	if (error)
		return (error);
	error = freebsd32_copyinmsghdr(uap->msg, &msg);
	if (error)
		return (error);
	error = freebsd32_copyiniov(PTRIN(m32.msg_iov), m32.msg_iovlen, &iov,
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

	if (control != NULL)
		m_freem(control);

	return (error);
}


static int
freebsd32_convert_msg_in(struct mbuf **controlp)
{
	struct mbuf *control = *controlp;
	struct cmsghdr *cm = mtod(control, struct cmsghdr *);
	void *data;
	socklen_t clen = control->m_len, datalen;
	int error;

	error = 0;
	*controlp = NULL;

	while (cm != NULL) {
		if (sizeof(struct cmsghdr) > clen || cm->cmsg_len > clen) {
			error = EINVAL;
			break;
		}

		data = FREEBSD32_CMSG_DATA(cm);
		datalen = (caddr_t)cm + cm->cmsg_len - (caddr_t)data;

		*controlp = sbcreatecontrol(data, datalen, cm->cmsg_type,
		    cm->cmsg_level);
		controlp = &(*controlp)->m_next;

		if (FREEBSD32_CMSG_SPACE(datalen) < clen) {
			clen -= FREEBSD32_CMSG_SPACE(datalen);
			cm = (struct cmsghdr *)
				((caddr_t)cm + FREEBSD32_CMSG_SPACE(datalen));
		} else {
			clen = 0;
			cm = NULL;
		}
	}

	m_freem(control);
	return (error);
}


int
freebsd32_sendmsg(struct thread *td,
		  struct freebsd32_sendmsg_args *uap)
{
	struct msghdr msg;
	struct msghdr32 m32;
	struct iovec *iov;
	struct mbuf *control = NULL;
	struct sockaddr *to = NULL;
	int error;

	error = copyin(uap->msg, &m32, sizeof(m32));
	if (error)
		return (error);
	error = freebsd32_copyinmsghdr(uap->msg, &msg);
	if (error)
		return (error);
	error = freebsd32_copyiniov(PTRIN(m32.msg_iov), m32.msg_iovlen, &iov,
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

		error = sockargs(&control, msg.msg_control,
		    msg.msg_controllen, MT_CONTROL);
		if (error)
			goto out;
		
		error = freebsd32_convert_msg_in(&control);
		if (error)
			goto out;
	}

	error = kern_sendit(td, uap->s, &msg, uap->flags, control,
	    UIO_USERSPACE);

out:
	free(iov, M_IOV);
	if (to)
		free(to, M_SONAME);
	return (error);
}

int
freebsd32_recvfrom(struct thread *td,
		   struct freebsd32_recvfrom_args *uap)
{
	struct msghdr msg;
	struct iovec aiov;
	int error;

	if (uap->fromlenaddr) {
		error = copyin(PTRIN(uap->fromlenaddr), &msg.msg_namelen,
		    sizeof(msg.msg_namelen));
		if (error)
			return (error);
	} else {
		msg.msg_namelen = 0;
	}

	msg.msg_name = PTRIN(uap->from);
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = PTRIN(uap->buf);
	aiov.iov_len = uap->len;
	msg.msg_control = NULL;
	msg.msg_flags = uap->flags;
	error = kern_recvit(td, uap->s, &msg, UIO_USERSPACE, NULL);
	if (error == 0 && uap->fromlenaddr)
		error = copyout(&msg.msg_namelen, PTRIN(uap->fromlenaddr),
		    sizeof (msg.msg_namelen));
	return (error);
}

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
	return (kern_utimes(td, uap->path, UIO_USERSPACE, sp, UIO_SYSSPACE));
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
	struct statfs32 s32;
	struct statfs s;
	int error;

	error = kern_statfs(td, uap->path, UIO_USERSPACE, &s);
	if (error)
		return (error);
	copy_statfs(&s, &s32);
	return (copyout(&s32, uap->buf, sizeof(s32)));
}
#endif

#ifdef COMPAT_FREEBSD4
int
freebsd4_freebsd32_fstatfs(struct thread *td, struct freebsd4_freebsd32_fstatfs_args *uap)
{
	struct statfs32 s32;
	struct statfs s;
	int error;

	error = kern_fstatfs(td, uap->fd, &s);
	if (error)
		return (error);
	copy_statfs(&s, &s32);
	return (copyout(&s32, uap->buf, sizeof(s32)));
}
#endif

#ifdef COMPAT_FREEBSD4
int
freebsd4_freebsd32_fhstatfs(struct thread *td, struct freebsd4_freebsd32_fhstatfs_args *uap)
{
	struct statfs32 s32;
	struct statfs s;
	fhandle_t fh;
	int error;

	if ((error = copyin(uap->u_fhp, &fh, sizeof(fhandle_t))) != 0)
		return (error);
	error = kern_fhstatfs(td, fh, &s);
	if (error)
		return (error);
	copy_statfs(&s, &s32);
	return (copyout(&s32, uap->buf, sizeof(s32)));
}
#endif

int
freebsd32_pread(struct thread *td, struct freebsd32_pread_args *uap)
{
	struct pread_args ap;

	ap.fd = uap->fd;
	ap.buf = uap->buf;
	ap.nbyte = uap->nbyte;
	ap.offset = PAIR32TO64(off_t,uap->offset);
	return (sys_pread(td, &ap));
}

int
freebsd32_pwrite(struct thread *td, struct freebsd32_pwrite_args *uap)
{
	struct pwrite_args ap;

	ap.fd = uap->fd;
	ap.buf = uap->buf;
	ap.nbyte = uap->nbyte;
	ap.offset = PAIR32TO64(off_t,uap->offset);
	return (sys_pwrite(td, &ap));
}

#ifdef COMPAT_43
int
ofreebsd32_lseek(struct thread *td, struct ofreebsd32_lseek_args *uap)
{
	struct lseek_args nuap;

	nuap.fd = uap->fd;
	nuap.offset = uap->offset;
	nuap.whence = uap->whence;
	return (sys_lseek(td, &nuap));
}
#endif

int
freebsd32_lseek(struct thread *td, struct freebsd32_lseek_args *uap)
{
	int error;
	struct lseek_args ap;
	off_t pos;

	ap.fd = uap->fd;
	ap.offset = PAIR32TO64(off_t,uap->offset);
	ap.whence = uap->whence;
	error = sys_lseek(td, &ap);
	/* Expand the quad return into two parts for eax and edx */
	pos = *(off_t *)(td->td_retval);
	td->td_retval[RETVAL_LO] = pos & 0xffffffff;	/* %eax */
	td->td_retval[RETVAL_HI] = pos >> 32;		/* %edx */
	return error;
}

int
freebsd32_truncate(struct thread *td, struct freebsd32_truncate_args *uap)
{
	struct truncate_args ap;

	ap.path = uap->path;
	ap.length = PAIR32TO64(off_t,uap->length);
	return (sys_truncate(td, &ap));
}

int
freebsd32_ftruncate(struct thread *td, struct freebsd32_ftruncate_args *uap)
{
	struct ftruncate_args ap;

	ap.fd = uap->fd;
	ap.length = PAIR32TO64(off_t,uap->length);
	return (sys_ftruncate(td, &ap));
}

#ifdef COMPAT_43
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

int
freebsd32_getdirentries(struct thread *td,
    struct freebsd32_getdirentries_args *uap)
{
	long base;
	int32_t base32;
	int error;

	error = kern_getdirentries(td, uap->fd, uap->buf, uap->count, &base,
	    NULL, UIO_USERSPACE);
	if (error)
		return (error);
	if (uap->basep != NULL) {
		base32 = base;
		error = copyout(&base32, uap->basep, sizeof(int32_t));
	}
	return (error);
}

#ifdef COMPAT_FREEBSD6
/* versions with the 'int pad' argument */
int
freebsd6_freebsd32_pread(struct thread *td, struct freebsd6_freebsd32_pread_args *uap)
{
	struct pread_args ap;

	ap.fd = uap->fd;
	ap.buf = uap->buf;
	ap.nbyte = uap->nbyte;
	ap.offset = PAIR32TO64(off_t,uap->offset);
	return (sys_pread(td, &ap));
}

int
freebsd6_freebsd32_pwrite(struct thread *td, struct freebsd6_freebsd32_pwrite_args *uap)
{
	struct pwrite_args ap;

	ap.fd = uap->fd;
	ap.buf = uap->buf;
	ap.nbyte = uap->nbyte;
	ap.offset = PAIR32TO64(off_t,uap->offset);
	return (sys_pwrite(td, &ap));
}

int
freebsd6_freebsd32_lseek(struct thread *td, struct freebsd6_freebsd32_lseek_args *uap)
{
	int error;
	struct lseek_args ap;
	off_t pos;

	ap.fd = uap->fd;
	ap.offset = PAIR32TO64(off_t,uap->offset);
	ap.whence = uap->whence;
	error = sys_lseek(td, &ap);
	/* Expand the quad return into two parts for eax and edx */
	pos = *(off_t *)(td->td_retval);
	td->td_retval[RETVAL_LO] = pos & 0xffffffff;	/* %eax */
	td->td_retval[RETVAL_HI] = pos >> 32;		/* %edx */
	return error;
}

int
freebsd6_freebsd32_truncate(struct thread *td, struct freebsd6_freebsd32_truncate_args *uap)
{
	struct truncate_args ap;

	ap.path = uap->path;
	ap.length = PAIR32TO64(off_t,uap->length);
	return (sys_truncate(td, &ap));
}

int
freebsd6_freebsd32_ftruncate(struct thread *td, struct freebsd6_freebsd32_ftruncate_args *uap)
{
	struct ftruncate_args ap;

	ap.fd = uap->fd;
	ap.length = PAIR32TO64(off_t,uap->length);
	return (sys_ftruncate(td, &ap));
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
	struct iovec32 *iov32;
	off_t offset;
	int error;
	off_t sbytes;
	struct sendfile_sync *sfs;

	offset = PAIR32TO64(off_t, uap->offset);
	if (offset < 0)
		return (EINVAL);

	hdr_uio = trl_uio = NULL;
	sfs = NULL;

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
		}
		if (hdtr.trailers != NULL) {
			iov32 = PTRIN(hdtr32.trailers);
			error = freebsd32_copyinuio(iov32,
			    hdtr32.trl_cnt, &trl_uio);
			if (error)
				goto out;
		}
	}

	error = _do_sendfile(td, uap->fd, uap->s, uap->flags, compat,
	    offset, uap->nbytes, &sbytes, hdr_uio, trl_uio);

	if (uap->sbytes != NULL)
		copyout(&sbytes, uap->sbytes, sizeof(off_t));

out:
	if (hdr_uio)
		free(hdr_uio, M_IOV);
	if (trl_uio)
		free(trl_uio, M_IOV);
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
	TS_CP(*in, *out, st_birthtim);
}

#ifdef COMPAT_43
static void
copy_ostat(struct stat *in, struct ostat32 *out)
{

	CP(*in, *out, st_dev);
	CP(*in, *out, st_ino);
	CP(*in, *out, st_mode);
	CP(*in, *out, st_nlink);
	CP(*in, *out, st_uid);
	CP(*in, *out, st_gid);
	CP(*in, *out, st_rdev);
	CP(*in, *out, st_size);
	TS_CP(*in, *out, st_atim);
	TS_CP(*in, *out, st_mtim);
	TS_CP(*in, *out, st_ctim);
	CP(*in, *out, st_blksize);
	CP(*in, *out, st_blocks);
	CP(*in, *out, st_flags);
	CP(*in, *out, st_gen);
}
#endif

int
freebsd32_stat(struct thread *td, struct freebsd32_stat_args *uap)
{
	struct stat sb;
	struct stat32 sb32;
	int error;

	error = kern_stat(td, uap->path, UIO_USERSPACE, &sb);
	if (error)
		return (error);
	copy_stat(&sb, &sb32);
	error = copyout(&sb32, uap->ub, sizeof (sb32));
	return (error);
}

#ifdef COMPAT_43
int
ofreebsd32_stat(struct thread *td, struct ofreebsd32_stat_args *uap)
{
	struct stat sb;
	struct ostat32 sb32;
	int error;

	error = kern_stat(td, uap->path, UIO_USERSPACE, &sb);
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
	error = copyout(&ub32, uap->ub, sizeof(ub32));
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
	error = copyout(&ub32, uap->ub, sizeof(ub32));
	return (error);
}
#endif

int
freebsd32_fstatat(struct thread *td, struct freebsd32_fstatat_args *uap)
{
	struct stat ub;
	struct stat32 ub32;
	int error;

	error = kern_statat(td, uap->flag, uap->fd, uap->path, UIO_USERSPACE, &ub);
	if (error)
		return (error);
	copy_stat(&ub, &ub32);
	error = copyout(&ub32, uap->buf, sizeof(ub32));
	return (error);
}

int
freebsd32_lstat(struct thread *td, struct freebsd32_lstat_args *uap)
{
	struct stat sb;
	struct stat32 sb32;
	int error;

	error = kern_lstat(td, uap->path, UIO_USERSPACE, &sb);
	if (error)
		return (error);
	copy_stat(&sb, &sb32);
	error = copyout(&sb32, uap->ub, sizeof (sb32));
	return (error);
}

#ifdef COMPAT_43
int
ofreebsd32_lstat(struct thread *td, struct ofreebsd32_lstat_args *uap)
{
	struct stat sb;
	struct ostat32 sb32;
	int error;

	error = kern_lstat(td, uap->path, UIO_USERSPACE, &sb);
	if (error)
		return (error);
	copy_ostat(&sb, &sb32);
	error = copyout(&sb32, uap->ub, sizeof (sb32));
	return (error);
}
#endif

int
freebsd32_sysctl(struct thread *td, struct freebsd32_sysctl_args *uap)
{
	int error, name[CTL_MAXNAME];
	size_t j, oldlen;

	if (uap->namelen > CTL_MAXNAME || uap->namelen < 2)
		return (EINVAL);
 	error = copyin(uap->name, name, uap->namelen * sizeof(int));
 	if (error)
		return (error);
	if (uap->oldlenp)
		oldlen = fuword32(uap->oldlenp);
	else
		oldlen = 0;
	error = userland_sysctl(td, name, uap->namelen,
		uap->old, &oldlen, 1,
		uap->new, uap->newlen, &j, SCTL_MASK32);
	if (error && error != ENOMEM)
		return (error);
	if (uap->oldlenp)
		suword32(uap->oldlenp, j);
	return (0);
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
	free(auio, M_IOV);
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
	free(auio, M_IOV);
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
	u_int32_t	sa_u;
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

int
ofreebsd32_sigprocmask(struct thread *td,
			       struct ofreebsd32_sigprocmask_args *uap)
{
	sigset_t set, oset;
	int error;

	OSIG2SIG(uap->mask, set);
	error = kern_sigprocmask(td, uap->how, &set, &oset, SIGPROCMASK_OLD);
	SIG2OSIG(oset, td->td_retval[0]);
	return (error);
}

int
ofreebsd32_sigpending(struct thread *td,
			      struct ofreebsd32_sigpending_args *uap)
{
	struct proc *p = td->td_proc;
	sigset_t siglist;

	PROC_LOCK(p);
	siglist = p->p_siglist;
	SIGSETOR(siglist, td->td_siglist);
	PROC_UNLOCK(p);
	SIG2OSIG(siglist, td->td_retval[0]);
	return (0);
}

struct sigvec32 {
	u_int32_t	sv_handler;
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

int
ofreebsd32_sigblock(struct thread *td,
			    struct ofreebsd32_sigblock_args *uap)
{
	sigset_t set, oset;

	OSIG2SIG(uap->mask, set);
	kern_sigprocmask(td, SIG_BLOCK, &set, &oset, 0);
	SIG2OSIG(oset, td->td_retval[0]);
	return (0);
}

int
ofreebsd32_sigsetmask(struct thread *td,
			      struct ofreebsd32_sigsetmask_args *uap)
{
	sigset_t set, oset;

	OSIG2SIG(uap->mask, set);
	kern_sigprocmask(td, SIG_SETMASK, &set, &oset, 0);
	SIG2OSIG(oset, td->td_retval[0]);
	return (0);
}

int
ofreebsd32_sigsuspend(struct thread *td,
			      struct ofreebsd32_sigsuspend_args *uap)
{
	sigset_t mask;

	OSIG2SIG(uap->mask, mask);
	return (kern_sigsuspend(td, mask));
}

struct sigstack32 {
	u_int32_t	ss_sp;
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
	struct timespec32 rmt32, rqt32;
	struct timespec rmt, rqt;
	int error;

	error = copyin(uap->rqtp, &rqt32, sizeof(rqt32));
	if (error)
		return (error);

	CP(rqt32, rqt, tv_sec);
	CP(rqt32, rqt, tv_nsec);

	if (uap->rmtp &&
	    !useracc((caddr_t)uap->rmtp, sizeof(rmt), VM_PROT_WRITE))
		return (EFAULT);
	error = kern_nanosleep(td, &rqt, &rmt);
	if (error && uap->rmtp) {
		int error2;

		CP(rmt, rmt32, tv_sec);
		CP(rmt, rmt32, tv_nsec);

		error2 = copyout(&rmt32, uap->rmtp, sizeof(rmt32));
		if (error2)
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
siginfo_to_siginfo32(const siginfo_t *src, struct siginfo32 *dst)
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

int
freebsd32_sigtimedwait(struct thread *td, struct freebsd32_sigtimedwait_args *uap)
{
	struct timespec32 ts32;
	struct timespec ts;
	struct timespec *timeout;
	sigset_t set;
	ksiginfo_t ksi;
	struct siginfo32 si32;
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
		error = copyout(&si32, uap->info, sizeof(struct siginfo32));
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
	struct siginfo32 si32;
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
		error = copyout(&si32, uap->info, sizeof(struct siginfo32));
	}	
	if (error == 0)
		td->td_retval[0] = ksi.ksi_signo;
	return (error);
}

int
freebsd32_cpuset_setid(struct thread *td,
    struct freebsd32_cpuset_setid_args *uap)
{
	struct cpuset_setid_args ap;

	ap.which = uap->which;
	ap.id = PAIR32TO64(id_t,uap->id);
	ap.setid = uap->setid;

	return (sys_cpuset_setid(td, &ap));
}

int
freebsd32_cpuset_getid(struct thread *td,
    struct freebsd32_cpuset_getid_args *uap)
{
	struct cpuset_getid_args ap;

	ap.level = uap->level;
	ap.which = uap->which;
	ap.id = PAIR32TO64(id_t,uap->id);
	ap.setid = uap->setid;

	return (sys_cpuset_getid(td, &ap));
}

int
freebsd32_cpuset_getaffinity(struct thread *td,
    struct freebsd32_cpuset_getaffinity_args *uap)
{
	struct cpuset_getaffinity_args ap;

	ap.level = uap->level;
	ap.which = uap->which;
	ap.id = PAIR32TO64(id_t,uap->id);
	ap.cpusetsize = uap->cpusetsize;
	ap.mask = uap->mask;

	return (sys_cpuset_getaffinity(td, &ap));
}

int
freebsd32_cpuset_setaffinity(struct thread *td,
    struct freebsd32_cpuset_setaffinity_args *uap)
{
	struct cpuset_setaffinity_args ap;

	ap.level = uap->level;
	ap.which = uap->which;
	ap.id = PAIR32TO64(id_t,uap->id);
	ap.cpusetsize = uap->cpusetsize;
	ap.mask = uap->mask;

	return (sys_cpuset_setaffinity(td, &ap));
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

	free(auio, M_IOV);
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
syscall32_register(int *offset, struct sysent *new_sysent,
    struct sysent *old_sysent)
{
	if (*offset == NO_SYSCALL) {
		int i;

		for (i = 1; i < SYS_MAXSYSCALL; ++i)
			if (freebsd32_sysent[i].sy_call ==
			    (sy_call_t *)lkmnosys)
				break;
		if (i == SYS_MAXSYSCALL)
			return (ENFILE);
		*offset = i;
	} else if (*offset < 0 || *offset >= SYS_MAXSYSCALL)
		return (EINVAL);
	else if (freebsd32_sysent[*offset].sy_call != (sy_call_t *)lkmnosys &&
	    freebsd32_sysent[*offset].sy_call != (sy_call_t *)lkmressys)
		return (EEXIST);

	*old_sysent = freebsd32_sysent[*offset];
	freebsd32_sysent[*offset] = *new_sysent;
	return 0;
}

int
syscall32_deregister(int *offset, struct sysent *old_sysent)
{

	if (*offset)
		freebsd32_sysent[*offset] = *old_sysent;
	return 0;
}

int
syscall32_module_handler(struct module *mod, int what, void *arg)
{
	struct syscall_module_data *data = (struct syscall_module_data*)arg;
	modspecific_t ms;
	int error;

	switch (what) {
	case MOD_LOAD:
		error = syscall32_register(data->offset, data->new_sysent,
		    &data->old_sysent);
		if (error) {
			/* Leave a mark so we know to safely unload below. */
			data->offset = NULL;
			return error;
		}
		ms.intval = *data->offset;
		MOD_XLOCK;
		module_setspecific(mod, &ms);
		MOD_XUNLOCK;
		if (data->chainevh)
			error = data->chainevh(mod, what, data->chainarg);
		return (error);
	case MOD_UNLOAD:
		/*
		 * MOD_LOAD failed, so just return without calling the
		 * chained handler since we didn't pass along the MOD_LOAD
		 * event.
		 */
		if (data->offset == NULL)
			return (0);
		if (data->chainevh) {
			error = data->chainevh(mod, what, data->chainarg);
			if (error)
				return (error);
		}
		error = syscall32_deregister(data->offset, &data->old_sysent);
		return (error);
	default:
		error = EOPNOTSUPP;
		if (data->chainevh)
			error = data->chainevh(mod, what, data->chainarg);
		return (error);
	}
}

int
syscall32_helper_register(struct syscall_helper_data *sd)
{
	struct syscall_helper_data *sd1;
	int error;

	for (sd1 = sd; sd1->syscall_no != NO_SYSCALL; sd1++) {
		error = syscall32_register(&sd1->syscall_no, &sd1->new_sysent,
		    &sd1->old_sysent);
		if (error != 0) {
			syscall32_helper_unregister(sd);
			return (error);
		}
		sd1->registered = 1;
	}
	return (0);
}

int
syscall32_helper_unregister(struct syscall_helper_data *sd)
{
	struct syscall_helper_data *sd1;

	for (sd1 = sd; sd1->registered != 0; sd1++) {
		syscall32_deregister(&sd1->syscall_no, &sd1->old_sysent);
		sd1->registered = 0;
	}
	return (0);
}

register_t *
freebsd32_copyout_strings(struct image_params *imgp)
{
	int argc, envc, i;
	u_int32_t *vectp;
	char *stringp, *destp;
	u_int32_t *stack_base;
	struct freebsd32_ps_strings *arginfo;
	char canary[sizeof(long) * 8];
	int32_t pagesizes32[MAXPAGESIZES];
	size_t execpath_len;
	int szsigcode;

	/*
	 * Calculate string base and vector table pointers.
	 * Also deal with signal trampoline code for this exec type.
	 */
	if (imgp->execpath != NULL && imgp->auxargs != NULL)
		execpath_len = strlen(imgp->execpath) + 1;
	else
		execpath_len = 0;
	arginfo = (struct freebsd32_ps_strings *)curproc->p_sysent->
	    sv_psstrings;
	if (imgp->proc->p_sysent->sv_sigcode_base == 0)
		szsigcode = *(imgp->proc->p_sysent->sv_szsigcode);
	else
		szsigcode = 0;
	destp =	(caddr_t)arginfo - szsigcode - SPARE_USRSPACE -
	    roundup(execpath_len, sizeof(char *)) -
	    roundup(sizeof(canary), sizeof(char *)) -
	    roundup(sizeof(pagesizes32), sizeof(char *)) -
	    roundup((ARG_MAX - imgp->args->stringspace), sizeof(char *));

	/*
	 * install sigcode
	 */
	if (szsigcode != 0)
		copyout(imgp->proc->p_sysent->sv_sigcode,
			((caddr_t)arginfo - szsigcode), szsigcode);

	/*
	 * Copy the image path for the rtld.
	 */
	if (execpath_len != 0) {
		imgp->execpathp = (uintptr_t)arginfo - szsigcode - execpath_len;
		copyout(imgp->execpath, (void *)imgp->execpathp,
		    execpath_len);
	}

	/*
	 * Prepare the canary for SSP.
	 */
	arc4rand(canary, sizeof(canary), 0);
	imgp->canary = (uintptr_t)arginfo - szsigcode - execpath_len -
	    sizeof(canary);
	copyout(canary, (void *)imgp->canary, sizeof(canary));
	imgp->canarylen = sizeof(canary);

	/*
	 * Prepare the pagesizes array.
	 */
	for (i = 0; i < MAXPAGESIZES; i++)
		pagesizes32[i] = (uint32_t)pagesizes[i];
	imgp->pagesizes = (uintptr_t)arginfo - szsigcode - execpath_len -
	    roundup(sizeof(canary), sizeof(char *)) - sizeof(pagesizes32);
	copyout(pagesizes32, (void *)imgp->pagesizes, sizeof(pagesizes32));
	imgp->pagesizeslen = sizeof(pagesizes32);

	/*
	 * If we have a valid auxargs ptr, prepare some room
	 * on the stack.
	 */
	if (imgp->auxargs) {
		/*
		 * 'AT_COUNT*2' is size for the ELF Auxargs data. This is for
		 * lower compatibility.
		 */
		imgp->auxarg_size = (imgp->auxarg_size) ? imgp->auxarg_size
			: (AT_COUNT * 2);
		/*
		 * The '+ 2' is for the null pointers at the end of each of
		 * the arg and env vector sets,and imgp->auxarg_size is room
		 * for argument of Runtime loader.
		 */
		vectp = (u_int32_t *) (destp - (imgp->args->argc +
		    imgp->args->envc + 2 + imgp->auxarg_size + execpath_len) *
		    sizeof(u_int32_t));
	} else
		/*
		 * The '+ 2' is for the null pointers at the end of each of
		 * the arg and env vector sets
		 */
		vectp = (u_int32_t *)
			(destp - (imgp->args->argc + imgp->args->envc + 2) * sizeof(u_int32_t));

	/*
	 * vectp also becomes our initial stack base
	 */
	stack_base = vectp;

	stringp = imgp->args->begin_argv;
	argc = imgp->args->argc;
	envc = imgp->args->envc;
	/*
	 * Copy out strings - arguments and environment.
	 */
	copyout(stringp, destp, ARG_MAX - imgp->args->stringspace);

	/*
	 * Fill in "ps_strings" struct for ps, w, etc.
	 */
	suword32(&arginfo->ps_argvstr, (u_int32_t)(intptr_t)vectp);
	suword32(&arginfo->ps_nargvstr, argc);

	/*
	 * Fill in argument portion of vector table.
	 */
	for (; argc > 0; --argc) {
		suword32(vectp++, (u_int32_t)(intptr_t)destp);
		while (*stringp++ != 0)
			destp++;
		destp++;
	}

	/* a null vector table pointer separates the argp's from the envp's */
	suword32(vectp++, 0);

	suword32(&arginfo->ps_envstr, (u_int32_t)(intptr_t)vectp);
	suword32(&arginfo->ps_nenvstr, envc);

	/*
	 * Fill in environment portion of vector table.
	 */
	for (; envc > 0; --envc) {
		suword32(vectp++, (u_int32_t)(intptr_t)destp);
		while (*stringp++ != 0)
			destp++;
		destp++;
	}

	/* end of vector table is a null pointer */
	suword32(vectp, 0);

	return ((register_t *)stack_base);
}

int
freebsd32_kldstat(struct thread *td, struct freebsd32_kldstat_args *uap)
{
	struct kld_file_stat stat;
	struct kld32_file_stat stat32;
	int error, version;

	if ((error = copyin(&uap->stat->version, &version, sizeof(version)))
	    != 0)
		return (error);
	if (version != sizeof(struct kld32_file_stat_1) &&
	    version != sizeof(struct kld32_file_stat))
		return (EINVAL);

	error = kern_kldstat(td, uap->fileid, &stat);
	if (error != 0)
		return (error);

	bcopy(&stat.name[0], &stat32.name[0], sizeof(stat.name));
	CP(stat, stat32, refs);
	CP(stat, stat32, id);
	PTROUT_CP(stat, stat32, address);
	CP(stat, stat32, size);
	bcopy(&stat.pathname[0], &stat32.pathname[0], sizeof(stat.pathname));
	return (copyout(&stat32, uap->stat, version));
}

int
freebsd32_posix_fallocate(struct thread *td,
    struct freebsd32_posix_fallocate_args *uap)
{

	return (kern_posix_fallocate(td, uap->fd,
	    PAIR32TO64(off_t, uap->offset), PAIR32TO64(off_t, uap->len)));
}

int
freebsd32_posix_fadvise(struct thread *td,
    struct freebsd32_posix_fadvise_args *uap)
{

	return (kern_posix_fadvise(td, uap->fd, PAIR32TO64(off_t, uap->offset),
	    PAIR32TO64(off_t, uap->len), uap->advice));
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
	int error, flags;

	switch (uap->com) {
	case PROC_SPROTECT:
		error = copyin(PTRIN(uap->data), &flags, sizeof(flags));
		if (error)
			return (error);
		data = &flags;
		break;
	default:
		return (EINVAL);
	}
	return (kern_procctl(td, uap->idtype, PAIR32TO64(id_t, uap->id),
	    uap->com, data));
}
