/* $OpenBSD: roaming_dummy.c,v 1.4 2015/01/19 19:52:16 markus Exp $ */
/*
 * Copyright (c) 2004-2009 AppGate Network Security AB
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file is included in the client programs which should not
 * support roaming.
 */

#include "includes.h"

#include <sys/types.h>
#include <unistd.h>

#include "roaming.h"

int resume_in_progress = 0;

u_int64_t
get_recv_bytes(void)
{
	return 0;
}

u_int64_t
get_sent_bytes(void)
{
	return 0;
}

void
roam_set_bytes(u_int64_t sent, u_int64_t recvd)
{
}

ssize_t
roaming_write(int fd, const void *buf, size_t count, int *cont)
{
	return write(fd, buf, count);
}

ssize_t
roaming_read(int fd, void *buf, size_t count, int *cont)
{
	if (cont)
		*cont = 0;
	return read(fd, buf, count);
}

void
add_recv_bytes(u_int64_t num)
{
}

int
resume_kex(void)
{
	return 1;
}
