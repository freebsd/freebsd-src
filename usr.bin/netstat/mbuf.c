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
    u_long mbhiaddr, u_long clhiaddr, u_long mbloaddr, u_long clloaddr,
    u_long cpusaddr __unused, u_long pgsaddr, u_long mbpaddr)
{
	int i, nmbclusters;
	int nsfbufs, nsfbufspeak, nsfbufsused;
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

	if (mbaddr) {
		if (kread(mbaddr, (char *)mbstat, sizeof mbstat))
			goto err;
		if (kread(nmbcaddr, (char *)&nmbclusters, sizeof(int)))
			goto err;
	} else {
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
	}
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

	mlen = sizeof(nsfbufs);
	if (!sysctlbyname("kern.ipc.nsfbufs", &nsfbufs, &mlen, NULL, 0) &&
	    !sysctlbyname("kern.ipc.nsfbufsused", &nsfbufsused, &mlen, NULL,
	    0) &&
	    !sysctlbyname("kern.ipc.nsfbufspeak", &nsfbufspeak, &mlen, NULL,
	    0)) {
		printf("%d/%d/%d sfbufs in use (current/peak/max)\n",
		    nsfbufsused, nsfbufspeak, nsfbufs);
	}
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
