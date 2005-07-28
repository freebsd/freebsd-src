/*
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.
 * Copyright (c) 2005 Robert N. M. Watson
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

#if 0
#ifndef lint
static char sccsid[] = "@(#)mbuf.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <err.h>
#include <memstat.h>
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
 * Print mbuf statistics extracted from kmem.
 */
void
mbpr_kmem(u_long mbaddr, u_long nmbcaddr, u_long nmbufaddr, u_long mbhiaddr,
    u_long clhiaddr, u_long mbloaddr, u_long clloaddr, u_long pgsaddr,
    u_long mbpaddr)
{
	int i, nmbclusters;
	short nmbtypes;
	size_t mlen;
	long *mbtypes = NULL;
	struct mbstat *mbstat = NULL;
	struct mbtypenames *mp;
	bool *seen = NULL;

	mlen = sizeof *mbstat;
	if ((mbstat = malloc(mlen)) == NULL) {
		warn("malloc: cannot allocate memory for mbstat");
		goto err;
	}

	if (kread(mbaddr, (char *)mbstat, sizeof mbstat))
		goto err;
	if (kread(nmbcaddr, (char *)&nmbclusters, sizeof(int)))
		goto err;

	if (mbstat->m_mbufs < 0) mbstat->m_mbufs = 0;		/* XXX */
	if (mbstat->m_mclusts < 0) mbstat->m_mclusts = 0;	/* XXX */

	nmbtypes = mbstat->m_numtypes;
	if ((seen = calloc(nmbtypes, sizeof(*seen))) == NULL) {
		warn("calloc: cannot allocate memory for mbtypes seen flag");
		goto err;
	}
	if ((mbtypes = calloc(nmbtypes, sizeof(long *))) == NULL) { 
		warn("calloc: cannot allocate memory for mbtypes");
		goto err;
	}

#undef MSIZE
#define MSIZE		(mbstat->m_msize)
#undef MCLBYTES
#define	MCLBYTES	(mbstat->m_mclbytes)

	printf("%lu mbufs in use\n", mbstat->m_mbufs);

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

	printf("%lu/%d mbuf clusters in use (current/max)\n",
	    mbstat->m_mclusts, nmbclusters);

	printf("%lu KBytes allocated to network\n", (mbstat->m_mbufs * MSIZE +
	    mbstat->m_mclusts * MCLBYTES) / 1024);
	printf("%lu requests for sfbufs denied\n", mbstat->sf_allocfail);
	printf("%lu requests for sfbufs delayed\n", mbstat->sf_allocwait);
	printf("%lu requests for I/O initiated by sendfile\n",
	    mbstat->sf_iocnt);
	printf("%lu calls to protocol drain routines\n", mbstat->m_drain);

err:
	if (mbtypes != NULL)
		free(mbtypes);
	if (seen != NULL)
		free(seen);
	if (mbstat != NULL)
		free(mbstat);
}

/*
 * If running on a live kernel, directly query the zone allocator for stats
 * from the four mbuf-related zones/types.
 */
static void
mbpr_sysctl(void)
{
	struct memory_type_list *mtlp;
	struct memory_type *mtp;
	u_int64_t mbuf_count, mbuf_bytes, mbuf_free, mbuf_failures, mbuf_size;
	u_int64_t cluster_count, cluster_bytes, cluster_limit, cluster_free;
	u_int64_t cluster_failures, cluster_size;
	u_int64_t packet_count, packet_bytes, packet_free, packet_failures;
	u_int64_t tag_count, tag_bytes;
	u_int64_t bytes_inuse, bytes_incache, bytes_total;
	int nsfbufs, nsfbufspeak, nsfbufsused;
	struct mbstat mbstat;
	size_t mlen;

	mtlp = memstat_mtl_alloc();
	if (mtlp == NULL) {
		warn("memstat_mtl_alloc");
		return;
	}

	/*
	 * Use memstat_sysctl_all() because some mbuf-related memory is in
	 * uma(9), and some malloc(9).
	 */
	if (memstat_sysctl_all(mtlp, 0) < 0) {
		warnx("memstat_sysctl_all: %s",
		    memstat_strerror(memstat_mtl_geterror(mtlp)));
		goto out;
	}

	mtp = memstat_mtl_find(mtlp, ALLOCATOR_UMA, MBUF_MEM_NAME);
	if (mtp == NULL) {
		warnx("memstat_mtl_find: zone %s not found", MBUF_MEM_NAME);
		goto out;
	}
	mbuf_count = memstat_get_count(mtp);
	mbuf_bytes = memstat_get_bytes(mtp);
	mbuf_free = memstat_get_free(mtp);
	mbuf_failures = memstat_get_failures(mtp);
	mbuf_size = memstat_get_size(mtp);

	mtp = memstat_mtl_find(mtlp, ALLOCATOR_UMA, MBUF_PACKET_MEM_NAME);
	if (mtp == NULL) {
		warnx("memstat_mtl_find: zone %s not found",
		    MBUF_PACKET_MEM_NAME);
		goto out;
	}
	packet_count = memstat_get_count(mtp);
	packet_bytes = memstat_get_bytes(mtp);
	packet_free = memstat_get_free(mtp);
	packet_failures = memstat_get_failures(mtp);

	mtp = memstat_mtl_find(mtlp, ALLOCATOR_UMA, MBUF_CLUSTER_MEM_NAME);
	if (mtp == NULL) {
		warnx("memstat_mtl_find: zone %s not found",
		    MBUF_CLUSTER_MEM_NAME);
		goto out;
	}
	cluster_count = memstat_get_count(mtp);
	cluster_bytes = memstat_get_bytes(mtp);
	cluster_limit = memstat_get_countlimit(mtp);
	cluster_free = memstat_get_free(mtp);
	cluster_failures = memstat_get_failures(mtp);
	cluster_size = memstat_get_size(mtp);

	mtp = memstat_mtl_find(mtlp, ALLOCATOR_MALLOC, MBUF_TAG_MEM_NAME);
	if (mtp == NULL) {
		warnx("memstat_mtl_find: malloc type %s not found",
		    MBUF_TAG_MEM_NAME);
		goto out;
	}
	tag_count = memstat_get_count(mtp);
	tag_bytes = memstat_get_bytes(mtp);

	printf("%llu/%llu/%llu mbufs in use (current/cache/total)\n",
	    mbuf_count + packet_count, mbuf_free + packet_free,
	    mbuf_count + packet_count + mbuf_free + packet_free);

	printf("%llu/%llu/%llu/%llu mbuf clusters in use "
	    "(current/cache/total/max)\n",
	    cluster_count - packet_free, cluster_free + packet_free,
	    cluster_count + cluster_free, cluster_limit);

#if 0
	printf("%llu mbuf tags in use\n", tag_count);
#endif

	mlen = sizeof(nsfbufs);
	if (!sysctlbyname("kern.ipc.nsfbufs", &nsfbufs, &mlen, NULL, 0) &&
	    !sysctlbyname("kern.ipc.nsfbufsused", &nsfbufsused, &mlen, NULL,
	    0) &&
	    !sysctlbyname("kern.ipc.nsfbufspeak", &nsfbufspeak, &mlen, NULL,
	    0)) {
		printf("%d/%d/%d sfbufs in use (current/peak/max)\n",
		    nsfbufsused, nsfbufspeak, nsfbufs);
	}

	/*-
	 * Calculate in-use bytes as:
	 * - straight mbuf memory
	 * - mbuf memory in packets
	 * - the clusters attached to packets
	 * - and the rest of the non-packet-attached clusters.
	 * - m_tag memory
	 * This avoids counting the clusters attached to packets in the cache.
	 * This currently excludes sf_buf space.
	 */
	bytes_inuse =
	    mbuf_bytes +			/* straight mbuf memory */
	    packet_bytes +			/* mbufs in packets */
	    (packet_count * cluster_size) +	/* clusters in packets */
	    /* other clusters */
	    ((cluster_count - packet_count - packet_free) * cluster_size) +
	    tag_bytes;

	/*
	 * Calculate in-cache bytes as:
	 * - cached straught mbufs
	 * - cached packet mbufs
	 * - cached packet clusters
	 * - cached straight clusters
	 * This currently excludes sf_buf space.
	 */
	bytes_incache =
	    (mbuf_free * mbuf_size) +		/* straight free mbufs */
	    (packet_free * mbuf_size) +		/* mbufs in free packets */
	    (packet_free * cluster_size) +	/* clusters in free packets */
	    (cluster_free * cluster_size);	/* free clusters */

	/*
	 * Total is bytes in use + bytes in cache.  This doesn't take into
	 * account various other misc data structures, overhead, etc, but
	 * gives the user something useful despite that.
	 */
	bytes_total = bytes_inuse + bytes_incache;

	printf("%lluK/%lluK/%lluK bytes allocated to network "
	    "(current/cache/total)\n", bytes_inuse / 1024,
	    bytes_incache / 1024, bytes_total / 1024);

#if 0
	printf("%llu/%llu/%llu requests for mbufs denied (mbufs/clusters/"
	    "mbuf+clusters)\n", mbuf_failures, cluster_failures,
	    packet_failures);
#endif

	mlen = sizeof(mbstat);
	if (!sysctlbyname("kern.ipc.mbstat", &mbstat, &mlen, NULL, 0)) {
		printf("%lu requests for sfbufs denied\n",
		    mbstat.sf_allocfail);
		printf("%lu requests for sfbufs delayed\n",
		    mbstat.sf_allocwait);
		printf("%lu requests for I/O initiated by sendfile\n",
		    mbstat.sf_iocnt);
		printf("%lu calls to protocol drain routines\n",
		    mbstat.m_drain);
	}

out:
	memstat_mtl_free(mtlp);
}

/*
 * Print mbuf statistics.
 */
void
mbpr(u_long mbaddr, u_long mbtaddr __unused, u_long nmbcaddr, u_long nmbufaddr,
    u_long mbhiaddr, u_long clhiaddr, u_long mbloaddr, u_long clloaddr,
    u_long cpusaddr __unused, u_long pgsaddr, u_long mbpaddr)
{

	if (mbaddr != 0)
		mbpr_kmem(mbaddr, nmbcaddr, nmbufaddr, mbhiaddr, clhiaddr,
		    mbloaddr, clloaddr, pgsaddr, mbpaddr);
	else
		mbpr_sysctl();
}
