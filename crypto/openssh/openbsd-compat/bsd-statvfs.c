/* $Id: bsd-statvfs.c,v 1.1 2008/06/08 17:32:29 dtucker Exp $ */

/*
 * Copyright (c) 2008 Darren Tucker <dtucker@zip.com.au>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"

#include <errno.h>

#ifndef HAVE_STATVFS
int statvfs(const char *path, struct statvfs *buf)
{
	errno = ENOSYS;
	return -1;
}
#endif

#ifndef HAVE_FSTATVFS
int fstatvfs(int fd, struct statvfs *buf)
{
	errno = ENOSYS;
	return -1;
}
#endif
