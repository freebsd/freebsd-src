/*	$OpenBSD: groupaccess.c,v 1.3 2001/01/29 01:58:15 niklas Exp $	*/

/*
 * Copyright (c) 2001 Kevin Steves.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#include "groupaccess.h"
#include "xmalloc.h"
#include "match.h"
#include "log.h"

static int ngroups;
static char *groups_byname[NGROUPS_MAX + 1];	/* +1 for base/primary group */

int
ga_init(const char *user, gid_t base)
{
	gid_t groups_bygid[NGROUPS_MAX + 1];
	int i, j;
	struct group *gr;

	if (ngroups > 0)
		ga_free();

	ngroups = sizeof(groups_bygid) / sizeof(gid_t);
	if (getgrouplist(user, base, groups_bygid, &ngroups) == -1)
		log("getgrouplist: groups list too small");
	for (i = 0, j = 0; i < ngroups; i++)
		if ((gr = getgrgid(groups_bygid[i])) != NULL)
			groups_byname[j++] = xstrdup(gr->gr_name);
	return (ngroups = j);
}

int
ga_match(char * const *groups, int n)
{
	int i, j;

	for (i = 0; i < ngroups; i++)
		for (j = 0; j < n; j++)
			if (match_pattern(groups_byname[i], groups[j]))
				return 1;
	return 0;
}

void
ga_free(void)
{
	int i;

	if (ngroups > 0) {
		for (i = 0; i < ngroups; i++)
			xfree(groups_byname[i]);
		ngroups = 0;
	}
}
