/*-
 * Copyright (c) 2009 Robert N. M. Watson
 * All rights reserved.
 *
 * WARNING: THIS IS EXPERIMENTAL SECURITY SOFTWARE THAT MUST NOT BE RELIED
 * ON IN PRODUCTION SYSTEMS.  IT WILL BREAK YOUR SOFTWARE IN NEW AND
 * UNEXPECTED WAYS.
 * 
 * This software was developed at the University of Cambridge Computer
 * Laboratory with support from a grant from Google, Inc. 
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * rtld maintains a cache of library file descriptors, which is passed from
 * host to sandbox at exec()-time in order to avoid the need for direct file
 * system access from within sandboxes.  When rtld starts, it inspects
 * LD_LIBCACHE to find library descriptors passed from the host.  This
 * variable maps file descriptor numbers to library names:
 *
 * 6:libc.so.7,7:libm.so.5
 *
 * In the event of ambiguity, the earliest entry will be matched.
 *
 * XXXRW: There should be locking around the libcache list.
 *
 * XXXRW: ld_libcache_lookup() should dup the fd before returning it so that
 * the caller is responsible for managing the returned fd reference.
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/limits.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rtld.h"
#include "rtld_libcache.h"

struct libcache_entry {
	char				*lie_name;
	int				 lie_fd;
	TAILQ_ENTRY(libcache_entry)	 lie_list;
};

static TAILQ_HEAD(, libcache_entry)	ld_libcache_list =
    TAILQ_HEAD_INITIALIZER(ld_libcache_list);

/*
 * Add a library to the library cache.
 */
int
ld_libcache_add(const char *name, int fd)
{
	struct libcache_entry *liep;

	liep = xmalloc(sizeof(*liep));
	liep->lie_name = xstrdup(name);
	liep->lie_fd = fd;
	TAILQ_INSERT_TAIL(&ld_libcache_list, liep, lie_list);
	return (0);
}

/*
 * Add a library to the library cache, with file descriptor passed as a
 * string.  Used internally when parsing LD_LIBCACHE.
 */
static void
ld_libcache_add_string(const char *name, const char *fdnumber)
{
	long long l;
	char *endp;

	if (strlen(name) == 0 || strlen(fdnumber) == 0)
		return;

	l = strtoll(fdnumber, &endp, 10);
	if (l < 0 || l > INT_MAX || *endp != '\0')
		return;

	ld_libcache_add(name, l);
}

/*
 * Given a library name, return its file descriptor (if defined).  Arguably,
 * we should dup the cache-owned fd rather than returning it directly to the
 * caller.
 */
int
ld_libcache_lookup(const char *libname, int *fdp)
{
	struct libcache_entry *liep;

	TAILQ_FOREACH(liep, &ld_libcache_list, lie_list) {
		if (strcmp(liep->lie_name, libname) == 0) {
			*fdp = liep->lie_fd;
			return (0);
		}
	}
	return (-1);
}

/*
 * Initialize the library cache given the LD_LIBCACHE environmental variable.
 */
void
ld_libcache_init(const char *libcache)
{
	char *libcache_copy, *libcache_tofree;
	char *entry, *fdnumber;

	libcache_copy = libcache_tofree = xstrdup(libcache);
	while ((entry = strsep(&libcache_copy, ",")) != NULL) {
		fdnumber = strsep(&entry, ":");
		if (fdnumber == NULL)
			continue;
		ld_libcache_add_string(entry, fdnumber);
	}
	free(libcache_tofree);
}
