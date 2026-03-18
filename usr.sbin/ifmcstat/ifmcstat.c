/*	$KAME: ifmcstat.c,v 1.48 2006/11/15 05:13:59 itojun Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2007-2009 Bruce Simpson.
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/tree.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/igmp.h>
#include <netinet/if_ether.h>
#include <netinet/igmp_var.h>

#ifdef INET6
#include <netinet/icmp6.h>
#include <netinet6/mld6_var.h>
#endif /* INET6 */

#include <arpa/inet.h>
#include <netdb.h>

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <ifaddrs.h>
#include <sysexits.h>
#include <unistd.h>

/* XXX: This file currently assumes INET support in the base system. */
#ifndef INET
#define INET
#endif

extern void	printb(const char *, unsigned int, const char *);

union sockunion {
	struct sockaddr_storage	ss;
	struct sockaddr		sa;
	struct sockaddr_dl	sdl;
#ifdef INET
	struct sockaddr_in	sin;
#endif
#ifdef INET6
	struct sockaddr_in6	sin6;
#endif
};
typedef union sockunion sockunion_t;

uint32_t	ifindex = 0;
int		af = AF_UNSPEC;
int		vflag = 0;

#define	sa_dl_equal(a1, a2)	\
	((((struct sockaddr_dl *)(a1))->sdl_len ==			\
	 ((struct sockaddr_dl *)(a2))->sdl_len) &&			\
	 (bcmp(LLADDR((struct sockaddr_dl *)(a1)),			\
	       LLADDR((struct sockaddr_dl *)(a2)),			\
	       ((struct sockaddr_dl *)(a1))->sdl_alen) == 0))

static int		ifmcstat_getifmaddrs(void);
#ifdef INET
static void		in_ifinfo(struct igmp_ifinfo *);
static const char *	inm_mode(u_int mode);
#endif
#ifdef INET6
static void		in6_ifinfo(struct mld_ifinfo *);
static const char *	inet6_n2a(struct in6_addr *, uint32_t);
#endif
int			main(int, char **);

static void
usage()
{

	fprintf(stderr,
	    "usage: ifmcstat [-i interface] [-f address family] [-v]\n");
	exit(EX_USAGE);
}

static const char *options = "i:f:vM:N:";

int
main(int argc, char **argv)
{
	int c, error;

	while ((c = getopt(argc, argv, options)) != -1) {
		switch (c) {
		case 'i':
			if ((ifindex = if_nametoindex(optarg)) == 0) {
				fprintf(stderr, "%s: unknown interface\n",
				    optarg);
				exit(EX_NOHOST);
			}
			break;

		case 'f':
#ifdef INET
			if (strcmp(optarg, "inet") == 0) {
				af = AF_INET;
				break;
			}
#endif
#ifdef INET6
			if (strcmp(optarg, "inet6") == 0) {
				af = AF_INET6;
				break;
			}
#endif
			if (strcmp(optarg, "link") == 0) {
				af = AF_LINK;
				break;
			}
			fprintf(stderr, "%s: unknown address family\n", optarg);
			exit(EX_USAGE);
			/*NOTREACHED*/
			break;

		case 'v':
			++vflag;
			break;

		default:
			usage();
			break;
			/*NOTREACHED*/
		}
	}

	if (af == AF_LINK && vflag)
		usage();

	error = ifmcstat_getifmaddrs();
	if (error != 0)
		exit(EX_OSERR);

	exit(EX_OK);
	/*NOTREACHED*/
}

#ifdef INET

static void
in_ifinfo(struct igmp_ifinfo *igi)
{

	printf("\t");
	switch (igi->igi_version) {
	case IGMP_VERSION_1:
	case IGMP_VERSION_2:
	case IGMP_VERSION_3:
		printf("igmpv%d", igi->igi_version);
		break;
	default:
		printf("igmpv?(%d)", igi->igi_version);
		break;
	}
	if (igi->igi_flags)
		printb(" flags", igi->igi_flags, "\020\1SILENT\2LOOPBACK");
	if (igi->igi_version == IGMP_VERSION_3) {
		printf(" rv %u qi %u qri %u uri %u",
		    igi->igi_rv, igi->igi_qi, igi->igi_qri, igi->igi_uri);
	}
	if (vflag >= 2) {
		printf(" v1timer %u v2timer %u v3timer %u",
		    igi->igi_v1_timer, igi->igi_v2_timer, igi->igi_v3_timer);
	}
	printf("\n");
}

static const char *inm_modes[] = {
	"undefined",
	"include",
	"exclude",
};

static const char *
inm_mode(u_int mode)
{

	if (mode >= MCAST_UNDEFINED && mode <= MCAST_EXCLUDE)
		return (inm_modes[mode]);
	return (NULL);
}

#endif /* INET */

#ifdef INET6

static void
in6_ifinfo(struct mld_ifinfo *mli)
{

	printf("\t");
	switch (mli->mli_version) {
	case MLD_VERSION_1:
	case MLD_VERSION_2:
		printf("mldv%d", mli->mli_version);
		break;
	default:
		printf("mldv?(%d)", mli->mli_version);
		break;
	}
	if (mli->mli_flags)
		printb(" flags", mli->mli_flags, "\020\1SILENT\2USEALLOW");
	if (mli->mli_version == MLD_VERSION_2) {
		printf(" rv %u qi %u qri %u uri %u",
		    mli->mli_rv, mli->mli_qi, mli->mli_qri, mli->mli_uri);
	}
	if (vflag >= 2) {
		printf(" v1timer %u v2timer %u", mli->mli_v1_timer,
		   mli->mli_v2_timer);
	}
	printf("\n");
}

static const char *
inet6_n2a(struct in6_addr *p, uint32_t scope_id)
{
	static char buf[NI_MAXHOST];
	struct sockaddr_in6 sin6;
	const int niflags = NI_NUMERICHOST;

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_addr = *p;
	sin6.sin6_scope_id = scope_id;
	if (getnameinfo((struct sockaddr *)&sin6, sin6.sin6_len,
	    buf, sizeof(buf), NULL, 0, niflags) == 0) {
		return (buf);
	} else {
		return ("(invalid)");
	}
}
#endif /* INET6 */

#ifdef INET
/*
 * Retrieve per-group source filter mode and lists via sysctl.
 */
static void
inm_print_sources_sysctl(uint32_t ifindex, struct in_addr gina)
{
#define	MAX_SYSCTL_TRY	5
	int mib[7];
	int ntry = 0;
	size_t mibsize;
	size_t len;
	size_t needed;
	size_t cnt;
	int i;
	char *buf;
	struct in_addr *pina;
	uint32_t *p;
	uint32_t fmode;
	const char *modestr;

	mibsize = nitems(mib);
	if (sysctlnametomib("net.inet.ip.mcast.filters", mib, &mibsize) == -1) {
		perror("sysctlnametomib");
		return;
	}

	needed = 0;
	mib[5] = ifindex;
	mib[6] = gina.s_addr;	/* 32 bits wide */
	mibsize = nitems(mib);
	do {
		if (sysctl(mib, mibsize, NULL, &needed, NULL, 0) == -1) {
			perror("sysctl net.inet.ip.mcast.filters");
			return;
		}
		if ((buf = malloc(needed)) == NULL) {
			perror("malloc");
			return;
		}
		if (sysctl(mib, mibsize, buf, &needed, NULL, 0) == -1) {
			if (errno != ENOMEM || ++ntry >= MAX_SYSCTL_TRY) {
				perror("sysctl");
				goto out_free;
			}
			free(buf);
			buf = NULL;
		} 
	} while (buf == NULL);

	len = needed;
	if (len < sizeof(uint32_t)) {
		perror("sysctl");
		goto out_free;
	}

	p = (uint32_t *)buf;
	fmode = *p++;
	len -= sizeof(uint32_t);

	modestr = inm_mode(fmode);
	if (modestr)
		printf(" mode %s", modestr);
	else
		printf(" mode (%u)", fmode);

	if (vflag == 0)
		goto out_free;

	cnt = len / sizeof(struct in_addr);
	pina = (struct in_addr *)p;

	for (i = 0; i < cnt; i++) {
		if (i == 0)
			printf(" srcs ");
		fprintf(stdout, "%s%s", (i == 0 ? "" : ","),
		    inet_ntoa(*pina++));
		len -= sizeof(struct in_addr);
	}
	if (len > 0) {
		fprintf(stderr, "warning: %u trailing bytes from %s\n",
		    (unsigned int)len, "net.inet.ip.mcast.filters");
	}

out_free:
	free(buf);
#undef	MAX_SYSCTL_TRY
}

#endif /* INET */

#ifdef INET6
/*
 * Retrieve MLD per-group source filter mode and lists via sysctl.
 *
 * Note: The 128-bit IPv6 group address needs to be segmented into
 * 32-bit pieces for marshaling to sysctl. So the MIB name ends
 * up looking like this:
 *  a.b.c.d.e.ifindex.g[0].g[1].g[2].g[3]
 * Assumes that pgroup originated from the kernel, so its components
 * are already in network-byte order.
 */
static void
in6m_print_sources_sysctl(uint32_t ifindex, struct in6_addr *pgroup)
{
#define	MAX_SYSCTL_TRY	5
	char addrbuf[INET6_ADDRSTRLEN];
	int mib[10];
	int ntry = 0;
	int *pi;
	size_t mibsize;
	size_t len;
	size_t needed;
	size_t cnt;
	int i;
	char *buf;
	struct in6_addr *pina;
	uint32_t *p;
	uint32_t fmode;
	const char *modestr;

	mibsize = nitems(mib);
	if (sysctlnametomib("net.inet6.ip6.mcast.filters", mib,
	    &mibsize) == -1) {
		perror("sysctlnametomib");
		return;
	}

	needed = 0;
	mib[5] = ifindex;
	pi = (int *)pgroup;
	for (i = 0; i < 4; i++)
		mib[6 + i] = *pi++;

	mibsize = nitems(mib);
	do {
		if (sysctl(mib, mibsize, NULL, &needed, NULL, 0) == -1) {
			perror("sysctl net.inet6.ip6.mcast.filters");
			return;
		}
		if ((buf = malloc(needed)) == NULL) {
			perror("malloc");
			return;
		}
		if (sysctl(mib, mibsize, buf, &needed, NULL, 0) == -1) {
			if (errno != ENOMEM || ++ntry >= MAX_SYSCTL_TRY) {
				perror("sysctl");
				goto out_free;
			}
			free(buf);
			buf = NULL;
		} 
	} while (buf == NULL);

	len = needed;
	if (len < sizeof(uint32_t)) {
		perror("sysctl");
		goto out_free;
	}

	p = (uint32_t *)buf;
	fmode = *p++;
	len -= sizeof(uint32_t);

	modestr = inm_mode(fmode);
	if (modestr)
		printf(" mode %s", modestr);
	else
		printf(" mode (%u)", fmode);

	if (vflag == 0)
		goto out_free;

	cnt = len / sizeof(struct in6_addr);
	pina = (struct in6_addr *)p;

	for (i = 0; i < cnt; i++) {
		if (i == 0)
			printf(" srcs ");
		inet_ntop(AF_INET6, (const char *)pina++, addrbuf,
		    INET6_ADDRSTRLEN);
		fprintf(stdout, "%s%s", (i == 0 ? "" : ","), addrbuf);
		len -= sizeof(struct in6_addr);
	}
	if (len > 0) {
		fprintf(stderr, "warning: %u trailing bytes from %s\n",
		    (unsigned int)len, "net.inet6.ip6.mcast.filters");
	}

out_free:
	free(buf);
#undef	MAX_SYSCTL_TRY
}
#endif /* INET6 */

static int
ifmcstat_getifmaddrs(void)
{
	char			 thisifname[IFNAMSIZ];
	char			 addrbuf[NI_MAXHOST];
	struct ifaddrs		*ifap, *ifa;
	struct ifmaddrs		*ifmap, *ifma;
	sockunion_t		 lastifasa;
	sockunion_t		*psa, *pgsa, *pllsa, *pifasa;
	char			*pcolon;
	char			*pafname;
	uint32_t		 lastifindex, thisifindex;
	int			 error;

	error = 0;
	ifap = NULL;
	ifmap = NULL;
	lastifindex = 0;
	thisifindex = 0;
	lastifasa.ss.ss_family = AF_UNSPEC;

	if (getifaddrs(&ifap) != 0) {
		warn("getifmaddrs");
		return (-1);
	}

	if (getifmaddrs(&ifmap) != 0) {
		warn("getifmaddrs");
		error = -1;
		goto out;
	}

	for (ifma = ifmap; ifma; ifma = ifma->ifma_next) {
		error = 0;
		if (ifma->ifma_name == NULL || ifma->ifma_addr == NULL)
			continue;

		psa = (sockunion_t *)ifma->ifma_name;
		if (psa->sa.sa_family != AF_LINK) {
			fprintf(stderr,
			    "WARNING: Kernel returned invalid data.\n");
			error = -1;
			break;
		}

		/* Filter on interface name. */
		thisifindex = psa->sdl.sdl_index;
		if (ifindex != 0 && thisifindex != ifindex)
			continue;

		/* Filter on address family. */
		pgsa = (sockunion_t *)ifma->ifma_addr;
		if (af != 0 && pgsa->sa.sa_family != af)
			continue;

		strlcpy(thisifname, link_ntoa(&psa->sdl), IFNAMSIZ);
		pcolon = strchr(thisifname, ':');
		if (pcolon)
			*pcolon = '\0';

		/* Only print the banner for the first ifmaddrs entry. */
		if (lastifindex == 0 || lastifindex != thisifindex) {
			lastifindex = thisifindex;
			fprintf(stdout, "%s:\n", thisifname);
		}

		/*
		 * Currently, multicast joins only take place on the
		 * primary IPv4 address, and only on the link-local IPv6
		 * address, as per IGMPv2/3 and MLDv1/2 semantics.
		 * Therefore, we only look up the primary address on
		 * the first pass.
		 */
		pifasa = NULL;
		for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
			if ((strcmp(ifa->ifa_name, thisifname) != 0) ||
			    (ifa->ifa_addr == NULL) ||
			    (ifa->ifa_addr->sa_family != pgsa->sa.sa_family))
				continue;
			/*
			 * For AF_INET6 only the link-local address should
			 * be returned. If built without IPv6 support,
			 * skip this address entirely.
			 */
			pifasa = (sockunion_t *)ifa->ifa_addr;
			if (pifasa->sa.sa_family == AF_INET6
#ifdef INET6
			    && !IN6_IS_ADDR_LINKLOCAL(&pifasa->sin6.sin6_addr)
#endif
			) {
				pifasa = NULL;
				continue;
			}
			break;
		}
		if (pifasa == NULL)
			continue;	/* primary address not found */

		if (!vflag && pifasa->sa.sa_family == AF_LINK)
			continue;

		/* Parse and print primary address, if not already printed. */
		if (lastifasa.ss.ss_family == AF_UNSPEC ||
		    ((lastifasa.ss.ss_family == AF_LINK &&
		      !sa_dl_equal(&lastifasa.sa, &pifasa->sa)) ||
		     !sa_equal(&lastifasa.sa, &pifasa->sa))) {

			switch (pifasa->sa.sa_family) {
			case AF_INET:
				pafname = "inet";
				break;
			case AF_INET6:
				pafname = "inet6";
				break;
			case AF_LINK:
				pafname = "link";
				break;
			default:
				pafname = "unknown";
				break;
			}

			switch (pifasa->sa.sa_family) {
			case AF_INET6:
#ifdef INET6
			{
				const char *p =
				    inet6_n2a(&pifasa->sin6.sin6_addr,
					pifasa->sin6.sin6_scope_id);
				strlcpy(addrbuf, p, sizeof(addrbuf));
				break;
			}
#else
			/* FALLTHROUGH */
#endif
			case AF_INET:
			case AF_LINK:
				error = getnameinfo(&pifasa->sa,
				    pifasa->sa.sa_len,
				    addrbuf, sizeof(addrbuf), NULL, 0,
				    NI_NUMERICHOST);
				if (error)
					perror("getnameinfo");
				break;
			default:
				addrbuf[0] = '\0';
				break;
			}

			fprintf(stdout, "\t%s %s", pafname, addrbuf);
#ifdef INET6
			if (pifasa->sa.sa_family == AF_INET6 &&
			    pifasa->sin6.sin6_scope_id)
				fprintf(stdout, " scopeid 0x%x",
				    pifasa->sin6.sin6_scope_id);
#endif
			fprintf(stdout, "\n");
#ifdef INET
			/*
			 * Print per-link IGMP information, if available.
			 */
			if (pifasa->sa.sa_family == AF_INET) {
				struct igmp_ifinfo igi;
				size_t mibsize, len;
				int mib[5];

				mibsize = nitems(mib);
				if (sysctlnametomib("net.inet.igmp.ifinfo",
				    mib, &mibsize) == -1) {
					perror("sysctlnametomib");
					goto next_ifnet;
				}
				mib[mibsize] = thisifindex;
				len = sizeof(struct igmp_ifinfo);
				if (sysctl(mib, mibsize + 1, &igi, &len, NULL,
				    0) == -1) {
					perror("sysctl net.inet.igmp.ifinfo");
					goto next_ifnet;
				}
				in_ifinfo(&igi);
			}
#endif /* INET */
#ifdef INET6
			/*
			 * Print per-link MLD information, if available.
			 */
			if (pifasa->sa.sa_family == AF_INET6) {
				struct mld_ifinfo mli;
				size_t mibsize, len;
				int mib[5];

				mibsize = nitems(mib);
				if (sysctlnametomib("net.inet6.mld.ifinfo",
				    mib, &mibsize) == -1) {
					perror("sysctlnametomib");
					goto next_ifnet;
				}
				mib[mibsize] = thisifindex;
				len = sizeof(struct mld_ifinfo);
				if (sysctl(mib, mibsize + 1, &mli, &len, NULL,
				    0) == -1) {
					perror("sysctl net.inet6.mld.ifinfo");
					goto next_ifnet;
				}
				in6_ifinfo(&mli);
			}
#endif /* INET6 */
#if defined(INET) || defined(INET6)
next_ifnet:
#endif
			lastifasa = *pifasa;
		}

		/* Print this group address. */
#ifdef INET6
		if (pgsa->sa.sa_family == AF_INET6) {
			const char *p = inet6_n2a(&pgsa->sin6.sin6_addr,
			    pgsa->sin6.sin6_scope_id);
			strlcpy(addrbuf, p, sizeof(addrbuf));
		} else
#endif
		{
			error = getnameinfo(&pgsa->sa, pgsa->sa.sa_len,
			    addrbuf, sizeof(addrbuf), NULL, 0, NI_NUMERICHOST);
			if (error)
				perror("getnameinfo");
		}

		fprintf(stdout, "\t\tgroup %s", addrbuf);
#ifdef INET6
		if (pgsa->sa.sa_family == AF_INET6 &&
		    pgsa->sin6.sin6_scope_id)
			fprintf(stdout, " scopeid 0x%x",
			    pgsa->sin6.sin6_scope_id);
#endif
#ifdef INET
		if (pgsa->sa.sa_family == AF_INET) {
			inm_print_sources_sysctl(thisifindex,
			    pgsa->sin.sin_addr);
		}
#endif
#ifdef INET6
		if (pgsa->sa.sa_family == AF_INET6) {
			in6m_print_sources_sysctl(thisifindex,
			    &pgsa->sin6.sin6_addr);
		}
#endif
		fprintf(stdout, "\n");

		/* Link-layer mapping, if present. */
		pllsa = (sockunion_t *)ifma->ifma_lladdr;
		if (pllsa != NULL) {
			error = getnameinfo(&pllsa->sa, pllsa->sa.sa_len,
			    addrbuf, sizeof(addrbuf), NULL, 0, NI_NUMERICHOST);
			fprintf(stdout, "\t\t\tmcast-macaddr %s\n", addrbuf);
		}
	}
out:
	if (ifmap != NULL)
		freeifmaddrs(ifmap);
	if (ifap != NULL)
		freeifaddrs(ifap);

	return (error);
}
