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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/exec.h>
#include <sys/fcntl.h>
#include <sys/filedesc.h>
#include <sys/imgact.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/file.h>		/* Must come after sys/malloc.h */
#include <sys/mman.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/selinfo.h>
#include <sys/pipe.h>		/* Must come after sys/selinfo.h */
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/unistd.h>
#include <sys/user.h>
#include <sys/utsname.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>

#include <compat/freebsd32/freebsd32_util.h>
#include <compat/freebsd32/freebsd32.h>
#include <compat/freebsd32/freebsd32_proto.h>

/*
 * [ taken from the linux emulator ]
 * Search an alternate path before passing pathname arguments on
 * to system calls. Useful for keeping a separate 'emulation tree'.
 *
 * If cflag is set, we check if an attempt can be made to create
 * the named file, i.e. we check if the directory it should
 * be in exists.
 */
int
freebsd32_emul_find(td, sgp, prefix, path, pbuf, cflag)
	struct thread	*td;
	caddr_t		*sgp;		/* Pointer to stackgap memory */
	const char	*prefix;
	char		*path;
	char		**pbuf;
	int		cflag;
{
	int			error;
	size_t			len, sz;
	char			*buf, *cp, *ptr;
	struct ucred		*ucred;
	struct nameidata	nd;
	struct nameidata	ndroot;
	struct vattr		vat;
	struct vattr		vatroot;

	buf = (char *) malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
	*pbuf = path;

	for (ptr = buf; (*ptr = *prefix) != '\0'; ptr++, prefix++)
		continue;

	sz = MAXPATHLEN - (ptr - buf);

	/*
	 * If sgp is not given then the path is already in kernel space
	 */
	if (sgp == NULL)
		error = copystr(path, ptr, sz, &len);
	else
		error = copyinstr(path, ptr, sz, &len);

	if (error) {
		free(buf, M_TEMP);
		return error;
	}

	if (*ptr != '/') {
		free(buf, M_TEMP);
		return EINVAL;
	}

	/*
	 *  We know that there is a / somewhere in this pathname.
	 *  Search backwards for it, to find the file's parent dir
	 *  to see if it exists in the alternate tree. If it does,
	 *  and we want to create a file (cflag is set). We don't
	 *  need to worry about the root comparison in this case.
	 */

	if (cflag) {
		for (cp = &ptr[len] - 1; *cp != '/'; cp--)
			;
		*cp = '\0';

		NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, buf, td);

		if ((error = namei(&nd)) != 0) {
			free(buf, M_TEMP);
			return error;
		}

		*cp = '/';
	} else {
		NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, buf, td);

		if ((error = namei(&nd)) != 0) {
			free(buf, M_TEMP);
			return error;
		}

		/*
		 * We now compare the vnode of the freebsd32_root to the one
		 * vnode asked. If they resolve to be the same, then we
		 * ignore the match so that the real root gets used.
		 * This avoids the problem of traversing "../.." to find the
		 * root directory and never finding it, because "/" resolves
		 * to the emulation root directory. This is expensive :-(
		 */
		NDINIT(&ndroot, LOOKUP, FOLLOW, UIO_SYSSPACE,
		    freebsd32_emul_path, td);

		if ((error = namei(&ndroot)) != 0) {
			/* Cannot happen! */
			free(buf, M_TEMP);
			vrele(nd.ni_vp);
			return error;
		}

		ucred = td->td_ucred;
		if ((error = VOP_GETATTR(nd.ni_vp, &vat, ucred, td)) != 0) {
			goto bad;
		}

		if ((error = VOP_GETATTR(ndroot.ni_vp, &vatroot, ucred,
		    td)) != 0) {
			goto bad;
		}

		if (vat.va_fsid == vatroot.va_fsid &&
		    vat.va_fileid == vatroot.va_fileid) {
			error = ENOENT;
			goto bad;
		}

	}
	if (sgp == NULL)
		*pbuf = buf;
	else {
		sz = &ptr[len] - buf;
		*pbuf = stackgap_alloc(sgp, sz + 1);
		error = copyout(buf, *pbuf, sz);
		free(buf, M_TEMP);
	}

	vrele(nd.ni_vp);
	if (!cflag)
		vrele(ndroot.ni_vp);

	return error;

bad:
	vrele(ndroot.ni_vp);
	vrele(nd.ni_vp);
	free(buf, M_TEMP);
	return error;
}

int
freebsd32_open(struct thread *td, struct freebsd32_open_args *uap)
{
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(td, &sg, uap->path);

	return open(td, (struct open_args *) uap);
}

int
freebsd32_wait4(struct thread *td, struct freebsd32_wait4_args *uap)
{
	int error;
	caddr_t sg;
	struct rusage32 *rusage32, ru32;
	struct rusage *rusage = NULL, ru;

	rusage32 = uap->rusage;
	if (rusage32) {
		sg = stackgap_init();
		rusage = stackgap_alloc(&sg, sizeof(struct rusage));
		uap->rusage = (struct rusage32 *)rusage;
	}
	error = wait4(td, (struct wait_args *)uap);
	if (error)
		return (error);
	if (rusage32 && (error = copyin(rusage, &ru, sizeof(ru)) == 0)) {
		TV_CP(ru, ru32, ru_utime);
		TV_CP(ru, ru32, ru_stime);
		CP(ru, ru32, ru_maxrss);
		CP(ru, ru32, ru_ixrss);
		CP(ru, ru32, ru_idrss);
		CP(ru, ru32, ru_isrss);
		CP(ru, ru32, ru_minflt);
		CP(ru, ru32, ru_majflt);
		CP(ru, ru32, ru_nswap);
		CP(ru, ru32, ru_inblock);
		CP(ru, ru32, ru_oublock);
		CP(ru, ru32, ru_msgsnd);
		CP(ru, ru32, ru_msgrcv);
		CP(ru, ru32, ru_nsignals);
		CP(ru, ru32, ru_nvcsw);
		CP(ru, ru32, ru_nivcsw);
		error = copyout(&ru32, rusage32, sizeof(ru32));
	}
	return (error);
}

static void
copy_statfs(struct statfs *in, struct statfs32 *out)
{
	CP(*in, *out, f_bsize);
	CP(*in, *out, f_iosize);
	CP(*in, *out, f_blocks);
	CP(*in, *out, f_bfree);
	CP(*in, *out, f_bavail);
	CP(*in, *out, f_files);
	CP(*in, *out, f_ffree);
	CP(*in, *out, f_fsid);
	CP(*in, *out, f_owner);
	CP(*in, *out, f_type);
	CP(*in, *out, f_flags);
	CP(*in, *out, f_flags);
	CP(*in, *out, f_syncwrites);
	CP(*in, *out, f_asyncwrites);
	bcopy(in->f_fstypename,
	      out->f_fstypename, MFSNAMELEN);
	bcopy(in->f_mntonname,
	      out->f_mntonname, MNAMELEN);
	CP(*in, *out, f_syncreads);
	CP(*in, *out, f_asyncreads);
	bcopy(in->f_mntfromname,
	      out->f_mntfromname, MNAMELEN);
}

int
freebsd32_getfsstat(struct thread *td, struct freebsd32_getfsstat_args *uap)
{
	int error;
	caddr_t sg;
	struct statfs32 *sp32, stat32;
	struct statfs *sp = NULL, stat;
	int maxcount, count, i;

	sp32 = uap->buf;
	maxcount = uap->bufsize / sizeof(struct statfs32);

	if (sp32) {
		sg = stackgap_init();
		sp = stackgap_alloc(&sg, sizeof(struct statfs) * maxcount);
		uap->buf = (struct statfs32 *)sp;
	}
	error = getfsstat(td, (struct getfsstat_args *) uap);
	if (sp32 && !error) {
		count = td->td_retval[0];
		for (i = 0; i < count; i++) {
			error = copyin(&sp[i], &stat, sizeof(stat));
			if (error)
				return (error);
			copy_statfs(&stat, &stat32);
			error = copyout(&stat32, &sp32[i], sizeof(stat32));
			if (error)
				return (error);
		}
	}
	return (error);
}

int
freebsd32_access(struct thread *td, struct freebsd32_access_args *uap)
{
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(td, &sg, uap->path);

	return access(td, (struct access_args *)uap);
}

int
freebsd32_chflags(struct thread *td, struct freebsd32_chflags_args *uap)
{
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(td, &sg, uap->path);

	return chflags(td, (struct chflags_args *)uap);
}

struct sigaltstack32 {
	u_int32_t	ss_sp;
	u_int32_t	ss_size;
	int		ss_flags;
};

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

int
freebsd32_execve(struct thread *td, struct freebsd32_execve_args *uap)
{
	int error;
	caddr_t sg;
	struct execve_args ap;
	u_int32_t *p32, arg;
	char **p;
	int count;

	sg = stackgap_init();
	CHECKALTEXIST(td, &sg, uap->fname);
	ap.fname = uap->fname;

	if (uap->argv) {
		count = 0;
		p32 = uap->argv;
		do {
			error = copyin(p32++, &arg, sizeof(arg));
			if (error)
				return error;
			count++;
		} while (arg != 0);
		p = stackgap_alloc(&sg, count * sizeof(char *));
		ap.argv = p;
		p32 = uap->argv;
		do {
			error = copyin(p32++, &arg, sizeof(arg));
			if (error)
				return error;
			*p++ = PTRIN(arg);
		} while (arg != 0);
	}
	if (uap->envv) {
		count = 0;
		p32 = uap->envv;
		do {
			error = copyin(p32++, &arg, sizeof(arg));
			if (error)
				return error;
			count++;
		} while (arg != 0);
		p = stackgap_alloc(&sg, count * sizeof(char *));
		ap.envv = p;
		p32 = uap->envv;
		do {
			error = copyin(p32++, &arg, sizeof(arg));
			if (error)
				return error;
			*p++ = PTRIN(arg);
		} while (arg != 0);
	}

	return execve(td, &ap);
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
		rv = vm_map_find(map, 0, 0,
				 &addr, PAGE_SIZE, FALSE, prot,
				 VM_PROT_ALL, 0);
		if (rv != KERN_SUCCESS)
			return (EINVAL);
	}

	if (fd != -1) {
		struct pread_args r;
		r.fd = fd;
		r.buf = (void *) start;
		r.nbyte = end - start;
		r.offset = pos;
		return (pread(td, &r));
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
freebsd32_mmap(struct thread *td, struct freebsd32_mmap_args *uap)
{
	struct mmap_args ap;
	vm_offset_t addr = (vm_offset_t) uap->addr;
	vm_size_t len	 = uap->len;
	int prot	 = uap->prot;
	int flags	 = uap->flags;
	int fd		 = uap->fd;
	off_t pos	 = (uap->poslo
			    | ((off_t)uap->poshi << 32));
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
			rv = vm_map_find(map, 0, 0,
					 &start, end - start, FALSE,
					 prot, VM_PROT_ALL, 0);
			if (rv != KERN_SUCCESS)
				return (EINVAL);
			r.fd = fd;
			r.buf = (void *) start;
			r.nbyte = end - start;
			r.offset = pos;
			error = pread(td, &r);
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

	ap.addr = (void *) addr;
	ap.len = len;
	ap.prot = prot;
	ap.flags = flags;
	ap.fd = fd;
	ap.pos = pos;

	return (mmap(td, &ap));
}

struct itimerval32 {
	struct timeval32 it_interval;
	struct timeval32 it_value;
};

int
freebsd32_setitimer(struct thread *td, struct freebsd32_setitimer_args *uap)
{
	int error;
	caddr_t sg;
	struct itimerval32 *p32, *op32, s32;
	struct itimerval *p = NULL, *op = NULL, s;

	p32 = uap->itv;
	if (p32) {
		sg = stackgap_init();
		p = stackgap_alloc(&sg, sizeof(struct itimerval));
		uap->itv = (struct itimerval32 *)p;
		error = copyin(p32, &s32, sizeof(s32));
		if (error)
			return (error);
		TV_CP(s32, s, it_interval);
		TV_CP(s32, s, it_value);
		error = copyout(&s, p, sizeof(s));
		if (error)
			return (error);
	}
	op32 = uap->oitv;
	if (op32) {
		sg = stackgap_init();
		op = stackgap_alloc(&sg, sizeof(struct itimerval));
		uap->oitv = (struct itimerval32 *)op;
	}
	error = setitimer(td, (struct setitimer_args *) uap);
	if (error)
		return (error);
	if (op32) {
		error = copyin(op, &s, sizeof(s));
		if (error)
			return (error);
		TV_CP(s, s32, it_interval);
		TV_CP(s, s32, it_value);
		error = copyout(&s32, op32, sizeof(s32));
	}
	return (error);
}

int
freebsd32_select(struct thread *td, struct freebsd32_select_args *uap)
{
	int error;
	caddr_t sg;
	struct timeval32 *p32, s32;
	struct timeval *p = NULL, s;

	p32 = uap->tv;
	if (p32) {
		sg = stackgap_init();
		p = stackgap_alloc(&sg, sizeof(struct timeval));
		uap->tv = (struct timeval32 *)p;
		error = copyin(p32, &s32, sizeof(s32));
		if (error)
			return (error);
		CP(s32, s, tv_sec);
		CP(s32, s, tv_usec);
		error = copyout(&s, p, sizeof(s));
		if (error)
			return (error);
	}
	/*
	 * XXX big-endian needs to convert the fd_sets too.
	 */
	return (select(td, (struct select_args *) uap));
}

struct kevent32 {
	u_int32_t	ident;		/* identifier for this event */
	short		filter;		/* filter for event */
	u_short		flags;
	u_int		fflags;
	int32_t		data;
	u_int32_t	udata;		/* opaque user data identifier */
};

int
freebsd32_kevent(struct thread *td, struct freebsd32_kevent_args *uap)
{
	int error;
	caddr_t sg;
	struct timespec32 ts32;
	struct timespec ts;
	struct kevent32 ks32;
	struct kevent *ks;
	struct kevent_args a;
	int i;

	sg = stackgap_init();

	a.fd = uap->fd;
	a.changelist = uap->changelist;
	a.nchanges = uap->nchanges;
	a.eventlist = uap->eventlist;
	a.nevents = uap->nevents;
	a.timeout = NULL;

	if (uap->timeout) {
		a.timeout = stackgap_alloc(&sg, sizeof(struct timespec));
		error = copyin(uap->timeout, &ts32, sizeof(ts32));
		if (error)
			return (error);
		CP(ts32, ts, tv_sec);
		CP(ts32, ts, tv_nsec);
		error = copyout(&ts, (void *)(uintptr_t)a.timeout, sizeof(ts));
		if (error)
			return (error);
	}
	if (uap->changelist) {
		a.changelist = (struct kevent *)stackgap_alloc(&sg,
		    uap->nchanges * sizeof(struct kevent));
		for (i = 0; i < uap->nchanges; i++) {
			error = copyin(&uap->changelist[i], &ks32,
			    sizeof(ks32));
			if (error)
				return (error);
			ks = (struct kevent *)(uintptr_t)&a.changelist[i];
			CP(ks32, *ks, ident);
			CP(ks32, *ks, filter);
			CP(ks32, *ks, flags);
			CP(ks32, *ks, fflags);
			CP(ks32, *ks, data);
			PTRIN_CP(ks32, *ks, udata);
		}
	}
	if (uap->eventlist) {
		a.eventlist = stackgap_alloc(&sg,
		    uap->nevents * sizeof(struct kevent));
	}
	error = kevent(td, &a);
	if (uap->eventlist && error > 0) {
		for (i = 0; i < error; i++) {
			ks = &a.eventlist[i];
			CP(*ks, ks32, ident);
			CP(*ks, ks32, filter);
			CP(*ks, ks32, flags);
			CP(*ks, ks32, fflags);
			CP(*ks, ks32, data);
			PTROUT_CP(*ks, ks32, udata);
			error = copyout(&ks32, &uap->eventlist[i],
			    sizeof(ks32));
			if (error)
				return (error);
		}
	}
	return error;
}

int
freebsd32_gettimeofday(struct thread *td,
		       struct freebsd32_gettimeofday_args *uap)
{
	int error;
	caddr_t sg;
	struct timeval32 *p32, s32;
	struct timeval *p = NULL, s;

	p32 = uap->tp;
	if (p32) {
		sg = stackgap_init();
		p = stackgap_alloc(&sg, sizeof(struct timeval));
		uap->tp = (struct timeval32 *)p;
	}
	error = gettimeofday(td, (struct gettimeofday_args *) uap);
	if (error)
		return (error);
	if (p32) {
		error = copyin(p, &s, sizeof(s));
		if (error)
			return (error);
		CP(s, s32, tv_sec);
		CP(s, s32, tv_usec);
		error = copyout(&s32, p32, sizeof(s32));
		if (error)
			return (error);
	}
	return (error);
}

int
freebsd32_getrusage(struct thread *td, struct freebsd32_getrusage_args *uap)
{
	int error;
	caddr_t sg;
	struct rusage32 *p32, s32;
	struct rusage *p = NULL, s;

	p32 = uap->rusage;
	if (p32) {
		sg = stackgap_init();
		p = stackgap_alloc(&sg, sizeof(struct rusage));
		uap->rusage = (struct rusage32 *)p;
	}
	error = getrusage(td, (struct getrusage_args *) uap);
	if (error)
		return (error);
	if (p32) {
		error = copyin(p, &s, sizeof(s));
		if (error)
			return (error);
		TV_CP(s, s32, ru_utime);
		TV_CP(s, s32, ru_stime);
		CP(s, s32, ru_maxrss);
		CP(s, s32, ru_ixrss);
		CP(s, s32, ru_idrss);
		CP(s, s32, ru_isrss);
		CP(s, s32, ru_minflt);
		CP(s, s32, ru_majflt);
		CP(s, s32, ru_nswap);
		CP(s, s32, ru_inblock);
		CP(s, s32, ru_oublock);
		CP(s, s32, ru_msgsnd);
		CP(s, s32, ru_msgrcv);
		CP(s, s32, ru_nsignals);
		CP(s, s32, ru_nvcsw);
		CP(s, s32, ru_nivcsw);
		error = copyout(&s32, p32, sizeof(s32));
	}
	return (error);
}

struct iovec32 {
	u_int32_t iov_base;
	int	iov_len;
};
#define	STACKGAPLEN	400

int
freebsd32_readv(struct thread *td, struct freebsd32_readv_args *uap)
{
	int error, osize, nsize, i;
	caddr_t sg;
	struct readv_args /* {
		syscallarg(int) fd;
		syscallarg(struct iovec *) iovp;
		syscallarg(u_int) iovcnt;
	} */ a;
	struct iovec32 *oio;
	struct iovec *nio;

	sg = stackgap_init();

	if (uap->iovcnt > (STACKGAPLEN / sizeof (struct iovec)))
		return (EINVAL);

	osize = uap->iovcnt * sizeof (struct iovec32);
	nsize = uap->iovcnt * sizeof (struct iovec);

	oio = malloc(osize, M_TEMP, M_WAITOK);
	nio = malloc(nsize, M_TEMP, M_WAITOK);

	error = 0;
	if ((error = copyin(uap->iovp, oio, osize)))
		goto punt;
	for (i = 0; i < uap->iovcnt; i++) {
		nio[i].iov_base = PTRIN(oio[i].iov_base);
		nio[i].iov_len = oio[i].iov_len;
	}

	a.fd = uap->fd;
	a.iovp = stackgap_alloc(&sg, nsize);
	a.iovcnt = uap->iovcnt;

	if ((error = copyout(nio, (caddr_t)a.iovp, nsize)))
		goto punt;
	error = readv(td, &a);

punt:
	free(oio, M_TEMP);
	free(nio, M_TEMP);
	return (error);
}

int
freebsd32_writev(struct thread *td, struct freebsd32_writev_args *uap)
{
	int error, i, nsize, osize;
	caddr_t sg;
	struct writev_args /* {
		syscallarg(int) fd;
		syscallarg(struct iovec *) iovp;
		syscallarg(u_int) iovcnt;
	} */ a;
	struct iovec32 *oio;
	struct iovec *nio;

	sg = stackgap_init();

	if (uap->iovcnt > (STACKGAPLEN / sizeof (struct iovec)))
		return (EINVAL);

	osize = uap->iovcnt * sizeof (struct iovec32);
	nsize = uap->iovcnt * sizeof (struct iovec);

	oio = malloc(osize, M_TEMP, M_WAITOK);
	nio = malloc(nsize, M_TEMP, M_WAITOK);

	error = 0;
	if ((error = copyin(uap->iovp, oio, osize)))
		goto punt;
	for (i = 0; i < uap->iovcnt; i++) {
		nio[i].iov_base = PTRIN(oio[i].iov_base);
		nio[i].iov_len = oio[i].iov_len;
	}

	a.fd = uap->fd;
	a.iovp = stackgap_alloc(&sg, nsize);
	a.iovcnt = uap->iovcnt;

	if ((error = copyout(nio, (caddr_t)a.iovp, nsize)))
		goto punt;
	error = writev(td, &a);

punt:
	free(oio, M_TEMP);
	free(nio, M_TEMP);
	return (error);
}

int
freebsd32_settimeofday(struct thread *td,
		       struct freebsd32_settimeofday_args *uap)
{
	int error;
	caddr_t sg;
	struct timeval32 *p32, s32;
	struct timeval *p = NULL, s;

	p32 = uap->tv;
	if (p32) {
		sg = stackgap_init();
		p = stackgap_alloc(&sg, sizeof(struct timeval));
		uap->tv = (struct timeval32 *)p;
		error = copyin(p32, &s32, sizeof(s32));
		if (error)
			return (error);
		CP(s32, s, tv_sec);
		CP(s32, s, tv_usec);
		error = copyout(&s, p, sizeof(s));
		if (error)
			return (error);
	}
	return (settimeofday(td, (struct settimeofday_args *) uap));
}

int
freebsd32_utimes(struct thread *td, struct freebsd32_utimes_args *uap)
{
	int error;
	caddr_t sg;
	struct timeval32 *p32, s32[2];
	struct timeval *p = NULL, s[2];

	p32 = uap->tptr;
	if (p32) {
		sg = stackgap_init();
		p = stackgap_alloc(&sg, 2*sizeof(struct timeval));
		uap->tptr = (struct timeval32 *)p;
		error = copyin(p32, s32, sizeof(s32));
		if (error)
			return (error);
		CP(s32[0], s[0], tv_sec);
		CP(s32[0], s[0], tv_usec);
		CP(s32[1], s[1], tv_sec);
		CP(s32[1], s[1], tv_usec);
		error = copyout(s, p, sizeof(s));
		if (error)
			return (error);
	}
	return (utimes(td, (struct utimes_args *) uap));
}

int
freebsd32_adjtime(struct thread *td, struct freebsd32_adjtime_args *uap)
{
	int error;
	caddr_t sg;
	struct timeval32 *p32, *op32, s32;
	struct timeval *p = NULL, *op = NULL, s;

	p32 = uap->delta;
	if (p32) {
		sg = stackgap_init();
		p = stackgap_alloc(&sg, sizeof(struct timeval));
		uap->delta = (struct timeval32 *)p;
		error = copyin(p32, &s32, sizeof(s32));
		if (error)
			return (error);
		CP(s32, s, tv_sec);
		CP(s32, s, tv_usec);
		error = copyout(&s, p, sizeof(s));
		if (error)
			return (error);
	}
	op32 = uap->olddelta;
	if (op32) {
		sg = stackgap_init();
		op = stackgap_alloc(&sg, sizeof(struct timeval));
		uap->olddelta = (struct timeval32 *)op;
	}
	error = utimes(td, (struct utimes_args *) uap);
	if (error)
		return error;
	if (op32) {
		error = copyin(op, &s, sizeof(s));
		if (error)
			return (error);
		CP(s, s32, tv_sec);
		CP(s, s32, tv_usec);
		error = copyout(&s32, op32, sizeof(s32));
	}
	return (error);
}

int
freebsd32_statfs(struct thread *td, struct freebsd32_statfs_args *uap)
{
	int error;
	caddr_t sg;
	struct statfs32 *p32, s32;
	struct statfs *p = NULL, s;

	p32 = uap->buf;
	if (p32) {
		sg = stackgap_init();
		p = stackgap_alloc(&sg, sizeof(struct statfs));
		uap->buf = (struct statfs32 *)p;
	}
	error = statfs(td, (struct statfs_args *) uap);
	if (error)
		return (error);
	if (p32) {
		error = copyin(p, &s, sizeof(s));
		if (error)
			return (error);
		copy_statfs(&s, &s32);
		error = copyout(&s32, p32, sizeof(s32));
	}
	return (error);
}

int
freebsd32_fstatfs(struct thread *td, struct freebsd32_fstatfs_args *uap)
{
	int error;
	caddr_t sg;
	struct statfs32 *p32, s32;
	struct statfs *p = NULL, s;

	p32 = uap->buf;
	if (p32) {
		sg = stackgap_init();
		p = stackgap_alloc(&sg, sizeof(struct statfs));
		uap->buf = (struct statfs32 *)p;
	}
	error = fstatfs(td, (struct fstatfs_args *) uap);
	if (error)
		return (error);
	if (p32) {
		error = copyin(p, &s, sizeof(s));
		if (error)
			return (error);
		copy_statfs(&s, &s32);
		error = copyout(&s32, p32, sizeof(s32));
	}
	return (error);
}

int
freebsd32_semsys(struct thread *td, struct freebsd32_semsys_args *uap)
{
	/*
	 * Vector through to semsys if it is loaded.
	 */
	return sysent[169].sy_call(td, uap);
}

int
freebsd32_msgsys(struct thread *td, struct freebsd32_msgsys_args *uap)
{
	/*
	 * Vector through to msgsys if it is loaded.
	 */
	return sysent[170].sy_call(td, uap);
}

int
freebsd32_shmsys(struct thread *td, struct freebsd32_shmsys_args *uap)
{
	/*
	 * Vector through to shmsys if it is loaded.
	 */
	return sysent[171].sy_call(td, uap);
}

int
freebsd32_pread(struct thread *td, struct freebsd32_pread_args *uap)
{
	struct pread_args ap;

	ap.fd = uap->fd;
	ap.buf = uap->buf;
	ap.nbyte = uap->nbyte;
	ap.offset = (uap->offsetlo | ((off_t)uap->offsethi << 32));
	return (pread(td, &ap));
}

int
freebsd32_pwrite(struct thread *td, struct freebsd32_pwrite_args *uap)
{
	struct pwrite_args ap;

	ap.fd = uap->fd;
	ap.buf = uap->buf;
	ap.nbyte = uap->nbyte;
	ap.offset = (uap->offsetlo | ((off_t)uap->offsethi << 32));
	return (pwrite(td, &ap));
}

int
freebsd32_lseek(struct thread *td, struct freebsd32_lseek_args *uap)
{
	int error;
	struct lseek_args ap;
	off_t pos;

	ap.fd = uap->fd;
	ap.offset = (uap->offsetlo | ((off_t)uap->offsethi << 32));
	ap.whence = uap->whence;
	error = lseek(td, &ap);
	/* Expand the quad return into two parts for eax and edx */
	pos = *(off_t *)(td->td_retval);
	td->td_retval[0] = pos & 0xffffffff;	/* %eax */
	td->td_retval[1] = pos >> 32;		/* %edx */
	return error;
}

int
freebsd32_truncate(struct thread *td, struct freebsd32_truncate_args *uap)
{
	struct truncate_args ap;

	ap.path = uap->path;
	ap.length = (uap->lengthlo | ((off_t)uap->lengthhi << 32));
	return (truncate(td, &ap));
}

int
freebsd32_ftruncate(struct thread *td, struct freebsd32_ftruncate_args *uap)
{
	struct ftruncate_args ap;

	ap.fd = uap->fd;
	ap.length = (uap->lengthlo | ((off_t)uap->lengthhi << 32));
	return (ftruncate(td, &ap));
}

#ifdef COMPAT_FREEBSD4
int
freebsd4_freebsd32_sendfile(struct thread *td,
    struct freebsd4_freebsd32_sendfile_args *uap)
{
	struct freebsd4_sendfile_args ap;

	ap.fd = uap->fd;
	ap.s = uap->s;
	ap.offset = (uap->offsetlo | ((off_t)uap->offsethi << 32));
	ap.nbytes = uap->nbytes;	/* XXX check */
	ap.hdtr = uap->hdtr;		/* XXX check */
	ap.sbytes = uap->sbytes;	/* XXX FIXME!! */
	ap.flags = uap->flags;
	return (freebsd4_sendfile(td, &ap));
}
#endif

int
freebsd32_sendfile(struct thread *td, struct freebsd32_sendfile_args *uap)
{
	struct sendfile_args ap;

	ap.fd = uap->fd;
	ap.s = uap->s;
	ap.offset = (uap->offsetlo | ((off_t)uap->offsethi << 32));
	ap.nbytes = uap->nbytes;	/* XXX check */
	ap.hdtr = uap->hdtr;		/* XXX check */
	ap.sbytes = uap->sbytes;	/* XXX FIXME!! */
	ap.flags = uap->flags;
	return (sendfile(td, &ap));
}

struct stat32 {
	udev_t	st_dev;
	ino_t	st_ino;
	mode_t	st_mode;
	nlink_t	st_nlink;
	uid_t	st_uid;
	gid_t	st_gid;
	udev_t	st_rdev;
	struct timespec32 st_atimespec;
	struct timespec32 st_mtimespec;
	struct timespec32 st_ctimespec;
	off_t	st_size;
	int64_t	st_blocks;
	u_int32_t st_blksize;
	u_int32_t st_flags;
	u_int32_t st_gen;
};

static void
copy_stat( struct stat *in, struct stat32 *out)
{
	CP(*in, *out, st_dev);
	CP(*in, *out, st_ino);
	CP(*in, *out, st_mode);
	CP(*in, *out, st_nlink);
	CP(*in, *out, st_uid);
	CP(*in, *out, st_gid);
	CP(*in, *out, st_rdev);
	TS_CP(*in, *out, st_atimespec);
	TS_CP(*in, *out, st_mtimespec);
	TS_CP(*in, *out, st_ctimespec);
	CP(*in, *out, st_size);
	CP(*in, *out, st_blocks);
	CP(*in, *out, st_blksize);
	CP(*in, *out, st_flags);
	CP(*in, *out, st_gen);
}

int
freebsd32_stat(struct thread *td, struct freebsd32_stat_args *uap)
{
	int error;
	caddr_t sg;
	struct stat32 *p32, s32;
	struct stat *p = NULL, s;

	p32 = uap->ub;
	if (p32) {
		sg = stackgap_init();
		p = stackgap_alloc(&sg, sizeof(struct stat));
		uap->ub = (struct stat32 *)p;
	}
	error = stat(td, (struct stat_args *) uap);
	if (error)
		return (error);
	if (p32) {
		error = copyin(p, &s, sizeof(s));
		if (error)
			return (error);
		copy_stat(&s, &s32);
		error = copyout(&s32, p32, sizeof(s32));
	}
	return (error);
}

int
freebsd32_fstat(struct thread *td, struct freebsd32_fstat_args *uap)
{
	int error;
	caddr_t sg;
	struct stat32 *p32, s32;
	struct stat *p = NULL, s;

	p32 = uap->ub;
	if (p32) {
		sg = stackgap_init();
		p = stackgap_alloc(&sg, sizeof(struct stat));
		uap->ub = (struct stat32 *)p;
	}
	error = fstat(td, (struct fstat_args *) uap);
	if (error)
		return (error);
	if (p32) {
		error = copyin(p, &s, sizeof(s));
		if (error)
			return (error);
		copy_stat(&s, &s32);
		error = copyout(&s32, p32, sizeof(s32));
	}
	return (error);
}

int
freebsd32_lstat(struct thread *td, struct freebsd32_lstat_args *uap)
{
	int error;
	caddr_t sg;
	struct stat32 *p32, s32;
	struct stat *p = NULL, s;

	p32 = uap->ub;
	if (p32) {
		sg = stackgap_init();
		p = stackgap_alloc(&sg, sizeof(struct stat));
		uap->ub = (struct stat32 *)p;
	}
	error = lstat(td, (struct lstat_args *) uap);
	if (error)
		return (error);
	if (p32) {
		error = copyin(p, &s, sizeof(s));
		if (error)
			return (error);
		copy_stat(&s, &s32);
		error = copyout(&s32, p32, sizeof(s32));
	}
	return (error);
}

/*
 * MPSAFE
 */
int
freebsd32_sysctl(struct thread *td, struct freebsd32_sysctl_args *uap)
{
	int error, name[CTL_MAXNAME];
	size_t j, oldlen;

	if (uap->namelen > CTL_MAXNAME || uap->namelen < 2)
		return (EINVAL);

 	error = copyin(uap->name, &name, uap->namelen * sizeof(int));
 	if (error)
		return (error);

	mtx_lock(&Giant);

	if (uap->oldlenp)
		oldlen = fuword32(uap->oldlenp);
	else
		oldlen = 0;
	error = userland_sysctl(td, name, uap->namelen,
		uap->old, &oldlen, 1,
		uap->new, uap->newlen, &j);
	if (error && error != ENOMEM)
		goto done2;
	if (uap->oldlenp) {
		suword32(uap->oldlenp, j);
	}
done2:
	mtx_unlock(&Giant);
	return (error);
}

struct sigaction32 {
	u_int32_t	sa_u;
	int		sa_flags;
	sigset_t	sa_mask;
};

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
	if (error != 0 && uap->oact != NULL) {
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
	if (error != 0 && uap->oact != NULL) {
		s32.sa_u = PTROUT(osa.sa_handler);
		CP(osa, s32, sa_flags);
		CP(osa, s32, sa_mask);
		error = copyout(&s32, uap->oact, sizeof(s32));
	}
	return (error);
}
#endif

#if 0

int
freebsd32_xxx(struct thread *td, struct freebsd32_xxx_args *uap)
{
	int error;
	caddr_t sg;
	struct yyy32 *p32, s32;
	struct yyy *p = NULL, s;

	p32 = uap->zzz;
	if (p32) {
		sg = stackgap_init();
		p = stackgap_alloc(&sg, sizeof(struct yyy));
		uap->zzz = (struct yyy32 *)p;
		error = copyin(p32, &s32, sizeof(s32));
		if (error)
			return (error);
		/* translate in */
		error = copyout(&s, p, sizeof(s));
		if (error)
			return (error);
	}
	error = xxx(td, (struct xxx_args *) uap);
	if (error)
		return (error);
	if (p32) {
		error = copyin(p, &s, sizeof(s));
		if (error)
			return (error);
		/* translate out */
		error = copyout(&s32, p32, sizeof(s32));
	}
	return (error);
}

#endif
