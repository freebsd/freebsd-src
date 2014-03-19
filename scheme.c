/*-
 * Copyright (c) 2013 Juniper Networks, Inc.
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
#include <sys/diskmbr.h>
#include <sys/diskpc98.h>
#include <sys/queue.h>
#include <sys/vtoc.h>
#include <err.h>
#include <errno.h>
#include <stdint.h>
#include <strings.h>
#include <unistd.h>

#include "mkimg.h"
#include "scheme.h"

static struct scheme {
	const char *lexeme;
	u_int token;
} schemes[] = {
	{ .lexeme = "apm", .token = SCHEME_APM },
	{ .lexeme = "bsd", .token = SCHEME_BSD },
	{ .lexeme = "ebr", .token = SCHEME_EBR },
	{ .lexeme = "gpt", .token = SCHEME_GPT },
	{ .lexeme = "mbr", .token = SCHEME_MBR },
	{ .lexeme = "pc98", .token = SCHEME_PC98 },
	{ .lexeme = "vtoc8", .token = SCHEME_VTOC8 },
	{ .lexeme = NULL, .token = SCHEME_UNDEF }
};

static u_int scheme = SCHEME_UNDEF;
static u_int secsz = 512;

int
scheme_select(const char *spec)
{
	struct scheme *s;

	s = schemes;
	while (s->lexeme != NULL) {
		if (strcasecmp(spec, s->lexeme) == 0) {
			scheme = s->token;
			return (0);
		}
		s++;
	}
	return (EINVAL);
}

u_int
scheme_selected(void)
{

	return (scheme);
}

int
scheme_check_part(struct part *p __unused)
{

	warnx("part: index=%u, type=`%s', offset=%ju, size=%ju", p->index,
	    p->type, (uintmax_t)p->offset, (uintmax_t)p->size);

	return (0);
}

u_int
scheme_max_parts(void)
{
	u_int parts;

	switch (scheme) {
	case SCHEME_APM:
		parts = 4096;
		break;
	case SCHEME_BSD:
		parts = 20;
		break;
	case SCHEME_EBR:
		parts = 4096;
		break;
	case SCHEME_GPT:
		parts = 4096;
		break;
	case SCHEME_MBR:
		parts = NDOSPART;
		break;
	case SCHEME_PC98:
		parts = PC98_NPARTS;
		break;
	case SCHEME_VTOC8:
		parts = VTOC8_NPARTS;
		break;
	default:
		parts = 0;
		break;
	}
	return (parts);
}

off_t
scheme_first_offset(u_int parts)
{
	off_t off;

	switch (scheme) {
	case SCHEME_APM:
		off = parts + 1;
		break;
	case SCHEME_BSD:
		off = 16;
		break;
	case SCHEME_EBR:
		off = 1;
		break;
	case SCHEME_GPT:
		off = 2 + (parts + 3) / 4;
		break;
	case SCHEME_MBR:
		off = 1;
		break;
	case SCHEME_PC98:
		off = 2;
		break;
	case SCHEME_VTOC8:
		off = 1;
		break;
	default:
		off = 0;
		break;
	}
	off *= secsz;
	return (off);
}

off_t
scheme_next_offset(off_t off, uint64_t sz)
{

	sz = (sz + secsz - 1) & ~(secsz - 1);
	if (scheme == SCHEME_EBR)
		sz += secsz;
	return (off + sz);
}

void
scheme_write(int fd, off_t off)
{
	off_t lim;

	switch (scheme) {
	case SCHEME_GPT:
		lim = off + secsz * (1 + (nparts + 3) / 4);
		break;
	case SCHEME_EBR:
		off -= secsz;
		/* FALLTHROUGH */
	default:
		lim = off;
		break;
	}
	ftruncate(fd, lim);
}
