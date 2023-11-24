/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2002 Dag-Erling Smørgrav
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
 *    derived from this software without specific prior written permission.
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
#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/jail.h>
#include <sys/user.h>
#include <sys/queue.h>
#include <sys/tree.h>

#include <sys/un.h>
#include <sys/unpcb.h>

#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/sctp.h>
#include <netinet/tcp.h>
#define TCPSTATES /* load state names */
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_var.h>
#include <arpa/inet.h>

#include <capsicum_helpers.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <jail.h>
#include <netdb.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libcasper.h>
#include <casper/cap_net.h>
#include <casper/cap_netdb.h>
#include <casper/cap_pwd.h>
#include <casper/cap_sysctl.h>

#define	sstosin(ss)	((struct sockaddr_in *)(ss))
#define	sstosin6(ss)	((struct sockaddr_in6 *)(ss))
#define	sstosun(ss)	((struct sockaddr_un *)(ss))
#define	sstosa(ss)	((struct sockaddr *)(ss))

static int	 opt_4;		/* Show IPv4 sockets */
static int	 opt_6;		/* Show IPv6 sockets */
static int	 opt_C;		/* Show congestion control */
static int	 opt_c;		/* Show connected sockets */
static int	 opt_i;		/* Show inp_gencnt */
static int	 opt_j;		/* Show specified jail */
static int	 opt_L;		/* Don't show IPv4 or IPv6 loopback sockets */
static int	 opt_l;		/* Show listening sockets */
static int	 opt_n;		/* Don't resolve UIDs to user names */
static int	 opt_q;		/* Don't show header */
static int	 opt_S;		/* Show protocol stack if applicable */
static int	 opt_s;		/* Show protocol state if applicable */
static int	 opt_U;		/* Show remote UDP encapsulation port number */
static int	 opt_u;		/* Show Unix domain sockets */
static int	 opt_v;		/* Verbose mode */
static int	 opt_w;		/* Wide print area for addresses */

/*
 * Default protocols to use if no -P was defined.
 */
static const char *default_protos[] = {"sctp", "tcp", "udp", "divert" };
static size_t	   default_numprotos = nitems(default_protos);

static int	*protos;	/* protocols to use */
static size_t	 numprotos;	/* allocated size of protos[] */

static int	*ports;

#define	INT_BIT (sizeof(int)*CHAR_BIT)
#define	SET_PORT(p) do { ports[p / INT_BIT] |= 1 << (p % INT_BIT); } while (0)
#define	CHK_PORT(p) (ports[p / INT_BIT] & (1 << (p % INT_BIT)))

struct addr {
	union {
		struct sockaddr_storage address;
		struct {	/* unix(4) faddr */
			kvaddr_t conn;
			kvaddr_t firstref;
			kvaddr_t nextref;
		};
	};
	unsigned int encaps_port;
	int state;
	struct addr *next;
};

struct sock {
	union {
		RB_ENTRY(sock) socket_tree;	/* tree of pcbs with socket */
		SLIST_ENTRY(sock) socket_list;	/* list of pcbs w/o socket */
	};
	RB_ENTRY(sock) pcb_tree;
	kvaddr_t socket;
	kvaddr_t pcb;
	uint64_t inp_gencnt;
	int shown;
	int vflag;
	int family;
	int proto;
	int state;
	const char *protoname;
	char stack[TCP_FUNCTION_NAME_LEN_MAX];
	char cc[TCP_CA_NAME_MAX];
	struct addr *laddr;
	struct addr *faddr;
};

static RB_HEAD(socks_t, sock) socks = RB_INITIALIZER(&socks);
static int64_t
socket_compare(const struct sock *a, const struct sock *b)
{
	return ((int64_t)(a->socket/2 - b->socket/2));
}
RB_GENERATE_STATIC(socks_t, sock, socket_tree, socket_compare);

static RB_HEAD(pcbs_t, sock) pcbs = RB_INITIALIZER(&pcbs);
static int64_t
pcb_compare(const struct sock *a, const struct sock *b)
{
        return ((int64_t)(a->pcb/2 - b->pcb/2));
}
RB_GENERATE_STATIC(pcbs_t, sock, pcb_tree, pcb_compare);

static SLIST_HEAD(, sock) nosocks = SLIST_HEAD_INITIALIZER(&nosocks);

struct file {
	RB_ENTRY(file)	file_tree;
	kvaddr_t	xf_data;
	pid_t	xf_pid;
	uid_t	xf_uid;
	int	xf_fd;
};

static RB_HEAD(files_t, file) ftree = RB_INITIALIZER(&ftree);
static int64_t
file_compare(const struct file *a, const struct file *b)
{
	return ((int64_t)(a->xf_data/2 - b->xf_data/2));
}
RB_GENERATE_STATIC(files_t, file, file_tree, file_compare);

static struct file *files;
static int nfiles;

static cap_channel_t *capnet;
static cap_channel_t *capnetdb;
static cap_channel_t *capsysctl;
static cap_channel_t *cappwd;

static int
xprintf(const char *fmt, ...)
{
	va_list ap;
	int len;

	va_start(ap, fmt);
	len = vprintf(fmt, ap);
	va_end(ap);
	if (len < 0)
		err(1, "printf()");
	return (len);
}

static bool
_check_ksize(size_t received_size, size_t expected_size, const char *struct_name)
{
	if (received_size != expected_size) {
		warnx("%s size mismatch: expected %zd, received %zd",
		    struct_name, expected_size, received_size);
		return false;
	}
	return true;
}
#define check_ksize(_sz, _struct)	(_check_ksize(_sz, sizeof(_struct), #_struct))

static void
_enforce_ksize(size_t received_size, size_t expected_size, const char *struct_name)
{
	if (received_size != expected_size) {
		errx(1, "fatal: struct %s size mismatch: expected %zd, received %zd",
		    struct_name, expected_size, received_size);
	}
}
#define enforce_ksize(_sz, _struct)	(_enforce_ksize(_sz, sizeof(_struct), #_struct))

static int
get_proto_type(const char *proto)
{
	struct protoent *pent;

	if (strlen(proto) == 0)
		return (0);
	if (capnetdb != NULL)
		pent = cap_getprotobyname(capnetdb, proto);
	else
		pent = getprotobyname(proto);
	if (pent == NULL) {
		warn("cap_getprotobyname");
		return (-1);
	}
	return (pent->p_proto);
}

static void
init_protos(int num)
{
	int proto_count = 0;

	if (num > 0) {
		proto_count = num;
	} else {
		/* Find the maximum number of possible protocols. */
		while (getprotoent() != NULL)
			proto_count++;
		endprotoent();
	}

	if ((protos = malloc(sizeof(int) * proto_count)) == NULL)
		err(1, "malloc");
	numprotos = proto_count;
}

static int
parse_protos(char *protospec)
{
	char *prot;
	int proto_type, proto_index;

	if (protospec == NULL)
		return (-1);

	init_protos(0);
	proto_index = 0;
	while ((prot = strsep(&protospec, ",")) != NULL) {
		if (strlen(prot) == 0)
			continue;
		proto_type = get_proto_type(prot);
		if (proto_type != -1)
			protos[proto_index++] = proto_type;
	}
	numprotos = proto_index;
	return (proto_index);
}

static void
parse_ports(const char *portspec)
{
	const char *p, *q;
	int port, end;

	if (ports == NULL)
		if ((ports = calloc(65536 / INT_BIT, sizeof(int))) == NULL)
			err(1, "calloc()");
	p = portspec;
	while (*p != '\0') {
		if (!isdigit(*p))
			errx(1, "syntax error in port range");
		for (q = p; *q != '\0' && isdigit(*q); ++q)
			/* nothing */ ;
		for (port = 0; p < q; ++p)
			port = port * 10 + digittoint(*p);
		if (port < 0 || port > 65535)
			errx(1, "invalid port number");
		SET_PORT(port);
		switch (*p) {
		case '-':
			++p;
			break;
		case ',':
			++p;
			/* fall through */
		case '\0':
		default:
			continue;
		}
		for (q = p; *q != '\0' && isdigit(*q); ++q)
			/* nothing */ ;
		for (end = 0; p < q; ++p)
			end = end * 10 + digittoint(*p);
		if (end < port || end > 65535)
			errx(1, "invalid port number");
		while (port++ < end)
			SET_PORT(port);
		if (*p == ',')
			++p;
	}
}

static void
sockaddr(struct sockaddr_storage *ss, int af, void *addr, int port)
{
	struct sockaddr_in *sin4;
	struct sockaddr_in6 *sin6;

	bzero(ss, sizeof(*ss));
	switch (af) {
	case AF_INET:
		sin4 = sstosin(ss);
		sin4->sin_len = sizeof(*sin4);
		sin4->sin_family = af;
		sin4->sin_port = port;
		sin4->sin_addr = *(struct in_addr *)addr;
		break;
	case AF_INET6:
		sin6 = sstosin6(ss);
		sin6->sin6_len = sizeof(*sin6);
		sin6->sin6_family = af;
		sin6->sin6_port = port;
		sin6->sin6_addr = *(struct in6_addr *)addr;
#define	s6_addr16	__u6_addr.__u6_addr16
		if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
			sin6->sin6_scope_id =
			    ntohs(sin6->sin6_addr.s6_addr16[1]);
			sin6->sin6_addr.s6_addr16[1] = 0;
		}
		break;
	default:
		abort();
	}
}

static void
free_socket(struct sock *sock)
{
	struct addr *cur, *next;

	cur = sock->laddr;
	while (cur != NULL) {
		next = cur->next;
		free(cur);
		cur = next;
	}
	cur = sock->faddr;
	while (cur != NULL) {
		next = cur->next;
		free(cur);
		cur = next;
	}
	free(sock);
}

static void
gather_sctp(void)
{
	struct sock *sock;
	struct addr *laddr, *prev_laddr, *faddr, *prev_faddr;
	struct xsctp_inpcb *xinpcb;
	struct xsctp_tcb *xstcb;
	struct xsctp_raddr *xraddr;
	struct xsctp_laddr *xladdr;
	const char *varname;
	size_t len, offset;
	char *buf;
	int vflag;
	int no_stcb, local_all_loopback, foreign_all_loopback;

	vflag = 0;
	if (opt_4)
		vflag |= INP_IPV4;
	if (opt_6)
		vflag |= INP_IPV6;

	varname = "net.inet.sctp.assoclist";
	if (cap_sysctlbyname(capsysctl, varname, 0, &len, 0, 0) < 0) {
		if (errno != ENOENT)
			err(1, "cap_sysctlbyname()");
		return;
	}
	if ((buf = (char *)malloc(len)) == NULL) {
		err(1, "malloc()");
		return;
	}
	if (cap_sysctlbyname(capsysctl, varname, buf, &len, 0, 0) < 0) {
		err(1, "cap_sysctlbyname()");
		free(buf);
		return;
	}
	xinpcb = (struct xsctp_inpcb *)(void *)buf;
	offset = sizeof(struct xsctp_inpcb);
	while ((offset < len) && (xinpcb->last == 0)) {
		if ((sock = calloc(1, sizeof *sock)) == NULL)
			err(1, "malloc()");
		sock->socket = xinpcb->socket;
		sock->proto = IPPROTO_SCTP;
		sock->protoname = "sctp";
		if (xinpcb->maxqlen == 0)
			sock->state = SCTP_CLOSED;
		else
			sock->state = SCTP_LISTEN;
		if (xinpcb->flags & SCTP_PCB_FLAGS_BOUND_V6) {
			sock->family = AF_INET6;
			/*
			 * Currently there is no way to distinguish between
			 * IPv6 only sockets or dual family sockets.
			 * So mark it as dual socket.
			 */
			sock->vflag = INP_IPV6 | INP_IPV4;
		} else {
			sock->family = AF_INET;
			sock->vflag = INP_IPV4;
		}
		prev_laddr = NULL;
		local_all_loopback = 1;
		while (offset < len) {
			xladdr = (struct xsctp_laddr *)(void *)(buf + offset);
			offset += sizeof(struct xsctp_laddr);
			if (xladdr->last == 1)
				break;
			if ((laddr = calloc(1, sizeof(struct addr))) == NULL)
				err(1, "malloc()");
			switch (xladdr->address.sa.sa_family) {
			case AF_INET:
#define	__IN_IS_ADDR_LOOPBACK(pina) \
	((ntohl((pina)->s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET)
				if (!__IN_IS_ADDR_LOOPBACK(
				    &xladdr->address.sin.sin_addr))
					local_all_loopback = 0;
#undef	__IN_IS_ADDR_LOOPBACK
				sockaddr(&laddr->address, AF_INET,
				    &xladdr->address.sin.sin_addr,
				    htons(xinpcb->local_port));
				break;
			case AF_INET6:
				if (!IN6_IS_ADDR_LOOPBACK(
				    &xladdr->address.sin6.sin6_addr))
					local_all_loopback = 0;
				sockaddr(&laddr->address, AF_INET6,
				    &xladdr->address.sin6.sin6_addr,
				    htons(xinpcb->local_port));
				break;
			default:
				errx(1, "address family %d not supported",
				    xladdr->address.sa.sa_family);
			}
			laddr->next = NULL;
			if (prev_laddr == NULL)
				sock->laddr = laddr;
			else
				prev_laddr->next = laddr;
			prev_laddr = laddr;
		}
		if (sock->laddr == NULL) {
			if ((sock->laddr =
			    calloc(1, sizeof(struct addr))) == NULL)
				err(1, "malloc()");
			sock->laddr->address.ss_family = sock->family;
			if (sock->family == AF_INET)
				sock->laddr->address.ss_len =
				    sizeof(struct sockaddr_in);
			else
				sock->laddr->address.ss_len =
				    sizeof(struct sockaddr_in6);
			local_all_loopback = 0;
		}
		if ((sock->faddr = calloc(1, sizeof(struct addr))) == NULL)
			err(1, "malloc()");
		sock->faddr->address.ss_family = sock->family;
		if (sock->family == AF_INET)
			sock->faddr->address.ss_len =
			    sizeof(struct sockaddr_in);
		else
			sock->faddr->address.ss_len =
			    sizeof(struct sockaddr_in6);
		no_stcb = 1;
		while (offset < len) {
			xstcb = (struct xsctp_tcb *)(void *)(buf + offset);
			offset += sizeof(struct xsctp_tcb);
			if (no_stcb) {
				if (opt_l && (sock->vflag & vflag) &&
				    (!opt_L || !local_all_loopback) &&
				    ((xinpcb->flags & SCTP_PCB_FLAGS_UDPTYPE) ||
				     (xstcb->last == 1))) {
					RB_INSERT(socks_t, &socks, sock);
				} else {
					free_socket(sock);
				}
			}
			if (xstcb->last == 1)
				break;
			no_stcb = 0;
			if (opt_c) {
				if ((sock = calloc(1, sizeof *sock)) == NULL)
					err(1, "malloc()");
				sock->socket = xinpcb->socket;
				sock->proto = IPPROTO_SCTP;
				sock->protoname = "sctp";
				sock->state = (int)xstcb->state;
				if (xinpcb->flags & SCTP_PCB_FLAGS_BOUND_V6) {
					sock->family = AF_INET6;
				/*
				 * Currently there is no way to distinguish
				 * between IPv6 only sockets or dual family
				 *  sockets. So mark it as dual socket.
				 */
					sock->vflag = INP_IPV6 | INP_IPV4;
				} else {
					sock->family = AF_INET;
					sock->vflag = INP_IPV4;
				}
			}
			prev_laddr = NULL;
			local_all_loopback = 1;
			while (offset < len) {
				xladdr = (struct xsctp_laddr *)(void *)(buf +
				    offset);
				offset += sizeof(struct xsctp_laddr);
				if (xladdr->last == 1)
					break;
				if (!opt_c)
					continue;
				laddr = calloc(1, sizeof(struct addr));
				if (laddr == NULL)
					err(1, "malloc()");
				switch (xladdr->address.sa.sa_family) {
				case AF_INET:
#define	__IN_IS_ADDR_LOOPBACK(pina) \
	((ntohl((pina)->s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET)
					if (!__IN_IS_ADDR_LOOPBACK(
					    &xladdr->address.sin.sin_addr))
						local_all_loopback = 0;
#undef	__IN_IS_ADDR_LOOPBACK
					sockaddr(&laddr->address, AF_INET,
					    &xladdr->address.sin.sin_addr,
					    htons(xstcb->local_port));
					break;
				case AF_INET6:
					if (!IN6_IS_ADDR_LOOPBACK(
					    &xladdr->address.sin6.sin6_addr))
						local_all_loopback = 0;
					sockaddr(&laddr->address, AF_INET6,
					    &xladdr->address.sin6.sin6_addr,
					    htons(xstcb->local_port));
					break;
				default:
					errx(1,
					    "address family %d not supported",
					    xladdr->address.sa.sa_family);
				}
				laddr->next = NULL;
				if (prev_laddr == NULL)
					sock->laddr = laddr;
				else
					prev_laddr->next = laddr;
				prev_laddr = laddr;
			}
			prev_faddr = NULL;
			foreign_all_loopback = 1;
			while (offset < len) {
				xraddr = (struct xsctp_raddr *)(void *)(buf +
				    offset);
				offset += sizeof(struct xsctp_raddr);
				if (xraddr->last == 1)
					break;
				if (!opt_c)
					continue;
				faddr = calloc(1, sizeof(struct addr));
				if (faddr == NULL)
					err(1, "malloc()");
				switch (xraddr->address.sa.sa_family) {
				case AF_INET:
#define	__IN_IS_ADDR_LOOPBACK(pina) \
	((ntohl((pina)->s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET)
					if (!__IN_IS_ADDR_LOOPBACK(
					    &xraddr->address.sin.sin_addr))
						foreign_all_loopback = 0;
#undef	__IN_IS_ADDR_LOOPBACK
					sockaddr(&faddr->address, AF_INET,
					    &xraddr->address.sin.sin_addr,
					    htons(xstcb->remote_port));
					break;
				case AF_INET6:
					if (!IN6_IS_ADDR_LOOPBACK(
					    &xraddr->address.sin6.sin6_addr))
						foreign_all_loopback = 0;
					sockaddr(&faddr->address, AF_INET6,
					    &xraddr->address.sin6.sin6_addr,
					    htons(xstcb->remote_port));
					break;
				default:
					errx(1,
					    "address family %d not supported",
					    xraddr->address.sa.sa_family);
				}
				faddr->encaps_port = xraddr->encaps_port;
				faddr->state = xraddr->state;
				faddr->next = NULL;
				if (prev_faddr == NULL)
					sock->faddr = faddr;
				else
					prev_faddr->next = faddr;
				prev_faddr = faddr;
			}
			if (opt_c) {
				if ((sock->vflag & vflag) &&
				    (!opt_L ||
				     !(local_all_loopback ||
				     foreign_all_loopback))) {
					RB_INSERT(socks_t, &socks, sock);
				} else {
					free_socket(sock);
				}
			}
		}
		xinpcb = (struct xsctp_inpcb *)(void *)(buf + offset);
		offset += sizeof(struct xsctp_inpcb);
	}
	free(buf);
}

static void
gather_inet(int proto)
{
	struct xinpgen *xig, *exig;
	struct xinpcb *xip;
	struct xtcpcb *xtp = NULL;
	struct xsocket *so;
	struct sock *sock;
	struct addr *laddr, *faddr;
	const char *varname, *protoname;
	size_t len, bufsize;
	void *buf;
	int retry, vflag;

	vflag = 0;
	if (opt_4)
		vflag |= INP_IPV4;
	if (opt_6)
		vflag |= INP_IPV6;

	switch (proto) {
	case IPPROTO_TCP:
		varname = "net.inet.tcp.pcblist";
		protoname = "tcp";
		break;
	case IPPROTO_UDP:
		varname = "net.inet.udp.pcblist";
		protoname = "udp";
		break;
	case IPPROTO_DIVERT:
		varname = "net.inet.divert.pcblist";
		protoname = "div";
		break;
	default:
		errx(1, "protocol %d not supported", proto);
	}

	buf = NULL;
	bufsize = 8192;
	retry = 5;
	do {
		for (;;) {
			if ((buf = realloc(buf, bufsize)) == NULL)
				err(1, "realloc()");
			len = bufsize;
			if (cap_sysctlbyname(capsysctl, varname, buf, &len,
			    NULL, 0) == 0)
				break;
			if (errno == ENOENT)
				goto out;
			if (errno != ENOMEM || len != bufsize)
				err(1, "cap_sysctlbyname()");
			bufsize *= 2;
		}
		xig = (struct xinpgen *)buf;
		exig = (struct xinpgen *)(void *)
		    ((char *)buf + len - sizeof *exig);
		enforce_ksize(xig->xig_len, struct xinpgen);
		enforce_ksize(exig->xig_len, struct xinpgen);
	} while (xig->xig_gen != exig->xig_gen && retry--);

	if (xig->xig_gen != exig->xig_gen && opt_v)
		warnx("warning: data may be inconsistent");

	for (;;) {
		xig = (struct xinpgen *)(void *)((char *)xig + xig->xig_len);
		if (xig >= exig)
			break;
		switch (proto) {
		case IPPROTO_TCP:
			xtp = (struct xtcpcb *)xig;
			xip = &xtp->xt_inp;
			if (!check_ksize(xtp->xt_len, struct xtcpcb))
				goto out;
			protoname = xtp->t_flags & TF_TOE ? "toe" : "tcp";
			break;
		case IPPROTO_UDP:
		case IPPROTO_DIVERT:
			xip = (struct xinpcb *)xig;
			if (!check_ksize(xip->xi_len, struct xinpcb))
				goto out;
			break;
		default:
			errx(1, "protocol %d not supported", proto);
		}
		so = &xip->xi_socket;
		if ((xip->inp_vflag & vflag) == 0)
			continue;
		if (xip->inp_vflag & INP_IPV4) {
			if ((xip->inp_fport == 0 && !opt_l) ||
			    (xip->inp_fport != 0 && !opt_c))
				continue;
#define	__IN_IS_ADDR_LOOPBACK(pina) \
	((ntohl((pina)->s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET)
			if (opt_L &&
			    (__IN_IS_ADDR_LOOPBACK(&xip->inp_faddr) ||
			     __IN_IS_ADDR_LOOPBACK(&xip->inp_laddr)))
				continue;
#undef	__IN_IS_ADDR_LOOPBACK
		} else if (xip->inp_vflag & INP_IPV6) {
			if ((xip->inp_fport == 0 && !opt_l) ||
			    (xip->inp_fport != 0 && !opt_c))
				continue;
			if (opt_L &&
			    (IN6_IS_ADDR_LOOPBACK(&xip->in6p_faddr) ||
			     IN6_IS_ADDR_LOOPBACK(&xip->in6p_laddr)))
				continue;
		} else {
			if (opt_v)
				warnx("invalid vflag 0x%x", xip->inp_vflag);
			continue;
		}
		if ((sock = calloc(1, sizeof(*sock))) == NULL)
			err(1, "malloc()");
		if ((laddr = calloc(1, sizeof *laddr)) == NULL)
			err(1, "malloc()");
		if ((faddr = calloc(1, sizeof *faddr)) == NULL)
			err(1, "malloc()");
		sock->socket = so->xso_so;
		sock->proto = proto;
		sock->inp_gencnt = xip->inp_gencnt;
		if (xip->inp_vflag & INP_IPV4) {
			sock->family = AF_INET;
			sockaddr(&laddr->address, sock->family,
			    &xip->inp_laddr, xip->inp_lport);
			sockaddr(&faddr->address, sock->family,
			    &xip->inp_faddr, xip->inp_fport);
		} else if (xip->inp_vflag & INP_IPV6) {
			sock->family = AF_INET6;
			sockaddr(&laddr->address, sock->family,
			    &xip->in6p_laddr, xip->inp_lport);
			sockaddr(&faddr->address, sock->family,
			    &xip->in6p_faddr, xip->inp_fport);
		}
		if (proto == IPPROTO_TCP)
			faddr->encaps_port = xtp->xt_encaps_port;
		laddr->next = NULL;
		faddr->next = NULL;
		sock->laddr = laddr;
		sock->faddr = faddr;
		sock->vflag = xip->inp_vflag;
		if (proto == IPPROTO_TCP) {
			sock->state = xtp->t_state;
			memcpy(sock->stack, xtp->xt_stack,
			    TCP_FUNCTION_NAME_LEN_MAX);
			memcpy(sock->cc, xtp->xt_cc, TCP_CA_NAME_MAX);
		}
		sock->protoname = protoname;
		if (sock->socket != 0)
			RB_INSERT(socks_t, &socks, sock);
		else
			SLIST_INSERT_HEAD(&nosocks, sock, socket_list);
	}
out:
	free(buf);
}

static void
gather_unix(int proto)
{
	struct xunpgen *xug, *exug;
	struct xunpcb *xup;
	struct sock *sock;
	struct addr *laddr, *faddr;
	const char *varname, *protoname;
	size_t len, bufsize;
	void *buf;
	int retry;

	switch (proto) {
	case SOCK_STREAM:
		varname = "net.local.stream.pcblist";
		protoname = "stream";
		break;
	case SOCK_DGRAM:
		varname = "net.local.dgram.pcblist";
		protoname = "dgram";
		break;
	case SOCK_SEQPACKET:
		varname = "net.local.seqpacket.pcblist";
		protoname = "seqpac";
		break;
	default:
		abort();
	}
	buf = NULL;
	bufsize = 8192;
	retry = 5;
	do {
		for (;;) {
			if ((buf = realloc(buf, bufsize)) == NULL)
				err(1, "realloc()");
			len = bufsize;
			if (cap_sysctlbyname(capsysctl, varname, buf, &len,
			    NULL, 0) == 0)
				break;
			if (errno != ENOMEM || len != bufsize)
				err(1, "cap_sysctlbyname()");
			bufsize *= 2;
		}
		xug = (struct xunpgen *)buf;
		exug = (struct xunpgen *)(void *)
		    ((char *)buf + len - sizeof(*exug));
		if (!check_ksize(xug->xug_len, struct xunpgen) ||
		    !check_ksize(exug->xug_len, struct xunpgen))
			goto out;
	} while (xug->xug_gen != exug->xug_gen && retry--);

	if (xug->xug_gen != exug->xug_gen && opt_v)
		warnx("warning: data may be inconsistent");

	for (;;) {
		xug = (struct xunpgen *)(void *)((char *)xug + xug->xug_len);
		if (xug >= exug)
			break;
		xup = (struct xunpcb *)xug;
		if (!check_ksize(xup->xu_len, struct xunpcb))
			goto out;
		if ((xup->unp_conn == 0 && !opt_l) ||
		    (xup->unp_conn != 0 && !opt_c))
			continue;
		if ((sock = calloc(1, sizeof(*sock))) == NULL)
			err(1, "malloc()");
		if ((laddr = calloc(1, sizeof *laddr)) == NULL)
			err(1, "malloc()");
		if ((faddr = calloc(1, sizeof *faddr)) == NULL)
			err(1, "malloc()");
		sock->socket = xup->xu_socket.xso_so;
		sock->pcb = xup->xu_unpp;
		sock->proto = proto;
		sock->family = AF_UNIX;
		sock->protoname = protoname;
		if (xup->xu_addr.sun_family == AF_UNIX)
			laddr->address =
			    *(struct sockaddr_storage *)(void *)&xup->xu_addr;
		faddr->conn = xup->unp_conn;
		faddr->firstref = xup->xu_firstref;
		faddr->nextref = xup->xu_nextref;
		laddr->next = NULL;
		faddr->next = NULL;
		sock->laddr = laddr;
		sock->faddr = faddr;
		RB_INSERT(socks_t, &socks, sock);
		RB_INSERT(pcbs_t, &pcbs, sock);
	}
out:
	free(buf);
}

static void
getfiles(void)
{
	struct xfile *xfiles;
	size_t len, olen;

	olen = len = sizeof(*xfiles);
	if ((xfiles = malloc(len)) == NULL)
		err(1, "malloc()");
	while (cap_sysctlbyname(capsysctl, "kern.file", xfiles, &len, 0, 0)
	    == -1) {
		if (errno != ENOMEM || len != olen)
			err(1, "cap_sysctlbyname()");
		olen = len *= 2;
		if ((xfiles = realloc(xfiles, len)) == NULL)
			err(1, "realloc()");
	}
	if (len > 0)
		enforce_ksize(xfiles->xf_size, struct xfile);
	nfiles = len / sizeof(*xfiles);

	if ((files = malloc(nfiles * sizeof(struct file))) == NULL)
		err(1, "malloc()");

	for (int i = 0; i < nfiles; i++) {
		files[i].xf_data = xfiles[i].xf_data;
		files[i].xf_pid = xfiles[i].xf_pid;
		files[i].xf_uid = xfiles[i].xf_uid;
		files[i].xf_fd = xfiles[i].xf_fd;
		RB_INSERT(files_t, &ftree, &files[i]);
	}

	free(xfiles);
}

static int
printaddr(struct sockaddr_storage *ss)
{
	struct sockaddr_un *sun;
	char addrstr[NI_MAXHOST] = { '\0', '\0' };
	int error, off, port = 0;

	switch (ss->ss_family) {
	case AF_INET:
		if (sstosin(ss)->sin_addr.s_addr == INADDR_ANY)
			addrstr[0] = '*';
		port = ntohs(sstosin(ss)->sin_port);
		break;
	case AF_INET6:
		if (IN6_IS_ADDR_UNSPECIFIED(&sstosin6(ss)->sin6_addr))
			addrstr[0] = '*';
		port = ntohs(sstosin6(ss)->sin6_port);
		break;
	case AF_UNIX:
		sun = sstosun(ss);
		off = (int)((char *)&sun->sun_path - (char *)sun);
		return (xprintf("%.*s", sun->sun_len - off, sun->sun_path));
	}
	if (addrstr[0] == '\0') {
		error = cap_getnameinfo(capnet, sstosa(ss), ss->ss_len,
		    addrstr, sizeof(addrstr), NULL, 0, NI_NUMERICHOST);
		if (error)
			errx(1, "cap_getnameinfo()");
	}
	if (port == 0)
		return xprintf("%s:*", addrstr);
	else
		return xprintf("%s:%d", addrstr, port);
}

static const char *
getprocname(pid_t pid)
{
	static struct kinfo_proc proc;
	size_t len;
	int mib[4];

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PID;
	mib[3] = (int)pid;
	len = sizeof(proc);
	if (cap_sysctl(capsysctl, mib, nitems(mib), &proc, &len, NULL, 0)
	    == -1) {
		/* Do not warn if the process exits before we get its name. */
		if (errno != ESRCH)
			warn("cap_sysctl()");
		return ("??");
	}
	return (proc.ki_comm);
}

static int
getprocjid(pid_t pid)
{
	static struct kinfo_proc proc;
	size_t len;
	int mib[4];

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PID;
	mib[3] = (int)pid;
	len = sizeof(proc);
	if (cap_sysctl(capsysctl, mib, nitems(mib), &proc, &len, NULL, 0)
	    == -1) {
		/* Do not warn if the process exits before we get its jid. */
		if (errno != ESRCH)
			warn("cap_sysctl()");
		return (-1);
	}
	return (proc.ki_jid);
}

static int
check_ports(struct sock *s)
{
	int port;
	struct addr *addr;

	if (ports == NULL)
		return (1);
	if ((s->family != AF_INET) && (s->family != AF_INET6))
		return (1);
	for (addr = s->laddr; addr != NULL; addr = addr->next) {
		if (s->family == AF_INET)
			port = ntohs(sstosin(&addr->address)->sin_port);
		else
			port = ntohs(sstosin6(&addr->address)->sin6_port);
		if (CHK_PORT(port))
			return (1);
	}
	for (addr = s->faddr; addr != NULL; addr = addr->next) {
		if (s->family == AF_INET)
			port = ntohs(sstosin(&addr->address)->sin_port);
		else
			port = ntohs(sstosin6(&addr->address)->sin6_port);
		if (CHK_PORT(port))
			return (1);
	}
	return (0);
}

static const char *
sctp_conn_state(int state)
{
	switch (state) {
	case SCTP_CLOSED:
		return "CLOSED";
		break;
	case SCTP_BOUND:
		return "BOUND";
		break;
	case SCTP_LISTEN:
		return "LISTEN";
		break;
	case SCTP_COOKIE_WAIT:
		return "COOKIE_WAIT";
		break;
	case SCTP_COOKIE_ECHOED:
		return "COOKIE_ECHOED";
		break;
	case SCTP_ESTABLISHED:
		return "ESTABLISHED";
		break;
	case SCTP_SHUTDOWN_SENT:
		return "SHUTDOWN_SENT";
		break;
	case SCTP_SHUTDOWN_RECEIVED:
		return "SHUTDOWN_RECEIVED";
		break;
	case SCTP_SHUTDOWN_ACK_SENT:
		return "SHUTDOWN_ACK_SENT";
		break;
	case SCTP_SHUTDOWN_PENDING:
		return "SHUTDOWN_PENDING";
		break;
	default:
		return "UNKNOWN";
		break;
	}
}

static const char *
sctp_path_state(int state)
{
	switch (state) {
	case SCTP_UNCONFIRMED:
		return "UNCONFIRMED";
		break;
	case SCTP_ACTIVE:
		return "ACTIVE";
		break;
	case SCTP_INACTIVE:
		return "INACTIVE";
		break;
	default:
		return "UNKNOWN";
		break;
	}
}

static void
displaysock(struct sock *s, int pos)
{
	int first, offset;
	struct addr *laddr, *faddr;

	while (pos < 30)
		pos += xprintf(" ");
	pos += xprintf("%s", s->protoname);
	if (s->vflag & INP_IPV4)
		pos += xprintf("4");
	if (s->vflag & INP_IPV6)
		pos += xprintf("6");
	if (s->vflag & (INP_IPV4 | INP_IPV6))
		pos += xprintf(" ");
	laddr = s->laddr;
	faddr = s->faddr;
	first = 1;
	while (laddr != NULL || faddr != NULL) {
		offset = 37;
		while (pos < offset)
			pos += xprintf(" ");
		switch (s->family) {
		case AF_INET:
		case AF_INET6:
			if (laddr != NULL) {
				pos += printaddr(&laddr->address);
				if (s->family == AF_INET6 && pos >= 58)
					pos += xprintf(" ");
			}
			offset += opt_w ? 46 : 22;
			while (pos < offset)
				pos += xprintf(" ");
			if (faddr != NULL)
				pos += printaddr(&faddr->address);
			offset += opt_w ? 46 : 22;
			break;
		case AF_UNIX:
			if ((laddr == NULL) || (faddr == NULL))
				errx(1, "laddr = %p or faddr = %p is NULL",
				    (void *)laddr, (void *)faddr);
			if (laddr->address.ss_len == 0 && faddr->conn == 0) {
				pos += xprintf("(not connected)");
				offset += opt_w ? 92 : 44;
				break;
			}
			/* Local bind(2) address, if any. */
			if (laddr->address.ss_len > 0)
				pos += printaddr(&laddr->address);
			/* Remote peer we connect(2) to, if any. */
			if (faddr->conn != 0) {
				struct sock *p;

				pos += xprintf("%s-> ",
				    laddr->address.ss_len > 0 ? " " : "");
				p = RB_FIND(pcbs_t, &pcbs,
				    &(struct sock){ .pcb = faddr->conn });
				if (__predict_false(p == NULL)) {
					/* XXGL: can this happen at all? */
					pos += xprintf("??");
				}  else if (p->laddr->address.ss_len == 0) {
					struct file *f;

					f = RB_FIND(files_t, &ftree,
					    &(struct file){ .xf_data =
					    p->socket });
					pos += xprintf("[%lu %d]",
					    (u_long)f->xf_pid, f->xf_fd);
				} else
					pos += printaddr(&p->laddr->address);
			}
			/* Remote peer(s) connect(2)ed to us, if any. */
			if (faddr->firstref != 0) {
				struct sock *p;
				struct file *f;
				kvaddr_t ref = faddr->firstref;
				bool fref = true;

				pos += xprintf(" <- ");

				while ((p = RB_FIND(pcbs_t, &pcbs,
				    &(struct sock){ .pcb = ref })) != 0) {
					f = RB_FIND(files_t, &ftree,
					    &(struct file){ .xf_data =
					    p->socket });
					pos += xprintf("%s[%lu %d]",
					    fref ? "" : ",",
					    (u_long)f->xf_pid, f->xf_fd);
					ref = p->faddr->nextref;
					fref = false;
				}
			}
			offset += opt_w ? 92 : 44;
			break;
		default:
			abort();
		}
		if (opt_i) {
			if (s->proto == IPPROTO_TCP ||
			    s->proto == IPPROTO_UDP) {
				while (pos < offset)
					pos += xprintf(" ");
				pos += xprintf("%" PRIu64, s->inp_gencnt);
			}
			offset += 9;
		}
		if (opt_U) {
			if (faddr != NULL &&
			    ((s->proto == IPPROTO_SCTP &&
			      s->state != SCTP_CLOSED &&
			      s->state != SCTP_BOUND &&
			      s->state != SCTP_LISTEN) ||
			     (s->proto == IPPROTO_TCP &&
			      s->state != TCPS_CLOSED &&
			      s->state != TCPS_LISTEN))) {
				while (pos < offset)
					pos += xprintf(" ");
				pos += xprintf("%u",
				    ntohs(faddr->encaps_port));
			}
			offset += 7;
		}
		if (opt_s) {
			if (faddr != NULL &&
			    s->proto == IPPROTO_SCTP &&
			    s->state != SCTP_CLOSED &&
			    s->state != SCTP_BOUND &&
			    s->state != SCTP_LISTEN) {
				while (pos < offset)
					pos += xprintf(" ");
				pos += xprintf("%s",
				    sctp_path_state(faddr->state));
			}
			offset += 13;
		}
		if (first) {
			if (opt_s) {
				if (s->proto == IPPROTO_SCTP ||
				    s->proto == IPPROTO_TCP) {
					while (pos < offset)
						pos += xprintf(" ");
					switch (s->proto) {
					case IPPROTO_SCTP:
						pos += xprintf("%s",
						    sctp_conn_state(s->state));
						break;
					case IPPROTO_TCP:
						if (s->state >= 0 &&
						    s->state < TCP_NSTATES)
							pos += xprintf("%s",
							    tcpstates[s->state]);
						else
							pos += xprintf("?");
						break;
					}
				}
				offset += 13;
			}
			if (opt_S) {
				if (s->proto == IPPROTO_TCP) {
					while (pos < offset)
						pos += xprintf(" ");
					pos += xprintf("%.*s",
					    TCP_FUNCTION_NAME_LEN_MAX,
					    s->stack);
				}
				offset += TCP_FUNCTION_NAME_LEN_MAX + 1;
			}
			if (opt_C) {
				if (s->proto == IPPROTO_TCP) {
					while (pos < offset)
						pos += xprintf(" ");
					xprintf("%.*s", TCP_CA_NAME_MAX, s->cc);
				}
				offset += TCP_CA_NAME_MAX + 1;
			}
		}
		if (laddr != NULL)
			laddr = laddr->next;
		if (faddr != NULL)
			faddr = faddr->next;
		if ((laddr != NULL) || (faddr != NULL)) {
			xprintf("\n");
			pos = 0;
		}
		first = 0;
	}
	xprintf("\n");
}

static void
display(void)
{
	struct passwd *pwd;
	struct file *xf;
	struct sock *s;
	int n, pos;

	if (opt_q != 1) {
		printf("%-8s %-10s %-5s %-3s %-6s %-*s %-*s",
		    "USER", "COMMAND", "PID", "FD", "PROTO",
		    opt_w ? 45 : 21, "LOCAL ADDRESS",
		    opt_w ? 45 : 21, "FOREIGN ADDRESS");
		if (opt_i)
			printf(" %-8s", "ID");
		if (opt_U)
			printf(" %-6s", "ENCAPS");
		if (opt_s) {
			printf(" %-12s", "PATH STATE");
			printf(" %-12s", "CONN STATE");
		}
		if (opt_S)
			printf(" %-*.*s", TCP_FUNCTION_NAME_LEN_MAX,
			    TCP_FUNCTION_NAME_LEN_MAX, "STACK");
		if (opt_C)
			printf(" %-.*s", TCP_CA_NAME_MAX, "CC");
		printf("\n");
	}
	cap_setpassent(cappwd, 1);
	for (xf = files, n = 0; n < nfiles; ++n, ++xf) {
		if (xf->xf_data == 0)
			continue;
		if (opt_j >= 0 && opt_j != getprocjid(xf->xf_pid))
			continue;
		s = RB_FIND(socks_t, &socks,
		    &(struct sock){ .socket = xf->xf_data});
		if (s != NULL && check_ports(s)) {
			s->shown = 1;
			pos = 0;
			if (opt_n ||
			    (pwd = cap_getpwuid(cappwd, xf->xf_uid)) == NULL)
				pos += xprintf("%lu ", (u_long)xf->xf_uid);
			else
				pos += xprintf("%s ", pwd->pw_name);
			while (pos < 9)
				pos += xprintf(" ");
			pos += xprintf("%.10s", getprocname(xf->xf_pid));
			while (pos < 20)
				pos += xprintf(" ");
			pos += xprintf("%5lu ", (u_long)xf->xf_pid);
			while (pos < 26)
				pos += xprintf(" ");
			pos += xprintf("%-3d ", xf->xf_fd);
			displaysock(s, pos);
		}
	}
	if (opt_j >= 0)
		return;
	SLIST_FOREACH(s, &nosocks, socket_list) {
		if (!check_ports(s))
			continue;
		pos = xprintf("%-8s %-10s %-5s %-2s ",
		    "?", "?", "?", "?");
		displaysock(s, pos);
	}
	RB_FOREACH(s, socks_t, &socks) {
		if (s->shown)
			continue;
		if (!check_ports(s))
			continue;
		pos = xprintf("%-8s %-10s %-5s %-2s ",
		    "?", "?", "?", "?");
		displaysock(s, pos);
	}
}

static int
set_default_protos(void)
{
	struct protoent *prot;
	const char *pname;
	size_t pindex;

	init_protos(default_numprotos);

	for (pindex = 0; pindex < default_numprotos; pindex++) {
		pname = default_protos[pindex];
		prot = cap_getprotobyname(capnetdb, pname);
		if (prot == NULL)
			err(1, "cap_getprotobyname: %s", pname);
		protos[pindex] = prot->p_proto;
	}
	numprotos = pindex;
	return (pindex);
}

/*
 * Return the vnet property of the jail, or -1 on error.
 */
static int
jail_getvnet(int jid)
{
	struct iovec jiov[6];
	int vnet;
	size_t len = sizeof(vnet);

	if (sysctlbyname("kern.features.vimage", &vnet, &len, NULL, 0) != 0)
		return (0);

	vnet = -1;
	jiov[0].iov_base = __DECONST(char *, "jid");
	jiov[0].iov_len = sizeof("jid");
	jiov[1].iov_base = &jid;
	jiov[1].iov_len = sizeof(jid);
	jiov[2].iov_base = __DECONST(char *, "vnet");
	jiov[2].iov_len = sizeof("vnet");
	jiov[3].iov_base = &vnet;
	jiov[3].iov_len = sizeof(vnet);
	jiov[4].iov_base = __DECONST(char *, "errmsg");
	jiov[4].iov_len = sizeof("errmsg");
	jiov[5].iov_base = jail_errmsg;
	jiov[5].iov_len = JAIL_ERRMSGLEN;
	jail_errmsg[0] = '\0';
	if (jail_get(jiov, nitems(jiov), 0) < 0) {
		if (!jail_errmsg[0])
			snprintf(jail_errmsg, JAIL_ERRMSGLEN,
			    "jail_get: %s", strerror(errno));
		return (-1);
	}
	return (vnet);
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: sockstat [-46CciLlnqSsUuvw] [-j jid] [-p ports] [-P protocols]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	cap_channel_t *capcas;
	cap_net_limit_t *limit;
	const char *pwdcmds[] = { "setpassent", "getpwuid" };
	const char *pwdfields[] = { "pw_name" };
	int protos_defined = -1;
	int o, i;

	opt_j = -1;
	while ((o = getopt(argc, argv, "46Ccij:Llnp:P:qSsUuvw")) != -1)
		switch (o) {
		case '4':
			opt_4 = 1;
			break;
		case '6':
			opt_6 = 1;
			break;
		case 'C':
			opt_C = 1;
			break;
		case 'c':
			opt_c = 1;
			break;
		case 'i':
			opt_i = 1;
			break;
		case 'j':
			opt_j = jail_getid(optarg);
			if (opt_j < 0)
				errx(1, "jail_getid: %s", jail_errmsg);
			break;
		case 'L':
			opt_L = 1;
			break;
		case 'l':
			opt_l = 1;
			break;
		case 'n':
			opt_n = 1;
			break;
		case 'p':
			parse_ports(optarg);
			break;
		case 'P':
			protos_defined = parse_protos(optarg);
			break;
		case 'q':
			opt_q = 1;
			break;
		case 'S':
			opt_S = 1;
			break;
		case 's':
			opt_s = 1;
			break;
		case 'U':
			opt_U = 1;
			break;
		case 'u':
			opt_u = 1;
			break;
		case 'v':
			++opt_v;
			break;
		case 'w':
			opt_w = 1;
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc > 0)
		usage();

	if (opt_j > 0) {
		switch (jail_getvnet(opt_j)) {
		case -1:
			errx(2, "jail_getvnet: %s", jail_errmsg);
		case JAIL_SYS_NEW:
			if (jail_attach(opt_j) < 0)
				err(3, "jail_attach()");
			/* Set back to -1 for normal output in vnet jail. */
			opt_j = -1;
			break;
		default:
			break;
		}
	}

	capcas = cap_init();
	if (capcas == NULL)
		err(1, "Unable to contact Casper");
	if (caph_enter_casper() < 0)
		err(1, "Unable to enter capability mode");
	capnet = cap_service_open(capcas, "system.net");
	if (capnet == NULL)
		err(1, "Unable to open system.net service");
	capnetdb = cap_service_open(capcas, "system.netdb");
	if (capnetdb == NULL)
		err(1, "Unable to open system.netdb service");
	capsysctl = cap_service_open(capcas, "system.sysctl");
	if (capsysctl == NULL)
		err(1, "Unable to open system.sysctl service");
	cappwd = cap_service_open(capcas, "system.pwd");
	if (cappwd == NULL)
		err(1, "Unable to open system.pwd service");
	cap_close(capcas);
	limit = cap_net_limit_init(capnet, CAPNET_ADDR2NAME);
	if (limit == NULL)
		err(1, "Unable to init cap_net limits");
	if (cap_net_limit(limit) < 0)
		err(1, "Unable to apply limits");
	if (cap_pwd_limit_cmds(cappwd, pwdcmds, nitems(pwdcmds)) < 0)
		err(1, "Unable to apply pwd commands limits");
	if (cap_pwd_limit_fields(cappwd, pwdfields, nitems(pwdfields)) < 0)
		err(1, "Unable to apply pwd commands limits");

	if ((!opt_4 && !opt_6) && protos_defined != -1)
		opt_4 = opt_6 = 1;
	if (!opt_4 && !opt_6 && !opt_u)
		opt_4 = opt_6 = opt_u = 1;
	if ((opt_4 || opt_6) && protos_defined == -1)
		protos_defined = set_default_protos();
	if (!opt_c && !opt_l)
		opt_c = opt_l = 1;

	if (opt_4 || opt_6) {
		for (i = 0; i < protos_defined; i++)
			if (protos[i] == IPPROTO_SCTP)
				gather_sctp();
			else
				gather_inet(protos[i]);
	}

	if (opt_u || (protos_defined == -1 && !opt_4 && !opt_6)) {
		gather_unix(SOCK_STREAM);
		gather_unix(SOCK_DGRAM);
		gather_unix(SOCK_SEQPACKET);
	}
	getfiles();
	display();
	exit(0);
}
