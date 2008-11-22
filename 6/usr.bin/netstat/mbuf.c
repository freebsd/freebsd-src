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
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <err.h>
#include <kvm.h>
#include <memstat.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "netstat.h"

/*
 * Print mbuf statistics.
 */
void
mbpr(void *kvmd, u_long mbaddr)
{
	struct memory_type_list *mtlp;
	struct memory_type *mtp;
	uintmax_t mbuf_count, mbuf_bytes, mbuf_free, mbuf_failures, mbuf_size;
	uintmax_t cluster_count, cluster_bytes, cluster_limit, cluster_free;
	uintmax_t cluster_failures, cluster_size;
	uintmax_t packet_count, packet_bytes, packet_free, packet_failures;
	uintmax_t tag_count, tag_bytes;
	uintmax_t jumbop_count, jumbop_bytes, jumbop_limit, jumbop_free;
	uintmax_t jumbop_failures, jumbop_size;
	uintmax_t jumbo9_count, jumbo9_bytes, jumbo9_limit, jumbo9_free;
	uintmax_t jumbo9_failures, jumbo9_size;
	uintmax_t jumbo16_count, jumbo16_bytes, jumbo16_limit, jumbo16_free;
	uintmax_t jumbo16_failures, jumbo16_size;
	uintmax_t bytes_inuse, bytes_incache, bytes_total;
	int nsfbufs, nsfbufspeak, nsfbufsused;
	struct mbstat mbstat;
	size_t mlen;
	int error;

	mtlp = memstat_mtl_alloc();
	if (mtlp == NULL) {
		warn("memstat_mtl_alloc");
		return;
	}

	/*
	 * Use memstat_*_all() because some mbuf-related memory is in uma(9),
	 * and some malloc(9).
	 */
	if (live) {
		if (memstat_sysctl_all(mtlp, 0) < 0) {
			warnx("memstat_sysctl_all: %s",
			    memstat_strerror(memstat_mtl_geterror(mtlp)));
			goto out;
		}
	} else {
		if (memstat_kvm_all(mtlp, kvmd) < 0) {
			error = memstat_mtl_geterror(mtlp);
			if (error == MEMSTAT_ERROR_KVM)
				warnx("memstat_kvm_all: %s",
				    kvm_geterr(kvmd));
			else
				warnx("memstat_kvm_all: %s",
				    memstat_strerror(error));
			goto out;
		}
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

	mtp = memstat_mtl_find(mtlp, ALLOCATOR_UMA, MBUF_JUMBOP_MEM_NAME);
	if (mtp == NULL) {
		warnx("memstat_mtl_find: zone %s not found",
		    MBUF_JUMBOP_MEM_NAME);
		goto out;
	}
	jumbop_count = memstat_get_count(mtp);
	jumbop_bytes = memstat_get_bytes(mtp);
	jumbop_limit = memstat_get_countlimit(mtp);
	jumbop_free = memstat_get_free(mtp);
	jumbop_failures = memstat_get_failures(mtp);
	jumbop_size = memstat_get_size(mtp);

	mtp = memstat_mtl_find(mtlp, ALLOCATOR_UMA, MBUF_JUMBO9_MEM_NAME);
	if (mtp == NULL) {
		warnx("memstat_mtl_find: zone %s not found",
		    MBUF_JUMBO9_MEM_NAME);
		goto out;
	}
	jumbo9_count = memstat_get_count(mtp);
	jumbo9_bytes = memstat_get_bytes(mtp);
	jumbo9_limit = memstat_get_countlimit(mtp);
	jumbo9_free = memstat_get_free(mtp);
	jumbo9_failures = memstat_get_failures(mtp);
	jumbo9_size = memstat_get_size(mtp);

	mtp = memstat_mtl_find(mtlp, ALLOCATOR_UMA, MBUF_JUMBO16_MEM_NAME);
	if (mtp == NULL) {
		warnx("memstat_mtl_find: zone %s not found",
		    MBUF_JUMBO16_MEM_NAME);
		goto out;
	}
	jumbo16_count = memstat_get_count(mtp);
	jumbo16_bytes = memstat_get_bytes(mtp);
	jumbo16_limit = memstat_get_countlimit(mtp);
	jumbo16_free = memstat_get_free(mtp);
	jumbo16_failures = memstat_get_failures(mtp);
	jumbo16_size = memstat_get_size(mtp);

	printf("%ju/%ju/%ju mbufs in use (current/cache/total)\n",
	    mbuf_count + packet_count, mbuf_free + packet_free,
	    mbuf_count + packet_count + mbuf_free + packet_free);

	printf("%ju/%ju/%ju/%ju mbuf clusters in use "
	    "(current/cache/total/max)\n",
	    cluster_count - packet_free, cluster_free + packet_free,
	    cluster_count + cluster_free, cluster_limit);

	printf("%ju/%ju mbuf+clusters out of packet secondary zone in use "
	    "(current/cache)\n",
	    packet_count, packet_free);

	printf("%ju/%ju/%ju/%ju %juk (page size) jumbo clusters in use "
	    "(current/cache/total/max)\n",
	    jumbop_count, jumbop_free, jumbop_count + jumbop_free,
	    jumbop_limit, jumbop_size / 1024);

	printf("%ju/%ju/%ju/%ju 9k jumbo clusters in use "
	    "(current/cache/total/max)\n",
	    jumbo9_count, jumbo9_free, jumbo9_count + jumbo9_free,
	    jumbo9_limit);

	printf("%ju/%ju/%ju/%ju 16k jumbo clusters in use "
	    "(current/cache/total/max)\n",
	    jumbo16_count, jumbo16_free, jumbo16_count + jumbo16_free,
	    jumbo16_limit);

#if 0
	printf("%ju mbuf tags in use\n", tag_count);
#endif

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
	    tag_bytes +
	    (jumbop_count * jumbop_size) +	/* jumbo clusters */
	    (jumbo9_count * jumbo9_size) +
	    (jumbo16_count * jumbo16_size);

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
	    (cluster_free * cluster_size) +	/* free clusters */
	    (jumbop_free * jumbop_size) +	/* jumbo clusters */
	    (jumbo9_free * jumbo9_size) +
	    (jumbo16_free * jumbo16_size);

	/*
	 * Total is bytes in use + bytes in cache.  This doesn't take into
	 * account various other misc data structures, overhead, etc, but
	 * gives the user something useful despite that.
	 */
	bytes_total = bytes_inuse + bytes_incache;

	printf("%juK/%juK/%juK bytes allocated to network "
	    "(current/cache/total)\n", bytes_inuse / 1024,
	    bytes_incache / 1024, bytes_total / 1024);

	printf("%ju/%ju/%ju requests for mbufs denied (mbufs/clusters/"
	    "mbuf+clusters)\n", mbuf_failures, cluster_failures,
	    packet_failures);

	printf("%ju/%ju/%ju requests for jumbo clusters denied "
	    "(%juk/9k/16k)\n", jumbop_failures, jumbo9_failures,
	    jumbo16_failures, jumbop_size / 1024);

	if (live) {
		mlen = sizeof(nsfbufs);
		if (!sysctlbyname("kern.ipc.nsfbufs", &nsfbufs, &mlen, NULL,
		    0) &&
		    !sysctlbyname("kern.ipc.nsfbufsused", &nsfbufsused,
		    &mlen, NULL, 0) &&
		    !sysctlbyname("kern.ipc.nsfbufspeak", &nsfbufspeak,
		    &mlen, NULL, 0))
			printf("%d/%d/%d sfbufs in use (current/peak/max)\n",
			    nsfbufsused, nsfbufspeak, nsfbufs);
		mlen = sizeof(mbstat);
		if (sysctlbyname("kern.ipc.mbstat", &mbstat, &mlen, NULL, 0)) {
			warn("kern.ipc.mbstat");
			goto out;
		}
	} else {
		if (kread(mbaddr, (char *)&mbstat, sizeof mbstat))
			goto out;
	}
	printf("%lu requests for sfbufs denied\n", mbstat.sf_allocfail);
	printf("%lu requests for sfbufs delayed\n", mbstat.sf_allocwait);
	printf("%lu requests for I/O initiated by sendfile\n",
	    mbstat.sf_iocnt);
	printf("%lu calls to protocol drain routines\n", mbstat.m_drain);
out:
	memstat_mtl_free(mtlp);
}
