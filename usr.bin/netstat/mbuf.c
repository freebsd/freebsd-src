/*
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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

#ifndef lint
#if 0
static char sccsid[] = "@(#)mbuf.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "netstat.h"

#define	YES	1
typedef int bool;

/* XXX: mbtypes stats temporarily disactivated. */
#if 0
static struct mbtypenames {
	int	mt_type;
	char	*mt_name;
} mbtypenames[] = {
	{ MT_DATA,	"data" },
	{ MT_OOBDATA,	"oob data" },
	{ MT_CONTROL,	"ancillary data" },
	{ MT_HEADER,	"packet headers" },
#ifdef MT_SOCKET
	{ MT_SOCKET,	"socket structures" },			/* XXX */
#endif
#ifdef MT_PCB
	{ MT_PCB,	"protocol control blocks" },		/* XXX */
#endif
#ifdef MT_RTABLE
	{ MT_RTABLE,	"routing table entries" },		/* XXX */
#endif
#ifdef MT_HTABLE
	{ MT_HTABLE,	"IMP host table entries" },		/* XXX */
#endif
#ifdef MT_ATABLE
	{ MT_ATABLE,	"address resolution tables" },
#endif
	{ MT_FTABLE,	"fragment reassembly queue headers" },	/* XXX */
	{ MT_SONAME,	"socket names and addresses" },
#ifdef MT_SOOPTS
	{ MT_SOOPTS,	"socket options" },
#endif
#ifdef MT_RIGHTS
	{ MT_RIGHTS,	"access rights" },
#endif
#ifdef MT_IFADDR
	{ MT_IFADDR,	"interface addresses" },		/* XXX */
#endif
	{ 0, 0 }
};
#endif	/* 0 */

/*
 * Print mbuf statistics.
 */
void
mbpr(u_long mbaddr, u_long mbtaddr, u_long nmbcaddr, u_long nmbufaddr,
    u_long mblimaddr, u_long cllimaddr, u_long cpusaddr, u_long pgsaddr,
    u_long mbpaddr)
{
	int i, nmbufs, nmbclusters, ncpu, page_size, num_objs;
	u_int mbuf_limit, clust_limit;
	u_long totspace, totnum, totfree;
	size_t mlen;
	struct mbstat *mbstat = NULL;
	struct mbpstat **mbpstat = NULL;

/* XXX: mbtypes stats temporarily disabled. */
#if 0
	int nmbtypes;
	size_t mbtypeslen;
	struct mbtypenames *mp;
	u_long *mbtypes = NULL;
	bool *seen = NULL;

	/*
	 * XXX
	 * We can't kread() mbtypeslen from a core image so we'll
	 * bogusly assume it's the same as in the running kernel.
	 */
	if (sysctlbyname("kern.ipc.mbtypes", NULL, &mbtypeslen, NULL, 0) < 0) {
		warn("sysctl: retrieving mbtypes length");
		goto err;
	}
	if ((mbtypes = malloc(mbtypeslen)) == NULL) {
		warn("malloc: %lu bytes for mbtypes", (u_long)mbtypeslen);
		goto err;
	}

	nmbtypes = mbtypeslen / sizeof(*mbtypes);
	if ((seen = calloc(nmbtypes, sizeof(*seen))) == NULL) {
		warn("calloc");
		goto err;
	}
#endif

	mlen = sizeof mbstat;
	if ((mbstat = malloc(mlen)) == NULL) {
		warn("malloc: cannot allocate memory for mbstat");
		goto err;
	}

	/*
	 * XXX: Unfortunately, for the time being, we have to fetch
	 * the total length of the per-CPU stats area via sysctl
	 * (regardless of whether we're looking at a core or not.
	 */
	if (sysctlbyname("kern.ipc.mb_statpcpu", NULL, &mlen, NULL, 0) < 0) {
		warn("sysctl: retrieving mb_statpcpu len");
		goto err;
	} 
	num_objs = (int)(mlen / sizeof(struct mbpstat));
	if ((mbpstat = calloc(num_objs, sizeof(struct mbpstat *))) == NULL) {
		warn("calloc: cannot allocate memory for mbpstats pointers");
		goto err;
	}
	if ((mbpstat[0] = calloc(num_objs, sizeof(struct mbpstat))) == NULL) {
		warn("calloc: cannot allocate memory for mbpstats");
		goto err;
	}

	if (mbaddr) {
		if (kread(mbpaddr, (char *)mbpstat[0], mlen))
			goto err; 
		if (kread(mbaddr, (char *)mbstat, sizeof mbstat))
			goto err;
#if 0
		if (kread(mbtaddr, (char *)mbtypes, mbtypeslen))
			goto err;
#endif
		if (kread(nmbcaddr, (char *)&nmbclusters, sizeof(int)))
			goto err;
		if (kread(nmbufaddr, (char *)&nmbufs, sizeof(int)))
			goto err;
		if (kread(mblimaddr, (char *)&mbuf_limit, sizeof(u_int)))
			goto err;
		if (kread(cllimaddr, (char *)&clust_limit, sizeof(u_int)))
			goto err;
		if (kread(cpusaddr, (char *)&ncpu, sizeof(int)))
			goto err;
		if (kread(pgsaddr, (char *)&page_size, sizeof(int)))
			goto err;
	} else {
		if (sysctlbyname("kern.ipc.mb_statpcpu", mbpstat[0], &mlen,
		    NULL, 0) < 0) {
			warn("sysctl: retrieving mb_statpcpu");
			goto err;
		}
		if (sysctlbyname("kern.ipc.mbstat", mbstat, &mlen, NULL, 0)
		    < 0) {
			warn("sysctl: retrieving mbstat");
			goto err;
		}
#if 0
		if (sysctlbyname("kern.ipc.mbtypes", mbtypes, &mbtypeslen, NULL,
		    0) < 0) {
			warn("sysctl: retrieving mbtypes");
			goto err;
		}
#endif
		mlen = sizeof(int);
		if (sysctlbyname("kern.ipc.nmbclusters", &nmbclusters, &mlen,
		    NULL, 0) < 0) {
			warn("sysctl: retrieving nmbclusters");
			goto err;
		}
		mlen = sizeof(int);
		if (sysctlbyname("kern.ipc.nmbufs", &nmbufs, &mlen, NULL, 0)
		    < 0) {
			warn("sysctl: retrieving nmbufs");
			goto err;
		}
		mlen = sizeof(u_int);
		if (sysctlbyname("kern.ipc.mbuf_limit", &mbuf_limit, &mlen,
		    NULL, 0) < 0) {
			warn("sysctl: retrieving mbuf_limit");
			goto err;
		}
		mlen = sizeof(u_int);
		if (sysctlbyname("kern.ipc.clust_limit", &clust_limit, &mlen,
		    NULL, 0) < 0) {
			warn("sysctl: retrieving clust_limit");
			goto err;
		}
		mlen = sizeof(int);
		if (sysctlbyname("kern.smp.cpus", &ncpu, &mlen, NULL, 0) < 0) {
			warn("sysctl: retrieving kern.smp.cpus");
			goto err;
		}
		mlen = sizeof(int);
		if (sysctlbyname("hw.pagesize", &page_size, &mlen, NULL, 0)
		    < 0) {
			warn("sysctl: retrieving hw.pagesize");
			goto err;
		}
	}

	for (i = 0; i < num_objs; i++)
		mbpstat[i] = mbpstat[0] + i;

#undef MSIZE
#define MSIZE		(mbstat->m_msize)
#undef MCLBYTES
#define	MCLBYTES	(mbstat->m_mclbytes)
#define	MBPERPG		(page_size / MSIZE)
#define	CLPERPG		(page_size / MCLBYTES)
#define	GENLST		(num_objs - 1)

	printf("mbuf usage:\n");
	printf("\tGEN list:\t%lu/%lu (in use/in pool)\n",
	    (mbpstat[GENLST]->mb_mbpgs * MBPERPG - mbpstat[GENLST]->mb_mbfree),
	    (mbpstat[GENLST]->mb_mbpgs * MBPERPG));
	totnum = mbpstat[GENLST]->mb_mbpgs * MBPERPG;
	totfree = mbpstat[GENLST]->mb_mbfree;
	totspace = mbpstat[GENLST]->mb_mbpgs * page_size;
	for (i = 0; i < ncpu; i++) {
		printf("\tCPU #%d list:\t%lu/%lu (in use/in pool)\n", i,
		    (mbpstat[i]->mb_mbpgs * MBPERPG - mbpstat[i]->mb_mbfree),
		    (mbpstat[i]->mb_mbpgs * MBPERPG));
		totspace += mbpstat[i]->mb_mbpgs * page_size;
		totnum += mbpstat[i]->mb_mbpgs * MBPERPG;
		totfree += mbpstat[i]->mb_mbfree;
	}
	printf("\tTotal:\t\t%lu/%lu (in use/in pool)\n", (totnum - totfree),
	    totnum);
	printf("\tMaximum number allowed on each CPU list: %d\n", mbuf_limit);
	printf("\tMaximum possible: %d\n", nmbufs);
	printf("\t%lu%% of mbuf map consumed\n", ((totspace * 100) / (nmbufs
	    * MSIZE)));

	printf("mbuf cluster usage:\n");
	printf("\tGEN list:\t%lu/%lu (in use/in pool)\n",
	    (mbpstat[GENLST]->mb_clpgs * CLPERPG - mbpstat[GENLST]->mb_clfree),
	    (mbpstat[GENLST]->mb_clpgs * CLPERPG));
	totnum = mbpstat[GENLST]->mb_clpgs * CLPERPG;
	totfree = mbpstat[GENLST]->mb_clfree;
	totspace = mbpstat[GENLST]->mb_clpgs * page_size;
	for (i = 0; i < ncpu; i++) {
		printf("\tCPU #%d list:\t%lu/%lu (in use/in pool)\n", i,
		    (mbpstat[i]->mb_clpgs * CLPERPG - mbpstat[i]->mb_clfree),
		    (mbpstat[i]->mb_clpgs * CLPERPG));
		totspace += mbpstat[i]->mb_clpgs * page_size;
		totnum += mbpstat[i]->mb_clpgs * CLPERPG;
		totfree += mbpstat[i]->mb_clfree;
	}
	printf("\tTotal:\t\t%lu/%lu (in use/in pool)\n", (totnum - totfree),
	    totnum);
	printf("\tMaximum number allowed on each CPU list: %d\n", clust_limit);
	printf("\tMaximum possible: %d\n", nmbclusters);
	printf("\t%lu%% of cluster map consumed\n", ((totspace * 100) /
	    (nmbclusters * MCLBYTES)));

	printf("%lu requests for memory denied\n", mbstat->m_drops);
	printf("%lu requests for memory delayed\n", mbstat->m_wait);
	printf("%lu calls to protocol drain routines\n", mbstat->m_drain);

err:
#if 0
	if (mbtypes != NULL)
		free(mbtypes);
	if (seen != NULL)
		free(seen);
#endif
	if (mbstat != NULL)
		free(mbstat);
	if (mbpstat != NULL) {
		if (mbpstat[0] != NULL)
			free(mbpstat[0]);
		free(mbpstat);
	}

	return;
}
