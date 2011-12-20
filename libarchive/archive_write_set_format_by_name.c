/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "archive_platform.h"
__FBSDID("$FreeBSD: head/lib/libarchive/archive_write_set_format_by_name.c 201168 2009-12-29 06:15:32Z kientzle $");

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "archive.h"
#include "archive_private.h"

/* A table that maps names to functions. */
static
struct { const char *name; int (*setter)(struct archive *); } names[] =
{
	{ "ar",		archive_write_set_format_ar_bsd },
	{ "arbsd",	archive_write_set_format_ar_bsd },
	{ "argnu",	archive_write_set_format_ar_svr4 },
	{ "arsvr4",	archive_write_set_format_ar_svr4 },
	{ "cpio",	archive_write_set_format_cpio },
	{ "mtree",	archive_write_set_format_mtree },
	{ "newc",	archive_write_set_format_cpio_newc },
	{ "odc",	archive_write_set_format_cpio },
	{ "pax",	archive_write_set_format_pax },
	{ "posix",	archive_write_set_format_pax },
	{ "shar",	archive_write_set_format_shar },
	{ "shardump",	archive_write_set_format_shar_dump },
	{ "ustar",	archive_write_set_format_ustar },
	{ "zip",	archive_write_set_format_zip },
	{ NULL,		NULL }
};

int
archive_write_set_format_by_name(struct archive *a, const char *name)
{
	int i;

	for (i = 0; names[i].name != NULL; i++) {
		if (strcmp(name, names[i].name) == 0)
			return ((names[i].setter)(a));
	}

	archive_set_error(a, EINVAL, "No such format '%s'", name);
	return (ARCHIVE_FATAL);
}
