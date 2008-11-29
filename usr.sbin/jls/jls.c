/*-
 * Copyright (c) 2003 Mike Barcroft <mike@FreeBSD.org>
 * Copyright (c) 2008 Bjoern A. Zeeb <bz@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/jail.h>
#include <sys/sysctl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	FLAG_A		0x00001
#define	FLAG_V		0x00002

#ifdef SUPPORT_OLD_XPRISON
static
char *print_xprison_v1(void *p, char *end)
{
	struct xprison_v1 *xp;
	struct in_addr in;

	if ((char *)p + sizeof(struct xprison_v1) > end)
		errx(1, "Invalid length for jail");

	xp = (struct xprison_v1 *)p;
	printf("%6d  %-29.29s %.74s\n",
		xp->pr_id, xp->pr_host, xp->pr_path);

	/* We are not printing an empty line here for state and name. */
	/* We are not printing an empty line here for cpusetid. */

	/* IPv4 address. */
	in.s_addr = htonl(xp->pr_ip);
	printf("%6s  %-15.15s\n", "", inet_ntoa(in));

	return ((char *)(xp + 1));
}
#endif

static
char *print_xprison_v3(void *p, char *end, unsigned flags)
{
	struct xprison *xp;
	struct in_addr *iap, in;
	struct in6_addr *ia6p;
	char buf[INET6_ADDRSTRLEN];
	const char *state;
	char *q;
	uint32_t i;

	if ((char *)p + sizeof(struct xprison) > end)
		errx(1, "Invalid length for jail");
	xp = (struct xprison *)p;

	if (xp->pr_state < 0 || xp->pr_state > (int)
	    ((sizeof(prison_states) / sizeof(struct prison_state))))
		state = "(bogus)";
	else
		state = prison_states[xp->pr_state].state_name;

	/* See if we should print non-ACTIVE jails. No? */
	if ((flags & FLAG_A) == 0 && strcmp(state, "ALIVE")) {
		q = (char *)(xp + 1);
		q += (xp->pr_ip4s * sizeof(struct in_addr));
		if (q > end)
			errx(1, "Invalid length for jail");
		q += (xp->pr_ip6s * sizeof(struct in6_addr));
		if (q > end)
			errx(1, "Invalid length for jail");
		return (q);
	}

	printf("%6d  %-29.29s %.74s\n",
		xp->pr_id, xp->pr_host, xp->pr_path);

	/* Jail state and name. */
	if (flags & FLAG_V)
		printf("%6s  %-29.29s %.74s\n",
		    "", (xp->pr_name != NULL) ? xp->pr_name : "", state);

	/* cpusetid. */
	if (flags & FLAG_V)
		printf("%6s  %-6d\n",
		    "", xp->pr_cpusetid);

	q = (char *)(xp + 1);
	/* IPv4 addresses. */
	iap = (struct in_addr *)(void *)q;
	q += (xp->pr_ip4s * sizeof(struct in_addr));
	if (q > end)
		errx(1, "Invalid length for jail");
	for (i = 0; i < xp->pr_ip4s; i++) {
		if (i == 0 || flags & FLAG_V) {
			in.s_addr = iap[i].s_addr;
			printf("%6s  %-15.15s\n", "", inet_ntoa(in));
		}
	}
	/* IPv6 addresses. */
	ia6p = (struct in6_addr *)(void *)q;
	q += (xp->pr_ip6s * sizeof(struct in6_addr));
	if (q > end)
		errx(1, "Invalid length for jail");
	for (i = 0; i < xp->pr_ip6s; i++) {
		if (flags & FLAG_V) {
			inet_ntop(AF_INET6, &ia6p[i], buf, sizeof(buf));
			printf("%6s  %s\n", "", buf);
		}
	}

	return (q);
}

static void
usage(void)
{

	(void)fprintf(stderr, "usage: jls [-av]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{ 
	int ch, version;
	unsigned flags;
	size_t i, j, len;
	void *p, *q;

	flags = 0;
	while ((ch = getopt(argc, argv, "av")) != -1) {
		switch (ch) {
		case 'a':
			flags |= FLAG_A;
			break;
		case 'v':
			flags |= FLAG_V;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (sysctlbyname("security.jail.list", NULL, &len, NULL, 0) == -1)
		err(1, "sysctlbyname(): security.jail.list");

	j = len;
	for (i = 0; i < 4; i++) {
		if (len <= 0)
			exit(0);	
		p = q = malloc(len);
		if (p == NULL)
			err(1, "malloc()");

		if (sysctlbyname("security.jail.list", q, &len, NULL, 0) == -1) {
			if (errno == ENOMEM) {
				free(p);
				p = NULL;
				len += j;
				continue;
			}
			err(1, "sysctlbyname(): security.jail.list");
		}
		break;
	}
	if (p == NULL)
		err(1, "sysctlbyname(): security.jail.list");
	if (len < sizeof(int))
		errx(1, "This is no prison. Kernel and userland out of sync?");
	version = *(int *)p;
	if (version > XPRISON_VERSION)
		errx(1, "Sci-Fi prison. Kernel/userland out of sync?");

	printf("   JID  Hostname                      Path\n");
	if (flags & FLAG_V) {
		printf("        Name                          State\n");
		printf("        CPUSetID\n");
	}
	printf("        IP Address(es)\n");
	for (; q != NULL && (char *)q + sizeof(int) < (char *)p + len;) {
		version = *(int *)q;
		if (version > XPRISON_VERSION)
			errx(1, "Sci-Fi prison. Kernel/userland out of sync?");
		switch (version) {
#ifdef SUPPORT_OLD_XPRISON
		case 1:
			q = print_xprison_v1(q, (char *)p + len);
			break;
		case 2:
			errx(1, "Version 2 was used by multi-IPv4 jail "
			    "implementations that never made it into the "
			    "official kernel.");
			/* NOTREACHED */
			break;
#endif
		case 3:
			q = print_xprison_v3(q, (char *)p + len, flags);
			break;
		default:
			errx(1, "Prison unknown. Kernel/userland out of sync?");
			/* NOTREACHED */
			break;
		}
	}

	free(p);
	exit(0);
}
