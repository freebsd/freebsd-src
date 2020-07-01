/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 1995 Søren Schmidt
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

/* XXX we use functions that might not exist. */
#include "opt_compat.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/capsicum.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syscallsubr.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/un.h>
#include <sys/unistd.h>

#include <security/audit/audit.h>

#include <net/if.h>
#include <net/vnet.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif

#ifdef COMPAT_LINUX32
#include <machine/../linux32/linux.h>
#include <machine/../linux32/linux32_proto.h>
#else
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#endif
#include <compat/linux/linux_common.h>
#include <compat/linux/linux_file.h>
#include <compat/linux/linux_mib.h>
#include <compat/linux/linux_socket.h>
#include <compat/linux/linux_timer.h>
#include <compat/linux/linux_util.h>

static int linux_sendmsg_common(struct thread *, l_int, struct l_msghdr *,
					l_uint);
static int linux_recvmsg_common(struct thread *, l_int, struct l_msghdr *,
					l_uint, struct msghdr *);
static int linux_set_socket_flags(int, int *);


static int
linux_to_bsd_sockopt_level(int level)
{

	if (level == LINUX_SOL_SOCKET)
		return (SOL_SOCKET);
	/* Remaining values are RFC-defined protocol numbers. */
	return (level);
}

static int
bsd_to_linux_sockopt_level(int level)
{

	if (level == SOL_SOCKET)
		return (LINUX_SOL_SOCKET);
	return (level);
}

static int
linux_to_bsd_ip_sockopt(int opt)
{

	switch (opt) {
	case LINUX_IP_TOS:
		return (IP_TOS);
	case LINUX_IP_TTL:
		return (IP_TTL);
	case LINUX_IP_OPTIONS:
		return (IP_OPTIONS);
	case LINUX_IP_MULTICAST_IF:
		return (IP_MULTICAST_IF);
	case LINUX_IP_MULTICAST_TTL:
		return (IP_MULTICAST_TTL);
	case LINUX_IP_MULTICAST_LOOP:
		return (IP_MULTICAST_LOOP);
	case LINUX_IP_ADD_MEMBERSHIP:
		return (IP_ADD_MEMBERSHIP);
	case LINUX_IP_DROP_MEMBERSHIP:
		return (IP_DROP_MEMBERSHIP);
	case LINUX_IP_HDRINCL:
		return (IP_HDRINCL);
	}
	return (-1);
}

static int
linux_to_bsd_ip6_sockopt(int opt)
{

	switch (opt) {
	case LINUX_IPV6_NEXTHOP:
		return (IPV6_NEXTHOP);
	case LINUX_IPV6_UNICAST_HOPS:
		return (IPV6_UNICAST_HOPS);
	case LINUX_IPV6_MULTICAST_IF:
		return (IPV6_MULTICAST_IF);
	case LINUX_IPV6_MULTICAST_HOPS:
		return (IPV6_MULTICAST_HOPS);
	case LINUX_IPV6_MULTICAST_LOOP:
		return (IPV6_MULTICAST_LOOP);
	case LINUX_IPV6_ADD_MEMBERSHIP:
		return (IPV6_JOIN_GROUP);
	case LINUX_IPV6_DROP_MEMBERSHIP:
		return (IPV6_LEAVE_GROUP);
	case LINUX_IPV6_V6ONLY:
		return (IPV6_V6ONLY);
	case LINUX_IPV6_DONTFRAG:
		return (IPV6_DONTFRAG);
#if 0
	case LINUX_IPV6_CHECKSUM:
		return (IPV6_CHECKSUM);
	case LINUX_IPV6_RECVPKTINFO:
		return (IPV6_RECVPKTINFO);
	case LINUX_IPV6_PKTINFO:
		return (IPV6_PKTINFO);
	case LINUX_IPV6_RECVHOPLIMIT:
		return (IPV6_RECVHOPLIMIT);
	case LINUX_IPV6_HOPLIMIT:
		return (IPV6_HOPLIMIT);
	case LINUX_IPV6_RECVHOPOPTS:
		return (IPV6_RECVHOPOPTS);
	case LINUX_IPV6_HOPOPTS:
		return (IPV6_HOPOPTS);
	case LINUX_IPV6_RTHDRDSTOPTS:
		return (IPV6_RTHDRDSTOPTS);
	case LINUX_IPV6_RECVRTHDR:
		return (IPV6_RECVRTHDR);
	case LINUX_IPV6_RTHDR:
		return (IPV6_RTHDR);
	case LINUX_IPV6_RECVDSTOPTS:
		return (IPV6_RECVDSTOPTS);
	case LINUX_IPV6_DSTOPTS:
		return (IPV6_DSTOPTS);
	case LINUX_IPV6_RECVPATHMTU:
		return (IPV6_RECVPATHMTU);
	case LINUX_IPV6_PATHMTU:
		return (IPV6_PATHMTU);
#endif
	}
	return (-1);
}

static int
linux_to_bsd_so_sockopt(int opt)
{

	switch (opt) {
	case LINUX_SO_DEBUG:
		return (SO_DEBUG);
	case LINUX_SO_REUSEADDR:
		return (SO_REUSEADDR);
	case LINUX_SO_TYPE:
		return (SO_TYPE);
	case LINUX_SO_ERROR:
		return (SO_ERROR);
	case LINUX_SO_DONTROUTE:
		return (SO_DONTROUTE);
	case LINUX_SO_BROADCAST:
		return (SO_BROADCAST);
	case LINUX_SO_SNDBUF:
	case LINUX_SO_SNDBUFFORCE:
		return (SO_SNDBUF);
	case LINUX_SO_RCVBUF:
	case LINUX_SO_RCVBUFFORCE:
		return (SO_RCVBUF);
	case LINUX_SO_KEEPALIVE:
		return (SO_KEEPALIVE);
	case LINUX_SO_OOBINLINE:
		return (SO_OOBINLINE);
	case LINUX_SO_LINGER:
		return (SO_LINGER);
	case LINUX_SO_REUSEPORT:
		return (SO_REUSEPORT_LB);
	case LINUX_SO_PEERCRED:
		return (LOCAL_PEERCRED);
	case LINUX_SO_RCVLOWAT:
		return (SO_RCVLOWAT);
	case LINUX_SO_SNDLOWAT:
		return (SO_SNDLOWAT);
	case LINUX_SO_RCVTIMEO:
		return (SO_RCVTIMEO);
	case LINUX_SO_SNDTIMEO:
		return (SO_SNDTIMEO);
	case LINUX_SO_TIMESTAMP:
		return (SO_TIMESTAMP);
	case LINUX_SO_ACCEPTCONN:
		return (SO_ACCEPTCONN);
	case LINUX_SO_PROTOCOL:
		return (SO_PROTOCOL);
	}
	return (-1);
}

static int
linux_to_bsd_tcp_sockopt(int opt)
{

	switch (opt) {
	case LINUX_TCP_NODELAY:
		return (TCP_NODELAY);
	case LINUX_TCP_MAXSEG:
		return (TCP_MAXSEG);
	case LINUX_TCP_CORK:
		return (TCP_NOPUSH);
	case LINUX_TCP_KEEPIDLE:
		return (TCP_KEEPIDLE);
	case LINUX_TCP_KEEPINTVL:
		return (TCP_KEEPINTVL);
	case LINUX_TCP_KEEPCNT:
		return (TCP_KEEPCNT);
	case LINUX_TCP_MD5SIG:
		return (TCP_MD5SIG);
	}
	return (-1);
}

static int
linux_to_bsd_msg_flags(int flags)
{
	int ret_flags = 0;

	if (flags & LINUX_MSG_OOB)
		ret_flags |= MSG_OOB;
	if (flags & LINUX_MSG_PEEK)
		ret_flags |= MSG_PEEK;
	if (flags & LINUX_MSG_DONTROUTE)
		ret_flags |= MSG_DONTROUTE;
	if (flags & LINUX_MSG_CTRUNC)
		ret_flags |= MSG_CTRUNC;
	if (flags & LINUX_MSG_TRUNC)
		ret_flags |= MSG_TRUNC;
	if (flags & LINUX_MSG_DONTWAIT)
		ret_flags |= MSG_DONTWAIT;
	if (flags & LINUX_MSG_EOR)
		ret_flags |= MSG_EOR;
	if (flags & LINUX_MSG_WAITALL)
		ret_flags |= MSG_WAITALL;
	if (flags & LINUX_MSG_NOSIGNAL)
		ret_flags |= MSG_NOSIGNAL;
#if 0 /* not handled */
	if (flags & LINUX_MSG_PROXY)
		;
	if (flags & LINUX_MSG_FIN)
		;
	if (flags & LINUX_MSG_SYN)
		;
	if (flags & LINUX_MSG_CONFIRM)
		;
	if (flags & LINUX_MSG_RST)
		;
	if (flags & LINUX_MSG_ERRQUEUE)
		;
#endif
	return (ret_flags);
}

static int
linux_to_bsd_cmsg_type(int cmsg_type)
{

	switch (cmsg_type) {
	case LINUX_SCM_RIGHTS:
		return (SCM_RIGHTS);
	case LINUX_SCM_CREDENTIALS:
		return (SCM_CREDS);
	}
	return (-1);
}

static int
bsd_to_linux_cmsg_type(int cmsg_type)
{

	switch (cmsg_type) {
	case SCM_RIGHTS:
		return (LINUX_SCM_RIGHTS);
	case SCM_CREDS:
		return (LINUX_SCM_CREDENTIALS);
	case SCM_TIMESTAMP:
		return (LINUX_SCM_TIMESTAMP);
	}
	return (-1);
}

static int
linux_to_bsd_msghdr(struct msghdr *bhdr, const struct l_msghdr *lhdr)
{
	if (lhdr->msg_controllen > INT_MAX)
		return (ENOBUFS);

	bhdr->msg_name		= PTRIN(lhdr->msg_name);
	bhdr->msg_namelen	= lhdr->msg_namelen;
	bhdr->msg_iov		= PTRIN(lhdr->msg_iov);
	bhdr->msg_iovlen	= lhdr->msg_iovlen;
	bhdr->msg_control	= PTRIN(lhdr->msg_control);

	/*
	 * msg_controllen is skipped since BSD and LINUX control messages
	 * are potentially different sizes (e.g. the cred structure used
	 * by SCM_CREDS is different between the two operating system).
	 *
	 * The caller can set it (if necessary) after converting all the
	 * control messages.
	 */

	bhdr->msg_flags		= linux_to_bsd_msg_flags(lhdr->msg_flags);
	return (0);
}

static int
bsd_to_linux_msghdr(const struct msghdr *bhdr, struct l_msghdr *lhdr)
{
	lhdr->msg_name		= PTROUT(bhdr->msg_name);
	lhdr->msg_namelen	= bhdr->msg_namelen;
	lhdr->msg_iov		= PTROUT(bhdr->msg_iov);
	lhdr->msg_iovlen	= bhdr->msg_iovlen;
	lhdr->msg_control	= PTROUT(bhdr->msg_control);

	/*
	 * msg_controllen is skipped since BSD and LINUX control messages
	 * are potentially different sizes (e.g. the cred structure used
	 * by SCM_CREDS is different between the two operating system).
	 *
	 * The caller can set it (if necessary) after converting all the
	 * control messages.
	 */

	/* msg_flags skipped */
	return (0);
}

static int
linux_set_socket_flags(int lflags, int *flags)
{

	if (lflags & ~(LINUX_SOCK_CLOEXEC | LINUX_SOCK_NONBLOCK))
		return (EINVAL);
	if (lflags & LINUX_SOCK_NONBLOCK)
		*flags |= SOCK_NONBLOCK;
	if (lflags & LINUX_SOCK_CLOEXEC)
		*flags |= SOCK_CLOEXEC;
	return (0);
}

static int
linux_sendit(struct thread *td, int s, struct msghdr *mp, int flags,
    struct mbuf *control, enum uio_seg segflg)
{
	struct sockaddr *to;
	int error, len;

	if (mp->msg_name != NULL) {
		len = mp->msg_namelen;
		error = linux_to_bsd_sockaddr(mp->msg_name, &to, &len);
		if (error != 0)
			return (error);
		mp->msg_name = to;
	} else
		to = NULL;

	error = kern_sendit(td, s, mp, linux_to_bsd_msg_flags(flags), control,
	    segflg);

	if (to)
		free(to, M_SONAME);
	return (error);
}

/* Return 0 if IP_HDRINCL is set for the given socket. */
static int
linux_check_hdrincl(struct thread *td, int s)
{
	int error, optval;
	socklen_t size_val;

	size_val = sizeof(optval);
	error = kern_getsockopt(td, s, IPPROTO_IP, IP_HDRINCL,
	    &optval, UIO_SYSSPACE, &size_val);
	if (error != 0)
		return (error);

	return (optval == 0);
}

/*
 * Updated sendto() when IP_HDRINCL is set:
 * tweak endian-dependent fields in the IP packet.
 */
static int
linux_sendto_hdrincl(struct thread *td, struct linux_sendto_args *linux_args)
{
/*
 * linux_ip_copysize defines how many bytes we should copy
 * from the beginning of the IP packet before we customize it for BSD.
 * It should include all the fields we modify (ip_len and ip_off).
 */
#define linux_ip_copysize	8

	struct ip *packet;
	struct msghdr msg;
	struct iovec aiov[1];
	int error;

	/* Check that the packet isn't too big or too small. */
	if (linux_args->len < linux_ip_copysize ||
	    linux_args->len > IP_MAXPACKET)
		return (EINVAL);

	packet = (struct ip *)malloc(linux_args->len, M_LINUX, M_WAITOK);

	/* Make kernel copy of the packet to be sent */
	if ((error = copyin(PTRIN(linux_args->msg), packet,
	    linux_args->len)))
		goto goout;

	/* Convert fields from Linux to BSD raw IP socket format */
	packet->ip_len = linux_args->len;
	packet->ip_off = ntohs(packet->ip_off);

	/* Prepare the msghdr and iovec structures describing the new packet */
	msg.msg_name = PTRIN(linux_args->to);
	msg.msg_namelen = linux_args->tolen;
	msg.msg_iov = aiov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	msg.msg_flags = 0;
	aiov[0].iov_base = (char *)packet;
	aiov[0].iov_len = linux_args->len;
	error = linux_sendit(td, linux_args->s, &msg, linux_args->flags,
	    NULL, UIO_SYSSPACE);
goout:
	free(packet, M_LINUX);
	return (error);
}

int
linux_socket(struct thread *td, struct linux_socket_args *args)
{
	int domain, retval_socket, type;

	type = args->type & LINUX_SOCK_TYPE_MASK;
	if (type < 0 || type > LINUX_SOCK_MAX)
		return (EINVAL);
	retval_socket = linux_set_socket_flags(args->type & ~LINUX_SOCK_TYPE_MASK,
		&type);
	if (retval_socket != 0)
		return (retval_socket);
	domain = linux_to_bsd_domain(args->domain);
	if (domain == -1)
		return (EAFNOSUPPORT);

	retval_socket = kern_socket(td, domain, type, args->protocol);
	if (retval_socket)
		return (retval_socket);

	if (type == SOCK_RAW
	    && (args->protocol == IPPROTO_RAW || args->protocol == 0)
	    && domain == PF_INET) {
		/* It's a raw IP socket: set the IP_HDRINCL option. */
		int hdrincl;

		hdrincl = 1;
		/* We ignore any error returned by kern_setsockopt() */
		kern_setsockopt(td, td->td_retval[0], IPPROTO_IP, IP_HDRINCL,
		    &hdrincl, UIO_SYSSPACE, sizeof(hdrincl));
	}
#ifdef INET6
	/*
	 * Linux AF_INET6 socket has IPV6_V6ONLY setsockopt set to 0 by default
	 * and some apps depend on this. So, set V6ONLY to 0 for Linux apps.
	 * For simplicity we do this unconditionally of the net.inet6.ip6.v6only
	 * sysctl value.
	 */
	if (domain == PF_INET6) {
		int v6only;

		v6only = 0;
		/* We ignore any error returned by setsockopt() */
		kern_setsockopt(td, td->td_retval[0], IPPROTO_IPV6, IPV6_V6ONLY,
		    &v6only, UIO_SYSSPACE, sizeof(v6only));
	}
#endif

	return (retval_socket);
}

int
linux_bind(struct thread *td, struct linux_bind_args *args)
{
	struct sockaddr *sa;
	int error;

	error = linux_to_bsd_sockaddr(PTRIN(args->name), &sa,
	    &args->namelen);
	if (error != 0)
		return (error);

	error = kern_bindat(td, AT_FDCWD, args->s, sa);
	free(sa, M_SONAME);

	/* XXX */
	if (error == EADDRNOTAVAIL && args->namelen != sizeof(struct sockaddr_in))
		return (EINVAL);
	return (error);
}

int
linux_connect(struct thread *td, struct linux_connect_args *args)
{
	struct socket *so;
	struct sockaddr *sa;
	struct file *fp;
	u_int fflag;
	int error;

	error = linux_to_bsd_sockaddr(PTRIN(args->name), &sa,
	    &args->namelen);
	if (error != 0)
		return (error);

	error = kern_connectat(td, AT_FDCWD, args->s, sa);
	free(sa, M_SONAME);
	if (error != EISCONN)
		return (error);

	/*
	 * Linux doesn't return EISCONN the first time it occurs,
	 * when on a non-blocking socket. Instead it returns the
	 * error getsockopt(SOL_SOCKET, SO_ERROR) would return on BSD.
	 */
	error = getsock_cap(td, args->s, &cap_connect_rights,
	    &fp, &fflag, NULL);
	if (error != 0)
		return (error);

	error = EISCONN;
	so = fp->f_data;
	if (fflag & FNONBLOCK) {
		SOCK_LOCK(so);
		if (so->so_emuldata == 0)
			error = so->so_error;
		so->so_emuldata = (void *)1;
		SOCK_UNLOCK(so);
	}
	fdrop(fp, td);

	return (error);
}

int
linux_listen(struct thread *td, struct linux_listen_args *args)
{

	return (kern_listen(td, args->s, args->backlog));
}

static int
linux_accept_common(struct thread *td, int s, l_uintptr_t addr,
    l_uintptr_t namelen, int flags)
{
	struct l_sockaddr *lsa;
	struct sockaddr *sa;
	struct file *fp, *fp1;
	int bflags, len;
	struct socket *so;
	int error, error1;

	bflags = 0;
	fp = NULL;
	sa = NULL;

	error = linux_set_socket_flags(flags, &bflags);
	if (error != 0)
		return (error);

	if (PTRIN(addr) == NULL) {
		len = 0;
		error = kern_accept4(td, s, NULL, NULL, bflags, NULL);
	} else {
		error = copyin(PTRIN(namelen), &len, sizeof(len));
		if (error != 0)
			return (error);
		if (len < 0)
			return (EINVAL);
		error = kern_accept4(td, s, &sa, &len, bflags, &fp);
	}

	/*
	 * Translate errno values into ones used by Linux.
	 */
	if (error != 0) {
		/*
		 * XXX. This is wrong, different sockaddr structures
		 * have different sizes.
		 */
		switch (error) {
		case EFAULT:
			if (namelen != sizeof(struct sockaddr_in))
				error = EINVAL;
			break;
		case EINVAL:
			error1 = getsock_cap(td, s, &cap_accept_rights, &fp1, NULL, NULL);
			if (error1 != 0) {
				error = error1;
				break;
			}
			so = fp1->f_data;
			if (so->so_type == SOCK_DGRAM)
				error = EOPNOTSUPP;
			fdrop(fp1, td);
			break;
		}
		return (error);
	}

	if (len != 0) {
		error = bsd_to_linux_sockaddr(sa, &lsa, len);
		if (error == 0)
			error = copyout(lsa, PTRIN(addr), len);
		free(lsa, M_SONAME);

		/*
		 * XXX: We should also copyout the len, shouldn't we?
		 */

		if (error != 0) {
			fdclose(td, fp, td->td_retval[0]);
			td->td_retval[0] = 0;
		}
	}
	if (fp != NULL)
		fdrop(fp, td);
	free(sa, M_SONAME);
	return (error);
}

int
linux_accept(struct thread *td, struct linux_accept_args *args)
{

	return (linux_accept_common(td, args->s, args->addr,
	    args->namelen, 0));
}

int
linux_accept4(struct thread *td, struct linux_accept4_args *args)
{

	return (linux_accept_common(td, args->s, args->addr,
	    args->namelen, args->flags));
}

int
linux_getsockname(struct thread *td, struct linux_getsockname_args *args)
{
	struct l_sockaddr *lsa;
	struct sockaddr *sa;
	int len, error;

	error = copyin(PTRIN(args->namelen), &len, sizeof(len));
	if (error != 0)
		return (error);

	error = kern_getsockname(td, args->s, &sa, &len);
	if (error != 0)
		return (error);

	if (len != 0) {
		error = bsd_to_linux_sockaddr(sa, &lsa, len);
		if (error == 0)
			error = copyout(lsa, PTRIN(args->addr),
			    len);
		free(lsa, M_SONAME);
	}

	free(sa, M_SONAME);
	if (error == 0)
		error = copyout(&len, PTRIN(args->namelen), sizeof(len));
	return (error);
}

int
linux_getpeername(struct thread *td, struct linux_getpeername_args *args)
{
	struct l_sockaddr *lsa;
	struct sockaddr *sa;
	int len, error;

	error = copyin(PTRIN(args->namelen), &len, sizeof(len));
	if (error != 0)
		return (error);
	if (len < 0)
		return (EINVAL);

	error = kern_getpeername(td, args->s, &sa, &len);
	if (error != 0)
		return (error);

	if (len != 0) {
		error = bsd_to_linux_sockaddr(sa, &lsa, len);
		if (error == 0)
			error = copyout(lsa, PTRIN(args->addr),
			    len);
		free(lsa, M_SONAME);
	}

	free(sa, M_SONAME);
	if (error == 0)
		error = copyout(&len, PTRIN(args->namelen), sizeof(len));
	return (error);
}

int
linux_socketpair(struct thread *td, struct linux_socketpair_args *args)
{
	int domain, error, sv[2], type;

	domain = linux_to_bsd_domain(args->domain);
	if (domain != PF_LOCAL)
		return (EAFNOSUPPORT);
	type = args->type & LINUX_SOCK_TYPE_MASK;
	if (type < 0 || type > LINUX_SOCK_MAX)
		return (EINVAL);
	error = linux_set_socket_flags(args->type & ~LINUX_SOCK_TYPE_MASK,
	    &type);
	if (error != 0)
		return (error);
	if (args->protocol != 0 && args->protocol != PF_UNIX) {

		/*
		 * Use of PF_UNIX as protocol argument is not right,
		 * but Linux does it.
		 * Do not map PF_UNIX as its Linux value is identical
		 * to FreeBSD one.
		 */
		return (EPROTONOSUPPORT);
	}
	error = kern_socketpair(td, domain, type, 0, sv);
	if (error != 0)
                return (error);
        error = copyout(sv, PTRIN(args->rsv), 2 * sizeof(int));
        if (error != 0) {
                (void)kern_close(td, sv[0]);
                (void)kern_close(td, sv[1]);
        }
	return (error);
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
struct linux_send_args {
	register_t s;
	register_t msg;
	register_t len;
	register_t flags;
};

static int
linux_send(struct thread *td, struct linux_send_args *args)
{
	struct sendto_args /* {
		int s;
		caddr_t buf;
		int len;
		int flags;
		caddr_t to;
		int tolen;
	} */ bsd_args;
	struct file *fp;
	int error, fflag;

	bsd_args.s = args->s;
	bsd_args.buf = (caddr_t)PTRIN(args->msg);
	bsd_args.len = args->len;
	bsd_args.flags = args->flags;
	bsd_args.to = NULL;
	bsd_args.tolen = 0;
	error = sys_sendto(td, &bsd_args);
	if (error == ENOTCONN) {
		/*
		 * Linux doesn't return ENOTCONN for non-blocking sockets.
		 * Instead it returns the EAGAIN.
		 */
		error = getsock_cap(td, args->s, &cap_send_rights, &fp,
		    &fflag, NULL);
		if (error == 0) {
			if (fflag & FNONBLOCK)
				error = EAGAIN;
			fdrop(fp, td);
		}
	}
	return (error);
}

struct linux_recv_args {
	register_t s;
	register_t msg;
	register_t len;
	register_t flags;
};

static int
linux_recv(struct thread *td, struct linux_recv_args *args)
{
	struct recvfrom_args /* {
		int s;
		caddr_t buf;
		int len;
		int flags;
		struct sockaddr *from;
		socklen_t fromlenaddr;
	} */ bsd_args;

	bsd_args.s = args->s;
	bsd_args.buf = (caddr_t)PTRIN(args->msg);
	bsd_args.len = args->len;
	bsd_args.flags = linux_to_bsd_msg_flags(args->flags);
	bsd_args.from = NULL;
	bsd_args.fromlenaddr = 0;
	return (sys_recvfrom(td, &bsd_args));
}
#endif /* __i386__ || (__amd64__ && COMPAT_LINUX32) */

int
linux_sendto(struct thread *td, struct linux_sendto_args *args)
{
	struct msghdr msg;
	struct iovec aiov;

	if (linux_check_hdrincl(td, args->s) == 0)
		/* IP_HDRINCL set, tweak the packet before sending */
		return (linux_sendto_hdrincl(td, args));

	msg.msg_name = PTRIN(args->to);
	msg.msg_namelen = args->tolen;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	msg.msg_flags = 0;
	aiov.iov_base = PTRIN(args->msg);
	aiov.iov_len = args->len;
	return (linux_sendit(td, args->s, &msg, args->flags, NULL,
	    UIO_USERSPACE));
}

int
linux_recvfrom(struct thread *td, struct linux_recvfrom_args *args)
{
	struct l_sockaddr *lsa;
	struct sockaddr *sa;
	struct msghdr msg;
	struct iovec aiov;
	int error, fromlen;

	if (PTRIN(args->fromlen) != NULL) {
		error = copyin(PTRIN(args->fromlen), &fromlen,
		    sizeof(fromlen));
		if (error != 0)
			return (error);
		if (fromlen < 0)
			return (EINVAL);
		sa = malloc(fromlen, M_SONAME, M_WAITOK);
	} else {
		fromlen = 0;
		sa = NULL;
	}

	msg.msg_name = sa;
	msg.msg_namelen = fromlen;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = PTRIN(args->buf);
	aiov.iov_len = args->len;
	msg.msg_control = 0;
	msg.msg_flags = linux_to_bsd_msg_flags(args->flags);

	error = kern_recvit(td, args->s, &msg, UIO_SYSSPACE, NULL);
	if (error != 0)
		goto out;

	if (PTRIN(args->from) != NULL) {
		error = bsd_to_linux_sockaddr(sa, &lsa, msg.msg_namelen);
		if (error == 0)
			error = copyout(lsa, PTRIN(args->from),
			    msg.msg_namelen);
		free(lsa, M_SONAME);
	}

	if (error == 0 && PTRIN(args->fromlen) != NULL)
		error = copyout(&msg.msg_namelen, PTRIN(args->fromlen),
		    sizeof(msg.msg_namelen));
out:
	free(sa, M_SONAME);
	return (error);
}

static int
linux_sendmsg_common(struct thread *td, l_int s, struct l_msghdr *msghdr,
    l_uint flags)
{
	struct cmsghdr *cmsg;
	struct mbuf *control;
	struct msghdr msg;
	struct l_cmsghdr linux_cmsg;
	struct l_cmsghdr *ptr_cmsg;
	struct l_msghdr linux_msghdr;
	struct iovec *iov;
	socklen_t datalen;
	struct sockaddr *sa;
	struct socket *so;
	sa_family_t sa_family;
	struct file *fp;
	void *data;
	l_size_t len;
	l_size_t clen;
	int error, fflag;

	error = copyin(msghdr, &linux_msghdr, sizeof(linux_msghdr));
	if (error != 0)
		return (error);

	/*
	 * Some Linux applications (ping) define a non-NULL control data
	 * pointer, but a msg_controllen of 0, which is not allowed in the
	 * FreeBSD system call interface.  NULL the msg_control pointer in
	 * order to handle this case.  This should be checked, but allows the
	 * Linux ping to work.
	 */
	if (PTRIN(linux_msghdr.msg_control) != NULL &&
	    linux_msghdr.msg_controllen == 0)
		linux_msghdr.msg_control = PTROUT(NULL);

	error = linux_to_bsd_msghdr(&msg, &linux_msghdr);
	if (error != 0)
		return (error);

#ifdef COMPAT_LINUX32
	error = linux32_copyiniov(PTRIN(msg.msg_iov), msg.msg_iovlen,
	    &iov, EMSGSIZE);
#else
	error = copyiniov(msg.msg_iov, msg.msg_iovlen, &iov, EMSGSIZE);
#endif
	if (error != 0)
		return (error);

	control = NULL;

	error = kern_getsockname(td, s, &sa, &datalen);
	if (error != 0)
		goto bad;
	sa_family = sa->sa_family;
	free(sa, M_SONAME);

	if (flags & LINUX_MSG_OOB) {
		error = EOPNOTSUPP;
		if (sa_family == AF_UNIX)
			goto bad;

		error = getsock_cap(td, s, &cap_send_rights, &fp,
		    &fflag, NULL);
		if (error != 0)
			goto bad;
		so = fp->f_data;
		if (so->so_type != SOCK_STREAM)
			error = EOPNOTSUPP;
		fdrop(fp, td);
		if (error != 0)
			goto bad;
	}

	if (linux_msghdr.msg_controllen >= sizeof(struct l_cmsghdr)) {

		error = ENOBUFS;
		control = m_get(M_WAITOK, MT_CONTROL);
		MCLGET(control, M_WAITOK);
		data = mtod(control, void *);
		datalen = 0;

		ptr_cmsg = PTRIN(linux_msghdr.msg_control);
		clen = linux_msghdr.msg_controllen;
		do {
			error = copyin(ptr_cmsg, &linux_cmsg,
			    sizeof(struct l_cmsghdr));
			if (error != 0)
				goto bad;

			error = EINVAL;
			if (linux_cmsg.cmsg_len < sizeof(struct l_cmsghdr) ||
			    linux_cmsg.cmsg_len > clen)
				goto bad;

			if (datalen + CMSG_HDRSZ > MCLBYTES)
				goto bad;

			/*
			 * Now we support only SCM_RIGHTS and SCM_CRED,
			 * so return EINVAL in any other cmsg_type
			 */
			cmsg = data;
			cmsg->cmsg_type =
			    linux_to_bsd_cmsg_type(linux_cmsg.cmsg_type);
			cmsg->cmsg_level =
			    linux_to_bsd_sockopt_level(linux_cmsg.cmsg_level);
			if (cmsg->cmsg_type == -1
			    || cmsg->cmsg_level != SOL_SOCKET) {
				linux_msg(curthread,
				    "unsupported sendmsg cmsg level %d type %d",
				    linux_cmsg.cmsg_level, linux_cmsg.cmsg_type);
				goto bad;
			}

			/*
			 * Some applications (e.g. pulseaudio) attempt to
			 * send ancillary data even if the underlying protocol
			 * doesn't support it which is not allowed in the
			 * FreeBSD system call interface.
			 */
			if (sa_family != AF_UNIX)
				continue;

			if (cmsg->cmsg_type == SCM_CREDS) {
				len = sizeof(struct cmsgcred);
				if (datalen + CMSG_SPACE(len) > MCLBYTES)
					goto bad;

				/*
				 * The lower levels will fill in the structure
				 */
				memset(CMSG_DATA(data), 0, len);
			} else {
				len = linux_cmsg.cmsg_len - L_CMSG_HDRSZ;
				if (datalen + CMSG_SPACE(len) < datalen ||
				    datalen + CMSG_SPACE(len) > MCLBYTES)
					goto bad;

				error = copyin(LINUX_CMSG_DATA(ptr_cmsg),
				    CMSG_DATA(data), len);
				if (error != 0)
					goto bad;
			}

			cmsg->cmsg_len = CMSG_LEN(len);
			data = (char *)data + CMSG_SPACE(len);
			datalen += CMSG_SPACE(len);

			if (clen <= LINUX_CMSG_ALIGN(linux_cmsg.cmsg_len))
				break;

			clen -= LINUX_CMSG_ALIGN(linux_cmsg.cmsg_len);
			ptr_cmsg = (struct l_cmsghdr *)((char *)ptr_cmsg +
			    LINUX_CMSG_ALIGN(linux_cmsg.cmsg_len));
		} while(clen >= sizeof(struct l_cmsghdr));

		control->m_len = datalen;
		if (datalen == 0) {
			m_freem(control);
			control = NULL;
		}
	}

	msg.msg_iov = iov;
	msg.msg_flags = 0;
	error = linux_sendit(td, s, &msg, flags, control, UIO_USERSPACE);
	control = NULL;

bad:
	m_freem(control);
	free(iov, M_IOV);
	return (error);
}

int
linux_sendmsg(struct thread *td, struct linux_sendmsg_args *args)
{

	return (linux_sendmsg_common(td, args->s, PTRIN(args->msg),
	    args->flags));
}

int
linux_sendmmsg(struct thread *td, struct linux_sendmmsg_args *args)
{
	struct l_mmsghdr *msg;
	l_uint retval;
	int error, datagrams;

	if (args->vlen > UIO_MAXIOV)
		args->vlen = UIO_MAXIOV;

	msg = PTRIN(args->msg);
	datagrams = 0;
	while (datagrams < args->vlen) {
		error = linux_sendmsg_common(td, args->s, &msg->msg_hdr,
		    args->flags);
		if (error != 0)
			break;

		retval = td->td_retval[0];
		error = copyout(&retval, &msg->msg_len, sizeof(msg->msg_len));
		if (error != 0)
			break;
		++msg;
		++datagrams;
	}
	if (error == 0)
		td->td_retval[0] = datagrams;
	return (error);
}

static int
linux_recvmsg_common(struct thread *td, l_int s, struct l_msghdr *msghdr,
    l_uint flags, struct msghdr *msg)
{
	struct cmsghdr *cm;
	struct cmsgcred *cmcred;
	struct l_cmsghdr *linux_cmsg = NULL;
	struct l_ucred linux_ucred;
	socklen_t datalen, maxlen, outlen;
	struct l_msghdr linux_msghdr;
	struct iovec *iov, *uiov;
	struct mbuf *control = NULL;
	struct mbuf **controlp;
	struct timeval *ftmvl;
	struct l_sockaddr *lsa;
	struct sockaddr *sa;
	l_timeval ltmvl;
	caddr_t outbuf;
	void *data;
	int error, i, fd, fds, *fdp;

	error = copyin(msghdr, &linux_msghdr, sizeof(linux_msghdr));
	if (error != 0)
		return (error);

	error = linux_to_bsd_msghdr(msg, &linux_msghdr);
	if (error != 0)
		return (error);

#ifdef COMPAT_LINUX32
	error = linux32_copyiniov(PTRIN(msg->msg_iov), msg->msg_iovlen,
	    &iov, EMSGSIZE);
#else
	error = copyiniov(msg->msg_iov, msg->msg_iovlen, &iov, EMSGSIZE);
#endif
	if (error != 0)
		return (error);

	if (msg->msg_name) {
		sa = malloc(msg->msg_namelen, M_SONAME, M_WAITOK);
		msg->msg_name = sa;
	} else
		sa = NULL;

	uiov = msg->msg_iov;
	msg->msg_iov = iov;
	controlp = (msg->msg_control != NULL) ? &control : NULL;
	error = kern_recvit(td, s, msg, UIO_SYSSPACE, controlp);
	msg->msg_iov = uiov;
	if (error != 0)
		goto bad;

	if (msg->msg_name) {
		msg->msg_name = PTRIN(linux_msghdr.msg_name);
		error = bsd_to_linux_sockaddr(sa, &lsa, msg->msg_namelen);
		if (error == 0)
			error = copyout(lsa, PTRIN(msg->msg_name),
			    msg->msg_namelen);
		free(lsa, M_SONAME);
		if (error != 0)
			goto bad;
	}

	error = bsd_to_linux_msghdr(msg, &linux_msghdr);
	if (error != 0)
		goto bad;

	maxlen = linux_msghdr.msg_controllen;
	linux_msghdr.msg_controllen = 0;
	if (control) {
		linux_cmsg = malloc(L_CMSG_HDRSZ, M_LINUX, M_WAITOK | M_ZERO);

		msg->msg_control = mtod(control, struct cmsghdr *);
		msg->msg_controllen = control->m_len;

		cm = CMSG_FIRSTHDR(msg);
		outbuf = PTRIN(linux_msghdr.msg_control);
		outlen = 0;
		while (cm != NULL) {
			linux_cmsg->cmsg_type =
			    bsd_to_linux_cmsg_type(cm->cmsg_type);
			linux_cmsg->cmsg_level =
			    bsd_to_linux_sockopt_level(cm->cmsg_level);
			if (linux_cmsg->cmsg_type == -1 ||
			    cm->cmsg_level != SOL_SOCKET) {
				linux_msg(curthread,
				    "unsupported recvmsg cmsg level %d type %d",
				    cm->cmsg_level, cm->cmsg_type);
				error = EINVAL;
				goto bad;
			}

			data = CMSG_DATA(cm);
			datalen = (caddr_t)cm + cm->cmsg_len - (caddr_t)data;

			switch (cm->cmsg_type) {
			case SCM_RIGHTS:
				if (flags & LINUX_MSG_CMSG_CLOEXEC) {
					fds = datalen / sizeof(int);
					fdp = data;
					for (i = 0; i < fds; i++) {
						fd = *fdp++;
						(void)kern_fcntl(td, fd,
						    F_SETFD, FD_CLOEXEC);
					}
				}
				break;

			case SCM_CREDS:
				/*
				 * Currently LOCAL_CREDS is never in
				 * effect for Linux so no need to worry
				 * about sockcred
				 */
				if (datalen != sizeof(*cmcred)) {
					error = EMSGSIZE;
					goto bad;
				}
				cmcred = (struct cmsgcred *)data;
				bzero(&linux_ucred, sizeof(linux_ucred));
				linux_ucred.pid = cmcred->cmcred_pid;
				linux_ucred.uid = cmcred->cmcred_uid;
				linux_ucred.gid = cmcred->cmcred_gid;
				data = &linux_ucred;
				datalen = sizeof(linux_ucred);
				break;

			case SCM_TIMESTAMP:
				if (datalen != sizeof(struct timeval)) {
					error = EMSGSIZE;
					goto bad;
				}
				ftmvl = (struct timeval *)data;
				ltmvl.tv_sec = ftmvl->tv_sec;
				ltmvl.tv_usec = ftmvl->tv_usec;
				data = &ltmvl;
				datalen = sizeof(ltmvl);
				break;
			}

			if (outlen + LINUX_CMSG_LEN(datalen) > maxlen) {
				if (outlen == 0) {
					error = EMSGSIZE;
					goto bad;
				} else {
					linux_msghdr.msg_flags |= LINUX_MSG_CTRUNC;
					m_dispose_extcontrolm(control);
					goto out;
				}
			}

			linux_cmsg->cmsg_len = LINUX_CMSG_LEN(datalen);

			error = copyout(linux_cmsg, outbuf, L_CMSG_HDRSZ);
			if (error != 0)
				goto bad;
			outbuf += L_CMSG_HDRSZ;

			error = copyout(data, outbuf, datalen);
			if (error != 0)
				goto bad;

			outbuf += LINUX_CMSG_ALIGN(datalen);
			outlen += LINUX_CMSG_LEN(datalen);

			cm = CMSG_NXTHDR(msg, cm);
		}
		linux_msghdr.msg_controllen = outlen;
	}

out:
	error = copyout(&linux_msghdr, msghdr, sizeof(linux_msghdr));

bad:
	if (control != NULL) {
		if (error != 0)
			m_dispose_extcontrolm(control);
		m_freem(control);
	}
	free(iov, M_IOV);
	free(linux_cmsg, M_LINUX);
	free(sa, M_SONAME);

	return (error);
}

int
linux_recvmsg(struct thread *td, struct linux_recvmsg_args *args)
{
	struct msghdr bsd_msg;

	return (linux_recvmsg_common(td, args->s, PTRIN(args->msg),
	    args->flags, &bsd_msg));
}

int
linux_recvmmsg(struct thread *td, struct linux_recvmmsg_args *args)
{
	struct l_mmsghdr *msg;
	struct msghdr bsd_msg;
	struct l_timespec lts;
	struct timespec ts, tts;
	l_uint retval;
	int error, datagrams;

	if (args->timeout) {
		error = copyin(args->timeout, &lts, sizeof(struct l_timespec));
		if (error != 0)
			return (error);
		error = linux_to_native_timespec(&ts, &lts);
		if (error != 0)
			return (error);
		getnanotime(&tts);
		timespecadd(&tts, &ts, &tts);
	}

	msg = PTRIN(args->msg);
	datagrams = 0;
	while (datagrams < args->vlen) {
		error = linux_recvmsg_common(td, args->s, &msg->msg_hdr,
		    args->flags & ~LINUX_MSG_WAITFORONE, &bsd_msg);
		if (error != 0)
			break;

		retval = td->td_retval[0];
		error = copyout(&retval, &msg->msg_len, sizeof(msg->msg_len));
		if (error != 0)
			break;
		++msg;
		++datagrams;

		/*
		 * MSG_WAITFORONE turns on MSG_DONTWAIT after one packet.
		 */
		if (args->flags & LINUX_MSG_WAITFORONE)
			args->flags |= LINUX_MSG_DONTWAIT;

		/*
		 * See BUGS section of recvmmsg(2).
		 */
		if (args->timeout) {
			getnanotime(&ts);
			timespecsub(&ts, &tts, &ts);
			if (!timespecisset(&ts) || ts.tv_sec > 0)
				break;
		}
		/* Out of band data, return right away. */
		if (bsd_msg.msg_flags & MSG_OOB)
			break;
	}
	if (error == 0)
		td->td_retval[0] = datagrams;
	return (error);
}

int
linux_shutdown(struct thread *td, struct linux_shutdown_args *args)
{

	return (kern_shutdown(td, args->s, args->how));
}

int
linux_setsockopt(struct thread *td, struct linux_setsockopt_args *args)
{
	l_timeval linux_tv;
	struct sockaddr *sa;
	struct timeval tv;
	socklen_t len;
	int error, level, name;

	level = linux_to_bsd_sockopt_level(args->level);
	switch (level) {
	case SOL_SOCKET:
		name = linux_to_bsd_so_sockopt(args->optname);
		switch (name) {
		case SO_RCVTIMEO:
			/* FALLTHROUGH */
		case SO_SNDTIMEO:
			error = copyin(PTRIN(args->optval), &linux_tv,
			    sizeof(linux_tv));
			if (error != 0)
				return (error);
			tv.tv_sec = linux_tv.tv_sec;
			tv.tv_usec = linux_tv.tv_usec;
			return (kern_setsockopt(td, args->s, level,
			    name, &tv, UIO_SYSSPACE, sizeof(tv)));
			/* NOTREACHED */
		default:
			break;
		}
		break;
	case IPPROTO_IP:
		if (args->optname == LINUX_IP_RECVERR &&
		    linux_ignore_ip_recverr) {
			/*
			 * XXX: This is a hack to unbreak DNS resolution
			 *	with glibc 2.30 and above.
			 */
			return (0);
		}
		name = linux_to_bsd_ip_sockopt(args->optname);
		break;
	case IPPROTO_IPV6:
		name = linux_to_bsd_ip6_sockopt(args->optname);
		break;
	case IPPROTO_TCP:
		name = linux_to_bsd_tcp_sockopt(args->optname);
		break;
	default:
		name = -1;
		break;
	}
	if (name == -1) {
		linux_msg(curthread,
		    "unsupported setsockopt level %d optname %d",
		    args->level, args->optname);
		return (ENOPROTOOPT);
	}

	if (name == IPV6_NEXTHOP) {
		len = args->optlen;
		error = linux_to_bsd_sockaddr(PTRIN(args->optval), &sa, &len);
		if (error != 0)
			return (error);

		error = kern_setsockopt(td, args->s, level,
		    name, sa, UIO_SYSSPACE, len);
		free(sa, M_SONAME);
	} else {
		error = kern_setsockopt(td, args->s, level,
		    name, PTRIN(args->optval), UIO_USERSPACE, args->optlen);
	}

	return (error);
}

int
linux_getsockopt(struct thread *td, struct linux_getsockopt_args *args)
{
	l_timeval linux_tv;
	struct timeval tv;
	socklen_t tv_len, xulen, len;
	struct l_sockaddr *lsa;
	struct sockaddr *sa;
	struct xucred xu;
	struct l_ucred lxu;
	int error, level, name, newval;

	level = linux_to_bsd_sockopt_level(args->level);
	switch (level) {
	case SOL_SOCKET:
		name = linux_to_bsd_so_sockopt(args->optname);
		switch (name) {
		case SO_RCVTIMEO:
			/* FALLTHROUGH */
		case SO_SNDTIMEO:
			tv_len = sizeof(tv);
			error = kern_getsockopt(td, args->s, level,
			    name, &tv, UIO_SYSSPACE, &tv_len);
			if (error != 0)
				return (error);
			linux_tv.tv_sec = tv.tv_sec;
			linux_tv.tv_usec = tv.tv_usec;
			return (copyout(&linux_tv, PTRIN(args->optval),
			    sizeof(linux_tv)));
			/* NOTREACHED */
		case LOCAL_PEERCRED:
			if (args->optlen < sizeof(lxu))
				return (EINVAL);
			/*
			 * LOCAL_PEERCRED is not served at the SOL_SOCKET level,
			 * but by the Unix socket's level 0.
			 */
			level = 0;
			xulen = sizeof(xu);
			error = kern_getsockopt(td, args->s, level,
			    name, &xu, UIO_SYSSPACE, &xulen);
			if (error != 0)
				return (error);
			lxu.pid = xu.cr_pid;
			lxu.uid = xu.cr_uid;
			lxu.gid = xu.cr_gid;
			return (copyout(&lxu, PTRIN(args->optval), sizeof(lxu)));
			/* NOTREACHED */
		case SO_ERROR:
			len = sizeof(newval);
			error = kern_getsockopt(td, args->s, level,
			    name, &newval, UIO_SYSSPACE, &len);
			if (error != 0)
				return (error);
			newval = -SV_ABI_ERRNO(td->td_proc, newval);
			return (copyout(&newval, PTRIN(args->optval), len));
			/* NOTREACHED */
		default:
			break;
		}
		break;
	case IPPROTO_IP:
		name = linux_to_bsd_ip_sockopt(args->optname);
		break;
	case IPPROTO_IPV6:
		name = linux_to_bsd_ip6_sockopt(args->optname);
		break;
	case IPPROTO_TCP:
		name = linux_to_bsd_tcp_sockopt(args->optname);
		break;
	default:
		name = -1;
		break;
	}
	if (name == -1) {
		linux_msg(curthread,
		    "unsupported getsockopt level %d optname %d",
		    args->level, args->optname);
		return (EINVAL);
	}

	if (name == IPV6_NEXTHOP) {
		error = copyin(PTRIN(args->optlen), &len, sizeof(len));
                if (error != 0)
                        return (error);
		sa = malloc(len, M_SONAME, M_WAITOK);

		error = kern_getsockopt(td, args->s, level,
		    name, sa, UIO_SYSSPACE, &len);
		if (error != 0)
			goto out;

		error = bsd_to_linux_sockaddr(sa, &lsa, len);
		if (error == 0)
			error = copyout(lsa, PTRIN(args->optval), len);
		free(lsa, M_SONAME);
		if (error == 0)
			error = copyout(&len, PTRIN(args->optlen),
			    sizeof(len));
out:
		free(sa, M_SONAME);
	} else {
		if (args->optval) {
			error = copyin(PTRIN(args->optlen), &len, sizeof(len));
			if (error != 0)
				return (error);
		}
		error = kern_getsockopt(td, args->s, level,
		    name, PTRIN(args->optval), UIO_USERSPACE, &len);
		if (error == 0)
			error = copyout(&len, PTRIN(args->optlen),
			    sizeof(len));
	}

	return (error);
}

static int
linux_sendfile_common(struct thread *td, l_int out, l_int in,
    l_loff_t *offset, l_size_t count)
{
	off_t bytes_read;
	int error;
	l_loff_t current_offset;
	struct file *fp;

	AUDIT_ARG_FD(in);
	error = fget_read(td, in, &cap_pread_rights, &fp);
	if (error != 0)
		return (error);

	if (offset != NULL) {
		current_offset = *offset;
	} else {
		error = (fp->f_ops->fo_flags & DFLAG_SEEKABLE) != 0 ?
		    fo_seek(fp, 0, SEEK_CUR, td) : ESPIPE;
		if (error != 0)
			goto drop;
		current_offset = td->td_uretoff.tdu_off;
	}

	bytes_read = 0;

	/* Linux cannot have 0 count. */
	if (count <= 0 || current_offset < 0) {
		error = EINVAL;
		goto drop;
	}

	error = fo_sendfile(fp, out, NULL, NULL, current_offset, count,
	    &bytes_read, 0, td);
	if (error != 0)
		goto drop;
	current_offset += bytes_read;

	if (offset != NULL) {
		*offset = current_offset;
	} else {
		error = fo_seek(fp, current_offset, SEEK_SET, td);
		if (error != 0)
			goto drop;
	}

	td->td_retval[0] = (ssize_t)bytes_read;
drop:
	fdrop(fp, td);
	return (error);
}

int
linux_sendfile(struct thread *td, struct linux_sendfile_args *arg)
{
	/*
	 * Differences between FreeBSD and Linux sendfile:
	 * - Linux doesn't send anything when count is 0 (FreeBSD uses 0 to
	 *   mean send the whole file.)  In linux_sendfile given fds are still
	 *   checked for validity when the count is 0.
	 * - Linux can send to any fd whereas FreeBSD only supports sockets.
	 *   The same restriction follows for linux_sendfile.
	 * - Linux doesn't have an equivalent for FreeBSD's flags and sf_hdtr.
	 * - Linux takes an offset pointer and updates it to the read location.
	 *   FreeBSD takes in an offset and a 'bytes read' parameter which is
	 *   only filled if it isn't NULL.  We use this parameter to update the
	 *   offset pointer if it exists.
	 * - Linux sendfile returns bytes read on success while FreeBSD
	 *   returns 0.  We use the 'bytes read' parameter to get this value.
	 */

	l_loff_t offset64;
	l_long offset;
	int ret;
	int error;

	if (arg->offset != NULL) {
		error = copyin(arg->offset, &offset, sizeof(offset));
		if (error != 0)
			return (error);
		offset64 = (l_loff_t)offset;
	}

	ret = linux_sendfile_common(td, arg->out, arg->in,
	    arg->offset != NULL ? &offset64 : NULL, arg->count);

	if (arg->offset != NULL) {
#if defined(__i386__) || defined(__arm__) || \
    (defined(__amd64__) && defined(COMPAT_LINUX32))
		if (offset64 > INT32_MAX)
			return (EOVERFLOW);
#endif
		offset = (l_long)offset64;
		error = copyout(&offset, arg->offset, sizeof(offset));
		if (error != 0)
			return (error);
	}

	return (ret);
}

#if defined(__i386__) || defined(__arm__) || \
    (defined(__amd64__) && defined(COMPAT_LINUX32))

int
linux_sendfile64(struct thread *td, struct linux_sendfile64_args *arg)
{
	l_loff_t offset;
	int ret;
	int error;

	if (arg->offset != NULL) {
		error = copyin(arg->offset, &offset, sizeof(offset));
		if (error != 0)
			return (error);
	}

	ret = linux_sendfile_common(td, arg->out, arg->in,
		arg->offset != NULL ? &offset : NULL, arg->count);

	if (arg->offset != NULL) {
		error = copyout(&offset, arg->offset, sizeof(offset));
		if (error != 0)
			return (error);
	}

	return (ret);
}

/* Argument list sizes for linux_socketcall */
static const unsigned char lxs_args_cnt[] = {
	0 /* unused*/,		3 /* socket */,
	3 /* bind */,		3 /* connect */,
	2 /* listen */,		3 /* accept */,
	3 /* getsockname */,	3 /* getpeername */,
	4 /* socketpair */,	4 /* send */,
	4 /* recv */,		6 /* sendto */,
	6 /* recvfrom */,	2 /* shutdown */,
	5 /* setsockopt */,	5 /* getsockopt */,
	3 /* sendmsg */,	3 /* recvmsg */,
	4 /* accept4 */,	5 /* recvmmsg */,
	4 /* sendmmsg */,	4 /* sendfile */
};
#define	LINUX_ARGS_CNT		(nitems(lxs_args_cnt) - 1)
#define	LINUX_ARG_SIZE(x)	(lxs_args_cnt[x] * sizeof(l_ulong))

int
linux_socketcall(struct thread *td, struct linux_socketcall_args *args)
{
	l_ulong a[6];
#if defined(__amd64__) && defined(COMPAT_LINUX32)
	register_t l_args[6];
#endif
	void *arg;
	int error;

	if (args->what < LINUX_SOCKET || args->what > LINUX_ARGS_CNT)
		return (EINVAL);
	error = copyin(PTRIN(args->args), a, LINUX_ARG_SIZE(args->what));
	if (error != 0)
		return (error);

#if defined(__amd64__) && defined(COMPAT_LINUX32)
	for (int i = 0; i < lxs_args_cnt[args->what]; ++i)
		l_args[i] = a[i];
	arg = l_args;
#else
	arg = a;
#endif
	switch (args->what) {
	case LINUX_SOCKET:
		return (linux_socket(td, arg));
	case LINUX_BIND:
		return (linux_bind(td, arg));
	case LINUX_CONNECT:
		return (linux_connect(td, arg));
	case LINUX_LISTEN:
		return (linux_listen(td, arg));
	case LINUX_ACCEPT:
		return (linux_accept(td, arg));
	case LINUX_GETSOCKNAME:
		return (linux_getsockname(td, arg));
	case LINUX_GETPEERNAME:
		return (linux_getpeername(td, arg));
	case LINUX_SOCKETPAIR:
		return (linux_socketpair(td, arg));
	case LINUX_SEND:
		return (linux_send(td, arg));
	case LINUX_RECV:
		return (linux_recv(td, arg));
	case LINUX_SENDTO:
		return (linux_sendto(td, arg));
	case LINUX_RECVFROM:
		return (linux_recvfrom(td, arg));
	case LINUX_SHUTDOWN:
		return (linux_shutdown(td, arg));
	case LINUX_SETSOCKOPT:
		return (linux_setsockopt(td, arg));
	case LINUX_GETSOCKOPT:
		return (linux_getsockopt(td, arg));
	case LINUX_SENDMSG:
		return (linux_sendmsg(td, arg));
	case LINUX_RECVMSG:
		return (linux_recvmsg(td, arg));
	case LINUX_ACCEPT4:
		return (linux_accept4(td, arg));
	case LINUX_RECVMMSG:
		return (linux_recvmmsg(td, arg));
	case LINUX_SENDMMSG:
		return (linux_sendmmsg(td, arg));
	case LINUX_SENDFILE:
		return (linux_sendfile(td, arg));
	}

	linux_msg(td, "socket type %d not implemented", args->what);
	return (ENOSYS);
}
#endif /* __i386__ || __arm__ || (__amd64__ && COMPAT_LINUX32) */
