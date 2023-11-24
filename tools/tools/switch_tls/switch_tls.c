/* $OpenBSD: tcpdrop.c,v 1.4 2004/05/22 23:55:22 deraadt Exp $ */

/*-
 * Copyright (c) 2009 Juli Mallett <jmallett@FreeBSD.org>
 * Copyright (c) 2004 Markus Friedl <markus@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_var.h>

#include <err.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	TCPDROP_FOREIGN		0
#define	TCPDROP_LOCAL		1

#define	SW_TLS			0
#define	IFNET_TLS		1

struct host_service {
	char hs_host[NI_MAXHOST];
	char hs_service[NI_MAXSERV];
};

static bool tcpswitch_list_commands = false;

static char *findport(const char *);
static struct xinpgen *getxpcblist(const char *);
static void sockinfo(const struct sockaddr *, struct host_service *);
static bool tcpswitch(const struct sockaddr *, const struct sockaddr *, int);
static bool tcpswitchall(const char *, int);
static bool tcpswitchbyname(const char *, const char *, const char *,
    const char *, int);
static bool tcpswitchconn(const struct in_conninfo *, int);
static void usage(void) __dead2;

/*
 * Switch a tcp connection.
 */
int
main(int argc, char *argv[])
{
	char stack[TCP_FUNCTION_NAME_LEN_MAX];
	char *lport, *fport;
	bool switchall, switchallstack;
	int ch, mode;

	switchall = false;
	switchallstack = false;
	stack[0] = '\0';
	mode = SW_TLS;

	while ((ch = getopt(argc, argv, "ailS:s")) != -1) {
		switch (ch) {
		case 'a':
			switchall = true;
			break;
		case 'i':
			mode = IFNET_TLS;
			break;
		case 'l':
			tcpswitch_list_commands = true;
			break;
		case 'S':
			switchallstack = true;
			strlcpy(stack, optarg, sizeof(stack));
			break;
		case 's':
			mode = SW_TLS;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (switchall && switchallstack)
		usage();
	if (switchall || switchallstack) {
		if (argc != 0)
			usage();
		if (!tcpswitchall(stack, mode))
			exit(1);
		exit(0);
	}

	if ((argc != 2 && argc != 4) || tcpswitch_list_commands)
		usage();

	if (argc == 2) {
		lport = findport(argv[0]);
		fport = findport(argv[1]);
		if (lport == NULL || lport[1] == '\0' || fport == NULL ||
		    fport[1] == '\0')
			usage();
		*lport++ = '\0';
		*fport++ = '\0';
		if (!tcpswitchbyname(argv[0], lport, argv[1], fport, mode))
			exit(1);
	} else if (!tcpswitchbyname(argv[0], argv[1], argv[2], argv[3], mode))
		exit(1);

	exit(0);
}

static char *
findport(const char *arg)
{
	char *dot, *colon;

	/* A strrspn() or strrpbrk() would be nice. */
	dot = strrchr(arg, '.');
	colon = strrchr(arg, ':');
	if (dot == NULL)
		return (colon);
	if (colon == NULL)
		return (dot);
	if (dot < colon)
		return (colon);
	else
		return (dot);
}

static struct xinpgen *
getxpcblist(const char *name)
{
	struct xinpgen *xinp;
	size_t len;
	int rv;

	len = 0;
	rv = sysctlbyname(name, NULL, &len, NULL, 0);
	if (rv == -1)
		err(1, "sysctlbyname %s", name);

	if (len == 0)
		errx(1, "%s is empty", name);

	xinp = malloc(len);
	if (xinp == NULL)
		errx(1, "malloc failed");

	rv = sysctlbyname(name, xinp, &len, NULL, 0);
	if (rv == -1)
		err(1, "sysctlbyname %s", name);

	return (xinp);
}

static void
sockinfo(const struct sockaddr *sa, struct host_service *hs)
{
	static const int flags = NI_NUMERICHOST | NI_NUMERICSERV;
	int rv;

	rv = getnameinfo(sa, sa->sa_len, hs->hs_host, sizeof hs->hs_host,
	    hs->hs_service, sizeof hs->hs_service, flags);
	if (rv == -1)
		err(1, "getnameinfo");
}

static bool
tcpswitch(const struct sockaddr *lsa, const struct sockaddr *fsa, int mode)
{
	struct host_service local, foreign;
	struct sockaddr_storage addrs[2];
	int rv;

	memcpy(&addrs[TCPDROP_FOREIGN], fsa, fsa->sa_len);
	memcpy(&addrs[TCPDROP_LOCAL], lsa, lsa->sa_len);

	sockinfo(lsa, &local);
	sockinfo(fsa, &foreign);

	if (tcpswitch_list_commands) {
		printf("switch_tls %s %s %s %s %s\n",
		    mode == SW_TLS ? "-s" : "-i",
		    local.hs_host, local.hs_service,
		    foreign.hs_host, foreign.hs_service);
		return (true);
	}

	rv = sysctlbyname(mode == SW_TLS ? "net.inet.tcp.switch_to_sw_tls" :
	    "net.inet.tcp.switch_to_ifnet_tls", NULL, NULL, &addrs,
	    sizeof addrs);
	if (rv == -1) {
		warn("%s %s %s %s", local.hs_host, local.hs_service,
		    foreign.hs_host, foreign.hs_service);
		return (false);
	}
	printf("%s %s %s %s: switched\n", local.hs_host, local.hs_service,
	    foreign.hs_host, foreign.hs_service);
	return (true);
}

static bool
tcpswitchall(const char *stack, int mode)
{
	struct xinpgen *head, *xinp;
	struct xtcpcb *xtp;
	struct xinpcb *xip;
	bool ok;

	ok = true;

	head = getxpcblist("net.inet.tcp.pcblist");

#define	XINP_NEXT(xinp)							\
	((struct xinpgen *)(uintptr_t)((uintptr_t)(xinp) + (xinp)->xig_len))

	for (xinp = XINP_NEXT(head); xinp->xig_len > sizeof *xinp;
	    xinp = XINP_NEXT(xinp)) {
		xtp = (struct xtcpcb *)xinp;
		xip = &xtp->xt_inp;

		/*
		 * XXX
		 * Check protocol, support just v4 or v6, etc.
		 */

		/* Ignore PCBs which were freed during copyout.  */
		if (xip->inp_gencnt > head->xig_gen)
			continue;

		/* Skip listening sockets.  */
		if (xtp->t_state == TCPS_LISTEN)
			continue;

		/* If requested, skip sockets not having the requested stack. */
		if (stack[0] != '\0' &&
		    strncmp(xtp->xt_stack, stack, TCP_FUNCTION_NAME_LEN_MAX))
			continue;

		if (!tcpswitchconn(&xip->inp_inc, mode))
			ok = false;
	}
	free(head);

	return (ok);
}

static bool
tcpswitchbyname(const char *lhost, const char *lport, const char *fhost,
    const char *fport, int mode)
{
	static const struct addrinfo hints = {
		/*
		 * Look for streams in all domains.
		 */
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};
	struct addrinfo *ail, *local, *aif, *foreign;
	int error;
	bool ok, infamily;

	error = getaddrinfo(lhost, lport, &hints, &local);
	if (error != 0)
		errx(1, "getaddrinfo: %s port %s: %s", lhost, lport,
		    gai_strerror(error));

	error = getaddrinfo(fhost, fport, &hints, &foreign);
	if (error != 0) {
		freeaddrinfo(local); /* XXX gratuitous */
		errx(1, "getaddrinfo: %s port %s: %s", fhost, fport,
		    gai_strerror(error));
	}

	ok = true;
	infamily = false;

	/*
	 * Try every combination of local and foreign address pairs.
	 */
	for (ail = local; ail != NULL; ail = ail->ai_next) {
		for (aif = foreign; aif != NULL; aif = aif->ai_next) {
			if (ail->ai_family != aif->ai_family)
				continue;
			infamily = true;
			if (!tcpswitch(ail->ai_addr, aif->ai_addr, mode))
				ok = false;
		}
	}

	if (!infamily) {
		warnx("%s %s %s %s: different address families", lhost, lport,
		    fhost, fport);
		ok = false;
	}

	freeaddrinfo(local);
	freeaddrinfo(foreign);

	return (ok);
}

static bool
tcpswitchconn(const struct in_conninfo *inc, int mode)
{
	struct sockaddr *local, *foreign;
	struct sockaddr_in6 sin6[2];
	struct sockaddr_in sin4[2];

	if ((inc->inc_flags & INC_ISIPV6) != 0) {
		memset(sin6, 0, sizeof sin6);

		sin6[TCPDROP_LOCAL].sin6_len = sizeof sin6[TCPDROP_LOCAL];
		sin6[TCPDROP_LOCAL].sin6_family = AF_INET6;
		sin6[TCPDROP_LOCAL].sin6_port = inc->inc_lport;
		memcpy(&sin6[TCPDROP_LOCAL].sin6_addr, &inc->inc6_laddr,
		    sizeof inc->inc6_laddr);
		local = (struct sockaddr *)&sin6[TCPDROP_LOCAL];

		sin6[TCPDROP_FOREIGN].sin6_len = sizeof sin6[TCPDROP_FOREIGN];
		sin6[TCPDROP_FOREIGN].sin6_family = AF_INET6;
		sin6[TCPDROP_FOREIGN].sin6_port = inc->inc_fport;
		memcpy(&sin6[TCPDROP_FOREIGN].sin6_addr, &inc->inc6_faddr,
		    sizeof inc->inc6_faddr);
		foreign = (struct sockaddr *)&sin6[TCPDROP_FOREIGN];
	} else {
		memset(sin4, 0, sizeof sin4);

		sin4[TCPDROP_LOCAL].sin_len = sizeof sin4[TCPDROP_LOCAL];
		sin4[TCPDROP_LOCAL].sin_family = AF_INET;
		sin4[TCPDROP_LOCAL].sin_port = inc->inc_lport;
		memcpy(&sin4[TCPDROP_LOCAL].sin_addr, &inc->inc_laddr,
		    sizeof inc->inc_laddr);
		local = (struct sockaddr *)&sin4[TCPDROP_LOCAL];

		sin4[TCPDROP_FOREIGN].sin_len = sizeof sin4[TCPDROP_FOREIGN];
		sin4[TCPDROP_FOREIGN].sin_family = AF_INET;
		sin4[TCPDROP_FOREIGN].sin_port = inc->inc_fport;
		memcpy(&sin4[TCPDROP_FOREIGN].sin_addr, &inc->inc_faddr,
		    sizeof inc->inc_faddr);
		foreign = (struct sockaddr *)&sin4[TCPDROP_FOREIGN];
	}

	return (tcpswitch(local, foreign, mode));
}

static void
usage(void)
{
	fprintf(stderr,
"usage: switch_tls [-i | -s] local-address local-port foreign-address foreign-port\n"
"       switch_tls [-i | -s] local-address:local-port foreign-address:foreign-port\n"
"       switch_tls [-i | -s] local-address.local-port foreign-address.foreign-port\n"
"       switch_tls [-l | -i | -s] -a\n"
"       switch_tls [-l | -i | -s] -S stack\n");
	exit(1);
}
