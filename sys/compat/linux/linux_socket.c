/*-
 * Copyright (c) 1995 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syscallsubr.h>
#include <sys/uio.h>
#include <sys/syslog.h>
#include <sys/un.h>
#include <sys/vimage.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
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
#include <compat/linux/linux_socket.h>
#include <compat/linux/linux_util.h>

static int do_sa_get(struct sockaddr **, const struct osockaddr *, int *,
    struct malloc_type *);
static int linux_to_bsd_domain(int);

/*
 * Reads a linux sockaddr and does any necessary translation.
 * Linux sockaddrs don't have a length field, only a family.
 */
static int
linux_getsockaddr(struct sockaddr **sap, const struct osockaddr *osa, int len)
{
	int osalen = len;

	return (do_sa_get(sap, osa, &osalen, M_SONAME));
}

/*
 * Copy the osockaddr structure pointed to by osa to kernel, adjust
 * family and convert to sockaddr.
 */
static int
do_sa_get(struct sockaddr **sap, const struct osockaddr *osa, int *osalen,
    struct malloc_type *mtype)
{
	int error=0, bdom;
	struct sockaddr *sa;
	struct osockaddr *kosa;
	int alloclen;
#ifdef INET6
	int oldv6size;
	struct sockaddr_in6 *sin6;
#endif

	if (*osalen < 2 || *osalen > UCHAR_MAX || !osa)
		return (EINVAL);

	alloclen = *osalen;
#ifdef INET6
	oldv6size = 0;
	/*
	 * Check for old (pre-RFC2553) sockaddr_in6. We may accept it
	 * if it's a v4-mapped address, so reserve the proper space
	 * for it.
	 */
	if (alloclen == sizeof (struct sockaddr_in6) - sizeof (u_int32_t)) {
		alloclen = sizeof (struct sockaddr_in6);
		oldv6size = 1;
	}
#endif

	MALLOC(kosa, struct osockaddr *, alloclen, mtype, M_WAITOK);

	if ((error = copyin(osa, kosa, *osalen)))
		goto out;

	bdom = linux_to_bsd_domain(kosa->sa_family);
	if (bdom == -1) {
		error = EINVAL;
		goto out;
	}

#ifdef INET6
	/*
	 * Older Linux IPv6 code uses obsolete RFC2133 struct sockaddr_in6,
	 * which lacks the scope id compared with RFC2553 one. If we detect
	 * the situation, reject the address and write a message to system log.
	 *
	 * Still accept addresses for which the scope id is not used.
	 */
	if (oldv6size && bdom == AF_INET6) {
		sin6 = (struct sockaddr_in6 *)kosa;
		if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr) ||
		    (!IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr) &&
		     !IN6_IS_ADDR_SITELOCAL(&sin6->sin6_addr) &&
		     !IN6_IS_ADDR_V4COMPAT(&sin6->sin6_addr) &&
		     !IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr) &&
		     !IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr))) {
			sin6->sin6_scope_id = 0;
		} else {
			log(LOG_DEBUG,
			    "obsolete pre-RFC2553 sockaddr_in6 rejected\n");
			error = EINVAL;
			goto out;
		}
	} else
#endif
	if (bdom == AF_INET)
		alloclen = sizeof(struct sockaddr_in);

	sa = (struct sockaddr *) kosa;
	sa->sa_family = bdom;
	sa->sa_len = alloclen;

	*sap = sa;
	*osalen = alloclen;
	return (0);

out:
	FREE(kosa, mtype);
	return (error);
}

static int
linux_to_bsd_domain(int domain)
{

	switch (domain) {
	case LINUX_AF_UNSPEC:
		return (AF_UNSPEC);
	case LINUX_AF_UNIX:
		return (AF_LOCAL);
	case LINUX_AF_INET:
		return (AF_INET);
	case LINUX_AF_INET6:
		return (AF_INET6);
	case LINUX_AF_AX25:
		return (AF_CCITT);
	case LINUX_AF_IPX:
		return (AF_IPX);
	case LINUX_AF_APPLETALK:
		return (AF_APPLETALK);
	}
	return (-1);
}

static int
bsd_to_linux_domain(int domain)
{

	switch (domain) {
	case AF_UNSPEC:
		return (LINUX_AF_UNSPEC);
	case AF_LOCAL:
		return (LINUX_AF_UNIX);
	case AF_INET:
		return (LINUX_AF_INET);
	case AF_INET6:
		return (LINUX_AF_INET6);
	case AF_CCITT:
		return (LINUX_AF_AX25);
	case AF_IPX:
		return (LINUX_AF_IPX);
	case AF_APPLETALK:
		return (LINUX_AF_APPLETALK);
	}
	return (-1);
}

static int
linux_to_bsd_sockopt_level(int level)
{

	switch (level) {
	case LINUX_SOL_SOCKET:
		return (SOL_SOCKET);
	}
	return (level);
}

static int
bsd_to_linux_sockopt_level(int level)
{

	switch (level) {
	case SOL_SOCKET:
		return (LINUX_SOL_SOCKET);
	}
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
		return (SO_SNDBUF);
	case LINUX_SO_RCVBUF:
		return (SO_RCVBUF);
	case LINUX_SO_KEEPALIVE:
		return (SO_KEEPALIVE);
	case LINUX_SO_OOBINLINE:
		return (SO_OOBINLINE);
	case LINUX_SO_LINGER:
		return (SO_LINGER);
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
	return ret_flags;
}

/*
* If bsd_to_linux_sockaddr() or linux_to_bsd_sockaddr() faults, then the
* native syscall will fault.  Thus, we don't really need to check the
* return values for these functions.
*/

static int
bsd_to_linux_sockaddr(struct sockaddr *arg)
{
	struct sockaddr sa;
	size_t sa_len = sizeof(struct sockaddr);
	int error;
	
	if ((error = copyin(arg, &sa, sa_len)))
		return (error);
	
	*(u_short *)&sa = sa.sa_family;
	
	error = copyout(&sa, arg, sa_len);
	
	return (error);
}

static int
linux_to_bsd_sockaddr(struct sockaddr *arg, int len)
{
	struct sockaddr sa;
	size_t sa_len = sizeof(struct sockaddr);
	int error;

	if ((error = copyin(arg, &sa, sa_len)))
		return (error);

	sa.sa_family = *(sa_family_t *)&sa;
	sa.sa_len = len;

	error = copyout(&sa, arg, sa_len);

	return (error);
}


static int
linux_sa_put(struct osockaddr *osa)
{
	struct osockaddr sa;
	int error, bdom;

	/*
	 * Only read/write the osockaddr family part, the rest is
	 * not changed.
	 */
	error = copyin(osa, &sa, sizeof(sa.sa_family));
	if (error)
		return (error);

	bdom = bsd_to_linux_domain(sa.sa_family);
	if (bdom == -1)
		return (EINVAL);

	sa.sa_family = bdom;
	error = copyout(&sa, osa, sizeof(sa.sa_family));
	if (error)
		return (error);

	return (0);
}

static int
linux_sendit(struct thread *td, int s, struct msghdr *mp, int flags,
    enum uio_seg segflg)
{
	struct mbuf *control;
	struct sockaddr *to;
	int error;

	if (mp->msg_name != NULL) {
		error = linux_getsockaddr(&to, mp->msg_name, mp->msg_namelen);
		if (error)
			return (error);
		mp->msg_name = to;
	} else
		to = NULL;

	if (mp->msg_control != NULL) {
		struct cmsghdr *cmsg;

		if (mp->msg_controllen < sizeof(struct cmsghdr)) {
			error = EINVAL;
			goto bad;
		}
		error = sockargs(&control, mp->msg_control,
		    mp->msg_controllen, MT_CONTROL);
		if (error)
			goto bad;

		cmsg = mtod(control, struct cmsghdr *);
		cmsg->cmsg_level = linux_to_bsd_sockopt_level(cmsg->cmsg_level);
	} else
		control = NULL;

	error = kern_sendit(td, s, mp, linux_to_bsd_msg_flags(flags), control,
	    segflg);

bad:
	if (to)
		FREE(to, M_SONAME);
	return (error);
}

/* Return 0 if IP_HDRINCL is set for the given socket. */
static int
linux_check_hdrincl(struct thread *td, int s)
{
	int error, optval, size_val;

	size_val = sizeof(optval);
	error = kern_getsockopt(td, s, IPPROTO_IP, IP_HDRINCL,
	    &optval, UIO_SYSSPACE, &size_val);
	if (error)
		return (error);

	return (optval == 0);
}

struct linux_sendto_args {
	int s;
	l_uintptr_t msg;
	int len;
	int flags;
	l_uintptr_t to;
	int tolen;
};

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

	packet = (struct ip *)malloc(linux_args->len, M_TEMP, M_WAITOK);

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
	    UIO_SYSSPACE);
goout:
	free(packet, M_TEMP);
	return (error);
}

struct linux_socket_args {
	int domain;
	int type;
	int protocol;
};

static int
linux_socket(struct thread *td, struct linux_socket_args *args)
{
	struct socket_args /* {
		int domain;
		int type;
		int protocol;
	} */ bsd_args;
	int retval_socket;

	bsd_args.protocol = args->protocol;
	bsd_args.type = args->type;
	bsd_args.domain = linux_to_bsd_domain(args->domain);
	if (bsd_args.domain == -1)
		return (EINVAL);

	retval_socket = socket(td, &bsd_args);
	if (bsd_args.type == SOCK_RAW
	    && (bsd_args.protocol == IPPROTO_RAW || bsd_args.protocol == 0)
	    && bsd_args.domain == AF_INET
	    && retval_socket >= 0) {
		/* It's a raw IP socket: set the IP_HDRINCL option. */
		int hdrincl;

		hdrincl = 1;
		/* We ignore any error returned by kern_setsockopt() */
		kern_setsockopt(td, td->td_retval[0], IPPROTO_IP, IP_HDRINCL,
		    &hdrincl, UIO_SYSSPACE, sizeof(hdrincl));
	}
#ifdef INET6
	/*
	 * Linux AF_INET6 socket has IPV6_V6ONLY setsockopt set to 0 by
	 * default and some apps depend on this. So, set V6ONLY to 0
	 * for Linux apps if the sysctl value is set to 1.
	 */
	if (bsd_args.domain == PF_INET6 && retval_socket >= 0
#ifndef KLD_MODULE
	    /*
	     * XXX: Avoid undefined symbol error with an IPv4 only
	     * kernel.
	     */
	    && V_ip6_v6only
#endif
	    ) {
		int v6only;

		v6only = 0;
		/* We ignore any error returned by setsockopt() */
		kern_setsockopt(td, td->td_retval[0], IPPROTO_IPV6, IPV6_V6ONLY,
		    &v6only, UIO_SYSSPACE, sizeof(v6only));
	}
#endif

	return (retval_socket);
}

struct linux_bind_args {
	int s;
	l_uintptr_t name;
	int namelen;
};

static int
linux_bind(struct thread *td, struct linux_bind_args *args)
{
	struct sockaddr *sa;
	int error;

	error = linux_getsockaddr(&sa, PTRIN(args->name),
	    args->namelen);
	if (error)
		return (error);

	error = kern_bind(td, args->s, sa);
	free(sa, M_SONAME);
	if (error == EADDRNOTAVAIL && args->namelen != sizeof(struct sockaddr_in))
	   	return (EINVAL);
	return (error);
}

struct linux_connect_args {
	int s;
	l_uintptr_t name;
	int namelen;
};
int linux_connect(struct thread *, struct linux_connect_args *);

int
linux_connect(struct thread *td, struct linux_connect_args *args)
{
	struct socket *so;
	struct sockaddr *sa;
	u_int fflag;
	int error;

	error = linux_getsockaddr(&sa, (struct osockaddr *)PTRIN(args->name),
	    args->namelen);
	if (error)
		return (error);

	error = kern_connect(td, args->s, sa);
	free(sa, M_SONAME);
	if (error != EISCONN)
		return (error);

	/*
	 * Linux doesn't return EISCONN the first time it occurs,
	 * when on a non-blocking socket. Instead it returns the
	 * error getsockopt(SOL_SOCKET, SO_ERROR) would return on BSD.
	 *
	 * XXXRW: Instead of using fgetsock(), check that it is a
	 * socket and use the file descriptor reference instead of
	 * creating a new one.
	 */
	error = fgetsock(td, args->s, &so, &fflag);
	if (error == 0) {
		error = EISCONN;
		if (fflag & FNONBLOCK) {
			SOCK_LOCK(so);
			if (so->so_emuldata == 0)
				error = so->so_error;
			so->so_emuldata = (void *)1;
			SOCK_UNLOCK(so);
		}
		fputsock(so);
	}
	return (error);
}

struct linux_listen_args {
	int s;
	int backlog;
};

static int
linux_listen(struct thread *td, struct linux_listen_args *args)
{
	struct listen_args /* {
		int s;
		int backlog;
	} */ bsd_args;

	bsd_args.s = args->s;
	bsd_args.backlog = args->backlog;
	return (listen(td, &bsd_args));
}

struct linux_accept_args {
	int s;
	l_uintptr_t addr;
	l_uintptr_t namelen;
};

static int
linux_accept(struct thread *td, struct linux_accept_args *args)
{
	struct accept_args /* {
		int	s;
		struct sockaddr * __restrict name;
		socklen_t * __restrict anamelen;
	} */ bsd_args;
	int error, fd;

	bsd_args.s = args->s;
	/* XXX: */
	bsd_args.name = (struct sockaddr * __restrict)PTRIN(args->addr);
	bsd_args.anamelen = PTRIN(args->namelen);/* XXX */
	error = accept(td, &bsd_args);
	bsd_to_linux_sockaddr((struct sockaddr *)bsd_args.name);
	if (error) {
		if (error == EFAULT && args->namelen != sizeof(struct sockaddr_in))
			return (EINVAL);
		return (error);
	}
	if (args->addr) {
		error = linux_sa_put(PTRIN(args->addr));
		if (error) {
			(void)kern_close(td, td->td_retval[0]);
			return (error);
		}
	}

	/*
	 * linux appears not to copy flags from the parent socket to the
	 * accepted one, so we must clear the flags in the new descriptor.
	 * Ignore any errors, because we already have an open fd.
	 */
	fd = td->td_retval[0];
	(void)kern_fcntl(td, fd, F_SETFL, 0);
	td->td_retval[0] = fd;
	return (0);
}

struct linux_getsockname_args {
	int s;
	l_uintptr_t addr;
	l_uintptr_t namelen;
};

static int
linux_getsockname(struct thread *td, struct linux_getsockname_args *args)
{
	struct getsockname_args /* {
		int	fdes;
		struct sockaddr * __restrict asa;
		socklen_t * __restrict alen;
	} */ bsd_args;
	int error;

	bsd_args.fdes = args->s;
	/* XXX: */
	bsd_args.asa = (struct sockaddr * __restrict)PTRIN(args->addr);
	bsd_args.alen = PTRIN(args->namelen);	/* XXX */
	error = getsockname(td, &bsd_args);
	bsd_to_linux_sockaddr((struct sockaddr *)bsd_args.asa);
	if (error)
		return (error);
	error = linux_sa_put(PTRIN(args->addr));
	if (error)
		return (error);
	return (0);
}

struct linux_getpeername_args {
	int s;
	l_uintptr_t addr;
	l_uintptr_t namelen;
};

static int
linux_getpeername(struct thread *td, struct linux_getpeername_args *args)
{
	struct getpeername_args /* {
		int fdes;
		caddr_t asa;
		int *alen;
	} */ bsd_args;
	int error;

	bsd_args.fdes = args->s;
	bsd_args.asa = (struct sockaddr *)PTRIN(args->addr);
	bsd_args.alen = (int *)PTRIN(args->namelen);
	error = getpeername(td, &bsd_args);
	bsd_to_linux_sockaddr((struct sockaddr *)bsd_args.asa);
	if (error)
		return (error);
	error = linux_sa_put(PTRIN(args->addr));
	if (error)
		return (error);
	return (0);
}

struct linux_socketpair_args {
	int domain;
	int type;
	int protocol;
	l_uintptr_t rsv;
};

static int
linux_socketpair(struct thread *td, struct linux_socketpair_args *args)
{
	struct socketpair_args /* {
		int domain;
		int type;
		int protocol;
		int *rsv;
	} */ bsd_args;

	bsd_args.domain = linux_to_bsd_domain(args->domain);
	if (bsd_args.domain == -1)
		return (EINVAL);

	bsd_args.type = args->type;
	bsd_args.protocol = args->protocol;
	bsd_args.rsv = (int *)PTRIN(args->rsv);
	return (socketpair(td, &bsd_args));
}

struct linux_send_args {
	int s;
	l_uintptr_t msg;
	int len;
	int flags;
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

	bsd_args.s = args->s;
	bsd_args.buf = (caddr_t)PTRIN(args->msg);
	bsd_args.len = args->len;
	bsd_args.flags = args->flags;
	bsd_args.to = NULL;
	bsd_args.tolen = 0;
	return sendto(td, &bsd_args);
}

struct linux_recv_args {
	int s;
	l_uintptr_t msg;
	int len;
	int flags;
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
	bsd_args.flags = args->flags;
	bsd_args.from = NULL;
	bsd_args.fromlenaddr = 0;
	return (recvfrom(td, &bsd_args));
}

static int
linux_sendto(struct thread *td, struct linux_sendto_args *args)
{
	struct msghdr msg;
	struct iovec aiov;
	int error;

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
	error = linux_sendit(td, args->s, &msg, args->flags, UIO_USERSPACE);
	return (error);
}

struct linux_recvfrom_args {
	int s;
	l_uintptr_t buf;
	int len;
	int flags;
	l_uintptr_t from;
	l_uintptr_t fromlen;
};

static int
linux_recvfrom(struct thread *td, struct linux_recvfrom_args *args)
{
	struct recvfrom_args /* {
		int	s;
		caddr_t	buf;
		size_t	len;
		int	flags;
		struct sockaddr * __restrict from;
		socklen_t * __restrict fromlenaddr;
	} */ bsd_args;
	size_t len;
	int error;

	if ((error = copyin(PTRIN(args->fromlen), &len, sizeof(size_t))))
		return (error);

	bsd_args.s = args->s;
	bsd_args.buf = PTRIN(args->buf);
	bsd_args.len = args->len;
	bsd_args.flags = linux_to_bsd_msg_flags(args->flags);
	/* XXX: */
	bsd_args.from = (struct sockaddr * __restrict)PTRIN(args->from);
	bsd_args.fromlenaddr = PTRIN(args->fromlen);/* XXX */
	
	linux_to_bsd_sockaddr((struct sockaddr *)bsd_args.from, len);
	error = recvfrom(td, &bsd_args);
	bsd_to_linux_sockaddr((struct sockaddr *)bsd_args.from);
	
	if (error)
		return (error);
	if (args->from) {
		error = linux_sa_put((struct osockaddr *)
		    PTRIN(args->from));
		if (error)
			return (error);
	}
	return (0);
}

struct linux_sendmsg_args {
	int s;
	l_uintptr_t msg;
	int flags;
};

static int
linux_sendmsg(struct thread *td, struct linux_sendmsg_args *args)
{
	struct msghdr msg;
	struct iovec *iov;
	int error;

	/* XXXTJR sendmsg is broken on amd64 */

	error = copyin(PTRIN(args->msg), &msg, sizeof(msg));
	if (error)
		return (error);

	/*
	 * Some Linux applications (ping) define a non-NULL control data
	 * pointer, but a msg_controllen of 0, which is not allowed in the
	 * FreeBSD system call interface.  NULL the msg_control pointer in
	 * order to handle this case.  This should be checked, but allows the
	 * Linux ping to work.
	 */
	if (msg.msg_control != NULL && msg.msg_controllen == 0)
		msg.msg_control = NULL;
	error = copyiniov(msg.msg_iov, msg.msg_iovlen, &iov, EMSGSIZE);
	if (error)
		return (error);
	msg.msg_iov = iov;
	msg.msg_flags = 0;
	error = linux_sendit(td, args->s, &msg, args->flags, UIO_USERSPACE);
	free(iov, M_IOV);
	return (error);
}

struct linux_recvmsg_args {
	int s;
	l_uintptr_t msg;
	int flags;
};

static int
linux_recvmsg(struct thread *td, struct linux_recvmsg_args *args)
{
	struct recvmsg_args /* {
		int	s;
		struct	msghdr *msg;
		int	flags;
	} */ bsd_args;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	int error;

	/* XXXTJR recvmsg is broken on amd64 */

	if ((error = copyin(PTRIN(args->msg), &msg, sizeof (msg))))
		return (error);

	bsd_args.s = args->s;
	bsd_args.msg = PTRIN(args->msg);
	bsd_args.flags = linux_to_bsd_msg_flags(args->flags);
	if (msg.msg_name) {
	   	linux_to_bsd_sockaddr((struct sockaddr *)msg.msg_name,
		      msg.msg_namelen);
		error = recvmsg(td, &bsd_args);
		bsd_to_linux_sockaddr((struct sockaddr *)msg.msg_name);
	} else
	   	error = recvmsg(td, &bsd_args);
	if (error)
		return (error);

	if (bsd_args.msg->msg_control != NULL &&
	    bsd_args.msg->msg_controllen > 0) {
		cmsg = (struct cmsghdr*)bsd_args.msg->msg_control;
		cmsg->cmsg_level = bsd_to_linux_sockopt_level(cmsg->cmsg_level);
	}

	error = copyin(PTRIN(args->msg), &msg, sizeof(msg));
	if (error)
		return (error);
	if (msg.msg_name && msg.msg_namelen > 2)
		error = linux_sa_put(msg.msg_name);
	return (error);
}

struct linux_shutdown_args {
	int s;
	int how;
};

static int
linux_shutdown(struct thread *td, struct linux_shutdown_args *args)
{
	struct shutdown_args /* {
		int s;
		int how;
	} */ bsd_args;

	bsd_args.s = args->s;
	bsd_args.how = args->how;
	return (shutdown(td, &bsd_args));
}

struct linux_setsockopt_args {
	int s;
	int level;
	int optname;
	l_uintptr_t optval;
	int optlen;
};

static int
linux_setsockopt(struct thread *td, struct linux_setsockopt_args *args)
{
	struct setsockopt_args /* {
		int s;
		int level;
		int name;
		caddr_t val;
		int valsize;
	} */ bsd_args;
	int error, name;

	bsd_args.s = args->s;
	bsd_args.level = linux_to_bsd_sockopt_level(args->level);
	switch (bsd_args.level) {
	case SOL_SOCKET:
		name = linux_to_bsd_so_sockopt(args->optname);
		break;
	case IPPROTO_IP:
		name = linux_to_bsd_ip_sockopt(args->optname);
		break;
	case IPPROTO_TCP:
		/* Linux TCP option values match BSD's */
		name = args->optname;
		break;
	default:
		name = -1;
		break;
	}
	if (name == -1)
		return (ENOPROTOOPT);

	bsd_args.name = name;
	bsd_args.val = PTRIN(args->optval);
	bsd_args.valsize = args->optlen;

	if (name == IPV6_NEXTHOP) {
		linux_to_bsd_sockaddr((struct sockaddr *)bsd_args.val,
			bsd_args.valsize);
		error = setsockopt(td, &bsd_args);
		bsd_to_linux_sockaddr((struct sockaddr *)bsd_args.val);
	} else
		error = setsockopt(td, &bsd_args);

	return (error);
}

struct linux_getsockopt_args {
	int s;
	int level;
	int optname;
	l_uintptr_t optval;
	l_uintptr_t optlen;
};

static int
linux_getsockopt(struct thread *td, struct linux_getsockopt_args *args)
{
	struct getsockopt_args /* {
		int s;
		int level;
		int name;
		caddr_t val;
		int *avalsize;
	} */ bsd_args;
	int error, name;

	bsd_args.s = args->s;
	bsd_args.level = linux_to_bsd_sockopt_level(args->level);
	switch (bsd_args.level) {
	case SOL_SOCKET:
		name = linux_to_bsd_so_sockopt(args->optname);
		break;
	case IPPROTO_IP:
		name = linux_to_bsd_ip_sockopt(args->optname);
		break;
	case IPPROTO_TCP:
		/* Linux TCP option values match BSD's */
		name = args->optname;
		break;
	default:
		name = -1;
		break;
	}
	if (name == -1)
		return (EINVAL);

	bsd_args.name = name;
	bsd_args.val = PTRIN(args->optval);
	bsd_args.avalsize = PTRIN(args->optlen);

	if (name == IPV6_NEXTHOP) {
		error = getsockopt(td, &bsd_args);
		bsd_to_linux_sockaddr((struct sockaddr *)bsd_args.val);
	} else
		error = getsockopt(td, &bsd_args);

	return (error);
}

int
linux_socketcall(struct thread *td, struct linux_socketcall_args *args)
{
	void *arg = (void *)(intptr_t)args->args;

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
	}

	uprintf("LINUX: 'socket' typ=%d not implemented\n", args->what);
	return (ENOSYS);
}
