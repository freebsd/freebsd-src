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
 *  $FreeBSD$
 */

/* XXX we use functions that might not exist. */
#define	COMPAT_43	1

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <netinet/in.h>

#include <i386/linux/linux.h>
#include <i386/linux/linux_proto.h>

static int
linux_to_bsd_domain(int domain)
{
    switch (domain) {
    case LINUX_AF_UNSPEC:
	return AF_UNSPEC;
    case LINUX_AF_UNIX:
	return AF_LOCAL;
    case LINUX_AF_INET:
	return AF_INET;
    case LINUX_AF_AX25:
	return AF_CCITT;
    case LINUX_AF_IPX:
	return AF_IPX;
    case LINUX_AF_APPLETALK:
	return AF_APPLETALK;
    default:
	return -1;
    }
}

static int
linux_to_bsd_sockopt_level(int level)
{
    switch (level) {
    case LINUX_SOL_SOCKET:
	return SOL_SOCKET;
    default:
	return level;
    }
}

static int linux_to_bsd_ip_sockopt(int opt)
{
    switch (opt) {
    case LINUX_IP_TOS:
	return IP_TOS;
    case LINUX_IP_TTL:
	return IP_TTL;
    case LINUX_IP_OPTIONS:
	return IP_OPTIONS;
    case LINUX_IP_MULTICAST_IF:
	return IP_MULTICAST_IF;
    case LINUX_IP_MULTICAST_TTL:
	return IP_MULTICAST_TTL;
    case LINUX_IP_MULTICAST_LOOP:
	return IP_MULTICAST_LOOP;
    case LINUX_IP_ADD_MEMBERSHIP:
	return IP_ADD_MEMBERSHIP;
    case LINUX_IP_DROP_MEMBERSHIP:
	return IP_DROP_MEMBERSHIP;
    case LINUX_IP_HDRINCL:
    default:
	return -1;
    }
}

static int
linux_to_bsd_so_sockopt(int opt)
{
    switch (opt) {
    case LINUX_SO_DEBUG:
	return SO_DEBUG;
    case LINUX_SO_REUSEADDR:
	return SO_REUSEADDR;
    case LINUX_SO_TYPE:
	return SO_TYPE;
    case LINUX_SO_ERROR:
	return SO_ERROR;
    case LINUX_SO_DONTROUTE:
	return SO_DONTROUTE;
    case LINUX_SO_BROADCAST:
	return SO_BROADCAST;
    case LINUX_SO_SNDBUF:
	return SO_SNDBUF;
    case LINUX_SO_RCVBUF:
	return SO_RCVBUF;
    case LINUX_SO_KEEPALIVE:
	return SO_KEEPALIVE;
    case LINUX_SO_OOBINLINE:
	return SO_OOBINLINE;
    case LINUX_SO_LINGER:
	return SO_LINGER;
    case LINUX_SO_PRIORITY:
    case LINUX_SO_NO_CHECK:
    default:
	return -1;
    }
}

struct linux_socket_args {
    int domain;
    int type;
    int protocol;
};

static int
linux_socket(struct proc *p, struct linux_socket_args *args, int *retval)
{
    struct linux_socket_args linux_args;
    struct socket_args /* {
	int domain;
	int type;
	int protocol;
    } */ bsd_args;
    int error;

    if ((error=copyin((caddr_t)args, (caddr_t)&linux_args, sizeof(linux_args))))
	return error;
    bsd_args.protocol = linux_args.protocol;
    bsd_args.type = linux_args.type;
    bsd_args.domain = linux_to_bsd_domain(linux_args.domain);
    if (bsd_args.domain == -1)
	return EINVAL;
    return socket(p, &bsd_args, retval);
}

struct linux_bind_args {
    int s;
    struct sockaddr *name;
    int namelen;
};

static int
linux_bind(struct proc *p, struct linux_bind_args *args, int *retval)
{
    struct linux_bind_args linux_args;
    struct bind_args /* {
	int s;
	caddr_t name;
	int namelen;
    } */ bsd_args;
    int error;

    if ((error=copyin((caddr_t)args, (caddr_t)&linux_args, sizeof(linux_args))))
	return error;
    bsd_args.s = linux_args.s;
    bsd_args.name = (caddr_t)linux_args.name;
    bsd_args.namelen = linux_args.namelen;
    return bind(p, &bsd_args, retval);
}

struct linux_connect_args {
    int s;
    struct sockaddr * name;
    int namelen;
};

static int
linux_connect(struct proc *p, struct linux_connect_args *args, int *retval)
{
    struct linux_connect_args linux_args;
    struct connect_args /* {
	int s;
	caddr_t name;
	int namelen;
    } */ bsd_args;
    int error;

    if ((error=copyin((caddr_t)args, (caddr_t)&linux_args, sizeof(linux_args))))
	return error;
    bsd_args.s = linux_args.s;
    bsd_args.name = (caddr_t)linux_args.name;
    bsd_args.namelen = linux_args.namelen;
    return connect(p, &bsd_args, retval);
}

struct linux_listen_args {
    int s;
    int backlog;
};

static int
linux_listen(struct proc *p, struct linux_listen_args *args, int *retval)
{
    struct linux_listen_args linux_args;
    struct listen_args /* {
	int s;
	int backlog;
    } */ bsd_args;
    int error;

    if ((error=copyin((caddr_t)args, (caddr_t)&linux_args, sizeof(linux_args))))
	return error;
    bsd_args.s = linux_args.s;
    bsd_args.backlog = linux_args.backlog;
    return listen(p, &bsd_args, retval);
}

struct linux_accept_args {
    int s;
    struct sockaddr *addr;
    int *namelen;
};

static int
linux_accept(struct proc *p, struct linux_accept_args *args, int *retval)
{
    struct linux_accept_args linux_args;
    struct accept_args /* {
	int s;
	caddr_t name;
	int *anamelen;
    } */ bsd_args;
    int error;

    if ((error=copyin((caddr_t)args, (caddr_t)&linux_args, sizeof(linux_args))))
	return error;
    bsd_args.s = linux_args.s;
    bsd_args.name = (caddr_t)linux_args.addr;
    bsd_args.anamelen = linux_args.namelen;
    return oaccept(p, &bsd_args, retval);
}

struct linux_getsockname_args {
    int s;
    struct sockaddr *addr;
    int *namelen;
};

static int
linux_getsockname(struct proc *p, struct linux_getsockname_args *args, int *retval)
{
    struct linux_getsockname_args linux_args;
    struct getsockname_args /* {
	int fdes;
	caddr_t asa;
	int *alen;
    } */ bsd_args;
    int error;

    if ((error=copyin((caddr_t)args, (caddr_t)&linux_args, sizeof(linux_args))))
	return error;
    bsd_args.fdes = linux_args.s;
    bsd_args.asa = (caddr_t) linux_args.addr;
    bsd_args.alen = linux_args.namelen;
    return ogetsockname(p, &bsd_args, retval);
}

struct linux_getpeername_args {
    int s;
    struct sockaddr *addr;
    int *namelen;
};

static int
linux_getpeername(struct proc *p, struct linux_getpeername_args *args, int *retval)
{
    struct linux_getpeername_args linux_args;
    struct ogetpeername_args /* {
	int fdes;
	caddr_t asa;
	int *alen;
    } */ bsd_args;
    int error;

    if ((error=copyin((caddr_t)args, (caddr_t)&linux_args, sizeof(linux_args))))
	return error;
    bsd_args.fdes = linux_args.s;
    bsd_args.asa = (caddr_t) linux_args.addr;
    bsd_args.alen = linux_args.namelen;
    return ogetpeername(p, &bsd_args, retval);
}

struct linux_socketpair_args {
    int domain;
    int type;
    int protocol;
    int *rsv;
};

static int
linux_socketpair(struct proc *p, struct linux_socketpair_args *args, int *retval)
{
    struct linux_socketpair_args linux_args;
    struct socketpair_args /* {
	int domain;
	int type;
	int protocol;
	int *rsv;
    } */ bsd_args;
    int error;

    if ((error=copyin((caddr_t)args, (caddr_t)&linux_args, sizeof(linux_args))))
	return error;
    bsd_args.domain = linux_to_bsd_domain(linux_args.domain);
    if (bsd_args.domain == -1)
	return EINVAL;
    bsd_args.type = linux_args.type;
    bsd_args.protocol = linux_args.protocol;
    bsd_args.rsv = linux_args.rsv;
    return socketpair(p, &bsd_args, retval);
}

struct linux_send_args {
    int s;
    void *msg;
    int len;
    int flags;
};

static int
linux_send(struct proc *p, struct linux_send_args *args, int *retval)
{
    struct linux_send_args linux_args;
    struct osend_args /* {
	int s;
	caddr_t buf;
	int len;
	int flags;
    } */ bsd_args;
    int error;

    if ((error=copyin((caddr_t)args, (caddr_t)&linux_args, sizeof(linux_args))))
	return error;
    bsd_args.s = linux_args.s;
    bsd_args.buf = linux_args.msg;
    bsd_args.len = linux_args.len;
    bsd_args.flags = linux_args.flags;
    return osend(p, &bsd_args, retval);
}

struct linux_recv_args {
    int s;
    void *msg;
    int len;
    int flags;
};

static int
linux_recv(struct proc *p, struct linux_recv_args *args, int *retval)
{
    struct linux_recv_args linux_args;
    struct orecv_args /* {
	int s;
	caddr_t buf;
	int len;
	int flags;
    } */ bsd_args;
    int error;

    if ((error=copyin((caddr_t)args, (caddr_t)&linux_args, sizeof(linux_args))))
	return error;
    bsd_args.s = linux_args.s;
    bsd_args.buf = linux_args.msg;
    bsd_args.len = linux_args.len;
    bsd_args.flags = linux_args.flags;
    return orecv(p, &bsd_args, retval);
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
linux_sendto(struct proc *p, struct linux_sendto_args *args, int *retval)
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

    if ((error=copyin((caddr_t)args, (caddr_t)&linux_args, sizeof(linux_args))))
	return error;
    bsd_args.s = linux_args.s;
    bsd_args.buf = linux_args.msg;
    bsd_args.len = linux_args.len;
    bsd_args.flags = linux_args.flags;
    bsd_args.to = linux_args.to;
    bsd_args.tolen = linux_args.tolen;
    return sendto(p, &bsd_args, retval);
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
linux_recvfrom(struct proc *p, struct linux_recvfrom_args *args, int *retval)
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

    if ((error=copyin((caddr_t)args, (caddr_t)&linux_args, sizeof(linux_args))))
	return error;
    bsd_args.s = linux_args.s;
    bsd_args.buf = linux_args.buf;
    bsd_args.len = linux_args.len;
    bsd_args.flags = linux_args.flags;
    bsd_args.from = linux_args.from;
    bsd_args.fromlenaddr = linux_args.fromlen;
    return orecvfrom(p, &bsd_args, retval);
}

struct linux_shutdown_args {
    int s;
    int how;
};

static int
linux_shutdown(struct proc *p, struct linux_shutdown_args *args, int *retval)
{
    struct linux_shutdown_args linux_args;
    struct shutdown_args /* {
	int s;
	int how;
    } */ bsd_args;
    int error;

    if ((error=copyin((caddr_t)args, (caddr_t)&linux_args, sizeof(linux_args))))
	return error;
    bsd_args.s = linux_args.s;
    bsd_args.how = linux_args.how;
    return shutdown(p, &bsd_args, retval);
}

struct linux_setsockopt_args {
    int s;
    int level;
    int optname;
    void *optval;
    int optlen;
};

static int
linux_setsockopt(struct proc *p, struct linux_setsockopt_args *args, int *retval)
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

    if ((error=copyin((caddr_t)args, (caddr_t)&linux_args, sizeof(linux_args))))
	return error;
    bsd_args.s = linux_args.s;
    bsd_args.level = linux_to_bsd_sockopt_level(linux_args.level);
    switch (bsd_args.level) {
    case SOL_SOCKET:
	name = linux_to_bsd_so_sockopt(linux_args.optname);
	break;
    case IPPROTO_IP:
	name = linux_to_bsd_ip_sockopt(linux_args.optname);
	break;
    default:
	return EINVAL;
    }
    if (name == -1)
	return EINVAL;
    bsd_args.name = name;
    bsd_args.val = linux_args.optval;
    bsd_args.valsize = linux_args.optlen;
    return setsockopt(p, &bsd_args, retval);
}

struct linux_getsockopt_args {
    int s;
    int level;
    int optname;
    void *optval;
    int *optlen;
};

static int
linux_getsockopt(struct proc *p, struct linux_getsockopt_args *args, int *retval)
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

    if ((error=copyin((caddr_t)args, (caddr_t)&linux_args, sizeof(linux_args))))
	return error;
    bsd_args.s = linux_args.s;
    bsd_args.level = linux_to_bsd_sockopt_level(linux_args.level);
    switch (bsd_args.level) {
    case SOL_SOCKET:
	name = linux_to_bsd_so_sockopt(linux_args.optname);
	break;
    case IPPROTO_IP:
	name = linux_to_bsd_ip_sockopt(linux_args.optname);
	break;
    default:
	return EINVAL;
    }
    if (name == -1)
	return EINVAL;
    bsd_args.val = linux_args.optval;
    bsd_args.avalsize = linux_args.optlen;
    return getsockopt(p, &bsd_args, retval);
}

int
linux_socketcall(struct proc *p, struct linux_socketcall_args *args,int *retval)
{
    switch (args->what) {
    case LINUX_SOCKET:
	return linux_socket(p, args->args, retval);
    case LINUX_BIND:
	return linux_bind(p, args->args, retval);
    case LINUX_CONNECT:
	return linux_connect(p, args->args, retval);
    case LINUX_LISTEN:
	return linux_listen(p, args->args, retval);
    case LINUX_ACCEPT:
	return linux_accept(p, args->args, retval);
    case LINUX_GETSOCKNAME:
	return linux_getsockname(p, args->args, retval);
    case LINUX_GETPEERNAME:
	return linux_getpeername(p, args->args, retval);
    case LINUX_SOCKETPAIR:
	return linux_socketpair(p, args->args, retval);
    case LINUX_SEND:
	return linux_send(p, args->args, retval);
    case LINUX_RECV:
	return linux_recv(p, args->args, retval);
    case LINUX_SENDTO:
	return linux_sendto(p, args->args, retval);
    case LINUX_RECVFROM:
	return linux_recvfrom(p, args->args, retval);
    case LINUX_SHUTDOWN:
	return linux_shutdown(p, args->args, retval);
    case LINUX_SETSOCKOPT:
	return linux_setsockopt(p, args->args, retval);
    case LINUX_GETSOCKOPT:
	return linux_getsockopt(p, args->args, retval);
    default:
	uprintf("LINUX: 'socket' typ=%d not implemented\n", args->what);
	return ENOSYS;
    }
}
