/*-
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
#include <sys/malloc.h>
#include <sys/sysctl.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memstat.h"
#include "memstat_internal.h"

/*
 * Extract malloc(9) statistics from the running kernel, and store all memory
 * type information in the passed list.  For each type, check the list for an
 * existing entry with the right name/allocator -- if present, update that
 * entry.  Otherwise, add a new entry.  On error, the entire list will be
 * cleared, as entries will be in an inconsistent state.
 *
 * To reduce the level of work for a list that starts empty, we keep around a
 * hint as to whether it was empty when we began, so we can avoid searching
 * the list for entries to update.  Updates are O(n^2) due to searching for
 * each entry before adding it.
 */
int
memstat_sysctl_malloc(struct memory_type_list *list, int flags)
{
	struct malloc_type_stream_header *mtshp;
	struct malloc_type_header *mthp;
	struct malloc_type_stats *mtsp;
	struct memory_type *mtp;
	int count, error, hint_dontsearch, i, j, maxcpus;
	char *buffer, *p;
	size_t size;

	hint_dontsearch = LIST_EMPTY(list);

	/*
	 * Query the number of CPUs, number of malloc types so that we can
	 * guess an initial buffer size.  We loop until we succeed or really
	 * fail.  Note that the value of maxcpus we query using sysctl is not
	 * the version we use when processing the real data -- that is read
	 * from the header.
	 */
retry:
	size = sizeof(maxcpus);
	if (sysctlbyname("kern.smp.maxcpus", &maxcpus, &size, NULL, 0) < 0) {
		error = errno;
		perror("kern.smp.maxcpus");
		errno = error;
		return (-1);
	}
	if (size != sizeof(maxcpus)) {
		fprintf(stderr, "kern.smp.maxcpus: wronge size");
		errno = EINVAL;
		return (-1);
	}

	if (maxcpus > MEMSTAT_MAXCPU) {
		fprintf(stderr, "kern.smp.maxcpus: too many CPUs\n");
		errno = EINVAL;
		return (-1);
	}

	size = sizeof(count);
	if (sysctlbyname("kern.malloc_count", &count, &size, NULL, 0) < 0) {
		error = errno;
		perror("kern.malloc_count");
		errno = error;
		return (-1);
	}
	if (size != sizeof(count)) {
		fprintf(stderr, "kern.malloc_count: wronge size");
		errno = EINVAL;
		return (-1);
	}

	size = sizeof(*mthp) + count * (sizeof(*mthp) + sizeof(*mtsp) *
	    maxcpus);

	buffer = malloc(size);
	if (buffer == NULL) {
		error = errno;
		perror("malloc");
		errno = error;
		return (-1);
	}

	if (sysctlbyname("kern.malloc_stats", buffer, &size, NULL, 0) < 0) {
		/*
		 * XXXRW: ENOMEM is an ambiguous return, we should bound the
		 * number of loops, perhaps.
		 */
		if (errno == ENOMEM) {
			free(buffer);
			goto retry;
		}
		error = errno;
		free(buffer);
		perror("kern.malloc_stats");
		errno = error;
		return (-1);
	}

	if (size == 0) {
		free(buffer);
		return (0);
	}

	if (size < sizeof(*mtshp)) {
		fprintf(stderr, "sysctl_malloc: invalid malloc header");
		free(buffer);
		errno = EINVAL;
		return (-1);
	}
	p = buffer;
	mtshp = (struct malloc_type_stream_header *)p;
	p += sizeof(*mtshp);

	if (mtshp->mtsh_version != MALLOC_TYPE_STREAM_VERSION) {
		fprintf(stderr, "sysctl_malloc: unknown malloc version");
		free(buffer);
		errno = EINVAL;
		return (-1);
	}

	if (mtshp->mtsh_maxcpus > MEMSTAT_MAXCPU) {
		fprintf(stderr, "sysctl_malloc: too many CPUs");
		free(buffer);
		errno = EINVAL;
		return (-1);
	}

	/*
	 * For the remainder of this function, we are quite trusting about
	 * the layout of structures and sizes, since we've determined we have
	 * a matching version and acceptable CPU count.
	 */
	maxcpus = mtshp->mtsh_maxcpus;
	count = mtshp->mtsh_count;
	for (i = 0; i < count; i++) {
		mthp = (struct malloc_type_header *)p;
		p += sizeof(*mthp);

		if (hint_dontsearch == 0) {
			mtp = memstat_mtl_find(list, ALLOCATOR_MALLOC,
			    mthp->mth_name);
			/*
			 * Reset the statistics on a reused node.
			 */
			if (mtp != NULL)
				memstat_mt_reset_stats(mtp);
		} else
			mtp = NULL;
		if (mtp == NULL)
			mtp = memstat_mt_allocate(list, ALLOCATOR_MALLOC,
			    mthp->mth_name);
		if (mtp == NULL) {
			memstat_mtl_free(list);
			free(buffer);
			errno = ENOMEM;
			perror("malloc");
			errno = ENOMEM;
			return (-1);
		}

		/*
		 * Reset the statistics on a current node.
		 */
		memstat_mt_reset_stats(mtp);

		for (j = 0; j < maxcpus; j++) {
			mtsp = (struct malloc_type_stats *)p;
			p += sizeof(*mtsp);

			/*
			 * Sumarize raw statistics across CPUs into coalesced
			 * statistics.
			 */
			mtp->mt_memalloced += mtsp->mts_memalloced;
			mtp->mt_memfreed += mtsp->mts_memfreed;
			mtp->mt_numallocs += mtsp->mts_numallocs;
			mtp->mt_numfrees += mtsp->mts_numfrees;
			mtp->mt_sizemask |= mtsp->mts_size;

			/*
			 * Copies of per-CPU statistics.
			 */
			mtp->mt_percpu_alloc[j].mtp_memalloced =
			    mtsp->mts_memalloced;
			mtp->mt_percpu_alloc[j].mtp_memfreed =
			    mtsp->mts_memfreed;
			mtp->mt_percpu_alloc[j].mtp_numallocs =
			    mtsp->mts_numallocs;
			mtp->mt_percpu_alloc[j].mtp_numfrees =
			    mtsp->mts_numfrees;
			mtp->mt_percpu_alloc[j].mtp_sizemask =
			    mtsp->mts_size;
		}

		/*
		 * Derived cross-CPU statistics.
		 */
		mtp->mt_bytes = mtp->mt_memalloced - mtp->mt_memfreed;
		mtp->mt_count = mtp->mt_numallocs - mtp->mt_numfrees;
	}

	free(buffer);

	return (0);
}
