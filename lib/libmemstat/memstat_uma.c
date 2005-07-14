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
#include <sys/sysctl.h>

#include <vm/uma.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memstat.h"
#include "memstat_internal.h"

/*
 * Extract uma(9) statistics from the running kernel, and store all memory
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
memstat_sysctl_uma(struct memory_type_list *list, int flags)
{
	struct uma_stream_header *ushp;
	struct uma_type_header *uthp;
	struct uma_percpu_stat *upsp;
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
	if (sysctlbyname("vm.zone_count", &count, &size, NULL, 0) < 0) {
		error = errno;
		perror("vm.zone_count");
		errno = error;
		return (-1);
	}
	if (size != sizeof(count)) {
		fprintf(stderr, "vm.zone_count: wronge size");
		errno = EINVAL;
		return (-1);
	}

	size = sizeof(*uthp) + count * (sizeof(*uthp) + sizeof(*upsp) *
	    maxcpus);

	buffer = malloc(size);
	if (buffer == NULL) {
		error = errno;
		perror("malloc");
		errno = error;
		return (-1);
	}

	if (sysctlbyname("vm.zone_stats", buffer, &size, NULL, 0) < 0) {
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
		perror("vm.zone_stats");
		errno = error;
		return (-1);
	}

	if (size == 0) {
		free(buffer);
		return (0);
	}

	if (size < sizeof(*ushp)) {
		fprintf(stderr, "sysctl_uma: invalid malloc header");
		free(buffer);
		errno = EINVAL;
		return (-1);
	}
	p = buffer;
	ushp = (struct uma_stream_header *)p;
	p += sizeof(*ushp);

	if (ushp->ush_version != UMA_STREAM_VERSION) {
		fprintf(stderr, "sysctl_uma: unknown malloc version");
		free(buffer);
		errno = EINVAL;
		return (-1);
	}

	if (ushp->ush_maxcpus > MEMSTAT_MAXCPU) {
		fprintf(stderr, "sysctl_uma: too many CPUs");
		free(buffer);
		errno = EINVAL;
		return (-1);
	}

	/*
	 * For the remainder of this function, we are quite trusting about
	 * the layout of structures and sizes, since we've determined we have
	 * a matching version and acceptable CPU count.
	 */
	maxcpus = ushp->ush_maxcpus;
	count = ushp->ush_count;
	for (i = 0; i < count; i++) {
		uthp = (struct uma_type_header *)p;
		p += sizeof(*uthp);

		if (hint_dontsearch == 0) {
			mtp = memstat_mtl_find(list, ALLOCATOR_UMA,
			    uthp->uth_name);
			/*
			 * Reset the statistics on a reused node.
			 */
			if (mtp != NULL)
				memstat_mt_reset_stats(mtp);
		} else
			mtp = NULL;
		if (mtp == NULL)
			mtp = memstat_mt_allocate(list, ALLOCATOR_UMA,
			    uthp->uth_name);
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
			upsp = (struct uma_percpu_stat *)p;
			p += sizeof(*upsp);

			mtp->mt_percpu_cache[j].mtp_free =
			    upsp->ups_cache_free;
			mtp->mt_free += upsp->ups_cache_free;
			mtp->mt_numallocs += upsp->ups_allocs;
			mtp->mt_numfrees += upsp->ups_frees;
		}

		mtp->mt_size = uthp->uth_size;
		mtp->mt_memalloced = uthp->uth_allocs * uthp->uth_size;
		mtp->mt_memfreed = uthp->uth_frees * uthp->uth_size;
		mtp->mt_bytes = mtp->mt_memalloced - mtp->mt_memfreed;
		mtp->mt_countlimit = uthp->uth_limit;
		mtp->mt_byteslimit = uthp->uth_limit * uthp->uth_size;

		mtp->mt_numallocs = uthp->uth_allocs;
		mtp->mt_numfrees = uthp->uth_frees;
		mtp->mt_count = mtp->mt_numallocs - mtp->mt_numfrees;
		mtp->mt_zonefree = uthp->uth_zone_free + uthp->uth_keg_free;
		mtp->mt_free += mtp->mt_zonefree;
	}

	free(buffer);

	return (0);
}
