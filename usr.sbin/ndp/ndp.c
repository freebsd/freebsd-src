/*	$FreeBSD$	*/
/*	$KAME: ndp.c,v 1.104 2003/06/27 07:48:39 itojun Exp $	*/

/*
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
 * 4. Neither the name of the University nor the names of its contributors
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
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <netinet/icmp6.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>

#include <arpa/inet.h>

#include <netdb.h>
#include <errno.h>
#include <nlist.h>
#include <stdio.h>
#include <string.h>
#include <paths.h>
#include <err.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>
#include "gmt2local.h"

/* packing rule for routing socket */
#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

static pid_t pid;
static int nflag;
static int tflag;
static int32_t thiszone;	/* time difference with gmt */
static int s = -1;
static int repeat = 0;

char ntop_buf[INET6_ADDRSTRLEN];	/* inet_ntop() */
char host_buf[NI_MAXHOST];		/* getnameinfo() */
char ifix_buf[IFNAMSIZ];		/* if_indextoname() */

int main __P((int, char **));
int file __P((char *));
void getsocket __P((void));
int set __P((int, char **));
void get __P((char *));
int delete __P((char *));
void dump __P((struct in6_addr *, int));
static struct in6_nbrinfo *getnbrinfo __P((struct in6_addr *, int, int));
static char *ether_str __P((struct sockaddr_dl *));
int ndp_ether_aton __P((char *, u_char *));
void usage __P((void));
int rtmsg __P((int));
void ifinfo __P((char *, int, char **));
void rtrlist __P((void));
void plist __P((void));
void pfx_flush __P((void));
void rtr_flush __P((void));
void harmonize_rtr __P((void));
#ifdef SIOCSDEFIFACE_IN6	/* XXX: check SIOCGDEFIFACE_IN6 as well? */
static void getdefif __P((void));
static void setdefif __P((char *));
#endif
static char *sec2str __P((time_t));
static char *ether_str __P((struct sockaddr_dl *));
static void ts_print __P((const struct timeval *));

#ifdef ICMPV6CTL_ND6_DRLIST
static char *rtpref_str[] = {
	"medium",		/* 00 */
	"high",			/* 01 */
	"rsv",			/* 10 */
	"low"			/* 11 */
};
#endif

int mode = 0;
char *arg = NULL;

int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch;

	pid = getpid();
	thiszone = gmt2local(0);
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
		case 'd':
		case 'f':
		case 'i' :
			if (mode) {
				usage();
				/*NOTREACHED*/
			}
			mode = ch;
			arg = optarg;
			break;
		case 'n':
			nflag = 1;
			break;
		case 't':
			tflag = 1;
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
		delete(arg);
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
				errx(1, "invalid interface %s", *argv);
		}
		getdefif(); /* always call it to print the result */
		break;
#else
		errx(1, "not supported yet");
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
	exit(0);
}

/*
 * Process a file to set standard ndp entries
 */
int
file(name)
	char *name;
{
	FILE *fp;
	int i, retval;
	char line[100], arg[5][50], *args[5];

	if ((fp = fopen(name, "r")) == NULL) {
		fprintf(stderr, "ndp: cannot open %s\n", name);
		exit(1);
	}
	args[0] = &arg[0][0];
	args[1] = &arg[1][0];
	args[2] = &arg[2][0];
	args[3] = &arg[3][0];
	args[4] = &arg[4][0];
	retval = 0;
	while (fgets(line, 100, fp) != NULL) {
		i = sscanf(line, "%49s %49s %49s %49s %49s",
		    arg[0], arg[1], arg[2], arg[3], arg[4]);
		if (i < 2) {
			fprintf(stderr, "ndp: bad line: %s\n", line);
			retval = 1;
			continue;
		}
		if (set(i, args))
			retval = 1;
	}
	fclose(fp);
	return (retval);
}

void
getsocket()
{
	if (s < 0) {
		s = socket(PF_ROUTE, SOCK_RAW, 0);
		if (s < 0) {
			err(1, "socket");
			/* NOTREACHED */
		}
	}
}

struct	sockaddr_in6 so_mask = {sizeof(so_mask), AF_INET6 };
struct	sockaddr_in6 blank_sin = {sizeof(blank_sin), AF_INET6 }, sin_m;
struct	sockaddr_dl blank_sdl = {sizeof(blank_sdl), AF_LINK }, sdl_m;
int	expire_time, flags, found_entry;
struct	{
	struct	rt_msghdr m_rtm;
	char	m_space[512];
}	m_rtmsg;

/*
 * Set an individual neighbor cache entry
 */
int
set(argc, argv)
	int argc;
	char **argv;
{
	register struct sockaddr_in6 *sin = &sin_m;
	register struct sockaddr_dl *sdl;
	register struct rt_msghdr *rtm = &(m_rtmsg.m_rtm);
	struct addrinfo hints, *res;
	int gai_error;
	u_char *ea;
	char *host = argv[0], *eaddr = argv[1];

	getsocket();
	argc -= 2;
	argv += 2;
	sdl_m = blank_sdl;
	sin_m = blank_sin;

	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_INET6;
	gai_error = getaddrinfo(host, NULL, &hints, &res);
	if (gai_error) {
		fprintf(stderr, "ndp: %s: %s\n", host,
			gai_strerror(gai_error));
		return 1;
	}
	sin->sin6_addr = ((struct sockaddr_in6 *)res->ai_addr)->sin6_addr;
#ifdef __KAME__
	if (IN6_IS_ADDR_LINKLOCAL(&sin->sin6_addr)) {
		*(u_int16_t *)&sin->sin6_addr.s6_addr[2] =
		    htons(((struct sockaddr_in6 *)res->ai_addr)->sin6_scope_id);
	}
#endif
	ea = (u_char *)LLADDR(&sdl_m);
	if (ndp_ether_aton(eaddr, ea) == 0)
		sdl_m.sdl_alen = 6;
	flags = expire_time = 0;
	while (argc-- > 0) {
		if (strncmp(argv[0], "temp", 4) == 0) {
			struct timeval time;

			gettimeofday(&time, 0);
			expire_time = time.tv_sec + 20 * 60;
		} else if (strncmp(argv[0], "proxy", 5) == 0)
			flags |= RTF_ANNOUNCE;
		argv++;
	}
	if (rtmsg(RTM_GET) < 0) {
		errx(1, "RTM_GET(%s) failed", host);
		/* NOTREACHED */
	}
	sin = (struct sockaddr_in6 *)(rtm + 1);
	sdl = (struct sockaddr_dl *)(ROUNDUP(sin->sin6_len) + (char *)sin);
	if (IN6_ARE_ADDR_EQUAL(&sin->sin6_addr, &sin_m.sin6_addr)) {
		if (sdl->sdl_family == AF_LINK &&
		    (rtm->rtm_flags & RTF_LLINFO) &&
		    !(rtm->rtm_flags & RTF_GATEWAY)) {
			switch (sdl->sdl_type) {
			case IFT_ETHER: case IFT_FDDI: case IFT_ISO88023:
			case IFT_ISO88024: case IFT_ISO88025:
				goto overwrite;
			}
		}
		/*
		 * IPv4 arp command retries with sin_other = SIN_PROXY here.
		 */
		fprintf(stderr, "set: cannot configure a new entry\n");
		return 1;
	}

overwrite:
	if (sdl->sdl_family != AF_LINK) {
		printf("cannot intuit interface index and type for %s\n", host);
		return (1);
	}
	sdl_m.sdl_type = sdl->sdl_type;
	sdl_m.sdl_index = sdl->sdl_index;
	return (rtmsg(RTM_ADD));
}

/*
 * Display an individual neighbor cache entry
 */
void
get(host)
	char *host;
{
	struct sockaddr_in6 *sin = &sin_m;
	struct addrinfo hints, *res;
	int gai_error;

	sin_m = blank_sin;
	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_INET6;
	gai_error = getaddrinfo(host, NULL, &hints, &res);
	if (gai_error) {
		fprintf(stderr, "ndp: %s: %s\n", host,
		    gai_strerror(gai_error));
		return;
	}
	sin->sin6_addr = ((struct sockaddr_in6 *)res->ai_addr)->sin6_addr;
#ifdef __KAME__
	if (IN6_IS_ADDR_LINKLOCAL(&sin->sin6_addr)) {
		*(u_int16_t *)&sin->sin6_addr.s6_addr[2] =
		    htons(((struct sockaddr_in6 *)res->ai_addr)->sin6_scope_id);
	}
#endif
	dump(&sin->sin6_addr, 0);
	if (found_entry == 0) {
		getnameinfo((struct sockaddr *)sin, sin->sin6_len, host_buf,
		    sizeof(host_buf), NULL ,0,
		    (nflag ? NI_NUMERICHOST : 0));
		printf("%s (%s) -- no entry\n", host, host_buf);
		exit(1);
	}
}

/*
 * Delete a neighbor cache entry
 */
int
delete(host)
	char *host;
{
	struct sockaddr_in6 *sin = &sin_m;
	register struct rt_msghdr *rtm = &m_rtmsg.m_rtm;
	struct sockaddr_dl *sdl;
	struct addrinfo hints, *res;
	int gai_error;

	getsocket();
	sin_m = blank_sin;

	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_INET6;
	gai_error = getaddrinfo(host, NULL, &hints, &res);
	if (gai_error) {
		fprintf(stderr, "ndp: %s: %s\n", host,
		    gai_strerror(gai_error));
		return 1;
	}
	sin->sin6_addr = ((struct sockaddr_in6 *)res->ai_addr)->sin6_addr;
#ifdef __KAME__
	if (IN6_IS_ADDR_LINKLOCAL(&sin->sin6_addr)) {
		*(u_int16_t *)&sin->sin6_addr.s6_addr[2] =
		    htons(((struct sockaddr_in6 *)res->ai_addr)->sin6_scope_id);
	}
#endif
	if (rtmsg(RTM_GET) < 0) {
		errx(1, "RTM_GET(%s) failed", host);
		/* NOTREACHED */
	}
	sin = (struct sockaddr_in6 *)(rtm + 1);
	sdl = (struct sockaddr_dl *)(ROUNDUP(sin->sin6_len) + (char *)sin);
	if (IN6_ARE_ADDR_EQUAL(&sin->sin6_addr, &sin_m.sin6_addr)) {
		if (sdl->sdl_family == AF_LINK &&
		    (rtm->rtm_flags & RTF_LLINFO) &&
		    !(rtm->rtm_flags & RTF_GATEWAY)) {
			goto delete;
		}
		/*
		 * IPv4 arp command retries with sin_other = SIN_PROXY here.
		 */
		fprintf(stderr, "delete: cannot delete non-NDP entry\n");
		return 1;
	}

delete:
	if (sdl->sdl_family != AF_LINK) {
		printf("cannot locate %s\n", host);
		return (1);
	}
	if (rtmsg(RTM_DELETE) == 0) {
		struct sockaddr_in6 s6 = *sin; /* XXX: for safety */

#ifdef __KAME__
		if (IN6_IS_ADDR_LINKLOCAL(&s6.sin6_addr)) {
			s6.sin6_scope_id = ntohs(*(u_int16_t *)&s6.sin6_addr.s6_addr[2]);
			*(u_int16_t *)&s6.sin6_addr.s6_addr[2] = 0;
		}
#endif
		getnameinfo((struct sockaddr *)&s6,
		    s6.sin6_len, host_buf,
		    sizeof(host_buf), NULL, 0,
		    (nflag ? NI_NUMERICHOST : 0));
		printf("%s (%s) deleted\n", host, host_buf);
	}

	return 0;
}

#define W_ADDR	36
#define W_LL	17
#define W_IF	6

/*
 * Dump the entire neighbor cache
 */
void
dump(addr, cflag)
	struct in6_addr *addr;
	int cflag;
{
	int mib[6];
	size_t needed;
	char *lim, *buf, *next;
	struct rt_msghdr *rtm;
	struct sockaddr_in6 *sin;
	struct sockaddr_dl *sdl;
	extern int h_errno;
	struct in6_nbrinfo *nbi;
	struct timeval time;
	int addrwidth;
	int llwidth;
	int ifwidth;
	char flgbuf[8];
	char *ifname;

	/* Print header */
	if (!tflag && !cflag)
		printf("%-*.*s %-*.*s %*.*s %-9.9s %1s %5s\n",
		    W_ADDR, W_ADDR, "Neighbor", W_LL, W_LL, "Linklayer Address",
		    W_IF, W_IF, "Netif", "Expire", "S", "Flags");

again:;
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET6;
	mib[4] = NET_RT_FLAGS;
	mib[5] = RTF_LLINFO;
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		err(1, "sysctl(PF_ROUTE estimate)");
	if (needed > 0) {
		if ((buf = malloc(needed)) == NULL)
			err(1, "malloc");
		if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
			err(1, "sysctl(PF_ROUTE, NET_RT_FLAGS)");
		lim = buf + needed;
	} else
		buf = lim = NULL;

	for (next = buf; next && next < lim; next += rtm->rtm_msglen) {
		int isrouter = 0, prbs = 0;

		rtm = (struct rt_msghdr *)next;
		sin = (struct sockaddr_in6 *)(rtm + 1);
		sdl = (struct sockaddr_dl *)((char *)sin + ROUNDUP(sin->sin6_len));

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
			if (!IN6_ARE_ADDR_EQUAL(addr, &sin->sin6_addr))
				continue;
			found_entry = 1;
		} else if (IN6_IS_ADDR_MULTICAST(&sin->sin6_addr))
			continue;
		if (IN6_IS_ADDR_LINKLOCAL(&sin->sin6_addr) ||
		    IN6_IS_ADDR_MC_LINKLOCAL(&sin->sin6_addr)) {
			/* XXX: should scope id be filled in the kernel? */
			if (sin->sin6_scope_id == 0)
				sin->sin6_scope_id = sdl->sdl_index;
#ifdef __KAME__
			/* KAME specific hack; removed the embedded id */
			*(u_int16_t *)&sin->sin6_addr.s6_addr[2] = 0;
#endif
		}
		getnameinfo((struct sockaddr *)sin, sin->sin6_len, host_buf,
		    sizeof(host_buf), NULL, 0, (nflag ? NI_NUMERICHOST : 0));
		if (cflag) {
#ifdef RTF_WASCLONED
			if (rtm->rtm_flags & RTF_WASCLONED)
				delete(host_buf);
#elif defined(RTF_CLONED)
			if (rtm->rtm_flags & RTF_CLONED)
				delete(host_buf);
#else
			delete(host_buf);
#endif
			continue;
		}
		gettimeofday(&time, 0);
		if (tflag)
			ts_print(&time);

		addrwidth = strlen(host_buf);
		if (addrwidth < W_ADDR)
			addrwidth = W_ADDR;
		llwidth = strlen(ether_str(sdl));
		if (W_ADDR + W_LL - addrwidth > llwidth)
			llwidth = W_ADDR + W_LL - addrwidth;
		ifname = if_indextoname(sdl->sdl_index, ifix_buf);
		if (!ifname)
			ifname = "?";
		ifwidth = strlen(ifname);
		if (W_ADDR + W_LL + W_IF - addrwidth - llwidth > ifwidth)
			ifwidth = W_ADDR + W_LL + W_IF - addrwidth - llwidth;

		printf("%-*.*s %-*.*s %*.*s", addrwidth, addrwidth, host_buf,
		    llwidth, llwidth, ether_str(sdl), ifwidth, ifwidth, ifname);

		/* Print neighbor discovery specific informations */
		nbi = getnbrinfo(&sin->sin6_addr, sdl->sdl_index, 1);
		if (nbi) {
			if (nbi->expire > time.tv_sec) {
				printf(" %-9.9s",
				    sec2str(nbi->expire - time.tv_sec));
			} else if (nbi->expire == 0)
				printf(" %-9.9s", "permanent");
			else
				printf(" %-9.9s", "expired");

			switch (nbi->state) {
			case ND6_LLINFO_NOSTATE:
				 printf(" N");
				 break;
#ifdef ND6_LLINFO_WAITDELETE
			case ND6_LLINFO_WAITDELETE:
				 printf(" W");
				 break;
#endif
			case ND6_LLINFO_INCOMPLETE:
				 printf(" I");
				 break;
			case ND6_LLINFO_REACHABLE:
				 printf(" R");
				 break;
			case ND6_LLINFO_STALE:
				 printf(" S");
				 break;
			case ND6_LLINFO_DELAY:
				 printf(" D");
				 break;
			case ND6_LLINFO_PROBE:
				 printf(" P");
				 break;
			default:
				 printf(" ?");
				 break;
			}

			isrouter = nbi->isrouter;
			prbs = nbi->asked;
		} else {
			warnx("failed to get neighbor information");
			printf("  ");
		}

		/*
		 * other flags. R: router, P: proxy, W: ??
		 */
		if ((rtm->rtm_addrs & RTA_NETMASK) == 0) {
			snprintf(flgbuf, sizeof(flgbuf), "%s%s",
			    isrouter ? "R" : "",
			    (rtm->rtm_flags & RTF_ANNOUNCE) ? "p" : "");
		} else {
			sin = (struct sockaddr_in6 *)
			    (sdl->sdl_len + (char *)sdl);
#if 0	/* W and P are mystery even for us */
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
		printf(" %s", flgbuf);

		if (prbs)
			printf(" %d", prbs);

		printf("\n");
	}
	if (buf != NULL)
		free(buf);

	if (repeat) {
		printf("\n");
		fflush(stdout);
		sleep(repeat);
		goto again;
	}
}

static struct in6_nbrinfo *
getnbrinfo(addr, ifindex, warning)
	struct in6_addr *addr;
	int ifindex;
	int warning;
{
	static struct in6_nbrinfo nbi;
	int s;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");

	bzero(&nbi, sizeof(nbi));
	if_indextoname(ifindex, nbi.ifname);
	nbi.addr = *addr;
	if (ioctl(s, SIOCGNBRINFO_IN6, (caddr_t)&nbi) < 0) {
		if (warning)
			warn("ioctl(SIOCGNBRINFO_IN6)");
		close(s);
		return(NULL);
	}

	close(s);
	return(&nbi);
}

static char *
ether_str(sdl)
	struct sockaddr_dl *sdl;
{
	static char hbuf[NI_MAXHOST];
	u_char *cp;

	if (sdl->sdl_alen) {
		cp = (u_char *)LLADDR(sdl);
		snprintf(hbuf, sizeof(hbuf), "%x:%x:%x:%x:%x:%x",
		    cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]);
	} else
		snprintf(hbuf, sizeof(hbuf), "(incomplete)");

	return(hbuf);
}

int
ndp_ether_aton(a, n)
	char *a;
	u_char *n;
{
	int i, o[6];

	i = sscanf(a, "%x:%x:%x:%x:%x:%x", &o[0], &o[1], &o[2],
	    &o[3], &o[4], &o[5]);
	if (i != 6) {
		fprintf(stderr, "ndp: invalid Ethernet address '%s'\n", a);
		return (1);
	}
	for (i = 0; i < 6; i++)
		n[i] = o[i];
	return (0);
}

void
usage()
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

int
rtmsg(cmd)
	int cmd;
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
	rtm->rtm_flags = flags;
	rtm->rtm_version = RTM_VERSION;

	switch (cmd) {
	default:
		fprintf(stderr, "ndp: internal wrong cmd\n");
		exit(1);
	case RTM_ADD:
		rtm->rtm_addrs |= RTA_GATEWAY;
		if (expire_time) {
			rtm->rtm_rmx.rmx_expire = expire_time;
			rtm->rtm_inits = RTV_EXPIRE;
		}
		rtm->rtm_flags |= (RTF_HOST | RTF_STATIC);
#if 0 /* we don't support ipv6addr/128 type proxying */
		if (rtm->rtm_flags & RTF_ANNOUNCE) {
			rtm->rtm_flags &= ~RTF_HOST;
			rtm->rtm_addrs |= RTA_NETMASK;
		}
#endif
		/* FALLTHROUGH */
	case RTM_GET:
		rtm->rtm_addrs |= RTA_DST;
	}
#define NEXTADDR(w, s) \
	if (rtm->rtm_addrs & (w)) { \
		bcopy((char *)&s, cp, sizeof(s)); cp += SA_SIZE(&s);}

	NEXTADDR(RTA_DST, sin_m);
	NEXTADDR(RTA_GATEWAY, sdl_m);
#if 0 /* we don't support ipv6addr/128 type proxying */
	memset(&so_mask.sin6_addr, 0xff, sizeof(so_mask.sin6_addr));
	NEXTADDR(RTA_NETMASK, so_mask);
#endif

	rtm->rtm_msglen = cp - (char *)&m_rtmsg;
doit:
	l = rtm->rtm_msglen;
	rtm->rtm_seq = ++seq;
	rtm->rtm_type = cmd;
	if ((rlen = write(s, (char *)&m_rtmsg, l)) < 0) {
		if (errno != ESRCH || cmd != RTM_DELETE) {
			err(1, "writing to routing socket");
			/* NOTREACHED */
		}
	}
	do {
		l = read(s, (char *)&m_rtmsg, sizeof(m_rtmsg));
	} while (l > 0 && (rtm->rtm_seq != seq || rtm->rtm_pid != pid));
	if (l < 0)
		(void) fprintf(stderr, "ndp: read from routing socket: %s\n",
		    strerror(errno));
	return (0);
}

void
ifinfo(ifname, argc, argv)
	char *ifname;
	int argc;
	char **argv;
{
	struct in6_ndireq nd;
	int i, s;
	u_int32_t newflags;
#ifdef IPV6CTL_USETEMPADDR
	u_int8_t nullbuf[8];
#endif

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		err(1, "socket");
		/* NOTREACHED */
	}
	bzero(&nd, sizeof(nd));
	strlcpy(nd.ifname, ifname, sizeof(nd.ifname));
	if (ioctl(s, SIOCGIFINFO_IN6, (caddr_t)&nd) < 0) {
		err(1, "ioctl(SIOCGIFINFO_IN6)");
		/* NOTREACHED */
	}
#define ND nd.ndi
	newflags = ND.flags;
	for (i = 0; i < argc; i++) {
		int clear = 0;
		char *cp = argv[i];

		if (*cp == '-') {
			clear = 1;
			cp++;
		}

#define SETFLAG(s, f) \
	do {\
		if (strcmp(cp, (s)) == 0) {\
			if (clear)\
				newflags &= ~(f);\
			else\
				newflags |= (f);\
		}\
	} while (0)
/*
 * XXX: this macro is not 100% correct, in that it matches "nud" against
 *      "nudbogus".  But we just let it go since this is minor.
 */
#define SETVALUE(f, v) \
	do { \
		char *valptr; \
		unsigned long newval; \
		v = 0; /* unspecified */ \
		if (strncmp(cp, f, strlen(f)) == 0) { \
			valptr = strchr(cp, '='); \
			if (valptr == NULL) \
				err(1, "syntax error in %s field", (f)); \
			errno = 0; \
			newval = strtoul(++valptr, NULL, 0); \
			if (errno) \
				err(1, "syntax error in %s's value", (f)); \
			v = newval; \
		} \
	} while (0)

		SETFLAG("nud", ND6_IFF_PERFORMNUD);
#ifdef ND6_IFF_ACCEPT_RTADV
		SETFLAG("accept_rtadv", ND6_IFF_ACCEPT_RTADV);
#endif
#ifdef ND6_IFF_PREFER_SOURCE
		SETFLAG("prefer_source", ND6_IFF_PREFER_SOURCE);
#endif
		SETVALUE("basereachable", ND.basereachable);
		SETVALUE("retrans", ND.retrans);
		SETVALUE("curhlim", ND.chlim);

		ND.flags = newflags;
		if (ioctl(s, SIOCSIFINFO_IN6, (caddr_t)&nd) < 0) {
			err(1, "ioctl(SIOCSIFINFO_IN6)");
			/* NOTREACHED */
		}
#undef SETFLAG
#undef SETVALUE
	}

	if (!ND.initialized) {
		errx(1, "%s: not initialized yet", ifname);
		/* NOTREACHED */
	}

	if (ioctl(s, SIOCGIFINFO_IN6, (caddr_t)&nd) < 0) {
		err(1, "ioctl(SIOCGIFINFO_IN6)");
		/* NOTREACHED */
	}
	printf("linkmtu=%d", ND.linkmtu);
	printf(", maxmtu=%d", ND.maxmtu);
	printf(", curhlim=%d", ND.chlim);
	printf(", basereachable=%ds%dms",
	    ND.basereachable / 1000, ND.basereachable % 1000);
	printf(", reachable=%ds", ND.reachable);
	printf(", retrans=%ds%dms", ND.retrans / 1000, ND.retrans % 1000);
#ifdef IPV6CTL_USETEMPADDR
	memset(nullbuf, 0, sizeof(nullbuf));
	if (memcmp(nullbuf, ND.randomid, sizeof(nullbuf)) != 0) {
		int j;
		u_int8_t *rbuf;

		for (i = 0; i < 3; i++) {
			switch (i) {
			case 0:
				printf("\nRandom seed(0): ");
				rbuf = ND.randomseed0;
				break;
			case 1:
				printf("\nRandom seed(1): ");
				rbuf = ND.randomseed1;
				break;
			case 2:
				printf("\nRandom ID:      ");
				rbuf = ND.randomid;
				break;
			default:
				errx(1, "impossible case for tempaddr display");
			}
			for (j = 0; j < 8; j++)
				printf("%02x", rbuf[j]);
		}
	}
#endif
	if (ND.flags) {
		printf("\nFlags: ");
		if ((ND.flags & ND6_IFF_PERFORMNUD))
			printf("nud ");
#ifdef ND6_IFF_ACCEPT_RTADV
		if ((ND.flags & ND6_IFF_ACCEPT_RTADV))
			printf("accept_rtadv ");
#endif
#ifdef ND6_IFF_PREFER_SOURCE
		if ((ND.flags & ND6_IFF_PREFER_SOURCE))
			printf("prefer_source ");
#endif
	}
	putc('\n', stdout);
#undef ND

	close(s);
}

#ifndef ND_RA_FLAG_RTPREF_MASK	/* XXX: just for compilation on *BSD release */
#define ND_RA_FLAG_RTPREF_MASK	0x18 /* 00011000 */
#endif

void
rtrlist()
{
#ifdef ICMPV6CTL_ND6_DRLIST
	int mib[] = { CTL_NET, PF_INET6, IPPROTO_ICMPV6, ICMPV6CTL_ND6_DRLIST };
	char *buf;
	struct in6_defrouter *p, *ep;
	size_t l;
	struct timeval time;

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), NULL, &l, NULL, 0) < 0) {
		err(1, "sysctl(ICMPV6CTL_ND6_DRLIST)");
		/*NOTREACHED*/
	}
	if (l == 0)
		return;
	buf = malloc(l);
	if (!buf) {
		err(1, "malloc");
		/*NOTREACHED*/
	}
	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), buf, &l, NULL, 0) < 0) {
		err(1, "sysctl(ICMPV6CTL_ND6_DRLIST)");
		/*NOTREACHED*/
	}

	ep = (struct in6_defrouter *)(buf + l);
	for (p = (struct in6_defrouter *)buf; p < ep; p++) {
		int rtpref;

		if (getnameinfo((struct sockaddr *)&p->rtaddr,
		    p->rtaddr.sin6_len, host_buf, sizeof(host_buf), NULL, 0,
		    (nflag ? NI_NUMERICHOST : 0)) != 0)
			strlcpy(host_buf, "?", sizeof(host_buf));

		printf("%s if=%s", host_buf,
		    if_indextoname(p->if_index, ifix_buf));
		printf(", flags=%s%s",
		    p->flags & ND_RA_FLAG_MANAGED ? "M" : "",
		    p->flags & ND_RA_FLAG_OTHER   ? "O" : "");
		rtpref = ((p->flags & ND_RA_FLAG_RTPREF_MASK) >> 3) & 0xff;
		printf(", pref=%s", rtpref_str[rtpref]);

		gettimeofday(&time, 0);
		if (p->expire == 0)
			printf(", expire=Never\n");
		else
			printf(", expire=%s\n",
			    sec2str(p->expire - time.tv_sec));
	}
	free(buf);
#else
	struct in6_drlist dr;
	int s, i;
	struct timeval time;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		err(1, "socket");
		/* NOTREACHED */
	}
	bzero(&dr, sizeof(dr));
	strlcpy(dr.ifname, "lo0", sizeof(dr.ifname)); /* dummy */
	if (ioctl(s, SIOCGDRLST_IN6, (caddr_t)&dr) < 0) {
		err(1, "ioctl(SIOCGDRLST_IN6)");
		/* NOTREACHED */
	}
#define DR dr.defrouter[i]
	for (i = 0 ; DR.if_index && i < DRLSTSIZ ; i++) {
		struct sockaddr_in6 sin6;

		bzero(&sin6, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_len = sizeof(sin6);
		sin6.sin6_addr = DR.rtaddr;
		getnameinfo((struct sockaddr *)&sin6, sin6.sin6_len, host_buf,
		    sizeof(host_buf), NULL, 0,
		    (nflag ? NI_NUMERICHOST : 0));

		printf("%s if=%s", host_buf,
		    if_indextoname(DR.if_index, ifix_buf));
		printf(", flags=%s%s",
		    DR.flags & ND_RA_FLAG_MANAGED ? "M" : "",
		    DR.flags & ND_RA_FLAG_OTHER   ? "O" : "");
		gettimeofday(&time, 0);
		if (DR.expire == 0)
			printf(", expire=Never\n");
		else
			printf(", expire=%s\n",
			    sec2str(DR.expire - time.tv_sec));
	}
#undef DR
	close(s);
#endif
}

void
plist()
{
#ifdef ICMPV6CTL_ND6_PRLIST
	int mib[] = { CTL_NET, PF_INET6, IPPROTO_ICMPV6, ICMPV6CTL_ND6_PRLIST };
	char *buf;
	struct in6_prefix *p, *ep, *n;
	struct sockaddr_in6 *advrtr;
	size_t l;
	struct timeval time;
	const int niflags = NI_NUMERICHOST;
	int ninflags = nflag ? NI_NUMERICHOST : 0;
	char namebuf[NI_MAXHOST];

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), NULL, &l, NULL, 0) < 0) {
		err(1, "sysctl(ICMPV6CTL_ND6_PRLIST)");
		/*NOTREACHED*/
	}
	buf = malloc(l);
	if (!buf) {
		err(1, "malloc");
		/*NOTREACHED*/
	}
	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), buf, &l, NULL, 0) < 0) {
		err(1, "sysctl(ICMPV6CTL_ND6_PRLIST)");
		/*NOTREACHED*/
	}

	ep = (struct in6_prefix *)(buf + l);
	for (p = (struct in6_prefix *)buf; p < ep; p = n) {
		advrtr = (struct sockaddr_in6 *)(p + 1);
		n = (struct in6_prefix *)&advrtr[p->advrtrs];

		if (getnameinfo((struct sockaddr *)&p->prefix,
		    p->prefix.sin6_len, namebuf, sizeof(namebuf),
		    NULL, 0, niflags) != 0)
			strlcpy(namebuf, "?", sizeof(namebuf));
		printf("%s/%d if=%s\n", namebuf, p->prefixlen,
		    if_indextoname(p->if_index, ifix_buf));

		gettimeofday(&time, 0);
		/*
		 * meaning of fields, especially flags, is very different
		 * by origin.  notify the difference to the users.
		 */
		printf("flags=%s%s%s%s%s",
		    p->raflags.onlink ? "L" : "",
		    p->raflags.autonomous ? "A" : "",
		    (p->flags & NDPRF_ONLINK) != 0 ? "O" : "",
		    (p->flags & NDPRF_DETACHED) != 0 ? "D" : "",
#ifdef NDPRF_HOME
		    (p->flags & NDPRF_HOME) != 0 ? "H" : ""
#else
		    ""
#endif
		    );
		if (p->vltime == ND6_INFINITE_LIFETIME)
			printf(" vltime=infinity");
		else
			printf(" vltime=%lu", (unsigned long)p->vltime);
		if (p->pltime == ND6_INFINITE_LIFETIME)
			printf(", pltime=infinity");
		else
			printf(", pltime=%lu", (unsigned long)p->pltime);
		if (p->expire == 0)
			printf(", expire=Never");
		else if (p->expire >= time.tv_sec)
			printf(", expire=%s",
			    sec2str(p->expire - time.tv_sec));
		else
			printf(", expired");
		printf(", ref=%d", p->refcnt);
		printf("\n");
		/*
		 * "advertising router" list is meaningful only if the prefix
		 * information is from RA.
		 */
		if (p->advrtrs) {
			int j;
			struct sockaddr_in6 *sin6;

			sin6 = advrtr;
			printf("  advertised by\n");
			for (j = 0; j < p->advrtrs; j++) {
				struct in6_nbrinfo *nbi;

				if (getnameinfo((struct sockaddr *)sin6,
				    sin6->sin6_len, namebuf, sizeof(namebuf),
				    NULL, 0, ninflags) != 0)
					strlcpy(namebuf, "?", sizeof(namebuf));
				printf("    %s", namebuf);

				nbi = getnbrinfo(&sin6->sin6_addr,
				    p->if_index, 0);
				if (nbi) {
					switch (nbi->state) {
					case ND6_LLINFO_REACHABLE:
					case ND6_LLINFO_STALE:
					case ND6_LLINFO_DELAY:
					case ND6_LLINFO_PROBE:
						printf(" (reachable)\n");
						break;
					default:
						printf(" (unreachable)\n");
					}
				} else
					printf(" (no neighbor state)\n");
				sin6++;
			}
		} else
			printf("  No advertising router\n");
	}
	free(buf);
#else
	struct in6_prlist pr;
	int s, i;
	struct timeval time;

	gettimeofday(&time, 0);

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		err(1, "socket");
		/* NOTREACHED */
	}
	bzero(&pr, sizeof(pr));
	strlcpy(pr.ifname, "lo0", sizeof(pr.ifname)); /* dummy */
	if (ioctl(s, SIOCGPRLST_IN6, (caddr_t)&pr) < 0) {
		err(1, "ioctl(SIOCGPRLST_IN6)");
		/* NOTREACHED */
	}
#define PR pr.prefix[i]
	for (i = 0; PR.if_index && i < PRLSTSIZ ; i++) {
		struct sockaddr_in6 p6;
		char namebuf[NI_MAXHOST];
		int niflags;

#ifdef NDPRF_ONLINK
		p6 = PR.prefix;
#else
		memset(&p6, 0, sizeof(p6));
		p6.sin6_family = AF_INET6;
		p6.sin6_len = sizeof(p6);
		p6.sin6_addr = PR.prefix;
#endif

		/*
		 * copy link index to sin6_scope_id field.
		 * XXX: KAME specific.
		 */
		if (IN6_IS_ADDR_LINKLOCAL(&p6.sin6_addr)) {
			u_int16_t linkid;

			memcpy(&linkid, &p6.sin6_addr.s6_addr[2],
			    sizeof(linkid));
			linkid = ntohs(linkid);
			p6.sin6_scope_id = linkid;
			p6.sin6_addr.s6_addr[2] = 0;
			p6.sin6_addr.s6_addr[3] = 0;
		}

		niflags = NI_NUMERICHOST;
		if (getnameinfo((struct sockaddr *)&p6,
		    sizeof(p6), namebuf, sizeof(namebuf),
		    NULL, 0, niflags)) {
			warnx("getnameinfo failed");
			continue;
		}
		printf("%s/%d if=%s\n", namebuf, PR.prefixlen,
		    if_indextoname(PR.if_index, ifix_buf));

		gettimeofday(&time, 0);
		/*
		 * meaning of fields, especially flags, is very different
		 * by origin.  notify the difference to the users.
		 */
#if 0
		printf("  %s",
		    PR.origin == PR_ORIG_RA ? "" : "advertise: ");
#endif
#ifdef NDPRF_ONLINK
		printf("flags=%s%s%s%s%s",
		    PR.raflags.onlink ? "L" : "",
		    PR.raflags.autonomous ? "A" : "",
		    (PR.flags & NDPRF_ONLINK) != 0 ? "O" : "",
		    (PR.flags & NDPRF_DETACHED) != 0 ? "D" : "",
#ifdef NDPRF_HOME
		    (PR.flags & NDPRF_HOME) != 0 ? "H" : ""
#else
		    ""
#endif
		    );
#else
		printf("flags=%s%s",
		    PR.raflags.onlink ? "L" : "",
		    PR.raflags.autonomous ? "A" : "");
#endif
		if (PR.vltime == ND6_INFINITE_LIFETIME)
			printf(" vltime=infinity");
		else
			printf(" vltime=%lu", PR.vltime);
		if (PR.pltime == ND6_INFINITE_LIFETIME)
			printf(", pltime=infinity");
		else
			printf(", pltime=%lu", PR.pltime);
		if (PR.expire == 0)
			printf(", expire=Never");
		else if (PR.expire >= time.tv_sec)
			printf(", expire=%s",
			    sec2str(PR.expire - time.tv_sec));
		else
			printf(", expired");
#ifdef NDPRF_ONLINK
		printf(", ref=%d", PR.refcnt);
#endif
#if 0
		switch (PR.origin) {
		case PR_ORIG_RA:
			printf(", origin=RA");
			break;
		case PR_ORIG_RR:
			printf(", origin=RR");
			break;
		case PR_ORIG_STATIC:
			printf(", origin=static");
			break;
		case PR_ORIG_KERNEL:
			printf(", origin=kernel");
			break;
		default:
			printf(", origin=?");
			break;
		}
#endif
		printf("\n");
		/*
		 * "advertising router" list is meaningful only if the prefix
		 * information is from RA.
		 */
		if (0 &&	/* prefix origin is almost obsolted */
		    PR.origin != PR_ORIG_RA)
			;
		else if (PR.advrtrs) {
			int j;
			printf("  advertised by\n");
			for (j = 0; j < PR.advrtrs; j++) {
				struct sockaddr_in6 sin6;
				struct in6_nbrinfo *nbi;

				bzero(&sin6, sizeof(sin6));
				sin6.sin6_family = AF_INET6;
				sin6.sin6_len = sizeof(sin6);
				sin6.sin6_addr = PR.advrtr[j];
				sin6.sin6_scope_id = PR.if_index; /* XXX */
				getnameinfo((struct sockaddr *)&sin6,
				    sin6.sin6_len, host_buf,
				    sizeof(host_buf), NULL, 0,
				    (nflag ? NI_NUMERICHOST : 0));
				printf("    %s", host_buf);

				nbi = getnbrinfo(&sin6.sin6_addr,
				    PR.if_index, 0);
				if (nbi) {
					switch (nbi->state) {
					case ND6_LLINFO_REACHABLE:
					case ND6_LLINFO_STALE:
					case ND6_LLINFO_DELAY:
					case ND6_LLINFO_PROBE:
						 printf(" (reachable)\n");
						 break;
					default:
						 printf(" (unreachable)\n");
					}
				} else
					printf(" (no neighbor state)\n");
			}
			if (PR.advrtrs > DRLSTSIZ)
				printf("    and %d routers\n",
				    PR.advrtrs - DRLSTSIZ);
		} else
			printf("  No advertising router\n");
	}
#undef PR
	close(s);
#endif
}

void
pfx_flush()
{
	char dummyif[IFNAMSIZ+8];
	int s;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");
	strlcpy(dummyif, "lo0", sizeof(dummyif)); /* dummy */
	if (ioctl(s, SIOCSPFXFLUSH_IN6, (caddr_t)&dummyif) < 0)
		err(1, "ioctl(SIOCSPFXFLUSH_IN6)");
}

void
rtr_flush()
{
	char dummyif[IFNAMSIZ+8];
	int s;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");
	strlcpy(dummyif, "lo0", sizeof(dummyif)); /* dummy */
	if (ioctl(s, SIOCSRTRFLUSH_IN6, (caddr_t)&dummyif) < 0)
		err(1, "ioctl(SIOCSRTRFLUSH_IN6)");

	close(s);
}

void
harmonize_rtr()
{
	char dummyif[IFNAMSIZ+8];
	int s;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");
	strlcpy(dummyif, "lo0", sizeof(dummyif)); /* dummy */
	if (ioctl(s, SIOCSNDFLUSH_IN6, (caddr_t)&dummyif) < 0)
		err(1, "ioctl(SIOCSNDFLUSH_IN6)");

	close(s);
}

#ifdef SIOCSDEFIFACE_IN6	/* XXX: check SIOCGDEFIFACE_IN6 as well? */
static void
setdefif(ifname)
	char *ifname;
{
	struct in6_ndifreq ndifreq;
	unsigned int ifindex;

	if (strcasecmp(ifname, "delete") == 0)
		ifindex = 0;
	else {
		if ((ifindex = if_nametoindex(ifname)) == 0)
			err(1, "failed to resolve i/f index for %s", ifname);
	}

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");

	strlcpy(ndifreq.ifname, "lo0", sizeof(ndifreq.ifname)); /* dummy */
	ndifreq.ifindex = ifindex;

	if (ioctl(s, SIOCSDEFIFACE_IN6, (caddr_t)&ndifreq) < 0)
		err(1, "ioctl(SIOCSDEFIFACE_IN6)");

	close(s);
}

static void
getdefif()
{
	struct in6_ndifreq ndifreq;
	char ifname[IFNAMSIZ+8];

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");

	memset(&ndifreq, 0, sizeof(ndifreq));
	strlcpy(ndifreq.ifname, "lo0", sizeof(ndifreq.ifname)); /* dummy */

	if (ioctl(s, SIOCGDEFIFACE_IN6, (caddr_t)&ndifreq) < 0)
		err(1, "ioctl(SIOCGDEFIFACE_IN6)");

	if (ndifreq.ifindex == 0)
		printf("No default interface.\n");
	else {
		if ((if_indextoname(ndifreq.ifindex, ifname)) == NULL)
			err(1, "failed to resolve ifname for index %lu",
			    ndifreq.ifindex);
		printf("ND default interface = %s\n", ifname);
	}

	close(s);
}
#endif

static char *
sec2str(total)
	time_t total;
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
static void
ts_print(tvp)
	const struct timeval *tvp;
{
	int s;

	/* Default */
	s = (tvp->tv_sec + thiszone) % 86400;
	(void)printf("%02d:%02d:%02d.%06u ",
	    s / 3600, (s % 3600) / 60, s % 60, (u_int32_t)tvp->tv_usec);
}
