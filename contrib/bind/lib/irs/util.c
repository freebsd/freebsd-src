/*
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$Id: util.c,v 1.12 2001/05/29 05:49:21 marka Exp $";
#endif

#include "port_before.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <irs.h>

#include "port_after.h"

#include "irs_p.h"

#ifdef SPRINTF_CHAR
# define SPRINTF(x) strlen(sprintf/**/x)
#else
# define SPRINTF(x) sprintf x
#endif

void
map_v4v6_address(const char *src, char *dst) {
	u_char *p = (u_char *)dst;
	char tmp[NS_INADDRSZ];
	int i;

	/* Stash a temporary copy so our caller can update in place. */
	memcpy(tmp, src, NS_INADDRSZ);
	/* Mark this ipv6 addr as a mapped ipv4. */
	for (i = 0; i < 10; i++)
		*p++ = 0x00;
	*p++ = 0xff;
	*p++ = 0xff;
	/* Retrieve the saved copy and we're done. */
	memcpy((void*)p, tmp, NS_INADDRSZ);
}

int
make_group_list(struct irs_gr *this, const char *name,
	gid_t basegid, gid_t *groups, int *ngroups)
{
	struct group *grp;
	int i, ng;
	int ret, maxgroups;

	ret = -1;
	ng = 0;
	maxgroups = *ngroups;
	/*
	 * When installing primary group, duplicate it;
	 * the first element of groups is the effective gid
	 * and will be overwritten when a setgid file is executed.
	 */
	if (ng >= maxgroups)
		goto done;
	groups[ng++] = basegid;
	if (ng >= maxgroups)
		goto done;
	groups[ng++] = basegid;
	/*
	 * Scan the group file to find additional groups.
	 */
	(*this->rewind)(this);
	while ((grp = (*this->next)(this)) != NULL) {
		if ((gid_t)grp->gr_gid == basegid)
			continue;
		for (i = 0; grp->gr_mem[i]; i++) {
			if (!strcmp(grp->gr_mem[i], name)) {
				if (ng >= maxgroups)
					goto done;
				groups[ng++] = grp->gr_gid;
				break;
			}
		}
	}
	ret = 0;
 done:
	*ngroups = ng;
	return (ret);
}
