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
 * When running in a capability sandbox, rtld-elf-cap will be passed a set of
 * open file descriptors to potentially useful libraries, along with an index
 * to these in the LD_CAPLIBINDEX environmental variable.  These routines
 * parse that index, and allow lookups by library name.  A typical string
 * might be:
 *
 * 6:libc.so.7,7:libm.so.5
 *
 * In the event of ambiguity, the earliest entry will be matched.
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/limits.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rtld.h"
#include "rtld_caplibindex.h"

struct libindex_entry {
	char				*lie_name;
	int				 lie_fd;
	TAILQ_ENTRY(libindex_entry)	 lie_list;
};

static TAILQ_HEAD(, libindex_entry)	ld_caplibindex_list =
    TAILQ_HEAD_INITIALIZER(ld_caplibindex_list);

static void
ld_caplibindex_add(const char *name, const char *fdnumber)
{
	struct libindex_entry *liep;
	long long l;
	char *endp;

	if (strlen(name) == 0 || strlen(fdnumber) == 0)
		return;

	l = strtoll(fdnumber, &endp, 10);
	if (l < 0 || l > INT_MAX || *endp != '\0')
		return;

	liep = xmalloc(sizeof(*liep));
	liep->lie_name = xstrdup(name);
	liep->lie_fd = l;
	TAILQ_INSERT_TAIL(&ld_caplibindex_list, liep, lie_list);
}

int
ld_caplibindex_lookup(const char *libname, int *fdp)
{
	struct libindex_entry *liep;

	TAILQ_FOREACH(liep, &ld_caplibindex_list, lie_list) {
		if (strcmp(liep->lie_name, libname) == 0) {
			*fdp = liep->lie_fd;
			return (0);
		}
	}
	return (-1);
}

void
ld_caplibindex_init(const char *caplibindex)
{
	char *caplibindex_copy, *caplibindex_tofree;
	char *entry, *fdnumber;

	caplibindex_copy = caplibindex_tofree = xstrdup(caplibindex);
	while ((entry = strsep(&caplibindex_copy, ",")) != NULL) {
		fdnumber = strsep(&entry, ":");
		if (fdnumber == NULL)
			continue;
		ld_caplibindex_add(entry, fdnumber);
	}
	free(caplibindex_tofree);
}
