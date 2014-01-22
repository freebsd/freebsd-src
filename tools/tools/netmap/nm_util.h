/*
 * Copyright (C) 2012-2014 Luigi Rizzo. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
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
 */

/*
 * $FreeBSD$
 * $Id$
 *
 * Some utilities to build netmap-based programs.
 */

#ifndef _NM_UTIL_H
#define _NM_UTIL_H

#define _GNU_SOURCE	/* for CPU_SET() */

#include <stdio.h>	/* fprintf */
#include <sys/poll.h>	/* POLLIN */
#include <inttypes.h>	/* PRI* macros */
#include <sys/types.h>	/* u_char */

#include <arpa/inet.h>	/* ntohs */
#include <sys/sysctl.h>	/* sysctl */
#include <ifaddrs.h>	/* getifaddrs */
#include <net/ethernet.h>	/* ETHERTYPE_IP */
#include <netinet/in.h>	/* IPPROTO_* */
#include <netinet/ip.h>	/* struct ip */
#include <netinet/udp.h>	/* struct udp */


#define NETMAP_WITH_LIBS
#include <net/netmap_user.h>

#include <pthread.h>	/* pthread_* */

#ifdef linux

#define cpuset_t	cpu_set_t

#define ifr_flagshigh  ifr_flags	/* only the low 16 bits here */
#define IFF_PPROMISC   IFF_PROMISC	/* IFF_PPROMISC does not exist */
#include <linux/ethtool.h>
#include <linux/sockios.h>

#define CLOCK_REALTIME_PRECISE CLOCK_REALTIME
#include <netinet/ether.h>      /* ether_aton */
#include <linux/if_packet.h>    /* sockaddr_ll */
#endif	/* linux */

#ifdef __FreeBSD__
#include <sys/endian.h> /* le64toh */
#include <machine/param.h>

#include <pthread_np.h> /* pthread w/ affinity */
#include <sys/cpuset.h> /* cpu_set */
#include <net/if_dl.h>  /* LLADDR */
#endif	/* __FreeBSD__ */

#ifdef __APPLE__

#define cpuset_t	uint64_t	// XXX
static inline void CPU_ZERO(cpuset_t *p)
{
	*p = 0;
}

static inline void CPU_SET(uint32_t i, cpuset_t *p)
{
	*p |= 1<< (i & 0x3f);
}

#define pthread_setaffinity_np(a, b, c)	((void)a, 0)

#define ifr_flagshigh  ifr_flags	// XXX
#define IFF_PPROMISC   IFF_PROMISC
#include <net/if_dl.h>  /* LLADDR */
#define clock_gettime(a,b)	\
	do {struct timespec t0 = {0,0};	*(b) = t0; } while (0)
#endif	/* __APPLE__ */

static inline int min(int a, int b) { return a < b ? a : b; }
extern int time_second;

/* debug support */
#define ND(format, ...)	do {} while(0)
#define D(format, ...)					\
	fprintf(stderr, "%s [%d] " format "\n",		\
	__FUNCTION__, __LINE__, ##__VA_ARGS__)

#define RD(lps, format, ...)				\
	do {						\
		static int t0, cnt;			\
		if (t0 != time_second) {		\
			t0 = time_second;		\
			cnt = 0;			\
		}					\
		if (cnt++ < lps)			\
			D(format, ##__VA_ARGS__);	\
	} while (0)



struct nm_desc_t * netmap_open(const char *name, int ringid, int promisc);
int nm_do_ioctl(struct nm_desc_t *me, u_long what, int subcmd);
int pkt_queued(struct nm_desc_t *d, int tx);
#endif /* _NM_UTIL_H */
