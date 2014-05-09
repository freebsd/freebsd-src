/*
 * Copyright (c) 2006, 2007 Cisco, Inc.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>

#include "mlx4.h"

#if !(defined(HAVE_IBV_DONTFORK_RANGE) && defined(HAVE_IBV_DOFORK_RANGE))

/*
 * If libibverbs isn't exporting these functions, then there's no
 * point in doing it here, because the rest of libibverbs isn't going
 * to be fork-safe anyway.
 */
static int ibv_dontfork_range(void *base, size_t size)
{
	return 0;
}

static int ibv_dofork_range(void *base, size_t size)
{
	return 0;
}

#endif /* HAVE_IBV_DONTFORK_RANGE && HAVE_IBV_DOFORK_RANGE */

int mlx4_alloc_buf(struct mlx4_buf *buf, size_t size, int page_size)
{
	int ret;

	buf->length = align(size, page_size);
	buf->buf = mmap(NULL, buf->length, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANON, -1, 0);
	if (buf->buf == MAP_FAILED)
		return errno;

	ret = ibv_dontfork_range(buf->buf, size);
	if (ret)
		munmap(buf->buf, buf->length);

	return ret;
}

void mlx4_free_buf(struct mlx4_buf *buf)
{
	ibv_dofork_range(buf->buf, buf->length);
	munmap(buf->buf, buf->length);
}
