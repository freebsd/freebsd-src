/*-
 * Copyright (c) 2002 Dag-Erling Coïdan Smørgrav
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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/file.h>
#include <sys/user.h>

#include <sys/un.h>
#include <sys/unpcb.h>

#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_var.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int	 opt_4;		/* Show IPv4 sockets */
static int	 opt_6;		/* Show IPv6 sockets */
static int	 opt_c;		/* Show connected sockets */
static int	 opt_l;		/* Show listening sockets */
static int	 opt_u;		/* Show Unix domain sockets */
static int	 opt_v;		/* Verbose mode */

static int	*ports;

#define INT_BIT (sizeof(int)*CHAR_BIT)
#define SET_PORT(p) do { ports[p / INT_BIT] |= 1 << (p % INT_BIT); } while (0)
#define CHK_PORT(p) (ports[p / INT_BIT] & (1 << (p % INT_BIT)))

struct sock {
	void *socket;
	void *pcb;
	int vflag;
	int family;
	int proto;
	const char *protoname;
	struct sockaddr_storage laddr;
	struct sockaddr_storage faddr;
	struct sock *next;
};

#define HASHSIZE 1009
static struct sock *sockhash[HASHSIZE];

static struct xfile *xfiles;
static int nxfiles;

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
sockaddr(struct sockaddr_storage *sa, int af, void *addr, int port)
{
	struct sockaddr_in *sin4;
	struct sockaddr_in6 *sin6;

	bzero(sa, sizeof *sa);
	switch (af) {
	case AF_INET:
		sin4 = (struct sockaddr_in *)sa;
		sin4->sin_len = sizeof *sin4;
		sin4->sin_family = af;
		sin4->sin_port = port;
		sin4->sin_addr = *(struct in_addr *)addr;
		break;
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)sa;
		sin6->sin6_len = sizeof *sin6;
		sin6->sin6_family = af;
		sin6->sin6_port = port;
		sin6->sin6_addr = *(struct in6_addr *)addr;
		break;
	default:
		abort();
	}
}

static void
gather_inet(int proto)
{
	struct xinpgen *xig, *exig;
	struct xinpcb *xip;
	struct xtcpcb *xtp;
	struct inpcb *inp;
	struct xsocket *so;
	struct sock *sock;
	const char *varname, *protoname;
	size_t len, bufsize;
	void *buf;
	int hash, retry, vflag;

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
			if (sysctlbyname(varname, buf, &len, NULL, 0) == 0)
				break;
			if (errno == ENOENT)
				goto out;
			if (errno != ENOMEM)
				err(1, "sysctlbyname()");
			bufsize *= 2;
		}
		xig = (struct xinpgen *)buf;
		exig = (struct xinpgen *)(void *)
		    ((char *)buf + len - sizeof *exig);
		if (xig->xig_len != sizeof *xig ||
		    exig->xig_len != sizeof *exig)
			errx(1, "struct xinpgen size mismatch");
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
			if (xtp->xt_len != sizeof *xtp) {
				warnx("struct xtcpcb size mismatch");
				goto out;
			}
			inp = &xtp->xt_inp;
			so = &xtp->xt_socket;
			break;
		case IPPROTO_UDP:
		case IPPROTO_DIVERT:
			xip = (struct xinpcb *)xig;
			if (xip->xi_len != sizeof *xip) {
				warnx("struct xinpcb size mismatch");
				goto out;
			}
			inp = &xip->xi_inp;
			so = &xip->xi_socket;
			break;
		default:
			abort();
		}
		if ((inp->inp_vflag & vflag) == 0)
			continue;
		if (inp->inp_vflag & INP_IPV4) {
			if ((inp->inp_fport == 0 && !opt_l) ||
			    (inp->inp_fport != 0 && !opt_c))
				continue;
		} else if (inp->inp_vflag & INP_IPV6) {
			if ((inp->in6p_fport == 0 && !opt_l) ||
			    (inp->in6p_fport != 0 && !opt_c))
				continue;
		} else {
			if (opt_v)
				warnx("invalid vflag 0x%x", inp->inp_vflag);
			free(sock);
			continue;
		}
		if ((sock = calloc(1, sizeof *sock)) == NULL)
			err(1, "malloc()");
		sock->socket = so->xso_so;
		sock->proto = proto;
		if (inp->inp_vflag & INP_IPV4) {
			sock->family = AF_INET;
			sockaddr(&sock->laddr, sock->family,
			    &inp->inp_laddr, inp->inp_lport);
			sockaddr(&sock->faddr, sock->family,
			    &inp->inp_faddr, inp->inp_fport);
		} else if (inp->inp_vflag & INP_IPV6) {
			sock->family = AF_INET6;
			sockaddr(&sock->laddr, sock->family,
			    &inp->in6p_laddr, inp->in6p_lport);
			sockaddr(&sock->faddr, sock->family,
			    &inp->in6p_faddr, inp->in6p_fport);
		}
		sock->vflag = inp->inp_vflag;
		sock->protoname = protoname;
		hash = (int)((uintptr_t)sock->socket % HASHSIZE);
		sock->next = sockhash[hash];
		sockhash[hash] = sock;
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
	const char *varname, *protoname;
	size_t len, bufsize;
	void *buf;
	int hash, retry;

	switch (proto) {
	case SOCK_STREAM:
		varname = "net.local.stream.pcblist";
		protoname = "stream";
		break;
	case SOCK_DGRAM:
		varname = "net.local.dgram.pcblist";
		protoname = "dgram";
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
			if (sysctlbyname(varname, buf, &len, NULL, 0) == 0)
				break;
			if (errno != ENOMEM)
				err(1, "sysctlbyname()");
			bufsize *= 2;
		}
		xug = (struct xunpgen *)buf;
		exug = (struct xunpgen *)(void *)
		    ((char *)buf + len - sizeof *exug);
		if (xug->xug_len != sizeof *xug ||
		    exug->xug_len != sizeof *exug) {
			warnx("struct xinpgen size mismatch");
			goto out;
		}
	} while (xug->xug_gen != exug->xug_gen && retry--);

	if (xug->xug_gen != exug->xug_gen && opt_v)
		warnx("warning: data may be inconsistent");

	for (;;) {
		xug = (struct xunpgen *)(void *)((char *)xug + xug->xug_len);
		if (xug >= exug)
			break;
		xup = (struct xunpcb *)xug;
		if (xup->xu_len != sizeof *xup) {
			warnx("struct xunpcb size mismatch");
			goto out;
		}
		if ((xup->xu_unp.unp_conn == NULL && !opt_l) ||
		    (xup->xu_unp.unp_conn != NULL && !opt_c))
			continue;
		if ((sock = calloc(1, sizeof *sock)) == NULL)
			err(1, "malloc()");
		sock->socket = xup->xu_socket.xso_so;
		sock->pcb = xup->xu_unpp;
		sock->proto = proto;
		sock->family = AF_UNIX;
		sock->protoname = protoname;
		if (xup->xu_unp.unp_addr != NULL)
			sock->laddr =
			    *(struct sockaddr_storage *)(void *)&xup->xu_addr;
		else if (xup->xu_unp.unp_conn != NULL)
			*(void **)&sock->faddr = xup->xu_unp.unp_conn;
		hash = (int)((uintptr_t)sock->socket % HASHSIZE);
		sock->next = sockhash[hash];
		sockhash[hash] = sock;
	}
out:
	free(buf);
}

static void
getfiles(void)
{
	size_t len;

	if ((xfiles = malloc(len = sizeof *xfiles)) == NULL)
		err(1, "malloc()");
	while (sysctlbyname("kern.file", xfiles, &len, 0, 0) == -1) {
		if (errno != ENOMEM)
			err(1, "sysctlbyname()");
		len *= 2;
		if ((xfiles = realloc(xfiles, len)) == NULL)
			err(1, "realloc()");
	}
	if (len > 0 && xfiles->xf_size != sizeof *xfiles)
		errx(1, "struct xfile size mismatch");
	nxfiles = len / sizeof *xfiles;
}

static int
printaddr(int af, struct sockaddr_storage *ss)
{
	char addrstr[INET6_ADDRSTRLEN] = { '\0', '\0' };
	struct sockaddr_un *sun;
	void *addr;
	int off, port;

	switch (af) {
	case AF_INET:
		addr = &((struct sockaddr_in *)ss)->sin_addr;
		if (inet_lnaof(*(struct in_addr *)addr) == INADDR_ANY)
			addrstr[0] = '*';
		port = ntohs(((struct sockaddr_in *)ss)->sin_port);
		break;
	case AF_INET6:
		addr = &((struct sockaddr_in6 *)ss)->sin6_addr;
		if (IN6_IS_ADDR_UNSPECIFIED((struct in6_addr *)addr))
			addrstr[0] = '*';
		port = ntohs(((struct sockaddr_in6 *)ss)->sin6_port);
		break;
	case AF_UNIX:
		sun = (struct sockaddr_un *)ss;
		off = (int)((char *)&sun->sun_path - (char *)sun);
		return (xprintf("%.*s", sun->sun_len - off, sun->sun_path));
	}
	if (addrstr[0] == '\0')
		inet_ntop(af, addr, addrstr, sizeof addrstr);
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
	len = sizeof proc;
	if (sysctl(mib, 4, &proc, &len, NULL, 0) == -1) {
		warn("sysctl()");
		return ("??");
	}
	return (proc.ki_ocomm);
}

static int
check_ports(struct sock *s)
{
	int port;

	if (ports == NULL)
		return (1);
	if ((s->family != AF_INET) && (s->family != AF_INET6))
		return (1);
	if (s->family == AF_INET)
		port = ntohs(((struct sockaddr_in *)(&s->laddr))->sin_port);
	else
		port = ntohs(((struct sockaddr_in6 *)(&s->laddr))->sin6_port);
	if (CHK_PORT(port))
		return (1);
	if (s->family == AF_INET)
		port = ntohs(((struct sockaddr_in *)(&s->faddr))->sin_port);
	else
		port = ntohs(((struct sockaddr_in6 *)(&s->faddr))->sin6_port);
	if (CHK_PORT(port))
		return (1);
	return (0);
}

static void
display(void)
{
	struct passwd *pwd;
	struct xfile *xf;
	struct sock *s;
	void *p;
	int hash, n, pos;

	printf("%-8s %-10s %-5s %-2s %-6s %-21s %-21s\n",
	    "USER", "COMMAND", "PID", "FD", "PROTO",
	    "LOCAL ADDRESS", "FOREIGN ADDRESS");
	setpassent(1);
	for (xf = xfiles, n = 0; n < nxfiles; ++n, ++xf) {
		if (xf->xf_data == NULL)
			continue;
		hash = (int)((uintptr_t)xf->xf_data % HASHSIZE);
		for (s = sockhash[hash]; s != NULL; s = s->next)
			if ((void *)s->socket == xf->xf_data)
				break;
		if (s == NULL)
			continue;
		if (!check_ports(s))
			continue;
		pos = 0;
		if ((pwd = getpwuid(xf->xf_uid)) == NULL)
			pos += xprintf("%lu", (u_long)xf->xf_uid);
		else
			pos += xprintf("%s", pwd->pw_name);
		while (pos < 9)
			pos += xprintf(" ");
		pos += xprintf("%.10s", getprocname(xf->xf_pid));
		while (pos < 20)
			pos += xprintf(" ");
		pos += xprintf("%lu", (u_long)xf->xf_pid);
		while (pos < 26)
			pos += xprintf(" ");
		pos += xprintf("%d", xf->xf_fd);
		while (pos < 29)
			pos += xprintf(" ");
		pos += xprintf("%s", s->protoname);
		if (s->vflag & INP_IPV4)
			pos += xprintf("4");
		if (s->vflag & INP_IPV6)
			pos += xprintf("6");
		while (pos < 36)
			pos += xprintf(" ");
		switch (s->family) {
		case AF_INET:
		case AF_INET6:
			pos += printaddr(s->family, &s->laddr);
			while (pos < 58)
				pos += xprintf(" ");
			pos += printaddr(s->family, &s->faddr);
			break;
		case AF_UNIX:
			/* server */
			if (s->laddr.ss_len > 0) {
				pos += printaddr(s->family, &s->laddr);
				break;
			}
			/* client */
			p = *(void **)&s->faddr;
			if (p == NULL) {
				pos += xprintf("(not connected)");
				break;
			}
			pos += xprintf("-> ");
			for (hash = 0; hash < HASHSIZE; ++hash) {
				for (s = sockhash[hash]; s != NULL; s = s->next)
					if (s->pcb == p)
						break;
				if (s != NULL)
					break;
			}
			if (s == NULL || s->laddr.ss_len == 0)
				pos += xprintf("??");
			else
				pos += printaddr(s->family, &s->laddr);
			break;
		default:
			abort();
		}
		xprintf("\n");
	}
}

static void
usage(void)
{
	fprintf(stderr, "Usage: sockstat [-46clu] [-p ports]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	int o;

	while ((o = getopt(argc, argv, "46clp:uv")) != -1)
		switch (o) {
		case '4':
			opt_4 = 1;
			break;
		case '6':
			opt_6 = 1;
			break;
		case 'c':
			opt_c = 1;
			break;
		case 'l':
			opt_l = 1;
			break;
		case 'p':
			parse_ports(optarg);
			break;
		case 'u':
			opt_u = 1;
			break;
		case 'v':
			++opt_v;
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc > 0)
		usage();

	if (!opt_4 && !opt_6 && !opt_u)
		opt_4 = opt_6 = opt_u = 1;
	if (!opt_c && !opt_l)
		opt_c = opt_l = 1;

	if (opt_4 || opt_6) {
		gather_inet(IPPROTO_TCP);
		gather_inet(IPPROTO_UDP);
		gather_inet(IPPROTO_DIVERT);
	}
	if (opt_u) {
		gather_unix(SOCK_STREAM);
		gather_unix(SOCK_DGRAM);
	}
	getfiles();
	display();

	exit(0);
}
