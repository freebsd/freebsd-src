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
 *    derived from this software withough specific prior written permission
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
 *
 * $FreeBSD$
 */

/* XXX we use functions that might not exist. */
#include "opt_compat.h"

#ifndef COMPAT_43
#error "Unable to compile Linux-emulator due to missing COMPAT_43 option!"
#endif

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#include <compat/linux/linux_socket.h>
#include <compat/linux/linux_util.h>

/*
 * FreeBSD's socket calls require the sockaddr struct length to agree
 * with the address family.  Linux does not, so we must force it.
 */
static int
linux_to_bsd_namelen(caddr_t name, int namelen)
{
	uint16_t	family;	/* XXX must match Linux sockaddr */

	if (copyin(name, &family, sizeof(family)))
		return namelen;

	switch (family) {
		case AF_INET:
			return sizeof(struct sockaddr_in);
		case AF_INET6:
			return sizeof(struct sockaddr_in6);
	}
	return namelen;
}

#ifndef __alpha__
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
linux_to_bsd_sockopt_level(int level)
{

	switch (level) {
	case LINUX_SOL_SOCKET:
		return (SOL_SOCKET);
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
	if (flags & LINUX_MSG_NOSIGNAL)
		;
#endif
	return ret_flags;
}

/* Return 0 if IP_HDRINCL is set for the given socket. */
static int
linux_check_hdrincl(struct thread *td, int s)
{
	struct getsockopt_args /* {
		int s;
		int level;
		int name;
		caddr_t val;
		int *avalsize;
	} */ bsd_args;
	int error;
	caddr_t sg, val, valsize;
	int size_val = sizeof val;
	int optval;

	sg = stackgap_init();
	val = stackgap_alloc(&sg, sizeof(int));
	valsize = stackgap_alloc(&sg, sizeof(int));

	if ((error = copyout(&size_val, valsize, sizeof(size_val))))
		return (error);

	bsd_args.s = s;
	bsd_args.level = IPPROTO_IP;
	bsd_args.name = IP_HDRINCL;
	bsd_args.val = val;
	bsd_args.avalsize = (int *)valsize;
	if ((error = getsockopt(td, &bsd_args)))
		return (error);

	if ((error = copyin(val, &optval, sizeof(optval))))
		return (error);

	return (optval == 0);
}

/*
 * Updated sendto() when IP_HDRINCL is set:
 * tweak endian-dependent fields in the IP packet.
 */
static int
linux_sendto_hdrincl(struct thread *td, struct sendto_args *bsd_args)
{
/*
 * linux_ip_copysize defines how many bytes we should copy
 * from the beginning of the IP packet before we customize it for BSD.
 * It should include all the fields we modify (ip_len and ip_off)
 * and be as small as possible to minimize copying overhead.
 */
#define linux_ip_copysize	8

	caddr_t sg;
	struct ip *packet;
	struct msghdr *msg;
	struct iovec *iov;

	int error;
	struct  sendmsg_args /* {
		int s;
		caddr_t msg;
		int flags;
	} */ sendmsg_args;

	/* Check the packet isn't too small before we mess with it */
	if (bsd_args->len < linux_ip_copysize)
		return (EINVAL);

	/*
	 * Tweaking the user buffer in place would be bad manners.
	 * We create a corrected IP header with just the needed length,
	 * then use an iovec to glue it to the rest of the user packet
	 * when calling sendmsg().
	 */
	sg = stackgap_init();
	packet = (struct ip *)stackgap_alloc(&sg, linux_ip_copysize);
	msg = (struct msghdr *)stackgap_alloc(&sg, sizeof(*msg));
	iov = (struct iovec *)stackgap_alloc(&sg, sizeof(*iov)*2);

	/* Make a copy of the beginning of the packet to be sent */
	if ((error = copyin(bsd_args->buf, packet, linux_ip_copysize)))
		return (error);

	/* Convert fields from Linux to BSD raw IP socket format */
	packet->ip_len = bsd_args->len;
	packet->ip_off = ntohs(packet->ip_off);

	/* Prepare the msghdr and iovec structures describing the new packet */
	msg->msg_name = bsd_args->to;
	msg->msg_namelen = bsd_args->tolen;
	msg->msg_iov = iov;
	msg->msg_iovlen = 2;
	msg->msg_control = NULL;
	msg->msg_controllen = 0;
	msg->msg_flags = 0;
	iov[0].iov_base = (char *)packet;
	iov[0].iov_len = linux_ip_copysize;
	iov[1].iov_base = (char *)(bsd_args->buf) + linux_ip_copysize;
	iov[1].iov_len = bsd_args->len - linux_ip_copysize;

	sendmsg_args.s = bsd_args->s;
	sendmsg_args.msg = (caddr_t)msg;
	sendmsg_args.flags = bsd_args->flags;
	return (sendmsg(td, &sendmsg_args));
}

struct linux_socket_args {
	int domain;
	int type;
	int protocol;
};

static int
linux_socket(struct thread *td, struct linux_socket_args *args)
{
	struct linux_socket_args linux_args;
	struct socket_args /* {
		int domain;
		int type;
		int protocol;
	} */ bsd_args;
	int error;
	int retval_socket;

	if ((error = copyin(args, &linux_args, sizeof(linux_args))))
		return (error);

	bsd_args.protocol = linux_args.protocol;
	bsd_args.type = linux_args.type;
	bsd_args.domain = linux_to_bsd_domain(linux_args.domain);
	if (bsd_args.domain == -1)
		return (EINVAL);

	retval_socket = socket(td, &bsd_args);
	if (bsd_args.type == SOCK_RAW
	    && (bsd_args.protocol == IPPROTO_RAW || bsd_args.protocol == 0)
	    && bsd_args.domain == AF_INET
	    && retval_socket >= 0) {
		/* It's a raw IP socket: set the IP_HDRINCL option. */
		struct setsockopt_args /* {
			int s;
			int level;
			int name;
			caddr_t val;
			int valsize;
		} */ bsd_setsockopt_args;
		caddr_t sg;
		int *hdrincl;

		sg = stackgap_init();
		hdrincl = (int *)stackgap_alloc(&sg, sizeof(*hdrincl));
		*hdrincl = 1;
		bsd_setsockopt_args.s = td->td_retval[0];
		bsd_setsockopt_args.level = IPPROTO_IP;
		bsd_setsockopt_args.name = IP_HDRINCL;
		bsd_setsockopt_args.val = (caddr_t)hdrincl;
		bsd_setsockopt_args.valsize = sizeof(*hdrincl);
		/* We ignore any error returned by setsockopt() */
		setsockopt(td, &bsd_setsockopt_args);
		/* Copy back the return value from socket() */
		td->td_retval[0] = bsd_setsockopt_args.s;
	}

	return (retval_socket);
}

struct linux_bind_args {
	int s;
	struct sockaddr *name;
	int namelen;
};

static int
linux_bind(struct thread *td, struct linux_bind_args *args)
{
	struct linux_bind_args linux_args;
	struct bind_args /* {
		int s;
		caddr_t name;
		int namelen;
	} */ bsd_args;
	int error;

	if ((error = copyin(args, &linux_args, sizeof(linux_args))))
		return (error);

	bsd_args.s = linux_args.s;
	bsd_args.name = (caddr_t)linux_args.name;
	bsd_args.namelen = linux_to_bsd_namelen(bsd_args.name, linux_args.namelen);
	return (bind(td, &bsd_args));
}

struct linux_connect_args {
	int s;
	struct sockaddr * name;
	int namelen;
};
int linux_connect(struct thread *, struct linux_connect_args *);
#endif /* !__alpha__*/

int
linux_connect(struct thread *td, struct linux_connect_args *args)
{
	struct linux_connect_args linux_args;
	struct connect_args /* {
		int s;
		caddr_t name;
		int namelen;
	} */ bsd_args;
	struct socket *so;
	struct file *fp;
	int error;

#ifdef __alpha__
	bcopy(args, &linux_args, sizeof(linux_args));
#else
	if ((error = copyin(args, &linux_args, sizeof(linux_args))))
		return (error);
#endif /* __alpha__ */

	bsd_args.s = linux_args.s;
	bsd_args.name = (caddr_t)linux_args.name;
	bsd_args.namelen = linux_to_bsd_namelen(bsd_args.name, linux_args.namelen);
	error = connect(td, &bsd_args);
	if (error != EISCONN)
		return (error);

	/*
	 * Linux doesn't return EISCONN the first time it occurs,
	 * when on a non-blocking socket. Instead it returns the
	 * error getsockopt(SOL_SOCKET, SO_ERROR) would return on BSD.
	 */
	error = holdsock(td->td_proc->p_fd, linux_args.s, &fp);
	if (error)
		return (error);
	error = EISCONN;
	if (fp->f_flag & FNONBLOCK) {
		so = (struct socket *)fp->f_data;
		if (so->so_emuldata == 0)
			error = so->so_error;
		so->so_emuldata = (void *)1;
	}
	fdrop(fp, td);
	return (error);
}

#ifndef __alpha__

struct linux_listen_args {
	int s;
	int backlog;
};

static int
linux_listen(struct thread *td, struct linux_listen_args *args)
{
	struct linux_listen_args linux_args;
	struct listen_args /* {
		int s;
		int backlog;
	} */ bsd_args;
	int error;

	if ((error = copyin(args, &linux_args, sizeof(linux_args))))
		return (error);

	bsd_args.s = linux_args.s;
	bsd_args.backlog = linux_args.backlog;
	return (listen(td, &bsd_args));
}

struct linux_accept_args {
	int s;
	struct sockaddr *addr;
	int *namelen;
};

static int
linux_accept(struct thread *td, struct linux_accept_args *args)
{
	struct linux_accept_args linux_args;
	struct accept_args /* {
		int s;
		caddr_t name;
		int *anamelen;
	} */ bsd_args;
	struct fcntl_args /* {
		int fd;
		int cmd;
		long arg;
	} */ f_args;
	int error;

	if ((error = copyin(args, &linux_args, sizeof(linux_args))))
		return (error);

	bsd_args.s = linux_args.s;
	bsd_args.name = (caddr_t)linux_args.addr;
	bsd_args.anamelen = linux_args.namelen;
	error = oaccept(td, &bsd_args);
	if (error)
		return (error);

	/*
	 * linux appears not to copy flags from the parent socket to the
	 * accepted one, so we must clear the flags in the new descriptor.
	 * Ignore any errors, because we already have an open fd.
	 */
	f_args.fd = td->td_retval[0];
	f_args.cmd = F_SETFL;
	f_args.arg = 0;
	(void)fcntl(td, &f_args);
	td->td_retval[0] = f_args.fd;
	return (0);
}

struct linux_getsockname_args {
	int s;
	struct sockaddr *addr;
	int *namelen;
};

static int
linux_getsockname(struct thread *td, struct linux_getsockname_args *args)
{
	struct linux_getsockname_args linux_args;
	struct getsockname_args /* {
		int fdes;
		caddr_t asa;
		int *alen;
	} */ bsd_args;
	int error;

	if ((error = copyin(args, &linux_args, sizeof(linux_args))))
		return (error);

	bsd_args.fdes = linux_args.s;
	bsd_args.asa = (caddr_t) linux_args.addr;
	bsd_args.alen = linux_args.namelen;
	return (ogetsockname(td, &bsd_args));
}

struct linux_getpeername_args {
	int s;
	struct sockaddr *addr;
	int *namelen;
};

static int
linux_getpeername(struct thread *td, struct linux_getpeername_args *args)
{
	struct linux_getpeername_args linux_args;
	struct ogetpeername_args /* {
		int fdes;
		caddr_t asa;
		int *alen;
	} */ bsd_args;
	int error;

	if ((error = copyin(args, &linux_args, sizeof(linux_args))))
		return (error);

	bsd_args.fdes = linux_args.s;
	bsd_args.asa = (caddr_t) linux_args.addr;
	bsd_args.alen = linux_args.namelen;
	return (ogetpeername(td, &bsd_args));
}

struct linux_socketpair_args {
	int domain;
	int type;
	int protocol;
	int *rsv;
};

static int
linux_socketpair(struct thread *td, struct linux_socketpair_args *args)
{
	struct linux_socketpair_args linux_args;
	struct socketpair_args /* {
		int domain;
		int type;
		int protocol;
		int *rsv;
	} */ bsd_args;
	int error;

	if ((error = copyin(args, &linux_args, sizeof(linux_args))))
		return (error);

	bsd_args.domain = linux_to_bsd_domain(linux_args.domain);
	if (bsd_args.domain == -1)
		return (EINVAL);

	bsd_args.type = linux_args.type;
	bsd_args.protocol = linux_args.protocol;
	bsd_args.rsv = linux_args.rsv;
	return (socketpair(td, &bsd_args));
}

struct linux_send_args {
	int s;
	void *msg;
	int len;
	int flags;
};

static int
linux_send(struct thread *td, struct linux_send_args *args)
{
	struct linux_send_args linux_args;
	struct osend_args /* {
		int s;
		caddr_t buf;
		int len;
		int flags;
	} */ bsd_args;
	int error;

	if ((error = copyin(args, &linux_args, sizeof(linux_args))))
		return (error);

	bsd_args.s = linux_args.s;
	bsd_args.buf = linux_args.msg;
	bsd_args.len = linux_args.len;
	bsd_args.flags = linux_args.flags;
	return (osend(td, &bsd_args));
}

struct linux_recv_args {
	int s;
	void *msg;
	int len;
	int flags;
};

static int
linux_recv(struct thread *td, struct linux_recv_args *args)
{
	struct linux_recv_args linux_args;
	struct orecv_args /* {
		int s;
		caddr_t buf;
		int len;
		int flags;
	} */ bsd_args;
	int error;

	if ((error = copyin(args, &linux_args, sizeof(linux_args))))
		return (error);

	bsd_args.s = linux_args.s;
	bsd_args.buf = linux_args.msg;
	bsd_args.len = linux_args.len;
	bsd_args.flags = linux_args.flags;
	return (orecv(td, &bsd_args));
}

struct linux_sendto_args {
	int s;
	void *msg;
	int len;
	int flags;
	caddr_t to;
	int tolen;
};

static int
linux_sendto(struct thread *td, struct linux_sendto_args *args)
{
	struct linux_sendto_args linux_args;
	struct sendto_args /* {
		int s;
		caddr_t buf;
		size_t len;
		int flags;
		caddr_t to;
		int tolen;
	} */ bsd_args;
	int error;

	if ((error = copyin(args, &linux_args, sizeof(linux_args))))
		return (error);

	bsd_args.s = linux_args.s;
	bsd_args.buf = linux_args.msg;
	bsd_args.len = linux_args.len;
	bsd_args.flags = linux_args.flags;
	bsd_args.to = linux_args.to;
	bsd_args.tolen = linux_args.tolen;

	if (linux_check_hdrincl(td, linux_args.s) == 0)
		/* IP_HDRINCL set, tweak the packet before sending */
		return (linux_sendto_hdrincl(td, &bsd_args));

	return (sendto(td, &bsd_args));
}

struct linux_recvfrom_args {
	int s;
	void *buf;
	int len;
	int flags;
	caddr_t from;
	int *fromlen;
};

static int
linux_recvfrom(struct thread *td, struct linux_recvfrom_args *args)
{
	struct linux_recvfrom_args linux_args;
	struct recvfrom_args /* {
		int s;
		caddr_t buf;
		size_t len;
		int flags;
		caddr_t from;
		int *fromlenaddr;
	} */ bsd_args;
	int error;

	if ((error = copyin(args, &linux_args, sizeof(linux_args))))
		return (error);

	bsd_args.s = linux_args.s;
	bsd_args.buf = linux_args.buf;
	bsd_args.len = linux_args.len;
	bsd_args.flags = linux_to_bsd_msg_flags(linux_args.flags);
	bsd_args.from = linux_args.from;
	bsd_args.fromlenaddr = linux_args.fromlen;
	return (orecvfrom(td, &bsd_args));
}

struct linux_recvmsg_args {
	int s;
	struct msghdr *msg;
	int flags;
};

static int
linux_recvmsg(struct thread *td, struct linux_recvmsg_args *args)
{
	struct linux_recvmsg_args linux_args;
	struct recvmsg_args /* {
		int	s;
		struct	msghdr *msg;
		int	flags;
	} */ bsd_args;
	int error;

	if ((error = copyin(args, &linux_args, sizeof(linux_args))))
		return (error);

	bsd_args.s = linux_args.s;
	bsd_args.msg = linux_args.msg;
	bsd_args.flags = linux_to_bsd_msg_flags(linux_args.flags);
	return (recvmsg(td, &bsd_args));
}

struct linux_shutdown_args {
	int s;
	int how;
};

static int
linux_shutdown(struct thread *td, struct linux_shutdown_args *args)
{
	struct linux_shutdown_args linux_args;
	struct shutdown_args /* {
		int s;
		int how;
	} */ bsd_args;
	int error;

	if ((error = copyin(args, &linux_args, sizeof(linux_args))))
		return (error);

	bsd_args.s = linux_args.s;
	bsd_args.how = linux_args.how;
	return (shutdown(td, &bsd_args));
}

struct linux_setsockopt_args {
	int s;
	int level;
	int optname;
	void *optval;
	int optlen;
};

static int
linux_setsockopt(struct thread *td, struct linux_setsockopt_args *args)
{
	struct linux_setsockopt_args linux_args;
	struct setsockopt_args /* {
		int s;
		int level;
		int name;
		caddr_t val;
		int valsize;
	} */ bsd_args;
	int error, name;

	if ((error = copyin(args, &linux_args, sizeof(linux_args))))
		return (error);

	bsd_args.s = linux_args.s;
	bsd_args.level = linux_to_bsd_sockopt_level(linux_args.level);
	switch (bsd_args.level) {
	case SOL_SOCKET:
		name = linux_to_bsd_so_sockopt(linux_args.optname);
		break;
	case IPPROTO_IP:
		name = linux_to_bsd_ip_sockopt(linux_args.optname);
		break;
	case IPPROTO_TCP:
		/* Linux TCP option values match BSD's */
		name = linux_args.optname;
		break;
	default:
		name = -1;
		break;
	}
	if (name == -1)
		return (EINVAL);

	bsd_args.name = name;
	bsd_args.val = linux_args.optval;
	bsd_args.valsize = linux_args.optlen;
	return (setsockopt(td, &bsd_args));
}

struct linux_getsockopt_args {
	int s;
	int level;
	int optname;
	void *optval;
	int *optlen;
};

static int
linux_getsockopt(struct thread *td, struct linux_getsockopt_args *args)
{
	struct linux_getsockopt_args linux_args;
	struct getsockopt_args /* {
		int s;
		int level;
		int name;
		caddr_t val;
		int *avalsize;
	} */ bsd_args;
	int error, name;

	if ((error = copyin(args, &linux_args, sizeof(linux_args))))
		return (error);

	bsd_args.s = linux_args.s;
	bsd_args.level = linux_to_bsd_sockopt_level(linux_args.level);
	switch (bsd_args.level) {
	case SOL_SOCKET:
		name = linux_to_bsd_so_sockopt(linux_args.optname);
		break;
	case IPPROTO_IP:
		name = linux_to_bsd_ip_sockopt(linux_args.optname);
		break;
	case IPPROTO_TCP:
		/* Linux TCP option values match BSD's */
		name = linux_args.optname;
		break;
	default:
		name = -1;
		break;
	}
	if (name == -1)
		return (EINVAL);

	bsd_args.name = name;
	bsd_args.val = linux_args.optval;
	bsd_args.avalsize = linux_args.optlen;
	return (getsockopt(td, &bsd_args));
}

int
linux_socketcall(struct thread *td, struct linux_socketcall_args *args)
{
	void *arg = (void *)args->args;

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
		do {
			int error;
			int level;
			caddr_t control;
			struct {
				int s;
				const struct msghdr *msg;
				int flags;
			} *uap = arg;

			error = copyin(&uap->msg->msg_control, &control,
			    sizeof(caddr_t));
			if (error)
				return (error);

			if (control == NULL)
				goto done;

			error = copyin(&((struct cmsghdr*)control)->cmsg_level,
			    &level, sizeof(int));
			if (error)
				return (error);

			if (level == 1) {
				/*
				 * Linux thinks that SOL_SOCKET is 1; we know
				 * that it's really 0xffff, of course.
				 */
				level = SOL_SOCKET;
				error = copyout(&level,
				    &((struct cmsghdr *)control)->cmsg_level,
				    sizeof(int));
				if (error)
					return (error);
			}
		done:
			return (sendmsg(td, arg));
		} while (0);
	case LINUX_RECVMSG:
		return (linux_recvmsg(td, arg));
	}

	uprintf("LINUX: 'socket' typ=%d not implemented\n", args->what);
	return (ENOSYS);
}
#endif	/*!__alpha__*/
