/*	$KAME: ndp.c,v 1.104 2003/06/27 07:48:39 itojun Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 1995, 1996, 1997, 1998, and 1999 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Copyright (c) 1984, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Sun Microsystems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Based on:
 * "@(#) Copyright (c) 1984, 1993\n\
 *	The Regents of the University of California.  All rights reserved.\n";
 *
 * "@(#)arp.c	8.2 (Berkeley) 1/2/94";
 */

/*
 * ndp - display, set, delete and flush neighbor cache
 */


#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <netinet/icmp6.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>

#include <arpa/inet.h>

#include <assert.h>
#include <ctype.h>
#include <netdb.h>
#include <errno.h>
#include <nlist.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <paths.h>
#include <err.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <libxo/xo.h>
#include <time.h>

#include "ndp.h"

#define	NEXTADDR(w, s)					\
	if (rtm->rtm_addrs & (w)) {			\
		bcopy((char *)&s, cp, sizeof(s));	\
		cp += SA_SIZE(&s);			\
	}

static pid_t pid;
static int32_t thiszone;	/* time difference with gmt */
static int s = -1;
static int repeat = 0;

static char host_buf[NI_MAXHOST];	/* getnameinfo() */
static char ifix_buf[IFNAMSIZ];		/* if_indextoname() */

static int file(char *);
static int set(int, char **);
static void get(char *);
static int delete(char *);
static int dump(struct sockaddr_in6 *, int);
static struct in6_nbrinfo *getnbrinfo(struct in6_addr *, int, int);
static int ndp_ether_aton(char *, u_char *);
static void usage(void) __dead2;
static void ifinfo(char *, int, char **);
static void rtrlist(void);
static void plist(void);
static void pfx_flush(void);
static void rtr_flush(void);
static void harmonize_rtr(void);
#ifdef SIOCSDEFIFACE_IN6	/* XXX: check SIOCGDEFIFACE_IN6 as well? */
static void getdefif(void);
static void setdefif(char *);
#endif

#ifdef WITHOUT_NETLINK
static void getsocket(void);
static int rtmsg(int);
#endif

static const char *rtpref_str[] = {
	"medium",		/* 00 */
	"high",			/* 01 */
	"rsv",			/* 10 */
	"low"			/* 11 */
};

struct ndp_opts opts = {};

#define NDP_XO_VERSION	"1"

bool
valid_type(int if_type)
{
	switch (if_type) {
	case IFT_ETHER:
	case IFT_FDDI:
	case IFT_ISO88023:
	case IFT_ISO88024:
	case IFT_ISO88025:
	case IFT_L2VLAN:
	case IFT_BRIDGE:
		return (true);
		break;
	}
	return (false);
}

static int32_t
utc_offset(void)
{
	time_t t;
	struct tm *tm;

	t = time(NULL);
	tm = localtime(&t);

	assert(tm->tm_gmtoff > INT32_MIN && tm->tm_gmtoff < INT32_MAX);

	return (tm->tm_gmtoff);
}

int
main(int argc, char **argv)
{
	int ch, mode = 0;
	char *arg = NULL;

	pid = getpid();
	thiszone = utc_offset();

	argc = xo_parse_args(argc, argv);
	if (argc < 0)
		exit(1);
	xo_set_version(NDP_XO_VERSION);
	xo_open_container("ndp");

	while ((ch = getopt(argc, argv, "acd:f:Ii:nprstA:HPR")) != -1)
		switch (ch) {
		case 'a':
		case 'c':
		case 'p':
		case 'r':
		case 'H':
		case 'P':
		case 'R':
		case 's':
		case 'I':
			if (mode) {
				usage();
				/*NOTREACHED*/
			}
			mode = ch;
			arg = NULL;
			break;
		case 'f':
			exit(file(optarg) ? 1 : 0);
		case 'd':
		case 'i':
			if (mode) {
				usage();
				/*NOTREACHED*/
			}
			mode = ch;
			arg = optarg;
			break;
		case 'n':
			opts.nflag = true;
			break;
		case 't':
			opts.tflag = true;
			break;
		case 'A':
			if (mode) {
				usage();
				/*NOTREACHED*/
			}
			mode = 'a';
			repeat = atoi(optarg);
			if (repeat < 0) {
				usage();
				/*NOTREACHED*/
			}
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	switch (mode) {
	case 'a':
	case 'c':
		if (argc != 0) {
			usage();
			/*NOTREACHED*/
		}
		dump(0, mode == 'c');
		break;
	case 'd':
		if (argc != 0) {
			usage();
			/*NOTREACHED*/
		}
		xo_open_list("neighbor-cache");
		delete(arg);
		xo_close_list("neighbor-cache");
		break;
	case 'I':
#ifdef SIOCSDEFIFACE_IN6	/* XXX: check SIOCGDEFIFACE_IN6 as well? */
		if (argc > 1) {
			usage();
			/*NOTREACHED*/
		} else if (argc == 1) {
			if (strcmp(*argv, "delete") == 0 ||
			    if_nametoindex(*argv))
				setdefif(*argv);
			else
				xo_errx(1, "invalid interface %s", *argv);
		}
		getdefif(); /* always call it to print the result */
		break;
#else
		xo_errx(1, "not supported yet");
		/*NOTREACHED*/
#endif
	case 'p':
		if (argc != 0) {
			usage();
			/*NOTREACHED*/
		}
		plist();
		break;
	case 'i':
		ifinfo(arg, argc, argv);
		break;
	case 'r':
		if (argc != 0) {
			usage();
			/*NOTREACHED*/
		}
		rtrlist();
		break;
	case 's':
		if (argc < 2 || argc > 4)
			usage();
		exit(set(argc, argv) ? 1 : 0);
	case 'H':
		if (argc != 0) {
			usage();
			/*NOTREACHED*/
		}
		harmonize_rtr();
		break;
	case 'P':
		if (argc != 0) {
			usage();
			/*NOTREACHED*/
		}
		pfx_flush();
		break;
	case 'R':
		if (argc != 0) {
			usage();
			/*NOTREACHED*/
		}
		rtr_flush();
		break;
	case 0:
		if (argc != 1) {
			usage();
			/*NOTREACHED*/
		}
		get(argv[0]);
		break;
	}
	xo_close_container("ndp");
	xo_finish();
	exit(0);
}

/*
 * Process a file to set standard ndp entries
 */
static int
file(char *name)
{
	FILE *fp;
	int i, retval;
	char line[100], arg[5][50], *args[5], *p;

	if ((fp = fopen(name, "r")) == NULL)
		xo_err(1, "cannot open %s", name);
	args[0] = &arg[0][0];
	args[1] = &arg[1][0];
	args[2] = &arg[2][0];
	args[3] = &arg[3][0];
	args[4] = &arg[4][0];
	retval = 0;
	while (fgets(line, sizeof(line), fp) != NULL) {
		if ((p = strchr(line, '#')) != NULL)
			*p = '\0';
		for (p = line; isblank(*p); p++);
		if (*p == '\n' || *p == '\0')
			continue;
		i = sscanf(line, "%49s %49s %49s %49s %49s",
		    arg[0], arg[1], arg[2], arg[3], arg[4]);
		if (i < 2) {
			xo_warnx("bad line: %s", line);
			retval = 1;
			continue;
		}
		if (set(i, args))
			retval = 1;
	}
	fclose(fp);
	return (retval);
}

static void
getsocket(void)
{
	if (s < 0) {
		s = socket(PF_ROUTE, SOCK_RAW, 0);
		if (s < 0) {
			xo_err(1, "socket");
			/* NOTREACHED */
		}
	}
}

static struct sockaddr_in6 so_mask = {
	.sin6_len = sizeof(so_mask),
	.sin6_family = AF_INET6
};
static struct sockaddr_in6 blank_sin = {
	.sin6_len = sizeof(blank_sin),
	.sin6_family = AF_INET6
};
static struct sockaddr_in6 sin_m;
static struct sockaddr_dl blank_sdl = {
	.sdl_len = sizeof(blank_sdl),
	.sdl_family = AF_LINK
};
static struct sockaddr_dl sdl_m;
#ifdef WITHOUT_NETLINK
static struct {
	struct	rt_msghdr m_rtm;
	char	m_space[512];
} m_rtmsg;
#endif

/*
 * Set an individual neighbor cache entry
 */
static int
set(int argc, char **argv)
{
	struct sockaddr_in6 *sin = &sin_m;
	int gai_error;
	u_char *ea;
	char *host = argv[0], *eaddr = argv[1];

	argc -= 2;
	argv += 2;
	sdl_m = blank_sdl;
	sin_m = blank_sin;

	gai_error = getaddr(host, sin);
	if (gai_error) {
		xo_warnx("%s: %s", host, gai_strerror(gai_error));
		return 1;
	}

	ea = (u_char *)LLADDR(&sdl_m);
	if (ndp_ether_aton(eaddr, ea) == 0)
		sdl_m.sdl_alen = 6;
	while (argc-- > 0) {
		if (strncmp(argv[0], "temp", 4) == 0) {
			struct timeval now;

			gettimeofday(&now, 0);
			opts.expire_time = now.tv_sec + 20 * 60;
		} else if (strncmp(argv[0], "proxy", 5) == 0)
			opts.flags |= RTF_ANNOUNCE;
		argv++;
	}

#ifndef WITHOUT_NETLINK
	return (set_nl(0, sin, &sdl_m, host));
#else
	struct rt_msghdr *rtm = &(m_rtmsg.m_rtm);
	struct sockaddr_dl *sdl;

	getsocket();

	if (rtmsg(RTM_GET) < 0) {
		xo_errx(1, "RTM_GET(%s) failed", host);
		/* NOTREACHED */
	}
	sin = (struct sockaddr_in6 *)(rtm + 1);
	sdl = (struct sockaddr_dl *)(ALIGN(sin->sin6_len) + (char *)sin);
	if (IN6_ARE_ADDR_EQUAL(&sin->sin6_addr, &sin_m.sin6_addr)) {
		if (sdl->sdl_family == AF_LINK &&
		    !(rtm->rtm_flags & RTF_GATEWAY)) {
			if (valid_type(sdl->sdl_type))
				goto overwrite;
		}
		xo_warnx("cannot configure a new entry");
		return 1;
	}

overwrite:
	if (sdl->sdl_family != AF_LINK) {
		xo_warnx("cannot intuit interface index and type for %s", host);
		return (1);
	}
	sdl_m.sdl_type = sdl->sdl_type;
	sdl_m.sdl_index = sdl->sdl_index;
	return (rtmsg(RTM_ADD));
#endif
}

int
getaddr(char *host, struct sockaddr_in6 *sin6)
{
	struct addrinfo hints = { .ai_family = AF_INET6 };
	struct addrinfo *res;

	int gai_error = getaddrinfo(host, NULL, &hints, &res);
	if (gai_error != 0)
		return (gai_error);
	sin6->sin6_family = AF_INET6;
	sin6->sin6_len = sizeof(*sin6);
	sin6->sin6_addr = ((struct sockaddr_in6 *)res->ai_addr)->sin6_addr;
	sin6->sin6_scope_id =
	    ((struct sockaddr_in6 *)res->ai_addr)->sin6_scope_id;
	return (0);
}

/*
 * Display an individual neighbor cache entry
 */
static void
get(char *host)
{
	struct sockaddr_in6 *sin = &sin_m;
	int gai_error;

	sin_m = blank_sin;

	gai_error = getaddr(host, sin);
	if (gai_error) {
		xo_warnx("%s: %s", host, gai_strerror(gai_error));
		return;
	}
	if (dump(sin, 0) == 0) {
		getnameinfo((struct sockaddr *)sin, sin->sin6_len, host_buf,
		    sizeof(host_buf), NULL ,0,
		    (opts.nflag ? NI_NUMERICHOST : 0));
		xo_errx(1, "%s (%s) -- no entry", host, host_buf);
	}
}

#ifdef WITHOUT_NETLINK
/*
 * Delete a neighbor cache entry
 */
static int
delete_rtsock(char *host)
{
	struct sockaddr_in6 *sin = &sin_m;
	register struct rt_msghdr *rtm = &m_rtmsg.m_rtm;
	register char *cp = m_rtmsg.m_space;
	struct sockaddr_dl *sdl;
	int gai_error;

	getsocket();
	sin_m = blank_sin;

	gai_error = getaddr(host, sin);
	if (gai_error) {
		xo_warnx("%s: %s", host, gai_strerror(gai_error));
		return 1;
	}

	if (rtmsg(RTM_GET) < 0) {
		xo_errx(1, "RTM_GET(%s) failed", host);
		/* NOTREACHED */
	}
	sin = (struct sockaddr_in6 *)(rtm + 1);
	sdl = (struct sockaddr_dl *)(ALIGN(sin->sin6_len) + (char *)sin);
	if (IN6_ARE_ADDR_EQUAL(&sin->sin6_addr, &sin_m.sin6_addr)) {
		if (sdl->sdl_family == AF_LINK &&
		    !(rtm->rtm_flags & RTF_GATEWAY)) {
			goto delete;
		}
		xo_warnx("delete: cannot delete non-NDP entry");
		return 1;
	}

delete:
	if (sdl->sdl_family != AF_LINK) {
		xo_warnx("cannot locate %s", host);
		return (1);
	}
	/*
	 * need to reinit the field because it has rt_key
	 * but we want the actual address
	 */
	NEXTADDR(RTA_DST, sin_m);
	rtm->rtm_flags |= RTF_LLDATA;
	if (rtmsg(RTM_DELETE) == 0) {
		getnameinfo((struct sockaddr *)sin,
		    sin->sin6_len, host_buf,
		    sizeof(host_buf), NULL, 0,
		    (opts.nflag ? NI_NUMERICHOST : 0));
		xo_open_instance("neighbor-cache");

		char *ifname = if_indextoname(sdl->sdl_index, ifix_buf);
		if (ifname == NULL) {
			strlcpy(ifix_buf, "?", sizeof(ifix_buf));
			ifname = ifix_buf;
		}
		char abuf[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, &sin->sin6_addr, abuf, sizeof(abuf));

		xo_emit("{:hostname/%s}{d:/ (%s) deleted\n}", host, host_buf);
		xo_emit("{e:address/%s}{e:interface/%s}", abuf, ifname);
		xo_close_instance("neighbor-cache");
	}

	return 0;
}

/*
 * Dump the entire neighbor cache
 */
static int
dump_rtsock(struct sockaddr_in6 *addr, int cflag)
{
	int mib[6];
	size_t needed;
	char *lim, *buf, *next;
	struct rt_msghdr *rtm;
	struct sockaddr_in6 *sin;
	struct sockaddr_dl *sdl;
	struct timeval now;
	time_t expire;
	int addrwidth;
	int llwidth;
	int ifwidth;
	char flgbuf[8];
	char *ifname;

	/* Print header */
	if (!opts.tflag && !cflag) {
		char xobuf[200];
		snprintf(xobuf, sizeof(xobuf),
		    "{T:/%%-%d.%ds} {T:/%%-%d.%ds} {T:/%%%d.%ds} {T:/%%-9.9s} {T:%%1s} {T:%%5s}\n",
		    W_ADDR, W_ADDR, W_LL, W_LL, W_IF, W_IF);
		xo_emit(xobuf, "Neighbor", "Linklayer Address", "Netif", "Expire", "S", "Flags");
	}
	xo_open_list("neighbor-cache");
again:;
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET6;
	mib[4] = NET_RT_FLAGS;
#ifdef RTF_LLINFO
	mib[5] = RTF_LLINFO;
#else
	mib[5] = 0;
#endif
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		xo_err(1, "sysctl(PF_ROUTE estimate)");
	if (needed > 0) {
		if ((buf = malloc(needed)) == NULL)
			xo_err(1, "malloc");
		if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
			xo_err(1, "sysctl(PF_ROUTE, NET_RT_FLAGS)");
		lim = buf + needed;
	} else
		buf = lim = NULL;

	int count = 0;
	for (next = buf; next && next < lim; next += rtm->rtm_msglen) {
		int isrouter = 0, prbs = 0;

		rtm = (struct rt_msghdr *)next;
		sin = (struct sockaddr_in6 *)(rtm + 1);
		sdl = (struct sockaddr_dl *)((char *)sin +
		    ALIGN(sin->sin6_len));

		/*
		 * Some OSes can produce a route that has the LINK flag but
		 * has a non-AF_LINK gateway (e.g. fe80::xx%lo0 on FreeBSD
		 * and BSD/OS, where xx is not the interface identifier on
		 * lo0).  Such routes entry would annoy getnbrinfo() below,
		 * so we skip them.
		 * XXX: such routes should have the GATEWAY flag, not the
		 * LINK flag.  However, there is rotten routing software
		 * that advertises all routes that have the GATEWAY flag.
		 * Thus, KAME kernel intentionally does not set the LINK flag.
		 * What is to be fixed is not ndp, but such routing software
		 * (and the kernel workaround)...
		 */
		if (sdl->sdl_family != AF_LINK)
			continue;

		if (!(rtm->rtm_flags & RTF_HOST))
			continue;

		if (addr) {
			if (IN6_ARE_ADDR_EQUAL(&addr->sin6_addr,
			    &sin->sin6_addr) == 0 ||
			    addr->sin6_scope_id != sin->sin6_scope_id)
				continue;
		} else if (IN6_IS_ADDR_MULTICAST(&sin->sin6_addr))
			continue;
		count++;
		if (IN6_IS_ADDR_LINKLOCAL(&sin->sin6_addr) ||
		    IN6_IS_ADDR_MC_LINKLOCAL(&sin->sin6_addr)) {
			/* XXX: should scope id be filled in the kernel? */
			if (sin->sin6_scope_id == 0)
				sin->sin6_scope_id = sdl->sdl_index;
		}
		getnameinfo((struct sockaddr *)sin, sin->sin6_len, host_buf,
		    sizeof(host_buf), NULL, 0, (opts.nflag ? NI_NUMERICHOST : 0));
		if (cflag) {
#ifdef RTF_WASCLONED
			if (rtm->rtm_flags & RTF_WASCLONED)
				delete(host_buf);
#elif defined(RTF_CLONED)
			if (rtm->rtm_flags & RTF_CLONED)
				delete(host_buf);
#else
			if (rtm->rtm_flags & RTF_PINNED)
				continue;
			delete(host_buf);
#endif
			continue;
		}
		gettimeofday(&now, 0);
		if (opts.tflag)
			ts_print(&now);

		addrwidth = strlen(host_buf);
		if (addrwidth < W_ADDR)
			addrwidth = W_ADDR;
		llwidth = strlen(ether_str(sdl));
		if (W_ADDR + W_LL - addrwidth > llwidth)
			llwidth = W_ADDR + W_LL - addrwidth;
		ifname = if_indextoname(sdl->sdl_index, ifix_buf);
		if (ifname == NULL) {
			strlcpy(ifix_buf, "?", sizeof(ifix_buf));
			ifname = ifix_buf;
		}
		ifwidth = strlen(ifname);
		if (W_ADDR + W_LL + W_IF - addrwidth - llwidth > ifwidth)
			ifwidth = W_ADDR + W_LL + W_IF - addrwidth - llwidth;

		xo_open_instance("neighbor-cache");
		/* Compose format string for libxo, as it doesn't support *.* */
		char xobuf[200];
		snprintf(xobuf, sizeof(xobuf),
		    "{:address/%%-%d.%ds/%%s} {:mac-address/%%-%d.%ds/%%s} {:interface/%%%d.%ds/%%s}",
		    addrwidth, addrwidth, llwidth, llwidth, ifwidth, ifwidth);
		xo_emit(xobuf, host_buf, ether_str(sdl), ifname);

		/* Print neighbor discovery specific information */
		expire = rtm->rtm_rmx.rmx_expire;
		int expire_in = expire - now.tv_sec;
		if (expire > now.tv_sec)
			xo_emit("{d:/ %-9.9s}{e:expires_sec/%d}", sec2str(expire_in), expire_in);
		else if (expire == 0)
			xo_emit("{d:/ %-9.9s}{en:permanent/true}", "permanent");
		else
			xo_emit("{d:/ %-9.9s}{e:expires_sec/%d}", "expired", expire_in);

		char *lle_state = "";
		switch (rtm->rtm_rmx.rmx_state) {
		case ND6_LLINFO_NOSTATE:
			lle_state = "N";
			break;
#ifdef ND6_LLINFO_WAITDELETE
		case ND6_LLINFO_WAITDELETE:
			lle_state = "W";
			break;
#endif
		case ND6_LLINFO_INCOMPLETE:
			lle_state = "I";
			break;
		case ND6_LLINFO_REACHABLE:
			lle_state = "R";
			break;
		case ND6_LLINFO_STALE:
			lle_state = "S";
			break;
		case ND6_LLINFO_DELAY:
			lle_state = "D";
			break;
		case ND6_LLINFO_PROBE:
			lle_state = "P";
			break;
		default:
			lle_state = "?";
			break;
		}
		xo_emit(" {:neighbor-state/%s}", lle_state);

		isrouter = rtm->rtm_flags & RTF_GATEWAY;
		prbs = rtm->rtm_rmx.rmx_pksent;

		/*
		 * other flags. R: router, P: proxy, W: ??
		 */
		if ((rtm->rtm_addrs & RTA_NETMASK) == 0) {
			snprintf(flgbuf, sizeof(flgbuf), "%s%s",
			    isrouter ? "R" : "",
			    (rtm->rtm_flags & RTF_ANNOUNCE) ? "p" : "");
		} else {
#if 0			/* W and P are mystery even for us */
			sin = (struct sockaddr_in6 *)
			    (sdl->sdl_len + (char *)sdl);
			snprintf(flgbuf, sizeof(flgbuf), "%s%s%s%s",
			    isrouter ? "R" : "",
			    !IN6_IS_ADDR_UNSPECIFIED(&sin->sin6_addr) ? "P" : "",
			    (sin->sin6_len != sizeof(struct sockaddr_in6)) ? "W" : "",
			    (rtm->rtm_flags & RTF_ANNOUNCE) ? "p" : "");
#else
			snprintf(flgbuf, sizeof(flgbuf), "%s%s",
			    isrouter ? "R" : "",
			    (rtm->rtm_flags & RTF_ANNOUNCE) ? "p" : "");
#endif
		}
		xo_emit(" {:nd-flags/%s}", flgbuf);

		if (prbs)
			xo_emit("{d:/ %d}", prbs);

		xo_emit("\n");
		xo_close_instance("neighbor-cache");
	}
	if (buf != NULL)
		free(buf);

	if (repeat) {
		xo_emit("\n");
		xo_flush();
		sleep(repeat);
		goto again;
	}

	xo_close_list("neighbor-cache");

	return (count);
}
#endif


static int
delete(char *host)
{
#ifndef WITHOUT_NETLINK
	return (delete_nl(0, host));
#else
	return (delete_rtsock(host));
#endif
}

static int
dump(struct sockaddr_in6 *addr, int cflag)
{
#ifndef WITHOUT_NETLINK
	return (print_entries_nl(0, addr, cflag));
#else
	return (dump_rtsock(addr, cflag));
#endif
}

static struct in6_nbrinfo *
getnbrinfo(struct in6_addr *addr, int ifindex, int warning)
{
	static struct in6_nbrinfo nbi;
	int sock;

	if ((sock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
		xo_err(1, "socket");

	bzero(&nbi, sizeof(nbi));
	if_indextoname(ifindex, nbi.ifname);
	nbi.addr = *addr;
	if (ioctl(sock, SIOCGNBRINFO_IN6, (caddr_t)&nbi) < 0) {
		if (warning)
			xo_warn("ioctl(SIOCGNBRINFO_IN6)");
		close(sock);
		return(NULL);
	}

	close(sock);
	return(&nbi);
}

char *
ether_str(struct sockaddr_dl *sdl)
{
	static char hbuf[NI_MAXHOST];

	if (sdl->sdl_alen == ETHER_ADDR_LEN) {
		strlcpy(hbuf, ether_ntoa((struct ether_addr *)LLADDR(sdl)),
		    sizeof(hbuf));
	} else if (sdl->sdl_alen) {
		int n = sdl->sdl_nlen > 0 ? sdl->sdl_nlen + 1 : 0;
		snprintf(hbuf, sizeof(hbuf), "%s", link_ntoa(sdl) + n);
	} else
		snprintf(hbuf, sizeof(hbuf), "(incomplete)");

	return(hbuf);
}

static int
ndp_ether_aton(char *a, u_char *n)
{
	int i, o[6];

	i = sscanf(a, "%x:%x:%x:%x:%x:%x", &o[0], &o[1], &o[2],
	    &o[3], &o[4], &o[5]);
	if (i != 6) {
		xo_warnx("invalid Ethernet address '%s'", a);
		return (1);
	}
	for (i = 0; i < 6; i++)
		n[i] = o[i];
	return (0);
}

static void
usage(void)
{
	printf("usage: ndp [-nt] hostname\n");
	printf("       ndp [-nt] -a | -c | -p | -r | -H | -P | -R\n");
	printf("       ndp [-nt] -A wait\n");
	printf("       ndp [-nt] -d hostname\n");
	printf("       ndp [-nt] -f filename\n");
	printf("       ndp [-nt] -i interface [flags...]\n");
#ifdef SIOCSDEFIFACE_IN6
	printf("       ndp [-nt] -I [interface|delete]\n");
#endif
	printf("       ndp [-nt] -s nodename etheraddr [temp] [proxy]\n");
	exit(1);
}

#ifdef WITHOUT_NETLINK
static int
rtmsg(int cmd)
{
	static int seq;
	int rlen;
	register struct rt_msghdr *rtm = &m_rtmsg.m_rtm;
	register char *cp = m_rtmsg.m_space;
	register int l;

	errno = 0;
	if (cmd == RTM_DELETE)
		goto doit;
	bzero((char *)&m_rtmsg, sizeof(m_rtmsg));
	rtm->rtm_flags = opts.flags;
	rtm->rtm_version = RTM_VERSION;

	switch (cmd) {
	default:
		xo_errx(1, "internal wrong cmd");
	case RTM_ADD:
		rtm->rtm_addrs |= RTA_GATEWAY;
		if (opts.expire_time) {
			rtm->rtm_rmx.rmx_expire = opts.expire_time;
			rtm->rtm_inits = RTV_EXPIRE;
		}
		rtm->rtm_flags |= (RTF_HOST | RTF_STATIC | RTF_LLDATA);
		/* FALLTHROUGH */
	case RTM_GET:
		rtm->rtm_addrs |= RTA_DST;
	}

	NEXTADDR(RTA_DST, sin_m);
	NEXTADDR(RTA_GATEWAY, sdl_m);

	rtm->rtm_msglen = cp - (char *)&m_rtmsg;
doit:
	l = rtm->rtm_msglen;
	rtm->rtm_seq = ++seq;
	rtm->rtm_type = cmd;
	if ((rlen = write(s, (char *)&m_rtmsg, l)) < 0) {
		if (errno != ESRCH || cmd != RTM_DELETE) {
			xo_err(1, "writing to routing socket");
			/* NOTREACHED */
		}
	}
	do {
		l = read(s, (char *)&m_rtmsg, sizeof(m_rtmsg));
	} while (l > 0 && (rtm->rtm_type != cmd || rtm->rtm_seq != seq ||
	    rtm->rtm_pid != pid));
	if (l < 0)
		xo_warn("read from routing socket");
	return (0);
}
#endif

static void
ifinfo(char *ifname, int argc, char **argv)
{
	struct in6_ndireq nd;
	int i, sock;
	u_int32_t newflags;
#ifdef IPV6CTL_USETEMPADDR
	u_int8_t nullbuf[8];
#endif

	if ((sock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		xo_err(1, "socket");
		/* NOTREACHED */
	}
	bzero(&nd, sizeof(nd));
	strlcpy(nd.ifname, ifname, sizeof(nd.ifname));
	if (ioctl(sock, SIOCGIFINFO_IN6, (caddr_t)&nd) < 0) {
		xo_err(1, "ioctl(SIOCGIFINFO_IN6)");
		/* NOTREACHED */
	}
#define	ND nd.ndi
	newflags = ND.flags;
	for (i = 0; i < argc; i++) {
		int clear = 0;
		char *cp = argv[i];

		if (*cp == '-') {
			clear = 1;
			cp++;
		}

#define	SETFLAG(s, f) do {			\
	if (strcmp(cp, (s)) == 0) {		\
		if (clear)			\
			newflags &= ~(f);	\
		else				\
			newflags |= (f);	\
	}					\
} while (0)
/*
 * XXX: this macro is not 100% correct, in that it matches "nud" against
 *      "nudbogus".  But we just let it go since this is minor.
 */
#define	SETVALUE(f, v) do {						\
	char *valptr;							\
	unsigned long newval;						\
	v = 0; /* unspecified */					\
	if (strncmp(cp, f, strlen(f)) == 0) {				\
		valptr = strchr(cp, '=');				\
		if (valptr == NULL)					\
			xo_err(1, "syntax error in %s field", (f));	\
		errno = 0;						\
		newval = strtoul(++valptr, NULL, 0);			\
		if (errno)						\
			xo_err(1, "syntax error in %s's value", (f));	\
		v = newval;						\
	}								\
} while (0)

		SETFLAG("disabled", ND6_IFF_IFDISABLED);
		SETFLAG("nud", ND6_IFF_PERFORMNUD);
#ifdef ND6_IFF_ACCEPT_RTADV
		SETFLAG("accept_rtadv", ND6_IFF_ACCEPT_RTADV);
#endif
#ifdef ND6_IFF_AUTO_LINKLOCAL
		SETFLAG("auto_linklocal", ND6_IFF_AUTO_LINKLOCAL);
#endif
#ifdef ND6_IFF_NO_PREFER_IFACE
		SETFLAG("no_prefer_iface", ND6_IFF_NO_PREFER_IFACE);
#endif
		SETVALUE("basereachable", ND.basereachable);
		SETVALUE("retrans", ND.retrans);
		SETVALUE("curhlim", ND.chlim);

		ND.flags = newflags;
		if (ioctl(sock, SIOCSIFINFO_IN6, (caddr_t)&nd) < 0) {
			xo_err(1, "ioctl(SIOCSIFINFO_IN6)");
			/* NOTREACHED */
		}
#undef SETFLAG
#undef SETVALUE
	}

	if (!ND.initialized) {
		xo_errx(1, "%s: not initialized yet", ifname);
		/* NOTREACHED */
	}

	if (ioctl(sock, SIOCGIFINFO_IN6, (caddr_t)&nd) < 0) {
		xo_err(1, "ioctl(SIOCGIFINFO_IN6)");
		/* NOTREACHED */
	}
	xo_open_container("ifinfo");

	xo_emit("{e:interface/%s}", ifname);
	xo_emit("linkmtu={:linkmtu/%d}", ND.linkmtu);
	xo_emit(", maxmtu={:maxmtu/%d}", ND.maxmtu);
	xo_emit(", curhlim={:curhlim/%d}", ND.chlim);
	xo_emit("{d:/, basereachable=%ds%dms}{e:basereachable_ms/%u}",
	    ND.basereachable / 1000, ND.basereachable % 1000, ND.basereachable);
	xo_emit("{d:/, reachable=%ds}{e:reachable_ms/%u}", ND.reachable, ND.reachable * 1000);
	xo_emit("{d:/, retrans=%ds%dms}{e:retrans_ms/%u}", ND.retrans / 1000, ND.retrans % 1000,
	    ND.retrans);
#ifdef IPV6CTL_USETEMPADDR
	memset(nullbuf, 0, sizeof(nullbuf));
	if (memcmp(nullbuf, ND.randomid, sizeof(nullbuf)) != 0) {
		int j;
		u_int8_t *rbuf;

		for (i = 0; i < 3; i++) {
			const char *txt, *field;
			switch (i) {
			case 0:
				txt = "\nRandom seed(0): ";
				field = "seed_0";
				rbuf = ND.randomseed0;
				break;
			case 1:
				txt = "\nRandom seed(1): ";
				field = "seed_1";
				rbuf = ND.randomseed1;
				break;
			case 2:
				txt = "\nRandom ID:      ";
				field = "random_id";
				rbuf = ND.randomid;
				break;
			default:
				xo_errx(1, "impossible case for tempaddr display");
			}
			char abuf[20], xobuf[200];
			for (j = 0; j < 8; j++)
				snprintf(&abuf[j * 2], sizeof(abuf), "%02X", rbuf[j]);
			snprintf(xobuf, sizeof(xobuf), "%s{:%s/%%s}", txt, field);
			xo_emit(xobuf, abuf);
		}
	}
#endif /* IPV6CTL_USETEMPADDR */
	if (ND.flags) {
		xo_emit("\nFlags: {e:flags/%u}", ND.flags);
		xo_open_list("flags_pretty");
#ifdef ND6_IFF_IFDISABLED
		if ((ND.flags & ND6_IFF_IFDISABLED))
			xo_emit("{l:%s} ", "disabled");
#endif
		if ((ND.flags & ND6_IFF_PERFORMNUD))
			xo_emit("{l:%s} ", "nud");
#ifdef ND6_IFF_ACCEPT_RTADV
		if ((ND.flags & ND6_IFF_ACCEPT_RTADV))
			xo_emit("{l:%s} ", "accept_rtadv");
#endif
#ifdef ND6_IFF_AUTO_LINKLOCAL
		if ((ND.flags & ND6_IFF_AUTO_LINKLOCAL))
			xo_emit("{l:%s} ", "auto_linklocal");
#endif
#ifdef ND6_IFF_NO_PREFER_IFACE
		if ((ND.flags & ND6_IFF_NO_PREFER_IFACE))
			xo_emit("{l:%s} ", "no_prefer_iface");
#endif
		xo_close_list("flags");
	}
	xo_emit("\n");
#undef ND
	xo_close_container("ifinfo");

	close(sock);
}

#ifndef ND_RA_FLAG_RTPREF_MASK	/* XXX: just for compilation on *BSD release */
#define ND_RA_FLAG_RTPREF_MASK	0x18 /* 00011000 */
#endif

static void
rtrlist(void)
{
	int mib[] = { CTL_NET, PF_INET6, IPPROTO_ICMPV6, ICMPV6CTL_ND6_DRLIST };
	char *buf;
	struct in6_defrouter *p, *ep;
	size_t l;
	struct timeval now;

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), NULL, &l, NULL, 0) < 0) {
		xo_err(1, "sysctl(ICMPV6CTL_ND6_DRLIST)");
		/*NOTREACHED*/
	}
	if (l == 0)
		return;
	buf = malloc(l);
	if (!buf) {
		xo_err(1, "malloc");
		/*NOTREACHED*/
	}
	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), buf, &l, NULL, 0) < 0) {
		xo_err(1, "sysctl(ICMPV6CTL_ND6_DRLIST)");
		/*NOTREACHED*/
	}

	xo_open_list("router-list");

	ep = (struct in6_defrouter *)(buf + l);
	for (p = (struct in6_defrouter *)buf; p < ep; p++) {
		int rtpref;
		char abuf[INET6_ADDRSTRLEN], *paddr;

		if (getnameinfo((struct sockaddr *)&p->rtaddr,
		    p->rtaddr.sin6_len, host_buf, sizeof(host_buf), NULL, 0,
		    (opts.nflag ? NI_NUMERICHOST : 0)) != 0)
			strlcpy(host_buf, "?", sizeof(host_buf));
		if (opts.nflag)
			paddr = host_buf;
		else {
			inet_ntop(AF_INET6, &p->rtaddr.sin6_addr, abuf, sizeof(abuf));
			paddr = abuf;
		}

		xo_open_instance("router-list");
		xo_emit("{:hostname/%s}{e:address/%s} if={:interface/%s}",
		    host_buf, paddr,
		    if_indextoname(p->if_index, ifix_buf));
		xo_open_list("flags_pretty");
		char rflags[6] = {}, *pflags = rflags;
		if (p->flags & ND_RA_FLAG_MANAGED) {
			*pflags++ = 'M';
			xo_emit("{el:%s}", "managed");
		}
		if (p->flags & ND_RA_FLAG_OTHER) {
			*pflags++ = 'O';
			xo_emit("{el:%s}", "other");
		}
#ifdef DRAFT_IETF_6MAN_IPV6ONLY_FLAG
		if (p->flags & ND_RA_FLAG_IPV6_ONLY) {
			*pflags++ = 'S';
			xo_emit("{el:%s}", "ipv6only");
		}
#endif
		xo_close_list("flags_pretty");
		xo_emit(", flags={:flags/%s}", rflags);

		rtpref = ((p->flags & ND_RA_FLAG_RTPREF_MASK) >> 3) & 0xff;
		xo_emit(", pref={:preference/%s}", rtpref_str[rtpref]);

		gettimeofday(&now, 0);
		if (p->expire == 0)
			xo_emit(", expire=Never\n{en:permanent/true}");
		else
			xo_emit("{d:/, expire=%s\n}{e:expires_sec/%ld}",
			    sec2str(p->expire - now.tv_sec),
			    (long)p->expire - now.tv_sec);
		xo_close_instance("router-list");
	}
	free(buf);
	xo_close_list("router-list");
}

static void
plist(void)
{
	int mib[] = { CTL_NET, PF_INET6, IPPROTO_ICMPV6, ICMPV6CTL_ND6_PRLIST };
	char *buf;
	struct in6_prefix *p, *ep, *n;
	struct sockaddr_in6 *advrtr;
	size_t l;
	struct timeval now;
	const int niflags = NI_NUMERICHOST;
	int ninflags = opts.nflag ? NI_NUMERICHOST : 0;
	char namebuf[NI_MAXHOST];

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), NULL, &l, NULL, 0) < 0) {
		xo_err(1, "sysctl(ICMPV6CTL_ND6_PRLIST)");
		/*NOTREACHED*/
	}
	buf = malloc(l);
	if (!buf) {
		xo_err(1, "malloc");
		/*NOTREACHED*/
	}
	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), buf, &l, NULL, 0) < 0) {
		xo_err(1, "sysctl(ICMPV6CTL_ND6_PRLIST)");
		/*NOTREACHED*/
	}

	xo_open_list("prefix-list");

	ep = (struct in6_prefix *)(buf + l);
	for (p = (struct in6_prefix *)buf; p < ep; p = n) {
		advrtr = (struct sockaddr_in6 *)(p + 1);
		n = (struct in6_prefix *)&advrtr[p->advrtrs];

		xo_open_instance("prefix-list");
		if (getnameinfo((struct sockaddr *)&p->prefix,
		    p->prefix.sin6_len, namebuf, sizeof(namebuf),
		    NULL, 0, niflags) != 0)
			strlcpy(namebuf, "?", sizeof(namebuf));
		xo_emit("{:prefix/%s%s%d} if={:interface/%s}\n", namebuf, "/",
		    p->prefixlen, if_indextoname(p->if_index, ifix_buf));

		gettimeofday(&now, 0);
		/*
		 * meaning of fields, especially flags, is very different
		 * by origin.  notify the difference to the users.
		 */
		char flags[10] = {}, *pflags = flags;
		xo_open_list("flags_pretty");
		if (p->raflags.onlink) {
			*pflags++ = 'L';
			xo_emit("{el:%s}", "ra_onlink");
		}
		if (p->raflags.autonomous) {
			*pflags++ = 'A';
			xo_emit("{el:%s}", "ra_autonomous");
		}
		if (p->flags & NDPRF_ONLINK) {
			*pflags++ = 'O';
			xo_emit("{el:%s}", "is_onlink");
		}
		if (p->flags & NDPRF_DETACHED) {
			*pflags++ = 'D';
			xo_emit("{el:%s}", "is_detached");
		}
#ifdef NDPRF_HOME
		if (p->flags & NDPRF_HOME) {
			*pflags++ = 'H';
			xo_emit("{el:%s}", "is_home");
		}
#endif
		xo_close_list("flags_pretty");
		xo_emit("flags={:flags/%s}", flags);
		int expire_in = p->expire - now.tv_sec;

		if (p->vltime == ND6_INFINITE_LIFETIME)
			xo_emit(" vltime=infinity{e:valid-lifetime/%lu}",
			    (unsigned long)p->vltime);
		else
			xo_emit(" vltime={:valid-lifetime/%lu}",
			    (unsigned long)p->vltime);
		if (p->pltime == ND6_INFINITE_LIFETIME)
			xo_emit(", pltime=infinity{e:preferred-lifetime/%lu}",
			    (unsigned long)p->pltime);
		else
			xo_emit(", pltime={:preferred-lifetime/%lu}",
			    (unsigned long)p->pltime);
		if (p->expire == 0)
			xo_emit(", expire=Never{en:permanent/true}");
		else if (p->expire >= now.tv_sec)
			xo_emit(", expire=%s{e:expires_sec/%d}",
			    sec2str(expire_in), expire_in);
		else
			xo_emit(", expired{e:expires_sec/%d}", expire_in);
		xo_emit(", ref={:refcount/%d}", p->refcnt);
		xo_emit("\n");
		/*
		 * "advertising router" list is meaningful only if the prefix
		 * information is from RA.
		 */
		if (p->advrtrs) {
			int j;
			struct sockaddr_in6 *sin6;

			sin6 = advrtr;
			xo_emit("  advertised by\n");
			xo_open_list("advertising-routers");
			for (j = 0; j < p->advrtrs; j++) {
				struct in6_nbrinfo *nbi;

				xo_open_instance("advertising-routers");
				if (getnameinfo((struct sockaddr *)sin6,
				    sin6->sin6_len, namebuf, sizeof(namebuf),
				    NULL, 0, ninflags) != 0)
					strlcpy(namebuf, "?", sizeof(namebuf));
				char abuf[INET6_ADDRSTRLEN];
				inet_ntop(AF_INET6, &sin6->sin6_addr, abuf,
				    sizeof(abuf));

				xo_emit("    {:hostname/%s}{e:address/%s}",
				    namebuf, abuf);

				nbi = getnbrinfo(&sin6->sin6_addr,
				    p->if_index, 0);
				const char *state = "";
				if (nbi) {
					switch (nbi->state) {
					case ND6_LLINFO_REACHABLE:
					case ND6_LLINFO_STALE:
					case ND6_LLINFO_DELAY:
					case ND6_LLINFO_PROBE:
						state = "reachable";
						break;
					default:
						state = "unreachable";
					}
				} else
					state = "no neighbor state";
				xo_emit(" ({:state/%s})\n", state);
				sin6++;
				xo_close_instance("advertising-routers");
			}
			xo_close_list("advertising-routers");
		} else
			xo_emit("  No advertising router\n");
		xo_close_instance("prefix-list");
	}
	free(buf);

	xo_close_list("prefix-list");
}

static void
pfx_flush(void)
{
	char dummyif[IFNAMSIZ+8];
	int sock;

	if ((sock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
		xo_err(1, "socket");
	strlcpy(dummyif, "lo0", sizeof(dummyif)); /* dummy */
	if (ioctl(sock, SIOCSPFXFLUSH_IN6, (caddr_t)&dummyif) < 0)
		xo_err(1, "ioctl(SIOCSPFXFLUSH_IN6)");

	close(sock);
}

static void
rtr_flush(void)
{
	char dummyif[IFNAMSIZ+8];
	int sock;

	if ((sock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
		xo_err(1, "socket");
	strlcpy(dummyif, "lo0", sizeof(dummyif)); /* dummy */
	if (ioctl(sock, SIOCSRTRFLUSH_IN6, (caddr_t)&dummyif) < 0)
		xo_err(1, "ioctl(SIOCSRTRFLUSH_IN6)");

	close(sock);
}

static void
harmonize_rtr(void)
{
	char dummyif[IFNAMSIZ+8];
	int sock;

	if ((sock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
		xo_err(1, "socket");
	strlcpy(dummyif, "lo0", sizeof(dummyif)); /* dummy */
	if (ioctl(sock, SIOCSNDFLUSH_IN6, (caddr_t)&dummyif) < 0)
		xo_err(1, "ioctl(SIOCSNDFLUSH_IN6)");

	close(sock);
}

#ifdef SIOCSDEFIFACE_IN6	/* XXX: check SIOCGDEFIFACE_IN6 as well? */
static void
setdefif(char *ifname)
{
	struct in6_ndifreq ndifreq;
	unsigned int ifindex;
	int sock;

	if (strcasecmp(ifname, "delete") == 0)
		ifindex = 0;
	else {
		if ((ifindex = if_nametoindex(ifname)) == 0)
			xo_err(1, "failed to resolve i/f index for %s", ifname);
	}

	if ((sock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
		xo_err(1, "socket");

	strlcpy(ndifreq.ifname, "lo0", sizeof(ndifreq.ifname)); /* dummy */
	ndifreq.ifindex = ifindex;

	if (ioctl(sock, SIOCSDEFIFACE_IN6, (caddr_t)&ndifreq) < 0)
		xo_err(1, "ioctl(SIOCSDEFIFACE_IN6)");

	close(sock);
}

static void
getdefif(void)
{
	struct in6_ndifreq ndifreq;
	char ifname[IFNAMSIZ+8];
	int sock;

	if ((sock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
		xo_err(1, "socket");

	memset(&ndifreq, 0, sizeof(ndifreq));
	strlcpy(ndifreq.ifname, "lo0", sizeof(ndifreq.ifname)); /* dummy */

	if (ioctl(sock, SIOCGDEFIFACE_IN6, (caddr_t)&ndifreq) < 0)
		xo_err(1, "ioctl(SIOCGDEFIFACE_IN6)");

	if (ndifreq.ifindex == 0)
		xo_emit("No default interface.\n");
	else {
		if ((if_indextoname(ndifreq.ifindex, ifname)) == NULL)
			xo_err(1, "failed to resolve ifname for index %lu",
			    ndifreq.ifindex);
		xo_emit("ND default interface = {:default-interface/%s}\n", ifname);
	}

	close(sock);
}
#endif /* SIOCSDEFIFACE_IN6 */

char *
sec2str(time_t total)
{
	static char result[256];
	int days, hours, mins, secs;
	int first = 1;
	char *p = result;
	char *ep = &result[sizeof(result)];
	int n;

	days = total / 3600 / 24;
	hours = (total / 3600) % 24;
	mins = (total / 60) % 60;
	secs = total % 60;

	if (days) {
		first = 0;
		n = snprintf(p, ep - p, "%dd", days);
		if (n < 0 || n >= ep - p)
			return "?";
		p += n;
	}
	if (!first || hours) {
		first = 0;
		n = snprintf(p, ep - p, "%dh", hours);
		if (n < 0 || n >= ep - p)
			return "?";
		p += n;
	}
	if (!first || mins) {
		first = 0;
		n = snprintf(p, ep - p, "%dm", mins);
		if (n < 0 || n >= ep - p)
			return "?";
		p += n;
	}
	snprintf(p, ep - p, "%ds", secs);

	return(result);
}

/*
 * Print the timestamp
 * from tcpdump/util.c
 */
void
ts_print(const struct timeval *tvp)
{
	int sec;

	/* Default */
	sec = (tvp->tv_sec + thiszone) % 86400;
	xo_emit("{:tv_sec/%lld}{:tv_usec/%lld}%02d:%02d:%02d.%06u ",
	    tvp->tv_sec, tvp->tv_usec,
	    sec / 3600, (sec % 3600) / 60, sec % 60, (u_int32_t)tvp->tv_usec);
}

#undef NEXTADDR
