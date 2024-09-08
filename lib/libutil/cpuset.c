/*
 * Copyright (c) 2007, 2008 	Jeffrey Roberson <jeff@freebsd.org>
 * All rights reserved.
 *
 * Copyright (c) 2008 Nokia Corporation
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

#include <sys/cdefs.h>
#define _WANT_FREEBSD_BITSET

#include <sys/types.h>
#include <sys/cpuset.h>
#include <sys/domainset.h>

#include <stdlib.h>
#include <string.h>
#include <libutil.h>
#include <ctype.h>

struct numa_policy {
	const char 	*name;
	int		policy;
};

static const struct numa_policy policies[] = {
	{ "round-robin", DOMAINSET_POLICY_ROUNDROBIN },
	{ "rr", DOMAINSET_POLICY_ROUNDROBIN },
	{ "first-touch", DOMAINSET_POLICY_FIRSTTOUCH },
	{ "ft", DOMAINSET_POLICY_FIRSTTOUCH },
	{ "prefer", DOMAINSET_POLICY_PREFER },
	{ "interleave", DOMAINSET_POLICY_INTERLEAVE},
	{ "il", DOMAINSET_POLICY_INTERLEAVE},
	{ NULL, DOMAINSET_POLICY_INVALID }
};

static int
parselist(const char *list, struct bitset *mask, int size)
{
	enum { NONE, NUM, DASH } state;
	int lastnum;
	int curnum;
	const char *l;

	state = NONE;
	curnum = lastnum = 0;
	for (l = list; *l != '\0';) {
		if (isdigit(*l)) {
			curnum = atoi(l);
			if (curnum >= size)
				return (CPUSET_PARSE_INVALID_CPU);
			while (isdigit(*l))
				l++;
			switch (state) {
			case NONE:
				lastnum = curnum;
				state = NUM;
				break;
			case DASH:
				for (; lastnum <= curnum; lastnum++)
					BIT_SET(size, lastnum, mask);
				state = NONE;
				break;
			case NUM:
			default:
				goto parserr;
			}
			continue;
		}
		switch (*l) {
		case ',':
			switch (state) {
			case NONE:
				break;
			case NUM:
				BIT_SET(size, curnum, mask);
				state = NONE;
				break;
			case DASH:
				goto parserr;
				break;
			}
			break;
		case '-':
			if (state != NUM)
				goto parserr;
			state = DASH;
			break;
		default:
			goto parserr;
		}
		l++;
	}
	switch (state) {
		case NONE:
			break;
		case NUM:
			BIT_SET(size, curnum, mask);
			break;
		case DASH:
			goto parserr;
	}
	return (CPUSET_PARSE_OK);
parserr:
	return (CPUSET_PARSE_ERROR);
}

/*
 * permissively parse policy:domain list
 * allow:
 *	round-robin:0-4		explicit
 *	round-robin:all		explicit root domains
 *	0-4			implicit root policy
 *	round-robin		implicit root domains
 *	all			explicit root domains and implicit policy
 */
int
domainset_parselist(const char *list, domainset_t *mask, int *policyp)
{
	domainset_t rootmask;
	const struct numa_policy *policy;
	const char *l;
	int p;

	/*
	 * Use the rootset's policy as the default for unspecified policies.
	 */
	if (cpuset_getdomain(CPU_LEVEL_ROOT, CPU_WHICH_PID, -1,
	    sizeof(rootmask), &rootmask, &p) != 0)
		return (CPUSET_PARSE_GETDOMAIN);

	if (list == NULL || strcasecmp(list, "all") == 0 || *list == '\0') {
		*policyp = p;
		DOMAINSET_COPY(&rootmask, mask);
		return (CPUSET_PARSE_OK);
	}

	l = list;
	for (policy = &policies[0]; policy->name != NULL; policy++) {
		if (strncasecmp(l, policy->name, strlen(policy->name)) == 0) {
			p = policy->policy;
			l += strlen(policy->name);
			if (*l != ':' && *l != '\0')
				return (CPUSET_PARSE_ERROR);
			if (*l == ':')
				l++;
			break;
		}
	}
	*policyp = p;

	return (parselist(l, (struct bitset *)mask, DOMAINSET_SETSIZE));
}

int
cpuset_parselist(const char *list, cpuset_t *mask)
{
	if (strcasecmp(list, "all") == 0) {
		if (cpuset_getaffinity(CPU_LEVEL_ROOT, CPU_WHICH_PID, -1,
		    sizeof(*mask), mask) != 0)
			return (CPUSET_PARSE_GETAFFINITY);
		return (CPUSET_PARSE_OK);
	}

	return (parselist(list, (struct bitset *)mask, CPU_SETSIZE));
}
