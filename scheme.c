/*-
 * Copyright (c) 2013,2014 Juniper Networks, Inc.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/linker_set.h>
#include <sys/queue.h>
#include <err.h>
#include <errno.h>
#include <stdint.h>
#include <strings.h>
#include <unistd.h>

#include "mkimg.h"
#include "scheme.h"

static struct mkimg_scheme *scheme;
static u_int secsz = 512;

int
scheme_select(const char *spec)
{
	struct mkimg_scheme *s, **iter;

	SET_FOREACH(iter, schemes) {
		s = *iter;
		if (strcasecmp(spec, s->name) == 0) {
			scheme = s;
			return (0);
		}
	}
	return (EINVAL);
}

struct mkimg_scheme *
scheme_selected(void)
{

	return (scheme);
}

int
scheme_check_part(struct part *p)
{
	struct mkimg_alias *alias, *iter;

	warnx("part(%s): index=%u, type=`%s', offset=%ju, size=%ju",
	    scheme->name, p->index, p->alias, (uintmax_t)p->offset,
	    (uintmax_t)p->size);

	/* Check the partition type alias */
	alias = NULL;
	iter = scheme->aliases;
	while (iter->name != NULL) {
		if (strcasecmp(p->alias, iter->name) == 0) {
			alias = iter;
			break;
		}
		iter++;
	}
	if (alias == NULL)
		return (EINVAL);

	return (0);
}

u_int
scheme_max_parts(void)
{

	return (scheme->nparts);
}

off_t
scheme_first_offset(u_int parts)
{
	off_t off;

	off = scheme->get_leader(parts);
	off *= secsz;
	return (off);
}

off_t
scheme_next_offset(off_t off, uint64_t sz)
{

	sz = (sz + secsz - 1) & ~(secsz - 1);
	sz += scheme->padding * secsz;
	return (off + sz);
}

void
scheme_write(int fd, off_t off)
{
	off_t trailer;

	trailer = scheme->get_trailer(nparts) * secsz;
	ftruncate(fd, off + trailer);
}
