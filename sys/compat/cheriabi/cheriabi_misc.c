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

#include <compat/cheriabi/cheriabi_util.h>
#include <compat/cheriabi/cheriabi.h>
#if 0
#include <compat/cheriabi/cheriabi_ipc.h>
#include <compat/cheriabi/cheriabi_misc.h>
#endif
#include <compat/cheriabi/cheriabi_signal.h>
#include <compat/cheriabi/cheriabi_proto.h>

FEATURE(compat_cheri_abi, "Compatible CHERI system call ABI");

#if 0
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
#endif

static int cheriabi_kevent_copyout(void *arg, struct kevent *kevp, int count);
static int cheriabi_kevent_copyin(void *arg, struct kevent *kevp, int count);

int
cheriabi_wait6(struct thread *td, struct cheriabi_wait6_args *uap)
{
#if 0
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
#endif
	return (ENOSYS);
}

/*
 * Custom version of exec_copyin_args() so that we can translate
 * the pointers.
 */
int
cheriabi_exec_copyin_args(struct image_args *args, char *fname,
    enum uio_seg segflg, u_int32_t *argv, u_int32_t *envv)
{
#if 0
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
#endif
	return (EINVAL);
}

int
cheriabi_execve(struct thread *td, struct cheriabi_execve_args *uap)
{
#if 0
	struct image_args eargs;
	struct vmspace *oldvmspace;
	int error;

	error = pre_execve(td, &oldvmspace);
	if (error != 0)
		return (error);
	error = freebsd32_exec_copyin_args(&eargs, uap->fname, UIO_USERSPACE,
	    uap->argv, uap->envv);
	if (error == 0)
		error = kern_execve(td, &eargs, NULL);
	post_execve(td, error, oldvmspace);
	return (error);
#endif
	return (ENOSYS);
}

int
cheriabi_fexecve(struct thread *td, struct cheriabi_fexecve_args *uap)
{
#if 0
	struct image_args eargs;
	struct vmspace *oldvmspace;
	int error;

	error = pre_execve(td, &oldvmspace);
	if (error != 0)
		return (error);
	error = freebsd32_exec_copyin_args(&eargs, NULL, UIO_SYSSPACE,
	    uap->argv, uap->envv);
	if (error == 0) {
		eargs.fd = uap->fd;
		error = kern_execve(td, &eargs, NULL);
	}
	post_execve(td, error, oldvmspace);
	return (error);
#endif
	return (ENOSYS);
}

/*
 * Copy 'count' items into the destination list pointed to by uap->eventlist.
 */
static int
cheriabi_kevent_copyout(void *arg, struct kevent *kevp, int count)
{
#if 0
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
#endif
	return (EINVAL);
}

/*
 * Copy 'count' items from the list pointed to by uap->changelist.
 */
static int
cheriabi_kevent_copyin(void *arg, struct kevent *kevp, int count)
{
#if 0
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
#endif
	return (EINVAL);
}

int
cheriabi_kevent(struct thread *td, struct cheriabi_kevent_args *uap)
{
	struct timespec ts, *tsp;
	struct kevent_copyops k_ops = { uap,
					cheriabi_kevent_copyout,
					cheriabi_kevent_copyin};
	int error;


	if (uap->timeout) {
		error = copyin(uap->timeout, &ts, sizeof(ts));
		if (error)
			return (error);
		tsp = &ts;
	} else
		tsp = NULL;
	error = kern_kevent(td, uap->fd, uap->nchanges, uap->nevents,
	    &k_ops, tsp);
	return (error);
}

static int
cheriabi_copyinuio(struct iovec_c *iovp, u_int iovcnt, struct uio **uiop)
{
#if 0
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
#endif
	return(EINVAL);
}

int
cheriabi_readv(struct thread *td, struct cheriabi_readv_args *uap)
{
	struct uio *auio;
	int error;

	error = cheriabi_copyinuio(uap->iovp, uap->iovcnt, &auio);
	if (error)
		return (error);
	error = kern_readv(td, uap->fd, auio);
	free(auio, M_IOV);
	return (error);
}

int
cheriabi_writev(struct thread *td, struct cheriabi_writev_args *uap)
{
	struct uio *auio;
	int error;

	error = cheriabi_copyinuio(uap->iovp, uap->iovcnt, &auio);
	if (error)
		return (error);
	error = kern_writev(td, uap->fd, auio);
	free(auio, M_IOV);
	return (error);
}

int
cheriabi_preadv(struct thread *td, struct cheriabi_preadv_args *uap)
{
	struct uio *auio;
	int error;

	error = cheriabi_copyinuio(uap->iovp, uap->iovcnt, &auio);
	if (error)
		return (error);
	error = kern_preadv(td, uap->fd, auio, uap->offset);
	free(auio, M_IOV);
	return (error);
}

int
cheriabi_pwritev(struct thread *td, struct cheriabi_pwritev_args *uap)
{
	struct uio *auio;
	int error;

	error = cheriabi_copyinuio(uap->iovp, uap->iovcnt, &auio);
	if (error)
		return (error);
	error = kern_pwritev(td, uap->fd, auio, uap->offset);
	free(auio, M_IOV);
	return (error);
}

int
cheriabi_copyiniov(struct iovec_c *iovp_c, u_int iovcnt, struct iovec **iovp,
    int error)
{
#if 0
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
#endif
	return (EINVAL);
}

#if 0
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
#endif

int
cheriabi_recvmsg(struct thread *td,
	struct cheriabi_recvmsg_args /* {
		int	s;
		struct	msghdr_c *msg;
		int	flags;
	} */ *uap)
{
#if 0
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
#endif
	return (EINVAL);
}

#if 0
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
	struct mbuf *m;
	void *md;
	u_int idx, len, msglen;
	int error;

	buflen = FREEBSD32_ALIGN(buflen);

	if (buflen > MCLBYTES)
		return (EINVAL);

	/*
	 * Iterate over the buffer and get the length of each message
	 * in there. This has 32-bit alignment and padding. Use it to
	 * determine the length of these messages when using 64-bit
	 * alignment and padding.
	 */
	idx = 0;
	len = 0;
	while (idx < buflen) {
		error = copyin(buf + idx, &msglen, sizeof(msglen));
		if (error)
			return (error);
		if (msglen < sizeof(struct cmsghdr))
			return (EINVAL);
		msglen = FREEBSD32_ALIGN(msglen);
		if (idx + msglen > buflen)
			return (EINVAL);
		idx += msglen;
		msglen += CMSG_ALIGN(sizeof(struct cmsghdr)) -
		    FREEBSD32_ALIGN(sizeof(struct cmsghdr));
		len += CMSG_ALIGN(msglen);
	}

	if (len > MCLBYTES)
		return (EINVAL);

	m = m_get(M_WAITOK, MT_CONTROL);
	if (len > MLEN)
		MCLGET(m, M_WAITOK);
	m->m_len = len;

	md = mtod(m, void *);
	while (buflen > 0) {
		error = copyin(buf, md, sizeof(struct cmsghdr));
		if (error)
			break;
		msglen = *(u_int *)md;
		msglen = FREEBSD32_ALIGN(msglen);

		/* Modify the message length to account for alignment. */
		*(u_int *)md = msglen + CMSG_ALIGN(sizeof(struct cmsghdr)) -
		    FREEBSD32_ALIGN(sizeof(struct cmsghdr));

		md = (char *)md + CMSG_ALIGN(sizeof(struct cmsghdr));
		buf += FREEBSD32_ALIGN(sizeof(struct cmsghdr));
		buflen -= FREEBSD32_ALIGN(sizeof(struct cmsghdr));

		msglen -= FREEBSD32_ALIGN(sizeof(struct cmsghdr));
		if (msglen > 0) {
			error = copyin(buf, md, msglen);
			if (error)
				break;
			md = (char *)md + CMSG_ALIGN(msglen);
			buf += msglen;
			buflen -= msglen;
		}
	}

	if (error)
		m_free(m);
	else
		*mp = m;
	return (error);
}
#endif

int
cheriabi_sendmsg(struct thread *td,
		  struct cheriabi_sendmsg_args *uap)
{
#if 0
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
#endif
	return (EINVAL);
}

#if 0
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
#endif

struct sf_hdtr_c {
	struct chericap headers;
	int hdr_cnt;
	struct chericap trailers;
	int trl_cnt;
};

static int
cheriabi_do_sendfile(struct thread *td,
    struct cheriabi_sendfile_args *uap, int compat)
{
#if 0
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
	    cap_rights_init(&rights, CAP_PREAD), &fp)) != 0)
		goto out;

	error = fo_sendfile(fp, uap->s, hdr_uio, trl_uio, offset,
	    uap->nbytes, &sbytes, uap->flags, compat ? SFK_COMPAT : 0, td);
	fdrop(fp, td);

	if (uap->sbytes != NULL)
		copyout(&sbytes, uap->sbytes, sizeof(off_t));

out:
	if (hdr_uio)
		free(hdr_uio, M_IOV);
	if (trl_uio)
		free(trl_uio, M_IOV);
	return (error);
#endif
	return (ENOSYS);
}

int
cheriabi_sendfile(struct thread *td, struct cheriabi_sendfile_args *uap)
{

	return (cheriabi_do_sendfile(td, uap, 0));
}

int
cheriabi_jail(struct thread *td, struct cheriabi_jail_args *uap)
{
#if 0
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
#endif
	return (ENOSYS);
}

int
cheriabi_jail_set(struct thread *td, struct cheriabi_jail_set_args *uap)
{
#if 0
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
#endif
	return (ENOSYS);
}

int
cheriabi_jail_get(struct thread *td, struct cheriabi_jail_get_args *uap)
{
#if 0
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
#endif
	return (ENOSYS);
}

struct sigvec_c {
	struct chericap	sv_handler;
	int		sv_mask;
	int		sv_flags;
};

struct sigstack32 {
	u_int32_t	ss_sp;
	int		ss_onstack;
};

int cheriabi_ktimer_create(struct thread *td,
    struct cheriabi_ktimer_create_args *uap)
{
#if 0
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
#endif
	return (ENOSYS);
}

int
cheriabi_thr_new(struct thread *td,
		  struct cheriabi_thr_new_args *uap)
{
#if 0
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
#endif
	return (ENOSYS);
}

void
siginfo_to_siginfo_c(const siginfo_t *src, struct siginfo_c *dst)
{
#if 0
	bzero(dst, sizeof(*dst));
	dst->si_signo = src->si_signo;
	dst->si_errno = src->si_errno;
	dst->si_code = src->si_code;
	dst->si_pid = src->si_pid;
	dst->si_uid = src->si_uid;
	dst->si_status = src->si_status;
	/* XXXBD: pcc relative? */
	/* XXX: use or void * is a POSIX bug */
	dst->si_addr = (uintptr_t)src->si_addr;
	dst->si_value.sival_int = src->si_value.sival_int;
	dst->si_timerid = src->si_timerid;
	dst->si_overrun = src->si_overrun;
#endif
}

int
cheriabi_sigtimedwait(struct thread *td, struct cheriabi_sigtimedwait_args *uap)
{
#if 0
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
#endif
	return (ENOSYS);
}

/*
 * MPSAFE
 */
int
cheriabi_sigwaitinfo(struct thread *td, struct cheriabi_sigwaitinfo_args *uap)
{
#if 0
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
#endif
	return (ENOSYS);
}

int
cheriabi_nmount(struct thread *td,
    struct cheriabi_nmount_args /* {
    	struct iovec_c *iovp;
    	unsigned int iovcnt;
    	int flags;
    } */ *uap)
{
#if 0
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
#endif
	return (ENOSYS);
}

#if 0
int
syscallcheri_register(int *offset, struct sysent *new_sysent,
    struct sysent *old_sysent, int flags)
{
}

int
syscallcheri_deregister(int *offset, struct sysent *old_sysent)
{
}

int
syscallcheri_module_handler(struct module *mod, int what, void *arg)
{
}

int
syscallcheri_helper_register(struct syscall_helper_data *sd, int flags)
{
}

int
syscallcheri_helper_unregister(struct syscall_helper_data *sd)
{
}
#endif

register_t *
cheriabi_copyout_strings(struct image_params *imgp)
{
#if 0
	int argc, envc, i;
	u_int32_t *vectp;
	char *stringp;
	uintptr_t destp;
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
	destp =	(uintptr_t)arginfo;

	/*
	 * install sigcode
	 */
	if (szsigcode != 0) {
		destp -= szsigcode;
		destp = rounddown2(destp, sizeof(uint32_t));
		copyout(imgp->proc->p_sysent->sv_sigcode, (void *)destp,
		    szsigcode);
	}

	/*
	 * Copy the image path for the rtld.
	 */
	if (execpath_len != 0) {
		destp -= execpath_len;
		imgp->execpathp = destp;
		copyout(imgp->execpath, (void *)destp, execpath_len);
	}

	/*
	 * Prepare the canary for SSP.
	 */
	arc4rand(canary, sizeof(canary), 0);
	destp -= sizeof(canary);
	imgp->canary = destp;
	copyout(canary, (void *)destp, sizeof(canary));
	imgp->canarylen = sizeof(canary);

	/*
	 * Prepare the pagesizes array.
	 */
	for (i = 0; i < MAXPAGESIZES; i++)
		pagesizes32[i] = (uint32_t)pagesizes[i];
	destp -= sizeof(pagesizes32);
	destp = rounddown2(destp, sizeof(uint32_t));
	imgp->pagesizes = destp;
	copyout(pagesizes32, (void *)destp, sizeof(pagesizes32));
	imgp->pagesizeslen = sizeof(pagesizes32);

	destp -= ARG_MAX - imgp->args->stringspace;
	destp = rounddown2(destp, sizeof(uint32_t));

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
	} else {
		/*
		 * The '+ 2' is for the null pointers at the end of each of
		 * the arg and env vector sets
		 */
		vectp = (u_int32_t *)(destp - (imgp->args->argc +
		    imgp->args->envc + 2) * sizeof(u_int32_t));
	}

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
	copyout(stringp, (void *)destp, ARG_MAX - imgp->args->stringspace);

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
#endif
	return(NULL);
}

int
convert_sigevent_c(struct sigevent_c *sig_c, struct sigevent *sig)
{
#if 0

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
#endif
	return (EINVAL);
}
