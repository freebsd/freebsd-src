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

static struct mbtypenames {
	short	mt_type;
	const char *mt_name;
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

/*
 * Print mbuf statistics.
 */
void
mbpr(u_long mbaddr, u_long mbtaddr __unused, u_long nmbcaddr, u_long nmbufaddr,
    u_long mblimaddr, u_long cllimaddr, u_long cpusaddr __unused,
    u_long pgsaddr, u_long mbpaddr)
{
	int i, j, nmbufs, nmbclusters, page_size, num_objs;
	u_int mbuf_limit, clust_limit;
	u_long totspace[2], totused[2], totnum, totfree;
	short nmbtypes;
	size_t mlen;
	long *mbtypes = NULL;
	struct mbstat *mbstat = NULL;
	struct mbpstat **mbpstat = NULL;
	struct mbtypenames *mp;
	bool *seen = NULL;

	mlen = sizeof *mbstat;
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
		if (kread(nmbcaddr, (char *)&nmbclusters, sizeof(int)))
			goto err;
		if (kread(nmbufaddr, (char *)&nmbufs, sizeof(int)))
			goto err;
		if (kread(mblimaddr, (char *)&mbuf_limit, sizeof(u_int)))
			goto err;
		if (kread(cllimaddr, (char *)&clust_limit, sizeof(u_int)))
			goto err;
		if (kread(pgsaddr, (char *)&page_size, sizeof(int)))
			goto err;
	} else {
		if (sysctlbyname("kern.ipc.mb_statpcpu", mbpstat[0], &mlen,
		    NULL, 0) < 0) {
			warn("sysctl: retrieving mb_statpcpu");
			goto err;
		}
		mlen = sizeof *mbstat;
		if (sysctlbyname("kern.ipc.mbstat", mbstat, &mlen, NULL, 0)
		    < 0) {
			warn("sysctl: retrieving mbstat");
			goto err;
		}
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
		if (sysctlbyname("hw.pagesize", &page_size, &mlen, NULL, 0)
		    < 0) {
			warn("sysctl: retrieving hw.pagesize");
			goto err;
		}
	}

	nmbtypes = mbstat->m_numtypes;
	if ((seen = calloc(nmbtypes, sizeof(*seen))) == NULL) {
		warn("calloc: cannot allocate memory for mbtypes seen flag");
		goto err;
	}
	if ((mbtypes = calloc(nmbtypes, sizeof(long *))) == NULL) { 
		warn("calloc: cannot allocate memory for mbtypes");
		goto err;
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
	for (j = 1; j < nmbtypes; j++)
		mbtypes[j] += mbpstat[GENLST]->mb_mbtypes[j];
	totspace[0] = mbpstat[GENLST]->mb_mbpgs * page_size;
	for (i = 0; i < (num_objs - 1); i++) {
		if (mbpstat[i]->mb_active == 0)
			continue;
		printf("\tCPU #%d list:\t%lu/%lu (in use/in pool)\n", i,
		    (mbpstat[i]->mb_mbpgs * MBPERPG - mbpstat[i]->mb_mbfree),
		    (mbpstat[i]->mb_mbpgs * MBPERPG));
		totspace[0] += mbpstat[i]->mb_mbpgs * page_size;
		totnum += mbpstat[i]->mb_mbpgs * MBPERPG;
		totfree += mbpstat[i]->mb_mbfree;
		for (j = 1; j < nmbtypes; j++)
			mbtypes[j] += mbpstat[i]->mb_mbtypes[j]; 
	}
	totused[0] = totnum - totfree;
	printf("\tTotal:\t\t%lu/%lu (in use/in pool)\n", totused[0], totnum);
	printf("\tMaximum number allowed on each CPU list: %d\n", mbuf_limit);
	printf("\tMaximum possible: %d\n", nmbufs);
	printf("\tAllocated mbuf types:\n");
	for (mp = mbtypenames; mp->mt_name; mp++) {
		if (mbtypes[mp->mt_type]) {
			seen[mp->mt_type] = YES;
			printf("\t  %lu mbufs allocated to %s\n",
				mbtypes[mp->mt_type], mp->mt_name);
		}
	}
	for (i = 1; i < nmbtypes; i++) {
		if (!seen[i] && mbtypes[i])
			printf("\t  %lu mbufs allocated to <mbuf type: %d>\n",
			    mbtypes[i], i);
	}
	printf("\t%lu%% of mbuf map consumed\n", ((totspace[0] * 100) / (nmbufs
	    * MSIZE)));

	printf("mbuf cluster usage:\n");
	printf("\tGEN list:\t%lu/%lu (in use/in pool)\n",
	    (mbpstat[GENLST]->mb_clpgs * CLPERPG - mbpstat[GENLST]->mb_clfree),
	    (mbpstat[GENLST]->mb_clpgs * CLPERPG));
	totnum = mbpstat[GENLST]->mb_clpgs * CLPERPG;
	totfree = mbpstat[GENLST]->mb_clfree;
	totspace[1] = mbpstat[GENLST]->mb_clpgs * page_size;
	for (i = 0; i < (num_objs - 1); i++) {
		if (mbpstat[i]->mb_active == 0)
			continue;
		printf("\tCPU #%d list:\t%lu/%lu (in use/in pool)\n", i,
		    (mbpstat[i]->mb_clpgs * CLPERPG - mbpstat[i]->mb_clfree),
		    (mbpstat[i]->mb_clpgs * CLPERPG));
		totspace[1] += mbpstat[i]->mb_clpgs * page_size;
		totnum += mbpstat[i]->mb_clpgs * CLPERPG;
		totfree += mbpstat[i]->mb_clfree;
	}
	totused[1] = totnum - totfree;
	printf("\tTotal:\t\t%lu/%lu (in use/in pool)\n", totused[1], totnum);
	printf("\tMaximum number allowed on each CPU list: %d\n", clust_limit);
	printf("\tMaximum possible: %d\n", nmbclusters);
	printf("\t%lu%% of cluster map consumed\n", ((totspace[1] * 100) /
	    (nmbclusters * MCLBYTES)));

	printf("%lu KBytes of wired memory reserved (%lu%% in use)\n",
	    (totspace[0] + totspace[1]) / 1024, ((totused[0] * MSIZE +
	    totused[1] * MCLBYTES) * 100) / (totspace[0] + totspace[1]));
	printf("%lu requests for memory denied\n", mbstat->m_drops);
	printf("%lu requests for memory delayed\n", mbstat->m_wait);
	printf("%lu calls to protocol drain routines\n", mbstat->m_drain);

err:
	if (mbtypes != NULL)
		free(mbtypes);
	if (seen != NULL)
		free(seen);
	if (mbstat != NULL)
		free(mbstat);
	if (mbpstat != NULL) {
		if (mbpstat[0] != NULL)
			free(mbpstat[0]);
		free(mbpstat);
	}

	return;
}
